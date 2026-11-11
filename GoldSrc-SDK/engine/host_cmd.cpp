#include "host.h"
#include "../common/dll_state.h"
#include "gl_model.h"
#include "host_cmd.h"
#include "world.h"
#include "registry.h"
#include "pr_dlls.h"
#include "pm_defs.h"
#include "engine_studio.h"
#include "keys.h"
#include "vgui_intwrap2.h"
#include "mem.h"
#include "buildnum.h"
#include "common.h"
#include "cmd.h"
#include "conprint.h"
#include "protocol.h"
#include "render.h"
#include "steam_api.h"
#include "voice.h"
#include "cvar.h"
#include "server.h"
#include "client.h"
#include "sys.h"
#include "zone.h"
#include "info.h"
#include "interface.h"
#include "server_static.h"
#include "sound.h"
#include "cl_demo.h"
#include "engine_edict.h"
#include "mathlib.h"
#include "filesystem.h"
#include "eiface.h"

#include <Windows.h>
#include <intrin.h>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <climits>
#include <io.h>

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

#define SAVEGAME_HEADER ( 'J' | ( 'S' << 8 ) | ( 'A' << 16 ) | ( 'V' << 24 ) )
#define SAVEFILE_HEADER ( 'V' | ( 'A' << 8 ) | ( 'L' << 16 ) | ( 'V' << 24 ) )
#define SAVEGAME_VERSION 0x0071

#define STATE_ENDTRAINING 1
#define STATE_ENDDEMO 3

#define FILE_COPY_SIZE 1024

typedef void( __cdecl* SV_SAVEGAMECOMMENT_FUNC )( char* comment, int length );
typedef int( __cdecl* SERVER_GETBLENDINGINTERFACE_FUNC )(
	int version, sv_blending_interface_t** blendingAPI, server_studio_api_t* studioAPI, float* rotation, float* bones );

int gHostSpawnCount;
int current_skill;

double cpuPercent;
int startTime;
int g_iQuitCommandIssued;
char* g_pPostRestartCmdLineArgs;

CareerStateType g_careerState;
qboolean g_bMajorMapChange;
qboolean noclip_anglehack;

cvar_t gHostMap = { "HostMap", "C1A0" };
cvar_t voice_recordtofile = { "voice_recordtofile", "0" };
cvar_t voice_inputfromfile = { "voice_inputfromfile", "0" };

void Host_SavegameComment( char* buffer, int bufferSize );
SV_SAVEGAMECOMMENT_FUNC g_pSaveGameCommentFunc = Host_SavegameComment;

extern int g_userid;
extern vec3_t r_origin;
extern qboolean noip;
extern cvar_t pausable;
extern cvar_t motdfile;
extern cvar_t skill;
extern cvar_t sv_skyname;
extern cvar_t sv_skycolor_r;
extern cvar_t sv_skycolor_g;
extern cvar_t sv_skycolor_b;
extern cvar_t sv_skyvec_x;
extern cvar_t sv_skyvec_y;
extern cvar_t sv_skyvec_z;
extern cvar_t sv_newunit;
extern qboolean scr_skipupdate;
extern playermove_t g_svmove;
extern sv_blending_interface_t* g_pSvBlendingAPI;
extern server_studio_api_t server_studio_api;
extern float rotationmatrix[3][4];
extern float bonetransform[MAXSTUDIOBONES][3][4];

void Sys_Quit();
void SV_ClientPrintf( const char* format, ... );
void SV_CountPlayers( int* clients );
void COM_Log( const char* filename, const char* format, ... );
qboolean Steam_GSBSecure();
qboolean Steam_GSBSecurePreference();
qboolean Steam_GSBLoggedOn();
const char* Steam_GetGSUniverse();
void Steam_ShowOnScreenKeyboard( int fieldX, int fieldY );
void SCR_BeginLoadingPlaque( qboolean reconnect );
void SCR_UpdateScreen();
void Voice_Deinit();
void NotifyDedicatedServerUI( const char* message );
extern "C" void VideoMode_SwitchMode( int hardware, int windowed );
extern "C" void VideoMode_SetVideoMode( int width, int height, int bitsPerPixel );
int DecalListCreate( DECALLIST* decals );
edict_t* CreateNamedEntity( int classname );
int PF_IsMapValid_I( char* mapname );
int PF_IsDedicatedServer();
void PF_MessageBegin_I( int destination, int type, const float* origin, edict_t* entity );
void PF_WriteByte_I( int value );
void PF_WriteString_I( const char* text );
void PF_MessageEnd_I();

void SV_InactivateClients();
void SV_ServerShutdown();
int SV_SpawnServer( qboolean bIsDemo, char* server, char* startspot );
void SV_LoadEntities();
void SV_ActivateServer( int runPhysics );
void SV_InitEncoders();
void R_ResetSvBlending();
void SV_GetPlayerHulls();
void SV_CheckBlendingInterface();
void SV_CheckSaveGameCommentInterface();

char* Host_SaveGameDirectory();
void Host_ClearSaveDirectory();
void Host_ClearGameState();
void Host_InitializeGameDLL();
void Host_Map( qboolean bIsDemo, char* mapstring, char* mapName, qboolean loadGame );
int __cdecl Host_Load( const char* name );
int Host_ValidSave();
void FileCopy( FileHandle_t output, FileHandle_t input, int fileSize );
int FileSize( FileHandle_t file );
void SaveExit( SAVERESTOREDATA* save );
int SaveReadHeader( FileHandle_t file, GAME_HEADER* header, int readGlobalState );
int SaveReadHeader_0( FileHandle_t file, GAME_HEADER* header, int readGlobalState );
void LandmarkOrigin( SAVERESTOREDATA* saveData, vec_t* output, const char* landmarkName );

void CD_Command_f();
void MP3_Command_f();
void CareerAudio_Command_f();
void Master_SetMaster_f();
void Master_Heartbeat_f();

void Host_KillServer_f();
void Host_Soundfade_f();
void Host_Status_Formatted_f();
void Host_SetRenderer_f();
void Host_SetVideoMode_f();
void Host_SetGameDir_f();
void Host_Map_f();
void Host_Career_f();
void Host_Restart_f();
void Host_Reload_f();
void Host_Changelevel_f();
void Host_Changelevel2_f();
void Host_Reconnect_f();
void Host_Say_f();
void Host_Say_Team_f();
void Host_Tell_f();
void Host_Kill_f();
void Host_TogglePause_f();
void Host_Pause_f();
void Host_Unpause_f();
void Host_Kick_f();
void Host_Motd_f();
void Host_Motd_Write_f();
void Host_InputSet_Menu_f();
void Host_InputSet_Game_f();
void Host_SetRichPresenceOverride_f();
void Host_SetRichPresenceNiceMap_f();
void Host_SetRichPresenceUpdate_f();
void Host_SetRichPresenceGameMode_f();
void Host_ShowOSK_f();
void Host_Loadgame_f();
void Host_Savegame_f();
void Host_AutoSave_f();
void Host_VoiceRecordStart_f();
void Host_VoiceRecordStop_f();
void Host_Startdemos_f();
void Host_Demos_f();
void Host_Stopdemo_f();
void Host_SetInfo_f();
void Host_FullInfo_f();
void Host_God_f();
void Host_Notarget_f();
void Host_Fly_f();
void Host_Noclip_f();
void Host_Viewmodel_f();
void Host_Viewframe_f();
void Host_Viewnext_f();
void Host_Viewprev_f();

void Host_Status_Printf( qboolean conprint, qboolean log, const char* fmt, ... )
{
	va_list argptr;
	char string[4096];

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	if ( conprint )
		Con_Printf( "%s", string );
	else
		SV_ClientPrintf( "%s", string );

	if ( log )
		COM_Log( "status.log", "%s", string );
}

/*
==================
Host_Status_f
==================
*/
void Host_Status_f()
{
	qboolean conprint = TRUE;
	qboolean log = FALSE;
	int hltv_slots = 0;
	int hltv_specs = 0;
	int hltv_delay = 0;
	const char* pchConnectionString = com_empty_string;
	const char* pchSteamUniverse = com_empty_string;

	if ( cmd_source == src_command )
	{
		if ( !sv.active )
		{
			Cmd_ForwardToServer();
			return;
		}
	}
	else
	{
		conprint = FALSE;
	}

	if ( Cmd_Argc() == 2 && !Q_strcasecmp( Cmd_Argv( 1 ), "log" ) )
	{
		_unlink( "status.log" );
		log = TRUE;
	}

	Host_Status_Printf( conprint, log, "hostname:  %s\n", Cvar_VariableString( "hostname" ) );

	qboolean bIsSecure = Steam_GSBSecure();
	qboolean bWantsToBeSecure = Steam_GSBSecurePreference();
	qboolean bConnectedToSteam3 = Steam_GSBLoggedOn();
	if ( !bIsSecure && bWantsToBeSecure )
	{
		pchConnectionString = bConnectedToSteam3 ? "(secure mode enabled, connected to Steam3)"
												 : "(secure mode enabled, disconnected from Steam3)";
	}
	if ( sv.active )
		pchSteamUniverse = Steam_GetGSUniverse();

	int appID = GetGameAppID();
	Host_Status_Printf( conprint, log, "version :  %i/%s %d %s %s%s (%d)\n", PROTOCOL_VERSION, gpszVersionString,
		build_number(), bIsSecure ? "secure" : "insecure", pchConnectionString, pchSteamUniverse, appID );

	if ( !noip )
		Host_Status_Printf( conprint, log, "tcp/ip  :  %s\n", NET_AdrToString( net_local_adr ) );

	Host_Status_Printf( conprint, log, "map     :  %s at: %d x, %d y, %d z\n", sv.name, (int)r_origin[0],
		(int)r_origin[1], (int)r_origin[2] );

	int nClients;
	SV_CountPlayers( &nClients );
	Host_Status_Printf( conprint, log, "players :  %i active (%i max)\n\n", nClients, svs.maxclients );
	Host_Status_Printf( conprint, log, "#      name userid uniqueid frag time ping loss adr\n" );

	client_t* client = svs.clients;
	for ( int j = 0; j < svs.maxclients; j++, client++ )
	{
		if ( !client->active )
			continue;

		int seconds = (int)( realtime - client->netchan.connect_time );
		int minutes = seconds / 60;
		int hours = 0;
		if ( minutes )
		{
			seconds -= minutes * 60;
			hours = minutes / 60;
			if ( hours )
				minutes -= hours * 60;
		}

		const char* uniqueID = client->fakeclient ? "BOT" : SV_GetClientIDString( client );
		Host_Status_Printf(
			conprint, log, "#%2i %8s %i %s", j + 1, va( "\"%s\"", client->name ), client->userid, uniqueID );

		if ( client->proxy )
		{
			const char* val = Info_ValueForKey( client->userinfo, "hspecs" );
			if ( Q_strlen( val ) )
				hltv_specs = Q_atoi( val );
			val = Info_ValueForKey( client->userinfo, "hslots" );
			if ( Q_strlen( val ) )
				hltv_slots = Q_atoi( val );
			val = Info_ValueForKey( client->userinfo, "hdelay" );
			if ( Q_strlen( val ) )
				hltv_delay = Q_atoi( val );

			Host_Status_Printf( conprint, log, " hltv:%u/%u delay:%u", hltv_specs, hltv_slots, hltv_delay );
		}
		else
		{
			Host_Status_Printf( conprint, log, " %3i", (int)client->edict->v.frags );
		}

		if ( hours )
			Host_Status_Printf( conprint, log, " %2i:%02i:%02i", hours, minutes, seconds );
		else
			Host_Status_Printf( conprint, log, " %02i:%02i", minutes, seconds );

		if ( !client->proxy )
		{
			int packet_loss = (int)client->packet_loss;
			int ping = SV_CalcPing( client );
			Host_Status_Printf( conprint, log, " %4i  %3i", ping, packet_loss );
		}

		if ( ( conprint || client->proxy ) && client->netchan.remote_address.type == NA_IP )
			Host_Status_Printf( conprint, log, " %s\n", NET_AdrToString( client->netchan.remote_address ) );
		else
			Host_Status_Printf( conprint, log, "\n" );
	}

	Host_Status_Printf( conprint, log, "%i users\n", nClients );
}

/*
==================
Host_Ping_f

==================
*/
void Host_Ping_f()
{
	int i;
	client_t* client;

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	SV_ClientPrintf( "Client ping times:\n" );
	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( client->active )
			SV_ClientPrintf( "%4i %s\n", SV_CalcPing( client ), client->name );
	}
}

void GetStatsString( char* buf, int bufSize )
{
	int i;
	float avgIn = 0.0f;
	float avgOut = 0.0f;
	int players = 0;

	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( ( host_client->active || host_client->connected || host_client->spawned ) && !host_client->fakeclient )
		{
			avgIn += host_client->netchan.flow[1].avgkbytespersec;
			avgOut += host_client->netchan.flow[0].avgkbytespersec;
			players++;
		}
	}

	double fps = 1.0 / host_frametime;
	int users = g_userid - 1;
	int minutes = (int)( Sys_FloatTime() - startTime ) / 60;

	_snprintf( buf, bufSize - 1, "%5.2f %5.2f %5.2f %7i %5i %7.2f %7i", cpuPercent * 100, avgIn, avgOut, minutes,
		users, fps, players );
	buf[bufSize - 1] = '\0';
}

void Host_Stats_f()
{
	char stats[512];

	GetStatsString( stats, sizeof( stats ) );
	Con_Printf( "CPU   In    Out   Uptime  Users   FPS    Players\n%s\n", stats );
}

/*
==================
Host_Quit_f
==================
*/
void Host_Quit_f()
{
	if ( Cmd_Argc() == 1 )
	{
		giActive = DLL_CLOSE;
		g_iQuitCommandIssued = 1;

		if ( cls.state != ca_dedicated )
			CL_Disconnect();
		Host_ShutdownServer( FALSE );
		NET_Config( FALSE );

		giActive = DLL_CLOSE;
		Sys_Quit();
	}
	else
	{
		giActive = DLL_PAUSED;
		giStateInfo = DLL_QUIT;
	}
}

void Host_Quit_Restart_f()
{
	giActive = 5;
	giStateInfo = DLL_QUIT;

	if ( !sv.active && cls.state == ca_active && cls.trueaddress[0] && g_pPostRestartCmdLineArgs )
	{
		strcat( g_pPostRestartCmdLineArgs, " +connect " );
		strcat( g_pPostRestartCmdLineArgs, cls.servername );
	}

	if ( sv.active && svs.maxclients == 1 && cls.state == ca_active && g_pPostRestartCmdLineArgs )
	{
		Cbuf_AddText( "save quick\n" );
		Cbuf_Execute();
		strcat( g_pPostRestartCmdLineArgs, " +load quick" );
	}

	if ( Host_IsServerActive() )
		Host_ShutdownServer( FALSE );
	else if ( cls.state != ca_dedicated )
		CL_Disconnect();

	NET_Config( FALSE );
	Cache_Force_Flush();
}

static uint64_t Host_FileTimeTicks( const FILETIME& time )
{
	ULARGE_INTEGER ticks;

	ticks.LowPart = time.dwLowDateTime;
	ticks.HighPart = time.dwHighDateTime;
	return ticks.QuadPart;
}

