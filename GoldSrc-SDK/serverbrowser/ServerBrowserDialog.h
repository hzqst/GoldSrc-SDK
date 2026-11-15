//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "utlvector.h"
#include "../external/steamworks/steamclientpublic.h"
#include "../external/steamworks/matchmakingtypes.h"
#include "tier0/platform.h"
#include "Frame.h"
#include "vgui/MessageMap.h"

class CDialogGameInfo;
class CFavoriteGames;
class CFriendsGames;
class CHistoryGames;
class CInternetGames;
class CLanGames;
class CServerBrowserDialog;
class CServerContextMenu;
class CSpectateGames;
class IGameList;
class IRunGameEngine;
class KeyValues;

namespace vgui2
{
class Label;
class Panel;
class PropertySheet;
} // namespace vgui2

extern IRunGameEngine *g_pRunGameEngine;

//-----------------------------------------------------------------------------
// Purpose: Root server browser dialog; holds the game list pages
//-----------------------------------------------------------------------------
class CServerBrowserDialog : public vgui2::Frame
{
public:
	DECLARE_CLASS_SIMPLE_NAMED( CServerBrowserDialog, vgui2::Frame, "vgui::Frame" );

	// Construction/destruction
	CServerBrowserDialog( vgui2::Panel *parent );
	virtual ~CServerBrowserDialog();

	void Initialize();

	// displays the dialog, moves it into focus, updates if it has to
	void Open();

	virtual void OnClose();

	// gets server info
	gameserveritem_t *GetServer( unsigned int serverID );

	// updates status text at bottom of window
	void UpdateStatusText( const char *fmt, ... );
	void UpdateStatusText( wchar_t *unicode );

	// context menu access
	CServerContextMenu *GetContextMenu( vgui2::Panel *pPanel );

	// returns a pointer to a static instance of this dialog
	// valid for use only in sort functions
	static CServerBrowserDialog *GetInstance();

	// Adds a server to the list of favorites
	void AddServerToFavorites( gameserveritem_t &server );

	// begins the process of joining a server from a game list
	// the game info dialog it opens will also update the game list
	CDialogGameInfo *JoinGame( IGameList *gameList, unsigned int serverIndex );

	// joins a game by a specified IP, not attached to any game list
	CDialogGameInfo *JoinGame( int serverIP, int serverPort );

	// opens a game info dialog from a game list
	CDialogGameInfo *OpenGameInfoDialog( IGameList *gameList, unsigned int serverIndex );

	// opens a game info dialog by a specified IP, not attached to any game list
	CDialogGameInfo *OpenGameInfoDialog( int serverIP, uint16 connPort, uint16 queryPort );

	// closes all the game info dialogs
	void CloseAllGameInfoDialogs();
	CDialogGameInfo *GetDialogGameInfoForFriend( uint64 ulSteamIDFriend );

	// accessor to the filter save data
	KeyValues *GetFilterSaveData( const char *filterSet );

	// gets the name of the mod directory we're restricted to accessing, NULL if none
	const char *GetActiveModName();
	CGameID &GetActiveAppID();
	const char *GetActiveGameName();

	// load/saves filter & favorites settings from disk
	void LoadUserData();
	void SaveUserData();

	// forces the currently active page to refresh
	void RefreshCurrentPage();

	virtual gameserveritem_t *GetCurrentConnectedServer();

private:
	// current game list change
	MESSAGE_FUNC( OnGameListChanged, "PageChanged" );
	void ReloadFilterSettings();

	MESSAGE_FUNC_PARAMS( OnActiveGameName, "ActiveGameName", pKV );
	MESSAGE_FUNC_PARAMS( OnConnectToGame, "ConnectedToGame", pMessageValues );
	MESSAGE_FUNC( OnDisconnectFromGame, "DisconnectedFromGame" );

	virtual bool GetDefaultScreenPosition( int &x, int &y, int &wide, int &tall );
	virtual void ActivateBuildMode();

	// list of all open game info dialogs
	CUtlVector<vgui2::DHANDLE<CDialogGameInfo>> m_GameInfoDialogs;

	// pointer to current game list
	IGameList *m_pGameList;

	// Status text
	vgui2::Label *m_pStatusLabel;

	// property sheet
	vgui2::PropertySheet *m_pTabPanel;
	CFavoriteGames *m_pFavorites;
	CHistoryGames *m_pHistory;
	CInternetGames *m_pInternetGames;
	CSpectateGames *m_pSpectateGames;
	CLanGames *m_pLanGames;
	CFriendsGames *m_pFriendsGames;

	KeyValues *m_pSavedData;
	KeyValues *m_pFilterData;

	// context menu
	CServerContextMenu *m_pContextMenu;

	// active game
	char m_szGameName[128];
	char m_szModDir[128];
	CGameID m_iLimitAppID;

	// currently connected game
	bool m_bCurrentlyConnected;
	gameserveritem_t m_CurrentConnection;
};

// singleton accessor
CServerBrowserDialog &ServerBrowserDialog( void );

// Used by the LAN tab and the add server dialog when trying to find servers without having
// been given any ports to look for servers on.
void GetMostCommonQueryPorts( CUtlVector<unsigned short> &ports );

// Converts a host order IPv4 address to network byte order, as htonl() does on x86
inline uint32 IPHostToNetwork( uint32 unIP )
{
	return ( ( unIP & 0xFF ) << 24 ) | ( ( unIP & 0xFF00 ) << 8 ) | ( ( unIP >> 8 ) & 0xFF00 ) | ( unIP >> 24 );
}
