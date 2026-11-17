//=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//=============================================================================

#include "MemPool.h"

#include <climits>
#include <cmath>
#include <cstdlib>
#include <new>

#if defined( _MSC_VER )
#include <sal.h>
#endif

CMemoryPool::CMemoryPool( int blockSize, int blocksPerBlob )
{
	_blockSize = blockSize;
	_numBlobs = 0;
	_numElements = 0;
	_blocksPerBlob = blocksPerBlob;
	AddNewBlob();
	_peakAlloc = 0;
	_blocksAllocated = 0;
}

CMemoryPool::~CMemoryPool()
{
	for ( int i = 0; i < _numBlobs; i++ )
		free( _memBlob[i] );
}

#if defined( _MSC_VER )
_Post_satisfies_( condition )
#endif
static inline void MemPool_Assert( bool condition )
{
	if ( !condition )
	{
#if defined( _MSC_VER )
		__debugbreak();
#endif // _DEBUG
		abort();
	}
}

void CMemoryPool::AddNewBlob()
{
	MemPool_Assert( _numBlobs >= 0 && _numBlobs < MAX_BLOBS );
	MemPool_Assert( _blocksPerBlob > 0 && _blockSize >= (int)sizeof( void* ) );

	// reject blob sizes that would overflow int before allocating
	int sizeMultiplier = (int)pow( 2.0, (double)_numBlobs );
	MemPool_Assert( _blocksPerBlob <= INT_MAX / sizeMultiplier );
	int nElements = sizeMultiplier * _blocksPerBlob;
	MemPool_Assert( _blockSize <= INT_MAX / nElements && _numElements <= INT_MAX - nElements );
	int blobSize = nElements * _blockSize;

	_memBlob[_numBlobs] = malloc( blobSize );
	MemPool_Assert( _memBlob[_numBlobs] != NULL );

	_headOfFreeList = _memBlob[_numBlobs];
	MemPool_Assert( _headOfFreeList != NULL );

	void** newBlob = (void**)_headOfFreeList;
	for ( int j = 0; j < nElements - 1; j++ )
	{
		newBlob[0] = (char*)newBlob + _blockSize;
		newBlob = (void**)newBlob[0];
	}
	// null terminate list
	newBlob[0] = NULL;

	_numBlobs++;
	_numElements += nElements;

	MemPool_Assert( _numBlobs < MAX_BLOBS - 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Allocs a single block of memory from the pool.  
// Input  : amount - 
//-----------------------------------------------------------------------------
void* CMemoryPool::Alloc( unsigned int amount )
{
	void* returnBlock;

	if ( amount > (unsigned int)_blockSize )
		return NULL;

	int allocated = _blocksAllocated + 1;
#if defined( _WIN32 )
	_blocksAllocated = allocated;
	if ( _peakAlloc <= _blocksAllocated )
		_peakAlloc = _blocksAllocated;
	if ( allocated >= _numElements )
		AddNewBlob();
#else
	if ( allocated >= _numElements )
		AddNewBlob();

	_blocksAllocated = allocated;
	if ( _peakAlloc <= _blocksAllocated )
		_peakAlloc = _blocksAllocated;
#endif

	MemPool_Assert( _headOfFreeList != NULL );

	returnBlock = _headOfFreeList;
	_headOfFreeList = *(void**)_headOfFreeList;

	return returnBlock;
}

void CMemoryPool::Free( void* memblock )
{
	if ( !memblock )
		return;  // trying to delete NULL pointer, ignore

	_blocksAllocated--;
	*(void**)memblock = _headOfFreeList;
	_headOfFreeList = memblock;
}