#ifndef _WIN32
static bool Host_ReadProcCpuTicks( uint64_t& cpuTicks, uint64_t& runTicks )
{
	char path[64];
	char line[4096];
	char* p;
	char* token;
	char* end;
	int field;
	unsigned long long value;
	unsigned long long utime = 0;
	unsigned long long stime = 0;
	unsigned long long starttime = 0;
	long uptime = 0;
	int matched;
	FILE* f;

	snprintf( path, sizeof( path ) - 1, "/proc/%i/stat", (int)GetCurrentProcessId() );
	path[sizeof( path ) - 1] = '\0';

	f = fopen( path, "r" );
	if ( !f )
		return false;
	p = fgets( line, sizeof( line ), f );
	fclose( f );
	if ( !p )
		return false;

	p = strrchr( line, ')' );
	if ( !p )
		return false;
	p++;

	for ( field = 0; *p && field <= 19; field++ )
	{
		while ( *p == ' ' || *p == '\t' )
			p++;
		if ( *p == '\0' || *p == '\n' )
			break;

		token = p;
		while ( *p && *p != ' ' && *p != '\t' && *p != '\n' )
			p++;

		if ( field == 11 || field == 12 || field == 19 )
		{
			end = NULL;
			value = strtoull( token, &end, 10 );
			if ( end == token )
				return false;

			if ( field == 11 )
				utime = value;
			else if ( field == 12 )
				stime = value;
			else
				starttime = value;
		}
	}

	if ( field <= 19 )
		return false;

	f = fopen( "/proc/uptime", "r" );
	if ( !f )
		return false;
	matched = fscanf( f, "%ld", &uptime );
	fclose( f );
	if ( matched != 1 )
		return false;

	cpuTicks = utime + stime;
	runTicks = (uint64_t)( 100 * (int64_t)uptime - (int64_t)starttime );
	return true;
}

static bool Host_ReadProcessCpuTicks( uint64_t& cpuTicks, uint64_t& runTicks )
{
	FILETIME creationTime;
	FILETIME exitTime;
	FILETIME kernelTime;
	FILETIME userTime;
	FILETIME systemTime;

	if ( !GetProcessTimes( GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime ) )
		return false;
	GetSystemTimeAsFileTime( &systemTime );

	uint64_t now = Host_FileTimeTicks( systemTime );
	uint64_t started = Host_FileTimeTicks( creationTime );
	if ( now < started )
		return false;

	cpuTicks = Host_FileTimeTicks( userTime ) + Host_FileTimeTicks( kernelTime );
	runTicks = now - started;
	return true;
}

#endif

void Host_UpdateStats()
{
	static float last;
	static float lastAvg;
	static uint64_t lastcputicks;
	static uint64_t lastrunticks;
	uint64_t cputicks;
	uint64_t runticks;

	if ( !startTime )
		startTime = (int)Sys_FloatTime();

#ifdef _WIN32
	if ( Sys_FloatTime() > last + 1.0f )
#else
	if ( Sys_FloatTime() > last + 1.0 )
#endif
	{
#ifdef _WIN32
		FILETIME creationTime;
		FILETIME exitTime;
		FILETIME kernelTime;
		FILETIME userTime;
		FILETIME systemTime;

		GetProcessTimes( GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime );
		GetSystemTimeAsFileTime( &systemTime );
		if ( !lastrunticks )
			lastrunticks = Host_FileTimeTicks( creationTime );

		cputicks = Host_FileTimeTicks( userTime ) + Host_FileTimeTicks( kernelTime );
		runticks = Host_FileTimeTicks( systemTime );
		cpuPercent = (double)(int64_t)( cputicks - lastcputicks ) / (double)(int64_t)( runticks - lastrunticks );

		if ( Sys_FloatTime() > lastAvg + 5.0f )
		{
			lastcputicks = cputicks;
			lastrunticks = runticks;
			lastAvg = last;
		}
#else
		if ( Host_ReadProcCpuTicks( cputicks, runticks ) || Host_ReadProcessCpuTicks( cputicks, runticks ) )
		{
			if ( !lastcputicks )
				lastcputicks = cputicks;

			if ( lastrunticks )
			{
				cpuPercent =
					(double)(int64_t)( cputicks - lastcputicks ) / (double)(int64_t)( runticks - lastrunticks );
			}
			else
			{
				lastrunticks = runticks;
			}

			if ( Sys_FloatTime() > lastAvg + 5.0 )
			{
				lastcputicks = cputicks;
				lastrunticks = runticks;
				lastAvg = (float)Sys_FloatTime();
			}

			if ( cpuPercent <= 0.999 )
			{
				if ( cpuPercent < 0.0 )
					cpuPercent = 0.0;
			}
			else
			{
				cpuPercent = 0.999;
			}
		}

#endif
		last = (float)Sys_FloatTime();
	}
}

int Host_GetStartTime()
{
	return startTime;
}

void Host_Maps_f()
{
	const char* pszSubString;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "Usage:  maps <substring>\nmaps * for full listing\n" );
		return;
	}

	pszSubString = Cmd_Argv( 1 );
	if ( !pszSubString || !pszSubString[0] )
		return;

	if ( pszSubString[0] == '*' )
		pszSubString = NULL;

	COM_ListMaps( (char*)pszSubString );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Host_Version_f
//-----------------------------------------------------------------------------
void Host_Version_f()
{
	Con_Printf( "Protocol version %i\nExe version %s (%s)\n", PROTOCOL_VERSION, gpszVersionString, gpszProductString );
	Con_Printf( "Exe build: " ENGINE_BUILD_TIME " Oct  7 2024 (%i)\n", build_number() );
}

void Host_Interp_f()
{
	r_dointerp = !r_dointerp;
	Con_Printf( r_dointerp ? "Frame Interpolation ON\n" : "Frame Interpolation OFF\n" );
}

/*
==================
Host_InitCommands
==================
*/
void Host_InitCommands()
{
	if ( !g_bIsDedicatedServer )
	{
		Cmd_AddCommandWithFlags( "cd", CD_Command_f, CMD_FLAG_FILTERED );
		Cmd_AddCommand( "mp3", MP3_Command_f );
		Cmd_AddCommand( "_careeraudio", CareerAudio_Command_f );
	}
	Cmd_AddCommand( "shutdownserver", Host_KillServer_f );
	Cmd_AddCommand( "soundfade", Host_Soundfade_f );
	Cmd_AddCommand( "status", Host_Status_f );
	Cmd_AddCommand( "stat", Host_Status_Formatted_f );
	Cmd_AddCommandWithFlags( "quit", Host_Quit_f, CMD_FLAG_PRIVILEGED );
	if ( !g_bIsDedicatedServer )
	{
		Cmd_AddCommandWithFlags( "_restart", Host_Quit_Restart_f, CMD_FLAG_PRIVILEGED );
		Cmd_AddCommandWithFlags( "_setrenderer", Host_SetRenderer_f, CMD_FLAG_PRIVILEGED );
		Cmd_AddCommandWithFlags( "_setvideomode", Host_SetVideoMode_f, CMD_FLAG_PRIVILEGED );
		Cmd_AddCommandWithFlags( "_setgamedir", Host_SetGameDir_f, CMD_FLAG_PRIVILEGED );
		Cmd_AddCommandWithFlags( "_sethdmodels", Host_SetHDModels_f, CMD_FLAG_PRIVILEGED );
		Cmd_AddCommandWithFlags( "_setaddons_folder", Host_SetAddonsFolder_f, CMD_FLAG_PRIVILEGED );
	}
	Cmd_AddCommand( "exit", Host_Quit_f );
	Cmd_AddCommand( "map", Host_Map_f );
	Cmd_AddCommand( "career", Host_Career_f );
	Cmd_AddCommand( "maps", Host_Maps_f );
	Cmd_AddCommand( "restart", Host_Restart_f );
	Cmd_AddCommand( "reload", Host_Reload_f );
	Cmd_AddCommand( "changelevel", Host_Changelevel_f );
	Cmd_AddCommand( "changelevel2", Host_Changelevel2_f );
	Cmd_AddCommand( "reconnect", Host_Reconnect_f );
	Cmd_AddCommand( "version", Host_Version_f );
	Cmd_AddCommandWithFlags( "say", Host_Say_f, CMD_FLAG_FILTERED );
	Cmd_AddCommand( "say_team", Host_Say_Team_f );
	Cmd_AddCommand( "tell", Host_Tell_f );
	Cmd_AddCommandWithFlags( "kill", Host_Kill_f, CMD_FLAG_PRIVILEGED );
	Cmd_AddCommand( "pause", Host_TogglePause_f );
	Cmd_AddCommand( "setpause", Host_Pause_f );
	Cmd_AddCommand( "unpause", Host_Unpause_f );
	Cmd_AddCommand( "kick", Host_Kick_f );
	Cmd_AddCommand( "ping", Host_Ping_f );
	Cmd_AddCommandWithFlags( "motd", Host_Motd_f, CMD_FLAG_PRIVILEGED );
	Cmd_AddCommandWithFlags( "motd_write", Host_Motd_Write_f, CMD_FLAG_PRIVILEGED );
	Cmd_AddCommand( "stats", Host_Stats_f );
	Cmd_AddCommand( "inputset_menu", Host_InputSet_Menu_f );
	Cmd_AddCommand( "inputset_game", Host_InputSet_Game_f );
	Cmd_AddCommand( "richpresence_override", Host_SetRichPresenceOverride_f );
	Cmd_AddCommand( "richpresence_nicemap", Host_SetRichPresenceNiceMap_f );
	Cmd_AddCommand( "richpresence_update", Host_SetRichPresenceUpdate_f );
	Cmd_AddCommand( "richpresence_gamemode", Host_SetRichPresenceGameMode_f );
	Cmd_AddCommand( "show_osk", Host_ShowOSK_f );
	Cmd_AddCommand( "load", Host_Loadgame_f );
	Cmd_AddCommand( "save", Host_Savegame_f );
	Cmd_AddCommand( "autosave", Host_AutoSave_f );
	Cmd_AddCommandWithFlags( "writecfg", Host_WriteCustomConfig, CMD_FLAG_PRIVILEGED );
	if ( !g_bIsDedicatedServer )
	{
		Cmd_AddCommand( "+voicerecord", Host_VoiceRecordStart_f );
		Cmd_AddCommand( "-voicerecord", Host_VoiceRecordStop_f );
	}
	Cmd_AddCommand( "startdemos", Host_Startdemos_f );
	Cmd_AddCommand( "demos", Host_Demos_f );
	Cmd_AddCommand( "stopdemo", Host_Stopdemo_f );
	Cmd_AddCommandWithFlags( "setinfo", Host_SetInfo_f, CMD_FLAG_FILTERED );
	Cmd_AddCommand( "fullinfo", Host_FullInfo_f );
	Cmd_AddCommand( "god", Host_God_f );
	Cmd_AddCommand( "notarget", Host_Notarget_f );
	Cmd_AddCommand( "fly", Host_Fly_f );
	Cmd_AddCommand( "noclip", Host_Noclip_f );
	if ( !g_bIsDedicatedServer )
	{
		Cmd_AddCommand( "viewmodel", Host_Viewmodel_f );
		Cmd_AddCommand( "viewframe", Host_Viewframe_f );
		Cmd_AddCommand( "viewnext", Host_Viewnext_f );
		Cmd_AddCommand( "viewprev", Host_Viewprev_f );
	}
	Cmd_AddCommand( "mcache", Mod_Print );
	Cmd_AddCommand( "interp", Host_Interp_f );
	Cmd_AddCommand( "setmaster", Master_SetMaster_f );
	Cmd_AddCommand( "heartbeat", Master_Heartbeat_f );

	Cvar_RegisterVariable( &gHostMap );
	Cvar_RegisterVariable( &voice_recordtofile );
	Cvar_RegisterVariable( &voice_inputfromfile );
}

void Host_InputSet_Menu_f()
{
	Steam_ActivateInputSet( true );
}

void Host_InputSet_Game_f()
{
	Steam_ActivateInputSet( false );
}

void Host_SetRichPresenceOverride_f()
{
	Steam_SetRichPresenceOverride( Cmd_Argc() > 0 ? Cmd_Argv( 1 ) : NULL );
}

void Host_SetRichPresenceNiceMap_f()
{
	Steam_SetRichPresenceInfo( Cmd_Argc() > 0 ? Cmd_Argv( 1 ) : NULL );
}

void Host_SetRichPresenceGameMode_f()
{
	Steam_SetRichPresenceGameMode( Cmd_Argc() > 0 ? Cmd_Argv( 1 ) : NULL );
}

void Host_SetRichPresenceUpdate_f()
{
	Steam_UpdateRichPresence();
}

void Host_VoiceRecordStart_f()
{
	const char* pUncompressedFile = NULL;
	const char* pDecompressedFile = NULL;
	const char* pInputFile = NULL;

	if ( cls.state != ca_active )
		return;

	if ( (int)voice_inputfromfile.value )
		pInputFile = "voice_input.wav";

	if ( (int)voice_recordtofile.value )
	{
		pUncompressedFile = "voice_micdata.wav";
		pDecompressedFile = "voice_decompressed.wav";
	}

	Voice_RecordStart( pUncompressedFile, pDecompressedFile, pInputFile );
}

void Host_VoiceRecordStop_f()
{
	if ( cls.state == ca_active && Voice_IsRecording() )
	{
		CL_AddVoiceToDatagram( TRUE );
		Voice_RecordStop();
	}
}

void Host_Career_f()
{
	if ( cmd_source == src_command )
	{
		g_careerState = CAREER_LOADING;
		Host_Map_f();
	}
}

void Host_ClearSaveDirectory()
{
	char szName[MAX_PATH];
	const char* pfn;

	_snprintf( szName, sizeof( szName ), "%s", Host_SaveGameDirectory() );
	strncat( szName, "*.HL?", sizeof( szName ) - 1 - strlen( szName ) );
	COM_FixSlashes( szName );

	if ( Sys_FindFirstPathID( szName, "GAMECONFIG" ) )
	{
		Sys_FindClose();

		_snprintf( szName, sizeof( szName ), "%s", Host_SaveGameDirectory() );
		COM_FixSlashes( szName );
		FS_CreateDirHierarchy( szName, "GAMECONFIG" );
		strncat( szName, "*.HL?", sizeof( szName ) - 1 - strlen( szName ) );

		for ( pfn = Sys_FindFirstPathID( szName, "GAMECONFIG" ); pfn; pfn = Sys_FindNext( NULL ) )
		{
			_snprintf( szName, sizeof( szName ), "%s%s", Host_SaveGameDirectory(), pfn );
			FS_RemoveFile( szName, "GAMECONFIG" );
		}
	}

	Sys_FindClose();
}

