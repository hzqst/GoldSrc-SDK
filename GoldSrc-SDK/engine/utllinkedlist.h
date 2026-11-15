//======== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Linked list container class 
//
// $Revision: $
// $NoKeywords: $
//=============================================================================

#pragma once

#include "utlmemory.h"
#include <cstddef>
#include <new>
#include <type_traits>

#ifdef UTL_ENGINE_ASSERTS
#ifndef UTLLINKEDLIST_SOURCE_PATH
#define UTLLINKEDLIST_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\utllinkedlist.h"
#endif
#endif

template <class T, class I> class CUtlLinkedList
{
public:
	// constructor, destructor
	CUtlLinkedList( int growSize = 0, int initialSize = 0 );
	CUtlLinkedList( void* memory, int allocationCount );
	~CUtlLinkedList();
	// gets particular elements
	T& Element( I index );
	const T& Element( I index ) const;
	T& operator[]( I index );
	const T& operator[]( I index ) const;
	// Make sure we have a particular amount of memory
	void EnsureCapacity( int count );
	// Memory deallocation
	void Purge();
	// Delete all the elements then call Purge.
	void PurgeAndDeleteElements();
	// Insertion methods....
	I InsertBefore( I before );
	I InsertAfter( I after );
	I AddToHead();
	I AddToTail();
	I InsertBefore( I before, const T& element );
	I InsertAfter( I after, const T& element );
	I AddToHead( const T& element );
	I AddToTail( const T& element );
	// Find an element and return its index or InvalidIndex() if it couldn't be found.
	I Find( const T& element );
	void FindAndRemove( const T& element );
	// Removal methods
	void Remove( I index );
	void RemoveAll();
	I Alloc( bool multilist );
	void Free( I index );
	// list modification
	void LinkBefore( I before, I index );
	void LinkAfter( I after, I index );
	void Unlink( I index );
	void LinkToHead( I index );
	void LinkToTail( I index );
	static I InvalidIndex();
	static std::size_t ElementSize();
	// list statistics
	int Count() const
	{
		return m_ElementCount;
	}
	I MaxElementIndex() const;
	// Traversing the list
	I Head() const;
	I Tail() const;
	I Previous( I index ) const;
	I Next( I index ) const;
	// Are nodes in the list or valid?
	bool IsValidIndex( I index ) const;
	bool IsInList( I index ) const;

protected:
	void ConstructList();
	// What the linked list element looks like
	struct ListElem_t
	{
		T m_Element;
		I m_Previous;
		I m_Next;
	};
	ListElem_t& InternalElement( I index );
	const ListElem_t& InternalElement( I index ) const;
	void ResetDbgInfo();
	CUtlLinkedList( const CUtlLinkedList& source );

	CUtlMemory<ListElem_t> m_Memory;
	I m_Head;
	I m_Tail;
	I m_FirstFree;
	I m_ElementCount; // The number actually in the list
	I m_TotalElements; // The number allocated
	// For debugging purposes; 
	// it's in release builds so this can be used in libraries correctly
	ListElem_t* m_pElements;
};

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

template <class T, class I>
CUtlLinkedList<T, I>::CUtlLinkedList( int growSize, int initialSize ) : m_Memory( growSize, initialSize )
{
	ConstructList();
}

template <class T, class I> void CUtlLinkedList<T, I>::ConstructList()
{
	m_Head = InvalidIndex();
	m_Tail = InvalidIndex();
	m_FirstFree = InvalidIndex();
	m_TotalElements = 0;
	m_ElementCount = 0;
	ResetDbgInfo();
}

//-----------------------------------------------------------------------------
// gets particular elements
//-----------------------------------------------------------------------------

template <class T, class I> T& CUtlLinkedList<T, I>::Element( I index )
{
	return InternalElement( index ).m_Element;
}

template <class T, class I> const T& CUtlLinkedList<T, I>::Element( I index ) const
{
	return InternalElement( index ).m_Element;
}

