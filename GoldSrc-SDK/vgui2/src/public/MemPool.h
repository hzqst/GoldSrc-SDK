//=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

#ifndef MEMPOOL_H
#define MEMPOOL_H

class CMemoryPool
{
public:
	CMemoryPool( int blockSize, int blocksPerBlob );
	~CMemoryPool();

	void* Alloc( unsigned int amount );
	void Free( void* memory );

	int Count();

private:
	void AddNewBlob();

	int	_blockSize;
	int	_blocksPerBlob;
	int	_numElements;

	enum
	{
		MAX_BLOBS	= 16
	};

	void*	_memBlob[MAX_BLOBS];
	void*	_headOfFreeList;
	int		_numBlobs;
	int		_peakAlloc;
	int		_blocksAllocated;
};

#endif
