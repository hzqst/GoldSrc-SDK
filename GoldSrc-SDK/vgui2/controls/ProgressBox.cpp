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

#include "ProgressBox.h"
#include "Button.h"
#include "Label.h"
#include "ProgressBar.h"
#include "controls.h"
#include "../../engine/controls.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"

#include <cwchar>

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\ProgressBox.cpp" )
#else
#include <assert.h>
#define ASSERT_AT( expression, line )	assert( expression )
#endif

namespace vgui2
{
static_assert( sizeof( Frame ) == 276 );
static_assert( sizeof( ProgressBox ) == 1072 );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ProgressBox::ProgressBox( const char* title, const char* info, const char* text, Panel* parent )
	: Frame( parent, nullptr, parent == nullptr, false )
{
	const wchar_t* localized = localize()->Find( title );
	if ( localized )
		std::wcsncpy( m_wszTitle, localized, sizeof( m_wszTitle ) / sizeof( m_wszTitle[0] ) );
	else
		localize()->ConvertANSIToUnicode( title, m_wszTitle, sizeof( m_wszTitle ) );

	m_pMessageLabel = new Label( this, nullptr, text );
	localized = localize()->Find( info );
	if ( localized )
		std::wcsncpy( m_wszInfo, localized, sizeof( m_wszInfo ) / sizeof( m_wszInfo[0] ) );
	else
		m_wszInfo[0] = L'\0';

	localized = localize()->Find( text );
	if ( localized )
		std::wcsncpy( m_wszText, localized, sizeof( m_wszText ) / sizeof( m_wszText[0] ) );
	else
		m_wszText[0] = L'\0';

	Init();
}

ProgressBox::~ProgressBox()
{
}

void ProgressBox::Init()
{
	constexpr int initialWide = 384;
	constexpr int initialTall = 128;
	constexpr int cancelWide = 72;
	constexpr int cancelTall = 24;
	constexpr int tickIntervalMilliseconds = 1000;

	m_pProgressBar = new ProgressBar( this, nullptr );
	m_pProgressBar->SetVisible( false );
	m_pCancelButton = new Button( this, nullptr, "#VGui_Cancel" );
	m_pCancelButton->SetSize( cancelWide, cancelTall );
	m_pCancelButton->SetCommand( "Cancel" );
	SetMenuButtonResponsive( false );
	SetMinimizeButtonVisible( false );
	SetCancelButtonVisible( false );
	SetSizeable( false );
	SetSize( initialWide, initialTall );
	m_flProgress = 0.0f;
	m_flStartTime = -0.1f;
	m_flLastProgressUpdate = 0.0f;
	// mark ourselves as needed ticked once a second, to force us to repaint
	ivgui()->AddTickSignal( GetVPanel(), tickIntervalMilliseconds );
	UpdateTitle();
}

//-----------------------------------------------------------------------------
// Purpose: resize the message label
//-----------------------------------------------------------------------------
void ProgressBox::ApplySchemeSettings( IScheme* scheme )
{
	constexpr int dialogWide = 384;
	constexpr int labelWide = 344;
	constexpr int verticalSpace = 92;
	Frame::ApplySchemeSettings( scheme );
	int wide;
	int tall;
	m_pMessageLabel->GetContentSize( wide, tall );
	SetSize( dialogWide, tall + verticalSpace );
	m_pMessageLabel->SetSize( labelWide, tall );
}

void ProgressBox::DoModal( Frame* frameOver )
{
	ShowWindow( frameOver );
	input()->SetAppModalSurface( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: Activates the window
//-----------------------------------------------------------------------------
void ProgressBox::ShowWindow( Frame* frameOver )
{
	int wide;
	int tall;
	// get our dialog size
	GetSize( wide, tall );
	if ( frameOver )
	{
		int x;
		int y;
		int frameWide;
		int frameTall;
		frameOver->GetPos( x, y );
		frameOver->GetSize( frameWide, frameTall );
		SetPos( x + ( frameWide - wide ) / 2, y + ( frameTall - tall ) / 2 );
	}
	else
	{
		int screenWide;
		int screenTall;
		surface()->GetScreenSize( screenWide, screenTall );
		// put the dialog in the middle of the screen
		SetPos( ( screenWide - wide ) / 2, ( screenTall - tall ) / 2 );
	}
	Frame::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: updates progress bar, range [0, 1]
//-----------------------------------------------------------------------------
void ProgressBox::SetProgress( float progress )
{
	ASSERT_AT( progress >= 0.0f && progress <= 1.0f, 209 );
	m_pProgressBar->SetProgress( progress );
	m_pProgressBar->SetVisible( true );
	// only update progress timings if the progress has actually changed
	if ( progress != m_flProgress )
	{
		if ( m_flStartTime < 0.0f )
			m_flStartTime = static_cast<float>( system()->GetFrameTime() );
		m_flProgress = progress;
		m_flLastProgressUpdate = static_cast<float>( system()->GetFrameTime() );
		UpdateTitle();
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the info text
//-----------------------------------------------------------------------------
void ProgressBox::SetText( const char* text )
{
	m_pMessageLabel->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: Toggles visibility of the close box.
//-----------------------------------------------------------------------------
void ProgressBox::SetCancelButtonVisible( bool state )
{
	Frame::SetCloseButtonVisible( state );
	m_pCancelButton->SetVisible( state );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ProgressBox::SetCancelButtonEnabled( bool state )
{
	m_pCancelButton->SetEnabled( state );
	Frame::SetCloseButtonVisible( state );
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: On update cancelled
//-----------------------------------------------------------------------------
void ProgressBox::OnCancel()
{
	// post a message that we've been cancelled
	PostActionSignal( new KeyValues( "ProgressBoxCancelled" ) );
	Close();
}

//-----------------------------------------------------------------------------
// Purpose: Deletes self when closed
//-----------------------------------------------------------------------------
void ProgressBox::OnClose()
{
	Frame::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: close button pressed
//-----------------------------------------------------------------------------
void ProgressBox::OnCloseFrameButtonPressed()
{
	OnCancel();
}

//-----------------------------------------------------------------------------
// Purpose: Handles ESC closing dialog
//-----------------------------------------------------------------------------
void ProgressBox::OnCommand( const char* command )
{
	if ( _stricmp( command, "Cancel" ) == 0 )
		OnCancel();
	else
		Frame::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ProgressBox::OnShutdownRequest()
{
	// Shutdown the dialog
	PostMessage( this, new KeyValues( "Command", "command", "Cancel" ), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: called every render
//-----------------------------------------------------------------------------
void ProgressBox::OnThink()
{
	if ( !( m_flStartTime >= 0.0f ) || !m_wszInfo[0] )
		return;

	wchar_t remaining[128];
	if ( ProgressBar::ConstructTimeRemainingString( remaining, sizeof( remaining ), m_flStartTime,
			 static_cast<float>( system()->GetFrameTime() ), m_flProgress, m_flLastProgressUpdate, true ) )
	{
		wchar_t info[256];
		localize()->ConstructString( info, sizeof( info ), m_wszInfo, 1, remaining );
		m_pMessageLabel->SetText( info );
	}
	else
	{
		m_pMessageLabel->SetText( m_wszText );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Forces us to repaint once per second
//-----------------------------------------------------------------------------
void ProgressBox::OnTick()
{
	if ( m_flStartTime >= 0.0f )
		Repaint();
	Panel::OnTick();
}

void ProgressBox::PerformLayout()
{
	constexpr int leftMargin = 16;
	constexpr int topMargin = 12;
	constexpr int progressTopMargin = 16;
	constexpr int progressRightSpace = 44;
	constexpr int progressTall = 24;
	constexpr int cancelGap = 10;
	int x;
	int y;
	int wide;
	int tall;
	GetClientArea( x, y, wide, tall );
	wide += x;
	tall += y;
	m_pMessageLabel->SetPos( x + leftMargin, y + topMargin );
	m_pProgressBar->SetPos( x + leftMargin, y + progressTopMargin + m_pMessageLabel->GetTall() );
	m_pProgressBar->SetSize( wide - progressRightSpace, progressTall );
	if ( m_pCancelButton->IsVisible() )
	{
		int cancelWide = m_pCancelButton->GetWide();
		int progressX;
		int progressY;
		int progressWide;
		int progressHeight;
		m_pProgressBar->GetBounds( progressX, progressY, progressWide, progressHeight );
		m_pCancelButton->SetPos( progressX + progressWide - cancelWide, progressY );
		m_pProgressBar->SetSize( progressWide - cancelWide - cancelGap, progressHeight );
	}
	Frame::PerformLayout();
}

void ProgressBox::UpdateTitle()
{
	wchar_t suffix[64];
	int percent = static_cast<int>( m_flProgress * 100.0f );
	if ( percent > 0 )
		_snwprintf( suffix, sizeof( suffix ) / sizeof( suffix[0] ), L"- %d%% complete", percent );
	else
		suffix[0] = L'\0';

	wchar_t title[256];
	localize()->ConstructString( title, sizeof( title ), m_wszTitle, 1, suffix );
	SetTitle( title, true );
}
}
