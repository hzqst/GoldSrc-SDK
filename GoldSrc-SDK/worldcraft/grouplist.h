//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

class CGroupList : public CListCtrl
{
public:
	CGroupList();
	virtual ~CGroupList();

// Generated message map functions
protected:
	//{{AFX_MSG(CGroupList)
	afx_msg void OnLButtonDown( UINT flags, CPoint point );
	afx_msg void OnBegindrag( NMHDR* header, LRESULT* result );
	afx_msg void OnEndlabeledit( NMHDR* header, LRESULT* result );
	afx_msg void OnLButtonUp( UINT flags, CPoint point );
	afx_msg void OnMouseMove( UINT flags, CPoint point );

	DECLARE_MESSAGE_MAP()

	CImageList*	m_pDragImageList;
	int			m_iLastDropItem;
	int			m_iDragItem;
};
