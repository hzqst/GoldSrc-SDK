//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "OptionsDialog.h"

#include "GameUI_Interface.h"
#include "ModInfo.h"
#include "MultiplayerAdvancedDialog.h"
#include "OptionsSubAdvanced.h"
#include "OptionsSubAudio.h"
#include "OptionsSubKeyboard.h"
#include "OptionsSubMouse.h"
#include "OptionsSubMultiplayer.h"
#include "OptionsSubVideo.h"
#include "OptionsSubVoice.h"
#include "Taskbar.h"

#include "PropertySheet.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/Controls.h"

#include <string.h>
#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
COptionsDialog::COptionsDialog( vgui2::Panel* parent )
	: vgui2::PropertyDialog( parent, "OptionsDialog" )
{
	SetHDProportional( true );
	int tall = GetProportionalScaledValue( 444 );
	int wide = GetProportionalScaledValue( 522 );
	SetBounds( 0, 0, wide, tall );
	SetCloseSound( "sound/UI/launch_dnmenu1.wav" );
	g_pTaskbar->AddTask( GetVPanel() );
	SetTitle( "#GameUI_Options", true );

	if ( ModInfo()->IsMultiplayerOnly() && !ModInfo()->IsSinglePlayerOnly() )
		AddPage( new COptionsSubMultiplayer( this ), "#GameUI_Multiplayer" );

	AddPage( new COptionsSubAudio( this ), "#GameUI_Audio" );
	AddPage( new COptionsSubVideo( this ), "#GameUI_Video" );
	AddPage( new COptionsSubMouse( this ), "#GameUI_Mouse" );
	AddPage( new COptionsSubKeyboard( this ), "#GameUI_Keyboard" );

	if ( !ModInfo()->IsSinglePlayerOnly() || !stricmp( ModInfo()->GetGameDescription(), "Half-Life" ) )
		AddPage( new COptionsSubVoice( this ), "#GameUI_Voice" );

	AddPage( new COptionsSubAdvanced( this ), "#GameUI_Content" );

	if ( ModInfo()->IsSinglePlayerOnly() && stricmp( ModInfo()->GetGameDescription(), "Half-Life" ) )
		AddPage( new CMultiplayerAdvancedPage( this ), "#GameUI_AdvancedNoEllipsis" );

	if ( ( !ModInfo()->IsMultiplayerOnly() && !ModInfo()->IsSinglePlayerOnly() )
			|| !stricmp( ModInfo()->GetGameDescription(), "Half-Life" ) )
		AddPage( new COptionsSubMultiplayer( this ), "#GameUI_Multiplayer" );

	SetApplyButtonVisible( true );
	vgui2::PropertySheet* sheet = GetPropertySheet();
	sheet->SetTabWidth( GetProportionalScaledValue( 72 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsDialog::~COptionsDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Overrides the base class so it can setup the taskbar title properly
//-----------------------------------------------------------------------------
void COptionsDialog::SetTitle( const char* title, bool surfaceTitle )
{
	vgui2::Frame::SetTitle( title, surfaceTitle );
	if ( !g_pTaskbar )
		return;

	wchar_t titleBuffer[1024];
	wchar_t* localizedTitle = vgui2::localize()->Find( title );
	if ( !localizedTitle )
	{
		vgui2::localize()->ConvertANSIToUnicode( title, titleBuffer, sizeof( titleBuffer ) );
		localizedTitle = titleBuffer;
	}

	g_pTaskbar->SetTitle( GetVPanel(), localizedTitle );
}

//-----------------------------------------------------------------------------
// Purpose: Brings the dialog to the fore
//-----------------------------------------------------------------------------
void COptionsDialog::Activate()
{
	vgui2::Frame::Activate();
	EnableApplyButton( false );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the dialog
//-----------------------------------------------------------------------------
void COptionsDialog::Run()
{
	SetTitle( "#GameUI_Options", true );
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Deletes the dialog on close
//-----------------------------------------------------------------------------
void COptionsDialog::OnClose()
{
	vgui2::Frame::OnClose();
	MarkForDeletion();
}
