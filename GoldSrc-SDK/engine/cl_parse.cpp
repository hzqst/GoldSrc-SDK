#include "cl_demo.h"
#include "client.h"
#include "cmd.h"
#include "common.h"
#include "conprint.h"
#include "crc.h"
#include "decals.h"
#include "delta.h"
#include "draw.h"
#include "enginecallbacks.h"
#include "filesystem.h"
#include "gl_model.h"
#include "hashpak.h"
#include "host.h"
#include "info.h"
#include "mem.h"
#include "modinfo.h"
#include "net.h"
#include "pm_defs.h"
#include "protocol.h"
#include "render.h"
#include "server.h"
#include "sound.h"
#include "steam_api.h"
#include "sys.h"
#include "usermsg.h"
#include "vgui_intwrap2.h"
#include "zone.h"
#include "../common/hltv.h"
#include "eiface.h"
#include "../pm_shared/pm_movevars.h"

#if defined( _WIN32 )
#include <cassert>
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\cl_parse.c" )

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

namespace
{
constexpr int BASELINE_TERMINATOR_BITS = 16;
constexpr unsigned int BASELINE_TERMINATOR = 0xffff;
constexpr int BASELINE_ENTITY_INDEX_BITS = 11;
constexpr int BASELINE_ENTITY_TYPE_BITS = 2;
constexpr int BASELINE_INSTANCE_COUNT_BITS = 6;

constexpr int USER_MESSAGE_VARIABLE_SIZE = 255;
constexpr int USER_MESSAGE_DATA_BYTES = 192;
constexpr int USER_MESSAGE_ID_LIMIT = 256;
constexpr int WEAPON_LIST_MESSAGE_MAX_BYTES = 123;
constexpr int APPID_HALF_LIFE = 70;
constexpr int DELTA_DESCRIPTION_NAME_BYTES = 256;
constexpr int DELTA_DESCRIPTION_COUNT_BITS = 16;

constexpr double SOUND_VOLUME_BYTE_MAX = 255.0;
constexpr double SOUND_ATTENUATION_BYTE_SCALE = 1.0 / 64.0;
constexpr int SIGNON_PRESPAWN = 2;

constexpr int MESSAGE_HISTORY_COUNT = 32;
constexpr int MESSAGE_HISTORY_MASK = MESSAGE_HISTORY_COUNT - 1;
constexpr int MESSAGE_USER_BUCKET = MESSAGE_STAT_BUCKETS - 1;
constexpr int MESSAGE_BITS_PER_BYTE = 8;
constexpr float MESSAGE_BYTES_TO_KILOBYTES = 1.0f / 1024.0f;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
typedef struct svc_func_s
{
	byte opcode;
	const char* pszname; // Display Name
	void ( *pfnParse )(); // Parse function
} svc_func_t;

typedef struct
{
	int command;
	int starting_offset;
	int frame_number;
} oldcmd_t;

static_assert( sizeof( svc_func_t ) == 12 );
static_assert( sizeof( oldcmd_t ) == 12 );

#if !defined( SW_RENDERER )
extern cvar_t gl_zmax;
extern cvar_t gl_wateramp;
#endif
extern cvar_t cl_showmessages;
extern cvar_t cl_weaponlistfix;
extern cvar_t cl_shownet;

double parsecounttime;
int CL_UPDATE_BACKUP = 64;
int CL_UPDATE_MASK = 63;
static UserMsg* gClientUserMsgs;
int msg_buckets[MESSAGE_STAT_BUCKETS];
int last_data[MESSAGE_STAT_BUCKETS];
int total_data[MESSAGE_STAT_BUCKETS];
static oldcmd_t oldcmd[MESSAGE_HISTORY_COUNT];
static int messageHistoryNext;
static int messageHistorySize;

qboolean CL_IsPlayerIndex( int index );
qboolean CL_CheckGameDirectory( char* gamedir );
void CL_ParseListen();
void R_DecalRemoveAll( int textureIndex );
void DELTA_FreeDescription( delta_t** description );
void SCR_EndLoadingPlaque();
void CL_Parse_Disconnect();
void CL_Parse_Version();
void CL_Parse_SetView();
void CL_Parse_Sound();
void CL_Parse_Time();
void CL_Parse_Print();
void CL_Parse_StuffText();
void CL_Parse_SetAngle();
void CL_Parse_ServerInfo();
void CL_Parse_LightStyle();
void CL_Parse_UpdateUserInfo();
void CL_Parse_DeltaDescription();
void CL_Parse_ClientData();
void CL_Parse_StopSound();
void CL_Parse_Pings();
void CL_Parse_Particle();
void CL_Parse_Damage();
void CL_Parse_SpawnStatic();
void CL_Parse_SpawnBaseline();
void CL_Parse_TempEntity();
void CL_Parse_SetPause();
void CL_Parse_SignonNum();
void CL_Parse_CenterPrint();
void CL_Parse_KilledMonster();
void CL_Parse_FoundSecret();
void CL_Parse_SpawnStaticSound();
void CL_Parse_Intermission();
void CL_Parse_Finale();
void CL_Parse_CDTrack();
void CL_Parse_Restore();
void CL_Parse_CutScene();
void CL_Parse_WeaponAnim();
void CL_Parse_DecalName();
void CL_Parse_RoomType();
void CL_Parse_AddAngle();
void CL_Parse_NewUserMsg();
void CL_Parse_PacketEntities();
void CL_Parse_DeltaPacketEntities();
void CL_Parse_Choke();
void CL_Parse_ResourceList();
void CL_Parse_NewMoveVars();
void CL_Parse_ResourceRequest();
void CL_Parse_Customization();
void CL_Parse_CrosshairAngle();
void CL_Parse_SoundFade();
void CL_ParseFileTxferFailed();
void CL_Parse_HLTV();
void CL_Parse_Director();
void CL_Parse_VoiceInit();
void CL_Parse_VoiceData();
void CL_Set_ServerExtraInfo();
void CL_Parse_Timescale();
void CL_Parse_ResourceLocation();
void CL_Send_CvarValue();
void CL_Send_CvarValue2();
void CL_Exec();

cl_entity_t* CL_EntityNum( int index )
{
	if ( index >= cl.num_entities )
	{
		if ( index >= cl.max_edicts )
			Host_Error( "CL_EntityNum: %i is an invalid number, cl.max_edicts is %i", index, cl.max_edicts );
		do
		{
			cl_entities[index].index = index;
			++cl.num_entities;
		} while ( index >= cl.num_entities );
	}
	cl_entities[index].index = index;
	return &cl_entities[index];
}

customization_t* CL_PlayerHasCustomization( int player, resourcetype_t type )
{
	for ( customization_t* customization = cl.players[player].customdata.pNext; customization;
		customization = customization->pNext )
	{
		if ( customization->resource.type == type )
			return customization;
	}
	return nullptr;
}

void CL_RemoveCustomization( int player, customization_t* remove )
{
	customization_t* previous = &cl.players[player].customdata;
	customization_t* customization = previous->pNext;
	while ( customization )
	{
		if ( customization == remove )
		{
			customization_t* next = remove->pNext;
			if ( remove->bInUse )
			{
				if ( remove->pBuffer )
					Mem_Free( remove->pBuffer );
				if ( remove->bInUse && remove->pInfo )
				{
					if ( remove->resource.type == t_decal )
					{
						cachewad_t* wad = static_cast<cachewad_t*>( remove->pInfo );
						if ( cls.state == ca_active )
						{
							R_DecalRemoveAll( ~player );
							wad = static_cast<cachewad_t*>( remove->pInfo );
						}
						Mem_Free( wad->lumps );
						for ( int entry = 0; entry < wad->cacheCount; ++entry )
						{
							cache_user_t* cache = &wad->cache[entry].cache;
							if ( Cache_Check( cache ) )
								Cache_Free( cache );
						}
						Mem_Free( wad->name );
						Mem_Free( wad->cache );
					}
					Mem_Free( remove->pInfo );
				}
			}
			Mem_Free( remove );
			previous->pNext = next;
			customization = next;
		}
		else
		{
			previous = customization;
			customization = customization->pNext;
		}
	}
}

void CL_ParseBaseline()
{
	delta_t** ppentity = DELTA_LookupRegistration( const_cast<char*>( "entity_state_t" ) );
	delta_t** ppplayer = DELTA_LookupRegistration( const_cast<char*>( "entity_state_player_t" ) );
	delta_t** ppcustom = DELTA_LookupRegistration( const_cast<char*>( "custom_entity_state_t" ) );
#if defined( _WIN32 )
	ASSERT_AT( ppentity && *ppentity && ppcustom && *ppcustom && ppplayer && *ppplayer, 2401 );
#endif
	entity_state_t nullstate;
	Q_memset( &nullstate, 0, sizeof( nullstate ) );
	MSG_StartBitReading( &net_message );
	ContinueLoadingProgressBar( "ClientConnect", 11, 0.0f );
	SetLoadingProgressBarStatusText( "#GameUI_ParseBaseline" );
	cl.highentity = 0;
	cl_entity_t* entity = nullptr;
	while ( MSG_PeekBits( BASELINE_TERMINATOR_BITS ) != BASELINE_TERMINATOR )
	{
		int index = static_cast<int>( MSG_ReadBits( BASELINE_ENTITY_INDEX_BITS ) );
		if ( index > cl.highentity )
			cl.highentity = index;
		entity = CL_EntityNum( index );
		entity->index = index;
		byte type = static_cast<byte>( MSG_ReadBits( BASELINE_ENTITY_TYPE_BITS ) );
		delta_t* description;
		if ( type & ENTITY_BEAM )
			description = *ppcustom;
		else
			description = CL_IsPlayerIndex( index ) ? *ppplayer : *ppentity;
		DELTA_ParseDelta( reinterpret_cast<byte*>( &nullstate ), reinterpret_cast<byte*>( &entity->baseline ),
			description, sizeof( entity->baseline ) );
		entity->baseline.entityType = type;
	}
	MSG_ReadBits( BASELINE_TERMINATOR_BITS );
	Q_memset( cl.instanced_baseline, 0, sizeof( cl.instanced_baseline ) );
	cl.instanced_baseline_number = static_cast<int>( MSG_ReadBits( BASELINE_INSTANCE_COUNT_BITS ) );
	for ( int baseline = 0; baseline < cl.instanced_baseline_number; ++baseline )
	{
		DELTA_ParseDelta( reinterpret_cast<byte*>( &nullstate ),
			reinterpret_cast<byte*>( &cl.instanced_baseline[baseline] ), *ppentity, sizeof( entity_state_t ) );
	}
	MSG_EndBitReading( &net_message );
	if ( entity )
	{
		entity->curstate = entity->baseline;
		entity->prevstate = entity->baseline;
	}
}

void CL_ParseCustomization()
{
	unsigned int player = static_cast<unsigned int>( MSG_ReadByte() );
	if ( player >= MAX_CLIENTS )
		Host_Error( "Bogus player index during customization parsing.\n" );
	resource_t* resource = static_cast<resource_t*>( Mem_ZeroMalloc( sizeof( resource_t ) ) );
	resource->type = static_cast<resourcetype_t>( MSG_ReadByte() );
	Q_strncpy( resource->szFileName, MSG_ReadString(), sizeof( resource->szFileName ) - 1 );
	resource->szFileName[sizeof( resource->szFileName ) - 1] = '\0';
	resource->nIndex = MSG_ReadShort();
	resource->nDownloadSize = MSG_ReadLong();
	resource->ucFlags = static_cast<byte>( MSG_ReadByte() & ~RES_WASMISSING );
	resource->pNext = nullptr;
	resource->pPrev = nullptr;
	if ( resource->ucFlags & RES_CUSTOM )
		MSG_ReadBuf( sizeof( resource->rgucMD5_hash ), resource->rgucMD5_hash );
	resource->playernum = static_cast<byte>( player );
	if ( cl_allow_download.value == 0.0f )
	{
		Con_DPrintf( "Refusing new resource, cl_allow_download set to 0\n" );
		Mem_Free( resource );
		return;
	}
	if ( cls.state == ca_active && cl_download_ingame.value == 0.0f )
	{
		Con_DPrintf( "Refusing new resource, cl_download_ingame set to 0\n" );
		Mem_Free( resource );
		return;
	}
	customization_t* previous = CL_PlayerHasCustomization( player, resource->type );
	if ( previous )
		CL_RemoveCustomization( player, previous );
	qboolean duplicate = false;
	for ( customization_t* customization = cl.players[resource->playernum].customdata.pNext; customization;
		customization = customization->pNext )
	{
		if ( Q_memcmp( customization->resource.rgucMD5_hash, resource->rgucMD5_hash, sizeof( resource->rgucMD5_hash ) )
			 == 0 )
		{
			duplicate = true;
			break;
		}
	}
	if ( HPAK_GetDataPointer( "custom.hpk", resource, nullptr, nullptr ) )
	{
		if ( duplicate )
			Con_DPrintf( "Duplicate resource ignored for local client\n" );
		else if ( !COM_CreateCustomization( &cl.players[resource->playernum].customdata, resource, resource->playernum,
					  FCUST_FROMHPAK, nullptr, nullptr ) )
			Con_DPrintf( "Error loading customization\n" );
		Mem_Free( resource );
	}
	else
	{
		resource->ucFlags |= RES_WASMISSING;
		CL_AddToResourceList( resource, &cl.resourcesneeded );
		Con_Printf( "Requesting %s from server\n", resource->szFileName );
		CL_StartResourceDownloading( const_cast<char*>( "Custom resource propagation...\n" ), true );
	}
}

void CL_ParseStatic()
{
}

// Dispatch table
static const svc_func_t cl_parsefuncs[] = { { 0, "svc_bad", nullptr }, { 1, "svc_nop", nullptr },
	{ 2, "svc_disconnect", CL_Parse_Disconnect }, { 3, "svc_event", CL_ParseEvent },
	{ 4, "svc_version", CL_Parse_Version }, { 5, "svc_setview", CL_Parse_SetView }, { 6, "svc_sound", CL_Parse_Sound },
	{ 7, "svc_time", CL_Parse_Time }, { 8, "svc_print", CL_Parse_Print }, { 9, "svc_stufftext", CL_Parse_StuffText },
	{ 10, "svc_setangle", CL_Parse_SetAngle }, { 11, "svc_serverinfo", CL_Parse_ServerInfo },
	{ 12, "svc_lightstyle", CL_Parse_LightStyle }, { 13, "svc_updateuserinfo", CL_Parse_UpdateUserInfo },
	{ 14, "svc_deltadescription", CL_Parse_DeltaDescription }, { 15, "svc_clientdata", CL_Parse_ClientData },
	{ 16, "svc_stopsound", CL_Parse_StopSound }, { 17, "svc_pings", CL_Parse_Pings },
	{ 18, "svc_particle", CL_Parse_Particle }, { 19, "svc_damage", CL_Parse_Damage },
	{ 20, "svc_spawnstatic", CL_Parse_SpawnStatic }, { 21, "svc_event_reliable", CL_Parse_ReliableEvent },
	{ 22, "svc_spawnbaseline", CL_Parse_SpawnBaseline }, { 23, "svc_temp_entity", CL_Parse_TempEntity },
	{ 24, "svc_setpause", CL_Parse_SetPause }, { 25, "svc_signonnum", CL_Parse_SignonNum },
	{ 26, "svc_centerprint", CL_Parse_CenterPrint }, { 27, "svc_killedmonster", CL_Parse_KilledMonster },
	{ 28, "svc_foundsecret", CL_Parse_FoundSecret }, { 29, "svc_spawnstaticsound", CL_Parse_SpawnStaticSound },
	{ 30, "svc_intermission", CL_Parse_Intermission }, { 31, "svc_finale", CL_Parse_Finale },
	{ 32, "svc_cdtrack", CL_Parse_CDTrack }, { 33, "svc_restore", CL_Parse_Restore },
	{ 34, "svc_cutscene", CL_Parse_CutScene }, { 35, "svc_weaponanim", CL_Parse_WeaponAnim },
	{ 36, "svc_decalname", CL_Parse_DecalName }, { 37, "svc_roomtype", CL_Parse_RoomType },
	{ 38, "svc_addangle", CL_Parse_AddAngle }, { 39, "svc_newusermsg", CL_Parse_NewUserMsg },
	{ 40, "svc_packetentities", CL_Parse_PacketEntities }, // 35
	{ 41, "svc_deltapacketentities", CL_Parse_DeltaPacketEntities }, { 42, "svc_choke", CL_Parse_Choke },
	{ 43, "svc_resourcelist", CL_Parse_ResourceList }, { 44, "svc_newmovevars", CL_Parse_NewMoveVars },
	{ 45, "svc_resourcerequest", CL_Parse_ResourceRequest }, { 46, "svc_customization", CL_Parse_Customization },
	{ 47, "svc_crosshairangle", CL_Parse_CrosshairAngle }, { 48, "svc_soundfade", CL_Parse_SoundFade },
	{ 49, "svc_filetxferfailed", CL_ParseFileTxferFailed }, { 50, "svc_hltv", CL_Parse_HLTV },
	{ 51, "svc_director", CL_Parse_Director }, { 52, "svc_voiceinit", CL_Parse_VoiceInit },
	{ 53, "svc_voicedata", CL_Parse_VoiceData }, { 54, "svc_sendextrainfo", CL_Set_ServerExtraInfo },
	{ 55, "svc_timescale", CL_Parse_Timescale }, { 56, "svc_resourcelocation", CL_Parse_ResourceLocation },
	{ 57, "svc_sendcvarvalue", CL_Send_CvarValue }, { 58, "svc_sendcvarvalue2", CL_Send_CvarValue2 },
	{ 59, "svc_exec", CL_Exec } };

void DispatchUserMsg( int messageId )
{
	static byte buffer[USER_MESSAGE_DATA_BYTES];
	if ( messageId >= USER_MESSAGE_ID_LIMIT )
		Host_Error( "DispatchUserMsg:  Illegal User Msg %d\n", messageId );
	UserMsg* message = gClientUserMsgs;
	while ( message && message->iMsg != messageId )
		message = message->next;
	if ( !message )
		Host_Error( "UserMsg: Not Present on Client %d\n", messageId );
	int size = message->iSize;
	if ( size == -1 ) // variable-length message
		size = MSG_ReadByte(); // extract the length of the variable-length message from the stream
	if ( cl_showmessages.value != 0.0f )
		Con_DPrintf( "UserMsg: %s (size %i)\n", message->szName, size );
	if ( size > USER_MESSAGE_DATA_BYTES )
	{
		Host_Error( "DispatchUserMsg:  User Msg %s/%d sent too much data (%i bytes), %i bytes max.\n", message->szName,
			message->iMsg, size, USER_MESSAGE_DATA_BYTES );
	}
	MSG_ReadBuf( size, buffer );
	if ( message->pfn )
	{
		if ( Q_strcasecmp( message->szName, "WeaponList" ) != 0 || size <= WEAPON_LIST_MESSAGE_MAX_BYTES
			 || ( GetGameAppID() == APPID_HALF_LIFE && Q_strcasecmp( com_gamedir, "valve" ) != 0
				  && cl_weaponlistfix.value <= 0.0f ) )
		{
			message->pfn( message->szName, size, buffer );
		}
		else
		{
			Con_Printf( "Malformed WeaponList request, ignoring\n" );
		}
	}
	else
	{
		Con_DPrintf( "UserMsg: No pfn %s %d\n", message->szName, messageId );
	}
}

char* CL_MsgInfo( int command )
{
	static char name[256];
	Q_strcpy( name, "???" );
	if ( command <= svc_lastmsg )
	{
		Q_strncpy( name, cl_parsefuncs[command].pszname, sizeof( name ) - 1 );
		name[sizeof( name ) - 1] = '\0';
	}
	else
	{
		for ( UserMsg* message = gClientUserMsgs; message; message = message->next )
		{
			if ( message->iMsg == command )
			{
				Q_strncpy( name, message->szName, sizeof( name ) - 1 );
				name[sizeof( name ) - 1] = '\0';
				break;
			}
		}
	}
	return name;
}

void CL_ResetFrame( frame_t* frame )
{
	frame->receivedtime = realtime;
	frame->latency = 0.0;
	frame->invalid = false;
	frame->choked = false;
	frame->clientbytes = 0;
	frame->playerinfobytes = 0;
	frame->packetentitybytes = 0;
	frame->tentitybytes = 0;
	frame->soundbytes = 0;
	frame->eventbytes = 0;
	frame->usrbytes = 0;
	frame->msgbytes = 0;
	frame->time = cl.mtime[0];
}

void CL_WriteErrorMessage( int starting_count, int current_count, sizebuf_t* message )
{
	char name[MAX_PATH];
	_snprintf( name, sizeof( name ), "%s", "buffer.dat" );
	COM_FixSlashes( name );
	COM_CreatePath( name );
	FileHandle_t file = FS_Open( name, "wb" );
	if ( file )
	{
		FS_Write( &starting_count, sizeof( starting_count ), 1, file );
		FS_Write( &current_count, sizeof( current_count ), 1, file );
		FS_Write( message->data, message->cursize, 1, file );
		FS_Close( file );
		Con_Printf( "Wrote erroneous message to %s\n", "buffer.dat" );
	}
}

void CL_WriteMessageHistory( int starting_count, int command )
{
	Con_Printf( "Last %i messages parsed.\n", messageHistorySize );
	for ( int previous = messageHistorySize; previous > 0; --previous )
	{
		unsigned int index = ( static_cast<unsigned int>( messageHistoryNext ) - previous ) & MESSAGE_HISTORY_MASK;
		Con_Printf( "%i %04i %s\n", oldcmd[index].frame_number, oldcmd[index].starting_offset,
			CL_MsgInfo( oldcmd[index].command ) );
	}
	Con_Printf( "BAD:  %3i:%s\n", msg_readcount - 1, CL_MsgInfo( command ) );
	if ( developer.value != 0.0f )
		CL_WriteErrorMessage( starting_count, msg_readcount - 1, &net_message );
}

void CL_TransferMessageData()
{
	for ( int bucket = 0; bucket < MESSAGE_STAT_BUCKETS; ++bucket )
	{
		unsigned int accumulated =
			static_cast<unsigned int>( total_data[bucket] ) + static_cast<unsigned int>( last_data[bucket] );
		total_data[bucket] = static_cast<int>( accumulated );
	}
}

void CL_ParseServerMessage( qboolean normal_message )
{
	int starting_count = msg_readcount;
	if ( cl_shownet.value == 1.0f )
		Con_Printf( "%i ", net_message.cursize );
	else if ( cl_shownet.value == 2.0f )
		Con_Printf( "------------------\n" );
	Q_memset( last_data, 0, sizeof( last_data ) );
	if ( normal_message )
	{
		frame_t* frame = &cl.frames[cls.netchan.incoming_sequence & CL_UPDATE_MASK];
		if ( cls.state == ca_active )
		{
			frame->invalid = true;
			frame->choked = false;
		}
		else
		{
			CL_ResetFrame( frame );
		}
	}
	while ( true )
	{
		if ( msg_badread )
			Host_Error( "CL_ParseServerMessage: Bad server message" );
		int commandStart = msg_readcount;
		int command = MSG_ReadByte();
		if ( command == -1 )
			break;
		if ( command != svc_nop )
		{
			oldcmd_t* history = &oldcmd[messageHistoryNext];
			history->command = command;
			history->starting_offset = commandStart;
			history->frame_number = host_framecount;
			messageHistoryNext = ( messageHistoryNext + 1 ) & MESSAGE_HISTORY_MASK;
			if ( messageHistorySize < MESSAGE_HISTORY_COUNT )
				++messageHistorySize;
		}
		if ( command > svc_lastmsg )
		{
			++msg_buckets[MESSAGE_USER_BUCKET];
			DispatchUserMsg( command );
			last_data[MESSAGE_USER_BUCKET] += msg_readcount - commandStart;
			frame_t* frame = &cl.frames[cl.parsecountmod];
			frame->usrbytes = static_cast<unsigned short>( frame->usrbytes + msg_readcount - commandStart );
			continue;
		}
		if ( cl_shownet.value == 2.0f && Q_strlen( cl_parsefuncs[command].pszname ) > 1 )
			Con_Printf( "%3i:%s\n", msg_readcount - 1, cl_parsefuncs[command].pszname );
		if ( static_cast<unsigned int>( command ) < MESSAGE_STAT_BUCKETS )
			++msg_buckets[command];
		if ( command == svc_bad )
		{
			CL_WriteMessageHistory( starting_count, command );
			Host_Error( "CL_ParseServerMessage: Illegible server message - %s\n", cl_parsefuncs[svc_bad].pszname );
		}
		void ( *parse )() = cl_parsefuncs[command].pfnParse;
		if ( parse )
		{
			if ( cl_showmessages.value != 0.0f )
				Con_DPrintf( "Msg: %s\n", cl_parsefuncs[command].pszname );
			parse();
		}
		last_data[command] += msg_readcount - commandStart;
		frame_t* frame = &cl.frames[cl.parsecountmod];
		switch ( command )
		{
		case svc_packetentities:
		case svc_deltapacketentities:
		{
			frame->packetentitybytes =
				static_cast<unsigned short>( frame->packetentitybytes + msg_readcount - commandStart );
			short playerBytes = static_cast<short>( cls.playerbits / MESSAGE_BITS_PER_BYTE );
			frame->packetentitybytes -= playerBytes;
			frame->playerinfobytes += playerBytes;
			break;
		}
		case svc_temp_entity:
			frame->tentitybytes = static_cast<unsigned short>( frame->tentitybytes + msg_readcount - commandStart );
			break;
		case svc_sound:
			frame->soundbytes = static_cast<unsigned short>( frame->soundbytes + msg_readcount - commandStart );
			break;
		case svc_clientdata:
			frame->clientbytes = static_cast<unsigned short>( frame->clientbytes + msg_readcount - commandStart );
			break;
		case svc_event:
		case svc_event_reliable:
			frame->eventbytes = static_cast<unsigned short>( frame->eventbytes + msg_readcount - commandStart );
			break;
		default:
			break;
		}
	}
	if ( cl_shownet.value == 2.0f && Q_strlen( "END OF MESSAGE" ) > 1 )
		Con_Printf( "%3i:%s\n", msg_readcount - 1, "END OF MESSAGE" );
	if ( cls.state == ca_active )
		cl.frames[cl.parsecountmod].msgbytes += static_cast<unsigned short>( net_message.cursize );
	CL_TransferMessageData();
	if ( !cls.demoplayback )
	{
		if ( cls.state != ca_active )
			CL_WriteDemoStartup( starting_count, &net_message );
		if ( cls.demorecording && !cls.demowaiting )
			CL_WriteDemoMessage( starting_count, &net_message );
	}
}

void CL_UpdateUserinfo()
{
	int index = MSG_ReadByte();
	if ( index >= MAX_CLIENTS )
		Host_EndGame( "CL_ParseServerMessage: svc_updateuserinfo > MAX_CLIENTS" );
	player_info_t* player = &cl.players[index];
	player->userid = MSG_ReadLong();
	Q_strncpy( player->userinfo, MSG_ReadString(), sizeof( player->userinfo ) - 1 );
	player->userinfo[sizeof( player->userinfo ) - 1] = '\0';
	MSG_ReadBuf( sizeof( player->hashedcdkey ), player->hashedcdkey );
	Q_strncpy( player->name, Info_ValueForKey( player->userinfo, "name" ), sizeof( player->name ) - 1 );
	player->name[sizeof( player->name ) - 1] = '\0';
	Q_strncpy( player->model, Info_ValueForKey( player->userinfo, "model" ), sizeof( player->model ) - 1 );
	player->model[sizeof( player->model ) - 1] = '\0';
	player->topcolor = Q_atoi( Info_ValueForKey( player->userinfo, "topcolor" ) );
	player->bottomcolor = Q_atoi( Info_ValueForKey( player->userinfo, "bottomcolor" ) );
	player->spectator = Q_atoi( Info_ValueForKey( player->userinfo, "*hltv" ) );
	player->m_nSteamID = _atoi64( Info_ValueForKey( player->userinfo, "*sid" ) );
	if ( !player->userinfo[0] || !player->name[0] )
		COM_ClearCustomizationList( &player->customdata, true );
	Steam_UpdateRichPresence();
}

void CL_ParseMovevars()
{
	// get the movevars
	movevars.gravity = MSG_ReadFloat();
	movevars.stopspeed = MSG_ReadFloat();
	movevars.maxspeed = MSG_ReadFloat();
	movevars.spectatormaxspeed = MSG_ReadFloat();
	movevars.accelerate = MSG_ReadFloat();
	movevars.airaccelerate = MSG_ReadFloat();
	movevars.wateraccelerate = MSG_ReadFloat();
	movevars.friction = MSG_ReadFloat();
	movevars.edgefriction = MSG_ReadFloat();
	movevars.waterfriction = MSG_ReadFloat();
	movevars.entgravity = MSG_ReadFloat();
	movevars.bounce = MSG_ReadFloat();
	movevars.stepsize = MSG_ReadFloat();
	movevars.maxvelocity = MSG_ReadFloat();
	movevars.zmax = MSG_ReadFloat();
	movevars.waveHeight = MSG_ReadFloat();
	movevars.footsteps = MSG_ReadByte();
	movevars.rollangle = MSG_ReadFloat();
	movevars.rollspeed = MSG_ReadFloat();
	movevars.skycolor_r = MSG_ReadFloat();
	movevars.skycolor_g = MSG_ReadFloat();
	movevars.skycolor_b = MSG_ReadFloat();
	movevars.skyvec_x = MSG_ReadFloat();
	movevars.skyvec_y = MSG_ReadFloat();
	movevars.skyvec_z = MSG_ReadFloat();
	Q_strncpy( movevars.skyName, MSG_ReadString(), sizeof( movevars.skyName ) - 1 );
	movevars.skyName[sizeof( movevars.skyName ) - 1] = '\0';
#if !defined( SW_RENDERER )
	if ( movevars.zmax != gl_zmax.value )
		Cvar_SetValue( "gl_zmax", movevars.zmax );
	if ( movevars.waveHeight != gl_wateramp.value )
		Cvar_SetValue( "gl_wateramp", movevars.waveHeight );
	if ( cl_entities )
		cl_entities[0].curstate.scale = gl_wateramp.value;
#endif
}

void CL_ParseStaticSound()
{
	vec3_t origin;
	for ( int axis = 0; axis < 3; ++axis )
		origin[axis] = MSG_ReadCoord( &net_message );
	int soundIndex = MSG_ReadShort();
	float volume = static_cast<float>( MSG_ReadByte() / SOUND_VOLUME_BYTE_MAX );
	float attenuation = static_cast<float>( MSG_ReadByte() * SOUND_ATTENUATION_BYTE_SCALE );
	int entity = MSG_ReadShort();
	int pitch = MSG_ReadByte();
	int flags = MSG_ReadByte();
	sfx_t sentence;
	sfx_t* sound;
	if ( flags & SND_SENTENCE )
	{
		_snprintf( sentence.name, sizeof( sentence.name ), "!%s", rgpszrawsentence[soundIndex] );
		sound = &sentence;
	}
	else
	{
		sound = cl.sound_precache[soundIndex];
	}
	S_StartStaticSound( entity, CHAN_STATIC, sound, origin, volume, attenuation, flags, pitch );
}

void CL_ParseSoundFade()
{
	int percent = MSG_ReadByte();
	int holdTime = MSG_ReadByte();
	int inTime = MSG_ReadByte();
	int outTime = MSG_ReadByte();
	cls.soundfade.nStartPercent = percent;
	cls.soundfade.soundFadeInTime = inTime;
	cls.soundfade.soundFadeHoldTime = holdTime;
	cls.soundfade.soundFadeStartTime = realtime;
	cls.soundfade.soundFadeOutTime = outTime;
}

void CL_ParseHLTV()
{
	switch ( MSG_ReadByte() )
	{
	case HLTV_ACTIVE:
		cls.spectator = true;
		g_bRedirectedToProxy = false;
		break;
	case HLTV_STATUS:
		MSG_ReadLong();
		MSG_ReadShort();
		MSG_ReadWord();
		MSG_ReadLong();
		MSG_ReadLong();
		MSG_ReadWord();
		break;
	case HLTV_LISTEN:
		CL_ParseListen();
		break;
	default:
		Con_Printf( " CL_Parse_HLTV: unknown HLTV command.\n" );
		break;
	}
}

void AddNewUserMsg()
{
	UserMsg received = {};
	received.iMsg = MSG_ReadByte();
	received.iSize = MSG_ReadByte();
	if ( received.iSize == USER_MESSAGE_VARIABLE_SIZE )
		received.iSize = -1;
	for ( int offset = 0; offset < USER_MESSAGE_NAME_BYTES; offset += sizeof( int ) )
	{
		int word = MSG_ReadLong();
		Q_strncpy( received.szName + offset, reinterpret_cast<const char*>( &word ), sizeof( word ) );
	}
	received.szName[USER_MESSAGE_NAME_BYTES - 1] = '\0';
	qboolean found = 0;
	for ( UserMsg* message = gClientUserMsgs; message; message = message->next )
	{
		if ( message->iMsg == received.iMsg )
			message->iMsg = 0;
		if ( Q_strcasecmp( message->szName, received.szName ) == 0 )
		{
			found = 1;
			message->iMsg = received.iMsg;
			message->iSize = received.iSize;
		}
	}
	if ( !found )
	{
		UserMsg* message = static_cast<UserMsg*>( Mem_Malloc( sizeof( UserMsg ) ) );
		Q_memcpy( message, &received, sizeof( *message ) );
		message->next = gClientUserMsgs;
		gClientUserMsgs = message;
	}
}

int DispatchDirectUserMsg( const char* name, int size, void* buffer )
{
	int found = 0;
	for ( UserMsg* message = gClientUserMsgs; message; message = message->next )
	{
		if ( Q_strcasecmp( name, message->szName ) == 0 )
		{
			found = 1;
			int messageSize = message->iSize == -1 ? size : message->iSize;
			if ( message->pfn )
				message->pfn( message->szName, messageSize, buffer );
			else
				Con_DPrintf( "UserMsg: No pfn %s %d\n", message->szName, message->iMsg );
		}
	}
	return found;
}

void CL_Parse_DeltaDescription()
{
	delta_description_t nullDescription;
	Q_memset( &nullDescription, 0, sizeof( nullDescription ) );
	char* descriptionName = MSG_ReadString();
	if ( !descriptionName || !*descriptionName )
		Host_Error( "CL_Parse_DeltaDescription:  Illegible description name\n" );
	char name[DELTA_DESCRIPTION_NAME_BYTES];
	Q_strncpy( name, descriptionName, sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = '\0';
	delta_t** description = DELTA_LookupRegistration( name );
	if ( !description )
		Host_Error( "CL_Parse_DeltaDescription:  Unregistered description name %s\n", name );
	if ( *description )
		DELTA_FreeDescription( description );
	MSG_StartBitReading( &net_message );
	int fieldCount = static_cast<int>( MSG_ReadBits( DELTA_DESCRIPTION_COUNT_BITS ) );
	*description = static_cast<delta_t*>( Mem_ZeroMalloc( sizeof( delta_t ) ) );
	delta_description_t* fields =
		static_cast<delta_description_t*>( Mem_ZeroMalloc( sizeof( delta_description_t ) * fieldCount ) );
	( *description )->dynamic = 1;
	( *description )->fieldCount = fieldCount;
	for ( int field = 0; field < fieldCount; ++field )
	{
		DELTA_ParseDelta( reinterpret_cast<byte*>( &nullDescription ), reinterpret_cast<byte*>( &fields[field] ),
			&g_MetaDelta, sizeof( delta_description_t ) );
	}
	( *description )->pdd = fields;
	MSG_EndBitReading( &net_message );
}

void CL_ShutDownUsrMessages()
{
	if ( gClientUserMsgs )
	{
		UserMsg* message = gClientUserMsgs;
		while ( message )
		{
			UserMsg* next = message->next;
			Mem_Free( message );
			message = next;
		}
		gClientUserMsgs = nullptr;
	}
}

pfnUserMsgHook HookServerMsg( const char* name, pfnUserMsgHook hook )
{
	UserMsg* previous = nullptr;
	for ( UserMsg* message = gClientUserMsgs; message; message = message->next )
	{
		if ( Q_strcasecmp( name, message->szName ) == 0 )
		{
			previous = message;
			if ( message->pfn == hook )
				return message->pfn;
		}
	}
	UserMsg* message = static_cast<UserMsg*>( Mem_ZeroMalloc( sizeof( UserMsg ) ) );
	if ( previous )
		Q_memcpy( message, previous, sizeof( *message ) );
	else
	{
		Q_strncpy( message->szName, name, sizeof( message->szName ) - 1 );
		message->szName[sizeof( message->szName ) - 1] = '\0';
	}
	message->pfn = hook;
	message->next = gClientUserMsgs;
	gClientUserMsgs = message;
	return nullptr;
}

void CL_ClearResourceList( resource_t* list )
{
	resource_t* resource = list->pNext;
	if ( resource )
	{
		while ( resource && resource != list )
		{
			resource_t* next = resource->pNext;
			CL_RemoveFromResourceList( resource );
			Mem_Free( resource );
			resource = next;
		}
	}
	list->pPrev = list->pNext = list;
}

void CL_ClearResourceLists()
{
	cl.downloadUrl[0] = 0;
	CL_ClearResourceList( &cl.resourcesneeded );
	CL_ClearResourceList( &cl.resourcesonhand );
}

qboolean CL_CheckFile( sizebuf_t* message, char* source )
{
	char filename[MAX_PATH];
	Q_strncpy( filename, source, sizeof( filename ) - 1 );
	filename[sizeof( filename ) - 1] = 0;
	if ( cl_allow_download.value == 0.0f )
	{
		Con_DPrintf( "Download refused, cl_allow_download is 0\n" );
		return true;
	}
	if ( cls.state == ca_active && cl_download_ingame.value == 0.0f )
	{
		Con_DPrintf( "In-game download refused...\n" );
		return true;
	}
	if ( FS_FileExists( filename ) )
		return true;
	if ( cls.passive || cls.demoplayback )
	{
		Con_Printf( "Warning! File %s missing during demo playback.\n", filename );
		return true;
	}
	if ( CL_CanUseHTTPDownload() )
		CL_QueueHTTPDownload( filename );
	else
	{
		MSG_WriteByte( message, clc_stringcmd );
		MSG_WriteString( message, va( "dlfile %s", filename ) );
	}
	return false;
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void CL_BatchResourceRequest()
{
	constexpr int RESOURCE_BATCH_BYTES = 65536;
	constexpr int DOWNLOAD_COMMAND_FILENAME_BYTES = 256;
	byte data[RESOURCE_BATCH_BYTES];
	sizebuf_t message;
	Q_memset( &message, 0, sizeof( message ) );
	message.buffername = "Client to Server Resource Batch";
	message.data = data;
	message.maxsize = sizeof( data );
	message.cursize = 0;
	CL_HTTPStop_f();
	resource_t* resource = cl.resourcesneeded.pNext;
	while ( resource && resource != &cl.resourcesneeded )
	{
		resource_t* next = resource->pNext;
		if ( !( resource->ucFlags & RES_WASMISSING ) )
			CL_MoveToOnHandList( resource );
		else if ( cls.state == ca_active && cl_download_ingame.value == 0.0f )
		{
			Con_Printf( "Skipping in game download of %s\n", resource->szFileName );
			CL_MoveToOnHandList( resource );
		}
		else if ( !IsSafeFileToDownload( resource->szFileName ) )
		{
			CL_RemoveFromResourceList( resource );
			Con_Printf( "Invalid file type...skipping download of %s\n", resource->szFileName );
			Mem_Free( resource );
		}
		else
		{
			qboolean onHand = false;
			switch ( resource->type )
			{
			case t_sound:
				onHand =
					resource->szFileName[0] == '*' || CL_CheckFile( &message, va( "sound/%s", resource->szFileName ) );
				break;
			case t_skin:
				onHand = true;
				break;
			case t_model:
				onHand = resource->szFileName[0] == '*' || CL_CheckFile( &message, resource->szFileName );
				break;
			case t_decal:
				onHand = HPAK_GetDataPointer( "custom.hpk", resource, nullptr, nullptr );
				if ( !onHand && !( resource->ucFlags & RES_REQUESTED ) )
				{
					char filename[DOWNLOAD_COMMAND_FILENAME_BYTES];
					_snprintf( filename, sizeof( filename ), "!MD5%s",
						COM_BinPrintf( resource->rgucMD5_hash, sizeof( resource->rgucMD5_hash ) ) );
					MSG_WriteByte( &message, clc_stringcmd );
					MSG_WriteString( &message, va( "dlfile %s", filename ) );
					resource->ucFlags |= RES_REQUESTED;
				}
				break;
			case t_generic:
			case t_eventscript:
				onHand = CL_CheckFile( &message, resource->szFileName );
				break;
			default:
				break;
			}
			if ( onHand )
				CL_MoveToOnHandList( resource );
		}
		resource = next;
	}
	if ( CL_GetDownloadQueueSize() > 0 )
	{
		char downloadUrl[MAX_DOWNLOAD_URL];
		Q_memcpy( downloadUrl, cl.downloadUrl, sizeof( downloadUrl ) );
		CL_MarkMapAsUsingHTTPDownload();
		CL_Disconnect();
		StartLoadingProgressBar( downloadUrl, CL_GetDownloadQueueSize() );
		SetLoadingProgressBarStatusText( "#GameUI_VerifyingAndDownloading" );
	}
	if ( cls.state == ca_disconnected )
		return;
	if ( !message.cursize && CL_PrecacheResources() )
		CL_RegisterResources( &message );
	if ( !cls.passive )
	{
		Netchan_CreateFragments( false, &cls.netchan, &message );
		Netchan_FragSend( &cls.netchan );
	}
}

qboolean CL_CheckCRCs( char* map )
{
	constexpr int CHECK_CRC_PROGRESS = 4;
	if ( fs_startup_timings.value != 0.0f )
		AddStartupTiming( "begin CL_CheckCRCs()" );
	ContinueLoadingProgressBar( "ClientConnect", CHECK_CRC_PROGRESS, 0.0f );
	SetLoadingProgressBarStatusText( "#GameUI_CheckCRCs" );
	if ( !Host_IsServerActive() )
	{
		CRC32_Init( &cl.mapCRC );
		if ( CRC_MapFile( &cl.mapCRC, map ) )
		{
			if ( cl.serverCRC != cl.mapCRC && !cls.demoplayback )
			{
				COM_ExplainDisconnection( true, "Your map [%s] differs from the server's.", map );
				Host_Error( "Disconnected" );
			}
		}
		else
		{
			if ( FS_FileExists( map ) )
			{
				COM_ExplainDisconnection( true, "#GameUI_ConnectionFailed", map );
				Host_Error( "Disconnected" );
			}
			if ( cl_allow_download.value == 0.0f )
			{
				COM_ExplainDisconnection(
					true, "Refusing to download map %s, (cl_allow_download is 0 ) disconnecting.\n", map );
				Host_Error( "Disconnected" );
			}
			Con_Printf( "Couldn't find map %s, server will download the map\n", map );
			cl.mapCRC = cl.serverCRC;
		}
	}
	if ( gmodinfo.clientcrccheck )
	{
		char dllName[MAX_QPATH]; // Client side DLL being used.
		_snprintf( dllName, sizeof( dllName ), "cl_dlls//client.dll" );
		if ( !MD5_Hash_File( cls.md5_clientdll, dllName, false, false, nullptr ) )
		{
			COM_ExplainDisconnection( true, "Couldn't CRC client side dll %s.", dllName );
			Host_Error( "Disconnected" );
		}
		if ( Q_memcmp( cl.clientdllmd5, cls.md5_clientdll, sizeof( cl.clientdllmd5 ) ) != 0 )
		{
			if ( !cls.demoplayback && !cls.spectator )
			{
				COM_ExplainDisconnection( true, "Your .dll [%s] differs from the server's.", dllName );
				Host_Error( "Disconnected" );
			}
			Con_Printf(
				"Your client side .dll [%s] failed the CRC check.\n"
				"The .dll may be out of date, or the demo may have been recorded using an old version of the .dll.\n"
				"Consider obtaining an updated version of the .dll from the server operator.\n"
				"Demo playback proceeding.\n",
				dllName );
		}
	}
	if ( fs_startup_timings.value != 0.0f )
		AddStartupTiming( "end   CL_CheckCRCs()" );
	return true;
}

void CL_ParseServerInfo()
{
	constexpr int PARSE_SERVERINFO_PROGRESS = 3;
	constexpr float LOADING_COMPLETE_PERCENT = 100.0f;
	if ( fs_startup_timings.value != 0.0f )
		AddStartupTiming( "begin CL_ParseServerInfo()" );
	HPAK_CheckSize( const_cast<char*>( "custom" ) );
	Con_DPrintf( "Serverinfo packet received.\n" );
	ContinueLoadingProgressBar( "ClientConnect", PARSE_SERVERINFO_PROGRESS, 0.0f );
	SetLoadingProgressBarStatusText( "#GameUI_ParseServerInfo" );
	if ( cls.passive )
		NET_LeaveGroup( cls.netchan.sock, cls.connect_stream );
	CL_ClearState( false );
	ClientDLL_HudVidInit();
	cls.demowaiting = false;
	int protocol = MSG_ReadLong();
	if ( protocol != PROTOCOL_VERSION && !cls.demoplayback )
	{
		Con_Printf( "Server returned version %i, not %i\n", protocol, PROTOCOL_VERSION );
		return;
	}
	// Parse servercount (i.e., # of servers spawned since server .exe started)
	// So that we can detect new server startup during download, etc.
	cl.servercount = MSG_ReadLong();
	if ( cls.demoplayback )
		cl.servercount = ++gHostSpawnCount;
	// The CRC of the server map must match the CRC of the client map. or else
	//  the client is probably cheating.
	cl.serverCRC = MSG_ReadLong();
	MSG_ReadBuf( sizeof( cl.clientdllmd5 ), cl.clientdllmd5 );
	cl.maxclients = MSG_ReadByte();
	if ( cl.maxclients < 1 || cl.maxclients > MAX_CLIENTS )
	{
		Con_Printf( "Bad maxclients (%u) from server\n", cl.maxclients );
		return;
	}
	Cvar_SetValue( const_cast<char*>( "r_decals" ), cl.maxclients > 1 ? mp_decals.value : sp_decals.value );
	R_ForceCVars( cl.maxclients > 1 );
	if ( cl_entities )
	{
		Mem_Free( cl_entities );
		cl_entities = nullptr;
	}
	R_DestroyObjects();
	CL_ReallocateDynamicData( cl.maxclients );
	cl.playernum = MSG_ReadByte();
	COM_UnMunge3(
		reinterpret_cast<byte*>( &cl.serverCRC ), sizeof( cl.serverCRC ), static_cast<byte>( ~cl.playernum ) );
	for ( int i = 0; i < MAX_CLIENTS; ++i )
		COM_ClearCustomizationList( &cl.players[i].customdata, true );
	cl.players[cl.playernum].customdata.pNext = nullptr;
	for ( int i = 0; i < cl.num_resources; ++i )
	{
		if ( !COM_CreateCustomization(
				 &cl.players[cl.playernum].customdata, &cl.resourcelist[i], cl.playernum, 0, nullptr, nullptr ) )
			Con_DPrintf( "Problem with client customization %i, ignoring...", i );
	}
	cl.gametype = MSG_ReadByte();
	if ( !CL_CheckGameDirectory( MSG_ReadString() ) )
		return;
	Con_DPrintf( "Remote host:  %s\n", MSG_ReadString() );
	Q_strncpy( cl.levelname, MSG_ReadString(), sizeof( cl.levelname ) - 1 );
	cl.levelname[sizeof( cl.levelname ) - 1] = 0;
	MSG_SkipString();
	char mapName[MAX_PATH];
	COM_FileBase( cl.levelname, mapName );
	if ( cl.maxclients > 1 )
	{
		FS_LogLevelLoadStarted( mapName );
		VGuiWrap2_LoadingStarted( "level", mapName );
	}
	cls.isVAC2Secure = MSG_ReadByte();
	// Verify the map and player .mdl crc's before requesting rest of signon
	if ( CL_CheckCRCs( cl.levelname ) )
	{
		gHostSpawnCount = cl.servercount;
		cls.state = ca_connected;
		CL_BeginDemoStartup();
		SetSecondaryProgressBar( LOADING_COMPLETE_PERCENT );
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_WriteString( &cls.netchan.message, "sendres" );
		if ( fs_startup_timings.value != 0.0f )
			AddStartupTiming( "end   CL_ParseServerInfo()" );
		Steam_UpdateRichPresence(); // make sure nothing is hurt
	}
}

void CL_ParseResourceLocation()
{
	const char* location = MSG_ReadString();
	if ( !location
		 || ( _strnicmp( "http://", location, sizeof( "http://" ) - 1 ) != 0
			  && _strnicmp( "https://", location, sizeof( "https://" ) - 1 ) != 0 ) )
		return;
	const char* slash = std::strrchr( location, '/' );
	if ( !slash || slash[1] )
		_snprintf( cl.downloadUrl, sizeof( cl.downloadUrl ), "%s/", location );
	else
	{
		std::strncpy( cl.downloadUrl, location, sizeof( cl.downloadUrl ) );
		cl.downloadUrl[sizeof( cl.downloadUrl ) - 1] = 0;
	}
	Con_DPrintf( "Using %s as primary download location\n", cl.downloadUrl );
}

void CL_ParseConsistencyInfo()
{
	constexpr int CONSISTENCY_DELTA_BITS = 5;
	constexpr int CONSISTENCY_INDEX_BITS = 10;
	if ( fs_startup_timings.value != 0.0f )
		AddStartupTiming( "begin CL_ParseConsistencyInfo()" );
	byte nullBuffer[sizeof( resource_t::rguc_reserved )] = {};
	resource_t* resource = cl.resourcesneeded.pNext;
	resource_t* previousResource = nullptr;
	std::uint32_t previousIndex = 0;
	cl.need_force_consistency_response = MSG_ReadBits( 1 ) != 0;
	if ( !cl.need_force_consistency_response )
		return;
	while ( MSG_ReadBits( 1 ) )
	{
		std::uint32_t index;
		if ( MSG_ReadBits( 1 ) )
			index = previousIndex + MSG_ReadBits( CONSISTENCY_DELTA_BITS );
		else
			index = MSG_ReadBits( CONSISTENCY_INDEX_BITS );
		int advance = static_cast<int>( index - previousIndex );
		previousIndex = index;
		while ( advance > 0 )
		{
			if ( resource != previousResource && std::strstr( resource->szFileName, "models/" ) )
				Mod_NeedCRC( resource->szFileName, false );
			resource = resource->pNext;
			--advance;
		}
		previousResource = resource;
		if ( std::strstr( resource->szFileName, "models/" ) )
			Mod_NeedCRC( resource->szFileName, true );
		if ( cl.num_consistency >= MAX_CONSISTENCY_FILES )
			Host_Error( "Consistency:  server sent too many filenames\n" );
		consistency_t& consistency = cl.consistency_list[cl.num_consistency++];
		Q_memset( &consistency, 0, sizeof( consistency ) );
		consistency.filename = resource->szFileName;
		consistency.orig_index = static_cast<int>( index );
		consistency.issound = resource->type == t_sound;
		consistency.value = 0;
		if ( resource->type == t_model && Q_memcmp( nullBuffer, resource->rguc_reserved, sizeof( nullBuffer ) ) != 0 )
		{
			COM_UnMunge( resource->rguc_reserved, sizeof( resource->rguc_reserved ), cl.servercount );
			consistency.check_type = resource->rguc_reserved[0];
		}
	}
	if ( fs_startup_timings.value != 0.0f )
		AddStartupTiming( "end   CL_ParseConsistencyInfo()" );
}

int CL_EstimateNeededResources()
{
	std::uint32_t total = 0;
	for ( resource_t* resource = cl.resourcesneeded.pNext; resource != &cl.resourcesneeded; resource = resource->pNext )
	{
		bool missing = false;
		switch ( resource->type )
		{
		case t_sound:
			if ( resource->szFileName[0] != '*' )
			{
				int size = static_cast<int>( FS_FileSize( va( "sound/%s", resource->szFileName ) ) );
				missing = size == 0 || size == -1;
			}
			break;
		case t_model:
			if ( resource->szFileName[0] == '*' || resource->ucFlags == 0 )
				break;
			[[fallthrough]];
		case t_skin:
		case t_generic:
		case t_eventscript:
		{
			int size = static_cast<int>( FS_FileSize( resource->szFileName ) );
			missing = size == 0 || size == -1;
			break;
		}
		case t_decal:
			missing = ( resource->ucFlags & RES_CUSTOM ) != 0;
			break;
		default:
			break;
		}
		if ( missing )
		{
			resource->ucFlags |= RES_WASMISSING;
			total += static_cast<std::uint32_t>( resource->nDownloadSize );
		}
	}
	return static_cast<int>( total );
}

void CL_StartResourceDownloading( char* message, qboolean custom )
{
	if ( fs_startup_timings.value != 0.0f )
		AddStartupTiming( "begin CL_StartResourceDownloading()" );
	if ( message )
		Con_DPrintf( "%s", message );
	resourceinfo_t info;
	cls.dl.nTotalSize = COM_SizeofResourceList( &cl.resourcesneeded, &info );
	int needed = CL_EstimateNeededResources();
	cls.dl.nTotalToTransfer = needed;
	if ( custom )
		cls.dl.custom = true;
	else
	{
		cls.state = ca_uninitialized;
		cls.dl.custom = false;
		gfExtendedError = false;
	}
	cls.dl.doneregistering = false;
	cls.dl.nRemainingToTransfer = needed;
	cls.dl.fLastStatusUpdate = static_cast<float>( realtime );
	Q_memset( cls.dl.rgStats, 0, sizeof( cls.dl.rgStats ) );
	cls.dl.nCurStat = 0;
	if ( fs_startup_timings.value != 0.0f )
		AddStartupTiming( "end   CL_StartResourceDownloading()" );
	CL_BatchResourceRequest();
}

void CL_ParseResourceList()
{
	constexpr int RESOURCE_COUNT_BITS = 12;
	constexpr int RESOURCE_TYPE_BITS = 4;
	constexpr int RESOURCE_INDEX_BITS = 12;
	constexpr int RESOURCE_SIZE_BITS = 24;
	constexpr int RESOURCE_FLAGS_BITS = 3;
	if ( fs_startup_timings.value != 0.0f )
		AddStartupTiming( "begin CL_ParseResourceList()" );
	MSG_StartBitReading( &net_message );
	int count = static_cast<int>( MSG_ReadBits( RESOURCE_COUNT_BITS ) );
	for ( int i = 0; i < count; ++i )
	{
		resource_t* resource = static_cast<resource_t*>( Mem_ZeroMalloc( sizeof( resource_t ) ) );
		resource->type = static_cast<resourcetype_t>( MSG_ReadBits( RESOURCE_TYPE_BITS ) );
		Q_strncpy( resource->szFileName, MSG_ReadBitString(), sizeof( resource->szFileName ) - 1 );
		resource->szFileName[sizeof( resource->szFileName ) - 1] = 0;
		resource->nIndex = static_cast<int>( MSG_ReadBits( RESOURCE_INDEX_BITS ) );
		resource->nDownloadSize = static_cast<int>( MSG_ReadBits( RESOURCE_SIZE_BITS ) );
		resource->ucFlags = static_cast<byte>( MSG_ReadBits( RESOURCE_FLAGS_BITS ) & ~RES_WASMISSING );
		if ( resource->ucFlags & RES_CUSTOM )
			MSG_ReadBitData( resource->rgucMD5_hash, sizeof( resource->rgucMD5_hash ) );
		if ( MSG_ReadBits( 1 ) )
			MSG_ReadBitData( resource->rguc_reserved, sizeof( resource->rguc_reserved ) );
		CL_AddToResourceList( resource, &cl.resourcesneeded );
	}
	if ( fs_startup_timings.value != 0.0f )
		AddStartupTiming( "end   CL_ParseResourceList()" );
	CL_ParseConsistencyInfo();
	MSG_EndBitReading( &net_message );
	SetLoadingProgressBarStatusText( "#GameUI_VerifyingResources" );
	CL_StartResourceDownloading( const_cast<char*>( "Verifying and downloading resources...\n" ), false );
}

void CL_Restore( const char* fileName )
{
	constexpr unsigned int SAVEGAME_HEADER = 'V' | ( 'A' << 8 ) | ( 'L' << 16 ) | ( 'V' << 24 );
	constexpr int RESTORE_FILE_BUFFER_BYTES = 4096;
	FileHandle_t file = FS_Open( fileName, "rb" );
	if ( file )
	{
		FS_SetVBuf( file, nullptr, _IOFBF, RESTORE_FILE_BUFFER_BYTES );
		unsigned int header;
		int version;
		FS_Read( &header, sizeof( header ), 1, file );
		FS_Read( &version, sizeof( version ), 1, file );
		if ( header == SAVEGAME_HEADER )
		{
			int decalCount;
			FS_Read( &decalCount, sizeof( decalCount ), 1, file );
			for ( int i = 0; i < decalCount; ++i )
			{
				char name[DECAL_NAME_BYTES];
				DECALLIST decalList;
				decalList.name = name;
				FS_Read( name, sizeof( name ), 1, file );
				FS_Read( &decalList.entityIndex, sizeof( decalList.entityIndex ), 1, file );
				FS_Read( &decalList.depth, sizeof( decalList.depth ), 1, file );
				FS_Read( &decalList.flags, sizeof( decalList.flags ), 1, file );
				FS_Read( decalList.position, sizeof( decalList.position ), 1, file );
				if ( r_decals.value != 0.0f )
				{
					int index = Draw_DecalIndexFromName( decalList.name );
					R_DecalShoot(
						Draw_DecalIndex( index ), decalList.entityIndex, 0, decalList.position, decalList.flags );
				}
			}
		}
		FS_Close( file );
	}
	int mapCount = MSG_ReadByte();
	for ( int i = 0; i < mapCount; ++i )
		Con_Printf( "Loading decals from %s\n", MSG_ReadString() );
}

void CL_SendConsistencyInfo( sizebuf_t* message )
{
	constexpr int CONSISTENCY_RESOURCE_BITS = 12;
	constexpr int CONSISTENCY_HASH_BITS = 32;
	constexpr float INVALID_MODEL_BOUNDS = 9999.9f;
	constexpr float MISSING_MODEL_BOUNDS = -1.0f;
	qboolean runningSteam = COM_CheckParm( "-steam" );
	if ( !cl.need_force_consistency_response )
		return;
	cl.need_force_consistency_response = false;
	MSG_WriteByte( message, clc_fileconsistency );
	int lengthPosition = message->cursize;
	MSG_WriteShort( message, 0 );
	MSG_StartBitWriting( message );
	for ( int i = 0; i < cl.num_consistency; ++i )
	{
		consistency_t& consistency = cl.consistency_list[i];
		MSG_WriteBits( 1, 1 );
		MSG_WriteBits( consistency.orig_index, CONSISTENCY_RESOURCE_BITS );
		char filename[MAX_PATH];
		if ( consistency.issound )
			_snprintf( filename, sizeof( filename ), "sound/%s", consistency.filename );
		else
		{
			Q_strncpy( filename, consistency.filename, sizeof( filename ) - 1 );
			filename[sizeof( filename ) - 1] = 0;
		}
		qboolean available = true;
		if ( runningSteam )
		{
			char steamFilename[MAX_PATH];
			_snprintf( steamFilename, sizeof( steamFilename ), "%s", filename );
			COM_FixSlashes( steamFilename );
			available = FS_FileExists( steamFilename ) != 0;
		}
		qboolean invalidModel = false;
		if ( std::strstr( filename, "models/" ) && available )
		{
			CRC32_t crc;
			CRC32_Init( &crc );
			CRC_File( &crc, filename );
			crc = CRC32_Final( crc );
			invalidModel = !Mod_ValidateCRC( filename, crc );
		}
		switch ( consistency.check_type )
		{
		case force_exactfile:
		{
			byte digest[MD5_DIGEST_BYTES];
			MD5_Hash_File( digest, filename, false, false, nullptr );
			std::uint32_t hash;
			Q_memcpy( &hash, digest, sizeof( hash ) );
			consistency.value = static_cast<int>( hash );
			MSG_WriteBits( invalidModel ? 0 : hash, CONSISTENCY_HASH_BITS );
			break;
		}
		case force_model_samebounds:
		case force_model_specifybounds:
		case force_model_specifybounds_if_avail:
		{
			vec3_t mins;
			vec3_t maxs;
			if ( consistency.check_type == force_model_specifybounds_if_avail && !available )
			{
				for ( int axis = 0; axis < 3; ++axis )
					mins[axis] = maxs[axis] = MISSING_MODEL_BOUNDS;
			}
			else
			{
				if ( !R_GetStudioBounds( filename, mins, maxs ) )
					Host_Error( "Unable to find %s\n", filename );
				if ( invalidModel )
				{
					for ( int axis = 0; axis < 3; ++axis )
					{
						mins[axis] = -INVALID_MODEL_BOUNDS;
						maxs[axis] = INVALID_MODEL_BOUNDS;
					}
				}
			}
			MSG_WriteBitData( mins, sizeof( mins ) );
			MSG_WriteBitData( maxs, sizeof( maxs ) );
			break;
		}
		default:
			Host_Error( "Unknown consistency type %i\n", consistency.check_type );
			break;
		}
	}
	MSG_WriteBits( 0, 1 );
	MSG_EndBitWriting( message );
	int dataPosition = lengthPosition + sizeof( std::uint16_t );
	int length = message->cursize - dataPosition;
	std::uint16_t wireLength = static_cast<std::uint16_t>( length );
	Q_memcpy( message->data + lengthPosition, &wireLength, sizeof( wireLength ) );
	COM_Munge( message->data + dataPosition, length, cl.servercount );
}

void CL_DumpMessageLoad_f()
{
	unsigned int total = 0;
	Con_Printf( "-------- Message Load ---------\n" );
	for ( int bucket = 0; bucket < MESSAGE_USER_BUCKET; ++bucket )
	{
		if ( msg_buckets[bucket] )
		{
			const char* name = bucket <= svc_lastmsg ? cl_parsefuncs[bucket].pszname : "bogus #";
			float kilobytes = static_cast<float>( total_data[bucket] ) * MESSAGE_BYTES_TO_KILOBYTES;
			Con_Printf( "%i:%s: %i msgs:%.2fK\n", bucket, name, msg_buckets[bucket], kilobytes );
			total += static_cast<unsigned int>( msg_buckets[bucket] );
		}
	}
	float kilobytes = static_cast<float>( total_data[MESSAGE_USER_BUCKET] ) * MESSAGE_BYTES_TO_KILOBYTES;
	Con_Printf( "User messages:  %i:%.2fK\n", msg_buckets[MESSAGE_USER_BUCKET], kilobytes );
	total += static_cast<unsigned int>( msg_buckets[MESSAGE_USER_BUCKET] );
	Con_Printf( "------ End:  %i Total----\n", static_cast<int>( total ) );
}

qboolean CL_CheckGameDirectory( char* gamedir )
{
	if ( !gamedir || !gamedir[0] )
	{
		Con_Printf( "Server didn't specify a gamedir, assuming no change\n" );
		return true;
	}
	char directory[MAX_PATH];
	// Rip out the current gamedir.
	COM_FileBase( com_gamedir, directory );
	if ( Q_strcasecmp( directory, gamedir ) )
	{
		Con_Printf( "Server is running game %s.  Restart in that game to connect.\n", gamedir );
		CL_Disconnect_f();
		return false;
	}
	return true;
}

void CL_Parse_Sound()
{
	constexpr int SOUND_FLAG_VOLUME = 1 << 0;
	constexpr int SOUND_FLAG_ATTENUATION = 1 << 1;
	constexpr int SOUND_FLAG_LARGE_INDEX = 1 << 2;
	constexpr int SOUND_FLAG_PITCH = 1 << 3;
	constexpr int SOUND_FLAGS_BITS = 9;
	constexpr int SOUND_BYTE_BITS = 8;
	constexpr int SOUND_CHANNEL_BITS = 3;
	constexpr int SOUND_ENTITY_BITS = 11;
	constexpr int SOUND_LARGE_INDEX_BITS = 16;
	MSG_StartBitReading( &net_message );
	int flags = static_cast<int>( MSG_ReadBits( SOUND_FLAGS_BITS ) );
	float volume = 1.0f;
	if ( flags & SOUND_FLAG_VOLUME )
		volume = static_cast<float>( static_cast<double>( MSG_ReadBits( SOUND_BYTE_BITS ) ) / SOUND_VOLUME_BYTE_MAX );
	float attenuation = 1.0f;
	if ( flags & SOUND_FLAG_ATTENUATION )
		attenuation =
			static_cast<float>( MSG_ReadBits( SOUND_BYTE_BITS ) ) * static_cast<float>( SOUND_ATTENUATION_BYTE_SCALE );
	int channel = static_cast<int>( MSG_ReadBits( SOUND_CHANNEL_BITS ) );
	int entity = static_cast<int>( MSG_ReadBits( SOUND_ENTITY_BITS ) );
	int soundIndex = static_cast<int>(
		MSG_ReadBits( ( flags & SOUND_FLAG_LARGE_INDEX ) ? SOUND_LARGE_INDEX_BITS : SOUND_BYTE_BITS ) );
	vec3_t origin;
	MSG_ReadBitVec3Coord( origin );
	int pitch = PITCH_NORM;
	if ( flags & SOUND_FLAG_PITCH )
		pitch = static_cast<int>( MSG_ReadBits( SOUND_BYTE_BITS ) );
	MSG_EndBitReading( &net_message );
	if ( entity >= cl.max_edicts )
	{
		Con_DPrintf( "CL_Parse_Sound: ent = %i, cl.max_edicts %i\n", entity, cl.max_edicts );
		return;
	}
	sfx_t sentence;
	sfx_t* sound;
	if ( flags & SND_SENTENCE )
	{
		if ( soundIndex >= MAX_SENTENCES )
			_snprintf( sentence.name, sizeof( sentence.name ), "!#%d", soundIndex - MAX_SENTENCES );
		else
			_snprintf( sentence.name, sizeof( sentence.name ), "!%s", rgpszrawsentence[soundIndex] );
		sound = &sentence;
	}
	else
		sound = cl.sound_precache[soundIndex];
	if ( channel == CHAN_STATIC )
		S_StartStaticSound( entity, channel, sound, origin, volume, attenuation, flags, pitch );
	else
		S_StartDynamicSound( entity, channel, sound, origin, volume, attenuation, flags, pitch );
}

void CL_PlayerDropped( int playerNumber )
{
	COM_ClearCustomizationList( &cl.players[playerNumber].customdata, true );
}

qboolean CL_RequestMissingResources()
{
	qboolean done = cls.dl.doneregistering;
	if ( done )
		return false;
	if ( cls.dl.custom || cls.state == ca_uninitialized )
	{
		if ( cl.resourcesneeded.pNext == &cl.resourcesneeded )
		{
			cls.dl.custom = false;
			cls.dl.doneregistering = true;
		}
		else if ( ( cl.resourcesneeded.pNext->ucFlags & RES_WASMISSING ) == 0 )
		{
			CL_MoveToOnHandList( cl.resourcesneeded.pNext );
			return true;
		}
	}
	return done;
}

void CL_CreateCustomizationList()
{
	int player = cl.playernum;
	cl.players[player].customdata.pNext = nullptr;
	if ( cl.num_resources <= 0 )
		return;
	customization_t* head = &cl.players[player].customdata;
	resource_t* resource = cl.resourcelist;
	for ( int index = 0; index < cl.num_resources; ++index, ++resource )
	{
		if ( !COM_CreateCustomization( head, resource, cl.playernum, 0, nullptr, nullptr ) )
			Con_DPrintf( "Problem with client customization %i, ignoring...", index );
	}
}

int CL_BuildMapCycleListHints( char** hints, char* mapCycleMsg, char* firstMap )
{
	constexpr int MAP_HINT_MOD_BYTES = 260;
	constexpr int MAP_HINT_PATH_BYTES = 524;
	constexpr int MAP_HINT_NAME_LIMIT = 523;
	char mod[MAP_HINT_MOD_BYTES];
	char mapPath[MAP_HINT_PATH_BYTES];
	char mapLine[MAP_HINT_PATH_BYTES];
	std::size_t length = std::strlen( mapCycleMsg );
	COM_FileBase( com_gamedir, mod );
	if ( !mapCycleMsg || length == 0 )
		return 1;
	const char* earlierAppName = GetCurrentSteamAppName();
	const char* laterAppName = GetCurrentSteamAppName();
	if ( std::snprintf( mapPath, sizeof( mapPath ), "%s\\%s\\%s%s\r\n%s\\%s\\%s%s\r\n", szReslistsBaseDir, laterAppName,
			 szCommonPreloads, szReslistsExt, szReslistsBaseDir, earlierAppName, firstMap, szReslistsExt )
		 >= static_cast<int>( sizeof( mapPath ) ) )
		std::abort();
	char* built = static_cast<char*>( Mem_ZeroMalloc( std::strlen( mapPath ) + 1 ) );
	*hints = built;
	if ( !built )
		return 0;
	std::strcpy( built, mapPath );
	for ( ;; )
	{
		mapCycleMsg = const_cast<char*>( COM_Parse( mapCycleMsg ) );
		if ( com_token[0] == 0 )
			break;
		std::strncpy( mapPath, com_token, MAP_HINT_NAME_LIMIT );
		mapPath[MAP_HINT_NAME_LIMIT] = 0;
		if ( COM_TokenWaiting( mapCycleMsg ) )
			mapCycleMsg = const_cast<char*>( COM_Parse( mapCycleMsg ) );
		if ( PF_IsMapValid_I( mapPath ) == 0 || std::strcmp( firstMap, mapPath ) == 0 )
			continue;
		const char* appName = GetCurrentSteamAppName();
		if ( std::snprintf(
				 mapLine, sizeof( mapLine ), "%s\\%s\\%s%s\r\n", szReslistsBaseDir, appName, mapPath, szReslistsExt )
			 >= static_cast<int>( sizeof( mapLine ) ) )
			std::abort();
		std::size_t hintLength = std::strlen( *hints );
		char* grown = static_cast<char*>( Mem_Realloc( *hints, std::strlen( mapLine ) + hintLength + 2 ) );
		*hints = grown;
		if ( !grown )
			return 0;
		std::strcat( grown, mapLine );
	}
	const char* closingAppName = GetCurrentSteamAppName();
	if ( std::snprintf( mapPath, sizeof( mapPath ), "%s\\%s\\mp_maps.txt\r\n", szReslistsBaseDir, closingAppName )
		 >= static_cast<int>( sizeof( mapPath ) ) )
		std::abort();
	std::size_t hintLength = std::strlen( *hints );
	char* grown = static_cast<char*>( Mem_Realloc( *hints, std::strlen( mapPath ) + hintLength + 2 ) );
	*hints = grown;
	std::strcat( grown, mapPath );
	return 1;
}

void CL_ParseListen()
{
	cls.signon = SIGNON_PRESPAWN;
	NET_StringToAdr( MSG_ReadString(), &cls.game_stream );
	NET_JoinGroup( cls.netchan.sock, cls.game_stream );
	SCR_EndLoadingPlaque();
}
