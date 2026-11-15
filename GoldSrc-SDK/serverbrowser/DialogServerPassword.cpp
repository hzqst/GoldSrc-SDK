//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "DialogServerPassword.h"

#include "vgui/KeyValues.h"
#include "Button.h"
#include "Label.h"
#include "TextEntry.h"


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDialogServerPassword::CDialogServerPassword( vgui2::Panel *parent )
	: vgui2::Frame( parent, "DialogServerPassword", true, false ), m_iServerID( -1 )
{
	SetSize( 320, 240 );
	SetDeleteSelfOnClose( true );
	SetSizeable( false );

	m_pInfoLabel = new vgui2::Label( this, "InfoLabel", "#ServerBrowser_ServerRequiresPassword" );
	m_pGameLabel = new vgui2::Label( this, "GameLabel", "<game label>" );
	m_pPasswordEntry = new vgui2::TextEntry( this, "PasswordEntry" );
	m_pConnectButton = new vgui2::Button( this, "ConnectButton", "#ServerBrowser_Connect" );
	m_pPasswordEntry->SetTextHidden( true );

	LoadControlSettings( "Servers/DialogServerPassword.res", NULL );
	SetTitle( "#ServerBrowser_ServerRequiresPasswordTitle", true );
	MoveToCenterOfScreen();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDialogServerPassword::~CDialogServerPassword()
{
}

//-----------------------------------------------------------------------------
// Purpose: Shows the dialog for the specified server and focuses the password field
//-----------------------------------------------------------------------------
void CDialogServerPassword::Activate( const char *serverName, unsigned int serverID )
{
	m_pGameLabel->SetText( serverName );
	m_iServerID = serverID;
	m_pConnectButton->SetAsDefaultButton( 1 );
	m_pPasswordEntry->RequestFocus( 0 );
	vgui2::Frame::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Posts the entered password to the action target on "Connect", then closes
//-----------------------------------------------------------------------------
void CDialogServerPassword::OnCommand( const char *command )
{
	if ( !stricmp( command, "Connect" ) )
	{
		KeyValues *msg = new KeyValues( "JoinServerWithPassword" );
		char szPassword[64];
		m_pPasswordEntry->GetText( szPassword, sizeof( szPassword ) - 1 );
		msg->SetString( "password", szPassword );
		msg->SetInt( "serverID", m_iServerID );
		PostActionSignal( msg );
	}
	else if ( stricmp( command, "Close" ) )
	{
		vgui2::Frame::OnCommand( command );
		return;
	}

	PostMessage( this, new KeyValues( "Close" ), 0.0f );
}
