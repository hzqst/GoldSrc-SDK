// sv_main.c -- server main program
#include "common.h"
#include "filter.h"
#include "isteamgameserver_c_api.h"
#include "server.h"
#include "sv_main.h"
#include "sound.h"
#include "keys.h"
#include "../common/dll_state.h"
#include "engine_edict.h"
#include "usermsg.h"
#include "conprint.h"
#include "mem.h"
#include "protocol.h"
#include "server_static.h"
#include "host.h"
#include "zone.h"
#include "gl_model.h"
#include "cmodel.h"
#include "cmd.h"
#include "delta.h"
#include "sys.h"
#include "pm_defs.h"
#include "filesystem.h"
#include "decals.h"
#include "info.h"
#include "tmessage.h"
#include "unicode_strtools.h"
#include "crc.h"
#include "client.h"
#include "buildnum.h"
#include "hashpak.h"
#include "render.h"
#include "inst_baseline.h"
#include "modinfo.h"
#include "enginecallbacks.h"
#include "vgui_intwrap2.h"
#include "steam_api.h"
#include "../common/in_buttons.h"
#include "../pm_shared/pm_shared.h"
#include "../pm_shared/pm_movevars.h"
#include "eiface.h"

#include <cctype>
#include <cfenv>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <malloc.h>

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

#if defined( _WIN32 ) && !defined( SWDS_DLL )
#include <assert.h>
#if defined( NDEBUG )
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );
#endif
#endif

#define MSG_FL_BROADCAST 1
#define MSG_FL_ONE 0x80

#define SINGLEPLAYER_BACKUP 8
#define MULTIPLAYER_BACKUP 64

extern "C" int CheckIP( netadr_t adr );
void SV_ExecuteClientMessage( client_t* client );
void Steam_HandleIncomingPacket( byte* data, int length, unsigned int fromIp, unsigned short port );
void SV_ClientPrintf( const char* format, ... );
void SV_DropClient( client_t* client, qboolean crash, const char* format, ... );
int Netchan_CreateFileFragments( qboolean server, netchan_t* channel, char* filename );

extern cvar_t sv_voiceenable;
extern cvar_t mp_consistency;
extern cvar_t sv_unlag;
extern cvar_t sv_maxunlag;
extern cvar_t sv_unlagpush;
extern cvar_t sv_unlagsamples;
extern cvar_t sv_gravity;
extern cvar_t sv_friction;
extern cvar_t sv_edgefriction;
extern cvar_t sv_stopspeed;
extern cvar_t sv_maxspeed;
extern cvar_t sv_footsteps;
extern cvar_t sv_accelerate;
extern cvar_t sv_rollangle;
extern cvar_t sv_rollspeed;
extern cvar_t sv_stepsize;
extern cvar_t sv_bounce;
extern cvar_t sv_airaccelerate;
extern cvar_t sv_wateraccelerate;
extern cvar_t sv_waterfriction;
extern cvar_t sv_zmax;
extern cvar_t sv_wateramp;
extern cvar_t sv_skyname;
extern cvar_t sv_maxvelocity;
extern cvar_t sv_spectatormaxspeed;
extern cvar_t coop;
extern cvar_t deathmatch;
extern cvar_t host_limitlocal;
extern cvar_t skill;
extern cvar_t clockwindow;

server_t sv;
server_static_t svs;
client_t* host_client; // current client

int SV_UPDATE_BACKUP = SINGLEPLAYER_BACKUP;
int SV_UPDATE_MASK = SINGLEPLAYER_BACKUP - 1;

int num_servers;
int g_bIsCStrike;
int g_userid = 1;
qboolean g_bOutOfDateRestart;
char* gNullString = (char*)com_empty_string;
int g_bIsCZero;
int g_bIsTerrorStrike;
int g_bIsCZeroRitual;
int g_bCS_CZ_Flags_Initialized;
int g_bIsTFC;
int g_bIsHL1;
globalvars_t gGlobalVariables;
int gPacketSuppressed;

decalname_t sv_decalnames[MAX_BASE_DECALS];
int sv_decalnamecount;

cvar_t violence_hblood = { "violence_hblood", "1", FCVAR_ARCHIVE };

UserMsg* sv_gpNewUserMsgs;
UserMsg* sv_gpUserMsgs;
static int giNextUserMsg = 64;

int SV_PointLeafnum( vec_t* p )
{
	mleaf_t* leaf;

	leaf = Mod_PointInLeaf( p, sv.worldmodel );
	if ( leaf )
		return leaf - sv.worldmodel->leafs;

	return 0;
}

qboolean SV_ValidClientMulticast( client_t* client, int soundLeaf, int to )
{
	byte* mask;
	int flags;
	int bitNumber;

	flags = to & ~MSG_FL_ONE;
	if ( Host_IsSinglePlayerGame() || client->proxy || flags == MSG_FL_BROADCAST )
		return TRUE;

	if ( flags == MSG_FL_PVS )
		mask = CM_LeafPVS( soundLeaf );
	else if ( flags == MSG_FL_PAS )
		mask = CM_LeafPAS( soundLeaf );
	else
	{
		Con_Printf( "MULTICAST: Error %d!\n", to );
		return FALSE;
	}

	if ( !mask )
		return TRUE;

	bitNumber = SV_PointLeafnum( client->edict->v.origin ) - 1;

	if ( mask[bitNumber >> 3] & ( 1 << ( bitNumber & 7 ) ) )
		return TRUE;

	return FALSE;
}

void SV_Multicast( edict_t* ent, vec_t* origin, int to, qboolean reliable )
{
	client_t* save;
	client_t* client;
	sizebuf_t* buffer;
	int leafnum;
	int j;

	save = host_client;
	leafnum = SV_PointLeafnum( origin );

	if ( ent && ( !host_client || host_client->edict != ent ) )
	{
		for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
		{
			if ( client->edict == ent )
			{
				host_client = client;
				break;
			}
		}
	}

	for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
	{
		if ( !client->active )
			continue;

		if ( ( to & MSG_FL_ONE ) && client == host_client )
			continue;

		if ( ent && ent->v.groupinfo && client->edict->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND && !( ent->v.groupinfo & client->edict->v.groupinfo ) )
				continue;
			if ( g_groupop == GROUP_OP_NAND && ( ent->v.groupinfo & client->edict->v.groupinfo ) )
				continue;
		}

		if ( !SV_ValidClientMulticast( client, leafnum, to ) )
		{
			gPacketSuppressed += sv.multicast.cursize;
			continue;
		}

		buffer = reliable ? &client->netchan.message : &client->datagram;
		if ( buffer->maxsize - buffer->cursize > sv.multicast.cursize )
			SZ_Write( buffer, sv.multicast.data, sv.multicast.cursize );
	}

	SZ_Clear( &sv.multicast );
	host_client = save;
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

int fatbytes;
byte fatpvs[1024];
int fatpasbytes;
byte fatpas[1024];

void SV_AddToFatPVS( vec_t* org, mnode_t* node )
{
	int i;
	byte* pvs;
	mplane_t* plane;
	float d;

	while ( 1 )
	{
		// if this is a leaf, accumulate the pvs bits
		if ( node->contents < 0 )
		{
			if ( node->contents != CONTENTS_SOLID )
			{
				pvs = Mod_LeafPVS( (mleaf_t*)node, sv.worldmodel );
				for ( i = 0; i < fatbytes; i++ )
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		d = DotProduct( org, plane->normal ) - plane->dist;
		if ( d > 8 )
			node = node->children[0];
		else if ( d < -8 )
			node = node->children[1];
		else
		{
			// go down both
			SV_AddToFatPVS( org, node->children[0] );
			node = node->children[1];
		}
	}
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
byte* SV_FatPVS( vec_t* org )
{
	fatbytes = ( sv.worldmodel->numleafs + 31 ) >> 3;
	Q_memset( fatpvs, 0, fatbytes );
	SV_AddToFatPVS( org, sv.worldmodel->nodes );
	return fatpvs;
}

void SV_AddToFatPAS( vec_t* org, mnode_t* node )
{
	int i;
	byte* pas;
	mplane_t* plane;
	float d;

	while ( 1 )
	{
		if ( node->contents < 0 )
		{
			if ( node->contents != CONTENTS_SOLID )
			{
				pas = CM_LeafPAS( (mleaf_t*)node - sv.worldmodel->leafs );
				for ( i = 0; i < fatpasbytes; i++ )
					fatpas[i] |= pas[i];
			}
			return;
		}

		plane = node->plane;
		d = DotProduct( org, plane->normal ) - plane->dist;
		if ( d > 8 )
			node = node->children[0];
		else if ( d < -8 )
			node = node->children[1];
		else
		{
			// go down both
			SV_AddToFatPAS( org, node->children[0] );
			node = node->children[1];
		}
	}
}

byte* SV_FatPAS( vec_t* org )
{
	fatpasbytes = ( sv.worldmodel->numleafs + 31 ) >> 3;
	Q_memset( fatpas, 0, fatpasbytes );
	SV_AddToFatPAS( org, sv.worldmodel->nodes );
	return fatpas;
}

int SV_CheckVisibility( const edict_t* entity, byte* pset )
{
	edict_t* ent;
	int i;
	int leaf;

	if ( !pset )
		return 1;

	if ( entity->headnode < 0 )
	{
		for ( i = 0; i < entity->num_leafs; i++ )
		{
			leaf = entity->leafnums[i];
			if ( pset[leaf >> 3] & ( 1 << ( leaf & 7 ) ) )
				return 1;
		}
		return 0;
	}

	leaf = -1;
	for ( i = 0; i < MAX_ENT_LEAFS; i++ )
	{
		leaf = entity->leafnums[i];
		if ( leaf == -1 )
			break;

		if ( pset[leaf >> 3] & ( 1 << ( leaf & 7 ) ) )
			return 1;
	}

	if ( !CM_HeadnodeVisible( &sv.worldmodel->nodes[entity->headnode], pset, &leaf ) )
		return 0;

	ent = (edict_t*)entity;
	ent->leafnums[ent->num_leafs] = (short)leaf;
	ent->num_leafs = ( ent->num_leafs + 1 ) % MAX_ENT_LEAFS;
	return 2;
}

void SetCStrikeFlags()
{
	if ( g_bCS_CZ_Flags_Initialized )
		return;

	if ( !_stricmp( com_gamedir, "cstrike" ) || !_stricmp( com_gamedir, "cstrike_beta" ) )
		g_bIsCStrike = 1;
	else if ( !_stricmp( com_gamedir, "czero" ) )
		g_bIsCZero = 1;
	else if ( !_stricmp( com_gamedir, "czeror" ) )
		g_bIsCZeroRitual = 1;
	else if ( !_stricmp( com_gamedir, "terror" ) )
		g_bIsTerrorStrike = 1;
	else if ( !_stricmp( com_gamedir, "tfc" ) )
		g_bIsTFC = 1;
	else if ( !_stricmp( com_gamedir, "valve" ) )
		g_bIsHL1 = 1;

	g_bCS_CZ_Flags_Initialized = 1;
}

const char* Q_stristr( const char* pStr, const char* pSearch )
{
	const char* letter;
	const char* match;
	const char* test;

	if ( !pStr || !pSearch || !*pStr )
		return NULL;

	for ( letter = pStr; *letter; letter++ )
	{
		if ( tolower( (unsigned char)*letter ) != tolower( (unsigned char)*pSearch ) )
			continue;

		match = letter + 1;
		test = pSearch + 1;
		while ( *test )
		{
			if ( !*match )
				return NULL;

			if ( tolower( (unsigned char)*match ) != tolower( (unsigned char)*test ) )
				break;

			match++;
			test++;
		}

		if ( !*test )
			return letter;
	}

	return NULL;
}

GameToAppIDMapItem_t g_GameToAppIDMap[GAME_APP_ID_MAP_COUNT] = {
	{ 10, "cstrike" },
	{ 20, "tfc" },
	{ 30, "dod" },
	{ 40, "dmc" },
	{ 50, "gearbox" },
	{ 60, "ricochet" },
	{ 70, "valve" },
	{ 80, "czero" },
	{ 100, "czeror" },
	{ 130, "bshift" },
	{ 150, "cstrike_beta" },
};

int GetGameAppID()
{
	char arg[GAME_DIRECTORY_BYTES];
	char gameDir[GAME_DIRECTORY_BYTES];
	int i;

	COM_ParseDirectoryFromCmd( "-game", arg, sizeof( arg ), "valve" );
	COM_FileBase( arg, gameDir );

	for ( i = 0; i < GAME_APP_ID_MAP_COUNT; i++ )
	{
		if ( !_stricmp( g_GameToAppIDMap[i].pGameDir, gameDir ) )
			return g_GameToAppIDMap[i].iAppID;
	}

	return 70;
}

qboolean IsGameSubscribed( const char* game )
{
	uint32 appID;
	int i;

	appID = 70;
	for ( i = 0; i < GAME_APP_ID_MAP_COUNT; i++ )
	{
		if ( !_stricmp( g_GameToAppIDMap[i].pGameDir, game ) )
		{
			appID = g_GameToAppIDMap[i].iAppID;
			break;
		}
	}

	return ISteamApps_BIsSubscribedApp( appID ) ? TRUE : FALSE;
}

qboolean IsSafeFileToDownload( const char* filename )
{
	static const char* const unsafeFiles[] = { ".cfg", ".rc", ".lst", ".exe", ".vbs", ".com", ".bat", ".dll", ".ini",
		".log", "halflife.wad", "pak0.pak", "xeno.wad", ".so", ".dylib", ".sys", ".asi", ".mix", ".flt" };
	char lwrfilename[260];
	char* first;
	char* last;
	int i;

	if ( !filename )
		return FALSE;

	Q_strncpy( lwrfilename, filename, sizeof( lwrfilename ) );
	Q_strlwr( lwrfilename );

	if ( Q_strstr( lwrfilename, ":" ) || Q_strstr( lwrfilename, ".." ) || Q_strstr( lwrfilename, "~" )
		|| lwrfilename[0] == '/' || lwrfilename[0] == '\\' )
		return FALSE;

	first = strchr( lwrfilename, '.' );
	last = strrchr( lwrfilename, '.' );
	if ( first != last )
		return FALSE;

	for ( i = 0; i < ARRAYSIZE( unsafeFiles ); i++ )
	{
		if ( Q_stristr( lwrfilename, unsafeFiles[i] ) )
			return FALSE;
	}

	return TRUE;
}

int RegUserMsg( const char* pszName, int iSize )
{
	UserMsg* pUserMsgs;
	UserMsg* pNewMsg;

	if ( giNextUserMsg > 255 )
	{
		Con_Printf( "Not enough room to register message %s\n", pszName );
		return 0;
	}

	if ( !pszName )
		return 0;

	if ( Q_strlen( pszName ) > 11 )
	{
		Con_Printf( "Message name too long: %s\n", pszName );
		return 0;
	}

	if ( iSize > 192 )
		return 0;

	for ( pUserMsgs = sv_gpUserMsgs; pUserMsgs; pUserMsgs = pUserMsgs->next )
	{
		if ( !Q_strcmp( pszName, pUserMsgs->szName ) )
			return pUserMsgs->iMsg;
	}

	pNewMsg = (UserMsg*)Mem_ZeroMalloc( sizeof( UserMsg ) );
	pNewMsg->iMsg = giNextUserMsg++;
	pNewMsg->iSize = iSize;
	Q_strcpy( pNewMsg->szName, pszName );
	pNewMsg->next = sv_gpNewUserMsgs;
	sv_gpNewUserMsgs = pNewMsg;

	return pNewMsg->iMsg;
}

void SV_SendUserReg( sizebuf_t* sb )
{
	UserMsg* pMsg;

	for ( pMsg = sv_gpNewUserMsgs; pMsg; pMsg = pMsg->next )
	{
		MSG_WriteByte( sb, svc_newusermsg );
		MSG_WriteByte( sb, pMsg->iMsg );
		MSG_WriteByte( sb, pMsg->iSize );
		MSG_WriteLong( sb, *(int*)&pMsg->szName[0] );
		MSG_WriteLong( sb, *(int*)&pMsg->szName[4] );
		MSG_WriteLong( sb, *(int*)&pMsg->szName[8] );
		MSG_WriteLong( sb, *(int*)&pMsg->szName[12] );
	}
}

void SV_ClearPacketEntities( client_frame_t* frame )
{
	if ( !frame )
		return;

	if ( frame->entities.entities )
		Mem_Free( frame->entities.entities );

	frame->entities.entities = NULL;
	frame->entities.num_entities = 0;
}

void SV_ClearFrames( client_frame_t** frames )
{
	client_frame_t* frame;
	int i;

	if ( !*frames )
		return;

	for ( i = 0, frame = *frames; i < SV_UPDATE_BACKUP; i++, frame++ )
	{
		SV_ClearPacketEntities( frame );
		frame->senttime = 0.0;
		frame->ping_time = -1.0f;
	}

	Mem_Free( *frames );
	*frames = NULL;
}

void SV_AllocClientFrames()
{
	client_t* client;
	int i;

	for ( i = 0, client = svs.clients; i < svs.maxclientslimit; i++, client++ )
	{
		if ( client->frames )
			Con_DPrintf( "Allocating over frame pointer?\n" );

		client->frames = (client_frame_t*)Mem_ZeroMalloc( sizeof( client_frame_t ) * SV_UPDATE_BACKUP );
	}
}

void SV_SetMaxclients()
{
	client_t* client;
	int i;

	for ( i = 0; i < svs.maxclientslimit; i++ )
		SV_ClearFrames( &svs.clients[i].frames );

	svs.maxclients = 1;

	i = COM_CheckParm( "-maxplayers" );
	if ( i )
		svs.maxclients = Q_atoi( com_argv[i + 1] );
	else if ( g_bIsDedicatedServer )
		svs.maxclients = 6;

	cls.state = g_bIsDedicatedServer ? ca_dedicated : ca_disconnected;

	if ( svs.maxclients < 1 )
		svs.maxclients = 6;
	else if ( svs.maxclients > MAX_CLIENTS )
		svs.maxclients = MAX_CLIENTS;

	if ( g_bIsDedicatedServer || host_parms.memsize > 16 * 1024 * 1024 )
		svs.maxclientslimit = MAX_CLIENTS;
	else
		svs.maxclientslimit = 4;

	SV_UPDATE_BACKUP = ( svs.maxclients == 1 ) ? SINGLEPLAYER_BACKUP : MULTIPLAYER_BACKUP;
	SV_UPDATE_MASK = SV_UPDATE_BACKUP - 1;

	svs.clients = (client_t*)Hunk_AllocName( sizeof( client_t ) * svs.maxclientslimit, "clients" );

	for ( i = 0, client = svs.clients; i < svs.maxclientslimit; i++, client++ )
	{
		Q_memset( client, 0, sizeof( *client ) );
		client->resourcesneeded.pNext = client->resourcesneeded.pPrev = &client->resourcesneeded;
		client->resourcesonhand.pNext = client->resourcesonhand.pPrev = &client->resourcesonhand;
	}

	Cvar_SetValue( "deathmatch", svs.maxclients > 1 ? 1.0f : 0.0f );

	SV_AllocClientFrames();

	if ( svs.maxclients > svs.maxclientslimit )
		svs.maxclients = svs.maxclientslimit;
}

char* pr_strings;
char localinfo[MAX_LOCALINFO_STRING];

cvar_t sv_failuretime = { "sv_failuretime", "0.5" };
cvar_t rcon_password = { "rcon_password", "", FCVAR_PRIVILEGED };
cvar_t sv_enableoldqueries = { "sv_enableoldqueries", "0" };
cvar_t sv_instancedbaseline = { "sv_instancedbaseline", "1" };
cvar_t sv_contact = { "sv_contact", "", FCVAR_SERVER };
cvar_t sv_filterban = { "sv_filterban", "1" };
cvar_t sv_maxupdaterate = { "sv_maxupdaterate", "30.0" };
cvar_t sv_minupdaterate = { "sv_minupdaterate", "10.0" };
cvar_t sv_logrelay = { "sv_logrelay", "0" };
cvar_t sv_lan = { "sv_lan", "0" };
cvar_t sv_lan_rate = { "sv_lan_rate", "20000.0" };
cvar_t sv_proxies = { "sv_proxies", "1", FCVAR_SERVER };
cvar_t sv_outofdatetime = { "sv_outofdatetime", "1800" };
cvar_t sv_allow_shaders = { "sv_allow_shaders", "0", FCVAR_SERVER | FCVAR_SPONLY };
cvar_t sv_visiblemaxplayers = { "sv_visiblemaxplayers", "-1" };
cvar_t sv_password = { "sv_password", "", FCVAR_SERVER | FCVAR_PROTECTED };
cvar_t sv_aim = { "sv_aim", "1", FCVAR_ARCHIVE };
cvar_t sv_allow_autoaim = { "sv_allow_autoaim", "0", FCVAR_SERVER | FCVAR_SPONLY };
cvar_t violence_ablood = { "violence_ablood", "1", FCVAR_ARCHIVE };
cvar_t violence_hgibs = { "violence_hgibs", "1", FCVAR_ARCHIVE };
cvar_t violence_agibs = { "violence_agibs", "1", FCVAR_ARCHIVE };
cvar_t sv_newunit = { "sv_newunit", "0" };
cvar_t sv_skycolor_r = { "sv_skycolor_r", "0" };
cvar_t sv_skycolor_g = { "sv_skycolor_g", "0" };
cvar_t sv_skycolor_b = { "sv_skycolor_b", "0" };
cvar_t sv_skyvec_x = { "sv_skyvec_x", "0" };
cvar_t sv_skyvec_y = { "sv_skyvec_y", "0" };
cvar_t sv_skyvec_z = { "sv_skyvec_z", "0" };
cvar_t sv_timeout = { "sv_timeout", "60" };
cvar_t sv_clienttrace = { "sv_clienttrace", "1", FCVAR_SERVER };
cvar_t sv_cheats = { "sv_cheats", "0", FCVAR_SERVER | FCVAR_SPONLY };
cvar_t sv_allow_download = { "sv_allowdownload", "1" };
cvar_t sv_allow_upload = { "sv_allowupload", "1", FCVAR_SERVER };
cvar_t sv_max_upload = { "sv_uploadmax", "0.5", FCVAR_SERVER };
cvar_t sv_max_upload_count = { "sv_uploadmaxcount", "10", FCVAR_SERVER };
cvar_t sv_send_logos = { "sv_send_logos", "1" };
cvar_t sv_send_resources = { "sv_send_resources", "1" };
cvar_t sv_logbans = { "sv_logbans", "0" };
cvar_t hpk_maxsize = { "hpk_maxsize", "4", FCVAR_ARCHIVE };
cvar_t mapcyclefile = { "mapcyclefile", "mapcycle.txt", FCVAR_NOBADPATHS };
cvar_t motdfile = { "motdfile", "motd.txt" };
cvar_t servercfgfile = { "servercfgfile", "server.cfg", FCVAR_FILTERCHARS };
cvar_t mapchangecfgfile = { "mapchangecfgfile", "", FCVAR_FILTERCHARS };
cvar_t disconcfgfile = { "disconcfgfile", "", FCVAR_FILTERCHARS };
cvar_t lservercfgfile = { "lservercfgfile", "listenserver.cfg", FCVAR_FILTERCHARS };
cvar_t logsdir = { "logsdir", "logs" };
cvar_t bannedcfgfile = { "bannedcfgfile", "banned.cfg" };
cvar_t sv_rcon_minfailures = { "sv_rcon_minfailures", "5" };
cvar_t sv_rcon_maxfailures = { "sv_rcon_maxfailures", "10" };
cvar_t sv_rcon_minfailuretime = { "sv_rcon_minfailuretime", "30" };
cvar_t sv_rcon_banpenalty = { "sv_rcon_banpenalty", "0" };
cvar_t sv_minrate = { "sv_minrate", "0", FCVAR_SERVER };
cvar_t sv_maxrate = { "sv_maxrate", "0", FCVAR_SERVER };
cvar_t max_queries_sec = { "max_queries_sec", "3.0", FCVAR_SERVER | FCVAR_PROTECTED };
cvar_t max_queries_sec_global = { "max_queries_sec_global", "30", FCVAR_SERVER | FCVAR_PROTECTED };
cvar_t max_queries_window = { "max_queries_window", "60", FCVAR_SERVER | FCVAR_PROTECTED };
cvar_t sv_logblocks = { "sv_logblocks", "0", FCVAR_SERVER };
cvar_t sv_downloadurl = { "sv_downloadurl", "", FCVAR_PROTECTED };
cvar_t sv_version = { "sv_version", "" };
cvar_t sv_allow_dlfile = { "sv_allow_dlfile", "1" };
cvar_t sv_use_steam_networking = { "sv_use_steam_networking", "1" };
cvar_t ui_cmd_on_connect = { "ui_cmd_on_connect", "" };
cvar_t _sv_override_scientist_mdl = { "_sv_override_scientist_mdl", "", FCVAR_SPONLY };

static void sv_cheats_hook_callback( cvar_t* )
{
	client_t* client;
	int i;

	if ( !Host_IsServerActive() )
		return;

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( client->fakeclient )
			continue;

		if ( !client->active && !client->spawned && !client->connected )
			continue;

		MSG_WriteByte( &client->netchan.message, svc_sendextrainfo );
		MSG_WriteString( &client->netchan.message, "" );
		MSG_WriteByte( &client->netchan.message, sv_cheats.value != 0.0f );
	}
}

static void mapcyclefile_hook_callback( cvar_t* cvar )
{
	char buf[261];

	if ( !Q_strcmp( COM_FileExtension( cvar->string ), "txt" ) )
		return;

	_snprintf( buf, sizeof( buf ), "%s.txt", cvar->string );
	if ( Q_strcmp( COM_FileExtension( buf ), "txt" ) )
		Cvar_DirectSet( cvar, "mapcycle.txt" );
	else
		Cvar_DirectSet( cvar, buf );
}

static cvarhook_t sv_cheats_hook = { sv_cheats_hook_callback, NULL, NULL };
static cvarhook_t mapcyclefile_hook = { mapcyclefile_hook_callback, NULL, NULL };

typedef struct delta_info_s
{
	struct delta_info_s* next;
	char* name;
	char* loadfile;
	delta_t* delta;
} delta_info_t;

delta_info_t* g_sv_delta;
static delta_t* g_pplayerdelta;
static delta_t* g_pentitydelta;
static delta_t* g_pcustomentitydelta;
static delta_t* g_pclientdelta;
static delta_t* g_pweapondelta;
delta_t* g_peventdelta;

delta_t* SV_LookupDelta( char* name )
{
	delta_info_t* p;

	for ( p = g_sv_delta; p; p = p->next )
	{
		if ( !Q_strcasecmp( name, p->name ) )
			return p->delta;
	}

	Sys_Error( "Couldn't find delta for %s\n", name );
}

void SV_RegisterDelta( char* name, char* loadfile )
{
	delta_info_t* p;
	delta_t* pdesc;

	pdesc = NULL;
	if ( !DELTA_Load( name, &pdesc, loadfile ) )
		Sys_Error( "Error parsing %s!!!\n", loadfile );

	p = (delta_info_t*)Mem_ZeroMalloc( sizeof( delta_info_t ) );
	p->loadfile = Mem_Strdup( loadfile );
	p->name = Mem_Strdup( name );
	p->delta = pdesc;
	p->next = g_sv_delta;
	g_sv_delta = p;
}

void SV_InitDeltas()
{
	Con_DPrintf( "Initializing deltas\n" );

	SV_RegisterDelta( "clientdata_t", "delta.lst" );
	SV_RegisterDelta( "entity_state_t", "delta.lst" );
	SV_RegisterDelta( "entity_state_player_t", "delta.lst" );
	SV_RegisterDelta( "custom_entity_state_t", "delta.lst" );
	SV_RegisterDelta( "usercmd_t", "delta.lst" );
	SV_RegisterDelta( "weapon_data_t", "delta.lst" );
	SV_RegisterDelta( "event_t", "delta.lst" );

	g_pplayerdelta = SV_LookupDelta( "entity_state_player_t" );
	if ( !g_pplayerdelta )
		Sys_Error( "No entity_state_player_t encoder on server!\n" );

	g_pentitydelta = SV_LookupDelta( "entity_state_t" );
	if ( !g_pentitydelta )
		Sys_Error( "No entity_state_t encoder on server!\n" );

	g_pcustomentitydelta = SV_LookupDelta( "custom_entity_state_t" );
	if ( !g_pcustomentitydelta )
		Sys_Error( "No custom_entity_state_t encoder on server!\n" );

	g_pclientdelta = SV_LookupDelta( "clientdata_t" );
	if ( !g_pclientdelta )
		Sys_Error( "No clientdata_t encoder on server!\n" );

	g_pweapondelta = SV_LookupDelta( "weapon_data_t" );
	if ( !g_pweapondelta )
		Sys_Error( "No weapon_data_t encoder on server!\n" );

	g_peventdelta = SV_LookupDelta( "event_t" );
	if ( !g_peventdelta )
		Sys_Error( "No event_t encoder on server!\n" );
}

/*
===========
SV_Serverinfo_f

  Examine or change the serverinfo string
===========
*/
void SV_Serverinfo_f()
{
	cvar_t* var;

	if ( Cmd_Argc() == 1 )
	{
		Con_Printf( "Server info settings:\n" );
		Info_Print( Info_Serverinfo() );
		return;
	}

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "usage: serverinfo [ <key> <value> ]\n" );
		return;
	}

	if ( Cmd_Argv( 1 )[0] == '*' )
	{
		Con_Printf( "Star variables cannot be changed.\n" );
		return;
	}

	Info_SetValueForKey( Info_Serverinfo(), Cmd_Argv( 1 ), Cmd_Argv( 2 ), MAX_SERVERINFO_STRING );

	// if this is a cvar, change it too
	var = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( var )
	{
		Z_Free( var->string );
		var->string = CopyString( (char*)Cmd_Argv( 2 ) );
		var->value = Q_atof( var->string );
	}

	SV_BroadcastCommand( "fullserverinfo \"%s\"\n", Info_Serverinfo() );
}

void SV_Localinfo_f()
{
	if ( Cmd_Argc() == 1 )
	{
		Con_Printf( "Local info settings:\n" );
		Info_Print( localinfo );
		return;
	}

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "usage: localinfo [ <key> <value> ]\n" );
		return;
	}

	if ( Cmd_Argv( 1 )[0] == '*' )
	{
		Con_Printf( "Star variables cannot be changed.\n" );
		return;
	}

	Info_SetValueForKey( localinfo, Cmd_Argv( 1 ), Cmd_Argv( 2 ), MAX_LOCALINFO_STRING - 1 );
}

void SV_ListId_f()
{
	int i;

	if ( numuserfilters <= 0 )
	{
		Con_Printf( "UserID filter list: empty\n" );
		return;
	}

	Con_Printf( "UserID filter list: %i entries\n", numuserfilters );

	for ( i = 0; i < numuserfilters; i++ )
	{
		if ( userfilters[i].banTime == 0.0f )
			Con_Printf( "%i %s : permanent\n", i + 1, SV_GetIDString( &userfilters[i].userid ) );
		else
			Con_Printf( "%i %s : %.3f min\n", i + 1, SV_GetIDString( &userfilters[i].userid ), userfilters[i].banTime );
	}
}

void SV_WriteId_f()
{
	FileHandle_t f;
	char name[260];
	int i;

	if ( !bannedcfgfile.string || !bannedcfgfile.string[0] )
	{
		Con_Printf( "Couldn't open banned config file, no file name specified.\n" );
		return;
	}

	if ( bannedcfgfile.string[0] == '/' || Q_strstr( bannedcfgfile.string, ":" ) || Q_strstr( bannedcfgfile.string, ".." )
		|| Q_strstr( bannedcfgfile.string, "\\" ) )
	{
		Con_Printf( "Couldn't open %s (contains illegal characters).\n", bannedcfgfile.string );
		return;
	}

	Q_strncpy( name, bannedcfgfile.string, 255 );
	COM_DefaultExtension( name, ".cfg" );

	if ( Q_strcmp( COM_FileExtension( name ), "cfg" ) )
	{
		Con_Printf( "Couldn't open %s (wrong file extension, must be .cfg).\n", name );
		return;
	}

	Con_Printf( "Writing %s.\n", name );

	f = FS_Open( name, "wt" );
	if ( !f )
	{
		Con_Printf( "Couldn't open %s\n", name );
		return;
	}

	for ( i = 0; i < numuserfilters; i++ )
	{
		if ( userfilters[i].banTime == 0.0f )
			FS_FPrintf( f, "banid 0.0 %s\n", SV_GetIDString( &userfilters[i].userid ) );
	}

	FS_Close( f );
}

