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

#include "Menu.h"
#include "MenuItem.h"
#include "ScrollBar.h"
#include "TextImage.h"
#include "controls.h"

#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"

#include <cstring>
#include <cwctype>

namespace vgui2
{
void Menu::OnMouseWheeled( int delta )
{
	if ( m_pScroller->IsVisible() )
	{
		m_pScroller->SetValue( m_pScroller->GetValue() - delta );
		InvalidateLayout( false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Makes menu items visible in relation to where the scroll bar is
//-----------------------------------------------------------------------------
void Menu::MakeItemsVisibleInScrollRange()
{
	int visibleCount = 0;
	int start = m_pScroller->GetValue();
	do
	{
		if ( start >= m_SortedItems.Count() )
			break;

		for ( int row = 0; row < start; ++row )
		{
			m_MenuItems[m_SortedItems[row]]->SetVisible( false );
		}

		int row = start;
		for ( ; visibleCount < m_iNumVisibleLines; ++visibleCount )
		{
			if ( row >= m_SortedItems.Count() )
				break;
			m_MenuItems[m_SortedItems[row]]->SetVisible( true );
			++row;
		}

		for ( ; row < m_SortedItems.Count(); ++row )
		{
			m_MenuItems[m_SortedItems[row]]->SetVisible( false );
		}

		// make sure we have enough items in the menu
		if ( visibleCount < m_iNumVisibleLines )
		{
			start--;
			visibleCount = 0;
		}
	} while ( visibleCount < m_iNumVisibleLines - 1 );
}

void Menu::Paint()
{
	if ( m_pScroller->IsVisible() )
	{
		int width;
		int height;
		Panel::GetSize( width, height );
		ISurface* drawingSurface = surface();
		drawingSurface->DrawSetColor( _borderDark );
		IsProportional();
		drawingSurface = surface();
		int dividerX = width - m_iProportionalScrollBarSize;
		int dividerRight = dividerX + 1;
		drawingSurface->DrawFilledRect( dividerX, -1, dividerRight, height );
	}
}

void Menu::SetBgColor( Color color )
{
	Panel::SetBgColor( color );
	for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex(); itemID = m_MenuItems.Next( itemID ) )
	{
		if ( m_MenuItems[itemID]->HasMenu() )
			m_MenuItems[itemID]->GetMenu()->SetBgColor( color );
	}
}

void Menu::SetFgColor( Color color )
{
	Panel::SetFgColor( color );
	for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex(); itemID = m_MenuItems.Next( itemID ) )
	{
		if ( m_MenuItems[itemID]->HasMenu() )
			m_MenuItems[itemID]->GetMenu()->SetFgColor( color );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Respond to string commands.
//-----------------------------------------------------------------------------
void Menu::OnCommand( const char* command )
{
	KeyValues* message = new KeyValues( "Command", "command", command );
	PostActionSignal( message );
	Panel::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Return which type of events the menu is currently interested in
//  MenuItems need to know because behaviour is different depending on mode.
//-----------------------------------------------------------------------------
int Menu::GetMenuMode()
{
	return m_iInputMode;
}

//-----------------------------------------------------------------------------
// Purpose: Locks down a specific width
//-----------------------------------------------------------------------------
void Menu::SetFixedWidth( int width )
{
	// the padding makes it so the menu has the label padding on each side of the menu.
	// makes the menu items look centered.
	m_iFixedWidth = width;
	InvalidateLayout( false, false );
}

void Menu::SetNumberOfVisibleItems( int count )
{
	m_iNumVisibleLines = count;
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int Menu::GetActiveItem()
{
	return m_iActivatedItem;
}

//-----------------------------------------------------------------------------
// Purpose: Set the menu to key mode if a child menu goes into keymode
// This mode change has to be chained up through the menu heirarchy
// so cascading menus will work when you do a bunch of stuff in keymode
// in high level menus and then switch to keymode in lower level menus.
//-----------------------------------------------------------------------------
void Menu::OnKeyModeSet()
{
	m_iInputMode = KEYBOARD;
}

void Menu::SetFont( HFont font )
{
	m_hItemFont = font;
	if ( font )
		m_iMenuItemHeight = surface()->GetFontTall( font ) + 2;
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Return the number of items currently in the menu list
//-----------------------------------------------------------------------------
int Menu::GetItemCount()
{
	return m_MenuItems.Count();
}

int Menu::GetMenuID( int index )
{
	if ( index < 0 || index >= m_SortedItems.Count() )
		return -1;
	return m_SortedItems[index];
}

void Menu::SetBorder( IBorder* border )
{
	Panel::SetBorder( border );
}

void Menu::LayoutMenuBorder()
{
	ISchemeManager* manager = scheme();
	HScheme handle = GetScheme();
	IScheme* menuScheme = manager->GetIScheme( handle );
	IBorder* border = menuScheme->GetBorder( "MenuBorder" );
	if ( border )
		SetBorder( border );
}

int Menu::AddMenuItem( const char* text, Panel* target, const KeyValues* userData )
{
	return AddMenuItem( text, text, target, userData );
}

int Menu::AddMenuItem( const char* text, KeyValues* command, Panel* target, const KeyValues* userData )
{
	return AddMenuItem( text, text, command, target, userData );
}

int Menu::AddMenuItem( const char* text, const char* command, Panel* target, const KeyValues* userData )
{
	return AddMenuItem( text, text, command, target, userData );
}

int Menu::AddCheckableMenuItem( const char* text, Panel* target, const KeyValues* userData )
{
	return AddCheckableMenuItem( text, text, target, userData );
}

int Menu::AddCheckableMenuItem( const char* text, KeyValues* command, Panel* target, const KeyValues* userData )
{
	return AddCheckableMenuItem( text, text, command, target, userData );
}

int Menu::AddCheckableMenuItem( const char* text, const char* command, Panel* target, const KeyValues* userData )
{
	return AddCheckableMenuItem( text, text, command, target, userData );
}

int Menu::AddCascadingMenuItem( const char* text, Panel* target, Menu* menu, const KeyValues* userData )
{
	return AddCascadingMenuItem( text, text, target, menu, userData );
}

int Menu::AddCascadingMenuItem(
	const char* text, KeyValues* command, Panel* target, Menu* menu, const KeyValues* userData )
{
	return AddCascadingMenuItem( text, text, command, target, menu, userData );
}

int Menu::AddCascadingMenuItem(
	const char* text, const char* command, Panel* target, Menu* menu, const KeyValues* userData )
{
	return AddCascadingMenuItem( text, text, command, target, menu, userData );
}


//-----------------------------------------------------------------------------
// Purpose: Force the menu to work out how wide it should be
//-----------------------------------------------------------------------------
void Menu::ForceCalculateWidth()
{
	_recalculateWidth = true;
	CalculateWidth();
	PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Figure out how wide the menu should be if the menu is not fixed width
//-----------------------------------------------------------------------------
void Menu::CalculateWidth()
{
	if ( !_recalculateWidth )
		return;

	_menuWide = 0;
	int candidateWidth = 0;
	if ( m_iFixedWidth == 0 )
	{
		for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex();
			itemID = m_MenuItems.Next( itemID ) )
		{
			int width;
			int height;
			m_MenuItems[itemID]->GetContentSize( width, height );
			if ( width > _menuWide - 8 )
				_menuWide = width + 8;
		}
		candidateWidth = _menuWide;
	}
	if ( candidateWidth < m_iMinimumWidth )
		_menuWide = m_iMinimumWidth;
	_recalculateWidth = false;
}

//-----------------------------------------------------------------------------
// Purpose: When a menuItem is selected, close cascading menus
//          if the menuItem selected has a cascading menu attached, we
//          want to keep that one open so skip it.
//          Passing NULL will close all cascading menus.
//-----------------------------------------------------------------------------
void Menu::CloseOtherMenus( MenuItem* item )
{
	for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex(); itemID = m_MenuItems.Next( itemID ) )
	{
		if ( m_MenuItems[itemID] != item )
			m_MenuItems[itemID]->CloseCascadeMenu();
	}
}

KeyValues* Menu::GetItemUserData( int itemID )
{
	if ( !m_MenuItems.IsValidIndex( itemID ) )
		return NULL;

	MenuItem* item = m_MenuItems[itemID];
	if ( item && item->IsEnabled() )
		return item->GetUserData();
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
MenuItem* Menu::GetMenuItem( int itemID )
{
	if ( !m_MenuItems.IsValidIndex( itemID ) )
		return NULL;
	return m_MenuItems[itemID];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool Menu::IsValidMenuID( int itemID )
{
	return m_MenuItems.IsValidIndex( itemID );
}

MenuItem* Menu::GetParentMenuItem()
{
	return dynamic_cast<MenuItem*>( GetParent() );
}

void Menu::SetItemEnabled( int itemID, bool state )
{
	if ( m_MenuItems.IsValidIndex( itemID ) )
		m_MenuItems[itemID]->SetEnabled( state );
}

void Menu::SetItemEnabled( const char* itemName, bool state )
{
	for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex(); itemID = m_MenuItems.Next( itemID ) )
	{
		const char* currentName = m_MenuItems[itemID]->GetName();
		if ( _stricmp( itemName, currentName ) == 0 )
			m_MenuItems[itemID]->SetEnabled( state );
	}
}

void Menu::SetItemVisible( int itemID, bool state )
{
	if ( m_MenuItems.IsValidIndex( itemID ) )
		m_MenuItems[itemID]->SetVisible( state );
}

void Menu::SetItemVisible( const char* itemName, bool state )
{
	for ( int item = m_MenuItems.Head(); item != m_MenuItems.InvalidIndex(); item = m_MenuItems.Next( item ) )
	{
		if ( _stricmp( itemName, m_MenuItems[item]->GetName() ) == 0 )
			m_MenuItems[item]->SetVisible( state );
	}
}

void Menu::DeleteAllItems()
{
	for ( int index = m_MenuItems.Head(); index != m_MenuItems.InvalidIndex(); index = m_MenuItems.Next( index ) )
		m_MenuItems[index]->MarkForDeletion();

	m_MenuItems.RemoveAll();
	m_SortedItems.RemoveAll();
	InvalidateLayout( false, false );
}

int Menu::AddMenuItem(
	const char* itemName, const wchar_t* itemText, KeyValues* command, Panel* target, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, NULL, false );
	return AddMenuItemKeyValuesCommand( item, command, target, userData );
}

int Menu::AddMenuItem(
	const char* itemName, const char* itemText, KeyValues* command, Panel* target, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, NULL, false );
	return AddMenuItemKeyValuesCommand( item, command, target, userData );
}

int Menu::AddMenuItem(
	const char* itemName, const wchar_t* itemText, const char* command, Panel* target, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, NULL, false );
	return AddMenuItemCharCommand( item, command, target, userData );
}

int Menu::AddMenuItem(
	const char* itemName, const char* itemText, const char* command, Panel* target, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, NULL, false );
	return AddMenuItemCharCommand( item, command, target, userData );
}

int Menu::AddCheckableMenuItem(
	const char* itemName, const wchar_t* itemText, KeyValues* command, Panel* target, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, NULL, true );
	return AddMenuItemKeyValuesCommand( item, command, target, userData );
}

int Menu::AddCheckableMenuItem(
	const char* itemName, const char* itemText, KeyValues* command, Panel* target, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, NULL, true );
	return AddMenuItemKeyValuesCommand( item, command, target, userData );
}

int Menu::AddCheckableMenuItem(
	const char* itemName, const wchar_t* itemText, const char* command, Panel* target, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, NULL, true );
	return AddMenuItemCharCommand( item, command, target, userData );
}

