/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
//  parsemsg.cpp
//
//--------------------------------------------------------------------------------------------------------------

#include "parsemsg.h"

#include <string.h>

static unsigned char* gpBuf;
static int giSize;
static int giRead;
static int giBadRead;

int READ_OK()
{
	return !giBadRead;
}

void BEGIN_READ( void* buffer, int size )
{
	giRead = 0;
	giBadRead = 0;
	giSize = size;
	gpBuf = (unsigned char*)buffer;
}

int READ_CHAR()
{
	if ( giRead + 1 > giSize )
	{
		giBadRead = 1;
		return -1;
	}
	return (signed char)gpBuf[giRead++];
}

int READ_BYTE()
{
	if ( giRead + 1 > giSize )
	{
		giBadRead = 1;
		return -1;
	}
	return gpBuf[giRead++];
}

int READ_SHORT()
{
	if ( giRead + 2 > giSize )
	{
		giBadRead = 1;
		return -1;
	}
	int value = (short)( gpBuf[giRead] + ( gpBuf[giRead + 1] << 8 ) );
	giRead += 2;
	return value;
}

int READ_WORD()
{
	return READ_SHORT();
}

int READ_LONG()
{
	if ( giRead + 4 > giSize )
	{
		giBadRead = 1;
		return -1;
	}
	int value = gpBuf[giRead] + ( gpBuf[giRead + 1] << 8 )
		+ ( gpBuf[giRead + 2] << 16 ) + ( gpBuf[giRead + 3] << 24 );
	giRead += 4;
	return value;
}

float READ_FLOAT()
{
	union
	{
		unsigned char bytes[4];
		float value;
	} number;
	for ( int i = 0; i < 4; ++i )
		number.bytes[i] = gpBuf[giRead + i];
	giRead += 4;
	return number.value;
}

char* READ_STRING()
{
	static char text[2048];
	int length = 0;
	text[0] = 0;
	do
	{
		if ( giRead + 1 > giSize )
			break; // no more characters
		int character = READ_CHAR();
		if ( character == -1 || character == 0 )
			break;
		text[length++] = (char)character;
	}
	while ( length < (int)sizeof( text ) - 1 );
	text[length] = 0;
	return text;
}

float READ_COORD()
{
	return (float)( READ_SHORT() * ( 1.0 / 8 ) );
}

float READ_ANGLE()
{
	return (float)( READ_CHAR() * ( 360.0 / 256 ) );
}

float READ_HIRESANGLE()
{
	return (float)( READ_SHORT() * ( 360.0 / 65536 ) );
}

//--------------------------------------------------------------------------------------------------------------
BufferWriter::BufferWriter()
{
	Init( NULL, 0 );
}

//--------------------------------------------------------------------------------------------------------------
BufferWriter::BufferWriter( unsigned char* buffer, int size )
{
	Init( buffer, size );
}

//--------------------------------------------------------------------------------------------------------------
void BufferWriter::Init( unsigned char* buffer, int size )
{
	m_buffer = buffer;
	m_remaining = size;
	m_overflow = false;
	m_overallLength = size;
}

//--------------------------------------------------------------------------------------------------------------
void BufferWriter::WriteByte( unsigned char value )
{
	if ( !m_buffer || !m_remaining )
	{
		m_overflow = true;
		return;
	}
	*m_buffer++ = value;
	--m_remaining;
}

//--------------------------------------------------------------------------------------------------------------
void BufferWriter::WriteLong( int value )
{
	if ( !m_buffer || m_remaining < 4 )
	{
		m_overflow = true;
		return;
	}
	for ( int i = 0; i < 4; ++i )
		m_buffer[i] = (unsigned char)( value >> ( 8 * i ) );
	m_buffer += 4;
	m_remaining -= 4;
}

//--------------------------------------------------------------------------------------------------------------
void BufferWriter::WriteString( const char* text )
{
	if ( !m_buffer || !m_remaining )
	{
		m_overflow = true;
		return;
	}
	if ( !text )
		text = "";
	int length = (int)strlen( text ) + 1;
	if ( length > m_remaining )
	{
		m_overflow = true;
		text = "";
		length = 1;
	}
	strcpy( (char*)m_buffer, text );
	m_buffer += length;
	m_remaining -= length;
}

//--------------------------------------------------------------------------------------------------------------
int BufferWriter::GetSpaceUsed()
{
	return m_overallLength - m_remaining;
}

//--------------------------------------------------------------------------------------------------------------
