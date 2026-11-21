#pragma once

#include "net.h"
#include "filesystem.h"
#include "engine_apiproxy.h"
#include "crc.h"
#include "../common/cvardef.h"
#include "custom.h"
#include "../common/screenfade.h"
#include "../common/usercmd.h"
#include "../common/dlight.h"

#include <cstdint>

#define SIGNONS 2 // signon messages to receive before connected

//
// stats are integers communicated to the client by the server
//
#define STAT_HEALTH 0
#define STAT_WEAPON 2

#define MAX_EFRAGS 640
#define MAX_DLIGHTS 32
#define MAX_ELIGHTS 64 // entity only point lights

#define MAX_LIGHTSTYLES 64
#define MAX_STYLESTRING 64

#define MAX_VISEDICTS 512
#define MAX_BEAMENTS 64

#define CLIENT_DATAGRAM_SIZE 6000

#define MAX_DL_STATS 8
#define MAX_DEMOS 32
#define MAX_DEMONAME 16
#define MAX_INFO_STRING 256
#define MAX_MAPSTRING 64
#define MAX_SPAWNPARMS 2048
#define MAX_RESOURCES 1280
#define MAX_CL_STATS 32
#define UPDATE_BACKUP 64
#define MAX_CLIENTS 32
#define MAX_EVENT_SCRIPTS 256
#define MAX_MODELS 512
#define MAX_SOUNDS 512
#define MAX_CONSISTENCY_FILES 512
#define MAX_LEVELNAME 40
#define MAX_PLAYERNAME 32
#define MAX_SERVERINFO_STRING 512
#define PACKET_ENTITY_FLAG_BYTES 128
#define MAX_WEAPON_DATA 64
#define MAX_INSTANCED_BASELINES 64
#define MAX_DOWNLOAD_URL 128
#define HASHED_CDKEY_BYTES 16

#define MESSAGE_STAT_BUCKETS 64

struct sfx_s;
struct physent_s;

typedef struct overviewInfo_s
{
	vec3_t origin;
	float z_min;
	float z_max;
	float zoom;
	qboolean rotated;
} overviewInfo_t;

typedef struct
{
	int length;
	char map[MAX_STYLESTRING];
} lightstyle_t;

typedef enum
{
	CAREER_NONE,
	CAREER_LOADING,
	CAREER_PLAYING
} CareerStateType;

enum cactive_t
{
	ca_dedicated,	  // a dedicated server with no ability to start a client
	ca_disconnected,  // full screen console with no connection
	ca_connecting,	  // Challenge requested, waiting for response or to resend connection request.
	ca_connected,	  // valid netcon, talking to a server, waiting for server data
	ca_uninitialized,
	ca_active		  // d/l complete, ready game views should be displayed
};

typedef struct downloadtime_s
{
	qboolean bUsed;
	float fTime;
	int nBytesRemaining;
} downloadtime_t;

typedef struct incomingtransfer_s
{
	qboolean doneregistering;
	int percent;
	qboolean downloadrequested;
	downloadtime_t rgStats[MAX_DL_STATS];
	int nCurStat;
	int nTotalSize;
	int nTotalToTransfer;
	int nRemainingToTransfer;
	float fLastStatusUpdate;
	qboolean custom;
} incomingtransfer_t;

typedef struct soundfade_s
{
	int nStartPercent;
	int nClientSoundFadePercent;
	double soundFadeStartTime;
	int soundFadeOutTime;
	int soundFadeHoldTime;
	int soundFadeInTime;
} soundfade_t;

