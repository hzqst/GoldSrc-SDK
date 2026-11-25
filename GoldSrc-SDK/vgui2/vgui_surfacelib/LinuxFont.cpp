#include "LinuxFont.h"

#include "FontEffects.h"
#include "FontManager.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fontconfig.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <strings.h>

namespace
{
constexpr int FONTFLAG_ITALIC = 0x001;
constexpr int FONTFLAG_UNDERLINE = 0x002;
constexpr int FONTFLAG_ANTIALIAS = 0x010;
constexpr int FONTFLAG_ROTARY = 0x040;
constexpr int FONTFLAG_DROPSHADOW = 0x080;
constexpr int FONTFLAG_ADDITIVE = 0x100;

struct font_scale_t
{
	const char* font;
	float scale;
};

constexpr font_scale_t g_FontScales[] = {
	{ "DejaVu Sans", 0.875f },
	{ "Liberation Sans", 0.9375f },
	{ "Fira Sans", 0.875f },
};
}

CUtlRBTree<CLinuxFont::font_name_entry> CLinuxFont::m_FriendlyNameCache( 0, 0, &CLinuxFont::FontEntryLess );
bool CLinuxFont::ms_bSetFriendlyNameCacheLessFunc = false;

static int Fixed26Dot6ToInt( int value )
{
	int rounded = value >> 6;
	if ( value & 0x20 )
		rounded += ( value >> 31 ) | 1;
	return rounded;
}

static int RoundHalfBBox( int xMax )
{
	int adjusted = xMax + (int)( (unsigned int)xMax >> 31 );
	int rounded = adjusted >> 7;
	if ( ( adjusted >> 1 ) & 0x20 )
		rounded += ( adjusted >> 31 ) | 1;
	return rounded;
}

static char LowByteDifference( int rounded, unsigned char first, unsigned char second )
{
	return (char)( (char)rounded - first - second );
}

static char LowByteDifference( int rounded, unsigned char first, unsigned char second, unsigned char third )
{
	return (char)( (char)rounded - first - second - third );
}

static FcPattern* FontMatch( const char* object, FcType type, const void* value, ... )
{
	va_list arguments;
	FcPattern* pattern;
	FcPattern* match;
	FcValue fcValue;
	FcResult result;

	va_start( arguments, value );
	pattern = FcPatternCreate();
	for ( ;; )
	{
		fcValue.type = type;
		fcValue.u.s = nullptr;
		if ( type == FcTypeInteger )
			fcValue.u.i = (int)(intptr_t)value;
		else if ( type == FcTypeString )
			fcValue.u.s = (const FcChar8*)value;
		FcPatternAdd( pattern, object, fcValue, 0 );

		object = va_arg( arguments, const char* );
		if ( !object )
			break;
		type = (FcType)va_arg( arguments, int );
		value = va_arg( arguments, const void* );
	}
	va_end( arguments );

	FcConfigSubstitute( nullptr, pattern, FcMatchPattern );
	FcDefaultSubstitute( pattern );
	result = FcResultMatch;
	match = FcFontMatch( nullptr, pattern, &result );
	FcPatternDestroy( pattern );
	return match;
}

bool CLinuxFont::ExtendedABCWidthsCacheLessFunc( const abc_cache_t& lhs, const abc_cache_t& rhs )
{
	return lhs.wch < rhs.wch;
}

bool CLinuxFont::FontEntryLess( const font_name_entry& a, const font_name_entry& b )
{
	return strcasecmp( b.m_pchFriendlyName, a.m_pchFriendlyName ) > 0;
}

CLinuxFont::CLinuxFont() : m_ExtendedABCWidthsCache( 256, 0, &CLinuxFont::ExtendedABCWidthsCacheLessFunc )
{
	m_szName[0] = 0;
	m_iTall = 0;
	m_iWeight = 0;
	m_iFlags = 0;
	m_bAntiAliased = false;
	m_bRotary = false;
	m_bAdditive = false;
	m_iDropShadowOffset = 0;
	m_bUnderlined = false;
	m_iOutlineSize = 0;
	m_iHeight = 0;
	m_iMaxCharWidth = 0;
	m_iAscent = 0;
	m_iScanLines = 0;
	m_iBlur = 0;
	m_pGaussianDistribution = nullptr;
	face = nullptr;
	m_iFxpHeight = 0;
	ms_bSetFriendlyNameCacheLessFunc = true;
}

CLinuxFont::~CLinuxFont()
{
	if ( face )
		FT_Done_Face( face );
	delete[] m_pGaussianDistribution;
}

