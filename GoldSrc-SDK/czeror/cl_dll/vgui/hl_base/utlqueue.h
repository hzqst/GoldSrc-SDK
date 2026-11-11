//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef UTLQUEUE_H
#define UTLQUEUE_H

#include "utlvector.h"

template<class T> class CUtlQueue
{
public:
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
	CUtlQueue( int growSize, int initSize ) : m_heap( growSize, initSize )
	{
	}
	CUtlQueue( T* memory, int count );
	const T& RemoveAtHead()
	{
		m_current = m_heap[0];
		m_heap.Remove( 0 );
		return m_current;
	}
	const T& RemoveAtTail();
	const T& Head()
	{
		m_current = m_heap[0];
		return m_current;
	}
	const T& Tail();
	void Insert( const T& value ) { m_heap.AddToTail( value ); }
	bool Check( int value )
	{
		for ( int i = 0; i < m_heap.Count(); i++ )
		{
			if ( m_heap[i] == value )
				return true;
		}
		return false;
	}

	// Returns the count of elements in the stack
	int Count() const { return m_heap.Count(); }
	void RemoveAll();

	// Memory deallocation
	void Purge() { m_heap.Purge(); }

protected:
	CUtlVector<T> m_heap;
	T m_current;
};

#endif
