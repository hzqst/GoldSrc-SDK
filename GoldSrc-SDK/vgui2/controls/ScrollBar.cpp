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

#include "ScrollBar.h"
#include "Button.h"
#include "ScrollBarSlider.h"
#include "controls.h"

#include "../../engine/common.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

#define SCROLL_BAR_DELAY 400
#define SCROLL_BAR_SPEED 50

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Set the value of the scroll bar slider.
//-----------------------------------------------------------------------------
void ScrollBar::SetValue( int value )
{
	_slider->SetValue( value );
}

//-----------------------------------------------------------------------------
// Purpose: Get the value of the scroll bar slider.
//-----------------------------------------------------------------------------
int ScrollBar::GetValue()
{
	return _slider->GetValue();
}

//-----------------------------------------------------------------------------
// Purpose: Set the range of the scroll bar slider.
// This the range of numbers the slider can scroll through.
//-----------------------------------------------------------------------------
void ScrollBar::SetRange( int minimum, int maximum )
{
	_slider->SetRange( minimum, maximum );
}

//-----------------------------------------------------------------------------
// Purpose: Gets the range of the scroll bar slider.
// This the range of numbers the slider can scroll through.
//-----------------------------------------------------------------------------
void ScrollBar::GetRange( int& minimum, int& maximum )
{
	_slider->GetRange( minimum, maximum );
}

//-----------------------------------------------------------------------------
// Purpose: Set the range of the rangewindow. This is how many 
// lines are displayed at one time 
// in the window the scroll bar is attached to.
// This also controls the size of the slider, its size is proportional
// to the number of lines displayed / total number of lines.
//-----------------------------------------------------------------------------
void ScrollBar::SetRangeWindow( int window )
{
	_slider->SetRangeWindow( window );
}

//-----------------------------------------------------------------------------
// Purpose: Get the range of the rangewindow. This is how many 
// lines are displayed at one time 
// in the window the scroll bar is attached to.
// This also controls the size of the slider, its size is proportional
// to the number of lines displayed / total number of lines.
//-----------------------------------------------------------------------------
int ScrollBar::GetRangeWindow()
{
	return _slider->GetRangeWindow();
}

//-----------------------------------------------------------------------------
// Purpose: Check if the scrollbar is vertical (true) or horizontal (false)
//-----------------------------------------------------------------------------
bool ScrollBar::IsVertical()
{
	return _slider->IsVertical();
}

//-----------------------------------------------------------------------------
// Purpose: Check if the the scrollbar slider has full range.
// Normally if you have a scroll bar and the range goes from a to b and
// the slider is sized to c, the range will go from a to b-c.
// This makes it so the slider goes from a to b fully.
//-----------------------------------------------------------------------------
bool ScrollBar::HasFullRange()
{
	return _slider->HasFullRange();
}

//-----------------------------------------------------------------------------
// Purpose: Return the indexed scroll bar button
//-----------------------------------------------------------------------------
Button* ScrollBar::GetButton( int index )
{
	return _button[index];
}

//-----------------------------------------------------------------------------
// Purpose: Return a pointer to the slider.
//-----------------------------------------------------------------------------
ScrollBarSlider* ScrollBar::GetSlider()
{
	return _slider;
}

