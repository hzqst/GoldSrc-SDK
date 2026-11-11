//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Panel.h"
#include "utllinkedlist.h"
#include "utlvector.h"

namespace vgui2
{
class MenuItem;
class ScrollBar;

//-----------------------------------------------------------------------------
// Purpose: A menu is a list of items that can be selected with one click, navigated
//			with arrow keys and/or hot keys, and have a lit behavior when mouse over.
//			It is NOT the button which opens the menu, but only the menu itself.
//
// Behaviour spec:
// Menu navigation can be done in 2 modes, via keyboard keys and via mouse.
// Clicking on menu button opens menu.
// Only one item in a menu is highlighted at a time.
// Only one submenu in a menu is open at a time.
// Disabled menuitems get highlighted via mouse and keys but will not activate.
//
// Mouse:
//   Moving mouse into a menuitem highlights it.
//   If the menuitem has a cascading menu, the menu opens when the mouse enters
//    the menuitem. The cascading menuitem stays highlighted while its menu is open.
//   No submenu items are highlighted by default.
//   Moving the mouse into another menuitem closes any previously open submenus in the list.
//   Clicking once in the menu item activates the menu item and closes all menus.
//   Moving the mouse off a menuitem unhighlights it.
//   The scroll bar arrows can be used to move up/down the menu one item at a time.
//   The clicking and dragging on the scroll bar nob also scrolls the menu items.
//   If a highlighed menuitem scrolls off, and the user then begins navigating via keys,
//    the menu will snap the scroll bar so the highlighted item is visible.
//   If user has been navigating via keys, moving the mouse over a menu item 
//    highlights it.
// Mousewheel:
//   You must have the mouse inside the menu/scroll bar to use the wheel.
//   The mouse wheel moves the highlighted menuitem up or down the list.
//   If the list has no scroll bar the wheel will cycle from the bottom of the list
//    to the top of the list and vice versa.
//   If the list has a scrollbar the mouse wheel will stop at the top or bottom
//    of the list. 
//   If the mouse is over the scroll bar no items are highlighted.
// Keyboard:
//   When a menu is opened, no items are highlighted.
//   If a menuitem has a cascading menu it does not open when the item is highlighted.
//   The down arrow selects the next item in the list. 
//    (first item if none are highlighted and there is a scrollbar).
//   The up arrow selects the previous item in the list 
//    (first item if none are highlighted and there is a scrollbar, last item if none are
//    highlighted and there is no scrollbar).
//   Selecting a new menuitem closes any previously open submenus in the list.
//   The enter key activates the selected item and closes all menus.
//   If the selected item has a cascading menu, activating it opens its submenu.
//   These may also be activated by pressing the right arrow.
//   Pressing the left arrow closes the submenu.
//   When the submenu is opened the cascading menuitem stays highlighted.
//   No items in the submenu are highlighted when it is opened.
//   
//   Note: Cascading menuitems in menus with a scrollbar is not supported.
//         Its a clunky UI and if we want this we should design a better solution,
//         perhaps along the lines of how explorer's bookmarks does it.
//         It currently functions, but there are some arm/disarm bugs.
//
//
//-----------------------------------------------------------------------------
class Menu : public Panel
{
	DECLARE_CLASS_SIMPLE( Menu, Panel );

	friend class MenuItem;

public:
	Menu( Panel* parent, const char* name );
	~Menu() override;

	static void PlaceContextMenu( Panel* parent, Menu* menu );

	enum MenuMode
	{
		MOUSE = 0,
		KEYBOARD = 1
	};

