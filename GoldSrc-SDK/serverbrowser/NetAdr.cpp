//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Implementation of the netadr_t and netmask_t classes
//
// $NoKeywords: $
//=============================================================================

#include "netadr.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

enum
{
	NA_NULL = 0,
	NA_LOOPBACK,
	NA_BROADCAST,
	NA_IP,
};

//-----------------------------------------------------------------------------
// Purpose: Returns the address as text, using one of four rotating static buffers
//-----------------------------------------------------------------------------
const char *netadr_t::ToString( bool baseOnly ) const
{
	static char s[4][64];
	static int nBuf = 0;

	snprintf( s[nBuf], 64, "unknown" );

	switch ( type )
	{
	case NA_LOOPBACK:
		snprintf( s[nBuf], 64, "loopback" );
		break;
	case NA_BROADCAST:
		snprintf( s[nBuf], 64, "broadcast" );
		break;
	case NA_IP:
		if ( baseOnly )
			snprintf( s[nBuf], 64, "%i.%i.%i.%i", ipByte[3], ipByte[2], ipByte[1], ipByte[0] );
		else
			snprintf( s[nBuf], 64, "%i.%i.%i.%i:%i", ipByte[3], ipByte[2], ipByte[1], ipByte[0], port );
		break;
	}

	const char *pszResult = s[nBuf];
	nBuf = ( nBuf + 1 ) % 4;
	return pszResult;
}

