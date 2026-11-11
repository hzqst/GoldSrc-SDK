//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "netapi.h"

#include <winsock.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

//-----------------------------------------------------------------------------
// Purpose: Implements INetAPI
//-----------------------------------------------------------------------------
class CNetAPI : public INetAPI
{
public:
	void NetAdrToSockAddr( netadr_t* address, struct sockaddr* socketAddress ) override;
	void SockAddrToNetAdr( struct sockaddr* socketAddress, netadr_t* address ) override;
	char* AdrToString( netadr_t* address ) override;
	bool StringToAdr( const char* string, netadr_t* address ) override;
	void GetSocketAddress( int socket, netadr_t* address ) override;
	bool CompareAdr( netadr_t* address, netadr_t* other ) override;
	void GetLocalIP( netadr_t* address ) override;
};

static CNetAPI g_NetAPI;
INetAPI* net = &g_NetAPI;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *a - 
//			*s - 
//-----------------------------------------------------------------------------
void CNetAPI::NetAdrToSockAddr( netadr_t* address, struct sockaddr* socketAddress )
{
	memset( socketAddress, 0, sizeof( *socketAddress ) );

	if ( address->type == NA_BROADCAST )
	{
		( (struct sockaddr_in*)socketAddress )->sin_family = AF_INET;
		( (struct sockaddr_in*)socketAddress )->sin_port = address->port;
		( (struct sockaddr_in*)socketAddress )->sin_addr.s_addr = INADDR_BROADCAST;
	}
	else if ( address->type == NA_IP )
	{
		( (struct sockaddr_in*)socketAddress )->sin_family = AF_INET;
		memcpy( &( (struct sockaddr_in*)socketAddress )->sin_addr.s_addr, address->ip, sizeof( address->ip ) );
		( (struct sockaddr_in*)socketAddress )->sin_port = address->port;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//			*a - 
//-----------------------------------------------------------------------------
void CNetAPI::SockAddrToNetAdr( struct sockaddr* socketAddress, netadr_t* address )
{
	if ( socketAddress->sa_family == AF_INET )
	{
		address->type = NA_IP;
		memcpy( address->ip, &( (struct sockaddr_in*)socketAddress )->sin_addr.s_addr, sizeof( address->ip ) );
		address->port = ( (struct sockaddr_in*)socketAddress )->sin_port;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *a - 
// Output : char
//-----------------------------------------------------------------------------
char* CNetAPI::AdrToString( netadr_t* address )
{
	static char string[64];

	memset( string, 0, sizeof( string ) );
	if ( address )
	{
		if ( address->type == NA_LOOPBACK )
			std::sprintf( string, "loopback" );
		else if ( address->type == NA_IP )
		{
			std::sprintf( string, "%i.%i.%i.%i:%i", address->ip[0], address->ip[1], address->ip[2], address->ip[3],
				ntohs( address->port ) );
		}
	}

	return string;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//			*sadr - 
// Output : static bool
//-----------------------------------------------------------------------------
static bool StringToSockaddr( const char* string, struct sockaddr* socketAddress )
{
	struct hostent* host;
	char* colon;
	char copy[128];
	struct sockaddr_in* address;

	memset( socketAddress, 0, sizeof( *socketAddress ) );
	address = (struct sockaddr_in*)socketAddress;
	address->sin_family = AF_INET;
	address->sin_port = 0;
	strcpy( copy, string );

	// strip off a trailing :port if present
	for ( colon = copy; *colon; colon++ )
	{
		if ( *colon == ':' )
		{
			// terminate
			*colon = 0;
			address->sin_port = htons( (short)atoi( colon + 1 ) );
		}
	}

	// Numeric IP, no DNS
	if ( copy[0] >= '0' && copy[0] <= '9' && strstr( copy, "." ) )
	{
		address->sin_addr.s_addr = inet_addr( copy );
	}
	else
	{
		host = gethostbyname( copy );
		if ( !host )
			return false;

		memcpy( &address->sin_addr.s_addr, host->h_addr_list[0], sizeof( address->sin_addr.s_addr ) );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//			*a - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNetAPI::StringToAdr( const char* string, netadr_t* address )
{
	struct sockaddr socketAddress;

	if ( !strcmp( string, "localhost" ) )
	{
		memset( address, 0, sizeof( *address ) );
		address->type = NA_LOOPBACK;
		return true;
	}

	if ( !StringToSockaddr( string, &socketAddress ) )
		return false;

	SockAddrToNetAdr( &socketAddress, address );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Lookup the IP address for the specified IP socket
// Input  : socket - 
//			*a - 
//-----------------------------------------------------------------------------
void CNetAPI::GetSocketAddress( int socket, netadr_t* address )
{
	char hostname[512];
	struct sockaddr_in socketAddress;
	int nameLength;

	memset( address, 0, sizeof( *address ) );
	gethostname( hostname, sizeof( hostname ) );
	hostname[sizeof( hostname ) - 1] = 0;
	StringToAdr( hostname, address );

	nameLength = sizeof( socketAddress );
	if ( !getsockname( socket, (struct sockaddr*)&socketAddress, &nameLength ) )
		address->port = socketAddress.sin_port;
}

//-----------------------------------------------------------------------------
// Purpose: Full IP address compare
// Input  : *a - 
//			*b - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNetAPI::CompareAdr( netadr_t* address, netadr_t* other )
{
	if ( address->type != other->type )
		return false;

	if ( address->type == NA_LOOPBACK )
		return true;

	return address->type == NA_IP
		&& address->ip[0] == other->ip[0]
		&& address->ip[1] == other->ip[1]
		&& address->ip[2] == other->ip[2]
		&& address->ip[3] == other->ip[3]
		&& address->port == other->port;
}

void CNetAPI::GetLocalIP( netadr_t* address )
{
	char hostname[64];

	if ( !gethostname( hostname, sizeof( hostname ) ) )
	{
		struct hostent* host = gethostbyname( hostname );
		if ( host )
		{
			address->type = NA_IP;
			address->port = 0;
			memcpy( address->ip, host->h_addr_list[0], sizeof( address->ip ) );
		}
	}
}
