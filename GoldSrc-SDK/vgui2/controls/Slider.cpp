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

#include "Slider.h"

#include "TextImage.h"
#include "controls.h"
#include "vgui/IBorder.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"

#include <stdio.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Create a slider bar with ticks underneath it
//-----------------------------------------------------------------------------
Slider::Slider( Panel* parent, const char* panelName ) : BaseClass( parent, panelName )
{
	_dragging = false;
	_range[0] = 0;
	_range[1] = 0;
	_value = 0;
	_buttonOffset = 0;
	_sliderBorder = NULL;
	_insetBorder = NULL;
	m_nNumTicks = 10;
	_leftCaption = NULL;
	_rightCaption = NULL;
	_nobSize = (float)GetProportionalScaledValue( 8 );
	RecomputeNobPosFromValue();
	AddActionSignalTarget( this );
}

Slider::~Slider()
{
}

//-----------------------------------------------------------------------------
// Purpose: Return the value of the slider
//-----------------------------------------------------------------------------
int Slider::GetValue()
{
	return _value;
}

//-----------------------------------------------------------------------------
// Purpose: Move the nob on the slider in response to changing its value.
//-----------------------------------------------------------------------------
void Slider::RecomputeNobPosFromValue()
{
	int wide, tall;
	GetPaintSize( wide, tall );
#ifdef _WIN32
	float firstPixel = (float)( _value - _range[0] ) / (float)( _range[1] - _range[0] )
		* ( (float)wide - _nobSize ) + 0.0f;
	float lastPixel = firstPixel + _nobSize;
#else
	long double position = (long double)( _value - _range[0] ) / (long double)( _range[1] - _range[0] )
		* ( (long double)wide - _nobSize ) + 0.0;
	float firstPixel = (float)position;
	float lastPixel = (float)( position + _nobSize );
#endif
	_nobPos[0] = (int)firstPixel;
	_nobPos[1] = (int)lastPixel;
	if ( _nobPos[1] > wide )
	{
		_nobPos[1] = wide;
		_nobPos[0] = wide - (int)_nobSize;
	}
	Repaint();
}

