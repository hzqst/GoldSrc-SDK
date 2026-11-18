#include "VGUI_Bitmap.h"

#include "VGUI.h"
#include "VGUI_Color.h"
#include "VGUI_Panel.h"
#include "VGUI_SurfaceBase.h"

#include <limits.h>
#include <string.h>

namespace vgui
{

Bitmap::Bitmap()
{
	_id = 0;
	_uploaded = false;
	_rgba = NULL;
}

#if VGUI_ABI_COMPAT_MODE != 1
Bitmap::Bitmap( const Bitmap& other )
	: Image( other )
{
	_id = 0;
	_uploaded = false;
	_rgba = NULL;
	*this = other;
}

Bitmap::~Bitmap()
{
	delete[] _rgba;
}

// A bitmap keeps its own texture and uploads the copied pixels to it.
Bitmap& Bitmap::operator=( const Bitmap& other )
{
	if ( this == &other )
		return *this;

	Image::operator=( other );

	delete[] _rgba;
	_rgba = NULL;
	_uploaded = false;

	if ( other._rgba != NULL )
	{
		int wide;
		int tall;
		getSize( wide, tall );

		_rgba = new uchar[VGUI_RGBA_BYTES * wide * tall];
		memcpy( _rgba, other._rgba, VGUI_RGBA_BYTES * wide * tall );
	}

	return *this;
}
#endif

void Bitmap::setSize( int wide, int tall )
{
	_uploaded = false;

	if ( _rgba != NULL )
	{
		delete[] _rgba;
		_rgba = NULL;
	}

	if ( wide < 0 || tall < 0 || ( wide != 0 && tall > INT_MAX / VGUI_RGBA_BYTES / wide ) )
	{
		Image::setSize( 0, 0 );
		return;
	}

	_rgba = new uchar[VGUI_RGBA_BYTES * wide * tall];
	if ( _rgba == NULL )
	{
		Image::setSize( 0, 0 );
		return;
	}

	Image::setSize( wide, tall );
}

void Bitmap::setRGBA( int x, int y, uchar r, uchar g, uchar b, uchar a )
{
	if ( _rgba == NULL )
		return;

	int wide;
	int tall;
	getSize( wide, tall );

	if ( x >= 0 && y >= 0 && x < wide && y < tall )
	{
		int offset = VGUI_RGBA_BYTES * y * wide + VGUI_RGBA_BYTES * x;
		_rgba[offset] = r;
		_rgba[offset + 1] = g;
		_rgba[offset + 2] = b;
		_rgba[offset + 3] = a;
		_uploaded = false;
	}
}

void Bitmap::paint( Panel* panel )
{
	if ( _rgba == NULL )
		return;

	int wide;
	int tall;
	getSize( wide, tall );

	if ( _id == 0 )
	{
		if ( panel->getSurfaceBase() == NULL )
			return;

		_id = panel->getSurfaceBase()->createNewTextureID();
	}

	if ( !_uploaded )
	{
		drawSetTextureRGBA( _id, (const char*)_rgba, wide, tall );
		_uploaded = true;
	}

	Color color;
	getColor( color );

	int r;
	int g;
	int b;
	int a;
	color.getColor( r, g, b, a );

	drawSetTexture( _id );
	drawSetColor( r, g, b, a );

	int x;
	int y;
	getPos( x, y );
	drawTexturedRect( x, y, x + wide, y + tall );
}

}
