//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "Worldcraft.h"
#include "ObjectProperties.h"
#include "ObjectPage.h"
#include "OP_Flags.h"
#include "OP_Groups.h"
#include "OP_Entity.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapGroup.h"
#include "MapSolid.h"
#include "MapWorld.h"
#include "History.h"
#include "GlobalFunctions.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//
// Layout types for remembering the last layout of the dialog. We could
// also remember this as an array of booleans for which pages were visible.
//
enum LayoutType_t
{
	ltZero,
	ltNone,
	ltSolid,		// Enable groups only
	ltEntity = 4,	// Enable entity, flags, groups
	ltWorld,
	ltMulti			// Enable groups only
};


//
// Used to indicate multiselect of entities with different keyvalues.
//
char *CObjectPage::VALUE_DIFFERENT_STRING = "(different)";


//
// Set while we are changing the page layout.
//
static BOOL bRESTRUCTURING = FALSE;


IMPLEMENT_DYNAMIC(CObjectProperties, CPropertySheet)


BEGIN_MESSAGE_MAP(CObjectProperties, CPropertySheet)
	//{{AFX_MSG_MAP(CObjectProperties)
	ON_WM_KILLFOCUS()
	ON_WM_ACTIVATE()
	ON_WM_CLOSE()
	ON_WM_SHOWWINDOW()
	ON_WM_CREATE()
	ON_COMMAND(IDC_APPLY, OnApply)
	ON_COMMAND(IDCANCEL, OnCancel)


	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


IMPLEMENT_DYNAMIC(editCMapClass, CObject);
IMPLEMENT_DYNAMIC(editCEditGameClass, CObject);


