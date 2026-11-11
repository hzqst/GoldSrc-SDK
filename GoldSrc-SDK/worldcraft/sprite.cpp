//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "Sprite.h"
#include "Texture.h"
#include "const.h"
#include <gl/GL.h>
#include "Render3d.h"

SpriteCache_t CSpriteCache::m_Cache[SPRITE_CACHE_SIZE];
int			  CSpriteCache::m_nItems = 0;

//-----------------------------------------------------------------------------
// Purpose: Returns an instance of a particular studio model. If the model is
//			in the cache, a pointer to that model is returned. If not, a new one
//			is created and added to the cache.
// Input  : pszModelPath - Full path of the .MDL file.
//-----------------------------------------------------------------------------
CSpriteModel* CSpriteCache::CreateSprite( const char* pszSpritePath )
{
	//
	// First look for the sprite in the cache. If it's there, increment the
	// reference count and return a pointer to the cached sprite.
	//
	for ( int i = 0; i < m_nItems; i++ )
	{
		if ( !stricmp( pszSpritePath, m_Cache[i].pszPath ) )
		{
			m_Cache[i].nRefCount++;
			return ( m_Cache[i].pSprite );
		}
	}

	//
	// If it isn't there, try to create one.
	//
	CSpriteModel* pSprite = new CSpriteModel;

	if ( pSprite != NULL )
	{
		if ( !pSprite->LoadSprite( pszSpritePath ) )
		{
			delete pSprite;
			pSprite = NULL;
		}
	}

	//
	// If we successfully created it, add it to the cache.
	//
	if ( pSprite != NULL )
	{
		if ( !CSpriteCache::AddSprite( pSprite, pszSpritePath ) )
		{
			delete pSprite;
			pSprite = NULL;
		}
	}

	return ( pSprite );
}

//-----------------------------------------------------------------------------
// Purpose: Adds the model to the cache, setting the reference count to one.
// Input  : pModel - Model to add to the cache.
//			pszSpritePath - The full path of the .MDL file, which is used as a
//				key in the sprite cache.
// Output : Returns TRUE if the sprite was successfully added, FALSE if we ran
//			out of memory trying to add the sprite to the cache.
//-----------------------------------------------------------------------------
bool CSpriteCache::AddSprite( CSpriteModel* pSprite, const char* pszSpritePath )
{
	if ( m_nItems == SPRITE_CACHE_SIZE )
		return false;

	//
	// Copy the sprite pointer.
	//
	m_Cache[m_nItems].pSprite = pSprite;

	//
	// Allocate space for and copy the model path.
	//
	m_Cache[m_nItems].pszPath = new char[strlen( pszSpritePath ) + 1];
	if ( m_Cache[m_nItems].pszPath != NULL )
		strcpy( m_Cache[m_nItems].pszPath, pszSpritePath );
	else
		return ( false );

	m_Cache[m_nItems].nRefCount = 1;

	m_nItems++;

	return ( true );
}

