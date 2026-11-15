//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for the user's favorite servers
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
// Purpose: Favorite games list
//-----------------------------------------------------------------------------
class CFavoriteGames : public CBaseGamesPage
{
public:
	DECLARE_CLASS_SIMPLE( CFavoriteGames, CBaseGamesPage );

	CFavoriteGames( vgui2::Panel *parent );
	virtual ~CFavoriteGames();

	// updates the empty list text and refreshes if requested
	void LoadFavoritesList();

	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem( IGameList::InterfaceItem_e item );

	// ISteamMatchmakingServerListResponse callbacks
	virtual void RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response );

	// passed from main server browser window instead of messages
	void OnConnectToGame();
	void OnDisconnectFromGame();

	void SetRefreshOnReload();

private:
	// context menu message handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );
	MESSAGE_FUNC( OnRemoveFromFavorites, "RemoveFromFavorites" );
	MESSAGE_FUNC( OnAddServerByName, "AddServerByName" );

	void OnAddCurrentServer();
	virtual void OnCommand( const char *command );

	bool m_bRefreshOnListReload;
};
