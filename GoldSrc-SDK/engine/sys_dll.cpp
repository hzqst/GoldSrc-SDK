#include "common.h"
#include "filesystem.h"
#include "server.h"
#include "sys.h"
#include "modinfo.h"
#include "pr_dlls.h"
#include "enginecallbacks.h"
#include "host.h"
#include "client.h"
#include "registry.h"
#include "cmd.h"
#include "keys.h"
#include "steam_api.h"
#include "ivideomode.h"
#include "../external/SDL2/SDL.h"
#include "mem.h"
#include "zone.h"
#include "conprint.h"
#include "eiface.h"

#if !defined( _WIN32 )
#include <ctime>
#include <dlfcn.h>
#include <unistd.h>
#else
#include <Windows.h>
#include <VersionHelpers.h>
#if defined( SWDS_DLL )
#include <process.h>
#endif
#endif
#include <cstdarg>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

qboolean g_bIsDedicatedServer;
DLL_FUNCTIONS gEntityInterface;
NEW_DLL_FUNCTIONS gNewDLLFunctions;
int Win32AtLeastV4;
modinfo_t gmodinfo;
extensiondll_t g_rgextdll[MAX_EXT_DLLS];
int g_iextdllMac;
int giActive;
int giStateInfo;
int giSubState;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iState - 
//-----------------------------------------------------------------------------
void GameSetState( int state )
{
	giActive = state;
}

void GameSetSubState( int state )
{
	constexpr int SUBSTATE_NORMALIZE_MASK = 2;
	constexpr int SUBSTATE_NORMALIZED = 1;
	if ( state & SUBSTATE_NORMALIZE_MASK )
		giSubState = SUBSTATE_NORMALIZED;
	else if ( state != SUBSTATE_NORMALIZED )
		giSubState = state;
}

qboolean gfExtendedError;
char gszDisconnectReason[DISCONNECT_REASON_BYTES];
char gszExtendedDisconnectReason[DISCONNECT_REASON_BYTES];
void ( *Console_Printf )( char* format, ... );
extern void ( *Launcher_ConsolePrintf )( char* format, ... );

constexpr int ENGINE_FUNCTION_TABLE_BYTES = 636;
constexpr int GAME_FUNCTION_TABLE_BYTES = 200;
static_assert( sizeof( enginefuncs_t ) == ENGINE_FUNCTION_TABLE_BYTES );
static_assert( sizeof( DLL_FUNCTIONS ) == GAME_FUNCTION_TABLE_BYTES );

enginefuncs_t g_engfuncsExportedToDlls = { PF_precache_model_I, PF_precache_sound_I, PF_setmodel_I, PF_modelindex,
	ModelFrames, PF_setsize_I, PF_changelevel_I, PF_setspawnparms_I, SaveSpawnParms, PF_vectoyaw_I, PF_vectoangles_I,
	SV_MoveToOrigin_I, PF_changeyaw_I, PF_changepitch_I, FindEntityByString, GetEntityIllum, FindEntityInSphere,
	PF_checkclient_I, PVSFindEntities, PF_makevectors_I, AngleVectors, PF_Spawn_I, PF_Remove_I, CreateNamedEntity,
	PF_makestatic_I, PF_checkbottom_I, PF_droptofloor_I, PF_walkmove_I, PF_setorigin_I, PF_sound_I, PF_ambientsound_I,
	PF_traceline_I, PF_TraceToss_DLL, TraceMonsterHull, PF_tracehull_I, TraceModel, TraceTexture, TraceSphere, PF_aim_I,
	PF_localcmd_I, PF_localexec_I, PF_stuffcmd_I, PF_particle_I, PF_lightstyle_I, PF_DecalIndex, PF_pointcontents_I,
	PF_MessageBegin_I, PF_MessageEnd_I, PF_WriteByte_I, PF_WriteChar_I, PF_WriteShort_I, PF_WriteLong_I,
	PF_WriteAngle_I, PF_WriteCoord_I, PF_WriteString_I, PF_WriteEntity_I, CVarRegister, CVarGetFloat, CVarGetString,
	CVarSetFloat, CVarSetString, AlertMessage, EngineFprintf, PvAllocEntPrivateData, PvEntPrivateData,
	FreeEntPrivateData, SzFromIndex, AllocEngineString, GetVarsOfEnt, PEntityOfEntOffset, EntOffsetOfPEntity,
	IndexOfEdict, PEntityOfEntIndex, FindEntityByVars, GetModelPtr, RegUserMsg, AnimationAutomove, GetBonePosition,
	FunctionFromName, NameForFunction, ClientPrintf, ServerPrint, Cmd_Args, Cmd_Argv, Cmd_Argc, GetAttachment,
	CRC32_Init, CRC32_ProcessBuffer, CRC32_ProcessByte, CRC32_Final, RandomLong, RandomFloat, PF_setview_I, PF_Time,
	PF_crosshairangle_I, COM_LoadFileForMe, COM_FreeFile, Host_EndSection, COM_CompareFileTime, COM_GetGameDir,
	Cvar_RegisterVariable, PF_FadeVolume, PF_SetClientMaxspeed, PF_CreateFakeClient_I, PF_RunPlayerMove_I,
	PF_NumberOfEntities_I, PF_GetInfoKeyBuffer_I, PF_InfoKeyValue_I, PF_SetKeyValue_I, PF_SetClientKeyValue_I,
	PF_IsMapValid_I, PF_StaticDecal, PF_precache_generic_I, PF_GetPlayerUserId, PF_BuildSoundMsg_I,
	PF_IsDedicatedServer, CVarGetPointer, PF_GetPlayerWONId, PF_RemoveKey_I, PF_GetPhysicsKeyValue,
	PF_SetPhysicsKeyValue, PF_GetPhysicsInfoString, EV_Precache, EV_Playback, SV_FatPVS, SV_FatPAS, SV_CheckVisibility,
	DELTA_SetField, DELTA_UnsetField, DELTA_AddEncoder, PF_GetCurrentPlayer, PF_CanSkipPlayer, DELTA_FindFieldIndex,
	DELTA_SetFieldByIndex, DELTA_UnsetFieldByIndex, PF_SetGroupMask, PF_CreateInstancedBaseline, PF_Cvar_DirectSet,
	PF_ForceUnmodified, PF_GetPlayerStats, Cmd_AddGameCommand, Voice_GetClientListening, Voice_SetClientListening,
	PF_GetPlayerAuthId, SequenceGet, SequencePickSentence, COM_FileSize, COM_GetApproxWavePlayLength,
	VGuiWrap2_IsInCareerMatch, VGuiWrap2_GetLocalizedStringLength, RegisterTutorMessageShown, GetTimesTutorMessageShown,
	ProcessTutorMessageDecayBuffer, ConstructTutorMessageDecayBuffer, ResetTutorMessageDecayData, QueryClientCvarValue,
	QueryClientCvarValue2, EngCheckParm, PEntityOfEntIndexAllEntities };

