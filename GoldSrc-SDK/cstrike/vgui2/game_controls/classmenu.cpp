//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "classmenu.h"
#include "mouseoverpanelbutton.h"
#include "../../../vgui2/controls/Label.h"
#include "vgui_controls/Controls.h"
#include "iviewport.h"
#include "igameuifuncs.h"
#include "../../../public/FileSystem.h"
#include <stdio.h>
#include <string.h>

extern IGameUIFuncs* gameuifuncs; // for key binding details

void MouseOverPanelButton::OnCursorEntered()
{
	vgui2::Button::OnCursorEntered();
	if ( !m_pPanel )
		return;
	if ( g_lastPanel )
		g_lastPanel->SetVisible( false );
	if ( m_pPanel )
	{
		m_pPanel->SetVisible( true );
		m_pPanel->MoveToFront();
		g_lastPanel = m_pPanel;
	}
}

class CClassMenu::ClassHelperPanel : public vgui2::EditablePanel
{
public:
	ClassHelperPanel( vgui2::Panel* parent, const char* name ) :
		vgui2::EditablePanel( parent, name ), m_pButton( NULL )
	{
	}
	void SetButton( MouseOverPanelButton* button ) { m_pButton = button; }
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	const char* GetClassPage( const char* name );
	MouseOverPanelButton* m_pButton;
};

const char* CClassMenu::ClassHelperPanel::GetClassPage( const char* name )
{
	static char classPanel[260];
	_snprintf( classPanel, sizeof( classPanel ), "classes/short_%s.res", name );
	classPanel[sizeof( classPanel ) - 1] = 0;
	if ( vgui2::filesystem()->FileExists( classPanel ) )
		return classPanel;
	if ( !vgui2::filesystem()->FileExists( "classes/default.res" ) )
		return NULL;

	_snprintf( classPanel, sizeof( classPanel ), "classes/default.res" );
	return classPanel;
}

void CClassMenu::ClassHelperPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	LoadControlSettings( GetClassPage( GetName() ), NULL );
	vgui2::Panel::ApplySchemeSettings( scheme );
}

CClassMenu::CClassMenu( vgui2::Panel* parent, bool loadResources ) : vgui2::Frame( parent, "ClassMenu", true, false )
{
	m_pFirstButton = NULL;
	m_iScoreBoardKey = 0; // this is looked up in Activate()
	m_iFirst = 1;

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
	if ( loadResources )
		LoadControlSettings( "Resource/UI/ClassMenu.res", NULL );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CClassMenu::~CClassMenu()
{
	m_pClassButtons.Purge();
}

void CClassMenu::Update( int* validClasses, int )
{
	int* teamClasses = &validClasses[gViewPortInterface->GetClientDllInterface()->TeamNumber()];
	if ( *teamClasses == -1 )
	{
		for ( int i = 0; i < m_pClassButtons.Count(); ++i )
		{
			MouseOverPanelButton* button = m_pClassButtons[i];
			button->SetEnabled( !_stricmp( button->GetName(), "civilian" ) );
		}
	}
	else
	{
		for ( int i = 0; i < m_pClassButtons.Count(); ++i )
		{
			MouseOverPanelButton* button = m_pClassButtons[i];
			button->SetEnabled( button->GetClass() != -1 && !( button->GetClass() & *validClasses )
				&& !( button->GetClass() & *teamClasses ) );
		}
	}
}

void CClassMenu::OnCommand( const char* command )
{
	if ( _stricmp( command, "vguicancel" ) )
		gViewPortInterface->GetClientDllInterface()->CallEnghudClientCmd( command );
	Close();
	gViewPortInterface->HideBackGround();
	vgui2::Frame::OnCommand( command );
}

void CClassMenu::OnKeyCodePressed( vgui2::KeyCode key )
{
	if ( key == vgui2::KEY_PAD_ENTER )
		OnCommand( "joinclass 6" );
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
		vgui2::Frame::OnKeyCodePressed( key );
}

void CClassMenu::Activate()
{
	vgui2::Frame::Activate();
	SetVisible( true );
	// load a default class page
	if ( m_pFirstButton )
		m_pFirstButton->ShowPage();
	if ( !m_iScoreBoardKey )
		m_iScoreBoardKey = gameuifuncs->GetVGUI2KeyCodeForBind( "showscores" );
}

void CClassMenu::SetLabelText( const char* name, const char* text )
{
	vgui2::Label* label = dynamic_cast<vgui2::Label*>( FindChildByName( name, false ) );
	if ( label )
		label->SetText( text );
}

vgui2::Panel* CClassMenu::CreateControlByName( const char* name )
{
	if ( _stricmp( name, "MouseOverPanelButton" ) )
		return vgui2::EditablePanel::CreateControlByName( name );

	ClassHelperPanel* panel = new ClassHelperPanel( this, NULL );
	panel->SetVisible( false );
	int x, y, wide, tall;
	m_pPanel->GetBounds( x, y, wide, tall );
	panel->SetBounds( x, y, wide, tall );
	MouseOverPanelButton* button = new MouseOverPanelButton( this, NULL, panel );
	m_pClassButtons.AddToTail( button );
	panel->SetButton( button );
	if ( m_iFirst )
	{
		m_iFirst = 0;
		m_pFirstButton = button;
	}
	return button;
}
