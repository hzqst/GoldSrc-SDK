//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for games running on the local network
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "BaseGamesPage.h"
#include "igamelist.h"

#include "../external/steamworks/isteammatchmaking.h"
#include "../external/steamworks/matchmakingtypes.h"
#include "vgui/MessageMap.h"

class netadr_t;

namespace vgui2
{
class Panel;
} // namespace vgui2

//-----------------------------------------------------------------------------
// Purpose: Games list for servers on the local network
//-----------------------------------------------------------------------------
class CLanGames : public CBaseGamesPage
{
public:
	DECLARE_CLASS_SIMPLE( CLanGames, CBaseGamesPage );

	CLanGames( vgui2::Panel *parent, bool bAutoRefresh, const char *pCustomResFilename );
	virtual ~CLanGames();

	// property page handlers
	virtual void OnPageShow();

	// IGameList handlers
	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem( IGameList::InterfaceItem_e item );

	// Control which button are visible.
	void ManualShowButtons( bool bShowConnect, bool bShowRefreshAll, bool bShowFilter );

	void InternalGetNewServerList( CUtlVector<netadr_t> * );

	// starts the servers refreshing
	virtual void StartRefresh();

	// stops current refresh/GetNewServerList()
	virtual void StopRefresh();

	// ISteamMatchmakingServerListResponse callbacks
	virtual void ServerFailedToRespond( HServerListRequest hRequest, int iServer );
	virtual void RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response );

	// sets the text shown when the list is empty
	virtual void SetEmptyListText();

private:
	// vgui message handlers
	virtual void OnTick();

	// lan timeout checking
	virtual void CheckRetryRequest();

	// context menu message handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );

	// number of servers refreshed
	int m_iServerRefreshCount;

	// true if we're broadcasting for servers
	bool m_bRequesting;

	// time at which we last broadcasted
	double m_fRequestTime;

	bool m_bAutoRefresh;
};
