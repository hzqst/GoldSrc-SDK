//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Implements all the functions exported by the GameUI dll
//
// $NoKeywords: $
//=============================================================================

#include "GameUI_Interface.h"
#include "BasePanel.h"
#include "CareerGame.h"
#include "LoadingDialog.h"
#include "ModInfo.h"
#include "Sys_Utils.h"
#include "Taskbar.h"
#include "VGuiSystemModuleLoader.h"
#include "common/ienginevgui.h"

#include "IBaseUI.h"
#include "igameuifuncs.h"
#include "cl_dll/IGameClientExports.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "../engine/controls.h"
#include "wrect.h"
#include "cl_dll.h"
#include "FileSystem.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include "TestScriptController.h"
#endif

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

cl_enginefunc_t gEngfuncs;
cl_enginefuncs_s* engine;
CTaskbar* g_pTaskbar;
CGameUI* g_pGameUI;
IEngineVGui* g_pEngineVGUI;
IGameClientExports* g_pGameClientExports;
IFriendsUser* g_pFriendsUser;
IBaseSystem* g_pSystemWrapper;
IBaseUI* baseuifuncs;
IGameUIFuncs* gameuifuncs;
vgui2::ISurface* enginesurfacefuncs;
static CBasePanel* staticPanel;
vgui2::DHANDLE<CLoadingDialog> g_hLoadingDialog;

CGameUI g_GameUI;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameUI, IGameUI, "GameUI007", g_GameUI );

IEngineVGui* EngineVGUI()
{
	return g_pEngineVGUI;
}

#ifndef _WIN32
void RunVguiTestScript()
{
}

void CGameUI::RunVguiTestScript( const char* filename )
{
	if ( !m_hTestScriptController.Get() )
		m_hTestScriptController = new vgui2::TestScriptController;
	m_hTestScriptController.Get()->ExecuteScript( filename );
}
#endif

#ifdef _WIN32
static WHANDLE g_hMutex;
static WHANDLE g_hWaitMutex;

