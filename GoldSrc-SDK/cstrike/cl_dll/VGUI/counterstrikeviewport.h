//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef COUNTERSTRIKEVIEWPORT_H
#define COUNTERSTRIKEVIEWPORT_H

#include "game_controls/vgui_TeamFortressViewport.h"
#include "icstrikeviewport.h"
#include "icstrikeviewportmsgs.h"

class CBuyMenu;
class CBuyPresetEditMenu;
class CBuyPresetEditMainMenu;
class CCSClassMenu;
class CCSClientScoreBoardDialog;
class CCSCareerRoundEndMenu;
class CCSCareerMatchEndMenu;
class CCSCreditsMenu;
class CCSProgressBar;
class CCSSpectatorGUI;
class CCSTeamMenu;

namespace vgui2
{
class CBitmapImagePanel;
class Frame;
class Label;
}

class CounterStrikeViewport : public TeamFortressViewport, public ICSViewPort, public ICSViewPortMsgs
{
public:
	CounterStrikeViewport();
	~CounterStrikeViewport() override;
	void Initialize( CreateInterfaceFn* factories, int count ) override;
	void Start() override;
	void Shutdown() override;
	void SetParent( vgui2::VPANEL parent ) override;
	void HideAllVGUIMenu() override;
	int KeyInput( int down, int keynum, const char* binding ) override;
	void HideVGUIMenu( int menu ) override;
	void OnLevelChange() override;
	int GetForceCamera() override;
	int MsgFunc_ForceCam( const char* name, int size, void* buffer ) override;
	void UpdateSpectatorPanel() override;
	int MsgFunc_TeamInfo( const char* name, int size, void* buffer ) override;
	int MsgFunc_ScoreInfo( const char* name, int size, void* buffer ) override;
	int MsgFunc_TeamScore( const char* name, int size, void* buffer ) override;
	int MsgFunc_Location( const char* name, int size, void* buffer ) override;
	void ActivateClientUI() override;
	void HideClientUI() override;
	void UpdateBuyPresets() override;
	void UpdateScheme() override;
	bool IsProgressBarVisible() override;
	void StartProgressBar( const char* title, int ticks, int progress, bool timeBased ) override;
	void UpdateProgressBar( const char* status, int progress ) override;
	void StopProgressBar() override;
	bool IsAnyVGUIMenuActive() override;
	bool IsVGUIMenuActive( int menu ) override;
	void DisplayVGUIMenu( int menu ) override;
	void OnTick() override;
	void CenterWindow( vgui2::Frame* window );

private:
	class CCSBackGroundPanel : public TeamFortressViewport::CBackGroundPanel
	{
	public:
		CCSBackGroundPanel( vgui2::Panel* parent );
		~CCSBackGroundPanel() override;
		void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
		void PaintBackground() override;
		void Activate() override;
		void PerformLayout() override;
		virtual void SetTitleText( const char* text );
		void GetOffset( int& x, int& y ) { x = m_offsetX; y = m_offsetY; }

	protected:
		vgui2::CBitmapImagePanel*	m_pTopLeftPanel;
		vgui2::CBitmapImagePanel*	m_pTopRightPanel;
		vgui2::CBitmapImagePanel*	m_pBottomLeftPanel;
		vgui2::CBitmapImagePanel*	m_pBottomRightPanel;
		vgui2::Label*			m_pTitleLabel;
		vgui2::Panel*			m_pGapPanel;
		vgui2::CBitmapImagePanel*	m_pExclamationPanel;
		Color					m_bgColor;
		Color					m_titleColor;
		Color					m_borderColor;
		int						m_offsetX;
		int						m_offsetY;
	};

	CCSBackGroundPanel*		m_pCSBackGround;



	// main panels
	CCSTeamMenu*			m_pTeamMenu;
	CCSClassMenu*			m_pClassMenu;
	CBuyMenu*				m_pBuyMenu;
	CCSSpectatorGUI*			m_pPrivateSpectatorGUI;
	CCSClientScoreBoardDialog*	m_pPrivateCSClientScoreBoard;
	CCSCareerRoundEndMenu*	m_pCareerRoundEndMenu;
	CCSCareerMatchEndMenu*	m_pCareerMatchEndMenu;
	CCSCreditsMenu*			m_pCreditsMenu;
	CBuyPresetEditMainMenu*	m_pBuyPresetMainMenu;
	CBuyPresetEditMenu*		m_pBuyPresetEditMenu;
	CCSProgressBar*			m_pProgressBar;
	int						m_iForceCamera;
	int						m_iForceChaseCam;
	int						m_iFadeToBlack;
	bool					m_bOldStyleMenus;
	bool					m_inClientUI;
	bool					m_bCenteredPersistentWindows;
};

extern VGuiLibraryInterface_t clientInterface;

int GetAlternateProportionalValueFromNormal( int normalizedValue );
int GetAlternateProportionalValueFromScaled( int scaledValue );
void RepositionControl( vgui2::Panel* panel );

#endif