#if defined( _WIN32 )
void Sys_CheckOSVersion();
#endif

void SV_ResetModInfo();
void DLL_SetModKey( modinfo_t* info, char* key, char* value );

static FileFindHandle_t g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;

static double curtime;
static double lastcurtime;

#if !defined( _WIN32 )
static bool monotonicClockInitialized;
static struct timespec monotonicStart;

static void Sys_InitializeMonotonicClock()
{
	if ( !monotonicClockInitialized )
	{
		monotonicClockInitialized = true;
		clock_gettime( CLOCK_MONOTONIC, &monotonicStart );
	}
}

double Sys_FloatTime()
{
	Sys_InitializeMonotonicClock();
	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	return static_cast<double>( now.tv_sec - monotonicStart.tv_sec ) + static_cast<double>( now.tv_nsec ) * 1e-9;
}
#else
static LARGE_INTEGER performanceFrequency;
static LARGE_INTEGER performanceStart;
static LONGLONG performanceTicksPerMillisecond;

static void Sys_InitializePerformanceCounter()
{
	if ( performanceFrequency.QuadPart == 0 )
	{
		constexpr int MILLISECONDS_PER_SECOND = 1000;
		QueryPerformanceFrequency( &performanceFrequency );
		performanceTicksPerMillisecond = performanceFrequency.QuadPart / MILLISECONDS_PER_SECOND;
		QueryPerformanceCounter( &performanceStart );
	}
}

double Sys_FloatTime()
{
	Sys_InitializePerformanceCounter();
	LARGE_INTEGER counter;
	QueryPerformanceCounter( &counter );
	LONGLONG elapsed = static_cast<LONGLONG>(
		static_cast<ULONGLONG>( counter.QuadPart ) - static_cast<ULONGLONG>( performanceStart.QuadPart ) );
	return static_cast<double>( elapsed ) / static_cast<double>( performanceFrequency.QuadPart );
}
#endif

void Sys_InitFloatTime()
{
#if !defined( _WIN32 )
	Sys_InitializeMonotonicClock();
#else
	Sys_InitializePerformanceCounter();
	LARGE_INTEGER counter;
	QueryPerformanceCounter( &counter );
#endif
	int parameter = COM_CheckParm( "-starttime" );
	if ( parameter )
		curtime = Q_atof( com_argv[parameter + 1] );
	else
		curtime = 0.0;
	lastcurtime = curtime;
}

void Sys_Init()
{
#if defined( _WIN32 )
	Sys_CheckOSVersion();
#endif
	Sys_InitFloatTime();
}

