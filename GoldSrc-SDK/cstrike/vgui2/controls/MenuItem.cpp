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

#include "MenuItem.h"
#include "Menu.h"
#include "TextImage.h"
#include "controls.h"

#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"

#include <cstdlib>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\MenuItem.cpp" )

class MenuItemCheckImage : public vgui2::TextImage
{
public:
	MenuItemCheckImage( vgui2::MenuItem* item ) : vgui2::TextImage( "g" ), _menuItem( item )
	{
		SetSize( 20, 13 );
	}
	~MenuItemCheckImage() override;
	void Paint() override;

private:
	vgui2::MenuItem* _menuItem;
};

MenuItemCheckImage::~MenuItemCheckImage()
{
}

void MenuItemCheckImage::Paint()
{
	DrawSetTextFont( GetFont() );
	DrawSetTextColor( _menuItem->GetFgColor() );
	DrawPrintChar( 0, 0, L'g' );
	if ( !_menuItem->IsChecked() )
		return;

	if ( _menuItem->IsEnabled() )
	{
		DrawSetTextColor( _menuItem->GetButtonFgColor() );
		DrawPrintChar( 0, 2, L'a' );
	}
	else if ( !_menuItem->IsEnabled() )
	{
		DrawSetTextColor( _menuItem->GetDisabledFgColor1() );
		DrawPrintChar( 1, 3, L'a' );
		DrawSetTextColor( _menuItem->GetDisabledFgColor2() );
		DrawPrintChar( 0, 2, L'a' );
	}
}

