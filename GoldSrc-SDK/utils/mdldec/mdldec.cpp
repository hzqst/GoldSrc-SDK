// Model loading and command-line entry point.

#include "mdldec.h"
#ifdef _WIN32
#include <direct.h>
#define MDLDEC_PATH_SEPARATOR "\\"
#else
#include <sys/stat.h>
#define _mkdir( path ) mkdir( path, 0777 )
#define MDLDEC_PATH_SEPARATOR "/"
#endif
#include <errno.h>

// LoadModel supplies the header views before success, WriteQC opens the QC file
// before use, and WriteReference fills bone transforms in model order before use.
// m_boneLocal is unused workspace.
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CMDLDecompiler::CMDLDecompiler()
{
	// The original empties both names with _strset over the fresh allocation;
	// clearing the first byte gives the same empty strings without scanning
	// uninitialized memory.
	m_szModelName[0] = 0;
	m_szOutputPath[0] = 0;
	m_szOutputPath[260] = 0;
	m_pModelData = NULL;
	m_nModelSize = 0;
	for (int i = 0; i < MAX_SEQUENCE_FILES; i++)
	{
		m_nSequenceSize[i] = 0;
		m_pSequenceData[i] = NULL;
	}
	m_pTextureData = NULL;
	m_nTextureSize = 0;
}

CMDLDecompiler::~CMDLDecompiler()
{
	free(m_pModelData);
	for (int i = 0; i < MAX_SEQUENCE_FILES; i++)
	{
		if (m_nSequenceSize[i] > 0)
			free(m_pSequenceData[i]);
	}
	if (m_nTextureSize > 0)
		free(m_pTextureData);
}

