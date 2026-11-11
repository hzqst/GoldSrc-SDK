#include "server.h"
#include "sv_user.h"
#include "render.h"
#include "conprint.h"
#include "crc.h"
#include "host.h"
#include "keys.h"
#include "pm_defs.h"
#include "sys.h"
#include "cmd.h"
#include "common.h"
#include "client.h"
#include "world.h"
#include "gl_model.h"
#include "cvar.h"
#include "engine_edict.h"
#include "protocol.h"
#include "eiface.h"

#include <cmath>
#include <cstdio>

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

void SV_ClientPrintf( const char* format, ... );
void SV_DropClient( client_t* client, qboolean crash, const char* format, ... );

cvar_t sv_edgefriction = { const_cast<char*>( "edgefriction" ), const_cast<char*>( "2" ), FCVAR_SERVER, 0.0f, nullptr };
cvar_t sv_maxspeed = { const_cast<char*>( "sv_maxspeed" ), const_cast<char*>( "320" ), FCVAR_SERVER, 0.0f, nullptr };
cvar_t sv_accelerate = { const_cast<char*>( "sv_accelerate" ), const_cast<char*>( "10" ), FCVAR_SERVER, 0.0f, nullptr };
cvar_t sv_footsteps = { const_cast<char*>( "mp_footsteps" ), const_cast<char*>( "1" ), FCVAR_SERVER, 0.0f, nullptr };
cvar_t sv_rollspeed = { const_cast<char*>( "sv_rollspeed" ), const_cast<char*>( "200" ), 0, 0.0f, nullptr };
cvar_t sv_rollangle = { const_cast<char*>( "sv_rollangle" ), const_cast<char*>( "2.0" ), 0, 0.0f, nullptr };
cvar_t sv_unlag = { const_cast<char*>( "sv_unlag" ), const_cast<char*>( "1" ), 0, 0.0f, nullptr };
cvar_t sv_maxunlag = { const_cast<char*>( "sv_maxunlag" ), const_cast<char*>( "0.5" ), 0, 0.0f, nullptr };
cvar_t sv_unlagpush = { const_cast<char*>( "sv_unlagpush" ), const_cast<char*>( "0.0" ), 0, 0.0f, nullptr };
cvar_t sv_unlagsamples = { const_cast<char*>( "sv_unlagsamples" ), const_cast<char*>( "1" ), 0, 0.0f, nullptr };
cvar_t mp_consistency = {
	const_cast<char*>( "mp_consistency" ), const_cast<char*>( "1" ), FCVAR_SERVER, 0.0f, nullptr };
cvar_t sv_voiceenable = {
	const_cast<char*>( "sv_voiceenable" ), const_cast<char*>( "1" ), FCVAR_ARCHIVE | FCVAR_SERVER, 0.0f, nullptr };

void SV_SendConsistencyList( sizebuf_t* )
{
	constexpr int CONSISTENCY_DELTA_BITS = 5;
	constexpr int CONSISTENCY_INDEX_BITS = 10;
	constexpr int CONSISTENCY_MAX_DELTA = ( 1 << CONSISTENCY_DELTA_BITS ) - 1;
	host_client->has_force_unmodified = 0;

	if ( svs.maxclients != 1 && mp_consistency.value != 0 && sv.num_consistency && !host_client->proxy )
	{
		host_client->has_force_unmodified = 1;
		MSG_WriteBits( 1, 1 );
		int previousIndex = 0;
		for ( int index = 0; index < sv.num_resources; ++index )
		{
			resource_t* resource = &sv.resourcelist[index];
			if ( resource && ( resource->ucFlags & RES_CHECKFILE ) )
			{
				int delta = index - previousIndex;
				MSG_WriteBits( 1, 1 );
				if ( delta <= CONSISTENCY_MAX_DELTA )
				{
					MSG_WriteBits( 1, 1 );
					MSG_WriteBits( delta, CONSISTENCY_DELTA_BITS );
				}
				else
				{
					MSG_WriteBits( 0, 1 );
					MSG_WriteBits( index, CONSISTENCY_INDEX_BITS );
				}
				previousIndex = index;
			}
		}
	}
	MSG_WriteBits( 0, 1 );
}

qboolean SV_FileInConsistencyList( const char* filename, consistency_t** entry )
{
	int index = 0;
	consistency_t* consistency = &sv.consistency_list[1];
	char* name = sv.consistency_list[0].filename;
	if ( name == nullptr )
		return false;
	while ( Q_stricmp( filename, name ) != 0 )
	{
		++consistency;
		name = consistency[-1].filename;
		++index;
		if ( name == nullptr )
			return false;
	}
	if ( entry != nullptr )
		*entry = &sv.consistency_list[index];
	return true;
}

void SV_ParseConsistencyResponse( client_t* )
{
	constexpr int RESOURCE_INDEX_BITS = 12;
	constexpr int RESOURCE_HASH_WORD_BITS = 32;
	constexpr int CONSISTENCY_MESSAGE_BYTES = 256;
	constexpr int RESOURCE_BOUNDS_MIN_OFFSET = sizeof( byte );
	constexpr int RESOURCE_BOUNDS_MAX_OFFSET = RESOURCE_BOUNDS_MIN_OFFSET + sizeof( vec3_t );
	constexpr float MISSING_MODEL_BOUNDS = -1.0f;
	byte nullBuffer[sizeof( resource_t::rguc_reserved )];
	Q_memset( nullBuffer, 0, sizeof( nullBuffer ) );
	int length = MSG_ReadShort();
	if ( length <= 0 || msg_readcount + length > net_message.cursize )
	{
		Con_Printf( "SV_ParseConsistencyResponse:  %s:%s sent invalid message length: %d\n", host_client->name,
			NET_AdrToString( host_client->netchan.remote_address ), length );
		SV_DropClient( host_client, 0, "Invalid ParseConsistency message" );
		msg_badread = 1;
		return;
	}
	COM_UnMunge( &net_message.data[msg_readcount], length, svs.spawncount );
	MSG_StartBitReading( &net_message );
	int dropClient = 0;
	int count = 0;
	qboolean invalidResource = 0;
	while ( MSG_ReadBits( 1 ) )
	{
		int index = static_cast<int>( MSG_ReadBits( RESOURCE_INDEX_BITS ) );
		if ( index < 0 || index >= sv.num_resources || !( sv.resourcelist[index].ucFlags & RES_CHECKFILE ) )
		{
			invalidResource = 1;
			break;
		}
		resource_t* resource = &sv.resourcelist[index];
		byte readBuffer[sizeof( resource->rguc_reserved )];
		Q_memcpy( readBuffer, resource->rguc_reserved, sizeof( readBuffer ) );
		if ( Q_memcmp( readBuffer, nullBuffer, sizeof( readBuffer ) ) == 0 )
		{
			std::uint32_t expected;
			Q_memcpy( &expected, resource->rgucMD5_hash, sizeof( expected ) );
			if ( MSG_ReadBits( RESOURCE_HASH_WORD_BITS ) != expected )
				dropClient = index + 1;
		}
		else
		{
			vec3_t clientMins;
			vec3_t clientMaxs;
			MSG_ReadBitData( clientMins, sizeof( clientMins ) );
			MSG_ReadBitData( clientMaxs, sizeof( clientMaxs ) );
			byte resourceBuffer[sizeof( resource->rguc_reserved )];
			Q_memcpy( resourceBuffer, resource->rguc_reserved, sizeof( resourceBuffer ) );
			COM_UnMunge( resourceBuffer, sizeof( resourceBuffer ), svs.spawncount );
			int checkType = resourceBuffer[0];
			if ( checkType != force_model_samebounds && checkType != force_model_specifybounds
				 && checkType != force_model_specifybounds_if_avail )
			{
				dropClient = index + 1;
				msg_badread = 1;
				break;
			}
			vec3_t mins;
			vec3_t maxs;
			Q_memcpy( mins, resourceBuffer + RESOURCE_BOUNDS_MIN_OFFSET, sizeof( mins ) );
			Q_memcpy( maxs, resourceBuffer + RESOURCE_BOUNDS_MAX_OFFSET, sizeof( maxs ) );
			if ( checkType == force_model_samebounds )
			{
				if ( !VectorCompare( clientMins, mins ) || !VectorCompare( clientMaxs, maxs ) )
					dropClient = index + 1;
			}
			else if ( checkType != force_model_specifybounds_if_avail || clientMins[0] != MISSING_MODEL_BOUNDS
					  || clientMins[1] != MISSING_MODEL_BOUNDS || clientMins[2] != MISSING_MODEL_BOUNDS
					  || clientMaxs[0] != MISSING_MODEL_BOUNDS || clientMaxs[1] != MISSING_MODEL_BOUNDS
					  || clientMaxs[2] != MISSING_MODEL_BOUNDS )
			{
				for ( int axis = 0; axis < 3; ++axis )
				{
					if ( mins[axis] > clientMins[axis] || clientMaxs[axis] > maxs[axis] )
					{
						dropClient = index + 1;
						break;
					}
				}
			}
		}
		if ( msg_badread )
			break;
		++count;
	}
	MSG_EndBitReading( &net_message );
	if ( invalidResource || dropClient < 0 || count != sv.num_consistency )
	{
		msg_badread = 1;
		Con_Printf( "SV_ParseConsistencyResponse:  %s:%s sent bad file data\n", host_client->name,
			NET_AdrToString( host_client->netchan.remote_address ) );
		SV_DropClient( host_client, 0, "Bad file data" );
		return;
	}
	if ( dropClient )
	{
		char dropMessage[CONSISTENCY_MESSAGE_BYTES];
		dropMessage[0] = 0;
		if ( gEntityInterface.pfnInconsistentFile(
				 host_client->edict, sv.resourcelist[dropClient - 1].szFileName, dropMessage ) )
		{
			if ( Q_strlen( dropMessage ) > 0 )
				SV_ClientPrintf( "%s", dropMessage );
			SV_DropClient( host_client, 0, "Bad file %s", dropMessage );
		}
	}
	else
	{
		host_client->has_force_unmodified = 0;
	}
}