namespace vgui2
{
MenuItem::MenuItem( Menu* parent, const char* name, const char* text, Menu* cascadeMenu, bool checkable )
	: Button( parent, name, text )
{
	m_pCascadeMenu = cascadeMenu;
	m_bCheckable = checkable;
	SetButtonActivationType( ACTIVATE_ONRELEASED );
	m_pUserData = NULL;
	ASSERT_AT( !(cascadeMenu && checkable), 87 );
	Init();
}

MenuItem::MenuItem( Menu* parent, const char* name, const wchar_t* text, Menu* cascadeMenu, bool checkable )
	: Button( parent, name, text )
{
	m_pCascadeMenu = cascadeMenu;
	m_bCheckable = checkable;
	SetButtonActivationType( ACTIVATE_ONRELEASED );
	m_pUserData = NULL;
	ASSERT_AT( !(cascadeMenu && checkable), 108 );
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
MenuItem::~MenuItem()
{
	delete m_pCascadeMenu;
	delete m_pCascadeArrow;
	delete m_pCheck;
	if ( m_pUserData )
		m_pUserData->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Basic initializer
//-----------------------------------------------------------------------------
void MenuItem::Init()
{
	m_pCascadeArrow = NULL;
	m_pCheck = NULL;
	if ( m_pCascadeMenu )
	{
		m_pCascadeMenu->SetParent( this );
		m_pCascadeArrow = new TextImage( "4" );	// this makes a right pointing arrow.
		m_pCascadeMenu->AddActionSignalTarget( this );
	}
	else if ( m_bCheckable )
	{
		// move the text image over so we have room for the check
		SetTextImageIndex( 1 );
		m_pCheck = new MenuItemCheckImage( this );
		SetImageAtIndex( 0, m_pCheck, CHECK_INSET );
		SetChecked( false );
	}
	SetButtonBorderEnabled( false );
	SetContentAlignment( a_west );
	SetUseCaptureMouse( false );
	SetContentAlignment( a_west );
}

//-----------------------------------------------------------------------------
// Purpose: Return a the menu that this menuItem contains
//          This is useful when the parent menu's commands must be
//          sent through all menus that are open as well (like hotkeys)
//-----------------------------------------------------------------------------
Menu* MenuItem::GetMenu()
{
	return m_pCascadeMenu;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Menu* MenuItem::GetParentMenu()
{
	return (Menu*)GetParent();
}

KeyValues* MenuItem::GetUserData()
{
	if ( m_pCascadeMenu )
		return m_pCascadeMenu->GetItemUserData( m_pCascadeMenu->GetActiveItem() );
	return m_pUserData;
}

void MenuItem::SetUserData( const KeyValues* data )
{
	if ( m_pUserData )
		m_pUserData->deleteThis();
	if ( data )
		m_pUserData = data->MakeCopy();
}

bool MenuItem::HasMenu()
{
	return m_pCascadeMenu != NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Return if this menuitem is checkable or not
// This is used by menus to perform the layout properly.
//-----------------------------------------------------------------------------
bool MenuItem::IsCheckable()
{
	return m_bCheckable;
}

//-----------------------------------------------------------------------------
// Purpose: Return if this menuitem is checked or not
//-----------------------------------------------------------------------------
bool MenuItem::IsChecked()
{
	return m_bChecked;
}

//-----------------------------------------------------------------------------
// Purpose: Set the checked state of a checkable menuitem
//			Does nothing if item is not checkable
//-----------------------------------------------------------------------------
void MenuItem::SetChecked( bool state )
{
	if ( m_bCheckable )
		m_bChecked = state;
}

//-----------------------------------------------------------------------------
// Purpose: Close the cascading menu if we have one.
//-----------------------------------------------------------------------------
void MenuItem::CloseCascadeMenu()
{
	if ( !m_pCascadeMenu )
		return;
	if ( m_pCascadeMenu->IsVisible() )
		m_pCascadeMenu->SetVisible( false );
	// disarm even if menu wasn't visible!
	SetArmed( false );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the cascading menu.
//-----------------------------------------------------------------------------
void MenuItem::OpenCascadeMenu()
{
	if ( !m_pCascadeMenu )
		return;
	// perform layout on menu, this way it will open in the right spot 
	// if the window's been moved
	m_pCascadeMenu->PerformLayout();
	m_pCascadeMenu->SetVisible( true );
	ArmItem();
}

void MenuItem::GetCheckImageSize( int& width, int& height )
{
	width = 0;
	height = 0;
	if ( m_pCheck )
	{
		((TextImage*)m_pCheck)->ResizeImageToContent();
		m_pCheck->GetSize( width, height );
		width += CHECK_INSET;
	}
}

void MenuItem::GetTextImageSize( int& width, int& height )
{
	GetTextImage()->GetSize( width, height );
}

//-----------------------------------------------------------------------------
// Purpose: Set the size of the text portion of the label.
//			For normal menu items this is the same as the label size, but for
//			cascading menus it sizes textImage portion only, without
//			the arrow.
//-----------------------------------------------------------------------------
void MenuItem::SetTextImageSize( int width, int height )
{
	GetTextImage()->SetSize( width, height );
}

void MenuItem::GetArrowImageSize( int& width, int& height )
{
	width = 0;
	height = 0;
	if ( m_pCascadeArrow )
		m_pCascadeArrow->GetSize( width, height );
}

//-----------------------------------------------------------------------------
// Purpose: Set the menu to key mode if a child menu goes into keymode
//-----------------------------------------------------------------------------
void MenuItem::OnKeyModeSet()
{
	// send the message to this parent in case this is a cascading menu
	ivgui()->PostMessage( GetVParent(), new KeyValues( "KeyModeSet" ), GetVPanel(), 0.0f );
}

void MenuItem::OnCursorEntered()
{
	KeyValues* message = new KeyValues( "CursorEnteredMenuItem" );
	message->SetInt( "VPanel", GetVPanel() );
	ivgui()->PostMessage( GetVParent(), message, 0, 0.0f );
}

void MenuItem::OnCursorExited()
{
	KeyValues* message = new KeyValues( "CursorExitedMenuItem" );
	message->SetInt( "VPanel", GetVPanel() );
	ivgui()->PostMessage( GetVParent(), message, 0, 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Handle cursor moving in a menuItem.
//-----------------------------------------------------------------------------
void MenuItem::OnCursorMoved( int x, int y )
{
	if ( GetParentMenu()->GetMenuMode() == Menu::KEYBOARD )
		OnCursorEntered();

	// chain up to parent
	CallParentFunction( new KeyValues( "OnCursorMoved", "x", x, "y", y ) );
}

bool MenuItem::CanBeDefaultButton()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: fire the menu item as if it has been selected and 
//			Tell the owner that it is closing
//-----------------------------------------------------------------------------
void MenuItem::FireActionSignal()
{
	// cascading menus items don't trigger the parent menu to disappear
	// (they trigger the cascading menu to open/close when cursor is moved over/off them)
	if ( m_pCascadeMenu )
	{
		if ( GetParentMenu()->GetMenuMode() == Menu::KEYBOARD )
			OpenCascadeMenu();
		return;
	}
	KeyValues* message = new KeyValues( "MenuItemSelected" );
	message->SetPtr( "panel", this );
	ivgui()->PostMessage( GetVParent(), message, GetVPanel(), 0.0f );

	//	ivgui()->PostMessage(GetVParent(), new KeyValues("MenuItemSelected"), GetVPanel());		
	Button::FireActionSignal();
	if ( m_bCheckable )
		m_bChecked = !m_bChecked;
}

//-----------------------------------------------------------------------------
// Purpose: Pass kill focus events up to parent, This will tell all panels
//          in the hierarchy to hide themselves, and enables cascading menus to
//		    all disappear on selecting an item at the end of the tree.
//-----------------------------------------------------------------------------
void MenuItem::OnKillFocus()
{
	GetParentMenu()->OnKillFocus();
}

//-----------------------------------------------------------------------------
// Purpose: Layout the Textimage and the Arrow part of the menuItem
//-----------------------------------------------------------------------------
void MenuItem::PerformLayout()
{
	Button::PerformLayout();
	if ( m_pCascadeArrow )
		m_pCascadeArrow->SetColor( GetButtonFgColor() );
}

IBorder* MenuItem::GetBorder( bool, bool, bool, bool )
{
	return NULL;
}

void MenuItem::OnArmed()
{
}

void MenuItem::OnDisarmed()
{
}

void MenuItem::ApplySchemeSettings( IScheme* menuScheme )
{
	Button::ApplySchemeSettings( menuScheme );
	Color background = GetSchemeColor( "Menu/BgColor", GetBgColor(), menuScheme );
	Color foreground = GetSchemeColor( "Menu/FgColor", GetFgColor(), menuScheme );
	SetDefaultColor( foreground, background );
	Color armedBackground = GetSchemeColor( "Menu/ArmedBgColor", GetBgColor(), menuScheme );
	Color armedForeground = GetSchemeColor( "Menu/ArmedFgColor", GetFgColor(), menuScheme );
	SetArmedColor( armedForeground, armedBackground );
	Color depressedBackground = GetSchemeColor( "Menu/ArmedBgColor", GetBgColor(), menuScheme );
	Color depressedForeground = GetSchemeColor( "Menu/ArmedFgColor", GetFgColor(), menuScheme );
	SetDepressedColor( depressedForeground, depressedBackground );
	SetTextInset( atoi( menuScheme->GetResourceString( "Menu/TextInset" ) ), 0 );
	if ( m_pCascadeArrow )
	{
		m_pCascadeArrow->SetFont( menuScheme->GetFontEx( "Marlett", IsProportional(), IsHDProportional() ) );
		m_pCascadeArrow->ResizeImageToContent();
		AddImage( m_pCascadeArrow, 0 );
	}
	else if ( m_bCheckable )
	{
		((TextImage*)m_pCheck)->SetFont( menuScheme->GetFontEx( "Marlett", IsProportional(), IsHDProportional() ) );
		SetImageAtIndex( 0, m_pCheck, CHECK_INSET );
		((TextImage*)m_pCheck)->ResizeImageToContent();
	}

	// Have the menu redo the layout
	// Get the parent to resize
	Menu* parent = GetParentMenu();
	if ( parent )
		parent->ForceCalculateWidth();
}

//-----------------------------------------------------------------------------
// Purpose: Highlight a menu item
//			Menu item buttons highlight if disabled, but won't activate.
//-----------------------------------------------------------------------------
void MenuItem::ArmItem()
{
	OnArmed();
	// close all other menus 
	GetParentMenu()->CloseOtherMenus( this );
	SetArmed( true );
	Menu* parent = GetParentMenu();
	if ( parent )
		parent->ForceCalculateWidth();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Unhighlight a menu item
//-----------------------------------------------------------------------------
void MenuItem::DisarmItem()
{
	OnDisarmed();
	if ( !m_pCascadeMenu )
		Button::OnCursorExited();
	Menu* parent = GetParentMenu();
	if ( parent )
		parent->ForceCalculateWidth();
	Repaint();
}

bool MenuItem::IsItemArmed()
{
	return Button::IsArmed();
}

void MenuItem::OnKeyCodeReleased( KeyCode code )
{
	Menu* parent = GetParentMenu();
	if ( code == KEY_ESCAPE || code == KEY_F4 )
		parent->OnKeyCodeTyped( code );
	else if ( parent->GetMenuMode() != Menu::KEYBOARD || !m_pCascadeMenu )
		Button::OnKeyCodeReleased( code );
}
}
