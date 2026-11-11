//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef MAINFRM_H
#define MAINFRM_H
#ifdef _WIN32
#pragma once
#endif

#include "MDIClientWnd.h"
#include "fstream.h"
#include "FilterControl.h"
#include "ObjectBar.h"
#include "Texturebar.h"

class CChildFrame;
class CObjectProperties;
class CTextureBrowser;
class CFaceEditSheet;
class CMessageWnd;

class CMainFrame : public CMDIFrameWnd
{
	DECLARE_DYNAMIC( CMainFrame )

public:
	CMainFrame();
	virtual ~CMainFrame();


	BOOL IsUndoActive() { return m_bUndoActive; }
	void SetUndoActive( BOOL bActive );

	void SetBrightness( float fBrightness );
	void UpdateAllDocViews( DWORD dwCmd );
	void Configure();
	void GlobalNotify( int nCode );
	void OnDeleteActiveDocument( void );

	void LoadWindowStates( fstream* pFile = NULL );

	inline CFaceEditSheet* GetFaceEditSheet( void ) { return m_pFaceEditSheet; }
	bool IsInFaceEditMode( void );
	inline CStatusBar* GetStatusBar() { return &m_wndStatusBar; }

	void ShowFaceEditSheetOrTextureBar( bool bShowFaceEditSheet );
	HACCEL GetAccelTable( void ) { return m_hAccelTable; }

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump( CDumpContext& dc ) const;
#endif

	//
	// Public attributes. FIXME: eliminate!
	//
	CMessageWnd* pMsgWnd;

	CTextureBrowser*   pTextureBrowser;
	CObjectProperties* pObjectProperties;

	CFilterControl	m_FilterControl;
	CObjectBar		m_ObjectBar;
	CTextureBar		m_TextureBar;
	CFaceEditSheet* m_pFaceEditSheet;

#ifndef SDK_BUILD
#endif // SDK_BUILD

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
public:
	virtual BOOL PreCreateWindow( CREATESTRUCT& cs );
	virtual BOOL PreTranslateMessage( MSG* pMsg );
	virtual void RecalcLayout( BOOL bNotify = TRUE );

protected:
	//virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void OnEditProperties();
	afx_msg void OnViewMessages();
	afx_msg void OnUpdateViewMessages( CCmdUI* pCmdUI );
	afx_msg void OnActivate( UINT nState, CWnd* pWndOther, BOOL bMinimized );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnClose();
	afx_msg void OnPaint();
	afx_msg void OnTimer( UINT nIDEvent );
	afx_msg void OnToolsOptions();
	afx_msg void OnViewShowconnections();
	afx_msg void OnToolsPrefabfactory();
	afx_msg BOOL OnHelpOpenURL( UINT nID );
	afx_msg void OnHelpFinder();
	afx_msg void OnEditUndoredoactive();
	afx_msg void OnUpdateEditUndoredoactive( CCmdUI* pCmdUI );
	afx_msg BOOL OnFileNew( UINT );
	afx_msg void OnSavewindowstate();
	afx_msg void OnLoadwindowstate();
	afx_msg void OnFileOpen();
	afx_msg BOOL OnChange3dViewType( UINT nID );
	afx_msg void OnUpdateView3d( CCmdUI* pCmdUI );
	afx_msg void OnUpdateView2d( CCmdUI* pCmdUI );
	afx_msg void OnUpdateToolUI( CCmdUI* pUI );
	afx_msg BOOL OnView3dChangeBrightness( UINT nID );
	afx_msg void OnUpdateApplicatorUI( CCmdUI* pUI );
	afx_msg BOOL OnHelpInfo( HELPINFO* );
	afx_msg void OnActivateApp( BOOL bActive, DWORD dwThreadID );
	afx_msg BOOL OnApplicator( UINT nID );
	afx_msg void OnViewBrusheditor();
	afx_msg void OnUpdateViewBrusheditor( CCmdUI* pCmdUI );
	afx_msg void OnTest();

public:
	afx_msg BOOL OnChangeTool( UINT nID );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

private:
	void DockControlBarLeftOf( CControlBar* Bar, CControlBar* LeftOf );

	void SaveWindowStates( fstream* pFile = NULL );

	CChildFrame* GetNextMDIChildWnd( CChildFrame* pCurChild );
	CChildFrame* GetNextMDIChildWndRecursive( CWnd* pCurChild );

	void EnableFaceEditMode( bool bEnable );

	CWnd			   MDIClient;
	CMDIClientWnd	   wndMDIClient; // dvs: what in God's name is this for?

	BOOL m_bUndoActive;
	BOOL m_bTextureBarVisible;
	bool m_bFaceEditMode;

	CStatusBar m_wndStatusBar;
	CToolBar   m_wndMapToolBar;
	CToolBar   m_wndMapEditToolBar;
	CToolBar   m_wndMapOps;

	CBitmap m_bmMapEditTools256;
};

CMainFrame* GetMainWnd();

#endif // MAINFRM_H
