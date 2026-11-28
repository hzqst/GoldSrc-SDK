#include "VGUI_TextImage.h"

#include "VGUI_App.h"
#include "VGUI_Font.h"
#include "VGUI_Scheme.h"
#include "UTF8Text.h"

#include <new>
#include <string.h>

namespace vgui
{

namespace
{

char* copyText( const char* text, int textBufferLen )
{
	if ( text == NULL || textBufferLen <= 0 )
		return NULL;

	char* copy = new char[textBufferLen];
	if ( copy != NULL )
		memcpy( copy, text, textBufferLen );

	return copy;
}

// Returns the length in bytes of the character at text, a UTF-8 sequence the UTF8TextHandler measures or a byte
// that is a glyph of the font, and its width.
int getCharABCwide( Font* font, const char* text, int& a, int& b, int& c )
{
	int length = UTF8Text_getCharABCwide( font, text, a, b, c );
	if ( length == 0 )
	{
		font->getCharABCwide( (signed char)*text, a, b, c );
		length = 1;
	}
	return length;
}

}

TextImage::TextImage( int textBufferLen, const char* text )
	: Image()
{
	init( textBufferLen, text );
}

TextImage::TextImage( const char* text )
	: Image()
{
	init( (int)strlen( text ) + 1, text );
}

// The original vgui.dll exports a shallow copy constructor and operator=;
// copies own their text so that setText and the destructor never free a
// shared buffer.
TextImage::TextImage( const TextImage& other )
	: Image( other )
{
	_text = copyText( other._text, other._textBufferLen );
	_textBufferLen = _text != NULL ? other._textBufferLen : 0;
	_schemeFont = other._schemeFont;
	_font = other._font;
	memcpy( _textColor, other._textColor, sizeof( _textColor ) );
	_textSchemeColor = other._textSchemeColor;
}

#if VGUI_ABI_COMPAT_MODE != 1
TextImage::~TextImage()
{
	delete[] _text;
}
#endif

TextImage& TextImage::operator=( const TextImage& other )
{
	if ( this != &other )
	{
		Image::operator=( other );

		char* text = copyText( other._text, other._textBufferLen );
		delete[] _text;
		_text = text;
		_textBufferLen = _text != NULL ? other._textBufferLen : 0;
		_schemeFont = other._schemeFont;
		_font = other._font;
		memcpy( _textColor, other._textColor, sizeof( _textColor ) );
		_textSchemeColor = other._textSchemeColor;
	}

	return *this;
}

Font* TextImage::getFont()
{
	if ( _font != NULL )
		return _font;

	return App::getInstance()->getScheme()->getFont( _schemeFont );
}

void TextImage::getTextSize( int& wide, int& tall )
{
	wide = 0;
	tall = 0;
	if ( _text != NULL )
	{
		Font* font = getFont();
		if ( font != NULL )
			font->getTextSize( _text, wide, tall );
	}
}

void TextImage::getTextSizeWrapped( int& wide, int& tall )
{
	wide = 0;
	tall = 0;
	if ( _text == NULL )
		return;

	int availableWide;
	int ignoredTall;
	getSize( availableWide, ignoredTall );
	Font* font = getFont();
	int lineTall = font->getTall();
	int x = 0;
	int index = 0;
	signed char ch = (signed char)_text[0];
	int a;
	int b;
	int c;
	int charLength = getCharABCwide( font, _text, a, b, c );
	int wordWide = c + a + b;
	if ( ch == 0 )
		return;

	int y = lineTall;
	do
	{
		if ( ch != '\r' )
		{
			if ( ch == '\n' )
			{
				x = 0;
				y += lineTall;
			}
			else if ( ch == ' ' )
			{
				signed char next = (signed char)_text[index + 1];
				font->getCharABCwide( ' ', a, b, c );
				if ( next != 0 && next != '\n' && next != '\r' )
				{
					x += c + b + a;
					if ( x > availableWide )
					{
						x = 0;
						y += lineTall;
					}
				}
			}
			else
			{
				int length = charLength;
				for ( signed char next = (signed char)_text[index + length];
					next != 0 && next != '\n' && next != '\r' && next != ' ';
					next = (signed char)_text[index + length] )
				{
					int aa;
					int bb;
					int cc;
					length += getCharABCwide( font, _text + index + length, aa, bb, cc );
					wordWide += cc + bb + aa;
				}

				if ( x + wordWide > availableWide )
				{
					x = 0;
					y += lineTall;
				}

				for ( int j = 0; j < length; )
				{
					j += getCharABCwide( font, _text + index + j, a, b, c );
					int nextX = x + c + b + a;
					if ( nextX > wide )
						wide = nextX;
					if ( y > tall )
						tall = y;
					x = nextX;
				}
				index += length - 1;
			}
		}
		ch = (signed char)_text[++index];
		charLength = getCharABCwide( font, _text + index, a, b, c );
		wordWide = c + a + b;
	}
	while ( ch != 0 );
}

void TextImage::paint( Panel* )
{
	int availableWide;
	int ignoredTall;
	getSize( availableWide, ignoredTall );
	if ( _text == NULL )
		return;

	Color color;
	getColor( color );
	int red;
	int green;
	int blue;
	int alpha;
	color.getColor( red, green, blue, alpha );
	drawSetTextColor( red, green, blue, alpha );
	Font* font = getFont();
	drawSetTextFont( font );
	int lineTall = font->getTall();
	int x = 0;
	int y = 0;
	int index = 0;
	signed char ch = (signed char)_text[0];
	int a;
	int b;
	int c;
	int charLength = getCharABCwide( font, _text, a, b, c );
	int wordWide = c + a + b;
	if ( ch == 0 )
		return;

	do
	{
		if ( ch != '\r' )
		{
			if ( ch == '\n' )
			{
				x = 0;
				y += lineTall;
			}
			else if ( ch == ' ' )
			{
				signed char next = (signed char)_text[index + 1];
				font->getCharABCwide( ' ', a, b, c );
				if ( next != 0 && next != '\n' && next != '\r' )
				{
					x += c + b + a;
					if ( x > availableWide )
					{
						x = 0;
						y += lineTall;
					}
				}
			}
			else
			{
				int length = charLength;
				for ( signed char next = (signed char)_text[index + length];
					next != 0 && next != '\n' && next != '\r' && next != ' ';
					next = (signed char)_text[index + length] )
				{
					int aa;
					int bb;
					int cc;
					length += getCharABCwide( font, _text + index + length, aa, bb, cc );
					wordWide += cc + bb + aa;
				}

				if ( x + wordWide > availableWide )
				{
					x = 0;
					y += lineTall;
				}

				for ( int j = 0; j < length; )
				{
					// A UTF-8 sequence goes to the surface in one call, so it sees the whole character.
					const int sequenceLength = getCharABCwide( font, _text + index + j, a, b, c );
					if ( sequenceLength == 1 )
						drawPrintChar( x, y, _text[index + j] );
					else
						drawPrintText( x, y, _text + index + j, sequenceLength );
					x += c + b + a;
					j += sequenceLength;
				}
				index += length - 1;
			}
		}
		ch = (signed char)_text[++index];
		charLength = getCharABCwide( font, _text + index, a, b, c );
		wordWide = c + a + b;
	}
	while ( ch != 0 );
}

void TextImage::setFont( Scheme::SchemeFont schemeFont )
{
	_schemeFont = schemeFont;
}

void TextImage::setFont( Font* font )
{
	_font = font;
}

void TextImage::setSize( int wide, int tall )
{
	Image::setSize( wide, tall );
}

void TextImage::setText( int textBufferLen, const char* text )
{
	if ( textBufferLen > _textBufferLen )
	{
		// Copy before freeing: text may point into the current buffer.
		char* newText = new char[textBufferLen];
		if ( newText != NULL )
			vgui_strcpy( newText, textBufferLen, text );

		delete[] _text;
		_text = newText;
		_textBufferLen = _text != NULL ? textBufferLen : 0;
	}
	else if ( _text != NULL )
	{
		vgui_strcpy( _text, _textBufferLen, text );
	}

	int wide;
	int tall;
	getTextSize( wide, tall );
	Image::setSize( wide, tall );
}

void TextImage::setText( const char* text )
{
	if ( text == NULL )
		text = "";

	setText( (int)( strlen( text ) + 1 ), text );
}

void TextImage::init( int, const char* text )
{
	int wide;
	int tall;

	_schemeFont = Scheme::SchemeFont::sf_primary1;
	_text = NULL;
	_font = NULL;
	_textBufferLen = 0;
	setText( text );

	getTextSize( wide, tall );
	setSize( wide, tall );
}

}
