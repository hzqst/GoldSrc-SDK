#include "host.h"
#include "engine_apiproxy.h"
#include "sound.h"
#include "common.h"
#include "client.h"
#include "cmd.h"
#include "buildnum.h"
#include "conprint.h"
#include "cmodel.h"
#include "cvar.h"
#include "decals.h"
#include "filesystem.h"
#include "info.h"
#include "keys.h"
#include "kbutton.h"
#include "mem.h"
#include "net.h"
#include "protocol.h"
#include "server.h"
#include "server_static.h"
#include "sys.h"
#include "wad.h"
#include "steam_api.h"
#include "hashpak.h"
#include "cd.h"
#include "pm_defs.h"
#include "registry.h"
#include "vid.h"
#include "voice.h"
#include "zone.h"
#include "eiface.h"

#include <climits>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <csetjmp>
#include <cstdlib>
#include <ctime>
#ifdef _WIN32
#include <io.h>
#endif
#include <sys/stat.h>

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _chmod chmod
#define _S_IREAD S_IRUSR
#define _S_IWRITE S_IWUSR
#endif

quakeparms_t host_parms;
qboolean host_initialized;
int minimum_memory;
qboolean gfNoMasterServer;
qboolean g_bUsingInGameAdvertisements;
double realtime; // without any filtering or bounding
double host_frametime;
double oldrealtime; // last frame run
int host_framecount;
unsigned short* host_basepal;
int host_hunklevel;
jmp_buf host_enddemo;
jmp_buf host_abortserver;
static double rolling_fps;

cvar_t developer = { const_cast<char*>( "developer" ), const_cast<char*>( "0" ), FCVAR_FILTERSTUFFTEXT, 0, nullptr };
cvar_t host_killtime = { const_cast<char*>( "host_killtime" ), const_cast<char*>( "0" ), 0, 0, nullptr };
cvar_t sys_ticrate = {
	const_cast<char*>( "sys_ticrate" ), const_cast<char*>( "100.0" ), FCVAR_FILTERSTUFFTEXT, 0, nullptr };
cvar_t fps_max = {
	const_cast<char*>( "fps_max" ), const_cast<char*>( "100.0" ), FCVAR_ARCHIVE | FCVAR_FILTERSTUFFTEXT, 0, nullptr };
cvar_t fps_override = { const_cast<char*>( "fps_override" ), const_cast<char*>( "0" ), FCVAR_ARCHIVE, 0, nullptr };
static cvar_t host_name = { const_cast<char*>( "hostname" ), const_cast<char*>( "Half-Life" ), 0, 0, nullptr };
cvar_t host_limitlocal = { const_cast<char*>( "host_limitlocal" ), const_cast<char*>( "0" ), 0, 0, nullptr };
cvar_t sys_timescale = { const_cast<char*>( "sys_timescale" ), const_cast<char*>( "1.0" ), 0, 0, nullptr };
cvar_t host_framerate = { const_cast<char*>( "host_framerate" ), const_cast<char*>( "0" ), 0, 0, nullptr };
static cvar_t host_speeds = { const_cast<char*>( "host_speeds" ), const_cast<char*>( "0" ), 0, 0, nullptr };
static cvar_t host_profile = { const_cast<char*>( "host_profile" ), const_cast<char*>( "0" ), 0, 0, nullptr };
cvar_t sv_stats = { const_cast<char*>( "sv_stats" ), const_cast<char*>( "1" ), 0, 0, nullptr };
cvar_t deathmatch = { const_cast<char*>( "deathmatch" ), const_cast<char*>( "0" ), FCVAR_SERVER, 0, nullptr };
cvar_t coop = { const_cast<char*>( "coop" ), const_cast<char*>( "0" ), FCVAR_SERVER, 0, nullptr };
cvar_t pausable = { const_cast<char*>( "pausable" ), const_cast<char*>( "1" ), FCVAR_SERVER, 0, nullptr };
cvar_t skill = { const_cast<char*>( "skill" ), const_cast<char*>( "1" ), 0, 0, nullptr };

extern cvar_t mp_logfile;
extern cvar_t mp_logecho;
extern cvar_t sv_log_onefile;
extern cvar_t sv_log_singleplayer;
extern cvar_t sv_logsecret;
extern cvar_t console;
extern cvar_t scr_connectmsg;
extern cvar_t gl_vsync;
extern cvar_t cl_gamegauge;
extern cvar_t suitvolume;
extern qboolean gfBackground;
extern qboolean cl_inmovie;
extern qboolean scr_skipupdate;
extern float scr_con_current;
extern vec3_t r_soundOrigin;
extern vec3_t r_playerViewportAngles;
extern playermove_t g_clmove;
extern int g_iQuitCommandIssued;
extern char* wadpath;

void CL_AdjustClock();
void CL_CheckClientState();
void CL_CheckForResend();
void CL_DecayLights();
void CL_GGSpeeds( float currentTime );
void CL_HTTPUpdate();
void CL_Init();
void CL_MoveSpectatorCamera();
void CL_ReadPackets();
qboolean CL_RequestMissingResources();
void CL_ShutDownClientStatic();
void CL_Shutdown();
void CL_UpdateSoundFade();
void CL_WriteMessageHistory( int startingCount, int command );

void SV_CheckForRcon();
void SV_ClearCaches();
void SV_ClearEntities();
void SV_ClearPacketEntities( client_frame_t* frame );
void SV_ClearResourceLists( client_t* client );
void SV_CountPlayers( int* clients );
void SV_DeallocateDynamicData();
void SV_Frame();
void SV_FullClientUpdate( client_t* client, sizebuf_t* message );
int SV_GetFragmentSize( void* client );
void SV_ServerShutdown();
void SV_Shutdown();
void ReleaseEntityDlls();

void Chase_Init();
void D_FlushCaches();
void Draw_Init();
void GL_Config();
void GL_Init();
void R_Init();
void R_InitTextures();
void SCR_EndLoadingPlaque();
void SCR_Init();
void SCR_UpdateScreen();
void V_Init();
void VID_WriteBuffer( const char* filename );

void S_Init();
void S_PrintStats();
void Voice_Deinit();
int Voice_GetCompressedData( char* destination, int maxBytes, qboolean bFinal );
void Voice_Idle( float frameTime );

void Cvar_CmdInit();
void DELTA_Init();
void DELTA_Shutdown();
int DemoPlayer_Init();
void Host_ClearSaveDirectory();
void Host_Quit_f();
void Host_UpdateStats();
void NotifyDedicatedServerUI( const char* message );
void Steam_NotifyClientDisconnect( client_t* client );
void Steam_ProcessInsecureLaunchParameters();
void Steam_Shutdown();
void SystemWrapper_Init();
void SystemWrapper_RunFrame( double time );
void SystemWrapper_ShutDown();
void VGui_Shutdown();
void VGui_Startup();

