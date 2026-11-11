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

#pragma once

#include "utlmemory.h"
#include <cstdarg>

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

	CUtlBuffer( int growSize, int initialSize, bool text );
	CUtlBuffer( const void* buffer, int size, bool text );
	~CUtlBuffer() = default;

	void EnsureCapacity( int num );

	int GetInt();
	void Get( void* pMem, int size );
	void GetString( char* output );
	// Just like scanf, but doesn't work in binary mode
	int Scanf( const char* format, ... );
	int VaScanf( const char* format, va_list arguments );
	// Eats white space, advances Get index
	void EatWhiteSpace();

	void PutInt( int value );
	void PutChar( char value );
	void PutString( const char* value );
	void Put( const void* source, int size );
	// Just like printf, writes a terminating zero in binary mode
	void Printf( const char* format, ... );
	void VaPrintf( const char* format, va_list arguments );

	void SeekGet( SeekType_t type, int offset );
	void SeekPut( SeekType_t type, int offset );
	int TellPut() const;
	int TellGet() const;

	const void* Base() const;
	void* Base();
	int Size() const { return m_Memory.NumAllocated(); }
	bool IsText() const;
	bool IsValid() const;

private:
	enum
	{
		TEXT_BUFFER = 1
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
