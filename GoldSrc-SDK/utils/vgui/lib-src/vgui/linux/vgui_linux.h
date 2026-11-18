#ifndef VGUI_LINUX_H
#define VGUI_LINUX_H

#include "VGUI.h"
#include "utlrbtree.h"
#include "vfontdata.h"
#include "fileimage.h"

typedef unsigned char uchar;
typedef unsigned char uint8;

namespace vgui
{

class SurfacePlat
{
public:
	int		bitmapSize[2];

	int		restoreInfo[4];
	bool	isFullscreen;
	int		fullscreenInfo[3];
};

class BaseFontPlat
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	BaseFontPlat() : references( 0 )
	{
	}
#endif

	virtual ~BaseFontPlat()
	{
	}

	virtual bool equals( const char* name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol ) = 0;
	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba ) = 0;
	virtual void getCharABCwide( int ch, int& a, int& b, int& c ) = 0;
	virtual int getTall() = 0;
	virtual int getWide() = 0;
	virtual void drawSetTextFont( SurfacePlat* plat ) = 0;

#if VGUI_ABI_COMPAT_MODE != 1
	// The platform list and each Font using this platform hold one.
	int		references;
#endif
};

class FontPlat : public BaseFontPlat
{
public:
	struct abc_t
	{
		short	b;
		char	a;
		char	c;
	};

	struct abc_cache_t
	{
		wchar_t	wch;
		abc_t	abc;
	};

	struct font_name_entry
	{
		char*	m_OSSpecificName;
		uint8	m_cbOSSpecificName;
		char*	m_pchFriendlyName;
	};

	FontPlat( const char* name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol );
	virtual ~FontPlat() override;

	virtual bool equals( const char* name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol ) override;
	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba ) override;
	virtual void getCharABCwide( int ch, int& a, int& b, int& c ) override;
	virtual int getTall() override;
	virtual int getWide() override;
	virtual void drawSetTextFont( SurfacePlat* plat ) override;

	static void CreateFontList();
	static bool ExtendedABCWidthsCacheLessFunc( const abc_cache_t& lhs, const abc_cache_t& rhs );
	static bool FontLessFunc( font_name_entry& lhs, font_name_entry& rhs );

private:
	int			bufSize[2];
	uchar*		buf;
	VFontData	m_BitmapFont;
	bool		m_bBitmapFont;

	char		m_szName[32];
	int			m_iTall;
	int			m_iWeight;
	int			m_iFlags;
	bool		m_bAntiAliased;
	bool		m_bRotary;
	bool		m_bAdditive;
	int			m_iDropShadowOffset;
	bool		m_bUnderlined;
	int			m_iOutlineSize;
	int			m_iHeight;
	int			m_iMaxCharWidth;
	int			m_iAscent;

	CUtlRBTree<abc_cache_t, unsigned short>	m_ExtendedABCWidthsCache;
	int											m_iScanLines;
	int											m_iBlur;
	float*										m_pGaussianDistribution;

	static CUtlRBTree<font_name_entry, int>	m_FriendlyNameCache;
	static bool									ms_bSetFriendlyNameCacheLessFunc;
};

class FontPlat_Bitmap : public BaseFontPlat
{
public:
	FontPlat_Bitmap();
	static FontPlat_Bitmap* Create( const char* name, FileImageStream* pStream );
	virtual ~FontPlat_Bitmap() override;

	virtual bool equals( const char* name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol ) override;
	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba ) override;
	virtual void getCharABCwide( int ch, int& a, int& b, int& c ) override;
	virtual int getTall() override;
	virtual int getWide() override;
	virtual void drawSetTextFont( SurfacePlat* plat ) override;

private:
	VFontData	m_FontData;
	char*		m_pName;
};

}

#endif