void Host_Shutdown()
{
	static qboolean isdown;
	if ( isdown )
	{
		std::printf( "recursive shutdown\n" );
		return;
	}
	isdown = 1;
	if ( host_initialized )
		Host_WriteConfiguration();
	SV_ServerShutdown();
	Voice_Deinit();
	host_initialized = 0;
	CDAudio_Shutdown();
	VGui_Shutdown();
	if ( cls.state != ca_dedicated )
		ClientDLL_Shutdown();
	Cmd_RemoveGameCmds();
	Cmd_Shutdown();
	Cvar_ResetList();
	HPAK_FlushHostQueue();
	SV_DeallocateDynamicData();
	for ( int i = 0; i < svs.maxclientslimit; ++i )
		SV_ClearFrames( &svs.clients[i].frames );
	SV_Shutdown();
	SystemWrapper_ShutDown();
	NET_Shutdown();
	S_Shutdown();
	Con_Shutdown();
	ReleaseEntityDlls();
	CL_ShutDownClientStatic();
	CM_FreePAS();
	if ( wadpath )
	{
		Mem_Free( wadpath );
		wadpath = nullptr;
	}
	if ( cls.state != ca_dedicated )
		Draw_Shutdown();
	Draw_DecalShutdown();
	W_Shutdown();
	Log_Printf( "Server shutdown\n" );
	Log_Close();
	COM_Shutdown();
	CL_Shutdown();
	DELTA_Shutdown();
	Key_Shutdown();
	realtime = 0.0;
	sv.time = 0.0;
	cl.time = 0.0;
}

qboolean Host_FilterTime( float time )
{
	constexpr float BACKGROUND_FRAME_RATE = 31.0f;
	constexpr float MINIMUM_FRAME_RATE = 0.5f;
	constexpr float DEFAULT_MAX_FRAME_RATE = 100.0f;
	constexpr float MULTIPLAYER_MIN_FRAME_RATE = 20.0f;
	constexpr float FRAME_RATE_ROUNDING_BIAS = 0.5f;
	constexpr double MAXIMUM_FRAME_TIME = 0.25;
	static int command_line_ticrate = -1;
	if ( host_framerate.value > 0.0f && ( ( sv.active ? svs.maxclients : cl.maxclients ) == 1 || cls.demoplayback ) )
	{
		host_frametime = static_cast<float>( sys_timescale.value * host_framerate.value );
		realtime += host_frametime;
		return 1;
	}
	realtime += static_cast<float>( time * sys_timescale.value );
	if ( g_bIsDedicatedServer )
	{
		if ( command_line_ticrate == -1 )
			command_line_ticrate = COM_CheckParm( "-sys_ticrate" );
		float tickRate = sys_ticrate.value;
		if ( command_line_ticrate > 0 )
			tickRate = static_cast<float>( std::atof( com_argv[command_line_ticrate + 1] ) );
		if ( tickRate > 0.0f && 1.0 / static_cast<float>( tickRate + 1.0f ) > realtime - oldrealtime )
			return 0;
	}
	else
	{	// don't allow really long or short frames
		float fps = fps_max.value;
		if ( !sv.active && cls.state != ca_disconnected && cls.state != ca_active )
			fps = BACKGROUND_FRAME_RATE;
		fps = MINIMUM_FRAME_RATE > fps ? MINIMUM_FRAME_RATE : fps;
		if ( fps_override.value == 0.0f )
			fps = DEFAULT_MAX_FRAME_RATE < fps ? DEFAULT_MAX_FRAME_RATE : fps;
		if ( cl.maxclients > 1 )
			fps = MULTIPLAYER_MIN_FRAME_RATE > fps ? MULTIPLAYER_MIN_FRAME_RATE : fps;
		if ( gl_vsync.value != 0.0f && fps_override.value == 0.0f )
			fps = DEFAULT_MAX_FRAME_RATE;
		if ( !cls.timedemo
			 && static_cast<float>( sys_timescale.value / static_cast<float>( fps + FRAME_RATE_ROUNDING_BIAS ) )
					> realtime - oldrealtime )
			return 0;
	}
	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;
	if ( host_frametime > MAXIMUM_FRAME_TIME )
		host_frametime = MAXIMUM_FRAME_TIME;
	return 1;
}