bool CMDLDecompiler::LoadModel(const char *filename)
{
	Log(0, "MDL: %s\r\n", filename);
	FILE *file = fopen(filename, "rb");
	if (file == NULL)
	{
		Log(2, "ERROR: Cannot open %s\r\n", filename);
		return false;
	}
	fseek(file, 0, SEEK_END);
	m_nModelSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	// guard: one terminating byte after the file data, so a name field without its NUL at the
	// very end of the file stops there when printed with %s instead of reading past the buffer.
	m_pModelData = (byte *)malloc(m_nModelSize + 1);
	fread(m_pModelData, m_nModelSize, 1, file);
	m_pModelData[m_nModelSize] = 0;
	fclose(file);

	// guard: prevents the native reading the studio header past the loaded file. A file
	// shorter than studiohdr_t makes the "IDST" compare and every header field below
	// (numbodyparts at 0xCC, numseqgroups, the indices used by the writers) read
	// uninitialized heap, so the original's output varies run to run (the "truncated"
	// case). Reuse the loader's existing "not a valid HL model file" rejection.
	if (m_nModelSize < (int)sizeof(studiohdr_t))
	{
		Log(2, "ERROR: %s is not a valid HL model file.\r\n", filename);
		return false;
	}

	if (memcmp(m_pModelData, "IDST", 4) != 0)
	{
		if (memcmp(m_pModelData, "IDSQ", 4) == 0)
			Log(2, "ERROR: %s is not a main HL model file.\r\n", filename);
		else
			Log(2, "ERROR: %s is not a valid HL model file.\r\n", filename);
		return false;
	}
	m_pStudioHeader = (studiohdr_t *)m_pModelData;
	if (m_pStudioHeader->numbodyparts == 0)
	{
		Log(2, "ERROR: %s is not a main HL model file.\r\n", filename);
		return false;
	}

	if (m_szOutputPath[0] != 0)
	{
		// Always true, so a separator is appended even after a trailing one.
		// guard: the native appends even when the 260-character path already fills the buffer.
		if ((m_szOutputPath[strlen(m_szOutputPath) - 1] != '/' || m_szOutputPath[strlen(m_szOutputPath) - 1] != '\\')
			&& strlen(m_szOutputPath) < sizeof(m_szOutputPath) - 1)
			strcat(m_szOutputPath, MDLDEC_PATH_SEPARATOR);
		for (char *separator = m_szOutputPath; *separator != 0; separator++)
		{
			char saved = *separator;
			if (saved == '/' || saved == '\\')
			{
				*separator = 0;
				if (_mkdir(m_szOutputPath) == -1 && errno != EEXIST)
				{
					Log(2, "ERROR: Cannot create %s\r\n", m_szOutputPath);
					return false;
				}
				*separator = saved;
			}
		}
	}
	else
	{
		ExtractFilePath(filename, m_szOutputPath, sizeof(m_szOutputPath));
	}

	char texturePath[260];
	if (m_pStudioHeader->numtextures != 0)
	{
		m_pTextureHeader = m_pStudioHeader;
	}
	else
	{
		// guard: the native copies the command-line path into texturePath[260] and writes "T.mdl"
		// over its last 4 characters without a check (overrun on long paths, a write before the
		// buffer on paths shorter than 4); take the existing "cannot find" failure path instead.
		size_t pathLength = strlen(filename);
		if (pathLength < 4 || pathLength - 4 + sizeof("T.mdl") > sizeof(texturePath))
		{
			Log(2, "ERROR: Cannot find external textures file %s\r\n", filename);
			return false;
		}
		strcpy(texturePath, filename);
		strcpy(texturePath + strlen(texturePath) - 4, "T.mdl");
		file = fopen(texturePath, "rb");
#ifndef _WIN32
		// Case-sensitive file systems: the game ships these as "<name>t.mdl".
		if (file == NULL)
		{
			texturePath[strlen(texturePath) - 5] = 't';
			file = fopen(texturePath, "rb");
			if (file == NULL)
				texturePath[strlen(texturePath) - 5] = 'T';
		}
#endif
		if (file == NULL)
		{
			Log(2, "ERROR: Cannot find external textures file %s\r\n", texturePath);
			return false;
		}
		fseek(file, 0, SEEK_END);
		m_nTextureSize = ftell(file);
		fseek(file, 0, SEEK_SET);
		// guard: terminating byte after the data, as for the main file.
		m_pTextureData = (byte *)malloc(m_nTextureSize + 1);
		fread(m_pTextureData, m_nTextureSize, 1, file);
		m_pTextureData[m_nTextureSize] = 0;
		fclose(file);
		// guard: a T.mdl shorter than the studio header makes the texture writers read
		// its numtextures/textureindex past the loaded file; reject it as the loader
		// already rejects an invalid main file.
		if (m_nTextureSize < (int)sizeof(studiohdr_t))
		{
			Log(2, "ERROR: %s is not a valid HL model file.\r\n", texturePath);
			return false;
		}
		m_pTextureHeader = (studiohdr_t *)m_pTextureData;
	}

	if (m_pStudioHeader->numseqgroups > 1)
	{
		char sequencePath[256];
		for (int group = 1; group < m_pStudioHeader->numseqgroups; group++)
		{
			// guard: the native builds "<name>NN.mdl" in sequencePath[256] without a length check
			// and stores each group in MAX_SEQUENCE_FILES-entry arrays for any numseqgroups the
			// file claims; take the existing "cannot open" failure path instead of overrunning.
			size_t pathLength = strlen(filename);
			if (group >= MAX_SEQUENCE_FILES || pathLength < 4 || pathLength - 4 + sizeof("00.mdl") > sizeof(sequencePath))
			{
				Log(2, "ERROR: Cannot open sequence file %s\r\n", filename);
				return false;
			}
			strcpy(sequencePath, filename);
			sprintf(sequencePath + strlen(sequencePath) - 4, "%02d.mdl", group);
			file = fopen(sequencePath, "rb");
			if (file == NULL)
			{
				Log(2, "ERROR: Cannot open sequence file %s\r\n", sequencePath);
				return false;
			}
			fseek(file, 0, SEEK_END);
			m_nSequenceSize[group] = ftell(file);
			fseek(file, 0, SEEK_SET);
			// guard: terminating byte after the data, as for the main file.
			m_pSequenceData[group] = (byte *)malloc(m_nSequenceSize[group] + 1);
			fread(m_pSequenceData[group], m_nSequenceSize[group], 1, file);
			m_pSequenceData[group][m_nSequenceSize[group]] = 0;
			fclose(file);
			m_pSequenceHeader[group] = (studioseqhdr_t *)m_pSequenceData[group];
		}
	}
	ExtractFileBase(filename, m_szModelName, sizeof(m_szModelName));
	RenameSequences();
	NameBones();
	return true;
}

// guard: the original appends "_<n>" with sprintf( label, "%s_%i", label, n ), formatting the label onto
// itself (overlapping copy) and, for long labels, writing past label[32] into the next fields. Write the
// suffix after the label instead; only when label and suffix do not fit together is the label shortened
// just enough for the suffix, so the names stay distinct inside the field. Shorter labels get exactly the
// bytes the original wrote (label, suffix, terminator; the rest of the field is left as is).
static void AppendLabelSuffix(char *label, int number)
{
	char suffix[16];
	_snprintf(suffix, sizeof(suffix), "_%i", number);
	suffix[sizeof(suffix) - 1] = 0;
	size_t suffixLength = strlen(suffix);
	size_t length = strnlen(label, 32);
	if (length > 31 - suffixLength)
		length = 31 - suffixLength;
	memcpy(label + length, suffix, suffixLength + 1);
}

