#include "wad.h"
#include "bspfile.h"
#include "common.h"
#include "filesystem.h"
#include "conprint.h"
#include "mem.h"
#include "sys.h"

#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define TEX_MAX_WADS 128
#define TEX_MAX_MIPTEX_NAME 64

typedef struct
{
	lumpinfo_t lump;
	int iTexFile; // index of the wad this texture is located in
} texlumpinfo_t;

int nummiptex;
char miptex[MAX_MAP_TEXTURES][TEX_MAX_MIPTEX_NAME];

FileHandle_t texfiles[TEX_MAX_WADS];
int nTexFiles;
texlumpinfo_t* lumpinfo;
int nTexLumps;

void CleanupName( char* in, char* out )
{
	int i;

	for ( i = 0; i < WAD_LUMP_NAME_BYTES; i++ )
	{
		if ( !in[i] )
			break;

		out[i] = (char)toupper( static_cast<unsigned char>( in[i] ) );
	}

	for ( ; i < WAD_LUMP_NAME_BYTES; i++ )
		out[i] = 0;
}

/*
=================
lump_sorters
=================
*/
int lump_sorter( const void* lump1, const void* lump2 )
{
	const texlumpinfo_t* plump1 = (const texlumpinfo_t*)lump1;
	const texlumpinfo_t* plump2 = (const texlumpinfo_t*)lump2;
	int cmp;

	cmp = strncmp( plump1->lump.name, plump2->lump.name, WAD_LUMP_NAME_BYTES );
	if ( cmp < 0 )
		return -1;
	if ( cmp > 0 )
		return 1;
	return 0;
}

void SafeRead( FileHandle_t f, void* buffer, int count )
{
	if ( FS_Read( buffer, count, 1, f ) != count )
		Sys_Error( "File read failure" );
}

void ForwardSlashes( char* pname )
{
	while ( *pname )
	{
		if ( *pname == '\\' )
			*pname = '/';
		pname++;
	}
}

/*
=================
TEX_InitFromWad
=================
*/
qboolean TEX_InitFromWad( char* path )
{
	int i;
	wadinfo_t wadinfo;
	char szTmpPath[1024];
	char* pszWadFile;
	FileHandle_t texfile;
	texlumpinfo_t* lump;
	char wadName[260];
	char wadPath[260];

	Q_strncpy( szTmpPath, path, sizeof( szTmpPath ) - 2 );
	szTmpPath[sizeof( szTmpPath ) - 2] = 0;

	// temporary kludge so we don't have to deal with no occurances of a semicolon
	//  in the path name ..
	if ( !strchr( szTmpPath, ';' ) )
		Q_strcat( szTmpPath, ";" );

	for ( pszWadFile = strtok( szTmpPath, ";" ); pszWadFile; pszWadFile = strtok( NULL, ";" ) )
	{
		ForwardSlashes( pszWadFile );

		if ( !COM_FileBase_s( pszWadFile, wadName, sizeof( wadName ) ) )
			Sys_Error( "TEX_InitFromWad: Invalid path length: %s", pszWadFile );

		snprintf( wadPath, 256, "%s", wadName );
		COM_DefaultExtension( wadPath, ".wad" );

		if ( Q_strstr( wadName, "pldecal" ) || Q_strstr( wadName, "tempdecal" ) )
			continue;

		if ( nTexFiles >= TEX_MAX_WADS )
			Sys_Error( "TEX_InitFromWad: too many WAD files" );
		texfiles[nTexFiles] = FS_Open( wadPath, "rb" );
		if ( !texfiles[nTexFiles] )
			Sys_Error( "WARNING: couldn't open %s\n", wadPath );

		texfile = texfiles[nTexFiles];
		nTexFiles++;

		Con_DPrintf( "Using WAD File: %s\n", wadPath );

		SafeRead( texfile, &wadinfo, sizeof( wadinfo ) );
		if ( Q_strncmp( wadinfo.identification, "WAD2", 4 ) && Q_strncmp( wadinfo.identification, "WAD3", 4 ) )
			Sys_Error( "TEX_InitFromWad: %s isn't a wadfile", wadPath );

		wadinfo.numlumps = LittleLong( wadinfo.numlumps );
		wadinfo.infotableofs = LittleLong( wadinfo.infotableofs );
		const size_t fileSize = FS_Size( texfile );
		if ( wadinfo.numlumps < 0 || wadinfo.infotableofs < 0 || (size_t)wadinfo.infotableofs > fileSize
			 || (size_t)wadinfo.numlumps > ( fileSize - wadinfo.infotableofs ) / sizeof( lumpinfo_t )
			 || wadinfo.numlumps > INT_MAX - nTexLumps )
			Sys_Error( "TEX_InitFromWad: invalid lump directory in %s", wadPath );
		if ( wadinfo.numlumps == 0 )
			continue;

		FS_Seek( texfile, wadinfo.infotableofs, FILESYSTEM_SEEK_HEAD );
		const size_t totalLumps = (size_t)wadinfo.numlumps + nTexLumps;
		if ( totalLumps > SIZE_MAX / sizeof( texlumpinfo_t ) )
			Sys_Error( "TEX_InitFromWad: too many lumps in %s", wadPath );
		texlumpinfo_t* expanded = (texlumpinfo_t*)Mem_Realloc( lumpinfo, sizeof( texlumpinfo_t ) * totalLumps );
		if ( !expanded )
			Sys_Error( "TEX_InitFromWad: could not allocate the lump directory for %s", wadPath );
		lumpinfo = expanded;

		for ( i = 0; i < wadinfo.numlumps; i++ )
		{
			lump = &lumpinfo[nTexLumps];
			SafeRead( texfile, lump, sizeof( lumpinfo_t ) );
			CleanupName( lump->lump.name, lump->lump.name );

			lump->lump.filepos = LittleLong( lump->lump.filepos );
			lump->lump.disksize = LittleLong( lump->lump.disksize );
			if ( lump->lump.filepos < 0 || lump->lump.disksize < 0 || (size_t)lump->lump.filepos > fileSize
				 || (size_t)lump->lump.disksize > fileSize - lump->lump.filepos )
				Sys_Error( "TEX_InitFromWad: lump outside file in %s", wadPath );
			lump->iTexFile = nTexFiles - 1;

			nTexLumps++;
		}
	}

	qsort( lumpinfo, nTexLumps, sizeof( texlumpinfo_t ), lump_sorter );

	return true;
}

