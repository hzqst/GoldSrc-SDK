#include "VGUI_BuildGroup.h"
#include "DefaultBorder.h"

#include "VGUI_App.h"
#include "VGUI_ChangeSignal.h"
#include "VGUI_Cursor.h"
#include "VGUI_Label.h"
#include "VGUI_LineBorder.h"
#include "VGUI_Panel.h"

#include <stdio.h>
#include <string.h>

namespace vgui
{

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
BuildGroup::BuildGroup()
{
	_enabled = false;
	_snapX = 4;
	_snapY = 4;
	_cursor_sizenwse = new Cursor( Cursor::DefaultCursor::dc_sizenwse );
	_cursor_sizenesw = new Cursor( Cursor::DefaultCursor::dc_sizenesw );
	_cursor_sizewe = new Cursor( Cursor::DefaultCursor::dc_sizewe );
	_cursor_sizens = new Cursor( Cursor::DefaultCursor::dc_sizens );
	_cursor_sizeall = new Cursor( Cursor::DefaultCursor::dc_sizeall );
	_currentPanel = NULL;
	_dragging = false;
}

#if VGUI_ABI_COMPAT_MODE != 1
BuildGroup::BuildGroup( const BuildGroup& other )
{
	_cursor_sizenwse = new Cursor( Cursor::DefaultCursor::dc_sizenwse );
	_cursor_sizenesw = new Cursor( Cursor::DefaultCursor::dc_sizenesw );
	_cursor_sizewe = new Cursor( Cursor::DefaultCursor::dc_sizewe );
	_cursor_sizens = new Cursor( Cursor::DefaultCursor::dc_sizens );
	_cursor_sizeall = new Cursor( Cursor::DefaultCursor::dc_sizeall );
	*this = other;
}

BuildGroup::~BuildGroup()
{
	delete _cursor_sizenwse;
	delete _cursor_sizenesw;
	delete _cursor_sizewe;
	delete _cursor_sizens;
	delete _cursor_sizeall;

	for ( int i = 0; i < _panelNameDar.getCount(); ++i )
		delete[] _panelNameDar[i];
}

// Keeps this group's own cursors.
BuildGroup& BuildGroup::operator=( const BuildGroup& other )
{
	if ( this == &other )
		return *this;

	_enabled = other._enabled;
	_snapX = other._snapX;
	_snapY = other._snapY;
	_dragging = other._dragging;
	_dragMouseCode = other._dragMouseCode;
	memcpy( _dragStartPanelPos, other._dragStartPanelPos, sizeof( _dragStartPanelPos ) );
	memcpy( _dragStartCursorPos, other._dragStartCursorPos, sizeof( _dragStartCursorPos ) );
	_currentPanel = other._currentPanel;
	_currentPanelChangeSignalDar = other._currentPanelChangeSignalDar;
	_panelDar = other._panelDar;

	Dar<char*> names( other._panelNameDar );
	for ( int i = 0; i < _panelNameDar.getCount(); ++i )
		delete[] _panelNameDar[i];
	_panelNameDar.removeAll();
	for ( int i = 0; i < names.getCount(); ++i )
		_panelNameDar.addElement( vgui_strdup( names[i] ) );

	return *this;
}

void* BuildGroup::operator new( size_t size )
{
	return ::operator new( size );
}

void BuildGroup::operator delete( void* memory )
{
	::operator delete( memory );
}
#endif

void BuildGroup::setEnabled( bool state )
{
	if ( _enabled != state )
	{
		if ( _currentPanel != NULL )
		{
			_currentPanel = NULL;
			fireCurrentPanelChangeSignal();
		}

		_currentPanel = NULL;
	}

	_enabled = state;
}

bool BuildGroup::isEnabled()
{
	return _enabled;
}

void BuildGroup::addCurrentPanelChangeSignal( ChangeSignal* s )
{
	_currentPanelChangeSignalDar.putElement( s );
}

void BuildGroup::mouseDoublePressed( MouseCode, Panel* )
{
}

void BuildGroup::panelAdded( Panel* panel, const char* panelName )
{
	_panelDar.addElement( panel );
	_panelNameDar.addElement( vgui_strdup( panelName ) );
}

Panel* BuildGroup::getCurrentPanel()
{
	return _currentPanel;
}

#if defined( _MSC_VER )
#pragma warning( suppress : 6262 )
#endif
void BuildGroup::copyPropertiesToClipboard()
{
	char text[32768];
	size_t textLen = 0;

	text[0] = '\0';
	for ( int i = 0; i < _panelDar.getCount(); ++i )
	{
		char buf[512] = {};

		_panelDar[i]->getPersistanceText( buf, sizeof( buf ) );
		const size_t remaining = sizeof( text ) - textLen;
		const int written = snprintf( text + textLen, remaining, "%s%s", _panelNameDar[i], buf );
		if ( written < 0 || (size_t)written >= remaining )
		{
			// Drop the entry that does not fit so the clipboard only holds complete lines.
			text[textLen] = '\0';
			break;
		}
		textLen += (size_t)written;
	}

	App::getInstance()->setClipboardText( text, (int)textLen );
	vgui_printf( "Copied to clipboard\n" );
}

void BuildGroup::applySnap( Panel* panel )
{
	int x;
	int y;
	int wide;
	int tall;
	int snapY;

	panel->getBounds( x, y, wide, tall );
	snapY = _snapY;
	x = x / _snapX * _snapX;
	y = y / snapY * snapY;
	panel->setPos( x, y );
	panel->setSize( _snapX * ( ( wide + x ) / _snapX ) - x, _snapY * ( ( y + tall ) / _snapY ) - y );
}

void BuildGroup::fireCurrentPanelChangeSignal()
{
	for ( int i = 0; i < _currentPanelChangeSignalDar.getCount(); ++i )
	{
		_currentPanelChangeSignalDar[i]->valueChanged( NULL );
	}
}

void BuildGroup::cursorMoved( int x, int y, Panel* panel )
{
	if ( _dragging )
	{
		panel->getApp()->getCursorPos( x, y );

		if ( _dragMouseCode == MouseCode::MOUSE_RIGHT )
		{
			panel->setSize( x - _dragStartCursorPos[0], y - _dragStartCursorPos[1] );
		}
		else
		{
			panel->setPos( _dragStartPanelPos[0] + x - _dragStartCursorPos[0],
					_dragStartPanelPos[1] + y - _dragStartCursorPos[1] );
		}

		applySnap( panel );
		panel->repaintParent();
	}
}

void BuildGroup::mousePressed( MouseCode code, Panel* panel )
{
	if ( code == MouseCode::MOUSE_RIGHT )
	{
		int lx;
		int ly;
		Label* newPanel;

		panel->getApp()->getCursorPos( lx, ly );
		panel->screenToLocal( lx, ly );
		newPanel = new Label( "Label", lx, ly, 0, 0 );
		newPanel->setBorder( defaultBorder<LineBorder>() );
		newPanel->setParent( panel );
		newPanel->setBuildGroup( this, "Label" );
		panel = newPanel;
	}

	_dragging = true;
	_dragMouseCode = code;
	panel->requestFocus();

	int lx;
	int ly;

	panel->getApp()->getCursorPos( lx, ly );
	_dragStartCursorPos[0] = lx;
	_dragStartCursorPos[1] = ly;
	panel->getPos( lx, ly );
	_dragStartPanelPos[0] = lx;
	_dragStartPanelPos[1] = ly;
	panel->getApp()->setMouseCapture( panel );

	if ( panel != _currentPanel )
	{
		_currentPanel = panel;
		fireCurrentPanelChangeSignal();
	}
}

void BuildGroup::mouseReleased( MouseCode, Panel* panel )
{
	_dragging = false;
	panel->getApp()->setMouseCapture( NULL );
}

void BuildGroup::keyTyped( KeyCode code, Panel* panel )
{
	bool shift = panel->isKeyDown( KeyCode::KEY_LSHIFT );
	bool control;

	if ( !shift )
	{
		shift = panel->isKeyDown( KeyCode::KEY_RSHIFT );
	}

	control = panel->isKeyDown( KeyCode::KEY_LCONTROL );

	if ( !control )
	{
		control = panel->isKeyDown( KeyCode::KEY_RCONTROL );
	}

	if ( code == KeyCode::KEY_C )
	{
		if ( control )
		{
			copyPropertiesToClipboard();
		}

		return;
	}

	int snapX = 0;
	int snapY = 0;

	if ( code == KeyCode::KEY_LEFT )
	{
		snapX = -_snapX;
	}
	else if ( code == KeyCode::KEY_RIGHT )
	{
		snapX = _snapX;
	}
	else if ( code == KeyCode::KEY_UP )
	{
		snapY = -_snapY;
	}
	else if ( code == KeyCode::KEY_DOWN )
	{
		snapY = _snapY;
	}
	else
	{
		return;
	}

	if ( ( snapX | snapY ) == 0 )
	{
		return;
	}

	int x;
	int y;
	int wide;
	int tall;

	panel->getBounds( x, y, wide, tall );

	if ( shift )
	{
		panel->setSize( wide + snapX, tall + snapY );
	}
	else
	{
		panel->setPos( x + snapX, y + snapY );
	}

	applySnap( panel );
	panel->repaint();

	if ( panel->getParent() != NULL )
	{
		panel->getParent()->repaint();
	}
}

Cursor* BuildGroup::getCursor( Panel* panel )
{
	int x;
	int y;
	int wide;
	int tall;

	panel->getApp()->getCursorPos( x, y );
	panel->screenToLocal( x, y );
	panel->getSize( wide, tall );

	if ( x > 1 )
	{
		return _cursor_sizeall;
	}

	if ( y <= 3 )
	{
		return _cursor_sizenwse;
	}

	if ( y >= tall - 4 )
	{
		return _cursor_sizenesw;
	}

	return _cursor_sizewe;
}

}
