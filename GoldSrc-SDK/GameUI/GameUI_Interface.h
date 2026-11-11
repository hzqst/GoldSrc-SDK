//========= Copyright © 1996-2000, Valve LLC, All rights reserved. ============
//
// Purpose: Defines the interface that the GameUI dll exports
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "IGameUI.h"
#include "PHandle.h"

struct cl_enginefuncs_s;
class CSocket;
class CTaskbar;
class CLoadingDialog;
class IBaseSystem;
class IBaseUI;
class IEngineVGui;
class IGameClientExports;
class IGameUIFuncs;
class IFriendsUser;

namespace vgui2
{
class ISurface;
class TestScriptController;
}

//-----------------------------------------------------------------------------
// Purpose: Implementation of GameUI's exposed interface 
//-----------------------------------------------------------------------------
class CGameUI : public IGameUI
{
public:
	CGameUI();
	~CGameUI() override;
	void Initialize( CreateInterfaceFn* factories, int count ) override;
	void Start( cl_enginefuncs_s* engineFunctions, int interfaceVersion, IBaseSystem* system ) override;
	void Shutdown() override;
	int ActivateGameUI() override; // activates the menus, returns 0 if it doesn't want to handle it
	int ActivateDemoUI() override; // activates the demo player, returns 0 if it doesn't want to handle it
	int HasExclusiveInput() override;
	void RunFrame() override;
	void ConnectToServer( const char* game, int address, int port ) override;
	void DisconnectFromServer() override;
	void HideGameUI() override;
	// returns true if the game UI is currently visible
	bool IsGameUIActive() override;
	void LoadingStarted( const char* resourceType, const char* resourceName ) override;
	void LoadingFinished( const char* resourceType, const char* resourceName ) override;
	void StartProgressBar( const char* progressType, int steps ) override;
	int ContinueProgressBar( int point, float fraction ) override;
	void StopProgressBar( bool error, const char* failureReason, const char* extendedReason ) override;
	int SetProgressBarStatusText( const char* statusText ) override;
	void SetSecondaryProgressBar( float progress ) override;
	void SetSecondaryProgressBarText( const char* statusText ) override;
	void ValidateCDKey( bool force, bool connect ) override;
	void OnDisconnectFromServer( int failureCode, const char* username ) override;
	void SetIsRunningOnSteamDeck( bool running ) override;
	bool IsRunningOnSteamDeck() const override;
#ifndef _WIN32
	void RunVguiTestScript( const char* filename );
#endif
	bool FindPlatformDirectory( char* platformDir, int bufferSize );
	void FinishCareerLoad();

private:
	bool				m_bTryingToLoadTracker;
	char				m_szPlatformDir[260];
	int					m_iNumFactories;
	CreateInterfaceFn	m_FactoryList[5];
	// details about the game we're currently in
	int					m_iGameIP;
	int					m_iGamePort;
	bool				m_bActivatedUI;
	float				m_flRetryPlatformReleaseMessageTime;
	const char*			m_pszCurrentProgressType;
	float				m_flProgressStartTime;
	CSocket*			m_pMaster;
	vgui2::DHANDLE<vgui2::TestScriptController> m_hTestScriptController;
	bool				m_bIsRunningOnDeck;
};

extern cl_enginefuncs_s* engine;
extern cl_enginefuncs_s gEngfuncs;
extern CTaskbar* g_pTaskbar;
extern CGameUI* g_pGameUI;
extern IEngineVGui* g_pEngineVGUI;
extern IGameClientExports* g_pGameClientExports;
extern IFriendsUser* g_pFriendsUser;
extern IBaseSystem* g_pSystemWrapper;
extern IBaseUI* baseuifuncs;
extern IGameUIFuncs* gameuifuncs;
extern vgui2::ISurface* enginesurfacefuncs;
extern vgui2::DHANDLE<CLoadingDialog> g_hLoadingDialog;

CGameUI*	GameUI();
IGameClientExports*	GameClientExports();
IEngineVGui*	EngineVGUI();

void	Career_Printf( const char* format, ... );
int		__MsgFunc_CZCareer( const char* name, int size, void* buffer );

char*	UTIL_va( char* format, ... );
