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

#include "UtlMemory.h"
#include <new>

template <class T> class CUtlStack
{
public:
	CUtlStack() : m_Size( 0 ) {}
	~CUtlStack()
	{
		while ( m_Size )
			m_Memory[--m_Size].~T();
	}

	void Push( const T& value )
	{
		if ( m_Size == m_Memory.NumAllocated() )
			m_Memory.Grow( 0 );
		new ( &m_Memory[m_Size++] ) T( value );
	}

	void Pop( T& value )
	{
		value = m_Memory[--m_Size];
		m_Memory[m_Size].~T();
	}

private:
	CUtlMemory<T>	m_Memory;
	int				m_Size;
};
