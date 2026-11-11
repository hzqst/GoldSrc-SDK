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

#include "Button.h"
#include "TextEntry.h"

namespace vgui2
{
class Menu;
class IImage;
class ComboBox;

//-----------------------------------------------------------------------------
// Purpose: Scroll bar button
//-----------------------------------------------------------------------------
class ComboBoxButton : public Button
{
public:
	ComboBoxButton( ComboBox* parent, const char* panelName, const char* text );
	~ComboBoxButton() override;

	void ApplySchemeSettings( IScheme* pScheme ) override;
	void OnCursorExited() override;
	Color GetButtonBgColor() override;

protected:
	IBorder* GetBorder( bool depressed, bool armed, bool selected, bool keyFocus ) override;

private:
	Color m_DisabledBgColor;
	IImage* m_pImage;
};

//-----------------------------------------------------------------------------
// Purpose: Text entry with drop down options list
//-----------------------------------------------------------------------------
class ComboBox : public TextEntry
{
	DECLARE_CLASS_SIMPLE( ComboBox, TextEntry );

public:
	ComboBox( Panel* parent, const char* panelName, int numLines, bool allowEdit );
	~ComboBox() override;

	void ApplySchemeSettings( IScheme* pScheme ) override;
	void PerformLayout() override;
	void OnSetFocus() override;
	void OnKillFocus() override;
	void OnCursorEntered() override;
	void OnCursorExited() override;
	void OnMousePressed( MouseCode code ) override;
	void OnMouseDoublePressed( MouseCode code ) override;
	void OnKeyCodeTyped( KeyCode code ) override;
	void OnKeyTyped( wchar_t unichar ) override;
	void OnSizeChanged( int wide, int tall ) override;
	void OnCommand( const char* command ) override;

	// functions designed to be overriden
	virtual void OnShowMenu( Menu* )
	{
	}
	virtual void OnHideMenu( Menu* )
	{
	}

	// Set the number of items in the drop down menu.
	virtual void SetNumberOfEditLines( int numLines );

	//  Add an item to the drop down
	virtual int AddItem( const char* itemText, const KeyValues* userData );
	virtual int AddItem( const wchar_t* itemText, const KeyValues* userData );

	virtual int GetItemCount();

	// update the item
	virtual bool UpdateItem( int itemID, const char* itemText, const KeyValues* userData );
	virtual bool UpdateItem( int itemID, const wchar_t* itemText, const KeyValues* userData );
	virtual bool IsItemIDValid( int itemID );

	// set the enabled state of an item
	virtual void SetItemEnabled( const char* itemText, bool state );
	virtual void SetItemEnabled( int itemID, bool state );

	// Remove all items from the drop down menu
	virtual void DeleteAllItems();

	// Sorts the items in the list - FIXME does nothing
	virtual void SortItems();

	// Set the visiblity of the drop down menu button.
	virtual void SetDropdownButtonVisible( bool state );

	// Return true if the combobox current has the dropdown menu open
	virtual bool IsDropdownVisible();

	MESSAGE_FUNC_INT( ActivateItem, "ActivateItem", itemID );
	virtual void ActivateItemByRow( int row );

	virtual int GetActiveItem();
	virtual KeyValues* GetActiveItemUserData();
	virtual KeyValues* GetItemUserData( int itemID );

	// sets a custom menu to use for the dropdown
	virtual void SetMenu( Menu* menu );
	virtual Menu* GetMenu()
	{
		return m_pDropDown;
	}

	virtual void ShowMenu();

	virtual void HideMenu();

	MESSAGE_FUNC( OnMenuClose, "MenuClose" );

	virtual void DoClick();

	enum MenuDirection_e : unsigned int
	{
		LEFT = 0,
		RIGHT,
		UP,
		DOWN,
		CURSOR,	// make the menu appear under the mouse cursor
		ALIGN_WITH_PARENT
	};
	virtual void SetOpenDirection( MenuDirection_e direction );

	virtual void SetUseFallbackFont( bool bState, HFont hFallback );

	MESSAGE_FUNC( OnMenuItemSelected, "MenuItemSelected" );

	MESSAGE_FUNC_WCHARPTR( OnSetText, "SetText", text );

private:
	void DoMenuLayout();
	void MoveAlongMenuItemList( int direction );

	Menu* m_pDropDown;
	ComboBoxButton* m_pButton;
	bool m_bAllowEdit;
	bool m_bHighlight;
	MenuDirection_e m_iDirection;
	int m_iOpenOffsetY;
};
}