/*
==================
Host_Restart_f

Restarts the current server for a dead player
==================
*/
void Host_Restart_f()
{
	char name[MAX_PATH];

	if ( cls.demoplayback || !sv.active || cmd_source != src_command )
		return;

	if ( cls.state != ca_dedicated )
		cls.state = ca_disconnected;

	Host_ClearGameState();
	SV_InactivateClients();

	Q_strncpy( name, sv.name, sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = '\0';

	SV_ServerShutdown();
	SV_SpawnServer( FALSE, name, NULL );
	SV_LoadEntities();
	SV_ActivateServer( 1 );
}

void Host_KillServer_f()
{
	if ( cls.state != ca_dedicated )
	{
		CL_Disconnect_f();
		return;
	}

	if ( sv.active )
	{
		Host_ShutdownServer( FALSE );
		if ( cls.state != ca_dedicated )
			NET_Config( FALSE );
	}
}

/*
==================
Host_Soundfade_f

==================
*/
void Host_Soundfade_f()
{
	int percent;
	int inTime = 0;
	int holdTime;
	int outTime = 0;

	if ( Cmd_Argc() != 3 && Cmd_Argc() != 5 )
	{
		Con_Printf( "soundfade <percent> <hold> [<out> <int>]\n" );
		return;
	}

	percent = Q_atoi( Cmd_Argv( 1 ) );
	if ( percent > 100 )
		percent = 100;
	if ( percent < 0 )
		percent = 0;

	holdTime = Q_atoi( Cmd_Argv( 2 ) );
	if ( holdTime > 255 )
		holdTime = 255;

	if ( Cmd_Argc() == 5 )
	{
		outTime = Q_atoi( Cmd_Argv( 3 ) );
		if ( outTime > 255 )
			outTime = 255;

		inTime = Q_atoi( Cmd_Argv( 4 ) );
		if ( inTime > 255 )
			inTime = 255;
	}

	cls.soundfade.nStartPercent = percent;
	cls.soundfade.soundFadeStartTime = realtime;
	cls.soundfade.soundFadeOutTime = outTime;
	cls.soundfade.soundFadeHoldTime = holdTime;
	cls.soundfade.soundFadeInTime = inTime;
}

void Host_Status_Formatted_f()
{
	const char* fmt = "%-2.2s\t%-9.9s\t%-7.7s\t%-20.20s\t%-4.4s\t%-8.8s\t%-4.4s\t%-4.4s\t%-21.21s\n";
	qboolean conprint = TRUE;
	qboolean log = FALSE;

	if ( cmd_source == src_command )
	{
		if ( !sv.active )
		{
			Cmd_ForwardToServer();
			return;
		}
	}
	else
	{
		conprint = FALSE;
	}

	if ( Cmd_Argc() == 2 && !Q_strcasecmp( Cmd_Argv( 1 ), "log" ) )
	{
		_unlink( "status.log" );
		log = TRUE;
	}

	qboolean bIsSecure = Steam_GSBSecure();
	Host_Status_Printf( conprint, log, "hostname:  %s\n", Cvar_VariableString( "hostname" ) );
	Host_Status_Printf( conprint, log, "version :  %i/%s %d %s\n", PROTOCOL_VERSION, gpszVersionString, build_number(),
		bIsSecure ? "secure" : "insecure" );

	if ( !noip )
		Host_Status_Printf( conprint, log, "tcp/ip  :  %s\n", NET_AdrToString( net_local_adr ) );

	Host_Status_Printf( conprint, log, "map     :  %s at: %d x, %d y, %d z\n", sv.name, (int)r_origin[0],
		(int)r_origin[1], (int)r_origin[2] );

	int nClients;
	SV_CountPlayers( &nClients );
	Host_Status_Printf( conprint, log, "players :  %i active (%i max)\n\n", nClients, svs.maxclients );
	Host_Status_Printf(
		conprint, log, fmt, "# ", "name    ", "userid   ", "uniqueid ", "frag", "time    ", "ping", "loss", "adr" );

	client_t* client = svs.clients;
	for ( int j = 0; j < svs.maxclients; j++, client++ )
	{
		if ( !client->active )
			continue;

		int seconds = (int)( realtime - client->netchan.connect_time );
		int minutes = seconds / 60;
		int hours = 0;
		if ( minutes )
		{
			seconds -= minutes * 60;
			hours = minutes / 60;
			if ( hours )
				minutes -= hours * 60;
		}

		char szTime[32];
		if ( hours )
			_snprintf( szTime, sizeof( szTime ), "%-2i:%02i:%02i", hours, minutes, seconds );
		else
			_snprintf( szTime, sizeof( szTime ), "%02i:%02i", minutes, seconds );

		const char* address = conprint ? NET_AdrToString( client->netchan.remote_address ) : com_empty_string;
		const char* loss = va( "%-4i", (int)client->packet_loss );
		const char* ping = va( "%-4i", SV_CalcPing( client ) );
		const char* frags = va( "%-4i", (int)client->edict->v.frags );
		const char* uniqueID = SV_GetClientIDString( client );
		const char* userid = va( "%-7i", client->userid );
		const char* name = va( "\"%s\"", client->name );
		const char* number = va( "%-2i", j + 1 );

		Host_Status_Printf( conprint, log, fmt, number, name, userid, uniqueID, frags, szTime, ping, loss, address );
	}

	Host_Status_Printf( conprint, log, "%i users\n", nClients );
}

/*
==================
Host_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
==================
*/
void Host_Reconnect_f()
{
	char cmd[128];

	if ( cls.state == ca_dedicated || cls.state == ca_disconnected || cls.state == ca_connecting )
		return;

	if ( cls.passive )
	{
		char temp[128];

		Q_strncpy_s( temp, NET_AdrToString( cls.connect_stream ), sizeof( temp ) );
		temp[strcspn( temp, ";\n" )] = '\0';
		_snprintf( cmd, sizeof( cmd ), "listen %s\n", temp );
		Cbuf_AddText( cmd );
		return;
	}

	SCR_BeginLoadingPlaque( TRUE );
	cls.signon = 0;
	cls.state = ca_connected;
	g_demotimescale = 1.0f;

	Netchan_Clear( &cls.netchan );
	SZ_Clear( &cls.netchan.message );
	MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
	MSG_WriteString( &cls.netchan.message, "new" );
}

/*
==================
Host_Kill_f
==================
*/
void Host_Kill_f()
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( sv_player->v.health <= 0 )
	{
		SV_ClientPrintf( "Can't suicide -- already dead!\n" );
		return;
	}

	if ( host_client->active && host_client->connected && host_client->spawned )
	{
		gGlobalVariables.time = (float)sv.time;
		gEntityInterface.pfnClientKill( sv_player );
	}
	else
	{
		SV_ClientPrintf( "Can't kill yourself -- you're not connected!\n" );
	}
}

void Host_TogglePause_f()
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( !pausable.value )
	{
		SV_ClientPrintf( "Pause not allowed.\n" );
	}
	else
	{
		sv.paused ^= 1;

		if ( sv.paused )
			SV_BroadcastPrintf( "%s paused the game\n", pr_strings + sv_player->v.netname );
		else
			SV_BroadcastPrintf( "%s unpaused the game\n", pr_strings + sv_player->v.netname );

		// send notification to all clients
		MSG_WriteByte( &sv.reliable_datagram, svc_setpause );
		MSG_WriteByte( &sv.reliable_datagram, sv.paused );
	}

	Steam_UpdateRichPresence();
}

/*
==================
Host_Pause_f
==================
*/
void Host_Pause_f()
{
	if ( !cl.levelname[0] )
		return;

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( !pausable.value )
		return;

	Steam_UpdateRichPresence();
	sv.paused = TRUE;
	// send notification to all clients
	MSG_WriteByte( &sv.reliable_datagram, svc_setpause );
	MSG_WriteByte( &sv.reliable_datagram, sv.paused );
	Steam_UpdateRichPresence();
}

void Host_Unpause_f()
{
	if ( !cl.levelname[0] )
		return;

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( !pausable.value )
		return;

	Steam_UpdateRichPresence();
	sv.paused = FALSE;
	MSG_WriteByte( &sv.reliable_datagram, svc_setpause );
	MSG_WriteByte( &sv.reliable_datagram, sv.paused );
	Steam_UpdateRichPresence();
}

/*
==================
Host_God_f

Sets client to godmode
==================
*/
void Host_God_f()
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( gGlobalVariables.deathmatch || !sv_cheats.value )
		return;

	sv_player->v.flags ^= FL_GODMODE;
	if ( !( sv_player->v.flags & FL_GODMODE ) )
		SV_ClientPrintf( "godmode OFF\n" );
	else
		SV_ClientPrintf( "godmode ON\n" );
}

void Host_Notarget_f()
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( gGlobalVariables.deathmatch )
		return;

	sv_player->v.flags ^= FL_NOTARGET;
	if ( !( sv_player->v.flags & FL_NOTARGET ) )
		SV_ClientPrintf( "notarget OFF\n" );
	else
		SV_ClientPrintf( "notarget ON\n" );
}

/*
==================
Host_Fly_f

Sets client to flymode
==================
*/
void Host_Fly_f()
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( gGlobalVariables.deathmatch )
		return;

	if ( sv_player->v.movetype != MOVETYPE_FLY )
	{
		sv_player->v.movetype = MOVETYPE_FLY;
		SV_ClientPrintf( "flymode ON\n" );
	}
	else
	{
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientPrintf( "flymode OFF\n" );
	}
}

int FindPassableSpace( edict_t* pEdict, vec_t* direction, float step )
{
	int i;

	for ( i = 0; i < 100; i++ )
	{
		VectorMA( pEdict->v.origin, step, direction, pEdict->v.origin );

		if ( !SV_TestEntityPosition( pEdict ) )
		{
			VectorCopy( pEdict->v.origin, pEdict->v.oldorigin );
			return TRUE;
		}
	}

	return FALSE;
}

void Host_Noclip_f()
{
	vec3_t forward, right, up;

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( gGlobalVariables.deathmatch || !sv_cheats.value )
		return;

	if ( sv_player->v.movetype != MOVETYPE_NOCLIP )
	{
		noclip_anglehack = TRUE;
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		SV_ClientPrintf( "noclip ON\n" );
		return;
	}

	noclip_anglehack = FALSE;
	sv_player->v.movetype = MOVETYPE_WALK;
	VectorCopy( sv_player->v.origin, sv_player->v.oldorigin );
	SV_ClientPrintf( "noclip OFF\n" );

	if ( !SV_TestEntityPosition( sv_player ) )
		return;

	AngleVectors( sv_player->v.v_angle, forward, right, up );
	if ( !FindPassableSpace( sv_player, forward, 1.0f ) && !FindPassableSpace( sv_player, right, 1.0f )
		 && !FindPassableSpace( sv_player, right, -1.0f ) && !FindPassableSpace( sv_player, up, 1.0f )
		 && !FindPassableSpace( sv_player, up, -1.0f ) && !FindPassableSpace( sv_player, forward, -1.0f ) )
	{
		Con_DPrintf( "Can't find the world\n" );
	}

	VectorCopy( sv_player->v.oldorigin, sv_player->v.origin );
}

char* Host_SaveGameDirectory()
{
	static char szDirectory[MAX_PATH];

	Q_memset( szDirectory, 0, sizeof( szDirectory ) );
	_snprintf( szDirectory, sizeof( szDirectory ), "SAVE/" );
	return szDirectory;
}

const char* Host_FindRecentSave( char* pNameBuf, int nameBufSize )
{
	char szPath[MAX_PATH];
	char basefindfn[MAX_PATH];
	const char* findfn;
	qboolean found = FALSE;
	int newest = 0;

	_snprintf( pNameBuf, nameBufSize, "%s*.sav", Host_SaveGameDirectory() );
	_snprintf( szPath, sizeof( szPath ), "%s", Host_SaveGameDirectory() );

	findfn = Sys_FindFirst( pNameBuf, basefindfn );
	while ( findfn )
	{
		if ( Q_strlen( findfn ) && Q_strcasecmp( findfn, "HLSave.sav" ) )
		{
			_snprintf( szPath, sizeof( szPath ), "%s%s", Host_SaveGameDirectory(), findfn );

			int ft = FS_GetFileTime( szPath );
			if ( ft > 0 && ( !found || newest < ft ) )
			{
				newest = ft;
				Q_strcpy( pNameBuf, findfn );
				found = TRUE;
			}
		}

		findfn = Sys_FindNext( basefindfn );
	}

	Sys_FindClose();

	return found ? pNameBuf : NULL;
}

void Host_Reload_f()
{
	char name[MAX_PATH];
	const char* pSaveName;

	if ( cls.demoplayback || !sv.active || cmd_source != src_command )
		return;

	Host_ClearGameState();
	SV_InactivateClients();
	SV_ServerShutdown();

	pSaveName = Host_FindRecentSave( name, sizeof( name ) );
	if ( !pSaveName || !Host_Load( pSaveName ) )
	{
		SV_SpawnServer( FALSE, gHostMap.string, NULL );
		SV_LoadEntities();
		SV_ActivateServer( 1 );
	}
}

void Host_Motd_f()
{
	FileHandle_t pFile;
	unsigned int length;
	char* pFileList;
	char* next;

	if ( motdfile.string[0] == '/' || Q_strstr( motdfile.string, ":" ) || Q_strstr( motdfile.string, ".." )
		 || Q_strstr( motdfile.string, "\\" ) )
	{
		Con_Printf( "Unable to open %s (contains illegal characters)\n", motdfile.string );
		return;
	}

	if ( Q_strcasecmp( COM_FileExtension( motdfile.string ), "txt" ) )
	{
		Con_Printf( "Invalid motdfile name %s (wrong file extension, must be .txt)\n", motdfile.string );
		return;
	}

	pFile = FS_Open( motdfile.string, "rb" );
	if ( !pFile )
	{
		Con_Printf( "Unable to open %s\n", motdfile.string );
		return;
	}

	length = FS_Size( pFile );
	if ( length )
	{
		char* pText = (char*)malloc( length + 1 );
		if ( pText )
		{
			FS_Read( pText, (int)length, 1, pFile );
			pText[length] = '\0';

			Con_Printf( "motd:" );
			pFileList = pText;
			while ( ( next = strchr( pFileList, '\n' ) ) != NULL )
			{
				*next = '\0';
				Con_Printf( "%s\n", pFileList );
				pFileList = next + 1;
			}
			Con_Printf( "%s\n", pFileList );

			free( pText );
		}
	}

	FS_Close( pFile );
}

void Host_Motd_Write_f()
{
	char newFile[2048];
	const char* pszExt;
	FileHandle_t pFile;
	size_t i;

	if ( !sv.active || cmd_source != src_command || cls.state != ca_dedicated )
		return;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "No message provided\n" );
		return;
	}

	pszExt = COM_FileExtension( motdfile.string );
	if ( !IsSafeFileToDownload( motdfile.string ) || Q_strcasecmp( pszExt, "txt" ) )
	{
		Con_Printf( "Invalid motdfile name (%s)\n", motdfile.string );
		return;
	}

	pFile = FS_Open( motdfile.string, "wb+" );
	if ( !pFile )
	{
		Con_Printf( "Unable to open %s\n", motdfile.string );
		return;
	}

	Q_strncpy( newFile, Cmd_Args(), sizeof( newFile ) );

	for ( i = 0; i < strlen( newFile ); i++ )
	{
		if ( newFile[i] == '\\' && newFile[i + 1] == 'n' )
		{
			newFile[i] = '\n';
			memmove( &newFile[i + 1], &newFile[i + 2], strlen( &newFile[i + 2] ) + 1 );
		}
	}

	FS_Write( newFile, (int)strlen( newFile ), 1, pFile );
	FS_Close( pFile );

	Con_Printf( "Done.\n" );
}

