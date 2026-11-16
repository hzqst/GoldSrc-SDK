#pragma once

#include "const.h"
#include "common.h"
#include "cvar.h"
#include "decal.h"
#include "usermsg.h"
#include "client.h"
#include "engine_edict.h"
#include "userid.h"
#include "server_static.h"

#define MAX_GENERIC 512
#define SOUND_HASH_TABLE_SIZE 1023
#define SERVER_DATAGRAM_SIZE 6000
#define SERVER_MULTICAST_SIZE 1024
#define SERVER_SIGNON_SIZE 32768
#define MAX_LOCALINFO_STRING 32769
#define GAME_APP_ID_MAP_COUNT 11

#define GROUP_OP_AND 0
#define GROUP_OP_NAND 1

#define AUTH_IDTYPE_UNKNOWN 0 // no interaction with other objects
#define AUTH_IDTYPE_STEAM 1 // touch on edge, but not blocking
#define AUTH_IDTYPE_VALVE 2 // touch on edge, block
#define AUTH_IDTYPE_LOCAL 3

struct delta_s;
struct extra_baselines_s;

typedef enum
{
	ss_dead,	// no map loaded
	ss_loading, // spawning level edicts
	ss_active	// actively running
} server_state_t;

typedef struct
{
	qboolean active; // false if only a net client
	qboolean paused;
	qboolean loadgame; // handle connections specially

	double time;
	double oldtime;

	int lastcheck; // used by PF_checkclient
	double lastchecktime;

	char name[MAX_QPATH]; // map name
	char oldname[MAX_QPATH];
	char startspot[MAX_QPATH];
	char modelname[MAX_QPATH]; // maps/<name>.bsp, for model_precache[0]

	model_s* worldmodel;
	CRC32_t worldmapCRC;
	unsigned char clientdllmd5[MD5_DIGEST_BYTES];

	resource_t resourcelist[MAX_RESOURCES];
	int num_resources;

	consistency_t consistency_list[MAX_CONSISTENCY_FILES];
	int num_consistency;

	char* model_precache[MAX_MODELS]; // NULL terminated
	model_s* models[MAX_MODELS];
	unsigned char model_precache_flags[MAX_MODELS];
	event_t event_precache[MAX_EVENT_SCRIPTS];

	char* sound_precache[MAX_SOUNDS]; // NULL terminated
	short sound_precache_hashedlookup[SOUND_HASH_TABLE_SIZE];
	qboolean sound_precache_hashedlookup_built;

	char* generic_precache[MAX_GENERIC];
	char generic_precache_names[MAX_GENERIC][MAX_QPATH];
	int num_generic_names;

	char* lightstyles[MAX_LIGHTSTYLES];

	int num_edicts;
	int max_edicts;
	edict_t* edicts; // Can array index now, edict_t is fixed
	entity_state_t* baselines;
	extra_baselines_s* instance_baselines;

	// edict_t is variable sized, but can
	// be used to reference the world ent
	server_state_t state; // some actions are only valid during load

	sizebuf_t datagram;
	byte datagram_buf[SERVER_DATAGRAM_SIZE];

	sizebuf_t reliable_datagram; // copied to all clients at end of frame
	byte reliable_datagram_buf[SERVER_DATAGRAM_SIZE];

	sizebuf_t multicast;
	byte multicast_buf[SERVER_MULTICAST_SIZE]; // Longest multicast message

	sizebuf_t spectator;
	byte spectator_buf[SERVER_MULTICAST_SIZE];

	sizebuf_t signon;
	byte signon_data[SERVER_SIGNON_SIZE];
} server_t;

typedef struct
{
	double senttime;
	float ping_time;
	clientdata_t clientdata;
	weapon_data_t weapondata[MAX_WEAPON_DATA];
	packet_entities_t entities;
} client_frame_t;

typedef struct client_s
{
	qboolean active;  // false = client is free
	qboolean spawned; // false = don't send datagrams
	qboolean fully_connected;
	qboolean connected;
	qboolean uploading;
	qboolean hasusrmsgs;
	qboolean has_force_unmodified;

//===== NETWORK ============
	netchan_t netchan; // The client's net connection.

	int chokecount; // Number of packets choked at the server because the client - server
										//  network channel is backlogged with too much data.
	int delta_sequence; // -1 = no compression.  This is where the server is creating the
										// compressed info from.

	qboolean fakeclient; // JAC: This client is a fake player controlled by the game DLL
	qboolean proxy;

	usercmd_t lastcmd; // for filling in big drops and partial predictions

	double connecttime;
	double cmdtime;
	double ignorecmdtime;

	float latency;
	float packet_loss;

	double localtime;
	double nextping;
	double svtimebase;

	sizebuf_t datagram;
	byte datagram_buf[CLIENT_DATAGRAM_SIZE];

	double connection_started; // or time of disconnect for zombies
	double next_messagetime;
	// Default time to wait for next message
	double next_messageinterval;
	qboolean send_message; // Set on frames a datagram arrived on
	qboolean skip_message; // Defer message sending for one frame

	client_frame_t* frames; // updates can be deltad from here
	event_state_t events;

	// Identity information.
	edict_t* edict; // EDICT_NUM(clientnum+1)
	const edict_t* pViewEntity;

	int userid; // identifying number on server
	USERID_t network_userid;

	char userinfo[MAX_INFO_STRING]; // infostring
	qboolean sendinfo;
	float sendinfo_time;
	char hashedcdkey[MAX_QPATH];

	char name[MAX_PLAYERNAME]; // for printing to other people
	int topcolor;
	int bottomcolor;
	int entityId; // ID # in save structure

	resource_t resourcesonhand;
	resource_t resourcesneeded;
	FileHandle_t upload;
	qboolean uploaddoneregistering;
	customization_t customdata;

	int crcValue;
	int lw;
	int lc;

	char physinfo[MAX_INFO_STRING];

	qboolean m_bLoopback;
	unsigned int m_VoiceStreams[2];
	double m_lastvoicetime;

	int m_sendrescount;
	qboolean m_bSentNewResponse;
} client_t;

