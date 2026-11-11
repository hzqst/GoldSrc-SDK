//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h" // for gEngfuncs
#include "cstrikeclassmenu.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "game_controls/mouseoverpanelbutton.h"
#include "vgui_controls/Controls.h"
#include "Button.h"
#include "FileSystem.h"
#include <stdio.h>
#include <string.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\cl_dll\\VGUI\\cstrikeclassmenu.cpp" )

const char* GetResourceFile( int team )
{
	return team == 1 ? "Resource/UI/ClassMenu_TER.res" : "Resource/UI/ClassMenu_CT.res";
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSClassMenu::CCSClassMenu( vgui2::Panel* parent ) : CClassMenu( parent, false )
{
	m_iFirst = 1;
	if ( g_pCareerUI && g_pCareerUI->IsPlayingMatch() )
		SetScheme( "CareerClientScheme" );
	else
		SetScheme( "ClientScheme" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSClassMenu::~CCSClassMenu()
{
}

void CCSClassMenu::SetVisibleButton( const char* name, bool state )
{
	vgui2::Button* button = dynamic_cast<vgui2::Button*>( FindChildByName( name, false ) );
	if ( button )
	{
		button->SetVisible( state );
	}
}

void CCSClassMenu::Update( int*, int team )
{
	LoadControlSettings( GetResourceFile( team ), NULL );
	if ( g_iPlayerClass )
		SetVisibleButton( "CancelButton", true );
	else
		SetVisibleButton( "CancelButton", false );
	if ( !gHUD.IsGame( "czero" ) )
	{

		// if we don't have the new models installed,
		// turn off the militia and spetsnaz buttons
		SetVisibleButton( "militia", false );
		SetVisibleButton( "spetsnaz", false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: magic override to allow vgui to create mouse over buttons for us, and to use our class res files, not TFC's one
//-----------------------------------------------------------------------------
vgui2::Panel* CCSClassMenu::CreateControlByName( const char* controlName )
{
	if ( stricmp( "MouseOverPanelButton", controlName ) )
	{
		return CClassMenu::CreateControlByName( controlName );
	}
	ClassHelperPanel* panel = new ClassHelperPanel( this, NULL );
	panel->SetVisible( false );
	int x, y, wide, tall;
	m_pPanel->GetBounds( x, y, wide, tall );
	panel->SetBounds( x, y, wide, tall );
	MouseOverPanelButton* button = new MouseOverPanelButton( this, NULL, panel );
	panel->SetAssociatedButton( button );
	if ( m_iFirst )
	{
		m_iFirst = 0;
		m_pFirstButton = button; // this first button's page is triggered when the panel is set visible
	}
	return button;
}

CCSClassMenu::ClassHelperPanel::ClassHelperPanel( vgui2::Panel* parent, const char* name ) :
	vgui2::EditablePanel( parent, name )
{
	m_pAssociatedButton = NULL;
}

void CCSClassMenu::ClassHelperPanel::SetAssociatedButton( vgui2::Panel* button )
{
	m_pAssociatedButton = button;
}

vgui2::Panel* CCSClassMenu::ClassHelperPanel::GetAssociatedButton()
{
	return m_pAssociatedButton;
}


//-----------------------------------------------------------------------------
// Purpose: returns the filename of the class file for this class
//-----------------------------------------------------------------------------
const char* CCSClassMenu::ClassHelperPanel::GetClassPage( const char* className )
{
	static char classPanel[MAX_PATH];
	_snprintf( classPanel, sizeof( classPanel ), "classes/%s.res", className );
	if ( !vgui2::filesystem()->FileExists( classPanel ) )
	{
		if ( vgui2::filesystem()->FileExists( "classes/default.res" ) )
			_snprintf( classPanel, sizeof( classPanel ), "classes/default.res" );
		else
			return NULL;
	}
	return classPanel;
}

void CCSClassMenu::ClassHelperPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	ASSERT_AT( GetAssociatedButton(), 135 );
	LoadControlSettings( GetClassPage( GetAssociatedButton()->GetName() ), NULL );
	vgui2::Panel::ApplySchemeSettings( scheme );
}
