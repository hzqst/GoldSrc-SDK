//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Implementation of vgui::TextImage control
//
// $NoKeywords: $
//=============================================================================

#include "TextImage.h"
#include "../../engine/controls.h"
#include "controls.h"
#include "vgui/ISurface.h"
#include "../../engine/common.h"

#include <cstring>
#include <cwchar>
#include <cwctype>

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Changes the current font
//-----------------------------------------------------------------------------
void TextImage::SetFont( HFont font )
{
	m_bRecalculateTruncation = true;
	_font = font;
}

HFont TextImage::GetFont()
{
	return m_bRenderUsingFallbackFont ? _fallbackFont : _font;
}

//-----------------------------------------------------------------------------
// Purpose: unlocalized text symbol
//-----------------------------------------------------------------------------
StringIndex_t TextImage::GetUnlocalizedTextSymbol()
{
	return _unlocalizedTextSymbol;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the width that the text can be.
//-----------------------------------------------------------------------------
void TextImage::SetDrawWidth( int width )
{
	m_bRecalculateTruncation = true;
	_drawWidth = width;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the size of the TextImage. This is directly tied to drawWidth
//-----------------------------------------------------------------------------
void TextImage::SetSize( int width, int height )
{
	Image::SetSize( width, height );
	m_bRecalculateTruncation = true;
	_drawWidth = width;
}

//-----------------------------------------------------------------------------
// Purpose: Get the size of the text in the image
//-----------------------------------------------------------------------------
void TextImage::GetContentSize( int& width, int& height )
{
	GetTextSize( width, height );
}

//-----------------------------------------------------------------------------
// Purpose: Resize the text image to the content size
//-----------------------------------------------------------------------------
void TextImage::ResizeImageToContent()
{
	int wide, tall;
	GetContentSize( wide, tall );
	SetSize( wide, tall );
}

void TextImage::GetText( char* buffer, int bufferSize )
{
	localize()->ConvertUnicodeToANSI( _utext, buffer, bufferSize );
}

void TextImage::GetText( wchar_t* buffer, int bufferLength )
{
	wcsncpy( buffer, _utext, bufferLength );
}

void TextImage::SetWrap( bool state )
{
	m_bWrap = state;
}

void TextImage::SetUseFallbackFont( bool state, HFont font )
{
	m_bUseFallbackFont = state;
	_fallbackFont = font;
}

TextImage::TextImage( const char* text )
{
	m_bWrap = false;
	_utext = NULL;
	_font = 0;
	_fallbackFont = 0;
	_unlocalizedTextSymbol = INVALID_STRING_INDEX;
	_drawWidth = 0;
	_textBufferLen = 0;
	_textLen = 0;
	m_LineBreaks.RemoveAll();
	m_bUseFallbackFont = false;
	m_bRenderUsingFallbackFont = false;
	m_pwszEllipsesPosition = NULL;
	TextImage::SetText( text );
}

TextImage::TextImage( const wchar_t* text )
{
	m_bWrap = false;
	_utext = NULL;
	_font = 0;
	_fallbackFont = 0;
	_unlocalizedTextSymbol = INVALID_STRING_INDEX;
	_drawWidth = 0;
	_textBufferLen = 0;
	_textLen = 0;
	m_LineBreaks.RemoveAll();
	m_bUseFallbackFont = false;
	m_bRenderUsingFallbackFont = false;
	TextImage::SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
TextImage::~TextImage()
{
	if ( _utext )
		delete[] _utext;
}

void TextImage::SetText( const char* text )
{
	if ( !text )
		text = "";

	if ( text[0] == '#' )
	{
		_unlocalizedTextSymbol = localize()->FindIndex( text + 1 );
		if ( _unlocalizedTextSymbol != INVALID_STRING_INDEX )
		{
			SetText( localize()->GetValueByIndex( _unlocalizedTextSymbol ) );
			return;
		}
	}

	// convert the ansi string to unicode and use that
	wchar_t unicode[1024];
	localize()->ConvertANSIToUnicode( text, unicode, sizeof( unicode ) );
	SetText( unicode );
}

void TextImage::SetText( const wchar_t* text )
{
	if ( !text )
		text = L"";

	// reallocate the buffer if necessary
	_textLen = (short)wcslen( text );
	if ( _textLen >= _textBufferLen )
	{
		if ( _utext )
			delete[] _utext;

		_textBufferLen = (short)( _textLen + 1 );
		_utext = new wchar_t[_textBufferLen];
	}

	m_LineBreaks.RemoveAll();
	// store the text as unicode
	wcscpy( _utext, text );

	m_bRecalculateTruncation = true;
	if ( m_bWrap )
		RecalculateNewLinePositions();
}

void TextImage::GetTextSize( int& width, int& height )
{
	int maxWidth = 0;
	width = 0;
	height = 0;

	HFont font = _font;
	wchar_t* text = _utext;
	if ( !font )
		return;

	if ( m_bWrap )
		RecalculateNewLinePositions();

	// For height, use the remapped font
	int fontTall = surface()->GetFontTall( GetFont() );
	height = fontTall;

	int textLen = (int)wcslen( text );
	for ( int i = 0; i < textLen; i++ )
	{
		wchar_t* ch = text + i;

		if ( *ch == L'&' && ch[1] != L'\0' )
		{
			i++;
			ch++;
		}

		int a, b, c;
		surface()->GetCharABCwide( font, *ch, a, b, c );
		width += b + a + c;

		if ( *ch == L'\n' )
		{
			height += fontTall;
			if ( width > maxWidth )
				maxWidth = width;
			width = 0; // new line, wide is reset...
		}

		if ( m_bWrap )
		{
			for ( int j = 0; j < m_LineBreaks.Count(); j++ )
			{
				if ( ch == m_LineBreaks.Element( j ) )
				{
					height += fontTall;
					if ( width > maxWidth )
						maxWidth = width;
					width = 0; // new line, wide is reset...
				}
			}
		}
	}

	if ( width < maxWidth )
		width = maxWidth;
}

void TextImage::RecalculateNewLinePositions()
{
	HFont font = GetFont();
	//int wordStartIndex = 0;
	wchar_t* wordStart = _utext;
	int lineWidth = 0;
	bool hasWord = false;
	bool justStartedNewLine = true;
	bool wordStartedOnNewLine = true;

	m_LineBreaks.RemoveAll();

	wchar_t* wsz = _utext;
	if ( *wsz == L'\r' || *wsz == L'\n' )
		wsz++;

	for ( ; *wsz; wsz++ )
	{
		wchar_t ch = *wsz;

		if ( iswspace( ch ) )
		{
			hasWord = false;
		}
		else if ( !hasWord )
		{
			hasWord = true;
			wordStart = wsz;
			wordStartedOnNewLine = justStartedNewLine;
		}

		int len = surface()->GetCharacterWidth( font, ch );
		if ( iswprint( ch ) )
			justStartedNewLine = false;
		lineWidth += len;

		if ( lineWidth >= _drawWidth || ch == L'\r' || ch == L'\n' )
		{
			justStartedNewLine = true;
			hasWord = false;

			if ( ch != L'\r' && ch != L'\n' )
			{
				if ( wordStartedOnNewLine )
				{
					// word is longer than a line, so set the break at the current cursor
					m_LineBreaks.AddToTail( wsz );
				}
				else
				{
					m_LineBreaks.AddToTail( wordStart );
					wsz = wordStart;
				}
			}

			lineWidth = 0;
		}
	}
}

void TextImage::RecalculateEllipsesPosition()
{
	m_bRecalculateTruncation = false;
	m_pwszEllipsesPosition = NULL;

	if ( m_bWrap || wcschr( _utext, L'\n' ) )
		return;

	for ( int check = 0; check < ( m_bUseFallbackFont ? 2 : 1 ); check++ )
	{
		HFont font = GetFont();
		if ( check == 1 && _fallbackFont )
		{
			m_bRenderUsingFallbackFont = true;
			font = _fallbackFont;
			m_pwszEllipsesPosition = NULL;
		}

		int ellipsesWidth = surface()->GetCharacterWidth( font, L'.' ) * 3;
		int x = 0;

		for ( wchar_t* wsz = _utext; *wsz; wsz++ )
		{
			wchar_t ch = *wsz;
			if ( ch == L'\r' )
				continue;

			if ( ch == L'&' )
			{
				if ( wsz[1] != L'&' )
					continue;
				wsz++;
			}

			// don't truncate the first character
			if ( wsz == _utext )
				continue;

			int len = surface()->GetCharacterWidth( font, ch );
			if ( x + len + ellipsesWidth > _drawWidth )
			{
				// potential have an ellipses, see if the remaining characters will fit
				int remainingLength = len;
				for ( wchar_t* rwsz = wsz + 1; *rwsz; rwsz++ )
				{
					if ( *rwsz == L'\r' )
						continue;

					if ( *rwsz == L'&' )
					{
						if ( rwsz[1] != L'&' )
							continue;
						rwsz++;
					}

					remainingLength += surface()->GetCharacterWidth( font, *rwsz );
				}

				if ( x + remainingLength > _drawWidth )
				{
					// looks like we've got an ellipses situation
					m_pwszEllipsesPosition = wsz;
					break;
				}
			}

			x += len;
		}

		// Didn't need ellipses...
		if ( !m_pwszEllipsesPosition )
			break;
	}
}

void TextImage::Paint()
{
	int wide, tall;
	GetSize( wide, tall );

	if ( !_utext || !GetFont() )
		return;

	if ( m_bRecalculateTruncation )
		RecalculateEllipsesPosition();

	DrawSetTextColor( GetColor() );
	HFont font = GetFont();
	DrawSetTextFont( font );

	int lineHeight = surface()->GetFontTall( font );
	int x = 0;
	int y = 0;

	int px, py;
	GetPos( px, py );

	int currentLineBreak = 0;
	for ( wchar_t* wsz = _utext; *wsz; wsz++ )
	{
		wchar_t ch = *wsz;
		if ( ch == L'\r' )
			continue;

		if ( ch == L'\n' )
		{
			y += lineHeight;
			x = 0;
			continue;
		}

		if ( ch == L'&' )
		{
			if ( wsz[1] != L'&' )
				continue;
			wsz++;
		}

		// see if we've hit the truncated portion of the string
		if ( wsz == m_pwszEllipsesPosition )
		{
			// string is truncated, draw ellipses
			for ( int i = 0; i < 3; i++ )
			{
				surface()->DrawSetTextPos( x + px, y + py );
				surface()->DrawUnicodeChar( L'.' );
				x += surface()->GetCharacterWidth( font, L'.' );
			}
			return;
		}

		if ( currentLineBreak != m_LineBreaks.Count() && wsz == m_LineBreaks[currentLineBreak] )
		{
			y += lineHeight;
			currentLineBreak++;
			x = 0;
		}

		surface()->DrawSetTextPos( x + px, y + py );
		if ( surface()->IsAdditive( font ) )
			surface()->DrawUnicodeCharAdd( ch );
		else
			surface()->DrawUnicodeChar( ch );
		x += surface()->GetCharacterWidth( font, ch );
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void TextImage::GetUnlocalizedText( char* buffer, int bufferSize )
{
	if ( _unlocalizedTextSymbol == INVALID_STRING_INDEX )
	{
		GetText( buffer, bufferSize );
		return;
	}

	const char* name = localize()->GetNameByIndex( _unlocalizedTextSymbol );
	buffer[0] = '#';
	strncpy( buffer + 1, name, bufferSize - 1 );
	buffer[bufferSize - 1] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: Gets the width that the text can be.
//-----------------------------------------------------------------------------
void TextImage::GetDrawWidth( int& width )
{
	width = _drawWidth;
}

void TextImage::DrawTruncatedText( HFont font, wchar_t* text, int drawWidth )
{
	int ellipsesWidth = 3 * surface()->GetCharacterWidth( font, L'.' );
	if ( !*text )
	{
		surface()->DrawPrintText( text, 0 );
		return;
	}

	wchar_t* wsz = text;
	int width = 0;
	wchar_t* ellipsesPos = NULL;
	while ( 1 )
	{
		width += surface()->GetCharacterWidth( font, *wsz );

		if ( !ellipsesPos && width + ellipsesWidth >= drawWidth )
			ellipsesPos = wsz;
		if ( width >= drawWidth )
			break;

		wsz++;
		if ( !*wsz )
		{
			surface()->DrawPrintText( text, (int)( wsz - text ) );
			return;
		}
	}

	if ( !ellipsesPos )
	{
		surface()->DrawPrintText( text, (int)( wsz - text ) );
		return;
	}

	int len = (int)( ellipsesPos - text );
	if ( len <= 0 )
		len = 1;
	surface()->DrawPrintText( text, len );
	surface()->DrawPrintText( L"...", 3 );
}

void TextImage::ResizeImageToContentMaxWidth( int nMaxWidth )
{
	int wide, tall;

	_drawWidth = nMaxWidth;
	if ( m_bRecalculateTruncation )
	{
		m_bRenderUsingFallbackFont = false;
		RecalculateEllipsesPosition();
	}

	GetContentSize( wide, tall );
	SetSize( wide, tall );
}
}
