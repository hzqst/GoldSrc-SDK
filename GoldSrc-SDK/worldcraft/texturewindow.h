//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef TEXTUREWINDOW_H
#define TEXTUREWINDOW_H
#pragma once


#include "IEditorTexture.h"


class CTextureWindow : public CWnd
{
public:
	CTextureWindow();
	virtual ~CTextureWindow();

	void Create(CWnd *pParentWnd, RECT& rect);

	struct TWENUMPOS
	{
		IEditorTexture *pTex;
		RECT texrect;
		int cur_x, cur_y;
		RECT clientrect;
		int largest_y;
		int iTexIndex;
	};

	void EnableUpdate(bool bEnable);
	void UpdateScrollSizes();
	BOOL EnumTexturePositions(TWENUMPOS *pTE, BOOL bStart = FALSE);
	void SetDisplaySize(int iSize);
	void HighlightCurTexture(CDC *pDC = NULL);
	void SetNameFilter(LPCTSTR pszFilter);
	void SetFileFilter( const char* pszFilter );
	void SetTextureFormat(TEXTUREFORMAT eTextureFormat);
	void SelectTexture(LPCTSTR pszTexture, BOOL bAllowRedraw = TRUE);
	void SetSpecificList(ITextureList *pList);

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTextureWindow)
	//}}AFX_VIRTUAL

	char szCurTexture[128];

protected:

	bool MatchKeywords(const char *pszSearch, char **pszKeyword, int nKeywords);

	int total_x;
	int total_y;
	int iDisplaySize;
	int iTexNameCharWidth;
	BOOL bFirstPaint;
	CFont TexFont;
	ITextureList *m_pSpecificList;
	CRect rectHighlight;

	char m_szFilter[128];			// Name filter, space, comma, or semicolon delimited.
	int m_nFilters;					// The number of names that were parsed out of the name filter.
	char *m_Filters[64];			// The individual name filters.


	bool m_bEnableUpdate;			// Locks GUI updates to control repaints.

	TEXTUREFORMAT m_eTextureFormat;
	DWORD m_dwFileFilter;

	//{{AFX_MSG(CTextureWindow)
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


#endif // TEXTUREWINDOW_H
