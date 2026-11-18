#include "VGUI_Cursor.h"

#include "VGUI_Bitmap.h"

namespace
{

class CursorBitmap : public vgui::Bitmap
{
public:
	CursorBitmap( vgui::Cursor::DefaultCursor cursor )
	{
		const int size = 16;
		const int center = size / 2;
		_hotspot[0] = center;
		_hotspot[1] = center;

		if ( cursor == vgui::Cursor::dc_none )
			return;

		setSize( size, size );
		for ( int y = 0; y < size; ++y )
		{
			for ( int x = 0; x < size; ++x )
				setRGBA( x, y, 0, 0, 0, 0 );
		}

		for ( int x = 0; x < size; ++x )
			setRGBA( x, center, 0, 255, 0, 255 );

		for ( int y = 0; y < size; ++y )
			setRGBA( center, y, 0, 0, 255, 255 );
	}

	virtual void getHotspot( int& x, int& y )
	{
		x = _hotspot[0];
		y = _hotspot[1];
	}

private:
	int	_hotspot[2];
};

}

vgui::Cursor::Cursor( DefaultCursor dc )
{
	_dc = dc;
	CursorBitmap* bitmap = new CursorBitmap( dc );
	int x = 0, y = 0;
	bitmap->getHotspot( x, y );
	privateInit( bitmap, x, y );
}

#if VGUI_ABI_COMPAT_MODE != 1
vgui::Cursor::Cursor( const Cursor& other )
{
	_dc = dc_user;
	_bitmap = NULL;
	*this = other;
}

vgui::Cursor& vgui::Cursor::operator=( const Cursor& other )
{
	if ( this == &other )
		return *this;

	if ( _dc != dc_user )
		delete _bitmap;

	_dc = other._dc;
	_bitmap = _dc != dc_user ? new CursorBitmap( _dc ) : other._bitmap;
	_hotspot[0] = other._hotspot[0];
	_hotspot[1] = other._hotspot[1];

	return *this;
}
#endif