qboolean g_balreadymoved;
edict_t* sv_player;
extern int net_drop;
void SV_RunCmd( usercmd_t* command, int randomSeed );

typedef struct clc_func_s
{
	byte opcode;
	// Display Name
	char* pszname;
	// Parse function
	void ( *pfnParse )( client_t* );
} clc_func_t;

static_assert( sizeof( clc_func_t ) == 12 );
static_assert( offsetof( clc_func_t, pszname ) == 4 );
static_assert( offsetof( clc_func_t, pfnParse ) == 8 );

void SV_ParseMove( client_t* sender );
void SV_ParseResourceList( client_t* );
void SV_IgnoreHLTV( client_t* );

extern const clc_func_t sv_clcfuncs[13] = { { clc_bad, const_cast<char*>( "clc_bad" ), nullptr },
	{ clc_nop, const_cast<char*>( "clc_nop" ), nullptr }, { clc_move, const_cast<char*>( "clc_move" ), SV_ParseMove },
	{ clc_stringcmd, const_cast<char*>( "clc_stringcmd" ), SV_ParseStringCommand },
	{ clc_delta, const_cast<char*>( "clc_delta" ), SV_ParseDelta },
	{ clc_resourcelist, const_cast<char*>( "clc_resourcelist" ), SV_ParseResourceList },
	{ clc_tmove, const_cast<char*>( "clc_tmove" ), nullptr },
	{ clc_fileconsistency, const_cast<char*>( "clc_fileconsistency" ), SV_ParseConsistencyResponse },
	{ clc_voicedata, const_cast<char*>( "clc_voicedata" ), SV_ParseVoiceData },
	{ clc_hltv, const_cast<char*>( "clc_hltv" ), SV_IgnoreHLTV },
	{ clc_cvarvalue, const_cast<char*>( "clc_cvarvalue" ), SV_ParseCvarValue },
	{ clc_cvarvalue2, const_cast<char*>( "clc_cvarvalue2" ), SV_ParseCvarValue2 },
	{ clc_endoflist, const_cast<char*>( "End of List" ), nullptr } };

extern playermove_t g_svmove;

typedef struct
{
	char* command;
} command_t;

command_t clcommands[] = { { const_cast<char*>( "status" ) }, { const_cast<char*>( "god" ) },
	{ const_cast<char*>( "notarget" ) }, { const_cast<char*>( "fly" ) }, { const_cast<char*>( "name" ) },
	{ const_cast<char*>( "noclip" ) }, { const_cast<char*>( "kill" ) }, { const_cast<char*>( "pause" ) },
	{ const_cast<char*>( "spawn" ) }, { const_cast<char*>( "new" ) }, { const_cast<char*>( "sendres" ) },
	{ const_cast<char*>( "dropclient" ) }, { const_cast<char*>( "kick" ) }, { const_cast<char*>( "ping" ) },
	{ const_cast<char*>( "dlfile" ) }, { const_cast<char*>( "nextdl" ) }, { const_cast<char*>( "setinfo" ) },
	{ const_cast<char*>( "showinfo" ) }, { const_cast<char*>( "sendents" ) }, { const_cast<char*>( "fullupdate" ) },
	{ const_cast<char*>( "setpause" ) }, { const_cast<char*>( "unpause" ) }, { nullptr } };

static_assert( sizeof( command_t ) == sizeof( char* ) );

void SV_IgnoreHLTV( client_t* )
{
}

int SV_ValidateClientCommand( char* commandText )
{
	COM_Parse( commandText );
	for ( command_t* command = clcommands; command->command != nullptr; ++command )
	{
		if ( Q_strcasecmp( com_token, command->command ) == 0 )
			return 1;
	}
	return 0;
}

void SV_ParseStringCommand( client_t* )
{
	constexpr int MAX_GAME_CLIENT_COMMAND_LENGTH = 127;
	char* text = MSG_ReadString();
	COM_Parse( text );
	for ( command_t* command = clcommands; command->command; ++command )
	{
		if ( Q_strcasecmp( com_token, command->command ) == 0 )
		{
			Cmd_ExecuteString( text, src_client );
			return;
		}
	}
	if ( Q_strlen( text ) > MAX_GAME_CLIENT_COMMAND_LENGTH )
		text[MAX_GAME_CLIENT_COMMAND_LENGTH] = 0;
	Cmd_TokenizeString( text );
	gEntityInterface.pfnClientCommand( sv_player );
}

void SV_ParseDelta( client_t* )
{
	host_client->delta_sequence = MSG_ReadByte();
}

void SV_ParseVoiceData( client_t* client )
{
	constexpr int MAX_VOICE_DATA_LENGTH = 4096;
	constexpr int VOICE_DATAGRAM_RESERVE_BYTES = 6;
	constexpr int VOICE_STREAM_WORD_BITS = 32;
	char received[MAX_VOICE_DATA_LENGTH];
	int clientIndex = static_cast<int>( client - svs.clients );
	unsigned int length = static_cast<unsigned int>( MSG_ReadShort() );
	if ( length > MAX_VOICE_DATA_LENGTH )
	{
		Con_DPrintf( "SV_ParseVoiceData: invalid incoming packet.\n" );
		SV_DropClient( client, 0, "Invalid voice data\n" );
		return;
	}
	MSG_ReadBuf( length, received );
	client->m_lastvoicetime = sv.time;
	if ( sv_voiceenable.value == 0.0f )
		return;
	for ( int index = 0; index < MAX_CLIENTS; ++index )
	{
		client_t* destination = &svs.clients[index];
		std::uint32_t mask = 1u << ( index % VOICE_STREAM_WORD_BITS );
		qboolean isSender = client == destination;
		if ( !( client->m_VoiceStreams[index / VOICE_STREAM_WORD_BITS] & mask ) && !isSender )
			continue;
		// Is this client even on the server?
		if ( !destination->active && !destination->connected && !isSender )
			continue;
		int sendLength = length;
		if ( isSender && !destination->m_bLoopback )
			sendLength = 0; // Still send something, just zero length (this is so the client 
								// can display something that shows knows the server knows it's talking).
		sizebuf_t* datagram = &destination->datagram;
		if ( static_cast<int>( length + VOICE_DATAGRAM_RESERVE_BYTES + datagram->cursize ) < datagram->maxsize )
		{
			MSG_WriteByte( datagram, svc_voicedata );
			MSG_WriteByte( datagram, clientIndex );
			MSG_WriteShort( datagram, sendLength );
			MSG_WriteBuf( datagram, sendLength, received );
		}
	}
}

void SV_ParseCvarValue( client_t* client )
{
	char* value = MSG_ReadString();
	if ( gNewDLLFunctions.pfnCvarValue )
		gNewDLLFunctions.pfnCvarValue( client->edict, value );
	Con_DPrintf( "Cvar query response: name:%s, value:%s\n", client->name, value );
}

