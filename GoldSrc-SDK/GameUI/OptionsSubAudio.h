//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyPage.h"

class CCvarSlider;
class CLabeledCommandComboBox;

namespace vgui = vgui2;

class COptionsSubAudio : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubAudio, vgui::PropertyPage );

public:
	COptionsSubAudio( vgui2::Panel* parent );
	~COptionsSubAudio() override;

protected:
	void OnResetData() override;
	void OnApplyChanges() override;
	MESSAGE_FUNC( OnControlModified, "ControlModified" );

private:
	CCvarSlider*			m_pSFXSlider;
	CCvarSlider*			m_pHEVSlider;
	CCvarSlider*			m_pMP3Slider;
	CLabeledCommandComboBox*	m_pSoundQualityCombo;
};
