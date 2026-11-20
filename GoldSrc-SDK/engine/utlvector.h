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

#pragma once

#include "utlmemory.h"

#include <cstring>
#include <new>

#ifdef UTL_ENGINE_ASSERTS
#define UTLVECTOR_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\UtlVector.h"
#endif

//-----------------------------------------------------------------------------
// The CUtlVector class:
// A growable array class which doubles in size by default.
// It will always keep all elements consecutive in memory, and may move the
// elements around in memory (via a realloc) when elements are inserted or
// removed. Clients should therefore refer to the elements of the vector
// by index (they should *never* maintain pointers to elements in the vector).
//-----------------------------------------------------------------------------

template <class T> class CUtlVector
{
public:
	// constructor, destructor
	CUtlVector( int growSize = 0, int initialSize = 0 );
	CUtlVector( T* memory, int elementCount );
	~CUtlVector();
	T& operator[]( int index );
	const T& operator[]( int index ) const;
	T& Element( int index );
	const T& Element( int index ) const;
	// Gets the base address (can change when adding elements!)
	T* Base();
	const T* Base() const;
	// Returns the number of elements in the vector
	// SIZE IS DEPRECATED!
	int Count() const;
	int Size() const;	// don't use me!
	bool IsValidIndex( int index ) const;
	int AddToHead();
	int AddToTail();
	int InsertBefore( int index );
	int InsertAfter( int index );
	int AddToHead( const T& source );
	int AddToTail( const T& source );
	int InsertBefore( int index, const T& source );
	int InsertAfter( int index, const T& source );
	// Adds multiple elements, uses default constructor
	int AddMultipleToHead( int count );
	int AddMultipleToTail( int count );
	int InsertMultipleBefore( int index, int count );
	int InsertMultipleAfter( int index, int count );
	void SetSize( int size );
	void CopyArray( const T* source, int size );
	// Add the specified array to the tail.
	int AddVectorToTail( const CUtlVector<T>& source );
	int Find( const T& value );
	bool HasElement( const T& value );
	void EnsureCapacity( int count );
	// Element removal
	void FastRemove( int index ); // doesn't preserve order
	void Remove( int index ); // preserves order, shifts elements
	void FindAndRemove( const T& value ); // removes first occurrence of src, preserves order, shifts elements
	void RemoveMultiple( int index, int count ); // preserves order, shifts elements
	void RemoveAll();				// doesn't deallocate memory
	// Memory deallocation
	void Purge();
	// Purges the list and calls delete on each element in it.
	void PurgeAndDeleteElements();
	// Set the size by which it grows when it needs to allocate more memory.
	void SetGrowSize( int growSize );

protected:
	CUtlVector( const CUtlVector<T>& source );

private:
	void GrowVector( int count );
	void ShiftElementsRight( int index, int count );
	void ShiftElementsLeft( int index, int count );
	// For easier access to the elements through the debugger
	void ResetDbgInfo();
	CUtlMemory<T> m_Memory;
	int m_Size;
	T* m_pElements;
};

template <class T>
CUtlVector<T>::CUtlVector( int growSize, int initialSize ) : m_Memory( growSize, initialSize ), m_Size( 0 ), m_pElements( NULL )
{
}

template <class T> CUtlVector<T>::~CUtlVector()
{
	Purge();
}

template <class T> void CUtlVector<T>::RemoveAll()
{
	for ( int i = m_Size; --i >= 0; )
		Element( i ).~T();

	m_Size = 0;
}

//-----------------------------------------------------------------------------
// Memory deallocation
//-----------------------------------------------------------------------------

template <class T> void CUtlVector<T>::Purge()
{
	RemoveAll();
	m_Memory.Purge();
}

template <class T> int CUtlVector<T>::Count() const
{
	return m_Size;
}

//-----------------------------------------------------------------------------
// Count
//-----------------------------------------------------------------------------

template <class T> int CUtlVector<T>::Size() const
{
	return m_Size;
}

//-----------------------------------------------------------------------------
// Is element index valid?
//-----------------------------------------------------------------------------

template <class T> bool CUtlVector<T>::IsValidIndex( int index ) const
{
	return index >= 0 && index < m_Size;
}

//-----------------------------------------------------------------------------
// Finds an element (element needs operator== defined)
//-----------------------------------------------------------------------------
template <class T> int CUtlVector<T>::Find( const T& value )
{
	for ( int i = 0; i < m_Size; ++i )
	{
		if ( Element( i ) == value )
			return i;
	}
	return -1;
}

template <class T> bool CUtlVector<T>::HasElement( const T& value )
{
	return Find( value ) >= 0;
}

template <class T> T& CUtlVector<T>::operator[]( int index )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !IsValidIndex( index ) )
		_wassert( L"IsValidIndex(i)", UTLVECTOR_SOURCE_PATH, 184 );
#endif
	return m_Memory[index];
}

template <class T> const T& CUtlVector<T>::operator[]( int index ) const
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !IsValidIndex( index ) )
		_wassert( L"IsValidIndex(i)", UTLVECTOR_SOURCE_PATH, 191 );
#endif
	return m_Memory[index];
}

template <class T> T& CUtlVector<T>::Element( int index )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !IsValidIndex( index ) )
		_wassert( L"IsValidIndex(i)", UTLVECTOR_SOURCE_PATH, 198 );
#endif
	return m_Memory[index];
}

template <class T> const T& CUtlVector<T>::Element( int index ) const
{
	return m_Memory[index];
}

//-----------------------------------------------------------------------------
// Gets the base address (can change when adding elements!)
//-----------------------------------------------------------------------------

