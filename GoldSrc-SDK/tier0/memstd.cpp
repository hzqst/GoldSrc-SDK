//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Memory allocation!
//
// $NoKeywords: $
//=============================================================================

#include "tier0/memalloc.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

#if defined( _WIN32 )
#include <malloc.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DBG_SOURCE_FILE	DBG_BUILDSLAVE_PATH( "tier0\\memstd.cpp" )

#define MAX_SMALL_BLOCK_SIZE	2048
#define SMALL_BLOCK_ALIGNMENT	4
#define NUM_POOLS				50
#define POOL_RESERVE_SIZE		( 4 * 1024 * 1024 )
#define POOL_COMMIT_SIZE			8192

class CSmallBlockPool
{
public:
#pragma warning( suppress: 26495 )
	CSmallBlockPool() = default;
	void Init( size_t nBlockSize, unsigned char* pBase );
	void* Alloc();
	void Free( void* p );
	bool IsOwner( void* p ) const;

	struct FreeBlock_t
	{
		FreeBlock_t* pNext;
	};

	FreeBlock_t*	m_pFreeBlocks;
	size_t			m_nBlockSize;
	unsigned char*	m_pNextAlloc;
	unsigned char*	m_pCommitLimit;
	unsigned char*	m_pAllocLimit;
	unsigned char*	m_pBase;
	int				m_nAllocated;
	bool			m_bCommitFailed;
	CThreadFastMutex m_Mutex;
};

class CSmallBlockHeap
{
public:
	CSmallBlockHeap();
	~CSmallBlockHeap();
	bool IsOwner( void* pMemory ) const;
	bool ShouldUse( size_t nSize ) const;
	CSmallBlockPool* FindPool( size_t nBytes );
	CSmallBlockPool* FindPool( void* pMemory );
	void DumpStats();

	CSmallBlockPool*	m_PoolLookup[MAX_SMALL_BLOCK_SIZE / SMALL_BLOCK_ALIGNMENT];
	CSmallBlockPool	m_Pools[NUM_POOLS];
	unsigned char*	m_pBase;
	unsigned char*	m_pLimit;
	bool			m_bEnabled;
};

class CStdMemAlloc : public IMemAlloc
{
public:
	CStdMemAlloc();
	~CStdMemAlloc();

	void* Alloc( size_t nSize ) override;
	void* Realloc( void* pMemory, size_t nSize ) override;
	void Free( void* pMemory, int ) override;
	void* Expand( void*, size_t ) override { return NULL; }

	void* Alloc( size_t nSize, const char*, int, int ) override { return Alloc( nSize ); }
	void* Realloc( void* pMemory, size_t nSize, const char*, int ) override { return Realloc( pMemory, nSize ); }
	void Free( void* pMemory, const char*, int, int ) override { Free( pMemory, 0 ); }
	void* Expand( void*, size_t, const char*, int ) override { return NULL; }

	size_t GetSize( void* pMemory ) override;
	void PushAllocDbgInfo( const char*, int ) override {}
	void PopAllocDbgInfo() override {}

	long CrtSetBreakAlloc( long ) override { return 0; }
	int CrtSetReportMode( int, int ) override { return 0; }
	int CrtIsValidHeapPointer( const void* ) override { return 1; }
	int CrtIsValidPointer( const void*, unsigned int, int ) override { return 1; }
	int CrtCheckMemory() override { return 1; }
	int CrtSetDbgFlag( int ) override { return 0; }
	void CrtMemCheckpoint( _CrtMemState* ) override {}

	void DumpStats() override { m_SmallBlockHeap.DumpStats(); }

	void* CrtSetReportFile( int, void* ) override { return NULL; }
	void* CrtSetReportHook( void* ) override { return NULL; }
	int CrtDbgReport( int, const char*, int, const char*, const char* ) override { return 0; }
	int heapchk() override { return _HEAPOK; }
	bool IsDebugHeap() override { return false; }

	void GetActualDbgInfo( const char*&, int& ) override {}
	void RegisterAllocation( const char*, int, int, int, unsigned int ) override {}
	void RegisterDeallocation( const char*, int, int, int, unsigned int ) override {}

	int CountAllocations() override { return 0; }
	size_t GetMemoryUsed() override;

private:
	CSmallBlockHeap m_SmallBlockHeap;
	HMODULE m_hPSAPI;
	BOOL ( WINAPI *m_pfnGetProcessMemoryInfo )( HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD );
};

