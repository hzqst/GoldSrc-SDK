#include "common.h"
#include "net.h"
#include "client.h"
#include "cvar.h"
#include "conprint.h"
#include "mem.h"
#include "sys.h"
#include "info.h"
#include "net_api.h"

#include <cstdio>

extern double g_flLatency;

typedef struct net_api_query_s
{
	struct net_api_query_s* next;
	int context;
	int type;
	int flags;
	double requesttime;
	double timeout;
	netadr_t request;
	net_api_response_func_t callback;
} net_api_query_t;

static_assert( sizeof( net_api_query_t ) == 56 );

constexpr int NETAPI_RESPONSE_SIZE = 2048;

static net_api_query_t* g_queries;
static net_adrlist_t* g_addresses;

static void Net_APIClearQueries()
{
	net_api_query_t* p = g_queries;
	while ( p )
	{
		net_api_query_t* next = p->next;
		Mem_Free( p );
		p = next;
	}
	g_queries = nullptr;
}

static void Net_KillServerList()
{
	net_adrlist_t* p = g_addresses;
	while ( p )
	{
		net_adrlist_t* next = p->next;
		Mem_Free( p );
		p = next;
	}
	g_addresses = nullptr;
}

void Net_APIShutDown()
{
	Net_APIClearQueries();
	Net_KillServerList();
}

void Net_APIInit()
{
	g_queries = nullptr;
	g_addresses = nullptr;
}

static net_api_query_t* Net_APIFind( int type, netadr_t* remote )
{
	for ( net_api_query_t* query = g_queries; query; query = query->next )
	{
		if ( query->type != type )
			continue;

		if ( query->request.type == NA_BROADCAST && ( remote->type == NA_BROADCAST || remote->type == NA_IP ) )
			return query;

		if ( NET_CompareAdr( *remote, query->request ) )
			return query;
	}

	return nullptr;
}

static net_api_query_t* Net_APIFindContext( int context )
{
	for ( net_api_query_t* p = g_queries; p; p = p->next )
	{
		if ( p->context == context )
			return p;
	}
	return nullptr;
}

static void Net_APIKill( net_api_query_t* kill )
{
	if ( !kill )
		return;

	net_api_query_t* newlist = nullptr;
	net_api_query_t* p = g_queries;
	while ( p )
	{
		net_api_query_t* next = p->next;
		if ( p == kill )
			Mem_Free( p );
		else
		{
			p->next = newlist;
			newlist = p;
		}
		p = next;
	}
	g_queries = newlist;
}

static void Net_CancelRequest( int context )
{
	net_api_query_t* p = Net_APIFindContext( context );
	if ( p )
		Net_APIKill( p );
}

static void Net_CancelAllRequests()
{
	Net_APIClearQueries();
}

static void Net_APIFailed( net_api_query_t* p )
{
	if ( p->callback )
	{
		net_response_t response;
		response.error = NET_ERROR_TIMEOUT;
		response.context = p->context;
		response.type = p->type;
		response.remote_address = p->request;
		response.ping = realtime - p->requesttime;
		response.response = nullptr;
		p->callback( &response );
	}
}

void Net_APICheckTimeouts()
{
	net_api_query_t* newlist = nullptr;
	net_api_query_t* p = g_queries;
	while ( p )
	{
		net_api_query_t* next = p->next;
		if ( realtime > p->requesttime + p->timeout )
		{
			Net_APIFailed( p );
			Mem_Free( p );
		}
		else
		{
			p->next = newlist;
			newlist = p;
		}
		p = next;
	}
	g_queries = newlist;
}

static void Net_GetBatchServerList( sizebuf_t* msg, int batch )
{
	constexpr int S2M_GET_SERVERS_BATCH = 'e';
	MSG_WriteByte( msg, S2M_GET_SERVERS_BATCH );
	MSG_WriteLong( msg, batch );
}

