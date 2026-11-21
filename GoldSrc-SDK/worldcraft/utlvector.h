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

#ifndef HAMMER_UTLVECTOR_H
#define HAMMER_UTLVECTOR_H
#pragma once

#include "../engine/utlvector.h"
#include <assert.h>

template<class T> void CUtlVector<T>::SetGrowSize( int size )
{
	m_Memory.SetGrowSize( size );
}

//-----------------------------------------------------------------------------
// Adds an element, uses copy constructor
//-----------------------------------------------------------------------------

template<class T> int CUtlVector<T>::AddToHead( const T& source )
{
	T value = source;
	return InsertBefore( 0, value );
}

template<class T> int CUtlVector<T>::AddVectorToTail( const CUtlVector<T>& source )
{
	int start = Count();
	int count = source.Count();
	EnsureCapacity( start + count );
	for ( int i = 0; i < count; i++ )
	{
		T value = source[i];
		AddToTail( value );
	}
	return start;
}

#endif
