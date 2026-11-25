#include "public/tier1/interface.h"
#include "public/engine_launcher_api.h"
#include "public/filesystem.h"
#include "common/icommandline.h"
#include "public/iregistry.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <locale.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#include <assert.h>
#include <ctype.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>

#define LAUNCHER_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\launcher\\launcher.cpp"
#else
#include <dlfcn.h>
#include <strings.h>
#include <unistd.h>
#endif

struct CSysModule;

namespace
{
#define GAME_DIRECTORY_LENGTH		260
#define MODULE_RETRY_LENGTH			512
#define EXECUTABLE_LEAF_LENGTH		256
// The engine appends the arguments of the next start up to this size (POST_RESTART_COMMAND_LENGTH in engine/host_cmd.cpp).
#define NEW_COMMAND_LENGTH			2048
#define LAUNCHER_MESSAGE_LENGTH		216

#define DEFAULT_DISPLAY_WIDTH		800
#define DEFAULT_DISPLAY_HEIGHT		600
#define SOFTWARE_DISPLAY_WIDTH		1024
#define SOFTWARE_DISPLAY_HEIGHT		764
#define DISPLAY_BITS_PER_PIXEL		32
#define EXECUTABLE_EXTENSION_LENGTH	( sizeof( ".exe" ) - 1 )

#if defined(_WIN32)
#define LAUNCHER_PATH_LENGTH		MAX_PATH
#define STEAM_LAUNCH_MESSAGE		( (UINT)( WM_USER + 3 ) )
#define STEAM_TEMPORARY_APP_ID		( (DWORD)-1 )
#define LAUNCHER_PATH_SEPARATOR		( (char)'\\' )

const char kHardwareEngine[] = "hw.dll";
const char kSoftwareEngine[] = "sw.dll";
const char kFileSystemModule[] = "filesystem_stdio.dll";
const char kThisExecutable[] = "hl.exe";
const char kLauncherMutex[] = "ValveHalfLifeLauncherMutex";
const char kSteamRegistryKey[] = "Software\\Valve\\Steam";
#else
#define LAUNCHER_PATH_LENGTH		4096
#define MODULE_PATH_LENGTH			1024
#define LOCALE_NAME_LENGTH			12
#define ENGINE_UNLOAD_ATTEMPT_LIMIT	9
#define LAUNCHER_PATH_SEPARATOR		( (char)'/' )

const char kHardwareEngine[] = "hw.so";
const char kSoftwareEngine[] = "sw.so";
const char kFileSystemModule[] = "filesystem_stdio.so";
const char kThisExecutable[] = "hl_linux";
#endif

const char kDefaultGame[] = "valve";
}

char			g_szEXEName[LAUNCHER_PATH_LENGTH];
char			szBaseDir[LAUNCHER_PATH_LENGTH];
char			com_gamedir[GAME_DIRECTORY_LENGTH];
IFileSystem*	g_pFileSystem = NULL;

