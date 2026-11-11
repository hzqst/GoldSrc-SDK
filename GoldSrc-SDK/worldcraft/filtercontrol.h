//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "resource.h"
#include "GroupList.h"
#include "WorldcraftBar.h"

class CFilterControl : public CWorldcraftBar
{
public:
	CFilterControl() : bInitialized( FALSE ) {}
	BOOL Create( CWnd* parent );
	void UpdateGroupList();
	virtual void OnUpdateCmdUI( CFrameWnd* target, BOOL disableIfNoHandler );

protected:
	afx_msg void OnApply();
	afx_msg void OnPurgeGroups();
	afx_msg void OnEditGroups();
	afx_msg void OnMarkMembers();
	afx_msg void UpdateControl( CCmdUI* cmdUI );
	afx_msg void OnActivate( UINT state, CWnd* otherWindow, BOOL minimized );
	afx_msg void OnShowWindow( BOOL show, UINT status );
	afx_msg void OnWindowPosChanged( WINDOWPOS* position );

	DECLARE_MESSAGE_MAP()

private:
	CGroupList	m_cGroupBox;
	CImageList	m_cGroupImageList;
	BOOL		bInitialized;
};
