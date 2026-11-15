   //=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//=============================================================================

#include "vgui/KeyValues.h"
#include "IKeyValues.h"
#include "Controls.h"
#include "FileSystem.h"
#include "UtlBuffer.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif
#include <malloc.h>

#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <new>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"public\\KeyValues.cpp" )

#if !defined( _WIN32 )
#define _wtoi( string ) static_cast<int>( std::wcstol( string, nullptr, 10 ) )
#define _mkdir( directory ) mkdir( directory, 0777 )
#else
static_assert( sizeof( wchar_t ) == 2 );
#endif

typedef unsigned long long uint64;

KeyValues::KeyValues( const char* name, const char* firstKey, const char* firstValue )
{
	Init( name );
	KeyValues::SetString( firstKey, firstValue );
}

KeyValues::KeyValues( const char* name, const char* firstKey, int firstValue )
{
	Init( name );
	KeyValues::SetInt( firstKey, firstValue );
}

KeyValues::KeyValues( const char* name, const char* firstKey, int firstValue, const char* secondKey, int secondValue )
{
	Init( name );
	KeyValues::SetInt( firstKey, firstValue );
	KeyValues::SetInt( secondKey, secondValue );
}

KeyValues::KeyValues(
	const char* name, const char* firstKey, const char* firstValue, const char* secondKey, const char* secondValue )
{
	Init( name );
	KeyValues::SetString( firstKey, firstValue );
	KeyValues::SetString( secondKey, secondValue );
}

KeyValues::KeyValues( const char* name, const char* firstKey, const wchar_t* firstValue )
{
	Init( name );
	KeyValues::SetWString( firstKey, firstValue );
}

KeyValues::KeyValues( const char* name )
{
	Init( name );
}

void KeyValues::Init( const char* name )
{
	m_iKeyName = keyvalues()->GetSymbolForString( name );
	m_iDataType = TYPE_NONE;
	m_pSub = nullptr;
	m_pPeer = nullptr;
	m_iValue = 0;
	m_iAllocationSize = 0;
}

KeyValues::~KeyValues()
{
	KeyValues* child = m_pSub;
	while ( child != nullptr )
	{
		KeyValues* next = child->m_pPeer;
		delete child;
		child = next;
	}
	FreeAllocatedValue();
}

void* KeyValues::operator new( std::size_t size )
{
	return keyvalues()->AllocKeyValuesMemory( static_cast<int>( size ) );
}

void KeyValues::operator delete( void* memory )
{
	keyvalues()->FreeKeyValuesMemory( memory );
}

void* KeyValues::operator new( std::size_t size, int, const char*, int )
{
	return keyvalues()->AllocKeyValuesMemory( static_cast<int>( size ) );
}

void KeyValues::operator delete( void* memory, int, const char*, int )
{
	keyvalues()->FreeKeyValuesMemory( memory );
}

void KeyValues::AllocateValueBlock( int size )
{
	const unsigned int allocationSize = static_cast<unsigned int>( size );
	if ( allocationSize > sizeof( KeyValues ) )
		m_pValue = new char[allocationSize];
	else
		m_pValue = keyvalues()->AllocKeyValuesMemory( size );
	m_iAllocationSize = static_cast<short>( size );
}

void KeyValues::FreeAllocatedValue()
{
	const unsigned short allocationSize = static_cast<unsigned short>( m_iAllocationSize );
	if ( allocationSize == 0 )
		return;

	if ( allocationSize > sizeof( KeyValues ) )
		delete[] static_cast<char*>( m_pValue );
	else
		keyvalues()->FreeKeyValuesMemory( m_pValue );
	m_iAllocationSize = 0;
}

