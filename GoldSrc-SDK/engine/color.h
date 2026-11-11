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

#pragma once

#include <cstring>

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

	Color( int red, int green, int blue );

	constexpr Color( int red, int green, int blue, int alpha )
		: _color{ (unsigned char)red, (unsigned char)green, (unsigned char)blue, (unsigned char)alpha }
	{
	}

	void SetColor( int red, int green, int blue, int alpha )
	{
		_color[0] = (unsigned char)red;
		_color[1] = (unsigned char)green;
		_color[2] = (unsigned char)blue;
		_color[3] = (unsigned char)alpha;
	}

	void GetColor( int& red, int& green, int& blue, int& alpha ) const;

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

	bool operator==( const Color& other ) const
	{
		return _color[0] == other._color[0] && _color[1] == other._color[1] && _color[2] == other._color[2]
			&& _color[3] == other._color[3];
	}

	bool operator!=( Color& other ) const
	{
		return !( *this == other );
	}

private:
	unsigned char _color[4];
};