//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------
static CStdMemAlloc s_StdMemAlloc;
extern "C" TIER0_API IMemAlloc* g_pMemAlloc = &s_StdMemAlloc;

void CSmallBlockPool::Init( size_t nBlockSize, unsigned char* pBase )
{
	AssertAt( nBlockSize % 4 == 0 && nBlockSize >= sizeof(FreeBlock_t), 108 );
	m_pFreeBlocks = NULL;
	m_nBlockSize = nBlockSize;
	m_pBase = pBase;
	m_pNextAlloc = pBase;
	m_pCommitLimit = pBase;
	m_pAllocLimit = pBase + POOL_RESERVE_SIZE;
	m_nAllocated = 0;
	m_bCommitFailed = false;
}

void* CSmallBlockPool::Alloc()
{
	CAutoLockT<CThreadFastMutex> lock( m_Mutex );
	if ( m_pFreeBlocks )
	{
		FreeBlock_t* pBlock = m_pFreeBlocks;
		m_pFreeBlocks = pBlock->pNext;
		++m_nAllocated;
		return pBlock;
	}

	if ( m_pNextAlloc + m_nBlockSize >= m_pAllocLimit )
		return NULL;

	if ( m_pNextAlloc + m_nBlockSize > m_pCommitLimit )
	{
		if ( !VirtualAlloc( m_pCommitLimit, POOL_COMMIT_SIZE, MEM_COMMIT, PAGE_READWRITE ) )
		{
			m_bCommitFailed = true;
			return NULL;
		}
		m_pCommitLimit += POOL_COMMIT_SIZE;
	}

	void* pMemory = m_pNextAlloc;
	m_pNextAlloc += m_nBlockSize;
	++m_nAllocated;
	return pMemory;
}

bool CSmallBlockPool::IsOwner( void* p ) const
{
	return p >= m_pBase && p < m_pAllocLimit;
}

void CSmallBlockPool::Free( void* p )
{
	CAutoLockT<CThreadFastMutex> lock( m_Mutex );
	AssertAt( IsOwner( p ), 164 );
	FreeBlock_t* pBlock = (FreeBlock_t*)p;
	pBlock->pNext = m_pFreeBlocks;
	m_pFreeBlocks = pBlock;
	--m_nAllocated;
}

#pragma warning( push )
#pragma warning( disable: 26495 )
CSmallBlockHeap::CSmallBlockHeap()
{
	m_bEnabled = true;
	m_pBase = (unsigned char*)VirtualAlloc( NULL, NUM_POOLS * POOL_RESERVE_SIZE, MEM_RESERVE, PAGE_NOACCESS );
	if ( !m_pBase )
	{
		m_bEnabled = false;
		return;
	}

	m_pLimit = m_pBase + NUM_POOLS * POOL_RESERVE_SIZE;
	int iCurPool = 0;
	size_t nBlockSize = 0;
	int nGroupSize = 1;
	for ( int i = 0; i < MAX_SMALL_BLOCK_SIZE / SMALL_BLOCK_ALIGNMENT; ++i )
	{
		if ( i == 16 || i == 32 || i == 64 || i == 128 || i == 192 || i == 256 )
			nGroupSize *= 2;

		if ( i % nGroupSize == 0 )
		{
			nBlockSize += nGroupSize * SMALL_BLOCK_ALIGNMENT;
			m_Pools[iCurPool].Init( nBlockSize, m_pBase + iCurPool * POOL_RESERVE_SIZE );
			++iCurPool;
		}
		m_PoolLookup[i] = &m_Pools[iCurPool - 1];
	}
	AssertAt( iCurPool == NUM_POOLS, 351 );
}
#pragma warning( pop )

CSmallBlockHeap::~CSmallBlockHeap()
{
	if ( m_pBase )
		VirtualFree( m_pBase, 0, MEM_RELEASE );
}

bool CSmallBlockHeap::IsOwner( void* pMemory ) const
{
	return pMemory >= m_pBase && pMemory < m_pLimit;
}

bool CSmallBlockHeap::ShouldUse( size_t nSize ) const
{
	return m_bEnabled && nSize <= MAX_SMALL_BLOCK_SIZE;
}

CSmallBlockPool* CSmallBlockHeap::FindPool( size_t nBytes )
{
	AssertAt( ShouldUse( nBytes ), 384 );
	return m_PoolLookup[( nBytes - 1 ) / SMALL_BLOCK_ALIGNMENT];
}

CSmallBlockPool* CSmallBlockHeap::FindPool( void* pMemory )
{
	return &m_Pools[( (unsigned char*)pMemory - m_pBase ) / POOL_RESERVE_SIZE];
}

