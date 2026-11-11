//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "HelpDialog.h"
#include "GameUI_Interface.h"
#include "Taskbar.h"

#include "Button.h"
#include "CheckButton.h"
#include "Label.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <string.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

CHelpDialog::CHelpDialog() : BaseClass( NULL, "HelpDialog", true, false )
{
	g_pTaskbar->AddTask( GetVPanel() );
	m_pInfoText = new vgui2::Label( this, "InfoText", "#GameUI_InfoText" );
	m_pClose = new vgui2::Button( this, "CloseButton", "#GameUI_Close" );
	m_pNeverShowAgain = new vgui2::CheckButton( this, "NeverShowButton", "#GameUI_NeverShowButton" );
	SetTitle( "#GameUI_HalfLifeDesktopHelp", true );
	LoadControlSettings( "Resource/DialogHelpIngame.res", NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHelpDialog::~CHelpDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHelpDialog::OnClose()
{
	BaseClass::OnClose();
	if ( m_pNeverShowAgain->IsSelected() )
		vgui2::system()->SetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Tracker\\NeverShowHelp", 1 );

	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *close - 
//-----------------------------------------------------------------------------
void CHelpDialog::OnCommand( const char* command )
{
	if ( stricmp( command, "Close" ) == 0 )
		PostMessage( this, new KeyValues( "Close" ), 0.0f );
	else
		BaseClass::OnCommand( command );
}
