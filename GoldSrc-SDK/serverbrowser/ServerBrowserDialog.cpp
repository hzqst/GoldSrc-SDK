//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ServerBrowserDialog.h"
#include "DialogGameInfo.h"
#include "FavoriteGames.h"
#include "FriendsGames.h"
#include "HistoryGames.h"
#include "InternetGames.h"
#include "LanGames.h"
#include "ServerContextMenu.h"
#include "SpectateGames.h"
#include "igamelist.h"

#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui_controls/Controls.h"
#include "Label.h"
#include "PropertySheet.h"
#include "vgui/KeyValues.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

static CServerBrowserDialog *s_InternetDlg = NULL;

//-----------------------------------------------------------------------------
// Purpose: Returns the single server browser dialog
//-----------------------------------------------------------------------------
CServerBrowserDialog &ServerBrowserDialog()
{
	return *s_InternetDlg;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerBrowserDialog::CServerBrowserDialog( vgui2::Panel *parent ) : vgui2::Frame( parent, "CServerBrowserDialog", true, true )
{
	m_CurrentConnection.m_NetAdr.SetIP( 0 );
	m_CurrentConnection.m_NetAdr.SetConnectionPort( 0 );
	m_CurrentConnection.m_NetAdr.SetQueryPort( 0 );

	SetHDProportional( true );

	m_pFilterData = NULL;
	m_pFavorites = NULL;
	m_szGameName[0] = 0;
	m_szModDir[0] = 0;
	m_pSavedData = NULL;
	m_pHistory = NULL;

	s_InternetDlg = this;

	LoadUserData();

	m_pInternetGames = new CInternetGames( this, "InternetGames", eInternetServer );
	m_pFavorites = new CFavoriteGames( this );
	m_pHistory = new CHistoryGames( this );
	m_pSpectateGames = new CSpectateGames( this );
	m_pLanGames = new CLanGames( this, true, NULL );
	m_pFriendsGames = new CFriendsGames( this );

	SetCloseSound( "sound/UI/launch_dnmenu1.wav" );
	SetMinimumSize( GetProportionalScaledValue( 640 ), GetProportionalScaledValue( 384 ) );
	SetSize( GetProportionalScaledValue( 640 ), GetProportionalScaledValue( 384 ) );

	m_pGameList = m_pInternetGames;

	m_pContextMenu = new CServerContextMenu( this );

	// property sheet
	m_pTabPanel = new vgui2::PropertySheet( this, "GameTabs" );
	int tabWidth = GetProportionalScaledValue( 72 );
	m_pTabPanel->SetTabWidth( tabWidth );
	m_pTabPanel->AddPage( m_pInternetGames, "#ServerBrowser_InternetTab" );
	m_pTabPanel->AddPage( m_pFavorites, "#ServerBrowser_FavoritesTab" );
	m_pTabPanel->AddPage( m_pHistory, "#ServerBrowser_HistoryTab" );
	m_pTabPanel->AddPage( m_pSpectateGames, "#ServerBrowser_SpectateTab" );
	m_pTabPanel->AddPage( m_pLanGames, "#ServerBrowser_LanTab" );
	m_pTabPanel->AddPage( m_pFriendsGames, "#ServerBrowser_FriendsTab" );
	m_pTabPanel->AddActionSignalTarget( this );

	m_pStatusLabel = new vgui2::Label( this, "StatusLabel", "" );

	LoadControlSettingsAndUserConfig( "Servers/DialogServerBrowser.res", 0 );

	// make sure the dialog fits on the screen
	int swide, stall;
	vgui2::surface()->GetScreenSize( swide, stall );
	int wide, tall;
	GetSize( wide, tall );
	if ( wide > swide )
	{
		wide = swide;
	}
	if ( tall > stall )
	{
		tall = stall;
	}
	SetSize( wide, tall );

	m_pStatusLabel->SetText( "" );

	// load current tab
	const char *gameList = m_pSavedData->GetString( "GameList", "" );

	if ( !stricmp( gameList, "spectate" ) )
	{
		m_pTabPanel->SetActivePage( m_pSpectateGames );
	}
	else if ( !stricmp( gameList, "favorites" ) )
	{
		m_pTabPanel->SetActivePage( m_pFavorites );
	}
	else if ( !stricmp( gameList, "history" ) )
	{
		m_pTabPanel->SetActivePage( m_pHistory );
	}
	else if ( !stricmp( gameList, "lan" ) )
	{
		m_pTabPanel->SetActivePage( m_pLanGames );
	}
	else if ( !stricmp( gameList, "friends" ) )
	{
		m_pTabPanel->SetActivePage( m_pFriendsGames );
	}
	else
	{
		m_pTabPanel->SetActivePage( m_pInternetGames );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerBrowserDialog::~CServerBrowserDialog()
{
	if ( m_pContextMenu )
	{
		delete m_pContextMenu;
	}

	SaveUserData();

	if ( m_pSavedData )
	{
		m_pSavedData->deleteThis();
	}

	if ( m_pFilterData )
	{
		m_pFilterData->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the dialog title; the dialog starts hidden
//-----------------------------------------------------------------------------
void CServerBrowserDialog::Initialize()
{
	SetTitle( "#ServerBrowser_Servers", true );
	SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the server info for the given ID on the current page
//-----------------------------------------------------------------------------
gameserveritem_t *CServerBrowserDialog::GetServer( unsigned int serverID )
{
	return m_pGameList->GetServerInfo( serverID );
}

//-----------------------------------------------------------------------------
// Purpose: Activates the dialog and focuses the tabs
//-----------------------------------------------------------------------------
void CServerBrowserDialog::Open()
{
	BaseClass::Activate();
	m_pTabPanel->RequestFocus( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Loads the saved filter and page settings from ServerBrowser.vdf
//-----------------------------------------------------------------------------
void CServerBrowserDialog::LoadUserData()
{
	// free any old filters
	if ( m_pSavedData )
	{
		m_pSavedData->deleteThis();
	}

	if ( m_pFilterData )
	{
		m_pFilterData->deleteThis();
	}

	m_pSavedData = new KeyValues( "Filters" );
	m_pSavedData->LoadFromFile( vgui2::filesystem(), "ServerBrowser.vdf", "CONFIG" );

	KeyValues *filters = m_pSavedData->FindKey( "Filters", false );
	if ( filters )
	{
		// take the filters out of the saved data; m_pFilterData owns them from now on
		m_pSavedData->RemoveSubKey( filters );
		m_pFilterData = filters;
	}
	else
	{
		m_pFilterData = new KeyValues( "Filters" );
	}

	// reload all the page settings if necessary
	if ( m_pHistory )
	{
		// history
		m_pHistory->LoadHistoryList();
		if ( IsVisible() && m_pHistory->IsVisible() )
		{
			m_pHistory->StartRefresh();
		}
	}

	if ( m_pFavorites )
	{
		// favorites
		m_pFavorites->LoadFavoritesList();

		// filters
		ReloadFilterSettings();

		if ( IsVisible() && m_pFavorites->IsVisible() )
		{
			m_pFavorites->StartRefresh();
		}
	}

	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Saves the current page, filters and page layouts
//-----------------------------------------------------------------------------
void CServerBrowserDialog::SaveUserData()
{
	// nothing to save until LoadUserData() has run
	if ( !m_pSavedData || !m_pFilterData )
	{
		return;
	}

	m_pSavedData->Clear();
	m_pSavedData->LoadFromFile( vgui2::filesystem(), "ServerBrowser.vdf", "CONFIG" );

	// set the current tab
	if ( m_pGameList == m_pSpectateGames )
	{
		m_pSavedData->SetString( "GameList", "spectate" );
	}
	else if ( m_pGameList == m_pFavorites )
	{
		m_pSavedData->SetString( "GameList", "favorites" );
	}
	else if ( m_pGameList == m_pLanGames )
	{
		m_pSavedData->SetString( "GameList", "lan" );
	}
	else if ( m_pGameList == m_pFriendsGames )
	{
		m_pSavedData->SetString( "GameList", "friends" );
	}
	else if ( m_pGameList == m_pHistory )
	{
		m_pSavedData->SetString( "GameList", "history" );
	}
	else
	{
		m_pSavedData->SetString( "GameList", "internet" );
	}

	// replace the filters read from disk with a copy of the current ones
	KeyValues *oldFilters = m_pSavedData->FindKey( "Filters", false );
	if ( oldFilters )
	{
		m_pSavedData->RemoveSubKey( oldFilters );
		oldFilters->deleteThis();
	}
	m_pSavedData->AddSubKey( m_pFilterData->MakeCopy() );
	m_pSavedData->SaveToFile( vgui2::filesystem(), "ServerBrowser.vdf", "CONFIG" );

	// save per-page config
	m_pFavorites->SaveUserConfig();
	m_pHistory->SaveUserConfig();
	m_pInternetGames->SaveUserConfig();
	m_pSpectateGames->SaveUserConfig();
	m_pLanGames->SaveUserConfig();
	m_pFriendsGames->SaveUserConfig();
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes the active game list
//-----------------------------------------------------------------------------
void CServerBrowserDialog::RefreshCurrentPage()
{
	if ( m_pGameList )
	{
		m_pGameList->StartRefresh();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Places the dialog relative to the workspace on first use
//-----------------------------------------------------------------------------
bool CServerBrowserDialog::GetDefaultScreenPosition( int &x, int &y, int &wide, int &tall )
{
	int wx, wy, ww, wt;
	vgui2::surface()->GetWorkspaceBounds( wx, wy, ww, wt );
	x = wx + (int)( ww * 0.05 );
	y = wy + (int)( wt * 0.4 );
	wide = (int)( ww * 0.5 );
	tall = (int)( wt * 0.55 );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Passes build mode activation down to the active page
//-----------------------------------------------------------------------------
void CServerBrowserDialog::ActivateBuildMode()
{
	// no subpanel, no build mode
	vgui2::EditablePanel *pg = dynamic_cast<vgui2::EditablePanel *>( m_pTabPanel->GetActivePage() );
	if ( !pg )
		return;

	pg->ActivateBuildMode();
}

//-----------------------------------------------------------------------------
// Purpose: Switches the current game list when the active tab changes
//-----------------------------------------------------------------------------
void CServerBrowserDialog::OnGameListChanged()
{
	m_pGameList = dynamic_cast<IGameList *>( m_pTabPanel->GetActivePage() );

	UpdateStatusText( "" );

	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the server we are currently connected to
//-----------------------------------------------------------------------------
gameserveritem_t *CServerBrowserDialog::GetCurrentConnectedServer()
{
	return &m_CurrentConnection;
}

//-----------------------------------------------------------------------------
// Purpose: Recreates the context menu and parents it to the given panel
//-----------------------------------------------------------------------------
CServerContextMenu *CServerBrowserDialog::GetContextMenu( vgui2::Panel *pPanel )
{
	// create a drop down for this object's states
	if ( m_pContextMenu )
	{
		delete m_pContextMenu;
	}

	m_pContextMenu = new CServerContextMenu( this );
	m_pContextMenu->SetAutoDelete( false );

	if ( !pPanel )
	{
		m_pContextMenu->SetParent( this );
	}
	else
	{
		m_pContextMenu->SetParent( pPanel );
	}

	m_pContextMenu->SetVisible( false );
	return m_pContextMenu;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the single server browser dialog
//-----------------------------------------------------------------------------
CServerBrowserDialog *CServerBrowserDialog::GetInstance()
{
	return s_InternetDlg;
}

//-----------------------------------------------------------------------------
// Purpose: Adds a server to the Steam favorites list
//-----------------------------------------------------------------------------
void CServerBrowserDialog::AddServerToFavorites( gameserveritem_t &server )
{
	SteamMatchmaking()->AddFavoriteGame( server.m_nAppID, server.m_NetAdr.GetIP(), server.m_NetAdr.GetConnectionPort(), server.m_NetAdr.GetQueryPort(), k_unFavoriteFlagFavorite, time( NULL ) );
}

//-----------------------------------------------------------------------------
// Purpose: Joins a game by a specified IP, not attached to any game list
//-----------------------------------------------------------------------------
CDialogGameInfo *CServerBrowserDialog::JoinGame( int serverIP, int serverPort )
{
	// open the game info dialog, then mark it to attempt to connect right away
	CDialogGameInfo *gameDialog = new CDialogGameInfo( NULL, serverIP, (uint16)serverPort, (uint16)serverPort );
	gameDialog->AddActionSignalTarget( this );
	gameDialog->SetParent( GetVParent() );
	gameDialog->Run( "" );

	int i = m_GameInfoDialogs.AddToTail();
	m_GameInfoDialogs[i] = gameDialog;

	gameDialog->Connect();
	return gameDialog;
}

//-----------------------------------------------------------------------------
// Purpose: Joins a game from one of the game lists
//-----------------------------------------------------------------------------
CDialogGameInfo *CServerBrowserDialog::JoinGame( IGameList *gameList, unsigned int serverIndex, bool hltvOnly )
{
	serverdisplay_t *pServer = gameList->GetServer( serverIndex );
	if ( !pServer )
		return NULL;

	// open the game info dialog, then mark it to attempt to connect right away
	CDialogGameInfo *gameDialog = new CDialogGameInfo( NULL, pServer->m_netAdr.GetIP(), pServer->m_netAdr.GetQueryPort(), pServer->m_netAdr.GetConnectionPort(), hltvOnly );
	gameDialog->SetParent( GetVParent() );
	gameDialog->AddActionSignalTarget( this );
	gameDialog->Run( NULL );

	int i = m_GameInfoDialogs.AddToTail();
	m_GameInfoDialogs[i] = gameDialog;

	gameDialog->Connect();
	return gameDialog;
}

//-----------------------------------------------------------------------------
// Purpose: Opens a game info dialog for a server in one of the game lists
//-----------------------------------------------------------------------------
CDialogGameInfo *CServerBrowserDialog::OpenGameInfoDialog( IGameList *gameList, unsigned int serverIndex, bool hltvOnly )
{
	serverdisplay_t *pServer = gameList->GetServer( serverIndex );
	if ( !pServer )
		return NULL;

	CDialogGameInfo *gameDialog = new CDialogGameInfo( NULL, pServer->m_netAdr.GetIP(), pServer->m_netAdr.GetQueryPort(), pServer->m_netAdr.GetConnectionPort(), hltvOnly );
	gameDialog->SetParent( GetVParent() );
	gameDialog->AddActionSignalTarget( this );
	gameDialog->Run( NULL );

	int i = m_GameInfoDialogs.AddToTail();
	m_GameInfoDialogs[i] = gameDialog;
	return gameDialog;
}

//-----------------------------------------------------------------------------
// Purpose: Opens a game info dialog for a server specified by IP and ports
//-----------------------------------------------------------------------------
CDialogGameInfo *CServerBrowserDialog::OpenGameInfoDialog( int serverIP, uint16 connPort, uint16 queryPort )
{
	CDialogGameInfo *gameDialog = new CDialogGameInfo( NULL, serverIP, queryPort, connPort );
	gameDialog->AddActionSignalTarget( this );
	gameDialog->SetParent( GetVParent() );
	gameDialog->Run( "" );

	int i = m_GameInfoDialogs.AddToTail();
	m_GameInfoDialogs[i] = gameDialog;
	return gameDialog;
}

//-----------------------------------------------------------------------------
// Purpose: Closes all the game info dialogs
//-----------------------------------------------------------------------------
void CServerBrowserDialog::CloseAllGameInfoDialogs()
{
	for ( int i = 0; i < m_GameInfoDialogs.Count(); i++ )
	{
		vgui2::Panel *dlg = m_GameInfoDialogs[i].Get();
		if ( dlg )
		{
			vgui2::ivgui()->PostMessage( dlg->GetVPanel(), new KeyValues( "Close" ), 0, 0.0f );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds an open game info dialog that is tracking the given friend
//-----------------------------------------------------------------------------
CDialogGameInfo *CServerBrowserDialog::GetDialogGameInfoForFriend( uint64 ulSteamIDFriend )
{
	for ( int i = 0; i < m_GameInfoDialogs.Count(); i++ )
	{
		CDialogGameInfo *pDlg = m_GameInfoDialogs[i];
		if ( pDlg && pDlg->GetAssociatedFriend() == ulSteamIDFriend )
		{
			return pDlg;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the saved filter settings for the named filter set
//-----------------------------------------------------------------------------
KeyValues *CServerBrowserDialog::GetFilterSaveData( const char *filterSet )
{
	return m_pFilterData->FindKey( filterSet, true );
}

//-----------------------------------------------------------------------------
// Purpose: Reloads the filter settings of every page
//-----------------------------------------------------------------------------
void CServerBrowserDialog::ReloadFilterSettings()
{
	m_pInternetGames->LoadFilterSettings();
	m_pSpectateGames->LoadFilterSettings();
	m_pFavorites->LoadFilterSettings();
	m_pLanGames->LoadFilterSettings();
	m_pFriendsGames->LoadFilterSettings();
	m_pHistory->LoadFilterSettings();
}

//-----------------------------------------------------------------------------
// Purpose: Stops all page refreshes when the dialog is closed
//-----------------------------------------------------------------------------
void CServerBrowserDialog::OnClose()
{
	// bug here if you exit before logging in.
	if ( m_pInternetGames )
	{
		m_pInternetGames->StopRefresh();
	}
	if ( m_pFavorites )
	{
		m_pFavorites->StopRefresh();
	}
	if ( m_pHistory )
	{
		m_pHistory->StopRefresh();
	}
	if ( m_pSpectateGames )
	{
		m_pSpectateGames->StopRefresh();
	}
	if ( m_pLanGames )
	{
		m_pLanGames->StopRefresh();
	}
	if ( m_pFriendsGames )
	{
		m_pFriendsGames->StopRefresh();
	}

	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the status bar text from a printf-style format
//-----------------------------------------------------------------------------
void CServerBrowserDialog::UpdateStatusText( const char *fmt, ... )
{
	if ( !m_pStatusLabel )
		return;

	if ( fmt && fmt[0] )
	{
		char str[1024];
		va_list argptr;
		va_start( argptr, fmt );
		vsnprintf( str, sizeof( str ), fmt, argptr );
		va_end( argptr );

		m_pStatusLabel->SetText( str );
	}
	else
	{
		// clear
		m_pStatusLabel->SetText( "" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the status bar text from a unicode string
//-----------------------------------------------------------------------------
void CServerBrowserDialog::UpdateStatusText( wchar_t *unicode )
{
	if ( !m_pStatusLabel )
		return;

	if ( unicode && wcslen( unicode ) > 0 )
	{
		m_pStatusLabel->SetText( unicode );
	}
	else
	{
		// clear
		m_pStatusLabel->SetText( "" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stores the active game/mod and reloads the page filters for it
//-----------------------------------------------------------------------------
void CServerBrowserDialog::OnActiveGameName( KeyValues *pKV )
{
	snprintf( m_szModDir, sizeof( m_szModDir ), "%s", pKV->GetString( "name", "" ) );
	snprintf( m_szGameName, sizeof( m_szGameName ), "%s", pKV->GetString( "game", "" ) );
	m_iLimitAppID = CGameID( pKV->GetUint64( "appid", 0 ) );

	// reload filter settings (since they are mod specific)
	ReloadFilterSettings();
}

//-----------------------------------------------------------------------------
// Purpose: Records the server we connected to and notifies the game info dialogs
//-----------------------------------------------------------------------------
void CServerBrowserDialog::OnConnectToGame( KeyValues *pMessageValues )
{
	int ip = pMessageValues->GetInt( "ip" );
	int connectionPort = pMessageValues->GetInt( "connectionport" );
	int queryPort = pMessageValues->GetInt( "queryport" );

	if ( !ip || !queryPort )
		return;

	memset( &m_CurrentConnection, 0, sizeof( m_CurrentConnection ) );
	m_CurrentConnection.m_NetAdr.Init( ip, queryPort, connectionPort );

	if ( m_pHistory && SteamMatchmaking() )
	{
		SteamMatchmaking()->AddFavoriteGame( 0, IPHostToNetwork( ip ), connectionPort, queryPort, k_unFavoriteFlagHistory, time( NULL ) );
		m_pHistory->SetRefreshOnReload();
	}

	// tell the game info dialogs, so they can cancel if we have connected
	// to a server they were auto-retrying
	for ( int i = 0; i < m_GameInfoDialogs.Count(); i++ )
	{
		vgui2::Panel *dlg = m_GameInfoDialogs[i].Get();
		if ( dlg )
		{
			KeyValues *kv = new KeyValues( "ConnectedToGame", "ip", ip, "connectionport", connectionPort );
			kv->SetInt( "queryport", queryPort );
			vgui2::ivgui()->PostMessage( dlg->GetVPanel(), kv, 0, 0.0f );
		}
	}

	// forward to favorites
	m_pFavorites->OnConnectToGame();

	m_bCurrentlyConnected = true;

	// Now we want to track which tabs have the quick list button checked
	Close();
}

//-----------------------------------------------------------------------------
// Purpose: Clears the current connection when we leave a game
//-----------------------------------------------------------------------------
void CServerBrowserDialog::OnDisconnectFromGame()
{
	m_bCurrentlyConnected = false;
	memset( &m_CurrentConnection, 0, sizeof( m_CurrentConnection ) );
	m_pFavorites->OnDisconnectFromGame();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the active mod directory, or NULL if none is set
//-----------------------------------------------------------------------------
const char *CServerBrowserDialog::GetActiveModName()
{
	return m_szModDir[0] ? m_szModDir : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the active game name, or NULL if none is set
//-----------------------------------------------------------------------------
const char *CServerBrowserDialog::GetActiveGameName()
{
	return m_szGameName[0] ? m_szGameName : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the app the server list is limited to
//-----------------------------------------------------------------------------
CGameID &CServerBrowserDialog::GetActiveAppID()
{
	return m_iLimitAppID;
}

//-----------------------------------------------------------------------------
// Purpose: Appends the most commonly used server query ports to the list
//-----------------------------------------------------------------------------
void GetMostCommonQueryPorts( CUtlVector<unsigned short> &ports )
{
	for ( int i = 0; i <= 5; i++ )
	{
		ports.AddToTail( 27015 + i );
		ports.AddToTail( 26900 + i );
	}

	ports.AddToTail( 4242 );
	ports.AddToTail( 27215 );
}
