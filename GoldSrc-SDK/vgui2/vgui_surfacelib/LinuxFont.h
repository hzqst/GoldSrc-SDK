#pragma once

#include "utlrbtree.h"

struct FT_FaceRec_;
typedef FT_FaceRec_* FT_Face;

struct __GLXcontextRec;
typedef __GLXcontextRec* GLXContext;

class CLinuxFont
{
public:
	struct abc_t
	{
		short b;
		char a;
		char c;
	};

	struct abc_cache_t
	{
		int wch;
		abc_t abc;
	};

	struct font_name_entry
	{
		char* m_pchFile;
		char* m_pchFriendlyName;
	};

	CLinuxFont();
	~CLinuxFont();

	bool FindFontDataFile( const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags,
		char* dataFile, int dataFileChars );

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

	void* SetAsActiveFont( GLXContext cglContext );
	bool HasChar( wchar_t wch );
	void GetCharABCWidths( int ch, int& a, int& b, int& c );

private:
	static bool ExtendedABCWidthsCacheLessFunc( const abc_cache_t& lhs, const abc_cache_t& rhs );
	static bool FontEntryLess( const font_name_entry& a, const font_name_entry& b );

	char m_szName[32];
	int m_iTall;
	int m_iWeight;
	int m_iFlags;

	bool m_bAntiAliased;
	bool m_bRotary;
	bool m_bAdditive;
	int m_iDropShadowOffset;
	bool m_bUnderlined;
	int m_iOutlineSize;

	int m_iHeight;
	int m_iMaxCharWidth;
	int m_iAscent;

	CUtlRBTree<abc_cache_t, unsigned short> m_ExtendedABCWidthsCache;

	int m_iScanLines;
	int m_iBlur;
	float* m_pGaussianDistribution;

	FT_Face face;
	int m_iFxpHeight;

	static CUtlRBTree<font_name_entry> m_FriendlyNameCache;
	static bool ms_bSetFriendlyNameCacheLessFunc;
};
