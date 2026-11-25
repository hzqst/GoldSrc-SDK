#define UTLRBTREE_SOURCE_PATH L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\UtlRBTree.h"

#include "Win32Font.h"

#include "FontEffects.h"

#include <windows.h>
#include <malloc.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef GetCharABCWidths
#undef GetCharABCWidths
#endif

#include <algorithm>
#include <cstring>

#define strcasecmp _stricmp

extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );

#define WIN32FONT_SOURCE_PATH L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\vgui2\\vgui_surfacelib\\Win32Font.cpp"

namespace
{
constexpr int FONTFLAG_ITALIC = 0x001;
constexpr int FONTFLAG_UNDERLINE = 0x002;
constexpr int FONTFLAG_STRIKEOUT = 0x004;
constexpr int FONTFLAG_SYMBOL = 0x008;
constexpr int FONTFLAG_ANTIALIAS = 0x010;
constexpr int FONTFLAG_ROTARY = 0x040;
constexpr int FONTFLAG_DROPSHADOW = 0x080;
constexpr int FONTFLAG_ADDITIVE = 0x100;

bool g_bWindowsFontVersionInitialized;
bool g_bWindowsFontSupportsUnicode;
bool g_bWindowsFontFamilyFound;
}

bool CWin32Font::ExtendedABCWidthsCacheLessFunc( const abc_cache_t& lhs, const abc_cache_t& rhs )
{
	return lhs.wch < rhs.wch;
}

CWin32Font::CWin32Font() : m_ExtendedABCWidthsCache( 256, 0, &CWin32Font::ExtendedABCWidthsCacheLessFunc )
{
	m_szName[0] = 0;
	m_iTall = 0;
	m_iWeight = 0;
	m_iFlags = 0;
	m_bAntiAliased = false;
	m_bRotary = false;
	m_iDropShadowOffset = 0;
	m_bUnderlined = false;
	m_iHeight = 0;
	m_iMaxCharWidth = 0;
	m_iAscent = 0;
	memset( m_ABCWidths, 0, sizeof( m_ABCWidths ) );
	m_hFont = nullptr;
	m_hDC = nullptr;
	m_hDIB = nullptr;
	m_rgiBitmapSize[0] = 0;
	m_rgiBitmapSize[1] = 0;
	m_pBuf = nullptr;
	m_bAdditive = false;
	m_iScanLines = 0;
	m_iBlur = 0;
	m_pGaussianDistribution = nullptr;

	if ( !g_bWindowsFontVersionInitialized )
	{
		g_bWindowsFontVersionInitialized = true;
		OSVERSIONINFOA version = {};
		version.dwOSVersionInfoSize = sizeof( version );
#if defined( _MSC_VER )
#pragma warning( suppress : 4996 28159 )
		GetVersionExA( &version );
#else
		GetVersionExA( &version );
#endif
		g_bWindowsFontSupportsUnicode = version.dwMajorVersion >= 5;
	}
}

CWin32Font::~CWin32Font()
{
	if ( m_hFont )
		DeleteObject( static_cast<HGDIOBJ>( m_hFont ) );
	if ( m_hDC )
		DeleteDC( static_cast<HDC>( m_hDC ) );
	if ( m_hDIB )
		DeleteObject( static_cast<HGDIOBJ>( m_hDIB ) );
	delete[] m_pGaussianDistribution;
}

static int CALLBACK FontFamilyFound( const LOGFONTA*, const TEXTMETRICA*, DWORD, LPARAM )
{
	g_bWindowsFontFamilyFound = true;
	return 0;
}