void SV_ParseCvarValue2( client_t* client )
{
	constexpr int CVAR_QUERY_NAME_BYTES = 255;
	char cvarName[CVAR_QUERY_NAME_BYTES];
	int requestID = MSG_ReadLong();
	Q_strncpy( cvarName, MSG_ReadString(), sizeof( cvarName ) );
	cvarName[sizeof( cvarName ) - 1] = 0;
	char* value = MSG_ReadString();
	if ( gNewDLLFunctions.pfnCvarValue2 )
		gNewDLLFunctions.pfnCvarValue2( client->edict, requestID, cvarName, value );
	Con_DPrintf(
		"Cvar query response: name:%s, request ID %d, cvar:%s, value:%s\n", client->name, requestID, cvarName, value );
}

static float SV_CalcClientTime( client_t* client )
{
	constexpr int MAX_LATENCY_SAMPLES = 16;
	constexpr int LATENCY_VARIATION_SAMPLES = 4;
	constexpr float INITIAL_MIN_LATENCY = 9999.0f;
	constexpr float INITIAL_MAX_LATENCY = -9999.0f;
	constexpr double MAX_LATENCY_VARIATION = 0.2;
	int samples = static_cast<int>( sv_unlagsamples.value );
	if ( samples < 1 )
		samples = 1;
	int limit = SV_UPDATE_BACKUP;
	if ( limit > MAX_LATENCY_SAMPLES )
		limit = MAX_LATENCY_SAMPLES;
	if ( samples >= limit )
		samples = limit;
	float total = 0.0f;
	int count = 0;
	for ( int index = 0; index < samples; ++index )
	{
		float ping = client->frames[( client->netchan.incoming_acknowledged - index ) & SV_UPDATE_MASK].ping_time;
		if ( ping > 0.0f )
		{
			total += ping;
			++count;
		}
	}
	if ( !count )
		return 0.0f;
	float average = total / static_cast<float>( count );
	float minimum = INITIAL_MIN_LATENCY;
	float maximum = INITIAL_MAX_LATENCY;
	int variationSamples = SV_UPDATE_BACKUP;
	if ( variationSamples > LATENCY_VARIATION_SAMPLES )
		variationSamples = LATENCY_VARIATION_SAMPLES;
	for ( int index = 0; index < variationSamples; ++index )
	{
		float ping = client->frames[( client->netchan.incoming_acknowledged - index ) & SV_UPDATE_MASK].ping_time;
		if ( ping > 0.0f )
		{
			maximum = std::fmax( ping, maximum );
			minimum = std::fmin( ping, minimum );
		}
	}
	if ( maximum < minimum || std::fabs( maximum - minimum ) <= MAX_LATENCY_VARIATION )
		return average;
	return 0.0f;
}

void SV_ComputeLatency( client_t* client )
{
	constexpr int MAX_LATENCY_SAMPLES = 16;
	constexpr int LATENCY_VARIATION_SAMPLES = 4;
	constexpr float INITIAL_MIN_LATENCY = 9999.0f;
	constexpr float INITIAL_MAX_LATENCY = -9999.0f;
	constexpr double MAX_LATENCY_VARIATION = 0.2;
	int samples = static_cast<int>( sv_unlagsamples.value );
	if ( samples < 1 )
		samples = 1;
	int limit = SV_UPDATE_BACKUP;
	if ( limit > MAX_LATENCY_SAMPLES )
		limit = MAX_LATENCY_SAMPLES;
	if ( samples >= limit )
		samples = limit;
	float total = 0.0f;
	int count = 0;
	for ( int index = 0; index < samples; ++index )
	{
		float ping = client->frames[( client->netchan.incoming_acknowledged - index ) & SV_UPDATE_MASK].ping_time;
		if ( ping > 0.0f )
		{
			total += ping;
			++count;
		}
	}
	float average = 0.0f;
	if ( count != 0 )
	{
		average = total / static_cast<float>( count );
		float minimum = INITIAL_MIN_LATENCY;
		float maximum = INITIAL_MAX_LATENCY;
		int variationSamples = SV_UPDATE_BACKUP;
		if ( variationSamples > LATENCY_VARIATION_SAMPLES )
			variationSamples = LATENCY_VARIATION_SAMPLES;
		if ( variationSamples > 0 )
		{
			for ( int index = 0; index < variationSamples; ++index )
			{
				float ping =
					client->frames[( client->netchan.incoming_acknowledged - index ) & SV_UPDATE_MASK].ping_time;
				if ( ping > 0.0f )
				{
					maximum = std::fmax( ping, maximum );
					minimum = std::fmin( ping, minimum );
				}
			}
			if ( maximum >= minimum && std::fabs( maximum - minimum ) > MAX_LATENCY_VARIATION )
				average = 0.0f;
		}
	}
	client->latency = average;
}

void SV_ExecuteClientMessage( client_t* client )
{
	constexpr int LAST_CLIENT_COMMAND = 11;
	constexpr double CONNECTION_PING_GRACE_SECONDS = 2.0;
	g_balreadymoved = 0;
	client_frame_t* frame = &client->frames[client->netchan.incoming_acknowledged & SV_UPDATE_MASK];
	frame->ping_time = static_cast<float>( realtime - frame->senttime - client->next_messageinterval );
	if ( frame->senttime == 0.0 )
		frame->ping_time = 0.0f;
	if ( realtime - client->connection_started < CONNECTION_PING_GRACE_SECONDS && frame->ping_time > 0.0f )
		frame->ping_time = 0.0f;
	client->latency = SV_CalcClientTime( client );
	host_client = client;
	sv_player = client->edict;
	client->delta_sequence = -1; // no delta unless requested
	pmove = &g_svmove;
	for ( ;; )
	{
		if ( msg_badread )
		{
			Con_Printf( "SV_ReadClientMessage: badread\n" );
			return;
		}
		int command = MSG_ReadByte();
		if ( command == -1 )
			return;
		if ( static_cast<unsigned int>( command ) > LAST_CLIENT_COMMAND )
		{
			Con_Printf( "SV_ReadClientMessage: unknown command char (%d)\n", command );
			SV_DropClient( client, 0, "Bad command character in client command" );
			return;
		}
		if ( sv_clcfuncs[command].pfnParse )
			sv_clcfuncs[command].pfnParse( client );
	}
}

void SV_EstablishTimeBase( client_t* client, usercmd_t* commands, int dropped, int numBackup, int numCommands )
{
	constexpr int MAX_DROPPED_COMMANDS = 24;
	constexpr double MILLISECONDS_PER_SECOND = 1000.0;
	double commandTime = 0.0;
	if ( dropped < MAX_DROPPED_COMMANDS )
	{
		while ( dropped > numBackup )
		{
			commandTime += static_cast<double>( client->lastcmd.msec ) / MILLISECONDS_PER_SECOND;
			--dropped;
		}
		while ( dropped > 0 )
		{
			commandTime += static_cast<double>( commands[numCommands + dropped - 1].msec ) / MILLISECONDS_PER_SECOND;
			--dropped;
		}
	}
	for ( int index = numCommands - 1; index >= 0; --index )
		commandTime += static_cast<double>( commands[index].msec ) / MILLISECONDS_PER_SECOND;
	client->svtimebase = sv.time + host_frametime - commandTime;
}

