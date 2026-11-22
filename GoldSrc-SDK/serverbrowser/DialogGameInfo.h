//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "netadr.h"
#include "../external/steamworks/isteammatchmaking.h"
#include "../external/steamworks/matchmakingtypes.h"
#include "../external/steamworks/steam_api_common.h"
#include "tier0/platform.h"
#include "Frame.h"
#include "vgui/MessageMap.h"

class CDialogGameInfo;
struct PersonaStateChange_t;
struct challenge_s;

namespace vgui2
{
class Button;
class Label;
class ListPanel;
struct ListPanelItem;
class Panel;
class RadioButton;
class ToggleButton;
} // namespace vgui2

struct challenge_s
{
	netadr_t addr;
	int challenge;
};

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CDialogGameInfo : public vgui2::Frame, public ISteamMatchmakingPlayersResponse, public ISteamMatchmakingPingResponse
{
public:
	DECLARE_CLASS_SIMPLE_NAMED( CDialogGameInfo, vgui2::Frame, "vgui::Frame" );

	CDialogGameInfo( vgui2::Panel *parent, int serverIP, int queryPort, unsigned short connectionPort, bool hltvOnly = false );
	virtual ~CDialogGameInfo();

	void Run( const char *titleName );
	void ChangeGame( int serverIP, int queryPort, unsigned short connectionPort );
	void SetFriend( uint64 ulSteamIDFriend );
	uint64 GetAssociatedFriend();

	// forces the dialog to attempt to connect to the server
	void Connect();

	// ISteamMatchmakingPingResponse
	virtual void ServerResponded( gameserveritem_t &server );
	virtual void ServerFailedToRespond();

	// ISteamMatchmakingPlayersResponse
	virtual void AddPlayerToList( const char *playerName, int score, float timePlayedSeconds );
	virtual void PlayersFailedToRespond();
	virtual void PlayersRefreshComplete();

	// called when the current refresh list is complete
	virtual void RefreshComplete( EMatchMakingServerResponse response );

	virtual void ClearPlayerList();
	virtual void SendPlayerQuery( uint32 unIP, uint16 usQueryPort );

protected:
	// message handlers
	MESSAGE_FUNC( OnConnect, "Connect" );
	MESSAGE_FUNC( OnRefresh, "Refresh" );
	MESSAGE_FUNC_PTR( OnButtonToggled, "ButtonToggled", panel );
	MESSAGE_FUNC_PTR( OnRadioButtonChecked, "RadioButtonChecked", panel );

	// response from the get password dialog
	MESSAGE_FUNC_CHARPTR( OnJoinServerWithPassword, "JoinServerWithPassword", password );

	MESSAGE_FUNC_INT_INT( OnConnectToGame, "ConnectedToGame", ip, port );

	// vgui overrides
	virtual void OnTick();
	virtual void PerformLayout();

private:
	void OnPersonaStateChange( PersonaStateChange_t *pPersonaStateChange );
	static int PlayerTimeColumnSortFunc( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );

	// methods
	void RequestInfo();
	void ConnectToServer();
	void ShowAutoRetryOptions( bool state );
	void ConstructConnectArgs( char *pchOptions, int cchOptions, const gameserveritem_t &server );
	void ApplyConnectCommand( const gameserveritem_t &server );

	CCallback<CDialogGameInfo, PersonaStateChange_t, false> m_CallbackPersonaStateChange;

	long m_iRequestRetry;	// time at which to retry the request

	vgui2::Button *m_pConnectButton;
	vgui2::Button *m_pCloseButton;
	vgui2::Button *m_pRefreshButton;
	vgui2::Label *m_pInfoLabel;
	vgui2::ToggleButton *m_pAutoRetry;
	vgui2::RadioButton *m_pAutoRetryAlert;
	vgui2::RadioButton *m_pAutoRetryJoin;
	vgui2::ListPanel *m_pPlayerList;

	// true if we should try connect to the server when it refreshes
	bool m_bConnecting;
	// Preserve the originating page's restriction through password prompts and retries.
	bool m_bHLTVOnly;

	// password, if entered
	char m_szPassword[64];

	// state
	bool m_bServerNotResponding;
	bool m_bServerFull;
	bool m_bShowAutoRetryToggle;
	bool m_bShowingExtendedOptions;
	uint64 m_SteamIDFriend;

	gameserveritem_t m_Server;
	HServerQuery m_hPingQuery;
	HServerQuery m_hPlayersQuery;
	bool m_bPlayerListUpdatePending;
};

extern "C" DLL_EXPORT bool JoiningSecureServerCall();

bool QueryLessFunc( const challenge_s &item1, const challenge_s &item2 );
