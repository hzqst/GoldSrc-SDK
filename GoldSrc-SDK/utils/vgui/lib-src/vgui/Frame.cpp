#include "VGUI_Frame.h"
#include "VGUI_ActionSignal.h"
#include "VGUI_App.h"
#include "VGUI_Button.h"
#include "VGUI_Cursor.h"
#include "VGUI_FrameSignal.h"
#include "VGUI_InputSignal.h"
#include "VGUI_Scheme.h"
#include "VGUI_SurfaceBase.h"

#include <string.h>

namespace
{

using namespace vgui;

class FooDraggerSignal : public InputSignal
{
public:
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
	FooDraggerSignal( Frame* frame )
	{
		_frame = frame;
		_dragging = false;
	}

	virtual void cursorMoved( int x, int y, Panel* panel ) override;
	virtual void cursorEntered( Panel* panel ) override;
	virtual void cursorExited( Panel* panel ) override;
	virtual void mousePressed( MouseCode code, Panel* panel ) override;
	virtual void mouseDoublePressed( MouseCode code, Panel* panel ) override;
	virtual void mouseReleased( MouseCode code, Panel* panel ) override;
	virtual void mouseWheeled( int delta, Panel* panel ) override;
	virtual void keyPressed( KeyCode code, Panel* panel ) override;
	virtual void keyTyped( KeyCode code, Panel* panel ) override;
	virtual void keyReleased( KeyCode code, Panel* panel ) override;
	virtual void keyFocusTicked( Panel* panel ) override;

	virtual void moved( int dx, int dy, bool internal, Panel* panel, Panel* parent );

protected:
	Frame*	_frame;
	bool	_dragging;

	int		_dragOrgPos[2];
	int		_dragOrgPos2[2];
	int		_dragOrgSize[2];
	int		_dragOrgSize2[2];
	int		_dragStart[2];
};

class FooCaptionGripSignal : public FooDraggerSignal
{
public:
	FooCaptionGripSignal( Frame* frame ) : FooDraggerSignal( frame )
	{
	}

	virtual void moved( int dx, int dy, bool internal, Panel* panel, Panel* parent ) override;
};

class FooTopGripSignal : public FooDraggerSignal
{
public:
	FooTopGripSignal( Frame* frame ) : FooDraggerSignal( frame )
	{
	}

	virtual void moved( int dx, int dy, bool internal, Panel* panel, Panel* parent ) override;
};

class FooBottomGripSignal : public FooDraggerSignal
{
public:
	FooBottomGripSignal( Frame* frame ) : FooDraggerSignal( frame )
	{
	}

	virtual void moved( int dx, int dy, bool internal, Panel* panel, Panel* parent ) override;
};

class FooLeftGripSignal : public FooDraggerSignal
{
public:
	FooLeftGripSignal( Frame* frame ) : FooDraggerSignal( frame )
	{
	}

	virtual void moved( int dx, int dy, bool internal, Panel* panel, Panel* parent ) override;
};

class FooRightGripSignal : public FooDraggerSignal
{
public:
	FooRightGripSignal( Frame* frame ) : FooDraggerSignal( frame )
	{
	}

	virtual void moved( int dx, int dy, bool internal, Panel* panel, Panel* parent ) override;
};

class FooTopLeftGripSignal : public FooDraggerSignal
{
public:
	FooTopLeftGripSignal( Frame* frame ) : FooDraggerSignal( frame )
	{
	}

	virtual void moved( int dx, int dy, bool internal, Panel* panel, Panel* parent ) override;
};

class FooTopRightGripSignal : public FooDraggerSignal
{
public:
	FooTopRightGripSignal( Frame* frame ) : FooDraggerSignal( frame )
	{
	}

	virtual void moved( int dx, int dy, bool internal, Panel* panel, Panel* parent ) override;
};

class FooBottomLeftGripSignal : public FooDraggerSignal
{
public:
	FooBottomLeftGripSignal( Frame* frame ) : FooDraggerSignal( frame )
	{
	}

	virtual void moved( int dx, int dy, bool internal, Panel* panel, Panel* parent ) override;
};

class FooBottomRightGripSignal : public FooDraggerSignal
{
public:
	FooBottomRightGripSignal( Frame* frame ) : FooDraggerSignal( frame )
	{
	}

