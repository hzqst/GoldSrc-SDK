//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Base class for all the server list pages
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "utlmap.h"
#include "utlvector.h"
#include "igamelist.h"
#include "../external/steamworks/isteammatchmaking.h"
#include "../external/steamworks/matchmakingtypes.h"
#include "../external/steamworks/steam_api_common.h"
#include "../external/steamworks/steamclientpublic.h"
#include "SteamBridgeClient.h"
#include "tier0/platform.h"
#include "vgui/ISurface.h"
#include "vgui/KeyCode.h"
#include "ListPanel.h"
#include "vgui/MessageMap.h"
#include "PropertyPage.h"

class CBaseGamesPage;
class CGameListPanel;
class KeyValues;
class netadr_t;

namespace vgui2
{
class Button;
class CheckButton;
class ComboBox;
class IScheme;
class Label;
class Panel;
class TextEntry;
class ToggleButton;
} // namespace vgui2

enum EMatchMakingType
{
	eInternetServer = 0,
	eLANServer = 1,
	eFriendsServer = 2,
	eFavoritesServer = 3,
	eHistoryServer = 4,
	eSpectatorServer = 5,
	eInvalidServer = 6,
};

//-----------------------------------------------------------------------------
// Purpose: Acts like a regular ListPanel but forwards enter key presses
// to its outer control.
//-----------------------------------------------------------------------------
class CGameListPanel : public vgui2::ListPanel
{
public:
	DECLARE_CLASS_SIMPLE_NAMED( CGameListPanel, vgui2::ListPanel, "vgui::ListPanel" );
	CGameListPanel( CBaseGamesPage *pOuter, const char *pName );
	virtual void OnKeyCodeTyped( vgui2::KeyCode code );
private:
	CBaseGamesPage *m_pOuter;
};

//-----------------------------------------------------------------------------
// Purpose: Base property page for all the games lists (internet/favorites/lan/etc.)
//-----------------------------------------------------------------------------
class CBaseGamesPage : public vgui2::PropertyPage, public IGameList, public ISteamMatchmakingServerListResponse, public ISteamMatchmakingPingResponse
{
public:
	DECLARE_CLASS_SIMPLE_NAMED( CBaseGamesPage, vgui2::PropertyPage, "vgui::PropertyPage" );
	CBaseGamesPage( vgui2::Panel *parent, const char *name, EMatchMakingType eType, const char *pCustomResFilename );
	virtual ~CBaseGamesPage();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	virtual serverdisplay_t *GetServer( int serverID );
	virtual gameserveritem_t *GetServerInfo( int serverID );
	uint32 GetServerFilters( MatchMakingKeyValuePair_t **pFilters );
	virtual void SetRefreshing( bool state );
	virtual void LoadFilterSettings();
	virtual bool OnGameListEnterPressed();
	int GetSelectedItemsCount();
	MESSAGE_FUNC( OnAddToFavorites, "AddToFavorites" );
	MESSAGE_FUNC_UINT64( OnLoadModList, "LoadModList", nAppID );
	virtual void StartRefresh();
	virtual void StopRefresh();
	void OnFavoritesMsg( FavoritesListChanged_t *pFavListChanged );
	HServerListRequest GetRequestHandle();
protected:
	static int PasswordCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );
	static int BotsCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );
	static int SecureCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );
	static int ServerNameCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );
	static int IPAddressCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );
	static int GameCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );
	static int PlayersCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );
	static int MapCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );
	static int PingCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );
	static int LastPlayedCompare( vgui2::ListPanel *pPanel, const vgui2::ListPanelItem &p1, const vgui2::ListPanelItem &p2 );
	virtual void OnCommand( const char *command );
	virtual void OnKeyCodePressed( vgui2::KeyCode code );
	virtual int GetRegionCodeToFilter();
public:
	CSteamBridgeCallback<CBaseGamesPage, FavoritesListChanged_t> m_CallbackFavoritesMsg;