void Host_Speeds( double* time )
{
	constexpr double MILLISECONDS_PER_SECOND = 1000.0;
	constexpr double MINIMUM_PROFILE_FRAME_TIME = 0.0001;
	constexpr double MAXIMUM_PROFILE_FRAME_RATE = 999.0;
	float pass1 = static_cast<float>( ( time[1] - time[0] ) * MILLISECONDS_PER_SECOND );
	float pass2 = static_cast<float>( ( time[2] - time[1] ) * MILLISECONDS_PER_SECOND );
	float pass3 = static_cast<float>( ( time[3] - time[2] ) * MILLISECONDS_PER_SECOND );
	float pass4 = static_cast<float>( ( time[4] - time[3] ) * MILLISECONDS_PER_SECOND );
	float pass5 = static_cast<float>( ( time[5] - time[4] ) * MILLISECONDS_PER_SECOND );
	double frameTime = static_cast<float>( pass1 + pass2 + pass3 + pass4 + pass5 ) / MILLISECONDS_PER_SECOND;
	double fps = frameTime >= MINIMUM_PROFILE_FRAME_TIME ? 1.0 / frameTime : MAXIMUM_PROFILE_FRAME_RATE;
	if ( host_speeds.value != 0.0f )
	{
		int activeEntities = 0;
		for ( int i = 0; i < sv.num_edicts; ++i )
		{
			if ( !sv.edicts[i].free )
				++activeEntities;
		}
		Con_Printf( "%3i fps -- host(%3.0f) sv(%3.0f) cl(%3.0f) gfx(%3.0f) snd(%3.0f) ents(%d)\n",
			static_cast<int>( fps ), static_cast<double>( pass1 ), static_cast<double>( pass2 ),
			static_cast<double>( pass3 ), static_cast<double>( pass4 ), static_cast<double>( pass5 ), activeEntities );
	}
	if ( cl_gamegauge.value != 0.0f )
		CL_GGSpeeds( static_cast<float>( time[3] ) );
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4611 )
#endif
void _Host_Frame( float time )
{
	constexpr double CURRENT_FRAME_WEIGHT = 0.4;
	constexpr double PREVIOUS_FRAME_WEIGHT = 0.6;
	constexpr int DISCONNECTED_PAUSE_FRAMES = 5;
	constexpr int SUBSTATE_GAME_ACTIVE = 1 << 2;
	constexpr int ENGINE_PAUSED = 2;
	constexpr int AUDIO_SIGNON_STATE = 2;
	constexpr int HOST_TIMING_MARKS = 6;
	static double host_times[HOST_TIMING_MARKS];
	static int frames = DISCONNECTED_PAUSE_FRAMES;
	if ( setjmp( host_enddemo ) )
		return;			// something bad happened, or the server disconnected
	NullDst();
// decide the simulation time
	if ( !Host_FilterTime( time ) )
		return;			// don't run too fast, or packets will flood out
	SystemWrapper_RunFrame( host_frametime );
	if ( g_modfuncs.m_pfnFrameBegin )
		g_modfuncs.m_pfnFrameBegin();
	rolling_fps = host_frametime * CURRENT_FRAME_WEIGHT + rolling_fps * PREVIOUS_FRAME_WEIGHT;
	NullDst();
	CL_CheckClientState();
// process console commands
	Cbuf_Execute();
	ClientDLL_UpdateClientData();
	if ( sv.active )
		CL_Move();
	host_times[1] = Sys_FloatTime();
	SV_Frame();
	host_times[2] = Sys_FloatTime();
	SV_CheckForRcon();
	if ( !sv.active )
		CL_Move();
	ClientDLL_Frame( host_frametime );
	CL_SetLastUpdate();
	CL_ReadPackets();
	CL_RedoPrediction();
	Voice_Idle( static_cast<float>( host_frametime ) );
	CL_EmitEntities();
	CL_CheckForResend();
	while ( CL_RequestMissingResources() )
	{
	}
	CL_UpdateSoundFade();
	if ( cls.state == ca_disconnected && !*scr_connectmsg.string
		 && ( ( giSubState & SUBSTATE_GAME_ACTIVE ) || console.value == 0.0f ) )
	{
		if ( frames-- <= 0 )
		{
			giActive = ENGINE_PAUSED;
			frames = DISCONNECTED_PAUSE_FRAMES;
		}
	}
	else
	{
		frames = DISCONNECTED_PAUSE_FRAMES;
	}
	CL_HTTPUpdate();
	SteamAPI_RunCallbacks();
	ClientDLL_CAM_Think();
	CL_MoveSpectatorCamera();
	host_times[3] = Sys_FloatTime();
	if ( !gfBackground )
	{
		SCR_UpdateScreen();
		if ( cl_inmovie && scr_con_current == 0.0f )
			VID_WriteBuffer( nullptr );
	}
	host_times[4] = Sys_FloatTime();
	CL_DecayLights();
	if ( !gfBackground )
	{
		if ( cls.signon == AUDIO_SIGNON_STATE )
		{
			vec3_t forward;
			vec3_t right;
			vec3_t up;
			AngleVectors( r_playerViewportAngles, forward, right, up );
			S_Update( r_soundOrigin, forward, right, up );
		}
		else
		{
			S_Update( vec3_origin, vec3_origin, vec3_origin, vec3_origin );
		}
		S_PrintStats();
	}
	host_times[0] = host_times[5];
	host_times[5] = Sys_FloatTime();
	Host_Speeds( host_times );
	++host_framecount;
	CL_AdjustClock();
	if ( sv_stats.value == 1.0f )
		Host_UpdateStats();
	if ( host_killtime.value != 0.0f && sv.time > host_killtime.value )
		Host_Quit_f();
}

int Host_Frame( float time, int iState, int* stateInfo )
{
	constexpr int ENGINE_CLOSE = 3;
	constexpr int PROFILE_SAMPLE_FRAMES = 1000;
	constexpr double MILLISECONDS_PER_SECOND = 1000.0;
	static double timetotal;
	static int timecount;
	if ( setjmp( host_abortserver ) )
		return giActive;
	if ( giActive != ENGINE_CLOSE || !g_iQuitCommandIssued )
		giActive = iState;
	*stateInfo = 0;
	// Commands executed during the frame can change host_profile, so sample it once.
	const bool profileFrame = host_profile.value != 0.0f;
	double time1 = 0.0;
	double time2 = 0.0;
	if ( profileFrame )
		time1 = Sys_FloatTime();
	_Host_Frame( time );
	if ( profileFrame )
		time2 = Sys_FloatTime();
	if ( giStateInfo )
	{
		*stateInfo = giStateInfo;
		giStateInfo = 0;
		Cbuf_Execute();
	}
	if ( profileFrame )
	{
		timetotal += time2 - time1;
		++timecount;
		if ( timecount >= PROFILE_SAMPLE_FRAMES )
		{
			double averageMilliseconds = timetotal * MILLISECONDS_PER_SECOND / timecount;
			timecount = 0;
			timetotal = 0.0;
			int activeClients = 0;
			for ( int i = 0; i < svs.maxclients; ++i )
			{
				if ( svs.clients[i].active )
					++activeClients;
			}
			Con_Printf(
				"host_profile: %2i clients %2i msec\n", activeClients, static_cast<int>( averageMilliseconds ) );
		}
	}
	return giActive;
}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif

