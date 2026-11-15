#include "System.h"

#include "HLTV/common/common.h"
#include "HLTV/common/random.h"
#include "GameUI/common/IVGuiModule.h"
#include "vgui/IScheme.h"
#include "vgui2/controls/Panel.h"
#include "vgui2/controls/controls.h"
#include "engine/controls.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define LIBRARY_SUFFIX ".dll"
#else
#include <time.h>
#include <unistd.h>

#define LIBRARY_SUFFIX ".so"

static void Sleep( unsigned int milliseconds )
{
	usleep( milliseconds * 1000 );
}
#endif

#define FILESYSTEM_LIBRARY "filesystem_stdio" LIBRARY_SUFFIX

static System g_System;

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
System::System()
{
	m_State = MODULE_UNDEFINED;
#ifdef _WIN32
	InitializeCriticalSection( &m_ClockLock );
#else
	pthread_mutex_init( &m_ClockLock, NULL );
#endif
}

System::~System()
{
#ifdef _WIN32
	DeleteCriticalSection( &m_ClockLock );
#else
	pthread_mutex_destroy( &m_ClockLock );
#endif
}

bool System::Init( IBaseSystem* system, int serial, char* name )
{
	if ( !BaseSystemModule::Init( system, serial, name ) )
		return false;

	m_Serial = serial;
	m_NextSerial = serial + 1;
	_snprintf( m_Name, sizeof( m_Name ), name ? name : "system" );
	strncpy( m_BaseDir, COM_GetBaseDir(), sizeof( m_BaseDir ) );
	m_Developer = CheckParam( "-dev" ) != NULL;

	if ( !m_Console.Init( this ) )
	{
#ifdef _WIN32
		char* message;
		FormatMessageA( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL, GetLastError(), MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), (char*)&message, 0, NULL );
		Errorf( "%s", message );
		LocalFree( message );
#else
		Errorf( "Couldn't initialize the console.\n" );
#endif
		return false;
	}

#ifdef _WIN32
	LARGE_INTEGER frequency;
	if ( !QueryPerformanceFrequency( &frequency ) )
	{
		Errorf( "Couldn't get system timer.\n" );
		return false;
	}

	m_ClockShift = 0;
	while ( frequency.HighPart || (double)frequency.LowPart > 2000000.0 )
	{
		m_ClockShift++;
		frequency.QuadPart = (unsigned __int64)frequency.QuadPart >> 1;
	}
	m_ClockFrequency = 1.0 / (double)frequency.LowPart;
