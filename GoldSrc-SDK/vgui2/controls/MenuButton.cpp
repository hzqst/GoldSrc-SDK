//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "MenuButton.h"
#include "Menu.h"
#include "controls.h"
#include "vgui/IInput.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
MenuButton::MenuButton( Panel* parent, const char* name, const char* text ) : Button( parent, name, text )
{
	SetUseCaptureMouse( false );
	SetButtonActivationType( ACTIVATE_ONPRESSED );
	m_pMenu = NULL;
	m_iDirection = DOWN;
	_openOffsetY = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the offset from where menu would normally be placed
//			Only is used if menu is ALIGN_WITH_PARENT
//-----------------------------------------------------------------------------
void MenuButton::SetOpenOffsetY( int offset )
{
	_openOffsetY = offset;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the direction from the menu button the menu should open
//-----------------------------------------------------------------------------
void MenuButton::SetOpenDirection( MenuDirection_e direction )
{
	m_iDirection = direction;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the menu button loses focus; hides the menu
//-----------------------------------------------------------------------------
void MenuButton::OnKillFocus()
{
	if ( !m_pMenu->HasFocus() )
		HideMenu();

	Button::OnKillFocus();
}

//-----------------------------------------------------------------------------
// Purpose: attaches a menu to the menu button
//-----------------------------------------------------------------------------
void MenuButton::SetMenu( Menu* menu )
{
	m_pMenu = menu;

	if ( menu )
	{
		menu->SetVisible( false );
		m_pMenu->AddActionSignalTarget( this );
		m_pMenu->SetParent( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: hides the menu
//-----------------------------------------------------------------------------
void MenuButton::HideMenu()
{
	if ( !m_pMenu )
		return;

	// hide the menu
	m_pMenu->SetVisible( false );
	Button::ForceDepressed( false );
	Repaint();
	OnHideMenu( m_pMenu );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the menu is closed
//-----------------------------------------------------------------------------
void MenuButton::OnMenuClose()
{
	HideMenu();
	PostActionSignal( new KeyValues( "MenuClose" ) );
}

MenuButton::~MenuButton()
{
}

void MenuButton::DrawFocusBorder( int, int, int, int )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool MenuButton::CanBeDefaultButton()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Handles hotkey accesses
//-----------------------------------------------------------------------------
void MenuButton::DoClick()
{
	if ( !m_pMenu )
		return;

	// menu is already visible, hide the menu
	if ( m_pMenu->IsVisible() )
	{
		HideMenu();
		return;
	}

	// do nothing if menu is not enabled
	if ( !m_pMenu->IsEnabled() )
		return;

	// force the menu to Think
	m_pMenu->PerformLayout();

	// move the menu to the correct place below the button
	int x, y, wide, tall;
	GetSize( wide, tall );

	if ( m_iDirection == CURSOR )
	{
		// force the menu to appear where the mouse button was pressed
		input()->GetCursorPos( x, y );
	}
	else if ( m_iDirection == ALIGN_WITH_PARENT && GetVParent() )
	{
		x = 0;
		y = tall;
		ParentLocalToScreen( x, y );
		x--;
		y += _openOffsetY;
	}
	else
	{
		x = 0;
		y = tall;
		LocalToScreen( x, y );
	}

	int menuWide, menuTall;
	m_pMenu->GetSize( menuWide, menuTall );

	int buttonWide, buttonTall;
	GetSize( buttonWide, buttonTall );

	if ( m_iDirection == UP )
		m_pMenu->SetPos( x, y - menuTall - buttonTall - 1 );
	else if ( m_iDirection == DOWN )
		m_pMenu->SetPos( x, y + 1 );
	else
		m_pMenu->SetPos( x + 1, y + 1 );

	int menuX, menuY;
	m_pMenu->GetBounds( menuX, menuY, menuWide, menuTall );

	int workX, workY, workWide, workTall;
	surface()->GetWorkspaceBounds( workX, workY, workWide, workTall );

	if ( menuX < workX )
		menuX = workX;
	if ( menuX + menuWide > workX + workWide )
		menuX = workX + workWide - menuWide;

	m_pMenu->SetBounds( menuX, menuY, menuWide, menuTall );

	MoveToFront();
	// notify
	OnShowMenu( m_pMenu );
	Button::ForceDepressed( true );
	// show the menu
	m_pMenu->SetVisible( true );
	// bring to focus
	m_pMenu->RequestFocus( 0 );
}

void MenuButton::OnKeyCodeTyped( KeyCode code )
{
	bool shift = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
	bool ctrl = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
	bool alt = input()->IsKeyDown( KEY_LALT ) || input()->IsKeyDown( KEY_RALT );

	if ( !alt && !shift && !ctrl && code == KEY_ENTER )
		DoClick();

	Panel::OnKeyCodeTyped( code );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MenuButton::OnCursorEntered()
{
	Button::OnCursorEntered();

	// post a message to the parent menu.
	// forward the message on to the parent of this menu.
	KeyValues* msg = new KeyValues( "CursorEnteredMenuButton" );
	// tell the parent this menuitem is the one that was entered so it can open the menu if it wants
	msg->SetInt( "VPanel", GetVPanel() );
	ivgui()->PostMessage( GetVParent(), msg, 0, 0.0f );
}

void MenuButton::OnShowMenu( Menu* )
{
}

void MenuButton::OnHideMenu( Menu* )
{
}

}
