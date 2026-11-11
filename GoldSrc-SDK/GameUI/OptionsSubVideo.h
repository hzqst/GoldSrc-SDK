//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyPage.h"

class CCvarSlider;
class CCvarToggleCheckButton;

namespace vgui2
{
class CheckButton;
class ComboBox;
}

class COptionsSubVideo : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_NAMED( COptionsSubVideo, vgui2::PropertyPage, "vgui::PropertyPage" );

public:
	COptionsSubVideo( vgui2::Panel* parent );
	~COptionsSubVideo() override;
	void OnResetData() override;
	void OnApplyChanges() override;

private:
	struct CVidSettings
	{
		int		w;
		int		h;
		int		bpp;
		int		windowed;
		char	renderer[128];
	};

	CVidSettings	m_OrigSettings;
	CVidSettings	m_CurrentSettings;

	void GetVidSettings();
	void RevertVidSettings();
	void ApplyVidSettings( bool forceRefresh );
	void SetCurrentRendererComboItem();
	void SetCurrentResolutionComboItem();

	MESSAGE_FUNC( OnDataChanged, "ControlModified" );
	MESSAGE_FUNC_PARAMS( OnButtonChecked, "CheckButtonChecked", data );
	_MessageFuncCommon( OnTextChanged, "TextChanged",
		(void ( vgui2::Panel::* )( vgui2::Panel*, const char* ))&COptionsSubVideo::OnTextChanged, 2,
		vgui2::DATATYPE_PTR, "panel", vgui2::DATATYPE_CONSTCHARPTR, "text" );
	virtual void OnTextChanged( vgui2::Panel* panel, const char* text );

	void SetRendererComboItem();
	void PrepareResolutionList( bool disallowBadResolutions );

	vgui2::ComboBox*			m_pMode;
	vgui2::ComboBox*			m_pRenderer;
	vgui2::CheckButton*		m_pWindowed;
	vgui2::ComboBox*			m_pAspectRatio;
	CCvarToggleCheckButton*	m_pVsync;
	CCvarToggleCheckButton*	m_pWidescreenFOV;
	CCvarToggleCheckButton*	m_pTextureFiltering;
	CCvarToggleCheckButton*	m_pUseShaders;
	CCvarSlider*			m_pBrightnessSlider;
	CCvarSlider*			m_pGammaSlider;
	char					m_pszRenderNames[3][32];
	char					m_pszAspectName[2][32];
	int						m_iStartRenderer;
	int						m_iStartResolution;
	bool					m_bStartWidescreen;
};
