//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "LocalizedStringTable.h"

#include "platform.h"
#include "KeyValues.h"
#include "UnicodeFileHelpers.h"
#include "filesystem.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui_internal.h"

#ifndef _WIN32
#include <iconv.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );

#define LOCALIZEDSTRINGTABLE_SOURCE_PATH \
	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\vgui2\\src\\LocalizedStringTable.cpp"
#define snprintf _snprintf
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

using namespace vgui2;

namespace
{

// helpers in sorting strings
struct LessCtx_t
{
	const char*				m_pUserString;
	CLocalizedStringTable*	m_pTable;

	LessCtx_t() : m_pUserString( NULL ), m_pTable( NULL )
	{
	}
};

}

CLocalizedStringTable* CLocalizedStringTable::s_pTable = NULL;

// global instance of table
CLocalizedStringTable g_StringTable;
// expose the interface
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CLocalizedStringTable, ILocalize, "VGUI_Localize003", g_StringTable );

CLocalizedStringTable_VGUI_Localize002 g_StringTable_Localize002;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CLocalizedStringTable_VGUI_Localize002,
	CLocalizedStringTable_VGUI_Localize002, "VGUI_Localize002", g_StringTable_Localize002 );

static LessCtx_t g_LessCtx;

char* Q_strncat( char* pDest, const char* pSrc, size_t destBufferSize, int max_chars_to_copy )
{
	size_t destLen;
	int srcLen;
	size_t n;

	destLen = strlen( pDest );
	srcLen = (int)strlen( pSrc );
	n = (size_t)srcLen;

	if ( max_chars_to_copy >= 0 && srcLen > max_chars_to_copy )
	{
		n = (size_t)max_chars_to_copy;
	}

	if ( n + destLen >= destBufferSize )
	{
		n = destBufferSize - 1 - destLen;
	}

	if ( n != 0 )
	{
		strncat( pDest, pSrc, n );
		pDest[destBufferSize - 1] = 0;
	}

	return pDest;
}