namespace
{
int CaseCompare( const char* left, const char* right )
{
#if defined(_WIN32)
	return _stricmp( left, right );
#else
	return strcasecmp( left, right );
#endif
}

void* LoadModuleHandle( const char* name )
{
#if defined(_WIN32)
	return LoadLibraryA( name );
#else
	return dlopen( name, RTLD_NOW );
#endif
}

void UnloadModuleHandle( void* module )
{
	if ( module == NULL )
	{
		return;
	}

#if defined(_WIN32)
	FreeLibrary( (HMODULE)module );
#else
	dlclose( module );
#endif
}

void* ModuleSymbol( void* module, const char* name )
{
#if defined(_WIN32)
	return (void*)GetProcAddress( (HMODULE)module, name );
#else
	return dlsym( module, name );
#endif
}

const char* SelectEngineDll()
{
	const char* stored = registry->ReadString( "EngineDLL", kHardwareEngine );
	const char* selected = kHardwareEngine;

	if ( CaseCompare( stored, kHardwareEngine ) == 0 )
	{
		selected = kHardwareEngine;
	}
	else if ( CaseCompare( stored, kSoftwareEngine ) == 0 )
	{
		selected = kSoftwareEngine;
	}

	if ( cmdline->CheckParm( "-soft", NULL ) != NULL || cmdline->CheckParm( "-software", NULL ) != NULL )
	{
		selected = kSoftwareEngine;
	}
	else if ( cmdline->CheckParm( "-gl", NULL ) != NULL || cmdline->CheckParm( "-d3d", NULL ) != NULL )
	{
		selected = kHardwareEngine;
	}

	registry->WriteString( "EngineDLL", selected );
	return selected;
}

void StripLaunchParameters( const char* newCommand )
{
	static const char* const parameters[] =
	{
		"-sw",
		"-startwindowed",
		"-windowed",
		"-window",
		"-full",
		"-fullscreen",
		"-soft",
		"-software",
		"-gl",
		"-d3d",
		"-w",
		"-width",
		"-h",
		"-height",
		"+connect"
	};

	for ( const char* parameter : parameters )
	{
		cmdline->RemoveParm( parameter );
	}

	cmdline->SetParm( "-novid", 0 );

	if ( strstr( newCommand, "-game" ) != NULL )
	{
		cmdline->RemoveParm( "-game" );
	}

	if ( strstr( newCommand, "+load" ) != NULL )
	{
		cmdline->RemoveParm( "+load" );
	}

	cmdline->AppendParm( newCommand, NULL );
}

void ApplyGameDirectory( char* executablePath )
{
	char* leaf = strrchr( executablePath, LAUNCHER_PATH_SEPARATOR ) + 1;

	if ( CaseCompare( kThisExecutable, leaf ) != 0 && cmdline->CheckParm( "-game", NULL ) == NULL )
	{
		leaf[strlen( leaf ) - EXECUTABLE_EXTENSION_LENGTH] = '\0';
		cmdline->AppendParm( "-game", leaf );
	}

	const char* game = cmdline->CheckParm( "-game", NULL );
	if ( game != NULL )
	{
		strncpy( com_gamedir, game, GAME_DIRECTORY_LENGTH );
	}
	else
	{
		strcpy( com_gamedir, kDefaultGame );
	}
}
}

char* UTIL_GetBaseDir()
{
#if defined(_WIN32)
	char executablePath[LAUNCHER_PATH_LENGTH];
	executablePath[0] = '\0';
	szBaseDir[0] = '\0';

	if ( GetModuleFileNameA( NULL, executablePath, sizeof( executablePath ) ) != 0 )
	{
		GetLongPathNameA( executablePath, szBaseDir, sizeof( szBaseDir ) );

		char* lastSeparator = strrchr( szBaseDir, LAUNCHER_PATH_SEPARATOR );
		if ( *lastSeparator != '\0' )
		{
			lastSeparator[1] = '\0';
		}

		const int length = (int)strlen( szBaseDir );
		if ( length > 0 )
		{
			if ( szBaseDir[length - 1] == '\\' || szBaseDir[length - 1] == '/' )
			{
				szBaseDir[length - 1] = '\0';
			}
		}
	}
#endif
	return szBaseDir;
}

int Sys_GetExecutableName( char* destination, int length )
{
	strncpy( destination, g_szEXEName, length );
	return 1;
}

