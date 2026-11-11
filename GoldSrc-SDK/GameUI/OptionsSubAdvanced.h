//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyPage.h"

class CContentControlDialog;
class CCvarToggleCheckButton;

namespace vgui2
{
class CheckButton;
}

class COptionsSubAdvanced : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_NAMED( COptionsSubAdvanced, vgui2::PropertyPage, "vgui::PropertyPage" );

public:
	COptionsSubAdvanced( vgui2::Panel* parent );
	~COptionsSubAdvanced() override;

	void OnResetData() override;
	void OnApplyChanges() override;
	MESSAGE_FUNC_PARAMS( OnButtonChecked, "CheckButtonChecked", data );
	void OnCommand( const char* command ) override;
	void OnOpenContentControlDialog();

private:
	MESSAGE_FUNC( OnDataChanged, "ControlModified" );
	MESSAGE_FUNC( OnContentControlClose, "ContentControlClose" );

	CContentControlDialog*	m_pContentControlDialog;
	vgui2::CheckButton*		m_pContentCheckButton;
	vgui2::CheckButton*		m_pOriginalModels;
	vgui2::CheckButton*		m_pAddonsFolder;
	CCvarToggleCheckButton*	m_pDetailTextures;
};