int Menu::AddCheckableMenuItem(
	const char* itemName, const char* itemText, const char* command, Panel* target, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, NULL, true );
	return AddMenuItemCharCommand( item, command, target, userData );
}

int Menu::AddCascadingMenuItem( const char* itemName, const wchar_t* itemText, KeyValues* command, Panel* target,
	Menu* cascadingMenu, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, cascadingMenu, false );
	return AddMenuItemKeyValuesCommand( item, command, target, userData );
}

int Menu::AddCascadingMenuItem( const char* itemName, const char* itemText, KeyValues* command, Panel* target,
	Menu* cascadingMenu, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, cascadingMenu, false );
	return AddMenuItemKeyValuesCommand( item, command, target, userData );
}

int Menu::AddCascadingMenuItem( const char* itemName, const wchar_t* itemText, const char* command, Panel* target,
	Menu* cascadingMenu, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, cascadingMenu, false );
	return AddMenuItemCharCommand( item, command, target, userData );
}

int Menu::AddCascadingMenuItem( const char* itemName, const char* itemText, const char* command, Panel* target,
	Menu* cascadingMenu, const KeyValues* userData )
{
	MenuItem* item = new MenuItem( this, itemName, itemText, cascadingMenu, false );
	return AddMenuItemCharCommand( item, command, target, userData );
}