void SV_RemoveId_f()
{
	char id[64];
	USERID_t removed;
	int i;
	int slot;

	if ( Cmd_Argc() != 2 && Cmd_Argc() != 6 )
	{
		Con_Printf( "Usage:  removeid <uniqueid | #slotnumber>\n" );
		return;
	}

	strncpy( id, Cmd_Argv( 1 ), sizeof( id ) - 1 );
	id[sizeof( id ) - 1] = '\0';

	if ( !id[0] )
	{
		Con_Printf( "SV_RemoveId_f:  Id string is empty!\n" );
		return;
	}

	if ( id[0] == '#' )
	{
		slot = Q_atoi( &id[1] );
		if ( slot <= 0 || slot > numuserfilters )
		{
			Con_Printf( "SV_RemoveId_f:  invalid slot #%i\n", slot );
			return;
		}

		slot--;
		removed = userfilters[slot].userid;

		if ( slot + 1 < numuserfilters )
			memmove( &userfilters[slot], &userfilters[slot + 1], ( numuserfilters - slot - 1 ) * sizeof( userfilter_t ) );
		numuserfilters--;

		Con_Printf( "UserID filter removed for %s, id %s\n", id, SV_GetIDString( &removed ) );
		return;
	}

	if ( !Q_strnicmp( id, "STEAM_", 6 ) || !Q_strnicmp( id, "VALVE_", 6 ) )
	{
		_snprintf( id, sizeof( id ) - 1, "%s:%s:%s", Cmd_Argv( 1 ), Cmd_Argv( 3 ), Cmd_Argv( 5 ) );
		id[sizeof( id ) - 1] = '\0';
	}

	for ( i = 0; i < numuserfilters; i++ )
	{
		if ( Q_strcasecmp( SV_GetIDString( &userfilters[i].userid ), id ) )
			continue;

		if ( i + 1 < numuserfilters )
			memmove( &userfilters[i], &userfilters[i + 1], ( numuserfilters - i - 1 ) * sizeof( userfilter_t ) );
		numuserfilters--;

		Con_Printf( "UserID filter removed for %s\n", id );
		return;
	}

	Con_Printf( "removeid: couldn't find %s\n", id );
}

qboolean SV_CompareUserID( USERID_t* id1, USERID_t* id2 )
{
	char szID1[64];
	char szID2[64];

	if ( !id1 || !id2 )
		return FALSE;

	if ( id1->idtype != id2->idtype )
		return FALSE;

	if ( id1->idtype != AUTH_IDTYPE_STEAM && id1->idtype != AUTH_IDTYPE_VALVE )
		return FALSE;

	strncpy( szID1, SV_GetIDString( id1 ), sizeof( szID1 ) - 1 );
	szID1[sizeof( szID1 ) - 1] = '\0';

	strncpy( szID2, SV_GetIDString( id2 ), sizeof( szID2 ) - 1 );
	szID2[sizeof( szID2 ) - 1] = '\0';

	return !_stricmp( szID1, szID2 );
}

USERID_t* SV_StringToUserID( const char* str )
{
	static USERID_t id;
	char szTemp[128];

	memset( &id, 0, sizeof( id ) );

	if ( !str || !str[0] )
		return &id;

	if ( !Q_strnicmp( str, "STEAM_", 6 ) )
		id.idtype = AUTH_IDTYPE_STEAM;
	else
		id.idtype = AUTH_IDTYPE_VALVE;

	strncpy( szTemp, str + 6, sizeof( szTemp ) - 1 );
	szTemp[sizeof( szTemp ) - 1] = '\0';
	id.m_SteamID = Steam_StringToSteamID( szTemp );

	return &id;
}

void SV_BanId_f()
{
	char idstring[64];
	char szreason[256];
	float banTime;
	qboolean bKick;
	USERID_t* id;
	client_t* client;
	client_t* save;
	const char* pszCmdGiver;
	const char* number;
	int search;
	int i;

	if ( Cmd_Argc() < 3 || Cmd_Argc() > 8 )
	{
		Con_Printf( "Usage:  banid <minutes> <uniqueid or #userid> { kick }\n" );
		Con_Printf( "Use 0 minutes for permanent\n" );
		return;
	}

	banTime = Q_atof( Cmd_Argv( 1 ) );
	if ( banTime < 0.01 )
		banTime = 0.0f;

	strncpy( idstring, Cmd_Argv( 2 ), sizeof( idstring ) - 1 );
	idstring[sizeof( idstring ) - 1] = '\0';

	bKick = Cmd_Argc() >= 4 && !Q_strcasecmp( Cmd_Argv( Cmd_Argc() - 1 ), "kick" );

	id = NULL;
	if ( idstring[0] == '#' )
	{
		if ( Q_strlen( idstring ) == 1 )
		{
			if ( Cmd_Argc() < 3 )
			{
				Con_Printf( "Insufficient arguments to banid\n" );
				return;
			}
			number = Cmd_Argv( 3 );
		}
		else
		{
			number = &idstring[1];
		}

		search = Q_atoi( number );

		for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
		{
			if ( !client->active && !client->connected && !client->spawned )
				continue;

			if ( client->fakeclient )
				continue;

			if ( client->userid == search )
			{
				id = &client->network_userid;
				break;
			}
		}

		if ( !id )
		{
			Con_Printf( "SV_BanId_f:  Couldn't find #userid %u\n", search );
			return;
		}
	}
	else
	{
		if ( !Q_strnicmp( idstring, "STEAM_", 6 ) || !Q_strnicmp( idstring, "VALVE_", 6 ) )
		{
			_snprintf( idstring, sizeof( idstring ) - 1, "%s:%s:%s", Cmd_Argv( 2 ), Cmd_Argv( 4 ), Cmd_Argv( 6 ) );
			idstring[sizeof( idstring ) - 1] = '\0';
		}

		for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
		{
			if ( !client->active && !client->connected && !client->spawned )
				continue;

			if ( client->fakeclient )
				continue;

			if ( !Q_strcasecmp( SV_GetClientIDString( client ), idstring ) )
			{
				id = &client->network_userid;
				break;
			}
		}

		if ( !id )
			id = SV_StringToUserID( idstring );
	}

	for ( i = 0; i < numuserfilters; i++ )
	{
		if ( SV_CompareUserID( &userfilters[i].userid, id ) )
			break;
	}

	if ( i >= numuserfilters )
	{
		if ( numuserfilters >= MAX_USERFILTERS )
		{
			Con_Printf( "SV_BanId_f:  User filter list is full\n" );
			return;
		}
		numuserfilters++;
	}

	userfilters[i].banTime = banTime;
	userfilters[i].banEndTime = ( banTime == 0.0f ) ? 0.0f : (float)( banTime * 60.0 + realtime );
	memcpy( &userfilters[i].userid, id, sizeof( USERID_t ) );

	if ( banTime == 0.0f )
		sprintf( szreason, "permanently" );
	else
		_snprintf( szreason, sizeof( szreason ), "for %.2f minutes", banTime );

	save = host_client;

	if ( cmd_source == src_command )
		pszCmdGiver = ( cls.state != ca_dedicated ) ? cl_name.string : "Console";
	else
		pszCmdGiver = save->name;

	if ( !bKick )
	{
		if ( sv_logbans.value )
			Log_Printf( "Ban: \"<><%s><>\" was banned \"%s\" by \"%s\"\n", SV_GetIDString( id ), szreason, pszCmdGiver );
		return;
	}

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( !client->active && !client->connected && !client->spawned )
			continue;

		if ( client->fakeclient )
			continue;

		if ( !SV_CompareUserID( &client->network_userid, id ) )
			continue;

		host_client = client;

		if ( sv_logbans.value )
			Log_Printf( "Ban: \"%s<%i><%s><>\" was kicked and banned \"%s\" by \"%s\"\n", host_client->name,
				host_client->userid, SV_GetClientIDString( host_client ), szreason, pszCmdGiver );

		SV_ClientPrintf( "You have been kicked and banned %s by the server op.\n", szreason );
		SV_DropClient( host_client, FALSE, "Kicked and banned" );
		break;
	}

	host_client = save;
}

//// Usercommands
/*
====================
SV_User_f

user <name or userid>

Dump userdata / masterdata for a user
====================
*/
void SV_User_f()
{
	client_t* client;
	int uid;
	int i;

	if ( !sv.active )
	{
		Con_Printf( "Can't 'user', not running a server\n" );
		return;
	}

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "Usage: user <username / userid>\n" );
		return;
	}

	uid = Q_atoi( Cmd_Argv( 1 ) );

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( !client->active && !client->spawned && !client->connected )
			continue;

		if ( client->fakeclient || !client->name[0] )
			continue;

		if ( client->userid == uid || !Q_strcmp( client->name, Cmd_Argv( 1 ) ) )
		{
			Info_Print( client->userinfo );
			return;
		}
	}

	Con_Printf( "User not in server.\n" );
}

/*
====================
SV_Users_f

Dump userids for all current players
====================
*/
void SV_Users_f()
{
	client_t* client;
	int count;
	int i;

	if ( !sv.active )
	{
		Con_Printf( "Can't 'users', not running a server\n" );
		return;
	}

	Con_Printf( "userid : uniqueid : name\n" );
	Con_Printf( "------ : ---------: ----\n" );

	count = 0;
	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( !client->active && !client->spawned && !client->connected )
			continue;

		if ( client->fakeclient || !client->name[0] )
			continue;

		Con_Printf( "%6i : %s : %s\n", client->userid, SV_GetClientIDString( client ), client->name );
		count++;
	}

	Con_Printf( "%i users\n", count );
}

void SV_Drop_f()
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	SV_FlushRedirect();
	sv_redirected = RD_NONE;

	SV_BroadcastPrintf( "%s dropped\n", host_client->name );
	SV_DropClient( host_client, FALSE, "Client sent 'drop'" );
}

void SV_BeginFileDownload_f()
{
	const char* filename;
	resource_t custResource;
	byte md5[MD5_DIGEST_BYTES];
	byte* pbuf;
	int size;

	if ( Cmd_Argc() < 2 || cmd_source == src_command )
		return;

	filename = Cmd_Argv( 1 );
	if ( !filename || !filename[0] || ( !Q_strncmp( filename, "!ModuleC.dll", 12 ) && svs.isSecure ) )
		return;

	if ( IsSafeFileToDownload( filename ) && sv_allow_download.value )
	{
		if ( filename[0] != '!' )
		{
			if ( !host_client->fully_connected && sv_send_resources.value
				&& ( !sv_downloadurl.string || !sv_downloadurl.string[0] || strlen( sv_downloadurl.string ) >= 128
					|| sv_allow_dlfile.value ) )
			{
				if ( Netchan_CreateFileFragments( TRUE, &host_client->netchan, (char*)filename ) )
				{
					Netchan_FragSend( &host_client->netchan );
					return;
				}
			}
		}
		else if ( Q_strlen( filename ) == 36 && !Q_strnicmp( filename, "!MD5", 4 ) && sv_send_logos.value )
		{
			pbuf = NULL;
			size = 0;

			Q_memset( &custResource, 0, sizeof( custResource ) );
			COM_HexConvert( filename + 4, 32, md5 );

			if ( HPAK_ResourceForHash( "custom.hpk", md5, &custResource ) )
			{
				HPAK_GetDataPointer( "custom.hpk", &custResource, &pbuf, &size );
				if ( pbuf && size )
				{
					Netchan_CreateFileFragmentsFromBuffer( TRUE, &host_client->netchan, (char*)filename, pbuf, size );
					Netchan_FragSend( &host_client->netchan );
					Mem_Free( pbuf );
				}
			}
			return;
		}
	}

	if ( filename[0] )
	{
		MSG_WriteByte( &host_client->netchan.message, svc_filetxferfailed );
		MSG_WriteString( &host_client->netchan.message, filename );
	}
}

void SV_ResetRcon_f();
void SV_SetLogAddress_f();
void SV_AddLogAddress_f();
void SV_DelLogAddress_f();
void SV_ServerLog_f();
void SV_ShowServerinfo_f();
void SV_AddIP_f();
void SV_RemoveIP_f();
void SV_ListIP_f();
void SV_WriteIP_f();
void SV_Spawn_f();
void SV_New_f();
void SV_SendRes_f();
void SV_SendEnts_f();

playermove_t g_svmove;

char localmodels[MAX_MODELS][5]; // inline model names for precache

/*
===============
SV_Init
===============
*/
void SV_Init()
{
	client_t* client;
	int i;

	Cmd_AddCommand( "banid", SV_BanId_f );
	Cmd_AddCommand( "removeid", SV_RemoveId_f );
	Cmd_AddCommand( "listid", SV_ListId_f );
	Cmd_AddCommand( "writeid", SV_WriteId_f );
	Cmd_AddCommand( "resetrcon", SV_ResetRcon_f );
	Cmd_AddCommand( "logaddress", SV_SetLogAddress_f );
	Cmd_AddCommand( "logaddress_add", SV_AddLogAddress_f );
	Cmd_AddCommand( "logaddress_del", SV_DelLogAddress_f );
	Cmd_AddCommand( "log", SV_ServerLog_f );
	Cmd_AddCommand( "serverinfo", SV_Serverinfo_f );
	Cmd_AddCommand( "localinfo", SV_Localinfo_f );
	Cmd_AddCommand( "showinfo", SV_ShowServerinfo_f );
	Cmd_AddCommand( "user", SV_User_f );
	Cmd_AddCommand( "users", SV_Users_f );
	Cmd_AddCommand( "dlfile", SV_BeginFileDownload_f );
	Cmd_AddCommand( "addip", SV_AddIP_f );
	Cmd_AddCommand( "removeip", SV_RemoveIP_f );
	Cmd_AddCommand( "listip", SV_ListIP_f );
	Cmd_AddCommand( "writeip", SV_WriteIP_f );
	Cmd_AddCommand( "dropclient", SV_Drop_f );
	Cmd_AddCommand( "spawn", SV_Spawn_f );
	Cmd_AddCommand( "new", SV_New_f );
	Cmd_AddCommand( "sendres", SV_SendRes_f );
	Cmd_AddCommand( "sendents", SV_SendEnts_f );
	Cmd_AddCommand( "fullupdate", SV_FullUpdate_f );

	Cvar_RegisterVariable( &sv_failuretime );
	Cvar_RegisterVariable( &sv_voiceenable );
	Cvar_RegisterVariable( &rcon_password );
	Cvar_RegisterVariable( &sv_enableoldqueries );
	Cvar_RegisterVariable( &mp_consistency );
	Cvar_RegisterVariable( &sv_instancedbaseline );
	Cvar_RegisterVariable( &sv_contact );
	Cvar_RegisterVariable( &sv_unlag );
	Cvar_RegisterVariable( &sv_maxunlag );
	Cvar_RegisterVariable( &sv_unlagpush );
	Cvar_RegisterVariable( &sv_unlagsamples );
	Cvar_RegisterVariable( &sv_filterban );
	Cvar_RegisterVariable( &sv_maxupdaterate );
	Cvar_RegisterVariable( &sv_minupdaterate );
	Cvar_RegisterVariable( &sv_logrelay );
	Cvar_RegisterVariable( &sv_lan );
	Cvar_DirectSet( &sv_lan, PF_IsDedicatedServer() ? "0" : "1" );
	Cvar_RegisterVariable( &sv_lan_rate );
	Cvar_RegisterVariable( &sv_proxies );
	Cvar_RegisterVariable( &sv_outofdatetime );
	Cvar_RegisterVariable( &sv_allow_shaders );
	Cvar_RegisterVariable( &sv_visiblemaxplayers );
	Cvar_RegisterVariable( &sv_password );
	Cvar_RegisterVariable( &sv_aim );
	Cvar_RegisterVariable( &sv_allow_autoaim );
	Cvar_RegisterVariable( &violence_hblood );
	Cvar_RegisterVariable( &violence_ablood );
	Cvar_RegisterVariable( &violence_hgibs );
	Cvar_RegisterVariable( &violence_agibs );
	Cvar_RegisterVariable( &sv_newunit );
	Cvar_RegisterVariable( &sv_gravity );
	Cvar_RegisterVariable( &sv_friction );
	Cvar_RegisterVariable( &sv_edgefriction );
	Cvar_RegisterVariable( &sv_stopspeed );
	Cvar_RegisterVariable( &sv_maxspeed );
	Cvar_RegisterVariable( &sv_footsteps );
	Cvar_RegisterVariable( &sv_accelerate );
	Cvar_RegisterVariable( &sv_rollangle );
	Cvar_RegisterVariable( &sv_rollspeed );
	Cvar_RegisterVariable( &sv_stepsize );
	Cvar_RegisterVariable( &sv_bounce );
	Cvar_RegisterVariable( &sv_airaccelerate );
	Cvar_RegisterVariable( &sv_wateraccelerate );
	Cvar_RegisterVariable( &sv_waterfriction );
	Cvar_RegisterVariable( &sv_skycolor_r );
	Cvar_RegisterVariable( &sv_skycolor_g );
	Cvar_RegisterVariable( &sv_skycolor_b );
	Cvar_RegisterVariable( &sv_skyvec_x );
	Cvar_RegisterVariable( &sv_skyvec_y );
	Cvar_RegisterVariable( &sv_skyvec_z );
	Cvar_RegisterVariable( &sv_timeout );
	Cvar_RegisterVariable( &sv_clienttrace );
	Cvar_RegisterVariable( &sv_zmax );
	Cvar_RegisterVariable( &sv_wateramp );
	Cvar_RegisterVariable( &sv_skyname );
	Cvar_RegisterVariable( &sv_maxvelocity );
	Cvar_RegisterVariable( &sv_cheats );
	Cvar_HookVariable( sv_cheats.name, &sv_cheats_hook );
	if ( COM_CheckParm( "-dev" ) )
		Cvar_SetValue( "sv_cheats", 1.0f );
	Cvar_RegisterVariable( &sv_spectatormaxspeed );
	Cvar_RegisterVariable( &sv_allow_download );
	Cvar_RegisterVariable( &sv_allow_upload );
	Cvar_RegisterVariable( &sv_max_upload );
	Cvar_RegisterVariable( &sv_max_upload_count );
	Cvar_RegisterVariable( &sv_send_logos );
	Cvar_RegisterVariable( &sv_send_resources );
	Cvar_RegisterVariable( &sv_logbans );
	Cvar_RegisterVariable( &hpk_maxsize );
	Cvar_RegisterVariable( &mapcyclefile );
	Cvar_HookVariable( mapcyclefile.name, &mapcyclefile_hook );
	Cvar_RegisterVariable( &motdfile );
	Cvar_RegisterVariable( &servercfgfile );
	Cvar_RegisterVariable( &mapchangecfgfile );
	Cvar_RegisterVariable( &disconcfgfile );
	Cvar_RegisterVariable( &lservercfgfile );
	Cvar_RegisterVariable( &logsdir );
	Cvar_RegisterVariable( &bannedcfgfile );
	Cvar_RegisterVariable( &sv_rcon_minfailures );
	Cvar_RegisterVariable( &sv_rcon_maxfailures );
	Cvar_RegisterVariable( &sv_rcon_minfailuretime );
	Cvar_RegisterVariable( &sv_rcon_banpenalty );
	Cvar_RegisterVariable( &sv_minrate );
	Cvar_RegisterVariable( &sv_maxrate );
	Cvar_RegisterVariable( &max_queries_sec );
	Cvar_RegisterVariable( &max_queries_sec_global );
	Cvar_RegisterVariable( &max_queries_window );
	Cvar_RegisterVariable( &sv_logblocks );
	Cvar_RegisterVariable( &sv_downloadurl );
	Cvar_RegisterVariable( &sv_version );
	Cvar_RegisterVariable( &sv_allow_dlfile );
	Cvar_RegisterVariable( &sv_use_steam_networking );
	Cvar_RegisterVariable( &ui_cmd_on_connect );
	Cvar_RegisterVariable( &_sv_override_scientist_mdl );

	for ( i = 0; i < MAX_MODELS; i++ )
		_snprintf( localmodels[i], sizeof( localmodels[i] ), "*%i", i );

	svs.isSecure = FALSE;

	for ( i = 0, client = svs.clients; i < svs.maxclientslimit; i++, client++ )
	{
		SV_ClearFrames( &client->frames );
		Q_memset( client, 0, sizeof( *client ) );
		client->resourcesonhand.pNext = client->resourcesonhand.pPrev = &client->resourcesonhand;
		client->resourcesneeded.pNext = client->resourcesneeded.pPrev = &client->resourcesneeded;
	}

	PM_Init( &g_svmove );
	SV_AllocClientFrames();
	SV_InitDeltas();
}

void SV_PropagateCustomizations()
{
	client_t* pHost;
	customization_t* pCust;
	resource_t* pResource;
	int i;

	pHost = host_client;

	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( !host_client->active && !host_client->spawned )
			continue;

		if ( host_client->fakeclient )
			continue;

		for ( pCust = host_client->customdata.pNext; pCust; pCust = pCust->pNext )
		{
			if ( !pCust->bInUse )
				continue;

			pResource = &pCust->resource;

			MSG_WriteByte( &pHost->netchan.message, svc_customization );
			MSG_WriteByte( &pHost->netchan.message, i );
			MSG_WriteByte( &pHost->netchan.message, pResource->type );
			MSG_WriteString( &pHost->netchan.message, pResource->szFileName );
			MSG_WriteShort( &pHost->netchan.message, pResource->nIndex );
			MSG_WriteLong( &pHost->netchan.message, pResource->nDownloadSize );
			MSG_WriteByte( &pHost->netchan.message, pResource->ucFlags );

			if ( pResource->ucFlags & RES_CUSTOM )
				SZ_Write( &pHost->netchan.message, pResource->rgucMD5_hash, sizeof( pResource->rgucMD5_hash ) );
		}
	}

	host_client = pHost;
}

static void SV_AddResource( resourcetype_t type, const char* name, int size, byte flags, int index )
{
	resource_t* r;

	if ( sv.num_resources >= MAX_RESOURCES )
		Sys_Error( "Too many resources on server." );

	r = &sv.resourcelist[sv.num_resources++];
	r->type = type;
	Q_strncpy( r->szFileName, name, sizeof( r->szFileName ) - 1 );
	r->szFileName[sizeof( r->szFileName ) - 1] = '\0';
	r->nIndex = index;
	r->nDownloadSize = size;
	r->ucFlags = flags;
}

void SV_CreateResourceList()
{
	qboolean ffirstsent;
	event_t* ep;
	int nSize;
	int i;

	sv.num_resources = 0;

	for ( i = 1; sv.generic_precache[i]; i++ )
	{
		nSize = ( svs.maxclients > 1 ) ? FS_FileSize( sv.generic_precache[i] ) : 0;
		SV_AddResource( t_generic, sv.generic_precache[i], nSize, RES_FATALIFMISSING, i );
	}

	ffirstsent = FALSE;
	for ( i = 1; sv.sound_precache[i]; i++ )
	{
		if ( sv.sound_precache[i][0] == '!' )
		{
			if ( !ffirstsent )
			{
				SV_AddResource( t_sound, "!", 0, RES_FATALIFMISSING, i );
				ffirstsent = TRUE;
			}
			continue;
		}

		nSize = ( svs.maxclients > 1 ) ? FS_FileSize( va( "sound/%s", sv.sound_precache[i] ) ) : 0;
		SV_AddResource( t_sound, sv.sound_precache[i], nSize, 0, i );
	}

	for ( i = 1; sv.model_precache[i]; i++ )
	{
		nSize = 0;
		if ( svs.maxclients > 1 && sv.model_precache[i][0] != '*' )
			nSize = FS_FileSize( sv.model_precache[i] );

		SV_AddResource( t_model, sv.model_precache[i], nSize, sv.model_precache_flags[i], i );
	}

	for ( i = 0; i < sv_decalnamecount; i++ )
		SV_AddResource( t_decal, sv_decalnames[i].name, Draw_DecalSize( i ), 0, i );

	for ( i = 1; i < MAX_EVENT_SCRIPTS && sv.event_precache[i].pszScript; i++ )
	{
		ep = &sv.event_precache[i];
		SV_AddResource( t_eventscript, ep->filename, ep->filesize, RES_FATALIFMISSING, i );
	}
}

void SV_SendConsistencyList( sizebuf_t* msg );