void Slider::RecomputeValueFromNobPos()
{
	int wide, tall;
	GetPaintSize( wide, tall );
#ifdef _WIN32
	float value = (float)_nobPos[0] / ( (float)wide - _nobSize ) * (float)( _range[1] - _range[0] )
		+ (float)_range[0];
	_value = (int)( value + 0.5 );
#else
	double value = (double)( (long double)_nobPos[0] / ( (long double)wide - _nobSize )
		* (long double)( _range[1] - _range[0] ) + (long double)_range[0] + 0.5 );
	_value = (int)value;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Set the range of the slider.
//-----------------------------------------------------------------------------
void Slider::SetRange( int min, int max )
{
	_range[0] = min;
	_range[1] = max < min ? min : max;
	if ( _value < _range[0] )
		_value = _range[0];
	else if ( _value > _range[1] )
		_value = _range[1];
}

//-----------------------------------------------------------------------------
// Purpose: Get the max and min values of the slider
//-----------------------------------------------------------------------------
void Slider::GetRange( int& min, int& max )
{
	min = _range[0];
	max = _range[1];
}

//-----------------------------------------------------------------------------
// Purpose: Get the nob's position (the ends of each side of the nob)
//-----------------------------------------------------------------------------
void Slider::GetNobPos( int& min, int& max )
{
	min = _nobPos[0];
	max = _nobPos[1];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Slider::SetButtonOffset( int offset )
{
	_buttonOffset = offset;
}

void Slider::SetThumbWidth( int width )
{
	_nobSize = (float)width;
}

//-----------------------------------------------------------------------------
// Purpose: Set the number of ticks that appear under the slider.
//-----------------------------------------------------------------------------
void Slider::SetNumTicks( int numTicks )
{
	m_nNumTicks = numTicks;
}

//-----------------------------------------------------------------------------
// Purpose: Get the rectangle to draw the slider track in.
//-----------------------------------------------------------------------------
void Slider::GetTrackRect( int& x, int& y, int& width, int& height )
{
	int wide, tall;
	GetPaintSize( wide, tall );
	x = 0;
	y = GetProportionalScaledValue( 8 );
	float trackWidth = (float)wide - _nobSize;
	width = (int)trackWidth;
	height = GetProportionalScaledValue( 4 );
}

//-----------------------------------------------------------------------------
// Purpose: Send a message to interested parties when the slider moves
//-----------------------------------------------------------------------------
void Slider::SendSliderMovedMessage()
{
	// send a changed message
	PostActionSignal( new KeyValues( "SliderMoved", "position", _value ) );
}

//-----------------------------------------------------------------------------
// Purpose: Set the text labels of the Start and end ticks.
//-----------------------------------------------------------------------------
void Slider::SetTickCaptions( const char* left, const char* right )
{
	if ( left )
	{
		if ( _leftCaption )
			_leftCaption->SetText( left );
		else
			_leftCaption = new TextImage( left );
	}
	if ( right )
	{
		if ( _rightCaption )
			_rightCaption->SetText( right );
		else
			_rightCaption = new TextImage( right );
	}
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Set the text labels of the Start and end ticks.
//-----------------------------------------------------------------------------
void Slider::SetTickCaptions( const wchar_t* left, const wchar_t* right )
{
	if ( left )
	{
		if ( _leftCaption )
			_leftCaption->SetText( left );
		else
			_leftCaption = new TextImage( left );
	}
	if ( right )
	{
		if ( _rightCaption )
			_rightCaption->SetText( right );
		else
			_rightCaption = new TextImage( right );
	}
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Stop dragging when the mouse is released.
//-----------------------------------------------------------------------------
void Slider::OnMouseReleased( MouseCode )
{
	_dragging = false;
	input()->SetMouseCapture( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Respond to mouse presses. Trigger Record staring positon.
//-----------------------------------------------------------------------------
void Slider::OnMousePressed( MouseCode )
{
	if ( !IsEnabled() )
		return;

	int x, y;
//	input()->GetCursorPos(x,y);
	input()->GetCursorPosition( x, y );
	ScreenToLocal( x, y );
	RequestFocus( 0 );
	if ( x >= _nobPos[0] && x < _nobPos[1] )
	{
		// drag the nob
		_dragging = true;
		input()->SetMouseCapture( GetVPanel() );
		_nobDragStartPos[0] = _nobPos[0];
		_nobDragStartPos[1] = _nobPos[1];
		_dragStartPos[0] = x;
		_dragStartPos[1] = y;
	}
	else
	{
		// we clicked elsewhere on the slider; move the nob to that position
		int min, max;
		GetRange( min, max );
		float value = (float)( max - min ) / (float)GetWide() * (float)x;
		SetValue( min + (int)value, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Respond when the cursor is moved in our window if we are clicking
// and dragging.
//-----------------------------------------------------------------------------
void Slider::OnCursorMoved( int x, int y )
{
	if ( !_dragging )
		return;

//	input()->GetCursorPos(x,y);
	input()->GetCursorPosition( x, y );
	ScreenToLocal( x, y );
	int wide, tall;
	GetPaintSize( wide, tall );
	_nobPos[0] = _nobDragStartPos[0] + x - _dragStartPos[0];
	_nobPos[1] = _nobDragStartPos[1] + x - _dragStartPos[0];
	if ( _nobPos[1] > wide )
	{
		_nobPos[0] = wide - ( _nobPos[1] - _nobPos[0] );
		_nobPos[1] = wide;
	}
	if ( _nobPos[0] < 0 )
	{
		_nobPos[1] -= _nobPos[0];
		_nobPos[0] = 0;
	}
	RecomputeValueFromNobPos();
	Repaint();
	SendSliderMovedMessage();
}

//-----------------------------------------------------------------------------
// Purpose: Set the value of the slider to one of the ticks.
//-----------------------------------------------------------------------------
void Slider::SetValue( int value, bool triggerChangeMessage )
{
	int oldValue = _value;
	_value = value < _range[0] ? _range[0] : value;
	if ( _value > _range[1] )
		_value = _range[1];

	RecomputeNobPosFromValue();
	if ( _value != oldValue && triggerChangeMessage )
		SendSliderMovedMessage();
}

//-----------------------------------------------------------------------------
// Purpose: Just handle double presses like mouse presses
//-----------------------------------------------------------------------------
void Slider::OnMouseDoublePressed( MouseCode code )
{
	OnMousePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: Draw the ticks below the slider.
//-----------------------------------------------------------------------------
void Slider::DrawTicks()
{
	int x, y, wide, tall;
	GetTrackRect( x, y, wide, tall );
	// Figure out how to draw the ticks
	GetPaintSize( wide, tall );
	y += (int)_nobSize;
	float leftPixel = _nobSize * 0.5f;
	float pixelsPerTick = ( (float)wide - _nobSize ) / (float)m_nNumTicks;
	int tickHeight = GetProportionalScaledValue( 5 );
	IScheme* panelScheme = scheme()->GetIScheme( GetScheme() );
	int onePixel = GetProportionalScaledValue( 1 );
	int twoPixels = GetProportionalScaledValue( 2 );
	if ( IsEnabled() )
	{
		surface()->DrawSetColor( GetSchemeColor( "SliderTickColor", panelScheme ) ); //vgui::Color( 127, 140, 127, 255 ) );
		for ( int i = 0; i <= m_nNumTicks; i++ )
		{
			float pixel = (float)i * pixelsPerTick + leftPixel;
			surface()->DrawFilledRect( (int)pixel, y, (int)pixel + onePixel, y + tickHeight );
		}
	}
	else
	{
		surface()->DrawSetColor( GetSchemeColor( "DisabledFgColor1", panelScheme ) ); //vgui::Color( 127, 140, 127, 255 ) );
		for ( int i = 0; i <= m_nNumTicks; i++ )
		{
			float pixel = (float)i * pixelsPerTick + leftPixel;
			surface()->DrawFilledRect( (int)pixel + onePixel, y + onePixel, (int)pixel + twoPixels,
				y + onePixel + tickHeight );
		}
		surface()->DrawSetColor( GetSchemeColor( "DisabledFgColor2", panelScheme ) ); //vgui::Color( 127, 140, 127, 255 ) );
		for ( int i = 0; i <= m_nNumTicks; i++ )
		{
			float pixel = (float)i * pixelsPerTick + leftPixel;
			surface()->DrawFilledRect( (int)pixel, y, (int)pixel + onePixel, y + tickHeight );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw Tick labels under the ticks.
//-----------------------------------------------------------------------------
void Slider::DrawTickLabels()
{
	int x, y, wide, tall;
	GetTrackRect( x, y, wide, tall );
	// Figure out how to draw the ticks
	GetPaintSize( wide, tall );
	y += GetProportionalScaledValue( 12 );
	IScheme* panelScheme = scheme()->GetIScheme( GetScheme() );
	surface()->DrawSetTextColor( GetSchemeColor( IsEnabled() ? "SliderTickColor" : "DisabledFgColor1", panelScheme ) );
	if ( _leftCaption )
	{
		_leftCaption->SetPos( 0, y );
		_leftCaption->SetColor( GetSchemeColor( IsEnabled() ? "SliderTickColor" : "DisabledFgColor1", panelScheme ) );
		_leftCaption->Paint();
	}
	if ( _rightCaption )
	{
		int textWide, textTall;
		_rightCaption->GetSize( textWide, textTall );
		_rightCaption->SetPos( wide - textWide, y );
		_rightCaption->SetColor( GetSchemeColor( IsEnabled() ? "SliderTickColor" : "DisabledFgColor1", panelScheme ) );
		_rightCaption->Paint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw the nob part of the slider.
//-----------------------------------------------------------------------------
void Slider::DrawNob()
{
	int x, y, wide, tall;
	GetTrackRect( x, y, wide, tall );
	surface()->DrawSetColor( GetFgColor() );
	int halfHeight = GetProportionalScaledValue( 16 ) / 2;
	surface()->DrawFilledRect( _nobPos[0], y + tall / 2 - halfHeight, _nobPos[1], y + tall / 2 + halfHeight );
	if ( _sliderBorder )
		_sliderBorder->Paint( _nobPos[0], y + tall / 2 - halfHeight, _nobPos[1], y + tall / 2 + halfHeight );
}

//-----------------------------------------------------------------------------
// Purpose: Draw everything on screen
//-----------------------------------------------------------------------------
void Slider::Paint()
{
	DrawTicks();
	DrawTickLabels();
	DrawNob();
}

//-----------------------------------------------------------------------------
// Purpose: Set the size of the slider bar.
//			Warning less than 30 pixels tall and everything probably won't fit.
//-----------------------------------------------------------------------------
void Slider::OnSizeChanged( int wide, int tall )
{
	BaseClass::OnSizeChanged( wide, tall );
	RecomputeNobPosFromValue();
}

//-----------------------------------------------------------------------------
// Purpose: Layout the slider before drawing it on screen.
//-----------------------------------------------------------------------------
void Slider::PerformLayout()
{
	BaseClass::PerformLayout();
	RecomputeNobPosFromValue();
	if ( _leftCaption )
		_leftCaption->ResizeImageToContent();
	if ( _rightCaption )
		_rightCaption->ResizeImageToContent();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Slider::ApplySchemeSettings( IScheme* panelScheme )
{
	BaseClass::ApplySchemeSettings( panelScheme );
	SetFgColor( GetSchemeColor( "Slider/SliderFgColor", panelScheme ) );
	// this line is useful for debugging
	//SetBgColor(GetSchemeColor("Slider/SliderBgColor"));
	_sliderBorder = panelScheme->GetBorder( "ButtonBorder" );
	_insetBorder = panelScheme->GetBorder( "ButtonDepressedBorder" );
	if ( _leftCaption )
		_leftCaption->SetFont( panelScheme->GetFontEx( "DefaultVerySmall", IsProportional(), IsHDProportional() ) );
	if ( _rightCaption )
		_rightCaption->SetFont( panelScheme->GetFontEx( "DefaultVerySmall", IsProportional(), IsHDProportional() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Slider::GetSettings( KeyValues* resourceData )
{
	BaseClass::GetSettings( resourceData );
	char text[256];
	if ( _leftCaption )
	{
		_leftCaption->GetText( text, sizeof( text ) - 1 );
		resourceData->SetString( "leftText", text );
	}
	if ( _rightCaption )
	{
		_rightCaption->GetText( text, sizeof( text ) - 1 );
		resourceData->SetString( "rightText", text );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Slider::ApplySettings( KeyValues* resourceData )
{
	BaseClass::ApplySettings( resourceData );
	const char* left = resourceData->GetString( "leftText", NULL );
	const char* right = resourceData->GetString( "rightText", NULL );
	int thumbWidth = resourceData->GetInt( "thumbwidth", 0 );
	if ( thumbWidth )
		SetThumbWidth( thumbWidth );
	SetTickCaptions( left, right );
}

const char* Slider::GetDescription()
{
	static char description[1024];
#ifdef _WIN32
	_snprintf( description, sizeof( description ), "%s, string leftText, string rightText", BaseClass::GetDescription() );
#else
	snprintf( description, sizeof( description ), "%s, string leftText, string rightText", BaseClass::GetDescription() );
#endif
	return description;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the slider track
//-----------------------------------------------------------------------------
void Slider::PaintBackground()
{
	BaseClass::PaintBackground();
	int x, y, wide, tall;
	GetTrackRect( x, y, wide, tall );
	IScheme* panelScheme = scheme()->GetIScheme( GetScheme() );
	surface()->DrawSetColor( GetSchemeColor( "SliderTrackColor", panelScheme ) );
	surface()->DrawFilledRect( x, y, x + wide, y + tall );
	if ( _insetBorder )
		_insetBorder->Paint( x, y, x + wide, y + tall );
}

//-----------------------------------------------------------------------------
// Purpose: Handle key presses
//-----------------------------------------------------------------------------
void Slider::OnKeyCodeTyped( KeyCode code )
{
	int value = GetValue();
	int min, max;
	GetRange( min, max );
	float range = (float)max - (float)min;
	float step = 0.02f * range;
	switch ( code )
	{
	case KEY_ENTER:
		break;
	case KEY_HOME:
		SetValue( min, true );
		break;
	case KEY_END:
		SetValue( max, true );
		break;
	case KEY_PAGEUP:
		{
			float page = range / (float)m_nNumTicks;
			SetValue( value + (int)page, true );
			break;
		}
	case KEY_PAGEDOWN:
		{
			float page = range / (float)m_nNumTicks;
			SetValue( value - (int)page, true );
			break;
		}
	case KEY_LEFT:
		SetValue( value - (int)step, true );
		break;
	case KEY_RIGHT:
		SetValue( value + (int)step, true );
		break;
	default:
		BaseClass::OnKeyCodeTyped( code );
		break;
	}
}