bool CWin32Font::CreateWindowsFont( const char* name, int tall, int weight, int flags )
{
	static int runningUnderWine = -1;
	if ( runningUnderWine == -1 )
	{
		runningUnderWine = 0;
		HMODULE module = GetModuleHandleA( "ntdll.dll" );
		if ( module != nullptr && GetProcAddress( module, "wine_get_version" ) != nullptr )
			runningUnderWine = 1;
	}
	m_bAntiAliased = runningUnderWine != 0 || ( flags & FONTFLAG_ANTIALIAS ) != 0;

	int charset = ( flags & FONTFLAG_SYMBOL ) ? SYMBOL_CHARSET : ANSI_CHARSET;
	m_bUnderlined = ( flags & FONTFLAG_UNDERLINE ) != 0;
	m_iDropShadowOffset = ( flags & FONTFLAG_DROPSHADOW ) != 0;
	m_bRotary = ( flags & FONTFLAG_ROTARY ) != 0;
	m_bAdditive = ( flags & FONTFLAG_ADDITIVE ) != 0;
	if ( !strcasecmp( name, "win98japanese" ) )
	{
		charset = SHIFTJIS_CHARSET;
		strncpy( m_szName, "Tahoma", sizeof( m_szName ) );
	}

	m_hDC = CreateCompatibleDC( nullptr );
	HDC dc = static_cast<HDC>( m_hDC );
	LOGFONTA requested = {};
	requested.lfCharSet = DEFAULT_CHARSET;
	requested.lfPitchAndFamily = 0;
	strcpy( requested.lfFaceName, m_szName );
	g_bWindowsFontFamilyFound = false;
	EnumFontFamiliesExA( dc, &requested, FontFamilyFound, 0, 0 );
	if ( !g_bWindowsFontFamilyFound )
		return false;

	m_hFont = CreateFontA( tall, 0, 0, 0, weight, flags & FONTFLAG_ITALIC, flags & FONTFLAG_UNDERLINE,
		flags & FONTFLAG_STRIKEOUT, charset, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		m_bAntiAliased ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, name );
	if ( !m_hFont )
		return false;

	SetMapMode( dc, MM_TEXT );
	SelectObject( dc, static_cast<HFONT>( m_hFont ) );
	SetTextAlign( dc, TA_LEFT | TA_TOP | TA_UPDATECP );

	TEXTMETRICA tm = {};
	GetTextMetricsA( dc, &tm );
	m_iHeight = tm.tmHeight + m_iDropShadowOffset;
	m_iAscent = tm.tmAscent;
	m_iMaxCharWidth = tm.tmMaxCharWidth;
	m_rgiBitmapSize[0] = tm.tmMaxCharWidth;
	m_rgiBitmapSize[1] = m_iHeight;

	BITMAPINFO bitmapInfo = {};
	bitmapInfo.bmiHeader.biSize = sizeof( bitmapInfo.bmiHeader );
	bitmapInfo.bmiHeader.biWidth = m_rgiBitmapSize[0];
	bitmapInfo.bmiHeader.biHeight = -m_rgiBitmapSize[1];
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	m_hDIB = CreateDIBSection( dc, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>( &m_pBuf ), nullptr, 0 );
	SelectObject( dc, static_cast<HBITMAP>( m_hDIB ) );

	memset( m_ABCWidths, 0, sizeof( m_ABCWidths ) );
	ABC widths[ASCII_CHAR_COUNT];
	if ( GetCharABCWidthsW( dc, 0, ASCII_CHAR_COUNT - 1, widths )
		|| GetCharABCWidthsA( dc, 0, ASCII_CHAR_COUNT - 1, widths ) )
	{
		for ( int ch = 0; ch < ASCII_CHAR_COUNT; ++ch )
		{
			m_ABCWidths[ch].a = static_cast<signed char>( widths[ch].abcA - m_iBlur );
			m_ABCWidths[ch].b = static_cast<signed char>( widths[ch].abcB + 2 * m_iBlur + m_iDropShadowOffset );
			m_ABCWidths[ch].c = static_cast<signed char>( widths[ch].abcC - m_iBlur - m_iDropShadowOffset );
		}
	}
	else
	{
		for ( int ch = 0; ch < ASCII_CHAR_COUNT; ++ch )
			m_ABCWidths[ch].b = static_cast<signed char>( tm.tmAveCharWidth );
	}
	return true;
}

bool CWin32Font::Create( const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags )
{
	strncpy( m_szName, windowsFontName, sizeof( m_szName ) - 1 );
	m_szName[sizeof( m_szName ) - 1] = 0;
	m_iWeight = weight;
	m_iTall = tall;
	m_iFlags = flags;
	m_iBlur = blur;
	m_iScanLines = scanlines;

	if ( !CreateWindowsFont( windowsFontName, tall, weight, flags ) )
	{
		m_szName[0] = 0;
		return false;
	}

	delete[] m_pGaussianDistribution;
	m_pGaussianDistribution = CreateGaussianDistribution( m_iBlur );
	return true;
}

