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
#include "commandline_token.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

#if defined( _WIN32 )
#include <windows.h>
#endif

#define COMMAND_LINE_SIZE	4096

class CCommandLine : public ICommandLine
{
public:
	// Construction
	CCommandLine();
	virtual ~CCommandLine();

	void CreateCmdLine( const char* commandline ) override;
#if !defined( _WIN32 )
	void CreateCmdLine( int argc, const char** argv ) override;
#endif
	const char* GetCmdLine() override;
	char* CheckParm( const char* psz, char** ppszValue ) override;

	void RemoveParm( const char* pszParm ) override;
	void AppendParm( const char* pszParm, const char* pszValues ) override;
	void SetParm( const char* pszParm, const char* pszValues ) override;
	void SetParm( const char* pszParm, int iValue ) override;

private:
	char* m_pszCmdLine;
	std::string m_ParmValue;
};

static CCommandLine g_CmdLine;
ICommandLine* cmdline = &g_CmdLine;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommandLine::CCommandLine()
{
	m_pszCmdLine = NULL;
}

CCommandLine::~CCommandLine()
{
	delete[] m_pszCmdLine;
}

#if !defined( _WIN32 )
void CCommandLine::CreateCmdLine( int argc, const char** argv )
{
	std::string full;
	for ( int i = 0; i < argc; ++i )
	{
		if ( i )
			full += ' ';
		const char* argument = argv[i] ? argv[i] : "";
		const bool quoted = !*argument || strpbrk( argument, " \t\r\n\v\f" );
		if ( quoted )
			full += '"';
		full += argument;
		if ( quoted )
			full += '"';
	}
	CreateCmdLine( full.c_str() );
}
#endif

void CCommandLine::CreateCmdLine( const char* commandline )
{
	char szFull[COMMAND_LINE_SIZE];
	const std::string input( commandline ? commandline : "" );
	const char* pSrc = input.c_str();
	char* pDst = szFull;
	bool allowAtSign = true;

#if defined( _MSC_VER )
#pragma warning( suppress: 6011 )
#endif
	while ( *pSrc && pDst - szFull < sizeof( szFull ) - 1 )
	{
		if ( allowAtSign && *pSrc == '@' )
		{
#if defined( _WIN32 )
			char szFileName[MAX_PATH];
			size_t len = strcspn( ++pSrc, "" );
#else
			char szFileName[COMMAND_LINE_SIZE];
			size_t len = strlen( ++pSrc );
#endif
			if ( len > sizeof( szFileName ) - 2 )
			{
				printf( "Parameter filename too long, skipping\n" );
				pSrc += len;
				if ( *pSrc )
					++pSrc;
				continue;
			}

			memcpy( szFileName, pSrc, len );
			szFileName[len] = '\0';
			// Skip the space after the file name
			if ( *pSrc )
				++pSrc;

			// Now read in parameters from file
			FILE* fp = fopen( szFileName, "r" );
			if ( !fp )
			{
				printf( "Parameter file '%s' not found, skipping...", szFileName );
				continue;
			}

			char c = (char)fgetc( fp );
			while ( c != EOF && pDst - szFull < sizeof( szFull ) - 2 )
			{
				*pDst++ = c == '\n' ? ' ' : c;
				// Get the next character, if there are more
				c = (char)fgetc( fp );
			}

			// Add a terminating space character
			*pDst++ = ' ';
			fclose( fp );
		}
		else
		{
			allowAtSign = isspace( (signed char)*pSrc ) != 0;
			*pDst++ = *pSrc++;
		}
	}

	*pDst = '\0';
	char* replacement = new char[strlen( szFull ) + 1];
	strcpy( replacement, szFull );
	delete[] m_pszCmdLine;
	m_pszCmdLine = replacement;
}

//-----------------------------------------------------------------------------
// Purpose: Remove specified string ( and any args attached to it ) from command line
// Input  : *pszParm - 
//-----------------------------------------------------------------------------
void CCommandLine::RemoveParm( const char* pszParm )
{
	if ( !m_pszCmdLine || !pszParm || !*pszParm )
		return;

	const std::string parm( pszParm );
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
void CCommandLine::AppendParm( const char* pszParm, const char* pszValues )
{
	int nNewLength = (int)strlen( pszParm );
	if ( pszValues )
		nNewLength += (int)strlen( pszValues ) + 1; // Values + leading space character.
	nNewLength++; // Terminal 0;

	if ( !m_pszCmdLine )
	{
		m_pszCmdLine = new char[nNewLength];
		strcpy( m_pszCmdLine, pszParm );
		if ( pszValues )
		{
			strcat( m_pszCmdLine, " " );
			strcat( m_pszCmdLine, pszValues );
		}
		return;
	}

	// Remove any remnants from the current Cmd Line.
	RemoveParm( pszParm );
	nNewLength += (int)strlen( m_pszCmdLine ) + 2;
	char* pCmdString = new char[nNewLength];
	memset( pCmdString, 0, nNewLength );
	strcpy( pCmdString, m_pszCmdLine ); // Copy old command line.
	strcat( pCmdString, " " ); // Put in a space
	strcat( pCmdString, pszParm );
	if ( pszValues )
	{
		strcat( pCmdString, " " );
		strcat( pCmdString, pszValues );
	}

	delete[] m_pszCmdLine;
	m_pszCmdLine = pCmdString;
}

void CCommandLine::SetParm( const char* pszParm, const char* pszValues )
{
	RemoveParm( pszParm );
	AppendParm( pszParm, pszValues );
}

void CCommandLine::SetParm( const char* pszParm, int iValue )
{
	char buf[64];
	_snprintf( buf, sizeof( buf ), "%d", iValue );
	SetParm( pszParm, buf );
}

//-----------------------------------------------------------------------------
// Purpose: Return current command line
// Output : const char
//-----------------------------------------------------------------------------
const char* CCommandLine::GetCmdLine()
{
	return m_pszCmdLine;
}

char* CCommandLine::CheckParm( const char* psz, char** ppszValue )
{
	if ( ppszValue )
		*ppszValue = NULL;
	if ( !m_pszCmdLine || !psz || !*psz )
		return NULL;
	const char* cursor = m_pszCmdLine;
	while ( *cursor )
	{
		const auto token = ReadToken( cursor );
		cursor = token.end;
		if ( !token.Matches( psz ) )
			continue;
		if ( ppszValue )
		{
			const auto value = ReadToken( cursor );
			if ( value.begin != value.end && !value.IsParameter() )
			{
				m_ParmValue = value.value;
				*ppszValue = &m_ParmValue[0];
			}
		}
		return m_pszCmdLine + ( token.begin - m_pszCmdLine );
	}
	return NULL;
}