//
// the client_static_t structure is persistant through an arbitrary number
// of server connections
//
typedef struct client_static_s
{
	// connection information
	cactive_t state; // Current state.
// Our sequenced channel to the remote server.
	netchan_t netchan;

	sizebuf_t datagram;
	byte datagram_buf[CLIENT_DATAGRAM_SIZE];

	double connect_time; // If gap of connect_time to realtime > 3000, then resend connect packet
	int connect_retry;
	int challenge;
	byte authprotocol;
	int userid;
	char trueaddress[32];
	float slist_time;

	int signon; // 0 to SIGNONS

	char servername[MAX_PATH]; // name of server from original connect
// personalization data sent to server	
	char mapstring[MAX_MAPSTRING];
	char spawnparms[MAX_SPAWNPARMS];

	// private userinfo for sending to masterless servers
	// personalization data sent to server	
	char userinfo[MAX_INFO_STRING];

	// When can we send the next command packet?
	float nextcmdtime;
	// Sequence number of last outgoing command
	int lastoutgoingcommand;

	// demo loop control
	int demonum; // -1 = don't play demos
	char demos[MAX_DEMOS][MAX_DEMONAME]; // when not playing

	// demo recording info must be here, because record is started before
	// entering a map (and clearing client_state_t)
	qboolean demorecording;
	qboolean demoplayback;
	qboolean timedemo;
	float demostarttime;
	int demostartframe;
	int forcetrack; // -1 = use normal cd track
	FileHandle_t demofile;
	FileHandle_t demoheader;
	qboolean demowaiting;
	qboolean demoappending;
	char demofilename[MAX_PATH];
	int demoframecount;

	int td_lastframe;
	int td_startframe; // host_framecount at start
	float td_starttime; // realtime at second frame of timedemo

	incomingtransfer_t dl;

	float packet_loss;
	double packet_loss_recalc_time;

	int playerbits;

	soundfade_t soundfade;

	char physinfo[MAX_INFO_STRING];

	byte md5_clientdll[MD5_DIGEST_BYTES];

	netadr_t game_stream;
	netadr_t connect_stream;

	qboolean passive;
	qboolean spectator;
	qboolean director;
	qboolean fSecureClient;
	qboolean isVAC2Secure;

	uint64_t GameServerSteamID;

	int build_num;
} client_static_t;

typedef struct packet_entities_s
{
	int num_entities;
	byte flags[PACKET_ENTITY_FLAG_BYTES];
	entity_state_t* entities;
} packet_entities_t;

typedef struct frame_s
{
	double receivedtime; // time message was received, or -1
	double latency;
	qboolean invalid; // true if the packet_entities delta was invalid
	qboolean choked;

	entity_state_t playerstate[MAX_CLIENTS]; // message received that reflects performing

	double time;
	clientdata_t clientdata;
	weapon_data_t weapondata[MAX_WEAPON_DATA];
	packet_entities_t packet_entities;

	unsigned short clientbytes;
	unsigned short playerinfobytes;
	unsigned short packetentitybytes;
	unsigned short tentitybytes;
	unsigned short soundbytes;
	unsigned short eventbytes;
	unsigned short usrbytes;
	unsigned short voicebytes;
	unsigned short msgbytes;
} frame_t;

typedef struct cmd_s
{
	usercmd_t cmd;
	float senttime;
	float receivedtime;
	float frame_lerp;
	qboolean processedfuncs;
	qboolean heldback;
	int sendsize;
} cmd_t;

typedef struct event_s
{
	unsigned short index;
	const char* filename;
	int filesize;
	const char* pszScript;
} event_t;

typedef struct consistency_s
{
	char* filename;
	int issound;
	int orig_index;
	int value;
	int check_type;
	vec3_t mins;
	vec3_t maxs;
} consistency_t;

typedef struct player_info_s
{
	// User id on server
	int userid;

	// User info string
	char userinfo[MAX_INFO_STRING];
	// Name
	char name[MAX_PLAYERNAME];

	// Spectator or not, unused
	int spectator;
	int ping;
	int packet_loss;

	// skin information
	char model[MAX_QPATH];
	int topcolor;
	int bottomcolor;

	// last frame rendered
	int renderframe;

	// Gait frame estimation
	int gaitsequence;
	float gaitframe;
	float gaityaw;
	vec3_t prevgaitorigin;

	customization_t customdata;

	char hashedcdkey[HASHED_CDKEY_BYTES];

	uint64_t m_nSteamID;
} player_info_t;

