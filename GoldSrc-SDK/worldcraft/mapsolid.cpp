//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "Box3D.h"
#include "BrushOps.h"
#include "GlobalFunctions.h"
#include "MapDefs.h" // dvs: For COORD_NOTINIT
#include "MapWorld.h"
#include "MapSolid.h"
#include "Options.h"
#include "Render2D.h"
#include "Render3D.h"
#include "MapDoc.h"

#include <gl/GL.h>

IMPLEMENT_MAPCLASS( CMapSolid )


//-----------------------------------------------------------------------------
// Purpose: Constructor. Sets this solid's color to a random blue-green color.
// Input  : Parent0 - The parent of this solid. Typically this is the world.
//-----------------------------------------------------------------------------
CMapSolid::CMapSolid( CMapClass* Parent0 ) : m_bValid( FALSE ) // well, no faces
{
	Parent = Parent0;
	m_eSolidType = btSolid;
	m_bIsCordonBrush = false;

	unsigned char b = (unsigned char)( 64 + random() % 192 );
	unsigned char g = (unsigned char)( 64 + random() % 192 );
	SetRenderColor( 0, g, b );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapSolid::~CMapSolid( void )
{
	Faces.SetCount( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Adds the plane to the given solid.
// Input  : pSolid - Solid to which the plane is being added.
//			p - Plane to add to the solid.
// Output : Returns true if the solid is still valid after the addition of the
//			plane, false if it was entirely behind the plane.
//-----------------------------------------------------------------------------
bool CMapSolid::AddPlane( const CMapFace* p )
{
	CMapFace NewFace;

	//
	// Copy the info from the carving face, including the plane, but not the points.
	//
	NewFace.CopyFrom( p, COPY_FACE_PLANE );

	//
	// Use texture from our first face - this function adds a plane
	// from the subtraction brush itself.
	//
	const CMapFace* pSolidFace = GetFace( 0 );

	NewFace.SetTexture( pSolidFace->texture.texture );
	NewFace.texture.q2contents = pSolidFace->texture.q2contents;
	NewFace.texture.q2surface = pSolidFace->texture.q2surface;
	NewFace.texture.q2value = pSolidFace->texture.q2value;

	//
	// Set up the texture axes for the new face.
	//
	NewFace.InitializeTextureAxes( Options.GetTextureAlignment(), INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE );

	//
	// Add the face to the solid and rebuild the solid.
	//
	AddFace( &NewFace );
	CreateFromPlanes();
	SetValid( TRUE );

	PostUpdate( Notify_Changed );

	RemoveEmptyFaces();

	return ( GetFaceCount() >= 4 );
}

//-----------------------------------------------------------------------------
// Purpose: Carves this solid using another.
// Input  : Inside - Receives the pieces of this solid that were inside the carver.
//			Outside - Receives the pieces of this solid that were outside the carver.
//			pCarver - The solid that is being subtracted from us.
//-----------------------------------------------------------------------------
bool CMapSolid::Carve( CMapObjectList* pInside, CMapObjectList* pOutside, CMapSolid* pCarver )
{
	int		   i;
	CMapSolid* front = NULL;
	CMapSolid* back = NULL;
	Vector	   bmins, bmaxs;
	Vector	   carvemins, carvemaxs;

	GetRender2DBox( bmins, bmaxs );
	pCarver->GetRender2DBox( carvemins, carvemaxs );

	//
	// If this solid doesn't intersect the carving solid at all, add a copy
	// to the outside list and exit.
	//
	for ( i = 0; i < 3; i++ )
	{
		if ( ( bmins[i] >= carvemaxs[i] ) || ( bmaxs[i] <= carvemins[i] ) )
		{
			if ( pOutside != NULL )
			{
				CMapSolid* pCopy = (CMapSolid*)Copy( false );
				pOutside->AddTail( pCopy );
			}
			return ( false );
		}
	}

	//
	// Build a duplicate of this solid to carve from.
	//
	CMapSolid CarveFrom;
	CarveFrom.CopyFrom( this, false );

	//
	// Carve the solid by the faces in the carving solid.
	//
	for ( i = 0; i < pCarver->GetFaceCount(); i++ )
	{
		const CMapFace* pFace = pCarver->GetFace( i );

		//
		// Split the solid by this face, into a front and a back piece.
		//
		CarveFrom.ClipByFace( pFace, pOutside != NULL ? &front : NULL, &back );

		//
		// If there was a front piece, add it to the outside list.
		//
		if ( ( front != NULL ) && ( pOutside != NULL ) )
			pOutside->AddTail( front );
		else
			delete front;

		//
		// If there was no back piece, we have found a face the solid is completely in front of.
		// Per the separating axis theorem, the two solids cannot intersect, so we are done.
		//
		if ( back == NULL )
			return ( false );

		//
		// The next clip uses the back piece from this clip to prevent the carve results
		// from overlapping.
		//
		CarveFrom.CopyFrom( back, false );

		//
		// Add the back piece of the carved solid to the inside list.
		//
		if ( pInside != NULL )
			pInside->AddTail( back );
		else
			delete back;
	}

	return ( true );
}

//-----------------------------------------------------------------------------
// Purpose: Clips the given solid by the given face, returning the results.
// Input  : pSolid - Solid to clip.
//			fa - Face to use for the clipping operation.
//			f - Returns the part of the solid that was in front of the clipping
//				face (in the direction of the face normal).
//			b - Returns the part of the solid that was in back of the clipping
//				face (in the opposite direction of the face normal).
//-----------------------------------------------------------------------------
void CMapSolid::ClipByFace( const CMapFace* fa, CMapSolid** f, CMapSolid** b )
{
	CMapSolid* front = new CMapSolid;
	CMapSolid* back = new CMapSolid;
	CMapFace   fb;

	//
	// Build a back facing version of the clip face by reversing the plane points
	// and recalculate the plane normal and distance.
	//
	fb.CopyFrom( fa );
	Vector temp = fb.plane.planepts[0];
	fb.plane.planepts[0] = fb.plane.planepts[2];
	fb.plane.planepts[2] = temp;
	fb.CalcPlane();

	front->CopyFrom( this, false );
	front->SetObjParent( NULL );

	back->CopyFrom( this, false );
	back->SetObjParent( NULL );

	if ( !back->AddPlane( fa ) )
	{
		delete back;
		back = NULL;
	}

	if ( !front->AddPlane( &fb ) )
	{
		delete front;
		front = NULL;
	}

	if ( f != NULL )
		*f = front;
	else
		delete front;

	if ( b != NULL )
		*b = back;
	else
		delete back;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this solid contains a face with the given ID, false if not.
// Input  : nFaceID - unique face ID to look for.
//-----------------------------------------------------------------------------
CMapFace* CMapSolid::FindFaceID( int nFaceID )
{
	int nFaceCount = GetFaceCount();
	for ( int nFace = 0; nFace < nFaceCount; nFace++ )
	{
		CMapFace* pFace = GetFace( nFace );
		if ( pFace->GetFaceID() == nFaceID )
			return ( pFace );
	}

	return ( NULL );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pWorld -
//-----------------------------------------------------------------------------
void CMapSolid::GenerateNewFaceIDs( CMapWorld* pWorld )
{
	int nFaceCount = GetFaceCount();
	for ( int nFace = 0; nFace < nFaceCount; nFace++ )
	{
		CMapFace* pFace = GetFace( nFace );
		pFace->SetFaceID( pWorld->FaceID_GetNext() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allows the solid to generate new face IDs before being added to the
//			world because of a clone.
// Input  : pClone - The clone of this object that is being added to the world.
//			pWorld - The world that the clone is being added to.
//			OriginalList - The list of objects that were cloned.
//			NewList - The list of clones.
//-----------------------------------------------------------------------------
void CMapSolid::OnPreClone( CMapClass* pClone, CMapWorld* pWorld, CMapObjectList& OriginalList,
							CMapObjectList& NewList )
{
	( (CMapSolid*)pClone )->GenerateNewFaceIDs( pWorld );
}

//-----------------------------------------------------------------------------
// Purpose: Notifies the object that a copy of it is being pasted from the
//			clipboard before the copy is added to the world.
// Input  : pCopy - The copy of this object that is being added to the world.
//			pSourceWorld - The world that the originals were in.
//			pDestWorld - The world that the copies are being added to.
//			OriginalList - The list of original objects that were copied.
//			NewList - The list of copied.
//-----------------------------------------------------------------------------
void CMapSolid::OnPrePaste( CMapClass* pCopy, CMapWorld* pSourceWorld, CMapWorld* pDestWorld,
							CMapObjectList& OriginalList, CMapObjectList& NewList )
{
	( (CMapSolid*)pCopy )->GenerateNewFaceIDs( pDestWorld );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a face to this solid.
// Input  : pFace - The face to add. The face is copied into this solid's face
//				array, so it can be safely freed when AddFace returns.
//-----------------------------------------------------------------------------
void CMapSolid::AddFace( CMapFace* pFace )
{
	int nFaces = Faces.GetCount();
	Faces.SetCount( nFaces + 1 );
	CMapFace* pNewFace = &Faces[nFaces];

	pNewFace->CopyFrom( pFace, COPY_FACE_POINTS );
	pNewFace->SetRenderColor( r, g, b );
	pNewFace->SetParent( this );
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass* CMapSolid::Copy( bool bUpdateDependencies )
{
	CMapSolid* pNew = new CMapSolid;
	pNew->CopyFrom( this, bUpdateDependencies );
	return pNew;
}

//-----------------------------------------------------------------------------
// Purpose: Turns this solid into a duplicate of the given solid.
// Input  : pobj - Object to copy, must be a CMapSolid.
// Output : Returns a pointer to this object.
//-----------------------------------------------------------------------------
CMapClass* CMapSolid::CopyFrom( CMapClass* pobj, bool bUpdateDependencies )
{
	ASSERT( pobj->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) );
	CMapSolid* pFrom = (CMapSolid*)pobj;

	CMapClass::CopyFrom( pobj, bUpdateDependencies );
	m_eSolidType = pFrom->GetHL1SolidType();

	int nFaces = pFrom->Faces.GetCount();
	Faces.SetCount( nFaces );

	// copy faces
	CMapFace* pFromFace;
	CMapFace* pToFace;

	for ( int i = nFaces - 1; i >= 0; i-- )
	{

		pToFace = &Faces[i];
		pFromFace = &pFrom->Faces[i];

		if ( !pToFace )
			continue;

		pToFace->SetParent( this );
		pToFace->CopyFrom( pFromFace, COPY_FACE_POINTS, bUpdateDependencies );
		ASSERT( pToFace->GetPointCount() != 0 );
	}

	return ( this );
}

//-----------------------------------------------------------------------------
// Purpose: Walks the faces of a solid for debugging.
//-----------------------------------------------------------------------------
#ifdef _DEBUG
void CMapSolid::DebugSolid( void )
{
	int nFaceCount = Faces.GetCount();
	for ( int nFace = 0; nFace < nFaceCount; nFace++ )
		CMapFace* pFace = GetFace( nFace );
}
#endif // _DEBUG

//-----------------------------------------------------------------------------
// Purpose:
// Input  : iIndex -
//-----------------------------------------------------------------------------
void CMapSolid::DeleteFace( int iIndex )
{
	// shifts 'em down.
	int nFaces = Faces.GetCount();
	for ( int j = iIndex; j < nFaces - 1; j++ )
		Faces[j].CopyFrom( &Faces[j + 1] );

	Faces.SetCount( nFaces - 1 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
LPCTSTR CMapSolid::GetDescription( void )
{
	static char szBuf[128];
	sprintf( szBuf, "solid with %d faces", Faces.GetCount() );
	return szBuf;
}

//-----------------------------------------------------------------------------
// Purpose: Calculates this solid's axis aligned bounding box.
// Input  : bFullUpdate - Whether to evaluate all children when calculating.
//-----------------------------------------------------------------------------
void CMapSolid::CalcBounds( BOOL bFullUpdate )
{
	CMapClass::CalcBounds( bFullUpdate );

	//
	// Update mins/maxes based on our faces.
	//
	int nFaces = Faces.GetCount();
	for ( int i = 0; i < nFaces; i++ )
	{
		// check for valid face
		if ( !Faces[i].Points )
			continue;

		//
		// Get the 2d render bounds of this face and update the solid. 2D render bounds
		// can be different from 3D culling bounds because the 2D bounds do not consider
		// displacement faces.
		//
		Vector mins, maxs;
		bool   result = Faces[i].GetRender2DBox( mins, maxs );
		if ( result )
			m_Render2DBox.UpdateBounds( mins, maxs );

		//
		// Get the culling bounds and update the solid
		//
		result = Faces[i].GetCullBox( mins, maxs );
		if ( result )
			m_CullBox.UpdateBounds( mins, maxs );
	}

	m_Render2DBox.GetBoundsCenter( m_Origin );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : t -
//-----------------------------------------------------------------------------
void CMapSolid::DoTransform( Box3D* t )
{
	KeepForUndo();
	// get all points, transform them
	int nFaces = Faces.GetCount();
	for ( int i = 0; i < nFaces; i++ )
	{
		CMapFace& curface = Faces[i];

		//
		// Offset texture coordinates if we're moving and texture locking.
		//
		if ( t->IsMoving() && Options.IsLockingTextures() && ( curface.GetTexture() != NULL ) )
		{
			Vector delta( (float)t->m_Scale[0], (float)t->m_Scale[1], (float)t->m_Scale[2] );
			curface.OffsetTexture( delta );
		}

		//
		// Transform the face points.
		//
		for ( int j = 0; j < curface.nPoints; j++ )
			t->TranslatePoint( curface.Points[j] );

		//
		// Recalculate the plane normal and distance.
		//
		curface.CalcPlaneFromFacePoints();

		if ( t->IsRotating() )
		{
			//
			// If we are rotating and texture locking, rotate the texture axes and
			// update the U/V shifts.
			//
			if ( Options.IsLockingTextures() )
			{
				// Move the rotation origin to the world origin.
				Vector SaveOrg = t->stat.rotate_org;
				ZeroVector( t->stat.rotate_org );

				// Rotate the U/V axes to keep them oriented the same relative
				// to this face.
				t->TranslatePoint( curface.texture.UAxis.AsVector3D() );
				t->TranslatePoint( curface.texture.VAxis.AsVector3D() );

				// Restore the rotate origin to the original value.
				t->stat.rotate_org = SaveOrg;

				// Calculate the shift in U/V space due to this rotation.
				Vector Shift;
				ZeroVector( Shift );
				t->TranslatePoint( Shift );

				curface.texture.UAxis[3] -=
					DotProduct( Shift, curface.texture.UAxis.AsVector3D() ) / curface.texture.scale[0];
				curface.texture.VAxis[3] -=
					DotProduct( Shift, curface.texture.VAxis.AsVector3D() ) / curface.texture.scale[1];
				curface.NormalizeTextureShifts();
			}
			//
			// If we are not texture locking, recalculate the texture axes based on current
			// texture alignment setting. This prevents the axes from becoming normal to the face.
			//
			else
			{
				curface.InitializeTextureAxes( Options.GetTextureAlignment(), INIT_TEXTURE_AXES | INIT_TEXTURE_FORCE );
			}

		}

		// Recalculate the texture coordinates for this face.
		curface.CalcTextureCoords();
	}

	CMapClass::DoTransform( t );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : delta -
//-----------------------------------------------------------------------------
void CMapSolid::DoTransMove( const Vector& delta )
{
	KeepForUndo();
	int nFaces = Faces.GetCount();
	for ( int face = 0; face < nFaces; face++ )
	{
		CMapFace& curface = Faces[face];

		if ( ( curface.GetTexture() != NULL ) && Options.IsLockingTextures() )
			curface.OffsetTexture( delta );

		//
		// Move the face points.
		//
		for ( int i = 0; i < curface.nPoints; i++ )
		{
			curface.Points[i][0] += delta[0];
			curface.Points[i][1] += delta[1];
			curface.Points[i][2] += delta[2];
		}

		//
		// Recalculate the plane and the texture coordinates.
		//
		curface.CalcPlaneFromFacePoints();
		curface.CalcTextureCoords();

	}

	CMapClass::DoTransMove( delta );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : RefPoint -
//			Scale -
//-----------------------------------------------------------------------------
void CMapSolid::DoTransScale( const Vector& RefPoint, const Vector& Scale )
{
	KeepForUndo();
	int nFaces = Faces.GetCount();
	for ( int face = 0; face < nFaces; face++ )
	{
		float fShift[2];

		CMapFace* pFace = &Faces[face];

		if ( Options.IsLockingTextures() )
		{
			fShift[0] = DotProduct( pFace->texture.UAxis.AsVector3D(), pFace->Points[0] ) / pFace->texture.scale[0];
			fShift[1] = DotProduct( pFace->texture.VAxis.AsVector3D(), pFace->Points[0] ) / pFace->texture.scale[1];
		}

		// Scale the plane points.
		int i;
		//
		//
		for ( i = 0; i < 3; i++ )
		{
			pFace->plane.planepts[i][0] = RefPoint[0] + ( ( pFace->plane.planepts[i][0] - RefPoint[0] ) * Scale[0] );
			pFace->plane.planepts[i][1] = RefPoint[1] + ( ( pFace->plane.planepts[i][1] - RefPoint[1] ) * Scale[1] );
			pFace->plane.planepts[i][2] = RefPoint[2] + ( ( pFace->plane.planepts[i][2] - RefPoint[2] ) * Scale[2] );
		}

		//
		// Scale the face points.
		//
		for ( i = 0; i < pFace->nPoints; i++ )
		{
			pFace->Points[i][0] = RefPoint[0] + ( ( pFace->Points[i][0] - RefPoint[0] ) * Scale[0] );
			pFace->Points[i][1] = RefPoint[1] + ( ( pFace->Points[i][1] - RefPoint[1] ) * Scale[1] );
			pFace->Points[i][2] = RefPoint[2] + ( ( pFace->Points[i][2] - RefPoint[2] ) * Scale[2] );
		}

		//
		// Adjust the U/V scales and shifts if texture locking is enabled.
		//
		if ( Options.IsLockingTextures() )
		{
			//
			// Build a vector representing the components of scale along the U axis.
			// Find the projection of that vector on the U axis to determine the U scale.
			//
			Vector Temp;
			Vector TexScale;

			ZeroVector( Temp );
			Temp[0] = Scale[0];
			TexScale[0] = DotProduct( pFace->texture.UAxis.AsVector3D(), Temp );

			ZeroVector( Temp );
			Temp[1] = Scale[1];
			TexScale[1] = DotProduct( pFace->texture.UAxis.AsVector3D(), Temp );

			ZeroVector( Temp );
			Temp[2] = Scale[2];
			TexScale[2] = DotProduct( pFace->texture.UAxis.AsVector3D(), Temp );

			pFace->texture.scale[0] *= DotProduct( pFace->texture.UAxis.AsVector3D(), TexScale );
			if ( pFace->texture.scale[0] == 0 )
				pFace->texture.scale[0] = 1.0f;

			//
			// Build a vector representing the components of scale along the V axis.
			// Find the projection of that vector on the V axis to determine the V scale.
			//
			ZeroVector( Temp );
			Temp[0] = Scale[0];
			TexScale[0] = DotProduct( pFace->texture.VAxis.AsVector3D(), Temp );

			ZeroVector( Temp );
			Temp[1] = Scale[1];
			TexScale[1] = DotProduct( pFace->texture.VAxis.AsVector3D(), Temp );

			ZeroVector( Temp );
			Temp[2] = Scale[2];
			TexScale[2] = DotProduct( pFace->texture.VAxis.AsVector3D(), Temp );
			pFace->texture.scale[1] *= DotProduct( pFace->texture.VAxis.AsVector3D(), TexScale );
			if ( pFace->texture.scale[1] == 0 )
				pFace->texture.scale[1] = 1.0f;

			//
			// Calculate the U/V shifts due to the scaling.
			//
			pFace->texture.UAxis[3] -=
				DotProduct( pFace->texture.UAxis.AsVector3D(), pFace->Points[0] ) / pFace->texture.scale[0] - fShift[0];
			pFace->texture.VAxis[3] -=
				DotProduct( pFace->texture.VAxis.AsVector3D(), pFace->Points[0] ) / pFace->texture.scale[1] - fShift[1];
			pFace->NormalizeTextureShifts();
		}

		//
		// Recalculate the plane and the texture coordinates.
		//
		pFace->CalcPlane();
		pFace->CalcTextureCoords();

	}

	CMapClass::DoTransScale( RefPoint, Scale );
}

//-----------------------------------------------------------------------------
// Purpose: Flips a given array of points around a reference point.
// Input  : RefPoint -
//			pPoints -
//			nCount -
//-----------------------------------------------------------------------------
static void FlipPoints( const Vector& RefPoint, Vector* pPoints, int nCount )
{
	static Vector tempPts[256]; // For holding the reversed list of points.

	for ( int i = 0; i < nCount; i++ )
	{
		for ( int nDim = 0; nDim < 3; nDim++ )
			if ( RefPoint[nDim] == COORD_NOTINIT )
				tempPts[nCount - i - 1][nDim] = pPoints[i][nDim];
			else
				tempPts[nCount - i - 1][nDim] = RefPoint[nDim] - ( pPoints[i][nDim] - RefPoint[nDim] );
	}

	// Put the flipped array in back in the original points array.
	memcpy( pPoints, tempPts, sizeof( Vector ) * nCount );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : RefPoint -
//-----------------------------------------------------------------------------
void CMapSolid::DoTransFlip( const Vector& RefPoint )
{
	KeepForUndo();
	int nFaces = Faces.GetCount();
	for ( int face = 0; face < nFaces; face++ )
	{
		Faces[face].TransFlip( RefPoint );

		FlipPoints( RefPoint, Faces[face].Points, Faces[face].nPoints );
		FlipPoints( RefPoint, Faces[face].plane.planepts, 3 );

		//
		// Recalculate the plane normal and distance.
		//
		Faces[face].CalcPlane();

		//
		// Flip the U/V axes. This is done by negating the sign of the component
		// that corresponds to the flip dimension.
		//
		if ( Options.IsLockingTextures() )
		{
			Vector Shift;
			ZeroVector( Shift );

			for ( int j = 0; j < 3; j++ )
			{
				if ( RefPoint[j] != COORD_NOTINIT )
				{
					Faces[face].texture.UAxis[j] = -Faces[face].texture.UAxis[j];
					Faces[face].texture.VAxis[j] = -Faces[face].texture.VAxis[j];
					Shift[j] = RefPoint[j];
				}
			}

			//
			// Adjust the U/V shifts to allow for the shift in U/V space of the face points.
			// Since our texture axes have flipped, the texture shift is twice the projection
			// of the flip reference point in U/V space.
			//
			Faces[face].texture.UAxis[3] -=
				2 * DotProduct( Faces[face].texture.UAxis.AsVector3D(), Shift ) / Faces[face].texture.scale[0];
			Faces[face].texture.VAxis[3] -=
				2 * DotProduct( Faces[face].texture.VAxis.AsVector3D(), Shift ) / Faces[face].texture.scale[1];
			Faces[face].NormalizeTextureShifts();
		}

		// udpate and notify the displacement map -- if any?

		Faces[face].CalcTextureCoords();
	}

	CMapClass::DoTransFlip( RefPoint );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : ptRef -
//			angles -
//			pPoints -
//			nCount -
//-----------------------------------------------------------------------------
static void RotatePoints( const Vector& RefPoint, const QAngle& angles, Vector* pPoints, int nCount )
{
	//
	// Rotate the face points.
	//
	for ( int i = 0; i < nCount; i++ )
	{
		Vector& pt3 = pPoints[i];

		pt3 -= RefPoint;

		// rotate x
		if ( angles[0] )
			rotate_coords( &pt3[1], &pt3[2], pt3[1], pt3[2], angles[0] );

		if ( angles[1] )
			rotate_coords( &pt3[0], &pt3[2], pt3[0], pt3[2], angles[1] );

		if ( angles[2] )
			rotate_coords( &pt3[0], &pt3[1], pt3[0], pt3[1], angles[2] );

		pt3 += RefPoint;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : RefPoint -
//			angles -
//-----------------------------------------------------------------------------
void CMapSolid::DoTransRotate( const Vector* pRefPoint, const QAngle& Angles )
{
	KeepForUndo();
	static Vector ptMyCenter;

	if ( !pRefPoint )
	{
		m_Render2DBox.GetBoundsCenter( ptMyCenter );
		pRefPoint = &ptMyCenter;
	}

	int nFaces = Faces.GetCount();
	for ( int face = 0; face < nFaces; face++ )
	{
		RotatePoints( *pRefPoint, Angles, Faces[face].Points, Faces[face].nPoints );
		RotatePoints( *pRefPoint, Angles, Faces[face].plane.planepts, 3 );

		//
		// Recalculate the plane.
		//
		Faces[face].CalcPlane();

		//
		// If we are texture locking, rotate the texture axes and update the U/V shift.
		//
		if ( Options.IsLockingTextures() )
		{
			// Rotate the U/V axes to keep them oriented the same relative to this face.
			if ( Angles[0] )
			{
				rotate_coords( &Faces[face].texture.UAxis[1], &Faces[face].texture.UAxis[2],
							   Faces[face].texture.UAxis[1], Faces[face].texture.UAxis[2], Angles[0] );
				rotate_coords( &Faces[face].texture.VAxis[1], &Faces[face].texture.VAxis[2],
							   Faces[face].texture.VAxis[1], Faces[face].texture.VAxis[2], Angles[0] );
			}

			if ( Angles[1] )
			{
				rotate_coords( &Faces[face].texture.UAxis[0], &Faces[face].texture.UAxis[2],
							   Faces[face].texture.UAxis[0], Faces[face].texture.UAxis[2], Angles[1] );
				rotate_coords( &Faces[face].texture.VAxis[0], &Faces[face].texture.VAxis[2],
							   Faces[face].texture.VAxis[0], Faces[face].texture.VAxis[2], Angles[1] );
			}

			if ( Angles[2] )
			{
				rotate_coords( &Faces[face].texture.UAxis[0], &Faces[face].texture.UAxis[1],
							   Faces[face].texture.UAxis[0], Faces[face].texture.UAxis[1], Angles[2] );
				rotate_coords( &Faces[face].texture.VAxis[0], &Faces[face].texture.VAxis[1],
							   Faces[face].texture.VAxis[0], Faces[face].texture.VAxis[1], Angles[2] );
			}

			// Calculate the shift in U/V space due to this rotation.
			Vector Shift;
			ZeroVector( Shift );

			Shift -= *pRefPoint;

			if ( Angles[0] )
				rotate_coords( &Shift[1], &Shift[2], Shift[1], Shift[2], Angles[0] );

			if ( Angles[1] )
				rotate_coords( &Shift[0], &Shift[2], Shift[0], Shift[2], Angles[1] );

			if ( Angles[2] )
				rotate_coords( &Shift[0], &Shift[1], Shift[0], Shift[1], Angles[2] );

			Shift += *pRefPoint;

			Faces[face].texture.UAxis[3] -=
				DotProduct( Shift, Faces[face].texture.UAxis.AsVector3D() ) / Faces[face].texture.scale[0];
			Faces[face].texture.VAxis[3] -=
				DotProduct( Shift, Faces[face].texture.VAxis.AsVector3D() ) / Faces[face].texture.scale[1];
			Faces[face].NormalizeTextureShifts();
		}
		//
		// If we are not texture locking, recalculate the texture axes based on current
		// texture alignment setting. This prevents the axes from becoming normal to the face.
		//
		else
		{
			Faces[face].InitializeTextureAxes( Options.GetTextureAlignment(), INIT_TEXTURE_AXES | INIT_TEXTURE_FORCE );
		}

		Faces[face].CalcTextureCoords();

	}

	CMapClass::DoTransRotate( pRefPoint, Angles );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the render color of all of our faces when our render color is set.
//-----------------------------------------------------------------------------
void CMapSolid::SetRenderColor( color32 rgbColor )
{
	CMapClass::SetRenderColor( rgbColor );

	int nFaces = Faces.GetCount();
	for ( int i = 0; i < nFaces; i++ )
		Faces[i].SetRenderColor( rgbColor );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the render color of all of our faces when our render color is set.
//-----------------------------------------------------------------------------
void CMapSolid::SetRenderColor( unsigned char uchRed, unsigned char uchGreen, unsigned char uchBlue )
{
	CMapClass::SetRenderColor( uchRed, uchGreen, uchBlue );

	int nFaces = Faces.GetCount();
	for ( int i = 0; i < nFaces; i++ )
		Faces[i].SetRenderColor( uchRed, uchGreen, uchBlue );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output : size_t
//-----------------------------------------------------------------------------
size_t CMapSolid::GetSize( void )
{
	size_t size = CMapClass::GetSize();
	size += sizeof *this;

	int nFaces = Faces.GetCount();
	for ( int i = 0; i < nFaces; i++ )
		size += Faces[i].GetDataSize();

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the texture for a given face.
// Input  : pszTex - Texture name.
//			iFace - Index of face for which to set texture.
//-----------------------------------------------------------------------------
void CMapSolid::SetTexture( LPCTSTR pszTex, int iFace )
{
	if ( iFace == -1 )
	{
		int nFaces = Faces.GetCount();
		for ( int i = 0; i < nFaces; i++ )
			Faces[i].SetTexture( pszTex );
	}
	else
	{
		Faces[iFace].SetTexture( pszTex );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the texture name of a given face.
// Input  : iFace - Index of face. If -1, returns the texture of face 0.
// Output : Returns the texture name.
//-----------------------------------------------------------------------------
LPCTSTR CMapSolid::GetTexture( int iFace )
{
	return Faces[iFace == -1 ? 0 : iFace].texture.texture;
}

//-----------------------------------------------------------------------------
// Purpose: Creates the solid using the plane information from the solid's faces.
//
//			ASSUMPTIONS: This solid's faces are assumed to have valid plane points.
//
// Input  : dwFlags - Can be any or none of the following flags:
//
//				CREATE_BUILD_PLANE_POINTS - if this flag is set, the 3-point
//					definition of each face plane will be regenerated based
//					on the face points after the solid is generated.
//
// Output : Returns TRUE if the solid is valid, FALSE if not.
//
// dvs: this should really use the public API of CMapSolid to add faces so that
//      parentage and render color are set automatically.
//-----------------------------------------------------------------------------
int CMapSolid::CreateFromPlanes( DWORD dwFlags )
{
	BOOL useplane[MAPSOLID_MAX_FACES];
	memset( useplane, 0, sizeof( useplane ) );
	m_Render2DBox.SetBounds( Vector( COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT ),
		Vector( -COORD_NOTINIT, -COORD_NOTINIT, -COORD_NOTINIT ) );
	m_bValid = TRUE;

	//
	// Free all points from all faces and assign parentage.
	//
	int nFaces = GetFaceCount();
	for ( int i = 0; i < nFaces; i++ )
	{
		CMapFace* pFace = GetFace( i );
		pFace->AllocatePoints( 0 );
		pFace->SetParent( this );
		pFace->SetRenderColor( r, g, b );
	}

	//
	// For every face that is not set to be ignored, check the plane and make sure
	// it is unique. We mark each plane that we intend to keep with a TRUE in the
	// 'useplane' array.
	//
	for ( int i = 0; i < nFaces; i++ )
	{
		CMapFace* pFace = GetFace( i );
		if ( pFace->IsIgnored() )
			continue;

		pFace->CalcPlane();
		PLANE* plane = &pFace->plane;
		if ( VectorCompare( plane->normal, vec3_origin ) )
			continue;

		//
		// If the plane duplicates another plane, don't use it (assume it is a brush
		// being edited that will be fixed).
		//
		useplane[i] = TRUE;
		for ( int j = 0; j < i; j++ )
		{
			CMapFace* pOther = GetFace( j );
			if ( !pOther->IsIgnored() &&
				 ( ( (double)pOther->plane.normal[2] * plane->normal[2] + (double)pOther->plane.normal[1] * plane->normal[1] )
				   + (double)plane->normal[0] * pOther->plane.normal[0] ) > 0.999 &&
				 fabs( (double)plane->dist - pOther->plane.dist ) < 0.01 )
			{
				useplane[j] = FALSE;
				break;
			}
		}
	}

	//
	// Now we have a set of planes, indicated by TRUE values in the 'useplanes' array,
	// from which we will build a solid.
	//
	BOOL bGotFaces = FALSE;
	for ( int i = 0; i < nFaces; i++ )
	{
		CMapFace* pFace = GetFace( i );
		//
		// Don't use ignored faces or faces marked as not to be used.
		//
		if ( !useplane[i] || pFace->IsIgnored() )
			continue;

		//
		// Create a huge winding from this face's plane, then clip it by all other
		// face planes.
		//
		winding_t* winding = CreateWindingFromPlane( &pFace->plane );
		for ( int j = 0; j < nFaces && winding; j++ )
		{
			CMapFace* pClip = GetFace( j );
			if ( j != i && !pClip->IsIgnored() )
			{
				PLANE plane;
				VectorNegate( pClip->plane.normal, plane.normal );
				plane.dist = -pClip->plane.dist;
				winding = ClipWinding( winding, &plane );
			}
		}
		if ( !winding )
			continue;

		//
		// Round all points in the winding that are within ROUND_VERTEX_EPSILON of
		// integer values.
		//
		for ( int j = 0; j < winding->numpoints; j++ )
		{
			for ( int axis = 0; axis < 3; axis++ )
			{
				float value = winding->p[j][axis];
				float rounded = WorldcraftRint( value );
				if ( fabs( value - rounded ) < ROUND_VERTEX_EPSILON )
					winding->p[j][axis] = rounded;
			}
		}

		bGotFaces = TRUE;
		pFace->CreateFace( winding );
		if ( !pFace->CheckFace() )
			m_bValid = FALSE;
		//
		// Done with the winding, we can free it now.
		//
		FreeWinding( winding );
	}

	if ( !bGotFaces )
	{
		m_bValid = FALSE;
		m_Render2DBox.SetBounds( vec3_origin, vec3_origin );
	}
	else if ( !( dwFlags & CREATE_KEEP_UNUSED_FACES ) )
	{
		for ( int i = 0; i < nFaces; i++ )
		{
			if ( !useplane[i] && !GetFace( i )->IsIgnored() )
			{
				DeleteFace( i );
				if ( i + 1 < nFaces )
					memmove( &useplane[i], &useplane[i + 1], ( nFaces - i - 1 ) * sizeof( useplane[0] ) );
				nFaces--;
				i--;
			}
		}
	}

	if ( !( dwFlags & CREATE_PRESERVE_TEXTURE_AXES ) )
		InitializeTextureAxes( Options.GetTextureAlignment(), INIT_TEXTURE_ALL );
	return m_bValid ? TRUE : FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the texture axes for all faces in the solid.
// Input  : eAlignment - See CMapFace::InitializeTextureAxes
//			dwFlags - See CMapFace::InitializeTextureAxes
//-----------------------------------------------------------------------------
void CMapSolid::InitializeTextureAxes( TextureAlignment_t eAlignment, DWORD dwFlags )
{
	int nFaces = Faces.GetCount();
	for ( int i = 0; i < nFaces; i++ )
		Faces[i].InitializeTextureAxes( eAlignment, dwFlags );
}

//-----------------------------------------------------------------------------
// Purpose: Called after this object is added to the world.
//
//			NOTE: This function is NOT called during serialization. Use PostloadWorld
//				  to do similar bookkeeping after map load.
//
// Input  : pWorld - The world that we have been added to.
//-----------------------------------------------------------------------------
void CMapSolid::OnAddToWorld( CMapWorld* pWorld )
{
	CMapClass::OnAddToWorld( pWorld );

	//
	// First, the common case: all our face IDs are zero. Assign new IDs to all faces
	// with zero IDs. Add unhandled faces to a list. Those we will need to check against
	// the world for uniqueness.
	//
	CMapFaceList CheckList;
	int			 nFaceCount = GetFaceCount();
	for ( int i = 0; i < nFaceCount; i++ )
	{
		CMapFace* pFace = GetFace( i );
		if ( pFace->GetFaceID() == 0 )
			pFace->SetFaceID( pWorld->FaceID_GetNext() );
		else
			CheckList.AddToTail( pFace );
	}

	if ( CheckList.Count() > 0 )
	{
		//
		// The less common case: make sure all our face IDs are unique in this world.
		// We do it here instead of in CMapFace in order to save world tree traversals.
		//
		EnumChildrenPos_t pos;
		CMapClass*		  pChild = pWorld->GetFirstDescendent( pos );
		while ( pChild != NULL )
		{
			CMapSolid* pSolid = dynamic_cast<CMapSolid*>( pChild );
			if ( pSolid != NULL )
			{
				if ( CheckList.Count() > 0 )
				{
					for ( int i = CheckList.Count() - 1; i >= 0; i-- )
					{
						CMapFace* pFace = CheckList.Element( i );

						//
						// If this face ID is not unique, assign it a new unique face ID
						// and remove it from our list.
						//
						if ( ( pSolid != this ) && ( pSolid->FindFaceID( pFace->GetFaceID() ) != NULL ) )
						{
							pFace->SetFaceID( pWorld->FaceID_GetNext() );
							CheckList.FastRemove( i );
						}
					}
				}

				if ( CheckList.Count() <= 0 )
				{
					//
					// We've handled all the faces in our list, early out.
					//
					break;
				}
			}

			pChild = pWorld->GetNextDescendent( pos );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called after the entire map has been loaded. This allows the object
//			to perform any linking with other map objects or to do other operations
//			that require all world objects to be present.
// Input  : pWorld - The world that we are in.
//-----------------------------------------------------------------------------
void CMapSolid::PostloadWorld( CMapWorld* pWorld )
{
	CMapClass::PostloadWorld( pWorld );

	//
	// Make sure all our faces have nonzero IDs. They might if the map was created
	// before unique IDs were added.
	//
	int nFaces = GetFaceCount();
	for ( int i = 0; i < nFaces; i++ )
	{
		CMapFace* pFace = GetFace( i );
		if ( pFace->GetFaceID() == 0 )
			pFace->SetFaceID( pWorld->FaceID_GetNext() );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : eSelectMode -
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass* CMapSolid::PrepareSelection( SelectMode_t eSelectMode )
{
	//
	// If we have a parent who is not the world object, consider whether we should
	// select it instead.
	//
	if ( ( eSelectMode != selectSolids ) && ( Parent != NULL ) && !Parent->IsWorldObject() )
	{
		//
		// If we are in group selection mode or our parent is an entity, select our
		// parent.
		//
		if ( ( eSelectMode == selectGroups ) || ( dynamic_cast<CMapEntity*>( Parent ) != NULL ) )
			return Parent->PrepareSelection( eSelectMode );
	}

	return this;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapSolid::RemoveEmptyFaces( void )
{
	int nFaces = GetFaceCount();

	for ( int i = 0; i < nFaces; i++ )
	{
		//
		// If this face has no points, delete it.
		//
		const CMapFace* pFace = GetFace( i );
		if ( pFace->Points == NULL )
		{
			DeleteFace( i );
			i--;
			nFaces--;
		}
	}

	if ( nFaces >= 4 )
	{
		// dvs: test to verify that the SetFaceCount below is unnecessary
		int nTest = GetFaceCount();
		ASSERT( nTest == nFaces );
		SetFaceCount( nFaces );
	}
}

//-----------------------------------------------------------------------------
// for sorting
//-----------------------------------------------------------------------------

bool CMapSolid::ShouldRenderLast()
{
	for ( int nFace = 0; nFace < GetFaceCount(); nFace++ )
	{
		CMapFace* pFace = GetFace( nFace );
		if ( pFace->ShouldRenderLast() )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Renders the solid using the default render mode. If the solid is
//			currently selected, it will be rendered with a yellow wireframe
//			in a second pass.
// Input  : pRender - Rendering interface.
//-----------------------------------------------------------------------------
void CMapSolid::Render3D( CRender3D* pRender )
{
	pRender->SetRenderMode( RENDER_MODE_DEFAULT );

	//
	// determine whether or not this is a displacement solid - i.e. one of the faces
	// on this solid is displaced
	//
	CMapDoc* pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	//
	// Determine whether we need to render in one or two passes. If we are selected,
	// and rendering in flat or textured mode, we need to render using two passes.
	//
	int				 nPasses;
	SelectionState_t eSolidSelectionState = GetSelectionState();
	RenderMode_t	 eDefaultRenderMode = pRender->GetDefaultRenderMode();
	if ( ( eSolidSelectionState != SELECT_NONE ) && ( eDefaultRenderMode != RENDER_MODE_WIREFRAME ) )
		nPasses = 2;
	else
		nPasses = 1;

	int iStartPass = 1;
	for ( int nPass = iStartPass; nPass <= nPasses; nPass++ )
	{
		//
		// Render the second pass in wireframe.
		//
		if ( nPass == 2 )
		{
			pRender->SetRenderMode( RENDER_MODE_WIREFRAME );
			glPolygonMode( GL_FRONT, GL_LINE );
			glEnable( GL_CULL_FACE );
			glEnable( GL_DEPTH_TEST );
			pRender->RenderEnable( RENDER_POLYGON_OFFSET_LINE, true );
		}

		for ( int nFace = 0; nFace < GetFaceCount(); nFace++ )
		{
			CMapFace* pFace = GetFace( nFace );

			// only render displaced faces on a displaced solid when the displacement
			// solid render mask is set

			{
				pRender->BeginRenderHitTarget( this, nFace );
				pFace->Render3D( pRender );
				pRender->EndRenderHitTarget();
			}
		}
	}

	// Put the original render mode back.
	if ( nPasses == 2 )
		pRender->RenderEnable( RENDER_POLYGON_OFFSET_LINE, false );
	pRender->SetRenderMode( RENDER_MODE_DEFAULT );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a solid type for the given texture name.
// Input  : pszTexture -
//-----------------------------------------------------------------------------
HL1_SolidType_t CMapSolid::HL1SolidTypeFromTextureName( const char* pszTexture )
{
	HL1_SolidType_t eSolidType;

	if ( pszTexture[0] == '*' )
	{
		if ( !strncmp( pszTexture + 1, "slime", 5 ) )
			eSolidType = btSlime;
		else if ( !strncmp( pszTexture + 1, "lava", 4 ) )
			eSolidType = btLava;
		else
			eSolidType = btWater;
	}
	else
	{
		eSolidType = btSolid;
	}

	return ( eSolidType );
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether this brush was created by the cordon tool. Brushes that
//			were created by the cordon tool are not loaded.
// Input  : bSet - true to set, false to clear.
//-----------------------------------------------------------------------------
void CMapSolid::SetCordonBrush( bool bSet )
{
	m_bIsCordonBrush = bSet;
}

//-----------------------------------------------------------------------------
// Purpose: Subtracts one solid from another.
// Input  : pSubtraction - Solid (or group of solids) to subtract with.
//			pOther - Solid (or group of solids) to subtract from.
//			pSubParent - Receives the results of the subtraction as children.
// Output : Returns true if the objects intersected (subtraction was performed),
//			false if the objects did not intersect (no subtraction was performed).
//-----------------------------------------------------------------------------
bool CMapSolid::Subtract( CMapObjectList* pInside, CMapObjectList* pOutside, CMapClass* pSubtractWith )
{
	//
	// Build a list of solids to subtract with.
	//
	CMapObjectList SubList;
	if ( pSubtractWith->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) )
		SubList.AddTail( pSubtractWith );

	EnumChildrenPos_t pos;
	CMapClass*		  pChild = pSubtractWith->GetFirstDescendent( pos );
	while ( pChild != NULL )
	{
		CMapSolid* pSolid = dynamic_cast<CMapSolid*>( pChild );
		if ( pSolid != NULL )
			SubList.AddTail( pSolid );

		pChild = pSubtractWith->GetNextDescendent( pos );
	}

	//
	// For every solid that we are subtracting with...
	//
	bool bIntersected = false;

	POSITION p = SubList.GetHeadPosition();
	while ( p != NULL )
	{
		CMapSolid* pCarver = (CMapSolid*)SubList.GetNext( p );

		//
		// Subtract the 'with' solid from the 'from' solid, and place the
		// results in the carve_in and carve_out lists.
		//
		CMapObjectList carve_in;
		CMapObjectList carve_out;

		CMapObjectList* pCarveIn = NULL;
		CMapObjectList* pCarveOut = NULL;

		if ( pInside != NULL )
			pCarveIn = &carve_in;

		if ( pOutside != NULL )
			pCarveOut = &carve_out;

		bIntersected |= Carve( pCarveIn, pCarveOut, pCarver );

		if ( pInside != NULL )
		{
			pInside->AddTail( &carve_in );
			carve_in.RemoveAll();
		}

		if ( pOutside != NULL )
		{
			pOutside->AddTail( &carve_out );
			carve_out.RemoveAll();
		}
	}

	return ( bIntersected );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
color32 CMapSolid::GetLineColor()
{
	//
	// If the solid is not selected, determine the appropriate pen color.
	//
	if ( !IsSelected() )
	{
		//
		// If this is a solid entity, use the entity pen color.
		//
		CMapEntity* pEntity = dynamic_cast<CMapEntity*>( GetParent() );
		if ( pEntity != NULL )
		{
			GDclass* pClass = pEntity->GetClass();
			if ( pClass )
			{
				return pClass->GetColor();
			}
			else
			{
				color32 clr;
				clr.r = GetRValue( Options.colors.clrEntity );
				clr.g = GetGValue( Options.colors.clrEntity );
				clr.b = GetBValue( Options.colors.clrEntity );
				clr.a = 255;
				return clr;
			}
		}
		//
		// Otherwise, use the solid color.
		//
		else if ( Options.view2d.bUsegroupcolors )
		{
			return GetRenderColor();
		}
		else
		{
			color32 clr;
			clr.r = GetRValue( Options.colors.clrBrush );
			clr.g = GetGValue( Options.colors.clrBrush );
			clr.b = GetBValue( Options.colors.clrBrush );
			clr.a = 255;
			return clr;
		}
	}
	//
	// The solid is selected, use the selected pen color.
	//
	else
	{
		color32 clr;
		clr.r = GetRValue( Options.colors.clrSelection );
		clr.g = GetGValue( Options.colors.clrSelection );
		clr.b = GetBValue( Options.colors.clrSelection );
		clr.a = 255;
		return clr;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pRender -
//-----------------------------------------------------------------------------
void CMapSolid::Render2D( CRender2D* pRender )
{
	Vector vecMins;
	Vector vecMaxs;
	GetRender2DBox( vecMins, vecMaxs );

	CPoint pt;
	CPoint pt2;
	pRender->TransformPoint3D( pt, vecMins );
	pRender->TransformPoint3D( pt2, vecMaxs );

	color32 rgbLineColor = GetLineColor();

	CPoint ptCur;
	CPoint ptNext;

	int nFaces = GetFaceCount();
	for ( int i = 0; i < nFaces; i++ )
	{
		CMapFace* pFace = GetFace( i );
		int		  nPoints = pFace->nPoints;
		if ( !nPoints )
			continue;

		Vector* pPoints = pFace->Points;
		pRender->TransformPoint3D( ptCur, pPoints[0] );
		pRender->MoveTo( ptCur );

		for ( int j = 1; j <= nPoints; j++ )
		{
			if ( Options.view2d.bDrawVertices && ( j != nPoints ) )
			{
				//
				// Draw a small box at each vertex.
				//
				pRender->SetLineType( CRender2D::LINE_SOLID, CRender2D::LINE_THIN,
									  GetRValue( Options.colors.clrVertex ), GetGValue( Options.colors.clrVertex ),
									  GetBValue( Options.colors.clrVertex ) );
				pRender->SetFillColor( GetRValue( Options.colors.clrVertex ), GetGValue( Options.colors.clrVertex ),
									   GetBValue( Options.colors.clrVertex ) );

				RECT r;
				r.left = ptCur.x - 1;
				r.right = ptCur.x + 2;
				r.top = ptCur.y - 1;
				r.bottom = ptCur.y + 2;
				pRender->Rectangle( r, true );
			}

			//
			// Draw a line to the next vertex in the face.
			//
			pRender->TransformPoint3D( ptNext, pPoints[j % nPoints] );

			pRender->SetLineType( CRender2D::LINE_SOLID, CRender2D::LINE_THIN, rgbLineColor.r, rgbLineColor.g,
								  rgbLineColor.b );
			pRender->LineTo( ptNext );

			ptCur = ptNext;
		}
	}

	//
	// Draw center handle.
	//
	POINT ptCenter;
	ptCenter.x = ( pt.x + pt2.x ) / 2;
	ptCenter.y = ( pt.y + pt2.y ) / 2;

	pRender->SetLineType( CRender2D::LINE_SOLID, CRender2D::LINE_THIN, 150, 150, 150 );
	pRender->MoveTo( ptCenter.x - 3, ptCenter.y - 3 );
	pRender->LineTo( ptCenter.x + 4, ptCenter.y + 4 );
	pRender->MoveTo( ptCenter.x + 3, ptCenter.y - 3 );
	pRender->LineTo( ptCenter.x - 4, ptCenter.y + 4 );
}
