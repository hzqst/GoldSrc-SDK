//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "TokenReader.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

TokenReader::TokenReader() : m_nLine( 1 ), m_nErrors( 0 ), m_bStuffed( false ), m_eStuffedType( TOKENNONE )
{
	m_szFilename[0] = 0;
	m_szToken[0] = 0;
}

bool TokenReader::Open( const char* filename )
{
	Close();
	m_File.clear();
	m_File.open( filename, std::ios::in | std::ios::binary );
	strncpy( m_szFilename, filename, sizeof( m_szFilename ) - 1 );
	m_szFilename[sizeof( m_szFilename ) - 1] = 0;
	m_nLine = 1;
	m_nErrors = 0;
	m_bStuffed = false;
	return m_File.is_open();
}

void TokenReader::Close()
{
	if ( m_File.is_open() )
		m_File.close();
	m_bStuffed = false;
}

trtoken_t TokenReader::NextToken( char* token, int size )
{
	if ( !token || size <= 0 )
		return TOKENSTRINGTOOLONG;
	token[0] = 0;
	if ( !m_File.is_open() )
		return TOKENEOF;
	//
	// If they stuffed a token, return that token.
	//
	if ( m_bStuffed )
	{
		m_bStuffed = false;
		strncpy( token, m_szToken, size - 1 );
		token[size - 1] = 0;
		return (int)strlen( m_szToken ) >= size ? TOKENSTRINGTOOLONG : m_eStuffedType;
	}

	int c;
	for ( ;; )
	{
		c = m_File.get();
		if ( c == EOF )
			return TOKENEOF;
		if ( c == '\n' )
		{
			m_nLine++;
			continue;
		}
		if ( c == ' ' || c == '\t' || c == '\r' )
			continue;
		if ( c == '/' && m_File.peek() == '/' )
		{
			m_File.ignore( 512, '\n' );
			m_nLine++;
			continue;
		}
		break;
	}

	if ( c && strchr( "!$&()*,.:=@[]", c ) )
	{
		if ( size < 2 )
			return TOKENSTRINGTOOLONG;
		token[0] = (char)c;
		token[1] = 0;
		return OPERATOR;
	}
	if ( c == '"' )
	{
		if ( m_File.peek() == '"' )
		{
			m_File.get();
			return STRING;
		}
		m_File.get( token, size, '"' );
		return m_File.get() == '"' ? STRING : TOKENSTRINGTOOLONG;
	}

	int	 length = 0;
	bool number = isdigit( (unsigned char)c ) || c == '-';
	while ( number ? isdigit( (unsigned char)c ) || c == '-'
				   : isalpha( (unsigned char)c ) || isdigit( (unsigned char)c ) || c == '_' )
	{
		if ( length >= size - 1 )
		{
			token[length] = 0;
			return TOKENSTRINGTOOLONG;
		}
		token[length++] = (char)c;
		c = m_File.get();
		if ( number && ( c == '-' || ( c != EOF && ( isalpha( (unsigned char)c ) || c == '_' ) ) ) )
		{
			token[length] = 0;
			return TOKENSTRINGTOOLONG;
		}
		if ( c == EOF )
			break;
	}
	if ( c != EOF )
		m_File.putback( (char)c );
	token[length] = 0;
	return number ? INTEGER : IDENT;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the next token, allocating enough memory to store the token
//			plus a terminating NULL.
// Input  : pszStore - Pointer to a string that will be allocated.
// Output : Returns the type of token that was read, or TOKENERROR.
//-----------------------------------------------------------------------------
trtoken_t TokenReader::NextTokenDynamic( char** token )
{
	char	  buffer[MAX_TOKEN];
	trtoken_t type = NextToken( buffer, sizeof( buffer ) );
	*token = new char[strlen( buffer ) + 1];
	strcpy( *token, buffer );
	return type;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pszStore - 
// Output : 
//-----------------------------------------------------------------------------
trtoken_t TokenReader::PeekTokenType( char* token )
{
	if ( !m_bStuffed )
	{
		m_eStuffedType = NextToken( m_szToken, sizeof( m_szToken ) );
		m_bStuffed = true;
		if ( token )
			strcpy( token, m_szToken );
	}
	return m_eStuffedType;
}

void TokenReader::Stuff( trtoken_t type, const char* token )
{
	strncpy( m_szToken, token, sizeof( m_szToken ) - 1 );
	m_szToken[sizeof( m_szToken ) - 1] = 0;
	m_eStuffedType = type;
	m_bStuffed = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : ttype - 
//			*pszToken - 
//-----------------------------------------------------------------------------
void TokenReader::IgnoreTill( trtoken_t type, const char* token )
{
	char buffer[MAX_TOKEN];
	for ( ;; )
	{
		trtoken_t next = NextToken( buffer, sizeof( buffer ) );
		if ( next == TOKENEOF )
			return;
		if ( next == type && !_stricmp( token, buffer ) )
		{
			Stuff( type, token );
			return;
		}
	}
}

const char* TokenReader::Error( const char* message )
{
	static char text[512];
	snprintf( text, sizeof( text ), "error parsing %s, line %d: %s", m_szFilename, m_nLine, message );
	m_nErrors++;
	return text;
}
