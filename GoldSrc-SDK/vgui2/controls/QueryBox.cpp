//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
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
//
// $NoKeywords: $
//=============================================================================

#include "QueryBox.h"
#include "Button.h"
#include "vgui/KeyValues.h"
#include <cstring>

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
QueryBox::QueryBox( const char* title, const char* queryText, Panel* parent ) : MessageBox( title, queryText, parent )
{
	SetHDProportional( true );

	m_pCancelButton = new Button( this, "CancelButton", "#QueryBox_Cancel" );
	m_pCancelButton->SetCommand( "Cancel" );
	m_pOkButton->SetCommand( "OK" );

	m_pCancelCommand = NULL;
	m_pOkCommand = NULL;

	m_pOkButton->SetTabPosition( 1 );
	m_pCancelButton->SetTabPosition( 2 );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
QueryBox::QueryBox( const wchar_t* wszTitle, const wchar_t* wszQueryText, Panel* parent )
	: MessageBox( wszTitle, wszQueryText, parent )
{
	SetHDProportional( true );

	m_pCancelButton = new Button( this, "CancelButton", "#QueryBox_Cancel" );
	m_pCancelButton->SetCommand( "Cancel" );
	m_pOkButton->SetCommand( "OK" );

	m_pCancelCommand = NULL;
	m_pOkCommand = NULL;

	m_pOkButton->SetTabPosition( 1 );
	m_pCancelButton->SetTabPosition( 2 );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
QueryBox::~QueryBox()
{
	if ( m_pCancelButton )
		delete m_pCancelButton;
	if ( m_pOkCommand )
		m_pOkCommand->deleteThis();
	if ( m_pCancelCommand )
		m_pCancelCommand->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Set the keyvalues to send when ok button is hit
// Input  : keyValues - 
//-----------------------------------------------------------------------------
void QueryBox::SetOKCommand( KeyValues* keyValues )
{
	if ( m_pOkCommand )
		m_pOkCommand->deleteThis();
	m_pOkCommand = keyValues;
}

//-----------------------------------------------------------------------------
// Purpose: Set a value of the ok command
// Input  : keyValues - 
//-----------------------------------------------------------------------------
void QueryBox::SetOKCommandValue( const char* keyName, int value )
{
	if ( !m_pOkCommand )
		m_pOkCommand = new KeyValues( "Command" );

	m_pOkCommand->SetInt( keyName, value );
}

//-----------------------------------------------------------------------------
// Purpose: Set the keyvalues to send when the cancel button is hit
// Input  : keyValues - 
//-----------------------------------------------------------------------------
void QueryBox::SetCancelCommand( KeyValues* keyValues )
{
	if ( m_pCancelCommand )
		m_pCancelCommand->deleteThis();
	m_pCancelCommand = keyValues;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the cancel button text
//-----------------------------------------------------------------------------
void QueryBox::SetCancelButtonText( const char* buttonText )
{
	m_pCancelButton->SetText( buttonText );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the cancel button text
//-----------------------------------------------------------------------------
void QueryBox::SetCancelButtonText( const wchar_t* wszButtonText )
{
	m_pCancelButton->SetText( wszButtonText );
	InvalidateLayout( false, false );
}

void QueryBox::SetKeyFocusOnCancelButton()
{
	m_pCancelButton->RequestFocus( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Layout the window for drawing 
//-----------------------------------------------------------------------------
void QueryBox::PerformLayout()
{
	MessageBox::PerformLayout();

	int boxWidth, boxTall;
	GetSize( boxWidth, boxTall );

	int x, y, wide, tall;
	GetClientArea( x, y, wide, tall );
	wide += x;
	tall += y;

	int oldWide, oldTall;
	m_pCancelButton->GetSize( oldWide, oldTall );

	int btnWide, btnTall;
	m_pCancelButton->GetContentSize( btnWide, btnTall );
	btnWide += GetProportionalScaledValue( 10 );
	if ( btnWide < oldWide )
		btnWide = oldWide;
	btnTall += GetProportionalScaledValue( 10 );
	if ( btnTall < oldTall )
		btnTall = oldTall;
	m_pCancelButton->SetSize( btnWide, btnTall );

//nt boxWidth, boxTall;
	GetSize( boxWidth, boxTall );
//	wide = max(wide, btnWide * 2 + 100);
//	SetSize(wide, tall);

	int okY = tall - m_pOkButton->GetTall();
	okY -= GetProportionalScaledValue( 15 );
	int okX = wide / 2 - m_pOkButton->GetWide();
	okX -= GetProportionalScaledValue( 1 );
	m_pOkButton->SetPos( okX + x, okY );

	int cancelY = tall - m_pCancelButton->GetTall();
	cancelY -= GetProportionalScaledValue( 15 );
	int cancelX = x + wide / 2;
	cancelX += GetProportionalScaledValue( 16 );
	m_pCancelButton->SetPos( cancelX, cancelY );
}

//-----------------------------------------------------------------------------
// Purpose: Handles command text from the buttons
//			Deletes self when closed
// Input  : *command - 
//-----------------------------------------------------------------------------
void QueryBox::OnCommand( const char* command )
{
	if ( !_stricmp( command, "OK" ) )
	{
		OnCommand( "Close" );

		if ( m_pOkCommand )
			PostActionSignal( m_pOkCommand->MakeCopy() );

		MarkForDeletion();
	}
	else if ( !_stricmp( command, "Cancel" ) )
	{
		OnCommand( "Close" );

		if ( m_pCancelCommand )
			PostActionSignal( m_pCancelCommand->MakeCopy() );

		MarkForDeletion();
	}

	Frame::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : code - 
//-----------------------------------------------------------------------------
void QueryBox::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ESCAPE )
		OnCommand( "Cancel" );
	else
		Frame::OnKeyCodeTyped( code );
}
}