static char* ReadToken( char** buffer )
{
	constexpr unsigned TOKEN_BUFFER_BYTES = 512;
	static char buf[TOKEN_BUFFER_BYTES];
	unsigned length = 0;
	signed char character;
	for ( ;; )
	{
		do
		{
			character = static_cast<signed char>( *( *buffer )++ );
		} while ( std::isspace( static_cast<unsigned char>( character ) ) );
		if ( character != '/' )
			break;
		character = static_cast<signed char>( *( *buffer )++ );
		if ( character != '/' )
		{
			buf[length++] = '/';
			break;
		}
		while ( character != '\n' && character > 0 )
			character = static_cast<signed char>( *( *buffer )++ );
	}
	if ( character == '"' )
	{
		character = static_cast<signed char>( *( *buffer )++ );
		while ( character != '"' && character != 0 )
		{
			if ( character == '\\' )
			{
				character = static_cast<signed char>( *( *buffer )++ );
				switch ( character )
				{
				case '"':
					character = '"';
					break;
				case '\\':
					character = '\\';
					break;
				case 'n':
					character = '\n';
					break;
				case 't':
					character = '\t';
					break;
				}
			}
			if ( length < TOKEN_BUFFER_BYTES )
				buf[length++] = character;
			character = static_cast<signed char>( *( *buffer )++ );
		}
	}
	else
	{
		if ( **buffer != 0 )
		{
			while ( character >= 0 )
			{
				if ( std::isspace( static_cast<unsigned char>( character ) ) )
					break;
				if ( length < TOKEN_BUFFER_BYTES )
					buf[length++] = character;
				character = static_cast<signed char>( *( *buffer )++ );
				if ( **buffer == 0 )
					break;
			}
		}
		if ( character < 0 && length == 0 )
			return nullptr;
	}
	if ( length == TOKEN_BUFFER_BYTES )
		length = TOKEN_BUFFER_BYTES - 1;
	buf[length] = 0;
	return buf;
}

const char* KeyValues::GetName() const
{
	return keyvalues()->GetStringForSymbol( m_iKeyName );
}

int KeyValues::GetNameSymbol() const
{
	return m_iKeyName;
}

void KeyValues::RemoveSubKey( KeyValues* key )
{
	if ( key == nullptr )
		return;

	// check the list pointer
	if ( m_pSub == key )
	{
		m_pSub = key->m_pPeer;
	}
	else
	{
		KeyValues* previous = m_pSub;
		while ( previous->m_pPeer != nullptr )
		{
			if ( previous->m_pPeer == key )
			{
				previous->m_pPeer = key->m_pPeer;
				break;
			}
			previous = previous->m_pPeer;
		}
	}
	key->m_pPeer = nullptr;
}



//-----------------------------------------------------------------------------
// Purpose: returns the first subkey in the list
// Output : KeyValues *
//-----------------------------------------------------------------------------
KeyValues* KeyValues::GetFirstSubKey()
{
	return m_pSub;
}

//-----------------------------------------------------------------------------
// Purpose: returns the next subkey
// Output : KeyValues
//-----------------------------------------------------------------------------
KeyValues* KeyValues::GetNextKey()
{
	return m_pPeer;
}

void KeyValues::AddSubKey( KeyValues* key )
{
	KeyValues* child = m_pSub;
	if ( child == nullptr )
	{
		m_pSub = key;
		return;
	}

	while ( child->GetNextKey() != nullptr )
		child = child->GetNextKey();

	child->SetNextKey( key );
}

void KeyValues::SetNextKey( KeyValues* key )
{
	m_pPeer = key;
}

KeyValues* KeyValues::FindKey( int symbol ) const
{
	for ( KeyValues* key = m_pSub; key; key = key->m_pPeer )
	{
		if ( key->m_iKeyName == symbol )
			return key;
	}
	return nullptr;
}

KeyValues* KeyValues::FindKey( const char* name, bool create )
{
	if ( !name || !*name )
		return this;

	char segment[256 + 1] = {};
	const char* slash = std::strchr( name, '/' );
	const char* keyName = name;
	if ( slash )
	{
		std::size_t length = slash - name;
		if ( length > sizeof( segment ) - 1 )
			length = sizeof( segment ) - 1;
		std::memcpy( segment, name, length );
		segment[length] = '\0';
		keyName = segment;
	}

	int symbol = keyvalues()->GetSymbolForString( keyName );
	KeyValues* previous = NULL;
	KeyValues* key = m_pSub;
	while ( key )
	{
		previous = key;
		if ( key->m_iKeyName == symbol )
			break;
		key = key->m_pPeer;
	}
	if ( !key )
	{
		if ( !create )
			return NULL;
		key = new KeyValues( keyName );
		if ( previous )
			previous->m_pPeer = key;
		else
			m_pSub = key;
		key->m_pPeer = NULL;
		m_iDataType = TYPE_NONE;
	}
	return slash ? key->FindKey( slash + 1, create ) : key;
}

