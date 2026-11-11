//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Memory allocation!
//
// $NoKeywords: $
//=============================================================================

#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier0/validator.h"
#include "tier0/memalloc.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define DBG_SOURCE_FILE	DBG_BUILDSLAVE_PATH( "tier0\\mem.cpp" )

#define MIN_SCRATCH_BYTES	( 1024 * 1024 )
#define MAX_STACK_DEPTH		32

static CThreadFastMutex s_MutexAllocScratch;
static int s_nBufAllocSize;
static int s_nBufCurSize;
static int s_pBufStackDepth[MAX_STACK_DEPTH];
static unsigned char* s_pBuf;
#if defined( _WIN32 )
static int s_nBufDepth = -1;
#else
static int s_nBufDepth;
#endif

extern "C"
{

#if defined( _WIN32 )
TIER0_API void* MemAllocScratch( int nMemSize )
{
	s_MutexAllocScratch.Lock();

	// Minimally allocate 1M scratch
	if ( s_nBufAllocSize < s_nBufCurSize + nMemSize )
	{
		s_nBufAllocSize = s_nBufCurSize + nMemSize;
		if ( s_nBufAllocSize < MIN_SCRATCH_BYTES )
		{
			s_nBufAllocSize = MIN_SCRATCH_BYTES;
		}

		if ( s_pBuf )
		{
			s_pBuf = (unsigned char*)g_pMemAlloc->Realloc( s_pBuf, s_nBufAllocSize, DBG_SOURCE_FILE, 46 );
			AssertAt( s_pBuf, 47 );
		}
		else
		{
			s_pBuf = (unsigned char*)g_pMemAlloc->Alloc( s_nBufAllocSize, DBG_SOURCE_FILE, 51 );
		}
	}

	int nBase = s_nBufCurSize;
	s_nBufCurSize += nMemSize;
	++s_nBufDepth;

	AssertAt( s_nBufDepth < MAX_STACK_DEPTH, 58 );

	s_pBufStackDepth[s_nBufDepth] = nMemSize;

	unsigned char* pResult = s_pBuf + nBase;
	s_MutexAllocScratch.Unlock();
	return pResult;
}





TIER0_API void MemFreeScratch()
{
	s_MutexAllocScratch.Lock();

	AssertAt( s_nBufDepth >= 0, 70 );

	s_nBufCurSize -= s_pBufStackDepth[s_nBufDepth];
	--s_nBufDepth;

	s_MutexAllocScratch.Unlock();
}
#else
TIER0_API void* MemAllocScratch( int nMemSize )
{
	s_MutexAllocScratch.Lock();
	unsigned int newDepth = (unsigned int)s_nBufDepth + 1;
	if ( nMemSize < 0 || s_nBufCurSize < 0 || nMemSize > INT_MAX - s_nBufCurSize
		|| newDepth >= MAX_STACK_DEPTH )
	{
		Assert( !"MemAllocScratch: invalid size or stack overflow" );
		s_MutexAllocScratch.Unlock();
		return NULL;
	}

	int newEnd = s_nBufCurSize + nMemSize;
	if ( newEnd > s_nBufAllocSize )
	{
		int newAllocSize = newEnd < MIN_SCRATCH_BYTES ? MIN_SCRATCH_BYTES : newEnd;
		unsigned char* pNewBuffer;

		if ( s_pBuf )
		{
			pNewBuffer = (unsigned char*)realloc( s_pBuf, newAllocSize );
		}
		else
		{
			pNewBuffer = (unsigned char*)malloc( newAllocSize );
		}
		if ( !pNewBuffer )
		{
			Assert( !"MemAllocScratch: allocation failed" );
			s_MutexAllocScratch.Unlock();
			return NULL;
		}
		s_pBuf = pNewBuffer;
		s_nBufAllocSize = newAllocSize;
	}

	unsigned char* pResult = s_pBuf ? s_pBuf + s_nBufCurSize : NULL;
	s_nBufCurSize = newEnd;

	s_nBufDepth = (int)newDepth;
	s_pBufStackDepth[newDepth] = nMemSize;

	s_MutexAllocScratch.Unlock();
	return pResult;
}






TIER0_API void MemFreeScratch()
{
	s_MutexAllocScratch.Lock();
	const int minDepth = 1;
	if ( s_nBufDepth < minDepth || s_nBufDepth >= MAX_STACK_DEPTH )
	{
		Assert( !"MemFreeScratch: stack underflow" );
		s_MutexAllocScratch.Unlock();
		return;
	}
	s_nBufCurSize -= s_pBufStackDepth[s_nBufDepth--];
	s_MutexAllocScratch.Unlock();
}

void ZeroMemory( void* mem, size_t length )
{
	if ( length != 0 )
	{
		memset( mem, 0, length );
	}
}

#endif

}

#if defined( _WIN32 )
void ValidateMemory( CValidator& validator )
{
	validator.Push( "MemScratch", NULL, "Global" );
	validator.ClaimMemory( s_pBuf );
	validator.Pop();
}
#endif