void SV_DropClient( client_t* client, qboolean crash, const char* format, ... )
{
	constexpr int DROPPED_CLIENT_REASON_BYTES = 1024;
	constexpr int DISCONNECT_PACKET_BYTES = 512;
	constexpr float MINIMUM_SESSION_SECONDS = 60.0f;
	char reason[DROPPED_CLIENT_REASON_BYTES];
	byte finalMessage[DISCONNECT_PACKET_BYTES];
	int finalLength = 0;
	va_list arguments;
	va_start( arguments, format );
	_vsnprintf( reason, sizeof( reason ), format, arguments );
	va_end( arguments );
	if ( !crash )
	{
		if ( !client->fakeclient )
		{
			MSG_WriteByte( &client->netchan.message, svc_disconnect );
			MSG_WriteString( &client->netchan.message, reason );
			finalMessage[0] = svc_disconnect;
			std::size_t copyBytes = std::strlen( reason ) + 1;
			if ( copyBytes > sizeof( finalMessage ) - 1 )
				copyBytes = sizeof( finalMessage ) - 1;
			std::strncpy( reinterpret_cast<char*>( finalMessage + 1 ), reason, copyBytes );
			finalMessage[sizeof( finalMessage ) - 1] = 0;
			std::size_t reasonLength = std::strlen( reason );
			finalLength = sizeof( finalMessage );
			if ( reasonLength + 1 <= sizeof( finalMessage ) - 1 )
				finalLength = static_cast<int>( reasonLength + 2 );
		}
		if ( client->edict && client->spawned )
			gEntityInterface.pfnClientDisconnect( client->edict );
		Sys_Printf( const_cast<char*>( "Dropped %s from server\nReason:  %s\n" ), client->name, reason );
		if ( !client->fakeclient )
			Netchan_Transmit( &client->netchan, finalLength, finalMessage );
	}
	float connectionTime = static_cast<float>( realtime - client->netchan.connect_time );
	if ( connectionTime > MINIMUM_SESSION_SECONDS )
	{
		++svs.stats.num_sessions;
		svs.stats.cumulative_sessiontime = connectionTime + svs.stats.cumulative_sessiontime;
	}
	NET_CheckCleanupFakeIPConnection( client->netchan.sock, &client->netchan.remote_address );
	Netchan_Clear( &client->netchan );
	Steam_NotifyClientDisconnect( client );
	if ( client->edict )
		++client->edict->serialnumber;
	client->active = 0;
	client->connected = 0;
	client->hasusrmsgs = 0;
	client->fakeclient = 0;
	client->spawned = 0;
	client->fully_connected = 0;
	client->name[0] = 0;
	client->connection_started = realtime;
	client->proxy = 0;
	COM_ClearCustomizationList( &client->customdata, 0 );
	client->edict = nullptr;
	Q_memset( client->userinfo, 0, sizeof( client->userinfo ) );
	Q_memset( client->physinfo, 0, sizeof( client->physinfo ) );
	SV_FullClientUpdate( client, &sv.reliable_datagram );
	NotifyDedicatedServerUI( "UpdatePlayers" );
}

void SV_ClearClientStates()
{
	client_t* client = svs.clients;
	for ( int i = 0; i < svs.maxclients; ++i, ++client )
	{
		COM_ClearCustomizationList( &client->customdata, 0 );
		SV_ClearResourceLists( client );
	}
}

void Host_ClearClients( qboolean bFramesOnly )
{
	host_client = svs.clients;
	for ( int i = 0; i < svs.maxclients; ++i, ++host_client )
	{
		if ( host_client->frames )
		{
			for ( int j = 0; j < SV_UPDATE_BACKUP; ++j )
			{
				client_frame_t* frame = &host_client->frames[j];
				SV_ClearPacketEntities( frame );
				frame->senttime = 0.0;
				frame->ping_time = -1.0f;
			}
		}
		if ( host_client->netchan.remote_address.type != NA_UNUSED )
		{
			netadr_t address = host_client->netchan.remote_address;
			Q_memset( &host_client->netchan, 0, sizeof( host_client->netchan ) );
			Netchan_Setup( NS_SERVER, &host_client->netchan, address, static_cast<int>( host_client - svs.clients ),
				&host_client, SV_GetFragmentSize );
		}
		COM_ClearCustomizationList( &host_client->customdata, 0 );
	}
	if ( !bFramesOnly )
	{
		host_client = svs.clients;
		for ( int i = 0; i < svs.maxclientslimit; ++i, ++host_client )
			SV_ClearFrames( &host_client->frames );
		Q_memset( svs.clients, 0, svs.maxclientslimit * sizeof( client_t ) );
		SV_AllocClientFrames();
	}
}

void Host_ShutdownServer( qboolean crash )
{
	if ( !sv.active )
		return;
	SV_ServerShutdown();
	sv.active = 0;
	NET_ClearLagData( 1, 1 );
	host_client = svs.clients;
	for ( int i = 0; i < svs.maxclients; ++i, ++host_client )
	{
		if ( host_client->active || host_client->connected )
			SV_DropClient( host_client, crash, "Server shutting down" );
	}
	CL_Disconnect();
	SV_ClearEntities();
	SV_ClearCaches();
	FreeAllEntPrivateData();
	Q_memset( &sv, 0, sizeof( sv ) );
	CL_ClearClientState();
	SV_ClearClientStates();
	Host_ClearClients( 0 );
	host_client = svs.clients;
	for ( int i = 0; i < svs.maxclientslimit; ++i, ++host_client )
		SV_ClearFrames( &host_client->frames );
	Q_memset( svs.clients, 0, svs.maxclientslimit * sizeof( client_t ) );
	HPAK_FlushHostQueue();
	Steam_Shutdown();
	Log_Printf( "Server shutdown\n" );
	Log_Close();
	Steam_UpdateRichPresence();
}

namespace
{
constexpr int MAX_VERSION_KEYS = 2;
constexpr int CONFIG_INFO_BUFFER_BYTES = 512;
constexpr int CONFIG_FILENAME_FORMAT_BYTES = 257;
const char PATCH_VERSION_KEY[] = "PatchVersion=";
const char PRODUCT_NAME_KEY[] = "ProductName=";
}

/*
=======================
Host_InitLocal
======================
*/
void Host_InitLocal()
{
	Host_InitCommands();
	Cvar_RegisterVariable( &host_killtime );
	Cvar_RegisterVariable( &sys_ticrate );
	Cvar_RegisterVariable( &fps_max );
	Cvar_RegisterVariable( &fps_override );
	Cvar_RegisterVariable( &host_name );
	Cvar_RegisterVariable( &host_limitlocal );
	sys_timescale.value = 1.0f;
	Cvar_RegisterVariable( &host_framerate );
	Cvar_RegisterVariable( &host_speeds );
	Cvar_RegisterVariable( &host_profile );
	Cvar_RegisterVariable( &mp_logfile );
	Cvar_RegisterVariable( &mp_logecho );
	Cvar_RegisterVariable( &sv_log_onefile );
	Cvar_RegisterVariable( &sv_log_singleplayer );
	Cvar_RegisterVariable( &sv_logsecret );
	Cvar_RegisterVariable( &sv_stats );
	Cvar_RegisterVariable( &developer );
	Cvar_RegisterVariable( &deathmatch );
	Cvar_RegisterVariable( &coop );
	Cvar_RegisterVariable( &pausable );
	Cvar_RegisterVariable( &skill );
	SV_SetMaxclients();
}

