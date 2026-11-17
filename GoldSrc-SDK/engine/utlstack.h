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
// A stack based on a growable array
//=============================================================================

#pragma once

#include "utlmemory.h"
#include <new>

#ifdef UTL_ENGINE_ASSERTS
#ifndef UTLSTACK_SOURCE_PATH
#define UTLSTACK_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\utlstack.h"
#endif
#endif

template <class T> class CUtlStack
{
public:
	// constructor, destructor
	CUtlStack( int growSize = 0, int initSize = 0 );
	~CUtlStack();

	// element access
	T& Element( int i );

	// Looks at the stack top
	T& Top();

	// Size
	int Count() const;

	// Is element index valid?
	bool IsIdxValid( int i ) const;

	int Push( const T& src );

	// Pops the stack
	void Pop();
	void Pop( T& value );

	// Clears the stack, no deallocation
	void Clear();

	// Memory deallocation
	void Purge();

private:
	// Grows the stack allocation
	void GrowStack();

	CUtlMemory<T>	m_Memory;
	int				m_Size;
};

template <class T> CUtlStack<T>::CUtlStack( int growSize, int initSize ) : m_Memory( growSize, initSize ), m_Size( 0 )
{
}

template <class T> CUtlStack<T>::~CUtlStack()
{
	Purge();
}

template <class T> T& CUtlStack<T>::Element( int i )
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !IsIdxValid( i ) )
		_wassert( L"IsIdxValid(i)", UTLSTACK_SOURCE_PATH, 149 );
#endif
	return m_Memory[i];
}

template <class T> T& CUtlStack<T>::Top()
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !( m_Size > 0 ) )
		_wassert( L"m_Size > 0", UTLSTACK_SOURCE_PATH, 184 );
#endif
	return Element( m_Size - 1 );
}

template <class T> int CUtlStack<T>::Count() const
{
	return m_Size;
}

template <class T> bool CUtlStack<T>::IsIdxValid( int i ) const
{
	return ( i >= 0 ) && ( i < m_Size );
}

template <class T> void CUtlStack<T>::GrowStack()
{
	if ( m_Size >= m_Memory.NumAllocated() )
		m_Memory.Grow( 0 );

	++m_Size;
}

template <class T> int CUtlStack<T>::Push( const T& src )
{
	GrowStack();
	new ( &Element( m_Size - 1 ) ) T( src );
	return m_Size - 1;
}

template <class T> void CUtlStack<T>::Pop()
{
#ifdef UTL_ENGINE_ASSERTS
	if ( !( m_Size > 0 ) )
		_wassert( L"m_Size > 0", UTLSTACK_SOURCE_PATH, 275 );
#endif
	Element( m_Size - 1 ).~T();
	--m_Size;
}

template <class T> void CUtlStack<T>::Pop( T& value )
{
	value = Top();
	Pop();
}

template <class T> void CUtlStack<T>::Clear()
{
	for ( int i = m_Size; --i >= 0; )
		Element( i ).~T();

	m_Size = 0;
}

template <class T> void CUtlStack<T>::Purge()
{
	Clear();
	m_Memory.Purge();
}
