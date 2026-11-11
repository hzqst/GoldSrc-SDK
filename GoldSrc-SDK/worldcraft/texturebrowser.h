//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef TEXTUREBROWSER_H
#define TEXTUREBROWSER_H
#pragma once


#include "resource.h"
#include "texturewindow.h"


class CTextureBrowser : public CDialog
{
public:

	CTextureBrowser(CWnd *pParent);

	//{{AFX_DATA(CTextureBrowser)
	enum { IDD = IDD_TEXTURES };
	//}}AFX_DATA

	void SetInitialTexture(LPCTSTR);
	inline CString GetCurTexture(void);
	void WriteSettings();
	void SetUsed(BOOL);
	void SaveAndExit();
	void SetTextureFormat(TEXTUREFORMAT eTextureFormat);

	CTextureWindow m_cTextureWindow; // dvs: make protected

protected:

	//{{AFX_MSG(CTextureBrowser)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSelendokTexturesize();
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnUsed();
	afx_msg void OnReplace();
	afx_msg void OnMark();
	afx_msg void OnChangeFilterOrKeywords(void);
	afx_msg void OnUpdateFiltersNOW();
	afx_msg LRESULT OnTexturewindowSelchange(WPARAM, LPARAM);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

	afx_msg void OnCancel();

	static CStringArray m_FilterHistory;
	static int m_nFilterHistory;

	CComboBox m_cSizeList;
	CStatic m_cCurName;
	CStatic m_cCurDescription;
	CButton m_cUsed;
	char szInitialTexture[128];
	DWORD m_uLastFilterChange;
	BOOL m_bFilterChanged;
	BOOL m_bUsed;
	CComboBox m_cFilter;
	ITextureList m_UsedGraphicsList;
};


//-----------------------------------------------------------------------------
// Purpose: Returns the name of the currently selected texture.
//-----------------------------------------------------------------------------
CString CTextureBrowser::GetCurTexture(void)
{
	return(CString(m_cTextureWindow.szCurTexture));
}

#endif // TEXTUREBROWSER_H
