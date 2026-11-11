//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#define UTLRBTREE_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\UtlRBTree.h"

#include "../../vgui2/vgui_surfacelib/FontTextureCache.h"
#include "../../vgui2/vgui_surfacelib/FontManager.h"
#include "../../vgui2/vgui_surfacelib/LinuxFont.h"
#include "BaseUISurface.h"

#include <cstring>
#include <malloc.h>
#include <vector>

extern BaseUISurface* staticSurface;

namespace
{
constexpr int FONT_TEXTURE_PAGE_SIZE = 256;
}

//-----------------------------------------------------------------------------
// Purpose: comparison function for cache entries
//-----------------------------------------------------------------------------
bool CFontTextureCache::CacheEntryLessFunc( const cacheentry_t& lhs, const cacheentry_t& rhs )
{
	if ( lhs.font < rhs.font )
		return true;
	if ( lhs.font > rhs.font )
		return false;
	return lhs.wch < rhs.wch;
}

CFontTextureCache::CFontTextureCache() : m_CharCache( 0, 256, CacheEntryLessFunc )
{
	cacheentry_t listHead = {};
	m_LRUListHeadIndex = m_CharCache.Insert( listHead );
	m_CharCache.Element( m_LRUListHeadIndex ).nextEntry = m_LRUListHeadIndex;
	m_CharCache.Element( m_LRUListHeadIndex ).prevEntry = m_LRUListHeadIndex;
}

CFontTextureCache::~CFontTextureCache()
{
}

#ifdef _MSC_VER
#pragma warning( suppress : 6262 )
#endif
bool CFontTextureCache::AllocatePageForChar(
	int charWide, int charTall, int& pageIndex, int& drawX, int& drawY, int& textureWide, int& textureTall )
{
	int page = m_PageList.Count() - 1;
	pageIndex = page;

	int fontHeight;
	if ( charTall <= 16 )
	{
		fontHeight = 16;
	}
	else
	{
		int shift = 4;
		do
		{
			fontHeight = static_cast<int>( 1u << shift );
			++shift;
		} while ( charTall > fontHeight && shift <= 31 );

		if ( fontHeight > FONT_TEXTURE_PAGE_SIZE )
			return false;
	}

	bool newPage = page < 0 || m_PageList[page].fontHeight != fontHeight;
	if ( !newPage )
	{
		const page_t& current = m_PageList[page];
		if ( current.nextY + fontHeight >= current.tall && current.nextX + charWide >= current.wide )
			newPage = true;
	}

	if ( newPage )
	{
		page = m_PageList.AddToTail();
		pageIndex = page;

		page_t& allocated = m_PageList[page];
		allocated.textureID = static_cast<short>( staticSurface->CreateNewTextureID( false ) );
		page_t& created = m_PageList[pageIndex];
		created.fontHeight = static_cast<short>( fontHeight );
		created.wide = static_cast<short>( FONT_TEXTURE_PAGE_SIZE );
		created.tall = static_cast<short>( FONT_TEXTURE_PAGE_SIZE );
		created.nextX = 0;
		created.nextY = 0;

		// upload a purely black page (only need to do this once since they share textures)
		unsigned char rgba[FONT_TEXTURE_PAGE_SIZE * FONT_TEXTURE_PAGE_SIZE * 4];
		memset( rgba, 0, sizeof( rgba ) );
		staticSurface->DrawSetTextureRGBAWithAlphaChannel(
			created.textureID, rgba, created.wide, created.tall, 0, true );
	}

	page_t& slot = m_PageList[pageIndex];
	if ( slot.nextX + charWide >= slot.tall )
	{
		slot.nextX = 0;
		slot.nextY = static_cast<short>( slot.nextY + slot.fontHeight );
	}

	drawX = slot.nextX;
	drawY = slot.nextY;
	textureWide = slot.wide;
	textureTall = slot.tall;
	slot.nextX = static_cast<short>( slot.nextX + charWide );
	return true;
}

bool CFontTextureCache::GetTextureForChar( vgui2::HFont font, wchar_t ch, int* textureID, float* texCoords )
{
	cacheentry_t cacheItem = {};
	cacheItem.font = font;
	cacheItem.wch = ch;

	const HCacheEntry entry = m_CharCache.Find( cacheItem );
	if ( !m_CharCache.IsValidIndex( entry ) )
	{
		CLinuxFont* fontForChar = FontManager().GetFontForChar( font, ch );
		if ( fontForChar == nullptr )
			return false;

		const int fontTall = fontForChar->GetHeight();
		int a = 0;
		int b = 0;
		int c = 0;
		fontForChar->GetCharABCWidths( ch, a, b, c );
		int fontWide = b;
		if ( fontForChar->GetUnderlined() )
			fontWide = a + c + b;

		int page;
		int drawX;
		int drawY;
		int textureWide;
		int textureTall;
		if ( !AllocatePageForChar( fontWide, fontTall, page, drawX, drawY, textureWide, textureTall ) )
			return false;

		const int rgbaSize = fontWide * 4 * fontTall;
		unsigned char* rgba = static_cast<unsigned char*>( _alloca( rgbaSize ) );
		memset( rgba, 0, rgbaSize );
		fontForChar->GetCharRGBA( ch, 0, 0, fontWide, fontTall, rgba );

		staticSurface->DrawSetTexture( m_PageList[page].textureID );
		staticSurface->DrawSetSubTextureRGBA( m_PageList[page].textureID, drawX, drawY, rgba, fontWide, fontTall );

		cacheItem.page = static_cast<short>( page );
		cacheItem.texCoords[0] = static_cast<float>( drawX ) / static_cast<float>( textureWide );
		cacheItem.texCoords[1] = static_cast<float>( drawY ) / static_cast<float>( textureTall );
		cacheItem.texCoords[2] = static_cast<float>( fontWide + drawX ) / static_cast<float>( textureWide );
		cacheItem.texCoords[3] = static_cast<float>( fontTall + drawY ) / static_cast<float>( textureTall );
		m_CharCache.Insert( cacheItem );

		*textureID = m_PageList[page].textureID;
		texCoords[0] = cacheItem.texCoords[0];
		texCoords[1] = cacheItem.texCoords[1];
		texCoords[2] = cacheItem.texCoords[2];
		texCoords[3] = cacheItem.texCoords[3];
		return true;
	}

	*textureID = m_PageList[m_CharCache[entry].page].textureID;
	memcpy( texCoords, m_CharCache[entry].texCoords, sizeof( m_CharCache[entry].texCoords ) );
	return true;
}

CFontTextureCache g_FontTextureCache;
