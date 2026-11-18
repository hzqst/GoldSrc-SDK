#include "VGUI_Cursor.h"

#include "VGUI_Bitmap.h"

using namespace vgui;

#if VGUI_ABI_COMPAT_MODE != 1
Cursor::~Cursor()
{
	if ( _dc != dc_user )
		delete _bitmap;
}

void* Cursor::operator new( size_t size )
{
	return ::operator new( size );
}

void Cursor::operator delete( void* memory )
{
	::operator delete( memory );
}
#endif

void Cursor::privateInit( Bitmap* bitmap, int hotspotX, int hotspotY )
{
	_bitmap = bitmap;
	_hotspot[0] = hotspotX;
	_hotspot[1] = hotspotY;
}

Bitmap* Cursor::getBitmap()
{
	return _bitmap;
}

Cursor::DefaultCursor Cursor::getDefaultCursor()
{
	return _dc;
}

void Cursor::getHotspot( int& x, int& y )
{
	x = _hotspot[0];
	y = _hotspot[1];
}

Cursor::Cursor( Bitmap* bitmap, int hotspotX, int hotspotY )
{
	_dc = dc_user;
	privateInit( bitmap, hotspotX, hotspotY );
}