void SV_SendResources( sizebuf_t* msg )
{
	byte nullbuffer[sizeof( resource_t::rguc_reserved )];
	resource_t* r;
	int i;

	Q_memset( nullbuffer, 0, sizeof( nullbuffer ) );

	MSG_WriteByte( msg, svc_resourcerequest );
	MSG_WriteLong( msg, svs.spawncount );
	MSG_WriteLong( msg, 0 );

	if ( sv_downloadurl.string && sv_downloadurl.string[0] && strlen( sv_downloadurl.string ) < MAX_DOWNLOAD_URL )
	{
		MSG_WriteByte( msg, svc_resourcelocation );
		MSG_WriteString( msg, sv_downloadurl.string );
	}

	MSG_WriteByte( msg, svc_resourcelist );
	MSG_StartBitWriting( msg );
	MSG_WriteBits( sv.num_resources, 12 );

	for ( i = 0, r = sv.resourcelist; i < sv.num_resources; i++, r++ )
	{
		MSG_WriteBits( r->type, 4 );
		MSG_WriteBitString( r->szFileName );
		MSG_WriteBits( r->nIndex, 12 );
		MSG_WriteBits( r->nDownloadSize, 24 );
		MSG_WriteBits( r->ucFlags & ( RES_FATALIFMISSING | RES_WASMISSING ), 3 );

		if ( r->ucFlags & RES_CUSTOM )
			MSG_WriteBitData( r->rgucMD5_hash, sizeof( r->rgucMD5_hash ) );

		if ( Q_memcmp( r->rguc_reserved, nullbuffer, sizeof( nullbuffer ) ) )
		{
			MSG_WriteBits( 1, 1 );
			MSG_WriteBitData( r->rguc_reserved, sizeof( r->rguc_reserved ) );
		}
		else
		{
			MSG_WriteBits( 0, 1 );
		}
	}

	SV_SendConsistencyList( msg );
	MSG_EndBitWriting( msg );
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void SV_SendRes_f()
{
	sizebuf_t msg;
	byte data[65536];

	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "SendResources";
	msg.data = data;
	msg.maxsize = sizeof( data );

	if ( cmd_source == src_command )
		return;

	if ( host_client->spawned && !host_client->active )
		return;

	if ( svs.maxclients > 1 && host_client->m_sendrescount >= 2 )
		return;

	host_client->m_sendrescount++;

	SV_SendResources( &msg );
	Netchan_CreateFragments( TRUE, &host_client->netchan, &msg );
	Netchan_FragSend( &host_client->netchan );
}

/*
===================
SV_FullClientUpdate

Writes all update values to a sizebuf
===================
*/
/*
===================
SV_FullClientUpdate

sends all the info about *cl to *sb
===================
*/
void SV_FullClientUpdate( client_t* client, sizebuf_t* sb )
{
	// Send the userinfo string
	char info[MAX_INFO_STRING];
	MD5Context_t ctx;
	byte digest[MD5_DIGEST_BYTES];

	Q_strncpy( info, client->userinfo, sizeof( info ) - 1 );
	info[sizeof( info ) - 1] = '\0';
	Info_RemovePrefixedKeys( info, '_' );

	MSG_WriteByte( sb, svc_updateuserinfo );
	MSG_WriteByte( sb, client - svs.clients );
	MSG_WriteLong( sb, client->userid );
	MSG_WriteString( sb, info );

	MD5Init( &ctx );
	MD5Update( &ctx, (byte*)client->hashedcdkey, sizeof( client->hashedcdkey ) );
	MD5Final( digest, &ctx );

	MSG_WriteBuf( sb, sizeof( digest ), digest );
}

void SV_SetMoveVars()
{
	movevars.gravity = sv_gravity.value;
	movevars.stopspeed = sv_stopspeed.value;
	movevars.maxspeed = sv_maxspeed.value;
	movevars.spectatormaxspeed = sv_spectatormaxspeed.value;
	movevars.accelerate = sv_accelerate.value;
	movevars.airaccelerate = sv_airaccelerate.value;
	movevars.wateraccelerate = sv_wateraccelerate.value;
	movevars.friction = sv_friction.value;
	movevars.edgefriction = sv_edgefriction.value;
	movevars.waterfriction = sv_waterfriction.value;
	movevars.bounce = sv_bounce.value;
	movevars.stepsize = sv_stepsize.value;
	movevars.entgravity = 1.0f;
	movevars.maxvelocity = sv_maxvelocity.value;
	movevars.zmax = sv_zmax.value;
	movevars.waveHeight = sv_wateramp.value;
	movevars.rollangle = sv_rollangle.value;
	movevars.rollspeed = sv_rollspeed.value;
	movevars.skycolor_r = sv_skycolor_r.value;
	movevars.skycolor_g = sv_skycolor_g.value;
	movevars.skycolor_b = sv_skycolor_b.value;
	movevars.skyvec_x = sv_skyvec_x.value;
	movevars.skyvec_y = sv_skyvec_y.value;
	movevars.skyvec_z = sv_skyvec_z.value;
	movevars.footsteps = (int)sv_footsteps.value;

	Q_strncpy( movevars.skyName, sv_skyname.string, sizeof( movevars.skyName ) - 1 );
	movevars.skyName[sizeof( movevars.skyName ) - 1] = '\0';
}

void SV_WriteMovevarsToClient( sizebuf_t* message )
{
	MSG_WriteByte( message, svc_newmovevars );
	MSG_WriteFloat( message, movevars.gravity );
	MSG_WriteFloat( message, movevars.stopspeed );
	MSG_WriteFloat( message, movevars.maxspeed );
	MSG_WriteFloat( message, movevars.spectatormaxspeed );
	MSG_WriteFloat( message, movevars.accelerate );
	MSG_WriteFloat( message, movevars.airaccelerate );
	MSG_WriteFloat( message, movevars.wateraccelerate );
	MSG_WriteFloat( message, movevars.friction );
	MSG_WriteFloat( message, movevars.edgefriction );
	MSG_WriteFloat( message, movevars.waterfriction );
	MSG_WriteFloat( message, movevars.entgravity );
	MSG_WriteFloat( message, movevars.bounce );
	MSG_WriteFloat( message, movevars.stepsize );
	MSG_WriteFloat( message, movevars.maxvelocity );
	MSG_WriteFloat( message, movevars.zmax );
	MSG_WriteFloat( message, movevars.waveHeight );
	MSG_WriteByte( message, movevars.footsteps != 0 );
	MSG_WriteFloat( message, movevars.rollangle );
	MSG_WriteFloat( message, movevars.rollspeed );
	MSG_WriteFloat( message, movevars.skycolor_r );
	MSG_WriteFloat( message, movevars.skycolor_g );
	MSG_WriteFloat( message, movevars.skycolor_b );
	MSG_WriteFloat( message, movevars.skyvec_x );
	MSG_WriteFloat( message, movevars.skyvec_y );
	MSG_WriteFloat( message, movevars.skyvec_z );
	MSG_WriteString( message, movevars.skyName );
}

void SV_WriteClientdataToMessage( client_t* client, sizebuf_t* msg )
{
#if defined( _WIN32 ) && !defined( SWDS_DLL )
	if ( !g_pclientdelta )
	{
		_wassert( L"g_pclientdelta",
			L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sv_main.c", 1764 );
	}
	if ( !g_pweapondelta )
	{
		_wassert( L"g_pweapondelta",
			L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sv_main.c", 1765 );
	}
#endif
	clientdata_t nullcd;
	weapon_data_t nullwd;
	clientdata_t* from;
	weapon_data_t* fdata;
	weapon_data_t* tdata;
	client_frame_t* frame;
	edict_t* ent;
	int i;

	Q_memset( &nullcd, 0, sizeof( nullcd ) );

	ent = client->edict;

	// This is the frame we are creating
	frame = &client->frames[client->netchan.outgoing_sequence & SV_UPDATE_MASK];
	// Save current time for next ping calculation
	frame->senttime = realtime;
	frame->ping_time = -1.0f;

	// send the chokecount for net usage graph
	if ( client->chokecount )
	{
		MSG_WriteByte( msg, svc_choke );
		client->chokecount = 0;
	}

	// a fixangle might get lost in a dropped packet.  Oh well.
	if ( ent->v.fixangle )
	{
		if ( ent->v.fixangle == 2 )
		{
			MSG_WriteByte( msg, svc_addangle );
			MSG_WriteHiresAngle( msg, ent->v.avelocity[YAW] );
			ent->v.avelocity[YAW] = 0;
		}
		else
		{
			MSG_WriteByte( msg, svc_setangle );
			MSG_WriteHiresAngle( msg, ent->v.angles[0] );
			MSG_WriteHiresAngle( msg, ent->v.angles[1] );
			MSG_WriteHiresAngle( msg, ent->v.angles[2] );
		}
		ent->v.fixangle = 0;
	}

	Q_memset( &frame->clientdata, 0, sizeof( frame->clientdata ) );
	gEntityInterface.pfnUpdateClientData( ent, host_client->lw != 0, &frame->clientdata );

	MSG_WriteByte( msg, svc_clientdata );

	if ( client->proxy )
		return;

	MSG_StartBitWriting( msg );

	if ( host_client->delta_sequence == -1 )
	{
		from = &nullcd;
		MSG_WriteBits( 0, 1 );
	}
	else
	{
		from = &host_client->frames[host_client->delta_sequence & SV_UPDATE_MASK].clientdata;
		MSG_WriteBits( 1, 1 );
		MSG_WriteBits( host_client->delta_sequence, 8 );
	}

	DELTA_WriteDelta( (byte*)from, (byte*)&frame->clientdata, TRUE, g_pclientdelta, NULL );

	if ( host_client->lw )
	{
		Q_memset( &nullwd, 0, sizeof( nullwd ) );

		if ( gEntityInterface.pfnGetWeaponData( host_client->edict, frame->weapondata ) )
		{
			for ( i = 0; i < MAX_WEAPON_DATA; i++ )
			{
				if ( host_client->delta_sequence == -1 )
					fdata = &nullwd;
				else
					fdata = &host_client->frames[host_client->delta_sequence & SV_UPDATE_MASK].weapondata[i];

				tdata = &frame->weapondata[i];

				if ( DELTA_CheckDelta( (byte*)fdata, (byte*)tdata, g_pweapondelta ) )
				{
					MSG_WriteBits( 1, 1 );
					MSG_WriteBits( i, 6 );
					DELTA_WriteDelta( (byte*)fdata, (byte*)tdata, TRUE, g_pweapondelta, NULL );
				}
			}
		}
	}

	MSG_WriteBits( 0, 1 );
	MSG_EndBitWriting( msg );
}

void SV_FullClientUpdate( client_t* client, sizebuf_t* message );
void SV_DropClient( client_t* client, qboolean crash, const char* format, ... );
void SV_WriteSpawn( sizebuf_t* message );
void SV_WriteClientdataToMessage( client_t* client, sizebuf_t* message );
char* Host_SaveGameDirectory();
void NotifyDedicatedServerUI( const char* message );
void SV_SetMoveVars();
void SV_WriteMovevarsToClient( sizebuf_t* message );

static void SV_WriteDeltaDescriptionsToClient( sizebuf_t* message )
{
	constexpr int DELTA_DESCRIPTION_COUNT_BITS = 16;
	delta_description_t emptyDescription;
	Q_memset( &emptyDescription, 0, sizeof( emptyDescription ) );
	for ( delta_info_t* info = g_sv_delta; info; info = info->next )
	{
		MSG_WriteByte( message, svc_deltadescription );
		MSG_WriteString( message, info->name );
		int count = info->delta->fieldCount;
		MSG_StartBitWriting( message );
		MSG_WriteBits( count, DELTA_DESCRIPTION_COUNT_BITS );
		for ( int index = 0; index < count; ++index )
			DELTA_WriteDelta( reinterpret_cast<byte*>( &emptyDescription ),
				reinterpret_cast<byte*>( &info->delta->pdd[index] ), 1, &g_MetaDelta, nullptr );
		MSG_EndBitWriting( message );
	}
}

void SV_SendServerinfo( sizebuf_t* message, client_t* client )
{
	constexpr int SERVER_INFO_TEXT_BYTES = 2048;
	constexpr int MAPCYCLE_TEXT_BYTES = 8192;
	constexpr int SERVER_PRINT_COLOR = 2;
	char text[SERVER_INFO_TEXT_BYTES];
	if ( developer.value != 0 || svs.maxclients > 1 )
	{
		MSG_WriteByte( message, svc_print );
		_snprintf( text, sizeof( text ), "%c\nBUILD %d SERVER (%i CRC)\nServer # %i\n", SERVER_PRINT_COLOR,
			build_number(), 0, svs.spawncount );
		MSG_WriteString( message, text );
	}

	MSG_WriteByte( message, svc_serverinfo );
	MSG_WriteLong( message, PROTOCOL_VERSION );
	MSG_WriteLong( message, svs.spawncount );
	int playerNumber = NUM_FOR_EDICT( client->edict ) - 1;
	CRC32_t mapCRC = sv.worldmapCRC;
	COM_Munge3( reinterpret_cast<byte*>( &mapCRC ), sizeof( mapCRC ), static_cast<byte>( ~playerNumber ) );
	MSG_WriteLong( message, mapCRC );
	MSG_WriteBuf( message, sizeof( sv.clientdllmd5 ), sv.clientdllmd5 );
	MSG_WriteByte( message, svs.maxclients );
	MSG_WriteByte( message, playerNumber );
	MSG_WriteByte( message, coop.value == 0 && deathmatch.value != 0 );
	COM_FileBase( com_gamedir, text );
	MSG_WriteString( message, text );
	MSG_WriteString( message, Cvar_VariableString( "hostname" ) );
	MSG_WriteString( message, sv.modelname );

	int length;
	byte* mapcycle = COM_LoadFileForMe( mapcyclefile.string, &length );
	if ( mapcycle && length )
	{
		if ( length > MAPCYCLE_TEXT_BYTES )
			mapcycle[MAPCYCLE_TEXT_BYTES - 1] = 0;
		MSG_WriteString( message, reinterpret_cast<char*>( mapcycle ) );
		COM_FreeFile( mapcycle );
	}
	else
	{
		MSG_WriteString( message, "mapcycle failure" );
	}
	MSG_WriteByte( message, 0 );
	MSG_WriteByte( message, svc_sendextrainfo );
	MSG_WriteString( message, com_clientfallback );
	MSG_WriteByte( message, sv_cheats.value != 0 );

	SV_WriteDeltaDescriptionsToClient( message );
	SV_SetMoveVars();
	SV_WriteMovevarsToClient( message );
	MSG_WriteByte( message, svc_cdtrack );
	MSG_WriteByte( message, gGlobalVariables.cdAudioTrack );
	MSG_WriteByte( message, gGlobalVariables.cdAudioTrack );
	MSG_WriteByte( message, svc_setview );
	MSG_WriteShort( message, NUM_FOR_EDICT( client->edict ) );
	client->spawned = 0;
	client->connected = 1;
	client->fully_connected = 0;
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void SV_New_f()
{
	constexpr int CONNECTION_MESSAGE_BYTES = 64 * 1024;
	constexpr int CONNECTION_NAME_BYTES = 64;
	constexpr int CONNECTION_ADDRESS_BYTES = 256;
	constexpr int CONNECTION_REJECT_BYTES = 128;
	byte data[CONNECTION_MESSAGE_BYTES];
	sizebuf_t message = {};
	message.buffername = "New Connection";
	message.data = data;
	message.maxsize = sizeof( data );
	// not valid on client
	if ( cmd_source == src_command )
		return;
	if ( host_client->hasusrmsgs && host_client->m_bSentNewResponse )
		return;
	if ( host_client->spawned && !host_client->active )
		return;

	edict_t* edict = host_client->edict;
	host_client->connected = 1;
	host_client->connection_started = realtime;
	host_client->m_sendrescount = 0;
	SZ_Clear( &host_client->netchan.message );
	SZ_Clear( &host_client->datagram );
	Netchan_Clear( &host_client->netchan );
	SV_SendServerinfo( &message, host_client );
	if ( sv_gpUserMsgs )
	{
		UserMsg* newUserMessages = sv_gpNewUserMsgs;
		sv_gpNewUserMsgs = sv_gpUserMsgs;
		SV_SendUserReg( &message );
		sv_gpNewUserMsgs = newUserMessages;
	}
	host_client->hasusrmsgs = 1;
	if ( ( host_client->active || host_client->spawned ) && host_client->edict )
		gEntityInterface.pfnClientDisconnect( host_client->edict );

	char name[CONNECTION_NAME_BYTES];
	char address[CONNECTION_ADDRESS_BYTES];
	char rejectReason[CONNECTION_REJECT_BYTES];
	_snprintf( name, sizeof( name ), "%s", host_client->name );
	_snprintf( address, sizeof( address ), "%s", NET_AdrToString( host_client->netchan.remote_address ) );
	_snprintf( rejectReason, sizeof( rejectReason ), "Connection rejected by game\n" );
	if ( !gEntityInterface.pfnClientConnect( edict, name, address, rejectReason ) )
	{
		MSG_WriteByte( &host_client->netchan.message, svc_stufftext );
		MSG_WriteString( &host_client->netchan.message, va( "echo %s\n", rejectReason ) );
		SV_DropClient( host_client, 0, "Server refused connection because:  %s", rejectReason );
		return;
	}

	MSG_WriteByte( &message, svc_stufftext );
	MSG_WriteString( &message, va( "fullserverinfo \"%s\"\n", Info_Serverinfo() ) );
	MSG_WriteByte( &message, svc_stufftext );
	MSG_WriteString( &message, va( "allow_shaders %d\n", sv_allow_shaders.value != 0 ) );
	MSG_WriteByte( &message, svc_stufftext );
	MSG_WriteString( &message, va( "allow_autoaim %d\n", sv_allow_autoaim.value != 0 ) );

	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		if ( client == host_client || client->active || client->connected || client->spawned )
			SV_FullClientUpdate( client, &message );
	}
	Netchan_CreateFragments( 1, &host_client->netchan, &message );
	Netchan_FragSend( &host_client->netchan );
	host_client->m_bSentNewResponse = 1;
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif // #if defined( DEBUG_NETWORKING )
void SV_Spawn_f()
{
	constexpr int SPAWN_MESSAGE_BYTES = 64 * 1024;
	constexpr int SPAWN_ARGUMENT_COUNT = 3;
	byte data[SPAWN_MESSAGE_BYTES];
	sizebuf_t message = {};
	message.buffername = "Spawning";
	message.data = data;
	message.maxsize = sizeof( data );

	if ( Cmd_Argc() != SPAWN_ARGUMENT_COUNT )
	{
		Con_Printf( "spawn is not valid\n" );
		return;
	}
	if ( cmd_source == src_command )
	{
		Con_Printf( "spawn is not valid from the console\n" );
		return;
	}
	if ( host_client->spawned )
	{
		Con_Printf( "spawn is not valid when already spawned\n" );
		return;
	}

	host_client->crcValue = std::atoi( Cmd_Argv( 2 ) );
	COM_UnMunge2( reinterpret_cast<byte*>( &host_client->crcValue ), sizeof( host_client->crcValue ),
		static_cast<byte>( ~svs.spawncount ) );
	if ( !cls.demoplayback && Q_atoi( Cmd_Argv( 1 ) ) != svs.spawncount )
	{
		SV_New_f();
		return;
	}
	SZ_Write( &message, sv.signon.data, sv.signon.cursize );
	SV_WriteSpawn( &message );
	MSG_WriteByte( &message, svc_voiceinit );
	MSG_WriteString( &message, com_empty_string );
	MSG_WriteByte( &message, 0 );
	Netchan_CreateFragments( 1, &host_client->netchan, &message );
	Netchan_FragSend( &host_client->netchan );
}

void SV_WriteSpawn( sizebuf_t* message )
{
	constexpr int SAVED_GAME_PATH_BYTES = 256;
	if ( sv.loadgame )
	{
		if ( host_client->proxy )
		{
			Con_Printf( "ERROR! Spectator mode doesn't work with saved game.\n" );
			return;
		}
		sv.paused = 0;
	}
	else
	{
		sv.state = ss_loading;
		ReleaseEntityDLLFields( sv_player );
		Q_memset( &sv_player->v, 0, sizeof( sv_player->v ) );
		InitEntityDLLFields( sv_player );
		sv_player->v.colormap = NUM_FOR_EDICT( sv_player );
		sv_player->v.netname = static_cast<string_t>( host_client->name - pr_strings );
		if ( host_client->proxy )
			sv_player->v.flags |= FL_PROXY;
		gGlobalVariables.time = static_cast<float>( sv.time );
		gEntityInterface.pfnClientPutInServer( sv_player );
		sv.state = ss_active;
	}

	SZ_Clear( &host_client->netchan.message );
	SZ_Clear( &host_client->datagram );
	MSG_WriteByte( message, svc_time );
	MSG_WriteFloat( message, static_cast<float>( sv.time ) );
	host_client->sendinfo = 1;
	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		if ( client == host_client || client->active || client->connected || client->spawned )
			SV_FullClientUpdate( client, message );
	}
	for ( int index = 0; index < MAX_LIGHTSTYLES; ++index )
	{
		MSG_WriteByte( message, svc_lightstyle );
		MSG_WriteByte( message, index );
		MSG_WriteString( message, sv.lightstyles[index] );
	}
	if ( !host_client->proxy )
	{
		MSG_WriteByte( message, svc_setangle );
		MSG_WriteHiresAngle( message, sv_player->v.v_angle[0] );
		MSG_WriteHiresAngle( message, sv_player->v.v_angle[1] );
		MSG_WriteHiresAngle( message, 0 );
		SV_WriteClientdataToMessage( host_client, message );
		if ( sv.loadgame )
		{
			SAVERESTOREDATA saveData = {};
			gGlobalVariables.pSaveData = &saveData;
			gEntityInterface.pfnParmsChangeLevel();
			MSG_WriteByte( message, svc_restore );
			char filename[SAVED_GAME_PATH_BYTES];
			_snprintf( filename, sizeof( filename ), "%s%s.HL2", Host_SaveGameDirectory(), sv.name );
			COM_FixSlashes( filename );
			MSG_WriteString( message, filename );
			MSG_WriteByte( message, saveData.connectionCount );
			for ( int index = 0; index < saveData.connectionCount; ++index )
				MSG_WriteString( message, saveData.levelList[index].mapName );
			sv.loadgame = 0;
			gGlobalVariables.pSaveData = nullptr;
		}
	}
	MSG_WriteByte( message, svc_signonnum );
	MSG_WriteByte( message, 1 );
	host_client->active = 1;
	host_client->connecttime = 0;
	host_client->ignorecmdtime = 0;
	host_client->cmdtime = 0;
	host_client->spawned = 1;
	host_client->connected = 1;
	host_client->fully_connected = 0;
	NotifyDedicatedServerUI( "UpdatePlayers" );
}

constexpr int MAX_RCON_FAILURES = 20;
constexpr int MAX_RCON_ADDRESSES = 32;

typedef struct
{
	qboolean active;
	qboolean shouldreject;
	netadr_t adr;
	int num_failures;
	float last_update;
	float failure_times[MAX_RCON_FAILURES];
} rcon_failure_t;

static_assert( sizeof( rcon_failure_t ) == 116 );
static rcon_failure_t g_rgRconFailures[MAX_RCON_ADDRESSES];

void SV_ResetRcon_f()
{
	Q_memset( g_rgRconFailures, 0, sizeof( g_rgRconFailures ) );
}

redirect_t sv_redirected;
netadr_t sv_redirectto;
char outputbuf[1400];

constexpr int MAX_CHALLENGES = 1024;

typedef struct
{
	netadr_t adr; // Address where challenge value was sent to.
	int challenge; // To connect, adr IP address must respond with this #
	int time; // # is valid for only a short duration.
} challenge_t;

static_assert( sizeof( challenge_t ) == 28 );
static challenge_t g_rg_sv_challenges[MAX_CHALLENGES];

/*
================
SV_CheckChallenge

Make sure connecting client is not spoofing
================
*/
int SV_CheckChallenge( netadr_t* address, int challenge )
{
	if ( !address )
	{
		Sys_Error( "SV_CheckChallenge:  Null address\n" );
	}
	// See if the challenge is valid
	// Don't care if it is a local address.
	if ( NET_IsLocalAddress( *address ) )
		return 1;
	for ( int i = 0; i < MAX_CHALLENGES; ++i )
	{
		if ( NET_CompareBaseAdr( net_from, g_rg_sv_challenges[i].adr ) )
		{
			if ( challenge == g_rg_sv_challenges[i].challenge )
				return 1;
			SV_RejectConnection( address, "Bad challenge.\n" );
			return 0;
		}
	}
	SV_RejectConnection( address, "No challenge for your address.\n" );
	return 0;
}

void SV_FlushRedirect()
{
	constexpr int REDIRECT_PACKET_OVERHEAD = 7;
	constexpr int REDIRECT_PACKET_MESSAGE = 'l';
	constexpr int CLIENT_PRINT_MESSAGE = 8;
	if ( sv_redirected == RD_PACKET )
	{
		byte data[sizeof( outputbuf ) + REDIRECT_PACKET_OVERHEAD];
		sizebuf_t buffer = {
			"Redirected Text", FSB_ALLOWOVERFLOW, data, Q_strlen( outputbuf ) + REDIRECT_PACKET_OVERHEAD, 0 };
		MSG_WriteLong( &buffer, -1 );
		MSG_WriteByte( &buffer, REDIRECT_PACKET_MESSAGE );
		MSG_WriteString( &buffer, outputbuf );
		MSG_WriteByte( &buffer, 0 );
		NET_SendPacket( NS_SERVER, buffer.cursize, buffer.data, sv_redirectto );
	}
	else if ( sv_redirected == RD_CLIENT )
	{
		MSG_WriteByte( &host_client->netchan.message, CLIENT_PRINT_MESSAGE );
		MSG_WriteString( &host_client->netchan.message, outputbuf );
	}
	outputbuf[0] = 0;
}

void SV_AddFailedRcon( netadr_t* address )
{
	constexpr float INITIAL_FAILURE_AGE = -99999.0f;
	sv_rcon_minfailures.value = static_cast<float>( static_cast<int>( sv_rcon_minfailures.value ) );
	sv_rcon_maxfailures.value = static_cast<float>( static_cast<int>( sv_rcon_maxfailures.value ) );
	sv_rcon_minfailuretime.value = static_cast<float>( static_cast<int>( sv_rcon_minfailuretime.value ) );
	if ( sv_rcon_minfailures.value < 1.0f )
		Cvar_SetValue( "sv_rcon_minfailures", 1.0f );
	if ( sv_rcon_minfailures.value > MAX_RCON_FAILURES )
		Cvar_SetValue( "sv_rcon_minfailures", MAX_RCON_FAILURES );
	if ( sv_rcon_maxfailures.value < 1.0f )
		Cvar_SetValue( "sv_rcon_maxfailures", 1.0f );
	if ( sv_rcon_maxfailures.value > MAX_RCON_FAILURES )
		Cvar_SetValue( "sv_rcon_maxfailures", MAX_RCON_FAILURES );
	if ( sv_rcon_minfailures.value > sv_rcon_maxfailures.value )
	{
		float maximum = sv_rcon_maxfailures.value;
		Cvar_SetValue( "sv_rcon_maxfailures", sv_rcon_minfailures.value );
		Cvar_SetValue( "sv_rcon_minfailures", maximum );
	}
	if ( sv_rcon_minfailuretime.value < 1.0f )
		Cvar_SetValue( "sv_rcon_minfailuretime", 1.0f );
	int best = 0;
	float bestUpdate = INITIAL_FAILURE_AGE;
	qboolean found = false;
	rcon_failure_t* failure = nullptr;
	for ( int i = 0; i < MAX_RCON_ADDRESSES; ++i )
	{
		failure = &g_rgRconFailures[i];
		if ( !failure->active )
			break;
		if ( NET_CompareAdr( failure->adr, *address ) )
		{
			found = true;
			break;
		}
		float age = static_cast<float>( realtime - failure->last_update );
		if ( age > bestUpdate )
		{
			bestUpdate = age;
			best = i;
		}
		if ( i == MAX_RCON_ADDRESSES - 1 )
			failure = &g_rgRconFailures[best];
	}
	if ( !failure || ( found && failure->shouldreject ) )
		return;
	failure->active = true;
	failure->shouldreject = false;
	failure->last_update = static_cast<float>( realtime );
	failure->adr = *address;
	if ( !found )
		failure->num_failures = 0;
#if defined( _WIN32 ) && !defined( SWDS_DLL )
	if ( static_cast<float>( failure->num_failures ) >= sv_rcon_maxfailures.value )
	{
		for ( int i = 1; sv_rcon_maxfailures.value > static_cast<float>( i ); ++i )
			failure->failure_times[i - 1] = failure->failure_times[i];
		--failure->num_failures;
	}
#else
	int maxFailures = (int)sv_rcon_maxfailures.value;
	if ( maxFailures < 1 )
		maxFailures = 1;
	if ( maxFailures > MAX_RCON_FAILURES )
		maxFailures = MAX_RCON_FAILURES;
	if ( failure->num_failures < 0 )
		failure->num_failures = 0;
	if ( failure->num_failures >= maxFailures )
	{
		for ( int i = 1; i < maxFailures; ++i )
			failure->failure_times[i - 1] = failure->failure_times[i];
		failure->num_failures = maxFailures - 1;
	}
#endif
	failure->failure_times[failure->num_failures] = static_cast<float>( realtime );
	++failure->num_failures;
	int failed = 0;
	for ( int i = 0; i < failure->num_failures; ++i )
	{
		float age = static_cast<float>( realtime - failure->failure_times[i] );
		if ( sv_rcon_minfailuretime.value > age )
			++failed;
	}
	if ( static_cast<float>( failed ) >= sv_rcon_minfailures.value )
	{
		Con_Printf( "User %s will be banned for rcon hacking\n", NET_AdrToString( *address ) );
		failure->shouldreject = true;
	}
}

int SV_Rcon_Validate()
{
	if ( Cmd_Argc() < 3 || Q_strlen( rcon_password.string ) == 0 )
		return 1;
	if ( sv_rcon_banpenalty.value < 0.0f )
		Cvar_SetValue( "sv_rcon_banpenalty", 0.0f );
	for ( int i = 0; i < MAX_RCON_ADDRESSES; ++i )
	{
		rcon_failure_t& failure = g_rgRconFailures[i];
		if ( failure.active && NET_CompareAdr( net_from, failure.adr ) && failure.shouldreject )
		{
			Con_Printf( "Banning %s for rcon hacking attempts\n", NET_AdrToString( net_from ) );
			Cbuf_AddText(
				va( "addip %i %s\n", static_cast<int>( sv_rcon_banpenalty.value ), NET_BaseAdrToString( net_from ) ) );
			return 3;
		}
	}
	int challenge = Q_atoi( Cmd_Argv( 1 ) );
	if ( !SV_CheckChallenge( &net_from, challenge ) )
		return 2;
	if ( Q_strcmp( Cmd_Argv( 2 ), rcon_password.string ) )
	{
		SV_AddFailedRcon( &net_from );
		return 1;
	}
	return 0;
}

void SV_Rcon( netadr_t* address )
{
	constexpr int RCON_BUFFER_BYTES = 512;
	constexpr int RCON_COMMAND_BYTES = 1024;
	int invalid = SV_Rcon_Validate();
	int length = static_cast<int>(
		static_cast<unsigned int>( net_message.cursize ) - static_cast<unsigned int>( Q_strlen( "rcon" ) ) );
	if ( static_cast<unsigned int>( length ) - 1 > RCON_BUFFER_BYTES - 3 )
		return;
	char rcon_buff[RCON_BUFFER_BYTES];
	Q_memcpy( rcon_buff, net_message.data + Q_strlen( "rcon" ), length );
	rcon_buff[length] = 0;
	const char* logFormat;
	if ( invalid )
	{
		Con_Printf( "Bad Rcon from %s:\n%s\n", NET_AdrToString( *address ), rcon_buff );
		logFormat = "Bad Rcon: \"%s\" from \"%s\"\n";
	}
	else
	{
		Con_Printf( "Rcon from %s:\n%s\n", NET_AdrToString( *address ), rcon_buff );
		logFormat = "Rcon: \"%s\" from \"%s\"\n";
	}
	Log_Printf( logFormat, rcon_buff, NET_AdrToString( *address ) );
	sv_redirected = RD_PACKET;
	sv_redirectto = *address;
	outputbuf[0] = 0;
	if ( invalid )
	{
		if ( invalid == 2 || std::strlen( rcon_password.string ) != 0 )
			Con_Printf( "Bad rcon_password.\n" );
		else
			Con_Printf( "Bad rcon_password.\nNo password set for this server.\n" );
	}
	else
	{
		const char* command = COM_Parse( rcon_buff );
		command = COM_Parse( command );
		command = COM_Parse( command );
		if ( !command )
		{
			Con_Printf( "Empty rcon\n" );
			return;
		}
		char remaining[RCON_COMMAND_BYTES];
		Q_strncpy( remaining, command, sizeof( remaining ) - 1 );
		remaining[sizeof( remaining ) - 1] = 0;
		Cmd_ExecuteString( remaining, src_command );
	}
	SV_FlushRedirect();
	sv_redirected = RD_NONE;
}

constexpr int SERVER_PING_MAXIMUM_SAMPLES = 16;
constexpr float SERVER_PING_MILLISECONDS_PER_SECOND = 1000.0f;
constexpr float SERVER_PING_INTEGER_LIMIT = 2147483648.0f;

int SV_CalcPing( client_t* client )
{
	if ( client->fakeclient )
		return 0;
	int frameCount = SV_UPDATE_BACKUP / 2;
	if ( frameCount >= SERVER_PING_MAXIMUM_SAMPLES )
		frameCount = SERVER_PING_MAXIMUM_SAMPLES;
	else if ( frameCount <= 0 )
		return 0;

	unsigned int sequence = static_cast<unsigned int>( client->netchan.incoming_acknowledged ) - 1;
	float total = 0.0f;
	int samples = 0;
	for ( int frameIndex = 0; frameIndex < frameCount; ++frameIndex, --sequence )
	{
		float ping = client->frames[sequence & static_cast<unsigned int>( SV_UPDATE_MASK )].ping_time;
		if ( ping > 0.0f )
		{
			total += ping;
			++samples;
		}
	}
	if ( !samples )
		return 0;
	float average = total / static_cast<float>( samples );
	if ( average < 0.0f )
		average = 0.0f;
	float milliseconds = average * SERVER_PING_MILLISECONDS_PER_SECOND;
	if ( !( milliseconds >= -SERVER_PING_INTEGER_LIMIT && milliseconds < SERVER_PING_INTEGER_LIMIT ) )
	{
		std::feraiseexcept( FE_INVALID );
		return ( std::numeric_limits<int>::min )();
	}
	return static_cast<int>( milliseconds );
}

extern delta_t* g_peventdelta;

void SV_EmitEvents( client_t* client, packet_entities_t* packet, sizebuf_t* message )
{
#if defined( _WIN32 ) && !defined( SWDS_DLL )
	if ( !g_peventdelta )
	{
		_wassert( L"g_peventdelta",
			L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sv_main.c", 5154 );
	}
#endif
	constexpr int EVENT_COUNT_BITS = 5;
	constexpr int EVENT_INDEX_BITS = 10;
	constexpr int EVENT_ENTITY_BITS = 11;
	constexpr int EVENT_DELAY_BITS = 16;
	constexpr int MAX_MESSAGE_EVENTS = ( 1 << EVENT_COUNT_BITS ) - 1;
	constexpr int EVENT_MESSAGE = 3;
	constexpr double EVENT_DELAY_SCALE = 100.0;

	event_args_t nullArgs;
	Q_memset( &nullArgs, 0, sizeof( nullArgs ) );
	int count = 0;
	for ( const event_info_t& event : client->events.ei )
	{
		if ( event.index )
			++count;
	}
	if ( !count )
		return;
	if ( count > MAX_MESSAGE_EVENTS )
		count = MAX_MESSAGE_EVENTS;

	for ( event_info_t& event : client->events.ei )
	{
		if ( !event.index )
			continue;
		int entity = 0;
		while ( entity < packet->num_entities )
		{
			if ( packet->entities[entity].number == event.entity_index )
				break;
			++entity;
		}
		if ( entity < packet->num_entities )
		{
			event.packet_index = static_cast<short>( entity );
			event.args.ducking = 0;
			if ( !( event.args.flags & FEVENT_ORIGIN ) )
			{
				for ( float& coordinate : event.args.origin )
					coordinate = 0.0f;
			}
			if ( !( event.args.flags & FEVENT_ANGLES ) )
			{
				for ( float& angle : event.args.angles )
					angle = 0.0f;
			}
		}
		else
		{
			event.packet_index = static_cast<short>( packet->num_entities );
			event.args.entindex = event.entity_index;
		}
	}

	MSG_WriteByte( message, EVENT_MESSAGE );
	MSG_StartBitWriting( message );
	MSG_WriteBits( count, EVENT_COUNT_BITS );
	int emitted = 0;
	for ( event_info_t& event : client->events.ei )
	{
		if ( event.index )
		{
			if ( emitted < count )
			{
				MSG_WriteBits( event.index, EVENT_INDEX_BITS );
				if ( event.packet_index != -1 )
				{
					MSG_WriteBits( 1, 1 );
					MSG_WriteBits( event.packet_index, EVENT_ENTITY_BITS );
					if ( Q_memcmp( &nullArgs, &event.args, sizeof( nullArgs ) ) )
					{
						MSG_WriteBits( 1, 1 );
						DELTA_WriteDelta( reinterpret_cast<byte*>( &nullArgs ), reinterpret_cast<byte*>( &event.args ),
							1, g_peventdelta, nullptr );
					}
					else
						MSG_WriteBits( 0, 1 );
				}
				else
					MSG_WriteBits( 0, 1 );

				if ( event.fire_time == 0.0f )
					MSG_WriteBits( 0, 1 );
				else
				{
					MSG_WriteBits( 1, 1 );
					double scaledDelay = static_cast<double>( event.fire_time ) * EVENT_DELAY_SCALE;
					int delay = INT_MIN;
					if ( scaledDelay >= static_cast<double>( INT_MIN )
						 && scaledDelay < -static_cast<double>( INT_MIN ) )
						delay = static_cast<int>( scaledDelay );
					MSG_WriteBits( static_cast<std::uint32_t>( delay ), EVENT_DELAY_BITS );
				}
			}
			++emitted;
			event.index = 0;
		}
		event.packet_index = -1;
		event.entity_index = -1;
	}
	MSG_EndBitWriting( message );
}

void Steam_Activate();
void SV_CreateGenericResources();
void SV_Physics();
void SV_CreateBaseline();
void SV_CreateResourceList();
int SV_TransferConsistencyInfo();
void SV_SendServerinfo( sizebuf_t* message, client_t* client );
extern delta_t* g_pcustomentitydelta;
extern delta_t* g_pentitydelta;
extern delta_t* g_pplayerdelta;

static extra_baselines_s g_sv_instance_baselines;
static int sv_lastnum;
int sv_playermodel;
deltacallback_t g_svdeltacallback;

constexpr int SERVER_ACTIVATION_MESSAGE_BYTES = 65536;
constexpr int SERVER_MULTIPLAYER_SETTLE_FRAMES = 16;
constexpr double SERVER_MULTIPLAYER_SETTLE_TIME = 0.8;
constexpr double SERVER_SINGLEPLAYER_SETTLE_TIME = 0.1;
constexpr double SERVER_ACTIVATION_MINIMUM_TIME = 0.001;

qboolean PR_IsEmptyString( const char* text );

void SV_CreateGenericResources()
{
	constexpr int GENERIC_RESOURCE_PATH_BYTES = 260;
	constexpr int GENERIC_RESOURCE_HEAP_LOAD = 5;
	static const struct
	{
		const char* extension;
		const char* message;
	} forbiddenExtensions[] = {
		{ ".cfg", "Can't precache .cfg files:  %s\n" },
		{ ".rc", "Can't precache .rc files:  %s\n" },
		{ ".lst", "Can't precache .lst files:  %s\n" },
		{ ".exe", "Can't precache .exe files:  %s\n" },
		{ ".vbs", "Can't precache .vbs files:  %s\n" },
		{ ".com", "Can't precache .com files:  %s\n" },
		{ ".bat", "Can't precache .bat files:  %s\n" },
		{ ".dll", "Can't precache .dll files:  %s\n" },
		{ ".asi", "Can't precache .asi files:  %s\n" },
		{ ".mix", "Can't precache .mix files:  %s\n" },
		{ ".flt", "Can't precache .flt files:  %s\n" },
	};
	char filename[GENERIC_RESOURCE_PATH_BYTES];
	COM_StripExtension( sv.modelname, filename );
	COM_DefaultExtension( filename, ".res" );
	COM_FixSlashes( filename );
	byte* file = COM_LoadFile( filename, GENERIC_RESOURCE_HEAP_LOAD, nullptr );
	if ( !file )
		return;
	const char* data = reinterpret_cast<const char*>( file );
	if ( file[0] == 0xEF && file[1] == 0xBB && file[2] == 0xBF )
		data += 3;
	Con_DPrintf( "Precaching from %s\n", filename );
	Con_DPrintf( "----------------------------------\n" );
	sv.num_generic_names = 0;
	for ( data = COM_Parse( data ); Q_strlen( com_token ) > 0; data = COM_Parse( data ) )
	{
		if ( com_token[0] == '\\' || com_token[0] == '/' )
		{
			Con_Printf( "Can't precache resource with absolute path %s\n", com_token );
			continue;
		}
		if ( Q_strstr( com_token, ".." ) )
		{
			Con_Printf( "Can't precache resource with invalid relative path %s\n", com_token );
			continue;
		}
		if ( Q_strstr( com_token, ":" ) )
		{
			Con_Printf( "Can't precache resource with absolute path %s\n", com_token );
			continue;
		}
		bool rejected = false;
		for ( const auto& forbidden : forbiddenExtensions )
		{
			if ( Q_stristr( com_token, forbidden.extension ) )
			{
				Con_Printf( forbidden.message, com_token );
				rejected = true;
				break;
			}
		}
		if ( rejected )
			continue;
		if ( PR_IsEmptyString( com_token ) )
		{
			Con_Printf( "Can't precache invalid files:  %s\n", com_token );
			continue;
		}
		char* name = sv.generic_precache_names[sv.num_generic_names];
		Q_strncpy( name, com_token, MAX_QPATH - 1 );
		name[MAX_QPATH - 1] = 0;
		PF_precache_generic_I( name );
		Con_DPrintf( "  %s\n", sv.generic_precache_names[sv.num_generic_names] );
		++sv.num_generic_names;
	}
	Con_DPrintf( "----------------------------------\n" );
	COM_FreeFile( file );
}

void SV_CreateBaseline()
{
	constexpr int BASELINE_ENTITY_INDEX_BITS = 11;
	constexpr int BASELINE_ENTITY_TYPE_BITS = 2;
	constexpr int BASELINE_TERMINATOR = 0xFFFF;
	constexpr int BASELINE_TERMINATOR_BITS = 16;
	constexpr int INSTANCE_BASELINE_COUNT_BITS = 6;
#if defined( _WIN32 ) && !defined( SWDS_DLL )
	if ( !g_pcustomentitydelta || !g_pentitydelta || !g_pplayerdelta )
	{
		_wassert( L"g_pcustomentitydelta && g_pentitydelta && g_pplayerdelta",
			L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sv_main.c", 7026 );
	}
#endif
	sv.instance_baselines = &g_sv_instance_baselines;
	entity_state_t emptyState;
	Q_memset( &emptyState, 0, sizeof( emptyState ) );
	sv_playermodel = -1;
	for ( int model = 0; model < MAX_MODELS && sv.model_precache[model]; ++model )
	{
		if ( Q_strcasecmp( sv.model_precache[model], "models/player.mdl" ) == 0 )
			sv_playermodel = model;
	}
	for ( int index = 0; index < sv.num_edicts; ++index )
	{
		edict_t* entity = &sv.edicts[index];
		if ( entity->free || ( index > svs.maxclients && !entity->v.modelindex ) )
			continue;
		entity_state_t* baseline = &sv.baselines[index];
		baseline->number = index;
		baseline->entityType = entity->v.flags & FL_CUSTOMENTITY ? ENTITY_BEAM : ENTITY_NORMAL;
		int player = index >= 1 && index <= svs.maxclients;
		gEntityInterface.pfnCreateBaseline(
			player, index, baseline, entity, sv_playermodel, player_mins[0], player_maxs[0] );
		sv_lastnum = index;
	}
	gEntityInterface.pfnCreateInstancedBaselines();
	MSG_WriteByte( &sv.signon, svc_spawnbaseline );
	MSG_StartBitWriting( &sv.signon );
	for ( int index = 0; index < sv.num_edicts; ++index )
	{
		edict_t* entity = &sv.edicts[index];
		if ( entity->free || ( index > svs.maxclients && !entity->v.modelindex ) )
			continue;
		entity_state_t* baseline = &sv.baselines[index];
		MSG_WriteBits( index, BASELINE_ENTITY_INDEX_BITS );
		MSG_WriteBits( baseline->entityType, BASELINE_ENTITY_TYPE_BITS );
		delta_t* fields;
		if ( baseline->entityType & ENTITY_NORMAL )
			fields = index >= 1 && index <= svs.maxclients ? g_pplayerdelta : g_pentitydelta;
		else
			fields = g_pcustomentitydelta;
		DELTA_WriteDelta(
			reinterpret_cast<byte*>( &emptyState ), reinterpret_cast<byte*>( baseline ), 1, fields, nullptr );
	}
	MSG_WriteBits( BASELINE_TERMINATOR, BASELINE_TERMINATOR_BITS );
	MSG_WriteBits( sv.instance_baselines->number, INSTANCE_BASELINE_COUNT_BITS );
	for ( int index = 0; index < sv.instance_baselines->number; ++index )
	{
		DELTA_WriteDelta( reinterpret_cast<byte*>( &emptyState ),
			reinterpret_cast<byte*>( &sv.instance_baselines->baseline[index] ), 1, g_pentitydelta, nullptr );
	}
	MSG_EndBitWriting( &sv.signon );
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void SV_ActivateServer( int runPhysics )
{
	byte data[SERVER_ACTIVATION_MESSAGE_BYTES];
	sizebuf_t message;
	Q_memset( &message, 0, sizeof( message ) );
	message.buffername = "Activate Server";
	message.data = data;
	message.cursize = 0;
	message.maxsize = sizeof( data );

	SetCStrikeFlags();
	Cvar_Set( "sv_newunit", "0" );
	ContinueLoadingProgressBar( "Server", 8, 0.0f );
	gEntityInterface.pfnServerActivate( sv.edicts, sv.num_edicts, svs.maxclients );
	Steam_Activate();
	ContinueLoadingProgressBar( "Server", 9, 0.0f );
	SV_CreateGenericResources();
	sv.active = 1;
	sv.state = ss_active;
	ContinueLoadingProgressBar( "Server", 10, 0.0f );

	if ( runPhysics )
	{
		if ( svs.maxclients > 1 )
		{
			host_frametime = SERVER_MULTIPLAYER_SETTLE_TIME;
			for ( int frame = 0; frame < SERVER_MULTIPLAYER_SETTLE_FRAMES; ++frame )
				SV_Physics();
		}
		else
		{
			host_frametime = SERVER_SINGLEPLAYER_SETTLE_TIME;
			SV_Physics();
			SV_Physics();
		}
	}
	else
	{
		host_frametime = SERVER_ACTIVATION_MINIMUM_TIME;
		SV_Physics();
	}

	SV_CreateBaseline();
	SV_CreateResourceList();
	sv.num_consistency = SV_TransferConsistencyInfo();
	host_client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++host_client )
	{
		if ( ( !host_client->active && !host_client->connected ) || host_client->fakeclient )
			continue;
		Netchan_Clear( &host_client->netchan );
		if ( svs.maxclients > 1 )
		{
			MSG_WriteByte( &host_client->netchan.message, svc_stufftext );
			MSG_WriteString( &host_client->netchan.message, "reconnect\n" );
			Netchan_Transmit( &host_client->netchan, 0, nullptr );
		}
		else
		{
			SV_SendServerinfo( &message, host_client );
		}
		if ( sv_gpUserMsgs )
		{
			UserMsg* newUserMessages = sv_gpNewUserMsgs;
			sv_gpNewUserMsgs = sv_gpUserMsgs;
			SV_SendUserReg( &message );
			sv_gpNewUserMsgs = newUserMessages;
		}
		host_client->hasusrmsgs = 1;
		Netchan_CreateFragments( 1, &host_client->netchan, &message );
		Netchan_FragSend( &host_client->netchan );
		SZ_Clear( &message );
	}

	HPAK_FlushHostQueue();
	if ( svs.maxclients > 1 )
		Con_DPrintf( "%i player server started\n", svs.maxclients );
	else
		Con_DPrintf( "Game started\n" );
	Log_Printf( "Started map \"%s\" (CRC \"%i\")\n", sv.name, sv.worldmapCRC );
}

void SV_WriteDeltaHeader( int number, qboolean remove, qboolean custom, int* numberBase, qboolean newBaseline,
	int newBaselineIndex, qboolean full, int offset )
{
	constexpr int ENTITY_NUMBER_BITS = 11;
	constexpr int ENTITY_NUMBER_DELTA_BITS = 6;
	constexpr int BASELINE_INDEX_BITS = 6;
	constexpr int BASELINE_OFFSET_BITS = 6;
	constexpr unsigned int MAX_COMPACT_ENTITY_DELTA = ( 1u << ENTITY_NUMBER_DELTA_BITS ) - 1;
	unsigned int difference = static_cast<unsigned int>( number ) - static_cast<unsigned int>( *numberBase );
	if ( full && difference == 1 )
	{
		MSG_WriteBits( 1, 1 );
	}
	else
	{
		MSG_WriteBits( full ? 0 : remove != 0, 1 );
		if ( difference - 1 >= MAX_COMPACT_ENTITY_DELTA )
		{
			MSG_WriteBits( 1, 1 );
			MSG_WriteBits( number, ENTITY_NUMBER_BITS );
		}
		else
		{
			MSG_WriteBits( 0, 1 );
			MSG_WriteBits( difference, ENTITY_NUMBER_DELTA_BITS );
		}
	}
	*numberBase = number;
	if ( remove )
		return;
	MSG_WriteBits( custom != 0, 1 );
	if ( sv.instance_baselines->number != 0 )
	{
		MSG_WriteBits( newBaseline != 0, 1 );
		if ( newBaseline )
			MSG_WriteBits( newBaselineIndex, BASELINE_INDEX_BITS );
	}
	if ( full && !newBaseline )
	{
		MSG_WriteBits( offset != 0, 1 );
		if ( offset )
			MSG_WriteBits( offset, BASELINE_OFFSET_BITS );
	}
}

void SV_InvokeCallback()
{
	SV_WriteDeltaHeader( g_svdeltacallback.num, g_svdeltacallback.remove, g_svdeltacallback.custom,
		g_svdeltacallback.numbase, g_svdeltacallback.newbl, g_svdeltacallback.newblindex, g_svdeltacallback.full,
		g_svdeltacallback.offset );
}

int SV_FindBestBaseline( int index, entity_state_t** baseline, entity_state_t* to, int number, qboolean custom )
{
	constexpr int BASELINE_OFFSET_BITS = 6;
	constexpr int MAX_BASELINE_OFFSET = ( 1 << BASELINE_OFFSET_BITS ) - 1;
	delta_t* fields =
		custom ? g_pcustomentitydelta : ( number >= 1 && number <= svs.maxclients ? g_pplayerdelta : g_pentitydelta );
	int bestBits =
		DELTA_TestDelta( reinterpret_cast<byte*>( *baseline ), reinterpret_cast<byte*>( &to[index] ), fields )
		- BASELINE_OFFSET_BITS;
	int bestIndex = index;
	for ( int previous = index - 1; previous >= 0 && index - previous < MAX_BASELINE_OFFSET && bestBits > 0;
		--previous )
	{
		if ( to[index].entityType != to[previous].entityType )
			continue;
		fields = custom ? g_pcustomentitydelta
						: ( number >= 1 && number <= svs.maxclients ? g_pplayerdelta : g_pentitydelta );
		int bits =
			DELTA_TestDelta( reinterpret_cast<byte*>( &to[previous] ), reinterpret_cast<byte*>( &to[index] ), fields );
		if ( bits < bestBits )
		{
			bestBits = bits;
			bestIndex = previous;
		}
	}
	if ( bestIndex != index )
		*baseline = &to[bestIndex];
	return index - bestIndex;
}

int SV_CreatePacketEntities( sv_delta_t type, client_t* client, packet_entities_t* to, sizebuf_t* message )
{
#if defined( _WIN32 ) && !defined( SWDS_DLL )
	if ( !g_pcustomentitydelta || !g_pentitydelta || !g_pplayerdelta )
	{
		_wassert( L"g_pcustomentitydelta && g_pentitydelta && g_pplayerdelta",
			L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sv_main.c", 5655 );
	}
#endif
	constexpr int ENTITY_NUMBER_SENTINEL = 9999;
	constexpr int ENTITY_NUMBER_BITS = 11;
	constexpr int ENTITY_NUMBER_DELTA_BITS = 6;
	constexpr unsigned int MAX_COMPACT_ENTITY_DELTA = ( 1u << ENTITY_NUMBER_DELTA_BITS ) - 1;
	int numberBase = 0;
	packet_entities_t* from;
	int oldCount;
	if ( type == sv_packet_delta )
	{
		from = &client->frames[client->delta_sequence & SV_UPDATE_MASK].entities;
		oldCount = from->num_entities;
		MSG_WriteByte( message, svc_deltapacketentities );
		MSG_WriteShort( message, to->num_entities );
		MSG_WriteByte( message, client->delta_sequence );
	}
	else
	{
		from = nullptr;
		oldCount = 0;
		MSG_WriteByte( message, svc_packetentities );
		MSG_WriteShort( message, to->num_entities );
	}
	int newIndex = 0;
	int oldIndex = 0;
	MSG_StartBitWriting( message );
	while ( newIndex < to->num_entities || oldIndex < oldCount )
	{
		int newNumber = newIndex < to->num_entities ? to->entities[newIndex].number : ENTITY_NUMBER_SENTINEL;
		int oldNumber = oldIndex < oldCount ? from->entities[oldIndex].number : ENTITY_NUMBER_SENTINEL;
		if ( newNumber == oldNumber )
		{
			entity_state_t* state = &to->entities[newIndex];
			qboolean custom = ( state->entityType & ENTITY_BEAM ) != 0;
			g_svdeltacallback = { &numberBase, newNumber, 0, custom, 0, 0, 0, 0 };
			delta_t* fields = custom
								  ? g_pcustomentitydelta
								  : ( newNumber >= 1 && newNumber <= svs.maxclients ? g_pplayerdelta : g_pentitydelta );
			DELTA_WriteDelta( reinterpret_cast<byte*>( &from->entities[oldIndex] ), reinterpret_cast<byte*>( state ), 0,
				fields, SV_InvokeCallback );
			++oldIndex;
			++newIndex;
		}
		else if ( newNumber > oldNumber )
		{
			unsigned int difference = static_cast<unsigned int>( oldNumber ) - static_cast<unsigned int>( numberBase );
			qboolean fullNumber = difference - 1 >= MAX_COMPACT_ENTITY_DELTA;
			MSG_WriteBits( 1, 1 );
			MSG_WriteBits( fullNumber, 1 );
			MSG_WriteBits(
				fullNumber ? oldNumber : difference, fullNumber ? ENTITY_NUMBER_BITS : ENTITY_NUMBER_DELTA_BITS );
			++oldIndex;
			numberBase = oldNumber;
		}
		else
		{
			edict_t* entity = EDICT_NUM( newNumber );
			entity_state_t* state = &to->entities[newIndex];
			qboolean custom = ( state->entityType & ENTITY_BEAM ) != 0;
			g_svdeltacallback = { &numberBase, newNumber, 0, custom, 0, 0, from == nullptr, 0 };
			entity_state_t* baseline = &sv.baselines[newNumber];
			if ( sv_instancedbaseline.value != 0.0f && sv.instance_baselines->number != 0 && newNumber > sv_lastnum )
			{
				for ( int index = 0; index < sv.instance_baselines->number; ++index )
				{
					if ( static_cast<unsigned int>( sv.instance_baselines->classname[index] ) == entity->v.classname )
					{
						g_svdeltacallback.newbl = 1;
						g_svdeltacallback.newblindex = index;
						baseline = &sv.instance_baselines->baseline[index];
						break;
					}
				}
			}
			else if ( !from )
			{
				int offset = SV_FindBestBaseline( newIndex, &baseline, to->entities, newNumber, custom );
				if ( offset )
					g_svdeltacallback.offset = offset;
			}
			delta_t* fields = custom
								  ? g_pcustomentitydelta
								  : ( newNumber >= 1 && newNumber <= svs.maxclients ? g_pplayerdelta : g_pentitydelta );
			DELTA_WriteDelta(
				reinterpret_cast<byte*>( baseline ), reinterpret_cast<byte*>( state ), 1, fields, SV_InvokeCallback );
			++newIndex;
		}
	}
	MSG_WriteBits( 0, 16 );
	MSG_EndBitWriting( message );
	return message->cursize;
}

void SV_EmitPings( client_t* client, sizebuf_t* message );

void SV_WriteEntitiesToClient( client_t* client, sizebuf_t* message )
{
	constexpr int MAX_VISIBLE_PACKET_ENTITIES = 1024;
	constexpr double PROXY_PING_INTERVAL = 2.0;
	byte* pvs = nullptr;
	byte* pas = nullptr;
	client_frame_t* frame = &client->frames[client->netchan.outgoing_sequence & SV_UPDATE_MASK];
	gEntityInterface.pfnSetupVisibility( const_cast<edict_t*>( client->pViewEntity ), client->edict, &pvs, &pas );
	packet_entities_t* packet = &frame->entities;
	int sendPings;
	if ( client->proxy )
	{
		if ( !( realtime >= client->nextping ) )
		{
			sendPings = 0;
		}
		else
		{
			sendPings = 1;
			client->nextping = realtime + PROXY_PING_INTERVAL;
		}
	}
	else
	{
		sendPings = client->lastcmd.buttons & IN_SCORE;
	}
	qboolean skipEntities = client->lw != 0;
	byte* visibilitySet = pvs;
	if ( frame )
	{
		packet->num_entities = 0;
		if ( !packet->entities )
			packet->entities = static_cast<entity_state_t*>(
				Mem_ZeroMalloc( sizeof( entity_state_t ) * MAX_VISIBLE_PACKET_ENTITIES ) );
	}
	packet->num_entities = 0;
	for ( int entityIndex = 1; entityIndex < sv.num_edicts; ++entityIndex )
	{
		qboolean player = entityIndex >= 1 && entityIndex <= svs.maxclients;
		if ( player )
		{
			client_t* entityClient = &svs.clients[entityIndex - 1];
			if ( ( !entityClient->active && !entityClient->spawned ) || entityClient->proxy )
				continue;
		}
		if ( packet->num_entities >= MAX_VISIBLE_PACKET_ENTITIES )
		{
			Con_DPrintf( "Too many entities in visible packet list.\n" );
			break;
		}
		if ( gEntityInterface.pfnAddToFullPack( &packet->entities[packet->num_entities], entityIndex,
				 &sv.edicts[entityIndex], host_client->edict, skipEntities, player, visibilitySet ) )
			++packet->num_entities;
	}
	SV_CreatePacketEntities(
		client->delta_sequence == -1 ? sv_packet_nodelta : sv_packet_delta, client, packet, message );
	SV_EmitEvents( client, packet, message );
	if ( sendPings )
		SV_EmitPings( client, message );
}

void SV_FullClientUpdate( client_t* client, sizebuf_t* message );

void SV_UpdateToReliableMessages()
{
	host_client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++host_client )
	{
		if ( !host_client->edict )
			continue;
		if ( host_client->sendinfo && realtime >= static_cast<double>( host_client->sendinfo_time ) )
		{
			host_client->sendinfo = 0;
			host_client->sendinfo_time = static_cast<float>( realtime + 1.0 );
			SV_ExtractFromUserinfo( host_client );
			SV_FullClientUpdate( host_client, &sv.reliable_datagram );
		}
		if ( !host_client->fakeclient && ( host_client->active || host_client->connected ) && sv_gpNewUserMsgs )
			SV_SendUserReg( &host_client->netchan.message );
	}

	if ( sv_gpNewUserMsgs )
	{
		if ( sv_gpUserMsgs )
		{
			UserMsg* tail = sv_gpUserMsgs;
			while ( tail->next )
				tail = tail->next;
			tail->next = sv_gpNewUserMsgs;
		}
		else
		{
			sv_gpUserMsgs = sv_gpNewUserMsgs;
		}
		sv_gpNewUserMsgs = nullptr;
	}

	if ( sv.datagram.flags & FSB_OVERFLOWED )
	{
		Con_DPrintf( "sv.datagram overflowed!\n" );
		SZ_Clear( &sv.datagram );
	}
	if ( sv.spectator.flags & FSB_OVERFLOWED )
	{
		Con_DPrintf( "sv.spectator overflowed!\n" );
		SZ_Clear( &sv.spectator );
	}

	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		if ( client->fakeclient || !client->active )
			continue;
		if ( sv.reliable_datagram.cursize + client->netchan.message.cursize < client->netchan.message.maxsize )
			SZ_Write( &client->netchan.message, sv.reliable_datagram.data, sv.reliable_datagram.cursize );
		else
			Netchan_CreateFragments( 1, &client->netchan, &sv.reliable_datagram );
		if ( sv.datagram.cursize + client->datagram.cursize >= client->datagram.maxsize )
			Con_DPrintf( "Warning:  Ignoring unreliable datagram for %s, would overflow\n", client->name );
		else
			SZ_Write( &client->datagram, sv.datagram.data, sv.datagram.cursize );
		if ( client->proxy && sv.spectator.cursize + client->datagram.cursize < client->datagram.maxsize )
			SZ_Write( &client->datagram, sv.spectator.data, sv.spectator.cursize );
	}
	SZ_Clear( &sv.reliable_datagram );
	SZ_Clear( &sv.datagram );
	SZ_Clear( &sv.spectator );
}

void SV_WriteClientdataToMessage( client_t* client, sizebuf_t* message );
void SV_DropClient( client_t* client, qboolean crash, const char* format, ... );

void SV_SendClientMessages()
{
	SV_UpdateToReliableMessages();
	host_client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++host_client )
	{
		if ( ( !host_client->active && !host_client->connected && !host_client->spawned ) || host_client->fakeclient )
			continue;
		if ( host_client->skip_message )
		{
			host_client->skip_message = 0;
			continue;
		}
		if ( host_limitlocal.value == 0.0f && host_client->netchan.remote_address.type == NA_LOOPBACK )
			host_client->send_message = 1;
		if ( host_client->active && host_client->spawned && host_client->fully_connected
			 && host_frametime + realtime >= host_client->next_messagetime )
			host_client->send_message = 1;

		if ( host_client->netchan.message.flags & FSB_OVERFLOWED )
		{
			SZ_Clear( &host_client->netchan.message );
			SZ_Clear( &host_client->datagram );
			SV_BroadcastPrintf( "%s overflowed\n", host_client->name );
			Con_Printf( "WARNING: reliable overflow for %s\n", host_client->name );
			SV_DropClient( host_client, 0, "Reliable channel overflowed" );
			host_client->send_message = 1;
			host_client->netchan.cleartime = 0.0;
		}
		else
		{
			if ( !host_client->send_message )
				continue;
			if ( realtime - static_cast<double>( host_client->netchan.last_received )
				 > static_cast<double>( sv_failuretime.value ) )
				host_client->send_message = 0;
		}
		if ( !host_client->send_message )
			continue;
		if ( !Netchan_CanPacket( &host_client->netchan ) )
		{
			++host_client->chokecount;
			continue;
		}

		host_client->send_message = 0;
		host_client->next_messagetime = host_frametime + realtime + host_client->next_messageinterval;
		client_t* client = host_client;
		if ( client->active && client->spawned && client->fully_connected )
		{
			byte data[SERVER_DATAGRAM_SIZE];
			sizebuf_t message;
			message.buffername = "Client Datagram";
			message.data = data;
			message.maxsize = sizeof( data );
			message.cursize = 0;
			message.flags = FSB_ALLOWOVERFLOW;
			MSG_WriteByte( &message, svc_time );
			MSG_WriteFloat( &message, static_cast<float>( sv.time ) );
			SV_WriteClientdataToMessage( client, &message );
			SV_WriteEntitiesToClient( client, &message );
			if ( client->datagram.flags & FSB_OVERFLOWED )
				Con_Printf( "WARNING: datagram overflowed for %s\n", client->name );
			else
				SZ_Write( &message, client->datagram.data, client->datagram.cursize );
			SZ_Clear( &client->datagram );
			if ( message.flags & FSB_OVERFLOWED )
			{
				Con_Printf( "WARNING: msg overflowed for %s\n", client->name );
				SZ_Clear( &message );
			}
			Netchan_Transmit( &client->netchan, message.cursize, data );
		}
		else
		{
			Netchan_Transmit( &client->netchan, 0, nullptr );
		}
	}
	for ( int index = 1; index < sv.num_edicts; ++index )
		sv.edicts[index].v.effects &= ~( EF_MUZZLEFLASH | EF_NOINTERP );
}

void SV_EmitPings( client_t*, sizebuf_t* message )
{
	constexpr int PING_MESSAGE = 17;
	constexpr int PING_CLIENT_BITS = 5;
	constexpr int PING_VALUE_BITS = 12;
	constexpr int PING_LOSS_BITS = 7;
	constexpr double PING_REFRESH_INTERVAL = 2.0;
	constexpr float LOSS_INTEGER_LIMIT = -static_cast<float>( ( std::numeric_limits<int>::min )() );
	static unsigned short lastping[MAX_CLIENTS];
	static byte lastloss[MAX_CLIENTS];

	MSG_WriteByte( message, PING_MESSAGE );
	MSG_StartBitWriting( message );
	client_t* other = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++other )
	{
		if ( !other->active )
			continue;

		int cacheIndex = static_cast<int>( other - svs.clients );
		if ( realtime >= other->nextping )
		{
			other->nextping = realtime + PING_REFRESH_INTERVAL;
			lastping[cacheIndex] = static_cast<unsigned short>( SV_CalcPing( other ) );
			int loss;
			if ( other->packet_loss >= -LOSS_INTEGER_LIMIT && other->packet_loss < LOSS_INTEGER_LIMIT )
				loss = static_cast<int>( other->packet_loss );
			else
			{
				std::feraiseexcept( FE_INVALID );
				loss = ( std::numeric_limits<int>::min )();
			}
			lastloss[cacheIndex] = static_cast<byte>( loss );
		}

		unsigned int ping = lastping[cacheIndex];
		unsigned int loss = lastloss[cacheIndex];
		MSG_WriteBits( 1, 1 );
		MSG_WriteBits( index, PING_CLIENT_BITS );
		MSG_WriteBits( ping, PING_VALUE_BITS );
		MSG_WriteBits( loss, PING_LOSS_BITS );
	}
	MSG_WriteBits( 0, 1 );
	MSG_EndBitWriting( message );
}

void DLL_SetModKey( modinfo_t* info, char* key, char* value );

void SV_ResetModInfo()
{
	constexpr int DEFAULT_MOD_EDICTS = 1200;
	constexpr int MAX_GAME_LISTING_SIZE = 0x40000;
	Q_memset( &gmodinfo, 0, sizeof( gmodinfo ) );
	gmodinfo.version = 1;
	gmodinfo.svonly = true;
	gmodinfo.num_edicts = DEFAULT_MOD_EDICTS;
	char filename[260];
	_snprintf( filename, sizeof( filename ), "%s", "liblist.gam" );
	FileHandle_t file = FS_Open( filename, "rb" );
	if ( !file )
		return;
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
		if ( Q_strcasecmp( key, "gamedll" ) != 0 )
			DLL_SetModKey( &gmodinfo, key, value );
		cursor = COM_Parse( cursor );
	}
	com_ignorecolons = false;
	std::free( listing );
	FS_Close( file );
}

constexpr int CONNECTION_REJECT_MESSAGE = 57;
constexpr int CONNECTION_REJECT_TEXT_BYTES = 1024;

/*
================
SV_RejectConnection

Rejects connection request and sends back a message
================
*/
void SV_RejectConnection( netadr_t* address, const char* format, ... )
{
	char text[CONNECTION_REJECT_TEXT_BYTES];
	va_list arguments;
	va_start( arguments, format );
	_vsnprintf( text, sizeof( text ), format, arguments );
	va_end( arguments );
	SZ_Clear( &net_message );
	MSG_WriteLong( &net_message, -1 ); // -1 -1 -1 -1 signal
	MSG_WriteByte( &net_message, CONNECTION_REJECT_MESSAGE );
	MSG_WriteString( &net_message, text );
	NET_SendPacket( NS_SERVER, net_message.cursize, net_message.data, *address );
	SZ_Clear( &net_message );
	NET_CheckCleanupFakeIPConnection( NS_SERVER, address );
}

constexpr int CONNECTION_PASSWORD_REJECT = 56;
constexpr int CONNECTION_MINIMUM_USERINFO_LENGTH = 5;
constexpr int CONNECTION_DUPLICATE_NAME_LENGTH = MAX_PLAYERNAME - 4;

int SV_CheckUserInfo( netadr_t* address, char* userinfo, qboolean reconnecting, int reconnectSlot, char* name )
{
	if ( !NET_IsLocalAddress( *address ) && sv_password.string[0] && Q_strcasecmp( sv_password.string, "none" ) )
	{
		const char* password = Info_ValueForKey( userinfo, "password" );
		if ( Q_strcmp( sv_password.string, password ) )
		{
			Con_Printf( "%s:  password failed\n", NET_AdrToString( *address ) );
			SZ_Clear( &net_message );
			MSG_WriteLong( &net_message, -1 );
			MSG_WriteByte( &net_message, CONNECTION_PASSWORD_REJECT );
			MSG_WriteString( &net_message, "BADPASSWORD" );
			NET_SendPacket( NS_SERVER, net_message.cursize, net_message.data, *address );
			SZ_Clear( &net_message );
			return 0;
		}
	}
	if ( std::strlen( userinfo ) < CONNECTION_MINIMUM_USERINFO_LENGTH || std::strstr( userinfo, "\\\\" )
		 || userinfo[std::strlen( userinfo ) - 1] == '\\' )
	{
		SV_RejectConnection( address, "Unknown HLTV client type.\n" );
		return 0;
	}
	Info_RemoveKey( userinfo, "password" ); // remove password so other users don't see it
	char newName[MAX_PLAYERNAME];
	const char* suppliedName = Info_ValueForKey( userinfo, "name" );
	if ( Q_strlen( suppliedName ) )
	{
		Q_strncpy( newName, suppliedName, sizeof( newName ) );
		newName[sizeof( newName ) - 1] = 0;
	}
	else
		Q_strcpy( newName, "unnamed" );
	for ( char* character = newName; *character; ++character )
	{
		if ( *character == '%' || *character == '&' )
			*character = ' ';
	}
	for ( char* character = newName; *character == '#'; ++character )
		*character = ' ';
	TrimSpace( newName, name );
	if ( !Q_UnicodeValidate( name ) )
		Q_UnicodeRepair( name );
	Info_SetValueForKey( userinfo, "name", name, MAX_INFO_STRING );

	const char* candidate = Info_ValueForKey( userinfo, "name" );
	qboolean changed = 0;
	if ( !candidate || !candidate[0] || !Q_strcasecmp( candidate, "console" ) || Q_strstr( candidate, ".." )
		 || Q_strstr( candidate, "\"" ) || Q_strstr( candidate, "\\" ) )
	{
		Info_SetValueForKey( userinfo, "name", "unamed", MAX_INFO_STRING );
		changed = 1;
	}
	else
	{
		int duplicateNumber = 1;
		for ( int slot = 0; slot < MAX_CLIENTS; ++slot )
		{
			client_t& client = svs.clients[slot];
			if ( ( reconnecting && slot == reconnectSlot ) || !client.connected
				 || Q_strcasecmp( client.name, candidate ) )
				continue;
			if ( candidate[0] == '(' )
			{
				if ( candidate[2] == ')' )
					candidate += 3;
				else if ( candidate[3] == ')' )
					candidate += 4;
			}
			char duplicateName[MAX_PLAYERNAME];
			_snprintf( duplicateName, sizeof( duplicateName ), "(%d)%-0.*s", duplicateNumber++,
				CONNECTION_DUPLICATE_NAME_LENGTH, candidate );
			Info_SetValueForKey( userinfo, "name", duplicateName, MAX_INFO_STRING );
			changed = 1;
			candidate = Info_ValueForKey( userinfo, "name" );
			slot = -1;
		}
	}
	if ( changed )
	{
		Q_strncpy( name, Info_ValueForKey( userinfo, "name" ), MAX_PLAYERNAME );
		name[MAX_PLAYERNAME - 1] = 0;
	}
	const char* hltv = Info_ValueForKey( userinfo, "*hltv" );
	if ( !Q_strlen( hltv ) )
		return 1;
	int clientType = Q_atoi( hltv );
	if ( !clientType )
		return 1;
	if ( clientType == 1 )
	{
		int proxies = 0;
		for ( int slot = 0; slot < svs.maxclients; ++slot )
		{
			client_t& client = svs.clients[slot];
			if ( ( client.active || client.spawned || client.connected ) && client.proxy )
				++proxies;
		}
		if ( static_cast<float>( proxies ) >= sv_proxies.value && !reconnecting )
		{
			SV_RejectConnection( address, "Proxy slots are full.\n" );
			return 0;
		}
		return 1;
	}
	if ( clientType == 3 )
		SV_RejectConnection( address, "Please connect to HLTV master proxy.\n" );
	else
		SV_RejectConnection( address, "Unknown HLTV client type.\n" );
	return 0;
}

char* SV_GetIDString( USERID_t* id )
{
	constexpr int AUTH_ID_STRING_BYTES = 64;
	constexpr int STEAM_AUTH_ID_TYPE = 1;
	constexpr int VALVE_AUTH_ID_TYPE = 2;
	constexpr int HLTV_AUTH_ID_TYPE = 3;
	static char idString[AUTH_ID_STRING_BYTES];
	idString[0] = '\0';
	if ( !id )
		return idString;

	const char* format;
	switch ( id->idtype )
	{
	case STEAM_AUTH_ID_TYPE:
		if ( sv_lan.value != 0.0f )
		{
			std::strcpy( idString, "STEAM_ID_LAN" );
			return idString;
		}
		if ( !id->m_SteamID )
		{
			std::strcpy( idString, "STEAM_ID_PENDING" );
			return idString;
		}
		format = "STEAM_%u:%u:%u";
		break;
	case VALVE_AUTH_ID_TYPE:
		if ( sv_lan.value != 0.0f )
		{
			std::strcpy( idString, "VALVE_ID_LAN" );
			return idString;
		}
		if ( !id->m_SteamID )
		{
			std::strcpy( idString, "VALVE_ID_PENDING" );
			return idString;
		}
		format = "VALVE_%u:%u:%u";
		break;
	case HLTV_AUTH_ID_TYPE:
		std::strcpy( idString, "HLTV" );
		return idString;
	default:
		std::strcpy( idString, "UNKNOWN" );
		return idString;
	}

	TSteamGlobalUserID steam2ID = Steam_Steam3IDtoSteam2( id->m_SteamID );
	_snprintf( idString, sizeof( idString ) - 1, format, static_cast<unsigned int>( steam2ID.m_SteamInstanceID ),
		steam2ID.m_SteamLocalUserID.Split.High32bits, steam2ID.m_SteamLocalUserID.Split.Low32bits );
	idString[sizeof( idString ) - 1] = '\0';
	return idString;
}

const char* SV_GetClientIDString( client_t* client )
{
	constexpr int CLIENT_ID_STRING_BYTES = 64;
	constexpr int VALVE_AUTH_ID_TYPE = 2;
	static char idString[CLIENT_ID_STRING_BYTES];
	idString[0] = '\0';
	if ( client )
	{
		if ( client->netchan.remote_address.type == NA_LOOPBACK && client->network_userid.idtype == VALVE_AUTH_ID_TYPE )
			_snprintf( idString, sizeof( idString ) - 1, "VALVE_ID_LOOPBACK" );
		else
			_snprintf( idString, sizeof( idString ) - 1, "%s", SV_GetIDString( &client->network_userid ) );
	}
	return idString;
}

void SV_ClientPrintf( const char* format, ... );

void Host_Kick_f()
{
	if ( Cmd_Argc() <= 1 )
	{
		Con_Printf( "usage:  kick < name > | < # userid >\n" );
		return;
	}

	client_t* savedClient = host_client;
	if ( cmd_source == src_command )
	{
		if ( !sv.active )
		{
			Cmd_ForwardToServer();
			return;
		}
	}
	else if ( savedClient->netchan.remote_address.type != NA_LOOPBACK )
	{
		SV_ClientPrintf( "You can't 'kick' because you are not a server operator\n" );
		return;
	}

	constexpr int KICK_ID_BYTES = 64;
	constexpr int AUTH_ID_PREFIX_BYTES = 6;
	constexpr int AUTH_ID_COLON_TOKENS = 4;
	char targetID[KICK_ID_BYTES];
	int reasonArgument = 2;
	qboolean splitAuthID = 0;
	int clientIndex = 0;
	const char* target = Cmd_Argv( 1 );
	if ( target && target[0] == '#' )
	{
		++target;
		if ( Cmd_Argc() > 2 && !target[0] )
		{
			target = Cmd_Argv( 2 );
			reasonArgument = 3;
		}
		int userID = Q_atoi( target );
		std::strncpy( targetID, target, sizeof( targetID ) - 1 );
		targetID[sizeof( targetID ) - 1] = '\0';
		if ( !Q_strnicmp( targetID, "STEAM_", AUTH_ID_PREFIX_BYTES )
			 || !Q_strnicmp( targetID, "VALVE_", AUTH_ID_PREFIX_BYTES ) )
		{
			const char* finalPart = Cmd_Argv( reasonArgument + 3 );
			const char* middlePart = Cmd_Argv( reasonArgument + 1 );
			_snprintf( targetID, sizeof( targetID ) - 1, "%s:%s:%s", target, middlePart, finalPart );
			targetID[sizeof( targetID ) - 1] = '\0';
			splitAuthID = 1;
			reasonArgument += AUTH_ID_COLON_TOKENS;
		}
		host_client = svs.clients;
		for ( ; clientIndex < svs.maxclients; ++clientIndex, ++host_client )
		{
			if ( !host_client->active && !host_client->connected )
				continue;
			if ( userID && host_client->userid == userID )
				break;
			if ( !Q_strcasecmp( SV_GetClientIDString( host_client ), targetID ) )
				break;
		}
	}
	else
	{
		host_client = svs.clients;
		for ( ; clientIndex < svs.maxclients; ++clientIndex, ++host_client )
		{
			if ( !host_client->active && !host_client->connected )
				continue;
			if ( !Q_strcasecmp( host_client->name, Cmd_Argv( 1 ) ) )
				break;
		}
	}

	if ( clientIndex < svs.maxclients )
	{
		const char* operatorName =
			cmd_source == src_command ? ( cls.state != ca_dedicated ? cl_name.string : "Console" ) : savedClient->name;
		if ( host_client->netchan.remote_address.type == NA_LOOPBACK )
		{
			Con_Printf( "The local player cannot be kicked!\n" );
			return;
		}

		const char* reason = nullptr;
		if ( Cmd_Argc() > reasonArgument )
		{
			const char* arguments = Cmd_Args();
			std::uint32_t reasonOffset = 0;
			for ( int argument = 1; argument < reasonArgument; ++argument )
				reasonOffset += static_cast<std::uint32_t>( std::strlen( Cmd_Argv( argument ) ) ) + 1u;
			if ( splitAuthID )
				reasonOffset -= AUTH_ID_COLON_TOKENS;
			if ( reasonOffset <= std::strlen( arguments ) )
				reason = arguments + reasonOffset;
		}
		if ( reason )
		{
			SV_ClientPrintf( "Kicked by %s: %s\n", operatorName, reason );
			Log_Printf( "Kick: \"%s<%i><%s><>\" was kicked by \"%s\" (message \"%s\")\n", host_client->name,
				host_client->userid, SV_GetClientIDString( host_client ), operatorName, reason );
			SV_DropClient( host_client, 0, va( "Kicked :%s", reason ) );
		}
		else
		{
			SV_ClientPrintf( "Kicked by %s\n", operatorName );
			Log_Printf( "Kick: \"%s<%i><%s><>\" was kicked by \"%s\"\n", host_client->name, host_client->userid,
				SV_GetClientIDString( host_client ), operatorName );
			SV_DropClient( host_client, 0, "Kicked" );
		}
	}
	host_client = savedClient;
}

void SV_CheckUpdateRate( double* messageInterval )
{
#if defined( _WIN32 ) && !defined( SWDS_DLL )
	if ( !messageInterval )
	{
		_wassert( L"rate",
			L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sv_main.c", 2266 );
	}
#endif
	constexpr double DEFAULT_CLIENT_MESSAGE_INTERVAL = 0.05;
	if ( *messageInterval == 0.0 )
	{
		*messageInterval = DEFAULT_CLIENT_MESSAGE_INTERVAL;
		return;
	}

	constexpr float MINIMUM_CONFIGURED_UPDATE_RATE = 0.001f;
	float maximum = sv_maxupdaterate.value;
	if ( maximum <= MINIMUM_CONFIGURED_UPDATE_RATE && maximum != 0.0f )
	{
		Cvar_Set( "sv_maxupdaterate", "30.0" );
		maximum = sv_maxupdaterate.value;
	}
	float minimum = sv_minupdaterate.value;
	if ( minimum <= MINIMUM_CONFIGURED_UPDATE_RATE && minimum != 0.0f )
	{
		Cvar_Set( "sv_minupdaterate", "1.0" );
		maximum = sv_maxupdaterate.value;
		minimum = sv_minupdaterate.value;
	}
	if ( maximum != 0.0f )
	{
		double interval = 1.0 / static_cast<double>( maximum );
		if ( interval > *messageInterval )
		{
			*messageInterval = interval;
			minimum = sv_minupdaterate.value;
		}
	}
	if ( minimum != 0.0f )
	{
		double interval = 1.0 / static_cast<double>( minimum );
		if ( *messageInterval > interval )
			*messageInterval = interval;
	}
}

void SV_ExtractFromUserinfo( client_t* client )
{
	constexpr int NAME_DUPLICATE_CLIENTS = 32;
	constexpr int DUPLICATE_NAME_BYTES = 28;
	constexpr int USERINFO_BYTES = 256;
	constexpr int MINIMUM_CLIENT_RATE = 1000;
	constexpr int MAXIMUM_CLIENT_RATE = 100000;
	constexpr int MINIMUM_CLIENT_UPDATE_RATE = 10;
	char rawName[MAX_PLAYERNAME];
	char newName[MAX_PLAYERNAME];
	char* userinfo = client->userinfo;
	const char* name = Info_ValueForKey( userinfo, "name" );
	Q_strncpy( newName, name, sizeof( newName ) - 1 );
	newName[sizeof( newName ) - 1] = '\0';
	Q_strcpy( rawName, newName );
	for ( char* character = rawName; *character == '#'; ++character )
		*character = ' ';
	TrimSpace( rawName, newName );
	if ( !Q_UnicodeValidate( newName ) )
		Q_UnicodeRepair( newName );
	if ( Q_strlen( newName ) <= 0 || !Q_strcasecmp( newName, "console" ) || Q_strstr( newName, ".." )
		 || Q_strstr( newName, "\"" ) || Q_strstr( newName, "\\" ) )
	{
		Info_SetValueForKey( userinfo, "name", "unnamed", USERINFO_BYTES );
		name = Info_ValueForKey( userinfo, "name" );
	}
	else if ( Q_strcmp( name, newName ) )
	{
		Info_SetValueForKey( userinfo, "name", newName, USERINFO_BYTES );
		name = Info_ValueForKey( userinfo, "name" );
	}

	int duplicateCount = 1;
	for ( int index = 0; index < NAME_DUPLICATE_CLIENTS; )
	{
		client_t* other = &svs.clients[index];
		if ( other->active && other->spawned && other != client && !Q_strcasecmp( other->name, name ) )
		{
			if ( static_cast<unsigned int>( Q_strlen( name ) ) > sizeof( newName ) - 1 )
				const_cast<char*>( name )[DUPLICATE_NAME_BYTES] = '\0';
			if ( name[0] == '(' )
			{
				if ( name[2] == ')' )
					name += 3;
				else if ( name[3] == ')' )
					name += 4;
			}
			_snprintf( newName, sizeof( newName ), "(%d)%-0.*s", duplicateCount++, DUPLICATE_NAME_BYTES, name );
			Info_SetValueForKey( userinfo, "name", newName, USERINFO_BYTES );
			name = Info_ValueForKey( userinfo, "name" );
			index = 0;
		}
		else
			++index;
	}

	gEntityInterface.pfnClientUserInfoChanged( client->edict, userinfo );
	Q_strncpy( client->name, Info_ValueForKey( userinfo, "name" ), sizeof( client->name ) - 1 );
	client->name[sizeof( client->name ) - 1] = '\0';
	ISteamGameServer_BUpdateUserData( client->network_userid.m_SteamID, client->name, 0 );

	const char* value = Info_ValueForKey( userinfo, "rate" );
	if ( Q_strlen( value ) )
	{
		int requestedRate = Q_atoi( value );
		if ( requestedRate < MINIMUM_CLIENT_RATE )
			requestedRate = MINIMUM_CLIENT_RATE;
		else if ( requestedRate > MAXIMUM_CLIENT_RATE )
			requestedRate = MAXIMUM_CLIENT_RATE;
		client->netchan.rate = static_cast<double>( requestedRate );
	}
	value = Info_ValueForKey( userinfo, "topcolor" );
	if ( Q_strlen( value ) )
		client->topcolor = Q_atoi( value );
	else
		Con_DPrintf( "topcolor unchanged for %s\n", client->name );
	value = Info_ValueForKey( userinfo, "bottomcolor" );
	if ( Q_strlen( value ) )
		client->bottomcolor = Q_atoi( value );
	else
		Con_DPrintf( "bottomcolor unchanged for %s\n", client->name );
	value = Info_ValueForKey( userinfo, "cl_updaterate" );
	if ( Q_strlen( value ) )
	{
		int updateRate = Q_atoi( value );
		if ( updateRate < MINIMUM_CLIENT_UPDATE_RATE )
			updateRate = MINIMUM_CLIENT_UPDATE_RATE;
		client->next_messageinterval = static_cast<double>( 1.0f / static_cast<float>( updateRate ) );
	}
	value = Info_ValueForKey( userinfo, "cl_lw" );
	client->lw = Q_strlen( value ) != 0 && Q_atoi( value ) != 0;
	value = Info_ValueForKey( userinfo, "cl_lc" );
	client->lc = Q_strlen( value ) != 0 && Q_atoi( value ) != 0;
	value = Info_ValueForKey( userinfo, "*hltv" );
	client->proxy = Q_strlen( value ) != 0 && Q_atoi( value ) == 1;
	SV_CheckUpdateRate( &client->next_messageinterval );
	if ( sv_maxrate.value > 0.0f && client->netchan.rate > static_cast<double>( sv_maxrate.value ) )
		client->netchan.rate =
			static_cast<double>( std::fmin( sv_maxrate.value, static_cast<float>( MAXIMUM_CLIENT_RATE ) ) );
	if ( sv_minrate.value > 0.0f && static_cast<double>( sv_minrate.value ) > client->netchan.rate )
		client->netchan.rate =
			static_cast<double>( std::fmax( sv_minrate.value, static_cast<float>( MINIMUM_CLIENT_RATE ) ) );
}

uint64 Steam_GSGetSteamID();
qboolean Steam_GSBSecure();
qboolean Steam_GSSteamIDIsValidOrLAN( uint64 steamID );

constexpr int MAX_CHALLENGE_HIGH_WORD = 36863;
constexpr int MAX_CHALLENGE_LOW_WORD = 65535;
constexpr int CHALLENGE_WORD_SHIFT = 16;
constexpr int CHALLENGE_REPLY_MARKER = 'A';
constexpr int CHALLENGE_AUTH_NON_STEAM = 2;
constexpr int CHALLENGE_AUTH_STEAM = 3;

void SVC_GetChallenge()
{
	constexpr int CHALLENGE_REPLY_BYTES = 1024;
	qboolean steam = false;
	if ( Cmd_Argc() == 2 && Q_strcasecmp( Cmd_Argv( 1 ), "steam" ) == 0 )
	{
		steam = true;
		uint64 steamID = Steam_GSGetSteamID();
		if ( NET_IsConfigured() && !Steam_GSSteamIDIsValidOrLAN( steamID ) )
			return;
	}
	int oldest = 0;
	int oldestTime = INT_MAX;
	int index;
	for ( index = 0; index < MAX_CHALLENGES; ++index )
	{
		if ( NET_CompareBaseAdr( net_from, g_rg_sv_challenges[index].adr ) )
			break;
		if ( g_rg_sv_challenges[index].time < oldestTime )
		{
			oldestTime = g_rg_sv_challenges[index].time;
			oldest = index;
		}
	}
	if ( index == MAX_CHALLENGES )
	{
		unsigned int highWord = static_cast<unsigned int>( RandomLong( 0, MAX_CHALLENGE_HIGH_WORD ) );
		unsigned int lowWord = static_cast<unsigned int>( RandomLong( 0, MAX_CHALLENGE_LOW_WORD ) );
		g_rg_sv_challenges[oldest].challenge = static_cast<int>( ( highWord << CHALLENGE_WORD_SHIFT ) | lowWord );
		g_rg_sv_challenges[oldest].adr = net_from;
		g_rg_sv_challenges[oldest].time = static_cast<int>( realtime );
		index = oldest;
	}
	char data[CHALLENGE_REPLY_BYTES];
	if ( steam )
	{
		qboolean secure = Steam_GSBSecure();
		uint64 steamID = Steam_GSGetSteamID();
#if defined( _WIN32 ) && !defined( SWDS_DLL )
		constexpr const char* replyFormat = "%c%c%c%c%c00000000 %u %i %I64i %d\n";
#else
		constexpr const char* replyFormat = "%c%c%c%c%c00000000 %u %i %llim %d\n";
#endif
		_snprintf( data, sizeof( data ), replyFormat, 255, 255, 255, 255,
			CHALLENGE_REPLY_MARKER, static_cast<unsigned int>( g_rg_sv_challenges[index].challenge ),
			CHALLENGE_AUTH_STEAM, static_cast<long long>( steamID ), secure );
	}
	else
	{
		Con_DPrintf( "Server requiring authentication\n" );
		_snprintf( data, sizeof( data ), "%c%c%c%c%c00000000 %u %i\n", 255, 255, 255, 255, CHALLENGE_REPLY_MARKER,
			static_cast<unsigned int>( g_rg_sv_challenges[index].challenge ), CHALLENGE_AUTH_NON_STEAM );
	}
	// send it back
	NET_SendPacket( NS_SERVER, Q_strlen( data ) + 1, data, net_from );
}

qboolean SV_FilterPacket()
{
	constexpr unsigned int FILTER_SKIP_EXPIRATION = ( std::numeric_limits<unsigned int>::max )();
	int count = numipfilters;
	if ( count <= 0 )
		return sv_filterban.value == 0.0f;
	double currentTime = realtime;
	unsigned int address;
	std::memcpy( &address, net_from.ip, sizeof( address ) );
	for ( int index = count - 1; index >= 0; --index )
	{
		ipfilter_t* filter = &ipfilters[index];
		unsigned int compare = filter->compare;
		if ( compare != FILTER_SKIP_EXPIRATION && filter->banEndTime != 0.0f
			 && currentTime >= static_cast<double>( filter->banEndTime ) )
		{
			if ( index + 1 < count )
			{
				std::memmove( filter, filter + 1, sizeof( ipfilter_t ) * ( count - index - 1 ) );
			}
			numipfilters = --count;
			continue;
		}
		if ( ( address & filter->mask ) == compare )
			return static_cast<int>( sv_filterban.value );
	}
	return sv_filterban.value == 0.0f;
}

void SV_AddIP_f()
{
	constexpr float MINIMUM_TEMPORARY_BAN_MINUTES = 0.01f;
	constexpr double SECONDS_PER_MINUTE = 60.0;
	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "Usage: addip <minutes> <ipaddress>\nUse 0 minutes for permanent\n" );
		return;
	}
	float banTime = Q_atof( Cmd_Argv( 1 ) );
	ipfilter_t filter;
	if ( !StringToFilter( const_cast<char*>( Cmd_Argv( 2 ) ), &filter ) )
	{
		Con_Printf( "Invalid IP address!\nUsage: addip <minutes> <ipaddress>\nUse 0 minutes for permanent\n" );
		return;
	}
	int index = 0;
	for ( ; index < numipfilters; ++index )
	{
		if ( ipfilters[index].mask == filter.mask && ipfilters[index].compare == filter.compare )
		{
			ipfilters[index].banTime = banTime;
			ipfilters[index].banEndTime =
				banTime != 0.0f ? static_cast<float>( static_cast<double>( banTime ) * SECONDS_PER_MINUTE + realtime )
								: 0.0f;
			return;
		}
	}
	if ( numipfilters >= MAX_IPFILTERS )
	{
		Con_Printf( "IP filter list is full\n" );
		return;
	}
	++numipfilters;
	if ( banTime < MINIMUM_TEMPORARY_BAN_MINUTES )
		banTime = 0.0f;
	ipfilters[index].banTime = banTime;
	float banEndTime =
		banTime != 0.0f ? static_cast<float>( static_cast<double>( banTime ) * SECONDS_PER_MINUTE + realtime ) : 0.0f;
	ipfilters[index].mask = filter.mask;
	ipfilters[index].compare = filter.compare;
	ipfilters[index].banEndTime = banEndTime;
	client_t* client = svs.clients;
	int maxclients = svs.maxclients;
	host_client = client;
	for ( int clientIndex = 0; clientIndex < maxclients; ++clientIndex )
	{
		if ( client->connected && client->active && client->spawned && !client->fakeclient )
		{
			net_from = client->netchan.remote_address;
			if ( SV_FilterPacket() )
			{
				SV_ClientPrintf( "The server operator has added you to banned list\n" );
				SV_DropClient( host_client, 0, "Added to banned list" );
				client = host_client;
				maxclients = svs.maxclients;
			}
		}
		++client;
		host_client = client;
	}
}

