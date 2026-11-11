//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "CheckButton.h"

class CCvarNegateCheckButton : public vgui2::CheckButton
{
	DECLARE_CLASS_SIMPLE_NAMED( CCvarNegateCheckButton, vgui2::CheckButton, "vgui::CheckButton" );

public:
	CCvarNegateCheckButton( vgui2::Panel* parent, const char* panelName, const char* text, const char* cvarName );
	~CCvarNegateCheckButton() override;

	void SetSelected( bool state ) override;
	void Paint() override;
	void Reset();
	void ApplyChanges();
	bool HasBeenModified();

private:
	MESSAGE_FUNC( OnButtonChecked, "CheckButtonChecked" );

	char*	m_pszCvarName;
	bool	m_bStartState;
};