void SV_ParseMove( client_t* sender )
{
	constexpr int CMD_BACKUP = 64;
	constexpr int CMD_MAXBACKUP = CMD_BACKUP - 2;
	constexpr int VOICE_LOOPBACK_FLAG = 1 << 7;
	constexpr int MAX_DROPPED_COMMANDS = 24;
	constexpr double MILLISECONDS_PER_SECOND = 1000.0;

	if ( g_balreadymoved )
	{
		msg_badread = 1;
		return;
	}
	g_balreadymoved = 1;
	client_frame_t* frame = &host_client->frames[host_client->netchan.incoming_acknowledged & SV_UPDATE_MASK];
	usercmd_t nullCommand;
	usercmd_t commands[CMD_BACKUP];
	Q_memset( &nullCommand, 0, sizeof( nullCommand ) );
	int checksumIndex = msg_readcount + 1;
	int length = MSG_ReadByte();
	byte checksum = static_cast<byte>( MSG_ReadByte() );
	if ( length <= 0 || length + checksumIndex + 1 > net_message.maxsize )
	{
		Con_Printf( "SV_ParseMove: invalid length: %d\n", length );
		SV_DropClient( host_client, 0, "Invalid Move message" );
		msg_badread = 1;
		return;
	}
	COM_UnMunge( &net_message.data[checksumIndex + 1], length, host_client->netchan.incoming_sequence );
	int packetLoss = MSG_ReadByte();
	sender->m_bLoopback = ( packetLoss & VOICE_LOOPBACK_FLAG ) != 0;
	packetLoss &= ~VOICE_LOOPBACK_FLAG;
	int numBackup = MSG_ReadByte();
	int numCommands = MSG_ReadByte();
	net_drop += 1 - numCommands;
	int totalCommands = numCommands + numBackup;
	if ( static_cast<unsigned int>( totalCommands ) > CMD_MAXBACKUP )
	{
		Con_Printf( "SV_ReadClientMessage: too many cmds %i sent for %s/%s\n", totalCommands, host_client->name,
			NET_AdrToString( host_client->netchan.remote_address ) );
		SV_DropClient( host_client, 0, "CMD_MAXBACKUP hit" );
		msg_badread = 1;
		return;
	}
	usercmd_t* previous = &nullCommand;
	for ( int index = totalCommands - 1; index >= 0; --index )
	{
		MSG_ReadUsercmd( &commands[index], previous );
		previous = &commands[index];
	}
	if ( !sv.active || ( !host_client->active && !host_client->spawned ) )
		return;
	if ( msg_badread )
	{
		Con_Printf( "Client %s:%s sent a bogus command packet\n", host_client->name,
			NET_AdrToString( host_client->netchan.remote_address ) );
		return;
	}
	if ( COM_BlockSequenceCRCByte( &net_message.data[checksumIndex + 1], msg_readcount - checksumIndex - 1,
			 host_client->netchan.incoming_sequence )
		 != checksum )
	{
		Con_DPrintf( "Failed command checksum for %s:%s\n", host_client->name,
			NET_AdrToString( host_client->netchan.remote_address ) );
		msg_badread = 1;
		return;
	}
	host_client->packet_loss = static_cast<float>( packetLoss );
	if ( sv.paused || ( svs.maxclients <= 1 && key_dest != key_game ) || ( sv_player->v.flags & FL_FROZEN ) )
	{
		for ( int index = 0; index < numCommands; ++index )
		{
			usercmd_t* command = &commands[index];
			command->msec = 0;
			command->forwardmove = 0.0f;
			command->sidemove = 0.0f;
			command->upmove = 0.0f;
			command->buttons = 0;
			if ( sv_player->v.flags & FL_FROZEN )
				command->impulse = 0;
			for ( int axis = 0; axis < 3; ++axis )
				command->viewangles[axis] = sv_player->v.v_angle[axis];
		}
		net_drop = 0;
	}
	else
	{
		for ( int axis = 0; axis < 3; ++axis )
			sv_player->v.v_angle[axis] = commands[0].viewangles[axis];
	}
	sv_player->v.button = commands[0].buttons;
	sv_player->v.light_level = commands[0].lightlevel;
	SV_EstablishTimeBase( host_client, commands, net_drop, numBackup, numCommands );
	if ( net_drop < MAX_DROPPED_COMMANDS )
	{
		while ( net_drop > numBackup )
		{
			SV_RunCmd( &host_client->lastcmd, 0 );
			--net_drop;
		}
		while ( net_drop > 0 )
		{
			int index = numCommands + net_drop - 1;
			SV_RunCmd( &commands[index], host_client->netchan.incoming_sequence - index );
			--net_drop;
		}
	}
	for ( int index = numCommands - 1; index >= 0; --index )
		SV_RunCmd( &commands[index], host_client->netchan.incoming_sequence - index );
	Q_memcpy( &host_client->lastcmd, &commands[0], sizeof( usercmd_t ) );
	frame->ping_time = static_cast<float>(
		frame->ping_time - static_cast<double>( host_client->lastcmd.msec ) * 0.5 / MILLISECONDS_PER_SECOND );
	if ( frame->ping_time < 0.0f )
		frame->ping_time = 0.0f;
	if ( sv_player->v.animtime > sv.time + host_frametime )
		sv_player->v.animtime = static_cast<float>( sv.time + host_frametime );
}

void SV_FullClientUpdate( client_t* client, sizebuf_t* message );

static float s_LastFullUpdate[MAX_CLIENTS + 1];

int SV_FullUpdate_Internal()
{
	constexpr double FULLUPDATE_INTERVAL = 0.45;
	if ( !host_client->active )
	{
		Con_DPrintf( "%s is trying to fullupdate when not active at (%f)\n", host_client->name, sv.time );
		return 0;
	}

	int entityIndex = IndexOfEdict( host_client->edict );
	double time = sv.time;
	float lastUpdate = s_LastFullUpdate[entityIndex];
	if ( lastUpdate > time )
	{
		lastUpdate = 0.0f;
		s_LastFullUpdate[entityIndex] = 0.0f;
	}

	float elapsed = static_cast<float>( time - lastUpdate );
	double clampedElapsed = elapsed < 0.0f ? 0.0f : elapsed;
	if ( !( clampedElapsed < FULLUPDATE_INTERVAL ) || !( time > FULLUPDATE_INTERVAL ) )
	{
		s_LastFullUpdate[entityIndex] = static_cast<float>( time );
		SV_ForceFullClientsUpdate();
		return 1;
	}

	Con_DPrintf( "%s is spamming fullupdate: (%f) (%f) (%f)\n", host_client->name, time,
		static_cast<double>( lastUpdate ), clampedElapsed );
	return 0;
}

void SV_FullUpdate_f()
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( SV_FullUpdate_Internal() > 0 )
		gEntityInterface.pfnClientCommand( sv_player );
}

char* Info_Serverinfo();
void Info_Print( const char* info );

void SV_ShowServerinfo_f()
{
	if ( cmd_source == src_command )
		Cmd_ForwardToServer();
	else
		Info_Print( Info_Serverinfo() );
}

struct sv_adjusted_positions_t
{
	int active;
	int needrelink;
	vec3_t neworg;
	vec3_t oldorg;
	vec3_t initial_correction_org;
	vec3_t oldabsmin;
	vec3_t oldabsmax;
	int deadflag;
	vec3_t temp_org;
	int temp_org_setflag;
};

static_assert( sizeof( sv_adjusted_positions_t ) == 88 );

void SV_CheckMovingGround( edict_t* player, float frametime )
{
	int flags = player->v.flags;
	if ( flags & FL_ONGROUND )
	{
		edict_t* ground = player->v.groundentity;
		if ( ground && ( ground->v.flags & FL_CONVEYOR ) )
		{
			if ( flags & FL_BASEVELOCITY )
				VectorMA( player->v.basevelocity, ground->v.speed, ground->v.movedir, player->v.basevelocity );
			else
				VectorScale( ground->v.movedir, ground->v.speed, player->v.basevelocity );
			player->v.flags |= FL_BASEVELOCITY;
			flags = player->v.flags;
		}
	}
	if ( !( flags & FL_BASEVELOCITY ) )
	{
		float scale = static_cast<float>( frametime * 0.5 + 1.0 );
		VectorMA( player->v.velocity, scale, player->v.basevelocity, player->v.velocity );
		player->v.basevelocity[0] = 0.0f;
		player->v.basevelocity[1] = 0.0f;
		player->v.basevelocity[2] = 0.0f;
		flags = player->v.flags;
	}
	player->v.flags = flags & ~FL_BASEVELOCITY;
}

void SV_PlayerRunPreThink( edict_t* player, float time )
{
	gGlobalVariables.time = time;
	gEntityInterface.pfnPlayerPreThink( player );
}

qboolean SV_PlayerRunThink( edict_t* entity, float frametime, double clienttimebase )
{
	int flags = entity->v.flags;
	if ( !( flags & FL_KILLME ) && !( flags & FL_DORMANT ) )
	{
		float thinkTime = entity->v.nextthink;
		if ( thinkTime <= 0.0f || thinkTime > frametime + clienttimebase )
			return true;
		if ( clienttimebase > thinkTime )
			thinkTime = static_cast<float>( clienttimebase );
		entity->v.nextthink = 0.0f;
		gGlobalVariables.time = thinkTime;
		gEntityInterface.pfnThink( entity );
		flags = entity->v.flags;
	}
	if ( flags & FL_KILLME )
		ED_Free( entity );
	return entity->free == false;
}

