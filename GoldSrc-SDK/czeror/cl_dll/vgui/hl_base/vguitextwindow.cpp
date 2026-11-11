//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "vguitextwindow.h"

#include "TextEntry.h"
#include "Button.h"
#include "game_controls/iviewport.h"

#include <string.h>

CVGUITextWindow::CVGUITextWindow( vgui2::Panel* parent )
	: vgui2::Frame( parent, "VGUITextWindow", true, false )
{
	MakePopup( true, false );
	SetVisible( false );
	// initialize dialog
	SetTitle( "", true );

	// load the new scheme early!!
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );
	SetProportional( true );

	// hide the system buttons
	SetTitleBarVisible( false );
	m_pMessage = new vgui2::TextEntry( this, "Message" );
	m_pMessage->SetEditable( false );
	m_pMessage->SetVerticalScrollbar( true );
	m_pMessage->SetEnabled( true );
	m_pOK = new vgui2::Button( this, "ok", "#PropertyDialog_OK" );
	m_pOK->SetCommand( "okay" );
	LoadControlSettings( "Resource/UI/TextWindow.res", NULL );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVGUITextWindow::~CVGUITextWindow()
{
}

void CVGUITextWindow::Activate( const char* title, const char* message )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	m_pMessage->SetText( message );
	SetVisible( true );
}

void CVGUITextWindow::Activate( const wchar_t* title, const wchar_t* message )
{
	vgui2::Frame::Activate();
	SetTitle( title, false );
	m_pMessage->SetText( message );
	SetVisible( true );
}

void CVGUITextWindow::OnCommand( const char* command )
{
	if ( !_stricmp( command, "okay" ) )
	{
		Close();
		gViewPortInterface->HideBackGround();
	}
	vgui2::Frame::OnCommand( command );
}
