//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "utlvector.h"
#include "vgui/IScheme.h"

class CLinuxFont;

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
	void AddFont( CLinuxFont* font, int lowRange, int highRange );
	CLinuxFont* GetFontForChar( int ch );

	// returns the max height of the font set
	int GetFontHeight();
	int GetFontBlur();
	bool IsAdditive();
	int GetFontMaxWidth();

	int GetFlags( int index );
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
		CLinuxFont*	font;
	};

	CUtlVector<TFontRange>	m_Fonts;
	char					m_szName[32];
	int						m_iMaxWidth;
	int						m_iMaxHeight;
};
