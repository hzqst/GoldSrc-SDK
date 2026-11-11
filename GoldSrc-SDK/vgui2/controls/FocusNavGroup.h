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

#include "vgui/ISurface.h"
#include "PHandle.h"

namespace vgui2
{
class Panel;

//-----------------------------------------------------------------------------
// Purpose: Handles navigation through a set of panels, with tab order & hotkeys
//-----------------------------------------------------------------------------
class FocusNavGroup
{
public:
	FocusNavGroup( Panel* mainPanel );
	~FocusNavGroup();

	virtual Panel* GetDefaultPanel(); // returns a pointer to the panel with the default focus

	virtual void SetDefaultButton( Panel* button ); // sets which panel should receive input when ENTER is hit
	virtual VPANEL GetDefaultButton();				    // panel which receives default input when ENTER is hit, if current focused item cannot accept ENTER
	virtual VPANEL GetCurrentDefaultButton();			// panel which receives input when ENTER is hit

	virtual Panel* FindPanelByHotkey( wchar_t key ); // finds the panel which is activated by the specified key

	virtual bool RequestFocusPrev( VPANEL panel ); // if panel is NULL, then the tab increment is based last known panel that had key focus
	virtual bool RequestFocusNext( VPANEL panel );
	virtual Panel* GetCurrentFocus();
	virtual VPANEL SetCurrentFocus( VPANEL panel, VPANEL defaultPanel ); // returns the Default panel
	// sets the panel that owns this FocusNavGroup to be the root in the focus traversal heirarchy
	// focus change via KEY_TAB will only travel to children of this main panel
	virtual void SetFocusTopLevel( bool state );

	virtual void SetCurrentDefaultButton( VPANEL panel, bool sendMessage );
	bool CanButtonBeDefault( VPANEL panel );

protected:
	VPanelHandle	_defaultButton;
	VPanelHandle	_currentDefaultButton;
	VPanelHandle	_currentFocus;
	Panel*			_mainPanel;
	bool			_topLevelFocus;
};
} // namespace vgui
