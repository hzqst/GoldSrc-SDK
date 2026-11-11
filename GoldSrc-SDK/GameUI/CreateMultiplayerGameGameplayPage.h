//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyPage.h"

class CDescription;
class CPanelListPanel;
class mpcontrol_t;

//-----------------------------------------------------------------------------
// Purpose: server options page of the create game server dialog
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameGameplayPage : public vgui2::PropertyPage
{
public:
	CCreateMultiplayerGameGameplayPage( vgui2::Panel* parent, const char* name );
	~CCreateMultiplayerGameGameplayPage() override;

	int GetMaxPlayers();
	const char* GetPassword();
	const char* GetHostName();

protected:
	void OnApplyChanges() override;
	void GatherCurrentValues();
	void LoadGameOptionsList();
	const char* GetValue( const char* cvarName, const char* defaultValue );

private:
	CDescription*		m_pDescription;
	mpcontrol_t*		m_pList;
	CPanelListPanel*		m_pOptionsList;
};
