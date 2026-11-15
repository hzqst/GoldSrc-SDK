//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ServerContextMenu.h"

#include "vgui/KeyValues.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerContextMenu::CServerContextMenu( vgui2::Panel *parent ) : vgui2::Menu( parent, "ServerContextMenu" )
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerContextMenu::~CServerContextMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the menu at the current cursor position
//-----------------------------------------------------------------------------
void CServerContextMenu::ShowMenu( vgui2::Panel *target, unsigned int serverID, bool showConnect, bool showViewGameInfo, bool showRefresh, bool showAddToFavorites )
{
	if ( showConnect )
	{
		AddMenuItem( "ConnectToServer", "#ServerBrowser_ConnectToServer", new KeyValues( "ConnectToServer", "serverID", serverID ), target, NULL );
	}

	if ( showViewGameInfo )
	{
		AddMenuItem( "ViewGameInfo", "#ServerBrowser_ViewServerInfo", new KeyValues( "ViewGameInfo", "serverID", serverID ), target, NULL );
	}

	if ( showRefresh )
	{
		AddMenuItem( "RefreshServer", "#ServerBrowser_RefreshServer", new KeyValues( "RefreshServer", "serverID", serverID ), target, NULL );
	}

	if ( showAddToFavorites )
	{
		AddMenuItem( "AddToFavorites", "#ServerBrowser_AddServerToFavorites", new KeyValues( "AddToFavorites", "serverID", serverID ), target, NULL );
	}

	int x, y, gx, gy;
	vgui2::input()->GetCursorPos( x, y );
	vgui2::ipanel()->GetPos( vgui2::surface()->GetEmbeddedPanel(), gx, gy );
	SetPos( x - gx, y - gy );
	SetVisible( true );
}
