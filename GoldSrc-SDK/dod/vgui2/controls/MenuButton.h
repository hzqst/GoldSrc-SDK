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
class IScheme;

//-----------------------------------------------------------------------------
// Purpose: Button that displays a menu when pressed
//-----------------------------------------------------------------------------
class MenuButton : public Button
{
	DECLARE_CLASS_SIMPLE( MenuButton, Button );

public:
	MenuButton( Panel* parent, const char* name, const char* text );
	~MenuButton() override;

	// functions designed to be overriden
	virtual void OnShowMenu( Menu* menu );
	virtual void OnHideMenu( Menu* menu );

	virtual void SetMenu( Menu* menu );
	virtual void HideMenu();
	void OnKillFocus() override;
	void DrawFocusBorder( int x0, int y0, int x1, int y1 ) override;
	MESSAGE_FUNC( OnMenuClose, "MenuClose" );

	void DoClick() override;
	virtual void SetOpenOffsetY( int offset );
	bool CanBeDefaultButton() override;

	// sets the direction in which the menu opens from the button, defaults to down
	enum MenuDirection_e : unsigned int
	{
		LEFT = 0,
		RIGHT,
		UP,
		DOWN,
		CURSOR,	// make the menu appear under the mouse cursor
		ALIGN_WITH_PARENT
	};

	// sets the direction in which the menu opens from the button, defaults to down
	virtual void SetOpenDirection( MenuDirection_e direction );

	void OnKeyCodeTyped( KeyCode code ) override;
	void OnCursorEntered() override;

private:
	Menu* m_pMenu;
	MenuDirection_e m_iDirection;
	int _openOffsetY;
};
}
