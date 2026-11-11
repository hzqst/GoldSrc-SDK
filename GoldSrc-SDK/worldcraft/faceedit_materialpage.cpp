//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdafx.h>
#include "Worldcraft.h"
#include "IEditorTexture.h"
#include "FaceEditSheet.h"
#include "MapFace.h"
#include "MapWorld.h"
#include "MainFrm.h"
#include "History.h"
#include "GlobalFunctions.h"
#include "MapSolid.h"
#include "TextureBrowser.h"
#include "TextureSystem.h"
#include "MapView3D.h"
#include "ReplaceTexDlg.h"
#include "FaceEdit_MaterialPage.h"
#include "Camera.h"
#include "MapDoc.h"
#include "ToolManager.h"

//=============================================================================

IMPLEMENT_DYNAMIC( CFaceEditMaterialPage, CDialogBar )

BEGIN_MESSAGE_MAP( CFaceEditMaterialPage, CTextureBar )
//{{AFX_MSG_MAP( CFaceEditMaterialPage )
ON_BN_CLICKED( ID_FACEEDIT_APPLY, OnButtonApply )
ON_COMMAND_EX( IDC_ALIGN_WORLD, OnAlign )
ON_COMMAND_EX( IDC_ALIGN_FACE, OnAlign )
ON_BN_CLICKED( IDC_HIDEMASK, OnHideMask )
ON_COMMAND_EX( IDC_JUSTIFY_TOP, OnJustify )
ON_COMMAND_EX( IDC_JUSTIFY_BOTTOM, OnJustify )
ON_COMMAND_EX( IDC_JUSTIFY_LEFT, OnJustify )
ON_COMMAND_EX( IDC_JUSTIFY_CENTER, OnJustify )
ON_COMMAND_EX( IDC_JUSTIFY_RIGHT, OnJustify )
ON_COMMAND_EX( IDC_JUSTIFY_FITTOFACE, OnJustify )
ON_BN_CLICKED( IDC_MODE, OnMode )
ON_WM_VSCROLL()
ON_NOTIFY( UDN_DELTAPOS, IDC_SPINSCALEX, OnDeltaPosFloatSpin )
ON_NOTIFY( UDN_DELTAPOS, IDC_SPINSCALEY, OnDeltaPosFloatSpin )
ON_WM_SIZE()
ON_CBN_SELCHANGE( IDC_TEXTURES, OnSelChangeTexture )

ON_COMMAND( IDC_TREAT_AS_ONE, OnTreatAsOne )
ON_BN_CLICKED( IDC_REPLACE, OnReplace )
ON_COMMAND_EX_RANGE( CFaceEditSheet::id_SwitchModeStart, CFaceEditSheet::id_SwitchModeEnd, OnSwitchMode )
ON_CBN_SELCHANGE( IDC_TEXTUREGROUPS, OnChangeTextureGroup )
ON_BN_CLICKED( IDC_BROWSE, OnBrowse )
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

const int NOT_INIT = -99999;

