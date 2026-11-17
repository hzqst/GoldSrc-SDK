//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "clientmotd.h"
#include "HTML.h"
#include "../../vgui2/controls/Label.h"
#include "../controls/controls.h"
#include "../../engine/controls.h"
#include "../../public/FileSystem.h"
#include "iviewport.h"
#include "igameuifuncs.h"
#include "../../engine/ILocalize.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

extern IGameUIFuncs* gameuifuncs; // for key binding details

// UTF-8 needs at most four bytes for each wide character
static const size_t MaxMultiByteCharLength = 4;

class ClientMOTDHTML : public vgui2::HTML
{
public:
	ClientMOTDHTML( vgui2::Panel* parent, const char* name ) : vgui2::HTML( parent, name, false, false )
	{
	}

	bool OnStartRequest( const char* url, const char*, const char*, bool ) override
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
	if ( !ShowMessage( message ) )
		return;
	if ( !m_iScoreBoardKey )
		m_iScoreBoardKey = gameuifuncs->GetVGUI2KeyCodeForBind( "showscores" );
}

void CClientMOTD::Activate( const wchar_t* title, const wchar_t* message )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	m_pServerName->SetText( title );

	// convert the whole message so local pages are written as text, not raw wide characters
	const size_t capacity = wcslen( message ) * MaxMultiByteCharLength + 1;
	if ( capacity > INT_MAX )
		return;
	char* text = (char*)malloc( capacity );
	if ( !text )
		return;
	vgui2::localize()->ConvertUnicodeToANSI( message, text, (int)capacity );
	text[capacity - 1] = 0;
	const bool shown = ShowMessage( text );
	free( text );
	if ( shown )
		SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: opens a URL directly, or writes the message to a local page and opens that
//-----------------------------------------------------------------------------
bool CClientMOTD::ShowMessage( const char* message )
{
	if ( IsURL( message ) )
	{
		m_pMessage->OpenURL( message, NULL, false );
		return true;
	}

	if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
		vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAMECONFIG" );
	if ( strstr( message, "img src=\"view-source:" ) || strstr( message, "<style>;@/*" ) )
		return false;

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
	return true;
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
