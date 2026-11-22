#include "SteamBridgeClient.h"

#include <SteamAPIBridge.h>
#include <map>
#include <string.h>
#include <vector>

namespace
{
CSteamBridgeMatchmakingServers g_MatchmakingServers;
CSteamBridgeMatchmaking g_Matchmaking;
CSteamBridgeFriends g_Friends;
CSteamBridgeApps g_Apps;

// Details that GetServerDetails returned, per request and server, until the request is released.
std::map<HServerListRequest, std::map<int, gameserveritem_t>> g_ServerDetails;
char g_szFriendPersonaName[128];

std::vector<CSteamBridgeCallbackBase *> &Callbacks()
{
	static std::vector<CSteamBridgeCallbackBase *> callbacks;
	return callbacks;
}

template <size_t Size> void CopyString( char ( &pchDest )[Size], const char *pchSource )
{
	strncpy( pchDest, pchSource, Size - 1 );
	pchDest[Size - 1] = 0;
}

void CopyServer( gameserveritem_t &server, const SteamBridgeServerItem &item )
{
	server = gameserveritem_t();
	server.m_NetAdr.Init( item.ip, item.queryPort, item.connectionPort );
	server.m_nPing = item.ping;
	server.m_bHadSuccessfulResponse = item.hadSuccessfulResponse != 0;
	server.m_bDoNotRefresh = item.doNotRefresh != 0;
	CopyString( server.m_szGameDir, item.gameDir );
	CopyString( server.m_szMap, item.map );
	CopyString( server.m_szGameDescription, item.description );
	server.m_nAppID = item.appID;
	server.m_nPlayers = item.players;
	server.m_nMaxPlayers = item.maxPlayers;
	server.m_nBotPlayers = item.botPlayers;
	server.m_bPassword = item.password != 0;
	server.m_bSecure = item.secure != 0;
	server.m_ulTimeLastPlayed = item.timeLastPlayed;
	server.m_nServerVersion = item.serverVersion;
	server.SetName( item.name );
	CopyString( server.m_szGameTags, item.tags );
	server.m_steamID = CSteamID( (uint64)item.steamID );
}

SteamBridgeServerListHandle ToHandle( HServerListRequest hRequest )
{
	return static_cast<SteamBridgeServerListHandle>( hRequest );
}

void STEAMBRIDGE_CALL OnServerList( void *user, const SteamBridgeServersEvent *event )
{
	ISteamMatchmakingServerListResponse *pResponse = static_cast<ISteamMatchmakingServerListResponse *>( user );
	HServerListRequest hRequest = event->request;
	switch ( event->kind )
	{
	case SB_SERVERS_LIST_RESPONDED:
		pResponse->ServerResponded( hRequest, event->index );
		break;
	case SB_SERVERS_LIST_FAILED:
		pResponse->ServerFailedToRespond( hRequest, event->index );
		break;
	case SB_SERVERS_LIST_COMPLETE:
		pResponse->RefreshComplete( hRequest, (EMatchMakingServerResponse)event->response );
		break;
	}
}

void STEAMBRIDGE_CALL OnPing( void *user, const SteamBridgeServersEvent *event )
{
	ISteamMatchmakingPingResponse *pResponse = static_cast<ISteamMatchmakingPingResponse *>( user );
	if ( event->kind == SB_SERVERS_PING_RESPONDED )
	{
		gameserveritem_t server;
		CopyServer( server, event->server );
		pResponse->ServerResponded( server );
	}
	else if ( event->kind == SB_SERVERS_PING_FAILED )
	{
		pResponse->ServerFailedToRespond();
	}
}

void STEAMBRIDGE_CALL OnPlayers( void *user, const SteamBridgeServersEvent *event )
{
	ISteamMatchmakingPlayersResponse *pResponse = static_cast<ISteamMatchmakingPlayersResponse *>( user );
	switch ( event->kind )
	{
	case SB_SERVERS_PLAYER:
		pResponse->AddPlayerToList( event->player, event->score, event->seconds );
		break;
	case SB_SERVERS_PLAYERS_FAILED:
		pResponse->PlayersFailedToRespond();
		break;
	case SB_SERVERS_PLAYERS_COMPLETE:
		pResponse->PlayersRefreshComplete();
		break;
	}
}

HServerQuery ToQuery( SteamBridgeStatus status, uint32_t query )
{
	return status == SB_OK ? (HServerQuery)query : HSERVERQUERY_INVALID;
}

void Dispatch( int iCallback, void *pvParam )
{
	// A callback may destroy its own or another subscriber, so run the subscribers of a snapshot
	// that are still subscribed.
	std::vector<CSteamBridgeCallbackBase *> snapshot = Callbacks();
	for ( CSteamBridgeCallbackBase *pCallback : snapshot )
	{
		const std::vector<CSteamBridgeCallbackBase *> &current = Callbacks();
		bool bSubscribed = false;
		for ( CSteamBridgeCallbackBase *pCurrent : current )
			bSubscribed = bSubscribed || pCurrent == pCallback;
		if ( bSubscribed && pCallback->m_iCallback == iCallback )
			pCallback->Run( pvParam );
	}
}

void STEAMBRIDGE_CALL OnMatchmaking( void *, const SteamBridgeMatchmakingEvent *event )
{
	if ( event->kind == SB_MATCHMAKING_FAVORITES_CHANGED )
	{
		FavoritesListChanged_t changed = {};
		changed.m_nIP = event->ip;
		changed.m_nQueryPort = event->queryPort;
		changed.m_nConnPort = event->connectionPort;
		changed.m_nAppID = event->appID;
		changed.m_nFlags = event->flags;
		changed.m_bAdd = event->add != 0;
		changed.m_unAccountId = event->accountID;
		Dispatch( FavoritesListChanged_t::k_iCallback, &changed );
	}
	else if ( event->kind == SB_MATCHMAKING_PERSONA_CHANGE )
	{
		PersonaStateChange_t changed = {};
		changed.m_ulSteamID = event->steamID;
		changed.m_nChangeFlags = event->changeFlags;
		Dispatch( PersonaStateChange_t::k_iCallback, &changed );
	}
}
} // namespace

