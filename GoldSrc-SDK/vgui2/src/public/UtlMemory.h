//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// A growable memory class.
//=============================================================================

#ifndef UTLMEMORY_H
#define UTLMEMORY_H

#ifdef _WIN32
#pragma once
#endif

#include <assert.h>
#include <stdlib.h>
#include <new>

#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
#define UTL_ENGINE_ASSERTS
#endif

#ifdef _WIN32
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );
#endif

#ifdef UTL_ENGINE_ASSERTS
#define UTLMEMORY_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\UtlMemory.h"
#endif

//-----------------------------------------------------------------------------
// The CUtlMemory class:
// A growable memory class which doubles in size by default.
//-----------------------------------------------------------------------------
template <class T>
class CUtlMemory
{
public:
	// constructor, destructor
	CUtlMemory( int growSize = 0, int initSize = 0 );
	CUtlMemory( T* pMemory, int numElements );
	~CUtlMemory();

	// element access
	T& operator[]( int i );
	const T& operator[]( int i ) const;

	// Can we use this index?
	bool IsIdxValid( int i ) const;

	// Gets the base address (can change when adding elements!)
	T* Base();
	const T* Base() const;

	int NumAllocated() const;

	// Grows the memory, so that at least allocated + num elements are allocated
	void Grow( int num = 0 );
	// Makes sure we've got at least this much memory
	void EnsureCapacity( int num );
	// Memory deallocation
	void Purge();

	// is the memory externally allocated?
	bool IsExternallyAllocated() const;

	// Set the size by which the memory grows
	void SetGrowSize( int size );
	int GetGrowSize() const;

	enum
	{
		EXTERNAL_BUFFER_MARKER	= -1
	};

private:
	CUtlMemory( const CUtlMemory<T>& other );
	CUtlMemory<T>& operator=( const CUtlMemory<T>& other );

	T*	m_pMemory;
	int	m_NumAllocated;
	int	m_GrowSize;
};

template <class T>
inline CUtlMemory<T>::CUtlMemory( int growSize, int initSize )
	: m_pMemory( NULL ), m_NumAllocated( initSize ), m_GrowSize( growSize )
{
	if ( m_NumAllocated > 0 )
	{
		m_pMemory = (T*)malloc( m_NumAllocated * sizeof( T ) );
	}
}

template <class T>
inline CUtlMemory<T>::CUtlMemory( T* pMemory, int numElements )
	: m_pMemory( pMemory ), m_NumAllocated( numElements ), m_GrowSize( EXTERNAL_BUFFER_MARKER )
{
}

template <class T>
inline CUtlMemory<T>::~CUtlMemory()
{
	Purge();
}

//-----------------------------------------------------------------------------
// element access
//-----------------------------------------------------------------------------
template <class T>
inline T& CUtlMemory<T>::operator[]( int i )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !IsIdxValid( i ) )
	{
		_wassert( L"IsIdxValid(i)", UTLMEMORY_SOURCE_PATH, 155 );
	}
#endif

	return m_pMemory[i];
}

template <class T>
inline const T& CUtlMemory<T>::operator[]( int i ) const
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !IsIdxValid( i ) )
	{
		_wassert( L"IsIdxValid(i)", UTLMEMORY_SOURCE_PATH, 162 );
	}
#endif

	return m_pMemory[i];
}

//-----------------------------------------------------------------------------
// Is element index valid?
//-----------------------------------------------------------------------------
template <class T>
inline bool CUtlMemory<T>::IsIdxValid( int i ) const
{
	return ( i >= 0 ) && ( i < m_NumAllocated );
}

//-----------------------------------------------------------------------------
// Gets the base address (can change when adding elements!)
//-----------------------------------------------------------------------------
template <class T>
inline T* CUtlMemory<T>::Base()
{
	return m_pMemory;
}

template <class T>
inline const T* CUtlMemory<T>::Base() const
{
	return m_pMemory;
}

//-----------------------------------------------------------------------------
// Size
//-----------------------------------------------------------------------------
template <class T>
inline int CUtlMemory<T>::NumAllocated() const
{
	return m_NumAllocated;
}

//-----------------------------------------------------------------------------
// is the memory externally allocated?
//-----------------------------------------------------------------------------
template <class T>
inline bool CUtlMemory<T>::IsExternallyAllocated() const
{
	return m_GrowSize < 0;
}

template <class T>
inline void CUtlMemory<T>::SetGrowSize( int size )
{
	assert( size >= 0 && !IsExternallyAllocated() );
	if ( size >= 0 && !IsExternallyAllocated() )
	{
		m_GrowSize = size;
	}
}

template <class T>
inline int CUtlMemory<T>::GetGrowSize() const
{
	return m_GrowSize;
}

//-----------------------------------------------------------------------------
// Makes sure we've got at least this much memory
//-----------------------------------------------------------------------------
template <class T>
inline void CUtlMemory<T>::EnsureCapacity( int num )
{
	if ( m_NumAllocated >= num )
	{
		return;
	}

	Grow( num - m_NumAllocated );
}

//-----------------------------------------------------------------------------
// Grows the memory
//-----------------------------------------------------------------------------
template <class T>
inline void CUtlMemory<T>::Grow( int num )
{
	int growSize;

	if ( num > 0 )
	{
#ifdef UTL_ENGINE_ASSERTS
		if ( IsExternallyAllocated() )
		{
			_wassert( L"0", UTLMEMORY_SOURCE_PATH, 233 );
		}
#endif

		growSize = num;
	}
	else
	{
		growSize = m_GrowSize;
		if ( growSize == 0 )
		{
			growSize = m_NumAllocated ? m_NumAllocated : 1;
		}
	}

	int numAllocated = m_NumAllocated + growSize;
	T* pMemory;
	if ( m_pMemory )
	{
		pMemory = (T*)realloc( m_pMemory, numAllocated * sizeof( T ) );
	}
	else
	{
		pMemory = (T*)malloc( numAllocated * sizeof( T ) );
	}

	// keep the old block on failure instead of continuing with a null pointer
	if ( !pMemory )
	{
#ifdef UTL_ENGINE_ASSERTS
		_wassert( L"m_pMemory", UTLMEMORY_SOURCE_PATH, 249 );
#endif
		abort();
	}

	m_pMemory = pMemory;
	m_NumAllocated = numAllocated;
}

//-----------------------------------------------------------------------------
// Memory deallocation
//-----------------------------------------------------------------------------
template <class T>
inline void CUtlMemory<T>::Purge()
{
	if ( IsExternallyAllocated() )
	{
		return;
	}

	if ( m_pMemory )
	{
		free( m_pMemory );
		m_pMemory = NULL;
	}

	m_NumAllocated = 0;
}

#endif // UTLMEMORY_H
