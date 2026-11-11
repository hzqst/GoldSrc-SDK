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

#include "ToggleButton.h"

class CheckImage;

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Tick-box button
//-----------------------------------------------------------------------------
class CheckButton : public ToggleButton
{
	DECLARE_CLASS_SIMPLE( CheckButton, ToggleButton );

public:
	CheckButton( Panel* parent, const char* panelName, const char* text );
	~CheckButton() override;

	// Check the button
	void SetSelected( bool state ) override;

	// sets whether or not the state of the check can be changed
	// if this is set to false, then no input in the code or by the user can change it's state
	virtual void SetCheckButtonCheckable( bool state );

	Color GetButtonFgColor() override;

protected:
	void ApplySchemeSettings( IScheme* pScheme ) override;
	MESSAGE_FUNC_PTR( OnCheckButtonChecked, "CheckButtonChecked", panel );
	IBorder* GetBorder( bool depressed, bool armed, bool selected, bool keyfocus ) override;

private:
	CheckImage* _checkBoxImage;
	bool m_bCheckButtonCheckable;
	Color _selectedFgColor;
	IImage* m_pDefault;
	IImage* m_pSelected;
};
}
