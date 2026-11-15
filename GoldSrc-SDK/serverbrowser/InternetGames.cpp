//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for games on the Internet master server list
//
// $NoKeywords: $
//=============================================================================

#include "InternetGames.h"
#include "ServerBrowserDialog.h"
#include "ServerContextMenu.h"

#include "vgui/IInput.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "Button.h"
#include "vgui_controls/Controls.h"
#include "ToggleButton.h"

extern CBaseGamesPage *g_CurrentGamesPage;

// minimum time between re-sorting the list while servers are still responding
static const float MINIMUM_SORT_TIME = 1.5f;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CInternetGames::CInternetGames( vgui2::Panel *parent, const char *panelName, EMatchMakingType eType )
	: CBaseGamesPage( parent, panelName, eType, NULL )
{
	m_fLastSort = 0.0f;
	m_bDirty = false;
	m_bRequireUpdate = true;

	// steam offline mode disables the internet list
	bool bOfflineMode = false;
	int iOfflineAFS = 0;
	vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\OfflineAFS", iOfflineAFS );
	if ( iOfflineAFS != 1 )
	{
		int iOffline = 0;
		vgui2::system()->GetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Offline", iOffline );
		bOfflineMode = ( iOffline == 1 );
	}
	m_bOfflineMode = bOfflineMode;

	m_bAnyServersRetrievedFromMaster = false;
	m_bNoServersListedOnMaster = false;
	m_bAnyServersRespondedToQuery = false;

	CBaseGamesPage::LoadFilterSettings();

	vgui2::ivgui()->AddTickSignal( GetVPanel(), 1000 );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CInternetGames::~CInternetGames()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the game list supports the specified ui elements
//-----------------------------------------------------------------------------
bool CInternetGames::SupportsItem( IGameList::InterfaceItem_e item )
{
	switch ( item )
	{
	case IGameList::FILTERS:
	case IGameList::GETNEWLIST:
		return true;

	default:
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Opens the context menu for the currently selected server
//-----------------------------------------------------------------------------
void CInternetGames::OnOpenContextMenu( int itemID )
{
	if ( m_pGameList->GetSelectedItemsCount() == 0 )
		return;

	int iItemID = m_pGameList->GetSelectedItem( 0 );
	unsigned int iServerID = m_pGameList->GetItemData( iItemID )->userData;

	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu( m_pGameList );
	menu->ShowMenu( this, iServerID, true, true, true, true );
}

//-----------------------------------------------------------------------------
// Purpose: Re-sorts the list if it changed and enough time has passed
//-----------------------------------------------------------------------------
void CInternetGames::CheckRedoSort()
{
	if ( !m_bDirty )
		return;

	float flCurrentTime = (float)Plat_FloatTime();
	if ( flCurrentTime - m_fLastSort < MINIMUM_SORT_TIME )
		return;

	// don't sort while the user is clicking in the list; try again in a second
	if ( vgui2::input()->IsMouseDown( vgui2::MOUSE_LEFT ) || vgui2::input()->IsMouseDown( vgui2::MOUSE_RIGHT ) )
	{
		m_fLastSort = flCurrentTime - MINIMUM_SORT_TIME + 1.0f;
		return;
	}

	m_bDirty = false;
	m_fLastSort = flCurrentTime;
	m_pGameList->SortList();
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CInternetGames::OnTick()
{
	if ( m_bOfflineMode )
	{
		vgui2::Panel::OnTick();
		return;
	}

	vgui2::Panel::OnTick();
	CheckRedoSort();
}

//-----------------------------------------------------------------------------
// Purpose: Makes this the active page and fetches the list the first time
//-----------------------------------------------------------------------------
void CInternetGames::OnPageShow()
{
	g_CurrentGamesPage = this;

	if ( m_pGameList->GetItemCount() != 0 )
		return;

	if ( !ServerBrowserDialog().IsVisible() )
		return;

	CBaseGamesPage::OnPageShow();
}

//-----------------------------------------------------------------------------
// Purpose: Requests a fresh server list from the master server
//-----------------------------------------------------------------------------
void CInternetGames::GetNewServerList()
{
	CBaseGamesPage::GetNewServerList();
	CBaseGamesPage::UpdateStatus();

	m_bRequireUpdate = false;
	m_bAnyServersRetrievedFromMaster = false;
	m_bAnyServersRespondedToQuery = false;

	m_pGameList->DeleteAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes a single server
//-----------------------------------------------------------------------------
void CInternetGames::OnRefreshServer( int serverID )
{
	CBaseGamesPage::OnRefreshServer( serverID );
	ServerBrowserDialog().UpdateStatusText( "#ServerBrowser_GettingNewServerList" );
}

//-----------------------------------------------------------------------------
// Purpose: Called when a server has answered the query
//-----------------------------------------------------------------------------
void CInternetGames::ServerResponded( HServerListRequest hRequest, int iServer )
{
	m_bDirty = true;
	CBaseGamesPage::ServerResponded( hRequest, iServer );
	m_bAnyServersRespondedToQuery = true;
	m_bAnyServersRetrievedFromMaster = true;
}

//-----------------------------------------------------------------------------
// Purpose: Called when a server did not answer the query
//-----------------------------------------------------------------------------
void CInternetGames::ServerFailedToRespond( HServerListRequest hRequest, int iServer )
{
	m_bDirty = true;

	gameserveritem_t *pServer = SteamMatchmakingServers()->GetServerDetails( m_hServerRequest, iServer );
	if ( pServer->m_bHadSuccessfulResponse )
	{
		// it answered before, so keep it in the list
		ServerResponded( hRequest, iServer );
	}
	else
	{
		int iIndex = m_mapServers.Find( iServer );
		if ( iIndex != m_mapServers.InvalidIndex() )
		{
			CBaseGamesPage::RemoveServer( m_mapServers.Element( iIndex ) );
		}
		m_iServerRefreshCount++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the server list request has finished
//-----------------------------------------------------------------------------
void CInternetGames::RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response )
{
	if ( m_hServerRequest != hRequest )
		return;

	SetRefreshing( false );
	CBaseGamesPage::UpdateFilterSettings();

	if ( response == eServerFailedToRespond )
	{
		m_pGameList->SetEmptyListText( "#ServerBrowser_MasterServerNotResponsive" );
	}
	else if ( m_bAnyServersRespondedToQuery )
	{
		m_pGameList->SetEmptyListText( "#ServerBrowser_NoInternetGames" );
	}
	else if ( response == eNoServersListedOnMasterServer )
	{
		m_pGameList->SetEmptyListText( "#ServerBrowser_MasterServerHasNoServersListed" );
	}
	else
	{
		m_pGameList->SetEmptyListText( "#ServerBrowser_NoInternetGamesResponded" );
	}

	m_bDirty = false;
	m_fLastSort = (float)Plat_FloatTime();
	if ( IsVisible() )
	{
		m_pGameList->SortList();
	}

	CBaseGamesPage::UpdateStatus();
}

//-----------------------------------------------------------------------------
// Purpose: Requests the server list when needed and disables controls offline
//-----------------------------------------------------------------------------
void CInternetGames::PerformLayout()
{
	if ( !m_bOfflineMode && m_bRequireUpdate && ServerBrowserDialog().IsVisible() )
	{
		PostMessage( this, new KeyValues( "GetNewServerList" ), 0.1f );
		m_bRequireUpdate = false;
	}

	if ( m_bOfflineMode )
	{
		m_pGameList->SetEmptyListText( "#ServerBrowser_OfflineMode" );
		m_pConnect->SetEnabled( false );
		m_pRefreshAll->SetEnabled( false );
		m_pRefreshQuick->SetEnabled( false );
		m_pAddServer->SetEnabled( false );
		m_pFilter->SetEnabled( false );
	}

	CBaseGamesPage::PerformLayout();
}
