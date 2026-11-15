#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int SOCKET;
#endif
#include "isteamgameserver_c_api.h"
#include "steam_api.h"
#include "sv_steam3.h"
#include "server.h"
#include "client.h"
#include "../external/steamworks/isteamapps.h"
#include "../external/steamworks/isteamgameserver.h"
#include "../external/steamworks/isteaminput.h"
#include "../external/steamworks/isteamutils.h"
#include "../external/steamworks/steam_gameserver.h"
#define STEAMNETWORKINGSOCKETS_STEAMAPI
#include "../external/steamworks/isteamnetworkingsockets.h"
#include "../external/steamworks/isteamnetworkingutils.h"
#include "net.h"
#include "common.h"
#include "cvar.h"
#include "conprint.h"
#include "sys.h"
#include "cmd.h"
#include "mem.h"
#include "host.h"
#include "iengine.h"
#include "IGameUI.h"
#include "eiface.h"

#if defined( _WIN32 )
#include <cassert>
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <io.h>
#endif
#include <mutex>
#include <string>
#include <vector>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\sv_steam3.cpp" )

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

extern cvar_t sv_lan;
extern cvar_t sv_use_steam_networking;
extern cvar_t sv_visiblemaxplayers;
extern cvar_t iphostport;
extern cvar_t hostport;
extern cvar_t password;
extern qboolean gfNoMasterServer;
extern SOCKET ip_sockets[];
extern IGameUI* staticGameUIFuncs;
int PF_IsDedicatedServer();
int Host_GetMaxClients();
qboolean SV_FilterUser( USERID_t* userid );
int SV_CheckForDuplicateSteamID( client_t* client );
const char* CL_MapLocalizationString( const char* mapName );
void SV_DropClient( client_t* client, qboolean crash, const char* format, ... );
bool SteamIDFromSteam2String( const char* text, EUniverse universe, CSteamID* steamID );

struct SteamDebugOutput_t
{
	ESteamNetworkingSocketsDebugOutputType m_nType;
	std::string m_sMsg;
};

static std::vector<SteamDebugOutput_t> g_SteamDebugOutputQueue;
static std::mutex g_SteamDebugOutputMutex;

static void SteamNetworkingSocketsDebugOutput( ESteamNetworkingSocketsDebugOutputType nType, const char* pszMsg )
{
	g_SteamDebugOutputMutex.lock();
	SteamDebugOutput_t debugOutput = { nType, pszMsg };
	g_SteamDebugOutputQueue.push_back( debugOutput );
	g_SteamDebugOutputMutex.unlock();
}

void SetSteamNetworkingSocketsDebugOutputFunction()
{
	if ( SteamNetworkingUtils_SteamAPI() )
	{
		SteamNetworkingUtils_SteamAPI()->SetDebugOutputFunction(
			k_ESteamNetworkingSocketsDebugOutputType_Msg, SteamNetworkingSocketsDebugOutput );
	}
}

std::uint32_t g_nFakeIP;
unsigned short g_arFakePorts[1];

namespace
{
constexpr int FAKE_UDP_MODULES = NS_MULTICAST + 1;
constexpr int FAKE_CLIENT_PORT_INDEX = -1;
constexpr int FAKE_SERVER_PORT_INDEX = 0;
constexpr int FAKE_IP_ADDRESS_TEXT_BYTES = 48;
constexpr int k_nFakePortCount = 1;
constexpr int GAME_CONNECTION_BLOB_BYTES = 1024;
constexpr int RICH_PRESENCE_VALUE_BYTES = 64;
constexpr int CONNECT_COMMAND_BYTES = 128;
constexpr int PLAYER_GROUP_BYTES = 64;

template <std::size_t Size> void SetRichPresenceText( char ( &destination )[Size], const char* text )
{
	destination[0] = '\0';
	if ( text && text[0] != '\0' )
		Q_strncpy_s( destination, text, sizeof( destination ) );
}
}

ISteamNetworkingFakeUDPPort* ip_fakeudpports[FAKE_UDP_MODULES];

static CSteam3Server* s_Steam3Server;
static CSteam3Client s_Steam3Client;
static char g_szRichPresenceNiceMap[32];
static char g_szRichPresenceGameMode[32];
static char g_szRichPresenceOverride[128];
static uint64 g_currentGameConnectionSteamID;
static uint32 g_nCurrentGameConnectionPort;
static uint32 g_nCurrentGameConnectionIP;

CSteam3::CSteam3() : m_bLoggedOn( false ), m_bLogOnResult( false ), m_hSteamPipe( 0 )
{
}

CSteam3::~CSteam3()
{
}

CSteam3Server::~CSteam3Server()
{
}

CSteam3Client::CSteam3Client()
	: m_CallbackClientGameServerDeny( this, &CSteam3Client::OnClientGameServerDeny ),
	  m_CallbackGameServerChangeRequested( this, &CSteam3Client::OnGameServerChangeRequested ),
	  m_CallbackGameOverlayActivated( this, &CSteam3Client::OnGameOverlayActivated )
{
}

CSteam3Client::~CSteam3Client()
{
}

CSteam3Client::CCallbackInternal_OnNewUrlLaunchParameters::~CCallbackInternal_OnNewUrlLaunchParameters()
{
}

void CSteam3Client::CCallbackInternal_OnNewUrlLaunchParameters::Run( void* pvParam )
{
	CSteam3Client* outer = reinterpret_cast<CSteam3Client*>(
		reinterpret_cast<char*>( this ) - offsetof( CSteam3Client, m_steamcallback_OnNewUrlLaunchParameters ) );
	outer->OnNewUrlLaunchParameters( static_cast<NewUrlLaunchParameters_t*>( pvParam ) );
}

CSteam3Client::CCallbackInternal_OnGameRichPresenceJoinRequested::~CCallbackInternal_OnGameRichPresenceJoinRequested()
{
}

void CSteam3Client::CCallbackInternal_OnGameRichPresenceJoinRequested::Run( void* pvParam )
{
	CSteam3Client* outer = reinterpret_cast<CSteam3Client*>(
		reinterpret_cast<char*>( this ) - offsetof( CSteam3Client, m_steamcallback_OnGameRichPresenceJoinRequested ) );
	outer->OnGameRichPresenceJoinRequested( static_cast<GameRichPresenceJoinRequested_t*>( pvParam ) );
}

CSteam3Server::CSteam3Server()
	: m_CallbackGSClientApprove( this, &CSteam3Server::OnGSClientApprove ),
	  m_CallbackGSClientDeny( this, &CSteam3Server::OnGSClientDeny ),
	  m_CallbackGSClientKick( this, &CSteam3Server::OnGSClientKick ),
	  m_CallbackGSPolicyResponse( this, &CSteam3Server::OnGSPolicyResponse ),
	  m_CallbackLogonSuccess( this, &CSteam3Server::OnLogonSuccess ),
	  m_CallbackLogonFailure( this, &CSteam3Server::OnLogonFailure ),
	  m_CallbackFakeIPResult( this, &CSteam3Server::OnFakeIPResult ), m_bHasActivePlayers( false ),
	  m_bWantToBeSecure( false ), m_bLanOnly( false ), m_SteamIDGS( k_steamIDNotInitYetGS )
{
}

void Steam_Activate()
{
	if ( !s_Steam3Server )
		s_Steam3Server = new CSteam3Server;
	Steam3Server()->Activate();
}