int __cdecl Host_Load( const char* pName )
{
	char name[256];
	GAME_HEADER gameHeader;
	FileHandle_t pFile;
	const char* pszExt;
	int i;

	if ( !pName || !pName[0] )
		return 0;

	if ( Q_strstr( pName, ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return 0;
	}

	if ( pName[0] == '_' )
	{
		char number[5];
		int digits = 0;
		const char* p = pName + 1;

		while ( *p && isdigit( (signed char)*p ) )
		{
			if ( digits == (int)sizeof( number ) )
				__fastfail( FAST_FAIL_RANGE_CHECK_FAILURE );
			number[digits++] = *p++;
		}
		if ( digits == (int)sizeof( number ) )
			__fastfail( FAST_FAIL_RANGE_CHECK_FAILURE );
		number[digits] = '\0';

		int slot = Q_atoi( number );
		if ( slot < 1 || slot > 12 )
			return 0;

		_snprintf( name, sizeof( name ) - 4, "%sHalf-Life-%i", Host_SaveGameDirectory(), slot );
	}
	else
	{
		_snprintf( name, sizeof( name ) - 4, "%s%s", Host_SaveGameDirectory(), pName );
	}
	name[sizeof( name ) - 5] = '\0';

	if ( !svs.dll_initialized )
		Host_InitializeGameDLL();

	COM_DefaultExtension( name, ".sav" );
	pszExt = COM_FileExtension( name );
	if ( !pszExt || Q_strcasecmp( pszExt, "sav" ) )
		return 0;

	COM_FixSlashes( name );
	name[sizeof( name ) - 1] = '\0';

	Con_Printf( "Loading game from %s...\n", name );

	pFile = FS_OpenPathID( name, "rb", "GAMECONFIG" );
	if ( !pFile )
		return 0;

	Host_ClearGameState();

	if ( !SaveReadHeader( pFile, &gameHeader, 1 ) )
	{
		giStateInfo = STATE_ENDTRAINING;
		Cbuf_AddText( "\ndisconnect\n" );
		return 0;
	}

	cls.demonum = -1;
	SV_InactivateClients();
	SCR_BeginLoadingPlaque( FALSE );

	for ( i = 0; i < gameHeader.mapCount; i++ )
	{
		char fileName[MAX_PATH];
		int fileSize;

		FS_Read( fileName, sizeof( fileName ), 1, pFile );
		fileName[sizeof( fileName ) - 1] = '\0';
		FS_Read( &fileSize, sizeof( fileSize ), 1, pFile );

		pszExt = COM_FileExtension( fileName );
		if ( !pszExt || Q_strstr( fileName, "\\" ) || Q_strstr( fileName, ":" ) || Q_strstr( fileName, ".." )
			 || Q_strstr( fileName, "~" ) || fileName[0] == '/' || pszExt[0] != 'H' || pszExt[1] != 'L' )
		{
			FS_Seek( pFile, fileSize, FILESYSTEM_SEEK_CURRENT );
		}
		else
		{
			char szPath[MAX_PATH];

			_snprintf( szPath, sizeof( szPath ), "%s%s", Host_SaveGameDirectory(), fileName );
			COM_FixSlashes( szPath );

			FileHandle_t pCopy = FS_OpenPathID( szPath, "wb", "GAMECONFIG" );
			FileCopy( pCopy, pFile, fileSize );
			FS_Close( pCopy );
		}
	}

	FS_Close( pFile );

	Cvar_SetValue( "deathmatch", 0 );
	Cvar_SetValue( "coop", 0 );

	if ( !Q_strcasecmp( com_gamedir, "valve" ) || !Q_strcasecmp( com_gamedir, "bshift" )
		 || !Q_strcasecmp( com_gamedir, "gearbox" ) )
	{
		svs.maxclients = 1;
		Cvar_SetValue( "maxplayers", 1 );
	}

	_snprintf( name, sizeof( name ), "map %s\n", gameHeader.mapName );
	CL_Disconnect();
	Host_Map( FALSE, name, gameHeader.mapName, TRUE );

	return 1;
}

void FileCopy( FileHandle_t pOutput, FileHandle_t pInput, int fileSize )
{
	char buf[FILE_COPY_SIZE];
	int size;

	while ( fileSize > 0 )
	{
		if ( fileSize > FILE_COPY_SIZE )
			size = FILE_COPY_SIZE;
		else
			size = fileSize;

		FS_Read( buf, size, 1, pInput );
		FS_Write( buf, size, 1, pOutput );
		fileSize -= size;
	}
}

TYPEDESCRIPTION gGameHeaderDescription[] =
{
	{ FIELD_INTEGER, "mapCount", offsetof( GAME_HEADER, mapCount ), 1, 0 },
	{ FIELD_CHARACTER, "mapName", offsetof( GAME_HEADER, mapName ), 32, 0 },
	{ FIELD_CHARACTER, "comment", offsetof( GAME_HEADER, comment ), 80, 0 },
};

void SaveExit( SAVERESTOREDATA* save )
{
	if ( save )
	{
		if ( save->pTokens )
		{
			Mem_Free( save->pTokens );
			save->pTokens = NULL;
			save->tokenCount = 0;
		}
		Mem_Free( save );
	}

	gGlobalVariables.pSaveData = NULL;
}

int SaveReadHeader( FileHandle_t pFile, GAME_HEADER* pHeader, int readGlobalState )
{
	int tag;

	FS_Read( &tag, sizeof( tag ), 1, pFile );
	if ( tag != SAVEGAME_HEADER )
	{
		FS_Close( pFile );
		return 0;
	}

	FS_Read( &tag, sizeof( tag ), 1, pFile );
	if ( tag != SAVEGAME_VERSION )
	{
		FS_Close( pFile );
		return 0;
	}

	return SaveReadHeader_0( pFile, pHeader, readGlobalState );
}

TYPEDESCRIPTION gEntityTableDescription[] =
{
	{ FIELD_INTEGER, "id", offsetof( ENTITYTABLE, id ), 1, 0 },
	{ FIELD_INTEGER, "location", offsetof( ENTITYTABLE, location ), 1, 0 },
	{ FIELD_INTEGER, "size", offsetof( ENTITYTABLE, size ), 1, 0 },
	{ FIELD_INTEGER, "flags", offsetof( ENTITYTABLE, flags ), 1, 0 },
	{ FIELD_STRING, "classname", offsetof( ENTITYTABLE, classname ), 1, 0 },
};

TYPEDESCRIPTION gSaveHeaderDescription[] =
{
	{ FIELD_INTEGER, "skillLevel", offsetof( SAVE_HEADER, skillLevel ), 1, 0 },
	{ FIELD_INTEGER, "entityCount", offsetof( SAVE_HEADER, entityCount ), 1, 0 },
	{ FIELD_INTEGER, "connectionCount", offsetof( SAVE_HEADER, connectionCount ), 1, 0 },
	{ FIELD_INTEGER, "lightStyleCount", offsetof( SAVE_HEADER, lightStyleCount ), 1, 0 },
	{ FIELD_TIME, "time", offsetof( SAVE_HEADER, time ), 1, 0 },
	{ FIELD_CHARACTER, "mapName", offsetof( SAVE_HEADER, mapName ), 32, 0 },
	{ FIELD_CHARACTER, "skyName", offsetof( SAVE_HEADER, skyName ), 32, 0 },
	{ FIELD_INTEGER, "skyColor_r", offsetof( SAVE_HEADER, skyColor_r ), 1, 0 },
	{ FIELD_INTEGER, "skyColor_g", offsetof( SAVE_HEADER, skyColor_g ), 1, 0 },
	{ FIELD_INTEGER, "skyColor_b", offsetof( SAVE_HEADER, skyColor_b ), 1, 0 },
	{ FIELD_FLOAT, "skyVec_x", offsetof( SAVE_HEADER, skyVec_x ), 1, 0 },
	{ FIELD_FLOAT, "skyVec_y", offsetof( SAVE_HEADER, skyVec_y ), 1, 0 },
	{ FIELD_FLOAT, "skyVec_z", offsetof( SAVE_HEADER, skyVec_z ), 1, 0 },
};

TYPEDESCRIPTION gAdjacencyDescription[] =
{
	{ FIELD_CHARACTER, "mapName", offsetof( LEVELLIST, mapName ), 32, 0 },
	{ FIELD_CHARACTER, "landmarkName", offsetof( LEVELLIST, landmarkName ), 32, 0 },
	{ FIELD_EDICT, "pentLandmark", offsetof( LEVELLIST, pentLandmark ), 1, 0 },
	{ FIELD_VECTOR, "vecLandmarkOrigin", offsetof( LEVELLIST, vecLandmarkOrigin ), 1, 0 },
};

TYPEDESCRIPTION gLightstyleDescription[] =
{
	{ FIELD_INTEGER, "index", offsetof( SAVELIGHTSTYLE, index ), 1, 0 },
	{ FIELD_CHARACTER, "style", offsetof( SAVELIGHTSTYLE, style ), MAX_LIGHTSTYLES, 0 },
};

SAVERESTOREDATA* LoadSaveData( const char* level )
{
	char name[MAX_PATH];
	char* pszTokenList;
	FileHandle_t pFile;
	SAVERESTOREDATA* pSaveData;
	int i, tag, size, tableCount, tokenCount, tokenSize;
	uint32_t tableSize, totalSize;

	_snprintf( name, sizeof( name ), "%s%s.HL1", Host_SaveGameDirectory(), level );
	COM_FixSlashes( name );
	Con_Printf( "Loading game from %s...\n", name );

	pFile = FS_OpenPathID( name, "rb", "GAMECONFIG" );
	if ( !pFile )
	{
		Con_Printf( "ERROR: couldn't open.\n" );
		return NULL;
	}

	//---------------------------------
	// Read the header
	FS_Read( &tag, sizeof( tag ), 1, pFile );
	if ( tag != SAVEFILE_HEADER )
	{
		FS_Close( pFile );
		return NULL;
	}

	FS_Read( &tag, sizeof( tag ), 1, pFile );
	if ( tag > SAVEGAME_VERSION )
	{
		FS_Close( pFile );
		return NULL;
	}

	FS_Read( &size, sizeof( size ), 1, pFile );
	FS_Read( &tableCount, sizeof( tableCount ), 1, pFile );
	FS_Read( &tokenCount, sizeof( tokenCount ), 1, pFile );
	FS_Read( &tokenSize, sizeof( tokenSize ), 1, pFile );

	tableSize = (uint32_t)tableCount * sizeof( ENTITYTABLE );
	if ( size < 0 || tokenCount < 0 || tokenSize < 0 || (uint32_t)tokenSize > INT_MAX - (uint32_t)size )
	{
		FS_Close( pFile );
		return NULL;
	}

	totalSize = (uint32_t)size + tokenSize;
	if ( tableSize > INT_MAX - totalSize )
	{
		FS_Close( pFile );
		return NULL;
	}

	totalSize += tableSize;
	if ( INT_MAX - totalSize < sizeof( SAVERESTOREDATA ) )
	{
		FS_Close( pFile );
		return NULL;
	}

	pSaveData = (SAVERESTOREDATA*)Mem_Calloc( (int)( totalSize + sizeof( SAVERESTOREDATA ) ), 1 );
	if ( !pSaveData )
	{
		FS_Close( pFile );
		return NULL;
	}

	pSaveData->tableCount = tableCount;
	pSaveData->tokenCount = tokenCount;
	pSaveData->tokenSize = tokenSize;

	Q_strncpy( pSaveData->szCurrentMapName, level, sizeof( pSaveData->szCurrentMapName ) - 1 );
	pSaveData->szCurrentMapName[sizeof( pSaveData->szCurrentMapName ) - 1] = '\0';

	pszTokenList = (char*)( pSaveData + 1 );

	if ( tokenSize > 0 )
	{
		FS_Read( pszTokenList, pSaveData->tokenSize, 1, pFile );

		if ( !pSaveData->pTokens )
			pSaveData->pTokens = (char**)Mem_Calloc( tokenCount, sizeof( char* ) );

		// Make sure the token strings pointed to by the pToken hashtable.
		for ( i = 0; i < tokenCount; i++ )
		{
			pSaveData->pTokens[i] = *pszTokenList ? pszTokenList : NULL;
			pszTokenList += strlen( pszTokenList ) + 1;
		}
	}

	pSaveData->pTable = (ENTITYTABLE*)pszTokenList;
	pSaveData->connectionCount = 0;
	pSaveData->size = 0;
	pSaveData->pBaseData = pszTokenList + tableSize;
	pSaveData->pCurrentData = pSaveData->pBaseData;
	pSaveData->bufferSize = size;
	pSaveData->fUseLandmark = 1;
	pSaveData->time = 0.0f;
	VectorClear( pSaveData->vecLandmarkOffset );

	gGlobalVariables.pSaveData = pSaveData;

	FS_Read( pSaveData->pBaseData, size, 1, pFile );
	FS_Close( pFile );

	return pSaveData;
}

void ParseSaveTables( SAVERESTOREDATA* pSaveData, SAVE_HEADER* pHeader, int updateGlobals )
{
	int i;
	SAVELIGHTSTYLE light;

	// Re-base the savedata since we re-ordered the entity/table / restore fields
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		gEntityInterface.pfnSaveReadFields( pSaveData, "ETABLE", &pSaveData->pTable[i], gEntityTableDescription,
			ARRAYSIZE( gEntityTableDescription ) );
		pSaveData->pTable[i].pent = NULL;
	}

	pSaveData->pBaseData = pSaveData->pCurrentData;
	pSaveData->size = 0;

	gEntityInterface.pfnSaveReadFields(
		pSaveData, "Save Header", pHeader, gSaveHeaderDescription, ARRAYSIZE( gSaveHeaderDescription ) );

	pSaveData->connectionCount = pHeader->connectionCount;
	pSaveData->time = pHeader->time;
	pSaveData->fUseLandmark = 1;
	VectorClear( pSaveData->vecLandmarkOffset );

	// Read adjacency list
	for ( i = 0; i < pSaveData->connectionCount; i++ )
	{
		gEntityInterface.pfnSaveReadFields( pSaveData, "ADJACENCY", &pSaveData->levelList[i], gAdjacencyDescription,
			ARRAYSIZE( gAdjacencyDescription ) );
	}

	if ( updateGlobals )
		memset( sv.lightstyles, 0, sizeof( sv.lightstyles ) );

	for ( i = 0; i < pHeader->lightStyleCount; i++ )
	{
		gEntityInterface.pfnSaveReadFields(
			pSaveData, "LIGHTSTYLE", &light, gLightstyleDescription, ARRAYSIZE( gLightstyleDescription ) );

		if ( updateGlobals )
		{
			if ( light.index < 0 || light.index >= MAX_LIGHTSTYLES )
			{
				Con_Printf( "Ignoring invalid saved lightstyle index %d\n", light.index );
				continue;
			}

			light.style[sizeof( light.style ) - 1] = '\0';
			int styleLength = Q_strlen( light.style );
			sv.lightstyles[light.index] = (char*)Hunk_Alloc( styleLength + 1 );
			Q_memcpy( sv.lightstyles[light.index], light.style, styleLength + 1 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Read the list of entities that are no longer in the save file for this level (they've been moved to another level)
//   and correct the table
// Input  : *pSaveData -
//			*level -
//-----------------------------------------------------------------------------
void EntityPatchRead( SAVERESTOREDATA* pSaveData, const char* level )
{
	char name[MAX_PATH];
	FileHandle_t pFile;
	int i, size, entityId;

	_snprintf( name, sizeof( name ), "%s%s.HL3", Host_SaveGameDirectory(), level );
	COM_FixSlashes( name );

	pFile = FS_OpenPathID( name, "rb", "GAMECONFIG" );
	if ( !pFile )
		return;

	FS_Read( &size, sizeof( int ), 1, pFile );
	for ( i = 0; i < size; i++ )
	{
		FS_Read( &entityId, sizeof( int ), 1, pFile );
		pSaveData->pTable[entityId].flags = FENTTABLE_REMOVED;
	}

	FS_Close( pFile );
}

static void EntityInit( edict_t* pEdict, string_t className )
{
	ENTITYINIT pEntityInit;

	if ( !className )
		Sys_Error( "Bad class!!\n" );

	ReleaseEntityDLLFields( pEdict );
	InitEntityDLLFields( pEdict );
	pEdict->v.classname = className;

	pEntityInit = GetEntityInit( pr_strings + className );
	if ( pEntityInit )
		pEntityInit( &pEdict->v );
}

static qboolean EntityInSolid( edict_t* pent )
{
	vec3_t point;

	if ( pent->v.movetype == MOVETYPE_FOLLOW && pent->v.aiment && ( pent->v.aiment->v.flags & FL_CLIENT ) )
		return FALSE;

	VectorAdd( pent->v.absmin, pent->v.absmax, point );
	VectorScale( point, 0.5f, point );

	g_groupmask = pent->v.groupinfo;

	return SV_PointContents( point ) == CONTENTS_SOLID;
}

int LoadGamestate( char* level, int createPlayers )
{
	int i;
	SAVERESTOREDATA* pSaveData;
	SAVE_HEADER header;
	ENTITYTABLE* pEntInfo;
	edict_t* pent;

	pSaveData = LoadSaveData( level );
	if ( !pSaveData ) // Couldn't load the file
		return 0;

	ParseSaveTables( pSaveData, &header, 1 );
	EntityPatchRead( pSaveData, level );

	Cvar_SetValue( "skill", (float)header.skillLevel );
	Q_strncpy( sv.name, header.mapName, sizeof( sv.name ) - 1 );
	sv.name[sizeof( sv.name ) - 1] = '\0';
	Cvar_Set( "sv_skyname", header.skyName );
	Cvar_SetValue( "sv_skycolor_r", (float)header.skyColor_r );
	Cvar_SetValue( "sv_skycolor_g", (float)header.skyColor_g );
	Cvar_SetValue( "sv_skycolor_b", (float)header.skyColor_b );
	Cvar_SetValue( "sv_skyvec_x", header.skyVec_x );
	Cvar_SetValue( "sv_skyvec_y", header.skyVec_y );
	Cvar_SetValue( "sv_skyvec_z", header.skyVec_z );

	// Create entity list
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		pEntInfo = &pSaveData->pTable[i];

		if ( !pEntInfo->classname || !pEntInfo->size || ( pEntInfo->flags & FENTTABLE_REMOVED ) )
		{
			pEntInfo->pent = NULL;
			continue;
		}

		if ( !pEntInfo->id )
		{
			pent = sv.edicts;
		}
		else if ( pEntInfo->id > svs.maxclients )
		{
			pEntInfo->pent = CreateNamedEntity( pEntInfo->classname );
			continue;
		}
		else
		{
			if ( !( pEntInfo->flags & FENTTABLE_PLAYER ) )
				Sys_Error( "ENTITY IS NOT A PLAYER: %d\n", i );

			pent = svs.clients[pEntInfo->id - 1].edict;
			if ( !createPlayers || !pent )
			{
				pEntInfo->pent = NULL;
				continue;
			}
		}

		EntityInit( pent, pEntInfo->classname );
		pEntInfo->pent = pent;
	}

	// Now spawn entities
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		pEntInfo = &pSaveData->pTable[i];
		pent = pEntInfo->pent;

		pSaveData->currentIndex = i;
		pSaveData->size = pEntInfo->location;
		pSaveData->pCurrentData = pSaveData->pBaseData + pEntInfo->location;

		if ( pent )
		{
			if ( gEntityInterface.pfnRestore( pent, pSaveData, 0 ) < 0 )
			{
				pEntInfo->pent = NULL;
				ED_Free( pent );
			}
			else
			{
				SV_LinkEdict( pent, FALSE );
			}
		}
	}

	SaveExit( pSaveData );

	sv.time = header.time;

	return 1;
}

void Host_Map( qboolean bIsDemo, char* mapstring, char* mapName, qboolean loadGame )
{
	int i;
	UserMsg* pMsg;

	Host_ShutdownServer( FALSE );
	key_dest = key_game;
	SCR_BeginLoadingPlaque( FALSE );

	if ( !loadGame )
	{
		Host_ClearGameState();
		SV_InactivateClients();
		svs.serverflags = 0;
	}

	Q_strncpy( cls.mapstring, mapstring, sizeof( cls.mapstring ) - 1 );
	cls.mapstring[sizeof( cls.mapstring ) - 1] = '\0';

	if ( !SV_SpawnServer( bIsDemo, mapName, NULL ) )
		return;

	ContinueLoadingProgressBar( "Server", 7, 0.0f );

	if ( loadGame )
	{
		if ( !LoadGamestate( mapName, 1 ) )
			SV_LoadEntities();

		sv.paused = TRUE;
		sv.loadgame = TRUE;
		SV_ActivateServer( 0 );
	}
	else
	{
		SV_LoadEntities();
		SV_ActivateServer( 1 );
		if ( !sv.active )
			return;

		if ( cls.state != ca_dedicated )
		{
			Q_strcpy( cls.spawnparms, com_empty_string );
			for ( i = 2; i < Cmd_Argc(); i++ )
			{
				strncat( cls.spawnparms, Cmd_Argv( i ), sizeof( cls.spawnparms ) - 1 - strlen( cls.spawnparms ) );
				strncat( cls.spawnparms, " ", sizeof( cls.spawnparms ) - 1 - strlen( cls.spawnparms ) );
			}
		}
	}

	if ( sv_gpNewUserMsgs )
	{
		pMsg = sv_gpUserMsgs;
		if ( pMsg )
		{
			while ( pMsg->next )
				pMsg = pMsg->next;
			pMsg->next = sv_gpNewUserMsgs;
		}
		else
		{
			sv_gpUserMsgs = sv_gpNewUserMsgs;
		}

		sv_gpNewUserMsgs = NULL;
	}

	if ( cls.state != ca_dedicated )
		Cmd_ExecuteString( "connect local", src_command );

	Steam_UpdateRichPresence();
}

void Host_InitializeGameDLL()
{
	Cbuf_Execute();
	NET_Config( svs.maxclients > 1 );

	if ( svs.dll_initialized )
	{
		Con_DPrintf( "Sys_InitializeGameDLL called twice, skipping second call\n" );
		return;
	}

	svs.dll_initialized = TRUE;
	LoadEntityDLLs( host_parms.basedir );

	gEntityInterface.pfnGameInit();
	gEntityInterface.pfnPM_Init( &g_svmove );
	gEntityInterface.pfnRegisterEncoders();

	SV_InitEncoders();
	SV_GetPlayerHulls();
	SV_CheckBlendingInterface();
	SV_CheckSaveGameCommentInterface();
	Cbuf_Execute();
}

edict_t* FindViewthing()
{
	int i;
	edict_t* e;

	for ( i = 0; i < sv.num_edicts; i++ )
	{
		e = &sv.edicts[i];
		if ( !Q_strcmp( pr_strings + e->v.classname, "viewthing" ) )
			return e;
	}

	Con_Printf( "No viewthing on map\n" );
	return NULL;
}

void PrintFrameName( model_t* m, int frame )
{
	aliashdr_t* hdr = (aliashdr_t*)Mod_Extradata( m );
	if ( hdr )
		Con_Printf( "frame %i: %s\n", frame, hdr->frames[frame].name );
}

/*
==================
Host_Viewmodel_f
==================
*/
void Host_Viewmodel_f()
{
	edict_t* e;
	model_t* m;

	e = FindViewthing();
	if ( !e )
		return;

	m = Mod_ForName( Cmd_Argv( 1 ), FALSE, FALSE );
	if ( !m )
	{
		Con_Printf( "Can't load %s\n", Cmd_Argv( 1 ) );
		return;
	}

	e->v.frame = 0;
	cl.model_precache[e->v.modelindex] = m;
}

/*
==================
Host_Viewframe_f
==================
*/
void Host_Viewframe_f()
{
	edict_t* e;
	model_t* m;
	int f;

	e = FindViewthing();
	if ( !e )
		return;

	m = CL_GetModelByIndex( e->v.modelindex );

	f = Q_atoi( Cmd_Argv( 1 ) );
	if ( f >= m->numframes )
		f = m->numframes - 1;

	e->v.frame = (float)f;
}

/*
==================
Host_Viewnext_f
==================
*/
void Host_Viewnext_f()
{
	edict_t* e;
	model_t* m;

	e = FindViewthing();
	if ( !e )
		return;

	m = CL_GetModelByIndex( e->v.modelindex );

	e->v.frame = e->v.frame + 1;
	if ( e->v.frame >= m->numframes )
		e->v.frame = (float)( m->numframes - 1 );

	PrintFrameName( m, (int)e->v.frame );
}

/*
==================
Host_Viewprev_f
==================
*/
void Host_Viewprev_f()
{
	edict_t* e;
	model_t* m;

	e = FindViewthing();
	if ( !e )
		return;

	m = CL_GetModelByIndex( e->v.modelindex );

	e->v.frame = e->v.frame - 1;
	if ( e->v.frame < 0 )
		e->v.frame = 0;

	PrintFrameName( m, (int)e->v.frame );
}

int CreateEntityList( SAVERESTOREDATA* pSaveData, int levelMask )
{
	int i;
	int active;
	int movedCount = 0;
	ENTITYTABLE* pEntInfo;
	edict_t* pent;
	client_t* client;

	// Create entity list
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		pEntInfo = &pSaveData->pTable[i];
		pent = NULL;

		if ( pEntInfo->classname && pEntInfo->size && pEntInfo->id > 0 )
		{
			active = pEntInfo->flags & levelMask;

			if ( pEntInfo->id > svs.maxclients )
			{
				if ( active )
					pent = CreateNamedEntity( pEntInfo->classname );
			}
			else
			{
				client = &svs.clients[pEntInfo->id - 1];
				pent = client->edict;

				if ( active )
				{
					if ( !( pEntInfo->flags & FENTTABLE_PLAYER ) )
						Sys_Error( "ENTITY IS NOT A PLAYER: %d\n", i );

					if ( client->active && pent )
						EntityInit( pent, pEntInfo->classname );
					else
						pent = NULL;
				}
			}
		}

		pEntInfo->pent = pent;
	}

	// Now spawn entities
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		pEntInfo = &pSaveData->pTable[i];
		pent = pEntInfo->pent;

		pSaveData->currentIndex = i;
		pSaveData->size = pEntInfo->location;
		pSaveData->pCurrentData = pSaveData->pBaseData + pEntInfo->location;

		if ( !pent || !( pEntInfo->flags & levelMask ) )
			continue;

		if ( pEntInfo->flags & FENTTABLE_GLOBAL )
		{
			Con_DPrintf( "Merging changes for global: %s\n", pr_strings + pEntInfo->classname );
			gEntityInterface.pfnRestore( pent, pSaveData, 1 );
			ED_Free( pent );
			continue;
		}

		Con_DPrintf( "Transferring %s (%d)\n", pr_strings + pEntInfo->classname, NUM_FOR_EDICT( pent ) );

		if ( gEntityInterface.pfnRestore( pent, pSaveData, 0 ) < 0 )
		{
			ED_Free( pent );
			continue;
		}

		SV_LinkEdict( pent, FALSE );

		if ( !( pSaveData->pTable[i].flags & FENTTABLE_PLAYER ) && EntityInSolid( pent ) )
		{
			Con_DPrintf( "Suppressing %s\n", pr_strings + pSaveData->pTable[i].classname );
			ED_Free( pent );
			continue;
		}

		movedCount++;
		pSaveData->pTable[i].flags = FENTTABLE_REMOVED;
	}

	return movedCount;
}

void EntityPatchWrite( SAVERESTOREDATA* pSaveData, const char* level )
{
	char name[MAX_PATH];
	FileHandle_t pFile;
	int i, size;

	_snprintf( name, sizeof( name ), "%s%s.HL3", Host_SaveGameDirectory(), level );
	COM_FixSlashes( name );

	pFile = FS_OpenPathID( name, "wb", "GAMECONFIG" );
	if ( !pFile )
		return;

	size = 0;
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		if ( pSaveData->pTable[i].flags & FENTTABLE_REMOVED )
			size++;
	}

	// Patch count
	FS_Write( &size, sizeof( int ), 1, pFile );
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		if ( pSaveData->pTable[i].flags & FENTTABLE_REMOVED )
			FS_Write( &i, sizeof( int ), 1, pFile );
	}

	FS_Close( pFile );
}

