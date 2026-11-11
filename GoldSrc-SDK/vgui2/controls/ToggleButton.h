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
//-----------------------------------------------------------------------------
// Purpose: Type of button that when pressed stays selected & depressed until pressed again
//-----------------------------------------------------------------------------
class ToggleButton : public Button
{
	DECLARE_CLASS_SIMPLE( ToggleButton, Button );

public:
	ToggleButton( Panel* parent, const char* panelName, const char* text );
	~ToggleButton() override;

	void DoClick() override;

	bool CanBeDefaultButton() override;

	Color GetButtonFgColor() override;

protected:
	void OnMouseDoublePressed( MouseCode code ) override;
	void ApplySchemeSettings( IScheme* pScheme ) override;
	void OnKeyCodePressed( KeyCode code ) override;
	void OnKeyCodeTyped( KeyCode code ) override;

private:
	Color _selectedColor;
};
} // namespace vgui