template <class T, class I> T& CUtlLinkedList<T, I>::operator[]( I index )
{
	return Element( index );
}

template <class T, class I> const T& CUtlLinkedList<T, I>::operator[]( I index ) const
{
	return Element( index );
}

template <class T, class I> typename CUtlLinkedList<T, I>::ListElem_t& CUtlLinkedList<T, I>::InternalElement( I index )
{
	return m_Memory[index];
}

template <class T, class I>
const typename CUtlLinkedList<T, I>::ListElem_t& CUtlLinkedList<T, I>::InternalElement( I index ) const
{
	return m_Memory[index];
}

template <class T, class I> I CUtlLinkedList<T, I>::InvalidIndex()
{
	return (I)-1;
}

//-----------------------------------------------------------------------------
// Traversing the list
//-----------------------------------------------------------------------------

template <class T, class I> I CUtlLinkedList<T, I>::Head() const
{
	return m_Head;
}

template <class T, class I> I CUtlLinkedList<T, I>::MaxElementIndex() const
{
	return (I)m_Memory.NumAllocated();
}

template <class T, class I> I CUtlLinkedList<T, I>::Tail() const
{
	return m_Tail;
}

template <class T, class I> I CUtlLinkedList<T, I>::Previous( I index ) const
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !IsValidIndex( index ) )
		_wassert( L"IsValidIndex(i)", UTLLINKEDLIST_SOURCE_PATH, 246 );
#endif
	return InternalElement( index ).m_Previous;
}

template <class T, class I> I CUtlLinkedList<T, I>::Next( I index ) const
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !IsValidIndex( index ) )
		_wassert( L"IsValidIndex(i)", UTLLINKEDLIST_SOURCE_PATH, 253 );
#endif
	return InternalElement( index ).m_Next;
}

template <class T, class I> void CUtlLinkedList<T, I>::ResetDbgInfo()
{
	m_pElements = m_Memory.Base();
}

template <class T, class I> bool CUtlLinkedList<T, I>::IsValidIndex( I index ) const
{
	if ( index >= m_TotalElements )
		return false;
	if constexpr ( std::is_signed<I>::value )
	{
		if ( index < 0 )
			return false;
	}
	return InternalElement( index ).m_Previous != index || InternalElement( index ).m_Next == index;
}

template <class T, class I> bool CUtlLinkedList<T, I>::IsInList( I index ) const
{
	if ( index >= m_TotalElements )
		return false;
	if constexpr ( std::is_signed<I>::value )
	{
		if ( index < 0 )
			return false;
	}
	return Previous( index ) != index;
}

template <class T, class I> I CUtlLinkedList<T, I>::Alloc( bool multilist )
{
	I index = m_FirstFree;
	if ( index == InvalidIndex() )
	{
		if ( m_TotalElements == m_Memory.NumAllocated() )
			m_Memory.Grow( 0 );
		index = m_TotalElements++;
	}
	else
	{
		m_FirstFree = InternalElement( index ).m_Next;
	}
	if ( multilist )
	{
		InternalElement( index ).m_Previous = InvalidIndex();
		InternalElement( index ).m_Next = InvalidIndex();
	}
	else
	{
		InternalElement( index ).m_Previous = index;
		InternalElement( index ).m_Next = index;
	}
	new ( &Element( index ) ) T;
	ResetDbgInfo();
	return index;
}

template <class T, class I> void CUtlLinkedList<T, I>::Unlink( I index )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !( IsValidIndex( index ) ) )
		_wassert( L"IsValidIndex(elem)", UTLLINKEDLIST_SOURCE_PATH, 613 );
#endif
	if ( !IsInList( index ) )
		return;
	ListElem_t& element = InternalElement( index );
	if ( element.m_Previous == InvalidIndex() )
		m_Head = element.m_Next;
	else
		InternalElement( element.m_Previous ).m_Next = element.m_Next;
	if ( element.m_Next == InvalidIndex() )
		m_Tail = element.m_Previous;
	else
		InternalElement( element.m_Next ).m_Previous = element.m_Previous;
	element.m_Next = index;
	element.m_Previous = index;
	m_ElementCount--;
}