ipfilter_t ipfilters[MAX_IPFILTERS];
int numipfilters;
userfilter_t userfilters[MAX_USERFILTERS];
int numuserfilters;

void SV_ListIP_f()
{
	if ( numipfilters <= 0 )
	{
		Con_Printf( "IP filter list: empty\n" );
		return;
	}
	Con_Printf( "IP filter list:\n" );
	for ( int index = 0; index < numipfilters; ++index )
	{
		unsigned int address = ipfilters[index].compare;
		if ( ipfilters[index].banTime == 0.0f )
		{
			Con_Printf( "%3i.%3i.%3i.%3i : permanent\n", static_cast<unsigned char>( address ),
				static_cast<unsigned char>( address >> 8 ), static_cast<unsigned char>( address >> 16 ),
				static_cast<unsigned char>( address >> 24 ) );
		}
		else
		{
			Con_Printf( "%3i.%3i.%3i.%3i : %.3f min\n", static_cast<unsigned char>( address ),
				static_cast<unsigned char>( address >> 8 ), static_cast<unsigned char>( address >> 16 ),
				static_cast<unsigned char>( address >> 24 ), static_cast<double>( ipfilters[index].banTime ) );
		}
	}
}

void SV_WriteIP_f()
{
	constexpr int FILTER_PATH_BYTES = 260;
	char name[FILTER_PATH_BYTES];
	_snprintf( name, sizeof( name ), "listip.cfg" );
	Con_Printf( "Writing %s.\n", name );
	FileHandle_t file = FS_Open( name, "wb" );
	if ( !file )
	{
		Con_Printf( "Couldn't open %s\n", name );
		return;
	}
	int count = numipfilters;
	for ( int index = 0; index < count; ++index )
	{
		if ( ipfilters[index].banTime == 0.0f )
		{
			unsigned int address = ipfilters[index].compare;
			FS_FPrintf( file, "addip 0.0 %i.%i.%i.%i\n", static_cast<unsigned char>( address ),
				static_cast<unsigned char>( address >> 8 ), static_cast<unsigned char>( address >> 16 ),
				static_cast<unsigned char>( address >> 24 ) );
			count = numipfilters;
		}
	}
	FS_Close( file );
}

