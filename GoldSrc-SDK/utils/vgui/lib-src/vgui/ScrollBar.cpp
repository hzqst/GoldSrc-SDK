#include "VGUI_ScrollBar.h"
#include "VGUI_Button.h"
#include "VGUI_Slider.h"
#include "VGUI_ActionSignal.h"
#include "VGUI_IntChangeSignal.h"

namespace
{

using namespace vgui;

class FooDefaultButtonSignal : public ActionSignal
{
public:
	FooDefaultButtonSignal( ScrollBar* scrollBar, int buttonIndex )
	{
		_scrollBar = scrollBar;
		_buttonIndex = buttonIndex;
	}

	virtual void actionPerformed( Panel* panel ) override;

protected:
	ScrollBar*	_scrollBar;
	int			_buttonIndex;
};

class FooDefaultScrollBarIntChangeSignal : public IntChangeSignal
{
public:
	FooDefaultScrollBarIntChangeSignal( ScrollBar* scrollBar )
	{
		_scrollBar = scrollBar;
	}

	virtual void intChanged( int value, Panel* panel ) override;

protected:
	ScrollBar*	_scrollBar;
};

void FooDefaultButtonSignal::actionPerformed( Panel* )
{
	_scrollBar->doButtonPressed( _buttonIndex );
}

void FooDefaultScrollBarIntChangeSignal::intChanged( int, Panel* )
{
	_scrollBar->fireIntChangeSignal();
}

}

namespace vgui
{

ScrollBar::ScrollBar( int x, int y, int wide, int tall, bool vertical ) : Panel( x, y, wide, tall )
{
	_slider = NULL;
	_button[0] = NULL;
	_button[1] = NULL;

	if ( vertical )
	{
		setSlider( new Slider( 0, wide - 1, wide, ( tall - 2 * wide ) + 2, true ) );
		setButton( new Button( "", 0, 0, wide, wide ), 0 );
		setButton( new Button( "", 0, tall - wide, wide, wide ), 1 );
	}
	else
	{
		setSlider( new Slider( tall, 0, wide - 2 * tall, tall, false ) );
		setButton( new Button( "", 0, 0, tall + 1, tall + 1 ), 0 );
		setButton( new Button( "", wide - tall, 0, tall + 1, tall + 1 ), 1 );
	}

	setPaintBorderEnabled( true );
	setPaintBackgroundEnabled( true );
	setPaintEnabled( true );
	setButtonPressedScrollValue( 15 );
	validate();
}

void ScrollBar::setSize( int wide, int tall )
{
	Panel::setSize( wide, tall );
	if ( _slider == NULL || _button[0] == NULL || _button[1] == NULL )
	{
		return;
	}

	getPaintSize( wide, tall );
	if ( _slider->isVertical() )
	{
		_slider->setBounds( 0, wide, wide, tall - 2 * wide );
		_button[0]->setBounds( 0, 0, wide, wide );
		_button[1]->setBounds( 0, tall - wide, wide, wide );
	}
	else
	{
		_slider->setBounds( tall, 0, wide - 2 * tall, tall );
		_button[0]->setBounds( 0, 0, tall, tall );
		_button[1]->setBounds( wide - tall, 0, tall, tall );
	}
}

void ScrollBar::performLayout()
{
}

void ScrollBar::setValue( int value )
{
	_slider->setValue( value );
}

int ScrollBar::getValue()
{
	return _slider->getValue();
}

void ScrollBar::setRange( int min, int max )
{
	_slider->setRange( min, max );
}

bool ScrollBar::isVertical()
{
	return _slider->isVertical();
}

bool ScrollBar::hasFullRange()
{
	return _slider->hasFullRange();
}

Button* ScrollBar::getButton( int index )
{
	return _button[index];
}

Slider* ScrollBar::getSlider()
{
	return _slider;
}

void ScrollBar::setButtonPressedScrollValue( int value )
{
	_buttonPressedScrollValue = value;
}

void ScrollBar::setRangeWindow( int rangeWindow )
{
	_slider->setRangeWindow( rangeWindow );
}

void ScrollBar::setRangeWindowEnabled( bool state )
{
	_slider->setRangeWindowEnabled( state );
}

void ScrollBar::validate()
{
	if ( _slider != NULL )
	{
		int buttonOffset = 0;

		for ( int i = 0; i < 2; ++i )
		{
			if ( _button[i] != NULL && _button[i]->isVisible() )
			{
				if ( _slider->isVertical() )
				{
					buttonOffset += _button[i]->getTall();
				}
				else
				{
					buttonOffset += _button[i]->getWide();
				}
			}
		}
		_slider->setButtonOffset( buttonOffset );
	}

	int wide;
	int tall;
	getSize( wide, tall );
	setSize( wide, tall );
}

void ScrollBar::setSlider( Slider* slider )
{
	if ( _slider != NULL )
	{
		removeChild( _slider );
	}

	_slider = slider;
	addChild( _slider );
	_slider->addIntChangeSignal( new FooDefaultScrollBarIntChangeSignal( this ) );
	validate();
}

void ScrollBar::setButton( Button* button, int index )
{
	if ( _button[index] != NULL )
	{
		removeChild( _button[index] );
	}

	_button[index] = button;
	addChild( button );
	button->addActionSignal( new FooDefaultButtonSignal( this, index ) );
	validate();
}

void ScrollBar::doButtonPressed( int buttonIndex )
{
	int value = _slider->getValue();

	if ( buttonIndex != 0 )
	{
		value = _buttonPressedScrollValue + value;
	}
	else
	{
		value = value - _buttonPressedScrollValue;
	}

	_slider->setValue( value );
}

// setSlider already relays the slider's changes to fireIntChangeSignal; one
// more relay per signal would call every signal once per relay.
void ScrollBar::addIntChangeSignal( IntChangeSignal* s )
{
	_intChangeSignalDar.putElement( s );
}

void ScrollBar::fireIntChangeSignal()
{
	for ( int i = 0; i < _intChangeSignalDar.getCount(); ++i )
	{
		_intChangeSignalDar[i]->intChanged( _slider->getValue(), this );
	}
}

}