KeyValues* KeyValues::CreateNewKey()
{
	int nextNumber = 1;
	for ( KeyValues* key = m_pSub; key; key = key->m_pPeer )
	{
		int number = atoi( key->GetName() );
		if ( nextNumber <= number )
			nextNumber = static_cast<int>( static_cast<unsigned int>( number ) + 1u );
	}
	char name[12];
	_snprintf( name, sizeof( name ), "%d", nextNumber );
	return FindKey( name, true );
}

int KeyValues::GetInt( const char* name, int defaultValue )
{
	KeyValues* key = FindKey( name, false );
	if ( !key )
		return defaultValue;
	switch ( key->m_iDataType )
	{
	case TYPE_STRING:
		return atoi( key->m_sValue );
	case TYPE_FLOAT:
	{
		constexpr double INTEGER_LIMIT = 2147483648.0;
		double value = key->m_flValue;
		if ( value >= -INTEGER_LIMIT && value < INTEGER_LIMIT )
			return static_cast<int>( value );
		return ( std::numeric_limits<int>::min )();
	}
	case TYPE_WSTRING:
		return _wtoi( reinterpret_cast<const wchar_t*>( key->m_sValue ) );
	case TYPE_UINT64:
		// can't convert, since it would lose data
		ASSERT_AT( 0, 812 );
		return 0;
	default:
		return key->m_iValue;
	}
}

uint64 KeyValues::GetUint64( const char* name, uint64 defaultValue )
{
	KeyValues* key = FindKey( name, false );
	if ( !key )
		return defaultValue;

	switch ( key->m_iDataType )
	{
	case TYPE_STRING:
		return static_cast<uint64>( atoi( key->m_sValue ) );
	case TYPE_FLOAT:
	{
		constexpr double INTEGER_LIMIT = 2147483648.0;
		double value = key->m_flValue;
		int integer = ( std::numeric_limits<int>::min )();
		if ( value >= -INTEGER_LIMIT && value < INTEGER_LIMIT )
			integer = static_cast<int>( value );
		return static_cast<uint64>( integer );
	}
	case TYPE_WSTRING:
		return static_cast<uint64>( _wtoi( static_cast<const wchar_t*>( key->m_pValue ) ) );
	case TYPE_UINT64:
	{
		uint64 value;
		std::memcpy( &value, key->m_pValue, sizeof( value ) );
		return value;
	}
	default:
		return static_cast<uint64>( key->m_iValue );
	}
}

float KeyValues::GetFloat( const char* name, float defaultValue )
{
	KeyValues* key = FindKey( name, false );
	if ( !key )
		return defaultValue;
	switch ( key->m_iDataType )
	{
	case TYPE_STRING:
		return static_cast<float>( atof( key->m_sValue ) );
	case TYPE_INT:
		return static_cast<float>( key->m_iValue );
	case TYPE_FLOAT:
		return key->m_flValue;
	case TYPE_UINT64:
	{
		std::int64_t value;
		std::memcpy( &value, key->m_pValue, sizeof( value ) );
		return static_cast<float>( static_cast<double>( value ) );
	}
	default:
		return 0.0f;
	}
}

void* KeyValues::GetPtr( const char* name, void* defaultValue )
{
	KeyValues* key = FindKey( name, false );
	if ( !key )
		return defaultValue;
	if ( key->m_iDataType == TYPE_PTR )
		return key->m_pValue;
	return nullptr;
}

