//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Network address and netmask types
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "tier0/platform.h"

struct sockaddr;

typedef int netadrtype_t;

//-----------------------------------------------------------------------------
// Purpose: An IPv4 address and port; the IP is stored in host byte order
//-----------------------------------------------------------------------------
class netadr_t
{
public:
	netadr_t();
	netadr_t( unsigned int unIP, unsigned short usPort );
	netadr_t( const char * );
	void Clear();
	void SetType( netadrtype_t newtype );
	void SetPort( unsigned short newport );
	bool SetFromSockadr( const sockaddr *s );
	void SetIP( uint8 b1, uint8 b2, uint8 b3, uint8 b4 );
	void SetIP( unsigned int unIP );
	void SetIPAndPort( unsigned int, unsigned short );
	void SetFromString( const char *pch, bool bUseDNS );
	bool CompareAdr( const netadr_t &, bool ) const;
	bool CompareClassBAdr( const netadr_t & ) const;
	netadrtype_t GetType() const;
	unsigned int GetIP() const;
	unsigned short GetPort() const;
	const char *ToString( bool baseOnly ) const;
	void ToSockadr( sockaddr *s ) const;
	bool IsLoopback() const;
	bool IsReservedAdr() const;
	bool IsValid() const;
	void SetFromSocket( int hSocket );
	bool operator==( const netadr_t & ) const;
	bool operator<( const netadr_t &netadr ) const;

private:
	unsigned short port;
	union
	{
		unsigned int ip;
		unsigned char ipByte[4];
	};
	netadrtype_t type;
};

//-----------------------------------------------------------------------------
// Purpose: An IPv4 base address and mask pair
//-----------------------------------------------------------------------------
class netmask_t
{
public:
	netmask_t();
	netmask_t( unsigned int, unsigned int );
	netmask_t( const char * );
	netmask_t( const char *, const char * );
	void Clear();
	void SetBaseIP( uint8 b1, uint8 b2, uint8 b3, uint8 b4 );
	void SetBaseIP( unsigned int );
	void SetMask( uint8 b1, uint8 b2, uint8 b3, uint8 b4 );
	void SetMask( unsigned int unMask );
	void SetFromString( const char *pchCIDR );
	void SetFromString( const char *pchBaseIP, const char *pchMask );
	bool AdrInRange( unsigned int );
	unsigned int GetBaseIP() const;
	unsigned int GetMask() const;
	const char *ToCIDRString() const;
	const char *BaseIPToString() const;
	const char *MaskToString() const;

private:
	union
	{
		unsigned int m_BaseIP;
		unsigned char m_BaseIPByte[4];
	};
	union
	{
		unsigned int m_Mask;
		unsigned char m_MaskByte[4];
	};
};

//-----------------------------------------------------------------------------
// Purpose: Constructor, defaults to an IP address of 0.0.0.0:0
//-----------------------------------------------------------------------------
inline netadr_t::netadr_t() : port( 0 ), ip( 0 ), type( 3 )
{
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
inline netadr_t::netadr_t( unsigned int unIP, unsigned short usPort ) : port( usPort ), ip( unIP ), type( 3 )
{
}

inline void netadr_t::SetPort( unsigned short newport )
{
	port = newport;
}

inline netadrtype_t netadr_t::GetType() const
{
	return type;
}

inline unsigned int netadr_t::GetIP() const
{
	return ip;
}

inline unsigned short netadr_t::GetPort() const
{
	return port;
}

//-----------------------------------------------------------------------------
// Purpose: Orders addresses by IP, then by port; the type is ignored
//-----------------------------------------------------------------------------
inline bool netadr_t::operator<( const netadr_t &netadr ) const
{
	return ip < netadr.ip || ( ip == netadr.ip && port < netadr.port );
}
