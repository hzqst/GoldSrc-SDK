//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "iregistry.h"

#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include "UtlVector.h"

#include <cstdio>
#include <cstdlib>
#include <strings.h>
#endif

#if defined(_WIN32)
#define REGISTRY_VALUE_LENGTH	512
#define REGISTRY_KEY_LENGTH		1024

static const char kRegistryProduct[] = "Half-Life";
static const char kRegistryKeyFormat[] = "Software\\Valve\\%s\\Settings\\";
#else
#define REGISTRY_FIELD_LENGTH		64
#define REGISTRY_LINE_LENGTH		256
#define REGISTRY_FILENAME_LENGTH	4096
#define FORMATTED_INTEGER_LENGTH	32
#define DECIMAL_RADIX				10
#endif

//-----------------------------------------------------------------------------
// Purpose: Exposes registry interface to rest of launcher
//-----------------------------------------------------------------------------
class CRegistry : public IRegistry
{
public:
	CRegistry();
	virtual ~CRegistry();

	void Init() override;
	void Shutdown() override;

	int ReadInt( const char* key, int defaultValue ) override;
	void WriteInt( const char* key, int value ) override;
	const char* ReadString( const char* key, const char* defaultValue ) override;
	void WriteString( const char* key, const char* value ) override;

private:
#if !defined(_WIN32)
	struct KV
	{
		char	key[REGISTRY_FIELD_LENGTH];
		char	value[REGISTRY_FIELD_LENGTH];
	};

	void LoadKeyValuesFromDisk();
	void UpdateOrAppend( const char* key, const char* value );
#endif

	bool	m_bValid;
#if defined(_WIN32)
	HKEY	m_hKey;
#else
	int				m_hKey;
	FILE*			m_fConfig;
	CUtlVector<KV>	m_vecValues;
#endif
};

static CRegistry g_Registry;

IRegistry* registry = &g_Registry;

CRegistry::CRegistry()
	: m_bValid( false )
	, m_hKey( 0 )
#if !defined(_WIN32)
	, m_fConfig( NULL )
#endif
{
}

CRegistry::~CRegistry()
{
#if !defined(_WIN32)
	m_vecValues.Purge();
#endif
}

