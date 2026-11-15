//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for games running on the local network
//
// $NoKeywords: $
//=============================================================================

#include "LanGames.h"
#include "ServerBrowserDialog.h"
#include "ServerContextMenu.h"

#include "Button.h"
#include "ToggleButton.h"

extern CBaseGamesPage *g_CurrentGamesPage;

// how long to wait for LAN servers to answer a broadcast
static const float BROADCAST_LIST_TIMEOUT = 0.4f;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLanGames::CLanGames( vgui2::Panel *parent, bool bAutoRefresh, const char *pCustomResFilename )
	: CBaseGamesPage( parent, "LanGames", eLANServer, pCustomResFilename )
{
	m_iServerRefreshCount = 0;
	m_bRequesting = false;
	m_bAutoRefresh = bAutoRefresh;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLanGames::~CLanGames()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CLanGames::SupportsItem( IGameList::InterfaceItem_e item )
{
	return item == IGameList::FILTERS;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text shown when the list contains no servers
//-----------------------------------------------------------------------------
void CLanGames::SetEmptyListText()
{
	m_pGameList->SetEmptyListText( "#ServerBrowser_NoLanServers" );
}

//-----------------------------------------------------------------------------
// Purpose: Shows or hides the connect, refresh and filter buttons
//-----------------------------------------------------------------------------
void CLanGames::ManualShowButtons( bool bShowConnect, bool bShowRefreshAll, bool bShowFilter )
{
	m_pConnect->SetVisible( bShowConnect );
	m_pRefreshAll->SetVisible( bShowRefreshAll );
	m_pFilter->SetVisible( bShowFilter );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the context menu for the currently selected server
//-----------------------------------------------------------------------------
void CLanGames::OnOpenContextMenu( int itemID )
{
	if ( m_pGameList->GetSelectedItemsCount() )
	{
		int iItemID = m_pGameList->GetSelectedItem( 0 );
		unsigned int iServerID = m_pGameList->GetItemUserData( iItemID );

		CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu( m_pGameList );
		menu->ShowMenu( this, iServerID, true, true, true, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clears the outstanding request once the broadcast has timed out
//-----------------------------------------------------------------------------
void CLanGames::CheckRetryRequest()
{
	if ( !m_bRequesting )
		return;

	if ( Plat_FloatTime() - m_fRequestTime > BROADCAST_LIST_TIMEOUT )
	{
		m_bRequesting = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CLanGames::OnTick()
{
	vgui2::Panel::OnTick();
	CheckRetryRequest();
}

//-----------------------------------------------------------------------------
// Purpose: Starts the process of refreshing the server list
//-----------------------------------------------------------------------------
void CLanGames::StartRefresh()
{
	CBaseGamesPage::StartRefresh();
	m_fRequestTime = Plat_FloatTime();
}

//-----------------------------------------------------------------------------
// Purpose: Stops the current refresh
//-----------------------------------------------------------------------------
void CLanGames::StopRefresh()
{
	CBaseGamesPage::StopRefresh();
	m_bRequesting = false;
}

//-----------------------------------------------------------------------------
// Purpose: Makes this the active page and refreshes it if auto refresh is set
//-----------------------------------------------------------------------------
void CLanGames::OnPageShow()
{
	g_CurrentGamesPage = this;

	if ( m_bAutoRefresh )
	{
		StartRefresh();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the server list request has finished
//-----------------------------------------------------------------------------
void CLanGames::RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response )
{
	SetRefreshing( false );
	m_pGameList->SortList();
	m_iServerRefreshCount = 0;
	m_pGameList->SetEmptyListText( "#ServerBrowser_NoLanServers" );
	SetEmptyListText();
}

//-----------------------------------------------------------------------------
// Purpose: Removes a server that did not answer the query from the list
//-----------------------------------------------------------------------------
void CLanGames::ServerFailedToRespond( HServerListRequest hRequest, int iServer )
{
	int iIndex = m_mapServers.Find( iServer );
	if ( iIndex != m_mapServers.InvalidIndex() )
	{
		CBaseGamesPage::RemoveServer( m_mapServers.Element( iIndex ) );
	}
}
