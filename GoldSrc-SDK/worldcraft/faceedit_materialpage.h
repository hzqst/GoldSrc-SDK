//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FACEEDIT_MATERIALPAGE_H
#define FACEEDIT_MATERIALPAGE_H
#pragma once

#include "resource.h"
#include "TextureBar.h"
#include "IEditorTexture.h"
#include "wndTex.h"
#include "MapFace.h"

class CMapSolid;

//=============================================================================
//
// Face Edit Material Page Class
//
class CFaceEditMaterialPage : public CTextureBar
{

	DECLARE_DYNAMIC( CFaceEditMaterialPage );

public:
	enum
	{
		MATERIALPAGETOOL_NONE = 0,
		MATERIALPAGETOOL_MATERIAL,
		MATERIALPAGETOOL_SMOOTHING_GROUP
	};

	//=========================================================================
	//
	// Creation/Destruction
	//
	CFaceEditMaterialPage();
	~CFaceEditMaterialPage();

	void Init( void );

	//=========================================================================
	//
	// Update
	//
	void ClickFace( CMapSolid* pSolid, int faceIndex, int cmd, int clickMode = -1 ); // primary interface update call
	void Apply( CMapFace* pOnlyFace = NULL, BOOL bAll = FALSE, BOOL bEdgeAlign = FALSE );

	void NotifyGraphicsChanged( void );
	void UpdateDialogData( CMapFace* pFace = NULL );

	void SetMaterialPageTool( unsigned short iMaterialTool );
	unsigned short GetMaterialPageTool( void ) { return m_iMaterialTool; }

	//=========================================================================
	//
	// Dialog Data
	//
	//{{AFX_DATA( CFaceEditMaterialPage )
	enum
	{
		IDD = IDD_FACEEDIT
	};
	//}}AFX_DATA

	//=========================================================================
	//
	// Virtual Overrides
	//
	//{{AFX_VIRTUAL( CFaceEditMaterialPage )
	virtual BOOL PreTranslateMessage( MSG* pMsg );
	virtual void OnUpdateCmdUI( CFrameWnd* pTarget, BOOL bDisableIfNoHandler );
	//}}AFX_VIRTUAL

protected:
	CEdit	m_shiftX;
	CEdit	m_shiftY;
	CEdit	m_scaleX;
	CEdit	m_scaleY;
	CEdit	m_rotate;
	CButton m_cHideMask;
	CButton m_cExpand;
	wndTex	m_texture;

	BOOL m_bInitialized;
	BOOL m_bHideMask;
	BOOL m_bIgnoreResize;
	BOOL m_bTreatAsOneFace; // whether to consider all selected faces as one face.

	FaceOrientation_t m_eOrientation; // The orientation of the lifted face.

	unsigned short m_iMaterialTool;

	//=========================================================================
	//
	// Texture Browser/Update
	//
	void SelectTexture( LPCSTR pszTextureName );
	void UpdateTexture( void );

	//=========================================================================
	//
	// Texture Alignement
	//
	void AlignToView( CMapFace* pFace );
	void CopyTCoordSystem( const CMapFace* pFrom, CMapFace* pTo );
	void GetAllFaceExtents( Extents_t Extents );

	//=========================================================================
	//
	// Message Map
	//
	//{{AFX_MSG( CFaceEditMaterialPage )
	afx_msg void OnButtonApply( void );
	afx_msg BOOL OnAlign( UINT uCmd );
	afx_msg void OnHideMask();
	afx_msg BOOL OnJustify( UINT uCmd );
	afx_msg void OnMode();
	afx_msg void OnVScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar );
	afx_msg void OnDeltaPosFloatSpin( NMHDR* pNMHDR, LRESULT* pResult );
	afx_msg void OnSize( UINT, int, int );
	afx_msg void OnSelChangeTexture( void );

	afx_msg void OnTreatAsOne( void );
	afx_msg void OnReplace( void );
	afx_msg BOOL OnSwitchMode( UINT id );
	afx_msg void OnBrowse( void );
	afx_msg void OnChangeTextureGroup( void );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

#endif // FACEEDIT_MATERIALPAGE_H
