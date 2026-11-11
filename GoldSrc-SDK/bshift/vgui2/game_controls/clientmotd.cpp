//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "clientmotd.h"
#include "HTML.h"
#include "Label.h"
#include "controls.h"
#include "../../../engine/controls.h"
#include "../../../public/FileSystem.h"
#include "VGUI/IViewPort.h"
#include "IGameUIFuncs.h" // for key bindings
#include <string.h>
#include <wchar.h>

extern IGameUIFuncs* gameuifuncs; // for key binding details

class ClientMOTDHTML : public vgui2::HTML
{
public:
	ClientMOTDHTML( vgui2::Panel* parent, const char* name ) : vgui2::HTML( parent, name, false, false )
	{
	}
	bool OnStartURL( const char* url, const char*, bool )
	{
		return strnicmp( "steam://", url, 8 ) != 0;
	}
	bool OnStartRequest( const char* url, const char* target, const char*, bool first ) override
	{
		return OnStartURL( url, target, first );
	}
};

CClientMOTD::CClientMOTD( vgui2::Panel* parent ) :
	vgui2::Frame( parent, "ClientMOTD", true, false ), m_bFileWritten( false ), m_iScoreBoardKey( 0 )
{
	strcpy( m_szTempFileName, "motd_temp.html" );

	// initialize dialog
	SetTitle( "", true );

	// load the new scheme early!!
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );
	SetProportional( true );
	m_pMessage = new ClientMOTDHTML( this, "Message" );
	LoadControlSettings( "Resource/UI/MOTD.res", NULL );
	InvalidateLayout( false, false );
	m_pServerName = new vgui2::Label( this, "serverName", "" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CClientMOTD::~CClientMOTD()
{
	if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
		vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
}

bool CClientMOTD::IsURL( const char* str )
{
	return strlen( str ) > 7 && str[0] == 'h' && str[1] == 't' && str[2] == 't' && str[3] == 'p' &&
		str[4] == ':' && str[5] == '/' && str[6] == '/';
}

void CClientMOTD::Activate( const char* title, const char* msg )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	SetControlString( "serverName", title );
	if ( IsURL( msg ) )
		m_pMessage->OpenURL( msg, NULL, false );
	else
	{
		if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
			vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
		if ( strstr( msg, "img src=\"view-source:" ) || strstr( msg, "<style>;@/*" ) )
			return;
		FileHandle_t file = vgui2::filesystem()->Open( m_szTempFileName, "w+", "GAMECONFIG" );
		if ( file )
		{
			vgui2::filesystem()->Write( msg, (int)strlen( msg ), file );
			vgui2::filesystem()->Close( file );
			char localURL[268] = "file:///";
			size_t length = strlen( localURL );
			vgui2::filesystem()->GetLocalPath( m_szTempFileName, localURL + length, (int)( 267 - length ) );
			m_pMessage->OpenURL( localURL, NULL, false );
		}
	}
	if ( !m_iScoreBoardKey )
		m_iScoreBoardKey = gameuifuncs->GetVGUI2KeyCodeForBind( "showscores" );
}

void CClientMOTD::Activate( const wchar_t* title, const wchar_t* msg )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	m_pServerName->SetText( title );
	char ansiURL[284];
	vgui2::localize()->ConvertUnicodeToANSI( msg, ansiURL, 256 );
	if ( IsURL( ansiURL ) )
		m_pMessage->OpenURL( ansiURL, NULL, false );
	else
	{
		if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
			vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
		FileHandle_t file = vgui2::filesystem()->Open( m_szTempFileName, "w+", "GAMECONFIG" );
		if ( file )
		{
			vgui2::filesystem()->Write( msg, (int)wcslen( msg ), file );
			vgui2::filesystem()->Close( file );
			char localURL[268] = "file:///";
			size_t length = strlen( localURL );
			vgui2::filesystem()->GetLocalPath( m_szTempFileName, localURL + length, (int)( sizeof( localURL ) - length ) );
			m_pMessage->OpenURL( localURL, NULL, false );
		}
	}
	SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: called when a button is pressed
//-----------------------------------------------------------------------------
void CClientMOTD::OnCommand( const char* command )
{
	if ( !stricmp( command, "okay" ) )
	{
		if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
			vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
		Close();
	}
	vgui2::Frame::OnCommand( command );
}

void CClientMOTD::OnKeyCodeTyped( vgui2::KeyCode key )
{
	if ( key == vgui2::KEY_PAD_ENTER || key == vgui2::KEY_SPACE )
		OnCommand( "okay" );
	else if ( m_iScoreBoardKey && m_iScoreBoardKey == key )
	{
		if ( !gViewPortInterface->IsScoreBoardVisible() )
		{
			gViewPortInterface->HideBackGround();
			gViewPortInterface->ShowScoreBoard();
			SetVisible( false );
		}
	}
	else
		vgui2::Frame::OnKeyCodeTyped( key );
}

void CClientMOTD::SetLabelText( const char* textEntryName, const wchar_t* text )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( textEntryName, false ) );
	if ( label )
		label->SetText( text );
}
