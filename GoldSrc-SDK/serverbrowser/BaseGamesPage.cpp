//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Base class for all the server list pages
//
// $NoKeywords: $
//=============================================================================

#include "BaseGamesPage.h"
#include "ModList.h"
#include "ServerBrowser.h"
#include "ServerBrowserDialog.h"

#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "Button.h"
#include "CheckButton.h"
#include "ComboBox.h"
#include "vgui_controls/Controls.h"
#include "ImageList.h"
#include "Label.h"
#include "PropertySheet.h"
#include "TextEntry.h"
#include "ToggleButton.h"

#include "vgui/KeyValues.h"
#include "netadr.h"
#include "../engine/controls.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

// page currently being shown; the static sort functions read from it
CBaseGamesPage *g_CurrentGamesPage;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseGamesPage::CBaseGamesPage( vgui2::Panel *parent, const char *name, EMatchMakingType eType, const char *pCustomResFilename )
	: BaseClass( parent, name, true ),
	m_CallbackFavoritesMsg( this, &CBaseGamesPage::OnFavoritesMsg ),
	m_pCustomResFilename( pCustomResFilename )
{
	int tall = GetProportionalScaledValue( 278 );
	int wide = GetProportionalScaledValue( 624 );
	SetSize( wide, tall );

	m_iPingFilter = 0;
	m_iServerRefreshCount = 0;
	m_iSecureFilter = 0;
	m_szGameFilter[0] = 0;
	m_szMapFilter[0] = 0;
	m_bFilterNoFullServers = false;
	m_bFilterNoEmptyServers = false;
	m_bFilterNoPasswordedServers = false;
	m_bFilterValidSteamAccount = false;
	m_hFont = 0;
	m_hServerRequest = NULL;
	m_eMatchMakingType = eType;

	SetDefLessFunc( m_mapServers );
	SetDefLessFunc( m_mapServerIP );
	SetDefLessFunc( m_mapGamesFilterItem );

	vgui2::localize()->ConvertUnicodeToANSI( vgui2::localize()->Find( "ServerBrowser_All" ), m_szComboAllText, sizeof( m_szComboAllText ) );

	m_pConnect = new vgui2::Button( this, "ConnectButton", "#ServerBrowser_Connect" );
	m_pConnect->SetEnabled( false );
	m_pRefreshAll = new vgui2::Button( this, "RefreshButton", "#ServerBrowser_Refresh" );
	m_pRefreshQuick = new vgui2::Button( this, "RefreshQuickButton", "#ServerBrowser_RefreshQuick" );
	m_pAddServer = new vgui2::Button( this, "AddServerButton", "#ServerBrowser_AddServer" );
	m_pAddCurrentServer = new vgui2::Button( this, "AddCurrentServerButton", "#ServerBrowser_AddCurrentServer" );
	m_pGameList = new CGameListPanel( this, "gamelist" );
	m_pGameList->SetAllowUserModificationOfColumns( true );

	m_pAddToFavoritesButton = new vgui2::Button( this, "AddToFavoritesButton", "" );
	m_pAddToFavoritesButton->SetEnabled( false );
	m_pAddToFavoritesButton->SetVisible( false );

	// Add the column headers
	m_pGameList->AddColumnHeader( 0, "Password", "#ServerBrowser_Password", GetProportionalScaledValue( 16 ), 5 );
	m_pGameList->AddColumnHeader( 1, "Bots", "#ServerBrowser_Bots", GetProportionalScaledValue( 20 ), 0 );
	m_pGameList->AddColumnHeader( 2, "Secure", "#ServerBrowser_Secure", GetProportionalScaledValue( 16 ), 5 );
	m_pGameList->AddColumnHeader( 3, "Players", "#ServerBrowser_Players", GetProportionalScaledValue( 55 ), 0 );
	m_pGameList->AddColumnHeader( 4, "Name", "#ServerBrowser_Servers", GetProportionalScaledValue( 50 ), 18 );
	m_pGameList->AddColumnHeader( 5, "GameDesc", "#ServerBrowser_Game", GetProportionalScaledValue( 112 ), 0 );
	m_pGameList->AddColumnHeader( 6, "Map", "#ServerBrowser_Map", GetProportionalScaledValue( 90 ), 0 );
	m_pGameList->AddColumnHeader( 7, "Ping", "#ServerBrowser_Latency", GetProportionalScaledValue( 55 ), 0 );
	m_pGameList->AddColumnHeader( 8, "IPAddr", "#ServerBrowser_IPAddress", GetProportionalScaledValue( 64 ), 8 );

	m_pGameList->SetColumnHeaderTooltip( 0, "#ServerBrowser_PasswordColumn_Tooltip" );
	m_pGameList->SetColumnHeaderTooltip( 1, "#ServerBrowser_BotColumn_Tooltip" );
	m_pGameList->SetColumnHeaderTooltip( 2, "#ServerBrowser_SecureColumn_Tooltip" );

	// setup fast sort functions
	m_pGameList->SetSortFunc( 0, PasswordCompare );
	m_pGameList->SetSortFunc( 1, BotsCompare );
	m_pGameList->SetSortFunc( 2, SecureCompare );
	m_pGameList->SetSortFunc( 3, PlayersCompare );
	m_pGameList->SetSortFunc( 4, ServerNameCompare );
	m_pGameList->SetSortFunc( 5, GameCompare );
	m_pGameList->SetSortFunc( 6, MapCompare );
	m_pGameList->SetSortFunc( 7, PingCompare );
	m_pGameList->SetSortFunc( 8, IPAddressCompare );

	// Sort by IP address by default
	m_pGameList->SetSortColumn( 8 );

	CreateFilters();
	OnButtonToggled( m_pFilter, 0 );
	LoadFilterSettings();

	char szConfigName[128];
	snprintf( szConfigName, sizeof( szConfigName ), "Servers/Pages/%s", name );
	LoadUserConfig( szConfigName, 0 );

	snprintf( szConfigName, sizeof( szConfigName ), "Servers/Pages/%s/Extended", name );
	KeyValues *kv = vgui2::system()->GetUserConfigFileData( szConfigName, 0 );
	if ( kv->GetInt( "Version", 0 ) <= 0 )
	{
		// first time with this config version: show the bots column, hide the IP column
		m_pGameList->SetColumnVisible( 1, true );
		m_pGameList->SetColumnVisible( 8, false );
		kv->SetInt( "Version", 1 );
	}

	m_bAutoSelectFirstItemInGameList = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBaseGamesPage::~CBaseGamesPage()
{
	if ( m_hServerRequest )
	{
		if ( SteamMatchmakingServers() )
			SteamMatchmakingServers()->ReleaseRequest( m_hServerRequest );
		m_hServerRequest = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of selected servers in the list
//-----------------------------------------------------------------------------
int CBaseGamesPage::GetSelectedItemsCount()
{
	return m_pGameList->GetSelectedItemsCount();
}

//-----------------------------------------------------------------------------
// Purpose: Adds all the selected servers to the favorites list
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnAddToFavorites()
{
	// loop through all the selected favorites
	for ( int i = 0; i < m_pGameList->GetSelectedItemsCount(); i++ )
	{
		int serverID = m_pGameList->GetItemUserData( m_pGameList->GetSelectedItem( i ) );

		gameserveritem_t *pServer = SteamMatchmakingServers()->GetServerDetails( m_hServerRequest, serverID );
		if ( pServer )
		{
			// add to favorites list
			ServerBrowserDialog().AddServerToFavorites( *pServer );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Positions the controls and updates the button states
//-----------------------------------------------------------------------------
void CBaseGamesPage::PerformLayout()
{
	vgui2::Panel::PerformLayout();

	m_pConnect->SetEnabled( m_pGameList->GetSelectedItemsCount() > 0 );

	if ( SupportsItem( IGameList::GETNEWLIST ) )
	{
		m_pRefreshQuick->SetVisible( true );
		m_pRefreshAll->SetText( "#ServerBrowser_RefreshAll" );
	}
	else
	{
		m_pRefreshQuick->SetVisible( false );
		m_pRefreshAll->SetText( "#ServerBrowser_Refresh" );
	}

	m_pAddServer->SetVisible( SupportsItem( IGameList::ADDSERVER ) );
	m_pAddCurrentServer->SetVisible( SupportsItem( IGameList::ADDCURRENTSERVER ) );

	if ( IsRefreshing() )
	{
		m_pRefreshAll->SetText( "#ServerBrowser_StopRefreshingList" );
	}

	m_pRefreshQuick->SetEnabled( m_pGameList->GetItemCount() > 0 );

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Loads the column icons and the list font
//-----------------------------------------------------------------------------
void CBaseGamesPage::ApplySchemeSettings( vgui2::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// load the password icon
	vgui2::ImageList *imageList = new vgui2::ImageList( false );
	imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_password", false ) );
	imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_bots", false ) );
	imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_robotron", false ) );
	imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_secure_deny", false ) );

	int passwordColumnImage = imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_password_column", false ) );
	int botColumnImage = imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_bots_column", false ) );
	int secureColumnImage = imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_robotron_column", false ) );

	m_pGameList->SetImageList( imageList, true );

	m_hFont = pScheme->GetFontEx( "ListSmall", IsProportional(), IsHDProportional() );
	if ( !m_hFont )
	{
		m_hFont = pScheme->GetFontEx( "DefaultSmall", IsProportional(), IsHDProportional() );
	}

	m_pGameList->SetFont( m_hFont );
	m_pGameList->SetColumnHeaderImage( 0, passwordColumnImage );
	m_pGameList->SetColumnHeaderImage( 1, botColumnImage );
	m_pGameList->SetColumnHeaderImage( 2, secureColumnImage );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the display data for the specified server, or NULL
//-----------------------------------------------------------------------------
serverdisplay_t *CBaseGamesPage::GetServer( int serverID )
{
	int iServerMap = m_mapServers.Find( serverID );
	if ( iServerMap == -1 )
		return NULL;

	return &m_mapServers[ iServerMap ];
}

//-----------------------------------------------------------------------------
// Purpose: Returns the matchmaking details for the specified server
//-----------------------------------------------------------------------------
gameserveritem_t *CBaseGamesPage::GetServerInfo( int serverID )
{
	if ( serverID < 0 )
		return NULL;

	return SteamMatchmakingServers()->GetServerDetails( m_hServerRequest, serverID );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the list panel's invalid item ID
//-----------------------------------------------------------------------------
int CBaseGamesPage::GetInvalidServerListID()
{
	return m_pGameList->InvalidItemID();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the current set of server filters
//-----------------------------------------------------------------------------
uint32 CBaseGamesPage::GetServerFilters( MatchMakingKeyValuePair_t **pFilters )
{
	*pFilters = m_vecServerFilters.Base();
	return m_vecServerFilters.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Creates the filter controls and fills in the game list
//-----------------------------------------------------------------------------
void CBaseGamesPage::CreateFilters()
{
	m_bFiltersVisible = false;
	m_pFilter = new vgui2::ToggleButton( this, "Filter", "#ServerBrowser_Filter" );
	m_pFilterString = new vgui2::Label( this, "FilterString", "" );

	// filter controls
	m_pGameFilter = new vgui2::ComboBox( this, "GameFilter", 6, false );

	m_pMapFilter = new vgui2::TextEntry( this, "MapFilter" );

	m_pPingFilter = new vgui2::ComboBox( this, "PingFilter", 6, false );
	m_pPingFilter->AddItem( "#ServerBrowser_All", NULL );
	m_pPingFilter->AddItem( "#ServerBrowser_LessThan50", NULL );
	m_pPingFilter->AddItem( "#ServerBrowser_LessThan100", NULL );
	m_pPingFilter->AddItem( "#ServerBrowser_LessThan150", NULL );
	m_pPingFilter->AddItem( "#ServerBrowser_LessThan250", NULL );
	m_pPingFilter->AddItem( "#ServerBrowser_LessThan350", NULL );
	m_pPingFilter->AddItem( "#ServerBrowser_LessThan600", NULL );

	m_pSecureFilter = new vgui2::ComboBox( this, "SecureFilter", 3, false );
	m_pSecureFilter->AddItem( "#ServerBrowser_All", NULL );
	m_pSecureFilter->AddItem( "#ServerBrowser_SecureOnly", NULL );
	m_pSecureFilter->AddItem( "#ServerBrowser_InsecureOnly", NULL );

	m_pNoEmptyServersFilterCheck = new vgui2::CheckButton( this, "ServerEmptyFilterCheck", "" );
	m_pNoFullServersFilterCheck = new vgui2::CheckButton( this, "ServerFullFilterCheck", "" );
	m_pNoPasswordFilterCheck = new vgui2::CheckButton( this, "NoPasswordFilterCheck", "" );
	m_pValidSteamAccountFilterCheck = new vgui2::CheckButton( this, "ValidSteamAccountFilterCheck", "" );

	// setup game list
	KeyValues *pkv = new KeyValues( "mod", "gamedir", "", "appid", NULL );
	m_pGameFilter->AddItem( "#ServerBrowser_All", pkv );

	for ( int i = 0; i < ModList().ModCount(); i++ )
	{
		pkv->SetString( "gamedir", ModList().GetModDir( i ) );
		pkv->SetUint64( "appid", ModList().GetAppID( i ).ToUint64() );
		int iItemID = m_pGameFilter->AddItem( ModList().GetModName( i ), pkv );
		m_mapGamesFilterItem.InsertOrReplace( ModList().GetAppID( i ).ToUint64(), iItemID );
	}
	pkv->deleteThis();

	// we want to be notified when new mods are found
	ModList().AddVGUIListener( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: Adds or updates a mod in the game filter combo box
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnLoadModList( uint64 nAppID )
{
	CGameID gameID( nAppID );
	KeyValues *pkv = new KeyValues( "mod" );

	int iModIndex = ModList().GetIndex( gameID );
	int iFilterItem = m_mapGamesFilterItem.Find( nAppID );
	const char *pszGameDir = ModList().GetModDir( iModIndex );

	pkv->SetString( "gamedir", pszGameDir );
	pkv->SetUint64( "appid", ModList().GetAppID( iModIndex ).ToUint64() );

	if ( iFilterItem == -1 )
	{
		int iItemID = m_pGameFilter->AddItem( ModList().GetModName( iModIndex ), pkv );
		m_mapGamesFilterItem.InsertOrReplace( ModList().GetAppID( iModIndex ).ToUint64(), iItemID );

		// reselect the mod if it matches the current filter
		if ( !stricmp( pszGameDir, m_szGameFilter ) )
		{
			m_pGameFilter->ActivateItem( iItemID );
		}
	}
	else
	{
		m_pGameFilter->UpdateItem( m_mapGamesFilterItem[ iFilterItem ], ModList().GetModName( iModIndex ), pkv );
	}

	pkv->deleteThis();

	UpdateGameFilter();
	UpdateFilterSettings();
}

//-----------------------------------------------------------------------------
// Purpose: Selects the game filter item matching the current filter settings
//-----------------------------------------------------------------------------
void CBaseGamesPage::UpdateGameFilter()
{
	bool bFound = false;
	for ( int i = 0; i < m_pGameFilter->GetItemCount(); i++ )
	{
		KeyValues *kv = m_pGameFilter->GetItemUserData( i );
		CGameID gameID( kv->GetUint64( "appID", 0 ) );
		const char *pchGameDir = kv->GetString( "gamedir", "" );

		if ( ( gameID == m_iLimitToAppID || m_iLimitToAppID.AppID() == 0 ) &&
			( !m_szGameFilter[0] || ( pchGameDir && pchGameDir[0] && !strncmp( pchGameDir, m_szGameFilter, strlen( pchGameDir ) ) ) ) )
		{
			if ( i != m_pGameFilter->GetActiveItem() )
			{
				m_pGameFilter->ActivateItem( i );
			}
			bFound = true;
			break;
		}
	}

	if ( !bFound )
	{
		// default to empty
		if ( m_pGameFilter->GetActiveItem() != 0 )
		{
			m_pGameFilter->ActivateItem( 0 );
		}
	}

	// only one mod is allowed in the game
	if ( ServerBrowserDialog().GetActiveModName() )
	{
		m_pGameFilter->SetEnabled( false );
		m_pGameFilter->SetText( ServerBrowserDialog().GetActiveGameName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Formats the last played time of a server into the list item
//-----------------------------------------------------------------------------
static void SetServerLastPlayed( KeyValues *kv, uint32 ulTimeLastPlayed )
{
	if ( !ulTimeLastPlayed )
		return;

	// construct a time string for last played time
	time_t lastPlayed = ulTimeLastPlayed;
	struct tm *now = localtime( &lastPlayed );
	if ( !now )
		return;

	char buf[64];
	strftime( buf, sizeof( buf ), "%a %d %b %I:%M%p", now );

	// lowercase the trailing "HH:MMAM" part
	for ( char *pch = buf + strlen( buf ) - 4; *pch; pch++ )
	{
		*pch = tolower( (unsigned char)*pch );
	}

	kv->SetString( "LastPlayed", buf );
}

//-----------------------------------------------------------------------------
// Purpose: Adds or updates a server in the list
//-----------------------------------------------------------------------------
void CBaseGamesPage::ServerResponded( int iServer, gameserveritem_t *pServerItem )
{
	int iServerMap = m_mapServers.Find( iServer );
	if ( iServerMap == -1 )
	{
		netadr_t netAdr( pServerItem->m_NetAdr.GetIP(), pServerItem->m_NetAdr.GetConnectionPort() );

		// remove any stale entry for the same address
		int iIPServer = m_mapServerIP.Find( netAdr );
		if ( iIPServer != -1 )
		{
			int iOldServer = m_mapServers.Find( m_mapServerIP[ iIPServer ] );
			if ( iOldServer != -1 )
			{
				serverdisplay_t &oldServer = m_mapServers[ iOldServer ];
				if ( m_pGameList->IsValidItemID( oldServer.m_iListID ) )
				{
					m_pGameList->RemoveItem( oldServer.m_iListID );
				}
				m_mapServers.RemoveAt( iOldServer );
			}
			m_mapServerIP.RemoveAt( iIPServer );
		}

		serverdisplay_t serverFind;
		serverFind.m_bDoNotRefresh = false;
		iServerMap = m_mapServers.Insert( iServer, serverFind );
		m_mapServerIP.Insert( netAdr, iServer );
	}

	// add the server to the list
	serverdisplay_t *pServer = &m_mapServers[ iServerMap ];
	pServer->m_iServerID = iServer;
	pServer->m_netAdr = pServerItem->m_NetAdr;

	bool bRemove = false;
	if ( !CheckPrimaryFilters( *pServerItem ) )
	{
		// never refresh a server that fails the primary filters
		pServer->m_bDoNotRefresh = true;
		bRemove = true;
	}
	else if ( !CheckSecondaryFilters( *pServerItem ) )
	{
		bRemove = true;
	}

	if ( bRemove )
	{
		// server failed filtering, remove it
		if ( m_pGameList->IsValidItemID( pServer->m_iListID ) )
		{
			m_pGameList->RemoveItem( pServer->m_iListID );
			pServer->m_iListID = GetInvalidServerListID();
		}
		return;
	}

	// update UI
	KeyValues *kv;
	if ( m_pGameList->IsValidItemID( pServer->m_iListID ) )
	{
		// we're updating an existing entry
		kv = m_pGameList->GetItem( pServer->m_iListID );
		m_pGameList->SetUserData( pServer->m_iListID, pServer->m_iServerID );
	}
	else
	{
		// new entry
		kv = new KeyValues( "Server" );
	}

	kv->SetString( "name", pServerItem->GetName() );
	kv->SetString( "map", pServerItem->m_szMap );
	kv->SetString( "GameDir", pServerItem->m_szGameDir );
	kv->SetString( "GameDesc", pServerItem->m_szGameDescription );
	kv->SetString( "GameTags", pServerItem->m_szGameTags );
	kv->SetInt( "password", pServerItem->m_bPassword );

	if ( pServerItem->m_nBotPlayers > 0 )
		kv->SetInt( "bots", pServerItem->m_nBotPlayers );
	else
		kv->SetString( "bots", "" );

	if ( pServerItem->m_bSecure )
	{
		// show the denied icon if banned from secure servers, the secure icon otherwise
		kv->SetInt( "secure", ServerBrowser().IsVACBannedFromGame( pServerItem->m_nAppID ) ? 4 : 3 );
	}
	else
	{
		kv->SetInt( "secure", 0 );
	}

	netadr_t reportedIPAddr( pServerItem->m_NetAdr.GetIP(), pServerItem->m_NetAdr.GetConnectionPort() );
	kv->SetString( "IPAddr", reportedIPAddr.ToString( false ) );

	int iHumans = pServerItem->m_nPlayers - pServerItem->m_nBotPlayers;
	if ( iHumans < 0 )
		iHumans = 0;

	char buf[32];
	snprintf( buf, sizeof( buf ), "%d / %d", iHumans, pServerItem->m_nMaxPlayers );
	kv->SetString( "Players", buf );

	kv->SetInt( "Ping", pServerItem->m_nPing );

	SetServerLastPlayed( kv, pServerItem->m_ulTimeLastPlayed );

	if ( pServer->m_bDoNotRefresh )
	{
		// clear out the vars
		kv->SetString( "Ping", "" );
		kv->SetWString( "GameDesc", vgui2::localize()->Find( "#ServerBrowser_NotResponding" ) );
		kv->SetString( "Players", "" );
		kv->SetString( "map", "" );
	}

	if ( !m_pGameList->IsValidItemID( pServer->m_iListID ) )
	{
		// new server, add to list
		pServer->m_iListID = m_pGameList->AddItem( kv, pServer->m_iServerID, false, false );
		if ( m_bAutoSelectFirstItemInGameList && m_pGameList->GetItemCount() == 1 )
		{
			m_pGameList->SetSingleSelectedItem( pServer->m_iListID );
		}

		kv->deleteThis();
	}
	else
	{
		// tell the list that we've changed the data
		m_pGameList->ApplyItemChanges( pServer->m_iListID );
		m_pGameList->SetItemVisible( pServer->m_iListID, true );
	}

	UpdateStatus();
	m_iServerRefreshCount++;
}

//-----------------------------------------------------------------------------
// Purpose: Handles incoming server refresh data from the matchmaking interface
//-----------------------------------------------------------------------------
void CBaseGamesPage::ServerResponded( HServerListRequest hRequest, int iServer )
{
	if ( hRequest != m_hServerRequest )
		return;

	gameserveritem_t *pServerItem = SteamMatchmakingServers()->GetServerDetails( m_hServerRequest, iServer );
	if ( !pServerItem )
		return;

	if ( m_eMatchMakingType == eFriendsServer )
	{
		// friends' servers are pinged directly to get their details
		SteamMatchmakingServers()->PingServer( pServerItem->m_NetAdr.GetIP(), pServerItem->m_NetAdr.GetConnectionPort(), this );
	}
	else
	{
		ServerResponded( iServer, pServerItem );
	}
}

//-----------------------------------------------------------------------------
// Purpose: A server failed to respond; still show it in the list
//-----------------------------------------------------------------------------
void CBaseGamesPage::ServerFailedToRespond( HServerListRequest hRequest, int iServer )
{
	ServerResponded( hRequest, iServer );
}

//-----------------------------------------------------------------------------
// Purpose: Callback for ISteamMatchmakingPingResponse; adds a server that was
//			pinged directly using a free negative server ID
//-----------------------------------------------------------------------------
void CBaseGamesPage::ServerResponded( gameserveritem_t &server )
{
	int iServer = -1;
	while ( m_mapServers.Find( iServer ) != -1 )
	{
		iServer--;
	}

	ServerResponded( iServer, &server );
}

//-----------------------------------------------------------------------------
// Purpose: Callback for ISteamMatchmakingPingResponse; nothing to do
//-----------------------------------------------------------------------------
void CBaseGamesPage::ServerFailedToRespond()
{
}

//-----------------------------------------------------------------------------
// Purpose: Hides a server in the list
//-----------------------------------------------------------------------------
void CBaseGamesPage::RemoveServer( serverdisplay_t &server )
{
	if ( m_pGameList->IsValidItemID( server.m_iListID ) )
	{
		// don't remove the server from list, just hide since this is a lot faster
		m_pGameList->SetItemVisible( server.m_iListID, false );
	}

	UpdateStatus();
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether a server that passed the filters should be shown
//-----------------------------------------------------------------------------
bool CBaseGamesPage::BShowServer( serverdisplay_t &server )
{
	return server.m_bDoNotRefresh;
}

//-----------------------------------------------------------------------------
// Purpose: Region code used by the server filters; -1 means any region
//-----------------------------------------------------------------------------
int CBaseGamesPage::GetRegionCodeToFilter()
{
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Removes all the servers from the list
//-----------------------------------------------------------------------------
void CBaseGamesPage::ClearServerList()
{
	m_mapServers.RemoveAll();
	m_mapServerIP.RemoveAll();
	m_pGameList->RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Applies the current filters to all the servers in the list
//-----------------------------------------------------------------------------
void CBaseGamesPage::ApplyGameFilters()
{
	// loop through all the servers checking filters
	for ( int i = 0; i < m_mapServers.MaxElement(); i++ )
	{
		if ( !m_mapServers.IsValidIndex( i ) )
			continue;

		serverdisplay_t &server = m_mapServers[ i ];
		gameserveritem_t *pServer = SteamMatchmakingServers()->GetServerDetails( m_hServerRequest, server.m_iServerID );
		if ( !pServer )
			continue;

		if ( !CheckPrimaryFilters( *pServer ) || !CheckSecondaryFilters( *pServer ) )
		{
			// server failed filtering, remove it
			server.m_bDoNotRefresh = true;
			if ( m_pGameList->IsValidItemID( server.m_iListID ) )
			{
				// don't remove the server from list, just hide since this is a lot faster
				m_pGameList->SetItemVisible( server.m_iListID, false );
			}
		}
		else if ( BShowServer( server ) )
		{
			// server passed filters, so it can be refreshed again
			server.m_bDoNotRefresh = false;
			gameserveritem_t *pServerItem = SteamMatchmakingServers()->GetServerDetails( m_hServerRequest, server.m_iServerID );

			// re-add item to list
			if ( !m_pGameList->IsValidItemID( server.m_iListID ) )
			{
				KeyValues *kv = new KeyValues( "Server" );
				kv->SetString( "name", pServerItem->GetName() );
				kv->SetString( "map", pServerItem->m_szMap );
				kv->SetString( "GameDir", pServerItem->m_szGameDir );
				kv->SetString( "GameTags", pServerItem->m_szGameTags );
				if ( pServerItem->m_szGameDescription[0] )
				{
					kv->SetString( "GameDesc", pServerItem->m_szGameDescription );
				}
				else
				{
					kv->SetWString( "GameDesc", vgui2::localize()->Find( "#ServerBrowser_PendingPing" ) );
				}

				int iHumans = pServerItem->m_nPlayers - pServerItem->m_nBotPlayers;
				if ( iHumans < 0 )
					iHumans = 0;

				char buf[256];
				snprintf( buf, sizeof( buf ), "%d / %d", iHumans, pServerItem->m_nMaxPlayers );
				kv->SetString( "Players", buf );
				kv->SetInt( "Ping", pServerItem->m_nPing );
				kv->SetInt( "password", pServerItem->m_bPassword );

				if ( pServerItem->m_nBotPlayers > 0 )
					kv->SetInt( "bots", pServerItem->m_nBotPlayers );
				else
					kv->SetString( "bots", "" );

				if ( pServerItem->m_bSecure )
				{
					// show the denied icon if banned from secure servers, the secure icon otherwise
					kv->SetInt( "secure", ServerBrowser().IsVACBannedFromGame( pServerItem->m_nAppID ) ? 4 : 3 );
				}
				else
				{
					kv->SetInt( "secure", 0 );
				}

				netadr_t reportedIPAddr( pServerItem->m_NetAdr.GetIP(), pServerItem->m_NetAdr.GetConnectionPort() );
				kv->SetString( "IPAddr", reportedIPAddr.ToString( false ) );

				SetServerLastPlayed( kv, pServerItem->m_ulTimeLastPlayed );

				if ( pServerItem->m_bDoNotRefresh )
				{
					// clear out the vars
					kv->SetString( "Ping", "" );
					kv->SetWString( "GameDesc", vgui2::localize()->Find( "#ServerBrowser_NotResponding" ) );
					kv->SetString( "Players", "" );
					kv->SetString( "map", "" );
				}

				server.m_iListID = m_pGameList->AddItem( kv, server.m_iServerID, false, false );
				kv->deleteThis();
			}

			// make sure the server is visible
			m_pGameList->SetItemVisible( server.m_iListID, true );
		}
	}

	UpdateStatus();
	m_pGameList->SortList();
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Updates the server count shown in the list header
//-----------------------------------------------------------------------------
void CBaseGamesPage::UpdateStatus()
{
	if ( m_pGameList->GetItemCount() > 1 )
	{
		wchar_t header[256];
		wchar_t count[128];

		swprintf( count, sizeof( count ) / sizeof( wchar_t ), L"%d", m_pGameList->GetItemCount() );
		vgui2::localize()->ConstructString( header, sizeof( header ), vgui2::localize()->Find( "#ServerBrowser_ServersCount" ), 1, count );
		m_pGameList->SetColumnHeaderText( 4, header );
	}
	else
	{
		m_pGameList->SetColumnHeaderText( 4, vgui2::localize()->Find( "#ServerBrowser_Servers" ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles filter dropdown being toggled
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnButtonToggled( vgui2::Panel *panel, int state )
{
	if ( panel == m_pFilter )
	{
		int wide, tall;
		GetSize( wide, tall );

		int scaledTall = GetProportionalScaledValue( 278 );
		int scaledWide = GetProportionalScaledValue( 624 );
		SetSize( scaledWide, scaledTall );

		// reload the whole control
		char szSettings[4096];
		if ( m_pCustomResFilename )
		{
			m_bFiltersVisible = false;
			snprintf( szSettings, sizeof( szSettings ), "%s", m_pCustomResFilename );
		}
		else if ( m_pFilter->IsSelected() )
		{
			// drop down
			m_bFiltersVisible = true;
			snprintf( szSettings, sizeof( szSettings ), "servers/%sPage_Filters.res", "InternetGames" );
		}
		else
		{
			// hide filter area
			m_bFiltersVisible = false;
			snprintf( szSettings, sizeof( szSettings ), "servers/%sPage.res", "InternetGames" );
		}

		LoadControlSettings( szSettings, NULL );

		m_pFilter->SetSelected( m_bFiltersVisible );

		UpdateGameFilter();

		if ( m_hFont )
		{
			m_pGameList->MakeReadyForUse();
			m_pGameList->SetFont( m_hFont );
		}

		SetSize( wide, tall );

		InvalidateLayout( false, false );
		Repaint();
	}
	else if ( panel == m_pNoFullServersFilterCheck || panel == m_pNoEmptyServersFilterCheck ||
		panel == m_pNoPasswordFilterCheck || panel == m_pValidSteamAccountFilterCheck )
	{
		// treat changing these buttons like any other filter has changed
		OnTextChanged( panel, "" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when one of the filter text entries or combo boxes changes
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnTextChanged( vgui2::Panel *panel, const char *text )
{
	if ( !stricmp( text, m_szComboAllText ) )
	{
		vgui2::ComboBox *box = dynamic_cast<vgui2::ComboBox *>( panel );
		if ( box )
		{
			box->SetText( "" );
		}
	}

	// get filter settings from controls
	UpdateFilterSettings();

	// apply settings
	ApplyGameFilters();

	if ( m_bFiltersVisible && panel == m_pGameFilter && ServerBrowserDialog().IsVisible() )
	{
		// if they changed games and/or filters, refresh the server list
		StopRefresh();
		GetNewServerList();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Loads the filter settings from the saved config and updates the controls
//-----------------------------------------------------------------------------
void CBaseGamesPage::LoadFilterSettings()
{
	KeyValues *filter = ServerBrowserDialog().GetFilterSaveData( GetName() );

	if ( ServerBrowserDialog().GetActiveModName() )
	{
		snprintf( m_szGameFilter, sizeof( m_szGameFilter ), "%s", ServerBrowserDialog().GetActiveModName() );
		m_iLimitToAppID = ServerBrowserDialog().GetActiveAppID();
	}
	else
	{
		snprintf( m_szGameFilter, sizeof( m_szGameFilter ), "%s", filter->GetString( "game", "" ) );
		m_iLimitToAppID.Set( filter->GetUint64( "appid", 0 ) );
	}

	snprintf( m_szMapFilter, sizeof( m_szMapFilter ), "%s", filter->GetString( "map", "" ) );
	m_iPingFilter = filter->GetInt( "ping", 0 );
	m_bFilterNoFullServers = filter->GetInt( "NoFull", 0 );
	m_bFilterNoEmptyServers = filter->GetInt( "NoEmpty", 0 );
	m_bFilterNoPasswordedServers = filter->GetInt( "NoPassword", 0 );
	m_bFilterValidSteamAccount = filter->GetInt( "ValidSteamAccount", 0 );

	m_pSecureFilter->ActivateItem( filter->GetInt( "Secure", 0 ) );

	// apply to the controls
	UpdateGameFilter();
	m_pMapFilter->SetText( m_szMapFilter );

	if ( m_iPingFilter )
	{
		char buf[32];
		snprintf( buf, sizeof( buf ), "< %d", m_iPingFilter );
		m_pPingFilter->SetText( buf );
	}

	m_pNoFullServersFilterCheck->SetSelected( m_bFilterNoFullServers );
	m_pNoEmptyServersFilterCheck->SetSelected( m_bFilterNoEmptyServers );
	m_pNoPasswordFilterCheck->SetSelected( m_bFilterNoPasswordedServers );
	m_pValidSteamAccountFilterCheck->SetSelected( m_bFilterValidSteamAccount );

	OnLoadFilter( filter );

	// update filter table
	UpdateFilterSettings();
}

//-----------------------------------------------------------------------------
// Purpose: Reads the filter settings from the controls, rebuilds the
//			matchmaking filter list and saves the settings
//-----------------------------------------------------------------------------
void CBaseGamesPage::UpdateFilterSettings()
{
	// game
	if ( ServerBrowserDialog().GetActiveModName() )
	{
		// overriding the game filter
		snprintf( m_szGameFilter, sizeof( m_szGameFilter ), "%s", ServerBrowserDialog().GetActiveModName() );
		m_iLimitToAppID = ServerBrowserDialog().GetActiveAppID();
		RecalculateFilterString();
		UpdateGameFilter();
	}
	else
	{
		KeyValues *data = m_pGameFilter->GetActiveItemUserData();
		if ( data && data->GetString( "gamedir", "" )[0] )
		{
			snprintf( m_szGameFilter, sizeof( m_szGameFilter ), "%s", data->GetString( "gamedir", "" ) );
			if ( m_szGameFilter[0] )
			{
				m_iLimitToAppID.Set( data->GetUint64( "appid", 0 ) );
			}
			else
			{
				m_iLimitToAppID.Set( 0ULL );
			}
		}
		else
		{
			m_iLimitToAppID.Set( 0ULL );
			m_szGameFilter[0] = 0;
		}
		m_pGameFilter->SetEnabled( true );
	}

	for ( char *pch = m_szGameFilter; *pch; pch++ )
	{
		*pch = tolower( *pch );
	}

	// map
	m_pMapFilter->GetText( m_szMapFilter, sizeof( m_szMapFilter ) - 1 );
	for ( char *pch = m_szMapFilter; *pch; pch++ )
	{
		*pch = tolower( *pch );
	}

	// ping
	char buf[256];
	m_pPingFilter->GetText( buf, sizeof( buf ) );
	if ( buf[0] )
	{
		// skip the "< " prefix of the combo box text
		m_iPingFilter = atoi( buf + 2 );
	}
	else
	{
		m_iPingFilter = 0;
	}

	// players
	m_bFilterNoFullServers = m_pNoFullServersFilterCheck->IsSelected();
	m_bFilterNoEmptyServers = m_pNoEmptyServersFilterCheck->IsSelected();
	m_bFilterNoPasswordedServers = m_pNoPasswordFilterCheck->IsSelected();
	m_bFilterValidSteamAccount = m_pValidSteamAccountFilterCheck->IsSelected();
	m_iSecureFilter = m_pSecureFilter->GetActiveItem();

	m_vecServerFilters.RemoveAll();

	// update master filter string text
	if ( m_szGameFilter[0] && m_iLimitToAppID.AppID() != 1002 )
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "gamedir", m_szGameFilter ) );
	}

	if ( m_bFilterNoEmptyServers )
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "empty", "1" ) );
	}

	if ( m_bFilterNoFullServers )
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "full", "1" ) );
	}

	if ( m_iSecureFilter == 1 )
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "secure", "1" ) );
	}

	// copy filter settings into filter file
	KeyValues *filter = ServerBrowserDialog().GetFilterSaveData( GetName() );

	// only save the game filter if we're not overriding it
	if ( !ServerBrowserDialog().GetActiveModName() )
	{
		filter->SetString( "game", m_szGameFilter );
		filter->SetUint64( "appid", m_iLimitToAppID.ToUint64() );
	}

	filter->SetString( "map", m_szMapFilter );
	filter->SetInt( "ping", m_iPingFilter );
	filter->SetInt( "NoFull", m_bFilterNoFullServers );
	filter->SetInt( "NoEmpty", m_bFilterNoEmptyServers );
	filter->SetInt( "NoPassword", m_bFilterNoPasswordedServers );
	filter->SetInt( "ValidSteamAccount", m_bFilterValidSteamAccount );
	filter->SetInt( "Secure", m_iSecureFilter );

	OnSaveFilter( filter );

	RecalculateFilterString();
}

//-----------------------------------------------------------------------------
// Purpose: Lets derived pages save their own filter settings
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnSaveFilter( KeyValues *filter )
{
}

//-----------------------------------------------------------------------------
// Purpose: Lets derived pages load their own filter settings
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnLoadFilter( KeyValues *filter )
{
}

//-----------------------------------------------------------------------------
// Purpose: Rebuilds the human readable description of the active filters
//-----------------------------------------------------------------------------
void CBaseGamesPage::RecalculateFilterString()
{
	wchar_t unicode[2048];
	wchar_t tempUnicode[128];
	wchar_t spacerUnicode[8];
	unicode[0] = 0;
	vgui2::localize()->ConvertANSIToUnicode( "; ", spacerUnicode, sizeof( spacerUnicode ) );

	if ( m_szGameFilter[0] )
	{
		vgui2::localize()->ConvertANSIToUnicode( ModList().GetModNameForModDir( m_iLimitToAppID ), tempUnicode, sizeof( tempUnicode ) );
		wcscat( unicode, tempUnicode );
		wcscat( unicode, spacerUnicode );
	}

	if ( m_iSecureFilter == 1 )
	{
		wcscat( unicode, vgui2::localize()->Find( "#ServerBrowser_FilterDescSecureOnly" ) );
		wcscat( unicode, spacerUnicode );
	}
	else if ( m_iSecureFilter == 2 )
	{
		wcscat( unicode, vgui2::localize()->Find( "#ServerBrowser_FilterDescInsecureOnly" ) );
		wcscat( unicode, spacerUnicode );
	}

	if ( m_iPingFilter )
	{
		char tmpBuf[16];
		snprintf( tmpBuf, sizeof( tmpBuf ), "%d", m_iPingFilter );

		wcscat( unicode, vgui2::localize()->Find( "#ServerBrowser_FilterDescLatency" ) );
		vgui2::localize()->ConvertANSIToUnicode( " < ", tempUnicode, sizeof( tempUnicode ) );
		wcscat( unicode, tempUnicode );
		vgui2::localize()->ConvertANSIToUnicode( tmpBuf, tempUnicode, sizeof( tempUnicode ) );
		wcscat( unicode, tempUnicode );
		wcscat( unicode, spacerUnicode );
	}

	if ( m_bFilterNoFullServers )
	{
		wcscat( unicode, vgui2::localize()->Find( "#ServerBrowser_FilterDescNotFull" ) );
		wcscat( unicode, spacerUnicode );
	}

	if ( m_bFilterNoEmptyServers )
	{
		wcscat( unicode, vgui2::localize()->Find( "#ServerBrowser_FilterDescNotEmpty" ) );
		wcscat( unicode, spacerUnicode );
	}

	if ( m_bFilterNoPasswordedServers )
	{
		wcscat( unicode, vgui2::localize()->Find( "#ServerBrowser_FilterDescNoPassword" ) );
		wcscat( unicode, spacerUnicode );
	}

	if ( m_bFilterValidSteamAccount )
	{
		wcscat( unicode, vgui2::localize()->Find( "#ServerBrowser_FilterDescValidSteamAccount" ) );
		wcscat( unicode, spacerUnicode );
	}

	if ( m_szMapFilter[0] )
	{
		vgui2::localize()->ConvertANSIToUnicode( m_szMapFilter, tempUnicode, sizeof( tempUnicode ) );
		wcscat( unicode, tempUnicode );
	}

	m_pFilterString->SetText( unicode );
}

//-----------------------------------------------------------------------------
// Purpose: Checks the filters that select which servers are queried at all
//-----------------------------------------------------------------------------
bool CBaseGamesPage::CheckPrimaryFilters( gameserveritem_t &server )
{
	if ( m_szGameFilter[0] && ( server.m_szGameDir[0] || server.m_nPing ) && stricmp( m_szGameFilter, server.m_szGameDir ) )
	{
		if ( server.m_nAppID != 1002 )
			return false;
	}

	// only list servers with an app ID below 200
	return server.m_nAppID < 200;
}

//-----------------------------------------------------------------------------
// Purpose: Checks the filters that may change while servers are listed
// Output : true if the server passes the filters
//-----------------------------------------------------------------------------
bool CBaseGamesPage::CheckSecondaryFilters( gameserveritem_t &server )
{
	if ( m_bFilterNoEmptyServers && ( server.m_nPlayers - server.m_nBotPlayers ) < 1 )
		return false;

	if ( m_bFilterNoFullServers && server.m_nPlayers >= server.m_nMaxPlayers )
		return false;

	if ( m_iPingFilter && server.m_nPing > m_iPingFilter )
		return false;

	if ( m_bFilterNoPasswordedServers && server.m_bPassword )
		return false;

	if ( m_bFilterValidSteamAccount && !server.m_steamID.GetAccountID() )
		return false;

	if ( m_iSecureFilter == 1 && !server.m_bSecure )
		return false;

	if ( m_iSecureFilter == 2 && server.m_bSecure )
		return false;

	// compare the first few characters of the filter
	int count = strlen( m_szMapFilter );
	if ( count && strnicmp( server.m_szMap, m_szMapFilter, count ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Starts the process of refreshing the server list
//-----------------------------------------------------------------------------
void CBaseGamesPage::StartRefresh()
{
	if ( m_hServerRequest )
	{
		SteamMatchmakingServers()->ReleaseRequest( m_hServerRequest );
		m_hServerRequest = NULL;
	}

	ClearServerList();

	MatchMakingKeyValuePair_t *pFilters = m_vecServerFilters.Base();
	int nFilters = m_vecServerFilters.Count();

	switch ( m_eMatchMakingType )
	{
	case eInternetServer:
		m_hServerRequest = SteamMatchmakingServers()->RequestInternetServerList( m_iLimitToAppID.AppID(), &pFilters, nFilters, this );
		break;
	case eLANServer:
		m_hServerRequest = SteamMatchmakingServers()->RequestLANServerList( m_iLimitToAppID.AppID(), this );
		break;
	case eFriendsServer:
		m_hServerRequest = SteamMatchmakingServers()->RequestFriendsServerList( m_iLimitToAppID.AppID(), &pFilters, nFilters, this );
		break;
	case eFavoritesServer:
		m_hServerRequest = SteamMatchmakingServers()->RequestFavoritesServerList( m_iLimitToAppID.AppID(), &pFilters, nFilters, this );
		break;
	case eHistoryServer:
		m_hServerRequest = SteamMatchmakingServers()->RequestHistoryServerList( m_iLimitToAppID.AppID(), &pFilters, nFilters, this );
		break;
	case eSpectatorServer:
		m_hServerRequest = SteamMatchmakingServers()->RequestSpectatorServerList( m_iLimitToAppID.AppID(), &pFilters, nFilters, this );
		break;
	default:
		break;
	}

	SetRefreshing( true );

	m_iServerRefreshCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Gets a new server list; by default the same as a full refresh
//-----------------------------------------------------------------------------
void CBaseGamesPage::GetNewServerList()
{
	StartRefresh();
}

//-----------------------------------------------------------------------------
// Purpose: Stops the current refresh
//-----------------------------------------------------------------------------
void CBaseGamesPage::StopRefresh()
{
	// clear update states
	m_iServerRefreshCount = 0;

	// Stop the server list refreshing
	SteamMatchmakingServers()->CancelQuery( m_hServerRequest );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the list is currently refreshing servers
//-----------------------------------------------------------------------------
bool CBaseGamesPage::IsRefreshing()
{
	if ( !SteamMatchmakingServers() )
		return false;

	return SteamMatchmakingServers()->IsRefreshing( m_hServerRequest );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the status text and refresh buttons for the refresh state
//-----------------------------------------------------------------------------
void CBaseGamesPage::SetRefreshing( bool state )
{
	if ( state )
	{
		ServerBrowserDialog().UpdateStatusText( "#ServerBrowser_RefreshingServerList" );

		// clear message in panel
		m_pGameList->SetEmptyListText( "" );
		m_pRefreshAll->SetText( "#ServerBrowser_StopRefreshingList" );
		m_pRefreshAll->SetCommand( "stoprefresh" );
		m_pRefreshQuick->SetEnabled( false );
	}
	else
	{
		ServerBrowserDialog().UpdateStatusText( "" );
		if ( SupportsItem( IGameList::GETNEWLIST ) )
		{
			m_pRefreshAll->SetText( "#ServerBrowser_RefreshAll" );
		}
		else
		{
			m_pRefreshAll->SetText( "#ServerBrowser_Refresh" );
		}
		m_pRefreshAll->SetCommand( "GetNewList" );

		// 'refresh quick' button is only enabled if there are servers in the list
		if ( m_pGameList->GetItemCount() > 0 )
		{
			m_pRefreshQuick->SetEnabled( true );
		}
		else
		{
			m_pRefreshQuick->SetEnabled( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles button commands
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnCommand( const char *command )
{
	if ( !stricmp( command, "Connect" ) )
	{
		OnBeginConnect();
	}
	else if ( !stricmp( command, "stoprefresh" ) )
	{
		// cancel the existing refresh
		StopRefresh();
	}
	else if ( !stricmp( command, "refresh" ) )
	{
		SteamMatchmakingServers()->RefreshQuery( m_hServerRequest );
		SetRefreshing( true );
		m_iServerRefreshCount = 0;
	}
	else if ( !stricmp( command, "GetNewList" ) )
	{
		GetNewServerList();
	}
	else
	{
		vgui2::Panel::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes the list on F5
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnKeyCodePressed( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_F5 )
	{
		StartRefresh();
	}
	else
	{
		vgui2::Panel::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Lets derived pages handle ENTER in the game list; returns true if handled
//-----------------------------------------------------------------------------
bool CBaseGamesPage::OnGameListEnterPressed()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Updates the connect button when the list selection changes
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnItemSelected()
{
	if ( m_pGameList->GetSelectedItemsCount() < 1 )
	{
		m_pConnect->SetEnabled( false );
	}
	else
	{
		m_pConnect->SetEnabled( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Initiates a server connection
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnBeginConnect()
{
	if ( !m_pGameList->GetSelectedItemsCount() )
		return;

	// get the server
	unsigned int serverID = m_pGameList->GetItemUserData( m_pGameList->GetSelectedItem( 0 ) );

	// Stop the current refresh
	StopRefresh();

	// join the game
	ServerBrowserDialog().JoinGame( this, serverID );
}

//-----------------------------------------------------------------------------
// Purpose: Displays the current game info without connecting
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnViewGameInfo()
{
	if ( !m_pGameList->GetSelectedItemsCount() )
		return;

	// get the server
	unsigned int serverID = m_pGameList->GetItemUserData( m_pGameList->GetSelectedItem( 0 ) );

	// Stop the current refresh
	StopRefresh();

	// join the game
	ServerBrowserDialog().OpenGameInfoDialog( this, serverID );
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes the selected servers
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnRefreshServer( int serverID )
{
	// walk the list of selected servers refreshing them
	for ( int i = 0; i < m_pGameList->GetSelectedItemsCount(); i++ )
	{
		int iServerID = m_pGameList->GetItemUserData( m_pGameList->GetSelectedItem( i ) );

		// refresh this server
		SteamMatchmakingServers()->RefreshServer( m_hServerRequest, iServerID );
	}

	SetRefreshing( IsRefreshing() );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the page becomes active; starts a refresh
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnPageShow()
{
	g_CurrentGamesPage = this;
	StartRefresh();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the page is hidden; stops any refresh
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnPageHide()
{
	StopRefresh();
}

//-----------------------------------------------------------------------------
// Purpose: Steam callback for favorites/history list changes
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnFavoritesMsg( FavoritesListChanged_t *pFavListChanged )
{
	if ( !pFavListChanged->m_nIP )
	{
		// a zero IP means the whole list was reloaded and we need to reload
		if ( m_eMatchMakingType == eFavoritesServer || m_eMatchMakingType == eHistoryServer )
		{
			// check containing property sheet to see if the page is visible.
			// if not, don't bother initiating a server list grab right now -
			// it will happen when the dialog is activated later.
			if ( static_cast<vgui2::PropertySheet *>( GetParent() )->GetActivePage() == this &&
				GetParent()->IsVisible() && ServerBrowserDialog().IsVisible() )
			{
				GetNewServerList();
			}
		}
		return;
	}

	if ( m_eMatchMakingType != eFavoritesServer && m_eMatchMakingType != eHistoryServer )
		return;

	if ( pFavListChanged->m_bAdd )
	{
		SteamMatchmakingServers()->PingServer( pFavListChanged->m_nIP, pFavListChanged->m_nQueryPort, this );
		return;
	}

	int iIPServer = m_mapServerIP.Find( netadr_t( pFavListChanged->m_nIP, pFavListChanged->m_nConnPort ) );
	if ( iIPServer != -1 )
	{
		int iServer = m_mapServers.Find( m_mapServerIP[ iIPServer ] );
		RemoveServer( m_mapServers[ iServer ] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: List password column sort function
//-----------------------------------------------------------------------------
int CBaseGamesPage::PasswordCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	if ( !g_CurrentGamesPage )
		return 0;

	gameserveritem_t *s1 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p1.userData );
	gameserveritem_t *s2 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	if ( s1->m_bPassword < s2->m_bPassword )
		return 1;
	else if ( s1->m_bPassword > s2->m_bPassword )
		return -1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: List bots column sort function
//-----------------------------------------------------------------------------
int CBaseGamesPage::BotsCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	if ( !g_CurrentGamesPage )
		return 0;

	gameserveritem_t *s1 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p1.userData );
	gameserveritem_t *s2 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	if ( s1->m_nBotPlayers < s2->m_nBotPlayers )
		return 1;
	else if ( s1->m_nBotPlayers > s2->m_nBotPlayers )
		return -1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: List secure (VAC) column sort function
//-----------------------------------------------------------------------------
int CBaseGamesPage::SecureCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	if ( !g_CurrentGamesPage )
		return 0;

	gameserveritem_t *s1 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p1.userData );
	gameserveritem_t *s2 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	if ( s1->m_bSecure < s2->m_bSecure )
		return 1;
	else if ( s1->m_bSecure > s2->m_bSecure )
		return -1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: List IP address column sort function; orders by IP, then query port
//-----------------------------------------------------------------------------
int CBaseGamesPage::IPAddressCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	if ( !g_CurrentGamesPage )
		return 0;

	gameserveritem_t *s1 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p1.userData );
	gameserveritem_t *s2 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	if ( s1->m_NetAdr.GetIP() < s2->m_NetAdr.GetIP() )
		return -1;
	else if ( s1->m_NetAdr.GetIP() > s2->m_NetAdr.GetIP() )
		return 1;

	if ( s1->m_NetAdr.GetQueryPort() < s2->m_NetAdr.GetQueryPort() )
		return -1;
	else if ( s1->m_NetAdr.GetQueryPort() > s2->m_NetAdr.GetQueryPort() )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: List ping column sort function
//-----------------------------------------------------------------------------
int CBaseGamesPage::PingCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	if ( !g_CurrentGamesPage )
		return 0;

	gameserveritem_t *s1 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p1.userData );
	gameserveritem_t *s2 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	int ping1 = s1->m_nPing;
	int ping2 = s2->m_nPing;

	if ( ping1 < ping2 )
		return -1;
	else if ( ping1 > ping2 )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: List map column sort function
//-----------------------------------------------------------------------------
int CBaseGamesPage::MapCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	if ( !g_CurrentGamesPage )
		return 0;

	gameserveritem_t *s1 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p1.userData );
	gameserveritem_t *s2 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	return stricmp( s1->m_szMap, s2->m_szMap );
}

//-----------------------------------------------------------------------------
// Purpose: List game description column sort function
//-----------------------------------------------------------------------------
int CBaseGamesPage::GameCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	if ( !g_CurrentGamesPage )
		return 0;

	gameserveritem_t *s1 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p1.userData );
	gameserveritem_t *s2 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	return stricmp( s1->m_szGameDescription, s2->m_szGameDescription );
}

//-----------------------------------------------------------------------------
// Purpose: List server name column sort function
//-----------------------------------------------------------------------------
int CBaseGamesPage::ServerNameCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	if ( !g_CurrentGamesPage )
		return 0;

	gameserveritem_t *s1 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p1.userData );
	gameserveritem_t *s2 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	// GetName() may return a shared static buffer, so fetch the second name first
	const char *pszName2 = s2->GetName();
	const char *pszName1 = s1->GetName();
	return stricmp( pszName1, pszName2 );
}

//-----------------------------------------------------------------------------
// Purpose: List players column sort function; compares human player counts
//-----------------------------------------------------------------------------
int CBaseGamesPage::PlayersCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	if ( !g_CurrentGamesPage )
		return 0;

	gameserveritem_t *s1 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p1.userData );
	gameserveritem_t *s2 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	int s1p = s1->m_nPlayers - s1->m_nBotPlayers;
	if ( s1p < 0 )
		s1p = 0;
	int s2p = s2->m_nPlayers - s2->m_nBotPlayers;
	if ( s2p < 0 )
		s2p = 0;

	// compare current players first, then max players
	if ( s1p > s2p )
		return -1;
	if ( s1p < s2p )
		return 1;

	int s1m = s1->m_nMaxPlayers - s1->m_nBotPlayers;
	if ( s1m < 0 )
		s1m = 0;
	int s2m = s2->m_nMaxPlayers - s2->m_nBotPlayers;
	if ( s2m < 0 )
		s2m = 0;

	if ( s1m > s2m )
		return -1;
	if ( s1m < s2m )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: List last played column sort function; most recent first
//-----------------------------------------------------------------------------
int CBaseGamesPage::LastPlayedCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	if ( !g_CurrentGamesPage )
		return 0;

	gameserveritem_t *s1 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p1.userData );
	gameserveritem_t *s2 = SteamMatchmakingServers()->GetServerDetails( g_CurrentGamesPage->m_hServerRequest, p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	// compare last played time
	if ( s1->m_ulTimeLastPlayed > s2->m_ulTimeLastPlayed )
		return -1;
	if ( s1->m_ulTimeLastPlayed < s2->m_ulTimeLastPlayed )
		return 1;

	return 0;
}
