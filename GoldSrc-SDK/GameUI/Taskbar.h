//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "EditablePanel.h"
#include "PHandle.h"
#include "GameUI/ICareerUI.h"

namespace vgui2
{
class Frame;
class ImagePanel;
}

namespace vgui = vgui2;

class CBackgroundMenuButton;
class CConsoleButton;
class CGameMenu;
class CTaskButton;

enum LogoBlip_t
{
	E_LOGO_BLIP_BOTH = 0,
	E_LOGO_BLIP_JUST_LOGO,
	E_LOGO_BLIP_JUST_BG,
	E_LOGO_BLIP_STAGGER,
	E_LOGO_BLIP_BOTH_SHOW_BLIP_LOGO_ONLY
};

class CTaskbar : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTaskbar, vgui::EditablePanel );

public:
	CTaskbar( vgui2::Panel* parent, const char* panelName );
	~CTaskbar() override;

	// update the taskbar a frame
	void RunFrame();
	// sets the title of a task
	void SetTitle( vgui2::VPANEL panel, const wchar_t* title );
	// adds a new task to the list
	void AddTask( vgui2::VPANEL panel );
	void OnGameUIActivated();
	void OnHide();

	void OnOpenNewGameDialog();
	void OnOpenLoadGameDialog();
	void OnOpenSaveGameDialog();
	void OnOpenOptionsDialog();
	void OnOpenServerBrowser();
	void OnOpenDemoDialog();
	void OnOpenCreateMultiplayerGameDialog();
	void OnOpenQuitConfirmationDialog();
	void OnOpenChangeGameDialog();
	void OnOpenPlayerListDialog();
	void OnOpenNewCareerNameDialog();
	void OnOpenCareerMapDialog( bool newGame );
	void OnOpenCareerBotDialog();
	void OnOpenCreateCareerGameDialog();
	vgui2::Frame* OnOpenResumeCareerGameDialog( CareerDifficultyType difficulty );
	void PositionDialog( vgui2::PHandle dialog );
	void OnOpenCareerProfileFrame();

protected:
	// menu manipulation
	void CreatePlatformMenu();
	void CreateGameMenu();
	void UpdateGameMenus();
	void UpdateTaskButtons();
	CGameMenu* RecursiveLoadGameMenu( KeyValues* datafile );
	CTaskButton* FindTask( vgui2::VPANEL panel );

	void OnCommand( const char* command ) override;
	void PerformLayout() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	MESSAGE_FUNC_INT( OnActivateModule, "ActivateModule", moduleIndex );
	MESSAGE_FUNC( ActivateGameUI, "ActivateGameUI" );

private:
	CBackgroundMenuButton* m_pGameMenuButton;
	CBackgroundMenuButton* m_pPlatformMenuButton;
	CConsoleButton* m_pConsoleButton;

	int m_nLogoImageXMin;
	int m_nLogoImageXMax;
	int m_nLogoImageXGoal;
	float m_flPrevFrameTime;
	float m_flTimeLogoNewGoal;
	float m_flTimeLogoNewGoalMin;
	float m_flTimeLogoNewGoalMax;
	float m_flTimeUntilLogoBlipMin;
	float m_flTimeUntilLogoBlipMax;
	float m_flTimeLogoBlip;
	int m_nLogoBlipType;
	int m_nLogoImageY;
	float m_fLogoImageX;
	float m_fLogoSpeedMin;
	float m_fLogoSpeedMax;
	float m_fLogoSpeed;
	int m_nLogoBGOffsetX;
	int m_nLogoBGOffsetY;
	bool m_bUseAnimatedLogo;
	LogoBlip_t m_nNextLogoBlipType;

	vgui2::DHANDLE<vgui2::ImagePanel> m_hLogoImage;
	vgui2::DHANDLE<vgui2::ImagePanel> m_hLogoImageBlip;
	vgui2::DHANDLE<vgui2::ImagePanel> m_hLogoImageBlip2;
	vgui2::DHANDLE<vgui2::ImagePanel> m_hLogoBackgroundImages[4];
	int m_nLogoBackgroundImageOffsets[4];
	vgui2::DHANDLE<vgui2::ImagePanel> m_hLogoBackgroundBlipImages[4];
	int m_nLogoBackgroundBlipImageOffsets[4];

	CGameMenu* m_pPlatformMenu;
	CGameMenu* m_pGameMenu;
	bool m_bShowPlatformMenu;
	bool m_bPlatformMenuInitialized;
	int m_iGameMenuInset;

	vgui2::DHANDLE<vgui2::Frame> m_hNewGameDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hLoadGameDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hSaveGameDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hOptionsDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hCreateMultiplayerGameDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hDemoPlayerDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hChangeGameDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hPlayerListDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hCareerProfileDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hCareerDifficultyDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hCareerMapDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hCareerBotDialog;

	CUtlVector<CTaskButton*> g_Tasks;
	bool m_bIsInGame;
	float m_flTimeTransitionedBetweenInGame;
};
