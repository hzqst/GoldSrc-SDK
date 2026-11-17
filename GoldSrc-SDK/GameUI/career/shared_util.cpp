//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: dll-agnostic routines (no dll dependencies here)
//
// $NoKeywords: $
//=============================================================================

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

#include "shared_util.h"

#ifdef CLIENT_DLL
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

static char s_shared_token[1500];
static char s_shared_quote = '"';

#ifdef CLIENT_DLL
wchar_t* SharedFindString( char* asciiIdentifier )
{
	static wchar_t strings[4][1024];
	static int currentString = 0;
	wchar_t* identifier = vgui2::localize()->Find( asciiIdentifier );
	if ( !identifier )
	{
		identifier = strings[currentString];
		currentString = (currentString + 1) % 4;
		vgui2::localize()->ConvertANSIToUnicode( asciiIdentifier, identifier, 2048 );
	}
	return identifier;
}
#endif

wchar_t* SharedWVarArgs( wchar_t* format, ... )
{
	static wchar_t strings[4][1024];
	static int currentString = 0;
	currentString = (currentString + 1) % 4;
	va_list arguments;
	va_start( arguments, format );
#ifdef _WIN32
	int written = _vsnwprintf( strings[currentString], 1024, format, arguments );
#else
	int written = vswprintf( strings[currentString], 1024, format, arguments );
#endif
	va_end( arguments );
	if ( written < 0 || written >= 1024 )
		strings[currentString][1023] = L'\0';
	return strings[currentString];
}

char* SharedVarArgs( char* format, ... )
{
	static char strings[4][1024];
	static int currentString = 0;
	currentString = (currentString + 1) % 4;
	va_list arguments;
	va_start( arguments, format );
#ifdef _WIN32
	int written = _vsnprintf( strings[currentString], 1024, format, arguments );
#else
	int written = vsnprintf( strings[currentString], 1024, format, arguments );
#endif
	va_end( arguments );
	if ( written < 0 || written >= 1024 )
		strings[currentString][1023] = '\0';
	return strings[currentString];
}

char* BufPrintf( char* buffer, int& length, const char* format, ... )
{
	if ( length <= 0 )
		return NULL;
	va_list arguments;
	va_start( arguments, format );
#ifdef _WIN32
	int written = _vsnprintf( buffer, length, format, arguments );
#else
	int written = vsnprintf( buffer, length, format, arguments );
#endif
	va_end( arguments );
	if ( written < 0 || written >= length )
		buffer[length - 1] = '\0';
	length -= (int)strlen( buffer );
	return buffer + strlen( buffer );
}

wchar_t* BufWPrintf( wchar_t* buffer, int& length, const wchar_t* format, ... )
{
	if ( length <= 0 )
		return NULL;
	va_list arguments;
	va_start( arguments, format );
#ifdef _WIN32
	int written = _vsnwprintf( buffer, length, format, arguments );
#else
	int written = vswprintf( buffer, length, format, arguments );
#endif
	va_end( arguments );
	if ( written < 0 || written >= length )
		buffer[length - 1] = L'\0';
	length -= (int)wcslen( buffer );
	return buffer + wcslen( buffer );
}

const wchar_t* NumAsWString( int value )
{
	static wchar_t strings[4][16];
	static int currentString = 0;
	currentString = (currentString + 1) % 4;
	int length = 16;
	BufWPrintf( strings[currentString], length, L"%d", value );
	return strings[currentString];
}

const char* NumAsString( int value )
{
	static char strings[4][16];
	static int currentString = 0;
	currentString = (currentString + 1) % 4;
	int length = 16;
	BufPrintf( strings[currentString], length, "%d", value );
	return strings[currentString];
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns the token parsed by SharedParse()
 */
char* SharedGetToken()
{
	return s_shared_token;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns the token parsed by SharedParse()
 */
void SharedSetQuoteChar( char quote )
{
	s_shared_quote = quote;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Parse a token out of a string
 */
const char* SharedParse( const char* data )
{
	int length = 0;
	int character;
	s_shared_token[0] = '\0';
	if ( !data )
		return NULL;

skipWhitespace:
	while ( ( character = *data ) <= ' ' )
	{
		if ( !character )
			return NULL;
		++data;
	}

// skip // comments
	if ( character == '/' && data[1] == '/' )
	{
		while ( *data && *data != '\n' )
			++data;
		goto skipWhitespace;
	}


// handle quoted strings specially
	if ( character == s_shared_quote )
	{
		++data;
		while ( length < (int)sizeof( s_shared_token ) - 1 )
		{
			character = *data++;
			if ( character == s_shared_quote || !character )
			{
				s_shared_token[length] = '\0';
				return data;
			}
			s_shared_token[length++] = (char)character;
		}
	}
	if ( length < (int)sizeof( s_shared_token ) - 1 )
	{
		if ( character == '{' || character == '}' || character == ')' || character == '('
			|| character == '\'' || character == ',' )
		{
			s_shared_token[length++] = (char)character;
			s_shared_token[length] = '\0';
			return data + 1;
		}
	}
	while ( length < (int)sizeof( s_shared_token ) - 1 )
	{
		s_shared_token[length++] = (char)character;
		character = *++data;
		if ( character == '{' || character == '}' || character == ')' || character == '('
			|| character == '\'' || character == ',' || character <= ' ' )
			break;
	}
	s_shared_token[length] = '\0';
	return data;
}

//--------------------------------------------------------------------------------------------------------
/**
 * Returns true if additional data is waiting to be processed on this line
 */
bool SharedTokenWaiting( const char* buffer )
{
	while ( *buffer && *buffer != '\n' )
	{
		if ( !isspace( *buffer ) || isalnum( *buffer ) )
			return true;
		++buffer;
	}
	return false;
}
