#ifndef VGUI_SCROLLBAR_H
#define VGUI_SCROLLBAR_H

#include "VGUI.h"
#include "VGUI_Panel.h"
#include "VGUI_Dar.h"

namespace vgui
{

class Button;
class Slider;
class IntChangeSignal;

class VGUIAPI ScrollBar : public Panel
{
public:
	ScrollBar( int x, int y, int wide, int tall, bool vertical );

	virtual void setValue( int value );
	virtual int getValue();
	virtual void addIntChangeSignal( IntChangeSignal* s );

	virtual void setRange( int min, int max );
	virtual void setRangeWindow( int rangeWindow );
	virtual void setRangeWindowEnabled( bool state );
	virtual void setSize( int wide, int tall ) override;
	virtual bool isVertical();
	virtual bool hasFullRange();

	virtual void setButton( Button* button, int index );
	virtual Button* getButton( int index );
	virtual void setSlider( Slider* slider );
	virtual Slider* getSlider();
	virtual void doButtonPressed( int buttonIndex );
	virtual void setButtonPressedScrollValue( int value );
	virtual void validate();
	virtual void fireIntChangeSignal();

protected:
	virtual void performLayout() override;

	Button*					_button[2];
	Slider*					_slider;

	// Owns its signals with VGUI_ABI_COMPAT_MODE != 1.
	OwningDar<IntChangeSignal*>	_intChangeSignalDar;
	int						_buttonPressedScrollValue;

public:
	ScrollBar& operator=( const ScrollBar& ) = default;
};

}

#endif
