#include "VGUI_Slider.h"
#include "VGUI_App.h"
#include "VGUI_InputSignal.h"
#include "VGUI_IntChangeSignal.h"
#include "VGUI_KeyCode.h"
#include "VGUI_Panel.h"

#if VGUI_ABI_COMPAT_MODE == 1
#include <type_traits>
#endif

namespace
{

using namespace vgui;

class FooDefaultSliderSignal : public InputSignal
{
public:
	FooDefaultSliderSignal( Slider* slider )
	{
		_slider = slider;
	}

	virtual void cursorMoved( int x, int y, Panel* panel ) override
	{
		_slider->privateCursorMoved( x, y, panel );
	}

	virtual void mousePressed( MouseCode code, Panel* panel ) override
	{
		_slider->privateMousePressed( code, panel );
	}

	virtual void cursorEntered( Panel* panel ) override;
	virtual void cursorExited( Panel* panel ) override;
	virtual void mouseDoublePressed( MouseCode code, Panel* panel ) override;
	virtual void mouseReleased( MouseCode code, Panel* panel ) override;
	virtual void mouseWheeled( int delta, Panel* panel ) override;

	virtual void keyPressed( KeyCode code, Panel* panel ) override;
	virtual void keyTyped( KeyCode code, Panel* panel ) override;
	virtual void keyReleased( KeyCode code, Panel* panel ) override;
	virtual void keyFocusTicked( Panel* panel ) override;

protected:
	Slider*	_slider;
};

#if VGUI_ABI_COMPAT_MODE == 1
static_assert( std::is_trivially_destructible<FooDefaultSliderSignal>::value,
	"VGUI_ABI_COMPAT_MODE == 1 teardown releases FooDefaultSliderSignal storage directly" );
#endif

void FooDefaultSliderSignal::cursorEntered( Panel* )
{
}

void FooDefaultSliderSignal::cursorExited( Panel* )
{
}

void FooDefaultSliderSignal::mouseDoublePressed( MouseCode, Panel* )
{
}

void FooDefaultSliderSignal::mouseReleased( MouseCode code, Panel* panel )
{
	_slider->privateMouseReleased( code, panel );
}

void FooDefaultSliderSignal::mouseWheeled( int, Panel* )
{
}

void FooDefaultSliderSignal::keyPressed( KeyCode, Panel* )
{
}

void FooDefaultSliderSignal::keyTyped( KeyCode, Panel* )
{
}

void FooDefaultSliderSignal::keyReleased( KeyCode, Panel* )
{
}

void FooDefaultSliderSignal::keyFocusTicked( Panel* )
{
}

}

