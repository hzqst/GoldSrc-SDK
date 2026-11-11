//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "buysubmenu.h"
#include "buymenu.h"
#include "buymouseoverpanelbutton.h"
#include "buypreset_mainmenu.h"
#include "game_controls/iviewport.h"
#include "vgui_controls/Controls.h"
#include "vgui/KeyValues.h"
#include "FileSystem.h"
#include <stdio.h>
#include <string.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\cl_dll\\VGUI\\buysubmenu.cpp" )

bool IsASMapType()
{
	return !strnicmp( gEngfuncs.pfnGetLevelName(), "maps/as_", 8 );
}

bool IsDEMapType()
{
	return !strnicmp( gEngfuncs.pfnGetLevelName(), "maps/de_", 8 );
}

CBuySubMenu::CBuySubMenu( vgui2::Panel* parent, const char* name ) : vgui2::WizardSubPanel( parent, name )
{
	_nextPanel = NULL;
	m_pFirstButton = NULL;
	m_pPanel = new vgui2::Panel( this, "ItemInfo" ); // info window about these items
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBuySubMenu::~CBuySubMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Causes the panel to delete itself when it closes
//-----------------------------------------------------------------------------
void CBuySubMenu::DeleteSubPanels()
{
	if ( _nextPanel )
	{
		_nextPanel->SetVisible( false );
		_nextPanel = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: return the next panel to show
//-----------------------------------------------------------------------------
vgui2::WizardSubPanel* CBuySubMenu::GetNextSubPanel()
{
	return _nextPanel;
}

void CBuySubMenu::OnDisplay()
{
	FitToFullScreen( this );
}

//-----------------------------------------------------------------------------
// Purpose: Make the first buttons page get displayed when the menu becomes visible
//-----------------------------------------------------------------------------
void CBuySubMenu::SetVisible( bool state )
{
	vgui2::Panel::SetVisible( state );
	for ( int i = 0; i < GetChildCount(); ++i )
	{
		BuyMouseOverPanelButton* button = dynamic_cast<BuyMouseOverPanelButton*>( GetChild( i ) );
		if ( button )
		{
			button->InvalidateLayout( false, false );
			button->HidePage();
		}
	}
	if ( state && m_pFirstButton )
	{
		m_pFirstButton->ShowPage();
	}
}

vgui2::Panel* CBuySubMenu::CreateControlByName( const char* controlName )
{
	if ( stricmp( "MouseOverPanelButton", controlName ) )
	{
		return vgui2::EditablePanel::CreateControlByName( controlName );
	}
	ClassHelperPanel* panel = new ClassHelperPanel( this, NULL );
	panel->SetVisible( false );
	int x, y, wide, tall;
	m_pPanel->GetBounds( x, y, wide, tall );
	panel->SetBounds( x, y, wide, tall );
	BuyMouseOverPanelButton* button = new BuyMouseOverPanelButton( this, NULL, panel );
	panel->SetAssociatedButton( button );
	if ( !m_pFirstButton )
	{
		m_pFirstButton = button;
	}
	return button;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the user picks a class
//-----------------------------------------------------------------------------
void CBuySubMenu::OnCommand( const char* command )
{
	if ( strstr( command, ".res" ) ) // if its a .res file then its a new menu
	{
		char fileName[256];
		if ( (size_t)( strstr( command, ".res" ) - command ) + strlen( g_iTeamNumber == 1 ? "_TER.res" : "_CT.res" ) >= sizeof( fileName ) )
			return;
		strncpy( fileName, command, sizeof( fileName ) );
		*strstr( fileName, ".res" ) = 0;
		if ( g_iTeamNumber == 1 )
			strcat( fileName, "_TER.res" );
		else
			strcat( fileName, "_CT.res" );

		int i;
		// check the cache
		for ( i = 0; i < m_SubMenus.Count(); ++i )
		{
			if ( !stricmp( m_SubMenus[i].filename, fileName ) )
			{
				_nextPanel = m_SubMenus[i].panel;
				ASSERT_AT( _nextPanel, 147 );
				_nextPanel->InvalidateLayout( false, false );
				break;
			}
		}

		if ( i == m_SubMenus.Count() )
		{
			// not there, add a new entry
			SubMenuEntry_t newEntry;
			memset( &newEntry, 0, sizeof( newEntry ) );

			CBuySubMenu* newMenu = new CBuySubMenu( this, "BuySubMenu" );
			newMenu->LoadControlSettings( fileName, NULL );
			_nextPanel = newMenu;

			strncpy( newEntry.filename, fileName, sizeof( newEntry.filename ) );
			newEntry.panel = newMenu;
			m_SubMenus.AddToTail( newEntry );
		}

		GetWizardPanel()->OnNextButton();
	}
	else // otherwise it must be a client command
	{
		MaintainBuyPresetInputFocus();
		gEngfuncs.pfnServerCmd( "client_buy_close\n" );
		GetWizardPanel()->Close();
		gViewPortInterface->HideBackGround();
		vgui2::Panel::OnCommand( command );
		if ( !stricmp( command, "buy_preset_edit" ) && gHUD.IsGame( "czero" ) )
		{
			ShowBuyPresetMainMenu( true, 0 );
		}
		else if ( stricmp( command, "vguicancel" ) )
		{
			gEngfuncs.pfnClientCmd( (char*)command );
		}
	}
}

CBuySubMenu::ClassHelperPanel::ClassHelperPanel( vgui2::Panel* parent, const char* name ) :
	vgui2::EditablePanel( parent, name )
{
	m_pAssociatedButton = NULL;
}

void CBuySubMenu::ClassHelperPanel::SetAssociatedButton( BuyMouseOverPanelButton* button )
{
	m_pAssociatedButton = button;
}

BuyMouseOverPanelButton* CBuySubMenu::ClassHelperPanel::GetAssociatedButton()
{
	return m_pAssociatedButton;
}


//-----------------------------------------------------------------------------
// Purpose: returns the filename of the class file for this class
//-----------------------------------------------------------------------------
const char* CBuySubMenu::ClassHelperPanel::GetClassPage( const char* className )
{
	static char classPanel[MAX_PATH];
	_snprintf( classPanel, sizeof( classPanel ), "classes/%s.res", className );
	if ( vgui2::filesystem()->FileExists( classPanel ) )
	{
		return classPanel;
	}
	if ( vgui2::filesystem()->FileExists( "classes/default.res" ) )
	{
		_snprintf( classPanel, sizeof( classPanel ), "classes/default.res" );
		return classPanel;
	}
	return NULL;
}

void CBuySubMenu::ClassHelperPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	ASSERT_AT( GetAssociatedButton(), 216 );
	const char* name;
	if ( ( IsASMapType() && GetAssociatedButton()->GetASRestrict() ) ||
		( !IsDEMapType() && GetAssociatedButton()->GetDEUseOnly() ) )
	{
		name = "not_available";
	}
	else
	{
		name = GetAssociatedButton()->GetName();
	}
	LoadControlSettings( GetClassPage( name ), NULL );
	vgui2::Panel::ApplySchemeSettings( scheme );
}

BuyMouseOverPanelButton::BuyMouseOverPanelButton( vgui2::Panel* parent, const char* name,
	vgui2::Panel* panel ) : MouseOverPanelButton( parent, name, panel )
{
	m_iPrice = 0;
	m_iASRestrict = 0;
	m_iDEUseOnly = 0;
}

void BuyMouseOverPanelButton::SetPriceState()
{
	if ( m_iPrice && m_iPrice > gHUD.m_accountBalance.GetBalance() )
		SetEnabled( false );
	else
		SetEnabled( true );
}

void BuyMouseOverPanelButton::SetMapTypeState()
{
	if ( IsASMapType() && m_iASRestrict )
	{
		SetEnabled( false );
	}
	if ( !IsDEMapType() && m_iDEUseOnly )
	{
		SetEnabled( false );
	}
}

void BuyMouseOverPanelButton::PerformLayout()
{
	vgui2::Button::PerformLayout();
	SetPriceState();
	SetMapTypeState();
}

void BuyMouseOverPanelButton::ApplySettings( KeyValues* settings )
{
	vgui2::Button::ApplySettings( settings );
	KeyValues* value = settings->FindKey( "cost" );
	if ( value )
	{
		m_iPrice = value->GetInt();
	}
	value = settings->FindKey( "as_restrict" );
	if ( value )
	{
		m_iASRestrict = value->GetInt();
	}
	value = settings->FindKey( "de_useonly" );
	if ( value )
	{
		m_iDEUseOnly = value->GetInt();
	}
	SetPriceState();
	SetMapTypeState();
}