//-----------------------------------------------------------------------------
// Purpose: Set how far the scroll bar slider moves when a scroll bar button is
// pressed.
//-----------------------------------------------------------------------------
void ScrollBar::SetButtonPressedScrollValue( int value )
{
	_buttonPressedScrollValue = value;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the Slider is dragged by the user
// Input  : value - 
//-----------------------------------------------------------------------------
void ScrollBar::OnSliderMoved( int position )
{
	SendSliderMoveMessage( position );
}

//-----------------------------------------------------------------------------
// Purpose: Setup the indexed scroll bar button with the input params.
//-----------------------------------------------------------------------------
//LEAK: new and old slider will leak
void ScrollBar::SetButton( Button* button, int index )
{
	if ( _button[index] )
		_button[index]->SetParent( (Panel*)NULL );

	_button[index] = button;
	button->SetParent( this );
	_button[index]->AddActionSignalTarget( this );
	_button[index]->SetCommand( new KeyValues( "ScrollButtonPressed", "index", index ) );

	Validate();
}

//-----------------------------------------------------------------------------
// Purpose: Set up the slider.
//-----------------------------------------------------------------------------
//LEAK: new and old slider will leak
void ScrollBar::SetSlider( ScrollBarSlider* slider )
{
	if ( _slider )
		_slider->SetParent( (Panel*)NULL );

	_slider = slider;
	slider->AddActionSignalTarget( this );
	_slider->SetParent( this );

	Validate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ScrollBar::Validate()
{
	if ( !_slider )
		return;

	int buttonOffset = 0;
	for ( int i = 0; i < 2; i++ )
	{
		if ( _button[i] && _button[i]->IsVisible() )
		{
			if ( _slider->IsVertical() )
				buttonOffset += _button[i]->GetTall();
			else
				buttonOffset += _button[i]->GetWide();
		}
	}

	_slider->SetButtonOffset( buttonOffset );
}

//-----------------------------------------------------------------------------
// Purpose: Set the slider's Paint border enabled.
//-----------------------------------------------------------------------------
void ScrollBar::SetPaintBorderEnabled( bool state )
{
	if ( _slider )
		_slider->SetPaintBorderEnabled( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScrollBar::SetPaintBackgroundEnabled( bool state )
{
	if ( _slider )
		_slider->SetPaintBackgroundEnabled( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScrollBar::SetPaintEnabled( bool state )
{
	if ( _slider )
		_slider->SetPaintEnabled( state );
}

//-----------------------------------------------------------------------------
// Purpose: Trigger layout changes when the window size is changed.
//-----------------------------------------------------------------------------
void ScrollBar::OnSizeChanged( int, int )
{
	InvalidateLayout( false, false );
	_slider->InvalidateLayout( false, false );
}

void ScrollBar::PerformLayout()
{
	if ( _slider )
	{
		int wide, tall;
		GetPaintSize( wide, tall );

		if ( _slider->IsVertical() )
		{
			_slider->Panel::SetBounds( 0, wide, wide, tall - wide * 2 + 1 );
			_button[0]->Panel::SetBounds( 0, 0, wide - 1, wide );
			_button[1]->Panel::SetBounds( 0, tall - wide, wide - 1, wide );
		}
		else
		{
			_slider->Panel::SetBounds( tall, -1, wide - tall * 2 + 1, tall + 1 );
			_button[0]->Panel::SetBounds( 0, 0, tall, tall );
			_button[1]->Panel::SetBounds( wide - tall, 0, tall, tall );
		}

		_slider->InvalidateLayout( false, false );
	}

	Panel::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Scrolls in response to clicking and holding on up or down arrow
// The idea is to have the slider move one step then delay a bit and then
// the bar starts moving at normal speed. This gives a stepping feeling
// to just clicking an arrow once.
//-----------------------------------------------------------------------------
void ScrollBar::OnMouseFocusTicked()
{
	int direction;
	if ( _button[0]->IsDepressed() )
	{
		direction = -1;
	}
	else if ( _button[1]->IsDepressed() )
	{
		direction = 1;
	}
	else
	{
		// if neither button is down keep delay at max
		_scrollDelay = system()->GetTimeMillis() + SCROLL_BAR_DELAY;
		_respond = true;
		return;
	}

	RespondToScrollArrow( direction );

	if ( _scrollDelay >= system()->GetTimeMillis() )
	{
		_respond = false;
	}
	else
	{
		_scrollDelay = system()->GetTimeMillis() + SCROLL_BAR_SPEED;
		_respond = true;
	}
}

void ScrollBar::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	SetBgColor( GetSchemeColor( "ScrollBar/BgColor", scheme ) );
}

//-----------------------------------------------------------------------------
// Purpose: Send a message when the slider is moved.
// Input  : value - 
//-----------------------------------------------------------------------------
void ScrollBar::SendSliderMoveMessage( int position )
{
	PostActionSignal( new KeyValues( "ScrollBarSliderMoved", "position", position ) );
}

//-----------------------------------------------------------------------------
// Purpose: move scroll bar in response to the first button
// Input: button and direction to move scroll bar when that button is pressed
//			direction can only by +/- 1 
// Output: whether button is down or not
//-----------------------------------------------------------------------------
void ScrollBar::RespondToScrollArrow( int direction )
{
	if ( !_respond )
		return;

	int newValue = _slider->GetValue() + _buttonPressedScrollValue * direction;
	_slider->SetValue( newValue );
	SendSliderMoveMessage( newValue );
}
}

namespace
{
class ScrollBarButton : public vgui2::Button
{
public:
	ScrollBarButton( vgui2::Panel* parent, const char* panelName, const char* imageName );
	~ScrollBarButton() override;
	void OnMouseFocusTicked() override;

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void OnMousePressed( vgui2::MouseCode code ) override;
	void OnMouseReleased( vgui2::MouseCode code ) override;

private:
	vgui2::IImage* m_pImage;
};

ScrollBarButton::ScrollBarButton( vgui2::Panel* parent, const char* panelName, const char* imageName )
	: vgui2::Button( parent, panelName, "" )
{
	SetButtonActivationType( vgui2::Button::ACTIVATE_ONPRESSED );
	m_pImage = vgui2::scheme()->GetImage( imageName, true );
}

ScrollBarButton::~ScrollBarButton()
{
}

void ScrollBarButton::OnMouseFocusTicked()
{
	CallParentFunction( new KeyValues( "MouseFocusTicked" ) );
}

void ScrollBarButton::OnMousePressed( vgui2::MouseCode code )
{
	if ( !IsEnabled() || !IsMouseClickEnabled( code ) || !IsUseCaptureMouseEnabled() )
		return;

	SetSelected( true );
	Repaint();
	vgui2::input()->SetMouseCapture( GetVPanel() );
}

void ScrollBarButton::OnMouseReleased( vgui2::MouseCode code )
{
	if ( !IsEnabled() || !IsMouseClickEnabled( code ) || !IsUseCaptureMouseEnabled() )
		return;

	SetSelected( false );
	Repaint();
	vgui2::input()->SetMouseCapture( 0 );
}

void ScrollBarButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Button::ApplySchemeSettings( scheme );

	SetDefaultBorder( scheme->GetBorder( "ScrollBarButtonBorder" ) );
	SetDepressedBorder( scheme->GetBorder( "ScrollBarButtonDepressedBorder" ) );

	Color bgColor = GetBgColor();
	SetArmedColor( GetFgColor(), bgColor );
	bgColor = GetBgColor();
	SetDepressedColor( GetFgColor(), bgColor );
	bgColor = GetBgColor();
	SetDefaultColor( GetSchemeColor( "DimBaseText", scheme ), bgColor );

	SetContentAlignment( a_center );
	if ( m_pImage )
		AddImage( m_pImage, 0 );
}
}

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ScrollBar::ScrollBar( Panel* parent, const char* panelName, bool vertical ) : Panel( parent, panelName )
{
	_slider = NULL;
	_button[0] = NULL;
	_button[1] = NULL;
	_scrollDelay = SCROLL_BAR_DELAY;
	_respond = true;

	if ( vertical )
	{
		// FIXME: proportional changes needed???
		SetSlider( new ScrollBarSlider( NULL, NULL, true ) );
		SetButton( new ScrollBarButton( NULL, NULL, "resource/icon_up" ), 0 );
		SetButton( new ScrollBarButton( NULL, NULL, "resource/icon_down" ), 1 );
		_button[0]->SetTextInset( GetProportionalScaledValue( -8 ), 0 );
		_button[1]->SetTextInset( GetProportionalScaledValue( -8 ), 0 );
	}
	else
	{
		SetSlider( new ScrollBarSlider( NULL, NULL, false ) );
		SetButton( new ScrollBarButton( NULL, NULL, "resource/icon_left" ), 0 );
		SetButton( new ScrollBarButton( NULL, NULL, "resource/icon_right" ), 1 );
	}

	Panel::SetPaintBorderEnabled( true );
	Panel::SetPaintBackgroundEnabled( false );
	Panel::SetPaintEnabled( true );
	_buttonPressedScrollValue = 20;

	Validate();
}

ScrollBar::~ScrollBar()
{
}
}