//-----------------------------------------------------------------------------
// Purpose: Increments the reference count on a sprite in the cache. Called by
//			client code when a pointer to the sprite is copied, making that
//			reference independent.
// Input  : pModel - Sprite for which to increment the reference count.
//-----------------------------------------------------------------------------
void CSpriteCache::AddRef( CSpriteModel* pSprite )
{
	for ( int i = 0; i < m_nItems; i++ )
	{
		if ( m_Cache[i].pSprite == pSprite )
		{
			m_Cache[i].nRefCount++;
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called by client code to release an instance of a model. If the
//			model's reference count is zero, the model is freed.
// Input  : pModel - Pointer to the model to release.
//-----------------------------------------------------------------------------
void CSpriteCache::Release( CSpriteModel* pSprite )
{
	for ( int i = 0; i < m_nItems; i++ )
	{
		if ( m_Cache[i].pSprite == pSprite )
		{
			m_Cache[i].nRefCount--;
			ASSERT( m_Cache[i].nRefCount >= 0 );

			//
			// If this model is no longer referenced, free it and remove it
			// from the cache.
			//
			if ( m_Cache[i].nRefCount <= 0 )
			{
				//
				// Free the path, which was allocated by AddModel.
				//
				delete[] m_Cache[i].pszPath;
				delete m_Cache[i].pSprite;

				//
				// Decrement the item count and copy the last element in the cache over
				// this element.
				//
				m_nItems--;

				m_Cache[i].pSprite = m_Cache[m_nItems].pSprite;
				m_Cache[i].pszPath = m_Cache[m_nItems].pszPath;
				m_Cache[i].nRefCount = m_Cache[m_nItems].nRefCount;
			}

			break;
		}
	}
}

CSpriteModel::CSpriteModel()
	: m_RenderMode( kRenderNormal ), m_NumFrames( 0 ), m_Type( SPR_VP_PARALLEL_UPRIGHT ), m_Width( 0 ), m_Height( 0 ),
	  m_Up( 0 ), m_Down( 0 ), m_Left( 0 ), m_Right( 0 )
{
}

CSpriteModel::~CSpriteModel()
{
	for ( size_t i = 0; i < m_Textures.size(); i++ )
		for ( size_t j = 0; j < m_Textures[i].size(); j++ )
			delete m_Textures[i][j];
}

void CSpriteModel::SetRenderMode( int mode )
{
	m_RenderMode = mode;
}

void CSpriteModel::Bind( CRender3D* pRender, int frame )
{
	if ( m_Textures.empty() )
		return;
	if ( frame < 0 || frame >= m_NumFrames )
		frame = 0;
	const SpriteFrame_t& group = m_File.frames[frame];
	size_t				 image = 0;
	if ( !group.intervals.empty() )
	{
		float time = (float)fmod( GetTickCount() * 0.001, group.intervals.back() );
		while ( image + 1 < group.intervals.size() && time >= group.intervals[image] )
			image++;
	}
	const SpriteImage_t& info = group.images[image];
	m_Left = (float)info.left;
	m_Up = (float)info.up;
	m_Right = m_Left + info.width;
	m_Down = m_Up - info.height;
	pRender->BindTexture( m_Textures[frame][image] );
	glEnable( GL_TEXTURE_2D );
	glDisable( GL_CULL_FACE );
	if ( m_File.header.texFormat == SPR_ALPHTEST )
	{
		glEnable( GL_ALPHA_TEST );
		glAlphaFunc( GL_GREATER, 0.5f );
	}
	if ( m_File.header.texFormat == SPR_ADDITIVE || m_RenderMode == kRenderTransAdd || m_RenderMode == kRenderGlow )
	{
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE );
		glDepthMask( GL_FALSE );
	}
	else if ( m_File.header.texFormat == SPR_INDEXALPHA || m_RenderMode != kRenderNormal )
	{
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
}

bool CSpriteModel::LoadSprite( const char* pszSpritePath )
{
	if ( !m_Textures.empty() || !m_File.Load( pszSpritePath ) )
		return false;
	m_Width = m_File.header.width;
	m_Height = m_File.header.height;
	m_Type = m_File.header.type;
	m_NumFrames = m_File.header.numframes;
	m_Textures.resize( m_File.frames.size() );
	for ( size_t i = 0; i < m_File.frames.size(); i++ )
		for ( size_t j = 0; j < m_File.frames[i].images.size(); j++ )
		{
			const SpriteImage_t& image = m_File.frames[i].images[j];
			CTexture*			 pTexture = new CTexture;
			m_Textures[i].push_back( pTexture );
			if ( !pTexture->Allocate( image.width, image.height, TEXTURE_HAS_ALPHA ) )
				return false;
			memcpy( pTexture->GetImageDataPtr(), image.pixels.data(), image.pixels.size() );
		}
	const SpriteImage_t& first = m_File.frames[0].images[0];
	m_Left = (float)first.left;
	m_Up = (float)first.up;
	m_Right = m_Left + first.width;
	m_Down = m_Up - first.height;
	return true;
}

int CSpriteModel::GetFrameCount()
{
	return m_NumFrames;
}