void CWin32Font::RenderWindowsGlyph( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba )
{
	HDC dc = static_cast<HDC>( m_hDC );

	int a, b, c;
	GetCharABCWidths( ch, a, b, c );
	SelectObject( dc, static_cast<HFONT>( m_hFont ) );
	const int charWide = m_bUnderlined ? a + b + c : b;

	MAT2 transform = {};
	transform.eM11.value = 1;
	transform.eM22.value = 1;
	GLYPHMETRICS metrics = {};
	int glyphBytes = 0;
	if ( m_bAntiAliased && ch <= 255 )
	{
		SelectObject( dc, static_cast<HFONT>( m_hFont ) );
		glyphBytes =
			static_cast<int>( GetGlyphOutlineA( dc, ch, GGO_GRAY8_BITMAP, &metrics, 0, nullptr, &transform ) );
	}

	if ( glyphBytes > 0 )
	{
		unsigned char* bitmap = static_cast<unsigned char*>( _alloca( glyphBytes ) );
		GetGlyphOutlineA( dc, ch, GGO_GRAY8_BITMAP, &metrics, glyphBytes, bitmap, &transform );

		const int blackBoxWide = static_cast<int>( metrics.gmBlackBoxX );
		const int stride = ( blackBoxWide + 3 ) & ~3;
		const int startX = blackBoxWide >= b + 2 ? ( blackBoxWide - b ) / 2 : 0;
		const int top = m_iAscent - metrics.gmptGlyphOrigin.y;
		for ( int row = 0; row < static_cast<int>( metrics.gmBlackBoxY ); ++row )
		{
			const int y = rgbaY + row + top;
			for ( int col = startX; col < blackBoxWide; ++col )
			{
				const int x = rgbaX + col - startX + m_iBlur;
				if ( x >= rgbaWide || y >= rgbaTall )
					continue;
				const unsigned char coverage = bitmap[row * stride + col];
				const unsigned char color = coverage && ch != '\t' ? 255 : 0;
				unsigned char* pixel = &rgba[4 * ( x + y * rgbaWide )];
				pixel[0] = pixel[1] = pixel[2] = color;
				pixel[3] = static_cast<unsigned char>( std::min( 1.0f, coverage / 64.0f ) * 255.0f );
			}
		}
		return;
	}

	SetBkColor( dc, RGB( 0, 0, 0 ) );
	SetTextColor( dc, RGB( 255, 255, 255 ) );
	SetBkMode( dc, OPAQUE );
	MoveToEx( dc, m_bUnderlined ? 0 : -a, 0, nullptr );
	const wchar_t wide = static_cast<wchar_t>( ch );
	if ( g_bWindowsFontSupportsUnicode )
	{
		ExtTextOutW( dc, 0, 0, 0, nullptr, &wide, 1, nullptr );
	}
	else
	{
		RECT rect = { 0, 0, charWide, m_iHeight };
		ExtTextOutA( dc, 0, 0, ETO_OPAQUE, &rect, nullptr, 0, nullptr );
		char text[6] = {};
		WideCharToMultiByte( CP_ACP, 0, &wide, 1, text, sizeof( text ), nullptr, nullptr );
		ExtTextOutA( dc, 0, 0, 0, nullptr, text, static_cast<UINT>( strlen( text ) ), nullptr );
	}
	SetBkMode( dc, TRANSPARENT );

	const int width = std::min( charWide, m_rgiBitmapSize[0] );
	const int height = std::min( m_iHeight, m_rgiBitmapSize[1] );
	for ( int row = 0; row < height; ++row )
	{
		const int y = rgbaY + row;
		for ( int col = 0; col < width - m_iDropShadowOffset; ++col )
		{
			const int x = rgbaX + col;
			if ( x >= rgbaWide || y >= rgbaTall )
				continue;
			const unsigned char* source = &m_pBuf[4 * ( col + row * m_rgiBitmapSize[0] )];
			float red = source[0] / 255.0f;
			float green = source[1] / 255.0f;
			float blue = source[2] / 255.0f;
			if ( ch == '\t' )
				red = green = blue = 0.0f;
			unsigned char* pixel = &rgba[4 * ( x + y * rgbaWide )];
			pixel[0] = static_cast<unsigned char>( red * 255.0f );
			pixel[1] = static_cast<unsigned char>( green * 255.0f );
			pixel[2] = static_cast<unsigned char>( blue * 255.0f );
			pixel[3] = static_cast<unsigned char>( ( green * 0.55f + red * 0.34f + blue * 0.11f ) * 255.0f );
		}
	}

	if ( m_iDropShadowOffset )
	{
		unsigned char* pixel = &rgba[4 * ( rgbaX + ( m_iHeight - 1 ) * rgbaWide )];
		for ( int col = 0; col < width; ++col, pixel += 4 )
			memset( pixel, 0, 4 );
	}
}