void SV_ConvertPMTrace( trace_t* destination, pmtrace_t* source, edict_t* entity )
{
	destination->allsolid = source->allsolid;
	destination->startsolid = source->startsolid;
	destination->inopen = source->inopen;
	destination->inwater = source->inwater;
	destination->fraction = source->fraction;
	destination->endpos[0] = source->endpos[0];
	destination->endpos[1] = source->endpos[1];
	destination->endpos[2] = source->endpos[2];
	destination->plane.normal[0] = source->plane.normal[0];
	destination->plane.normal[1] = source->plane.normal[1];
	destination->plane.normal[2] = source->plane.normal[2];
	destination->plane.dist = source->plane.dist;
	destination->hitgroup = source->hitgroup;
	destination->ent = entity;
}

sv_adjusted_positions_t truepositions[MAX_CLIENTS];
qboolean nofind;

int SV_UnlagCheckTeleport( vec_t* v1, vec_t* v2 )
{
	constexpr double TELEPORT_DISTANCE = 128.0;
	return std::fabs( v1[0] - v2[0] ) > TELEPORT_DISTANCE || std::fabs( v1[1] - v2[1] ) > TELEPORT_DISTANCE
		   || std::fabs( v1[2] - v2[2] ) > TELEPORT_DISTANCE;
}

void SV_GetTrueOrigin( int player, vec_t* origin )
{
	if ( host_client->lw != 0 && host_client->lc != 0 && sv_unlag.value != 0.0 && svs.maxclients > 1 && player >= 0
		 && player < svs.maxclients && host_client->active != 0 )
	{
		sv_adjusted_positions_t* position = &truepositions[player];
		if ( position->active != 0 && position->needrelink != 0 )
		{
			origin[0] = position->oldorg[0];
			origin[1] = position->oldorg[1];
			origin[2] = position->oldorg[2];
		}
	}
}

void SV_GetTrueMinMax( int player, float** fmin, float** fmax )
{
	if ( host_client->lw != 0 && host_client->lc != 0 && sv_unlag.value != 0.0 && svs.maxclients > 1 && player >= 0
		 && player < svs.maxclients && host_client->active != 0 )
	{
		if ( truepositions[player].active != 0 && truepositions[player].needrelink != 0 )
		{
			*fmin = truepositions[player].oldabsmin;
			*fmax = truepositions[player].oldabsmax;
		}
	}
}

entity_state_t* SV_FindEntInPack( int index, packet_entities_t* pack )
{
	if ( pack->num_entities > 0 )
	{
		entity_state_t* state = pack->entities;
		if ( index == state->number )
			return state;
		entity_state_t* next = state + 1;
		int count = 0;
		while ( ++count != pack->num_entities )
		{
			state = next++;
			if ( next[-1].number == index )
				return state;
		}
	}
	return nullptr;
}

qboolean SV_SetPlayer( int idnum )
{
	client_t* client = svs.clients;
	while ( client->spawned == 0 || client->active == 0 || client->connected == 0 || client->userid != idnum )
	{
		++client;
		if ( client == svs.clients + MAX_CLIENTS )
		{
			Con_Printf( "Userid %i is not on the server\n", idnum );
			return false;
		}
	}
	host_client = client;
	sv_player = client->edict;
	return true;
}

void SV_SetupMove( client_t* client )
{
	constexpr double MAX_LATENCY = 1.5;
	constexpr float MAX_INTERPOLATION = 0.1f;
	constexpr double MAX_HISTORY_GAP = 1.0;
	constexpr double MAX_ORIGIN_CHANGE = 128.0;
	constexpr double MILLISECONDS_PER_SECOND = 1000.0;

	Q_memset( truepositions, 0, sizeof( truepositions ) );
	nofind = true;
	if ( !gEntityInterface.pfnAllowLagCompensation() || sv_unlag.value == 0.0f || !client->lw || !client->lc
		 || svs.maxclients <= 1 || !client->active )
		return;

	nofind = false;
	for ( int index = 0; index < svs.maxclients; ++index )
	{
		client_t* other = &svs.clients[index];
		if ( other == client || !other->active )
			continue;

		sv_adjusted_positions_t* position = &truepositions[index];
		VectorCopy( other->edict->v.origin, position->oldorg );
		VectorCopy( other->edict->v.absmin, position->oldabsmin );
		VectorCopy( other->edict->v.absmax, position->oldabsmax );
		position->active = true;
	}

	float latency = static_cast<float>( std::fmin( MAX_LATENCY, static_cast<double>( client->latency ) ) );
	float maxUnlag = sv_maxunlag.value;
	if ( maxUnlag != 0.0f )
	{
		if ( maxUnlag < 0.0f )
		{
			Cvar_SetValue( const_cast<char*>( "sv_maxunlag" ), 0.0f );
			maxUnlag = sv_maxunlag.value;
		}
		latency = std::fmin( latency, maxUnlag );
	}

	float interpolation = std::fmin( MAX_INTERPOLATION,
		static_cast<float>( static_cast<float>( client->lastcmd.lerp_msec ) / MILLISECONDS_PER_SECOND ) );
	interpolation =
		static_cast<float>( std::fmax( client->next_messageinterval, static_cast<double>( interpolation ) ) );
	double historyTime = realtime - latency;
	float desiredTime = static_cast<float>( historyTime - interpolation + sv_unlagpush.value );
	desiredTime = static_cast<float>( std::fmin( realtime, static_cast<double>( desiredTime ) ) );

	client_frame_t* newer = nullptr;
	client_frame_t* older = nullptr;
	int frameIndex = 0;
	for ( ; frameIndex < SV_UPDATE_BACKUP; ++frameIndex )
	{
		unsigned int sequence = static_cast<unsigned int>( client->netchan.outgoing_sequence );
		sequence -= static_cast<unsigned int>( frameIndex ) + 1;
		older = &client->frames[sequence & static_cast<unsigned int>( SV_UPDATE_MASK )];
		for ( int index = 0; index < older->entities.num_entities; ++index )
		{
			entity_state_t* state = &older->entities.entities[index];
			if ( state->number < 1 || state->number > svs.maxclients )
				continue;

			sv_adjusted_positions_t* position = &truepositions[state->number - 1];
			if ( position->deadflag )
				continue;
			if ( state->health <= 0 || ( state->effects & EF_NOINTERP ) )
				position->deadflag = true;
			if ( position->temp_org_setflag )
			{
				for ( int axis = 0; axis < 3; ++axis )
				{
					float change = state->origin[axis] - position->temp_org[axis];
					if ( !( std::fabs( static_cast<double>( change ) ) <= MAX_ORIGIN_CHANGE ) )
					{
						position->deadflag = true;
						break;
					}
				}
			}
			else
			{
				position->temp_org_setflag = true;
			}
			VectorCopy( state->origin, position->temp_org );
		}

		if ( desiredTime > older->senttime )
			break;
		newer = older;
	}

	if ( frameIndex >= SV_UPDATE_BACKUP || desiredTime - older->senttime > MAX_HISTORY_GAP )
	{
		Q_memset( truepositions, 0, sizeof( truepositions ) );
		nofind = true;
		return;
	}

	float fraction = 0.0f;
	if ( newer )
	{
		float interval = static_cast<float>( newer->senttime - older->senttime );
		if ( interval != 0.0f )
		{
			float ratio = static_cast<float>( ( desiredTime - older->senttime ) / interval );
			fraction = std::fmax( 0.0f, std::fmin( 1.0f, ratio ) );
		}
	}
	else
	{
		newer = older;
	}

	for ( int index = 0; index < older->entities.num_entities; ++index )
	{
		entity_state_t* state = &older->entities.entities[index];
		if ( state->number < 1 || state->number > svs.maxclients )
			continue;

		client_t* other = &svs.clients[state->number - 1];
		if ( other == client || !other->active )
			continue;
		sv_adjusted_positions_t* position = &truepositions[state->number - 1];
		if ( position->deadflag )
			continue;
		if ( !position->active )
		{
			Con_DPrintf( "tried to store off position of bogus player %i/%s\n", index, other->name );
			continue;
		}

		entity_state_t* nextState = nullptr;
		for ( int nextIndex = 0; nextIndex < newer->entities.num_entities; ++nextIndex )
		{
			entity_state_t* candidate = &newer->entities.entities[nextIndex];
			if ( candidate->number == state->number )
			{
				nextState = candidate;
				break;
			}
		}

		vec3_t origin;
		if ( nextState )
		{
			vec3_t delta;
			VectorSubtract( nextState->origin, state->origin, delta );
			VectorMA( state->origin, fraction, delta, origin );
		}
		else
		{
			VectorCopy( state->origin, origin );
		}
		VectorCopy( origin, position->neworg );
		VectorCopy( origin, position->initial_correction_org );
		if ( !VectorCompare( origin, other->edict->v.origin ) )
		{
			VectorCopy( origin, other->edict->v.origin );
			SV_LinkEdict( other->edict, false );
			position->needrelink = true;
		}
	}
}

