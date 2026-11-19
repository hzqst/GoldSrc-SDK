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

char* copyModeName( const char* text )
{
#if defined( _WIN32 )
	return _strdup( text );
#else
	return strdup( text );
#endif
}

// A copy owns names of its own, as the destructor frees them.
void copyModeInfo( Dar<char*>& modeInfoDar, const Dar<char*>& otherModeInfoDar )
{
	freeModeInfo( modeInfoDar );
	modeInfoDar = otherModeInfoDar;
	for ( int i = 0; i < modeInfoDar.getCount(); ++i )
	{
		modeInfoDar.setElementAt( copyModeName( modeInfoDar[i] ), i );
	}
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

// Like the original memberwise copy, except for the mode names.
SurfaceBase::SurfaceBase( const SurfaceBase& other )
{
	_needsSwap = other._needsSwap;
	_app = other._app;
	_embeddedPanel = other._embeddedPanel;
	_emulatedCursor = other._emulatedCursor;
	_currentCursor = other._currentCursor;
	copyModeInfo( _modeInfoDar, other._modeInfoDar );
}

SurfaceBase& SurfaceBase::operator=( const SurfaceBase& other )
{
	if ( this == &other )
	{
		return *this;
	}

	_needsSwap = other._needsSwap;
	_app = other._app;
	_embeddedPanel = other._embeddedPanel;
	_emulatedCursor = other._emulatedCursor;
	_currentCursor = other._currentCursor;
	copyModeInfo( _modeInfoDar, other._modeInfoDar );
	return *this;
}

SurfaceBase::~SurfaceBase()
{
	App::getInstance()->surfaceBaseDeleted( this );
#if VGUI_ABI_COMPAT_MODE != 1
	// A root that outlives its surface loses the emulated cursor the surface
	// made, and its tree leaves the surface unless it was moved to another one.
	// A deleted root has already cleared _embeddedPanel. With
	// VGUI_ABI_COMPAT_MODE == 1 the root may have been freed without a
	// destructor, so it is not touched.
	if ( _embeddedPanel != NULL )
	{
		delete _emulatedCursor;
		if ( _embeddedPanel->getSurfaceBase() == this )
			_embeddedPanel->setSurfaceBaseTraverse( NULL );
	}
#endif
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
	_modeInfoDar.addElement( copyModeName( text ) );
}