CSteam3Server* Steam3Server()
{
#if defined( _WIN32 )
	ASSERT_AT( s_Steam3Server, 1864 );
#endif
	return s_Steam3Server;
}

bool CSteam3Server::BLanOnly()
{
	return m_bLanOnly;
}

uint64 CSteam3Server::GetSteamID()
{
	return m_SteamIDGS.ConvertToUint64();
}

bool NET_ShouldUseSteamFakeIP()
{
	return sv_lan.value <= 0.0f && !PF_IsDedicatedServer() && sv_use_steam_networking.value > 0.0f;
}

unsigned int NET_NetAdr_GetIPHostByteOrder( netadr_t* address )
{
	std::uint32_t ip;
	Q_memcpy( &ip, address->ip, sizeof( ip ) );
	return static_cast<unsigned int>( BigLong( static_cast<int>( ip ) ) );
}

unsigned int NET_NetAdr_GetIPNetworkByteOrder( netadr_t* address )
{
	unsigned int ip;
	Q_memcpy( &ip, address->ip, sizeof( ip ) );
	return ip;
}

unsigned short NET_NetAdr_GetPort( netadr_t* address )
{
	return static_cast<unsigned short>( BigShort( static_cast<short>( address->port ) ) );
}

void NET_NetAdr_SetIP( netadr_t* address, unsigned int ip )
{
	int networkIP = BigLong( static_cast<int>( ip ) );
	Q_memcpy( address->ip, &networkIP, sizeof( networkIP ) );
}

void NET_NetAdr_SetPort( netadr_t* address, unsigned short port )
{
	address->port = static_cast<unsigned short>( BigShort( static_cast<short>( port ) ) );
}

extern "C" bool ISteamApps_BIsSubscribedApp( uint32 appid )
{
	if ( !SteamApps() )
		return false;

	return SteamApps()->BIsSubscribedApp( appid );
}

TSteamGlobalUserID Steam_Steam3IDtoSteam2( uint64 unSteamID )
{
	const CSteamID steamID( unSteamID );
	const AccountID_t accountID = steamID.GetAccountID();
	TSteamGlobalUserID result;
	result.m_SteamInstanceID = 0;
	result.m_SteamLocalUserID.Split.High32bits = accountID % 2;
	result.m_SteamLocalUserID.Split.Low32bits = accountID / 2;
	return result;
}

extern "C" bool ISteamGameServer_BUpdateUserData( uint64 steamID, const char* playerName, uint32 score )
{
	if ( !SteamGameServer() )
		return false;

	return SteamGameServer()->BUpdateUserData( CSteamID( steamID ), playerName, score );
}

qboolean Steam_GSSteamIDIsValidOrLAN( uint64 unSteamID )
{
	const CSteamID steamID( unSteamID );
	return steamID.IsValid() || steamID == k_steamIDLanModeGS;
}

qboolean Steam_GSBSecure()
{
	Steam3Server();
	return SteamGameServer()->BSecure();
}

uint64 Steam_GSGetSteamID()
{
	CSteam3Server* server = Steam3Server();
	if ( server->BLanOnly() )
		return k_steamIDLanModeGS.ConvertToUint64();
	return server->GetSteamID();
}

void CSteam3Server::NotifyOfLevelChange( bool )
{
	SendUpdatedServerDetails();
	const bool passwordProtected = sv_password.string[0] && Q_strcasecmp( sv_password.string, "none" ) != 0;
	SteamGameServer()->SetPasswordProtected( passwordProtected );
	SteamGameServer()->ClearAllKeyValues();
	for ( cvar_t* variable = cvar_vars; variable; variable = variable->next )
	{
		if ( !( variable->flags & FCVAR_SERVER ) )
			continue;
		const char* value = variable->string;
		if ( variable->flags & FCVAR_PROTECTED )
		{
			value = Q_strlen( variable->string ) > 0 && Q_strcasecmp( variable->string, "none" ) != 0 ? "1" : "0";
		}
		SteamGameServer()->SetKeyValue( variable->name, value );
	}
}

void CSteam3Server::SendUpdatedServerDetails()
{
	int botCount = 0;
	for ( int i = 0; i < svs.maxclients; ++i )
	{
		const client_t& client = svs.clients[i];
		if ( ( client.active || client.spawned || client.connected ) && client.fakeclient )
			++botCount;
	}

	int maxPlayers = static_cast<int>( sv_visiblemaxplayers.value );
	if ( maxPlayers < 0 )
		maxPlayers = svs.maxclients;
	SteamGameServer()->SetMaxPlayerCount( maxPlayers );
	SteamGameServer()->SetBotPlayerCount( botCount );
	ISteamGameServer* server = SteamGameServer();
	server->SetServerName( Cvar_VariableString( "hostname" ) );
	SteamGameServer()->SetMapName( sv.name );
}

qboolean NET_IsSteamFakeIP( netsrc_t socketnumber, const sockaddr* to )
{
	ISteamNetworkingFakeUDPPort* port = ip_fakeudpports[socketnumber];
	ISteamNetworkingUtils* utils = SteamNetworkingUtils_SteamAPI();
	if ( utils && to->sa_family == AF_INET )
	{
		const sockaddr_in* address = reinterpret_cast<const sockaddr_in*>( to );
		if ( utils->GetIPv4FakeIPType( ntohl( address->sin_addr.s_addr ) ) > k_ESteamNetworkingFakeIPType_NotFake
			 && port )
			return true;
	}
	return false;
}

int NET_SteamFakeIPSendTo( netsrc_t socketnumber, SOCKET, char* packet, int len, int, const sockaddr* to, int )
{
	ISteamNetworkingFakeUDPPort* port = ip_fakeudpports[socketnumber];
	const sockaddr_in* destination = reinterpret_cast<const sockaddr_in*>( to );
	const uint32 ip = ntohl( destination->sin_addr.s_addr );
	const uint16 destinationPort = ntohs( destination->sin_port );
	SteamNetworkingIPAddr address;
	address.SetIPv4( ip, destinationPort );
	EResult result = port->SendMessageToFakeIP( address, packet, len, k_nSteamNetworkingSend_NoNagle );
	if ( result != k_EResultOK )
	{
		char addressText[FAKE_IP_ADDRESS_TEXT_BYTES];
		SteamNetworkingUtils_SteamAPI()->SteamNetworkingIPAddr_ToString(
			address, addressText, sizeof( addressText ), true );
		Con_Printf( "SendMessageToFakeIP to %s returned %d\n", addressText, result );
	}
	return len;
}

