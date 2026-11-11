//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "StudioModel.h"
#include "Render3D.h"
#include "Texture.h"
#include <gl/GL.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string>

vec3_t g_vright = { 0, 1, 0 };
float  g_lambert = 1.5f;

//-----------------------------------------------------------------------------
// Model meshes themselves are cached to avoid redundancy. There should never be
// more than one copy of a given studio model in memory at once.
//-----------------------------------------------------------------------------
ModelCache_t CStudioModelCache::m_Cache[1024];
int			 CStudioModelCache::m_nItems = 0;

//-----------------------------------------------------------------------------
// Purpose: Returns an instance of a particular studio model. If the model is
//			in the cache, a pointer to that model is returned. If not, a new one
//			is created and added to the cache.
// Input  : pszModelPath - Full path of the .MDL file.
//-----------------------------------------------------------------------------
StudioModel* CStudioModelCache::CreateModel( const char* pszModelPath )
{
	//
	// First look for the model in the cache. If it's there, increment the
	// reference count and return a pointer to the cached model.
	//
	for ( int i = 0; i < m_nItems; i++ )
	{
		if ( !stricmp( pszModelPath, m_Cache[i].pszPath ) )
		{
			m_Cache[i].nRefCount++;
			return ( m_Cache[i].pModel );
		}
	}

	//
	// If it isn't there, try to create one.
	//
	StudioModel* pModel = new StudioModel;

	if ( pModel != NULL )
	{
		bool bLoaded = pModel->LoadModel( pszModelPath );

		if ( bLoaded )
			bLoaded = pModel->PostLoadModel( pszModelPath );

		if ( !bLoaded )
		{
			delete pModel;
			pModel = NULL;
		}
	}

	//
	// If we successfully created it, add it to the cache.
	//
	if ( pModel != NULL )
	{
		if ( !CStudioModelCache::AddModel( pModel, pszModelPath ) )
		{
			delete pModel;
			pModel = NULL;
		}
	}

	return ( pModel );
}

//-----------------------------------------------------------------------------
// Purpose: Adds the model to the cache, setting the reference count to one.
// Input  : pModel - Model to add to the cache.
//			pszModelPath - The full path of the .MDL file, which is used as a
//				key in the model cache.
// Output : Returns TRUE if the model was successfully added, FALSE if we ran
//			out of memory trying to add the model to the cache.
//-----------------------------------------------------------------------------
BOOL CStudioModelCache::AddModel( StudioModel* pModel, const char* pszModelPath )
{
	if ( m_nItems == sizeof( m_Cache ) / sizeof( m_Cache[0] ) )
		return FALSE;
	//
	// Copy the model pointer.
	//
	m_Cache[m_nItems].pModel = pModel;

	//
	// Allocate space for and copy the model path.
	//
	m_Cache[m_nItems].pszPath = new char[strlen( pszModelPath ) + 1];
	if ( m_Cache[m_nItems].pszPath != NULL )
		strcpy( m_Cache[m_nItems].pszPath, pszModelPath );
	else
		return ( FALSE );

	m_Cache[m_nItems].nRefCount = 1;

	m_nItems++;

	return ( TRUE );
}

//-----------------------------------------------------------------------------
// Purpose: Advances the animation of all models in the cache for the given interval.
// Input  : flInterval - delta time in seconds.
//-----------------------------------------------------------------------------
void CStudioModelCache::AdvanceAnimation( float flInterval )
{
	for ( int i = 0; i < m_nItems; i++ )
		m_Cache[i].pModel->AdvanceFrame( flInterval );
}