void CWin32Font::GetCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba )
{
	RenderWindowsGlyph( ch, rgbaX, rgbaY, rgbaWide, rgbaTall, rgba );

	ApplyDropShadowToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, rgba, m_iDropShadowOffset );
	ApplyGaussianBlurToTexture( rgbaWide, rgbaTall, rgba, m_iBlur, m_pGaussianDistribution );
	ApplyScanlineEffectToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, rgba, m_iScanLines );
	ApplyRotaryEffectToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, rgba, m_bRotary );
}

bool CWin32Font::IsEqualTo( const char* windowsFontName, int tall, int weight, int blur, int, int flags )
{
	if ( strcasecmp( windowsFontName, m_szName ) )
		return false;

	return m_iTall == tall && m_iWeight == weight && m_iBlur == blur && m_iFlags == flags;
}

bool CWin32Font::IsValid()
{
	return m_szName[0] != 0;
}

int CWin32Font::GetHeight()
{
	if ( !IsValid() )
		_wassert( L"IsValid()", WIN32FONT_SOURCE_PATH, 702 );
	return m_iHeight;
}

int CWin32Font::GetAscent()
{
	if ( !IsValid() )
		_wassert( L"IsValid()", WIN32FONT_SOURCE_PATH, 711 );
	return m_iAscent;
}

int CWin32Font::GetMaxCharWidth()
{
	if ( !IsValid() )
		_wassert( L"IsValid()", WIN32FONT_SOURCE_PATH, 720 );
	return m_iMaxCharWidth;
}

int CWin32Font::GetFlags()
{
	return m_iFlags;
}

void CWin32Font::SetAsActiveFont( HDC hdc )
{
	if ( !IsValid() )
		_wassert( L"IsValid()", WIN32FONT_SOURCE_PATH, 630 );
	SelectObject( hdc, static_cast<HFONT>( m_hFont ) );
}

void CWin32Font::GetCharABCWidths( int ch, int& a, int& b, int& c )
{
	if ( !IsValid() )
		_wassert( L"IsValid()", WIN32FONT_SOURCE_PATH, 639 );

	if ( ch < ASCII_CHAR_COUNT )
	{
		a = m_ABCWidths[ch].a;
		b = m_ABCWidths[ch].b;
		c = m_ABCWidths[ch].c;
		return;
	}

	abc_cache_t entry = {};
	entry.wch = static_cast<unsigned short>( ch );
	unsigned short index = m_ExtendedABCWidthsCache.Find( entry );
	if ( m_ExtendedABCWidthsCache.IsValidIndex( index ) )
	{
		const abc_t& cached = m_ExtendedABCWidthsCache[index].abc;
		a = cached.a;
		b = cached.b;
		c = cached.c;
		return;
	}

	ABC measured = {};
	if ( GetCharABCWidthsW( static_cast<HDC>( m_hDC ), ch, ch, &measured ) )
	{
		a = measured.abcA;
		b = static_cast<int>( measured.abcB );
		c = measured.abcC;
	}
	else
	{
		wchar_t wide = static_cast<wchar_t>( ch );
		char text[6] = {};
		WideCharToMultiByte( CP_ACP, 0, &wide, 1, text, sizeof( text ), nullptr, nullptr );
		SIZE size = {};
		a = c = 0;
		b = GetTextExtentPoint32A( static_cast<HDC>( m_hDC ), text, static_cast<int>( strlen( text ) ), &size )
			? size.cx : m_iMaxCharWidth;
	}

	entry.abc.a = static_cast<signed char>( a );
	entry.abc.b = static_cast<signed char>( b );
	entry.abc.c = static_cast<signed char>( c );
	m_ExtendedABCWidthsCache.Insert( entry );
}
