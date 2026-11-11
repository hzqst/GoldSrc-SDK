//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "CheckButton.h"

class CKeyToggleCheckButton : public vgui2::CheckButton
{
public:
	CKeyToggleCheckButton( vgui2::Panel* parent, const char* panelName, const char* text,
		const char* keyName, const char* commandName );
	~CKeyToggleCheckButton() override;

	void Reset();
	void ApplyChanges();
	bool HasBeenModified();
	void Paint() override;

private:
	char*	m_pszKeyName;
	char*	m_pszCmdName;
	bool	m_bStartValue;
};
