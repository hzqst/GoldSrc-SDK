#include "VGUI_Font.h"

#include "VGUI_Dar.h"
#include "fileimage.h"
#include "vgui_linux.h"

#include <stdlib.h>
#include <string.h>

namespace vgui
{

static int staticFontId = VGUI_FIRST_FONT_ID;
static Dar<BaseFontPlat*> staticFontPlatDar( 4 );

CUtlRBTree<FontPlat::font_name_entry, int> FontPlat::m_FriendlyNameCache;
bool FontPlat::ms_bSetFriendlyNameCacheLessFunc = false;

void Font_Reset()
{
	staticFontPlatDar.removeAll();
}

bool FontPlat::ExtendedABCWidthsCacheLessFunc( const abc_cache_t& lhs, const abc_cache_t& rhs )
{
	return lhs.wch < rhs.wch;
}

bool FontPlat::FontLessFunc( font_name_entry& lhs, font_name_entry& rhs )
{
	return strcasecmp( rhs.m_pchFriendlyName, lhs.m_pchFriendlyName ) > 0;
}

FontPlat::FontPlat( const char* name, int tall, int, float, int weight, bool, bool underline, bool, bool )
	: m_ExtendedABCWidthsCache( 256, 0, &FontPlat::ExtendedABCWidthsCacheLessFunc )
{
	bufSize[0] = bufSize[1] = 0;
	buf = NULL;
	m_iHeight = m_iMaxCharWidth = m_iAscent = 0;
	m_pGaussianDistribution = NULL;
	m_bBitmapFont = false;
	vgui_strcpy( m_szName, sizeof( m_szName ), name );
	m_iFlags = 0;
	m_bAntiAliased = false;
	m_iDropShadowOffset = 0;
	m_iTall = tall;
	m_bUnderlined = underline;
	m_bRotary = false;
	m_bAdditive = false;
	m_iWeight = weight;
	m_iOutlineSize = 0;
	m_iBlur = 0;
	m_iScanLines = 0;
}

FontPlat::~FontPlat()
{
}

void FontPlat::CreateFontList()
{
}

bool FontPlat::equals( const char* name, int tall, int, float, int weight, bool, bool underline, bool, bool )
{
	if ( strcasecmp( name, m_szName ) == 0 && m_iTall == tall && weight == m_iWeight )
	{
		return m_bUnderlined == underline;
	}
	return false;
}

void FontPlat::getCharRGBA( int, int, int, int, int, uchar* )
{
}

void FontPlat::getCharABCwide( int ch, int& a, int& b, int& c )
{
	a = b = c = 0;
	abc_cache_t finder;
	unsigned short i;

	finder.wch = ch;
	finder.abc.b = 0;
	finder.abc.a = 0;
	finder.abc.c = 0;
	i = m_ExtendedABCWidthsCache.Find( finder );
	if ( m_ExtendedABCWidthsCache.IsValidIndex( i ) )
	{
		a = m_ExtendedABCWidthsCache[i].abc.a;
		b = m_ExtendedABCWidthsCache[i].abc.b;
		c = m_ExtendedABCWidthsCache[i].abc.c;
	}
}

int FontPlat::getTall()
{
	return m_iHeight;
}

int FontPlat::getWide()
{
	return m_iMaxCharWidth;
}

void FontPlat::drawSetTextFont( SurfacePlat* )
{
}

FontPlat_Bitmap::FontPlat_Bitmap()
{
	m_pName = NULL;
}

FontPlat_Bitmap::~FontPlat_Bitmap()
{
	if ( m_pName != NULL )
	{
		delete[] m_pName;
	}
}

FontPlat_Bitmap* FontPlat_Bitmap::Create( const char* name, FileImageStream* pStream )
{
	FontPlat_Bitmap* pBitmap;
	char* pNameCopy;
	int nameLen;

	pBitmap = new FontPlat_Bitmap();
	if ( pBitmap == NULL )
	{
		return NULL;
	}

	if ( !LoadVFontDataFrom32BitTGA( pStream, &pBitmap->m_FontData ) )
	{
		delete pBitmap;
		return NULL;
	}

	nameLen = (int)strlen( name ) + 1;
	pNameCopy = new char[nameLen];
	pBitmap->m_pName = pNameCopy;
	if ( pNameCopy == NULL )
	{
		delete pBitmap;
		return NULL;
	}

	strcpy( pNameCopy, name );
	return pBitmap;
}

bool FontPlat_Bitmap::equals( const char*, int, int, float, int, bool, bool, bool, bool )
{
	return false;
}

void FontPlat_Bitmap::getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba )
{
	int chIndex;
	int outX;
	int outY;
	int destRow;
	unsigned char* pSrcPos;

	chIndex = VFont_NormalizeGlyphIndex( ch );

	if ( m_FontData.m_BitmapCharHeight <= 0 )
	{
		return;
	}

	outY = 0;
	destRow = VGUI_RGBA_BYTES * ( rgbaX + rgbaY * rgbaWide );
	do
	{
		pSrcPos = &m_FontData.m_pBitmap[outY * ( m_FontData.m_BitmapCharWidth << 8 )
			+ m_FontData.m_BitmapCharWidth * chIndex];
		if ( m_FontData.m_BitmapCharWidth > 0 )
		{
			outX = 0;
			do
			{
				if ( rgbaWide > rgbaX + outX && rgbaTall > outY + rgbaY )
				{
					uchar* pOutPos = &rgba[destRow + VGUI_RGBA_BYTES * outX];
					if ( pSrcPos[outX] == 0 )
					{
						pOutPos[3] = 0;
						pOutPos[2] = 0;
						pOutPos[1] = 0;
						pOutPos[0] = 0;
					}
					else
					{
						pOutPos[3] = 0xFF;
						pOutPos[2] = 0xFF;
						pOutPos[1] = 0xFF;
						pOutPos[0] = 0xFF;
					}
				}
				++outX;
			}
			while ( outX < m_FontData.m_BitmapCharWidth );
		}
		++outY;
		destRow += VGUI_RGBA_BYTES * rgbaWide;
	}
	while ( outY < m_FontData.m_BitmapCharHeight );
}

