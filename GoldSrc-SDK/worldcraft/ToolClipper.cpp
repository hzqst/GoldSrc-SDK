//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "GlobalFunctions.h"
#include "History.h"
#include "MapDefs.h"
#include "MapDoc.h"
#include "MapFace.h"
#include "MapSolid.h"
#include "MapView2D.h"
#include "MapWorld.h"
#include "Options.h"
#include "Render2D.h"
#include "Render3D.h"
#include "RenderUtils.h"
#include "StatusBarIDs.h" // dvs: remove
#include "ToolClipper.h"
#include "ToolManager.h"

#pragma warning( disable : 4244 )

//=============================================================================
//
// Friend Function (for MapClass->EnumChildren Callback)
//

//-----------------------------------------------------------------------------
// Purpose: This function creates a new clip group with the given solid as
//          the original solid.
//   Input: pSolid - the original solid to put in the clip list
//          pClipper - the clipper tool
//  Output: successful?? (true/false)
//-----------------------------------------------------------------------------
BOOL AddToClipList( CMapSolid* pSolid, Clipper3D* pClipper )
{
	POSITION pos = pClipper->m_ClipResults.GetHeadPosition();
	while ( pos )
		if ( pClipper->m_ClipResults.GetNext( pos )->GetOrigSolid() == pSolid )
			return TRUE;

	CClipGroup* pClipGroup = new CClipGroup;
	pClipGroup->SetOrigSolid( pSolid );
	int nSides = pClipper->m_Mode == Clipper3D::BOTH ? 2 : 1;
	for ( int side = 0; side < nSides; side++ )
	{
		CMapSolid* pClipSolid = new CMapSolid;
		pClipSolid->CopyFrom( pSolid, false );
		pClipSolid->SetObjParent( NULL );
		pClipSolid->SetTemporary( TRUE );
		CMapFace face;
		if ( pSolid->GetFaceCount() )
		{
			CMapFace* pFirst = pSolid->GetFace( 0 );
			face.SetTexture( pFirst->GetTexture() );
			face.texture.q2surface = pFirst->texture.q2surface;
			face.texture.q2contents = pFirst->texture.q2contents;
			face.texture.q2value = pFirst->texture.q2value;
		}
		face.InitializeTextureAxes( Options.GetTextureAlignment(), INIT_TEXTURE_ALL | INIT_TEXTURE_FORCE );
		pClipSolid->AddFace( &face );
		pClipGroup->SetClipSolid( pClipSolid, side );
	}
	pClipper->m_ClipResults.AddTail( pClipGroup );
	return TRUE;
}

//=============================================================================
//
// CClipGroup
//

//-----------------------------------------------------------------------------
// Purpose: Destructor. Gets rid of the unnecessary clip solids.
//-----------------------------------------------------------------------------
CClipGroup::~CClipGroup()
{
	delete m_pClipSolids[0];
	delete m_pClipSolids[1];
}

//-----------------------------------------------------------------------------
// Purpose: constructor - initialize the clipper variables
//-----------------------------------------------------------------------------
Clipper3D::Clipper3D( void )
{
	m_Mode = FRONT;
	m_axClipHorz = -1;
	m_axClipVert = -1;
	ZeroVector( m_ClipStartPoints[0] );
	ZeroVector( m_ClipStartPoints[1] );

	ZeroVector( m_ClipPlane.normal );
	m_ClipPlane.dist = 0.0f;
	ZeroVector( m_ClipPoints[0] );
	ZeroVector( m_ClipPoints[1] );
	m_ClipPointHit = -1;

	m_pOrigObjects = NULL;

	m_bLButtonDown = false;

	m_bDrawMeasurements = false;
	SetEmpty();
}

