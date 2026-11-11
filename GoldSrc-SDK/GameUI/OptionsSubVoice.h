//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyPage.h"
#include "ivoicetweak.h"

class CCvarSlider;
class CCvarToggleCheckButton;

namespace vgui2
{
class Button;
class CheckButton;
class ImagePanel;
class Label;
class Slider;
}

class COptionsSubVoice : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_NAMED( COptionsSubVoice, vgui2::PropertyPage, "vgui::PropertyPage" );

public:
	COptionsSubVoice( vgui2::Panel* parent );
	~COptionsSubVoice() override;

	void OnResetData() override;
	void OnApplyChanges() override;
	void OnCommand( const char* command ) override;
	void OnThink() override;

	MESSAGE_FUNC( OnPageHide, "PageHide" );
	MESSAGE_FUNC_INT( OnSliderMoved, "SliderMoved", position );
	MESSAGE_FUNC_INT( OnCheckButtonChecked, "CheckButtonChecked", state );
	MESSAGE_FUNC( OnControlModified, "ControlModified" );

	void UseCurrentVoiceParameters();
	void StartTestMicrophone();
	void ResetVoiceParameters();
	void EndTestMicrophone();

private:
	IVoiceTweak*			m_pVoiceTweak; // Engine voice tweak API.
	vgui2::CheckButton*		m_pMicBoost;
	vgui2::ImagePanel*		m_pMicMeter;
	vgui2::ImagePanel*		m_pMicMeter2;
	vgui2::Button*			m_pTestMicrophoneButton;
	vgui2::Label*			m_pMicrophoneSliderLabel;
	vgui2::Slider*			m_pMicrophoneVolume;
	vgui2::Label*			m_pReceiveSliderLabel;
	CCvarSlider*			m_pReceiveVolume;
	CCvarToggleCheckButton*	m_pVoiceEnableCheckButton;
	int						m_nMicVolumeValue;
	bool					m_bMicBoostSelected;
	float					m_fReceiveVolume;
	int						m_nReceiveSliderValue;
	bool					m_bVoiceOn;
};