#endif
	m_SystemTime = 0;
	m_LastTime = 0;

	m_FileSystemLibrary = Sys_LoadModule( FILESYSTEM_LIBRARY );
	if ( !m_FileSystemLibrary )
	{
		Errorf( "Couldn't load %s.\n", FILESYSTEM_LIBRARY );
		return false;
	}

	CreateInterfaceFn factory = Sys_GetFactory( m_FileSystemLibrary );
	m_FileSystem = (IFileSystem*)factory( "VFileSystem009", NULL );
	if ( !m_FileSystem )
	{
		Errorf( "Couldn't get IFileSystem from %s.\n", FILESYSTEM_LIBRARY );
		return false;
	}

	m_FileSystem->Mount();
	m_FileSystem->AddSearchPath( m_BaseDir, "ROOT" );
	Printf( "FileSystem initialized" );
	DPrintf( " (%s, %s)", FILESYSTEM_LIBRARY, m_BaseDir );
	Printf( ".\n" );

	m_Commands.Init();
	m_Modules.Init();
	m_Libraries.Init();
	m_LogFile = NULL;
	memset( m_LogFileName, 0, sizeof( m_LogFileName ) );
	m_RedirectBuffer = NULL;
	m_RedirectSize = 0;
	m_Tick = 0;
	m_SleepTime = 1;
	m_VGUI = false;
	m_VGuiLibrary = NULL;
	memset( m_Factories, 0, sizeof( m_Factories ) );
	SeedRandomNumberGenerator();

	if ( CheckParam( "-highpriority" ) )
	{
		m_SleepTime = 0;
#ifdef _WIN32
		SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
#endif
	}

	if ( CheckParam( "-maxfps" ) )
		SetFPS( (float)atof( CheckParam( "-maxfps" ) ) );
	else
		SetFPS( 100 );

	RegisterCommand( "cmdlist", this, CMD_LIST );
	RegisterCommand( "help", this, CMD_LIST );
	RegisterCommand( "logfile", this, CMD_LOGFILE );
	RegisterCommand( "modules", this, CMD_MODULES );
	RegisterCommand( "exec", this, CMD_EXEC );
	RegisterCommand( "developer", this, CMD_DEVELOPER );
	RegisterCommand( "showcon", this, CMD_SHOWCON );
	RegisterCommand( "quit", this, CMD_QUIT );
	RegisterCommand( "exit", this, CMD_QUIT );
	RegisterCommand( "echo", this, CMD_ECHO );
	RegisterCommand( "loadmodule", this, CMD_LOADMODULE );
	RegisterCommand( "unloadmodule", this, CMD_UNLOADMODULE );

	SetTitle( "Console" );
	GetModule( "proxy001", "proxy", NULL );
	SetTitle( "HLTV" );
	m_State = MODULE_RUNNING;
	DPrintf( "System initialized.\n" );

	int token = 0;
	char* argument = m_CommandLine.GetToken( token );
	while ( argument )
	{
		token++;
		if ( *argument == '+' )
		{
			char command[256];
			memset( command, 0, sizeof( command ) );
			strncpy( command, argument + 1, sizeof( command ) - 1 );
			command[sizeof( command ) - 1] = 0;
			argument = m_CommandLine.GetToken( token );
			while ( argument && *argument != '+' && *argument != '-' )
			{
				strncat( command, " ", sizeof( command ) - 1 - strlen( command ) );
				strncat( command, argument, sizeof( command ) - 1 - strlen( command ) );
				argument = m_CommandLine.GetToken( ++token );
			}
			ExecuteString( command );
		}
		else
		{
			argument = m_CommandLine.GetToken( token );
		}
	}

	Printf( "Type 'help' for a list of commands.\n" );
	return true;
}

void System::RunFrame( double time )
{
#ifdef _WIN32
	static int firstCount = 1;
	static DWORD previousCount;
#endif
	float nextConsoleUpdate = 0;
	BaseSystemModule::RunFrame( time );

	while ( m_State == MODULE_RUNNING || m_State == MODULE_PAUSED )
	{
#ifdef _WIN32
		EnterCriticalSection( &m_ClockLock );
		LARGE_INTEGER counter;
		QueryPerformanceCounter( &counter );
		DWORD count = counter.LowPart;
		if ( m_ClockShift )
			count = ( counter.LowPart >> m_ClockShift ) | ( (DWORD)counter.HighPart << ( 32 - m_ClockShift ) );

		if ( firstCount )
			firstCount = 0;
		else if ( count > previousCount || previousCount - count >= 0x10000000 )
			m_SystemTime = (double)( count - previousCount ) * m_ClockFrequency + m_SystemTime;

		previousCount = count;
		LeaveCriticalSection( &m_ClockLock );
#else
		pthread_mutex_lock( &m_ClockLock );
		static double startTime = -1.0;
		struct timespec now;
		clock_gettime( CLOCK_MONOTONIC, &now );
		double seconds = (double)now.tv_sec + (double)now.tv_nsec * 1.0e-9;
		if ( startTime < 0 )
			startTime = seconds;
		m_SystemTime = seconds - startTime;
		pthread_mutex_unlock( &m_ClockLock );
#endif

		double difference = m_SystemTime - m_LastTime;
		if ( difference == 0 )
		{
			Sleep( m_SleepTime );
			continue;
		}
		if ( m_LastTime > 0 && difference <= 0 )
		{
			Printf( "WARNING! System::RunFrame: system time difference <= 0.\n" );
			difference = 0.001f;
		}

		if ( m_MinFrameTime <= difference )
		{
			m_Tick++;
			m_LastTime = m_SystemTime;
			if ( m_SystemTime > nextConsoleUpdate )
			{
				ExecuteString( m_Console.GetLine() );
				m_Console.SetStatusLine( m_StatusLine );
				nextConsoleUpdate = (float)( m_SystemTime + 0.1f );
			}

#ifdef _WIN32
			if ( m_VGUI )
				vgui2::ivgui()->RunFrame();
#endif

			if ( m_State == MODULE_PAUSED )
			{
				Sleep( 1 );
			}
			else
			{
				ISystemModule* module = (ISystemModule*)m_Modules.GetFirst();
				while ( module )
				{
					if ( m_State == MODULE_DISCONNECTED )
						break;
					module->RunFrame( m_SystemTime );
					module = (ISystemModule*)m_Modules.GetNext();
				}
			}
		}
		else
		{
			Sleep( m_SleepTime );
		}
	}
}

