   //=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//=============================================================================

#include "KeyValues.h"

#include "FileSystem.h"
#include "UtlBuffer.h"
#include "../../../common/q_ftol.h"

#ifdef _WIN32
#include <direct.h>
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for _mkdir
#include <sys/stat.h>
#include <wchar.h>

#ifdef _WIN32
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );

#define KEYVALUES_SOURCE_PATH L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\KeyValues.cpp"
#define snprintf _snprintf
#define swprintf _snwprintf
#endif

#define KEYVALUES_SMALL_ALLOC	24U

static char s_TokenBuf[512];

static char* TerminateToken( int len )
{
	if ( len == 512 )
	{
		len = 511;
	}

	s_TokenBuf[len] = 0;
	return s_TokenBuf;
}

static char* ReadQuotedToken( char** buffer, int len )
{
	for ( ;; )
	{
		char* start = *buffer;
		unsigned char ch = (unsigned char)*start;
		*buffer = start + 1;

		if ( ch == 0 || ch == '"' )
		{
			break;	// new token starts here
		}

		if ( ch == '\\' )
		{
			ch = (unsigned char)start[1];
			*buffer = start + 2;

			if ( ch == 'n' )
			{
				ch = '\n';
			}
			else if ( ch == 't' )
			{
				ch = '\t';
			}
		}

		len++;
		s_TokenBuf[len - 1] = (char)ch;

		if ( len == 512 )
		{
			for ( ;; )
			{
				char* here = *buffer;
				unsigned char skip = (unsigned char)*here;
				*buffer = here + 1;

				if ( skip == 0 || skip == '"' )
				{
					break;
				}

				if ( skip == '\\' )
				{
					*buffer = here + 2;
				}
			}

			len = 511;
			break;
		}
	}

	return TerminateToken( len );
}

static char* ReadUnquotedToken( char** buffer, unsigned char current, int len )
{
	unsigned char lookahead = (unsigned char)**buffer;

	if ( lookahead == 0 || (signed char)current < 0 )
	{
		if ( len != 0 || (signed char)current >= 0 )
		{
			return TerminateToken( len );
		}

		return NULL;
	}

	for ( ;; )
	{
		if ( isspace( (signed char)current ) )
		{
			break;
		}

		if ( len <= 511 )
		{
			len++;
			s_TokenBuf[len - 1] = (char)current;
		}

		char* p = *buffer;
		current = (unsigned char)*p;
		*buffer = p + 1;
		lookahead = (unsigned char)p[1];

		if ( (signed char)current < 0 || lookahead == 0 )
		{
			break;
		}
	}

	return TerminateToken( len );
}

