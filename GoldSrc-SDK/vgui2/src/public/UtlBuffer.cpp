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

#include "UtlBuffer.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );
#endif

//-----------------------------------------------------------------------------
// constructors
//-----------------------------------------------------------------------------
CUtlBuffer::CUtlBuffer( int growSize, int initSize, bool text )
	: m_Memory( growSize, initSize ), m_Error( 0 )
{
	m_Get = 0;
	m_Put = 0;
	m_Flags = 0;
	if ( text )
	{
		m_Flags |= TEXT_BUFFER;
	}
}

CUtlBuffer::CUtlBuffer( const void* pBuffer, int size, bool text )
	: m_Memory( (unsigned char*)pBuffer, size ), m_Error( 0 )
{
	m_Get = 0;
	m_Put = 0;
	m_Flags = 0;
	if ( text )
	{
		m_Flags |= TEXT_BUFFER;
	}
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
	memcpy( pMem, &m_Memory[m_Get], size );
	m_Get += size;
}

//-----------------------------------------------------------------------------
// Eats whitespace
//-----------------------------------------------------------------------------
void CUtlBuffer::EatWhiteSpace()
{
	if ( IsText() && IsValid() )
	{
		int lastpos = Size();
		while ( m_Get < lastpos )
		{
			if ( !isspace( (char)m_Memory[m_Get] ) )
			{
				break;
			}

			++m_Get;
		}
	}
}

void CUtlBuffer::GetString( char* pString )
{
	if ( !IsValid() )
	{
		*pString = 0;
		return;
	}

	if ( IsText() )
	{
		int lastpos = Size();

		EatWhiteSpace();

		while ( m_Get < lastpos )
		{
			char c = (char)m_Memory[m_Get];
			if ( isspace( c ) || c == 0 )
			{
				break;
			}

			*pString++ = c;
			++m_Get;
		}

		*pString = 0;
	}
	else
	{
		int len = strlen( (char*)&m_Memory[m_Get] ) + 1;
		Get( pString, len );
	}
}

//-----------------------------------------------------------------------------
// Checks if a get is ok
//-----------------------------------------------------------------------------
bool CUtlBuffer::CheckGet( int size )
{
	if ( m_Error )
	{
		return false;
	}

	if ( m_Memory.NumAllocated() >= m_Get + size )
	{
		return true;
	}

	m_Error |= GET_OVERFLOW;
	return false;
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

int CUtlBuffer::VaScanf( const char* pFmt, va_list list )
{
	if ( m_Error || ( m_Flags & TEXT_BUFFER ) == 0 )
	{
		return 0;
	}

	int numScanned = 0;
	char c;
	char* pEnd;

	while ( ( c = *pFmt++ ) )
	{
		if ( m_Get >= m_Memory.m_NumAllocated )
		{
			m_Error |= GET_OVERFLOW;
			break;
		}

		switch ( c )
		{
		case ' ':
			// eat all whitespace
			EatWhiteSpace();
			break;

		case '%':
		{
			// Conversion character... try to convert baby!
			char type = *pFmt++;
			if ( type == 0 )
			{
				return numScanned;
			}

			switch ( type )
			{
			case 'c':
			{
				char* ch = va_arg( list, char* );
				*ch = m_Memory.m_pMemory[m_Get];
				++m_Get;
				break;
			}

			case 'i':
			case 'd':
			{
				int* i = va_arg( list, int* );
				*i = strtol( (char*)( m_Memory.m_pMemory + m_Get ), &pEnd, 10 );
				if ( pEnd == (char*)( m_Memory.m_pMemory + m_Get ) )
				{
					return numScanned;
				}

				m_Get = pEnd - (char*)m_Memory.m_pMemory;
				break;
			}

			case 'u':
			{
				unsigned int* u = va_arg( list, unsigned int* );
				*u = strtoul( (char*)( m_Memory.m_pMemory + m_Get ), &pEnd, 10 );
				if ( pEnd == (char*)( m_Memory.m_pMemory + m_Get ) )
				{
					return numScanned;
				}

				m_Get = pEnd - (char*)m_Memory.m_pMemory;
				break;
			}

			case 'f':
			{
				float* f = va_arg( list, float* );
				*f = (float)strtod( (char*)( m_Memory.m_pMemory + m_Get ), &pEnd );
				if ( pEnd == (char*)( m_Memory.m_pMemory + m_Get ) )
				{
					return numScanned;
				}

				m_Get = pEnd - (char*)m_Memory.m_pMemory;
				break;
			}

			case 's':
			{
				char* s = va_arg( list, char* );
				GetString( s );
				break;
			}

			default:
				return numScanned;
			}

			++numScanned;
			break;
		}

		default:
			if ( c != m_Memory.m_pMemory[m_Get] )
			{
				return numScanned;
			}

			++m_Get;
			break;
		}
	}

	return numScanned;
}

int CUtlBuffer::Scanf( const char* pFmt, ... )
{
	va_list args;

	va_start( args, pFmt );
	int count = VaScanf( pFmt, args );
	va_end( args );
	return count;
}

//-----------------------------------------------------------------------------
// Serialization
//-----------------------------------------------------------------------------

void CUtlBuffer::Put( const void* pMem, int size )
{
	if ( CheckPut( size ) )
	{
		memcpy( &m_Memory.m_pMemory[m_Put], pMem, size );
		m_Put += size;
	}
}

//-----------------------------------------------------------------------------
// Writes a null-terminated string
//-----------------------------------------------------------------------------

void CUtlBuffer::PutString( const char* pString )
{
	int len = strlen( pString );

	if ( ( m_Flags & TEXT_BUFFER ) == 0 )
	{
		++len;
	}

	Put( pString, len );
}

void CUtlBuffer::PutUnsignedShort( unsigned short value )
{
	if ( IsText() )
	{
		Printf( "%u", (unsigned int)value );
	}
	else if ( CheckPut( sizeof( value ) ) )
	{
		memcpy( &m_Memory[m_Put], &value, sizeof( value ) );
		m_Put += sizeof( value );
	}
}

void CUtlBuffer::VaPrintf( const char* pFmt, va_list list )
{
	char temp[2048];
	int len = vsprintf( temp, pFmt, list );

#ifdef _WIN32
	if ( len >= 2048 )
	{
		_wassert( L"len < 2048", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\UtlBuffer.cpp", 325 );
	}
#endif

	if ( ( m_Flags & TEXT_BUFFER ) == 0 )
	{
		++len;
	}

	Put( temp, len );
}

void CUtlBuffer::Printf( const char* pFmt, ... )
{
	va_list args;

	va_start( args, pFmt );
	VaPrintf( pFmt, args );
	va_end( args );
}

//-----------------------------------------------------------------------------
// Checks if a put is ok
//-----------------------------------------------------------------------------

bool CUtlBuffer::CheckPut( int size )
{
	if ( m_Error )
	{
		return false;
	}

	while ( m_Memory.m_NumAllocated < m_Put + size )
	{
		if ( m_Memory.m_GrowSize < 0 )
		{
			m_Error |= PUT_OVERFLOW;
			return false;
		}

		m_Memory.Grow( 0 );
	}

	return true;
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
		m_Put = m_Memory.m_NumAllocated - offset;
		break;
	}
}