static int __stdcall EnumFunc( WHANDLE window, int )
{
	unsigned int message = Sys_RegisterWindowMessage( "ShutdownValvePlatform" );
	Sys_PostMessage( window, message, 0, 1 );
	return 1;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CGameUI* GameUI()
{
	return &g_GameUI;
}

IGameClientExports* GameClientExports()
{
	return g_pGameClientExports;
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameUI::CGameUI()
{
	m_iGameIP = 0;
	m_bTryingToLoadTracker = false;
	g_pGameUI = this;
	m_iGamePort = 0;
	m_flProgressStartTime = 0;
	m_pszCurrentProgressType = "";
	m_bActivatedUI = false;
	m_bIsRunningOnDeck = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameUI::~CGameUI()
{
	g_pGameUI = NULL;
}

void CGameUI::Initialize( CreateInterfaceFn* factories, int count )
{
//	TRACE_FUNCTION("CGameUI::Initialize");
	CreateInterfaceFn engineFactory = factories[0];
	CreateInterfaceFn clientFactory = count > 5 ? factories[5] : NULL;
	m_iNumFactories = count;
	m_FactoryList[0] = Sys_GetFactoryThis();
	m_FactoryList[1] = engineFactory;
	m_FactoryList[2] = factories[1];
	m_FactoryList[3] = factories[2];
	m_FactoryList[4] = clientFactory;
	vgui2::VGuiControls_Init( "GameUI", m_FactoryList, count );
	vgui2::localize()->AddFile( vgui2::filesystem(), "resource/gameui_%language%.txt" );
	staticPanel = new CBasePanel;
	staticPanel->SetBounds( 0, 0, 400, 300 );
	staticPanel->SetPaintBorderEnabled( false );
	staticPanel->SetPaintBackgroundEnabled( true );
	staticPanel->SetPaintEnabled( false );
	staticPanel->SetVisible( true );
	staticPanel->SetMouseInputEnabled( false );
	staticPanel->SetKeyBoardInputEnabled( false );
	g_pEngineVGUI = (IEngineVGui*)engineFactory( "VEngineVGui001", NULL );
	if ( g_pEngineVGUI )
		staticPanel->SetParent( g_pEngineVGUI->GetPanel( PANEL_GAMEUIDLL ) );
	gameuifuncs = (IGameUIFuncs*)engineFactory( "VENGINE_GAMEUIFUNCS_VERSION001", NULL );
	enginesurfacefuncs = (vgui2::ISurface*)engineFactory( "VGUI_Surface026", NULL );
	baseuifuncs = (IBaseUI*)engineFactory( "BaseUI001", NULL );
	if ( clientFactory )
		g_pGameClientExports = (IGameClientExports*)clientFactory( "GameClientExports001", NULL );
	m_flRetryPlatformReleaseMessageTime = 0;
}

void CGameUI::Start( cl_enginefuncs_s* engineFunctions, int, IBaseSystem* system )
{
//	TRACE_FUNCTION("CGameUI::Start");
//	m_pMaster = NULL;
	m_pMaster = NULL;
	memcpy( &gEngfuncs, engineFunctions, sizeof( gEngfuncs ) );
	g_pSystemWrapper = system;
	engine = &gEngfuncs;
	ModInfo()->LoadCurrentGameInfo();
	strncpy( m_szPlatformDir, "platform", sizeof( m_szPlatformDir ) );
	// setup config file directory
	char configDir[512];
	strcpy( configDir, m_szPlatformDir );
#ifdef _WIN32
	strcat( configDir, "\\" );
#else
	strcat( configDir, "/" );
#endif
	strcat( configDir, "config" );
	vgui2::filesystem()->AddSearchPath( configDir, "CONFIG" );
#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning( suppress: 6031 )
#endif
	_mkdir( configDir );
#else
	mkdir( configDir, 0777 );
#endif
	vgui2::ivgui()->DPrintf( "Platform config directory: %s\n", configDir );
	vgui2::system()->SetUserConfigFile( "InGameDialogConfig.vdf", "CONFIG" );
	vgui2::localize()->AddFile( vgui2::filesystem(), "resource/platform_%language%.txt" );
	vgui2::localize()->AddFile( vgui2::filesystem(), "resource/vgui_%language%.txt" );
	// task bar - needs to be first thing created
	g_pTaskbar = new CTaskbar( staticPanel, "TaskBar" );
	g_pTaskbar->SetVisible( false );
// FOR SRC
//	vgui::surface()->SetWorkspaceInsets( 0, 0, 0, g_pTaskbar->GetTall() );

	// Start loading tracker
	if ( m_szPlatformDir[0] )
	{
		vgui2::ivgui()->DPrintf2( "Initializing platform...\n" );
		// open a mutex
		Sys_SetLastError( 0 );
#ifdef _WIN32
		// primary mutex is the platform.exe name
		char platformPath[sizeof( m_szPlatformDir ) + 32];
		sprintf( platformPath, "%splatform.exe", m_szPlatformDir );
		for ( char* cursor = platformPath; *cursor; ++cursor )
		{
			*cursor = (char)tolower( *cursor );
			if ( *cursor == '\\' )
				*cursor = '/';
		}
		g_hMutex = Sys_CreateMutex( "ValvePlatformUIMutex" );
		g_hWaitMutex = Sys_CreateMutex( "ValvePlatformWaitMutex" );
		if ( g_hMutex && g_hWaitMutex && Sys_GetLastError() != ERROR_INVALID_HANDLE )
		{
			unsigned int result = Sys_WaitForSingleObject( g_hMutex, 0 );
			if ( result != WAIT_OBJECT_0 && result != WAIT_ABANDONED )
			{
				result = Sys_WaitForSingleObject( g_hWaitMutex, 0 );
				if ( result == WAIT_OBJECT_0 || result == WAIT_ABANDONED )
					Sys_EnumWindows( (void*)EnumFunc, 1 );
				m_flRetryPlatformReleaseMessageTime = (float)( vgui2::system()->GetCurrentTime() + 2.0 );
			}
			m_bTryingToLoadTracker = true;
		}
		else
		{
			if ( g_hMutex )
				Sys_ReleaseMutex( g_hMutex );
			if ( g_hWaitMutex )
				Sys_ReleaseMutex( g_hWaitMutex );
			g_hMutex = NULL;
			g_hWaitMutex = NULL;
			vgui2::ivgui()->DPrintf2( "Tracker Error: Could not access Tracker, bad mutex\n" );
			return;
		}
#else
		m_bTryingToLoadTracker = true;
#endif
	// now we are set up to check every frame to see if we can Start tracker
	}
	staticPanel->SetBackgroundRenderState( CBasePanel::BACKGROUND_DESKTOPIMAGE );
	engine->pfnClientCmd( "mp3 loop media/gamestartup.mp3\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Called to Shutdown the game UI system
//-----------------------------------------------------------------------------
void CGameUI::Shutdown()
{
	// notify all the modules of Shutdown
	g_VModuleLoader.ShutdownPlatformModules();
	// unload the modules them from memory
	g_VModuleLoader.UnloadPlatformModules();
	// free mod info
	ModInfo()->FreeModInfo();
#ifdef _WIN32
	if ( g_hMutex )
		Sys_ReleaseMutex( g_hMutex );
	if ( g_hWaitMutex )
		Sys_ReleaseMutex( g_hWaitMutex );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: paints all the vgui elements
//-----------------------------------------------------------------------------
void CGameUI::RunFrame()
{
	// resize the background panel to the screen size
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	staticPanel->SetSize( wide, tall );
	// Run frames
	g_VModuleLoader.RunFrame();
	if ( g_pTaskbar )
		g_pTaskbar->RunFrame();
	bool loadPlatform = m_bTryingToLoadTracker;
#ifdef _WIN32
	loadPlatform = loadPlatform && g_hMutex && g_hWaitMutex;
	if ( loadPlatform )
	{
		// try and load tracker
		unsigned int result = Sys_WaitForSingleObject( g_hMutex, 0 );
		loadPlatform = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
		if ( !loadPlatform && vgui2::system()->GetFrameTime() > m_flRetryPlatformReleaseMessageTime )
		{
			Sys_EnumWindows( (void*)EnumFunc, 1 );
			m_flRetryPlatformReleaseMessageTime = (float)( vgui2::system()->GetFrameTime() + 5.0 );
		}
	}
#endif
	if ( loadPlatform )
	{
		m_bTryingToLoadTracker = false;
		m_flRetryPlatformReleaseMessageTime = 0;
		g_VModuleLoader.LoadPlatformModules( m_FactoryList, m_iNumFactories, false );
		// get our own interfaces
		for ( int i = 0; i < g_VModuleLoader.GetModuleCount(); ++i )
		{
			if ( !g_pFriendsUser && g_VModuleLoader.GetModuleFactory( i ) )
			{
				CreateInterfaceFn factory = g_VModuleLoader.GetModuleFactory( i );
				g_pFriendsUser = (IFriendsUser*)factory( "FriendsUser001", NULL );
			}
		}
#ifdef _WIN32
		Sys_ReleaseMutex( g_hWaitMutex );
#else
		Sys_ReleaseMutex( 0 );
#endif
		KeyValues* message = new KeyValues( "ActiveGameName" );
		message->SetString( "name", engine->pfnGetGameDirectory() );
		message->SetInt( "type", 1 );
		message->SetInt( "appid", engine->pfnGetAppID() );
		message->SetString( "game", ModInfo()->GetGameDescription() );
		g_VModuleLoader.PostMessageToAllModules( message );
		if ( m_iGameIP )
		{
			message = new KeyValues( "ConnectedToGame" );
			message->SetInt( "ip", m_iGameIP );
			message->SetInt( "connectionport", m_iGamePort );
			message->SetInt( "queryport", m_iGamePort );
			g_VModuleLoader.PostMessageToAllModules( message );
		}
	}
	if ( vgui2::surface()->GetModalPanel() )
		vgui2::surface()->PaintTraverse( staticPanel->GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: Activate the game UI
//-----------------------------------------------------------------------------
int CGameUI::ActivateGameUI()
{
	if ( IsGameUIActive() )
		return 1;
	m_bActivatedUI = true;
//	TRACE_FUNCTION("CGameUI::ActivateGameUI");

	// hide/show the main panel to Activate all game ui
	staticPanel->SetVisible( true );
	g_pTaskbar->SetVisible( true );
	engine->pfnClientCmd( "inputset_menu" );
	engine->pfnClientCmd( "setpause" );
	if ( g_pTaskbar )
		g_pTaskbar->OnGameUIActivated();
	engine->pfnClientCmd( "richpresence_update\n" );
	// return that things have been handled
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Activate the demo player UI
//-----------------------------------------------------------------------------
int CGameUI::ActivateDemoUI()
{
	if ( !g_pTaskbar )
		return 0;
	g_pTaskbar->OnOpenDemoDialog();
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: returns 1 on the game UI having exclusive input, false otherwise
//-----------------------------------------------------------------------------
int CGameUI::HasExclusiveInput()
{
	return IsGameUIActive();
}

bool CGameUI::IsGameUIActive()
{
	return m_bActivatedUI && staticPanel->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::StartProgressBar( const char* progressType, int steps )
{
//	TRACE_FUNCTION("CGameUI::StartProgressBar");
	if ( !g_hLoadingDialog.Get() )
		g_hLoadingDialog = new CLoadingDialog( staticPanel );
	// close the start menu
	staticPanel->SetBackgroundRenderState( CBasePanel::BACKGROUND_LOADING );
	m_pszCurrentProgressType = progressType;
	if ( m_flProgressStartTime < 0.001f )
		m_flProgressStartTime = (float)vgui2::system()->GetCurrentTime();
	// open a loading dialog
	g_hLoadingDialog->SetProgressRange( 0, steps );
	g_hLoadingDialog->SetProgressPoint( 0 );
	g_hLoadingDialog->DisplayProgressBar( progressType, "invalid" );
	if ( progressType && ( !strnicmp( progressType, "http://", 7 ) || !strnicmp( progressType, "https://", 8 ) ) )
		g_hLoadingDialog->SwitchToHttpDownloadDialog( progressType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CGameUI::ContinueProgressBar( int point, float )
{
	if ( !g_hLoadingDialog.Get() )
		return 0;
	g_hLoadingDialog->SetProgressPoint( point );
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: stops progress bar, displays error if necessary
//-----------------------------------------------------------------------------
void CGameUI::StopProgressBar( bool error, const char* failureReason, const char* extendedReason )
{
	if ( !g_hLoadingDialog.Get() && error )
		g_hLoadingDialog = new CLoadingDialog( staticPanel );
	if ( g_hLoadingDialog.Get() )
	{
		if ( error )
		{
			// turn the dialog to error display mode
			g_hLoadingDialog->DisplayGenericError( failureReason, extendedReason );
		}
		else
		{
			// close loading dialog
			g_hLoadingDialog->Close();
			g_hLoadingDialog = NULL;
		}
		// stop drawing loading screen
		staticPanel->SetBackgroundRenderState( CBasePanel::BACKGROUND_DESKTOPIMAGE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets loading info text
//-----------------------------------------------------------------------------
int CGameUI::SetProgressBarStatusText( const char* statusText )
{
	if ( !g_hLoadingDialog.Get() )
		return 0;
	g_hLoadingDialog->SetStatusText( statusText );
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBar( float progress )
{
	if ( g_hLoadingDialog.Get() )
		g_hLoadingDialog->SetSecondaryProgress( progress );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBarText( const char* statusText )
{
	if ( g_hLoadingDialog.Get() )
		g_hLoadingDialog->SetSecondaryProgressText( statusText );
}

void CGameUI::OnDisconnectFromServer( int failureCode, const char* username )
{
	DisconnectFromServer();
	switch ( failureCode )
	{
	case STEAMLOGINFAILURE_NOSTEAMLOGIN:
		if ( g_hLoadingDialog.Get() )
			g_hLoadingDialog->DisplayNoSteamConnectionError();
		break;
	case STEAMLOGINFAILURE_VACBANNED:
		if ( g_hLoadingDialog.Get() )
			g_hLoadingDialog->DisplayVACBannedError();
		break;
	case STEAMLOGINFAILURE_BADTICKET:
	case STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE:
	case STEAMLOGINFAILURE_CONNECTIONLOST:
	case STEAMLOGINFAILURE_NOCONNECTION:
		if ( g_hLoadingDialog.Get() )
			g_hLoadingDialog->DisplaySteamConnectionError( username, (ESteamLoginFailure)failureCode );
		break;
	default:
		break;
	}
}

void CGameUI::ConnectToServer( const char* game, int address, int port )
{
	if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
	{
		baseuifuncs->ActivateGameUI();
		StartProgressBar( "CareerPause", 1 );
		staticPanel->SetBackgroundRenderState( CBasePanel::BACKGROUND_CAREERLOAD );
		ContinueProgressBar( 1, 100.0f );
		SetProgressBarStatusText( "#Career_Start" );
		g_hLoadingDialog->SwitchToPausedCareerDialog();
		TheCareerGame->FinishMatchStart();
	}
	else
	{
		if ( stricmp( game, "valve" ) )
			engine->pfnClientCmd( "mp3 stop\n" );
		char command[128];
		char cvarName[] = "ui_cmd_on_connect";
		strncpy( command, engine->pfnGetCvarString( cvarName ), sizeof( command ) );
		command[sizeof( command ) - 1] = 0;
		if ( strlen( command ) > 2 )
		{
			engine->pfnClientCmd( command );
			engine->pfnClientCmd( "ui_cmd_on_connect 0\n" );
		}
		//engine->pfnClientCmd("mp3 stop\n");
		// SRC version
//	engine->ClientCmd("stop\n");
		baseuifuncs->HideGameUI();
	}
	m_iGameIP = address;
	m_iGamePort = port;
	KeyValues* message = new KeyValues( "ConnectedToGame" );
	message->SetInt( "ip", m_iGameIP );
	message->SetInt( "connectionport", m_iGamePort );
	message->SetInt( "queryport", m_iGamePort );
	g_VModuleLoader.PostMessageToAllModules( message );
}

void CGameUI::HideGameUI()
{
//	TRACE_FUNCTION("CGameUI::HideGameUI");
	// we can't hide the UI if we're not in a level
	const char* level = engine->pfnGetLevelName();
	if ( level && level[0] )
	{
		//show both the background panel and the taskbar 
		staticPanel->SetVisible( false );
		g_pTaskbar->OnHide();
		engine->pfnClientCmd( "inputset_game" );
		if ( !TheCareerGame || !TheCareerGame->IsPlayingMatch() || TheCareerGame->CanUnpause() )
			engine->pfnClientCmd( "unpause" );
		engine->pfnClientCmd( "hideconsole" );
	}
	engine->pfnClientCmd( "richpresence_update\n" );
}

//-----------------------------------------------------------------------------
// Purpose: activates the loading dialog on level load start
//-----------------------------------------------------------------------------
void CGameUI::LoadingStarted( const char* resourceType, const char* resourceName )
{
	KeyValues* message = new KeyValues( "LoadingStarted", "type", resourceType, "name", resourceName );
	g_VModuleLoader.PostMessageToAllModules( message );
	staticPanel->SetBackgroundRenderState( !stricmp( resourceType, "transition" ) ?
			CBasePanel::BACKGROUND_LOADINGTRANSITION : CBasePanel::BACKGROUND_LOADING );
}

//-----------------------------------------------------------------------------
// Purpose: closes any level load dialog
//-----------------------------------------------------------------------------
void CGameUI::LoadingFinished( const char* resourceType, const char* resourceName )
{
	KeyValues* message = new KeyValues( "LoadingFinished", "type", resourceType, "name", resourceName );
	g_VModuleLoader.PostMessageToAllModules( message );
	// stop drawing loading screen
	staticPanel->SetBackgroundRenderState( CBasePanel::BACKGROUND_DESKTOPIMAGE );
	baseuifuncs->HideGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game disconnects from a server
//-----------------------------------------------------------------------------
void CGameUI::DisconnectFromServer()
{
	ActivateGameUI();
	m_iGameIP = 0;
	m_iGamePort = 0;
	g_VModuleLoader.PostMessageToAllModules( new KeyValues( "DisconnectedFromGame" ) );
}

void CGameUI::ValidateCDKey( bool, bool )
{
}

bool CGameUI::FindPlatformDirectory( char* platformDir, int bufferSize )
{
	strncpy( platformDir, "platform", bufferSize );
	return true;
}

void CGameUI::FinishCareerLoad()
{
	engine->pfnClientCmd( "mp3 stop\n" );
	engine->pfnClientCmd( "_careeraudio\n" );
	baseuifuncs->HideGameUI();
	staticPanel->SetBackgroundRenderState( CBasePanel::BACKGROUND_DESKTOPIMAGE );
}

void CGameUI::SetIsRunningOnSteamDeck( bool running )
{
	m_bIsRunningOnDeck = running;
}

bool CGameUI::IsRunningOnSteamDeck() const
{
	return m_bIsRunningOnDeck;
}
