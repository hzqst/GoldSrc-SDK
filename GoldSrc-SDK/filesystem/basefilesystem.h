//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FILESYSTEM_STDIO_BASEFILESYSTEM_H
#define FILESYSTEM_STDIO_BASEFILESYSTEM_H

#include "public/interface.h"
#include "public/utlsymbol.h"
#include "../public/characterset.h"

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdarg.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <malloc.h>
#include <time.h>
#include <assert.h>

#ifdef NDEBUG
extern "C" _ACRTIMP void __cdecl _wassert( wchar_t const* message, wchar_t const* file, unsigned line );
#endif

#undef GetCurrentDirectory
#undef GetModuleHandle
#else
#include <dirent.h>
#include <unistd.h> // unlink
#include <dlfcn.h>
#include <utime.h>
#include <sys/time.h>
#endif

#ifdef _WIN32
#define FS_MAX_PATH_LENGTH				( (size_t)MAX_PATH )
#else
#define FS_MAX_PATH_LENGTH				( (size_t)4096 )
#endif
#define FS_SEARCH_PATH_CAPACITY			( FS_MAX_PATH_LENGTH + (size_t)2 )
#define FS_FILE_LOOKUP_CAPACITY			( FS_MAX_PATH_LENGTH + (size_t)128 )
#define FS_WARNING_TEXT_LENGTH			( (size_t)4096 )
#define FS_PATH_MATCH_BUFFER_SIZE		( (size_t)512 )

#define FS_MAX_PACK_FILES				32768
#define FS_INITIAL_PACK_CAPACITY		32
#define FS_PACK_NAME_LENGTH				( (size_t)56 )
#define FS_PACK64_NAME_LENGTH			( (size_t)112 )
#define FS_PACK_SIGNATURE_SIZE			( (size_t)4 )
#define FS_PACK64_DIRECTORY_OFFSET		FS_PACK_SIGNATURE_SIZE
#define FS_PACK64_DIRECTORY_LENGTH		( FS_PACK64_DIRECTORY_OFFSET + sizeof( long long ) )
#define FS_PACK64_HEADER_SIZE			( FS_PACK64_DIRECTORY_LENGTH + sizeof( long long ) )
#define FS_APPENDED_PACK_FOOTER_SIZE	( (size_t)24 )
#define FS_APPENDED_PACK_MARKER			"PACKAPPE"

#ifndef _WIN32
#define FS_LINUX_STAT_VERSION		3
#define FS_LINUX_DEVICE_VERSION		1
#define FS_DIRECTORY_PERMISSIONS	( (mode_t)( S_IRWXU | S_IRGRP | S_IROTH ) )
#define FS_CTYPE_TABLE_RANGE		383U
#endif

struct dirent;
struct CSysModule;
class CBaseFileSystem;

typedef void* FileHandle_t;
#ifndef _WIN32
typedef void* HANDLE;
#define INVALID_HANDLE_VALUE	( (HANDLE)-1 )
#endif
typedef int FileFindHandle_t;
typedef int WaitForResourcesHandle_t;

enum FileSystemSeek_t
{
	FILESYSTEM_SEEK_HEAD	= 0,
	FILESYSTEM_SEEK_CURRENT	= 1,
	FILESYSTEM_SEEK_TAIL	= 2
};

enum FileWarningLevel_t
{
	FILESYSTEM_WARNING_QUIET				= 0,
	FILESYSTEM_WARNING_REPORTUNCLOSED		= 1,
	FILESYSTEM_WARNING_REPORTUSAGE			= 2,
	FILESYSTEM_WARNING_REPORTALLACCESSES	= 3
};

#define FILESYSTEM_WARNING	( (FileWarningLevel_t)-1 )

enum PathMod_t
{
	kPathUnchanged	= 0,
	kPathLowered	= 1,
	kPathChanged	= 2,
	kPathFailed		= 3
};

#ifdef _WIN32
typedef WIN32_FIND_DATAA FIND_DATA;
#else
struct FIND_DATA
{
	unsigned int	dwFileAttributes;
	char			cFileName[FS_MAX_PATH_LENGTH];
	int				numMatches;

	dirent**		namelist;
	char			cDir[FS_MAX_PATH_LENGTH];
};
#endif

class IFileSystem : public IBaseInterface
{
public:
	virtual void Mount() = 0;
	virtual void Unmount() = 0;

	virtual void RemoveAllSearchPaths() = 0;
	virtual void AddSearchPath( const char* pPath, const char* pathID ) = 0;
	virtual bool RemoveSearchPath( const char* pPath ) = 0;

