#ifndef VGUI_DAR_H
#define VGUI_DAR_H

#include "VGUI.h"

#include <stdlib.h>
#include <string.h>

namespace vgui
{

template <class ELEMTYPE>
class Dar
{
public:
	Dar()
	{
		_count = 0;
		_capacity = 0;
		_data = NULL;
		ensureCapacity( 4 );
	}

	Dar( int initialCapacity )
	{
		_count = 0;
		_capacity = 0;
		_data = NULL;
		ensureCapacity( initialCapacity );
	}

	// Copies get their own array, so destroying or growing one copy cannot
	// leave the other pointing at freed memory.
	Dar( const Dar& other )
	{
		_count = 0;
		_capacity = 0;
		_data = NULL;
		copyFrom( other );
	}

	Dar& operator=( const Dar& other )
	{
		if ( this != &other )
		{
			_count = 0;
			copyFrom( other );
		}

		return *this;
	}

	~Dar()
	{
		delete[] _data;
		_data = NULL;
		_count = 0;
		_capacity = 0;
	}

	void ensureCapacity( int wantedCapacity )
	{
		if ( wantedCapacity <= _capacity )
		{
			return;
		}

		int newCapacity = _capacity;
		if ( newCapacity == 0 )
		{
			newCapacity = 1;
		}
		while ( newCapacity < wantedCapacity )
		{
			newCapacity *= 2;
		}

		ELEMTYPE* newData = new ELEMTYPE[newCapacity];
		if ( newData == NULL )
		{
			exit( 0 );
		}

		memset( newData, 0, sizeof( ELEMTYPE ) * newCapacity );
		_capacity = newCapacity;

		for ( int i = 0; i < _count; ++i )
		{
			newData[i] = _data[i];
		}

		delete[] _data;
		_data = newData;
	}

	void setCount( int count )
	{
		if ( count < 0 || count > _capacity )
		{
			return;
		}

		_count = count;
	}

	int getCount()
	{
		return _count;
	}

	void addElement( ELEMTYPE elem )
	{
		ensureCapacity( _count + 1 );
		_data[_count] = elem;
		_count++;
	}

	bool hasElement( ELEMTYPE elem )
	{
		for ( int i = 0; i < _count; ++i )
		{
			if ( _data[i] == elem )
			{
				return true;
			}
		}

		return false;
	}

	void putElement( ELEMTYPE elem )
	{
		if ( hasElement( elem ) )
		{
			return;
		}

		addElement( elem );
	}

	void insertElementAt( ELEMTYPE elem, int index )
	{
		if ( index < 0 || index > _count )
		{
			return;
		}

		if ( index == _count || _count == 0 )
		{
			addElement( elem );
		}
		else
		{
			addElement( elem );
			for ( int i = _count - 1; i > index; --i )
			{
				_data[i] = _data[i - 1];
			}

			_data[index] = elem;
		}
	}

	void setElementAt( ELEMTYPE elem, int index )
	{
		if ( index < 0 || index >= _count )
		{
			return;
		}

		_data[index] = elem;
	}

	void removeElementAt( int index )
	{
		if ( index < 0 || index >= _count )
		{
			return;
		}

		for ( int i = index; i < _count - 1; ++i )
		{
			_data[i] = _data[i + 1];
		}

		_count--;
	}

	void removeElement( ELEMTYPE elem )
	{
		for ( int i = 0; i < _count; ++i )
		{
			if ( _data[i] == elem )
			{
				removeElementAt( i );
				break;
			}
		}
	}

	void removeAll()
	{
		_count = 0;
	}

	ELEMTYPE operator[]( int index )
	{
		if ( index < 0 || index >= _count )
		{
			return ELEMTYPE();
		}

		return _data[index];
	}

protected:
	void copyFrom( const Dar& other )
	{
		ensureCapacity( other._capacity );
		for ( int i = 0; i < other._count; ++i )
		{
			_data[i] = other._data[i];
		}

		_count = other._count;
	}

	int			_count;
	int			_capacity;
	ELEMTYPE*	_data;
};

#if VGUI_ABI_COMPAT_MODE != 1
// A Dar of pointers that owns what they point to: it deletes them when it is
// destroyed, and a copy starts empty instead of sharing them. It adds no
// members, so a class keeps its layout when a Dar member becomes one.
template <class ELEMTYPE>
class OwningDar : public Dar<ELEMTYPE>
{
public:
	OwningDar() {}
	OwningDar( int initialCapacity ) : Dar<ELEMTYPE>( initialCapacity ) {}
	OwningDar( const OwningDar& ) {}

	OwningDar& operator=( const OwningDar& )
	{
		return *this;
	}

	~OwningDar()
	{
		for ( int i = 0; i < this->_count; ++i )
			delete this->_data[i];
	}
};
#else
// The original vgui.dll frees nothing.
template <class ELEMTYPE>
using OwningDar = Dar<ELEMTYPE>;
#endif

#ifdef _WIN32
template class VGUIAPI Dar<char>;
template class VGUIAPI Dar<char*>;
template class VGUIAPI Dar<int>;
template class VGUIAPI Dar<class Button*>;
template class VGUIAPI Dar<class SurfaceBase*>;
template class VGUIAPI Dar<class InputSignal*>;
template class VGUIAPI Dar<class FocusChangeSignal*>;
template class VGUIAPI Dar<class FrameSignal*>;
template class VGUIAPI Dar<class ActionSignal*>;
template class VGUIAPI Dar<class IntChangeSignal*>;
template class VGUIAPI Dar<class TickSignal*>;
template class VGUIAPI Dar<Dar<char>*>;
template class VGUIAPI Dar<class Frame*>;
template class VGUIAPI Dar<class DesktopIcon*>;
template class VGUIAPI Dar<class ChangeSignal*>;
template class VGUIAPI Dar<class Panel*>;
template class VGUIAPI Dar<class Label*>;
template class VGUIAPI Dar<class RepaintSignal*>;
#endif

}

#endif
