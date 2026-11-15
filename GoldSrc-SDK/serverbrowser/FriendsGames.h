//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for servers the user's friends are playing on
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "BaseGamesPage.h"
#include "igamelist.h"

#include "../external/steamworks/isteammatchmaking.h"
#include "../external/steamworks/matchmakingtypes.h"
#include "vgui/MessageMap.h"

namespace vgui2
{
class Panel;
} // namespace vgui2

//-----------------------------------------------------------------------------
// Purpose: Friends games list
//-----------------------------------------------------------------------------
class CFriendsGames : public CBaseGamesPage
{
public:
	DECLARE_CLASS_SIMPLE( CFriendsGames, CBaseGamesPage );

	CFriendsGames( vgui2::Panel *parent );
	virtual ~CFriendsGames();

	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem( IGameList::InterfaceItem_e item );

	// ISteamMatchmakingServerListResponse callbacks
	virtual void RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response );

private:
	// context menu message handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );

	// number of servers refreshed
	int m_iServerRefreshCount;
};