	virtual void moved( int dx, int dy, bool internal, Panel* panel, Panel* parent ) override;
};

class FooCloseButtonHandler : public ActionSignal
{
public:
	FooCloseButtonHandler( Frame* frame )
	{
		_frame = frame;
	}

	virtual void actionPerformed( Panel* panel ) override;

protected:
	Frame*	_frame;
};

class FooMinimizeButtonHandler : public ActionSignal
{
public:
	FooMinimizeButtonHandler( Frame* frame )
	{
		_frame = frame;
	}

	virtual void actionPerformed( Panel* panel ) override;

protected:
	Frame*	_frame;
};

void FooDraggerSignal::cursorMoved( int x, int y, Panel* panel )
{
	if ( _dragging )
	{
		Panel* parent = panel->getParent();
		if ( parent != NULL )
		{
			parent->getApp()->getCursorPos( x, y );
			Frame* frame = dynamic_cast<Frame*>( parent );
			if ( frame != NULL )
			{
				int dx = x - _dragStart[0];
				int dy = y - _dragStart[1];
				if ( frame->isInternal() )
				{
					moved( dx, dy, true, parent, parent->getParent() );
					if ( parent->getParent() != NULL )
					{
						parent->getParent()->repaint();
					}
					parent->repaint();
				}
				else
				{
					Panel* grand = parent->getParent();
					if ( grand != NULL )
					{
						moved( dx, dy, false, parent, grand );
						if ( grand->getParent() != NULL )
						{
							grand->getParent()->repaint();
						}
						grand->repaint();
					}
				}
			}
		}
	}
}

void FooDraggerSignal::cursorEntered( Panel* )
{
}

void FooDraggerSignal::cursorExited( Panel* )
{
}

void FooDraggerSignal::mousePressed( MouseCode code, Panel* panel )
{
	Panel* parent = panel->getParent();
	if ( parent != NULL )
	{
		Frame* frame = dynamic_cast<Frame*>( parent );
		if ( code == MOUSE_LEFT && frame != NULL )
		{
			_dragging = true;

			int x;
			int y;
			parent->getApp()->getCursorPos( x, y );
			_dragStart[0] = x;
			_dragStart[1] = y;
			parent->getPos( _dragOrgPos[0], _dragOrgPos[1] );
			parent->getSize( _dragOrgSize[0], _dragOrgSize[1] );

			if ( parent->getParent() != NULL )
			{
				parent->getParent()->getPos( _dragOrgPos2[0], _dragOrgPos2[1] );
				parent->getParent()->getSize( _dragOrgSize2[0], _dragOrgSize2[1] );

				int x0;
				int y0;
				int x1;
				int y1;
				parent->getParent()->getAbsExtents( x0, y0, x1, y1 );
				parent->getApp()->setMouseArena( x0, y0, x1, y1, true );
				parent->getParent()->removeChild( parent );
				parent->getParent()->addChild( parent );
			}

			parent->getApp()->setMouseCapture( panel );
			parent->requestFocus();
			parent->repaintAll();
		}
	}
}

void FooDraggerSignal::mouseDoublePressed( MouseCode, Panel* )
{
}

void FooDraggerSignal::mouseReleased( MouseCode, Panel* panel )
{
	_dragging = false;
	panel->getApp()->setMouseArena( 0, 0, 0, 0, false );
	panel->getApp()->setMouseCapture( NULL );
}

void FooDraggerSignal::mouseWheeled( int, Panel* )
{
}

void FooDraggerSignal::keyPressed( KeyCode, Panel* )
{
}

void FooDraggerSignal::keyTyped( KeyCode, Panel* )
{
}

void FooDraggerSignal::keyReleased( KeyCode, Panel* )
{
}

void FooDraggerSignal::keyFocusTicked( Panel* )
{
}

void FooDraggerSignal::moved( int, int, bool, Panel*, Panel* )
{
}

void FooCaptionGripSignal::moved( int dx, int dy, bool internal, Panel* panel, Panel* parent )
{
	if ( _frame->isMoveable() )
	{
		if ( internal )
		{
			panel->setPos( _dragOrgPos[0] + dx, _dragOrgPos[1] + dy );
		}
		else
		{
			parent->setPos( _dragOrgPos2[0] + dx, _dragOrgPos2[1] + dy );
		}
	}
}

void FooTopGripSignal::moved( int, int dy, bool internal, Panel* panel, Panel* parent )
{
	if ( _frame->isSizeable() )
	{
		if ( internal )
		{
			panel->setPos( _dragOrgPos[0], _dragOrgPos[1] + dy );
			panel->setSize( _dragOrgSize[0], _dragOrgSize[1] - dy );
		}
		else
		{
			parent->setPos( _dragOrgPos2[0], _dragOrgPos2[1] + dy );
			parent->setSize( _dragOrgSize2[0], _dragOrgSize2[1] - dy );
		}
	}
}

void FooBottomGripSignal::moved( int, int dy, bool internal, Panel* panel, Panel* parent )
{
	if ( _frame->isSizeable() )
	{
		if ( internal )
		{
			panel->setSize( _dragOrgSize[0], _dragOrgSize[1] + dy );
		}
		else
		{
			parent->setSize( _dragOrgSize2[0], _dragOrgSize2[1] + dy );
		}
	}
}

void FooLeftGripSignal::moved( int dx, int, bool internal, Panel* panel, Panel* parent )
{
	if ( _frame->isSizeable() )
	{
		if ( internal )
		{
			panel->setPos( _dragOrgPos[0] + dx, _dragOrgPos[1] );
			panel->setSize( _dragOrgSize[0] - dx, _dragOrgSize[1] );
		}
		else
		{
			parent->setPos( _dragOrgPos2[0] + dx, _dragOrgPos2[1] );
			parent->setSize( _dragOrgSize2[0] - dx, _dragOrgSize2[1] );
		}
	}
}

void FooRightGripSignal::moved( int dx, int, bool internal, Panel* panel, Panel* parent )
{
	if ( _frame->isSizeable() )
	{
		if ( internal )
		{
			panel->setSize( _dragOrgSize[0] + dx, _dragOrgSize[1] );
		}
		else
		{
			parent->setSize( _dragOrgSize2[0] + dx, _dragOrgSize2[1] );
		}
	}
}

void FooTopLeftGripSignal::moved( int dx, int dy, bool internal, Panel* panel, Panel* parent )
{
	if ( _frame->isSizeable() )
	{
		if ( internal )
		{
			panel->setPos( _dragOrgPos[0] + dx, _dragOrgPos[1] + dy );
			panel->setSize( _dragOrgSize[0] - dx, _dragOrgSize[1] - dy );
		}
		else
		{
			parent->setPos( _dragOrgPos2[0] + dx, _dragOrgPos2[1] );
			parent->setSize( _dragOrgSize2[0] - dx, _dragOrgSize2[1] + dy );
		}
	}
}

void FooTopRightGripSignal::moved( int dx, int dy, bool internal, Panel* panel, Panel* parent )
{
	if ( _frame->isSizeable() )
	{
		if ( internal )
		{
			panel->setPos( _dragOrgPos[0], _dragOrgPos[1] + dy );
			panel->setSize( _dragOrgSize[0] + dx, _dragOrgSize[1] - dy );
		}
		else
		{
			parent->setPos( _dragOrgPos2[0], _dragOrgPos2[1] + dy );
			parent->setSize( _dragOrgSize2[0] + dx, _dragOrgSize2[1] - dy );
		}
	}
}

void FooBottomLeftGripSignal::moved( int dx, int dy, bool internal, Panel* panel, Panel* parent )
{
	if ( _frame->isSizeable() )
	{
		if ( internal )
		{
			panel->setPos( _dragOrgPos[0] + dx, _dragOrgPos[1] );
			panel->setSize( _dragOrgSize[0] - dx, _dragOrgSize[1] + dy );
		}
		else
		{
			parent->setPos( _dragOrgPos2[0] + dx, _dragOrgPos2[1] );
			parent->setSize( _dragOrgSize2[0] - dx, _dragOrgSize2[1] + dy );
		}
	}
}

void FooBottomRightGripSignal::moved( int dx, int dy, bool internal, Panel* panel, Panel* parent )
{
	if ( _frame->isSizeable() )
	{
		if ( internal )
		{
			panel->setSize( _dragOrgSize[0] + dx, _dragOrgSize[1] + dy );
		}
		else
		{
			parent->setSize( _dragOrgSize2[0] + dx, _dragOrgSize2[1] + dy );
		}
	}
}

void FooCloseButtonHandler::actionPerformed( Panel* )
{
	_frame->fireClosingSignal();
}

void FooMinimizeButtonHandler::actionPerformed( Panel* )
{
	_frame->fireMinimizingSignal();
}

static void disableGripPaint( Panel* grip )
{
	grip->setPaintBorderEnabled( false );
	grip->setPaintBackgroundEnabled( false );
	grip->setPaintEnabled( false );
}

}

