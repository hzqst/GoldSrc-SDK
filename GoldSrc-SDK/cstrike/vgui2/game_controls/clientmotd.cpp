//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "clientmotd.h"
#include "vgui_controls/HTML.h"
#include "../../../vgui2/controls/Label.h"
#include "vgui_controls/Controls.h"
#include "../../../public/FileSystem.h"
#include "iviewport.h"
#include "igameuifuncs.h"
#include "vgui/ILocalize.h"
#include <string.h>
#include <wchar.h>

extern IGameUIFuncs* gameuifuncs; // for key binding details

class ClientMOTDHTML : public vgui2::HTML
{
public:
	ClientMOTDHTML( vgui2::Panel* parent, const char* name ) : vgui2::HTML( parent, name, false, false )
	{
	}

	virtual bool OnStartURL( const char* url, const char*, bool )
	{
		return _strnicmp( "steam://", url, 8 ) != 0;
	}
};

CClientMOTD::CClientMOTD( vgui2::Panel* parent ) : vgui2::Frame( parent, "ClientMOTD", true, false )
{
	strcpy( m_szTempFileName, "motd_temp.html" );
	m_bFileWritten = false;
	m_iScoreBoardKey = 0;

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

bool CClientMOTD::IsURL( const char* text )
{
	return strlen( text ) > 7 && text[0] == 'h' && text[1] == 't' && text[2] == 't'
		&& text[3] == 'p' && text[4] == ':' && text[5] == '/' && text[6] == '/';
}

void CClientMOTD::Activate( const char* title, const char* message )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	SetControlString( "serverName", title );
	if ( IsURL( message ) )
		m_pMessage->OpenURL( message, NULL, false );
	else
	{
		if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
			vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
		if ( strstr( message, "img src=\"view-source:" ) || strstr( message, "<style>;@/*" ) )
			return;

		FileHandle_t file = vgui2::filesystem()->Open( m_szTempFileName, "w+", "GAMECONFIG" );
		if ( file )
		{
			vgui2::filesystem()->Write( message, (int)strlen( message ), file );
			vgui2::filesystem()->Close( file );
			char localURL[4103] = "file:///";
			size_t length = strlen( localURL );
			vgui2::filesystem()->GetLocalPath( m_szTempFileName, localURL + length,
				(int)( sizeof( localURL ) - length ) );
			m_pMessage->OpenURL( localURL, NULL, false );
		}
	}
	if ( !m_iScoreBoardKey )
		m_iScoreBoardKey = gameuifuncs->GetVGUI2KeyCodeForBind( "showscores" );
}

void CClientMOTD::Activate( const wchar_t* title, const wchar_t* message )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	m_pServerName->SetText( title );
	char ansiURL[256];
	vgui2::localize()->ConvertUnicodeToANSI( message, ansiURL, sizeof( ansiURL ) );
	if ( IsURL( ansiURL ) )
		m_pMessage->OpenURL( ansiURL, NULL, false );
	else
	{
		if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
			vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
		FileHandle_t file = vgui2::filesystem()->Open( m_szTempFileName, "w+", "GAMECONFIG" );
		if ( file )
		{
			vgui2::filesystem()->Write( message, (int)wcslen( message ), file );
			vgui2::filesystem()->Close( file );
			char localURL[4103] = "file:///";
			size_t length = strlen( localURL );
			vgui2::filesystem()->GetLocalPath( m_szTempFileName, localURL + length,
				(int)( sizeof( localURL ) + length ) );
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
	if ( !_stricmp( command, "okay" ) )
	{
		if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
			vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
		Close();
	}
	vgui2::Frame::OnCommand( command );
}

void CClientMOTD::OnKeyCodeTyped( vgui2::KeyCode key )
{
	if ( key == vgui2::KEY_SPACE || key == vgui2::KEY_PAD_ENTER )
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

void CClientMOTD::SetLabelText( const char* name, const wchar_t* text )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( name, false ) );
	if ( label )
		label->SetText( text );
}
