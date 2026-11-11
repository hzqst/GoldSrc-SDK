#if !defined( _WIN32 )
#include <cerrno>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int SOCKET;
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#endif

#include "net.h"
#include "client.h"
#include "server.h"
#include "server_static.h"
#include "common.h"
#include "conprint.h"
#include "sys.h"
#include "cvar.h"
#include "mem.h"
#include "cmd.h"
#include "steam_api.h"
#include "render.h"
#include "vid.h"
#include "kbutton.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

void Draw_SetTextColor( float red, float green, float blue );
int Draw_String( int x, int y, char* text );
void Draw_FillRGBA( int x, int y, int width, int height, int red, int green, int blue, int alpha );
extern double host_frametime;
extern cvar_t scr_graphheight;
extern cvar_t cl_showfps;
extern cvar_t cl_cmdrate;
extern cvar_t cl_updaterate;

constexpr int MAX_LOOPBACK = 4;
constexpr int LOOPBACK_MESSAGE_BYTES = 6037;
constexpr int LOOPBACK_CHANNELS = 2;
constexpr int NET_SOCKET_SOURCES = 3;

typedef struct
{
	byte data[LOOPBACK_MESSAGE_BYTES];
	int datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t msgs[MAX_LOOPBACK];
	int get;
	int send;
} loopback_t;

static_assert( sizeof( loopmsg_t ) == 6044 );
static_assert( sizeof( loopback_t ) == 24184 );

static loopback_t loopbacks[LOOPBACK_CHANNELS];
int use_thread;
qboolean net_thread_initialized;
SOCKET ip_sockets[NET_SOCKET_SOURCES];
#if defined( _WIN32 )
CRITICAL_SECTION net_thread_cs;
#endif

qboolean NET_IsSteamFakeIP( netsrc_t source, const sockaddr* destination );
int NET_SteamFakeIPSendTo(
	netsrc_t source, SOCKET socket, char* data, int length, int flags, const sockaddr* destination, int addressLength );

const char* NET_ErrorString( int error )
{
#if !defined( _WIN32 )
	return strerror( error );
#else
	switch ( error )
	{
	case WSAEINTR:
		return "WSAEINTR";
	case WSAEBADF:
		return "WSAEBADF";
	case WSAEACCES:
		return "WSAEACCES";
	case WSAEFAULT:
		return "WSAEFAULT";
	case WSAEINVAL:
		return "WSAEINVAL";
	case WSAEMFILE:
		return "WSAEMFILE";
	case WSAEWOULDBLOCK:
		return "WSAEWOULDBLOCK";
	case WSAEINPROGRESS:
		return "WSAEINPROGRESS";
	case WSAEALREADY:
		return "WSAEALREADY";
	case WSAENOTSOCK:
		return "WSAENOTSOCK";
	case WSAEDESTADDRREQ:
		return "WSAEDESTADDRREQ";
	case WSAEMSGSIZE:
		return "WSAEMSGSIZE";
	case WSAEPROTOTYPE:
		return "WSAEPROTOTYPE";
	case WSAENOPROTOOPT:
		return "WSAENOPROTOOPT";
	case WSAEPROTONOSUPPORT:
		return "WSAEPROTONOSUPPORT";
	case WSAESOCKTNOSUPPORT:
		return "WSAESOCKTNOSUPPORT";
	case WSAEOPNOTSUPP:
		return "WSAEOPNOTSUPP";
	case WSAEPFNOSUPPORT:
		return "WSAEPFNOSUPPORT";
	case WSAEAFNOSUPPORT:
		return "WSAEAFNOSUPPORT";
	case WSAEADDRINUSE:
		return "WSAEADDRINUSE";
	case WSAEADDRNOTAVAIL:
		return "WSAEADDRNOTAVAIL";
	case WSAENETDOWN:
		return "WSAENETDOWN";
	case WSAENETUNREACH:
		return "WSAENETUNREACH";
	case WSAENETRESET:
		return "WSAENETRESET";
	case WSAECONNABORTED:
		return "WSWSAECONNABORTEDAEINTR";
	case WSAECONNRESET:
		return "WSAECONNRESET";
	case WSAENOBUFS:
		return "WSAENOBUFS";
	case WSAEISCONN:
		return "WSAEISCONN";
	case WSAENOTCONN:
		return "WSAENOTCONN";
	case WSAESHUTDOWN:
		return "WSAESHUTDOWN";
	case WSAETOOMANYREFS:
		return "WSAETOOMANYREFS";
	case WSAETIMEDOUT:
		return "WSAETIMEDOUT";
	case WSAECONNREFUSED:
		return "WSAECONNREFUSED";
	case WSAELOOP:
		return "WSAELOOP";
	case WSAENAMETOOLONG:
		return "WSAENAMETOOLONG";
	case WSAEHOSTDOWN:
		return "WSAEHOSTDOWN";
	case WSASYSNOTREADY:
		return "WSASYSNOTREADY";
	case WSAVERNOTSUPPORTED:
		return "WSAVERNOTSUPPORTED";
	case WSANOTINITIALISED:
		return "WSANOTINITIALISED";
	case WSAEDISCON:
		return "WSAEDISCON";
	case WSAHOST_NOT_FOUND:
		return "WSAHOST_NOT_FOUND";
	case WSATRY_AGAIN:
		return "WSATRY_AGAIN";
	case WSANO_RECOVERY:
		return "WSANO_RECOVERY";
	case WSANO_DATA:
		return "WSANO_DATA";
	default:
		return "NO ERROR";
	}
#endif
}

#pragma pack( push, 1 )
typedef struct
{
	int netID;
	int sequenceNumber;
	byte packetID;
} SPLITPACKET;
#pragma pack( pop )

static_assert( sizeof( SPLITPACKET ) == 9 );

constexpr int MAX_ROUTABLE_PACKET = 1400;
constexpr int SPLIT_PAYLOAD_BYTES = MAX_ROUTABLE_PACKET - sizeof( SPLITPACKET );
constexpr int SPLIT_PACKET_ID = -2;
constexpr int SPLIT_PACKET_NUMBER_SHIFT = 4;
constexpr float SHOW_SPLIT_PACKETS = 4.0f;

void SockadrToNetadr( sockaddr* source, netadr_t* destination )
{
	if ( source->sa_family == AF_INET )
	{
		const sockaddr_in* address = reinterpret_cast<const sockaddr_in*>( source );
		destination->type = NA_IP;
		Q_memcpy( destination->ip, &address->sin_addr.s_addr, sizeof( destination->ip ) );
		destination->port = address->sin_port;
	}
}

int NET_SendLong( netsrc_t source, SOCKET socket, const char* data, int length, int flags, const sockaddr* destination,
	int addressLength )
{
	static int gSequenceNumber = 1;
	if ( source != NS_SERVER || length <= MAX_ROUTABLE_PACKET )
	{
		if ( NET_IsSteamFakeIP( source, destination ) )
			return NET_SteamFakeIPSendTo(
				source, socket, const_cast<char*>( data ), length, flags, destination, addressLength );
		return sendto( socket, data, length, flags, destination, addressLength );
	}
	gSequenceNumber = static_cast<int>( static_cast<unsigned int>( gSequenceNumber ) + 1 );
	if ( gSequenceNumber < 0 )
		gSequenceNumber = 1;
	struct
	{
		SPLITPACKET header;
		byte data[SPLIT_PAYLOAD_BYTES];
	} packet;
	packet.header.netID = SPLIT_PACKET_ID;
	packet.header.sequenceNumber = gSequenceNumber;
	unsigned int packetCount = ( static_cast<unsigned int>( length ) + SPLIT_PAYLOAD_BYTES - 1 ) / SPLIT_PAYLOAD_BYTES;
	int packetNumber = 0;
	int remaining = length;
	unsigned int sent = 0;
	const char* position = data;
	while ( remaining > 0 )
	{
		int size = static_cast<unsigned int>( remaining ) > SPLIT_PAYLOAD_BYTES ? SPLIT_PAYLOAD_BYTES : remaining;
		packet.header.packetID = static_cast<byte>( packetCount + ( packetNumber << SPLIT_PACKET_NUMBER_SHIFT ) );
		Q_memcpy( packet.data, position, size );
		if ( net_showpackets.value == SHOW_SPLIT_PACKETS )
		{
			netadr_t address;
			SockadrToNetadr( const_cast<sockaddr*>( destination ), &address );
			Con_DPrintf( "Sending split %i of %i with %i bytes and seq %i to %s\n", packetNumber + 1, packetCount, size,
				gSequenceNumber, NET_AdrToString( address ) );
		}
		int packetLength = size + sizeof( SPLITPACKET );
		int result;
		if ( NET_IsSteamFakeIP( NS_SERVER, destination ) )
			result = NET_SteamFakeIPSendTo( NS_SERVER, socket, reinterpret_cast<char*>( &packet ), packetLength, flags,
				destination, addressLength );
		else
			result = sendto(
				socket, reinterpret_cast<const char*>( &packet ), packetLength, flags, destination, addressLength );
		if ( result < 0 )
			return result;
		if ( result >= size )
			sent += size;
		position += SPLIT_PAYLOAD_BYTES;
		remaining -= size;
		++packetNumber;
	}
	return static_cast<int>( sent );
}

void NET_SendPacket( netsrc_t source, int length, void* data, netadr_t destination )
{
	if ( destination.type == NA_LOOPBACK )
	{
#if !defined( _WIN32 )
#else
		if ( use_thread && net_thread_initialized )
			EnterCriticalSection( &net_thread_cs );
#endif
		loopback_t* loop = &loopbacks[source ^ 1];
		loopmsg_t* message = &loop->msgs[loop->send & ( MAX_LOOPBACK - 1 )];
		loop->send = static_cast<int>( static_cast<unsigned int>( loop->send ) + 1 );
		Q_memcpy( message->data, data, length );
		message->datalen = length;
#if !defined( _WIN32 )
#else
		if ( use_thread && net_thread_initialized )
			LeaveCriticalSection( &net_thread_cs );
#endif
		return;
	}
	if ( destination.type != NA_BROADCAST && destination.type != NA_IP )
	{
		Sys_Error( "NET_SendPacket: bad address type" );
		return;
	}
	SOCKET socket = ip_sockets[source];
	if ( !socket )
		return;
	sockaddr_in address;
	Q_memset( &address, 0, sizeof( address ) );
	address.sin_family = AF_INET;
	address.sin_port = destination.port;
	if ( destination.type == NA_BROADCAST )
		address.sin_addr.s_addr = INADDR_BROADCAST;
	else
		Q_memcpy( &address.sin_addr.s_addr, destination.ip, sizeof( destination.ip ) );
#if !defined( _WIN32 )
	if ( NET_SendLong( source, socket, static_cast<const char*>( data ), length, 0,
			 reinterpret_cast<sockaddr*>( &address ), sizeof( address ) )
		 != -1 )
		return;
	int error = errno;
	if ( error == EAGAIN || error == ECONNREFUSED || error == ECONNRESET
		 || ( error == EADDRNOTAVAIL && destination.type == NA_BROADCAST ) )
		return;
	if ( cls.state == ca_dedicated )
		Con_Printf( "NET_SendPacket ERROR: %s\n", strerror( error ) );
	else
	{
		if ( error != EADDRNOTAVAIL && error != ENOBUFS )
			Sys_Error( "NET_SendPacket ERROR: %s\n", strerror( error ) );
		Con_DPrintf( "NET_SendPacket Warning: %s : %s\n", strerror( error ), NET_AdrToString( destination ) );
	}
#else
	if ( NET_SendLong( source, socket, static_cast<const char*>( data ), length, 0,
			 reinterpret_cast<sockaddr*>( &address ), sizeof( address ) )
		 != SOCKET_ERROR )
		return;
	int error = WSAGetLastError();
	if ( error == WSAEWOULDBLOCK || error == WSAECONNREFUSED || error == WSAECONNRESET
		 || ( error == WSAEADDRNOTAVAIL && destination.type == NA_BROADCAST ) )
		return;
	if ( cls.state == ca_dedicated ) // let dedicated servers continue after errors
		Con_Printf( "NET_SendPacket ERROR: %s\n", NET_ErrorString( error ) );
	else
	{
		if ( error != WSAEADDRNOTAVAIL && error != WSAENOBUFS )
			Sys_Error( "NET_SendPacket ERROR: %s\n", NET_ErrorString( error ) );
		Con_DPrintf( "NET_SendPacket Warning: %s : %s\n", NET_ErrorString( error ), NET_AdrToString( destination ) );
	}
#endif
}