int NET_SteamFakeIPRecvFrom(
	netsrc_t source, char* packet, int maxLength, int, int fallbackResult, sockaddr* from, [[maybe_unused]] int* fromlen )
{
	ISteamNetworkingFakeUDPPort* port = ip_fakeudpports[source];
	SteamNetworkingMessage_t* message;
	if ( !port || port->ReceiveMessages( &message, 1 ) != 1 )
		return fallbackResult;
	int result = fallbackResult;
	if ( message->m_identityPeer.m_eType == k_ESteamNetworkingIdentityType_IPAddress )
	{
		int length = message->m_cbSize;
		if ( length <= maxLength )
		{
			if ( from )
			{
#if defined( _WIN32 )
				ASSERT_AT( *fromlen >= sizeof(struct sockaddr_in), 295 );
#endif
				sockaddr_in* address = reinterpret_cast<sockaddr_in*>( from );
				address->sin_family = AF_INET;
				const SteamNetworkingIPAddr& peer = *message->m_identityPeer.GetIPAddr();
				address->sin_addr.s_addr = htonl( peer.GetIPv4() );
				address->sin_port = htons( peer.m_port );
			}
			std::memcpy( packet, message->m_pData, length );
			result = length;
		}
		else
		{
			char address[SteamNetworkingIPAddr::k_cchMaxString];
			SteamNetworkingUtils_SteamAPI()->SteamNetworkingIPAddr_ToString(
				*message->m_identityPeer.GetIPAddr(), address, sizeof( address ), true );
			Con_Printf( "Ignoring message of size %d on FakeUDP port %d from %s\n", message->m_cbSize, g_arFakePorts[0],
				address );
		}
	}
	else
	{
		char identity[SteamNetworkingIdentity::k_cchMaxString];
		SteamNetworkingUtils_SteamAPI()->SteamNetworkingIdentity_ToString(
			message->m_identityPeer, identity, sizeof( identity ) );
		Con_Printf(
			"Ignoring message on FakeUDP port %d from %s -- not a fakeUDP identity?\n", g_arFakePorts[0], identity );
	}
	message->Release();
	return result;
}

void NET_SteamFakeIPDestroySocket( netsrc_t source )
{
	int module = static_cast<int>( source );
	if ( module >= NS_MULTICAST + 1 )
		return;

	ISteamNetworkingFakeUDPPort* port = ip_fakeudpports[module];
	if ( port )
	{
		port->DestroyFakeUDPPort();
		ip_fakeudpports[module] = nullptr;
	}
}

static void NET_InternalOpenOrCloseFakeUDPPort(
	ISteamNetworkingSockets* sockets, bool open, int module, int fakePortIndex )
{
	if ( open )
	{
		if ( !ip_fakeudpports[module] )
		{
			ip_fakeudpports[module] = sockets->CreateFakeUDPPort( fakePortIndex );
			if ( !ip_fakeudpports[module] )
				Con_Printf( "Failed to create fake UDP port, module %d fake port idx %d", module, fakePortIndex );
		}
	}
	else if ( ip_fakeudpports[module] )
	{
		ip_fakeudpports[module]->DestroyFakeUDPPort();
		ip_fakeudpports[module] = nullptr;
	}
}

static void NET_InternalOpenOrCloseFakeUDPPorts( bool client, bool open )
{
	ISteamNetworkingSockets* sockets =
		client ? SteamNetworkingSockets_SteamAPI() : SteamGameServerNetworkingSockets_SteamAPI();
	if ( !sockets )
		return;

	if ( client )
	{
		NET_InternalOpenOrCloseFakeUDPPort( sockets, open, NS_CLIENT, FAKE_CLIENT_PORT_INDEX );
		return;
	}

	if ( sv_lan.value > 0.0f || PF_IsDedicatedServer() || !( sv_use_steam_networking.value > 0.0f ) )
	{
		if ( open )
			return;
	}
	NET_InternalOpenOrCloseFakeUDPPort( sockets, open, NS_SERVER, FAKE_SERVER_PORT_INDEX );
}

void CSteam3Server::Activate()
{
	if ( m_bLoggedOn )
	{
		const bool lanOnly = sv_lan.value != 0.0f;
		if ( m_bLanOnly != lanOnly )
		{
			m_bLanOnly = lanOnly;
			m_bWantToBeSecure = !COM_CheckParm( "-insecure" ) && !lanOnly;
		}
		return;
	}

	m_bLoggedOn = true;
	uint32 serverIP = 0;
	if ( net_local_adr.type == NA_IP )
		serverIP = NET_NetAdr_GetIPHostByteOrder( &net_local_adr );
	m_bLanOnly = sv_lan.value > 0.0f;
	m_bWantToBeSecure = !COM_CheckParm( "-insecure" ) && !m_bLanOnly;

	char gameDirectory[GAME_DIRECTORY_BYTES];
	COM_FileBase( com_gamedir, gameDirectory );
	int serverPort = static_cast<int>( iphostport.value );
	EServerMode mode = m_bWantToBeSecure ? eServerModeAuthenticationAndSecure : eServerModeAuthentication;
	if ( m_bLanOnly )
		mode = eServerModeNoAuthentication;
	if ( !serverPort )
		serverPort = static_cast<int>( hostport.value );

	const int appID = GetGameAppID();
#if defined( _WIN32 )
	if ( appID > 0 && cls.state != ca_dedicated )
#else
	// The Linux dedicated engine also needs it: SteamGameServer_Init() reads it.
	if ( appID > 0 )
#endif
	{
		FILE* file = std::fopen( "steam_appid.txt", "w+" );
		if ( file )
		{
			std::fprintf( file, "%d\n", appID );
			std::fclose( file );
		}
	}

	if ( !SteamInternal_GameServer_Init( serverIP, 0, static_cast<uint16>( serverPort ),
			 STEAMGAMESERVER_QUERY_PORT_SHARED, mode, gpszVersionString ) )
		Sys_Error( "Unable to initialize Steam." );

	SteamGameServer()->SetProduct( gpszProductString );
	SteamGameServer()->SetModDir( gameDirectory );
	SteamGameServer()->SetDedicatedServer( cls.state == ca_dedicated );
	ISteamGameServer* server = SteamGameServer();
	server->SetGameDescription( gEntityInterface.pfnGetGameDescription() );

	if ( !( sv_lan.value > 0.0f ) && !PF_IsDedicatedServer() && sv_use_steam_networking.value > 0.0f )
	{
		SetSteamNetworkingSocketsDebugOutputFunction();
		Con_Printf( "FakeIP enabled! Requesting a fake IP.\n" );
		SteamGameServerNetworkingSockets_SteamAPI()->BeginAsyncRequestFakeIP( 1 );
	}

	SteamGameServer()->LogOnAnonymous();
	m_bLogOnResult = false;
	if ( COM_CheckParm( "-nomaster" ) )
	{
		Con_Printf( "Master server communication disabled.\n" );
		gfNoMasterServer = true;
	}
	else if ( !gfNoMasterServer && svs.maxclients > 1 )
	{
		SteamGameServer()->SetAdvertiseServerActive( true );
		if ( NET_ShouldUseSteamFakeIP() )
			NET_InternalOpenOrCloseFakeUDPPorts( false, true );
		if ( sv_password.string[0] )
			Q_strcasecmp( sv_password.string, "none" );
		NotifyOfLevelChange( true );
	}
}

uint64 Steam_StringToSteamID( const char* text )
{
	CSteamID steamID;
	EUniverse universe = k_EUniversePublic;
	if ( s_Steam3Server && !s_Steam3Server->BLanOnly() )
	{
		CSteamID serverID( s_Steam3Server->GetSteamID() );
		universe = serverID.GetEUniverse();
	}
	SteamIDFromSteam2String( text, universe, &steamID );
	return steamID.ConvertToUint64();
}

void CSteam3Server::NotifyClientDisconnect( client_t* client )
{
	constexpr int STEAM_AUTH_ID_TYPE = 1;
	constexpr int STEAM_DISCONNECT_ID_TYPE = 3;
	if ( !client || !m_bLoggedOn )
		return;
	int idtype = client->network_userid.idtype;
	if ( idtype != STEAM_AUTH_ID_TYPE && idtype != STEAM_DISCONNECT_ID_TYPE )
		return;
	SteamGameServer()->SendUserDisconnect_DEPRECATED( CSteamID( client->network_userid.m_SteamID ) );
}

