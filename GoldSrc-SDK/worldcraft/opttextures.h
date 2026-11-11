//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

#ifndef OPTTEXTURES_H
#define OPTTEXTURES_H
#pragma once

#include "GameData.h"

class COPTTextures : public CPropertyPage
{
	DECLARE_DYNCREATE( COPTTextures )

public:
	//=========================================================================
	//
	// Construction/Deconstruction
	//
	COPTTextures();
	~COPTTextures();

	//=========================================================================
	//
	// Dialog Data
	//
	//{{AFX_DATA(COPTTextures)
	enum
	{
		IDD = IDD_OPTIONS_TEXTURES
	};
	CListBox	m_TextureFiles;
	CSliderCtrl m_cBrightness;
	//}}AFX_DATA

	//=========================================================================
	//
	// Overrides
	// ClassWizard generate virtual function overrides
	//
	//{{AFX_VIRTUAL(COPTTextures)
public:
	virtual BOOL OnApply();

protected:
	virtual void DoDataExchange( CDataExchange* pDX ); // DDX/DDV support
													   //}}AFX_VIRTUAL

	// Implementation
protected:
	BOOL m_bDeleted;

	//=========================================================================
	//
	// Generated message map functions
	//
	//{{AFX_MSG(COPTTextures)
	virtual BOOL OnInitDialog();
	afx_msg void OnExtract();
	afx_msg void OnAddtexfile();
	afx_msg void OnRemovetexfile();
	afx_msg void OnHScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

#endif // OPTTEXTURES_H