void System::ShutDown()
{
	m_Commands.Clear( true );
	ISystemModule* module = (ISystemModule*)m_Modules.GetFirst();
	while ( module )
	{
		module->ShutDown();
		module = (ISystemModule*)m_Modules.GetFirst();
	}

	library_t* library = (library_t*)m_Libraries.RemoveTail();
	while ( library )
	{
		if ( library->module )
			Sys_UnloadModule( library->module );
		free( library );
		library = (library_t*)m_Libraries.RemoveTail();
	}

	CloseLogFile();
	if ( m_FileSystem )
		m_FileSystem->Unmount();
	Sys_UnloadModule( m_FileSystemLibrary );

#ifdef _WIN32
	if ( m_VGUI )
	{
		vgui2::surface()->Shutdown();
		delete m_MainPanel;
		m_MainPanel = NULL;
		Sys_UnloadModule( m_VGuiLibrary );
		Printf( "VGUI shutdown.\n" );
	}
#endif

	if ( m_Developer )
		Sleep( 3000 );
	m_Console.ShutDown();
	m_State = MODULE_DISCONNECTED;
}

double System::GetTime()
{
	return m_SystemTime;
}

unsigned int System::GetTick()
{
	return m_Tick;
}

void System::SetFPS( float fps )
{
	m_MinFrameTime = fps > 0 ? 1.0f / fps : 0;
}

char* System::GetStatusLine()
{
	static char string[80];
	sprintf( string, " Libraries %i, Modules %i, Commands %i.\n", m_Libraries.CountElements(),
			m_Modules.CountElements(), m_Commands.CountElements() );
	return string;
}

char* System::GetType()
{
	return "basesystem002";
}

void System::Stop()
{
	m_State = MODULE_DISCONNECTED;
}