void LoadAdjacentEntities( const char* pOldLevel, const char* pLandmarkName )
{
	SAVERESTOREDATA currentLevelData;
	SAVERESTOREDATA* pSaveData;
	SAVE_HEADER header;
	vec3_t landmarkOrigin, oldLandmarkOrigin;
	const char* pLevel;
	int i, test, index;
	uint32_t flags;
	int movedCount = 0;

	Q_memset( &currentLevelData, 0, sizeof( SAVERESTOREDATA ) );
	gGlobalVariables.pSaveData = &currentLevelData;

	// Build the adjacent map list
	gEntityInterface.pfnParmsChangeLevel();

	for ( i = 0; i < currentLevelData.connectionCount; i++ )
	{
		pLevel = currentLevelData.levelList[i].mapName;

		// Only do maps once
		for ( test = 0; test < i; test++ )
		{
			if ( !Q_strcmp( pLevel, currentLevelData.levelList[test].mapName ) )
				break;
		}

		// Map was already in the list
		if ( test < i )
			continue;

		pSaveData = LoadSaveData( pLevel );
		if ( !pSaveData )
			continue;

		ParseSaveTables( pSaveData, &header, 0 );
		EntityPatchRead( pSaveData, pLevel );
		pSaveData->fUseLandmark = 1;
		pSaveData->time = (float)sv.time;

		LandmarkOrigin( &currentLevelData, landmarkOrigin, pLandmarkName );
		LandmarkOrigin( pSaveData, oldLandmarkOrigin, pLandmarkName );
		VectorSubtract( landmarkOrigin, oldLandmarkOrigin, pSaveData->vecLandmarkOffset );

		flags = 0;
		if ( !Q_strcmp( pLevel, pOldLevel ) )
			flags |= FENTTABLE_PLAYER;

		for ( index = 0; index < pSaveData->connectionCount; index++ )
		{
			if ( !Q_strcmp( pSaveData->levelList[index].mapName, sv.name ) )
				flags |= 1u << index;
		}

		if ( flags )
			movedCount = CreateEntityList( pSaveData, (int)flags );
		if ( movedCount )
			EntityPatchWrite( pSaveData, pLevel );

		SaveExit( pSaveData );
	}

	gGlobalVariables.pSaveData = NULL;
}

void Host_EndSection( const char* pszSection )
{
	giActive = DLL_PAUSED;
	giSubState = ENG_NORMAL;
	giStateInfo = STATE_ENDTRAINING;

	if ( pszSection && *pszSection )
	{
		if ( !Q_strcasecmp( pszSection, "_oem_end_training" ) )
			giStateInfo = STATE_ENDTRAINING;
		else if ( !Q_strcasecmp( pszSection, "_oem_end_logo" ) )
			giStateInfo = STATE_ENDLOGO;
		else if ( !Q_strcasecmp( pszSection, "_oem_end_demo" ) )
			giStateInfo = STATE_ENDDEMO;
		else
			Con_DPrintf( " endsection with unknown Section keyvalue\n" );
	}
	else
	{
		Con_Printf( " endsection with no arguments\n" );
	}

	Cbuf_AddText( "\ndisconnect\n" );
}

