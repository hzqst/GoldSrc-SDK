//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ContentControlDialog.h"

#include "GameUI_Interface.h"
#include "Taskbar.h"

#include "Button.h"
#include "Label.h"
#include "TextEntry.h"
#include "wrect.h"
#include "cl_dll.h"
#include "vgui/IInput.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "../common/crc.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

CContentControlDialog::CContentControlDialog( vgui2::Panel* parent )
	: CTaskFrame( parent, "ContentControlDialog", true, false )
{
	SetBounds( 0, 0, 372, 160 );
	SetSizeable( false );
	SetCloseSound( "sound/UI/launch_dnmenu1.wav" );
	g_pTaskbar->AddTask( GetVPanel() );
	SetTitle( "#GameUI_ContentLock", true );

	m_pStatus = new vgui2::Label( this, "ContentStatus", "" );
	m_pPasswordLabel = new vgui2::Label( this, "PasswordPrompt", "#GameUI_PasswordPrompt" );
	m_pPassword2Label = new vgui2::Label( this, "PasswordReentryPrompt", "#GameUI_PasswordReentryPrompt" );
	m_pExplain = new vgui2::Label( this, "ContentControlExplain", "" );
	m_pPassword = new vgui2::TextEntry( this, "Password" );
	m_pPassword2 = new vgui2::TextEntry( this, "Password2" );
	m_pOK = new vgui2::Button( this, "Ok", "#GameUI_OK" );
	m_pOK->SetCommand( "Ok" );
	vgui2::Button* cancel = new vgui2::Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Cancel" );

	m_szGorePW[0] = 0;
	ResetPassword();
	LoadControlSettings( "Resource\\ContentControlDialog.res", NULL );
//	Explain("");
//	UpdateContentControlStatus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CContentControlDialog::~CContentControlDialog()
{
}

void CContentControlDialog::Activate()
{
	vgui2::Frame::Activate();
	m_pPassword->SetText( "" );
	m_pPassword->RequestFocus( 0 );
	m_pPassword2->SetText( "" );
	Explain( "" );
	UpdateContentControlStatus();
	vgui2::input()->SetAppModalSurface( GetVPanel() );
}

void CContentControlDialog::ResetPassword()
{
	if ( !vgui2::system()->GetRegistryString( "Software\\Valve\\Half-Life\\Settings\\User Token 2",
			m_szGorePW, sizeof( m_szGorePW ) ) )
		m_szGorePW[0] = 0;
	m_bSavedPasswordEnabled = IsPasswordEnabledInDialog();
}

void CContentControlDialog::ApplyPassword()
{
	if ( vgui2::system()->SetRegistryString( "Software\\Valve\\Half-Life\\Settings\\User Token 2", m_szGorePW ) )
		m_bSavedPasswordEnabled = IsPasswordEnabledInDialog();
	UpdateContentControlStatus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::Explain( const char* format, ... )
{
	if ( !m_pExplain )
		return;

	char text[1024];
	va_list args;
	va_start( args, format );
#ifdef _WIN32
	_vsnprintf( text, sizeof( text ), format, args );
#else
	vsnprintf( text, sizeof( text ), format, args );
#endif
	va_end( args );
	m_pExplain->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CContentControlDialog::OnCommand( const char* command )
{
	if ( !stricmp( command, "Ok" ) )
	{
		char password[256], repeatedPassword[256];
		m_pPassword->GetText( password, sizeof( password ) );
		m_pPassword2->GetText( repeatedPassword, sizeof( repeatedPassword ) );
		if ( IsPasswordEnabledInDialog() )
		{
			if ( !DisablePassword( password ) )
				return;
//            canclose = CheckPassword( m_szGorePW, pw1, false );
		}
		else
		{
			if ( strcmp( password, repeatedPassword ) )
			{
				Explain( "#GameUI_PasswordsDontMatch" );
				return;
			}

			if ( !EnablePassword( password ) )
				return;
		}

		OnClose();
	}
	else if ( !stricmp( command, "Cancel" ) )
	{
		OnClose();
	}
	else
	{
		vgui2::Frame::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::OnClose()
{
	vgui2::Frame::OnClose();
	PostActionSignal( new KeyValues( "ContentControlClose" ) );
//	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char* CContentControlDialog::BinPrintf( unsigned char* buffer, int length )
{
	static char text[1024];
	char chunk[10];
	memset( text, 0, sizeof( text ) );
	for ( int i = 0; i < length; i++ )
	{
		unsigned char value = buffer[i];
#ifdef _WIN32
		_snprintf( chunk, sizeof( chunk ), "%02x", value );
#else
		snprintf( chunk, sizeof( chunk ), "%02x", value );
#endif
#ifdef _MSC_VER
#pragma warning( suppress: 6053 6054 )
#endif
		strncat( text, chunk, sizeof( text ) - strlen( text ) - 1 );
	}

	return text;
}

void CContentControlDialog::WriteToken( const char* text )
{
	if ( !vgui2::system()->SetRegistryString( "Software\\Valve\\Half-Life\\Settings\\User Token 2", text ) )
		return;
	if ( text != m_szGorePW )
		strcpy( m_szGorePW, text );
	m_bSavedPasswordEnabled = IsPasswordEnabledInDialog();

	UpdateContentControlStatus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::HashPassword( const char* password, char* hashBuffer )
{
	MD5Context_t context;
	unsigned char digest[16];
	MD5Init( &context );
	MD5Update( &context, (const unsigned char*)password, strlen( password ) );
	MD5Final( digest, &context );
	strcpy( hashBuffer, BinPrintf( digest, sizeof( digest ) ) );
}

bool CContentControlDialog::EnablePassword( const char* password )
{
	if ( !password[0] )
	{
		Explain( "#GameUI_MustEnterPassword" );
		return false;
	}

	char hash[128];
	HashPassword( password, hash );
	engine->Cvar_SetValue( "violence_hblood", 0.0f );
	engine->Cvar_SetValue( "violence_hgibs", 0.0f );
	engine->Cvar_SetValue( "violence_ablood", 0.0f );
	engine->Cvar_SetValue( "violence_agibs", 0.0f );
	// Store digest to registry
//    WriteToken( digestedPW );
	strcpy( m_szGorePW, hash );
	return true;
}

bool CContentControlDialog::DisablePassword( const char* password )
{
	if ( !password[0] )
	{
		Explain( "#GameUI_MustEnterPassword" );
		return false;
	}

	char hash[128];
	HashPassword( password, hash );
	if ( stricmp( m_szGorePW, hash ) )
	{
		Explain( "#GameUI_IncorrectPassword" );
		return false;
	}

	m_szGorePW[0] = 0;
	engine->Cvar_SetValue( "violence_hblood", 1.0f );
	engine->Cvar_SetValue( "violence_hgibs", 1.0f );
	engine->Cvar_SetValue( "violence_ablood", 1.0f );
	engine->Cvar_SetValue( "violence_agibs", 1.0f );
//		// Remove digest value
//		WriteToken( "" );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CContentControlDialog::IsPasswordEnabledInDialog()
{
	return m_szGorePW[0] != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CContentControlDialog::UpdateContentControlStatus()
{
	bool enabled = IsPasswordEnabledInDialog();
	m_pStatus->SetText( enabled ? "#GameUI_ContentStatusEnabled" : "#GameUI_ContentStatusDisabled" );
	m_pPasswordLabel->SetText( enabled ? "#GameUI_PasswordDisablePrompt" : "#GameUI_PasswordPrompt" );
	// hide the re-entry
	m_pPassword2Label->SetVisible( !enabled );
	m_pPassword2->SetVisible( !enabled );
}
