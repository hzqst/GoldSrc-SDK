#pragma once

#include "basecareermenu.h"
#include "utlvector.h"

class TaskLabel;
namespace vgui2
{
class Button;
class Label;
}

class CCSCareerRoundEndMenu : public CCSCareerBaseMenu
{
public:
	CCSCareerRoundEndMenu( vgui2::Panel* parent );
	~CCSCareerRoundEndMenu() override;
	void Update();
	vgui2::Panel* CreateControlByName( const char* controlName ) override;

private:
	void OnKeyCodePressed( vgui2::KeyCode code ) override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void OnCommand( const char* command ) override;
	vgui2::Label*			m_pWinsLabel;
	vgui2::Label*			m_pLossesLabel;
	vgui2::Label*			m_pNeededLabel;
	vgui2::Label*			m_pWinByLabel;
	vgui2::Label*			m_pRoundResultLabel;
	vgui2::Label*			m_pImminentVictoryLabel;
	vgui2::Button*			m_pContinueButton;
	vgui2::Button*			m_pSurrenderButton;
	CUtlVector<TaskLabel*>	m_pTaskLabels;
	CUtlVector<TaskLabel*>	m_pTaskStatusLabels;
	vgui2::Label*			m_pTaskNoticeLabel;
};
