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

namespace vgui2
{
class Menu;
class Image;
class TextImage;

//-----------------------------------------------------------------------------
// Purpose: The items in a menu
//			MenuItems MUST have the Menu class as parents.
//-----------------------------------------------------------------------------
class MenuItem : public Button
{
	DECLARE_CLASS_SIMPLE( MenuItem, Button );

public:
	MenuItem( Menu* parent, const char* name, const char* text, Menu* cascadeMenu, bool checkable );
	MenuItem( Menu* parent, const char* name, const wchar_t* text, Menu* cascadeMenu, bool checkable );
	~MenuItem() override;

	// Activate the menu item as if it had been selected by the user
	void FireActionSignal() override;
	bool CanBeDefaultButton() override;

	// Handle mouse cursor entering a MenuItem.
	void OnCursorEntered() override;
	// Handle mouse cursor exiting a MenuItem.
	void OnCursorExited() override;

	// Close the cascading menu if we have one.
	void CloseCascadeMenu();

	// Pass kill focus events up to parent on loss of focus
	MESSAGE_FUNC( OnKillFocus, "MenuClose" );

	// Return true if this item triggers a cascading menu
	bool HasMenu();

	// Set the size of the text portion of the label.
	void SetTextImageSize( int width, int height );
	//Return the size of the text portion of the label.
	void GetTextImageSize( int& width, int& height );
	// Return the size of the arrow portion of the label.
	void GetArrowImageSize( int& width, int& height );
	// Return the size of the check portion of the label.
	void GetCheckImageSize( int& width, int& height );

	// Return the menu that this menuItem contains
	Menu* GetMenu();

	void PerformLayout() override;

	// Respond to cursor movement
	void OnCursorMoved( int x, int y ) override;

	// Highlight item
	MESSAGE_FUNC( ArmItem, "ArmItem" );
	// Unhighlight item.
	MESSAGE_FUNC( DisarmItem, "DisarmItem" );

	// is the item highlighted?
	bool IsItemArmed();

	// Open cascading menu if there is one.
	void OpenCascadeMenu();

	bool IsCheckable();
	bool IsChecked();

	// Set a checkable menuItem checked or unchecked.
	void SetChecked( bool state );

	KeyValues* GetUserData();
	void SetUserData( const KeyValues* data );

	int GetActiveItem();

	Menu* GetParentMenu();

protected:
	void OnKeyCodeReleased( KeyCode code ) override;
	void OnMenuClose();
	MESSAGE_FUNC( OnKeyModeSet, "KeyModeSet" );

	// vgui overrides
	virtual void Init();
	void ApplySchemeSettings( IScheme* scheme ) override;
	IBorder* GetBorder( bool depressed, bool armed, bool selected, bool keyFocus ) override;

private:
	enum
	{
		CHECK_INSET = 6
	};
	Menu* m_pCascadeMenu;  // menu triggered to open upon selecting this menu item
	bool m_bCheckable;     // can this menu item have a little check to the left of it when you select it?
	bool m_bChecked;       // whether item is checked or not.
	TextImage* m_pCascadeArrow; // little arrow that appears to the right of menuitems that open a menu
	Image* m_pCheck;  // the check that appears to the left of checked menu items
	TextImage* m_pBlankCheck;  // a blank image same size as the check for when items are not checked.
	KeyValues* m_pUserData;
};
}