template <class T> T* CUtlVector<T>::Base()
{
	return m_Memory.Base();
}

template <class T> const T* CUtlVector<T>::Base() const
{
	return m_Memory.Base();
}

template <class T> void CUtlVector<T>::GrowVector( int num )
{
	if ( m_Size + num > m_Memory.NumAllocated() )
	{
		int growSize = m_Size + num - m_Memory.NumAllocated();
		if ( growSize < m_Memory.GetGrowSize() )
			growSize = m_Memory.GetGrowSize();

		m_Memory.Grow( growSize );
	}

	m_Size += num;
#ifdef UTL_ENGINE_ASSERTS
	if ( !( m_Size <= m_Memory.NumAllocated() ) )
		_wassert( L"m_Size <= m_Memory.NumAllocated()", UTLVECTOR_SOURCE_PATH, 270 );
#endif
}

//-----------------------------------------------------------------------------
// Shifts elements
//-----------------------------------------------------------------------------
template <class T> void CUtlVector<T>::ShiftElementsRight( int elem, int num )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !( IsValidIndex( elem ) || ( m_Size == 0 ) || ( num == 0 ) ) )
		_wassert( L"IsValidIndex(elem) || ( m_Size == 0 ) || ( num == 0 )", UTLVECTOR_SOURCE_PATH, 293 );
#endif
	int numToMove = m_Size - elem - num;
	if ( ( numToMove > 0 ) && ( num > 0 ) )
		memmove( &Element( elem + num ), &Element( elem ), numToMove * sizeof( T ) );
}
template <class T> int CUtlVector<T>::InsertBefore( int index, const T& source )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !( ( index == Size() ) || IsValidIndex( index ) ) )
		_wassert( L"(elem == Size()) || IsValidIndex(elem)", UTLVECTOR_SOURCE_PATH, 375 );
#endif
	GrowVector( 1 );
	ShiftElementsRight( index, 1 );
	new ( &Element( index ) ) T( source );
	return index;
}

template <class T> int CUtlVector<T>::AddToTail()
{
	return InsertBefore( m_Size );
}

template <class T> int CUtlVector<T>::AddToTail( const T& source )
{
	return InsertBefore( m_Size, source );
}

//-----------------------------------------------------------------------------
// Makes sure we have enough memory allocated to store a requested # of elements
//-----------------------------------------------------------------------------
template <class T> void CUtlVector<T>::EnsureCapacity( int count )
{
	m_Memory.EnsureCapacity( count );
}

template <class T> int CUtlVector<T>::AddMultipleToTail( int count )
{
	return InsertMultipleBefore( m_Size, count );
}

template <class T> int CUtlVector<T>::InsertMultipleBefore( int index, int count )
{
	if ( !count )
		return index;
#ifdef UTL_ENGINE_ASSERTS
	if ( !( ( index == Size() ) || IsValidIndex( index ) ) )
		_wassert( L"(elem == Size()) || IsValidIndex(elem)", UTLVECTOR_SOURCE_PATH, 443 );
#endif
	GrowVector( count );
	ShiftElementsRight( index, count );
	for ( int element = 0; element < count; ++element )
		new ( &Element( index + element ) ) T;
	return index;
}

template <class T> int CUtlVector<T>::InsertBefore( int index )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !( ( index == Size() ) || IsValidIndex( index ) ) )
		_wassert( L"(elem == Size()) || IsValidIndex(elem)", UTLVECTOR_SOURCE_PATH, 340 );
#endif
	GrowVector( 1 );
	ShiftElementsRight( index, 1 );
	new ( &Element( index ) ) T;
	return index;
}

template <class T> void CUtlVector<T>::ShiftElementsLeft( int index, int count )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !( IsValidIndex( index ) || ( m_Size == 0 ) || ( count == 0 ) ) )
		_wassert( L"IsValidIndex(elem) || ( m_Size == 0 ) || ( num == 0 )", UTLVECTOR_SOURCE_PATH, 302 );
#endif
	int moveCount = m_Size - index - count;
	if ( ( moveCount > 0 ) && ( count > 0 ) )
		std::memmove( &Element( index ), &Element( index + count ), moveCount * sizeof( T ) );
}

template <class T> void CUtlVector<T>::Remove( int index )
{
	Element( index ).~T();
	ShiftElementsLeft( index, 1 );
	--m_Size;
}

//-----------------------------------------------------------------------------
// Element removal
//-----------------------------------------------------------------------------

template <class T> void CUtlVector<T>::FastRemove( int index )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !( IsValidIndex( index ) ) )
		_wassert( L"IsValidIndex(elem)", UTLVECTOR_SOURCE_PATH, 483 );
#endif
	Element( index ).~T();
	if ( m_Size > 0 )
	{
		std::memcpy( &Element( index ), &Element( m_Size - 1 ), sizeof( T ) );
		--m_Size;
	}
}

template <class T> void CUtlVector<T>::FindAndRemove( const T& value )
{
	int index = Find( value );
	if ( index != -1 )
		Remove( index );
}

template <class T> void CUtlVector<T>::RemoveMultiple( int index, int count )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !IsValidIndex( index ) )
		_wassert( L"IsValidIndex(elem)", UTLVECTOR_SOURCE_PATH, 514 );
	if ( !( index + count <= Size() ) )
		_wassert( L"elem + num <= Size()", UTLVECTOR_SOURCE_PATH, 515 );
#endif
	for ( int i = index + count; --i >= index; )
		Element( i ).~T();

	ShiftElementsLeft( index, count );
	m_Size -= count;
}
