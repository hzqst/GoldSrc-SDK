#include "VGUI_Surface.h"

#include "VGUI_Panel.h"

using namespace vgui;

Surface::Surface( Panel* embeddedPanel ) : SurfaceBase( embeddedPanel )
{
	createPlat();
	recreateContext();
}

// A copy has no window, like the detached SurfaceBase it copies; it can only be
// assigned or destroyed.
Surface::Surface( const Surface& other ) : SurfaceBase( other )
{
	_plat = NULL;
}

// The surface keeps its own window.
Surface& Surface::operator=( const Surface& other )
{
	SurfaceBase::operator=( other );
	return *this;
}

void Surface::createPopup( Panel* embeddedPanel )
{
	embeddedPanel->setParent( NULL );
	new Surface( embeddedPanel );
}

void Surface::GetMousePos( int&, int& )
{
}
