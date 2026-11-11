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

#pragma once

#include "Panel.h"

namespace vgui2
{
class Button;
class ScrollBarSlider;

class ScrollBar : public Panel
{
	DECLARE_CLASS_SIMPLE( ScrollBar, Panel );

public:
	ScrollBar( Panel* parent, const char* name, bool vertical );
	~ScrollBar() override;

	// Set the value of the scroll bar slider.
	virtual void SetValue( int value );

	// Get the value of the scroll bar slider.
	virtual int GetValue();

	// Set the rangeof numbers the slider can scroll through
	virtual void SetRange( int minimum, int maximum );
	virtual void GetRange( int& minimum, int& maximum );

	// Set how many lines are displayed at one time 
	// in the window the scroll bar is attached to.
	virtual void SetRangeWindow( int window );

	// Get how many lines are displayed at one time 
	// in the window the scroll bar is attached to.
	virtual int GetRangeWindow();

	// Check if the scrollbar is vertical or horizontal
	virtual bool IsVertical();

	// Purpose: Check if the slider can move through one or more pixels per
	// unit of its range.
	virtual bool HasFullRange();

	// Setup the indexed scroll bar button with the input params.
	virtual void SetButton( Button* button, int index );

	// Return the indexed scroll bar button
	virtual Button* GetButton( int index );

	// Set up the slider.
	virtual void SetSlider( ScrollBarSlider* slider );

	// Return a pointer to the slider.
	virtual ScrollBarSlider* GetSlider();

	// Set how far the scroll bar slider moves 
	// when a scroll bar button is pressed
	virtual void SetButtonPressedScrollValue( int value );

	virtual void Validate();

	// Update and look for clicks when mouse is in the scroll bar window.
	void OnMouseFocusTicked() override;

	// Set the slider's Paint border enabled.
	void SetPaintBorderEnabled( bool state ) override;

	// Set the slider's Paint background enabled.
	void SetPaintBackgroundEnabled( bool state ) override;

	// Set the slider's Paint enabled.
	void SetPaintEnabled( bool state ) override;

protected:
	void PerformLayout() override;
	virtual void SendSliderMoveMessage( int position );
	void ApplySchemeSettings( IScheme* scheme ) override;
	void OnSizeChanged( int width, int height ) override;

	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );
	virtual void RespondToScrollArrow( int direction );

private:
	Button* _button[2];
	ScrollBarSlider* _slider;
	int _buttonPressedScrollValue;
	int _scrollDelay; // used to control delays in scrolling
	bool _respond;
};
}