typedef struct GameToAppIDMapItem_s
{
	unsigned int iAppID;
	const char* pGameDir;
} GameToAppIDMapItem_t;

typedef enum
{
	RD_NONE,
	RD_CLIENT,
	RD_PACKET
} redirect_t;

typedef enum
{
	sv_packet_nodelta,
	sv_packet_delta
} sv_delta_t;

typedef struct
{
	int* numbase;
	int num;
	qboolean remove;
	qboolean custom;
	qboolean newbl;
	int newblindex;
	qboolean full;
	int offset;
} deltacallback_t;

extern cvar_t violence_hblood;
extern cvar_t sv_allow_autoaim;
extern cvar_t sv_allow_shaders;
extern cvar_t sv_password;
extern cvar_t sv_proxies;
extern cvar_t sv_instancedbaseline;
extern cvar_t rcon_password;
extern cvar_t sv_rcon_banpenalty;
extern cvar_t sv_rcon_minfailures;
extern cvar_t sv_rcon_maxfailures;
extern cvar_t sv_rcon_minfailuretime;

extern server_t sv; // local server
extern client_t* host_client;
extern edict_t* sv_player;

extern int SV_UPDATE_BACKUP;
extern int SV_UPDATE_MASK;

extern qboolean g_bIsDedicatedServer;
extern int g_bIsCStrike;
extern int g_bIsCZero;
extern int g_bIsTFC;
extern GameToAppIDMapItem_t g_GameToAppIDMap[GAME_APP_ID_MAP_COUNT];

extern int g_groupmask;
extern int g_groupop;
extern int gPacketSuppressed;
extern char* pr_strings;
extern char localinfo[MAX_LOCALINFO_STRING];
extern int num_servers;

extern redirect_t sv_redirected;
extern netadr_t sv_redirectto;
extern char outputbuf[MAX_ROUTEABLE_PACKET];

extern deltacallback_t g_svdeltacallback;

//
// sv_main.c
//
void			SV_Init();
void			SV_SetMaxclients();
void			SV_AllocClientFrames();
void			SV_ClearFrames( client_frame_t** frames );
void			SV_InitDeltas();
delta_s*		SV_LookupDelta( char* name );

void			SV_Multicast( edict_t* entity, vec_t* origin, int destination, qboolean reliable );
void			SV_BroadcastCommand( const char* format, ... );
void			SV_BroadcastPrintf( const char* format, ... );
void			SV_SendUserReg( sizebuf_t* buffer );
void			SV_StartParticle( const float* origin, const float* direction, int color, int count );

int				SV_LookupSoundIndex( const char* sample );
qboolean		SV_BuildSoundMsg( edict_t* entity, int channel, const char* sample, int volume, float attenuation,
					int flags, int pitch, const vec_t* origin, sizebuf_t* buffer );
void			SV_StartSound( int recipients, edict_t* entity, int channel, const char* sample, int volume,
					float attenuation, int flags, int pitch );

int				SV_ModelIndex( const char* name );
struct model_s* SV_ModelForIndex( int modelindex );
unsigned short	EV_Precache( int type, const char* name );

int				SV_CheckUserInfo( netadr_t* address, char* userinfo, qboolean reconnecting, int reconnectSlot,
					char* name );
void			SV_RejectConnection( netadr_t* address, const char* format, ... );
int				SV_CheckChallenge( netadr_t* address, int challenge );
void			SV_ExtractFromUserinfo( client_t* client );
char*			SV_GetIDString( USERID_t* id );
const char*		SV_GetClientIDString( client_t* client );
int				SV_CalcPing( client_t* client );

void			SV_AddFailedRcon( netadr_t* address );
int				SV_Rcon_Validate();
void			SV_Rcon( netadr_t* address );
void			SV_FlushRedirect();

void			SV_ParseStringCommand( client_t* client );
void			SV_ParseDelta( client_t* client );
void			SV_ParseVoiceData( client_t* client );
void			SV_ParseCvarValue( client_t* client );
void			SV_ParseCvarValue2( client_t* client );

void			SV_InvokeCallback();
void			SV_WriteDeltaHeader( int number, qboolean remove, qboolean custom, int* numberBase, qboolean newBaseline,
					int newBaselineIndex, qboolean full, int offset );
int				SV_CreatePacketEntities( sv_delta_t type, client_t* client, packet_entities_t* packet,
					sizebuf_t* message );
void			SV_EmitEvents( client_t* client, packet_entities_t* packet, sizebuf_t* message );
void			SV_WriteEntitiesToClient( client_t* client, sizebuf_t* message );
void			SV_UpdateToReliableMessages();
void			SV_SendClientMessages();
void			SV_ForceFullClientsUpdate();
int				SV_FullUpdate_Internal();
void			SV_FullUpdate_f();

void			SetCStrikeFlags();
int				GetGameAppID();
qboolean		IsGameSubscribed( const char* game );
qboolean		IsSafeFileToDownload( const char* filename );
void			Steam_SetCVar( const char* name, const char* value );

void			PF_SetKeyValue_I( char* info, char* key, char* value );