SAVERESTOREDATA* SaveInit( int size )
{
	int i;
	SAVERESTOREDATA* pSaveData;
	ENTITYTABLE* pTable;

	if ( size <= 0 )
		size = 0x80000;

	pSaveData = (SAVERESTOREDATA*)Mem_Calloc(
		(int)( size + sizeof( ENTITYTABLE ) * sv.num_edicts + sizeof( SAVERESTOREDATA ) ), 1 );

	pSaveData->tokenSize = 0;
	pSaveData->pTable = (ENTITYTABLE*)( pSaveData + 1 );

	pSaveData->tokenCount = 0xFFF;
	pSaveData->pTokens = (char**)Mem_Calloc( 0xFFF, sizeof( char* ) );

	for ( i = 0; i < sv.num_edicts; i++ )
	{
		pTable = &pSaveData->pTable[i];
		pTable->id = i;
		pTable->pent = &sv.edicts[i];
		pTable->flags = 0;
		pTable->location = 0;
		pTable->size = 0;
		pTable->classname = 0;
	}

	pSaveData->tableCount = sv.num_edicts;
	pSaveData->connectionCount = 0;
	pSaveData->size = 0;
	pSaveData->pBaseData = (char*)( pSaveData->pTable + sv.num_edicts );
	pSaveData->pCurrentData = pSaveData->pBaseData;
	pSaveData->bufferSize = size;
	pSaveData->time = gGlobalVariables.time;
	VectorCopy( vec3_origin, pSaveData->vecLandmarkOffset );
	pSaveData->fUseLandmark = 0;

	gGlobalVariables.pSaveData = pSaveData;

	return pSaveData;
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void CL_Save( const char* name )
{
	DECALLIST decalList[4096];
	DECALLIST* entry;
	FileHandle_t pFile;
	int i, decalCount, tag;

	decalCount = DecalListCreate( decalList );

	pFile = FS_OpenPathID( name, "wb", "GAMECONFIG" );
	if ( !pFile )
		return;

	tag = SAVEFILE_HEADER;
	FS_Write( &tag, sizeof( int ), 1, pFile );
	tag = SAVEGAME_VERSION;
	FS_Write( &tag, sizeof( int ), 1, pFile );
	FS_Write( &decalCount, sizeof( int ), 1, pFile );

	for ( i = 0; i < decalCount; i++ )
	{
		entry = &decalList[i];
#ifdef _WIN32
		FS_Write( entry->name, DECAL_NAME_BYTES, 1, pFile );
#else
		FS_Write( entry->name, sizeof( entry->name ), 1, pFile );
#endif
		FS_Write( &entry->entityIndex, sizeof( entry->entityIndex ), 1, pFile );
		FS_Write( &entry->depth, sizeof( entry->depth ), 1, pFile );
		FS_Write( &entry->flags, sizeof( entry->flags ), 1, pFile );
		FS_Write( entry->position, sizeof( entry->position ), 1, pFile );
	}

	FS_Close( pFile );
}

SAVERESTOREDATA* SaveGamestate()
{
	char name[256];
	char* pTableData;
	char* pTokenData;
	const char* pTokenList;
	FileHandle_t pFile;
	SAVERESTOREDATA* pSaveData;
	SAVE_HEADER header;
	SAVELIGHTSTYLE light;
	edict_t* pent;
	int i, tag, dataSize, tableSize;

	if ( !gEntityInterface.pfnParmsChangeLevel )
		return NULL;

	FS_CreateDirHierarchy( Host_SaveGameDirectory(), "GAMECONFIG" );
	pSaveData = SaveInit( 0 );

	_snprintf( name, sizeof( name ), "%s%s.HL1", Host_SaveGameDirectory(), sv.name );
	COM_FixSlashes( name );

	// Build the adjacent map list (after entity table build by game in presave)
	gEntityInterface.pfnParmsChangeLevel();

	header.version = build_number();
	header.skillLevel = (int)skill.value;
	header.entityCount = pSaveData->tableCount;
	header.connectionCount = pSaveData->connectionCount;
	header.time = (float)sv.time;
	Q_strncpy( header.skyName, sv_skyname.string, sizeof( header.skyName ) - 1 );
	header.skyName[sizeof( header.skyName ) - 1] = '\0';
	header.skyColor_r = (int)sv_skycolor_r.value;
	header.skyColor_g = (int)sv_skycolor_g.value;
	header.skyColor_b = (int)sv_skycolor_b.value;
	header.skyVec_x = sv_skyvec_x.value;
	header.skyVec_y = sv_skyvec_y.value;
	header.skyVec_z = sv_skyvec_z.value;

	pSaveData->time = 0;

	Q_strncpy( header.mapName, sv.name, sizeof( header.mapName ) - 1 );
	header.mapName[sizeof( header.mapName ) - 1] = '\0';

	header.lightStyleCount = 0;
	for ( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		if ( sv.lightstyles[i] )
			header.lightStyleCount++;
	}

	gEntityInterface.pfnSaveWriteFields(
		pSaveData, "Save Header", &header, gSaveHeaderDescription, ARRAYSIZE( gSaveHeaderDescription ) );

	pSaveData->time = header.time;

	// Write adjacency list
	for ( i = 0; i < pSaveData->connectionCount; i++ )
	{
		gEntityInterface.pfnSaveWriteFields( pSaveData, "ADJACENCY", &pSaveData->levelList[i], gAdjacencyDescription,
			ARRAYSIZE( gAdjacencyDescription ) );
	}

	// write the light styles
	for ( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		if ( !sv.lightstyles[i] )
			continue;

		light.index = i;
		Q_strncpy( light.style, sv.lightstyles[i], sizeof( light.style ) - 1 );
		light.style[sizeof( light.style ) - 1] = '\0';
		gEntityInterface.pfnSaveWriteFields(
			pSaveData, "LIGHTSTYLE", &light, gLightstyleDescription, ARRAYSIZE( gLightstyleDescription ) );
	}

	for ( i = 0; i < sv.num_edicts; i++ )
	{
		pent = &sv.edicts[i];

		pSaveData->currentIndex = i;
		pSaveData->pTable[i].location = pSaveData->size;
		pSaveData->pTable[i].size = 0;

		if ( pent->free )
			continue;

		gEntityInterface.pfnSave( pent, pSaveData );

		if ( i > 0 && i <= svs.maxclients )
			pSaveData->pTable[i].flags |= FENTTABLE_PLAYER;
	}

	dataSize = pSaveData->size;
	pTableData = pSaveData->pCurrentData;

	for ( i = 0; i < sv.num_edicts; i++ )
	{
		gEntityInterface.pfnSaveWriteFields( pSaveData, "ETABLE", &pSaveData->pTable[i], gEntityTableDescription,
			ARRAYSIZE( gEntityTableDescription ) );
	}

	tableSize = pSaveData->size - dataSize;

	// Write entity string token table
	pTokenData = pSaveData->pCurrentData;
	if ( pSaveData->pTokens )
	{
		for ( i = 0; i < pSaveData->tokenCount; i++ )
		{
			pTokenList = pSaveData->pTokens[i];
			if ( pTokenList )
			{
				do
				{
					*pSaveData->pCurrentData++ = *pTokenList;
				} while ( *pTokenList++ );
			}
			else
			{
				*pSaveData->pCurrentData++ = '\0';
			}
		}
	}
	pSaveData->tokenSize = (int)( pSaveData->pCurrentData - pTokenData );

	COM_CreatePath( name );

	pFile = FS_OpenPathID( name, "wb", "GAMECONFIG" );
	if ( !pFile )
	{
		Con_Printf( "Unable to open save game file %s.", name );
		return NULL;
	}

	// Write the header -- THIS SHOULD NEVER CHANGE STRUCTURE, USE SAVE_HEADER FOR NEW HEADER INFORMATION
	tag = SAVEFILE_HEADER;
	FS_Write( &tag, sizeof( int ), 1, pFile );
	tag = SAVEGAME_VERSION;
	FS_Write( &tag, sizeof( int ), 1, pFile );
	FS_Write( &pSaveData->size, sizeof( int ), 1, pFile ); // Does not include token table
	FS_Write( &pSaveData->tableCount, sizeof( int ), 1, pFile );
	// Write out the tokens first so we can load them before we load the entities
	FS_Write( &pSaveData->tokenCount, sizeof( int ), 1, pFile );
	FS_Write( &pSaveData->tokenSize, sizeof( int ), 1, pFile );

	FS_Write( pTokenData, pSaveData->tokenSize, 1, pFile );
	FS_Write( pTableData, tableSize, 1, pFile );
	FS_Write( pSaveData->pBaseData, dataSize, 1, pFile );
	FS_Close( pFile );

	EntityPatchWrite( pSaveData, sv.name );

	_snprintf( name, sizeof( name ), "%s%s.HL2", Host_SaveGameDirectory(), sv.name );
	COM_FixSlashes( name );
	CL_Save( name );

	return pSaveData;
}

void Host_Changelevel2_f()
{
	char level[MAX_QPATH];
	char _startspot[MAX_QPATH];
	char oldlevel[MAX_QPATH];
	char* startspot;
	SAVERESTOREDATA* pSaveData;
	int restored;

	giActive = DLL_TRANS;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "changelevel2 <levelname> : continue game on a new level in the unit\n" );
		return;
	}

	if ( !sv.active || cls.demoplayback || sv.paused )
	{
		Con_Printf( "Only the server may changelevel\n" );
		return;
	}

	if ( svs.maxclients > 1 )
	{
		Con_Printf( "changelevel2 <levelname> : not for use with multiplayer games\n" );
		return;
	}

	SCR_BeginLoadingPlaque( TRUE );
	S_StopAllSounds( TRUE );

	Q_strncpy( level, Cmd_Argv( 1 ), sizeof( level ) - 1 );
	level[sizeof( level ) - 1] = '\0';

	startspot = NULL;
	if ( Cmd_Argc() != 2 )
	{
		Q_strncpy( _startspot, Cmd_Argv( 2 ), sizeof( _startspot ) - 1 );
		_startspot[sizeof( _startspot ) - 1] = '\0';
		if ( _startspot[0] )
			startspot = _startspot;
	}

	Q_strncpy( oldlevel, sv.name, sizeof( oldlevel ) - 1 );
	oldlevel[sizeof( oldlevel ) - 1] = '\0';

	pSaveData = SaveGamestate();
	SV_ServerShutdown();
	FS_LogLevelLoadStarted( level );

	if ( !SV_SpawnServer( FALSE, level, startspot ) )
		Sys_Error( "Host_Changelevel2: Couldn't load map %s\n", level );

	if ( pSaveData )
		SaveExit( pSaveData );

	// try to restore the new level
	restored = LoadGamestate( level, 0 );
	if ( !restored )
		SV_LoadEntities();

	LoadAdjacentEntities( oldlevel, startspot );

	sv.paused = TRUE;
	sv.loadgame = TRUE;
	gGlobalVariables.time = (float)sv.time;

	if ( !restored && sv_newunit.value )
		Host_ClearSaveDirectory();

	SV_ActivateServer( 0 );
	Steam_UpdateRichPresence();
}

/*
==================
Host_Changelevel_f

Goes to a new map, taking all clients along
==================
*/
void Host_Changelevel_f()
{
	char level[MAX_QPATH];
	char _startspot[MAX_QPATH];
	char* startspot;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "changelevel <levelname> : continue game on a new level\n" );
		return;
	}

	if ( !sv.active || cls.demoplayback )
	{
		Con_Printf( "Only the server may changelevel\n" );
		return;
	}

	if ( !PF_IsMapValid_I( (char*)Cmd_Argv( 1 ) ) )
	{
		Con_Printf( "changelevel failed: '%s' not found on server.\n", Cmd_Argv( 1 ) );
		return;
	}

	Q_strncpy( level, Cmd_Argv( 1 ), sizeof( level ) - 1 );
	level[sizeof( level ) - 1] = '\0';

	startspot = NULL;
	if ( Cmd_Argc() != 2 )
	{
		Q_strncpy( _startspot, Cmd_Argv( 2 ), sizeof( _startspot ) - 1 );
		_startspot[sizeof( _startspot ) - 1] = '\0';
		startspot = _startspot;
	}

	SCR_BeginLoadingPlaque( TRUE );
	S_StopAllSounds( TRUE );
	SV_InactivateClients();
	SV_ServerShutdown();
	SV_SpawnServer( FALSE, level, startspot );
	SV_LoadEntities();
	SV_ActivateServer( 1 );
}

struct TITLECOMMENT
{
	const char* pBSPName;
	const char* pTitleName;
};

TITLECOMMENT gTitleComments[] =
{
	{ "T0A0", "#T0A0TITLE" },
	{ "C0A0", "#C0A0TITLE" },
	{ "C1A0", "#C0A1TITLE" },
	{ "C1A1", "#C1A1TITLE" },
	{ "C1A2", "#C1A2TITLE" },
	{ "C1A3", "#C1A3TITLE" },
	{ "C1A4", "#C1A4TITLE" },
	{ "C2A1", "#C2A1TITLE" },
	{ "C2A2", "#C2A2TITLE" },
	{ "C2A3", "#C2A3TITLE" },
	{ "C2A4D", "#C2A4TITLE2" },
	{ "C2A4E", "#C2A4TITLE2" },
	{ "C2A4F", "#C2A4TITLE2" },
	{ "C2A4G", "#C2A4TITLE2" },
	{ "C2A4", "#C2A4TITLE1" },
	{ "C2A5", "#C2A5TITLE" },
	{ "C3A1", "#C3A1TITLE" },
	{ "C3A2", "#C3A2TITLE" },
	{ "C4A1A", "#C4A1ATITLE" },
	{ "C4A1B", "#C4A1ATITLE" },
	{ "C4A1C", "#C4A1ATITLE" },
	{ "C4A1D", "#C4A1ATITLE" },
	{ "C4A1E", "#C4A1ATITLE" },
	{ "C4A1", "#C4A1TITLE" },
	{ "C4A2", "#C4A2TITLE" },
	{ "C4A3", "#C4A3TITLE" },
	{ "C5A1", "#C5TITLE" },

	{ "OFBOOT", "#OF_BOOT0TITLE" },
	{ "OF0A", "#OF1A1TITLE" },
	{ "OF1A1", "#OF1A3TITLE" },
	{ "OF1A2", "#OF1A3TITLE" },
	{ "OF1A3", "#OF1A3TITLE" },
	{ "OF1A4", "#OF1A3TITLE" },
	{ "OF1A", "#OF1A5TITLE" },
	{ "OF2A1", "#OF2A1TITLE" },
	{ "OF2A2", "#OF2A1TITLE" },
	{ "OF2A3", "#OF2A1TITLE" },
	{ "OF2A", "#OF2A4TITLE" },
	{ "OF3A1", "#OF3A1TITLE" },
	{ "OF3A2", "#OF3A1TITLE" },
	{ "OF3A", "#OF3A3TITLE" },
	{ "OF4A1", "#OF4A1TITLE" },
	{ "OF4A2", "#OF4A1TITLE" },
	{ "OF4A3", "#OF4A1TITLE" },
	{ "OF4A", "#OF4A4TITLE" },
	{ "OF5A", "#OF5A1TITLE" },
	{ "OF6A1", "#OF6A1TITLE" },
	{ "OF6A2", "#OF6A1TITLE" },
	{ "OF6A3", "#OF6A1TITLE" },
	{ "OF6A4b", "#OF6A4TITLE" },
	{ "OF6A4", "#OF6A1TITLE" },
	{ "OF6A5", "#OF6A4TITLE" },
	{ "OF6A", "#OF6A4TITLE" },
	{ "OF7A", "#OF7A0TITLE" },

	{ "ba_tram", "#BA_TRAMTITLE" },
	{ "ba_security", "#BA_SECURITYTITLE" },
	{ "ba_main", "#BA_SECURITYTITLE" },
	{ "ba_elevator", "#BA_SECURITYTITLE" },
	{ "ba_canal", "#BA_CANALSTITLE" },
	{ "ba_yard", "#BA_YARDTITLE" },
	{ "ba_xen", "#BA_XENTITLE" },
	{ "ba_hazard", "#BA_HAZARD" },
	{ "ba_power", "#BA_POWERTITLE" },
	{ "ba_teleport1", "#BA_YARDTITLE" },
	{ "ba_teleport", "#BA_TELEPORTTITLE" },
	{ "ba_outro", "#BA_OUTRO" },

	{ "contamination", "#contamination" },
	{ "crossfire", "#crossfire" },
	{ "datacore", "#datacore" },
	{ "disposal", "#disposal" },
	{ "doublecross", "#doublecross" },
	{ "frenzy", "#frenzy" },
	{ "gasworks", "#gasworks" },
	{ "lambda_bunker", "#lambda_bunker" },
	{ "pool_party", "#pool_party" },
	{ "rapidcore", "#rapidcore" },
	{ "rocket_frenzy", "#rocket_frenzy" },
	{ "rustmill", "#rustmill" },
	{ "snark_pit", "#snark_pit" },
	{ "stalkyard", "#stalkyard" },
	{ "subtransit", "#subtransit" },
	{ "undertow", "#undertow" },
	{ "xen_dm", "#xen_dm" },
	{ "bounce", "#bounce" },
};

