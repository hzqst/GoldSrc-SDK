//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "TextEntry.h"

class CCvarTextEntry : public vgui2::TextEntry
{
	DECLARE_CLASS_SIMPLE_NAMED( CCvarTextEntry, vgui2::TextEntry, "vgui::TextEntry" );

public:
	CCvarTextEntry( vgui2::Panel* parent, const char* panelName, const char* cvarName );
	~CCvarTextEntry() override;

	MESSAGE_FUNC( OnTextChanged, "TextChanged" );
	void ApplyChanges( bool immediate );
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void Reset();
	bool HasBeenModified();

private:
	char*	m_pszCvarName;
	char	m_pszStartValue[64];
};