int Host_Init( quakeparms_t* parms )
{
	constexpr int VERSION_BUFFER_BYTES = 256;
	constexpr int PALETTE_COLOR_COUNT = 256;
	constexpr int PALETTE_COMPONENT_COUNT = 4;
	constexpr int DISK_PALETTE_COMPONENT_COUNT = 3;
	constexpr int REGISTRY_BUFFER_BYTES = 128;
	constexpr int ENGINE_ACTIVE = 1;
	constexpr double BYTES_PER_MEGABYTE = 1048576.0;
	const char REGISTRY_PRODUCT[] = "Half-Life";

	std::srand( static_cast<unsigned int>( std::time( nullptr ) ) );
	host_parms = *parms;
	com_argc = parms->argc;
	com_argv = parms->argv;
	realtime = 0.0;
	Memory_Init( parms->membase, parms->memsize );
	Voice_RegisterCvars();
	Cvar_RegisterVariable( &console );
	if ( COM_CheckParm( "-console" ) || COM_CheckParm( "-toconsole" ) || COM_CheckParm( "-dev" ) )
		Cvar_DirectSet( &console, "1.0" );
	Host_InitLocal();
	if ( COM_CheckParm( "-dev" ) )
		Cvar_SetValue( "developer", 1.0f );
	Cbuf_Init();
	Cmd_Init();
	Cvar_Init();
	Cvar_CmdInit();
	V_Init();
	Chase_Init();
	COM_Init( parms->basedir );
	Host_ClearSaveDirectory();
	HPAK_Init();
	W_LoadWadFile( const_cast<char*>( "gfx.wad" ) );
	W_LoadWadFile( const_cast<char*>( "fonts.wad" ) );
	Key_Init();
	Con_Init();
	Decal_Init();
	Mod_Init();
	NET_Init();
	Netchan_Init();
	DELTA_Init();
	SV_Init();
	SystemWrapper_Init();
	Host_Version();
	char versionString[VERSION_BUFFER_BYTES];
	_snprintf( 
		versionString, sizeof( versionString ), "%s,%i,%i", gpszVersionString, PROTOCOL_VERSION, build_number() );
	Cvar_Set( "sv_version", versionString );
	Con_DPrintf( "%4.1f Mb heap\n", static_cast<double>( parms->memsize ) / BYTES_PER_MEGABYTE );
	R_InitTextures(); // needed even for dedicated servers
	HPAK_CheckIntegrity( const_cast<char*>( "custom" ) );
	Q_memset( &g_module, 0, sizeof( g_module ) );

	if ( cls.state != ca_dedicated )
	{
		char* diskPalette = reinterpret_cast<char*>( COM_LoadHunkFile( const_cast<char*>( "gfx/palette.lmp" ) ) );
		if ( !diskPalette )
			Sys_Error( "Host_Init: Couldn't load gfx/palette.lmp" );
		host_basepal = static_cast<unsigned short*>(
			Hunk_AllocName( PALETTE_COLOR_COUNT * PALETTE_COMPONENT_COUNT * sizeof( *host_basepal ), "palette.lmp" ) );
		for ( int i = 0; i < PALETTE_COLOR_COUNT; ++i )
		{
			host_basepal[i * PALETTE_COMPONENT_COUNT] = diskPalette[i * DISK_PALETTE_COMPONENT_COUNT + 2];
			host_basepal[i * PALETTE_COMPONENT_COUNT + 1] = diskPalette[i * DISK_PALETTE_COMPONENT_COUNT + 1];
			host_basepal[i * PALETTE_COMPONENT_COUNT + 2] = diskPalette[i * DISK_PALETTE_COMPONENT_COUNT];
			host_basepal[i * PALETTE_COMPONENT_COUNT + 3] = 0;
		}
#if !defined( SW_RENDERER )
		GL_Init();
#endif
		PM_Init( &g_clmove );
		CL_InitEventSystem();
		ClientDLL_Init();
		VGui_Startup();
		if ( !VID_Init( host_basepal ) )
		{
			VGui_Shutdown();
			return 0;
		}
		Draw_Init();
		SCR_Init();
		R_Init();
		S_Init();
		CDAudio_Init();
		Voice_Init( "voice_speex", 1 );
		DemoPlayer_Init();
		CL_Init();
	}
	else
	{
		Cvar_RegisterVariable( &suitvolume );
	}

	Cbuf_InsertText( "exec controller.cfg\n" );
	Cbuf_InsertText( "exec valve.rc\n" );
#if !defined( SW_RENDERER )
	if ( cls.state != ca_dedicated )
		GL_Config();
#endif
	// Mark hunklevel at end of startup
	Hunk_AllocName( 0, "-HOST_HUNKLEVEL-" );
	host_hunklevel = Hunk_LowMark();
	giActive = ENGINE_ACTIVE;
	scr_skipupdate = 0;

	char data[REGISTRY_BUFFER_BYTES];
	char keyPath[REGISTRY_BUFFER_BYTES];
	Q_memset( data, 0, sizeof( data ) );
	_snprintf( keyPath, sizeof( keyPath ), "Software\\Valve\\%s\\Settings", REGISTRY_PRODUCT );
	Q_strcpy( data, com_empty_string );
	Sys_GetRegKeyValue( keyPath, "User Token 2", data, sizeof( data ) - 1, data );
	bool lowViolence = Q_strlen( data ) > 0;
	if ( !lowViolence )
	{
		Sys_GetRegKeyValue( keyPath, "User Token 3", data, sizeof( data ) - 1, data );
		lowViolence = Q_strlen( data ) > 0;
	}
	_snprintf( keyPath, sizeof( keyPath ), "Software\\Valve\\%s\\%s\\Settings", REGISTRY_PRODUCT, com_gamedir );
	Q_strcpy( data, com_empty_string );
	Sys_GetRegKeyValue( keyPath, "User Token 2", data, sizeof( data ) - 1, data );
	if ( Q_strlen( data ) > 0 )
		lowViolence = true;
	Sys_GetRegKeyValue( keyPath, "User Token 3", data, sizeof( data ) - 1, data );
	if ( Q_strlen( data ) > 0 )
		lowViolence = true;
	float violence = lowViolence || bLowViolenceBuild ? 0.0f : 1.0f;
	Cvar_SetValue( "violence_hblood", violence );
	Cvar_SetValue( "violence_hgibs", violence );
	Cvar_SetValue( "violence_ablood", violence );
	Cvar_SetValue( "violence_agibs", violence );
	host_initialized = 1;
	Steam_ProcessInsecureLaunchParameters();
	return 1;
}

qboolean Host_IsServerActive()
{
	return sv.active;
}

qboolean Host_IsSinglePlayerGame()
{
	return ( sv.active ? svs.maxclients : cl.maxclients ) == 1;
}