static char* ReadToken( char** buffer )
{
	for ( ;; )
	{
		signed char current;

		for ( ;; )
		{
			current = **buffer;
			( *buffer )++;

			if ( !isspace( current ) )
			{
				break;
			}
		}

		if ( current != '/' )
		{
			if ( current == '"' )
			{
				return ReadQuotedToken( buffer, 0 );
			}

			return ReadUnquotedToken( buffer, (unsigned char)current, 0 );
		}

		unsigned char next = (unsigned char)**buffer;
		( *buffer )++;

		if ( next == '/' )
		{
			signed char ch;

			do
			{
				ch = **buffer;
				( *buffer )++;
			}
			while ( ch > 0 && ch != '\n' );

			continue;
		}

		s_TokenBuf[0] = '/';

		if ( next == '"' )
		{
			return ReadQuotedToken( buffer, 1 );
		}

		return ReadUnquotedToken( buffer, next, 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the symbol name of the current key section
//-----------------------------------------------------------------------------
int KeyValues::GetNameSymbol() const
{
	return m_iKeyName;
}

//-----------------------------------------------------------------------------
// Purpose: looks up a key by symbol name
//-----------------------------------------------------------------------------
KeyValues* KeyValues::FindKey( int keySymbol ) const
{
	for ( KeyValues* dat = m_pSub; dat != NULL; dat = dat->m_pPeer )
	{
		if ( dat->m_iKeyName == keySymbol )
		{
			return dat;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Remove a subkey from the list
//-----------------------------------------------------------------------------
void KeyValues::RemoveSubKey( KeyValues* subKey )
{
	if ( subKey == NULL )
	{
		return;
	}

	if ( subKey == m_pSub )
	{
		m_pSub = subKey->m_pPeer;
		subKey->m_pPeer = NULL;
		return;
	}

	// look through the list
	KeyValues* kv = m_pSub;

	for ( ;; )
	{
		KeyValues* peer = kv->m_pPeer;

		if ( peer == NULL )
		{
			subKey->m_pPeer = NULL;
			return;
		}

		if ( subKey == peer )
		{
			break;
		}

		kv = peer;
	}

	kv->m_pPeer = subKey->m_pPeer;
	subKey->m_pPeer = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Return the first subkey in the list
//-----------------------------------------------------------------------------
KeyValues* KeyValues::GetFirstSubKey()
{
	return m_pSub;
}

//-----------------------------------------------------------------------------
// Purpose: Return the next subkey
//-----------------------------------------------------------------------------
KeyValues* KeyValues::GetNextKey()
{
	return m_pPeer;
}

//-----------------------------------------------------------------------------
// Purpose: Get the pointer value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
void* KeyValues::GetPtr( const char* keyName, void* defaultValue )
{
	KeyValues* dat = FindKey( keyName, false );

	if ( dat == NULL )
	{
		return defaultValue;
	}

	if ( dat->m_iDataType == TYPE_PTR )
	{
		return dat->m_pValue;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Check if a keyName has no value assigned to it.
//-----------------------------------------------------------------------------
bool KeyValues::IsEmpty( const char* keyName )
{
	KeyValues* dat = FindKey( keyName, false );

	if ( dat == NULL )
	{
		return true;
	}

	if ( dat->m_iDataType == TYPE_NONE && dat->m_pSub == NULL )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the data type of the value stored in a keyName
//-----------------------------------------------------------------------------
KeyValues::types_t KeyValues::GetDataType( const char* keyName )
{
	KeyValues* dat = FindKey( keyName, false );

	if ( dat != NULL )
	{
		return (types_t)dat->m_iDataType;
	}

	return TYPE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Get the name of the current key section
//-----------------------------------------------------------------------------
const char* KeyValues::GetName() const
{
	return keyvalues()->GetStringForSymbol( m_iKeyName );
}

//-----------------------------------------------------------------------------
// Purpose: Get the integer value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
int KeyValues::GetInt( const char* keyName, int defaultValue )
{
	KeyValues* dat = FindKey( keyName, false );

	if ( dat == NULL )
	{
		return defaultValue;
	}

	switch ( dat->m_iDataType )
	{
	case TYPE_FLOAT:
		return Q_ftol( dat->m_flValue );

	case TYPE_STRING:
		return atoi( dat->m_sValue );

	case TYPE_WSTRING:
#ifdef _WIN32
		return _wtoi( dat->m_wsValue );
#else
		return (int)wcstol( dat->m_wsValue, NULL, 10 );
#endif

	case TYPE_UINT64:
#ifdef _WIN32
		_wassert( L"0", KEYVALUES_SOURCE_PATH, 812 );
#endif
		return 0;

	default:
		return dat->m_iValue;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the float value of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
float KeyValues::GetFloat( const char* keyName, float defaultValue )
{
	KeyValues* dat = FindKey( keyName, false );

	if ( dat == NULL )
	{
		return defaultValue;
	}

	switch ( dat->m_iDataType )
	{
	case TYPE_STRING:
		return (float)atof( dat->m_sValue );

	case TYPE_INT:
		return (float)dat->m_iValue;

	case TYPE_FLOAT:
		return dat->m_flValue;

	case TYPE_UINT64:
		return (float)( *(long long*)dat->m_sValue );

	default:
		return 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create a new key, with an autogenerated name.  
//			Name is guaranteed to be an integer, of value 1 higher than the highest 
//			other integer key name
//-----------------------------------------------------------------------------
KeyValues* KeyValues::CreateNewKey()
{
	int newID = 1;

	for ( KeyValues* sub = m_pSub; sub != NULL; sub = sub->m_pPeer )
	{
		// case-insensitive string compare
		int id = atoi( sub->GetName() );

		if ( id >= newID )
		{
			newID = id + 1;
		}
	}

	char buf[12];
	snprintf( buf, sizeof( buf ), "%d", newID );
	return FindKey( buf, true );
}

//-----------------------------------------------------------------------------
// Purpose: Set the float value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetFloat( const char* keyName, float value )
{
	KeyValues* dat = FindKey( keyName, true );

	if ( dat == NULL )
	{
		return;
	}

	dat->FreeAllocatedValue();
	dat->m_iDataType = TYPE_FLOAT;
	dat->m_flValue = value;
}

//-----------------------------------------------------------------------------
// Purpose: Set the integer value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetInt( const char* keyName, int value )
{
	KeyValues* dat = FindKey( keyName, true );

	if ( dat == NULL )
	{
		return;
	}

	dat->FreeAllocatedValue();
	dat->m_iDataType = TYPE_INT;
	dat->m_iValue = value;
}

//-----------------------------------------------------------------------------
// Purpose: Set the pointer value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetPtr( const char* keyName, void* value )
{
	KeyValues* dat = FindKey( keyName, true );

	if ( dat == NULL )
	{
		return;
	}

	dat->FreeAllocatedValue();
	dat->m_iDataType = TYPE_PTR;
	dat->m_pValue = value;
}

//-----------------------------------------------------------------------------
// Purpose: Set the string value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetString( const char* keyName, const char* value )
{
	KeyValues* dat = FindKey( keyName, true );

	if ( dat == NULL )
	{
		return;
	}

	dat->FreeAllocatedValue();

	if ( value == NULL )
	{
		// ensure a valid value
		value = "";
	}

	int len = (int)strlen( value ) + 1;
	dat->AllocateValueBlock( len );
	memcpy( dat->m_sValue, value, len );
	dat->m_iDataType = TYPE_STRING;
}

//-----------------------------------------------------------------------------
// Purpose: Set the string value of a keyName. 
//-----------------------------------------------------------------------------
void KeyValues::SetWString( const char* keyName, const wchar_t* value )
{
	KeyValues* dat = FindKey( keyName, true );

	if ( dat == NULL )
	{
		return;
	}

	dat->FreeAllocatedValue();

	if ( value == NULL )
	{
		// ensure a valid value
		value = L"";
	}

	int len = (int)( ( wcslen( value ) + 1 ) * sizeof( wchar_t ) );
	dat->AllocateValueBlock( len );
	memcpy( dat->m_wsValue, value, len );
	dat->m_iDataType = TYPE_WSTRING;
}

//-----------------------------------------------------------------------------
// Purpose: Find a keyValue, create it if it is not found.
//			Set bCreate to true to create the key if it doesn't already exist 
//			(which ensures a valid pointer will be returned)
//-----------------------------------------------------------------------------
KeyValues* KeyValues::FindKey( const char* keyName, bool bCreate )
{
	if ( keyName == NULL || keyName[0] == 0 )
	{
		return this;
	}

	char szBuf[256];
	memset( szBuf, 0, sizeof( szBuf ) );

	const char* search = keyName;
	const char* slash = strchr( keyName, '/' );

	if ( slash != NULL )
	{
		int len = (int)( slash - keyName );

		if ( len > (int)sizeof( szBuf ) )
		{
			len = (int)sizeof( szBuf );
		}

		memcpy( szBuf, keyName, len );
		szBuf[len] = 0;
		search = szBuf;
	}

	int symbol = keyvalues()->GetSymbolForString( search );
	KeyValues* last = NULL;
	KeyValues* dat = m_pSub;

	while ( dat != NULL )
	{
		// symbol compare
		if ( dat->m_iKeyName == symbol )
		{
			break;
		}

		last = dat;
		dat = dat->m_pPeer;
	}

	if ( dat == NULL )
	{
		if ( !bCreate )
		{
			return NULL;
		}

		// we need to create a new key
		dat = new KeyValues( search );
//			assert(dat != NULL);

		if ( last != NULL )
		{
			last->m_pPeer = dat;
		}
		else
		{
			m_pSub = dat;
		}

		dat->m_pPeer = NULL;
		m_iDataType = TYPE_NONE;
	}

	if ( slash != NULL )
	{
		// recursively chain down through the paths in the string
		return dat->FindKey( slash + 1, bCreate );
	}

	return dat;
}

//-----------------------------------------------------------------------------
// Purpose: Makes a copy of the whole key-value pair set
//-----------------------------------------------------------------------------
KeyValues* KeyValues::MakeCopy() const
{
	KeyValues* newKV = new KeyValues( GetName() );

	newKV->m_iDataType = m_iDataType;

	switch ( m_iDataType )
	{
	case TYPE_STRING:
		if ( m_sValue != NULL )
		{
			int len = (int)strlen( m_sValue ) + 1;
			newKV->AllocateValueBlock( len );
			memcpy( newKV->m_sValue, m_sValue, len );
		}
		break;

	case TYPE_INT:
	case TYPE_PTR:
	case TYPE_COLOR:
		newKV->m_iValue = m_iValue;
		break;

	case TYPE_FLOAT:
		newKV->m_flValue = m_flValue;
		break;

	case TYPE_WSTRING:
		if ( m_wsValue != NULL )
		{
			int len = (int)( ( wcslen( m_wsValue ) + 1 ) * sizeof( wchar_t ) );
			newKV->AllocateValueBlock( len );
			memcpy( newKV->m_wsValue, m_wsValue, len );
		}
		break;

	case TYPE_UINT64:
		newKV->AllocateValueBlock( 8 );
		memcpy( newKV->m_sValue, m_sValue, 8 );
		break;

	default:
		break;
	}

	// recursively copy subkeys
	// Also maintain ordering....
	KeyValues* prev = NULL;

	for ( KeyValues* sub = m_pSub; sub != NULL; sub = sub->m_pPeer )
	{
		// take a copy of the subkey
		KeyValues* copy = sub->MakeCopy();

		if ( prev == NULL )
		{
			newKV->m_pSub = copy;
		}
		else
		{
			prev->m_pPeer = copy;
		}

		copy->m_pPeer = NULL;
		prev = copy;
	}

	return newKV;
}

const wchar_t* KeyValues::GetWString( const char* keyName, const wchar_t* defaultValue )
{
	KeyValues* dat = FindKey( keyName, false );

	if ( dat == NULL )
	{
		return defaultValue;
	}

	wchar_t wbuf[512];

	switch ( dat->m_iDataType )
	{
	case TYPE_STRING:
		keyvalues()->GetLocalizedFromANSI( dat->m_sValue, wbuf, sizeof( wbuf ) );
		SetWString( keyName, wbuf );
		break;

	case TYPE_INT:
	case TYPE_PTR:
		swprintf( wbuf, 64, L"%d", dat->m_iValue );
		SetWString( keyName, wbuf );
		break;

	case TYPE_FLOAT:
		swprintf( wbuf, 64, L"%f", dat->m_flValue );
		SetWString( keyName, wbuf );
		break;

	case TYPE_WSTRING:
		break;

	case TYPE_UINT64:
		swprintf( wbuf, 64, L"%I64i", *(long long*)dat->m_sValue );
		SetWString( keyName, wbuf );
		break;

	default:
		return defaultValue;
	}

	return dat->m_wsValue;
}

//-----------------------------------------------------------------------------
// Purpose: Get the string pointer of a keyName. Default value is returned
//			if the keyName can't be found.
//-----------------------------------------------------------------------------
const char* KeyValues::GetString( const char* keyName, const char* defaultValue )
{
	KeyValues* dat = FindKey( keyName, false );

	if ( dat == NULL )
	{
		return defaultValue;
	}

	// convert the data to string form then return it
	char buf[512];

	switch ( dat->m_iDataType )
	{
	case TYPE_STRING:
		break;

	case TYPE_INT:
	case TYPE_PTR:
		snprintf( buf, 64, "%d", dat->m_iValue );
		SetString( keyName, buf );
		break;

	case TYPE_FLOAT:
		snprintf( buf, 64, "%f", dat->m_flValue );
		SetString( keyName, buf );
		break;

	case TYPE_WSTRING:
		keyvalues()->GetANSIFromLocalized( dat->m_wsValue, buf, 512 );
		SetString( keyName, buf );
		break;

	case TYPE_UINT64:
		snprintf( buf, 64, "%I64i", *(long long*)dat->m_sValue );
		SetString( keyName, buf );
		break;

	default:
		return defaultValue;
	}

	return dat->m_sValue;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char* setName )
{
	Init( setName );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char* setName, const char* firstKey, const char* firstValue )
{
	Init( setName );
	SetString( firstKey, firstValue );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
KeyValues::KeyValues( const char* setName, const char* firstKey, const wchar_t* firstValue )
{
	Init( setName );
	SetWString( firstKey, firstValue );
}

static bool UseKeyValuesPool( int size )
{
	return (unsigned int)size <= KEYVALUES_SMALL_ALLOC;
}

static void FreeValueMemory( void* value, int allocationSize )
{
	if ( allocationSize == 0 )
	{
		return;
	}

	if ( UseKeyValuesPool( allocationSize ) )
	{
		keyvalues()->FreeKeyValuesMemory( value );
	}
	else if ( value != NULL )
	{
		delete[] (char*)value;
	}
}

static void PutTabs( CUtlBuffer& buffer, int count )
{
	int i;

	for ( i = 0; i < count; i++ )
	{
		buffer.Put( "\t", 1 );
	}
}

static void PutKeyName( CUtlBuffer& buffer, KeyValues* key )
{
	const char* name = key->GetName();
	int length = (int)strlen( name );

	name = key->GetName();
	buffer.Put( name, length );
}

static void PutValueLeadIn( CUtlBuffer& buffer, KeyValues* key, int indentLevel )
{
	int i;

	for ( i = 0; i <= indentLevel; i++ )
	{
		buffer.Put( "\t", 1 );
	}

	buffer.Put( "\"", 1 );
	PutKeyName( buffer, key );
	buffer.Put( "\"\t\t\"", 4 );
}

KeyValues::KeyValues( const char* setName, const char* firstKey, int firstValue )
{
	Init( setName );

	KeyValues* key = FindKey( firstKey, true );

	if ( key != NULL )
	{
		key->FreeAllocatedValue();
		key->m_iDataType = TYPE_INT;
		key->m_iValue = firstValue;
	}
}

KeyValues::KeyValues( const char* setName, const char* firstKey, const char* firstValue,
		const char* secondKey, const char* secondValue )
{
	const char* names[2];
	const char* values[2];
	int i;

	names[0] = firstKey;
	names[1] = secondKey;
	values[0] = firstValue;
	values[1] = secondValue;

	Init( setName );

	for ( i = 0; i < 2; i++ )
	{
		KeyValues* key = FindKey( names[i], true );
		const char* text;
		unsigned int length;

		if ( key == NULL )
		{
			continue;
		}

		if ( values[i] == NULL )
		{
			text = "";
			length = 1;
		}
		else
		{
			text = values[i];
			length = (unsigned int)strlen( text ) + 1;
		}

		key->FreeAllocatedValue();
		key->AllocateValueBlock( (int)length );
		memcpy( key->m_sValue, text, length );
		key->m_iDataType = TYPE_STRING;
	}
}

KeyValues::KeyValues( const char* setName, const char* firstKey, int firstValue,
		const char* secondKey, int secondValue )
{
	const char* names[2];
	int values[2];
	int i;

	names[0] = firstKey;
	names[1] = secondKey;
	values[0] = firstValue;
	values[1] = secondValue;

	Init( setName );

	for ( i = 0; i < 2; i++ )
	{
		KeyValues* key = FindKey( names[i], true );

		if ( key == NULL )
		{
			continue;
		}

		key->FreeAllocatedValue();
		key->m_iDataType = TYPE_INT;
		key->m_iValue = values[i];
	}
}

void KeyValues::Init( const char* setName )
{
	int symbol = keyvalues()->GetSymbolForString( setName );

	m_iDataType = TYPE_NONE;
	m_pSub = NULL;
	m_pPeer = NULL;
	m_iValue = 0;
	m_iAllocationSize = 0;
	m_iKeyName = symbol;
}

KeyValues::~KeyValues()
{
	KeyValues* dat = m_pSub;

	while ( dat != NULL )
	{
		KeyValues* next = dat->m_pPeer;

		delete dat;
		dat = next;
	}

	FreeValueMemory( m_pValue, m_iAllocationSize );
}

void KeyValues::Clear()
{
	KeyValues* sub = m_pSub;

	while ( sub != NULL )
	{
		KeyValues* peer = sub->m_pPeer;

		delete sub;
		sub = peer;
	}

	m_pSub = NULL;
	FreeAllocatedValue();
	m_iDataType = TYPE_NONE;
}

void KeyValues::deleteThis()
{
	if ( this )
	{
		delete this;
	}
}

void KeyValues::RecursiveLoadFromBuffer( char** buffer )
{
	for ( ;; )
	{
		char* token = ReadToken( buffer );

		if ( token == NULL )
		{
			return;
		}

		for ( ;; )
		{
			KeyValues* dat;
			char* value;
			unsigned int length;

			if ( *token == 0 || strcmp( token, "}" ) == 0 )
			{
				return;
			}

			dat = FindKey( token, true );
			value = ReadToken( buffer );

			if ( value == NULL || strcmp( value, "}" ) == 0 )
			{
				return;
			}

			if ( strcmp( value, "{" ) == 0 )
			{
				dat->RecursiveLoadFromBuffer( buffer );
				token = ReadToken( buffer );

				if ( token == NULL )
				{
					return;
				}

				continue;
			}

			FreeAllocatedValue();
			length = (unsigned int)strlen( value ) + 1;
			dat->AllocateValueBlock( (int)length );
			memcpy( dat->m_sValue, value, length );
			dat->m_iDataType = TYPE_STRING;
			break;
		}
	}
}

void KeyValues::LoadFromBuffer( const char* buffer )
{
	char* current = (char*)buffer;
	char* token = ReadToken( &current );

	if ( token != NULL )
	{
		m_iKeyName = keyvalues()->GetSymbolForString( token );
	}

	token = ReadToken( &current );

	if ( m_iKeyName >= 0 && token != NULL && *token == '{' )
	{
		RecursiveLoadFromBuffer( &current );
	}
}

bool KeyValues::LoadFromFile( IFileSystem* filesystem, const char* resourceName, const char* pathID )
{
	FileHandle_t file;
	int fileSize;
	char* fileBuffer;
	char* current;
	char* token;

#ifdef _WIN32
	if ( !filesystem )
	{
		_wassert( L"filesystem", KEYVALUES_SOURCE_PATH, 283 );
	}

	if ( _heapchk() != _HEAPOK )
	{
		_wassert( L"_heapchk() == _HEAPOK", KEYVALUES_SOURCE_PATH, 284 );
	}
#endif

	file = filesystem->Open( resourceName, "rb", pathID );
	if ( file == NULL )
	{
		return false;
	}

	fileSize = filesystem->Size( file );
	fileBuffer = (char*)malloc( (unsigned int)fileSize + 1 );

#ifdef _WIN32
	if ( !fileBuffer )
	{
		_wassert( L"buffer", KEYVALUES_SOURCE_PATH, 294 );
	}
#endif

	fileBuffer[fileSize] = 0; // null terminate file as EOF
	filesystem->Read( fileBuffer, fileSize, file );

	current = fileBuffer;
	token = ReadToken( &current );

	if ( token != NULL )
	{
		m_iKeyName = keyvalues()->GetSymbolForString( token );
	}

	token = ReadToken( &current );

	if ( m_iKeyName >= 0 && token != NULL && *token == '{' )
	{
		RecursiveLoadFromBuffer( &current );
	}

	filesystem->Close( file );
	free( fileBuffer );
	return true;
}

void KeyValues::WriteConvertedString( CUtlBuffer& buffer, const char* pszString )
{
	int len = (int)strlen( pszString );
	char* convertedString = (char*)alloca( ( len + 1 ) * sizeof( char ) * 2 );
	int j = 0;

	for ( int i = 0; i <= len; i++ )
	{
		if ( pszString[i] == '"' || pszString[i] == '\\' )
		{
			convertedString[j] = '\\';
			j++;
		}

		convertedString[j] = pszString[i];
		j++;
	}

	buffer.Put( convertedString, (int)strlen( convertedString ) );
}

void KeyValues::RecursiveSaveToFile( IFileSystem* filesystem, CUtlBuffer& buffer, int indentLevel )
{
	KeyValues* sub;
	char text[1024];

	PutTabs( buffer, indentLevel );
	buffer.Put( "\"", 1 );
	PutKeyName( buffer, this );
	buffer.Put( "\"\n", 2 );

	PutTabs( buffer, indentLevel );
	buffer.Put( "{\n", 2 );

	sub = m_pSub;

	while ( sub != NULL )
	{
		unsigned short type = sub->m_iDataType;

		if ( sub->m_pSub )
		{
			sub->RecursiveSaveToFile( filesystem, buffer, indentLevel + 1 );
		}
		else if ( type == TYPE_INT )
		{
			PutValueLeadIn( buffer, sub, indentLevel );
			snprintf( text, 32, "%d", sub->m_iValue );
			buffer.Put( text, (int)strlen( text ) );
			buffer.Put( "\"\n", 2 );
		}
		else if ( type == TYPE_FLOAT )
		{
			PutValueLeadIn( buffer, sub, indentLevel );
			snprintf( text, 48, "%f", sub->m_flValue );
			buffer.Put( text, (int)strlen( text ) );
			buffer.Put( "\"\n", 2 );
		}
		else if ( type == TYPE_WSTRING && sub->m_pValue )
		{
			keyvalues()->GetANSIFromLocalized( (const wchar_t*)sub->m_pValue, text, 1024 );
			PutValueLeadIn( buffer, sub, indentLevel );
			WriteConvertedString( buffer, text );
			buffer.Put( "\"\n", 2 );
		}
		else if ( type == TYPE_STRING && sub->m_sValue && sub->m_sValue[0] )
		{
			PutValueLeadIn( buffer, sub, indentLevel );
			WriteConvertedString( buffer, sub->m_sValue );
			buffer.Put( "\"\n", 2 );
		}

		sub = sub->m_pPeer;
	}

	PutTabs( buffer, indentLevel );
	buffer.Put( "}\n", 2 );
}

bool KeyValues::SaveToFile( IFileSystem* filesystem, const char* resourceName, const char* pathID )
{
	// create a write file
	FileHandle_t file = filesystem->Open( resourceName, "wb", pathID );

	if ( file == NULL )
	{
		char szBuf[1024];
		const char* current = resourceName;

		memset( szBuf, 0, sizeof( szBuf ) );

		for ( ;; )
		{
			const char* slash = strchr( current, '\\' );
			int pathSize;

			if ( slash == NULL )
			{
				break;
			}

			pathSize = (int)( slash - current );

			if ( szBuf[0] )
			{
				strcat( szBuf, "\\" );
			}

			strncat( szBuf, current, pathSize );
			current += pathSize + 1;
#ifdef _WIN32
#ifdef _MSC_VER
#pragma warning( suppress : 6031 )
#endif
			// make sure the first path is created
			_mkdir( szBuf );
#else
			mkdir( szBuf, 0777 );
#endif
		}

		file = filesystem->Open( resourceName, "wb", pathID );

		if ( file == NULL )
		{
			return false;
		}
	}

	CUtlBuffer buffer( 0, 4096, true );

	RecursiveSaveToFile( filesystem, buffer, 0 );

	if ( buffer.IsText() )
	{
		buffer.Printf( "%c", 0 );
	}
	else
	{
		char terminator = 0;

		buffer.Put( &terminator, 1 );
	}

	filesystem->Write( buffer.Base(), buffer.TellPut(), file );
	filesystem->Close( file );
	return true;
}

//-----------------------------------------------------------------------------
// Adds a subkey. Make sure the subkey isn't a child of some other keyvalues
//-----------------------------------------------------------------------------
void KeyValues::AddSubKey( KeyValues* pSubkey )
{
	KeyValues* sub = m_pSub;

	if ( sub == NULL )
	{
		m_pSub = pSubkey;
		return;
	}

	while ( sub->GetNextKey() != NULL )
	{
		sub = sub->GetNextKey();
	}

	sub->m_pPeer = pSubkey;
}

//-----------------------------------------------------------------------------
// Purpose: Sets this key's peer to the KeyValues passed in
//-----------------------------------------------------------------------------
void KeyValues::SetNextKey( KeyValues* pDat )
{
	m_pPeer = pDat;
}

unsigned long long KeyValues::GetUint64( const char* keyName, unsigned long long defaultValue )
{
	KeyValues* dat = FindKey( keyName, false );

	if ( dat == NULL )
	{
		return defaultValue;
	}

	switch ( dat->m_iDataType )
	{
	case TYPE_STRING:
		return (unsigned long long)strtol( dat->m_sValue, NULL, 10 );

	case TYPE_FLOAT:
		return (unsigned long long)Q_ftol( dat->m_flValue );

	case TYPE_WSTRING:
		return (unsigned long long)wcstol( (const wchar_t*)dat->m_pValue, NULL, 10 );

	case TYPE_UINT64:
		return *(unsigned long long*)dat->m_pValue;

	default:
		return (unsigned long long)dat->m_iValue;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets a color
//-----------------------------------------------------------------------------
Color KeyValues::GetColor( const char* keyName )
{
	Color color;
	KeyValues* dat;

	color[0] = 0;
	color[1] = 0;
	color[2] = 0;
	color[3] = 0;

	dat = FindKey( keyName, false );

	if ( dat == NULL )
	{
		return color;
	}

	switch ( dat->m_iDataType )
	{
	case TYPE_STRING:
		{
			float red = 0.0f;
			float green = 0.0f;
			float blue = 0.0f;
			float alpha = 0.0f;

			if ( sscanf( dat->m_sValue, "%f %f %f %f", &red, &green, &blue, &alpha ) <= 0 )
				break;
			color[0] = (unsigned char)Q_ftol( red );
			color[1] = (unsigned char)Q_ftol( green );
			color[2] = (unsigned char)Q_ftol( blue );
			color[3] = (unsigned char)Q_ftol( alpha );
			break;
		}

	case TYPE_INT:
		color[0] = (unsigned char)dat->m_iValue;
		break;

	case TYPE_FLOAT:
		color[0] = (unsigned char)Q_ftol( dat->m_flValue );
		break;

	case TYPE_COLOR:
		color[0] = dat->m_Color[0];
		color[1] = dat->m_Color[1];
		color[2] = dat->m_Color[2];
		color[3] = dat->m_Color[3];
		break;
	}

	return color;
}

//-----------------------------------------------------------------------------
// Purpose: Sets a color
//-----------------------------------------------------------------------------
void KeyValues::SetColor( const char* keyName, Color value )
{
	KeyValues* key = FindKey( keyName, true );

	if ( key == NULL )
	{
		return;
	}

	key->FreeAllocatedValue();
	key->m_Color[0] = value[0];
	key->m_Color[1] = value[1];
	key->m_Color[2] = value[2];
	key->m_Color[3] = value[3];
	key->m_iDataType = TYPE_COLOR;
}

void KeyValues::SetUint64( const char* keyName, unsigned long long value )
{
	KeyValues* key = FindKey( keyName, true );
	unsigned long long* stored;

	if ( key == NULL )
	{
		return;
	}

	key->FreeAllocatedValue();
	stored = (unsigned long long*)keyvalues()->AllocKeyValuesMemory( 8 );
	key->m_iAllocationSize = 8;
	key->m_pValue = stored;
	*stored = value;
	key->m_iDataType = TYPE_UINT64;
}

void KeyValues::AllocateValueBlock( int iAllocSize )
{
	void* block;

	if ( UseKeyValuesPool( iAllocSize ) )
	{
		block = keyvalues()->AllocKeyValuesMemory( iAllocSize );
	}
	else
	{
		block = new char[iAllocSize];
	}

	m_iAllocationSize = (short)iAllocSize;
	m_pValue = block;
}

void KeyValues::FreeAllocatedValue()
{
	if ( m_iAllocationSize == 0 )
	{
		return;
	}

	FreeValueMemory( m_pValue, m_iAllocationSize );
	m_iAllocationSize = 0;
}

//-----------------------------------------------------------------------------
// Purpose: memory allocator
//-----------------------------------------------------------------------------
void* KeyValues::operator new( size_t iAllocSize )
{
	return keyvalues()->AllocKeyValuesMemory( (int)iAllocSize );
}

void* KeyValues::operator new( size_t iAllocSize, int, const char*, int )
{
	return keyvalues()->AllocKeyValuesMemory( (int)iAllocSize );
}

//-----------------------------------------------------------------------------
// Purpose: deallocator
//-----------------------------------------------------------------------------
void KeyValues::operator delete( void* pMem )
{
	keyvalues()->FreeKeyValuesMemory( pMem );
}

void KeyValues::operator delete( void* pMem, int, const char*, int )
{
	keyvalues()->FreeKeyValuesMemory( pMem );
}
