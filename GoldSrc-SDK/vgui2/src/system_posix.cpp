#include "interface.h"
#include "vgui_internal.h"
#include "vgui_key_translation.h"
#include "KeyValues.h"
#include "vgui/ISystem.h"
#include "vgui/IInputInternal.h"
#include "LocalizedStringTable.h"
#include "ClipboardText.h"
#include "CommandLineArgs.h"

#include "SDL_clipboard.h"
#include "SDL_stdinc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <memory>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/statvfs.h>
#include <sys/time.h>
#include <time.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#undef GetCurrentTime
#undef ShellExecute
#endif

using namespace vgui2;

// the SDL clipboard is always UTF-8, so convert with the string table rather than the process locale
// returns the bytes written including the terminator, or the bytes required for a null buffer
static int Q_UnicodeToUTF8( const wchar_t* pUnicode, char* pUTF8, int cubDestSizeInBytes )
{
	return g_StringTable.ConvertUnicodeToANSI( pUnicode, pUTF8, cubDestSizeInBytes );
}

// returns the wide characters written including the terminator, also when the text is cut to fit
static int Q_UTF8ToUnicode( const char* pUTF8, wchar_t* pwchDest, int cubDestSizeInBytes )
{
	int cchResult = g_StringTable.ConvertANSIToUnicode( pUTF8, pwchDest, cubDestSizeInBytes );
	if ( cchResult == 0 && pwchDest && cubDestSizeInBytes >= (int)sizeof( wchar_t ) )
	{
		cchResult = (int)wcslen( pwchDest ) + 1;
	}
	return cchResult;
}

#ifdef _WIN32
#pragma warning( push )
#pragma warning( disable: 4121 )
#pragma pack( push, 4 )
#endif
class CSystem : public ISystem
{
public:
	CSystem();
	~CSystem() override;

	virtual void Shutdown() override;
	virtual void RunFrame() override;
	virtual void ShellExecute( const char* command, const char* file ) override;

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

	virtual const char* GetStartMenuFolderPath();
	virtual const char* GetAllUserDesktopFolderPath();
	virtual const char* GetAllUserStartMenuFolderPath();

private:
	bool		m_bStaticWatchForComputerUse;
	double		m_StaticLastComputerUseTime;
	int			m_iStaticMouseOldX;
	int			m_iStaticMouseOldY;

	double		m_flFrameTime;

	KeyValues*	m_pUserConfigData;
	char		m_szFileName[4096];
	char		m_szPathID[4096];
};
#ifdef _WIN32
#pragma pack( pop )
#pragma warning( pop )
#endif

CSystem g_System;
namespace vgui2
{
ISystem* g_pSystem = &g_System;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSystem, ISystem, "VGUI_System009", g_System );

CSystem::CSystem()
{
	m_bStaticWatchForComputerUse = false;
	m_StaticLastComputerUseTime = 0.0;
	m_iStaticMouseOldX = 0;
	m_iStaticMouseOldY = 0;
	m_flFrameTime = 0.0;
	m_pUserConfigData = NULL;
	memset( m_szFileName, 0, sizeof( m_szFileName ) );
	memset( m_szPathID, 0, sizeof( m_szPathID ) );
}

CSystem::~CSystem()
{
}

void CSystem::Shutdown()
{
	if ( m_pUserConfigData )
	{
		m_pUserConfigData->deleteThis();
	}
}

void CSystem::RunFrame()
{
	int x;
	int y;

	m_flFrameTime = GetCurrentTime();

	if ( !m_bStaticWatchForComputerUse )
	{
		return;
	}

	g_pInput->GetCursorPos( x, y );

	if ( abs( ( x + y ) - ( m_iStaticMouseOldX + m_iStaticMouseOldY ) ) > 50 )
	{
		m_StaticLastComputerUseTime = GetCurrentTime();
		m_iStaticMouseOldX = x;
		m_iStaticMouseOldY = y;
	}
}

double CSystem::GetFrameTime()
{
	return m_flFrameTime;
}

double CSystem::GetCurrentTime()
{
#ifdef _WIN32
	FILETIME fileTime;
	ULARGE_INTEGER ticks;
	GetSystemTimeAsFileTime( &fileTime );
	ticks.LowPart = fileTime.dwLowDateTime;
	ticks.HighPart = fileTime.dwHighDateTime;

	const unsigned long long ticksPerSecond = 10000000;
	const unsigned long long unixEpochSeconds = 11644473600;
	long long seconds = (long long)( ticks.QuadPart / ticksPerSecond ) - unixEpochSeconds;
	unsigned long microseconds = (unsigned long)( ( ticks.QuadPart % ticksPerSecond ) / 10 );
	static long long secbase;
	if ( !secbase )
	{
		secbase = seconds;
	}

	return ( seconds - secbase ) + microseconds / 1000000.0f;
#else
	struct timeval tp;
	static int secbase;

	gettimeofday( &tp, NULL );

	if ( !secbase )
	{
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000000.0f;
	}

	return ( tp.tv_sec - secbase ) + tp.tv_usec / 1000000.0f;
#endif
}

