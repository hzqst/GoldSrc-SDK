//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Slider.h"

class CCvarSlider : public vgui2::Slider
{
	DECLARE_CLASS_SIMPLE_NAMED( CCvarSlider, vgui2::Slider, "vgui::Slider" );

public:
	CCvarSlider( vgui2::Panel* parent, const char* panelName, const char* caption, float minValue,
		float maxValue, const char* cvarName, bool allowOutOfRange );
	~CCvarSlider() override;

	void Paint() override;
	void ApplyChanges();
	float GetSliderValue();
	void SetSliderValue( float value );
	void Reset();
	bool HasBeenModified();

private:
	MESSAGE_FUNC( OnSliderMoved, "SliderMoved" );

	bool	m_bAllowOutOfRange;
	bool	m_bModifiedOnce;
	float	m_fStartValue;
	int		m_iStartValue;
	int		m_iLastSliderValue;
	float	m_fCurrentValue;
	char	m_szCvarName[64];
};