//-----------------------------------------------------------------------------
// Purpose: Fills in a sockaddr_in for this address
//-----------------------------------------------------------------------------
void netadr_t::ToSockadr( sockaddr *s ) const
{
	memset( s, 0, sizeof( sockaddr ) );

	sockaddr_in *pAddress = (sockaddr_in *)s;
	pAddress->sin_family = AF_INET;
	pAddress->sin_port = htons( port );

	switch ( type )
	{
	case NA_BROADCAST:
		pAddress->sin_addr.s_addr = INADDR_BROADCAST;
		break;
	case NA_IP:
		pAddress->sin_addr.s_addr = htonl( ip );
		break;
	case NA_LOOPBACK:
		pAddress->sin_addr.s_addr = htonl( INADDR_LOOPBACK );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the address from an AF_INET sockaddr; clears it otherwise
//-----------------------------------------------------------------------------
bool netadr_t::SetFromSockadr( const sockaddr *s )
{
	if ( s->sa_family == AF_INET )
	{
		type = NA_IP;

		unsigned int uAddress;
		memcpy( &uAddress, s->sa_data + 2, sizeof( uAddress ) );
		ip = ntohl( uAddress );

		unsigned short usPort;
		memcpy( &usPort, s->sa_data, sizeof( usPort ) );
		port = ntohs( usPort );
		return true;
	}

	ip = 0;
	port = 0;
	type = NA_NULL;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Parses "a.b.c.d[:port]", or resolves a host name when bUseDNS is set
//-----------------------------------------------------------------------------
void netadr_t::SetFromString( const char *pch, bool bUseDNS )
{
	ip = 0;
	port = 0;
	type = NA_IP;

	if ( !pch || strlen( pch ) == 0 )
		return;

	if ( pch[0] >= '0' && pch[0] <= '9' && strchr( pch, '.' ) )
	{
		int n1, n2, n3, n4, n5;
		int nRes = sscanf( pch, "%d.%d.%d.%d:%d", &n1, &n2, &n3, &n4, &n5 );
		if ( nRes > 3 )
		{
			ip = ( (unsigned int)(uint8)n1 << 24 ) | ( (unsigned int)(uint8)n2 << 16 ) | ( (unsigned int)(uint8)n3 << 8 ) | (uint8)n4;
			if ( nRes == 5 )
				port = (unsigned short)n5;
		}
	}
	else if ( bUseDNS )
	{
		char szHostName[256];
		snprintf( szHostName, 256, "%s", pch );

		char *pchColon = strchr( szHostName, ':' );
		if ( pchColon )
			*pchColon = 0;

		hostent *pHost = gethostbyname( szHostName );
		if ( pHost )
		{
			in_addr address;
			memcpy( &address, pHost->h_addr_list[0], sizeof( address ) );
			ip = ntohl( address.s_addr );

			if ( pchColon )
				port = (unsigned short)strtol( pchColon + 1, NULL, 10 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the address to the local name of the given socket
//-----------------------------------------------------------------------------
void netadr_t::SetFromSocket( int hSocket )
{
	sockaddr address;
	socklen_t nAddressLen = sizeof( address );

	ip = 0;
	port = 0;
	type = NA_IP;

	if ( getsockname( hSocket, &address, &nAddressLen ) == 0 )
		SetFromSockadr( &address );
}

//-----------------------------------------------------------------------------
// Purpose: Parses a CIDR string of the form "a.b.c.d/bits"
//-----------------------------------------------------------------------------
void netmask_t::SetFromString( const char *pchCIDR )
{
	if ( !pchCIDR )
		return;

	unsigned int n1 = 0, n2 = 0, n3 = 0, n4 = 0, nBits, nConsumed = 0;
	char szBaseIP[32];
	if ( sscanf( pchCIDR, "%16[^/]/%u%n", szBaseIP, &nBits, &nConsumed ) == 2 )
	{
		sscanf( szBaseIP, "%d.%d.%d.%d", &n1, &n2, &n3, &n4 );
		m_BaseIP = ( (unsigned int)(uint8)n1 << 24 ) | ( (unsigned int)(uint8)n2 << 16 ) | ( (unsigned int)(uint8)n3 << 8 ) | (uint8)n4;
		m_Mask = 0xFFFFFFFF << ( 32 - nBits );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parses a dotted base IP and a dotted mask
//-----------------------------------------------------------------------------
void netmask_t::SetFromString( const char *pchBaseIP, const char *pchMask )
{
	if ( !pchBaseIP || !pchMask )
		return;

	if ( pchBaseIP[0] >= '0' && pchBaseIP[0] <= '9' && strchr( pchBaseIP, '.' ) )
	{
		int n1 = 0, n2 = 0, n3 = 0, n4 = 0;
		sscanf( pchBaseIP, "%d.%d.%d.%d", &n1, &n2, &n3, &n4 );
		m_BaseIP = ( (unsigned int)(uint8)n1 << 24 ) | ( (unsigned int)(uint8)n2 << 16 ) | ( (unsigned int)(uint8)n3 << 8 ) | (uint8)n4;
	}

	if ( pchMask[0] >= '0' && pchMask[0] <= '9' && strchr( pchMask, '.' ) )
	{
		int n1 = 0, n2 = 0, n3 = 0, n4 = 0;
		sscanf( pchMask, "%d.%d.%d.%d", &n1, &n2, &n3, &n4 );
		m_Mask = ( (unsigned int)(uint8)n1 << 24 ) | ( (unsigned int)(uint8)n2 << 16 ) | ( (unsigned int)(uint8)n3 << 8 ) | (uint8)n4;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns "a.b.c.d/bits", dropping trailing zero octets of the base IP
//-----------------------------------------------------------------------------
const char *netmask_t::ToCIDRString() const
{
	static char s[4][64];
	static int nBuf = 0;

	unsigned int uInverse = ~m_Mask;
	int nBits = 32;
	while ( uInverse )
	{
		nBits--;
		uInverse >>= 1;
	}

	if ( m_BaseIPByte[0] )
		snprintf( s[nBuf], 64, "%i.%i.%i.%i/%i", m_BaseIPByte[3], m_BaseIPByte[2], m_BaseIPByte[1], m_BaseIPByte[0], nBits );
	else if ( m_BaseIPByte[1] )
		snprintf( s[nBuf], 64, "%i.%i.%i/%i", m_BaseIPByte[3], m_BaseIPByte[2], m_BaseIPByte[1], nBits );
	else if ( m_BaseIPByte[2] )
		snprintf( s[nBuf], 64, "%i.%i/%i", m_BaseIPByte[3], m_BaseIPByte[2], nBits );
	else
		snprintf( s[nBuf], 64, "%i/%i", m_BaseIPByte[3], nBits );

	const char *pszResult = s[nBuf];
	nBuf = ( nBuf + 1 ) % 4;
	return pszResult;
}
