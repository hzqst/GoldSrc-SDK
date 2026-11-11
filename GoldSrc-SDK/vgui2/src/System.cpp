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

#include "interface.h"
#include "vgui_internal.h"
#include "vgui_key_translation.h"
#include "KeyValues.h"
#include "filesystem_helpers.h"
#include "vgui/ISystem.h"
#include "vgui/IInputInternal.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\src\\System.cpp" )
#undef GetCurrentTime
#undef ShellExecute

using namespace vgui2;

namespace vgui2
{
void vgui_strcpy( char* dst, int dstLen, const char* src );
}

class CSystem : public ISystem
{
public:
	CSystem();
	~CSystem() override;

	virtual void Shutdown() override;
	virtual void RunFrame() override;
	virtual void ShellExecute( const char* command, const char* file ) override;

	// returns the time at the start of the frame
	virtual double GetFrameTime() override;
	virtual double GetCurrentTime() override;
	virtual long GetTimeMillis() override;

	virtual int GetClipboardTextCount() override;
	virtual void SetClipboardText( const char* text, int textLen ) override;
	virtual void SetClipboardText( const wchar_t* text, int textLen ) override;
	virtual int GetClipboardText( int offset, char* buf, int bufLen ) override;
	virtual int GetClipboardText( int offset, wchar_t* buf, int bufLen ) override;

	virtual bool SetRegistryString( const char* key, const char* value ) override;
	virtual bool GetRegistryString( const char* key, char* value, int valueLen ) override;
	virtual bool SetRegistryInteger( const char* key, int value ) override;
	virtual bool GetRegistryInteger( const char* key, int& value ) override;

	virtual KeyValues* GetUserConfigFileData( const char* dialogName, int dialogID ) override;
	virtual void SetUserConfigFile( const char* fileName, const char* pathName ) override;
	virtual void SaveUserConfigFile() override;

	virtual bool SetWatchForComputerUse( bool state ) override;
	virtual double GetTimeSinceLastUse() override;
	virtual int GetAvailableDrives( char* buf, int bufLen ) override;

	virtual bool CommandLineParamExists( const char* paramName ) override;
	virtual const char* GetFullCommandLine() override;
	virtual bool GetCurrentTimeAndDate( int* year, int* month, int* dayOfWeek,
		int* day, int* hour, int* minute, int* second ) override;

	virtual double GetFreeDiskSpace( const char* path ) override;

	virtual bool CreateShortcut( const char* linkFileName, const char* targetPath,
		const char* arguments, const char* workingDirectory, const char* iconFile ) override;
	virtual bool GetShortcutTarget( const char* linkFileName, char* targetPath,
		char* arguments, int destBufferSizes ) override;
	virtual bool ModifyShortcutTarget( const char* linkFileName, const char* targetPath,
		const char* arguments, const char* workingDirectory ) override;

	virtual bool GetCommandLineParamValue( const char* paramName, char* value, int valueBufferSize ) override;
	virtual bool DeleteRegistryKey( const char* keyName ) override;

	virtual const char* GetDesktopFolderPath() override;
	virtual KeyCode KeyCode_VirtualKeyToVGUI( int keyCode ) override;
	virtual int KeyCode_VGUIToVirtualKey( KeyCode keyCode ) override;

private:
	// auto-away data
	bool		m_bStaticWatchForComputerUse;
	HHOOK		m_hStaticKeyboardHook;
	HHOOK		m_hStaticMouseHook;
	double		m_StaticLastComputerUseTime;
	int			m_iStaticMouseOldX;
	int			m_iStaticMouseOldY;

	// timer data
	double		m_flFrameTime;

	int			m_iLowShift;
	double		m_flPFreq;
	double		m_flCurTime;
	double		m_flLastCurTime;

	KeyValues*	m_pUserConfigData;
	char		m_szFileName[MAX_PATH];
	char		m_szPathID[MAX_PATH];

	CRITICAL_SECTION	m_TimeLock;
};

CSystem g_System;

namespace vgui2
{
ISystem* g_pSystem = &g_System;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSystem, ISystem, "VGUI_System009", g_System );

short System_GetKeyState( int virtualKeyCode )
{
	return GetKeyState( virtualKeyCode );
}