void SV_RemoveIP_f()
{
	ipfilter_t filter;
	if ( !StringToFilter( const_cast<char*>( Cmd_Argv( 1 ) ), &filter ) )
		return;
	int count = numipfilters;
	for ( int index = 0; index < numipfilters; ++index )
	{
		if ( ipfilters[index].mask != filter.mask || ipfilters[index].compare != filter.compare )
			continue;
		if ( index + 1 < numipfilters )
		{
			std::memmove(
				&ipfilters[index], &ipfilters[index + 1], sizeof( ipfilter_t ) * ( numipfilters - index - 1 ) );
		}
		numipfilters = count - 1;
		ipfilters[numipfilters].banTime = 0.0f;
		ipfilters[numipfilters].banEndTime = 0.0f;
		ipfilters[numipfilters].compare = 0;
		ipfilters[numipfilters].mask = 0;
		Con_Printf( "IP filter removed.\n" );
		return;
	}
	Con_Printf( "removeip: couldn't find %s.\n", Cmd_Argv( 1 ) );
}

void SVC_ServiceChallenge()
{
	constexpr int SERVICE_CHALLENGE_REPLY_BYTES = 128;
	if ( Cmd_Argc() != 2 )
		return;
	const char* service = Cmd_Argv( 1 );
	if ( !service || !*service || Q_strcasecmp( service, "rcon" ) != 0 )
		return;
	int oldest = 0;
	int oldestTime = INT_MAX;
	int index;
	for ( index = 0; index < MAX_CHALLENGES; ++index )
	{
		if ( NET_CompareBaseAdr( net_from, g_rg_sv_challenges[index].adr ) )
			break;
		if ( g_rg_sv_challenges[index].time < oldestTime )
		{
			oldestTime = g_rg_sv_challenges[index].time;
			oldest = index;
		}
	}
	if ( index == MAX_CHALLENGES )
	{
		index = oldest;
		unsigned int highWord = static_cast<unsigned int>( RandomLong( 0, MAX_CHALLENGE_HIGH_WORD ) );
		unsigned int lowWord = static_cast<unsigned int>( RandomLong( 0, MAX_CHALLENGE_LOW_WORD ) );
		g_rg_sv_challenges[oldest].challenge = static_cast<int>( ( highWord << CHALLENGE_WORD_SHIFT ) | lowWord );
		g_rg_sv_challenges[oldest].adr = net_from;
		g_rg_sv_challenges[oldest].time = static_cast<int>( realtime );
	}
	char data[SERVICE_CHALLENGE_REPLY_BYTES];
	_snprintf( data, sizeof( data ), "%c%c%c%cchallenge %s %u\n", 255, 255, 255, 255, service,
		static_cast<unsigned int>( g_rg_sv_challenges[index].challenge ) );
	NET_SendPacket( NS_SERVER, Q_strlen( data ) + 1, data, net_from );
}

void SV_BroadcastCommand( const char* format, ... )
{
	constexpr int BROADCAST_FORMAT_BYTES = 1024;
	constexpr int BROADCAST_MESSAGE_BYTES = 128;
	char text[BROADCAST_FORMAT_BYTES];
	byte data[BROADCAST_MESSAGE_BYTES];
	sizebuf_t message;
	message.buffername = "Broadcast Command";
	message.data = data;
	message.cursize = 0;
	message.maxsize = sizeof( data );
	message.flags = FSB_ALLOWOVERFLOW;
	va_list arguments;
	va_start( arguments, format );
	if ( sv.active )
	{
		_vsnprintf( text, sizeof( text ), format, arguments );
		MSG_WriteByte( &message, svc_stufftext );
		MSG_WriteString( &message, text );
		if ( message.flags & FSB_OVERFLOWED )
			Sys_Error( "SV_BroadcastCommand:  Overflowed on %s, %i is max size\n", text, message.maxsize );
		client_t* client = svs.clients;
		for ( int index = 0; index < svs.maxclients; ++index, ++client )
		{
			if ( ( client->active || client->connected || client->spawned ) && !client->fakeclient )
				SZ_Write( &client->netchan.message, message.data, message.cursize );
		}
	}
	va_end( arguments );
}

int hashstrings_collisions;

static unsigned int SV_HashSoundName( const char* sample )
{
	unsigned int hash = 0;
	while ( *sample )
	{
		int character = static_cast<signed char>( *sample++ );
		hash = std::tolower( character ) + 2 * hash;
	}
	return hash % SOUND_HASH_TABLE_SIZE;
}

int SV_LookupSoundIndex( const char* sample )
{
	unsigned int slot = SV_HashSoundName( sample );
	unsigned int firstSlot = slot;
	if ( !sv.sound_precache_hashedlookup_built )
	{
		if ( sv.state == ss_loading )
		{
			for ( int index = 1; index < MAX_SOUNDS && sv.sound_precache[index]; ++index )
			{
				if ( !Q_strcasecmp( sample, sv.sound_precache[index] ) )
					return index;
			}
			return 0;
		}

		Q_memset( sv.sound_precache_hashedlookup, 0, sizeof( sv.sound_precache_hashedlookup ) );
		for ( int index = 0; index < MAX_SOUNDS && sv.sound_precache[index]; ++index )
		{
			unsigned int insertSlot = SV_HashSoundName( sv.sound_precache[index] );
			unsigned int firstInsertSlot = insertSlot;
			while ( sv.sound_precache_hashedlookup[insertSlot] != 0 )
			{
				hashstrings_collisions = static_cast<int>( static_cast<unsigned int>( hashstrings_collisions ) + 1 );
				++insertSlot;
				if ( insertSlot == SOUND_HASH_TABLE_SIZE )
					insertSlot = 0;
				if ( insertSlot == firstInsertSlot )
					Sys_Error( "SV_AddSampleToHashedLookupTable: NO FREE SLOTS IN SOUND LOOKUP TABLE" );
			}
			sv.sound_precache_hashedlookup[insertSlot] = static_cast<short>( index );
		}
		sv.sound_precache_hashedlookup_built = 1;
	}

	if ( !sv.sound_precache_hashedlookup[slot] )
		return 0;
	while ( Q_strcasecmp( sample, sv.sound_precache[sv.sound_precache_hashedlookup[slot]] ) )
	{
		++slot;
		if ( slot == SOUND_HASH_TABLE_SIZE )
			slot = 0;
		if ( slot == firstSlot || !sv.sound_precache_hashedlookup[slot] )
			return 0;
	}
	return sv.sound_precache_hashedlookup[slot];
}

constexpr int SOUND_MESSAGE = 6;
constexpr int SOUND_VOLUME_MAX = 255;
constexpr int SOUND_PITCH_MAX = 255;
constexpr int SOUND_CHANNEL_MAX = 7;
constexpr int SOUND_FLAG_VOLUME = 1 << 0;
constexpr int SOUND_FLAG_ATTENUATION = 1 << 1;
constexpr int SOUND_FLAG_LARGE_INDEX = 1 << 2;
constexpr int SOUND_FLAG_PITCH = 1 << 3;
constexpr int SOUND_FLAGS_BITS = 9;
constexpr int SOUND_BYTE_BITS = 8;
constexpr int SOUND_CHANNEL_BITS = 3;
constexpr int SOUND_ENTITY_BITS = 11;
constexpr int SOUND_LARGE_INDEX_BITS = 16;
constexpr int SOUND_MULTICAST_ALL = 1;
constexpr int SOUND_MULTICAST_SKIP_SOURCE = 1 << 7;