void Sys_ShutdownFloatTime()
{
	lastcurtime = 0.0;
	curtime = 0.0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			*basename - 
// Output : char *Sys_FindFirst
//-----------------------------------------------------------------------------
const char* Sys_FindFirst( const char* path, char* basename )
{
	if ( g_hfind != FILESYSTEM_INVALID_FIND_HANDLE )
		Sys_Error( "Sys_FindFirst without close" );

	const char* filename = FS_FindFirst( path, &g_hfind, nullptr );
	if ( basename && filename )
		COM_FileBase( filename, basename );
	return filename;
}

const char* Sys_FindFirstPathID( const char* path, char* pathid )
{
	if ( g_hfind != FILESYSTEM_INVALID_FIND_HANDLE )
		Sys_Error( "Sys_FindFirst without close" );

	return FS_FindFirst( path, &g_hfind, pathid );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *basename - 
// Output : char *Sys_FindNext
//-----------------------------------------------------------------------------
const char* Sys_FindNext( char* basename )
{
	const char* filename = FS_FindNext( g_hfind );
	if ( basename && filename )
		COM_FileBase( filename, basename );
	return filename;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Sys_FindClose
//-----------------------------------------------------------------------------

void Sys_FindClose()
{
	if ( g_hfind != FILESYSTEM_INVALID_FIND_HANDLE )
	{
		FS_FindClose( g_hfind );
		g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;
	}
}

void Sys_Printf( char* format, ... )
{
	constexpr int SYSTEM_PRINT_BUFFER_BYTES = 1024;
	char text[SYSTEM_PRINT_BUFFER_BYTES];
	va_list arguments;
	va_start( arguments, format );
#if defined( _WIN32 ) && defined( SWDS_DLL )
	_vsnprintf( text, sizeof( text ), format, arguments );
#else
	std::vsnprintf( text, sizeof( text ), format, arguments );
#endif
	va_end( arguments );

	if ( g_bIsDedicatedServer )
	{
		if ( Launcher_ConsolePrintf )
			Launcher_ConsolePrintf( const_cast<char*>( "%s" ), text );
	}
#if defined( _WIN32 )
	OutputDebugStringA( text );
#else
	else
	{
		std::fprintf( stderr, "%s\n", text );
	}
#endif
}

void Sys_SetRegKeyValue( const char* subkey, const char* valueName, const char* value )
{
#if defined( _WIN32 )
	HKEY key;
	DWORD disposition;

	if ( RegCreateKeyExA( HKEY_CURRENT_USER, subkey, 0, (char*)"String", REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS, NULL, &key, &disposition ) != ERROR_SUCCESS )
		return;

	RegSetValueExA( key, valueName, 0, REG_SZ, (const BYTE*)value, Q_strlen( value ) + 1 );
	RegCloseKey( key );
#else
	( void )subkey;
	registry->WriteString( valueName, value );
#endif
}

#if defined( _WIN32 )
void Sys_GetRegKeyValue( const char* keyPath, const char* valueName, char* output, int outputLength,
	const char* defaultValue )
{
	char data[1024];
	HKEY key;
	DWORD disposition;
	DWORD type;
	DWORD size = outputLength;

	_snprintf( output, static_cast<std::size_t>( outputLength ), "%s", defaultValue );
	if ( outputLength > (int)sizeof( data ) )
		return;

	HKEY root = HKEY_CURRENT_USER;
	if ( !_strnicmp( keyPath, "HKEY_LOCAL_MACHINE", sizeof( "HKEY_LOCAL_MACHINE" ) - 1 ) )
	{
		root = HKEY_LOCAL_MACHINE;
		keyPath += sizeof( "HKEY_LOCAL_MACHINE" );
	}

	if ( RegCreateKeyExA( root, keyPath, 0, (char*)"String", REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS, NULL, &key, &disposition ) != ERROR_SUCCESS )
		return;

	if ( disposition == REG_CREATED_NEW_KEY
		|| RegQueryValueExA( key, valueName, NULL, &type, (BYTE*)data, &size ) != ERROR_SUCCESS )
	{
		RegSetValueExA( key, valueName, 0, REG_SZ, (const BYTE*)defaultValue, Q_strlen( defaultValue ) + 1 );
	}
	else if ( type == REG_SZ )
	{
		Q_strncpy( output, data, outputLength );
		output[outputLength - 1] = '\0';
	}

	RegCloseKey( key );
}
#endif

DISPATCHFUNCTION GetDispatch( const char* name )
{
	for ( int index = 0; index < g_iextdllMac; ++index )
	{
#if !defined( _WIN32 )
		void* function = dlsym( reinterpret_cast<void*>( g_rgextdll[index].lDLLHandle ), name );
#else
		FARPROC function = GetProcAddress( g_rgextdll[index].lDLLHandle, name );
#endif
		if ( function )
			return reinterpret_cast<DISPATCHFUNCTION>( function );
	}
	return nullptr;
}

ENTITYINIT GetEntityInit( const char* classname )
{
	return reinterpret_cast<ENTITYINIT>( GetDispatch( classname ) );
}

void LoadEntityDLLs( char* basedir )
{
	constexpr int MAX_GAME_LISTING_SIZE = 0x40000;
	SV_ResetModInfo();
	g_iextdllMac = 0;
	Q_memset( g_rgextdll, 0, MAX_EXT_DLLS * sizeof( extensiondll_t ) );
	char gameDirectory[64];
	Q_strncpy( gameDirectory, com_gamedir, sizeof( gameDirectory ) - 1 );
	if ( Q_strcasecmp( gameDirectory, "valve" ) != 0 )
		gmodinfo.bIsMod = true;

	char listFilename[260];
	char dllFilename[8192];
	_snprintf( listFilename, sizeof( listFilename ), "%s", "liblist.gam" );
	FileHandle_t file = FS_Open( listFilename, "rb" );
	if ( file )
	{
		int fileSize = static_cast<int>( FS_Size( file ) );
		if ( fileSize == 0 || fileSize > MAX_GAME_LISTING_SIZE )
			Sys_Error( "Game listing file size is bogus [%s: size %i]", "liblist.gam", fileSize );
		int allocationSize = static_cast<int>( static_cast<unsigned int>( fileSize ) + 1u );
		char* listing = static_cast<char*>( Mem_Malloc( allocationSize ) );
		if ( !listing )
			Sys_Error( "Could not allocate space for game listing file of %i bytes", allocationSize );
		int bytesRead = FS_Read( listing, fileSize, 1, file );
		if ( bytesRead != fileSize )
			Sys_Error( "Error reading in game listing file, expected %i bytes, read %i", fileSize, bytesRead );
		listing[fileSize] = 0;
		com_ignorecolons = true;
		const char* cursor = COM_Parse( listing );
		while ( Q_strlen( com_token ) > 0 )
		{
			char key[64], value[256];
			Q_strncpy( key, com_token, sizeof( key ) - 1 );
			key[sizeof( key ) - 1] = 0;
			cursor = COM_Parse( cursor );
			Q_strncpy( value, com_token, sizeof( value ) - 1 );
			value[sizeof( value ) - 1] = 0;
#if !defined( _WIN32 )
			if ( Q_strcasecmp( key, "gamedll_linux" ) != 0 )
#else
			if ( Q_strcasecmp( key, "gamedll" ) != 0 )
#endif
			{
				DLL_SetModKey( &gmodinfo, key, value );
			}
			else
			{
				int parameter = COM_CheckParm( "-dll" );
				if ( parameter && parameter < com_argc - 1 )
				{
					Q_strncpy( value, com_argv[parameter + 1], sizeof( value ) - 1 );
					value[sizeof( value ) - 1] = 0;
				}
				char* parentDirectory;
				if ( value[0] == '/' || value[0] == '\\' || Q_strstr( value, ":" )
					 || ( ( parentDirectory = Q_strstr( value, ".." ) ) && Q_strstr( parentDirectory + 2, ".." ) ) )
				{
					Con_DPrintf( "Skipping library with illegal characters in path: %s\n", value );
				}
				else
				{
#if !defined( _WIN32 )
					char* underscore = std::strchr( value, '_' );
					if ( underscore != nullptr )
					{
						*underscore = '\0';
						Q_strcat( value, ".so" );
					}
					if ( Q_strcasecmp( COM_LastFileExtension( value ), "so" ) != 0 )
					{
						Con_DPrintf( "Skipping non-shared library:  %s\n", value );
					}
					else
					{
						FS_GetLocalPath( value, dllFilename, sizeof( dllFilename ) );
						Con_DPrintf( "\nAdding:  %s/%s\n", gameDirectory, value );
						LoadThisDll( dllFilename );
					}
#else
					if ( Q_strcasecmp( COM_FileExtension( value ), "dll" ) != 0 )
					{
						Con_DPrintf( "Skipping non-dll:  %s\n", value );
					}
					else
					{
						FS_GetLocalPath( value, dllFilename, sizeof( dllFilename ) );
						Con_DPrintf( "\nAdding:  %s/%s\n", gameDirectory, value );
						LoadThisDll( dllFilename );
					}
#endif
				}
			}
			cursor = COM_Parse( cursor );
		}
		com_ignorecolons = false;
		std::free( listing );
		FS_Close( file );
	}
	else
	{
		char wildcard[260];
#if !defined( _WIN32 )
		std::snprintf( wildcard, sizeof( wildcard ), "%s/*.so", "valve/dlls" );
#else
		_snprintf( wildcard, sizeof( wildcard ), "%s\\*.dll", "valve\\dlls" );
#endif
		for ( const char* filename = Sys_FindFirst( wildcard, nullptr ); filename; filename = Sys_FindNext( nullptr ) )
		{
#if !defined( _WIN32 )
			std::snprintf( dllFilename, sizeof( dllFilename ), "%s/%s/%s", basedir, "valve/dlls", filename );
#else
			_snprintf( dllFilename, sizeof( dllFilename ), "%s/%s/%s", basedir, "valve\\dlls", filename );
#endif
			LoadThisDll( dllFilename );
		}
		Sys_FindClose();
	}

	gNewDLLFunctions.pfnCvarValue2 = nullptr;
	gNewDLLFunctions.pfnOnFreeEntPrivateData = nullptr;
	gNewDLLFunctions.pfnGameShutdown = nullptr;
	gNewDLLFunctions.pfnShouldCollide = nullptr;
	gNewDLLFunctions.pfnCvarValue = nullptr;
	NEW_DLL_FUNCTIONS_FN getNewFunctions =
		reinterpret_cast<NEW_DLL_FUNCTIONS_FN>( GetDispatch( "GetNewDLLFunctions" ) );
	int interfaceVersion;
	if ( getNewFunctions )
	{
		interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		getNewFunctions( &gNewDLLFunctions, &interfaceVersion );
	}
	APIFUNCTION2 getEntityAPI2 = reinterpret_cast<APIFUNCTION2>( GetDispatch( "GetEntityAPI2" ) );
	if ( getEntityAPI2 )
	{
		interfaceVersion = INTERFACE_VERSION;
		if ( !getEntityAPI2( &gEntityInterface, &interfaceVersion ) )
		{
			Con_Printf( "==================\n" );
			Con_Printf( "Game DLL version mismatch\n" );
			Con_Printf( "DLL version is %i, engine version is %i\n", interfaceVersion, INTERFACE_VERSION );
			if ( interfaceVersion <= INTERFACE_VERSION )
				Con_Printf( "The game DLL for %s appears to be outdated, check for updates\n", gameDirectory );
			else
				Con_Printf( "Engine appears to be outdated, check for updates\n" );
			Con_Printf( "==================\n" );
			Host_Error( "\n" );
		}
	}
	else
	{
		APIFUNCTION getEntityAPI = reinterpret_cast<APIFUNCTION>( GetDispatch( "GetEntityAPI" ) );
		if ( !getEntityAPI )
			Host_Error( "Couldn't get DLL API from %s!", dllFilename );
		interfaceVersion = INTERFACE_VERSION;
		if ( !getEntityAPI( &gEntityInterface, INTERFACE_VERSION ) )
		{
			Con_Printf( "==================\n" );
			Con_Printf( "Game DLL version mismatch\n" );
			Con_Printf( "The game DLL for %s appears to be outdated, check for updates\n", gameDirectory );
			Con_Printf( "==================\n" );
			Host_Error( "\n" );
		}
	}
	const char* description = gEntityInterface.pfnGetGameDescription();
	Con_DPrintf( "Dll loaded for %s %s\n", gmodinfo.bIsMod ? "mod" : "game", description );
}

#if defined( _WIN32 )
static byte* GetDirectoryInFile( byte* image, unsigned int directory )
{
	IMAGE_NT_HEADERS32* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>( image + reinterpret_cast<IMAGE_DOS_HEADER*>( image )->e_lfanew );
	IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION( nt );
	int sectionCount = nt->FileHeader.NumberOfSections;
	int i;

	if ( directory >= nt->OptionalHeader.NumberOfRvaAndSizes )
		return NULL;

	DWORD address = nt->OptionalHeader.DataDirectory[directory].VirtualAddress;
	for ( i = 0; i < sectionCount; i++, section++ )
	{
		if ( section->VirtualAddress <= address && section->VirtualAddress + section->SizeOfRawData > address )
			break;
	}

	if ( i + 1 > sectionCount )
		return NULL;

	return image + address + section->PointerToRawData - section->VirtualAddress;
}

static char* GetExportNamesInFile( byte* image )
{
	IMAGE_EXPORT_DIRECTORY* exports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>( GetDirectoryInFile( image, IMAGE_DIRECTORY_ENTRY_EXPORT ) );
	if ( !exports )
		return NULL;

	IMAGE_NT_HEADERS32* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>( image + reinterpret_cast<IMAGE_DOS_HEADER*>( image )->e_lfanew );
	int sectionCount = nt->FileHeader.NumberOfSections;
	IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION( nt );
	if ( !section )
		return NULL;

	for ( int i = 0; i < sectionCount; i++, section++ )
	{
		if ( !strcmp( reinterpret_cast<char*>( section->Name ), ".rdata" ) )
		{
			DWORD* names = reinterpret_cast<DWORD*>( image + exports->AddressOfNames - section->VirtualAddress + section->PointerToRawData );
			return reinterpret_cast<char*>( image + names[0] - section->VirtualAddress + section->PointerToRawData );
		}
	}

	return NULL;
}

static int GetExportNamesLength( byte* image )
{
	IMAGE_EXPORT_DIRECTORY* exports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>( GetDirectoryInFile( image, IMAGE_DIRECTORY_ENTRY_EXPORT ) );
	if ( !exports )
		return 0;

	char* first = GetExportNamesInFile( image );
	if ( !first )
		return 0;

	char* name = first;
	for ( int i = 0; i < (int)exports->NumberOfNames; i++ )
		name += strlen( name ) + 1;

	return name - first;
}

static void CopyExportNames( byte* image, char* output )
{
	IMAGE_EXPORT_DIRECTORY* exports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>( GetDirectoryInFile( image, IMAGE_DIRECTORY_ENTRY_EXPORT ) );
	if ( !exports )
		return;

	char* first = GetExportNamesInFile( image );
	if ( !first )
		return;

	char* name = first;
	for ( int i = 0; i < (int)exports->NumberOfNames; i++ )
		name += strlen( name ) + 1;

	memcpy( output, first, name - first );
}

static int GetExportNames( const char* filename, char** names )
{
	OFSTRUCT openBuffer;

	*names = NULL;
	FS_GetLocalCopy( filename );

	// OpenFile reports failure with HFILE_ERROR, not NULL
	HFILE opened = OpenFile( filename, &openBuffer, OF_READ );
	if ( opened == HFILE_ERROR )
		return 0;

	HANDLE file = reinterpret_cast<HANDLE>( static_cast<INT_PTR>( opened ) );

	HANDLE mapping = CreateFileMappingA( file, NULL, PAGE_READONLY, 0, 0, NULL );
	if ( !mapping )
	{
		CloseHandle( file );
		return 0;
	}

	byte* image = static_cast<byte*>( MapViewOfFile( mapping, FILE_MAP_READ, 0, 0, 0 ) );
	if ( !image )
	{
		CloseHandle( mapping );
		CloseHandle( file );
		return 0;
	}

	char* buffer = NULL;
	int length = GetExportNamesLength( image );
	if ( length )
	{
		buffer = static_cast<char*>( Hunk_TempAlloc( length ) );
		// a full hunk returns NULL, report no exports
		if ( !buffer )
			length = 0;
		else
			CopyExportNames( image, buffer );
	}

	UnmapViewOfFile( image );
	CloseHandle( mapping );
	CloseHandle( file );
	*names = buffer;
	return length;
}

static char* ConvertExportName( const char* exportName, char* output, int outputSize )
{
	int i;
	int separators = 0;

	for ( i = 1; i < outputSize; i++ )
	{
		output[i - 1] = exportName[i];
		if ( exportName[i] == '@' )
			separators++;
		if ( separators >= 2 )
		{
			i++;
			break;
		}
	}

	output[i - 2] = '\0';
	if ( i == outputSize )
		Con_DPrintf( "Export name is too large\n" );

	return output;
}

void BuildExportTable( extensiondll_t* pDll, const char* filename )
{
	char* names = NULL;
	char convertedName[256];
	int namesLength;
	int count;
	int nameBytes;
	int offset;
	int index;
	char* output;

	if ( !pDll || !filename )
		return;

	namesLength = GetExportNames( filename, &names );
	pDll->functionTable = NULL;
	pDll->functionCount = 0;
	if ( !names )
		return;

	count = 0;
	nameBytes = 0;
	for ( offset = 0; offset < namesLength; offset += strlen( &names[offset] ) + 1 )
	{
		if ( names[offset] == '?' )
		{
			count++;
			ConvertExportName( &names[offset], convertedName, sizeof( convertedName ) );
			nameBytes += strlen( convertedName ) + 1;
		}
	}

	pDll->functionCount = count;
	pDll->functionTable = static_cast<functiontable_t*>( Mem_Malloc( nameBytes + sizeof( functiontable_t ) * count ) );
	output = reinterpret_cast<char*>( &pDll->functionTable[count] );

	index = 0;
	for ( offset = 0; offset < namesLength; offset += strlen( &names[offset] ) + 1 )
	{
		if ( names[offset] == '?' )
		{
			ConvertExportName( &names[offset], output, namesLength - offset );
			pDll->functionTable[index].pFunctionName = output;
			pDll->functionTable[index].pFunction = static_cast<uint32>( reinterpret_cast<std::uintptr_t>( GetProcAddress( pDll->lDLLHandle, &names[offset] ) ) );
			output += strlen( output ) + 1;
			index++;
		}
	}
}
#endif

void LoadThisDll( char* filename )
{
#if !defined( _WIN32 )
	void* library = dlopen( filename, RTLD_NOW );
	if ( library == nullptr )
	{
		const char* error = dlerror();
		Con_Printf( "LoadLibrary failed on %s: %s\n", filename, error );
		return;
	}
	using PFN_GiveFnptrsToDll = void ( * )( enginefuncs_t*, globalvars_t* );
	PFN_GiveFnptrsToDll giveFunctions = reinterpret_cast<PFN_GiveFnptrsToDll>( dlsym( library, "GiveFnptrsToDll" ) );
	if ( giveFunctions == nullptr )
	{
		Con_Printf( "Couldn't get GiveFnptrsToDll in %s\n", filename );
		dlclose( library );
		return;
	}
	giveFunctions( &g_engfuncsExportedToDlls, &gGlobalVariables );
	if ( g_iextdllMac == MAX_EXT_DLLS )
	{
		Con_Printf( "Too many DLLs, ignoring remainder\n" );
		dlclose( library );
		return;
	}
	extensiondll_t* dll = &g_rgextdll[g_iextdllMac++];
	Q_memset( dll, 0, sizeof( *dll ) );
	dll->lDLLHandle = reinterpret_cast<HINSTANCE>( library );
#else
	HMODULE library = static_cast<HMODULE>( FS_LoadLibrary( filename ) );
	if ( !library )
	{
		DWORD error = GetLastError();
		Con_Printf( "LoadLibrary failed on %s (%d)\n", filename, error );
		return;
	}
	typedef void( __stdcall * PFN_GiveFnptrsToDll )( enginefuncs_t*, globalvars_t* );
	PFN_GiveFnptrsToDll giveFunctions =
		reinterpret_cast<PFN_GiveFnptrsToDll>( GetProcAddress( library, "GiveFnptrsToDll" ) );
	if ( !giveFunctions )
	{
		Con_Printf( "Couldn't get GiveFnptrsToDll in %s\n", filename );
		FreeLibrary( library );
		return;
	}
	giveFunctions( &g_engfuncsExportedToDlls, &gGlobalVariables );
	if ( g_iextdllMac == MAX_EXT_DLLS )
	{
		Con_Printf( "Too many DLLs, ignoring remainder\n" );
		FreeLibrary( library );
		return;
	}
	extensiondll_t* dll = &g_rgextdll[g_iextdllMac++];
	Q_memset( dll, 0, sizeof( *dll ) );
	dll->lDLLHandle = library;
	BuildExportTable( dll, filename );
#endif
}

void DLL_SetModKey( modinfo_t* info, char* key, char* value )
{
	constexpr int MINIMUM_MOD_EDICTS = 1200;
	if ( Q_strcasecmp( key, "url_info" ) == 0 )
	{
		info->bIsMod = true;
		Q_strncpy( info->szInfo, value, sizeof( info->szInfo ) - 1 );
		info->szInfo[sizeof( info->szInfo ) - 1] = 0;
	}
	else if ( Q_strcasecmp( key, "url_dl" ) == 0 )
	{
		info->bIsMod = true;
		Q_strncpy( info->szDL, value, sizeof( info->szDL ) - 1 );
		info->szDL[sizeof( info->szDL ) - 1] = 0;
	}
	else if ( Q_strcasecmp( key, "version" ) == 0 )
	{
		info->bIsMod = true;
		info->version = std::atoi( value );
	}
	else if ( Q_strcasecmp( key, "size" ) == 0 )
	{
		info->bIsMod = true;
		info->size = std::atoi( value );
	}
	else if ( Q_strcasecmp( key, "svonly" ) == 0 )
	{
		info->bIsMod = true;
		info->svonly = std::atoi( value ) != 0;
	}
	else if ( Q_strcasecmp( key, "cldll" ) == 0 )
	{
		info->bIsMod = true;
		info->cldll = std::atoi( value ) != 0;
	}
	else if ( Q_strcasecmp( key, "secure" ) == 0 )
	{
		info->bIsMod = true;
		info->secure = std::atoi( value ) != 0;
	}
	else if ( Q_strcasecmp( key, "hlversion" ) == 0 )
	{
		Q_strncpy( info->szHLVersion, value, sizeof( info->szHLVersion ) - 1 );
		info->szHLVersion[sizeof( info->szHLVersion ) - 1] = 0;
	}
	else if ( Q_strcasecmp( key, "edicts" ) == 0 )
	{
		info->num_edicts = std::atoi( value );
		if ( info->num_edicts < MINIMUM_MOD_EDICTS )
			info->num_edicts = MINIMUM_MOD_EDICTS;
	}
	else if ( Q_strcasecmp( key, "crcclientdll" ) == 0 )
	{
		info->bIsMod = true;
		info->clientcrccheck = std::atoi( value ) != 0;
	}
	else if ( Q_strcasecmp( key, "type" ) == 0 )
	{
		if ( _stricmp( value, "singleplayer_only" ) == 0 )
			info->type = SINGLEPLAYER_ONLY;
		else
			info->type = _stricmp( value, "multiplayer_only" ) == 0 ? MULTIPLAYER_ONLY : BOTH;
	}
	else if ( Q_strcasecmp( key, "fallback_dir" ) == 0 )
	{
		COM_AddDefaultDir( value );
	}
}

qboolean Voice_GetClientListening( int receiver, int sender )
{
	if ( receiver <= 0 || receiver > svs.maxclients || sender <= 0 || sender > svs.maxclients )
		return 0;

	int receiverIndex = receiver - 1;
	int senderIndex = sender - 1;
	constexpr int VOICE_STREAM_BITS = 32;
	std::uint32_t mask = std::uint32_t{ 1 } << ( receiverIndex & ( VOICE_STREAM_BITS - 1 ) );
	return static_cast<qboolean>(
		mask & ( svs.clients[senderIndex].m_VoiceStreams[receiverIndex / VOICE_STREAM_BITS] != 0 ) );
}

qboolean Voice_SetClientListening( int receiver, int sender, qboolean listen )
{
	if ( receiver <= 0 || receiver > svs.maxclients || sender <= 0 || sender > svs.maxclients )
		return 0;

	int receiverIndex = receiver - 1;
	int senderIndex = sender - 1;
	constexpr int VOICE_STREAM_BITS = 32;
	std::uint32_t mask = std::uint32_t{ 1 } << ( receiverIndex & ( VOICE_STREAM_BITS - 1 ) );
	std::uint32_t& streams = svs.clients[senderIndex].m_VoiceStreams[receiverIndex / VOICE_STREAM_BITS];
	if ( listen )
		streams |= mask;
	else
		streams &= ~mask;
	return 1;
}

namespace
{
constexpr int ALERT_BUFFER_SIZE = 1024;
constexpr int FUNCTION_NAME_BUFFER_SIZE = 512;

bool IsFunctionNameDigit( char character )
{
	return static_cast<unsigned char>( character - '0' ) <= 9;
}

const char* ConvertNameToLocalPlatform( const char* name )
{
	if ( std::strstr( name, "@" ) )
		return name;
	if ( name[0] != '_' || name[1] != 'Z' )
		return "unknown";

	char encodedName[FUNCTION_NAME_BUFFER_SIZE];
	std::strncpy( encodedName, name + 3, sizeof( encodedName ) );
	encodedName[sizeof( encodedName ) - 1] = '\0';
	char* cursor = encodedName;
	char* end = encodedName + std::strlen( encodedName );
	const int classNameLength = std::atoi( encodedName );
	while ( IsFunctionNameDigit( *cursor ) && cursor < end )
		++cursor;
	const char* className = cursor;
	cursor += classNameLength;
	const int methodNameLength = std::atoi( cursor );
	*cursor = '\0';
	do
	{
		++cursor;
	} while ( IsFunctionNameDigit( *cursor ) && cursor < end );
	cursor[methodNameLength] = '\0';

	static char convertedName[FUNCTION_NAME_BUFFER_SIZE];
	_snprintf( convertedName, sizeof( convertedName ), "%s@%s", cursor, className );
	return convertedName;
}
}

void AlertMessage( ALERT_TYPE type, char* format, ... )
{
	static char message[ALERT_BUFFER_SIZE];
	va_list args;
	va_start( args, format );
	if ( type == at_logged && svs.maxclients > 1 )
	{
		_vsnprintf( message, sizeof( message ), format, args );
		Log_Printf( "%s", message );
	}
	else if ( developer.value != 0.0f )
	{
		switch ( type )
		{
		case at_notice:
			Q_strcpy( message, "NOTE:  " );
			break;
		case at_aiconsole:
			if ( developer.value < 2.0f )
			{
				va_end( args );
				return;
			}
			message[0] = '\0';
			break;
		case at_console:
			message[0] = '\0';
			break;
		case at_warning:
			Q_strcpy( message, "WARNING:  " );
			break;
		case at_error:
			Q_strcpy( message, "ERROR:  " );
			break;
		default:
			break;
		}
		const std::size_t remaining = sizeof( message ) - Q_strlen( message );
		char* output = message + Q_strlen( message );
		_vsnprintf( output, remaining, format, args );
		Con_Printf( "%s", message );
	}
	va_end( args );
}

void EngineFprintf( void*, char*, ... )
{
	AlertMessage( at_console, const_cast<char*>( "EngineFprintf:  Obsolete API\n" ) );
}

uint32 FunctionFromName( const char* name )
{
	name = ConvertNameToLocalPlatform( name );
	for ( int dllIndex = 0; dllIndex < g_iextdllMac; ++dllIndex )
	{
		const extensiondll_t& dll = g_rgextdll[dllIndex];
		for ( int functionIndex = 0; functionIndex < dll.functionCount; ++functionIndex )
		{
			const functiontable_t& function = dll.functionTable[functionIndex];
			if ( Q_strcmp( name, function.pFunctionName ) == 0 )
			{
				if ( function.pFunction )
					return function.pFunction;
				break;
			}
		}
	}
	Con_Printf( "Can't find proc: %s\n", name );
	return 0;
}

const char* NameForFunction( uint32 address )
{
#if !defined( _WIN32 )
	Dl_info info;
	for ( int index = 0; index < g_iextdllMac; ++index )
	{
		if ( dladdr( reinterpret_cast<void*>( static_cast<std::uintptr_t>( address ) ), &info ) != 0
			 && info.dli_sname != nullptr )
			return info.dli_sname;
	}
	Con_Printf( "Can't find address: %08lx\n", static_cast<unsigned long>( address ) );
	return nullptr;
#else
	for ( int dllIndex = 0; dllIndex < g_iextdllMac; ++dllIndex )
	{
		const extensiondll_t& dll = g_rgextdll[dllIndex];
		for ( int functionIndex = 0; functionIndex < dll.functionCount; ++functionIndex )
		{
			const functiontable_t& function = dll.functionTable[functionIndex];
			if ( function.pFunction == address )
			{
				if ( function.pFunctionName )
					return function.pFunctionName;
				break;
			}
		}
	}
	Con_Printf( "Can't find address: %08lx\n", static_cast<unsigned long>( address ) );
	return nullptr;
#endif
}

extern jmp_buf host_abortserver;
extern SDL_Window** pmainwindow;

[[noreturn]] void Sys_Error( const char* format, ... )
{
	constexpr int SYSTEM_ERROR_BUFFER_BYTES = 1024;
	constexpr int SYSTEM_ERROR_JUMP = 2;
	constexpr int SYSTEM_ERROR_EXIT = -1;
	static qboolean bReentry;
	char text[SYSTEM_ERROR_BUFFER_BYTES];
	va_list arguments;
	va_start( arguments, format );
#if defined( _WIN32 ) && defined( SWDS_DLL )
	_vsnprintf( text, sizeof( text ), format, arguments );
#else
	std::vsnprintf( text, sizeof( text ), format, arguments );
#endif
	va_end( arguments );
	if ( bReentry )
	{
		std::fprintf( stderr, "%s\n", text );
		longjmp( host_abortserver, SYSTEM_ERROR_JUMP );
	}
	bReentry = true;
	if ( svs.dll_initialized && gEntityInterface.pfnSys_Error )
		gEntityInterface.pfnSys_Error( text );
	Log_Printf( "FATAL ERROR (shutting down): %s\n", text );
	if ( g_bIsDedicatedServer )
	{
		if ( Launcher_ConsolePrintf )
			Launcher_ConsolePrintf( const_cast<char*>( "FATAL ERROR (shutting down): %s\n" ), text );
		else
			std::printf( "FATAL ERROR (shutting down): %s\n", text );
#if defined( _WIN32 ) && defined( SWDS_DLL )
		_exit( SYSTEM_ERROR_EXIT );
#else
		std::exit( SYSTEM_ERROR_EXIT );
#endif
	}
	SDL_Window* window = pmainwindow ? *pmainwindow : nullptr;
	Sys_Printf( text );
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "Fatal Error", text, window );
	VideoMode_IsWindowed();
#if defined( _WIN32 ) && defined( SWDS_DLL )
	_exit( SYSTEM_ERROR_EXIT );
#else
	std::exit( SYSTEM_ERROR_EXIT );
#endif
}

struct saverestore_s;
using FIELDIOFUNCTION = void ( * )( saverestore_s*, const char*, void*, TYPEDESCRIPTION*, int );

extern cvar_t gl_vsync;
const char* GetRateRegistrySetting( const char* pchDef );

static char* SkipCharacterClassTail( char* scan, bool negated )
{
	char current = *scan;
	char* after = scan + 1;
	if ( current == '\0' )
		return nullptr;

	for ( ;; )
	{
		scan += 2;
		if ( current == '\\' )
		{
			current = *scan;
			after = scan + 1;
			if ( current == '\0' )
				return nullptr;
			continue;
		}
		if ( current == ']' )
		{
			scan = after;
			break;
		}
		scan = after;
		current = *scan;
		after = scan + 1;
		if ( current == '\0' )
			return nullptr;
	}

	if ( negated )
		return nullptr;
	return scan;
}

static char* MatchCharacterClass( char* pattern, char textChar )
{
	if ( textChar == '\0' )
		return nullptr;

	const bool negated = pattern[1] == '!' || pattern[1] == '^';
	char classChar;
	char* next;
	if ( negated )
	{
		classChar = pattern[2];
		next = pattern + 3;
	}
	else
	{
		classChar = pattern[1];
		next = pattern + 2;
	}

	for ( ;; )
	{
		if ( classChar == '\\' )
		{
			classChar = *next;
			++next;
		}
		else if ( classChar == '\0' )
		{
			return nullptr;
		}

		char following = *next;
		char* afterFollowing = next + 1;
		char endChar = classChar;
		if ( following == '-' )
		{
			endChar = next[1];
			if ( endChar == ']' )
			{
				if ( static_cast<signed char>( textChar ) != static_cast<signed char>( classChar ) )
				{
					classChar = following;
					next = afterFollowing;
					continue;
				}
				if ( negated )
					return nullptr;
				return next + 2;
			}

			char* endPointer;
			if ( endChar == '\\' )
			{
				endPointer = next + 3;
				endChar = next[2];
			}
			else
			{
				endPointer = next + 2;
			}
			if ( endChar == '\0' )
				return nullptr;
			afterFollowing = endPointer + 1;
			following = *endPointer;
		}

		const bool inRange = static_cast<signed char>( textChar ) >= static_cast<signed char>( classChar )
							 && static_cast<signed char>( endChar ) >= static_cast<signed char>( textChar );
		if ( inRange )
		{
			if ( following == ']' )
			{
				if ( negated )
					return nullptr;
				return afterFollowing;
			}
			if ( following == '\0' )
				return nullptr;
			return SkipCharacterClassTail( afterFollowing, negated );
		}

		if ( following == ']' )
		{
			if ( negated )
				return afterFollowing;
			return nullptr;
		}

		classChar = following;
		next = afterFollowing;
	}
}

int glob_match( char* pattern, char* text );

static int MatchAfterStar( char* pattern, char* text )
{
	char* remainder = pattern;
	char* lookahead = remainder + 1;
	char nextChar = *remainder;
	bool consumeQuestion = nextChar == '?';

	for ( ;; )
	{
		if ( !consumeQuestion && nextChar != '*' )
			break;
		if ( !consumeQuestion )
			remainder = lookahead;

		for ( ;; )
		{
			if ( !consumeQuestion )
			{
				lookahead = remainder + 1;
				nextChar = *remainder;
				if ( nextChar != '?' )
					break;
			}

			consumeQuestion = false;
			if ( *text == '\0' )
				return 0;
			++text;
			remainder = lookahead;
		}
	}

	if ( nextChar == '\0' )
		return 1;
	if ( nextChar == '[' )
	{
		char* cursor = text;
		for ( ;; )
		{
			if ( glob_match( remainder, cursor ) != 0 )
				return 1;
			if ( *cursor == '\0' )
				return 0;
			++cursor;
		}
	}
	if ( nextChar == '\\' )
		nextChar = remainder[1];

	char* cursor = text;
	for ( ;; )
	{
		char textChar = *cursor;
		if ( nextChar == textChar )
		{
			if ( glob_match( remainder, cursor ) != 0 )
				return 1;
			textChar = nextChar;
		}
		if ( textChar == '\0' )
			return 0;
		++cursor;
	}
}

void Sys_PageIn( void* ptr, int size )
{
	( void )ptr;
	( void )size;
}

int glob_match( char* pattern, char* text )
{
	if ( *pattern == '\0' )
	{
		if ( *text == '\0' )
			return 1;
		return 0;
	}

	for ( ;; )
	{
		const char current = *pattern;
		if ( current == '?' )
		{
			if ( *text == '\0' )
				return 0;
		}
		else if ( current == '*' )
		{
			return MatchAfterStar( pattern + 1, text );
		}
		else if ( current == '[' )
		{
			char* remainder = MatchCharacterClass( pattern, *text );
			if ( remainder == nullptr )
				return 0;
			pattern = remainder;
			++text;
			if ( *pattern == '\0' )
			{
				if ( *text == '\0' )
					return 1;
				return 0;
			}
			continue;
		}
		else if ( current == '\\' )
		{
			if ( pattern[1] != *text )
				return 0;
			pattern += 2;
			++text;
			if ( *pattern == '\0' )
			{
				if ( *text == '\0' )
					return 1;
				return 0;
			}
			continue;
		}
		else if ( current != *text )
		{
			return 0;
		}

		++pattern;
		++text;
		if ( *pattern == '\0' )
		{
			if ( *text == '\0' )
				return 1;
			return 0;
		}
	}
}

void Sys_MakeCodeWriteable( uint32 startaddr, uint32 length )
{
	( void )startaddr;
	( void )length;
}

void Sys_SetFPCW()
{
}

void Sys_PushFPCW_SetHigh()
{
}

void Sys_PopFPCW()
{
}

void MaskExceptions()
{
}

void Sys_Sleep( int msec )
{
#if defined( _WIN32 )
	Sleep( static_cast<DWORD>( msec ) );
#else
	usleep( static_cast<unsigned int>( 1000 * msec ) );
#endif
}

void Sys_DebugOutStraight( const char* pStr )
{
	std::fprintf( stderr, "%s\n", pStr );
}

void Sys_Warning( const char* format, ... )
{
	constexpr int WARNING_BUFFER_BYTES = 1024;
	char text[WARNING_BUFFER_BYTES];
	va_list arguments;
	va_start( arguments, format );
	_vsnprintf( text, sizeof( text ), format, arguments );
	va_end( arguments );
	Sys_Printf( text );
}

void Sys_Quit()
{
	constexpr int ENGINE_CLOSE = 3;
	giActive = ENGINE_CLOSE;
}

void Dispatch_Substate( int iSubState )
{
	giSubState = iSubState;
}

qboolean gfBackground;

void GameSetBackground( qboolean bNewSetting )
{
	gfBackground = bNewSetting;
}

void ForceReloadProfile()
{
	constexpr int RATE_SETTING_BYTES = 32;
	Cbuf_AddText( "exec config.cfg\n" );
	Cbuf_AddText( "+mlook\n" );
	Cbuf_Execute();
	if ( COM_CheckParm( "-nomousegrab" ) != 0 )
		Cvar_Set( "cl_mousegrab", "0" );

	Key_SetBinding( '~', const_cast<char*>( "toggleconsole" ) );
	Key_SetBinding( '`', const_cast<char*>( "toggleconsole" ) );
	Key_SetBinding( K_ESCAPE, const_cast<char*>( "cancelselect" ) );

	SDL_GL_SetSwapInterval( ( gl_vsync.value <= 0.0f ) - 1 );

	if ( cls.state != ca_dedicated )
	{
		char rateSetting[RATE_SETTING_BYTES];
		const char* registryRate = GetRateRegistrySetting( rate.string );
		std::strncpy( rateSetting, registryRate, RATE_SETTING_BYTES );
		Cvar_DirectSet( &rate, rateSetting );
	}

	const char* communityName = Steam_GetCommunityName();
	if ( communityName == nullptr )
		communityName = "unknown";
	const char* playerName = Cvar_VariableString( "name" );
	if ( playerName != nullptr
		 && ( Q_stricmp( playerName, "player" ) == 0 || Q_stricmp( playerName, "unnamed" ) == 0 ) )
	{
		Cvar_Set( "name", communityName );
	}
}

const char* FindAddressInTable( extensiondll_t* pDll, uint32 function )
{
#if defined( _WIN32 )
	for ( int index = 0; index < pDll->functionCount; ++index )
	{
		if ( pDll->functionTable[index].pFunction == function )
			return pDll->functionTable[index].pFunctionName;
	}
	return nullptr;
#else
	( void )pDll;
	Dl_info info;
	if ( dladdr( reinterpret_cast<void*>( static_cast<std::uintptr_t>( function ) ), &info ) == 0 )
		return nullptr;
	return info.dli_sname;
#endif
}

uint32 FindNameInTable( extensiondll_t* pDll, const char* pName )
{
#if defined( _WIN32 )
	FARPROC function = GetProcAddress( pDll->lDLLHandle, pName );
#else
	void* function = dlsym( reinterpret_cast<void*>( pDll->lDLLHandle ), pName );
#endif
	return static_cast<uint32>( reinterpret_cast<std::uintptr_t>( function ) );
}

FIELDIOFUNCTION GetIOFunction( char* pName )
{
	for ( int index = 0; index < g_iextdllMac; ++index )
	{
#if defined( _WIN32 )
		FIELDIOFUNCTION function =
			reinterpret_cast<FIELDIOFUNCTION>( GetProcAddress( g_rgextdll[index].lDLLHandle, pName ) );
#else
		FIELDIOFUNCTION function = reinterpret_cast<FIELDIOFUNCTION>(
			dlsym( reinterpret_cast<void*>( g_rgextdll[index].lDLLHandle ), pName ) );
#endif
		if ( function != nullptr )
			return function;
	}
	return nullptr;
}

void ReleaseEntityDlls()
{
	if ( svs.dll_initialized == 0 )
		return;

	const int loadedCount = g_iextdllMac;
	FreeAllEntPrivateData();
	if ( gNewDLLFunctions.pfnGameShutdown != nullptr )
		gNewDLLFunctions.pfnGameShutdown();

	extensiondll_t* end = &g_rgextdll[loadedCount];
	Cvar_UnlinkExternals();
	for ( extensiondll_t* dll = g_rgextdll; end > dll; ++dll )
	{
#if defined( _WIN32 )
		FreeLibrary( dll->lDLLHandle );
#else
		dlclose( reinterpret_cast<void*>( dll->lDLLHandle ) );
#endif
		functiontable_t* functionTable = dll->functionTable;
		dll->lDLLHandle = nullptr;
		if ( functionTable != nullptr )
			Mem_Free( functionTable );
		dll->functionTable = nullptr;
	}
	svs.dll_initialized = 0;
}

void IN_DeactivateMouse()
{
	ClientDLL_DeactivateMouse();
}

void IN_ActivateMouse()
{
	ClientDLL_ActivateMouse();
}

void IN_MouseEvent( int mstate )
{
	ClientDLL_MouseEvent( mstate );
}

void IN_ClearStates()
{
	ClientDLL_ClearStates();
}

void Sys_SplitPath( const char* path, char* drive, char* dir, char* fname, char* ext )
{
	constexpr std::size_t SPLIT_PATH_COMPONENT_LIMIT = 255;
	const char* start = path;
	char current = *path;
	if ( current != '\0' && path[1] == ':' )
	{
		if ( drive != nullptr )
		{
			std::strncpy( drive, path, 2 );
			drive[2] = '\0';
		}
		current = path[2];
		start = path + 2;
	}
	else if ( drive != nullptr )
	{
		*drive = '\0';
	}

	const char* name = nullptr;
	const char* dot = nullptr;
	const char* end = start;
	const char* cursor = start + 1;
	bool scanning = current != '\0';
	while ( scanning )
	{
		if ( current == '/' || current == '\\' )
		{
			current = *cursor;
			name = cursor;
			end = cursor;
			++cursor;
			if ( current == '\0' )
				scanning = false;
			continue;
		}

		const bool isDot = current == '.';
		current = *cursor;
		end = cursor;
		if ( isDot )
			dot = cursor - 1;
		++cursor;
		if ( current == '\0' )
			scanning = false;
	}

	const bool hasExtension = name == nullptr ? dot != nullptr && dot >= start : dot != nullptr && name <= dot;
	if ( name == nullptr )
	{
		name = start;
		if ( dir != nullptr )
			*dir = '\0';
	}
	else if ( dir != nullptr )
	{
		std::size_t length = static_cast<std::size_t>( name - start );
		if ( length > SPLIT_PATH_COMPONENT_LIMIT )
			length = SPLIT_PATH_COMPONENT_LIMIT;
		std::strncpy( dir, start, length );
		dir[length] = '\0';
	}

	if ( hasExtension )
	{
		if ( fname != nullptr )
		{
			std::size_t length = static_cast<std::size_t>( dot - name );
			if ( length > SPLIT_PATH_COMPONENT_LIMIT )
				length = SPLIT_PATH_COMPONENT_LIMIT;
			std::strncpy( fname, name, length );
			fname[length] = '\0';
		}
		if ( ext != nullptr )
		{
			std::size_t length = static_cast<std::size_t>( end - dot );
			if ( length > SPLIT_PATH_COMPONENT_LIMIT )
				length = SPLIT_PATH_COMPONENT_LIMIT;
			std::strncpy( ext, dot, length );
			ext[length] = '\0';
		}
		return;
	}

	if ( fname != nullptr )
	{
		std::size_t length = static_cast<std::size_t>( end - name );
		if ( length > SPLIT_PATH_COMPONENT_LIMIT )
			length = SPLIT_PATH_COMPONENT_LIMIT;
		std::strncpy( fname, name, length );
		fname[length] = '\0';
	}
	if ( ext != nullptr )
		*ext = '\0';
}