namespace vgui
{

Slider::Slider( int x, int y, int wide, int tall, bool vertical )
	: Panel( x, y, wide, tall ),
	_intChangeSignalDar( 4 )
{
	_vertical = vertical;
	_value = 0;
	_rangeWindow = 0;
	_dragging = false;
	_range[0] = 0;
	_range[1] = 299;
	_rangeWindowEnabled = false;
	_buttonOffset = 0;
	recomputeNobPosFromValue();
	addInputSignal( new FooDefaultSliderSignal( this ) );
}

void Slider::setSize( int wide, int tall )
{
	Panel::setSize( wide, tall );
	recomputeNobPosFromValue();
}

void Slider::setValue( int value )
{
	int oldValue = _value;

	if ( value < _range[0] )
	{
		value = _range[0];
	}

	if ( value > _range[1] )
	{
		value = _range[1];
	}

	_value = value;
	recomputeNobPosFromValue();

	if ( _value != oldValue )
	{
		fireIntChangeSignal();
	}
}

int Slider::getValue()
{
	return _value;
}

void Slider::setRange( int min, int max )
{
	if ( max < min )
	{
		max = min;
	}
	_range[0] = min;
	_range[1] = max;
}

void Slider::getRange( int& min, int& max )
{
	min = _range[0];
	max = _range[1];
}

void Slider::setRangeWindow( int rangeWindow )
{
	_rangeWindow = rangeWindow;
}

void Slider::setRangeWindowEnabled( bool state )
{
	_rangeWindowEnabled = state;
}

void Slider::setButtonOffset( int buttonOffset )
{
	_buttonOffset = buttonOffset;
}

bool Slider::isVertical()
{
	return _vertical;
}

void Slider::getNobPos( int& min, int& max )
{
	min = _nobPos[0];
	max = _nobPos[1];
}

void Slider::addIntChangeSignal( IntChangeSignal* s )
{
	_intChangeSignalDar.putElement( s );
}

void Slider::fireIntChangeSignal()
{
	for ( int i = 0; i < _intChangeSignalDar.getCount(); ++i )
	{
		IntChangeSignal* signal = _intChangeSignalDar[i];
		signal->intChanged( getValue(), this );
	}
}

void Slider::privateMouseReleased( MouseCode, Panel* )
{
	_dragging = false;
	getApp()->setMouseCapture( NULL );
}

bool Slider::hasFullRange()
{
	int wide;
	int tall;
	getPaintSize( wide, tall );
	const float available = (float)( _vertical ? tall : wide );
	double rangeWindow = _rangeWindow;
	if ( rangeWindow < 0.0 )
	{
		rangeWindow = 0.0;
	}
	if ( !_rangeWindowEnabled )
	{
		rangeWindow = (float)( _range[1] - _range[0] );
	}
	return rangeWindow > 0.0 && rangeWindow <= (double)_buttonOffset + available;
}

void Slider::recomputeNobPosFromValue()
{
	int wide;
	int tall;
	getPaintSize( wide, tall );
	const float range = (float)( _range[1] - _range[0] );
	const float position = (float)( (double)( _value - _range[0] ) / range );
	double rangeWindow = _rangeWindow < 0 ? 0.0 : _rangeWindow;
	if ( !_rangeWindowEnabled )
	{
		rangeWindow = range;
	}
	if ( rangeWindow > 0.0 )
	{
		const int extent = _vertical ? tall : wide;
		const float pixels = (float)extent;
		const double nobWide = pixels / rangeWindow * pixels;
		const double start = ( pixels - nobWide ) * position;
		_nobPos[0] = (int)start;
		_nobPos[1] = (int)( start + nobWide );
		if ( _nobPos[1] > extent )
		{
			_nobPos[0] = extent - (int)nobWide;
			_nobPos[1] = extent;
		}
	}
	repaint();
}

void Slider::recomputeValueFromNobPos()
{
	int wide;
	int tall;
	getPaintSize( wide, tall );
	const float range = (float)( _range[1] - _range[0] );
	float rangeWindow = (float)( _rangeWindow < 0 ? 0 : _rangeWindow );
	if ( !_rangeWindowEnabled )
	{
		rangeWindow = range;
	}
	double position = _value - _range[0];
	if ( rangeWindow > 0.0f )
	{
		const float pixels = (float)( _vertical ? tall : wide );
		const double travel = pixels - (double)pixels / rangeWindow * pixels;
		position = (float)_nobPos[0] / travel * range;
	}
	_value = (int)( _range[0] + position + 0.5 );
	if ( _value >= _range[1] )
	{
		_value = _range[1];
	}
}

void Slider::privateCursorMoved( int x, int y, Panel* )
{
	if ( !_dragging )
	{
		return;
	}
	getApp()->getCursorPos( x, y );
	screenToLocal( x, y );
	int wide;
	int tall;
	getPaintSize( wide, tall );
	const int delta = _vertical ? y - _dragStartPos[1] : x - _dragStartPos[0];
	const int extent = _vertical ? tall : wide;
	_nobPos[0] = _nobDragStartPos[0] + delta;
	_nobPos[1] = _nobDragStartPos[1] + delta;
	if ( _nobPos[1] > extent )
	{
		_nobPos[0] += extent - _nobPos[1];
		_nobPos[1] = extent;
	}
	if ( _nobPos[0] < 0 )
	{
		_nobPos[1] -= _nobPos[0];
		_nobPos[0] = 0;
	}
	recomputeValueFromNobPos();
	repaint();
	fireIntChangeSignal();
}

void Slider::privateMousePressed( MouseCode, Panel* )
{
	int x;
	int y;
	getApp()->getCursorPos( x, y );
	screenToLocal( x, y );
	const int position = _vertical ? y : x;
	if ( position >= _nobPos[0] && position < _nobPos[1] )
	{
		_dragging = true;
		getApp()->setMouseCapture( this );
		_nobDragStartPos[0] = _nobPos[0];
		_nobDragStartPos[1] = _nobPos[1];
		_dragStartPos[0] = x;
		_dragStartPos[1] = y;
	}
}

void Slider::paintBackground()
{
	int wide;
	int tall;
	getPaintSize( wide, tall );
	if ( _vertical )
	{
		drawSetColor( Scheme::sc_secondary1 );
		drawFilledRect( 0, 0, wide, 1 );
		drawFilledRect( 0, tall - 1, wide, tall );
		drawFilledRect( 0, 1, 1, tall - 1 );
		drawFilledRect( wide - 1, 1, wide, tall - 1 );
		drawSetColor( Scheme::sc_secondary2 );
		drawFilledRect( 1, 1, wide - 1, 2 );
		drawFilledRect( 1, 2, 3, tall - 1 );
		drawFilledRect( 2, _nobPos[1], wide - 1, _nobPos[1] + 1 );
		drawSetColor( Scheme::sc_secondary3 );
		drawFilledRect( 2, 2, wide - 1, tall - 1 );
		drawSetColor( Scheme::sc_primary1 );
		drawFilledRect( 0, _nobPos[0], wide, _nobPos[0] + 1 );
		drawFilledRect( 0, _nobPos[1], wide, _nobPos[1] + 1 );
		drawFilledRect( 0, _nobPos[0] + 1, 1, _nobPos[1] );
		drawFilledRect( wide - 1, _nobPos[0] + 1, wide, _nobPos[1] );
		drawSetColor( Scheme::sc_primary3 );
		drawFilledRect( 1, _nobPos[0] + 1, wide - 1, _nobPos[0] + 2 );
		drawFilledRect( 1, _nobPos[0] + 2, 2, _nobPos[1] );
		drawSetColor( Scheme::sc_primary2 );
		drawFilledRect( 2, _nobPos[0] + 2, wide - 1, _nobPos[1] );
	}
	else
	{
		drawSetColor( Scheme::sc_secondary3 );
		drawFilledRect( 0, 0, wide, tall );
		drawSetColor( Scheme::sc_black );
		drawOutlinedRect( 0, 0, wide, tall );
		drawSetColor( Scheme::sc_primary2 );
		drawFilledRect( _nobPos[0], 0, _nobPos[1], tall );
		drawSetColor( Scheme::sc_black );
		drawOutlinedRect( _nobPos[0], 0, _nobPos[1], tall );
	}
}

}
