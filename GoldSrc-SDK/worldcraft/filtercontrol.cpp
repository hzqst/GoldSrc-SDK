//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "FilterControl.h"
#include "ControlBarIDs.h"
#include "MapDoc.h"
#include "MapWorld.h"
#include "GlobalFunctions.h"
#include "EditGroups.h"
#include "CustomMessages.h"
#include "VisGroup.h"

typedef struct
{
	CVisGroup*		group;
	CMapDoc*		doc;
	SelectMode_t	selectMode;
} MarkMembersInfo_t;

BEGIN_MESSAGE_MAP( CFilterControl, CWorldcraftBar )
	ON_BN_CLICKED( IDC_APPLY, OnApply )
	ON_BN_CLICKED( IDC_PURGEVISGROUPS, OnPurgeGroups )
	ON_BN_CLICKED( IDC_EDITGROUPS, OnEditGroups )
	ON_BN_CLICKED( IDC_MARKMEMBERS, OnMarkMembers )
	ON_UPDATE_COMMAND_UI( IDC_GROUPS, UpdateControl )
	ON_UPDATE_COMMAND_UI( IDC_APPLY, UpdateControl )
	ON_UPDATE_COMMAND_UI( IDC_PURGEVISGROUPS, UpdateControl )
	ON_UPDATE_COMMAND_UI( IDC_EDITGROUPS, UpdateControl )
	ON_UPDATE_COMMAND_UI( IDC_MARKMEMBERS, UpdateControl )
	ON_WM_ACTIVATE()
	ON_WM_SHOWWINDOW()
	ON_WM_WINDOWPOSCHANGED()
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParentWnd - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL CFilterControl::Create( CWnd* parent )
{
	if ( !CWorldcraftBar::Create( parent, IDD_FILTERCONTROL, CBRS_RIGHT, IDCB_FILTERCONTROL ) )
		return FALSE;

	SetWindowText( "Filter Control" );
	// set up controls
	m_cGroupBox.SubclassDlgItem( IDC_GROUPS, this );
	m_cGroupImageList.Create( IDB_VISGROUPSTATUS, 10, 1, RGB( 255, 255, 255 ) );
	m_cGroupBox.SetImageList( &m_cGroupImageList, LVSIL_SMALL );
	CRect rect;
	m_cGroupBox.GetClientRect( &rect );
	// set up columns in groupbox
	m_cGroupBox.InsertColumn( 0, "pic", LVCFMT_LEFT, rect.Width() - 20 );
//	m_cGroupBox.InsertColumn(1, "name", LVCFMT_LEFT, 60);
	UpdateGroupList();
	bInitialized = TRUE;
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFilterControl::UpdateGroupList()
{
	CMapDoc* doc = CMapDoc::GetActiveMapDoc();
	if ( !doc )
		return;

	m_cGroupBox.SetRedraw( FALSE );
	m_cGroupBox.DeleteAllItems();
	POSITION pos = doc->VisGroups_GetHeadPosition();
	int row = 0;
	while ( pos )
	{
		CVisGroup* group = doc->VisGroups_GetNext( pos );
		m_cGroupBox.InsertItem( row, group->GetName(), group->IsVisible() );
		m_cGroupBox.SetItemData( row, group->GetID() );
		row++;
	}
	m_cGroupBox.SetRedraw( TRUE );
	m_cGroupBox.Invalidate();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pCmdUI - 
//-----------------------------------------------------------------------------
void CFilterControl::UpdateControl( CCmdUI* cmdUI )
{
	cmdUI->Enable( GetActiveWorld() != NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTarget - 
//			bDisableIfNoHndler - 
//-----------------------------------------------------------------------------
void CFilterControl::OnUpdateCmdUI( CFrameWnd* target, BOOL )
{
	UpdateDialogControls( target, FALSE );
}

void CFilterControl::OnApply()
{
	CMapDoc* doc = CMapDoc::GetActiveMapDoc();
	if ( !doc )
		return;

	bool changed = false;
	for ( int row = 0; row < m_cGroupBox.GetItemCount(); row++ )
	{
		LVITEM item = {};
		item.mask = LVIF_IMAGE;
		item.iItem = row;
		m_cGroupBox.GetItem( &item );
		CVisGroup* group = doc->VisGroups_GroupForID( m_cGroupBox.GetItemData( row ) );
		if ( group && group->IsVisible() != ( item.iImage != 0 ) )
		{
			group->SetVisible( item.iImage != 0 );
			changed = true;
		}
	}
	if ( changed )
		doc->UpdateVisibilityAll( 1 );
}

static BOOL IsOutsideVisGroup( CMapClass* object, CVisGroup* group )
{
	return object->GetOwnVisGroup() != group;
}

void CFilterControl::OnPurgeGroups()
{
	CMapDoc* doc = CMapDoc::GetActiveMapDoc();
	if ( !doc )
		return;

	for ( int row = 0; row < m_cGroupBox.GetItemCount(); row++ )
	{
		DWORD id = m_cGroupBox.GetItemData( row );
		CVisGroup* group = doc->VisGroups_GroupForID( id );
		if ( doc->GetMapWorld()->EnumChildren( (ENUMMAPCHILDRENPROC)IsOutsideVisGroup, (DWORD)group ) )
		{
			doc->VisGroups_RemoveID( id );
			m_cGroupBox.DeleteItem( row-- );
		}
	}
}

void CFilterControl::OnEditGroups()
{
	CEditGroups dialog;
	dialog.DoModal();
	UpdateGroupList();
}

static BOOL MarkMembersOfGroup( CMapClass* object, MarkMembersInfo_t* info )
{
	if ( object->GetVisGroup() == info->group && info->doc->ShouldObjectBeVisible( object ) )
	{
		CMapClass* selected = object->PrepareSelection( info->selectMode );
		if ( selected )
			info->doc->SelectObject( selected, CMapDoc::scSelect );
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CFilterControl::OnMarkMembers()
{
	CMapDoc* doc = CMapDoc::GetActiveMapDoc();
	if ( !doc )
		return;
	int row = m_cGroupBox.GetNextItem( -1, LVNI_SELECTED );
	if ( row == -1 )
		return;
	CVisGroup* group = doc->VisGroups_GroupForID( m_cGroupBox.GetItemData( row ) );
	if ( !group )
		return;

	MarkMembersInfo_t info = { group, doc, doc->Selection_GetMode() };
	doc->SelectObject( NULL, CMapDoc::scClear );
	doc->GetMapWorld()->EnumChildren( (ENUMMAPCHILDRENPROC)MarkMembersOfGroup, (DWORD)&info );
	doc->SelectObject( NULL, CMapDoc::scUpdateDisplay );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pPos - 
//-----------------------------------------------------------------------------
void CFilterControl::OnWindowPosChanged( WINDOWPOS* position )
{
	if ( bInitialized && ( position->flags & SWP_SHOWWINDOW ) )
		UpdateGroupList();
	Default();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bShow - 
//			nStatus - 
//-----------------------------------------------------------------------------
void CFilterControl::OnShowWindow( BOOL show, UINT )
{
	if ( show )
		UpdateGroupList();
	Default();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nState - 
//			pWnd - 
//			bMinimized - 
//-----------------------------------------------------------------------------
void CFilterControl::OnActivate( UINT state, CWnd*, BOOL )
{
	if ( state == WA_ACTIVE )
		UpdateGroupList();
	Default();
}
