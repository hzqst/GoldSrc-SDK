//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for servers the user has played on recently
//
// $NoKeywords: $
//=============================================================================

#include "HistoryGames.h"
#include "ServerBrowserDialog.h"
#include "ServerContextMenu.h"

#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "Button.h"
#include "vgui_controls/Controls.h"
#include "ToggleButton.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHistoryGames::CHistoryGames( vgui2::Panel *parent )
	: CBaseGamesPage( parent, "HistoryGames", eHistoryServer, NULL )
{
	m_bRefreshOnListReload = false;

	// add a column showing when the server was last played on, and sort by it
	m_pGameList->AddColumnHeader( 9, "LastPlayed", "#ServerBrowser_LastPlayed", 100, 0 );
	m_pGameList->SetSortFunc( 9, CBaseGamesPage::LastPlayedCompare );
	m_pGameList->SetSortColumn( 9 );

	// disable the page while steam is in offline mode
	int iOfflineAFS = 0;
	vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\OfflineAFS", iOfflineAFS );
	if ( iOfflineAFS != 1 )
	{
		int iOffline = 0;
		vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Offline", iOffline );
		if ( iOffline == 1 )
		{
			m_pGameList->SetEmptyListText( "#ServerBrowser_OfflineMode" );
			m_pConnect->SetEnabled( false );
			m_pRefreshAll->SetEnabled( false );
			m_pRefreshQuick->SetEnabled( false );
			m_pAddServer->SetEnabled( false );
			m_pFilter->SetEnabled( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CHistoryGames::~CHistoryGames()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CHistoryGames::SupportsItem( IGameList::InterfaceItem_e item )
{
	return item == IGameList::FILTERS;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the server list request has finished
//-----------------------------------------------------------------------------
void CHistoryGames::RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response )
{
	SetRefreshing( false );
	m_pGameList->SetEmptyListText( "#ServerBrowser_NoServersPlayed" );
	m_pGameList->SortList();
}

//-----------------------------------------------------------------------------
// Purpose: Updates the empty list text and refreshes if a reload was requested
//-----------------------------------------------------------------------------
void CHistoryGames::LoadHistoryList()
{
	// keep the offline-mode text set in the constructor while steam is offline
	bool bOfflineMode = false;
	int iOfflineAFS = 0;
	vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\OfflineAFS", iOfflineAFS );
	if ( iOfflineAFS != 1 )
	{
		int iOffline = 0;
		vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Offline", iOffline );
		bOfflineMode = ( iOffline == 1 );
	}

	if ( !bOfflineMode )
	{
		m_pGameList->SetEmptyListText( "#ServerBrowser_NoServersPlayed" );
	}

	if ( m_bRefreshOnListReload )
	{
		m_bRefreshOnListReload = false;
		StartRefresh();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes the list the next time it is reloaded
//-----------------------------------------------------------------------------
void CHistoryGames::SetRefreshOnReload()
{
	m_bRefreshOnListReload = true;
}

//-----------------------------------------------------------------------------
// Purpose: Opens the context menu, with a remove entry for the selected server
//-----------------------------------------------------------------------------
void CHistoryGames::OnOpenContextMenu( int itemID )
{
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu( m_pGameList );

	if ( m_pGameList->GetSelectedItemsCount() )
	{
		int iItemID = m_pGameList->GetSelectedItem( 0 );
		unsigned int iServerID = m_pGameList->GetItemUserData( iItemID );

		menu->ShowMenu( this, iServerID, true, true, true, true );
		menu->AddMenuItem( "RemoveServer", "#ServerBrowser_RemoveServerFromHistory", new KeyValues( "RemoveFromHistory" ), this, NULL );
	}
	else
	{
		// no server selected
		menu->ShowMenu( this, (unsigned int)-1, false, false, false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes the selected servers from the history list
//-----------------------------------------------------------------------------
void CHistoryGames::OnRemoveFromHistory()
{
	for ( int i = m_pGameList->GetSelectedItemsCount() - 1; i >= 0; i-- )
	{
		int iItemID = m_pGameList->GetSelectedItem( i );
		int iServerID = m_pGameList->GetItemData( iItemID )->userData;

		gameserveritem_t *pServer = SteamBridgeMatchmakingServers()->GetServerDetails( m_hServerRequest, iServerID );
		if ( pServer )
		{
			SteamBridgeMatchmaking()->RemoveFavoriteGame( pServer->m_nAppID,
				pServer->m_NetAdr.GetIP(),
				pServer->m_NetAdr.GetConnectionPort(),
				pServer->m_NetAdr.GetQueryPort(),
				k_unFavoriteFlagHistory );
		}
	}

	CBaseGamesPage::UpdateStatus();
	InvalidateLayout( false, false );
	Repaint();
}
