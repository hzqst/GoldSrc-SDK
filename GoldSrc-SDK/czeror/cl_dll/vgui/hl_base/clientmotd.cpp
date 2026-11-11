//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "clientmotd.h"

#include "HTML.h"
#include "Button.h"
#include "Label.h"
#include "controls.h"
#include "FileSystem.h"
#include "port.h"
#include "../../../../engine/controls.h"

#include <string.h>
#include <wchar.h>

CClientMOTD::CClientMOTD( vgui2::Panel* parent )
	: vgui2::Frame( parent, "ClientMOTD", true, false ), m_bFileWritten( false )
{
	strcpy( m_szTempFileName, "Resource/motd_temp.html" );
	MakePopup( true, false );
	SetVisible( false );

	// initialize dialog
	SetTitle( "", true );

	// load the new scheme early!!
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );
	SetProportional( true );
	m_pMessage = new vgui2::HTML( this, "Message", false, false );
	m_pOK = new vgui2::Button( this, "ok", "#PropertyDialog_OK" );
	m_pOK->SetCommand( "okay" );
	LoadControlSettings( "Resource/UI/MOTD.res", NULL );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CClientMOTD::~CClientMOTD()
{
	if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
		vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAME" );
}

bool CClientMOTD::IsURL( const char* text )
{
	return strlen( text ) > 7 && !strncmp( text, "http://", 7 );
}

void CClientMOTD::SetLabelText( const char* entryName, const wchar_t* text )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( entryName, false ) );
	if ( label )
		label->SetText( text );
}

void CClientMOTD::Activate( const char* title, const char* message )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	SetControlString( "serverName", title );
	if ( IsURL( message ) )
	{
		m_pMessage->OpenURL( message, "", NULL );
	}
	else
	{
		if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
			vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAME" );
		FileHandle_t file = vgui2::filesystem()->Open( m_szTempFileName, "w+", "GAME" );
		if ( file )
		{
			vgui2::filesystem()->Write( message, (int)strlen( message ), file );
			vgui2::filesystem()->Close( file );
			char localURL[MAX_PATH + 7];
			strcpy( localURL, "file:///" );
			vgui2::filesystem()->GetLocalPath( m_szTempFileName, localURL + strlen( localURL ),
				(int)( sizeof( localURL ) + strlen( localURL ) ) );
			m_pMessage->OpenURL( localURL, "", NULL );
		}
	}
	SetVisible( true );
}

void CClientMOTD::Activate( const wchar_t* title, const wchar_t* message )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	SetLabelText( "serverName", title );
	char ansiURL[256];
	vgui2::localize()->ConvertUnicodeToANSI( message, ansiURL, sizeof( ansiURL ) );
	if ( IsURL( ansiURL ) )
	{
		m_pMessage->OpenURL( ansiURL, "", NULL );
	}
	else
	{
		if ( vgui2::filesystem()->FileExists( m_szTempFileName ) )
			vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAME" );
		FileHandle_t file = vgui2::filesystem()->Open( m_szTempFileName, "w+", "GAME" );
		if ( file )
		{
			vgui2::filesystem()->Write( message, (int)wcslen( message ), file );
			vgui2::filesystem()->Close( file );
			char localURL[MAX_PATH + 7];
			strcpy( localURL, "file:///" );
			vgui2::filesystem()->GetLocalPath( m_szTempFileName, localURL + strlen( localURL ),
				(int)( sizeof( localURL ) + strlen( localURL ) ) );
			m_pMessage->OpenURL( localURL, "", NULL );
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
			vgui2::filesystem()->RemoveFile( m_szTempFileName, "GAME" );
		Close();
	}
	vgui2::Frame::OnCommand( command );
}

void CClientMOTD::OnKeyCodeTyped( vgui2::KeyCode code )
{
	vgui2::Frame::OnKeyCodeTyped( code );
	if ( code == vgui2::KEY_SPACE )
		OnCommand( "okay" );
}
