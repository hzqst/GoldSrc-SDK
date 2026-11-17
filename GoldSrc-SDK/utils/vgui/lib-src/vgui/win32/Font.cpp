#include "VGUI_Font.h"

#include "VGUI_Dar.h"
#include "fileimage.h"
#include "vfontdata.h"
#include "vgui_win32.h"

#include <stdlib.h>
#include <string.h>

#define GLYPH_COUNT		256

namespace vgui
{
namespace
{

Dar<BaseFontPlat*> fontPlatforms;
// fontPlatformIds[i] is the font id assigned when fontPlatforms[i] was created.
Dar<int> fontPlatformIds;
int nextFontId = VGUI_FIRST_FONT_ID;

static_assert( GLYPH_COUNT == VFONT_GLYPH_COUNT, "System and bitmap fonts share one glyph range" );

}

class FontPlat : public BaseFontPlat
{
public:
	FontPlat( const char* name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol )
		: _name( _strdup( name ) ), _wide( wide ), _tall( tall ), _rotation( rotation ), _weight( weight ),
		_italic( italic ), _underline( underline ), _strikeout( strikeout ), _symbol( symbol )
	{
		const int tenthsOfDegrees = (int)( rotation * 10.0 );
		_font = CreateFontA( tall, wide, tenthsOfDegrees, tenthsOfDegrees, weight,
			italic, underline, strikeout, symbol ? SYMBOL_CHARSET : ANSI_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, name );
		_dc = CreateCompatibleDC( NULL );
		SelectObject( _dc, _font );
		SetTextAlign( _dc, TA_UPDATECP );
		GetTextMetricsA( _dc, &_metrics );
		_bufferWide = _metrics.tmMaxCharWidth;
		_bufferTall = _metrics.tmAscent + _metrics.tmDescent + _metrics.tmHeight;
		BITMAPINFO info = {};
		info.bmiHeader.biSize = sizeof( info.bmiHeader );
		info.bmiHeader.biWidth = _bufferWide;
		info.bmiHeader.biHeight = -_bufferTall;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = VGUI_RGBA_BYTES * 8;
		info.bmiHeader.biCompression = BI_RGB;
		void* pixels = NULL;
		_bitmap = CreateDIBSection( _dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0 );
		_pixels = (uchar*)pixels;
#if defined( _MSC_VER )
#pragma warning( suppress : 28183 )
#endif
		SelectObject( _dc, _bitmap );
		memset( _abc, 0, sizeof( _abc ) );
		if ( !GetCharABCWidthsA( _dc, 0, GLYPH_COUNT - 1, _abc ) )
		{
			for ( int i = 0; i < GLYPH_COUNT; ++i )
			{
				_abc[i].abcB = _metrics.tmAveCharWidth;
			}
		}
	}

	virtual ~FontPlat() override
	{
	}

	virtual bool equals( const char* name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol ) override
	{
		return _stricmp( name, _name ) == 0 && tall == _tall && wide == _wide
			&& rotation == _rotation && weight == _weight && italic == _italic
			&& underline == _underline && strikeout == _strikeout && symbol == _symbol;
	}

	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba ) override
	{
		ch = VFont_NormalizeGlyphIndex( ch );
		SelectObject( _dc, _font );
		SetBkColor( _dc, RGB( 0, 0, 0 ) );
		SetTextColor( _dc, RGB( 255, 255, 255 ) );
		SetBkMode( _dc, OPAQUE );
		MoveToEx( _dc, -_abc[ch].abcA, 0, NULL );
		const char character = (char)ch;
		ExtTextOutA( _dc, 0, 0, 0, NULL, &character, 1, NULL );
		SetBkMode( _dc, TRANSPARENT );
		const int width = (int)_abc[ch].abcB < _bufferWide ? (int)_abc[ch].abcB : _bufferWide;
		const int height = _metrics.tmHeight < _bufferTall ? _metrics.tmHeight : _bufferTall;
		const float byteToUnit = 1.0f / 255.0f;
		for ( int y = 0; y < height; ++y )
		{
			for ( int x = 0; x < width; ++x )
			{
				const int outX = rgbaX + x;
				const int outY = rgbaY + y;
				if ( outX >= rgbaWide || outY >= rgbaTall )
				{
					continue;
				}
				const uchar* pixel = _pixels + VGUI_RGBA_BYTES * ( x + y * _bufferWide );
				const float blue = ch == '\t' ? 0.0f : (float)( pixel[0] * (double)byteToUnit );
				const float green = ch == '\t' ? 0.0f : (float)( pixel[1] * (double)byteToUnit );
				const double red = ch == '\t' ? 0.0 : pixel[2] * (double)byteToUnit;
				uchar* output = rgba + VGUI_RGBA_BYTES * ( outX + outY * rgbaWide );
				output[0] = (uchar)(int)( blue * 255.0 );
				output[1] = (uchar)(int)( green * 255.0 );
				output[2] = (uchar)(int)( red * 255.0 );
				const double coverage = red * 0.11f + (double)green * 0.55f + (double)blue * 0.34f;
				output[3] = (uchar)(int)( coverage * 255.0 );
			}
		}
	}

