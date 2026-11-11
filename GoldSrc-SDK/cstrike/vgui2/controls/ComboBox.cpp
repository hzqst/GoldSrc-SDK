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

#include "vgui_controls/ComboBox.h"
#include "Menu.h"
#include "MenuItem.h"
#include "controls.h"

#include "vgui/Cursor.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/KeyValues.h"
#include "ILocalize.h"

#include <malloc.h>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <climits>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#endif

namespace vgui2
{
ComboBoxButton::ComboBoxButton( ComboBox* parent, const char* panelName, const char* text )
	: Button( parent, panelName, "" )
{
	m_DisabledBgColor = Color( 0, 0, 0, 0 );
	SetButtonActivationType( ACTIVATE_ONPRESSED );
	m_pImage = scheme()->GetImage( text, false );
}

ComboBoxButton::~ComboBoxButton()
{
}

void ComboBoxButton::ApplySchemeSettings( IScheme* buttonScheme )
{
	Button::ApplySchemeSettings( buttonScheme );
	SetDefaultBorder( buttonScheme->GetBorder( "ScrollBarButtonBorder" ) );
	SetArmedColor( GetSchemeColor( "MenuButton/ArmedArrowColor", buttonScheme ),
		GetSchemeColor( "MenuButton/ButtonBgColor", buttonScheme ) );
	SetDepressedColor( GetSchemeColor( "MenuButton/ArmedArrowColor", buttonScheme ),
		GetSchemeColor( "MenuButton/ButtonBgColor", buttonScheme ) );
	SetDefaultColor( GetSchemeColor( "MenuButton/ButtonArrowColor", buttonScheme ),
		GetSchemeColor( "MenuButton/ButtonBgColor", buttonScheme ) );
	m_DisabledBgColor = GetSchemeColor( "ControlBG", buttonScheme );
	if ( m_pImage )
		AddImage( m_pImage, 0 );
}

void ComboBoxButton::OnCursorExited()
{
	CallParentFunction( new KeyValues( "CursorExited" ) );
}

Color ComboBoxButton::GetButtonBgColor()
{
	if ( IsEnabled() )
		return Button::GetButtonBgColor();
	return m_DisabledBgColor;
}

IBorder* ComboBoxButton::GetBorder( bool, bool, bool, bool )
{
	return NULL;
}

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
ComboBox::ComboBox( Panel* parent, const char* panelName, int numLines, bool allowEdit )
	: TextEntry( parent, panelName )
{
	SetEditable( allowEdit );
	SetHorizontalScrolling( false ); // do not scroll, always Start at the beginning of the text.

	// create the drop-down menu
	m_pDropDown = new Menu( this, NULL );
	m_pDropDown->AddActionSignalTarget( this );

	// button to Activate menu
	m_pButton = new ComboBoxButton( this, NULL, "resource/icon_down" );
	m_pButton->SetCommand( "ButtonClicked" );
	m_pButton->AddActionSignalTarget( this );
	m_pDropDown->SetNumberOfVisibleItems( numLines );
	m_bHighlight = false;
	m_iDirection = DOWN;
	m_iOpenOffsetY = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ComboBox::~ComboBox()
{
	delete m_pDropDown;
	delete m_pButton;
}

//-----------------------------------------------------------------------------
// Purpose: Set the number of items in the dropdown menu.
// Input  : numLines -  number of items in dropdown menu
//-----------------------------------------------------------------------------
void ComboBox::SetNumberOfEditLines( int numLines )
{
	m_pDropDown->SetNumberOfVisibleItems( numLines );
}

//-----------------------------------------------------------------------------
// Purpose: Updates a current item to the drop down
// Input  : wchar_t *itemText - name of dropdown menu item
//-----------------------------------------------------------------------------
bool ComboBox::IsItemIDValid( int itemID )
{
	return m_pDropDown->IsValidMenuID( itemID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ComboBox::SetItemEnabled( const char* text, bool state )
{
	m_pDropDown->SetItemEnabled( text, state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ComboBox::SetItemEnabled( int itemID, bool state )
{
	m_pDropDown->SetItemEnabled( itemID, state );
}

//-----------------------------------------------------------------------------
// Purpose: Remove all items from the drop down menu
//-----------------------------------------------------------------------------
void ComboBox::DeleteAllItems()
{
	m_pDropDown->DeleteAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int ComboBox::GetItemCount()
{
	return m_pDropDown->GetItemCount();
}

//-----------------------------------------------------------------------------
// Purpose: Activate the item in the menu list, as if that menu item had been selected by the user
// Input  : itemID - itemID from AddItem in list of dropdown items
//-----------------------------------------------------------------------------
void ComboBox::ActivateItem( int itemID )
{
	m_pDropDown->ActivateItem( itemID );
}

//-----------------------------------------------------------------------------
// Purpose: Activate the item in the menu list, as if that menu item had been selected by the user
// Input  : itemID - itemID from AddItem in list of dropdown items
//-----------------------------------------------------------------------------
void ComboBox::ActivateItemByRow( int row )
{
	m_pDropDown->ActivateItemByRow( row );
}

//-----------------------------------------------------------------------------
// Purpose: Allows a custom menu to be used with the combo box
//-----------------------------------------------------------------------------
void ComboBox::SetMenu( Menu* menu )
{
	if ( m_pDropDown )
		m_pDropDown->MarkForDeletion();
	m_pDropDown = menu;
}

void ComboBox::SortItems()
{
}

//-----------------------------------------------------------------------------
// Purpose: return the index of the last selected item
//-----------------------------------------------------------------------------
int ComboBox::GetActiveItem()
{
	return m_pDropDown->GetActiveItem();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
KeyValues* ComboBox::GetItemUserData( int itemID )
{
	return m_pDropDown->GetItemUserData( itemID );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ComboBox::IsDropdownVisible()
{
	return m_pDropDown->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: Set the visiblity of the drop down menu button.
//-----------------------------------------------------------------------------
void ComboBox::SetDropdownButtonVisible( bool state )
{
	m_pButton->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ComboBox::OnMenuItemSelected()
{
	m_bHighlight = true;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the direction from the menu button the menu should open
//-----------------------------------------------------------------------------
void ComboBox::SetOpenDirection( MenuDirection_e direction )
{
	m_iDirection = direction;
}

int ComboBox::AddItem( const char* text, const KeyValues* userData )
{
	// when the menu item is selected it will send the custom message "SetText"
	return m_pDropDown->AddMenuItem( text, new KeyValues( "SetText", "text", text ), this, userData );
}

bool ComboBox::UpdateItem( int itemID, const char* text, const KeyValues* userData )
{
	if ( !m_pDropDown->IsValidMenuID( itemID ) )
		return false;
	m_pDropDown->UpdateMenuItem( itemID, text, new KeyValues( "SetText", "text", text ), userData );
	InvalidateLayout( false, false );
	return true;
}

bool ComboBox::UpdateItem( int itemID, const wchar_t* text, const KeyValues* userData )
{
	if ( !m_pDropDown->IsValidMenuID( itemID ) )
		return false;
	m_pDropDown->UpdateMenuItem( itemID, text, new KeyValues( "SetText", "text", text ), userData );
	InvalidateLayout( false, false );
	return true;
}

int ComboBox::AddItem( const wchar_t* text, const KeyValues* userData )
{
#ifdef _WIN32
	int length = WideCharToMultiByte( CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL );
#else
	size_t bytes = std::wcstombs( NULL, text, 0 );
	if ( bytes >= INT_MAX )
		return m_pDropDown->GetInvalidMenuID();
	int length = (int)bytes + 1;
#endif
	if ( length <= 0 )
		return m_pDropDown->GetInvalidMenuID();
	CUtlVector<char> ansi;
	ansi.AddMultipleToTail( length );
	if ( localize()->ConvertUnicodeToANSI( text, ansi.Base(), length ) <= 0 )
		return m_pDropDown->GetInvalidMenuID();
	return m_pDropDown->AddMenuItem( ansi.Base(), text, new KeyValues( "SetText", "text", text ), this, userData );
}

//-----------------------------------------------------------------------------
// Purpose: Called when a command is received from the menu
//			Changes the label text to be that of the command
// Input  : char *command - 
//-----------------------------------------------------------------------------
void ComboBox::OnCommand( const char* command )
{
	if ( !_stricmp( command, "ButtonClicked" ) )
		DoClick();
	TextEntry::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the window loses focus; hides the menu
//-----------------------------------------------------------------------------
void ComboBox::OnKillFocus()
{
	SelectNoText();
}

void ComboBox::SetUseFallbackFont( bool state, HFont font )
{
	TextEntry::SetUseFallbackFont( state, font );
	m_pDropDown->SetUseFallbackFont( state, font );
}

void ComboBox::HideMenu()
{
	if ( !m_pDropDown )
		return;

	// hide the menu
	m_pDropDown->SetVisible( false );
	Repaint();
	OnHideMenu( m_pDropDown );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
KeyValues* ComboBox::GetActiveItemUserData()
{
	return m_pDropDown->GetItemUserData( GetActiveItem() );
}

void ComboBox::OnSetText( const wchar_t* text )
{
	if ( text[0] == L'#' )
	{
		char ansi[255];
		localize()->ConvertUnicodeToANSI( text, ansi, sizeof( ansi ) );
		StringIndex_t token = localize()->FindIndex( ansi + 1 );
		if ( token != (StringIndex_t)-1 )
			text = localize()->GetValueByIndex( token );
	}
	wchar_t currentText[255];
	GetText( currentText, 254 );
	if ( wcscmp( currentText, text ) )
	{
		// text has changed
		SetText( text );

		// fire off that things have changed
		PostActionSignal( new KeyValues( "TextChanged", "text", text ) );
		Repaint();
	}
	// close the box
	HideMenu();
}

void ComboBox::DoMenuLayout()
{
	// move the menu to the correct place below the button
	int x, y, wide, tall;
	GetSize( wide, tall );
	if ( m_iDirection == CURSOR )
		input()->GetCursorPos( x, y );
	else if ( m_iDirection == ALIGN_WITH_PARENT && GetVParent() )
	{
		x = 0;
		y = tall;
		ParentLocalToScreen( x, y );
		x -= GetProportionalScaledValue( 1 );
		y += m_iOpenOffsetY;
	}
	else
	{
		x = 0;
		y = tall;
		LocalToScreen( x, y );
	}
	int menuWide, menuTall;
	m_pDropDown->GetSize( menuWide, menuTall );
	int boxWide, boxTall;
	GetSize( boxWide, boxTall );
	if ( m_iDirection == UP )
		m_pDropDown->SetPos( x, y - menuTall - boxTall - GetProportionalScaledValue( 1 ) );
	else if ( m_iDirection == DOWN )
		m_pDropDown->SetPos( x, y + GetProportionalScaledValue( 1 ) );
	else
	{
		int menuY = y + GetProportionalScaledValue( 1 );
		int menuX = x + GetProportionalScaledValue( 1 );
		m_pDropDown->SetPos( menuX, menuY );
	}

	// reset the width of the drop down menu to be the width of the combo box
	m_pDropDown->SetFixedWidth( GetWide() );
}

//-----------------------------------------------------------------------------
// Purpose: Handles hotkey accesses
// FIXME: make this open different directions as necessary see menubutton.
//-----------------------------------------------------------------------------
void ComboBox::DoClick()
{
	// menu is already visible, hide the menu
	if ( m_pDropDown->IsVisible() )
	{
		HideMenu();
		return;
	}

	// do nothing if menu is not enabled
	if ( !m_pDropDown->IsEnabled() )
		return;
	// force the menu to Think
	m_pDropDown->PerformLayout();
	wchar_t text[255];
	GetText( text, 255 );
	for ( int row = 0; row < m_pDropDown->GetItemCount(); ++row )
	{
		wchar_t itemText[255];
		m_pDropDown->GetMenuItem( m_pDropDown->GetMenuID( row ) )->GetText( itemText, 255 );
		if ( !wcscmp( itemText, text ) )
		{
			m_pDropDown->SetCurrentlyHighlightedItem( row );
			break;
		}
	}

	// reset the dropdown's position
	DoMenuLayout();


	// make sure we're at the top of the draw order (and therefore our children as well)
	// this important to make sure the menu will be drawn in the foreground
	MoveToFront();

	// notify
	OnShowMenu( m_pDropDown );

	// show the menu
	m_pDropDown->SetVisible( true );

	// bring to focus
	m_pDropDown->RequestFocus( 0 );

	// no text is highlighted when the menu is opened
	SelectNoText();

	// highlight the arrow while menu is open
	m_pButton->SetArmed( true );
	Repaint();
}

void ComboBox::ShowMenu()
{
	if ( !m_pDropDown )
		return;

	// hide the menu
	m_pDropDown->SetVisible( false );
	DoClick();
}

void ComboBox::MoveAlongMenuItemList( int direction )
{
	wchar_t text[255];
	GetText( text, 254 );
	int selected = -1;
	for ( int row = 0; row < m_pDropDown->GetItemCount(); ++row )
	{
		wchar_t itemText[255];
		m_pDropDown->GetMenuItem( m_pDropDown->GetMenuID( row ) )->GetText( itemText, 254 );
		if ( !wcscmp( itemText, text ) )
		{
			selected = row;
			break;
		}
	}
	if ( selected < 0 )
		return;
	int row = selected + direction;
	if ( row < 0 )
		row = 0;
	else if ( row >= m_pDropDown->GetItemCount() )
		row = m_pDropDown->GetItemCount() - 1;
	wchar_t itemText[255];
	m_pDropDown->GetMenuItem( m_pDropDown->GetMenuID( row ) )->GetText( itemText, 255 );
	OnSetText( itemText );
	SelectAllText( false );
	m_pDropDown->ActivateItem( row );
}

void ComboBox::PerformLayout()
{
	int wide, tall;
	GetPaintSize( wide, tall );
	TextEntry::PerformLayout();
	int buttonTall = tall - GetProportionalScaledValue( 2 );
	int buttonWide = GetProportionalScaledValue( 16 );
	int buttonY = GetProportionalScaledValue( 2 );
	int buttonX = wide - GetProportionalScaledValue( 20 );
	m_pButton->SetBounds( buttonX, buttonY, buttonWide, buttonTall );
	SetCursor( IsEditable() ? dc_ibeam : dc_arrow );
	m_pButton->SetEnabled( IsEnabled() );
	DoMenuLayout();
}

void ComboBox::ApplySchemeSettings( IScheme* comboScheme )
{
	TextEntry::ApplySchemeSettings( comboScheme );
	SetBorder( comboScheme->GetBorder( "ComboBoxBorder" ) );
}

//-----------------------------------------------------------------------------
// Purpose: overloads TextEntry MousePressed
//-----------------------------------------------------------------------------
void ComboBox::OnMousePressed( MouseCode code )
{
	if ( !m_pDropDown || !IsEnabled() )
		return;

	// make sure it's getting pressed over us (it may not be due to mouse capture)
	if ( !IsCursorOver() )
	{
		HideMenu();
		return;
	}
	if ( IsEditable() )
	{
		TextEntry::OnMousePressed( code );
		HideMenu();
	}
	else
	{
		// clicking on a non-editable text box just activates the drop down menu
		RequestFocus( 0 );
		DoClick();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Double-click acts the same as a single-click
//-----------------------------------------------------------------------------
void ComboBox::OnMouseDoublePressed( MouseCode code )
{
	if ( IsEditable() )
		TextEntry::OnMouseDoublePressed( code );
	else
		OnMousePressed( code );
}


//-----------------------------------------------------------------------------
// Purpose: Brighten the arrow on the button when entering the box
//-----------------------------------------------------------------------------
void ComboBox::OnCursorEntered()
{
	// want the arrow to go white when we enter the box 
	m_pButton->OnCursorEntered();
	TextEntry::OnCursorEntered();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the menu is closed
//-----------------------------------------------------------------------------
void ComboBox::OnMenuClose()
{
	HideMenu();
	if ( HasFocus() )
		SelectAllText( false );
	else if ( m_bHighlight )
	{
		m_bHighlight = false;
		// we want the text to be highlighted when we request the focus
//		SelectAllOnFirstFocus(true);
		RequestFocus( 0 );
	}
	// if cursor is in this box or the arrow box
	else if ( IsCursorOver() ) // make sure it's getting pressed over us (it may not be due to mouse capture)
	{
		SelectAllText( false );
		OnCursorEntered();
		// Get focus so the box will unhighlight if we click somewhere else.
		RequestFocus( 0 );
	}
	else
		m_pButton->SetArmed( false );
}

//-----------------------------------------------------------------------------
// Purpose: Dim the arrow on the button when exiting the box
//-----------------------------------------------------------------------------
void ComboBox::OnCursorExited()
{
	if ( m_pDropDown->IsVisible() )
		return;
	m_pButton->SetArmed( false );
	TextEntry::OnCursorExited();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ComboBox::OnSizeChanged( int wide, int tall )
{
	TextEntry::OnSizeChanged( wide, tall );
	PerformLayout();
	int buttonWide, buttonTall;
	m_pButton->GetSize( buttonWide, buttonTall );
	SetDrawWidth( wide - buttonWide );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ComboBox::OnSetFocus()
{
	TextEntry::OnSetFocus();
	GotoTextEnd();
	SelectAllText( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ComboBox::OnKeyCodeTyped( KeyCode code )
{
	switch ( code )
	{
	case KEY_ENTER:
	case KEY_SPACE:
		DoClick();
		break;
	case KEY_UP:
		if ( m_pDropDown->IsVisible() )
			MoveAlongMenuItemList( -1 );
		else
			TextEntry::OnKeyCodeTyped( code );
		break;
	case KEY_DOWN:
		if ( m_pDropDown->IsVisible() )
			MoveAlongMenuItemList( 1 );
		else
			TextEntry::OnKeyCodeTyped( code );
		break;
	default:
		TextEntry::OnKeyCodeTyped( code );
		break;
	}
}

void ComboBox::OnKeyTyped( wchar_t character )
{
	if ( IsEditable() )
	{
		TextEntry::OnKeyTyped( character );
		return;
	}
	int selected = m_pDropDown->GetActiveItem();
	if ( selected < 0 )
		selected = 0;
	int row = selected + 1;
	if ( row >= m_pDropDown->GetItemCount() )
		row = 0;
	wchar_t text[255];
	while ( row != selected )
	{
		m_pDropDown->GetMenuItem( m_pDropDown->GetMenuID( row ) )->GetText( text, 254 );
		if ( towlower( text[0] ) == towlower( character ) )
		{
			selected = row;
			break;
		}
		if ( ++row >= m_pDropDown->GetItemCount() )
			row = 0;
	}
	if ( selected >= m_pDropDown->GetItemCount() )
	{
		TextEntry::OnKeyTyped( character );
		return;
	}
	m_pDropDown->GetMenuItem( m_pDropDown->GetMenuID( selected ) )->GetText( text, 255 );
	OnSetText( text );
	SelectAllText( false );
	m_pDropDown->ActivateItem( selected );
}
}