//
// the client_state_t structure is wiped completely at every
// server signon
//
typedef struct client_state_s
{
	int max_edicts;

	resource_t resourcesonhand;
	resource_t resourcesneeded;
	resource_t resourcelist[MAX_RESOURCES];
	int num_resources;

	qboolean need_force_consistency_response;

	char serverinfo[MAX_SERVERINFO_STRING];

	int servercount; // server identification for prespawns
	int validsequence;
	int parsecount;
	int parsecountmod;

	int stats[MAX_CL_STATS]; // health, etc
	int weapons;

	// throw out the first couple, so the player
	// doesn't accidentally do something the 
	// first frame
	usercmd_t cmd; // last command sent to the server

	vec3_t viewangles;
	vec3_t punchangle;
	vec3_t crosshairangle;

	// is rendering at.  allways <= realtime
	vec3_t simorg;
	vec3_t simvel;
	vec3_t simangles;

	vec3_t predicted_origins[UPDATE_BACKUP];
	vec3_t prediction_error;

	float idealpitch;
	vec3_t viewheight;

	screenfade_t sf;

	qboolean paused; // send over by server
	int onground;
	int moving;
	int waterlevel;
	int usehull;
	float maxspeed;
	int pushmsec;
	int light_level;
	int intermission; // don't change view angle, full screen, etc

	double mtime[2]; // the timestamp of last two messages
	double time; // clients view of time, should be between
								// servertime and oldservertime to generate
								// a lerp point for other data
	double oldtime; // previous cl.time, time-oldtime is used
								// to decay light values and smooth step ups

	frame_t frames[UPDATE_BACKUP];
	cmd_t commands[UPDATE_BACKUP];
	local_state_t predicted_frames[UPDATE_BACKUP];
	int delta_sequence;

	int playernum;
	event_t event_precache[MAX_EVENT_SCRIPTS];
	model_s* model_precache[MAX_MODELS];
	int model_precache_count;
	sfx_s* sound_precache[MAX_SOUNDS];
	consistency_t consistency_list[MAX_CONSISTENCY_FILES];
	int num_consistency;

	int highentity;

	char levelname[MAX_LEVELNAME]; // for display on solo scoreboard
	int maxclients;
	int gametype;

	// refresh related state
	int viewentity;
	model_s* worldmodel; // cl_entitites[0].model
	efrag_s* free_efrags;
	int num_entities; // held in cl_entities array
	int num_statics;  // held in cl_staticentities array
	cl_entity_t viewent;

	int cdtrack;
	int looptrack;

	CRC32_t serverCRC;
	byte clientdllmd5[MD5_DIGEST_BYTES];

	float weaponstarttime;
	int weaponsequence;

	qboolean fPrecaching;
	dlight_t* pLight;
	player_info_t players[MAX_CLIENTS];

	entity_state_t instanced_baseline[MAX_INSTANCED_BASELINES];
	int instanced_baseline_number;

	CRC32_t mapCRC;
	event_state_t events;
	char downloadUrl[MAX_DOWNLOAD_URL];
} client_state_t;

//
// cvars
//
extern cvar_t scr_downloading;
extern cvar_t cl_lw;
extern cvar_t fs_startup_timings;
extern cvar_t cl_filterstuffcmd;
extern cvar_t cl_allow_download;
extern cvar_t cl_download_ingame;

extern client_static_t cls;
extern client_state_t cl;

extern efrag_t cl_efrags[MAX_EFRAGS];
extern dlight_t cl_dlights[MAX_DLIGHTS];
extern dlight_t cl_elights[MAX_ELIGHTS];
extern lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];

extern cl_entity_t* cl_entities;

inline cl_entity_t* CL_GetEntityIfValid( int index )
{
	return cl_entities && index >= 0 && index < cl.max_edicts ? &cl_entities[index] : NULL;
}

extern int cl_numvisedicts;
extern cl_entity_t* cl_visedicts[MAX_VISEDICTS];
extern int cl_numbeamentities;
extern cl_entity_t* cl_beamentities[MAX_BEAMENTS];

extern overviewInfo_t gDevOverview;
extern CareerStateType g_careerState;

extern float scr_fov_value;
extern int CL_UPDATE_MASK;
extern int CL_UPDATE_BACKUP;
extern qboolean noclip_anglehack;
extern int g_bRedirectedToProxy;

extern int msg_buckets[MESSAGE_STAT_BUCKETS];
extern int total_data[MESSAGE_STAT_BUCKETS];

