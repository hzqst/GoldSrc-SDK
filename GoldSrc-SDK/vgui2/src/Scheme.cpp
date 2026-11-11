//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "Scheme.h"
#include "Bitmap.h"
#include "Border.h"
#include "IKeyValues.h"
#include "platform.h"
#include "vgui_internal.h"

#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <KeyValues.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\src\\Scheme.cpp" )

using namespace vgui2;

const char* CSchemeManager::s_pszSearchString = NULL;

//-----------------------------------------------------------------------------
// Purpose: search function for stored bitmaps
//-----------------------------------------------------------------------------
bool CSchemeManager::BitmapHandleSearchFunc( const CachedBitmapHandle_t& lhs, const CachedBitmapHandle_t& rhs )
{
	if ( lhs.bitmap )
	{
		if ( rhs.bitmap )
		{
			return strcasecmp( lhs.bitmap->GetName(), rhs.bitmap->GetName() ) > 0;
		}

		return strcasecmp( lhs.bitmap->GetName(), s_pszSearchString ) > 0;
	}

	return strcasecmp( s_pszSearchString, rhs.bitmap->GetName() ) > 0;
}

CSchemeManager g_Scheme;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSchemeManager, ISchemeManager, "VGUI_Scheme009", g_Scheme );

namespace vgui2
{
ISchemeManager* g_pScheme = &g_Scheme;
}

//-----------------------------------------------------------------------------
// Converts the handle into an interface
//-----------------------------------------------------------------------------
IScheme* CSchemeManager::GetIScheme( HScheme scheme )
{
	return _scheme[scheme];
}