char* System::COM_GetBaseDir()
{
	static char baseDir[MAX_PATH];
	baseDir[0] = 0;
#ifdef _WIN32
	if ( GetModuleFileNameA( NULL, baseDir, sizeof( baseDir ) ) )
	{
		char* separator = strrchr( baseDir, '\\' );
#else
	ssize_t length = readlink( "/proc/self/exe", baseDir, sizeof( baseDir ) - 1 );
	if ( length > 0 )
	{
		baseDir[length] = 0;
		char* separator = strrchr( baseDir, '/' );
#endif
		if ( separator && *separator )
			separator[1] = 0;

		int length = (int)strlen( baseDir );
		if ( length > 0 && ( baseDir[length - 1] == '\\' || baseDir[length - 1] == '/' ) )
			baseDir[length - 1] = 0;
	}
	return baseDir;
}

char* System::CheckParam( char* param )
{
	return m_CommandLine.CheckToken( param );
}

IFileSystem* System::GetFileSystem()
{
	return m_FileSystem;
}

void System::SetTitle( char* title )
{
	m_Console.SetTitle( title );
}

void System::SetStatusLine( char* line )
{
	strncpy( m_StatusLine, line, sizeof( m_StatusLine ) - 1 );
	m_StatusLine[sizeof( m_StatusLine ) - 1] = 0;
}

void System::ShowConsole( bool visible )
{
	m_Console.SetVisible( visible );
}

void System::Printf( char* format, ... )
{
	static char text[8192];
	va_list args;
	va_start( args, format );
	_vsnprintf( text, sizeof( text ), format, args );
	text[sizeof( text ) - 1] = 0;
	va_end( args );

	if ( m_LogFile )
		m_FileSystem->Write( text, (int)strlen( text ), m_LogFile );
	if ( m_RedirectBuffer && strlen( text ) + strlen( m_RedirectBuffer ) + 1 < (size_t)m_RedirectSize )
		strcat( m_RedirectBuffer, text );
	m_Console.Print( text );
}

void System::DPrintf( char* format, ... )
{
	if ( !m_Developer )
		return;

	static char text[8192];
	va_list args;
	va_start( args, format );
	_vsnprintf( text, sizeof( text ), format, args );
	text[sizeof( text ) - 1] = 0;
	va_end( args );

	if ( m_LogFile )
		m_FileSystem->Write( text, (int)strlen( text ), m_LogFile );
	if ( m_RedirectBuffer && strlen( text ) + strlen( m_RedirectBuffer ) + 1 < (size_t)m_RedirectSize )
		strcat( m_RedirectBuffer, text );
	m_Console.Print( text );
}

void System::Errorf( char* format, ... )
{
	static char text[1024];
	va_list args;
	va_start( args, format );
	_vsnprintf( text, sizeof( text ), format, args );
	text[sizeof( text ) - 1] = 0;
	va_end( args );
	Printf( "***** FATAL ERROR *****\n" );
	Printf( "%s", text );
	Printf( "*** STOPPING SYSTEM ***\n" );
	Stop();
}

void System::RedirectOutput( char* buffer, int maxSize )
{
	m_RedirectBuffer = buffer;
	m_RedirectSize = maxSize;
	if ( buffer )
		memset( buffer, 0, maxSize );
}

unsigned char* System::LoadFile( const char* name, int* length )
{
	if ( !m_FileSystem )
		return NULL;
	if ( length )
		*length = 0;
	FileHandle_t file = m_FileSystem->Open( name, "rb" );
	if ( !file )
		return NULL;

	m_FileSystem->Seek( file, 0, FILESYSTEM_SEEK_TAIL );
	int size = m_FileSystem->Tell( file );
	unsigned char* buffer = (unsigned char*)malloc( size + 1 );
	if ( !buffer )
	{
		Printf( "WARNING! System::LoadFile: not enough space for %s.\n", name );
		m_FileSystem->Close( file );
		return NULL;
	}

	buffer[size] = 0;
	m_FileSystem->Seek( file, 0, FILESYSTEM_SEEK_HEAD );
	m_FileSystem->Read( buffer, size, file );
	m_FileSystem->Close( file );
	if ( length )
		*length = size;
	return buffer;
}

void System::FreeFile( unsigned char* buffer )
{
	if ( buffer )
		free( buffer );
}

void System::LogConsole( char* filename )
{
	if ( filename )
		OpenLogFile( filename );
	else
		CloseLogFile();
}

void System::OpenLogFile( char* filename )
{
	CloseLogFile();
	if ( *filename == '/' || strstr( filename, ":" ) || strstr( filename, ".." ) || strstr( filename, "\\" ) )
	{
		Printf( "WARNING! System::OpenLogFile: unable to open %s (contains illegal characters).\n", filename );
		return;
	}

	strncpy( m_LogFileName, filename, 255 );
	m_LogFileName[255] = 0;
	COM_DefaultExtension( m_LogFileName, ".txt" );
	if ( strcmp( COM_FileExtension( m_LogFileName ), "txt" ) )
	{
		memset( m_LogFileName, 0, sizeof( m_LogFileName ) );
		Printf( "WARNING! System::OpenLogFile: unable to open %s (wrong file extension, must be .txt).\n", filename );
		return;
	}

	m_LogFile = m_FileSystem->Open( m_LogFileName, "wt" );
	if ( !m_LogFile )
	{
		memset( m_LogFileName, 0, sizeof( m_LogFileName ) );
		Printf( "System::OpenLogFile: error while opening logfile.\n" );
	}
}

void System::CloseLogFile()
{
	if ( m_LogFile && m_FileSystem )
	{
		m_FileSystem->Flush( m_LogFile );
		m_FileSystem->Close( m_LogFile );
		m_LogFile = NULL;
		Printf( "Console log file closed.\n" );
		memset( m_LogFileName, 0, sizeof( m_LogFileName ) );
	}
}

bool System::AddModule( ISystemModule* module, char* name )
{
	if ( !module )
		return false;
	if ( !module->Init( this, m_NextSerial, name ) )
	{
		Printf( "ERROR! System::AddModule: couldn't initialize module %s.\n", name );
		return false;
	}

	m_Modules.AddHead( module );
	m_NextSerial++;
	return true;
}

bool System::RemoveModule( ISystemModule* module )
{
	if ( !module )
		return true;
	module->ShutDown();

	command_t* command = (command_t*)m_Commands.GetFirst();
	while ( command )
	{
		if ( command->module->GetSerial() == module->GetSerial() )
		{
			m_Commands.Remove( command );
			free( command );
		}
		command = (command_t*)m_Commands.GetNext();
	}

	ISystemModule* entry = (ISystemModule*)m_Modules.GetFirst();
	while ( entry )
	{
		if ( entry->GetSerial() == module->GetSerial() )
		{
			m_Modules.Remove( entry );
			return true;
		}
		entry = (ISystemModule*)m_Modules.GetNext();
	}
	return false;
}

ISystemModule* System::FindModule( char* interfaceName, char* instanceName )
{
	if ( !interfaceName || !*interfaceName )
		return NULL;

	ISystemModule* module = (ISystemModule*)m_Modules.GetFirst();
	while ( module )
	{
		if ( !_stricmp( interfaceName, module->GetType() ) )
		{
			if ( !instanceName || !_stricmp( instanceName, module->GetName() ) )
				return module;
		}
		module = (ISystemModule*)m_Modules.GetNext();
	}
	return NULL;
}

ISystemModule* System::GetModule( char* interfaceName, char* libraryName, char* instanceName )
{
	ISystemModule* module = FindModule( interfaceName, instanceName );
	if ( module )
		return module;

	char filename[MAX_PATH];
	strncpy( filename, libraryName, sizeof( filename ) - 1 );
	filename[sizeof( filename ) - 1] = 0;
#ifdef _WIN32
	for ( char* character = filename; *character; character++ )
	{
		if ( *character == '/' )
			*character = '\\';
	}
#endif

	library_t* library = (library_t*)m_Libraries.GetFirst();
	while ( library && _stricmp( libraryName, library->name ) )
		library = (library_t*)m_Libraries.GetNext();

	if ( !library )
	{
		library = (library_t*)malloc( sizeof( library_t ) );
		if ( !library )
		{
			DPrintf( "ERROR! System::GetLibrary: out of memory (%s).\n", libraryName );
			return NULL;
		}

		_snprintf( library->name, sizeof( library->name ), "%s" LIBRARY_SUFFIX, filename );
		library->module = Sys_LoadModule( library->name );
		if ( !library->module )
		{
			DPrintf( "WARNING! System::GetLibrary: coulnd't load library (%s).\n", library->name );
			free( library );
			return NULL;
		}

		library->factory = Sys_GetFactory( library->module );
		if ( !library->factory )
		{
			DPrintf( "ERROR! System::GetLibrary: coulnd't get object factory(%s).\n", library->name );
			free( library );
			return NULL;
		}

		m_Libraries.Add( library );
		DPrintf( "Loaded library %s.\n", library->name );
	}

	module = (ISystemModule*)library->factory( interfaceName, NULL );
	if ( !module )
	{
		DPrintf( "ERROR! System::GetModule: interface \"%s\" not found in library %s.\n", interfaceName, library->name );
		return NULL;
	}

	AddModule( module, instanceName );
	return module;
}

bool System::RegisterCommand( char* name, ISystemModule* module, int commandID )
{
	command_t* command = (command_t*)m_Commands.GetFirst();
	while ( command )
	{
		if ( !_stricmp( command->name, name ) )
		{
			Printf( "WARNING! System::RegisterCommand: command \"%s\" already exists.\n", name );
			return false;
		}
		command = (command_t*)m_Commands.GetNext();
	}

	command = (command_t*)malloc( sizeof( command_t ) );
	strncpy( command->name, name, sizeof( command->name ) );
	command->module = module;
	command->name[sizeof( command->name ) - 1] = 0;
	command->id = commandID;
	m_Commands.Add( command );
	return true;
}

void System::GetCommandMatches( char* string, ObjectList* matches )
{
	size_t length = strlen( string );
	command_t* command = (command_t*)m_Commands.GetFirst();
	while ( command )
	{
		if ( !strncmp( command->name, string, length ) )
			matches->AddTail( command->name );
		command = (command_t*)m_Commands.GetNext();
	}
}

void System::ExecuteFile( char* filename )
{
	if ( *filename == '/' || strstr( filename, ":" ) || strstr( filename, ".." ) || strstr( filename, "\\" ) )
	{
		Printf( "WARNING! System::ExecuteFile: unable to open %s (contains illegal characters).\n", filename );
		return;
	}

	FileHandle_t file = m_FileSystem->Open( filename, "rt" );
	if ( !file )
	{
		Printf( "Couldn't open config file %s.\n", filename );
		return;
	}

	Printf( "Executing file %s.\n", filename );
	char line[256];
	while ( m_FileSystem->ReadLine( line, sizeof( line ) - 1, file ) )
	{
		char* comment = strstr( line, "//" );
		if ( comment )
			*comment = 0;
		ExecuteString( line );
	}
	m_FileSystem->Close( file );
}

void System::ExecuteString( char* commands )
{
	if ( !commands || !*commands )
		return;

	for ( char* character = commands; *character; character++ )
	{
		if ( (unsigned char)( *character - ' ' ) > '~' - ' ' || *character == '%' )
			*character = ' ';
	}

	char text[256];
	char* output = text;
	bool quoted = false;
	unsigned int length = 1;
	for ( char* character = commands; ; character++, length++ )
	{
		*output = *character;
		if ( *character == '"' )
		{
			quoted = !quoted;
			output++;
		}
		else if ( ( *character == ';' && !quoted ) || !*character )
		{
			*output = 0;
			char* commandText = text;
			while ( *commandText == ' ' )
				commandText++;

			if ( *commandText )
			{
				TokenLine tokens( commandText );
				command_t* command = (command_t*)m_Commands.GetFirst();
				while ( command && _stricmp( command->name, tokens.GetToken( 0 ) ) )
					command = (command_t*)m_Commands.GetNext();

				if ( command )
					command->module->ExecuteCommand( command->id, commandText );
				else
					Printf( "WARNING! System::DispatchCommand: command \"%s\" not registered.\n", commandText );
			}

			output = text;
			length = 0;
			if ( !*character )
				return;
		}
		else
		{
			output++;
		}

		if ( length + 1 >= sizeof( text ) )
		{
			Printf( "WARNING! System::ExecuteString: Command token too long.\n" );
			return;
		}
	}
}

void System::ExecuteCommand( int commandID, char* commandLine )
{
	switch ( commandID )
	{
	case CMD_DEVELOPER:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() == 2 )
		{
			m_Developer = atoi( params.GetToken( 1 ) ) != 0;
		}
		else
		{
			g_System.Printf( "Syntax: developer <0|1>\n" );
			g_System.Printf( "Developer mode is %s.\n", m_Developer ? "on" : "off" );
		}
		break;
	}
	case CMD_EXEC:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() == 2 )
			ExecuteFile( params.GetToken( 1 ) );
		else
			g_System.Printf( "Syntax: exec <file.cfg>\n" );
		break;
	}
	case CMD_MODULES:
	{
		ISystemModule* module = (ISystemModule*)m_Modules.GetFirst();
		while ( module )
		{
			Printf( "Module %s, Interface %s, Version %i\n", module->GetName(), module->GetType(), module->GetVersion() );
			module = (ISystemModule*)m_Modules.GetNext();
		}
		Printf( "--- %i modules in total ---\n", m_Modules.CountElements() );
		break;
	}
	case CMD_LIST:
	{
		Printf( "Registerd console commands:" );
		command_t* command = (command_t*)m_Commands.GetFirst();
		while ( command )
		{
			Printf( "\n %s", command->name );
			DPrintf( " (%s)", command->module->GetName() );
			command = (command_t*)m_Commands.GetNext();
		}
		Printf( "\n" );
		break;
	}
	case CMD_LOGFILE:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() == 2 )
		{
			char* filename = params.GetToken( 1 );
			if ( !_stricmp( filename, "1" ) )
			{
				if ( m_LogFile )
				{
					Printf( "Console is logged to \"%s\".\n", m_LogFileName );
				}
				else
				{
					char name[MAX_PATH];
					sprintf( name, "logfile%s.txt", COM_TimeString() );
					OpenLogFile( name );
				}
			}
			else if ( !_stricmp( filename, "0" ) )
			{
				CloseLogFile();
			}
			else
			{
				OpenLogFile( filename );
			}
		}
		else
		{
			Printf( "Syntax: logfile <0|1|filename>\n" );
			if ( m_LogFile )
				Printf( "Console is logged to \"%s\".\n", m_LogFileName );
			else
				Printf( "Logging is off.\n" );
		}
		break;
	}
	case CMD_QUIT:
		Stop();
		break;
	case CMD_LOADMODULE:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() < 2 )
			g_System.Printf( "Syntax: loadmodule <module> [<library>] [<name>]\n" );
		else if ( params.CountToken() == 2 )
			GetModule( params.GetToken( 1 ), params.GetToken( 1 ), NULL );
		else if ( params.CountToken() == 3 )
			GetModule( params.GetToken( 1 ), params.GetToken( 2 ), NULL );
		else
			GetModule( params.GetToken( 1 ), params.GetToken( 2 ), params.GetToken( 3 ) );
		break;
	}
	case CMD_UNLOADMODULE:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() < 2 )
		{
			Printf( "Syntax: unloadmodule <module> [<name>]\n" );
			break;
		}

		ISystemModule* module = NULL;
		if ( params.CountToken() == 2 )
			module = FindModule( params.GetToken( 1 ), NULL );
		else if ( params.CountToken() == 3 )
			module = FindModule( params.GetToken( 1 ), params.GetToken( 2 ) );

		if ( module )
			RemoveModule( module );
		else
			Printf( "Module not found.\n" );
		break;
	}
	case CMD_TITLE:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() == 2 )
			m_Console.SetTitle( params.GetToken( 1 ) );
		else
			Printf( "Syntax: title <text>\n" );
		break;
	}
	case CMD_SHOWCON:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() == 2 )
			ShowConsole( *params.GetToken( 1 ) != '0' );
		else
			Printf( "Syntax: showcon <0|1>\n" );
		break;
	}
	case CMD_ECHO:
	{
		TokenLine params( commandLine );
		if ( params.CountToken() >= 2 )
			Printf( "%s\n", params.GetRestOfLine( 1 ) );
		break;
	}
	default:
		Printf( "ERROR! System::ExecuteCommand: unknown command ID %i.\n", commandID );
		break;
	}
}