long CSystem::GetTimeMillis()
{
	return (long)( GetCurrentTime() * 1000.0f );
}

void CSystem::ShellExecute( const char* command, const char* file )
{
#ifdef _WIN32
	::ShellExecuteA( NULL, command, file, NULL, NULL, SW_SHOWNORMAL );
#else
	char szRealCommand[1024];

	snprintf( szRealCommand, sizeof( szRealCommand ), "xdg-open %s", file );
	system( szRealCommand );
#endif
}

void CSystem::SetClipboardText( const char* text, int textLen )
{
	if ( !text || textLen <= 0 )
	{
		return;
	}

	// copy at most textLen bytes, as on Windows
	size_t length = strnlen( text, textLen );
	std::unique_ptr<char[]> copy( new char[length + 1] );
	memcpy( copy.get(), text, length );
	copy[length] = 0;

	SDL_SetClipboardText( copy.get() );
}

void CSystem::SetClipboardText( const wchar_t* text, int textLen )
{
	if ( !text || textLen <= 0 )
	{
		return;
	}

	// copy at most textLen characters, as on Windows
	size_t length = wcsnlen( text, textLen );
	std::unique_ptr<wchar_t[]> unicode( new wchar_t[length + 1] );
	wmemcpy( unicode.get(), text, length );
	unicode[length] = 0;

	int size = Q_UnicodeToUTF8( unicode.get(), NULL, 0 );
	if ( size <= 0 )
	{
		return;
	}

	std::unique_ptr<char[]> utf8( new char[size] );
	if ( Q_UnicodeToUTF8( unicode.get(), utf8.get(), size ) != size )
	{
		return;
	}

	SetClipboardText( utf8.get(), size - 1 );
}

int CSystem::GetClipboardTextCount()
{
	int len;
	char* clip = SDL_GetClipboardText();

	if ( !clip )
	{
		return 0;
	}

	len = strlen( clip ) + 1;
	SDL_free( clip );
	return len;
}

int CSystem::GetClipboardText( int offset, char* buf, int bufLen )
{
	if ( !buf || bufLen <= 0 )
		return 0;
	buf[0] = 0;
	char* clip = SDL_GetClipboardText();

	if ( !clip )
	{
		return 0;
	}

	// offset counts the UTF-8 bytes, as Windows counts the CF_TEXT bytes
	int count = CopyClipboardText( clip, (int)strlen( clip ), offset, buf, bufLen );
	SDL_free( clip );
	return count;
}

int CSystem::GetClipboardText( int offset, wchar_t* buf, int bufLen )
{
	if ( !buf || bufLen < (int)sizeof( wchar_t ) )
		return 0;
	buf[0] = 0;
	char* clip = SDL_GetClipboardText();

	if ( !clip )
	{
		return 0;
	}

	// offset counts wide characters, so convert the whole text before skipping them
	int size = Q_UTF8ToUnicode( clip, NULL, 0 );
	std::unique_ptr<wchar_t[]> unicode( new wchar_t[size] );
	Q_UTF8ToUnicode( clip, unicode.get(), size * (int)sizeof( wchar_t ) );
	SDL_free( clip );

	return CopyClipboardText( unicode.get(), size - 1, offset, buf, bufLen / (int)sizeof( wchar_t ) );
}

bool CSystem::SetRegistryString( const char*, const char* )
{
	return false;
}

bool CSystem::GetRegistryString( const char*, char*, int )
{
	return false;
}

bool CSystem::SetRegistryInteger( const char*, int )
{
	return false;
}

bool CSystem::GetRegistryInteger( const char*, int& )
{
	return false;
}

bool CSystem::DeleteRegistryKey( const char* )
{
	return false;
}

bool CSystem::SetWatchForComputerUse( bool state )
{
	if ( m_bStaticWatchForComputerUse != state )
	{
		m_bStaticWatchForComputerUse = state;
	}

	return true;
}

double CSystem::GetTimeSinceLastUse()
{
	if ( m_bStaticWatchForComputerUse )
	{
		return GetCurrentTime() - m_StaticLastComputerUseTime;
	}

	return 0.0;
}

int CSystem::GetAvailableDrives( char*, int )
{
	return 0;
}

