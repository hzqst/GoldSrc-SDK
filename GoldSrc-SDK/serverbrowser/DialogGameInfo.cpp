//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "DialogGameInfo.h"
#include "DialogServerPassword.h"
#include "ServerBrowser.h"
#include "ServerBrowserDialog.h"
#include "VACBannedConnRefusedDialog.h"

#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "Button.h"
#include "vgui_controls/Controls.h"
#include "Label.h"
#include "ListPanel.h"
#include "MessageBox.h"
#include "RadioButton.h"
#include "ToggleButton.h"
#include "vgui/KeyValues.h"
#include "GameUI/common/IRunGameEngine.h"
#include "../external/steamworks/isteamapps.h"

#include <stdio.h>
#include <string.h>

// refresh time in ms
static const long RETRY_TIME = 10000;
static const long CHALLENGE_ENTRIES = 1024;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDialogGameInfo::CDialogGameInfo( vgui2::Panel *parent, int serverIP, int queryPort, unsigned short connectionPort, bool hltvOnly )
	: vgui2::Frame( parent, "DialogGameInfo", true, true ),
	m_CallbackPersonaStateChange( this, &CDialogGameInfo::OnPersonaStateChange )
{
	SetHDProportional( true );
	SetBounds( 0, 0, GetProportionalScaledValue( 512 ), GetProportionalScaledValue( 512 ) );
	SetMinimumSize( GetProportionalScaledValue( 416 ), GetProportionalScaledValue( 340 ) );
	SetDeleteSelfOnClose( true );

	m_bConnecting = false;
	m_bHLTVOnly = hltvOnly;
	m_bServerFull = false;
	m_bShowAutoRetryToggle = false;
	m_bServerNotResponding = false;
	m_bShowingExtendedOptions = false;
	m_SteamIDFriend = 0;
	m_hPingQuery = HSERVERQUERY_INVALID;
	m_hPlayersQuery = HSERVERQUERY_INVALID;
	m_bPlayerListUpdatePending = false;

	m_szPassword[0] = 0;

	m_pConnectButton = new vgui2::Button( this, "Connect", "#ServerBrowser_JoinGame" );
	m_pCloseButton = new vgui2::Button( this, "Close", "#ServerBrowser_Close" );
	m_pRefreshButton = new vgui2::Button( this, "Refresh", "#ServerBrowser_Refresh" );
	m_pInfoLabel = new vgui2::Label( this, "InfoLabel", "" );
	m_pAutoRetry = new vgui2::ToggleButton( this, "AutoRetry", "#ServerBrowser_AutoRetry" );
	m_pAutoRetry->AddActionSignalTarget( this );

	m_pAutoRetryAlert = new vgui2::RadioButton( this, "AutoRetryAlert", "#ServerBrowser_AlertMeWhenSlotOpens" );
	m_pAutoRetryJoin = new vgui2::RadioButton( this, "AutoRetryJoin", "#ServerBrowser_JoinWhenSlotOpens" );

	m_pPlayerList = new vgui2::ListPanel( this, "PlayerList" );
	m_pPlayerList->AddColumnHeader( 0, "PlayerName", "#ServerBrowser_PlayerName", GetProportionalScaledValue( 156 ), 0 );
	m_pPlayerList->AddColumnHeader( 1, "Score", "#ServerBrowser_Score", GetProportionalScaledValue( 64 ), 0 );
	m_pPlayerList->AddColumnHeader( 2, "Time", "#ServerBrowser_Time", GetProportionalScaledValue( 64 ), 0 );

	m_pPlayerList->SetSortFunc( 2, &PlayerTimeColumnSortFunc );

	// set the defaults for sorting
	// hack, need to post this to make sure the sort columns are set after the defaults
	PostMessage( m_pPlayerList, new KeyValues( "SetSortColumn", "column", 2 ), 0.0f );
	PostMessage( m_pPlayerList, new KeyValues( "SetSortColumn", "column", 1 ), 0.0f );
	PostMessage( m_pPlayerList, new KeyValues( "SetSortColumn", "column", 1 ), 0.0f );

	m_pAutoRetryAlert->SetSelected( true );

	m_pConnectButton->SetCommand( new KeyValues( "Connect" ) );
	m_pCloseButton->SetCommand( new KeyValues( "Close" ) );
	m_pRefreshButton->SetCommand( new KeyValues( "Refresh" ) );

	m_iRequestRetry = 0;

	// create a new server to watch
	memset( &m_Server, 0, sizeof( m_Server ) );
	m_Server.m_NetAdr.Init( serverIP, queryPort, connectionPort );

	// refresh immediately
	RequestInfo();

	// let us be ticked every frame
	vgui2::ivgui()->AddTickSignal( GetVPanel(), 0 );

	LoadControlSettings( "Servers/DialogGameInfo.res", NULL );
	RegisterControlSettingsFile( "Servers/DialogGameInfo_SinglePlayer.res", NULL );
	RegisterControlSettingsFile( "Servers/DialogGameInfo_AutoRetry.res", NULL );
	MoveToCenterOfScreen();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDialogGameInfo::~CDialogGameInfo()
{
	if ( !SteamMatchmakingServers() )
		return;

	if ( m_hPingQuery != HSERVERQUERY_INVALID )
	{
		SteamMatchmakingServers()->CancelServerQuery( m_hPingQuery );
	}

	if ( m_hPlayersQuery != HSERVERQUERY_INVALID )
	{
		SteamMatchmakingServers()->CancelServerQuery( m_hPlayersQuery );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Activates the dialog
//-----------------------------------------------------------------------------
void CDialogGameInfo::Run( const char *titleName )
{
	SetTitle( "#ServerBrowser_GameInfoWithNameTitle", true );
	SetDialogVariable( "game", titleName );

	// get the info from the server
	RequestInfo();

	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Changes which server this dialog displays and reloads its layout
//-----------------------------------------------------------------------------
void CDialogGameInfo::ChangeGame( int serverIP, int queryPort, unsigned short connectionPort )
{
	memset( &m_Server, 0, sizeof( m_Server ) );
	m_Server.m_NetAdr.SetConnectionPort( connectionPort );
	m_Server.m_NetAdr.SetIP( serverIP );
	m_Server.m_NetAdr.SetQueryPort( queryPort );

	int x, y;
	GetPos( x, y );

	// see if we need to change dialog state
	if ( !m_Server.m_NetAdr.GetIP() || !m_Server.m_NetAdr.GetQueryPort() )
	{
		// not in a server, load the simple settings dialog
		SetMinimumSize( 0, 0 );
		SetSizeable( false );
		LoadControlSettings( "Servers/DialogGameInfo_SinglePlayer.res", NULL );
	}
	else
	{
		// moving from a single-player game -> multiplayer, reset dialog
		SetMinimumSize( GetProportionalScaledValue( 416 ), GetProportionalScaledValue( 340 ) );
		SetSizeable( true );
		LoadControlSettings( "Servers/DialogGameInfo.res", NULL );
	}
	SetPos( x, y );

	// Start refresh immediately
	m_iRequestRetry = 0;
	RequestInfo();
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Associates the dialog with a friend and shows the game they are playing
//-----------------------------------------------------------------------------
void CDialogGameInfo::SetFriend( uint64 ulSteamIDFriend )
{
	// set the title to include the friends name
	SetTitle( "#ServerBrowser_GameInfoWithNameTitle", true );
	SetDialogVariable( "game", SteamFriends()->GetFriendPersonaName( CSteamID( ulSteamIDFriend ) ) );
	SetDialogVariable( "friend", SteamFriends()->GetFriendPersonaName( CSteamID( ulSteamIDFriend ) ) );

	// store the friend we're associated with
	m_SteamIDFriend = ulSteamIDFriend;

	FriendGameInfo_t friendGameInfo;
	if ( SteamFriends()->GetFriendGamePlayed( CSteamID( ulSteamIDFriend ), &friendGameInfo ) )
	{
		ChangeGame( friendGameInfo.m_unGameIP, friendGameInfo.m_usQueryPort, friendGameInfo.m_usGamePort );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the friend this dialog is tracking, if any
//-----------------------------------------------------------------------------
uint64 CDialogGameInfo::GetAssociatedFriend()
{
	return m_SteamIDFriend;
}

//-----------------------------------------------------------------------------
// Purpose: Updates the dialog controls from the current server info
//-----------------------------------------------------------------------------
void CDialogGameInfo::PerformLayout()
{
	vgui2::Frame::PerformLayout();

	SetControlString( "ServerText", m_Server.GetName() );
	SetControlString( "GameText", m_Server.m_szGameDescription );
	SetControlString( "MapText", m_Server.m_szMap );
	SetControlString( "GameTags", m_Server.m_szGameTags );

	if ( !m_Server.m_szGameDescription[0] && m_SteamIDFriend && SteamFriends() )
	{
		// no game description set yet; the public Steamworks API cannot resolve the
		// friend's game name, so only report when they are not in a game and let the
		// server reply fill in the description otherwise
		FriendGameInfo_t friendGameInfo;
		if ( !SteamFriends()->GetFriendGamePlayed( CSteamID( m_SteamIDFriend ), &friendGameInfo ) )
		{
			SetControlString( "GameText", "#ServerBrowser_NotInGame" );
		}
	}

	if ( m_Server.m_bHadSuccessfulResponse )
	{
		SetControlString( "SecureText", m_Server.m_bSecure ? "#ServerBrowser_Secure" : "#ServerBrowser_NotSecure" );
	}
	else
	{
		SetControlString( "SecureText", "" );
	}

	int iHumanPlayers = m_Server.m_nPlayers - m_Server.m_nBotPlayers;
	if ( iHumanPlayers < 0 )
	{
		iHumanPlayers = 0;
	}

	char buf[128];
	if ( m_Server.m_nMaxPlayers > 0 )
	{
		snprintf( buf, sizeof( buf ), "%d / %d", iHumanPlayers, m_Server.m_nMaxPlayers );
	}
	else
	{
		buf[0] = 0;
	}
	SetControlString( "PlayersText", buf );

	if ( m_Server.m_NetAdr.GetIP() && m_Server.m_NetAdr.GetQueryPort() )
	{
		SetControlString( "ServerIPText", m_Server.m_NetAdr.GetConnectionAddressString() );
		m_pConnectButton->SetEnabled( true );
		if ( m_pAutoRetry->IsSelected() )
		{
			m_pAutoRetryAlert->SetVisible( true );
			m_pAutoRetryJoin->SetVisible( true );
		}
		else
		{
			m_pAutoRetryAlert->SetVisible( false );
			m_pAutoRetryJoin->SetVisible( false );
		}
	}
	else
	{
		SetControlString( "ServerIPText", "" );
		m_pConnectButton->SetEnabled( false );
	}

	if ( m_Server.m_bHadSuccessfulResponse )
	{
		snprintf( buf, sizeof( buf ), "%d", m_Server.m_nPing );
		SetControlString( "PingText", buf );
	}
	else
	{
		SetControlString( "PingText", "" );
	}

	// set the info text
	if ( m_pAutoRetry->IsSelected() )
	{
		if ( m_Server.m_nPlayers >= m_Server.m_nMaxPlayers )
		{
			if ( m_pAutoRetryJoin->IsSelected() )
			{
				m_pInfoLabel->SetText( "#ServerBrowser_JoinWhenSlotIsFree" );
			}
			else
			{
				m_pInfoLabel->SetText( "#ServerBrowser_AlertWhenSlotIsFree" );
			}
		}
		else
		{
			m_pInfoLabel->SetText( "#ServerBrowser_PressJoinToConnect" );
		}
	}
	else if ( m_bServerFull )
	{
		m_pInfoLabel->SetText( "#ServerBrowser_CouldNotConnectServerFull" );
	}
	else if ( m_bServerNotResponding )
	{
		m_pInfoLabel->SetText( "#ServerBrowser_ServerNotResponding" );
	}
	else
	{
		// clear the status
		m_pInfoLabel->SetText( "" );
	}

	if ( m_Server.m_bHadSuccessfulResponse && !iHumanPlayers )
	{
		m_pPlayerList->SetEmptyListText( "#ServerBrowser_ServerHasNoPlayers" );
	}
	else
	{
		m_pPlayerList->SetEmptyListText( "#ServerBrowser_ServerNotResponding" );
	}

	// auto-retry layout
	m_pAutoRetry->SetVisible( m_bShowAutoRetryToggle );

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Initiates connecting to the server once fresh info has arrived
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnConnect()
{
	// flag that we are attempting connection
	m_bConnecting = true;

	// reset state
	m_bServerFull = false;
	m_bServerNotResponding = false;

	InvalidateLayout( false, false );

	// need to refresh server before attempting to connect, to make sure there is enough room on the server
	m_iRequestRetry = vgui2::system()->GetTimeMillis() + RETRY_TIME;
	if ( m_hPingQuery != HSERVERQUERY_INVALID )
	{
		SteamMatchmakingServers()->CancelServerQuery( m_hPingQuery );
	}
	m_hPingQuery = SteamMatchmakingServers()->PingServer( m_Server.m_NetAdr.GetIP(), m_Server.m_NetAdr.GetQueryPort(), this );
}

//-----------------------------------------------------------------------------
// Purpose: Connects the user to this game
//-----------------------------------------------------------------------------
void CDialogGameInfo::Connect()
{
	OnConnect();
}

//-----------------------------------------------------------------------------
// Purpose: Closes the dialog once we have connected to this server
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnConnectToGame( int ip, int port )
{
	if ( m_Server.m_NetAdr.GetIP() == (uint32)ip && m_Server.m_NetAdr.GetConnectionPort() == port )
	{
		// we've connected to the server, close the dialog
		Close();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Forces the server info to be refreshed immediately
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnRefresh()
{
	// re-ask the server for the game info
	m_iRequestRetry = 0;
	RequestInfo();
}

//-----------------------------------------------------------------------------
// Purpose: Handles the auto-retry toggle
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnButtonToggled( vgui2::Panel *panel )
{
	if ( panel == m_pAutoRetry )
	{
		ShowAutoRetryOptions( m_pAutoRetry->IsSelected() );
	}

	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Handles the auto-retry radio buttons
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnRadioButtonChecked( vgui2::Panel *panel )
{
	OnButtonToggled( panel );
}

//-----------------------------------------------------------------------------
// Purpose: Grows or shrinks the dialog to show or hide the auto-retry options
//-----------------------------------------------------------------------------
void CDialogGameInfo::ShowAutoRetryOptions( bool state )
{
	// we need to extend the dialog
	int growSize = GetProportionalScaledValue( 60 );
	if ( !state )
	{
		growSize = -growSize;
	}

	// alter the dialog size accordingly
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );

	// load a new layout file depending on the state
	SetMinimumSize( GetProportionalScaledValue( 416 ), GetProportionalScaledValue( 340 ) );
	if ( state )
	{
		LoadControlSettings( "Servers/DialogGameInfo_AutoRetry.res", NULL );
	}
	else
	{
		LoadControlSettings( "Servers/DialogGameInfo.res", NULL );
	}

	// restore the size of the dialog
	SetBounds( x, y, wide, tall + growSize );

	// force a layout
	PerformLayout();
	m_pAutoRetryAlert->SetSelected( true );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Pings the server unless a request is already pending
//-----------------------------------------------------------------------------
void CDialogGameInfo::RequestInfo()
{
	if ( m_iRequestRetry )
		return;

	m_iRequestRetry = vgui2::system()->GetTimeMillis() + RETRY_TIME;
	if ( m_hPingQuery != HSERVERQUERY_INVALID )
	{
		SteamMatchmakingServers()->CancelServerQuery( m_hPingQuery );
	}
	m_hPingQuery = SteamMatchmakingServers()->PingServer( m_Server.m_NetAdr.GetIP(), m_Server.m_NetAdr.GetQueryPort(), this );
}

//-----------------------------------------------------------------------------
// Purpose: Re-pings the server once the retry time has elapsed
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnTick()
{
	// check to see if we should perform an auto-refresh
	if ( m_iRequestRetry && m_iRequestRetry < vgui2::system()->GetTimeMillis() )
	{
		m_iRequestRetry = 0;
		RequestInfo();
	}
}

//-----------------------------------------------------------------------------
// Purpose: The server answered our ping; update the info and connect if requested
//-----------------------------------------------------------------------------
void CDialogGameInfo::ServerResponded( gameserveritem_t &server )
{
	// make sure the response is for the server we are tracking
	if ( m_Server.m_NetAdr.GetConnectionPort() && server.m_NetAdr.GetConnectionPort() != m_Server.m_NetAdr.GetConnectionPort() )
		return;

	m_hPingQuery = HSERVERQUERY_INVALID;
	m_Server = server;

	if ( m_bConnecting )
	{
		ConnectToServer();
	}
	else if ( m_pAutoRetry->IsSelected() && server.m_nPlayers < server.m_nMaxPlayers )
	{
		// there is a slot free, we can join

		// make the sound
		vgui2::surface()->PlaySound( "Servers/game_ready.wav" );

		// flash this window
		FlashWindow();

		// if it's set, connect right away
		if ( m_pAutoRetryJoin->IsSelected() )
		{
			ConnectToServer();
		}
	}
	else
	{
		SendPlayerQuery( server.m_NetAdr.GetIP(), server.m_NetAdr.GetQueryPort() );
	}

	m_bServerNotResponding = false;

	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: The server didn't respond to our ping
//-----------------------------------------------------------------------------
void CDialogGameInfo::ServerFailedToRespond()
{
	// only mark the server as not responding if we haven't ever received a response
	if ( !m_Server.m_bHadSuccessfulResponse )
	{
		m_bServerNotResponding = true;
	}

	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Connects an already running game engine to the server
//-----------------------------------------------------------------------------
void CDialogGameInfo::ApplyConnectCommand( const gameserveritem_t &server )
{
	char command[256];

	// set the server password, if any
	if ( m_szPassword[0] )
	{
		snprintf( command, sizeof( command ), "password \"%s\"\n", m_szPassword );
		g_pRunGameEngine->AddTextCommand( command );
	}

	// send engine command to change servers
	snprintf( command, sizeof( command ), "%s %s\n", m_bHLTVOnly ? "connect_hltv" : "connect", server.m_NetAdr.GetConnectionAddressString() );
	g_pRunGameEngine->AddTextCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Builds the command line used to launch the game into this server
//-----------------------------------------------------------------------------
void CDialogGameInfo::ConstructConnectArgs( char *pchOptions, int cchOptions, const gameserveritem_t &server )
{
	snprintf( pchOptions, cchOptions, " +%s %s", m_bHLTVOnly ? "connect_hltv" : "connect", server.m_NetAdr.GetConnectionAddressString() );
	if ( m_szPassword[0] )
	{
		int iLength = strlen( pchOptions );
		snprintf( pchOptions + iLength, cchOptions - iLength, " +password \"%s\"", m_szPassword );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Connects to the server, prompting for a password or reporting errors as needed
//-----------------------------------------------------------------------------
void CDialogGameInfo::ConnectToServer()
{
	m_bConnecting = false;

	// check VAC status
	if ( m_Server.m_bSecure && ServerBrowser().IsVACBannedFromGame( m_Server.m_nAppID ) )
	{
		// refuse the user
		CVACBannedConnRefusedDialog *pDlg = new CVACBannedConnRefusedDialog( GetVParent(), "VACBannedConnRefusedDialog" );
		pDlg->Activate();
		Close();
		return;
	}

	// check to see if we need a password
	if ( m_Server.m_bPassword && !m_szPassword[0] )
	{
		CDialogServerPassword *pDlg = new CDialogServerPassword( this );
		pDlg->AddActionSignalTarget( this );
		pDlg->Activate( m_Server.GetName(), 0 );
		return;
	}

	// check the player count
	if ( m_Server.m_nPlayers >= m_Server.m_nMaxPlayers )
	{
		// mark why we cannot connect
		m_bServerFull = true;
		// give them access to auto-retry options
		m_bShowAutoRetryToggle = true;
		InvalidateLayout( false, false );
		return;
	}

	// tell the engine to connect
	if ( g_pRunGameEngine->IsRunning() )
	{
		ApplyConnectCommand( m_Server );
	}
	else
	{
		char connectArgs[256];
		ConstructConnectArgs( connectArgs, sizeof( connectArgs ), m_Server );

		IRunGameEngine::ERunResult result = g_pRunGameEngine->RunEngine( m_Server.m_nAppID, m_Server.m_szGameDir, connectArgs );
		if ( result != IRunGameEngine::k_ERunResultOkay )
		{
			vgui2::MessageBox *dlg;
			switch ( result )
			{
			case IRunGameEngine::k_ERunResultAppNotFound:
				dlg = new vgui2::MessageBox( "#ServerBrowser_GameInfoTitle", "#ServerBrowser_AppNotFound", NULL );
				break;
			case IRunGameEngine::k_ERunResultNotInitialized:
				dlg = new vgui2::MessageBox( "#ServerBrowser_GameInfoTitle", "#ServerBrowser_NotInitialized", NULL );
				break;
			case IRunGameEngine::k_ERunResultModNotInstalled:
				dlg = new vgui2::MessageBox( "#ServerBrowser_GameInfoTitle", "#ServerBrowser_ModNotInstalled", NULL );
				break;
			default:
				dlg = NULL;
				break;
			}

			if ( dlg )
			{
				dlg->DoModal( NULL );
				SetVisible( false );
				return;
			}
		}
	}

	// close this dialog
	PostMessage( this, new KeyValues( "Close" ), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Server refresh complete; nothing to do
//-----------------------------------------------------------------------------
void CDialogGameInfo::RefreshComplete( EMatchMakingServerResponse response )
{
}

//-----------------------------------------------------------------------------
// Purpose: Stores the password entered by the user and retries the connection
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnJoinServerWithPassword( const char *password )
{
	// copy out the password
	snprintf( m_szPassword, sizeof( m_szPassword ), "%s", password );

	// retry connecting to the server again
	OnConnect();
}

//-----------------------------------------------------------------------------
// Purpose: Removes all players from the list
//-----------------------------------------------------------------------------
void CDialogGameInfo::ClearPlayerList()
{
	m_pPlayerList->DeleteAllItems();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Adds a player to the player list
//-----------------------------------------------------------------------------
void CDialogGameInfo::AddPlayerToList( const char *playerName, int score, float timePlayedSeconds )
{
	if ( m_bPlayerListUpdatePending )
	{
		m_bPlayerListUpdatePending = false;
		m_pPlayerList->RemoveAll();
	}

	// only show players if the server reports some human players
	if ( m_Server.m_nPlayers - m_Server.m_nBotPlayers <= 0 )
		return;

	KeyValues *player = new KeyValues( "player" );
	player->SetString( "PlayerName", playerName );
	player->SetInt( "Score", score );
	player->SetInt( "TimeSec", (int)timePlayedSeconds );

	// construct a time string
	int seconds = (int)timePlayedSeconds;
	int minutes = seconds / 60;
	int hours = minutes / 60;
	seconds %= 60;
	minutes %= 60;

	char buf[64];
	buf[0] = 0;
	if ( hours )
	{
		snprintf( buf, sizeof( buf ), "%dh %dm %ds", hours, minutes, seconds );
	}
	else if ( minutes )
	{
		snprintf( buf, sizeof( buf ), "%dm %ds", minutes, seconds );
	}
	else
	{
		snprintf( buf, sizeof( buf ), "%ds", seconds );
	}

	player->SetString( "Time", buf );

	m_pPlayerList->AddItem( player, 0, false, true );
	player->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Starts a player details query, cancelling any query still in flight
//-----------------------------------------------------------------------------
void CDialogGameInfo::SendPlayerQuery( uint32 unIP, uint16 usQueryPort )
{
	if ( m_hPlayersQuery != HSERVERQUERY_INVALID )
	{
		SteamMatchmakingServers()->CancelServerQuery( m_hPlayersQuery );
	}

	m_hPlayersQuery = SteamMatchmakingServers()->PlayerDetails( unIP, usQueryPort, this );
	m_bPlayerListUpdatePending = true;
}

//-----------------------------------------------------------------------------
// Purpose: The player query failed; nothing to do
//-----------------------------------------------------------------------------
void CDialogGameInfo::PlayersFailedToRespond()
{
}

//-----------------------------------------------------------------------------
// Purpose: The player query is finished
//-----------------------------------------------------------------------------
void CDialogGameInfo::PlayersRefreshComplete()
{
	m_hPlayersQuery = HSERVERQUERY_INVALID;
}

//-----------------------------------------------------------------------------
// Purpose: Follows the associated friend when they change games
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnPersonaStateChange( PersonaStateChange_t *pPersonaStateChange )
{
	if ( !m_SteamIDFriend || pPersonaStateChange->m_ulSteamID != m_SteamIDFriend )
		return;

	// friend may have changed servers
	FriendGameInfo_t friendGameInfo;
	if ( !SteamFriends()->GetFriendGamePlayed( CSteamID( m_SteamIDFriend ), &friendGameInfo ) )
		return;

	if ( pPersonaStateChange->m_nChangeFlags & k_EPersonaChangeGamePlayed )
	{
		ChangeGame( friendGameInfo.m_unGameIP, friendGameInfo.m_usQueryPort, friendGameInfo.m_usGamePort );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sorts the player list by time played, longest first
//-----------------------------------------------------------------------------
int CDialogGameInfo::PlayerTimeColumnSortFunc( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 )
{
	int p1time = p1.kv->GetInt( "TimeSec", 0 );
	int p2time = p2.kv->GetInt( "TimeSec", 0 );

	if ( p1time > p2time )
		return -1;
	if ( p1time < p2time )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Always allows joining secure servers
//-----------------------------------------------------------------------------
bool JoiningSecureServerCall()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Orders challenge entries by port, then by IP address
//-----------------------------------------------------------------------------
bool QueryLessFunc( const challenge_s &item1, const challenge_s &item2 )
{
	// compare port then ip
	if ( item1.addr.GetPort() < item2.addr.GetPort() )
		return true;
	else if ( item1.addr.GetPort() > item2.addr.GetPort() )
		return false;

	int ip1 = item1.addr.GetIP();
	int ip2 = item2.addr.GetIP();

	return ip1 < ip2;
}
