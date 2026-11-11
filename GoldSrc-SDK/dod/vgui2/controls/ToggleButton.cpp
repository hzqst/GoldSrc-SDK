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

#include "ToggleButton.h"

#include "vgui/KeyValues.h"

namespace vgui2
{
ToggleButton::ToggleButton( Panel* parent, const char* panelName, const char* text )
	: Button( parent, panelName, text ), _selectedColor()
{
	SetButtonActivationType( ACTIVATE_ONPRESSED );
}

ToggleButton::~ToggleButton()
{
}

//-----------------------------------------------------------------------------
// Purpose: Toggles the state of the button
//-----------------------------------------------------------------------------
void ToggleButton::DoClick()
{
	if ( IsSelected() )
		ForceDepressed( false );
	else if ( !IsSelected() )
		ForceDepressed( true );

	SetSelected( !IsSelected() );
	FireActionSignal();

	// post a button toggled message
	KeyValues* msg = new KeyValues( "ButtonToggled" );
	msg->SetInt( "state", (int)IsSelected() );
	PostActionSignal( msg );
	Repaint();
}

bool ToggleButton::CanBeDefaultButton()
{
	return false;
}

Color ToggleButton::GetButtonFgColor()
{
	if ( IsSelected() )
		return _selectedColor;
	return Button::GetButtonFgColor();
}

//-----------------------------------------------------------------------------
// Purpose: Turns double-click into normal click
//-----------------------------------------------------------------------------
void ToggleButton::OnMouseDoublePressed( MouseCode code )
{
	OnMousePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ToggleButton::ApplySchemeSettings( IScheme* pScheme )
{
	Button::ApplySchemeSettings( pScheme );
	_selectedColor = GetSchemeColor( "BrightControlText", pScheme );
}

void ToggleButton::OnKeyCodePressed( KeyCode code )
{
	if ( code != KEY_ENTER )
		Button::OnKeyCodePressed( code );
}
}
