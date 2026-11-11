//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "basefilesystem.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <io.h>

#define FILESYSTEM_STDIO_SOURCE	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\filesystem\\FileSystem_Stdio\\FileSystem_Stdio.cpp"
#endif

CFileSystem_Stdio::~CFileSystem_Stdio()
{
	CBaseFileSystem::RemoveAllSearchPaths();
	CBaseFileSystem::Trace_DumpUnclosedFiles();
#ifdef _WIN32
	if ( m_bMounted )
		_wassert( L"!m_bMounted", FILESYSTEM_STDIO_SOURCE, 84 );
#endif
}

void CFileSystem_Stdio::CancelWaitForResources( int )
{
}

bool CFileSystem_Stdio::IsAppReadyForOfflinePlay( int )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::FS_fclose( FILE* fp )
{
	fclose( fp );
}

void CFileSystem_Stdio::FS_fseek( FILE* fp, long long pos, int seekType )
{
	int ret = fseek( fp, (long)pos, seekType );
#ifdef _WIN32
	if ( ret != 0 )
		_wassert( L"ret == 0", FILESYSTEM_STDIO_SOURCE, 147 );
#else
	(void)ret;
#endif
}

int CFileSystem_Stdio::FS_ftell( FILE* fp )
{
	return ftell( fp );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_feof( FILE* fp )
{
	return feof( fp );
}

size_t CFileSystem_Stdio::FS_fread( void* dest, unsigned int count, unsigned int size, FILE* fp )
{
#ifdef _WIN32
	return read( fileno( fp ), dest, size * count );
#else
	return fread( dest, count, size, fp );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Stdio::FS_fwrite( const void* src, size_t count, size_t size, FILE* fp )
{
	return fwrite( src, count, size, fp );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
size_t CFileSystem_Stdio::FS_vfprintf( FILE* fp, const char* fmt, va_list list )
{
	return vfprintf( fp, fmt, list );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_ferror( FILE* fp )
{
	return ferror( fp );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
int CFileSystem_Stdio::FS_fflush( FILE* fp )
{
	return fflush( fp );
}

//-----------------------------------------------------------------------------
// Purpose: low-level filesystem wrapper
//-----------------------------------------------------------------------------
char* CFileSystem_Stdio::FS_fgets( char* dest, int destSize, FILE* fp )
{
	return fgets( dest, destSize, fp );
}

int CFileSystem_Stdio::FS_stat( const char* path, struct stat* buf )
{
#ifdef _WIN32
	return stat( path, buf );
#else
	int status;
	const char* matched;

	status = __wrap___xstat( FS_LINUX_STAT_VERSION, path, buf );
	if ( status == -1 )
	{
		matched = findFileInDirCaseInsensitive( path );
		if ( matched != NULL )
		{
			return __wrap___xstat( FS_LINUX_STAT_VERSION, matched, buf );
		}
	}

	return status;
#endif
}

HANDLE CFileSystem_Stdio::FS_FindFirstFile( char* findname, FIND_DATA* dat )
{
#ifdef _WIN32
	return ::FindFirstFileA( findname, dat );
#else
	return (HANDLE)::FindFirstFile( findname, dat );
#endif
}

bool CFileSystem_Stdio::FS_FindNextFile( HANDLE handle, FIND_DATA* dat )
{
#ifdef _WIN32
	return ::FindNextFileA( handle, dat ) != FALSE;
#else
	return ::FindNextFile( (int)handle, dat );
#endif
}

bool CFileSystem_Stdio::FS_FindClose( HANDLE handle )
{
#ifdef _WIN32
	return ::FindClose( handle ) != FALSE;
#else
	return ::FindClose( (int)handle );
#endif
}

void CFileSystem_Stdio::GetInterfaceVersion( char* p, int maxlen )
{
	strncpy( p, "Stdio", maxlen );
}

FILE* CFileSystem_Stdio::FS_fopen( const char* filename, const char* options, bool bFromCache )
{
#ifdef _WIN32
	(void)bFromCache;
	if ( strchr( filename, '\n' ) || strchr( filename, '\r' ) )
		_wassert( L"!strchr(filename, '\\n') && !strchr(filename, '\\r')", FILESYSTEM_STDIO_SOURCE, 115 );
	return fopen( filename, options );
#else
	FILE* opened;

	(void)bFromCache;
	opened = __wrap_fopen( filename, options );
	if ( opened != NULL || strchr( options, 'w' ) != NULL || strchr( options, '+' ) != NULL )
	{
		return opened;
	}

	return __wrap_fopen( findFileInDirCaseInsensitive( filename ), options );
#endif
}

int CFileSystem_Stdio::SetVBuf( FileHandle_t stream, char* buffer, int mode, long size )
{
	CBaseFileSystem::CFileHandle* opened;

	if ( stream != NULL )
	{
		opened = (CBaseFileSystem::CFileHandle*)stream;
		return setvbuf( opened->m_pFile, buffer, mode, size );
	}

	CBaseFileSystem::Warning( (FileWarningLevel_t)-1, "FS:  Tried to SetVBuf NULL file handle!\n" );
	return 0;
}

CFileSystem_Stdio::CFileSystem_Stdio()
	: CBaseFileSystem()
{
	m_bMounted = false;
}

void CFileSystem_Stdio::Mount()
{
	CBaseFileSystem::Mount();
#ifdef _WIN32
	if ( m_bMounted )
		_wassert( L"!m_bMounted", FILESYSTEM_STDIO_SOURCE, 93 );
#endif
	m_bMounted = true;
}

void CFileSystem_Stdio::Unmount()
{
#ifdef _WIN32
	if ( !m_bMounted )
		_wassert( L"m_bMounted", FILESYSTEM_STDIO_SOURCE, 102 );
#endif
	m_bMounted = false;
	CBaseFileSystem::Unmount();
}

extern "C" void* CreateInterface( const char* pName, int* pReturnCode )
{
	return InterfaceReg::Create( pName, pReturnCode );
}

void* GetModuleHandle( const char* name )
{
#ifdef _WIN32
	return ::GetModuleHandleA( name );
#else
	void* handle;

	if ( name == NULL )
	{
		return NULL;
	}

	handle = dlopen( name, RTLD_NOW );
	if ( handle == NULL )
	{
		return NULL;
	}
	dlclose( handle );

	return handle;
#endif
}

static void* GetFileSystemInstance()
{
	return &g_FileSystem_Stdio;
}

CFileSystem_Stdio	g_FileSystem_Stdio;
static InterfaceReg	g_FileSystemReg( GetFileSystemInstance, "VFileSystem009" );

bool CFileSystem_Stdio::GetWaitForResourcesProgress(
		WaitForResourcesHandle_t handle, float* progress, bool* complete )
{
	(void)handle;
	if ( progress != NULL )
	{
		*progress = 0.0f;
	}

	if ( complete != NULL )
	{
		*complete = true;
	}

	return false;
}

bool CFileSystem_Stdio::IsThreadSafe()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: files are always immediately available on disk
//-----------------------------------------------------------------------------
bool CFileSystem_Stdio::IsFileImmediatelyAvailable( char const* )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFileName - 
//-----------------------------------------------------------------------------
void CFileSystem_Stdio::GetLocalCopy( char const* )
{
}

int CFileSystem_Stdio::HintResourceNeed( char const*, int )
{
	return 0;
}

int CFileSystem_Stdio::PauseResourcePreloading()
{
	return 0;
}

int CFileSystem_Stdio::WaitForResources( char const* )
{
	return 0;
}
