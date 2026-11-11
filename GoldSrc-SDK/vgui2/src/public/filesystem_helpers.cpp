//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "filesystem_helpers.h"

#include "../../../public/characterset.h"

#include <stddef.h>

// wordbreak parsing set
static characterset_t g_BreakSet, g_BreakSetIncludingColons;

static void InitializeCharacterSets()
{
	static bool s_CharacterSetInitialized = false;

	if ( !s_CharacterSetInitialized )
	{
		CharacterSetBuild( &g_BreakSet, "{}()'" );
		CharacterSetBuild( &g_BreakSetIncludingColons, "{}()':" );
		s_CharacterSetInitialized = true;
	}
}

const char* ParseFile( const char* pFileBytes, char* pToken, bool* pWasQuoted, characterset_t* charset )
{
	if ( pWasQuoted )
	{
		*pWasQuoted = false;
	}

	if ( !pFileBytes )
	{
		return NULL;
	}

	InitializeCharacterSets();

	static const bool com_ignorecolons = false;
	characterset_t& breaks = charset ? *charset : ( com_ignorecolons ? g_BreakSet : g_BreakSetIncludingColons );

	int c;
	int len = 0;
	pToken[0] = 0;

// skip whitespace
skipwhite:

	while ( ( c = *pFileBytes ) <= ' ' )
	{
		if ( c == 0 )
		{
			return NULL;
		}

		pFileBytes++;
	}

// skip // comments
	if ( c == '/' && pFileBytes[1] == '/' )
	{
		while ( *pFileBytes && *pFileBytes != '\n' )
		{
			pFileBytes++;
		}

		goto skipwhite;
	}

// skip c-style comments
	if ( c == '/' && pFileBytes[1] == '*' )
	{
		// Skip "/*"
		pFileBytes += 2;

		while ( *pFileBytes )
		{
			if ( *pFileBytes == '*' && pFileBytes[1] == '/' )
			{
				pFileBytes += 2;
				break;
			}

			pFileBytes++;
		}

		goto skipwhite;
	}

// handle quoted strings specially
	if ( c == '\"' )
	{
		if ( pWasQuoted )
		{
			*pWasQuoted = true;
		}

		pFileBytes++;

		while ( 1 )
		{
			c = *pFileBytes++;

			if ( c == '\"' || !c )
			{
				pToken[len] = 0;
				return pFileBytes;
			}

			pToken[len] = (char)c;
			len++;
		}
	}

	if ( breaks.set[c] )
	{
		pToken[len] = (char)c;
		len++;
		pToken[len] = 0;
		return pFileBytes + 1;
	}

// parse a regular word
	do
	{
		pToken[len] = (char)c;
		pFileBytes++;
		len++;
		c = *pFileBytes;

		if ( breaks.set[c] )
		{
			break;
		}
	} while ( c > 32 );

	pToken[len] = 0;
	return pFileBytes;
}

char* ParseFile( char* pFileBytes, char* pToken, bool* pWasQuoted )
{
	return (char*)ParseFile( pFileBytes, pToken, pWasQuoted, NULL );
}