//-----------------------------------------------------------------------------
// Purpose: Increments the reference count on a model in the cache. Called by
//			client code when a pointer to the model is copied, making that
//			reference independent.
// Input  : pModel - Model for which to increment the reference count.
//-----------------------------------------------------------------------------
void CStudioModelCache::AddRef( StudioModel* pModel )
{
	for ( int i = 0; i < m_nItems; i++ )
	{
		if ( m_Cache[i].pModel == pModel )
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
void CStudioModelCache::Release( StudioModel* pModel )
{
	for ( int i = 0; i < m_nItems; i++ )
	{
		if ( m_Cache[i].pModel == pModel )
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
				delete m_Cache[i].pModel;

				//
				// Decrement the item count and copy the last element in the cache over
				// this element.
				//
				m_nItems--;

				m_Cache[i].pModel = m_Cache[m_nItems].pModel;
				m_Cache[i].pszPath = m_Cache[m_nItems].pszPath;
				m_Cache[i].nRefCount = m_Cache[m_nItems].nRefCount;
			}

			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Rotates the given bounding box by the given angles and computes the
//			bounds of the rotated box. This is used to take the rotation angles
//			into consideration when returning the bounding box. Note that this
//			can produce a larger than optimal bounding box.
// Input  : Mins - 
//			Maxs - 
//			Angles - 
//-----------------------------------------------------------------------------
void StudioModel::RotateBbox( Vector& Mins, Vector& Maxs, const QAngle& Angles )
{
	Vector Points[8];

	//
	// Generate the corner points of the box.
	//
	Points[0][0] = Mins[0];
	Points[0][1] = Mins[1];
	Points[0][2] = Mins[2];

	Points[1][0] = Mins[0];
	Points[1][1] = Mins[1];
	Points[1][2] = Maxs[2];

	Points[2][0] = Mins[0];
	Points[2][1] = Maxs[1];
	Points[2][2] = Mins[2];

	Points[3][0] = Mins[0];
	Points[3][1] = Maxs[1];
	Points[3][2] = Maxs[2];

	Points[4][0] = Maxs[0];
	Points[4][1] = Mins[1];
	Points[4][2] = Mins[2];

	Points[5][0] = Maxs[0];
	Points[5][1] = Mins[1];
	Points[5][2] = Maxs[2];

	Points[6][0] = Maxs[0];
	Points[6][1] = Maxs[1];
	Points[6][2] = Mins[2];

	Points[7][0] = Maxs[0];
	Points[7][1] = Maxs[1];
	Points[7][2] = Maxs[2];

	//
	// Rotate the corner points by the specified angles, in the same
	// order that our Render code uses.
	//
	matrix4_t fMatrix;
	IdentityMatrix( fMatrix );

	RotateZ( fMatrix, Angles[YAW] );
	RotateY( fMatrix, Angles[PITCH] );
	RotateX( fMatrix, Angles[ROLL] );

	matrix3x4_t fMatrix2;
	for ( int nRow = 0; nRow < 3; nRow++ )
		for ( int nCol = 0; nCol < 3; nCol++ )
			fMatrix2[nRow][nCol] = fMatrix[nRow][nCol];

	Vector RotatedPoints[8];
	int i;
	for ( i = 0; i < 8; i++ )
		VectorRotate( Points[i], fMatrix2, RotatedPoints[i] );

	//
	// Calculate the new mins and maxes.
	//
	for ( i = 0; i < 8; i++ )
	{
		for ( int nDim = 0; nDim < 3; nDim++ )
		{
			if ( ( i == 0 ) || ( RotatedPoints[i][nDim] < Mins[nDim] ) )
				Mins[nDim] = RotatedPoints[i][nDim];

			if ( ( i == 0 ) || ( RotatedPoints[i][nDim] > Maxs[nDim] ) )
				Maxs[nDim] = RotatedPoints[i][nDim];
		}
	}
}

static bool StudioRange( const studiohdr_t* pHeader, int offset, int count, size_t itemSize )
{
	return offset >= 0 && offset <= pHeader->length && count >= 0 &&
		   (size_t)count <= ( pHeader->length - offset ) / itemSize;
}

static void* ReadStudioFile( const char* pszPath, size_t minimum, int ident )
{
	FILE* pFile = fopen( pszPath, "rb" );
	if ( !pFile )
		return NULL;
	fseek( pFile, 0, SEEK_END );
	long size = ftell( pFile );
	rewind( pFile );
	if ( size < 0 || (size_t)size < minimum )
	{
		fclose( pFile );
		return NULL;
	}
	studioseqhdr_t* pData = (studioseqhdr_t*)malloc( size );
	bool			valid = pData && fread( pData, size, 1, pFile ) == 1;
	fclose( pFile );
	if ( !valid || pData->id != ident || pData->version != 10 || pData->length < (int)minimum || pData->length > size )
	{
		free( pData );
		return NULL;
	}
	return pData;
}

StudioModel::StudioModel()
	: m_sequence( 0 ), m_frame( 0 ), m_bodynum( 0 ), m_skinnum( 0 ), m_mouth( 0 ), m_pstudiohdr( NULL ),
	  m_pmodel( NULL ), m_ptexturehdr( NULL )
{
	memset( m_origin, 0, sizeof( m_origin ) );
	memset( m_angles, 0, sizeof( m_angles ) );
	memset( m_controller, 0, sizeof( m_controller ) );
	memset( m_blending, 0, sizeof( m_blending ) );
	memset( m_panimhdr, 0, sizeof( m_panimhdr ) );
	memset( m_adj, 0, sizeof( m_adj ) );
}

StudioModel::~StudioModel()
{
	FreeModel();
}

void StudioModel::FreeModel()
{
	for ( size_t i = 0; i < m_Textures.size(); i++ )
		delete m_Textures[i];
	m_Textures.clear();
	for ( size_t i = 0; i < sizeof( m_panimhdr ) / sizeof( m_panimhdr[0] ); i++ )
	{
		free( m_panimhdr[i] );
		m_panimhdr[i] = NULL;
	}
	if ( m_ptexturehdr != m_pstudiohdr )
		free( m_ptexturehdr );
	free( m_pstudiohdr );
	m_ptexturehdr = NULL;
	m_pstudiohdr = NULL;
	m_pmodel = NULL;
}

bool StudioModel::LoadModel( const char* pszPath )
{
	FreeModel();
	m_pstudiohdr = (studiohdr_t*)ReadStudioFile( pszPath, sizeof( studiohdr_t ), 'TSDI' );
	if ( !m_pstudiohdr )
		return false;
	const studiohdr_t* p = m_pstudiohdr;
	if ( p->numbones <= 0 || p->numbones > MAXSTUDIOBONES || p->numseq <= 0 || p->numseqgroups <= 0 ||
		 p->numseqgroups > (int)( sizeof( m_panimhdr ) / sizeof( m_panimhdr[0] ) ) ||
		 !StudioRange( p, p->boneindex, p->numbones, sizeof( mstudiobone_t ) ) ||
		 !StudioRange( p, p->bonecontrollerindex, p->numbonecontrollers, sizeof( mstudiobonecontroller_t ) ) ||
		 !StudioRange( p, p->seqindex, p->numseq, sizeof( mstudioseqdesc_t ) ) ||
		 !StudioRange( p, p->seqgroupindex, p->numseqgroups, sizeof( mstudioseqgroup_t ) ) ||
		 !StudioRange( p, p->bodypartindex, p->numbodyparts, sizeof( mstudiobodyparts_t ) ) )
		return false;
	m_sequence = 0;
	m_frame = 0;
	m_bodynum = 0;
	m_skinnum = 0;
	return true;
}

bool StudioModel::PostLoadModel( const char* pszPath )
{
	if ( !m_pstudiohdr )
		return false;
	std::string path( pszPath );
	if ( path.size() < 4 )
		return false;
	std::string base = path.substr( 0, path.size() - 4 );
	m_ptexturehdr = m_pstudiohdr;
	if ( m_pstudiohdr->numtextures == 0 )
		m_ptexturehdr = (studiohdr_t*)ReadStudioFile( ( base + "T.mdl" ).c_str(), sizeof( studiohdr_t ), 'TSDI' );
	if ( !m_ptexturehdr )
		return false;
	studiohdr_t* p = m_ptexturehdr;
	if ( p->numtextures <= 0 || !StudioRange( p, p->textureindex, p->numtextures, sizeof( mstudiotexture_t ) ) ||
		 p->numskinref < 0 || p->numskinfamilies < 0 ||
		 ( p->numskinfamilies && p->numskinref > INT_MAX / p->numskinfamilies ) ||
		 !StudioRange( p, p->skinindex, p->numskinref * p->numskinfamilies, sizeof( short ) ) )
		return false;
	mstudiotexture_t* pTextures = (mstudiotexture_t*)( (byte*)p + p->textureindex );
	for ( int i = 0; i < p->numtextures; i++ )
	{
		mstudiotexture_t& texture = pTextures[i];
		if ( texture.width <= 0 || texture.height <= 0 || texture.width > INT_MAX / 4 / texture.height )
			return false;
		int count = texture.width * texture.height;
		if ( count > INT_MAX - 768 || !StudioRange( p, texture.index, count + 768, 1 ) )
			return false;
		CTexture* pImage = new CTexture;
		m_Textures.push_back( pImage );
		if ( !pImage->Allocate( texture.width, texture.height, TEXTURE_HAS_ALPHA ) )
			return false;
		const byte* pIndices = (byte*)p + texture.index;
		const byte* pPalette = pIndices + count;
		byte*		pPixels = (byte*)pImage->GetImageDataPtr();
		for ( int j = 0; j < count; j++, pPixels += 4 )
		{
			memcpy( pPixels, pPalette + pIndices[j] * 3, 3 );
			pPixels[3] = ( texture.flags & STUDIO_NF_MASKED ) && pIndices[j] == 255 ? 0 : 255;
		}
	}
	for ( int i = 1; i < m_pstudiohdr->numseqgroups; i++ )
	{
		char suffix[16];
		sprintf( suffix, "%02d.mdl", i );
		m_panimhdr[i] = (studioseqhdr_t*)ReadStudioFile( ( base + suffix ).c_str(), sizeof( studioseqhdr_t ), 'QSDI' );
		if ( !m_panimhdr[i] )
			return false;
	}
	SetSequence( 0 );
	return true;
}

void StudioModel::DrawModel( CRender3D* pRender )
{
	if ( !m_pstudiohdr || !m_ptexturehdr )
		return;
	Vector right;
	pRender->GetViewRight( right );
	VectorCopy( right.Base(), g_vright );
	glPushAttrib( GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_POLYGON_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT );
	mstudiotexture_t* pTextures = (mstudiotexture_t*)( (byte*)m_ptexturehdr + m_ptexturehdr->textureindex );
	for ( size_t i = 0; i < m_Textures.size(); i++ )
	{
		pRender->BindTexture( m_Textures[i] );
		glGetIntegerv( GL_TEXTURE_BINDING_2D, &pTextures[i].index );
	}
	glFrontFace( GL_CCW );
	DrawModel();
	glPopAttrib();
}

void StudioModel::ExtractBbox( Vector& mins, Vector& maxs )
{
	ExtractBbox( mins.Base(), maxs.Base() );
	QAngle angles( m_angles );
	RotateBbox( mins, maxs, angles );
}

void StudioModel::ExtractClippingBbox( Vector& mins, Vector& maxs )
{
	mins = Vector( m_pstudiohdr->bbmin );
	maxs = Vector( m_pstudiohdr->bbmax );
}

void StudioModel::ExtractMovementBbox( Vector& mins, Vector& maxs )
{
	mins = Vector( m_pstudiohdr->min );
	maxs = Vector( m_pstudiohdr->max );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int StudioModel::GetSequenceCount()
{
	return m_pstudiohdr ? m_pstudiohdr->numseq : 0;
}

void StudioModel::GetSequenceName( int index, char* pszName )
{
	if ( index < 0 || index >= GetSequenceCount() )
	{
		pszName[0] = 0;
		return;
	}
	mstudioseqdesc_t* pSequences = (mstudioseqdesc_t*)( (byte*)m_pstudiohdr + m_pstudiohdr->seqindex );
	memcpy( pszName, pSequences[index].label, sizeof( pSequences[index].label ) );
	pszName[sizeof( pSequences[index].label ) - 1] = 0;
}

void StudioModel::SetOrigin( float x, float y, float z )
{
	m_origin[0] = x;
	m_origin[1] = y;
	m_origin[2] = z;
}

void StudioModel::SetAngles( QAngle& angles )
{
	VectorCopy( angles.Base(), m_angles );
}

bool StudioModel::IsTranslucent()
{
	if ( !m_ptexturehdr )
		return false;
	mstudiotexture_t* pTextures = (mstudiotexture_t*)( (byte*)m_ptexturehdr + m_ptexturehdr->textureindex );
	for ( int i = 0; i < m_ptexturehdr->numtextures; i++ )
		if ( !( pTextures[i].flags & STUDIO_NF_ADDITIVE ) )
			return false;
	return m_ptexturehdr->numtextures > 0;
}

int StudioModel::SetBodygroup( int group, int value )
{
	if ( group < 0 || group >= m_pstudiohdr->numbodyparts )
		return -1;
	mstudiobodyparts_t* pPart = (mstudiobodyparts_t*)( (byte*)m_pstudiohdr + m_pstudiohdr->bodypartindex ) + group;
	int					current = ( m_bodynum / pPart->base ) % pPart->nummodels;
	if ( value < 0 || value >= pPart->nummodels )
		return current;
	m_bodynum += ( value - current ) * pPart->base;
	return value;
}

int StudioModel::SetSkin( int value )
{
	if ( value >= 0 && value < m_ptexturehdr->numskinfamilies )
		m_skinnum = value;
	return m_skinnum;
}