CSysModule* Sys_LoadModule( const char* moduleName )
{
#if defined(_WIN32)
	void* loaded = LoadModuleHandle( moduleName );
	if ( loaded == NULL )
	{
		char retryName[MODULE_RETRY_LENGTH];
		_snprintf( retryName, sizeof( retryName ), "%s.dll", moduleName );
		loaded = LoadModuleHandle( retryName );
	}

	return (CSysModule*)loaded;
#else
	char absoluteName[MODULE_PATH_LENGTH];
	char workingDirectory[MODULE_PATH_LENGTH];
	char joined[MODULE_PATH_LENGTH];
	void* loaded;

	absoluteName[0] = '\0';

	if ( moduleName[0] == LAUNCHER_PATH_SEPARATOR )
	{
		snprintf( absoluteName, sizeof( absoluteName ), "%s", moduleName );
		loaded = LoadModuleHandle( moduleName );
		if ( loaded != NULL )
		{
			return (CSysModule*)loaded;
		}
	}
	else
	{
		getcwd( workingDirectory, sizeof( workingDirectory ) );
		const size_t directoryLength = strlen( workingDirectory );
		if ( directoryLength > 0 && workingDirectory[directoryLength - 1] == LAUNCHER_PATH_SEPARATOR )
		{
			workingDirectory[directoryLength - 1] = '\0';
		}

		snprintf( joined, sizeof( joined ), "%s/%s", workingDirectory, moduleName );
		loaded = LoadModuleHandle( joined );
		if ( loaded != NULL )
		{
			return (CSysModule*)loaded;
		}
	}

	printf( "Error:%s\n", dlerror() );
	snprintf( joined, MODULE_RETRY_LENGTH, "%s.so", absoluteName );
	return (CSysModule*)LoadModuleHandle( joined );
#endif
}

void Sys_UnloadModule( CSysModule* module )
{
	UnloadModuleHandle( module );
}

void* Sys_GetProcAddress( void* module, const char* name )
{
	return ModuleSymbol( module, name );
}

CreateInterfaceFn Sys_GetFactory( CSysModule* module )
{
	if ( module == NULL )
	{
		return NULL;
	}

	return (CreateInterfaceFn)ModuleSymbol( module, "CreateInterface" );
}

CreateInterfaceFn Sys_GetFactoryThis()
{
	return CreateInterface;
}

CreateInterfaceFn Sys_GetFactory( const char* moduleName )
{
#if defined(_WIN32)
	HMODULE module = GetModuleHandleA( moduleName );
	if ( module == NULL )
	{
		return NULL;
	}

	return (CreateInterfaceFn)GetProcAddress( module, "CreateInterface" );
#else
	void* module = NULL;

	if ( moduleName != NULL )
	{
		module = LoadModuleHandle( moduleName );
		if ( module != NULL )
		{
			UnloadModuleHandle( module );
		}
	}

	return (CreateInterfaceFn)ModuleSymbol( module, "CreateInterface" );
#endif
}

#if !defined(_WIN32)
void* GetModuleHandle( const char* name )
{
	if ( name == NULL )
	{
		return NULL;
	}

	void* module = LoadModuleHandle( name );
	if ( module == NULL )
	{
		return NULL;
	}

	UnloadModuleHandle( module );
	return module;
}
#endif

CSysModule* LoadFilesystemModule( const char*, bool )
{
	CSysModule* module = Sys_LoadModule( kFileSystemModule );
	if ( module == NULL )
	{
		fwrite( "Failed to load filesystem library, exiting...\n", 1,
			sizeof( "Failed to load filesystem library, exiting...\n" ) - 1, stderr );
		return NULL;
	}

	return module;
}

int OnVideoModeFailed()
{
	fwrite( "The specified video mode is not supported.\nThe game will now exit.\n", 1,
		sizeof( "The specified video mode is not supported.\nThe game will now exit.\n" ) - 1, stderr );
	return 0;
}

void SetEngineDLL( const char** engineDll )
{
	*engineDll = SelectEngineDll();
}

#if defined(_WIN32)

static void ShowError( const char* text, const char* title )
{
	MessageBoxA( NULL, text, title, MB_OK | MB_ICONERROR );
}

