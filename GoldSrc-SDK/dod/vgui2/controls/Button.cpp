//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Basic button control
//
// $NoKeywords: $
//=============================================================================

#include "Button.h"
#include "controls.h"

#include "utlsymbol.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

#include <cstdio>
#include <cstring>

CUtlSymbolTable g_ButtonSoundNames( 0, 32, false );

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose:	Set armed state.
//-----------------------------------------------------------------------------
void Button::SetArmed( bool state )
{
	if ( _armed == state )
		return;

	_armed = state;
	RecalculateDepressedState();
	InvalidateLayout( false, false );

	// play any sounds specified
	if ( _armed && m_sArmedSoundName != UTL_INVAL_SYMBOL )
		surface()->PlaySound( g_ButtonSoundNames.String( CUtlSymbol( m_sArmedSoundName ) ) );
}

//-----------------------------------------------------------------------------
// Purpose:	Check armed state
//-----------------------------------------------------------------------------
bool Button::IsArmed()
{
	return _armed;
}

//-----------------------------------------------------------------------------
// Purpose:	Check depressed state
//-----------------------------------------------------------------------------
bool Button::IsDepressed()
{
	return _depressed;
}

//-----------------------------------------------------------------------------
// Purpose:	Set button force depressed state.
//-----------------------------------------------------------------------------
void Button::ForceDepressed( bool state )
{
	if ( _forceDepressed == state )
		return;

	_forceDepressed = state;
	RecalculateDepressedState();
	InvalidateLayout( false, false );
}

void Button::RecalculateDepressedState()
{
	_depressed = IsEnabled() && ( _forceDepressed || ( _armed && _selected ) );
}