void CSmallBlockHeap::DumpStats()
{
	Msg( "SBH usage:\n" );
	int nTotalUsed = 0;
	for ( int i = 0; i < NUM_POOLS; ++i )
	{
		CSmallBlockPool& pool = m_Pools[i];
		int nUsed = (int)( pool.m_pCommitLimit - pool.m_pBase );
		nTotalUsed += nUsed;
		if ( nUsed > POOL_COMMIT_SIZE )
		{
			Msg( "pool: %4d\tused: %5d Kb\t commited: %5d Kb\t(%d%%)\n",
				(int)pool.m_nBlockSize, pool.m_nAllocated * (int)pool.m_nBlockSize / 1024,
				nUsed / 1024, nUsed * 100 / (int)( pool.m_pAllocLimit - pool.m_pBase ) );
		}
	}
	Msg( "Total usage: %d Mb\n", nTotalUsed / ( 1024 * 1024 ) );
}

CStdMemAlloc::CStdMemAlloc()
{
	m_hPSAPI = LoadLibraryA( "psapi.dll" );
	m_pfnGetProcessMemoryInfo = m_hPSAPI ? (BOOL ( WINAPI* )( HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD ))
		GetProcAddress( m_hPSAPI, "GetProcessMemoryInfo" ) : NULL;
}

CStdMemAlloc::~CStdMemAlloc()
{
	if ( m_hPSAPI )
	{
		FreeLibrary( m_hPSAPI );
		m_hPSAPI = NULL;
	}
}

void* CStdMemAlloc::Alloc( size_t nSize )
{
	void* pMemory;
	if ( m_SmallBlockHeap.ShouldUse( nSize ) )
	{
		size_t nAllocationSize = nSize ? nSize : 1;
		CSmallBlockPool* pPool = m_SmallBlockHeap.FindPool( nAllocationSize );
		pMemory = pPool->Alloc();
		if ( !pMemory )
		{
			if ( pPool->m_bCommitFailed )
				m_SmallBlockHeap.m_bEnabled = false;
			pMemory = malloc( nAllocationSize );
		}
	}
	else
	{
		pMemory = malloc( nSize );
		AssertFatalMsg1At( pMemory, "CStdMemAlloc::Alloc mem allocation of %d bytes returned NULL", (int)nSize, 640 );
	}

	ApplyMemoryInitializations( pMemory, nSize );
	return pMemory;
}

void* CStdMemAlloc::Realloc( void* pMemory, size_t nSize )
{
	if ( !pMemory )
		return Alloc( nSize );

	if ( !m_SmallBlockHeap.IsOwner( pMemory ) )
		return realloc( pMemory, nSize );

	if ( !nSize )
		nSize = 1;
	CSmallBlockPool* pOldPool = m_SmallBlockHeap.FindPool( pMemory );
	CSmallBlockPool* pNewPool = m_SmallBlockHeap.ShouldUse( nSize ) ? m_SmallBlockHeap.FindPool( nSize ) : NULL;
	if ( pOldPool == pNewPool )
		return pMemory;

	void* pNewMemory = pNewPool ? pNewPool->Alloc() : NULL;
	if ( !pNewMemory )
		pNewMemory = malloc( nSize );

	if ( pNewMemory )
		memcpy( pNewMemory, pMemory, nSize < pOldPool->m_nBlockSize ? nSize : pOldPool->m_nBlockSize );
	pOldPool->Free( pMemory );
	return pNewMemory;
}

void CStdMemAlloc::Free( void* pMemory, int )
{
	if ( !pMemory )
		return;

	if ( m_SmallBlockHeap.IsOwner( pMemory ) )
		m_SmallBlockHeap.FindPool( pMemory )->Free( pMemory );
	else
		free( pMemory );
}

size_t CStdMemAlloc::GetSize( void* pMemory )
{
	if ( m_SmallBlockHeap.IsOwner( pMemory ) )
		return m_SmallBlockHeap.FindPool( pMemory )->m_nBlockSize;
	return _msize( pMemory );
}

size_t CStdMemAlloc::GetMemoryUsed()
{
	PROCESS_MEMORY_COUNTERS counters;
	counters.cb = sizeof( counters );
	if ( m_pfnGetProcessMemoryInfo && m_pfnGetProcessMemoryInfo( GetCurrentProcess(), &counters, sizeof( counters ) ) )
		return counters.PagefileUsage;
	return 0;
}
#endif