void Host_Version()
{
	Q_strcpy( gpszVersionString, "1.0.1.4" );
	Q_strcpy( gpszProductString, "valve" );

	char filename[GAME_DIRECTORY_BYTES];
	sprintf( filename, "steam.inf" );
	FileHandle_t file = FS_Open( filename, "r" );
	if ( file )
	{
		unsigned int fileSize = FS_Size( file );
		char* buffer = fileSize < INT_MAX ? static_cast<char*>( Mem_Malloc( fileSize + 1 ) ) : NULL;
		int bytesRead = buffer ? FS_Read( buffer, static_cast<int>( fileSize ), 1, file ) : 0;
		FS_Close( file );
		// text mode reads fewer bytes than the file size when it turns CRLF into LF
		if ( buffer )
			buffer[bytesRead > 0 ? bytesRead : 0] = 0;

		int matchedKeys = 0;
		for ( const char* cursor = buffer ? COM_Parse( buffer ) : NULL; cursor; cursor = COM_Parse( cursor ) )
		{
			int tokenLength = Q_strlen( com_token );
			if ( tokenLength <= 0 || matchedKeys >= MAX_VERSION_KEYS )
				break;

			if ( Q_strnicmp( com_token, PATCH_VERSION_KEY, Q_strlen( PATCH_VERSION_KEY ) ) == 0 )
			{
				Q_strncpy( gpszVersionString, com_token + Q_strlen( PATCH_VERSION_KEY ), VERSION_STRING_BYTES - 1 );
				gpszVersionString[VERSION_STRING_BYTES - 1] = 0;

				if ( COM_CheckParm( "-steam" ) )
				{
					char steamVersion[VERSION_STRING_BYTES];
					FS_GetInterfaceVersion( steamVersion, VERSION_STRING_BYTES - 1 );
					_snprintf( gpszVersionString, VERSION_STRING_BYTES, "%s/%s",
						com_token + Q_strlen( PATCH_VERSION_KEY ), steamVersion );
					gpszVersionString[VERSION_STRING_BYTES - 1] = 0;
				}
				++matchedKeys;
			}
			else if ( Q_strnicmp( com_token, PRODUCT_NAME_KEY, Q_strlen( PRODUCT_NAME_KEY ) ) == 0 )
			{
				Q_strncpy( gpszProductString, com_token + Q_strlen( PRODUCT_NAME_KEY ), VERSION_STRING_BYTES - 1 );
				gpszProductString[VERSION_STRING_BYTES - 1] = 0;
				++matchedKeys;
			}
		}

		if ( buffer )
			std::free( buffer );
	}

	if ( cls.state != ca_dedicated )
	{
		Con_DPrintf(
			"Protocol version %i\nExe version %s (%s)\n", PROTOCOL_VERSION, gpszVersionString, gpszProductString );
		Con_DPrintf( "Exe build: " ENGINE_BUILD_TIME " Oct  7 2024 (%i)\n", build_number() );
	}
	else
	{
		Con_Printf(
			"Protocol version %i\nExe version %s (%s)\n", PROTOCOL_VERSION, gpszVersionString, gpszProductString );
		Con_Printf( "Exe build: " ENGINE_BUILD_TIME " Oct  7 2024 (%i)\n", build_number() );
	}
}

void Info_WriteVars( FileHandle_t file )
{
	static char values[INFO_VALUE_BUFFERS][CONFIG_INFO_BUFFER_BYTES];
	static int valueIndex;
	valueIndex = ( valueIndex + 1 ) % INFO_VALUE_BUFFERS;

	const char* info = cls.userinfo;
	if ( *info == '\\' )
		++info;

	for ( ;; )
	{
		char key[CONFIG_INFO_BUFFER_BYTES];
		char* output = key;
		while ( *info != '\\' )
		{
			if ( !*info )
				return;
			*output++ = *info++;
		}
		*output = 0;
		++info;

		output = values[valueIndex];
		while ( *info != '\\' )
		{
			if ( !*info )
				break;
			*output++ = *info++;
		}
		*output = 0;

		if ( !Cvar_FindVar( key ) && key[0] != '*' )
			FS_FPrintf( file, "setinfo \"%s\" \"%s\"\n", key, values[valueIndex] );
		if ( !*info )
			return;
		++info;
	}
}

void Host_WriteConfiguration()
{
	if ( !host_initialized || cls.state == ca_dedicated )
		return;

#if defined( _WIN32 )
	Sys_SetRegKeyValue( "Software\\Valve\\Steam", "Rate", rate.string );

	if ( cl_name.string && Q_strcasecmp( cl_name.string, "unnamed" ) != 0
		 && Q_strcasecmp( cl_name.string, "player" ) != 0 && Q_strlen( cl_name.string ) != 0 )
	{
		Sys_SetRegKeyValue( "Software\\Valve\\Steam", "LastGameNameUsed", cl_name.string );
	}
#else
	Sys_SetRegKeyValue( "Software\\Valve\\Steam", "rate", rate.string );
#endif

	if ( Key_CountBindings() <= 1 )
	{
		Con_Printf( "skipping config.cfg output, no keys bound\n" );
		return;
	}

	qboolean restoreReadOnly = 0;
	char filename[GAME_DIRECTORY_BYTES];
	FileHandle_t file = FS_OpenPathID( "config.cfg", "w", "GAMECONFIG" );
	if ( !file && developer.value != 0.0f && FS_FileExists( "../goldsrc/dev_build_all.bat" ) )
	{
		FS_GetLocalPath( "config.cfg", filename, sizeof( filename ) );
#ifdef _MSC_VER
#pragma warning( suppress: 6031 )
#endif
		_chmod( filename, _S_IREAD | _S_IWRITE );
		file = FS_OpenPathID( "config.cfg", "w", "GAMECONFIG" );
		restoreReadOnly = 1;
	}
	if ( !file )
	{
		Con_Printf( "Couldn't write config.cfg.\n" );
		return;
	}

	FS_FPrintf( file, "// This file is overwritten whenever you change your user settings in the game.\n" );
	FS_FPrintf( file, "// Add custom configurations to the file \"userconfig.cfg\".\n\n" );
	FS_FPrintf( file, "unbindall\n" );
	Key_WriteBindings( file );
	Cvar_WriteVariables( file );
	Info_WriteVars( file );
	FS_FPrintf( file, "+mlook\n" );
	FS_FPrintf( file, "+jlook\n" );
	FS_FPrintf( file, "exec controller.cfg\n" );
	FS_FPrintf( file, "exec userconfig.cfg\n" );
	FS_Close( file );

	if ( restoreReadOnly )
	{
		FS_GetLocalPath( "config.cfg", filename, sizeof( filename ) );
#ifdef _MSC_VER
#pragma warning( suppress: 6031 )
#endif
		_chmod( filename, _S_IREAD );
	}
}

