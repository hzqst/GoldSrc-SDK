//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "DialogAddServer.h"
#include "ServerBrowser.h"
#include "ServerBrowserDialog.h"

#include "vgui/IScheme.h"
#include "Button.h"
#include "vgui_controls/Controls.h"
#include "ImageList.h"
#include "ListPanel.h"
#include "MessageBox.h"
#include "PropertySheet.h"
#include "TextEntry.h"
#include "vgui/KeyValues.h"
#include "netadr.h"
#include "../engine/controls.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDialogAddServer::CDialogAddServer( vgui2::Panel *parent, IGameList *gameList ) : vgui2::Frame( parent, "DialogAddServer", true, false )
{
	SetDeleteSelfOnClose( true );

	m_pGameList = gameList;

	SetTitle( "#ServerBrowser_AddServersTitle", true );
	SetSizeable( false );

	m_pTabPanel = new vgui2::PropertySheet( this, "GameTabs" );
	m_pTabPanel->SetTabWidth( GetProportionalScaledValue( 72 ) );

	m_pDiscoveredGames = new vgui2::ListPanel( this, "Servers" );

	// Add the column headers
	m_pDiscoveredGames->AddColumnHeader( 0, "Password", "#ServerBrowser_Password", GetProportionalScaledValue( 16 ), 5 );
	m_pDiscoveredGames->AddColumnHeader( 1, "Bots", "#ServerBrowser_Bots", GetProportionalScaledValue( 16 ), 5 );
	m_pDiscoveredGames->AddColumnHeader( 2, "Secure", "#ServerBrowser_Secure", GetProportionalScaledValue( 16 ), 5 );
	m_pDiscoveredGames->AddColumnHeader( 3, "Players", "#ServerBrowser_Players", GetProportionalScaledValue( 60 ), 0 );
	m_pDiscoveredGames->AddColumnHeader( 4, "Name", "#ServerBrowser_Servers", GetProportionalScaledValue( 20 ), 18 );
	m_pDiscoveredGames->AddColumnHeader( 5, "IPAddr", "#ServerBrowser_IPAddress", GetProportionalScaledValue( 60 ), 8 );
	m_pDiscoveredGames->AddColumnHeader( 6, "GameDesc", "#ServerBrowser_Game", GetProportionalScaledValue( 150 ), 0 );
	m_pDiscoveredGames->AddColumnHeader( 7, "Map", "#ServerBrowser_Map", GetProportionalScaledValue( 80 ), 0 );
	m_pDiscoveredGames->AddColumnHeader( 8, "Ping", "#ServerBrowser_Latency", GetProportionalScaledValue( 60 ), 0 );

	m_pDiscoveredGames->SetColumnHeaderTooltip( 0, "#ServerBrowser_PasswordColumn_Tooltip" );
	m_pDiscoveredGames->SetColumnHeaderTooltip( 1, "#ServerBrowser_BotColumn_Tooltip" );
	m_pDiscoveredGames->SetColumnHeaderTooltip( 2, "#ServerBrowser_SecureColumn_Tooltip" );

	// setup fast sort functions
	m_pDiscoveredGames->SetSortColumn( 8 );

	m_pTextEntry = new vgui2::TextEntry( this, "ServerNameText" );
	m_pTextEntry->AddActionSignalTarget( this );

	m_pTestServersButton = new vgui2::Button( this, "TestServersButton", "" );
	m_pAddServerButton = new vgui2::Button( this, "OKButton", "" );
	m_pAddSelectedServerButton = new vgui2::Button( this, "SelectedOKButton", "" );
	m_pAddSelectedServerButton->SetCommand( "addselected" );
	m_pAddSelectedServerButton->AddActionSignalTarget( this );

	m_pTabPanel->AddPage( m_pDiscoveredGames, "#ServerBrowser_Servers" );

	LoadControlSettings( "Servers/DialogAddServer.res", NULL );

	// Setup the buttons. We leave them disabled until there is text in the textbox
	m_pAddServerButton->SetEnabled( false );
	m_pTestServersButton->SetEnabled( false );
	m_pAddSelectedServerButton->SetEnabled( false );
	m_pAddSelectedServerButton->SetVisible( false );
	m_pTabPanel->SetVisible( false );

	m_pTextEntry->RequestFocus( 0 );

	// Initially, we aren't high enough to show the tab panel
	int x, y;
	m_pTabPanel->GetPos( x, y );
	m_OriginalHeight = y + m_pTabPanel->GetTall() + GetProportionalScaledValue( 50 );
	SetTall( y );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDialogAddServer::~CDialogAddServer()
{
	for ( int i = 0; i < m_Queries.Count(); i++ )
	{
		SteamMatchmakingServers()->CancelServerQuery( m_Queries[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Enables the add/test buttons only when an address has been entered
//-----------------------------------------------------------------------------
void CDialogAddServer::OnTextChanged()
{
	bool bAnyText = ( m_pTextEntry->GetTextLength() > 0 );
	m_pAddServerButton->SetEnabled( bAnyText );
	m_pTestServersButton->SetEnabled( bAnyText );
}

//-----------------------------------------------------------------------------
// Purpose: button command handler
//-----------------------------------------------------------------------------
void CDialogAddServer::OnCommand( const char *command )
{
	if ( !stricmp( command, "OK" ) )
	{
		OnOK();
	}
	else if ( !stricmp( command, "TestServers" ) )
	{
		SetTall( m_OriginalHeight );
		m_pTabPanel->SetVisible( true );
		m_pAddSelectedServerButton->SetVisible( true );

		TestServers();
	}
	else if ( !stricmp( command, "addselected" ) )
	{
		int itemID = m_pDiscoveredGames->GetSelectedItem( 0 );
		if ( m_pDiscoveredGames->IsValidItemID( itemID ) )
		{
			// get the server
			int serverID = m_pDiscoveredGames->GetItemUserData( itemID );
			if ( m_Servers.IsValidIndex( serverID ) )
			{
				ServerBrowserDialog().AddServerToFavorites( m_Servers[serverID] );
			}
			m_pDiscoveredGames->RemoveItem( itemID );
			m_pDiscoveredGames->SetEmptyListText( "" );
		}

		// removing the row doesn't post ItemSelected, so update the button here
		m_pAddSelectedServerButton->SetEnabled( m_pDiscoveredGames->GetSelectedItem( 0 ) != -1 );
	}
	else
	{
		vgui2::Frame::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the OK button being pressed; adds the server to the game list
//-----------------------------------------------------------------------------
void CDialogAddServer::OnOK()
{
	// try and parse out IP address
	const char *address = GetControlString( "ServerNameText", "" );
	netadr_t netaddr;
	netaddr.SetFromString( address, true );

	uint16 iPort = netaddr.GetPort();
	if ( !iPort )
	{
		netaddr.SetPort( 27015 );
		iPort = 27015;
	}

	if ( netaddr.GetType() != 0 && netaddr.GetIP() != 0 )
	{
		gameserveritem_t server;
		memset( &server, 0, sizeof( server ) );
		server.SetName( address );
		server.m_NetAdr.Init( netaddr.GetIP(), iPort, iPort );

		ServerBrowserDialog().AddServerToFavorites( server );
	}
	else
	{
		// could not parse the ip address, popup an error
		vgui2::MessageBox *dlg = new vgui2::MessageBox( "#ServerBrowser_AddServerErrorTitle", "#ServerBrowser_AddServerError", NULL );
		dlg->DoModal( NULL );
	}

	// mark ourselves to be closed
	PostMessage( this, new KeyValues( "Close" ), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Pings the entered address (on every common query port if none was given)
//-----------------------------------------------------------------------------
void CDialogAddServer::TestServers()
{
	// cancel pings still pending from the previous test, their late replies would index the new round
	for ( int i = 0; i < m_Queries.Count(); i++ )
	{
		SteamMatchmakingServers()->CancelServerQuery( m_Queries[i] );
	}
	m_Queries.RemoveAll();

	// rows store indices into m_Servers, so both must be cleared together
	m_pDiscoveredGames->RemoveAll();
	m_Servers.RemoveAll();
	m_pAddSelectedServerButton->SetEnabled( false );

	m_pDiscoveredGames->SetEmptyListText( "" );

	const char *address = GetControlString( "ServerNameText", "" );
	netadr_t netaddr;
	netaddr.SetFromString( address, true );

	CUtlVector<netadr_t> vecAdress;
	if ( netaddr.GetPort() )
	{
		vecAdress.AddToTail( netaddr );
	}
	else
	{
		CUtlVector<unsigned short> portsToTry;
		GetMostCommonQueryPorts( portsToTry );
		for ( int i = 0; i < portsToTry.Count(); i++ )
		{
			netadr_t newAddr = netaddr;
			newAddr.SetPort( portsToTry[i] );
			vecAdress.AddToTail( newAddr );
		}
	}

	// Change the text on the tab panel..
	m_pTabPanel->RemoveAllPages();

	wchar_t wstr[512];
	wchar_t waddress[512];
	if ( address[0] )
	{
		vgui2::localize()->ConvertANSIToUnicode( address, waddress, sizeof( waddress ) );
		vgui2::localize()->ConstructString( wstr, sizeof( wstr ), vgui2::localize()->Find( "#ServerBrowser_ServersResponding" ), 1, waddress );
	}
	else
	{
		wcsncpy( wstr, vgui2::localize()->Find( "#ServerBrowser_ServersRespondingLocal" ), ( sizeof( wstr ) / sizeof( wstr[0] ) ) - 1 );
		wstr[( sizeof( wstr ) / sizeof( wstr[0] ) ) - 1] = 0;
	}

	// each wide character needs at most 4 UTF-8 bytes
	CUtlVector<char> str;
	str.AddMultipleToTail( (int)wcslen( wstr ) * 4 + 1 );
	if ( vgui2::localize()->ConvertUnicodeToANSI( wstr, str.Base(), str.Count() ) <= 0 )
	{
		str[0] = 0;
	}
	m_pTabPanel->AddPage( m_pDiscoveredGames, str.Base() );
	m_pTabPanel->InvalidateLayout( false, false );

	// Start the servers refreshing
	for ( int i = 0; i < vecAdress.Count(); i++ )
	{
		m_Queries.AddToTail( SteamMatchmakingServers()->PingServer( vecAdress[i].GetIP(), vecAdress[i].GetPort(), this ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: A server answered our ping; add it to the discovered games list
//-----------------------------------------------------------------------------
void CDialogAddServer::ServerResponded( gameserveritem_t &server )
{
	KeyValues *kv = new KeyValues( "Server" );

	kv->SetString( "name", server.GetName() );
	kv->SetString( "map", server.m_szMap );
	kv->SetString( "GameDir", server.m_szGameDir );
	kv->SetString( "GameDesc", server.m_szGameDescription );
	kv->SetString( "GameTags", server.m_szGameTags );
	kv->SetInt( "password", server.m_bPassword );
	kv->SetInt( "bots", server.m_nBotPlayers ? 2 : 0 );

	if ( server.m_bSecure )
	{
		// show the denied icon if banned from secure servers, the secure icon otherwise
		kv->SetInt( "secure", ServerBrowser().IsVACBannedFromGame( server.m_nAppID ) ? 4 : 3 );
	}
	else
	{
		kv->SetInt( "secure", 0 );
	}

	netadr_t reportedIPAddr( server.m_NetAdr.GetIP(), server.m_NetAdr.GetConnectionPort() );
	kv->SetString( "IPAddr", reportedIPAddr.ToString( false ) );

	char buf[32];
	snprintf( buf, sizeof( buf ), "%d / %d", server.m_nPlayers, server.m_nMaxPlayers );
	kv->SetString( "Players", buf );

	kv->SetInt( "Ping", server.m_nPing );

	int iServer = m_Servers.AddToTail( server );
	int iListID = m_pDiscoveredGames->AddItem( kv, iServer, false, false );
	if ( m_pDiscoveredGames->GetItemCount() == 1 )
	{
		m_pDiscoveredGames->SetSingleSelectedItem( iListID );
	}
	kv->deleteThis();

	m_pDiscoveredGames->InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: The ping failed; tell the user the server did not respond
//-----------------------------------------------------------------------------
void CDialogAddServer::ServerFailedToRespond()
{
	m_pDiscoveredGames->SetEmptyListText( "#ServerBrowser_ServerNotResponding" );
}

//-----------------------------------------------------------------------------
// Purpose: Enables the "add selected" button when a discovered server is selected
//-----------------------------------------------------------------------------
void CDialogAddServer::OnItemSelected()
{
	m_pAddSelectedServerButton->SetEnabled( m_pDiscoveredGames->GetSelectedItem( 0 ) != -1 );
}

//-----------------------------------------------------------------------------
// Purpose: Loads the list icons and font from the scheme
//-----------------------------------------------------------------------------
void CDialogAddServer::ApplySchemeSettings( vgui2::IScheme *pScheme )
{
	vgui2::Frame::ApplySchemeSettings( pScheme );

	vgui2::ImageList *imageList = new vgui2::ImageList( false );
	imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_password", false ) );
	imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_bots", false ) );
	imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_robotron", false ) );
	imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_secure_deny", false ) );

	int passwordColumnImage = imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_password_column", false ) );
	int botColumnImage = imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_bots_column", false ) );
	int secureColumnImage = imageList->AddImage( vgui2::scheme()->GetImage( "servers/icon_robotron_column", false ) );

	m_pDiscoveredGames->SetImageList( imageList, true );

	vgui2::HFont hFont = pScheme->GetFontEx( "ListSmall", IsProportional(), IsHDProportional() );
	if ( !hFont )
	{
		hFont = pScheme->GetFontEx( "DefaultSmall", IsProportional(), IsHDProportional() );
	}
	m_pDiscoveredGames->SetFont( hFont );

	m_pDiscoveredGames->SetColumnHeaderImage( 0, passwordColumnImage );
	m_pDiscoveredGames->SetColumnHeaderImage( 1, botColumnImage );
	m_pDiscoveredGames->SetColumnHeaderImage( 2, secureColumnImage );
}
