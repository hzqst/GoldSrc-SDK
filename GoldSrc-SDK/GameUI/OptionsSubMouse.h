//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyPage.h"

class CCvarNegateCheckButton;
class CCvarToggleCheckButton;
class CCvarSlider;

namespace vgui2
{
class TextEntry;
}

class COptionsSubMouse : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_NAMED( COptionsSubMouse, vgui2::PropertyPage, "vgui::PropertyPage" );

public:
	COptionsSubMouse( vgui2::Panel* parent );
	~COptionsSubMouse() override;

	void OnResetData() override;
	void OnApplyChanges() override;

protected:
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

private:
	MESSAGE_FUNC_PTR( OnControlModified, "ControlModified", panel );
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	void UpdateSensitivityLabel();

	CCvarNegateCheckButton*	m_pReverseMouseCheckBox;
	CCvarToggleCheckButton*	m_pLowLatencyCheckBox;
	CCvarToggleCheckButton*	m_pMouseFilterCheckBox;
	CCvarToggleCheckButton*	m_pMouseRawInputCheckBox;
	CCvarSlider*			m_pMouseSensitivitySlider;
	vgui2::TextEntry*		m_pMouseSensitivityLabel;
	CCvarToggleCheckButton*	m_pAutoAimCheckBox;
};
