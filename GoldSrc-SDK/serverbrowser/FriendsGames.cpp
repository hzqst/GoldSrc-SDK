//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for servers the user's friends are playing on
//
// $NoKeywords: $
//=============================================================================

#include "FriendsGames.h"
#include "ServerBrowserDialog.h"
#include "ServerContextMenu.h"

#include "vgui/ISystem.h"
#include "Button.h"
#include "vgui_controls/Controls.h"
#include "ToggleButton.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFriendsGames::CFriendsGames( vgui2::Panel *parent )
	: CBaseGamesPage( parent, "FriendsGames", eFriendsServer, NULL )
{
	m_iServerRefreshCount = 0;

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
CFriendsGames::~CFriendsGames()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CFriendsGames::SupportsItem( IGameList::InterfaceItem_e item )
{
	return item == IGameList::FILTERS;
}

//-----------------------------------------------------------------------------
// Purpose: Opens the context menu for the currently selected server
//-----------------------------------------------------------------------------
void CFriendsGames::OnOpenContextMenu( int itemID )
{
	if ( m_pGameList->GetSelectedItemsCount() )
	{
		int iItemID = m_pGameList->GetSelectedItem( 0 );
		unsigned int iServerID = m_pGameList->GetItemData( iItemID )->userData;

		CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu( m_pGameList );
		menu->ShowMenu( this, iServerID, true, true, true, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the server list request has finished
//-----------------------------------------------------------------------------
void CFriendsGames::RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response )
{
	SetRefreshing( false );
	m_pGameList->SortList();
	m_iServerRefreshCount = 0;

	// keep the offline-mode text set in the constructor while steam is offline
	int iOfflineAFS = 0;
	vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\OfflineAFS", iOfflineAFS );
	if ( iOfflineAFS != 1 )
	{
		int iOffline = 0;
		vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Offline", iOffline );
		if ( iOffline == 1 )
			return;
	}

	m_pGameList->SetEmptyListText( "#ServerBrowser_NoFriendsServers" );
}
