#pragma once

#include "CareerBaseFrame.h"
#include <string>

class CCareerPanelListPanel;
class CProfileButton;

namespace vgui2
{
class CBitmapImagePanel;
class ComboBox;
class URLLabel;
}

class CCareerProfileFrame : public CCareerBaseFrame
{
public:
	CCareerProfileFrame( vgui2::Panel* parent );
	~CCareerProfileFrame() override;
	void Activate() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void OnCommand( const char* command ) override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void PerformLayout() override;
	void SelectMissionPack( const char* missionPackName );
	void OnThink() override;
	void QueueFocusUpdate( int frames ) { m_needFocus = frames; }

private:
	int PopulateProfiles();
	void ChooseDifficulty();
	void UpdateSelectedProfile( const char* profile );
	void RestoreFocus();
	void PopulateMissionPacks();

	vgui2::Dar<CProfileButton*>	m_profiles;
	CCareerPanelListPanel*		m_pProfileList;
	vgui2::Label*				m_pProfileSelectionBackground;
	vgui2::Button*				m_pResumeButton;
	vgui2::Button*				m_pDeleteButton;
	vgui2::Button*				m_pNewButton;
	std::string					m_name;
	vgui2::Label*				m_pMissionPackBackground;
	vgui2::Label*				m_pMissionPackAuthor;
	vgui2::Label*				m_pMissionPackDescription;
	vgui2::URLLabel*				m_pMissionPackURL;
	vgui2::CBitmapImagePanel*		m_pMissionPackLogo;
	vgui2::ComboBox*				m_pMissionPackComboBox;
	int							m_needFocus;
};
