//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "TextEntry.h"

#include "Menu.h"
#include "ScrollBar.h"
#include "controls.h"

#include "ILocalize.h"
#include "../../../engine/controls.h"
#include "vgui/Cursor.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"

#include <cstdio>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>

#define DRAW_OFFSET_X 3
#define DRAW_OFFSET_Y 1
#define BUFFER_SIZE 999999

int getCharWidth( vgui2::HFont font, wchar_t ch )
{
	if ( !iswprint( ch ) )
		return 0;

	int a = 0;
	int b = 0;
	int c = 0;
	vgui2::surface()->GetCharABCwide( font, ch, a, b, c );
	return a + b + c;
}

namespace vgui2
{


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TextEntry::TextEntry( Panel* parent, const char* panelName ) : Panel( parent, panelName )
{
	_font = 0;
	_cursorPos = 0;
	_currentStartIndex = 0;
	_currentStartLine = 0;
	_cursorBlinkRate = 400;
	_vertScrollBar = NULL;
	_maxCharCount = -1;
	m_bAllowNumericInputOnly = false;
	m_bAllowNonAsciiCharacters = false;
	m_bAutoProgressOnHittingCharLimit = false;
	m_bUseFallbackFont = false;
	_hideText = false;
	_editable = false;
	_verticalScrollbar = false;
	_horizScrollingAllowed = true;
	_cursorIsAtEnd = false;
	_putCursorAtEnd = false;
	_multiline = false;
	_mouseSelection = false;
	_mouseDragSelection = false;
	_catchEnterKey = false;
	_charCount = 0;
	_wrap = false; // don't wrap by default
	_sendNewLines = false; // don't pass on a newline msg by default
	_drawWidth = 0;
	m_hFallbackFont = 0;

	//a -1 for _select[0] means that the selection is empty
	_select[0] = -1;
	_select[1] = -1;
	m_pEditMenu = NULL;
	_cursorBlink = false;
	_dataChanged = false;
	_pixelsIndent = 0;
	_undoCursorPos = 0;
	_mouseSelectCursorStart = 0;
	_cursorNextBlinkTime = _cursorBlinkRate + system()->GetTimeMillis();
	SetCursor( dc_ibeam );
	GotoTextEnd();
	_editable = true;

	// initialize the line break array
	m_LineBreaks.AddToTail( BUFFER_SIZE );

	_selectAllOnFirstFocus = false;
	_recalculateBreaksIndex = 0;

	if ( IsProportional() )
	{
		int width = 0;
		int height = 0;
		int screenWide = 0;
		int screenTall = 0;
		surface()->GetProportionalBase( width, height );
		surface()->GetScreenSize( screenWide, screenTall );
		_scrollBarSize = (int)( (double)screenWide / (double)width * 18.0 );
	}
	else
	{
		_scrollBarSize = 18;
	}
}

TextEntry::~TextEntry()
{
	delete m_pEditMenu;
}

//-----------------------------------------------------------------------------
// Purpose: sets the color of the background when the control is disabled
// Input  : col - 
//-----------------------------------------------------------------------------
void TextEntry::SetDisabledBgColor( Color col )
{
	_disabledBgColor = col;
}

//-----------------------------------------------------------------------------
// Purpose: Hides the text buffer so it will not be drawn
//-----------------------------------------------------------------------------
void TextEntry::SetTextHidden( bool bHideText )
{
	_hideText = bHideText;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether text wraps around multiple lines or not
// Input  : state - true or false
//-----------------------------------------------------------------------------
void TextEntry::SetMultiline( bool state )
{
	_multiline = state;
}

//-----------------------------------------------------------------------------
// Purpose: sets whether or not the edit catches and stores ENTER key presses
//-----------------------------------------------------------------------------
void TextEntry::SetCatchEnterKey( bool state )
{
	_catchEnterKey = state;
}

//-----------------------------------------------------------------------------
// Purpose: sets _editable flag
// Input  : state - true or false
//-----------------------------------------------------------------------------
void TextEntry::SetEditable( bool state )
{
	_editable = state;
}

//-----------------------------------------------------------------------------
// Purpose: If a cursor enters the window, we are not elegible for 
//          MouseFocusTicked events
//-----------------------------------------------------------------------------
void TextEntry::OnCursorEntered()
{
	_mouseDragSelection = false; // outside of window dont recieve drag scrolling ticks
}

//-----------------------------------------------------------------------------
// Purpose: When the cursor is outside the window, if we are holding the mouse
//			button down, then we want the window to scroll the text one char at a time
//			using Ticks
//-----------------------------------------------------------------------------
void TextEntry::OnCursorExited() // outside of window recieve drag scrolling ticks
{
	if ( _mouseSelection )
		_mouseDragSelection = true;
}

//-----------------------------------------------------------------------------
// Purpose: Turn off text selection code when mouse button is not down
//-----------------------------------------------------------------------------
void TextEntry::OnMouseCaptureLost()
{
	_mouseSelection = false;
}

//-----------------------------------------------------------------------------
// Purpose: set the maximum number of chars in the text buffer
//-----------------------------------------------------------------------------
void TextEntry::SetMaximumCharCount( int maxChars )
{
	_maxCharCount = maxChars;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
int TextEntry::GetMaximumCharCount()
{
	return _maxCharCount;
}

void TextEntry::SetAutoProgressOnHittingCharLimit( bool state )
{
	m_bAutoProgressOnHittingCharLimit = state;
}

//-----------------------------------------------------------------------------
// Purpose: set whether to wrap the text buffer
//-----------------------------------------------------------------------------
void TextEntry::SetWrap( bool wrap )
{
	_wrap = wrap;
}

//-----------------------------------------------------------------------------
// Purpose: set whether to pass newline msgs to parent
//-----------------------------------------------------------------------------
void TextEntry::SendNewLine( bool send )
{
	_sendNewLines = send;
}

//-----------------------------------------------------------------------------
// Purpose: Declare a selection empty
//-----------------------------------------------------------------------------
void TextEntry::SelectNone()
{
	// tag the selection as empty
	_select[0] = -1;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Load in the selection range so cx0 is the Start and cx1 is the end
//			from smallest to highest (right to left)
//-----------------------------------------------------------------------------
bool TextEntry::GetSelectedRange( int& cx0, int& cx1 )
{
	if ( _select[0] == -1 )
		return false;

	// sort the two position so cx0 is the smallest
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

//-----------------------------------------------------------------------------
// Purpose: Set the font of the buffer text 
// Input:	font to change to
//-----------------------------------------------------------------------------
void TextEntry::SetFont( HFont font )
{
	_font = font;
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the scrollbar slider is moved
//-----------------------------------------------------------------------------
void TextEntry::OnSliderMoved()
{
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TextEntry::OnSetText( const wchar_t* text )
{
	SetText( text );
}

// moves x,y to the Start of the next line of text
void TextEntry::AddAnotherLine( int& cx, int& cy )
{
	cx = _pixelsIndent + DRAW_OFFSET_X;
	cy += surface()->GetFontTall( _font ) + DRAW_OFFSET_Y;
}

//-----------------------------------------------------------------------------
// Purpose: Check if we are selecting text (so we can highlight it)
//-----------------------------------------------------------------------------
void TextEntry::SelectCheck()
{
	if ( HasFocus() && ( input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT ) ) )
	{
		if ( _select[0] == -1 )
			_select[0] = _cursorPos;
	}
	else
	{
		_select[0] = -1;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the text array - convert ANSI text to unicode and pass to unicode function
//-----------------------------------------------------------------------------
void TextEntry::SetText( const char* text )
{
	if ( text == NULL )
		text = "";
	if ( text[0] == '#' )
	{
		wchar_t* localized = localize()->Find( text );
		if ( localized != NULL )
		{
			SetText( localized );
			return;
		}
	}

	wchar_t unicode[1024];
	localize()->ConvertANSIToUnicode( text, unicode, sizeof( unicode ) );
	SetText( unicode );
}

//-----------------------------------------------------------------------------
// Purpose: Restarts the time of the next cursor blink
//-----------------------------------------------------------------------------
void TextEntry::ResetCursorBlink()
{
	_cursorBlink = false;
	_cursorNextBlinkTime = _cursorBlinkRate + system()->GetTimeMillis();
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame the entry has keyboard focus; 
//          blinks the text cursor
//-----------------------------------------------------------------------------
void TextEntry::OnKeyFocusTicked()
{
	int now = system()->GetTimeMillis();
	if ( now > _cursorNextBlinkTime )
	{
		_cursorBlink = !_cursorBlink;
		_cursorNextBlinkTime = _cursorBlinkRate + now;
		Repaint();
	}
}

void TextEntry::LayoutVerticalScrollBarSlider()
{
	if ( !_vertScrollBar )
		return;

	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );

	int il = 0;
	int it = 0;
	int ir = 0;
	int ib = 0;
	GetInset( il, it, ir, ib );

	wide -= it;
	_vertScrollBar->SetPos( wide - _scrollBarSize, 0 );
	// scrollbar is inside the borders.
	_vertScrollBar->SetSize( _scrollBarSize, tall - ib - ir );

	// calculate how many lines we can fully display
	int displayLines = tall / ( surface()->GetFontTall( _font ) + DRAW_OFFSET_Y );
	int numLines = m_LineBreaks.Count();
	if ( displayLines < numLines )
	{
		// set the scrollbars range
		_vertScrollBar->SetRange( 0, numLines );
		_vertScrollBar->SetRangeWindow( displayLines );
		_vertScrollBar->SetEnabled( true );

		// this should make it scroll one line at a time
		_vertScrollBar->SetButtonPressedScrollValue( 1 );

		int startLine = _vertScrollBar->GetValue();
		int endLine = _vertScrollBar->GetValue() + displayLines;
		int cursorLine = GetCursorLine();
		if ( cursorLine < startLine )
			startLine = cursorLine;
		else if ( cursorLine >= endLine )
			startLine = cursorLine + 1 - displayLines;

		_vertScrollBar->SetValue( startLine );
		_vertScrollBar->InvalidateLayout( false, false );
		_vertScrollBar->Repaint();
	}
	else
	{
		// disable the scrollbar
		_vertScrollBar->SetEnabled( false );
		_vertScrollBar->SetRange( 0, numLines );
		_vertScrollBar->SetRangeWindow( numLines );
		_vertScrollBar->SetValue( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether a vertical scrollbar is visible
// Input  : state - true or false
//-----------------------------------------------------------------------------
void TextEntry::SetVerticalScrollbar( bool state )
{
	_verticalScrollbar = state;
	if ( state )
	{
		if ( !_vertScrollBar )
		{
			_vertScrollBar = new ScrollBar( this, "ScrollBar", true );
			_vertScrollBar->AddActionSignalTarget( this );
		}
		_vertScrollBar->SetVisible( true );
	}
	else if ( _vertScrollBar )
	{
		_vertScrollBar->SetVisible( false );
	}
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Sends a message that the text has changed
//-----------------------------------------------------------------------------
void TextEntry::FireActionSignal()
{
	PostActionSignal( new KeyValues( "TextChanged" ) );
	_dataChanged = false;	// reset the data changed flag
	InvalidateLayout( false, false );
}

void TextEntry::GetText( wchar_t* wbuf, int bufLen )
{
	int count = m_TextStream.Count();
	if ( !count )
	{
		wbuf[0] = 0;
		return;
	}

	if ( count >= bufLen - 1 )
		count = bufLen - 1;
	wcsncpy( wbuf, m_TextStream.Base(), count );
	wbuf[count] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: as above, but sets an integer
//-----------------------------------------------------------------------------
void TextEntry::OnSetState( int state )
{
	char buf[64];
	snprintf( buf, sizeof( buf ), "%d", state );
	SetText( buf );
}

//-----------------------------------------------------------------------------
// Purpose: Set the starting ypixel positon for a walk through the window
//-----------------------------------------------------------------------------
int TextEntry::GetYStart()
{
	if ( _multiline )
		return DRAW_OFFSET_Y;

	int fontTall = surface()->GetFontTall( _font );
	return GetTall() / 2 - fontTall / 2;
}

//-----------------------------------------------------------------------------
// Purpose: Handle mouse double clicks
//-----------------------------------------------------------------------------
void TextEntry::OnMouseDoublePressed( MouseCode code )
{
	if ( code != MOUSE_LEFT )
		return;

	OnMousePressed( MOUSE_LEFT );

	GotoWordLeft();
	// then find the start and end of the word we are in to highlight it.
	int selectSpot[2];
	selectSpot[0] = _cursorPos;
	GotoWordRight();
	selectSpot[1] = _cursorPos;

	if ( _cursorPos > 0 )
	{
		if ( iswspace( m_TextStream[_cursorPos - 1] ) )
		{
			selectSpot[1]--;
			_cursorPos--;
		}

		_select[0] = selectSpot[0];
		_select[1] = selectSpot[1];
		_mouseSelection = true;
	}
}

bool TextEntry::DrawCursor( int, int )
{
	if ( _cursorBlink )
		return false;

	int cx = 0;
	int cy = 0;
	CursorToPixelSpace( _cursorPos, cx, cy );
	surface()->DrawSetColor( _cursorColor );
	int fontTall = surface()->GetFontTall( _font );
	surface()->DrawFilledRect( cx, cy, cx + 1, cy + fontTall );
	return true;
}

bool TextEntry::IsEditable()
{
	return _editable && IsEnabled();
}

//-----------------------------------------------------------------------------
// Purpose: We want single line windows to scroll horizontally and select text
//          in response to clicking and holding outside window
//-----------------------------------------------------------------------------
void TextEntry::OnMouseFocusTicked()
{
	// if a button is down move the scrollbar slider the appropriate direction
	if ( _mouseDragSelection ) // text is being selected via mouse clicking and dragging
		OnCursorMoved( 0, 0 );	// we want the text to scroll as if we were dragging
}

void TextEntry::OnMouseReleased( MouseCode )
{
	_mouseSelection = false;
	input()->SetMouseCapture( 0 );

	int x0 = 0;
	int x1 = 0;
	if ( GetSelectedRange( x0, x1 ) && x0 == x1 )
		_select[0] = -1;
}

void TextEntry::OpenEditMenu()
{
	int cursorX = 0;
	int cursorY = 0;
	input()->GetCursorPos( cursorX, cursorY );

	int x0 = 0;
	int x1 = 0;
	bool hasSelection = GetSelectedRange( x0, x1 );
	m_pEditMenu->SetItemEnabled( "&Cut", hasSelection );
	m_pEditMenu->SetItemEnabled( "C&opy", hasSelection );

	m_pEditMenu->SetVisible( true );
	m_pEditMenu->RequestFocus( 0 );
	m_pEditMenu->InvalidateLayout( true, false );

	int menuWide = 0;
	int menuTall = 0;
	m_pEditMenu->GetSize( menuWide, menuTall );
	int screenWide = 0;
	int screenTall = 0;
	surface()->GetScreenSize( screenWide, screenTall );

	int menuY = cursorY;
	if ( screenTall - menuTall <= cursorY )
		menuY = cursorY - menuTall;
	if ( screenWide - menuWide <= cursorX )
		m_pEditMenu->SetPos( cursorX - menuWide, menuY );
	else
		m_pEditMenu->SetPos( cursorX, menuY );
	m_pEditMenu->RequestFocus( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Saves the current state to the undo stack
//-----------------------------------------------------------------------------
void TextEntry::SaveUndoState()
{
	_undoCursorPos = _cursorPos;
	m_UndoTextStream.RemoveAll();
	for ( int i = 0; i < m_TextStream.Count(); i++ )
		m_UndoTextStream.AddToTail( m_TextStream[i] );
}

//-----------------------------------------------------------------------------
// Purpose: Reverts back to last saved changes
//-----------------------------------------------------------------------------
void TextEntry::Undo()
{
	_cursorPos = _undoCursorPos;
	m_TextStream.RemoveAll();
	for ( int i = 0; i < m_UndoTextStream.Count(); i++ )
		m_TextStream.AddToTail( m_UndoTextStream[i] );

	InvalidateLayout( false, false );
	Repaint();
	SelectNone();
}

//-----------------------------------------------------------------------------
// Purpose: Given cursor's position in the text buffer, convert it to
//   the local window's x and y pixel coordinates
// Input: cursorPos: cursor index
// Output: cx, cy, the corresponding coords in the local window
//-----------------------------------------------------------------------------
void TextEntry::CursorToPixelSpace( int cursorPos, int& cx, int& cy )
{
	int y = GetYStart();
	_pixelsIndent = 0;

	int x = DRAW_OFFSET_X;
	int lineBreakIndexIndex = 0;
	for ( int i = GetStartDrawIndex( lineBreakIndexIndex ); i < m_TextStream.Count(); i++ )
	{
		if ( i == cursorPos )
			break;

		wchar_t ch = _hideText ? L'*' : m_TextStream[i];

		// if we've passed a line break go to that
		if ( m_LineBreaks.Count() && lineBreakIndexIndex < m_LineBreaks.Count()
			 && m_LineBreaks[lineBreakIndexIndex] == i )
		{
			// add another line
			AddAnotherLine( x, y );
			lineBreakIndexIndex++;
		}

		// add to the current position
		x += getCharWidth( _font, ch );
	}

	cx = x;
	cy = y;
}

//-----------------------------------------------------------------------------
// Purpose: Converts local pixel coordinates to an index in the text buffer
//          This function appears to be used only in response to mouse clicking
// Input  : cx - 
//			cy - pixel location
//-----------------------------------------------------------------------------
int TextEntry::PixelToCursorSpace( int cx, int cy )
{
	_putCursorAtEnd = false; //	Start off assuming we clicked somewhere in the text

	int fontTall = surface()->GetFontTall( _font );
	int yStart = GetYStart();
	int x = DRAW_OFFSET_X;
	int y = yStart;
	_pixelsIndent = 0;

	int lineBreakIndexIndex = 0;
	int startIndex = GetStartDrawIndex( lineBreakIndexIndex );
	bool onRightLine = false;
	int i;
	for ( i = startIndex; i < m_TextStream.Count(); i++ )
	{
		wchar_t ch = _hideText ? L'*' : m_TextStream[i];

		// if we are on the right line but off the end of if put the cursor at the end of the line
		if ( m_LineBreaks[lineBreakIndexIndex] == i )
		{
			// add another line
			AddAnotherLine( x, y );
			lineBreakIndexIndex++;

			if ( onRightLine )
			{
				_putCursorAtEnd = true;
				return i;
			}
		}

		// check to see if we're on the right line
		if ( cy < yStart )
		{
			// cursor is above panel
			onRightLine = true;
			_putCursorAtEnd = true;	// this will make the text scroll up if needed
		}
		else if ( cy >= y && cy <= y + fontTall )
		{
			onRightLine = true;
		}

		int wide = getCharWidth( _font, ch );

		// if we've found the position, break
		if ( onRightLine )
		{
			if ( GetWide() >= cx && ( cx <= _pixelsIndent + 2 || cy < yStart ) )
				return i;

			if ( cx >= x && cx < x + wide )
			{
				if ( wide * 0.5 + x > cx )
					return i;
				return i + 1;
			}
		}
		x += wide;
	}

	return i;
}

//-----------------------------------------------------------------------------
// Purpose: Draws a character in the panel
// Input:	ch - character to draw
//			font - font to use
//			x, y - pixel location to draw char at
// Output:	returns the width of the character drawn
//-----------------------------------------------------------------------------
int TextEntry::DrawChar( wchar_t ch, HFont font, int index, int x, int y )
{
	// add to the current position
	int charWide = getCharWidth( font, ch );
	if ( !iswprint( ch ) )
		return 0;

	int fontTall = surface()->GetFontTall( font );

	// draw selection, if any
	int selection0 = -1;
	int selection1 = -1;
	GetSelectedRange( selection0, selection1 );
	if ( index >= selection0 && index < selection1 )
	{
			// draw background selection color
		VPANEL focus = input()->GetFocus();
		bool hasFocus = HasFocus();
		if ( !hasFocus && focus )
			hasFocus = ipanel()->HasParent( focus, GetVPanel() );

		if ( hasFocus )
			surface()->DrawSetColor( _selectionColor );
		else
			surface()->DrawSetColor( _defaultSelectionBG2Color );
		surface()->DrawFilledRect( x, y, x + charWide, y + fontTall + 1 );

		// reset text color
		surface()->DrawSetTextColor( _selectionTextColor );
	}

	surface()->DrawSetTextPos( x, y );
	surface()->DrawUnicodeChar( ch );
	return charWide;
}

void TextEntry::OnCursorMoved( int, int )
{
	if ( !_mouseSelection )
		return;

	// update the cursor position
	int x = 0;
	int y = 0;
	input()->GetCursorPos( x, y );
	ScreenToLocal( x, y );
	_cursorPos = PixelToCursorSpace( x, y );

	// if we are at Start of buffer don't put cursor at end, this will keep
	// window from scrolling up to a blank line
	if ( _cursorPos == 0 )
		_putCursorAtEnd = false;

	// scroll if we went off left side
	if ( _cursorPos == _currentStartIndex )
	{
		if ( _cursorPos > 0 )
			_cursorPos--;

		if ( !_multiline && _horizScrollingAllowed )
		{
			ScrollLeft();
			_cursorPos = _currentStartIndex;
		}
	}

	if ( _select[1] != _cursorPos )
	{
		_select[1] = _cursorPos;
		Repaint();
	}
}

void TextEntry::GetText( char* buf, int bufLen )
{
	if ( !m_TextStream.Count() )
	{
		buf[0] = 0;
		return;
	}

	m_TextStream.AddToTail( 0 );
	localize()->ConvertUnicodeToANSI( m_TextStream.Base(), buf, bufLen );
	m_TextStream.Remove( m_TextStream.Count() - 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Move the cursor to a line, need to know how many pixels are in a line
//-----------------------------------------------------------------------------
void TextEntry::MoveCursor( int line, int pixelsAcross )
{
	// clamp to a valid line
	if ( line < 0 )
		line = 0;
	if ( line >= m_LineBreaks.Count() )
		line = m_LineBreaks.Count() - 1;

	int y = GetYStart();
	_pixelsIndent = 0;

	int x = DRAW_OFFSET_X;
	int lineBreakIndexIndex = 0;
	int i;
	for ( i = 0; i < m_TextStream.Count(); i++ )
	{
		wchar_t ch = _hideText ? L'*' : m_TextStream[i];

		// if we've passed a line break go to that
		if ( m_LineBreaks[lineBreakIndexIndex] == i )
		{
			if ( lineBreakIndexIndex == line )
			{
				_putCursorAtEnd = true;
				_cursorPos = i;
				break;
			}

			// add another line
			AddAnotherLine( x, y );
			lineBreakIndexIndex++;
		}

		// add to the current position
		// check to see if we're in range
		int charWidth = getCharWidth( _font, ch );
		if ( line == lineBreakIndexIndex && x + charWidth / 2 > pixelsAcross )
		{
			// found position
			_cursorPos = i;
			break;
		}

		x += charWidth;
	}

	// if we never find the cursor it must be past the end
	// of the text buffer, to let's just slap it on the end of the text buffer then.
	if ( i == m_TextStream.Count() )
		GotoTextEnd();

	LayoutVerticalScrollBarSlider();
	ResetCursorBlink();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the value of char at index position.
//-----------------------------------------------------------------------------
void TextEntry::SetCharAt( wchar_t ch, int index )
{
	if ( ch == L'\n' || ch == 0 )
	{
		_recalculateBreaksIndex = 0;
		m_LineBreaks.RemoveAll();
		m_LineBreaks.AddToTail( BUFFER_SIZE );
	}

	if ( index < 0 )
		return;

	while ( m_TextStream.Count() <= index )
		m_TextStream.AddToTail( 0 );

	m_TextStream[index] = ch;
	_dataChanged = true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the index in the text buffer of the
//          character the drawing should Start at
//-----------------------------------------------------------------------------
int TextEntry::GetStartDrawIndex( int& lineBreakIndexIndex )
{
	int startLine = 0;
	int startIndex = 0;

	// determine the Start point from the scroll bar
	// do this only if we are not selecting text in the window with the mouse
	if ( _vertScrollBar && !_mouseDragSelection )
	{
		// skip to line indicated by scrollbar
		startLine = _vertScrollBar->GetValue();
	}
	else
	{
		int numLines = m_LineBreaks.Count();
		int displayLines = GetTall() / ( surface()->GetFontTall( _font ) + DRAW_OFFSET_Y );
		if ( displayLines < numLines )
		{
			int cursorLine = GetCursorLine();
			startLine = _currentStartLine;
			if ( startLine > cursorLine )
			{
				if ( !_vertScrollBar )
				{
					startLine = cursorLine;
				}
				else
				{
					_vertScrollBar->SetValue( _vertScrollBar->GetValue() - 3 );
					startLine = _vertScrollBar->GetValue();
				}
			}
			else if ( startLine + displayLines <= cursorLine )
			{
				if ( !_vertScrollBar )
				{
					// cursor is down below visible area; scroll forward
					startLine = cursorLine - displayLines + 1;
				}
				else
				{
					_vertScrollBar->SetValue( _vertScrollBar->GetValue() + 3 );
					startLine = _vertScrollBar->GetValue();
				}
			}
		}
		else if ( !_multiline )
		{
			// check to see if cursor is off the right side of screen-single line case
			// get cursor's x coordinate in pixel space
			int x = DRAW_OFFSET_X;
			for ( int i = _currentStartIndex; i < m_TextStream.Count(); i++ )
			{

				// if we've found the position, break
				if ( _cursorPos == i )
					break;

				wchar_t ch = _hideText ? L'*' : m_TextStream[i];
				x += getCharWidth( _font, ch );
			}

			if ( x >= GetWide() )
				_currentStartIndex++;
			else if ( x <= 0 && _currentStartIndex > 0 )
				_currentStartIndex--;
		}
	}

	if ( startLine > 0 )
	{
		lineBreakIndexIndex = startLine;
		if ( startLine < m_LineBreaks.Count() )
			startIndex = m_LineBreaks[startLine - 1];
	}

	if ( !_horizScrollingAllowed )
		return 0;

	_currentStartLine = startLine;
	if ( !_multiline )
		return _currentStartIndex;
	return startIndex;
}

void TextEntry::GotoWordLeft()
{
	SelectCheck();

	if ( _cursorPos < 1 )
		return;

	int i = _cursorPos - 1;
	while ( i >= 0 && iswspace( m_TextStream[i] ) )
		i--;
	while ( i >= 0 && !iswspace( m_TextStream[i] ) )
		i--;
	_cursorPos = i + 1;

	if ( !_multiline && _horizScrollingAllowed && _cursorPos < _currentStartIndex )
	{
		_currentStartIndex = _cursorPos;
		LayoutVerticalScrollBarSlider();
	}

	LayoutVerticalScrollBarSlider();
	ResetCursorBlink();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Move cursor to the Start of the text buffer
//-----------------------------------------------------------------------------
void TextEntry::GotoTextStart()
{
	SelectCheck();
	_currentStartIndex = 0;
	_cursorPos = 0;
	_putCursorAtEnd = false;
	LayoutVerticalScrollBarSlider();
	ResetCursorBlink();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Move cursor to the Start of the current line
//-----------------------------------------------------------------------------
void TextEntry::GotoFirstOfLine()
{
	SelectCheck();

	if ( _multiline )
		_cursorPos = GetCurrentLineStart();
	else
		_cursorPos = _currentStartIndex;

	_putCursorAtEnd = false;
	_currentStartIndex = _cursorPos;
	LayoutVerticalScrollBarSlider();
	ResetCursorBlink();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Handle the effect of user hitting backspace key
//			we delete the char before the cursor and reformat the text so it
//			behaves like in windows.
//-----------------------------------------------------------------------------
void TextEntry::Backspace()
{
	if ( !IsEditable() )
		return;

	if ( _cursorPos == 0 || m_TextStream.Count() == 0 )
		return;

	SaveUndoState();

	//shift chars left one, starting at the cursor position, then make the line one smaller
	for ( int i = _cursorPos; i < m_TextStream.Count(); i++ )
		SetCharAt( m_TextStream[i], i - 1 );
	m_TextStream.Remove( m_TextStream.Count() - 1 );

	// As we hit the Start of the window, expose more chars so we can see what we are deleting
	if ( _cursorPos == _currentStartIndex )
	{
		// windows tabs over 6 chars
		if ( _currentStartIndex - 6 >= 0 ) // dont scroll if there are not enough chars to scroll
			_currentStartIndex -= 6;
		else
			_currentStartIndex = 0;
	}

	//move the cursor left one
	_cursorPos--;
	_dataChanged = true;

	CalcBreakIndex();

	LayoutVerticalScrollBarSlider();
	ResetCursorBlink();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Move the cursor one character to the left, scroll the text
//  horizontally if needed
//-----------------------------------------------------------------------------
void TextEntry::GotoLeft()
{
	SelectCheck();

	if ( IsLineBreak( _cursorPos ) && !_cursorIsAtEnd )
		_putCursorAtEnd = true;
	else if ( !_putCursorAtEnd && _cursorPos > 0 )
		_cursorPos--;

	ScrollLeft();
	ResetCursorBlink();
	Repaint();
}


//-----------------------------------------------------------------------------
// Purpose: Move the cursor one line down
//-----------------------------------------------------------------------------
void TextEntry::GotoDown()
{
	SelectCheck();

	if ( _cursorIsAtEnd )
	{
		_cursorPos--;
		if ( _cursorPos < 0 )
			_cursorPos = 0;
	}

	int cx = 0;
	int cy = 0;
	CursorToPixelSpace( _cursorPos, cx, cy );

	// move the cursor to the next line
	MoveCursor( GetCursorLine() + 1, cx );
	if ( !_putCursorAtEnd && _cursorIsAtEnd )
	{
		_cursorPos++;
		if ( _cursorPos > m_TextStream.Count() )
			_cursorPos = m_TextStream.Count();
	}
	LayoutVerticalScrollBarSlider();
}

//-----------------------------------------------------------------------------
// Purpose: Move the cursor one line up 
//-----------------------------------------------------------------------------
void TextEntry::GotoUp()
{
	SelectCheck();

	if ( _cursorIsAtEnd )
	{
		if ( GetCursorLine() == 1 )
		{
			// stay at end of line
			_putCursorAtEnd = true;
			return;	 // dont move the cursor
		}
		_cursorPos--;
	}

	int cx = 0;
	int cy = 0;
	CursorToPixelSpace( _cursorPos, cx, cy );

	// move the cursor to the previous line
	MoveCursor( GetCursorLine(), cx );
}

//-----------------------------------------------------------------------------
// Purpose: Move cursor to the end of the text buffer
//-----------------------------------------------------------------------------
void TextEntry::GotoTextEnd()
{
	SelectCheck();
	_putCursorAtEnd = true;
	_cursorPos = m_TextStream.Count();
	ScrollRight();				// scroll over until cursor is on screen
	LayoutVerticalScrollBarSlider();
	ResetCursorBlink();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Move cursor to the end of the current line
//-----------------------------------------------------------------------------
void TextEntry::GotoEndOfLine()
{
	SelectCheck();
	// to get to the end of the line you have to take into account line wrap in the buffer
	// we have to figure out at which point the line wraps
	// given the current cursor position, select[1], find the index that is the
	// line end to the right of the cursor
	//_cursorPos=m_TextStream.Count(); //TODO: this is wrong, should go to last non-whitespace, then to true EOL
	_cursorPos = GetCurrentLineEnd();
	_putCursorAtEnd = true;
	ScrollRight();
	LayoutVerticalScrollBarSlider();
	ResetCursorBlink();
	Repaint();
}

void TextEntry::GotoWordRight()
{
	SelectCheck();

	int count = m_TextStream.Count();
	int i = _cursorPos + 1;
	while ( i < count && !iswspace( m_TextStream[i] ) )
		i++;
	while ( i < count && iswspace( m_TextStream[i] ) )
		i++;
	if ( i > count )
		i = count;
	_cursorPos = i;

	// now we are at the start of the next word

	// scroll right if we need to
	ScrollRight();
	LayoutVerticalScrollBarSlider();
	ResetCursorBlink();
	Repaint();
}

void TextEntry::GotoRight()
{
	SelectCheck();

	// if we are on a line break just move the cursor to the next line
	if ( IsLineBreak( _cursorPos ) )
	{
		if ( _cursorIsAtEnd )
			_putCursorAtEnd = false;
	}
	else if ( _cursorPos < m_TextStream.Count() )
	{
		_cursorPos++;

		if ( IsLineBreak( _cursorPos ) && !_cursorIsAtEnd )
			_putCursorAtEnd = true;
	}

	ScrollRight();
	LayoutVerticalScrollBarSlider();
	ResetCursorBlink();
	Repaint();
}

bool TextEntry::NeedsEllipses( HFont font, int* pIndex )
{
	*pIndex = -1;

	int wide = DRAW_OFFSET_X;
	for ( int i = 0; i < m_TextStream.Count(); i++ )
	{
		wide += getCharWidth( font, m_TextStream[i] );
		if ( wide > _drawWidth )
		{
			*pIndex = i;
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Create cut/copy/paste dropdown menu
//-----------------------------------------------------------------------------
void TextEntry::CreateEditMenu()
{
	delete m_pEditMenu;

	m_pEditMenu = new Menu( this, "EditMenu" );
	if ( _editable && !_hideText )
		m_pEditMenu->AddMenuItem( "#TextEntry_Cut", new KeyValues( "DoCutSelected" ), this, NULL );
	if ( !_hideText )
		m_pEditMenu->AddMenuItem( "#TextEntry_Copy", new KeyValues( "DoCopySelected" ), this, NULL );
	if ( _editable )
		m_pEditMenu->AddMenuItem( "#TextEntry_Paste", new KeyValues( "DoPaste" ), this, NULL );
	m_pEditMenu->SetVisible( false );
	m_pEditMenu->SetParent( this );
	m_pEditMenu->AddActionSignalTarget( this );
}

//-----------------------------------------------------------------------------
// Purpose: Handle Mouse button down events.
//-----------------------------------------------------------------------------
void TextEntry::OnMousePressed( MouseCode code )
{
	if ( code == MOUSE_RIGHT )
	{
		CreateEditMenu();
		OpenEditMenu();
		return;
	}

	if ( code != MOUSE_LEFT )
		return;

	SelectCheck();

	// move the cursor to where the mouse was pressed
	int x = 0;
	int y = 0;
	input()->GetCursorPos( x, y );
	ScreenToLocal( x, y );

	_cursorIsAtEnd = _putCursorAtEnd; // save this off before calling PixelToCursorSpace()
	_cursorPos = PixelToCursorSpace( x, y );

	// if we are at Start of buffer don't put cursor at end, this will keep
	// window from scrolling up to a blank line
	if ( _cursorPos == 0 )
		_putCursorAtEnd = false;

	// enter selection mode
	input()->SetMouseCapture( GetVPanel() );
	_mouseSelection = true;

	if ( _select[0] < 0 )
		_select[0] = _cursorPos;
	_select[1] = _cursorPos;

	ResetCursorBlink();
	RequestFocus( 0 );
	Repaint();
}

void TextEntry::MoveScrollBar( int delta )
{
	if ( _vertScrollBar )
		_vertScrollBar->SetValue( _vertScrollBar->GetValue() - 3 * delta );
}

//-----------------------------------------------------------------------------
// Purpose: Tell if an index is a linebreakindex
//-----------------------------------------------------------------------------
bool TextEntry::IsLineBreak( int index )
{
	for ( int i = 0; i < m_LineBreaks.Count(); i++ )
	{
		if ( m_LineBreaks[i] == index )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Find out what line the cursor is on
//-----------------------------------------------------------------------------
int TextEntry::GetCursorLine()
{
	int numLines = m_LineBreaks.Count();
	int cursorLine = 0;
	if ( numLines > 0 && _cursorPos >= m_LineBreaks[0] )
	{
		for ( cursorLine = 1; cursorLine < numLines; cursorLine++ )
		{
			if ( m_LineBreaks[cursorLine] > _cursorPos )
				break;
		}
	}

	if ( _putCursorAtEnd && _cursorPos != m_TextStream.Count() )
		cursorLine--;

	return cursorLine;
}

//-----------------------------------------------------------------------------
// Purpose: Turn horizontal scrolling on or off.
//			Horizontal scrolling is disabled in multline windows. 
//		    Toggling this will disable it in single line windows as well.
//-----------------------------------------------------------------------------
void TextEntry::SetHorizontalScrolling( bool status )
{
	_horizScrollingAllowed = status;
}

void TextEntry::ScrollLeft()
{
	if ( _multiline || !_horizScrollingAllowed )
		return;

	if ( _cursorPos < _currentStartIndex )	 // scroll left if we need to
	{
		if ( _cursorPos < 0 ) // dont scroll past the Start of buffer
			_cursorPos = 0;
		_currentStartIndex = _cursorPos;
	}

	LayoutVerticalScrollBarSlider();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TextEntry::ScrollLeftForResize()
{
	if ( _multiline || !_horizScrollingAllowed )
		return;

	// go until we hit leftmost
	while ( _currentStartIndex > 0 )
	{
		_currentStartIndex--;
		int nVal = _currentStartIndex;

		// check if the cursor is now off the screen
		if ( IsCursorOffRightSideOfWindow( _cursorPos ) )
		{
			// we've gone too far, return it
			_currentStartIndex++;
			break;
		}

        // IsCursorOffRightSideOfWindow actually fixes the _currentStartIndex, 
        // so if our value changed that menas we really are off the screen
		if ( nVal != _currentStartIndex )
			break;
	}

	LayoutVerticalScrollBarSlider();
}

//-----------------------------------------------------------------------------
// Purpose: Horizontal scrolling function, not used in multiline windows
//			Scroll one char right until the cursor is visible in the window.
//			We do this one char at a time because char width isn't a constant.
//-----------------------------------------------------------------------------
void TextEntry::ScrollRight()
{
	if ( !_horizScrollingAllowed )
		return;

	if ( IsCursorOffRightSideOfWindow( _cursorPos ) )
	{
		_currentStartIndex++; //scroll over
		ScrollRight(); // scroll again, check if cursor is in window yet 
	}

	LayoutVerticalScrollBarSlider();
}

bool TextEntry::IsCursorOffRightSideOfWindow( int cursorPos )
{
	int cx = 0;
	int cy = 0;
	CursorToPixelSpace( cursorPos, cx, cy );

	int wx = GetWide() - 1;	//width of inside of window is GetWide()-1
	return wx >= 0 && cx >= wx;
}

bool TextEntry::IsCursorOffLeftSideOfWindow( int cursorPos )
{
	int cx = 0;
	int cy = 0;
	CursorToPixelSpace( cursorPos, cx, cy );
	return cx <= 0;
}

int TextEntry::GetCurrentLineStart()
{
	if ( !_multiline )			// quick out for non multline buffers
		return _currentStartIndex;

	int numLines = m_LineBreaks.Count();
	if ( numLines <= 0 || _cursorPos < m_LineBreaks[0] )
		return 0;

	for ( int i = 0; i < numLines; i++ )
	{
		if ( _cursorPos == m_LineBreaks[i] )
		{
			if ( !_cursorIsAtEnd || i == 0 )
				return _cursorPos;
			return m_LineBreaks[i - 1];
		}

		if ( i + 1 < numLines && _cursorPos < m_LineBreaks[i + 1] )
			return m_LineBreaks[i];
	}
	// if there were no line breaks, the first char in the line is the Start of the buffer
	return 0;
}

int TextEntry::GetCurrentLineEnd()
{
	int numLines = m_LineBreaks.Count();
	int i;

	for ( i = 0; i < numLines; i++ )
	{
		if ( _cursorPos != m_LineBreaks[i] )
			continue;

		if ( _cursorIsAtEnd )
			return _cursorPos;
		if ( i != numLines - 2 )
			return m_LineBreaks[i + 1];
		break;
	}

	for ( i = 0; i < numLines - 1; i++ )
	{
		if ( _cursorPos < m_LineBreaks[i] )
			return m_LineBreaks[i];
	}
	return m_TextStream.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Get the lineBreakIndex index of the line before the cursor
//			note _recalculateBreaksIndex < 0 flags RecalculateLineBreaks 
//			to figure it all out from scratch
//-----------------------------------------------------------------------------
void TextEntry::CalcBreakIndex()
{
	// an optimization to handle when the cursor is at the end of the buffer.
	// pays off if the buffer is large, and the search loop would be long.
	if ( _cursorPos == m_TextStream.Count() )
	{
		// we know m_LineBreaks array always has at least one element in it (99999 sentinel)
		// when there is just one line this will make recalc = -1 which is ok.
		_recalculateBreaksIndex = m_LineBreaks.Count() - 2;
		return;
	}

	_recalculateBreaksIndex = 0;
	// find the line break just before the cursor position
	while ( _cursorPos > m_LineBreaks[_recalculateBreaksIndex] )
		_recalculateBreaksIndex++;

	_recalculateBreaksIndex--;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of lines in the window
//-----------------------------------------------------------------------------
int TextEntry::GetNumLines()
{
	return m_LineBreaks.Count();
}

void TextEntry::SelectAllText( bool bResetCursorPos )
{
	if ( bResetCursorPos )
		_cursorPos = 0;

	// if there's no text at all, select none
	_select[0] = m_TextStream.Count() ? 0 : -1;
	_select[1] = m_TextStream.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Select no text.
//-----------------------------------------------------------------------------
void TextEntry::SelectNoText()
{
	_select[0] = -1;
	_select[1] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TextEntry::SelectAllOnFirstFocus( bool status )
{
	_selectAllOnFirstFocus = status;
}

//-----------------------------------------------------------------------------
// Purpose: Set the width we have to draw text in.
//			Do not use in multiline windows.
//-----------------------------------------------------------------------------
void TextEntry::SetDrawWidth( int width )
{
	_drawWidth = width;
}

//-----------------------------------------------------------------------------
// Purpose: Get the width we have to draw text in.
//-----------------------------------------------------------------------------
int TextEntry::GetDrawWidth()
{
	return _drawWidth;
}

void TextEntry::SetAllowNonAsciiCharacters( bool state )
{
	m_bAllowNonAsciiCharacters = state;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void TextEntry::SetAllowNumericInputOnly( bool state )
{
	m_bAllowNumericInputOnly = state;
}

void TextEntry::SetUseFallbackFont( bool bState, HFont hFallback )
{
	m_bUseFallbackFont = bState;
	m_hFallbackFont = hFallback;
}

int TextEntry::GetTextLength() const
{
	return m_TextStream.Count();
}

void TextEntry::PaintBackground()
{
	// draw background
	Color col = IsEnabled() ? GetBgColor() : _disabledBgColor;
	surface()->DrawSetColor( col );
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	surface()->DrawFilledRect( 0, 0, wide, tall );

	HFont useFont = _font;
	surface()->DrawSetTextFont( useFont );
	col = IsEnabled() ? GetFgColor() : _disabledFgColor;
	surface()->DrawSetTextColor( col );

	_pixelsIndent = 0;
	int lineBreakIndexIndex = 0;
	int startIndex = GetStartDrawIndex( lineBreakIndexIndex );
	int x = _pixelsIndent + DRAW_OFFSET_X;
	int y = GetYStart();

	if ( _multiline || _horizScrollingAllowed )
	{
		for ( int i = startIndex; i < m_TextStream.Count(); i++ )
		{
			wchar_t ch = _hideText ? L'*' : m_TextStream[i];

			// if we've passed a line break go to that
			if ( _multiline && m_LineBreaks[lineBreakIndexIndex] == i )
			{
				AddAnotherLine( x, y );
				lineBreakIndexIndex++;
			}
			x += DrawChar( ch, useFont, i, x, y );
		}
	}
	else
	{
		int endIndex = m_TextStream.Count();

		if ( !HasFocus() || !IsEditable() )
		{
			int ellipsisIndex = -1;
			bool shouldDrawEllipsis = NeedsEllipses( useFont, &ellipsisIndex );

			if ( shouldDrawEllipsis && !IsEditable() && m_bUseFallbackFont && m_hFallbackFont )
			{
				surface()->DrawSetTextFont( m_hFallbackFont );
				useFont = m_hFallbackFont;

				int fallbackIndex = -1;
				if ( !NeedsEllipses( useFont, &fallbackIndex ) )
					shouldDrawEllipsis = false;
				else
					ellipsisIndex = fallbackIndex;
			}

			if ( shouldDrawEllipsis )
			{
				int ellipsisWidth = 3 * getCharWidth( useFont, L'.' );
				int i = ellipsisIndex;
				while ( i >= 0 && ellipsisWidth > 0 )
				{
					ellipsisWidth -= getCharWidth( useFont, m_TextStream[i] );
					i--;
				}
				endIndex = i + 1;

				int remaining = m_TextStream.Count() - endIndex;
				if ( remaining > 0 && remaining < 3 )
					endIndex = m_TextStream.Count() - 3;
			}
		}

		// draw the text
		for ( int i = startIndex; i < endIndex; i++ )
		{
			wchar_t ch = _hideText ? L'*' : m_TextStream[i];
			x += DrawChar( ch, useFont, i, x, y );
		}

		if ( endIndex < m_TextStream.Count() ) // add an elipsis
		{
			x += DrawChar( L'.', useFont, endIndex, x, y );
			x += DrawChar( L'.', useFont, endIndex + 1, x, y );
			x += DrawChar( L'.', useFont, endIndex + 2, x, y );
		}
	}

	// custom border
	//!! need to replace this with scheme stuff (TextEntryBorder/TextEntrySelectedBorder)
	surface()->DrawSetColor( 50, 50, 50, 255 );
	if ( IsEnabled() && IsEditable() && HasFocus() )
	{
		// set a more distinct border color
		surface()->DrawSetColor( 0, 0, 0, 255 );
		DrawCursor( x, y );
	}
}

void TextEntry::RecalculateLineBreaks()
{
	if ( !_multiline || _hideText || m_TextStream.Count() <= 0 )
		return;

	HFont font = _font;

	// line break to our width -2 pixel to keep cursor blinking in window
	// (assumes borders are 1 pixel)
	int wide = GetWide() - 2;

	// subtract the scrollbar width
	if ( _vertScrollBar )
		wide -= _vertScrollBar->GetWide();

	int x = DRAW_OFFSET_X;
	int y = DRAW_OFFSET_Y;
	int wordStartIndex = 0;
	bool hasWord = false;
	bool justStartedNewLine = true;
	bool wordStartedOnNewLine = true;

	int i = 0;
	if ( _recalculateBreaksIndex <= 0 )
	{
		m_LineBreaks.RemoveAll();
	}
	else
	{
		while ( m_LineBreaks.Count() > _recalculateBreaksIndex + 1 )
			m_LineBreaks.Remove( _recalculateBreaksIndex + 1 );
		i = m_LineBreaks[_recalculateBreaksIndex];
	}

	if ( i < m_TextStream.Count() && ( m_TextStream[i] == L'\n' || m_TextStream[i] == L'\r' ) )
		i++;

	while ( i < m_TextStream.Count() )
	{
		wchar_t ch = m_TextStream[i];

		// line break only on whitespace characters
		if ( iswspace( ch ) )
		{
			hasWord = false;
		}
		else if ( !hasWord )
		{
			// Start a new word
			wordStartIndex = i;
			hasWord = true;
			wordStartedOnNewLine = justStartedNewLine;
		}

		int charWidth = 0;
		if ( iswprint( ch ) )
		{

			// get the width
			charWidth = getCharWidth( font, ch );
			justStartedNewLine = false;
		}

		bool newLine = ch == L'\n' || ch == L'\r';
		if ( x + charWidth < wide && !newLine )
		{
			x += charWidth;
			i++;
			continue;
		}

		AddAnotherLine( x, y );

		int breakIndex = i;
		if ( !newLine && !wordStartedOnNewLine && !iswspace( ch ) )
			breakIndex = wordStartIndex;
		m_LineBreaks.AddToTail( breakIndex );

		justStartedNewLine = true;
		hasWord = false;
		x += charWidth;

		i = breakIndex + 1;
	}

	_charCount = i - 1;

	// end the list
	m_LineBreaks.AddToTail( BUFFER_SIZE );

	// set up the scrollbar
	LayoutVerticalScrollBarSlider();
}

//-----------------------------------------------------------------------------
// Purpose: Insert a character into the text buffer
//-----------------------------------------------------------------------------
void TextEntry::InsertChar( wchar_t ch )
{
	// throw away redundant linefeed characters
	if ( ch == L'\r' )
		return;

	// no newline characters in single-line dialogs
	if ( !_multiline && ch == L'\n' )
		return;

	// no tab characters
	if ( ch == L'\t' )
		return;

	if ( m_bAllowNumericInputOnly && !iswdigit( ch ) && ch != L'.' )
	{
		surface()->PlaySound( "Resource\\warning.wav" );
		return;
	}

	// check against unicode characters
	if ( !m_bAllowNonAsciiCharacters && ch > 127 )
	{
		surface()->PlaySound( "Resource\\warning.wav" );
		return;
	}

	// don't add characters if the max char count has been reached
	// ding at the user
	if ( _maxCharCount >= 0 && m_TextStream.Count() >= _maxCharCount )
	{
		if ( _maxCharCount <= 0 || !_multiline || !_wrap )
		{
			surface()->PlaySound( "Resource\\warning.wav" );
			return;
		}

		// if we wrap lines rather than stopping
		while ( m_TextStream.Count() > _maxCharCount )
		{
			if ( _recalculateBreaksIndex == 0 )
				RecalculateLineBreaks();

			if ( m_LineBreaks[0] > m_TextStream.Count() )
			{
				// if the line break is the past the end of the buffer recalc
				_recalculateBreaksIndex = -1;
				RecalculateLineBreaks();
			}

			int firstBreak = m_LineBreaks[0];
			int count = m_TextStream.Count();
			if ( firstBreak + 1 < count )
			{
				int remaining = count - firstBreak;
				if ( remaining > 0 && firstBreak > 0 )
					memmove( m_TextStream.Base(), m_TextStream.Base() + firstBreak, sizeof( wchar_t ) * remaining );
				while ( m_TextStream.Count() > remaining )
					m_TextStream.Remove( m_TextStream.Count() - 1 );

				if ( _cursorPos > remaining )
				{
					_cursorPos = remaining;
				}
				else
				{ // shift the cursor up. don't let it wander past zero
					_cursorPos -= firstBreak + 1;
					if ( _cursorPos < 0 )
						_cursorPos = 0;
				}

				if ( _select[0] >= 0 )
				{
					_select[0] -= firstBreak + 1;
					if ( _select[0] <= 0 )
						_select[0] = -1;

					_select[1] -= firstBreak + 1;
					if ( _select[1] <= 0 )
						_select[1] = -1;
				}

				for ( int j = remaining; j > 0; j-- )
					SetCharAt( m_TextStream[j - 1], j );

				// redo all the line breaks
				_recalculateBreaksIndex = -1;
				RecalculateLineBreaks();
			}
		}
	}

	if ( _wrap )
	{
		// when wrapping you always insert the new char at the end of the buffer
		SetCharAt( ch, m_TextStream.Count() );
		_cursorPos = m_TextStream.Count();
	}
	else
	{
		for ( int i = m_TextStream.Count(); i > _cursorPos; i-- )
			SetCharAt( m_TextStream[i - 1], i );

		SetCharAt( ch, _cursorPos );
		_cursorPos++;
	}

	if ( ch == L'\n' )
		RecalculateLineBreaks();

	if ( m_bAutoProgressOnHittingCharLimit && _maxCharCount == m_TextStream.Count() )
		RequestFocusNext( 0 );

	// scroll right if this pushed the cursor off screen
	ScrollRight();

	_dataChanged = true;

	CalcBreakIndex();
	LayoutVerticalScrollBarSlider();
	ResetCursorBlink();
	Repaint();
}

void TextEntry::InsertString( wchar_t* text )
{
	SaveUndoState();

	for ( ; *text; text++ )
		InsertChar( *text );

	if ( _dataChanged )
		FireActionSignal();
}

//-----------------------------------------------------------------------------
// Purpose: Converts an ansi string to unicode and inserts it into the text stream
//-----------------------------------------------------------------------------
void TextEntry::InsertString( const char* text )
{
	// check for to see if the string is in the localization tables
	if ( text[0] == '#' )
	{
		wchar_t* localized = localize()->Find( text );
		if ( localized != NULL )
		{
			InsertString( localized );
			return;
		}
	}

	// straight convert the ansi to unicode and insert
	wchar_t unicode[1024];
	localize()->ConvertANSIToUnicode( text, unicode, sizeof( unicode ) );
	InsertString( unicode );
}

//-----------------------------------------------------------------------------
// Purpose: Set the text array
//          Using this function will cause all lineBreaks to be discarded.
//          This is because this fxn replaces the contents of the text buffer.
//          For modifying large buffers use insert functions.
//-----------------------------------------------------------------------------
void TextEntry::SetText( const wchar_t* wszText )
{
	if ( wszText == NULL )
		wszText = L"";
	int textLen = (int)wcslen( wszText );
	m_TextStream.RemoveAll();
	m_TextStream.EnsureCapacity( textLen );
	int missed = 0;
	for ( int i = 0; i < textLen; ++i )
	{
		if ( wszText[i] == L'\r' ) // don't insert \r characters
		{
			++missed;
			continue;
		}
		m_TextStream.AddToTail( wszText[i] );
		SetCharAt( wszText[i], i - missed );
	}
	GotoTextStart();
	SelectNone();

	// blow away the line breaks list 
	_recalculateBreaksIndex = 0;
	m_LineBreaks.RemoveAll();
	m_LineBreaks.AddToTail( BUFFER_SIZE );
	_dataChanged = false;
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Deletes the current selection, if any, moving the cursor to the Start
//			of the selection
//-----------------------------------------------------------------------------
void TextEntry::DeleteSelected()
{
	if ( !IsEditable() || m_TextStream.Count() == 0 )
		return;
	int x0 = 0;
	int x1 = 0;
	if ( !GetSelectedRange( x0, x1 ) )
		return;
	SaveUndoState();
	int count = x1 - x0;
	for ( int i = 0; i < count; ++i )
	{
		if ( x0 < m_TextStream.Count() )
			m_TextStream.Remove( x0 );
	}

	// clear any selection
	SelectNone();
	ResetCursorBlink();

	// move the cursor to just after the deleted section
	_cursorPos = x0;
	_dataChanged = true;
	_recalculateBreaksIndex = 0;
	m_LineBreaks.RemoveAll();
	m_LineBreaks.AddToTail( BUFFER_SIZE );
	CalcBreakIndex();
	LayoutVerticalScrollBarSlider();
}

//-----------------------------------------------------------------------------
// Purpose: Handle the effect of the user hitting the delete key
//			removes the char in front of the cursor
//-----------------------------------------------------------------------------
void TextEntry::Delete()
{
	if ( !IsEditable() || m_TextStream.Count() == 0 )
		return;
	int x0 = 0;
	int x1 = 0;
	if ( !GetSelectedRange( x0, x1 ) )
	{
		// no selection, so just delete the one character
		x0 = _cursorPos;
		x1 = x0 + 1;

		// if we're at the end of the line don't do anything
		if ( x0 >= m_TextStream.Count() )
			return;
	}
	SaveUndoState();
	int count = x1 - x0;
	for ( int i = 0; i < count; ++i )
	{
		if ( x0 < m_TextStream.Count() )
			m_TextStream.Remove( x0 );
	}
	ResetCursorBlink();

	// clear any selection
	SelectNone();

	// move the cursor to just after the deleted section
	_cursorPos = x0;
	_dataChanged = true;
	_recalculateBreaksIndex = 0;
	m_LineBreaks.RemoveAll();
	m_LineBreaks.AddToTail( BUFFER_SIZE );
	CalcBreakIndex();
	LayoutVerticalScrollBarSlider();
}

void TextEntry::Paste()
{
	if ( _hideText || !IsEditable() )
		return;

	wchar_t* buf = NULL;
	int bufferSize = system()->GetClipboardTextCount();
	if ( !m_bAutoProgressOnHittingCharLimit )
	{
		if ( _maxCharCount > 0 )
			bufferSize = _maxCharCount + 1;
		else
			bufferSize = system()->GetClipboardTextCount();
	}
	if ( bufferSize <= 0 || bufferSize > INT_MAX / (int)sizeof( wchar_t ) )
		return;
	buf = (wchar_t*)malloc( sizeof( wchar_t ) * bufferSize );
	if ( !buf )
		return;

	int count = system()->GetClipboardText( 0, buf, bufferSize * (int)sizeof( wchar_t ) );
	if ( count > bufferSize )
		count = bufferSize;
	bool chained = false;
	if ( count > 0 )
	{
		SaveUndoState();
		int i = 0;
		for ( ; i < count && i < bufferSize; ++i )
		{
			if ( buf[i] == 0 )
				break;
			if ( m_bAutoProgressOnHittingCharLimit && _maxCharCount == m_TextStream.Count() )
			{
				RequestFocusNext( 0 );
				system()->SetClipboardText( buf + i, count - i - 1 );
				VPANEL parent = GetVParent();
				if ( parent != 0 && ipanel()->GetCurrentKeyFocus( parent ) != GetVPanel() )
				{
					ipanel()->SendMessage(
						ipanel()->GetCurrentKeyFocus( GetVParent() ), new KeyValues( "DoPaste" ), GetVPanel() );
					chained = true;
				}
				break;
			}
			InsertChar( buf[i] );
		}
		if ( m_bAutoProgressOnHittingCharLimit )
			system()->SetClipboardText( buf, bufferSize );
		_dataChanged = true;
		FireActionSignal();
		if ( !chained )
			RequestFocus( 0 );
	}
	free( buf );
}

//-----------------------------------------------------------------------------
// Purpose: Cuts the selected chars from the buffer and 
//          copies them into the clipboard
//-----------------------------------------------------------------------------
void TextEntry::CutSelected()
{
	CopySelected();
	DeleteSelected();
	RequestFocus( 0 );
	if ( _dataChanged )
		FireActionSignal();
}

//-----------------------------------------------------------------------------
// Purpose: Copies the selected chars into the clipboard
//-----------------------------------------------------------------------------
void TextEntry::CopySelected()
{
	if ( _hideText )
		return;
	int x0 = 0;
	int x1 = 0;
	if ( GetSelectedRange( x0, x1 ) )
	{
		CUtlVector<wchar_t> buf;
		for ( int i = x0; i < x1; ++i )
		{
			wchar_t ch = m_TextStream[i];
			if ( ch == L'\n' )
				buf.AddToTail( L'\r' );
			buf.AddToTail( ch );
		}
		buf.AddToTail( 0 );
		system()->SetClipboardText( buf.Base(), x1 - x0 );
	}
	RequestFocus( 0 );
	if ( _dataChanged )
		FireActionSignal();
}

void TextEntry::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	SetFgColor( GetSchemeColor( "WindowFgColor", scheme ) );
	SetBgColor( GetSchemeColor( "WindowBgColor", scheme ) );
	_cursorColor = GetSchemeColor( "TextCursorColor", scheme );
	_disabledFgColor = GetSchemeColor( "WindowDisabledFgColor", scheme );
	_disabledBgColor = GetSchemeColor( "ControlBG", scheme );
	_selectionTextColor = GetSchemeColor( "SelectionFgColor", GetFgColor(), scheme );
	_selectionColor = GetSchemeColor( "SelectionBgColor", scheme );
	_defaultSelectionBG2Color = GetSchemeColor( "SelectionBG2", scheme );
	_focusEdgeColor = GetSchemeColor( "BorderSelection", Color( 0, 0, 0, 0 ), scheme );
	SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
	HFont font = scheme->GetFont( "Default", IsProportional() );
	_font = font;
	SetFont( font );
}


//-----------------------------------------------------------------------------
// Purpose: Sends a message if the data has changed
//          Turns off any selected text in the window if we are not using the edit menu
//-----------------------------------------------------------------------------
void TextEntry::OnKillFocus()
{
	if ( _dataChanged )
	{
		FireActionSignal();
		_dataChanged = false;
	}
	bool mouseRight = input()->WasMousePressed( MOUSE_RIGHT ) || input()->WasMouseReleased( MOUSE_RIGHT )
					  || input()->IsMouseDown( MOUSE_RIGHT );
	if ( !mouseRight )
	{
		if ( IsEditable() )
			SelectNone();
		Panel::OnKillFocus();
		return;
	}
	int cursorX = 0;
	int cursorY = 0;
	input()->GetCursorPos( cursorX, cursorY );
	if ( !IsWithin( cursorX, cursorY ) )
	{
		if ( IsEditable() )
			SelectNone();
		Panel::OnKillFocus();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Wipe line breaks after the size of a panel has been changed
//-----------------------------------------------------------------------------
void TextEntry::OnSizeChanged( int newWide, int newTall )
{
	Panel::OnSizeChanged( newWide, newTall );

   	// blow away the line breaks list 
	_recalculateBreaksIndex = 0;
	m_LineBreaks.RemoveAll();
	m_LineBreaks.AddToTail( BUFFER_SIZE );

	// if we're bigger, see if we can scroll left to put more text in the window
	if ( newWide > _drawWidth )
		ScrollLeftForResize();
	_drawWidth = newWide;
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Called when data changes or panel size changes
//-----------------------------------------------------------------------------
void TextEntry::PerformLayout()
{
	Panel::PerformLayout();
	RecalculateLineBreaks();
	if ( _verticalScrollbar )
		LayoutVerticalScrollBarSlider();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the height of the text entry window so all text will fit inside
//-----------------------------------------------------------------------------
void TextEntry::SetToFullHeight()
{
	PerformLayout();
	int wide = 0;
	int tall = 0;
	GetSize( wide, tall );
	tall = ( surface()->GetFontTall( _font ) + 1 ) * m_LineBreaks.Count() + DRAW_OFFSET_X;
	SetSize( wide, tall );
	PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the width of the text entry window so all text will fit inside
//-----------------------------------------------------------------------------
void TextEntry::SetToFullWidth()
{
	// probably be problems if you try using this on multi line buffers
	// or buffers with clickable text in them.
	if ( _multiline )
		return;
	PerformLayout();
	int wide = 6;
	int count = m_TextStream.Count();
	for ( int i = 0; i < count; ++i )
		wide += getCharWidth( _font, m_TextStream[i] );
	int tall = surface()->GetFontTall( _font ) + 4;
	SetSize( wide, tall );
	PerformLayout();
}


//-----------------------------------------------------------------------------
// Purpose: Set boolean value of baseclass variables.
//-----------------------------------------------------------------------------
void TextEntry::SetEnabled( bool state )
{
	Panel::SetEnabled( state );
	Repaint();
}

void TextEntry::OnKeyCodeTyped( KeyCode code )
{
	_cursorIsAtEnd = _putCursorAtEnd;
	_putCursorAtEnd = false;

	bool shift = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
	bool ctrl = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
	bool alt = input()->IsKeyDown( KEY_LALT ) || input()->IsKeyDown( KEY_RALT );
	if ( !input()->IsKeyDown( KEY_LWIN ) )
		(void)input()->IsKeyDown( KEY_RWIN );

	bool chain = true;
	if ( ctrl )
	{
		switch ( code )
		{
		case KEY_A:
			SelectAllText( false );
			if ( _dataChanged )
				FireActionSignal();
			return;
		case KEY_C:
		case KEY_INSERT:
			CopySelected();
			break;
		case KEY_V:
			DeleteSelected();
			Paste();
			break;
		case KEY_X:
			CopySelected();
			DeleteSelected();
			break;
		case KEY_Z:
			Undo();
			break;
		case KEY_ENTER:
			if ( _multiline )
			{
				DeleteSelected();
				SaveUndoState();
				InsertChar( L'\n' );
			}
			if ( _sendNewLines )
				PostActionSignal( new KeyValues( "TextNewLine" ) );
			break;
		case KEY_HOME:
			GotoTextStart();
			break;
		case KEY_END:
			GotoTextEnd();
			break;
		case KEY_LEFT:
			GotoWordLeft();
			break;
		case KEY_RIGHT:
			GotoWordRight();
			break;
		default:
			_select[1] = _cursorPos;
			if ( _dataChanged )
				FireActionSignal();
			else
			{
				_putCursorAtEnd = _cursorIsAtEnd;
				Panel::OnKeyCodeTyped( code );
			}
			return;
		}
		_select[1] = _cursorPos;
		if ( _dataChanged )
			FireActionSignal();
		return;
	}

	if ( alt )
	{
		_select[1] = _cursorPos;
		if ( _dataChanged )
			FireActionSignal();
		_putCursorAtEnd = _cursorIsAtEnd;
		Panel::OnKeyCodeTyped( code );
		return;
	}

	switch ( code )
	{
	case KEY_ENTER:
		if ( _catchEnterKey && _multiline )
		{
			chain = false;
			DeleteSelected();
			SaveUndoState();
			InsertChar( L'\n' );
		}
		else
		{
			chain = true;
		}
		if ( _sendNewLines )
			PostActionSignal( new KeyValues( "TextNewLine" ) );
		break;
	case KEY_BACKSPACE:
	{
		int x0 = 0;
		int x1 = 0;
		if ( GetSelectedRange( x0, x1 ) )
			DeleteSelected();
		else
			Backspace();
		chain = false;
		break;
	}
	case KEY_TAB:
	case KEY_ESCAPE:
	case KEY_LSHIFT:
	case KEY_RSHIFT:
	case KEY_F1:
	case KEY_F2:
	case KEY_F3:
	case KEY_F4:
	case KEY_F5:
	case KEY_F6:
	case KEY_F7:
	case KEY_F8:
	case KEY_F9:
	case KEY_F10:
	case KEY_F11:
	case KEY_F12:
		break;
	case KEY_INSERT:
		if ( !shift )
			break;
		DeleteSelected();
		Paste();
		chain = false;
		break;
	case KEY_DELETE:
		if ( shift )
		{
			CopySelected();
			DeleteSelected();
		}
		else
		{
			Delete();
		}
		chain = false;
		break;
	case KEY_HOME:
		if ( _multiline )
			GotoFirstOfLine();
		else
			GotoTextStart();
		chain = false;
		break;
	case KEY_END:
		GotoEndOfLine();
		chain = false;
		break;
	case KEY_PAGEUP:
		if ( _vertScrollBar != NULL || _multiline )
		{
			int value = _vertScrollBar != NULL ? _vertScrollBar->GetValue() : 0;
			if ( _multiline )
			{
				int lines = GetTall() / ( surface()->GetFontTall( _font ) + 1 );
				for ( int i = 0; i < lines; ++i )
					GotoUp();
			}
			if ( _vertScrollBar != NULL )
			{
				int window = _vertScrollBar->GetRangeWindow();
				int valueNow = _vertScrollBar->GetValue();
				_vertScrollBar->SetValue( value - ( window - ( value - valueNow ) ) - 1 );
			}
		}
		chain = false;
		break;
	case KEY_PAGEDOWN:
		if ( _vertScrollBar != NULL || _multiline )
		{
			int value = _vertScrollBar != NULL ? _vertScrollBar->GetValue() : 0;
			if ( _multiline )
			{
				int lines = GetTall() / ( surface()->GetFontTall( _font ) + 1 );
				for ( int i = 0; i < lines; ++i )
					GotoDown();
			}
			if ( _vertScrollBar != NULL )
			{
				int window = _vertScrollBar->GetRangeWindow();
				int valueNow = _vertScrollBar->GetValue();
				_vertScrollBar->SetValue( window + 2 * value - valueNow + 1 );
			}
		}
		chain = false;
		break;
	case KEY_UP:
		if ( !_multiline )
			break;
		GotoUp();
		chain = false;
		break;
	case KEY_DOWN:
		if ( !_multiline )
			break;
		GotoDown();
		chain = false;
		break;
	case KEY_LEFT:
		GotoLeft();
		chain = false;
		break;
	case KEY_RIGHT:
		GotoRight();
		chain = false;
		break;
	default:
		return;
	}

	_select[1] = _cursorPos;
	if ( _dataChanged )
		FireActionSignal();
	if ( chain )
	{
		_putCursorAtEnd = _cursorIsAtEnd;
		Panel::OnKeyCodeTyped( code );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Masks which keys get chained up
//			Maps keyboard input to text window functions.
//-----------------------------------------------------------------------------
void TextEntry::OnKeyTyped( wchar_t unichar )
{
	_cursorIsAtEnd = _putCursorAtEnd;
	_putCursorAtEnd = false;
	if ( unichar == L'\t' || iswprint( unichar ) == 0 )
		return;

	// do readonly keys
	if ( !IsEditable() )
	{
		Panel::OnKeyTyped( unichar );
		return;
	}
	if ( unichar != 0 )
	{
		DeleteSelected();
		SaveUndoState();
		InsertChar( unichar );
	}

	// select[1] is the location in the line where the blinking cursor started
	_select[1] = _cursorPos;
	if ( _dataChanged )
		FireActionSignal();
}

void TextEntry::OnMouseWheeled( int delta )
{
	if ( _vertScrollBar != NULL )
		MoveScrollBar( delta );
	else
		Panel::OnMouseWheeled( delta );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool TextEntry::RequestInfo( KeyValues* outputData )
{
	if ( _stricmp( outputData->GetName(), "GetText" ) == 0 )
	{
		wchar_t wbuf[256];
		GetText( wbuf, 255 );
		outputData->SetWString( "text", wbuf );
		return true;
	}
	if ( _stricmp( outputData->GetName(), "GetState" ) == 0 )
	{
		wchar_t wbuf[64];
		GetText( wbuf, 64 );
		outputData->SetInt( "state", (int)wcstol( wbuf, NULL, 10 ) );
		return true;
	}
	return Panel::RequestInfo( outputData );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TextEntry::ApplySettings( KeyValues* inResourceData )
{
	Panel::ApplySettings( inResourceData );
	SetTextHidden( inResourceData->GetInt( "textHidden", 0 ) != 0 );
	SetEditable( inResourceData->GetInt( "editable", 1 ) != 0 );
	SetMaximumCharCount( inResourceData->GetInt( "maxchars", -1 ) );
	m_bAllowNumericInputOnly = inResourceData->GetInt( "NumericInputOnly", 0 ) != 0;
	m_bAllowNonAsciiCharacters = inResourceData->GetInt( "unicode", 0 ) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TextEntry::GetSettings( KeyValues* outResourceData )
{
	Panel::GetSettings( outResourceData );
	outResourceData->SetInt( "textHidden", _hideText );
	outResourceData->SetInt( "editable", IsEditable() );
	outResourceData->SetInt( "maxchars", GetMaximumCharCount() );
	outResourceData->SetInt( "NumericInputOnly", m_bAllowNumericInputOnly );
	outResourceData->SetInt( "unicode", m_bAllowNonAsciiCharacters );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char* TextEntry::GetDescription()
{
	static char buf[1024];
	snprintf( buf, sizeof( buf ),
		"%s, bool textHidden, bool editable, bool unicode, bool NumericInputOnly, int maxchars",
		Panel::GetDescription() );
	return buf;
}

//-----------------------------------------------------------------------------
// Purpose: called when the text entry receives focus
//-----------------------------------------------------------------------------
void TextEntry::OnSetFocus()
{
	// see if we should highlight all on selection
	if ( _selectAllOnFirstFocus )
	{
		_select[0] = 0;
		_select[1] = m_TextStream.Count();
		_cursorPos = m_TextStream.Count();
		_selectAllOnFirstFocus = 0;
	}
	else if ( !input()->IsMouseDown( MOUSE_LEFT ) && !input()->IsMouseDown( MOUSE_RIGHT ) )
	{
		// if we've tabbed to this field then move to the end of the text
		GotoTextEnd();
		// clear any selection
		SelectNone();
	}
	Panel::OnSetFocus();
}
}
