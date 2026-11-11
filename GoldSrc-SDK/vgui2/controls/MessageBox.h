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

class KeyValues;

namespace vgui2
{
class Button;
class Label;
class IScheme;

//-----------------------------------------------------------------------------
// Purpose: Popup discardable message box
//-----------------------------------------------------------------------------
class MessageBox : public Frame
{
	DECLARE_CLASS_SIMPLE( MessageBox, Frame );

public:
	MessageBox( const char* title, const char* message, Panel* parent );
	MessageBox( const wchar_t* title, const wchar_t* message, Panel* parent );
	~MessageBox() override;

	// Put the message box into a modal state
	virtual void DoModal( Frame* frameOver );

	// make the message box appear and in a modeless state
	virtual void ShowWindow( Frame* frameOver );

	// Set a string command to be sent when the OK button is pressed
	virtual void SetCommand( const char* command );

	virtual void SetCommand( KeyValues* command );

	// Set the visibility of the OK button.
	virtual void SetOKButtonVisible( bool state );

	// Set the text on the OK button
	virtual void SetOKButtonText( const char* text );
	virtual void SetOKButtonText( const wchar_t* text );

	// Toggles visibility of the close box.
	virtual void DisableCloseButton( bool state );

protected:
	void PerformLayout() override;
	void OnClose() override;
	void ApplySchemeSettings( IScheme* scheme ) override;

	Button* m_pOkButton;
	Label* m_pMessageLabel;

private:
	MESSAGE_FUNC( OnShutdownRequest, "ShutdownRequest" );

	void Init();

	KeyValues* m_OkCommand;
	Frame* m_pFrameOver;
};
} // namespace vgui