namespace vgui
{

void Frame::setInternal( bool state )
{
	_internal = state;
}

bool Frame::isInternal()
{
	return _internal;
}

Panel* Frame::getClient()
{
	return _client;
}

void Frame::setMoveable( bool state )
{
	_moveable = state;
}

void Frame::setSizeable( bool state )
{
	_sizeable = state;
}

bool Frame::isMoveable()
{
	return _moveable;
}

bool Frame::isSizeable()
{
	return _sizeable;
}

void Frame::fireClosingSignal()
{
	for ( int i = 0; i < _frameSignalDar.getCount(); i++ )
	{
		_frameSignalDar[i]->closing( this );
	}
}

void Frame::fireMinimizingSignal()
{
	for ( int i = 0; i < _frameSignalDar.getCount(); i++ )
	{
		_frameSignalDar[i]->minimizing( this, false );
	}
}

void Frame::setMenuButtonVisible( bool state )
{
	_menuButton->setVisible( state );
}

void Frame::setTrayButtonVisible( bool state )
{
	_trayButton->setVisible( state );
}

void Frame::setMinimizeButtonVisible( bool state )
{
	_minimizeButton->setVisible( state );
}

void Frame::setMaximizeButtonVisible( bool state )
{
	_maximizeButton->setVisible( state );
}

void Frame::setCloseButtonVisible( bool state )
{
	_closeButton->setVisible( state );
}

void Frame::setTitle( const char* title )
{
	delete[] _title;
	_title = vgui_strdup( title );
	repaint();
}

void Frame::getTitle( char* buf, int bufLen )
{
	vgui_strcpy( buf, bufLen, _title );
}

void Frame::paintBackground()
{
	Scheme::SchemeColor fill = Scheme::sc_secondary3;
	Scheme::SchemeColor edge = Scheme::sc_secondary1;
	Scheme::SchemeColor shade = Scheme::sc_secondary2;

	if ( getSurfaceBase()->hasFocus() )
	{
		Panel* focus = getApp()->getFocus();
		while ( focus != NULL )
		{
			if ( focus == this )
			{
				fill = Scheme::sc_primary3;
				edge = Scheme::sc_primary1;
				shade = Scheme::sc_primary2;
			}
			focus = focus->getParent();
		}
	}

	int wide = _size[0];
	int tall = _size[1];

	drawSetColor( fill );
	drawFilledRect( 5, 5, wide - 5, 28 );

	drawSetColor( Scheme::sc_white );
	drawFilledRect( 120, 10, wide - 70, 11 );
	drawFilledRect( 120, 15, wide - 70, 16 );
	drawFilledRect( 120, 20, wide - 70, 21 );

	drawSetColor( edge );
	drawFilledRect( 120, 12, wide - 70, 13 );
	drawFilledRect( 120, 17, wide - 70, 18 );
	drawFilledRect( 120, 22, wide - 70, 23 );

	drawSetColor( edge );
	drawFilledRect( 1, 0, wide - 1, 5 );
	drawFilledRect( 1, tall - 5, wide - 1, tall );
	drawFilledRect( 0, 1, 5, tall - 1 );
	drawFilledRect( wide - 5, 1, wide, tall - 1 );
	drawFilledRect( 5, 5, 6, 6 );
	drawFilledRect( wide - 6, 5, wide - 5, 6 );
	drawFilledRect( 5, 28, wide - 5, 29 );

	drawSetColor( Scheme::sc_black );
	drawFilledRect( 13, 2, wide - 13, 3 );
	drawFilledRect( 13, tall - 3, wide - 13, tall - 2 );
	drawFilledRect( 2, 13, 3, tall - 13 );
	drawFilledRect( wide - 3, 13, wide - 2, tall - 13 );

	drawSetColor( shade );
	drawFilledRect( 14, 3, wide - 12, 4 );
	drawFilledRect( 14, tall - 2, wide - 12, tall - 1 );
	drawFilledRect( 3, 14, 4, tall - 12 );
	drawFilledRect( wide - 2, 14, wide - 1, tall - 12 );

	if ( _title != NULL )
	{
		drawSetTextFont( Scheme::sf_primary1 );
		drawSetTextColor( Scheme::sc_black );
		drawPrintText( 28, 7, _title, (int)strlen( _title ) );
	}
}

void Frame::addFrameSignal( FrameSignal* s )
{
	_frameSignalDar.putElement( s );
}

void Frame::setSize( int wide, int tall )
{
	if ( getWide() != wide || getTall() != tall )
	{
		Panel::setSize( wide, tall );

		wide = _size[0];
		tall = _size[1];

		_topGrip->setBounds( 15, 0, wide - 30, 5 );
		_bottomGrip->setBounds( 15, tall - 5, wide - 30, 5 );
		_leftGrip->setBounds( 0, 15, 5, tall - 30 );
		_rightGrip->setBounds( wide - 5, 15, 5, tall - 30 );
		_topLeftGrip->setBounds( 0, 0, 15, 15 );
		_topRightGrip->setBounds( wide - 15, 0, 15, 15 );
		_bottomLeftGrip->setBounds( 0, tall - 15, 15, 15 );
		_bottomRightGrip->setBounds( wide - 15, tall - 15, 15, 15 );
		_captionGrip->setSize( wide - 10, 23 );
		_client->setSize( wide - 10, tall - 34 );
		_trayButton->setPos( wide - 85, 8 );
		_minimizeButton->setPos( wide - 65, 8 );
		_maximizeButton->setPos( wide - 45, 8 );
		_closeButton->setPos( wide - 25, 8 );
		_client->invalidateLayout( false );
	}
}

void Frame::setVisible( bool state )
{
	Panel::setVisible( state );
	if ( getParent() != NULL )
	{
		getParent()->repaint();
	}
}

Frame::Frame( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	_title = NULL;
	_internal = true;
	_moveable = true;
	_sizeable = true;
	setTitle( "Untitled" );
	setMinimumSize( 64, 33 );

	// The grips use the scheme's cursors: Panel does not own a cursor, so one
	// made here would never be freed.
	_topGrip = new Panel( 15, 0, wide - 30, 5 );
	_topGrip->setParent( this );
	_topGrip->setCursor( Scheme::scu_sizens );
	_topGrip->addInputSignal( new FooTopGripSignal( this ) );
	disableGripPaint( _topGrip );

	_bottomGrip = new Panel( 15, tall - 5, wide - 30, 5 );
	_bottomGrip->setParent( this );
	_bottomGrip->setCursor( Scheme::scu_sizens );
	_bottomGrip->addInputSignal( new FooBottomGripSignal( this ) );
	disableGripPaint( _bottomGrip );

	_leftGrip = new Panel( 0, 15, 5, tall - 30 );
	_leftGrip->setParent( this );
	_leftGrip->setCursor( Scheme::scu_sizewe );
	_leftGrip->addInputSignal( new FooLeftGripSignal( this ) );
	disableGripPaint( _leftGrip );

	_rightGrip = new Panel( wide - 5, 15, 5, tall - 30 );
	_rightGrip->setParent( this );
	_rightGrip->setCursor( Scheme::scu_sizewe );
	_rightGrip->addInputSignal( new FooRightGripSignal( this ) );
	disableGripPaint( _rightGrip );

	_topLeftGrip = new Panel( 0, 0, 15, 15 );
	_topLeftGrip->setParent( this );
	_topLeftGrip->setCursor( Scheme::scu_sizenwse );
	_topLeftGrip->addInputSignal( new FooTopLeftGripSignal( this ) );
	disableGripPaint( _topLeftGrip );

	_topRightGrip = new Panel( wide - 15, 0, 15, 15 );
	_topRightGrip->setParent( this );
	_topRightGrip->setCursor( Scheme::scu_sizenesw );
	_topRightGrip->addInputSignal( new FooTopRightGripSignal( this ) );
	disableGripPaint( _topRightGrip );

	_bottomLeftGrip = new Panel( 0, tall - 15, 15, 15 );
	_bottomLeftGrip->setParent( this );
	_bottomLeftGrip->setCursor( Scheme::scu_sizenesw );
	_bottomLeftGrip->addInputSignal( new FooBottomLeftGripSignal( this ) );
	disableGripPaint( _bottomLeftGrip );

	_bottomRightGrip = new Panel( wide - 15, tall - 15, 15, 15 );
	_bottomRightGrip->setParent( this );
	_bottomRightGrip->setCursor( Scheme::scu_sizenwse );
	_bottomRightGrip->addInputSignal( new FooBottomRightGripSignal( this ) );
	disableGripPaint( _bottomRightGrip );

	_captionGrip = new Panel( 5, 5, wide - 10, 23 );
	_captionGrip->setParent( this );
	_captionGrip->setCursor( Scheme::scu_sizeall );
	_captionGrip->addInputSignal( new FooCaptionGripSignal( this ) );
	disableGripPaint( _captionGrip );

	_client = new Panel( 5, 29, wide - 10, tall - 34 );
	_client->setParent( this );
	_client->addInputSignal( new FooDraggerSignal( this ) );

	_trayButton = new Button( ".", wide - 85, 8, 18, 18 );
	_trayButton->setParent( this );

	_minimizeButton = new Button( "2", wide - 65, 8, 18, 18 );
	_minimizeButton->setFont( Scheme::sf_secondary1 );
	_minimizeButton->setParent( this );
	_minimizeButton->addActionSignal( new FooMinimizeButtonHandler( this ) );

	_maximizeButton = new Button( "1", wide - 45, 8, 18, 18 );
	_maximizeButton->setFont( Scheme::sf_secondary1 );
	_maximizeButton->setParent( this );

	_closeButton = new Button( "r", wide - 25, 8, 18, 18 );
	_closeButton->setFont( Scheme::sf_secondary1 );
	_closeButton->setParent( this );
	_closeButton->addActionSignal( new FooCloseButtonHandler( this ) );

	_menuButton = new Button( "s", 7, 8, 18, 18 );
	_menuButton->setFont( Scheme::sf_secondary1 );
	_menuButton->setParent( this );
}

#if VGUI_ABI_COMPAT_MODE != 1
Frame::Frame( const Frame& other ) : Panel( other )
{
	_title = NULL;
	copyFrom( other );
}

Frame::~Frame()
{
	// A copy, since a signal may delete itself; the signals are not owned.
	Dar<FrameSignal*> signals( _frameSignalDar );
	for ( int i = 0; i < signals.getCount(); ++i )
		signals[i]->frameDeleted( this );

	delete[] _title;
}

Frame& Frame::operator=( const Frame& other )
{
	if ( this != &other )
	{
		Panel::operator=( other );
		copyFrom( other );
	}

	return *this;
}

// Like the original copy, the grips, buttons and signals stay shared.
void Frame::copyFrom( const Frame& other )
{
	setTitle( other._title );
	_internal = other._internal;
	_sizeable = other._sizeable;
	_moveable = other._moveable;
	_topGrip = other._topGrip;
	_bottomGrip = other._bottomGrip;
	_leftGrip = other._leftGrip;
	_rightGrip = other._rightGrip;
	_topLeftGrip = other._topLeftGrip;
	_topRightGrip = other._topRightGrip;
	_bottomLeftGrip = other._bottomLeftGrip;
	_bottomRightGrip = other._bottomRightGrip;
	_captionGrip = other._captionGrip;
	_client = other._client;
	_trayButton = other._trayButton;
	_minimizeButton = other._minimizeButton;
	_maximizeButton = other._maximizeButton;
	_closeButton = other._closeButton;
	_menuButton = other._menuButton;
	_frameSignalDar = other._frameSignalDar;
	_resizeable = other._resizeable;
}
#endif

}
