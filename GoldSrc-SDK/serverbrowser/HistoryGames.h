//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for servers the user has played on recently
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
// Purpose: History of servers the user has played on
//-----------------------------------------------------------------------------
class CHistoryGames : public CBaseGamesPage
{
public:
	DECLARE_CLASS_SIMPLE( CHistoryGames, CBaseGamesPage );

	CHistoryGames( vgui2::Panel *parent );
	virtual ~CHistoryGames();

	// updates the empty list text and refreshes if requested
	void LoadHistoryList();

	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem( IGameList::InterfaceItem_e item );

	// ISteamMatchmakingServerListResponse callbacks
	virtual void RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response );

	void SetRefreshOnReload();

private:
	// context menu message handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );
	MESSAGE_FUNC( OnRemoveFromHistory, "RemoveFromHistory" );

	bool m_bRefreshOnListReload;
};
