#ifndef SCHEME_H
#define SCHEME_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui/IScheme.h"
#include "utlvector.h"
#include "utlrbtree.h"

#include <Color.h>

class KeyValues;

namespace vgui2
{

typedef unsigned long HScheme;
typedef unsigned long HFont;
typedef unsigned long HTexture;

class IBorder;
class IImage;
class Bitmap;
class Border;

}

class CScheme : public vgui2::IScheme
{
public:
	CScheme();
	~CScheme() override;

	const char* GetResourceString( const char* stringName ) override;
	vgui2::IBorder* GetBorder( const char* borderName ) override;
	vgui2::HFont GetFont( const char* fontName, bool proportional ) override;
	Color GetColor( const char* colorName, Color defaultColor ) override;
	vgui2::HFont GetFontEx( const char* fontName, bool proportional, bool hd ) override;

private:
	struct SchemeBorder_t
	{
		vgui2::Border*	border;
		int				borderSymbol;
		bool			bSharedBorder;
	};

	struct fontalias_t
	{
		char			_fontName[64];
		vgui2::HFont	_font;
	};

	char	fileName[256];
	char	tag[64];

	KeyValues*	data;
	KeyValues*	baseSettings;
	KeyValues*	colors;

	CUtlVector<SchemeBorder_t>	m_BorderList;
	vgui2::Border*				_baseBorder;
	KeyValues*					_borders;
	CUtlVector<fontalias_t>		_fontAliases;
	// borders taken out of m_BorderList by a reload, kept alive because panels may still point at them
	CUtlVector<vgui2::Border*>	m_RetiredBorders;

	void Shutdown( bool full );
	void LoadFromFile( const char* inFilename, const char* inTag, KeyValues* inKeys );
	void Reload( KeyValues* inKeys );
	void LoadFonts();
	void LoadBorders();
	vgui2::Border* TakeRetiredBorder( const char* borderName );
	vgui2::HFont FindFontInAliasList( const char* fontName );
	const char* GetMungedFontName( const char* fontName, const char* scheme, bool proportional, bool bHDProportional );
	int GetAdjustedFontHeightForCurrentLanguage( int tall );

	friend class CSchemeManager;
};

class CSchemeManager : public vgui2::ISchemeManager
{
public:
	CSchemeManager();
	~CSchemeManager() override;

	vgui2::HScheme LoadSchemeFromFile( const char* fileName, const char* tag ) override;
	void ReloadSchemes() override;
	vgui2::HScheme GetDefaultScheme() override;
	vgui2::HScheme GetScheme( const char* tag ) override;

	vgui2::IImage* GetImage( const char* imageName, bool hardwareFiltered ) override;
	vgui2::HTexture GetImageID( const char* imageName, bool hardwareFiltered ) override;
	vgui2::IScheme* GetIScheme( vgui2::HScheme scheme ) override;
	void Shutdown( bool full ) override;

	int GetProportionalScaledValue( int normalizedValue ) override;
	int GetProportionalNormalizedValue( int scaledValue ) override;
	float GetProportionalScale() override;
	int GetHDProportionalScaledValue( int normalizedValue ) override;
	int GetHDProportionalNormalizedValue( int scaledValue ) override;

	struct CachedBitmapHandle_t
	{
		vgui2::Bitmap*	bitmap;
		bool			hardwareFiltered;
	};

	static bool BitmapHandleSearchFunc( const CachedBitmapHandle_t& lhs, const CachedBitmapHandle_t& rhs );

private:
	vgui2::HScheme FindLoadedScheme( const char* fileName );

	CUtlVector<CScheme*>					_scheme;
	CUtlRBTree<CachedBitmapHandle_t, int>	m_Bitmaps;

	static const char*	s_pszSearchString;
};

#endif
