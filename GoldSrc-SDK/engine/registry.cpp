//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "registry.h"

#include "common.h"
#include "utlvector.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined( _MSC_VER )
#define strncasecmp _strnicmp
#endif

#if defined( _WIN32 )

#include <Windows.h>

//-----------------------------------------------------------------------------
// Purpose: Exposes registry interface to rest of launcher
//-----------------------------------------------------------------------------
class CRegistry : public IRegistry
{
public:
	CRegistry();
	void Init() override;
	void Shutdown() override;
	int ReadInt( const char* key, int defaultValue ) override;
	void WriteInt( const char* key, int value ) override;
	const char* ReadString( const char* key, const char* defaultValue ) override;
	void WriteString( const char* key, const char* value ) override;
	virtual ~CRegistry();

private:
	bool m_bValid;
	HKEY m_hKey;
};

static CRegistry g_Registry;
IRegistry* registry = &g_Registry;

CRegistry::CRegistry() : m_bValid( false ), m_hKey( NULL )
{
}

CRegistry::~CRegistry()
{
}

void CRegistry::Init()
{
	char keyName[1024];
	DWORD disposition;

	wsprintfA( keyName, "Software\\Valve\\%s\\Settings\\", "Half-Life" );
	m_bValid = RegCreateKeyExA( HKEY_CURRENT_USER, keyName, 0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS, NULL, &m_hKey, &disposition ) == ERROR_SUCCESS;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRegistry::Shutdown()
{
	if ( m_bValid )
	{
		// Make invalid
		m_bValid = false;
		RegCloseKey( m_hKey );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Read integer from registry
// Input  : *key - 
//			defaultValue - 
// Output : int
//-----------------------------------------------------------------------------
int CRegistry::ReadInt( const char* key, int defaultValue )
{
	DWORD type;
	DWORD size = sizeof( int );
	int value;

	if ( !m_bValid )
		return defaultValue;

	if ( RegQueryValueExA( m_hKey, key, NULL, &type, reinterpret_cast<BYTE*>( &value ), &size ) != ERROR_SUCCESS
		|| type != REG_DWORD )
		return defaultValue;

	return value;
}

//-----------------------------------------------------------------------------
// Purpose: Read string value from registry
// Input  : *key - 
//			*defaultValue - 
// Output : const char
//-----------------------------------------------------------------------------
const char* CRegistry::ReadString( const char* key, const char* defaultValue )
{
	static char value[512];
	DWORD type;
	DWORD size = sizeof( value );

	value[0] = '\0';
	if ( !m_bValid )
		return defaultValue;

	if ( RegQueryValueExA( m_hKey, key, NULL, &type, reinterpret_cast<BYTE*>( value ), &size ) != ERROR_SUCCESS
		|| type != REG_SZ )
		return defaultValue;

	return value;
}

//-----------------------------------------------------------------------------
// Purpose: Save integer to registry
// Input  : *key - 
//			value - 
//-----------------------------------------------------------------------------
void CRegistry::WriteInt( const char* key, int value )
{
	if ( m_bValid )
		RegSetValueExA( m_hKey, key, 0, REG_DWORD, reinterpret_cast<const BYTE*>( &value ), sizeof( value ) );
}

void CRegistry::WriteString( const char* key, const char* value )
{
	if ( m_bValid )
		RegSetValueExA( m_hKey, key, 0, REG_SZ, reinterpret_cast<const BYTE*>( value ), static_cast<DWORD>( strlen( value ) + 1 ) );
}

#else

class CRegistry : public IRegistry
{
public:
	CRegistry();
	void Init() override;
	void Shutdown() override;
	int ReadInt( const char* key, int defaultValue ) override;
	void WriteInt( const char* key, int value ) override;
	const char* ReadString( const char* key, const char* defaultValue ) override;
	void WriteString( const char* key, const char* value ) override;
	void GetKeyValues();
	virtual ~CRegistry();

private:
	struct KV
	{
		char key[64];
		char value[64];
	};

	void LoadKeyValuesFromDisk();
	void WriteKeyValuesToDisk();
	void StoreKeyValue( const char* key, const char* value );

	bool m_bValid;
	int m_hKey;
	FILE* m_fConfig;
	CUtlVector<KV> m_vecValues;
};

static CRegistry g_Registry;
IRegistry* registry = &g_Registry;

void CRegistry::Init()
{
	LoadKeyValuesFromDisk();
}

void CRegistry::Shutdown()
{
}

int CRegistry::ReadInt( const char* key, int defaultValue )
{
	const char* value = ReadString( key, NULL );
	if ( !value )
		return defaultValue;

	return (int)strtol( value, NULL, 10 );
}

const char* CRegistry::ReadString( const char* key, const char* defaultValue )
{
	size_t len;
	int i;

	GetKeyValues();

	if ( m_vecValues.Count() <= 0 )
		return defaultValue;

	len = strlen( key );
	for ( i = 0; i < m_vecValues.Count(); i++ )
	{
		if ( !strncasecmp( key, m_vecValues[i].key, len ) )
			return m_vecValues[i].value;
	}

	return defaultValue;
}

void CRegistry::WriteInt( const char* key, int value )
{
	char buf[32];

	LoadKeyValuesFromDisk();
	snprintf( buf, sizeof( buf ), "%d", value );
	StoreKeyValue( key, buf );
}

void CRegistry::WriteString( const char* key, const char* value )
{
	LoadKeyValuesFromDisk();
	StoreKeyValue( key, value );
}

void CRegistry::StoreKeyValue( const char* key, const char* value )
{
	bool found = false;
	size_t len = strlen( key );
	int i;

	for ( i = 0; i < m_vecValues.Count(); i++ )
	{
		KV& kv = m_vecValues[i];
		if ( !strncasecmp( key, kv.key, len ) )
		{
			strncpy( kv.value, value, sizeof( kv.value ) );
			found = true;
		}
	}

	if ( !found )
	{
		KV& kv = m_vecValues[m_vecValues.AddToTail()];
		strncpy( kv.key, key, sizeof( kv.key ) );
		strncpy( kv.value, value, sizeof( kv.value ) );
	}

	WriteKeyValuesToDisk();
}

void Sys_GetRegKeyValue( const char*, const char* valueName, char* output, int maxBytes, const char* defaultValue )
{
	if ( maxBytes <= 0 )
		return;

	const char* value = g_Registry.ReadString( valueName, defaultValue );
	if ( !value )
		value = "";

	Q_strncpy( output, value, maxBytes );
	output[maxBytes - 1] = '\0';
}

CRegistry::CRegistry() : m_bValid( false ), m_hKey( 0 ), m_fConfig( NULL )
{
}

CRegistry::~CRegistry()
{
}

void CRegistry::GetKeyValues()
{
	if ( !m_vecValues.Count() )
		LoadKeyValuesFromDisk();
}

void CRegistry::LoadKeyValuesFromDisk()
{
	char line[256];
	char* value;
	char* end;
	KV kv;

	if ( m_fConfig )
		fclose( m_fConfig );

	m_vecValues.RemoveAll();

	m_fConfig = fopen( "hl.conf", "r" );
	if ( !m_fConfig )
		return;

	while ( !feof( m_fConfig ) )
	{
		if ( !fgets( line, sizeof( line ), m_fConfig ) )
			break;

		value = strchr( line, '=' );
		if ( !value )
			continue;

		*value++ = '\0';

		end = value + strlen( value ) - 1;
		if ( *end == '\n' )
			*end = '\0';

		strncpy( kv.key, line, sizeof( kv.key ) );
		strncpy( kv.value, value, sizeof( kv.value ) );
		m_vecValues.AddToTail( kv );
	}

	fclose( m_fConfig );
	m_fConfig = NULL;
}

void CRegistry::WriteKeyValuesToDisk()
{
	int i;

	if ( !m_vecValues.Count() )
		return;

	m_fConfig = fopen( "hl.conf", "w+" );
	for ( i = 0; i < m_vecValues.Count(); i++ )
		fprintf( m_fConfig, "%s=%s\n", m_vecValues[i].key, m_vecValues[i].value );

	fclose( m_fConfig );
	m_fConfig = NULL;
}

#endif // POSIX