double CSystem::GetFreeDiskSpace( const char* path )
{
#ifdef _WIN32
	ULARGE_INTEGER freeBytes;
	if ( !GetDiskFreeSpaceExA( path, NULL, NULL, &freeBytes ) )
	{
		return 0.0;
	}

	return (double)freeBytes.QuadPart;
#else
	struct statvfs buf;
	int ret;

	ret = statvfs( path, &buf );
	if ( ret < 0 )
	{
		return 0.0;
	}

	return (double)buf.f_bsize * (double)buf.f_bfree;
#endif
}

KeyValues* CSystem::GetUserConfigFileData( const char* dialogName, int dialogID )
{
	char buf[256];

	if ( !m_pUserConfigData )
	{
		return NULL;
	}

	if ( dialogID )
	{
		snprintf( buf, sizeof( buf ), "%s_%d", dialogName, dialogID );
		dialogName = buf;
	}

	return m_pUserConfigData->FindKey( dialogName, true );
}

void CSystem::SetUserConfigFile( const char* fileName, const char* pathName )
{
	if ( m_pUserConfigData )
	{
		m_pUserConfigData->Clear();
	}
	else
	{
		m_pUserConfigData = new KeyValues( "UserConfigData" );
	}

	strncpy( m_szFileName, fileName, sizeof( m_szFileName ) );
	strncpy( m_szPathID, pathName, sizeof( m_szPathID ) );
	m_pUserConfigData->LoadFromFile( g_pFileSystem, m_szFileName, m_szPathID );
}

void CSystem::SaveUserConfigFile()
{
	if ( m_pUserConfigData )
	{
		m_pUserConfigData->SaveToFile( g_pFileSystem, m_szFileName, m_szPathID );
	}
}

// reads the process arguments as the kernel received them, so values with spaces keep their boundaries
// returns no arguments when the file cannot be read
static std::vector<std::string> ReadCommandLineArgs( const char* path )
{
	std::string data;
	FILE* fp = fopen( path, "rb" );
	if ( !fp )
	{
		return std::vector<std::string>();
	}

	char buf[4096];
	size_t count;
	while ( ( count = fread( buf, 1, sizeof( buf ), fp ) ) > 0 )
	{
		data.append( buf, count );
	}
	fclose( fp );
	return SplitNulSeparatedArgs( data.data(), data.size() );
}

static const std::vector<std::string>& GetCommandLineArgs()
{
	static const std::vector<std::string> s_Args = ReadCommandLineArgs( "/proc/self/cmdline" );
	return s_Args;
}

bool CSystem::CommandLineParamExists( const char* paramName )
{
	return CommandLineArgExists( GetCommandLineArgs(), paramName );
}

bool CSystem::GetCommandLineParamValue( const char* paramName, char* value, int valueBufferSize )
{
	return CopyCommandLineArgValue( GetCommandLineArgs(), paramName, value, valueBufferSize );
}

const char* CSystem::GetFullCommandLine()
{
	static const std::string s_CmdLine = JoinCommandLine( GetCommandLineArgs() );
	return s_CmdLine.c_str();
}

// struct tm counts years from 1900 and months from 0, callers expect the calendar year and months from 1 as on Windows
static const int TM_YEAR_BASE = 1900;
static const int TM_MONTH_BASE = 1;

bool CSystem::GetCurrentTimeAndDate( int* year, int* month, int* dayOfWeek, int* day, int* hour, int* minute, int* second )
{
	time_t now = time( NULL );
	if ( now == (time_t)-1 )
	{
		return false;
	}

	struct tm local;
	if ( !localtime_r( &now, &local ) )
	{
		return false;
	}

	if ( year )
	{
		*year = local.tm_year + TM_YEAR_BASE;
	}
	if ( month )
	{
		*month = local.tm_mon + TM_MONTH_BASE;
	}
	if ( dayOfWeek )
	{
		*dayOfWeek = local.tm_wday;
	}
	if ( day )
	{
		*day = local.tm_mday;
	}
	if ( hour )
	{
		*hour = local.tm_hour;
	}
	if ( minute )
	{
		*minute = local.tm_min;
	}
	if ( second )
	{
		*second = local.tm_sec;
	}

	return true;
}

bool CSystem::CreateShortcut( const char*, const char*, const char*, const char*, const char* )
{
	return false;
}

bool CSystem::GetShortcutTarget( const char*, char*, char*, int )
{
	return false;
}

bool CSystem::ModifyShortcutTarget( const char*, const char*, const char*, const char* )
{
	return false;
}

const char* CSystem::GetDesktopFolderPath()
{
	return NULL;
}

const char* CSystem::GetStartMenuFolderPath()
{
	return NULL;
}

const char* CSystem::GetAllUserDesktopFolderPath()
{
	return NULL;
}

const char* CSystem::GetAllUserStartMenuFolderPath()
{
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