const char* CL_MapLocalizationString( const char* mapName )
{
	size_t i;

	for ( i = 0; i < ARRAYSIZE( gTitleComments ); i++ )
	{
		if ( !Q_strnicmp( mapName, gTitleComments[i].pBSPName, Q_strlen( gTitleComments[i].pBSPName ) ) )
			return gTitleComments[i].pTitleName;
	}

	return mapName;
}

int DirectoryCount( const char* pPath )
{
	int count = 0;

	if ( Sys_FindFirstPathID( pPath, "GAMECONFIG" ) )
	{
		count++;
		while ( Sys_FindNext( NULL ) )
			count++;
	}

	Sys_FindClose();

	return count;
}

int EntryInTable( SAVERESTOREDATA* pSaveData, const char* pMapName, int index )
{
	int i;

	for ( i = index + 1; i < pSaveData->connectionCount; i++ )
	{
		if ( !Q_strcmp( pSaveData->levelList[i].mapName, pMapName ) )
			return i;
	}

	return -1;
}

int FileSize( FileHandle_t pFile )
{
	if ( !pFile )
		return 0;

	return (int)FS_Size( pFile );
}

void Host_ClearGameState()
{
	S_StopAllSounds( TRUE );
	Host_ClearSaveDirectory();
	if ( gEntityInterface.pfnResetGlobalState )
		gEntityInterface.pfnResetGlobalState();
}

void Host_Crash_f()
{
	*(volatile int*)NULL = (int)0xDEADBEEF;
}

void DirectoryCopy( const char* pPath, FileHandle_t pFile )
{
	char basefindfn[MAX_PATH];
	char szName[MAX_PATH];
	const char* findfn;
	FileHandle_t pCopy;
	int size;

	for ( findfn = Sys_FindFirst( pPath, basefindfn ); findfn; findfn = Sys_FindNext( basefindfn ) )
	{
		_snprintf( szName, sizeof( szName ), "%s%s", Host_SaveGameDirectory(), findfn );
		COM_FixSlashes( szName );

		pCopy = FS_OpenPathID( szName, "rb", "GAMECONFIG" );
		size = FileSize( pCopy );

		FS_Write( findfn, MAX_PATH, 1, pFile );
		FS_Write( &size, sizeof( int ), 1, pFile );
		FileCopy( pFile, pCopy, size );
		FS_Close( pCopy );
	}

	Sys_FindClose();
}

void DirectoryExtract( FileHandle_t pFile, int fileCount )
{
	char fileName[MAX_PATH];
	char szName[MAX_PATH];
	char* pszExt;
	FileHandle_t pCopy;
	int i, fileSize;

#if defined( _WIN32 ) && ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) )
	for ( i = 0; i < fileCount; i++ )
#else
	for ( i = 0; i != fileCount; i++ )
#endif
	{
		// Filename can only be as long as a map name + extension
		FS_Read( fileName, MAX_PATH, 1, pFile );
		fileName[MAX_PATH - 1] = '\0';
		FS_Read( &fileSize, sizeof( int ), 1, pFile );

		pszExt = COM_LastFileExtension( fileName );
		if ( !pszExt || Q_strstr( fileName, "\\" ) || Q_strstr( fileName, ":" ) || Q_strstr( fileName, ".." )
			 || Q_strstr( fileName, "~" ) || fileName[0] == '/' || pszExt[0] != 'H' || pszExt[1] != 'L' )
		{
			FS_Seek( pFile, fileSize, FILESYSTEM_SEEK_CURRENT );
			continue;
		}

		_snprintf( szName, sizeof( szName ), "%s%s", Host_SaveGameDirectory(), fileName );
		COM_FixSlashes( szName );

		pCopy = FS_OpenPathID( szName, "wb", "GAMECONFIG" );
		FileCopy( pCopy, pFile, fileSize );
		FS_Close( pCopy );
	}
}

void LandmarkOrigin( SAVERESTOREDATA* pSaveData, vec_t* output, const char* pLandmarkName )
{
	int i;

	for ( i = 0; i < pSaveData->connectionCount; i++ )
	{
		if ( !Q_strcmp( pSaveData->levelList[i].landmarkName, pLandmarkName ) )
		{
			VectorCopy( pSaveData->levelList[i].vecLandmarkOrigin, output );
			return;
		}
	}

	VectorClear( output );
}

void Host_SavegameComment( char* pszBuffer, int iSizeBuffer )
{
	const char* pszMapName = pr_strings + gGlobalVariables.mapname;
	const char* pszTitleName = NULL;
	size_t i;

	for ( i = 0; i < ARRAYSIZE( gTitleComments ); i++ )
	{
		if ( !Q_strnicmp( pszMapName, gTitleComments[i].pBSPName, Q_strlen( gTitleComments[i].pBSPName ) ) )
		{
			pszTitleName = gTitleComments[i].pTitleName;
			break;
		}
	}

	if ( !pszTitleName )
	{
		if ( ( !pszMapName || !pszMapName[0] ) && cl.levelname[0] )
			pszTitleName = cl.levelname;
		else
			pszTitleName = pszMapName;
	}

	strncpy( pszBuffer, pszTitleName, iSizeBuffer - 1 );
	pszBuffer[iSizeBuffer - 1] = '\0';
}

int SaveReadHeader_0( FileHandle_t pFile, GAME_HEADER* pHeader, int readGlobalState )
{
	int i, size, tokenCount, tokenSize;
	char* pszTokenList;
	SAVERESTOREDATA* pSaveData;

	FS_Read( &size, sizeof( int ), 1, pFile );
	FS_Read( &tokenCount, sizeof( int ), 1, pFile );
	FS_Read( &tokenSize, sizeof( int ), 1, pFile );

	if ( size < 0 || tokenCount < 0 || tokenSize < 0 || INT_MAX - size < tokenSize
		 || INT_MAX - ( size + tokenSize ) < (int)sizeof( SAVERESTOREDATA ) )
	{
		FS_Close( pFile );
		return 0;
	}

	pSaveData = (SAVERESTOREDATA*)Mem_Calloc( size + tokenSize + (int)sizeof( SAVERESTOREDATA ), 1 );
	if ( !pSaveData )
	{
		FS_Close( pFile );
		return 0;
	}

	pSaveData->tableCount = 0;
	pSaveData->pTable = NULL;
	pSaveData->connectionCount = 0;

	pszTokenList = (char*)( pSaveData + 1 );

	if ( tokenSize > 0 )
	{
		pSaveData->tokenCount = tokenCount;
		pSaveData->tokenSize = tokenSize;

		FS_Read( pszTokenList, tokenSize, 1, pFile );

		if ( !pSaveData->pTokens )
			pSaveData->pTokens = (char**)Mem_Calloc( tokenCount, sizeof( char* ) );

		for ( i = 0; i < tokenCount; i++ )
		{
			pSaveData->pTokens[i] = *pszTokenList ? pszTokenList : NULL;
			pszTokenList += strlen( pszTokenList ) + 1;
		}
	}

	pSaveData->size = 0;
	pSaveData->bufferSize = size;
	pSaveData->fUseLandmark = 0;
	pSaveData->time = 0.0f;
	pSaveData->pCurrentData = pszTokenList;
	pSaveData->pBaseData = pszTokenList;

	FS_Read( pszTokenList, size, 1, pFile );

	gEntityInterface.pfnSaveReadFields(
		pSaveData, "GameHeader", pHeader, gGameHeaderDescription, ARRAYSIZE( gGameHeaderDescription ) );
	if ( readGlobalState )
		gEntityInterface.pfnRestoreGlobalState( pSaveData );

	SaveExit( pSaveData );

	return 1;
}

int LoadGame( const char* pName )
{
	int iRet;
	qboolean q = scr_skipupdate;

	scr_skipupdate = TRUE;
	iRet = Host_Load( pName );
	scr_skipupdate = q;

	return iRet;
}

void Host_Loadgame_f()
{
	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "load <savename> : load a game\n" );
		return;
	}

	if ( !Host_Load( Cmd_Argv( 1 ) ) )
		Con_Printf( "Error loading saved game\n" );
}

void Host_NextDemo()
{
	char str[1024];

	if ( cls.demonum == -1 )
		return; // don't play demos

	SCR_BeginLoadingPlaque( FALSE );

	if ( !cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS )
	{
		cls.demonum = 0;
		if ( !cls.demos[0][0] )
		{
			Con_Printf( "No demos listed with startdemos\n" );
			cls.demonum = -1;
			return;
		}
	}

	_snprintf( str, sizeof( str ), "playdemo %s\n", cls.demos[cls.demonum] );
	Cbuf_InsertText( str );
	cls.demonum++;
}

/*
==================
Host_Demos_f

Return to looping demos
==================
*/
void Host_Demos_f()
{
	if ( cls.state == ca_dedicated )
		return;

	if ( cls.demonum == -1 )
		cls.demonum = 0;

	CL_Disconnect_f();
	Host_NextDemo();
}

/*
==================
Host_Startdemos_f
==================
*/
void Host_Startdemos_f()
{
	int i, c;

	if ( cls.state == ca_dedicated )
	{
		if ( !sv.active )
			Con_Printf( "Cannot play demos on a dedicated server.\n" );
		//Cbuf_AddText ("map hldm1\n");
		return;
	}

	c = Cmd_Argc() - 1;
	if ( c > MAX_DEMOS )
	{
		Con_Printf( "Max %i demos in demoloop\n", MAX_DEMOS );
		c = MAX_DEMOS;
	}
	Con_Printf( "%i demo(s) in loop\n", c );

	for ( i = 1; i <= c; i++ )
	{
		Q_strncpy( cls.demos[i - 1], Cmd_Argv( i ), MAX_DEMONAME - 1 );
		cls.demos[i - 1][MAX_DEMONAME - 1] = '\0';

		cls.demos[i - 1][strcspn( cls.demos[i - 1], "\n;" )] = '\0';
	}

	if ( !sv.active && cls.demonum != -1 && !cls.demoplayback )
	{
		cls.demonum = 0;
		Host_NextDemo();
	}
	else
	{
		cls.demonum = -1;
	}
}

/*
==================
Host_Stopdemo_f

Return to looping demos
==================
*/
void Host_Stopdemo_f()
{
	if ( cls.state == ca_dedicated )
		return;

	if ( !cls.demoplayback )
		return;

	CL_StopPlayback();
	CL_Disconnect();
}

static void Host_BroadcastServerSay()
{
	char text[128];
	char* p;
	int i, j;
	client_t* client;
	client_t* save;

	if ( cls.state != ca_dedicated )
	{
		if ( cmd_source == src_command )
			Cmd_ForwardToServer();
		return;
	}

	if ( Cmd_Argc() < 2 )
		return;

	p = (char*)Cmd_Args();
	if ( !p )
		return;

	save = host_client;

	// remove quotes if present
	if ( *p == '"' )
	{
		p++;
		p[Q_strlen( p ) - 1] = '\0';
	}

	_snprintf( text, sizeof( text ), "%c<%s> ", 1, Cvar_VariableString( "hostname" ) );

	if ( Q_strlen( p ) > 63 )
		p[63] = '\0';

	j = (int)sizeof( text ) - 2 - Q_strlen( text ); // -2 for /n and null terminator
	if ( Q_strlen( p ) > j )
		p[j] = '\0';

	Q_strcat( text, p );
	Q_strcat( text, "\n" );

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( !client || !client->active || !client->spawned || client->fakeclient )
			continue;

		host_client = client;
		PF_MessageBegin_I( MSG_ONE, RegUserMsg( "SayText", -1 ), NULL, &sv.edicts[i + 1] );
		PF_WriteByte_I( 0 );
		PF_WriteString_I( text );
		PF_MessageEnd_I();
	}

	host_client = save;

	Sys_Printf( "%s", text + 1 );
	Log_Printf( "Server say \"%s\"\n", p );
}

void Host_Say( qboolean )
{
	Host_BroadcastServerSay();
}

void Host_Say_f()
{
	Host_BroadcastServerSay();
}

void Host_Say_Team_f()
{
	Host_BroadcastServerSay();
}

void Host_Tell_f()
{
	char text[64];
	char* p;
	char* pTarget;
	const char* tellmsg;
	int i, j;
	client_t* client;
	client_t* save;

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( Cmd_Argc() < 3 )
		return;

	p = (char*)Cmd_Args();
	if ( !p )
		return;

	_snprintf( text, sizeof( text ), "%s TELL: ", host_client->name );

	// remove quotes if present
	if ( *p == '"' )
	{
		p++;
		p[Q_strlen( p ) - 1] = '\0';
	}

	// check length & truncate if necessary
	j = (int)sizeof( text ) - 2 - Q_strlen( text ); // -2 for /n and null terminator
	if ( Q_strlen( p ) > j )
		p[j] = '\0';

	tellmsg = Cmd_Argv( 1 );
	pTarget = (char*)Q_strstr( p, tellmsg );
	if ( pTarget )
		Q_strcat( text, pTarget + Q_strlen( tellmsg ) );
	else
		Q_strcat( text, p );
	Q_strcat( text, "\n" );

	save = host_client;
	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( !client->active || !client->spawned || client->fakeclient )
			continue;

		if ( !Q_strcasecmp( client->name, tellmsg ) )
		{
			host_client = client;
			PF_MessageBegin_I( MSG_ONE, RegUserMsg( "SayText", -1 ), NULL, &sv.edicts[i + 1] );
			PF_WriteByte_I( 0 );
			PF_WriteString_I( text );
			PF_MessageEnd_I();
			break;
		}
	}

	host_client = save;
}

/*
==================
Host_SetInfo_f

Allow clients to change userinfo
==================
*/
void Host_SetInfo_f()
{
	if ( Cmd_Argc() == 1 )
	{
		Info_Print( cls.userinfo );
		return;
	}

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "usage: setinfo [ <key> <value> ]\n" );
		return;
	}

	if ( cmd_source == src_command )
	{
		Info_SetValueForKey( cls.userinfo, Cmd_Argv( 1 ), Cmd_Argv( 2 ), MAX_INFO_STRING );
		Cmd_ForwardToServer();
		return;
	}

	Info_SetValueForKey( host_client->userinfo, Cmd_Argv( 1 ), Cmd_Argv( 2 ), MAX_INFO_STRING );
	host_client->sendinfo = TRUE;
}

/*
==================
Host_FullInfo_f

Allow clients to change userinfo
==================
*/
void Host_FullInfo_f()
{
	char key[512];
	char value[512];
	char* o;
	const char* s;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "fullinfo <complete info string>\n" );
		return;
	}

	s = Cmd_Argv( 1 );
	if ( *s == '\\' )
		s++;

	while ( *s )
	{
		o = key;
		while ( *s && *s != '\\' && o < key + sizeof( key ) - 1 )
			*o++ = *s++;
		*o = '\0';

		if ( !*s )
		{
			Con_Printf( "MISSING VALUE\n" );
			return;
		}

		o = value;
		s++;
		while ( *s && *s != '\\' && o < value + sizeof( value ) - 1 )
			*o++ = *s++;
		*o = '\0';

		if ( *s )
			s++;

		if ( cmd_source == src_command )
		{
			Info_SetValueForKey( cls.userinfo, key, value, MAX_INFO_STRING );
			Cmd_ForwardToServer();
			return;
		}

		Info_SetValueForKey( host_client->userinfo, key, value, MAX_INFO_STRING );
		host_client->sendinfo = TRUE;
	}
}