#ifdef _WIN32
bool System::StartVGUI()
{
	if ( !m_FileSystemLibrary )
	{
		Printf( "System::StartVGUI: filesystem not loaded." );
		return false;
	}
	if ( CheckParam( "-novgui" ) )
		return false;

	m_VGuiLibrary = Sys_LoadModule( "vgui2" LIBRARY_SUFFIX );
	m_Factories[0] = Sys_GetFactoryThis();
	m_Factories[1] = Sys_GetFactory( m_VGuiLibrary );
	m_Factories[2] = Sys_GetFactory( m_FileSystemLibrary );
	if ( !vgui2::VGuiControls_Init( "HLTV", m_Factories, 3 ) )
	{
		Printf( "System::StartVGUI: Could not initialize vgui." );
		return false;
	}

	vgui2::filesystem()->AddSearchPath( "platform", "PLATFORM" );
	vgui2::localize()->AddFile( vgui2::filesystem(), "Resource/platform_%language%.txt" );
	vgui2::localize()->AddFile( vgui2::filesystem(), "Resource/vgui_%language%.txt" );
	m_MainPanel = new vgui2::Panel( NULL, "MainPanel" );
	vgui2::surface()->SetEmbeddedPanel( m_MainPanel->GetVPanel() );
	if ( !vgui2::scheme()->LoadSchemeFromFile( "platform/resource/TrackerScheme.res", "DEFAULT" ) )
	{
		Printf( "System::StartVGUI: Could not load TrackerScheme.res\n" );
		return false;
	}

	vgui2::ivgui()->Start();
	m_VGUI = true;
	Printf( "VGUI initialized.\n" );
	return true;
}

