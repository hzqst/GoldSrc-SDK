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

#include "UtlMemory.h"

#include <stdarg.h>

//-----------------------------------------------------------------------------
// Command parsing..
//-----------------------------------------------------------------------------

class CUtlBuffer
{
public:
	enum SeekType_t
	{
		SEEK_HEAD	= 0,
		SEEK_CURRENT,
		SEEK_TAIL
	};

	CUtlBuffer( int growSize = 0, int initSize = 0, bool text = false );
	CUtlBuffer( const void* pBuffer, int size, bool text = false );

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
	void GetString( char* pString );
	void Get( void* pMem, int size );

	// Just like scanf, but doesn't work in binary mode
	int Scanf( const char* pFmt, ... );
	int VaScanf( const char* pFmt, va_list list );

	// Eats white space, advances Get index
	void EatWhiteSpace();

	// Write stuff in
	// Binary mode: it'll just write the bits directly in, and strings will be
	//		written with a null terminating character
	// Text mode: it'll convert the numbers to text versions
	//		PutString will not write a terminating character
	void PutChar( char c );
	void PutUnsignedChar( unsigned char uc );
	void PutShort( short s );
	void PutUnsignedShort( unsigned short us );
	void PutInt( int i );
	void PutUnsignedInt( unsigned int u );
	void PutFloat( float f );
	void PutDouble( double d );
	void PutString( const char* pString );
	void Put( const void* pMem, int size );

	// Just like printf, writes a terminating zero in binary mode
	void Printf( const char* pFmt, ... );
	void VaPrintf( const char* pFmt, va_list list );

	void* PeekPut( int offset = 0 );
	const void* PeekGet( int offset = 0 ) const;

	int TellPut() const;
	int TellGet() const;

	void SeekPut( SeekType_t type, int offset );
	void SeekGet( SeekType_t type, int offset );

	const void* Base() const;
	void* Base();

	// memory allocation size, does *not* reflect size written or read,
	//	use TellPut or TellGet for that
	int Size() const;

	bool IsText() const;
	bool IsValid() const;

	void Clear();

private:
	// error flags
	enum
	{
		PUT_OVERFLOW	= 0x1,
		GET_OVERFLOW	= 0x2
	};

	// flags
	enum
	{
		TEXT_BUFFER	= 0x1
	};

	// Checks if a get/put is ok
	bool CheckPut( int size );
	bool CheckGet( int size );

	CUtlMemory<unsigned char>	m_Memory;

	int							m_Get;
	int							m_Put;

	unsigned char				m_Error;
	unsigned char				m_Flags;
};

inline bool CUtlBuffer::IsText() const
{
	return ( m_Flags & TEXT_BUFFER ) != 0;
}

inline bool CUtlBuffer::IsValid() const
{
	return m_Error == 0;
}

inline int CUtlBuffer::Size() const
{
	return m_Memory.NumAllocated();
}

//-----------------------------------------------------------------------------
// Where am I writing?
//-----------------------------------------------------------------------------
inline int CUtlBuffer::TellPut() const
{
	return m_Put;
}

//-----------------------------------------------------------------------------
// Buffer base and size
//-----------------------------------------------------------------------------

inline const void* CUtlBuffer::Base() const
{
	return m_Memory.Base();
}

inline void* CUtlBuffer::Base()
{
	return m_Memory.Base();
}

#endif // UTLBUFFER_H
