//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "utlvector.h"
#include "../external/steamworks/isteammatchmaking.h"
#include "SteamBridgeClient.h"
#include "Frame.h"
#include "vgui/MessageMap.h"

class CDialogAddServer;
class IGameList;
class gameserveritem_t;

namespace vgui2
{
class Button;
class IScheme;
class ListPanel;
class Panel;
class PropertySheet;
class TextEntry;
} // namespace vgui2

//-----------------------------------------------------------------------------
// Purpose: Dialog which lets the user add a server by IP address
//-----------------------------------------------------------------------------
class CDialogAddServer : public vgui2::Frame, public ISteamMatchmakingPingResponse
{
public:
	DECLARE_CLASS_SIMPLE_NAMED( CDialogAddServer, vgui2::Frame, "vgui::Frame" );

	CDialogAddServer( vgui2::Panel *parent, IGameList *gameList );
	virtual ~CDialogAddServer();

	// ISteamMatchmakingPingResponse
	virtual void ServerResponded( gameserveritem_t &server );
	virtual void ServerFailedToRespond();

	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );

	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

private:
	virtual void OnCommand( const char *command );

	void OnOK();
	void TestServers();

	MESSAGE_FUNC( OnTextChanged, "TextChanged" );

	IGameList *m_pGameList;

	vgui2::Button *m_pTestServersButton;
	vgui2::Button *m_pAddServerButton;
	vgui2::Button *m_pAddSelectedServerButton;

	vgui2::PropertySheet *m_pTabPanel;
	vgui2::TextEntry *m_pTextEntry;
	vgui2::ListPanel *m_pDiscoveredGames;
	int m_OriginalHeight;
	CUtlVector<gameserveritem_t> m_Servers;
	CUtlVector<int> m_Queries;
};