void Steam_NotifyClientDisconnect( client_t* client )
{
	if ( s_Steam3Server )
		s_Steam3Server->NotifyClientDisconnect( client );
}

void Net_CheckOpenFakeUDPPorts( qboolean client )
{
	NET_InternalOpenOrCloseFakeUDPPorts( client != 0, true );
}

void Net_PollSteamFakeIPResult()
{
	ISteamNetworkingSockets* sockets = SteamGameServerNetworkingSockets_SteamAPI();
	if ( g_nFakeIP || !sockets )
		return;

	SteamNetworkingFakeIPResult_t result;
	sockets->GetFakeIP( 0, &result );
	if ( result.m_eResult == k_EResultBusy || result.m_eResult == k_EResultNoMatch )
		return;
	if ( result.m_eResult != k_EResultOK )
		Sys_Error( "FakeIP allocation failed with error code %d", result.m_eResult );

	g_nFakeIP = result.m_unIP;
#if defined( _WIN32 )
	ASSERT_AT( result.m_unPorts[k_nFakePortCount] == 0, 403 );
#endif

	netadr_t address;
	address.type = NA_IP;
	NET_NetAdr_SetIP( &address, result.m_unIP );
	NET_NetAdr_SetPort( &address, result.m_unPorts[0] );
	Con_Printf( "FakeIP allocation succeeded: %s", NET_AdrToString( address ) );
	Log_Printf( "FakeIP allocation succeeded: %s", NET_AdrToString( address ) );

	for ( int i = 0; i < k_nFakePortCount; ++i )
	{
#if defined( _WIN32 )
		ASSERT_AT( result.m_unPorts[i], 414 );
#endif
		g_arFakePorts[i] = result.m_unPorts[i];
	}
	Con_Printf( "\n" );
	Log_Printf( "\n" );
}

bool CSteam3Server::NotifyClientConnect( client_t* client, const void* authentication, uint32 bytes )
{
	if ( !client || !m_bLoggedOn )
		return false;

	constexpr int STEAM_AUTH_ID_TYPE = 1;
	CSteamID steamID;
	client->network_userid.idtype = STEAM_AUTH_ID_TYPE;
	const bool connected = SteamGameServer()->SendUserConnectAndAuthenticate_DEPRECATED(
		ntohl( client->network_userid.clientip ), authentication, bytes, &steamID );
	client->network_userid.m_SteamID = steamID.ConvertToUint64();
	return connected;
}

bool CSteam3Server::NotifyBotConnect( client_t* client )
{
	if ( !client || !m_bLoggedOn )
		return false;

	constexpr int STEAM_BOT_ID_TYPE = 3;
	client->network_userid.idtype = STEAM_BOT_ID_TYPE;
	client->network_userid.m_SteamID = SteamGameServer()->CreateUnauthenticatedUserConnection().ConvertToUint64();
	return true;
}

qboolean Steam_NotifyClientConnect( client_t* client, const void* authentication, unsigned int bytes )
{
	if ( !s_Steam3Server )
		return false;
	return s_Steam3Server->NotifyClientConnect( client, authentication, bytes );
}

qboolean Steam_NotifyBotConnect( client_t* client )
{
	if ( !s_Steam3Server )
		return false;
	return s_Steam3Server->NotifyBotConnect( client );
}

extern "C" uint64 ISteamGameServer_CreateUnauthenticatedUserConnection()
{
	if ( !SteamGameServer() )
		return 0;
	return SteamGameServer()->CreateUnauthenticatedUserConnection().ConvertToUint64();
}

static bool IsSafeConnectCharacter( unsigned char character )
{
	if ( static_cast<unsigned char>( character - '0' ) <= 10 )
		return true;
	if ( character == '_' )
		return true;
	if ( static_cast<unsigned char>( ( character & 0xDF ) - 'A' ) <= 25 )
		return true;
	return static_cast<unsigned char>( character - '-' ) <= 1;
}

static void UnsafeCmdLineProcessor( const char* command )
{
	constexpr int CONNECT_PREFIX_LENGTH = 9;
	if ( Q_strncmp( command, "+connect ", CONNECT_PREFIX_LENGTH ) != 0 )
		return;

	const char* addressText = command + CONNECT_PREFIX_LENGTH;
	if ( addressText[0] == '\0' )
		return;

	for ( const unsigned char* cursor = reinterpret_cast<const unsigned char*>( addressText ); *cursor != 0; ++cursor )
	{
		if ( !IsSafeConnectCharacter( *cursor ) )
		{
			Con_Printf( "Rejecting unsafe cmdline\n" );
			return;
		}
	}

	netadr_t address{};
	if ( !NET_StringToAdr( const_cast<char*>( addressText ), &address ) )
	{
		Con_Printf( "Invalid address to connect to from unsafe location! Ignoring.\n" );
		return;
	}

	char sanitized[256];
	_snprintf( sanitized, sizeof( sanitized ), "\nconnect %s\n", NET_AdrToString( address ) );
	sanitized[sizeof( sanitized ) - 1] = '\0';
	Cbuf_AddText( sanitized );
	Cbuf_AddText( "\ncl_skipvid 1\n" );
}

static void ReadLaunchCommandLine()
{
	int size = SteamApps()->GetLaunchCommandLine( nullptr, 0 );
	void* command = Mem_ZeroMalloc( static_cast<std::size_t>( size ) );
	if ( !command )
		return;

	SteamApps()->GetLaunchCommandLine( static_cast<char*>( command ), size );
	UnsafeCmdLineProcessor( static_cast<const char*>( command ) );
	Mem_Free( command );
}

void CSteam3Server::OnLogonFailure( SteamServerConnectFailure_t* failure )
{
	if ( !m_bLogOnResult && failure->m_eResult != k_EResultServiceUnavailable )
	{
		if ( !m_bLanOnly )
			Con_Printf( "Could not establish connection to Steam servers.\n" );
	}
	else if ( !m_bLogOnResult && !m_bLanOnly )
	{
		Con_Printf( "Connection to Steam servers successful (SU).\n" );
		if ( m_bWantToBeSecure )
			Con_Printf( "   VAC secure mode not available.\n" );
	}
	m_bLogOnResult = true;
}

void CSteam3Server::OnFakeIPResult( SteamNetworkingFakeIPResult_t* )
{
	NET_GetLocalAddress();
}

void CSteam3Server::OnGSPolicyResponse( GSPolicyResponse_t* )
{
	if ( SteamGameServer()->BSecure() )
		Con_Printf( "   VAC secure mode is activated.\n" );
	else
		Con_Printf( "   VAC secure mode disabled.\n" );
}

client_t* CSteam3Server::ClientFindFromSteamID( CSteamID& steamID )
{
	constexpr int STEAM_AUTH_ID_TYPE = 1;
	if ( svs.maxclients <= 0 )
		return nullptr;

	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		if ( ( client->connected || client->active || client->spawned )
			 && client->network_userid.idtype == STEAM_AUTH_ID_TYPE
			 && client->network_userid.m_SteamID == steamID.ConvertToUint64() )
			return client;
	}
	return nullptr;
}

