//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "basefilesystem.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <alloca.h>
#include <strings.h>

#define _strlwr		strlwr
#define _strnicmp	strncasecmp
#define _snprintf	snprintf
#endif

#include "../public/buildslave_assert.h"
#define ASSERT_FILE	BUILDSLAVE_PATH( L"filesystem\\FileSystem_Stdio\\BaseFileSystem.cpp" )

#ifdef _WIN32
#define CORRECT_PATH_SEPARATOR		'\\'
#define INCORRECT_PATH_SEPARATOR	'/'
#else
#define CORRECT_PATH_SEPARATOR		'/'
#define INCORRECT_PATH_SEPARATOR	'\\'
#endif

float g_flDummyFloat = 1.0f;

CBaseFileSystem* CBaseFileSystem::s_pFileSystem = NULL;

static const char* COM_FileExtension( const char* in )
{
	static char exten[8];
	int i;

	while ( *in && *in != '.' )
		in++;
	if ( !*in )
		return "";
	in++;
	for ( i = 0; i < 7 && *in; i++, in++ )
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}

static const char* COM_LastFileExtension( const char* in )
{
	const char* cur = NULL;
	int i;

	if ( !in )
		return "";

	for ( i = 0; in[i]; i++ )
	{
		if ( in[i] == '.' )
			cur = &in[i];
	}

	if ( !cur )
		return "";

	return COM_FileExtension( cur );
}

CBaseFileSystem* BaseFileSystem()
{
	return CBaseFileSystem::s_pFileSystem;
}

CBaseFileSystem::CBaseFileSystem()
{
	s_pFileSystem = this;
	m_fwLevel = FILESYSTEM_WARNING_REPORTUNCLOSED;
	m_pfnWarning = NULL;
	m_nOpenCount = 0;
}

CBaseFileSystem::~CBaseFileSystem()
{
}

void CBaseFileSystem::Mount()
{
	g_flDummyFloat *= 2.0f;
}

