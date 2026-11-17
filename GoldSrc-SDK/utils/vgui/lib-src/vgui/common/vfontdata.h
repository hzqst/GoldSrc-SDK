#ifndef VFONTDATA_H
#define VFONTDATA_H

#include <limits.h>

#define VFONT_GLYPH_COUNT	256

// Glyph lookups are single-byte: map sign-extended chars (-128..-1) back to 0x80..0xFF,
// then clamp anything else into the valid 0..VFONT_GLYPH_COUNT-1 range.
inline int VFont_NormalizeGlyphIndex( int ch )
{
	if ( ch < 0 && ch >= CHAR_MIN )
	{
		ch = (unsigned char)ch;
	}
	if ( ch < 0 )
	{
		return 0;
	}
	if ( ch >= VFONT_GLYPH_COUNT )
	{
		return VFONT_GLYPH_COUNT - 1;
	}
	return ch;
}

class FileImageStream;

class VFontData
{
public:
	VFontData();
	~VFontData();

	int				m_CharWidths[VFONT_GLYPH_COUNT];
	int				m_BitmapCharWidth;
	int				m_BitmapCharHeight;
	unsigned char*	m_pBitmap;
};

bool	LoadVFontDataFrom32BitTGA( FileImageStream* fp, VFontData* pData );

#endif