HServerListRequest CSteamBridgeMatchmakingServers::RequestList( int nKind, AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
	// Steam reads the filters as an array through *ppchFilters.
	std::vector<SteamBridgeServerFilter> filters;
	for ( uint32 i = 0; ppchFilters && *ppchFilters && i < nFilters; ++i )
		filters.push_back( { ( *ppchFilters )[i].m_szKey, ( *ppchFilters )[i].m_szValue } );

	SteamBridgeServerListHandle request = NULL;
	if ( SteamBridge_ServersRequestList( nKind, iApp, filters.empty() ? NULL : filters.data(), (uint32_t)filters.size(), OnServerList, pRequestServersResponse, &request ) != SB_OK )
		return NULL;
	return request;
}

HServerListRequest CSteamBridgeMatchmakingServers::RequestInternetServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
	return RequestList( SB_SERVER_LIST_INTERNET, iApp, ppchFilters, nFilters, pRequestServersResponse );
}

HServerListRequest CSteamBridgeMatchmakingServers::RequestLANServerList( AppId_t iApp, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
	return RequestList( SB_SERVER_LIST_LAN, iApp, NULL, 0, pRequestServersResponse );
}

HServerListRequest CSteamBridgeMatchmakingServers::RequestFriendsServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
	return RequestList( SB_SERVER_LIST_FRIENDS, iApp, ppchFilters, nFilters, pRequestServersResponse );
}

HServerListRequest CSteamBridgeMatchmakingServers::RequestFavoritesServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
	return RequestList( SB_SERVER_LIST_FAVORITES, iApp, ppchFilters, nFilters, pRequestServersResponse );
}

HServerListRequest CSteamBridgeMatchmakingServers::RequestHistoryServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
	return RequestList( SB_SERVER_LIST_HISTORY, iApp, ppchFilters, nFilters, pRequestServersResponse );
}

HServerListRequest CSteamBridgeMatchmakingServers::RequestSpectatorServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
	return RequestList( SB_SERVER_LIST_SPECTATOR, iApp, ppchFilters, nFilters, pRequestServersResponse );
}

void CSteamBridgeMatchmakingServers::ReleaseRequest( HServerListRequest hServerListRequest )
{
	SteamBridge_ServersReleaseRequest( ToHandle( hServerListRequest ) );
	g_ServerDetails.erase( hServerListRequest );
}

gameserveritem_t *CSteamBridgeMatchmakingServers::GetServerDetails( HServerListRequest hRequest, int iServer )
{
	SteamBridgeServerItem item;
	if ( SteamBridge_ServersGetDetails( ToHandle( hRequest ), iServer, &item ) != SB_OK )
		return NULL;
	gameserveritem_t &server = g_ServerDetails[hRequest][iServer];
	CopyServer( server, item );
	return &server;
}

void CSteamBridgeMatchmakingServers::CancelQuery( HServerListRequest hRequest )
{
	SteamBridge_ServersCancelQuery( ToHandle( hRequest ) );
}

void CSteamBridgeMatchmakingServers::RefreshQuery( HServerListRequest hRequest )
{
	SteamBridge_ServersRefreshQuery( ToHandle( hRequest ) );
}

