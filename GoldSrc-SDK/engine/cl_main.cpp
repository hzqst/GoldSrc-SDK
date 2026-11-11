// cl_main.c  -- client main loop
#include <winsock2.h>

#include "client.h"
#include "pm_defs.h"
#include "common.h"
#include "cvar.h"
#include "conprint.h"
#include "mem.h"
#include "sys.h"
#include "decals.h"
#include "gl_model.h"
#include "host.h"
#include "protocol.h"
#include "sound.h"
#include "voice.h"
#include "vgui_intwrap2.h"
#include "zone.h"
#include "../common/dlight.h"
#include "render.h"
#include "hashpak.h"
#include "filesystem.h"
#include "crc.h"
#include "cl_demo.h"
#include "server.h"
#include "steam_api.h"
#include "cmd.h"
#include "info.h"
#include "net_api.h"
#include "delta.h"
#include "../common/con_nprint.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define MAX_STARTUP_TIMINGS 32
#define MAX_CACHED_SERVERS 16

#define CL_CONNECTION_RETRIES 4

#define PORT_SERVER 27015
#define PORT_MULTICAST 27025

#define PROTOCOL_HASHEDCDKEY 2
#define PROTOCOL_STEAM 3

#define S2C_CHALLENGE 'A'
#define S2C_CONNECTION 'B'
#define S2C_LISTEN 'G'
#define S2A_INFO_SRC 'I'
#define S2C_REDIRECT 'L'
#define A2A_PRINT 'l'
#define S2C_BADPASSWORD '8'
#define S2C_CONNREJECT '9'

#define S2A_EXTRA_DATA_HAS_GAME_PORT 0x80
#define S2A_EXTRA_DATA_HAS_SPECTATOR_DATA 0x40
#define S2A_EXTRA_DATA_HAS_GAMETAG_DATA 0x20
#define S2A_EXTRA_DATA_HAS_STEAMID 0x10
#define S2A_EXTRA_DATA_GAMEID 0x01

#define MAX_INTERP_MSEC 100

#define MAX_PACKETS_PER_FRAME 250

#define MAX_BACKUP_COMMANDS 8
#define MAX_LEGACY_COMMANDS 16
#define MAX_TOTAL_COMMANDS 62

#define HLTV_COMMENTATOR 3

extern cvar_t cl_gamegauge;
extern cvar_t dev_overview;
extern cvar_t host_framerate;
extern qboolean noip;
extern cvar_t ex_interp;
extern cvar_t host_limitlocal;
extern int r_dlightchanged;
extern int r_dlightactive;

void CL_SendConnectPacket();
void CL_SendRcon( netadr_t* to, unsigned int challenge );
int CL_ClientDllQuery( const char* text );
void CL_PrintCachedServer_0( int slot );
static void CL_ConnectionlessPacket();
void CL_InitTEnts();
void CL_InitExtrap();
void TextMessageInit();
void TextMessageShutdown();
void SCR_EndLoadingPlaque();
void VID_TakeSnapshot( const char* filename );
void VID_WriteBuffer( const char* filename );
short Q_ntohs( short value );
void Sequence_Init();
void CL_GameDir_f();
void CL_FullServerinfo_f();
void CL_ViewDemo_f();
void CL_GameGauge_f();
void CL_ListDemo_f();
void CL_AppendDemo_f();
void CL_RemoveDemo_f();
void CL_SwapDemo_f();
void CL_SetDemoInfo_f();
void CL_Commentator_f();
void CL_TakeSnapshot_f();
void CL_StartMovie_f();
void CL_EndMovie_f();
void CL_PrintEntities_f();
void CL_DumpMessageLoad_f();
void CL_PingServers_f();
void CL_BeginUpload_f();
void CL_HTTPCancel_f();
qboolean CL_CheckGameDirectory( char* gamedir );
void CL_ClearCaches();
void CL_CreateResourceList();
void CL_RegisterCustomization( resource_t* resource );
const char* CL_CleanFileName( const char* filename );
qboolean CL_GetMessage();
void CL_Listen_0();
void CL_Retry_f_0();
void SetupStartupTimings();
void GetPos( vec3_t origin, vec3_t angles );

playermove_t g_clmove;

void CL_GetPlayerHulls()
{
	int i;

	for ( i = 0; i < MAX_PLAYER_HULLS; i++ )
	{
		if ( !ClientDLL_GetHullBounds( i, player_mins[i], player_maxs[i] ) )
			break;
	}
}

client_static_t cls;
client_state_t cl;
lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];

int g_bRedirectedToProxy;
qboolean cl_inmovie;

cvar_t cl_shownet = { "cl_shownet", "0" };
cvar_t cl_showmessages = { "cl_showmessages", "0" };
cvar_t cl_weaponlistfix = { "cl_weaponlistfix", "0" };
cvar_t cl_clockreset = { "cl_clockreset", "0.1" };
cvar_t cl_download_ingame = { "cl_download_ingame", "1", FCVAR_ARCHIVE };
cvar_t cl_filterstuffcmd = { "cl_filterstuffcmd", "0", FCVAR_ARCHIVE | FCVAR_PRIVILEGED };
cvar_t cl_lw = { "cl_lw", "1", FCVAR_ARCHIVE | FCVAR_USERINFO };

int cl_numvisedicts;
cl_entity_t* cl_visedicts[MAX_VISEDICTS];

cvar_t fs_startup_timings = { "fs_startup_timings", "0" };
cvar_t fs_lazy_precache = { "fs_lazy_precache", "0" };
cvar_t fs_precache_timings = { "fs_precache_timings", "0" };

model_t* CL_GetModelByIndex( int index )
{
	model_t* model;
	double start;

	if ( index >= MAX_MODELS )
		return NULL;

	model = cl.model_precache[index];
	if ( !model )
		return NULL;

	if ( model->needload == NL_NEEDS_LOADED || model->needload == NL_UNREFERENCED )
	{
		if ( fs_precache_timings.value )
		{
			start = Sys_FloatTime();
			Mod_LoadModel( model, FALSE, FALSE );
			Con_DPrintf( "fs_precache_timings: loaded model %s in time %.3f sec\n", model->name,
				(float)( Sys_FloatTime() - start ) );
		}
		else
		{
			Mod_LoadModel( model, FALSE, FALSE );
		}
	}

	return cl.model_precache[index];
}

typedef struct
{
	const char* name;
	float time;
} startup_timing_t;

startup_timing_t g_StartupTimings[MAX_STARTUP_TIMINGS];
int g_iCurrentTiming;
cl_entity_t* cl_entities;
int cl_playerindex;
efrag_t cl_efrags[MAX_EFRAGS];
dlight_t cl_dlights[MAX_DLIGHTS];
dlight_t cl_elights[MAX_ELIGHTS];

netadr_t g_GameServerAddress;
float g_LastScreenUpdateTime;