void Host_WriteCustomConfig()
{
	if ( Cmd_Argc() == 1 || !Cmd_Args() || !Cmd_Args()[0] )
	{
		Con_Printf( "writecfg: writecfg <filename>\n" );
		return;
	}

	char filename[GAME_DIRECTORY_BYTES + 1];
	_snprintf( filename, CONFIG_FILENAME_FORMAT_BYTES, "%s", Cmd_Args() );
	if ( !filename[0] || filename[0] == '/' || Q_strstr( filename, ".." ) || Q_strstr( filename, "\\" )
		 || Q_strstr( filename, ":" ) || Q_strstr( filename, "./" ) )
	{
		Con_Printf(
			"skipping writecfg output, invalid filename %s given (invalid characters in filename)\n", filename );
		return;
	}
	if ( !host_initialized || cls.state == ca_dedicated )
		return;
	if ( Key_CountBindings() <= 1 )
	{
		Con_Printf( "skipping config.cfg output, no keys bound\n" );
		return;
	}

	COM_DefaultExtension( filename, const_cast<char*>( ".cfg" ) );
	if ( Q_strcasecmp( COM_FileExtension( filename ), "cfg" ) != 0 )
	{
		Con_Printf(
			"skipping writecfg output, invalid filename %s given (wrong file extension, must be .cfg)\n", filename );
		return;
	}
	if ( Q_strcasecmp( filename, "config.cfg" ) == 0 || Q_strcasecmp( filename, "autoexec.cfg" ) == 0
		 || Q_strcasecmp( filename, "listenserver.cfg" ) == 0 || Q_strcasecmp( filename, "server.cfg" ) == 0
		 || Q_strcasecmp( filename, "userconfig.cfg" ) == 0 || Q_strcasecmp( filename, "language.cfg" ) == 0
		 || Q_strcasecmp( filename, "violence.cfg" ) == 0 || Q_strcasecmp( filename, "banned.cfg" ) == 0
		 || Q_strcasecmp( filename, "listip.cfg" ) == 0 || Q_strcasecmp( filename, "skill.cfg" ) == 0
		 || Q_strcasecmp( filename, "game.cfg" ) == 0 || Q_strcasecmp( filename, "joystick.cfg" ) == 0
		 || Q_striprefix( filename, "hw/" ) )
	{
		Con_Printf( "skipping writecfg output, invalid filename %s given\n", filename );
		return;
	}

	FileHandle_t file = FS_OpenPathID( filename, "w", "GAMECONFIG" );
	if ( !file )
	{
		Con_Printf( "Couldn't write %s.\n", filename );
		return;
	}
	FS_FPrintf( file, "unbindall\n" );
	Key_WriteBindings( file );
	Cvar_WriteVariables( file );
	Info_WriteVars( file );
	kbutton_t* mlook = ClientDLL_FindKey( "in_mlook" );
	kbutton_t* jlook = ClientDLL_FindKey( "in_jlook" );
	if ( mlook && ( mlook->state & BUTTON_DOWN ) )
		FS_FPrintf( file, "+mlook\n" );
	if ( jlook && ( jlook->state & BUTTON_DOWN ) )
		FS_FPrintf( file, "+jlook\n" );
	FS_Close( file );
	Con_Printf( "%s successfully created!\n", filename );
}

/*
=================
SV_ClientPrintf

Sends text across to be displayed 
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf( const char* format, ... )
{
	constexpr int CLIENT_PRINT_BYTES = 1024;
	char text[CLIENT_PRINT_BYTES];
	va_list arguments;
	va_start( arguments, format );
	if ( !host_client->fakeclient )
	{
		_vsnprintf( text, sizeof( text ), format, arguments );
		MSG_WriteByte( &host_client->netchan.message, svc_print );
		MSG_WriteString( &host_client->netchan.message, text );
	}
	va_end( arguments );
}

/*
================
Host_EndGame
================
*/
void Host_EndGame( const char* format, ... )
{
	constexpr int HOST_ERROR_MESSAGE_BYTES = 1024;
	char message[HOST_ERROR_MESSAGE_BYTES];
	va_list arguments;
	va_start( arguments, format );
	_vsnprintf( message, sizeof( message ), format, arguments );
	va_end( arguments );
	Con_DPrintf( "Host_EndGame: %s\n", message );

	int demoNumber = cls.demonum;
	if ( sv.active )
		Host_ShutdownServer( 0 );
	cls.demonum = demoNumber;
	if ( cls.state == ca_dedicated )
		Sys_Error( "Host_EndGame: %s\n", message ); // dedicated servers exit

	if ( demoNumber != -1 )
	{
		CL_Disconnect_f();
		cls.demonum = demoNumber;
		Host_NextDemo();
		longjmp( host_enddemo, 1 );
	}

	CL_Disconnect();
	Cbuf_AddText( "cd stop\n" );
	Cbuf_Execute();
	longjmp( host_abortserver, 1 );
}

[[noreturn]] void Host_Error( const char* format, ... )
{
	constexpr int HOST_ERROR_MESSAGE_BYTES = 1024;
	static qboolean inerror;
	if ( inerror )
		Sys_Error( "Host_Error: recursively entered" );
	inerror = 1;
	SCR_EndLoadingPlaque();

	char message[HOST_ERROR_MESSAGE_BYTES];
	va_list arguments;
	va_start( arguments, format );
	_vsnprintf( message, sizeof( message ), format, arguments );
	va_end( arguments );
	if ( !sv.active && developer.value != 0.0f )
		CL_WriteMessageHistory( 0, 0 );
	Con_Printf( "Host_Error: %s\n", message );
	if ( sv.active )
		Host_ShutdownServer( 0 );
	if ( cls.state == ca_dedicated )
		Sys_Error( "Host_Error: %s\n", message );

	CL_Disconnect();
	cls.demonum = -1;
	inerror = 0;
	longjmp( host_abortserver, 1 );
}

void Host_ClientCommands( char* format, ... )
{
	constexpr int CLIENT_COMMAND_BYTES = 1024;
	char text[CLIENT_COMMAND_BYTES];
	va_list arguments;
	va_start( arguments, format );
	if ( !host_client->fakeclient )
	{
		_vsnprintf( text, sizeof( text ), format, arguments );
		text[sizeof( text ) - 1] = '\0';
		MSG_WriteByte( &host_client->netchan.message, svc_stufftext );
		MSG_WriteString( &host_client->netchan.message, text );
	}
	va_end( arguments );
}

qboolean Master_IsLanGame()
{
	extern cvar_t sv_lan;
	return sv_lan.value != 0.0;
}

int Host_GetMaxClients()
{
	return sv.active ? svs.maxclients : cl.maxclients;
}

