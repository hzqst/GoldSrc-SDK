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

#include "MessageBox.h"
#include "controls.h"
#include "vgui/IInput.h"
#include "vgui/KeyValues.h"
#include "Button.h"
#include "Label.h"

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Sets the command
//-----------------------------------------------------------------------------
void MessageBox::SetCommand( KeyValues* command )
{
	if ( m_OkCommand )
		m_OkCommand->deleteThis();
	m_OkCommand = command;
}


//-----------------------------------------------------------------------------
// Purpose: Set a string command to be sent when the OK button is pressed.
//-----------------------------------------------------------------------------
void MessageBox::SetCommand( const char* command )
{
	if ( m_OkCommand )
		m_OkCommand->deleteThis();
	m_OkCommand = new KeyValues( "Command", "command", command );
}

void MessageBox::DoModal( Frame* frameOver )
{
	ShowWindow( frameOver );
/*
	// move to the middle of the screen
	// get the screen size
	int wide, tall;
	// get our dialog size
	GetSize(wide, tall);

	if (pFrameOver)
	{
		int frameX, frameY;
		int frameWide, frameTall;
		pFrameOver->GetPos(frameX, frameY);
		pFrameOver->GetSize(frameWide, frameTall);

		SetPos((frameWide - wide) / 2 + frameX, (frameTall - tall) / 2 + frameY);
	}
	else
	{
		int swide, stall;
		surface()->GetScreenSize(swide, stall);
		// put the dialog in the middle of the screen
		SetPos((swide - wide) / 2, (stall - tall) / 2);
	}

	SetVisible( true );
	SetEnabled( true );
	MoveToFront();

	if (m_pOkButton->IsVisible())
		m_pOkButton->RequestFocus();
	else	 // handle message boxes with no button
		RequestFocus();
*/
	input()->SetAppModalSurface( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void MessageBox::OnShutdownRequest()
{
	// Shutdown the dialog
	PostMessage( this, new KeyValues( "Close" ), 0.0f );
}

void MessageBox::ShowWindow( Frame* frameOver )
{
	m_pFrameOver = frameOver;

	SetVisible( true );
	SetEnabled( true );
	MoveToFront();

	if ( m_pOkButton->IsVisible() )
		m_pOkButton->RequestFocus( 0 );
	else
		RequestFocus( 0 );

	InvalidateLayout( false, false );
}


//-----------------------------------------------------------------------------
// Purpose: Toggles visibility of the close box.
//-----------------------------------------------------------------------------
void MessageBox::DisableCloseButton( bool state )
{
	Frame::SetCloseButtonVisible( state );
	m_pOkButton->SetCommand( "" );
}

//-----------------------------------------------------------------------------
// Purpose: Set the visibility of the OK button.
//-----------------------------------------------------------------------------
void MessageBox::SetOKButtonVisible( bool state )
{
	m_pOkButton->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the Text on the OK button
//-----------------------------------------------------------------------------
void MessageBox::SetOKButtonText( const char* text )
{
	m_pOkButton->SetText( text );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the Text on the OK button
//-----------------------------------------------------------------------------
void MessageBox::SetOKButtonText( const wchar_t* text )
{
	m_pOkButton->SetText( text );
	InvalidateLayout( false, false );
}

MessageBox::MessageBox( const char* title, const char* text, Panel* parent ) : Frame( parent, NULL, false, false )
{
	SetTitle( title, true );
	m_pMessageLabel = new Label( this, NULL, text );

	Init();
}

MessageBox::MessageBox( const wchar_t* wszTitle, const wchar_t* wszText, Panel* parent )
	: Frame( parent, NULL, false, false )
{
	SetTitle( wszTitle, true );
	m_pMessageLabel = new Label( this, NULL, wszText );

	Init();
}

MessageBox::~MessageBox()
{
	if ( m_OkCommand )
		m_OkCommand->deleteThis();
}

void MessageBox::Init()
{
	m_pFrameOver = NULL;

	SetMenuButtonResponsive( false );
	SetMinimizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetSizeable( false );

	m_pOkButton = new Button( this, "OKButton", "#MessageBox_OK" );
	m_pOkButton->SetCommand( "Close" );
	m_pOkButton->AddActionSignalTarget( this );

	m_OkCommand = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: size the message label properly
//-----------------------------------------------------------------------------
void MessageBox::ApplySchemeSettings( IScheme* pScheme )
{
	Frame::ApplySchemeSettings( pScheme );

	int wide, tall;
	m_pMessageLabel->GetContentSize( wide, tall );
	m_pMessageLabel->SetSize( wide, tall );

	wide += GetProportionalScaledValue( 100 );
	tall += GetProportionalScaledValue( 100 );
	SetSize( wide, tall );

	if ( m_pFrameOver )
	{
		int frameX, frameY;
		m_pFrameOver->GetPos( frameX, frameY );

		int frameWide, frameTall;
		m_pFrameOver->GetSize( frameWide, frameTall );

		SetPos( frameX + ( frameWide - wide ) / 2, frameY + ( frameTall - tall ) / 2 );
	}
	else
	{
		int swide, stall;
		surface()->GetScreenSize( swide, stall );

		// put the dialog in the middle of the screen
		SetPos( ( swide - wide ) / 2, ( stall - tall ) / 2 );
	}
}

void MessageBox::PerformLayout()
{
	int x, y, wide, tall;
	GetClientArea( x, y, wide, tall );
	wide += x;
	tall += y;

	int boxWidth, boxTall;
	GetSize( boxWidth, boxTall );

	int oldWide, oldTall;
	m_pOkButton->GetSize( oldWide, oldTall );

	int btnWide, btnTall;
	m_pOkButton->GetContentSize( btnWide, btnTall );
	btnWide += GetProportionalScaledValue( 10 );
	if ( btnWide < oldWide )
		btnWide = oldWide;
	btnTall += GetProportionalScaledValue( 10 );
	if ( btnTall < oldTall )
		btnTall = oldTall;
	m_pOkButton->SetSize( btnWide, btnTall );

	int labelWide = m_pMessageLabel->GetWide();
	labelWide += GetProportionalScaledValue( 100 );
	if ( boxWidth < btnWide * 2 + 100 )
		boxWidth = btnWide * 2 + 100;
	if ( boxWidth < labelWide )
		boxWidth = labelWide;
	SetSize( boxWidth, boxTall );
	GetSize( boxWidth, boxTall );

	int labelY = y + GetProportionalScaledValue( 15 );
	m_pMessageLabel->SetPos( x + wide / 2 - m_pMessageLabel->GetWide() / 2, labelY );

	int buttonY = tall - m_pOkButton->GetTall();
	buttonY -= GetProportionalScaledValue( 15 );
	m_pOkButton->SetPos( x + wide / 2 - m_pOkButton->GetWide() / 2, buttonY );

	Frame::PerformLayout();
	GetSize( boxWidth, boxTall );
}

void MessageBox::OnClose()
{
	Frame::OnClose();

	if ( m_OkCommand )
		PostActionSignal( m_OkCommand->MakeCopy() );

	MarkForDeletion();
}
}
