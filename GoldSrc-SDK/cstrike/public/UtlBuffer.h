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
// Serialization/unserialization buffer
//=============================================================================

#ifndef UTLBUFFER_H
#define UTLBUFFER_H

#include "../../engine/utlmemory.h"
#include <stdarg.h>

//-----------------------------------------------------------------------------
// Command parsing..
//-----------------------------------------------------------------------------

class CUtlBuffer
{
public:
	enum SeekType_t
	{
		SEEK_HEAD,
		SEEK_CURRENT,
		SEEK_TAIL
	};

	CUtlBuffer( int growSize = 0, int initSize = 0, bool text = false );
	CUtlBuffer( const void* buffer, int size, bool text = false );

	// Makes sure we've got at least this much memory
	void EnsureCapacity( int num );

	// Read stuff out.
	// Binary mode: it'll just read the bits directly in, and characters will be
	//		read for strings until a null character is reached.
	// Text mode: it'll parse the file, turning text #s into real numbers.
	//		GetString will read a string until a space is reaced
	char GetChar();
	unsigned char GetUnsignedChar();
	short GetShort();
	unsigned short GetUnsignedShort();
	int GetInt();
	unsigned int GetUnsignedInt();
	float GetFloat();
	double GetDouble();
	void GetString( char* string );
	void Get( void* memory, int size );
	int Scanf( const char* format, ... );
	int VaScanf( const char* format, va_list arguments );

	// Eats white space, advances Get index
	void EatWhiteSpace();
	void PutChar( char value );
	void PutUnsignedChar( unsigned char value );
	void PutShort( short value );
	void PutUnsignedShort( unsigned short value );
	void PutInt( int value );
	void PutUnsignedInt( unsigned int value );
	void PutFloat( float value );
	void PutDouble( double value );
	void PutString( const char* string );
	void Put( const void* memory, int size );
	void Printf( const char* format, ... );
	void VaPrintf( const char* format, va_list arguments );

	// What am I writing (put)/reading (get)?
	void* PeekPut( int offset = 0 );
	const void* PeekGet( int offset = 0 ) const;
	int TellPut() const { return m_Put; }
	int TellGet() const;

	// Change where I'm writing (put)/reading (get)
	void SeekPut( SeekType_t type, int offset );
	void SeekGet( SeekType_t type, int offset );
	const void* Base() const { return m_Memory.Base(); }
	void* Base() { return m_Memory.Base(); }
	int Size() const { return m_Memory.NumAllocated(); }
	bool IsText() const { return ( m_Flags & TEXT_BUFFER ) != 0; }
	bool IsValid() const { return m_Error == 0; }
	void Clear();

private:
	// error flags
	enum
	{
		PUT_OVERFLOW = 1,
		GET_OVERFLOW = 2,
		TEXT_BUFFER = 1
	};

	// Checks if a get/put is ok
	bool CheckPut( int size );
	bool CheckGet( int size );
	CUtlMemory<unsigned char> m_Memory;
	int m_Get;
	int m_Put;
	unsigned char m_Error;
	unsigned char m_Flags;
};

inline void CUtlBuffer::PutChar( char value )
{
	if ( IsText() )
	{
		Printf( "%c", value );
	}
	else if ( CheckPut( 1 ) )
	{
		m_Memory[m_Put] = value;
		++m_Put;
	}
}

#endif
