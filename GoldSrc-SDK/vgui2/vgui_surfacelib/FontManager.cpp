//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "FontManager.h"
#include "LinuxFont.h"

#if !defined( _WIN32 )
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#include <clocale>
#include <cstring>
#include <cwctype>

#if defined( _WIN32 )
#define strcasecmp _stricmp
#endif

struct FallbackFont_t
{
	const char* font;
	const char* fallbackFont;
};

#if defined( _WIN32 )
FallbackFont_t g_ForceReplaceFonts[] = {
	{ "Marlett", nullptr },
	{ "Times New Roman", "Courier New" },
	{ "Courier New", "Courier" },
	{ "Verdana", "Arial" },
	{ "Trebuchet MS", "Arial" },
	{ "Tahoma", nullptr },
	{ nullptr, nullptr },
};
FallbackFont_t g_FallbackFonts[] = {
	{ "Times New Roman", "Courier New" },
	{ "Courier New", "Courier" },
	{ "Verdana", "Arial" },
	{ "Trebuchet MS", "Arial" },
	{ "Tahoma", nullptr },
	{ nullptr, "Tahoma" },
};
#else
FallbackFont_t g_ForceReplaceFonts[] = {
	{ "Trebuchet MS", "Fira Sans" },
	{ "Verdana", "DejaVu Sans" },
	{ "Verdana Bold Italic", "DejaVu Sans" },
	{ "Verdana Bold", "DejaVu Sans" },
	{ "Verdana Italic", "DejaVu Sans" },
	{ "Tahoma", "Liberation Sans" },
	{ "Arial", "Liberation Sans" },
	{ "Arial Bold", "Liberation Sans" },
	{ "Courier", "Liberation Mono" },
	{ "Courier New", "Liberation Mono" },
	{ "Lucida Console", "Liberation Mono" },
	{ nullptr, nullptr },
};

FallbackFont_t g_FallbackFonts[] = {
	{ "DejaVu Sans", nullptr },
	{ nullptr, "DejaVu Sans" },
};
#endif

static const char* g_szValidAsianFonts[] = {
	"Marlett",
#if !defined( _WIN32 )
	"WenQuanYi Zen Hei",
	"unifont",
#endif
	nullptr,
};

//-----------------------------------------------------------------------------
// Purpose: returns the pixel width of a single character
//-----------------------------------------------------------------------------
int CFontManager::GetCharacterWidth( vgui2::HFont font, int ch )
{
	if ( !std::iswprint( static_cast<std::wint_t>( ch ) ) )
		return 0;

	int a, b, c;
	GetCharABCwide( font, ch, a, b, c );
	return c + a + b;
}

int CFontManager::GetFontBlur( vgui2::HFont font )
{
	return m_FontAmalgams[font].GetFontBlur();
}