//-----------------------------------------------------------------------------
// Purpose: Make the scroll bar visible and narrow the menu
//			also make items visible or invisible in the list as appropriate
//-----------------------------------------------------------------------------
void Menu::AddScrollBar()
{
	m_pScroller->SetVisible( true );
	_sizedForScrollBar = true;
}

//-----------------------------------------------------------------------------
// Purpose: Make the scroll bar invisible and widen the menu
//-----------------------------------------------------------------------------
void Menu::RemoveScrollBar()
{
	m_pScroller->SetVisible( false );
	_sizedForScrollBar = false;
}

Menu::~Menu()
{
	if ( m_pScroller )
		delete m_pScroller;
	m_SortedItems.Purge();
}

int Menu::AddMenuItem( MenuItem* item )
{
	item->SetParent( this );
	int itemID = m_MenuItems.AddToTail( item );
	m_SortedItems.AddToTail( itemID );
	InvalidateLayout( false, false );
	_recalculateWidth = true;
	if ( m_hItemFont )
		item->SetFont( m_hItemFont );
	if ( m_bUseFallbackFont && m_hFallbackItemFont )
	{
		TextImage* text = item->GetTextImage();
		if ( text )
			text->SetUseFallbackFont( m_bUseFallbackFont, m_hFallbackItemFont );
	}
	return itemID;
}

//-----------------------------------------------------------------------------
// Purpose: Activate the n'th item in the menu list, as if that menu item had been selected by the user
//-----------------------------------------------------------------------------
void Menu::ActivateItem( int itemID )
{
	if ( m_MenuItems.IsValidIndex( itemID ) )
	{
		MenuItem* item = m_MenuItems.Element( itemID );
		if ( item && item->IsEnabled() )
		{
			item->DoClick();
			m_iActivatedItem = itemID;
		}
	}
}

void Menu::ActivateItemByRow( int row )
{
	if ( m_SortedItems.IsValidIndex( row ) )
		ActivateItem( m_SortedItems.Element( row ) );
}

