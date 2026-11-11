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

#include "ProgressBar.h"
#include "controls.h"
#include "../../engine/controls.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"
#include <cstdio>
#include <cstring>

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\ProgressBar.cpp" )
#else
#include <assert.h>
#define ASSERT_AT( expression, line )	assert( expression )
#endif

#if !defined( _WIN32 )
#define _snprintf snprintf
#endif

namespace vgui2
{
#pragma warning( suppress : 26495 )
ProgressBar::ProgressBar( Panel* parent, const char* panelName ) : Panel( parent, panelName )
{
	_progress = 0.0f;
	m_pszDialogVar = NULL;
	_segmentWide = GetProportionalScaledValue( 8 );
	_segmentGap = GetProportionalScaledValue( 4 );
	m_iBarInset = GetProportionalScaledValue( 4 );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ProgressBar::~ProgressBar()
{
	delete[] m_pszDialogVar;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ProgressBar::SetProgress( float progress )
{
	if ( progress == _progress )
		return;

	float clamped = 0.0f;
	if ( progress >= 0.0f )
		clamped = progress < 1.0f ? progress : 1.0f;

	_progress = clamped;
	Repaint();
}

void ProgressBar::SetSegmentInfo( int gap, int width )
{
	_segmentGap = gap;
	_segmentWide = width;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
float ProgressBar::GetProgress()
{
	return _progress;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void ProgressBar::SetBarInset( int pixels )
{
	m_iBarInset = pixels;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
int ProgressBar::GetBarInset()
{
	return m_iBarInset;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ProgressBar::PaintBackground()
{
	int wide, tall;
	GetSize( wide, tall );

	surface()->DrawSetColor( GetBgColor() );
	surface()->DrawFilledRect( 0, 0, wide, tall );

	// gaps
	int segmentTotal = wide / ( _segmentGap + _segmentWide );
	int segmentsDrawn = (int)( (float)segmentTotal * _progress );

	surface()->DrawSetColor( GetFgColor() );

	int x = 0;
	for ( int i = 0; i < segmentsDrawn; i++ )
	{
		x += _segmentGap;
		surface()->DrawFilledRect( x, m_iBarInset, x + _segmentWide, tall - m_iBarInset );
		x += _segmentWide;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ProgressBar::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );

	SetFgColor( GetSchemeColor( "BrightControlText", scheme ) );
	SetBgColor( GetSchemeColor( "WindowBgColor", scheme ) );
	SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ProgressBar::ApplySettings( KeyValues* inResourceData )
{
	_progress = inResourceData->GetFloat( "progress", 0.0f );

	const char* dialogVar = inResourceData->GetString( "variable", "" );
	if ( dialogVar && dialogVar[0] )
	{
		m_pszDialogVar = new char[strlen( dialogVar ) + 1];
		strcpy( m_pszDialogVar, dialogVar );
	}

	Panel::ApplySettings( inResourceData );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ProgressBar::GetSettings( KeyValues* outResourceData )
{
	Panel::GetSettings( outResourceData );

	outResourceData->SetFloat( "progress", _progress );
	if ( m_pszDialogVar )
		outResourceData->SetString( "variable", m_pszDialogVar );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a string description of the panel fields for use in the UI
//-----------------------------------------------------------------------------
const char* ProgressBar::GetDescription()
{
	static char buf[1024];
	_snprintf( buf, sizeof( buf ), "%s, string progress, string variable", Panel::GetDescription() );
	return buf;
}

//-----------------------------------------------------------------------------
// Purpose: updates progress bar bases on values
//-----------------------------------------------------------------------------
void ProgressBar::OnDialogVariablesChanged( KeyValues* dialogVariables )
{
	if ( !m_pszDialogVar )
		return;

	int val = dialogVariables->GetInt( m_pszDialogVar, -1 );
	if ( val >= 0 )
		SetProgress( (float)val / 100.0f );
}

//-----------------------------------------------------------------------------
// Purpose: utility function for calculating a time remaining string
//-----------------------------------------------------------------------------
bool ProgressBar::ConstructTimeRemainingString( wchar_t* output, int outputBufferSizeInBytes, float startTime,
	float currentTime, float currentProgress, float lastProgressUpdateTime, bool addRemainingSuffix )
{
#if defined( _WIN32 )
	ASSERT_AT( lastProgressUpdateTime <= currentTime, 128 );
#endif
	output[0] = 0;

	// calculate pre-extrapolation values
	float timeElapsed = lastProgressUpdateTime - startTime;
	float totalTime = timeElapsed / currentProgress;
	// calculate seconds
	int secondsRemaining = (int)( totalTime - timeElapsed );
	if ( lastProgressUpdateTime < currentTime )
	{
		// old update, extrapolate
		float progressRate = currentProgress / timeElapsed;
		float extrapolatedProgress = progressRate * ( currentTime - startTime );
		float extrapolatedTotalTime = ( currentTime - startTime ) / extrapolatedProgress;
		secondsRemaining = (int)( extrapolatedTotalTime - timeElapsed );
	}

	if ( secondsRemaining == 0 && ( totalTime - timeElapsed ) > 0 )
		secondsRemaining = 1;

	// calculate minutes
	int minutesRemaining = 0;
	if ( secondsRemaining >= 60 )
	{
		minutesRemaining = secondsRemaining / 60;
		secondsRemaining %= 60;
	}

	char minutesBuf[16];
	sprintf( minutesBuf, "%d", minutesRemaining );
	char secondsBuf[16];
	sprintf( secondsBuf, "%d", secondsRemaining );

	char unlocString[64];
	if ( minutesRemaining > 0 )
	{
		wchar_t unicodeMinutes[16];
		localize()->ConvertANSIToUnicode( minutesBuf, unicodeMinutes, sizeof( unicodeMinutes ) );
		wchar_t unicodeSeconds[16];
		localize()->ConvertANSIToUnicode( secondsBuf, unicodeSeconds, sizeof( unicodeSeconds ) );

		const char* token = "#vgui_TimeLeftMinutesSeconds";
		if ( minutesRemaining == 1 && secondsRemaining == 1 )
			token = "#vgui_TimeLeftMinuteSecond";
		else if ( minutesRemaining == 1 )
			token = "#vgui_TimeLeftMinuteSeconds";
		else if ( secondsRemaining == 1 )
			token = "#vgui_TimeLeftMinutesSecond";

		strcpy( unlocString, token );
		if ( addRemainingSuffix )
			strcat( unlocString, "Remaining" );

		localize()->ConstructString(
			output, outputBufferSizeInBytes, localize()->Find( unlocString ), 2, unicodeMinutes, unicodeSeconds );
	}
	else if ( secondsRemaining > 0 )
	{
		wchar_t unicodeSeconds[16];
		localize()->ConvertANSIToUnicode( secondsBuf, unicodeSeconds, sizeof( unicodeSeconds ) );

		const char* token = "#vgui_TimeLeftSeconds";
		if ( secondsRemaining == 1 )
			token = "#vgui_TimeLeftSecond";

		strcpy( unlocString, token );
		if ( addRemainingSuffix )
			strcat( unlocString, "Remaining" );

		localize()->ConstructString( output, outputBufferSizeInBytes, localize()->Find( unlocString ), 1, unicodeSeconds );
	}
	else
	{
		return false;
	}

	return true;
}
}