void SV_RestoreMove( client_t* client )
{
	if ( nofind )
	{
		nofind = false;
		return;
	}
	if ( !gEntityInterface.pfnAllowLagCompensation() || svs.maxclients <= 1 || sv_unlag.value == 0.0f || !client->lw
		 || !client->lc || !client->active )
		return;

	for ( int index = 0; index < svs.maxclients; ++index )
	{
		client_t* other = &svs.clients[index];
		if ( other == client || !other->active )
			continue;

		sv_adjusted_positions_t* position = &truepositions[index];
		if ( VectorCompare( position->neworg, position->oldorg ) || !position->needrelink )
			continue;
		if ( !position->active )
		{
			Con_DPrintf( "SV_RestoreMove:  Tried to restore 'inactive' player %i/%s\n", index, other->name );
			continue;
		}

		if ( VectorCompare( position->initial_correction_org, other->edict->v.origin ) )
		{
			VectorCopy( position->oldorg, other->edict->v.origin );
			SV_LinkEdict( other->edict, false );
		}
	}
}

static sv_adjusted_positions_t* SV_GetAdjustedPosition( int clientIndex )
{
	if ( !host_client->lw || !host_client->lc || sv_unlag.value == 0.0f || svs.maxclients <= 1 || !host_client->active
		 || clientIndex < 0 || clientIndex >= svs.maxclients )
		return nullptr;
	sv_adjusted_positions_t* position = &truepositions[clientIndex];
	return position->active && position->needrelink ? position : nullptr;
}

void SV_CopyEdictToPhysent( physent_t* physent, int index, edict_t* entity )
{
	physent->info = index;
	for ( int axis = 0; axis < 3; ++axis )
		physent->origin[axis] = entity->v.origin[axis];
	if ( index >= 1 && index <= svs.maxclients )
	{
		sv_adjusted_positions_t* position = SV_GetAdjustedPosition( index - 1 );
		if ( position )
		{
			for ( int axis = 0; axis < 3; ++axis )
				physent->origin[axis] = position->oldorg[axis];
		}
		physent->player = index;
	}
	else
	{
		physent->player = 0;
	}
	for ( int axis = 0; axis < 3; ++axis )
		physent->angles[axis] = entity->v.angles[axis];
	physent->studiomodel = nullptr;
	physent->rendermode = entity->v.rendermode;
	if ( entity->v.solid == SOLID_BSP || ( entity->v.solid == SOLID_NOT && entity->v.modelindex ) )
	{
		model_t* model = sv.models[entity->v.modelindex];
		physent->model = model;
		Q_strncpy( physent->name, model->name, sizeof( physent->name ) );
		physent->name[sizeof( physent->name ) - 1] = '\0';
	}
	else
	{
		physent->model = nullptr;
		if ( entity->v.solid != SOLID_NOT )
		{
			if ( entity->v.solid == SOLID_BBOX )
			{
				if ( entity->v.modelindex )
				{
					model_t* model = sv.models[entity->v.modelindex];
					if ( model )
					{
						if ( model->flags & STUDIO_TRACE_HITBOX )
							physent->studiomodel = model;
						Q_strncpy( physent->name, model->name, sizeof( physent->name ) );
						physent->name[sizeof( physent->name ) - 1] = '\0';
					}
				}
			}
			else
			{
				if ( entity->v.solid == SOLID_SLIDEBOX && entity->v.modelindex > 0 &&
					(unsigned int)entity->v.modelindex < MAX_MODELS )
				{
					model_t* model = sv.models[entity->v.modelindex];
					if ( model && model->type == mod_studio && ( model->flags & STUDIO_TRACE_HITBOX ) )
						physent->studiomodel = model;
				}
				if ( entity->v.classname )
					Q_strncpy( physent->name, pr_strings + entity->v.classname, sizeof( physent->name ) );
				else
					Q_strcpy( physent->name, "?" );
				physent->name[sizeof( physent->name ) - 1] = '\0';
			}
			for ( int axis = 0; axis < 3; ++axis )
			{
				physent->mins[axis] = entity->v.mins[axis];
				physent->maxs[axis] = entity->v.maxs[axis];
			}
		}
	}
	physent->solid = entity->v.solid;
	physent->skin = entity->v.skin;
	physent->frame = entity->v.frame;
	physent->sequence = entity->v.sequence;
	Q_memcpy( physent->controller, entity->v.controller, sizeof( physent->controller ) );
	Q_memcpy( physent->blending, entity->v.blending, sizeof( physent->blending ) );
	physent->movetype = entity->v.movetype;
	physent->takedamage = DAMAGE_NO;
	physent->blooddecal = 0;
	physent->iuser1 = entity->v.iuser1;
	physent->iuser2 = entity->v.iuser2;
	physent->iuser3 = entity->v.iuser3;
	physent->iuser4 = entity->v.iuser4;
	physent->fuser1 = entity->v.fuser1;
	physent->fuser2 = entity->v.fuser2;
	physent->fuser3 = entity->v.fuser3;
	physent->fuser4 = entity->v.fuser4;
	for ( int axis = 0; axis < 3; ++axis )
	{
		physent->vuser1[axis] = entity->v.vuser1[axis];
		physent->vuser2[axis] = entity->v.vuser2[axis];
		physent->vuser3[axis] = entity->v.vuser3[axis];
		physent->vuser4[axis] = entity->v.vuser4[axis];
	}
}

static void SV_AddLinksToPM_( areanode_t* node, float* mins, float* maxs )
{
	for ( ;; )
	{
		for ( link_t *link = node->solid_edicts.next, *next; link != &node->solid_edicts; link = next )
		{
			next = link->next;
			edict_t* entity = EDICT_FROM_AREA( link );
			if ( entity->v.groupinfo )
			{
				int commonGroups = entity->v.groupinfo & sv_player->v.groupinfo;
				if ( ( g_groupop == 0 && !commonGroups ) || ( g_groupop == 1 && commonGroups ) )
					continue;
			}
			if ( entity->v.owner == sv_player )
				continue; // player's own missile
			if ( entity->v.solid != SOLID_BSP && entity->v.solid != SOLID_BBOX && entity->v.solid != SOLID_SLIDEBOX
				 && entity->v.solid != SOLID_NOT )
				continue;
			int entityIndex = NUM_FOR_EDICT( entity );
			physent_t* visible = &pmove->visents[pmove->numvisent++];
			SV_CopyEdictToPhysent( visible, entityIndex, entity );
			if ( entity->v.solid == SOLID_NOT && ( !entity->v.skin || !entity->v.modelindex ) )
				continue;
			if ( ( entity->v.flags & FL_MONSTERCLIP ) && entity->v.solid == SOLID_BSP )
				continue;
			if ( entity == sv_player || ( ( entity->v.flags & FL_CLIENT ) && entity->v.health <= 0.0f ) )
				continue;
			if ( entity->v.mins[2] == 0.0f && entity->v.maxs[2] == 1.0f )
				continue;
			if ( Length( entity->v.size ) == 0.0f )
				continue;
			const float* entityMins = entity->v.absmin;
			const float* entityMaxs = entity->v.absmax;
			if ( entity->v.flags & FL_CLIENT )
			{
				sv_adjusted_positions_t* position = SV_GetAdjustedPosition( entityIndex - 1 );
				if ( position )
				{
					entityMins = position->oldabsmin;
					entityMaxs = position->oldabsmax;
				}
			}
			int axis = 0;
			for ( ; axis < 3; ++axis )
			{
				if ( entityMins[axis] > maxs[axis] || mins[axis] > entityMaxs[axis] )
					break;
			}
			if ( axis != 3 )
				continue;
			physent_t* physical;
			if ( entity->v.solid == SOLID_NOT && entity->v.skin == CONTENTS_LADDER )
			{
				if ( pmove->nummoveent >= MAX_MOVEENTS )
				{
					Con_DPrintf( "SV_AddLinksToPM:  pmove->nummoveent >= MAX_MOVEENTS\n" );
					continue;
				}
				physical = &pmove->moveents[pmove->nummoveent++];
			}
			else
			{
				if ( pmove->numphysent >= MAX_PHYSENTS )
				{
					Con_DPrintf( "SV_AddLinksToPM:  pmove->numphysent >= MAX_PHYSENTS\n" );
					return;
				}
				physical = &pmove->physents[pmove->numphysent++];
			}
			Q_memcpy( physical, visible, sizeof( physent_t ) );
		}
		if ( node->axis == -1 )
			return;
		if ( maxs[node->axis] > node->dist )
			SV_AddLinksToPM_( node->children[0], mins, maxs );
		if ( node->dist <= mins[node->axis] )
			return;
		node = node->children[1];
	}
}

