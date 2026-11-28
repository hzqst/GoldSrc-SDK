#ifndef VGUI_UTF8TEXT_H
#define VGUI_UTF8TEXT_H

#include "VGUI.h"

// With VGUI1_USE_VGUI2FONT_FOR_UTF8TEXT 1 (the default), the text of TextImage and Font::getTextSize is UTF-8. A
// character outside ASCII takes the width the UTF8TextHandler gives for its code point, and TextImage passes its
// whole sequence to one drawPrintText call, so the surface can draw it with a font that has the glyph: the engine
// sets a handler that measures it with a VGUI2 font of the face and height of the Font. Without a handler, for a
// code point the handler does not measure and for bytes that are not valid UTF-8, every byte is a glyph of the Font,
// as in the original vgui.dll; 0 keeps that for all text.
#ifndef VGUI1_USE_VGUI2FONT_FOR_UTF8TEXT
#define VGUI1_USE_VGUI2FONT_FOR_UTF8TEXT 1
#endif

// The longest UTF-8 sequence of one code point.
#define VGUI_UTF8_MAX_CHAR_BYTES	4

namespace vgui
{

class Font;

class UTF8TextHandler
{
public:
	// Returns false, leaving a, b and c unset, when the bytes of the character are to be glyphs of the font.
	virtual bool getCharABCwide( Font* font, int codePoint, int& a, int& b, int& c ) = 0;
};

// The functions have C linkage so that their names in vgui.def do not depend on the decoration of the compiler.
extern "C"
{

// The handler is used until another one is set; NULL removes it.
VGUIAPI void vgui_setUTF8TextHandler( UTF8TextHandler* handler );

// Returns the length of the UTF-8 sequence of a character outside ASCII at text, read up to a NUL byte or textLen
// bytes, and sets codePoint to its code point. Returns 0, leaving codePoint unset, for an ASCII byte and for bytes
// that are not such a sequence (overlong, surrogate, above U+10FFFF, or cut short).
VGUIAPI int vgui_decodeUTF8Char( const char* text, int textLen, int* codePoint );

// The face name the font was created with.
VGUIAPI const char* vgui_getFontName( Font* font );

}

}

#endif
