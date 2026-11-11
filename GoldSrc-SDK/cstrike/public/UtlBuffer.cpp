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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"public\\UtlBuffer.cpp" )

CUtlBuffer::CUtlBuffer( int growSize, int initSize, bool text )
	: m_Memory( growSize, initSize ), m_Get( 0 ), m_Put( 0 ), m_Error( 0 ), m_Flags( text ? TEXT_BUFFER : 0 )
{
}

CUtlBuffer::CUtlBuffer( const void* buffer, int size, bool text )
	: m_Memory( (unsigned char*)buffer, size ), m_Get( 0 ), m_Put( 0 ), m_Error( 0 ), m_Flags( text ? TEXT_BUFFER : 0 )
{
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
void CUtlBuffer::Get( void* memory, int size )
{
	memcpy( memory, &m_Memory[m_Get], size );
	m_Get += size;
}

void CUtlBuffer::EatWhiteSpace()
{
	if ( IsText() && IsValid() )
	{
		while ( m_Get < Size() && isspace( (char)m_Memory[m_Get] ) )
			++m_Get;
	}
}

void CUtlBuffer::GetString( char* string )
{
	if ( !IsValid() )
	{
		*string = 0;
		return;
	}
	if ( IsText() )
	{
		EatWhiteSpace();
		while ( m_Get < Size() )
		{
			char value = (char)m_Memory[m_Get];
			if ( isspace( value ) || !value )
				break;
			*string++ = value;
			++m_Get;
		}
		*string = 0;
	}
	else
	{
		Get( string, (int)strlen( (char*)&m_Memory[m_Get] ) + 1 );
	}
}


//-----------------------------------------------------------------------------
// Checks if a get is ok
//-----------------------------------------------------------------------------
bool CUtlBuffer::CheckGet( int size )
{
	if ( m_Error )
		return false;
	if ( m_Get + size > Size() )
	{
		m_Error |= GET_OVERFLOW;
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Checks if a put is ok
//-----------------------------------------------------------------------------

bool CUtlBuffer::CheckPut( int size )
{
	if ( m_Error )
		return false;
	while ( Size() < m_Put + size )
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
		m_Get = Size() - offset;
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
		m_Put = Size() - offset;
		break;
	}
}

int CUtlBuffer::VaScanf( const char* format, va_list arguments )
{
	if ( !IsValid() || !IsText() )
		return 0;
	int count = 0;
	char value;
	while ( ( value = *format++ ) != 0 )
	{
		// Stop if we hit the end of the buffer
		if ( m_Get >= Size() )
		{
			m_Error |= GET_OVERFLOW;
			break;
		}
		if ( value == ' ' )
		{
			EatWhiteSpace();
		}
		else if ( value == '%' )
		{
			char* end;
			switch ( *format++ )
			{
			case 'c':
				*va_arg( arguments, char* ) = (char)m_Memory[m_Get++];
				break;
			case 'd':
			case 'i':
				*va_arg( arguments, int* ) = strtol( (char*)&m_Memory[m_Get], &end, 10 );
				if ( end == (char*)&m_Memory[m_Get] )
					return count;
				m_Get = (int)( end - (char*)m_Memory.Base() );
				break;
			case 'u':
				*va_arg( arguments, unsigned int* ) = strtoul( (char*)&m_Memory[m_Get], &end, 10 );
				if ( end == (char*)&m_Memory[m_Get] )
					return count;
				m_Get = (int)( end - (char*)m_Memory.Base() );
				break;
			case 'f':
				*va_arg( arguments, float* ) = (float)strtod( (char*)&m_Memory[m_Get], &end );
				if ( end == (char*)&m_Memory[m_Get] )
					return count;
				m_Get = (int)( end - (char*)m_Memory.Base() );
				break;
			case 's':
				GetString( va_arg( arguments, char* ) );
				break;
			default:
				return count;
			}
			++count;
		}
		else
		{
			if ( value != m_Memory[m_Get] )
				return count;
			++m_Get;
		}
	}
	return count;
}

int CUtlBuffer::Scanf( const char* format, ... )
{
	va_list arguments;
	va_start( arguments, format );
	int result = VaScanf( format, arguments );
	va_end( arguments );
	return result;
}


//-----------------------------------------------------------------------------
// Serialization
//-----------------------------------------------------------------------------

void CUtlBuffer::Put( const void* memory, int size )
{
	if ( CheckPut( size ) )
	{
		memcpy( &m_Memory[m_Put], memory, size );
		m_Put += size;
	}
}

void CUtlBuffer::PutString( const char* string )
{
	Put( string, (int)strlen( string ) + ( IsText() ? 0 : 1 ) );
}

void CUtlBuffer::VaPrintf( const char* format, va_list arguments )
{
	char buffer[2048];
	int len = vsprintf( buffer, format, arguments );
	ASSERT_AT( len < 2048, 325 );

	Put( buffer, len + ( IsText() ? 0 : 1 ) );
}

void CUtlBuffer::Printf( const char* format, ... )
{
	va_list arguments;
	va_start( arguments, format );
	VaPrintf( format, arguments );
	va_end( arguments );
}
