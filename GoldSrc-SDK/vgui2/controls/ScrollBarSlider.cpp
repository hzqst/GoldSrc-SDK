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

#include "ScrollBarSlider.h"
#include "controls.h"

#include "vgui/IBorder.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

#include <cmath>

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Get the ScrollBarSlider value of the nob.
//-----------------------------------------------------------------------------
int ScrollBarSlider::GetValue()
{
	return _value;
}

//-----------------------------------------------------------------------------
// Purpose: Whether the scroll bar is vertical (true) or not (false)
//-----------------------------------------------------------------------------
bool ScrollBarSlider::IsVertical()
{
	return _vertical;
}

//-----------------------------------------------------------------------------
// Purpose: Get the range values of the ScrollBarSlider
//-----------------------------------------------------------------------------
void ScrollBarSlider::GetRange( int& minimum, int& maximum )
{
	minimum = _range[0];
	maximum = _range[1];
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of lines visible in the window the ScrollBarSlider is attached to
//-----------------------------------------------------------------------------
int ScrollBarSlider::GetRangeWindow()
{
	return _rangeWindow;
}

//-----------------------------------------------------------------------------
// Purpose: Set the number of lines visible in the window the ScrollBarSlider is attached to
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetRangeWindow( int rangeWindow )
{
	_rangeWindow = rangeWindow;
}

//-----------------------------------------------------------------------------
// Purpose: Get the position of the ends of the ScrollBarSlider.
//-----------------------------------------------------------------------------
void ScrollBarSlider::GetNobPos( int& minimum, int& maximum )
{
	minimum = _nobPos[0];
	maximum = _nobPos[1];
}

//-----------------------------------------------------------------------------
// Purpose: Check if the ScrollBarSlider can move through one or more pixels per
//			unit of its range.
//-----------------------------------------------------------------------------
bool ScrollBarSlider::HasFullRange()
{
	int wide, tall;
	GetPaintSize( wide, tall );

	float frangewindow = (float)_rangeWindow;
	float checkAgainst = (float)( _vertical ? tall : wide );
	if ( frangewindow > 0.0f )
		return (float)_buttonOffset + checkAgainst >= frangewindow;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetButtonOffset( int offset )
{
	_buttonOffset = offset;
}

//-----------------------------------------------------------------------------
// The ScrollBarSlider is the scroll bar nob that moves up and down in through a range.
//-----------------------------------------------------------------------------
ScrollBarSlider::ScrollBarSlider( Panel* parent, const char* name, bool vertical ) : Panel( parent, name )
{
	_vertical = vertical;
	_dragging = false;
	_value = 0;
	_range[0] = 0;
	_range[1] = 0;
	_rangeWindow = 0;
	_buttonOffset = 0;
	_ScrollBarSliderBorder = NULL;
	ScrollBarSlider::RecomputeNobPosFromValue();
}

ScrollBarSlider::~ScrollBarSlider()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ScrollBarSlider::Paint()
{
	int wide, tall;
	GetPaintSize( wide, tall );

	int range = _range[1] - _range[0];
	if ( range <= 0 || range <= _rangeWindow )
		return;

	surface()->DrawSetColor( GetFgColor() );

	if ( _vertical )
	{
		// Nob
		surface()->DrawFilledRect( 0, _nobPos[0], wide - 1, _nobPos[1] );
		if ( _ScrollBarSliderBorder )
			_ScrollBarSliderBorder->Paint( 0, _nobPos[0], wide - 1, _nobPos[1] );
	}
	else
	{
		// horizontal nob
		surface()->DrawFilledRect( _nobPos[0], 0, _nobPos[1], tall );
		if ( _ScrollBarSliderBorder )
			_ScrollBarSliderBorder->Paint( _nobPos[0] - 1, 1, _nobPos[1], tall );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ScrollBarSlider::PaintBackground()
{
//	BaseClass::PaintBackground();
	int wide, tall;
	GetPaintSize( wide, tall );

	surface()->DrawSetColor( GetBgColor() );
	surface()->DrawFilledRect( 0, 0, wide - 1, tall - 1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScrollBarSlider::PerformLayout()
{
	RecomputeNobPosFromValue();
	Panel::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Respond to cursor movements, we only care about clicking and dragging
//-----------------------------------------------------------------------------
void ScrollBarSlider::OnCursorMoved( int x, int y )
{
	if ( !_dragging )
		return;

//	input()->GetCursorPos(x, y);
//	ScreenToLocal(x, y);

	int wide, tall;
	GetPaintSize( wide, tall );

	if ( _vertical )
	{
		_nobPos[0] = _nobDragStartPos[0] + ( y - _dragStartPos[1] );
		_nobPos[1] = _nobDragStartPos[1] + ( y - _dragStartPos[1] );

		if ( _nobPos[1] > tall )
		{
			_nobPos[0] = tall - ( _nobPos[1] - _nobPos[0] );
			_nobPos[1] = tall;
			SetValue( _range[1] - _rangeWindow );
		}
	}
	else
	{
		_nobPos[0] = _nobDragStartPos[0] + ( x - _dragStartPos[0] );
		_nobPos[1] = _nobDragStartPos[1] + ( x - _dragStartPos[0] );

		if ( _nobPos[1] > wide )
		{
			_nobPos[0] = wide - ( _nobPos[1] - _nobPos[0] );
			_nobPos[1] = wide;
		}
	}

	if ( _nobPos[0] < 0 )
	{
		_nobPos[1] = _nobPos[1] - _nobPos[0];
		_nobPos[0] = 0;
		SetValue( 0 );
	}

	InvalidateLayout( false, false );
	RecomputeValueFromNobPos();
//	Repaint();
	SendScrollBarSliderMovedMessage();
}

void ScrollBarSlider::OnMousePressed( MouseCode )
{
	int x, y;
	input()->GetCursorPos( x, y );
	Panel::ScreenToLocal( x, y );

	int pos = _vertical ? y : x;
	if ( pos < _nobPos[0] )
	{
		SetValue( GetValue() - _rangeWindow );
	}
	else if ( pos >= _nobPos[1] )
	{
		SetValue( GetValue() + _rangeWindow );
	}
	else
	{
		_dragging = true;
		input()->SetMouseCapture( GetVPanel() );
		_nobDragStartPos[0] = _nobPos[0];
		_nobDragStartPos[1] = _nobPos[1];
		_dragStartPos[0] = x;
		_dragStartPos[1] = y;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Treat double clicks as single clicks
//-----------------------------------------------------------------------------
void ScrollBarSlider::OnMouseDoublePressed( MouseCode code )
{
	OnMousePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: Stop looking for mouse events when mouse is up.
//-----------------------------------------------------------------------------
void ScrollBarSlider::OnMouseReleased( MouseCode )
{
	_dragging = false;
	input()->SetMouseCapture( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScrollBarSlider::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );

	SetFgColor( GetSchemeColor( "ScrollBarSlider/ScrollBarSliderFgColor", scheme ) );
	SetBgColor( GetSchemeColor( "ScrollBarSlider/ScrollBarSliderBgColor", scheme ) );
	_ScrollBarSliderBorder = scheme->GetBorder( "ButtonBorder" );
}

//-----------------------------------------------------------------------------
// Purpose: Given the value of the ScrollBarSlider, adjust the ends of the nob.
//-----------------------------------------------------------------------------
void ScrollBarSlider::RecomputeNobPosFromValue()
{
	int wide, tall;
	GetPaintSize( wide, tall );

	float frange = (float)( _range[1] - _range[0] );
	float frangewindow = (float)_rangeWindow;
	float fvalue = (float)( _value - _range[0] );
	float fper = fvalue / ( frange - frangewindow );
	float ftall = (float)tall - 1.0f;
	float fwide = (float)wide;

	if ( frangewindow > 0.0f )
	{
		if ( frange <= 0.0f )
			frange = 1.0f;

		float flength = _vertical ? ftall : fwide;
		float fminsize = _vertical ? fwide : ftall;

		float fnobsize = fmaxf( fminsize, frangewindow / frange * flength );
		float freepixels = flength - fnobsize;
		float firstpixel = freepixels * fper;

		_nobPos[0] = (int)firstpixel;
		_nobPos[1] = (int)( firstpixel + fnobsize );

		if ( (float)_nobPos[1] > flength )
		{
			_nobPos[0] = (int)freepixels;
			_nobPos[1] = (int)flength;
		}
	}

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Get the ScrollBarSlider value using the location of the nob ends.
//-----------------------------------------------------------------------------
void ScrollBarSlider::RecomputeValueFromNobPos()
{
	int wide, tall;
	GetPaintSize( wide, tall );

	float frangewindow = (float)_rangeWindow;
	float frange = (float)( _range[1] - _range[0] );
	float fvalue = (float)( _value - _range[0] );

	if ( frangewindow > 0.0f )
	{
		if ( frange <= 0.0f )
			frange = 1.0f;

		float flength = _vertical ? (float)tall : (float)wide;
		float fminsize = _vertical ? (float)wide : (float)tall;

		float fnobsize = frangewindow / frange * flength;
		if ( fminsize > fnobsize )
			fnobsize = fminsize;

		float fper = (float)_nobPos[0] / ( flength - fnobsize );
		fvalue = fper * ( frange - frangewindow );
	}

	int maxValue = _range[1] - _rangeWindow;

	if ( fabs( frangewindow + fvalue - (float)_range[1] ) < frange * 0.01f )
	{
		_value = maxValue;
	}
	else
	{
		float fpos = (float)_range[0] + fvalue;
		int value = (int)( fpos + 0.5 );
		_value = ( value < maxValue ) ? value : maxValue;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Inform other watchers that the ScrollBarSlider was moved
//-----------------------------------------------------------------------------
void ScrollBarSlider::SendScrollBarSliderMovedMessage()
{
	// send a changed message
	PostActionSignal( new KeyValues( "ScrollBarSliderMoved", "position", _value ) );
}

//-----------------------------------------------------------------------------
// Purpose: Set the ScrollBarSlider value of the nob.
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetValue( int value )
{
	int oldValue = _value;

	if ( value < _range[0] )
		value = _range[0];
	if ( value > _range[1] - _rangeWindow )
		value = _range[1] - _rangeWindow;

	_value = value;
	RecomputeNobPosFromValue();

	if ( _value != oldValue )
		SendScrollBarSliderMovedMessage();
}

//-----------------------------------------------------------------------------
// Purpose: Set the range of the ScrollBarSlider
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetRange( int minimum, int maximum )
{
	if ( maximum < minimum )
		maximum = minimum;

	_range[0] = minimum;
	_range[1] = maximum;
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Set the size of the ScrollBarSlider nob
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetSize( int width, int height )
{
	Panel::SetSize( width, height );
	RecomputeNobPosFromValue();
}
}