//-----------------------------------------------------------------------------
// Purpose: Invalidate layout if the slider is moved so items scroll 
//-----------------------------------------------------------------------------
void Menu::OnSliderMoved()
{
	CloseOtherMenus( NULL ); // close any cascading menus
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the item currently highlighted in the menu by index
//-----------------------------------------------------------------------------
void Menu::SetCurrentlySelectedItem( int itemID )
{
	// dont deselect if its the same item
	if ( itemID == m_iCurrentlySelectedItemID )
		return;

	if ( m_MenuItems.IsValidIndex( m_iCurrentlySelectedItemID ) )
		m_MenuItems[m_iCurrentlySelectedItemID]->DisarmItem();

	KeyValues* message = new KeyValues( "MenuItemHighlight", "itemID", itemID );
	PostActionSignal( message );
	m_iCurrentlySelectedItemID = itemID;
}

int Menu::GetMenuItemID( MenuItem* item )
{
	for ( int index = m_MenuItems.Head(); index != m_MenuItems.InvalidIndex(); index = m_MenuItems.Next( index ) )
	{
		if ( m_MenuItems[index] == item )
			return index;
	}
	return m_MenuItems.InvalidIndex();
}

void Menu::OnCursorEnteredMenuItem( int VPanel )
{
	// if we are in mouse mode
	if ( m_iInputMode == MOUSE )
	{
		IPanel* panels = ipanel();
		const char* module = GetModuleName();
		MenuItem* item = (MenuItem*)panels->GetPanel( VPanel, module );
		item->ArmItem();
		// open the cascading menu if there is one.
		item->OpenCascadeMenu();
		int itemNum = GetMenuItemID( item );
		SetCurrentlySelectedItem( itemNum );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Respond to cursor exiting a menuItem
//-----------------------------------------------------------------------------
void Menu::OnCursorExitedMenuItem( int VPanel )
{
	// only care if we are in mouse mode
	if ( m_iInputMode == MOUSE )
	{
		IPanel* panels = ipanel();
		const char* module = GetModuleName();
		MenuItem* item = (MenuItem*)panels->GetPanel( VPanel, module );
		// unhighlight the item.
		// note menuItems with cascading menus will stay lit.
		item->DisarmItem();
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the height of each menu item
//-----------------------------------------------------------------------------
void Menu::SetMenuItemHeight( int itemHeight )
{
	m_iMenuItemHeight = itemHeight;
}

void Menu::UpdateMenuItem( int itemID, const char* text, KeyValues* command, const KeyValues* userData )
{
	if ( m_MenuItems.IsValidIndex( itemID ) )
	{
		MenuItem* item = m_MenuItems[itemID];
		if ( item != NULL )
		{
			item->SetText( text );
			item->SetCommand( command );
			if ( userData != NULL )
				item->SetUserData( userData );
		}
	}
}

void Menu::UpdateMenuItem( int itemID, const wchar_t* text, KeyValues* command, const KeyValues* userData )
{
	if ( m_MenuItems.IsValidIndex( itemID ) )
	{
		MenuItem* item = m_MenuItems[itemID];
		if ( item != NULL )
		{
			item->SetText( text );
			item->SetCommand( command );
			if ( userData != NULL )
				item->SetUserData( userData );
		}
	}
}

Menu::Menu( Panel* parent, const char* panelName )
	: Panel( parent, panelName ), m_MenuItems( 0, 0 ), m_SortedItems( 0, 0 )
{
	m_iFixedWidth = 0;
	m_iMinimumWidth = 0;
	m_iNumVisibleLines = 99;

	m_pScroller = new ScrollBar( this, "MenuScrollBar", true );
	m_pScroller->SetVisible( false );
	m_pScroller->AddActionSignalTarget( this );
	_sizedForScrollBar = false;
	SetZPos( 1 );
	SetVisible( false );
	MakePopup( false, false );
	SetParent( parent );
	m_iCurrentlySelectedItemID = -1;
	m_iInputMode = MOUSE;
	m_bUseFallbackFont = false;
	_recalculateWidth = true;
	m_iCheckImageWidth = 0;
	m_iActivatedItem = 0;
	m_hFallbackItemFont = 0;
	if ( IsProportional() )
	{
		int width;
		int height;
		surface()->GetProportionalBase( width, height );
		int screenWide;
		int screenTall;
		surface()->GetScreenSize( screenWide, screenTall );
		( void )height;
		( void )screenTall;
		float scrollBarSize = (float)( (double)screenWide / width * SCROLLBAR_SIZE );
		float itemHeight = (float)( (double)screenWide / width * DEFAULT_MENU_ITEM_HEIGHT );
		m_iProportionalScrollBarSize = (int)scrollBarSize;
		m_iMenuItemHeight = (int)itemHeight;
	}
	else
	{
		m_iProportionalScrollBarSize = SCROLLBAR_SIZE;
		m_iMenuItemHeight = DEFAULT_MENU_ITEM_HEIGHT;
	}
	m_hItemFont = 0;
}

int Menu::GetInvalidMenuID()
{
	return -1;
}

void Menu::OnCursorMoved( int x, int y )
{
	m_iInputMode = MOUSE;
	KeyValues* message = new KeyValues( "OnCursorMoved", "x", x, "y", y );
	CallParentFunction( message );
	RequestFocus( 0 );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Toggle into keyboard mode. 
//-----------------------------------------------------------------------------
void Menu::OnKeyCodePressed( KeyCode )
{
	m_iInputMode = KEYBOARD;
	if ( GetVParent() != 0 )
	{
		KeyValues* message = new KeyValues( "KeyModeSet" );
		ivgui()->PostMessage( GetVParent(), message, GetVPanel(), 0.0f );
	}
}

void Menu::OnKillFocus()
{
	if ( input()->GetFocus() == 0 )
	{
		if ( !IsKeyBoardInputEnabled() && input()->GetFocus() == 0 )
			return;
	}
	else if ( ipanel()->HasParent( input()->GetFocus(), GetVPanel() ) )
	{
		return;
	}
	else if ( !IsKeyBoardInputEnabled() && input()->GetFocus() == 0 )
	{
		return;
	}

	MenuItem* parentItem = NULL;
	Panel* parent = GetParent();
	if ( parent != NULL )
		parentItem = dynamic_cast<MenuItem*>( parent );
	if ( parentItem != NULL && input()->GetFocus() == parentItem->GetVParent() && m_iInputMode == MOUSE )
		MoveToFront();
	else
	{

		// forward the message to the parent.
		PostActionSignal( new KeyValues( "MenuClose" ) );

		// hide this menu
		SetVisible( false );
	}
}

int Menu::AddMenuItemCharCommand( MenuItem* item, const char* command, Panel* target, const KeyValues* userData )
{
	item->SetCommand( command );
	item->AddActionSignalTarget( target );
	item->SetUserData( userData );
	return AddMenuItem( item );
}

//-----------------------------------------------------------------------------
// Purpose: Add a menu item to the menu. 
// Input  : *itemName - Name of item 
//			*itemText - Name of item text that will appear in the manu.
//			*message - pointer to the message to send when the item is selected
//			*target - Target panel of the command
//          *cascadeMenu - if the menu item opens a cascading menu, this is a 
//							ptr to the menu that opens on selecting the item
// Output:  itemID - ID of this item
//-----------------------------------------------------------------------------
int Menu::AddMenuItemKeyValuesCommand( MenuItem* item, KeyValues* command, Panel* target, const KeyValues* userData )
{
	item->SetCommand( command );
	item->AddActionSignalTarget( target );
	item->SetUserData( userData );
	return AddMenuItem( item );
}

//-----------------------------------------------------------------------------
// Purpose: Set up the scroll bar attributes,size and location.
//-----------------------------------------------------------------------------
void Menu::LayoutScrollBar()
{
	//!! need to make it recalculate scroll positions
	m_pScroller->SetEnabled( false );
	m_pScroller->SetRangeWindow( m_iNumVisibleLines );
	m_pScroller->SetRange( 0, m_MenuItems.Count() );
	m_pScroller->SetButtonPressedScrollValue( 1 );
	int wide;
	int tall;
	GetSize( wide, tall );
	int left;
	int top;
	int right;
	int bottom;
	GetInset( left, top, right, bottom );
	wide -= top;
	m_pScroller->SetPos( wide - m_iProportionalScrollBarSize, 1 );
	m_pScroller->SetSize( m_iProportionalScrollBarSize, tall - bottom - right );
}

void Menu::PositionCascadingMenu()
{
	int parentWide;
	int parentTall;
	ipanel()->GetSize( GetVParent(), parentWide, parentTall );
	int parentX;
	int parentY;
	ipanel()->GetPos( GetVParent(), parentX, parentY );
	parentY = 0;
	parentX += parentWide;
	ParentLocalToScreen( parentX, parentY );
	SetPos( parentX, parentY );

	int x;
	int y;
	int wide;
	int tall;
	GetBounds( x, y, wide, tall );
	int workX;
	int workY;
	int workWide;
	int workTall;
	surface()->GetWorkspaceBounds( workX, workY, workWide, workTall );
	int placedX;
	if ( x + wide > workX + workWide )
		placedX = x - ( parentWide + wide ) - 2;
	else
		placedX = x + 1;
	SetPos( placedX, y - 1 );
	MoveToFront();
}

void Menu::PerformLayout()
{
	if ( m_iNumVisibleLines >= m_SortedItems.Count() )
		RemoveScrollBar();
	else
	{
		// add it to the display
		AddScrollBar();
		MakeItemsVisibleInScrollRange();
	}

	// get the appropriate menu border
	LayoutMenuBorder();

	int left;
	int top;
	int right;
	int bottom;
	GetInset( left, top, right, bottom );
	( void )left;
	( void )top;
	int itemHeight = m_iMenuItemHeight;

	int contentTall = 0;
	for ( int row = 0; row < m_SortedItems.Count(); ++row )
	{
		MenuItem* item = m_MenuItems[m_SortedItems[row]];
		if ( item == NULL )
			continue;
		if ( !item->IsVisible() )
			continue;
		item->SetPos( 0, contentTall );
		if ( m_hItemFont != 0 )
			item->SetFont( m_hItemFont );
		contentTall += itemHeight;
		if ( item->IsCheckable() || m_iCheckImageWidth <= 0 )
		{
			if ( item->IsCheckable() )
				item->SetTextInset( 0, 0 );
		}
		else
		{
			item->SetTextInset( m_iCheckImageWidth, 0 ); //TODO: for some reason I can't comment this out.
		}
	}

	if ( m_iFixedWidth != 0 )
	{
		_menuWide = m_iFixedWidth;
		if ( _sizedForScrollBar )
			_menuWide = m_iFixedWidth - m_iProportionalScrollBarSize;
	}
	else if ( _recalculateWidth )
	{
		CalculateWidth();
	}

	int insetLeft;
	int insetTop;
	int insetRight;
	int insetBottom;
	GetInset( insetLeft, insetTop, insetRight, insetBottom );
	( void )insetRight;
	( void )insetBottom;
	int scaledHeight = m_iMenuItemHeight;
	int itemWide = _menuWide - insetLeft - insetTop;
	for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex(); itemID = m_MenuItems.Next( itemID ) )
	{
		MenuItem* item = m_MenuItems[itemID];
		if ( item != NULL )
			item->SetSize( itemWide, scaledHeight );
	}

	int scrollWide = 0;
	if ( _sizedForScrollBar )
		scrollWide = m_iProportionalScrollBarSize;
	int menuWide = _menuWide + scrollWide;
	int menuTall = bottom + right + contentTall;
	SetSize( menuWide, menuTall );

	Panel* parent = GetParent();
	if ( parent != NULL && dynamic_cast<MenuItem*>( parent ) != NULL )
		PositionCascadingMenu();
	if ( m_pScroller->IsVisible() )
		LayoutScrollBar();
	for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex(); itemID = m_MenuItems.Next( itemID ) )
		m_MenuItems[itemID]->InvalidateLayout( false, false );
	Repaint();
}

void Menu::SizeMenuItems()
{
	int left;
	int top;
	int right;
	int bottom;
	GetInset( left, top, right, bottom );
	( void )right;
	( void )bottom;
	int itemTall = m_iMenuItemHeight;
	int itemWide = _menuWide - left - top;
	for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex(); itemID = m_MenuItems.Next( itemID ) )
	{
		MenuItem* item = m_MenuItems[itemID];
		if ( item != NULL )
			item->SetSize( itemWide, itemTall );
	}
}

int Menu::GetCurrentlyVisibleItemsCount()
{
	int count = m_MenuItems.Count();
	if ( m_iNumVisibleLines <= count )
		return m_iNumVisibleLines;
	return count;
}

void Menu::ClearCurrentlyHighlightedItem()
{
	if ( m_MenuItems.IsValidIndex( m_iCurrentlySelectedItemID ) )
		m_MenuItems[m_iCurrentlySelectedItemID]->DisarmItem();
	m_iCurrentlySelectedItemID = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Set visibility of menu and its children as appropriate. 
//-----------------------------------------------------------------------------
void Menu::SetVisible( bool state )
{
	if ( IsVisible() == state )
		return;

	if ( state )
	{
		MoveToFront();
		RequestFocus( 0 );
	}
	else
	{
		PostActionSignal( new KeyValues( "MenuClose" ) );
		for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex();
			itemID = m_MenuItems.Next( itemID ) )
		{
			MenuItem* item = m_MenuItems[itemID];
			if ( item != NULL )
				item->CloseCascadeMenu();
		}
		SetCurrentlySelectedItem( -1 );
	}
	Panel::SetVisible( state );
	_sizedForScrollBar = false;
}

void Menu::SetCurrentlySelectedItem( MenuItem* item )
{
	int itemID = m_MenuItems.Head();
	if ( itemID != m_MenuItems.InvalidIndex() )
	{
		while ( m_MenuItems[itemID] != item )
		{
			itemID = m_MenuItems.Next( itemID );
			if ( itemID == m_MenuItems.InvalidIndex() )
				break;
		}
	}
	SetCurrentlySelectedItem( itemID );
}

//-----------------------------------------------------------------------------
// This will set the item to be currenly selected and highlight it
// will not open cascading menu. This was added for comboboxes
// to have the combobox item highlighted in the menu when they open the
// dropdown.
//-----------------------------------------------------------------------------
void Menu::SetCurrentlyHighlightedItem( int itemID )
{
	SetCurrentlySelectedItem( itemID );
	int row = -1;
	int count = m_SortedItems.Count();
	if ( count > 0 )
	{
		if ( itemID == m_SortedItems[0] )
		{
			row = 0;
		}
		else
		{
			for ( int index = 1; index < count; ++index )
			{
				if ( m_SortedItems[index] == itemID )
				{
					row = index;
					break;
				}
			}
		}
	}

	if ( m_pScroller->IsVisible()
		 && ( m_iNumVisibleLines + m_pScroller->GetValue() <= row || m_pScroller->GetValue() > row ) )
	{
		if ( !m_pScroller->IsVisible() )
			return;
		m_pScroller->SetValue( row );
	}

	if ( m_MenuItems.IsValidIndex( m_iCurrentlySelectedItemID ) )
	{
		MenuItem* item = m_MenuItems[m_iCurrentlySelectedItemID];
		if ( !item->IsArmed() )
			item->ArmItem();
	}
}

void Menu::OnKeyTyped( wchar_t unichar )
{
	int start = m_iCurrentlySelectedItemID;
	if ( start < 0 )
		start = 0;
	int itemToSelect = start;
	int itemID = start + 1;
	if ( itemID >= m_MenuItems.Count() )
		itemID = 0;
	if ( start != itemID )
	{
		wchar_t menuItemName[262];
		int candidate = itemID;
		while ( true )
		{
			m_MenuItems[candidate]->GetText( menuItemName, 254 );
			if ( std::towlower( menuItemName[0] ) == std::towlower( unichar ) )
			{
				if ( candidate < 0 )
					return;
				itemID = candidate;
				break;
			}
			++candidate;
			if ( candidate >= m_MenuItems.Count() )
				candidate = 0;
			if ( itemToSelect == candidate )
			{
				itemID = candidate;
				break;
			}
		}
	}
	SetCurrentlyHighlightedItem( itemID );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int Menu::GetCurrentlyHighlightedItem()
{
	return m_iCurrentlySelectedItemID;
}

void Menu::MoveAlongMenuItemList( int direction, int loopCount )
{
	int count = m_SortedItems.Count();
	int selected = m_iCurrentlySelectedItemID;
	int row = -1;
	if ( count > 0 )
	{
		row = 0;
		if ( selected != m_SortedItems[0] )
		{
			row = -1;
			for ( int index = 1; index < count; ++index )
			{
				if ( selected == m_SortedItems[index] )
				{
					row = index;
					break;
				}
			}
		}
	}

	int position = direction + row;
	int selectedRow;
	if ( position >= count )
	{
		if ( !m_pScroller->IsVisible() )
		{
			selectedRow = 0;
			position = 0;
		}
		else
		{
			position = count - 1;
			selectedRow = position;
		}
	}
	else if ( position >= 0 )
	{
		selectedRow = position;
	}
	else if ( !m_pScroller->IsVisible() )
	{
		position = count - 1;
		selectedRow = position;
	}
	else
	{
		position = m_pScroller->GetValue();
		selectedRow = position;
	}

	if ( m_pScroller->IsVisible() )
	{
		int value = m_pScroller->GetValue();
		if ( position >= m_iNumVisibleLines + value || position < m_pScroller->GetValue() )
		{
			m_pScroller->SetValue( direction + m_pScroller->GetValue() );
			InvalidateLayout( false, false );
		}
		if ( position >= m_iNumVisibleLines + m_pScroller->GetValue() || position < m_pScroller->GetValue() )
			m_pScroller->SetValue( position );
	}

	// switch it back to an itemID from row
	SetCurrentlySelectedItem( m_SortedItems[selectedRow] );

	// don't allow us to loop around more than once
	if ( loopCount < m_MenuItems.Count() )
	{
		wchar_t text[263];
		MenuItem* item = m_MenuItems[m_iCurrentlySelectedItemID];
		item->GetText( text, 255 );
		if ( text[0] == 0 || !item->IsVisible() )
			MoveAlongMenuItemList( direction, loopCount + 1 );
	}
}

void Menu::OnKeyCodeTyped( KeyCode code )
{
	if ( input()->IsKeyDown( KEY_LALT ) || input()->IsKeyDown( KEY_RALT ) )
	{
		Panel::OnKeyCodeTyped( code );
		PostActionSignal( new KeyValues( "MenuClose" ) );
	}

	switch ( code )
	{
	case KEY_ENTER:
		if ( m_MenuItems.IsValidIndex( m_iCurrentlySelectedItemID ) )
			ActivateItem( m_iCurrentlySelectedItemID );
		else
			Panel::OnKeyCodeTyped( code );
		return;
	case KEY_ESCAPE:
		SetVisible( false );
		return;
	case KEY_UP:
		MoveAlongMenuItemList( UP, 0 );
		m_MenuItems[m_iCurrentlySelectedItemID]->ArmItem();
		return;
	case KEY_LEFT:
		if ( dynamic_cast<MenuItem*>( GetParent() ) != NULL )
			SetVisible( false );
		else
			Panel::OnKeyCodeTyped( KEY_LEFT );
		return;
	case KEY_DOWN:
		MoveAlongMenuItemList( DOWN, 0 );
		m_MenuItems[m_iCurrentlySelectedItemID]->ArmItem();
		return;
	case KEY_RIGHT:
		if ( !m_MenuItems.IsValidIndex( m_iCurrentlySelectedItemID )
			 || !m_MenuItems[m_iCurrentlySelectedItemID]->HasMenu() )
			Panel::OnKeyCodeTyped( KEY_RIGHT );
		else
			ActivateItem( m_iCurrentlySelectedItemID );
		return;
	default:
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the checked state of a menuItem
//-----------------------------------------------------------------------------
void Menu::SetMenuItemChecked( int itemID, bool state )
{
	m_MenuItems[itemID]->SetChecked( state );
}

//-----------------------------------------------------------------------------
// Purpose: Check if item is checked.
//-----------------------------------------------------------------------------
bool Menu::IsChecked( int itemID )
{
	return m_MenuItems[itemID]->IsChecked();
}

//-----------------------------------------------------------------------------
// Purpose: Set the minmum width the menu has to be. This
// is useful if you have a menu that is sized to the largest item in it
// but you don't want the menu to be thinner than the menu button
//-----------------------------------------------------------------------------
void Menu::SetMinimumWidth( int width )
{
	m_iMinimumWidth = width;
}

//-----------------------------------------------------------------------------
// Purpose: Get the minmum width the menu
//-----------------------------------------------------------------------------
int Menu::GetMinimumWidth()
{
	return m_iMinimumWidth;
}

void Menu::PlaceContextMenu( Panel* parent, Menu* menu )
{
	if ( menu == NULL || parent == NULL )
		return;

	menu->SetVisible( false );
	menu->SetParent( parent );
	menu->AddActionSignalTarget( parent );
	int cursorX;
	int cursorY;
	input()->GetCursorPos( cursorX, cursorY );
	menu->SetVisible( true );
	menu->InvalidateLayout( true, false );
	int menuWide;
	int menuTall;
	menu->GetSize( menuWide, menuTall );
	int wide;
	int tall;
	surface()->GetScreenSize( wide, tall );
	int placedY = cursorY;
	if ( tall - menuTall <= cursorY )
		placedY = cursorY - menuTall;
	if ( wide - menuWide > cursorX )
		menu->SetPos( cursorX, placedY );
	else
		menu->SetPos( cursorX - menuWide, placedY );
	menu->RequestFocus( 0 );
}

void Menu::SetUseFallbackFont( bool state, HFont font )
{
	m_hFallbackItemFont = font;
	m_bUseFallbackFont = state;
}

void Menu::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	SetFgColor( GetSchemeColor( "Menu/FgColor", scheme ) );
	SetBgColor( GetSchemeColor( "Menu/BgColor", scheme ) );
	_borderDark = scheme->GetColor( "BorderDark", Color( 255, 255, 255, 0 ) );

	for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex(); itemID = m_MenuItems.Next( itemID ) )
	{
		MenuItem* item = m_MenuItems[itemID];
		if ( !item->IsCheckable() )
			continue;
		int wide;
		int tall;
		item->GetCheckImageSize( wide, tall );
		if ( m_iCheckImageWidth < wide )
			m_iCheckImageWidth = wide;
	}

	_menuWide = 0;
	_recalculateWidth = true;
	int measured = 0;
	if ( m_iFixedWidth == 0 )
	{
		for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex();
			itemID = m_MenuItems.Next( itemID ) )
		{
			int wide;
			int tall;
			m_MenuItems[itemID]->GetContentSize( wide, tall );
			if ( measured - 7 <= wide )
			{
				measured = wide + 8;
				_menuWide = measured;
			}
		}
	}
	if ( m_iMinimumWidth > measured )
		_menuWide = m_iMinimumWidth;
	_recalculateWidth = false;
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Hide the menu when an item has been selected
//-----------------------------------------------------------------------------
void Menu::OnMenuItemSelected( Panel* panel )
{
	SetVisible( false );
	m_pScroller->SetVisible( false );

	Panel* parent = GetParent();
	if ( parent != NULL )
	{
		MenuItem* parentItem = dynamic_cast<MenuItem*>( parent );
		if ( parentItem != NULL )
		{
			Menu* parentMenu = parentItem->GetParentMenu();
			if ( parentMenu != NULL )
			{
				KeyValues* message = new KeyValues( "MenuItemSelected" );
				message->SetPtr( "panel", panel );
				ivgui()->PostMessage( parentMenu->GetVPanel(), message, GetVPanel(), 0.0f );
			}
		}
	}

	int activated = m_MenuItems.InvalidIndex();
	bool found = false;
	for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex(); itemID = m_MenuItems.Next( itemID ) )
	{
		if ( m_MenuItems[itemID] == panel )
		{
			activated = itemID;
			found = true;
			break;
		}
	}
	if ( !found )
	{
		for ( int itemID = m_MenuItems.Head(); itemID != m_MenuItems.InvalidIndex();
			itemID = m_MenuItems.Next( itemID ) )
			m_MenuItems[itemID]->HasMenu();
		if ( GetVParent() == 0 )
			return;
	}
	else
	{
		m_iActivatedItem = activated;
		if ( GetVParent() == 0 )
			return;
	}

	KeyValues* message = new KeyValues( "MenuItemSelected" );
	message->SetPtr( "panel", panel );
	ivgui()->PostMessage( GetVParent(), message, GetVPanel(), 0.0f );
}
}