void CSteam3Server::OnGSClientDenyHelper( client_t* client, EDenyReason reason, const char* text )
{
	const char* dropText;
	switch ( reason )
	{
	case k_EDenyInvalidVersion:
		Con_Printf( "Client %s DENY REASON: k_EDenyInvalidVersion\n", client->name );
		dropText = "Client version incompatible with server. \nPlease exit and restart";
		break;
	case k_EDenyNotLoggedOn:
		Con_Printf( "Client %s DENY REASON: k_EDenyNotLoggedOn\n", client->name );
		if ( m_bLanOnly )
			return;
		dropText = "No Steam logon\n";
		break;
	case k_EDenyNoLicense:
		Con_Printf( "Client %s DENY REASON: k_EDenyNoLicense\n", client->name );
		dropText = "This Steam account does not own this game. \nPlease login to the correct Steam account.";
		break;
	case k_EDenyCheater:
		Con_Printf( "Client %s DENY REASON: k_EDenyCheater\n", client->name );
		dropText = "VAC banned from secure server\n";
		break;
	case k_EDenyLoggedInElseWhere:
		Con_Printf( "Client %s DENY REASON: k_EDenyLoggedInElseWhere\n", client->name );
		if ( m_bLanOnly )
			return;
		dropText = "This Steam account is being used in another location\n";
		break;
	case k_EDenyUnknownText:
		Con_Printf( "Client %s DENY REASON: k_EDenyUnknownText\n", client->name );
		if ( !text || text[0] == '\0' )
			dropText = "Client dropped by server";
		else
			dropText = text;
		break;
	case k_EDenyIncompatibleAnticheat:
		Con_Printf( "Client %s DENY REASON: k_EDenyIncompatibleAnticheat\n", client->name );
		dropText = "You are running an external tool that is incompatible with Secure servers.";
		break;
	case k_EDenyMemoryCorruption:
		Con_Printf( "Client %s DENY REASON: k_EDenyMemoryCorruption\n", client->name );
		dropText = "Memory corruption detected.";
		break;
	case k_EDenyIncompatibleSoftware:
		Con_Printf( "Client %s DENY REASON: k_EDenyIncompatibleSoftware\n", client->name );
		dropText = "You are running software that is not compatible with Secure servers.";
		break;
	case k_EDenySteamConnectionLost:
		Con_Printf( "Client %s DENY REASON: k_EDenySteamConnectionLost\n", client->name );
		if ( m_bLanOnly )
			return;
		dropText = "Steam connection lost\n";
		break;
	case k_EDenySteamConnectionError:
		Con_Printf( "Client %s DENY REASON: k_EDenySteamConnectionError\n", client->name );
		if ( !m_bLanOnly )
		{
			Con_Printf(
				"Steam auth rejected client '%s' (%llu) with reason SteamConnectionError, but we're ignoring that\n",
				client->name, client->network_userid.m_SteamID );
		}
		return;
	case k_EDenySteamResponseTimedOut:
		Con_Printf( "Client %s DENY REASON: k_EDenySteamResponseTimedOut\n", client->name );
		dropText =
			"Client timed out while answering challenge.\n"
			"---> Please make sure that you have opened the appropriate ports on any firewall you are connected behind.\n"
			"---> See http://support.steampowered.com for help with firewall configuration.";
		break;
	case k_EDenySteamValidationStalled:
		Con_Printf( "Client %s DENY REASON: k_EDenySteamValidationStalled\n", client->name );
		if ( m_bLanOnly )
			client->network_userid.m_SteamID = 1;
		return;
	case k_EDenyGeneric:
		Con_Printf( "Client %s DENY REASON: k_EDenyGeneric\n", client->name );
		dropText = "Client dropped by server";
		break;
	case k_EDenyInvalid:
	case k_EDenySteamOwnerLeftGuestUser:
	default:
		dropText = "Client dropped by server";
		break;
	}
	SV_DropClient( client, false, dropText );
}

void CSteam3Server::OnGSClientApprove( GSClientApprove_t* result )
{
	client_t* client = ClientFindFromSteamID( result->m_SteamID );
	if ( !client )
		return;

	if ( SV_FilterUser( &client->network_userid ) )
	{
		constexpr char BANNED_TEXT[] = "You have been banned from this server\n";
		SV_RejectConnection( &client->netchan.remote_address, BANNED_TEXT );
		SV_DropClient( client, false, "STEAM UserID %s is in server ban list\n", SV_GetClientIDString( client ) );
		return;
	}

	if ( SV_CheckForDuplicateSteamID( client ) == -1 )
	{
		char message[512];
		_snprintf( message, sizeof( message ), "\"%s<%i><%s><>\" STEAM USERID validated\n", client->name,
			client->userid, SV_GetClientIDString( client ) );
		Con_DPrintf( "%s", message );
		Log_Printf( "%s", message );
		return;
	}

	constexpr char DUPLICATE_TEXT[] = "Your UserID is already in use on this server.\n";
	SV_RejectConnection( &client->netchan.remote_address, DUPLICATE_TEXT );
	SV_DropClient(
		client, false, "STEAM UserID %s is already\nin use on this server\n", SV_GetClientIDString( client ) );
}

void CSteam3Server::OnGSClientDeny( GSClientDeny_t* result )
{
	client_t* client = ClientFindFromSteamID( result->m_SteamID );
	if ( client )
		OnGSClientDenyHelper( client, result->m_eDenyReason, result->m_rgchOptionalText );
}

void CSteam3Server::OnGSClientKick( GSClientKick_t* result )
{
	client_t* client = ClientFindFromSteamID( result->m_SteamID );
	if ( client )
		OnGSClientDenyHelper( client, result->m_eDenyReason, nullptr );
}

void CSteam3Server::OnLogonSuccess( SteamServersConnected_t* )
{
	if ( m_bLogOnResult )
	{
		if ( !m_bLanOnly )
			Con_Printf( "Reconnected to Steam servers.\n" );
	}
	else
	{
		m_bLogOnResult = true;
		if ( !m_bLanOnly )
			Con_Printf( "Connection to Steam servers successful.\n" );
	}
	m_SteamIDGS = SteamGameServer()->GetSteamID();
	SendUpdatedServerDetails();
}

void CSteam3Server::Shutdown()
{
	if ( !m_bLoggedOn )
		return;

	NET_InternalOpenOrCloseFakeUDPPorts( false, false );
	SteamGameServer()->SetAdvertiseServerActive( false );
	SteamGameServer()->LogOff();
	SteamGameServer_Shutdown();
	m_bLoggedOn = false;
	g_arFakePorts[0] = 0;
	g_nFakeIP = 0;
	if ( ip_sockets[NS_SERVER] )
		NET_GetLocalAddress();
}

