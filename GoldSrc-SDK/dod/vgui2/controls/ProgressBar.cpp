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
#include "../../../engine/controls.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"
#include <cstdio>
#include <cstring>

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
ProgressBar::ProgressBar( Panel* parent, const char* panelName ) : Panel( parent, panelName )
{
	_progress = 0.0f;
	m_pszDialogVar = NULL;
	_segmentWide = 8;
	_segmentGap = 4;
	m_iBarInset = 4;
}

ProgressBar::~ProgressBar()
{
	if ( m_pszDialogVar )
		delete[] m_pszDialogVar;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ProgressBar::SetProgress( float progress )
{
	if ( progress == _progress )
		return;

	// clamp the progress value within the range
	if ( progress < 0.0f )
		progress = 0.0f;
	else if ( progress > 1.0f )
		progress = 1.0f;

	_progress = progress;
	Repaint();
}

void ProgressBar::SetSegmentInfo( int gap, int width )
{
	_segmentGap = gap;
	_segmentWide = width;
}

float ProgressBar::GetProgress()
{
	return _progress;
}

void ProgressBar::SetBarInset( int pixels )
{
	m_iBarInset = pixels;
}

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

	surface()->DrawSetColor( GetFgColor() );
	surface()->DrawFilledRect( 0, 0, wide, tall );


	// gaps
	int segmentTotal = wide / ( _segmentGap + _segmentWide );
	int segmentsDrawn = (int)( (float)segmentTotal * _progress );

	surface()->DrawSetColor( GetBgColor() );

	int x = 0;
	for ( int i = 0; i < segmentsDrawn; i++ )
	{
		x += _segmentGap;
		surface()->DrawFilledRect( x, m_iBarInset, x + _segmentWide, tall - m_iBarInset );
		x += _segmentWide;
	}
}

void ProgressBar::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );

	SetFgColor( GetSchemeColor( "BrightControlText", scheme ) );
	SetBgColor( GetSchemeColor( "WindowBgColor", scheme ) );
	SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
}

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

void ProgressBar::GetSettings( KeyValues* outResourceData )
{
	Panel::GetSettings( outResourceData );

	outResourceData->SetFloat( "progress", _progress );
	if ( m_pszDialogVar )
		outResourceData->SetString( "variable", m_pszDialogVar );
}

const char* ProgressBar::GetDescription()
{
	static char buf[1024];
	snprintf( buf, sizeof( buf ), "%s, string progress, string variable", Panel::GetDescription() );
	return buf;
}

void ProgressBar::OnDialogVariablesChanged( KeyValues* dialogVariables )
{
	if ( !m_pszDialogVar )
		return;

	int val = dialogVariables->GetInt( m_pszDialogVar, -1 );
	if ( val >= 0 )
		SetProgress( (float)val / 100.0f );
}

bool ProgressBar::ConstructTimeRemainingString( wchar_t* output, int outputBufferSizeInBytes, float startTime,
	float currentTime, float currentProgress, float lastProgressUpdateTime, bool addRemainingSuffix )
{
	output[0] = 0;

	// calculate pre-extrapolation values
	float timeElapsed = lastProgressUpdateTime - startTime;
	float totalTime = timeElapsed / currentProgress;
	float timeRemaining = totalTime - timeElapsed;
	float estimatedTimeRemaining = timeRemaining;

	if ( lastProgressUpdateTime < currentTime )
	{
		// old update, extrapolate
		float progressRate = currentProgress / timeElapsed;
		float estimatedProgress = progressRate * ( currentTime - startTime );
		float estimatedTotalTime = ( currentTime - startTime ) / estimatedProgress;
		estimatedTimeRemaining = estimatedTotalTime - timeElapsed;
	}

	int secondsRemaining = (int)estimatedTimeRemaining;
	char unlocString[64];

	if ( secondsRemaining > 59 )
	{
		int minutesRemaining = secondsRemaining / 60;
		secondsRemaining %= 60;

		char minutesBuf[16];
		char secondsBuf[16];
		snprintf( minutesBuf, sizeof( minutesBuf ), "%d", minutesRemaining );
		snprintf( secondsBuf, sizeof( secondsBuf ), "%d", secondsRemaining );

		wchar_t unicodeMinutes[16];
		wchar_t unicodeSeconds[16];
		localize()->ConvertANSIToUnicode( minutesBuf, unicodeMinutes, sizeof( unicodeMinutes ) );
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
		return true;
	}

	if ( secondsRemaining == 0 )
		secondsRemaining = timeRemaining > 0.0f ? 1 : 0;
	if ( secondsRemaining <= 0 )
		return false;

	char secondsBuf[16];
	snprintf( secondsBuf, sizeof( secondsBuf ), "%d", secondsRemaining );

	wchar_t unicodeSeconds[16];
	localize()->ConvertANSIToUnicode( secondsBuf, unicodeSeconds, sizeof( unicodeSeconds ) );

	const char* token = "#vgui_TimeLeftSeconds";
	if ( secondsRemaining == 1 )
		token = "#vgui_TimeLeftSecond";

	strcpy( unlocString, token );
	if ( addRemainingSuffix )
		strcat( unlocString, "Remaining" );

	localize()->ConstructString( output, outputBufferSizeInBytes, localize()->Find( unlocString ), 1, unicodeSeconds );
	return true;
}
}