constexpr int MAX_RECEIVE_PACKET_BYTES = 6010;
constexpr int MAX_NETWORK_MESSAGE_BYTES = 65536;
constexpr int MAX_SPLIT_PACKETS = 5;
constexpr int SPLIT_PACKET_COUNT_MASK = 15;

typedef struct packetlag_s
{
	byte* pPacketData;
	int nSize;
	netadr_t net_from;
	float receivedTime;
	packetlag_s* pNext;
	packetlag_s* pPrev;
} packetlag_t;

// Split long packets.  Anything over 1460 is failing on some routers
typedef struct
{
	int currentSequence;
	int splitCount;
	int totalSize;
	char buffer[MAX_RECEIVE_PACKET_BYTES]; // This has to be big enough to hold the largest message
} LONGPACKET;

typedef struct net_messages_s
{
	net_messages_s* next;
	qboolean preallocated;
	byte* buffer;
	netadr_t from;
	int buffersize;
} net_messages_t;

static_assert( sizeof( packetlag_t ) == 40 );
static_assert( sizeof( LONGPACKET ) == 6024 );
static_assert( sizeof( net_messages_t ) == 36 );

static packetlag_t g_pLagData[NET_SOCKET_SOURCES]; // List of lag structures, if fakelag is set.
static int losscount[NET_SOCKET_SOURCES];
static LONGPACKET gNetSplit;
static int gNetSplitFlags[MAX_SPLIT_PACKETS] = { -1, -1, -1, -1, -1 };
static net_messages_t* messages[NET_SOCKET_SOURCES];
static net_messages_t* normalqueue;
static float gFakeLag;
byte net_message_buffer[MAX_NETWORK_MESSAGE_BYTES];
byte in_message_buf[MAX_NETWORK_MESSAGE_BYTES];
sizebuf_t net_message;
sizebuf_t in_message;
netadr_t net_from;
netadr_t in_from;

cvar_t fakelag = { const_cast<char*>( "fakelag" ), const_cast<char*>( "0.0" ), 0, 0.0f, nullptr };
cvar_t fakeloss = { const_cast<char*>( "fakeloss" ), const_cast<char*>( "0.0" ), 0, 0.0f, nullptr };

int RandomLong( int low, int high );
int NET_SteamFakeIPRecvFrom(
	netsrc_t source, char* data, int length, int flags, int fallbackResult, sockaddr* from, int* fromLength );

void NET_ClearLaggedList( packetlag_t* list )
{
	packetlag_t* packet = list->pNext;
	while ( packet && packet != list )
	{
		packetlag_t* next = packet->pNext;
		packet->pPrev->pNext = packet->pNext;
		packet->pNext->pPrev = packet->pPrev;
		packet->pPrev = nullptr;
		packet->pNext = nullptr;
		if ( packet->pPacketData )
		{
			Mem_Free( packet->pPacketData );
			packet->pPacketData = nullptr;
		}
		Mem_Free( packet );
		packet = next;
	}
	list->pPrev = list->pNext = list;
}

void NET_AdjustLag()
{
	constexpr double MAX_LAG_ADJUST_INTERVAL = 0.1;
	constexpr double LAG_ADJUST_MILLISECONDS_PER_SECOND = 200.0;
	static double lasttime;
	double elapsed = realtime - lasttime;
	lasttime = realtime;
	if ( elapsed < 0.0 )
		elapsed = 0.0;
	if ( elapsed > MAX_LAG_ADJUST_INTERVAL )
		elapsed = MAX_LAG_ADJUST_INTERVAL;
	if ( sv_cheats.value != 0.0f || fakelag.value == 0.0f )
	{
		if ( fakelag.value != gFakeLag )
		{
			float difference = fakelag.value - gFakeLag;
			float step = static_cast<float>( elapsed * LAG_ADJUST_MILLISECONDS_PER_SECOND );
			if ( step > std::fabs( difference ) )
				step = std::fabs( difference );
			if ( difference < 0.0f )
				step = -step;
			gFakeLag += step;
		}
	}
	else
	{
		Con_Printf( "Server must enable cheats to activate fakelag\n" );
		Cvar_SetValue( const_cast<char*>( "fakelag" ), 0.0f );
		gFakeLag = 0.0f;
	}
}

qboolean NET_LagPacket( qboolean newData, netsrc_t source, netadr_t* from, sizebuf_t* data )
{
	constexpr int LOSS_PERCENT_MAX = 100;
	constexpr int MIN_LOSS_INTERVAL = 2;
	constexpr double MILLISECONDS_PER_SECOND = 1000.0;
	if ( gFakeLag <= 0.0f )
	{
#if !defined( _WIN32 )
#else
		if ( use_thread && net_thread_initialized )
			EnterCriticalSection( &net_thread_cs );
#endif
		NET_ClearLaggedList( &g_pLagData[NS_CLIENT] );
		NET_ClearLaggedList( &g_pLagData[NS_MULTICAST] );
		NET_ClearLaggedList( &g_pLagData[NS_SERVER] );
#if !defined( _WIN32 )
#else
		if ( use_thread && net_thread_initialized )
			LeaveCriticalSection( &net_thread_cs );
#endif
		return newData;
	}
	float time = static_cast<float>( realtime );
	if ( newData )
	{
		float loss = fakeloss.value;
		if ( loss != 0.0f )
		{
			if ( sv_cheats.value == 0.0f )
				Cvar_SetValue( const_cast<char*>( "fakeloss" ), 0.0f );
			else
			{
				losscount[source] = static_cast<int>( static_cast<unsigned int>( losscount[source] ) + 1 );
				if ( loss <= 0.0f )
				{
					int interval = static_cast<int>( std::fabs( loss ) );
					if ( interval < MIN_LOSS_INTERVAL )
						interval = MIN_LOSS_INTERVAL;
					if ( losscount[source] % interval == 0 )
						return false;
				}
				else if ( RandomLong( 0, LOSS_PERCENT_MAX ) <= static_cast<int>( fakeloss.value ) )
					return false;
			}
		}
		packetlag_t* packet = static_cast<packetlag_t*>( Mem_ZeroMalloc( sizeof( packetlag_t ) ) );
		packetlag_t* list = &g_pLagData[source];
		if ( packet->pPrev || packet->pNext )
			Con_Printf( "Packet already linked\n" );
		else
		{
			packet->pPrev = list->pPrev;
			list->pPrev->pNext = packet;
			list->pPrev = packet;
			packet->pNext = list;
			packet->pPacketData = static_cast<byte*>( Mem_Malloc( data->cursize ) );
			Q_memcpy( packet->pPacketData, data->data, data->cursize );
			packet->nSize = data->cursize;
			packet->receivedTime = time;
			packet->net_from = *from;
		}
	}
	packetlag_t* list = &g_pLagData[source];
	packetlag_t* packet = list->pNext;
	if ( packet == list )
		return false;
	float latestTime = time - static_cast<float>( gFakeLag / MILLISECONDS_PER_SECOND );
	while ( !( latestTime >= packet->receivedTime ) )
	{
		packet = packet->pNext;
		if ( packet == list )
			return false;
	}
	packet->pPrev->pNext = packet->pNext;
	packet->pNext->pPrev = packet->pPrev;
	packet->pPrev = nullptr;
	packet->pNext = nullptr;
	Q_memcpy( in_message.data, packet->pPacketData, packet->nSize );
	in_message.cursize = packet->nSize;
	in_from = packet->net_from;
	if ( packet->pPacketData )
		Mem_Free( packet->pPacketData );
	Mem_Free( packet );
	return true;
}

qboolean NET_GetLong( char* data, int length, int* outputLength )
{
	SPLITPACKET* header = reinterpret_cast<SPLITPACKET*>( data );
	int sequence = header->sequenceNumber;
	unsigned int packetNumber = header->packetID >> SPLIT_PACKET_NUMBER_SHIFT;
	int packetCount = header->packetID & SPLIT_PACKET_COUNT_MASK;
	if ( packetNumber >= MAX_SPLIT_PACKETS || packetCount > MAX_SPLIT_PACKETS )
	{
		Con_Printf( "Malformed packet number (%i)\n", packetNumber );
		return false;
	}
	if ( gNetSplit.currentSequence == -1 || sequence != gNetSplit.currentSequence )
	{
		gNetSplit.currentSequence = sequence;
		gNetSplit.splitCount = packetCount;
		if ( net_showpackets.value == SHOW_SPLIT_PACKETS )
			Con_Printf( "<-- Split packet restart %i count %i seq\n", packetCount, sequence );
	}
	unsigned int payloadSize = static_cast<unsigned int>( length ) - sizeof( SPLITPACKET );
	if ( gNetSplitFlags[packetNumber] == sequence )
		Con_Printf( "NET_GetLong:  Ignoring duplicated split packet %i of %i ( %i bytes )\n", packetNumber + 1,
			packetCount, payloadSize );
	else
	{
		if ( packetNumber == static_cast<unsigned int>( packetCount - 1 ) )
			gNetSplit.totalSize =
				static_cast<int>( payloadSize + SPLIT_PAYLOAD_BYTES * packetCount - SPLIT_PAYLOAD_BYTES );
		--gNetSplit.splitCount;
		gNetSplitFlags[packetNumber] = sequence;
		if ( net_showpackets.value == SHOW_SPLIT_PACKETS )
			Con_Printf(
				"<-- Split packet %i of %i, %i bytes %i seq\n", packetNumber + 1, packetCount, payloadSize, sequence );
		unsigned int offset = SPLIT_PAYLOAD_BYTES * packetNumber;
		if ( offset + payloadSize > sizeof( gNetSplit.buffer ) )
		{
			Con_Printf( "Malformed packet size (%i, %i)\n", offset, payloadSize );
			return false;
		}
		std::memcpy( gNetSplit.buffer + offset, data + sizeof( SPLITPACKET ), payloadSize );
	}
	if ( gNetSplit.splitCount > 0 )
		return false;
	for ( int packet = 0; packet < packetCount; ++packet )
	{
		if ( gNetSplitFlags[packet] != gNetSplit.currentSequence )
		{
			Con_Printf( "Split packet without all %i parts, part %i had wrong sequence %i/%i\n", packetCount,
				packet + 1, gNetSplitFlags[packet], gNetSplit.currentSequence );
			return false;
		}
	}
	gNetSplit.currentSequence = -1;
	if ( static_cast<unsigned int>( gNetSplit.totalSize ) > sizeof( gNetSplit.buffer ) )
	{
		Con_Printf( "Split packet too large! %d bytes\n", gNetSplit.totalSize );
		return false;
	}
	std::memcpy( data, gNetSplit.buffer, gNetSplit.totalSize );
	*outputLength = gNetSplit.totalSize;
	return true;
}