CSystem::CSystem()
{
	m_bStaticWatchForComputerUse = false;
	m_hStaticKeyboardHook = NULL;
	m_hStaticMouseHook = NULL;
	m_StaticLastComputerUseTime = 0.0;
	m_iStaticMouseOldX = -1;
	m_iStaticMouseOldY = -1;
	m_flFrameTime = 0.0;
	m_pUserConfigData = NULL;

	InitializeCriticalSection( &m_TimeLock );

	LARGE_INTEGER PerformanceFreq;
	QueryPerformanceFrequency( &PerformanceFreq );

	unsigned int lowpart = (unsigned int)PerformanceFreq.LowPart;
	unsigned int highpart = (unsigned int)PerformanceFreq.HighPart;
	m_iLowShift = 0;

	while ( highpart || ( lowpart > 2000000.0 ) )
	{
		m_iLowShift++;
		lowpart >>= 1;
		lowpart |= ( highpart & 1 ) << 31;
		highpart >>= 1;
	}

	m_flPFreq = 1.0 / (double)lowpart;
}

CSystem::~CSystem()
{
	DeleteCriticalSection( &m_TimeLock );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSystem::Shutdown()
{
	if ( m_pUserConfigData )
	{
		m_pUserConfigData->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles all the per frame actions
//-----------------------------------------------------------------------------
void CSystem::RunFrame()
{
	// record the current frame time
	m_flFrameTime = GetCurrentTime();

	if ( m_bStaticWatchForComputerUse )
	{
		// check for mouse movement
		int x, y;
		g_pInput->GetCursorPos( x, y );

		// allow a little slack for jittery mice, don't reset until it's moved more than fifty pixels
		if ( abs( ( x + y ) - ( m_iStaticMouseOldX + m_iStaticMouseOldY ) ) > 50 )
		{
			m_StaticLastComputerUseTime = GetTimeMillis() / 1000.0;
			m_iStaticMouseOldX = x;
			m_iStaticMouseOldY = y;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the time at the start of the frame
//-----------------------------------------------------------------------------
double CSystem::GetFrameTime()
{
	return m_flFrameTime;
}

double CSystem::GetCurrentTime()
{
	static int first = 1;
	static int sametimecount;
	static unsigned int oldtime;

	EnterCriticalSection( &m_TimeLock );

	LARGE_INTEGER PerformanceCount;
	QueryPerformanceCounter( &PerformanceCount );

	unsigned int temp;
	if ( m_iLowShift == 0 )
	{
		temp = (unsigned int)PerformanceCount.LowPart;
	}
	else
	{
		temp = ( (unsigned int)PerformanceCount.LowPart >> m_iLowShift )
			| ( (unsigned int)PerformanceCount.HighPart << ( 32 - m_iLowShift ) );
	}

	if ( first )
	{
		oldtime = temp;
		first = 0;
	}
	else if ( ( temp <= oldtime ) && ( ( oldtime - temp ) < 0x10000000 ) )
	{
		oldtime = temp;
	}
	else
	{
		unsigned int t2 = temp - oldtime;
		double time = (double)t2 * m_flPFreq;
		oldtime = temp;

		m_flCurTime += time;

		if ( m_flCurTime == m_flLastCurTime )
		{
			sametimecount++;
			if ( sametimecount > 100000 )
			{
				m_flCurTime += 1.0;
				sametimecount = 0;
			}
		}
		else
		{
			sametimecount = 0;
		}

		m_flLastCurTime = m_flCurTime;
	}

	LeaveCriticalSection( &m_TimeLock );
	return m_flCurTime;
}

//-----------------------------------------------------------------------------
// Purpose: returns the current time in milliseconds
//-----------------------------------------------------------------------------
long CSystem::GetTimeMillis()
{
	return (long)( GetCurrentTime() * 1000.0 );
}

void CSystem::ShellExecute( const char* command, const char* file )
{
	::ShellExecuteA( NULL, command, file, NULL, NULL, SW_SHOWNORMAL );
}

int CSystem::GetClipboardTextCount()
{
	int count = 0;
	if ( OpenClipboard( NULL ) )
	{
		HANDLE hmem = GetClipboardData( CF_TEXT );
		if ( hmem )
		{
			count = (int)GlobalSize( hmem );
		}

		CloseClipboard();
	}

	return count;
}

void CSystem::SetClipboardText( const char* text, int textLen )
{
	if ( !text )
	{
		return;
	}

	if ( textLen <= 0 )
	{
		return;
	}

	if ( !OpenClipboard( NULL ) )
	{
		return;
	}

	EmptyClipboard();

	HANDLE hmem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DDESHARE, textLen + 1 );
	if ( hmem )
	{
		void* ptr = GlobalLock( hmem );
		if ( ptr )
		{
			memset( ptr, 0, textLen + 1 );
			memcpy( ptr, text, textLen );
			GlobalUnlock( hmem );

			SetClipboardData( CF_TEXT, hmem );
		}
	}

	CloseClipboard();
}

//-----------------------------------------------------------------------------
// Purpose: Puts unicode text into the clipboard
//-----------------------------------------------------------------------------
void CSystem::SetClipboardText( const wchar_t* text, int textLen )
{
	if ( !text )
	{
		return;
	}

	if ( textLen <= 0 )
	{
		return;
	}

	if ( !OpenClipboard( NULL ) )
	{
		return;
	}

	EmptyClipboard();

	HANDLE hmem = GlobalAlloc( GMEM_MOVEABLE | GMEM_DDESHARE, ( textLen + 1 ) * sizeof( wchar_t ) );
	if ( hmem )
	{
		void* ptr = GlobalLock( hmem );
		if ( ptr )
		{
			memset( ptr, 0, ( textLen + 1 ) * sizeof( wchar_t ) );
			memcpy( ptr, text, textLen * sizeof( wchar_t ) );
			GlobalUnlock( hmem );

			SetClipboardData( CF_UNICODETEXT, hmem );
		}
	}

	CloseClipboard();
}

int CSystem::GetClipboardText( int offset, char* buf, int bufLen )
{
	if ( !buf )
	{
		return 0;
	}

	if ( bufLen <= 0 )
	{
		return 0;
	}

	int count = 0;
	if ( !OpenClipboard( NULL ) )
	{
		return 0;
	}

	HANDLE hmem = GetClipboardData( CF_UNICODETEXT );
	if ( hmem )
	{
		count = (int)GlobalSize( hmem ) - offset;
		if ( count <= 0 )
		{
			CloseClipboard();
			return 0;
		}

		if ( bufLen < count )
		{
			count = bufLen;
		}

		void* ptr = GlobalLock( hmem );
		if ( ptr )
		{
			memcpy( buf, (char*)ptr + offset, count );
			GlobalUnlock( hmem );
		}
	}

	CloseClipboard();
	return count;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves unicode text from the clipboard
//-----------------------------------------------------------------------------
int CSystem::GetClipboardText( int offset, wchar_t* buf, int bufLen )
{
	if ( !buf )
	{
		return 0;
	}

	if ( bufLen <= 0 )
	{
		return 0;
	}

	int count = 0;
	if ( !OpenClipboard( NULL ) )
	{
		return 0;
	}

	HANDLE hmem = GetClipboardData( CF_UNICODETEXT );
	if ( hmem )
	{
		count = (int)GlobalSize( hmem ) - offset;
		if ( count <= 0 )
		{
			CloseClipboard();
			return 0;
		}

		if ( bufLen < count )
		{
			count = bufLen;
		}

		void* ptr = GlobalLock( hmem );
		if ( ptr )
		{
			memcpy( buf, (wchar_t*)ptr + offset, count );
			GlobalUnlock( hmem );
		}
	}

	CloseClipboard();
	return (int)( count / sizeof( wchar_t ) );
}

static bool staticSplitRegistryKey( const char* key, char* key0, int key0Len, char* key1, int key1Len )
{
	if ( !key )
	{
		return false;
	}

	int len = (int)strlen( key );
	if ( len <= 0 )
	{
		return false;
	}

	int start = -1;
	for ( int i = len - 1; i >= 0; i-- )
	{
		if ( key[i] == '\\' )
		{
			break;
		}

		start = i;
	}

	if ( start == -1 )
	{
		return false;
	}

	(void)key0Len;
	(void)key1Len;
	vgui_strcpy( key0, start + 1, key );
	vgui_strcpy( key1, len - start + 1, key + start );
	return true;
}

static HKEY staticRegistrySlot( const char*& key )
{
	HKEY hSlot = HKEY_CURRENT_USER;
	if ( !strncmp( key, "HKEY_LOCAL_MACHINE", 18 ) )
	{
		hSlot = HKEY_LOCAL_MACHINE;
		key += 19;
	}
	else if ( !strncmp( key, "HKEY_CURRENT_USER", 17 ) )
	{
		hSlot = HKEY_CURRENT_USER;
		key += 18;
	}

	return hSlot;
}

bool CSystem::SetRegistryString( const char* key, const char* value )
{
	HKEY hSlot = staticRegistrySlot( key );

	char key0[256], key1[256];
	if ( !staticSplitRegistryKey( key, key0, sizeof( key0 ), key1, sizeof( key1 ) ) )
	{
		return false;
	}

	HKEY hKey;
	if ( RegCreateKeyExA( hSlot, key0, 0, NULL, REG_OPTION_NON_VOLATILE, value ? KEY_WRITE : KEY_ALL_ACCESS,
		NULL, &hKey, NULL ) != ERROR_SUCCESS )
	{
		return false;
	}

	if ( RegSetValueExA( hKey, key1, 0, REG_SZ, (const BYTE*)value, (DWORD)strlen( value ) + 1 ) == ERROR_SUCCESS )
	{
		RegCloseKey( hKey );
		return true;
	}

	RegCloseKey( hKey );
	return false;
}

bool CSystem::GetRegistryString( const char* key, char* value, int valueLen )
{
	if ( !value )
	{
		return false;
	}

	value[0] = 0;

	HKEY hSlot = staticRegistrySlot( key );

	char key0[256], key1[256];
	if ( !staticSplitRegistryKey( key, key0, sizeof( key0 ), key1, sizeof( key1 ) ) )
	{
		return false;
	}

	HKEY hKey;
	if ( RegOpenKeyExA( hSlot, key0, 0, KEY_READ, &hKey ) != ERROR_SUCCESS )
	{
		return false;
	}

	DWORD len = valueLen;
	if ( RegQueryValueExA( hKey, key1, NULL, NULL, (BYTE*)value, &len ) == ERROR_SUCCESS )
	{
		RegCloseKey( hKey );
		return true;
	}

	RegCloseKey( hKey );
	return false;
}

bool CSystem::SetRegistryInteger( const char* key, int value )
{
	HKEY hSlot = staticRegistrySlot( key );

	char key0[256], key1[256];
	if ( !staticSplitRegistryKey( key, key0, sizeof( key0 ), key1, sizeof( key1 ) ) )
	{
		return false;
	}

	HKEY hKey;
	if ( RegCreateKeyExA( hSlot, key0, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL ) != ERROR_SUCCESS )
	{
		return false;
	}

	if ( RegSetValueExA( hKey, key1, 0, REG_DWORD, (const BYTE*)&value, 4 ) == ERROR_SUCCESS )
	{
		RegCloseKey( hKey );
		return true;
	}

	RegCloseKey( hKey );
	return false;
}

bool CSystem::GetRegistryInteger( const char* key, int& value )
{
	HKEY hSlot = staticRegistrySlot( key );

	char key0[256], key1[256];
	if ( !staticSplitRegistryKey( key, key0, sizeof( key0 ), key1, sizeof( key1 ) ) )
	{
		return false;
	}

	HKEY hKey;
	if ( RegOpenKeyExA( hSlot, key0, 0, KEY_READ, &hKey ) != ERROR_SUCCESS )
	{
		return false;
	}

	DWORD len = 4;
	if ( RegQueryValueExA( hKey, key1, NULL, NULL, (BYTE*)&value, &len ) == ERROR_SUCCESS )
	{
		RegCloseKey( hKey );
		return true;
	}

	RegCloseKey( hKey );
	return false;
}

bool CSystem::DeleteRegistryKey( const char* key )
{
	HKEY hSlot = staticRegistrySlot( key );

	if ( SHDeleteKeyA( hSlot, key ) == ERROR_SUCCESS )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: user config
//-----------------------------------------------------------------------------
KeyValues* CSystem::GetUserConfigFileData( const char* dialogName, int dialogID )
{
	if ( !m_pUserConfigData )
	{
		return NULL;
	}

	ASSERT_AT( dialogName && *dialogName, 788 );

	char buf[256];
	if ( dialogID )
	{
		_snprintf( buf, sizeof( buf ) - 1, "%s_%d", dialogName, dialogID );
		dialogName = buf;
	}

	return m_pUserConfigData->FindKey( dialogName, true );
}

//-----------------------------------------------------------------------------
// Purpose: sets the name of the config file to save/restore from.  Settings are loaded immediately.
//-----------------------------------------------------------------------------
void CSystem::SetUserConfigFile( const char* fileName, const char* pathName )
{
	if ( !m_pUserConfigData )
	{
		m_pUserConfigData = new KeyValues( "UserConfigData" );
	}

	strncpy( m_szFileName, fileName, sizeof( m_szFileName ) - 1 );
	strncpy( m_szPathID, pathName, sizeof( m_szPathID ) - 1 );

	m_pUserConfigData->LoadFromFile( g_pFileSystem, m_szFileName, m_szPathID );
}

//-----------------------------------------------------------------------------
// Purpose: saves all the current settings to the user config file
//-----------------------------------------------------------------------------
void CSystem::SaveUserConfigFile()
{
	if ( m_pUserConfigData )
	{
		m_pUserConfigData->SaveToFile( g_pFileSystem, m_szFileName, m_szPathID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets whether or not the app watches for global computer use
// Input  : state - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSystem::SetWatchForComputerUse( bool state )
{
	if ( state == m_bStaticWatchForComputerUse )
	{
		return true;
	}

	m_bStaticWatchForComputerUse = state;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns the time, in seconds, since the last computer use.
// Output : double
//-----------------------------------------------------------------------------
double CSystem::GetTimeSinceLastUse()
{
	if ( m_bStaticWatchForComputerUse )
	{
		return ( GetTimeMillis() / 1000.0 ) - m_StaticLastComputerUseTime;
	}

	return 0.0;
}

//-----------------------------------------------------------------------------
// Purpose: Get the drives a user has available on thier system
//-----------------------------------------------------------------------------
int CSystem::GetAvailableDrives( char* buf, int bufLen )
{
	return GetLogicalDriveStringsA( bufLen, buf );
}

//-----------------------------------------------------------------------------
// Purpose: returns whether or not the parameter was on the command line
//-----------------------------------------------------------------------------
bool CSystem::CommandLineParamExists( const char* paramName )
{
	const char* cmdLine = GetFullCommandLine();
	const char* loc = strstr( cmdLine, paramName );
	return ( loc != NULL );
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the currently running exe
//-----------------------------------------------------------------------------
const char* CSystem::GetFullCommandLine()
{
	return GetCommandLineA();
}

bool CSystem::GetCommandLineParamValue( const char* paramName, char* value, int valueBufferSize )
{
	const char* cmdLine = GetFullCommandLine();
	const char* loc = strstr( cmdLine, paramName );
	if ( !loc )
	{
		return false;
	}

	loc += strlen( paramName );

	char token[512];
	ParseFile( loc, token, NULL, NULL );
	strncpy( value, token, valueBufferSize - 1 );
	value[valueBufferSize - 1] = 0;
	return true;
}

bool CSystem::GetCurrentTimeAndDate( int* year, int* month, int* dayOfWeek, int* day, int* hour, int* minute, int* second )
{
	SYSTEMTIME time;
	GetLocalTime( &time );

	if ( year )
	{
		*year = time.wYear;
	}
	if ( month )
	{
		*month = time.wMonth;
	}
	if ( dayOfWeek )
	{
		*dayOfWeek = time.wDayOfWeek;
	}
	if ( day )
	{
		*day = time.wDay;
	}
	if ( hour )
	{
		*hour = time.wHour;
	}
	if ( minute )
	{
		*minute = time.wMinute;
	}
	if ( second )
	{
		*second = time.wSecond;
	}

	return true;
}

double CSystem::GetFreeDiskSpace( const char* path )
{
	char buf[MAX_PATH];
	strcpy( buf, path );

	char* slash = strstr( buf, "\\" );
	if ( slash )
	{
		slash[1] = 0;
	}

	ULARGE_INTEGER userFreeBytes, totalBytes, totalFreeBytes;
	if ( GetDiskFreeSpaceExA( buf, &userFreeBytes, &totalBytes, &totalFreeBytes ) )
	{
		return (double)(__int64)userFreeBytes.QuadPart;
	}

	return 0.0;
}

bool CSystem::CreateShortcut( const char* linkFileName, const char* targetPath, const char* arguments,
	const char* workingDirectory, const char* iconFile )
{
	bool bSuccess = false;
	char temp[MAX_PATH];
	strcpy( temp, linkFileName );

	struct _stat statBuf;
	if ( _stat( linkFileName, &statBuf ) != -1 )
	{
		return false;
	}

	IShellLinkA* psl;
	HRESULT hres = CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (LPVOID*)&psl );
	if ( SUCCEEDED( hres ) )
	{
		psl->SetPath( targetPath );
		psl->SetArguments( arguments );
		if ( workingDirectory && *workingDirectory )
		{
			psl->SetWorkingDirectory( workingDirectory );
		}
		if ( iconFile && *iconFile )
		{
			psl->SetIconLocation( iconFile, 0 );
		}

		IPersistFile* ppf;
		hres = psl->QueryInterface( IID_IPersistFile, (LPVOID*)&ppf );
		if ( SUCCEEDED( hres ) )
		{
			wchar_t wsz[MAX_PATH];
			MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, temp, -1, wsz, MAX_PATH );

			hres = ppf->Save( wsz, TRUE );
			if ( SUCCEEDED( hres ) )
			{
				bSuccess = true;
			}
			ppf->Release();
		}
		psl->Release();
	}

	return bSuccess;
}

bool CSystem::GetShortcutTarget( const char* linkFileName, char* targetPath, char* arguments, int destBufferSizes )
{
	char temp[MAX_PATH];
	strcpy( temp, linkFileName );
	_strlwr( temp );

	targetPath[0] = 0;
	arguments[0] = 0;

	IShellLinkA* psl;
	HRESULT hres = CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (LPVOID*)&psl );
	if ( SUCCEEDED( hres ) )
	{
		IPersistFile* ppf;
		hres = psl->QueryInterface( IID_IPersistFile, (LPVOID*)&ppf );
		if ( SUCCEEDED( hres ) )
		{
			wchar_t wsz[MAX_PATH];
			MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, temp, -1, wsz, MAX_PATH );

			hres = ppf->Load( wsz, 0 );
			if ( SUCCEEDED( hres ) )
			{
				psl->GetPath( targetPath, destBufferSizes, NULL, SLGP_UNCPRIORITY );
				psl->GetArguments( arguments, destBufferSizes );
			}
			ppf->Release();
		}
		psl->Release();
	}

	return ( targetPath[0] != 0 );
}

bool CSystem::ModifyShortcutTarget( const char* linkFileName, const char* targetPath, const char* arguments,
	const char* workingDirectory )
{
	bool bSuccess = false;
	char temp[MAX_PATH];
	strcpy( temp, linkFileName );
	_strlwr( temp );

	IShellLinkA* psl;
	HRESULT hres = CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (LPVOID*)&psl );
	if ( SUCCEEDED( hres ) )
	{
		IPersistFile* ppf;
		hres = psl->QueryInterface( IID_IPersistFile, (LPVOID*)&ppf );
		if ( SUCCEEDED( hres ) )
		{
			wchar_t wsz[MAX_PATH];
			MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, temp, -1, wsz, MAX_PATH );

			hres = ppf->Load( wsz, 0 );
			if ( SUCCEEDED( hres ) )
			{
				psl->SetPath( targetPath );
				psl->SetArguments( arguments );
				psl->SetWorkingDirectory( workingDirectory );
				bSuccess = true;
				ppf->Save( wsz, TRUE );
			}
			ppf->Release();
		}
		psl->Release();
	}

	return bSuccess;
}

const char* CSystem::GetDesktopFolderPath()
{
	static char folderPath[MAX_PATH];
	folderPath[0] = 0;

	if ( GetRegistryString( "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders\\Desktop",
		folderPath, sizeof( folderPath ) ) && strlen( folderPath ) > 6 )
	{
		return folderPath;
	}

	if ( SHGetSpecialFolderPathA( NULL, folderPath, CSIDL_DESKTOP, false ) && strlen( folderPath ) > 6 )
	{
		return folderPath;
	}

	return NULL;
}

KeyCode CSystem::KeyCode_VirtualKeyToVGUI( int keyCode )
{
	return ::KeyCode_VirtualKeyToVGUI( keyCode );
}

int CSystem::KeyCode_VGUIToVirtualKey( KeyCode keyCode )
{
	return ::KeyCode_VGUIToVirtualKey( keyCode );
}