void FontPlat_Bitmap::getCharABCwide( int ch, int& a, int& b, int& c )
{
	ch = VFont_NormalizeGlyphIndex( ch );
	c = 0;
	a = 0;
	b = m_FontData.m_CharWidths[ch] + 1;
}

int FontPlat_Bitmap::getTall()
{
	return m_FontData.m_BitmapCharHeight;
}

int FontPlat_Bitmap::getWide()
{
	return m_FontData.m_BitmapCharWidth;
}

void FontPlat_Bitmap::drawSetTextFont( SurfacePlat* )
{
}

Font::Font( const char* name, int tall, int wide, float rotation, int weight,
	bool italic, bool underline, bool strikeout, bool symbol )
{
	init( name, NULL, 0, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
}

Font::Font( const char* name, void* pFileData, int fileDataLen, int tall, int wide, float rotation, int weight,
	bool italic, bool underline, bool strikeout, bool symbol )
{
	init( name, pFileData, fileDataLen, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
}

void Font::init( const char* name, void* pFileData, int fileDataLen, int tall, int wide, float rotation, int weight,
	bool italic, bool underline, bool strikeout, bool symbol )
{
	_id = -1;
	_name = strdup( name );

	if ( pFileData != NULL )
	{
		FileImageStream_Memory memStream( pFileData, fileDataLen );
		FontPlat_Bitmap* bitmap = FontPlat_Bitmap::Create( name, &memStream );
		if ( bitmap == NULL )
		{
			return;
		}

		_plat = bitmap;
		staticFontPlatDar.addElement( bitmap );
		_id = staticFontId;
		++staticFontId;
		return;
	}

	_plat = NULL;
	for ( int i = 0; i < staticFontPlatDar.getCount(); ++i )
	{
		BaseFontPlat* entry = staticFontPlatDar[i];
		if ( entry->equals( name, tall, wide, rotation, weight, italic, underline, strikeout, symbol ) )
		{
			_plat = staticFontPlatDar[i];
			return;
		}
	}

	_plat = new FontPlat( name, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
	staticFontPlatDar.addElement( _plat );
	_id = staticFontId;
	++staticFontId;
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

int Font::getWide()
{
	return _plat->getWide();
}

}
