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

#include "Label.h"

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class Button : public Label
{
	DECLARE_CLASS_SIMPLE( Button, Label );

public:
	Button( Panel* parent, const char* name, const char* text );
	Button( Panel* parent, const char* name, const wchar_t* text );
	~Button() override;

	enum ActivationType_t
	{
		ACTIVATE_ONPRESSEDANDRELEASED = 0, // normal button behaviour
		ACTIVATE_ONPRESSED = 1,			   // menu buttons, toggle buttons
		ACTIVATE_ONRELEASED = 2			   // menu items
	};

	// Set armed state.
	virtual void SetArmed( bool state );
	// Check armed state
	virtual bool IsArmed();

	// Check depressed state
	virtual bool IsDepressed();
	// Set button force depressed state.
	virtual void ForceDepressed( bool state );
	// Set button depressed state with respect to the force depressed state.
	virtual void RecalculateDepressedState();

	// Set button selected state.
	virtual void SetSelected( bool state );
	// Check selected state
	virtual bool IsSelected();

	//Set whether or not the button captures all mouse input when depressed.
	virtual void SetUseCaptureMouse( bool state );
	virtual bool IsUseCaptureMouseEnabled();

	// Activate a button click.
	MESSAGE_FUNC( DoClick, "PressButton" );
	MESSAGE_FUNC( OnHotkey, "Hotkey" );

	// Set button to be mouse clickable or not.
	virtual void SetMouseClickEnabled( MouseCode code, bool state );
	// Check if button is mouse clickable
	virtual bool IsMouseClickEnabled( MouseCode code );
	// sets the how this button activates
	virtual void SetButtonActivationType( ActivationType_t type );

	// Message targets that the button has been pressed
	virtual void FireActionSignal();
	// Perform graphical layout of button
	void PerformLayout() override;

	bool RequestInfo( KeyValues* output ) override;

	virtual bool CanBeDefaultButton();

	// Set this button to be the button that is accessed by default when the user hits ENTER or SPACE
	MESSAGE_FUNC_INT( SetAsDefaultButton, "SetAsDefaultButton", state );
	// Set this button to be the button that is currently accessed by default when the user hits ENTER or SPACE
	MESSAGE_FUNC_INT( SetAsCurrentDefaultButton, "SetAsCurrentDefaultButton", state );

	// Respond when key focus is received
	void OnSetFocus() override;
	void OnKillFocus() override;

	// Set button border attribute enabled, controls display of button.
	virtual void SetButtonBorderEnabled( bool state );

	// Set default button colors.
	virtual void SetDefaultColor( Color foreground, Color background );
	// Set armed button colors
	virtual void SetArmedColor( Color foreground, Color background );
	// Set depressed button colors
	virtual void SetDepressedColor( Color foreground, Color background );

	// Get button foreground color
	virtual Color GetButtonFgColor();
	// Get button background color
	virtual Color GetButtonBgColor();

	// Set default button border attributes.
	virtual void SetDefaultBorder( IBorder* border );
	// Set depressed button border attributes.
	virtual void SetDepressedBorder( IBorder* border );
	// Set key focused button border attributes.
	virtual void SetKeyFocusBorder( IBorder* border );

	// Set the command to send when the button is pressed
	// Set the panel to send the command to with setActionSignalTarget()
	virtual void SetCommand( const char* command );
	// Set the message to send when the button is pressed
	virtual void SetCommand( KeyValues* command );

	// sound handling
	void SetArmedSound( const char* name );
	void SetDepressedSound( const char* name );
	void SetReleasedSound( const char* name );

	void OnCursorEntered() override;
	void OnCursorExited() override;

	void SizeToContents() override;

protected:
	virtual void DrawFocusBorder( int x0, int y0, int x1, int y1 );

	// Paint button on screen
	void Paint() override;
	// Get button border attributes.
	virtual IBorder* GetBorder( bool depressed, bool armed, bool selected, bool keyFocus );

	void ApplySchemeSettings( IScheme* scheme ) override;
	MESSAGE_FUNC_INT( OnSetState, "SetState", state );

	void OnMousePressed( MouseCode code ) override;
	void OnMouseDoublePressed( MouseCode code ) override;
	void OnMouseReleased( MouseCode code ) override;
	void OnKeyCodePressed( KeyCode code ) override;
	void OnKeyCodeReleased( KeyCode code ) override;

	// Get control settings for editing
	void GetSettings( KeyValues* settings ) override;
	void ApplySettings( KeyValues* settings ) override;
	const char* GetDescription() override;

private:
	void Init();

	bool _armed;		// whether the mouse is over the button
	bool _depressed;	// whether the button is visual depressed
	bool _selected;	// whether the mouse is being held down on this button
	bool _forceDepressed;
	bool _buttonBorderEnabled;
	bool _useCaptureMouse;
	bool _keyDown;
	int _mouseClickMask;
	KeyValues* _actionMessage;
	ActivationType_t _activationType;
	IBorder* _defaultBorder;
	IBorder* _depressedBorder;
	IBorder* _keyFocusBorder;
	Color _defaultFgColor;
	Color _defaultBgColor;
	Color _armedFgColor;
	Color _armedBgColor;
	Color _depressedFgColor;
	Color _depressedBgColor;
	Color _keyboardFocusColor;
	unsigned short m_sArmedSoundName;
	unsigned short m_sDepressedSoundName;
	unsigned short m_sReleasedSoundName;
	bool _defaultButton;
};
}
