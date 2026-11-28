#include "UTF8Text.h"

#include "VGUI_Font.h"

namespace vgui
{

namespace
{

UTF8TextHandler* utf8TextHandler = NULL;

}

void vgui_setUTF8TextHandler( UTF8TextHandler* handler )
{
	utf8TextHandler = handler;
}

int vgui_decodeUTF8Char( const char* text, int textLen, int* codePoint )
{
	if ( text == NULL || textLen <= 0 )
		return 0;

	const unsigned int lead = (unsigned char)text[0];
	int length;
	unsigned int value;
	unsigned int minimum;
	if ( lead >= 0xC2 && lead <= 0xDF )
	{
		length = 2;
		value = lead & 0x1F;
		minimum = 0x80;
	}
	else if ( lead >= 0xE0 && lead <= 0xEF )
	{
		length = 3;
		value = lead & 0x0F;
		minimum = 0x800;
	}
	else if ( lead >= 0xF0 && lead <= 0xF4 )
	{
		length = 4;
		value = lead & 0x07;
		minimum = 0x10000;
	}
	else
	{
		// ASCII, a continuation byte, or a lead byte that only starts overlong or out of range sequences.
		return 0;
	}

	if ( length > textLen )
		return 0;

	for ( int i = 1; i < length; ++i )
	{
		const unsigned int continuation = (unsigned char)text[i];
		if ( ( continuation & 0xC0 ) != 0x80 )
			return 0;

		value = ( value << 6 ) | ( continuation & 0x3F );
	}

	const bool surrogate = value >= 0xD800 && value <= 0xDFFF;
	if ( value < minimum || value > 0x10FFFF || surrogate )
		return 0;

	*codePoint = (int)value;
	return length;
}

const char* vgui_getFontName( Font* font )
{
	return font->_name;
}

int UTF8Text_getCharABCwide( Font* font, const char* text, int& a, int& b, int& c )
{
#if VGUI1_USE_VGUI2FONT_FOR_UTF8TEXT
	if ( utf8TextHandler != NULL )
	{
		int codePoint;
		const int length = vgui_decodeUTF8Char( text, VGUI_UTF8_MAX_CHAR_BYTES, &codePoint );
		if ( length > 0 && utf8TextHandler->getCharABCwide( font, codePoint, a, b, c ) )
			return length;
	}
#else
	(void)font;
	(void)text;
	(void)a;
	(void)b;
	(void)c;
#endif

	return 0;
}

}
