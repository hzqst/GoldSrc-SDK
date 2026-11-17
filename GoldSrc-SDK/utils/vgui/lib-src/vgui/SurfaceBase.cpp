#include "VGUI_SurfaceBase.h"

#include "VGUI_App.h"
#include "VGUI_Cursor.h"
#include "VGUI_Image.h"
#include "VGUI_ImagePanel.h"
#include "VGUI_Panel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace vgui;

namespace
{

class FakeImagePanel : public ImagePanel
{
public:
	FakeImagePanel( Image* image ) : ImagePanel( image )
	{
	}

	virtual bool isWithin( int x, int y ) override;
};

bool FakeImagePanel::isWithin( int, int )
{
	return false;
}

void freeModeInfo( Dar<char*>& modeInfoDar )
{
	for ( int i = 0; i < modeInfoDar.getCount(); ++i )
	{
		free( modeInfoDar[i] );
	}

	modeInfoDar.removeAll();
}

}

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
SurfaceBase::SurfaceBase( Panel* embeddedPanel )
{
	_embeddedPanel = embeddedPanel;
	_needsSwap = true;
	_emulatedCursor = new FakeImagePanel( NULL );
	_emulatedCursor->setVisible( false );
	_currentCursor = NULL;
	_embeddedPanel->setSurfaceBaseTraverse( this );
	App::getInstance()->surfaceBaseCreated( this );
	_emulatedCursor->setParent( _embeddedPanel );
}

SurfaceBase::~SurfaceBase()
{
	App::getInstance()->surfaceBaseDeleted( this );
	freeModeInfo( _modeInfoDar );
}

Panel* SurfaceBase::getPanel()
{
	return _embeddedPanel;
}

void SurfaceBase::requestSwap()
{
	_needsSwap = true;
}

void SurfaceBase::resetModeInfo()
{
	freeModeInfo( _modeInfoDar );
}

int SurfaceBase::getModeInfoCount()
{
	return _modeInfoDar.getCount();
}

bool SurfaceBase::getModeInfo( int index, int& wide, int& tall, int& bpp )
{
	if ( index >= 0 && index < _modeInfoDar.getCount() )
	{
#if defined( _WIN32 )
		sscanf_s( _modeInfoDar[index], "%dx%dx%d", &wide, &tall, &bpp );
#else
		sscanf( _modeInfoDar[index], "%dx%dx%d", &wide, &tall, &bpp );
#endif
		return true;
	}

	return false;
}

App* SurfaceBase::getApp()
{
	return App::getInstance();
}

void SurfaceBase::setEmulatedCursorVisible( bool state )
{
	_emulatedCursor->setVisible( state );
}

void SurfaceBase::setEmulatedCursorPos( int x, int y )
{
	getPanel()->removeChild( _emulatedCursor );
	getPanel()->addChild( _emulatedCursor );
	getPanel()->screenToLocal( x, y );
	if ( _currentCursor != NULL && !_emulatedCursor->isVisible() )
	{
		int hx;
		int hy;

		_currentCursor->getHotspot( hx, hy );
		x -= hx;
		y -= hy;
	}

	_emulatedCursor->setPos( x, y );
}

void SurfaceBase::addModeInfo( int wide, int tall, int bpp )
{
	char text[256];

	snprintf( text, sizeof( text ), "%dx%dx%d", wide, tall, bpp );
#if defined( _WIN32 )
	_modeInfoDar.addElement( _strdup( text ) );
#else
	_modeInfoDar.addElement( strdup( text ) );
#endif
}