void CSteam3Server::RunFrame()
{
	if ( svs.maxclients <= 1 )
		return;

	static double s_fLastRunFragsUpdate;
	static double s_fLastRunCallback;
	static double s_fLastRunSendPackets;
	double now = Sys_FloatTime();
	if ( now - s_fLastRunFragsUpdate > 1.0 )
	{
		s_fLastRunFragsUpdate = now;
		bool activePlayers = false;
		for ( int index = 0; index < svs.maxclients; ++index )
		{
			const client_t& client = svs.clients[index];
			if ( client.active || client.spawned || client.connected )
			{
				activePlayers = true;
				break;
			}
		}
		m_bHasActivePlayers = activePlayers;
		SendUpdatedServerDetails();
		bool passwordProtected = false;
		if ( sv_password.string[0] != '\0' )
			passwordProtected = Q_stricmp( sv_password.string, "none" ) != 0;
		SteamGameServer()->SetPasswordProtected( passwordProtected );
		for ( int index = 0; index < svs.maxclients; ++index )
		{
			client_t& client = svs.clients[index];
			if ( !client.active )
				continue;
			uint32 score = static_cast<uint32>( static_cast<int>( client.edict->v.frags ) );
			SteamGameServer()->BUpdateUserData( CSteamID( client.network_userid.m_SteamID ), client.name, score );
		}
		if ( SteamGameServer()->WasRestartRequested() )
		{
			Con_Printf( "%cMasterRequestRestart\n", 3 );
			if ( COM_CheckParm( "-steam" ) )
			{
				Con_Printf( "Your server needs to be restarted in order to receive the latest update.\n" );
				Log_Printf( "Your server needs to be restarted in order to receive the latest update.\n" );
			}
			else
			{
				Con_Printf( "Your server is out of date.  Please update and restart.\n" );
			}
		}
	}

	if ( now - s_fLastRunCallback > 0.1 )
	{
		SteamGameServer_RunCallbacks();
		s_fLastRunCallback = now;
	}

	if ( now - s_fLastRunSendPackets > 0.01 )
	{
		s_fLastRunSendPackets = now;
		char packet[4096];
		uint32 ip;
		uint16 port;
		int length;
		while ( ( length = SteamGameServer()->GetNextOutgoingPacket( packet, sizeof( packet ), &ip, &port ) ) > 0 )
		{
			netadr_t destination;
			destination.type = NA_IP;
			NET_NetAdr_SetIP( &destination, ip );
			NET_NetAdr_SetPort( &destination, port );
			NET_SendPacket( NS_SERVER, length, packet, destination );
		}
	}
}

void NET_CheckCleanupFakeIPConnection( netsrc_t socketnumber, netadr_t* remoteAddress )
{
	if ( socketnumber >= NS_MULTICAST + 1 )
		return;

	ISteamNetworkingFakeUDPPort* port = ip_fakeudpports[socketnumber];
	if ( !port || remoteAddress->type != NA_IP )
		return;

	ISteamNetworkingUtils* utils = SteamNetworkingUtils_SteamAPI();
	if ( !utils )
		return;

	uint32 ip = NET_NetAdr_GetIPHostByteOrder( remoteAddress );
	if ( utils->GetIPv4FakeIPType( ip ) <= k_ESteamNetworkingFakeIPType_NotFake )
		return;

	SteamNetworkingIPAddr address;
	address.SetIPv4( ip, NET_NetAdr_GetPort( remoteAddress ) );
	port->ScheduleCleanup( address );
}

void Net_CloseFakeUDPPorts( qboolean client )
{
	NET_InternalOpenOrCloseFakeUDPPorts( client != 0, false );
}

void Steam_NotifyOfLevelChange()
{
	if ( s_Steam3Server )
		s_Steam3Server->NotifyOfLevelChange( false );
}

void Steam_Shutdown()
{
	if ( !s_Steam3Server )
		return;
	s_Steam3Server->Shutdown();
	delete s_Steam3Server;
	s_Steam3Server = nullptr;
}

void Steam_RunFrame()
{
	g_SteamDebugOutputMutex.lock();
	for ( const SteamDebugOutput_t& output : g_SteamDebugOutputQueue )
	{
		switch ( output.m_nType )
		{
		case k_ESteamNetworkingSocketsDebugOutputType_Important:
		case k_ESteamNetworkingSocketsDebugOutputType_Msg:
		case k_ESteamNetworkingSocketsDebugOutputType_Verbose:
		case k_ESteamNetworkingSocketsDebugOutputType_Debug:
		case k_ESteamNetworkingSocketsDebugOutputType_Everything:
			Con_DPrintf( "[SteamNetworkingSockets] %s\n", output.m_sMsg.c_str() );
			break;
		case k_ESteamNetworkingSocketsDebugOutputType_Bug:
		case k_ESteamNetworkingSocketsDebugOutputType_Error:
		case k_ESteamNetworkingSocketsDebugOutputType_Warning:
			Con_Printf( "[SteamNetworkingSockets] %s\n", output.m_sMsg.c_str() );
			Log_Printf( "[SteamNetworkingSockets] %s\n", output.m_sMsg.c_str() );
			break;
		case k_ESteamNetworkingSocketsDebugOutputType_None:
		case k_ESteamNetworkingSocketsDebugOutputType__Force32Bit:
		default:
			break;
		}
	}
	g_SteamDebugOutputQueue.clear();
	g_SteamDebugOutputMutex.unlock();

	if ( s_Steam3Server )
		s_Steam3Server->RunFrame();
}

void Steam_SetCVar( const char* key, const char* value )
{
	if ( s_Steam3Server )
		SteamGameServer()->SetKeyValue( key, value );
}

qboolean Steam_GSBSecurePreference()
{
#if defined( _WIN32 )
	CSteam3Server* server = Steam3Server();
#else
	CSteam3Server* server = s_Steam3Server;
#endif
	return server->m_bWantToBeSecure;
}

qboolean Steam_GSBLoggedOn()
{
#if defined( _WIN32 )
	CSteam3Server* server = Steam3Server();
#else
	CSteam3Server* server = s_Steam3Server;
#endif
	if ( !server->m_bLoggedOn )
		return false;
	return SteamGameServer()->BLoggedOn();
}

const char* Steam_GetGSUniverse()
{
#if defined( _WIN32 )
	CSteam3Server* server = Steam3Server();
#else
	CSteam3Server* server = s_Steam3Server;
#endif
	EUniverse universe = k_EUniversePublic;
	if ( !server->BLanOnly() )
		universe = CSteamID( server->GetSteamID() ).GetEUniverse();

	switch ( universe )
	{
	case k_EUniversePublic:
		return "";
	case k_EUniverseBeta:
		return "(beta)";
	case k_EUniverseInternal:
		return "(internal)";
	case k_EUniverseInvalid:
	case k_EUniverseDev:
	case k_EUniverseMax:
	default:
		return "(u)";
	}
}

const char* Steam_GetCommunityName()
{
	if ( !SteamFriends() )
		return nullptr;
	return SteamFriends()->GetPersonaName();
}

void Steam_ProcessInsecureLaunchParameters()
{
	static bool s_bProcessedCommandLine;
	if ( cls.state == ca_dedicated || s_bProcessedCommandLine )
		return;

	s_bProcessedCommandLine = true;
	Con_Printf( "OnNewUrlLaunchParameters\n" );
	ReadLaunchCommandLine();
}

CSteam3Client* Steam3Client()
{
	return &s_Steam3Client;
}

void Master_SetMaster_f()
{
	int argc = Cmd_Argc();
	if ( !s_Steam3Server )
	{
		Con_Printf( "Usage:\nSetmaster unavailable, start a server first.\n" );
		return;
	}
	if ( static_cast<unsigned int>( argc - 2 ) > 3 )
	{
		Con_Printf( "Usage:\nSetmaster <enable | disable>\n" );
		return;
	}

	const char* argument = Cmd_Argv( 1 );
	if ( !argument || argument[0] == '\0' )
		return;

	if ( Q_stricmp( argument, "disable" ) != 0 || gfNoMasterServer )
	{
		if ( Q_stricmp( argument, "enable" ) != 0 || !gfNoMasterServer )
			return;
		gfNoMasterServer = false;
	}
	else
	{
		gfNoMasterServer = true;
	}
	SteamGameServer()->SetAdvertiseServerActive( gfNoMasterServer != 0 );
}

