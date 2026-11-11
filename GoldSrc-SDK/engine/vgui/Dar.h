//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Holds the enumerated list of default cursors
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include <cstdlib>
#include <cstring>

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Simple lightweight dynamic array implementation
//-----------------------------------------------------------------------------
template <class T> class Dar
{
public:
	Dar() : _count( 0 ), _capacity( 0 ), _data( NULL )
	{
	}

	Dar( int initialCapacity );

	~Dar()
	{
		delete[] _data;
	}

	void EnsureCapacity( int wantedCapacity )
	{
		if ( wantedCapacity <= _capacity )
			return;

		//double capacity until it is >= wantedCapacity
		//this could be done with math, but iterative is just so much more fun
		int newCapacity = _capacity;
		if ( !newCapacity )
			newCapacity = 1;

		while ( newCapacity < wantedCapacity )
			newCapacity *= 2;

		T* newData = new T[newCapacity];
		if ( !newData )
			std::exit( 0 );

		std::memset( newData, 0, sizeof( T ) * newCapacity );
		_capacity = newCapacity;

		//copy data into newData
		for ( int i = 0; i < _count; ++i )
		{
#ifdef _MSC_VER
#pragma warning( suppress: 6386 )
#endif
			newData[i] = _data[i];
		}

		delete[] _data;
		_data = newData;
	}

	void SetCount( int count );

	int GetCount()
	{
		return _count;
	}

	int AddElement( T element )
	{
		EnsureCapacity( _count + 1 );
		_data[_count] = element;
		return _count++;
	}

	void MoveElementToEnd( T element )
	{
		// quick check to see if it's already at the end
		if ( _data[_count - 1] == element )
			return;

		// search backwards in the array looking for the element (since it's probably close to the end already)
		for ( int i = _count - 2; i >= 0; --i )
		{
			if ( _data[i] == element )
			{
				std::memmove( &_data[i], &_data[i + 1], sizeof( T ) * ( _count - i - 1 ) );
				_data[_count - 1] = element;
				return;
			}
		}
	}

	int FindElement( T element )
	{
		for ( int i = 0; i < _count; ++i )
		{
			if ( _data[i] == element )
				return i;
		}

		return -1;
	}

	bool HasElement( T element )
	{
		return FindElement( element ) != -1;
	}

	int PutElement( T element )
	{
		int index = FindElement( element );
		return index != -1 ? index : AddElement( element );
	}

	// insert element at index and move all the others down 1
	void InsertElementAt( T element, int index )
	{
		if ( index < 0 || index > _count )
			return;

		AddElement( element );

		for ( int i = _count - 1; i > index; --i )
			_data[i] = _data[i - 1];

		_data[index] = element;
	}

	void SetElementAt( T element, int index );

	void RemoveElementAt( int index )
	{
		if ( index < 0 || index >= _count )
			return;

		for ( int i = index; i < _count - 1; ++i )
			_data[i] = _data[i + 1];

		--_count;
	}

	void RemoveElementsBefore( int index );

	void RemoveElement( T element )
	{
		RemoveElementAt( FindElement( element ) );
	}

	void RemoveAll()
	{
		_count = 0;
	}

	T operator[]( int index )
	{
		if ( index < 0 || index >= _count )
			return 0;

		return _data[index];
	}

	void* GetBaseData();
	void CopyFrom( Dar<T>& other );

protected:
	int	_count;
	int	_capacity;
	T*	_data;
};
}
