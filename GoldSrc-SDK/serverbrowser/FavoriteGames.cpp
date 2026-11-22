//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for the user's favorite servers
//
// $NoKeywords: $
//=============================================================================

#include "FavoriteGames.h"
#include "DialogAddServer.h"
#include "ServerBrowserDialog.h"
#include "ServerContextMenu.h"

#include "vgui/KeyValues.h"
#include "Button.h"

#include <time.h>

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFavoriteGames::CFavoriteGames( vgui2::Panel *parent )
	: CBaseGamesPage( parent, "FavoriteGames", eFavoritesServer, NULL )
{
	m_bRefreshOnListReload = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFavoriteGames::~CFavoriteGames()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CFavoriteGames::SupportsItem( IGameList::InterfaceItem_e item )
{
	switch ( item )
	{
	case IGameList::FILTERS:
	case IGameList::ADDSERVER:
	case IGameList::ADDCURRENTSERVER:
		return true;

	default:
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the server list request has finished
//-----------------------------------------------------------------------------
void CFavoriteGames::RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response )
{
	SetRefreshing( false );

	if ( SteamBridgeMatchmaking()->GetFavoriteGameCount() == 0 )
	{
		// no favorite servers
		m_pGameList->SetEmptyListText( "#ServerBrowser_NoFavoriteServers" );
	}
	else
	{
		// favorites exist but none of them answered
		m_pGameList->SetEmptyListText( "#ServerBrowser_NoInternetGamesResponded" );
	}

	m_pGameList->SortList();
}

//-----------------------------------------------------------------------------
// Purpose: Updates the empty list text and refreshes if a reload was requested
//-----------------------------------------------------------------------------
void CFavoriteGames::LoadFavoritesList()
{
	if ( SteamBridgeMatchmaking()->GetFavoriteGameCount() == 0 )
	{
		m_pGameList->SetEmptyListText( "#ServerBrowser_NoFavoriteServers" );
	}
	else
	{
		m_pGameList->SetEmptyListText( "#ServerBrowser_NoInternetGamesResponded" );
	}

	if ( m_bRefreshOnListReload )
	{
		m_bRefreshOnListReload = false;
		StartRefresh();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds the server the user is currently connected to as a favorite
//-----------------------------------------------------------------------------
void CFavoriteGames::OnAddCurrentServer()
{
	gameserveritem_t *pConnected = ServerBrowserDialog().GetCurrentConnectedServer();
	if ( pConnected )
	{
		SteamBridgeMatchmaking()->AddFavoriteGame( pConnected->m_nAppID,
			IPHostToNetwork( pConnected->m_NetAdr.GetIP() ),
			pConnected->m_NetAdr.GetConnectionPort(),
			pConnected->m_NetAdr.GetQueryPort(),
			k_unFavoriteFlagFavorite,
			(uint32)time( NULL ) );

		m_bRefreshOnListReload = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Opens the dialog used to add a server by its address
//-----------------------------------------------------------------------------
void CFavoriteGames::OnAddServerByName()
{
	CDialogAddServer *pDlg = new CDialogAddServer( &ServerBrowserDialog(), this );
	pDlg->vgui2::Frame::MoveToCenterOfScreen();
	pDlg->DoModal();
}

//-----------------------------------------------------------------------------
// Purpose: Handles the add-server button commands
//-----------------------------------------------------------------------------
void CFavoriteGames::OnCommand( const char *command )
{
	if ( !stricmp( command, "AddServerByName" ) )
	{
		OnAddServerByName();
	}
	else if ( !stricmp( command, "AddCurrentServer" ) )
	{
		OnAddCurrentServer();
	}
	else
	{
		CBaseGamesPage::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Enables the "add current server" button while connected
//-----------------------------------------------------------------------------
void CFavoriteGames::OnConnectToGame()
{
	m_pAddCurrentServer->SetEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: Disables the "add current server" button after disconnecting
//-----------------------------------------------------------------------------
void CFavoriteGames::OnDisconnectFromGame()
{
	m_pAddCurrentServer->SetEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the context menu, with remove/add entries for favorites
//-----------------------------------------------------------------------------
void CFavoriteGames::OnOpenContextMenu( int itemID )
{
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu( m_pGameList );

	if ( m_pGameList->GetSelectedItemsCount() )
	{
		int iItemID = m_pGameList->GetSelectedItem( 0 );
		unsigned int iServerID = m_pGameList->GetItemUserData( iItemID );

		menu->ShowMenu( this, iServerID, true, true, true, false );
		menu->AddMenuItem( "RemoveServer", "#ServerBrowser_RemoveServerFromFavorites", new KeyValues( "RemoveFromFavorites" ), this, NULL );
	}
	else
	{
		// no server selected
		menu->ShowMenu( this, (unsigned int)-1, false, false, false, false );
	}

	menu->AddMenuItem( "AddServerByName", "#ServerBrowser_AddServerByIP", new KeyValues( "AddServerByName" ), this, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Removes the selected servers from the favorites list
//-----------------------------------------------------------------------------
void CFavoriteGames::OnRemoveFromFavorites()
{
	for ( int i = 0; i < m_pGameList->GetSelectedItemsCount(); i++ )
	{
		int iItemID = m_pGameList->GetSelectedItem( i );
		int iServerID = m_pGameList->GetItemUserData( iItemID );

		gameserveritem_t *pServer = SteamBridgeMatchmakingServers()->GetServerDetails( m_hServerRequest, iServerID );
		if ( pServer )
		{
			SteamBridgeMatchmaking()->RemoveFavoriteGame( pServer->m_nAppID,
				pServer->m_NetAdr.GetIP(),
				pServer->m_NetAdr.GetConnectionPort(),
				pServer->m_NetAdr.GetQueryPort(),
				k_unFavoriteFlagFavorite );
		}
	}

	CBaseGamesPage::UpdateStatus();
	InvalidateLayout( false, false );
	Repaint();
}