static void FindSteamExecutable( char* steamPath )
{
	char directory[LAUNCHER_PATH_LENGTH];
	steamPath[0] = '\0';

	if ( GetCurrentDirectoryA( sizeof( directory ), directory ) != 0 )
	{
		char* separator = strrchr( directory, LAUNCHER_PATH_SEPARATOR );
		while ( separator != NULL )
		{
			separator[1] = '\0';
			strcat( separator, "steam_dev.exe" );
			FILE* file = fopen( directory, "rb" );
			if ( file != NULL )
			{
				fclose( file );
				strcpy( steamPath, directory );
				return;
			}

			separator[1] = '\0';
			strcat( separator, "steam.exe" );
			file = fopen( directory, "rb" );
			if ( file != NULL )
			{
				fclose( file );
				strcpy( steamPath, directory );
				return;
			}

			*separator = '\0';
			separator = strrchr( directory, LAUNCHER_PATH_SEPARATOR );
		}
	}

	HKEY steamKey;
	if ( RegOpenKeyA( HKEY_CURRENT_USER, kSteamRegistryKey, &steamKey ) == ERROR_SUCCESS )
	{
		DWORD type;
		DWORD length = LAUNCHER_PATH_LENGTH;
		RegQueryValueExA( steamKey, "SteamExe", NULL, &type, (BYTE*)steamPath, &length );
		RegCloseKey( steamKey );
	}
}

static void LaunchThroughSteam( const char* commandLine )
{
	char executableDirectory[LAUNCHER_PATH_LENGTH];
	GetModuleFileNameA( GetModuleHandleA( NULL ), executableDirectory, sizeof( executableDirectory ) );
	char* separator = strrchr( executableDirectory, LAUNCHER_PATH_SEPARATOR );
	if ( separator != NULL )
	{
		*separator = '\0';
	}

	HKEY steamKey;
	if ( RegOpenKeyA( HKEY_CURRENT_USER, kSteamRegistryKey, &steamKey ) == ERROR_SUCCESS )
	{
		RegSetValueExA( steamKey, "TempAppPath", 0, REG_SZ,
			(const BYTE*)executableDirectory, strlen( executableDirectory ) + 1 );
		RegSetValueExA( steamKey, "TempAppCmdLine", 0, REG_SZ,
			(const BYTE*)commandLine, strlen( commandLine ) + 1 );
		DWORD appId = STEAM_TEMPORARY_APP_ID;
		RegSetValueExA( steamKey, "TempAppID", 0, REG_DWORD, (const BYTE*)&appId, sizeof( appId ) );
		RegCloseKey( steamKey );
	}

	HWND steamWindow = FindWindowA( "Valve_SteamIPC_Class", "Hidden Window" );
	if ( steamWindow != NULL )
	{
		PostMessageA( steamWindow, STEAM_LAUNCH_MESSAGE, 0, 0 );
		return;
	}

	char steamPath[LAUNCHER_PATH_LENGTH];
	FindSteamExecutable( steamPath );
	if ( steamPath[0] == '\0' )
	{
		ShowError( "Error running game: could not find steam.exe to launch", "Fatal Error" );
		return;
	}

	for ( char* character = steamPath; *character != '\0'; ++character )
	{
		if ( *character == '/' )
		{
			*character = LAUNCHER_PATH_SEPARATOR;
		}
	}

	strcpy( executableDirectory, steamPath );
	separator = strrchr( executableDirectory, LAUNCHER_PATH_SEPARATOR );
	if ( separator != NULL )
	{
		*separator = '\0';
		(void)_chdir( executableDirectory );
	}

	const char* arguments[] = { steamPath, "-silent", "-applaunch", NULL };
	_spawnv( _P_NOWAIT, steamPath, arguments );
}

static bool CheckSteamLaunch( const char* commandLine, const char* fallbackFile, const char* requiredFile )
{
	const char* parameter = strstr( commandLine, "-steam" );
	if ( parameter != NULL )
	{
		const char next = parameter[sizeof( "-steam" ) - 1];
		if ( next == '\0' || isspace( next ) )
		{
			return false;
		}
	}

	struct _stat status;
	if ( _stat( requiredFile, &status ) == 0 || _stat( fallbackFile, &status ) != 0 )
	{
		return false;
	}

	LaunchThroughSteam( commandLine );
	return true;
}