void Host_SetGameDir_f()
{
	const char* pszGameDir;

	if ( Cmd_Argc() < 1 || !g_pPostRestartCmdLineArgs )
		return;

	pszGameDir = Cmd_Argv( 1 );
	if ( !pszGameDir || pszGameDir[0] == '/' || Q_strstr( pszGameDir, ":" ) || Q_strstr( pszGameDir, ".." )
		 || Q_strstr( pszGameDir, "\\" ) )
	{
		Con_Printf( "Couldn't set gamedir to %s (contains illegal characters)\n", pszGameDir );
		return;
	}

	strcat( g_pPostRestartCmdLineArgs, " -game " );
	strcat( g_pPostRestartCmdLineArgs, pszGameDir );
}

void Host_SetRenderer_f()
{
	int hardware;
	int windowed = 0;

	if ( cls.state == ca_dedicated || ( Cmd_Argc() != 2 && Cmd_Argc() != 3 ) )
		return;

	if ( !Q_strcasecmp( Cmd_Argv( 1 ), "software" ) )
		hardware = 0;
	else if ( !Q_strcasecmp( Cmd_Argv( 1 ), "gl" ) || Q_strcasecmp( Cmd_Argv( 1 ), "d3d" ) )
		hardware = 1;
	else
		hardware = 2;

	if ( Cmd_Argc() == 3 )
		windowed = !Q_strcasecmp( Cmd_Argv( 2 ), "windowed" );

	VideoMode_SwitchMode( hardware, windowed );
}

void Host_SetVideoMode_f()
{
	int width, height;
	int bpp = 16;

	if ( cls.state == ca_dedicated || ( Cmd_Argc() != 3 && Cmd_Argc() != 4 ) )
		return;

	width = Q_atoi( Cmd_Argv( 1 ) );
	height = Q_atoi( Cmd_Argv( 2 ) );
	if ( Cmd_Argc() == 4 )
		bpp = Q_atoi( Cmd_Argv( 3 ) );

	VideoMode_SetVideoMode( width, height, bpp );
}

void Host_ShowOSK_f()
{
	int x = 0;
	int y = 0;

	if ( Cmd_Argc() > 1 )
	{
#ifdef _WIN32
		x = atoi( Cmd_Argv( 1 ) );
		y = atoi( Cmd_Argv( 2 ) );
#else
		x = strtol( Cmd_Argv( 1 ), NULL, 10 );
		y = strtol( Cmd_Argv( 2 ), NULL, 10 );
#endif
	}

	Steam_ShowOnScreenKeyboard( x, y );
}

void SV_GetPlayerHulls()
{
	int i;

	for ( i = 0; i < MAX_PLAYER_HULLS; i++ )
	{
		if ( !gEntityInterface.pfnGetHullBounds( i, player_mins[i], player_maxs[i] ) )
			break;
	}
}

void SV_CheckBlendingInterface()
{
	int i;
	SERVER_GETBLENDINGINTERFACE_FUNC blend;

	R_ResetSvBlending();

	for ( i = 0; i < g_iextdllMac; i++ )
	{
		blend = (SERVER_GETBLENDINGINTERFACE_FUNC)Sys_GetProcAddress(
			g_rgextdll[i].lDLLHandle, "Server_GetBlendingInterface" );
		if ( !blend )
			continue;

		if ( blend( SV_BLENDING_INTERFACE_VERSION, &g_pSvBlendingAPI, &server_studio_api, rotationmatrix[0],
				 bonetransform[0][0] ) )
			return;

		Con_DPrintf( "Couldn't get server .dll studio model blending interface. Version mismatch?\n" );
		R_ResetSvBlending();
	}
}

void SV_CheckSaveGameCommentInterface()
{
	int i;
	SV_SAVEGAMECOMMENT_FUNC pTemp;

	for ( i = 0; i < g_iextdllMac; i++ )
	{
		pTemp = (SV_SAVEGAMECOMMENT_FUNC)Sys_GetProcAddress( g_rgextdll[i].lDLLHandle, "SV_SaveGameComment" );
		if ( pTemp )
		{
			g_pSaveGameCommentFunc = pTemp;
			return;
		}
	}
}

void Host_SaveAgeList( const char* pName, int count )
{
	char newName[MAX_PATH];
	char oldName[MAX_PATH];

	_snprintf( newName, sizeof( newName ), "%s%s%02d.sav", Host_SaveGameDirectory(), pName, count );
	COM_FixSlashes( newName );
	FS_RemoveFile( newName, "GAMECONFIG" );

	while ( count > 0 )
	{
		if ( count == 1 )
			_snprintf( oldName, sizeof( oldName ), "%s%s.sav", Host_SaveGameDirectory(), pName );
		else
			_snprintf( oldName, sizeof( oldName ), "%s%s%02d.sav", Host_SaveGameDirectory(), pName, count - 1 );
		COM_FixSlashes( oldName );

		_snprintf( newName, sizeof( newName ), "%s%s%02d.sav", Host_SaveGameDirectory(), pName, count );
		COM_FixSlashes( newName );

		FS_Rename( oldName, newName );
		count--;
	}
}

int SaveGameSlot( const char* pSaveName, const char* pSaveComment )
{
	char hlPath[256];
	char name[256];
	char* pTokenData;
	const char* pTokenList;
	FileHandle_t pFile;
	SAVERESTOREDATA* pSaveData;
	GAME_HEADER gameHeader;
	int i, tag;

	FS_CreateDirHierarchy( Host_SaveGameDirectory(), "GAMECONFIG" );

	pSaveData = SaveGamestate();
	if ( !pSaveData )
		return 0;

	SaveExit( pSaveData );
	pSaveData = SaveInit( 0 );

	_snprintf( hlPath, sizeof( hlPath ), "%s*.HL?", Host_SaveGameDirectory() );
	COM_FixSlashes( hlPath );

	gameHeader.mapCount = DirectoryCount( hlPath );
	Q_strncpy( gameHeader.mapName, sv.name, sizeof( gameHeader.mapName ) - 1 );
	gameHeader.mapName[sizeof( gameHeader.mapName ) - 1] = '\0';
	Q_strncpy( gameHeader.comment, pSaveComment, sizeof( gameHeader.comment ) - 1 );
	gameHeader.comment[sizeof( gameHeader.comment ) - 1] = '\0';

	gEntityInterface.pfnSaveWriteFields(
		pSaveData, "GameHeader", &gameHeader, gGameHeaderDescription, ARRAYSIZE( gGameHeaderDescription ) );
	gEntityInterface.pfnSaveGlobalState( pSaveData );

	// Write entity string token table
	pTokenData = pSaveData->pCurrentData;
	if ( pSaveData->pTokens )
	{
		for ( i = 0; i < pSaveData->tokenCount; i++ )
		{
			pTokenList = pSaveData->pTokens[i];
			if ( pTokenList )
				pSaveData->size += Q_strlen( pTokenList ) + 1;
			else
				pSaveData->size += 1;

			if ( pSaveData->size > pSaveData->bufferSize )
			{
				Con_Printf( "Token Table Save/Restore overflow!" );
				pSaveData->size = pSaveData->bufferSize;
				break;
			}

			if ( pTokenList )
			{
				do
				{
					*pSaveData->pCurrentData++ = *pTokenList;
				} while ( *pTokenList++ );
			}
			else
			{
				*pSaveData->pCurrentData++ = '\0';
			}
		}
	}

	pSaveData->tokenSize = (int)( pSaveData->pCurrentData - pTokenData );
	if ( pSaveData->size < pSaveData->bufferSize )
		pSaveData->size -= pSaveData->tokenSize;

	_snprintf( name, sizeof( name ) - 4, "%s%s", Host_SaveGameDirectory(), pSaveName );
	COM_DefaultExtension( name, ".sav" );
	COM_FixSlashes( name );
	Con_DPrintf( "Saving game to %s...\n", name );

#if defined( _WIN32 ) && ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) )
	if ( Q_strcasecmp( pSaveName, "quick" ) || Q_strcasecmp( pSaveName, "autosave" ) )
#else
	if ( Q_stricmp( pSaveName, "quick" ) || Q_stricmp( pSaveName, "autosave" ) )
#endif
		Host_SaveAgeList( pSaveName, 1 );

	pFile = FS_OpenPathID( name, "wb", "GAMECONFIG" );

	tag = SAVEGAME_HEADER;
	FS_Write( &tag, sizeof( int ), 1, pFile );
	tag = SAVEGAME_VERSION;
	FS_Write( &tag, sizeof( int ), 1, pFile );
	FS_Write( &pSaveData->size, sizeof( int ), 1, pFile );
	FS_Write( &pSaveData->tokenCount, sizeof( int ), 1, pFile );
	FS_Write( &pSaveData->tokenSize, sizeof( int ), 1, pFile );
	FS_Write( pTokenData, pSaveData->tokenSize, 1, pFile );
	FS_Write( pSaveData->pBaseData, pSaveData->size, 1, pFile );

	DirectoryCopy( hlPath, pFile );
	FS_Close( pFile );

	SaveExit( pSaveData );

	return 1;
}

int SaveGame( const char* pszSlot, const char* pszComment )
{
	int iRet;
	qboolean q = scr_skipupdate;

	scr_skipupdate = TRUE;
	iRet = SaveGameSlot( pszSlot, pszComment );
	scr_skipupdate = q;

	return iRet;
}

static void Host_FormatSaveComment( char* comment )
{
	char szTitle[80];
	int minutes, seconds;

	g_pSaveGameCommentFunc( szTitle, sizeof( szTitle ) );

	minutes = (int)( sv.time / 60.0 );
	seconds = (int)fmod( sv.time, 60.0 );

	_snprintf( comment, 80 - 1, "%-64.64s %02d:%02d", szTitle, minutes, seconds );
	comment[80 - 1] = '\0';
}

void Host_Savegame_f()
{
	char comment[80];
	char* pszSaveName;
	char* pszExt;

	if ( !Host_ValidSave() )
		return;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "save <savename> : save a game\n" );
		return;
	}

	pszSaveName = (char*)Cmd_Argv( 1 );
	if ( pszSaveName[0] == '/' || Q_strstr( pszSaveName, ":" ) || Q_strstr( pszSaveName, ".." )
		 || Q_strstr( pszSaveName, "\\" ) )
	{
		Con_Printf( "Couldn't open %s (contains illegal characters).\n", pszSaveName );
		return;
	}

	COM_DefaultExtension( pszSaveName, ".sav" );
	pszExt = COM_LastFileExtension( pszSaveName );
#if defined( _WIN32 ) && ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) )
	if ( !pszExt || Q_strcasecmp( pszExt, "sav" ) )
#else
	if ( !pszExt || Q_stricmp( pszExt, "sav" ) )
#endif
	{
		Con_Printf( "Couldn't open %s (wrong file extension, must be .sav)\n", pszSaveName );
		return;
	}

	Host_FormatSaveComment( comment );
	SaveGameSlot( pszSaveName, comment );
	CL_HudMessage( "GAMESAVED" );
}

void Host_AutoSave_f()
{
	char comment[80];

	if ( !Host_ValidSave() )
		return;

	Host_FormatSaveComment( comment );
	SaveGameSlot( "autosave", comment );
}

void SaveReadComment( FileHandle_t pFile, char* name )
{
	GAME_HEADER gameHeader;

	if ( SaveReadHeader( pFile, &gameHeader, 0 ) )
		Q_strcpy( name, gameHeader.comment );
}

/*
======================
Host_Map_f

handle a 
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
void Host_Map_f()
{
	int i, len;
	char mapstring[64];
	char name[64];
	CareerStateType careerState;

	if ( cmd_source != src_command )
	{
		g_careerState = CAREER_NONE;
		return;
	}

	careerState = g_careerState;

	if ( Cmd_Argc() > 1 && Q_strlen( Cmd_Args() ) > 54 )
	{
		Con_Printf( "map change failed: command string is too long.\n" );
		g_careerState = CAREER_NONE;
		return;
	}

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "map <levelname> : changes server to specified map\n" );
		g_careerState = CAREER_NONE;
		return;
	}

	CL_Disconnect();

	if ( careerState == CAREER_LOADING )
		g_careerState = CAREER_LOADING;

	if ( COM_CheckParm( "-steam" ) && PF_IsDedicatedServer() )
		g_bMajorMapChange = TRUE;

	FS_LogLevelLoadStarted( "Map_Common" );

	mapstring[0] = '\0';
	for ( i = 0; i < Cmd_Argc(); i++ )
	{
		strncat( mapstring, Cmd_Argv( i ), sizeof( mapstring ) - 2 - strlen( mapstring ) );
		strncat( mapstring, " ", sizeof( mapstring ) - 2 - strlen( mapstring ) );
	}
	Q_strcat( mapstring, "\n" );

	Q_strncpy( name, Cmd_Argv( 1 ), sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = '\0';

	if ( !svs.dll_initialized )
		Host_InitializeGameDLL();

	len = Q_strlen( name );
	if ( len > 4 && !Q_strcasecmp( name + len - 4, ".bsp" ) )
		name[len - 4] = '\0';

	FS_LogLevelLoadStarted( name );
	VGuiWrap2_LoadingStarted( "level", name );
	SCR_UpdateScreen();
	SCR_UpdateScreen();

	if ( !PF_IsMapValid_I( name ) )
	{
		Con_Printf( "map change failed: '%s' not found on server.\n", name );
		if ( COM_CheckParm( "-steam" ) && PF_IsDedicatedServer() )
		{
			g_bMajorMapChange = FALSE;
			Sys_Printf( "\n" );
		}
		return;
	}

	StartLoadingProgressBar( "Server", 24 );
	SetLoadingProgressBarStatusText( "#GameUI_StartingServer" );
	ContinueLoadingProgressBar( "Server", 1, 0.0f );

	Cvar_Set( "HostMap", name );
	Host_Map( FALSE, mapstring, name, FALSE );

	if ( COM_CheckParm( "-steam" ) && PF_IsDedicatedServer() )
	{
		g_bMajorMapChange = FALSE;
		Sys_Printf( "\n" );
	}

	ContinueLoadingProgressBar( "Server", 11, 0.0f );
	NotifyDedicatedServerUI( "UpdateMap" );

	if ( g_careerState == CAREER_LOADING )
	{
		g_careerState = CAREER_PLAYING;
		SetCareerAudioState( 1 );
	}
	else
	{
		SetCareerAudioState( 0 );
	}
}

void Host_KillVoice_f()
{
	Voice_Deinit();
}

int Host_ValidSave()
{
	if ( cmd_source != src_command )
		return 0;

	if ( !sv.active )
	{
		Con_Printf( "Not playing a local game.\n" );
		return 0;
	}

	if ( svs.maxclients != 1 )
	{
		Con_Printf( "Can't save multiplayer games.\n" );
		return 0;
	}

	if ( cls.state != ca_active || cls.signon != 2 )
	{
		Con_Printf( "Can't save during transition.\n" );
		return 0;
	}

	if ( cl.intermission )
	{
		Con_Printf( "Can't save in intermission.\n" );
		return 0;
	}

	if ( svs.clients->active && svs.clients->edict->v.health <= 0.0 )
	{
		Con_Printf( "Can't savegame with a dead player\n" );
		return 0;
	}

	return 1;
}