qboolean SV_BuildSoundMsg( edict_t* entity, int channel, const char* sample, int volume, float attenuation, int flags,
	int pitch, const vec_t* origin, sizebuf_t* buffer )
{
	if ( static_cast<unsigned int>( volume ) > SOUND_VOLUME_MAX )
	{
		Con_Printf( "SV_StartSound: volume = %i", volume );
		volume = volume < 0 ? 0 : SOUND_VOLUME_MAX;
	}
	if ( attenuation < 0.0 || attenuation > 4.0 )
	{
		Con_Printf( "SV_StartSound: attenuation = %f", attenuation );
		attenuation = attenuation >= 0.0 ? 4.0f : 0.0f;
	}
	if ( static_cast<unsigned int>( channel ) > SOUND_CHANNEL_MAX )
	{
		Con_Printf( "SV_StartSound: channel = %i", channel );
		channel = channel < 0 ? 0 : SOUND_CHANNEL_MAX;
	}
	if ( static_cast<unsigned int>( pitch ) > SOUND_PITCH_MAX )
	{
		Con_Printf( "SV_StartSound: pitch = %i", pitch );
		pitch = pitch < 0 ? 0 : SOUND_CHANNEL_MAX;
	}

	int soundIndex;
	if ( sample && *sample == CHAR_SENTENCE )
	{
		flags |= SND_SENTENCE;
		soundIndex = Q_atoi( sample + 1 );
		if ( soundIndex >= MAX_SENTENCES )
		{
			Con_Printf( "invalid sentence number: %s", sample + 1 );
			return 0;
		}
	}
	else if ( sample && *sample == '#' )
	{
		flags |= SND_SENTENCE;
		soundIndex = static_cast<int>( static_cast<unsigned int>( Q_atoi( sample + 1 ) ) + MAX_SENTENCES );
	}
	else
	{
		soundIndex = SV_LookupSoundIndex( sample );
		if ( soundIndex == 0 || !sv.sound_precache[soundIndex] )
		{
			Con_Printf( "SV_StartSound: %s not precached (%d)\n", sample, soundIndex );
			return 0;
		}
	}

	unsigned int entityIndex = static_cast<unsigned int>( NUM_FOR_EDICT( entity ) );
	if ( volume != SOUND_VOLUME_MAX )
		flags |= SOUND_FLAG_VOLUME;
	if ( attenuation != 1.0 )
		flags |= SOUND_FLAG_ATTENUATION;
	if ( pitch != PITCH_NORM )
		flags |= SOUND_FLAG_PITCH;
	if ( soundIndex > SOUND_VOLUME_MAX )
		flags |= SOUND_FLAG_LARGE_INDEX;

	MSG_WriteByte( buffer, SOUND_MESSAGE );
	MSG_StartBitWriting( buffer );
	MSG_WriteBits( static_cast<unsigned int>( flags ), SOUND_FLAGS_BITS );
	if ( flags & SOUND_FLAG_VOLUME )
		MSG_WriteBits( volume, SOUND_BYTE_BITS );
	if ( flags & SOUND_FLAG_ATTENUATION )
		MSG_WriteBits( static_cast<int>( attenuation * 64.0 ), SOUND_BYTE_BITS );
	MSG_WriteBits( channel, SOUND_CHANNEL_BITS );
	MSG_WriteBits( entityIndex, SOUND_ENTITY_BITS );
	MSG_WriteBits( soundIndex, soundIndex > SOUND_VOLUME_MAX ? SOUND_LARGE_INDEX_BITS : SOUND_BYTE_BITS );
	MSG_WriteBitVec3Coord( origin );
	if ( flags & SOUND_FLAG_PITCH )
		MSG_WriteBits( pitch, SOUND_BYTE_BITS );
	MSG_EndBitWriting( buffer );
	return 1;
}

void SV_StartSound( int recipients, edict_t* entity, int channel, const char* sample, int volume, float attenuation,
	int flags, int pitch )
{
	vec3_t origin;
	for ( int axis = 0; axis < 3; ++axis )
		origin[axis] =
			static_cast<float>( ( entity->v.maxs[axis] + entity->v.mins[axis] ) * 0.5 + entity->v.origin[axis] );

	if ( !SV_BuildSoundMsg( entity, channel, sample, volume, attenuation, flags, pitch, origin, &sv.multicast ) )
		return;

	int destination;
	qboolean reliable = 0;
	if ( channel == CHAN_STATIC || ( flags & SND_STOP ) )
	{
		destination = SOUND_MULTICAST_ALL;
		reliable = !Host_IsSinglePlayerGame();
	}
	else
	{
		destination = MSG_FL_PAS;
	}
	if ( recipients == 1 )
		destination |= SOUND_MULTICAST_SKIP_SOURCE;
	SV_Multicast( entity, origin, destination, reliable );
}

/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex( const char* name )
{
	if ( !name || !name[0] )
		return 0;

	int index;
	for ( index = 0; index < MAX_MODELS; ++index )
	{
		const char* filename = sv.model_precache[index];
		if ( !filename )
			break;
		if ( Q_strcasecmp( filename, name ) == 0 )
			return index;
	}
	if ( index == MAX_MODELS || !sv.model_precache[index] )
		Sys_Error( "SV_ModelIndex: model %s not precached", name );
	return index;
}

constexpr int PARTICLE_MESSAGE = 18;
constexpr int PARTICLE_DATAGRAM_RESERVE_BYTES = 16;
constexpr float PARTICLE_DIRECTION_SCALE = 16.0f;
constexpr int PARTICLE_DIRECTION_MIN = -128;
constexpr int PARTICLE_DIRECTION_MAX = 127;

void SV_StartParticle( const float* origin, const float* direction, int color, int count )
{
	if ( sv.datagram.cursize > SERVER_DATAGRAM_SIZE - PARTICLE_DATAGRAM_RESERVE_BYTES )
		return;

	MSG_WriteByte( &sv.datagram, PARTICLE_MESSAGE );
	MSG_WriteCoord( &sv.datagram, origin[0] );
	MSG_WriteCoord( &sv.datagram, origin[1] );
	MSG_WriteCoord( &sv.datagram, origin[2] );
	for ( int axis = 0; axis < 3; ++axis )
	{
		int velocity = static_cast<int>( direction[axis] * PARTICLE_DIRECTION_SCALE );
		if ( velocity > PARTICLE_DIRECTION_MAX )
			velocity = PARTICLE_DIRECTION_MAX;
		else if ( velocity < PARTICLE_DIRECTION_MIN )
			velocity = PARTICLE_DIRECTION_MIN;
		MSG_WriteByte( &sv.datagram, velocity );
	}
	MSG_WriteByte( &sv.datagram, count );
	MSG_WriteByte( &sv.datagram, color );
}

void SV_SkipUpdates()
{
	client_t* client = svs.clients;
	int clientCount = svs.maxclients;
	for ( int index = 0; index < clientCount; ++index, ++client )
	{
		if ( ( client->active || client->connected || client->spawned ) && !host_client->fakeclient )
		{
			client->skip_message = 1;
			clientCount = svs.maxclients;
		}
	}
}

void SVC_GetChallenge();
void SVC_ServiceChallenge();
void SV_ConnectClient();

constexpr int CONNECTIONLESS_REPLY_BYTES = 2048;
constexpr int CONNECTIONLESS_PAYLOAD_BYTES = CONNECTIONLESS_REPLY_BYTES - sizeof( int );
constexpr char CONNECTIONLESS_PING = 'i';
constexpr char CONNECTIONLESS_ACK = 'j';

void SV_ConnectionlessPacket()
{
	MSG_BeginReading();
	MSG_ReadLong(); // skip the -1 marker
	char* text = MSG_ReadStringLine();
	Cmd_TokenizeString( text );
	const char* command = Cmd_Argv( 0 );
	if ( !Q_strcmp( command, "ping" )
		 || ( command[0] == CONNECTIONLESS_PING && ( !command[1] || command[1] == '\n' ) ) )
	{
		struct
		{
			int marker;
			char message[2];
		} reply = { -1, { CONNECTIONLESS_ACK, '\0' } };
		NET_SendPacket( NS_SERVER, sizeof( reply.marker ) + sizeof( reply.message ), &reply, net_from );
		return;
	}
	if ( command[0] == CONNECTIONLESS_ACK && ( !command[1] || command[1] == '\n' ) )
	{
		Con_Printf( "A2A_ACK from %s\n", NET_AdrToString( net_from ) );
		return;
	}
	switch ( command[0] )
	{
	case 'W':
	case 'T':
	case 'U':
	case 'V':
	case 'R':
	case 'O':
	case 's':
		return;
	}

	if ( !Q_strcasecmp( command, "log" ) )
	{
		if ( sv_logrelay.value != 0.0 && text && Q_strlen( text ) > 4 )
		{
			const char* message = text + Q_strlen( "log " );
			if ( *message )
				Con_Printf( "%s\n", message ); // add back in the \n that read string line stripped off.
		}
		return;
	}
	if ( !Q_strcmp( command, "getchallenge" ) )
	{
		SVC_GetChallenge();
		return;
	}
	if ( !Q_strcasecmp( command, "challenge" ) )
	{
		SVC_ServiceChallenge();
		return;
	}
	if ( !Q_strcmp( command, "connect" ) )
	{
		SV_ConnectClient();
		return;
	}
	if ( !Q_strcmp( command, "pstat" ) )
	{
		if ( g_modfuncs.m_pfnPlayerStatus )
			g_modfuncs.m_pfnPlayerStatus( net_message.data, net_message.cursize );
		return;
	}
	if ( !Q_strcmp( command, "rcon" ) )
	{
		SV_Rcon( &net_from );
		return;
	}

	if ( sv.active && svs.maxclients > 1 )
	{
		struct
		{
			int marker;
			char payload[CONNECTIONLESS_PAYLOAD_BYTES];
		} reply;
		Q_memset( &reply, 0, sizeof( reply ) );
		int responseSize = CONNECTIONLESS_PAYLOAD_BYTES;
		gEntityInterface.pfnConnectionlessPacket( &net_from, text, reply.payload, &responseSize );
		if ( responseSize != 0 && responseSize <= CONNECTIONLESS_PAYLOAD_BYTES )
		{
			reply.marker = -1;
			int packetSize = static_cast<int>( static_cast<unsigned int>( responseSize ) + sizeof( reply.marker ) );
			NET_SendPacket( NS_SERVER, packetSize, &reply, net_from );
		}
	}
}

extern int g_userid;
qboolean Master_IsLanGame();
qboolean Steam_NotifyClientConnect( client_t* client, const void* authentication, unsigned int bytes );
qboolean Steam_NotifyBotConnect( client_t* client );
void Steam_NotifyClientDisconnect( client_t* client );
void SV_ClearResourceLists( client_t* client );
int SV_GetFragmentSize( void* client );
qboolean Steam_GSBSecure();

constexpr int CONNECT_INFO_BYTES = 1024;
constexpr int CONNECT_CERTIFICATE_BYTES = 512;
constexpr int CONNECT_HASHED_KEY_BYTES = 32;
constexpr int CONNECT_KEY_BUFFER_BYTES = 64;
constexpr int CONNECT_USERINFO_LIMIT = 256;
constexpr int CONNECT_AUTH_CERTIFICATE = 2;
constexpr int CONNECT_AUTH_STEAM = 3;
constexpr int CONNECT_UNFINISHED_IP_LIMIT = 5;
constexpr int CONNECT_REPLY_MARKER = 'B';
constexpr double CONNECT_MESSAGE_INTERVAL = 0.05;
constexpr unsigned int CONNECT_LOOPBACK_IP = 0x7F000001;

void SV_ConnectClient()
{
	client_t* client = nullptr;
	netadr_t address = net_from;
	qboolean reconnecting = 0;
	int slot = 0;
	Q_atoi( "27005" );
	if ( Cmd_Argc() < 5 )
	{
		SV_RejectConnection( &address, "Insufficient connection info\n" );
		return;
	}

	int protocol = Q_atoi( Cmd_Argv( 1 ) );
	if ( protocol != PROTOCOL_VERSION )
	{
		if ( protocol <= PROTOCOL_VERSION )
		{
			SV_RejectConnection( &address,
				"This server is using a newer protocol ( %i ) than your client ( %i ).  You should check for updates to your client.\n",
				PROTOCOL_VERSION, protocol );
		}
		else
		{
			const char* contact =
				Q_strlen( sv_contact.string ) > 0 ? sv_contact.string : "(no email address specified)";
			SV_RejectConnection( &address,
				"This server is using an older protocol ( %i ) than your client ( %i ).  If you believe this server is outdated, "
				"you can contact the server administrator at %s.\n",
				PROTOCOL_VERSION, protocol, contact );
		}
		return;
	}

	int challenge = Q_atoi( Cmd_Argv( 2 ) );
	if ( !SV_CheckChallenge( &address, challenge ) )
		return;
	char certificate[CONNECT_CERTIFICATE_BYTES];
	Q_memset( certificate, 0, sizeof( certificate ) );
	const char* suppliedProtocolInfo = Cmd_Argv( 3 );
	if ( !Info_IsValid( suppliedProtocolInfo ) )
	{
		SV_RejectConnection( &address, "Invalid protinfo in connect command\n" );
		return;
	}
	char protocolInfo[CONNECT_INFO_BYTES];
	Q_strncpy( protocolInfo, suppliedProtocolInfo, sizeof( protocolInfo ) - 1 );
	protocolInfo[sizeof( protocolInfo ) - 1] = 0;
	int authentication = Q_atoi( Info_ValueForKey( protocolInfo, "prot" ) );
	if ( static_cast<unsigned int>( authentication ) - 1 > 3 )
	{
		SV_RejectConnection( &address, "Invalid connection.\n" );
		return;
	}
	const char* rawCertificate = Info_ValueForKey( protocolInfo, "raw" );
	if ( Q_strlen( rawCertificate ) <= 0
		 || ( authentication == CONNECT_AUTH_CERTIFICATE && Q_strlen( rawCertificate ) != CONNECT_HASHED_KEY_BYTES ) )
	{
		SV_RejectConnection( &address, "Invalid authentication certificate length.\n" );
		return;
	}
	Q_strncpy( certificate, rawCertificate, sizeof( certificate ) );
	certificate[sizeof( certificate ) - 1] = 0;
	const char* key = rawCertificate;
	if ( authentication != CONNECT_AUTH_CERTIFICATE )
	{
		key = Info_ValueForKey( protocolInfo, "cdkey" );
		if ( Q_strlen( key ) != CONNECT_HASHED_KEY_BYTES )
		{
			SV_RejectConnection( &address, "Invalid hashed CD key.\n" );
			return;
		}
	}
	char hashedKey[CONNECT_KEY_BUFFER_BYTES];
	Q_strncpy( hashedKey, key, sizeof( hashedKey ) );
	unsigned short defaultPort = static_cast<unsigned short>( Q_atoi( "27005" ) );
	if ( ( sv_lan.value != 0.0 || authentication != CONNECT_AUTH_STEAM ) && authentication != CONNECT_AUTH_CERTIFICATE
		 && ( sv_lan.value == 0.0
			  || ( !NET_CompareClassBAdr( address, net_local_adr ) && !NET_IsReservedAdr( address ) ) ) )
	{
		SV_RejectConnection( &address, "LAN servers are restricted to local clients (class C).\n" );
		return;
	}
	const char* suppliedUserInfo = Cmd_Argv( 4 );
	if ( Q_strlen( suppliedUserInfo ) > CONNECT_USERINFO_LIMIT || !Info_IsValid( suppliedUserInfo ) )
	{
		SV_RejectConnection( &address, "Invalid userinfo in connect command\n" );
		return;
	}
	char userInfo[CONNECT_INFO_BYTES];
	Q_strncpy( userInfo, suppliedUserInfo, sizeof( userInfo ) - 1 );
	userInfo[sizeof( userInfo ) - 1] = 0;
	if ( Master_IsLanGame() || authentication == CONNECT_AUTH_CERTIFICATE || authentication == CONNECT_AUTH_STEAM )
	{
		for ( slot = 0; slot < svs.maxclients; ++slot )
		{
			client = &svs.clients[slot];
			if ( NET_CompareAdr( address, client->netchan.remote_address ) )
			{
				reconnecting = 1;
				break;
			}
		}
	}
	char name[MAX_PLAYERNAME];
	if ( !SV_CheckUserInfo( &address, userInfo, reconnecting, slot, name ) )
		return;
	if ( authentication == CONNECT_AUTH_CERTIFICATE )
	{
		qboolean validCertificate = Q_strlen( certificate ) == CONNECT_HASHED_KEY_BYTES;
		if ( validCertificate && address.type != NA_LOOPBACK )
		{
			const char* hltv = Info_ValueForKey( userInfo, "*hltv" );
			validCertificate = Q_strlen( hltv ) != 0 && Q_atoi( hltv ) == 1;
		}
		if ( !validCertificate )
		{
			SV_RejectConnection( &address, "Invalid CD Key.\n" );
			return;
		}
	}
	else if ( _stricmp( certificate, "steam" ) )
	{
		SV_RejectConnection( &address, "Expecting STEAM authentication USERID ticket!\n" );
		return;
	}

	if ( reconnecting )
	{
		Steam_NotifyClientDisconnect( client );
		if ( ( client->active || client->spawned ) && client->edict )
			gEntityInterface.pfnClientDisconnect( client->edict );
		Con_Printf( "%s:reconnect\n", NET_AdrToString( address ) );
	}
	else
	{
		for ( slot = 0; slot < svs.maxclients; ++slot )
		{
			client = &svs.clients[slot];
			if ( !client->active && !client->spawned && !client->connected )
				break;
		}
		if ( slot >= svs.maxclients )
		{
			SV_RejectConnection( &address, "Server is full.\n" );
			return;
		}
	}
	int unfinishedConnections = 0;
	for ( int index = 0; index < svs.maxclients; ++index )
	{
		client_t* other = &svs.clients[index];
		if ( other->connected && !other->fully_connected
			 && NET_CompareBaseAdr( address, other->netchan.remote_address ) )
			++unfinishedConnections;
	}
	if ( unfinishedConnections > CONNECT_UNFINISHED_IP_LIMIT )
	{
		Log_Printf( "Too many connect packets from %s\n", NET_AdrToString( address ) );
		return;
	}

	host_client = client;
	client->userid = g_userid;
	g_userid = static_cast<int>( static_cast<unsigned int>( g_userid ) + 1 );
	if ( authentication == CONNECT_AUTH_STEAM )
	{
		int certificateBytes = static_cast<int>(
			static_cast<unsigned int>( net_message.cursize ) - static_cast<unsigned int>( msg_readcount ) );
		if ( certificateBytes <= 0 || static_cast<unsigned int>( certificateBytes ) >= CONNECT_INFO_BYTES )
		{
			SV_RejectConnection(
				&address, "STEAM certificate length error! %i/%i\n", certificateBytes, CONNECT_INFO_BYTES );
			return;
		}
		char steamCertificate[CONNECT_INFO_BYTES];
		std::memcpy( steamCertificate, net_message.data + msg_readcount, certificateBytes );
		host_client->network_userid.clientip = NET_NetAdr_GetIPNetworkByteOrder( &address );
		if ( address.type == NA_LOOPBACK )
		{
			if ( sv_lan.value <= 0.0 )
				host_client->network_userid.clientip = NET_NetAdr_GetIPNetworkByteOrder( &net_local_adr );
			else
				host_client->network_userid.clientip = static_cast<unsigned int>( BigLong( CONNECT_LOOPBACK_IP ) );
		}
		host_client->netchan.remote_address.port = address.port ? address.port : defaultPort;
		if ( !Steam_NotifyClientConnect( client, steamCertificate, certificateBytes ) )
		{
			if ( sv_lan.value == 0.0 )
			{
				SV_RejectConnection( &address, "STEAM validation rejected\n" );
				return;
			}
			host_client->network_userid.idtype = 1;
			host_client->network_userid.m_SteamID = 0;
		}
	}
	else
	{
		if ( authentication != CONNECT_AUTH_CERTIFICATE )
		{
			SV_RejectConnection( &address, "Invalid authentication type\n" );
			return;
		}
		const char* hltv = Info_ValueForKey( userInfo, "*hltv" );
		if ( !Q_strlen( hltv ) || Q_atoi( hltv ) != 1 )
		{
			SV_RejectConnection( &address, "Invalid validation type\n" );
			return;
		}
		host_client->network_userid.idtype = 3;
		host_client->network_userid.m_SteamID = 0;
		host_client->network_userid.clientip = NET_NetAdr_GetIPNetworkByteOrder( &address );
		Steam_NotifyBotConnect( client );
	}

	SV_ClearResourceLists( host_client );
	SV_ClearFrames( &host_client->frames );
	host_client->frames = static_cast<client_frame_t*>( Mem_ZeroMalloc( sizeof( client_frame_t ) * SV_UPDATE_BACKUP ) );
	host_client->resourcesneeded.pPrev = &host_client->resourcesneeded;
	host_client->resourcesneeded.pNext = &host_client->resourcesneeded;
	host_client->resourcesonhand.pPrev = &host_client->resourcesonhand;
	host_client->resourcesonhand.pNext = &host_client->resourcesonhand;
	host_client->edict = EDICT_NUM( slot + 1 );
	if ( g_modfuncs.m_pfnConnectClient )
		g_modfuncs.m_pfnConnectClient( slot );
	Netchan_Setup( NS_SERVER, &host_client->netchan, address, static_cast<int>( client - svs.clients ), client,
		SV_GetFragmentSize );
	host_client->next_messageinterval = CONNECT_MESSAGE_INTERVAL;
	host_client->next_messagetime = realtime + CONNECT_MESSAGE_INTERVAL;
	host_client->delta_sequence = -1;
	Q_memset( &host_client->lastcmd, 0, sizeof( host_client->lastcmd ) );
	host_client->nextping = -1.0;
	if ( host_client->netchan.remote_address.type == NA_LOOPBACK )
		Con_DPrintf( "Local connection.\n" );
	else
		Con_DPrintf( "Client %s connected\nAdr: %s\n", name, NET_AdrToString( host_client->netchan.remote_address ) );
	Q_strncpy( host_client->hashedcdkey, hashedKey, CONNECT_HASHED_KEY_BYTES );
	host_client->hashedcdkey[CONNECT_HASHED_KEY_BYTES] = 0;
	host_client->active = 0;
	host_client->spawned = 0;
	host_client->connected = 1;
	host_client->uploading = 0;
	host_client->fully_connected = 0;
	qboolean secure = Steam_GSBSecure();
	int build = build_number();
	char* remoteAddress = NET_AdrToString( host_client->netchan.remote_address );
	Netchan_OutOfBandPrint( NS_SERVER, address, "%c %i \"%s\" %i %i", CONNECT_REPLY_MARKER,
		host_client->userid, remoteAddress, secure, build );
	remoteAddress = NET_AdrToString( host_client->netchan.remote_address );
	const char* clientID = SV_GetClientIDString( host_client );
	Log_Printf( "\"%s<%i><%s><>\" connected, address \"%s\"\n", name, host_client->userid, clientID, remoteAddress );
	Q_strncpy( host_client->userinfo, userInfo, CONNECT_USERINFO_LIMIT );
	host_client->userinfo[CONNECT_USERINFO_LIMIT - 1] = 0;
	SV_ExtractFromUserinfo( host_client );
	Info_SetValueForStarKey( host_client->userinfo, "*sid",
		va( "%lld", static_cast<long long>( host_client->network_userid.m_SteamID ) ), CONNECT_USERINFO_LIMIT );
	host_client->datagram.flags = FSB_ALLOWOVERFLOW;
	host_client->datagram.data = host_client->datagram_buf;
	host_client->datagram.maxsize = CLIENT_DATAGRAM_SIZE;
	host_client->datagram.buffername = host_client->name;
	host_client->sendinfo_time = 0.0f;
}
int SV_GetFragmentSize( void* state )
{
	constexpr int SV_MINIMUM_FRAGMENT_SIZE = 256;
	constexpr int SV_MAXIMUM_FRAGMENT_SIZE = 1024;
	client_t* client = static_cast<client_t*>( state );
	if ( !client->active || !client->spawned || !client->connected || !client->fully_connected )
		return SV_MAXIMUM_FRAGMENT_SIZE;
	const char* downloadSize = Info_ValueForKey( client->userinfo, "cl_dlmax" );
	if ( !Q_strlen( downloadSize ) )
		return SV_MINIMUM_FRAGMENT_SIZE;
	int size = Q_atoi( downloadSize );
	if ( size < SV_MINIMUM_FRAGMENT_SIZE )
		size = SV_MINIMUM_FRAGMENT_SIZE;
	if ( size > SV_MAXIMUM_FRAGMENT_SIZE )
		return SV_MAXIMUM_FRAGMENT_SIZE;
	return size;
}

void SV_LoadEntities()
{
	ED_LoadFromFile( sv.worldmodel->entities );
}

void SV_ClearEntities()
{
	int count = sv.num_edicts;
	for ( int index = 0; index < count; ++index )
	{
		edict_t* entity = &sv.edicts[index];
		if ( !entity->free )
		{
			ReleaseEntityDLLFields( entity );
			count = sv.num_edicts;
		}
	}
}

void SV_ClearCaches()
{
	for ( int index = 1; index < MAX_EVENT_SCRIPTS; ++index )
	{
		event_t* event = &sv.event_precache[index];
		if ( !event->filename )
			break;
		const char* script = event->pszScript;
		event->filename = nullptr;
		if ( script )
			Mem_Free( const_cast<char*>( script ) );
		event->pszScript = nullptr;
	}
}

extern qboolean g_bOutOfDateRestart;
extern float scr_centertime_off;
extern float g_LastScreenUpdateTime;
extern int current_skill;
extern char* gNullString;
extern edict_t** g_moved_edict;
extern vec3_t* g_moved_from;
constexpr int INLINE_MODEL_NAME_BYTES = 5;
extern char localmodels[MAX_MODELS][INLINE_MODEL_NAME_BYTES];
void SV_ClearWorld();
void SV_SetMoveVars();

int SV_SpawnServer( qboolean, char* mapName, char* startspot )
{
	constexpr int MAP_COMMAND_BYTES = 256;
	constexpr int MAX_SKILL_LEVEL = 3;
	if ( sv.active && svs.maxclients > 1 )
	{
		client_t* client = svs.clients;
		for ( int index = 0; index < svs.maxclients; ++index, ++client )
		{
			if ( ( client->active || client->spawned || client->connected ) && client->edict && !client->edict->free )
			{
				if ( client->edict->pvPrivateData )
					gEntityInterface.pfnClientDisconnect( client->edict );
				else
					Con_Printf( "Skipping reconnect on %s, no pvPrivateData\n", client->name );
			}
		}
	}
	if ( g_bOutOfDateRestart == 1 )
	{
		g_bOutOfDateRestart = 0;
		Cmd_ExecuteString( "quit\n", src_command );
	}
	Log_Open();
	Log_Printf( "Loading map \"%s\"\n", mapName );
	char command[MAP_COMMAND_BYTES];
	if ( mapchangecfgfile.string && std::strlen( mapchangecfgfile.string ) > 2 )
	{
		AlertMessage( at_console, "Executing map change config file\n" );
		_snprintf( command, sizeof( command ), "exec %s\n", mapchangecfgfile.string );
		Cbuf_AddText( command );
	}
	if ( mapName && std::strlen( mapName ) > 2 )
	{
		_snprintf( command, sizeof( command ), "exec maps/%s_load.cfg\n", mapName );
		Cbuf_AddText( command );
	}
	Log_PrintServerVars();
	NET_Config( svs.maxclients > 1 );
	const char* hostname = Cvar_VariableString( "hostname" );
	if ( hostname && !*hostname )
	{
		if ( gEntityInterface.pfnGetGameDescription )
			Cvar_Set( "hostname", gEntityInterface.pfnGetGameDescription() );
		else
			Cvar_Set( "hostname", "Half-Life" );
	}
	scr_centertime_off = 0.0f;
	if ( startspot )
		Con_DPrintf( "Spawn Server %s: [%s]\n", mapName, startspot );
	else
		Con_DPrintf( "Spawn Server %s\n", mapName );
	svs.spawncount = ++gHostSpawnCount;
	g_LastScreenUpdateTime = 0.0f;
	if ( coop.value != 0.0f )
		Cvar_SetValue( "deathmatch", 0.0f );
	double adjustedSkill = skill.value + 0.5;
	if ( adjustedSkill >= static_cast<double>( std::numeric_limits<int>::min() )
		 && adjustedSkill < static_cast<double>( std::numeric_limits<int>::max() ) + 1.0 )
		current_skill = static_cast<int>( adjustedSkill );
	else
		current_skill = std::numeric_limits<int>::min();
	if ( current_skill < 0 )
		current_skill = 0;
	else if ( current_skill > MAX_SKILL_LEVEL )
		current_skill = MAX_SKILL_LEVEL;
	Cvar_SetValue( "skill", static_cast<float>( current_skill ) );
	ContinueLoadingProgressBar( "Server", 2, 0.0f );
	HPAK_CheckSize( "custom" );
	char oldname[MAX_QPATH];
	oldname[0] = '\0';
	Q_strncpy( oldname, sv.name, sizeof( oldname ) - 1 );
	oldname[sizeof( oldname ) - 1] = '\0';
	Host_ClearMemory( 0 );
	for ( int index = 0; index < svs.maxclientslimit; ++index )
		SV_ClearFrames( &svs.clients[index].frames );
	SV_UPDATE_BACKUP = svs.maxclients == 1 ? SINGLEPLAYER_BACKUP : MULTIPLAYER_BACKUP;
	SV_UPDATE_MASK = SV_UPDATE_BACKUP - 1;
	SV_AllocClientFrames();
	Q_memset( &sv, 0, sizeof( sv ) );
	Q_strncpy( sv.oldname, oldname, sizeof( sv.oldname ) - 1 );
	sv.oldname[sizeof( sv.oldname ) - 1] = '\0';
	Q_strncpy( sv.name, mapName, sizeof( sv.name ) - 1 );
	sv.name[sizeof( sv.name ) - 1] = '\0';
	if ( startspot )
	{
		Q_strncpy( sv.startspot, startspot, sizeof( sv.startspot ) - 1 );
		sv.startspot[sizeof( sv.startspot ) - 1] = '\0';
	}
	else
		sv.startspot[0] = '\0';
	pr_strings = gNullString;
	gGlobalVariables.pStringBase = gNullString;
	if ( svs.maxclients == 1 )
		Cvar_SetValue( "sv_clienttrace", 1.0f );
	// Allocate server memory
	sv.max_edicts = COM_EntsForPlayerSlots( svs.maxclients );
	if ( g_moved_edict )
		std::free( g_moved_edict );
	if ( g_moved_from )
		std::free( g_moved_from );
	int maxEdicts = sv.max_edicts;
	g_moved_edict = nullptr;
	g_moved_from = nullptr;
	if ( maxEdicts )
	{
		g_moved_edict = static_cast<edict_t**>( Mem_ZeroMalloc( sizeof( *g_moved_edict ) * maxEdicts ) );
		if ( g_moved_from )
			Con_Printf( "Reallocate on moved_from\n" );
		g_moved_from = static_cast<vec3_t*>( Mem_ZeroMalloc( sizeof( *g_moved_from ) * maxEdicts ) );
	}
	else
		Con_DPrintf( "SV_ReallocateDynamicData with sv.max_edicts == 0" );
	gGlobalVariables.maxEntities = sv.max_edicts;
	sv.edicts = static_cast<edict_t*>( Hunk_AllocName( sizeof( edict_t ) * sv.max_edicts, "edicts" ) );
	sv.baselines =
		static_cast<entity_state_t*>( Hunk_AllocName( sizeof( entity_state_t ) * sv.max_edicts, "baselines" ) );
	sv.datagram.maxsize = sizeof( sv.datagram_buf );
	sv.datagram.cursize = 0;
	sv.datagram.data = sv.datagram_buf;
	sv.datagram.buffername = "Server Datagram";
	sv.reliable_datagram.maxsize = sizeof( sv.reliable_datagram_buf );
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;
	sv.reliable_datagram.buffername = "Server Reliable Datagram";
	sv.multicast.maxsize = sizeof( sv.multicast_buf );
	sv.multicast.data = sv.multicast_buf;
	sv.multicast.buffername = "Server Multicast Buffer";
	sv.spectator.maxsize = sizeof( sv.spectator_buf );
	sv.spectator.data = sv.spectator_buf;
	sv.spectator.buffername = "Server Spectator Buffer";
	sv.signon.data = sv.signon_data;
	sv.signon.maxsize = sizeof( sv.signon_data );
	sv.signon.cursize = 0;
	sv.signon.buffername = "Server Signon Buffer";
	sv.num_edicts = svs.maxclients + 1;
	for ( int index = 0; index < svs.maxclients; ++index )
		svs.clients[index].edict = &sv.edicts[index + 1];
	gGlobalVariables.maxClients = svs.maxclients;
	sv.state = ss_loading;
	sv.paused = 0;
	sv.time = 1.0;
	gGlobalVariables.time = 1.0f;
	R_ForceCVars( svs.maxclients > 1 );
	ContinueLoadingProgressBar( "Server", 3, 0.0f );
	_snprintf( sv.modelname, sizeof( sv.modelname ), "maps/%s.bsp", mapName );
	sv.worldmodel = Mod_ForName( sv.modelname, 0, 0 );
	if ( !sv.worldmodel )
	{
		Con_Printf( "Couldn't spawn server %s\n", sv.modelname );
		sv.active = 0;
		return 0;
	}
	Mod_CreateInlineModels( sv.worldmodel );
	Sequence_OnLevelLoad( mapName );
	ContinueLoadingProgressBar( "Server", 4, 0.0f );
	if ( gmodinfo.clientcrccheck )
	{
		char dllName[MAX_QPATH];
		_snprintf( dllName, sizeof( dllName ), "cl_dlls//client.dll" );
		COM_FixSlashes( dllName );
		if ( !MD5_Hash_File( sv.clientdllmd5, dllName, 0, 0, nullptr ) )
		{
			Con_Printf( "Couldn't CRC client side dll:  %s\n", dllName );
			sv.active = 0;
			return 0;
		}
	}
	ContinueLoadingProgressBar( "Server", 6, 0.0f );
	if ( svs.maxclients <= 1 )
		sv.worldmapCRC = 0;
	else
	{
		// Server map CRC check.
		CRC32_Init( &sv.worldmapCRC );
		if ( !CRC_MapFile( &sv.worldmapCRC, sv.modelname ) )
		{
			Con_Printf( "Couldn't CRC server map:  %s\n", sv.modelname );
			sv.active = 0;
			return 0;
		}
		CM_CalcPAS( sv.worldmodel );
	}
	sv.models[1] = sv.worldmodel;
	SV_ClearWorld();
	sv.model_precache_flags[1] |= 1;
	sv.sound_precache[0] = pr_strings;
	sv.model_precache[0] = pr_strings;
	sv.generic_precache[0] = pr_strings;
	sv.model_precache[1] = sv.modelname;
	for ( int index = 1; index < sv.worldmodel->numsubmodels; ++index )
	{
		sv.model_precache[index + 1] = localmodels[index];
		sv.models[index + 1] = Mod_ForName( localmodels[index], 0, 0 );
		sv.model_precache_flags[index + 1] |= 1;
	}
	edict_t* world = &sv.edicts[0];
	Q_memset( &world->v, 0, sizeof( world->v ) );
	world->free = 0;
	world->v.model = static_cast<string_t>( sv.worldmodel->name - pr_strings );
	world->v.modelindex = 1; // world model
	world->v.solid = SOLID_BSP;
	world->v.movetype = MOVETYPE_PUSH;
	if ( coop.value == 0.0f )
		gGlobalVariables.deathmatch = deathmatch.value;
	else
		gGlobalVariables.coop = coop.value;
	gGlobalVariables.mapname = static_cast<string_t>( sv.name - pr_strings );
	gGlobalVariables.startspot = static_cast<string_t>( sv.startspot - pr_strings );
	gGlobalVariables.serverflags = static_cast<float>( svs.serverflags );
	const char* allowValue = "1";
	if ( deathmatch.value != 0.0f )
	{
		Cvar_Set( "sv_skyname", "desert" );
		Cvar_Set( "sv_skycolor_r", "0" );
		Cvar_Set( "sv_skycolor_g", "0" );
		Cvar_Set( "sv_skycolor_b", "0" );
		Cvar_Set( "sv_skyvec_x", "0" );
		Cvar_Set( "sv_skyvec_y", "0" );
		Cvar_Set( "sv_skyvec_z", "0" );
		Cvar_Set( "room_type", "0" );
		allowValue = "0";
	}
	Cvar_Set( "sv_allow_shaders", allowValue );
	Cvar_Set( "sv_allow_autoaim", allowValue );
	SV_SetMoveVars();
	return 1;
}

