//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: This header should never be used directly from leaf code!!!
// Instead, just add the file memoverride.cpp into your project and all this
// will automagically be used
//
// $NoKeywords: $
//=============================================================================

#ifndef TIER0_MEMALLOC_H
#define TIER0_MEMALLOC_H

#include "tier0/platform.h"

#include <stddef.h>

struct _CrtMemState;

//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
class IMemAlloc
{
public:
	// Release versions
	virtual void* Alloc( size_t nSize ) = 0;
	virtual void* Realloc( void* pMem, size_t nSize ) = 0;
	virtual void Free( void* pMem, int reserved = 0 ) = 0;
	virtual void* Expand( void* pMem, size_t nSize ) = 0;

	virtual void* Alloc( size_t nSize, const char* pFileName, int nLine, int reserved = 0 ) = 0;
	virtual void* Realloc( void* pMem, size_t nSize, const char* pFileName, int nLine ) = 0;
	virtual void Free( void* pMem, const char* pFileName, int nLine, int reserved = 0 ) = 0;
	virtual void* Expand( void* pMem, size_t nSize, const char* pFileName, int nLine ) = 0;

	// Returns size of a particular allocation
	virtual size_t GetSize( void* pMem ) = 0;
	// Force file + line information for an allocation
	virtual void PushAllocDbgInfo( const char* pFileName, int nLine ) = 0;
	virtual void PopAllocDbgInfo() = 0;
	// FIXME: Remove when we have our own allocator
	// these methods of the Crt debug code is used in our codebase currently
	virtual long CrtSetBreakAlloc( long lNewBreakAlloc ) = 0;
	virtual int CrtSetReportMode( int nReportType, int nReportMode ) = 0;
	virtual int CrtIsValidHeapPointer( const void* pMem ) = 0;
	virtual int CrtIsValidPointer( const void* pMem, unsigned int size, int access ) = 0;
	virtual int CrtCheckMemory() = 0;
	virtual int CrtSetDbgFlag( int nNewFlag ) = 0;
	virtual void CrtMemCheckpoint( _CrtMemState* pState ) = 0;
	// FIXME: Make a better stats interface
	virtual void DumpStats() = 0;
	// FIXME: Remove when we have our own allocator
	virtual void* CrtSetReportFile( int nReportType, void* hFile ) = 0;
	virtual void* CrtSetReportHook( void* pfnNewHook ) = 0;
	virtual int CrtDbgReport( int nReportType, const char* pFile, int nLine,
		const char* pModule, const char* pMessage ) = 0;
	virtual int heapchk() = 0;
	virtual bool IsDebugHeap() = 0;
	virtual void GetActualDbgInfo( const char*& pFileName, int& nLine ) = 0;
	virtual void RegisterAllocation( const char* pFileName, int nLine, int nLogicalSize,
		int nActualSize, unsigned int nTime ) = 0;
	virtual void RegisterDeallocation( const char* pFileName, int nLine, int nLogicalSize,
		int nActualSize, unsigned int nTime ) = 0;
	virtual int CountAllocations() = 0;
	virtual size_t GetMemoryUsed() = 0;
};

extern "C" TIER0_API IMemAlloc* g_pMemAlloc;

void ApplyMemoryInitializations( void* pMemory, size_t nSize );
size_t CalcHeapUsed();

#endif /* TIER0_MEMALLOC_H */