//-----------------------------------------------------------------------------
CFaceEditMaterialPage::CFaceEditMaterialPage()
{
	m_bHideMask = FALSE;
	m_bInitialized = FALSE;
	m_bIgnoreResize = FALSE;
	m_bTreatAsOneFace = FALSE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CFaceEditMaterialPage::~CFaceEditMaterialPage()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL CFaceEditMaterialPage::PreTranslateMessage( MSG* pMsg )
{
	HACCEL hAccel = GetMainWnd()->GetAccelTable();
	if ( !( hAccel && ::TranslateAccelerator( GetMainWnd()->m_hWnd, hAccel, pMsg ) ) )
		return CTextureBar::PreTranslateMessage( pMsg );
	else
		return TRUE;
}

void CFaceEditMaterialPage::OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHandler )
{
	UpdateDialogControls( pTarget, FALSE );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::Init( void )
{
	//
	// Connect dialog control objects to their control IDs.
	//
	m_shiftX.SubclassDlgItem( IDC_SHIFTX, this );
	m_shiftY.SubclassDlgItem( IDC_SHIFTY, this );
	m_scaleX.SubclassDlgItem( IDC_SCALEX, this );
	m_scaleY.SubclassDlgItem( IDC_SCALEY, this );
	m_rotate.SubclassDlgItem( IDC_ROTATE, this );
	m_cHideMask.SubclassDlgItem( IDC_HIDEMASK, this );
	m_cExpand.SubclassDlgItem( IDC_EXPAND, this );

	//
	// Set spin ranges.
	//
	CWnd* pWnd = GetDlgItem( IDC_SPINSHIFTX );
	::PostMessage( pWnd->m_hWnd, UDM_SETRANGE, 0, MAKELONG( 1024, -1024 ) );

	pWnd = GetDlgItem( IDC_SPINSHIFTY );
	::PostMessage( pWnd->m_hWnd, UDM_SETRANGE, 0, MAKELONG( 1024, -1024 ) );

	pWnd = GetDlgItem( IDC_SPINROTATE );
	::PostMessage( pWnd->m_hWnd, UDM_SETRANGE, 0, MAKELONG( 359, -359 ) );

	pWnd = GetDlgItem( IDC_SPINSCALEX );
	::PostMessage( pWnd->m_hWnd, UDM_SETRANGE, 0, MAKELONG( UD_MAXVAL, UD_MINVAL ) );

	pWnd = GetDlgItem( IDC_SPINSCALEY );
	::PostMessage( pWnd->m_hWnd, UDM_SETRANGE, 0, MAKELONG( UD_MAXVAL, UD_MINVAL ) );

	// set the initial switch mode
	OnSwitchMode( CFaceEditSheet::ModeLiftSelect );

	//
	// initially update the texture controls
	//
	NotifyGraphicsChanged();
	UpdateTexture();

	// initialized!
	m_bInitialized = TRUE;
}

//-----------------------------------------------------------------------------
// NOTE: clean this up and make a global function!!!
// Purpose:
// Input  : fValue -
//			*pSpin -
//			bMantissa -
// Output : static void
//-----------------------------------------------------------------------------
void FloatToSpin( float fValue, CSpinButtonCtrl* pSpin, BOOL bMantissa )
{
	char szNew[128];

	CWnd* pEdit = pSpin->GetBuddy();

	if ( fValue == NOT_INIT )
		szNew[0] = 0;
	else if ( bMantissa )
		sprintf( szNew, "%.2f", fValue );
	else
		sprintf( szNew, "%.0f", fValue );

	pSpin->SetPos( atoi( szNew ) );

	char szCurrent[128];
	pEdit->GetWindowText( szCurrent, 128 );
	if ( strcmp( szNew, szCurrent ) )
		pEdit->SetWindowText( szNew );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : nValue -
//			pSpin -
// Output : static void
//-----------------------------------------------------------------------------
void IntegerToSpin( int nValue, CSpinButtonCtrl* pSpin )
{
	char szNew[128];

	CWnd* pEdit = pSpin->GetBuddy();

	if ( nValue == NOT_INIT )
		szNew[0] = 0;
	else
		sprintf( szNew, "%d", abs( nValue ) );

	pSpin->SetPos( atoi( szNew ) );

	char szCurrent[128];
	pEdit->GetWindowText( szCurrent, 128 );
	if ( strcmp( szNew, szCurrent ) != 0 )
		pEdit->SetWindowText( szNew );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : fValue -
//			*pWnd -
// Output : static void
//-----------------------------------------------------------------------------
void FloatToWnd( float fValue, CWnd* pWnd )
{
	char szCurrent[128];
	char szNew[128];

	if ( fValue == NOT_INIT )
		szNew[0] = 0;
	else
		sprintf( szNew, "%.2f", fValue );

	pWnd->GetWindowText( szCurrent, 128 );
	if ( strcmp( szNew, szCurrent ) )
		pWnd->SetWindowText( szNew );
}

//-----------------------------------------------------------------------------
// Purpose: Fetches a string value from a window and places it in a float. The
//			float is only modified if there is text in the window.
// Input  : pWnd - Window from which to get text.
//			fValue - Float in which to place value.
//-----------------------------------------------------------------------------
void TransferToFloat( CWnd* pWnd, float& fValue )
{
	CString str;
	pWnd->GetWindowText( str );

	if ( !str.IsEmpty() )
		fValue = (float)atof( str );
}

//-----------------------------------------------------------------------------
// Purpose: Fetches a string value from a window and places it in an integer. The
//			integer is only modified if there is text in the window.
// Input  : pWnd - Window from which to get text.
//			nValue - Float in which to place value.
//-----------------------------------------------------------------------------
void TransferToInteger( CWnd* pWnd, int& nValue )
{
	CString str;
	pWnd->GetWindowText( str );

	if ( !str.IsEmpty() )
		nValue = abs( atoi( str ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::ClickFace( CMapSolid* pSolid, int faceIndex, int cmd, int clickMode )
{
	// get the face
	CMapFace* pFace = pSolid->GetFace( faceIndex );

	//
	// are updates enalbed?
	//
	CFaceEditSheet* pSheet = (CFaceEditSheet*)this;
	bool			bEnableUpdate = pSheet->HasUpdateEnabled();

	//
	// find the behaviour of the page based on the "click mode"
	//
	switch ( clickMode )
	{
	case CFaceEditSheet::ModeAlignToView:
	{
		AlignToView( pFace );
		break;
	}

	case CFaceEditSheet::ModeLift:
	{
		if ( bEnableUpdate )
			UpdateDialogData( pFace );
		break;
	}

	case CFaceEditSheet::ModeLiftSelect:
	{
		if ( bEnableUpdate )
			UpdateDialogData();
		break;
	}

	case CFaceEditSheet::ModeApply:
	case CFaceEditSheet::ModeApplyAll:
	{
		Apply( pFace, clickMode == CFaceEditSheet::ModeApplyAll, !!( cmd & CFaceEditSheet::cfEdgeAlign ) );
		break;
	}
	}
}

//-----------------------------------------------------------------------------
// Maps the texture onto the face using the 3D view's up and right vectors.
// This can be useful for mapping curvy things like hills because if you don't
// move the 3D view, the texture will line up on any polies you map this way.
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::AlignToView( CMapFace* pFace )
{
	CView*		pActiveView;
	CMapView3D* pView3D;
	CFrameWnd*	pFrame;
	Vector		vView;

	if ( pFrame = GetMainWnd()->GetActiveFrame() )
	{
		if ( pActiveView = pFrame->GetActiveView() )
		{
			if ( pActiveView->IsKindOf( RUNTIME_CLASS( CMapView3D ) ) )
			{
				pView3D = dynamic_cast<CMapView3D*>( pActiveView );
				if ( pView3D )
				{
					const CCamera* pCamera = pView3D->GetCamera();
					if ( pCamera )
					{
						Vector right, up;
						pCamera->GetViewRight( right );
						pCamera->GetViewUp( up );
						pCamera->GetViewPoint( vView );

						pFace->texture.UAxis.AsVector3D() = right;
						pFace->texture.VAxis.AsVector3D() = up;
						pFace->texture.UAxis[3] = DotProduct( right, vView );
						pFace->texture.VAxis[3] = DotProduct( up, vView );

						pFace->texture.rotate = 0.0f;
						pFace->texture.scale[0] = 1.0f;
						pFace->texture.scale[1] = 1.0f;

						pFace->CalcTextureCoords();
					}
				}
			}
		}
	}
}

static Vector EdgeAlignCross( const Vector& a, const Vector& b )
{
	return Vector( (float)( (double)a[1] * b[2] - (double)a[2] * b[1] ),
				   (float)( (double)a[2] * b[0] - (double)a[0] * b[2] ),
				   (float)( (double)a[0] * b[1] - (double)a[1] * b[0] ) );
}

static double EdgeAlignDot( const Vector& a, const Vector& b )
{
	return ( (double)a[2] * b[2] + (double)a[1] * b[1] ) + (double)a[0] * b[0];
}

static void EdgeAlignNormalize( Vector& v )
{
	double flLength = sqrt( ( (double)v[0] * v[0] + (double)v[1] * v[1] ) + (double)v[2] * v[2] );
	if ( flLength == 0 )
	{
		v.Init( 0, 0, 1 );
		return;
	}

	double flScale = 1.0 / flLength;
	v.Init( (float)( v[0] * flScale ), (float)( v[1] * flScale ), (float)( v[2] * flScale ) );
}

static Vector EdgeAlignReject( const Vector& v, const Vector& vAxis )
{
	Vector vAlong = vAxis * (float)EdgeAlignDot( vAxis, v );
	return Vector( (float)( (double)v[0] - vAlong[0] ), (float)( (double)v[1] - vAlong[1] ),
				   (float)( (double)v[2] - vAlong[2] ) );
}

static void EdgeAlignRotation( float Matrix[3][3], const Vector& Axis, float fDegrees )
{
	double flRadians = fDegrees * 0.017453292519943295;
	double flSin = FpuSin( flRadians );
	float flCos = (float)FpuCos( flRadians );
	double flOneMinusCos = 1.0 - flCos;
	float t = (float)flOneMinusCos;

	float tx = (float)( flOneMinusCos * Axis[0] );
	float ty = (float)( (double)t * Axis[1] );
	float sx = (float)( flSin * Axis[0] );
	float sy = (float)( flSin * Axis[1] );
	float sz = (float)( flSin * Axis[2] );

	double txy = (double)tx * Axis[1];
	double txz = (double)tx * Axis[2];
	double tyz = (double)ty * Axis[2];
	float ftxy = (float)txy;
	float ftxz = (float)txz;
	float ftyz = (float)tyz;

	Matrix[0][0] = (float)( (double)tx * Axis[0] + flCos );
	Matrix[0][1] = (float)( (double)ftxy - sz );
	Matrix[0][2] = (float)( (double)ftxz + sy );
	Matrix[1][0] = (float)( txy + sz );
	Matrix[1][1] = (float)( (double)ty * Axis[1] + flCos );
	Matrix[1][2] = (float)( (double)ftyz - sx );
	Matrix[2][0] = (float)( txz - sy );
	Matrix[2][1] = (float)( tyz + sx );
	Matrix[2][2] = (float)( (double)t * Axis[2] * Axis[2] + flCos );
}

static Vector EdgeAlignRotate( const float Matrix[3][3], const Vector& v )
{
	return Vector(
		(float)( ( (double)Matrix[0][2] * v[2] + (double)Matrix[0][1] * v[1] ) + (double)v[0] * Matrix[0][0] ),
		(float)( ( (double)Matrix[1][2] * v[2] + (double)Matrix[1][0] * v[0] ) + (double)Matrix[1][1] * v[1] ),
		(float)( ( (double)Matrix[2][2] * v[2] + (double)Matrix[2][0] * v[0] ) + (double)Matrix[2][1] * v[1] ) );
}

static Vector EdgeAlignRotateAbout( const float Matrix[3][3], const Vector& vPoint, const Vector& vCenter )
{
	float flMove[3];
	for ( int i = 0; i < 3; i++ )
	{
		flMove[i] = (float)( ( ( ( 0.0 - vCenter[i] ) + (double)vCenter[2] * Matrix[i][2] ) +
							   (double)Matrix[i][0] * vCenter[0] ) +
							 (double)vCenter[1] * Matrix[i][1] );
	}

	return Vector(
		(float)( ( ( (double)Matrix[0][2] * vPoint[2] + (double)Matrix[0][1] * vPoint[1] ) +
				   (double)Matrix[0][0] * vPoint[0] ) +
				 flMove[0] ),
		(float)( ( ( (double)Matrix[1][0] * vPoint[0] + (double)Matrix[1][2] * vPoint[2] ) +
				   (double)Matrix[1][1] * vPoint[1] ) +
				 flMove[1] ),
		(float)( ( ( (double)Matrix[2][2] * vPoint[2] + (double)Matrix[2][1] * vPoint[1] ) +
				   (double)Matrix[2][0] * vPoint[0] ) +
				 flMove[2] ) );
}

//-----------------------------------------------------------------------------
// Copies the texture coordinate system from pFrom into pTo. Then it rotates
// the texture around the edge until it's as close to pTo's normal as possible.
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::CopyTCoordSystem( const CMapFace* pFrom, CMapFace* pTo )
{
	Vector	  axis[2], vEdge, vEdgePt, vOrigin;
	Vector	  vTextureNormal;
	float	  mEdgeRotation[3][3];
	float	  fAngle, fDot;
	bool	  bRotate;
	float	  fShift[2];
	Vector	  vProjTexNormal;
	Vector	  vProjPolyNormal;

	// The edge vector lies on both planes.
	vEdge = EdgeAlignCross( pFrom->plane.normal, pTo->plane.normal );
	EdgeAlignNormalize( vEdge );

	float planes[3][4];
	int	  rows[] = { 0, 1, 2 };
	for ( int i = 0; i < 3; i++ )
	{
		planes[0][i] = pFrom->plane.normal[i];
		planes[1][i] = pTo->plane.normal[i];
		planes[2][i] = vEdge[i];
	}
	planes[0][3] = pFrom->plane.dist;
	planes[1][3] = pTo->plane.dist;
	planes[2][3] = 0;
	bRotate = true;
	for ( int column = 0; column < 3; column++ )
	{
		int	  pivot = -1;
		float largest = 0.001f;
		for ( int row = column; row < 3; row++ )
		{
			float value = fabsf( planes[rows[row]][column] );
			if ( value > largest )
			{
				largest = value;
				pivot = row;
			}
		}
		if ( pivot == -1 )
		{
			bRotate = false;
			break;
		}
		int selected = rows[pivot];
		rows[pivot] = rows[column];
		rows[column] = selected;
		double inverse = 1.0 / planes[selected][column];
		for ( int i = 0; i < 4; i++ )
			planes[selected][i] = (float)( planes[selected][i] * inverse );
		planes[selected][column] = 1.0f;
		for ( int row = 0; row < 3; row++ )
		{
			if ( row == column )
				continue;
			float factor = -planes[rows[row]][column];
			for ( int i = 0; i < 4; i++ )
				planes[rows[row]][i] = (float)( planes[rows[row]][i] + (double)factor * planes[selected][i] );
			planes[rows[row]][column] = 0;
		}
	}
	if ( bRotate )
		for ( int i = 0; i < 3; i++ )
			vEdgePt[i] = planes[rows[i]][3];

	// Copy the texture coordinate system.
	axis[0] = pFrom->texture.UAxis.AsVector3D();
	axis[1] = pFrom->texture.VAxis.AsVector3D();
	fShift[0] = pFrom->texture.UAxis[3];
	fShift[1] = pFrom->texture.VAxis[3];

	Vector vUOrigin = axis[0] * fShift[0] * pFrom->texture.scale[0];
	Vector vVOrigin = axis[1] * fShift[1] * pFrom->texture.scale[1];
	vOrigin.Init( (float)( (double)vUOrigin[0] + vVOrigin[0] ), (float)( (double)vUOrigin[1] + vVOrigin[1] ),
				  (float)( (double)vUOrigin[2] + vVOrigin[2] ) );

	vTextureNormal = EdgeAlignCross( axis[0], axis[1] );
	EdgeAlignNormalize( vTextureNormal );

	if ( bRotate )
	{
		// Project texture normal and poly normal into the plane of rotation
		// to get the angle between them.
		vProjTexNormal = EdgeAlignReject( vTextureNormal, vEdge );
		vProjPolyNormal = EdgeAlignReject( pTo->plane.normal, vEdge );
		EdgeAlignNormalize( vProjTexNormal );
		EdgeAlignNormalize( vProjPolyNormal );

		fDot = (float)EdgeAlignDot( vProjTexNormal, vProjPolyNormal );
		fAngle = (float)( acos( fDot ) * ( 180.0f / M_PI ) );
		if ( fDot < 0.0f )
			fAngle = 180.0f - fAngle;

		// Ok, rotate them for the final values.
		EdgeAlignRotation( mEdgeRotation, vEdge, fAngle );
		axis[0] = EdgeAlignRotate( mEdgeRotation, axis[0] );
		axis[1] = EdgeAlignRotate( mEdgeRotation, axis[1] );

		vOrigin = EdgeAlignRotateAbout( mEdgeRotation, vOrigin, vEdgePt );
	}

	pTo->texture.UAxis.AsVector3D() = axis[0];
	pTo->texture.VAxis.AsVector3D() = axis[1];

	pTo->texture.UAxis[3] = (float)( ( ( (double)vOrigin[0] * axis[0][0] + (double)axis[0][2] * vOrigin[2] ) +
									   (double)axis[0][1] * vOrigin[1] ) /
									 pFrom->texture.scale[0] );
	pTo->texture.VAxis[3] = (float)( EdgeAlignDot( axis[1], vOrigin ) / pFrom->texture.scale[1] );

	pTo->texture.scale[0] = pFrom->texture.scale[0];
	pTo->texture.scale[1] = pFrom->texture.scale[1];

	// rotate is only for UI purposes, it doesn't actually do anything.
	pTo->texture.rotate = 0.0f;

	pTo->CalcTextureCoords();
}

//-----------------------------------------------------------------------------
// Purpose: Applies dialog data to the list of selected faces.
// Input  : *pOnlyFace -
//			bAll -
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::Apply( CMapFace* pOnlyFace, BOOL bAll, BOOL bEdgeAlign )
{
	int				i;
	CString			str;
	float			fshiftX = NOT_INIT;
	float			fshiftY = NOT_INIT;
	float			fscaleX = NOT_INIT;
	float			fscaleY = NOT_INIT;
	float			frotate = NOT_INIT;
	int				material = NOT_INIT;
	IEditorTexture* pTex = m_TexturePic.GetTexture();

	//
	// Get numeric data.
	//
	TransferToFloat( &m_shiftX, fshiftX );
	TransferToFloat( &m_shiftY, fshiftY );
	TransferToFloat( &m_scaleX, fscaleX );
	TransferToFloat( &m_scaleY, fscaleY );
	TransferToFloat( &m_rotate, frotate );

	if ( !pOnlyFace )
	{
		GetHistory()->MarkUndoPosition( NULL, "Apply Face Attributes" );
		// make sure we apply everything in this case.
		bAll = TRUE;
	}

	//
	// Run thru stored faces & apply.
	//
	CFaceEditSheet* pSheet = (CFaceEditSheet*)this;
	int				faceCount = pSheet->GetFaceListCount();
	for ( i = 0; i < faceCount || pOnlyFace; i++ )
	{
		CMapFace* pFace;
		if ( pOnlyFace )
		{
			pFace = pOnlyFace;
		}
		else
		{
			pFace = pSheet->GetFaceListDataFace( i );
			CMapSolid* pSolid = pSheet->GetFaceListDataSolid( i );
			GetHistory()->Keep( pSolid );
		}

		//
		// Get values for texture shift, scale, rotate, and material.
		//
		if ( bAll )
		{
			if ( !bEdgeAlign )
			{
				if ( fshiftX != NOT_INIT )
					pFace->texture.UAxis[3] = fshiftX;

				if ( fshiftY != NOT_INIT )
					pFace->texture.VAxis[3] = fshiftY;

				if ( fscaleX != NOT_INIT )
					pFace->texture.scale[0] = fscaleX;

				if ( fscaleY != NOT_INIT )
					pFace->texture.scale[1] = fscaleY;

				if ( frotate != NOT_INIT )
				{
					pFace->RotateTextureAxes( frotate - pFace->texture.rotate );
					pFace->texture.rotate = frotate;
				}
			}

			if ( material != NOT_INIT )
				pFace->texture.material = material;
		}

		//
		// Update the texture and recalculate texture coordinates.
		//
		if ( pTex != NULL )
		{
			char szCurrentTexName[MAX_PATH];
			char szNewTexName[MAX_PATH];

			pFace->GetTextureName( szCurrentTexName );
			pTex->GetShortName( szNewTexName );

			if ( stricmp( szCurrentTexName, szNewTexName ) != 0 )
				pFace->SetTexture( szNewTexName );
		}

		//
		// Copy texture coordinate system.
		//
		if ( bEdgeAlign && faceCount >= 1 )
			CopyTCoordSystem( pSheet->GetFaceListDataFace( faceCount - 1 ), pFace );

		//
		// Recalculate texture coordinates.
		//
		pFace->CalcTextureCoords();

		//
		// Update the face flags.
		//

		if ( pOnlyFace )
			break;
	}

	CMapDoc::GetActiveMapDoc()->UpdateAllViews( NULL, MAPVIEW_UPDATE_OBJECTS | MAPVIEW_COLOR_ONLY );
	CMapDoc::GetActiveMapDoc()->SetModifiedFlag();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pOnlyFace -
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::UpdateDialogData( CMapFace* pOnlyFace )
{
	BOOL  bFirst;
	int	  nFaceAlignCount;
	int	  nWorldAlignCount;
	float fshiftX = NOT_INIT;
	float fshiftY = NOT_INIT;
	float fscaleX = NOT_INIT;
	float fscaleY = NOT_INIT;
	float frotate = NOT_INIT;
	//float fsmooth = NOT_INIT;
	int		material = NOT_INIT;
	CString strTexture;

	bFirst = TRUE;
	nFaceAlignCount = 0;
	nWorldAlignCount = 0;

	CFaceEditSheet* pSheet = (CFaceEditSheet*)this;
	int				faceCount = pSheet->GetFaceListCount();

	for ( int i = 0; i < faceCount || pOnlyFace; i++ )
	{
		CMapFace* pFace;

		if ( pOnlyFace )
			pFace = pOnlyFace;
		else
			pFace = pSheet->GetFaceListDataFace( i );

		TEXTURE& t = pFace->texture;

		//
		// Gather statistics about the texture alignment of all the selected faces.
		// This is used later to set the state of the alignment checkboxes.
		//
		int nAlignment = pFace->GetTextureAlignment();
		if ( nAlignment & TEXTURE_ALIGN_FACE )
			nFaceAlignCount++;

		if ( nAlignment & TEXTURE_ALIGN_WORLD )
			nWorldAlignCount++;

		//
		// First update - copy first face's stuff into edit fields.
		//
		if ( bFirst )
		{
			fshiftX = t.UAxis[3];
			fshiftY = t.VAxis[3];
			fscaleX = t.scale[0];
			fscaleY = t.scale[1];
			frotate = t.rotate;
			material = t.material;
			strTexture = t.texture;

			//
			// Get the face's orientation. This is used by Apply to make intelligent decisions.
			//
			m_eOrientation = pFace->GetOrientation();
			ASSERT( m_eOrientation != FACE_ORIENTATION_INVALID );

			// Set the appropriate checkbox state for the face attributes.

			bFirst = FALSE;

			if ( pOnlyFace ) // use one face - now break
				break;
		}
		else
		{
			// update fields with face's data
			if ( t.UAxis[3] != fshiftX )
				fshiftX = NOT_INIT;

			if ( t.VAxis[3] != fshiftY )
				fshiftY = NOT_INIT;

			if ( t.scale[0] != fscaleX )
				fscaleX = NOT_INIT;

			if ( t.scale[1] != fscaleY )
				fscaleY = NOT_INIT;

			if ( t.rotate != frotate )
				frotate = NOT_INIT;

			if ( t.material != material )
				material = NOT_INIT;

			if ( !strTexture.IsEmpty() && strTexture != t.texture )
				strTexture = "";

			// Update the checkbox state for the face attributes. If any of this face's
			// attributes are different from the current checkbox state, set the checkbox
			// to the undefined state.
		}
	}

	//
	// Set the state of the face alignment checkbox.
	//
	CButton* pFaceAlign = (CButton*)GetDlgItem( IDC_ALIGN_FACE );

	if ( nFaceAlignCount == 0 )
		pFaceAlign->SetCheck( 0 );
	else if ( nFaceAlignCount == faceCount )
		pFaceAlign->SetCheck( 1 );
	else
		pFaceAlign->SetCheck( 2 );

	//
	// Set the state of the world alignment checkbox.
	//
	CButton* pWorldAlign = (CButton*)GetDlgItem( IDC_ALIGN_WORLD );

	if ( nWorldAlignCount == 0 )
		pWorldAlign->SetCheck( 0 );
	else if ( nWorldAlignCount == faceCount )
		pWorldAlign->SetCheck( 1 );
	else
		pWorldAlign->SetCheck( 2 );

	//
	// Set up fields.
	//
	FloatToSpin( fshiftX, (CSpinButtonCtrl*)GetDlgItem( IDC_SPINSHIFTX ), FALSE );
	FloatToSpin( fshiftY, (CSpinButtonCtrl*)GetDlgItem( IDC_SPINSHIFTY ), FALSE );

	FloatToWnd( fscaleX, &m_scaleX );
	FloatToWnd( fscaleY, &m_scaleY );

	FloatToSpin( frotate, (CSpinButtonCtrl*)GetDlgItem( IDC_SPINROTATE ), TRUE );

	if ( !strTexture.IsEmpty() )
	{
		SelectTexture( strTexture );
	}
	else
	{
		// make empty
		m_TextureList.SetCurSel( -1 );
	}

	//
	// if no faces selected -- get selection from texture bar
	//
	if ( faceCount == 0 )
	{
		CString strTexName = GetDefaultTextureName();
		SelectTexture( strTexName );
	}

	//
	// Call ctexturebar implementation because OUR implementation sets the
	// q2 checkboxes, which flashes the screen a bit (cuz we change them
	// again three lines down.)
	//
	UpdateTexture();

	// Update the smoothing group data.
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : uCmd -
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CFaceEditMaterialPage::OnAlign( UINT uCmd )
{
	// Set the material tool current.
	SetMaterialPageTool( MATERIALPAGETOOL_MATERIAL );

	// mark position in undo stack
	GetHistory()->MarkUndoPosition( NULL, "Align texture" );

	CFaceEditSheet* pSheet = (CFaceEditSheet*)this;
	int				faceCount = pSheet->GetFaceListCount();

	for ( int i = 0; i < faceCount; i++ )
	{
		CMapFace* pFace = pSheet->GetFaceListDataFace( i );

		CMapSolid* pSolid = pSheet->GetFaceListDataSolid( i );
		GetHistory()->Keep( pSolid );

		switch ( uCmd )
		{
		case IDC_ALIGN_WORLD:
		{
			pFace->InitializeTextureAxes( TEXTURE_ALIGN_WORLD, INIT_TEXTURE_AXES | INIT_TEXTURE_FORCE );
			break;
		}

		case IDC_ALIGN_FACE:
		{
			pFace->InitializeTextureAxes( TEXTURE_ALIGN_FACE, INIT_TEXTURE_AXES | INIT_TEXTURE_FORCE );
			break;
		}
		}
	}

	CMapDoc::GetActiveMapDoc()->UpdateAllViews( NULL, MAPVIEW_UPDATE_OBJECTS );
	CMapDoc::GetActiveMapDoc()->SetModifiedFlag();

	UpdateDialogData();

	return ( TRUE );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnHideMask( void )
{
	m_bHideMask = m_cHideMask.GetCheck();

	CMapFace::SetShowSelection( m_bHideMask == FALSE );

	CMapDoc::GetActiveMapDoc()->UpdateAllViews( NULL, MAPVIEW_UPDATE_OBJECTS | MAPVIEW_COLOR_ONLY );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : Extents -
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::GetAllFaceExtents( Extents_t Extents )
{
	BOOL	  bFirst = TRUE;
	Extents_t FaceExtents;

	CFaceEditSheet* pSheet = (CFaceEditSheet*)this;
	int				faceCount = pSheet->GetFaceListCount();

	for ( int nFace = 0; nFace < faceCount; nFace++ )
	{
		CMapFace* pFace = pSheet->GetFaceListDataFace( nFace );
		pFace->GetFaceExtents( FaceExtents );

		if ( ( FaceExtents[EXTENTS_XMIN][0] < Extents[EXTENTS_XMIN][0] ) || ( bFirst ) )
			Extents[EXTENTS_XMIN] = FaceExtents[EXTENTS_XMIN];

		if ( ( FaceExtents[EXTENTS_XMAX][0] > Extents[EXTENTS_XMAX][0] ) || ( bFirst ) )
			Extents[EXTENTS_XMAX] = FaceExtents[EXTENTS_XMAX];

		if ( ( FaceExtents[EXTENTS_YMIN][1] < Extents[EXTENTS_YMIN][1] ) || ( bFirst ) )
			Extents[EXTENTS_YMIN] = FaceExtents[EXTENTS_YMIN];

		if ( ( FaceExtents[EXTENTS_YMAX][1] > Extents[EXTENTS_YMAX][1] ) || ( bFirst ) )
			Extents[EXTENTS_YMAX] = FaceExtents[EXTENTS_YMAX];

		if ( ( FaceExtents[EXTENTS_ZMIN][2] < Extents[EXTENTS_ZMIN][2] ) || ( bFirst ) )
			Extents[EXTENTS_ZMIN] = FaceExtents[EXTENTS_ZMIN];

		if ( ( FaceExtents[EXTENTS_ZMAX][2] > Extents[EXTENTS_ZMAX][2] ) || ( bFirst ) )
			Extents[EXTENTS_ZMAX] = FaceExtents[EXTENTS_ZMAX];

		bFirst = FALSE;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : uCmd -
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CFaceEditMaterialPage::OnJustify( UINT uCmd )
{
	// Set the material tool current.
	SetMaterialPageTool( MATERIALPAGETOOL_MATERIAL );

	BOOL	  bTreatManyAsOneFace;
	Extents_t Extents;

	// mark undo position
	GetHistory()->MarkUndoPosition( NULL, "Justify texture" );

	CFaceEditSheet* pSheet = (CFaceEditSheet*)this;
	int				faceCount = pSheet->GetFaceListCount();

	// If multiple faces are selected, use the m_bTreatManyAsOneFace variable to determine
	// how to perform the justification.
	if ( faceCount > 1 )
	{
		bTreatManyAsOneFace = m_bTreatAsOneFace;
		if ( bTreatManyAsOneFace )
			GetAllFaceExtents( Extents );
	}
	// If only one face is selected, treat it singly.
	else
	{
		bTreatManyAsOneFace = FALSE;
	}

	for ( int i = 0; i < faceCount; i++ )
	{
		CMapFace* pFace = pSheet->GetFaceListDataFace( i );

		CMapSolid* pSolid = pSheet->GetFaceListDataSolid( i );
		GetHistory()->Keep( pSolid );

		if ( !bTreatManyAsOneFace )
			pFace->GetFaceExtents( Extents );

		switch ( uCmd )
		{
		case IDC_JUSTIFY_TOP:
		{
			pFace->JustifyTextureUsingExtents( TEXTURE_JUSTIFY_TOP, Extents );
			break;
		}

		case IDC_JUSTIFY_BOTTOM:
		{
			pFace->JustifyTextureUsingExtents( TEXTURE_JUSTIFY_BOTTOM, Extents );
			break;
		}

		case IDC_JUSTIFY_LEFT:
		{
			pFace->JustifyTextureUsingExtents( TEXTURE_JUSTIFY_LEFT, Extents );
			break;
		}

		case IDC_JUSTIFY_RIGHT:
		{
			pFace->JustifyTextureUsingExtents( TEXTURE_JUSTIFY_RIGHT, Extents );
			break;
		}

		case IDC_JUSTIFY_CENTER:
		{
			pFace->JustifyTextureUsingExtents( TEXTURE_JUSTIFY_CENTER, Extents );
			break;
		}

		case IDC_JUSTIFY_FITTOFACE:
		{
			pFace->JustifyTextureUsingExtents( TEXTURE_JUSTIFY_FIT, Extents );
			break;
		}
		}
	}

	CMapDoc::GetActiveMapDoc()->UpdateAllViews( NULL, MAPVIEW_UPDATE_OBJECTS );
	CMapDoc::GetActiveMapDoc()->SetModifiedFlag();

	UpdateDialogData();

	return ( TRUE );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : id -
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CFaceEditMaterialPage::OnSwitchMode( UINT id )
{
	CWnd* pButton = GetDlgItem( IDC_MODE );

	CFaceEditSheet* pSheet = (CFaceEditSheet*)this;
	pSheet->SetClickMode( id );

	switch ( id )
	{
	case CFaceEditSheet::ModeLiftSelect: // set
		pButton->SetWindowText( "Mode: Lift+Select" );
		break;
	case CFaceEditSheet::ModeLift:
		pButton->SetWindowText( "Mode: Lift" );
		break;
	case CFaceEditSheet::ModeSelect:
		pButton->SetWindowText( "Mode: Select" );
		break;
	case CFaceEditSheet::ModeApply:
		pButton->SetWindowText( "Mode: Apply (texture)" );
		break;
	case CFaceEditSheet::ModeApplyAll:
		pButton->SetWindowText( "Mode: Apply (all)" );
		break;
	case CFaceEditSheet::ModeAlignToView:
		pButton->SetWindowText( "Align To View" );
		break;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnMode()
{
	// Set the material tool current.
	SetMaterialPageTool( MATERIALPAGETOOL_MATERIAL );

	// switch mode -
	//  LIFT - lift texture from clicked face
	//	APPLY - apply selected texture to clicked face
	//	SELECT - mark each face
	//	LIFT/SELECT - mark clicked faces & lift textures

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu( MF_STRING, CFaceEditSheet::ModeLiftSelect, "Lift+Select" );
	menu.AppendMenu( MF_STRING, CFaceEditSheet::ModeLift, "Lift" );
	menu.AppendMenu( MF_STRING, CFaceEditSheet::ModeSelect, "Select" );
	menu.AppendMenu( MF_STRING, CFaceEditSheet::ModeApply, "Apply (texture only)" );
	menu.AppendMenu( MF_STRING, CFaceEditSheet::ModeApplyAll, "Apply (texture + values)" );
	menu.AppendMenu( MF_STRING, CFaceEditSheet::ModeAlignToView, "Align To View" );

	// track menu
	CWnd* pButton = GetDlgItem( IDC_MODE );
	CRect r;
	pButton->GetWindowRect( r );
	menu.TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, r.left, r.bottom, this, NULL );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : nSBCode -
//			nPos -
//			*pScrollBar -
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnVScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar )
{
	Apply();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pNMHDR -
//			pResult -
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnDeltaPosFloatSpin( NMHDR* pNMHDR, LRESULT* pResult )
{
	NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;

	CEdit* pEdit = NULL;
	switch ( pNMUpDown->hdr.idFrom )
	{
	case IDC_SPINSCALEY:
	{
		pEdit = &m_scaleY;
		break;
	}

	case IDC_SPINSCALEX:
	{
		pEdit = &m_scaleX;
		break;
	}
	}

	if ( pEdit != NULL )
	{
		CString str;
		pEdit->GetWindowText( str );
		float fTmp = atof( str );
		fTmp += 0.1f * float( pNMUpDown->iDelta );
		str.Format( "%.2f", fTmp );
		pEdit->SetWindowText( str );

		*pResult = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : nType -
//			cx -
//			cy -
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnSize( UINT nType, int cx, int cy )
{
	return;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnSelChangeTexture( void )
{
	// Set the material tool current.
	SetMaterialPageTool( MATERIALPAGETOOL_MATERIAL );

	if ( !m_bInitialized )
		return;

	UpdateTexture();

	if ( m_pCurTex != NULL )
		m_TextureList.AddMRU( m_pCurTex );
}

// Purpose:

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnTreatAsOne( void )
{
	// Set the material tool current.
	SetMaterialPageTool( MATERIALPAGETOOL_MATERIAL );

	CButton* pCheck = (CButton*)GetDlgItem( IDC_TREAT_AS_ONE );
	ASSERT( pCheck != NULL );
	m_bTreatAsOneFace = pCheck->GetCheck();
}

//-----------------------------------------------------------------------------
// Purpose: Invokes the texture replace dialog.
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnReplace( void )
{
	// Set the material tool current.
	SetMaterialPageTool( MATERIALPAGETOOL_MATERIAL );

	//
	// get active map doc
	//
	CMapDoc* pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	// ready the replace dialog
	CReplaceTexDlg dlg( pDoc->Selection_GetCount() );

	// get the texture to replace -- the default texture?!
	dlg.m_strFind = GetDefaultTextureName();

	//
	// open replace dialog -- modal
	//
	if ( dlg.DoModal() != IDOK )
		return;

	// mark undo position
	GetHistory()->MarkUndoPosition( pDoc->Selection_GetList(), "Replace Textures" );

	if ( dlg.m_bMarkOnly )
		pDoc->SelectObject( NULL, CMapDoc::scClear ); // clear selection first

	pDoc->ReplaceTextures( dlg.m_strFind, dlg.m_strReplace, dlg.m_iSearchAll,
						   dlg.m_iAction | ( dlg.m_bMarkOnly ? 0x100 : 0 ), dlg.m_bHidden );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the m_pTexture data member based on the current selection.
//			Also updates the window text and the texture picture.
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::UpdateTexture( void )
{
	int iSel = m_TextureList.GetCurSel();

	if ( iSel == LB_ERR )
	{
		m_TexturePic.SetTexture( NULL );
		m_pCurTex = NULL;
		return;
	}

	m_pCurTex = (IEditorTexture*)m_TextureList.GetItemDataPtr( iSel );
	m_TexturePic.SetTexture( m_pCurTex );

	if ( m_pCurTex )
	{
		char szBuf[128];
		sprintf( szBuf, "%dx%d", m_pCurTex->GetWidth(), m_pCurTex->GetHeight() );
		GetDlgItem( IDC_TEXTURESIZE )->SetWindowText( szBuf );

		char szTexName[128];
		m_pCurTex->GetShortName( szTexName );
		SetDefaultTextureName( szTexName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Selects a texture by name.
// Input  : pszTextureName - Texture name to select.
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::SelectTexture( LPCSTR pszTextureName )
{
	int nIndex = m_TextureList.SelectString( -1, pszTextureName );

	//
	// If the texture is not in the list, add it to the list.
	//
	if ( nIndex == LB_ERR )
	{
		IEditorTexture* pTex = g_Textures.FindActiveTexture( pszTextureName );
		if ( pTex != NULL )
		{
			nIndex = m_TextureList.AddString( pszTextureName );
			m_TextureList.SetItemDataPtr( nIndex, pTex );
			m_TextureList.SetCurSel( nIndex );
		}
	}

	UpdateTexture();

	if ( nIndex != LB_ERR )
	{
		IEditorTexture* pTex = (IEditorTexture*)m_TextureList.GetItemDataPtr( nIndex );
		m_TextureList.AddMRU( pTex );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::NotifyGraphicsChanged( void )
{
	if ( !IsWindow( m_hWnd ) )
		return;

	// load groups into group list
	CString str;
	int		iCurSel = m_TextureGroupList.GetCurSel();
	if ( iCurSel != LB_ERR )
		m_TextureGroupList.GetLBText( iCurSel, str );

	m_TextureGroupList.SetRedraw( FALSE );
	m_TextureGroupList.ResetContent();
	m_TextureGroupList.AddString( "All Textures" );

	POSITION p = g_Textures.GroupsGetHeadPosition();

	if ( p != NULL )
	{
		//
		// Skip first group ("All Textures").
		//
		g_Textures.GroupsGetNext( p );

		while ( p )
		{
			CTextureGroup* pGroup = g_Textures.GroupsGetNext( p );
			if ( pGroup->GetTextureFormat() == g_pGameConfig->GetTextureFormat() )
			{
				const char* p = strstr( pGroup->GetName(), "textures\\" );
				if ( p )
					p += strlen( "textures\\" );
				else
					p = pGroup->GetName();

				m_TextureGroupList.AddString( p );
			}
		}
	}
	m_TextureGroupList.SetRedraw( TRUE );

	if ( iCurSel == LB_ERR || m_TextureGroupList.SelectString( -1, str ) == LB_ERR )
		m_TextureGroupList.SetCurSel( 0 );

	m_TextureGroupList.Invalidate();

	char szName[MAX_PATH];
	m_TextureGroupList.GetLBText( m_TextureGroupList.GetCurSel(), szName );
	g_Textures.SetActiveGroup( szName );

	//
	// This is called when the loaded graphics list is changed,
	// or on first init by this->Create().
	//
	m_TextureList.LoadGraphicList();
	UpdateTexture();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnBrowse( void )
{
	// Set the material tool current.
	SetMaterialPageTool( MATERIALPAGETOOL_MATERIAL );

	CTextureBrowser* pBrowser = GetMainWnd()->pTextureBrowser;

	int iSel = m_TextureList.GetCurSel();

	if ( iSel != LB_ERR )
	{
		IEditorTexture* pTex = (IEditorTexture*)m_TextureList.GetItemDataPtr( iSel );
		if ( pTex != NULL )
		{
			char sz[128];

			pTex->GetShortName( sz );
			pBrowser->SetInitialTexture( sz );
		}
	}

	if ( pBrowser->DoModal() == IDOK )
	{
		IEditorTexture* pTex = g_Textures.FindActiveTexture( pBrowser->m_cTextureWindow.szCurTexture );
		if ( pTex != NULL )
		{
			int iCount = m_TextureList.GetCount();
			for ( int i = 0; i < iCount; i++ )
			{
				if ( pTex == (IEditorTexture*)m_TextureList.GetItemDataPtr( i ) )
				{
					m_TextureList.SetCurSel( i );
					UpdateTexture();
					m_TextureList.AddMRU( pTex );
					break;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnChangeTextureGroup( void )
{
	// Set the material tool current.
	SetMaterialPageTool( MATERIALPAGETOOL_MATERIAL );

	int iGroup = m_TextureGroupList.GetCurSel();

	//
	// Set the active texture group by name.
	//
	char szName[MAX_PATH];
	m_TextureGroupList.GetLBText( iGroup, szName );
	g_Textures.SetActiveGroup( szName );

	//
	// Refresh the texture list contents.
	//
	m_TextureList.LoadGraphicList();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::OnButtonApply( void )
{
	// Set the material tool current.
	SetMaterialPageTool( MATERIALPAGETOOL_MATERIAL );

	Apply( NULL, TRUE );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditMaterialPage::SetMaterialPageTool( unsigned short iMaterialTool )
{

	// Set the new material tool.
	m_iMaterialTool = iMaterialTool;
}
