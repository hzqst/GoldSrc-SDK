#include "VGUI_TextEntry.h"
#include "VGUI_App.h"
#include "VGUI_Font.h"
#include "VGUI_ActionSignal.h"
#include "VGUI_FocusChangeSignal.h"

#include <cstdlib>
#include <cstring>

#define TEXTENTRY_TEXT_INSET	3

namespace
{

using namespace vgui;

class FooDefaultTextEntrySignal : public FocusChangeSignal
{
public:
	FooDefaultTextEntrySignal( TextEntry* textEntry ) : _textEntry( textEntry )
	{
	}

	virtual void focusChanged( bool lost, Panel* panel ) override;

protected:
	TextEntry*	_textEntry;
};

void FooDefaultTextEntrySignal::focusChanged( bool, Panel* )
{
	_textEntry->resetCursorBlink();
	_textEntry->doSelectNone();
}

}

namespace vgui
{

TextEntry::TextEntry( const char* text, int x, int y, int wide, int tall )
	: Panel( x, y, wide, tall ), _lineDar( 4 ), _actionSignalDar( 4 )
{
	_font = NULL;
	_hideText = false;
	_cursorPos = 0;
	_cursorBlinkRate = 400;
	_select[0] = -1;
	_select[1] = -1;
	resetCursorBlink();
	setText( text, (int)std::strlen( text ) );
	doGotoEndOfLine();
	addInputSignal( this );
	addFocusChangeSignal( new FooDefaultTextEntrySignal( this ) );
}

void TextEntry::cursorMoved( int, int, Panel* )
{
}

void TextEntry::cursorEntered( Panel* )
{
}

void TextEntry::cursorExited( Panel* )
{
}

void TextEntry::mousePressed( MouseCode, Panel* )
{
	resetCursorBlink();
	requestFocus();
	repaint();
}

void TextEntry::mouseDoublePressed( MouseCode, Panel* )
{
}

void TextEntry::mouseReleased( MouseCode, Panel* )
{
}

void TextEntry::mouseWheeled( int, Panel* )
{
}

void TextEntry::keyPressed( KeyCode, Panel* )
{
}

void TextEntry::keyReleased( KeyCode, Panel* )
{
}

void TextEntry::keyFocusTicked( Panel* )
{
	const long now = getApp()->getTimeMillis();
	if ( now > _cursorNextBlinkTime )
	{
		_cursorBlink = !_cursorBlink;
		_cursorNextBlinkTime = now + _cursorBlinkRate;
		repaint();
	}
}

void TextEntry::keyTyped( KeyCode code, Panel* )
{
	const bool shifted = isKeyDown( KEY_LSHIFT ) || isKeyDown( KEY_RSHIFT );
	if ( isKeyDown( KEY_LCONTROL ) || isKeyDown( KEY_RCONTROL ) )
	{
		if ( code == KEY_C )
		{
			doCopySelected();
		}
		else if ( code == KEY_V )
		{
			doPaste();
		}
	}
	else
	{
		switch ( code )
		{
		case KEY_ENTER:
		case KEY_TAB:
		case KEY_LSHIFT:
		case KEY_RSHIFT:
			break;
		case KEY_BACKSPACE:
			doBackspace();
			break;
		case KEY_INSERT:
			if ( shifted )
			{
				doPaste();
			}
			break;
		case KEY_DELETE:
			if ( shifted )
			{
				doDeleteSelected();
			}
			else
			{
				doDelete();
			}
			break;
		case KEY_HOME:
			doGotoFirstOfLine();
			break;
		case KEY_END:
			doGotoEndOfLine();
			break;
		case KEY_LEFT:
			doGotoLeft();
			break;
		case KEY_RIGHT:
			doGotoRight();
			break;
		default:
		{
			const bool characterShifted = isKeyDown( KEY_LSHIFT ) || isKeyDown( KEY_RSHIFT );
			const char ch = getApp()->getKeyCodeChar( code, characterShifted );
			if ( ch != '\0' )
			{
				doInsertChar( ch );
			}
			break;
		}
		}
	}
	_select[1] = _cursorPos;
	if ( code == KEY_ENTER )
	{
		fireActionSignal();
	}
}

void TextEntry::paintBackground()
{
	Font* font = _font;
	if ( font == NULL )
	{
		font = getApp()->getScheme()->getFont( Scheme::sf_primary1 );
	}
	const int fontTall = font->getTall();
	int selectionStart;
	int selectionEnd;
	if ( getSelectedPixelRange( selectionStart, selectionEnd ) )
	{
		drawSetColor( Scheme::sc_white );
		drawFilledRect( 0, 0, selectionStart, fontTall + 1 );
		drawFilledRect( selectionEnd, 0, _size[0], fontTall + 1 );
		drawSetColor( 0, 0, 200, 0 );
		drawFilledRect( selectionStart, 0, selectionEnd, fontTall + 1 );
	}
	else
	{
		drawSetColor( Scheme::sc_white );
		drawFilledRect( 0, 0, _size[0], _size[1] );
	}
	drawSetTextFont( font );
	drawSetTextColor( Scheme::sc_black );
	drawSetTextPos( TEXTENTRY_TEXT_INSET, 0 );
	for ( int i = 0; i < _lineDar.getCount(); ++i )
	{
		drawPrintChar( _hideText ? '*' : _lineDar[i] );
	}
	if ( hasFocus() )
	{
		drawSetColor( Scheme::sc_black );
		drawFilledRect( 0, 0, _size[0], 1 );
		drawFilledRect( 0, _size[1] - 1, _size[0], _size[1] );
		drawFilledRect( 0, 1, 1, _size[1] - 1 );
		drawFilledRect( _size[0] - 1, 1, _size[0], _size[1] - 1 );
		if ( !_cursorBlink )
		{
			const int cursorX = cursorToPixelSpace( _cursorPos );
			drawSetColor( Scheme::sc_black );
			drawFilledRect( cursorX + TEXTENTRY_TEXT_INSET, 2,
				cursorX + TEXTENTRY_TEXT_INSET + 1, fontTall - 1 );
		}
	}
}

void TextEntry::doInsertChar( char ch )
{
	const int newCount = _lineDar.getCount() + 1;
	_lineDar.setCount( newCount );
	for ( int i = _lineDar.getCount() - 1; i >= _cursorPos; --i )
	{
		setCharAt( _lineDar[i], i + 1 );
	}
	setCharAt( ch, _cursorPos );
	++_cursorPos;
	resetCursorBlink();
	repaint();
}

void TextEntry::doBackspace()
{
	if ( _cursorPos == 0 || _lineDar.getCount() == 0 )
	{
		return;
	}
	for ( int i = _cursorPos; i < _lineDar.getCount(); ++i )
	{
		setCharAt( _lineDar[i], i - 1 );
	}
	const int newCount = _lineDar.getCount() - 1;
	_lineDar.setCount( newCount );
	--_cursorPos;
	resetCursorBlink();
	repaint();
}

void TextEntry::doDelete()
{
	if ( _lineDar.getCount() == 0 || _cursorPos == _lineDar.getCount() )
	{
		return;
	}
	for ( int i = _cursorPos + 1; i < _lineDar.getCount(); ++i )
	{
		setCharAt( _lineDar[i], i - 1 );
	}
	const int newCount = _lineDar.getCount() - 1;
	_lineDar.setCount( newCount );
	resetCursorBlink();
	repaint();
}

void TextEntry::doCopySelected()
{
	int start;
	int end;
	if ( !getSelectedRange( start, end ) )
	{
		return;
	}
	char text[256];
	int count = 0;
	for ( int i = start; i < end && count < 199; ++i )
	{
		text[count++] = _lineDar[i];
	}
	text[count] = '\0';
	getApp()->setClipboardText( text, count );
}

void TextEntry::doPaste()
{
	char text[256];
	const int count = getApp()->getClipboardText( 0, text, sizeof( text ) ) - 1;
	for ( int i = 0; i < count; ++i )
	{
		doInsertChar( text[i] );
	}
}

void TextEntry::doPasteSelected()
{
	doDeleteSelected();
	doPaste();
}

void TextEntry::resetCursorBlink()
{
	_cursorBlink = false;
	_cursorNextBlinkTime = getApp()->getTimeMillis() + _cursorBlinkRate;
}

void TextEntry::doSelectNone()
{
	_select[0] = -1;
	repaint();
}

void TextEntry::setTextHidden( bool state )
{
	_hideText = state;
	repaint();
}

void TextEntry::selectCheck()
{
	if ( isKeyDown( KEY_LSHIFT ) || isKeyDown( KEY_RSHIFT ) )
	{
		if ( _select[0] == -1 )
		{
			_select[0] = _cursorPos;
		}
	}
	else
	{
		_select[0] = -1;
	}
}

void TextEntry::doGotoLeft()
{
	selectCheck();
	--_cursorPos;
	if ( _cursorPos < 0 )
	{
		_cursorPos = 0;
	}
	resetCursorBlink();
	repaint();
}

void TextEntry::doGotoRight()
{
	selectCheck();
	++_cursorPos;
	if ( _cursorPos > _lineDar.getCount() )
	{
		_cursorPos = _lineDar.getCount();
	}
	resetCursorBlink();
	repaint();
}

void TextEntry::doGotoFirstOfLine()
{
	selectCheck();
	_cursorPos = 0;
	resetCursorBlink();
	repaint();
}

void TextEntry::doGotoEndOfLine()
{
	selectCheck();
	_cursorPos = _lineDar.getCount();
	resetCursorBlink();
	repaint();
}

int TextEntry::cursorToPixelSpace( int cursorPos )
{
	Font* font = _font;
	if ( font == NULL )
	{
		font = getApp()->getScheme()->getFont( Scheme::sf_primary1 );
	}
	int width = 0;
	for ( int i = 0; i < _lineDar.getCount(); ++i )
	{
		if ( i == cursorPos )
		{
			break;
		}
		int a;
		int b;
		int c;
		font->getCharABCwide( _hideText ? '*' : _lineDar[i], a, b, c );
		width += a + b + c;
	}
	return width;
}

bool TextEntry::getSelectedRange( int& cx0, int& cx1 )
{
	if ( _select[0] == -1 )
	{
		return false;
	}

	cx0 = _select[0];
	cx1 = _select[1];
	if ( cx1 < cx0 )
	{
		int temp = cx0;
		cx0 = cx1;
		cx1 = temp;
	}
	return true;
}

void TextEntry::doDeleteSelected()
{
}

void TextEntry::getText( int offset, char* buf, int bufLen )
{
	if ( buf == NULL || bufLen <= 0 )
	{
		return;
	}

	for ( int i = offset; i < bufLen - 1; i++ )
	{
		buf[i - offset] = 0;
		if ( i >= _lineDar.getCount() )
		{
			break;
		}
		buf[i - offset] = _lineDar[i];
	}
	buf[bufLen - 1] = 0;
}

void TextEntry::fireActionSignal()
{
	for ( int i = 0; i < _actionSignalDar.getCount(); ++i )
	{
		_actionSignalDar[i]->actionPerformed( this );
	}
}

void TextEntry::setFont( Font* font )
{
	_font = font;
}

void TextEntry::addActionSignal( ActionSignal* s )
{
	_actionSignalDar.putElement( s );
}

bool TextEntry::getSelectedPixelRange( int& cx0, int& cx1 )
{
	if ( !getSelectedRange( cx0, cx1 ) )
	{
		return false;
	}

	cx0 = cursorToPixelSpace( cx0 );
	cx1 = cursorToPixelSpace( cx1 );
	return true;
}

void TextEntry::setCharAt( char ch, int index )
{
	if ( index < 0 )
	{
		return;
	}
	_lineDar.ensureCapacity( index + 1 );
	_lineDar.setElementAt( ch, index );
}

void TextEntry::setText( const char* text, int textLen )
{
	_lineDar.removeAll();
	if ( text != NULL )
	{
		_lineDar.ensureCapacity( textLen );
		for ( int i = 0; i < textLen; ++i )
		{
			_lineDar.addElement( text[i] );
			setCharAt( text[i], i );
		}
	}
	doGotoEndOfLine();
}

}
