//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "utlrbtree.h"
#include "utlvector.h"
#include "vgui/IScheme.h"

//-----------------------------------------------------------------------------
// Purpose: manages texture memory for unicode fonts in vgui
//-----------------------------------------------------------------------------
class CFontTextureCache
{
public:
	CFontTextureCache();
	~CFontTextureCache();

	bool GetTextureForChar( vgui2::HFont font, wchar_t ch, int* textureID, float* texCoords );

private:
	// a single character in the cache
	typedef unsigned short HCacheEntry;

	struct cacheentry_t
	{
		vgui2::HFont	font;
		wchar_t			wch;
		short			page;
		float			texCoords[4];

		HCacheEntry		nextEntry; // doubly-linked list for use in the LRU
		HCacheEntry		prevEntry;
	};

	struct page_t
	{
		short	textureID;
		short	fontHeight;
		short	wide;
		short	tall;
		short	nextX;
		short	nextY;
	};

	static bool CacheEntryLessFunc( const cacheentry_t& lhs, const cacheentry_t& rhs );

	// allocates a new page for a given character
	bool AllocatePageForChar( int charWide, int charTall, int& pageIndex, int& drawX, int& drawY, int& textureWide,
		int& textureTall );

	CUtlRBTree<cacheentry_t, HCacheEntry>	m_CharCache;
	CUtlVector<page_t>						m_PageList;
	HCacheEntry								m_LRUListHeadIndex;
};

extern CFontTextureCache g_FontTextureCache;
