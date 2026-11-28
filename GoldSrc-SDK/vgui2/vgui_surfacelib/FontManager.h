//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "FontAmalgam.h"

#if !defined( _WIN32 )
#include "utlrbtree.h"

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

	CPlatformFont* GetFontForChar( vgui2::HFont font, int ch );
	bool IsFontForeignLanguageCapable( const char* name );
	CPlatformFont* CreateOrFindWin32Font( const char* name, int tall, int weight, int blur, int scanlines, int flags );

	const char* GetFallbackFontName( const char* name );
	const char* GetForeignFallbackFontName();

private:
	CUtlVector<CFontAmalgam>	m_FontAmalgams;
	CUtlVector<CPlatformFont*>	m_Win32Fonts;
	char						m_szLanguage[64];

#if !defined( _WIN32 )
	// A Linux font has only the characters of its own file. For a character none of the fonts of the amalgam has,
	// fontconfig finds an installed font that has it, which is added to the amalgam.
	CPlatformFont* AddFontForChar( vgui2::HFont font, int ch );

	// Cache lookup results per amalgam, since fontconfig prefers its primary font's family and style.
	struct CharFamily_t
	{
		vgui2::HFont	font;
		int		ch;
		char	family[64];
	};

	static bool CharFamilyLess( const CharFamily_t& lhs, const CharFamily_t& rhs );

	CUtlRBTree<CharFamily_t, int>	m_CharFamilies;
#endif

#if !defined( _WIN32 )
public:
	FT_LibraryRec_*				m_Library;
#endif
};

CFontManager& FontManager();