bool CLinuxFont::FindFontDataFile(
	const char* windowsFontName, int, int weight, int, int, int flags, char* dataFile, int dataFileChars )
{
	const char* family = windowsFontName;
	FcChar8* filename;
	FcPattern* pattern;
	bool found = false;

	if ( FcInit() )
	{
		int fontWeight = FC_WEIGHT_REGULAR;
		if ( weight > 699 )
			fontWeight = FC_WEIGHT_BOLD;

		if ( !strcasecmp( windowsFontName, "Tahoma" ) )
			family = "Bitstream Vera Sans";

		int slant = FC_SLANT_ROMAN;
		if ( flags & FONTFLAG_ITALIC )
			slant = FC_SLANT_ITALIC;

		pattern = FontMatch( "family", FcTypeString, family, "weight", FcTypeInteger, (const void*)(intptr_t)fontWeight,
			"slant", FcTypeInteger, (const void*)(intptr_t)slant, nullptr );
		if ( pattern )
		{
			filename = nullptr;
			if ( FcPatternGetString( pattern, "file", 0, &filename ) == FcResultMatch )
			{
				found = true;
				if ( dataFileChars > 0 )
				{
					strncpy( dataFile, (const char*)filename, static_cast<size_t>( dataFileChars - 1 ) );
					dataFile[dataFileChars - 1] = 0;
				}
			}
			FcPatternDestroy( pattern );
		}
	}

	return found;
}

bool CLinuxFont::Create( const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags )
{
	float scale = 1.0f;

	for ( const font_scale_t& entry : g_FontScales )
	{
		if ( !strcasecmp( entry.font, windowsFontName ) )
			scale = entry.scale;
	}

	strncpy( m_szName, windowsFontName, sizeof( m_szName ) - 1 );
	m_szName[sizeof( m_szName ) - 1] = 0;
	m_iFlags = flags;
	m_iOutlineSize = 0;
	m_iTall = tall;
	m_iWeight = weight;
	m_bAntiAliased = ( flags & FONTFLAG_ANTIALIAS ) != 0;
	m_bUnderlined = ( flags & FONTFLAG_UNDERLINE ) != 0;
	m_iDropShadowOffset = ( flags & FONTFLAG_DROPSHADOW ) != 0;
	m_iFxpHeight = 0;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bRotary = ( flags & FONTFLAG_ROTARY ) != 0;
	m_iHeight = (int)( (float)tall * scale );
	m_bAdditive = ( flags & FONTFLAG_ADDITIVE ) != 0;

	if ( !FcInit() )
	{
		printf( "Unable to find font named %s\n", windowsFontName );
		m_szName[0] = 0;
		return false;
	}

	int fontWeight = FC_WEIGHT_REGULAR;
	if ( weight >= 700 )
		fontWeight = FC_WEIGHT_BOLD;

	const char* family = windowsFontName;
	if ( !strcasecmp( windowsFontName, "Tahoma" ) )
		family = "Bitstream Vera Sans";

	int slant = FC_SLANT_ROMAN;
	if ( flags & FONTFLAG_ITALIC )
		slant = FC_SLANT_ITALIC;

	FcPattern* pattern = FontMatch( "family", FcTypeString, family, "weight", FcTypeInteger,
		(const void*)(intptr_t)fontWeight, "slant", FcTypeInteger, (const void*)(intptr_t)slant, nullptr );
	if ( !pattern )
	{
		printf( "Unable to find font named %s\n", windowsFontName );
		m_szName[0] = 0;
		return false;
	}

	FcChar8* filename = nullptr;
	if ( FcPatternGetString( pattern, "file", 0, &filename ) != FcResultMatch )
	{
		FcPatternDestroy( pattern );
		printf( "Unable to find font named %s\n", windowsFontName );
		m_szName[0] = 0;
		return false;
	}

	char fontFile[4096];
	strncpy( fontFile, (const char*)filename, sizeof( fontFile ) - 1 );
	fontFile[sizeof( fontFile ) - 1] = 0;
	FcPatternDestroy( pattern );

	if ( FT_New_Face( FontManager().m_Library, fontFile, 0, &face ) )
		return false;

	FT_Set_Pixel_Sizes( face, 0, (FT_UInt)( m_iHeight - 1 ) );

	const FT_Size_Metrics& metrics = face->size->metrics;
	m_iAscent = Fixed26Dot6ToInt( (int)metrics.ascender );
	m_iHeight = 2 * m_iOutlineSize + m_iDropShadowOffset + Fixed26Dot6ToInt( (int)metrics.height );
	m_iMaxCharWidth = Fixed26Dot6ToInt( (int)metrics.max_advance );
	m_iFxpHeight = (int)metrics.height + ( ( m_iDropShadowOffset + 2 * m_iOutlineSize ) << 6 );

	delete[] m_pGaussianDistribution;
	m_pGaussianDistribution = CreateGaussianDistribution( m_iBlur );
	return true;
}