bool System::InitVGUI( IVGuiModule* module )
{
	if ( !m_VGUI && !StartVGUI() )
		return false;
	module->Initialize( m_Factories, 3 );
	module->PostInitialize( NULL, 0 );
	module->SetParent( m_MainPanel->GetVPanel() );
	return true;
}

vgui2::Panel* System::GetPanel()
{
	if ( m_VGUI || StartVGUI() )
		return m_MainPanel;
	return NULL;
}
#else
// VGUI2 is not available on Linux: modules use the text console.
bool System::InitVGUI( IVGuiModule* )
{
	return false;
}
#endif

void System_Printf( const char* format, ... )
{
	static char text[8192];
	va_list args;
	va_start( args, format );
	_vsnprintf( text, sizeof( text ), format, args );
	text[sizeof( text ) - 1] = 0;
	va_end( args );
	g_System.Printf( "%s", text );
}

#ifdef _WIN32
int WINAPI WinMain( _In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR commandLine, _In_ int )
{
	g_System.m_CommandLine.SetLine( commandLine );
#else
// Used by the shared text console (common/textconsole.cpp).
void Sys_Printf( char* format, ... )
{
	static char text[8192];
	va_list args;
	va_start( args, format );
	vsnprintf( text, sizeof( text ), format, args );
	va_end( args );
	g_System.Printf( "%s", text );
}

int main( int argc, char** argv )
{
	// Like the Windows command line, without the program name.
	static char commandLine[2048];
	for ( int i = 1; i < argc; ++i )
	{
		if ( i > 1 )
			strncat( commandLine, " ", sizeof( commandLine ) - 1 - strlen( commandLine ) );
		strncat( commandLine, argv[i], sizeof( commandLine ) - 1 - strlen( commandLine ) );
	}
	g_System.m_CommandLine.SetLine( commandLine );
#endif
	if ( g_System.Init( &g_System, 0, "Console" ) )
	{
		g_System.RunFrame( 0 );
		g_System.ShutDown();
	}
	else
	{
		Sleep( 2000 );
	}
	return 0;
}
