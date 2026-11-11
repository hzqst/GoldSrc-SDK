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

#pragma once

#include "Panel.h"
#include "utllinkedlist.h"
#include "utlvector.h"

namespace vgui2
{
class MenuItem;
class ScrollBar;

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


	// Add a simple text item to the menu
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

	// Add a checkable item to the menu
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

	// Add a cascading menu item to the menu
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

	// Sets the values of a menu item at the specified index
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

	virtual void SetFixedWidth( int width );
	// sets the height of each menu item
	virtual void SetMenuItemHeight( int height );
	virtual void SetMenuItemBlurOffset( int offset );
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
	// Return the number of items currently in the menu list
	virtual int GetItemCount();
	// return the menuID of the n'th item in the menu list, valid from [0, GetItemCount)
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
		SCROLLBAR_SIZE				= 18, // the width of a scrollbar
		DEFAULT_MENU_ITEM_HEIGHT	= 22, // height of items in the menu

		UP							= -1, // used for moving up/down list of menu items in the menu
		DOWN						= 1
	};

private:
	MenuItem* GetParentMenuItem();

	int								m_iMenuItemHeight;
	int								m_iFixedWidth;
	int								m_iMinimumWidth; // a minimum width the menu has to be if it is not fixed width
	int								m_iNumVisibleLines; // number of items in menu before scroll bar adds on
	int								m_iMenuItemBlurOffset;

	ScrollBar*						m_pScroller;
	CUtlLinkedList<MenuItem*, int>	m_MenuItems;
	CUtlVector<int>					m_SortedItems;

	bool							_sizedForScrollBar : 1; // whether menu has been sized for a scrollbar
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