CLinuxFont* CFontManager::GetFontForChar( vgui2::HFont font, wchar_t ch )
{
	return m_FontAmalgams[font].GetFontForChar( ch );
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
int CFontManager::GetFontTall( vgui2::HFont font )
{
	return m_FontAmalgams[font].GetFontHeight();
}

bool CFontManager::GetFontUnderlined( vgui2::HFont font )
{
	return m_FontAmalgams[font].GetUnderlined();
}

//-----------------------------------------------------------------------------
// Purpose: returns the area of a text string, including newlines
//-----------------------------------------------------------------------------
void CFontManager::GetTextSize( vgui2::HFont font, const wchar_t* text, int& width, int& height )
{
	width = 0;
	height = 0;
	if ( !text )
		return;

	height = GetFontTall( font );
	int lineWidth = 0;
	for ( wchar_t ch = *text; ch; ch = *++text )
	{
		if ( ch == L'\n' )
		{
			height += GetFontTall( font );
			lineWidth = 0;
		}
		else if ( ch != L'&' )
		{
			lineWidth += GetCharacterWidth( font, ch );
			if ( lineWidth > width )
				width = lineWidth;
		}
	}
}

static CFontManager g_FontManager;

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CFontManager::CFontManager()
{
	// add a single empty font, to act as an invalid font handle 0
	constexpr int INITIAL_FONT_CAPACITY = 100;
	m_FontAmalgams.EnsureCapacity( INITIAL_FONT_CAPACITY );
	m_FontAmalgams.AddToTail();
	m_Win32Fonts.EnsureCapacity( INITIAL_FONT_CAPACITY );
	std::setlocale( LC_CTYPE, "" );
	std::setlocale( LC_TIME, "" );
	std::setlocale( LC_COLLATE, "" );
	std::setlocale( LC_MONETARY, "" );
#if !defined( _WIN32 )
	FT_Init_FreeType( &m_Library );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CFontManager& FontManager()
{
	return g_FontManager;
}

CFontManager::~CFontManager()
{
	int index = 0;
	int count = m_Win32Fonts.Count();
	if ( count > 0 )
	{
		do
		{
			CLinuxFont* font = m_Win32Fonts[index];
			if ( font != nullptr )
			{
				delete font;
				count = m_Win32Fonts.Count();
			}
			++index;
		} while ( index < count );
	}

	m_Win32Fonts.RemoveAll();

	for ( int amalgam = 0; amalgam < m_FontAmalgams.Count(); ++amalgam )
		m_FontAmalgams[amalgam].RemoveAll();

#if !defined( _WIN32 )
	FT_Done_FreeType( m_Library );
#endif
	m_Win32Fonts.Purge();
	m_FontAmalgams.Purge();
}

void CFontManager::SetLanguage( const char* language )
{
	std::strncpy( m_szLanguage, language, sizeof( m_szLanguage ) );
}

void CFontManager::ClearAllFonts()
{
	int index = 0;
	int count = m_Win32Fonts.Count();
	if ( count > 0 )
	{
		do
		{
			CLinuxFont* font = m_Win32Fonts[index];
			if ( font != nullptr )
			{
				delete font;
				count = m_Win32Fonts.Count();
			}
			++index;
		} while ( index < count );
	}

	m_Win32Fonts.RemoveAll();

	for ( int amalgam = 0; amalgam < m_FontAmalgams.Count(); ++amalgam )
		m_FontAmalgams[amalgam].RemoveAll();
}

vgui2::HFont CFontManager::CreateFont()
{
	return static_cast<vgui2::HFont>( m_FontAmalgams.AddToTail() );
}

CLinuxFont* CFontManager::CreateOrFindWin32Font(
	const char* name, int tall, int weight, int blur, int scanlines, int flags )
{
	if ( m_Win32Fonts.Count() > 0 )
	{
		int index = 0;
		while ( !m_Win32Fonts[index]->IsEqualTo( name, tall, weight, blur, scanlines, flags ) )
		{
			++index;
			if ( index >= m_Win32Fonts.Count() )
				break;
		}

		if ( index < m_Win32Fonts.Count() )
		{
			CLinuxFont* found = m_Win32Fonts[index];
			if ( found != nullptr )
				return found;
		}
	}

#if defined( _WIN32 )
	const int createdIndex = m_Win32Fonts.AddToTail();
	m_Win32Fonts[createdIndex] = new CLinuxFont;
	CLinuxFont* created = m_Win32Fonts[createdIndex];
#else
	CLinuxFont* created = new CLinuxFont;
	const int createdIndex = m_Win32Fonts.AddToTail( created );
#endif
	if ( !created->Create( name, tall, weight, blur, scanlines, flags ) )
	{
		CLinuxFont* failed = m_Win32Fonts[createdIndex];
		if ( failed != nullptr )
			delete failed;
		m_Win32Fonts.Remove( createdIndex );
		return nullptr;
	}

	return m_Win32Fonts[createdIndex];
}

bool CFontManager::AddGlyphSetToFont(
	vgui2::HFont font, const char* name, int tall, int weight, int blur, int scanlines, int flags )
{
	if ( m_FontAmalgams[font].GetCount() > 0 )
		return false;

	const char* fontName = name;
#if !defined( _WIN32 )
	for ( int index = 0; g_ForceReplaceFonts[index].font != nullptr; ++index )
	{
		if ( strcasecmp( g_ForceReplaceFonts[index].font, name ) == 0 )
		{
			fontName = g_ForceReplaceFonts[index].fallbackFont;
			break;
		}
	}
#endif

	CLinuxFont* primary = CreateOrFindWin32Font( fontName, tall, weight, blur, scanlines, flags );
	const char* foreignFallback = GetForeignFallbackFontName();
	do
	{
		if ( IsFontForeignLanguageCapable( fontName ) )
		{
			if ( primary != nullptr )
			{
				m_FontAmalgams[font].AddFont( primary, 0, 0xFFFF );
				return true;
			}
		}
		else if ( primary == nullptr )
		{
			CLinuxFont* fallback = CreateOrFindWin32Font( foreignFallback, tall, weight, blur, scanlines, flags );
			if ( fallback != nullptr )
			{
				m_FontAmalgams[font].AddFont( fallback, 0, 0xFFFF );
				return true;
			}
		}
		else if ( strcasecmp( foreignFallback, fontName ) == 0 )
		{
			m_FontAmalgams[font].AddFont( primary, 0, 0xFFFF );
			return true;
		}
		else
		{
			CLinuxFont* fallback = CreateOrFindWin32Font( foreignFallback, tall, weight, blur, scanlines, flags );
			if ( fallback != nullptr )
			{
				m_FontAmalgams[font].AddFont( primary, 0, 255 );
				m_FontAmalgams[font].AddFont( fallback, 256, 0xFFFF );
				return true;
			}
		}

		fontName = GetFallbackFontName( fontName );
	} while ( fontName != nullptr );

	return false;
}

vgui2::HFont CFontManager::GetFontByName( const char* name )
{
	if ( m_FontAmalgams.Count() <= 1 )
		return 0;

	int index = 1;
	while ( strcasecmp( name, m_FontAmalgams[index].Name() ) != 0 )
	{
		++index;
		if ( index >= m_FontAmalgams.Count() )
			return 0;
	}

	return static_cast<vgui2::HFont>( index );
}

//-----------------------------------------------------------------------------
// Purpose: returns the abc widths of a single character
//-----------------------------------------------------------------------------
void CFontManager::GetCharABCwide( vgui2::HFont font, int ch, int& a, int& b, int& c )
{
	CLinuxFont* fontForChar = m_FontAmalgams[font].GetFontForChar( ch );
	if ( fontForChar != nullptr )
	{
		fontForChar->GetCharABCWidths( ch, a, b, c );
	}
	else
	{
		c = 0;
		a = 0;
		b = m_FontAmalgams[font].GetFontMaxWidth();
	}
}

bool CFontManager::IsAdditive( vgui2::HFont font )
{
	return m_FontAmalgams[font].IsAdditive();
}

int CFontManager::GetFontAscent( vgui2::HFont font, wchar_t ch )
{
	CLinuxFont* fontForChar = m_FontAmalgams[font].GetFontForChar( ch );
	if ( fontForChar != nullptr )
		return fontForChar->GetAscent();

	return 0;
}

bool CFontManager::IsFontForeignLanguageCapable( const char* name )
{
	const char* asianName = "Marlett";
	const char** asianFonts = g_szValidAsianFonts;
	while ( strcasecmp( asianName, name ) != 0 )
	{
		asianName = *++asianFonts;
		if ( *asianFonts == nullptr )
			return false;
	}

	return true;
}

const char* GetReplacementFont( const char* windowsFontName )
{
	const char* font = g_ForceReplaceFonts[0].font;
	if ( font == nullptr )
		return windowsFontName;

	int index = 0;
	while ( strcasecmp( font, windowsFontName ) != 0 )
	{
		font = g_ForceReplaceFonts[++index].font;
		if ( font == nullptr )
			return windowsFontName;
	}

	return g_ForceReplaceFonts[index].fallbackFont;
}

const char* CFontManager::GetFallbackFontName( const char* name )
{
	int index = 0;
	while ( g_FallbackFonts[index].font != nullptr )
	{
		if ( strcasecmp( g_FallbackFonts[index].font, name ) == 0 )
			break;
		++index;
	}
	return g_FallbackFonts[index].fallbackFont;
}

const char* CFontManager::GetForeignFallbackFontName()
{
#if defined( _WIN32 )
	return "Tahoma";
#else
	return "Noto Sans";
#endif
}
