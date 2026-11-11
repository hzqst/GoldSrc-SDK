//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include <shlobj.h>
#include "EditGameConfigs.h"
#include "worldcraft.h"
#include "OPTConfigs.h"
#include "process.h"
#include "Options.h"
#include "TextureBrowser.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNCREATE( COPTConfigs, CPropertyPage )

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
COPTConfigs::COPTConfigs(void) : CPropertyPage(COPTConfigs::IDD)
{
	//{{AFX_DATA_INIT(COPTConfigs)
	//}}AFX_DATA_INIT

	m_pLastSelConfig = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
COPTConfigs::~COPTConfigs(void)
{
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pDX -
//-----------------------------------------------------------------------------
void COPTConfigs::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COPTConfigs)
	DDX_Control(pDX, IDC_MAPDIR, m_cMapDir);
	DDX_Control(pDX, IDC_GAMEEXEDIR, m_cGameExeDir);
	DDX_Control(pDX, IDC_MODDIR, m_cModDir);
	DDX_Control(pDX, IDC_GAMEDIR, m_cGameDir);
	DDX_Control(pDX, IDC_MAPFORMAT, m_cMapFormat);
	DDX_Control(pDX, IDC_TEXTUREFORMAT, m_cTextureFormat);
	DDX_Control(pDX, IDC_DEFAULTPOINT, m_cDefaultPoint);
	DDX_Control(pDX, IDC_DEFAULTENTITY, m_cDefaultSolid);
	DDX_Control(pDX, IDC_DATAFILES, m_cGDFiles);
	DDX_Control(pDX, IDC_CONFIGURATIONS, m_cConfigs);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP( COPTConfigs, CPropertyPage )
//{{AFX_MSG_MAP(COPTConfigs)
ON_BN_CLICKED( IDC_EDITCONFIGS, OnEditconfigs )
ON_BN_CLICKED( IDC_GDFILE_ADD, OnGdfileAdd )
ON_BN_CLICKED( IDC_GDFILE_EDIT, OnGdfileEdit )
ON_BN_CLICKED( IDC_GDFILE_REMOVE, OnGdfileRemove )
ON_CBN_SELCHANGE( IDC_CONFIGURATIONS, OnSelchangeConfigurations )
ON_BN_CLICKED( IDC_BROWSEMAPDIR, OnBrowsemapdir )
ON_BN_CLICKED( IDC_BROWSEGAMEDIR, OnBrowseGameDir )
ON_BN_CLICKED( IDC_BROWSEGAMEEXEDIR, OnBrowseGameExeDir )
ON_BN_CLICKED( IDC_BROWSEMODDIR, OnBrowseModDir )
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void COPTConfigs::OnEditconfigs( void )
{
	SaveInfo(m_pLastSelConfig);

	CEditGameConfigs dlg;
	dlg.DoModal();
	UpdateConfigList();
}

void COPTConfigs::OnGdfileAdd(void)
{
	if(m_pLastSelConfig == NULL)
		return;

	// browse for .FGD files
	CFileDialog dlg( TRUE, ".fgd", NULL, OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ENABLESIZING,
		"Game Data Files (*.fgd)|*.fgd||", NULL, 0, FALSE );
	dlg.m_ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;

	if(dlg.DoModal() != IDOK)
		return;

	m_cGDFiles.AddString(dlg.GetPathName());

	SaveInfo(m_pLastSelConfig);
	m_pLastSelConfig->LoadGDFiles();
	UpdateEntityLists();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COPTConfigs::OnGdfileEdit(void)
{
	if(m_pLastSelConfig == NULL)
		return;

	// edit the selected FGD file
	int iCurSel = m_cGDFiles.GetCurSel();
	if(iCurSel == LB_ERR)
		return;

	CString str;
	m_cGDFiles.GetText(iCurSel, str);

	// call editor (notepad!)
	char szBuf[MAX_PATH];
	GetWindowsDirectory(szBuf, MAX_PATH);
	strcat(szBuf, "\\notepad.exe");
	_spawnl(_P_WAIT, szBuf, szBuf, str, NULL);

	m_pLastSelConfig->LoadGDFiles();
	UpdateEntityLists();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COPTConfigs::OnGdfileRemove(void)
{
	if(m_pLastSelConfig == NULL)
		return;

	int iCurSel = m_cGDFiles.GetCurSel();
	if(iCurSel == LB_ERR)
		return;

	m_cGDFiles.DeleteString(iCurSel);

	SaveInfo(m_pLastSelConfig);
	m_pLastSelConfig->LoadGDFiles();
	UpdateEntityLists();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pConfig -
//-----------------------------------------------------------------------------
void COPTConfigs::SaveInfo(CGameConfig *pConfig)
{
	if (pConfig == NULL)
	{
		return;
	}

	// game data files
	pConfig->nGDFiles = 0;
	pConfig->GDFiles.RemoveAll();
	int iSize = m_cGDFiles.GetCount();
	for(int i = 0; i < iSize; i++)
	{
		CString str;
		m_cGDFiles.GetText(i, str);
		pConfig->GDFiles.Add(str);
		++pConfig->nGDFiles;
	}

	//
	// Map file type.
	//
	int nIndex = m_cMapFormat.GetCurSel();
	if (nIndex != CB_ERR)
	{
		pConfig->mapformat = (MAPFORMAT)m_cMapFormat.GetItemData(nIndex);
	}
	else
	{
		pConfig->mapformat = mfHalfLife;
	}

	//
	// WAD file type.
	//
	nIndex = m_cTextureFormat.GetCurSel();
	if (nIndex != CB_ERR)
	{
		pConfig->SetTextureFormat((TEXTUREFORMAT)m_cTextureFormat.GetItemData(nIndex));
	}
	else
	{
		pConfig->SetTextureFormat( tfWAD3 );
	}

	m_cDefaultSolid.GetWindowText(pConfig->szDefaultSolid, sizeof pConfig->szDefaultSolid);
	m_cDefaultPoint.GetWindowText(pConfig->szDefaultPoint, sizeof pConfig->szDefaultPoint);
	m_cGameExeDir.GetWindowText(pConfig->m_szGameExeDir, sizeof(pConfig->m_szGameExeDir));
	m_cModDir.GetWindowText(pConfig->m_szModDir, sizeof(pConfig->m_szModDir));
	m_cGameDir.GetWindowText(pConfig->m_szGameDir, sizeof(pConfig->m_szGameDir));
	m_cMapDir.GetWindowText(pConfig->szMapDir, sizeof pConfig->szMapDir);


}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COPTConfigs::OnSelchangeConfigurations( void )
{
	// save info from controls into last selected config
	SaveInfo(m_pLastSelConfig);

	m_pLastSelConfig = NULL;

	// load info from newly selected config into controls
	int iCurSel = m_cConfigs.GetCurSel();
	CGameConfig *pConfig = Options.configs.FindConfig(m_cConfigs.GetItemData(iCurSel));

	BOOL bKillFields = FALSE;
	if (pConfig == NULL)
	{
		bKillFields = TRUE;
	}

	m_cGDFiles.EnableWindow(!bKillFields);
	m_cDefaultPoint.EnableWindow(!bKillFields);
	m_cDefaultSolid.EnableWindow(!bKillFields);
	m_cTextureFormat.EnableWindow(!bKillFields);
	m_cMapFormat.EnableWindow(!bKillFields);
	m_cGameExeDir.EnableWindow(!bKillFields);
	m_cGameDir.EnableWindow(!bKillFields);
	m_cModDir.EnableWindow(!bKillFields);
	m_cMapDir.EnableWindow(!bKillFields);

	if (pConfig == NULL)
	{
		return;
	}

	m_pLastSelConfig = pConfig;

	// load game data files
	m_cGDFiles.ResetContent();
	int i;
	for (i = 0; i < pConfig->nGDFiles; i++)
	{
		m_cGDFiles.AddString(pConfig->GDFiles[i]);
	}

	//
	// Select the correct map format.
	//
	m_cMapFormat.SetCurSel(0);
	int nItems = m_cMapFormat.GetCount();
	for (i = 0; i < nItems; i++)
	{
		if (pConfig->mapformat == (MAPFORMAT)m_cMapFormat.GetItemData(i))
		{
			m_cMapFormat.SetCurSel(i);
			break;
		}
	}

	//
	// Select the correct texture format.
	//
	m_cTextureFormat.SetCurSel(0);
	nItems = m_cTextureFormat.GetCount();
	for (i = 0; i < nItems; i++)
	{
		if (pConfig->GetTextureFormat() == (TEXTUREFORMAT)m_cTextureFormat.GetItemData(i))
		{
			m_cTextureFormat.SetCurSel(i);
			break;
		}
	}

	m_cGameExeDir.SetWindowText(pConfig->m_szGameExeDir);
	m_cModDir.SetWindowText(pConfig->m_szModDir);
	m_cGameDir.SetWindowText(pConfig->m_szGameDir);
	m_cMapDir.SetWindowText(pConfig->szMapDir);

	// load entity type comboboxes and set strings
	UpdateEntityLists();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COPTConfigs::UpdateEntityLists(void)
{
	if(m_pLastSelConfig == NULL)
		return;

	m_cDefaultPoint.ResetContent();
	m_cDefaultSolid.ResetContent();

	CGameConfig *pConfig = m_pLastSelConfig;

	POSITION p = pConfig->GD.Classes.GetHeadPosition();
	while(p)
	{
		GDclass *pClass = pConfig->GD.Classes.GetNext(p);
		if (pClass->IsBaseClass())
		{
			continue;
		}
		else if (pClass->IsSolidClass())
		{
			m_cDefaultSolid.AddString(pClass->GetName());
		}
		else
		{
			m_cDefaultPoint.AddString(pClass->GetName());
		}
	}

	if (pConfig->szDefaultSolid[0] != '\0')
	{
		m_cDefaultSolid.SetWindowText(pConfig->szDefaultSolid);
	}
	else
	{
		m_cDefaultSolid.SetCurSel(0);
	}

	if ( pConfig->szDefaultPoint[0] != '\0' )
	{
		m_cDefaultPoint.SetWindowText(pConfig->szDefaultPoint);
	}
	else
	{
		m_cDefaultPoint.SetCurSel(0);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COPTConfigs::UpdateConfigList(void)
{
	m_pLastSelConfig = NULL;

	int iCurSel = m_cConfigs.GetCurSel();
	DWORD dwSelID = 0xffffffff;
	if (iCurSel != CB_ERR)
	{
		dwSelID = m_cConfigs.GetItemData(iCurSel);
	}

	m_cConfigs.ResetContent();

	iCurSel = -1;	// reset it for check below

	// add configs to the combo box
	for (int i = 0; i < Options.configs.nConfigs; i++)
	{
		CGameConfig *pConfig = Options.configs.Configs[i];

		int iIndex = m_cConfigs.AddString(pConfig->szName);
		m_cConfigs.SetItemData(iIndex, pConfig->dwID);

		// found same config as was already selected - make sure
		//  it stays selected
		if(dwSelID != 0xffffffff && dwSelID == pConfig->dwID)
			iCurSel = iIndex;
	}

	m_cConfigs.SetCurSel(iCurSel == -1 ? 0 : iCurSel);

	OnSelchangeConfigurations();

	SetModified();
}

//-----------------------------------------------------------------------------
// Purpose: Populates the combo boxes with the map formats and WAD formats, and
//			the list of game configurations.
//-----------------------------------------------------------------------------
BOOL COPTConfigs::OnInitDialog( void )
{
	CPropertyPage::OnInitDialog();

	int nIndex;

	//
	// Add map formats.
	//
	nIndex = m_cMapFormat.AddString( "HalfLife / TFC" );
	m_cMapFormat.SetItemData(nIndex, mfHalfLife);

	//
	// Add WAD formats.
	//
	nIndex = m_cTextureFormat.AddString( "WAD3 (HalfLife / TFC)" );
	m_cTextureFormat.SetItemData( nIndex, tfWAD3 );

	UpdateConfigList();

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL COPTConfigs::OnApply(void)
{
	SaveInfo(m_pLastSelConfig);

	Options.PerformChanges(COptions::secConfigs);

	return CPropertyPage::OnApply();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pszTitle -
//			*pszDirectory -
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL COPTConfigs::BrowseForFolder(char *pszTitle, char *pszDirectory)
{
	char szTmp[MAX_PATH];

	BROWSEINFO bi;
	memset(&bi, 0, sizeof bi);
	bi.hwndOwner = m_hWnd;
	bi.pszDisplayName = szTmp;
	bi.lpszTitle = pszTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS;

	LPITEMIDLIST idl = SHBrowseForFolder(&bi);

	if(idl == NULL)
		return FALSE;

	SHGetPathFromIDList(idl, pszDirectory);
	CoTaskMemFree(idl);

	return TRUE;
}

// Purpose:


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COPTConfigs::OnBrowseGameDir(void)
{
	char szTmp[MAX_PATH];
	if (!BrowseForFolder("Select Game Directory", szTmp))
	{
		return;
	}
	m_cGameDir.SetWindowText(szTmp);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COPTConfigs::OnBrowseGameExeDir(void)
{
	char szTmp[MAX_PATH];
	if (!BrowseForFolder("Select Game Executable Directory", szTmp))
	{
		return;
	}
	m_cGameExeDir.SetWindowText(szTmp);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COPTConfigs::OnBrowseModDir(void)
{
	char szTmp[MAX_PATH];
	if (!BrowseForFolder("Select Mod Directory", szTmp))
	{
		return;
	}
	m_cModDir.SetWindowText(szTmp);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COPTConfigs::OnBrowsemapdir(void)
{
	char szTmp[MAX_PATH];
	if(!BrowseForFolder("Select Map Directory", szTmp))
		return;
	m_cMapDir.SetWindowText(szTmp);
}
