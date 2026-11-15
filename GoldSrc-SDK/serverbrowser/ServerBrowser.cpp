//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server browser module; exposes IServerBrowser and IVGuiModule
//
// $NoKeywords: $
//=============================================================================

#include "ServerBrowser.h"
#include "ServerBrowserDialog.h"
#include "DialogGameInfo.h"

#include "GameUI/common/IRunGameEngine.h"
#include "../engine/controls.h"
#include "vgui_controls/Controls.h"
#include "tier0/dbg.h"
#include "../external/steamworks/isteamapps.h"

S_API bool S_CALLTYPE SteamAPI_Init();

IRunGameEngine *g_pRunGameEngine = NULL;

CServerBrowser g_ServerBrowserSingleton;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CServerBrowser, IServerBrowser, SERVERBROWSER_INTERFACE_VERSION, g_ServerBrowserSingleton );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CServerBrowser, IVGuiModule, "VGuiModuleServerBrowser001", g_ServerBrowserSingleton );

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CServerBrowser &ServerBrowser()
{
	return g_ServerBrowserSingleton;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerBrowser::CServerBrowser()
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerBrowser::~CServerBrowser()
{
}

//-----------------------------------------------------------------------------
// Purpose: Creates the server browser dialog if it doesn't exist yet
//-----------------------------------------------------------------------------
void CServerBrowser::CreateDialog()
{
	if ( !m_hInternetDlg.Get() )
	{
		m_hInternetDlg = new CServerBrowserDialog( NULL );
		m_hInternetDlg->Initialize();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Initializes Steam, vgui and the engine interface
//-----------------------------------------------------------------------------
bool CServerBrowser::Initialize( CreateInterfaceFn *factorylist, int factoryCount )
{
	g_pRunGameEngine = NULL;

	if ( !SteamAPI_Init() )
	{
		Error( "Unable to initialize interface\n" );
		return false;
	}

	IRunGameEngine *pRunGameEngine = g_pRunGameEngine;
	for ( int i = 0; i < factoryCount; i++ )
	{
		if ( !pRunGameEngine )
		{
			pRunGameEngine = (IRunGameEngine *)factorylist[i]( RUNGAMEENGINE_INTERFACE_VERSION, NULL );
			g_pRunGameEngine = pRunGameEngine;
		}
	}

	// load the vgui interfaces
	if ( !vgui2::VGuiControls_Init( "ServerBrowser", factorylist, factoryCount ) )
		return false;

	// load localization file
	return vgui2::localize()->AddFile( vgui2::filesystem(), "servers/serverbrowser_%language%.txt" );
}

//-----------------------------------------------------------------------------
// Purpose: Finds the engine interface and creates the (hidden) dialog
//-----------------------------------------------------------------------------
bool CServerBrowser::PostInitialize( CreateInterfaceFn *modules, int factoryCount )
{
	IRunGameEngine *pRunGameEngine = g_pRunGameEngine;
	for ( int i = 0; i < factoryCount; i++ )
	{
		if ( !pRunGameEngine )
		{
			pRunGameEngine = (IRunGameEngine *)modules[i]( RUNGAMEENGINE_INTERFACE_VERSION, NULL );
			g_pRunGameEngine = pRunGameEngine;
		}
	}

	// the dialog is created hidden; it is shown on Activate()
	CreateDialog();
	m_hInternetDlg->SetVisible( false );

	return g_pRunGameEngine != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: true if the user can't play a game due to VAC banning
//-----------------------------------------------------------------------------
bool CServerBrowser::IsVACBannedFromGame( int nAppID )
{
	if ( !SteamApps() )
		return false;

	return SteamApps()->BIsVACBanned();
}

//-----------------------------------------------------------------------------
// Purpose: true if the module has found the engine interface
//-----------------------------------------------------------------------------
bool CServerBrowser::IsValid()
{
	return g_pRunGameEngine != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Activates the server browser window, loading the saved settings
//			the first time it is opened
//-----------------------------------------------------------------------------
bool CServerBrowser::Activate()
{
	static bool firstTimeOpening = true;
	if ( firstTimeOpening )
	{
		m_hInternetDlg->LoadUserData();
		firstTimeOpening = false;
	}

	Open();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Saves the user's settings when the module is deactivated
//-----------------------------------------------------------------------------
void CServerBrowser::Deactivate()
{
	if ( m_hInternetDlg.Get() )
	{
		m_hInternetDlg->SaveUserData();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reloads the user's settings and refreshes the visible page
//-----------------------------------------------------------------------------
void CServerBrowser::Reactivate()
{
	if ( m_hInternetDlg.Get() )
	{
		m_hInternetDlg->LoadUserData();
		if ( m_hInternetDlg->IsVisible() )
		{
			m_hInternetDlg->RefreshCurrentPage();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Opens the server browser dialog
//-----------------------------------------------------------------------------
void CServerBrowser::Open()
{
	m_hInternetDlg->Open();
}

//-----------------------------------------------------------------------------
// Purpose: returns direct handle to main server browser dialog
//-----------------------------------------------------------------------------
vgui2::VPANEL CServerBrowser::GetPanel()
{
	if ( !m_hInternetDlg.Get() )
		return 0;

	return m_hInternetDlg->GetVPanel();
}

//-----------------------------------------------------------------------------
// Purpose: sets the parent panel of the main module panel
//-----------------------------------------------------------------------------
void CServerBrowser::SetParent( vgui2::VPANEL parent )
{
	if ( m_hInternetDlg.Get() )
	{
		m_hInternetDlg->SetParent( parent );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Closes down the server browser for good
//-----------------------------------------------------------------------------
void CServerBrowser::Shutdown()
{
	if ( m_hInternetDlg.Get() )
	{
		m_hInternetDlg->Close();
		m_hInternetDlg->MarkForDeletion();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Runs the Steam callbacks once per frame
//-----------------------------------------------------------------------------
void CServerBrowser::RunFrame()
{
	SteamAPI_RunCallbacks();
}

//-----------------------------------------------------------------------------
// Purpose: opens a game info dialog for the game a friend is playing in
//-----------------------------------------------------------------------------
bool CServerBrowser::OpenGameInfoDialog( uint64 ulSteamIDFriend )
{
	if ( !m_hInternetDlg.Get() )
		return false;

	// activate an already-open dialog
	CDialogGameInfo *pDialogGameInfo = m_hInternetDlg->GetDialogGameInfoForFriend( ulSteamIDFriend );
	if ( pDialogGameInfo )
	{
		pDialogGameInfo->Activate();
		return true;
	}

	// none yet, create a new dialog
	FriendGameInfo_t friendGameInfo;
	if ( SteamFriends()->GetFriendGamePlayed( CSteamID( ulSteamIDFriend ), &friendGameInfo ) )
	{
		uint16 usQueryPort = friendGameInfo.m_usQueryPort;
		uint16 usConnPort = friendGameInfo.m_usGamePort;
		int iServerIP = (int)friendGameInfo.m_unGameIP;

		pDialogGameInfo = m_hInternetDlg->OpenGameInfoDialog( iServerIP, usConnPort, usQueryPort );
		pDialogGameInfo->SetFriend( ulSteamIDFriend );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: joins a specified game - game info dialog will only be opened if the server is full or passworded
//-----------------------------------------------------------------------------
bool CServerBrowser::JoinGame( uint64 ulSteamIDFriend )
{
	if ( OpenGameInfoDialog( ulSteamIDFriend ) )
	{
		CDialogGameInfo *pDialogGameInfo = m_hInternetDlg->GetDialogGameInfoForFriend( ulSteamIDFriend );
		pDialogGameInfo->Connect();
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: joins a game by IP/port
//-----------------------------------------------------------------------------
bool CServerBrowser::JoinGame( uint32 unGameIP, uint16 usGamePort )
{
	m_hInternetDlg->JoinGame( (int)unGameIP, (int)usGamePort );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: forces the game info dialog closed
//-----------------------------------------------------------------------------
void CServerBrowser::CloseGameInfoDialog( uint64 ulSteamIDFriend )
{
	CDialogGameInfo *pDialogGameInfo = m_hInternetDlg->GetDialogGameInfoForFriend( ulSteamIDFriend );
	if ( pDialogGameInfo )
	{
		pDialogGameInfo->Close();
	}
}

//-----------------------------------------------------------------------------
// Purpose: closes all the game info dialogs
//-----------------------------------------------------------------------------
void CServerBrowser::CloseAllGameInfoDialogs()
{
	if ( m_hInternetDlg.Get() )
	{
		m_hInternetDlg->CloseAllGameInfoDialogs();
	}
}
