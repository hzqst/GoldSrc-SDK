#include "VGUI_TaskBar.h"
#include "ActionSignalForwarder.h"
#include "DefaultBorder.h"
#include "VGUI_ActionSignal.h"
#include "VGUI_App.h"
#include "VGUI_Button.h"
#include "VGUI_FocusChangeSignal.h"
#include "VGUI_Frame.h"
#include "VGUI_FrameSignal.h"
#include "VGUI_LoweredBorder.h"
#include "VGUI_RaisedBorder.h"
#include "VGUI_Scheme.h"
#include "VGUI_TickSignal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

namespace
{

using namespace vgui;

class FooClock : public Panel, public TickSignal
{
public:
	FooClock( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
	{
		_hour = 0;
		_minute = 0;
		_second = 0;
		getApp()->addTickSignal( this );
	}

#if VGUI_ABI_COMPAT_MODE != 1
	// The App would otherwise tick a deleted clock.
	virtual ~FooClock() override
	{
		App* app = App::getInstance();
		if ( app != NULL )
			app->removeTickSignal( this );
	}
#endif

	virtual void paintBackground() override;
	virtual void ticked() override;

protected:
	int	_hour;
	int	_minute;
	int	_second;
};

class FooTaskBarButtonHandler : public ActionSignal, public FrameSignal, public FocusChangeSignal
{
public:
	FooTaskBarButtonHandler( TaskBar* taskBar, Button* button, Frame* frame )
	{
		_taskBar = taskBar;
		_button = button;
		_frame = frame;
	}

	virtual void actionPerformed( Panel* panel ) override;

	virtual void closing( Frame* frame ) override;
	virtual void minimizing( Frame* frame, bool toTray ) override;
#if VGUI_ABI_COMPAT_MODE != 1
	virtual void frameDeleted( Frame* frame ) override;
#endif

	virtual void focusChanged( bool lost, Panel* panel ) override;

protected:
	TaskBar*	_taskBar;
	Button*		_button;
	Frame*		_frame;

	friend class FooTaskBarButtonSignal;
};

// The action signal a task bar button owns. The frame keeps the handler after
// the button is deleted, so the handler stops using the button then.
class FooTaskBarButtonSignal : public ActionSignalForwarder
{
public:
	FooTaskBarButtonSignal( FooTaskBarButtonHandler* handler ) : ActionSignalForwarder( handler )
	{
#if VGUI_ABI_COMPAT_MODE != 1
		_handler = handler;
#endif
	}

#if VGUI_ABI_COMPAT_MODE != 1
	virtual ~FooTaskBarButtonSignal() override
	{
		_handler->_button = NULL;
	}

private:
	FooTaskBarButtonHandler*	_handler;
#endif
};

void FooClock::paintBackground()
{
	char buf[50];

	if ( _hour > 12 )
	{
		snprintf( buf, sizeof( buf ), "%d:%.2d:%.2d PM", _hour - 12, _minute, _second );
	}
	else
	{
		snprintf( buf, sizeof( buf ), "%d:%.2d:%.2d AM", _hour, _minute, _second );
	}

	Panel::paintBackground();
	drawSetTextFont( Scheme::SchemeFont::sf_primary1 );
	drawSetTextColor( Scheme::SchemeColor::sc_black );
	drawPrintText( 0, 0, buf, strlen( buf ) );
}

void FooClock::ticked()
{
	time_t aclock;
	tm* newtime;
	int hour;
	int minute;
	int second;

	time( &aclock );
#if defined( _WIN32 )
#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4996 )
#endif
	newtime = localtime( &aclock );
#if defined( _MSC_VER )
#pragma warning( pop )
#endif
#else
	newtime = localtime( &aclock );
	if ( newtime == NULL )
	{
		return;
	}
#endif
	hour = newtime->tm_hour;
	minute = newtime->tm_min;
	second = newtime->tm_sec;
	if ( _hour != hour || _minute != minute || _second != second )
	{
		_hour = hour;
		_minute = minute;
		_second = second;
		repaint();
	}
}

void FooTaskBarButtonHandler::actionPerformed( Panel* )
{
	_frame->setVisible( true );
	_frame->requestFocus();
}

void FooTaskBarButtonHandler::closing( Frame* )
{
}

void FooTaskBarButtonHandler::minimizing( Frame*, bool )
{
	_frame->setVisible( false );
	_frame->getApp()->requestFocus( NULL );
}

#if VGUI_ABI_COMPAT_MODE != 1
// The handler lives as long as its frame. The button, while there is one, is a
// child of the task bar, so the task bar is still there too.
void FooTaskBarButtonHandler::frameDeleted( Frame* frame )
{
	// Frame copies share the frame signals.
	if ( frame != _frame )
	{
		return;
	}

	if ( _button != NULL )
	{
		_taskBar->removeFrame( _frame );
	}

	delete this;
}
#endif

void FooTaskBarButtonHandler::focusChanged( bool lost, Panel* )
{
	if ( _button != NULL )
	{
		_button->setSelected( !lost );
	}
}

}

namespace vgui
{

TaskBar::TaskBar( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	FooClock* clock;

	setBorder( defaultBorder<RaisedBorder>() );
	_tray = new Panel( 100, 0, 120, 26 );
	_tray->setBorder( defaultBorder<LoweredBorder>() );
	_tray->setParent( this );
	clock = new FooClock( 25, 2, 85, 20 );
	clock->setParent( _tray );
}

void TaskBar::addFrame( Frame* frame )
{
	char buf[256];
	Button* button;
	FooTaskBarButtonHandler* handler;

	_frameDar.addElement( frame );
	frame->getTitle( buf, 256 );
	button = new Button( buf, 2, 2 );
	handler = new FooTaskBarButtonHandler( this, button, frame );
	button->setParent( this );
	// The button deletes its action signals; the frame keeps the handler.
	button->addActionSignal( new FooTaskBarButtonSignal( handler ) );
	frame->addFrameSignal( handler );
	frame->addFocusChangeSignal( handler );
	_taskButtonDar.addElement( button );
	invalidateLayout( false );
}

#if VGUI_ABI_COMPAT_MODE != 1
void TaskBar::removeFrame( Frame* frame )
{
	for ( int i = 0; i < _frameDar.getCount(); ++i )
	{
		if ( _frameDar[i] == frame )
		{
			Button* button = _taskButtonDar[i];
			_frameDar.removeElementAt( i );
			_taskButtonDar.removeElementAt( i );
			delete button;
			invalidateLayout( false );
			return;
		}
	}
}
#endif

void TaskBar::performLayout()
{
	int wide;
	int tall;
	int tWide;
	int tTall;
	int xx;

	getPaintSize( wide, tall );
	_tray->getSize( tWide, tTall );
	_tray->setBounds( wide - tWide - 3, 2, tWide, tall - 4 );

	xx = 3;
	for ( int i = 0; i < _taskButtonDar.getCount(); i++ )
	{
		Button* button;
		int x;
		int y;

		button = _taskButtonDar[i];
		button->getBounds( x, y, wide, tall );
		button->setPos( xx, y );
		xx += wide + 3;
	}
}

}
