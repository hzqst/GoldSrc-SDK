//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "PropertyPage.h"
#include "utlrbtree.h"
#include "utlsymbol.h"

class CCvarToggleCheckButton;

bool CaselessStringLessThan( const CUtlSymbol& left, const CUtlSymbol& right );

namespace vgui2
{
class CheckButton;
class ComboBox;
class TextEntry;
}

namespace vgui = vgui2;

class CCreateMultiplayerGameServerPage : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( CCreateMultiplayerGameServerPage, vgui::PropertyPage );

public:
	CCreateMultiplayerGameServerPage( vgui2::Panel* parent, const char* name );
	~CCreateMultiplayerGameServerPage() override;

	const char* GetMapName();
	void SetMap( const char* mapName );
	bool IsRandomMapSelected();
	void SetBotQuota( int quota );
	void SetBotsEnabled( bool enabled );
	int GetBotQuota();
	bool GetBotsEnabled();
	void SetSteamNetworkingEnabled( bool enabled );
	bool GetSteamNetworkingEnabled();

protected:
	void OnApplyChanges() override;

private:
	void LoadMapList();
	void LoadMaps( const char* pathID );
	MESSAGE_FUNC( OnCheckButtonChecked, "CheckButtonChecked" );

	vgui2::ComboBox*		m_pMapList;
	vgui2::TextEntry*		m_pBotQuotaCombo;
	vgui2::CheckButton*		m_pEnableBotsCheck;
	vgui2::CheckButton*		m_pEnableSteamNetworkingCheck;
	CCvarToggleCheckButton*	m_pEnableTutorCheck;

	enum
	{
		DATA_STR_LENGTH = 64
	};

	char	m_szHostName[DATA_STR_LENGTH];
	char	m_szPassword[DATA_STR_LENGTH];
	char	m_szMapName[DATA_STR_LENGTH];
	int		m_iMaxPlayers;

	CUtlRBTree<CUtlSymbol, short>	m_MapNames;
};
