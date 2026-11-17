#include "vfontdata.h"

#include "VGUI.h"
#include "fileimage.h"

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
VFontData::VFontData() : m_BitmapCharWidth( 0 ), m_BitmapCharHeight( 0 ), m_pBitmap( NULL )
{
}

VFontData::~VFontData()
{
	delete[] m_pBitmap;
}

bool LoadVFontDataFrom32BitTGA( FileImageStream* stream, VFontData* data )
{
	FileImage image;
	if ( !Load32BitTGA( stream, &image ) )
		return false;

	// An image narrower than one pixel per glyph would load as a font of empty glyphs.
	if ( image.m_Width < VFONT_GLYPH_COUNT || image.m_Height <= 0 )
		return false;

	data->m_pBitmap = new unsigned char[image.m_Width * image.m_Height];
	if ( data->m_pBitmap == NULL )
		return false;

	data->m_BitmapCharWidth = image.m_Width / VFONT_GLYPH_COUNT;
	data->m_BitmapCharHeight = image.m_Height;
	const int width = data->m_BitmapCharWidth;
	const int rowStride = VFONT_GLYPH_COUNT * width;
	for ( int ch = 0; ch < VFONT_GLYPH_COUNT; ++ch )
	{
		int rightmost = 0;
		for ( int y = 0; y < data->m_BitmapCharHeight; ++y )
		{
			for ( int x = 0; x < width; ++x )
			{
				const int offset = y * rowStride + ch * width + x;
				const unsigned char* pixel = image.m_pData + VGUI_RGBA_BYTES * offset;
				const bool set = pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0 || pixel[3] != 0;
				data->m_pBitmap[offset] = set ? 1 : 0;
				if ( set && x > rightmost )
					rightmost = x;
			}
		}
		data->m_CharWidths[ch] = ch == ' ' ? width / 4 : rightmost;
	}

	return true;
}
