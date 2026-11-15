// hlds_linux: text console front end for the dedicated server on POSIX
// systems. Follows the console path of the Windows launcher (sys_ded.cpp)
// without its VGUI administration window.

#include "../common/icommandline.h"
#include "../common/TextConsoleUnix.h"
#include "../public/FileSystem.h"
#include "../public/idedicatedexports.h"

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#define ENGINE_MODULE			"engine_i486.so"
#define FILESYSTEM_MODULE		"filesystem_stdio.so"
#define ENGINE_QUIT_RESULT		3
#define FRAME_SLEEP_MICROSECONDS	1000

class IDedicatedServerAPI : public IBaseInterface
{
public:
	virtual bool Init( char* basedir, char* command, CreateInterfaceFn launcherFactory,
		CreateInterfaceFn filesystemFactory ) = 0;
	virtual int Shutdown() = 0;
	virtual bool RunFrame() = 0;
	virtual void AddConsoleText( char* text ) = 0;
	virtual void UpdateStatus( float* fps, int* activePlayers, int* maxPlayers, char* map ) = 0;
};

class CDedicatedExports : public IDedicatedExports
{
public:
	void Sys_Printf( char* text ) override;
};

static CTextConsoleUnix			console;
static CreateInterfaceFn		filesystemFactory;
static volatile sig_atomic_t	gbAppHasBeenTerminated;

void Sys_Printf( char* format, ... )
{
	char text[1024];
	va_list arguments;
	va_start( arguments, format );
	vsnprintf( text, sizeof( text ), format, arguments );
	va_end( arguments );
	console.Print( text );
}

void CDedicatedExports::Sys_Printf( char* text )
{
	::Sys_Printf( (char*)"%s", text );
}

EXPOSE_SINGLE_INTERFACE( CDedicatedExports, IDedicatedExports, VENGINE_DEDICATEDEXPORTS_API_VERSION );

static void TerminationHandler( int )
{
	gbAppHasBeenTerminated = 1;
}

static int RunServer()
{
	int result = 0;
	while ( !gbAppHasBeenTerminated )
	{
		CSysModule* engineModule = Sys_LoadModule( ENGINE_MODULE );
		if ( !engineModule )
		{
			fprintf( stderr, "Unable to load engine, image is corrupt.\n" );
			return -1;
		}

		CreateInterfaceFn engineFactory = Sys_GetFactory( engineModule );
		IDedicatedServerAPI* engineAPI = engineFactory
			? (IDedicatedServerAPI*)engineFactory( "VENGINE_HLDS_API_VERSION002", NULL )
			: NULL;
		if ( !engineAPI || !engineAPI->Init( (char*)".", (char*)cmdline->GetCmdLine(), Sys_GetFactoryThis(), filesystemFactory ) )
		{
			Sys_UnloadModule( engineModule );
			return -1;
		}

		while ( !gbAppHasBeenTerminated )
		{
			usleep( FRAME_SLEEP_MICROSECONDS );

			char* line = console.GetLine();
			if ( line )
			{
				char command[256];
				snprintf( command, sizeof( command ), "%s\n", line );
				engineAPI->AddConsoleText( command );
			}

			if ( !engineAPI->RunFrame() )
				break;
		}

		result = engineAPI->Shutdown();
		Sys_UnloadModule( engineModule );
		if ( result == ENGINE_QUIT_RESULT )
			break;
	}
	return result;
}

int main( int argc, char** argv )
{
	cmdline->CreateCmdLine( argc, (const char**)argv );

	CSysModule* filesystemModule = Sys_LoadModule( FILESYSTEM_MODULE );
	filesystemFactory = Sys_GetFactory( filesystemModule );
	if ( !filesystemFactory )
	{
		fprintf( stderr, "Unable to load %s.\n", FILESYSTEM_MODULE );
		return -1;
	}

	IFileSystem* fileSystem = (IFileSystem*)filesystemFactory( "VFileSystem009", NULL );
	if ( !fileSystem )
		return -1;
	fileSystem->Mount();

	if ( !console.Init( NULL ) )
	{
		fprintf( stderr, "Failed to initialize console.\n" );
		return 0;
	}

	signal( SIGINT, TerminationHandler );
	signal( SIGTERM, TerminationHandler );
	signal( SIGPIPE, SIG_IGN );

	gbAppHasBeenTerminated = 0;
	RunServer();

	console.ShutDown();
	fileSystem->Unmount();
	Sys_UnloadModule( filesystemModule );
	return 0;
}
