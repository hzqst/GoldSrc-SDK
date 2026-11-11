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

#include "Frame.h"

namespace vgui2
{
class Button;
class PropertySheet;

//-----------------------------------------------------------------------------
// Purpose: Simple frame that holds a property sheet
//-----------------------------------------------------------------------------
class PropertyDialog : public Frame
{
	DECLARE_CLASS_SIMPLE( PropertyDialog, Frame );

public:
	PropertyDialog( Panel* parent, const char* panelName );
	~PropertyDialog() override;

	// returns a pointer to the PropertySheet this dialog encapsulates 
	virtual PropertySheet* GetPropertySheet();
	// wrapper for PropertySheet interface
	virtual void AddPage( Panel* page, const char* title );
	virtual Panel* GetActivePage();
	virtual void ApplyChanges();
	virtual void ResetAllData();

	void RequestFocus( int direction ) override;
	void ActivateBuildMode() override;
	void OnCommand( const char* command ) override;

	void SetOKButtonText( const char* text );
	void SetCancelButtonText( const char* text );
	void SetApplyButtonText( const char* text );
	// changes the visibility of the buttons
	void SetOKButtonVisible( bool state );
	void SetCancelButtonVisible( bool state );
	void SetApplyButtonVisible( bool state );
	void EnableApplyButton( bool state );
	bool IsApplyButtonEnabled();

protected:
	void PerformLayout() override;
	void OnKeyCodeTyped( KeyCode code ) override;
	virtual void OnOK();
	MESSAGE_FUNC( OnApplyButtonEnable, "ApplyButtonEnable" );

private:
	PropertySheet*	_propertySheet;
	Button*			_okButton;
	Button*			_cancelButton;
	Button*			_applyButton;
};
}