//-----------------------------------------------------------------------------
// list modification
//-----------------------------------------------------------------------------

template <class T, class I> void CUtlLinkedList<T, I>::LinkBefore( I before, I index )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !( IsValidIndex( index ) ) )
		_wassert( L"IsValidIndex(elem)", UTLLINKEDLIST_SOURCE_PATH, 535 );
#endif
	// Unlink it if it's in the list at the moment
	Unlink( index );
	ListElem_t& element = InternalElement( index );
	element.m_Next = before;
	if ( before == InvalidIndex() )
	{
		element.m_Previous = m_Tail;
		m_Tail = index;
	}
	else
	{
#ifdef UTL_ENGINE_ASSERTS
		if ( !IsInList( before ) )
			_wassert( L"IsInList(before)", UTLLINKEDLIST_SOURCE_PATH, 555 );
#endif
		ListElem_t& beforeElement = InternalElement( before );
		element.m_Previous = beforeElement.m_Previous;
		beforeElement.m_Previous = index;
	}
	// Reset the head if we linked to the head of the list
	if ( element.m_Previous == InvalidIndex() )
		m_Head = index;
	else
		InternalElement( element.m_Previous ).m_Next = index;
	m_ElementCount++;
}

template <class T, class I> void CUtlLinkedList<T, I>::LinkToTail( I index )
{
	LinkBefore( InvalidIndex(), index );
}

//-----------------------------------------------------------------------------
// Insertion methods; allocates and links (uses default constructor)
//-----------------------------------------------------------------------------

template <class T, class I> I CUtlLinkedList<T, I>::InsertBefore( I before )
{
	I index = Alloc( false );
	// Link it in
	LinkBefore( before, index );
	new ( &Element( index ) ) T;
	return index;
}

//-----------------------------------------------------------------------------
// Insertion methods; allocates and links (uses copy constructor)
//-----------------------------------------------------------------------------

template <class T, class I> I CUtlLinkedList<T, I>::InsertBefore( I before, const T& element )
{
	I index = Alloc( false );
	// Link it in
	LinkBefore( before, index );
	new ( &Element( index ) ) T( element );
	return index;
}

template <class T, class I> I CUtlLinkedList<T, I>::AddToTail( const T& item )
{
	return InsertBefore( InvalidIndex(), item );
}

template <class T, class I> I CUtlLinkedList<T, I>::AddToTail()
{
	return InsertBefore( InvalidIndex() );
}

template <class T, class I> void CUtlLinkedList<T, I>::Free( I index )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !IsValidIndex( index ) )
		_wassert( L"IsValidIndex(elem)", UTLLINKEDLIST_SOURCE_PATH, 371 );
#endif
	Element( index ).~T();
	if ( IsInList( index ) )
		Unlink( index );
	InternalElement( index ).m_Next = m_FirstFree;
	m_FirstFree = index;
}

template <class T, class I> void CUtlLinkedList<T, I>::Remove( I index )
{
	Free( index );
}

template <class T, class I> void CUtlLinkedList<T, I>::RemoveAll()
{
	if ( !m_TotalElements )
		return;
	I firstFree = InvalidIndex();
	I index = m_TotalElements;
	while ( index > 0 )
	{
		--index;
		if ( IsValidIndex( index ) )
			Element( index ).~T();
		InternalElement( index ).m_Next = firstFree;
		InternalElement( index ).m_Previous = index;
		firstFree = index;
	}
	// First free points to the first element
	m_FirstFree = 0;
	// Clear everything else out
	m_Head = InvalidIndex();
	m_Tail = InvalidIndex();
	m_ElementCount = 0;
}

template <class T, class I> CUtlLinkedList<T, I>::~CUtlLinkedList()
{
	RemoveAll();
}