static editCMapClass e_CMapClass;
static editCEditGameClass e_CEditGameClass;


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CObjectProperties::CObjectProperties(void) :
	CPropertySheet()
{
	m_bDummy = false;
	m_pDummy = NULL;
	m_pApplyButton = NULL;
	m_pCancelButton = NULL;

	CreatePages();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
// Input  : nIDCaption -
//			pParentWnd -
//			iSelectPage -
//-----------------------------------------------------------------------------
CObjectProperties::CObjectProperties(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
	m_bDummy = false;
	m_pDummy = NULL;
	m_pApplyButton = NULL;
	m_pCancelButton = NULL;

	CreatePages();
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
// Input  : pszCaption -
//			pParentWnd -
//			iSelectPage -
//-----------------------------------------------------------------------------
CObjectProperties::CObjectProperties(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
	:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
	m_bDummy = false;
	m_pDummy = NULL;
	m_pApplyButton = NULL;
	m_pCancelButton = NULL;

	CreatePages();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CObjectProperties::~CObjectProperties()
{
	delete m_pDummy;

	delete m_pEntity;
	delete m_pFlags;
	delete m_pGroups;

	delete m_pApplyButton;
	delete m_pCancelButton;

	delete[] m_ppPages;
}


//-----------------------------------------------------------------------------
// Purpose: Creates all possible pages and attaches our object list to them.
//			Not all will be used depending on the types of objects being edited.
//-----------------------------------------------------------------------------
void CObjectProperties::CreatePages( void )
{
	m_pEntity = new COP_Entity;
	m_pEntity->SetObjectList( &m_DstObjects );

	m_pFlags = new COP_Flags;
	m_pFlags->SetObjectList( &m_DstObjects );

	m_pGroups = new COP_Groups;
	m_pGroups->SetObjectList( &m_DstObjects );

	m_pDummy = new CPropertyPage( IDD_OBJPAGE_DUMMY );

	m_ppPages = NULL;
	m_nPages = 0;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pType -
//-----------------------------------------------------------------------------
PVOID CObjectProperties::GetEditObject(CRuntimeClass *pType)
{
	if (pType == RUNTIME_CLASS(editCMapClass))
	{
		return PVOID((CMapClass*)&e_CMapClass);
	}
	else if (pType == RUNTIME_CLASS(editCEditGameClass))
	{
		return PVOID((CEditGameClass*)&e_CEditGameClass);
	}

	ASSERT(0);
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : pobj -
//			pType -
//-----------------------------------------------------------------------------
PVOID CObjectProperties::GetEditObjectFromMapObject(CMapClass *pobj, CRuntimeClass *pType)
{
	if (pType == RUNTIME_CLASS(editCMapClass))
	{
		return PVOID(pobj);
	}
	else if (pType == RUNTIME_CLASS(editCEditGameClass))
	{
		if (pobj->IsMapClass(MAPCLASS_TYPE(CMapEntity)))
		{
			return PVOID((CEditGameClass*)((CMapEntity*)pobj));
		}

		if (pobj->IsMapClass(MAPCLASS_TYPE(CMapWorld)))
		{
			return PVOID((CEditGameClass*)((CMapWorld*)pobj));
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pobj -
//-----------------------------------------------------------------------------
void CObjectProperties::CopyDataToEditObjects(CMapClass *pobj)
{
	//
	// All copies here are done without updating object dependencies, because
	// we're copying to a place that is outside of the world.
	//
	e_CMapClass.CopyFrom(pobj, false);

	if (pobj->IsMapClass(MAPCLASS_TYPE(CMapEntity)))
	{
		e_CEditGameClass.CopyFrom((CEditGameClass *)((CMapEntity *)pobj));
	}
	else if (pobj->IsMapClass(MAPCLASS_TYPE(CMapWorld)))
	{
		e_CEditGameClass.CopyFrom((CEditGameClass *)((CMapWorld *)pobj));
	}
}







//-----------------------------------------------------------------------------
// Purpose: Creates the Apply and Cancel buttons.
//-----------------------------------------------------------------------------
void CObjectProperties::CreateButtons(void)
{
	CRect rect;
	GetWindowRect(&rect);
	rect.InflateRect(0, 0, 0, 32);
	MoveWindow(&rect, FALSE);

	GetClientRect(&rect);

	//
	// Add an apply button.
	//
	m_pApplyButton = new CButton;
	m_pApplyButton->Create("Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(rect.right - 80, rect.bottom - 28, rect.right - 6, rect.bottom - 6), this, IDC_APPLY);

	HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	if (hFont == NULL)
	{
		hFont = (HFONT)GetStockObject(ANSI_VAR_FONT);
	}
  	m_pApplyButton->SendMessage(WM_SETFONT, (WPARAM)hFont);

	//
	// Add a cancel button.
	//
	m_pCancelButton = new CButton;
	m_pCancelButton->Create("Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, CRect(rect.right - 160, rect.bottom - 28, rect.right - 86, rect.bottom - 6), this, IDCANCEL);
  	m_pCancelButton->SendMessage(WM_SETFONT, (WPARAM)hFont);

}


//-----------------------------------------------------------------------------
// Purpose: Returns the appropriate page layout for the current object list.
//-----------------------------------------------------------------------------
LayoutType_t CObjectProperties::GetLayout( bool& bEntity, bool& bGroups, bool& bFlags )
{
	bEntity = bGroups = bFlags = false;
	if ( ( m_DstObjects.GetCount() == 0 ) || ( CMapDoc::GetActiveMapDoc() == NULL ) )
		return ltNone;
	//
	// Only show the model tab when we have a single entity selected that
	// has a model helper.
	//
	if ( m_DstObjects.GetCount() > 1 )
	{
		bEntity = bGroups = bFlags = true;
		return ltMulti;
	}

	MAPCLASSTYPE type = m_DstObjects.GetHead()->GetType();
	if ( type == MAPCLASS_TYPE( CMapEntity ) )
	{
		bEntity = bGroups = bFlags = true;
		return ltEntity;
	}
	if ( type == MAPCLASS_TYPE( CMapSolid ) || type == MAPCLASS_TYPE( CMapGroup ) )
	{
		bGroups = true;
		return ltSolid;
	}
	if ( type == MAPCLASS_TYPE( CMapWorld ) )
	{
		bEntity = true;
		return ltWorld;
	}
	return ltNone;
}

//-----------------------------------------------------------------------------
// Purpose: Sets up pages to display based on "m_DstObjects".
// Output : Returns TRUE if the page structure changed, FALSE if not.
//-----------------------------------------------------------------------------
BOOL CObjectProperties::SetupPages( void )
{
	static LayoutType_t eLastLayoutType = ltZero;

	//
	// Determine the appropriate layout for the current object list.
	//
	bool bEntity;
	bool bGroups;
	bool bFlags;

	LayoutType_t eLayoutType = GetLayout( bEntity, bGroups, bFlags );

	//
	// If the layout has not changed, we're done. All the pages are already set up.
	//
	if ( eLayoutType == eLastLayoutType )
		return ( FALSE );

	eLastLayoutType = eLayoutType;

	bRESTRUCTURING = TRUE;

	UINT nAddPages = bEntity + bGroups + bFlags;

	// don't want to change focus .. just pages!
	CWnd* pActiveWnd = GetActiveWindow();

	bool bDisabledraw = false;
	if ( ::IsWindow( m_hWnd ) && IsWindowVisible() )
	{
		SetRedraw( FALSE );
		bDisabledraw = true;
	}

	if ( !m_bDummy && ( nAddPages == 0 ) )
	{
		AddPage( m_pDummy );
		m_bDummy = true;
	}
	else if ( m_bDummy && ( nAddPages > 0 ) )
	{
		RemovePage( m_pDummy );
		m_bDummy = false;
	}

	//
	// Remove all the pages.
	//
	if ( GetPageIndex( m_pEntity ) != -1 )
		RemovePage( m_pEntity );

	if ( GetPageIndex( m_pGroups ) != -1 )
		RemovePage( m_pGroups );

	if ( GetPageIndex( m_pFlags ) != -1 )
		RemovePage( m_pFlags );

	//
	// Add the appropriate pages.
	//
	//
	if ( bEntity )
		AddPage( m_pEntity );

	if ( bFlags )
		AddPage( m_pFlags );

	if ( bGroups )
		AddPage( m_pGroups );

	//
	// Store active pages in our array.
	//
	if ( !m_bDummy )
	{
		delete[] m_ppPages;
		m_nPages = GetPageCount();
		m_ppPages = new CObjectPage*[m_nPages];

		for ( int i = 0; i < m_nPages; i++ )
		{
			m_ppPages[i] = (CObjectPage*)GetPage( i );
			m_ppPages[i]->m_bFirstTimeActive = true;
		}
	}

	bRESTRUCTURING = FALSE;

	SetActivePage( 0 );

	//
	// Add an apply and cancel button once only.
	//
	static bool bCreatedButtons = false;
	if ( IsWindow( m_hWnd ) && !bCreatedButtons )
	{
		CreateButtons();
		bCreatedButtons = true;
	}

	//
	// Enable redraws if they were disabled above.
	//
	if ( bDisabledraw )
	{
		SetRedraw( TRUE );
		Invalidate( FALSE );
	}

	// Set button status

	if ( pActiveWnd != NULL )
		pActiveWnd->SetActiveWindow();

	return TRUE; // pages changed - return true
}



//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectProperties::SaveData(void)
{
	//
	// Make sure window is visible - don't want to save otherwise.
	//
	if (!IsWindowVisible())
	{
		return;
	}

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if (!pDoc || !m_DstObjects.GetCount() || m_bDummy)
	{
		return;
	}

	//
	// Transfer all page data to the objects being edited.
	//
	GetHistory()->MarkUndoPosition(pDoc->Selection_GetList(), "Change Properties");
	GetHistory()->Keep(&m_DstObjects);

	for (int i = 0; i < m_nPages; i++)
	{
		//
		// Pages that have never been shown have no hwnd.
		//
		if (IsWindow(m_ppPages[i]->m_hWnd))
		{
			m_ppPages[i]->SaveData();
		}
	}

	//
	// Objects may have changed. Update the views.
	//
	CMapDoc::GetActiveMapDoc()->Selection_UpdateBounds();
	CMapDoc::GetActiveMapDoc()->UpdateAllViews(NULL, MAPVIEW_UPDATE_2D | MAPVIEW_UPDATE_OBJECTS);
	pDoc->SetModifiedFlag();
}


//-----------------------------------------------------------------------------
// Purpose: Submits the objects to be edited to the property pages so they can
//			update their controls.
// Input  : iPage - Page index or -1 to update all pages.
//-----------------------------------------------------------------------------
void CObjectProperties::LoadDataForPages(int iPage)
{
	if (m_bDummy)
	{
		return;
	}

	//
	// Determine whether we are editing multiple objects or not.
	//
	bool bMultiEdit = (m_DstObjects.GetCount() > 1);

	//
	// Submit the edit objects to each page one at a time.
	//
	int nMode = CObjectPage::LoadFirstData;
	POSITION p = m_DstObjects.GetHeadPosition();
	while (p != NULL)
	{
		CMapClass *pobj = m_DstObjects.GetNext(p);

		if (iPage != -1)
		{
			//
			// Specific page.
			//
			m_ppPages[iPage]->SetMultiEdit(bMultiEdit);

			void *pObject = GetEditObjectFromMapObject(pobj, m_ppPages[iPage]->GetEditObjectRuntimeClass());
			if (pObject != NULL)
			{
				m_ppPages[iPage]->UpdateData(nMode, pObject);
			}
		}
		else for (int i = 0; i < m_nPages; i++)
		{
			//
			// All pages.
			//
			m_ppPages[i]->SetMultiEdit(bMultiEdit);

			if (m_ppPages[i]->m_bFirstTimeActive)
			{
				continue;
			}

			void *pObject = GetEditObjectFromMapObject(pobj, m_ppPages[i]->GetEditObjectRuntimeClass());
			if (pObject != NULL)
			{
				m_ppPages[i]->UpdateData(nMode, pObject);
			}
		}

		nMode = CObjectPage::LoadData;
	}

	//
	// Tell the pages that we are done submitting data.
	//
	if (iPage != -1)
	{
		//
		// Specific page.
		//
		m_ppPages[iPage]->UpdateData(CObjectPage::LoadFinished, NULL);
	}
	else for (int i = 0; i < m_nPages; i++)
	{
		//
		// All pages.
		//
		m_ppPages[i]->UpdateData(CObjectPage::LoadFinished, NULL);
	}

	//
	// Update the input/output icons based on the new data.
	//
}



//-----------------------------------------------------------------------------
// Purpose: Updates the property page data when the selection contents change.
// Input  : pObjects - List of currently selected objects.
//-----------------------------------------------------------------------------
void CObjectProperties::LoadData(CMapObjectList *pObjects, bool bSave)
{
	//
	// Disable window so it does not gain focus during this operation.
	//
	EnableWindow(FALSE);

	if (bSave)
	{
		SaveData();
	}

	m_DstObjects.RemoveAll();
	m_DstObjects.AddTail(pObjects);

	//
	// If there is only one object selected, copy its data to our temporary
	// edit objects.
	//
	if (m_DstObjects.GetCount() == 1)
	{
		//
		// Copy the single destination object's data to our temporary
		// edit objects.
		//
		POSITION p = m_DstObjects.GetHeadPosition();
		CMapClass *pobj = m_DstObjects.GetAt(p);
		CopyDataToEditObjects(pobj);
	}

	//
	// Only call LoadDataForPages() if page structure hasn't changed
	// (SetupPages() returns 0) - otherwise, might be trying to update
	// pages with no hWnd since RemovePage() destroys a page's hWnd.
	//
	if (!SetupPages())
	{
		LoadDataForPages();
	}

	EnableWindow(TRUE);
}


BOOL CObjectProperties::OnInitDialog()
{
	BOOL b = CPropertySheet::OnInitDialog();
	SetWindowText("Object Properties");
	return b;
}


//-----------------------------------------------------------------------------
// Purpose: Closes the object properties dialog, saving changes.
//-----------------------------------------------------------------------------
void CObjectProperties::OnClose(void)
{
	SaveData();
	ShowWindow(SW_HIDE);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectProperties::AbortData()
{
	m_DstObjects.RemoveAll();
	SetupPages();
}


void CObjectProperties::DontEdit(CMapClass *pObject)
{
	POSITION p = m_DstObjects.Find(pObject);
	if(!p)
		return;
	m_DstObjects.RemoveAt(p);
	SetupPages();
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : bShow -
//			nStatus -
//-----------------------------------------------------------------------------
void CObjectProperties::OnShowWindow( BOOL bShow, UINT nStatus )
{
	CPropertySheet::OnShowWindow( bShow, nStatus );

	for ( int i = 0; i < m_nPages; i++ )
		m_ppPages[i]->OnShowPropertySheet( bShow, nStatus );
}

IMPLEMENT_DYNCREATE(CObjectPage, CPropertyPage)


//-----------------------------------------------------------------------------
// Purpose: Called when we become the active page.
//-----------------------------------------------------------------------------
BOOL CObjectPage::OnSetActive(void)
{
	if (bRESTRUCTURING || !GetActiveWorld())
	{
		return CPropertyPage::OnSetActive();
	}

	CObjectProperties *pParent = (CObjectProperties *)GetParent();

	if (m_bFirstTimeActive)
	{
		m_bFirstTimeActive = false;
		pParent->LoadDataForPages(pParent->GetPageIndex(this));
	}

	return CPropertyPage::OnSetActive();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
PVOID CObjectPage::GetEditObject()
{
	return ((CObjectProperties*) GetParent())->GetEditObject(GetEditObjectRuntimeClass());
}


//-----------------------------------------------------------------------------
// Purpose: Handles the Apply button.
//-----------------------------------------------------------------------------
void CObjectProperties::OnApply(void)
{
	for (int i = 0; i < m_nPages; i++)
	{
		if (!m_ppPages[i]->OnApply())
		{
			return;
		}
	}

	//
	// Save and reload the data so the GUI updates.
	//
	SaveData();

	CMapObjectList TempList;
	TempList.AddTail(&m_DstObjects);
	LoadData(&TempList, false);
}


//-----------------------------------------------------------------------------
// Purpose: Handles the Apply button.
//-----------------------------------------------------------------------------
void CObjectProperties::OnCancel(void)
{
	ShowWindow(SW_HIDE);

	//
	// Deselect the current selection, then reselect it to refresh the dialog contents.
	//
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	CMapObjectList OldSelection;
	OldSelection.AddTail(pDoc->Selection_GetList());
	pDoc->SelectObjectList(&OldSelection);
}






//-----------------------------------------------------------------------------
// Purpose:
// Input  : lpCreateStruct -
// Output : int
//-----------------------------------------------------------------------------
int CObjectProperties::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	lpCreateStruct->dwExStyle |= WS_EX_TOOLWINDOW;

	if (CPropertySheet::OnCreate(lpCreateStruct) == -1)
	{
		return -1;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CObjectProperties::RefreshData(void)
{
	for (int i = 0; i < m_nPages; i++)
	{
		if (m_ppPages[i]->m_hWnd)
		{
			m_ppPages[i]->RememberState();
		}
	}

	CMapObjectList TempList;
	TempList.AddTail(&m_DstObjects);
	LoadData(&TempList, false);
}
