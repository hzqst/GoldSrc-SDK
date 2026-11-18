//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: bounded string helpers that report truncation instead of hiding it
//
// $NoKeywords: $
//=============================================================================

#include "StringUtils.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

bool UTIL_FormatString( char* buffer, size_t bufferSize, const char* format, ... )
{
	if ( !bufferSize )
		return false;

	va_list args;
	va_start( args, format );
#ifdef _WIN32
	int length = _vsnprintf( buffer, bufferSize, format, args );
#else
	int length = vsnprintf( buffer, bufferSize, format, args );
#endif
	va_end( args );

	// _vsnprintf leaves no terminator when it truncates
	buffer[bufferSize - 1] = 0;
	return length >= 0 && (size_t)length < bufferSize;
}

bool UTIL_CopyString( char* buffer, size_t bufferSize, const char* source )
{
	size_t length = strlen( source );
	if ( length >= bufferSize )
		return false;

	memcpy( buffer, source, length + 1 );
	return true;
}

void UTIL_UppercaseASCII( char* text )
{
	for ( ; *text; text++ )
	{
		if ( *text >= 'a' && *text <= 'z' )
			*text += 'A' - 'a';
	}
}
