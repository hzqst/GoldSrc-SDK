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
// A growable array class that maintains a free list and keeps elements
// in the same location
//=============================================================================

#ifndef UTLVECTOR_H
#define UTLVECTOR_H

#ifdef _WIN32
#pragma once
#endif

#include "UtlMemory.h"

#include <new>
#include <string.h>

#ifdef _WIN32
#define UTLVECTOR_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\UtlVector.h"
#endif

template <class T>
class CUtlVector
{
public:
	typedef T ElemType_t;

	// constructor, destructor
	CUtlVector( int growSize = 0, int initSize = 0 );
	~CUtlVector();

	// element access
	T& operator[]( int i );
	const T& operator[]( int i ) const;
	T& Element( int i );
	const T& Element( int i ) const;

	// Gets the base address (can change when adding elements!)
	T* Base();
	const T* Base() const;

	// Returns the number of elements in the vector
	// SIZE IS DEPRECATED!
	int Count() const;
	int Size() const;	// don't use me!
	bool IsValidIndex( int i ) const;

	// Makes sure we have enough memory allocated to store a requested # of elements
	void EnsureCapacity( int num );

	int AddToTail();
	int AddToTail( const T& src );
	int AddMultipleToTail( int num );
	int InsertBefore( int elem );
	int InsertBefore( int elem, const T& src );
	int InsertMultipleBefore( int elem, int num );

	void Remove( int elem );
	void FastRemove( int elem );	// doesn't preserve order
	void RemoveAll();				// doesn't deallocate memory
	void Purge();

private:
	// Grows the vector
	void GrowVector( int num = 1 );
	// Shifts elements....
	void ShiftElementsRight( int elem, int num = 1 );
	void ShiftElementsLeft( int elem, int num = 1 );

	CUtlMemory<T>	m_Memory;
	int				m_Size;
	// For easier access to the elements through the debugger
	// it's in release builds so this can be used in libraries correctly
	T*				m_pElements;
};

template <class T>
inline CUtlVector<T>::CUtlVector( int growSize, int initSize )
	: m_Memory( growSize, initSize ), m_Size( 0 ), m_pElements( NULL )
{
}

template <class T>
inline CUtlVector<T>::~CUtlVector()
{
	Purge();
}

template <class T>
inline T& CUtlVector<T>::operator[]( int i )
{
#ifdef _WIN32
	if ( !IsValidIndex( i ) )
	{
		_wassert( L"IsValidIndex(i)", UTLVECTOR_SOURCE_PATH, 184 );
	}
#endif

	return m_Memory[i];
}

template <class T>
inline const T& CUtlVector<T>::operator[]( int i ) const
{
#ifdef _WIN32
	if ( !IsValidIndex( i ) )
	{
		_wassert( L"IsValidIndex(i)", UTLVECTOR_SOURCE_PATH, 191 );
	}
#endif

	return m_Memory[i];
}

template <class T>
inline T& CUtlVector<T>::Element( int i )
{
#ifdef _WIN32
	if ( !IsValidIndex( i ) )
	{
		_wassert( L"IsValidIndex(i)", UTLVECTOR_SOURCE_PATH, 198 );
	}
#endif

	return m_Memory[i];
}

template <class T>
inline const T& CUtlVector<T>::Element( int i ) const
{
	return m_Memory[i];
}

//-----------------------------------------------------------------------------
// Gets the base address (can change when adding elements!)
//-----------------------------------------------------------------------------

template <class T>
inline T* CUtlVector<T>::Base()
{
	return m_Memory.Base();
}

template <class T>
inline const T* CUtlVector<T>::Base() const
{
	return m_Memory.Base();
}

template <class T>
inline int CUtlVector<T>::Count() const
{
	return m_Size;
}

//-----------------------------------------------------------------------------
// Count
//-----------------------------------------------------------------------------

template <class T>
inline int CUtlVector<T>::Size() const
{
	return m_Size;
}

//-----------------------------------------------------------------------------
// Is element index valid?
//-----------------------------------------------------------------------------

template <class T>
inline bool CUtlVector<T>::IsValidIndex( int i ) const
{
	return ( i >= 0 ) && ( i < m_Size );
}

//-----------------------------------------------------------------------------
// Makes sure we have enough memory allocated to store a requested # of elements
//-----------------------------------------------------------------------------
template <class T>
inline void CUtlVector<T>::EnsureCapacity( int num )
{
	m_Memory.EnsureCapacity( num );
}

template <class T>
void CUtlVector<T>::GrowVector( int num )
{
	if ( m_Size + num > m_Memory.NumAllocated() )
	{
		int growSize = m_Size + num - m_Memory.NumAllocated();
		if ( growSize < m_Memory.GetGrowSize() )
		{
			growSize = m_Memory.GetGrowSize();
		}

		m_Memory.Grow( growSize );
	}

	m_Size += num;

#ifdef _WIN32
	if ( !( m_Size <= m_Memory.NumAllocated() ) )
	{
		_wassert( L"m_Size <= m_Memory.NumAllocated()", UTLVECTOR_SOURCE_PATH, 270 );
	}
#endif
}

