#include "VGUI_ScrollBar.h"
#include "VGUI_Button.h"
#include "VGUI_Slider.h"
#include "VGUI_ActionSignal.h"
#include "VGUI_IntChangeSignal.h"

#if VGUI_ABI_COMPAT_MODE == 1
#include <type_traits>
#endif

namespace
{

using namespace vgui;

const int BUTTON_COUNT = 2;

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

#if VGUI_ABI_COMPAT_MODE == 1
static_assert( std::is_trivially_destructible<FooDefaultButtonSignal>::value,
	"VGUI_ABI_COMPAT_MODE == 1 teardown releases FooDefaultButtonSignal storage directly" );
static_assert( std::is_trivially_destructible<FooDefaultScrollBarIntChangeSignal>::value,
	"VGUI_ABI_COMPAT_MODE == 1 teardown releases FooDefaultScrollBarIntChangeSignal storage directly" );
#endif

void FooDefaultButtonSignal::actionPerformed( Panel* )
{
	_scrollBar->doButtonPressed( _buttonIndex );
}

void FooDefaultScrollBarIntChangeSignal::intChanged( int, Panel* )
{
	_scrollBar->fireIntChangeSignal();
}

// The new part is added before the old one goes, so it may have been a child of
// the old one. An old part that is not a child of the bar, as in a copied
// ScrollBar, is not touched.
void RemovePart( ScrollBar* scrollBar, Panel* part )
{
	if ( part == NULL || part->getParent() != scrollBar )
	{
		return;
	}

	scrollBar->removeChild( part );
#if VGUI_ABI_COMPAT_MODE != 1
	// The part's relay signal, which calls the bar, goes with it.
	delete part;
#endif
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
	if ( index < 0 || index >= BUTTON_COUNT )
	{
		return NULL;
	}

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

		for ( int i = 0; i < BUTTON_COUNT; ++i )
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

// With VGUI_ABI_COMPAT_MODE != 1 the bar owns its parts and deletes a replaced
// one. Setting the current part again would add a second relay, so it does
// nothing.
void ScrollBar::setSlider( Slider* slider )
{
	if ( slider == NULL || slider == _slider )
	{
		return;
	}

	Slider* old = _slider;
	_slider = slider;
	addChild( _slider );
	_slider->addIntChangeSignal( new FooDefaultScrollBarIntChangeSignal( this ) );
	RemovePart( this, old );
	validate();
}

void ScrollBar::setButton( Button* button, int index )
{
	if ( index < 0 || index >= BUTTON_COUNT || button == NULL || button == _button[0] || button == _button[1] )
	{
		return;
	}

	Button* old = _button[index];
	_button[index] = button;
	addChild( button );
	button->addActionSignal( new FooDefaultButtonSignal( this, index ) );
	RemovePart( this, old );
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
