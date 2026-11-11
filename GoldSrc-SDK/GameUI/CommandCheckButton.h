//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "CheckButton.h"

class CCommandCheckButton : public vgui2::CheckButton
{
public:
	CCommandCheckButton( vgui2::Panel* parent, const char* panelName, const char* text,
		const char* downCommand, const char* upCommand );
	~CCommandCheckButton() override;

	void SetSelected( bool state ) override;

private:
	char*	m_pszDown;
	char*	m_pszUp;
};
