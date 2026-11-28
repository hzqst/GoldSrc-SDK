//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "utlvector.h"
#include "vgui/IScheme.h"

#if defined( _WIN32 )
class CWin32Font;
using CPlatformFont = CWin32Font;
#else
class CLinuxFont;
using CPlatformFont = CLinuxFont;
#endif

//-----------------------------------------------------------------------------
// Purpose: object that holds a set of fonts in specified ranges
//-----------------------------------------------------------------------------
class CFontAmalgam
{
public:
	CFontAmalgam();
	~CFontAmalgam();

	const char* Name();
	void SetName( const char* name );

	// adds a font to the amalgam
	void AddFont( CPlatformFont* font, int lowRange, int highRange );
	CPlatformFont* GetFontForChar( int ch );

	// returns the max height of the font set
	int GetFontHeight();
	int GetFontBlur();
	bool IsAdditive();
	int GetFontMaxWidth();

	int GetFlags( int index );
	// returns the font at index, which is lower than GetCount()
	CPlatformFont* GetFont( int index );
	// returns the windows name for the font
	const char* GetFontName( int index );
	// returns the number of fonts in this amalgam
	int GetCount();
	bool GetUnderlined();
	void RemoveAll();

private:
	struct TFontRange
	{
		int			lowRange;
		int			highRange;
		CPlatformFont*	font;
	};

	CUtlVector<TFontRange>	m_Fonts;
	char					m_szName[32];
	int						m_iMaxWidth;
	int						m_iMaxHeight;
};