//-----------------------------------------------------------------------------
// Purpose:	Set button selected state.
//-----------------------------------------------------------------------------
void Button::SetSelected( bool state )
{
	if ( _selected == state )
		return;

	_selected = state;
	RecalculateDepressedState();
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Check selected state
//-----------------------------------------------------------------------------
bool Button::IsSelected()
{
	return _selected;
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether or not the button captures all mouse input when depressed
//			Defaults to true
//			Should be set to false for things like menu items where there is a higher-level mouse capture
//-----------------------------------------------------------------------------
void Button::SetUseCaptureMouse( bool state )
{
	_useCaptureMouse = state;
}

void Button::SetButtonBorderEnabled( bool state )
{
	if ( _buttonBorderEnabled == state )
		return;

	_buttonBorderEnabled = state;
	InvalidateLayout( false, false );
}

void Button::SetDefaultColor( Color foreground, Color background )
{
	if ( _defaultFgColor != foreground || _defaultBgColor != background )
	{
		_defaultBgColor = background;
		_defaultFgColor = foreground;
		InvalidateLayout( false, false );
	}
}

void Button::SetArmedColor( Color foreground, Color background )
{
	if ( _armedFgColor != foreground || _armedBgColor != background )
	{
		_armedBgColor = background;
		_armedFgColor = foreground;
		InvalidateLayout( false, false );
	}
}

void Button::SetDepressedColor( Color foreground, Color background )
{
	if ( _depressedFgColor != foreground || _depressedBgColor != background )
	{
		_depressedBgColor = background;
		_depressedFgColor = foreground;
		InvalidateLayout( false, false );
	}
}

Color Button::GetButtonFgColor()
{
	if ( _depressed )
		return _depressedFgColor;
	if ( _armed )
		return _armedFgColor;
	return _defaultFgColor;
}

Color Button::GetButtonBgColor()
{
	if ( _depressed )
		return _depressedBgColor;
	if ( _armed )
		return _armedBgColor;
	return _defaultBgColor;
}

//-----------------------------------------------------------------------------
// Purpose: Set default button border attributes.
//-----------------------------------------------------------------------------
void Button::SetDefaultBorder( IBorder* border )
{
	_defaultBorder = border;
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Set depressed button border attributes.
//-----------------------------------------------------------------------------
void Button::SetDepressedBorder( IBorder* border )
{
	_depressedBorder = border;
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Set key focus button border attributes.
//-----------------------------------------------------------------------------
void Button::SetKeyFocusBorder( IBorder* border )
{
	_keyFocusBorder = border;
	InvalidateLayout( false, false );
}

IBorder* Button::GetBorder( bool depressed, bool, bool, bool keyFocus )
{
	if ( _buttonBorderEnabled )
	{
		if ( depressed )
			return _depressedBorder;
		if ( keyFocus || ( IsEnabled() && _defaultButton ) )
			return _keyFocusBorder;
	}
	else if ( depressed )
	{
		return _depressedBorder;
	}
	return _defaultBorder;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::OnSetState( int state )
{
	SetSelected( state != 0 );
	Repaint();
}

void Button::SetCommand( KeyValues* command )
{
	if ( _actionMessage )
		_actionMessage->deleteThis();
	_actionMessage = command;
}

//-----------------------------------------------------------------------------
// Purpose: sets the command to send when the button is pressed
//-----------------------------------------------------------------------------
void Button::SetCommand( const char* command )
{
	SetCommand( new KeyValues( "Command", "command", command ) );
}

void Button::SetAsDefaultButton( int state )
{
	if ( _defaultButton == ( state != 0 ) )
		return;

	_defaultButton = state != 0;
	if ( state && GetVParent() )
	{
		KeyValues* msg = new KeyValues( "DefaultButtonSet" );
		msg->SetPtr( "button", this );
		ivgui()->PostMessage( GetVParent(), msg, GetVPanel(), 0.0f );
	}

	InvalidateLayout( false, false );
	Repaint();
}

void Button::SetAsCurrentDefaultButton( int state )
{
	if ( _defaultButton == ( state != 0 ) )
		return;

	_defaultButton = state != 0;
	if ( state && GetVParent() )
	{
		KeyValues* msg = new KeyValues( "CurrentDefaultButtonSet" );
		msg->SetPtr( "button", this );
		ivgui()->PostMessage( GetVParent(), msg, GetVPanel(), 0.0f );
	}

	InvalidateLayout( false, false );
	Repaint();
}

bool Button::IsUseCaptureMouseEnabled()
{
	return _useCaptureMouse;
}

//-----------------------------------------------------------------------------
// Purpose: Set button to be mouse clickable or not.
//-----------------------------------------------------------------------------
void Button::SetMouseClickEnabled( MouseCode code, bool state )
{
	if ( state )
		_mouseClickMask |= 1 << ( ( code + 1 ) & 31 );
	else
		_mouseClickMask &= ~( 1 << ( ( code + 1 ) & 31 ) );
}

bool Button::IsMouseClickEnabled( MouseCode code )
{
	return ( _mouseClickMask & ( 1 << ( ( code + 1 ) & 31 ) ) ) != 0;
}

void Button::SetButtonActivationType( ActivationType_t type )
{
	_activationType = type;
}

//-----------------------------------------------------------------------------
// Purpose:	Activate a button click.
//-----------------------------------------------------------------------------
void Button::DoClick()
{
	SetSelected( true );
	FireActionSignal();

	// check for playing a transition sound
	if ( m_sReleasedSoundName != UTL_INVAL_SYMBOL )
		surface()->PlaySound( g_ButtonSoundNames.String( CUtlSymbol( m_sReleasedSoundName ) ) );
	SetSelected( false );
}

void Button::OnHotkey()
{
	DoClick();
}

//-----------------------------------------------------------------------------
// Purpose: Called when key focus is received
//-----------------------------------------------------------------------------
void Button::OnSetFocus()
{
	InvalidateLayout( false, false );
	Panel::OnSetFocus();
}

//-----------------------------------------------------------------------------
// Purpose: Respond when focus is killed
//-----------------------------------------------------------------------------
void Button::OnKillFocus()
{
	InvalidateLayout( false, false );
	Panel::OnKillFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::OnCursorEntered()
{
	if ( IsEnabled() )
		SetArmed( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::OnCursorExited()
{
	if ( !_keyDown )
		SetArmed( false );
}

//-----------------------------------------------------------------------------
// Purpose: Size the object to its button and text.  - only works from in ApplySchemeSettings or PerformLayout()
//-----------------------------------------------------------------------------
void Button::SizeToContents()
{
	int wide, tall;
	GetContentSize( wide, tall );

	int padding = 8;
	Panel::SetSize( wide + padding, tall + padding );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool Button::RequestInfo( KeyValues* outputData )
{
	if ( !_stricmp( outputData->GetName(), "CanBeDefaultButton" ) )
	{
		outputData->SetInt( "result", CanBeDefaultButton() );
		return true;
	}

	if ( !_stricmp( outputData->GetName(), "GetState" ) )
	{
		outputData->SetInt( "state", IsSelected() );
		return true;
	}

	if ( !_stricmp( outputData->GetName(), "GetCommand" ) )
	{
		const char* command = "";
		if ( _actionMessage )
			command = _actionMessage->GetString( "command", "" );
		outputData->SetString( "command", command );
		return true;
	}

	return Label::RequestInfo( outputData );
}

//-----------------------------------------------------------------------------
// Purpose: Perform graphical layout of button.
//-----------------------------------------------------------------------------
void Button::PerformLayout()
{
	// reset our border
	SetBorder( GetBorder( _depressed, _armed, _selected, HasFocus() ) );

	// set our color
	SetFgColor( GetButtonFgColor() );
	SetBgColor( GetButtonBgColor() );
	Label::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Message targets that the button has been pressed
//-----------------------------------------------------------------------------
void Button::FireActionSignal()
{
	if ( !_actionMessage )
		return;

	// see if it's a url
	if ( !_stricmp( _actionMessage->GetName(), "command" )
		&& !_strnicmp( _actionMessage->GetString( "command", "" ), "url ", 4 )
		&& strstr( _actionMessage->GetString( "command", "" ), "://" ) )
	{
		system()->ShellExecute( "open", _actionMessage->GetString( "command", "      " ) + 4 );
	}

	PostActionSignal( _actionMessage->MakeCopy() );
}

Button::Button( Panel* parent, const char* name, const char* text ) : Label( parent, name, text )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
Button::Button( Panel* parent, const char* name, const wchar_t* text ) : Label( parent, name, text )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
Button::~Button()
{
	if ( _actionMessage )
		_actionMessage->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::Init()
{
	_armed = false;
	_depressed = false;
	_selected = false;
	_forceDepressed = false;
	_buttonBorderEnabled = true;
	_useCaptureMouse = true;
	_keyDown = false;
	_mouseClickMask = 0;
	_actionMessage = NULL;
	_defaultBorder = NULL;
	_depressedBorder = NULL;
	_keyFocusBorder = NULL;
	_defaultButton = false;
	m_sArmedSoundName = UTL_INVAL_SYMBOL;
	m_sDepressedSoundName = UTL_INVAL_SYMBOL;
	m_sReleasedSoundName = UTL_INVAL_SYMBOL;

	SetTextInset( 6, 0 );
	SetMouseClickEnabled( MOUSE_LEFT, true );
	SetButtonActivationType( ACTIVATE_ONPRESSEDANDRELEASED );
	SetContentAlignment( a_west );
	SetTabPosition( 1 );
}

void Button::SetArmedSound( const char* name )
{
	if ( name )
		m_sArmedSoundName = g_ButtonSoundNames.AddString( name );
	else
		m_sArmedSoundName = UTL_INVAL_SYMBOL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::SetDepressedSound( const char* name )
{
	if ( name )
		m_sDepressedSoundName = g_ButtonSoundNames.AddString( name );
	else
		m_sDepressedSoundName = UTL_INVAL_SYMBOL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::SetReleasedSound( const char* name )
{
	if ( name )
		m_sReleasedSoundName = g_ButtonSoundNames.AddString( name );
	else
		m_sReleasedSoundName = UTL_INVAL_SYMBOL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::OnMouseReleased( MouseCode code )
{
	// ensure mouse capture gets released
	if ( IsUseCaptureMouseEnabled() )
		input()->SetMouseCapture( 0 );

	if ( _activationType == ACTIVATE_ONPRESSED )
		return;
	if ( !IsMouseClickEnabled( code ) )
		return;
	if ( !IsSelected() && _activationType == ACTIVATE_ONPRESSEDANDRELEASED )
		return;

	// it has to be both enabled and (mouse over the button or using a key) to fire
	if ( IsEnabled() && ( GetVPanel() == input()->GetMouseOver() || _keyDown ) )
		DoClick();
	else
		SetSelected( false );

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::OnMousePressed( MouseCode code )
{
	if ( !IsEnabled() || !IsMouseClickEnabled( code ) )
		return;

	if ( _activationType == ACTIVATE_ONPRESSED )
	{
		DoClick();
	}
	else
	{

		// play activation sound
		if ( m_sDepressedSoundName != UTL_INVAL_SYMBOL )
			surface()->PlaySound( g_ButtonSoundNames.String( CUtlSymbol( m_sDepressedSoundName ) ) );
		if ( IsUseCaptureMouseEnabled() && _activationType == ACTIVATE_ONPRESSEDANDRELEASED )
		{
			RequestFocus( 0 );
			SetSelected( true );
			Repaint();

			// lock mouse input to going to this button
			input()->SetMouseCapture( GetVPanel() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::OnMouseDoublePressed( MouseCode code )
{
	OnMousePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::OnKeyCodePressed( KeyCode code )
{
	if ( code == KEY_SPACE || code == KEY_ENTER )
	{
		SetArmed( true );
		_keyDown = true;
		OnMousePressed( MOUSE_LEFT );
		if ( IsUseCaptureMouseEnabled() )
			input()->SetMouseCapture( 0 );
	}
	else
	{
		_keyDown = false;
		Panel::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::OnKeyCodeReleased( KeyCode code )
{
	if ( _keyDown && ( code == KEY_SPACE || code == KEY_ENTER ) )
	{
		SetArmed( true );
		OnMouseReleased( MOUSE_LEFT );
	}
	else
	{
		Panel::OnKeyCodeReleased( code );
	}

	_keyDown = false;
	SetArmed( false );
}

void Button::ApplySchemeSettings( IScheme* scheme )
{
	Label::ApplySchemeSettings( scheme );

	_defaultBorder = scheme->GetBorder( "ButtonBorder" );
	_depressedBorder = scheme->GetBorder( "ButtonDepressedBorder" );
	_keyFocusBorder = scheme->GetBorder( "ButtonKeyFocusBorder" );

	_defaultFgColor = GetSchemeColor( "ButtonFgColor", GetFgColor(), scheme );
	_defaultBgColor = GetSchemeColor( "ButtonBgColor", GetBgColor(), scheme );
	_armedFgColor = GetSchemeColor( "ButtonArmedFgColor", _defaultFgColor, scheme );
	_armedBgColor = GetSchemeColor( "ButtonArmedBgColor", _defaultBgColor, scheme );
	_depressedFgColor = GetSchemeColor( "ButtonDepressedFgColor", _defaultFgColor, scheme );
	_depressedBgColor = GetSchemeColor( "ButtonDepressedBgColor", _defaultBgColor, scheme );
	_keyboardFocusColor = GetSchemeColor( "ButtonFocusBorder", Color( 0, 0, 0, 255 ), scheme );

	InvalidateLayout( false, false );
}

void Button::Paint()
{
	Label::Paint();

	if ( HasFocus() && IsEnabled() )
	{
		int wide, tall;
		GetSize( wide, tall );
		DrawFocusBorder( 3, 3, wide - 4, tall - 2 );
	}
}

void Button::DrawFocusBorder( int x0, int y0, int x1, int y1 )
{
	surface()->DrawSetColor( _keyboardFocusColor );

	int dash = 1;
	DrawDashedLine( x0, y0, x1, y0 + dash, dash, dash );	// top
	DrawDashedLine( x0, y0, x0 + dash, y1, dash, dash );	// left
	DrawDashedLine( x0, y1 - dash, x1, y1, dash, dash );	// bottom
	DrawDashedLine( x1 - dash, y0, x1, y1, dash, dash );	// right
}

bool Button::CanBeDefaultButton()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Get control settings for editing
//-----------------------------------------------------------------------------
void Button::GetSettings( KeyValues* outResourceData )
{
	Label::GetSettings( outResourceData );

	if ( _actionMessage )
		outResourceData->SetString( "command", _actionMessage->GetString( "command", "" ) );
	outResourceData->SetInt( "default", _defaultButton );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Button::ApplySettings( KeyValues* inResourceData )
{
	Label::ApplySettings( inResourceData );
	SetTextColorState( CS_LIGHT );

	const char* command = inResourceData->GetString( "command", "" );
	if ( *command )
		SetCommand( command );

	if ( inResourceData->GetInt( "default", 0 ) && CanBeDefaultButton() )
		SetAsDefaultButton( 1 );
}


//-----------------------------------------------------------------------------
// Purpose: Describes editing details
//-----------------------------------------------------------------------------
const char* Button::GetDescription()
{
	static char buf[1024];
	snprintf( buf, sizeof( buf ), "%s, string command, int default", Label::GetDescription() );
	return buf;
}
}
