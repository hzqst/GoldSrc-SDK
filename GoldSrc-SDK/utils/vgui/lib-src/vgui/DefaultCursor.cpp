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
