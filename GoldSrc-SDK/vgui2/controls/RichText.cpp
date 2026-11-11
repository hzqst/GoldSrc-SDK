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

#include "RichText.h"
#include "Menu.h"
#include "ScrollBar.h"
#include "controls.h"

#include "../../engine/controls.h"
#include "ILocalize.h"
#include "vgui/Cursor.h"
#include "vgui/IInput.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\RichText.cpp" )
#else
#include <assert.h>
#define ASSERT_AT( expression, line )	assert( expression )
#endif

#if !defined( _WIN32 )
#define _snprintf snprintf
#endif

namespace vgui2
{
constexpr int kLineBreakSentinel = 999999;

ClickPanel::ClickPanel( Panel* parent ) : Panel()
{
	_textIndex = 0;
	SetParent( parent );
	AddActionSignalTarget( parent );
	SetCursor( dc_hand );
	SetPaintBackgroundEnabled( false );
	SetPaintEnabled( false );
	SetPaintBorderEnabled( false );
}

ClickPanel::~ClickPanel()
{
}

void ClickPanel::OnMousePressed( MouseCode code )
{
	if ( code == MOUSE_LEFT )
		PostActionSignal( new KeyValues( "ClickPanel", "index", _textIndex ) );
}

RichText::RichText( Panel* parent, const char* panelName, bool bTrustedInput )
	: Panel( parent, panelName ), m_TextStream( 128 ), m_CachedRenderState{}
{
	_disabledFgColor = Color( 0, 0, 0, 0 );
	_selectionColor = Color( 0, 0, 0, 0 );
	_selectionTextColor = Color( 0, 0, 0, 0 );
	_defaultTextColor = Color( 0, 0, 0, 0 );
	m_CachedRenderState.textColor = Color( 0, 0, 0, 0 );
	_font = 0;
	m_bRecalcLineBreaks = true;
	_mouseSelection = false;
	m_bTrustInput = bTrustedInput;
	m_pszInitialText = NULL;
	_cursorPos = 0;
	_mouseDragSelection = false;

	_vertScrollBar = new ScrollBar( this, "ScrollBar", true );
	_vertScrollBar->AddActionSignalTarget( this );
	_recalcSavedRenderState = true;
	_maxCharCount = 65536;
	AddActionSignalTarget( this );
	_select[0] = -1;
	_select[1] = -1;
	m_pEditMenu = NULL;
	SetCursor( dc_ibeam );
	GotoTextEnd();
	_defaultTextColor = Color( 0, 0, 0, 0 );
	InvalidateLineBreakStream();

	if ( IsProportional() )
	{
		int width;
		int height;
		GetProportionalBase( width, height );
		int screenWide;
		int screenTall;
		surface()->GetScreenSize( screenWide, screenTall );
		float scale = static_cast<float>( screenWide ) / static_cast<float>( width );
		_scrollBarSize = static_cast<int>( 18.0f * scale );
		_drawOffsetX = static_cast<int>( 3.0f * scale );
		_drawOffsetY = static_cast<int>( scale );
	}
	else
	{
		_scrollBarSize = 18;
		_drawOffsetX = 3;
		_drawOffsetY = 1;
	}

	TFormatStream stream = {};
	m_FormatStream.AddToTail( stream );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
RichText::~RichText()
{
	delete[] m_pszInitialText;
	delete m_pEditMenu;
}

void RichText::OnCursorEntered()
{
	_mouseDragSelection = false; // outside of window dont recieve drag scrolling ticks
}

//-----------------------------------------------------------------------------
// Purpose: When the cursor is outside the window, if we are holding the mouse
//			button down, then we want the window to scroll the text one char at a time using Ticks
//-----------------------------------------------------------------------------
void RichText::OnCursorExited()
{
	if ( _mouseSelection )
		_mouseDragSelection = true;
}

//-----------------------------------------------------------------------------
// Purpose: Turn off text selection code when mouse button is not down
//-----------------------------------------------------------------------------
void RichText::OnMouseCaptureLost()
{
	_mouseSelection = false;
}

//-----------------------------------------------------------------------------
// Purpose: Cuts the selected chars from the buffer and 
//          copies them into the clipboard
//-----------------------------------------------------------------------------
void RichText::CutSelected()
{
	CopySelected();
	RequestFocus( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the scrollbar slider is moved
//-----------------------------------------------------------------------------
void RichText::OnSliderMoved()
{
	_recalcSavedRenderState = true;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: if the default format color isn't set then set it
//-----------------------------------------------------------------------------
void RichText::SetFgColor( Color color )
{
	Panel::SetFgColor( color );
	if ( m_FormatStream.Count() == 1 && _defaultTextColor == m_FormatStream[0].color )
		m_FormatStream[0].color = color;
}

//-----------------------------------------------------------------------------
// Purpose: Handle mouse button up events
//-----------------------------------------------------------------------------
void RichText::OnMouseReleased( MouseCode )
{
	_mouseSelection = false;
	input()->SetMouseCapture( 0 );
	if ( _select[0] != -1 && _select[0] == _select[1] )
		_select[0] = -1;
}

void RichText::PerformLayout()
{
	Panel::PerformLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RichText::GetSettings( KeyValues* settings )
{
	Panel::GetSettings( settings );
	settings->SetInt( "maxchars", _maxCharCount );
	if ( m_pszInitialText )
		settings->SetString( "text", m_pszInitialText );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RichText::OnSetFocus()
{
	Panel::OnSetFocus();
}

//-----------------------------------------------------------------------------
// Purpose: Opens the web browser with the text
//-----------------------------------------------------------------------------
void RichText::OnTextClicked( const wchar_t* text )
{
	char ansi[512];
	localize()->ConvertUnicodeToANSI( text, ansi, 512 );
	system()->ShellExecute( "open", ansi );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char* RichText::GetDescription()
{
	static char buf[1024];
	_snprintf( buf, sizeof( buf ), "%s, string text", Panel::GetDescription() );
	return buf;
}

bool RichText::RequestInfo( KeyValues* outputData )
{
	if ( _stricmp( outputData->GetName(), "GetText" ) != 0 )
		return Panel::RequestInfo( outputData );

	wchar_t wbuf[512];
	int count = m_TextStream.Count();
	int copied = 0;
	while ( copied < count && copied != 511 )
	{
		wbuf[copied] = m_TextStream[copied];
		++copied;
	}
	wbuf[copied] = 0;
	wbuf[511] = 0;
	outputData->SetWString( "text", wbuf );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Scrolls the list according to the mouse wheel movement
//-----------------------------------------------------------------------------
void RichText::OnMouseWheeled( int delta )
{
	MoveScrollBar( delta );
}

//-----------------------------------------------------------------------------
// Purpose: configures colors
//-----------------------------------------------------------------------------
void RichText::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	_font = scheme->GetFontEx( "Default", IsProportional(), IsHDProportional() );
	SetFgColor( GetSchemeColor( "WindowFgColor", scheme ) );
	SetBgColor( GetSchemeColor( "WindowBgColor", scheme ) );
	_disabledFgColor = GetSchemeColor( "WindowDisabledFgColor", scheme );
	_selectionTextColor = GetSchemeColor( "SelectionFgColor", GetFgColor(), scheme );
	_selectionColor = GetSchemeColor( "SelectionBgColor", scheme );
	SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
}

void RichText::OnMouseDoublePressed( MouseCode code )
{
	if ( code != MOUSE_LEFT )
		return;

	OnMousePressed( MOUSE_LEFT );
	int selectionStart = _cursorPos;
	if ( _cursorPos > 0 )
	{
		GotoWordLeft();
		selectionStart = _cursorPos;
	}

	int size = m_TextStream.Count();
	int i = _cursorPos;
	for ( ;; )
	{
		_cursorPos = i + 1;
		if ( i + 1 >= size )
			break;
		if ( iswspace( m_TextStream[i + 1] ) )
			break;
		++i;
	}

	int end = _cursorPos;
	for ( ;; )
	{
		_cursorPos = ++end;
		if ( end >= size )
			break;
		if ( !iswspace( m_TextStream[end] ) )
			goto placed;
	}
	if ( end > size )
		_cursorPos = size;
placed:
	Repaint();
	int cursor = _cursorPos;
	if ( cursor > 0 && cursor <= m_TextStream.Count() && iswspace( m_TextStream[cursor - 1] ) )
		_cursorPos = --cursor;
	_select[1] = cursor;
	_mouseSelection = true;
	_select[0] = selectionStart;
}

void RichText::OnClickPanel( int index )
{
#if defined( _WIN32 )
	const int textPositionLimit = 1023;
#else
	const int textPositionLimit = 2047;
#endif
	_currentTextClickable = true;
	int formatCount = m_FormatStream.Count();
	int formatIndex = FindFormatStreamIndexForTextStreamPos( index );
	wchar_t wBuf[512];
	int length = 0;
	if ( static_cast<unsigned int>( index ) < static_cast<unsigned int>( textPositionLimit ) && index < m_TextStream.Count() )
	{
		int remaining = m_TextStream.Count() - index;
		bool clickable = m_FormatStream[formatIndex].textClickable;
		int position = index;
		while ( length != remaining )
		{
			if ( formatIndex >= 0 && formatIndex < formatCount
				 && m_FormatStream[formatIndex].textStreamIndex == position )
			{
				clickable = m_FormatStream[formatIndex].textClickable;
				_pixelsIndent = m_FormatStream[formatIndex].pixelsIndent;
				++formatIndex;
			}
			if ( !clickable )
				break;
			wBuf[length] = m_TextStream[index + length];
			++length;
			++position;
			if ( position == textPositionLimit )
				break;
		}
	}
	wBuf[length] = 0;
	PostActionSignal( new KeyValues( "TextClicked", "text", wBuf ) );
	OnTextClicked( wBuf );
}

//-----------------------------------------------------------------------------
// Purpose: Wipe line breaks after the size of a panel has been changed
//-----------------------------------------------------------------------------
void RichText::OnSizeChanged( int wide, int tall )
{
	Panel::OnSizeChanged( wide, tall );
	// blow away the line breaks list 
	_invalidateVerticalScrollbarSlider = true;
	InvalidateLineBreakStream();
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Masks which keys get chained up
//			Maps keyboard input to text window functions.
//-----------------------------------------------------------------------------
void RichText::OnKeyCodeTyped( KeyCode code )
{
	bool shift = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
	bool ctrl = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
	bool alt = input()->IsKeyDown( KEY_LALT ) || input()->IsKeyDown( KEY_RALT );
	( void )( input()->IsKeyDown( KEY_LWIN ) || input()->IsKeyDown( KEY_RWIN ) );

	if ( ctrl )
	{
		switch ( code )
		{
		case KEY_C:
		case KEY_X:
		case KEY_INSERT:
			CopySelected();
			_select[1] = _cursorPos;
			return;
		case KEY_HOME:
		case KEY_PAGEUP:
			GotoTextStart();
			_select[1] = _cursorPos;
			return;
		case KEY_END:
		case KEY_PAGEDOWN:
			GotoTextEnd();
			_select[1] = _cursorPos;
			return;
		default:
			_select[1] = _cursorPos;
			Panel::OnKeyCodeTyped( code );
			return;
		}
	}

	if ( alt )
	{
		_select[1] = _cursorPos;
		Panel::OnKeyCodeTyped( code );
		return;
	}

	switch ( code )
	{
	case KEY_ENTER:
	case KEY_TAB:
	case KEY_ESCAPE:
	case KEY_LSHIFT:
	case KEY_RSHIFT:
		_select[1] = _cursorPos;
		Panel::OnKeyCodeTyped( code );
		return;
	case KEY_DELETE:
		if ( shift )
			CopySelected();
		_select[1] = _cursorPos;
		return;
	case KEY_HOME:
		GotoTextStart();
		_select[1] = _cursorPos;
		return;
	case KEY_END:
		GotoTextEnd();
		_select[1] = _cursorPos;
		return;
	case KEY_PAGEUP:
		// if there is a scroll bar scroll down one rangewindow
		if ( _vertScrollBar->IsVisible() )
		{
			int window = _vertScrollBar->GetRangeWindow();
			int value = _vertScrollBar->GetValue();
			_vertScrollBar->SetValue( value - window - 1 );
		}
		_select[1] = _cursorPos;
		return;
	case KEY_PAGEDOWN:
		// if there is a scroll bar scroll down one rangewindow
		if ( _vertScrollBar->IsVisible() )
		{
			int window = _vertScrollBar->GetRangeWindow();
			int value = _vertScrollBar->GetValue();
			_vertScrollBar->SetValue( value + window + 1 );
		}
		_select[1] = _cursorPos;
		return;
	default:
		return;
	}
}

void RichText::CursorToPixelSpace( int cursorPos, int& cx, int& cy )
{
	_pixelsIndent = 0;
	int y = _drawOffsetY;
	int x = _drawOffsetX;
	int lineBreakIndexIndex;
	int textIndex = GetStartDrawIndex( lineBreakIndexIndex );

	if ( textIndex < m_TextStream.Count() )
	{
		wchar_t ch = m_TextStream[textIndex];
		// if we've found the position, break
		if ( cursorPos == textIndex )
		{
			// if we've passed a line break go to that
			if ( m_LineBreaks[lineBreakIndexIndex] == textIndex )
			{
				x = _drawOffsetX + _pixelsIndent;
				y += _drawOffsetY + surface()->GetFontTall( _font );
			}
		}
		else
		{
			while ( true )
			{
				// if we've passed a line break go to that
				if ( m_LineBreaks[lineBreakIndexIndex] == textIndex )
				{
					x = _drawOffsetX + _pixelsIndent;
					++lineBreakIndexIndex;
					y += _drawOffsetY + surface()->GetFontTall( _font );
				}
				// add to the current position
				x += surface()->GetCharacterWidth( _font, ch );
				++textIndex;
				if ( textIndex >= m_TextStream.Count() )
					break;
				ch = m_TextStream[textIndex];
				if ( cursorPos == textIndex )
				{
					if ( m_LineBreaks[lineBreakIndexIndex] == textIndex )
					{
						x = _drawOffsetX + _pixelsIndent;
						y += _drawOffsetY + surface()->GetFontTall( _font );
					}
					break;
				}
			}
		}
	}
	cx = x;
	cy = y;
}

//-----------------------------------------------------------------------------
// Purpose: Sends a message if the data has changed
//          Turns off any selected text in the window if we are not using the edit menu
//-----------------------------------------------------------------------------
void RichText::OnKillFocus()
{
	// check if we clicked the right mouse button or if it is down
	bool mouseRightPressed = input()->WasMousePressed( MOUSE_RIGHT );
	bool mouseRightReleased = input()->WasMouseReleased( MOUSE_RIGHT );
	bool mouseRightDown = input()->IsMouseDown( MOUSE_RIGHT );
	if ( mouseRightPressed || mouseRightReleased || mouseRightDown )
	{
		if ( _select[0] != -1 )
		{
			int start = _select[0];
			int end = _select[1];
			if ( start > end )
			{
				int swap = start;
				start = end;
				end = swap;
			}
			int startX;
			int startY;
			int endX;
			int endY;
			CursorToPixelSpace( start, startX, startY );
			CursorToPixelSpace( end, endX, endY );
			int cursorX;
			int cursorY;
			input()->GetCursorPos( cursorX, cursorY );
			ScreenToLocal( cursorX, cursorY );
			endY += surface()->GetFontTall( _font );
			if ( startY < cursorY && endY > cursorY )
				return;
		}
	}
	_select[0] = -1;
	Repaint();
	Panel::OnKillFocus();
}

//-----------------------------------------------------------------------------
// Purpose: Converts local pixel coordinates to an index in the text buffer
//-----------------------------------------------------------------------------
int RichText::PixelToCursorSpace( int cx, int cy )
{
	int fontTall = surface()->GetFontTall( _font );
	_pixelsIndent = 0;
	int y = _drawOffsetY;
	int x = _drawOffsetX;
	int lineBreakIndexIndex;
	int textIndex = GetStartDrawIndex( lineBreakIndexIndex );

	if ( _recalcSavedRenderState && m_TextStream.Count() != 0 )
		RecalculateDefaultState( textIndex );
	_pixelsIndent = m_CachedRenderState.pixelsIndent;
	_currentTextClickable = m_CachedRenderState.textClickable;
	if ( textIndex >= m_TextStream.Count() )
		return textIndex;

	bool onRightLine = false;
	int yEnd = y;
	int streamIndex = textIndex;
	while ( true )
	{
		wchar_t ch = m_TextStream[streamIndex];
		// if we are on the right line but off the end of if put the cursor at the end of the line
		if ( m_LineBreaks[lineBreakIndexIndex] == textIndex )
		{
			int linePos = m_LineBreaks[lineBreakIndexIndex];
			x = _drawOffsetX + _pixelsIndent;
			yEnd += _drawOffsetY + surface()->GetFontTall( _font );
			++lineBreakIndexIndex;
			if ( onRightLine )
				return linePos;
		}

		int charWidth = surface()->GetCharacterWidth( _font, ch );
		bool inBand = y > cy || ( cy >= yEnd && cy < yEnd + fontTall + _drawOffsetY );
		if ( !inBand && !onRightLine )
		{
			x += charWidth;
			++textIndex;
			++streamIndex;
			if ( textIndex >= m_TextStream.Count() )
				return textIndex;
			continue;
		}

		if ( GetWide() >= cx && ( cx < _drawOffsetX + _pixelsIndent || y > cy ) )
			return textIndex;
		if ( cx >= x )
		{
			if ( cx < charWidth + x )
			{
				if ( static_cast<double>( charWidth ) * 0.5 + static_cast<double>( x ) > static_cast<double>( cx ) )
					return textIndex;
				return textIndex + 1;
			}
			x += charWidth;
			onRightLine = true;
		}
		else
		{
			x += charWidth;
			onRightLine = true;
		}
		++textIndex;
		++streamIndex;
		if ( textIndex >= m_TextStream.Count() )
			return textIndex;
	}
}

//-----------------------------------------------------------------------------
// Purpose: We want single line windows to scroll horizontally and select text
//          in response to clicking and holding outside window
//-----------------------------------------------------------------------------
void RichText::OnMouseFocusTicked()
{
	if ( !_mouseDragSelection ) // text is being selected via mouse clicking and dragging
		return;
	OnCursorMoved( 0, 0 ); // we want the text to scroll as if we were dragging
}

//-----------------------------------------------------------------------------
// Purpose: Handle selection of text by mouse
//-----------------------------------------------------------------------------
void RichText::OnCursorMoved( int, int )
{
	if ( !_mouseSelection )
		return;
	int x;
	int y;
	input()->GetCursorPos( x, y );
	ScreenToLocal( x, y );
	int cursor = PixelToCursorSpace( x, y );
	_cursorPos = cursor;
	if ( cursor != _select[1] )
	{
		_select[1] = cursor;
		Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws a character in the panel
// Input:	ch	- character to draw
//			font- font to use
//			x,y	- pixel location to draw char at
// Output:	returns the width of the character drawn
//-----------------------------------------------------------------------------
int RichText::DrawChar( wchar_t ch, TRenderState& renderState, HFont font, int index )
{
	if ( !iswprint( ch ) )
		return 0;
	int width = surface()->GetCharacterWidth( font, ch );
	int tall = surface()->GetFontTall( font );
	int selectionStart = _select[0];
	int selectionEnd = -1;
	if ( selectionStart != -1 )
	{
		selectionEnd = _select[1];
		if ( selectionStart > selectionEnd )
		{
			int swap = selectionStart;
			selectionStart = selectionEnd;
			selectionEnd = swap;
		}
	}
	if ( index < selectionEnd && index >= selectionStart )
	{
		// draw background selection color
		surface()->DrawSetColor( _selectionColor );
		surface()->DrawFilledRect( renderState.x, renderState.y, width + renderState.x, tall + renderState.y + 1 );
		// reset text color
		surface()->DrawSetTextColor( _selectionTextColor );
	}
	else
	{
		surface()->DrawSetTextColor( renderState.textColor );
	}
	surface()->DrawSetTextPos( renderState.x, renderState.y );
	surface()->DrawUnicodeChar( ch );
	return width;
}

//-----------------------------------------------------------------------------
// Purpose: Draws the text in the panel
//-----------------------------------------------------------------------------
void RichText::PaintBackground()
{
	surface()->DrawSetColor( GetBgColor() );
	int wide;
	int tall;
	GetSize( wide, tall );
	surface()->DrawFilledRect( 0, 0, wide, tall );
	surface()->DrawSetTextFont( _font );

	for ( int i = 0; i < _clickableTextPanels.GetCount(); ++i )
		_clickableTextPanels[i]->SetVisible( false );
	if ( m_TextStream.Count() == 0 )
		return;

	int lineBreakIndexIndex;
	int textIndex = GetStartDrawIndex( lineBreakIndexIndex );

	_currentTextClickable = false;
	_clickableTextIndex = GetClickableTextIndexStart( textIndex );
	if ( _recalcSavedRenderState && m_TextStream.Count() != 0 )
		RecalculateDefaultState( textIndex );

	TRenderState renderState = m_CachedRenderState;
	_currentTextClickable = m_CachedRenderState.textClickable;
	renderState.textClickable = m_CachedRenderState.textClickable;
	int pixelsIndent = m_CachedRenderState.pixelsIndent;
	_pixelsIndent = pixelsIndent;
	int appliedFormat = renderState.formatStreamIndex;
	renderState.formatStreamIndex = appliedFormat + 1;
	renderState.textColor = m_FormatStream[appliedFormat].color;
	bool startedClickable = m_CachedRenderState.textClickable;
	// where to start drawing
	renderState.x = _drawOffsetX + pixelsIndent;
	renderState.y = _drawOffsetY;
	if ( startedClickable )
		_clickableTextIndex = textIndex;
	if ( textIndex >= m_TextStream.Count() || renderState.y >= tall )
	{
		if ( startedClickable )
			FinishingURL( renderState.x, renderState.y );
		return;
	}

	while ( textIndex < m_TextStream.Count() && renderState.y < tall )
	{
		int formatIndex = renderState.formatStreamIndex;
		if ( formatIndex >= 0 && formatIndex < m_FormatStream.Count()
			 && m_FormatStream[formatIndex].textStreamIndex == textIndex )
		{
			bool clickable = m_FormatStream[formatIndex].textClickable;
			renderState.textColor = m_FormatStream[formatIndex].color;
			renderState.textClickable = clickable;
			int indent = m_FormatStream[formatIndex].pixelsIndent;
			if ( indent != renderState.pixelsIndent )
				renderState.x = indent + _drawOffsetX;
			renderState.pixelsIndent = indent;
			_pixelsIndent = indent;
			renderState.formatStreamIndex = formatIndex + 1;
			if ( _currentTextClickable != clickable )
			{
				if ( clickable )
				{
					int panelIndex = ++_clickableTextIndex;
					if ( panelIndex >= 0 && panelIndex < _clickableTextPanels.GetCount() )
					{
						ClickPanel* panel = _clickableTextPanels[panelIndex];
						if ( panel )
							panel->SetPos( renderState.x, renderState.y );
					}
				}
				else
				{
					FinishingURL( renderState.x, renderState.y );
				}
				_currentTextClickable = clickable;
			}
		}

		// if we've passed a line break go to that
		if ( m_LineBreaks[lineBreakIndexIndex] == textIndex )
		{
			if ( _currentTextClickable )
				FinishingURL( renderState.x, renderState.y );
			renderState.x = _drawOffsetX + _pixelsIndent;
			renderState.y += _drawOffsetY + surface()->GetFontTall( _font );
			++lineBreakIndexIndex;
			if ( renderState.textClickable )
			{
				int panelIndex = ++_clickableTextIndex;
				if ( panelIndex >= 0 && panelIndex < _clickableTextPanels.GetCount() )
				{
					ClickPanel* panel = _clickableTextPanels[panelIndex];
					if ( panel )
						panel->SetPos( renderState.x, renderState.y );
				}
			}
		}

		wchar_t ch = m_TextStream[textIndex];
		int drawn = textIndex;
		// draw the character and update xposition  
		renderState.x += DrawChar( ch, renderState, _font, drawn );
		++textIndex;
		if ( textIndex >= m_TextStream.Count() || renderState.y >= tall )
		{
			if ( renderState.textClickable )
				FinishingURL( renderState.x, renderState.y );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finish drawing url
//-----------------------------------------------------------------------------
void RichText::FinishingURL( int x, int y )
{
	if ( _clickableTextIndex < 0 || _clickableTextIndex >= _clickableTextPanels.GetCount() )
		return;
	// finishing URL
	ClickPanel* panel = _clickableTextPanels[_clickableTextIndex];
	if ( !panel )
		return;
	int px;
	int py;
	panel->GetPos( px, py );
	int fontTall = surface()->GetFontTall( _font );
	panel->SetSize( x - px, y - py + fontTall );
	panel->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int RichText::GetClickableTextIndexStart( int startIndex )
{
	int count = _clickableTextPanels.GetCount();
	for ( int i = 0; i < count; ++i )
	{
		if ( startIndex < _clickableTextPanels[i]->_textIndex )
			return i - 1;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Recalcultes the formatting state from the specified index
//-----------------------------------------------------------------------------
void RichText::RecalculateDefaultState( int startIndex )
{
	if ( m_TextStream.Count() == 0 )
		return;
#if defined( _WIN32 )
	ASSERT_AT( startIndex < m_TextStream.Count(), 650 );
#endif

	Color foreground = GetFgColor();
	_currentTextClickable = false;
	m_CachedRenderState.textColor = foreground;
	_pixelsIndent = 0;

	_clickableTextIndex = GetClickableTextIndexStart( startIndex );
	// find where in the formatting stream we need to be
	GenerateRenderStateForTextStreamIndex( startIndex, m_CachedRenderState );
	_recalcSavedRenderState = false;
}

//-----------------------------------------------------------------------------
// Purpose: updates a render state based on the formatting and color streams
//-----------------------------------------------------------------------------
bool RichText::UpdateRenderState( int textStreamPos, TRenderState& renderState )
{
	int formatIndex = renderState.formatStreamIndex;
	if ( formatIndex < 0 || formatIndex >= m_FormatStream.Count() )
		return false;
	if ( m_FormatStream[formatIndex].textStreamIndex != textStreamPos )
		return false;
	renderState.textColor = m_FormatStream[formatIndex].color;
	renderState.textClickable = m_FormatStream[formatIndex].textClickable;
	int indent = m_FormatStream[formatIndex].pixelsIndent;
	int previousIndent = renderState.pixelsIndent;
	renderState.pixelsIndent = indent;
	if ( indent != previousIndent )
		renderState.x = indent + _drawOffsetX;
	_pixelsIndent = indent;
	++renderState.formatStreamIndex;
	return true;
}

int RichText::FindFormatStreamIndexForTextStreamPos( int textStreamIndex )
{
	int result = 0;
	int count = m_FormatStream.Count();
	if ( count > 0 && textStreamIndex >= m_FormatStream[0].textStreamIndex )
	{
		while ( result + 1 != count )
		{
			if ( m_FormatStream[result + 1].textStreamIndex > textStreamIndex )
				break;
			++result;
		}
		if ( result >= count )
			return 0;
	}
	return result;
}

//-----------------------------------------------------------------------------
// Purpose: Generates a base renderstate given a index into the text stream
//-----------------------------------------------------------------------------
void RichText::GenerateRenderStateForTextStreamIndex( int textStreamIndex, TRenderState& renderState )
{
	int formatIndex = FindFormatStreamIndexForTextStreamPos( textStreamIndex );
	renderState.formatStreamIndex = formatIndex;
	renderState.textColor = m_FormatStream[formatIndex].color;
	renderState.pixelsIndent = m_FormatStream[formatIndex].pixelsIndent;
	renderState.textClickable = m_FormatStream[formatIndex].textClickable;
}

//-----------------------------------------------------------------------------
// Purpose: inserts a color change into the formatting stream
//-----------------------------------------------------------------------------
void RichText::InsertColorChange( Color color )
{
	int last = m_FormatStream.Count() - 1;
	TFormatStream& current = m_FormatStream[last];
	if ( current.color == color )
		return;
	if ( current.textStreamIndex == m_TextStream.Count() )
	{
		current.color = color;
		return;
	}
	TFormatStream next = current;
	next.color = color;
	next.textStreamIndex = m_TextStream.Count();
	m_FormatStream.AddToTail( next );
}

//-----------------------------------------------------------------------------
// Purpose: inserts an indent change into the formatting stream
//-----------------------------------------------------------------------------
void RichText::InsertIndentChange( int pixelsIndent )
{
	if ( pixelsIndent > 255 )
		pixelsIndent = 255;
	if ( pixelsIndent < 0 )
		pixelsIndent = 0;

	int last = m_FormatStream.Count() - 1;
	TFormatStream& current = m_FormatStream[last];
	if ( current.pixelsIndent == pixelsIndent )
		return;
	if ( current.textStreamIndex == m_TextStream.Count() )
	{
		current.pixelsIndent = pixelsIndent;
		return;
	}
	TFormatStream next = current;
	next.pixelsIndent = pixelsIndent;
	next.textStreamIndex = m_TextStream.Count();
	m_FormatStream.AddToTail( next );
}

//-----------------------------------------------------------------------------
// Purpose: Inserts character end for clickable text, eg. URLS
//-----------------------------------------------------------------------------
void RichText::InsertClickableTextEnd()
{
	int last = m_FormatStream.Count() - 1;
	TFormatStream& current = m_FormatStream[last];
	if ( !current.textClickable )
		return;
	if ( current.textStreamIndex == m_TextStream.Count() )
	{
		current.textClickable = false;
		return;
	}
	TFormatStream next = current;
	next.textClickable = false;
	next.textStreamIndex = m_TextStream.Count();
	m_FormatStream.AddToTail( next );
}

//-----------------------------------------------------------------------------
// Purpose: moves x,y to the Start of the next line of text
//-----------------------------------------------------------------------------
void RichText::AddAnotherLine( int& cx, int& cy )
{
	cx = _drawOffsetX + _pixelsIndent;
	cy += _drawOffsetY + surface()->GetFontTall( _font );
}

//-----------------------------------------------------------------------------
// Purpose: Recalculate where the vertical scroll bar slider should be 
//			based on the current cursor line we are on.
//-----------------------------------------------------------------------------
void RichText::LayoutVerticalScrollBarSlider()
{
	_invalidateVerticalScrollbarSlider = false;
	// set up the scrollbar
	if ( !_vertScrollBar->IsVisible() )
		return;

	// see where the scrollbar currently is
	int value = _vertScrollBar->GetValue();
	int rangeMin;
	int rangeMax;
	_vertScrollBar->GetRange( rangeMin, rangeMax );
	bool atBottom = false;
	if ( rangeMax != 0 )
	{
		int rangeWindow = _vertScrollBar->GetRangeWindow();
		if ( rangeMin + value + rangeWindow == rangeMax )
			atBottom = true;
	}

	int wide;
	int tall;
	GetSize( wide, tall );
	int left;
	int top;
	int right;
	int bottom;
	GetInset( left, top, right, bottom );
	wide -= top;
	_vertScrollBar->SetPos( wide - _scrollBarSize - 1, 0 );
	// scrollbar is inside the borders.
	_vertScrollBar->SetSize( _scrollBarSize, tall - bottom - right );

	int fontTall = surface()->GetFontTall( _font );
	int displayLines = tall / ( _drawOffsetY + fontTall );
	int lineCount = m_LineBreaks.Count();
	if ( displayLines < lineCount && displayLines != 0 )
	{
		// set the scrollbars range
		_vertScrollBar->SetRange( 0, lineCount );
		_vertScrollBar->SetRangeWindow( displayLines );
		_vertScrollBar->SetEnabled( true );
		// this should make it scroll one line at a time
		_vertScrollBar->SetButtonPressedScrollValue( 1 );
		if ( atBottom )
			_vertScrollBar->SetValue( lineCount - displayLines );
		_vertScrollBar->InvalidateLayout( false, false );
		_vertScrollBar->Repaint();
	}
	else
	{
		// disable the scrollbar
		_vertScrollBar->SetEnabled( false );
		_vertScrollBar->SetRange( 0, lineCount );
		_vertScrollBar->SetRangeWindow( lineCount );
		_vertScrollBar->SetValue( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether a vertical scrollbar is visible
//-----------------------------------------------------------------------------
void RichText::SetVerticalScrollbar( bool state )
{
	if ( _vertScrollBar->IsVisible() != state )
	{
		_vertScrollBar->SetVisible( state );
		InvalidateLayout( false, false );
	}
}

void RichText::CreateEditMenu()
{
	delete m_pEditMenu;
	m_pEditMenu = new Menu( this, "EditMenu" );
	// add cut/copy/paste drop down options if its editable, just copy if it is not
	m_pEditMenu->AddMenuItem( "C&opy", new KeyValues( "DoCopySelected" ), this, NULL );
	m_pEditMenu->SetVisible( false );
	m_pEditMenu->SetParent( this );
	m_pEditMenu->AddActionSignalTarget( this );
}

//-----------------------------------------------------------------------------
// Purpose: Scrolls the list 
// Input  : delta - amount to move scrollbar up
//-----------------------------------------------------------------------------
void RichText::MoveScrollBar( int delta )
{
	if ( !_vertScrollBar->IsVisible() )
		return;
	int value = _vertScrollBar->GetValue();
	_vertScrollBar->SetValue( value - 3 * delta );
	_recalcSavedRenderState = true;
}

//-----------------------------------------------------------------------------
// Purpose: set the maximum number of chars in the text buffer
//-----------------------------------------------------------------------------
void RichText::SetMaximumCharCount( int maxChars )
{
	_maxCharCount = maxChars;
}

//-----------------------------------------------------------------------------
// Purpose: Find out what line the cursor is on
//-----------------------------------------------------------------------------
int RichText::GetCursorLine()
{
	return m_LineBreaks[m_LineBreaks.Count() - 1];
}

void RichText::GotoWordRight()
{
	int size = m_TextStream.Count();
	int i = _cursorPos + 1;
	while ( true )
	{
		_cursorPos = i;
		if ( i >= size )
			break;
		if ( iswspace( m_TextStream[i] ) )
			break;
		++i;
	}

	int position = i;
	while ( true )
	{
		_cursorPos = ++position;
		if ( position >= size )
			break;
		if ( !iswspace( m_TextStream[position] ) )
		{
			Repaint();
			return;
		}
	}
	if ( position > size )
		_cursorPos = size;
	// now we are at the start of the next word
	Repaint();
}

void RichText::GotoWordLeft()
{
	int cursor = _cursorPos;
	if ( cursor <= 0 )
		return;

	int i = cursor - 1;
	int nonSpace = i;
	while ( true )
	{
		nonSpace = i;
		_cursorPos = i;
		if ( i == -1 )
			break;
		wchar_t ch = m_TextStream[i];
		if ( !iswspace( ch ) )
			break;
		--i;
	}

	int previous = nonSpace - 1;
	while ( true )
	{
		_cursorPos = previous;
		if ( previous < 0 )
			break;
		if ( iswspace( m_TextStream[previous] ) )
			break;
		nonSpace = previous;
		--previous;
	}
	_cursorPos = nonSpace;
	// now we are at the start of the previous word
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Move cursor to the Start of the text buffer
//-----------------------------------------------------------------------------
void RichText::GotoTextStart()
{
	_cursorPos = 0;	// set cursor to start
	_invalidateVerticalScrollbarSlider = true;
	// force scrollbar to the top
	_vertScrollBar->SetValue( 0 );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Move cursor to the end of the text buffer
//-----------------------------------------------------------------------------
void RichText::GotoTextEnd()
{
	_invalidateVerticalScrollbarSlider = true;
	_cursorPos = m_TextStream.Count();	// set cursor to end of buffer
	int rangeMin;
	int rangeMax;
	_vertScrollBar->GetRange( rangeMin, rangeMax );
	_vertScrollBar->SetValue( rangeMax );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Culls the text stream down to a managable size
//-----------------------------------------------------------------------------
void RichText::TruncateTextStream()
{
	if ( _maxCharCount < 1 )
		return;

	// choose a point to cull at
	int cullPos = _maxCharCount / 2;
	// kill half the buffer
	m_TextStream.RemoveMultiple( 0, cullPos );

	// work out where in the format stream we can start
	int formatIndex = FindFormatStreamIndexForTextStreamPos( cullPos );
	if ( formatIndex > 0 )
	{
		// take a copy, make it first
		m_FormatStream[0] = m_FormatStream[formatIndex];
		m_FormatStream[0].textStreamIndex = 0;
		// kill the others
		m_FormatStream.RemoveMultiple( 1, formatIndex );
	}

	// renormalize the remainder of the format stream
	for ( int i = 1; i < m_FormatStream.Count(); i++ )
	{
#if defined( _WIN32 )
		ASSERT_AT( m_FormatStream[i].textStreamIndex > cullPos, 1599 );
#endif
		m_FormatStream[i].textStreamIndex -= cullPos;
	}

	// mark everything to be recalculated
	InvalidateLineBreakStream();
	InvalidateLayout( false, false );
	_invalidateVerticalScrollbarSlider = true;
}

//-----------------------------------------------------------------------------
// Purpose: Insert a character into the text buffer
//-----------------------------------------------------------------------------
void RichText::InsertChar( wchar_t ch )
{
	if ( ch == L'\r' )
		return;
	if ( _maxCharCount > 0 && _maxCharCount < m_TextStream.Count() )
		TruncateTextStream();
	// insert the new char at the end of the buffer
	m_TextStream.AddToTail( ch );
	// mark the linebreak steam as needing recalculating from that point
	_recalculateBreaksIndex = m_LineBreaks.Count() - 2;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Insertsa a unicode string into the buffer
//-----------------------------------------------------------------------------
void RichText::InsertString( const wchar_t* wszText )
{
	for ( const wchar_t* text = wszText; *text; ++text )
		InsertChar( *text );
	InvalidateLayout( false, false );
	m_bRecalcLineBreaks = true;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Insert a string into the text buffer, this is just a series
//			of char inserts because we have to check each char is ok to insert
//-----------------------------------------------------------------------------
void RichText::InsertString( const char* text )
{
	if ( text[0] == '#' )
	{
		const wchar_t* localized = localize()->Find( text );
		if ( localized )
		{
			InsertString( localized );
			return;
		}
	}

	size_t length = strlen( text );
	if ( length >= INT_MAX / sizeof( wchar_t ) )
		return;

	int characters = (int)length + 1;
	int bytes = characters * (int)sizeof( wchar_t );
	CUtlMemory<wchar_t> unicode( 0, characters );
	if ( !unicode.Base() )
		return;
	localize()->ConvertANSIToUnicode( text, unicode.Base(), bytes );
	unicode[characters - 1] = 0;
	InsertString( unicode.Base() );
}

//-----------------------------------------------------------------------------
// Purpose: Declare a selection empty
//-----------------------------------------------------------------------------
void RichText::SelectNone()
{
	// tag the selection as empty
	_select[0] = -1;
	Repaint();
}

bool RichText::GetSelectedRange( int& cx0, int& cx1 )
{
	// if there is nothing selected return false
	if ( _select[0] == -1 )
		return false;
	// sort the two position so cx0 is the smallest
	cx0 = _select[0];
	cx1 = _select[1];
	if ( cx1 < cx0 )
	{
		int swap = cx0;
		cx0 = cx1;
		cx1 = swap;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Opens the cut/copy/paste dropdown menu
//-----------------------------------------------------------------------------
void RichText::OpenEditMenu()
{
#if defined( _WIN32 )
	ASSERT_AT( m_pEditMenu, 1249 );
#endif
	int cursorX;
	int cursorY;
	input()->GetCursorPos( cursorX, cursorY );
	if ( _select[0] == -1 )
	{
		m_pEditMenu->SetItemEnabled( "&Cut", false );
		m_pEditMenu->SetItemEnabled( "C&opy", false );
	}
	else	// there is nothing selected, disable cut/copy options
	{
		m_pEditMenu->SetItemEnabled( "&Cut", true );
		m_pEditMenu->SetItemEnabled( "C&opy", true );
	}
	m_pEditMenu->SetVisible( true );
	m_pEditMenu->RequestFocus( 0 );
	m_pEditMenu->InvalidateLayout( true, false );

	int menuWide;
	int menuTall;
	m_pEditMenu->GetSize( menuWide, menuTall );
	int screenWide;
	int screenTall;
	surface()->GetScreenSize( screenWide, screenTall );
	if ( screenWide - menuWide <= cursorX )
	{
		int y = cursorY;
		if ( screenTall - menuTall <= cursorY )
			y = cursorY - menuTall;
		m_pEditMenu->SetPos( cursorX - menuWide, y );
	}
	else
	{
		int y = cursorY;
		if ( screenTall - menuTall <= cursorY )
			y = cursorY - menuTall;
		m_pEditMenu->SetPos( cursorX, y );
	}
	m_pEditMenu->RequestFocus( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Handle mouse button down events.
//-----------------------------------------------------------------------------
void RichText::OnMousePressed( MouseCode code )
{
	if ( code == MOUSE_RIGHT )
	{
		CreateEditMenu();
		OpenEditMenu();
		return;
	}
	if ( code != MOUSE_LEFT )
		return;

	_select[0] = -1;
	Repaint();
	int x;
	int y;
	input()->GetCursorPos( x, y );
	ScreenToLocal( x, y );
	_cursorPos = PixelToCursorSpace( x, y );
	// enter selection mode
	input()->SetMouseCapture( GetVPanel() );
	_mouseSelection = true;
	if ( _select[0] < 0 )
		_select[0] = _cursorPos;
	_select[1] = _cursorPos;
	RequestFocus( 0 );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the index in the text buffer of the
//          character the drawing should Start at
//-----------------------------------------------------------------------------
int RichText::GetStartDrawIndex( int& lineBreakIndexIndex )
{
	int value = _vertScrollBar->GetValue();
	int lineCount = m_LineBreaks.Count();
	if ( value >= lineCount )
		value = lineCount - 1;
	lineBreakIndexIndex = value;
	if ( value != 0 && value < m_LineBreaks.Count() )
		return m_LineBreaks[value - 1];
	return 0;
}

void RichText::GetText( int offset, wchar_t* buf, int bufLenInBytes )
{
	if ( !buf )
		return;
	int maxChars = bufLenInBytes / static_cast<int>( sizeof( wchar_t ) );
	int end = maxChars + offset - 1;
	int written = 0;
	if ( maxChars > 1 && offset < m_TextStream.Count() )
	{
		int index = offset;
		do
		{
			buf[index - offset] = m_TextStream[index];
			++index;
			if ( index == end )
				break;
		} while ( index < m_TextStream.Count() );
		written = index - offset;
	}
	buf[written] = 0;
	buf[maxChars - 1] = 0;
}

void RichText::GetText( int offset, char* pch, int bufLenInBytes )
{
	wchar_t unicode[4096];
	int count = 0;
	if ( offset < m_TextStream.Count() )
	{
		int index = offset;
		while ( index != m_TextStream.Count() && count < 4095 )
		{
			unicode[count] = m_TextStream[index];
			++count;
			++index;
		}
	}
	unicode[count] = 0;
	unicode[4095] = 0;
	localize()->ConvertUnicodeToANSI( unicode, pch, bufLenInBytes );
}

//-----------------------------------------------------------------------------
// Purpose: Set the font of the buffer text 
//-----------------------------------------------------------------------------
void RichText::SetFont( HFont font )
{
	_font = font;
	InvalidateLayout( false, false );
	m_bRecalcLineBreaks = true;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of lines in the window
//-----------------------------------------------------------------------------
int RichText::GetNumLines()
{
	return m_LineBreaks.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the height of the text entry window so all text will fit inside
//-----------------------------------------------------------------------------
void RichText::SetToFullHeight()
{
	PerformLayout();
	int wide;
	int tall;
	GetSize( wide, tall );
	int fontTall = surface()->GetFontTall( _font );
	tall = ( fontTall + _drawOffsetY ) * m_LineBreaks.Count() + _drawOffsetY;
	tall += GetProportionalScaledValue( 2 );
	SetSize( wide, tall );
	PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Select all the text.
//-----------------------------------------------------------------------------
void RichText::SelectAllText()
{
	_cursorPos = 0;
	_select[0] = 0;
	_select[1] = m_TextStream.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Select all the text.
//-----------------------------------------------------------------------------
void RichText::SelectNoText()
{
	_select[0] = 0;
	_select[1] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Invalidates the current linebreak stream
//-----------------------------------------------------------------------------
void RichText::InvalidateLineBreakStream()
{
	// clear the buffer
	m_LineBreaks.RemoveAll();
	m_LineBreaks.AddToTail( kLineBreakSentinel );
	_recalculateBreaksIndex = 0;
	m_bRecalcLineBreaks = true;
}

int RichText::ParseTextStringForUrls(
	const char* text, int startPos, char* resultBuffer, int resultBufferSize, bool& clickable )
{
	if ( text[startPos] == 0 )
	{
		clickable = false;
		resultBuffer[0] = 0;
		return startPos;
	}

	// scan for text that looks like a URL
	int position = startPos;
	while ( text[position] != 0 )
	{
		bool url = false;
		if ( _strnicmp( text + position, "www.", 4 ) == 0 )
		{
			const char* dot = strchr( text + position + 5, '.' );
			url = dot != NULL;
		}
		else if ( _strnicmp( text + position, "http://", 7 ) == 0 || _strnicmp( text + position, "ftp://", 6 ) == 0
				  || _strnicmp( text + position, "mailto:", 7 ) == 0 )
		{
			url = true;
		}
		else if ( m_bTrustInput && _strnicmp( text + position, "\\\\", 2 ) == 0 )
		{
			url = true;
		}

		if ( url )
		{
			if ( position != startPos )
				break;
			// we're at the Start of a URL, so parse that out
			clickable = true;
			int copied = 0;
			int cursor = startPos;
			char ch = text[startPos];
			while ( ch != 0 )
			{
				if ( iswspace( static_cast<wint_t>( static_cast<unsigned char>( ch ) ) ) )
					break;
				resultBuffer[copied] = ch;
				++copied;
				++cursor;
				ch = text[cursor];
			}
			resultBuffer[copied] = 0;
			return cursor;
		}
		++position;
	}

	clickable = false;
	int copied = 0;
	if ( startPos < position && resultBufferSize > 0 )
	{
		do
		{
			resultBuffer[copied] = text[startPos + copied];
			++copied;
		} while ( position > startPos + copied && resultBufferSize > copied );
	}
	resultBuffer[copied] = 0;
	return position;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RichText::SetText( const wchar_t* text )
{
	// reset the formatting stream
	m_FormatStream.RemoveAll();
	TFormatStream stream = {};
	stream.color = GetFgColor();
	stream.pixelsIndent = 0;
	stream.textClickable = false;
	stream.textStreamIndex = 0;
	m_FormatStream.AddToTail( stream );
	// set the new text stream
	m_TextStream.RemoveAll();
	if ( text )
	{
		int length = static_cast<int>( wcslen( text ) );
		m_TextStream.EnsureCapacity( length );
		for ( int i = 0; i < length; ++i )
			m_TextStream.AddToTail( text[i] );
	}
	_cursorPos = 0;
	_invalidateVerticalScrollbarSlider = true;
	_vertScrollBar->SetValue( 0 );
	Repaint();
	_select[0] = -1;
	Repaint();
	// blow away the line breaks list 
	InvalidateLineBreakStream();
	InvalidateLayout( false, false );
}

void RichText::SetText( const char* text )
{
	if ( !text )
		text = "";
	if ( text[0] == '#' )
	{
		const wchar_t* localized = localize()->Find( text );
		if ( localized )
		{
			SetText( localized );
			return;
		}
	}
	// convert to unicode
	wchar_t unicode[1024];
	localize()->ConvertANSIToUnicode( text, unicode, static_cast<int>( sizeof( unicode ) ) );
	SetText( unicode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RichText::ApplySettings( KeyValues* settings )
{
	Panel::ApplySettings( settings );
	_maxCharCount = settings->GetInt( "maxchars", -1 );
	// get the starting text, if any
	const char* text = settings->GetString( "text", "" );
	if ( text[0] == 0 )
		return;
	delete[] m_pszInitialText;
	size_t length = strlen( text );
	m_pszInitialText = new char[length + 1];
	strcpy( m_pszInitialText, text );
	SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void RichText::OnSetText( const wchar_t* text )
{
	SetText( text );
}

void RichText::InsertClickableTextStart( const char* pchClickAction )
{
	( void )pchClickAction;
	int last = m_FormatStream.Count() - 1;
	TFormatStream& current = m_FormatStream[last];
	if ( !current.textClickable )
	{
		if ( current.textStreamIndex == m_TextStream.Count() )
		{
			current.textClickable = true;
		}
		else
		{
			TFormatStream next = current;
			next.textClickable = true;
			next.textStreamIndex = m_TextStream.Count();
			m_FormatStream.AddToTail( next );
		}
	}
	// invalidate the layout to recalculate where the click panels should go
	InvalidateLineBreakStream();
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Inserts a text string while making URLs clickable/different color
// Input  : *text - string that may contain URLs to make clickable/color coded
//			URLTextColor - color for URL text
//          normalTextColor - color for normal text
//-----------------------------------------------------------------------------
void RichText::InsertPossibleURLString( const char* text, Color URLTextColor, Color normalTextColor )
{
	InsertColorChange( normalTextColor );
	size_t textLength = strlen( text );
	if ( textLength >= INT_MAX )
		return;

	int length = (int)textLength;
	CUtlMemory<char> buffer( 0, length + 1 );
	if ( !buffer.Base() )
		return;
	int position = 0;
	bool clickable = false;
	while ( length > 0 )
	{
		position = ParseTextStringForUrls( text, position, buffer.Base(), length, clickable );
		if ( clickable )
		{
			InsertClickableTextStart( NULL );
			InsertColorChange( URLTextColor );
		}
		InsertString( buffer.Base() );
		if ( clickable )
		{
			InsertColorChange( normalTextColor );
			InsertClickableTextEnd();
		}
		if ( length <= position )
			return;
	}
}

void RichText::CopySelected()
{
	if ( _select[0] != -1 )
	{
		int start = _select[0];
		int end = _select[1];
		if ( end < start )
		{
			int swap = start;
			start = end;
			end = swap;
		}

		CUtlVector<wchar_t> buf;
		int extra = 0;
		for ( int i = start; i < end; ++i )
		{
			if ( m_TextStream[i] == L'\n' )
			{
				buf.AddToTail( L'\r' );
				++extra;
			}
			buf.AddToTail( m_TextStream[i] );
		}
		buf.AddToTail( 0 );
		system()->SetClipboardText( buf.Base(), extra + end - start );
	}
	RequestFocus( 0 );
}

void RichText::RecalculateLineBreaks()
{
	m_bRecalcLineBreaks = false;
	_recalcSavedRenderState = true;
	if ( m_TextStream.Count() == 0 )
		return;

	HFont font = _font;
	int wide = GetWide();
	if ( _vertScrollBar->IsVisible() )
		wide -= _vertScrollBar->GetWide();

	int textIndex;
	if ( _recalculateBreaksIndex <= 0 )
	{
		m_LineBreaks.RemoveAll();
		textIndex = 0;
	}
	else
	{
		while ( m_LineBreaks.Count() > _recalculateBreaksIndex + 1 )
			m_LineBreaks.Remove( _recalculateBreaksIndex + 1 );
		textIndex = m_LineBreaks[_recalculateBreaksIndex];
	}

	if ( textIndex < m_TextStream.Count() )
	{
		wchar_t first = m_TextStream[textIndex];
		if ( first == L'\r' || first == L'\n' )
			++textIndex;
	}

	int clickableTextNum = 0;
	int panelCount = _clickableTextPanels.GetCount();
	if ( panelCount > 0 )
	{
		bool found = false;
		for ( int i = 0; i < panelCount; ++i )
		{
			if ( textIndex < _clickableTextPanels[i]->_textIndex )
			{
				clickableTextNum = i;
				found = true;
				break;
			}
		}
		if ( !found )
			clickableTextNum = 0;
	}

	int formatIndex = FindFormatStreamIndexForTextStreamPos( textIndex );
	int pixelsIndent = m_FormatStream[formatIndex].pixelsIndent;
	bool textClickable = m_FormatStream[formatIndex].textClickable;
	_currentTextClickable = false;
	if ( textIndex >= m_TextStream.Count() )
	{
		m_LineBreaks.AddToTail( kLineBreakSentinel );
		_invalidateVerticalScrollbarSlider = true;
		return;
	}

	int x = _drawOffsetX;
	bool wordStartedOnNewLine = true;
	bool wordStartedAtLineStart = true;
	bool hasWord = false;
	int wordStartIndex = 0;
	int index = textIndex;
	while ( index < m_TextStream.Count() )
	{
		wchar_t ch = m_TextStream[index];
		if ( formatIndex >= 0 && formatIndex < m_FormatStream.Count()
			 && m_FormatStream[formatIndex].textStreamIndex == index )
		{
			bool clickable = m_FormatStream[formatIndex].textClickable;
			int indent = m_FormatStream[formatIndex].pixelsIndent;
			textClickable = clickable;
			if ( indent != pixelsIndent )
				x = indent + _drawOffsetX;
			_pixelsIndent = indent;
			pixelsIndent = indent;
			++formatIndex;
			if ( _currentTextClickable != clickable )
			{
				if ( clickable )
				{
					ClickPanel* panel;
					if ( clickableTextNum < _clickableTextPanels.GetCount() )
						panel = _clickableTextPanels[clickableTextNum];
					else
					{
						panel = new ClickPanel( this );
						_clickableTextPanels.AddElement( panel );
						panel = _clickableTextPanels[clickableTextNum];
					}
					++clickableTextNum;
					panel->_textIndex = index;
				}
				_currentTextClickable = clickable;
			}
		}

		if ( iswspace( ch ) )
			hasWord = false;
		else if ( !hasWord )
		{
			hasWord = true;
			wordStartIndex = index;
			wordStartedAtLineStart = wordStartedOnNewLine;
		}

		int charWidth = surface()->GetCharacterWidth( font, ch );
		if ( iswprint( ch ) )
			wordStartedOnNewLine = false;

		if ( wide > charWidth + x && ch != L'\r' && ch != L'\n' )
		{
			x += charWidth;
			++index;
			continue;
		}

		int nextX = _drawOffsetX + _pixelsIndent;
		( void )surface()->GetFontTall( _font );
		if ( !( wide > charWidth + x ) && ch != L'\r' && ch != L'\n' && !wordStartedAtLineStart
			 && !iswspace( ch ) )
		{
			m_LineBreaks.AddToTail( wordStartIndex );
			wordStartedOnNewLine = true;
			hasWord = false;
			x = nextX + charWidth;
			index = wordStartIndex + 1;
			continue;
		}

		m_LineBreaks.AddToTail( index );
		if ( !( wide > charWidth + x ) && ch != L'\r' && ch != L'\n' && textClickable
			 && clickableTextNum > 0 && clickableTextNum <= _clickableTextPanels.GetCount() )
		{
			int previousSlot = clickableTextNum - 1;
			ClickPanel* previous = _clickableTextPanels[previousSlot];
			int previousText = previous->_textIndex;
			ClickPanel* panel;
			if ( clickableTextNum < _clickableTextPanels.GetCount() )
				panel = _clickableTextPanels[clickableTextNum];
			else
			{
				panel = new ClickPanel( this );
				_clickableTextPanels.AddElement( panel );
				panel = _clickableTextPanels[clickableTextNum];
			}
			++clickableTextNum;
			panel->_textIndex = previousText;
		}
		wordStartedOnNewLine = true;
		hasWord = false;
		x = nextX + charWidth;
		++index;
	}

	// end the list
	m_LineBreaks.AddToTail( kLineBreakSentinel );
	_invalidateVerticalScrollbarSlider = true;
}

//-----------------------------------------------------------------------------
// Purpose: Called pre render
//-----------------------------------------------------------------------------
void RichText::OnThink()
{
	if ( !m_bRecalcLineBreaks )
		return;
	_recalcSavedRenderState = true;
	RecalculateLineBreaks();
	if ( _invalidateVerticalScrollbarSlider )
		LayoutVerticalScrollBarSlider();
}
}