static void Net_SendRequest(
	int context, int request, int flags, double timeout, netadr_t* remote_address, net_api_response_func_t response )
{
	constexpr int NET_API_REQUEST_BYTES = 1024;
	constexpr int CONNECTIONLESS_HEADER = -1;
	byte data[NET_API_REQUEST_BYTES];
	sizebuf_t msg;
	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "Net_SendRequest";
	msg.data = data;
	msg.maxsize = sizeof( data );
	msg.cursize = 0;

	net_api_query_t* q = static_cast<net_api_query_t*>( Mem_ZeroMalloc( sizeof( net_api_query_t ) ) );
	q->context = context;
	q->type = request;
	q->flags = flags;
	q->requesttime = realtime;
	q->timeout = timeout;
	q->request = *remote_address;
	q->callback = response;
	q->next = g_queries;
	g_queries = q;

	switch ( request )
	{
	case NETAPI_REQUEST_SERVERLIST:
		Net_KillServerList();
		Net_GetBatchServerList( &msg, 0 );
		break;
	case NETAPI_REQUEST_PING:
		MSG_WriteLong( &msg, CONNECTIONLESS_HEADER );
		MSG_WriteString( &msg, "ping\n" );
		break;
	case NETAPI_REQUEST_RULES:
		MSG_WriteLong( &msg, CONNECTIONLESS_HEADER );
		MSG_WriteString( &msg, "rules\n" );
		break;
	case NETAPI_REQUEST_PLAYERS:
		MSG_WriteLong( &msg, CONNECTIONLESS_HEADER );
		MSG_WriteString( &msg, "players\n" );
		break;
	case NETAPI_REQUEST_DETAILS:
		MSG_WriteLong( &msg, CONNECTIONLESS_HEADER );
		MSG_WriteString( &msg, "details\n" );
		break;
	default:
		Con_Printf( "Unknown request type:  %i\n", request );
		break;
	}

	if ( msg.cursize > 0 )
		NET_SendPacket( NS_CLIENT, msg.cursize, msg.data, q->request );
}

static char* Net_AdrToString( netadr_t* address )
{
	return NET_AdrToString( *address );
}

static int Net_CompareAdr( netadr_t* first, netadr_t* second )
{
	return NET_CompareAdr( *first, *second );
}

static int Net_StringToAdr( char* text, netadr_t* address )
{
	return NET_StringToAdr( text, address );
}

static void Net_InitNetworking()
{
	NET_Config( true );
}

static int Net_GetPacketLoss()
{
	constexpr int MAX_PACKET_LOSS_PERCENT = 100;
	int packet_loss = 0;
	if ( cls.state == ca_active )
	{
		packet_loss = static_cast<int>( cls.packet_loss );
		if ( packet_loss < 0 )
			packet_loss = 0;
		if ( packet_loss > MAX_PACKET_LOSS_PERCENT )
			packet_loss = MAX_PACKET_LOSS_PERCENT;
	}
	return packet_loss;
}

static void Net_Status( net_status_t* status )
{
	if ( !status )
		return;

	status->connected = cls.state != ca_disconnected;
	status->local_address = net_local_adr;
	status->remote_address = cls.netchan.remote_address;
	if ( status->connected )
	{
		status->packet_loss = Net_GetPacketLoss();
		status->latency = g_flLatency;
		status->connection_time = realtime - cls.netchan.connect_time;
	}
	else
	{
		status->packet_loss = 0;
		status->latency = 0.0;
		status->connection_time = 0.0;
	}
	status->rate = rate.value;
}

static void NET_ParsePlayersResponse( char* returninfo )
{
	constexpr int PLAYER_FIELD_SIZE = 256;
	char text[PLAYER_FIELD_SIZE];

	*returninfo = 0;
	MSG_BeginReading();
	MSG_ReadLong();
	MSG_ReadByte();
	int playerCount = MSG_ReadByte();
	Info_SetValueForKey( returninfo, "players", va( "%i", playerCount ), NETAPI_RESPONSE_SIZE );
	for ( int index = 0; index < playerCount; ++index )
	{
		int playerNumber = MSG_ReadByte();
		char* name = MSG_ReadString();
		if ( !name )
			break;

		std::snprintf( text, sizeof( text ), "%s", name );
		text[sizeof( text ) - 1] = 0;
		Info_SetValueForKey( returninfo, va( "p%iname", playerNumber ), text, NETAPI_RESPONSE_SIZE );

		std::snprintf( text, sizeof( text ), "%i", MSG_ReadLong() );
		text[sizeof( text ) - 1] = 0;
		Info_SetValueForKey( returninfo, va( "p%ifrags", playerNumber ), text, NETAPI_RESPONSE_SIZE );

		std::snprintf( text, sizeof( text ), "%.2f", MSG_ReadFloat() );
		text[sizeof( text ) - 1] = 0;
		Info_SetValueForKey( returninfo, va( "p%itime", playerNumber ), text, NETAPI_RESPONSE_SIZE );
	}
}