static bool AskVideoChange( const char* text )
{
	return MessageBoxA( NULL, text, "Video mode change failure", MB_OKCANCEL | MB_ICONWARNING ) == IDOK;
}

static void DesktopSize( int& width, int& height )
{
	RECT rectangle;

	width = DEFAULT_DISPLAY_WIDTH;
	height = DEFAULT_DISPLAY_HEIGHT;

	if ( GetWindowRect( GetDesktopWindow(), &rectangle ) )
	{
		width = rectangle.right - rectangle.left;
		height = rectangle.bottom - rectangle.top;
	}
}

static bool RecoverFailedVideoStart()
{
	if ( registry->ReadInt( "CrashInitializingVideoMode", 0 ) == 0 )
	{
		return true;
	}

	registry->WriteInt( "CrashInitializingVideoMode", 0 );

	int width = DEFAULT_DISPLAY_WIDTH;
	int height = DEFAULT_DISPLAY_HEIGHT;
	DesktopSize( width, height );

	const char* engine = registry->ReadString( "EngineDLL", "" );
	bool accepted;

	if ( CaseCompare( engine, kHardwareEngine ) == 0 )
	{
		if ( registry->ReadInt( "EngineD3D", 0 ) == 1 )
		{
			registry->WriteInt( "EngineD3D", 0 );
			accepted = AskVideoChange(
				"The game has detected that the previous attempt to start in D3D video mode failed.\n"
				"The game will now run attempt to run in openGL mode." );
		}
		else
		{
			registry->WriteString( "EngineDLL", kSoftwareEngine );
			accepted = AskVideoChange(
				"The game has detected that the previous attempt to start in OpenGL video mode failed.\n"
				"The game will now run in software mode." );
			width = SOFTWARE_DISPLAY_WIDTH;
			height = SOFTWARE_DISPLAY_HEIGHT;
		}
	}
	else
	{
		registry->WriteString( "EngineDLL", kHardwareEngine );
		AskVideoChange(
			"The game has detected that the previous attempt to start in software video mode failed.\n"
			"The game will now run in OpenGL mode." );
		accepted = true;
	}

	if ( !accepted )
	{
		return false;
	}

	registry->WriteInt( "DisplayBPP", DISPLAY_BITS_PER_PIXEL );
	registry->WriteInt( "DisplayHeight", height );
	registry->WriteInt( "DisplayWidth", width );
	return true;
}

static void ReportFileSystemFailure( const char* executablePath )
{
	if ( strchr( executablePath, ';' ) != NULL )
	{
		ShowError( "Game cannot be run from directories containing the semicolon character (';').", "Fatal Error" );
		return;
	}

	struct _finddata_t found;
	intptr_t search = _findfirst( kFileSystemModule, &found );

	if ( search == -1 )
	{
		ShowError( "Could not find filesystem dll to load.", "Fatal Error" );
	}
	else
	{
		ShowError( "Could not load filesystem dll.\nFileSystem crashed during construction.", "Fatal Error" );
		_findclose( search );
	}
}