protected:
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );
	void ApplyGameFilters();
	void UpdateStatus();
	virtual void ServerResponded( HServerListRequest hRequest, int iServer );
	virtual void ServerResponded( int iServer, gameserveritem_t *pServerItem );
	virtual void ServerFailedToRespond( HServerListRequest hRequest, int iServer );
	virtual void RefreshComplete( HServerListRequest, EMatchMakingServerResponse ) = 0;
	virtual void ServerResponded( gameserveritem_t &server );
	virtual void ServerFailedToRespond();
	void RemoveServer( serverdisplay_t &server );
	virtual bool BShowServer( serverdisplay_t &server );
	void ClearServerList();
	virtual bool CheckPrimaryFilters( gameserveritem_t &server );
	virtual bool CheckSecondaryFilters( gameserveritem_t &server );
	virtual int GetInvalidServerListID();
	virtual void OnSaveFilter( KeyValues * );
	virtual void OnLoadFilter( KeyValues *filter );
	void UpdateFilterSettings();
	CGameID &GetFilterAppID();
	virtual void GetNewServerList();
	virtual bool IsRefreshing();
	virtual void OnPageShow();
	virtual void OnPageHide();
	MESSAGE_FUNC( OnBeginConnect, "ConnectToServer" );
	MESSAGE_FUNC( OnViewGameInfo, "ViewGameInfo" );
	MESSAGE_FUNC_INT( OnRefreshServer, "RefreshServer", serverID );
private:
	void RequestServersResponse( int, EMatchMakingServerResponse, bool );
	void CreateFilters();
	void UpdateGameFilter();
protected:
	bool m_bAutoSelectFirstItemInGameList;
	CGameListPanel *m_pGameList;
	vgui2::Button *m_pConnect;
	vgui2::Button *m_pRefreshAll;
	vgui2::Button *m_pRefreshQuick;
	vgui2::Button *m_pAddServer;
	vgui2::Button *m_pAddCurrentServer;
	vgui2::Button *m_pAddToFavoritesButton;
	vgui2::ToggleButton *m_pFilter;
	CUtlMap<long long unsigned int, int, int> m_mapGamesFilterItem;
	CUtlMap<int, serverdisplay_t, int> m_mapServers;
	CUtlMap<netadr_t, int, int> m_mapServerIP;
	CUtlVector<MatchMakingKeyValuePair_t> m_vecServerFilters;
	int m_iServerRefreshCount;
	EMatchMakingType m_eMatchMakingType;
	HServerListRequest m_hServerRequest;
	CUtlVector<HServerQuery> m_vecPingQueries;
private:
	MESSAGE_FUNC_PTR_INT( OnButtonToggled, "ButtonToggled", panel, state );
	MESSAGE_FUNC_PTR_CHARPTR( OnTextChanged, "TextChanged", panel, text );
	void RecalculateFilterString();
	const char *m_pCustomResFilename;
	vgui2::ComboBox *m_pGameFilter;
	vgui2::TextEntry *m_pMapFilter;
	vgui2::ComboBox *m_pPingFilter;
	vgui2::ComboBox *m_pSecureFilter;
	vgui2::CheckButton *m_pNoFullServersFilterCheck;
	vgui2::CheckButton *m_pNoEmptyServersFilterCheck;
	vgui2::CheckButton *m_pNoPasswordFilterCheck;
	vgui2::CheckButton *m_pValidSteamAccountFilterCheck;
	vgui2::Label *m_pFilterString;
	char m_szComboAllText[64];
	KeyValues *m_pFilters;
	bool m_bFiltersVisible;
	vgui2::HFont m_hFont;
	char m_szGameFilter[32];
	char m_szMapFilter[32];
	int m_iPingFilter;
	bool m_bFilterNoFullServers;
	bool m_bFilterNoEmptyServers;
	bool m_bFilterNoPasswordedServers;
	bool m_bFilterValidSteamAccount;
	int m_iSecureFilter;
	CGameID m_iLimitToAppID;
};