static void NET_ParseRulesResponse( char* returninfo )
{
	constexpr int RULE_KEY_SIZE = 64;
	constexpr int RULE_VALUE_SIZE = 1024;
	char keyText[RULE_KEY_SIZE];
	char valueText[RULE_VALUE_SIZE];

	*returninfo = 0;
	MSG_BeginReading();
	MSG_ReadLong();
	MSG_ReadByte();
	int ruleCount = MSG_ReadShort();
	Info_SetValueForKey( returninfo, "rules", va( "%i", ruleCount ), NETAPI_RESPONSE_SIZE );
	for ( int index = 0; index < ruleCount; ++index )
	{
		char* key = MSG_ReadString();
		if ( !key || !key[0] )
			break;

		Q_strncpy( keyText, key, RULE_KEY_SIZE - 1 );
		keyText[RULE_KEY_SIZE - 1] = 0;

		char* value = MSG_ReadString();
		if ( !value )
			break;

		Q_strncpy( valueText, value, RULE_VALUE_SIZE - 1 );
		valueText[RULE_VALUE_SIZE - 1] = 0;
		Info_SetValueForKey( returninfo, keyText, valueText, NETAPI_RESPONSE_SIZE );
	}
}

static void NET_ParseServerInfoResponse( char* returninfo )
{
	*returninfo = 0;
	MSG_BeginReading();
	MSG_ReadLong();
	MSG_ReadByte();
	MSG_ReadString();

	Info_SetValueForKey( returninfo, "hostname", MSG_ReadString(), NETAPI_RESPONSE_SIZE );
	Info_SetValueForKey( returninfo, "map", MSG_ReadString(), NETAPI_RESPONSE_SIZE );
	Info_SetValueForKey( returninfo, "gamedir", MSG_ReadString(), NETAPI_RESPONSE_SIZE );
	Info_SetValueForKey( returninfo, "gamedesc", MSG_ReadString(), NETAPI_RESPONSE_SIZE );
	Info_SetValueForKey( returninfo, "current", va( "%i", MSG_ReadByte() ), NETAPI_RESPONSE_SIZE );
	Info_SetValueForKey( returninfo, "max", va( "%i", MSG_ReadByte() ), NETAPI_RESPONSE_SIZE );
	Info_SetValueForKey( returninfo, "protocol", va( "%i", MSG_ReadByte() ), NETAPI_RESPONSE_SIZE );
	Info_SetValueForKey( returninfo, "type", va( "%c", MSG_ReadByte() ), NETAPI_RESPONSE_SIZE );
	Info_SetValueForKey( returninfo, "os", va( "%c", MSG_ReadByte() ), NETAPI_RESPONSE_SIZE );

	const char* password = MSG_ReadByte() == 0 ? "N" : "Y";
	Info_SetValueForKey( returninfo, "pw", password, NETAPI_RESPONSE_SIZE );

	if ( MSG_ReadByte() != 0 )
	{
		Info_SetValueForKey( returninfo, "mod", "Y", NETAPI_RESPONSE_SIZE );
		Info_SetValueForKey( returninfo, "mod_url", MSG_ReadString(), NETAPI_RESPONSE_SIZE );
		Info_SetValueForKey( returninfo, "mod_dl", MSG_ReadString(), NETAPI_RESPONSE_SIZE );
		MSG_ReadString();
		Info_SetValueForKey( returninfo, "mod_ver", va( "%i", MSG_ReadLong() ), NETAPI_RESPONSE_SIZE );
		Info_SetValueForKey( returninfo, "mod_size", va( "%i", MSG_ReadLong() ), NETAPI_RESPONSE_SIZE );

		const char* serverOnly = MSG_ReadByte() == 0 ? "N" : "Y";
		Info_SetValueForKey( returninfo, "svonly", serverOnly, NETAPI_RESPONSE_SIZE );

		const char* clientDll = MSG_ReadByte() == 0 ? "N" : "Y";
		Info_SetValueForKey( returninfo, "cldll", clientDll, NETAPI_RESPONSE_SIZE );
	}
	else
	{
		Info_SetValueForKey( returninfo, "mod", "N", NETAPI_RESPONSE_SIZE );
	}

	const char* secure = MSG_ReadByte() == 0 ? "0" : "1";
	Info_SetValueForKey( returninfo, "secure", secure, NETAPI_RESPONSE_SIZE );
}