	virtual void RemoveFile( const char* pRelativePath, const char* pathID ) = 0;
	virtual void CreateDirHierarchy( const char* path, const char* pathID ) = 0;
	virtual bool FileExists( const char* pFileName ) = 0;
	virtual bool IsDirectory( const char* pFileName ) = 0;

	virtual FileHandle_t Open( const char* pFileName, const char* pOptions, const char* pathID ) = 0;
	virtual void Close( FileHandle_t file ) = 0;
	virtual void Seek( FileHandle_t file, int pos, FileSystemSeek_t whence ) = 0;
	virtual unsigned int Tell( FileHandle_t file ) = 0;
	virtual unsigned int Size( FileHandle_t file ) = 0;
	virtual unsigned int Size( const char* pFileName ) = 0;
	virtual int GetFileTime( const char* pFileName ) = 0;
	virtual void FileTimeToString( char* pStrip, int maxCharsIncludingTerminator, long fileTime ) = 0;
	virtual bool IsOk( FileHandle_t file ) = 0;
	virtual void Flush( FileHandle_t file ) = 0;
	virtual bool EndOfFile( FileHandle_t file ) = 0;
	virtual int Read( void* pOutput, int size, FileHandle_t file ) = 0;
	virtual int Write( const void* pInput, int size, FileHandle_t file ) = 0;
	virtual char* ReadLine( char* pOutput, int maxChars, FileHandle_t file ) = 0;
	virtual int FPrintf( FileHandle_t file, char* pFormat, ... ) = 0;
	virtual void* GetReadBuffer( FileHandle_t file, int* outBufferSize, bool failIfNotInCache ) = 0;
	virtual void ReleaseReadBuffer( FileHandle_t file, void* readBuffer ) = 0;

	virtual const char* FindFirst( const char* pWildCard, FileFindHandle_t* pHandle, const char* pPathID ) = 0;
	virtual const char* FindNext( FileFindHandle_t handle ) = 0;
	virtual bool FindIsDirectory( FileFindHandle_t handle ) = 0;
	virtual void FindClose( FileFindHandle_t handle ) = 0;

	virtual void GetLocalCopy( const char* pFileName ) = 0;
	virtual const char* GetLocalPath( const char* pFileName, char* pLocalPath, int localPathBufferSize ) = 0;
	virtual char* ParseFile( char* pFileBytes, char* pToken, bool* pWasQuoted ) = 0;
	virtual bool FullPathToRelativePath( const char* pFullpath, char* pRelative ) = 0;
	virtual bool GetCurrentDirectory( char* pDirectory, int maxlen ) = 0;

	virtual void PrintOpenedFiles() = 0;
	virtual void SetWarningFunc( void ( *pfnWarning )( const char*, ... ) ) = 0;
	virtual void SetWarningLevel( FileWarningLevel_t level ) = 0;

	virtual void LogLevelLoadStarted( const char* name ) = 0;
	virtual void LogLevelLoadFinished( const char* name ) = 0;
	virtual int HintResourceNeed( const char* hintlist, int forgetEverything ) = 0;
	virtual int PauseResourcePreloading() = 0;
	virtual int ResumeResourcePreloading() = 0;
	virtual int SetVBuf( FileHandle_t stream, char* buffer, int mode, long size ) = 0;
	virtual void GetInterfaceVersion( char* p, int maxlen ) = 0;
	virtual bool IsFileImmediatelyAvailable( const char* pFileName ) = 0;
	virtual WaitForResourcesHandle_t WaitForResources( const char* resourcelist ) = 0;
	virtual bool GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float* progress, bool* complete ) = 0;
	virtual void CancelWaitForResources( WaitForResourcesHandle_t handle ) = 0;
	virtual bool IsAppReadyForOfflinePlay( int appID ) = 0;

	virtual bool AddPackFile( const char* pFileName, const char* pathID ) = 0;
	virtual FileHandle_t OpenFromCacheForRead( const char* pFileName, const char* pOptions, const char* pathID ) = 0;
	virtual void AddSearchPathNoWrite( const char* pPath, const char* pathID ) = 0;
	virtual int GetFileModificationTime( const char* pFileName ) = 0;

	virtual FILE* FS_fopen( const char* filename, const char* options, bool bFromCache ) = 0;
	virtual void FS_fclose( FILE* fp ) = 0;
	virtual void FS_fseek( FILE* fp, long long pos, int seekType ) = 0;
	virtual int FS_ftell( FILE* fp ) = 0;
	virtual int FS_feof( FILE* fp ) = 0;
	virtual size_t FS_fread( void* dest, unsigned int count, unsigned int size, FILE* fp ) = 0;
	virtual size_t FS_fwrite( const void* src, unsigned int count, unsigned int size, FILE* fp ) = 0;
	virtual size_t FS_vfprintf( FILE* fp, const char* fmt, va_list list ) = 0;
	virtual int FS_ferror( FILE* fp ) = 0;
	virtual int FS_fflush( FILE* fp ) = 0;
	virtual char* FS_fgets( char* dest, int destSize, FILE* fp ) = 0;
	virtual int FS_stat( const char* path, struct stat* buf ) = 0;
	virtual void* FS_FindFirstFile( char* findname, FIND_DATA* dat ) = 0;
	virtual bool FS_FindNextFile( void* handle, FIND_DATA* dat ) = 0;
	virtual bool FS_FindClose( void* handle ) = 0;
	virtual bool IsThreadSafe() = 0;
};

