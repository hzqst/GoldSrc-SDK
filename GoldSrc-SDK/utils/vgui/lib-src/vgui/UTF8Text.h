#ifndef UTF8TEXT_H
#define UTF8TEXT_H

#include "VGUI_UTF8Text.h"

namespace vgui
{

// Measures the character outside ASCII whose UTF-8 sequence starts at the NUL-terminated text with the handler of
// vgui_setUTF8TextHandler and returns the length of the sequence. Returns 0, leaving a, b and c unset, when the byte
// at text is a glyph of the font: an ASCII byte, a byte that does not start a valid sequence, a code point the
// handler does not measure, no handler, or VGUI1_USE_VGUI2FONT_FOR_UTF8TEXT 0.
int UTF8Text_getCharABCwide( Font* font, const char* text, int& a, int& b, int& c );

}

#endif
