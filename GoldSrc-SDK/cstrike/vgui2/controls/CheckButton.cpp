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

#include "CheckButton.h"
#include "controls.h"
#include "vgui/IScheme.h"
#include "vgui/KeyValues.h"

namespace vgui2
{
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CheckButton::CheckButton( Panel* parent, const char* panelName, const char* text )
	: ToggleButton( parent, panelName, text )
{
	SetContentAlignment( a_west );
	m_bCheckButtonCheckable = true;
	m_pSelected = scheme()->GetImage( "resource/icon_checked", true );
	m_pDefault = scheme()->GetImage( "resource/icon_emptybox", false );
	_selectedFgColor = Color( 196, 181, 80, 255 );
}

CheckButton::~CheckButton()
{
}

IBorder* CheckButton::GetBorder( bool, bool, bool, bool )
{
	return NULL;
}

void CheckButton::SetCheckButtonCheckable( bool state )
{
	m_bCheckButtonCheckable = state;
	Repaint();
}

void CheckButton::OnCheckButtonChecked( Panel* )
{
}

void CheckButton::ApplySchemeSettings( IScheme* pScheme )
{
	ToggleButton::ApplySchemeSettings( pScheme );

	SetDefaultColor( GetSchemeColor( "FgColor", pScheme ), GetBgColor() );
	_selectedFgColor = GetSchemeColor( "BrightControlText", GetSchemeColor( "ControlText", pScheme ), pScheme );
	SetPaintBackgroundEnabled( false );

	SetTextImageIndex( 1 );
	if ( IsSelected() && m_pSelected )
		SetImageAtIndex( 0, m_pSelected, 0 );
	else if ( m_pDefault )
		SetImageAtIndex( 0, m_pDefault, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Check the button
//-----------------------------------------------------------------------------
void CheckButton::SetSelected( bool state )
{
	if ( !m_bCheckButtonCheckable )
		return;

	// send a message saying we've been checked
	KeyValues* msg = new KeyValues( "CheckButtonChecked", "state", (int)state );
	PostActionSignal( msg );

	Button::SetSelected( state );

	if ( m_pDefault && !state )
		SetImageAtIndex( 0, m_pDefault, 0 );
	if ( m_pSelected && state )
		SetImageAtIndex( 0, m_pSelected, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Gets a different foreground text color if we are selected
//-----------------------------------------------------------------------------
Color CheckButton::GetButtonFgColor()
{
	if ( IsSelected() )
		return _selectedFgColor;
	return ToggleButton::GetButtonFgColor();
}
}
