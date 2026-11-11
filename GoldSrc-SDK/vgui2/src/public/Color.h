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

class Color
{
public:
	Color() : _color{}
	{
	}

	Color( int r, int g, int b, int a )
	{
		SetColor( r, g, b, a );
	}

	void SetColor( int r, int g, int b, int a )
	{
		_color[0] = (unsigned char)r;
		_color[1] = (unsigned char)g;
		_color[2] = (unsigned char)b;
		_color[3] = (unsigned char)a;
	}

	unsigned char& operator[]( int index )
	{
		return _color[index];
	}

	const unsigned char& operator[]( int index ) const
	{
		return _color[index];
	}

	Color& operator=( const Color& rhs )
	{
		*((int*)this) = *((int*)&rhs);
		return *this;
	}

private:
	unsigned char	_color[4];
};

#endif // COLOR_H