// cl_main.cpp
//
void		CL_ClearState( qboolean inGame );
void		CL_ClearClientState();
void		CL_ReallocateDynamicData( int maxClients );
void		CL_DeallocateDynamicData();
void		CL_Disconnect();
void		CL_Disconnect_f();
void		CL_Restore( const char* filename );
void		CL_ProcessFile( qboolean success, const char* filename );
void		CL_HudMessage( const char* message );
void		CL_SignonReply();
void		CL_Move();
dlight_t*	CL_AllocDlight( int key );
dlight_t*	CL_AllocElight( int key );
int			CL_GetFragmentSize( void* status );
char*		CL_GetCDKeyHash();
qboolean	CL_IsSpectateOnly();
qboolean	CL_IsDevOverviewMode();
void		CL_InitSpectator();
void		CL_GetPlayerHulls();
void		CL_RecordHUDCommand( char* commandName );
void		CL_WriteClientDLLMessage( int size, byte* buffer );
void		AddStartupTiming( const char* name );
void		SPR_Shutdown_NoModelFree();
void		CloseSecurityModule();
qboolean	ValidStuffText( const char* text );
int			DispatchDirectUserMsg( const char* name, int size, void* buffer );

//
// cl_parse.cpp
//
void		CL_ParseServerMessage( qboolean normalMessage );
void		CL_ParseServerInfo();
char*		CL_MsgInfo( int message );
model_s*	CL_GetModelByIndex( int index );

void		CL_ParseResourceList();
void		CL_ParseResourceLocation();
void		CL_ParseConsistencyInfo();
void		CL_SendConsistencyInfo( sizebuf_t* message );
void		CL_BatchResourceRequest();
qboolean	CL_CheckCRCs( char* map );
qboolean	CL_CheckFile( sizebuf_t* message, char* source );
int			CL_EstimateNeededResources();
void		CL_StartResourceDownloading( char* message, qboolean custom );
qboolean	CL_PrecacheResources();
void		CL_RegisterResources( sizebuf_t* message );
void		CL_ClearResourceList( resource_t* list );
void		CL_ClearResourceLists();
void		CL_MoveToOnHandList( resource_t* resource );
void		CL_RemoveFromResourceList( resource_t* resource );
void		CL_AddToResourceList( resource_t* resource, resource_t* list );
void		CL_HTTPStop_f();
int			CL_GetDownloadQueueSize();
void		CL_MarkMapAsUsingHTTPDownload();
int			CL_CanUseHTTPDownload();
void		CL_QueueHTTPDownload( char* filename );

//
// cl_ents.c
//
void		CL_SetSolidEntities();
void		CL_ParsePacketEntities( qboolean delta, int* bitCount );
void		CL_InterpolateEntity( cl_entity_t* entity );
void		CL_AddStateToEntlist( physent_s* destination, entity_state_t* state );
void		CL_LinkPacketEntities();
qboolean	CL_EntityTeleported( cl_entity_t* entity );
void		CL_PlayerFlashlight();
void		CL_EmitEntities();

//
// cl_pred.c
//
void		CL_InitPrediction();
void		CL_CheckPredictionError();
void		CL_PushPMStates();
void		CL_PopPMStates();
void		CL_SetLastUpdate();
void		CL_RedoPrediction();
void		CL_PredictMove( qboolean repredicting );
void		CL_RunUsercmd( local_state_t* from, local_state_t* to, usercmd_t* command, qboolean runfuncs,
				double* elapsed, unsigned int randomSeed );
void		CL_SetUpPlayerPrediction( qboolean dopred, qboolean includeLocalClient );
void		CL_SetSolidPlayers( int player );

void		CL_TempEntInit();
void		CL_FxTransform( cl_entity_t* entity, float ( *transform )[4] );
int			CL_FxBlend( cl_entity_t* entity );

//
// cl_demo.c
//
int			CL_DemoAPIRecording();
int			CL_DemoAPIPlayback();
int			CL_DemoAPITimedemo();
float		CL_DemoOutTime();
float		CL_DemoInTime();

void		CL_InitEventSystem();
void		CL_FlushEventSystem();
void		CL_HookEvent( char* name, void ( *callback )( event_args_t* args ) );
void		CL_QueueEvent( int flags, int index, float delay, event_args_t* args );
void		CL_ClientQueueEvent( int flags, int index, int clientIndex, float delay, event_args_t* args );
void		CL_FireEvents();
void		CL_ParseEvent();
void		CL_Parse_ReliableEvent();

extern "C" void ClientDLL_MouseEvent( int buttons );
extern "C" void ClientDLL_VoiceStatus( int entityIndex, qboolean talking );
