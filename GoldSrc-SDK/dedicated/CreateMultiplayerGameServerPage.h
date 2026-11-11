//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "../vgui2/controls/Frame.h"
#include "../engine/utlvector.h"
#include "../tracker/common/server.h"

class CMainPanel;
class KeyValues;

namespace vgui2
{
class Button;
class CheckButton;
class ComboBox;
}

class CCreateMultiplayerGameServerPage : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE_NAMED( CCreateMultiplayerGameServerPage, vgui2::Frame, "vgui::Frame" );

public:
	CCreateMultiplayerGameServerPage( vgui2::Panel* parent, const char* name );
	~CCreateMultiplayerGameServerPage() override;

	void GetServerInfo( serveritem_t* server );
	void OnCommand( const char* command ) override;
	virtual void EnableModSelection();

private:
	friend class CMainPanel;

	enum
	{
		GAME_DIR_LENGTH = 64
	};

	struct GameDir_t
	{
		char gameDir[GAME_DIR_LENGTH];
		char name[GAME_DIR_LENGTH];
	};

	void LoadConfig();
	void LoadModList();
	void LoadMapList();
	int LoadMaps( const char* gameDir );
	bool GetLiblistValue( const char* filename, const char* key, char* value, int valueLength );

	vgui2::ComboBox*	m_pModList;
	vgui2::ComboBox*	m_pMapList;
	vgui2::ComboBox*	m_pPlayersList;
	vgui2::ComboBox*	m_pNetworkList;
	vgui2::Button*		m_pStartButton;
	vgui2::Button*		m_pCancelButton;
	vgui2::CheckButton*	m_pSecure;
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	serveritem_t		m_Server;
	char				m_szHostName[GAME_DIR_LENGTH];
	char				m_szRconPassword[GAME_DIR_LENGTH];
	char				m_szMapName[GAME_DIR_LENGTH];
	char				m_szMod[256];
	int					m_iMaxPlayers;
	int					m_iPort;
	int					m_iCurrentGame;
	CUtlVector<GameDir_t>	m_GameDirs;
	vgui2::Panel*		m_pParent;
	KeyValues*			m_pConfig;
};
