//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Implements the entity/prefab placement tool.
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "GameData.h"
#include "History.h"
#include "MainFrm.h"
#include "MapDefs.h"
#include "MapSolid.h"
#include "MapDoc.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "Render2D.h"
#include "Render3D.h"
#include "StatusBarIDs.h"
#include "TextureSystem.h"
#include "ToolEntity.h"
#include "ToolManager.h"
#include "Worldcraft.h"

//#pragma warning(disable:4244)
static HCURSOR s_hcurEntity = NULL;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Marker3D::Marker3D( void )
{
	SetEmpty();

	m_vecPos = vec3_origin;
	m_vecTranslatePos = vec3_origin;

	m_bLButtonDown = false;

	if ( s_hcurEntity == NULL )
		s_hcurEntity = LoadCursor( AfxGetInstanceHandle(), MAKEINTRESOURCE( IDC_ENTITY ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Marker3D::~Marker3D( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL Marker3D::IsEmpty( void )
{
	return m_bEmpty;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Marker3D::SetEmpty( void )
{
	m_bEmpty = true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pt -
//			BOOL -
// Output :
//-----------------------------------------------------------------------------
int Marker3D::HitTest( CPoint pt, BOOL )
{
	CRect r( m_vecPos[axHorz], m_vecPos[axVert], m_vecPos[axHorz], m_vecPos[axVert] );
	RectToScreen( r );
	r.InflateRect( 8, 8 );

	return r.PtInRect( pt ) ? TRUE : -1;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pt -
// Output : Returns TRUE.
//-----------------------------------------------------------------------------
BOOL Marker3D::StartTranslation( CPoint& pt )
{
	m_vecTranslatePos = m_vecPos;
	Tool3D::StartTranslation( pt );
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : bSave -
//-----------------------------------------------------------------------------
void Marker3D::FinishTranslation( BOOL bSave )
{
	if ( bSave )
	{
		m_vecPos = m_vecTranslatePos;
		m_bEmpty = false;
	}

	Tool3D::FinishTranslation( bSave );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pt -
//			uFlags -
//			size -
// Output : Returns true if the translation delta was nonzero.
//-----------------------------------------------------------------------------
BOOL Marker3D::UpdateTranslation( CPoint pt, UINT uFlags, CSize& size )
{
	if ( !( uFlags & constrainNosnap ) )
		m_pDocument->Snap( pt );

	Vector vecCompare = m_vecTranslatePos;

	m_vecTranslatePos[axHorz] = pt.x;
	m_vecTranslatePos[axVert] = pt.y;

	return !CompareVectors2D( m_vecTranslatePos, vecCompare );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pt3 -
//-----------------------------------------------------------------------------
void Marker3D::StartNew( const Vector& vecStart )
{
	m_vecTranslatePos = vecStart;

	m_bEmpty = false;

	CPoint pt( vecStart[axHorz], vecStart[axVert] );
	Tool3D::StartTranslation( pt );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pRender -
//-----------------------------------------------------------------------------
void Marker3D::RenderTool2D( CRender2D* pRender )
{
	if ( !IsActiveTool() )
		return;

	Vector vecPos;
	CRect  rect;
	CPoint pt;

	if ( IsTranslating() )
	{
		vecPos = m_vecTranslatePos;
	}
	else
	{
		if ( IsEmpty() )
			return;

		vecPos = m_vecPos;
	}

	pt.x = vecPos[axHorz];
	pt.y = vecPos[axVert];
	PointToScreen( pt );

	pRender->SetLineType( CRender2D::LINE_SOLID, CRender2D::LINE_THIN, 35, 255, 75 );

	//
	// Draw center rect.
	//
	rect.SetRect( pt, pt );
	rect.InflateRect( 6, 6 );
	pRender->Rectangle( rect, false );

	//
	// Draw crosshairs.
	//
	pRender->MoveTo( g_MIN_MAP_COORD, pt.y );
	pRender->LineTo( g_MAX_MAP_COORD, pt.y );
	pRender->MoveTo( pt.x, g_MIN_MAP_COORD );
	pRender->LineTo( pt.x, g_MAX_MAP_COORD );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pView -
//			point -
// Output :
//-----------------------------------------------------------------------------
bool Marker3D::OnContextMenu2D( CMapView2D* pView, CPoint point )
{
	if ( !IsEmpty() )
	{
		CMapDoc* pDoc = pView->GetDocument();
		if ( pDoc == NULL )
			return true;

		// FIXME: functionalize this or fix it!
		pView->ScreenToClient( &point );

		CRect rect;
		pView->GetClientRect( &rect );
		if ( !rect.PtInRect( point ) )
			return true;

		CPoint ptScreen( point );
		CPoint ptMapScreen( point );
		pView->ClientToScreen( &ptScreen );
		pView->ClientToWorld( point );

		ptMapScreen.x += pView->GetScrollPos( SB_HORZ );
		ptMapScreen.y += pView->GetScrollPos( SB_VERT );

		if ( HitTest( ptMapScreen, FALSE ) != -1 )
		{
			static CMenu menu, menuCreate;
			static bool	 bInit = false;

			if ( !bInit )
			{
				bInit = true;
				menu.LoadMenu( IDR_POPUPS );
				menuCreate.Attach( ::GetSubMenu( menu.m_hMenu, 1 ) );
			}

			menuCreate.TrackPopupMenu( TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN, ptScreen.x, ptScreen.y,
									   pView );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pView -
//			nChar -
//			nRepCnt -
//			nFlags -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool Marker3D::OnKeyDown2D( CMapView2D* pView, UINT nChar, UINT nRepCnt, UINT nFlags )
{
	switch ( nChar )
	{
	case VK_RETURN:
	{
		if ( !IsEmpty() )
			CreateMapObject( pView );
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
// Purpose:
// Input  : pView -
//			nFlags -
//			point -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool Marker3D::OnLMouseDown2D( CMapView2D* pView, UINT nFlags, CPoint point )
{
	CMapDoc* pDoc = pView->GetDocument();
	if ( pDoc == NULL )
		return true;

	CMapWorld* pWorld = pDoc->GetMapWorld();

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
	pDoc->GetBestVisiblePoint( ptOrg );

	//
	// Snap starting position to grid.
	//
	if ( !( GetAsyncKeyState( VK_MENU ) & 0x8000 ) )
		pDoc->Snap( ptOrg );

	if ( IsEmpty() )
		StartNew( ptOrg );
	else
		StartTranslation( ptScreen );

	pView->SetUpdateFlag( CMapView2D::updTool );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : Pre CWnd::OnLButtonUp.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool Marker3D::OnLMouseUp2D( CMapView2D* pView, UINT nFlags, CPoint point )
{
	ReleaseCapture();

	if ( IsTranslating() )
	{
		m_bLButtonDown = false;
		FinishTranslation( TRUE );
		m_pDocument->ToolUpdateViews( CMapView2D::updTool );
	}

	m_pDocument->UpdateStatusbar();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pView -
//			nFlags -
//			point -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool Marker3D::OnMouseMove2D( CMapView2D* pView, UINT nFlags, CPoint point )
{
	CMapDoc* pDoc = pView->GetDocument();
	if ( !pDoc )
		return false;

	bool		 bCursorSet = false;
	unsigned int uConstraints = 0;
	if ( ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) )
		uConstraints |= Tool3D::constrainNosnap;

	//
	// Make sure the point is visible.
	//
	if ( m_bLButtonDown )
		pView->ToolScrollToPoint( point );

	//
	// Convert to some odd coordinate space that the base tools code uses.
	//
	CPoint ptHitTest = point;
	ptHitTest.x += pView->GetScrollPos( SB_HORZ );
	ptHitTest.y += pView->GetScrollPos( SB_VERT );

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

	//
	// If we are currently dragging the marker, update that operation based on
	// the current cursor position and keyboard state.
	//
	if ( IsTranslating() )
	{
		if ( UpdateTranslation( point, uConstraints, CSize( 0, 0 ) ) )
		{
			pDoc->ToolUpdateViews( CMapView2D::updTool );
			pDoc->Update3DViews();
		}

		// Don't change the cursor while dragging - it should remain a cross.
		bCursorSet = true;
		SetCursor( AfxGetApp()->LoadStandardCursor( IDC_CROSS ) );
	}
	else if ( !IsEmpty() )
	{
		// Don't change the cursor while dragging - it should remain a cross.
		bCursorSet = true;
		SetCursor( AfxGetApp()->LoadStandardCursor( IDC_CROSS ) );
	}

	if ( !bCursorSet )
		SetCursor( s_hcurEntity );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void Marker3D::OnEscape( void )
{
	//
	// Cancel the object creation tool.
	//
	if ( IsTranslating() )
	{
		FinishTranslation( FALSE );
		m_pDocument->ToolUpdateViews( CMapView2D::updTool );
	}
	else if ( !IsEmpty() )
	{
		SetEmpty();
		m_pDocument->ToolUpdateViews( CMapView2D::updEraseTool );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pView -
//			nFlags -
//			point -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool Marker3D::OnLMouseDown3D( CMapView3D* pView, UINT nFlags, CPoint point )
{
	if ( nFlags & MK_SHIFT )
		return false;

	ULONG	   face;
	CMapClass* pObject = pView->NearestObjectAt( point, face );
	CMapSolid* pSolid = dynamic_cast<CMapSolid*>( pObject );
	if ( !pSolid )
		return true;

	Vector start, end;
	pView->BuildRay( point, start, end );
	Vector hitPos, hitNormal;
	if ( !pSolid->GetFace( face )->TraceLine( hitPos, hitNormal, start, end ) )
		return true;

	CMapDoc* pDoc = pView->GetDocument();
	//
	// Entity creation.
	//
	GetHistory()->MarkUndoPosition( pDoc->Selection_GetList(), "New Entity" );

	CMapEntity* pEntity = new CMapEntity;
	pEntity->SetPlaceholder( TRUE );
	pEntity->SetOrigin( hitPos );
	pEntity->SetClass( CObjectBar::GetDefaultEntityClass() );
	pDoc->AddObjectToWorld( pEntity );
	GetHistory()->KeepNew( pEntity );
	//
	// Select the new object.
	//
	pDoc->SelectObject( pEntity, CMapDoc::scClear | CMapDoc::scSelect | CMapDoc::scUpdateDisplay );

	UpdateBox	   update;
	CMapObjectList objects;
	objects.AddTail( pEntity );
	update.Objects = &objects;
	Vector mins, maxs;
	pEntity->GetRender2DBox( mins, maxs );
	update.Box.SetBounds( mins, maxs );
	pDoc->UpdateAllViews( NULL, MAPVIEW_UPDATE_OBJECTS, &update );
	pDoc->SetModifiedFlag();
	return true;
}

void Marker3D::CreateMapObject( CMapView2D* pView )
{
	GetHistory()->MarkUndoPosition( m_pDocument->Selection_GetList(), "New Entity" );
	CMapEntity* pEntity = new CMapEntity;
	pEntity->SetPlaceholder( TRUE );
	pEntity->SetOrigin( m_vecPos );
	pEntity->SetClass( CObjectBar::GetDefaultEntityClass() );
	m_pDocument->AddObjectToWorld( pEntity );
	GetHistory()->KeepNew( pEntity );
	m_pDocument->ToolUpdateViews( CMapView2D::updEraseTool );
	//
	// Select the new object.
	//
	m_pDocument->SelectObject( pEntity, CMapDoc::scClear | CMapDoc::scSelect | CMapDoc::scUpdateDisplay );

	UpdateBox	   update;
	CMapObjectList objects;
	objects.AddTail( pEntity );
	update.Objects = &objects;
	Vector mins, maxs;
	pEntity->GetRender2DBox( mins, maxs );
	update.Box.SetBounds( mins, maxs );
	m_pDocument->UpdateAllViews( NULL, MAPVIEW_UPDATE_OBJECTS, &update );
	SetEmpty();
	m_pDocument->SetModifiedFlag();
}
