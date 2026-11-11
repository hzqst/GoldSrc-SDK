//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "FontAmalgam.h"
#include "LinuxFont.h"

#include <cassert>
#include <cstring>

#if defined( _WIN32 )
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );

#define FONTAMALGAM_SOURCE_PATH L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\vgui2\\vgui_surfacelib\\FontAmalgam.cpp"
#endif

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontAmalgam::CFontAmalgam()
{
	m_Fonts.EnsureCapacity( 4 );
	m_iMaxHeight = 0;
	m_iMaxWidth = 0;
}

CFontAmalgam::~CFontAmalgam()
{
}

void CFontAmalgam::RemoveAll()
{
	m_Fonts.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of fonts this amalgam contains
//-----------------------------------------------------------------------------
int CFontAmalgam::GetCount()
{
	return m_Fonts.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character in a font
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFontMaxWidth()
{
	return m_iMaxWidth;
}

//-----------------------------------------------------------------------------
// Purpose: Data accessor
//-----------------------------------------------------------------------------
const char* CFontAmalgam::Name()
{
	return m_szName;
}

//-----------------------------------------------------------------------------
// Purpose: Data accessor
//-----------------------------------------------------------------------------
void CFontAmalgam::SetName( const char* name )
{
	std::strncpy( m_szName, name, sizeof( m_szName ) - 1 );
	m_szName[sizeof( m_szName ) - 1] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: adds a font to the amalgam
//-----------------------------------------------------------------------------
void CFontAmalgam::AddFont( CLinuxFont* font, int lowRange, int highRange )
{
	int index = m_Fonts.AddToTail();
	m_Fonts[index].font = font;
	m_Fonts[index].lowRange = lowRange;
	m_Fonts[index].highRange = highRange;

	int height = font->GetHeight();
	if ( m_iMaxHeight >= height )
		height = m_iMaxHeight;
	m_iMaxHeight = height;

	int maxCharWidth = font->GetMaxCharWidth();
	if ( m_iMaxWidth >= maxCharWidth )
		maxCharWidth = m_iMaxWidth;
	m_iMaxWidth = maxCharWidth;
}

CLinuxFont* CFontAmalgam::GetFontForChar( int ch )
{
	int count = m_Fonts.Count();
	if ( count <= 0 )
		return nullptr;

	for ( int index = 0; index < count; ++index )
	{
		if ( m_Fonts[index].lowRange <= ch && ch <= m_Fonts[index].highRange )
		{
#if defined( _WIN32 )
			if ( !m_Fonts[index].font->IsValid() )
				_wassert( L"m_Fonts[i].font->IsValid()", FONTAMALGAM_SOURCE_PATH, 76 );
			return m_Fonts[index].font;
#else
			if ( m_Fonts[index].font->HasChar( static_cast<wchar_t>( ch ) ) )
				return m_Fonts[index].font;
			count = m_Fonts.Count();
#endif
		}
	}

	return nullptr;
}

int CFontAmalgam::GetFontHeight()
{
	if ( m_Fonts.Count() != 0 )
		return m_Fonts[0].font->GetHeight();
	return m_iMaxHeight;
}

int CFontAmalgam::GetFontBlur()
{
	if ( m_Fonts.Count() != 0 )
		return m_Fonts[0].font->GetBlur();
	return 0;
}

bool CFontAmalgam::IsAdditive()
{
	if ( m_Fonts.Count() != 0 )
		return m_Fonts[0].font->IsAdditive();
	return false;
}

const char* CFontAmalgam::GetFontName( int index )
{
	return m_Fonts[index].font->GetName();
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the font that is loaded
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFlags( int index )
{
	if ( m_Fonts[index].font != nullptr )
		return m_Fonts[index].font->GetFlags();
	return 0;
}

bool CFontAmalgam::GetUnderlined()
{
	if ( m_Fonts.Count() != 0 )
		return m_Fonts[0].font->GetUnderlined();
	return false;
}
