//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "FontAmalgam.h"

#if !defined( _WIN32 )
struct FT_LibraryRec_;
#endif

//-----------------------------------------------------------------------------
// Purpose: Creates and maintains list of actively used fonts
//-----------------------------------------------------------------------------
class CFontManager
{
public:
	CFontManager();
	~CFontManager();

	void SetLanguage( const char* language );
	void ClearAllFonts();

	vgui2::HFont CreateFont();
	bool AddGlyphSetToFont( vgui2::HFont font, const char* name, int tall, int weight, int blur, int scanlines,
		int flags );
	vgui2::HFont GetFontByName( const char* name );

	void GetCharABCwide( vgui2::HFont font, int ch, int& a, int& b, int& c );
	int GetFontTall( vgui2::HFont font );
	int GetFontAscent( vgui2::HFont font, wchar_t ch );
	int GetCharacterWidth( vgui2::HFont font, int ch );
	bool GetFontUnderlined( vgui2::HFont font );
	void GetTextSize( vgui2::HFont font, const wchar_t* text, int& width, int& height );
	int GetFontBlur( vgui2::HFont font );
	bool IsAdditive( vgui2::HFont font );

	CLinuxFont* GetFontForChar( vgui2::HFont font, wchar_t ch );
	bool IsFontForeignLanguageCapable( const char* name );
	CLinuxFont* CreateOrFindWin32Font( const char* name, int tall, int weight, int blur, int scanlines, int flags );

	const char* GetFallbackFontName( const char* name );
	const char* GetForeignFallbackFontName();

private:
	CUtlVector<CFontAmalgam>	m_FontAmalgams;
	CUtlVector<CLinuxFont*>		m_Win32Fonts;
	char						m_szLanguage[64];

#if !defined( _WIN32 )
public:
	FT_LibraryRec_*				m_Library;
#endif
};

CFontManager& FontManager();