	virtual int AddMenuItem( const char* itemName, const char* itemText, const char* command, Panel* target,
		const KeyValues* userData );
	virtual int AddMenuItem( const char* itemName, const wchar_t* itemText, const char* command, Panel* target,
		const KeyValues* userData );
	virtual int AddMenuItem( const char* itemName, const char* itemText, KeyValues* command, Panel* target,
		const KeyValues* userData );
	virtual int AddMenuItem( const char* itemName, const wchar_t* itemText, KeyValues* command, Panel* target,
		const KeyValues* userData );
	virtual int AddMenuItem( const char* itemText, const char* command, Panel* target, const KeyValues* userData );
	virtual int AddMenuItem( const char* itemText, KeyValues* command, Panel* target, const KeyValues* userData );
	virtual int AddMenuItem( const char* itemText, Panel* target, const KeyValues* userData );

	virtual int AddCheckableMenuItem( const char* itemName, const char* itemText, const char* command, Panel* target,
		const KeyValues* userData );
	virtual int AddCheckableMenuItem( const char* itemName, const wchar_t* itemText, const char* command,
		Panel* target, const KeyValues* userData );
	virtual int AddCheckableMenuItem( const char* itemName, const char* itemText, KeyValues* command, Panel* target,
		const KeyValues* userData );
	virtual int AddCheckableMenuItem( const char* itemName, const wchar_t* itemText, KeyValues* command,
		Panel* target, const KeyValues* userData );
	virtual int AddCheckableMenuItem( const char* itemText, const char* command, Panel* target,
		const KeyValues* userData );
	virtual int AddCheckableMenuItem( const char* itemText, KeyValues* command, Panel* target,
		const KeyValues* userData );
	virtual int AddCheckableMenuItem( const char* itemText, Panel* target, const KeyValues* userData );

	virtual int AddCascadingMenuItem( const char* itemName, const char* itemText, const char* command, Panel* target,
		Menu* cascadingMenu, const KeyValues* userData );
	virtual int AddCascadingMenuItem( const char* itemName, const wchar_t* itemText, const char* command,
		Panel* target, Menu* cascadingMenu, const KeyValues* userData );
	virtual int AddCascadingMenuItem( const char* itemName, const char* itemText, KeyValues* command, Panel* target,
		Menu* cascadingMenu, const KeyValues* userData );
	virtual int AddCascadingMenuItem( const char* itemName, const wchar_t* itemText, KeyValues* command,
		Panel* target, Menu* cascadingMenu, const KeyValues* userData );
	virtual int AddCascadingMenuItem( const char* itemText, const char* command, Panel* target, Menu* cascadingMenu,
		const KeyValues* userData );
	virtual int AddCascadingMenuItem( const char* itemText, KeyValues* command, Panel* target, Menu* cascadingMenu,
		const KeyValues* userData );
	virtual int AddCascadingMenuItem( const char* itemText, Panel* target, Menu* cascadingMenu,
		const KeyValues* userData );

	virtual int AddMenuItem( MenuItem* item );

	virtual void UpdateMenuItem( int itemID, const char* itemText, KeyValues* command, const KeyValues* userData );
	virtual void UpdateMenuItem( int itemID, const wchar_t* itemText, KeyValues* command, const KeyValues* userData );

	virtual bool IsValidMenuID( int itemID );
	virtual int GetInvalidMenuID();
	virtual KeyValues* GetItemUserData( int itemID );

	virtual void SetItemEnabled( const char* itemName, bool state );
	virtual void SetItemEnabled( int itemID, bool state );
	virtual void SetItemVisible( const char* itemName, bool state );
	virtual void SetItemVisible( int itemID, bool state );
	virtual void DeleteAllItems();

	// Override the auto-width setting with a single fixed width
	virtual void SetFixedWidth( int width );
	virtual void SetMenuItemHeight( int height );
	virtual void SetNumberOfVisibleItems( int count );

	void PerformLayout() override;
	void SetBorder( IBorder* border ) override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void OnKeyTyped( wchar_t character ) override;
	void OnKeyCodeTyped( KeyCode code ) override;
	void SetVisible( bool state ) override;