static int NET_ParseServerList()
{
	constexpr int ADDRESS_TEXT_SIZE = 128;
	constexpr int SERVER_LIST_PACKET_SIZE = 1024;
	char addressText[ADDRESS_TEXT_SIZE];

	MSG_BeginReading();
	MSG_ReadLong();
	MSG_ReadByte();
	MSG_ReadByte();
	int unique = MSG_ReadLong();

	for ( int remaining = ( net_message.cursize - msg_readcount ) / 6; remaining > 0; --remaining )
	{
		Q_memset( addressText, 0, sizeof( addressText ) );
		int ip0 = static_cast<byte>( MSG_ReadByte() );
		int ip1 = static_cast<byte>( MSG_ReadByte() );
		int ip2 = static_cast<byte>( MSG_ReadByte() );
		int ip3 = static_cast<byte>( MSG_ReadByte() );
		int port = static_cast<unsigned short>( BigShort( static_cast<short>( MSG_ReadShort() ) ) );
		std::snprintf( addressText, sizeof( addressText ), "%i.%i.%i.%i:%i", ip0, ip1, ip2, ip3, port );
		addressText[sizeof( addressText ) - 1] = 0;

		netadr_t address;
		if ( NET_StringToAdr( addressText, &address ) )
		{
			net_adrlist_t* entry = static_cast<net_adrlist_t*>( Mem_ZeroMalloc( sizeof( net_adrlist_t ) ) );
			entry->remote_address = address;
			entry->next = g_addresses;
			g_addresses = entry;
		}
	}

	if ( unique != 0 )
	{
		byte data[SERVER_LIST_PACKET_SIZE];
		sizebuf_t msg;
		Q_memset( &msg, 0, sizeof( msg ) );
		msg.buffername = "NET_ParseServerList";
		msg.data = data;
		msg.cursize = 0;
		msg.maxsize = sizeof( data );
		Net_GetBatchServerList( &msg, unique );
		if ( msg.cursize != 0 )
			NET_SendPacket( NS_CLIENT, msg.cursize, msg.data, net_from );
		return 0;
	}

	return 1;
}

static int Net_APIRespond( int type, char* returninfo, void ( *parse )( char* ) )
{
	net_api_query_t* query = Net_APIFind( type, &net_from );
	if ( !query )
		return 0;

	if ( query->callback )
	{
		if ( parse )
			parse( returninfo );
		net_response_t response;
		response.error = NET_SUCCESS;
		response.context = query->context;
		response.type = query->type;
		response.remote_address = net_from;
		response.ping = realtime - query->requesttime;
		response.response = returninfo;
		query->callback( &response );
	}
	if ( query->flags & FNETAPI_MULTIPLE_RESPONSE )
		return 1;
	Net_APIKill( query );
	return 1;
}

int Net_APIProcess()
{
	char returninfo[NETAPI_RESPONSE_SIZE];
	returninfo[0] = 0;

	MSG_BeginReading();
	MSG_ReadLong();
	char messageType = static_cast<char>( MSG_ReadByte() );
	switch ( messageType )
	{
	case 'D':
		return Net_APIRespond( NETAPI_REQUEST_PLAYERS, returninfo, NET_ParsePlayersResponse );
	case 'E':
		return Net_APIRespond( NETAPI_REQUEST_RULES, returninfo, NET_ParseRulesResponse );
	case 'f':
	{
		if ( !NET_ParseServerList() )
			return 0;

		net_api_query_t* query = Net_APIFind( NETAPI_REQUEST_SERVERLIST, &net_from );
		int handled = 0;
		if ( query )
		{
			if ( query->callback )
			{
				net_response_t response;
				Q_memset( &response, 0, offsetof( net_response_t, response ) + sizeof( response.response ) );
				response.error = NET_SUCCESS;
				response.context = query->context;
				response.type = query->type;
				response.ping = realtime - query->requesttime;
				response.response = g_addresses;
				query->callback( &response );
			}

			if ( ( query->flags & FNETAPI_MULTIPLE_RESPONSE ) == 0 )
				Net_APIKill( query );
			handled = 1;
		}

		Net_KillServerList();
		return handled;
	}
	case 'j':
		return Net_APIRespond( NETAPI_REQUEST_PING, returninfo, nullptr );
	case 'm':
		return Net_APIRespond( NETAPI_REQUEST_DETAILS, returninfo, NET_ParseServerInfoResponse );
	default:
		Con_Printf( "Net_APIProcess: Unknown message type %i\n", static_cast<int>( messageType ) );
		return 0;
	}
}

net_api_t netapi = { Net_InitNetworking, Net_Status, Net_SendRequest, Net_CancelRequest, Net_CancelAllRequests,
	Net_AdrToString, Net_CompareAdr, Net_StringToAdr, Info_ValueForKey, Info_RemoveKey, Info_SetValueForKey };