void Host_ClearMemory( qboolean quiet )
{
	CM_FreePAS();
	SV_ClearEntities();
	if ( !quiet )
		Con_DPrintf( "Clearing memory\n" );
	D_FlushCaches();
	Mod_ClearAll();
	int mark = host_hunklevel;
	if ( mark )
	{
		client_t* client = svs.clients;
		if ( client )
		{
			int clientLimit = svs.maxclientslimit;
			if ( clientLimit > 0 )
			{
				for ( int index = 0; index < clientLimit && client; ++index, ++client )
				{
					if ( client->frames )
					{
						SV_ClearFrames( &client->frames );
						clientLimit = svs.maxclientslimit;
					}
				}
				mark = host_hunklevel;
			}
		}
		Hunk_FreeToLowMark( mark );
	}
	cls.signon = 0;
	SV_ClearCaches();
	Q_memset( &sv, 0, sizeof( sv ) );
	CL_ClearClientState();
	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		COM_ClearCustomizationList( &client->customdata, false );
		SV_ClearResourceLists( client );
	}
}

void SV_BroadcastPrintf( const char* format, ... )
{
	constexpr int BROADCAST_PRINT_BYTES = 1024;
	char text[BROADCAST_PRINT_BYTES];
	va_list arguments;
	va_start( arguments, format );
	_vsnprintf( text, sizeof( text ), format, arguments );
	va_end( arguments );

	int clientLimit = svs.maxclients;
	for ( int index = 0; index < clientLimit; ++index )
	{
		client_t* client = &svs.clients[index];
		if ( ( client->active || client->spawned ) && !client->fakeclient )
		{
			MSG_WriteByte( &client->netchan.message, svc_print );
			MSG_WriteString( &client->netchan.message, text );
			clientLimit = svs.maxclients;
		}
	}
	Con_DPrintf( "%s", text );
}

void Host_CheckDyanmicStructures()
{
	client_t* client = svs.clients;
	if ( client == nullptr )
		return;

	int clientLimit = svs.maxclientslimit;
	if ( clientLimit <= 0 )
		return;

	for ( int index = 0; client != nullptr && index < clientLimit; )
	{
		if ( client->frames != nullptr )
		{
			SV_ClearFrames( &client->frames );
			clientLimit = svs.maxclientslimit;
		}
		++index;
		++client;
	}
}

void Master_Heartbeat_f()
{
}

void Host_ComputeFPS( double frametime )
{
	rolling_fps = 0.6 * rolling_fps + 0.4 * frametime;
}

/*
==================
Host_GetHostInfo
==================
*/
void Host_GetHostInfo( float* fps, int* nActive, int* unused, int* nMaxPlayers, char* pszMap )
{
	int clients = 0;
	if ( rolling_fps <= 0.0 )
	{
		rolling_fps = 0.0;
		*fps = 0.0f;
	}
	else
	{
		*fps = static_cast<float>( 1.0 / static_cast<float>( rolling_fps ) );
	}

	// Count clients, report 
	SV_CountPlayers( &clients );
	*nActive = clients;
	if ( unused != nullptr )
		*unused = 0;
	if ( pszMap != nullptr )
	{
		if ( sv.name[0] != 0 )
			Q_strcpy( pszMap, sv.name );
		else
			*pszMap = 0;
	}
	*nMaxPlayers = svs.maxclients;
}

void Host_UpdateScreen()
{
	if ( gfBackground )
		return;

	SCR_UpdateScreen();
	if ( cl_inmovie && scr_con_current == 0.0f )
		VID_WriteBuffer( nullptr );
}

void CL_AddVoiceToDatagram( qboolean bFinal )
{
	constexpr int VOICE_DATA_BYTES = 2048;
	constexpr int VOICE_PACKET_HEADER_BYTES = 3;
	if ( cls.state != ca_active || !Voice_IsRecording() )
		return;

	char voiceData[VOICE_DATA_BYTES];
	int length = Voice_GetCompressedData( voiceData, VOICE_DATA_BYTES, bFinal );
	if ( length == 0 || length + cls.datagram.cursize + VOICE_PACKET_HEADER_BYTES >= cls.datagram.maxsize )
		return;

	MSG_WriteByte( &cls.datagram, clc_voicedata );
	MSG_WriteShort( &cls.datagram, length );
	MSG_WriteBuf( &cls.datagram, length, voiceData );
}

void CL_VoiceIdle()
{
	Voice_Idle( static_cast<float>( host_frametime ) );
}

void Host_UpdateSounds()
{
	constexpr int AUDIO_SIGNON_STATE = 2;
	if ( gfBackground )
		return;

	if ( cls.signon == AUDIO_SIGNON_STATE )
	{
		vec3_t forward;
		vec3_t right;
		vec3_t up;
		AngleVectors( r_playerViewportAngles, forward, right, up );
		S_Update( r_soundOrigin, forward, right, up );
	}
	else
	{
		S_Update( vec3_origin, vec3_origin, vec3_origin, vec3_origin );
	}
	S_PrintStats();
}

void Host_CheckConnectionFailure()
{
	constexpr int DISCONNECTED_PAUSE_FRAMES = 5;
	constexpr int SUBSTATE_GAME_ACTIVE = 1 << 2;
	constexpr int ENGINE_PAUSED = 2;
	static int frames = DISCONNECTED_PAUSE_FRAMES;
	if ( cls.state != ca_disconnected || *scr_connectmsg.string != 0
		 || ( ( giSubState & SUBSTATE_GAME_ACTIVE ) == 0 && console.value != 0.0f ) )
	{
		frames = DISCONNECTED_PAUSE_FRAMES;
		return;
	}
	if ( frames-- <= 0 )
	{
		giActive = ENGINE_PAUSED;
		frames = DISCONNECTED_PAUSE_FRAMES;
	}
}

void CheckGore()
{
	constexpr int GORE_TOKEN_BYTES = 128;
	char szBuffer[GORE_TOKEN_BYTES];
	Q_memset( szBuffer, 0, sizeof( szBuffer ) );
	if ( bLowViolenceBuild )
	{
		Cvar_SetValue( "violence_hblood", 0.0f );
		Cvar_SetValue( "violence_hgibs", 0.0f );
		Cvar_SetValue( "violence_ablood", 0.0f );
		Cvar_SetValue( "violence_agibs", 0.0f );
	}
	else
	{
		Cvar_SetValue( "violence_hblood", 1.0f );
		Cvar_SetValue( "violence_hgibs", 1.0f );
		Cvar_SetValue( "violence_ablood", 1.0f );
		Cvar_SetValue( "violence_agibs", 1.0f );
	}
}
