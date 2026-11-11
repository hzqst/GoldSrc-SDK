#pragma once

#include "basecareermenu.h"

namespace vgui2
{
class Button;
class CBitmapImagePanel;
class Label;
}

class CCSCareerMatchEndMenu : public CCSCareerBaseMenu
{
public:
	CCSCareerMatchEndMenu( vgui2::Panel* parent );
	~CCSCareerMatchEndMenu() override;
	void Update();

private:
	void OnKeyCodePressed( vgui2::KeyCode code ) override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void OnCommand( const char* command ) override;
	void ShowMap( int index, const char* name );
	vgui2::Label*				m_pWinsLabel;
	vgui2::Label*				m_pLossesLabel;
	vgui2::Label*				m_pResultLabel;
	vgui2::Label*				m_pNewPointsLabel;
	vgui2::Label*				m_pNewLocationsLabel;
	vgui2::Label*				m_pCampaignCompleteLabel;
	vgui2::Label*				m_pMedalEarnedLabel;
	vgui2::Label*				m_pMedalNameLabel;
	vgui2::Label*				m_pMapCompleteLabel[3];
	vgui2::Label*				m_pMapLabel[3];
	vgui2::Button*				m_pFinishButton;
	vgui2::Button*				m_pRetryButton;
	vgui2::CBitmapImagePanel*	m_pMedalPanel;
	vgui2::CBitmapImagePanel*	m_pMapPanel[3];
};
