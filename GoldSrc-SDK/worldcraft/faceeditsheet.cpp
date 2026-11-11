//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdafx.h>
#include "Worldcraft.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "FaceEditSheet.h"
#include "MapSolid.h"
#include "MapFace.h"
#include "ToolManager.h"

//=============================================================================

IMPLEMENT_DYNAMIC( CFaceEditSheet, CFaceEditMaterialPage )

BEGIN_MESSAGE_MAP( CFaceEditSheet, CFaceEditMaterialPage )
//{{AFX_MSG_MAP( CFaceEdtiSheet )
ON_WM_CLOSE()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CFaceEditSheet::CFaceEditSheet()
{
	m_FaceCount = 0;
	m_Faces.SetSize( 0, 32 );
	m_ClickMode = -1;
	m_bEnableUpdate = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CFaceEditSheet::~CFaceEditSheet()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL CFaceEditSheet::Create( CWnd* pParentWnd )
{
	if ( !CTextureBar::Create( pParentWnd, IDD_FACEEDIT, IDCB_FACEEDIT ) )
		return FALSE;

	SetWindowText( "Face Properties" );
	Init();
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: Disables window updates when adding a large number of faces to the
//			dialog at once. When updates are re-enabled, the window is updated.
// Input  : bEnable - true to enable updates, false to disable them.
//-----------------------------------------------------------------------------
void CFaceEditSheet::EnableUpdate( bool bEnable )
{
	bool bOld = m_bEnableUpdate;
	m_bEnableUpdate = bEnable;

	if ( ( bEnable ) && ( !bOld ) )
		UpdateDialogData();
}

//-----------------------------------------------------------------------------
// Purpose: Clear the face list.
//-----------------------------------------------------------------------------
void CFaceEditSheet::ClearFaceList( void )
{
	//
	// reset selection state of all faces currently in the list
	//
	for ( int i = 0; i < m_FaceCount; i++ )
		m_Faces[i].pMapFace->SetSelectionState( SELECT_NONE );

	//
	// reset the list
	//
	m_FaceCount = 0;
	m_Faces.SetSize( 0, FACE_LIST_SIZE );
}

//-----------------------------------------------------------------------------
// Purpose: Search for the given face in the face selection list.  If found,
//          return the index of the face in the list.  Otherwise, return -1.
//-----------------------------------------------------------------------------
int CFaceEditSheet::FindFaceInList( CMapFace* pFace )
{
	for ( int i = 0; i < m_FaceCount; i++ )
		if ( m_Faces[i].pMapFace == pFace )
			return i;

	return -1;
}

//-----------------------------------------------------------------------------
// It is really lame that I have to have material/displacement specific code
// here!  It is unfortunately necessary - see CMapDoc (SelectFace).  ClickFace
// not only specifies face selection (which it should do only!!!), but also
// specifies the "mode" (material specific) of the click -- LAME!!!!  This will
// be a problem spot if/when we make face selection general.  cab
//-----------------------------------------------------------------------------
void CFaceEditSheet::ClickFace( CMapSolid* pSolid, int faceIndex, int cmd, int clickMode )
{
	//
	// set the click mode (either to new mode or previously used)
	//
	if ( clickMode == -1 )
		clickMode = m_ClickMode;

	//
	// clear the face list?
	//
	if ( cmd & cfClear )

		ClearFaceList();
	cmd &= ~cfClear;

	//
	// check for valid solid
	//
	if ( !pSolid )
		return;

	//
	// check for face in list, -1 from FindFaceInList indicates face not found
	//
	CMapFace* pFace = pSolid->GetFace( faceIndex );
	int		  selectIndex = FindFaceInList( pFace );
	bool	  bFoundInList = ( selectIndex != -1 );

	//
	// handle the face list selection
	//
	if ( ( clickMode == ModeSelect ) || ( clickMode == ModeLiftSelect ) )
	{
		// toggle selection if necessary
		if ( cmd == cfToggle )
			cmd = bFoundInList ? cfUnselect : cfSelect;

		if ( ( cmd == cfSelect ) && !bFoundInList )
		{
			// add face to list
			StoredFace_t sf;
			sf.pMapFace = pFace;
			sf.pMapSolid = pSolid;
			m_Faces.SetAtGrow( m_FaceCount, sf );
			++m_FaceCount;
			pFace->SetSelectionState( SELECT_NORMAL );
		}
		else if ( ( cmd == cfUnselect ) && bFoundInList )
		{
			// remove from list
			m_Faces.RemoveAt( selectIndex );
			--m_FaceCount;
			pFace->SetSelectionState( SELECT_NONE );
		}
	}

	CFaceEditMaterialPage::ClickFace( pSolid, faceIndex, cmd, clickMode );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditSheet::SetVisibility( bool bVisible )
{
	GetDockingFrame()->ShowControlBar( this, bVisible, TRUE );
	if ( bVisible )
	{
		g_pToolManager->SetTool( TOOL_FACEEDIT_MATERIAL );
		SetMaterialPageTool( MATERIALPAGETOOL_MATERIAL );
		UpdateDialogData();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL CFaceEditSheet::PreTranslateMessage( MSG* pMsg )
{
	HACCEL hAccel = GetMainWnd()->GetAccelTable();
	if ( !( hAccel && ::TranslateAccelerator( GetMainWnd()->m_hWnd, hAccel, pMsg ) ) )
	{
		if ( IsDialogMessage( pMsg ) )
			return ( TRUE );

		return CWnd::PreTranslateMessage( pMsg );
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditSheet::OnClose( void )
{
	// toggle the face edit sheet and texture bar!
	GetMainWnd()->ShowFaceEditSheetOrTextureBar( false );

	SetMaterialPageTool( CFaceEditMaterialPage::MATERIALPAGETOOL_MATERIAL );

	// set the tool pointer as default tool
	GetMainWnd()->OnChangeTool( ID_TOOLS_POINTER );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CFaceEditSheet::UpdateControls( void )
{
	UpdateDialogData();
}
