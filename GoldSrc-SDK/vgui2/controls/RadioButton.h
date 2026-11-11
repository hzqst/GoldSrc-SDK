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

class RadioImage;

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Radio buttons are automatically selected into groups by who their
//			parent is. At most one radio button is active at any time.
//-----------------------------------------------------------------------------
class RadioButton : public ToggleButton
{
	DECLARE_CLASS_SIMPLE( RadioButton, ToggleButton );

public:
	RadioButton( Panel* parent, const char* name, const char* text );
	~RadioButton() override;

	// Set the radio button checked. When a radio button is checked, a
	// message is sent to all other radio buttons in the same group so
	// they will become unchecked.
	void SetSelected( bool state ) override;

	void DoClick() override;
	void Paint() override;
	void PerformLayout() override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void ApplySettings( KeyValues* settings ) override;
	void GetSettings( KeyValues* settings ) override;
	const char* GetDescription() override;
	void OnKeyCodeTyped( KeyCode code ) override;
	IBorder* GetBorder( bool depressed, bool armed, bool selected, bool keyFocus ) override;

	// Get the tab position of the radio button with the set of radio buttons
	// A group of RadioButtons must have the same TabPosition, with [1, n] subtabpositions
	virtual int GetSubTabPosition();
	virtual void SetSubTabPosition( int position );

	// Return the RadioButton's real tab position (its Panel one changes)
	virtual int GetRadioTabPosition();

	MESSAGE_FUNC_INT( OnRadioButtonChecked, "RadioButtonChecked", tabposition );

private:
	RadioButton* FindBestRadioButton( int direction );

	RadioImage* _radioBoxImage;
	int _oldTabPosition;
	Color _selectedFgColor;
	int _subTabPosition;
};
}
