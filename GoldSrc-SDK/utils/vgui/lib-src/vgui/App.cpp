#include "VGUI_App.h"

#include "VGUI_Panel.h"
#include "VGUI_Scheme.h"
#include "VGUI_SurfaceBase.h"
#include "VGUI_TickSignal.h"

#include <stdlib.h>
#include <string.h>

namespace vgui
{

#if defined( _WIN32 )
namespace
{
const char* staticKeyTrans[KEY_LAST];
const char* const keyTranslations[KEY_LAST] =
{
	"0)KEY_0", "1!KEY_1", "2@KEY_2", "3#KEY_3", "4$KEY_4",
	"5%KEY_5", "6^KEY_6", "7&KEY_7", "8*KEY_8", "9(KEY_9",
	"aAKEY_A", "bBKEY_B", "cCKEY_C", "dDKEY_D", "eEKEY_E", "fFKEY_F",
	"gGKEY_G", "hHKEY_H", "iIKEY_I", "jJKEY_J", "kKKEY_K", "lLKEY_L",
	"mMKEY_M", "nNKEY_N", "oOKEY_O", "pPKEY_P", "qQKEY_Q", "rRKEY_R",
	"sSKEY_S", "tTKEY_T", "uUKEY_U", "vVKEY_V", "wWKEY_W", "xXKEY_X",
	"yYKEY_Y", "zZKEY_Z",
	"0\0KEY_PAD_0", "1\0KEY_PAD_1", "2\0KEY_PAD_2", "3\0KEY_PAD_3",
	"4\0KEY_PAD_4", "5\0KEY_PAD_5", "6\0KEY_PAD_6", "7\0KEY_PAD_7",
	"8\0KEY_PAD_8", "9\0KEY_PAD_9", "//KEY_PAD_DIVIDE", "**KEY_PAD_MULTIPLY",
	"--KEY_PAD_MINUS", "++KEY_PAD_PLUS", "\0\0KEY_PAD_ENTER", ".\0KEY_PAD_DECIMAL",
	"[{KEY_LBRACKET", "]}KEY_RBRACKET", ";:KEY_SEMICOLON", "'\"KEY_APOSTROPHE",
	"`~KEY_BACKQUOTE", ",<KEY_COMMA", ".>KEY_PERIOD", "/?KEY_SLASH",
	"\\|KEY_BACKSLASH", "-_KEY_MINUS", "=+KEY_EQUAL", "\0\0KEY_ENTER",
	"  KEY_SPACE", "\0\0KEY_BACKSPACE", "\0\0KEY_TAB", "\0\0KEY_CAPSLOCK",
	"\0\0KEY_NUMLOCK", "\0\0KEY_ESCAPE", "\0\0KEY_SCROLLLOCK", "\0\0KEY_INSERT",
	"\0\0KEY_DELETE", "\0\0KEY_HOME", "\0\0KEY_END", "\0\0KEY_PAGEUP",
	"\0\0KEY_PAGEDOWN", "\0\0KEY_BREAK", "\0\0KEY_LSHIFT", "\0\0KEY_RSHIFT",
	"\0\0KEY_LALT", "\0\0KEY_RALT", "\0\0KEY_LCONTROL", "\0\0KEY_RCONTROL",
	"\0\0KEY_LWIN", "\0\0KEY_RWIN", "\0\0KEY_APP", "\0\0KEY_UP",
	"\0\0KEY_LEFT", "\0\0KEY_DOWN", "\0\0KEY_RIGHT", "\0\0KEY_F1",
	"\0\0KEY_F2", "\0\0KEY_F3", "\0\0KEY_F4", "\0\0KEY_F5",
	"\0\0KEY_F6", "\0\0KEY_F7", "\0\0KEY_F8", "\0\0KEY_F9",
	"\0\0KEY_F10", "\0\0KEY_F11", "\0\0KEY_F12"
};
}
#endif

App* App::_instance = NULL;

namespace
{
// The surface that has the native mouse capture. The capture panel can leave
// that surface while it holds the capture, so it is released here rather than
// on the panel's surface. Kept outside App so that App keeps its layout; there
// is only one App.
SurfaceBase* capturedSurface = NULL;
}

App::App()
{
	init();
}

App::App( bool externalMain )
{
	init();
	_externalMain = externalMain;
}

void App::init()
{
	_externalMain = false;
	_running = false;
	_keyFocus = NULL;
	_oldMouseFocus = NULL;
	_mouseFocus = NULL;
	_mouseCapture = NULL;
	_wantedKeyFocus = NULL;
	_instance = this;

#if VGUI_ABI_COMPAT_MODE != 1
	_scheme = _ownScheme.scheme;
#else
	_scheme = new Scheme();
#endif
	_mouseArenaPanel = NULL;
	_buildMode = false;
	_wantedBuildMode = false;
	_cursorOveride = NULL;
	_nextTickMillis = getTimeMillis();
	_minimumTickMillisInterval = 50;

	memset( _mousePressed, 0, sizeof( _mousePressed ) );
	memset( _mouseDoublePressed, 0, sizeof( _mouseDoublePressed ) );
	memset( _mouseDown, 0, sizeof( _mouseDown ) );
	memset( _mouseReleased, 0, sizeof( _mouseReleased ) );
	memset( _keyPressed, 0, sizeof( _keyPressed ) );
	memset( _keyTyped, 0, sizeof( _keyTyped ) );
	memset( _keyDown, 0, sizeof( _keyDown ) );
	memset( _keyReleased, 0, sizeof( _keyReleased ) );
#if defined( _WIN32 )
	for ( int code = KEY_0; code < KEY_LAST; ++code )
	{
		staticKeyTrans[code] = keyTranslations[code];
	}
#endif
}

void App::stop()
{
	_running = false;
}

void App::start()
{
#if defined( _WIN32 )
	main( __argc, __argv );
#endif
	if ( !_externalMain )
	{
		run();
		for ( int i = 0; i < _surfaceBaseDar.getCount(); ++i )
			_surfaceBaseDar[i]->setWindowedMode();
	}
}

void App::run()
{
	_running = true;
	while ( _running )
		internalTick();

	setMouseArena( 0, 0, 0, 0, false );
}

void App::externalTick()
{
	internalTick();
}

void App::internalTick()
{
	if ( getTimeMillis() < _nextTickMillis )
		return;

	platTick();

	int cursorX;
	int cursorY;
	getCursorPos( cursorX, cursorY );

	bool mouseOverAnySurface = false;
	for ( int i = 0; i < _surfaceBaseDar.getCount(); ++i )
	{
		updateMouseFocus( cursorX, cursorY, _surfaceBaseDar[i] );
		if ( _surfaceBaseDar[i]->isWithin( cursorX, cursorY ) )
			mouseOverAnySurface = true;
		_surfaceBaseDar[i]->setEmulatedCursorPos( cursorX, cursorY );
	}

	if ( !mouseOverAnySurface )
		setMouseFocus( NULL );

	if ( _mouseFocus != NULL )
		_mouseFocus->internalSetCursor();

	for ( int i = 0; i < _tickSignalDar.getCount(); ++i )
		_tickSignalDar[i]->ticked();

#if !defined( _WIN32 )
	if ( _keyFocus == NULL && _surfaceBaseDar.getCount() > 0 )
		_surfaceBaseDar[0]->getPanel()->requestFocus();
#endif
	if ( _keyFocus != NULL )
		_keyFocus->internalKeyFocusTicked();

	memset( _mousePressed, 0, sizeof( _mousePressed ) );
	memset( _mouseDoublePressed, 0, sizeof( _mouseDoublePressed ) );
	memset( _mouseReleased, 0, sizeof( _mouseReleased ) );
	memset( _keyPressed, 0, sizeof( _keyPressed ) );
	memset( _keyTyped, 0, sizeof( _keyTyped ) );
	memset( _keyReleased, 0, sizeof( _keyReleased ) );

	int focusedIndex;
	for ( focusedIndex = 0; focusedIndex < _surfaceBaseDar.getCount(); ++focusedIndex )
	{
		if ( _surfaceBaseDar[focusedIndex]->hasFocus() )
			break;
	}
	if ( focusedIndex == _surfaceBaseDar.getCount() )
		_wantedKeyFocus = NULL;

	// A focus change may delete the panel it is sent to, which clears App's
	// pointers to it.
	if ( _keyFocus != _wantedKeyFocus )
	{
		Panel* oldFocus = _keyFocus;
		if ( oldFocus != NULL )
		{
			oldFocus->internalFocusChanged( true );
			if ( _keyFocus == oldFocus )
				oldFocus->repaint();
		}

		Panel* newFocus = _wantedKeyFocus;
		if ( newFocus != NULL )
		{
			newFocus->internalFocusChanged( false );
			if ( _wantedKeyFocus == newFocus )
				newFocus->repaint();
		}
	}

	_keyFocus = _wantedKeyFocus;
	_buildMode = _wantedBuildMode;

	for ( int i = 0; i < _surfaceBaseDar.getCount(); ++i )
	{
		_surfaceBaseDar[i]->getPanel()->solveTraverse();
		_surfaceBaseDar[i]->applyChanges();
	}

	if ( _mouseArenaPanel != NULL )
	{
		SurfaceBase* surfaceBase = _mouseArenaPanel->getSurfaceBase();
		if ( surfaceBase != NULL )
		{
			int x;
			int y;
			surfaceBase->getPanel()->getPos( x, y );

			int x0;
			int y0;
			int x1;
			int y1;
			_mouseArenaPanel->getAbsExtents( x0, y0, x1, y1 );
			internalSetMouseArena( x0 + x, y0 + y, x + x1, y + y1, true );
		}
	}

	_nextTickMillis = _minimumTickMillisInterval + getTimeMillis();
}

void App::internalCursorMoved( int x, int y, SurfaceBase* surfaceBase )
{
	surfaceBase->getPanel()->localToScreen( x, y );
	if ( !_buildMode )
	{
		updateMouseFocus( x, y, surfaceBase );
		if ( _mouseFocus != NULL )
			_mouseFocus->internalCursorMoved( x, y );
	}
}

void App::updateMouseFocus( int x, int y, SurfaceBase* surfaceBase )
{
	if ( _mouseCapture == NULL )
	{
		if ( !surfaceBase->isWithin( x, y ) )
			return;

		Panel* panel = surfaceBase->getPanel()->isWithinTraverse( x, y );
		if ( panel == NULL )
			return;

		setMouseFocus( panel );
		return;
	}

	setMouseFocus( _mouseCapture );
}

void App::setMouseFocus( Panel* newMouseFocus )
{
	if ( _mouseFocus != newMouseFocus )
	{
		Panel* previous = _mouseFocus;
		_oldMouseFocus = previous;
		_mouseFocus = newMouseFocus;
		if ( previous != NULL )
			previous->internalCursorExited();
		if ( _mouseFocus != NULL )
			_mouseFocus->internalCursorEntered();
	}
}

// A panel that is on no surface gets the capture without a native one.
void App::setMouseCapture( Panel* panel )
{
	SurfaceBase* surfaceBase = panel != NULL ? panel->_surfaceBase : NULL;
	if ( capturedSurface != NULL && capturedSurface != surfaceBase )
		capturedSurface->enableMouseCapture( false );
	capturedSurface = surfaceBase;
	if ( surfaceBase != NULL )
		surfaceBase->enableMouseCapture( true );
	_mouseCapture = panel;
}

void App::setMouseArena( int x0, int y0, int x1, int y1, bool enabled )
{
	setMouseArena( NULL );
	internalSetMouseArena( x0, y0, x1, y1, enabled );
}

void App::setMouseArena( Panel* panel )
{
	_mouseArenaPanel = panel;
}

void App::requestFocus( Panel* panel )
{
	_wantedKeyFocus = panel;
}

Panel* App::getFocus()
{
	return _keyFocus;
}

void App::repaintAll()
{
	for ( int i = 0; i < _surfaceBaseDar.getCount(); ++i )
	{
		_surfaceBaseDar[i]->getPanel()->repaintAll();
		_surfaceBaseDar[i]->invalidate( _surfaceBaseDar[i]->getPanel() );
	}
}

void App::setScheme( Scheme* scheme )
{
	if ( scheme != NULL )
	{
		_scheme = scheme;
		repaintAll();
	}
}

Scheme* App::getScheme()
{
	return _scheme;
}

void App::enableBuildMode()
{
	_wantedBuildMode = true;
}

void App::reset()
{
	_mouseArenaPanel = NULL;
	_tickSignalDar.removeAll();
	_keyFocus = NULL;
	_mouseFocus = NULL;
	_mouseCapture = NULL;
	capturedSurface = NULL;
	_wantedKeyFocus = NULL;
	_buildMode = false;
	_wantedBuildMode = false;
	Font_Reset();

#if VGUI_ABI_COMPAT_MODE != 1
	// The default fonts keep their platform fonts, which Font_Reset forgets
	// but does not free.
	_ownScheme.scheme->restoreDefaults();
	setScheme( _ownScheme.scheme );
#else
	Scheme* scheme = new Scheme();
	setScheme( scheme );
#endif
}

#if VGUI_ABI_COMPAT_MODE != 1
App::OwnScheme::OwnScheme()
{
	scheme = new Scheme();
}

App::OwnScheme::OwnScheme( const OwnScheme& )
{
	scheme = new Scheme();
}

App::OwnScheme::~OwnScheme()
{
	delete scheme;
}

App::OwnScheme& App::OwnScheme::operator=( const OwnScheme& )
{
	return *this;
}
#endif

void App::surfaceBaseCreated( SurfaceBase* surfaceBase )
{
	if ( _surfaceBaseDar.hasElement( surfaceBase ) )
		return;

	_surfaceBaseDar.addElement( surfaceBase );
}

// Forgets every panel App points to, as the panels may be on this surface. The
// system releases the capture of a destroyed window by itself; the cursor clip
// is not tied to a window.
void App::surfaceBaseDeleted( SurfaceBase* surfaceBase )
{
	if ( capturedSurface == surfaceBase )
		capturedSurface = NULL;
	// A copied surface was never registered, and a surface whose root was
	// deleted was unregistered after App forgot the panels of that tree.
	if ( !_surfaceBaseDar.hasElement( surfaceBase ) )
		return;

	_surfaceBaseDar.removeElement( surfaceBase );
	if ( _mouseArenaPanel != NULL )
		internalSetMouseArena( 0, 0, 0, 0, false );
	_oldMouseFocus = NULL;
	_mouseFocus = NULL;
	_mouseCapture = NULL;
	_keyFocus = NULL;
	_wantedKeyFocus = NULL;
	_mouseArenaPanel = NULL;
}

#if VGUI_ABI_COMPAT_MODE != 1
// Also releases the native capture and cursor clip the panel holds.
void App::panelDeleted( Panel* panel )
{
	if ( _mouseCapture == panel )
		setMouseCapture( NULL );
	if ( _mouseArenaPanel == panel )
		setMouseArena( 0, 0, 0, 0, false );
	if ( _keyFocus == panel )
		_keyFocus = NULL;
	if ( _oldMouseFocus == panel )
		_oldMouseFocus = NULL;
	if ( _mouseFocus == panel )
		_mouseFocus = NULL;
	if ( _wantedKeyFocus == panel )
		_wantedKeyFocus = NULL;
}
#endif

void App::addTickSignal( TickSignal* s )
{
	if ( _tickSignalDar.hasElement( s ) )
		return;

	_tickSignalDar.addElement( s );
}

#if VGUI_ABI_COMPAT_MODE != 1
void App::removeTickSignal( TickSignal* s )
{
	_tickSignalDar.removeElement( s );
}
#endif

bool App::wasMousePressed( MouseCode code, Panel* )
{
	return _mousePressed[code];
}

bool App::wasMouseDoublePressed( MouseCode code, Panel* )
{
	return _mouseDoublePressed[code];
}

bool App::isMouseDown( MouseCode code, Panel* )
{
	return _mouseDown[code];
}

bool App::wasMouseReleased( MouseCode code, Panel* )
{
	return _mouseReleased[code];
}

bool App::wasKeyPressed( KeyCode code, Panel* panel )
{
	if ( panel == NULL || panel == _keyFocus )
		return _keyPressed[code];

	return false;
}

bool App::isKeyDown( KeyCode code, Panel* panel )
{
	if ( panel == NULL || panel == _keyFocus )
		return _keyDown[code];

	return false;
}

bool App::wasKeyTyped( KeyCode code, Panel* panel )
{
	if ( panel == NULL || panel == _keyFocus )
		return _keyTyped[code];

	return false;
}

bool App::wasKeyReleased( KeyCode code, Panel* panel )
{
	if ( panel == NULL || panel == _keyFocus )
		return _keyReleased[code];

	return false;
}

void App::internalMousePressed( MouseCode code, SurfaceBase* )
{
	_mousePressed[code] = true;
	_mouseDown[code] = true;
	if ( !_buildMode && _mouseFocus != NULL )
		_mouseFocus->internalMousePressed( code );
}

void App::internalMouseDoublePressed( MouseCode code, SurfaceBase* )
{
	_mouseDoublePressed[code] = true;
	if ( !_buildMode && _mouseFocus != NULL )
		_mouseFocus->internalMouseDoublePressed( code );
}

void App::internalMouseReleased( MouseCode code, SurfaceBase* )
{
	_mouseReleased[code] = true;
	_mouseDown[code] = false;
	if ( !_buildMode && _mouseFocus != NULL )
		_mouseFocus->internalMouseReleased( code );
}

void App::internalMouseWheeled( int delta, SurfaceBase* )
{
	if ( !_buildMode && _keyFocus != NULL )
		_keyFocus->internalMouseWheeled( delta );
}

void App::internalKeyPressed( KeyCode code, SurfaceBase* )
{
	if ( (unsigned int)code <= KEY_F12 )
	{
		_keyPressed[code] = true;
		_keyDown[code] = true;
		if ( !_buildMode && _keyFocus != NULL )
			_keyFocus->internalKeyPressed( code );
	}
}

void App::internalKeyTyped( KeyCode code, SurfaceBase* )
{
	if ( (unsigned int)code <= KEY_F12 )
	{
		_keyTyped[code] = true;
		if ( !_buildMode && _keyFocus != NULL )
			_keyFocus->internalKeyTyped( code );
	}
}

void App::internalKeyReleased( KeyCode code, SurfaceBase* )
{
	if ( (unsigned int)code <= KEY_F12 )
	{
		_keyReleased[code] = true;
		_keyDown[code] = false;
		if ( !_buildMode && _keyFocus != NULL )
			_keyFocus->internalKeyReleased( code );
	}
}

#if defined( _WIN32 )
char App::getKeyCodeChar( KeyCode code, bool shifted )
{
	return staticKeyTrans[code][shifted ? 1 : 0];
}

void App::getKeyCodeText( KeyCode code, char* buf, int buflen )
{
	const int keyNameOffset = 2;

	if ( buf != NULL )
	{
		for ( int i = 0; i < buflen; ++i )
		{
			const char ch = staticKeyTrans[code][i + keyNameOffset];
			buf[i] = ch;
			if ( ch == '\0' )
			{
				break;
			}
		}
	}
}
#else
char App::getKeyCodeChar( KeyCode, bool )
{
	return '\0';
}

void App::getKeyCodeText( KeyCode, char* buf, int buflen )
{
	if ( buf != NULL && buflen > 0 )
	{
		buf[0] = '\0';
	}
}
#endif

void App::setCursorOveride( Cursor* cursor )
{
	_cursorOveride = cursor;
}

Cursor* App::getCursorOveride()
{
	return _cursorOveride;
}

void App::setMinimumTickMillisInterval( int interval )
{
	_minimumTickMillisInterval = interval;
}

}