bool CSteamBridgeMatchmakingServers::IsRefreshing( HServerListRequest hRequest )
{
	uint32_t refreshing = 0;
	SteamBridge_ServersIsRefreshing( ToHandle( hRequest ), &refreshing );
	return refreshing != 0;
}

void CSteamBridgeMatchmakingServers::RefreshServer( HServerListRequest hRequest, int iServer )
{
	SteamBridge_ServersRefreshServer( ToHandle( hRequest ), iServer );
}

HServerQuery CSteamBridgeMatchmakingServers::PingServer( uint32 unIP, uint16 usPort, ISteamMatchmakingPingResponse *pRequestServersResponse )
{
	uint32_t query = 0;
	return ToQuery( SteamBridge_ServersPing( unIP, usPort, OnPing, pRequestServersResponse, &query ), query );
}

HServerQuery CSteamBridgeMatchmakingServers::PlayerDetails( uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse *pRequestServersResponse )
{
	uint32_t query = 0;
	return ToQuery( SteamBridge_ServersPlayerDetails( unIP, usPort, OnPlayers, pRequestServersResponse, &query ), query );
}

void CSteamBridgeMatchmakingServers::CancelServerQuery( HServerQuery hServerQuery )
{
	if ( hServerQuery != HSERVERQUERY_INVALID )
		SteamBridge_ServersCancelServerQuery( (uint32_t)hServerQuery );
}

int CSteamBridgeMatchmaking::GetFavoriteGameCount()
{
	uint32_t count = 0;
	SteamBridge_MatchmakingGetFavoriteCount( &count );
	return (int)count;
}

int CSteamBridgeMatchmaking::AddFavoriteGame( AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags, uint32 rTime32LastPlayedOnServer )
{
	// Steam's entry index is not reported; the server browser does not use it.
	return SteamBridge_MatchmakingAddFavorite( nAppID, nIP, nConnPort, nQueryPort, unFlags, rTime32LastPlayedOnServer ) == SB_OK ? 0 : -1;
}

bool CSteamBridgeMatchmaking::RemoveFavoriteGame( AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags )
{
	return SteamBridge_MatchmakingRemoveFavorite( nAppID, nIP, nConnPort, nQueryPort, unFlags ) == SB_OK;
}

const char *CSteamBridgeFriends::GetFriendPersonaName( CSteamID steamIDFriend )
{
	// Steam returns an empty name for an unknown user.
	SteamBridge_FriendsGetPersonaName( steamIDFriend.ConvertToUint64(), g_szFriendPersonaName, sizeof( g_szFriendPersonaName ) );
	return g_szFriendPersonaName;
}

bool CSteamBridgeFriends::GetFriendGamePlayed( CSteamID steamIDFriend, FriendGameInfo_t *pFriendGameInfo )
{
	SteamBridgeFriendGame game;
	if ( SteamBridge_FriendsGetGamePlayed( steamIDFriend.ConvertToUint64(), &game ) != SB_OK )
		return false;
	pFriendGameInfo->m_gameID = CGameID( (uint64)game.gameID );
	pFriendGameInfo->m_unGameIP = game.ip;
	pFriendGameInfo->m_usGamePort = game.gamePort;
	pFriendGameInfo->m_usQueryPort = game.queryPort;
	pFriendGameInfo->m_steamIDLobby = CSteamID( (uint64)game.lobbySteamID );
	return true;
}

bool CSteamBridgeApps::BIsVACBanned()
{
	uint32_t banned = 0;
	SteamBridge_AppsIsVACBanned( &banned );
	return banned != 0;
}

CSteamBridgeMatchmakingServers *SteamBridgeMatchmakingServers()
{
	return &g_MatchmakingServers;
}

CSteamBridgeMatchmaking *SteamBridgeMatchmaking()
{
	return &g_Matchmaking;
}

CSteamBridgeFriends *SteamBridgeFriends()
{
	return &g_Friends;
}

CSteamBridgeApps *SteamBridgeApps()
{
	return &g_Apps;
}

CSteamBridgeCallbackBase::CSteamBridgeCallbackBase( int iCallback ) : m_iCallback( iCallback )
{
	Callbacks().push_back( this );
}

CSteamBridgeCallbackBase::~CSteamBridgeCallbackBase()
{
	std::vector<CSteamBridgeCallbackBase *> &callbacks = Callbacks();
	for ( size_t i = 0; i < callbacks.size(); ++i )
	{
		if ( callbacks[i] == this )
		{
			callbacks.erase( callbacks.begin() + i );
			break;
		}
	}
}

void SteamBridgeEvents_Connect()
{
	SteamBridge_HostSetMatchmakingCallback( OnMatchmaking, NULL );
}

void SteamBridgeEvents_Disconnect()
{
	SteamBridge_HostSetMatchmakingCallback( NULL, NULL );
}