//-----------------------------------------------------------------------------
// Shifts elements
//-----------------------------------------------------------------------------
template <class T>
void CUtlVector<T>::ShiftElementsRight( int elem, int num )
{
#ifdef _WIN32
	if ( !( IsValidIndex( elem ) || ( m_Size == 0 ) || ( num == 0 ) ) )
	{
		_wassert( L"IsValidIndex(elem) || ( m_Size == 0 ) || ( num == 0 )", UTLVECTOR_SOURCE_PATH, 293 );
	}
#endif

	int numToMove = m_Size - elem - num;
	if ( ( numToMove > 0 ) && ( num > 0 ) )
	{
		memmove( &Element( elem + num ), &Element( elem ), numToMove * sizeof( T ) );
	}
}

template <class T>
void CUtlVector<T>::ShiftElementsLeft( int elem, int num )
{
#ifdef _WIN32
	if ( !( IsValidIndex( elem ) || ( m_Size == 0 ) || ( num == 0 ) ) )
	{
		_wassert( L"IsValidIndex(elem) || ( m_Size == 0 ) || ( num == 0 )", UTLVECTOR_SOURCE_PATH, 302 );
	}
#endif

	int numToMove = m_Size - elem - num;
	if ( ( numToMove > 0 ) && ( num > 0 ) )
	{
		memmove( &Element( elem ), &Element( elem + num ), numToMove * sizeof( T ) );
	}
}

template <class T>
inline int CUtlVector<T>::AddToTail()
{
	return InsertBefore( m_Size );
}

template <class T>
int CUtlVector<T>::InsertBefore( int elem )
{
#ifdef _WIN32
	// Can insert at the end
	if ( !( ( elem == Size() ) || IsValidIndex( elem ) ) )
	{
		_wassert( L"(elem == Size()) || IsValidIndex(elem)", UTLVECTOR_SOURCE_PATH, 340 );
	}
#endif

	GrowVector();
	ShiftElementsRight( elem );
	new ( &Element( elem ) ) T;
	return elem;
}

template <class T>
inline int CUtlVector<T>::AddToTail( const T& src )
{
	return InsertBefore( m_Size, src );
}

template <class T>
int CUtlVector<T>::InsertBefore( int elem, const T& src )
{
#ifdef _WIN32
	// Can insert at the end
	if ( !( ( elem == Size() ) || IsValidIndex( elem ) ) )
	{
		_wassert( L"(elem == Size()) || IsValidIndex(elem)", UTLVECTOR_SOURCE_PATH, 375 );
	}
#endif

	GrowVector();
	ShiftElementsRight( elem );
	new ( &Element( elem ) ) T( src );
	return elem;
}

template <class T>
inline int CUtlVector<T>::AddMultipleToTail( int num )
{
	return InsertMultipleBefore( m_Size, num );
}

template <class T>
int CUtlVector<T>::InsertMultipleBefore( int elem, int num )
{
	if ( num == 0 )
	{
		return elem;
	}

#ifdef _WIN32
	// Can insert at the end
	if ( !( ( elem == Size() ) || IsValidIndex( elem ) ) )
	{
		_wassert( L"(elem == Size()) || IsValidIndex(elem)", UTLVECTOR_SOURCE_PATH, 443 );
	}
#endif

	GrowVector( num );
	ShiftElementsRight( elem, num );

	for ( int i = 0; i < num; ++i )
	{
		new ( &Element( elem + i ) ) T;
	}

	return elem;
}

//-----------------------------------------------------------------------------
// Element removal
//-----------------------------------------------------------------------------

template <class T>
void CUtlVector<T>::FastRemove( int elem )
{
#ifdef _WIN32
	if ( !IsValidIndex( elem ) )
	{
		_wassert( L"IsValidIndex(elem)", UTLVECTOR_SOURCE_PATH, 483 );
	}
#endif

	Element( elem ).~T();
	if ( m_Size > 0 )
	{
		memcpy( &Element( elem ), &Element( m_Size - 1 ), sizeof( T ) );
		--m_Size;
	}
}

template <class T>
void CUtlVector<T>::Remove( int elem )
{
	Element( elem ).~T();
	ShiftElementsLeft( elem );
	--m_Size;
}

template <class T>
void CUtlVector<T>::RemoveAll()
{
	for ( int i = m_Size; --i >= 0; )
	{
		Element( i ).~T();
	}

	m_Size = 0;
}

//-----------------------------------------------------------------------------
// Memory deallocation
//-----------------------------------------------------------------------------

template <class T>
inline void CUtlVector<T>::Purge()
{
	RemoveAll();
	m_Memory.Purge();
}

#endif
