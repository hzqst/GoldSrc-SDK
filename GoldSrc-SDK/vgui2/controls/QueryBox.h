//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Creates a Message box with a question in it and yes/no buttons
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "MessageBox.h"

class KeyValues;

namespace vgui2
{
class Button;

//-----------------------------------------------------------------------------
// Purpose: Creates A Message box with a question in it and yes/no buttons
//-----------------------------------------------------------------------------
class QueryBox : public MessageBox
{
	DECLARE_CLASS_SIMPLE( QueryBox, MessageBox );

public:
	QueryBox( const char* title, const char* queryText, Panel* parent );
	QueryBox( const wchar_t* wszTitle, const wchar_t* wszQueryText, Panel* parent );
	~QueryBox() override;

	// Set the keyvalues to send when ok button is hit
	void SetOKCommand( KeyValues* keyValues );
	void SetOKCommandValue( const char* keyName, int value );

	// Set the keyvalues to send when the cancel button is hit
	void SetCancelCommand( KeyValues* keyValues );

	// Set the text on the Cancel button
	void SetCancelButtonText( const char* buttonText );
	void SetCancelButtonText( const wchar_t* wszButtonText );

	void SetKeyFocusOnCancelButton();

	void OnCommand( const char* command ) override;

protected:
	void PerformLayout() override;
	void OnKeyCodeTyped( KeyCode code ) override;

protected:
	Button* m_pCancelButton;

private:
	KeyValues* m_pCancelCommand;
	KeyValues* m_pOkCommand;
};
}