void CBaseFileSystem::Unmount()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::CreateDirHierarchy( const char* path, const char* pathID )
{
	CSearchPath* searchPath = GetWritePath( pathID );

	int len = (int)( strlen( path ) + 1 + strlen( searchPath->m_Path.String() ) );
	char* buf = (char*)_alloca( len + 1 );
	strcpy( buf, searchPath->m_Path.String() );
	strcat( buf, path );
	FixSlashes( buf );

	char* s = buf;
	char* end = buf + len;
	while ( s < end )
	{
		if ( *s == CORRECT_PATH_SEPARATOR )
		{
			*s = '\0';
#ifdef _WIN32
			_mkdir( buf );
#else
			__wrap_mkdir( buf, FS_DIRECTORY_PERMISSIONS );
#endif
			*s = CORRECT_PATH_SEPARATOR;
		}
		s++;
	}
#ifdef _WIN32
	_mkdir( buf );
#else
	__wrap_mkdir( buf, FS_DIRECTORY_PERMISSIONS );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::PrintOpenedFiles()
{
	Trace_DumpUnclosedFiles();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : **ppdir - 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::GetCurrentDirectory( char* pDirectory, int maxlen )
{
#ifdef _WIN32
	if ( !::GetCurrentDirectoryA( maxlen, pDirectory ) )
#else
	if ( !getcwd( pDirectory, maxlen ) )
#endif
		return false;

	FixSlashes( pDirectory );

	// Strip the last slash
	int len = (int)strlen( pDirectory );
	if ( pDirectory[len - 1] == CORRECT_PATH_SEPARATOR )
	{
		pDirectory[len - 1] = 0;
	}

	return true;
}

void CBaseFileSystem::AddSearchPathNoWrite( const char* pPath, const char* pathID )
{
	AddSearchPathInternal( pPath, pathID, false );
}

void CBaseFileSystem::AddSearchPath( const char* pPath, const char* pathID )
{
	AddSearchPathInternal( pPath, pathID, true );
}

void CBaseFileSystem::AddSearchPathInternal( const char* pPath, const char* pathID, bool bAllowWrite )
{
	if ( strstr( pPath, ".bsp" ) )
		return;

	// Clean up the name
	int NewPathSize = (int)FS_SEARCH_PATH_CAPACITY;
	char* newPath = (char*)_alloca( NewPathSize );
#ifdef _WIN32
	if ( strchr( pPath, ':' ) )
#else
	if ( pPath[0] == CORRECT_PATH_SEPARATOR )
#endif
	{
		strncpy( newPath, pPath, NewPathSize );
		newPath[NewPathSize - 1] = 0;
	}
	else
	{
		GetCurrentDirectory( newPath, (int)FS_MAX_PATH_LENGTH );
		FixPath( newPath );
		if ( strcmp( pPath, "." ) )
		{
			strncat( newPath, pPath, NewPathSize );
			newPath[NewPathSize - 1] = 0;
		}
	}

#ifdef _WIN32
	_strlwr( newPath );
#endif
	FixPath( newPath );

	CUtlSymbol pathSymbol( newPath );
	CUtlSymbol pathIDSymbol( pathID );

	for ( int i = 0; i < m_SearchPaths.Count(); i++ )
	{
		CSearchPath* sp = &m_SearchPaths[i];
		if ( sp->m_Path == pathSymbol && sp->m_PathID == pathIDSymbol )
			return;
	}

	int newIndex = m_SearchPaths.AddToTail();
	CSearchPath* sp = &m_SearchPaths[newIndex];
	sp->m_bAllowWrite = bAllowWrite;
	sp->m_Path = pathSymbol;
	sp->m_PathID = pathIDSymbol;

	// Add pack files for this path next
	AddPackFiles( newPath );
}

bool CBaseFileSystem::RemoveSearchPath( const char* pPath )
{
	char* newPath = (char*)_alloca( strlen( pPath ) + 2 + FS_MAX_PATH_LENGTH );
#ifdef _WIN32
	if ( strchr( pPath, ':' ) )
#else
	if ( pPath[0] == CORRECT_PATH_SEPARATOR )
#endif
	{
		strcpy( newPath, pPath );
	}
	else
	{
		GetCurrentDirectory( newPath, (int)FS_MAX_PATH_LENGTH );
		FixPath( newPath );
		if ( strcmp( pPath, "." ) )
		{
			strcat( newPath, pPath );
		}
	}

#ifdef _WIN32
	_strlwr( newPath );
#endif
	FixPath( newPath );

	CUtlSymbol lookup = newPath;

	bool bret = false;

	// Count backward since we're possibly deleting one or more pack files, too
	int i;
	int c = m_SearchPaths.Count();
	for ( i = c - 1; i >= 0; i-- )
	{
		if ( m_SearchPaths[i].m_Path != lookup )
			continue;

		m_SearchPaths.Remove( i );
		bret = true;
	}
	return bret;
}

CBaseFileSystem::CSearchPath* CBaseFileSystem::GetWritePath( const char* pathID )
{
	int iPath = 0;
	CSearchPath* searchPath = &m_SearchPaths[iPath];
	while ( searchPath && iPath < m_SearchPaths.Count() && !searchPath->m_bAllowWrite )
	{
		searchPath = &m_SearchPaths[++iPath];
	}

	if ( pathID )
	{
		CUtlSymbol lookup( pathID );
		for ( int i = 0; i < m_SearchPaths.Count(); i++ )
		{
			if ( m_SearchPaths[i].m_PathID == lookup )
			{
				searchPath = &m_SearchPaths[i];
				break;
			}
		}
	}

	return searchPath;
}

bool CBaseFileSystem::IsDirectory( const char* pFileName )
{
	struct stat buf;
	int i;

	for ( i = 0; i < m_SearchPaths.Count(); i++ )
	{
		// Pack files can't be directories
		CSearchPath* sp = &m_SearchPaths[i];
		if ( !sp->m_bIsPackFile )
		{
			char* pTmpFileName = (char*)_alloca( strlen( pFileName ) + strlen( m_SearchPaths[i].m_Path.String() ) + 1 );
			strcpy( pTmpFileName, m_SearchPaths[i].m_Path.String() );
			strcat( pTmpFileName, pFileName );
			FixSlashes( pTmpFileName );

			if ( FS_stat( pTmpFileName, &buf ) != -1 )
			{
				if ( buf.st_mode & S_IFDIR )
					return true;
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: The base file search goes through here
// Input  : *path - 
//			*pFileName - 
//			*pOptions - 
//			packfile - 
//			*filetime - 
// Output : FileHandle_t
//-----------------------------------------------------------------------------
FileHandle_t CBaseFileSystem::FindFile( const CSearchPath* path, const char* pFileName, const char* pOptions,
		bool bFromCache )
{
	if ( path->m_bIsPackFile )
	{
		// Search the tree for the filename
		CPackFileEntry search;
		char* temp = (char*)_alloca( strlen( pFileName ) + 1 );
		strcpy( temp, pFileName );
		_strlwr( temp );
		FixSlashes( temp );

		search.m_Name = temp;

		int searchresult = path->m_PackFiles.Find( search );
		if ( searchresult == path->m_PackFiles.InvalidIndex() )
			return NULL;

		CPackFileEntry result = path->m_PackFiles[searchresult];

		FS_fseek( path->m_hPackFile->m_pFile, path->m_hPackFile->m_nStartOffset + result.m_nPosition, SEEK_SET );

		CFileHandle* fh = new CFileHandle;
		fh->m_pFile = path->m_hPackFile->m_pFile;
		fh->m_nStartOffset = result.m_nPosition;
		fh->m_nLength = result.m_nLength;
		fh->m_nFileTime = path->m_lPackFileTime;
		fh->m_nModificationTime = path->m_lPackFileTime;
		fh->m_bPack = true;
		return (FileHandle_t)fh;
	}

	bool bWrite = ( strchr( pOptions, 'w' ) || strchr( pOptions, '+' ) );
	if ( bWrite && !path->m_bAllowWrite )
		return NULL;

	char* pTmpFileName = (char*)_alloca( strlen( pFileName ) + 1 + strlen( path->m_Path.String() ) );
	strcpy( pTmpFileName, path->m_Path.String() );
	strcat( pTmpFileName, pFileName );
	FixSlashes( pTmpFileName );

	FILE* fp = Trace_FOpen( pTmpFileName, pOptions, bFromCache );
	if ( !fp )
		return NULL;

	CFileHandle* fh = new CFileHandle;
	fh->m_pFile = fp;
	fh->m_bPack = false;

	struct stat buf = {};
	int sr = FS_stat( pTmpFileName, &buf );
	if ( sr == -1 )
	{
		Warning( FILESYSTEM_WARNING, "_stat on file %s which appeared to exist failed!!!\n", pTmpFileName );
	}

	fh->m_nFileTime = (int)( buf.st_ctime > buf.st_mtime ? buf.st_ctime : buf.st_mtime );
	fh->m_nModificationTime = (int)buf.st_mtime;
	fh->m_nLength = buf.st_size;
	fh->m_nStartOffset = 0;
	return (FileHandle_t)fh;
}

bool CBaseFileSystem::FileExists( const char* pFileName )
{
	for ( int i = 0; i < m_SearchPaths.Count(); i++ )
	{
		int size = FastFindFileSize( &m_SearchPaths[i], pFileName );
		if ( size != -1 )
			return true;
	}
	return false;
}

FileHandle_t CBaseFileSystem::Open( const char* pFileName, const char* pOptions, const char* pathID )
{
	ASSERT_AT( !strchr(pFileName, ':'), 471 );

	// Try each of the search paths in succession
	// FIXME: call createdirhierarchy upon opening for write.
	if ( strstr( pOptions, "r" ) && !strstr( pOptions, "+" ) )
	{
		CUtlSymbol lookup;
		if ( pathID )
		{
			lookup = pathID;
		}

		for ( int i = 0; i < m_SearchPaths.Count(); i++ )
		{
			if ( pathID && m_SearchPaths[i].m_PathID != lookup )
				continue;

			FileHandle_t filehandle = FindFile( &m_SearchPaths[i], pFileName, pOptions, false );
			if ( filehandle )
				return filehandle;
		}

		return NULL;
	}

	CSearchPath* searchPath = GetWritePath( pathID );
	char* pTmpFileName = (char*)_alloca( strlen( pFileName ) + strlen( searchPath->m_Path.String() ) + 2 );
	strcpy( pTmpFileName, searchPath->m_Path.String() );
	strcat( pTmpFileName, pFileName );
	FixSlashes( pTmpFileName );

	FILE* fp = Trace_FOpen( pTmpFileName, pOptions, false );
	if ( !fp )
		return NULL;

	CFileHandle* fh = new CFileHandle;

	struct stat buf = {};
	int sr = FS_stat( pTmpFileName, &buf );
	if ( sr == -1 )
	{
		Warning( FILESYSTEM_WARNING, "_stat on file %s which appeared to exist failed!!!\n", pTmpFileName );
	}

	fh->m_nFileTime = (int)buf.st_mtime;
	fh->m_nModificationTime = (int)buf.st_mtime;
	fh->m_nLength = buf.st_size;
	fh->m_pFile = fp;
	fh->m_nStartOffset = 0;
	fh->m_bPack = false;
	return (FileHandle_t)fh;
}

FileHandle_t CBaseFileSystem::OpenFromCacheForRead( const char* pFileName, const char* pOptions, const char* pathID )
{
	CUtlSymbol lookup;
	if ( pathID )
	{
		lookup = pathID;
	}

	// Try each of the search paths in succession
	for ( int i = 0; i < m_SearchPaths.Count(); i++ )
	{
		if ( pathID && m_SearchPaths[i].m_PathID != lookup )
			continue;

		FileHandle_t filehandle = FindFile( &m_SearchPaths[i], pFileName, pOptions, true );
		if ( filehandle )
			return filehandle;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Close( FileHandle_t file )
{
	if ( !file )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Close NULL file handle!\n" );
		return;
	}

	CFileHandle* fh = (CFileHandle*)file;
	if ( !fh->m_pFile )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Close NULL file pointer inside valid file handle!\n" );
		return;
	}

	if ( m_PackFileHandles.Find( fh->m_pFile ) != -1 )
		return;

	Trace_FClose( fh->m_pFile );
	fh->m_pFile = NULL;

	delete fh;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Seek( FileHandle_t file, int pos, FileSystemSeek_t whence )
{
	if ( file == NULL )
	{
		Warning( FILESYSTEM_WARNING, "Tried to Seek NULL file handle!\n" );
		return;
	}

	CFileHandle* handle = (CFileHandle*)file;
	if ( handle->m_pFile == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Seek NULL file pointer inside valid file handle!\n" );
		return;
	}

	if ( whence == FILESYSTEM_SEEK_HEAD )
	{
		if ( handle->m_bPack != 0 )
		{
			long long offset = (long long)handle->m_nStartOffset + (long long)pos;
			FS_fseek( handle->m_pFile, offset, 0 );
			return;
		}
		FS_fseek( handle->m_pFile, (long long)pos, 0 );
		return;
	}

	if ( whence == FILESYSTEM_SEEK_CURRENT )
	{
		FS_fseek( handle->m_pFile, (long long)pos, 1 );
		return;
	}

	if ( handle->m_bPack != 0 )
	{
		long long offset = (long long)pos + (long long)handle->m_nStartOffset + (long long)handle->m_nLength;
		FS_fseek( handle->m_pFile, offset, 2 );
		return;
	}
	FS_fseek( handle->m_pFile, (long long)pos, 2 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Tell( FileHandle_t file )
{
	CBaseFileSystem::CFileHandle* fh;

	if ( file == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Tell NULL file handle!\n" );
		return 0;
	}

	fh = (CBaseFileSystem::CFileHandle*)file;
	if ( fh->m_pFile == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Tell NULL file pointer inside valid file handle!\n" );
		return 0;
	}

	return (unsigned int)FS_ftell( fh->m_pFile ) - (unsigned int)fh->m_nStartOffset;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : file - 
// Output : unsigned int
//-----------------------------------------------------------------------------
unsigned int CBaseFileSystem::Size( FileHandle_t file )
{
	CBaseFileSystem::CFileHandle* fh;

	if ( file == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Size NULL file handle!\n" );
		return 0;
	}

	fh = (CBaseFileSystem::CFileHandle*)file;
	if ( fh->m_pFile == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Size NULL file pointer inside valid file handle!\n" );
		return 0;
	}

	return (unsigned int)fh->m_nLength;
}

unsigned int CBaseFileSystem::Size( const char* pFileName )
{
	for ( int i = 0; i < m_SearchPaths.Count(); i++ )
	{
		int size = FastFindFileSize( &m_SearchPaths[i], pFileName );
		if ( size != -1 )
			return size;
	}
	return (unsigned int)-1;
}

int CBaseFileSystem::FastFindFileSize( const CSearchPath* path, const char* pFileName )
{
	struct stat buf;

	if ( path->m_bIsPackFile )
	{
		CPackFileEntry search;
		char* temp = (char*)_alloca( strlen( pFileName ) + 1 );
		strcpy( temp, pFileName );
		_strlwr( temp );
		FixSlashes( temp );

		search.m_Name = temp;

		int searchresult = path->m_PackFiles.Find( search );
		if ( searchresult == path->m_PackFiles.InvalidIndex() )
			return -1;

		CPackFileEntry result = path->m_PackFiles[searchresult];
		return (int)result.m_nLength;
	}

	char pTmpFileName[FS_FILE_LOOKUP_CAPACITY];
	ASSERT_AT( sizeof(pTmpFileName) > strlen(path->m_Path.String()) + strlen(pFileName) + 1, 720 );
	strcpy( pTmpFileName, path->m_Path.String() );
	strcat( pTmpFileName, pFileName );
	FixSlashes( pTmpFileName );
	if ( FS_stat( pTmpFileName, &buf ) != -1 )
		return buf.st_size;
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::EndOfFile( FileHandle_t file )
{
	if ( file == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to EndOfFile NULL file handle!\n" );
		return true;
	}

	CFileHandle* handle = (CFileHandle*)file;
	if ( handle->m_pFile == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to EndOfFile NULL file pointer inside valid file handle!\n" );
		return true;
	}

	if ( handle->m_bPack != 0 )
	{
		unsigned long long end = (unsigned long long)handle->m_nStartOffset +
				(unsigned long long)handle->m_nLength;
		return FS_ftell( handle->m_pFile ) >= end;
	}

	return FS_feof( handle->m_pFile ) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::Read( void* pOutput, int size, FileHandle_t file )
{
	CBaseFileSystem::CFileHandle* fh;
	int bytesRead;

	if ( file == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Read NULL file handle!\n" );
		return 0;
	}

	fh = (CBaseFileSystem::CFileHandle*)file;
	if ( fh->m_pFile == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Read NULL file pointer inside valid file handle!\n" );
		return 0;
	}

	bytesRead = (int)FS_fread( pOutput, 1, size, fh->m_pFile );
	if ( size != bytesRead )
	{
		fh->m_bErrorFlagged = true;
	}
	return bytesRead;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::Write( const void* pInput, int size, FileHandle_t file )
{
	CBaseFileSystem::CFileHandle* fh;

	if ( file == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Write NULL file handle!\n" );
		return 0;
	}

	fh = (CBaseFileSystem::CFileHandle*)file;
	if ( fh->m_pFile == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Write NULL file pointer inside valid file handle!\n" );
		return 0;
	}

	return (int)FS_fwrite( pInput, 1, size, fh->m_pFile );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseFileSystem::FPrintf( FileHandle_t file, char* pFormat, ... )
{
	va_list args;
	va_start( args, pFormat );

	int written = 0;
	if ( file == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to FS: fprintf NULL file handle!\n" );
	}
	else
	{
		CFileHandle* handle = (CFileHandle*)file;
		if ( handle->m_pFile == NULL )
		{
			Warning( FILESYSTEM_WARNING, "FS:  Tried to FS: fprintf NULL file pointer inside valid file handle!\n" );
		}
		else
		{
			written = (int)FS_vfprintf( handle->m_pFile, pFormat, args );
		}
	}

	va_end( args );
	return written;
}

void* CBaseFileSystem::GetReadBuffer( FileHandle_t, int* outBufferSize, bool )
{
	*outBufferSize = 0;
	return NULL;
}

void CBaseFileSystem::ReleaseReadBuffer( FileHandle_t, void* )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseFileSystem::IsOk( FileHandle_t file )
{
	CBaseFileSystem::CFileHandle* fh;

	if ( file == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to IsOk NULL file handle!\n" );
		return false;
	}

	fh = (CBaseFileSystem::CFileHandle*)file;
	if ( fh->m_pFile == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to IsOk NULL file pointer inside valid file handle!\n" );
		return false;
	}

	if ( fh->m_bErrorFlagged )
	{
		return false;
	}

	return FS_ferror( fh->m_pFile ) == 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Flush( FileHandle_t file )
{
	if ( file == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Flush NULL file handle!\n" );
		return;
	}

	CFileHandle* handle = (CFileHandle*)file;
	if ( handle->m_pFile == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to Flush NULL file pointer inside valid file handle!\n" );
		return;
	}

	FS_fflush( handle->m_pFile );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char* CBaseFileSystem::ReadLine( char* pOutput, int maxChars, FileHandle_t file )
{
	CBaseFileSystem::CFileHandle* fh;

	if ( file == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to ReadLine NULL file handle!\n" );
		return (char*)"";
	}

	fh = (CBaseFileSystem::CFileHandle*)file;
	if ( fh->m_pFile == NULL )
	{
		Warning( FILESYSTEM_WARNING, "FS:  Tried to ReadLine NULL file pointer inside valid file handle!\n" );
		return (char*)"";
	}

	return FS_fgets( pOutput, maxChars, fh->m_pFile );
}

void CBaseFileSystem::RemoveFile( const char* pRelativePath, const char* pathID )
{
	CSearchPath* searchPath = GetWritePath( pathID );
	char* pTmpFileName = (char*)_alloca( strlen( searchPath->m_Path.String() ) + strlen( pRelativePath ) + 2 );
	strcpy( pTmpFileName, searchPath->m_Path.String() );
	strcat( pTmpFileName, pRelativePath );
	FixSlashes( pTmpFileName );
#ifdef _WIN32
	_unlink( pTmpFileName );
#else
	__wrap_unlink( pTmpFileName );
#endif
}

const char* CBaseFileSystem::GetLocalPath( const char* pFileName, char* pLocalPath, int localPathBufferSize )
{
	if ( strchr( pFileName, ':' ) )
	{
		strncpy( pLocalPath, pFileName, localPathBufferSize );
		pLocalPath[localPathBufferSize - 1] = 0;
		FixSlashes( pLocalPath );
		return pLocalPath;
	}

	const char* pExt = COM_LastFileExtension( pFileName );
	bool bDisableUnsafePaths = !strcmp( pExt, "dll" ) || !strcmp( pExt, "dylib" ) || !strcmp( pExt, "so" );

	struct stat buf;
	for ( int i = 0; i < m_SearchPaths.Count(); i++ )
	{
		if ( bDisableUnsafePaths && strstr( m_SearchPaths[i].m_Path.String(), "_downloads" ) )
			continue;

		if ( m_SearchPaths[i].m_bIsPackFile )
		{
			CPackFileEntry search;
			char* temp = (char*)_alloca( strlen( pFileName ) + 1 );
			strcpy( temp, pFileName );
			_strlwr( temp );
			FixSlashes( temp );

			search.m_Name = temp;

			int searchresult = m_SearchPaths[i].m_PackFiles.Find( search );
			if ( searchresult == m_SearchPaths[i].m_PackFiles.InvalidIndex() )
				continue;

			char* pTmpFileName = (char*)_alloca( strlen( pFileName ) + 1 );
			strcpy( pTmpFileName, pFileName );
			_snprintf( pLocalPath, localPathBufferSize - 1, "%s%s", m_SearchPaths[i].m_Path.String(), pTmpFileName );
			pLocalPath[localPathBufferSize - 1] = 0;
			FixSlashes( pLocalPath );
			return pLocalPath;
		}
		else
		{
			char* pTmpFileName = (char*)_alloca( strlen( m_SearchPaths[i].m_Path.String() ) + strlen( pFileName ) + 1 );
			strcpy( pTmpFileName, m_SearchPaths[i].m_Path.String() );
			strcat( pTmpFileName, pFileName );
			FixSlashes( pTmpFileName );
			if ( FS_stat( pTmpFileName, &buf ) != -1 )
			{
				strncpy( pLocalPath, pTmpFileName, localPathBufferSize );
				pLocalPath[localPathBufferSize - 1] = 0;
				return pLocalPath;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*options - 
// Output : FILE
//-----------------------------------------------------------------------------
FILE* CBaseFileSystem::Trace_FOpen( const char* filename, const char* options, bool bFromCache )
{
	FILE* fp = FS_fopen( filename, options, bFromCache );

	if ( fp )
	{
		COpenedFile file;

		file.SetName( filename );
		file.m_pFile = fp;

		//m_OpenedFiles.Insert( file );
		m_OpenedFiles.AddToTail( file );

		m_nOpenCount++;

		if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES )
		{
			Warning( FILESYSTEM_WARNING_REPORTALLACCESSES, "---FS:  open %s %p %i\n", filename, fp, m_nOpenCount );
		}
	}

	return fp;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fp - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Trace_FClose( FILE* fp )
{
	if ( fp )
	{
		COpenedFile file;
		file.m_pFile = fp;

		int result = m_OpenedFiles.Find( file );
		if ( result != -1 )
		{
			m_nOpenCount--;

			COpenedFile found = m_OpenedFiles[result];
			if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES )
			{
				Warning( FILESYSTEM_WARNING_REPORTALLACCESSES, "---FS:  close %s %p %i\n", found.GetName(), fp,
						m_nOpenCount );
			}
			m_OpenedFiles.FindAndRemove( found );
		}
		else
		{
			ASSERT_AT( 0, 1057 );

			if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTALLACCESSES )
			{
				Warning( FILESYSTEM_WARNING_REPORTALLACCESSES, "Tried to close unknown file pointer %p\n", fp );
			}
		}

		if ( result != -1 )
			FS_fclose( fp );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Trace_DumpUnclosedFiles()
{
	for ( int i = 0; i < m_OpenedFiles.Count(); i++ )
	{
		COpenedFile* found = &m_OpenedFiles[i];

		if ( m_fwLevel >= FILESYSTEM_WARNING_REPORTUNCLOSED )
		{
			Warning( FILESYSTEM_WARNING_REPORTUNCLOSED, "File %s was never closed\n", found->GetName() );
		}
	}
}

bool CBaseFileSystem::AddPackFile( const char* pFileName, const char* pathID )
{
	return AddPackFileFromPath( "", pFileName, true, pathID );
}

bool CBaseFileSystem::AddPackFileFromPath( const char* pPath, const char* pakfile, bool bCheckForAppendedPack,
		const char* pathID )
{
	char fullpath[FS_MAX_PATH_LENGTH];
	snprintf( fullpath, sizeof( fullpath ), "%s%s", pPath, pakfile );
	FixSlashes( fullpath );

	struct stat buf;
	if ( FS_stat( fullpath, &buf ) == -1 )
		return false;

	int size = m_SearchPaths.Count();
	m_SearchPaths.AddToTail();
	CSearchPath* sp = &m_SearchPaths[size];

	sp->m_Path = pPath;
	sp->m_PathID = pathID;
	sp->m_bIsPackFile = true;
	sp->m_lPackFileTime = GetFileTime( pakfile );
	sp->m_hPackFile = new CFileHandle;
	sp->m_hPackFile->m_pFile = Trace_FOpen( fullpath, "rb", false );

	long long headeroffset = 0;

	if ( bCheckForAppendedPack )
	{
		packappenededheader_t appended;
		FS_fseek( sp->m_hPackFile->m_pFile, -(long long)sizeof( appended ), SEEK_END );
		FS_fread( &appended, 1, sizeof( appended ), sp->m_hPackFile->m_pFile );

		if ( memcmp( appended.id, FS_APPENDED_PACK_MARKER, sizeof( appended.id ) ) )
		{
			m_SearchPaths.Remove( size );
			return false;
		}

		headeroffset = appended.packheaderpos;
	}

	if ( PreparePackFile( *sp, headeroffset ) )
	{
		m_PackFileHandles.AddToTail( sp->m_hPackFile->m_pFile );
		return true;
	}

	// Failed for some reason, ignore it
	Trace_FClose( sp->m_hPackFile->m_pFile );
	m_SearchPaths.Remove( size );
	return false;
}

bool CBaseFileSystem::PreparePackFile( CSearchPath& packfile, long long offsetofpackinmetafile )
{
	packheader_t header;
	int i;
	packfile_t* newfiles;
	int numpackfiles;

	ASSERT_AT( packfile.m_bIsPackFile, 1175 );
	ASSERT_AT( packfile.m_hPackFile, 1176 );

	FS_fseek( packfile.m_hPackFile->m_pFile, packfile.m_hPackFile->m_nStartOffset + offsetofpackinmetafile, SEEK_SET );
	FS_fread( &header, 1, sizeof( header ), packfile.m_hPackFile->m_pFile );

	if ( header.id[0] == 'P' && header.id[1] == 'K' && header.id[2] == '6' && header.id[3] == '4' )
	{
		return Prepare64BitPackFile( packfile, offsetofpackinmetafile );
	}

	if ( header.id[0] != 'P' || header.id[1] != 'A' || header.id[2] != 'C' || header.id[3] != 'K' )
	{
		Warning( FILESYSTEM_WARNING, "%s is not a packfile", packfile.m_Path.String() );
		return false;
	}

	numpackfiles = header.dirlen / sizeof( packfile_t );

	if ( numpackfiles > FS_MAX_PACK_FILES )
	{
		Warning( FILESYSTEM_WARNING, "%s has %i files", packfile.m_Path.String(), numpackfiles );
		return false;
	}

	if ( !numpackfiles )
		// No files, sigh...
		return false;

	newfiles = new packfile_t[numpackfiles];
	ASSERT_AT( newfiles, 1212 );
	if ( !newfiles )
	{
		Warning( FILESYSTEM_WARNING, "%s out of memory allocating directoryf for %i files", packfile.m_Path.String(),
				numpackfiles );
		return false;
	}

	FS_fseek( packfile.m_hPackFile->m_pFile,
			packfile.m_hPackFile->m_nStartOffset + header.dirofs + offsetofpackinmetafile, SEEK_SET );
	Read( newfiles, header.dirlen, packfile.m_hPackFile );

	// Insert into rb tree
	for ( i = 0; i < numpackfiles; i++ )
	{
		CPackFileEntry lookup;
		_strlwr( newfiles[i].name );
		FixSlashes( newfiles[i].name );
		lookup.m_Name = newfiles[i].name;
		lookup.m_nPosition = newfiles[i].filepos + offsetofpackinmetafile;
		lookup.m_nLength = newfiles[i].filelen;
		packfile.m_PackFiles.Insert( lookup );
	}

	packfile.m_nNumPackFiles = numpackfiles;
	delete[] newfiles;
	return true;
}

bool CBaseFileSystem::Prepare64BitPackFile( CSearchPath& packfile, long long offsetofpackinmetafile )
{
	packheader64_t header;
	packfile64_t* newfiles;
	int numpackfiles;

	FS_fseek( packfile.m_hPackFile->m_pFile, packfile.m_hPackFile->m_nStartOffset + offsetofpackinmetafile, SEEK_SET );
	FS_fread( &header, 1, sizeof( header ), packfile.m_hPackFile->m_pFile );

	if ( header.id[0] != 'P' || header.id[1] != 'K' || header.id[2] != '6' || header.id[3] != '4' )
	{
		Warning( FILESYSTEM_WARNING, "%s is not a packfile", packfile.m_Path.String() );
		return false;
	}

	numpackfiles = (int)( header.dirlen / (long long)sizeof( packfile64_t ) );

	if ( numpackfiles > FS_MAX_PACK_FILES )
	{
		Warning( FILESYSTEM_WARNING, "%s has %i files", packfile.m_Path.String(), numpackfiles );
		return false;
	}

	if ( numpackfiles <= 0 )
		return false;

	newfiles = new packfile64_t[numpackfiles];
	ASSERT_AT( newfiles, 1278 );
	if ( !newfiles )
	{
		Warning( FILESYSTEM_WARNING, "%s out of memory allocating directoryf for %i files", packfile.m_Path.String(),
				numpackfiles );
		return false;
	}

	FS_fseek( packfile.m_hPackFile->m_pFile,
			packfile.m_hPackFile->m_nStartOffset + header.dirofs + offsetofpackinmetafile, SEEK_SET );
	Read( newfiles, (int)header.dirlen, packfile.m_hPackFile );

	for ( int i = 0; i < numpackfiles; i++ )
	{
		CPackFileEntry lookup;
		_strlwr( newfiles[i].name );
		FixSlashes( newfiles[i].name );
		lookup.m_Name = newfiles[i].name;
		lookup.m_nPosition = newfiles[i].filepos + offsetofpackinmetafile;
		lookup.m_nLength = newfiles[i].filelen;
		packfile.m_PackFiles.Insert( lookup );
	}

	packfile.m_nNumPackFiles = numpackfiles;
	delete[] newfiles;
	return true;
}

void CBaseFileSystem::AddPackFiles( const char* pPath )
{
	int pakcount;
	int i;
	for ( pakcount = 0; ; pakcount++ )
	{
		char pakfile[512];
		char fullpath[512];
		_snprintf( pakfile, sizeof( pakfile ), "pak%i.pak", pakcount );
		_snprintf( fullpath, sizeof( fullpath ), "%s%s", pPath, pakfile );

		FixSlashes( fullpath );

		struct stat buf;
		if ( FS_stat( fullpath, &buf ) == -1 )
			break;
	}

	for ( i = pakcount - 1; i >= 0; i-- )
	{
		char pakfile[512];
		_snprintf( pakfile, sizeof( pakfile ), "pak%i.pak", i );
		AddPackFileFromPath( pPath, pakfile, false, "" );
	}
}

void CBaseFileSystem::RemoveAllMapSearchPaths()
{
	int c = m_SearchPaths.Count();
	for ( int i = c - 1; i >= 0; i-- )
	{
		if ( !m_SearchPaths[i].m_bIsMapPath )
			continue;

		m_SearchPaths.Remove( i );
	}
}

static bool WildCardMatch( const char* name, const char* mask )
{
	if ( !strcmp( name, "." ) || !strcmp( name, ".." ) )
		return false;

	if ( !strcmp( mask, "*.*" ) )
		return true;

	while ( *mask && *name )
	{
		if ( *mask == '*' )
		{
			mask++;
			if ( !*mask )
				return true;

			while ( toupper( *name ) != toupper( *mask ) )
			{
				name++;
				if ( !*name )
					return false;
			}
		}
		else if ( *mask == '?' )
		{
			mask++;
			name++;
		}
		else
		{
			if ( toupper( *mask ) != toupper( *name ) )
				return false;
			mask++;
			name++;
		}
	}

	return !*mask && !*name;
}

const char* CBaseFileSystem::SearchPakFile( const char* pWildCard, int currentSearchPathID, bool first )
{
	if ( first )
		m_SearchPaths[currentSearchPathID].m_iCurSearchFile = 0;

	while ( m_SearchPaths[currentSearchPathID].m_iCurSearchFile <
			(unsigned int)m_SearchPaths[currentSearchPathID].m_PackFiles.Count() )
	{
		const char* file = m_SearchPaths[currentSearchPathID].m_PackFiles[
				m_SearchPaths[currentSearchPathID].m_iCurSearchFile].m_Name.String();
		if ( WildCardMatch( file, pWildCard ) )
		{
			m_SearchPaths[currentSearchPathID].m_iCurSearchFile++;
			return file;
		}
		m_SearchPaths[currentSearchPathID].m_iCurSearchFile++;
	}

	return NULL;
}

const char* CBaseFileSystem::FindFirst( const char* pWildCard, FileFindHandle_t* pHandle, const char* pathID )
{
	ASSERT_AT( pWildCard, 1450 );
	ASSERT_AT( pHandle, 1451 );

	FileFindHandle_t hTmpHandle = m_FindData.AddToTail();
	FindData_t* pFindData = &m_FindData[hTmpHandle];
	ASSERT_AT( pFindData, 1456 );

	int maxlen = (int)strlen( pWildCard ) + 1;
	pFindData->wildCardString.AddMultipleToTail( maxlen );
	strcpy( pFindData->wildCardString.Base(), pWildCard );
	FixSlashes( pFindData->wildCardString.Base() );

	CUtlSymbol lookup;
	lookup = pathID;

	for ( pFindData->currentSearchPathID = 0;
		  pFindData->currentSearchPathID < m_SearchPaths.Count();
		  pFindData->currentSearchPathID++ )
	{
		if ( pathID )
		{
			if ( m_SearchPaths[pFindData->currentSearchPathID].m_PathID != lookup )
				continue;
			pFindData->limitedPathID = lookup;
		}
		else
		{
			pFindData->limitedPathID = -1;
		}

		const char* fileName = FindFirstHelper( pWildCard, pHandle, pFindData->currentSearchPathID, pFindData );
		if ( fileName )
			return fileName;
	}

	m_FindData.Remove( hTmpHandle );
	return NULL;
}

const char* CBaseFileSystem::FindFirstHelper( const char*, FileFindHandle_t* pHandle, int searchPath,
		FindData_t* pFindData )
{
	if ( m_SearchPaths[searchPath].m_bIsPackFile )
	{
		const char* fname = SearchPakFile( pFindData->wildCardString.Base(), searchPath, true );
		if ( fname )
		{
			pFindData->findHandle = INVALID_HANDLE_VALUE;
			*pHandle = m_FindData.Count() - 1;
			return fname;
		}
	}
	else
	{
		char* pTmpFileName = (char*)_alloca( strlen( m_SearchPaths[searchPath].m_Path.String() ) +
				pFindData->wildCardString.Count() + 1 );
		strcpy( pTmpFileName, m_SearchPaths[searchPath].m_Path.String() );
		strcat( pTmpFileName, pFindData->wildCardString.Base() );
		FixSlashes( pTmpFileName );

		pFindData->findHandle = FS_FindFirstFile( pTmpFileName, &pFindData->findData );
		if ( pFindData->findHandle != INVALID_HANDLE_VALUE )
		{
			*pHandle = m_FindData.Count() - 1;
			return pFindData->findData.cFileName;
		}
	}

	return NULL;
}

// assumes that pSearchWildcard has the proper slash convention.
bool CBaseFileSystem::FileInSearchPaths( const char* pSearchWildcard, const char* pFileName, int minSearchPathID,
		int maxSearchPathID )
{
	if ( minSearchPathID > maxSearchPathID )
		return false;

	// FIGURE OUT THE FILENAME WITHOUT THE SEARCH PATH.
	const char* tmp = &pSearchWildcard[strlen( pSearchWildcard ) - 1];
	while ( *tmp != '/' && tmp > pSearchWildcard )
		tmp--;
	tmp++;

	if ( tmp <= pSearchWildcard )
		return false;

	int pathStrLen = (int)( tmp - pSearchWildcard );
	int fileNameStrLen = (int)strlen( pFileName );
	char* pFileNameWithPath = (char*)_alloca( pathStrLen + fileNameStrLen + 1 );
	memcpy( pFileNameWithPath, pSearchWildcard, pathStrLen );
	pFileNameWithPath[pathStrLen] = '\0';
	strcat( pFileNameWithPath, pFileName );

	for ( int i = minSearchPathID; i <= maxSearchPathID; i++ )
	{
		// FIXME: Should this work with PAK files?
		if ( m_SearchPaths[i].m_bIsPackFile )
		{
			unsigned int curSearch = m_SearchPaths[i].m_iCurSearchFile;
			const char* ret = SearchPakFile( pSearchWildcard, i, true );
			m_SearchPaths[i].m_iCurSearchFile = curSearch;
			if ( ret )
				return true;
		}
		else
		{
			char* fullFilePath = (char*)_alloca( strlen( m_SearchPaths[i].m_Path.String() ) +
					strlen( pFileNameWithPath ) + 1 );
			strcpy( fullFilePath, m_SearchPaths[i].m_Path.String() );
			strcat( fullFilePath, pFileNameWithPath );

			struct stat buf;
			if ( FS_stat( fullFilePath, &buf ) != -1 )
				return true;
		}
	}

	return false;
}

bool CBaseFileSystem::FindNextFileHelper( FindData_t* pFindData )
{
	// PAK files???
	ASSERT_AT( pFindData, 1608 );

	// Try the same search path that we were already searching on.
	if ( m_SearchPaths[pFindData->currentSearchPathID].m_bIsPackFile )
	{
		const char* file = SearchPakFile( pFindData->wildCardString.Base(), pFindData->currentSearchPathID, false );
		if ( file )
		{
			const char* pFileNameNoPath = strrchr( file, CORRECT_PATH_SEPARATOR );
			if ( pFileNameNoPath )
				file = pFileNameNoPath + 1;
			strcpy( pFindData->findData.cFileName, file );
			FixSlashes( pFindData->findData.cFileName );
			return true;
		}
	}
	else
	{
		if ( FS_FindNextFile( pFindData->findHandle, &pFindData->findData ) )
			return true;
	}

	pFindData->currentSearchPathID++;

	if ( pFindData->findHandle != INVALID_HANDLE_VALUE )
	{
		FS_FindClose( pFindData->findHandle );
	}
	pFindData->findHandle = INVALID_HANDLE_VALUE;

	while ( pFindData->currentSearchPathID < m_SearchPaths.Count() )
	{
		if ( pFindData->limitedPathID != -1 &&
			 m_SearchPaths[pFindData->currentSearchPathID].m_PathID != pFindData->limitedPathID )
			return false;

		if ( m_SearchPaths[pFindData->currentSearchPathID].m_bIsPackFile )
		{
			const char* file = SearchPakFile( pFindData->wildCardString.Base(), pFindData->currentSearchPathID, true );
			if ( file )
			{
				strcpy( pFindData->findData.cFileName, strrchr( file, CORRECT_PATH_SEPARATOR ) + 1 );
				FixSlashes( pFindData->findData.cFileName );
				return true;
			}
		}
		else
		{
			char* pTmpFileName = (char*)_alloca(
					strlen( m_SearchPaths[pFindData->currentSearchPathID].m_Path.String() ) +
					pFindData->wildCardString.Count() ); // don't need two NULL terminators.
			strcpy( pTmpFileName, m_SearchPaths[pFindData->currentSearchPathID].m_Path.String() );
			strcat( pTmpFileName, pFindData->wildCardString.Base() );
			FixSlashes( pTmpFileName );

			pFindData->findHandle = FS_FindFirstFile( pTmpFileName, &pFindData->findData );
			if ( pFindData->findHandle != INVALID_HANDLE_VALUE )
				return true;
		}

		pFindData->currentSearchPathID++;
	}

	return false;
}

const char* CBaseFileSystem::FindNext( FileFindHandle_t handle )
{
	if ( !m_FindData.Count() || handle >= m_FindData.Count() )
		return NULL;

	FindData_t* pFindData = &m_FindData[handle];
	ASSERT_AT( pFindData, 1690 );

	if ( FindNextFileHelper( pFindData ) )
		return pFindData->findData.cFileName;

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FindIsDirectory( FileFindHandle_t handle )
{
	FindData_t* pFindData = &m_FindData[handle];
#ifdef _WIN32
	return !!( pFindData->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY );
#else
	return !!( pFindData->findData.dwFileAttributes & S_IFDIR );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : handle - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::FindClose( FileFindHandle_t handle )
{
	if ( !m_FindData.Count() || handle >= m_FindData.Count() )
		return;

	FindData_t* pFindData = &m_FindData[handle];
	ASSERT_AT( pFindData, 1728 );

	if ( pFindData->findHandle != INVALID_HANDLE_VALUE )
	{
		FS_FindClose( pFindData->findHandle );
	}
	pFindData->findHandle = NULL;

	pFindData->wildCardString.Purge();
	m_FindData.FastRemove( handle );
}

characterset_t g_BreakSet, g_BreakSetIncludingColons;

static void InitializeCharacterSets()
{
	static bool s_CharacterSetInitialized = false;
	if ( !s_CharacterSetInitialized )
	{
		CharacterSetBuild( &g_BreakSet, "{}()'" );
		CharacterSetBuild( &g_BreakSetIncludingColons, "{}()':" );
		s_CharacterSetInitialized = true;
	}
}

char* CBaseFileSystem::ParseFile( char* pFileBytes, char* pToken, bool* pWasQuoted )
{
	if ( pWasQuoted )
		*pWasQuoted = false;

	if ( !pFileBytes )
		return NULL;

	InitializeCharacterSets();

	// YWB:  Ignore colons as token separators in COM_Parse
	static bool com_ignorecolons = false;
	characterset_t* breaks = com_ignorecolons ? &g_BreakSet : &g_BreakSetIncludingColons;

	int c;
	int len = 0;
	pToken[0] = 0;

	// skip whitespace
skipwhite:
	while ( ( c = *pFileBytes ) <= ' ' )
	{
		if ( c == 0 )
			return NULL;	// end of file;
		pFileBytes++;
	}

	// skip // comments
	if ( c == '/' && pFileBytes[1] == '/' )
	{
		while ( *pFileBytes && *pFileBytes != '\n' )
			pFileBytes++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if ( c == '\"' )
	{
		if ( pWasQuoted )
			*pWasQuoted = true;

		pFileBytes++;
		for ( ;; )
		{
			c = *pFileBytes++;
			if ( c == '\"' || !c )
			{
				pToken[len] = 0;
				return pFileBytes;
			}
			pToken[len] = (char)c;
			len++;
		}
	}

	if ( breaks->set[(unsigned char)c] )
	{
		pToken[len] = (char)c;
		len++;
		pToken[len] = 0;
		return pFileBytes + 1;
	}

	// parse a regular word
	do
	{
		pToken[len] = (char)c;
		pFileBytes++;
		len++;
		c = *pFileBytes;
		if ( breaks->set[(unsigned char)c] )
			break;
	}
	while ( c > 32 );

	pToken[len] = 0;
	return pFileBytes;
}

int CBaseFileSystem::GetFileTime( const char* pFileName )
{
	for ( int i = 0; i < m_SearchPaths.Count(); i++ )
	{
		FileHandle_t filehandle = FindFile( &m_SearchPaths[i], pFileName, "rb", false );
		if ( filehandle )
		{
			CFileHandle* fh = (CFileHandle*)filehandle;
			int time = fh->m_nFileTime;
			Close( filehandle );
			return time;
		}
	}
	return 0;
}

int CBaseFileSystem::GetFileModificationTime( const char* pFileName )
{
	for ( int i = 0; i < m_SearchPaths.Count(); i++ )
	{
		FileHandle_t filehandle = FindFile( &m_SearchPaths[i], pFileName, "rb", false );
		if ( filehandle )
		{
			CFileHandle* fh = (CFileHandle*)filehandle;
			int time = fh->m_nModificationTime;
			Close( filehandle );
			return time;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pString - 
//			maxCharsIncludingTerminator - 
//			fileTime - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::FileTimeToString( char* pString, int maxCharsIncludingTerminator, long fileTime )
{
	time_t timeTyped = fileTime;
	char* src = ctime( &timeTyped );

	strncpy( pString, src, maxCharsIncludingTerminator );

	pString[maxCharsIncludingTerminator - 1] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pfnWarning - warning function callback
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetWarningFunc( void ( *pfnWarning )( const char*, ... ) )
{
	m_pfnWarning = pfnWarning;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : level - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::SetWarningLevel( FileWarningLevel_t level )
{
	m_fwLevel = level;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : level - 
//			*fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::Warning( FileWarningLevel_t level, const char* fmt, ... )
{
	if ( m_fwLevel >= level )
	{
		char warningtext[FS_WARNING_TEXT_LENGTH];
		va_list args;
		va_start( args, fmt );
#ifdef _WIN32
		_vsnprintf( warningtext, FS_WARNING_TEXT_LENGTH, fmt, args );
#else
		vsnprintf( warningtext, FS_WARNING_TEXT_LENGTH, fmt, args );
#endif
		va_end( args );

		// Dump to stdio
		printf( warningtext );
		if ( m_pfnWarning != NULL )
		{
			m_pfnWarning( warningtext );
		}
		else
		{
#ifdef _WIN32
			OutputDebugStringA( warningtext );
#else
			fprintf( stderr, "%s", warningtext );
#endif
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true on success ( based on current list of search paths, otherwise false if 
//  it can't be resolved )
// Input  : *pFullpath - 
//			*pRelative - 
//			maxRelativePathLength - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::FullPathToRelativePath( const char* pFullpath, char* pRelative )
{
	bool success = false;

	int inlen = (int)strlen( pFullpath );
	if ( inlen <= 0 )
	{
		pRelative[0] = 0;
		return success;
	}

	strcpy( pRelative, pFullpath );

	char* inpath = (char*)_alloca( inlen + 1 );
	ASSERT_AT( inpath, 1966 );
	strcpy( inpath, pFullpath );
	FixSlashes( inpath );
	_strlwr( inpath );

	for ( int i = 0; i < m_SearchPaths.Count() && !success; i++ )
	{
		// FIXME: Should this work with embedded pak files
		if ( m_SearchPaths[i].m_bIsMapPath )
			continue;

		char* searchbase = new char[strlen( m_SearchPaths[i].m_Path.String() ) + 1];
		ASSERT_AT( searchbase, 1980 );
		strcpy( searchbase, m_SearchPaths[i].m_Path.String() );
		FixSlashes( searchbase );
		_strlwr( searchbase );

		if ( !_strnicmp( searchbase, inpath, strlen( searchbase ) ) )
		{
			success = true;
			strcpy( pRelative, &inpath[strlen( searchbase )] );
		}

		delete[] searchbase;
	}

	return success;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src1 - 
//			src2 - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::OpenedFileLessFunc( const COpenedFile& src1, const COpenedFile& src2 )
{
	return src1.m_pFile < src2.m_pFile;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseFileSystem::RemoveAllSearchPaths()
{
	m_SearchPaths.Purge();
	m_PackFileHandles.Purge();
}

void CBaseFileSystem::StripFilename( char* path )
{
	int length;

	length = (int)strlen( path ) - 1;
	while ( length > 0 && path[length] != '/' )
		length--;
	path[length] = 0;
}

void CBaseFileSystem::FixSlashes( char* str )
{
	while ( *str )
	{
		if ( *str == INCORRECT_PATH_SEPARATOR )
		{
			*str = CORRECT_PATH_SEPARATOR;
		}
		str++;
	}
}

void CBaseFileSystem::FixPath( char* str )
{
	char* lastChar = &str[strlen( str ) - 1];
	if ( *lastChar != CORRECT_PATH_SEPARATOR && *lastChar != INCORRECT_PATH_SEPARATOR )
	{
		lastChar[1] = CORRECT_PATH_SEPARATOR;
		lastChar[2] = '\0';
	}

	FixSlashes( str );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::COpenedFile()
{
	m_pFile = NULL;
	m_pName = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::~COpenedFile()
{
	delete[] m_pName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
//-----------------------------------------------------------------------------
CBaseFileSystem::COpenedFile::COpenedFile( const CBaseFileSystem::COpenedFile& src )
{
	m_pFile = src.m_pFile;
	if ( src.m_pName )
	{
		m_pName = new char[strlen( src.m_pName ) + 1];
		strcpy( m_pName, src.m_pName );
	}
	else
	{
		m_pName = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseFileSystem::COpenedFile::operator==( const CBaseFileSystem::COpenedFile& src ) const
{
	return src.m_pFile == m_pFile;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CBaseFileSystem::COpenedFile::SetName( const char* name )
{
	delete[] m_pName;
	m_pName = new char[strlen( name ) + 1];
	strcpy( m_pName, name );
}

const char* CBaseFileSystem::COpenedFile::GetName()
{
	return m_pName ? m_pName : "???";
}

bool CBaseFileSystem::CSearchPath::PackFileLessFunc( const CBaseFileSystem::CPackFileEntry& src1,
		const CBaseFileSystem::CPackFileEntry& src2 )
{
	return src1.m_Name < src2.m_Name;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath::CSearchPath() : m_PackFiles( 0, FS_INITIAL_PACK_CAPACITY, PackFileLessFunc )
{
	m_Path = "";
	m_bIsMapPath = false;
	m_bIsPackFile = false;
	m_lPackFileTime = 0L;
	m_nNumPackFiles = 0;
	m_bAllowWrite = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseFileSystem::CSearchPath::~CSearchPath()
{
	if ( m_bIsPackFile && m_hPackFile )
	{
		s_pFileSystem->m_PackFileHandles.FindAndRemove( m_hPackFile->m_pFile );
		s_pFileSystem->Close( m_hPackFile );
	}
}