//-----------------------------------------------------------------------------
// Purpose: returns a handle to the default (first loaded) scheme
//-----------------------------------------------------------------------------
HScheme CSchemeManager::GetDefaultScheme()
{
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: returns a handle to the scheme identified by "tag"
//-----------------------------------------------------------------------------
HScheme CSchemeManager::GetScheme( const char* tag )
{
	for ( int i = 1; i < _scheme.Count(); ++i )
	{
		if ( !strcasecmp( tag, _scheme[i]->tag ) )
		{
			return i;
		}
	}

	return 1; // default scheme
}

void CSchemeManager::Shutdown( bool full )
{
	if ( _scheme.Count() <= 1 )
	{
		return;
	}

	for ( int i = 1; i < _scheme.Count(); ++i )
	{
		CScheme* pScheme = _scheme[i];

		for ( int j = 0; j < pScheme->m_BorderList.Count(); ++j )
		{
			if ( !pScheme->m_BorderList[j].bSharedBorder )
			{
				Border* pBorder = pScheme->m_BorderList[j].border;

				if ( pBorder )
				{
					delete pBorder;
				}
			}
		}

		pScheme->_baseBorder = NULL;
		pScheme->m_BorderList.RemoveAll();
		pScheme->_borders = NULL;

		if ( full && pScheme->data )
		{
			pScheme->data->deleteThis();
			pScheme->data = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: reloads the scheme from the file
//-----------------------------------------------------------------------------
void CSchemeManager::ReloadSchemes()
{
	int count = _scheme.Count();

	Shutdown( false );

	// reload the scheme
	for ( int i = 1; i < count; ++i )
	{
		LoadSchemeFromFile( _scheme[i]->fileName, _scheme[i]->tag );
	}
}

float CSchemeManager::GetProportionalScale()
{
	int wide;
	int tall;
	int proW;
	int proH;

	g_pSurface->GetScreenSize( wide, tall );
	g_pSurface->GetProportionalBase( proW, proH );
	return (float)( (double)tall / (double)proH );
}

//-----------------------------------------------------------------------------
// Purpose: converts a value into proportional mode
//-----------------------------------------------------------------------------
int CSchemeManager::GetProportionalScaledValue( int normalizedValue )
{
	int wide;
	int tall;
	int proW;
	int proH;

	g_pSurface->GetScreenSize( wide, tall );
	g_pSurface->GetProportionalBase( proW, proH );
	return (int)( (double)tall / (double)proH * normalizedValue );
}

int CSchemeManager::GetHDProportionalScaledValue( int normalizedValue )
{
	int wide;
	int tall;
	int proW;
	int proH;

	g_pSurface->GetScreenSize( wide, tall );
	g_pSurface->GetHDProportionalBase( proW, proH );
	return (int)( (double)tall / (double)proH * normalizedValue );
}

//-----------------------------------------------------------------------------
// Purpose: converts a value out of proportional mode
//-----------------------------------------------------------------------------
int CSchemeManager::GetProportionalNormalizedValue( int scaledValue )
{
	int wide;
	int tall;
	int proW;
	int proH;

	g_pSurface->GetScreenSize( wide, tall );
	g_pSurface->GetProportionalBase( proW, proH );
	return (int)( (float)scaledValue / ( (float)tall / (float)proH ) );
}

int CSchemeManager::GetHDProportionalNormalizedValue( int scaledValue )
{
	int wide;
	int tall;
	int proW;
	int proH;

	g_pSurface->GetScreenSize( wide, tall );
	g_pSurface->GetHDProportionalBase( proW, proH );
	return (int)( (float)scaledValue / ( (float)tall / (float)proH ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
HTexture CSchemeManager::GetImageID( const char* imageName, bool hardwareFiltered )
{
	Bitmap* pBitmap = (Bitmap*)GetImage( imageName, hardwareFiltered );
	return pBitmap->GetID();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char* CScheme::GetResourceString( const char* stringName )
{
	return baseSettings->GetString( stringName, "" );
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an existing border
//-----------------------------------------------------------------------------
IBorder* CScheme::GetBorder( const char* borderName )
{
	int symbol = g_pKeyValues->GetSymbolForString( borderName );

	for ( int i = 0; i < m_BorderList.Count(); ++i )
	{
		if ( m_BorderList[i].borderSymbol == symbol )
		{
			return (IBorder*)m_BorderList[i].border;
		}
	}

	return (IBorder*)_baseBorder;
}

//-----------------------------------------------------------------------------
// Purpose: Gets a color from the scheme file
//-----------------------------------------------------------------------------
Color CScheme::GetColor( const char* colorName, Color defaultColor )
{
	int r, g, b, a;
	const char* s;

	s = colors->GetString( colorName, NULL );
	if ( s )
	{
		sscanf( s, "%d %d %d %d", &r, &g, &b, &a );
		return Color( r, g, b, a );
	}

	s = baseSettings->GetString( colorName, NULL );
	if ( s )
	{
		if ( sscanf( s, "%d %d %d %d", &r, &g, &b, &a ) >= 3 )
		{
			return Color( r, g, b, a );
		}

		// must be a color name, lookup color
		char aliasName[64];
		strncpy( aliasName, s, sizeof( aliasName ) - 1 );
		aliasName[sizeof( aliasName ) - 1] = 0;
		return GetColor( s, defaultColor );
	}

	if ( sscanf( colorName, "%d %d %d %d", &r, &g, &b, &a ) > 2 )
	{
		return Color( r, g, b, a );
	}

	return defaultColor;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSchemeManager::CSchemeManager()
{
	// 0th element is null, since that would be an invalid handle
	CScheme* pNullScheme = new CScheme();

	_scheme.AddToTail( pNullScheme );
	m_Bitmaps.SetLessFunc( &CSchemeManager::BitmapHandleSearchFunc );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSchemeManager::~CSchemeManager()
{
	int i;

	for ( i = 0; i < m_Bitmaps.MaxElement(); ++i )
	{
		if ( m_Bitmaps.IsValidIndex( i ) && m_Bitmaps[i].bitmap )
		{
			delete m_Bitmaps[i].bitmap;
		}
	}

	m_Bitmaps.RemoveAll();

	for ( i = 1; i < _scheme.Count(); ++i )
	{
		_scheme[i]->Shutdown( false );
	}
}

CScheme::CScheme()
{
	fileName[0] = 0;
	tag[0] = 0;

	data = NULL;
	baseSettings = NULL;
	colors = NULL;

	_baseBorder = NULL;	// default border to use if others not found
	_borders = NULL;
}

CScheme::~CScheme()
{
}

//-----------------------------------------------------------------------------
// Purpose: kills all the schemes
//-----------------------------------------------------------------------------
void CScheme::Shutdown( bool full )
{
	int i;

	for ( i = 0; i < m_BorderList.Count(); ++i )
	{
		if ( !m_BorderList[i].bSharedBorder )
		{
			Border* pBorder = m_BorderList[i].border;

			if ( pBorder )
			{
				delete pBorder;
			}
		}
	}

	_baseBorder = NULL;
	m_BorderList.RemoveAll();
	_borders = NULL;

	if ( full && data )
	{
		data->deleteThis();
		data = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from disk
//-----------------------------------------------------------------------------
HScheme CSchemeManager::FindLoadedScheme( const char* fileName )
{
	int i;

	if ( _scheme.Count() <= 1 )
	{
		return 0;
	}

	// Find the scheme in the list of already loaded schemes
	for ( i = 1; i < _scheme.Count(); ++i )
	{
		if ( !strcasecmp( _scheme[i]->fileName, fileName ) )
		{
			return i;
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from disk
//-----------------------------------------------------------------------------
HScheme CSchemeManager::LoadSchemeFromFile( const char* fileName, const char* tag )
{
	// Look to see if we've already got this scheme...
	HScheme hScheme = FindLoadedScheme( fileName );
	KeyValues* pData;
	CScheme* pScheme;

	if ( hScheme )
	{
		return hScheme;
	}

	pData = new KeyValues( "Scheme" );

	if ( pData->LoadFromFile( g_pFileSystem, fileName, "SKIN" )
		|| pData->LoadFromFile( g_pFileSystem, fileName, NULL ) )
	{
		pScheme = new CScheme();
		pScheme->LoadFromFile( fileName, tag, pData );
		return _scheme.AddToTail( pScheme );
	}

	pData->deleteThis();
	return 0;
}

void CScheme::LoadFromFile( const char* inFilename, const char* inTag, KeyValues* inKeys )
{
	KeyValues* pName;
	float wideValue;
	float tallValue;

	strncpy( fileName, inFilename, sizeof( fileName ) );
	fileName[sizeof( fileName ) - 1] = 0;

	data = inKeys;
	baseSettings = data->FindKey( "BaseSettings", true );
	colors = data->FindKey( "Colors", true );

	pName = data->FindKey( "Name", true );
	pName->SetString( "Name", inTag );

	if ( inTag )
	{
		strncpy( tag, inTag, sizeof( tag ) );
	}
	else
	{
		strcpy( tag, "default" );
	}

	if ( baseSettings->FindKey( "ProportionalBaseWidth", false )
		&& baseSettings->FindKey( "ProportionalBaseHeight", false ) )
	{
		tallValue = baseSettings->GetFloat( "ProportionalBaseHeight", 480.0f );
		wideValue = baseSettings->GetFloat( "ProportionalBaseWidth", 640.0f );
		g_pSurface->SetProportionalBase( (int)wideValue, (int)tallValue );
	}

	if ( baseSettings->FindKey( "ProportionalBaseWidthHD", false )
		&& baseSettings->FindKey( "ProportionalBaseHeightHD", false ) )
	{
		tallValue = baseSettings->GetFloat( "ProportionalBaseHeightHD", 720.0f );
		wideValue = baseSettings->GetFloat( "ProportionalBaseWidthHD", 1280.0f );
		g_pSurface->SetHDProportionalBase( (int)wideValue, (int)tallValue );
	}

	// need to copy tag before loading fonts
	LoadFonts();
	LoadBorders();
}

//-----------------------------------------------------------------------------
// Purpose: returns a char string of the munged name this font is stored as in the font manager
//-----------------------------------------------------------------------------
const char* CScheme::GetMungedFontName( const char* fontName, const char* scheme, bool proportional,
		bool bHDProportional )
{
	static char mungeBuffer[64];
	const char* suffix = "hdp";

	if ( !bHDProportional )
	{
		suffix = "p";

		if ( !proportional )
		{
			suffix = "no";
		}
	}

	if ( scheme )
	{
		_snprintf( mungeBuffer, sizeof( mungeBuffer ), "%s%s-%s", fontName, scheme, suffix );
	}
	else
	{
		_snprintf( mungeBuffer, sizeof( mungeBuffer ), "%s-%s", fontName, suffix ); // we don't want the "(null)" snprintf appends
	}

	return mungeBuffer;
}

//-----------------------------------------------------------------------------
// Finds a font in the alias list
//-----------------------------------------------------------------------------
HFont CScheme::FindFontInAliasList( const char* fontName )
{
	int i;

	// FIXME: Slow!!!
	for ( i = _fontAliases.Count() - 1; i >= 0; --i )
	{
		if ( !strncasecmp( fontName, _fontAliases[i]._fontName, sizeof( _fontAliases[i]._fontName ) ) )
		{
			return _fontAliases[i]._font;
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an existing font, tall=0 means use default
//-----------------------------------------------------------------------------
HFont CScheme::GetFont( const char* fontName, bool proportional )
{
	// First look in the list of aliases...
	return FindFontInAliasList( GetMungedFontName( fontName, tag, proportional, false ) );
}

HFont CScheme::GetFontEx( const char* fontName, bool proportional, bool hd )
{
	return FindFontInAliasList( GetMungedFontName( fontName, tag, proportional, hd ) );
}

int CScheme::GetAdjustedFontHeightForCurrentLanguage( int tall )
{
	char language[64];
	const char* pchLang;
	int adjusted;

	language[0] = 0;
	pchLang = g_pSurface->GetLanguage();

	if ( pchLang && pchLang[0] )
	{
		strncpy( language, pchLang, sizeof( language ) );
	}

	if ( language[0] == 0
		&& !g_pSystem->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language",
				language, (int)sizeof( language ) - 1 ) )
	{
		return tall;
	}

	if ( ( !strcasecmp( language, "korean" )
			|| !strcasecmp( language, "tchinese" )
			|| !strcasecmp( language, "schinese" )
			|| !strcasecmp( language, "thai" ) )
		&& tall <= 12 )
	{
		return 13;
	}

	if ( strcasecmp( language, "russian" ) || tall <= 12 )
	{
		return tall;
	}

	adjusted = (int)( (double)tall * 0.9 );

	if ( adjusted < 12 )
	{
		return 12;
	}

	return adjusted;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CScheme::LoadFonts()
{
	KeyValues* pCustom;
	KeyValues* pFont;
	int screenWide;
	int screenTall;

	for ( pCustom = data->FindKey( "CustomFontFiles", true )->GetFirstSubKey(); pCustom != NULL;
			pCustom = pCustom->GetNextKey() )
	{
		const char* fontFile = pCustom->GetString();

		if ( fontFile && fontFile[0] )
		{
			g_pSurface->AddCustomFontFile( fontFile );
		}
	}

	g_pSurface->GetScreenSize( screenWide, screenTall );

	for ( pFont = data->FindKey( "Fonts", true )->GetFirstSubKey(); pFont != NULL; pFont = pFont->GetNextKey() )
	{
		int i;

		for ( i = 0; i < 3; ++i )
		{
			bool proportional = ( i == 1 );
			bool bHDProportional = ( i == 2 );
			const char* fontName = GetMungedFontName( pFont->GetName(), tag, proportional, bHDProportional ); // first time it adds a normal font, and then a proportional one
			HFont font = g_pSurface->CreateFont();
			KeyValues* pGlyph;

			for ( pGlyph = pFont->GetFirstSubKey(); pGlyph != NULL; pGlyph = pGlyph->GetNextKey() )
			{
				int fontYResMin = 0;
				int fontYResMax = 0;
				int flags = 0;
				int tall;
				int blur;
				int scanlines;
				int index;

				sscanf( pGlyph->GetString( "yres", "" ), "%d %d", &fontYResMin, &fontYResMax );
				if ( fontYResMin )
				{
					if ( !fontYResMax )
					{
						fontYResMax = fontYResMin;
					}

					// check the range
					if ( screenTall < fontYResMin || screenTall > fontYResMax )
					{
						continue;
					}
				}

				if ( pGlyph->GetInt( "italic" ) )
				{
					flags |= ISurface::FONTFLAG_ITALIC;
				}

				if ( pGlyph->GetInt( "underline" ) )
				{
					flags |= ISurface::FONTFLAG_UNDERLINE;
				}

				if ( pGlyph->GetInt( "strikeout" ) )
				{
					flags |= ISurface::FONTFLAG_STRIKEOUT;
				}

				if ( pGlyph->GetInt( "symbol" ) )
				{
					flags |= ISurface::FONTFLAG_SYMBOL;
				}

				if ( pGlyph->GetInt( "antialias" ) && g_pSurface->SupportsFeature( ISurface::ANTIALIASED_FONTS ) )
				{
					flags |= ISurface::FONTFLAG_ANTIALIAS;
				}

				if ( pGlyph->GetInt( "dropshadow" ) && g_pSurface->SupportsFeature( ISurface::DROPSHADOW_FONTS ) )
				{
					flags |= ISurface::FONTFLAG_DROPSHADOW;
				}

				if ( pGlyph->GetInt( "rotary" ) )
				{
					flags |= ISurface::FONTFLAG_ROTARY;
				}

				if ( pGlyph->GetInt( "additive" ) )
				{
					flags |= ISurface::FONTFLAG_ADDITIVE;
				}

				tall = pGlyph->GetInt( "tall" );
				blur = pGlyph->GetInt( "blur" );
				scanlines = pGlyph->GetInt( "scanlines" );

				if ( !fontYResMin && !fontYResMax && i != 0 )
				{
					if ( i == 2 )
					{
						tall = g_Scheme.GetHDProportionalScaledValue( tall );
						blur = g_Scheme.GetHDProportionalScaledValue( blur );
						scanlines = g_Scheme.GetHDProportionalScaledValue( scanlines );
					}
					else
					{
						tall = g_Scheme.GetProportionalScaledValue( tall );
						blur = g_Scheme.GetProportionalScaledValue( blur );
						scanlines = g_Scheme.GetProportionalScaledValue( scanlines );
					}
				}

				// clip the font size so that fonts can't be too big
				if ( tall > 128 )
				{
					tall = 128;
				}

				if ( ( flags & ISurface::FONTFLAG_SYMBOL ) == 0 )
				{
					tall = GetAdjustedFontHeightForCurrentLanguage( tall );
				}

				g_pSurface->AddGlyphSetToFont(
					font,
					pGlyph->GetString( "name", "" ),
					tall,
					pGlyph->GetInt( "weight" ),
					blur,
					scanlines,
					flags,
					0,
					0xFFFF );

				index = _fontAliases.AddToTail();
				strncpy( _fontAliases[index]._fontName, fontName, sizeof( _fontAliases[index]._fontName ) );
				_fontAliases[index]._font = font;
			}
		}
	}
}

void CScheme::LoadBorders()
{
	KeyValues* kv;

	_borders = data->FindKey( "Borders", true );

	for ( kv = _borders->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey() )
	{
		Border* pBorder;
		int index;

		if ( kv->GetDataType() == KeyValues::TYPE_STRING )
		{
			continue;
		}

		index = m_BorderList.AddToTail();

		pBorder = new Border();
		pBorder->SetName( kv->GetName() );
		pBorder->ApplySchemeSettings( this, kv );

		m_BorderList[index].border = pBorder;
		m_BorderList[index].bSharedBorder = false;
		m_BorderList[index].borderSymbol = kv->GetNameSymbol();
	}

	for ( kv = _borders->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey() )
	{
		Border* border;
		int index;

		if ( kv->GetDataType() != KeyValues::TYPE_STRING )
		{
			continue;
		}

		border = (Border*)GetBorder( kv->GetString() );
		(void)kv->GetName();
		ASSERT_AT( border, 498 );

		index = m_BorderList.AddToTail();
		m_BorderList[index].border = border;
		m_BorderList[index].bSharedBorder = true;
		m_BorderList[index].borderSymbol = kv->GetNameSymbol();
	}

	_baseBorder = (Border*)GetBorder( "BaseBorder" );
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an image
//-----------------------------------------------------------------------------
IImage* CSchemeManager::GetImage( const char* imageName, bool hardwareFiltered )
{
	// set up to search for the bitmap
	CachedBitmapHandle_t searchBitmap;
	int i;

	s_pszSearchString = imageName;
	searchBitmap.bitmap = NULL;

	i = m_Bitmaps.Find( searchBitmap );

	if ( m_Bitmaps.IsValidIndex( i ) )
	{
		return (IImage*)m_Bitmaps[i].bitmap;
	}

	CachedBitmapHandle_t bitmap;

	bitmap.bitmap = new Bitmap( imageName, hardwareFiltered );
	m_Bitmaps.Insert( bitmap );
	return (IImage*)bitmap.bitmap;
}
