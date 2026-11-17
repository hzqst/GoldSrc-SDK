#include "VGUI_Surface.h"

#include "VGUI_Bitmap.h"
#include "VGUI_Cursor.h"
#include "VGUI_Font.h"
#include "VGUI_ImagePanel.h"
#include "VGUI_Panel.h"
#include "vgui_linux.h"

using namespace vgui;

namespace
{

class Texture
{
public:
	int		_id;
	int		_wide;
	int		_tall;
	void*	_dib;
};

int staticTextureCount;
Texture staticTexture[VGUI_TEXTURE_CAPACITY];
Texture* staticTextureCurrent;

}

static Texture* staticGetTextureById( int id )
{
	if ( staticTextureCurrent != NULL && staticTextureCurrent->_id == id )
	{
		return staticTextureCurrent;
	}

	for ( int i = 0; i < staticTextureCount; ++i )
	{
		if ( staticTexture[i]._id == id )
		{
			return &staticTexture[i];
		}
	}
	return NULL;
}

bool Surface::recreateContext()
{
	int wide;
	int tall;
	getPanel()->getSize( wide, tall );
	return true;
}

void Surface::drawPrintText( const char*, int )
{
}

int Surface::createNewTextureID()
{
	const int firstTextureId = 2700;
	static int staticBindIndex = firstTextureId;
	return staticBindIndex++;
}

void Surface::drawSetTexture( int id )
{
	staticTextureCurrent = staticGetTextureById( id );
}

void Surface::drawSetColor( int, int, int, int )
{
}

void Surface::drawFilledRect( int, int, int, int )
{
}

void Surface::drawSetTextColor( int, int, int, int )
{
}

void Surface::drawTexturedRect( int, int, int, int )
{
}

void Surface::pushMakeCurrent( Panel* panel, bool useInsets )
{
	int inset0;
	int inset1;
	int inset2;
	int inset3;
	panel->getInset( inset0, inset1, inset2, inset3 );
	if ( !useInsets )
	{
		inset0 = 0;
		inset1 = 0;
		inset2 = 0;
		inset3 = 0;
	}

	int x0;
	int y0;
	int x1;
	int y1;
	getPanel()->getAbsExtents( x0, y0, x1, y1 );

	int px0;
	int py0;
	int px1;
	int py1;
	panel->getAbsExtents( px0, py0, px1, py1 );

	int cx0;
	int cy0;
	int cx1;
	int cy1;
	panel->getClipRect( cx0, cy0, cx1, cy1 );
}

void Surface::drawSetTextFont( Font* font )
{
	if ( font == NULL )
	{
		return;
	}
	font->getPlat()->drawSetTextFont( _plat );
}

void Surface::setTitle( const char* )
{
}

void Surface::invalidate( Panel* )
{
}

void Surface::popMakeCurrent( Panel* )
{
}

bool Surface::hasFocus()
{
	return true;
}

bool Surface::createPlat()
{
	_plat = new SurfacePlat();
	return true;
}

void Surface::setWindowedMode()
{
}

void Surface::swapBuffers()
{
}

void Surface::applyChanges()
{
}

void Surface::drawSetTextPos( int, int )
{
}

void Surface::setAsTopMost( bool )
{
}

void Surface::enableMouseCapture( bool )
{
}

bool Surface::isWithin( int, int )
{
	return false;
}

void Surface::drawSetTextureRGBA( int id, const char*, int, int )
{
	if ( staticTextureCurrent != NULL && staticTextureCurrent->_id == id )
	{
		return;
	}

	if ( staticTextureCount > 0 )
	{
		for ( int i = 0; i < staticTextureCount; ++i )
		{
			if ( staticTexture[i]._id == id )
			{
				return;
			}
		}
		if ( staticTextureCount >= VGUI_TEXTURE_CAPACITY )
		{
			return;
		}
	}

	staticTexture[staticTextureCount]._id = id;
	++staticTextureCount;
}

void Surface::setCursor( Cursor* cursor )
{
	_currentCursor = cursor;
	if ( cursor != NULL )
	{
		_emulatedCursor->setImage( cursor->getBitmap() );
		cursor->getDefaultCursor();
	}
	else
	{
		_emulatedCursor->setImage( NULL );
	}
}

void Surface::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	drawFilledRect( x0, y0, x1, y0 + 1 );
	drawFilledRect( x0, y1 - 1, x1, y1 );
	drawFilledRect( x0, y0 + 1, x0 + 1, y1 - 1 );
	drawFilledRect( x1 - 1, y0 + 1, x1, y1 - 1 );
}

int Surface::getModeInfoCount()
{
	resetModeInfo();
	addModeInfo( 640, 480, 16 );
	addModeInfo( 800, 600, 16 );
	return SurfaceBase::getModeInfoCount();
}

bool Surface::setFullscreenMode( int wide, int tall, int bpp )
{
	if ( _plat->isFullscreen
		&& _plat->fullscreenInfo[0] == wide
		&& _plat->fullscreenInfo[1] == tall
		&& _plat->fullscreenInfo[2] == bpp )
	{
		return false;
	}

	if ( SurfaceBase::getModeInfoCount() != 0 )
	{
		return false;
	}

	getModeInfoCount();
	return false;
}