int WINAPI WinMain( _In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPSTR commandLine, _In_ int )
{
	static char szNewCommandParams[NEW_COMMAND_LENGTH];

	if ( CheckSteamLaunch( commandLine, kFileSystemModule, kFileSystemModule ) )
	{
		return 0;
	}

	HANDLE mutex = CreateMutexA( NULL, FALSE, kLauncherMutex );
	if ( mutex != NULL )
	{
		(void)GetLastError();
	}

#pragma warning( suppress : 6387 )
	const DWORD waitResult = WaitForSingleObject( mutex, 0 );
	if ( waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED )
	{
		ShowError( "Could not launch game.\nOnly one instance of this game can be run at a time.", "Error" );
		return 0;
	}

	WSADATA sockets;
	(void)WSAStartup( MAKEWORD( 2, 0 ), &sockets );

	registry->Init();
	cmdline->CreateCmdLine( GetCommandLineA() );
	cmdline->CheckParm( "-steam", NULL );

	char executablePath[EXECUTABLE_LEAF_LENGTH];
	GetModuleFileNameA( GetModuleHandleA( NULL ), executablePath, sizeof( executablePath ) );

	ApplyGameDirectory( executablePath );
	_unlink( "mssv29.asi" );
	_unlink( "mssv12.asi" );
	_unlink( "mp3dec.asi" );
	_unlink( "opengl32.dll" );

	if ( !RecoverFailedVideoStart() )
	{
		return 0;
	}

	for ( ;; )
	{
		CSysModule* fileSystemModule = Sys_LoadModule( kFileSystemModule );
		if ( fileSystemModule == NULL )
		{
			ReportFileSystemFailure( executablePath );
			break;
		}

		CreateInterfaceFn fileSystemFactory = Sys_GetFactory( fileSystemModule );
		g_pFileSystem = (IFileSystem*)fileSystemFactory( FILESYSTEM_INTERFACE_VERSION, NULL );
		if ( !g_pFileSystem )
		{
			_wassert( L"g_pFileSystem", LAUNCHER_SOURCE_PATH, 439 );
		}

		g_pFileSystem->Mount();
		g_pFileSystem->AddSearchPath( UTIL_GetBaseDir(), "ROOT" );

		int runResult = kEngineRunNone;
		szNewCommandParams[0] = '\0';
		bool restart = false;

		const char* engineDll = SelectEngineDll();
		CSysModule* engineModule = Sys_LoadModule( engineDll );
		if ( engineModule != NULL )
		{
			CreateInterfaceFn engineFactory = Sys_GetFactory( engineModule );
			if ( engineFactory != NULL )
			{
				IEngineAPI* engine = (IEngineAPI*)engineFactory( kEngineLauncherVersion, NULL );
				if ( engine != NULL )
				{
					runResult = engine->Run( instance, UTIL_GetBaseDir(), cmdline->GetCmdLine(), szNewCommandParams,
						Sys_GetFactoryThis(), Sys_GetFactory( fileSystemModule ) );
				}
			}

			Sys_UnloadModule( engineModule );

			switch ( runResult )
			{
			case kEngineRunNone:
				restart = false;
				break;

			case kEngineRunRestart:
				restart = true;
				break;

			case kEngineRunVideoModeFailed:
			{
				int width;
				int height;
				DesktopSize( width, height );
				registry->WriteInt( "DisplayBPP", DISPLAY_BITS_PER_PIXEL );
				registry->WriteInt( "DisplayHeight", height );
				registry->WriteInt( "DisplayWidth", width );
				registry->WriteString( "EngineDLL", kHardwareEngine );
				restart = AskVideoChange(
					"The specified video mode is not supported.\nThe game will now run in gl mode." );
				break;
			}
			}
		}
		else
		{
			char message[LAUNCHER_MESSAGE_LENGTH];
			sprintf( message, "Could not load %s.\nPlease try again at a later time.", engineDll );
			ShowError( message, "Fatal Error" );
			restart = false;
		}

		StripLaunchParameters( szNewCommandParams );
		g_pFileSystem->Unmount();
		Sys_UnloadModule( fileSystemModule );

		if ( !restart )
		{
			break;
		}
	}

	registry->Shutdown();
#pragma warning( suppress : 6387 )
	ReleaseMutex( mutex );
#pragma warning( suppress : 6387 )
	CloseHandle( mutex );
	WSACleanup();
	return 0;
}

#else

