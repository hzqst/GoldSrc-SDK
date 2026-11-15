#if defined( A3D_GEOMETRY_CORE )

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "A3d3.h"
#include "A3dGeom.h"
#include "ChunkPage.h"
#include "A3dList.h"
#include "WallEdge.h"
#include "matrix.h"
#include "A3dSource.h"

#include <string.h>
#include <new>
#include <stddef.h>
#include <math.h>
#include <stdio.h>

HRESULT STDMETHODCALLTYPE CA3d::Enable( DWORD dwFeatures )
{
	if ( !( m_dwFeaturesAvailable & dwFeatures ) )
		return A3DERROR_FEATURE_NOT_SUPPORTED;

	m_dwEnabledFeatures |= dwFeatures;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::Disable( DWORD dwFeatures )
{
	if ( m_dwEnabledFeatures & dwFeatures )
		m_dwEnabledFeatures -= dwFeatures;

	return S_OK;
}

BOOL STDMETHODCALLTYPE CA3d::IsEnabled( DWORD dwFeatures )
{
	return ( m_dwEnabledFeatures & dwFeatures ) != 0;
}

HRESULT STDMETHODCALLTYPE CA3d::SetReflectionGainScale( A3DVAL flScale )
{
	if ( !( flScale >= 0.0f ) )
		return E_INVALIDARG;

	m_flReflectionGainScale = flScale;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::GetReflectionGainScale( LPA3DVAL pScale )
{
	if ( !pScale )
		return E_INVALIDARG;

	DWORD dwScale;
	memcpy( &dwScale, &m_flReflectionGainScale, sizeof( dwScale ) );
	memcpy( pScale, &dwScale, sizeof( dwScale ) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::SetReflectionDelayScale( A3DVAL flScale )
{
	if ( flScale >= 0.0f )
		m_flReflectionDelayScale = flScale;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::GetReflectionDelayScale( LPA3DVAL pScale )
{
	if ( !pScale )
		return E_INVALIDARG;

	DWORD dwScale;
	memcpy( &dwScale, &m_flReflectionDelayScale, sizeof( dwScale ) );
	memcpy( pScale, &dwScale, sizeof( dwScale ) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::Begin( DWORD dwMode )
{
	switch ( dwMode )
	{
	case A3D_LINES:
	case A3D_SUB_LINES:
		m_Primitive.m_nVertices = A3D_LINES;
		break;

	case A3D_TRIANGLES:
	case A3D_SUB_TRIANGLES:
		m_Primitive.m_nVertices = A3D_TRIANGLES;
		break;

	case A3D_QUADS:
	case A3D_SUB_QUADS:
		m_Primitive.m_nVertices = A3D_QUADS;
		break;

	default:
		m_dwInputMode = (DWORD)A3D_INVALID_INPUTMODE;
		return A3DERROR_INVALID_BEGIN_MODE;
	}

	m_dwInputMode = dwMode;
	m_bCalculateNormal = TRUE;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::End()
{
	m_dwInputMode = (DWORD)A3D_INVALID_INPUTMODE;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::Tag( DWORD dwTag )
{
	m_Primitive.m_dwTag = dwTag;
	return S_OK;
}

void CA3d::AppendGeometryPool( void* pPool )
{
	m_GeometryPools.Append( pPool );
}

HRESULT STDMETHODCALLTYPE CA3d::SetOpeningFactorf( A3DVAL flFactor )
{
	if ( m_pCurrentCommand )
		memcpy( &m_pCurrentCommand->m_flOpeningFactor, &flFactor, sizeof( flFactor ) );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::SetOpeningFactorfv( LPA3DVAL pFactor )
{
	if ( m_pCurrentCommand )
		m_pCurrentCommand->m_pOpeningFactor = pFactor;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::NewList( LPA3DLIST* ppList )
{
	CA3dList* pList = new ( std::nothrow ) CA3dList( this );
	if ( !pList )
		return A3DERROR_MEMORY_ALLOCATION;

	pList->AddRef();
	*ppList = (LPA3DLIST)pList;
	return S_OK;
}

double CA3d::ReflectPoint( const A3DVAL* pPoint, const A3DVAL* pNormal, const A3DVAL* pPlanePoint,
		A3DVAL* pReflectedPoint, A3DVAL* pScaledPoint )
{
	double flDistance = ( ( (double)pPoint[2] - pPlanePoint[2] ) * pNormal[2] +
		( (double)pPoint[1] - pPlanePoint[1] ) * pNormal[1] ) +
		( (double)pPoint[0] - pPlanePoint[0] ) * pNormal[0];
	A3DVAL flTwiceDistance = (A3DVAL)( flDistance + flDistance );
	for ( int i = 0; i < 3; i++ )
		pReflectedPoint[i] = (A3DVAL)( (double)pPoint[i] - (double)pNormal[i] * flTwiceDistance );

	double flScaledDistance = flDistance * m_flReflectionPlaneScale;
	for ( int i = 0; i < 3; i++ )
		pScaledPoint[i] = (A3DVAL)( (double)pPoint[i] - (double)pNormal[i] * flScaledDistance );

	return flDistance;
}

a3d_geometry_command_t* CA3dCommandBuffer::ReadCommand()
{
	if ( m_iCursor >= m_cCommands )
		return NULL;

	int iCommand = m_iCursor++;
	return &m_pCommands[iCommand];
}

void* CA3dObjectList::Next()
{
	if ( !m_pCurrent )
		return NULL;

	void* pObject = m_pCurrent->m_pObject;
	m_pCurrent = m_pCurrent->m_pNext;
	return pObject;
}

void CA3dObjectList::Append( void* pObject )
{
	if ( !m_nCount )
	{
		m_pFirst = new ( std::nothrow ) CA3dListNode( NULL, pObject );
		m_pCurrent = m_pFirst;
		m_nCount = (int)( (unsigned int)m_nCount + 1 );
		return;
	}

	CA3dListNode* pLast = m_pFirst;
	while ( pLast->m_pNext )
		pLast = pLast->m_pNext;

	m_pLast = pLast;
	if ( pLast->m_pNext )
	{
		pLast = pLast->m_pNext;
		while ( pLast->m_pNext )
			pLast = pLast->m_pNext;

		m_pLast = pLast->Append( pObject );
	}
	else
	{
		CA3dListNode* pNode = new ( std::nothrow ) CA3dListNode( pLast, pObject );
		pLast->m_pNext = pNode;
		m_pLast = pNode;
	}

	m_pCurrent = m_pLast;
	m_nCount = (int)( (unsigned int)m_nCount + 1 );
}

BOOL A3dIntersectTriangle( const A3DVAL* pStart, const A3DVAL* pDirection, a3d_primitive_t* pPrimitive,
		BOOL bExpandEdges, BOOL bRay )
{
	A3DVAL* pNormal = pPrimitive->m_Normal;
	const A3DVAL* pFirst = pPrimitive->m_Vertices[0];
	const A3DVAL* pSecond = pPrimitive->m_Vertices[1];
	const A3DVAL* pThird = pPrimitive->m_Vertices[2];
	double flDirectionDot = ( (double)pDirection[0] * pNormal[0] + (double)pDirection[1] * pNormal[1] ) +
		(double)pDirection[2] * pNormal[2];
	double flAbsDot = flDirectionDot;
	if ( !( flDirectionDot >= 0 ) )
		flAbsDot = -flDirectionDot;
	if ( !( flAbsDot >= A3D_EPSILON ) )
		return FALSE;

	double flFraction = ( ( ( (double)pFirst[1] * pNormal[1] + (double)pFirst[0] * pNormal[0] ) +
		(double)pFirst[2] * pNormal[2] ) -
		( ( (double)pStart[0] * pNormal[0] + (double)pStart[2] * pNormal[2] ) +
		(double)pStart[1] * pNormal[1] ) ) / flDirectionDot;
	if ( !( flFraction >= 0 ) )
	{
		A3DVAL flNormalX = -pNormal[0];
		A3DVAL flNormalY = -pNormal[1];
		double flNormalZ = -pNormal[2];
		pNormal[0] = flNormalX;
		pNormal[1] = flNormalY;
		pNormal[2] = (A3DVAL)flNormalZ;

		A3DVAL flDot = (A3DVAL)( ( (double)pDirection[0] * flNormalX +
			flNormalZ * pDirection[2] ) + (double)flNormalY * pDirection[1] );
		flFraction = ( ( ( flNormalZ * pFirst[2] + (double)flNormalY * pFirst[1] ) +
			(double)flNormalX * pFirst[0] ) -
			( ( (double)pStart[0] * flNormalX + flNormalZ * pStart[2] ) +
			(double)flNormalY * pStart[1] ) ) / flDot;
	}

	pPrimitive->m_flDistance = (A3DVAL)flFraction;
	if ( !( flFraction >= 0 ) )
		return FALSE;
	if ( !bRay && flFraction > 1 )
		return FALSE;

	double flPointX = flFraction * pDirection[0] + pStart[0];
	A3DVAL flPointY = (A3DVAL)( flFraction * pDirection[1] + pStart[1] );
	A3DVAL flPointZ = (A3DVAL)( flFraction * pDirection[2] + pStart[2] );
	A3DVAL flAbsX = pNormal[0] < 0 ? -pNormal[0] : pNormal[0];
	double flAbsY = pNormal[1] < 0 ? -(double)pNormal[1] : pNormal[1];
	double flAbsZ = pNormal[2] < 0 ? -(double)pNormal[2] : pNormal[2];
	double flPointU;
	double flEdgeU;
	A3DVAL flPointV;
	A3DVAL flOtherEdgeU;
	A3DVAL flEdgeV;
	A3DVAL flOtherEdgeV;

	if ( flAbsX > flAbsY && flAbsX > flAbsZ )
	{
		flPointU = (double)flPointY - pFirst[1];
		flPointV = (A3DVAL)( (double)flPointZ - pFirst[2] );
		flEdgeU = (double)pSecond[1] - pFirst[1];
		flOtherEdgeU = (A3DVAL)( (double)pThird[1] - pFirst[1] );
		flEdgeV = (A3DVAL)( (double)pSecond[2] - pFirst[2] );
		flOtherEdgeV = (A3DVAL)( (double)pThird[2] - pFirst[2] );
	}
	else if ( flAbsY > flAbsX && flAbsY > flAbsZ )
	{
		flPointU = flPointX - pFirst[0];
		flPointV = (A3DVAL)( (double)flPointZ - pFirst[2] );
		flEdgeU = (double)pSecond[0] - pFirst[0];
		flOtherEdgeU = (A3DVAL)( (double)pThird[0] - pFirst[0] );
		flEdgeV = (A3DVAL)( (double)pSecond[2] - pFirst[2] );
		flOtherEdgeV = (A3DVAL)( (double)pThird[2] - pFirst[2] );
	}
	else
	{
		flPointU = flPointX - pFirst[0];
		flPointV = (A3DVAL)( (double)flPointY - pFirst[1] );
		flEdgeU = (double)pSecond[0] - pFirst[0];
		flOtherEdgeU = (A3DVAL)( (double)pThird[0] - pFirst[0] );
		flEdgeV = (A3DVAL)( (double)pSecond[1] - pFirst[1] );
		flOtherEdgeV = (A3DVAL)( (double)pThird[1] - pFirst[1] );
	}

	A3DVAL flAlpha;
	A3DVAL flBeta;
	if ( !( flEdgeU < 0 || flEdgeU > 0 ) )
	{
		if ( !( flOtherEdgeU < 0 || flOtherEdgeU > 0 ) )
			return FALSE;

		double flBetaWide = flPointU / flOtherEdgeU;
		flBeta = (A3DVAL)flBetaWide;
		if ( !( flBetaWide > 0 ) || !( flBeta < 1 ) || !( flEdgeV < 0 || flEdgeV > 0 ) )
			return FALSE;

		flAlpha = (A3DVAL)( ( (double)flPointV - (double)flBeta * flOtherEdgeV ) / flEdgeV );
	}
	else
	{
		double flDeterminant = (double)flOtherEdgeV * flEdgeU - (double)flEdgeV * flOtherEdgeU;
		if ( !( flDeterminant < 0 || flDeterminant > 0 ) )
			return FALSE;

		flBeta = (A3DVAL)( ( (double)flPointV * flEdgeU - (double)flEdgeV * flPointU ) / flDeterminant );
		if ( !( flBeta > 0 ) || !( flBeta < 1 ) )
			return FALSE;

		flAlpha = (A3DVAL)( ( flPointU - (double)flBeta * flOtherEdgeU ) / flEdgeU );
	}

	if ( bExpandEdges )
		return flAlpha >= -A3D_EPSILON && (double)flAlpha + flBeta <= 1.000001f;

	return flAlpha > 0 && (double)flAlpha + flBeta < 1;
}

BOOL A3dIntersectQuad( const A3DVAL* pStart, const A3DVAL* pDirection, a3d_primitive_t* pPrimitive,
		BOOL bExpandEdges, BOOL bRay )
{
	A3DVAL* pNormal = pPrimitive->m_Normal;
	const A3DVAL* pFirst = pPrimitive->m_Vertices[0];
	double flDirectionDot = ( (double)pDirection[0] * pNormal[0] + (double)pNormal[1] * pDirection[1] ) +
		(double)pNormal[2] * pDirection[2];
	double flAbsDot = flDirectionDot;
	if ( !( flDirectionDot >= 0 ) )
		flAbsDot = -flDirectionDot;
	if ( !( flAbsDot >= A3D_EPSILON ) )
		return FALSE;

	double flFraction = ( ( ( (double)pFirst[0] * pNormal[0] + (double)pFirst[1] * pNormal[1] ) +
		(double)pFirst[2] * pNormal[2] ) -
		( ( (double)pStart[0] * pNormal[0] + (double)pStart[2] * pNormal[2] ) +
		(double)pStart[1] * pNormal[1] ) ) / flDirectionDot;
	if ( !( flFraction >= 0 ) )
	{
		double flVertexZ = pFirst[2];
		double flVertexY = pFirst[1];
		A3DVAL flNormalX = -pNormal[0];
		A3DVAL flNormalY = -pNormal[1];
		double flNormalZ = -pNormal[2];
		pNormal[1] = flNormalY;
		pNormal[2] = (A3DVAL)flNormalZ;
		pNormal[0] = flNormalX;

		A3DVAL flDot = (A3DVAL)( ( (double)pDirection[0] * flNormalX +
			(double)pDirection[2] * flNormalZ ) + (double)pDirection[1] * flNormalY );
		flFraction = ( ( ( flVertexZ * flNormalZ + flVertexY * flNormalY ) +
			(double)pFirst[0] * flNormalX ) -
			( ( (double)pStart[0] * flNormalX + (double)pStart[2] * flNormalZ ) +
			(double)pStart[1] * flNormalY ) ) / flDot;
	}

	pPrimitive->m_flDistance = (A3DVAL)flFraction;
	if ( !( flFraction >= 0 ) )
		return FALSE;
	if ( !bRay && flFraction > 1 )
		return FALSE;

	A3DVAL point[3];
	point[0] = (A3DVAL)( flFraction * pDirection[0] + pStart[0] );
	point[1] = (A3DVAL)( flFraction * pDirection[1] + pStart[1] );
	point[2] = (A3DVAL)( flFraction * pDirection[2] + pStart[2] );
	A3DVAL flAbsX = pNormal[0] < 0 ? -pNormal[0] : pNormal[0];
	double flAbsY = pNormal[1] < 0 ? -(double)pNormal[1] : pNormal[1];
	double flAbsZ = pNormal[2] < 0 ? -(double)pNormal[2] : pNormal[2];
	int u = 0;
	int v = 1;
	if ( flAbsX > flAbsY && flAbsX > flAbsZ )
	{
		u = 1;
		v = 2;
	}
	else if ( flAbsY > flAbsX && flAbsY > flAbsZ )
	{
		v = 2;
	}

	for ( int i = 0; i < 2; i++ )
	{
		const A3DVAL* pSecond = pPrimitive->m_Vertices[i + 1];
		const A3DVAL* pThird = pPrimitive->m_Vertices[i + 2];
		double flPointU = (double)point[u] - pFirst[u];
		A3DVAL flPointV = (A3DVAL)( (double)point[v] - pFirst[v] );
		double flEdgeU = (double)pSecond[u] - pFirst[u];
		A3DVAL flOtherEdgeU = (A3DVAL)( (double)pThird[u] - pFirst[u] );
		A3DVAL flEdgeV = (A3DVAL)( (double)pSecond[v] - pFirst[v] );
		A3DVAL flOtherEdgeV = (A3DVAL)( (double)pThird[v] - pFirst[v] );
		A3DVAL flAlpha;
		A3DVAL flBeta;

		if ( !( flEdgeU < 0 || flEdgeU > 0 ) )
		{
			if ( !( flOtherEdgeU < 0 || flOtherEdgeU > 0 ) )
				continue;

			double flBetaWide = flPointU / flOtherEdgeU;
			flBeta = (A3DVAL)flBetaWide;
			A3DVAL flCheckedEdge = i ? flOtherEdgeV : flEdgeV;
			if ( !( flBetaWide > 0 ) || !( flBeta < 1 ) || !( flCheckedEdge < 0 || flCheckedEdge > 0 ) )
				continue;

			flAlpha = (A3DVAL)( ( (double)flPointV - (double)flBeta * flOtherEdgeV ) / flEdgeV );
		}
		else
		{
			double flDeterminant = (double)flOtherEdgeV * flEdgeU - (double)flEdgeV * flOtherEdgeU;
			if ( !( flDeterminant < 0 || flDeterminant > 0 ) )
				continue;

			flBeta = (A3DVAL)( ( (double)flPointV * flEdgeU - (double)flEdgeV * flPointU ) / flDeterminant );
			if ( !( flBeta > 0 ) || !( flBeta < 1 ) )
				continue;

			flAlpha = (A3DVAL)( ( flPointU - (double)flBeta * flOtherEdgeU ) / flEdgeU );
		}

		if ( bExpandEdges )
		{
			if ( flAlpha >= -A3D_EPSILON && (double)flAlpha + flBeta <= 1.000001f )
				return TRUE;
		}
		else if ( flAlpha > 0 && (double)flAlpha + flBeta < 1 )
		{
			return TRUE;
		}
	}

	return FALSE;
}

#define A3D_FACE_SKIP_RENDER	0x00000002

BOOL CA3dGeometryFace::IntersectsSegment( const A3DVERTEX* pEndpoints )
{
	m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
	for ( int nPolygon = m_Polygons.m_nCount; nPolygon > 0; nPolygon-- )
	{
		a3d_polygon_t* pPolygon = (a3d_polygon_t*)m_Polygons.Next();
		a3d_polygon_t transformed;
		m_Transform.TransformPolygon( &transformed, pPolygon );

		A3DVAL direction[3];
		for ( int i = 0; i < 3; i++ )
			direction[i] = pEndpoints[0][i] - pEndpoints[1][i];

		transformed.m_flDistance = 0.0f;
		BOOL bHit = FALSE;
		if ( transformed.m_nVertices == A3D_TRIANGLES )
			bHit = A3dIntersectTriangle( pEndpoints[0], direction, &transformed, FALSE, FALSE );
		else if ( transformed.m_nVertices == A3D_QUADS )
			bHit = A3dIntersectQuad( pEndpoints[0], direction, &transformed, FALSE, FALSE );
		if ( bHit && !( transformed.m_flDistance > 1.0f ) )
			return TRUE;

		for ( int i = 0; i < 3; i++ )
			direction[i] *= -1.0f;

		transformed.m_flDistance = 0.0f;
		bHit = FALSE;
		if ( transformed.m_nVertices == A3D_TRIANGLES )
			bHit = A3dIntersectTriangle( pEndpoints[1], direction, &transformed, FALSE, FALSE );
		else if ( transformed.m_nVertices == A3D_QUADS )
			bHit = A3dIntersectQuad( pEndpoints[1], direction, &transformed, FALSE, FALSE );
		if ( bHit )
			return TRUE;
	}

	return FALSE;
}

BOOL CA3dGeometryFace::HasEdgeConflict()
{
	m_Edges.m_pCurrent = m_Edges.m_pFirst;
	for ( int nEdge = m_Edges.m_nCount; nEdge > 0; nEdge-- )
	{
		CA3dGeometryEdge* pEdge = (CA3dGeometryEdge*)m_Edges.Next();
		if ( pEdge->HasConflict() )
			return TRUE;
	}

	return FALSE;
}

int CA3dGeometryFace::ConnectEdges( CA3dGeometryFace* pFace )
{
	int nConnectedEdges = 0;
	m_Edges.m_pCurrent = m_Edges.m_pFirst;
	for ( int nEdge = m_Edges.m_nCount; nEdge > 0; nEdge-- )
	{
		CA3dGeometryEdge* pEdge = (CA3dGeometryEdge*)m_Edges.Next();
		A3DVERTEX endpoints[2];
		pEdge->TransformEndpoints( endpoints );
		if ( pFace->IntersectsSegment( endpoints ) )
			return 1;

		pFace->m_Edges.m_pCurrent = pFace->m_Edges.m_pFirst;
		for ( int nOtherEdge = pFace->m_Edges.m_nCount; nOtherEdge > 0; nOtherEdge-- )
		{
			CA3dGeometryEdge* pOtherEdge = (CA3dGeometryEdge*)pFace->m_Edges.Next();
			A3DVERTEX otherEndpoints[2];
			pOtherEdge->TransformEndpoints( otherEndpoints );

			BOOL bConnect = Geometry_SameSegment( endpoints, otherEndpoints );
			if ( !bConnect )
			{
				BOOL bOtherFirstInside = Geometry_PointOnSegment( otherEndpoints[0], endpoints );
				BOOL bOtherSecondInside = Geometry_PointOnSegment( otherEndpoints[1], endpoints );
				if ( bOtherFirstInside && bOtherSecondInside )
				{
					bConnect = TRUE;
				}
				else
				{
					BOOL bFirstInside = Geometry_PointOnSegment( endpoints[0], otherEndpoints );
					BOOL bSecondInside = Geometry_PointOnSegment( endpoints[1], otherEndpoints );
					bConnect = ( bFirstInside && bSecondInside ) ||
						( bOtherFirstInside && bFirstInside ) || ( bOtherSecondInside && bSecondInside );
				}
			}

			if ( bConnect )
				pEdge->m_AdjacentEdges.Append( pOtherEdge );
		}

		if ( pEdge->m_AdjacentEdges.m_nCount > 0 )
			nConnectedEdges++;
	}

	return nConnectedEdges > 0 ? 0 : 2;
}

BOOL CA3dGeometryFace::IsAdjacentTo( CA3dGeometryFace* pFace )
{
	A3DVECTOR normal;
	A3DVECTOR otherNormal;
	m_Transform.TransformVector( normal, m_Plane );
	pFace->m_Transform.TransformVector( otherNormal, pFace->m_Plane );

	double flDot = (double)otherNormal[0] * normal[0] +
		(double)otherNormal[1] * normal[1] + (double)otherNormal[2] * normal[2];
	if ( 1.0 - fabs( flDot ) > A3D_EPSILON )
		return FALSE;

	return HasPolygonWithin( pFace ) || pFace->HasPolygonWithin( this );
}

HRESULT CA3dGeometryFace::ConnectTo( CA3dGeometryFace* pFace )
{
	A3DVECTOR normal;
	A3DVECTOR otherNormal;
	m_Transform.TransformVector( normal, m_Plane );
	pFace->m_Transform.TransformVector( otherNormal, pFace->m_Plane );

	double flDot = (double)otherNormal[0] * normal[0] +
		(double)otherNormal[1] * normal[1] + (double)otherNormal[2] * normal[2];
	if ( !( 1.0 - fabs( flDot ) > A3D_EPSILON ) &&
		( IsWithin( pFace ) || pFace->IsWithin( this ) ) )
	{
		m_AdjacentFaces.Append( pFace );
	}

	return S_OK;
}

void CA3dGeometryFace::Render( CA3d* pA3d )
{
	if ( m_dwFlags & A3D_FACE_SKIP_RENDER )
		return;

	IA3dGeomTarget* pGeometry = pA3d;
	if ( m_pMaterial )
		pGeometry->BindMaterial( m_pMaterial );

	m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
	for ( int nPolygon = m_Polygons.m_nCount; nPolygon > 0; nPolygon-- )
	{
		a3d_polygon_t* pPolygon = (a3d_polygon_t*)m_Polygons.Next();
		a3d_polygon_t transformed;
		m_Transform.TransformPolygon( &transformed, pPolygon );

		int nVertices = pPolygon->m_nVertices;
		if ( nVertices == A3D_TRIANGLES || nVertices == A3D_QUADS )
		{
			pGeometry->Begin( nVertices );
			pGeometry->Tag( (DWORD)pPolygon );
			pGeometry->Normal3fv( transformed.m_Normal );
			for ( int i = 0; i < nVertices; i++ )
				pGeometry->Vertex3fv( transformed.m_Vertices[i] );
		}

		pGeometry->End();
	}

	m_Openings.m_pCurrent = m_Openings.m_pFirst;
	for ( int nOpening = m_Openings.m_nCount; nOpening > 0; nOpening-- )
	{
		CA3dOpening* pOpening = (CA3dOpening*)m_Openings.Next();
		pOpening->Render( pA3d );
	}
}

void CA3dGeometryFace::RebuildOpenings()
{
	m_AdjacentFaces.m_pCurrent = m_AdjacentFaces.m_pFirst;
	for ( int nFace = m_AdjacentFaces.m_nCount; nFace > 0; nFace-- )
	{
		CA3dGeometryFace* pFace = (CA3dGeometryFace*)m_AdjacentFaces.Next();
		pFace->m_Openings.m_pCurrent = pFace->m_Openings.m_pFirst;
		for ( int nOpening = pFace->m_Openings.m_nCount; nOpening > 0; nOpening-- )
		{
			CA3dOpening* pOpening = (CA3dOpening*)pFace->m_Openings.Next();
			if ( pOpening->m_dwInstanceMode && ContainsOpening( pOpening ) )
			{
				CA3dOpening* pCopy = new ( std::nothrow ) CA3dOpening( pOpening, this, pFace );
				pCopy->AddRef();
				m_Openings.Append( pCopy );
			}
		}
	}
}

BOOL CA3dGeometryFace::HasPolygonWithin( CA3dGeometryFace* pFace )
{
	m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
	for ( int nSourceRemaining = m_Polygons.m_nCount; nSourceRemaining > 0; --nSourceRemaining )
	{
		a3d_polygon_t* pPolygon = (a3d_polygon_t*)m_Polygons.Next();
		a3d_polygon_t polygon;
		m_Transform.TransformPolygon( &polygon, pPolygon );
		A3DVECTOR plane;
		pFace->m_Transform.TransformVector( plane, pFace->m_Plane );

		BOOL bWithin = TRUE;
		int nVertex = polygon.m_nVertices;
		while ( bWithin && nVertex-- > 0 )
		{
			pFace->m_Polygons.m_pCurrent = pFace->m_Polygons.m_pFirst;
			for ( int nTargetRemaining = pFace->m_Polygons.m_nCount; nTargetRemaining > 0; --nTargetRemaining )
			{
				a3d_polygon_t* pTarget = (a3d_polygon_t*)pFace->m_Polygons.Next();
				a3d_polygon_t target;
				pFace->m_Transform.TransformPolygon( &target, pTarget );

				double dx = (double)target.m_Vertices[0][0] - polygon.m_Vertices[nVertex][0];
				double dy = (double)target.m_Vertices[0][1] - polygon.m_Vertices[nVertex][1];
				double dz = (double)target.m_Vertices[0][2] - polygon.m_Vertices[nVertex][2];
				double flDistanceSquared = ( dx * dx + dy * dy ) + dz * dz;
				if ( flDistanceSquared >= A3D_EPSILON_SQUARED )
				{
					double flDistance = ( dx * plane[0] + dy * plane[1] ) + dz * plane[2];
					if ( fabs( flDistance ) >= A3D_EPSILON )
					{
						bWithin = FALSE;
						break;
					}
				}

				int nClass = Geometry_ClassifyTrianglePoint( polygon.m_Vertices[nVertex], plane, target.m_Vertices );
				if ( nClass == TRIANGLE_POINT_OUTSIDE && target.m_nVertices == 4 )
				{
					A3DVERTEX triangle[3];
					memcpy( triangle[0], target.m_Vertices[0], sizeof( triangle[0] ) );
					memcpy( triangle[1], target.m_Vertices[2], sizeof( triangle[1] ) );
					memcpy( triangle[2], target.m_Vertices[3], sizeof( triangle[2] ) );
					nClass = Geometry_ClassifyTrianglePoint( polygon.m_Vertices[nVertex], plane, triangle );
				}
				if ( nClass < TRIANGLE_POINT_BOUNDARY || nClass > TRIANGLE_POINT_INTERIOR )
				{
					bWithin = FALSE;
					break;
				}
			}
		}
		if ( bWithin )
			return TRUE;
	}
	return FALSE;
}

BOOL CA3dGeometryFace::IsWithin( CA3dGeometryFace* pFace )
{
	m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
	for ( int nSourceRemaining = m_Polygons.m_nCount; nSourceRemaining > 0; --nSourceRemaining )
	{
		a3d_polygon_t* pPolygon = (a3d_polygon_t*)m_Polygons.Next();
		a3d_polygon_t polygon;
		m_Transform.TransformPolygon( &polygon, pPolygon );
		A3DVECTOR plane;
		pFace->m_Transform.TransformVector( plane, pFace->m_Plane );

		int nVertex = polygon.m_nVertices;
		while ( nVertex-- > 0 )
		{
			pFace->m_Polygons.m_pCurrent = pFace->m_Polygons.m_pFirst;
			for ( int nTargetRemaining = pFace->m_Polygons.m_nCount; nTargetRemaining > 0; --nTargetRemaining )
			{
				a3d_polygon_t* pTarget = (a3d_polygon_t*)pFace->m_Polygons.Next();
				a3d_polygon_t target;
				pFace->m_Transform.TransformPolygon( &target, pTarget );

				double dx = (double)target.m_Vertices[0][0] - polygon.m_Vertices[nVertex][0];
				double dy = (double)target.m_Vertices[0][1] - polygon.m_Vertices[nVertex][1];
				double dz = (double)target.m_Vertices[0][2] - polygon.m_Vertices[nVertex][2];
				double flDistanceSquared = ( dx * dx + dy * dy ) + dz * dz;
				if ( flDistanceSquared >= A3D_EPSILON_SQUARED )
				{
					double flDistance = ( dx * plane[0] + dy * plane[1] ) + dz * plane[2];
					if ( fabs( flDistance ) >= A3D_EPSILON )
						return FALSE;
				}

				int nClass = Geometry_ClassifyTrianglePoint( polygon.m_Vertices[nVertex], plane, target.m_Vertices );
				if ( nClass == TRIANGLE_POINT_OUTSIDE && target.m_nVertices == 4 )
				{
					A3DVERTEX triangle[3];
					memcpy( triangle[0], target.m_Vertices[0], sizeof( triangle[0] ) );
					memcpy( triangle[1], target.m_Vertices[2], sizeof( triangle[1] ) );
					memcpy( triangle[2], target.m_Vertices[3], sizeof( triangle[2] ) );
					nClass = Geometry_ClassifyTrianglePoint( polygon.m_Vertices[nVertex], plane, triangle );
				}
				if ( nClass < TRIANGLE_POINT_BOUNDARY || nClass > TRIANGLE_POINT_INTERIOR )
					return FALSE;
			}
		}
	}
	return TRUE;
}

BOOL CA3dGeometryFace::ContainsOpening( CA3dOpening* pOpening )
{
	pOpening->m_Polygons.m_pCurrent = pOpening->m_Polygons.m_pFirst;
	for ( int nSourceRemaining = pOpening->m_Polygons.m_nCount; nSourceRemaining > 0; --nSourceRemaining )
	{
		a3d_polygon_t* pPolygon = (a3d_polygon_t*)pOpening->m_Polygons.Next();
		a3d_polygon_t polygon;
		pOpening->m_Transform.TransformPolygon( &polygon, pPolygon );
		A3DVECTOR plane;
		m_Transform.TransformVector( plane, m_Plane );

		BOOL bWithin = TRUE;
		int nVertex = polygon.m_nVertices;
		while ( bWithin && nVertex-- > 0 )
		{
			m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
			for ( int nTargetRemaining = m_Polygons.m_nCount; nTargetRemaining > 0; --nTargetRemaining )
			{
				a3d_polygon_t* pTarget = (a3d_polygon_t*)m_Polygons.Next();
				a3d_polygon_t target;
				m_Transform.TransformPolygon( &target, pTarget );

				double dx = (double)target.m_Vertices[0][0] - polygon.m_Vertices[nVertex][0];
				double dy = (double)target.m_Vertices[0][1] - polygon.m_Vertices[nVertex][1];
				double dz = (double)target.m_Vertices[0][2] - polygon.m_Vertices[nVertex][2];
				double flDistanceSquared = ( dx * dx + dy * dy ) + dz * dz;
				if ( flDistanceSquared >= A3D_EPSILON_SQUARED )
				{
					double flDistance = ( dx * plane[0] + dy * plane[1] ) + dz * plane[2];
					if ( fabs( flDistance ) >= A3D_EPSILON )
					{
						bWithin = FALSE;
						break;
					}
				}

				int nClass = Geometry_ClassifyTrianglePoint( polygon.m_Vertices[nVertex], plane, target.m_Vertices );
				if ( nClass == TRIANGLE_POINT_OUTSIDE && target.m_nVertices == 4 )
				{
					A3DVERTEX triangle[3];
					memcpy( triangle[0], target.m_Vertices[0], sizeof( triangle[0] ) );
					memcpy( triangle[1], target.m_Vertices[2], sizeof( triangle[1] ) );
					memcpy( triangle[2], target.m_Vertices[3], sizeof( triangle[2] ) );
					nClass = Geometry_ClassifyTrianglePoint( polygon.m_Vertices[nVertex], plane, triangle );
				}
				if ( nClass < TRIANGLE_POINT_BOUNDARY || nClass > TRIANGLE_POINT_INTERIOR )
				{
					bWithin = FALSE;
					break;
				}
			}
		}
		if ( bWithin )
			return TRUE;
	}
	return FALSE;
}

int __cdecl Geometry_ClassifyTrianglePoint( const A3DVAL* pPoint, const A3DVAL* pNormal,
		const A3DVERTEX* pVertices )
{
	double normalX = fabs( (double)pNormal[0] );
	float normalY = (float)fabs( (double)pNormal[1] );
	float normalZ = (float)fabs( (double)pNormal[2] );
	int u = 0;
	int v = 1;

	if ( normalX > normalY && normalX > normalZ )
	{
		u = 1;
		v = 2;
	}
	else if ( normalY > normalX && normalY > normalZ )
	{
		v = 2;
	}

	double pointU = (double)pPoint[u] - pVertices[0][u];
	double firstU = (double)pVertices[1][u] - pVertices[0][u];
	float secondU = (float)( (double)pVertices[2][u] - pVertices[0][u] );
	float pointV = (float)( (double)pPoint[v] - pVertices[0][v] );
	float firstV = (float)( (double)pVertices[1][v] - pVertices[0][v] );
	float secondV = (float)( (double)pVertices[2][v] - pVertices[0][v] );
	double firstWeight;
	float secondWeight;

	if ( !( firstU < 0.0 || firstU > 0.0 ) )
	{
		if ( !( secondU < 0.0f || secondU > 0.0f ) )
			return TRIANGLE_POINT_OUTSIDE;

		double weight = pointU / secondU;
		secondWeight = (float)weight;
		if ( !( weight >= 0.0 ) || secondWeight > 1.0f )
			return TRIANGLE_POINT_OUTSIDE;
		if ( !( firstV < 0.0f || firstV > 0.0f ) )
			return TRIANGLE_POINT_OUTSIDE;

		firstWeight = ( (double)pointV - (double)secondWeight * secondV ) / firstV;
	}
	else
	{
		double determinant = (double)secondV * firstU - (double)firstV * secondU;
		if ( !( determinant < 0.0 || determinant > 0.0 ) )
			return TRIANGLE_POINT_OUTSIDE;

		secondWeight = (float)( ( (double)pointV * firstU - (double)firstV * pointU ) / determinant );
		if ( !( secondWeight >= 0.0f ) || secondWeight > 1.0f )
			return TRIANGLE_POINT_OUTSIDE;

		firstWeight = ( pointU - (double)secondWeight * secondU ) / firstU;
	}

	bool firstNonNegative = firstWeight >= 0.0;
	if ( !firstNonNegative || firstWeight + secondWeight > 1.0 )
		return TRIANGLE_POINT_OUTSIDE;

	double totalWeight = firstWeight + secondWeight;
	if ( !( fabs( totalWeight - 1.0 ) >= A3D_EPSILON ) ||
		!( totalWeight >= A3D_EPSILON ) )
		return TRIANGLE_POINT_BOUNDARY;

	return TRIANGLE_POINT_INTERIOR;
}

BOOL __cdecl Geometry_PointOnSegment( const A3DVAL* pPoint, const A3DVERTEX* pEndpoints )
{
	double differenceX = fabs( (double)pEndpoints[0][0] - pEndpoints[1][0] );
	float differenceY = (float)( (double)pEndpoints[0][1] - pEndpoints[1][1] );
	float differenceZ = (float)( (double)pEndpoints[0][2] - pEndpoints[1][2] );
	BOOL firstProjection = FALSE;
	BOOL secondProjection = FALSE;

	if ( !( differenceX >= A3D_EPSILON ) && !( fabs( (double)differenceY ) >= A3D_EPSILON ) )
	{
		double pointZ = (double)pPoint[2] - pEndpoints[0][2];
		double edgeZ = (double)pEndpoints[1][2] - pEndpoints[0][2];
		{
			double pointX = (double)pPoint[0] - pEndpoints[0][0];
			double edgeX = (double)pEndpoints[1][0] - pEndpoints[0][0];
			float roundedPointX = (float)pointX;
			float roundedPointZ = (float)pointZ;
			float roundedEdgeX = (float)edgeX;
			float roundedEdgeZ = (float)edgeZ;
			double cross = pointZ * roundedEdgeX - pointX * roundedEdgeZ;

			firstProjection = !( cross > A3D_EPSILON ) && cross >= -A3D_EPSILON &&
				(double)roundedPointX * roundedEdgeX >= -A3D_EPSILON &&
				(double)roundedPointZ * roundedEdgeZ >= -A3D_EPSILON &&
				!( (double)roundedPointX * roundedPointX + (double)roundedPointZ * roundedPointZ >
				(double)roundedEdgeX * roundedEdgeX + (double)roundedEdgeZ * roundedEdgeZ );
		}
		{
			double pointY = (double)pPoint[1] - pEndpoints[0][1];
			double edgeY = (double)pEndpoints[1][1] - pEndpoints[0][1];
			float roundedPointY = (float)pointY;
			float roundedPointZ = (float)pointZ;
			float roundedEdgeY = (float)edgeY;
			float roundedEdgeZ = (float)edgeZ;
			double cross = pointZ * roundedEdgeY - pointY * roundedEdgeZ;

			secondProjection = !( cross > A3D_EPSILON ) && cross >= -A3D_EPSILON &&
				(double)roundedPointY * roundedEdgeY >= -A3D_EPSILON &&
				(double)roundedPointZ * roundedEdgeZ >= -A3D_EPSILON &&
				!( (double)roundedPointY * roundedPointY + (double)roundedPointZ * roundedPointZ >
				(double)roundedEdgeY * roundedEdgeY + (double)roundedEdgeZ * roundedEdgeZ );
		}
	}
	else if ( !( differenceX >= A3D_EPSILON ) &&
		!( fabs( (double)differenceZ ) >= A3D_EPSILON ) )
	{
		double pointY = (double)pPoint[1] - pEndpoints[0][1];
		double edgeY = (double)pEndpoints[1][1] - pEndpoints[0][1];
		{
			double pointX = (double)pPoint[0] - pEndpoints[0][0];
			double edgeX = (double)pEndpoints[1][0] - pEndpoints[0][0];
			float roundedPointX = (float)pointX;
			float roundedPointY = (float)pointY;
			float roundedEdgeX = (float)edgeX;
			float roundedEdgeY = (float)edgeY;
			double cross = pointY * roundedEdgeX - pointX * roundedEdgeY;

			firstProjection = !( cross > A3D_EPSILON ) && cross >= -A3D_EPSILON &&
				(double)roundedPointX * roundedEdgeX >= -A3D_EPSILON &&
				(double)roundedPointY * roundedEdgeY >= -A3D_EPSILON &&
				!( (double)roundedPointX * roundedPointX + (double)roundedPointY * roundedPointY >
				(double)roundedEdgeX * roundedEdgeX + (double)roundedEdgeY * roundedEdgeY );
		}
		{
			double pointZ = (double)pPoint[2] - pEndpoints[0][2];
			double edgeZ = (double)pEndpoints[1][2] - pEndpoints[0][2];
			float roundedPointZ = (float)pointZ;
			float roundedPointY = (float)pointY;
			float roundedEdgeZ = (float)edgeZ;
			float roundedEdgeY = (float)edgeY;
			double cross = pointY * roundedEdgeZ - pointZ * roundedEdgeY;

			secondProjection = !( cross > A3D_EPSILON ) && cross >= -A3D_EPSILON &&
				(double)roundedPointZ * roundedEdgeZ >= -A3D_EPSILON &&
				(double)roundedPointY * roundedEdgeY >= -A3D_EPSILON &&
				!( (double)roundedPointZ * roundedPointZ + (double)roundedPointY * roundedPointY >
				(double)roundedEdgeZ * roundedEdgeZ + (double)roundedEdgeY * roundedEdgeY );
		}
	}
	else if ( !( fabs( (double)differenceY ) >= A3D_EPSILON ) &&
		!( fabs( (double)differenceZ ) >= A3D_EPSILON ) )
	{
		firstProjection = Geometry_PointOnSegmentProjection( pPoint, 1, 0, pEndpoints );
		secondProjection = Geometry_PointOnSegmentProjection( pPoint, 2, 0, pEndpoints );
	}

	if ( !firstProjection || !secondProjection )
		return FALSE;

	for ( int endpoint = 0; endpoint < 2; endpoint++ )
	{
		double x = (double)pPoint[0] - pEndpoints[endpoint][0];
		double y = (double)pPoint[1] - pEndpoints[endpoint][1];
		double z = (double)pPoint[2] - pEndpoints[endpoint][2];
		float roundedX = (float)x;
		float roundedY = (float)y;
		double distanceSquared = ( x * roundedX + y * roundedY ) + z * z;
		if ( !( distanceSquared >= A3D_EPSILON_SQUARED ) )
			return FALSE;
	}

	return TRUE;
}

BOOL __cdecl Geometry_PointOnSegmentProjection( const A3DVAL* pPoint, int firstComponent,
		int secondComponent, const A3DVERTEX* pEndpoints )
{
	double pointU = (double)pPoint[firstComponent] - pEndpoints[0][firstComponent];
	double pointV = (double)pPoint[secondComponent] - pEndpoints[0][secondComponent];
	float edgeU = (float)( (double)pEndpoints[1][firstComponent] - pEndpoints[0][firstComponent] );
	float edgeV = (float)( (double)pEndpoints[1][secondComponent] - pEndpoints[0][secondComponent] );
	float roundedPointU = (float)pointU;
	float roundedPointV = (float)pointV;
	double cross = pointV * edgeU - pointU * edgeV;

	if ( cross > A3D_EPSILON || !( cross >= -A3D_EPSILON ) )
		return FALSE;
	if ( !( (double)roundedPointU * edgeU >= -A3D_EPSILON ) ||
		!( (double)roundedPointV * edgeV >= -A3D_EPSILON ) )
		return FALSE;

	return !( (double)roundedPointU * roundedPointU + (double)roundedPointV * roundedPointV >
		(double)edgeU * edgeU + (double)edgeV * edgeV );
}

BOOL __cdecl Geometry_SameSegment( const A3DVERTEX* pFirst, const A3DVERTEX* pSecond )
{
	for ( int reversed = 0; reversed < 2; reversed++ )
	{
		double firstX = (double)pFirst[0][0] - pSecond[reversed][0];
		double firstY = (double)pFirst[0][1] - pSecond[reversed][1];
		double firstZ = (double)pFirst[0][2] - pSecond[reversed][2];
		float roundedFirstX = (float)firstX;
		double firstDistanceSquared;
		if ( reversed )
		{
			float roundedFirstZ = (float)firstZ;
			firstDistanceSquared = ( firstX * roundedFirstX + firstY * firstY ) + firstZ * roundedFirstZ;
		}
		else
		{
			float roundedFirstY = (float)firstY;
			firstDistanceSquared = ( firstX * roundedFirstX + firstY * roundedFirstY ) + firstZ * firstZ;
		}

		double secondX = (double)pFirst[1][0] - pSecond[1 - reversed][0];
		double secondY = (double)pFirst[1][1] - pSecond[1 - reversed][1];
		double secondZ = (double)pFirst[1][2] - pSecond[1 - reversed][2];
		float roundedSecondX = (float)secondX;
		float roundedSecondY = (float)secondY;
		float roundedSecondZ = (float)secondZ;
		double secondDistanceSquared = ( secondX * roundedSecondX + secondY * roundedSecondY ) +
			secondZ * roundedSecondZ;

		if ( !( firstDistanceSquared >= A3D_EPSILON_SQUARED ) &&
			!( secondDistanceSquared >= A3D_EPSILON_SQUARED ) )
			return TRUE;
	}

	return FALSE;
}

CA3dMaterialReference::CA3dMaterialReference() : m_pMaterial( NULL )
{
}

CA3dMaterialReference::~CA3dMaterialReference()
{
	if ( m_pMaterial )
		m_pMaterial->Release();
}

HRESULT CA3dMaterialReference::Set( IA3dMaterial* pMaterial )
{
	if ( !pMaterial )
		return E_INVALIDARG;

	m_pMaterial = pMaterial;
	pMaterial->AddRef();
	return S_OK;
}

HRESULT CA3dMaterialReference::Clone( IA3dMaterial** ppMaterial )
{
	if ( !ppMaterial )
		return E_INVALIDARG;

	if ( !m_pMaterial )
	{
		*ppMaterial = NULL;
		return S_OK;
	}

	CA3dMaterial* pCopy = new ( std::nothrow ) CA3dMaterial( *(CA3dMaterial*)m_pMaterial );
	pCopy->AddRef();
	*ppMaterial = pCopy;
	return S_OK;
}

HRESULT CA3dMaterialReference::Copy( const CA3dMaterialReference* pReference )
{
	if ( pReference->m_pMaterial )
		return Set( pReference->m_pMaterial );

	return S_OK;
}

CA3dName::CA3dName()
{
	memset( m_szName, 0, sizeof( m_szName ) );
}

CA3dName::~CA3dName()
{
}

HRESULT CA3dName::Format( const char* pszName, int nId )
{
	sprintf( m_szName, "%s-%d", pszName, nId );
	return S_OK;
}

HRESULT CA3dName::Set( const char* pszName )
{
	if ( !pszName )
		return E_INVALIDARG;

	memcpy( m_szName, pszName, sizeof( m_szName ) );
	return S_OK;
}

HRESULT CA3dName::Get( char* pszName, DWORD dwSize )
{
	if ( !pszName )
		return E_INVALIDARG;

	if ( dwSize < sizeof( m_szName ) )
		return E_INVALIDARG;

	memcpy( pszName, m_szName, sizeof( m_szName ) );
	return S_OK;
}

CA3dGeometryInstance::CA3dGeometryInstance()
{
	m_Polygons.m_pFirst = NULL;
	m_Polygons.m_pLast = NULL;
	m_Polygons.m_pCurrent = NULL;
	m_Polygons.m_nCount = 0;
}

CA3dGeometryInstance::~CA3dGeometryInstance()
{
	Clear();
	m_Polygons.Clear();
}

void CA3dGeometryInstance::Clear()
{
	int nPolygons = m_Polygons.m_nCount;
	m_Polygons.m_pCurrent = m_Polygons.m_pFirst;

	for ( int i = 0; i < nPolygons; i++ )
	{
		void* pPolygon = NULL;
		if ( m_Polygons.m_pCurrent )
		{
			pPolygon = m_Polygons.m_pCurrent->m_pObject;
			m_Polygons.m_pCurrent = m_Polygons.m_pCurrent->m_pNext;
		}
		operator delete( pPolygon );
	}

	m_Polygons.Clear();
}

void CA3dGeometryInstance::CopyPolygons( CA3dObjectList* pPolygons )
{
	int nPolygons = pPolygons->m_nCount;
	pPolygons->m_pCurrent = pPolygons->m_pFirst;

	for ( int i = 0; i < nPolygons; i++ )
	{
		void* pPolygon = NULL;
		if ( pPolygons->m_pCurrent )
		{
			pPolygon = pPolygons->m_pCurrent->m_pObject;
			pPolygons->m_pCurrent = pPolygons->m_pCurrent->m_pNext;
		}

		a3d_polygon_t* pCopy = (a3d_polygon_t*)operator new( sizeof( *pCopy ), std::nothrow );
		memcpy( pCopy, pPolygon, sizeof( *pCopy ) );
		m_Polygons.Append( pCopy );
	}
}

CA3dGeometryBuilder::CA3dGeometryBuilder( int nSceneType )
{
	m_Polygons.m_pFirst = NULL;
	m_Polygons.m_pLast = NULL;
	m_Polygons.m_pCurrent = NULL;
	m_Polygons.m_nCount = 0;
	m_Edges.m_pFirst = NULL;
	m_Edges.m_pLast = NULL;
	m_Edges.m_pCurrent = NULL;
	m_Edges.m_nCount = 0;

	m_nSceneType = nSceneType;
	m_bValidated = FALSE;
	m_pNormal = NULL;

	m_Polygon.m_nVertices = 0;

	for ( int i = 0; i < 4; i++ )
	{
		m_Polygon.m_Vertices[i][0] = 0.0f;
		m_Polygon.m_Vertices[i][1] = 0.0f;
		m_Polygon.m_Vertices[i][2] = 0.0f;
		m_Polygon.m_Vertices[i][3] = 1.0f;
	}
	memset( m_Polygon.m_Normal, 0, sizeof( m_Polygon.m_Normal ) );
}

CA3dGeometryBuilder::~CA3dGeometryBuilder()
{
	Clear();
	m_Edges.Clear();
	m_Polygons.Clear();
}

HRESULT CA3dGeometryBuilder::Begin( DWORD dwMode )
{
	m_Polygon.m_nVertices = 0;

	switch ( dwMode )
	{
	case A3D_LINES:
	case A3D_SUB_LINES:
		if ( m_nSceneType == A3D_SCENE_3D )
			return A3DERROR_MIXING_2D_AND_3D_MODES;
		m_Polygon.m_nVertices = A3D_LINES;
		break;

	case A3D_TRIANGLES:
	case A3D_SUB_TRIANGLES:
		if ( m_nSceneType == A3D_SCENE_2D )
			return A3DERROR_MIXING_2D_AND_3D_MODES;
		m_Polygon.m_nVertices = A3D_TRIANGLES;
		break;

	case A3D_QUADS:
	case A3D_SUB_QUADS:
		if ( m_nSceneType == A3D_SCENE_2D )
			return A3DERROR_MIXING_2D_AND_3D_MODES;
		m_Polygon.m_nVertices = A3D_QUADS;
		break;

	default:
		return A3DERROR_INVALID_BEGIN_MODE;
	}

	m_nVertex = 0;
	return S_OK;
}

HRESULT CA3dGeometryBuilder::End()
{
	if ( !m_Polygon.m_nVertices )
		return A3DERROR_END_BEFORE_VALID_BEGIN_BLOCK;

	m_Polygon.m_nVertices = 0;
	return S_OK;
}

HRESULT CA3dGeometryBuilder::AddVertex3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	if ( !m_Polygon.m_nVertices )
		return A3DERROR_INVALID_BEGIN_MODE;

	m_Polygon.m_Vertices[m_nVertex][0] = x;
	m_Polygon.m_Vertices[m_nVertex][1] = y;
	m_Polygon.m_Vertices[m_nVertex][2] = z;

	if ( m_nSceneType == A3D_SCENE_2D && m_Polygons.m_nCount == 1 )
		return A3DERROR_2DWALL_REQUIRES_EXACTLY_ONE_LINE;

	FinishPolygon();
	return S_OK;
}

HRESULT CA3dGeometryBuilder::AddVertex3fv( const A3DVAL* pVertex )
{
	if ( !pVertex )
		return A3DERROR_INVALID_ARGUMENT;

	if ( !m_Polygon.m_nVertices )
		return A3DERROR_INVALID_BEGIN_MODE;

	if ( m_nSceneType == A3D_SCENE_2D && m_Polygons.m_nCount == 1 )
		return A3DERROR_2DWALL_REQUIRES_EXACTLY_ONE_LINE;

	for ( int i = 0; i < 3; i++ )
		memcpy( &m_Polygon.m_Vertices[m_nVertex][i], &pVertex[i], sizeof( *pVertex ) );

	FinishPolygon();
	return S_OK;
}

HRESULT CA3dGeometryBuilder::RemovePolygon( int nPolygon )
{
	CA3dListNode* pNode = NULL;
	if ( nPolygon >= 0 && nPolygon < m_Polygons.m_nCount )
	{
		pNode = m_Polygons.m_pFirst;
		for ( int i = 0; i < nPolygon; i++ )
			pNode = pNode->m_pNext;
	}

	void* pPolygon = NULL;
	if ( pNode )
	{
		m_Polygons.m_pCurrent = pNode;
		pPolygon = pNode->m_pObject;
	}
	if ( !pPolygon )
		return A3DERROR_INVALID_INDEX;

	operator delete( pPolygon );
	pNode = NULL;
	if ( nPolygon >= 0 && nPolygon < m_Polygons.m_nCount )
	{
		pNode = m_Polygons.m_pFirst;
		for ( int i = 0; i < nPolygon; i++ )
			pNode = pNode->m_pNext;
	}

	if ( pNode )
	{
		if ( m_Polygons.m_pFirst == pNode )
			m_Polygons.m_pFirst = pNode->m_pNext;
		if ( m_Polygons.m_pLast == pNode )
			m_Polygons.m_pLast = pNode->m_pPrevious;
		if ( m_Polygons.m_pCurrent == pNode )
		{
			m_Polygons.m_pCurrent = pNode->m_pPrevious;
			if ( !m_Polygons.m_pCurrent )
				m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
		}
		if ( pNode->m_pNext )
			pNode->m_pNext->m_pPrevious = pNode->m_pPrevious;
		if ( pNode->m_pPrevious )
			pNode->m_pPrevious->m_pNext = pNode->m_pNext;
		operator delete( pNode );
	}

	m_Polygons.m_nCount--;
	return S_OK;
}

int CA3dGeometryBuilder::GetPolygonCount()
{
	return m_Polygons.m_nCount;
}

HRESULT CA3dGeometryBuilder::SetVertex( int nPolygon, int nVertex, A3DVAL x, A3DVAL y, A3DVAL z )
{
	if ( nPolygon < 0 || nPolygon >= m_Polygons.m_nCount )
		return A3DERROR_INVALID_PRIMITIVE_INDEX;

	CA3dListNode* pNode = m_Polygons.m_pFirst;
	for ( int i = 0; i < nPolygon; i++ )
		pNode = pNode->m_pNext;

	a3d_polygon_t* pPolygon = NULL;
	if ( pNode )
	{
		m_Polygons.m_pCurrent = pNode;
		pPolygon = (a3d_polygon_t*)pNode->m_pObject;
	}

	if ( nVertex < 0 || nVertex >= pPolygon->m_nVertices )
		return A3DERROR_INVALID_VERTEX_INDEX;

	pPolygon->m_Vertices[nVertex][0] = x;
	pPolygon->m_Vertices[nVertex][1] = y;
	pPolygon->m_Vertices[nVertex][2] = z;
	return S_OK;
}

HRESULT CA3dGeometryBuilder::GetVertex( int nPolygon, int nVertex, A3DVAL* pVertex )
{
	if ( !pVertex )
		return A3DERROR_INVALID_ARGUMENT;
	if ( nPolygon < 0 || nPolygon >= m_Polygons.m_nCount )
		return A3DERROR_INVALID_PRIMITIVE_INDEX;

	CA3dListNode* pNode = m_Polygons.m_pFirst;
	for ( int i = 0; i < nPolygon; i++ )
		pNode = pNode->m_pNext;

	a3d_polygon_t* pPolygon = NULL;
	if ( pNode )
	{
		m_Polygons.m_pCurrent = pNode;
		pPolygon = (a3d_polygon_t*)pNode->m_pObject;
	}
	if ( nVertex < 0 || nVertex >= pPolygon->m_nVertices )
		return A3DERROR_INVALID_VERTEX_INDEX;

	for ( int i = 0; i < 3; i++ )
		memcpy( &pVertex[i], &pPolygon->m_Vertices[nVertex][i], sizeof( *pVertex ) );
	return S_OK;
}

HRESULT CA3dGeometryBuilder::Clear()
{
	ClearPolygons();
	ClearEdges();
	m_pNormal = NULL;
	return S_OK;
}

void CA3dGeometryBuilder::ClearEdges()
{
	int nEdges = m_Edges.m_nCount;
	m_Edges.m_pCurrent = m_Edges.m_pFirst;
	for ( int i = 0; i < nEdges; i++ )
	{
		void* pEdge = NULL;
		if ( m_Edges.m_pCurrent )
		{
			pEdge = m_Edges.m_pCurrent->m_pObject;
			m_Edges.m_pCurrent = m_Edges.m_pCurrent->m_pNext;
		}
		operator delete( pEdge );
	}
	m_Edges.Clear();
}

void CA3dGeometryBuilder::ClearPolygons()
{
	int nPolygons = m_Polygons.m_nCount;
	m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
	for ( int i = 0; i < nPolygons; i++ )
	{
		void* pPolygon = NULL;
		if ( m_Polygons.m_pCurrent )
		{
			pPolygon = m_Polygons.m_pCurrent->m_pObject;
			m_Polygons.m_pCurrent = m_Polygons.m_pCurrent->m_pNext;
		}
		operator delete( pPolygon );
	}
	m_Polygons.Clear();
}

void CA3dGeometryBuilder::AddEdge( const a3d_polygon_t* pPolygon, int nStart, int nEnd )
{
	a3d_edge_t* pEdge = (a3d_edge_t*)operator new( sizeof( *pEdge ), std::nothrow );
	memcpy( pEdge->vertices[0], pPolygon->m_Vertices[nStart], sizeof( A3DVERTEX ) );
	memcpy( pEdge->vertices[1], pPolygon->m_Vertices[nEnd], sizeof( A3DVERTEX ) );
	m_Edges.Append( pEdge );
}

void CA3dGeometryBuilder::SetEdgeFlag( a3d_polygon_t* pPolygon, DWORD dwEdge, BOOL bSet )
{
	DWORD dwMask = dwEdge;
	switch ( dwEdge )
	{
	case 0: dwMask = 1; break;
	case 1: dwMask = 2; break;
	case 2: dwMask = 4; break;
	case 4: dwMask = 8; break;
	}

	if ( bSet )
	{
		if ( !( pPolygon->m_dwEdgeFlags & dwMask ) )
			pPolygon->m_dwEdgeFlags ^= dwMask;
	}
	else
	{
		if ( pPolygon->m_dwEdgeFlags & dwMask )
			pPolygon->m_dwEdgeFlags ^= dwMask;
	}
}

CPlex* __stdcall CPlex::Create( CPlex** ppBlocks,
		DWORD nItems, DWORD dwItemSize )
{
	DWORD dwBytes = nItems * dwItemSize + sizeof( CPlex );
	CPlex* pBlock = (CPlex*)operator new( dwBytes, std::nothrow );
	pBlock->pNext = *ppBlocks;
	*ppBlocks = pBlock;
	return pBlock;
}

void CPlex::FreeDataChain()
{
	CPlex* pBlocks = this;
	while ( pBlocks )
	{
		CPlex* pNextBlock = pBlocks->pNext;
		operator delete( pBlocks );
		pBlocks = pNextBlock;
	}
}

void CA3dGeometryBuilder::AddPolygonEdges( a3d_polygon_t* pPolygon )
{
	switch ( pPolygon->m_dwEdgeFlags & 15 )
	{
	case 0:
		return;
	case 1:
		AddEdge( pPolygon, 0, 1 );
		if ( pPolygon->m_nVertices == 4 )
			AddEdge( pPolygon, 3, 0 );
		else
			AddEdge( pPolygon, 2, 0 );
		return;
	case 2:
		AddEdge( pPolygon, 0, 1 );
		AddEdge( pPolygon, 1, 2 );
		return;
	case 3:
		AddEdge( pPolygon, 0, 1 );
		AddEdge( pPolygon, 1, 2 );
		if ( pPolygon->m_nVertices == 4 )
			AddEdge( pPolygon, 3, 0 );
		else
			AddEdge( pPolygon, 2, 0 );
		return;
	case 4:
		AddEdge( pPolygon, 1, 2 );
		if ( pPolygon->m_nVertices == 4 )
			AddEdge( pPolygon, 2, 3 );
		else
			AddEdge( pPolygon, 2, 0 );
		return;
	case 6:
		AddEdge( pPolygon, 0, 1 );
		AddEdge( pPolygon, 1, 2 );
		if ( pPolygon->m_nVertices == 4 )
			AddEdge( pPolygon, 2, 3 );
		else
			AddEdge( pPolygon, 2, 0 );
		return;
	case 8:
		AddEdge( pPolygon, 2, 3 );
		AddEdge( pPolygon, 3, 0 );
		return;
	case 9:
		AddEdge( pPolygon, 0, 1 );
		AddEdge( pPolygon, 2, 3 );
		AddEdge( pPolygon, 3, 0 );
		return;
	case 12:
		AddEdge( pPolygon, 1, 2 );
		AddEdge( pPolygon, 2, 3 );
		AddEdge( pPolygon, 3, 0 );
		return;
	default:
		AddEdge( pPolygon, 0, 1 );
		AddEdge( pPolygon, 1, 2 );
		if ( pPolygon->m_nVertices == 4 )
		{
			AddEdge( pPolygon, 2, 3 );
			AddEdge( pPolygon, 3, 0 );
		}
		else
		{
			AddEdge( pPolygon, 2, 0 );
		}
		return;
	}
}

void CA3dGeometryBuilder::CheckEdgeIntersections()
{
	int nEdges = m_Edges.m_nCount;
	m_Edges.m_pCurrent = m_Edges.m_pFirst;
	for ( int i = 0; i < nEdges; i++ )
	{
		a3d_edge_t* pEdge = NULL;
		if ( m_Edges.m_pCurrent )
		{
			pEdge = (a3d_edge_t*)m_Edges.m_pCurrent->m_pObject;
			m_Edges.m_pCurrent = m_Edges.m_pCurrent->m_pNext;
		}
		int nPolygons = m_Polygons.m_nCount;
		m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
		for ( int j = 0; j < nPolygons; j++ )
		{
			a3d_polygon_t* pPolygon = NULL;
			if ( m_Polygons.m_pCurrent )
			{
				pPolygon = (a3d_polygon_t*)m_Polygons.m_pCurrent->m_pObject;
				m_Polygons.m_pCurrent = m_Polygons.m_pCurrent->m_pNext;
			}
			CheckEdgeIntersection( pPolygon, 0, 1, pEdge );
			CheckEdgeIntersection( pPolygon, 1, 2, pEdge );
			if ( pPolygon->m_nVertices == 3 )
			{
				CheckEdgeIntersection( pPolygon, 2, 0, pEdge );
			}
			else
			{
				CheckEdgeIntersection( pPolygon, 2, 3, pEdge );
				CheckEdgeIntersection( pPolygon, 3, 0, pEdge );
			}
		}
	}
}

HRESULT CA3dGeometryBuilder::Validate()
{
	if ( m_Polygons.m_nCount <= 0 )
		return A3DERROR_NO_PRIMITIVES_DEFINED;
	if ( m_nSceneType == A3D_SCENE_3D && FAILED( CheckCoplanarity() ) )
		return A3DERROR_PRIMITIVES_NON_PLANAR;

	CA3dObjectList polygons = { NULL, NULL, NULL, 0 };
	int nPolygons = m_Polygons.m_nCount;
	m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
	polygons.Clear();
	for ( int i = 0; i < nPolygons; i++ )
	{
		void* pPolygon = NULL;
		if ( m_Polygons.m_pCurrent )
		{
			pPolygon = m_Polygons.m_pCurrent->m_pObject;
			m_Polygons.m_pCurrent = m_Polygons.m_pCurrent->m_pNext;
		}
		polygons.Append( pPolygon );
	}

	ClearEdges();
	nPolygons = m_Polygons.m_nCount;
	m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
	for ( int i = 0; i < nPolygons; i++ )
	{
		a3d_polygon_t* pPolygon = NULL;
		if ( m_Polygons.m_pCurrent )
		{
			pPolygon = (a3d_polygon_t*)m_Polygons.m_pCurrent->m_pObject;
			m_Polygons.m_pCurrent = m_Polygons.m_pCurrent->m_pNext;
		}
		BOOL bAdjacent = FALSE;
		pPolygon->m_dwEdgeFlags = pPolygon->m_nVertices == 4 ? 15 : 7;

		for ( int j = pPolygon->m_nVertices - 1; j >= 0; j-- )
		{
			int nOtherPolygons = polygons.m_nCount;
			polygons.m_pCurrent = polygons.m_pFirst;
			for ( int k = 0; k < nOtherPolygons; k++ )
			{
				a3d_polygon_t* pOther = NULL;
				if ( polygons.m_pCurrent )
				{
					pOther = (a3d_polygon_t*)polygons.m_pCurrent->m_pObject;
					polygons.m_pCurrent = polygons.m_pCurrent->m_pNext;
				}
				if ( pOther == pPolygon )
					continue;

				int nClassification = Geometry_ClassifyTrianglePoint( pPolygon->m_Vertices[j],
					m_pNormal, pOther->m_Vertices );
				if ( nClassification == 2 && pOther->m_nVertices == 4 )
				{
					A3DVERTEX triangle[3];
					memcpy( triangle[0], pOther->m_Vertices[0], sizeof( A3DVERTEX ) );
					memcpy( triangle[1], pOther->m_Vertices[2], sizeof( A3DVERTEX ) );
					memcpy( triangle[2], pOther->m_Vertices[3], sizeof( A3DVERTEX ) );
					nClassification = Geometry_ClassifyTrianglePoint( pPolygon->m_Vertices[j],
						m_pNormal, triangle );
				}

				switch ( nClassification )
				{
				case 0:
					bAdjacent = TRUE;
					SetEdgeFlag( pPolygon, j, FALSE );
					break;
				case 1:
					polygons.Clear();
					return A3DERROR_PRIMITIVES_OVERLAPPING;
				case 2:
					SetEdgeFlag( pPolygon, j, TRUE );
					break;
				}
			}
		}

		if ( !bAdjacent && m_Polygons.m_nCount > 1 )
		{
			polygons.Clear();
			return A3DERROR_PRIMITIVES_NOT_ADJACENT;
		}
		AddPolygonEdges( pPolygon );
	}

	CheckEdgeIntersections();
	m_bValidated = TRUE;
	polygons.Clear();
	return S_OK;
}

HRESULT CA3d::InitializeListenerState()
{
	memset( m_vPosition, 0, sizeof( m_vPosition ) );
	m_vPosition[3] = 1.0f;
	memset( m_vOrientationAngles, 0, sizeof( m_vOrientationAngles ) );
	memset( m_vVelocity, 0, sizeof( m_vVelocity ) );
	m_vFront[0] = 0.0f;
	m_vFront[1] = 0.0f;
	m_vFront[2] = -1.0f;
	memset( m_vTop, 0, sizeof( m_vTop ) );
	m_vTop[1] = 1.0f;
	Matrix_Identity( m_ListenerMatrix );
	m_vLeftEar[0] = -0.07f;
	m_vLeftEar[1] = 0.0f;
	m_vLeftEar[2] = 0.0f;
	m_vRightEar[0] = 0.07f;
	m_vRightEar[1] = 0.0f;
	m_vRightEar[2] = 0.0f;
	m_bListenerInitialized = TRUE;
	return S_OK;
}

HRESULT CA3d::ShutdownListenerState()
{
	m_bListenerInitialized = FALSE;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::SetPosition3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	A3DVAL position[3] = { x, y, z };
	return SetPosition3fv( position );
}

HRESULT STDMETHODCALLTYPE CA3d::SetPosition3fv( LPA3DVAL pPosition )
{
	for ( int i = 0; i < 3; i++ )
		memcpy( &m_vPosition[i], &pPosition[i], sizeof( *pPosition ) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::SetOrientationAngles3f( A3DVAL yaw, A3DVAL pitch, A3DVAL roll )
{
	A3DVAL angles[3];
	memcpy( &angles[0], &yaw, sizeof( yaw ) );
	memcpy( &angles[1], &pitch, sizeof( pitch ) );
	memcpy( &angles[2], &roll, sizeof( roll ) );
	return SetOrientationAngles3fv( angles );
}

HRESULT STDMETHODCALLTYPE CA3d::SetOrientationAngles3fv( LPA3DVAL pAngles )
{
	for ( int i = 0; i < 3; i++ )
		memcpy( &m_vOrientationAngles[i], &pAngles[i], sizeof( *pAngles ) );
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3d::SetOrientation6f( A3DVAL frontX, A3DVAL frontY, A3DVAL frontZ,
		A3DVAL topX, A3DVAL topY, A3DVAL topZ )
{
	A3DVAL orientation[6];
	memcpy( &orientation[0], &frontX, sizeof( frontX ) );
	memcpy( &orientation[1], &frontY, sizeof( frontY ) );
	memcpy( &orientation[2], &frontZ, sizeof( frontZ ) );
	memcpy( &orientation[3], &topX, sizeof( topX ) );
	memcpy( &orientation[4], &topY, sizeof( topY ) );
	memcpy( &orientation[5], &topZ, sizeof( topZ ) );
	return SetOrientation6fv( orientation );
}

HRESULT STDMETHODCALLTYPE CA3d::SetOrientation6fv( LPA3DVAL pOrientation )
{
	for ( int i = 0; i < 3; i++ )
		memcpy( &m_vFront[i], &pOrientation[i], sizeof( *pOrientation ) );
	for ( int i = 0; i < 3; i++ )
		memcpy( &m_vTop[i], &pOrientation[i + 3], sizeof( *pOrientation ) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::SetVelocity3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	A3DVAL velocity[3] = { x, y, z };
	return SetVelocity3fv( velocity );
}

HRESULT STDMETHODCALLTYPE CA3d::SetVelocity3fv( LPA3DVAL pVelocity )
{
	for ( int i = 0; i < 3; i++ )
		memcpy( &m_vVelocity[i], &pVelocity[i], sizeof( *pVelocity ) );
	return S_OK;
}

void CA3d::GetListenerVelocity( float velocity[3] )
{
	A3DMATRIX matrix;
	memcpy( matrix, m_ListenerMatrix, sizeof( matrix ) );
	matrix[12] = 0.0f;
	matrix[13] = 0.0f;
	matrix[14] = 0.0f;
	Matrix_TransformVector( m_vVelocity, matrix, velocity );
}

void CA3d::SetSourceDopplerScale( A3DVAL flScale )
{
	if ( m_Sources.m_nCount < 1 )
		return;

	int nSources = m_Sources.m_nCount;
	m_Sources.m_pCurrent = m_Sources.m_pFirst;
	for ( int i = 0; i < nSources; i++ )
	{
		IA3dSource202* pSource = NULL;
		if ( m_Sources.m_pCurrent )
		{
			pSource = (IA3dSource202*)m_Sources.m_pCurrent->m_pObject;
			m_Sources.m_pCurrent = m_Sources.m_pCurrent->m_pNext;
		}
		pSource->SetDopplerScale( flScale );
	}
}

void CA3d::SetSourceDistanceModelScale( A3DVAL flScale )
{
	if ( m_Sources.m_nCount < 1 )
		return;

	int nSources = m_Sources.m_nCount;
	m_Sources.m_pCurrent = m_Sources.m_pFirst;
	for ( int i = 0; i < nSources; i++ )
	{
		IA3dSource202* pSource = NULL;
		if ( m_Sources.m_pCurrent )
		{
			pSource = (IA3dSource202*)m_Sources.m_pCurrent->m_pObject;
			m_Sources.m_pCurrent = m_Sources.m_pCurrent->m_pNext;
		}
		pSource->SetDistanceModelScale( flScale );
	}
}

static DWORD g_cMaterialNames = 0;

CA3dList::CA3dList( CA3d* pOwner )
{
	m_cReferences = 0;
	m_pOwner = pOwner;
	m_pCommands = NULL;
}

CA3dList::~CA3dList()
{
	delete m_pCommands;
}

HRESULT STDMETHODCALLTYPE CA3dList::QueryInterface( REFIID iid, void** ppObject )
{
	if ( !ppObject )
		return E_INVALIDARG;

	if ( !memcmp( &iid, &IID_IA3dList, sizeof( IID ) ) ||
		!memcmp( &iid, &IID_IUnknown, sizeof( IID ) ) )
		*ppObject = this;

	if ( !*ppObject )
		return E_NOINTERFACE;

	( (IUnknown*)*ppObject )->AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE CA3dList::AddRef()
{
	return ++m_cReferences;
}

ULONG STDMETHODCALLTYPE CA3dList::Release()
{
	if ( --m_cReferences )
		delete this;

	return 0;
}

HRESULT STDMETHODCALLTYPE CA3dList::Begin()
{
	if ( m_pCommands )
		return E_FAIL;

	m_pBeginBuffer = m_pOwner->m_pCommandBuffer;
	m_nBeginCommand = m_pBeginBuffer->m_iCursor;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dList::End()
{
	m_pEndBuffer = m_pOwner->m_pCommandBuffer;
	m_nEndCommand = m_pEndBuffer->m_iCursor;
	int nCapacity = (int)m_pOwner->m_nCommandCapacity;
	int nPoolDistance = 0;
	int nCommands;

	if ( m_pBeginBuffer == m_pEndBuffer )
	{
		nCommands = (int)( (DWORD)m_nEndCommand - (DWORD)m_nBeginCommand );
	}
	else
	{
		int nBeginPool = 0;
		CA3dListNode* pNode = m_pOwner->m_GeometryPools.m_pFirst;
		for ( int i = 0; i < m_pOwner->m_GeometryPools.m_nCount; i++ )
		{
			if ( pNode->m_pObject == m_pBeginBuffer )
			{
				m_pOwner->m_GeometryPools.m_pCurrent = pNode;
				nBeginPool = i + 1;
				break;
			}
			pNode = pNode->m_pNext;
		}

		int nEndPool = 0;
		pNode = m_pOwner->m_GeometryPools.m_pFirst;
		for ( int i = 0; i < m_pOwner->m_GeometryPools.m_nCount; i++ )
		{
			if ( pNode->m_pObject == m_pEndBuffer )
			{
				m_pOwner->m_GeometryPools.m_pCurrent = pNode;
				nEndPool = i + 1;
				break;
			}
			pNode = pNode->m_pNext;
		}

		nPoolDistance = nEndPool - nBeginPool;
		DWORD cCommands = (DWORD)nCapacity - (DWORD)m_nBeginCommand;
		if ( nPoolDistance > 1 )
			cCommands += (DWORD)nCapacity * (DWORD)( nPoolDistance - 1 );
		nCommands = (int)( cCommands + (DWORD)m_nEndCommand );
	}

	m_pCommands = new ( std::nothrow ) CA3dCommandBuffer( nCommands, 1 );
	if ( m_pBeginBuffer == m_pEndBuffer )
	{
		m_pCommands->m_cCommands = CommandBuffer_Copy( m_pCommands, 0, m_pBeginBuffer,
			m_nBeginCommand, nCommands );
	}
	else
	{
		m_pCommands->m_cCommands = CommandBuffer_Copy( m_pCommands, 0, m_pBeginBuffer,
			m_nBeginCommand, (int)( (DWORD)nCapacity - (DWORD)m_nBeginCommand ) );
		if ( nPoolDistance > 1 )
		{
			m_pOwner->m_GeometryPools.m_pCurrent = m_pOwner->m_GeometryPools.m_pFirst;
			int nPools = m_pOwner->m_GeometryPools.m_nCount;
			if ( nPools > 0 )
			{
				int nPool = (int)( (DWORD)nPoolDistance - ( (DWORD)nPools - 1 ) );
				do
				{
					CA3dCommandBuffer* pBuffer = NULL;
					CA3dListNode* pNode = m_pOwner->m_GeometryPools.m_pCurrent;
					if ( pNode )
					{
						pBuffer = (CA3dCommandBuffer*)pNode->m_pObject;
						m_pOwner->m_GeometryPools.m_pCurrent = pNode->m_pNext;
					}

					int nCommand = (int)( (DWORD)nPool - 1 );
					if ( nCommand > m_nBeginCommand && nCommand < (int)( (DWORD)m_nEndCommand - 1 ) )
					{
						int nFit = CommandBuffer_CheckFit( m_pCommands, m_pCommands->m_cCommands, pBuffer );
						m_pCommands->m_cCommands = (int)( (DWORD)m_pCommands->m_cCommands + (DWORD)nFit );
					}
					nPool = (int)( (DWORD)nPool + 1 );
				}
				while ( --nPools );
			}
		}

		int nCopied = CommandBuffer_Copy( m_pCommands, m_pCommands->m_cCommands, m_pEndBuffer, 0, m_nEndCommand );
		m_pCommands->m_cCommands = (int)( (DWORD)m_pCommands->m_cCommands + (DWORD)nCopied );
	}

	return m_pCommands->m_cCommands;
}

HRESULT STDMETHODCALLTYPE CA3dList::Call()
{
	m_pOwner->AppendGeometryPool( m_pCommands );
	return m_pCommands->m_cCommands;
}

CA3dMaterial::CA3dMaterial()
{
	m_cReferences = 0;
	m_flReflectance = 1.0f;
	m_flReflectionFilter = 1.0f;
	m_flTransmittance = 0.0f;
	m_flTransmissionFilter = 0.0f;
	m_dwPreset = (DWORD)-1;
	Format( "Material", (int)++g_cMaterialNames );
}

CA3dMaterial::CA3dMaterial( const CA3dMaterial& source ) : CA3dName()
{
	m_cReferences = 0;
	memcpy( &m_flReflectance, &source.m_flReflectance, sizeof( A3DVAL ) * 6 );
	m_dwPreset = source.m_dwPreset;
}

CA3dMaterial::~CA3dMaterial()
{
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::QueryInterface( REFIID, void** ppObject )
{
	return ppObject ? S_OK : E_INVALIDARG;
}

ULONG STDMETHODCALLTYPE CA3dMaterial::AddRef()
{
	return ++m_cReferences;
}

ULONG STDMETHODCALLTYPE CA3dMaterial::Release()
{
	ULONG cReferences = --m_cReferences;
	if ( !cReferences )
		delete this;

	return cReferences;
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::Load( LPSTR )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::Save( LPSTR )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::UnSerialize( LPVOID, UINT )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::Serialize( LPVOID*, UINT* )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::Duplicate( LPA3DMATERIAL* )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::SetNameID( LPSTR pszName )
{
	return Set( pszName );
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::GetNameID( LPSTR pszName, INT nSize )
{
	return Get( pszName, nSize );
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::SelectPreset( DWORD )
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::GetClosestPreset( LPDWORD pdwPreset )
{
	*pdwPreset = m_dwPreset;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::GetReflectance( LPA3DVAL pReflectance, LPA3DVAL pHighFrequency )
{
	memcpy( pReflectance, &m_flReflectance, sizeof( A3DVAL ) );
	memcpy( pHighFrequency, &m_flReflectanceHF, sizeof( A3DVAL ) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::GetTransmittance( LPA3DVAL pTransmittance, LPA3DVAL pHighFrequency )
{
	memcpy( pTransmittance, &m_flTransmittance, sizeof( A3DVAL ) );
	memcpy( pHighFrequency, &m_flTransmittanceHF, sizeof( A3DVAL ) );
	return S_OK;
}

ULONG STDMETHODCALLTYPE CA3d::PushMatrix()
{
	m_nMatrix = (LONG)( (DWORD)m_nMatrix + 1 );
	if ( m_nMatrix == 32 )
	{
		m_nMatrix = 31;
		return (ULONG)E_FAIL;
	}

	memcpy( m_Matrix[m_nMatrix], m_Matrix[m_nMatrix - 1], sizeof( A3DMATRIX ) );
	return S_OK;
}

ULONG STDMETHODCALLTYPE CA3d::PopMatrix()
{
	m_nMatrix = (LONG)( (DWORD)m_nMatrix - 1 );
	if ( m_nMatrix == -1 )
	{
		m_nMatrix = 0;
		return (ULONG)E_FAIL;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::LoadIdentity()
{
	Matrix_Identity( m_Matrix[m_nMatrix] );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::LoadMatrix( A3DMATRIX matrix )
{
	memcpy( m_Matrix[m_nMatrix], matrix, sizeof( A3DMATRIX ) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::GetMatrix( A3DMATRIX matrix )
{
	memcpy( matrix, m_Matrix[m_nMatrix], sizeof( A3DMATRIX ) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::MultMatrix( A3DMATRIX matrix )
{
	Matrix_Multiply( m_Matrix[m_nMatrix], matrix, m_Matrix[m_nMatrix] );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::Translate3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	A3DVAL translation[3] = { x, y, z };
	return Translate3fv( translation );
}

HRESULT STDMETHODCALLTYPE CA3d::Translate3fv( LPA3DVAL pTranslation )
{
	A3DMATRIX translation;
	Matrix_Identity( translation );
	memcpy( &translation[12], pTranslation, sizeof( A3DVAL ) * 3 );
	Matrix_Multiply( m_Matrix[m_nMatrix], translation, m_Matrix[m_nMatrix] );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::Rotate3f( A3DVAL flAngle, A3DVAL x, A3DVAL y, A3DVAL z )
{
	A3DVAL axis[3] = { x, y, z };
	return Rotate3fv( flAngle, axis );
}

HRESULT STDMETHODCALLTYPE CA3d::Scale3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	A3DVAL scale[3] = { x, y, z };
	return Scale3fv( scale );
}

HRESULT STDMETHODCALLTYPE CA3d::Scale3fv( LPA3DVAL pScale )
{
	A3DMATRIX scaling;
	Matrix_Identity( scaling );
	scaling[0] = pScale[0];
	scaling[5] = pScale[1];
	scaling[10] = pScale[2];
	Matrix_Multiply( m_Matrix[m_nMatrix], scaling, m_Matrix[m_nMatrix] );
	return S_OK;
}

static const float identityMatrix[16] =
{
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};

void __cdecl Matrix_Translate( const float offset[3], float matrix[16] )
{
	float translation[16];

	Matrix_Identity( translation );
	memcpy( &translation[12], offset, sizeof( float ) * 3 );
	Matrix_Multiply( matrix, translation, matrix );
}

void __cdecl Matrix_Scale( const float scale[3], float matrix[16] )
{
	float scaling[16];

	Matrix_Identity( scaling );
	scaling[0] = scale[0];
	scaling[5] = scale[1];
	scaling[10] = scale[2];
	Matrix_Multiply( matrix, scaling, matrix );
}

void __cdecl Matrix_Multiply( const float left[16], const float right[16], float out[16] )
{
	float product[16];

	for ( int i = 0; i < 4; i++ )
	{
		product[i] = ( ( left[i + 8] * right[2] + left[i + 12] * right[3] ) +
			left[i + 4] * right[1] ) + left[i] * right[0];
		product[i + 4] = ( ( left[i + 12] * right[7] + left[i + 8] * right[6] ) +
			left[i] * right[4] ) + left[i + 4] * right[5];
		product[i + 8] = ( ( left[i] * right[8] + left[i + 8] * right[10] ) +
			left[i + 4] * right[9] ) + left[i + 12] * right[11];
		product[i + 12] = ( ( left[i + 8] * right[14] + left[i + 12] * right[15] ) +
			left[i + 4] * right[13] ) + left[i] * right[12];
	}

	memcpy( out, product, sizeof( product ) );
}

void __cdecl Matrix_Identity( float matrix[16] )
{
	memcpy( matrix, identityMatrix, sizeof( identityMatrix ) );
}

void __cdecl Matrix_GetTranslation( const float matrix[16], float out[3] )
{
	memcpy( out, &matrix[12], sizeof( float ) * 3 );
}

void __cdecl Matrix_TransformVector( const float in[3], const float matrix[16], float out[3] )
{
	float transformed[3];

	for ( int i = 0; i < 3; i++ )
	{
		transformed[i] = ( ( matrix[i * 4] * in[0] + in[2] * matrix[i * 4 + 2] ) +
			matrix[i * 4 + 1] * in[1] ) + matrix[i * 4 + 3];
	}

	memcpy( out, transformed, sizeof( transformed ) );
}

#elif defined( A3D_GEOMETRY_OBJECTS )

#include "LinkList.h"
#include "A3dGeom.h"
#include "WallEdge.h"
#include "A3dRoom.h"
#include "A3dWallBuilder.h"
#include "A3dOpeningBuilder.h"
#include "matrix.h"
#include "rotation.h"

#include <stddef.h>
#include <new>
#include <string.h>

CA3dListNode* CA3dListNode::GetLast()
{
	CA3dListNode* pNode = this;

	while ( pNode->m_pNext )
		pNode = pNode->m_pNext;

	return pNode;
}

void CA3dObjectList::Clear()
{
	CA3dListNode* pNode = m_pFirst;

	for ( int i = 0; i < m_nCount; i++ )
	{
		CA3dListNode* pNext = pNode->m_pNext;
		if ( pNode )
			operator delete( pNode );
		pNode = pNext;
	}

	m_nCount = 0;
	m_pCurrent = NULL;
	m_pFirst = NULL;
	m_pLast = NULL;
}

CA3dGeometryFace::CA3dGeometryFace( CA3dWallBuilder* pBuilder, DWORD dwFlags, const A3DVAL* pPosition,
		const A3DVAL* pForward, const A3DVAL* pUp ) :
	m_cReferences( 0 ),
	m_Edges(),
	m_Openings(),
	m_AdjacentFaces(),
	m_dwFlags( dwFlags )
{
	m_Transform.SetFrame( this, pPosition, pForward, pUp );
	CopyEdges( &pBuilder->m_Edges );
	CopyPolygons( &pBuilder->m_Polygons );
	memcpy( m_Plane, pBuilder->m_pNormal, sizeof( m_Plane ) );

	CopyOpenings( &pBuilder->m_Openings );
	CA3dMaterialReference::Copy( pBuilder );
	m_pRoom = NULL;
}

CA3dGeometryFace::CA3dGeometryFace( CA3dRoom* pRoom, CA3dGeometryFace* pFace ) :
	m_cReferences( 0 ),
	m_Edges(),
	m_Openings(),
	m_AdjacentFaces(),
	m_dwFlags( pFace->m_dwFlags )
{
	m_Transform.CopyFrom( this, &pFace->m_Transform );
	m_Transform.m_pParent = &pRoom->m_Transform;
	ClearEdges();

	pFace->m_Edges.m_pCurrent = pFace->m_Edges.m_pFirst;
	for ( int nEdge = pFace->m_Edges.m_nCount; nEdge > 0; nEdge-- )
	{
		CA3dGeometryEdge* pEdge = (CA3dGeometryEdge*)pFace->m_Edges.Next();
		CA3dGeometryEdge* pCopy = new ( std::nothrow ) CA3dGeometryEdge( pEdge->m_EndPoints, this );
		m_Edges.Append( pCopy );
	}

	CopyPolygons( &pFace->m_Polygons );
	CopyOpenings( &pFace->m_Openings );
	memcpy( m_Plane, pFace->m_Plane, sizeof( m_Plane ) );

	CA3dMaterialReference::Copy( pFace );
	m_pRoom = pRoom;
}

CA3dGeometryFace::~CA3dGeometryFace()
{
	ClearEdges();
	ClearOpenings();
	m_AdjacentFaces.Clear();
	m_Openings.Clear();
	m_Edges.Clear();
}

ULONG STDMETHODCALLTYPE CA3dGeometryFace::AddRef()
{
	return ++m_cReferences;
}

ULONG STDMETHODCALLTYPE CA3dGeometryFace::Release()
{
	ULONG cReferences = --m_cReferences;
	if ( !cReferences )
		delete this;

	return cReferences;
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::GetParent( IUnknown** ppParent )
{
	if ( !ppParent )
		return E_INVALIDARG;

	*ppParent = NULL;
	if ( !m_pRoom )
		return E_NOINTERFACE;

	*ppParent = (IUnknown*)m_pRoom;
	( (IUnknown*)m_pRoom )->AddRef();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::SetMaterial( IA3dMaterial* pMaterial )
{
	return CA3dMaterialReference::Set( pMaterial );
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::GetMaterial( IA3dMaterial** ppMaterial )
{
	return CA3dMaterialReference::Clone( ppMaterial );
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::GetOpening( int nOpening, IUnknown** ppOpening )
{
	if ( !ppOpening )
		return E_INVALIDARG;
	if ( nOpening <= 0 )
		return A3DERROR_INVALID_OPENING_INDEX;

	*ppOpening = NULL;
	int nIndex = nOpening - 1;
	if ( m_Openings.m_nCount <= 0 )
		return A3DERROR_NO_ROOMS_IN_SCENE;

	CA3dListNode* pNode = NULL;
	if ( nIndex >= 0 && nIndex < m_Openings.m_nCount )
	{
		pNode = m_Openings.m_pFirst;
		for ( int i = 0; i < nIndex; i++ )
			pNode = pNode->m_pNext;
	}

	IUnknown* pOpening = NULL;
	if ( pNode )
	{
		m_Openings.m_pCurrent = pNode;
		pOpening = (IUnknown*)pNode->m_pObject;
	}
	if ( !pOpening )
		return A3DERROR_INVALID_OPENING_INDEX;

	*ppOpening = pOpening;
	pOpening->AddRef();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::SetPosition( CA3dTransform* pFrame, const A3DVAL* pPosition )
{
	HRESULT status = m_Transform.SetPosition( pFrame, pPosition );
	if ( SUCCEEDED( status ) && ( m_dwFlags & A3D_SHELL_WALL ) && m_pRoom )
		m_pRoom->InvalidateScene();

	return status;
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::GetPosition( CA3dTransform* pFrame, A3DVAL* pPosition )
{
	return m_Transform.GetPosition( pFrame, pPosition );
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::SetOrientation( CA3dTransform* pFrame, const A3DVAL* pDirection,
		const A3DVAL* pUp )
{
	HRESULT status = m_Transform.SetOrientation( pFrame, pDirection, pUp );
	if ( SUCCEEDED( status ) && ( m_dwFlags & A3D_SHELL_WALL ) && m_pRoom )
		m_pRoom->InvalidateScene();

	return status;
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::GetOrientation( CA3dTransform* pFrame, A3DVAL* pDirection,
		A3DVAL* pUp )
{
	return m_Transform.GetOrientation( pFrame, pDirection, pUp );
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::SetTransform( CA3dTransform* pTransform )
{
	if ( !pTransform )
		return A3DERROR_INVALID_ARGUMENT;

	m_Transform.CopyMatrixFrom( pTransform );
	if ( ( m_dwFlags & A3D_SHELL_WALL ) && m_pRoom )
		m_pRoom->InvalidateScene();

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::GetTransform( CA3dTransform** ppTransform )
{
	CA3dTransform* pTransform = new ( std::nothrow ) CA3dTransform;
	if ( !pTransform )
		return A3DERROR_MEMORY_ALLOCATION;

	pTransform->CopyFrom( this, &m_Transform );
	pTransform->AddRef();
	*ppTransform = pTransform;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dGeometryFace::UnsupportedOperation( DWORD )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

void CA3dGeometryFace::TransformPlane( A3DVAL* pPlane )
{
	m_Transform.TransformVector( pPlane, m_Plane );
}

void CA3dGeometryFace::ClearOpenings()
{
	m_Openings.m_pCurrent = m_Openings.m_pFirst;
	for ( int nOpening = m_Openings.m_nCount; nOpening > 0; nOpening-- )
	{
		IUnknown* pOpening = (IUnknown*)m_Openings.Next();
		pOpening->Release();
	}

	m_Openings.Clear();
}

void CA3dGeometryFace::ClearEdges()
{
	m_Edges.m_pCurrent = m_Edges.m_pFirst;
	for ( int nEdge = m_Edges.m_nCount; nEdge > 0; nEdge-- )
	{
		CA3dGeometryEdge* pEdge = (CA3dGeometryEdge*)m_Edges.Next();
		if ( pEdge )
			delete pEdge;
	}

	m_Edges.Clear();
}

void CA3dGeometryFace::ClearEdgeLinks()
{
	m_Edges.m_pCurrent = m_Edges.m_pFirst;
	for ( int nEdge = m_Edges.m_nCount; nEdge > 0; nEdge-- )
	{
		CA3dGeometryEdge* pEdge = (CA3dGeometryEdge*)m_Edges.Next();
		pEdge->m_AdjacentEdges.Clear();
	}
}

void CA3dGeometryFace::CopyEdges( CA3dObjectList* pEdges )
{
	ClearEdges();
	pEdges->m_pCurrent = pEdges->m_pFirst;
	for ( int nEdge = pEdges->m_nCount; nEdge > 0; nEdge-- )
	{
		const A3DVERTEX* pVertices = (const A3DVERTEX*)pEdges->Next();
		CA3dGeometryEdge* pEdge = new ( std::nothrow ) CA3dGeometryEdge( pVertices, this );
		m_Edges.Append( pEdge );
	}
}

void CA3dGeometryFace::CopyOpenings( CA3dObjectList* pOpenings )
{
	pOpenings->m_pCurrent = pOpenings->m_pFirst;
	for ( int nOpening = pOpenings->m_nCount; nOpening > 0; nOpening-- )
	{
		CA3dOpening* pOpening = (CA3dOpening*)pOpenings->Next();
		CA3dOpening* pCopy = new ( std::nothrow ) CA3dOpening( pOpening, this );
		m_Openings.Append( pCopy );
		pCopy->AddRef();
	}
}

static const IID IID_A3dOpening =
{
	0x103e722b, 0x0113, 0x11d2, { 0x90, 0xfb, 0x00, 0x60, 0x08, 0xa1, 0xf4, 0x41 }
};

CA3dOpening::CA3dOpening( CA3dOpeningBuilder* pBuilder, const A3DVAL* pPosition,
		const A3DVAL* pForward, const A3DVAL* pUp )
{
	m_cReferences = 0;
	m_Transform.SetFrame( this, pPosition, pForward, pUp );
	m_pParent = NULL;
	CopyPolygons( &pBuilder->m_Polygons );

	const A3DVAL* pNormal = pBuilder->m_pNormal;
	memcpy( m_Plane, pNormal, sizeof( m_Plane ) );

	m_dwInstanceMode = 1;
	m_pLinkedOpening = NULL;
	m_flOpeningFactor = 0.0f;
}

CA3dOpening::CA3dOpening( CA3dOpening* pOpening, CA3dGeometryFace* pFace )
{
	m_cReferences = 0;
	m_Transform.CopyFrom( this, &pOpening->m_Transform );
	m_pParent = pFace;
	m_Transform.m_pParent = &pFace->m_Transform;
	CopyPolygons( &pOpening->m_Polygons );

	memcpy( m_Plane, pOpening->m_Plane, sizeof( m_Plane ) );

	m_pLinkedOpening = NULL;
	m_flOpeningFactor = 0.0f;
	m_dwInstanceMode = 1;
}

CA3dOpening::~CA3dOpening()
{
}

HRESULT STDMETHODCALLTYPE CA3dOpening::QueryInterface( REFIID iid, void** ppObject )
{
	if ( !ppObject )
		return E_INVALIDARG;

	if ( !memcmp( &iid, &IID_IUnknown, sizeof( IID ) ) ||
		!memcmp( &iid, &IID_A3dOpening, sizeof( IID ) ) )
	{
		*ppObject = (IUnknown*)this;
	}

	if ( !*ppObject )
		return E_NOINTERFACE;

	( (IUnknown*)*ppObject )->AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE CA3dOpening::AddRef()
{
	return ++m_cReferences;
}

ULONG STDMETHODCALLTYPE CA3dOpening::Release()
{
	ULONG cReferences = --m_cReferences;
	if ( !cReferences )
		delete this;

	return cReferences;
}

HRESULT STDMETHODCALLTYPE CA3dOpening::GetParent( IUnknown** ppParent )
{
	if ( !ppParent )
		return E_INVALIDARG;

	*ppParent = NULL;
	if ( m_pParent )
	{
		*ppParent = m_pParent;
		m_pParent->AddRef();
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dOpening::SetOpeningFactor( A3DVAL flFactor )
{
	if ( m_pLinkedOpening )
		m_pLinkedOpening->SetOpeningFactor( flFactor );
	else
		memcpy( &m_flOpeningFactor, &flFactor, sizeof( flFactor ) );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dOpening::GetOpeningFactor( A3DVAL* pFactor )
{
	if ( !pFactor )
		return E_INVALIDARG;

	if ( m_pLinkedOpening )
		m_pLinkedOpening->GetOpeningFactor( pFactor );
	else
		memcpy( pFactor, &m_flOpeningFactor, sizeof( *pFactor ) );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dOpening::SetPosition( CA3dTransform* pReference, const A3DVAL* pPosition )
{
	return m_Transform.SetPosition( pReference, pPosition );
}

HRESULT STDMETHODCALLTYPE CA3dOpening::GetPosition( CA3dTransform* pReference, A3DVAL* pPosition )
{
	return m_Transform.GetPosition( pReference, pPosition );
}

CA3dGeometryEdge::CA3dGeometryEdge( const A3DVERTEX* pEndpoints, CA3dGeometryFace* pFace )
{
	m_AdjacentEdges.m_pFirst = NULL;
	m_AdjacentEdges.m_pLast = NULL;
	m_AdjacentEdges.m_pCurrent = NULL;
	m_AdjacentEdges.m_nCount = 0;
	memcpy( m_EndPoints, pEndpoints, sizeof( m_EndPoints ) );
	m_pFace = pFace;
}

CA3dGeometryEdge::~CA3dGeometryEdge()
{
	CA3dListNode* pNode = m_AdjacentEdges.m_pFirst;

	for ( int i = 0; i < m_AdjacentEdges.m_nCount; i++ )
	{
		CA3dListNode* pNext = pNode->m_pNext;
		if ( pNode )
			operator delete( pNode );
		pNode = pNext;
	}

	m_AdjacentEdges.m_nCount = 0;
	m_AdjacentEdges.m_pCurrent = NULL;
	m_AdjacentEdges.m_pFirst = NULL;
	m_AdjacentEdges.m_pLast = NULL;
}

DWORD __stdcall Mixer_MultiplyFixed16( DWORD left, DWORD right )
{
	return (DWORD)( ( (unsigned __int64)left * right ) >> 16 );
}

static const IID iidOpeningBuilder =
{
	0x103e7234, 0x0113, 0x11d2, { 0x90, 0xfb, 0x00, 0x60, 0x08, 0xa1, 0xf4, 0x41 }
};

static DWORD openingBuilderCount;

CA3dOpeningBuilder::CA3dOpeningBuilder( int nSceneType ) :
	CA3dGeometryBuilder( nSceneType ),
	m_cReferences( 0 )
{
	CA3dName::Format( "OpeningBuilder", (int)++openingBuilderCount );
}

CA3dOpeningBuilder::~CA3dOpeningBuilder()
{
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::QueryInterface( REFIID iid, void** ppObject )
{
	if ( !ppObject )
		return E_INVALIDARG;

	if ( IsEqualIID( iid, IID_IUnknown ) || IsEqualIID( iid, iidOpeningBuilder ) )
		*ppObject = (IUnknown*)this;

	IUnknown* pObject = (IUnknown*)*ppObject;
	if ( !pObject )
		return E_NOINTERFACE;

	pObject->AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE CA3dOpeningBuilder::AddRef()
{
	return ++m_cReferences;
}

ULONG STDMETHODCALLTYPE CA3dOpeningBuilder::Release()
{
	ULONG cReferences = --m_cReferences;
	if ( !cReferences )
		delete this;

	return cReferences;
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::Begin( DWORD dwMode )
{
	return CA3dGeometryBuilder::Begin( dwMode );
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::AddVertex3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	return CA3dGeometryBuilder::AddVertex3f( x, y, z );
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::AddVertex3fv( const A3DVAL* pVertex )
{
	return CA3dGeometryBuilder::AddVertex3fv( pVertex );
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::End()
{
	return CA3dGeometryBuilder::End();
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::RemovePolygon( int nPolygon )
{
	return CA3dGeometryBuilder::RemovePolygon( nPolygon );
}

int STDMETHODCALLTYPE CA3dOpeningBuilder::GetPolygonCount()
{
	return CA3dGeometryBuilder::GetPolygonCount();
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::SetVertex( int nPolygon, int nVertex, A3DVAL x, A3DVAL y, A3DVAL z )
{
	return CA3dGeometryBuilder::SetVertex( nPolygon, nVertex, x, y, z );
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::GetVertex( int nPolygon, int nVertex, A3DVAL* pVertex )
{
	return CA3dGeometryBuilder::GetVertex( nPolygon, nVertex, pVertex );
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::Validate()
{
	return CA3dGeometryBuilder::Validate();
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::Clear()
{
	return CA3dGeometryBuilder::Clear();
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::UnsupportedRequest1( DWORD )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::UnsupportedRequest2( DWORD )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::UnsupportedRequest3( DWORD, DWORD )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::UnsupportedRequest4( DWORD, DWORD )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::UnsupportedRequest5( DWORD )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::SetNameID( LPSTR pszName )
{
	return CA3dName::Set( pszName );
}

HRESULT STDMETHODCALLTYPE CA3dOpeningBuilder::GetNameID( LPSTR pszName, INT nSize )
{
	return CA3dName::Get( pszName, (DWORD)nSize );
}

CA3dTransform::~CA3dTransform()
{
}

void CA3dTransform::SetOwner( void* pOwner )
{
	m_pOwner = pOwner;
}

CA3dTransform* CA3dTransform::CopyFrom( void* pOwner, const CA3dTransform* pTransform )
{
	memcpy( m_Matrix, pTransform->m_Matrix, sizeof( A3DMATRIX ) );

	m_pParent = pTransform->m_pParent;
	if ( pOwner )
		m_pOwner = pOwner;

	return this;
}

ULONG STDMETHODCALLTYPE CA3dTransform::AddRef()
{
	return ++m_cReferences;
}

ULONG STDMETHODCALLTYPE CA3dTransform::Release()
{
	ULONG cReferences = --m_cReferences;
	if ( !cReferences )
		delete this;

	return cReferences;
}

HRESULT STDMETHODCALLTYPE CA3dTransform::Scale( A3DVAL x, A3DVAL y, A3DVAL z )
{
	A3DVECTOR scale = { x, y, z, 0.0f };
	Matrix_Scale( scale, m_Matrix );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dTransform::Rotate( A3DVAL* pAxis, A3DVAL angle )
{
	Matrix_Rotate( angle, pAxis, m_Matrix );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dTransform::Translate( const A3DVAL* pOffset )
{
	Matrix_Translate( pOffset, m_Matrix );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dTransform::LoadMatrix( const A3DVAL* pMatrix )
{
	if ( !pMatrix )
		return E_INVALIDARG;

	memcpy( m_Matrix, pMatrix, sizeof( A3DMATRIX ) );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dTransform::GetMatrix( A3DVAL* pMatrix )
{
	if ( !pMatrix )
		return E_INVALIDARG;

	memcpy( pMatrix, m_Matrix, sizeof( A3DMATRIX ) );

	return S_OK;
}

void CA3dTransform::CopyMatrixFrom( const CA3dTransform* pTransform )
{
	memcpy( m_Matrix, pTransform->m_Matrix, sizeof( A3DMATRIX ) );
}

#elif defined( A3D_GEOMETRY_FINALIZE )

#define _USE_MATH_DEFINES

#include "A3dGeom.h"
#include "LinkList.h"
#include "vector_math.h"

#include <math.h>
#include <new>
#include <string.h>
#include <stddef.h>

void CA3dGeometryBuilder::FinishPolygon()
{
	m_nVertex = (int)( (DWORD)m_nVertex + 1 );
	if ( m_nVertex < m_Polygon.m_nVertices )
		return;

	a3d_polygon_t* pPolygon = (a3d_polygon_t*)operator new( sizeof( *pPolygon ), std::nothrow );
	if ( pPolygon )
	{
		A3DVAL edgeX = m_Polygon.m_Vertices[0][0] - m_Polygon.m_Vertices[1][0];
		double otherY = (double)m_Polygon.m_Vertices[0][1] - m_Polygon.m_Vertices[2][1];
		double otherZ = (double)m_Polygon.m_Vertices[0][2] - m_Polygon.m_Vertices[2][2];
		double otherX = (double)m_Polygon.m_Vertices[0][0] - m_Polygon.m_Vertices[2][0];
		A3DVAL roundedOtherX = (A3DVAL)otherX;
		A3DVAL roundedOtherY = (A3DVAL)otherY;
		A3DVAL edgeY = m_Polygon.m_Vertices[0][1] - m_Polygon.m_Vertices[1][1];
		A3DVAL edgeZ = m_Polygon.m_Vertices[0][2] - m_Polygon.m_Vertices[1][2];

		double normalX = otherZ * edgeY - otherY * edgeZ;
		double normalY = otherX * edgeZ - otherZ * edgeX;
		double normalZ = (double)roundedOtherY * edgeX - (double)roundedOtherX * edgeY;
		m_Polygon.m_Normal[0] = (A3DVAL)normalX;
		m_Polygon.m_Normal[1] = (A3DVAL)normalY;
		m_Polygon.m_Normal[2] = (A3DVAL)normalZ;

		double length = sqrt( ( normalX * normalX + normalY * normalY ) + normalZ * normalZ );
		if ( length > 0.0 )
		{
			double inverseLength = 1.0 / length;
			m_Polygon.m_Normal[0] = (A3DVAL)( inverseLength * normalX );
			m_Polygon.m_Normal[1] = (A3DVAL)( inverseLength * normalY );
			m_Polygon.m_Normal[2] = (A3DVAL)( inverseLength * normalZ );
		}

		memcpy( pPolygon, &m_Polygon, sizeof( *pPolygon ) );
		m_Polygons.Append( pPolygon );
	}

	m_nVertex = 0;
}

HRESULT CA3dGeometryBuilder::CheckCoplanarity()
{
	int nPolygons = (int)( (DWORD)m_Polygons.m_nCount - 1 );
	m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
	m_pFirstPolygon = NULL;
	if ( m_Polygons.m_pCurrent )
	{
		m_pFirstPolygon = (a3d_polygon_t*)m_Polygons.m_pCurrent->m_pObject;
		m_Polygons.m_pCurrent = m_Polygons.m_pCurrent->m_pNext;
	}
	if ( !m_pFirstPolygon )
		return E_FAIL;

	double otherY = (double)m_pFirstPolygon->m_Vertices[0][1] - m_pFirstPolygon->m_Vertices[2][1];
	double otherZ = (double)m_pFirstPolygon->m_Vertices[0][2] - m_pFirstPolygon->m_Vertices[2][2];
	A3DVAL edgeX = m_pFirstPolygon->m_Vertices[0][0] - m_pFirstPolygon->m_Vertices[1][0];
	double otherX = (double)m_pFirstPolygon->m_Vertices[0][0] - m_pFirstPolygon->m_Vertices[2][0];
	A3DVAL roundedOtherY = (A3DVAL)otherY;
	A3DVAL roundedOtherZ = (A3DVAL)otherZ;
	A3DVAL edgeY = m_pFirstPolygon->m_Vertices[0][1] - m_pFirstPolygon->m_Vertices[1][1];
	A3DVAL edgeZ = m_pFirstPolygon->m_Vertices[0][2] - m_pFirstPolygon->m_Vertices[1][2];

	m_pFirstPolygon->m_Normal[0] = (A3DVAL)( otherZ * edgeY - otherY * edgeZ );
	m_pFirstPolygon->m_Normal[1] = (A3DVAL)( otherX * edgeZ - (double)roundedOtherZ * edgeX );
	m_pFirstPolygon->m_Normal[2] = (A3DVAL)( (double)roundedOtherY * edgeX - otherX * edgeY );
	A3DVAL normalX = m_pFirstPolygon->m_Normal[0];
	A3DVAL normalY = m_pFirstPolygon->m_Normal[1];
	A3DVAL normalZ = m_pFirstPolygon->m_Normal[2];
	double length = sqrt( ( (double)normalX * normalX + (double)normalY * normalY ) +
		(double)normalZ * normalZ );
	if ( length > 0.0 )
	{
		double inverseLength = 1.0 / length;
		m_pFirstPolygon->m_Normal[0] = (A3DVAL)( inverseLength * m_pFirstPolygon->m_Normal[0] );
		m_pFirstPolygon->m_Normal[1] = (A3DVAL)( inverseLength * m_pFirstPolygon->m_Normal[1] );
		m_pFirstPolygon->m_Normal[2] = (A3DVAL)( inverseLength * m_pFirstPolygon->m_Normal[2] );
	}
	m_pNormal = m_pFirstPolygon->m_Normal;

	if ( m_pFirstPolygon->m_nVertices == A3D_QUADS )
	{
		double x = (double)m_pFirstPolygon->m_Vertices[0][0] - m_pFirstPolygon->m_Vertices[3][0];
		double y = (double)m_pFirstPolygon->m_Vertices[0][1] - m_pFirstPolygon->m_Vertices[3][1];
		double z = (double)m_pFirstPolygon->m_Vertices[0][2] - m_pFirstPolygon->m_Vertices[3][2];
		double distanceSquared = ( x * x + y * y ) + z * z;
		if ( distanceSquared >= A3D_EPSILON_SQUARED )
		{
			double distance = ( x * m_pNormal[0] + z * m_pNormal[2] ) + y * m_pNormal[1];
			if ( fabs( distance ) >= A3D_EPSILON )
				return E_FAIL;
		}
	}

	for ( int i = 0; i < nPolygons; i++ )
	{
		a3d_polygon_t* pPolygon = (a3d_polygon_t*)m_Polygons.Next();
		for ( int j = pPolygon->m_nVertices - 1; j >= 0; j-- )
		{
			double x = (double)m_pFirstPolygon->m_Vertices[0][0] - pPolygon->m_Vertices[j][0];
			double y = (double)m_pFirstPolygon->m_Vertices[0][1] - pPolygon->m_Vertices[j][1];
			double z = (double)m_pFirstPolygon->m_Vertices[0][2] - pPolygon->m_Vertices[j][2];
			double distanceSquared = ( x * x + y * y ) + z * z;
			if ( distanceSquared >= A3D_EPSILON_SQUARED )
			{
				double distance = ( x * m_pNormal[0] + y * m_pNormal[1] ) + z * m_pNormal[2];
				if ( fabs( distance ) >= A3D_EPSILON )
					return E_FAIL;
			}
		}
	}

	return S_OK;
}

void CA3dGeometryBuilder::CheckEdgeIntersection( const a3d_polygon_t* pPolygon, int nStart, int nEnd,
		const a3d_edge_t* pEdge )
{
	A3DVERTEX segment[2];
	segment[0][0] = pPolygon->m_Vertices[nStart][0];
	segment[0][1] = pPolygon->m_Vertices[nStart][1];
	segment[0][2] = pPolygon->m_Vertices[nStart][2];
	segment[1][0] = pPolygon->m_Vertices[nEnd][0];
	segment[1][1] = pPolygon->m_Vertices[nEnd][1];
	segment[1][2] = pPolygon->m_Vertices[nEnd][2];

	double x = (double)pEdge->vertices[0][0] - segment[0][0];
	double y = (double)pEdge->vertices[0][1] - segment[0][1];
	double z = (double)pEdge->vertices[0][2] - segment[0][2];
	A3DVAL roundedX = (A3DVAL)x;
	A3DVAL roundedZ = (A3DVAL)z;
	A3DVAL firstDistance = (A3DVAL)( ( x * roundedX + y * y ) + z * roundedZ );
	double otherX = (double)pEdge->vertices[1][0] - segment[1][0];
	double otherY = (double)pEdge->vertices[1][1] - segment[1][1];
	double otherZ = (double)pEdge->vertices[1][2] - segment[1][2];
	A3DVAL roundedOtherX = (A3DVAL)otherX;
	A3DVAL roundedOtherY = (A3DVAL)otherY;
	A3DVAL roundedOtherZ = (A3DVAL)otherZ;
	double secondDistance = ( (double)roundedOtherX * roundedOtherX + otherY * roundedOtherY ) +
		otherZ * roundedOtherZ;
	if ( !( firstDistance >= A3D_EPSILON_SQUARED ) && !( secondDistance >= A3D_EPSILON_SQUARED ) )
		return;

	x = (double)pEdge->vertices[0][0] - segment[1][0];
	y = (double)pEdge->vertices[0][1] - segment[1][1];
	z = (double)pEdge->vertices[0][2] - segment[1][2];
	roundedX = (A3DVAL)x;
	roundedZ = (A3DVAL)z;
	firstDistance = (A3DVAL)( ( x * roundedX + y * y ) + z * roundedZ );
	otherX = (double)pEdge->vertices[1][0] - segment[0][0];
	otherY = (double)pEdge->vertices[1][1] - segment[0][1];
	otherZ = (double)pEdge->vertices[1][2] - segment[0][2];
	roundedOtherX = (A3DVAL)otherX;
	roundedOtherY = (A3DVAL)otherY;
	roundedOtherZ = (A3DVAL)otherZ;
	secondDistance = ( otherX * roundedOtherX + otherY * roundedOtherY ) + otherZ * roundedOtherZ;
	if ( !( firstDistance >= A3D_EPSILON_SQUARED ) && !( secondDistance >= A3D_EPSILON_SQUARED ) )
		return;

	BOOL bFirstOnSegment = Geometry_PointOnSegment( pEdge->vertices[0], segment );
	BOOL bSecondOnSegment = Geometry_PointOnSegment( pEdge->vertices[1], segment );
	if ( !bFirstOnSegment || !bSecondOnSegment )
	{
		Geometry_PointOnSegment( segment[0], pEdge->vertices );
		Geometry_PointOnSegment( segment[1], pEdge->vertices );
	}
}

CA3dListNode::CA3dListNode( CA3dListNode* pPrevious, void* pObject )
{
	m_pPrevious = pPrevious;
	m_pNext = NULL;
	m_pObject = pObject;
}

#define RADIANS_TO_DEGREES	( (float)( 180.0 / M_PI ) )
#define HALF_PI				( (float)( M_PI / 2 ) )

void __cdecl Vector_ToSphericalDegrees( const float in[3], float out[3] )
{
	float spherical[3];

	Vector_ToSphericalRadians( in, spherical );
	spherical[0] *= RADIANS_TO_DEGREES;
	spherical[1] *= RADIANS_TO_DEGREES;

	memcpy( out, spherical, sizeof( spherical ) );
}

void __cdecl Vector_ToSphericalRadians( const float in[3], float out[3] )
{
	float heading;
	float elevation;
	double horizontalLength;

	if ( !( in[0] < 0.0f || in[0] > 0.0f ) && !( in[2] < 0.0f || in[2] > 0.0f ) )
	{
		heading = 0.0f;
		horizontalLength = 0.0;
	}
	else
	{
		heading = (float)atan2( -(double)in[0], -(double)in[2] );
		horizontalLength = sqrt( (double)in[2] * in[2] + (double)in[0] * in[0] );
	}

	if ( !( horizontalLength < 0.0 || horizontalLength > 0.0 ) )
	{
		if ( in[1] > 0.0f )
			elevation = HALF_PI;
		else if ( in[1] < 0.0f || in[1] != in[1] )
			elevation = -HALF_PI;
		else
			elevation = 0.0f;
	}
	else
	{
		elevation = (float)atan2( (double)in[1], horizontalLength );
	}

	float length = (float)sqrt( ( (double)in[1] * in[1] + (double)in[2] * in[2] ) +
		(double)in[0] * in[0] );

	out[0] = heading;
	out[1] = elevation;
	out[2] = length;
}

#elif defined( A3D_GEOMETRY_ACOUSTICS )

#include "A3dList.h"
#include "A3dGeom.h"
#include "A3d3.h"
#include "matrix_inverse.h"
#include "matrix.h"

#include <string.h>
#include <math.h>

static const float materialFilterCurve[7] =
{
	3.7264376f,
	17.449827f,
	43.63063f,
	52.999165f,
	27.087683f,
	0.7774167f,
	3.7731748f
};

HRESULT STDMETHODCALLTYPE CA3dMaterial::SetReflectance( A3DVAL reflectance, A3DVAL highFrequency )
{
	double x = highFrequency;
	double square = x * x;
	float cube = (float)( x * square );
	float fourth = (float)( x * cube );
	float fifth = (float)( x * fourth );
	double sixth = x * fifth;
	float roundedSixth = (float)sixth;
	m_flReflectance = reflectance;

	double filter = x * materialFilterCurve[0] - square * materialFilterCurve[1];
	filter = filter + cube * (double)materialFilterCurve[2];
	filter = filter - fourth * (double)materialFilterCurve[3];
	filter = filter + fifth * (double)materialFilterCurve[4];
	filter = filter + sixth * materialFilterCurve[5];
	filter = filter - ( x * roundedSixth ) * materialFilterCurve[6];
	m_flReflectionFilter = (float)filter;

	if ( !( filter >= 0.0 ) )
		m_flReflectionFilter = 0.0f;
	else if ( m_flReflectionFilter > 1.0f )
		m_flReflectionFilter = 1.0f;

	m_dwPreset = (DWORD)-1;
	m_flReflectanceHF = highFrequency;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dMaterial::SetTransmittance( A3DVAL transmittance, A3DVAL highFrequency )
{
	double x = highFrequency;
	double square = x * x;
	float cube = (float)( x * square );
	float fourth = (float)( x * cube );
	float fifth = (float)( x * fourth );
	double sixth = x * fifth;
	float roundedSixth = (float)sixth;
	m_flTransmittance = transmittance;

	double filter = x * materialFilterCurve[0] - square * materialFilterCurve[1];
	filter = filter + cube * (double)materialFilterCurve[2];
	filter = filter - fourth * (double)materialFilterCurve[3];
	filter = filter + fifth * (double)materialFilterCurve[4];
	filter = filter + sixth * materialFilterCurve[5];
	filter = filter - ( x * roundedSixth ) * materialFilterCurve[6];
	m_flTransmissionFilter = (float)filter;

	if ( !( filter >= 0.0 ) )
		m_flTransmissionFilter = 0.0f;
	else if ( m_flTransmissionFilter > 1.0f )
		m_flTransmissionFilter = 1.0f;

	m_dwPreset = (DWORD)-1;
	m_flTransmittanceHF = highFrequency;
	return S_OK;
}

CA3dOpening::CA3dOpening( CA3dOpening* pOpening, CA3dGeometryFace* pFace, CA3dGeometryFace* )
{
	m_cReferences = 0;

	A3DVECTOR axes[4] =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f, 1.0f }
	};

	for ( int i = 0; i < 4; i++ )
		pFace->m_Transform.TransformVector( axes[i] );

	A3DMATRIX matrix;
	A3DMATRIX inverse;
	memcpy( matrix, axes, sizeof( matrix ) );
	Matrix_Inverse( matrix, inverse );
	TransformPolygons( &pOpening->m_Polygons, &pOpening->m_Transform, inverse );
	m_Transform.SetOwner( this );

	A3DVECTOR plane = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pParent = pFace;
	m_Transform.m_pParent = &pFace->m_Transform;
	pOpening->m_Transform.TransformVector( plane, pOpening->m_Plane );

	m_Plane[0] = ( ( plane[0] * inverse[0] + plane[1] * inverse[4] ) +
		plane[2] * inverse[8] ) + plane[3] * inverse[12];
	m_Plane[1] = ( ( plane[0] * inverse[1] + plane[1] * inverse[5] ) +
		plane[2] * inverse[9] ) + plane[3] * inverse[13];

	m_dwInstanceMode = 0;
	memset( &m_flOpeningFactor, 0, sizeof( m_flOpeningFactor ) );
	m_pLinkedOpening = pOpening;

	m_Plane[2] = ( ( plane[0] * inverse[2] + plane[1] * inverse[6] ) +
		plane[2] * inverse[10] ) + plane[3] * inverse[14];
	m_Plane[3] = ( ( plane[0] * inverse[3] + plane[1] * inverse[7] ) +
		plane[2] * inverse[11] ) + plane[3] * inverse[15];
}

void CA3dOpening::Render( CA3d* pA3d )
{
	CA3dOpening* pLinked = m_pLinkedOpening;
	A3DVAL* pFactor = pLinked ? &pLinked->m_flOpeningFactor : &m_flOpeningFactor;
	m_Polygons.m_pCurrent = m_Polygons.m_pFirst;
	int nPolygons = m_Polygons.m_nCount;
	IA3dGeomTarget* pGeom = pA3d;

	for ( int i = 0; i < nPolygons; i++ )
	{
		a3d_polygon_t* pPolygon = (a3d_polygon_t*)m_Polygons.Next();
		a3d_polygon_t polygon;
		m_Transform.TransformPolygon( &polygon, pPolygon );

		BYTE nVertices = pPolygon->m_nVertices;
		if ( nVertices == 3 || nVertices == 4 )
		{
			pGeom->Begin( nVertices == 3 ? A3D_SUB_TRIANGLES : A3D_SUB_QUADS );
			pGeom->Normal3fv( polygon.m_Normal );
			pGeom->Vertex3fv( polygon.m_Vertices[0] );
			pGeom->Vertex3fv( polygon.m_Vertices[1] );
			pGeom->Vertex3fv( polygon.m_Vertices[2] );
			if ( nVertices == 4 )
				pGeom->Vertex3fv( polygon.m_Vertices[3] );
			pGeom->SetOpeningFactorfv( pFactor );
		}

		pGeom->End();
	}
}

#pragma intrinsic( sqrt )

static void __cdecl Matrix_OrientListener( float matrix[16], const float front[3], const float top[3] )
{
	float direction[3];
	float vertical[3];
	memcpy( direction, front, sizeof( direction ) );
	memcpy( vertical, top, sizeof( vertical ) );

	float directionLength = (float)sqrt( ( (double)direction[0] * direction[0] +
		(double)direction[1] * direction[1] ) + (double)direction[2] * direction[2] );
	float directionScale = (float)( -1.0 / directionLength );
	double backX = (double)direction[0] * directionScale;
	float backY = (float)( (double)direction[1] * directionScale );
	float backZ = (float)( (double)direction[2] * directionScale );

	float verticalLength = (float)sqrt( ( (double)vertical[0] * vertical[0] +
		(double)vertical[1] * vertical[1] ) + (double)vertical[2] * vertical[2] );
	float verticalScale = (float)( 1.0 / verticalLength );
	double upX = (double)vertical[0] * verticalScale;
	float roundedUpX = (float)upX;
	double upY = (double)vertical[1] * verticalScale;
	float roundedUpY = (float)upY;
	double upZ = (double)vertical[2] * verticalScale;
	float roundedUpZ = (float)upZ;

	float rightX = (float)( upY * backZ - upZ * backY );
	float rightY = (float)( (double)roundedUpZ * backX - upX * backZ );
	float previous[16];
	memcpy( previous, matrix, sizeof( previous ) );
	float rightZ = (float)( (double)roundedUpX * backY - (double)roundedUpY * backX );

	for ( int i = 0; i < 4; i++ )
	{
		matrix[i] = (float)( ( (double)previous[i] * rightX + (double)previous[i + 4] * rightY ) +
			(double)previous[i + 8] * rightZ + 0.0 );
		matrix[i + 4] = (float)( ( (double)previous[i] * roundedUpX +
			(double)previous[i + 4] * roundedUpY ) + (double)previous[i + 8] * roundedUpZ + 0.0 );
		matrix[i + 8] = (float)( ( (double)previous[i] * backX + (double)previous[i + 4] * backY ) +
			(double)previous[i + 8] * backZ + 0.0 );
	}

	memcpy( &matrix[12], &previous[12], sizeof( float ) * 4 );
}

void CA3d::GetListenerMatrices( float left[16], float center[16], float right[16] )
{
	memcpy( center, m_ListenerMatrix, sizeof( m_ListenerMatrix ) );
	Matrix_Translate( m_vPosition, center );
	Matrix_OrientListener( center, m_vFront, m_vTop );
	memcpy( left, center, sizeof( m_ListenerMatrix ) );
	Matrix_Translate( m_vLeftEar, left );
	memcpy( right, center, sizeof( m_ListenerMatrix ) );
	Matrix_Translate( m_vRightEar, right );
}

#elif defined( A3D_GEOMETRY_SUBMISSION )

#include "A3d3.h"
#include "ChunkPage.h"
#include "A3dSource.h"
#include "A3dList.h"

#include <math.h>
#include <new>
#include <string.h>

void CA3d::NextFreeChunk()
{
	CA3dCommandBuffer* pBuffer = m_pCommandBuffer;
	int nCommand = pBuffer->m_iCursor;
	if ( nCommand < pBuffer->m_cCapacity )
	{
		pBuffer->m_cCommands = (int)( (DWORD)pBuffer->m_cCommands + 1 );
		pBuffer->m_iCursor = (int)( (DWORD)nCommand + 1 );
		m_pCurrentCommand = &pBuffer->m_pCommands[nCommand];
	}
	else
	{
		m_pCurrentCommand = NULL;
	}

	if ( m_pCurrentCommand )
		return;

	m_pCommandBuffer = new ( std::nothrow ) CA3dCommandBuffer( (int)m_nCommandCapacity, 0 );
	if ( !m_pCommandBuffer )
		return;

	m_GeometryPools.Append( m_pCommandBuffer );
	pBuffer = m_pCommandBuffer;
	nCommand = pBuffer->m_iCursor;
	if ( nCommand < pBuffer->m_cCapacity )
	{
		pBuffer->m_cCommands = (int)( (DWORD)pBuffer->m_cCommands + 1 );
		pBuffer->m_iCursor = (int)( (DWORD)nCommand + 1 );
		m_pCurrentCommand = &pBuffer->m_pCommands[nCommand];
	}
	else
	{
		m_pCurrentCommand = NULL;
	}
}

HRESULT STDMETHODCALLTYPE CA3d::Vertex3fv( LPA3DVAL pVertex )
{
	return ( (IA3dGeomTarget*)this )->Vertex3f( pVertex[0], pVertex[1], pVertex[2] );
}

HRESULT STDMETHODCALLTYPE CA3d::Vertex3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	m_Primitive.m_Vertices[m_nNextVertex][0] = (A3DVAL)(
		( (double)m_Matrix[m_nMatrix][4] * y + (double)m_Matrix[m_nMatrix][0] * x ) +
		(double)m_Matrix[m_nMatrix][8] * z + m_Matrix[m_nMatrix][12] );
	m_Primitive.m_Vertices[m_nNextVertex][1] = (A3DVAL)(
		( (double)m_Matrix[m_nMatrix][5] * y + (double)m_Matrix[m_nMatrix][1] * x ) +
		(double)m_Matrix[m_nMatrix][9] * z + m_Matrix[m_nMatrix][13] );
	m_Primitive.m_Vertices[m_nNextVertex][2] = (A3DVAL)(
		( (double)m_Matrix[m_nMatrix][2] * x + (double)m_Matrix[m_nMatrix][10] * z ) +
		(double)m_Matrix[m_nMatrix][6] * y + m_Matrix[m_nMatrix][14] );
	m_Primitive.m_Vertices[m_nNextVertex][3] = (A3DVAL)(
		( (double)m_Matrix[m_nMatrix][3] * x + (double)m_Matrix[m_nMatrix][11] * z ) +
		(double)m_Matrix[m_nMatrix][7] * y + m_Matrix[m_nMatrix][15] );

	if ( m_nNextVertex < m_Primitive.m_nVertices - 1 )
	{
		m_nNextVertex = (LONG)( (DWORD)m_nNextVertex + 1 );
		return S_OK;
	}

	if ( m_bCompat1009 )
	{
		m_Primitive.m_dwEdgeFlags |= A3D_PRIMITIVE_COMPAT1009;
	}
	else if ( m_Primitive.m_dwEdgeFlags & A3D_PRIMITIVE_COMPAT1009 )
	{
		m_Primitive.m_dwEdgeFlags -= A3D_PRIMITIVE_COMPAT1009;
	}

	if ( m_bCalculateNormal )
	{
		const A3DVAL* pFirst = m_Primitive.m_Vertices[0];
		const A3DVAL* pSecond = m_Primitive.m_Vertices[1];
		const A3DVAL* pThird = m_Primitive.m_Vertices[2];
		double flThirdX = (double)pThird[0] - pFirst[0];
		double flThirdY = (double)pThird[1] - pFirst[1];
		A3DVAL flSecondX = pSecond[0] - pFirst[0];
		A3DVAL flSecondY = pSecond[1] - pFirst[1];
		double flThirdZ = (double)pThird[2] - pFirst[2];
		A3DVAL flSecondZ = pSecond[2] - pFirst[2];
		A3DVAL flThirdXRounded = (A3DVAL)flThirdX;
		A3DVAL flThirdZRounded = (A3DVAL)flThirdZ;

		m_Primitive.m_Normal[0] = (A3DVAL)( flThirdY * flSecondZ - flThirdZ * flSecondY );
		m_Primitive.m_Normal[1] = (A3DVAL)( (double)flThirdZRounded * flSecondX -
			(double)flThirdXRounded * flSecondZ );
		m_Primitive.m_Normal[2] = (A3DVAL)( flThirdX * flSecondY - flThirdY * flSecondX );

		double flLength = sqrt( ( (double)m_Primitive.m_Normal[0] * m_Primitive.m_Normal[0] +
			(double)m_Primitive.m_Normal[1] * m_Primitive.m_Normal[1] ) +
			(double)m_Primitive.m_Normal[2] * m_Primitive.m_Normal[2] );
		if ( flLength > 0.0 )
		{
			double flScale = 1.0 / flLength;
			m_Primitive.m_Normal[0] = (A3DVAL)( (double)m_Primitive.m_Normal[0] * flScale );
			m_Primitive.m_Normal[1] = (A3DVAL)( flScale * m_Primitive.m_Normal[1] );
			m_Primitive.m_Normal[2] = (A3DVAL)( flScale * m_Primitive.m_Normal[2] );
		}
	}

	NextFreeChunk();
	m_pCurrentCommand->m_dwOpcode = m_dwInputMode;
	m_pCurrentCommand->m_Payload.m_Primitive = m_Primitive;

	m_nNextVertex = 0;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::Normal3fv( LPA3DVAL pNormal )
{
	return ( (IA3dGeomTarget*)this )->Normal3f( pNormal[0], pNormal[1], pNormal[2] );
}

HRESULT STDMETHODCALLTYPE CA3d::Normal3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	m_Primitive.m_Normal[0] = x;
	m_Primitive.m_Normal[1] = y;
	m_Primitive.m_Normal[2] = z;
	m_bCalculateNormal = FALSE;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::NewMaterial( LPA3DMATERIAL* ppMaterial )
{
	*ppMaterial = NULL;
	CA3dMaterial* pMaterial = new ( std::nothrow ) CA3dMaterial;
	*ppMaterial = pMaterial;
	if ( !pMaterial )
		return E_FAIL;

	pMaterial->AddRef();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::BindMaterial( LPA3DMATERIAL pMaterial )
{
	NextFreeChunk();
	m_pCurrentCommand->m_dwOpcode = A3D_MATERIAL;
	memcpy( m_pCurrentCommand->m_Payload.m_MaterialProperties, &( (CA3dMaterial*)pMaterial )->m_flReflectance,
		sizeof( m_MaterialProperties ) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::BindListener()
{
	memcpy( m_ListenerMatrix, m_Matrix[m_nMatrix], sizeof( A3DMATRIX ) );
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3d::BindSource( LPA3DSOURCE pSource )
{
	memcpy( ( (CA3dSource*)pSource )->m_Transform, m_Matrix[m_nMatrix], sizeof( A3DMATRIX ) );
	return S_OK;
}

#elif defined( A3D_GEOMETRY_STATE )

#include "A3d3.h"
#include "ChunkPage.h"

#include <new>
#include <string.h>

HRESULT CA3d::InitializeGeometryState()
{
	m_nCommandCapacity = 512;
	m_pCommandBuffer = new ( std::nothrow ) CA3dCommandBuffer( (int)m_nCommandCapacity, 0 );
	m_pCurrentCommand = NULL;
	if ( !m_pCommandBuffer )
		return A3DERROR_MEMORY_ALLOCATION;

	m_GeometryPools.Append( m_pCommandBuffer );
	m_nNextVertex = 0;
	memset( &m_Primitive, 0, sizeof( m_Primitive ) );
	m_bGeometryInitialized = TRUE;
	m_dwEnabledFeatures = A3D_DIRECT_PATH_A3D | A3D_DIRECT_PATH_GENERIC;
	m_dwOcclusionMode = A3D_FASTEST;
	m_dwReflectionMode = A3D_FASTEST;
	Compat( A3D_COMPAT_LEGACY_STATE_ALTERNATE, 1 );

	m_MaterialProperties[0] = 1.0f;
	m_MaterialProperties[1] = 1.0f;
	m_MaterialProperties[2] = 0.0f;
	m_MaterialProperties[3] = 0.0f;
	( (IA3dGeomTarget*)this )->LoadIdentity();

	m_dwGeometryState = 1;
	m_bListTransform = FALSE;
	memset( m_ListMatrix, 0, sizeof( m_ListMatrix ) );
	m_ListMatrix[5] = 1.0f;
	m_ListMatrix[10] = 1.0f;
	m_ListMatrix[15] = 1.0f;
	m_flReflectionPlaneScale = A3D_DEFAULT_REFLECTION_PLANE_SCALE;
	m_dwPolygonTestLimit = A3D_DEFAULT_POLYGON_TEST_LIMIT;
	m_dwPolygonTestCount = 0;
	m_dwReflectionUpdateModulus = A3D_DEFAULT_REFLECTION_UPDATE_MODULUS;
	m_dwOcclusionUpdateModulus = A3D_DEFAULT_OCCLUSION_UPDATE_MODULUS;
	m_dwLastUpdateTick = 0;
	m_dwUpdateTickPeriod = A3D_DEFAULT_UPDATE_TICK_PERIOD;
	m_ListMatrix[0] = 1.0f;
	return S_OK;
}

HRESULT CA3d::ShutdownGeometryState()
{
	int remaining = m_GeometryPools.m_nCount;
	m_GeometryPools.m_pCurrent = m_GeometryPools.m_pFirst;
	while ( remaining > 0 )
	{
		CA3dCommandBuffer* pBuffer = (CA3dCommandBuffer*)m_GeometryPools.Next();
		if ( !pBuffer->m_iBufferType )
			delete pBuffer;
		remaining--;
	}

	m_bGeometryInitialized = FALSE;
	return S_OK;
}

#endif