void CLinuxFont::GetCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba )
{
	FT_Error error = FT_Load_Glyph( face, FT_Get_Char_Index( face, (FT_ULong)ch ), FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT );
	if ( error )
	{
		fprintf( stderr, "Error in FL_Load_Glyph: %x\n", (unsigned int)error );
		return;
	}

	FT_GlyphSlot slot = face->glyph;
	int rows = (int)slot->bitmap.rows;
	int width = (int)slot->bitmap.width;
	unsigned char* buffer = slot->bitmap.buffer;
	int top = m_iAscent - slot->bitmap_top;
	int srcY = 0;
	if ( top < 0 )
		srcY = -top;
	int maxY = rows;
	if ( rows + top > rgbaTall )
		maxY = rgbaTall - top;

	unsigned char* dstRow = &rgba[4 * rgbaWide * ( srcY + top )];
	int srcIndex = width * srcY;
	for ( ; srcY < maxY; srcY++ )
	{
		int left = slot->bitmap_left;
		int startX = 0;
		int endX = width;
		if ( left < 0 )
			startX = -left;
		if ( rgbaWide < width + left )
			endX = rgbaWide - left;

		unsigned char* dst = dstRow + 4 * ( startX + left );
		for ( int x = startX; x < endX; x++, dst += 4 )
		{
			dst[0] = 255;
			dst[1] = 255;
			dst[2] = 255;
			dst[3] = buffer[srcIndex + x];
		}

		srcIndex += width;
		dstRow += 4 * rgbaWide;
	}

	ApplyDropShadowToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, rgba, m_iDropShadowOffset );
	ApplyGaussianBlurToTexture( rgbaWide, rgbaTall, rgba, m_iBlur, m_pGaussianDistribution );
	ApplyScanlineEffectToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, rgba, m_iScanLines );
	ApplyRotaryEffectToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, rgba, m_bRotary );
}

bool CLinuxFont::IsEqualTo( const char* windowsFontName, int tall, int weight, int blur, int, int flags )
{
	if ( strcasecmp( windowsFontName, m_szName ) )
		return false;

	return m_iTall == tall && m_iWeight == weight && m_iBlur == blur && m_iFlags == flags;
}

bool CLinuxFont::IsValid()
{
	return m_szName[0] != 0;
}

int CLinuxFont::GetHeight()
{
	return m_iHeight;
}

int CLinuxFont::GetAscent()
{
	return m_iAscent;
}

int CLinuxFont::GetMaxCharWidth()
{
	return m_iMaxCharWidth;
}

int CLinuxFont::GetFlags()
{
	return m_iFlags;
}

void* CLinuxFont::SetAsActiveFont( GLXContext )
{
	return nullptr;
}

bool CLinuxFont::HasChar( wchar_t wch )
{
	return FT_Get_Char_Index( face, (FT_ULong)wch ) != 0;
}

void CLinuxFont::GetCharABCWidths( int ch, int& a, int& b, int& c )
{
	abc_cache_t finder;
	unsigned short i;

	finder.wch = ch;
	finder.abc.a = 0;
	finder.abc.b = 0;
	finder.abc.c = 0;

	i = m_ExtendedABCWidthsCache.Find( finder );
	if ( m_ExtendedABCWidthsCache.IsValidIndex( i ) )
	{
		a = m_ExtendedABCWidthsCache.Element( i ).abc.a;
		b = m_ExtendedABCWidthsCache.Element( i ).abc.b;
		c = m_ExtendedABCWidthsCache.Element( i ).abc.c;
		return;
	}

	a = 0;
	b = 0;
	c = 0;

	if ( FT_Load_Char( face, (FT_ULong)ch, 0 ) )
	{
		finder.abc.a = 0;
		finder.abc.c = 0;
		finder.abc.b = (short)( RoundHalfBBox( (int)face->bbox.xMax ) + m_iDropShadowOffset
			+ 2 * ( m_iOutlineSize + m_iBlur ) );
	}
	else
	{
		const FT_Glyph_Metrics& metrics = face->glyph->metrics;
		unsigned char blur = (unsigned char)m_iBlur;
		unsigned char outline = (unsigned char)m_iOutlineSize;
		unsigned char shadow = (unsigned char)m_iDropShadowOffset;
		char left = LowByteDifference( Fixed26Dot6ToInt( (int)metrics.horiBearingX ), blur, outline );
		int rightRaw = ( (int)metrics.horiAdvance - (int)metrics.horiBearingX ) - (int)metrics.width;
		char right = LowByteDifference( Fixed26Dot6ToInt( rightRaw ), blur, shadow, outline );
		int mid = Fixed26Dot6ToInt( (int)metrics.width ) + m_iDropShadowOffset + 2 * ( m_iOutlineSize + m_iBlur );
		finder.abc.a = 0;
		finder.abc.c = 0;
		finder.abc.b = (short)( (int)(unsigned short)mid + (short)left + (short)right );
	}

	m_ExtendedABCWidthsCache.Insert( finder );
	a = finder.abc.a;
	b = finder.abc.b;
	c = finder.abc.c;
}