//-----------------------------------------------------------------------------
// Purpose: deconstructor
//-----------------------------------------------------------------------------
Clipper3D::~Clipper3D( void )
{
	ResetClipResults();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the tool is activated.
// Input  : eOldTool - The ID of the previously active tool.
//-----------------------------------------------------------------------------
void Clipper3D::OnActivate( ToolID_t eOldTool )
{
	if ( IsActiveTool() )
	{
		//
		// Already the active tool - toggle the mode.
		//
		IterateClipMode();
		m_pDocument->ToolUpdateViews( CMapView2D::updTool );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the tool is deactivated.
// Input  : eNewTool - The ID of the tool that is being activated.
//-----------------------------------------------------------------------------
void Clipper3D::OnDeactivate( ToolID_t eNewTool )
{
	SetEmpty();
}

//-----------------------------------------------------------------------------
// Purpose: (virtual imp) It initializes a new clipping instance.  Called from
//          MapView2D.  Initiated with a left mouse button down call after the
//          clipping tool has been selected.
//   Input: pt3 - a 3D point in the 2D view where the mouse was located when
//                the left button was pushed
//-----------------------------------------------------------------------------
void Clipper3D::StartNew( const Vector& vecStart )
{
	m_axClipHorz = axHorz;
	m_axClipVert = axVert;
	//
	// set the initial clip points
	//
	CPoint pt( vecStart[axHorz], vecStart[axVert] );
	SetClipPoint( 0, pt );
	SetClipPoint( 1, pt );
	m_ClipPointHit = 0;
	BuildClipPlane();
	// start the tools translation functionality
	Tool3D::StartTranslation( pt );
}

//-----------------------------------------------------------------------------
// Purpose: (virtual imp) It handles the initial translation functionality.
//          It tests to see that the left mouse click hits a clipping plane
//          point.  The point is moved in the UpdateTranslation function.
// Input  : pt - current location of the mouse in the 2DView
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL Clipper3D::StartTranslation( CPoint& pt )
{
	//
	// test for clip point hit (result = {0, 1, -1})
	//
	m_ClipPointHit = HitTest( pt );
	if ( m_ClipPointHit == -1 )
		return FALSE;

	m_ClipPoints[0] = m_ClipStartPoints[0];
	m_ClipPoints[1] = m_ClipStartPoints[1];
	// pass along the info to parent class
	Tool3D::StartTranslation( pt );
	// translation started
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: (virtual imp) This function handles the "dragging" of the mouse
//          while the left mouse button is depressed.  It updates the position
//          of the clippoing plane point selected in the StartTranslation
//          function.  This function rebuilds the clipping plane and updates
//          the clipping solids when necessary.
//   Input: pt - current location of the mouse in the 2DView
//          uFlags - constrained clipping plane point movement
//          *dragSize - not used in the virtual implementation
//  Output: success of translation (TRUE/FALSE)
//-----------------------------------------------------------------------------
BOOL Clipper3D::UpdateTranslation( CPoint pt, UINT uFlags, CSize& dragSize )
{
	// sanity check
	if ( m_bEmpty )
		return FALSE;

	if ( !( uFlags & constrainNosnap ) )
		// snap point if need be
		m_pDocument->Snap( pt );

	//
	// update clipping point positions
	//
	if ( !( CompareClipPoint( m_ClipPointHit, pt ) ) )
	{
		if ( uFlags & constrainMoveBoth )
		{
			//
			// calculate the point and delta - to move both clip points simultaneously
			//
			Vector point;
			point[axHorz] = pt.x;
			point[axVert] = pt.y;
			point[axThird] = 0.0f;

			Vector delta;
			VectorSubtract( point, m_ClipPoints[m_ClipPointHit], delta );

			VectorAdd( m_ClipPoints[( m_ClipPointHit + 1 ) % 2], delta, m_ClipPoints[( m_ClipPointHit + 1 ) % 2] );
		}

		SetClipPoint( m_ClipPointHit, pt );

		// build the new clip plane and update clip results
		BuildClipPlane();
		GetClipResults();
		return TRUE;
	}

	// render the clip results based on the old clip plane
	GetClipResults();

	return FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: (virtual imp) This function defines all finishing functionality
//          necessary at the end of a clipping action.  Nothing really!!!
// Input  : bSave - passed along the the Tool finish translation call
//-----------------------------------------------------------------------------
void Clipper3D::FinishTranslation( BOOL bSave )
{
	if ( bSave )
	{
		m_ClipStartPoints[0] = m_ClipPoints[0];
		m_ClipStartPoints[1] = m_ClipPoints[1];
	}
	Tool3D::FinishTranslation( bSave );
}

//-----------------------------------------------------------------------------
// Purpose: iterate through the types of clipping modes, update after an
//          iteration takes place to visualize the new clip results
//-----------------------------------------------------------------------------
void Clipper3D::IterateClipMode( void )
{
	//
	// increment the clipping mode (wrap when necessary)
	//
	m_Mode++;

	if ( m_Mode > BOTH )
		m_Mode = FRONT;

	// update the clipped objects based on the mode
	GetClipResults();
}

//-----------------------------------------------------------------------------
// Purpose: This resets the solids to clip (the original list) and calls the
//          CalcClipResults function to generate new "clip" solids
//-----------------------------------------------------------------------------
void Clipper3D::GetClipResults( void )
{
	// reset the clip list to the original solid lsit
	SetClipObjects( m_pOrigObjects );

	// calculate the clipped objects based on the current "clip plane"
	CalcClipResults();
}

//-----------------------------------------------------------------------------
// Purpose: This function allows one to specifically set the clipping plane
//          information, as opposed to building a clip plane during "translation"
//   Input: pPlane - the plane information used to create the clip plane
//-----------------------------------------------------------------------------
void Clipper3D::SetClipPlane( PLANE* pPlane )
{
	//
	// copy the clipping plane info
	//
	m_ClipPlane.normal = pPlane->normal;
	m_ClipPlane.dist = pPlane->dist;
}

//-----------------------------------------------------------------------------
// Purpose: This function builds a clipping plane based on the clip point
//          locations manipulated in the "translation" functions and the 2DView
//-----------------------------------------------------------------------------
void Clipper3D::BuildClipPlane( void )
{
	if ( m_ClipPoints[0] == m_ClipPoints[1] )
	{
		memset( &m_ClipPlane, 0, sizeof( m_ClipPlane ) );
		return;
	}

	//
	// calculate the up vector
	//
	Vector upVect;
	if ( m_axClipHorz != AXIS_Z && m_axClipVert != AXIS_Z )
	{
		upVect[0] = 0.0f;
		upVect[1] = 0.0f;
		upVect[2] = 1.0f;
	}
	else if ( m_axClipHorz != AXIS_X && m_axClipVert != AXIS_X )
	{
		upVect[0] = 1.0f;
		upVect[1] = 0.0f;
		upVect[2] = 0.0f;
	}
	else
	{
		upVect[0] = 0.0f;
		upVect[1] = 1.0f;
		upVect[2] = 0.0f;
	}

	//
	// calculate the right vector
	//
	Vector rightVect;
	VectorSubtract( m_ClipPoints[1], m_ClipPoints[0], rightVect );

	//
	// calculate the forward (normal) vector
	//
	Vector forwardVect;
	CrossProduct( upVect, rightVect, forwardVect );
	VectorNormalize( forwardVect );

	//
	// save the clip plane info
	//
	m_ClipPlane.normal = forwardVect;
	m_ClipPlane.dist = DotProduct( m_ClipPoints[0], forwardVect );
	m_ClipPlane.planepts[0] = m_ClipPoints[0];
	m_ClipPlane.planepts[1] = m_ClipPoints[1];
	VectorMA( m_ClipPoints[1], 128.0f, upVect, m_ClipPlane.planepts[2] );
}

//-----------------------------------------------------------------------------
// Purpose: This functions sets up the list of objects to be clipped.
//          Initially the list is passed in (typically a Selection set).  On
//          subsequent "translation" updates the list is refreshed from the
//          m_pOrigObjects list.
//   Input: pList - the list of objects (solids) to be clipped
//-----------------------------------------------------------------------------
void Clipper3D::SetClipObjects( CMapObjectList* pList )
{
	// check for an empty list
	if ( !pList )
		return;

	// save the original list
	m_pOrigObjects = pList;

	// clear the clip results list
	ResetClipResults();

	//
	// copy solids into the clip list
	//
	POSITION pos = m_pOrigObjects->GetHeadPosition();
	while ( pos )
	{
		CMapClass* pObject = m_pOrigObjects->GetNext( pos );
		if ( !pObject )
			continue;

		if ( pObject->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) )
			AddToClipList( (CMapSolid*)pObject, this );

		pObject->EnumChildren( ENUMMAPCHILDRENPROC( AddToClipList ), DWORD( this ), MAPCLASS_TYPE( CMapSolid ) );
	}

	// the clipping list is not empty anymore
	m_bEmpty = FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: This function calculates based on the defined or given clipping
//          plane and clipping mode the new clip solids.
//-----------------------------------------------------------------------------
void Clipper3D::CalcClipResults( void )
{
	if ( IsEmpty() )
		return;

	bool	 bValidPlane = !VectorCompare( m_ClipPlane.normal, vec3_origin );
	//
	// iterate through and clip all of the solids in the clip list
	//
	POSITION pos = m_ClipResults.GetHeadPosition();
	while ( pos )
	{
		CClipGroup* pClipGroup = m_ClipResults.GetNext( pos );
		for ( int side = 0; side < 2; side++ )
		{
			CMapSolid* pSolid = pClipGroup->GetClipSolid( side );
			if ( !pSolid )
				continue;

			CMapFace* pCap = pSolid->GetFace( pSolid->GetFaceCount() - 1 );
			pCap->SetIgnored( !bValidPlane );
			if ( bValidPlane )
			{
				pCap->plane = m_ClipPlane;
				if ( m_Mode == BACK || side == CClipGroup::BACK )
				{
					VectorNegate( pCap->plane.normal );
					pCap->plane.dist = -pCap->plane.dist;
					Vector point = pCap->plane.planepts[0];
					pCap->plane.planepts[0] = pCap->plane.planepts[2];
					pCap->plane.planepts[2] = point;
				}
			}
			pSolid->CreateFromPlanes( CREATE_KEEP_UNUSED_FACES );
			pSolid->InitializeTextureAxes( Options.GetTextureAlignment(), INIT_TEXTURE_ALL );
			pSolid->CalcBounds();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: This function handles the removal of the "original" solid when it
//          has been clipped into new solid(s) or removed from the world (group
//          or entity) entirely.  It handles this in an undo safe fashion.
//   Input: pOrigSolid - the solid to remove
//-----------------------------------------------------------------------------
void Clipper3D::RemoveOrigSolid( CMapSolid* pOrigSolid )
{
	GetHistory()->KeepForDestruction( pOrigSolid );
	if ( m_pDocument->Selection_IsSelected( pOrigSolid ) )
		m_pDocument->Selection_Remove( pOrigSolid );
	else
		pOrigSolid->SetSelectionState( SELECT_NONE );
	m_pDocument->RemoveObjectFromWorld( pOrigSolid, true );
}

//-----------------------------------------------------------------------------
// Purpose: This function handles the saving of newly clipped solids (derived
//          from an "original" solid).  It handles them in an undo safe fashion.
//   Input: pSolid - the newly clipped solid
//          pOrigSolid - the "original" solid or solid the clipped solid was
//                       derived from
//-----------------------------------------------------------------------------
void Clipper3D::SaveClipSolid( CMapSolid* pSolid, CMapSolid* pOrigSolid )
{
	//
	// no longer a temporary solid
	//
	pSolid->SetTemporary( FALSE );

	//
	// Add the new solid to the original solid's parent (group, entity, world, etc.).
	//
	m_pDocument->AddObjectToWorld( pSolid, pOrigSolid->GetParent() );

	//
	// handle linking solid into selection -- via selection set when parent is the world
	// and selected, or set the selection state if parent is group or entity in selection set
	//
	if ( m_pDocument->Selection_IsSelected( pOrigSolid ) )
		m_pDocument->SelectObject( pSolid, CMapDoc::scSelect );
	else
		pSolid->SetSelectionState( SELECT_NORMAL );

	GetHistory()->KeepNew( pSolid );
}

//-----------------------------------------------------------------------------
// Purpose: This function saves all the clipped solid information.  If new solids
//          were generated from the original, they are saved and the original is
//          set for desctruciton.  Otherwise, the original solid is kept.
//-----------------------------------------------------------------------------
void Clipper3D::SaveClipResults( void )
{
	if ( IsEmpty() )
		return;

	UpdateBox ub;
	m_pDocument->Selection_GetBounds( ub.Box.bmins, ub.Box.bmaxs );
	ub.Objects = m_pDocument->Selection_GetList();
	// mark this place in the history
	GetHistory()->MarkUndoPosition( NULL, "Clip Objects" );

	//
	// save all new objects into the selection list
	//
	POSITION pos = m_ClipResults.GetHeadPosition();
	while ( pos )
	{
		CClipGroup* pClipGroup = m_ClipResults.GetNext( pos );
		if ( !pClipGroup || VectorCompare( m_ClipPlane.normal, vec3_origin ) )
			continue;

		CMapSolid* pOrigSolid = pClipGroup->GetOrigSolid();
		CMapSolid* pPrimary = pClipGroup->GetClipSolid( CClipGroup::FRONT );
		CMapSolid* pSecondary = m_Mode == BOTH ? pClipGroup->GetClipSolid( BACK ) : NULL;
		CMapSolid* pSolids[2] = { pPrimary, pSecondary };
		for ( int side = 0; side < 2; side++ )
		{
			CMapSolid* pSolid = pSolids[side];
			if ( !pSolid )
				continue;

			for ( int face = 0; face < pSolid->GetFaceCount(); face++ )
			{
				CMapFace* pFace = pSolid->GetFace( face );
				bool	  bDuplicate = false;
				for ( int prev = 0; prev < face; prev++ )
				{
					CMapFace* pPrev = pSolid->GetFace( prev );
					if ( DotProduct( pFace->plane.normal, pPrev->plane.normal ) > 0.999 &&
						 fabs( pFace->plane.dist - pPrev->plane.dist ) < 0.01 )
					{
						bDuplicate = true;
						break;
					}
				}
				if ( pFace->IsIgnored() || !pFace->Points || bDuplicate )
					pSolid->DeleteFace( face-- );
			}
		}

		if ( pPrimary && pPrimary->GetFaceCount() >= 4 )
		{
			CMapClass* pParent = pOrigSolid->GetParent();
			GetHistory()->Keep( pOrigSolid );
			pOrigSolid->CopyFrom( pPrimary, false );
			pOrigSolid->SetObjParent( pParent );
			pOrigSolid->SetTemporary( FALSE );
			pOrigSolid->PostUpdate( Notify_Changed );
		}

		if ( pSecondary && pSecondary->GetFaceCount() >= 4 )
		{
			SaveClipSolid( pSecondary, pOrigSolid );
			pClipGroup->SetClipSolid( NULL, BACK );
		}

		if ( !pPrimary || pPrimary->GetFaceCount() < 4 )
			// remove the original solid
			RemoveOrigSolid( pOrigSolid );
	}

	SetEmpty();
	// update the selection bounds -- given the new solids
	m_pDocument->Selection_UpdateBounds();
	//
	// update world and views
	//
	m_pDocument->UpdateAllViews( NULL, MAPVIEW_UPDATE_OBJECTS, &ub );
	m_pDocument->ToolUpdateViews( CMapView2D::updTool );
	m_pDocument->SetModifiedFlag();
}

//-----------------------------------------------------------------------------
// Purpose: Draws the measurements of a brush in the 2D view.
// Input  : pRender -
//			pSolid -
//			nFlags -
//-----------------------------------------------------------------------------
void Clipper3D::DrawBrushExtents( CRender2D* pRender, CMapSolid* pSolid, int nFlags )
{
	//
	// get the bounds of the solid
	//
	Vector Mins, Maxs;
	pSolid->GetRender2DBox( Mins, Maxs );

	//
	// Determine which side of the clipping plane this solid is on in screen
	// space. This tells us where to draw the extents.
	//
	if ( ( m_ClipPlane.normal[0] == 0 ) && ( m_ClipPlane.normal[1] == 0 ) && ( m_ClipPlane.normal[2] == 0 ) )
		return;

	Vector2D planeNormal;
	Vector	 normal = m_ClipPlane.normal;
	if ( m_Mode == BACK || ( nFlags & DBT_BACK ) )
		VectorNegate( normal );
	pRender->TransformPoint3D( planeNormal, normal );

	if ( planeNormal[0] < 0 )
	{
		nFlags &= ~DBT_LEFT;
		nFlags |= DBT_RIGHT;
	}
	else if ( planeNormal[0] > 0 )
	{
		nFlags &= ~DBT_RIGHT;
		nFlags |= DBT_LEFT;
	}

	if ( planeNormal[1] < 0 )
	{
		nFlags &= ~DBT_TOP;
		nFlags |= DBT_BOTTOM;
	}
	else if ( planeNormal[1] > 0 )
	{
		nFlags &= ~DBT_BOTTOM;
		nFlags |= DBT_TOP;
	}

	DrawBoundsText( pRender, Mins, Maxs, nFlags );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pRender -
//-----------------------------------------------------------------------------
void Clipper3D::RenderTool2D( CRender2D* pRender )
{
	// sanity check!
	if ( m_bEmpty )
		return;

	// check flag for rendering vertices
	bool bDrawVerts = (bool)( Options.view2d.bDrawVertices == TRUE );

	// setup the line to use
	pRender->SetLineType( CRender2D::LINE_SOLID, CRender2D::LINE_THICK, 255, 255, 255 );

	//
	// render the clipped solids
	//
	POSITION pos = m_ClipResults.GetHeadPosition();
	while ( pos )
	{
		CClipGroup* pClipGroup = m_ClipResults.GetNext( pos );
		CMapSolid*	pClipBack = pClipGroup->GetClipSolid( CClipGroup::BACK );
		CMapSolid*	pClipFront = pClipGroup->GetClipSolid( CClipGroup::FRONT );
		if ( !pClipBack && !pClipFront )
			continue;

		//
		// draw clip solids with the extents
		//
		if ( pClipBack )
		{
			int faceCount = pClipBack->GetFaceCount() - 1;
			for ( int i = 0; i < faceCount; i++ )
			{
				CMapFace* pFace = pClipBack->GetFace( i );
				if ( pFace->IsIgnored() || !pFace->Points )
					continue;

				pRender->DrawLineLoop( pFace->nPoints, pFace->Points, bDrawVerts, 3 );
			}
			if ( m_bDrawMeasurements )
				DrawBrushExtents( pRender, pClipBack, DBT_BOTTOM | DBT_RIGHT | DBT_BACK );
		}

		if ( pClipFront )
		{
			int faceCount = pClipFront->GetFaceCount() - 1;
			for ( int i = 0; i < faceCount; i++ )
			{
				CMapFace* pFace = pClipFront->GetFace( i );
				if ( pFace->IsIgnored() || !pFace->Points )
					continue;

				pRender->DrawLineLoop( pFace->nPoints, pFace->Points, bDrawVerts, 3 );
			}
			if ( m_bDrawMeasurements )
				DrawBrushExtents( pRender, pClipFront, DBT_TOP | DBT_LEFT );
		}
	}

	if ( pRender->GetAxisHorz() == m_axClipHorz && pRender->GetAxisVert() == m_axClipVert )
	{
		const Vector* points = IsTranslating() ? m_ClipPoints : m_ClipStartPoints;
		//
		// draw the clip-plane endpoints
		//
		pRender->SetFillColor( 255, 255, 255 );
		pRender->SetLineType( CRender2D::LINE_SOLID, CRender2D::LINE_THIN, 0, 0, 0 );
		for ( int i = 0; i < 2; i++ )
		{
			CPoint point;
			pRender->TransformPoint3D( point, points[i] );
			pRender->DrawEllipse( point, 4, 4, true );
		}
		pRender->SetLineType( CRender2D::LINE_SOLID, CRender2D::LINE_THICK, 0, 255, 255 );
		pRender->DrawLine( points[0], points[1] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Renders the brushes that will be left by the clipper in white
//			wireframe.
// Input  : pRender - Rendering interface.
//-----------------------------------------------------------------------------
void Clipper3D::RenderTool3D( CRender3D* pRender )
{
	// is there anything to render?
	if ( m_bEmpty )
		return;

	//
	// setup the renderer
	//
	RenderMode_t eDefaultRenderMode = pRender->GetDefaultRenderMode();
	pRender->SetDefaultRenderMode( RENDER_MODE_WIREFRAME );

	POSITION pos = m_ClipResults.GetHeadPosition();
	while ( pos )
	{
		CClipGroup* pClipGroup = m_ClipResults.GetNext( pos );

		CMapSolid* pFrontSolid = pClipGroup->GetClipSolid( CClipGroup::FRONT );
		if ( pFrontSolid )
		{
			color32 rgbColor = pFrontSolid->GetRenderColor();
			pFrontSolid->SetRenderColor( 255, 255, 255 );
			pFrontSolid->Render3D( pRender );
			pFrontSolid->SetRenderColor( rgbColor );
		}

		CMapSolid* pBackSolid = pClipGroup->GetClipSolid( CClipGroup::BACK );
		if ( pBackSolid )
		{
			color32 rgbColor = pBackSolid->GetRenderColor();
			pBackSolid->SetRenderColor( 255, 255, 255 );
			pBackSolid->Render3D( pRender );
			pBackSolid->SetRenderColor( rgbColor );
		}
	}

	pRender->SetDefaultRenderMode( eDefaultRenderMode );
}

//-----------------------------------------------------------------------------
// Purpose: (virtual imp)
// Input  : pt -
//			BOOL -
// Output : int
//-----------------------------------------------------------------------------
int Clipper3D::HitTest( CPoint pt, BOOL )
{
	if ( IsEmpty() || axHorz != m_axClipHorz || axVert != m_axClipVert )
		return -1;

	const int POINT_TOLERANCE = 4;
	CRect	  r;

	//
	// check point point 0
	//
	r.SetRect( m_ClipStartPoints[0][axHorz], m_ClipStartPoints[0][axVert], m_ClipStartPoints[0][axHorz],
			   m_ClipStartPoints[0][axVert] );
	RectToScreen( r );
	r.InflateRect( POINT_TOLERANCE, POINT_TOLERANCE );
	if ( r.PtInRect( pt ) )
		return 0;

	//
	// check clip point 1
	//
	r.SetRect( m_ClipStartPoints[1][axHorz], m_ClipStartPoints[1][axVert], m_ClipStartPoints[1][axHorz],
			   m_ClipStartPoints[1][axVert] );
	RectToScreen( r );
	r.InflateRect( POINT_TOLERANCE, POINT_TOLERANCE );
	if ( r.PtInRect( pt ) )
		return 1;

	// neither point hit
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: compare a given point to the clip point at the given index (this is
//          a 2D operation)
//   Input: index - index of the clip point to compare against
//          pt - the point to compare against the clip point
//  Output: true - if the points are the same, false otherwise
//-----------------------------------------------------------------------------
bool Clipper3D::CompareClipPoint( int index, CPoint pt )
{
	if ( ( m_ClipPoints[index][axHorz] == (float)pt.x ) && ( m_ClipPoints[index][axVert] == (float)pt.y ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: set the clip point at the given index, snap the point to the grid
//          when necessary
//   Input: index - the index of the clip point to set
//          pt - the values to set the clip point to
//-----------------------------------------------------------------------------
void Clipper3D::SetClipPoint( int index, CPoint pt )
{
	m_ClipPoints[index][axHorz] = pt.x;
	m_ClipPoints[index][axVert] = pt.y;
	m_ClipPoints[index][axThird] = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: (virtual imp)Determines whether there are objects currently being clipped.
// Output : Returns TRUE if there are no objects being clipped, FALSE if there are.
//-----------------------------------------------------------------------------
BOOL Clipper3D::IsEmpty()
{
	return m_bEmpty || m_ClipResults.IsEmpty();
}

//-----------------------------------------------------------------------------
// Purpose: (virtual imp) Sets the clipping list flag to empty
//-----------------------------------------------------------------------------
void Clipper3D::SetEmpty()
{
	ResetClipResults();
	m_pOrigObjects = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Reset (clear) the clip results.
//-----------------------------------------------------------------------------
void Clipper3D::ResetClipResults( void )
{
	//
	// delete the clip solids held in the list -- originals are just pointers
	// to pre-existing objects
	//
	while ( !m_ClipResults.IsEmpty() )
	{
		CClipGroup* pClipGroup = m_ClipResults.RemoveTail();

		if ( pClipGroup )
		{
			delete pClipGroup;
			pClipGroup = NULL;
		}
	}

	// the clipping list is empty
	m_bEmpty = TRUE;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : nChar -
//			nRepCnt -
//			nFlags -
//-----------------------------------------------------------------------------
bool Clipper3D::OnKeyDown2D( CMapView2D* pView, UINT nChar, UINT nRepCnt, UINT nFlags )
{
	switch ( nChar )
	{
	case 'O':
	{
		//
		// Toggle the rendering of measurements.
		//
		ToggleMeasurements();
		CMapDoc* pDoc = pView->GetDocument();
		pDoc->ToolUpdateViews( CMapView2D::updTool );
		return true;
	}

	case VK_RETURN:
	{
		//
		// Do the clip.
		//
		if ( !IsEmpty() )
			SaveClipResults();
		return true;
	}

	case VK_ESCAPE:
	{
		OnEscape();
		return true;
	}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Clipper3D::OnLMouseDown2D( CMapView2D* pView, UINT nFlags, CPoint point )
{
	CMapDoc*   pDoc = pView->GetDocument();
	CMapWorld* pWorld = pDoc->GetMapWorld();

	m_ptLDownClient = point;
	m_bLButtonDown = true;

	pView->SetCapture();

	//
	// Convert to some odd coordinate space that the base tools code uses.
	//
	CPoint ptScreen = point;
	ptScreen.x += pView->GetScrollPos( SB_HORZ );
	ptScreen.y += pView->GetScrollPos( SB_VERT );

	//
	// Convert point to world coords.
	//
	pView->ClientToWorld( point );

	Vector ptOrg( COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT );
	ptOrg[axHorz] = point.x;
	ptOrg[axVert] = point.y;

	// getvisiblepoint fills in any coord that's still set to COORD_NOTINIT:
	pDoc->GetBestVisiblePoint( ptOrg );

	// snap starting position to grid
	if ( !( GetAsyncKeyState( VK_MENU ) & 0x8000 ) )
		pDoc->Snap( ptOrg );

	BOOL bStarting = FALSE;

	// if the tool is not empty, and shift is not held down (to
	//  start a new camera), don't do anything.
	if ( !IsEmpty() )
	{
		if ( !StartTranslation( ptScreen ) )
		{
			if ( nFlags & MK_SHIFT )
			{
				SetEmpty();
				bStarting = TRUE;
			}
			else
			{
				goto _DoNothing;
			}
		}
	}
	else
	{
		bStarting = TRUE;
	}

	SetClipObjects( pDoc->Selection_GetList() );

	if ( bStarting )
	{
		StartNew( ptOrg );
		pView->SetUpdateFlag( CMapView2D::updTool );
	}

_DoNothing:

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Clipper3D::OnLMouseUp2D( CMapView2D* pView, UINT nFlags, CPoint point )
{
	ReleaseCapture();
	m_bLButtonDown = false;

	if ( IsTranslating() )
	{
		FinishTranslation( TRUE );
		m_pDocument->ToolUpdateViews( CMapView2D::updTool );
	}

	m_pDocument->UpdateStatusbar();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Clipper3D::OnMouseMove2D( CMapView2D* pView, UINT nFlags, CPoint point )
{
	CMapDoc* pDoc = pView->GetDocument();
	if ( !pDoc )
		return false;

	bool bCursorSet = false;
	BOOL bDisableSnap = ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) ? TRUE : FALSE;

	//
	// Make sure the point is visible.
	//
	if ( m_bLButtonDown )
		pView->ToolScrollToPoint( point );

	//
	// Convert to some odd coordinate space that the base tools code uses.
	//
	CPoint ptScreen = point;
	ptScreen.x += pView->GetScrollPos( SB_HORZ );
	ptScreen.y += pView->GetScrollPos( SB_VERT );

	//
	// Convert to world coords.
	//
	Vector vecWorld;
	pView->ClientToWorld( vecWorld, point );
	point.x = vecWorld[axHorz];
	point.y = vecWorld[axVert];

	//
	// Update status bar position display.
	//
	char szBuf[128];
	m_pDocument->Snap( vecWorld );
	sprintf( szBuf, " @%.0f, %.0f ", vecWorld[axHorz], vecWorld[axVert] );
	SetStatusText( SBI_COORDS, szBuf );

	if ( IsTranslating() )
	{
		// cursor is cross here
		bCursorSet = true;

		UINT uConstraints = 0;

		if ( bDisableSnap )
			uConstraints |= Tool3D::constrainNosnap;

		if ( nFlags & MK_CONTROL )
			uConstraints |= Clipper3D::constrainMoveBoth;

		if ( UpdateTranslation( point, uConstraints, CSize( 0, 0 ) ) )
		{
			pDoc->ToolUpdateViews( CMapView2D::updTool );
			pDoc->Update3DViews();
		}
	}
	else if ( !IsEmpty() )
	{
		//
		// If the cursor is on a handle, set it to a cross.
		//
		if ( HitTest( ptScreen, TRUE ) != -1 )
		{
			SetCursor( AfxGetApp()->LoadStandardCursor( IDC_CROSS ) );
			bCursorSet = true;
		}
	}

	if ( !bCursorSet )
		SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles character events.
// Input  : Per CWnd::OnKeyDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Clipper3D::OnKeyDown3D( CMapView3D* pView, UINT nChar, UINT nRepCnt, UINT nFlags )
{
	return false;
}

bool Clipper3D::OnChar3D( CMapView3D* pView, UINT nChar, UINT nRepCnt, UINT nFlags )
{
	if ( nChar != VK_RETURN )
		return false;

	if ( !IsEmpty() )
		SaveClipResults();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void Clipper3D::OnEscape( void )
{
	if ( IsTranslating() )
	{
		FinishTranslation( FALSE );
		m_bLButtonDown = false;
		ReleaseCapture();
		m_pDocument->UpdateAllViews( NULL, MAPVIEW_UPDATE_DISPLAY );
	}
}
