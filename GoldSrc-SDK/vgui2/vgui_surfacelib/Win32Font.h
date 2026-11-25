#pragma once

#include "utlrbtree.h"

struct HDC__;

class CWin32Font
{
public:
	struct abc_t
	{
		signed char a;
		signed char b;
		signed char c;
		char reserved;
	};

	struct abc_cache_t
	{
		unsigned short wch;
		abc_t abc;
	};

	CWin32Font();
	~CWin32Font();

	bool Create( const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags );
	void GetCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba );
	bool IsEqualTo( const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags );
	bool IsValid();

	int GetHeight();
	int GetAscent();
	int GetMaxCharWidth();
	int GetFlags();

	int GetBlur() { return m_iBlur; }
	bool GetUnderlined() { return m_bUnderlined; }
	bool IsAdditive() { return m_bAdditive; }
	const char* GetName() { return m_szName; }
	int GetWeight() { return m_iWeight; }

	void SetAsActiveFont( HDC__* hdc );
	void GetCharABCWidths( int ch, int& a, int& b, int& c );

private:
	static bool ExtendedABCWidthsCacheLessFunc( const abc_cache_t& lhs, const abc_cache_t& rhs );

	bool CreateWindowsFont( const char* name, int tall, int weight, int flags );
	void RenderWindowsGlyph( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba );

	char m_szName[32];
	int m_iTall;
	int m_iWeight;
	int m_iFlags;

	bool m_bAntiAliased;
	bool m_bRotary;
	int m_iDropShadowOffset;
	bool m_bUnderlined;

	int m_iHeight;
	int m_iMaxCharWidth;
	int m_iAscent;

	static constexpr int ASCII_CHAR_COUNT = 128;
	abc_t m_ABCWidths[ASCII_CHAR_COUNT];
	CUtlRBTree<abc_cache_t, unsigned short> m_ExtendedABCWidthsCache;

	void* m_hFont;
	void* m_hDC;
	void* m_hDIB;
	int m_rgiBitmapSize[2];
	unsigned char* m_pBuf;
	bool m_bAdditive;

	int m_iScanLines;
	int m_iBlur;
	float* m_pGaussianDistribution;
};