class CBaseFileSystem : public IFileSystem
{
public:
	struct packfile_t
	{
		char	name[FS_PACK_NAME_LENGTH];
		int		filepos;
		int		filelen;
	};

	struct packheader_t
	{
		char	id[4];
		int		dirofs;
		int		dirlen;
	};

	struct packfile64_t
	{
		char		name[FS_PACK64_NAME_LENGTH];
		long long	filepos;
		long long	filelen;
	};

#pragma pack( push, 4 )
	struct packheader64_t
	{
		char		id[4];
		long long	dirofs;
		long long	dirlen;
	};
#pragma pack( pop )

	struct packappenededheader_t
	{
		char		id[8];
		long long	packheaderpos;
		long long	originalfilesize;
	};

	class CPackFileEntry
	{
	public:
		CUtlSymbol	m_Name;
		long long	m_nPosition;
		long long	m_nLength;
	};

	class CFileHandle
	{
	public:
		CFileHandle()
		{
			m_pFile = NULL;
			m_bPack = false;
			m_bErrorFlagged = false;
			m_nStartOffset = 0;
			m_nLength = 0;
			m_nFileTime = 0;
			m_nModificationTime = 0;
		}

		FILE*		m_pFile;
		bool		m_bPack;
		bool		m_bErrorFlagged;

		long long	m_nStartOffset;
		long long	m_nLength;

		int			m_nFileTime;
		int			m_nModificationTime;
	};

	class COpenedFile
	{
	public:
		COpenedFile();
		COpenedFile( const COpenedFile& src );
		~COpenedFile();

		bool operator==( const COpenedFile& src ) const;
		void SetName( const char* name );
		const char* GetName();

		FILE*	m_pFile;
		char*	m_pName;
	};

	class CSearchPath
	{
	public:
		CSearchPath();
		~CSearchPath();

		static bool PackFileLessFunc( const CPackFileEntry& left, const CPackFileEntry& right );

		CUtlSymbol						m_Path;
		CUtlSymbol						m_PathID;
		bool							m_bIsMapPath;
		bool							m_bIsPackFile;

		long							m_lPackFileTime;
		CFileHandle*					m_hPackFile;
		int								m_nNumPackFiles;
		unsigned int					m_iCurSearchFile;

		bool							m_bAllowWrite;
		CUtlRBTree<CPackFileEntry, int>	m_PackFiles;
	};

	struct FindData_t
	{
		FIND_DATA			findData;
		int					currentSearchPathID;
		int					limitedPathID;

		CUtlVector<char>	wildCardString;
		HANDLE				findHandle;
	};

	CBaseFileSystem();
	~CBaseFileSystem() override;

	void Mount() override;
	void Unmount() override;

	void RemoveAllSearchPaths() override;
	void AddSearchPath( const char* pPath, const char* pathID ) override;
	bool RemoveSearchPath( const char* pPath ) override;

	void RemoveFile( const char* pRelativePath, const char* pathID ) override;
	void CreateDirHierarchy( const char* path, const char* pathID ) override;
	bool FileExists( const char* pFileName ) override;
	bool IsDirectory( const char* pFileName ) override;

