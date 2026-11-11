//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef UTLPRIORITYQUEUE_H
#define UTLPRIORITYQUEUE_H

#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

template <class T>
class CUtlPriorityQueue
{
public:
	// Less func typedef
	// Returns true if the first parameter is "less priority" than the second
	// Items that are "less priority" sort toward the tail of the queue
	typedef bool ( *LessFunc_t )( const T&, const T& );
	typedef T ElemType_t;

	// constructor: lessfunc is required, but may be set after the constructor with
	// SetLessFunc
	CUtlPriorityQueue( int growSize = 0, int initSize = 0, LessFunc_t lessfunc = NULL );

	const T& ElementAtHead() const;

	void RemoveAtHead();
	void Insert( const T& element );

	int Count() const;

protected:
	void Swap( int index1, int index2 );

	CUtlVector<T>	m_heap;
	// Used for sorting.
	LessFunc_t		m_LessFunc;
};

template <class T>
inline CUtlPriorityQueue<T>::CUtlPriorityQueue( int growSize, int initSize, LessFunc_t lessfunc )
	: m_heap( growSize, initSize ), m_LessFunc( lessfunc )
{
}

template <class T>
inline const T& CUtlPriorityQueue<T>::ElementAtHead() const
{
	return m_heap[0];
}

template <class T>
inline int CUtlPriorityQueue<T>::Count() const
{
	return m_heap.Count();
}

template <class T>
inline void CUtlPriorityQueue<T>::Swap( int index1, int index2 )
{
	T tmp = m_heap[index1];
	m_heap[index1] = m_heap[index2];
	m_heap[index2] = tmp;
}

#endif // UTLPRIORITYQUEUE_H