void Steam_HandleIncomingPacket( byte* data, int length, unsigned int fromIp, unsigned short port )
{
	SteamGameServer()->HandleIncomingPacket( data, length, fromIp, port );
}

void CSteam3Client::OnClientGameServerDeny( ClientGameServerDeny_t* )
{
	COM_ExplainDisconnection( true, "Invalid server version, unable to connect." );
	CL_Disconnect();
}

void CSteam3Client::OnGameServerChangeRequested( GameServerChangeRequested_t* request )
{
	char server[128];
	Cvar_DirectSet( &password, request->m_rgchPassword );
	Con_Printf( "Connecting to %s\n", request->m_rgchServer );
	Q_strncpy_s( server, request->m_rgchServer, sizeof( server ) );
	server[std::strcspn( server, ";\n" )] = '\0';
	Cbuf_AddText( va( "connect %s\n", server ) );
}

void CSteam3Client::OnGameOverlayActivated( GameOverlayActivated_t* activated )
{
	if ( !Host_IsSinglePlayerGame() )
		return;
	if ( activated->m_bActive )
	{
		Cbuf_AddText( "setpause;" );
		return;
	}
	if ( !staticGameUIFuncs->IsGameUIActive() )
		Cbuf_AddText( "unpause;" );
}

void CSteam3Client::Shutdown()
{
	if ( !m_bLoggedOn )
		return;

	if ( SteamNetworkingSockets_SteamAPI() && ip_fakeudpports[NS_CLIENT] )
	{
		ip_fakeudpports[NS_CLIENT]->DestroyFakeUDPPort();
		ip_fakeudpports[NS_CLIENT] = nullptr;
	}
	if ( SteamInput() )
		SteamInput()->Shutdown();
	if ( eng->GetQuitting() == 1 )
		SteamAPI_Shutdown();
	m_bLoggedOn = false;
}

void CSteam3Client::OnNewUrlLaunchParameters( NewUrlLaunchParameters_t* )
{
	Con_Printf( "OnNewUrlLaunchParameters\n" );
	ReadLaunchCommandLine();
}

void CSteam3Client::OnGameRichPresenceJoinRequested( GameRichPresenceJoinRequested_t* request )
{
	Con_Printf( "OnGameRichPresenceJoinRequested\n" );
	if ( request && request->m_rgchConnect[0] != '\0' )
		UnsafeCmdLineProcessor( request->m_rgchConnect );
}

void CSteam3Client::SetRichPresenceOverride( const char* text )
{
	SetRichPresenceText( g_szRichPresenceOverride, text );
}

void CSteam3Client::SetRichPresenceGameMode( const char* text )
{
	SetRichPresenceText( g_szRichPresenceGameMode, text );
}

void CSteam3Client::SetRichPresenceInfo( const char* mapName )
{
	SetRichPresenceText( g_szRichPresenceNiceMap, mapName );
}

void CSteam3Client::UpdateRichPresence()
{
	char connectCommand[CONNECT_COMMAND_BYTES] = {};
	char groupName[PLAYER_GROUP_BYTES] = {};
	char mapName[256] = {};
	connectCommand[CONNECT_COMMAND_BYTES - 1] = '\0';
	groupName[PLAYER_GROUP_BYTES - 1] = '\0';
	if ( cls.state == ca_dedicated )
		return;

	bool hasLevel = cl.levelname[0] != '\0';
	if ( hasLevel )
	{
		if ( std::sscanf( cl.levelname, "maps/%s.bsp", mapName ) == 1 )
		{
			char* extension = std::strstr( mapName, ".bsp" );
			if ( extension )
				extension[0] = '\0';
		}
		else
		{
			mapName[0] = '\0';
		}
	}

	int maxClients = cl.maxclients >= 0 ? cl.maxclients : 0;
	int playerCount = 0;
	for ( int index = 0; index < MAX_CLIENTS; ++index )
	{
		if ( cl.players[index].name[0] != '\0' )
			++playerCount;
	}

	char currentPlayers[32];
	char maxPlayers[32];
	_snprintf( currentPlayers, sizeof( currentPlayers ), "%d", playerCount );
	_snprintf( maxPlayers, sizeof( maxPlayers ), "%d", maxClients );

	if ( !Host_IsSinglePlayerGame() && g_nCurrentGameConnectionIP != 0 && g_nCurrentGameConnectionPort != 0 )
	{
		netadr_t server{};
		server.type = NA_IP;
		NET_NetAdr_SetIP( &server, g_nCurrentGameConnectionIP );
		NET_NetAdr_SetPort( &server, static_cast<unsigned short>( g_nCurrentGameConnectionPort ) );
		if ( g_nFakeIP != 0 && NET_IsLocalAddress( server ) )
		{
			NET_NetAdr_SetIP( &server, g_nFakeIP );
			NET_NetAdr_SetPort( &server, g_arFakePorts[0] );
		}
		if ( !NET_IsReservedAdr( server ) && !NET_IsLocalAddress( server ) )
		{
			_snprintf( 
				groupName, sizeof( groupName ), "%x%x", g_nCurrentGameConnectionIP, g_nCurrentGameConnectionPort );
			_snprintf( connectCommand, sizeof( connectCommand ), "+connect %s", NET_AdrToString( server ) );
		}
	}

	const char* pauseState = "#HL_RP_Unpaused";
	if ( Host_IsServerActive() && sv.paused )
		pauseState = "#HL_RP_Paused";

	const char* niceMap = g_szRichPresenceNiceMap;
	const char* known = "Unknown";
	if ( g_szRichPresenceNiceMap[0] != '\0' || ( niceMap = CL_MapLocalizationString( mapName ) ) != mapName )
		known = "Known";

	char gameState[RICH_PRESENCE_VALUE_BYTES];
	char status[RICH_PRESENCE_VALUE_BYTES];
	Q_strncpy_s( gameState, "#HL_RP_MainMenu", sizeof( gameState ) );
	Q_strncpy_s( status, "#HL_Status_MainMenu", sizeof( status ) );
	if ( hasLevel )
	{
		const char* mode = g_szRichPresenceGameMode;
		if ( mode[0] == '\0' )
			mode = cl.gametype == 1 ? "Deathmatch" : "Singleplayer";
		_snprintf( gameState, sizeof( gameState ), "#HL_RP_%s%s", mode, known );
		_snprintf( status, sizeof( status ), "#HL_Status_%s%s", mode, known );
	}

	if ( !SteamFriends() )
		return;

	const char* display = g_szRichPresenceOverride[0] != '\0' ? g_szRichPresenceOverride : gameState;
	const char* statusText = g_szRichPresenceOverride[0] != '\0' ? g_szRichPresenceOverride : status;
	SteamFriends()->SetRichPresence( "steam_display", display );
	SteamFriends()->SetRichPresence( "status", statusText );
	SteamFriends()->SetRichPresence( "steam_player_group", groupName );
	SteamFriends()->SetRichPresence( "steam_player_group_size", currentPlayers );
	if ( std::strlen( connectCommand ) <= 1 )
		SteamFriends()->SetRichPresence( "connect", com_empty_string );
	else
		SteamFriends()->SetRichPresence( "connect", connectCommand );
	SteamFriends()->SetRichPresence( "game:raw_map_name", mapName );
	SteamFriends()->SetRichPresence(
		"game:map_name", g_szRichPresenceNiceMap[0] != '\0' ? g_szRichPresenceNiceMap : niceMap );
	SteamFriends()->SetRichPresence( "game:current_player_count", currentPlayers );
	SteamFriends()->SetRichPresence( "game:max_player_count", maxPlayers );
	SteamFriends()->SetRichPresence( "game:server_name", Cvar_VariableString( "hostname" ) );
	SteamFriends()->SetRichPresence( "game:pause_state", pauseState );
}