void CMDLDecompiler::RenameSequences()
{
	studiohdr_t *header = (studiohdr_t *)m_pModelData;
	bool renamed = false;
	int count = 0;
	mstudioseqdesc_t *sequence = NULL;
	// guard: seqindex/numseq come from the file; only visit sequence descriptors that lie
	// inside the loaded buffer (the native reads and renames past it).
	const mstudioseqdesc_t *first = (const mstudioseqdesc_t *)(m_pModelData + header->seqindex);
	int numseq = header->numseq;
	if ((const byte *)first < m_pModelData || (const byte *)first > m_pModelData + m_nModelSize)
		numseq = 0;
	else if (numseq > (int)((m_pModelData + m_nModelSize - (const byte *)first) / sizeof(mstudioseqdesc_t)))
		numseq = (int)((m_pModelData + m_nModelSize - (const byte *)first) / sizeof(mstudioseqdesc_t));
	for (int i = 0; i < numseq; i++)
	{
		sequence = (mstudioseqdesc_t *)(m_pModelData + header->seqindex) + i;
		count = 1;
		for (int j = 0; j < numseq; j++)
		{
			if (j != i)
			{
				mstudioseqdesc_t *other = (mstudioseqdesc_t *)(m_pModelData + header->seqindex) + j;
				if (strncmp(other->label, sequence->label, 32) == 0)
					AppendLabelSuffix(other->label, ++count);
			}
		}
		if (count > 1)
		{
			Log(1, "WARNING: Sequence name \"%s\" is repeated %i times.\r\n", sequence->label, count);
			AppendLabelSuffix(sequence->label, 1);
			renamed = true;
		}
	}
	// The original passes the last label and count here although the text has no conversions.
	if (renamed)
		Log(1, "WARNING: Added numeric suffix to repeated sequence name(s).\r\n", sequence->label, count);
}

void CMDLDecompiler::NameBones()
{
	studiohdr_t *header = (studiohdr_t *)m_pModelData;
	int count = 0;
	for (int i = 0; i < header->numbones; i++)
	{
		mstudiobone_t *bone = (mstudiobone_t *)(m_pModelData + header->boneindex) + i;
		// guard: boneindex/numbones come from the file; the native names (writes) bones past the
		// loaded buffer. Stop at the first bone that does not fit.
		if ((const byte *)bone < m_pModelData || (const byte *)(bone + 1) > m_pModelData + m_nModelSize)
			break;
		if (strncmp(bone->name, "", 32) == 0)
			sprintf(bone->name, "MDLDEC_Bone%i", ++count);
	}
	if (count != 0)
		Log(1, "WARNING: Gived name to %i unnamed bone(s).\r\n", count);
}

void CMDLDecompiler::ExtractFileBase(const char *path, char *name, size_t nameSize)
{
	// guard: on an empty path the native starts one byte before it and walks backwards out of
	// the string (model and texture names come from the file); return an empty name instead.
	if (*path == 0)
	{
		*name = 0;
		return;
	}
	const char *base = path + strlen(path) - 1;
	while (base != path && base[-1] != '\\' && base[-1] != '/')
		base--;
	// guard: the native copies without a limit into fixed name buffers (65/68 bytes, or the
	// 64-byte model name in place); stop one byte short of the destination size.
	char *last = name + nameSize - 1;
	while (*base != 0 && *base != '.' && name < last)
		*name++ = *base++;
	*name = 0;
}

void CMDLDecompiler::ExtractFilePath(const char *path, char *directory, size_t directorySize)
{
	const char *base = path + strlen(path) - 1;
	while (base != path && base[-1] != '\\' && base[-1] != '/')
		base--;
	size_t length = base - path;
	// guard: the directory part of the input path comes from the command line; the native
	// memcpy overruns the 261-byte output path when it is longer.
	if (length > directorySize - 1)
		length = directorySize - 1;
	memcpy(directory, path, length);
	directory[length] = 0;
}

void CMDLDecompiler::Log(int level, const char *format, ...)
{
	(void)level;
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

int main(int argc, char **argv)
{
	printf("\nHalf-Life MDL Decompiler %s\n", "1.2");
	printf("2003, Kratisto. Based on code from Valve's HL SDK.\n");
	printf("--------------------------------------------------\n");
	if (argc == 1)
	{
		printf("Usage: mdldec.exe <path\\mdlfile.mdl> [<destpath>]\n");
	}
	else
	{
		CMDLDecompiler *decompiler = new CMDLDecompiler;
		if (argc == 3)
		{
			// guard: the native strcpy overruns m_szOutputPath[261] with a longer argument.
			strncpy(decompiler->m_szOutputPath, argv[2], sizeof(decompiler->m_szOutputPath) - 1);
			decompiler->m_szOutputPath[sizeof(decompiler->m_szOutputPath) - 1] = 0;
		}
		if (decompiler->LoadModel(argv[1]))
		{
			decompiler->WriteQC();
			decompiler->WriteReference();
			decompiler->WriteSequences();
			decompiler->WriteTextures();
			printf("Done.\n");
		}
		delete decompiler;
	}
	printf("--------------------------------------------------\n");
	return 0;
}