	FileHandle_t Open( const char* pFileName, const char* pOptions, const char* pathID ) override;
	void Close( FileHandle_t file ) override;
	void Seek( FileHandle_t file, int pos, FileSystemSeek_t whence ) override;
	unsigned int Tell( FileHandle_t file ) override;
	unsigned int Size( FileHandle_t file ) override;
	unsigned int Size( const char* pFileName ) override;
	int GetFileTime( const char* pFileName ) override;
	void FileTimeToString( char* pStrip, int maxCharsIncludingTerminator, long fileTime ) override;
	bool IsOk( FileHandle_t file ) override;
	void Flush( FileHandle_t file ) override;
	bool EndOfFile( FileHandle_t file ) override;
	int Read( void* pOutput, int size, FileHandle_t file ) override;
	int Write( const void* pInput, int size, FileHandle_t file ) override;
	char* ReadLine( char* pOutput, int maxChars, FileHandle_t file ) override;
	int FPrintf( FileHandle_t file, char* pFormat, ... ) override;
	void* GetReadBuffer( FileHandle_t file, int* outBufferSize, bool failIfNotInCache ) override;
	void ReleaseReadBuffer( FileHandle_t file, void* readBuffer ) override;

	const char* FindFirst( const char* pWildCard, FileFindHandle_t* pHandle, const char* pPathID ) override;
	const char* FindNext( FileFindHandle_t handle ) override;
	bool FindIsDirectory( FileFindHandle_t handle ) override;
	void FindClose( FileFindHandle_t handle ) override;

	const char* GetLocalPath( const char* pFileName, char* pLocalPath, int localPathBufferSize ) override;
	char* ParseFile( char* pFileBytes, char* pToken, bool* pWasQuoted ) override;
	bool FullPathToRelativePath( const char* pFullpath, char* pRelative ) override;
	bool GetCurrentDirectory( char* pDirectory, int maxlen ) override;

	void PrintOpenedFiles() override;
	void SetWarningFunc( void ( *pfnWarning )( const char*, ... ) ) override;
	void SetWarningLevel( FileWarningLevel_t level ) override;

	bool AddPackFile( const char* pFileName, const char* pathID ) override;
	FileHandle_t OpenFromCacheForRead( const char* pFileName, const char* pOptions, const char* pathID ) override;
	void AddSearchPathNoWrite( const char* pPath, const char* pathID ) override;
	int GetFileModificationTime( const char* pFileName ) override;

	void Warning( FileWarningLevel_t level, const char* fmt, ... );
	static bool OpenedFileLessFunc( const COpenedFile& src1, const COpenedFile& src2 );

	void StripFilename( char* path );
	void FixSlashes( char* str );
	void FixPath( char* str );
	CSearchPath* GetWritePath( const char* pathID );
	const char* SearchPakFile( const char* pWildCard, int currentSearchPathID, bool first );
	void AddSearchPathInternal( const char* pPath, const char* pathID, bool bAllowWrite );
	const char* FindFirstHelper( const char* pWildCard, FileFindHandle_t* pHandle,
			int searchPath, FindData_t* pFindData );
	bool FileInSearchPaths( const char* pSearchWildcard, const char* pFileName,
			int minSearchPathID, int maxSearchPathID );
	bool FindNextFileHelper( FindData_t* pFindData );
	bool IsDuplicateFindDirectory( int searchPath, int limitedPathID ) const;

	void AddPackFiles( const char* pPath );
	bool AddPackFileFromPath( const char* pPath, const char* pakfile,
			bool bCheckForAppendedPack, const char* pathID );
	bool PreparePackFile( CSearchPath& packfile, long long offsetofpackinmetafile );
	bool Prepare64BitPackFile( CSearchPath& packfile, long long offsetofpackinmetafile );
	void RemoveAllMapSearchPaths();
	int FastFindFileSize( const CSearchPath* path, const char* pFileName );

	void Trace_FClose( FILE* fp );
	FILE* Trace_FOpen( const char* filename, const char* options, bool bFromCache );
	void Trace_DumpUnclosedFiles();
	void* FindFile( const CSearchPath* path, const char* pFileName, const char* pOptions, bool bFromCache );

	static CBaseFileSystem*		s_pFileSystem;

	CUtlVector<COpenedFile>		m_OpenedFiles;
	// Global list of pack file handles
	CUtlVector<FILE*>			m_PackFileHandles;
	CUtlVector<FindData_t>		m_FindData;
	CUtlVector<CSearchPath>		m_SearchPaths;

	FileWarningLevel_t			m_fwLevel;
	void ( *					m_pfnWarning )( const char*, ... );
	int							m_nOpenCount;
};

class CFileSystem_Stdio : public CBaseFileSystem
{
public:
	CFileSystem_Stdio();
	~CFileSystem_Stdio() override;

