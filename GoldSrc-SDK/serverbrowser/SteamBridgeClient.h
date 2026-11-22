#pragma once

#include "../external/steamworks/isteammatchmaking.h"
#include "../external/steamworks/isteamfriends.h"
#include "../external/steamworks/matchmakingtypes.h"
#include "../external/steamworks/steamclientpublic.h"

// The server browser uses the Steam client session that the HLND2T engine opens through
// SteamAPIBridge (MetaHookSv/SteamAPIBridge) host mode: it neither initializes Steam nor runs
// its callbacks, and it links no steam_api. These objects keep the Steamworks names of the calls
// the server browser makes; without a client session each returns Steam's failure value.
class CSteamBridgeMatchmakingServers
{
public:
	HServerListRequest RequestInternetServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse );
	HServerListRequest RequestLANServerList( AppId_t iApp, ISteamMatchmakingServerListResponse *pRequestServersResponse );
	HServerListRequest RequestFriendsServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse );
	HServerListRequest RequestFavoritesServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse );
	HServerListRequest RequestHistoryServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse );
	HServerListRequest RequestSpectatorServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse );
	void ReleaseRequest( HServerListRequest hServerListRequest );

	// The details stay valid, and are refreshed by each call, until the request is released.
	gameserveritem_t *GetServerDetails( HServerListRequest hRequest, int iServer );
	void CancelQuery( HServerListRequest hRequest );
	void RefreshQuery( HServerListRequest hRequest );
	bool IsRefreshing( HServerListRequest hRequest );
	void RefreshServer( HServerListRequest hRequest, int iServer );

	HServerQuery PingServer( uint32 unIP, uint16 usPort, ISteamMatchmakingPingResponse *pRequestServersResponse );
	HServerQuery PlayerDetails( uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse *pRequestServersResponse );
	void CancelServerQuery( HServerQuery hServerQuery );

private:
	HServerListRequest RequestList( int nKind, AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse );
};

class CSteamBridgeMatchmaking
{
public:
	int GetFavoriteGameCount();
	int AddFavoriteGame( AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags, uint32 rTime32LastPlayedOnServer );
	bool RemoveFavoriteGame( AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags );
};

class CSteamBridgeFriends
{
public:
	// The name stays valid until the next call.
	const char *GetFriendPersonaName( CSteamID steamIDFriend );
	bool GetFriendGamePlayed( CSteamID steamIDFriend, FriendGameInfo_t *pFriendGameInfo );
};

class CSteamBridgeApps
{
public:
	bool BIsVACBanned();
};

CSteamBridgeMatchmakingServers *SteamBridgeMatchmakingServers();
CSteamBridgeMatchmaking *SteamBridgeMatchmaking();
CSteamBridgeFriends *SteamBridgeFriends();
CSteamBridgeApps *SteamBridgeApps();

// Delivers the Steam callbacks that SteamAPIBridge reports to the server browser
// (FavoritesListChanged_t and PersonaStateChange_t), as CCallback does for Steam callbacks.
class CSteamBridgeCallbackBase
{
public:
	explicit CSteamBridgeCallbackBase( int iCallback );
	virtual ~CSteamBridgeCallbackBase();
	virtual void Run( void *pvParam ) = 0;

	const int m_iCallback;
};

template <class T, class P> class CSteamBridgeCallback : public CSteamBridgeCallbackBase
{
public:
	typedef void ( T::*func_t )( P * );

	CSteamBridgeCallback( T *pObj, func_t func ) : CSteamBridgeCallbackBase( P::k_iCallback ), m_pObj( pObj ), m_Func( func ) {}
	void Run( void *pvParam ) override { ( m_pObj->*m_Func )( static_cast<P *>( pvParam ) ); }

private:
	T *m_pObj;
	func_t m_Func;
};

// Subscribes the server browser to the SteamAPIBridge matchmaking events, which the engine
// delivers while it runs the Steam callbacks; disconnect before the module unloads.
void SteamBridgeEvents_Connect();
void SteamBridgeEvents_Disconnect();