qboolean NET_QueuePacket( netsrc_t source )
{
#if !defined( _WIN32 )
	SOCKET socket = ip_sockets[source];
	if ( socket )
	{
		sockaddr from;
		int fromLength = sizeof( from );
		char data[MAX_RECEIVE_PACKET_BYTES];
		int result = static_cast<int>(
			recvfrom( socket, data, sizeof( data ), 0, &from, reinterpret_cast<socklen_t*>( &fromLength ) ) );
		if ( result <= 0 )
			result = NET_SteamFakeIPRecvFrom( source, data, sizeof( data ), 0, result, &from, &fromLength );
		if ( result == -1 )
		{
			int error = errno;
			if ( error != ECONNRESET && error != EAGAIN && error != ECONNREFUSED )
			{
				if ( error == EMSGSIZE )
					Con_DPrintf( "NET_QueuePacket:  Ignoring oversized network message\n" );
				else
				{
					const char* description = strerror( error );
					if ( cls.state != ca_dedicated )
						Sys_Error( "NET_QueuePacket: %s", description );
					Con_Printf( "NET_QueuePacket: %s\n", description );
				}
			}
		}
		else
		{
			SockadrToNetadr( &from, &in_from );
			if ( result == static_cast<int>( sizeof( data ) ) )
				Con_Printf( "NET_QueuePacket:  Oversize packet from %s\n", NET_AdrToString( in_from ) );
			else
			{
				Q_memcpy( in_message.data, data, result );
				in_message.cursize = result;
				if ( reinterpret_cast<SPLITPACKET*>( in_message.data )->netID != SPLIT_PACKET_ID )
					return NET_LagPacket( true, source, &in_from, &in_message );
				if ( static_cast<unsigned int>( result ) >= sizeof( SPLITPACKET ) )
					return NET_GetLong( reinterpret_cast<char*>( in_message.data ), result, &in_message.cursize );
				Con_Printf( "Invalid split packet length %i\n", result );
				return false;
			}
		}
	}
	return NET_LagPacket( false, source, nullptr, nullptr );
#else
	SOCKET socket = 0;
	for ( int attempt = 0; attempt < LOOPBACK_CHANNELS; ++attempt )
	{
		if ( !attempt )
			socket = ip_sockets[source];
		if ( !socket )
			continue;
		sockaddr from;
		int fromLength = sizeof( from );
		char data[MAX_RECEIVE_PACKET_BYTES];
		int result = recvfrom( socket, data, sizeof( data ), 0, &from, &fromLength );
		if ( result <= 0 )
			result = NET_SteamFakeIPRecvFrom( source, data, sizeof( data ), 0, result, &from, &fromLength );
		if ( result == SOCKET_ERROR )
		{
			int error = WSAGetLastError();
			if ( error != WSAENETRESET && error != WSAEWOULDBLOCK && error != WSAECONNRESET
				 && error != WSAECONNREFUSED )
			{
				if ( error == WSAEMSGSIZE )
					Con_DPrintf( "NET_QueuePacket:  Ignoring oversized network message\n" );
				else
				{
					const char* description = NET_ErrorString( error );
					if ( cls.state != ca_dedicated )
						Sys_Error( "NET_QueuePacket: %s", description );
					Con_Printf( "NET_QueuePacket: %s\n", description );
				}
			}
			continue;
		}
		SockadrToNetadr( &from, &in_from );
		if ( result == sizeof( data ) )
		{
			Con_Printf( "NET_QueuePacket:  Oversize packet from %s\n", NET_AdrToString( in_from ) );
			continue;
		}
		Q_memcpy( in_message.data, data, result );
		in_message.cursize = result;
		if ( reinterpret_cast<SPLITPACKET*>( in_message.data )->netID != SPLIT_PACKET_ID )
			return NET_LagPacket( true, source, &in_from, &in_message );
		if ( static_cast<unsigned int>( result ) >= sizeof( SPLITPACKET ) )
			return NET_GetLong( reinterpret_cast<char*>( in_message.data ), result, &in_message.cursize );
		Con_Printf( "Invalid split packet length %i\n", result );
		return false;
	}
	return NET_LagPacket( false, source, nullptr, nullptr );
#endif
}

qboolean NET_GetPacket( netsrc_t source )
{
	NET_AdjustLag();
	int threaded = use_thread;
#if !defined( _WIN32 )
#else
	if ( threaded && net_thread_initialized )
	{
		EnterCriticalSection( &net_thread_cs );
		threaded = use_thread;
	}
#endif
	loopback_t* loop = &loopbacks[source];
	if ( static_cast<int>( static_cast<unsigned int>( loop->send ) - loop->get ) > MAX_LOOPBACK )
		loop->get = static_cast<int>( static_cast<unsigned int>( loop->send ) - MAX_LOOPBACK );
	qboolean received;
	if ( loop->get < loop->send )
	{
		loopmsg_t* message = &loop->msgs[loop->get & ( MAX_LOOPBACK - 1 )];
		loop->get = static_cast<int>( static_cast<unsigned int>( loop->get ) + 1 );
		Q_memcpy( in_message.data, message->data, message->datalen );
		in_message.cursize = message->datalen;
		Q_memset( &in_from, 0, sizeof( in_from ) );
		in_from.type = NA_LOOPBACK;
		received = NET_LagPacket( true, source, &in_from, &in_message );
	}
	else
	{
		received = threaded ? false : NET_QueuePacket( source );
		if ( !received )
			received = NET_LagPacket( false, source, nullptr, nullptr );
	}
	// Got a packet from fakelag system
	if ( received )
	{
		Q_memcpy( net_message.data, in_message.data, in_message.cursize );
		net_message.cursize = in_message.cursize;
		net_from = in_from;
	}
	else
	{
		net_messages_t* message = messages[source];
		if ( message )
		{
			messages[source] = message->next;
			// Copy in data
			net_message.cursize = message->buffersize;
			Q_memcpy( net_message.data, message->buffer, message->buffersize );
			net_from = message->from;
			msg_readcount = 0;
			if ( message->preallocated )
			{
				message->next = normalqueue;
				normalqueue = message;
			}
			else
			{
				Mem_Free( message->buffer );
				Mem_Free( message );
			}
			received = true;
		}
	}
#if !defined( _WIN32 )
#else
	if ( use_thread && net_thread_initialized )
		LeaveCriticalSection( &net_thread_cs );
#endif
	return received;
}

constexpr int NET_GRAPH_COLOR_COUNT = 24;
constexpr int NET_GRAPH_LOW_COLORS = 8;
constexpr int NET_GRAPH_HIGH_COLORS = NET_GRAPH_COLOR_COUNT - NET_GRAPH_LOW_COLORS;
constexpr int RGB_COMPONENTS = 3;
constexpr int PREALLOCATED_PACKETS = 40;
constexpr int PREALLOCATED_PACKET_BYTES = 1536;

byte colors[NET_GRAPH_COLOR_COUNT][RGB_COMPONENTS];
qboolean noip;
int net_configured;
int net_sleepforever = 1;
#if defined( _WIN32 )
HANDLE hNetThread;
DWORD dwNetThreadId;
static std::atomic<bool> net_thread_stop_requested{ false };
#endif

cvar_t net_address = { const_cast<char*>( "net_address" ), const_cast<char*>( "" ), 0, 0.0f, nullptr };
cvar_t ipname = { const_cast<char*>( "ip" ), const_cast<char*>( "localhost" ), 0, 0.0f, nullptr };
cvar_t iphostport = { const_cast<char*>( "ip_hostport" ), const_cast<char*>( "0" ), 0, 0.0f, nullptr };
cvar_t hostport = { const_cast<char*>( "hostport" ), const_cast<char*>( "0" ), 0, 0.0f, nullptr };
cvar_t defport = { const_cast<char*>( "port" ), const_cast<char*>( "27015" ), 0, 0.0f, nullptr };
cvar_t ip_clientport = { const_cast<char*>( "ip_clientport" ), const_cast<char*>( "0" ), 0, 0.0f, nullptr };
cvar_t clientport = { const_cast<char*>( "clientport" ), const_cast<char*>( "27005" ), 0, 0.0f, nullptr };
cvar_t clockwindow = { const_cast<char*>( "clockwindow" ), const_cast<char*>( "0.5" ), 0, 0.0f, nullptr };
cvar_t multicastport = { const_cast<char*>( "multicastport" ), const_cast<char*>( "27025" ), 0, 0.0f, nullptr };
cvar_t net_graph = { const_cast<char*>( "net_graph" ), const_cast<char*>( "0" ), FCVAR_ARCHIVE, 0.0f, nullptr };
cvar_t net_graphwidth = { const_cast<char*>( "net_graphwidth" ), const_cast<char*>( "150" ), 0, 0.0f, nullptr };
cvar_t net_scale = { const_cast<char*>( "net_scale" ), const_cast<char*>( "5" ), FCVAR_ARCHIVE, 0.0f, nullptr };
cvar_t net_graphpos = { const_cast<char*>( "net_graphpos" ), const_cast<char*>( "1" ), FCVAR_ARCHIVE, 0.0f, nullptr };

static void MaxPlayers_f()
{
	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "\"maxplayers\" is \"%u\"\n", svs.maxclients );
		return;
	}
	if ( sv.active )
	{
		Con_Printf( "maxplayers cannot be changed while a server is running.\n" );
		return;
	}

	int maxplayers = Q_atoi( Cmd_Argv( 1 ) );
	if ( maxplayers < 1 )
		maxplayers = 1;
	if ( maxplayers > svs.maxclientslimit )
	{
		maxplayers = svs.maxclientslimit;
		Con_Printf( "\"maxplayers\" set to \"%u\"\n", maxplayers );
	}
	svs.maxclients = maxplayers;
	Cvar_Set( "deathmatch", maxplayers == 1 ? "0" : "1" );
}
bool NET_ShouldUseSteamFakeIP();
void NET_SteamFakeIPDestroySocket( netsrc_t source );
#if defined( _WIN32 )
DWORD WINAPI NET_ThreadEntry( void* context );

DWORD WINAPI NET_ThreadEntry( void* )
{
	constexpr long NET_THREAD_POLL_MICROSECONDS = 20000;
	constexpr DWORD NET_THREAD_SLEEP_MILLISECONDS = 1;
	while ( !net_thread_stop_requested.load() )
	{
		fd_set readSockets;
		readSockets.fd_count = 0;
		int maxSocket = 0;
		for ( int source = 0; source < NET_SOCKET_SOURCES; ++source )
		{
			SOCKET socket = ip_sockets[source];
			if ( socket )
			{
				readSockets.fd_array[readSockets.fd_count++] = socket;
				if ( static_cast<int>( socket ) > maxSocket )
					maxSocket = static_cast<int>( socket );
			}
		}
		timeval timeout = { 0, NET_THREAD_POLL_MICROSECONDS };
		int ready = select( maxSocket + 1, &readSockets, nullptr, nullptr, &timeout );
		while ( ready > 0 && !net_thread_stop_requested.load() )
		{
			qboolean idle = true;
			for ( int source = 0; source < NET_SOCKET_SOURCES; ++source )
			{
				if ( net_thread_stop_requested.load() )
					break;
				if ( use_thread && net_thread_initialized )
					EnterCriticalSection( &net_thread_cs );
				if ( NET_QueuePacket( static_cast<netsrc_t>( source ) ) )
				{
					idle = false;
					net_messages_t* message;
					if ( in_message.cursize <= PREALLOCATED_PACKET_BYTES && normalqueue )
					{
						message = normalqueue;
						normalqueue = message->next;
					}
					else
					{
						message = static_cast<net_messages_t*>( Mem_ZeroMalloc( sizeof( net_messages_t ) ) );
						message->buffer = static_cast<byte*>( Mem_ZeroMalloc( in_message.cursize ) );
						message->preallocated = false;
					}
					message->buffersize = in_message.cursize;
					Q_memcpy( message->buffer, in_message.data, in_message.cursize );
					message->from = in_from;
					net_messages_t** tail = &messages[source];
					while ( *tail )
						tail = &( *tail )->next;
					*tail = message;
					message->next = nullptr;
				}
				if ( use_thread && net_thread_initialized )
					LeaveCriticalSection( &net_thread_cs );
			}
			if ( idle )
				break;
		}
		Sleep( NET_THREAD_SLEEP_MILLISECONDS );
	}
	return 0;
}
#endif

void NET_InitColors()
{
	const byte mincolor[RGB_COMPONENTS] = { 63, 0, 100 };
	const byte maxcolor[RGB_COMPONENTS] = { 255, 127, 0 };
	const vec3_t lowDelta = { -63.0f, 63.0f, 155.0f };
	const vec3_t highDelta = { -5.0f, -127.0f, 0.0f };
	for ( int i = 0; i < NET_GRAPH_COLOR_COUNT; ++i )
	{
		for ( int j = 0; j < RGB_COMPONENTS; ++j )
		{
			float value;
			if ( i < NET_GRAPH_LOW_COLORS )
				value = static_cast<float>( i ) / NET_GRAPH_LOW_COLORS * lowDelta[j] + mincolor[j];
			else
				value =
					static_cast<float>( i - NET_GRAPH_LOW_COLORS ) / NET_GRAPH_HIGH_COLORS * highDelta[j] + maxcolor[j];
			colors[i][j] = static_cast<byte>( static_cast<int>( value ) );
		}
	}
}