void SV_AddLinksToPM( areanode_t* node, float* origin )
{
	constexpr float PLAYER_MOVE_SEARCH_RADIUS = 256.0f;
	Q_memset( &pmove->physents[0], 0, sizeof( physent_t ) );
	Q_memset( &pmove->visents[0], 0, sizeof( physent_t ) );
	pmove->physents[0].model = sv.worldmodel;
	if ( sv.worldmodel )
	{
		Q_strncpy( pmove->physents[0].name, sv.worldmodel->name, sizeof( pmove->physents[0].name ) );
		pmove->physents[0].name[sizeof( pmove->physents[0].name ) - 1] = '\0';
	}
	for ( int axis = 0; axis < 3; ++axis )
		pmove->physents[0].origin[axis] = vec3_origin[axis];
	pmove->physents[0].info = 0;
	pmove->physents[0].solid = SOLID_BSP;
	pmove->physents[0].movetype = MOVETYPE_NONE;
	pmove->physents[0].takedamage = DAMAGE_YES;
	pmove->physents[0].blooddecal = 0;
	pmove->numphysent = 1;
	pmove->numvisent = 1;
	Q_memcpy( &pmove->visents[0], &pmove->physents[0], sizeof( physent_t ) );
	pmove->nummoveent = 0;
	vec3_t mins;
	vec3_t maxs;
	for ( int axis = 0; axis < 3; ++axis )
	{
		mins[axis] = origin[axis] - PLAYER_MOVE_SEARCH_RADIUS;
		maxs[axis] = origin[axis] + PLAYER_MOVE_SEARCH_RADIUS;
	}
	SV_AddLinksToPM_( node, mins, maxs );
}

extern areanode_t sv_areanodes[];
void SV_SetupMove( client_t* client );
void SV_RestoreMove( client_t* client );
void SV_CheckMovingGround( edict_t* player, float frametime );
void SV_PlayerRunPreThink( edict_t* player, float time );
qboolean SV_PlayerRunThink( edict_t* player, float frametime, double time );
void SV_ConvertPMTrace( trace_t* trace, pmtrace_t* pmtrace, edict_t* entity );
void SV_AddLinksToPM( areanode_t* node, float* origin );
void SV_Impact( edict_t* entity, edict_t* touch, trace_t* trace );
void SetMinMaxSize( edict_t* entity, const float* mins, const float* maxs, qboolean rotate );
void PM_SV_PlaySound( int channel, const char* sample, float volume, float attenuation, int flags, int pitch );
const char* PM_SV_TraceTexture( int ground, float* start, float* end );
void PM_SV_PlaybackEventFull( int flags, int clientIndex, unsigned short eventIndex, float delay, float* origin,
	float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );

void SV_PreRunCmd()
{
}

void SV_RunCmd( usercmd_t* command, int randomSeed )
{
	constexpr int MAX_COMMAND_MILLISECONDS = 50;
	constexpr int FULLUPDATE_IMPULSE = 204;
	constexpr double SECONDS_PER_MILLISECOND = 0.001;
	constexpr double MILLISECONDS_PER_SECOND = 1000.0;
	usercmd_t movementCommand;
	Q_memcpy( &movementCommand, command, sizeof( movementCommand ) );
	if ( host_client->ignorecmdtime > realtime )
	{
		host_client->cmdtime += static_cast<double>( command->msec ) / MILLISECONDS_PER_SECOND;
		return;
	}
	host_client->ignorecmdtime = 0.0;
	if ( movementCommand.msec > MAX_COMMAND_MILLISECONDS )
	{
		byte halfMilliseconds = static_cast<byte>( command->msec / 2 );
		movementCommand.msec = halfMilliseconds;
		SV_RunCmd( &movementCommand, randomSeed );
		movementCommand.msec = halfMilliseconds;
		movementCommand.impulse = 0;
		SV_RunCmd( &movementCommand, randomSeed );
		return;
	}

	if ( !host_client->fakeclient )
		SV_SetupMove( host_client );
	gEntityInterface.pfnCmdStart( sv_player, command, randomSeed );
	float frametime = static_cast<float>( command->msec * SECONDS_PER_MILLISECOND );
	host_client->svtimebase += frametime;
	host_client->cmdtime += static_cast<double>( command->msec ) / MILLISECONDS_PER_SECOND;
	if ( command->impulse )
	{
		sv_player->v.impulse = command->impulse;
		if ( command->impulse == FULLUPDATE_IMPULSE )
			SV_FullUpdate_Internal();
	}
	sv_player->v.clbasevelocity[0] = 0.0f;
	sv_player->v.clbasevelocity[1] = 0.0f;
	sv_player->v.clbasevelocity[2] = 0.0f;
	sv_player->v.button = command->buttons;
	SV_CheckMovingGround( sv_player, frametime );
	VectorCopy( sv_player->v.v_angle, pmove->oldangles );
	if ( !sv_player->v.fixangle )
		VectorCopy( command->viewangles, sv_player->v.v_angle );
	SV_PlayerRunPreThink( sv_player, static_cast<float>( host_client->svtimebase ) );
	SV_PlayerRunThink( sv_player, frametime, host_client->svtimebase );
	if ( Length( sv_player->v.basevelocity ) > 0.0f )
		VectorCopy( sv_player->v.basevelocity, sv_player->v.clbasevelocity );

	pmove->server = 1;
	pmove->multiplayer = svs.maxclients > 1;
	pmove->time = static_cast<float>( MILLISECONDS_PER_SECOND * host_client->svtimebase );
	pmove->usehull = ( sv_player->v.flags & FL_DUCKING ) != 0;
	pmove->maxspeed = sv_maxspeed.value;
	pmove->clientmaxspeed = sv_player->v.maxspeed;
	pmove->flDuckTime = static_cast<float>( sv_player->v.flDuckTime );
	pmove->bInDuck = sv_player->v.bInDuck;
	pmove->flTimeStepSound = sv_player->v.flTimeStepSound;
	pmove->iStepLeft = sv_player->v.iStepLeft;
	pmove->flFallVelocity = sv_player->v.flFallVelocity;
	pmove->flSwimTime = static_cast<float>( sv_player->v.flSwimTime );
	pmove->oldbuttons = sv_player->v.oldbuttons;
	Q_strncpy( pmove->physinfo, host_client->physinfo, sizeof( pmove->physinfo ) - 1 );
	pmove->physinfo[sizeof( pmove->physinfo ) - 1] = '\0';
	VectorCopy( sv_player->v.velocity, pmove->velocity );
	VectorCopy( sv_player->v.movedir, pmove->movedir );
	VectorCopy( sv_player->v.v_angle, pmove->angles );
	VectorCopy( sv_player->v.basevelocity, pmove->basevelocity );
	VectorCopy( sv_player->v.view_ofs, pmove->view_ofs );
	VectorCopy( sv_player->v.punchangle, pmove->punchangle );
	pmove->deadflag = sv_player->v.deadflag;
	pmove->effects = sv_player->v.effects;
	pmove->gravity = sv_player->v.gravity;
	pmove->spectator = 0;
	pmove->friction = sv_player->v.friction;
	pmove->waterjumptime = sv_player->v.teleport_time;
	Q_memcpy( &pmove->cmd, &movementCommand, sizeof( movementCommand ) );
	pmove->dead = sv_player->v.health <= 0.0f;
	pmove->movetype = sv_player->v.movetype;
	pmove->flags = sv_player->v.flags;
	pmove->player_index = NUM_FOR_EDICT( sv_player ) - 1;
	pmove->iuser1 = sv_player->v.iuser1;
	pmove->iuser2 = sv_player->v.iuser2;
	pmove->iuser3 = sv_player->v.iuser3;
	pmove->iuser4 = sv_player->v.iuser4;
	pmove->fuser1 = sv_player->v.fuser1;
	pmove->fuser2 = sv_player->v.fuser2;
	pmove->fuser3 = sv_player->v.fuser3;
	pmove->fuser4 = sv_player->v.fuser4;
	VectorCopy( sv_player->v.vuser1, pmove->vuser1 );
	VectorCopy( sv_player->v.vuser2, pmove->vuser2 );
	VectorCopy( sv_player->v.vuser3, pmove->vuser3 );
	VectorCopy( sv_player->v.vuser4, pmove->vuser4 );
	VectorCopy( sv_player->v.origin, pmove->origin );
	SV_AddLinksToPM( sv_areanodes, pmove->origin );
	pmove->frametime = frametime;
	pmove->runfuncs = 1;
	pmove->PM_PlaySound = PM_SV_PlaySound;
	pmove->PM_TraceTexture = PM_SV_TraceTexture;
	pmove->PM_PlaybackEventFull = PM_SV_PlaybackEventFull;
	gEntityInterface.pfnPM_Move( pmove, 1 );

	sv_player->v.deadflag = pmove->deadflag;
	sv_player->v.effects = pmove->effects;
	sv_player->v.teleport_time = pmove->waterjumptime;
	sv_player->v.waterlevel = pmove->waterlevel;
	sv_player->v.watertype = pmove->watertype;
	sv_player->v.flags = pmove->flags;
	sv_player->v.friction = pmove->friction;
	sv_player->v.movetype = pmove->movetype;
	sv_player->v.maxspeed = pmove->clientmaxspeed;
	sv_player->v.iStepLeft = pmove->iStepLeft;
	VectorCopy( pmove->view_ofs, sv_player->v.view_ofs );
	VectorCopy( pmove->movedir, sv_player->v.movedir );
	VectorCopy( pmove->punchangle, sv_player->v.punchangle );
	if ( pmove->onground == -1 )
		sv_player->v.flags &= ~FL_ONGROUND;
	else
	{
		sv_player->v.flags |= FL_ONGROUND;
		sv_player->v.groundentity = EDICT_NUM( pmove->physents[pmove->onground].info );
	}
	VectorCopy( pmove->origin, sv_player->v.origin );
	VectorCopy( pmove->velocity, sv_player->v.velocity );
	VectorCopy( pmove->basevelocity, sv_player->v.basevelocity );
	if ( !sv_player->v.fixangle )
	{
		VectorCopy( pmove->angles, sv_player->v.v_angle );
		VectorCopy( pmove->angles, sv_player->v.angles );
		sv_player->v.angles[0] = static_cast<float>( -pmove->angles[0] / 3.0 );
	}
	sv_player->v.bInDuck = pmove->bInDuck;
	sv_player->v.flDuckTime = static_cast<int>( pmove->flDuckTime );
	sv_player->v.flTimeStepSound = pmove->flTimeStepSound;
	sv_player->v.flFallVelocity = pmove->flFallVelocity;
	sv_player->v.flSwimTime = static_cast<int>( pmove->flSwimTime );
	sv_player->v.oldbuttons = pmove->cmd.buttons;
	sv_player->v.iuser1 = pmove->iuser1;
	sv_player->v.iuser2 = pmove->iuser2;
	sv_player->v.iuser3 = pmove->iuser3;
	sv_player->v.iuser4 = pmove->iuser4;
	sv_player->v.fuser1 = pmove->fuser1;
	sv_player->v.fuser2 = pmove->fuser2;
	sv_player->v.fuser3 = pmove->fuser3;
	sv_player->v.fuser4 = pmove->fuser4;
	VectorCopy( pmove->vuser1, sv_player->v.vuser1 );
	VectorCopy( pmove->vuser2, sv_player->v.vuser2 );
	VectorCopy( pmove->vuser3, sv_player->v.vuser3 );
	VectorCopy( pmove->vuser4, sv_player->v.vuser4 );
	SetMinMaxSize( sv_player, player_mins[pmove->usehull], player_maxs[pmove->usehull], 0 );

	if ( host_client->edict->v.solid )
	{
		SV_LinkEdict( sv_player, 1 );
		vec3_t savedVelocity;
		VectorCopy( sv_player->v.velocity, savedVelocity );
		for ( int index = 0; index < pmove->numtouch; ++index )
		{
			pmtrace_t* touch = &pmove->touchindex[index];
			edict_t* entity = EDICT_NUM( pmove->physents[touch->ent].info );
			trace_t trace;
			SV_ConvertPMTrace( &trace, touch, entity );
			VectorCopy( touch->deltavelocity, sv_player->v.velocity );
			SV_Impact( entity, sv_player, &trace );
		}
		VectorCopy( savedVelocity, sv_player->v.velocity );
	}

	gGlobalVariables.time = static_cast<float>( host_client->svtimebase );
	gGlobalVariables.frametime = frametime;
	gEntityInterface.pfnPlayerPostThink( sv_player );
	gEntityInterface.pfnCmdEnd( sv_player );
	if ( !host_client->fakeclient )
		SV_RestoreMove( host_client );
}

