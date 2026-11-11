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

#include "EditablePanel.h"
#include "PHandle.h"

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Property page, as held by a set of property sheets
//-----------------------------------------------------------------------------
class PropertyPage : public EditablePanel
{
	DECLARE_CLASS_SIMPLE( PropertyPage, EditablePanel );

public:
	PropertyPage( Panel* parent, const char* panelName, bool paintBorder );
	~PropertyPage() override;

	// Called when page is loaded.  Data should be reloaded from document into controls.
	MESSAGE_FUNC( OnResetData, "ResetData" );
	// Called when the OK / Apply button is pressed.  Changed data should be written into document.
	MESSAGE_FUNC( OnApplyChanges, "ApplyChanges" );
	// called when the page is shown/hidden
	MESSAGE_FUNC( OnPageShow, "PageShow" );
	MESSAGE_FUNC( OnPageHide, "PageHide" );

	virtual void SetPaintBorder( bool state );
	void OnKeyCodeTyped( KeyCode code ) override;

protected:
	void ApplySchemeSettings( IScheme* scheme ) override;
	void PaintBorder() override;
	void SetVisible( bool state ) override;
	// called to be notified of the tab button used to Activate this page
	// if overridden this must be chained back to
	MESSAGE_FUNC_PTR( OnPageTabActivated, "PageTabActivated", panel );

private:
	PHandle	_pageTab;
	bool	_paintRaised;
};
} // namespace vgui
