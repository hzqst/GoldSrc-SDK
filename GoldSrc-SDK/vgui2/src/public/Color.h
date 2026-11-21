//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef COLOR_H
#define COLOR_H

#ifdef _WIN32
#pragma once
#endif

#include <string.h>

//-----------------------------------------------------------------------------
// Purpose: Basic handler for an rgb set of colors
//			This class is fully inline
//-----------------------------------------------------------------------------
class Color
{
public:
	constexpr Color() : _color{}
	{
	}

	Color( int r, int g, int b );

	constexpr Color( int r, int g, int b, int a )
		: _color{ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a }
	{
	}

	void SetColor( int r, int g, int b, int a )
	{
		_color[0] = (unsigned char)r;
		_color[1] = (unsigned char)g;
		_color[2] = (unsigned char)b;
		_color[3] = (unsigned char)a;
	}

	void GetColor( int& r, int& g, int& b, int& a ) const;

	void SetRawColor( int color )
	{
		memcpy( _color, &color, sizeof( _color ) );
	}

	int GetRawColor() const
	{
		int color;
		memcpy( &color, _color, sizeof( color ) );
		return color;
	}

	int r() const { return _color[0]; }
	int g() const { return _color[1]; }
	int b() const { return _color[2]; }
	int a() const { return _color[3]; }

	unsigned char& operator[]( int index )
	{
		return _color[index];
	}

	const unsigned char& operator[]( int index ) const
	{
		return _color[index];
	}

	bool operator==( const Color& rhs ) const
	{
		return _color[0] == rhs._color[0] && _color[1] == rhs._color[1] && _color[2] == rhs._color[2]
			&& _color[3] == rhs._color[3];
	}

	bool operator!=( const Color& rhs ) const
	{
		return !( *this == rhs );
	}

private:
	unsigned char	_color[4];
};

#endif // COLOR_H
