//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef TEAMFORTRESSVIEWPORT_H
#define TEAMFORTRESSVIEWPORT_H

#include "game_controls/iviewport.h"
#include "game_controls/iviewportmsgs.h"
#include "game_controls/iclientvgui.h"
#include "utlqueue.h"
#include "vguitextwindow.h"
#include "ispectatorinterface.h"

#define MENU_TEAM		2
#define MENU_CLASS		3
#define MENU_MAPBRIEFING	4
#define MENU_INTRO		5
#define MENU_CLASSHELP	6
#define MENU_SPECHELP		9

class CClientMOTD;
class ISpectatorInterface;
class IScoreBoardInterface;
class CTeamMenu;
class CClassMenu;
class CVGUITextWindow;
class CSpectatorGUI;
class CClientScoreBoardDialog;
namespace vgui2
{
class Menu;
}

class TeamFortressViewport : public IViewPort, public IClientVGUI, public IViewPortMsgs
{
public:
	TeamFortressViewport();
	~TeamFortressViewport() override;
	void Initialize( CreateInterfaceFn* factories, int count ) override;
	void Start() override;
	void SetParent( vgui2::VPANEL parent ) override;
	bool UseVGUI1() override;
	void HideScoreBoard() override;
	void HideAllVGUIMenu() override;
	void ActivateClientUI() override;
	void HideClientUI() override;
	void Shutdown() override;
	void UpdateScoreBoard() override;
	bool AllowedToPrintText() override;
	void GetAllPlayersInfo() override;
	void DeathMsg( int killer, int victim ) override;
	void ShowScoreBoard() override;
	bool IsScoreBoardVisible() override;
	void UpdateSpectatorPanel() override;
	int KeyInput( int down, int key, const char* binding ) override;
	int GetAllowSpectators() override;
	void OnLevelChange() override;
	void InputPlayerSpecial() override;
	void ShowVGUIMenu( int menu ) override;
	virtual void HideVGUIMenu( int menu );
	void ShowSpectatorGUI() override;
	void ShowSpectatorGUIBar() override;
	void HideSpectatorGUI() override;
	void DeactivateSpectatorGUI() override;
	void SetSpectatorBanner( const char* image ) override;
	bool IsSpectatorGUIVisible() override;
	bool IsSpectatorBarVisible() override;
	void SpectatorGUIEnableInsetView( int enabled ) override;
	void ShowCommandMenu() override;
	void HideCommandMenu() override;
	int IsCommandMenuVisible() override;
	int GetValidClasses( int team ) override;
	virtual int GetNumberOfTeams();
	virtual bool GetIsFeigning();
	virtual bool GetIsSettingDetpack();
	virtual int GetBuildState();
	virtual int IsRandomPC();
	virtual char* GetTeamName( int team );
	virtual int GetCurrentMenuID();
	virtual const char* GetMapName();
	const char* GetServerName() override;

	int MsgFunc_ValClass( const char* name, int size, void* buffer ) override;
	int MsgFunc_TeamNames( const char* name, int size, void* buffer ) override;
	int MsgFunc_Feign( const char* name, int size, void* buffer ) override;
	int MsgFunc_Detpack( const char* name, int size, void* buffer ) override;
	int MsgFunc_VGUIMenu( const char* name, int size, void* buffer ) override;
	int MsgFunc_MOTD( const char* name, int size, void* buffer ) override;
	int MsgFunc_BuildSt( const char* name, int size, void* buffer ) override;
	int MsgFunc_RandomPC( const char* name, int size, void* buffer ) override;
	int MsgFunc_ServerName( const char* name, int size, void* buffer ) override;
	int MsgFunc_ScoreInfo( const char* name, int size, void* buffer ) override;
	int MsgFunc_TeamScore( const char* name, int size, void* buffer ) override;
	int MsgFunc_TeamInfo( const char* name, int size, void* buffer ) override;
	int MsgFunc_Spectator( const char* name, int size, void* buffer ) override;
	int MsgFunc_AllowSpec( const char* name, int size, void* buffer ) override;
	int MsgFunc_SpecFade( const char* name, int size, void* buffer ) override;
	int MsgFunc_ResetFade( const char* name, int size, void* buffer ) override;
	int GetViewPortScheme() override;
	vgui2::Panel* GetViewPortPanel() override;
	void HideBackGround() override;
	void ChatInputPosition( int* x, int* y ) override;
	int GetSpectatorBottomBarHeight() override;
	int GetSpectatorTopBarHeight() override;
	virtual bool IsVGUIMenuActive( int menu );
	virtual bool IsAnyVGUIMenuActive();
	virtual void DisplayVGUIMenu( int menu );
	void OnTick() override;

protected:
	class CSpecHelpWindow : public CVGUITextWindow
	{
	public:
		CSpecHelpWindow( ISpectatorInterface* spectator ) : CVGUITextWindow( NULL ), m_SpecGUI( spectator ) {}
	private:
		void OnClose() override;
		ISpectatorInterface* m_SpecGUI;
	};
	class CBackGroundPanel : public vgui2::EditablePanel
	{
	public:
		CBackGroundPanel() : vgui2::EditablePanel( NULL, "ViewPortBackGround" ) {}
		void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
		void PerformLayout() override;
		void OnMousePressed( vgui2::MouseCode ) override {}
	};
	char m_sTeamNames[5][32];
	// main panels
	CClientMOTD* m_pMOTD;
	ISpectatorInterface* m_pSpectatorGUI; // spectator is an interface so you can subclass it without having to override all the spectator function calls in TeamFortressViewport
	IScoreBoardInterface* m_pClientScoreBoard;
	CTeamMenu* m_pTeamMenu;
	CClassMenu* m_pClassMenu;
	vgui2::Menu* m_pCommandMenu;
	CVGUITextWindow* m_pMapBriefing;
	CVGUITextWindow* m_pClassHelp;
	CSpecHelpWindow* m_pSpecHelp;
	CBackGroundPanel* m_pBackGround;
	bool m_bInitialized;
	CreateInterfaceFn m_FactoryList[5];
	CSpectatorGUI* m_pPrivateSpectatorGUI;
	CClientScoreBoardDialog* m_pPrivateClientScoreBoard;
	CUtlQueue<int> m_PendingDialogs; // a queue of menus to display, the top of the stack is the 
	char m_szServerName[32];
	int m_iNumTeams;
	int m_iGotAllMOTD;
	char m_szMOTD[1536];
	int m_iAllowSpectators;
	char m_sMapName[64];
//	int		m_iAllowSpectators;
//	char	m_sMapName[64];
	int m_iValidClasses[5];
	bool m_bIsFeigning;
	bool m_bIsSettingDetpack;
	int m_iBuildState;
	int m_iRandomPC;


	// viewport state
	float m_flScoreBoardLastUpdated;
	int m_iCurrentTeamNumber;	// used to detect team change
	int m_iUser1;
	int m_iUser2;
};

#endif