void SV_ForceFullClientsUpdate()
{
	constexpr int FULL_UPDATE_MESSAGE_BYTES = 9 * 1024;
	byte data[FULL_UPDATE_MESSAGE_BYTES];
	sizebuf_t message = {};
	message.buffername = "Force Update";
	message.data = data;
	message.maxsize = sizeof( data );
	client_t* client = svs.clients;
	for ( int index = 0; index < svs.maxclients; ++index, ++client )
	{
		if ( client == host_client || client->active || client->connected || client->spawned )
			SV_FullClientUpdate( client, &message );
	}
	Con_DPrintf( "Client %s started recording. Send full update.\n", host_client->name );
	Netchan_CreateFragments( 1, &host_client->netchan, &message );
	Netchan_FragSend( &host_client->netchan );
}

void SV_SendEnts_f()
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}
	if ( host_client->active && host_client->spawned && host_client->connected )
		host_client->fully_connected = 1;
}

int SV_TransferConsistencyInfo()
{
	constexpr int CONSISTENCY_PATH_BYTES = 260;
	constexpr int RESOURCE_BOUNDS_MIN_OFFSET = sizeof( byte );
	constexpr int RESOURCE_BOUNDS_MAX_OFFSET = RESOURCE_BOUNDS_MIN_OFFSET + sizeof( vec3_t );
	int count = 0;
	for ( int index = 0; index < sv.num_resources; ++index )
	{
		resource_t* resource = &sv.resourcelist[index];
		if ( resource->ucFlags & RES_CHECKFILE )
			continue;
		consistency_t* consistency = sv.consistency_list;
		while ( consistency->filename && Q_strcasecmp( resource->szFileName, consistency->filename ) != 0 )
			++consistency;
		if ( !consistency->filename )
			continue;

		resource->ucFlags |= RES_CHECKFILE;
		char filename[CONSISTENCY_PATH_BYTES];
		if ( resource->type == t_sound )
		{
			_snprintf( filename, sizeof( filename ), "sound/%s", resource->szFileName );
		}
		else
		{
			Q_strncpy( filename, resource->szFileName, sizeof( filename ) - 1 );
			filename[sizeof( filename ) - 1] = 0;
		}
		MD5_Hash_File( resource->rgucMD5_hash, filename, 0, 0, nullptr );
		if ( resource->type == t_model )
		{
			vec3_t mins;
			vec3_t maxs;
			if ( consistency->check_type == force_model_samebounds )
			{
				if ( !R_GetStudioBounds( filename, mins, maxs ) )
					Host_Error( "Server unable to get bounds for %s\n", filename );
			}
			else if ( consistency->check_type == force_model_specifybounds
					  || consistency->check_type == force_model_specifybounds_if_avail )
			{
				Q_memcpy( mins, consistency->mins, sizeof( mins ) );
				Q_memcpy( maxs, consistency->maxs, sizeof( maxs ) );
			}
			else
			{
				++count;
				continue;
			}
			Q_memcpy( &resource->rguc_reserved[RESOURCE_BOUNDS_MIN_OFFSET], mins, sizeof( mins ) );
			Q_memcpy( &resource->rguc_reserved[RESOURCE_BOUNDS_MAX_OFFSET], maxs, sizeof( maxs ) );
			resource->rguc_reserved[0] = static_cast<byte>( consistency->check_type );
			COM_Munge( resource->rguc_reserved, sizeof( resource->rguc_reserved ), svs.spawncount );
		}
		++count;
	}
	return count;
}
