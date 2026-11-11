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

#include "PropertyPage.h"

#include "vgui/IBorder.h"
#include "vgui/IScheme.h"

namespace vgui2
{
PropertyPage::PropertyPage( Panel* parent, const char* panelName, bool paintBorder )
	: EditablePanel( parent, panelName ), _paintRaised( paintBorder )
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
PropertyPage::~PropertyPage()
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when page is loaded.  Data should be reloaded from document into controls.
//-----------------------------------------------------------------------------
void PropertyPage::OnResetData()
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when the OK / Apply button is pressed.  Changed data should be written into document.
//-----------------------------------------------------------------------------
void PropertyPage::OnApplyChanges()
{
}

//-----------------------------------------------------------------------------
// Purpose: Designed to be overriden
//-----------------------------------------------------------------------------
void PropertyPage::OnPageShow()
{
}

//-----------------------------------------------------------------------------
// Purpose: Designed to be overriden
//-----------------------------------------------------------------------------
void PropertyPage::OnPageHide()
{
}

void PropertyPage::OnPageTabActivated( Panel* panel )
{
	_pageTab.Set( panel );
}

void PropertyPage::SetPaintBorder( bool state )
{
	_paintRaised = state;
}

//-----------------------------------------------------------------------------
// Purpose: Paints the border, but with the required breaks for the page tab
//-----------------------------------------------------------------------------
void PropertyPage::PaintBorder()
{
	IBorder* border = GetBorder();
	if ( !_paintRaised || !border || !_pageTab.Get() )
	{
		Panel::PaintBorder();
		return;
	}

	int x, y, tabWide, tabTall;
	_pageTab.Get()->GetBounds( x, y, tabWide, tabTall );
	int wide, tall;
	GetSize( wide, tall );
	border->Paint( 0, 0, wide, tall, 1, x + 1, x + tabWide - 1 );
}

void PropertyPage::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_LEFT || code == KEY_RIGHT )
	{
		if ( !_pageTab.Get() || !_pageTab.Get()->HasFocus() )
			return;
	}

	EditablePanel::OnKeyCodeTyped( code );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *inResourceData - 
//-----------------------------------------------------------------------------
void PropertyPage::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	if ( _paintRaised )
		SetBorder( scheme->GetBorder( "ButtonBorder" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertyPage::SetVisible( bool state )
{
	if ( IsVisible() && !state )
	{
		if ( GetFocusNavGroup().GetCurrentDefaultButton() )
			GetFocusNavGroup().SetCurrentDefaultButton( 0, true );
	}

	Panel::SetVisible( state );
}
}
