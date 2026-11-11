//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

#include <stddef.h>

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
typedef void* FileHandle_t;
typedef int FileFindHandle_t;
typedef int WaitForResourcesHandle_t;

enum FileSystemSeek_t
{
	FILESYSTEM_SEEK_HEAD = 0,
	FILESYSTEM_SEEK_CURRENT,
	FILESYSTEM_SEEK_TAIL
};

enum FileWarningLevel_t
{
	// Don't print anything
	FILESYSTEM_WARNING_QUIET = 0,
	FILESYSTEM_WARNING_REPORTUNCLOSED,
	FILESYSTEM_WARNING_REPORTUSAGE,
	FILESYSTEM_WARNING_REPORTALLACCESSES
};

//-----------------------------------------------------------------------------
// Purpose: Main file system interface
//-----------------------------------------------------------------------------
class IFileSystem : public IBaseInterface
{
public:
	// Mount and unmount the filesystem
	virtual void Mount() = 0;
	virtual void Unmount() = 0;

	virtual void RemoveAllSearchPaths() = 0;
	// Add paths in priority order (mod dir, game dir, ....)
	// If one or more .pak files are in the specified directory, then they are
	//  added after the file system path
	// If the path is the relative path to a .bsp file, then any previous .bsp file 
	//  override is cleared and the current .bsp is searched for an embedded PAK file
	//  and this file becomes the highest priority search path ( i.e., it's looked at first
	//   even before the mod's file system path ).
	virtual void AddSearchPath( const char* path, const char* pathID ) = 0;
	virtual bool RemoveSearchPath( const char* path ) = 0;

	// Deletes a file
	virtual void RemoveFile( const char* filename, const char* pathID ) = 0;
	// this isn't implementable on STEAM as is.
	virtual void CreateDirHierarchy( const char* path, const char* pathID ) = 0;
	// File I/O and info
	virtual bool FileExists( const char* filename ) = 0;
	virtual bool IsDirectory( const char* path ) = 0;

	// opens a file
	// if pathID is NULL, all paths will be searched for the file
	virtual FileHandle_t Open( const char* pFileName, const char* pOptions, const char* pathID = NULL ) = 0;
	virtual void Close( FileHandle_t file ) = 0;
	virtual void Seek( FileHandle_t file, int position, FileSystemSeek_t type ) = 0;
	virtual unsigned int Tell( FileHandle_t file ) = 0;
	virtual unsigned int Size( FileHandle_t file ) = 0;
	virtual unsigned int Size( const char* filename ) = 0;
	virtual long GetFileTime( const char* filename ) = 0;
	virtual void FileTimeToString( char* buffer, int size, long time ) = 0;

	virtual bool IsOk( FileHandle_t file ) = 0;
	virtual void Flush( FileHandle_t file ) = 0;
	virtual bool EndOfFile( FileHandle_t file ) = 0;
	virtual int Read( void* pOutput, int size, FileHandle_t file ) = 0;
	virtual int Write( const void* pInput, int size, FileHandle_t file ) = 0;
	virtual char* ReadLine( char* output, int maxChars, FileHandle_t file ) = 0;
	virtual int FPrintf( FileHandle_t file, char* format, ... ) = 0;
	// direct filesystem buffer access
	// returns a handle to a buffer containing the file data
	// this is the optimal way to access the complete data for a file, 
	// since the file preloader has probably already got it in memory
	virtual void* GetReadBuffer( FileHandle_t file, int* size, bool failIfNotInCache ) = 0;
	virtual void ReleaseReadBuffer( FileHandle_t file, void* buffer ) = 0;

	// FindFirst/FindNext
	virtual const char* FindFirst( const char* wildcard, FileFindHandle_t* handle, const char* pathID = NULL ) = 0;
	virtual const char* FindNext( FileFindHandle_t handle ) = 0;
	virtual bool FindIsDirectory( FileFindHandle_t handle ) = 0;
	virtual void FindClose( FileFindHandle_t handle ) = 0;

	virtual void GetLocalCopy( const char* filename ) = 0;
	virtual const char* GetLocalPath( const char* filename, char* buffer, int size ) = 0;
	// Note: This is sort of a secondary feature; but it's really useful to have it here
	virtual char* ParseFile( char* data, char* token, bool* quoted ) = 0;
	// Returns true on success ( based on current list of search paths, otherwise false if 
	//  it can't be resolved )
	virtual bool FullPathToRelativePath( const char* fullPath, char* relativePath ) = 0;
	// Gets the current working directory
	virtual bool GetCurrentDirectory( char* directory, int size ) = 0;

	virtual void PrintOpenedFiles() = 0;
	virtual void SetWarningFunc( void ( *warning )( const char*, ... ) ) = 0;
	virtual void SetWarningLevel( FileWarningLevel_t level ) = 0;
	virtual void LogLevelLoadStarted( const char* name ) = 0;
	virtual void LogLevelLoadFinished( const char* name ) = 0;

	virtual int HintResourceNeed( const char* resources, int forgetEverything ) = 0;
	virtual int PauseResourcePreloading() = 0;
	virtual int ResumeResourcePreloading() = 0;
	virtual int SetVBuf( FileHandle_t file, char* buffer, int mode, long size ) = 0;
	virtual void GetInterfaceVersion( char* buffer, int size ) = 0;
	virtual bool IsFileImmediatelyAvailable( const char* filename ) = 0;
	// starts waiting for resources to be available
	// returns FILESYSTEM_INVALID_HANDLE if there is nothing to wait on
	virtual WaitForResourcesHandle_t WaitForResources( const char* resources ) = 0;
	// get progress on waiting for resources; progress is a float [0, 1], complete is true on the waiting being done
	// returns false if no progress is available
	// any calls after complete is true or on an invalid handle will return false, 0.0f, true
	virtual bool GetWaitForResourcesProgress( WaitForResourcesHandle_t handle, float* progress, bool* complete ) = 0;
	// cancels a progress call
	virtual void CancelWaitForResources( WaitForResourcesHandle_t handle ) = 0;
	// returns true if the appID has all its caches fully preloaded
	virtual bool IsAppReadyForOfflinePlay( int appID ) = 0;
	// interface for custom pack files > 4Gb
	virtual bool AddPackFile( const char* fullPath, const char* pathID ) = 0;
	// open a file but force the data to come from the steam cache, NOT from disk
	virtual FileHandle_t OpenFromCacheForRead( const char* filename, const char* options,
		const char* pathID = NULL ) = 0;
	virtual void AddSearchPathNoWrite( const char* path, const char* pathID ) = 0;
	//
	// ABOVE: Pre-25th anniversary interface
	//

	virtual long GetFileModificationTime( const char* filename ) = 0;
};

#endif // FILESYSTEM_H
