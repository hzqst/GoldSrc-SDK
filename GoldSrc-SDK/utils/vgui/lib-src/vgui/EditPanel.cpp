#include "VGUI_EditPanel.h"
#include "VGUI_InputSignal.h"
#include "VGUI_Font.h"
#include "VGUI_App.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

void vgui::EditPanel::paintBackground()
{
	int wide, tall;
	getPaintSize( wide, tall );
	drawSetColor( Scheme::sc_white );
	drawFilledRect( 0, 0, wide, tall );
}

void vgui::EditPanel::paint()
{
	Font* font = _font;
	if ( font == NULL )
	{
		font = getApp()->getScheme()->getFont( Scheme::sf_primary1 );
	}
	drawSetTextFont( font );
	drawSetTextColor( Scheme::sc_black );
	int y = 0;
	for ( int row = 0; row < _lineDarDar.getCount(); ++row )
	{
		Dar<char>* line = _lineDarDar[row];
		int x = 0;
		int cursorX = 0;
		for ( int column = 0; column < line->getCount(); ++column )
		{
			const char ch = ( *line )[column];
			drawPrintChar( x, y, ch );
			if ( _cursor[0] == column && _cursor[1] == row )
			{
				cursorX = x;
			}
			int a, b, c;
			font->getCharABCwide( ch, a, b, c );
			x += a + b + c;
		}
		if ( _cursor[1] == row && _cursorBlink )
		{
			if ( _cursor[0] >= line->getCount() )
			{
				int a, b, c;
				font->getCharABCwide( ' ', a, b, c );
				cursorX = x + ( _cursor[0] - line->getCount() ) * ( a + b + c );
			}
			drawSetColor( 255, 0, 0, 0 );
			drawFilledRect( cursorX - 1, y, cursorX + 1, y + font->getTall() );
		}
		y += font->getTall();
	}
}

