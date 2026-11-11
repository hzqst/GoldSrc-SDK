//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "iviewport.h"
#include "iclientvgui.h"
#include "iviewportmsgs.h"
#include "utlvector.h"
#include "Frame.h"
#include "vguitextwindow.h"

#define MENU_TEAM			2
#define MENU_CLASS			3
#define MENU_MAPBRIEFING	4
#define MENU_INTRO			5
#define MENU_CLASSHELP		6
#define MENU_SPECHELP		9

class CClientMOTD;
class ISpectatorInterface;
class IScoreBoardInterface;
class CTeamMenu;
class CClassMenu;
class CCommandMenu;
class CVGUITextWindow;
class TutorWindowManager;
class CSpectatorGUI;
class CClientScoreBoardDialog;

template<class T> class CUtlQueue
{
public:
	CUtlVector<T> m_heap;
	T m_current;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

class TeamFortressViewport : public IViewPort, public IClientVGUI, public IViewPortMsgs
{
public:
	TeamFortressViewport();
	~TeamFortressViewport() override;
	void Initialize( CreateInterfaceFn* factories, int count ) override;
	void Start() override;
	void Shutdown() override;
	void SetParent( vgui2::VPANEL parent ) override;
	bool UseVGUI1() override;
	void ActivateClientUI() override;
	void HideClientUI() override;
	virtual void SetNumberOfTeams( int teams );

	int MsgFunc_ValClass( const char* name, int size, void* buffer ) override;
	int MsgFunc_TeamNames( const char* name, int size, void* buffer ) override;
	int MsgFunc_Feign( const char* name, int size, void* buffer ) override;
	int MsgFunc_Detpack( const char* name, int size, void* buffer ) override;
	int MsgFunc_VGUIMenu( const char* name, int size, void* buffer ) override;
	int MsgFunc_TutorText( const char* name, int size, void* buffer ) override;
	int MsgFunc_TutorLine( const char* name, int size, void* buffer ) override;
	int MsgFunc_TutorState( const char* name, int size, void* buffer ) override;
	int MsgFunc_TutorClose( const char* name, int size, void* buffer ) override;
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
	virtual bool IsVGUIMenuActive( int menu );
	virtual bool IsAnyVGUIMenuActive();
	virtual void DisplayVGUIMenu( int menu );

	VGuiLibraryInterface_t* GetClientDllInterface() override;
	void SetClientDllInterface( VGuiLibraryInterface_t* callbacks ) override;
	void UpdateScoreBoard() override;
	bool AllowedToPrintText() override;
	void GetAllPlayersInfo() override;
	void DeathMsg( int killer, int victim ) override;
	void ShowScoreBoard() override;
	bool CanShowScoreBoard() override;
	void HideAllVGUIMenu() override;
	void UpdateSpectatorPanel() override;
	bool IsScoreBoardVisible() override;
	void HideScoreBoard() override;
	int KeyInput( int down, int keynum, const char* binding ) override;
	void ShowVGUIMenu( int menu ) override;
	void HideVGUIMenu( int menu ) override;
	void ShowTutorTextWindow( const wchar_t* text, int id, int messageClass, int spectator ) override;
	void ShowTutorLine( int entity, int id ) override;
	void ShowTutorState( const wchar_t* text ) override;
	void CloseTutorTextWindow() override;
	bool IsTutorTextWindowOpen() const override;
	void ShowSpectatorGUI() override;
	void ShowSpectatorGUIBar() override;
	void HideSpectatorGUI() override;
	void DeactivateSpectatorGUI() override;
	bool IsSpectatorGUIVisible() override;
	bool IsSpectatorBarVisible() override;
	int MsgFunc_ResetFade( const char* name, int size, void* buffer ) override;
	void SetSpectatorBanner( const char* image ) override;
	void SpectatorGUIEnableInsetView( int enabled ) override;
	void ShowCommandMenu() override;
	void UpdateCommandMenu() override;
	void HideCommandMenu() override;
	int IsCommandMenuVisible() override;
	int GetValidClasses( int team ) override;
	int GetNumberOfTeams() override;
	bool GetIsFeigning() override;
	int GetIsSettingDetpack() override;
	int GetBuildState() override;
	int IsRandomPC() override;
	char* GetTeamName( int team ) override;
	int GetCurrentMenuID() override;
	const char* GetMapName() override;
	const char* GetServerName() override;
	void InputPlayerSpecial() override;
	void OnTick() override;
	int GetViewPortScheme() override;
	vgui2::VPANEL GetViewPortPanel() override;
	int GetAllowSpectators() override;
	void OnLevelChange() override;
	void HideBackGround() override;
	void ChatInputPosition( int* x, int* y ) override;
	int GetSpectatorBottomBarHeight() override;
	int GetSpectatorTopBarHeight() override;
	bool SlotInput( int slot ) override;
	VGuiLibraryTeamInfo_t GetPlayerTeamInfo( int player ) override;
	void MakeSafeName( const char* oldName, char* newName, int capacity ) override;

protected:
	class CSpecHelpWindow : public CVGUITextWindow
	{
	public:
		CSpecHelpWindow( vgui2::Panel* parent, ISpectatorInterface* spectator );
		void SetSpectatorInterface( ISpectatorInterface* spectator );

	private:
		void OnClose() override;
		ISpectatorInterface* m_SpecGUI;
	};
	class CBackGroundPanel : public vgui2::Frame
	{
	public:
		CBackGroundPanel( vgui2::Panel* parent );
		void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
		void PerformLayout() override;
		void OnMousePressed( vgui2::MouseCode code ) override;
		vgui2::VPANEL IsWithinTraverse( int x, int y, bool traversePopups ) override;
	};
	char*						m_sTeamNames[5];
	// main panels
	CClientMOTD*				m_pMOTD;
	ISpectatorInterface*		m_pSpectatorGUI; // spectator is an interface so you can subclass it without having to override all the spectator function calls in TeamFortressViewport
	IScoreBoardInterface*		m_pClientScoreBoard;
	CTeamMenu*					m_pTeamMenu;
	CClassMenu*					m_pClassMenu;
	CCommandMenu*				m_pCommandMenu;
	CVGUITextWindow*			m_pMapBriefing;
	CVGUITextWindow*			m_pClassHelp;
	TutorWindowManager*			m_pTutorWindowManager;
	CSpecHelpWindow*			m_pSpecHelp;
	CBackGroundPanel*			m_pBackGround;
	bool						m_bInitialized;
	CreateInterfaceFn			m_FactoryList[5];
	CSpectatorGUI*				m_pPrivateSpectatorGUI;
	CClientScoreBoardDialog*		m_pPrivateClientScoreBoard;
	CUtlQueue<int>				m_PendingDialogs; // a queue of menus to display, the top of the stack is the 
	char						m_szServerName[32];
	char						m_szMOTD[1536];
	int							m_iNumTeams; // the number of teams in the game
	int							m_iGotAllMOTD;
	int							m_iAllowSpectators;
	char						m_sMapName[64];
	int							m_iValidClasses[5];
	bool						m_bIsFeigning;
	int							m_iIsSettingDetpack;
	int							m_iBuildState;
	int							m_iRandomPC;


	// viewport state
	float						m_flScoreBoardLastUpdated;
	int							m_iCurrentTeamNumber;	// used to detect team change
	int							m_iUser1;
	int							m_iUser2;
	VGuiLibraryInterface_t*		m_pClientDllInterface;
};
