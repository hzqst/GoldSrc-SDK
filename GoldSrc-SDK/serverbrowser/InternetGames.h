//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for games on the Internet master server list
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
// Purpose: Internet games list
//-----------------------------------------------------------------------------
class CInternetGames : public CBaseGamesPage
{
public:
	DECLARE_CLASS_SIMPLE( CInternetGames, CBaseGamesPage );

	CInternetGames( vgui2::Panel *parent, const char *panelName, EMatchMakingType eType );
	virtual ~CInternetGames();

	// property page handlers
	virtual void OnPageShow();

	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem( IGameList::InterfaceItem_e item );

	// gets a new server list
	MESSAGE_FUNC( GetNewServerList, "GetNewServerList" );

	// ISteamMatchmakingServerListResponse callbacks
	virtual void ServerResponded( HServerListRequest hRequest, int iServer );
	virtual void ServerFailedToRespond( HServerListRequest hRequest, int iServer );
	virtual void RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response );

	MESSAGE_FUNC_INT( OnRefreshServer, "RefreshServer", serverID );

protected:
	// vgui overrides
	virtual void PerformLayout();
	virtual void OnTick();

private:
	// re-sorts the list if it is dirty
	void CheckRedoSort();

	// context menu message handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );

	// time the list was last sorted
	float m_fLastSort;

	// true if the list needs re-sorting
	bool m_bDirty;

	// true if the server list must be fetched on the next layout
	bool m_bRequireUpdate;

	bool m_bAnyServersRetrievedFromMaster;
	bool m_bAnyServersRespondedToQuery;
	bool m_bNoServersListedOnMaster;

	// true if steam is running in offline mode
	bool m_bOfflineMode;
};