	void Mount() override;
	void Unmount() override;
	void GetLocalCopy( const char* pFileName ) override;

	void LogLevelLoadStarted( const char* ) override
	{
	}

	void LogLevelLoadFinished( const char* ) override
	{
	}

	int HintResourceNeed( const char* hintlist, int forgetEverything ) override;
	int PauseResourcePreloading() override;

	int ResumeResourcePreloading() override
	{
		return 0;
	}

	int SetVBuf( FileHandle_t stream, char* buffer, int mode, long size ) override;
	void GetInterfaceVersion( char* p, int maxlen ) override;
	bool IsFileImmediatelyAvailable( const char* pFileName ) override;
	WaitForResourcesHandle_t WaitForResources( const char* resourcelist ) override;
	bool GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float* progress, bool* complete ) override;
	void CancelWaitForResources( WaitForResourcesHandle_t handle ) override;
	bool IsAppReadyForOfflinePlay( int appID ) override;

	FILE* FS_fopen( const char* filename, const char* options, bool bFromCache ) override;
	void FS_fclose( FILE* fp ) override;
	void FS_fseek( FILE* fp, long long pos, int seekType ) override;
	int FS_ftell( FILE* fp ) override;
	int FS_feof( FILE* fp ) override;
	size_t FS_fread( void* dest, unsigned int count, unsigned int size, FILE* fp ) override;
	size_t FS_fwrite( const void* src, unsigned int count, unsigned int size, FILE* fp ) override;
	size_t FS_vfprintf( FILE* fp, const char* fmt, va_list list ) override;
	int FS_ferror( FILE* fp ) override;
	int FS_fflush( FILE* fp ) override;
	char* FS_fgets( char* dest, int destSize, FILE* fp ) override;
	int FS_stat( const char* path, struct stat* buf ) override;
	void* FS_FindFirstFile( char* findname, FIND_DATA* dat ) override;
	bool FS_FindNextFile( void* handle, FIND_DATA* dat ) override;
	bool FS_FindClose( void* handle ) override;
	bool IsThreadSafe() override;

	bool	m_bMounted;
};

#ifndef _WIN32
class CDirPtr
{
public:
	CDirPtr() : m_pDir( NULL )
	{
	}

	DIR*	m_pDir;
};
#endif

class CPerformanceCounter
{
public:
	CPerformanceCounter();
	void InitializePerformanceCounter();
	double GetCurTime();

	int		m_iLowShift = 0;
	double	m_flPerfCounterFreq = 0.0;
	double	m_flCurrentTime = 0.0;
	double	m_flLastCurrentTime = 0.0;
};

extern CFileSystem_Stdio	g_FileSystem_Stdio;

extern float			g_flDummyFloat;
extern characterset_s	g_BreakSet;
extern characterset_s	g_BreakSetIncludingColons;

#ifndef _WIN32
extern char				fileName[FS_MAX_PATH_LENGTH];

extern const char*		s_pszDbgPathMatch;
extern bool				s_bShowDiag;
#endif

// singleton accessor
CBaseFileSystem*	BaseFileSystem();

void*				GetModuleHandle( const char* name );

void*				Sys_GetProcAddress( void* pModule, const char* pName );
CSysModule*			Sys_LoadModule( const char* pModuleName );
void				Sys_UnloadModule( CSysModule* pModule );
CreateInterfaceFn	Sys_GetFactory( CSysModule* pModule );
CreateInterfaceFn	Sys_GetFactoryThis();
CreateInterfaceFn	Sys_GetFactory( const char* pModuleName );

#ifndef _WIN32
int					strlwr( char* src );
int					FindFirstFile( char* fileName, FIND_DATA* dat );
bool				FindNextFile( int handle, FIND_DATA* dat );
const char*			findFileInDirCaseInsensitive( const char* file );
bool				FindClose( int handle );

extern "C" int		__wrap_unlink( const char* pathname );
extern "C" int		__wrap_mkdir( const char* pathname, mode_t mode );
extern "C" int		__wrap___xstat( int ver, const char* path, struct stat* buf );
extern "C" FILE*	__wrap_fopen( const char* path, const char* mode );

PathMod_t			pathmatch( const char* pszIn, char** ppszOut, bool bAllowBasenameMismatch,
						char* pszOutBuf, size_t OutBufLen );
int					ResolveCaseInsensitivePath( const char* pszIn, char** ppszOut,
						bool bAllowBasenameMismatch, char* pszOutBuf, unsigned int OutBufLen );
#endif	//_WIN32

#endif