void CL_DeallocateDynamicData()
{
	if ( cl_entities )
	{
		Mem_Free( cl_entities );
		cl_entities = NULL;
	}

	R_DestroyObjects();
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect()
{
	byte final[13];
	int i;

	cls.connect_time = -99999;
	cls.connect_retry = 0;

	g_careerState = CAREER_NONE;
	SetCareerAudioState( FALSE );

	SPR_Shutdown_NoModelFree();

	// stop sounds (especially looping!)
	S_StopAllSounds( TRUE );

	if ( cls.demoplayback )
	{
		CL_StopPlayback();
	}
	else if ( cls.state == ca_active || cls.state == ca_connected || cls.state == ca_uninitialized
		|| cls.state == ca_connecting )
	{
		if ( cls.demorecording )
			CL_Stop_f();

		FS_LogLevelLoadStarted( "ExitGame" );

		if ( cls.passive )
		{
			NET_LeaveGroup( cls.netchan.sock, cls.connect_stream );
			NET_LeaveGroup( cls.netchan.sock, cls.game_stream );
		}
		else if ( cls.netchan.remote_address.type != NA_UNUSED )
		{
			// Send a drop command.
			final[0] = clc_stringcmd;
			Q_strcpy( (char*)( final + 1 ), "dropclient\n" );
			for ( i = 0; i < 3; i++ )
				Netchan_Transmit( &cls.netchan, sizeof( final ), final );
		}

		cls.state = ca_disconnected;
		Steam_GSTerminateGameConnection();
		Q_memset( &g_GameServerAddress, 0, sizeof( g_GameServerAddress ) );

		// if running a local server, shut it down
		if ( Host_IsServerActive() )
			Host_ShutdownServer( FALSE );

		CloseSecurityModule();
	}

	cls.timedemo = FALSE;
	cls.demoplayback = FALSE;
	cls.signon = 0;

	CL_ClearState( TRUE );

	NET_CheckCleanupFakeIPConnection( cls.netchan.sock, &cls.netchan.remote_address );
	Netchan_Clear( &cls.netchan );

	CL_DeallocateDynamicData();

	scr_downloading.value = -1.0f;
	sys_timescale.value = 1.0f;
	g_LastScreenUpdateTime = 0.0f;

	if ( !Host_IsServerActive() )
	{
		Cvar_DirectSet( &sv_allow_autoaim, "0" );
		Cvar_DirectSet( &sv_allow_shaders, "0" );
	}

	VGuiWrap2_NotifyOfServerDisconnect();
	StopLoadingProgressBar();
}

void CL_ClearClientState()
{
	int i;

	for ( i = 0; i < CL_UPDATE_BACKUP; i++ )
	{
		if ( cl.frames[i].packet_entities.entities )
			Mem_Free( cl.frames[i].packet_entities.entities );

		cl.frames[i].packet_entities.entities = NULL;
		cl.frames[i].packet_entities.num_entities = 0;
	}

	CL_ClearResourceLists();

	for ( i = 0; i < MAX_CLIENTS; i++ )
		COM_ClearCustomizationList( &cl.players[i].customdata, FALSE );

	CL_ClearCaches();

	Q_memset( &cl, 0, sizeof( cl ) );

	cl.resourcesneeded.pPrev = cl.resourcesneeded.pNext = &cl.resourcesneeded;
	cl.resourcesonhand.pPrev = cl.resourcesonhand.pNext = &cl.resourcesonhand;

	CL_CreateResourceList();
}

void CL_MoveToOnHandList( resource_t* resource )
{
	if ( !resource )
	{
		Con_DPrintf( "Null resource passed to CL_MoveToOnHandList\n" );
		return;
	}

	CL_RemoveFromResourceList( resource );
	CL_AddToResourceList( resource, &cl.resourcesonhand );
}

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState( qboolean clearMemory )
{
	int i;

	if ( !Host_IsServerActive() )
		Host_ClearMemory( clearMemory );

	CL_ClearClientState();

	SZ_Clear( &cls.netchan.message );

	// clear other arrays
	Q_memset( cl_efrags, 0, sizeof( cl_efrags ) );
	Q_memset( cl_dlights, 0, sizeof( cl_dlights ) );
	Q_memset( cl_elights, 0, sizeof( cl_elights ) );
	Q_memset( cl_lightstyle, 0, sizeof( cl_lightstyle ) );

	CL_TempEntInit();

//
	// allocate the efrags and chain together into a free list
//
	cl.free_efrags = cl_efrags;
	for ( i = 0; i < MAX_EFRAGS - 1; i++ )
		cl.free_efrags[i].entnext = &cl.free_efrags[i + 1];

	cl.free_efrags[i].entnext = NULL;
}

void AddStartupTiming( const char* name )
{
	g_iCurrentTiming++;
	g_StartupTimings[g_iCurrentTiming].name = name;
	g_StartupTimings[g_iCurrentTiming].time = (float)Sys_FloatTime();
}

void CL_ReallocateDynamicData( int maxClients )
{
	cl.max_edicts = COM_EntsForPlayerSlots( maxClients );
	if ( cl.max_edicts <= 0 )
		Sys_Error( "CL_ReallocateDynamicData allocating 0 entities" );

	if ( cl_entities )
		Con_Printf( "CL_Reallocate cl_entities\n" );

	cl_entities = (cl_entity_t*)Mem_ZeroMalloc( sizeof( cl_entity_t ) * cl.max_edicts );

	R_AllocObjects( cl.max_edicts );

	Q_memset( cl.frames, 0, sizeof( frame_t ) * CL_UPDATE_BACKUP );
}

qboolean CL_PrecacheResources( void )
{
	resource_t *p, *n;

	SetLoadingProgressBarStatusText( "#GameUI_PrecachingResources" );
	ContinueLoadingProgressBar( "ClientConnect", 7, 0.0f );

	if ( fs_startup_timings.value )
		AddStartupTiming( "begin CL_PrecacheResources()" );

	for ( p = cl.resourcesonhand.pNext; p && p != &cl.resourcesonhand; p = n )
	{
		n = p->pNext;

		if ( p->ucFlags & RES_PRECACHED )
			continue;

		switch ( p->type )
		{
		case t_sound:
			if ( p->ucFlags & RES_WASMISSING )
			{
				cl.sound_precache[p->nIndex] = NULL;
				break;
			}

			S_BeginPrecaching();
			cl.sound_precache[p->nIndex] = S_PrecacheSound( p->szFileName );
			S_EndPrecaching();

			if ( !cl.sound_precache[p->nIndex] && ( p->ucFlags & RES_FATALIFMISSING ) )
			{
				COM_ExplainDisconnection( TRUE, "Cannot continue without sound %s, disconnecting.", p->szFileName );
				CL_Disconnect();
				return FALSE;
			}
			break;

		case t_skin:
		case t_generic:
			break;

		case t_model:
		{
			int modelIndex = p->nIndex;
			if ( modelIndex >= cl.model_precache_count )
			{
				cl.model_precache_count = modelIndex + 1;
				if ( cl.model_precache_count > MAX_MODELS )
					cl.model_precache_count = MAX_MODELS;
			}

			if ( p->szFileName[0] == '*' )
				break;

			model_t* model;
			if ( !fs_lazy_precache.value || !Q_strnicmp( p->szFileName, "maps", 4 ) )
				model = Mod_ForName( p->szFileName, FALSE, TRUE );
			else
				model = Mod_FindName( TRUE, p->szFileName );
			cl.model_precache[modelIndex] = model;

			if ( !model )
			{
				if ( p->ucFlags )
					Con_Printf( "Model %s not found and not available from server\n", p->szFileName );

				if ( p->ucFlags & RES_FATALIFMISSING )
				{
					COM_ExplainDisconnection( TRUE, "Cannot continue without model %s, disconnecting.", p->szFileName );
					CL_Disconnect();
					return FALSE;
				}
			}

			if ( !Q_strcasecmp( p->szFileName, "models/player.mdl" ) )
				cl_playerindex = modelIndex;
			break;
		}

		case t_decal:
			if ( !( p->ucFlags & RES_CUSTOM ) )
				Draw_DecalSetName( p->nIndex, p->szFileName );
			break;

		case t_eventscript:
			cl.event_precache[p->nIndex].filename = Mem_Strdup( p->szFileName );
			cl.event_precache[p->nIndex].pszScript = (const char*)COM_LoadFile( p->szFileName, 5, NULL );

			if ( !cl.event_precache[p->nIndex].pszScript )
			{
				Con_Printf( "Script %s not found and not available from server\n", p->szFileName );

				if ( p->ucFlags & RES_FATALIFMISSING )
				{
					COM_ExplainDisconnection( TRUE, "Cannot continue without script %s, disconnecting.", p->szFileName );
					CL_Disconnect();
					return FALSE;
				}
			}
			break;

		default:
			Con_DPrintf( "Unknown resource type\n" );
			break;
		}

		p->ucFlags |= RES_PRECACHED;
	}

	if ( fs_startup_timings.value )
		AddStartupTiming( "end  CL_PrecacheResources()" );

	return TRUE;
}

void CL_PrecacheBSPModels( const char* mapName )
{
	resource_t *p, *n;

	if ( !mapName || Q_strnicmp( mapName, "maps/", Q_strlen( "maps/" ) ) || !Q_strstr( mapName, ".bsp" ) )
		return;

	ContinueLoadingProgressBar( "ClientConnect", 9, 0.0f );

	for ( p = cl.resourcesonhand.pNext; p != &cl.resourcesonhand; p = n )
	{
		n = p->pNext;

		if ( p->type != t_model || p->szFileName[0] != '*' )
			continue;

		cl.model_precache[p->nIndex] = Mod_ForName( p->szFileName, FALSE, FALSE );
		if ( cl.model_precache[p->nIndex] )
			continue;

		Con_Printf( "Model %s not found\n", p->szFileName );

		if ( p->ucFlags & RES_FATALIFMISSING )
		{
			COM_ExplainDisconnection( TRUE, "Cannot continue without model %s, disconnecting.", p->szFileName );
			CL_Disconnect();
			return;
		}
	}
}

/*
==================
CL_RegisterResources

Clean up and move to next part of sequence.
==================
*/
void CL_RegisterResources( sizebuf_t* msg )
{
	CRC32_t mungebuffer;

	if ( cls.dl.custom || ( cls.signon == SIGNONS && cls.state == ca_active ) )
	{
		cls.dl.custom = FALSE;
		return;
	}

	if ( fs_startup_timings.value )
		AddStartupTiming( "begin CL_RegisterResources()" );

	ContinueLoadingProgressBar( "ClientConnect", 8, 0.0f );

	if ( !cls.demoplayback )
		CL_SendConsistencyInfo( msg );

	cl.worldmodel = cl.model_precache[1];
	if ( !cl.worldmodel || cl.maxclients < 1 )
	{
		Con_Printf( "Client world model is NULL\n" );
		COM_ExplainDisconnection( TRUE, "Client world model is NULL\n" );
		CL_Disconnect();
		return;
	}

	if ( !cl_entities )
		CL_ReallocateDynamicData( cl.maxclients );

	cl_entities[0].model = cl.worldmodel;

	Mod_CreateInlineModels( cl.worldmodel );
	CL_PrecacheBSPModels( cl.model_precache[1]->name );
	if ( cls.state == ca_disconnected )
		return;

	Con_DPrintf( "Setting up renderer...\n" );

	R_NewMap();

	Hunk_Check(); // make sure nothing is hurt

	noclip_anglehack = FALSE; // noclip is turned off at start

	if ( !cls.passive )
	{
		// Done with all resources, issue prespawn command.
		mungebuffer = cl.mapCRC;
		MSG_WriteByte( msg, clc_stringcmd );
		COM_Munge2( (byte*)&mungebuffer, sizeof( mungebuffer ), (byte)~cl.servercount );
		MSG_WriteString( msg, va( "spawn %i %i", cl.servercount, mungebuffer ) );
	}

	CL_InitSpectator();

	if ( fs_startup_timings.value )
		AddStartupTiming( "end   CL_RegisterResources()" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMessage - 
//-----------------------------------------------------------------------------
void CL_HudMessage( const char* message )
{
	DispatchDirectUserMsg( "HudText", Q_strlen( message ), (char*)message );
}

void CL_RemoveFromResourceList( resource_t* resource )
{
	if ( !resource->pPrev || !resource->pNext )
		Sys_Error( "Mislinked resource in CL_RemoveFromResourceList" );

	if ( resource->pNext == resource || resource->pPrev == resource )
		Sys_Error( "Attempt to free last entry in list." );

	resource->pPrev->pNext = resource->pNext;
	resource->pNext->pPrev = resource->pPrev;
	resource->pNext = NULL;
	resource->pPrev = NULL;
}

void CL_AddToResourceList( resource_t* resource, resource_t* list )
{
	if ( resource->pPrev || resource->pNext )
	{
		Con_Printf( "Resource already linked\n" );
		return;
	}

	if ( !list->pPrev || !list->pNext )
		Sys_Error( "Resource list corrupted.\n" );

	resource->pPrev = list->pPrev;
	list->pPrev->pNext = resource;
	list->pPrev = resource;
	resource->pNext = list;
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void CL_ProcessFile( qboolean successfully_received, const char* filename )
{
	resource_t* p;
	const char* pfilename;
	byte md5[MD5_DIGEST_BYTES];
	byte data[20480];
	sizebuf_t msg;

	if ( successfully_received )
	{
		if ( filename && filename[0] != '!' )
			Con_Printf( "Processing %s\n", filename );
	}
	else
	{
		Con_Printf( "Error: server failed to transmit file '%s'\n", CL_CleanFileName( filename ) );
	}

	pfilename = filename;
	if ( !Q_strnicmp( filename, "sound/", Q_strlen( "sound/" ) ) )
		pfilename = filename + Q_strlen( "sound/" );

	for ( p = cl.resourcesneeded.pNext; p != &cl.resourcesneeded; p = p->pNext )
	{
		if ( !Q_strnicmp( filename, "!MD5", 4 ) )
		{
			COM_HexConvert( pfilename + 4, sizeof( md5 ) * 2, md5 );
			if ( Q_memcmp( p->rgucMD5_hash, md5, sizeof( md5 ) ) )
				continue;
		}
		else if ( Q_strcasecmp( p->szFileName, p->type == t_generic ? filename : pfilename ) )
		{
			continue;
		}

		if ( successfully_received )
			p->ucFlags &= ~RES_WASMISSING;

		if ( filename[0] == '!' && cls.netchan.tempbuffer )
		{
			if ( p->nDownloadSize != cls.netchan.tempbuffersize )
			{
				Con_Printf( "CL_ProcessFile:  Downloaded %i bytes for purported %i byte file, ignoring download\n",
					cls.netchan.tempbuffersize, p->nDownloadSize );
			}
			else if ( p->ucFlags & RES_CUSTOM )
			{
				HPAK_AddLump( TRUE, "custom.hpk", p, cls.netchan.tempbuffer, NULL );
				CL_RegisterCustomization( p );
			}
		}

		if ( cls.netchan.tempbuffer )
			Mem_Free( cls.netchan.tempbuffer );

		cls.netchan.tempbuffer = NULL;
		cls.netchan.tempbuffersize = 0;

		CL_MoveToOnHandList( p );
		break;
	}

	if ( cls.state == ca_disconnected )
		return;

	if ( cl.resourcesneeded.pNext == &cl.resourcesneeded )
	{
		Q_memset( &msg, 0, sizeof( msg ) );
		msg.buffername = "Resource Registration";
		msg.data = data;
		msg.maxsize = sizeof( data );
		msg.cursize = 0;

		if ( CL_PrecacheResources() )
			CL_RegisterResources( &msg );

		if ( msg.cursize > 0 )
		{
			Netchan_CreateFragments( FALSE, &cls.netchan, &msg );
			Netchan_FragSend( &cls.netchan );
		}
	}

	if ( cls.netchan.tempbuffer )
	{
		Con_Printf( "CL_ProcessFile: Received a decal %s, but didn't find it in resources needed list!\n", pfilename );
		Mem_Free( cls.netchan.tempbuffer );
	}

	cls.netchan.tempbuffer = NULL;
	cls.netchan.tempbuffersize = 0;
}

cvar_t cl_resend = { "cl_resend", "6.0" };
cvar_t cl_timeout = { "cl_timeout", "60", FCVAR_ARCHIVE };
cvar_t cl_slisttimeout = { "cl_slist", "10.0" };

/*
=======================
CL_GetCDKeyHash()

Connections will now use a hashed cd key value
A LAN server will know not to allows more then xxx users with the same CD Key
=======================
*/
char* CL_GetCDKeyHash()
{
	static char szHashedKeyBuffer[256];
	char szKeyBuffer[256]; // Keys are about 13 chars long.	
	int nKeyLength;
	int bDedicated;
	MD5Context_t ctx;
	byte digest[MD5_DIGEST_BYTES];

	nKeyLength = 64;
	bDedicated = 0;

	Sys_GetCDKey( szKeyBuffer, &nKeyLength, &bDedicated );

	if ( bDedicated )
	{
		Con_Printf( "Key has no meaning on dedicated server...\n" );
		return (char*)com_empty_string;
	}

	if ( nKeyLength <= 0 || nKeyLength >= (int)sizeof( szKeyBuffer ) )
	{
		Con_Printf( "Bogus key length on CD Key...\n" );
		return (char*)com_empty_string;
	}

	szKeyBuffer[nKeyLength] = 0;

	// Now get the md5 hash of the key
	Q_memset( &ctx, 0, sizeof( ctx ) );
	Q_memset( digest, 0, sizeof( digest ) );

	MD5Init( &ctx );
	MD5Update( &ctx, (byte*)szKeyBuffer, nKeyLength );
	MD5Final( digest, &ctx );

	Q_memset( szHashedKeyBuffer, 0, sizeof( szHashedKeyBuffer ) );
	Q_strncpy( szHashedKeyBuffer, MD5_Print( digest ), sizeof( szHashedKeyBuffer ) - 1 );
	szHashedKeyBuffer[sizeof( szHashedKeyBuffer ) - 1] = 0;
	return szHashedKeyBuffer;
}

typedef struct
{
	int inuse;
	netadr_t adr;
	netadr_t spec_adr;
	char name[256];
	char spec_name[256];
	char map[256];
	char desc[256];
	int players;
	int maxplayers;
	int bots;
	int has_spectator_address;
	char os;
	char type;
	qboolean is_private;
	qboolean is_vac;
} server_cache_t;

static server_cache_t cached_servers[MAX_CACHED_SERVERS];
static char g_lastrconcommand[1024];
static netadr_t g_rconaddress;

static qboolean IsFromConnectedServer( netadr_t adr )
{
	return NET_CompareAdr( adr, g_GameServerAddress ) || NET_CompareAdr( adr, g_rconaddress );
}

/*
=====================
CL_Connect_f

User command to connect to server
=====================
*/
void CL_Connect_f()
{
	const char* server;
	char name[MAX_PATH];
	CareerStateType careerState;
	int num;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "usage: connect <server>\n" );
		return;
	}

	SetCStrikeFlags();
	if ( ( g_bIsCStrike || g_bIsCZero ) && cmd_source != src_command )
	{
		Con_Printf( "Connect only works from the console.\n" );
		return;
	}

	server = Cmd_Args();
	if ( !server )
		return;

	Q_strncpy( name, server, sizeof( name ) );
	name[sizeof( name ) - 1] = 0;

	if ( cls.demoplayback )
		CL_StopPlayback();

	careerState = g_careerState;
	CL_Disconnect();
	if ( careerState == CAREER_LOADING )
		g_careerState = CAREER_LOADING;

	StartLoadingProgressBar( "Connecting", 12 );
	SetLoadingProgressBarStatusText( "#GameUI_EstablishingConnection" );

	num = Q_atoi( name );
	if ( !Q_strstr( name, "." ) && num > 0 && num <= num_servers )
	{
		Q_strncpy( name, NET_AdrToString( cached_servers[num - 1].adr ), sizeof( name ) );
		name[sizeof( name ) - 1] = 0;
	}

	Q_memset( msg_buckets, 0, sizeof( msg_buckets ) );
	Q_memset( total_data, 0, sizeof( total_data ) );

	Q_strncpy( cls.servername, name, sizeof( cls.servername ) - 1 );
	cls.servername[sizeof( cls.servername ) - 1] = 0;

	cls.connect_time = -99999;
	cls.state = ca_connecting;
	cls.connect_retry = 0;
	cls.passive = FALSE;
	cls.spectator = FALSE;
	cls.isVAC2Secure = FALSE;
	cls.GameServerSteamID = 0;
	cls.build_num = 0;
	Q_memset( &g_GameServerAddress, 0, sizeof( g_GameServerAddress ) );
	cls.challenge = 0;
	gfExtendedError = FALSE;
	g_LastScreenUpdateTime = 0.0f;

	if ( Q_strnicmp( cls.servername, "local", 5 ) )
		NET_Config( TRUE ); // allow remote
}

static void CL_ServiceChallenge()
{
	const char* type;

	if ( Cmd_Argc() != 3 )
		return;

	type = Cmd_Argv( 1 );
	if ( !type || !type[0] || Q_strcasecmp( type, "rcon" ) )
		return;

	CL_SendRcon( &net_from, (unsigned int)Q_atoi( Cmd_Argv( 2 ) ) );
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend()
{
	netadr_t adr;
	char szServerName[MAX_PATH];
	char data[2048];
	const char* status;

	if ( cls.state == ca_disconnected && Host_IsServerActive() )
	{
		cls.state = ca_connecting;
		gfExtendedError = FALSE;
		Q_strncpy( cls.servername, "localhost", sizeof( cls.servername ) - 1 );
		cls.servername[sizeof( cls.servername ) - 1] = 0;
		cls.authprotocol = PROTOCOL_STEAM;
		CL_SendConnectPacket();
		return;
	}

	// resend if we haven't gotten a reply yet
	// We only resend during the connection process.
	if ( cls.state != ca_connecting )
		return;

	if ( cls.passive )
	{
		if ( realtime - cls.connect_time > 30.0 )
		{
			if ( cls.challenge )
			{
				Con_Printf( "Multicast failed. Falling back to unicast...\n" );
				cls.passive = FALSE;
				NET_LeaveGroup( cls.netchan.sock, cls.connect_stream );
				CL_SendConnectPacket();
			}
			else
			{
				COM_ExplainDisconnection( TRUE, "Joining multicast group failed after %.0f seconds.", 30.0 );
				cls.connect_time = -99999;
				cls.connect_retry = 0;
				cls.state = ca_disconnected;
				Q_memset( &g_GameServerAddress, 0, sizeof( g_GameServerAddress ) );
				cls.passive = FALSE;
				NET_LeaveGroup( cls.netchan.sock, cls.connect_stream );
			}
		}
		return;
	}

	if ( cl_resend.value < 1.5f )
		Cvar_SetValue( "cl_resend", 1.5f );
	else if ( cl_resend.value > 20.0f )
		Cvar_SetValue( "cl_resend", 20.0f );

	// Wait at least the resend # of seconds.
	if ( realtime - cls.connect_time < cl_resend.value )
		return;

	Q_strncpy( szServerName, cls.servername, sizeof( szServerName ) );
	szServerName[sizeof( szServerName ) - 1] = 0;

	// Deal with local connection.
	if ( !Q_strcasecmp( cls.servername, "local" ) )
		snprintf( szServerName, sizeof( szServerName ), "%s", "localhost" );

	if ( !NET_StringToAdr( szServerName, &adr ) )
	{
		Con_Printf( "Bad server address\n" );
		cls.state = ca_disconnected;
		Q_memset( &g_GameServerAddress, 0, sizeof( g_GameServerAddress ) );
		return;
	}

	if ( !adr.port )
		adr.port = BigShort( PORT_SERVER );

	// Only retry so many times before failure.
	if ( cls.connect_retry >= CL_CONNECTION_RETRIES )
	{
		COM_ExplainDisconnection( TRUE, "#GameUI_CouldNotContactGameServer" );
		cls.connect_time = -99999;
		cls.connect_retry = 0;
		cls.state = ca_disconnected;
		Q_memset( &g_GameServerAddress, 0, sizeof( g_GameServerAddress ) );
		CL_Disconnect();
		return;
	}

	// Mark time of this attempt for retransmit requests
	// Mark time of this attempt.
	cls.connect_time = realtime;	// for retransmit requests

	// Display appropriate message
	if ( Q_strcasecmp( szServerName, "localhost" ) )
	{
		if ( !cls.connect_retry )
		{
			Con_Printf( "Connecting to %s...\n", szServerName );
		}
		else
		{
			if ( cls.connect_retry == 2 )
				status = "#GameUI_RetryingConnectionToServer2";
			else if ( cls.connect_retry == 3 )
				status = "#GameUI_RetryingConnectionToServer3";
			else
				status = "#GameUI_RetryingConnectionToServer";

			SetLoadingProgressBarStatusText( status );
			Con_Printf( "Retrying %s...\n", szServerName );
		}
	}

	cls.connect_retry++;

	snprintf( data, sizeof( data ), "%c%c%c%cgetchallenge steam\n", 255, 255, 255, 255 );

	g_GameServerAddress = adr;
	// Send the request.
	NET_SendPacket( NS_CLIENT, Q_strlen( data ), data, adr );
}

void CL_SendConnectPacket()
{
	netadr_t adr;
	char szServerName[MAX_PATH];
	char protinfo[1024];
	char cdkey[1024];
	char steamKey[1024];
	char data[2048];
	qboolean steam;
	unsigned int ip;
	size_t keySize, total;
	int len;

	Q_memset( protinfo, 0, sizeof( protinfo ) );

	ContinueLoadingProgressBar( "ClientConnect", 1, 0.0f );

	if ( fs_startup_timings.value )
		SetupStartupTimings();

	Q_strncpy( szServerName, cls.servername, sizeof( szServerName ) );
	szServerName[sizeof( szServerName ) - 1] = 0;

	// Deal with local connection.
	if ( !Q_strcasecmp( cls.servername, "local" ) )
		snprintf( szServerName, sizeof( szServerName ), "%s", "localhost" );

	if ( !NET_StringToAdr( szServerName, &adr ) )
	{
		Con_Printf( "Unable to resolve %s\n", szServerName );
		cls.connect_time = -99999;
		cls.connect_retry = 0;
		cls.state = ca_disconnected;
		Q_memset( &g_GameServerAddress, 0, sizeof( g_GameServerAddress ) );
		return;
	}

	if ( !adr.port )
		adr.port = BigShort( PORT_SERVER );

	if ( cls.authprotocol == PROTOCOL_STEAM )
	{
		steam = TRUE;
	}
	else
	{
		steam = FALSE;
		cls.authprotocol = PROTOCOL_HASHEDCDKEY;

		CL_GetCDKeyHash();
		Q_strncpy( cdkey, CL_GetCDKeyHash(), sizeof( cdkey ) - 1 );
		cdkey[sizeof( cdkey ) - 1] = 0;
	}

	Info_SetValueForStarKey( protinfo, "prot", va( "%i", cls.authprotocol ), sizeof( protinfo ) );
	if ( steam )
		Info_SetValueForStarKey( protinfo, "unique", va( "%i", -1 ), sizeof( protinfo ) );
	Info_SetValueForStarKey( protinfo, "raw", steam ? "steam" : cdkey, sizeof( protinfo ) );
	if ( steam )
		Info_SetValueForStarKey( protinfo, "cdkey", CL_GetCDKeyHash(), sizeof( protinfo ) );

	Q_memset( steamKey, 0, sizeof( steamKey ) );
	keySize = 0;

	if ( steam )
	{
		ip = NET_NetAdr_GetIPNetworkByteOrder( &adr );
		if ( adr.type == NA_LOOPBACK )
			ip = htonl( 0x7f000001 );

		keySize = (size_t)Steam_GSInitiateGameConnection(
			steamKey, sizeof( steamKey ), cls.GameServerSteamID, ip, adr.port, cls.isVAC2Secure );
	}

	cls.connect_time = realtime;

	snprintf( data, sizeof( data ), "%c%c%c%cconnect %i %i \"%s\" \"%s\"\n", 255, 255, 255, 255, PROTOCOL_VERSION,
		cls.challenge, protinfo, cls.userinfo ); // Send protocol and challenge value

	len = Q_strlen( data );

	if ( steam )
	{
		total = len + keySize;
		if ( total > sizeof( data ) )
		{
			Con_Printf(
				"CL_SendConnectPacket:  Potential buffer overrun (%i/%i>%i) copying steamkeybuffer into connection packet\n",
				len, (int)keySize, (int)sizeof( data ) );
			CL_Disconnect_f();
			return;
		}

		memcpy( data + len, steamKey, keySize );
		len = (int)total;
	}

	g_GameServerAddress = adr;
	NET_SendPacket( NS_CLIENT, len, data, adr );
}
/*
=================
CL_ReadPackets

Updates the local time and reads/handles messages on client net connection.
=================
*/
void CL_ReadPackets()
{
	int nMessages;
	char szStatus[128];

	if ( cls.state == ca_dedicated )
		return;

	cl.oldtime = cl.time;
	cl.time += host_frametime;

	nMessages = 0;
	while ( CL_GetMessage() )
	{
		nMessages++;
		if ( nMessages > MAX_PACKETS_PER_FRAME && !cls.demoplayback )
			continue;

		//
		// remote command packet
		//
		if ( *(int*)net_message.data == -1 )
		{
			CL_ConnectionlessPacket();
			continue;
		}

		//
		// packet from server, verify source IP address.
		//
		if ( !cls.passive && !cls.demoplayback && !NET_CompareAdr( net_from, cls.netchan.remote_address ) )
			continue;

		if ( cls.passive && cls.state == ca_connecting )
			cls.state = ca_connected;
		else if ( cls.state == ca_disconnected || cls.state == ca_connecting )
			continue;		// dump sequenced message from server if not connected yet

		if ( cls.demoplayback )
		{
			MSG_BeginReading();
			CL_ParseServerMessage( TRUE );
		}
		else if ( net_message.cursize >= 8 )
		{
			if ( Netchan_Process( &cls.netchan ) )
				CL_ParseServerMessage( TRUE );
		}
		else
		{
			Con_Printf( "CL_ReadPackets:  undersized packet %i bytes from %s\n", net_message.cursize,
				NET_AdrToString( net_from ) );
		}
	}

	if ( nMessages > MAX_PACKETS_PER_FRAME && !cls.demoplayback )
		Con_DPrintf( "Ignored %i messages\n", nMessages - MAX_PACKETS_PER_FRAME );

	CL_SetSolidEntities();

	if ( cls.state >= ca_connecting && Netchan_IncomingReady( &cls.netchan ) )
	{
		// Process the incoming buffer(s)
		if ( Netchan_CopyNormalFragments( &cls.netchan ) )
		{
			MSG_BeginReading();
			CL_ParseServerMessage( FALSE );
		}

		if ( Netchan_CopyFileFragments( &cls.netchan ) )
		{
			if ( gDownloadFile[0] )
			{
				if ( gDownloadFile[0] == '!' )
				{
					SetSecondaryProgressBarText( "#GameUI_SecurityModule" );
				}
				else
				{
					snprintf( szStatus, sizeof( szStatus ), "File '%s'", gDownloadFile );
					SetSecondaryProgressBarText( szStatus );
				}

				SetSecondaryProgressBar( 100.0f );
			}

			// Remove from resource request stuff.
			CL_ProcessFile( TRUE, cls.netchan.incomingfilename );
		}
	}

	// check timeout
	if ( cls.state >= ca_connected && !cls.demoplayback && realtime - cls.netchan.last_received > cl_timeout.value )
	{
		COM_ExplainDisconnection( TRUE, "#GameUI_ServerConnectionTimeout" );
		CL_Disconnect();
		return;
	}

	if ( cl_shownet.value )
		Con_Printf( "\n" );

	Netchan_UpdateProgress( &cls.netchan );

	if ( scr_downloading.value > 0.0f && gDownloadFile[0] )
	{
		if ( gDownloadFile[0] == '!' )
			snprintf( szStatus, sizeof( szStatus ), "#GameUI_DownloadingSecurityModule" );
		else
			snprintf( szStatus, sizeof( szStatus ), "Downloading file '%s'", gDownloadFile );

		SetSecondaryProgressBar( scr_downloading.value / 100.0f );
		SetSecondaryProgressBarText( szStatus );
		SetLoadingProgressBarStatusText( "#GameUI_VerifyingAndDownloading" );
	}

	Net_APICheckTimeouts();
}

static void CL_ConnectClient()
{
	ContinueLoadingProgressBar( "ClientConnect", 2, 0.0f );

	// Already connected?
	if ( cls.state == ca_connected )
	{
		if ( !cls.demoplayback )
			Con_DPrintf( "Duplicate connect ack. received.  Ignored.\n" );
		return;
	}

	cls.userid = 0;
	cls.trueaddress[0] = 0;

	if ( Cmd_Argc() >= 3 )
	{
		cls.userid = Q_atoi( Cmd_Argv( 1 ) );
		Q_strncpy( cls.trueaddress, Cmd_Argv( 2 ), sizeof( cls.trueaddress ) - 1 );
		cls.trueaddress[sizeof( cls.trueaddress ) - 1] = 0;

		if ( Cmd_Argc() >= 5 )
			cls.build_num = Q_atoi( Cmd_Argv( 4 ) );
	}

	Netchan_Setup( NS_CLIENT, &cls.netchan, net_from, -1, &cls, CL_GetFragmentSize );
	NET_ClearLagData( TRUE, FALSE );

	if ( fs_startup_timings.value )
		AddStartupTiming( "connection accepted by server" );

	if ( Q_strcasecmp( "loopback", NET_AdrToString( net_from ) ) )
		Con_Printf( "Connection accepted by %s\n", NET_AdrToString( net_from ) );
	else
		Con_DPrintf( "Connection accepted.\n" );

	cls.state = ca_connected;

	MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
	MSG_WriteString( &cls.netchan.message, "new" );

	cls.connect_time = realtime;
	cls.demonum = -1; // Not in the demo loop now
	cls.signon = 0;
	// Haven't gotten a valid frame update yet
	cl.validsequence = 0;
	// We'll request a full delta from the baseline
	cl.delta_sequence = -1;
	// We don't have a backed up cmd history yet
	cls.lastoutgoingcommand = -1;
	cls.nextcmdtime = (float)realtime;
}

static void CL_Listen( char* address )
{
	if ( !address )
		return;

	NET_StringToAdr( address, &cls.connect_stream );
	cls.connect_stream.type = NA_IP;

	if ( cls.connect_stream.ip[0] < 224 || cls.connect_stream.ip[0] > 239 )
	{
		Con_Printf( "%s is not a class D IP.\n", address );
		return;
	}

	CL_Listen_0();
}

static void CL_PrintCachedServer( int slot )
{
	if ( slot < 0 || slot >= MAX_CACHED_SERVERS )
		return;

	CL_PrintCachedServer_0( slot );
}

/*
=================
CL_AddToServerCache

Adds the address, name to the server cache
=================
*/
static void CL_AddToServerCache( server_cache_t* server )
{
	int i;
	float elapsed;

	if ( num_servers >= MAX_CACHED_SERVERS )
		return;

	if ( cl_slisttimeout.value > 0.0f )
	{
		elapsed = (float)realtime - cls.slist_time;
		if ( elapsed >= cl_slisttimeout.value )
			return;
	}

	// Already cached by address
	for ( i = 0; i < num_servers; i++ )
	{
		if ( NET_CompareAdr( cached_servers[i].adr, server->adr ) )
			return;
	}

	cached_servers[num_servers] = *server;
	// Display it.
	cached_servers[num_servers].inuse = 1;
	num_servers++;

	Con_Printf( "------------------\n" );
	CL_PrintCachedServer( num_servers - 1 );
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
static void CL_ConnectionlessPacket()
{
	char* s;
	char* reason;
	char* server;
	const char* args;
	const char* steamid;
	const char* secure;
	byte c;
	byte flags;
	char redirect[MAX_PATH];
	server_cache_t info;

	MSG_BeginReading();
	MSG_ReadLong(); // skip the -1 marker

	s = MSG_ReadStringLine();
	Cmd_TokenizeString( s );

	args = Cmd_Argv( 0 );
	c = args ? (byte)args[0] : 0;

	if ( !Q_strcasecmp( args, "challenge" ) )
	{
		if ( IsFromConnectedServer( net_from ) )
			CL_ServiceChallenge();
		return;
	}

	// FIXME:  For some of these, we should confirm that the sender of 
	// the message is what we think the server is...

	switch ( c )
	{
	case S2C_CONNECTION:
		if ( IsFromConnectedServer( net_from ) )
			CL_ConnectClient();
		break;

	case S2C_LISTEN:
		if ( cls.state != ca_connecting || Cmd_Argc() != 4 )
			break;

		cls.challenge = Q_atoi( Cmd_Argv( 1 ) );
		cls.authprotocol = (byte)Q_atoi( Cmd_Argv( 2 ) );

		if ( cls.authprotocol == 255 )
		{
			cls.authprotocol = PROTOCOL_HASHEDCDKEY;
		}
		else if ( cls.authprotocol != PROTOCOL_HASHEDCDKEY )
		{
			Con_Printf( "Invalid multicast authentication type (%i).\n", cls.authprotocol );
			CL_Disconnect_f();
			break;
		}

		CL_Listen( (char*)Cmd_Argv( 3 ) );
		break;

	case S2C_CHALLENGE:
		if ( !IsFromConnectedServer( net_from ) || cls.state != ca_connecting
			|| ( Cmd_Argc() != 3 && Cmd_Argc() != 5 ) )
			break;

		cls.challenge = Q_atoi( Cmd_Argv( 1 ) );
		cls.authprotocol = (byte)Q_atoi( Cmd_Argv( 2 ) );

		if ( cls.authprotocol == 255 )
		{
			cls.authprotocol = PROTOCOL_HASHEDCDKEY;
		}
		else if ( cls.authprotocol == PROTOCOL_STEAM )
		{
			if ( Cmd_Argc() != 5 )
				return;

			steamid = Cmd_Argv( 3 );
			if ( !steamid )
			{
				Con_Printf( "STEAM keysize is bogus (1)\n" );
				CL_Disconnect_f();
				return;
			}

			secure = Cmd_Argv( 4 );
			if ( !secure )
			{
				Con_Printf( "STEAM keysize is bogus (2)\n" );
				CL_Disconnect_f();
				return;
			}

			cls.isVAC2Secure = Q_atoi( secure );
			cls.GameServerSteamID = (uint64_t)_atoi64( steamid );
		}

		//Con_Printf( "Using Normal\n");
		CL_SendConnectPacket();
		break;

	case A2A_PRINT:
		if ( IsFromConnectedServer( net_from ) )
		{
			MSG_BeginReading();
			MSG_ReadLong(); // skip the -1 marker
			MSG_ReadByte();
			Con_Printf( "%s\n", MSG_ReadString() );
		}
		break;

	case S2C_BADPASSWORD:
		if ( !IsFromConnectedServer( net_from ) || cls.state != ca_connecting )
			break;

		reason = s + 1;
		if ( !Q_strnicmp( reason, "BADPASSWORD", Q_strlen( "BADPASSWORD" ) ) )
			reason += Q_strlen( "BADPASSWORD" );

		Con_Printf( "%s", reason );
		COM_ExplainDisconnection( FALSE, "#GameUI_ServerConnectionFailedBadPassword" );
		Con_Printf( "Invalid server password.\n" );
		CL_Disconnect();
		break;

	case S2C_REDIRECT:
		if ( !IsFromConnectedServer( net_from ) || cls.state != ca_connecting )
			break;

		server = s + 1;
		if ( strchr( server, '\n' ) || strchr( server, ';' ) )
		{
			COM_ExplainDisconnection( TRUE, "Invalid command separator in redirect server name, disconnecting.\n" );
			CL_Disconnect();
			break;
		}

		snprintf( redirect, sizeof( redirect ), "connect %s\n", server );
		Cbuf_AddText( redirect );
		Con_Printf( "Redirecting connection to %s.\n", server );
		g_bRedirectedToProxy = TRUE;
		break;

	case S2C_CONNREJECT:
		if ( IsFromConnectedServer( net_from ) && ( COM_CheckParm( "-steam" ) || cls.state == ca_connecting ) )
		{
			COM_ExplainDisconnection( TRUE, s + 1 );
			CL_Disconnect();
		}
		break;

	case S2A_INFO_SRC:
		Q_memset( &info, 0, sizeof( info ) );
		info.adr = net_from;

		Q_strncpy_s( info.name, s + 2, sizeof( info.name ) );
		Q_strncpy_s( info.map, MSG_ReadString(), sizeof( info.map ) );
		// game directory
		MSG_SkipString();
		Q_strncpy_s( info.desc, MSG_ReadString(), sizeof( info.desc ) );
		MSG_ReadShort();
		info.players = MSG_ReadByte();
		info.maxplayers = MSG_ReadByte();
		info.bots = MSG_ReadByte();
		info.type = (char)MSG_ReadByte();
		info.os = (char)MSG_ReadByte();
		info.is_private = MSG_ReadByte() != 0;
		info.is_vac = MSG_ReadByte() != 0;
		MSG_SkipString();

		flags = (byte)MSG_ReadByte();
		if ( !msg_badread )
		{
			if ( flags & S2A_EXTRA_DATA_HAS_GAME_PORT )
				info.adr.port = ntohs( (unsigned short)MSG_ReadShort() );

			if ( flags & S2A_EXTRA_DATA_HAS_STEAMID )
			{
				MSG_ReadLong();
				MSG_ReadLong();
			}

			if ( flags & S2A_EXTRA_DATA_HAS_SPECTATOR_DATA )
			{
				info.spec_adr = net_from;
				info.spec_adr.port = ntohs( (unsigned short)MSG_ReadShort() );
				Q_strncpy_s( info.spec_name, MSG_ReadString(), sizeof( info.spec_name ) );
				info.has_spectator_address = 1;
			}

			if ( flags & S2A_EXTRA_DATA_HAS_GAMETAG_DATA )
				MSG_SkipString();

			if ( flags & S2A_EXTRA_DATA_GAMEID )
			{
				MSG_ReadLong();
				MSG_ReadLong();
			}
		}

		CL_AddToServerCache( &info );
		break;

	default:
		if ( CL_ClientDllQuery( s ) )
			return;

		Con_Printf( "Unknown command:\n%c\n", c );
		break;
	}
}

void CL_SpecPos_f()
{
	vec3_t origin, angles;

	GetPos( origin, angles );

	Con_Printf( "spec_pos %.1f %.1f %.1f %.1f %.1f\n", origin[0], origin[1], origin[2], angles[PITCH], angles[YAW] );
}

cvar_t cl_fixtimerate = { "cl_fixtimerate", "7.5" };
cvar_t cl_name = { "name", "unnamed", FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE };
//
// info mirrors
//
cvar_t password = { "password", "", FCVAR_USERINFO };
cvar_t team = { "team", "", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t cl_model = { "model", "", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t skin = { "skin", "", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t topcolor = { "topcolor", "0", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t bottomcolor = { "bottomcolor", "0", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t rate = { "rate", "30000", FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_FILTERSTUFFTEXT };
cvar_t cl_updaterate = { "cl_updaterate", "60", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t cl_lc = { "cl_lc", "1", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t cl_dlmax = { "cl_dlmax", "512", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t fs_perf_warnings = { "fs_perf_warnings", "0" };
cvar_t cl_showevents = { "cl_showevents", "0" };
cvar_t cl_himodels = { "cl_himodels", "0", FCVAR_ARCHIVE };
cvar_t cl_gaitestimation = { "cl_gaitestimation", "1" };
cvar_t cl_idealpitchscale = { "cl_idealpitchscale", "0.8", FCVAR_ARCHIVE };
cvar_t cl_cmdbackup = { "cl_cmdbackup", "2", FCVAR_ARCHIVE };
cvar_t rcon_address = { "rcon_address", "", FCVAR_PRIVILEGED };
cvar_t rcon_port = { "rcon_port", "0", FCVAR_PRIVILEGED };
cvar_t cl_solid_players = { "cl_solid_players", "1" };
cvar_t cl_allow_download = { "cl_allowdownload", "1", FCVAR_ARCHIVE };
cvar_t cl_allow_upload = { "cl_allowupload", "1", FCVAR_ARCHIVE };
cvar_t cl_cmdrate = { "cl_cmdrate", "60", FCVAR_ARCHIVE };
cvar_t cl_showfps = { "cl_showfps", "0", FCVAR_ARCHIVE };
cvar_t cl_needinstanced = { "cl_needinstanced", "0" };
cvar_t cl_logofile = { "cl_logofile", "lambda", FCVAR_ARCHIVE };
cvar_t cl_logocolor = { "cl_logocolor", "orange", FCVAR_ARCHIVE };
cvar_t cl_mousegrab = { "cl_mousegrab", "1", FCVAR_ARCHIVE };
cvar_t m_rawinput = { "m_rawinput", "0", FCVAR_ARCHIVE };
cvar_t cl_fixmodelinterpolationartifacts = { "cl_fixmodelinterpolationartifacts", "1" };
cvar_t cl_skipvid = { "cl_skipvid", "0" };

void CL_Disconnect_f()
{
	CL_Disconnect();

	if ( Host_IsServerActive() )
		Host_ShutdownServer( FALSE );
}

int CL_GetFragmentSize( void* state )
{
	if ( ( (client_static_t*)state )->state != ca_active )
		return 128;

	if ( !( cl_dlmax.value >= 16 ) )
		return 256;

	if ( cl_dlmax.value >= 1024 )
		return 1024;

	return (int)cl_dlmax.value;
}

/*
================
CL_Retry_f

  Retry last connection (e.g., after we enter a password)

================
*/
void CL_Retry_f()
{
	if ( !cls.servername[0] )
	{
		Con_Printf( "Can't retry, no previous connection\n" );
		return;
	}

	if ( strchr( cls.servername, '\n' ) || strchr( cls.servername, ';' ) )
	{
		Con_Printf( "Invalid command separator in server name, refusing retry\n" );
		return;
	}

	CL_Retry_f_0();
}

void CL_Listen_f()
{
	const char* address;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "usage: listen <IP>\n" );
		return;
	}

	address = Cmd_Args();
	if ( !address )
		return;

	cls.challenge = 0;
	CL_Listen( (char*)address );
}

void CL_ListCachedServers_f()
{
	int i;

	if ( !num_servers )
	{
		Con_Printf( "No local servers in list.\nTry 'slist' to search again.\n" );
		return;
	}

	for ( i = 0; i < num_servers; i++ )
	{
		if ( !cached_servers[i].inuse )
			continue;

		Con_Printf( "------------------\n" );
		CL_PrintCachedServer( i );
	}
}

/*
==================
CL_Slist_f (void)

Populates the client's server_cache_t structrue
Replaces Slist command
==================
*/
void CL_Slist_f()
{
	num_servers = 0;
	Q_memset( cached_servers, 0, sizeof( cached_servers ) );

	CL_PingServers_f();
}

/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f()
{
	netadr_t to;    // Command destination
	unsigned short port; // Outgoing port.

	if ( !rcon_password.string )
	{
		Con_Printf( "You must set 'rcon_password' before\nissuing an rcon command.\n" );
		return;
	}

	if ( !Cmd_Args() )
	{
		Con_Printf( "Rcon:  Empty rcon string\n" );
		return;
	}

	if ( cls.state < ca_connected )
	{
		NET_Config( TRUE ); // allow remote

		if ( !Q_strlen( rcon_address.string ) )
		{
			Con_Printf( "You must either be connected,\nor set the 'rcon_address' cvar\nto issue rcon commands\n" );
			return;
		}
	}

	if ( Q_strlen( rcon_address.string ) )
	{
		if ( !NET_StringToAdr( rcon_address.string, &to ) )
		{
			Con_Printf( "Unable to resolve rcon address %s\n", rcon_address.string );
			return;
		}
	}
	else
	{
		to = cls.netchan.remote_address;
	}

	port = (unsigned short)(int)rcon_port.value;
	if ( !port )
	{
		if ( cls.state < ca_connected )
			port = PORT_SERVER;
		else
			port = BigShort( cls.netchan.remote_address.port );
	}
	to.port = BigShort( port );

	Q_strncpy( g_lastrconcommand, Cmd_Args(), sizeof( g_lastrconcommand ) - 1 );
	g_lastrconcommand[sizeof( g_lastrconcommand ) - 1] = 0;
	g_rconaddress = to;

	Netchan_OutOfBandPrint( NS_CLIENT, to, "challenge rcon\n" );
}

/*
=================
CL_Init
=================
*/
void CL_Init()
{
	const char* name;

	name = Steam_GetCommunityName();
	if ( !name )
		name = "unknown";

	Info_SetValueForStarKey( cls.userinfo, "name", name, sizeof( cls.userinfo ) );
	Info_SetValueForStarKey( cls.userinfo, "topcolor", "0", sizeof( cls.userinfo ) );
	Info_SetValueForStarKey( cls.userinfo, "bottomcolor", "0", sizeof( cls.userinfo ) );
	Info_SetValueForStarKey( cls.userinfo, "rate", "2500", sizeof( cls.userinfo ) );
	Info_SetValueForStarKey( cls.userinfo, "cl_updaterate", "20", sizeof( cls.userinfo ) );
	Info_SetValueForStarKey( cls.userinfo, "cl_lw", "1", sizeof( cls.userinfo ) );
	Info_SetValueForStarKey( cls.userinfo, "cl_lc", "1", sizeof( cls.userinfo ) );

	Net_APIInit();
	CL_InitTEnts();
	CL_InitExtrap();
	TextMessageInit();
	Sequence_Init();

//
	// register our commands
//
	Cvar_RegisterVariable( &cl_weaponlistfix );
	Cvar_RegisterVariable( &cl_fixtimerate );
	Cvar_RegisterVariable( &cl_showmessages );
//
// register our commands
//
	Cvar_RegisterVariable( &cl_name );
	Cvar_RegisterVariable( &password );
	Cvar_RegisterVariable( &team );
	Cvar_RegisterVariable( &cl_model );
	Cvar_RegisterVariable( &skin );
	Cvar_RegisterVariable( &topcolor );
	Cvar_RegisterVariable( &bottomcolor );
	Cvar_RegisterVariable( &rate );
	Cvar_RegisterVariable( &cl_updaterate );
	Cvar_RegisterVariable( &cl_lw );
	Cvar_RegisterVariable( &cl_lc );
	Cvar_RegisterVariable( &cl_dlmax );
	Cvar_RegisterVariable( &fs_startup_timings );
	Cvar_RegisterVariable( &fs_lazy_precache );
	Cvar_RegisterVariable( &fs_precache_timings );
	Cvar_RegisterVariable( &fs_perf_warnings );
	Cvar_RegisterVariable( &cl_clockreset );
	Cvar_RegisterVariable( &cl_showevents );
	Cvar_RegisterVariable( &cl_himodels );
	Cvar_RegisterVariable( &cl_gaitestimation );
	Cvar_RegisterVariable( &cl_idealpitchscale );
	Cvar_RegisterVariable( &cl_resend );
	Cvar_RegisterVariable( &cl_timeout );
	Cvar_RegisterVariable( &cl_cmdbackup );
	Cvar_RegisterVariable( &cl_shownet );
	Cvar_RegisterVariable( &rcon_address );
	Cvar_RegisterVariable( &rcon_port );
	Cvar_RegisterVariable( &cl_solid_players );
	Cvar_RegisterVariable( &cl_slisttimeout );
	Cvar_RegisterVariable( &cl_download_ingame );
	Cvar_RegisterVariable( &cl_allow_download );
	Cvar_RegisterVariable( &cl_allow_upload );
	Cvar_RegisterVariable( &cl_gamegauge );
	Cvar_RegisterVariable( &cl_cmdrate );
	Cvar_RegisterVariable( &cl_showfps );
	Cvar_RegisterVariable( &cl_needinstanced );
	Cvar_RegisterVariable( &dev_overview );
	Cvar_RegisterVariable( &cl_logofile );
	Cvar_RegisterVariable( &cl_logocolor );
	Cvar_RegisterVariable( &cl_mousegrab );
	Cvar_RegisterVariable( &m_rawinput );
	Cvar_RegisterVariable( &cl_filterstuffcmd );
	Cvar_RegisterVariable( &cl_fixmodelinterpolationartifacts );
	Cvar_RegisterVariable( &cl_skipvid );

	if ( COM_CheckParm( "-nomousegrab" ) )
		Cvar_Set( "cl_mousegrab", "0" );

	if ( COM_CheckParm( "-novid" ) || COM_CheckParm( "-dev" ) )
		Cvar_Set( "cl_skipvid", "1" );

	ClientDLL_HudInit();

	Cmd_AddCommand( "gamedir", CL_GameDir_f );
	Cmd_AddCommandWithFlags( "connect", CL_Connect_f, CMD_FLAG_PRIVILEGED );
	Cmd_AddCommand( "fullserverinfo", CL_FullServerinfo_f );
	Cmd_AddCommandWithFlags( "retry", CL_Retry_f, CMD_FLAG_PRIVILEGED );
	Cmd_AddCommand( "disconnect", CL_Disconnect_f );
	Cmd_AddCommand( "record", CL_Record_f );
	Cmd_AddCommand( "stop", CL_Stop_f );
	Cmd_AddCommand( "playdemo", CL_PlayDemo_f );
	Cmd_AddCommand( "viewdemo", CL_ViewDemo_f );
	Cmd_AddCommand( "timedemo", CL_TimeDemo_f );
	Cmd_AddCommand( "gg", CL_GameGauge_f );
	Cmd_AddCommand( "listdemo", CL_ListDemo_f );
	Cmd_AddCommand( "appenddemo", CL_AppendDemo_f );
	Cmd_AddCommandWithFlags( "removedemo", CL_RemoveDemo_f, CMD_FLAG_FILTERED );
	Cmd_AddCommand( "swapdemo", CL_SwapDemo_f );
	Cmd_AddCommand( "setdemoinfo", CL_SetDemoInfo_f );
	Cmd_AddCommand( "listen", CL_Listen_f );
	Cmd_AddCommand( "commentator", CL_Commentator_f );
	Cmd_AddCommand( "waveplaylen", CL_WavePlayLen_f );
	Cmd_AddCommand( "snapshot", CL_TakeSnapshot_f );
	Cmd_AddCommand( "startmovie", CL_StartMovie_f );
	Cmd_AddCommand( "endmovie", CL_EndMovie_f );
	Cmd_AddCommand( "entities", CL_PrintEntities_f );
	Cmd_AddCommandWithFlags( "rcon", CL_Rcon_f, CMD_FLAG_PRIVILEGED );
	Cmd_AddCommand( "cl_messages", CL_DumpMessageLoad_f );
	Cmd_AddCommand( "pingservers", CL_PingServers_f );
	Cmd_AddCommand( "slist", CL_Slist_f );
	Cmd_AddCommand( "list", CL_ListCachedServers_f );
	Cmd_AddCommand( "upload", CL_BeginUpload_f );

	CL_InitPrediction();

	Cmd_AddCommand( "httpstop", CL_HTTPCancel_f );
	Cmd_AddCommand( "spec_pos", CL_SpecPos_f );

	cls.datagram.data = cls.datagram_buf;
	cls.datagram.cursize = 0;
	cls.datagram.maxsize = sizeof( cls.datagram_buf );
	cls.datagram.flags = 0;
	cls.datagram.buffername = "cls.datagram";

	cls.isVAC2Secure = FALSE;
	cls.GameServerSteamID = 0;
	cls.build_num = 0;

	Q_memset( &cl, 0, sizeof( cl ) );
	cl.resourcesneeded.pPrev = &cl.resourcesneeded;
	cl.resourcesneeded.pNext = &cl.resourcesneeded;
	cl.resourcesonhand.pPrev = &cl.resourcesonhand;
	cl.resourcesonhand.pNext = &cl.resourcesonhand;

	DELTA_RegisterDescription( "clientdata_t" );
	DELTA_RegisterDescription( "entity_state_t" );
	DELTA_RegisterDescription( "entity_state_player_t" );
	DELTA_RegisterDescription( "custom_entity_state_t" );
	DELTA_RegisterDescription( "usercmd_t" );
	DELTA_RegisterDescription( "weapon_data_t" );
	DELTA_RegisterDescription( "event_t" );
}
#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void CL_SendResourceListBlock()
{
	byte data[65536];
	sizebuf_t msg;
	int servercount, start, i;
	resource_t* p;

	ContinueLoadingProgressBar( "ClientConnect", 6, 0.0f );

	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "Client to Server Resource Block";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof( data );

	if ( cls.state < ca_connecting )
	{
		Con_Printf( "custom resource list not valid -- not connected\n" );
		return;
	}

	servercount = MSG_ReadLong();
	if ( !cls.demoplayback && servercount != cl.servercount )
	{
		Con_Printf( "CL_SendResourceListBlock_f from different level\n" );
		return;
	}

	start = MSG_ReadLong();
	if ( start < 0 || start > cl.num_resources )
	{
		Con_Printf( "custom resource list request out of range\n" );
		return;
	}

	MSG_WriteByte( &msg, clc_resourcelist );
	MSG_WriteShort( &msg, cl.num_resources );

	for ( i = start; i < cl.num_resources; i++ )
	{
		p = &cl.resourcelist[i];

		MSG_WriteString( &msg, p->szFileName );
		MSG_WriteByte( &msg, p->type );
		MSG_WriteShort( &msg, p->nIndex );
		MSG_WriteLong( &msg, p->nDownloadSize );
		MSG_WriteByte( &msg, p->ucFlags );

		if ( p->ucFlags & RES_CUSTOM )
			SZ_Write( &msg, p->rgucMD5_hash, sizeof( p->rgucMD5_hash ) );
	}

	if ( msg.cursize > 0 )
	{
		Netchan_CreateFragments( FALSE, &cls.netchan, &msg );
		Netchan_FragSend( &cls.netchan );
	}
}

void CL_GameDir_f()
{
	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "gamedir is %s\n", com_gamedir );
		return;
	}

	CL_Disconnect();

	if ( Host_IsServerActive() )
		Host_ShutdownServer( FALSE );

	Con_Printf( "Setting gamedir to %s\n", Cmd_Argv( 1 ) );
	CL_CheckGameDirectory( (char*)Cmd_Argv( 1 ) );
}

void CL_FullServerinfo_f()
{
	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "usage: fullserverinfo <complete info string>\n" );
		return;
	}

	Q_strncpy( cl.serverinfo, Cmd_Argv( 1 ), sizeof( cl.serverinfo ) - 1 );
	cl.serverinfo[sizeof( cl.serverinfo ) - 1] = 0;
}

void CL_Commentator_f()
{
	const char* value;
	int enabled;

	if ( Cmd_Argc() < 2 )
	{
		value = Info_ValueForKey( cls.userinfo, "*hltv" );
		if ( Q_strlen( value ) && Q_atoi( value ) == HLTV_COMMENTATOR )
			Con_Printf( "HLTV commentator mode enabled.\n" );
		else
			Con_Printf( "HLTV commentator mode disabled.\n" );
		return;
	}

	enabled = Q_atoi( Cmd_Args() );
	if ( enabled != 0 && enabled != 1 )
		return;

	Info_SetValueForStarKey( cls.userinfo, "*hltv", va( "%i", enabled ? HLTV_COMMENTATOR : 0 ), sizeof( cls.userinfo ) );
}

/*
==============
CL_PrintEntities_f
==============
*/
void CL_PrintEntities_f()
{
	int i;
	cl_entity_t* ent;

	for ( i = 0; i < cl.num_entities; i++ )
	{
		Con_Printf( "%3i:", i );

		ent = &cl_entities[i];
		if ( !ent->model )
			continue;

		Con_Printf( "%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n", ent->model->name, ent->curstate.frame,
			ent->origin[0], ent->origin[1], ent->origin[2], ent->angles[0], ent->angles[1], ent->angles[2] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Broadcast pings to any servers that we can see on our LAN
//-----------------------------------------------------------------------------
void CL_PingServers_f()
{
	netadr_t adr;
	int i;

	// send a broadcast packet
	Con_Printf( "Searching for local servers...\n" );

	cls.slist_time = (float)realtime;

	NET_Config( TRUE ); // allow remote

	Q_memset( &adr, 0, sizeof( adr ) );

	for ( i = 0; i < 10; i++ )
	{
		adr.port = BigShort( (unsigned short)( i + PORT_SERVER ) );

		if ( !noip )
		{
			adr.type = NA_BROADCAST;
			Netchan_OutOfBandPrint( NS_CLIENT, adr, "%c%s", 'T', "Source Engine Query" );
		}
	}
}

void CL_BeginUpload_f()
{
	const char* filename;
	byte* buf;
	int size;
	resource_t custResource;
	byte md5[MD5_DIGEST_BYTES];
	byte digest[MD5_DIGEST_BYTES];
	MD5Context_t ctx;

	buf = NULL;
	size = 0;

	filename = Cmd_Argv( 1 );
	if ( !filename || !filename[0] )
	{
		Con_Printf( "Upload without filename\n" );
		return;			// framerate is too high
	}

	if ( !cl_allow_upload.value )
	{
		Con_Printf( "Ingoring decal upload ( cl_allow_upload is 0 )\n" );
		return;
	}

	if ( Q_strlen( filename ) != 4 + MD5_DIGEST_BYTES * 2 || Q_strnicmp( filename, "!MD5", 4 ) )
	{
		Con_Printf( "Ingoring upload of non-customization\n" );
		return;
	}

	ContinueLoadingProgressBar( "ClientConnect", 10, 0.0f );

	Q_memset( &custResource, 0, sizeof( custResource ) );
	COM_HexConvert( filename + 4, MD5_DIGEST_BYTES * 2, md5 );

	if ( HPAK_ResourceForHash( "custom.hpk", md5, &custResource ) )
	{
		if ( Q_memcmp( md5, custResource.rgucMD5_hash, sizeof( md5 ) ) )
		{
			Con_DPrintf( "Bogus data retrieved from custom.hpk, attempting to delete entry\n" );
			HPAK_RemoveLump( "custom.hpk", &custResource );
			return;
		}

		if ( HPAK_GetDataPointer( "custom.hpk", &custResource, &buf, &size ) )
		{
			Q_memset( &ctx, 0, sizeof( ctx ) );
			MD5Init( &ctx );
			MD5Update( &ctx, buf, size );
			MD5Final( digest, &ctx );

			if ( Q_memcmp( custResource.rgucMD5_hash, digest, sizeof( digest ) ) )
			{
				Con_Printf( "HPAK_AddLump called with bogus lump, md5 mismatch\n" );
				Con_Printf( "Purported:  %s\n", MD5_Print( custResource.rgucMD5_hash ) );
				Con_Printf( "Actual   :  %s\n", MD5_Print( digest ) );
				Con_Printf( "Removing conflicting lump\n" );
				HPAK_RemoveLump( "custom.hpk", &custResource );
				return;
			}
		}
	}

	if ( buf && size )
	{
		Netchan_CreateFileFragmentsFromBuffer( FALSE, &cls.netchan, (char*)filename, buf, size );
		Netchan_FragSend( &cls.netchan );
		free( buf );
	}
	else
	{
		Con_Printf( "Ingoring customization upload, couldn't find decal locally\n" );
	}
}

cvar_t cl_nodelta = { "cl_nodelta", "0" };

static float g_flInterpolationAmount = 0.1f;
static double frametime_remainder;

void CL_ComputePacketLoss()
{
	int i, lost, count;

	if ( realtime < cls.packet_loss_recalc_time )
		return;

	cls.packet_loss_recalc_time = realtime + 1.0;

	lost = 0;
	count = 0;
	// Compuate packet loss
	// Fill in frame data
	for ( i = cls.netchan.incoming_sequence - CL_UPDATE_BACKUP + 1; i <= cls.netchan.incoming_sequence; i++ )
	{
		if ( cl.frames[i & CL_UPDATE_MASK].receivedtime == -1.0 )
			lost++;
		count++;
	}

	if ( count <= 0 )
		cls.packet_loss = 0.0f;
	else
		cls.packet_loss = (float)( lost * 100.0 / count );
}

int CL_DriftInterpolationAmount( int goal )
{
	float fgoal, maxmove, diff;
	int msec;

	fgoal = (float)goal / 1000.0f;

	if ( fgoal != g_flInterpolationAmount )
	{
		maxmove = (float)( host_frametime * 0.05f );
		diff = fgoal - g_flInterpolationAmount;

		if ( diff > 0 )
		{
			if ( maxmove < diff )
				diff = maxmove;
		}
		else
		{
			if ( maxmove < -diff )
				diff = -maxmove;
		}

		g_flInterpolationAmount += diff;
	}

	msec = (int)( g_flInterpolationAmount * 1000.0f );
	if ( msec < 0 )
		msec = 0;
	if ( msec > MAX_INTERP_MSEC )
		msec = MAX_INTERP_MSEC;

	return msec;
}

void CL_ComputeClientInterpolationAmount( usercmd_t* cmd )
{
	float updaterate;
	int min_interp, max_interp, interp_msec;
	qboolean forced;

	updaterate = cl_updaterate.value;

	if ( updaterate < 10.0f )
	{
		Cvar_Set( "cl_updaterate", "20" );
		Con_Printf( "cl_updaterate minimum is %f, resetting to default (20)\n", 10.0 );
		updaterate = cl_updaterate.value;
	}

	if ( updaterate > 102.0f )
	{
		Cvar_Set( "cl_updaterate", "102" );
		Con_Printf( "cl_updaterate maximum is %f, resetting to maximum (102)\n", 102.0 );
		updaterate = cl_updaterate.value;
	}

	max_interp = cls.spectator ? 200 : MAX_INTERP_MSEC;

	min_interp = 50;
	if ( updaterate > 0.0f )
	{
		min_interp = (int)( 1000.0f / updaterate );
		if ( min_interp < 1 )
			min_interp = 1;
	}

	interp_msec = (int)( ex_interp.value * 1000.0f );

	forced = FALSE;
	if ( interp_msec + 1 < min_interp )
	{
		interp_msec = min_interp;
		Con_Printf( "ex_interp forced up to %i msec\n", interp_msec );
		forced = TRUE;
	}
	else if ( interp_msec - 1 > max_interp )
	{
		interp_msec = max_interp;
		Con_Printf( "ex_interp forced down to %i msec\n", interp_msec );
		forced = TRUE;
	}

	if ( forced )
		Cvar_Set( "ex_interp", va( "%f", (float)interp_msec / 1000.0f ) );

	if ( interp_msec < min_interp )
		interp_msec = min_interp;
	if ( interp_msec > max_interp )
		interp_msec = max_interp;

	cmd->lerp_msec = (short)CL_DriftInterpolationAmount( interp_msec );
}

qboolean UserIsConnectedOnLoopback()
{
	return cls.netchan.remote_address.type == NA_LOOPBACK;
}
void CL_Move()
{
	sizebuf_t buf;
	byte data[2048];
	cmd_t* pcmd;
	usercmd_t nullcmd;
	usercmd_t *from, *to;
	int i, numbackup, newcmds;
	int key, len, size;
	int lost, msec;
	double frametime_msec;
	float cmdrate;
	qboolean active, send;

	if ( cls.state <= ca_connecting )
		return;

	if ( cls.state == ca_connected )
	{
		if ( !cls.passive )
			Netchan_Transmit( &cls.netchan, 0, NULL );
		return;
	}

	CL_ComputePacketLoss();

	numbackup = (int)cl_cmdbackup.value;
	if ( numbackup < 0 )
		numbackup = 0;
	if ( numbackup > MAX_BACKUP_COMMANDS )
		numbackup = MAX_BACKUP_COMMANDS;

	buf.buffername = "Client Move";
	buf.flags = 0;
	buf.data = data;
	buf.maxsize = sizeof( data );
	buf.cursize = 0;

	pcmd = &cl.commands[cls.netchan.outgoing_sequence & CL_UPDATE_MASK];
	pcmd->senttime = (float)realtime;
	pcmd->receivedtime = -1.0f;
	pcmd->processedfuncs = FALSE;
	// Assume we'll transmit this command this frame ( rather than queueing it for next frame )
	pcmd->heldback = FALSE;
	pcmd->sendsize = 0;
	Q_memset( &pcmd->cmd, 0, sizeof( pcmd->cmd ) );

	active = cls.signon == SIGNONS;

	CL_SetSolidEntities();
	CL_PushPMStates();
	CL_SetSolidPlayers( cl.playernum );

	ClientDLL_CreateMove( (float)host_frametime, &pcmd->cmd, active );

	CL_PopPMStates();

	CL_ComputeClientInterpolationAmount( &pcmd->cmd );

	pcmd->cmd.lightlevel = (byte)cl.light_level;

	MSG_WriteByte( &buf, clc_move );

	key = buf.cursize + 1;
	MSG_WriteByte( &buf, 0 );
	MSG_WriteByte( &buf, 0 );

	lost = (int)cls.packet_loss;
	if ( lost < 0 )
		lost = 0;
	if ( lost > 100 )
		lost = 100;

	if ( Voice_GetLoopback() )
		lost |= 0x80;

	MSG_WriteByte( &buf, lost );

	frametime_msec = host_frametime * 1000.0;
	msec = (int)frametime_msec;
	frametime_remainder += frametime_msec - msec;

	if ( frametime_remainder > 1.0 )
	{
		msec++;
		frametime_remainder -= 1.0;
	}

	if ( msec > 255 )
		msec = 255;

	pcmd->cmd.msec = (byte)msec;

	cl.cmd = pcmd->cmd;

	if ( cls.passive || cls.demoplayback )
		return;

	CL_PredictMove( FALSE );

	cmdrate = cl_cmdrate.value;
	if ( cmdrate < 10.0f )
	{
		Cvar_Set( "cl_cmdrate", "10" );
		cmdrate = cl_cmdrate.value;
	}

	if ( cls.spectator && cls.state == ca_active && cl.delta_sequence == cl.validsequence
		&& !( cls.demorecording && cls.demowaiting ) && cls.nextcmdtime + 1.0f > realtime )
		return;

	send = FALSE;
	if ( msec > 0 )
	{
		if ( cl.maxclients == 1 || ( UserIsConnectedOnLoopback() && !host_limitlocal.value ) )
		{
			send = TRUE;
		}
		else if ( realtime >= cls.nextcmdtime && Netchan_CanPacket( &cls.netchan ) )
		{
			send = TRUE;
			cmdrate = cl_cmdrate.value;
		}
	}

	if ( send )
	{
		if ( cmdrate > 0.0f )
			cls.nextcmdtime = (float)( realtime + 1.0f / cmdrate );
		else
			cls.nextcmdtime = (float)realtime;

		if ( cls.lastoutgoingcommand == -1 )
			cls.lastoutgoingcommand = cls.netchan.outgoing_sequence;

		if ( cls.spectator )
		{
			SZ_Clear( &buf );
		}
		else
		{
			MSG_WriteByte( &buf, numbackup );

			newcmds = cls.netchan.outgoing_sequence - cls.lastoutgoingcommand;
			if ( cls.build_num < 5971 )
			{
				if ( newcmds > MAX_LEGACY_COMMANDS )
					newcmds = MAX_LEGACY_COMMANDS;
			}
			else
			{
				if ( newcmds > MAX_TOTAL_COMMANDS - numbackup )
					newcmds = MAX_TOTAL_COMMANDS - numbackup;
			}

			MSG_WriteByte( &buf, newcmds );

			Q_memset( &nullcmd, 0, sizeof( nullcmd ) );
			from = &nullcmd;
			for ( i = newcmds + numbackup - 1; i >= 0; i-- )
			{
				to = &cl.commands[( cls.netchan.outgoing_sequence - i ) & CL_UPDATE_MASK].cmd;
				MSG_WriteUsercmd( &buf, to, from );
				from = to;
			}

			// calculate a checksum over the move commands
			len = buf.cursize - key - 1;
			size = len;
			if ( size > 255 )
				size = 255;

			buf.data[key - 1] = (byte)size;
			buf.data[key] = COM_BlockSequenceCRCByte( &buf.data[key + 1], len, cls.netchan.outgoing_sequence );

			COM_Munge( &buf.data[key + 1], size, cls.netchan.outgoing_sequence );
		}

		if ( cls.state == ca_active && cls.signon == SIGNONS
			&& cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged >= CL_UPDATE_MASK
			&& realtime - cls.netchan.last_received > 15.0 )
		{
			con_nprint_t np = { 0, 1.0f, { 1.0f, 0.2f, 0.0f } };

			Con_NXPrintf( &np, "WARNING:  Connection Problem" );
			cl.validsequence = 0;
			cl.delta_sequence = -1;
		}
		else if ( cl.validsequence && !cl_nodelta.value && cls.state == ca_active
			&& !( cls.demorecording && cls.demowaiting ) )
		{
			// Request delta compression of entities
			cl.delta_sequence = cl.validsequence;
			MSG_WriteByte( &buf, clc_delta );
			MSG_WriteByte( &buf, cl.validsequence );
		}
		else
		{
			cl.delta_sequence = -1;
		}

		// Remember outgoing command that we are sending
		cls.lastoutgoingcommand = cls.netchan.outgoing_sequence;

		// Update size counter for netgraph
		cl.commands[cls.netchan.outgoing_sequence & CL_UPDATE_MASK].sendsize = buf.cursize;

		CL_AddVoiceToDatagram( FALSE );

		if ( cls.datagram.cursize <= buf.maxsize - buf.cursize )
		{
			Q_memcpy( buf.data + buf.cursize, cls.datagram.data, cls.datagram.cursize );
			buf.cursize += cls.datagram.cursize;
		}

		cls.datagram.cursize = 0;

		Netchan_Transmit( &cls.netchan, buf.cursize, buf.data );
	}
	else
	{
		// Mark command as held back so we'll send it next time
		cl.commands[cls.netchan.outgoing_sequence & CL_UPDATE_MASK].heldback = TRUE;
		cls.netchan.outgoing_sequence++;
	}

	// Update download/upload slider.
	Netchan_UpdateProgress( &cls.netchan );
}

/*
===============
CL_AllocDlight

===============
*/
dlight_t* CL_AllocDlight( int key )
{
	int i;
	dlight_t* dl;

	i = MAX_DLIGHTS;

	// first look for an exact key match
	if ( key )
	{
		for ( i = 0; i < MAX_DLIGHTS; i++ )
		{
			if ( cl_dlights[i].key == key )
				break;
		}
	}

	// then look for anything else
	if ( i == MAX_DLIGHTS )
	{
		for ( i = 0; i < MAX_DLIGHTS; i++ )
		{
			if ( cl.time > cl_dlights[i].die )
				break;
		}
	}

	if ( i == MAX_DLIGHTS )
		i = 0;

	dl = &cl_dlights[i];
	Q_memset( dl, 0, sizeof( *dl ) );
	dl->key = key;

	r_dlightchanged |= 1 << i;
	r_dlightactive |= 1 << i;
	return dl;
}

/*
===============
CL_AllocElight

===============
*/
dlight_t* CL_AllocElight( int key )
{
	int i;
	dlight_t* dl;

	i = MAX_ELIGHTS;

	// first look for an exact key match
	if ( key )
	{
		for ( i = 0; i < MAX_ELIGHTS; i++ )
		{
			if ( cl_elights[i].key == key )
				break;
		}
	}

	// then look for anything else
	if ( i == MAX_ELIGHTS )
	{
		for ( i = 0; i < MAX_ELIGHTS; i++ )
		{
			if ( cl.time > cl_elights[i].die )
				break;
		}
	}

	if ( i == MAX_ELIGHTS )
		i = 0;

	dl = &cl_elights[i];
	Q_memset( dl, 0, sizeof( *dl ) );
	dl->key = key;
	return dl;
}

void SetupStartupTimings()
{
	g_iCurrentTiming = 0;
	g_StartupTimings[0].name = "Startup";
	g_StartupTimings[0].time = (float)Sys_FloatTime();
}

void PrintStartupTimings()
{
	int i;

	Con_Printf( "Startup timings (%.2f total)\n", g_StartupTimings[g_iCurrentTiming].time - g_StartupTimings[0].time );
	Con_Printf( "    0.00    Startup\n" );

	for ( i = 1; i <= g_iCurrentTiming; i++ )
	{
		Con_Printf( "    %.2f    %s\n", g_StartupTimings[i].time - g_StartupTimings[i - 1].time,
			g_StartupTimings[i].name );
	}
}

void CL_CheckClientState()
{
	unsigned short port;
	const unsigned char* ip;
	int address;

	if ( ( cls.state != ca_connected && cls.state != ca_uninitialized ) || cls.signon != SIGNONS )
		return;

	cls.state = ca_active;

	if ( fs_startup_timings.value )
		PrintStartupTimings();

	port = cls.netchan.remote_address.port;
	ip = cls.netchan.remote_address.ip;

	if ( !port )
	{
		port = net_local_adr.port;
		ip = net_local_adr.ip;
	}

	memcpy( &address, ip, sizeof( address ) );
	VGuiWrap2_NotifyOfServerConnect( com_gamedir, address, (unsigned short)Q_ntohs( (short)port ) );
}

void CL_UpdateSoundFade()
{
	int nTotal;
	float fGap;
	float f;

	// Assume no fading remains
	cls.soundfade.nClientSoundFadePercent = 0;

	nTotal = cls.soundfade.soundFadeHoldTime + cls.soundfade.soundFadeOutTime + cls.soundfade.soundFadeInTime;

	if ( !( realtime < (float)nTotal + cls.soundfade.soundFadeStartTime ) )
		return;

	fGap = (float)( realtime - cls.soundfade.soundFadeStartTime );

	// We are in the fade time, so determine amount of fade.
	if ( cls.soundfade.soundFadeOutTime && fGap < cls.soundfade.soundFadeOutTime )
	{
		// Ramp up
		f = fGap / cls.soundfade.soundFadeOutTime;
	}
	else if ( fGap < cls.soundfade.soundFadeOutTime + cls.soundfade.soundFadeHoldTime )
	{
		// Stay
		f = 1.0;
	}
	else
	{
		// Ramp down
		f = 1.0f - ( fGap - ( cls.soundfade.soundFadeOutTime + cls.soundfade.soundFadeHoldTime ) )
			/ cls.soundfade.soundFadeOutTime;
	}

	cls.soundfade.nClientSoundFadePercent = (int)( cls.soundfade.nStartPercent * f );
}

void CL_ParseServerList()
{
	int i, j, count;
	unsigned char ip[4];
	char szAddress[128];
	unsigned short port;

	MSG_ReadByte();

	count = ( net_message.cursize - 6 ) / 6;

	for ( i = 1; i <= count; i++ )
	{
		Q_memset( szAddress, 0, sizeof( szAddress ) );

		for ( j = 0; j < 4; j++ )
			ip[j] = (unsigned char)MSG_ReadByte();

		snprintf( szAddress, sizeof( szAddress ), "%i.%i.%i.%i", ip[0], ip[1], ip[2], ip[3] );

		port = (unsigned short)BigShort( (short)MSG_ReadShort() );

		if ( i <= 100 )
			Con_Printf( "%4i:  %s:%i\n", i, szAddress, port );
	}

	Con_Printf( "%i total servers\n", count > 0 ? count + 1 : 1 );
}

void CL_ParseBatchModList()
{
	int nServers;
	int nServerCount, nOtherCount;
	char* text;
	char szMod[256];
	char szRequest[128];

	MSG_ReadByte();

	nServers = 0;
	szMod[0] = 0;

	text = MSG_ReadString();
	while ( text[0] && Q_stricmp( text, "end-of-list" ) && Q_stricmp( text, "more-in-list" ) )
	{
		nServers++;

		Q_strncpy( szMod, text, sizeof( szMod ) - 1 );
		szMod[sizeof( szMod ) - 1] = 0;

		nServerCount = Q_atoi( MSG_ReadString() );
		nOtherCount = Q_atoi( MSG_ReadString() );
		Con_Printf( "%3i %5i %s\n", nServerCount, nOtherCount, szMod );

		text = MSG_ReadString();
	}

	Con_Printf( "%i servers\n", nServers );

	if ( !text[0] || Q_stricmp( text, "more-in-list" ) )
	{
		Con_Printf( "Done.\n" );
		return;
	}

	snprintf( szRequest, sizeof( szRequest ), "%c\r\n%s\r\n", 'x', szMod );
	Con_DPrintf( "Requesting next batch ( >%s ) of mods from %s\n", szMod, NET_AdrToString( net_from ) );
	NET_SendPacket( NS_CLIENT, Q_strlen( szRequest ) + 1, szRequest, net_from );
}

void CL_ParseBatchUserList()
{
	static int count = 1;
	int i, j, entries;
	int hash, unique, len;
	unsigned char ip[4];
	char szAddress[128];
	char packet[16];
	unsigned short port;

	MSG_ReadByte();

	hash = MSG_ReadLong();
	unique = MSG_ReadLong();

	entries = ( net_message.cursize - msg_readcount ) / 6;

	for ( i = 0; i < entries; i++ )
	{
		Q_memset( szAddress, 0, sizeof( szAddress ) );

		for ( j = 0; j < 4; j++ )
			ip[j] = (unsigned char)MSG_ReadByte();

		snprintf( szAddress, sizeof( szAddress ), "%i.%i.%i.%i", ip[0], ip[1], ip[2], ip[3] );

		port = (unsigned short)BigShort( (short)MSG_ReadShort() );

		Con_Printf( "%4i:  %s:%i\n", count, szAddress, port );
		count++;
	}

	if ( unique && hash )
	{
		Q_strcpy( packet, "users" );
		len = Q_strlen( "users" );
		memcpy( packet + len + 1, &hash, sizeof( hash ) );
		memcpy( packet + len + 1 + sizeof( hash ), &unique, sizeof( unique ) );

		Con_DPrintf( "Requesting next batch ( %i ) user list from %s\n", unique, NET_AdrToString( net_from ) );
		NET_SendPacket( NS_CLIENT, len + 9, packet, net_from );
		return;
	}

	Con_Printf( "%i users\n", count );
	count = 0;
	Con_Printf( "Done.\n" );
}
/*
=======================
CL_ClientDllQuery

=======================
*/
int CL_ClientDllQuery( const char* text )
{
	byte data[2048];
	int len, ret;

	if ( !Host_IsServerActive() || Host_IsSinglePlayerGame() )
		return 0;

	Q_memset( data, 0, sizeof( data ) );

	len = (int)sizeof( data ) - 4;
	ret = ClientDLL_ConnectionlessPacket( &net_from, text, (char*)( data + 4 ), &len );

	if ( len <= (int)sizeof( data ) - 4 && len )
	{
		*(int*)data = -1;
		NET_SendPacket( NS_CLIENT, len + 4, data, net_from );
	}

	return ret;
}

void CL_SendRcon( netadr_t* to, unsigned int challenge )
{
	char message[1152];

	if ( !g_lastrconcommand[0] )
	{
		Con_Printf( "Rcon:  Empty rcon string\n" );
		return;
	}

	if ( !NET_CompareAdr( *to, g_rconaddress ) )
	{
		Con_Printf( "Rcon:  Challenge from spoofed/invalid IP address\n" );
		return;
	}

	snprintf( message, sizeof( message ), "rcon %u \"%s\" %s", challenge, rcon_password.string, g_lastrconcommand );
	Netchan_OutOfBandPrint( NS_CLIENT, *to, "%s", message );
}

void CL_CreateResponse( netadr_t* to, const char* varname, const char* value )
{
	char response[1024];

	if ( value )
		snprintf( response, sizeof( response ), "%c%c%c%c\"%s\"=\"%s\"\n", 255, 255, 255, 255, varname, value );
	else
		snprintf( response, sizeof( response ), "%c%c%c%c\"%s\"\n", 255, 255, 255, 255, varname );

	NET_SendPacket( NS_CLIENT, Q_strlen( response ) + 1, response, *to );
}

void CL_ServiceGet()
{
	const char* name;
	cvar_t* var;

	if ( net_from.type != NA_IP || Cmd_Argc() != 2 )
		return;

	if ( net_from.ip[0] != 127 || net_from.ip[1] != 0 || net_from.ip[2] != 0 || net_from.ip[3] != 1 )
		return;

	name = Cmd_Argv( 1 );
	if ( !name || !name[0] )
		return;

	if ( !Q_stricmp( "clientaddress", name ) )
	{
		CL_CreateResponse( &net_from, name, va( "%s", cls.trueaddress ) );
		return;
	}

	if ( !Q_stricmp( "serveraddress", name ) )
	{
		CL_CreateResponse( &net_from, name, va( "%s", NET_AdrToString( cls.netchan.remote_address ) ) );
		return;
	}

	if ( !Q_stricmp( "userid", name ) )
	{
		CL_CreateResponse( &net_from, name, va( "%i", cls.userid ) );
		return;
	}

	var = Cvar_FindVar( name );
	if ( !var )
	{
		CL_CreateResponse( &net_from, name, "unknown variable" );
		return;
	}

	CL_CreateResponse( &net_from, name, va( "%s", var->string ) );
}

const char* CL_CleanFileName( const char* filename )
{
	if ( !filename )
		return NULL;

	if ( filename[0] == '!' )
		return "customization";

	return filename;
}

void CL_RegisterCustomization( resource_t* resource )
{
	customization_t* pList;

	for ( pList = cl.players[resource->playernum].customdata.pNext; pList; pList = pList->pNext )
	{
		if ( !Q_memcmp( pList->resource.rgucMD5_hash, resource->rgucMD5_hash, sizeof( resource->rgucMD5_hash ) ) )
		{
			Con_DPrintf( "Duplicate resource received and ignored.\n" );
			return;
		}
	}

	if ( !COM_CreateCustomization( &cl.players[resource->playernum].customdata, resource, resource->playernum,
			 FCUST_FROMHPAK, NULL, NULL ) )
		Con_Printf( "Unable to create custom decal for player %i\n", resource->playernum );
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
qboolean CL_GetMessage()
{
	if ( cls.demoplayback )
		return CL_ReadDemoMessage();

	if ( cls.passive )
		return NET_GetPacket( NS_MULTICAST );

	return NET_GetPacket( NS_CLIENT );
}

resource_t* CL_AddResource( resourcetype_t type, char* name, int size, qboolean bFatalIfMissing, int index )
{
	resource_t* r;

	r = &cl.resourcelist[cl.num_resources];

	if ( cl.num_resources >= MAX_RESOURCES )
		Sys_Error( "Too many resources on client." );

	cl.num_resources++;

	r->type = type;
	Q_strncpy( r->szFileName, name, sizeof( r->szFileName ) - 1 );
	r->szFileName[sizeof( r->szFileName ) - 1] = 0;
	r->nDownloadSize = size;

	if ( bFatalIfMissing )
		r->ucFlags |= RES_FATALIFMISSING;

	r->nIndex = index;
	return r;
}

void CL_CreateResourceList()
{
	char szFileName[MAX_PATH];
	byte rgucMD5_hash[MD5_DIGEST_BYTES];
	FileHandle_t fp;
	int nSize;
	resource_t* pNewResource;

	if ( cls.state == ca_dedicated )
		return;

	HPAK_FlushHostQueue();

	cl.num_resources = 0;

	Q_strcpy( szFileName, "tempdecal.wad" );
	Q_memset( rgucMD5_hash, 0, sizeof( rgucMD5_hash ) );

	fp = FS_OpenPathID( szFileName, "rb", "GAMECONFIG" );
	if ( !fp )
		return;

	nSize = FS_Size( fp );
	MD5_Hash_File( rgucMD5_hash, szFileName, FALSE, FALSE, NULL );

	if ( nSize )
	{
		pNewResource = CL_AddResource( t_decal, szFileName, nSize, FALSE, 0 );
		pNewResource->ucFlags |= RES_CUSTOM;
		Q_memcpy( pNewResource->rgucMD5_hash, rgucMD5_hash, sizeof( rgucMD5_hash ) );

		HPAK_AddLump( FALSE, "custom.hpk", pNewResource, NULL, fp );
	}

	FS_Close( fp );
}

void CL_ClearCaches()
{
	int i;
	event_t* ev;

	for ( i = 1; i < MAX_EVENT_SCRIPTS; i++ )
	{
		ev = &cl.event_precache[i];
		if ( !ev->pszScript )
			break;

		Mem_Free( (char*)ev->pszScript );
		Mem_Free( (char*)ev->filename );
		Q_memset( ev, 0, sizeof( *ev ) );
	}
}

void CL_Listen_0()
{
	if ( cls.demoplayback )
		CL_StopPlayback();

	CL_Disconnect();

	Q_memset( msg_buckets, 0, sizeof( msg_buckets ) );
	Q_memset( total_data, 0, sizeof( total_data ) );

	cls.connect_retry = 0;
	gfExtendedError = FALSE;

	NET_Config( TRUE ); // allow remote

	Netchan_Setup( NS_MULTICAST, &cls.netchan, cls.connect_stream, -1, &cls, CL_GetFragmentSize );
	NET_FlushSocket( NS_MULTICAST );

	cls.passive = TRUE;

	if ( !cls.connect_stream.port )
		cls.connect_stream.port = BigShort( PORT_MULTICAST );

	NET_JoinGroup( cls.netchan.sock, cls.connect_stream );
	Con_Printf( "Joining multicast group %s ...\n", NET_AdrToString( cls.connect_stream ) );

	NET_ClearLagData( TRUE, FALSE );

	cl.validsequence = 0;
	cl.delta_sequence = -1;
	cls.signon = 0;
	cls.connect_time = realtime;
	cls.state = ca_connecting;
	cls.demonum = -1;
	cls.nextcmdtime = (float)realtime;
	cls.lastoutgoingcommand = -1;
}

void CL_ShutDownClientStatic()
{
	int i;

	for ( i = 0; i < CL_UPDATE_BACKUP; i++ )
	{
		if ( cl.frames[i].packet_entities.entities )
			Mem_Free( cl.frames[i].packet_entities.entities );

		cl.frames[i].packet_entities.entities = NULL;
	}

	Q_memset( cl.frames, 0, sizeof( frame_t ) * CL_UPDATE_BACKUP );
}

void CL_ConnectTest()
{
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void CL_SignonReply()
{
	char mapname[256];
	size_t len;

	mapname[0] = 0;

	Con_DPrintf( "CL_SignonReply: %i\n", cls.signon );

	if ( cls.signon == 1 )
	{
		Cache_Report(); // print remaining memory
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_WriteString( &cls.netchan.message, "sendents" );
		return;
	}

	if ( cls.signon != SIGNONS )
		return;

	SCR_EndLoadingPlaque(); // allow normal screen updates

	sscanf( cl.levelname, "maps/%s.bsp", mapname );
	len = strlen( mapname );
	if ( len > 4 && !strcmp( mapname + len - 4, ".bsp" ) )
		mapname[len - 4] = 0;

	FS_LogLevelLoadFinished( mapname );
	VGuiWrap2_LoadingFinished( "level", mapname );

	if ( g_bRedirectedToProxy && !cls.spectator )
	{
		Con_Printf( "Redirected to invalid server\n" );
		CL_Disconnect();

		if ( Host_IsServerActive() )
			Host_ShutdownServer( FALSE );
	}

	g_bRedirectedToProxy = 0;

	if ( cls.demoplayback )
		Sequence_OnLevelLoad( mapname );
}

/*
===============
SetPal

Debugging tool, just flashes the screen
===============
*/
void SetPal( int )
{
}

/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights()
{
	int i;
	dlight_t* dl;
	float frametime;

	if ( cls.signon != SIGNONS )
		return;

	frametime = (float)( cl.time - cl.oldtime );

	r_dlightchanged = 0;
	r_dlightactive = 0;

	for ( i = 0, dl = cl_dlights; i < MAX_DLIGHTS; i++, dl++ )
	{
		if ( !dl->radius )
			continue;

		if ( dl->die < cl.time )
		{
			r_dlightchanged |= 1 << i;
			dl->radius = 0;
			continue;
		}

		if ( !dl->decay )
		{
			r_dlightactive |= 1 << i;
			continue;
		}

		r_dlightchanged |= 1 << i;

		dl->radius -= dl->decay * frametime;
		if ( dl->radius < 0 )
			dl->radius = 0;
		else if ( dl->radius )
			r_dlightactive |= 1 << i;
	}

	for ( i = 0, dl = cl_elights; i < MAX_ELIGHTS; i++, dl++ )
	{
		if ( !dl->radius )
			continue;

		if ( dl->die < cl.time )
		{
			dl->radius = 0;
			continue;
		}

		dl->radius -= dl->decay * frametime;
		if ( dl->radius < 0 )
			dl->radius = 0;
	}
}

void CL_Shutdown()
{
	CL_FlushEventSystem();
	TextMessageShutdown();
	Net_APIShutDown();

	if ( cls.demoheader )
	{
		FS_Close( cls.demoheader );
		cls.demoheader = NULL;
	}

	if ( FS_FileExists( "demoheader.dmf" ) )
		FS_RemoveFile( "demoheader.dmf", NULL );
}

void CL_Rate_f()
{
	double newrate;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "Usage:  cl_rate <num>\nClient to server transmission rate (bytes/sec)\nCurrent:  %.5f\n",
			cls.netchan.rate );
		return;
	}

	newrate = Q_atof( Cmd_Argv( 1 ) );
	if ( !newrate )
	{
		newrate = 30000.0;
	}
	else if ( newrate < 1000.0 || newrate > 100000.0 )
	{
		Con_Printf( "cl_rate:  Maximum %f, Minimum %f\n", 100000.0, 1000.0 );
		return;
	}

	cls.netchan.rate = newrate;
}

void GetPos( vec3_t origin, vec3_t angles )
{
	entity_state_t* ps;

	VectorCopy( r_refdef.vieworg, origin );
	VectorCopy( r_refdef.viewangles, angles );

	if ( Cmd_Argc() == 2 && Q_atoi( Cmd_Argv( 1 ) ) == 2 && cls.state == ca_active )
	{
		ps = &cl.frames[cl.parsecountmod].playerstate[cl.playernum];
		VectorCopy( ps->origin, origin );
		VectorCopy( ps->angles, angles );
	}
}

void CL_SendKeepaliveToServer()
{
	sizebuf_t buf;
	byte data[32];
	int i;

	if ( cls.state == ca_disconnected )
		return;

	buf.cursize = 0;
	buf.buffername = "Client Keepalive";
	buf.maxsize = sizeof( data );
	buf.flags = 0;
	buf.data = data;

	for ( i = 0; i < (int)sizeof( data ); i++ )
		MSG_WriteByte( &buf, clc_nop );

	Netchan_Transmit( &cls.netchan, buf.cursize, buf.data );
}

void CL_KeepConnectionActive()
{
	static float flLastKeepaliveTime;
	float curtime;

	curtime = (float)Sys_FloatTime();

	if ( !( flLastKeepaliveTime <= curtime ) )
		flLastKeepaliveTime = curtime;

	if ( curtime - flLastKeepaliveTime > 5.0f )
	{
		CL_SendKeepaliveToServer();
		flLastKeepaliveTime = curtime;
	}
}

model_t* CL_GetWorldModel()
{
	return cl.worldmodel;
}

int CL_GetServerCount()
{
	return cl.servercount;
}

/*
===============
CL_TakeSnapshot_f

Generates a filename and calls the vidwin.c function to write a .bmp file
===============
*/
void CL_TakeSnapshot_f()
{
	int i;
	char base[64];
	char filename[64];
	FileHandle_t fp;

	if ( cl.num_entities && cl_entities->model )
		COM_FileBase( cl_entities->model->name, base );
	else
		Q_strcpy( base, "Snapshot" );

	// find a file name to save it to
	for ( i = 0; i < 1000; i++ )
	{
		snprintf( filename, sizeof( filename ), "%s%04d.bmp", base, i );

		fp = FS_OpenPathID( filename, "r", "GAMECONFIG" );
		if ( !fp )
		{
			VID_TakeSnapshot( filename );
			return;
		}

		FS_Close( fp );
	}

	Con_Printf( "Unable to take a screenshot.\n" );
}

/*
===============
CL_EndMovie_f

Ends frame dumping
===============
*/
void CL_EndMovie_f()
{
	if ( !cl_inmovie )
	{
		Con_Printf( "No movie started.\n" );
		return;
	}

	cl_inmovie = FALSE;
	host_framerate.value = 0.0f;
	Con_Printf( "Stopped recording movie...\n" );
}

void CL_Retry_f_0()
{
	char szCommand[MAX_PATH];

	snprintf( szCommand, sizeof( szCommand ), cls.passive ? "listen %s\n" : "connect %s\n", cls.servername );
	Cbuf_AddText( szCommand );
	Con_Printf( "Commencing connection retry to %s\n", cls.servername );
}

/*
===============
CL_StartMovie_f

Sets the engine up to dump frames
===============
*/
void CL_StartMovie_f()
{
	const char* filename;

	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "startmovie <filename> <fps>\n" );
		return;
	}

	if ( !cls.demoplayback )
	{
		Con_Printf( "startmovie only allowed during demo playback.\n" );
		return;
	}

	filename = Cmd_Argv( 1 );
	if ( filename[0] == '/' || Q_strstr( filename, ":" ) || Q_strstr( filename, ".." ) || Q_strstr( filename, "\\" ) )
	{
		Con_Printf( "Couldn't open %s (contains illegal characters).\n", filename );
		return;
	}

	cl_inmovie = TRUE;
	host_framerate.value = (float)( 1.0 / Q_atof( Cmd_Argv( 2 ) ) );

	VID_WriteBuffer( Cmd_Argv( 1 ) );

	Con_Printf( "Started recording movie...\n" );
}

void CL_PrintCachedServer_0( int slot )
{
	server_cache_t* server;
	const char* type;
	const char* os;

	if ( !cached_servers[slot].inuse )
		return;

	server = &cached_servers[slot];

	Con_Printf( "%d ", slot + 1 );
	Con_Printf( "Name: %s\n", server->name );
	Con_Printf( "Address: %s\n", NET_AdrToString( server->adr ) );
	Con_Printf( "Map: %s\n", server->map );
	Con_Printf( "Description: %s\n", server->desc );
	Con_Printf( "Players: %d/%d   Bots: %d\n", server->players, server->maxplayers, server->bots );

	if ( server->type == 'd' )
		type = "Dedicated";
	else if ( server->type == 'p' )
		type = "Proxy";
	else if ( server->type == 'l' )
		type = "Listen";
	else
		type = "Unknown";

	if ( server->os == 'w' )
		os = "Windows";
	else if ( server->os == 'l' )
		os = "Linux";
	else
		os = "macOS";

	Con_Printf( "%s - %s - %s - %s\n", type, os, server->is_private ? "Private" : "Public",
		server->is_vac ? "Secure" : "Insecure" );

	if ( server->has_spectator_address )
		Con_Printf( "Spectator Address: %s\n", NET_AdrToString( server->spec_adr ) );
}