Color KeyValues::GetColor( const char* name )
{
	Color color( 0, 0, 0, 0 );
	KeyValues* key = FindKey( name, false );
	if ( !key )
		return color;

	switch ( key->m_iDataType )
	{
	case TYPE_STRING:
	{
		float red = 0.0f;
		float green = 0.0f;
		float blue = 0.0f;
		float alpha = 0.0f;
		if ( std::sscanf( key->m_sValue, "%f %f %f %f", &red, &green, &blue, &alpha ) <= 0 )
			break;
		color.SetColor( (int)red, (int)green, (int)blue, (int)alpha );
		break;
	}
	case TYPE_INT:
		color.SetColor( key->m_iValue, 0, 0, 0 );
		break;
	case TYPE_FLOAT:
		color.SetColor( (int)key->m_flValue, 0, 0, 0 );
		break;
	case TYPE_COLOR:
		color.SetColor( key->m_Color[0], key->m_Color[1], key->m_Color[2], key->m_Color[3] );
		break;
	default:
		break;
	}
	return color;
}

const char* KeyValues::GetString( const char* name, const char* defaultValue )
{
	constexpr int FORMAT_VALUE_CHARACTERS = 64;
	constexpr int ANSI_VALUE_BYTES = 512;
	KeyValues* key = FindKey( name, false );
	if ( !key )
		return defaultValue;

	char formatted[FORMAT_VALUE_CHARACTERS];
	switch ( key->m_iDataType )
	{
	case TYPE_STRING:
		return key->m_sValue;
	case TYPE_INT:
	case TYPE_PTR:
		_snprintf( formatted, FORMAT_VALUE_CHARACTERS, "%d", key->m_iValue );
		break;
	case TYPE_FLOAT:
		_snprintf( formatted, FORMAT_VALUE_CHARACTERS, "%f", static_cast<double>( key->m_flValue ) );
		break;
	case TYPE_WSTRING:
	{
		char ansi[ANSI_VALUE_BYTES];
		keyvalues()->GetANSIFromLocalized( static_cast<const wchar_t*>( key->m_pValue ), ansi, ANSI_VALUE_BYTES );
		SetString( name, ansi );
		return key->m_sValue;
	}
	case TYPE_UINT64:
	{
		uint64 value;
		std::memcpy( &value, key->m_pValue, sizeof( value ) );
		_snprintf( formatted, FORMAT_VALUE_CHARACTERS, "%I64i", value );
		break;
	}
	default:
		return defaultValue;
	}

	SetString( name, formatted );
	return key->m_sValue;
}

const wchar_t* KeyValues::GetWString( const char* name, const wchar_t* defaultValue )
{
	constexpr int FORMAT_VALUE_CHARACTERS = 64;
	constexpr int LOCALIZED_VALUE_CHARACTERS = 512;
	KeyValues* key = FindKey( name, false );
	if ( !key )
		return defaultValue;

	wchar_t formatted[FORMAT_VALUE_CHARACTERS];
	switch ( key->m_iDataType )
	{
	case TYPE_STRING:
	{
		wchar_t localized[LOCALIZED_VALUE_CHARACTERS];
		keyvalues()->GetLocalizedFromANSI( key->m_sValue, localized, sizeof( localized ) );
		SetWString( name, localized );
		return static_cast<const wchar_t*>( key->m_pValue );
	}
	case TYPE_INT:
	case TYPE_PTR:
		_snwprintf( formatted, FORMAT_VALUE_CHARACTERS, L"%d", key->m_iValue );
		break;
	case TYPE_FLOAT:
		_snwprintf( formatted, FORMAT_VALUE_CHARACTERS, L"%f", static_cast<double>( key->m_flValue ) );
		break;
	case TYPE_WSTRING:
		return static_cast<const wchar_t*>( key->m_pValue );
	case TYPE_UINT64:
	{
		uint64 value;
		std::memcpy( &value, key->m_pValue, sizeof( value ) );
		_snwprintf( formatted, FORMAT_VALUE_CHARACTERS, L"%I64i", value );
		break;
	}
	default:
		return defaultValue;
	}

	SetWString( name, formatted );
	return static_cast<const wchar_t*>( key->m_pValue );
}

bool KeyValues::IsEmpty( const char* name )
{
	KeyValues* key = FindKey( name, false );
	return !key || ( key->m_iDataType == TYPE_NONE && !key->m_pSub );
}