void TEX_CleanupWadInfo()
{
	int i;

	if ( lumpinfo )
	{
		Mem_Free( lumpinfo );
		lumpinfo = NULL;
	}

	for ( i = 0; i < nTexFiles; i++ )
	{
		FS_Close( texfiles[i] );
		texfiles[i] = NULL;
	}

	nTexLumps = 0;
	nTexFiles = 0;
}

static bool TEX_ValidateMiptex( const byte* data, size_t length )
{
	constexpr size_t mipAlignment = 16;
	constexpr size_t paletteEntries = 256;
	constexpr size_t rgbBytes = 3;
	if ( length < sizeof( miptex_t ) )
		return false;

	miptex_t header;
	memcpy( &header, data, sizeof( header ) );
	if ( !memchr( header.name, '\0', sizeof( header.name ) ) )
		return false;

	const size_t width = (unsigned int)LittleLong( header.width );
	const size_t height = (unsigned int)LittleLong( header.height );
	size_t offset = sizeof( header );
	if ( !width || !height || width % mipAlignment || height % mipAlignment || width > ( length - offset ) / height )
		return false;

	size_t pixels = width * height;
	for ( int level = 0; level < MIPLEVELS; ++level )
	{
		if ( (unsigned int)LittleLong( header.offsets[level] ) != offset || pixels > length - offset )
			return false;
		offset += pixels;
		pixels /= 4;
	}

	unsigned short colors;
	if ( length - offset < sizeof( colors ) )
		return false;
	memcpy( &colors, data + offset, sizeof( colors ) );
	colors = (unsigned short)LittleShort( colors );
	return colors >= paletteEntries && colors <= ( length - offset - sizeof( colors ) ) / rgbBytes;
}

int TEX_LoadLump( char* name, byte* dest, int destSize )
{
	texlumpinfo_t key;
	texlumpinfo_t* found;

	CleanupName( name, key.lump.name );

	found = (texlumpinfo_t*)bsearch( &key, lumpinfo, nTexLumps, sizeof( key ), lump_sorter );
	if ( !found )
	{
		Con_SafePrintf( "WARNING: texture lump \"%s\" not found\n", name );
		return 0;
	}

	constexpr int uncompressed = 0;
	if ( found->lump.disksize <= 0 || found->lump.disksize > destSize || found->lump.compression != uncompressed )
		Sys_Error( "TEX_LoadLump: invalid storage for texture lump \"%.*s\"", WAD_LUMP_NAME_BYTES, name );

	FS_Seek( texfiles[found->iTexFile], found->lump.filepos, FILESYSTEM_SEEK_HEAD );
	SafeRead( texfiles[found->iTexFile], dest, found->lump.disksize );
	if ( !TEX_ValidateMiptex( dest, found->lump.disksize ) )
		Sys_Error( "TEX_LoadLump: invalid miptex in texture lump \"%.*s\"", WAD_LUMP_NAME_BYTES, name );
	return found->lump.disksize;
}

//==========================================================================


int FindMiptex( char* name )
{
	int i;

	for ( i = 0; i < nummiptex; i++ )
	{
		if ( !Q_strcasecmp( name, miptex[i] ) )
			return i;
	}

	if ( nummiptex == MAX_MAP_TEXTURES )
		Sys_Error( "Exceeded MAX_MAP_TEXTURES" );

	Q_strncpy( miptex[i], name, TEX_MAX_MIPTEX_NAME - 1 );
	miptex[i][TEX_MAX_MIPTEX_NAME - 1] = 0;
	nummiptex++;
	return i;
}

void TEX_AddAnimatingTextures()
{
	int base;
	int i, j, k;
	char name[32];

	base = nummiptex;

	for ( i = 0; i < base; i++ )
	{
		if ( miptex[i][0] != '+' && miptex[i][0] != '-' )
			continue;

		Q_strncpy( name, miptex[i], sizeof( name ) - 1 );
		name[sizeof( name ) - 1] = 0;

		for ( j = 0; j < 20; j++ )
		{
			if ( j < 10 )
				name[1] = (char)( '0' + j );
			else
				name[1] = (char)( 'A' + j - 10 ); // alternate animation

			// see if this name exists in the wadfile
			for ( k = 0; k < nTexLumps; k++ )
			{
				if ( !Q_strcmp( name, lumpinfo[k].lump.name ) )
				{
					FindMiptex( name ); // add to the miptex list
					break;
				}
			}
		}
	}

	if ( nummiptex != base )
		Con_DPrintf( "added %i texture frames\n", nummiptex - base );
}
