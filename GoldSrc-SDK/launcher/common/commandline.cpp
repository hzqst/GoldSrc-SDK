//======== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================

#include "icommandline.h"
#include "../../common/commandline_token.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <ctype.h>

#define _snprintf	snprintf
#endif

#define COMMAND_BUFFER_SIZE			4096
#define PARAMETER_VALUE_LIMIT		128
#define FORMATTED_INTEGER_LENGTH	64
#define PARAMETER_FILE_MARK			( (char)'@' )

#if defined(_WIN32)
#define PARAMETER_FILE_BUFFER_SIZE	MAX_PATH
#define PARAMETER_VALUE_BUFFER_SIZE	PARAMETER_VALUE_LIMIT
#else
#define PARAMETER_FILE_BUFFER_SIZE	COMMAND_BUFFER_SIZE
#define PARAMETER_VALUE_BUFFER_SIZE	( PARAMETER_VALUE_LIMIT + 1 )
#endif

static bool IsCommandWhitespace( char character )
{
#if defined(_WIN32)
	return isspace( (signed char)character ) != 0;
#else
	return ( ( *__ctype_b_loc() )[(signed char)character] & _ISspace ) != 0;
#endif
}

class CCommandLine : public ICommandLine
{
public:
	// Construction
	CCommandLine();
	virtual ~CCommandLine();

	void CreateCmdLine( const char* commandLine ) override;
#if !defined(_WIN32)
	void CreateCmdLine( int argc, const char** argv ) override;
#endif
	const char* GetCmdLine() override;
	const char* CheckParm( const char* parameter, char** value ) override;

	void RemoveParm( const char* parameter ) override;
	void AppendParm( const char* parameter, const char* values ) override;
	void SetParm( const char* parameter, const char* values ) override;
	void SetParm( const char* parameter, int value ) override;

private:
	char*	m_pszCmdLine;
};

static CCommandLine g_CmdLine;

ICommandLine* cmdline = &g_CmdLine;

CCommandLine::CCommandLine()
	: m_pszCmdLine( NULL )
{
}

CCommandLine::~CCommandLine()
{
	delete[] m_pszCmdLine;
}

//-----------------------------------------------------------------------------
// Purpose: Return current command line
// Output : const char
//-----------------------------------------------------------------------------
const char* CCommandLine::GetCmdLine()
{
	return m_pszCmdLine;
}

#if !defined(_WIN32)
void CCommandLine::CreateCmdLine( int argc, const char** argv )
{
	char commandLine[COMMAND_BUFFER_SIZE];
	int index;

	commandLine[0] = '\0';

	for ( index = 0; index < argc; ++index )
	{
		const char* argument = argv[index];

		if ( strchr( argument, ' ' ) != NULL )
		{
			strcat( commandLine, "\"" );
			strncat( commandLine, argument, COMMAND_BUFFER_SIZE );
			strcat( commandLine, "\"" );
		}
		else
		{
			strncat( commandLine, argument, COMMAND_BUFFER_SIZE );
		}

		strcat( commandLine, " " );
	}

	if ( argc > 0 )
	{
		commandLine[strlen( commandLine ) - 1] = '\0';
	}

	CreateCmdLine( commandLine );
}
#endif

void CCommandLine::CreateCmdLine( const char* commandLine )
{
	char expanded[COMMAND_BUFFER_SIZE];
	char fileName[PARAMETER_FILE_BUFFER_SIZE];

	delete[] m_pszCmdLine;

	char* copy;
	if ( commandLine != NULL )
	{
		copy = new char[strlen( commandLine ) + 1];
		strcpy( copy, commandLine );
	}
	else
	{
		copy = NULL;
	}

	char* out = expanded;
	const char* cursor = copy;
	bool atTokenStart = true;

#pragma warning( suppress : 6011 )
	while ( *cursor != '\0' )
	{
		if ( (size_t)( out - expanded ) >= sizeof( expanded ) - 1 )
		{
			break;
		}

		if ( atTokenStart && *cursor == PARAMETER_FILE_MARK )
		{
			++cursor;

			const size_t fileNameLength = strcspn( cursor, "\0 " );
			if ( fileNameLength >= sizeof( fileName ) - 1 )
			{
				printf( "Parameter filename too long, skipping\n" );
				cursor += fileNameLength;
				if ( *cursor == '\0' )
				{
					break;
				}

				++cursor;
				continue;
			}

			memcpy( fileName, cursor, fileNameLength );
			fileName[fileNameLength] = '\0';

			if ( *cursor != '\0' )
			{
				++cursor;
			}

			// Now read in parameters from file
			FILE* parameterFile = fopen( fileName, "r" );
			if ( parameterFile != NULL )
			{
				char next = (char)fgetc( parameterFile );
				while ( next != EOF )
				{
					if ( (size_t)( out - expanded ) >= sizeof( expanded ) - 2 )
					{
						break;
					}

					if ( next == '\n' )
					{
						next = ' ';
					}

					*out++ = next;
					next = (char)fgetc( parameterFile );
				}

				// Add a terminating space character
				*out++ = ' ';
				fclose( parameterFile );
			}
			else
			{
				printf( "Parameter file '%s' not found, skipping...", fileName );
			}

			continue;
		}

		atTokenStart = IsCommandWhitespace( *cursor );
		*out++ = *cursor++;
	}

	*out = '\0';
	delete[] copy;

	m_pszCmdLine = new char[strlen( expanded ) + 1];
	strcpy( m_pszCmdLine, expanded );
}

