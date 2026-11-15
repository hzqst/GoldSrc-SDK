//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Menu.h"

class CServerContextMenu;

namespace vgui2
{
class Panel;
} // namespace vgui2

//-----------------------------------------------------------------------------
// Purpose: Basic right-click context menu for servers
//-----------------------------------------------------------------------------
class CServerContextMenu : public vgui2::Menu
{
public:
	CServerContextMenu( vgui2::Panel *parent );
	virtual ~CServerContextMenu();

	// call this to activate the menu
	void ShowMenu( vgui2::Panel *target, unsigned int serverID, bool showConnect, bool showViewGameInfo, bool showRefresh, bool showAddToFavorites );
};