KeyValues::types_t KeyValues::GetDataType( const char* name )
{
	KeyValues* key = FindKey( name, false );
	return key ? static_cast<types_t>( key->m_iDataType ) : TYPE_NONE;
}

void KeyValues::SetWString( const char* name, const wchar_t* value )
{
	KeyValues* key = FindKey( name, true );
	if ( key == nullptr )
		return;

	key->FreeAllocatedValue();
	const wchar_t* string = value ? value : L"";
	std::size_t size = ( std::wcslen( string ) + 1 ) * sizeof( wchar_t );
	key->AllocateValueBlock( static_cast<int>( size ) );
	std::memcpy( key->m_pValue, string, size );
	key->m_iDataType = TYPE_WSTRING;
}

void KeyValues::SetString( const char* name, const char* value )
{
	KeyValues* key = FindKey( name, true );
	if ( key == nullptr )
		return;

	key->FreeAllocatedValue();
	const char* string = value ? value : "";
	std::size_t size = std::strlen( string ) + 1;
	key->AllocateValueBlock( static_cast<int>( size ) );
	std::memcpy( key->m_pValue, string, size );
	key->m_iDataType = TYPE_STRING;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : char *keyName - 
//			value - 
//-----------------------------------------------------------------------------
void KeyValues::SetInt( const char* name, int value )
{
	KeyValues* key = FindKey( name, true );
	if ( key == nullptr )
		return;

	key->FreeAllocatedValue();
	key->m_iValue = value;
	key->m_iDataType = TYPE_INT;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : char *keyName - 
//			value - 
//-----------------------------------------------------------------------------
void KeyValues::SetFloat( const char* name, float value )
{
	KeyValues* key = FindKey( name, true );
	if ( key == nullptr )
		return;

	key->FreeAllocatedValue();
	key->m_flValue = value;
	key->m_iDataType = TYPE_FLOAT;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : char *keyName - 
//			value - 
//-----------------------------------------------------------------------------
void KeyValues::SetPtr( const char* name, void* value )
{
	KeyValues* key = FindKey( name, true );
	if ( key == nullptr )
		return;

	key->FreeAllocatedValue();
	key->m_pValue = value;
	key->m_iDataType = TYPE_PTR;
}

void KeyValues::SetColor( const char* name, Color value )
{
	KeyValues* key = FindKey( name, true );
	if ( key == nullptr )
		return;

	key->FreeAllocatedValue();
	key->m_Color[0] = static_cast<unsigned char>( value.r() );
	key->m_Color[1] = static_cast<unsigned char>( value.g() );
	key->m_Color[2] = static_cast<unsigned char>( value.b() );
	key->m_Color[3] = static_cast<unsigned char>( value.a() );
	key->m_iDataType = TYPE_COLOR;
}

void KeyValues::SetUint64( const char* name, uint64 value )
{
	KeyValues* key = FindKey( name, true );
	if ( key == nullptr )
		return;

	key->FreeAllocatedValue();
	key->m_pValue = keyvalues()->AllocKeyValuesMemory( static_cast<int>( sizeof( uint64 ) ) );
	key->m_iAllocationSize = static_cast<short>( sizeof( uint64 ) );
	std::memcpy( key->m_pValue, &value, sizeof( value ) );
	key->m_iDataType = TYPE_UINT64;
}

KeyValues* KeyValues::MakeCopy() const
{
	KeyValues* copy = new KeyValues( GetName() );
	copy->m_iDataType = m_iDataType;
	switch ( m_iDataType )
	{
	case TYPE_STRING:
		if ( m_sValue )
		{
			std::size_t size = std::strlen( m_sValue ) + 1;
			copy->AllocateValueBlock( static_cast<int>( size ) );
			std::memcpy( copy->m_pValue, m_pValue, size );
		}
		break;
	case TYPE_INT:
	case TYPE_FLOAT:
	case TYPE_PTR:
		std::memcpy( &copy->m_iValue, &m_iValue, sizeof( m_iValue ) );
		break;
	case TYPE_WSTRING:
		if ( m_pValue )
		{
			std::size_t size = ( std::wcslen( static_cast<const wchar_t*>( m_pValue ) ) + 1 ) * sizeof( wchar_t );
			copy->AllocateValueBlock( static_cast<int>( size ) );
			std::memcpy( copy->m_pValue, m_pValue, size );
		}
		break;
	case TYPE_COLOR:
		std::memcpy( copy->m_Color, m_Color, sizeof( m_Color ) );
		break;
	case TYPE_UINT64:
		copy->m_pValue = keyvalues()->AllocKeyValuesMemory( sizeof( uint64 ) );
		copy->m_iAllocationSize = sizeof( uint64 );
		std::memcpy( copy->m_pValue, m_pValue, sizeof( uint64 ) );
		break;
	default:
		break;
	}

	KeyValues* previous = nullptr;
	for ( KeyValues* child = m_pSub; child; child = child->m_pPeer )
	{
		KeyValues* childCopy = child->MakeCopy();
		if ( previous )
			previous->m_pPeer = childCopy;
		else
			copy->m_pSub = childCopy;
		childCopy->m_pPeer = nullptr;
		previous = childCopy;
	}
	return copy;
}

void KeyValues::Clear()
{
	KeyValues* child = m_pSub;
	while ( child )
	{
		KeyValues* next = child->m_pPeer;
		delete child;
		child = next;
	}
	m_pSub = nullptr;
	FreeAllocatedValue();
	m_iDataType = TYPE_NONE;
}

void KeyValues::deleteThis()
{
	delete this;
}

void KeyValues::LoadFromBuffer( const char* buffer )
{
	char* cursor = const_cast<char*>( buffer );
	char* token = ReadToken( &cursor );
	if ( token )
		m_iKeyName = keyvalues()->GetSymbolForString( token );
	token = ReadToken( &cursor );
	if ( m_iKeyName >= 0 && token && token[0] == '{' )
		RecursiveLoadFromBuffer( &cursor );
}

void KeyValues::RecursiveLoadFromBuffer( char** buffer )
{
	for ( char* token = ReadToken( buffer ); token; token = ReadToken( buffer ) )
	{
		if ( !token[0] || std::strcmp( token, "}" ) == 0 )
			return;
		KeyValues* child = FindKey( token, true );
		const char* value = ReadToken( buffer );
		if ( !value || std::strcmp( value, "}" ) == 0 )
			return;
		if ( std::strcmp( value, "{" ) == 0 )
		{
			child->RecursiveLoadFromBuffer( buffer );
		}
		else
		{
			std::size_t size = std::strlen( value ) + 1;
			FreeAllocatedValue();
			child->AllocateValueBlock( static_cast<int>( size ) );
			std::memcpy( child->m_pValue, value, size );
			child->m_iDataType = TYPE_STRING;
		}
	}
}

void KeyValues::WriteConvertedString( CUtlBuffer& buffer, const char* text )
{
	int length = static_cast<int>( std::strlen( text ) );
#if defined( _MSC_VER )
#pragma warning( suppress : 6255 )
#endif
	char* convertedString = static_cast<char*>( _alloca( ( length + 1 ) * sizeof( char ) * 2 ) );
	int j = 0;
	for ( int i = 0; i <= length; i++ )
	{
		if ( text[i] == '"' || text[i] == '\\' )
		{
			convertedString[j] = '\\';
			j++;
		}
		convertedString[j] = text[i];
		j++;
	}

	buffer.Put( convertedString, static_cast<int>( std::strlen( convertedString ) ) );
}

void KeyValues::RecursiveSaveToFile( IFileSystem* fileSystem, CUtlBuffer& buffer, int indent )
{
	for ( int tab = 0; tab < indent; ++tab )
		buffer.Put( "\t", 1 );
	buffer.Put( "\"", 1 );
	int nameLength = static_cast<int>( std::strlen( GetName() ) );
	buffer.Put( GetName(), nameLength );
	buffer.Put( "\"\n", 2 );
	for ( int tab = 0; tab < indent; ++tab )
		buffer.Put( "\t", 1 );
	buffer.Put( "{\n", 2 );

	for ( KeyValues* key = m_pSub; key != nullptr; key = key->m_pPeer )
	{
		if ( key->m_pSub != nullptr )
		{
			key->RecursiveSaveToFile( fileSystem, buffer, indent + 1 );
			continue;
		}

		char localized[1024];
		types_t type = static_cast<types_t>( key->m_iDataType );
		switch ( type )
		{
		case TYPE_STRING:
			if ( key->m_sValue == nullptr || key->m_sValue[0] == '\0' )
				continue;
			break;
		case TYPE_WSTRING:
			if ( key->m_sValue == nullptr )
				continue;
			keyvalues()->GetANSIFromLocalized(
				reinterpret_cast<const wchar_t*>( key->m_sValue ), localized, sizeof( localized ) );
			break;
		case TYPE_INT:
		case TYPE_FLOAT:
			break;
		default:
			continue;
		}

		for ( int tab = 0; tab < indent + 1; ++tab )
			buffer.Put( "\t", 1 );
		buffer.Put( "\"", 1 );
		nameLength = static_cast<int>( std::strlen( key->GetName() ) );
		buffer.Put( key->GetName(), nameLength );
		buffer.Put( "\"\t\t\"", 4 );

		switch ( type )
		{
		case TYPE_STRING:
			WriteConvertedString( buffer, key->m_sValue );
			break;
		case TYPE_WSTRING:
			WriteConvertedString( buffer, localized );
			break;
		case TYPE_INT:
		{
			char value[32];
			_snprintf( value, sizeof( value ), "%d", key->m_iValue );
			buffer.Put( value, static_cast<int>( std::strlen( value ) ) );
			break;
		}
		case TYPE_FLOAT:
		{
			char value[48];
			_snprintf( value, sizeof( value ), "%f", key->m_flValue );
			buffer.Put( value, static_cast<int>( std::strlen( value ) ) );
			break;
		}
		default:
			break;
		}
		buffer.Put( "\"\n", 2 );
	}

	for ( int tab = 0; tab < indent; ++tab )
		buffer.Put( "\t", 1 );
	buffer.Put( "}\n", 2 );
}
bool KeyValues::LoadFromFile( IFileSystem* filesystem, const char* resourceName, const char* pathID )
{
	ASSERT_AT( filesystem, 283 );
#ifdef _WIN32
	ASSERT_AT( _heapchk() == _HEAPOK, 284 );
#endif

	FileHandle_t file = filesystem->Open( resourceName, "rb", pathID );
	if ( !file )
		return false;
	unsigned int size = filesystem->Size( file );
	char* buffer = static_cast<char*>( std::malloc( static_cast<std::size_t>( size ) + 1 ) );
	ASSERT_AT( buffer, 294 );
	if ( !buffer )
		return false;
	buffer[size] = '\0';
	filesystem->Read( buffer, static_cast<int>( size ), file );

	LoadFromBuffer( buffer );
	filesystem->Close( file );
	std::free( buffer );
	return true;
}

bool KeyValues::SaveToFile( IFileSystem* filesystem, const char* resourceName, const char* pathID )
{
	FileHandle_t file = filesystem->Open( resourceName, "wb", pathID );
	if ( !file )
	{
		char directory[1024] = {};
		const char* component = resourceName;
		for ( const char* separator = std::strchr( resourceName, '\\' ); separator;
			separator = std::strchr( separator + 1, '\\' ) )
		{
			if ( std::strlen( directory ) + 1 + ( separator - component ) >= sizeof( directory ) )
				break;
			if ( directory[0] )
				std::strcat( directory, "\\" );
			std::strncat( directory, component, separator - component );
#if defined( _MSC_VER )
#pragma warning( suppress : 6031 )
#endif
			_mkdir( directory );
			component = separator + 1;
		}
		file = filesystem->Open( resourceName, "wb", pathID );
		if ( !file )
			return false;
	}

	CUtlBuffer buffer( 0, 4096, true );
	RecursiveSaveToFile( filesystem, buffer, 0 );
	buffer.PutChar( 0 );
	filesystem->Write( buffer.Base(), buffer.TellPut(), file );
	filesystem->Close( file );
	return true;
}
