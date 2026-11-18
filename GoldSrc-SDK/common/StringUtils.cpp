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

// Cbuf ends a command at ';' outside quotes and at a line break anywhere;
// a quote cannot be escaped, so it ends the quoted argument
static const char UNQUOTED_ARGUMENT_BREAKS[] = ";\"\n\r";
static const char QUOTED_ARGUMENT_BREAKS[] = "\"\n\r";

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

bool UTIL_IsUnquotedCommandArgument( const char* text )
{
	return !strpbrk( text, UNQUOTED_ARGUMENT_BREAKS );
}

bool UTIL_IsQuotedCommandArgument( const char* text )
{
	return !strpbrk( text, QUOTED_ARGUMENT_BREAKS );
}

void UTIL_StripQuotedCommandBreaks( char* text )
{
	char* dest = text;
	for ( const char* src = text; *src; src++ )
	{
		if ( !strchr( QUOTED_ARGUMENT_BREAKS, *src ) )
			*dest++ = *src;
	}
	*dest = 0;
}

void UTIL_UppercaseASCII( char* text )
{
	for ( ; *text; text++ )
	{
		if ( *text >= 'a' && *text <= 'z' )
			*text += 'A' - 'a';
	}
}