void CSteam3Client::ActivateInputSet( bool inMenu )
{
	if ( SteamUtils() )
		staticGameUIFuncs->SetIsRunningOnSteamDeck( SteamUtils()->IsSteamRunningOnSteamDeck() );
	if ( !SteamInput() )
		return;

	SteamInput()->DeactivateAllActionSetLayers( STEAM_INPUT_HANDLE_ALL_CONTROLLERS );
	if ( inMenu )
	{
		uint64 actionSet = SteamInput()->GetActionSetHandle( "HL_MenuDefault" );
		SteamInput()->ActivateActionSet( STEAM_INPUT_HANDLE_ALL_CONTROLLERS, actionSet );
		static_cast<void>( Host_GetMaxClients() );
		return;
	}

	uint64 actionSet = SteamInput()->GetActionSetHandle( "HL_GameDefault" );
	SteamInput()->ActivateActionSet( STEAM_INPUT_HANDLE_ALL_CONTROLLERS, actionSet );
	if ( Host_GetMaxClients() > 1 )
	{
		uint64 layer = SteamInput()->GetActionSetHandle( "HL_LayerMultiplayer" );
		SteamInput()->ActivateActionSetLayer( STEAM_INPUT_HANDLE_ALL_CONTROLLERS, layer );
	}
}

void CSteam3Client::ShowOnScreenKeyboard( int, int fieldY )
{
	if ( !SteamUtils() )
		return;
	SteamUtils()->ShowFloatingGamepadTextInput(
		k_EFloatingGamepadTextInputModeModeSingleLine, 0, 0, 1280, fieldY + 36 );
}

int CSteam3Client::InitiateGameConnection(
	void* data, int maxData, uint64 steamID, uint32 serverIP, uint16 serverPort, bool secure )
{
	static char blob[GAME_CONNECTION_BLOB_BYTES];
	static int blobSize;
	static bool secureConnection;

	uint32 hostIP = static_cast<uint32>( BigLong( static_cast<int>( serverIP ) ) );
	uint16 hostPort = static_cast<uint16>( BigShort( static_cast<short>( serverPort ) ) );
	netadr_t address{};
	address.type = NA_IP;
	NET_NetAdr_SetIP( &address, hostIP );
	NET_NetAdr_SetPort( &address, hostPort );
	if ( g_nFakeIP != 0 && NET_IsLocalAddress( address ) )
	{
		hostIP = g_nFakeIP;
		hostPort = g_arFakePorts[0];
	}

	if ( hostIP != g_nCurrentGameConnectionIP || hostPort != g_nCurrentGameConnectionPort
		 || g_currentGameConnectionSteamID != steamID || secureConnection != secure )
	{
		if ( g_nCurrentGameConnectionIP != 0 || g_nCurrentGameConnectionPort != 0 )
		{
			SteamUser()->TerminateGameConnection_DEPRECATED(
				g_nCurrentGameConnectionIP, static_cast<uint16>( g_nCurrentGameConnectionPort ) );
		}
		g_nCurrentGameConnectionIP = hostIP;
		g_nCurrentGameConnectionPort = hostPort;
		g_currentGameConnectionSteamID = steamID;
		secureConnection = secure;
		blobSize =
			SteamUser()->InitiateGameConnection_DEPRECATED( blob, GAME_CONNECTION_BLOB_BYTES, CSteamID( steamID ),
				g_nCurrentGameConnectionIP, static_cast<uint16>( g_nCurrentGameConnectionPort ), secureConnection );
	}

	if ( maxData < blobSize )
	{
#if defined( _WIN32 )
		ASSERT_AT( false, 1498 );
#else
		return 0;
#endif
	}
	std::memcpy( data, blob, static_cast<std::size_t>( blobSize ) );
	return blobSize;
}

void CSteam3Client::TerminateConnection()
{
	if ( g_nCurrentGameConnectionIP != 0 )
	{
		SteamUser()->TerminateGameConnection_DEPRECATED(
			g_nCurrentGameConnectionIP, static_cast<uint16>( g_nCurrentGameConnectionPort ) );
		g_nCurrentGameConnectionIP = 0;
		g_nCurrentGameConnectionPort = 0;
		g_currentGameConnectionSteamID = 0;
	}
	SteamUser()->AdvertiseGame( CSteamID(), 0, 0 );
	g_szRichPresenceNiceMap[0] = '\0';
	g_szRichPresenceOverride[0] = '\0';
	g_szRichPresenceGameMode[0] = '\0';
}

void CSteam3Client::InitClient()
{
	if ( m_bLoggedOn )
		return;

	m_bLoggedOn = true;
	_unlink( "steam_appid.txt" );
	if ( !std::getenv( "SteamAppId" ) )
	{
		int appID = GetGameAppID();
		if ( appID > 0 )
		{
			FILE* file = std::fopen( "steam_appid.txt", "w+" );
			if ( file )
			{
				std::fprintf( file, "%d\n", appID );
				std::fclose( file );
			}
		}
	}
	if ( !SteamAPI_Init() )
		Sys_Error( "Failed to initalize authentication interface. Exiting...\n" );
	if ( SteamInput() )
		SteamInput()->Init( false );
	m_bLogOnResult = false;
}

void CSteam3Client::RunFrame()
{
	SteamAPI_RunCallbacks();
}

void Steam_ClientRunFrame()
{
	s_Steam3Client.RunFrame();
}

void Steam_InitClient()
{
	s_Steam3Client.InitClient();
}

int Steam_GSInitiateGameConnection(
	void* data, int maxData, uint64 serverID, uint32 serverIP, uint16 serverPort, qboolean secure )
{
	return s_Steam3Client.InitiateGameConnection( data, maxData, serverID, serverIP, serverPort, secure != 0 );
}

void Steam_GSTerminateGameConnection()
{
	s_Steam3Client.TerminateConnection();
}

void Steam_ShutdownClient()
{
	s_Steam3Client.Shutdown();
}

void Steam_ActivateInputSet( bool inMenu )
{
	s_Steam3Client.ActivateInputSet( inMenu );
}

void Steam_UpdateRichPresence()
{
	s_Steam3Client.UpdateRichPresence();
}

void Steam_SetRichPresenceInfo( const char* mapName )
{
	s_Steam3Client.SetRichPresenceInfo( mapName );
}

void Steam_SetRichPresenceOverride( const char* text )
{
	s_Steam3Client.SetRichPresenceOverride( text );
}

void Steam_SetRichPresenceGameMode( const char* text )
{
	s_Steam3Client.SetRichPresenceGameMode( text );
}

void Steam_ShowOnScreenKeyboard( int fieldX, int fieldY )
{
	s_Steam3Client.ShowOnScreenKeyboard( fieldX, fieldY );
}
