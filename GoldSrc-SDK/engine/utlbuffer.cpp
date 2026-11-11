//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Serialization buffer
//=============================================================================

#include "utlbuffer.h"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"public\\UtlBuffer.cpp" )
#else
#define ASSERT_AT( expression, line )	assert( expression )
#endif

// error flags
#define PUT_OVERFLOW 1
#define GET_OVERFLOW 2

//-----------------------------------------------------------------------------
// Checks if a get is ok
//-----------------------------------------------------------------------------
bool CUtlBuffer::CheckGet( int size )
{
	if ( m_Error )
		return false;

	if ( m_Memory.NumAllocated() >= m_Get + size )
		return true;

	m_Error |= GET_OVERFLOW;
	return false;
}

void CUtlBuffer::PutChar( char value )
{
	if ( IsText() )
	{
		Printf( "%c", value );
	}
	else if ( CheckPut( sizeof( char ) ) )
	{
		m_Memory[m_Put] = value;
		m_Put++;
	}
}

//-----------------------------------------------------------------------------
// Checks if a put is ok
//-----------------------------------------------------------------------------

bool CUtlBuffer::CheckPut( int size )
{
	if ( m_Error )
		return false;

	while ( m_Memory.NumAllocated() < m_Put + size )
	{
		if ( m_Memory.IsExternallyAllocated() )
		{
			m_Error |= PUT_OVERFLOW;
			return false;
		}

		m_Memory.Grow( 0 );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Eats whitespace
//-----------------------------------------------------------------------------
void CUtlBuffer::EatWhiteSpace()
{
	if ( !IsText() || m_Error )
		return;

	int lastpos = m_Memory.NumAllocated();
	while ( m_Get < lastpos )
	{
		if ( !isspace( (char)m_Memory[m_Get] ) )
			break;
		m_Get++;
	}
}

int CUtlBuffer::Scanf( const char* pFmt, ... )
{
	va_list args;
	int count;

	va_start( args, pFmt );
	count = VaScanf( pFmt, args );
	va_end( args );

	return count;
}

int CUtlBuffer::VaScanf( const char* pFmt, va_list list )
{
	int numScanned = 0;
	char* pEnd;
	const char* pStart;

#if defined( _WIN32 )
	ASSERT_AT( pFmt, 173 );
#endif
	if ( m_Error || !IsText() )
		return 0;

	while ( *pFmt )
	{
		// Stop if we hit the end of the buffer
		if ( m_Get >= m_Memory.NumAllocated() )
		{
			m_Error |= GET_OVERFLOW;
			return numScanned;
		}

		if ( *pFmt == ' ' )
		{
			// eat all whitespace
			EatWhiteSpace();
			pFmt++;
			continue;
		}

		if ( *pFmt != '%' )
		{
			// Here we have to match the format string character
			// against what's in the buffer or we're done.
			if ( *pFmt != m_Memory[m_Get] )
				return numScanned;
			m_Get++;
			pFmt++;
			continue;
		}

		pStart = (const char*)&m_Memory[m_Get];

		switch ( pFmt[1] )
		{
		case 0:
			return numScanned;

		case 'c':
			*va_arg( list, char* ) = (char)m_Memory[m_Get];
			m_Get++;
			break;

		case 's':
			GetString( va_arg( list, char* ) );
			break;

		case 'd':
		case 'i':
			*va_arg( list, int* ) = strtol( pStart, &pEnd, 10 );
			if ( pEnd == pStart )
				return numScanned;
			m_Get = (int)( pEnd - (char*)m_Memory.Base() );
			break;

		case 'u':
			*va_arg( list, unsigned int* ) = strtoul( pStart, &pEnd, 10 );
			if ( pEnd == pStart )
				return numScanned;
			m_Get = (int)( pEnd - (char*)m_Memory.Base() );
			break;

		case 'f':
			*va_arg( list, float* ) = (float)strtod( pStart, &pEnd );
			if ( pEnd == pStart )
				return numScanned;
			m_Get = (int)( pEnd - (char*)m_Memory.Base() );
			break;

		default:
			// unimplemented scanf type
#if defined( _WIN32 )
			ASSERT_AT( 0, 255 );
#endif
			return numScanned;
		}

		numScanned++;
		pFmt += 2;
	}

	return numScanned;
}

void CUtlBuffer::Printf( const char* pFmt, ... )
{
	va_list args;

	va_start( args, pFmt );
	VaPrintf( pFmt, args );
	va_end( args );
}

void CUtlBuffer::VaPrintf( const char* pFmt, va_list list )
{
	char temp[2048];
	int len;

	len = _vsnprintf( temp, sizeof( temp ), pFmt, list );
	if ( len < 0 )
		len = sizeof( temp ) - 1;
	temp[sizeof( temp ) - 1] = 0;
#if defined( _WIN32 )
	ASSERT_AT( len < 2048, 325 );
#endif

	Put( temp, len + ( IsText() ? 0 : 1 ) );
}

CUtlBuffer::CUtlBuffer( int growSize, int initialSize, bool text )
	: m_Memory( growSize, initialSize ), m_Get( 0 ), m_Put( 0 ), m_Error( 0 ), m_Flags( text ? TEXT_BUFFER : 0 )
{
}

CUtlBuffer::CUtlBuffer( const void* buffer, int size, bool text )
	: m_Memory( (unsigned char*)buffer, size ), m_Get( 0 ), m_Put( 0 ), m_Error( 0 ),
	  m_Flags( text ? TEXT_BUFFER : 0 )
{
}

const void* CUtlBuffer::Base() const
{
	return m_Memory.Base();
}

void* CUtlBuffer::Base()
{
	return m_Memory.Base();
}

int CUtlBuffer::TellPut() const
{
	return m_Put;
}

int CUtlBuffer::TellGet() const
{
	return m_Get;
}

bool CUtlBuffer::IsText() const
{
	return ( m_Flags & TEXT_BUFFER ) != 0;
}

bool CUtlBuffer::IsValid() const
{
	return !m_Error;
}

int CUtlBuffer::GetInt()
{
	int i;

	if ( IsText() )
	{
		Scanf( "%d", &i );
		return i;
	}

	if ( !CheckGet( sizeof( int ) ) )
		return 0;

	memcpy( &i, &m_Memory[m_Get], sizeof( i ) );
	m_Get += sizeof( i );
	return i;
}

void CUtlBuffer::PutInt( int i )
{
	if ( IsText() )
	{
		Printf( "%d", i );
		return;
	}

	if ( !CheckPut( sizeof( i ) ) )
		return;

	memcpy( &m_Memory[m_Put], &i, sizeof( i ) );
	m_Put += sizeof( i );
}

void CUtlBuffer::GetString( char* pString )
{
	if ( m_Error )
	{
		*pString = 0;
		return;
	}

	if ( IsText() )
	{
		EatWhiteSpace();

		int lastpos = m_Memory.NumAllocated();
		while ( m_Get < lastpos )
		{
			char c = (char)m_Memory[m_Get];
			if ( isspace( c ) || !c )
				break;
			*pString++ = c;
			m_Get++;
		}

		*pString = 0;
		return;
	}

	int len = (int)strlen( (const char*)&m_Memory[m_Get] ) + 1;
	Get( pString, len );
}

//-----------------------------------------------------------------------------
// Serialization
//-----------------------------------------------------------------------------

void CUtlBuffer::Put( const void* pMem, int size )
{
	if ( !CheckPut( size ) )
		return;

	memcpy( &m_Memory[m_Put], pMem, size );
	m_Put += size;
}

//-----------------------------------------------------------------------------
// Writes a null-terminated string
//-----------------------------------------------------------------------------

void CUtlBuffer::PutString( const char* pString )
{
	int len = (int)strlen( pString );
	// Not text? append a null at the end.
	if ( !IsText() )
		len++;

	Put( pString, len );
}

//-----------------------------------------------------------------------------
// Makes sure we've got at least this much memory
//-----------------------------------------------------------------------------
void CUtlBuffer::EnsureCapacity( int num )
{
	m_Memory.EnsureCapacity( num );
}

//-----------------------------------------------------------------------------
// Base get method from which all others derive
//-----------------------------------------------------------------------------
void CUtlBuffer::Get( void* pMem, int size )
{
#if defined( _WIN32 )
	ASSERT_AT( m_Get + size <= m_Memory.NumAllocated(), 62 );
#endif
	memcpy( pMem, m_Memory.Base() + m_Get, size );
	m_Get += size;
}

//-----------------------------------------------------------------------------
// Change where I'm reading
//-----------------------------------------------------------------------------
void CUtlBuffer::SeekGet( SeekType_t type, int offset )
{
	switch ( type )
	{
	case SEEK_HEAD:
		m_Get = offset;
		break;
	case SEEK_CURRENT:
		m_Get += offset;
		break;
	case SEEK_TAIL:
		m_Get = m_Memory.NumAllocated() - offset;
		break;
	}
}

void CUtlBuffer::SeekPut( SeekType_t type, int offset )
{
	switch ( type )
	{
	case SEEK_HEAD:
		m_Put = offset;
		break;
	case SEEK_CURRENT:
		m_Put += offset;
		break;
	case SEEK_TAIL:
		m_Put = m_Memory.NumAllocated() - offset;
		break;
	}
}