int Q_UCS2ToUTF8( const ucs2* pUCS2, int cubSrcInBytes, char* pUTF8, int cubDestSizeInBytes )
{
#ifdef _WIN32
	int result;

	UNREFERENCED_PARAMETER( cubSrcInBytes );

	*pUTF8 = 0;
	result = WideCharToMultiByte( CP_UTF8, 0, (const wchar_t*)pUCS2, -1, pUTF8, cubDestSizeInBytes, NULL, NULL );
	pUTF8[cubDestSizeInBytes - 1] = 0;
	return result;
#else
	char* pIn;
	char* pOut;
	size_t nLenUnicode;
	size_t nMaxUTF8;
	iconv_t cd;
	int result;

	*pUTF8 = 0;
	nMaxUTF8 = (size_t)cubDestSizeInBytes;
	pOut = pUTF8;
	cd = iconv_open( "UTF-8", "UCS-2LE" );
	nLenUnicode = (size_t)cubSrcInBytes;
	pIn = (char*)pUCS2;

	if ( cd != NULL )
	{
		result = (int)iconv( cd, &pIn, &nLenUnicode, &pOut, &nMaxUTF8 );
		iconv_close( cd );
	}
	else
	{
		result = -1;
	}

	pUTF8[cubDestSizeInBytes - 1] = 0;
	return result;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Used to sort strings
//-----------------------------------------------------------------------------
bool CLocalizedStringTable::SymLess( const localizedstring_t& i1, const localizedstring_t& i2 )
{
	const char* s1;
	const char* s2;

	if ( i1.nameIndex == INVALID_LOCALIZE_STRING_INDEX )
	{
		s1 = g_LessCtx.m_pUserString;
	}
	else
	{
		s1 = &g_LessCtx.m_pTable->m_Names[i1.nameIndex];
	}

	if ( i2.nameIndex == INVALID_LOCALIZE_STRING_INDEX )
	{
		s2 = g_LessCtx.m_pUserString;
	}
	else
	{
		s2 = &g_LessCtx.m_pTable->m_Names[i2.nameIndex];
	}

	return strcasecmp( s1, s2 ) < 0;
}

bool CLocalizedStringTable::FastValueLessFunc( const fastvalue_t& lhs, const fastvalue_t& rhs )
{
	const wchar_t* s1;
	const wchar_t* s2;

#ifdef _WIN32
	if ( !s_pTable )
	{
		_wassert( L"s_pTable", LOCALIZEDSTRINGTABLE_SOURCE_PATH, 907 );
	}
#endif

	s1 = lhs.search ? lhs.search : &s_pTable->m_Values[lhs.valueindex];
	s2 = rhs.search ? rhs.search : &s_pTable->m_Values[rhs.valueindex];
	return wcscmp( s1, s2 ) < 0;
}

CLocalizedStringTable::CLocalizedStringTable() :
	m_Lookup( 0, 0, SymLess ),
	m_Names( 1024 ),
	m_Values( 2048 ),
	m_FastValueLookup( 0, 0, FastValueLessFunc )
{
	memset( m_szLanguage, 0, sizeof( m_szLanguage ) );
	m_bUseOnlyLongestLanguageString = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLocalizedStringTable::~CLocalizedStringTable()
{
	m_Names.Purge();
	m_Values.Purge();
	m_LocalizationFiles.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Finds a string in the table
//-----------------------------------------------------------------------------
wchar_t* CLocalizedStringTable::Find( const char* pName )
{
	StringIndex_t idx;

	idx = FindIndex( pName );
	if ( idx == INVALID_LOCALIZE_STRING_INDEX )
	{
		return NULL;
	}

	return &m_Values[m_Lookup[idx].valueIndex];
}

//-----------------------------------------------------------------------------
// Purpose: gets the name of the localization string by index
//-----------------------------------------------------------------------------
const char* CLocalizedStringTable::GetNameByIndex( StringIndex_t index )
{
	return &m_Names[m_Lookup[index].nameIndex];
}

wchar_t* CLocalizedStringTable::GetValueByIndex( StringIndex_t index )
{
	if ( index == INVALID_LOCALIZE_STRING_INDEX )
	{
		return NULL;
	}

	return &m_Values[m_Lookup[index].valueIndex];
}

//-----------------------------------------------------------------------------
// Purpose: returns number of localization files currently loaded
//-----------------------------------------------------------------------------
int CLocalizedStringTable::GetLocalizationFileCount()
{
	return m_LocalizationFiles.Count();
}

void CLocalizedStringTable::ReloadLocalizationFiles( IFileSystem* filesystem )
{
	int i;

	for ( i = 0; i < m_LocalizationFiles.Count(); i++ )
	{
		AddFile( filesystem, m_LocalizationFiles[i].String() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns localization filename by index
//-----------------------------------------------------------------------------
const char* CLocalizedStringTable::GetLocalizationFileName( int index )
{
	return m_LocalizationFiles[index].String();
}

//-----------------------------------------------------------------------------
// Purpose: returns which file a string was loaded from
//-----------------------------------------------------------------------------
const char* CLocalizedStringTable::GetFileNameByIndex( StringIndex_t index )
{
	return m_Lookup[index].filename.String();
}

//-----------------------------------------------------------------------------
// Purpose: sets the value in the index
//-----------------------------------------------------------------------------
void CLocalizedStringTable::SetValueByIndex( StringIndex_t index, wchar_t* newValue )
{
	// get the existing string
	localizedstring_t& lstr = m_Lookup[index];
	wchar_t* wstr = &m_Values[lstr.valueIndex];
	int newLen = (int)wcslen( newValue );
	int oldLen = (int)wcslen( wstr );

	if ( newLen > oldLen )
	{
		// it won't fit, so allocate new memory - this is wasteful, but only happens in edit mode
		lstr.valueIndex = m_Values.AddMultipleToTail( newLen + 1 );
		memcpy( &m_Values[lstr.valueIndex], newValue, ( newLen + 1 ) * sizeof( wchar_t ) );
	}
	else
	{
		// copy the string into the old position
		wcscpy( wstr, newValue );
	}
}

void CLocalizedStringTable::ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
	const char* tokenName, KeyValues* localizationVariables )
{
	StringIndex_t index;

	index = FindIndex( tokenName );
	if ( index == INVALID_LOCALIZE_STRING_INDEX )
	{
		ConvertANSIToUnicode( tokenName, unicodeOutput, unicodeBufferSizeInBytes );
	}
	else
	{
		ConstructString( unicodeOutput, unicodeBufferSizeInBytes, index, localizationVariables );
	}
}

void CLocalizedStringTable::ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
	StringIndex_t unlocalizedTextSymbol, KeyValues* localizationVariables )
{
	const wchar_t* searchPos;
	wchar_t* outputPos;
	int remaining;
	wchar_t ch;

	if ( unicodeBufferSizeInBytes <= 0 )
	{
		return;
	}

	unicodeOutput[0] = 0;
	searchPos = GetValueByIndex( unlocalizedTextSymbol );
	if ( !searchPos )
	{
		wcsncpy( unicodeOutput, L"[unknown string]", (unsigned int)unicodeBufferSizeInBytes / sizeof( wchar_t ) );
		return;
	}

	remaining = (int)( (unsigned int)unicodeBufferSizeInBytes / sizeof( wchar_t ) );
	outputPos = unicodeOutput;

	while ( 1 )
	{
		ch = *searchPos;
		if ( remaining <= 0 || ch == 0 )
		{
			*outputPos = 0;
			return;
		}

		if ( ch == L'%' )
		{
			for ( ;; )
			{
				wchar_t next = searchPos[1];

				if ( next == L's' && (unsigned int)( searchPos[2] - L'0' ) <= 9 )
				{
					++searchPos;
					break;
				}

				if ( next == L'%' )
				{
					searchPos += 2;
					ch = L'%';
					break;
				}

				if ( !localizationVariables )
				{
					++searchPos;
					break;
				}

				++searchPos;
				{
					const wchar_t* tokenEnd = wcschr( searchPos, L'%' );
					char variableName[32];
					char* nameWrite;
					const wchar_t* replacement;
					int copyCount;

					if ( !tokenEnd || *tokenEnd != L'%' )
					{
						break;
					}

					nameWrite = variableName;
					while ( searchPos < tokenEnd && nameWrite < variableName + 31 )
					{
						*nameWrite++ = (char)*searchPos++;
					}

					*nameWrite = 0;

					replacement = localizationVariables->GetWString( variableName, L"[unknown]" );
					copyCount = (int)wcslen( replacement );
					if ( copyCount > remaining )
					{
						copyCount = remaining;
					}

					remaining -= copyCount;
					wcsncpy( outputPos, replacement, copyCount );
					outputPos += copyCount;

					searchPos = tokenEnd + 1;
					ch = *searchPos;
					if ( remaining <= 0 || ch == 0 )
					{
						*outputPos = 0;
						return;
					}

					if ( ch != L'%' )
					{
						searchPos = tokenEnd + 2;
						break;
					}
				}
			}
		}
		else
		{
			++searchPos;
		}

		--remaining;
		*outputPos++ = ch;
	}
}

//-----------------------------------------------------------------------------
// Purpose: iteration functions
//-----------------------------------------------------------------------------
StringIndex_t CLocalizedStringTable::GetFirstStringIndex()
{
	return m_Lookup.FirstInorder();
}

//-----------------------------------------------------------------------------
// Remove all symbols in the table.
//-----------------------------------------------------------------------------
void CLocalizedStringTable::RemoveAll()
{
	m_Lookup.RemoveAll();
	m_Names.RemoveAll();
	m_Values.RemoveAll();
	m_LocalizationFiles.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: finds the index of a token by token name
//-----------------------------------------------------------------------------
StringIndex_t CLocalizedStringTable::FindIndex( const char* pName )
{
	localizedstring_t invalidItem;

	if ( !pName )
	{
		return 0;
	}

	// strip the pound character (which is used elsewhere to indicate that it's a string that should be translated)
	if ( pName[0] == '#' )
	{
		pName++;
	}

	g_LessCtx.m_pTable = this;
	g_LessCtx.m_pUserString = pName;

	invalidItem.nameIndex = INVALID_LOCALIZE_STRING_INDEX;
	invalidItem.valueIndex = INVALID_LOCALIZE_STRING_INDEX;
	invalidItem.filename = CUtlSymbol();

	return m_Lookup.Find( invalidItem );
}

StringIndex_t CLocalizedStringTable::GetNextStringIndex( StringIndex_t index )
{
	return m_Lookup.NextInorder( index );
}

int CLocalizedStringTable::ConvertANSIToUnicode( const char* ansi, wchar_t* unicode,
	int unicodeBufferSizeInBytes )
{
#ifdef _WIN32
	int result;

	result = MultiByteToWideChar( CP_UTF8, 0, ansi, -1, unicode, unicodeBufferSizeInBytes / sizeof( wchar_t ) );
	unicode[unicodeBufferSizeInBytes / sizeof( wchar_t ) - 1] = 0;
	return result;
#else
	unsigned int maxChars;
	int result;

	if ( !ansi || !unicode )
	{
		return -1;
	}

	maxChars = (unsigned int)unicodeBufferSizeInBytes >> 2;
	result = (int)mbstowcs( unicode, ansi, maxChars );
	unicode[maxChars - 1] = 0;
	return result;
#endif
}

int CLocalizedStringTable::ConvertUnicodeToANSI( const wchar_t* unicode, char* ansi, int ansiBufferSize )
{
#ifdef _WIN32
	int result;

	result = WideCharToMultiByte( CP_UTF8, 0, unicode, -1, ansi, ansiBufferSize, NULL, NULL );
	ansi[ansiBufferSize - 1] = 0;
	return result;
#else
	int result;

	if ( !ansi || !unicode )
	{
		return -1;
	}

	result = (int)wcstombs( ansi, unicode, ansiBufferSize );
	ansi[ansiBufferSize - 1] = 0;
	return result;
#endif
}

static const char* s_LanguageToken = "%language%";
static const char* s_English = "english";

static void ConstructStringFromArgs( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
	wchar_t* formatString, int numFormatParameters, va_list argList )
{
	int unicodeBufferSize;
	wchar_t* searchPos;
	wchar_t* outputPos;
	int formatLength;

	unicodeBufferSize = (int)( (unsigned int)unicodeBufferSizeInBytes / sizeof( wchar_t ) );
	searchPos = formatString;
	outputPos = unicodeOutput;
	formatLength = (int)wcslen( formatString );

	while ( unicodeBufferSize > 1 )
	{
		wchar_t ch = *searchPos;

		if ( ch == 0 )
		{
			break;
		}

		if ( ch == L'%' && formatLength > 2 && searchPos[1] == L's' )
		{
			int argindex = (int)searchPos[2] - (int)L'1';

			if ( (unsigned int)argindex <= 8 )
			{
				if ( argindex >= numFormatParameters )
				{
					*outputPos = L'%';
					--unicodeBufferSize;
					++outputPos;
					++searchPos;
					--formatLength;
				}
				else
				{
					wchar_t* param = ( (wchar_t**)argList )[argindex];
					int paramSize;

					if ( !param )
					{
#ifdef _WIN32
						_wassert( L"!(\"CLocalizedStringTable::ConstructString - Found a %s# escape sequence "
							L"who's index was more than the number of args.\")",
							LOCALIZEDSTRINGTABLE_SOURCE_PATH, 1095 );
#else
						*outputPos = 0;
						return;
#endif
					}

					paramSize = (int)wcslen( param );
					if ( unicodeBufferSize < paramSize )
					{
						paramSize = unicodeBufferSize - 1;
					}

					searchPos += 3;
					formatLength -= 3;
					wcsncpy( outputPos, param, paramSize );
					unicodeBufferSize -= paramSize;
					outputPos += paramSize;
				}

				continue;
			}
		}

		*outputPos = ch;
		--unicodeBufferSize;
		++outputPos;
		++searchPos;
		--formatLength;
	}

	*outputPos = 0;
}

bool CLocalizedStringTable::AddAllLanguageFiles( IFileSystem* fileSystem, const char* baseFileName )
{
	char szFilePath[MAX_PATH];
	char szSearchPath[MAX_PATH];
	char szFile[MAX_PATH];
	char* lastSlash;
	FileFindHandle_t hFind;
	const char* file;
	bool success;

	strncpy( szFilePath, baseFileName, sizeof( szFilePath ) );
	lastSlash = strrchr( szFilePath, '\\' );
	if ( !lastSlash )
	{
		lastSlash = strrchr( szFilePath, '/' );
	}

	if ( lastSlash )
	{
		lastSlash[1] = 0;
	}
	else
	{
		szFilePath[0] = 0;
	}

	snprintf( szSearchPath, sizeof( szSearchPath ), "%s*.txt", baseFileName );

	hFind = 0;
	file = fileSystem->FindFirst( szSearchPath, &hFind, NULL );
	success = true;

	while ( file )
	{
		bool added;

		snprintf( szFile, sizeof( szFile ), "%s%s", szFilePath, file );
		added = AddFile( fileSystem, szFile );
		success = success & added;
		file = fileSystem->FindNext( hFind );
	}

	fileSystem->FindClose( hFind );
	return success;
}

void CLocalizedStringTable::DiscardFastValueLookup()
{
	m_FastValueLookup.RemoveAll();
	s_pTable = NULL;
}

int CLocalizedStringTable::FindExistingValueIndex( const wchar_t* value )
{
	fastvalue_t val;
	int idx;

	if ( !s_pTable )
	{
		return -1;
	}

	val.valueindex = -1;
	val.search = value;
	idx = m_FastValueLookup.Find( val );
	if ( idx != -1 )
	{
		return m_FastValueLookup[idx].valueindex;
	}

	return -1;
}

void CLocalizedStringTable::ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
	wchar_t* formatString, int numFormatParameters, va_list argList )
{
	ConstructStringFromArgs( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
}

void CLocalizedStringTable::ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
	wchar_t* formatString, int numFormatParameters, ... )
{
	va_list argList;

	va_start( argList, numFormatParameters );

	if ( formatString )
	{
		ConstructString( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
	}
	else
	{
		*unicodeOutput = 0;
	}

	va_end( argList );
}

//-----------------------------------------------------------------------------
// Purpose: saves the entire contents of the token tree to the file
//-----------------------------------------------------------------------------
bool CLocalizedStringTable::SaveToFile( IFileSystem* fileSystem, const char* szFileName )
{
	static wchar_t unicodeString[1024];
	static const char* startStr = "\"lang\"\r\n{\r\n\"Language\" \"English\"\r\n\"Tokens\"\r\n{\r\n";
	static const char* endStr = "}\r\n}\r\n";
	FileHandle_t file;
	CUtlSymbol fileSymbol;
	unsigned short marker;
	wchar_t unicodeQuote;
	wchar_t unicodeCR;
	wchar_t unicodeNewline;
	wchar_t unicodeTab;
	StringIndex_t idx;
	int strLength;

	file = fileSystem->Open( szFileName, "wb", NULL );
	if ( !file )
	{
		return false;
	}

	fileSymbol = CUtlSymbol( szFileName );
	marker = 0xFEFF;
	fileSystem->Write( &marker, sizeof( marker ), file );

	strLength = ConvertANSIToUnicode( startStr, unicodeString, (int)sizeof( unicodeString ) );
	if ( !strLength )
	{
		return false;
	}

	fileSystem->Write( unicodeString, wcslen( unicodeString ) * sizeof( wchar_t ), file );

	unicodeQuote = L'\"';
	unicodeCR = L'\r';
	unicodeNewline = L'\n';
	unicodeTab = L'\t';

	// write out all the key/value pairs
	for ( idx = GetFirstStringIndex(); idx != INVALID_LOCALIZE_STRING_INDEX; idx = GetNextStringIndex( idx ) )
	{
		const char* name;
		wchar_t* value;

		if ( !( fileSymbol == m_Lookup[idx].filename ) )
		{
			continue;
		}

		name = GetNameByIndex( idx );
		value = GetValueByIndex( idx );
		ConvertANSIToUnicode( name, unicodeString, (int)sizeof( unicodeString ) );

		fileSystem->Write( &unicodeTab, sizeof( wchar_t ), file );
		fileSystem->Write( &unicodeQuote, sizeof( wchar_t ), file );
		fileSystem->Write( unicodeString, wcslen( unicodeString ) * sizeof( wchar_t ), file );
		fileSystem->Write( &unicodeQuote, sizeof( wchar_t ), file );
		fileSystem->Write( &unicodeTab, sizeof( wchar_t ), file );
		fileSystem->Write( &unicodeTab, sizeof( wchar_t ), file );
		fileSystem->Write( &unicodeQuote, sizeof( wchar_t ), file );
		fileSystem->Write( value, wcslen( value ) * sizeof( wchar_t ), file );
		fileSystem->Write( &unicodeQuote, sizeof( wchar_t ), file );
		fileSystem->Write( &unicodeCR, sizeof( wchar_t ), file );
		fileSystem->Write( &unicodeNewline, sizeof( wchar_t ), file );
	}

	// write end string
	strLength = ConvertANSIToUnicode( endStr, unicodeString, (int)sizeof( unicodeString ) );
	fileSystem->Write( unicodeString, strLength * sizeof( wchar_t ), file );
	fileSystem->Close( file );
	return true;
}

void CLocalizedStringTable::AddString( const char* pString, wchar_t* pValue, const char* fileName )
{
	int valueIndex;
	StringIndex_t stridx;
	localizedstring_t item;

	if ( !pString )
	{
		return;
	}

	valueIndex = FindExistingValueIndex( pValue );
	if ( valueIndex == -1 )
	{
		int len = (int)wcslen( pValue ) + 1;

		valueIndex = m_Values.AddMultipleToTail( len );
		memcpy( &m_Values[valueIndex], pValue, len * sizeof( wchar_t ) );
	}

	stridx = FindIndex( pString );
	item.nameIndex = stridx;

	if ( stridx == INVALID_LOCALIZE_STRING_INDEX )
	{
		int len = (int)strlen( pString ) + 1;

		stridx = m_Names.AddMultipleToTail( len );
		memcpy( &m_Names[stridx], pString, len );

		item.nameIndex = stridx;
		item.valueIndex = valueIndex;
		item.filename = fileName ? CUtlSymbol( fileName ) : m_CurrentFile;
		m_Lookup.Insert( item );
		return;
	}

	if ( m_bUseOnlyLongestLanguageString )
	{
		wchar_t* oldValue;
		int newWide;
		int oldWide;
		int tall;

		oldValue = GetValueByIndex( stridx );
		g_pSurface->GetTextSize( 1, pValue, newWide, tall );
		g_pSurface->GetTextSize( 1, oldValue, oldWide, tall );

		if ( newWide < oldWide )
		{
			return;
		}
	}

	item.nameIndex = (StringIndex_t)( GetNameByIndex( stridx ) - &m_Names[0] );
	item.valueIndex = valueIndex;
	item.filename = fileName ? CUtlSymbol( fileName ) : m_CurrentFile;
	m_Lookup[stridx] = item;
}

void CLocalizedStringTable::BuildFastValueLookup()
{
	int c;
	int i;

	m_FastValueLookup.RemoveAll();
	s_pTable = this;

	c = m_Lookup.Count();
	for ( i = 0; i < c; ++i )
	{
		fastvalue_t val;

		val.valueindex = m_Lookup[i].valueIndex;
		val.search = NULL;
		m_FastValueLookup.Insert( val );
	}
}

bool CLocalizedStringTable::AddFile( IFileSystem* fileSystem, const char* szFileName )
{
	char language[64];
	char fileName[MAX_PATH];
	const char* langptr;
	FileHandle_t file;
	unsigned int fileSize;
	ucs2* data;
	ucs2* start;
	bool bEnglishFile;
	bool bQuoted;
	int state;

	enum
	{
		STATE_BASE,
		STATE_TOKENS
	};

	memset( language, 0, sizeof( language ) );
	strncpy( fileName, szFileName, sizeof( fileName ) );

	langptr = strstr( szFileName, s_LanguageToken );
	if ( langptr )
	{
		int offs;
		bool success;
		const char* surfaceLanguage;

		offs = (int)( langptr - szFileName );
		strncpy( fileName, szFileName, offs );
		fileName[offs] = 0;

		if ( g_pSystem->CommandLineParamExists( "-all_languages" ) )
		{
			m_bUseOnlyLongestLanguageString = true;
			return AddAllLanguageFiles( fileSystem, fileName );
		}

		Q_strncat( fileName, s_English, sizeof( fileName ), -1 );
		Q_strncat( fileName, szFileName + offs + (int)strlen( s_LanguageToken ), sizeof( fileName ), -1 );

		success = AddFile( fileSystem, fileName );
		surfaceLanguage = g_pSurface->GetLanguage();
		if ( surfaceLanguage && surfaceLanguage[0] )
		{
			strncpy( language, surfaceLanguage, sizeof( language ) );
		}

		if ( !language[0] )
		{
			if ( !g_pSystem->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language",
				language, (int)sizeof( language ) - 1 ) )
			{
				return success;
			}
		}

		if ( language[0] && strcasecmp( language, s_English ) != 0 )
		{
			bool added;

			strncpy( fileName, szFileName, offs );
			fileName[offs] = 0;
			Q_strncat( fileName, language, sizeof( fileName ), -1 );
			Q_strncat( fileName, szFileName + offs + (int)strlen( s_LanguageToken ), sizeof( fileName ), -1 );
			added = AddFile( fileSystem, fileName );
			success = success & added;
		}

		return success;
	}

	file = fileSystem->Open( fileName, "rb", NULL );
	if ( !file )
	{
		g_pIVgui->DPrintf( "ILocalize::AddFile() failed to load file \"%s\".\n", szFileName );
		return false;
	}

	m_CurrentFile = CUtlSymbol( fileName );
	for ( int i = 0; i < m_LocalizationFiles.Count(); ++i )
	{
		if ( m_LocalizationFiles[i] == m_CurrentFile )
		{
			m_LocalizationFiles.Remove( i );
			break;
		}
	}
	m_LocalizationFiles.AddToTail( m_CurrentFile );

	fileSize = fileSystem->Size( file );
	data = (ucs2*)malloc( (size_t)fileSize + sizeof( ucs2 ) );
	fileSystem->Read( data, (int)fileSize, file );
	data[fileSize >> 1] = 0;

	if ( *data != 0xFEFF )
	{
		fileSystem->Close( file );
		free( data );
		return false;
	}

	start = data + 1;
	bEnglishFile = strstr( fileName, "_english.txt" ) != NULL;
	g_pSystem->CommandLineParamExists( "-ccsyntax" );
	BuildFastValueLookup();
	state = STATE_BASE;

	while ( 1 )
	{
		ucs2 keytoken[128];
		ucs2 valuetoken[2048];
		char key[128];
		ucs2* pchNewdata;

		pchNewdata = ReadUnicodeToken( start, keytoken, 128, bQuoted );
		if ( !keytoken[0] )
		{
			break;	// we've hit the null terminator
		}

		Q_UCS2ToUTF8( keytoken, (int)( (char*)pchNewdata - (char*)start ), key, (int)sizeof( key ) );

		if ( strncasecmp( key, "//", 2 ) == 0 )
		{
			start = ReadToEndOfLine( pchNewdata );
			continue;
		}

		start = ReadUnicodeToken( pchNewdata, valuetoken, 2048, bQuoted );
		if ( !valuetoken[0] && !bQuoted )
		{
			break;	// we've hit the null terminator
		}

		if ( state == STATE_BASE )
		{
			if ( strcasecmp( key, "Language" ) == 0 )
			{
				// copy out our language setting
				char value[2048];

				Q_UCS2ToUTF8( valuetoken, (int)sizeof( valuetoken ), value, (int)sizeof( value ) );
				strncpy( m_szLanguage, value, sizeof( m_szLanguage ) - 1 );
			}
			else if ( strcasecmp( key, "Tokens" ) == 0 )
			{
				state = STATE_TOKENS;
			}
			else if ( strcasecmp( key, "}" ) == 0 )
			{
				break;
			}
		}
		else if ( strcasecmp( key, "}" ) == 0 )
		{
			state = STATE_BASE;
		}
		else if ( bEnglishFile || strncasecmp( key, "[english]", 9 ) != 0 )
		{
			wchar_t fullString[2049];
			int j;

			for ( j = 0; j != 2048; ++j )
			{
				if ( valuetoken[j] == 0 )
				{
					break;
				}

				fullString[j] = valuetoken[j];
			}

			fullString[j] = 0;
			// add the string to the table
			AddString( key, fullString, NULL );
		}
	}

	DiscardFastValueLookup();
	fileSystem->Close( file );
	free( data );
	m_CurrentFile = UTL_INVAL_SYMBOL;
	return true;
}

CLocalizedStringTable_VGUI_Localize002::~CLocalizedStringTable_VGUI_Localize002()
{
}

bool CLocalizedStringTable_VGUI_Localize002::AddFile( IFileSystem* fileSystem, const char* fileName )
{
	return g_StringTable.AddFile( fileSystem, fileName );
}

void CLocalizedStringTable_VGUI_Localize002::RemoveAll()
{
	g_StringTable.RemoveAll();
}

wchar_t* CLocalizedStringTable_VGUI_Localize002::Find( const char* tokenName )
{
	return g_StringTable.Find( tokenName );
}

int CLocalizedStringTable_VGUI_Localize002::ConvertANSIToUnicode( const char* ansi,
	wchar_t* unicode, int unicodeBufferSizeInBytes )
{
	return g_StringTable.ConvertANSIToUnicode( ansi, unicode, unicodeBufferSizeInBytes );
}

int CLocalizedStringTable_VGUI_Localize002::ConvertUnicodeToANSI( const wchar_t* unicode,
	char* ansi, int ansiBufferSize )
{
	return g_StringTable.ConvertUnicodeToANSI( unicode, ansi, ansiBufferSize );
}

OldStringIndex_t CLocalizedStringTable_VGUI_Localize002::FindIndex( const char* tokenName )
{
	return (OldStringIndex_t)g_StringTable.FindIndex( tokenName );
}

void CLocalizedStringTable_VGUI_Localize002::ConstructString( wchar_t* unicodeOutput,
	int unicodeBufferSizeInBytes, wchar_t* formatString, int numFormatParameters, ... )
{
	va_list argList;

	va_start( argList, numFormatParameters );
	ConstructStringFromArgs( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
	va_end( argList );
}

const char* CLocalizedStringTable_VGUI_Localize002::GetNameByIndex( OldStringIndex_t index )
{
	return g_StringTable.GetNameByIndex( index );
}

wchar_t* CLocalizedStringTable_VGUI_Localize002::GetValueByIndex( OldStringIndex_t index )
{
	return g_StringTable.GetValueByIndex( index );
}

OldStringIndex_t CLocalizedStringTable_VGUI_Localize002::GetFirstStringIndex()
{
	return (OldStringIndex_t)g_StringTable.GetFirstStringIndex();
}

OldStringIndex_t CLocalizedStringTable_VGUI_Localize002::GetNextStringIndex( OldStringIndex_t index )
{
	return (OldStringIndex_t)g_StringTable.GetNextStringIndex( index );
}

void CLocalizedStringTable_VGUI_Localize002::AddString( const char* tokenName,
	wchar_t* unicodeString, const char* fileName )
{
	g_StringTable.AddString( tokenName, unicodeString, fileName );
}

void CLocalizedStringTable_VGUI_Localize002::SetValueByIndex( OldStringIndex_t index, wchar_t* newValue )
{
	g_StringTable.SetValueByIndex( index, newValue );
}

bool CLocalizedStringTable_VGUI_Localize002::SaveToFile( IFileSystem* fileSystem, const char* fileName )
{
	return g_StringTable.SaveToFile( fileSystem, fileName );
}

int CLocalizedStringTable_VGUI_Localize002::GetLocalizationFileCount()
{
	return g_StringTable.GetLocalizationFileCount();
}

const char* CLocalizedStringTable_VGUI_Localize002::GetLocalizationFileName( int index )
{
	return g_StringTable.GetLocalizationFileName( index );
}

const char* CLocalizedStringTable_VGUI_Localize002::GetFileNameByIndex( OldStringIndex_t index )
{
	return g_StringTable.GetFileNameByIndex( index );
}