const char* CCommandLine::CheckParm( const char* parameter, char** value )
{
	static char parameterValue[PARAMETER_VALUE_BUFFER_SIZE];
	if ( value )
		*value = NULL;
	if ( !m_pszCmdLine || !parameter || !*parameter )
		return NULL;
	const char* cursor = m_pszCmdLine;
	while ( *cursor )
	{
		const auto token = ReadToken( cursor );
		cursor = token.end;
		if ( !token.Matches( parameter ) )
			continue;
		if ( value )
		{
			const auto next = ReadToken( cursor );
			if ( next.begin != next.end && !next.IsParameter() )
			{
				strncpy( parameterValue, next.value.c_str(), sizeof( parameterValue ) - 1 );
				parameterValue[sizeof( parameterValue ) - 1] = '\0';
				*value = parameterValue;
			}
		}
		return token.begin;
	}
	return NULL;
}

void CCommandLine::RemoveParm( const char* parameter )
{
	if ( m_pszCmdLine == NULL || parameter == NULL || *parameter == '\0' )
	{
		return;
	}

	const std::string parm( parameter );
	const char* cursor = m_pszCmdLine;
	while ( *cursor )
	{
		const auto token = ReadToken( cursor );
		if ( !token.Matches( parm.c_str() ) )
		{
			cursor = token.end;
			continue;
		}
		const char* next = token.end;
		for ( ;; )
		{
			const auto value = ReadToken( next );
			next = value.begin;
			if ( !*next || value.IsParameter() )
				break;
			next = value.end;
		}
		cursor = token.begin;
		memmove( m_pszCmdLine + ( cursor - m_pszCmdLine ), next, strlen( next ) + 1 );
	}
	size_t length = strlen( m_pszCmdLine );
	while ( length && isspace( static_cast<unsigned char>( m_pszCmdLine[length - 1] ) ) )
		m_pszCmdLine[--length] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: Append parameter and argument values to command line
// Input  : *pszParm - 
//			*pszValues - 
//-----------------------------------------------------------------------------
void CCommandLine::AppendParm( const char* parameter, const char* values )
{
	size_t newLength = strlen( parameter );
	if ( values != NULL )
	{
		newLength += strlen( values ) + 1; // Values + leading space character.
	}
	++newLength;

	if ( m_pszCmdLine == NULL )
	{
		m_pszCmdLine = new char[newLength];
		strcpy( m_pszCmdLine, parameter );
		if ( values != NULL )
		{
			strcat( m_pszCmdLine, " " );
			strcat( m_pszCmdLine, values );
		}

		return;
	}

	// Remove any remnants from the current Cmd Line.
	RemoveParm( parameter );

	newLength += strlen( m_pszCmdLine ) + 1 + 1;

	char* updated = new char[newLength];
	memset( updated, 0, newLength );

	strcpy( updated, m_pszCmdLine ); // Copy old command line.
	strcat( updated, " " ); // Put in a space
	strcat( updated, parameter );
	if ( values != NULL )
	{
		strcat( updated, " " );
		strcat( updated, values );
	}

	delete[] m_pszCmdLine;
	m_pszCmdLine = updated;
}

void CCommandLine::SetParm( const char* parameter, const char* values )
{
	RemoveParm( parameter );
	AppendParm( parameter, values );
}

void CCommandLine::SetParm( const char* parameter, int value )
{
	char text[FORMATTED_INTEGER_LENGTH];

	_snprintf( text, sizeof( text ), "%d", value );
	SetParm( parameter, text );
}