static bool UnloadEngineImage( const char* engineDll )
{
	void* handle = dlopen( engineDll, RTLD_NOLOAD );
	if ( handle == NULL )
	{
		return true;
	}

	int attempt = 0;
	do
	{
		++attempt;
		dlclose( handle );
		handle = dlopen( engineDll, RTLD_NOLOAD );
	}
	while ( handle != NULL && attempt <= ENGINE_UNLOAD_ATTEMPT_LIMIT );

	if ( handle != NULL )
	{
		fprintf( stderr, "Failed to unload engine on exit (%s).", engineDll );
	}

	return true;
}

int main( int argc, const char** argv, const char** )
{
	static char szNewCommandParams[NEW_COMMAND_LENGTH];
	char localeName[LOCALE_NAME_LENGTH];
	char executableName[EXECUTABLE_LEAF_LENGTH];

	strcpy( localeName, "en_US.UTF-8" );
	setenv( "LC_ALL", localeName, 1 );
	setlocale( LC_ALL, localeName );

	char* activeLocale = setlocale( LC_ALL, NULL );
	if ( CaseCompare( activeLocale, localeName ) != 0 )
	{
		fprintf( stderr, "WARNING: setlocale('%s') failed, using locale:'%s'. International characters may not work.\n",
			localeName, activeLocale );
	}

	snprintf( g_szEXEName, LAUNCHER_PATH_LENGTH, "%s", argv[0] );
	snprintf( szBaseDir, LAUNCHER_PATH_LENGTH, "%s", argv[0] );

	char* lastSeparator = strrchr( szBaseDir, LAUNCHER_PATH_SEPARATOR );
	if ( lastSeparator != NULL )
	{
		*lastSeparator = '\0';
	}

	cmdline->CreateCmdLine( argc, argv );
	cmdline->CheckParm( "-steam", NULL );
	strncpy( executableName, g_szEXEName, EXECUTABLE_LEAF_LENGTH );
	ApplyGameDirectory( executableName );

	for ( ;; )
	{
		CSysModule* fileSystemModule = Sys_LoadModule( kFileSystemModule );
		if ( fileSystemModule == NULL )
		{
			fwrite( "Failed to load filesystem library, exiting...\n", 1,
				sizeof( "Failed to load filesystem library, exiting...\n" ) - 1, stderr );
			return 0;
		}

		CreateInterfaceFn fileSystemFactory = Sys_GetFactory( fileSystemModule );
		g_pFileSystem = (IFileSystem*)fileSystemFactory( FILESYSTEM_INTERFACE_VERSION, NULL );
		g_pFileSystem->Mount();
		g_pFileSystem->AddSearchPath( szBaseDir, "ROOT" );
		szNewCommandParams[0] = '\0';

		const char* engineDll = NULL;
		SetEngineDLL( &engineDll );

		CSysModule* engineModule = Sys_LoadModule( engineDll );
		bool restart = false;

		if ( engineModule != NULL )
		{
			CreateInterfaceFn engineFactory = Sys_GetFactory( engineModule );
			IEngineAPI* engine = NULL;

			if ( engineFactory != NULL )
			{
				engine = (IEngineAPI*)engineFactory( kEngineLauncherVersion, NULL );
			}

			if ( engine != NULL )
			{
				const int runResult = engine->Run( NULL, szBaseDir, cmdline->GetCmdLine(), szNewCommandParams,
					Sys_GetFactoryThis(), Sys_GetFactory( fileSystemModule ) );
				Sys_UnloadModule( engineModule );

				if ( runResult == kEngineRunRestart )
				{
					restart = UnloadEngineImage( engineDll );
				}
				else if ( runResult == kEngineRunVideoModeFailed )
				{
					OnVideoModeFailed();
				}
			}
			else
			{
				Sys_UnloadModule( engineModule );
			}
		}
		else
		{
			fprintf( stderr, "Could not load %s.\nPlease try again at a later time.", engineDll );
		}

		StripLaunchParameters( szNewCommandParams );
		g_pFileSystem->Unmount();
		Sys_UnloadModule( fileSystemModule );

		if ( !restart )
		{
			return 0;
		}
	}
}

#endif