void NET_StartThread()
{
#if !defined( _WIN32 )
	if ( use_thread && !net_thread_initialized )
		net_thread_initialized = true;
#else
	if ( use_thread && !net_thread_initialized )
	{
		net_thread_stop_requested.store( false );
		net_thread_initialized = true;
		InitializeCriticalSection( &net_thread_cs );
		hNetThread = CreateThread( nullptr, 0, NET_ThreadEntry, nullptr, 0, &dwNetThreadId );
		if ( !hNetThread )
		{
			DeleteCriticalSection( &net_thread_cs );
			net_thread_initialized = false;
			use_thread = 0;
			Sys_Error( "Couldn't initialize network thread, run without -net_thread\n" );
		}
	}
#endif
}

void NET_StopThread()
{
#if !defined( _WIN32 )
	if ( use_thread && net_thread_initialized )
		net_thread_initialized = false;
#else
	if ( hNetThread )
	{
		net_thread_stop_requested.store( true );
		if ( WaitForSingleObject( hNetThread, INFINITE ) != WAIT_OBJECT_0 )
			Sys_Error( "Couldn't wait for network thread shutdown\n" );
		CloseHandle( hNetThread );
		hNetThread = nullptr;
		dwNetThreadId = 0;
		DeleteCriticalSection( &net_thread_cs );
		net_thread_initialized = false;
	}
#endif
}

void NET_Init()
{
	NET_InitColors();
	Cmd_AddCommand( "maxplayers", MaxPlayers_f );
	Cvar_RegisterVariable( &net_address );
	Cvar_RegisterVariable( &ipname );
	Cvar_RegisterVariable( &iphostport );
	Cvar_RegisterVariable( &hostport );
	Cvar_RegisterVariable( &defport );
	Cvar_RegisterVariable( &ip_clientport );
	Cvar_RegisterVariable( &clientport );
	Cvar_RegisterVariable( &clockwindow );
	Cvar_RegisterVariable( &multicastport );
	Cvar_RegisterVariable( &fakelag );
	Cvar_RegisterVariable( &fakeloss );
	Cvar_RegisterVariable( &net_graph );
	Cvar_RegisterVariable( &net_graphwidth );
	Cvar_RegisterVariable( &net_scale );
	Cvar_RegisterVariable( &net_graphpos );
#if !defined( _WIN32 )
#else
	if ( COM_CheckParm( "-netthread" ) )
		use_thread = 1;
#endif
	if ( COM_CheckParm( "-netsleep" ) )
		net_sleepforever = 0;
	if ( COM_CheckParm( "-noip" ) )
		noip = true;
	int parameter = COM_CheckParm( "-port" );
	if ( parameter )
		Cvar_SetValue( const_cast<char*>( "hostport" ), static_cast<float>( std::atof( com_argv[parameter + 1] ) ) );
	parameter = COM_CheckParm( "-clockwindow" );
	if ( parameter )
		Cvar_SetValue( const_cast<char*>( "clockwindow" ), static_cast<float>( std::atof( com_argv[parameter + 1] ) ) );
	//
	// init the message buffer
	//
	net_message.maxsize = sizeof( net_message_buffer );
	net_message.data = net_message_buffer;
	net_message.flags = 0;
	net_message.buffername = "net_message";
	in_message.maxsize = sizeof( in_message_buf );
	in_message.data = in_message_buf;
	in_message.flags = 0;
	in_message.buffername = "in_message";
	for ( int source = 0; source < NET_SOCKET_SOURCES; ++source )
		g_pLagData[source].pNext = g_pLagData[source].pPrev = &g_pLagData[source]; // List of lag structures, if fakelag is set.
	for ( int i = 0; i < PREALLOCATED_PACKETS; ++i )
	{
		net_messages_t* message = static_cast<net_messages_t*>( Mem_ZeroMalloc( sizeof( net_messages_t ) ) );
		message->buffer = static_cast<byte*>( Mem_ZeroMalloc( PREALLOCATED_PACKET_BYTES ) );
		message->preallocated = true;
		message->next = normalqueue;
		normalqueue = message;
	}
#if !defined( _WIN32 )
	if ( use_thread && !net_thread_initialized )
		net_thread_initialized = true;
#else
	NET_StartThread();
#endif
	Con_DPrintf( "Base networking initialized.\n" );
}

void NET_Config( qboolean multiplayer )
{
	static qboolean old_config;
	static qboolean bFirst = true;
	if ( old_config == multiplayer )
		return;
	old_config = multiplayer;
	if ( multiplayer )
	{	// open sockets
		if ( !noip )
			NET_OpenIP();
		if ( bFirst && !NET_ShouldUseSteamFakeIP() )
		{
			bFirst = false;
			NET_GetLocalAddress();
		}
	}
	else
	{
#if !defined( _WIN32 )
		for ( int source = 0; source < NET_SOCKET_SOURCES; ++source )
		{
			if ( ip_sockets[source] )
			{
				close( ip_sockets[source] );
				ip_sockets[source] = 0;
			}
			NET_SteamFakeIPDestroySocket( static_cast<netsrc_t>( source ) );
		}
#else
		if ( use_thread && net_thread_initialized )
			EnterCriticalSection( &net_thread_cs );
		for ( int source = 0; source < NET_SOCKET_SOURCES; ++source )
		{
			if ( ip_sockets[source] )
			{
				closesocket( ip_sockets[source] );
				ip_sockets[source] = 0;
			}
			NET_SteamFakeIPDestroySocket( static_cast<netsrc_t>( source ) );
		}
		if ( use_thread && net_thread_initialized )
			LeaveCriticalSection( &net_thread_cs );
#endif
	}
	net_configured = multiplayer != false;
}

void NET_ClearLagData( qboolean client, qboolean server )
{
#if !defined( _WIN32 )
#else
	if ( use_thread && net_thread_initialized )
		EnterCriticalSection( &net_thread_cs );
#endif
	if ( client )
	{
		NET_ClearLaggedList( &g_pLagData[NS_CLIENT] );
		NET_ClearLaggedList( &g_pLagData[NS_MULTICAST] );
	}
	if ( server )
		NET_ClearLaggedList( &g_pLagData[NS_SERVER] );
#if !defined( _WIN32 )
#else
	if ( use_thread && net_thread_initialized )
		LeaveCriticalSection( &net_thread_cs );
#endif
}

void NET_Shutdown()
{
	NET_StopThread();
	NET_ClearLaggedList( &g_pLagData[NS_CLIENT] );
	NET_ClearLaggedList( &g_pLagData[NS_SERVER] );
	NET_Config( false );
	for ( int source = 0; source < NET_SOCKET_SOURCES; ++source )
	{
		net_messages_t* message = messages[source];
		while ( message )
		{
			net_messages_t* next = message->next;
			Mem_Free( message->buffer );
			Mem_Free( message );
			message = next;
		}
		messages[source] = nullptr;
	}
	net_messages_t* message = normalqueue;
	while ( message )
	{
		net_messages_t* next = message->next;
		Mem_Free( message->buffer );
		Mem_Free( message );
		message = next;
	}
	normalqueue = nullptr;
}

constexpr int SOCKET_ADDRESS_TEXT_BYTES = 128;
constexpr int LOCAL_HOSTNAME_BYTES = 512;
constexpr int PORT_ANY = -1;
constexpr int FAKE_IP_PORTS = 1;

netadr_t net_local_adr;
extern std::uint32_t g_nFakeIP;
extern unsigned short g_arFakePorts[FAKE_IP_PORTS];

