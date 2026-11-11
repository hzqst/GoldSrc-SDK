//=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
// This class is a message box that has two buttons, ok and cancel instead of
// just the ok button of a message box. We use a message box class for the ok button
// and implement another button here.
//=============================================================================

#include "Tooltip.h"
#include "TextEntry.h"
#include "controls.h"
#include "vgui/IInput.h"
#include "vgui/ISystem.h"

#include <cmath>
#include <cstring>

namespace vgui2
{
static TextEntry* s_pTooltipWindow = NULL;
static int s_iTooltipWindowCount = 0;

//-----------------------------------------------------------------------------
// Purpose: Reset the tooltip delay timer
//-----------------------------------------------------------------------------
void Tooltip::ResetDelay()
{
	_delay = system()->GetTimeMillis() + _tooltipDelay;
}

void Tooltip::InvalidateLayout()
{
	m_bInvalidLayout = true;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
Tooltip::Tooltip( Panel*, const char* text )
{
	if ( !s_pTooltipWindow )
	{
		s_pTooltipWindow = new TextEntry( NULL, "tooltip" );
		// Shared by all helpers; parent panels must only detach this window.
		s_pTooltipWindow->SetAutoDelete( false );
	}
	s_iTooltipWindowCount++;

	// this line prevents the main window from losing focus
	// when a tooltip pops up
	s_pTooltipWindow->MakePopup( false, true );

	SetText( text );
	s_pTooltipWindow->SetText( m_Text.Base() );
	s_pTooltipWindow->SetEditable( false );
	s_pTooltipWindow->SetMultiline( true );
	s_pTooltipWindow->SetVisible( false );

	_displayOnOneLine = false;
	_makeVisible = false;
	m_bInvalidLayout = true;
	_tooltipDelay = 500;
	_delay = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
Tooltip::~Tooltip()
{
	s_iTooltipWindowCount--;
	if ( s_iTooltipWindowCount <= 0 )
	{
		if ( s_pTooltipWindow )
			delete s_pTooltipWindow;
		s_pTooltipWindow = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the tooltip text
// Input: text- the tooltip message
//-----------------------------------------------------------------------------
void Tooltip::SetText( const char* text )
{
	if ( !text )
		text = "";

	if ( m_Text.Count() > 0 )
		m_Text.RemoveAll();

	for ( size_t i = 0; i < strlen( text ); i++ )
		m_Text.AddToTail( text[i] );
	m_Text.AddToTail( '\0' );

	if ( s_pTooltipWindow )
		s_pTooltipWindow->SetText( m_Text.Base() );
}

//-----------------------------------------------------------------------------
// Purpose: Get the tooltip text
// Output: text- the tooltip message
//-----------------------------------------------------------------------------
const char* Tooltip::GetText()
{
	return m_Text.Base();
}

//-----------------------------------------------------------------------------
// Purpose: Set the tooltip delay before a tooltip comes up.
//-----------------------------------------------------------------------------
void Tooltip::SetTooltipDelay( int tooltipDelay )
{
	_tooltipDelay = tooltipDelay;
}

//-----------------------------------------------------------------------------
// Purpose: Get the tooltip delay before a tooltip comes up.
//-----------------------------------------------------------------------------
int Tooltip::GetTooltipDelay()
{
	return _tooltipDelay;
}

//-----------------------------------------------------------------------------
// Purpose: Size the text window so all the text fits inside it.
//-----------------------------------------------------------------------------
void Tooltip::SizeTextWindow()
{
	if ( _displayOnOneLine )
	{
		// We want the tool tip to be one line
		s_pTooltipWindow->SetMultiline( false );
		s_pTooltipWindow->SetToFullWidth();
		return;
	}

	// We want the tool tip to be one line
	s_pTooltipWindow->SetMultiline( false );
	s_pTooltipWindow->SetToFullWidth();

	int wide = 0;
	int tall = 0;
	s_pTooltipWindow->GetSize( wide, tall );

	double newWide = sqrt( 2.0 * wide * tall );
	double newTall = newWide * 0.0;
	s_pTooltipWindow->SetMultiline( true );
	s_pTooltipWindow->SetSize( (int)newWide, (int)newTall );
	s_pTooltipWindow->SetToFullHeight();

	s_pTooltipWindow->GetSize( wide, tall );
	if ( wide < 100 && s_pTooltipWindow->GetNumLines() == 2 )
	{
		s_pTooltipWindow->SetMultiline( false );
		s_pTooltipWindow->SetToFullWidth();
	}
	else
	{
#if defined( _WIN32 )
		while ( (float)wide / (float)tall < 2.0f )
#else
		while ( (double)wide / (double)tall < 2.0 )
#endif
		{
			s_pTooltipWindow->SetSize( wide + 1, tall );
			s_pTooltipWindow->SetToFullHeight();
			s_pTooltipWindow->GetSize( wide, tall );
		}
	}

	s_pTooltipWindow->GetSize( wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: Display the tooltip
//-----------------------------------------------------------------------------
void Tooltip::PerformLayout()
{
	if ( !m_bInvalidLayout || !_makeVisible )
		return;

	if ( _delay > system()->GetTimeMillis() )
		return;

	m_bInvalidLayout = false;

	// this line reports the wrong panel with focus
	//	Panel *currentFocus = ipanel()->Client(input()->GetFocus())->GetPanel();
	//Panel *currentFocus = s_pTooltipWindow->GetParent();

	s_pTooltipWindow->SetVisible( true );

	IScheme* pScheme = scheme()->GetIScheme( s_pTooltipWindow->GetScheme() );
	s_pTooltipWindow->SetBgColor(
		s_pTooltipWindow->GetSchemeColor( "SelectionBG", s_pTooltipWindow->GetBgColor(), pScheme ) );
	s_pTooltipWindow->SetFgColor(
		s_pTooltipWindow->GetSchemeColor( "BorderSelection", s_pTooltipWindow->GetFgColor(), pScheme ) );
	s_pTooltipWindow->SetBorder( pScheme->GetBorder( "ToolTipBorder" ) );

	int cursorX = 0;
	int cursorY = 0;
	input()->GetCursorPos( cursorX, cursorY );

	// relayout the textwindow immediately so that we know it's size
	//m_pTextEntry->InvalidateLayout(true);
	SizeTextWindow();

	int menuWide = 0;
	int menuTall = 0;
	s_pTooltipWindow->GetSize( menuWide, menuTall );

	int wide = 0;
	int tall = 0;
	surface()->GetScreenSize( wide, tall );

	if ( cursorX >= wide - menuWide )
	{
		if ( cursorY >= tall - menuTall )
			cursorY = cursorY - menuTall - 20;

		// menu hanging down
		s_pTooltipWindow->SetPos( cursorX - menuWide, cursorY );
	}
	else
	{
		cursorY = cursorY + 20;
		if ( cursorY >= tall - menuTall )
			cursorY = cursorY - menuTall - 20;

		s_pTooltipWindow->SetPos( cursorX, cursorY );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Display the tooltip
//-----------------------------------------------------------------------------
void Tooltip::ShowTooltip( Panel* currentPanel )
{
	s_pTooltipWindow->SetText( m_Text.Base() );
//	s_pTooltipWindow->SetVisible(true);

	s_pTooltipWindow->SetParent( currentPanel );
	_makeVisible = true;

	PerformLayout();
}

void Tooltip::SetTooltipFormatToSingleLine()
{
	_displayOnOneLine = true;
}

void Tooltip::SetTooltipFormatToMultiLine()
{
	_displayOnOneLine = false;
}

//-----------------------------------------------------------------------------
// Purpose: Display the tooltip
//-----------------------------------------------------------------------------
void Tooltip::HideTooltip()
{
	s_pTooltipWindow->SetVisible( false );
	_makeVisible = false;
}

void Tooltip::ApplySchemeSettings( IScheme* scheme )
{
	bool hdProportional = s_pTooltipWindow->IsHDProportional();
	bool proportional = s_pTooltipWindow->IsProportional();
	s_pTooltipWindow->SetFont( scheme->GetFontEx( "DefaultSmall", proportional, hdProportional ) );
}
}