int Cvar_CountServerVariables();
encoder_t DELTA_LookupEncoder( char* name );
void DELTA_FreeDescription( delta_t** description );
qboolean COM_CreateCustomization( customization_t* head, resource_t* resource, int playerNumber, int flags,
	customization_t** result, int* lumpCount );
void SV_MoveToOnHandList( resource_t* resource );
void SV_RequestMissingResourcesFromClients();
void Steam_RunFrame();
void Steam_NotifyOfLevelChange();

void SV_EndRedirect()
{
	SV_FlushRedirect();
	sv_redirected = RD_NONE;
}

void SV_SetNewInfo( int newblindex )
{
	g_svdeltacallback.newbl = true;
	g_svdeltacallback.newblindex = newblindex;
}

qboolean RequireValidChallenge( netadr_t* )
{
	return sv_enableoldqueries.value == 0.0f;
}

qboolean SV_IsPlayerIndex( int index )
{
	return index > 0 && index <= svs.maxclients;
}

int SV_HashString( const char* text, int bounds )
{
	if ( !*text )
		return 0;

	unsigned int hash = 0;
	int character = static_cast<signed char>( *text );
	do
	{
		++text;
		hash = static_cast<unsigned int>( std::tolower( character ) ) + 2 * hash;
		character = static_cast<signed char>( *text );
	} while ( character != 0 );
	return static_cast<int>( hash % static_cast<unsigned int>( bounds ) );
}

void SV_AddSampleToHashedLookupTable( const char* sample, int sampleIndex )
{
	unsigned int slot = static_cast<unsigned int>( SV_HashString( sample, SOUND_HASH_TABLE_SIZE ) );
	unsigned int firstSlot = slot;
	while ( sv.sound_precache_hashedlookup[slot] != 0 )
	{
		++hashstrings_collisions;
		++slot;
		if ( slot == SOUND_HASH_TABLE_SIZE )
			slot = 0;
		if ( slot == firstSlot )
			Sys_Error( "SV_AddSampleToHashedLookupTable: NO FREE SLOTS IN SOUND LOOKUP TABLE" );
	}
	sv.sound_precache_hashedlookup[slot] = static_cast<short>( sampleIndex );
}

void SV_BuildHashedSoundLookupTable()
{
	Q_memset( sv.sound_precache_hashedlookup, 0, sizeof( sv.sound_precache_hashedlookup ) );
	for ( int index = 0; index < MAX_SOUNDS; ++index )
	{
		const char* sample = sv.sound_precache[index];
		if ( !sample )
			break;
		SV_AddSampleToHashedLookupTable( sample, index );
	}
	sv.sound_precache_hashedlookup_built = true;
}

void SV_AllocPacketEntities( client_frame_t* frame )
{
	constexpr int MAX_PACKET_ENTITIES = 1024;
	if ( !frame )
		return;

	frame->entities.num_entities = 0;
	if ( !frame->entities.entities )
		frame->entities.entities =
			static_cast<entity_state_t*>( Mem_ZeroMalloc( sizeof( entity_state_t ) * MAX_PACKET_ENTITIES ) );
}

void SV_WriteVoiceCodec( sizebuf_t* buffer )
{
	MSG_WriteByte( buffer, svc_voiceinit );
	MSG_WriteString( buffer, com_empty_string );
	MSG_WriteByte( buffer, 0 );
}

void SV_CleanupEnts()
{
	for ( int index = 1; index < sv.num_edicts; ++index )
		sv.edicts[index].v.effects &= ~( EF_MUZZLEFLASH | EF_NOINTERP );
}

void SV_BeginRedirect( redirect_t target, netadr_t* address )
{
	sv_redirected = target;
	sv_redirectto = *address;
	outputbuf[0] = '\0';
}

void SV_FailDownload( char* filename )
{
	if ( !filename || !filename[0] )
		return;

	MSG_WriteByte( &host_client->netchan.message, svc_filetxferfailed );
	MSG_WriteString( &host_client->netchan.message, filename );
}

void SV_SetCallback( int number, qboolean remove, qboolean custom, int* numberBase, qboolean full, int offset )
{
	g_svdeltacallback.newblindex = 0;
	g_svdeltacallback.num = number;
	g_svdeltacallback.remove = remove;
	g_svdeltacallback.custom = custom;
	g_svdeltacallback.numbase = numberBase;
	g_svdeltacallback.newbl = false;
	g_svdeltacallback.full = full;
	g_svdeltacallback.offset = offset;
}

int SV_GetFakeClientCount()
{
	if ( svs.maxclients <= 0 )
		return 0;

	int count = 0;
	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		if ( client->fakeclient )
			++count;
	}
	return count;
}

void SV_EmitPacketEntities( client_t* client, packet_entities_t* to, sizebuf_t* message )
{
	if ( client->delta_sequence == -1 )
		SV_CreatePacketEntities( sv_packet_nodelta, client, to, message );
	else // Otherwise, copy the client->datagram into the message stream, too.
		SV_CreatePacketEntities( sv_packet_delta, client, to, message );
}

qboolean SV_IsSimulating()
{
	if ( sv.paused )
		return false;
	if ( svs.maxclients > 1 )
		return true;
	if ( key_dest == key_game )
		return cls.state == ca_active || cls.state == ca_dedicated;
	return false;
}

void SV_FindModelNumbers()
{
	sv_playermodel = -1;
	for ( int index = 0; index < MAX_MODELS; ++index )
	{
		char* model = sv.model_precache[index];
		if ( !model )
			break;
		if ( Q_strcasecmp( model, "models/player.mdl" ) == 0 )
			sv_playermodel = index;
	}
}

/*
==================
void SV_CountPlayers

Counts number of connections.  Clients includes regular connections
==================
*/
void SV_CountPlayers( int* clients )
{
	*clients = 0;
	int count = svs.maxclients;
	client_t* client = svs.clients;
	for ( int index = 0; index < count; ++index, ++client )
	{
		if ( client->active || client->spawned || client->connected )
		{
			++*clients;
			count = svs.maxclients;
		}
	}
}

qboolean SV_ShouldUpdatePing( client_t* client )
{
	constexpr double PROXY_PING_INTERVAL = 2.0;
	if ( !client->proxy )
		return ( client->lastcmd.buttons & IN_SCORE ) != 0;
	if ( realtime < client->nextping )
		return false;
	client->nextping = realtime + PROXY_PING_INTERVAL;
	return true;
}

void SV_Shutdown()
{
	delta_info_t* info = g_sv_delta;
	while ( info )
	{
		delta_info_t* next = info->next;
		if ( info->delta )
			DELTA_FreeDescription( &info->delta );
		Mem_Free( info->name );
		Mem_Free( info->loadfile );
		Mem_Free( info );
		info = next;
	}
	g_sv_delta = nullptr;
}

void SV_CountProxies( int* proxies )
{
	*proxies = 0;
	int count = svs.maxclients;
	client_t* client = svs.clients;
	for ( int index = 0; index < count; ++index, ++client )
	{
		if ( ( client->active || client->spawned || client->connected ) && client->proxy )
		{
			++*proxies;
			count = svs.maxclients;
		}
	}
}

qboolean ValidInfoChallenge( netadr_t* address, const char* nugget )
{
	( void )address;
	if ( !sv.active || !nugget || svs.maxclients <= 1 )
		return false;
	if ( sv_enableoldqueries.value == 0.0f )
		return Q_stricmp( nugget, "Source Engine Query" ) == 0;
	return true;
}

int SV_FindEmptySlot( netadr_t* address, int* slot, client_t** clientOut )
{
	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		if ( client->active || client->spawned || client->connected )
			continue;
		*slot = index;
		*clientOut = client;
		return 1;
	}
	SV_RejectConnection( address, "Server is full.\n" );
	return 0;
}

void SV_DeallocateDynamicData()
{
	if ( g_moved_edict )
		Mem_Free( g_moved_edict );
	if ( g_moved_from )
		Mem_Free( g_moved_from );
	g_moved_edict = nullptr;
	g_moved_from = nullptr;
}

void SV_ReallocateDynamicData()
{
	int maxEdicts = sv.max_edicts;
	if ( !maxEdicts )
	{
		Con_DPrintf( "SV_ReallocateDynamicData with sv.max_edicts == 0" );
		return;
	}
	if ( g_moved_edict )
		Con_Printf( "Reallocate on moved_edict\n" );
	g_moved_edict = static_cast<edict_t**>( Mem_ZeroMalloc( sizeof( *g_moved_edict ) * maxEdicts ) );
	if ( g_moved_from )
		Con_Printf( "Reallocate on moved_from\n" );
	g_moved_from = static_cast<vec3_t*>( Mem_ZeroMalloc( sizeof( *g_moved_from ) * maxEdicts ) );
}

void SV_CheckMapDifferences()
{
	static double lastCheck = 0.0;
	if ( realtime - lastCheck < 5.0 )
		return;

	lastCheck = realtime;
	int count = svs.maxclients;
	client_t* client = svs.clients;
	for ( int index = 0; index < count; ++index, ++client )
	{
		if ( !client->active )
			continue;
		if ( client->crcValue != 0 && client->netchan.remote_address.type != NA_LOOPBACK
			 && client->crcValue != static_cast<int>( sv.worldmapCRC ) )
			client->netchan.message.flags |= FSB_OVERFLOWED;
	}
}

qboolean SV_CheckRconFailure( netadr_t* address )
{
	for ( int index = 0; index < MAX_RCON_ADDRESSES; ++index )
	{
		rcon_failure_t* failure = &g_rgRconFailures[index];
		if ( failure->active && failure->shouldreject && NET_CompareAdr( *address, failure->adr ) )
			return true;
	}
	return false;
}

void SVC_Ping()
{
	constexpr int PING_PACKET_BYTES = 6;
	byte data[PING_PACKET_BYTES];
	int marker = -1;
	std::memcpy( data, &marker, sizeof( marker ) );
	data[4] = 'j';
	data[5] = 0;
	NET_SendPacket( NS_SERVER, PING_PACKET_BYTES, data, net_from );
}

void SV_HandleRconPacket()
{
	MSG_BeginReading();
	MSG_ReadLong();
	char* text = MSG_ReadStringLine();
	Cmd_TokenizeString( text );
	const char* command = Cmd_Argv( 0 );
	if ( Q_strcmp( command, "getchallenge" ) == 0 )
		SVC_GetChallenge();
	else if ( Q_stricmp( command, "challenge" ) == 0 )
		SVC_ServiceChallenge();
	else if ( Q_strcmp( command, "rcon" ) == 0 )
		SV_Rcon( &net_from );
}

void SV_CheckRate( client_t* client )
{
	constexpr float MINIMUM_CLIENT_RATE = 1000.0f;
	constexpr float MAXIMUM_CLIENT_RATE = 100000.0f;
	if ( sv_maxrate.value > 0.0f && client->netchan.rate > static_cast<double>( sv_maxrate.value ) )
	{
		if ( sv_maxrate.value >= MAXIMUM_CLIENT_RATE )
			client->netchan.rate = MAXIMUM_CLIENT_RATE;
		else
			client->netchan.rate = sv_maxrate.value;
	}
	if ( sv_minrate.value > 0.0f && static_cast<double>( sv_minrate.value ) > client->netchan.rate )
	{
		double minimumRate = MINIMUM_CLIENT_RATE;
		if ( sv_minrate.value > MINIMUM_CLIENT_RATE )
			minimumRate = sv_minrate.value;
		client->netchan.rate = minimumRate;
	}
}

/*
================
SV_RejectConnectionForPassword(char *)

Rejects connection request for having a bad password and sends back a message
================
*/
void SV_RejectConnectionForPassword( netadr_t* address )
{
	constexpr int PASSWORD_REJECTION = 56;
	SZ_Clear( &net_message );
	MSG_WriteLong( &net_message, -1 ); // -1 -1 -1 -1 signal
	MSG_WriteByte( &net_message, PASSWORD_REJECTION );
	MSG_WriteString( &net_message, "BADPASSWORD" );
	NET_SendPacket( NS_SERVER, net_message.cursize, net_message.data, *address );
	SZ_Clear( &net_message );
}

int SV_FinishCertificateCheck( netadr_t* address, int authProtocol, char* certificate, char* userinfo )
{
	constexpr int CERTIFICATE_TEXT_BYTES = 32;
	if ( authProtocol == CHALLENGE_AUTH_NON_STEAM )
	{
		if ( Q_strlen( certificate ) == CERTIFICATE_TEXT_BYTES )
		{
			if ( address->type == NA_LOOPBACK )
				return 1;
			const char* hltv = Info_ValueForKey( userinfo, "*hltv" );
			if ( Q_strlen( hltv ) != 0 && Q_atoi( hltv ) == 1 )
				return 1;
		}
		SV_RejectConnection( address, "Invalid CD Key.\n" );
		return 0;
	}
	if ( Q_strcasecmp( certificate, "steam" ) == 0 )
		return 1;
	SV_RejectConnection( address, "Expecting STEAM authentication USERID ticket!\n" );
	return 0;
}

qboolean CheckChallengeNr( netadr_t* address, int challenge )
{
	constexpr double CHALLENGE_LIFETIME = 3600.0;
	if ( !address || challenge == -1 )
		return false;
	for ( int index = 0; index < MAX_CHALLENGES; ++index )
	{
		challenge_t* entry = &g_rg_sv_challenges[index];
		if ( !NET_CompareBaseAdr( *address, entry->adr ) )
			continue;
		return challenge == entry->challenge && realtime <= static_cast<double>( entry->time ) + CHALLENGE_LIFETIME;
	}
	return false;
}

int GetChallengeNr( netadr_t* address )
{
	int oldest = 0;
	int oldestTime = INT_MAX;
	for ( int index = 0; index < MAX_CHALLENGES; ++index )
	{
		challenge_t* entry = &g_rg_sv_challenges[index];
		if ( NET_CompareBaseAdr( *address, entry->adr ) )
		{
			entry->time = static_cast<int>( realtime );
			return entry->challenge;
		}
		if ( entry->time < oldestTime )
		{
			oldestTime = entry->time;
			oldest = index;
		}
	}

	unsigned int highWord = static_cast<unsigned int>( RandomLong( 0, MAX_CHALLENGE_HIGH_WORD ) );
	unsigned int lowWord = static_cast<unsigned int>( RandomLong( 0, MAX_CHALLENGE_LOW_WORD ) );
	g_rg_sv_challenges[oldest].challenge = static_cast<int>( ( highWord << CHALLENGE_WORD_SHIFT ) | lowWord );
	g_rg_sv_challenges[oldest].adr = net_from;
	g_rg_sv_challenges[oldest].time = static_cast<int>( realtime );
	return g_rg_sv_challenges[oldest].challenge;
}

void ReplyServerChallenge( netadr_t* address )
{
	constexpr int CHALLENGE_PACKET_BYTES = 16;
	byte data[CHALLENGE_PACKET_BYTES];
	sizebuf_t buffer;
	buffer.buffername = "SVC_Challenge";
	buffer.maxsize = sizeof( data );
	buffer.cursize = 0;
	buffer.data = data;
	buffer.flags = FSB_ALLOWOVERFLOW;
	MSG_WriteLong( &buffer, -1 );
	MSG_WriteByte( &buffer, CHALLENGE_REPLY_MARKER );
	MSG_WriteLong( &buffer, GetChallengeNr( address ) );
	NET_SendPacket( NS_SERVER, buffer.cursize, buffer.data, *address );
}

void SV_CheckTimeouts()
{
	float dropTime = static_cast<float>( realtime - sv_timeout.value );
	int count = svs.maxclients;
	client_t* client = svs.clients;
	for ( int index = 0; index < count; )
	{
		if ( client->fakeclient || ( !client->connected && !client->active && !client->spawned )
			 || !( dropTime > client->netchan.last_received ) )
		{
			++index;
			++client;
			continue;
		}
		SV_BroadcastPrintf( "%s timed out\n", client->name );
		client_t* timedOut = client;
		++index;
		++client;
		SV_DropClient( timedOut, false, "Timed out" );
		count = svs.maxclients;
	}
}

void SV_CheckCmdTimes()
{
	static double lastReset = 0.0;
	if ( Host_IsSinglePlayerGame() )
		return;
	if ( realtime - lastReset < 1.0 )
		return;

	lastReset = realtime;
	if ( svs.maxclients <= 0 )
		return;

	float upper = clockwindow.value;
	float lower = -clockwindow.value;
	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		if ( !client->connected || !client->active )
			continue;
		if ( client->connecttime == 0.0 )
			client->connecttime = realtime;

		float difference = static_cast<float>( client->cmdtime + client->connecttime - realtime );
		if ( difference > upper )
		{
			client->ignorecmdtime = realtime + upper;
			client->cmdtime = realtime - client->connecttime;
		}
		else if ( difference < lower )
		{
			client->cmdtime = realtime - client->connecttime;
		}
	}
}

/*
================
SV_CheckProtocol

Make sure connecting client is using proper protocol
================
*/
int SV_CheckProtocol( netadr_t* address, int protocol )
{
	if ( !address )
		Sys_Error( "SV_CheckProtocol:  Null address\n" );
	if ( protocol == PROTOCOL_VERSION )
		return 1;

	if ( protocol > PROTOCOL_VERSION )
	{
		const char* contact = "(no email address specified)";
		if ( Q_strlen( sv_contact.string ) > 0 )
			contact = sv_contact.string;
		SV_RejectConnection( address,
			"This server is using an older protocol ( %i ) than your client ( %i ).  If you believe this server is outdated, you can contact the server administrator at %s.\n",
			PROTOCOL_VERSION, protocol, contact );
		return 0;
	}

	SV_RejectConnection( address,
		"This server is using a newer protocol ( %i ) than your client ( %i ).  You should check for updates to your client.\n",
		PROTOCOL_VERSION, protocol );
	return 0;
}

qboolean SV_HasEventsInQueue( client_t* client )
{
	if ( !client )
		return 0;
	event_info_t* info = client->events.ei;
	event_info_t* end = reinterpret_cast<event_info_t*>( &client->edict );
	while ( info->index == 0 )
	{
		++info;
		if ( info == end )
			return 0;
	}
	return 1;
}

void SV_InitEncoders()
{
	for ( delta_info_t* info = g_sv_delta; info; info = info->next )
	{
		delta_t* delta = info->delta;
		if ( Q_strlen( delta->conditionalencodename ) > 0 )
			delta->conditionalencode = DELTA_LookupEncoder( delta->conditionalencodename );
	}
}

void SV_SendBan()
{
	const char* message = "You have been banned from this server.\n";
	SZ_Clear( &net_message );
	MSG_WriteLong( &net_message, -1 );
	MSG_WriteByte( &net_message, 'l' );
	MSG_WriteString( &net_message, message );
	NET_SendPacket( NS_SERVER, net_message.cursize, net_message.data, net_from );
	SZ_Clear( &net_message );
}

qboolean ValidChallenge( netadr_t* address, int challenge )
{
	if ( !sv.active )
		return sv.active;
	if ( svs.maxclients <= 1 )
		return 0;
	if ( sv_enableoldqueries.value != 0.0f )
		return 1;
	if ( CheckChallengeNr( address, challenge ) )
		return 1;
	ReplyServerChallenge( address );
	return 0;
}

void SVC_RuleInfo()
{
	constexpr int RULE_PACKET_BYTES = 8192;
	if ( !sv.active || svs.maxclients <= 1 )
		return;
	int count = Cvar_CountServerVariables();
	if ( count <= 0 ) // No server rules active.
		return;
	sizebuf_t message;
	byte data[RULE_PACKET_BYTES];
	message.buffername = "SVC_RuleInfo";
	message.maxsize = RULE_PACKET_BYTES;
	message.cursize = 0;
	message.data = data;
	message.flags = FSB_ALLOWOVERFLOW;
	MSG_WriteLong( &message, -1 );
	MSG_WriteByte( &message, 'E' );
	MSG_WriteShort( &message, count );
	for ( cvar_t* variable = cvar_vars; variable; variable = variable->next )
	{
		if ( !( variable->flags & FCVAR_SERVER ) )
			continue;
		MSG_WriteString( &message, variable->name );
		if ( variable->flags & FCVAR_PROTECTED )
		{
			if ( Q_strlen( variable->string ) > 0 && Q_stricmp( variable->string, "none" ) != 0 )
				MSG_WriteString( &message, "1" );
			else
				MSG_WriteString( &message, "0" );
		}
		else
			MSG_WriteString( &message, variable->string );
	}
	NET_SendPacket( NS_SERVER, message.cursize, message.data, net_from );
}

void SV_BuildReconnect( sizebuf_t* message )
{
	MSG_WriteByte( message, svc_stufftext );
	MSG_WriteString( message, "reconnect\n" );
}

int StringToFilter( char* address, ipfilter_t* filter )
{
	unsigned char compareBytes[4];
	unsigned char maskBytes[4];
	char number[128];
	Q_memset( compareBytes, 0, sizeof( compareBytes ) );
	Q_memset( maskBytes, 0, sizeof( maskBytes ) );
	int part = 0;
	while ( true )
	{
		char character = *address;
		if ( character < '0' || character > '9' )
		{
			Con_Printf( "Bad filter address: %s\n", address );
			return false;
		}
		int length = 0;
		do
		{
			++length;
			++address;
			number[length - 1] = character;
			character = *address;
		} while ( character >= '0' && character <= '9' );
		number[length] = '\0';
		unsigned char value = static_cast<unsigned char>( Q_atoi( number ) );
		compareBytes[part] = value;
		if ( value != 0 )
			maskBytes[part] = 0xFF;
		if ( *address == '\0' )
			break;
		++part;
		++address;
		if ( part == 4 )
			break;
	}
	unsigned int mask = 0;
	unsigned int compare = 0;
	std::memcpy( &mask, maskBytes, sizeof( mask ) );
	std::memcpy( &compare, compareBytes, sizeof( compare ) );
	filter->mask = mask;
	filter->compare = compare;
	return true;
}

qboolean SV_FilterUser( USERID_t* userid )
{
	constexpr int AUTH_ID_STRING_BYTES = 64;
	constexpr int STEAM_AUTH_ID_TYPE = 1;
	constexpr int VALVE_AUTH_ID_TYPE = 2;
	if ( numuserfilters <= 0 )
		return sv_filterban.value == 0.0f;

	for ( int index = numuserfilters - 1; index >= 0; --index )
	{
		userfilter_t* record = &userfilters[index];
		float banEnd = record->banEndTime;
		bool expired = banEnd != 0.0f && realtime >= banEnd;
		if ( expired )
		{
			int count = numuserfilters;
			if ( count > index + 1 )
			{
				std::memmove( record, record + 1, static_cast<std::size_t>( count - index - 1 ) * sizeof( *record ) );
			}
			numuserfilters = count - 1;
		}
		else if ( userid )
		{
			USERID_t* filtered = &record->userid;
			if ( userid->idtype == filtered->idtype
				 && ( userid->idtype == STEAM_AUTH_ID_TYPE || userid->idtype == VALVE_AUTH_ID_TYPE ) )
			{
				char first[AUTH_ID_STRING_BYTES];
				char second[AUTH_ID_STRING_BYTES];
				std::strncpy( first, SV_GetIDString( userid ), AUTH_ID_STRING_BYTES - 1 );
				first[AUTH_ID_STRING_BYTES - 1] = '\0';
				std::strncpy( second, SV_GetIDString( filtered ), AUTH_ID_STRING_BYTES - 1 );
				second[AUTH_ID_STRING_BYTES - 1] = '\0';
				if ( Q_strcasecmp( first, second ) == 0 )
					return static_cast<int>( sv_filterban.value );
			}
		}
	}
	return sv_filterban.value == 0.0f;
}

int SV_CheckKeyInfo( netadr_t* address, char* protinfo, unsigned short* port, int* authProtocol, char* rawCertificate,
	int rawSize, char* cdkey )
{
	constexpr int KEYINFO_HASH_BYTES = 32;
	constexpr int CDKEY_COPY_BYTES = 64;
	int protocol = Q_atoi( Info_ValueForKey( protinfo, "prot" ) );
	if ( static_cast<unsigned int>( protocol - 1 ) > 3u )
	{
		SV_RejectConnection( address, "Invalid connection.\n" );
		return 0;
	}
	const char* raw = Info_ValueForKey( protinfo, "raw" );
	if ( Q_strlen( raw ) > 0 )
	{
		if ( protocol != CHALLENGE_AUTH_NON_STEAM )
		{
			Q_strncpy( rawCertificate, raw, rawSize );
			rawCertificate[rawSize - 1] = '\0';
			const char* key = Info_ValueForKey( protinfo, "cdkey" );
			if ( Q_strlen( key ) != KEYINFO_HASH_BYTES )
			{
				SV_RejectConnection( address, "Invalid hashed CD key.\n" );
				return 0;
			}
			Q_strncpy( cdkey, key, CDKEY_COPY_BYTES );
		}
		else if ( Q_strlen( raw ) == KEYINFO_HASH_BYTES )
		{
			Q_strncpy( rawCertificate, raw, rawSize );
			rawCertificate[rawSize - 1] = '\0';
			Q_strncpy( cdkey, raw, CDKEY_COPY_BYTES );
		}
		else
		{
			SV_RejectConnection( address, "Invalid authentication certificate length.\n" );
			return 0;
		}
		*port = static_cast<unsigned short>( Q_atoi( "27005" ) );
		*authProtocol = protocol;
		return 1;
	}
	SV_RejectConnection( address, "Invalid authentication certificate length.\n" );
	return 0;
}

int SV_CheckIPRestrictions( netadr_t* address, int authProtocol )
{
	if ( ( authProtocol == CHALLENGE_AUTH_STEAM && sv_lan.value == 0.0f ) || authProtocol == CHALLENGE_AUTH_NON_STEAM )
		return 1;
	if ( sv_lan.value == 0.0f )
		return 0;
	return NET_CompareClassBAdr( *address, net_local_adr ) || NET_IsReservedAdr( *address );
}

int SV_CheckIPConnectionReuse( netadr_t* address )
{
	constexpr int MAX_PARTIAL_CONNECTIONS = 5;
	int partial = 0;
	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		if ( client->connected && !client->fully_connected
			 && NET_CompareBaseAdr( *address, client->netchan.remote_address ) )
			++partial;
	}
	if ( partial <= MAX_PARTIAL_CONNECTIONS )
		return 1;
	Log_Printf( "Too many connect packets from %s\n", NET_AdrToString( *address ) );
	return 0;
}

qboolean SV_GetModInfo(
	char* info, char* download, int* version, int* size, qboolean* serverOnly, qboolean* clientDll, char* hlVersion )
{
	// Read in the liblist .gam file for this mod?	No, do this at the time we load the entity dll and share the structure.
	//
	if ( gmodinfo.bIsMod )
	{
		Q_strcpy( info, gmodinfo.szInfo );
		Q_strcpy( download, gmodinfo.szDL );
		Q_strcpy( hlVersion, gmodinfo.szHLVersion );
		*version = gmodinfo.version;
		*size = gmodinfo.size;
		*serverOnly = gmodinfo.svonly;
		*clientDll = gmodinfo.cldll;
	}
	else
	{
		Q_strcpy( info, com_empty_string );
		Q_strcpy( download, com_empty_string );
		Q_strcpy( hlVersion, com_empty_string );
		*version = 1;
		*size = 0;
		*serverOnly = true;
		*clientDll = false;
	}
	return gmodinfo.bIsMod;
}