void Net_CheckOpenFakeUDPPorts( qboolean client );
void Net_PollSteamFakeIPResult();

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4996 )
#endif
qboolean NET_StringToSockaddr( char* source, sockaddr* destination )
{
	Q_memset( destination, 0, sizeof( *destination ) );
	sockaddr_in* address = reinterpret_cast<sockaddr_in*>( destination );
	address->sin_family = AF_INET;
	char name[SOCKET_ADDRESS_TEXT_BYTES];
	Q_strncpy( name, source, sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = 0;
	for ( char* position = name; *position; ++position )
	{
		if ( *position == ':' )
		{
			*position = 0;
			address->sin_port = htons( static_cast<unsigned short>( std::atoi( position + 1 ) ) );
		}
	}
	address->sin_addr.s_addr = inet_addr( name );
	if ( address->sin_addr.s_addr == INADDR_NONE )
	{
		hostent* host = gethostbyname( name );
		if ( !host )
			return false;
		Q_memcpy( &address->sin_addr.s_addr, host->h_addr_list[0], sizeof( address->sin_addr.s_addr ) );
	}
	return true;
}

#if defined( _MSC_VER )
#pragma warning( pop )
#endif

SOCKET NET_IPSocket( char* networkInterface, int port, qboolean multicast )
{
#if !defined( _WIN32 )
	int nonblocking = 1;
	int option = 1;
	SOCKET socket = ::socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( socket == -1 )
	{
		int error = errno;
		if ( error != EAFNOSUPPORT )
			Con_Printf( "WARNING: UDP_OpenSocket: port: %d socket: %s", port, strerror( error ) );
		return 0;
	}
	if ( ioctl( socket, FIONBIO, &nonblocking ) == -1 )
	{
		Con_Printf( "WARNING: UDP_OpenSocket: port: %d  ioctl FIONBIO: %s\n", port, strerror( errno ) );
		return 0;
	}
	if ( setsockopt( socket, SOL_SOCKET, SO_BROADCAST, &option, sizeof( option ) ) == -1 )
	{
		Con_Printf( "WARNING: UDP_OpenSocket: port: %d  setsockopt SO_BROADCAST: %s\n", port, strerror( errno ) );
		return 0;
	}
	if ( ( COM_CheckParm( "-reuse" ) || multicast )
		 && setsockopt( socket, SOL_SOCKET, SO_REUSEADDR, &nonblocking, sizeof( nonblocking ) ) == -1 )
	{
		Con_Printf( "WARNING: UDP_OpenSocket: port: %d  setsockopt SO_REUSEADDR: %s\n", port, strerror( errno ) );
		return 0;
	}
	if ( COM_CheckParm( "-tos" ) )
	{
		int tos = 16;
		Con_Printf( "Enabling LOWDELAY TOS option\n" );
		if ( setsockopt( socket, IPPROTO_IP, IP_TOS, &tos, sizeof( tos ) ) == -1 && errno != ENOPROTOOPT )
		{
			Con_Printf( "WARNING: UDP_OpenSocket: port: %d  setsockopt IP_TOS: %s\n", port, strerror( errno ) );
			return 0;
		}
	}
	sockaddr_in address;
	if ( networkInterface && *networkInterface && Q_strcasecmp( networkInterface, "localhost" ) != 0 )
		NET_StringToSockaddr( networkInterface, reinterpret_cast<sockaddr*>( &address ) );
	else
		address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = port == PORT_ANY ? 0 : htons( static_cast<unsigned short>( port ) );
	address.sin_family = AF_INET;
	if ( bind( socket, reinterpret_cast<sockaddr*>( &address ), sizeof( address ) ) == -1 )
	{
		Con_Printf( "WARNING: UDP_OpenSocket: port: %d  bind: %s\n", port, strerror( errno ) );
		close( socket );
		return 0;
	}
	option = COM_CheckParm( "-loopback" ) != 0;
	if ( setsockopt( socket, IPPROTO_IP, IP_MULTICAST_LOOP, &option, sizeof( option ) ) == -1 )
		Con_DPrintf( "WARNING: UDP_OpenSocket: port %d setsockopt IP_MULTICAST_LOOP: %s\n", port, strerror( errno ) );
	return socket;
#else
	u_long nonblocking = 1;
	int option = 1;
	SOCKET socket = ::socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( socket == INVALID_SOCKET )
	{
		int error = WSAGetLastError();
		if ( error != WSAEAFNOSUPPORT )
			Con_Printf( "WARNING: UDP_OpenSocket: port: %d socket: %s", port, NET_ErrorString( error ) );
		return 0;
	}
	if ( ioctlsocket( socket, FIONBIO, &nonblocking ) == SOCKET_ERROR )
	{
		Con_Printf(
			"WARNING: UDP_OpenSocket: port: %d  ioctl FIONBIO: %s\n", port, NET_ErrorString( WSAGetLastError() ) );
		return 0;
	}
	if ( setsockopt( socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>( &option ), sizeof( option ) )
		 == SOCKET_ERROR )
	{
		Con_Printf( "WARNING: UDP_OpenSocket: port: %d  setsockopt SO_BROADCAST: %s\n", port,
			NET_ErrorString( WSAGetLastError() ) );
		return 0;
	}
	if ( ( COM_CheckParm( "-reuse" ) || multicast )
		 && setsockopt(
				socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>( &nonblocking ), sizeof( nonblocking ) )
				== SOCKET_ERROR )
	{
		Con_Printf( "WARNING: UDP_OpenSocket: port: %d  setsockopt SO_REUSEADDR: %s\n", port,
			NET_ErrorString( WSAGetLastError() ) );
		return 0;
	}
	sockaddr_in address;
	if ( networkInterface && *networkInterface && Q_strcasecmp( networkInterface, "localhost" ) != 0 )
		NET_StringToSockaddr( networkInterface, reinterpret_cast<sockaddr*>( &address ) );
	else
		address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = port == PORT_ANY ? 0 : htons( static_cast<unsigned short>( port ) );
	address.sin_family = AF_INET;
	if ( bind( socket, reinterpret_cast<sockaddr*>( &address ), sizeof( address ) ) == SOCKET_ERROR )
	{
		Con_Printf( "WARNING: UDP_OpenSocket: port: %d  bind: %s\n", port, NET_ErrorString( WSAGetLastError() ) );
		closesocket( socket );
		return 0;
	}
	option = COM_CheckParm( "-loopback" ) != 0;
	if ( setsockopt( socket, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>( &option ), sizeof( option ) )
		 == SOCKET_ERROR )
		Con_DPrintf( "WARNING: UDP_OpenSocket: port %d setsockopt IP_MULTICAST_LOOP: %s\n", port,
			NET_ErrorString( WSAGetLastError() ) );
	return socket;
#endif
}

void NET_OpenIP()
{
	int serverPort = 0;
	int clientPort = 0;
	qboolean dedicated = cls.state == ca_dedicated;
#if !defined( _WIN32 )
#else
	if ( use_thread && net_thread_initialized )
		EnterCriticalSection( &net_thread_cs );
#endif
	if ( !ip_sockets[NS_SERVER] )
	{
		serverPort = static_cast<int>( iphostport.value );
		if ( !serverPort )
		{
			serverPort = static_cast<int>( hostport.value );
			if ( !serverPort )
			{
				hostport.value = defport.value;
				serverPort = static_cast<int>( defport.value );
			}
		}
		ip_sockets[NS_SERVER] = NET_IPSocket( ipname.string, serverPort, false );
		if ( !ip_sockets[NS_SERVER] && dedicated )
			Sys_Error( "Couldn't allocate dedicated server IP port %d.", serverPort );
	}
#if !defined( _WIN32 )
#else
	if ( use_thread && net_thread_initialized )
		LeaveCriticalSection( &net_thread_cs );
#endif
	// dedicated servers don't need client ports
	if ( cls.state == ca_dedicated )
		return;
#if !defined( _WIN32 )
#else
	if ( use_thread && net_thread_initialized )
		EnterCriticalSection( &net_thread_cs );
#endif
	if ( !ip_sockets[NS_CLIENT] )
	{
		clientPort = static_cast<int>( ip_clientport.value );
		if ( !clientPort )
		{
			clientPort = static_cast<int>( clientport.value );
			if ( !clientPort )
				clientPort = PORT_ANY;
		}
		ip_sockets[NS_CLIENT] = NET_IPSocket( ipname.string, clientPort, false );
		if ( !ip_sockets[NS_CLIENT] )
			ip_sockets[NS_CLIENT] = NET_IPSocket( ipname.string, PORT_ANY, false );
	}
	if ( !ip_sockets[NS_MULTICAST] )
	{
		ip_sockets[NS_MULTICAST] = NET_IPSocket( ipname.string, static_cast<int>( multicastport.value ), true );
		if ( !ip_sockets[NS_MULTICAST] && !dedicated )
			Con_Printf( "Warning! Couldn't allocate multicast IP port.\n" );
	}
	Net_CheckOpenFakeUDPPorts( true );
	Net_CheckOpenFakeUDPPorts( false );
#if !defined( _WIN32 )
#else
	if ( use_thread && net_thread_initialized )
		LeaveCriticalSection( &net_thread_cs );
#endif
	static qboolean bFirst = true;
	if ( bFirst )
	{
		bFirst = false;
		Con_Printf( "NET Ports:  server %i, client %i\n", serverPort, clientPort );
	}
}

void NET_GetLocalAddress()
{
	if ( noip )
	{
		Con_Printf( "TCP/IP Disabled.\n" );
		Q_memset( &net_local_adr, 0, sizeof( net_local_adr ) );
		return;
	}
	if ( NET_ShouldUseSteamFakeIP() )
	{
		if ( !g_nFakeIP )
		{
			Net_PollSteamFakeIPResult();
			if ( !g_nFakeIP )
			{
				Q_memset( &net_local_adr, 0, sizeof( net_local_adr ) );
				return;
			}
		}
		if ( net_local_adr.type == NA_IP && NET_NetAdr_GetIPHostByteOrder( &net_local_adr ) == g_nFakeIP
			 && NET_NetAdr_GetPort( &net_local_adr ) == g_arFakePorts[0] )
			return;
		net_local_adr.type = NA_IP;
		NET_NetAdr_SetIP( &net_local_adr, g_nFakeIP );
		NET_NetAdr_SetPort( &net_local_adr, g_arFakePorts[0] );
		Con_Printf( "Using FakeIP\n" );
		Con_Printf( "Server IP address %s\n", NET_AdrToString( net_local_adr ) );
		Cvar_Set( const_cast<char*>( "net_address" ), va( "%s", NET_AdrToString( net_local_adr ) ) );
		Steam_UpdateRichPresence();
		return;
	}
	char name[LOCAL_HOSTNAME_BYTES];
	if ( Q_strcmp( ipname.string, "localhost" ) != 0 )
		Q_strncpy( name, ipname.string, sizeof( name ) - 1 );
	else
		gethostname( name, sizeof( name ) );
	name[sizeof( name ) - 1] = 0;
	Q_memset( &net_local_adr, 0, sizeof( net_local_adr ) );
	if ( Q_strcmp( name, "localhost" ) != 0 && Q_strcmp( name, "127.0.0.1" ) != 0 )
	{
		sockaddr address;
		if ( NET_StringToSockaddr( name, &address ) && address.sa_family == AF_INET )
			SockadrToNetadr( &address, &net_local_adr );
	}
	else
	{
		Q_memset( &net_local_adr, 0, sizeof( net_local_adr ) );
		net_local_adr.type = NA_LOOPBACK;
	}
	sockaddr_in address;
#if !defined( _WIN32 )
	socklen_t addressLength = sizeof( address );
	if ( getsockname( ip_sockets[NS_SERVER], reinterpret_cast<sockaddr*>( &address ), &addressLength ) != 0 )
	{
		noip = true;
		Con_Printf( "Could not get TCP/IP address, TCP/IP disabled\nReason:  %s\n", strerror( errno ) );
		return;
	}
#else
	int addressLength = sizeof( address );
	if ( getsockname( ip_sockets[NS_SERVER], reinterpret_cast<sockaddr*>( &address ), &addressLength ) != 0 )
	{
		int error = WSAGetLastError();
		noip = true;
		Con_Printf( "Could not get TCP/IP address, TCP/IP disabled\nReason:  %s\n", NET_ErrorString( error ) );
		return;
	}
#endif
	net_local_adr.port = address.sin_port;
	Con_Printf( "Server IP address %s\n", NET_AdrToString( net_local_adr ) );
	Cvar_Set( const_cast<char*>( "net_address" ), va( "%s", NET_AdrToString( net_local_adr ) ) );
}

qboolean NET_StringToAdr( char* text, netadr_t* address )
{
	if ( Q_strcmp( text, "localhost" ) == 0 || Q_strcmp( text, "127.0.0.1" ) == 0 )
	{
		Q_memset( address, 0, sizeof( *address ) );
		address->type = NA_LOOPBACK;
		return true;
	}

	sockaddr socketAddress;
	if ( !NET_StringToSockaddr( text, &socketAddress ) )
		return false;
	SockadrToNetadr( &socketAddress, address );
	return true;
}

char* NET_AdrToString( netadr_t address )
{
	constexpr int NETWORK_ADDRESS_TEXT_BYTES = 64;
	static char s[NETWORK_ADDRESS_TEXT_BYTES];
	Q_memset( s, 0, sizeof( s ) );
	if ( address.type == NA_LOOPBACK )
		_snprintf( s, sizeof( s ), "loopback" );
	else if ( address.type == NA_IP )
		_snprintf( s, sizeof( s ), "%i.%i.%i.%i:%i", address.ip[0], address.ip[1], address.ip[2], address.ip[3],
			ntohs( address.port ) );
	return s;
}

void NET_FlushSocket( netsrc_t source )
{
	constexpr int FLUSH_PACKET_BYTES = 6010;
	SOCKET socket = ip_sockets[source];
	if ( !socket )
		return;
	char buffer[FLUSH_PACKET_BYTES];
	sockaddr from;
	int fromLength = sizeof( from );
	while ( recvfrom( socket, buffer, sizeof( buffer ), 0, &from, &fromLength ) > 0 )
	{
	}
	while ( NET_SteamFakeIPRecvFrom( source, buffer, sizeof( buffer ), 0, 0, &from, &fromLength ) > 0 )
	{
	}
}

/*
====================
NET_IsConfigured

Is winsock ip initialized?
====================
*/
int NET_IsConfigured()
{
	return net_configured;
}

qboolean NET_IsLocalAddress( netadr_t address )
{
	constexpr byte LOOPBACK_NETWORK = 127;
	return address.type == NA_LOOPBACK
		   || ( address.type == NA_IP && address.ip[0] == LOOPBACK_NETWORK && address.ip[1] == 0 && address.ip[2] == 0
				&& address.ip[3] == 1 );
}

qboolean NET_IsReservedAdr( netadr_t address )
{
	constexpr byte PRIVATE_CLASS_A_NETWORK = 10;
	constexpr byte LOOPBACK_NETWORK = 127;
	constexpr byte PRIVATE_CLASS_B_NETWORK = 172;
	constexpr byte PRIVATE_CLASS_B_FIRST_SUBNET = 16;
	constexpr byte PRIVATE_CLASS_B_LAST_SUBNET = 31;
	constexpr byte PRIVATE_CLASS_C_NETWORK = 192;
	constexpr byte PRIVATE_CLASS_C_FIRST_SUBNET = 168;
	if ( address.type == NA_LOOPBACK )
		return true;
	if ( address.type != NA_IP )
		return false;
	if ( address.ip[0] == PRIVATE_CLASS_A_NETWORK || address.ip[0] == LOOPBACK_NETWORK )
		return true;
	if ( address.ip[0] == PRIVATE_CLASS_B_NETWORK )
		return address.ip[1] >= PRIVATE_CLASS_B_FIRST_SUBNET && address.ip[1] <= PRIVATE_CLASS_B_LAST_SUBNET;
	return address.ip[0] == PRIVATE_CLASS_C_NETWORK && address.ip[1] >= PRIVATE_CLASS_C_FIRST_SUBNET;
}

qboolean NET_JoinGroup( netsrc_t source, netadr_t address )
{
	ip_mreq request;
	std::memcpy( &request.imr_multiaddr, address.ip, sizeof( address.ip ) );
	request.imr_interface.s_addr = INADDR_ANY;
#if !defined( _WIN32 )
	if ( setsockopt( ip_sockets[source], IPPROTO_IP, IP_ADD_MEMBERSHIP, &request, sizeof( request ) ) != -1 )
		return true;
	int error = errno;
	if ( error != EAFNOSUPPORT )
		Con_Printf( "WARNING: NET_JoinGroup: IP_ADD_MEMBERSHIP: %s", strerror( error ) );
	return false;
#else
	if ( setsockopt( ip_sockets[source], IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>( &request ),
			 sizeof( request ) )
		 != SOCKET_ERROR )
		return true;
	int error = WSAGetLastError();
	if ( error != WSAEAFNOSUPPORT )
		Con_Printf( "WARNING: NET_JoinGroup: IP_ADD_MEMBERSHIP: %s", NET_ErrorString( error ) );
	return false;
#endif
}

qboolean NET_LeaveGroup( netsrc_t source, netadr_t address )
{
	ip_mreq request;
	std::memcpy( &request.imr_multiaddr, address.ip, sizeof( address.ip ) );
	request.imr_interface.s_addr = INADDR_ANY;
#if !defined( _WIN32 )
	return setsockopt( ip_sockets[source], IPPROTO_IP, IP_DROP_MEMBERSHIP, &request, sizeof( request ) ) != -1
		   || errno == EAFNOSUPPORT;
#else
	return setsockopt( ip_sockets[source], IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char*>( &request ),
			   sizeof( request ) )
			   != SOCKET_ERROR
		   || WSAGetLastError() == WSAEAFNOSUPPORT;
#endif
}

qboolean NET_CompareBaseAdr( netadr_t first, netadr_t second )
{
	if ( first.type != second.type )
		return false;
	if ( first.type == NA_LOOPBACK )
		return true;
	return first.type == NA_IP && std::memcmp( first.ip, second.ip, sizeof( first.ip ) ) == 0;
}

qboolean NET_CompareClassBAdr( netadr_t first, netadr_t second )
{
	if ( first.type != second.type )
		return false;
	if ( first.type == NA_LOOPBACK )
		return true;
	return first.type == NA_IP && first.ip[0] == second.ip[0] && first.ip[1] == second.ip[1];
}

char* NET_BaseAdrToString( netadr_t address )
{
	constexpr int NETWORK_ADDRESS_TEXT_BYTES = 64;
	static char s[NETWORK_ADDRESS_TEXT_BYTES];
	Q_memset( s, 0, sizeof( s ) );
	if ( address.type == NA_LOOPBACK )
		_snprintf( s, sizeof( s ), "loopback" );
	else if ( address.type == NA_IP )
		_snprintf( s, sizeof( s ), "%i.%i.%i.%i", address.ip[0], address.ip[1], address.ip[2], address.ip[3] );
	return s;
}

typedef struct
{
	unsigned short client;
	unsigned short players;
	unsigned short entities;
	unsigned short tentities;
	unsigned short sound;
	unsigned short event;
	unsigned short usr;
	unsigned short msgbytes;
	unsigned short voicebytes;
} netbandwidthgraph_t;

typedef struct
{
	int latency;
	int choked;
} packet_latency_t;

typedef struct
{
	float cmd_lerp;
	int size;
	qboolean sent;
} cmdinfo_t;

constexpr int NET_GRAPH_SAMPLES = 1024;
constexpr int NET_GRAPH_MASK = NET_GRAPH_SAMPLES - 1;
constexpr int NET_GRAPH_INVALID = 9998;
constexpr int NET_GRAPH_LOST = 9999;
constexpr int NET_GRAPH_SKIPPED = 9997;
constexpr int NET_GRAPH_MAX_VALID = 9995;
constexpr int NET_GRAPH_TEXT_FONT = 1;
constexpr float NET_GRAPH_TEXT_RED = 0.9f;
constexpr float NET_GRAPH_TEXT_GREEN = 0.9f;
constexpr float NET_GRAPH_TEXT_BLUE = 0.7f;
constexpr int NET_GRAPH_LINE_HEIGHT = 15;
constexpr int NET_GRAPH_TEXT_BYTES = 256;
constexpr float NET_GRAPH_LATENCY_ROWS = 24.0f;
constexpr float NET_GRAPH_LATENCY_MARGIN = 2.0f;
constexpr double NET_GRAPH_LATENCY_BIAS = 0.1;
constexpr double NET_GRAPH_LATENCY_RANGE = 1.1;
constexpr double NET_GRAPH_MILLISECONDS = 1000.0;
constexpr double NET_GRAPH_SMOOTHING = 0.5;

static packet_latency_t packet_latency[NET_GRAPH_SAMPLES];
static cmdinfo_t cmdinfo[NET_GRAPH_SAMPLES];
static netbandwidthgraph_t graph[NET_GRAPH_SAMPLES];
static float framerate;
static float packet_loss;
static float packet_choke;
static double rolling_fps;
static const byte netcolors[][4] = {
	{ 255, 0, 0, 255 }, { 0, 0, 255, 255 }, { 240, 127, 63, 255 }, { 255, 255, 0, 255 }, { 63, 255, 63, 150 } };
static byte holdcolor[] = { 255, 0, 0 };
static byte extrap_base_color[] = { 255, 255, 255 };

static void NET_DrawString( int x, int y, int, float red, float green, float blue, char* format, ... )
{
	constexpr int NET_GRAPH_FORMAT_BYTES = 1024;
	static char string[NET_GRAPH_FORMAT_BYTES];
	va_list args;
	va_start( args, format );
	_vsnprintf( string, sizeof( string ), format, args );
	va_end( args );
	Draw_SetTextColor( red, green, blue );
	Draw_String( x, y, string );
}

[[maybe_unused]] static void NET_DrawTextFields( int graphvalue, int x, int y, netbandwidthgraph_t* bandwidth,
	cmdinfo_t* commands, int count, float avg, float* frameTime, int loss, int choke )
{
	constexpr int NET_GRAPH_PING_TEXT_OFFSET = 75;
	constexpr double NET_GRAPH_PERCENT_ROUNDING = 0.49;
	static int lastout;
	float ping = 0.0f;
	if ( count > 0 )
	{
		ping = static_cast<float>( avg / static_cast<float>( count ) - host_frametime * NET_GRAPH_SMOOTHING );
		if ( cl_updaterate.value > 0.0f )
		{
			int interval = static_cast<int>( static_cast<float>( NET_GRAPH_MILLISECONDS / cl_updaterate.value ) );
			ping -= static_cast<float>( interval );
		}
		if ( ping < 0.0f )
			ping = 0.0f;
	}
	*frameTime = static_cast<float>( *frameTime * NET_GRAPH_SMOOTHING + host_frametime * NET_GRAPH_SMOOTHING );
	y = static_cast<int>( static_cast<float>( y ) - scr_graphheight.value );
	if ( !( *frameTime > 0.0f ) )
		return;
	char text[NET_GRAPH_TEXT_BYTES];
	_snprintf( text, sizeof( text ), "%.1f fps", 1.0 / *frameTime );
	NET_DrawString( x, y, NET_GRAPH_TEXT_FONT, NET_GRAPH_TEXT_RED, NET_GRAPH_TEXT_GREEN, NET_GRAPH_TEXT_BLUE, text );
	if ( ping > 1.0f )
	{
		_snprintf( text, sizeof( text ), "%i ms ", static_cast<int>( ping ) );
		NET_DrawString( x + NET_GRAPH_PING_TEXT_OFFSET, y, NET_GRAPH_TEXT_FONT, NET_GRAPH_TEXT_RED,
			NET_GRAPH_TEXT_GREEN, NET_GRAPH_TEXT_BLUE, text );
	}
	y += NET_GRAPH_LINE_HEIGHT;
	int outgoing = commands[( cls.netchan.outgoing_sequence - 1 ) & NET_GRAPH_MASK].size;
	if ( outgoing )
		lastout = outgoing;
	else
		outgoing = lastout;
	_snprintf( text, sizeof( text ), "in :  %i %.2f k/s",
		bandwidth[cls.netchan.incoming_sequence & NET_GRAPH_MASK].msgbytes, cls.netchan.flow[1].avgkbytespersec );
	NET_DrawString( x, y, NET_GRAPH_TEXT_FONT, NET_GRAPH_TEXT_RED, NET_GRAPH_TEXT_GREEN, NET_GRAPH_TEXT_BLUE, text );
	y += NET_GRAPH_LINE_HEIGHT;
	_snprintf( text, sizeof( text ), "out:  %i %.2f k/s", outgoing, cls.netchan.flow[0].avgkbytespersec );
	NET_DrawString( x, y, NET_GRAPH_TEXT_FONT, NET_GRAPH_TEXT_RED, NET_GRAPH_TEXT_GREEN, NET_GRAPH_TEXT_BLUE, text );
	if ( graphvalue > 2 )
	{
		_snprintf( text, sizeof( text ), "loss: %i choke:  %i",
			static_cast<int>( loss + NET_GRAPH_PERCENT_ROUNDING ),
			static_cast<int>( choke + NET_GRAPH_PERCENT_ROUNDING ) );
		NET_DrawString( x, y + NET_GRAPH_LINE_HEIGHT, NET_GRAPH_TEXT_FONT, NET_GRAPH_TEXT_RED, NET_GRAPH_TEXT_GREEN,
			NET_GRAPH_TEXT_BLUE, text );
	}
}

void SCR_DrawFPS()
{
	constexpr double FPS_CURRENT_FRAME_WEIGHT = 0.4;
	constexpr double FPS_PREVIOUS_FRAME_WEIGHT = 0.6;
	constexpr int FPS_TEXT_ORIGIN = 2;
	if ( cl_showfps.value != 0.0f && host_frametime > 0.0 )
	{
		rolling_fps = host_frametime * FPS_CURRENT_FRAME_WEIGHT + rolling_fps * FPS_PREVIOUS_FRAME_WEIGHT;
		int fps = static_cast<int>( std::floor( 1.0 / rolling_fps ) );
		NET_DrawString( FPS_TEXT_ORIGIN, FPS_TEXT_ORIGIN, 0, 1.0f, 1.0f, 1.0f, const_cast<char*>( "%d fps" ), fps );
	}
}

[[maybe_unused]] static void NET_DrawTimes( vrect_t viewport, cmdinfo_t* commands, int x, int width )
{
	constexpr int COMMAND_RATE_TEXT_RIGHT_MARGIN = 26;
	constexpr int COMMAND_RATE_TEXT_BOTTOM_MARGIN = 27;
	constexpr int COMMAND_GRAPH_BOTTOM_MARGIN = 4;
	constexpr int COMMAND_GRAPH_BASE_MARGIN = 12;
	constexpr int COMMAND_GRAPH_HOLD_MARGIN = 3;
	constexpr int COMMAND_GRAPH_EDGE_PIXELS = 4;
	constexpr int COMMAND_INTERPOLATION_ROWS = 24;
	constexpr int COMMAND_INTERPOLATION_SPLIT = 8;
	constexpr double COMMAND_INTERPOLATION_MAX = 3.0;
	constexpr int COMMAND_GRAPH_ALPHA = 255;
	constexpr int COMMAND_HOLD_ALPHA = 200;
	int textX = x + width - COMMAND_RATE_TEXT_RIGHT_MARGIN;
	if ( textX < 1 )
		textX = 1;
	int textY = viewport.y + viewport.height - COMMAND_RATE_TEXT_BOTTOM_MARGIN;
	if ( textY < 1 )
		textY = 1;
	NET_DrawString( textX, textY, NET_GRAPH_TEXT_FONT, NET_GRAPH_TEXT_RED, NET_GRAPH_TEXT_GREEN, NET_GRAPH_TEXT_BLUE,
		const_cast<char*>( "%i/s" ), static_cast<int>( cl_cmdrate.value ) );
	int bottom = viewport.y + viewport.height;
	for ( int i = 0; i < width; ++i )
	{
		cmdinfo_t* command = &commands[( cls.netchan.outgoing_sequence - i ) & NET_GRAPH_MASK];
		double lerp = command->cmd_lerp / COMMAND_INTERPOLATION_MAX * COMMAND_INTERPOLATION_ROWS;
		if ( !( lerp < COMMAND_INTERPOLATION_ROWS ) )
			lerp = COMMAND_INTERPOLATION_ROWS;
		int height = static_cast<int>( lerp );
		int column = x + width - 1 - i;
		if ( height < COMMAND_INTERPOLATION_SPLIT )
			Draw_FillRGBA( column, bottom - COMMAND_GRAPH_BOTTOM_MARGIN - height, 1, 1, colors[height][0],
				colors[height][1], colors[height][2], COMMAND_GRAPH_ALPHA );
		else
		{
			int color = height - 1;
			Draw_FillRGBA( column, bottom - COMMAND_GRAPH_BASE_MARGIN - ( height - COMMAND_INTERPOLATION_SPLIT - 1 ), 1,
				1, colors[color][0], colors[color][1], colors[color][2], COMMAND_GRAPH_ALPHA );
		}
		if ( i < COMMAND_GRAPH_EDGE_PIXELS || i >= width - COMMAND_GRAPH_EDGE_PIXELS )
			Draw_FillRGBA( column, bottom - COMMAND_GRAPH_BASE_MARGIN, 1, 1, extrap_base_color[0], extrap_base_color[1],
				extrap_base_color[2], COMMAND_GRAPH_ALPHA );
		if ( !command->sent )
			Draw_FillRGBA( column, bottom - COMMAND_GRAPH_HOLD_MARGIN, 1, 1, holdcolor[0], holdcolor[1], holdcolor[2],
				COMMAND_HOLD_ALPHA );
	}
}

qboolean NET_CompareAdr( netadr_t first, netadr_t second )
{
	if ( first.type != second.type )
		return false;
	if ( first.type == NA_LOOPBACK )
		return true;
	if ( first.type == NA_IP )
		return std::memcmp( first.ip, second.ip, sizeof( first.ip ) ) == 0 && first.port == second.port;
	return false;
}

static void NET_GetFrameData( packet_latency_t* latency, netbandwidthgraph_t* bandwidth, int* chokeCount,
	int* lossCount, int* biggestMessage, float* average, int* pingCount )
{
	constexpr double FRAME_NOT_RECEIVED = -1.0;
	constexpr double FRAME_SKIPPED = -3.0;
	constexpr int RECENT_PING_SAMPLES = 8;
	constexpr int MAX_GRAPH_MESSAGE_BYTES = 1000;
	*chokeCount = 0;
	*lossCount = 0;
	*biggestMessage = 0;
	*average = 0.0f;
	*pingCount = 0;
	for ( int sequence = cls.netchan.incoming_sequence - CL_UPDATE_BACKUP + 1;
		sequence <= cls.netchan.incoming_sequence; ++sequence )
	{
		frame_t* frame = &cl.frames[sequence & CL_UPDATE_MASK];
		packet_latency_t* sample = &latency[sequence & NET_GRAPH_MASK];
		sample->choked = frame->choked;
		if ( frame->choked )
			++*chokeCount;
		if ( frame->invalid )
			sample->latency = NET_GRAPH_INVALID;
		else if ( frame->receivedtime == FRAME_NOT_RECEIVED )
		{
			sample->latency = NET_GRAPH_LOST;
			++*lossCount;
		}
		else if ( frame->receivedtime == FRAME_SKIPPED )
			sample->latency = NET_GRAPH_SKIPPED;
		else
		{
			float seconds = static_cast<float>( std::fmin( 1.0, frame->latency ) );
			float height = ( scr_graphheight.value - NET_GRAPH_LATENCY_ROWS ) - NET_GRAPH_LATENCY_MARGIN;
			sample->latency =
				static_cast<int>( ( seconds + NET_GRAPH_LATENCY_BIAS ) / NET_GRAPH_LATENCY_RANGE * height );
			if ( sequence > cls.netchan.incoming_sequence - RECENT_PING_SAMPLES )
			{
				++*pingCount;
				*average = static_cast<float>( frame->latency * NET_GRAPH_MILLISECONDS + *average );
			}
		}
		netbandwidthgraph_t* bytes = &bandwidth[sequence & NET_GRAPH_MASK];
		bytes->client = frame->clientbytes;
		bytes->players = frame->playerinfobytes;
		bytes->entities = frame->packetentitybytes;
		bytes->tentities = frame->tentitybytes;
		bytes->sound = frame->soundbytes;
		bytes->event = frame->eventbytes;
		bytes->usr = frame->usrbytes;
		bytes->msgbytes = frame->msgbytes;
		bytes->voicebytes = frame->voicebytes;
		if ( frame->msgbytes > *biggestMessage )
			*biggestMessage = frame->msgbytes;
	}
	if ( *biggestMessage > MAX_GRAPH_MESSAGE_BYTES )
		*biggestMessage = MAX_GRAPH_MESSAGE_BYTES;
}

static qboolean NET_DrawDataSegment( vrect_t* rectangle, int bytes, byte red, byte green, byte blue, byte alpha )
{
	rectangle->height = 1;
	rectangle->y -= static_cast<int>( static_cast<float>( bytes ) / net_scale.value );
	if ( rectangle->y <= 1 )
		return false;
	Draw_FillRGBA( rectangle->x, rectangle->y, rectangle->width, rectangle->height, red, green, blue, alpha );
	return true;
}

static void NET_DrawHatches( int x, int y, int maxmsgbytes )
{
	constexpr double HATCH_BYTE_STEP = 10.0;
	constexpr int HATCH_MAJOR_BYTES = 50;
	constexpr int HATCH_MINOR_SPACING = 5;
	constexpr int HATCH_WIDTH = 3;
	int step = static_cast<int>( HATCH_BYTE_STEP / net_scale.value );
	if ( step < 1 )
		step = 1;
	int top = y;
	float limit = static_cast<float>( maxmsgbytes + HATCH_MAJOR_BYTES );
	while ( y > 0 )
	{
		float bytes = static_cast<float>( top - y ) * net_scale.value;
		if ( limit <= bytes )
			break;
		if ( static_cast<int>( bytes ) % HATCH_MAJOR_BYTES == 0 )
			Draw_FillRGBA( x, y, HATCH_WIDTH, 1, 0, 200, 0, 255 );
		else if ( step > HATCH_MINOR_SPACING )
			Draw_FillRGBA( x, y, HATCH_WIDTH, 1, 63, 63, 0, 200 );
		y -= step;
	}
}

void SCR_NetGraph()
{
	constexpr int GRAPH_SCREEN_MARGIN = 5;
	constexpr int GRAPH_SCREEN_PADDING = GRAPH_SCREEN_MARGIN * 2;
	constexpr int GRAPH_BOTTOM_MARGIN = 29;
	constexpr int GRAPH_EDGE_PIXELS = 4;
	constexpr int GRAPH_RATE_TEXT_MARGIN = 25;
	constexpr double GRAPH_PERCENT_SCALE = 100.0;
	int graphvalue = static_cast<int>( net_graph.value );
	kbutton_t* key = ClientDLL_FindKey( "in_graph" );
	if ( !graphvalue )
	{
		if ( !key || !( key->state & BUTTON_DOWN ) )
			return;
		graphvalue = 2;
	}
	if ( net_scale.value <= 0.0f )
		net_scale.value = 1.0f;
	vrect_t viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = vid.width;
	viewport.height = vid.height;
	int width = static_cast<int>( net_graphwidth.value );
	if ( width > NET_GRAPH_SAMPLES )
		width = NET_GRAPH_SAMPLES;
	if ( viewport.width < width + GRAPH_SCREEN_PADDING )
		width = viewport.width - GRAPH_SCREEN_PADDING;
	int chokeCount = 0;
	int lossCount = 0;
	int biggestMessage = 0;
	int pingCount = 0;
	float average = 0.0f;
	NET_GetFrameData( packet_latency, graph, &chokeCount, &lossCount, &biggestMessage, &average, &pingCount );
	float loss = static_cast<float>( lossCount * GRAPH_PERCENT_SCALE / static_cast<float>( CL_UPDATE_BACKUP ) );
	float choke = static_cast<float>( chokeCount * GRAPH_PERCENT_SCALE / static_cast<float>( CL_UPDATE_BACKUP ) );
	packet_loss = static_cast<float>( loss * NET_GRAPH_SMOOTHING + packet_loss * NET_GRAPH_SMOOTHING );
	packet_choke = static_cast<float>( choke * NET_GRAPH_SMOOTHING + packet_choke * NET_GRAPH_SMOOTHING );
	for ( int sequence = cls.netchan.outgoing_sequence - CL_UPDATE_BACKUP + 1;
		sequence <= cls.netchan.outgoing_sequence; ++sequence )
	{
		cmd_t* command = &cl.commands[sequence & CL_UPDATE_MASK];
		cmdinfo_t* sample = &cmdinfo[sequence & NET_GRAPH_MASK];
		sample->cmd_lerp = command->frame_lerp;
		sample->sent = !command->heldback;
		sample->size = command->sendsize;
	}
	int x = GRAPH_SCREEN_MARGIN;
	if ( static_cast<int>( net_graphpos.value ) == 1 )
		x = viewport.width - width - GRAPH_SCREEN_MARGIN;
	else if ( static_cast<int>( net_graphpos.value ) == 2 )
		x = ( viewport.width - width - GRAPH_SCREEN_PADDING ) / 2;
	int bottom = viewport.height - GRAPH_BOTTOM_MARGIN;
	if ( graphvalue < 3 )
	{
		vrect_t fill;
		int lastvalidh = 0;
		for ( int i = 0; i < width; ++i )
		{
			int index = ( cls.netchan.incoming_sequence - i ) & NET_GRAPH_MASK;
			packet_latency_t* sample = &packet_latency[index];
			const byte* color;
			qboolean valid = false;
			switch ( sample->latency )
			{
			case NET_GRAPH_SKIPPED:
				color = netcolors[2];
				break;
			case NET_GRAPH_INVALID:
				color = netcolors[1];
				break;
			case NET_GRAPH_LOST:
				color = netcolors[0];
				break;
			default:
				color = netcolors[sample->choked ? 3 : 4];
				lastvalidh = sample->latency;
				valid = true;
				break;
			}
			int height = lastvalidh;
			float maximumHeight = ( scr_graphheight.value - NET_GRAPH_LATENCY_ROWS ) - NET_GRAPH_LATENCY_MARGIN;
			if ( static_cast<float>( height ) > maximumHeight )
				height = static_cast<int>( maximumHeight );
			fill.x = x + width - 1 - i;
			fill.y = bottom - height;
			fill.width = 1;
			fill.height = valid ? 1 : height;
			if ( !valid && height >= GRAPH_EDGE_PIXELS )
			{
				Draw_FillRGBA( fill.x, fill.y, 1, 2, color[0], color[1], color[2], color[3] );
				fill.y = bottom - 2;
				fill.height = 2;
			}
			Draw_FillRGBA( fill.x, fill.y, fill.width, fill.height, color[0], color[1], color[2], color[3] );
			qboolean edge = i < GRAPH_EDGE_PIXELS || i >= width - GRAPH_EDGE_PIXELS;
			if ( edge )
				Draw_FillRGBA( fill.x, bottom, 1, 1, 0, 255, 0, 160 );
			if ( graphvalue >= 2 )
			{
				fill.y = static_cast<int>( ( static_cast<float>( bottom ) - scr_graphheight.value ) - 1.0f );
				fill.height = 1;
				if ( edge )
					Draw_FillRGBA( fill.x, fill.y, 1, 1, 255, 255, 255, 255 );
				if ( sample->latency <= NET_GRAPH_MAX_VALID )
				{
					--fill.y;
					netbandwidthgraph_t* bytes = &graph[index];
					if ( NET_DrawDataSegment( &fill, bytes->client, 255, 0, 0, 128 )
						 && NET_DrawDataSegment( &fill, bytes->players, 255, 255, 0, 128 )
						 && NET_DrawDataSegment( &fill, bytes->entities, 255, 0, 255, 128 )
						 && NET_DrawDataSegment( &fill, bytes->tentities, 0, 0, 255, 128 )
						 && NET_DrawDataSegment( &fill, bytes->sound, 0, 255, 0, 128 )
						 && NET_DrawDataSegment( &fill, bytes->event, 0, 255, 255, 128 )
						 && NET_DrawDataSegment( &fill, bytes->usr, 200, 200, 200, 128 )
						 && NET_DrawDataSegment( &fill, bytes->voicebytes, 255, 255, 255, 255 ) )
					{
						fill.y = static_cast<int>( ( ( static_cast<float>( bottom ) - scr_graphheight.value ) - 1.0f )
												   - static_cast<float>( static_cast<int>(
													   static_cast<float>( bytes->msgbytes ) / net_scale.value ) ) );
						if ( fill.y >= 2 )
							Draw_FillRGBA( fill.x, fill.y, 1, 1, 240, 240, 240, 128 );
					}
				}
			}
		}
		if ( graphvalue >= 2 )
			NET_DrawHatches( x, static_cast<int>( ( static_cast<float>( bottom ) - scr_graphheight.value ) - 1.0f ),
				biggestMessage );
		int textX = width + x - GRAPH_RATE_TEXT_MARGIN;
		if ( textX < 1 )
			textX = 1;
		int textY = static_cast<int>( std::fmax( 1.0f, static_cast<float>( bottom ) - scr_graphheight.value ) );
		NET_DrawString( textX, textY, NET_GRAPH_TEXT_FONT, NET_GRAPH_TEXT_RED, NET_GRAPH_TEXT_GREEN,
			NET_GRAPH_TEXT_BLUE, const_cast<char*>( "%i/s" ), static_cast<int>( cl_updaterate.value ) );
		NET_DrawTimes( viewport, cmdinfo, x, width );
	}
	NET_DrawTextFields( graphvalue, x, bottom, graph, cmdinfo, pingCount, average, &framerate,
		static_cast<int>( packet_loss ), static_cast<int>( packet_choke ) );
}

void NET_ThreadLock()
{
}

void NET_ThreadUnlock()
{
}

short Q_ntohs( short value )
{
	unsigned short bits = static_cast<unsigned short>( value );
	return static_cast<short>( ( bits >> 8 ) | ( bits << 8 ) );
}

//=============================================================================

void NetadrToSockadr( netadr_t* a, sockaddr* s )
{
	Q_memset( s, 0, sizeof( sockaddr ) );
	if ( a->type == NA_BROADCAST )
	{
		sockaddr_in* address = reinterpret_cast<sockaddr_in*>( s );
		address->sin_family = AF_INET;
		address->sin_port = a->port;
		address->sin_addr.s_addr = INADDR_BROADCAST;
	}
	else if ( a->type == NA_IP )
	{
		sockaddr_in* address = reinterpret_cast<sockaddr_in*>( s );
		address->sin_family = AF_INET;
		Q_memcpy( &address->sin_addr.s_addr, a->ip, sizeof( a->ip ) );
		address->sin_port = a->port;
	}
}

unsigned short NET_HostToNetShort( unsigned short value )
{
	return static_cast<unsigned short>( ( value >> 8 ) | ( value << 8 ) );
}

/*
=============================================================================

LOOPBACK BUFFERS FOR LOCAL PLAYER

=============================================================================
*/
void NET_TransferRawData( sizebuf_t* msg, unsigned char* start, int size )
{
	Q_memcpy( msg->data, start, size );
	msg->cursize = size;
}

qboolean NET_GetLoopPacket( netsrc_t sock, netadr_t* from, sizebuf_t* msg )
{
	int send = loopbacks[sock].send;
	int get = loopbacks[sock].get;
	if ( send - get > MAX_LOOPBACK )
	{
		get = send - MAX_LOOPBACK;
		loopbacks[sock].get = get;
	}
	if ( send > get )
	{
		loopbacks[sock].get = get + 1;
		loopmsg_t* message = &loopbacks[sock].msgs[get & ( MAX_LOOPBACK - 1 )];
		int count = message->datalen;
		Q_memcpy( msg->data, message->data, count );
		msg->cursize = count;
		Q_memset( from, 0, sizeof( netadr_t ) );
		from->type = NA_LOOPBACK;
		return true;
	}
	return false;
}

void NET_SendLoopPacket( netsrc_t sock, int length, void* data, netadr_t )
{
	int channel = static_cast<int>( sock ) ^ 1;
	int index = loopbacks[channel].send++;
	loopmsg_t* message = &loopbacks[channel].msgs[index & ( MAX_LOOPBACK - 1 )];
	Q_memcpy( message->data, data, length );
	message->datalen = length;
}

//=============================================================================

/*
==================
NET_RemoveFromPacketList(packetlag_t *pPacket)

Unlinks it from the current list.
==================
*/
void NET_RemoveFromPacketList( packetlag_t* packet )
{
	packet->pPrev->pNext = packet->pNext;
	packet->pNext->pPrev = packet->pPrev;
	packet->pPrev = nullptr;
	packet->pNext = nullptr;
}

int NET_CountLaggedList( packetlag_t* list )
{
	int count = 0;
	for ( packetlag_t* packet = list->pNext; packet != nullptr && list != packet; packet = packet->pNext )
		++count;
	return count;
}

/*
===================
NET_AddToLagged

===================
*/
void NET_AddToLagged(
	netsrc_t, packetlag_t* list, packetlag_t* packet, netadr_t* from, sizebuf_t message, float timestamp )
{
	if ( packet->pPrev != nullptr || packet->pNext != nullptr )
	{
		Con_Printf( "Packet already linked\n" );
		return;
	}
	packet->pPrev = list->pPrev;
	list->pPrev->pNext = packet;
	list->pPrev = packet;
	packet->pNext = list;
	byte* data = static_cast<byte*>( Mem_Malloc( message.cursize ) );
	packet->pPacketData = data;
	Q_memcpy( data, message.data, message.cursize );
	packet->nSize = message.cursize;
	packet->receivedTime = timestamp; // Our time stamp.
	packet->net_from = *from;
}

extern cvar_t sys_ticrate;

int NET_Sleep_Timeout()
{
	static int lasttime;
	static int numFrames;
	static int staggerFrames;
	int value = static_cast<int>( sys_ticrate.value );
	int now = static_cast<int>( Sys_FloatTime() );
	if ( lasttime != 0 )
	{
		if ( now - lasttime > 1 )
		{
			lasttime = now;
			numFrames = value;
			staggerFrames = value / 100 + 1;
		}
	}
	else
	{
		lasttime = now;
	}
	fd_set readSockets;
	readSockets.fd_count = 0;
	int maxSocket = 0;
	for ( int source = 0; source < NET_SOCKET_SOURCES; ++source )
	{
		SOCKET socket = ip_sockets[source];
		if ( socket )
		{
			readSockets.fd_array[readSockets.fd_count++] = socket;
			if ( static_cast<int>( socket ) > maxSocket )
				maxSocket = static_cast<int>( socket );
		}
	}
	timeval timeout;
	timeout.tv_sec = 0;
	int usec = 1000 * ( 1000 / value );
	if ( usec <= 0 )
		usec = 1;
	timeout.tv_usec = usec;
	int result;
	if ( numFrames > 0 && numFrames % staggerFrames != 0 )
		result = select( maxSocket + 1, &readSockets, nullptr, nullptr, &timeout );
	else
		result = select( 0, nullptr, nullptr, nullptr, &timeout );
	--numFrames;
	return result;
}

int NET_Sleep()
{
	fd_set readSockets;
	readSockets.fd_count = 0;
	int maxSocket = 0;
	for ( int source = 0; source < NET_SOCKET_SOURCES; ++source )
	{
		SOCKET socket = ip_sockets[source];
		if ( socket )
		{
			readSockets.fd_array[readSockets.fd_count++] = socket;
			if ( static_cast<int>( socket ) > maxSocket )
				maxSocket = static_cast<int>( socket );
		}
	}
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 20000;
	timeval* wait = &timeout;
	if ( net_sleepforever != 0 )
		wait = nullptr;
	return select( maxSocket + 1, &readSockets, nullptr, nullptr, wait );
}

void* net_malloc( std::size_t size )
{
	return Mem_ZeroMalloc( size );
}

net_messages_t* NET_AllocMsg( int size )
{
	if ( size <= PREALLOCATED_PACKET_BYTES && normalqueue != nullptr )
	{
		net_messages_t* message = normalqueue;
		net_messages_t* next = message->next;
		message->buffersize = size;
		normalqueue = next;
		return message;
	}
	net_messages_t* message = static_cast<net_messages_t*>( Mem_ZeroMalloc( sizeof( net_messages_t ) ) );
	byte* buffer = static_cast<byte*>( Mem_ZeroMalloc( size ) );
	message->buffersize = size;
	message->preallocated = false;
	message->buffer = buffer;
	return message;
}

void NET_FreeMsg( net_messages_t* message )
{
	if ( message->preallocated != false )
	{
		message->next = normalqueue;
		normalqueue = message;
		return;
	}
	Mem_Free( message->buffer );
	Mem_Free( message );
}

void NET_AllocateQueues()
{
	int remaining = PREALLOCATED_PACKETS;
	do
	{
		net_messages_t* message = static_cast<net_messages_t*>( Mem_ZeroMalloc( sizeof( net_messages_t ) ) );
		byte* buffer = static_cast<byte*>( Mem_ZeroMalloc( PREALLOCATED_PACKET_BYTES ) );
		--remaining;
		message->preallocated = true;
		message->buffer = buffer;
		message->next = normalqueue;
		normalqueue = message;
	} while ( remaining != 0 );
	NET_StartThread();
}

void NET_FlushQueues()
{
	NET_StopThread();
	for ( int source = 0; source < NET_SOCKET_SOURCES; ++source )
	{
		net_messages_t* message = messages[source];
		while ( message != nullptr )
		{
			net_messages_t* next = message->next;
			Mem_Free( message->buffer );
			Mem_Free( message );
			message = next;
		}
		messages[source] = nullptr;
	}
	net_messages_t* message = normalqueue;
	while ( message != nullptr )
	{
		net_messages_t* next = message->next;
		Mem_Free( message->buffer );
		Mem_Free( message );
		message = next;
	}
	normalqueue = nullptr;
}

int NET_SendToImpl(
	netsrc_t sock, SOCKET socket, char* packet, int length, int flags, const sockaddr* to, int addressLength )
{
	if ( NET_IsSteamFakeIP( sock, to ) != false )
		return NET_SteamFakeIPSendTo( sock, socket, packet, length, flags, to, addressLength );
	return sendto( socket, packet, length, flags, to, addressLength );
}

void NET_FillRect( vrect_t* rect, byte* color, byte alpha )
{
	Draw_FillRGBA( rect->x, rect->y, rect->width, rect->height, color[0], color[1], color[2], alpha );
}

void NET_GetColorValues( int color, byte* cv, byte* alpha )
{
	cv[0] = netcolors[color][0];
	cv[1] = netcolors[color][1];
	cv[2] = netcolors[color][2];
	*alpha = netcolors[color][3];
}

void NET_ColorForHeight( packet_latency_t* packet, byte* color, int* ping, byte* alpha )
{
	int latency = packet->latency;
	*ping = 0;
	const byte* source;
	switch ( latency )
	{
	case NET_GRAPH_INVALID:
		source = netcolors[1];
		break;
	case NET_GRAPH_LOST:
		source = netcolors[0];
		break;
	case NET_GRAPH_SKIPPED:
		source = netcolors[2];
		break;
	default:
		*ping = 1;
		source = netcolors[packet->choked != 0 ? 3 : 4];
		color[0] = source[0];
		color[1] = source[1];
		color[2] = source[2];
		*alpha = source[3];
		return;
	}
	color[0] = source[0];
	color[1] = source[1];
	color[2] = source[2];
	*alpha = 255;
}

qboolean NET_AtEdge( int x, int width )
{
	if ( x > 3 )
		return x >= width - 4;
	return true;
}

void NET_GetCommandInfo( cmdinfo_t* commands )
{
	for ( int sequence = cls.netchan.outgoing_sequence - CL_UPDATE_BACKUP + 1;
		sequence <= cls.netchan.outgoing_sequence; ++sequence )
	{
		cmd_t* command = &cl.commands[sequence & CL_UPDATE_MASK];
		cmdinfo_t* sample = &commands[sequence & NET_GRAPH_MASK];
		sample->cmd_lerp = command->frame_lerp;
		sample->sent = command->heldback == 0;
		sample->size = command->sendsize;
	}
}

int NET_GraphValue()
{
	int value = static_cast<int>( net_graph.value );
	kbutton_t* key = ClientDLL_FindKey( "in_graph" );
	if ( value == 0 && key != nullptr )
	{
		int down = key->state & BUTTON_DOWN;
		if ( down != 0 )
			return 2;
		return down;
	}
	return value;
}

void NET_GraphGetXY( vrect_t* rect, int width, int* x, int* y )
{
	*x = rect->x + 5;
	if ( static_cast<int>( net_graphpos.value ) == 1 )
		*x = rect->x + rect->width - 5 - width;
	else if ( static_cast<int>( net_graphpos.value ) == 2 )
		*x = rect->x + ( rect->width - 10 - width ) / 2;
	*y = rect->y + rect->height - 29;
}

void NET_DrawUpdateRate( int x, int y )
{
	NET_DrawString( x, y, NET_GRAPH_TEXT_FONT, NET_GRAPH_TEXT_RED, NET_GRAPH_TEXT_GREEN, NET_GRAPH_TEXT_BLUE,
		const_cast<char*>( "%i/s" ), static_cast<int>( cl_updaterate.value ) );
}
