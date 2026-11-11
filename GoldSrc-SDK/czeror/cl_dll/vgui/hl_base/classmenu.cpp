//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "classmenu.h"

#include "mouseoverpanelbutton.h"
#include "Label.h"
#include "controls.h"
#include "FileSystem.h"
#include "game_controls/iviewport.h"
#include "hud.h"

#include <stdio.h>
#include <string.h>

CClassMenu::CClassMenu( vgui2::Panel* parent )
	: vgui2::Frame( parent, "ClassMenu", true, false ), m_pFirstButton( NULL )
{
	MakePopup( true, false );
	SetVisible( false );

	// initialize dialog
	SetTitle( "", true );

	// load the new scheme early!!
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );

	// hide the system buttons
	SetTitleBarVisible( false );
	SetProportional( true );
	m_pPanel = new vgui2::Panel( this, "ClassInfo" );
	LoadControlSettings( "Resource/UI/ClassMenu.res", NULL );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CClassMenu::~CClassMenu()
{
}

CClassMenu::ClassHelperPanel::ClassHelperPanel( vgui2::Panel* parent, const char* name )
	: vgui2::EditablePanel( parent, name ), m_pButton( NULL )
{
}

void CClassMenu::ClassHelperPanel::SetButton( MouseOverPanelButton* button )
{
	m_pButton = button;
}

const char* CClassMenu::ClassHelperPanel::GetClassPage( const char* className )
{
	static char classPanel[MAX_PATH];
	_snprintf( classPanel, sizeof( classPanel ), "classes/short_%s.res", className );
	if ( vgui2::filesystem()->FileExists( classPanel ) )
		return classPanel;
	if ( !vgui2::filesystem()->FileExists( "classes/default.res" ) )
		return NULL;
	strcpy( classPanel, "classes/default.res" );
	return classPanel;
}

void CClassMenu::ClassHelperPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	const char* name = GetName();
	LoadControlSettings( GetClassPage( name ), NULL );
	if ( m_pButton )
	{
		if ( !_stricmp( "scout", name ) )
			m_pButton->SetClass( 1, 1 );
		else if ( !_stricmp( "sniper", name ) )
			m_pButton->SetClass( 2, 2 );
		else if ( !_stricmp( "soldier", name ) )
			m_pButton->SetClass( 3, 4 );
		else if ( !_stricmp( "demoman", name ) )
			m_pButton->SetClass( 4, 8 );
		else if ( !_stricmp( "medic", name ) )
			m_pButton->SetClass( 5, 16 );
		else if ( !_stricmp( "hwguy", name ) )
			m_pButton->SetClass( 6, 32 );
		else if ( !_stricmp( "pyro", name ) )
			m_pButton->SetClass( 7, 64 );
		else if ( !_stricmp( "spy", name ) )
			m_pButton->SetClass( 8, 256 );
		else if ( !_stricmp( "engineer", name ) )
			m_pButton->SetClass( 9, 512 );
		else if ( !_stricmp( "randompc", name ) )
			m_pButton->SetClass( 0, 128 );
		else
			m_pButton->SetClass( 0, -1 );
	}
	vgui2::Panel::ApplySchemeSettings( scheme );
}

vgui2::Panel* CClassMenu::CreateControlByName( const char* controlName )
{
	if ( _stricmp( "MouseOverPanelButton", controlName ) )
		return vgui2::EditablePanel::CreateControlByName( controlName );
	ClassHelperPanel* panel = new ClassHelperPanel( this, NULL );
	panel->SetVisible( false );
	int x, y, wide, tall;
	m_pPanel->GetBounds( x, y, wide, tall );
	panel->SetBounds( x, y, wide, tall );
	MouseOverPanelButton* button = new MouseOverPanelButton( this, NULL, panel );
	m_pClassButtons.AddToTail( button );
	panel->SetButton( button );
	static int first = 1;
	if ( first )
	{
		first = 0;
		m_pFirstButton = button;
	}
	return button;
}

void CClassMenu::SetLabelText( const char* entryName, const char* text )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( entryName, false ) );
	if ( label )
		label->SetText( text );
}

void CClassMenu::Activate()
{
	vgui2::Frame::Activate();
	SetVisible( true );
	// load a default class page
	if ( m_pFirstButton )
		m_pFirstButton->ShowPage();
}

void CClassMenu::Update( int* validClasses, int )
{
	if ( validClasses[g_iTeamNumber] == -1 )
	{
		for ( int i = 0; i < m_pClassButtons.Count(); i++ )
			m_pClassButtons[i]->SetEnabled( !_stricmp( m_pClassButtons[i]->GetName(), "civilian" ) );
	}
	else
	{
		for ( int i = 0; i < m_pClassButtons.Count(); i++ )
		{
			int mask = m_pClassButtons[i]->GetClass();
			m_pClassButtons[i]->SetEnabled( mask != -1 && !( mask & validClasses[0] )
				&& !( mask & validClasses[g_iTeamNumber] ) );
		}
	}
}

void CClassMenu::OnCommand( const char* command )
{
	if ( _stricmp( command, "vguicancel" ) )
		gEngfuncs.pfnClientCmd( (char*)command );
	Close();
	gViewPortInterface->HideBackGround();
	vgui2::Frame::OnCommand( command );
}
