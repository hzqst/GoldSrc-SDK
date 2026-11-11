//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyDialog.h"

class CCreateMultiplayerGameServerPage;
class CCreateMultiplayerGameGameplayPage;
class CCreateMultiplayerGameBotPage;
class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: dialog for launching a listenserver
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameDialog : public vgui2::PropertyDialog
{
	typedef vgui2::PropertyDialog BaseClass;

public:
	CCreateMultiplayerGameDialog( vgui2::Panel* parent );
	~CCreateMultiplayerGameDialog() override;

protected:
	void OnOK() override;
	void OnClose() override;
	void SetTitle( const char* title, bool surfaceTitle ) override;

private:
	CCreateMultiplayerGameServerPage*		m_pServerPage;
	CCreateMultiplayerGameGameplayPage*	m_pGameplayPage;
	CCreateMultiplayerGameBotPage*		m_pBotPage;
	bool								m_bBotsEnabled;
	// for loading/saving bot config
	KeyValues*							m_pBotSavedData;
};