namespace
{

using namespace vgui;

class FooDefaultEditPanelSignal : public InputSignal
{
public:
	FooDefaultEditPanelSignal( EditPanel* editPanel )
	{
		_editPanel = editPanel;
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

protected:
	EditPanel*	_editPanel;
};

void FooDefaultEditPanelSignal::cursorMoved( int, int, Panel* )
{
}

void FooDefaultEditPanelSignal::cursorEntered( Panel* )
{
}

void FooDefaultEditPanelSignal::cursorExited( Panel* )
{
}

void FooDefaultEditPanelSignal::mousePressed( MouseCode, Panel* )
{
	_editPanel->requestFocus();
	_editPanel->repaint();
}

void FooDefaultEditPanelSignal::mouseDoublePressed( MouseCode, Panel* )
{
}

void FooDefaultEditPanelSignal::mouseReleased( MouseCode, Panel* )
{
}

void FooDefaultEditPanelSignal::mouseWheeled( int, Panel* )
{
}

void FooDefaultEditPanelSignal::keyPressed( KeyCode, Panel* )
{
}

void FooDefaultEditPanelSignal::keyTyped( KeyCode code, Panel* )
{
	bool shift = ( _editPanel->isKeyDown( KEY_LSHIFT ) || _editPanel->isKeyDown( KEY_RSHIFT ) );

	switch ( code )
	{
	case KeyCode::KEY_ENTER:
		_editPanel->doCursorNewLine();
		return;
	case KeyCode::KEY_BACKSPACE:
		_editPanel->doCursorBackspace();
		return;
	case KeyCode::KEY_DELETE:
		_editPanel->doCursorDelete();
		return;
	case KeyCode::KEY_HOME:
		_editPanel->doCursorToStartOfLine();
		return;
	case KeyCode::KEY_END:
		_editPanel->doCursorToEndOfLine();
		return;
	case KeyCode::KEY_LSHIFT:
	case KeyCode::KEY_RSHIFT:
	case KeyCode::KEY_LALT:
	case KeyCode::KEY_RALT:
	case KeyCode::KEY_LCONTROL:
	case KeyCode::KEY_RCONTROL:
		return;
	case KeyCode::KEY_UP:
		_editPanel->doCursorUp();
		return;
	case KeyCode::KEY_LEFT:
		_editPanel->doCursorLeft();
		return;
	case KeyCode::KEY_DOWN:
		_editPanel->doCursorDown();
		return;
	case KeyCode::KEY_RIGHT:
		_editPanel->doCursorRight();
		return;
	default:
		_editPanel->doCursorInsertChar( _editPanel->getApp()->getKeyCodeChar( code, shift ) );
		return;
	}
}

void FooDefaultEditPanelSignal::keyReleased( KeyCode, Panel* )
{
}

void FooDefaultEditPanelSignal::keyFocusTicked( Panel* )
{
	bool blink;
	int nextBlinkTime;
	_editPanel->getCursorBlink( blink, nextBlinkTime );

	if ( _editPanel->getApp()->getTimeMillis() > nextBlinkTime )
	{
		_editPanel->setCursorBlink( !blink );
	}
}

}

namespace vgui
{

EditPanel::EditPanel( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall ), _lineDarDar( 4 )
{
	_font = NULL;
	_cursor[0] = 0;
	_cursor[1] = 0;
	setCursorBlink( true );
	addInputSignal( new FooDefaultEditPanelSignal( this ) );
	getLine( 0 );
}

#if VGUI_ABI_COMPAT_MODE != 1
EditPanel::EditPanel( const EditPanel& other ) : Panel( other ), _lineDarDar( 4 )
{
	copyFrom( other );
}

EditPanel::~EditPanel()
{
	for ( int i = 0; i < _lineDarDar.getCount(); ++i )
		delete _lineDarDar[i];
}

EditPanel& EditPanel::operator=( const EditPanel& other )
{
	if ( this != &other )
	{
		Panel::operator=( other );
		copyFrom( other );
	}

	return *this;
}

void EditPanel::copyFrom( const EditPanel& other )
{
	for ( int i = 0; i < _lineDarDar.getCount(); ++i )
		delete _lineDarDar[i];
	_lineDarDar.removeAll();

	// Dar has no const accessors; the source lines are only read.
	Dar<Dar<char>*>& lines = const_cast<Dar<Dar<char>*>&>( other._lineDarDar );
	for ( int i = 0; i < lines.getCount(); ++i )
		_lineDarDar.addElement( new Dar<char>( *lines[i] ) );

	_cursor[0] = other._cursor[0];
	_cursor[1] = other._cursor[1];
	_cursorBlink = other._cursorBlink;
	_cursorNextBlinkTime = other._cursorNextBlinkTime;
	_font = other._font;
}
#endif

void EditPanel::doCursorUp()
{
	int x = _cursor[0];
	Dar<char>* above = getLine( _cursor[1] - 1 );
	Dar<char>* current = getLine( _cursor[1] );
	x = spatialCharOffsetBetweenTwoLines( current, above, x );
	_cursor[0] = x;
	int y = _cursor[1] - 1;
	if ( y < 0 )
	{
		y = 0;
	}
	_cursor[1] = y;
	setCursorBlink( true );
	repaint();
}

void EditPanel::doCursorDown()
{
	int visible = getVisibleLineCount();
	int x = _cursor[0];
	Dar<char>* below = getLine( _cursor[1] + 1 );
	Dar<char>* current = getLine( _cursor[1] );
	x = spatialCharOffsetBetweenTwoLines( current, below, x );
	_cursor[0] = x;

	int lineCount = getLineCount();
	if ( visible > lineCount )
	{
		visible = getLineCount();
	}

	int next = _cursor[1] + 1;
	if ( visible > next )
	{
		_cursor[1] = next;
		setCursorBlink( true );
		repaint();
	}
}

void EditPanel::doCursorLeft()
{
	int x = _cursor[0] - 1;
	if ( x < 0 )
	{
		x = 0;
	}
	_cursor[0] = x;
	setCursorBlink( true );
	repaint();
}

void EditPanel::doCursorRight()
{
	++_cursor[0];
	setCursorBlink( true );
	repaint();
}

void EditPanel::doCursorToStartOfLine()
{
	_cursor[0] = 0;
	setCursorBlink( true );
	repaint();
}

void EditPanel::doCursorToEndOfLine()
{
	Dar<char>* line = getLine( _cursor[1] );
	if ( line != NULL )
	{
		_cursor[0] = line->getCount();
		setCursorBlink( true );
		repaint();
	}
}

void EditPanel::doCursorInsertChar( char ch )
{
	Dar<char>* line = getLine( _cursor[1] );
	if ( line == NULL )
	{
		return;
	}

	shiftLineRight( line, _cursor[0], 1 );
	setChar( line, _cursor[0], ch );
	doCursorRight();
	repaint();
}

void EditPanel::doCursorBackspace()
{
	Dar<char>* line = getLine( _cursor[1] );
	if ( line == NULL )
	{
		return;
	}

	if ( _cursor[0] != 0 )
	{
		shiftLineLeft( line, _cursor[0], 1 );
		doCursorLeft();
	}
	else
	{
		Dar<char>* previous = getLine( _cursor[1] - 1 );
		if ( previous == NULL )
		{
			return;
		}

		int joined = previous->getCount();
		for ( int index = 0; index < line->getCount(); ++index )
		{
			setChar( previous, joined + index, ( *line )[index] );
		}

		_lineDarDar.removeElementAt( _cursor[1] );
		doCursorUp();
		_cursor[0] = joined;
	}
	repaint();
}

void EditPanel::doCursorNewLine()
{
	Dar<char>* line = getLine( _cursor[1] );
	if ( line == NULL )
	{
		return;
	}

	Dar<char>* fresh = new Dar<char>( 4 );
	_lineDarDar.insertElementAt( fresh, _cursor[1] + 1 );

	int x = _cursor[0];
	for ( int index = x; index < line->getCount(); ++index )
	{
		setChar( fresh, index - x, ( *line )[index] );
	}

	line->setCount( x );
	_cursor[0] = 0;
	doCursorDown();
	repaint();
}

void EditPanel::doCursorDelete()
{
	Dar<char>* line = getLine( _cursor[1] );
	if ( line == NULL )
	{
		return;
	}

	doCursorRight();
	shiftLineLeft( line, _cursor[0], 1 );
	doCursorLeft();
	repaint();
}

void EditPanel::doCursorPrintf( char* format, ... )
{
	char buf[8192] = {};
	va_list args;
	va_start( args, format );
	int ret = vsnprintf( buf, sizeof( buf ), format, args );
	va_end( args );
	if ( ret < 0 )
	{
		buf[0] = 0;
	}

	for ( int i = 0; i < (int)sizeof( buf ); i++ )
	{
		char ch = buf[i];
		if ( ch == 0 )
		{
			break;
		}
		if ( ch == '\n' )
		{
			doCursorNewLine();
		}
		else
		{
			doCursorInsertChar( ch );
		}
	}
	repaint();
}

int EditPanel::getLineCount()
{
	return _lineDarDar.getCount();
}

int EditPanel::getVisibleLineCount()
{
	int wide;
	int tall;
	getPaintSize( wide, tall );

	Font* font = _font;
	if ( font == NULL )
	{
		font = getApp()->getScheme()->getFont( Scheme::sf_primary1 );
	}
	return tall / font->getTall();
}

void EditPanel::setCursorBlink( bool state )
{
	_cursorBlink = state;
	_cursorNextBlinkTime = getApp()->getTimeMillis() + 400;
	repaint();
}

void EditPanel::setFont( Font* font )
{
	_font = font;
	repaint();
}

void EditPanel::getText( int lineIndex, int offset, char* buf, int bufLen )
{
	if ( buf == NULL || bufLen <= 0 )
	{
		return;
	}

	if ( lineIndex < 0 || lineIndex >= _lineDarDar.getCount() )
	{
		return;
	}

	Dar<char>* line = _lineDarDar[lineIndex];

	int i = 0;
	for ( int x = offset; x < line->getCount(); x++ )
	{
		if ( i >= bufLen - 1 )
		{
			break;
		}
		buf[x - offset] = ( *line )[x];
		i++;
	}
	buf[i] = 0;
}

void EditPanel::addLine()
{
}

Dar<char>* EditPanel::getLine( int lineIndex )
{
	if ( lineIndex < 0 )
	{
		return NULL;
	}

	if ( lineIndex == 0 && _lineDarDar.getCount() == 0 )
	{
		Dar<char>* line = new Dar<char>( 4 );
		_lineDarDar.addElement( line );
		return line;
	}

	if ( lineIndex >= _lineDarDar.getCount() )
	{
		return NULL;
	}
	return _lineDarDar[lineIndex];
}

void EditPanel::setChar( Dar<char>* lineDar, int x, char ch, char fill )
{
	if ( lineDar == NULL || x < 0 )
	{
		return;
	}
	const int oldCount = lineDar->getCount();
	if ( x >= oldCount )
	{
		lineDar->ensureCapacity( x + 1 );
		lineDar->setCount( x + 1 );
		for ( int i = oldCount; i < x; ++i )
		{
			lineDar->setElementAt( fill, i );
		}
	}
	lineDar->setElementAt( ch, x );
}

void EditPanel::setChar( Dar<char>* lineDar, int x, char ch )
{
	setChar( lineDar, x, ch, ' ' );
}

void EditPanel::shiftLineLeft( Dar<char>* lineDar, int x, int count )
{
	if ( x <= 0 || x > lineDar->getCount() || count < 0 )
	{
		return;
	}

	for ( int i = x; i < lineDar->getCount(); i++ )
	{
		if ( i - count >= 0 )
		{
			setChar( lineDar, i - count, ( *lineDar )[i] );
		}
	}

	lineDar->setCount( lineDar->getCount() - count );
}

void EditPanel::shiftLineRight( Dar<char>* lineDar, int x, int count )
{
	if ( x < 0 || count < 0 )
	{
		return;
	}

	for ( int i = lineDar->getCount() - 1; i >= x; i-- )
	{
		setChar( lineDar, i + count, ( *lineDar )[i] );
	}
}

int EditPanel::spatialCharOffsetBetweenTwoLines( Dar<char>* srcDar, Dar<char>* dstDar, int x )
{
	if ( srcDar == NULL || dstDar == NULL )
	{
		return x;
	}

	Font* font = _font;
	if ( font == NULL )
	{
		font = getApp()->getScheme()->getFont( Scheme::sf_primary1 );
	}

	int x1 = 0;
	for ( int i = 0; i != x; i++ )
	{
		char ch = ' ';
		if ( i < srcDar->getCount() )
		{
			ch = ( *srcDar )[i];
		}

		int a;
		int b;
		int c;
		font->getCharABCwide( ch, a, b, c );
		x1 += a + b + c;
	}

	int x0 = 0;
	for ( int i = 0;; i++ )
	{
		char ch = ' ';
		if ( i < dstDar->getCount() )
		{
			ch = ( *dstDar )[i];
		}

		int a;
		int b;
		int c;
		font->getCharABCwide( ch, a, b, c );

		int wide = a + b + c;
		int mid = x0 + ( wide / 2 );
		if ( x1 >= x0 && x1 < mid )
		{
			return i;
		}
		if ( x1 >= mid && x1 < x0 + wide )
		{
			return i + 1;
		}
		x0 += wide;
	}
}

void EditPanel::getCursorBlink( bool& blink, int& nextBlinkTime )
{
	blink = _cursorBlink;
	nextBlinkTime = _cursorNextBlinkTime;
}

}