	virtual void getCharABCwide( int ch, int& a, int& b, int& c ) override
	{
		ch = VFont_NormalizeGlyphIndex( ch );
		a = _abc[ch].abcA < 0 ? 0 : _abc[ch].abcA;
		b = (int)_abc[ch].abcB;
		c = _abc[ch].abcC;
	}

	virtual int getTall() override { return _metrics.tmHeight; }

	virtual void drawSetTextFont( SurfacePlat* surface ) override
	{
		SelectObject( surface->drawDC, _font );
	}

private:
	HFONT		_font;
	HDC			_dc;
	HBITMAP		_bitmap;
	TEXTMETRICA	_metrics;
	ABC			_abc[GLYPH_COUNT];
	int			_bufferWide;
	int			_bufferTall;
	uchar*		_pixels;

	VFontData	_unusedFontData;
	int			_unused;

	char*		_name;
	int			_wide;
	int			_tall;
	float		_rotation;
	int			_weight;
	bool		_italic;
	bool		_underline;
	bool		_strikeout;
	bool		_symbol;
};

class FontPlat_Bitmap : public BaseFontPlat
{
public:
	FontPlat_Bitmap() : _name( NULL )
	{
	}

	virtual ~FontPlat_Bitmap() override
	{
		delete[] _name;
	}

	static FontPlat_Bitmap* Create( const char* name, FileImageStream* stream )
	{
		FontPlat_Bitmap* font = new FontPlat_Bitmap();
		if ( !LoadVFontDataFrom32BitTGA( stream, &font->_data ) )
		{
			delete font;
			return NULL;
		}
		font->_name = vgui_strdup( name );
		return font;
	}

	virtual bool equals( const char*, int, int, float, int, bool, bool, bool, bool ) override
	{
		return false;
	}

	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba ) override
	{
		ch = VFont_NormalizeGlyphIndex( ch );
		for ( int y = 0; y < _data.m_BitmapCharHeight; ++y )
		{
			for ( int x = 0; x < _data.m_BitmapCharWidth; ++x )
			{
				const int outX = rgbaX + x;
				const int outY = rgbaY + y;
				if ( outX >= rgbaWide || outY >= rgbaTall )
				{
					continue;
				}
				const int offset = ( y * VFONT_GLYPH_COUNT + ch ) * _data.m_BitmapCharWidth + x;
				const uchar color = _data.m_pBitmap[offset] == 0 ? 0 : 255;
				uchar* output = rgba + VGUI_RGBA_BYTES * ( outX + outY * rgbaWide );
				output[0] = output[1] = output[2] = output[3] = color;
			}
		}
	}

	virtual void getCharABCwide( int ch, int& a, int& b, int& c ) override
	{
		ch = VFont_NormalizeGlyphIndex( ch );
		a = c = 0;
		b = _data.m_CharWidths[ch] + 1;
	}

	virtual int getTall() override { return _data.m_BitmapCharHeight; }
	virtual void drawSetTextFont( SurfacePlat* ) override
	{
	}

private:
	VFontData	_data;
	char*		_name;
};

void Font_Reset()
{
	fontPlatforms.setCount( 0 );
	fontPlatformIds.setCount( 0 );
}

Font::Font( const char* name, int tall, int wide, float rotation, int weight,
	bool italic, bool underline, bool strikeout, bool symbol )
{
	init( name, NULL, 0, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
}

Font::Font( const char* name, void* data, int length, int tall, int wide, float rotation, int weight,
	bool italic, bool underline, bool strikeout, bool symbol )
{
	init( name, data, length, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
}

void Font::init( const char* name, void* data, int length, int tall, int wide, float rotation, int weight,
	bool italic, bool underline, bool strikeout, bool symbol )
{
	_name = _strdup( name );
	_id = -1;
	_plat = NULL;
	if ( data != NULL )
	{
		FileImageStream_Memory stream( data, length );
		_plat = FontPlat_Bitmap::Create( name, &stream );
	}
	// Invalid bitmap data falls back to the system font, as VGUI_Font.h documents.
	if ( _plat == NULL )
	{
		for ( int i = 0; i < fontPlatforms.getCount(); ++i )
		{
			if ( fontPlatforms[i]->equals( name, tall, wide, rotation, weight, italic, underline, strikeout, symbol ) )
			{
				// Fonts sharing a platform share its id, so the engine caches its glyphs once.
				_plat = fontPlatforms[i];
				_id = fontPlatformIds[i];
				return;
			}
		}
		_plat = new FontPlat( name, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
	}
	_id = nextFontId++;
	fontPlatforms.addElement( _plat );
	fontPlatformIds.addElement( _id );
}

void Font::getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba )
{
	_plat->getCharRGBA( ch, rgbaX, rgbaY, rgbaWide, rgbaTall, rgba );
}

void Font::getCharABCwide( int ch, int& a, int& b, int& c )
{
	_plat->getCharABCwide( ch, a, b, c );
}

int Font::getTall()
{
	return _plat->getTall();
}

}