	// Activates item in the menu list, as if that menu item had been selected by the user
	virtual void ActivateItem( int itemID );
	virtual void ActivateItemByRow( int row );
	virtual int GetActiveItem();		// returns the itemID (not the row) of the active item
	virtual int GetItemCount();
	virtual int GetMenuID( int row );

	// Return the number of items currently visible in the menu list
	int GetCurrentlyVisibleItemsCount();
	MenuItem* GetMenuItem( int itemID );
	int GetMenuItemID( MenuItem* item );
	void CloseOtherMenus( MenuItem* item );
	void OnKillFocus() override;
	int GetMenuMode();

	void SetCurrentlyHighlightedItem( int itemID );
	int GetCurrentlyHighlightedItem();
	void ClearCurrentlyHighlightedItem();

	// Set the checked state of a checkable menuItem
	void SetMenuItemChecked( int itemID, bool state );
	bool IsChecked( int itemID ); // check if item is checked.

	void SetMinimumWidth( int width );
	int GetMinimumWidth();

	void SetFgColor( Color color ) override;
	void SetBgColor( Color color ) override;
	virtual void SetFont( HFont font );
	void SetUseFallbackFont( bool state, HFont font );

protected:
	// helper functions	
	int AddMenuItemCharCommand( MenuItem* item, const char* command, Panel* target, const KeyValues* userData );
	int AddMenuItemKeyValuesCommand( MenuItem* item, KeyValues* command, Panel* target, const KeyValues* userData );

	void OnCommand( const char* command ) override;
	MESSAGE_FUNC_PTR( OnMenuItemSelected, "MenuItemSelected", panel );

	virtual void AddScrollBar();
	virtual void RemoveScrollBar();
	MESSAGE_FUNC( OnSliderMoved, "ScrollBarSliderMoved" );

	void Paint() override;
	virtual void LayoutMenuBorder();
	virtual void MakeItemsVisibleInScrollRange();
	void OnMouseWheeled( int delta ) override;

	void CalculateWidth();
	void ForceCalculateWidth();
	void LayoutScrollBar();
	void PositionCascadingMenu();
	void SizeMenuItems();

	void OnCursorMoved( int x, int y ) override;
	void OnKeyCodePressed( KeyCode code ) override;
	void OnMenuClose();
	MESSAGE_FUNC( OnKeyModeSet, "KeyModeSet" );

	void SetCurrentlySelectedItem( MenuItem* item );
	void SetCurrentlySelectedItem( int itemID );
	MESSAGE_FUNC_INT( OnCursorEnteredMenuItem, "CursorEnteredMenuItem", VPanel );
	MESSAGE_FUNC_INT( OnCursorExitedMenuItem, "CursorExitedMenuItem", VPanel );

	void MoveAlongMenuItemList( int direction, int loopCount );

	enum
	{
		SCROLLBAR_SIZE				= 18,
		DEFAULT_MENU_ITEM_HEIGHT	= 22, // height of items in the menu

		UP							= -1,
		DOWN						= 1
	};

private:
	MenuItem* GetParentMenuItem();

	int								m_iMenuItemHeight;
	int								m_iFixedWidth;
	int								m_iMinimumWidth; // a minimum width the menu has to be if it is not fixed width
	int								m_iNumVisibleLines;	// number of items in menu before scroll bar adds on

	ScrollBar*						m_pScroller;
	CUtlLinkedList<MenuItem*, int>	m_MenuItems;
	CUtlVector<int>					m_SortedItems;

	bool							_sizedForScrollBar : 1;  // whether menu has been sized for a scrollbar
	bool							m_bUseFallbackFont : 1;
	bool							_recalculateWidth : 1;

	int								_menuWide;
	int								m_iCurrentlySelectedItemID;
	int								m_iInputMode;
	int								m_iCheckImageWidth; // the size of the check box spot on a checkable menu.
	int								m_iProportionalScrollBarSize;
	Color							_borderDark;
	int								m_iActivatedItem;

	HFont							m_hItemFont;
	HFont							m_hFallbackItemFont;
};
}
