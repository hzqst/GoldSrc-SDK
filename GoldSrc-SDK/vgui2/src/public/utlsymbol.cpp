//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Defines a symbol table
//
// $Header: $
// $NoKeywords: $
//=============================================================================

#include "utlsymbol.h"

#include "platform.h"

#include <string.h>

#if defined( _WIN32 ) && ( !defined( NDEBUG ) || defined( UTL_ENGINE_ASSERTS ) )
#define UTLSYMBOL_ASSERTS
#define UTLSYMBOL_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\utlsymbol.cpp"
#endif

#define INVALID_STRING_INDEX	((unsigned int)0xFFFFFFFF)

//-----------------------------------------------------------------------------
// symbol table stuff
//-----------------------------------------------------------------------------

struct LessCtx_t
{
	const char*			m_pUserString;
	CUtlSymbolTable*	m_pTable;

	LessCtx_t() : m_pUserString( NULL ), m_pTable( NULL )
	{
	}
};

static LessCtx_t g_LessCtx;

CUtlSymbolTable* CUtlSymbol::s_pSymbolTable = NULL;

void CUtlSymbol::Initialize()
{
	// necessary to allow us to create global symbols
	static bool symbolsInitialized = false;

	if ( !symbolsInitialized )
	{
		s_pSymbolTable = new CUtlSymbolTable;
		symbolsInitialized = true;
	}
}

CUtlSymbolTable* CUtlSymbol::CurrTable()
{
	Initialize();
	return s_pSymbolTable;
}

const char* CUtlSymbol::String() const
{
	return CurrTable()->String( m_Id );
}

bool CUtlSymbolTable::SymLess( const unsigned int& i1, const unsigned int& i2 )
{
	const char* str1;
	const char* str2;

	if ( i1 == INVALID_STRING_INDEX )
	{
		str1 = g_LessCtx.m_pUserString;
	}
	else
	{
		str1 = &g_LessCtx.m_pTable->m_Strings[i1];
	}

	if ( i2 == INVALID_STRING_INDEX )
	{
		str2 = g_LessCtx.m_pUserString;
	}
	else
	{
		str2 = &g_LessCtx.m_pTable->m_Strings[i2];
	}

	return strcmp( str1, str2 ) < 0;
}

bool CUtlSymbolTable::SymLessi( const unsigned int& i1, const unsigned int& i2 )
{
	const char* str1;
	const char* str2;

	if ( i1 == INVALID_STRING_INDEX )
	{
		str1 = g_LessCtx.m_pUserString;
	}
	else
	{
		str1 = &g_LessCtx.m_pTable->m_Strings[i1];
	}

	if ( i2 == INVALID_STRING_INDEX )
	{
		str2 = g_LessCtx.m_pUserString;
	}
	else
	{
		str2 = &g_LessCtx.m_pTable->m_Strings[i2];
	}

	return strcasecmp( str1, str2 ) < 0;
}

//-----------------------------------------------------------------------------
// string->symbol->string
//-----------------------------------------------------------------------------

CUtlSymbol::CUtlSymbol( const char* pStr )
{
	m_Id = CurrTable()->AddString( pStr );
}

//-----------------------------------------------------------------------------
// checks if the symbol matches a string
//-----------------------------------------------------------------------------

bool CUtlSymbol::operator==( const char* pStr ) const
{
	if ( !IsValid() )
	{
		return false;
	}

	return strcmp( String(), pStr ) == 0;
}

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

CUtlSymbolTable::CUtlSymbolTable( int growSize, int initSize, bool caseInsensitive )
	: m_Lookup( growSize, initSize, caseInsensitive ? &CUtlSymbolTable::SymLessi : &CUtlSymbolTable::SymLess ),
	  m_Strings( 256 )
{
}

CUtlSymbolTable::~CUtlSymbolTable()
{
}

CUtlSymbol CUtlSymbolTable::Find( const char* pString )
{
	if ( !pString )
	{
		return CUtlSymbol();
	}

	// Store a special context used to help with insertion
	g_LessCtx.m_pUserString = pString;
	g_LessCtx.m_pTable = this;
	return CUtlSymbol( m_Lookup.Find( INVALID_STRING_INDEX ) );
}

//-----------------------------------------------------------------------------
// Finds and/or creates a symbol based on the string
//-----------------------------------------------------------------------------

CUtlSymbol CUtlSymbolTable::AddString( const char* pString )
{
	if ( !pString )
	{
		return CUtlSymbol();
	}

	CUtlSymbol symbol = Find( pString );

	if ( symbol.IsValid() )
	{
		return symbol;
	}

	int length = (int)strlen( pString ) + 1;
	unsigned int stringIndex = (unsigned int)m_Strings.AddMultipleToTail( length );
	memcpy( &m_Strings[stringIndex], pString, length );
	return CUtlSymbol( m_Lookup.Insert( stringIndex ) );
}

const char* CUtlSymbolTable::String( CUtlSymbol id ) const
{
	if ( !id.IsValid() )
	{
		return "";
	}

#ifdef UTLSYMBOL_ASSERTS
	if ( !m_Lookup.IsValidIndex( (UtlSymId_t)id ) )
	{
		_wassert( L"m_Lookup.IsValidIndex((UtlSymId_t)id)", UTLSYMBOL_SOURCE_PATH, 178 );
	}
#endif

	return &m_Strings[m_Lookup[(UtlSymId_t)id]];
}

//-----------------------------------------------------------------------------
// Remove all symbols in the table.
//-----------------------------------------------------------------------------

void CUtlSymbolTable::RemoveAll()
{
	m_Lookup.RemoveAll();
	m_Strings.RemoveAll();
}