void SV_InactivateClients()
{
	int count = svs.maxclients;
	client_t* client = svs.clients;
	for ( int index = 0; index < count; )
	{
		if ( !client->active && !client->connected && !client->spawned )
		{
			++index;
			++client;
			continue;
		}
		// Fake clients get killed in here.
		if ( client->fakeclient )
		{
			SV_DropClient( client, false, "Dropping fakeclient on level change" );
			count = svs.maxclients;
			++index;
			++client;
			continue;
		}
		client->m_bSentNewResponse = 0;
		client->active = 0;
		client->connected = 1;
		client->spawned = 0;
		client->fully_connected = 0;
		client->hasusrmsgs = 0;
		SZ_Clear( &client->netchan.message );
		SZ_Clear( &client->datagram );
		COM_ClearCustomizationList( &client->customdata, false );
		Q_memset( client->physinfo, 0, sizeof( client->physinfo ) );
		++index;
		++client;
		count = svs.maxclients;
	}
}

void SV_RunMapChangeConfig( char* level )
{
	constexpr int MAP_COMMAND_BYTES = 256;
	char command[MAP_COMMAND_BYTES];
	if ( mapchangecfgfile.string && std::strlen( mapchangecfgfile.string ) > 2 )
	{
		AlertMessage( at_console, "Executing map change config file\n" );
		_snprintf( command, sizeof( command ), "exec %s\n", mapchangecfgfile.string );
		Cbuf_AddText( command );
	}
	if ( level && std::strlen( level ) > 2 )
	{
		_snprintf( command, sizeof( command ), "exec maps/%s_load.cfg\n", level );
		Cbuf_AddText( command );
	}
}

void SV_DownloadingModules()
{
	Con_Printf( "Downloading Security Module from Speakeasy.net ...\n" );
}

int SV_CheckForDuplicateSteamID( client_t* client )
{
	constexpr int AUTH_ID_STRING_BYTES = 64;
	constexpr int STEAM_AUTH_ID_TYPE = 1;
	constexpr int VALVE_AUTH_ID_TYPE = 2;
	if ( sv_lan.value != 0.0f )
		return -1;
	for ( int index = 0; index < svs.maxclients; ++index )
	{
		client_t* other = &svs.clients[index];
		if ( !other->connected || other->fakeclient || other == client )
			continue;
		int idType = other->network_userid.idtype;
		if ( idType != STEAM_AUTH_ID_TYPE && idType != VALVE_AUTH_ID_TYPE )
			continue;
		if ( idType != client->network_userid.idtype )
			continue;
		char first[AUTH_ID_STRING_BYTES];
		char second[AUTH_ID_STRING_BYTES];
		std::strncpy( first, SV_GetIDString( &client->network_userid ), AUTH_ID_STRING_BYTES - 1 );
		first[AUTH_ID_STRING_BYTES - 1] = '\0';
		std::strncpy( second, SV_GetIDString( &other->network_userid ), AUTH_ID_STRING_BYTES - 1 );
		second[AUTH_ID_STRING_BYTES - 1] = '\0';
		if ( Q_strcasecmp( first, second ) == 0 )
			return index;
	}
	return -1;
}

bool BIsValveGame()
{
	for ( int index = 0; index < GAME_APP_ID_MAP_COUNT; ++index )
	{
		if ( Q_strcasecmp( g_GameToAppIDMap[index].pGameDir, com_gamedir ) == 0 )
			return true;
	}
	return false;
}

void SV_ReconnectAllClients()
{
	const char* message = "Server updating Security Module.\n";
	int count = svs.maxclients;
	client_t* client = svs.clients;
	for ( int index = 0; index < count; )
	{
		host_client = client;
		if ( ( !client->active && !client->connected ) || client->fakeclient )
		{
			++index;
			++client;
			continue;
		}
		Netchan_Clear( &client->netchan );
		MSG_WriteByte( &host_client->netchan.message, svc_print );
		MSG_WriteString( &host_client->netchan.message, message );
		MSG_WriteByte( &host_client->netchan.message, svc_stufftext );
		MSG_WriteString( &host_client->netchan.message, "retry\n" );
		SV_DropClient( host_client, false, message );
		count = svs.maxclients;
		client = host_client + 1;
		++index;
	}
}

void SV_ServerShutdown()
{
	constexpr int MAP_COMMAND_BYTES = 256;
	char command[MAP_COMMAND_BYTES];
	if ( disconcfgfile.string && std::strlen( disconcfgfile.string ) > 2 )
	{
		AlertMessage( at_console, "Executing disconnect config file\n" );
		_snprintf( command, sizeof( command ), "exec %s\n", disconcfgfile.string );
		Cbuf_AddText( command );
	}
	if ( std::strlen( sv.name ) > 2 )
	{
		_snprintf( command, sizeof( command ), "exec maps/%s_unload.cfg\n", sv.name );
		Cbuf_AddText( command );
	}
	Steam_NotifyOfLevelChange();
	gGlobalVariables.time = static_cast<float>( sv.time );
	if ( svs.dll_initialized && sv.active )
		gEntityInterface.pfnServerDeactivate();
}

void SV_ProcessFile( client_t* client, char* filename )
{
	constexpr int CUSTOMIZATION_PLAYER_NONE = -1;
	constexpr int CUSTOMIZATION_FLAGS = FCUST_FROMHPAK | FCUST_WIPEDATA | FCUST_IGNOREINIT;
	if ( *filename != '!' )
	{
		Con_Printf( "Ignoring non-customization file upload of %s\n", filename );
		return;
	}
	unsigned char hash[16];
	COM_HexConvert( filename + 4, 32, hash );
	for ( resource_t* resource = client->resourcesneeded.pNext; resource != &client->resourcesneeded;
		resource = resource->pNext )
	{
		if ( Q_memcmp( resource->rgucMD5_hash, hash, sizeof( hash ) ) != 0 )
			continue;
		if ( resource->nDownloadSize != client->netchan.tempbuffersize )
		{
			Con_Printf( "SV_ProcessFile:  Downloaded %i bytes for purported %i byte file\n",
				client->netchan.tempbuffersize, resource->nDownloadSize );
			return;
		}
		HPAK_AddLump( true, "custom.hpk", resource, client->netchan.tempbuffer, nullptr );
		resource->ucFlags = static_cast<unsigned char>( resource->ucFlags & ~RES_WASMISSING );
		SV_MoveToOnHandList( resource );
		for ( customization_t* existing = client->customdata.pNext; existing; existing = existing->pNext )
		{
			if ( Q_memcmp( existing->resource.rgucMD5_hash, resource->rgucMD5_hash, sizeof( hash ) ) == 0 )
			{
				Con_DPrintf( "Duplicate resource received and ignored.\n" );
				return;
			}
		}
		if ( !COM_CreateCustomization(
				 &client->customdata, resource, CUSTOMIZATION_PLAYER_NONE, CUSTOMIZATION_FLAGS, nullptr, nullptr ) )
			Con_Printf( "Error parsing custom decal from %s\n", client->name );
		return;
	}
	Con_Printf( "SV_ProcessFile:  Unrequested decal\n" );
}

extern qboolean noip;

int SVC_GameDllQuery( const char* query )
{
	if ( !sv.active || svs.maxclients <= 1 )
		return 0;

	constexpr int QUERY_RESPONSE_BYTES = 2048;
	constexpr int QUERY_PAYLOAD_BYTES = QUERY_RESPONSE_BYTES - static_cast<int>( sizeof( int ) );
	struct
	{
		int marker;
		char payload[QUERY_PAYLOAD_BYTES];
	} reply;
	Q_memset( &reply, 0, sizeof( reply ) );
	int length = QUERY_PAYLOAD_BYTES;
	int result = gEntityInterface.pfnConnectionlessPacket( &net_from, query, reply.payload, &length );
	if ( length != 0 && length <= QUERY_PAYLOAD_BYTES )
	{
		reply.marker = -1;
		int packetSize = length + static_cast<int>( sizeof( reply.marker ) );
		NET_SendPacket( NS_SERVER, packetSize, &reply, net_from );
	}
	return result;
}

void TRACE_DELTA( char*, ... )
{
}

void SV_GetNetInfo( client_t* client, int* ping, int* loss )
{
	constexpr int NETINFO_SAMPLE_LIMIT = 16;
	constexpr double NETINFO_REFRESH_INTERVAL = 2.0;
	constexpr float NETINFO_MILLISECONDS = 1000.0f;
	static unsigned short lastPing[MAX_CLIENTS];
	static byte lastLoss[MAX_CLIENTS];

	int cacheIndex = static_cast<int>( client - svs.clients );
	if ( realtime < client->nextping )
	{
		*ping = lastPing[cacheIndex];
		*loss = lastLoss[cacheIndex];
		return;
	}

	client->nextping = realtime + NETINFO_REFRESH_INTERVAL;
	unsigned short measuredPing = 0;
	int frameCount = SV_UPDATE_BACKUP <= 31 ? SV_UPDATE_BACKUP / 2 : NETINFO_SAMPLE_LIMIT;
	if ( !client->fakeclient && frameCount > 0 )
	{
		unsigned int sequence = static_cast<unsigned int>( client->netchan.incoming_acknowledged ) - 1u;
		float total = 0.0f;
		int samples = 0;
		for ( int frameIndex = 0; frameIndex < frameCount; ++frameIndex, --sequence )
		{
			float sample = client->frames[sequence & static_cast<unsigned int>( SV_UPDATE_MASK )].ping_time;
			if ( sample > 0.0f )
			{
				total += sample;
				++samples;
			}
		}
		if ( samples )
		{
			float average = total / static_cast<float>( samples );
			if ( average >= 0.0f )
				measuredPing = static_cast<unsigned short>( static_cast<int>( average * NETINFO_MILLISECONDS ) );
		}
	}

	int packetLoss = static_cast<int>( client->packet_loss );
	lastPing[cacheIndex] = measuredPing;
	lastLoss[cacheIndex] = static_cast<byte>( packetLoss );
	*ping = measuredPing;
	*loss = lastLoss[cacheIndex];
}

void SV_GatherStatistics()
{
	constexpr double STATISTICS_CLEAR_INTERVAL = 3600.0;
	constexpr double STATISTICS_SAMPLE_INTERVAL = 60.0;

	if ( realtime > svs.next_cleartime )
	{
		int users = 0;
		client_t* client = svs.clients;
		for ( int index = 0; index < svs.maxclients; ++index, ++client )
		{
			if ( client->active || client->spawned || client->connected )
				++users;
		}
		svs.next_cleartime = realtime + STATISTICS_CLEAR_INTERVAL;
		Q_memset( &svs.stats, 0, sizeof( svs.stats ) );
		svs.stats.maxusers = users;
		svs.stats.minusers = users;
		return;
	}

	if ( svs.next_sampletime > realtime )
		return;

	int samples = ++svs.stats.num_samples;
	svs.next_sampletime = realtime + STATISTICS_SAMPLE_INTERVAL;
	int users = 0;
	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		if ( client->active || client->spawned || client->connected )
			++users;
	}

	float occupancySample =
		static_cast<float>( static_cast<double>( users ) * 100.0 / static_cast<double>( svs.maxclients ) );
	svs.stats.cumulative_occupancy += occupancySample;
	if ( samples > 0 )
		svs.stats.occupancy = svs.stats.cumulative_occupancy / static_cast<float>( samples );
	if ( svs.stats.minusers > users )
		svs.stats.minusers = users;
	else if ( svs.stats.maxusers < users )
		svs.stats.maxusers = users;
	if ( svs.maxclients - 1 <= users )
		++svs.stats.at_capacity;
	if ( users <= 1 )
		++svs.stats.at_empty;
	if ( samples > 0 )
	{
		float sampleCount = static_cast<float>( samples );
		svs.stats.capacity_percent = static_cast<float>( svs.stats.at_capacity ) * 100.0f / sampleCount;
		svs.stats.empty_percent = 100.0f * static_cast<float>( svs.stats.at_empty ) / sampleCount;
	}

	float latency = 0.0f;
	if ( svs.maxclients > 0 )
	{
		float total = 0.0f;
		int counted = 0;
		client = svs.clients;
		for ( int index = 0; index < svs.maxclients; ++index, ++client )
		{
			if ( client->active && !client->fakeclient )
			{
				total += client->latency;
				++counted;
			}
		}
		if ( counted )
			latency = total / static_cast<float>( counted );
	}
	svs.stats.cumulative_latency += latency;
	if ( samples > 0 )
		svs.stats.average_latency = svs.stats.cumulative_latency / static_cast<float>( samples );
	if ( svs.stats.num_sessions > 0 )
		svs.stats.average_session_len = svs.stats.cumulative_sessiontime / static_cast<float>( svs.stats.num_sessions );
}

void SV_QueryMovevarsChanged()
{
	if ( sv_maxspeed.value == movevars.maxspeed && sv_gravity.value == movevars.gravity
		 && sv_stopspeed.value == movevars.stopspeed && sv_spectatormaxspeed.value == movevars.spectatormaxspeed
		 && sv_accelerate.value == movevars.accelerate && sv_airaccelerate.value == movevars.airaccelerate
		 && sv_wateraccelerate.value == movevars.wateraccelerate && sv_friction.value == movevars.friction
		 && sv_edgefriction.value == movevars.edgefriction && sv_waterfriction.value == movevars.waterfriction
		 && movevars.entgravity == 1.0f && sv_bounce.value == movevars.bounce && sv_stepsize.value == movevars.stepsize
		 && sv_maxvelocity.value == movevars.maxvelocity && sv_zmax.value == movevars.zmax
		 && sv_wateramp.value == movevars.waveHeight && sv_footsteps.value == static_cast<float>( movevars.footsteps )
		 && sv_rollangle.value == movevars.rollangle && sv_rollspeed.value == movevars.rollspeed
		 && sv_skycolor_r.value == movevars.skycolor_r && sv_skycolor_g.value == movevars.skycolor_g
		 && sv_skycolor_b.value == movevars.skycolor_b && sv_skyvec_x.value == movevars.skyvec_x
		 && sv_skyvec_y.value == movevars.skyvec_y && sv_skyvec_z.value == movevars.skyvec_z
		 && Q_strcmp( sv_skyname.string, movevars.skyName ) == 0 )
		return;

	SV_SetMoveVars();
	for ( int index = 0; index < svs.maxclients; ++index )
	{
		client_t* client = &svs.clients[index];
		if ( client->fakeclient )
			continue;
		if ( !client->active && !client->spawned && !client->connected )
			continue;
		SV_WriteMovevarsToClient( &client->netchan.message );
	}
}

int SV_CheckForDuplicateNames( char* userinfo, qboolean reconnecting, int excludeSlot )
{
	constexpr int DUPLICATE_NAME_PRECISION = MAX_PLAYERNAME - 4;
	const char* name = Info_ValueForKey( userinfo, "name" );
	if ( !name || !name[0] || Q_stricmp( name, "console" ) == 0 || Q_strstr( name, ".." ) || Q_strstr( name, "\"" )
		 || Q_strstr( name, "\\" ) )
	{
		Info_SetValueForKey( userinfo, "name", "unamed", MAX_INFO_STRING );
		return 1;
	}

	int changed = 0;
	int duplicateNumber = 1;
	const char* candidate = name;
	for ( int slot = 0; slot < MAX_CLIENTS; ++slot )
	{
		if ( ( excludeSlot != slot || !reconnecting ) && svs.clients[slot].connected
			 && Q_stricmp( svs.clients[slot].name, candidate ) == 0 )
		{
			if ( candidate[0] == '(' )
			{
				if ( candidate[2] == ')' )
					candidate += 3;
				else if ( candidate[3] == ')' )
					candidate += 4;
			}
			char duplicateName[MAX_PLAYERNAME];
			_snprintf( duplicateName, sizeof( duplicateName ), "(%d)%-0.*s", duplicateNumber,
				DUPLICATE_NAME_PRECISION, candidate );
			++duplicateNumber;
			Info_SetValueForKey( userinfo, "name", duplicateName, MAX_INFO_STRING );
			candidate = Info_ValueForKey( userinfo, "name" );
			changed = 1;
			slot = -1;
		}
	}
	return changed;
}

void SVC_InfoString()
{
	constexpr int INFO_PACKET_BYTES = 1400;
	constexpr int INFO_STRING_BYTES = 2048;
	constexpr int INFO_ADDRESS_BYTES = 256;
	constexpr int INFO_DIRECTORY_BYTES = 260;
	if ( noip || !sv.active || svs.maxclients <= 1 )
		return;

	int proxies = 0;
	int players = 0;
	for ( client_t* client = svs.clients; client != &svs.clients[svs.maxclients]; ++client )
	{
		if ( client->active || client->spawned || client->connected )
		{
			if ( client->proxy )
				++proxies;
			++players;
		}
	}

	char address[INFO_ADDRESS_BYTES];
	address[0] = 0;
	Q_strncpy( address, NET_AdrToString( net_local_adr ), INFO_ADDRESS_BYTES - 1 );
	address[INFO_ADDRESS_BYTES - 1] = 0;

	qboolean hasPassword = false;
	if ( sv_password.string[0] )
		hasPassword = Q_stricmp( sv_password.string, "none" ) != 0;

	char info[INFO_STRING_BYTES];
	info[0] = 0;
	Info_SetValueForKey( info, "protocol", va( "%i", PROTOCOL_VERSION ), INFO_STRING_BYTES );
	Info_SetValueForKey( info, "address", address, INFO_STRING_BYTES );
	// Actual players
	Info_SetValueForKey( info, "players", va( "%i", players ), INFO_STRING_BYTES );
	Info_SetValueForKey( info, "proxytarget", va( "%i", proxies ), INFO_STRING_BYTES );
	Info_SetValueForKey( info, "lan", va( "%i", Master_IsLanGame() != 0 ), INFO_STRING_BYTES );
	int visiblePlayers = static_cast<int>( sv_visiblemaxplayers.value );
	if ( visiblePlayers < 0 )
		visiblePlayers = svs.maxclients;
	Info_SetValueForKey( info, "max", va( "%i", visiblePlayers ), INFO_STRING_BYTES );

	int bots = 0;
	for ( int index = 0; index < svs.maxclients; ++index )
	{
		if ( svs.clients[index].fakeclient )
			++bots;
	}
	Info_SetValueForKey( info, "bots", va( "%i", bots ), INFO_STRING_BYTES );
	char gameDirectory[INFO_DIRECTORY_BYTES];
	COM_FileBase( com_gamedir, gameDirectory );
	Info_SetValueForKey( info, "gamedir", gameDirectory, INFO_STRING_BYTES );
	Info_SetValueForKey( info, "description", gEntityInterface.pfnGetGameDescription(), INFO_STRING_BYTES );
	Info_SetValueForKey( info, "hostname", Cvar_VariableString( "hostname" ), INFO_STRING_BYTES );
	Info_SetValueForKey( info, "map", sv.name, INFO_STRING_BYTES );
	Info_SetValueForKey( info, "type", cls.state == ca_dedicated ? "d" : "l", INFO_STRING_BYTES );
	Info_SetValueForKey( info, "password", va( "%i", hasPassword ), INFO_STRING_BYTES );
	Info_SetValueForKey( info, "os", "l", INFO_STRING_BYTES );
	Info_SetValueForKey( info, "secure", Steam_GSBSecure() ? "1" : "0", INFO_STRING_BYTES );
	if ( gmodinfo.bIsMod )
	{
		Info_SetValueForKey( info, "mod", va( "%i", 1 ), INFO_STRING_BYTES );
		Info_SetValueForKey( info, "modversion", va( "%i", gmodinfo.version ), INFO_STRING_BYTES );
		Info_SetValueForKey( info, "svonly", va( "%i", gmodinfo.svonly ), INFO_STRING_BYTES );
		Info_SetValueForKey( info, "cldll", va( "%i", gmodinfo.cldll ), INFO_STRING_BYTES );
	}

	byte data[INFO_PACKET_BYTES];
	sizebuf_t message;
	message.maxsize = INFO_PACKET_BYTES;
	message.cursize = 0;
	message.data = data;
	message.buffername = "SVC_InfoString";
	message.flags = FSB_ALLOWOVERFLOW;
	MSG_WriteLong( &message, -1 );
	MSG_WriteString( &message, "infostringresponse" );
	MSG_WriteString( &message, info );
	NET_SendPacket( NS_SERVER, message.cursize, message.data, net_from );
}

void SVC_Info( qboolean detailed )
{
	constexpr int INFO_PACKET_BYTES = 1400;
	constexpr int INFO_DIRECTORY_BYTES = 260;
	if ( !sv.active || svs.maxclients <= 1 )
		return;

	int players = 0;
	for ( client_t* client = svs.clients; client != &svs.clients[svs.maxclients]; ++client )
	{
		if ( client->active || client->spawned || client->connected )
			++players;
	}

	byte data[INFO_PACKET_BYTES];
	sizebuf_t message;
	message.maxsize = INFO_PACKET_BYTES;
	message.cursize = 0;
	message.data = data;
	message.buffername = "SVC_Info";
	message.flags = FSB_ALLOWOVERFLOW;
	MSG_WriteLong( &message, -1 );
	MSG_WriteByte( &message, detailed ? 'm' : 'C' );
	if ( noip ) // Always try to send the IP address
		MSG_WriteString( &message, "LOOPBACK" );
	else
		MSG_WriteString( &message, NET_AdrToString( net_local_adr ) );
	MSG_WriteString( &message, Cvar_VariableString( "hostname" ) );
	MSG_WriteString( &message, sv.name );
	char gameDirectory[INFO_DIRECTORY_BYTES];
	COM_FileBase( com_gamedir, gameDirectory );
	MSG_WriteString( &message, gameDirectory );
	MSG_WriteString( &message, gEntityInterface.pfnGetGameDescription() );
	MSG_WriteByte( &message, players );
	int visiblePlayers = static_cast<int>( sv_visiblemaxplayers.value );
	if ( visiblePlayers < 0 )
		visiblePlayers = svs.maxclients;
	MSG_WriteByte( &message, visiblePlayers );
	MSG_WriteByte( &message, PROTOCOL_VERSION );
	if ( !detailed )
	{
		NET_SendPacket( NS_SERVER, message.cursize, message.data, net_from );
		return;
	}

	MSG_WriteByte( &message, cls.state == ca_dedicated ? 'd' : 'l' );
	MSG_WriteByte( &message, 'l' );
	if ( Q_strlen( sv_password.string ) > 0 && Q_stricmp( sv_password.string, "none" ) != 0 )
		MSG_WriteByte( &message, 1 );
	else
		MSG_WriteByte( &message, 0 );
	if ( gmodinfo.bIsMod )
	{
		MSG_WriteByte( &message, 1 );
		MSG_WriteString( &message, gmodinfo.szInfo );
		MSG_WriteString( &message, gmodinfo.szDL );
		MSG_WriteString( &message, com_empty_string );
		MSG_WriteLong( &message, gmodinfo.version );
		MSG_WriteLong( &message, gmodinfo.size );
		MSG_WriteByte( &message, gmodinfo.svonly );
		MSG_WriteByte( &message, gmodinfo.cldll );
	}
	else
		MSG_WriteByte( &message, 0 );
	MSG_WriteByte( &message, Steam_GSBSecure() != 0 );
	int bots = 0;
	for ( int index = 0; index < svs.maxclients; ++index )
	{
		if ( svs.clients[index].fakeclient )
			++bots;
	}
	MSG_WriteByte( &message, bots );
	NET_SendPacket( NS_SERVER, message.cursize, message.data, net_from );
}

void SVC_PlayerInfo()
{
	constexpr int PLAYER_INFO_BYTES = 2048;
	if ( !sv.active || svs.maxclients <= 1 )
		return;

	byte data[PLAYER_INFO_BYTES];
	sizebuf_t message;
	message.buffername = "SVC_PlayerInfo";
	message.maxsize = PLAYER_INFO_BYTES;
	message.cursize = 0;
	message.data = data;
	message.flags = FSB_ALLOWOVERFLOW;
	MSG_WriteLong( &message, -1 );
	MSG_WriteByte( &message, 'D' );

	int count = 0;
	for ( int index = 0; index < svs.maxclients; ++index )
	{
		if ( svs.clients[index].active )
			++count;
	}
	MSG_WriteByte( &message, count );

	int playerNumber = 0;
	for ( int index = 0; index < svs.maxclients; ++index )
	{
		client_t* client = &svs.clients[index];
		if ( !client->active )
			continue;
		++playerNumber;
		MSG_WriteByte( &message, playerNumber );
		MSG_WriteString( &message, client->name );
		MSG_WriteLong( &message, static_cast<int>( client->edict->v.frags ) );
		MSG_WriteFloat( &message, static_cast<float>( realtime - client->netchan.connect_time ) );
	}
	NET_SendPacket( NS_SERVER, message.cursize, message.data, net_from );
}

qboolean SV_SendClientDatagram( client_t* client )
{
	byte data[SERVER_DATAGRAM_SIZE];
	sizebuf_t message;
	message.buffername = "Client Datagram";
	message.data = data;
	message.maxsize = sizeof( data );
	message.cursize = 0;
	message.flags = FSB_ALLOWOVERFLOW;
	MSG_WriteByte( &message, svc_time );
	MSG_WriteFloat( &message, static_cast<float>( sv.time ) );
	SV_WriteClientdataToMessage( client, &message );
	SV_WriteEntitiesToClient( client, &message );
	if ( client->datagram.flags & FSB_OVERFLOWED )
		Con_Printf( "WARNING: datagram overflowed for %s\n", client->name );
	else
		SZ_Write( &message, client->datagram.data, client->datagram.cursize );
	SZ_Clear( &client->datagram );
	if ( message.flags & FSB_OVERFLOWED )
	{
		Con_Printf( "WARNING: msg overflowed for %s\n", client->name );
		SZ_Clear( &message );
	}
	Netchan_Transmit( &client->netchan, message.cursize, data );
	return true;
}

void SV_CheckForRcon()
{
	constexpr int ENGINE_CLOSE = 3;
	if ( sv.active || cls.state != ca_dedicated || giActive == ENGINE_CLOSE || !host_initialized )
		return;

	while ( NET_GetPacket( NS_SERVER ) )
	{
		if ( SV_FilterPacket() )
		{
			SV_SendBan();
#if defined( _WIN32 ) && !defined( SWDS_DLL )
			NET_CheckCleanupFakeIPConnection( NS_SERVER, &net_from );
#endif
			continue;
		}
		if ( *reinterpret_cast<int*>( net_message.data ) != -1 )
			continue;

		MSG_BeginReading();
		MSG_ReadLong();
		char* text = MSG_ReadStringLine();
		Cmd_TokenizeString( text );
		const char* command = Cmd_Argv( 0 );
		if ( Q_strcmp( command, "getchallenge" ) == 0 )
			SVC_GetChallenge();
		else if ( Q_stricmp( command, "challenge" ) == 0 )
			SVC_ServiceChallenge();
		else if ( Q_strcmp( command, "rcon" ) == 0 )
			SV_Rcon( &net_from );
	}
}

void SV_KickPlayer( int playerSlot, int )
{
	constexpr int CHEAT_NOTICE_BYTES = 1024;
	if ( playerSlot < 0 || playerSlot >= svs.maxclients )
		return;

	client_t* client = &svs.clients[playerSlot];
	if ( !client->connected || !svs.isSecure )
		return;

	USERID_t id = client->network_userid;
	Log_Printf( "Secure: \"%s<%i><%s><>\" was detected cheating and dropped from the server.\n", client->name,
		client->userid, SV_GetIDString( &id ) );

	const char* explanation = "********************************************\n"
							  "You have been automatically disconnected\n"
							  "from this secure server because an illegal\n"
							  "cheat was detected on your computer.\n"
							  "Repeat violators may be permanently banned\n"
							  "from all secure servers.\n"
							  "\n"
							  "For help cleaning your system of cheats, visit:\n"
							  "http://www.counter-strike.net/cheat.html\n"
							  "********************************************\n"
							  "\n";
	byte notice[CHEAT_NOTICE_BYTES];
	notice[0] = svc_print;
	notice[1] = '\n';
	Q_strcpy( reinterpret_cast<char*>( notice + 2 ), explanation );
	Netchan_Transmit( &client->netchan, Q_strlen( reinterpret_cast<char*>( notice ) ) + 1, notice );

	std::snprintf( reinterpret_cast<char*>( notice ), sizeof( notice ),
		"%s was automatically disconnected\nfrom this secure server.\n", svs.clients[playerSlot].name );
	int maxclients = svs.maxclients;
	for ( int index = 0; index < maxclients; ++index )
	{
		client_t* other = &svs.clients[index];
		if ( ( other->active || other->spawned ) && !other->fakeclient )
		{
			MSG_WriteByte( &other->netchan.message, svc_centerprint );
			MSG_WriteString( &other->netchan.message, reinterpret_cast<char*>( notice ) );
			maxclients = svs.maxclients;
		}
	}
	SV_DropClient( &svs.clients[playerSlot], false, "Automatically dropped by cheat detector" );
}

void SV_ReadPackets()
{
	while ( NET_GetPacket( NS_SERVER ) )
	{
		if ( SV_FilterPacket() )
		{
			SV_SendBan(); // tell them we aren't listening...
#if defined( _WIN32 ) && !defined( SWDS_DLL )
			NET_CheckCleanupFakeIPConnection( NS_SERVER, &net_from );
#endif
			continue;
		}

		if ( *reinterpret_cast<int*>( net_message.data ) == -1 )
		{
			if ( CheckIP( net_from ) )
			{
				unsigned int addressBytes;
				std::memcpy( &addressBytes, net_from.ip, sizeof( addressBytes ) );
				unsigned int fromIp = ( addressBytes << 24 ) | ( ( addressBytes << 8 ) & 0x00FF0000u )
									  | ( ( addressBytes >> 8 ) & 0x0000FF00u ) | ( addressBytes >> 24 );
				unsigned short port = static_cast<unsigned short>( ( net_from.port << 8 ) | ( net_from.port >> 8 ) );
				Steam_HandleIncomingPacket( net_message.data, net_message.cursize, fromIp, port );
				SV_ConnectionlessPacket();
			}
			else if ( sv_logblocks.value != 0.0f )
				Log_Printf( "Traffic from %s was blocked for exceeding rate limits\n", NET_AdrToString( net_from ) );
			continue;
		}

		int maxclients = svs.maxclients;
		client_t* client = svs.clients;
		for ( int index = 0; index < maxclients; ++index, ++client )
		{
			if ( !client->connected && !client->active && !client->spawned )
				continue;
			if ( !NET_CompareAdr( net_from, client->netchan.remote_address ) )
				continue;
			if ( Netchan_Process( &client->netchan ) )
			{
				if ( svs.maxclients == 1 || !client->active || !client->spawned || !client->fully_connected )
					client->send_message = 1;
				SV_ExecuteClientMessage( client );
				gGlobalVariables.frametime = static_cast<float>( host_frametime );
			}
			// Fragmentation/reassembly sending takes priority over all game messages, want this in the future?
			if ( Netchan_IncomingReady( &client->netchan ) )
			{
				if ( Netchan_CopyNormalFragments( &client->netchan ) )
				{
					MSG_BeginReading();
					SV_ExecuteClientMessage( client );
				}
				if ( Netchan_CopyFileFragments( &client->netchan ) )
				{
					host_client = client;
					SV_ProcessFile( client, client->netchan.incomingfilename );
				}
			}
			maxclients = svs.maxclients;
		}
	}
}

void SV_Frame()
{
	if ( !sv.active )
		return;
	sv.oldtime = sv.time;
	gGlobalVariables.frametime = static_cast<float>( host_frametime );
	SV_CheckCmdTimes();
	SV_ReadPackets();
	if ( SV_IsSimulating() )
	{
		SV_Physics();
		sv.time += host_frametime;
	}
	SV_QueryMovevarsChanged();
	SV_RequestMissingResourcesFromClients();
	SV_CheckTimeouts();
	SV_SendClientMessages();
	SV_CheckMapDifferences();
	SV_GatherStatistics();
	Steam_RunFrame();
}
