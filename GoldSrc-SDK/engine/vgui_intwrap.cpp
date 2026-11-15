#include "engine_apiproxy.h"
#include "IEngineSurface.h"
#include "VGUI_EngineSurfaceWrap.h"
#include "interface.h"
#include "ivideomode.h"
#include "vgui_int.h"
#include "vgui_intwrap2.h"
#include "../external/SDL2/SDL.h"

#include <VGUI_App.h>
#include <VGUI_Panel.h>
#include <VGUI_Scheme.h>

#undef null

class CDummyApp : public vgui::App
{
public:
	void main( int argc, char* argv[] ) override;

protected:
	void platTick() override;
};

CDummyApp theApp;

static vgui::Panel* staticPanel;
static EngineSurfaceWrap* staticEngineSurface;

SDL_Window** pmainwindow;

extern "C" void SetMouseEnable( qboolean fEnable );

void* VGuiWrap_GetPanel()
{
	g_engdstAddrs.VGui_GetPanel();
	return staticPanel;
}

void CDummyApp::main( int, char*[] )
{
}

void CDummyApp::platTick()
{
}

void VGuiWrap_SetRootPanelSize()
{
	int x = 0, y = 0;
	int wide, tall;

	g_engdstAddrs.VGui_GetPanel();
	if ( !staticPanel )
		return;

	SDL_GetWindowPosition( *pmainwindow, &x, &y );
	if ( VideoMode_IsWindowed() )
		SDL_GetWindowSize( *pmainwindow, &wide, &tall );
	else
		VideoMode_GetCurrentVideoMode( &wide, &tall, NULL );

	staticPanel->setBounds( x, y, wide, tall );
}

void VGuiWrap_Shutdown()
{
	::operator delete( staticPanel );
	staticPanel = NULL;

	delete staticEngineSurface;
	staticEngineSurface = NULL;
}

int VGuiWrap_CallEngineSurfaceAppHandler( void* event, void* userData )
{
	if ( staticEngineSurface )
		staticEngineSurface->AppHandler( event, userData );
	return 0;
}

void VGuiWrap_ReleaseMouse()
{
	if ( !vgui::App::getInstance() || !staticEngineSurface )
		return;

	if ( VGuiWrap2_UseVGUI1() )
	{
		staticEngineSurface->setCursor( vgui::App::getInstance()->getScheme()->getCursor( vgui::Scheme::scu_arrow ) );
		staticEngineSurface->lockCursor();
	}
	ClientDLL_DeactivateMouse();
	SetMouseEnable( FALSE );
}

void VGuiWrap_GetMouse()
{
	if ( !staticEngineSurface )
		return;

	if ( VGuiWrap2_UseVGUI1() )
		staticEngineSurface->unlockCursor();
	ClientDLL_ActivateMouse();
	SetMouseEnable( TRUE );
}

void VGuiWrap_SetVisible( int state )
{
	if ( staticPanel )
		staticPanel->setVisible( state != 0 );
}

void VGuiWrap_Paint( int paintAll )
{
	int extents[4];

	g_engdstAddrs.VGui_GetPanel();
	if ( !staticPanel )
		return;

	VGuiWrap_SetRootPanelSize();
	staticPanel->repaint();
	vgui::App::getInstance()->externalTick();

	if ( paintAll )
	{
		staticPanel->paintTraverse();
	}
	else
	{
		staticPanel->getAbsExtents( extents[0], extents[1], extents[2], extents[3] );
		VGui_ViewportPaintBackground( extents );
	}
}

void VGuiWrap_Startup()
{
	CreateInterfaceFn factory;

	if ( staticEngineSurface )
		return;

	vgui::App::getInstance()->reset();

	staticPanel = new vgui::Panel( 0, 0, 320, 240 );
	staticPanel->setPaintBorderEnabled( false );
	staticPanel->setPaintBackgroundEnabled( false );
	staticPanel->setPaintEnabled( false );
	staticPanel->setCursor( vgui::App::getInstance()->getScheme()->getCursor( vgui::Scheme::scu_none ) );

	factory = Sys_GetFactoryThis();
	staticEngineSurface = new EngineSurfaceWrap( staticPanel, (IEngineSurface*)factory( "EngineSurface007", NULL ) );
	VGuiWrap_SetRootPanelSize();
}