void CRegistry::Init()
{
#if defined(_WIN32)
	char subKey[REGISTRY_KEY_LENGTH];
	DWORD disposition;

	wsprintfA( subKey, kRegistryKeyFormat, kRegistryProduct );
	m_bValid = RegCreateKeyExA( HKEY_CURRENT_USER, subKey, 0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS, NULL, &m_hKey, &disposition ) == ERROR_SUCCESS;
#else
	LoadKeyValuesFromDisk();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CRegistry::Shutdown()
{
#if defined(_WIN32)
	if ( m_bValid )
	{
		// Make invalid
		m_bValid = false;
		RegCloseKey( m_hKey );
	}
#endif
}

int CRegistry::ReadInt( const char* key, int defaultValue )
{
#if defined(_WIN32)
	DWORD type;
	DWORD size;
	int data;

	if ( !m_bValid )
	{
		return defaultValue;
	}

	size = sizeof( DWORD );
	if ( RegQueryValueExA( m_hKey, key, NULL, &type, (BYTE*)&data, &size ) != ERROR_SUCCESS
		|| type != REG_DWORD )
	{
		return defaultValue;
	}

	return data;
#else
	LoadKeyValuesFromDisk();
	const int count = m_vecValues.Count();

	if ( count <= 0 )
	{
		return defaultValue;
	}

	const size_t keyLength = strlen( key );
	for ( int index = 0; index < count; ++index )
	{
		const KV& entry = m_vecValues[index];

		if ( strncasecmp( key, entry.key, keyLength ) == 0 )
		{
			return (int)strtol( entry.value, NULL, DECIMAL_RADIX );
		}
	}

	return defaultValue;
#endif
}

const char* CRegistry::ReadString( const char* key, const char* defaultValue )
{
#if defined(_WIN32)
	static char text[REGISTRY_VALUE_LENGTH];
	DWORD type;
	DWORD size = sizeof( text );

	text[0] = '\0';

	if ( !m_bValid )
	{
		return defaultValue;
	}

	if ( RegQueryValueExA( m_hKey, key, NULL, &type, (BYTE*)text, &size ) != ERROR_SUCCESS || type != REG_SZ )
	{
		return defaultValue;
	}

	return text;
#else
	LoadKeyValuesFromDisk();
	const int count = m_vecValues.Count();

	if ( count <= 0 )
	{
		return defaultValue;
	}

	const size_t keyLength = strlen( key );
	for ( int index = 0; index < count; ++index )
	{
		const KV& entry = m_vecValues[index];

		if ( strncasecmp( key, entry.key, keyLength ) == 0 )
		{
			return entry.value;
		}
	}

	return defaultValue;
#endif
}

void CRegistry::WriteInt( const char* key, int value )
{
#if defined(_WIN32)
	if ( m_bValid )
	{
		RegSetValueExA( m_hKey, key, 0, REG_DWORD, (const BYTE*)&value, sizeof( value ) );
	}
#else
	char formatted[FORMATTED_INTEGER_LENGTH];

	LoadKeyValuesFromDisk();
	snprintf( formatted, sizeof( formatted ), "%d", value );
	UpdateOrAppend( key, formatted );
#endif
}

void CRegistry::WriteString( const char* key, const char* value )
{
#if defined(_WIN32)
	if ( m_bValid )
	{
		RegSetValueExA( m_hKey, key, 0, REG_SZ, (const BYTE*)value, (DWORD)( strlen( value ) + 1 ) );
	}
#else
	LoadKeyValuesFromDisk();
	UpdateOrAppend( key, value );
#endif
}

#if !defined(_WIN32)
void CRegistry::LoadKeyValuesFromDisk()
{
	char line[REGISTRY_LINE_LENGTH];
	char fileName[REGISTRY_FILENAME_LENGTH];

	if ( m_fConfig != NULL )
	{
		fclose( m_fConfig );
	}

	m_vecValues.RemoveAll();
	snprintf( fileName, sizeof( fileName ), "%s.conf", "hl" );
	m_fConfig = fopen( fileName, "r" );

	if ( m_fConfig == NULL )
	{
		return;
	}

	while ( feof( m_fConfig ) == 0 )
	{
		char* equals;
		char* value;
		char* valueEnd;
		KV entry;

		if ( fgets( line, REGISTRY_LINE_LENGTH, m_fConfig ) == NULL )
		{
			break;
		}

		equals = strchr( line, '=' );
		if ( equals == NULL )
		{
			continue;
		}

		*equals = '\0';
		value = equals + 1;
		valueEnd = equals + strlen( value );
		if ( *valueEnd == '\n' )
		{
			*valueEnd = '\0';
		}

		strncpy( entry.key, line, REGISTRY_FIELD_LENGTH );
		strncpy( entry.value, value, REGISTRY_FIELD_LENGTH );
		m_vecValues.Add( entry );
	}

	fclose( m_fConfig );
	m_fConfig = NULL;
}

void CRegistry::UpdateOrAppend( const char* key, const char* value )
{
	int count = m_vecValues.Count();
	bool matched = false;

	if ( count > 0 )
	{
		for ( int index = 0; index < count; ++index )
		{
			KV& entry = m_vecValues[index];

			if ( strncasecmp( key, entry.key, strlen( key ) ) == 0 )
			{
				strncpy( entry.value, value, REGISTRY_FIELD_LENGTH );
				matched = true;
				count = m_vecValues.Count();
			}
		}
	}

	if ( matched )
	{
		return;
	}

	const int index = m_vecValues.AddToTail();
	strncpy( m_vecValues[index].key, key, REGISTRY_FIELD_LENGTH );
	strncpy( m_vecValues[index].value, value, REGISTRY_FIELD_LENGTH );
}
#endif // POSIX
