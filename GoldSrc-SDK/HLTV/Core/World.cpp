#include "HLTV/Core/World.h"
#include "HLTV/Core/delta.h"
#include "HLTV/Core/DownloadURL.h"
#include "common/event_args.h"
#include "common/weaponinfo.h"
#include "common/IBaseSystem.h"
#include "HLTV/common/DemoFile.h"
#include "HLTV/common/NetChannel.h"
#include "HLTV/common/mem.h"
#include "HLTV/common/munge.h"
#include "HLTV/common/DirectorCmd.h"
#include "HLTV/common/IDirector.h"
#include "public/tier1/interface.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define MAX_WORLD_CLIENT_INDEX		( MAX_WORLD_CLIENTS - 1 )
#define MAX_WORLD_ENTITY_INDEX		( MAX_WORLD_ENTITIES - 1 )

#define WORLD_SIGNON_BYTES			0x8000
#define WORLD_SERVERINFO_BYTES		0x200
#define WORLD_CACHE_ENTRIES			32
#define WORLD_DEMOINFO_BYTES		436

#define WORLD_ENTITY_SENTINEL		9999
#define WORLD_EMPTY_ENTITY_TYPE		0x40000000
#define WORLD_FULLFRAME_DELTA		0xFFFFFFFFu
#define WORLD_VARIABLE_USER_MESSAGE	255

char g_DownloadURL[128];

void* CreateWorld()
{
	return new World();
}

static InterfaceReg __g_CreateIWorld_reg( CreateWorld, "world001" );

World::World()
{
}

World::~World()
{
}

static delta_t* DeltaFields( bool custom, int entityNumber, World* world )
{
	if ( custom )
		return g_pcustomentitydelta;
	if ( world->IsPlayerIndex( entityNumber ) )
		return g_pplayerdelta;
	return g_pentitydelta;
}

UserMsg* World::GetUserMsg( int msgNumber )
{
	UserMsg* message;

	message = ClientUserMsgs;
	if ( message == NULL )
		return NULL;
	while ( message != NULL && message->iMsg != msgNumber )
		message = message->next;
	return message;
}

bool World::IsPlayerIndex( int index )
{
	return index > 0 && index <= maxclients;
}

void World::SetTime( double newTime )
{
	m_System->DPrintf( "Synchronizing time %.2f ( %.2f)\n", newTime, newTime - worldTime );
	worldTime = newTime;
}

int World::GetNumPlayers()
{
	int count;
	int i;

	if ( maxclients <= 0 )
		return 0;
	count = 0;
	for ( i = 0; i < maxclients; ++i )
	{
		if ( players[i].active )
			++count;
	}
	return count;
}

bool World::IsActive()
{
	return state > WORLD_CONNECTING;
}

char* World::GetHostName()
{
	if ( hostName[0] != 0 )
		return hostName;
	return servername;
}

char* World::GetType()
{
	return "world001";
}

double World::GetTime()
{
	return worldTime;
}

int World::DuplicateFrames( unsigned int, unsigned int )
{
	m_System->Printf( "TODO World::DuplicateFrames\n" );
	return 0;
}

int World::MoveFrames( unsigned int, unsigned int, double )
{
	m_System->Printf( "TODO World::MoveFrames\n" );
	return 0;
}

int World::RevertFrames( unsigned int, unsigned int )
{
	m_System->Printf( "TODO World::RevertFrames\n" );
	return 0;
}

bool World::IsPaused()
{
	return m_IsPaused;
}

bool World::IsComplete()
{
	return state == WORLD_COMPLETE;
}

void World::SetBufferSize( float seconds )
{
	if ( seconds <= 0.0f )
		maxBufferLength = -1.0f;
	else
		maxBufferLength = seconds + 10.0f;
}

void World::SetCDInfo( int ncdtrack, int nlooptrack )
{
	cdtrack = ncdtrack;
	looptrack = nlooptrack;
}

InfoString* World::GetServerInfoString()
{
	return &serverinfo;
}

bool World::IsVoiceEnabled()
{
	return voiceEnabled;
}

int World::GetSlotNumber()
{
	return playernum;
}

void World::SetViewEntity( int nviewentity )
{
	viewentity = nviewentity;
}

void World::SetVoiceEnabled( bool enabled )
{
	voiceEnabled = enabled;
}

NetAddress* World::GetGameServerAddress()
{
	return &gameServerAddress;
}

char* World::GetLevelName()
{
	return levelname;
}

char* World::GetGameDir()
{
	return gamedir;
}

int World::GetServerCount()
{
	return servercount;
}

int World::GetMaxClients()
{
	return maxclients;
}

serverinfo_t* World::GetServerInfo()
{
	return &detailedServerInfo;
}

void World::SetHLTV( bool enabled )
{
	isHLTV = enabled;
}

bool World::IsHLTV()
{
	return isHLTV;
}

void World::SetExtraInfo( char* nclientfallback, int nallowCheats )
{
	strncpy( clientfallback, nclientfallback, 0x103u );
	clientfallback[259] = 0;
	allowCheats = nallowCheats != 0;
}

void World::StopGame()
{
	FireSignal( 1u, NULL );
	Reset();
	if ( state != WORLD_DISCONNECTED )
		state = WORLD_DISCONNECTED;
}

void World::SetServerInfo( serverinfo_t* info )
{
	memcpy( &detailedServerInfo, info, sizeof( detailedServerInfo ) );
}

void World::SetMoveVars( movevars_t* nmovevars )
{
	memcpy( &movevars, nmovevars, sizeof( movevars ) );
}

bool World::AddResource( resource_t* resource )
{
	resource_t* copy;

	copy = (resource_t*)Mem_ZeroMalloc( sizeof( resource_t ) );
	if ( copy == NULL )
		return false;
	memcpy( copy, resource, sizeof( resource_t ) );
	copy->data = NULL;
	copy->pNext = resourcesList;
	++num_resources;
	resourcesList = copy;
	return true;
}

void World::SetServerInfoString( char* infostring )
{
	serverinfo.SetString( infostring );
}

void World::WriteLightStyles( BitBuffer* stream )
{
	int index;

	for ( index = 0; index < MAX_WORLD_LIGHTSTYLES; ++index )
	{
		stream->WriteByte( svc_lightstyle );
		stream->WriteByte( index );
		stream->WriteString( lightstyles[index] );
	}
}

bool World::AddSignonData( unsigned char type, unsigned char* data, int size )
{
	signonData.WriteByte( type );
	signonData.WriteBuf( data, size );
	return signonData.sizeError;
}

void World::SetServerInfo( int protocol, CRC32_t nserverCRC, unsigned char* nclientdllmd5, int nmaxclients,
		int nplayernum, int ngametype, char* ngamedir, char* nservername, char* nlevelname )
{
	m_Protocol = protocol;
	serverCRC = nserverCRC;
	memcpy( clientdllmd5, nclientdllmd5, sizeof( clientdllmd5 ) );
	playernum = nplayernum;
	maxclients = nmaxclients;
	gametype = ngametype;
	strncpy( gamedir, ngamedir, sizeof( gamedir ) );
	gamedir[sizeof( gamedir ) - 1] = 0;
	strncpy( levelname, nlevelname, sizeof( levelname ) );
	levelname[sizeof( levelname ) - 1] = 0;
	snprintf( servername, 254, "%s:%i", nservername, nplayernum );
}

void World::WriteMovevars( BitBuffer* stream )
{
	stream->WriteByte( svc_newmovevars );
	stream->WriteFloat( movevars.gravity );
	stream->WriteFloat( movevars.stopspeed );
	stream->WriteFloat( movevars.maxspeed );
	stream->WriteFloat( movevars.spectatormaxspeed );
	stream->WriteFloat( movevars.accelerate );
	stream->WriteFloat( movevars.airaccelerate );
	stream->WriteFloat( movevars.wateraccelerate );
	stream->WriteFloat( movevars.friction );
	stream->WriteFloat( movevars.edgefriction );
	stream->WriteFloat( movevars.waterfriction );
	stream->WriteFloat( movevars.entgravity );
	stream->WriteFloat( movevars.bounce );
	stream->WriteFloat( movevars.stepsize );
	stream->WriteFloat( movevars.maxvelocity );
	stream->WriteFloat( movevars.zmax );
	stream->WriteFloat( movevars.waveHeight );
	stream->WriteByte( movevars.footsteps != 0 );
	stream->WriteFloat( movevars.rollangle );
	stream->WriteFloat( movevars.rollspeed );
	stream->WriteFloat( movevars.skycolor_r );
	stream->WriteFloat( movevars.skycolor_g );
	stream->WriteFloat( movevars.skycolor_b );
	stream->WriteFloat( movevars.skyvec_x );
	stream->WriteFloat( movevars.skyvec_y );
	stream->WriteFloat( movevars.skyvec_z );
	stream->WriteString( movevars.skyName );
}

frame_t* World::GetFrameBySeqNr( unsigned int seqnr )
{
	return (frame_t*)frames.FindExactKey( (float)seqnr );
}

bool World::GetClientData( unsigned int SeqNr, clientdata_t* clientData )
{
	frame_t* frame;

	frame = (frame_t*)frames.FindExactKey( (float)SeqNr );
	return GetClientData( frame, clientData );
}

frame_t* World::GetLastFrame()
{
	return (frame_t*)frames.GetLast();
}

void World::ParseEvent( BitBuffer* stream )
{
	event_args_t nullargs;

	DELTA_ParseDelta( stream, (unsigned char*)&nullargs, (unsigned char*)&nullargs, g_peventdelta );
}

void World::WriteSigonData( BitBuffer* stream )
{
	stream->WriteBuf( signonData.data, signonData.CurrentSize() );
}

int World::FindUserMsgByName( char* name )
{
	UserMsg* message;

	message = ClientUserMsgs;
	if ( message == NULL )
		return 0;
	while ( strcmp( message->szName, name ) != 0 )
	{
		message = message->next;
		if ( message == NULL )
			return 0;
	}
	return message->iMsg;
}

void World::SetGameServerAddress( NetAddress* address )
{
	gameServerAddress.FromNetAddress( address );
}

bool World::AddUserMessage( int msgNumber, int size, char* name )
{
	UserMsg* message;
	UserMsg* created;
	int storedSize;
	bool found;

	storedSize = size;
	if ( size == WORLD_VARIABLE_USER_MESSAGE )
		storedSize = -1;

	found = false;
	for ( message = ClientUserMsgs; message != NULL; message = message->next )
	{
		if ( strcasecmp( message->szName, name ) == 0 )
		{
			message->iMsg = msgNumber;
			message->iSize = storedSize;
			found = true;
		}
	}
	if ( ClientUserMsgs != NULL && found )
		return false;

	created = (UserMsg*)malloc( sizeof( UserMsg ) );
	created->iMsg = msgNumber;
	created->iSize = storedSize;
	strncpy( created->szName, name, sizeof( created->szName ) );
	created->next = ClientUserMsgs;
	ClientUserMsgs = created;
	return true;
}

void World::ParseDeltaDescription( BitBuffer* stream )
{
	delta_description_t nulldesc;
	char szDesc[256];
	char* name;
	delta_t** ppdelta;
	int count;
	int i;
	delta_description_t* pdesc;

	memset( &nulldesc, 0, sizeof( nulldesc ) );
	name = stream->ReadString();
	if ( name == NULL || name[0] == 0 )
	{
		m_System->Errorf( "ParseDeltaDescription:  Illegible description name\n" );
		stream->sizeError = true;
		return;
	}

	strncpy( szDesc, name, 0xFFu );
	szDesc[255] = 0;
	m_System->DPrintf( "Reading delta description for: %s.\n", name );
	ppdelta = DELTA_LookupRegistration( szDesc );
	if ( ppdelta == NULL )
	{
		m_System->Printf( "WARNING! World::ParseDeltaDescription: unregistered description name %s.\n", szDesc );
		stream->sizeError = true;
		return;
	}
	if ( *ppdelta != NULL )
		DELTA_FreeDescription( ppdelta );
	stream->StartBitMode();
	count = stream->ReadBits( 16 );
	*ppdelta = (delta_t*)Mem_ZeroMalloc( sizeof( delta_t ) );
	pdesc = (delta_description_t*)Mem_ZeroMalloc( sizeof( delta_description_t ) * count );
	( *ppdelta )->dynamic = 1;
	( *ppdelta )->fieldCount = count;
	for ( i = 0; i < count; ++i )
		DELTA_ParseDelta( stream, (unsigned char*)&nulldesc, (unsigned char*)&pdesc[i], &g_MetaDelta );
	( *ppdelta )->pdd = pdesc;
	stream->EndBitMode();
	DELTA_UpdateDescriptions();
}

void World::FinishGame()
{
	if ( state == WORLD_RUNNING )
	{
		state = WORLD_COMPLETE;
		FireSignal( 7u, NULL );
	}
}

void World::WriteClientUpdate( BitBuffer* stream, int playerIndex )
{
	if ( !players[playerIndex].active )
		return;
	stream->WriteByte( svc_updateuserinfo );
	stream->WriteByte( playerIndex );
	stream->WriteLong( players[playerIndex].userid );
	stream->WriteString( players[playerIndex].userinfo );
	stream->WriteBuf( players[playerIndex].hashedcdkey, 16 );
}

void World::SetPaused( bool paused )
{
	if ( m_IsPaused == paused )
		return;
	m_IsPaused = paused;
	if ( paused )
		FireSignal( 5u, NULL );
	else
		FireSignal( 6u, NULL );
}

void World::AddLightStyle( int index, char* style )
{
	int length;

	if ( (unsigned int)index > 0x3Fu )
	{
		m_System->Printf( "WARNING! World::SetLightStyle: index (%i) out of bounds.\n", index );
		return;
	}
	length = (int)strlen( style );
	if ( length > WORLD_LIGHTSTYLE_LENGTH )
		m_System->Printf( "WARNING! World::SetLightStyle: style too long (%i).\n", length );
	strncpy( lightstyles[index], style, 0x40u );
	lightstyles[index][63] = 0;
}

bool World::GetPlayerInfoString( int playerNum, InfoString* infoString )
{
	if ( infoString == NULL )
		return false;
	if ( (unsigned int)playerNum > 0x1Fu )
		return false;
	if ( players[playerNum].userinfo[0] == 0 )
		return false;
	if ( !players[playerNum].active )
		return false;
	return infoString->SetString( players[playerNum].userinfo );
}

void World::SetHostName( char* name )
{
	if ( name != NULL && name[0] != 0 )
		strncpy( hostName, name, 0xFEu );
	else
		memset( hostName, 0, sizeof( hostName ) );
}

void World::AddInstancedBaselineEntity( int index, entity_state_t* ent )
{
	if ( (unsigned int)index > 0x3Fu )
	{
		m_System->Printf( "WARNING! World::SetInstancedBaselineEntity: index (%i) out of bounds.\n", index );
		return;
	}
	memcpy( &instanced_baselines[index], ent, sizeof( entity_state_t ) );
}

void World::AddBaselineEntity( int index, entity_state_t* ent )
{
	if ( (unsigned int)index > (unsigned int)MAX_WORLD_ENTITY_INDEX )
	{
		m_System->Printf( "WARNING! World::SetBaselineEntity: index (%i) out of bounds.\n", index );
		return;
	}
	memcpy( &baselines[index], ent, sizeof( entity_state_t ) );
}

void World::UpdateServerInfo()
{
	char* name;

	strncpy( detailedServerInfo.address, "", 0x3Fu );
	detailedServerInfo.address[63] = 0;
	name = GetHostName();
	strncpy( detailedServerInfo.name, name, 0x4Fu );
	detailedServerInfo.name[79] = 0;
	COM_FileBase( levelname, detailedServerInfo.map );
	detailedServerInfo.map[15] = 0;
	strncpy( detailedServerInfo.gamedir, gamedir, 0xFFu );
	detailedServerInfo.gamedir[255] = 0;
	strncpy( detailedServerInfo.description, "HLTV", 0xFFu );
	detailedServerInfo.description[255] = 0;
	detailedServerInfo.type = 'd';
#ifdef _WIN32
	detailedServerInfo.os = 'w';
#else
	detailedServerInfo.os = 'l';
#endif
	detailedServerInfo.activePlayers = playernum;
	detailedServerInfo.pw = '0';
	detailedServerInfo.mod = false;
	detailedServerInfo.maxPlayers = maxclients;
	detailedServerInfo.protocol = (unsigned char)m_Protocol;
}

void World::ParseBaseline( BitBuffer* stream )
{
	delta_t** entityDelta;
	delta_t** playerDelta;
	delta_t** customDelta;
	entity_state_t nullstate;
	entity_state_t entstate;
	int count;
	int index;
	int type;
	unsigned int i;

	entityDelta = DELTA_LookupRegistration( "entity_state_t" );
	playerDelta = DELTA_LookupRegistration( "entity_state_player_t" );
	customDelta = DELTA_LookupRegistration( "custom_entity_state_t" );
	if ( entityDelta == NULL || *entityDelta == NULL || customDelta == NULL || *customDelta == NULL ||
			playerDelta == NULL || *playerDelta == NULL )
	{
		m_System->Errorf( "Server::ParseBaseline: needed delta encoder missing.\n" );
		return;
	}

	memset( &nullstate, 0, sizeof( nullstate ) );
	stream->StartBitMode();
	max_baseline = 0;
	ClearBaseline();
	count = 0;
	while ( stream->PeekBits( 16 ) != 0xFFFF )
	{
		delta_t* fields;

		index = stream->ReadBits( 11 );
		if ( max_baseline < index )
			max_baseline = index;
		memset( &entstate, 0, sizeof( entstate ) );
		type = stream->ReadBits( 2 );
		if ( ( type & 2 ) != 0 )
			fields = *customDelta;
		else if ( IsPlayerIndex( index ) )
			fields = *playerDelta;
		else
			fields = *entityDelta;
		DELTA_ParseDelta( stream, (unsigned char*)&nullstate, (unsigned char*)&entstate, fields );
		entstate.entityType = type;
		AddBaselineEntity( index, &entstate );
		++count;
	}
	m_System->Printf( "Received baseline with %i entities.\n", count );
	stream->ReadBits( 16 );
	ClearInstancedBaseline();
	max_instanced_baseline = stream->ReadBits( 6 );
	for ( i = 0; (int)i < max_instanced_baseline; ++i )
	{
		memset( &entstate, 0, sizeof( entstate ) );
		DELTA_ParseDelta( stream, (unsigned char*)&nullstate, (unsigned char*)&entstate, *entityDelta );
		AddInstancedBaselineEntity( (int)i, &entstate );
	}
	if ( max_instanced_baseline != 0 )
		m_System->Printf( "Received instanced baseline with %i entities.\n", max_instanced_baseline );
	stream->EndBitMode();
}

void World::NewGame( int newServerCount )
{
	FireSignal( 1u, NULL );
	Reset();
	servercount = newServerCount;
	SetState( WORLD_CONNECTING );
}

bool World::UncompressEntitiesFromStream( frame_t* frame, BitBuffer* stream )
{
	entity_state_t* to;
	int entityNumber;
	int newindex;
	unsigned int expected;
	bool failed;

	to = (entity_state_t*)frame->entities;
	expected = frame->entitynum;
	DELTA_SetTime( frame->time );
	stream->StartBitMode();
	failed = stream->sizeError;
	newindex = 0;
	entityNumber = 0;
	if ( !failed )
	{
		while ( !stream->sizeError )
		{
			unsigned char* from;
			bool custom;
			unsigned int instancedIndex;
			unsigned int offset;

			if ( stream->PeekBits( 16 ) == 0 )
				break;
			if ( stream->ReadBit() != 0 )
				++entityNumber;
			else if ( stream->ReadBit() != 0 )
				entityNumber = stream->ReadBits( 11 );
			else
				entityNumber += stream->ReadBits( 6 );

			custom = stream->ReadBit() != 0;
			if ( max_instanced_baseline != 0 && stream->ReadBit() != 0 )
			{
				instancedIndex = stream->ReadBits( 6 );
				if ( entityNumber > MAX_WORLD_ENTITY_INDEX )
					goto entity_overflow;
				from = (unsigned char*)&instanced_baselines[instancedIndex];
			}
			else if ( stream->ReadBit() == 0 )
			{
				if ( entityNumber > MAX_WORLD_ENTITY_INDEX )
					goto entity_overflow;
				from = (unsigned char*)&baselines[entityNumber];
			}
			else
			{
				offset = stream->ReadBits( 6 );
				if ( entityNumber > MAX_WORLD_ENTITY_INDEX )
					goto entity_overflow;
				if ( offset == 0 )
					from = (unsigned char*)&baselines[entityNumber];
				else
					from = (unsigned char*)&( (entity_state_t*)frame->entities )[newindex - (int)offset];
			}

			if ( !custom )
			{
				DELTA_ParseDelta( stream, from, (unsigned char*)to, DeltaFields( false, entityNumber, this ) );
				to->entityType = 1;
			}
			else
			{
				DELTA_ParseDelta( stream, from, (unsigned char*)to, g_pcustomentitydelta );
				to->entityType = 2;
			}
			to->number = entityNumber;
			++to;
			++newindex;
		}
		if ( stream->sizeError )
			failed = true;
	}

	if ( failed )
		m_System->Printf( "WARNING! World::UncompressEntitiesFromStream: incoming entities are corrupt.\n" );
	if ( stream->ReadShort() != 0 )
	{
		m_System->Printf( "WARNING! World::UncompressEntitiesFromStream: missing end tag.\n" );
		return false;
	}
	stream->EndBitMode();
	if ( expected != (unsigned int)newindex )
	{
		m_System->DPrintf( "WARNING! World::UncompressEntitiesFromStream: newindex != entnum.\n" );
		return false;
	}
	return true;

entity_overflow:
	m_System->Errorf( "World::GetUncompressedFrame: entity number %i > MAX_ENTITIES\n", entityNumber );
	memset( frame, 0, sizeof( *frame ) );
	return false;
}

bool World::SetMaxClients( int max )
{
	if ( (unsigned int)( max - 1 ) > 0x1Fu )
	{
		m_System->Printf( "WARNING! World::SetMaxClients: Bad maxclients (%u)\n", max );
		return false;
	}
	maxclients = max;
	return true;
}

void World::ClearUserMessages()
{
	UserMsg* message;
	UserMsg* next;

	message = ClientUserMsgs;
	while ( message != NULL )
	{
		next = message->next;
		free( message );
		message = next;
	}
	ClientUserMsgs = NULL;
}

void World::ClearResources()
{
	resource_t* resource;
	resource_t* next;

	resource = resourcesList;
	while ( resource != NULL )
	{
		next = resource->pNext;
		free( resource );
		resource = next;
	}
	resourcesList = NULL;
	num_resources = 0;
}

void World::ClearBaseline()
{
	int i;

	memset( baselines, 0, sizeof( baselines ) );
	for ( i = 0; i < MAX_WORLD_ENTITIES; ++i )
		baselines[i].entityType = WORLD_EMPTY_ENTITY_TYPE;
}

void World::ClearInstancedBaseline()
{
	memset( instanced_baselines, 0, sizeof( instanced_baselines ) );
}

void World::ClearLightStyles()
{
	memset( lightstyles, 0, sizeof( lightstyles ) );
}

void World::SetState( WorldState newState )
{
	if ( state == newState )
		return;
	switch ( newState )
	{
	case WORLD_INITILAZING:
	case WORLD_DISCONNECTED:
		state = newState;
		break;
	case WORLD_CONNECTING:
		if ( (unsigned int)( state - WORLD_RUNNING ) <= 1u || state == WORLD_DISCONNECTED )
			state = newState;
		else
			m_System->Errorf( "World::SetState: not valid m_State (%i -> %i).\n", state, newState );
		break;
	case WORLD_RUNNING:
		if ( state == WORLD_CONNECTING )
			state = newState;
		else
			m_System->Errorf( "World::SetState: not valid m_State (%i -> %i).\n", state, newState );
		break;
	case WORLD_COMPLETE:
		if ( state == WORLD_RUNNING )
			state = newState;
		else
			m_System->Errorf( "World::SetState: not valid m_State (%i -> %i).\n", state, newState );
		break;
	default:
		m_System->Errorf( "World::SetState: not valid m_State (%i -> %i).\n", state, newState );
		break;
	}
}

void World::ConnectionComplete()
{
	WriteBaseline( &signonData );
	startTime = m_SystemTime;
	SetState( WORLD_RUNNING );
	FireSignal( 2u, NULL );
}

void World::ClearPlayers()
{
	memset( players, 0, sizeof( players ) );
}

void World::WriteBaseline( BitBuffer* stream )
{
	entity_state_t nullstate;
	int index;
	int instanced;

	if ( g_pcustomentitydelta == NULL || g_pentitydelta == NULL || g_pplayerdelta == NULL )
	{
		m_System->Errorf( "World::WriteBaseline: delta encoder not found.\n" );
		return;
	}

	memset( &nullstate, 0, sizeof( nullstate ) );
	stream->WriteByte( svc_spawnbaseline );
	stream->StartBitMode();
	for ( index = 0; index < MAX_WORLD_ENTITIES; ++index )
	{
		delta_t* fields;

		if ( baselines[index].entityType == WORLD_EMPTY_ENTITY_TYPE )
			continue;
		stream->WriteBits( (unsigned int)index, 11 );
		stream->WriteBits( (unsigned int)baselines[index].entityType, 2 );
		if ( ( baselines[index].entityType & 1 ) == 0 )
			fields = g_pcustomentitydelta;
		else if ( IsPlayerIndex( index ) )
			fields = g_pplayerdelta;
		else
			fields = g_pentitydelta;
		DELTA_WriteDelta( stream, (unsigned char*)&nullstate, (unsigned char*)&baselines[index], true, fields, NULL );
	}
	stream->WriteBits( 0xFFFFu, 16 );
	stream->WriteBits( (unsigned int)max_instanced_baseline, 6 );
	for ( instanced = 0; instanced < max_instanced_baseline; ++instanced )
	{
		DELTA_WriteDelta( stream, (unsigned char*)&nullstate, (unsigned char*)&instanced_baselines[instanced],
				true, g_pentitydelta, NULL );
	}
	stream->EndBitMode();
}

void World::WriteDeltaDescriptions( BitBuffer* stream )
{
	delta_description_t nulldesc;
	delta_registry_t* entry;

	memset( &nulldesc, 0, sizeof( nulldesc ) );
	for ( entry = g_deltaregistry; entry != NULL; entry = entry->next )
	{
		unsigned int count;
		unsigned int i;

		stream->WriteByte( svc_deltadescription );
		stream->WriteString( entry->name );
		if ( entry->pdesc == NULL )
		{
			m_System->Errorf( "World::WriteDeltaDescriptions : missing delta for %s\n", entry->name );
			return;
		}
		count = (unsigned int)entry->pdesc->fieldCount;
		stream->StartBitMode();
		stream->WriteBits( count, 16 );
		for ( i = 0; i < count; ++i )
		{
			DELTA_WriteDelta( stream, (unsigned char*)&nulldesc, (unsigned char*)&entry->pdesc->pdd[i],
					true, &g_MetaDelta, NULL );
		}
		stream->EndBitMode();
	}
}

void World::WriteServerinfo( BitBuffer* stream )
{
	char message[2048];
	const char* hltv;
	CRC32_t mungebuffer;

	hltv = isHLTV ? "(HLTV)" : "";
	snprintf( message, sizeof( message ), "Protocol Version %i, Spawn count %i %s\n", m_Protocol, servercount, hltv );
	stream->WriteByte( svc_print );
	stream->WriteString( message );
	if ( isHLTV )
	{
		stream->WriteByte( svc_hltv );
		stream->WriteByte( 0 );
	}
	stream->WriteByte( svc_serverinfo );
	stream->WriteLong( m_Protocol );
	stream->WriteLong( servercount );
	mungebuffer = serverCRC;
	COM_Munge3( (unsigned char*)&mungebuffer, 4, (unsigned char)( -1 - playernum ) );
	stream->WriteLong( mungebuffer );
	stream->WriteBuf( clientdllmd5, 16 );
	stream->WriteByte( maxclients );
	stream->WriteByte( playernum );
	stream->WriteByte( gametype );
	stream->WriteString( gamedir );
	stream->WriteString( GetHostName() );
	stream->WriteString( levelname );
	stream->WriteString( "mapcycle failure" );
	stream->WriteByte( 0 );
	stream->WriteByte( svc_sendextrainfo );
	stream->WriteString( clientfallback );
	stream->WriteByte( allowCheats );
	WriteDeltaDescriptions( stream );
	WriteMovevars( stream );
	stream->WriteByte( svc_cdtrack );
	stream->WriteByte( cdtrack );
	stream->WriteByte( cdtrack );
	stream->WriteByte( svc_setview );
	stream->WriteShort( viewentity );
}

void World::WriteRegisteredUserMessages( BitBuffer* stream )
{
	UserMsg* message;

	for ( message = ClientUserMsgs; message != NULL; message = message->next )
	{
		stream->WriteByte( svc_newusermsg );
		stream->WriteByte( message->iMsg );
		stream->WriteByte( message->iSize );
		stream->WriteBuf( message->szName, 16 );
	}
}

void World::WriteResources( BitBuffer* stream )
{
	unsigned char nullbuffer[32];
	resource_t* resource;

	if ( g_DownloadURL[0] != 0 )
	{
		stream->WriteByte( svc_resourcelocation );
		stream->WriteString( g_DownloadURL );
	}
	memset( nullbuffer, 0, sizeof( nullbuffer ) );
	stream->WriteByte( svc_resourcelist );
	stream->StartBitMode();
	stream->WriteBits( (unsigned int)num_resources, 12 );
	for ( resource = resourcesList; resource != NULL; resource = resource->pNext )
	{
		stream->WriteBits( (unsigned int)resource->type, 4 );
		stream->WriteBitString( resource->szFileName );
		stream->WriteBits( (unsigned int)resource->nIndex, 12 );
		stream->WriteBits( (unsigned int)resource->nDownloadSize, 24 );
		stream->WriteBits( resource->ucFlags & 3u, 3 );
		if ( ( resource->ucFlags & 4 ) != 0 )
			stream->WriteBitData( resource->rgucMD5_hash, 16 );
		if ( memcmp( resource->rguc_reserved, nullbuffer, sizeof( nullbuffer ) ) != 0 )
		{
			stream->WriteBit( 1 );
			stream->WriteBitData( resource->rguc_reserved, 32 );
		}
		else
		{
			stream->WriteBit( 0 );
		}
	}
	stream->WriteBit( 0 );
	stream->EndBitMode();
}

void World::WriteNewData( BitBuffer* stream )
{
	char* info;
	char* command;

	WriteServerinfo( stream );
	WriteRegisteredUserMessages( stream );
	stream->WriteByte( svc_stufftext );
	info = serverinfo.GetString();
	command = COM_VarArgs( "fullserverinfo \"%s\"\n", info );
	stream->WriteString( command );
	WriteResources( stream );
}

int World::ParseDeltaHeader( BitBuffer* stream, bool* remove, bool* custom, int* numbase, bool* newbl,
		int* newblindex, bool full, int* offset )
{
	unsigned int entityNumber;

	*custom = false;
	*newbl = false;
	if ( full )
	{
		*remove = false;
		if ( stream->ReadBit() != 0 )
		{
			entityNumber = (unsigned int)( *numbase + 1 );
			goto have_number;
		}
	}
	else
	{
		*remove = stream->ReadBit() != 0;
	}

	if ( stream->ReadBit() == 0 )
	{
		entityNumber = (unsigned int)( *numbase + stream->ReadBits( 6 ) );
		*numbase = (int)entityNumber;
		if ( *remove )
			return (int)entityNumber;
		goto have_custom;
	}
	entityNumber = stream->ReadBits( 11 );

have_number:
	*numbase = (int)entityNumber;
	if ( *remove )
		return (int)entityNumber;

have_custom:
	*custom = stream->ReadBit() != 0;
	if ( max_instanced_baseline != 0 )
	{
		*newbl = stream->ReadBit() != 0;
		if ( *newbl )
		{
			*newblindex = (int)stream->ReadBits( 6 );
			if ( !full || *newbl )
			{
				*offset = 0;
				return (int)entityNumber;
			}
		}
	}
	else
	{
		*newbl = false;
	}
	if ( !full )
	{
		*offset = 0;
		return (int)entityNumber;
	}
	*offset = stream->ReadBit();
	if ( *offset != 0 )
		*offset = (int)stream->ReadBits( 6 );
	return (int)entityNumber;
}

int World::FindBestBaseline( int index, entity_state_t** baseline, entity_state_t* to, int num, bool custom )
{
	entity_state_t* target;
	delta_t* fields;
	int bestBits;
	int threshold;
	int bestIndex;
	int i;

	target = &to[index];
	fields = DeltaFields( custom, num, this );
	bestBits = DELTA_TestDelta( (unsigned char*)*baseline, (unsigned char*)target, fields );
	threshold = bestBits - 6;
	i = index - 1;
	if ( i < 0 || threshold <= 0 )
		return 0;

	bestIndex = index;
	do
	{
		if ( to[i].modelindex == target->modelindex )
		{
			int bits;

			bits = DELTA_TestDelta( (unsigned char*)&to[i], (unsigned char*)target, fields );
			if ( bits < threshold )
			{
				bestIndex = i;
				threshold = bits;
			}
		}
		--i;
		if ( threshold <= 0 || i < 0 )
			break;
	}
	while ( i != index - 63 );

	if ( bestIndex != index )
		*baseline = &to[bestIndex];
	return index - bestIndex;
}

int World::CompressFrame( frame_t* from, BitBuffer* stream )
{
	deltacallback_t header;
	entity_state_t* entities;
	unsigned char* start;
	int i;

	if ( from->delta != 0 )
	{
		m_System->Errorf( "World::CompressFrame: frame was not uncompressed.\n" );
		return 0;
	}

	memset( &header, 0, sizeof( header ) );
	header.full = true;
	header.instanced_baseline = max_instanced_baseline > 0;
	entities = (entity_state_t*)from->entities;
	DELTA_SetTime( from->time );
	start = stream->currentByte;
	stream->StartBitMode();
	for ( i = 0; i < (int)from->entitynum; ++i )
	{
		entity_state_t* baseline;
		delta_t* fields;

		header.num = entities[i].number;
		header.custom = ( entities[i].entityType & 2 ) != 0;
		header.newbl = false;
		header.newblindex = 0;
		baseline = &baselines[header.num];
		header.offset = FindBestBaseline( i, &baseline, entities, header.num, header.custom );
		fields = DeltaFields( header.custom, header.num, this );
		DELTA_WriteDelta( stream, (unsigned char*)baseline, (unsigned char*)&entities[i], true, fields, &header );
	}
	stream->WriteBits( 0, 16 );
	stream->EndBitMode();
	return (int)( stream->currentByte - start );
}

void World::ClearEntityCache()
{
	int i;

	if ( deltaCache != NULL )
	{
		for ( i = 0; i < maxCacheIndex; ++i )
			deltaCache[i].buffer.Free();
		memset( deltaCache, 0, sizeof( deltaCache_t ) * maxCacheIndex );
	}
	if ( frameCache != NULL )
		memset( frameCache, 0, sizeof( frameCache_t ) * maxCacheIndex );
	cacheHits = 1;
	cacheFaults = 1;
}

bool World::GetFrameFromCache( unsigned int seqNr, entity_state_t** entities )
{
	static int next = 0;
	int i;
	int slot;

	if ( maxCacheIndex > 0 )
	{
		for ( i = 0; i < maxCacheIndex; ++i )
		{
			if ( frameCache[i].seqNr == seqNr )
			{
				*entities = frameCache[i].entities;
				++cacheHits;
				return true;
			}
		}
	}

	slot = next++;
	if ( next == maxCacheIndex )
		next = 0;
	frameCache[slot].seqNr = seqNr;
	*entities = frameCache[slot].entities;
	++cacheFaults;
	return false;
}

bool World::GetDeltaFromCache( unsigned int seqNr, unsigned int deltaNr, BitBuffer** buffer )
{
	static int next = 0;
	int i;
	int slot;

	if ( maxCacheIndex > 0 )
	{
		for ( i = 0; i < maxCacheIndex; ++i )
		{
			if ( deltaCache[i].seqNr == seqNr && deltaCache[i].deltaNr == deltaNr )
			{
				*buffer = &deltaCache[i].buffer;
				++cacheHits;
				return true;
			}
		}
	}

	slot = next;
	next = next + 1;
	if ( next == maxCacheIndex )
		next = 0;
	deltaCache[slot].seqNr = seqNr;
	deltaCache[slot].deltaNr = deltaNr;
	*buffer = &deltaCache[slot].buffer;
	++cacheFaults;
	return false;
}

void World::WritePacketEntities( BitBuffer* stream, frame_t* frame, frame_t* deltaframe )
{
	deltacallback_t header;
	entity_state_t* newEnts;
	entity_state_t* oldEnts;
	unsigned int newIndex;
	unsigned int oldIndex;

	if ( frame->delta != 0 || deltaframe->delta != 0 )
	{
		m_System->Errorf( "World::WritePacketEntities: frame and delta frame must be uncompressed.\n" );
		return;
	}

	memset( &header, 0, sizeof( header ) );
	header.full = false;
	header.instanced_baseline = max_instanced_baseline > 0;
	newEnts = (entity_state_t*)frame->entities;
	oldEnts = (entity_state_t*)deltaframe->entities;
	newIndex = 0;
	oldIndex = 0;
	DELTA_SetTime( frame->time );
	stream->StartBitMode();
	while ( newIndex < frame->entitynum || oldIndex < deltaframe->entitynum )
	{
		int newNum;
		int oldNum;

		newNum = WORLD_ENTITY_SENTINEL;
		oldNum = WORLD_ENTITY_SENTINEL;
		if ( newIndex < frame->entitynum )
			newNum = newEnts[newIndex].number;
		if ( oldIndex < deltaframe->entitynum )
			oldNum = oldEnts[oldIndex].number;

		if ( newNum == oldNum )
		{
			header.num = newNum;
			header.remove = false;
			header.custom = ( newEnts[newIndex].entityType & 2 ) != 0;
			header.newbl = false;
			header.newblindex = 0;
			DELTA_WriteDelta( stream, (unsigned char*)&oldEnts[oldIndex], (unsigned char*)&newEnts[newIndex],
					false, DeltaFields( header.custom, newNum, this ), &header );
			++newIndex;
			++oldIndex;
		}
		else if ( oldNum < newNum )
		{
			header.num = oldNum;
			header.remove = true;
			header.newbl = false;
			header.newblindex = 0;
			DELTA_WriteHeader( stream, &header );
			++oldIndex;
		}
		else
		{
			header.num = newNum;
			header.remove = false;
			header.custom = ( newEnts[newIndex].entityType & 2 ) != 0;
			header.newbl = false;
			header.newblindex = 0;
			DELTA_WriteDelta( stream, (unsigned char*)&baselines[newNum], (unsigned char*)&newEnts[newIndex],
					true, DeltaFields( header.custom, newNum, this ), &header );
			++newIndex;
		}
	}
	stream->WriteBits( 0, 16 );
	stream->EndBitMode();
}

void World::ClearServerInfo()
{
	serverinfo.Clear();
	memset( &detailedServerInfo, 0, sizeof( detailedServerInfo ) );
	detailedServerInfo.type = '?';
	detailedServerInfo.os = '?';
	detailedServerInfo.pw = '?';
}

void World::SetName( char* newName )
{
	strncpy( m_Name, newName, 0xFEu );
	m_Name[254] = 0;
}

void World::ReorderFrameTimes( float newLastTime )
{
	frame_t* last;
	frame_t* older;
	int seq;

	last = (frame_t*)frames.GetLast();
	if ( last == NULL )
		return;
	seq = (int)last->seqnr - 1;
	older = (frame_t*)frames.FindExactKey( (float)seq );
	if ( older == NULL )
	{
		last->time = newLastTime;
		return;
	}
	while ( older != NULL )
	{
		frame_t* nextOlder;
		float span;

		--seq;
		span = last->time - older->time;
		last->time = newLastTime;
		newLastTime = newLastTime - span;
		last = older;
		nextOlder = (frame_t*)frames.FindExactKey( (float)seq );
		if ( nextOlder == NULL )
			break;
		older = nextOlder;
	}
	older->time = newLastTime;
}

bool World::GetClientData( frame_t* frame, clientdata_t* clientData )
{
	clientdata_t baseline;

	if ( frame == NULL || frame->clientData == NULL || frame->clientDataSize == 0 )
		return false;
	memset( &baseline, 0, sizeof( baseline ) );
	{
		BitBuffer stream( frame->clientData, frame->clientDataSize );
		stream.StartBitMode();
		DELTA_ParseDelta( &stream, (unsigned char*)&baseline, (unsigned char*)clientData, g_pclientdelta );
	}
	return true;
}

bool World::GetUncompressedFrame( frame_t* deltaFrame, frame_t* frame )
{
	BitBuffer stream;
	entity_state_t* cached;

	if ( deltaFrame == NULL || deltaFrame->delta != WORLD_FULLFRAME_DELTA )
	{
		if ( deltaFrame != NULL )
			m_System->DPrintf( "World::GetUncompressedFrame: unexpected non baseline delta frame.\n" );
		return false;
	}

	*frame = *deltaFrame;
	frame->delta = 0;
	frame->entitiesSize = sizeof( entity_state_t ) * deltaFrame->entitynum;
	if ( frame->entitynum > (unsigned int)MAX_PACKET_ENTITIES )
		m_System->Errorf( "World::GetUncompressedFrame: entnum>=MAX_PACKET_ENTITIES.\n" );
	if ( frame->entitynum == 0 )
		return true;
	if ( !GetFrameFromCache( deltaFrame->seqnr, &cached ) )
	{
		frame->entities = cached;
		DELTA_SetLargeTimeBufferSize( true );
		stream.SetBuffer( deltaFrame->entities, deltaFrame->entitiesSize );
		UncompressEntitiesFromStream( frame, &stream );
		DELTA_SetLargeTimeBufferSize( false );
		return true;
	}
	frame->entities = cached;
	return true;
}

bool World::WriteDeltaEntities( BitBuffer* stream, frame_t* fullFrame, unsigned int deltaSeqNr,
		unsigned int clientDelta )
{
	BitBuffer* buffer;
	frame_t deltaFrame;

	if ( !GetDeltaFromCache( fullFrame->seqnr, deltaSeqNr, &buffer ) )
	{
		if ( !GetUncompressedFrame( deltaSeqNr, &deltaFrame ) )
		{
			m_System->DPrintf( "WARNING! World::WriteFrame: delta frame too old (%i).\n", deltaSeqNr );
			return false;
		}
		buffer->Resize( fullFrame->entitiesSize );
		WritePacketEntities( buffer, fullFrame, &deltaFrame );
	}
	stream->WriteByte( svc_deltapacketentities );
	stream->WriteShort( (int)fullFrame->entitynum );
	stream->WriteByte( (int)clientDelta );
	stream->ConcatBuffer( buffer );
	return true;
}

bool World::UncompressEntitiesFromStream( frame_t* frame, BitBuffer* stream, unsigned int from )
{
	frame_t deltaFrame;
	entity_state_t* oldEnts;
	entity_state_t* newEnts;
	int newindex;
	unsigned int oldindex;
	int numbase;
	int offset;

	if ( !GetUncompressedFrame( from, &deltaFrame ) )
	{
		m_System->DPrintf( "WARNING! World::UncompressEntitiesFromStream: delta frame too old.\n" );
		stream->sizeError = true;
		return false;
	}

	DELTA_SetTime( frame->time );
	stream->StartBitMode();
	newindex = 0;
	oldindex = 0;
	numbase = 0;
	offset = 0;
	oldEnts = (entity_state_t*)deltaFrame.entities;
	newEnts = (entity_state_t*)frame->entities;
	while ( stream->PeekBits( 16 ) != 0 )
	{
		bool remove;
		bool custom;
		bool newbl;
		int newblindex;
		int newnum;
		int oldnum;

		remove = false;
		custom = false;
		newbl = false;
		newblindex = 0;
		newnum = ParseDeltaHeader( stream, &remove, &custom, &numbase, &newbl, &newblindex, false, &offset );
		oldnum = WORLD_ENTITY_SENTINEL;
		if ( oldindex < deltaFrame.entitynum )
			oldnum = oldEnts[oldindex].number;

		while ( newnum > oldnum )
		{
			if ( newindex > MAX_PACKET_ENTITIES - 1 )
			{
				m_System->DPrintf( "WARNING!World::UncompressEntitiesFromStream: newindex >= MAX_PACKET_ENTITIES.\n" );
				stream->sizeError = true;
			}
			memcpy( &newEnts[newindex], &oldEnts[oldindex], sizeof( entity_state_t ) );
			++oldindex;
			++newindex;
			oldnum = WORLD_ENTITY_SENTINEL;
			if ( oldindex < deltaFrame.entitynum )
				oldnum = oldEnts[oldindex].number;
		}

		if ( newnum == oldnum )
		{
			if ( !remove )
			{
				unsigned char* dest;

				dest = (unsigned char*)&newEnts[newindex];
				if ( custom )
				{
					DELTA_ParseDelta( stream, (unsigned char*)&oldEnts[oldindex], dest, g_pcustomentitydelta );
					newEnts[newindex].entityType = 2;
				}
				else
				{
					DELTA_ParseDelta( stream, (unsigned char*)&oldEnts[oldindex], dest,
							DeltaFields( false, newnum, this ) );
					newEnts[newindex].entityType = 1;
				}
				newEnts[newindex].number = newnum;
				++newindex;
			}
			++oldindex;
		}
		else if ( !remove )
		{
			unsigned char* fromState;
			unsigned char* dest;

			if ( newindex > MAX_PACKET_ENTITIES - 1 )
				m_System->DPrintf( "World::UncompressEntitiesFromStream: newindex >= MAX_PACKET_ENTITIES.\n" );
			if ( newindex > MAX_PACKET_ENTITIES - 1 )
				stream->sizeError = true;
			if ( newbl )
				fromState = (unsigned char*)&instanced_baselines[newblindex];
			else if ( offset != 0 )
				fromState = (unsigned char*)&newEnts[newindex - offset];
			else
				fromState = (unsigned char*)&baselines[newnum];
			dest = (unsigned char*)&newEnts[newindex];
			if ( custom )
			{
				DELTA_ParseDelta( stream, fromState, dest, g_pcustomentitydelta );
				newEnts[newindex].entityType = 2;
			}
			else
			{
				DELTA_ParseDelta( stream, fromState, dest, DeltaFields( false, newnum, this ) );
				newEnts[newindex].entityType = 1;
			}
			newEnts[newindex].number = newnum;
			++newindex;
		}
	}

	if ( stream->ReadShort() != 0 )
	{
		m_System->DPrintf( "WARNING! World::UncompressEntitiesFromStream: missing end tag.\n" );
		return false;
	}
	stream->EndBitMode();
	while ( oldindex < deltaFrame.entitynum )
	{
		if ( newindex > MAX_PACKET_ENTITIES - 1 )
		{
			m_System->DPrintf( "WARNING! World::UncompressEntitiesFromStream: newindex >= MAX_PACKET_ENTITIES.\n" );
			stream->sizeError = true;
		}
		memcpy( &newEnts[newindex], &oldEnts[oldindex], sizeof( entity_state_t ) );
		++newindex;
		++oldindex;
	}
	if ( frame->entitynum != (unsigned int)newindex )
		m_System->DPrintf( "WARNING! World::UncompressEntitiesFromStream: newindex != frame.entitynum.\n" );
	return true;
}

void World::ParseClientData( BitBuffer* stream, unsigned int deltaSeqNr, BitBuffer* to, clientdata_t* clientData )
{
	clientdata_t nullClientData;
	clientdata_t fromClientData;
	weapon_data_t nullWeaponData;

	memset( &nullClientData, 0, sizeof( nullClientData ) );
	memset( &fromClientData, 0, sizeof( fromClientData ) );
	memset( clientData, 0, 4 );
	if ( deltaSeqNr != 0 && !GetClientData( deltaSeqNr, &fromClientData ) )
		m_System->Printf( "WARNING! World::ParseClientData: couldn't uncompress delta frame %i\n", deltaSeqNr );

	DELTA_ParseDelta( stream, (unsigned char*)&fromClientData, (unsigned char*)clientData, g_pclientdelta );
	to->StartBitMode();
	DELTA_WriteDelta( to, (unsigned char*)&nullClientData, (unsigned char*)clientData, true, g_pclientdelta, NULL );
	to->EndBitMode();
	while ( stream->ReadBit() != 0 )
	{
		stream->ReadBits( 6 );
		DELTA_ParseDelta( stream, (unsigned char*)&nullWeaponData, (unsigned char*)&nullWeaponData, g_pweapondelta );
	}
}

void World::RearrangeFrame( frame_t* frame, int seqNrOffset, float timeOffset )
{
	frame_t fullFrame;
	unsigned int i;

	if ( !GetUncompressedFrame( frame, &fullFrame ) )
		return;

	framesByTime.ChangeKey( frame, frame->time - timeOffset );
	frames.ChangeKey( frame, (float)( frame->seqnr - (unsigned int)seqNrOffset ) );
	frame->seqnr -= (unsigned int)seqNrOffset;
	frame->time = frame->time - timeOffset;
	DELTA_SetLargeTimeBufferSize( true );
	if ( frame->entitynum != 0 )
	{
		BitBuffer tempStream( frame->entities, frame->entitiesSize );
		int compressed;

		if ( fullFrame.entitynum != 0 )
		{
			entity_state_t* ents;

			ents = (entity_state_t*)fullFrame.entities;
			for ( i = 0; i < fullFrame.entitynum; ++i )
			{
				int number;

				number = ents[i].number;
				if ( ents[i].animtime != baselines[number].animtime )
					ents[i].animtime -= timeOffset;
				if ( ents[i].impacttime != baselines[number].impacttime )
					ents[i].impacttime -= timeOffset;
				if ( ents[i].starttime != baselines[number].starttime )
					ents[i].starttime -= timeOffset;
			}
		}
		memset( frame->entities, 0, frame->entitiesSize );
		compressed = CompressFrame( &fullFrame, &tempStream );
		if ( frame->entitiesSize < (unsigned int)compressed || tempStream.sizeError )
		{
			m_System->Printf( "WARNING! World::RearrangeFrame: wrong entities size (%i != %i).\n",
					frame->entitiesSize, compressed );
			return;
		}
		frame->entitiesSize = (unsigned int)compressed;
	}
	DELTA_SetLargeTimeBufferSize( false );
}

bool World::GetUncompressedFrame( unsigned int seqNr, frame_t* frame )
{
	frame_t* deltaFrame;

	deltaFrame = (frame_t*)frames.FindExactKey( (float)seqNr );
	if ( deltaFrame == NULL )
		return false;
	if ( seqNr != deltaFrame->seqnr )
	{
		m_System->DPrintf( "WARNING! World::GetUncompressedFrame: frame not found %i.\n", seqNr );
		return false;
	}
	return GetUncompressedFrame( deltaFrame, frame );
}

void World::UpdatePlayer( int playerNum, int userId, char* infostring, char* hashedcdkey )
{
	InfoString infostr( infostring );
	char* value;

	if ( playerNum > MAX_WORLD_CLIENT_INDEX )
	{
		m_System->Errorf( "World::UpdatePlayer: player number %i > MAX_CLIENTS\n", playerNum );
		return;
	}
	if ( infostring[0] == 0 )
		return;

	players[playerNum].active = true;
	m_System->DPrintf( "Player update(%i:%s)\n", playerNum, players[playerNum].name );
	infostr.RemovePrefixedKeys( '_' );
	players[playerNum].userid = userId;
	value = infostr.GetString();
	strncpy( players[playerNum].userinfo, value, 0xFFu );
	players[playerNum].userinfo[255] = 0;
	memcpy( players[playerNum].hashedcdkey, hashedcdkey, 16 );
	value = infostr.ValueForKey( "name" );
	strncpy( players[playerNum].name, value, 0x1Fu );
	players[playerNum].name[31] = 0;
	value = infostr.ValueForKey( "model" );
	strncpy( players[playerNum].model, value, 0x103u );
	players[playerNum].model[259] = 0;
	players[playerNum].topcolor = atoi( infostr.ValueForKey( "topcolor" ) );
	players[playerNum].bottomcolor = atoi( infostr.ValueForKey( "bottomcolor" ) );
	players[playerNum].spectator = atoi( infostr.ValueForKey( "*hltv" ) );
	players[playerNum].trackerID = atoi( infostr.ValueForKey( "*fid" ) );
	FireSignal( 4u, &playerNum );
}

bool World::Init( IBaseSystem* system, int serial, char* name )
{
	const char* cacheParam;
	int cacheEntries;
	unsigned int frameBytes;

	BaseSystemModule::Init( system, serial, name );
	if ( state != WORLD_INITILAZING )
		state = WORLD_INITILAZING;
	if ( name != NULL )
		SetName( name );
	else
		strcpy( m_Name, "world001" );

	DELTA_Init();
	ClientUserMsgs = NULL;
	resourcesList = NULL;
	num_resources = 0;
	m_Protocol = PROTOCOL_VERSION;
	signonData.Resize( WORLD_SIGNON_BYTES );
	serverinfo.SetMaxSize( WORLD_SERVERINFO_BYTES );
	memset( hostName, 0, sizeof( hostName ) );
	strcpy( servername, "Unnamed HLTV" );
	frames.Init();
	framesByTime.Init();
	worldModel.Init( system );
	maxBufferLength = -1.0f;
	cacheParam = m_System->CheckParam( "-cachesize" );
	if ( cacheParam != NULL && ( cacheEntries = atoi( cacheParam ),
			maxCacheIndex = cacheEntries, cacheEntries > 0 ) )
		frameBytes = (unsigned int)( sizeof( frameCache_t ) * cacheEntries );
	else
	{
		maxCacheIndex = WORLD_CACHE_ENTRIES;
		frameBytes = (unsigned int)( sizeof( frameCache_t ) * WORLD_CACHE_ENTRIES );
	}
	frameCache = (frameCache_t*)Mem_ZeroMalloc( frameBytes );
	deltaCache = (deltaCache_t*)Mem_ZeroMalloc( sizeof( deltaCache_t ) * (unsigned int)maxCacheIndex );
	if ( frameCache == NULL || deltaCache == NULL )
	{
		m_System->Errorf( "World::Init: Not enough memory for caches. Reduce -cachesize.\n" );
		return false;
	}
	Reset();
	if ( state != WORLD_DISCONNECTED )
		state = WORLD_DISCONNECTED;
	m_State = 3;
	m_System->Printf( "World module initialized.\n" );
	return true;
}

void World::ShutDown()
{
	if ( m_State == 4 )
		return;
	Reset();
	if ( state != WORLD_DISCONNECTED )
		state = WORLD_DISCONNECTED;
	FireSignal( 8u, NULL );
	DELTA_Shutdown();
	m_Listener.Clear( false );
	if ( deltaCache != NULL )
	{
		free( deltaCache );
		deltaCache = NULL;
	}
	if ( frameCache != NULL )
	{
		free( frameCache );
		frameCache = NULL;
	}
	BaseSystemModule::ShutDown();
	m_System->Printf( "World module shutdown.\n" );
}

frame_t* World::GetFirstFrame()
{
	return (frame_t*)frames.GetFirst();
}

frame_t* World::GetFrameByTime( double time )
{
	frame_t* frame;
	frame_t* next;

	frame = (frame_t*)framesByTime.FindClosestKey( (float)time );
	if ( frame == NULL )
		return NULL;
	if ( time > frame->time )
	{
		next = (frame_t*)framesByTime.GetNext();
		if ( next == NULL )
			return frame;
		return next;
	}
	return frame;
}

void World::WriteFrame( frame_t* frame, unsigned int lastFrameSeqnr, BitBuffer* reliableStream,
		BitBuffer* unreliableStream, unsigned int deltaSeqNr, unsigned int clientDelta, bool addVoice )
{
	frame_t fullFrame;
	clientdata_t clientData;
	clientdata_t nullClientData;
	frame_t* packed;

	(void)addVoice;
	if ( !GetUncompressedFrame( frame->seqnr, &fullFrame ) )
	{
		m_System->DPrintf( "World::WriteFrame: couldn't uncompress frame.\n" );
		return;
	}

	if ( isHLTV )
		unreliableStream->WriteByte( svc_clientdata );
	else if ( GetClientData( frame, &clientData ) )
	{
		memset( &nullClientData, 0, sizeof( nullClientData ) );
		unreliableStream->WriteByte( svc_clientdata );
		unreliableStream->StartBitMode();
		unreliableStream->WriteBit( 0 );
		DELTA_WriteDelta( unreliableStream, (unsigned char*)&nullClientData, (unsigned char*)&clientData,
				true, g_pclientdelta, NULL );
		unreliableStream->WriteBit( 0 );
		unreliableStream->EndBitMode();
	}

	if ( deltaSeqNr != 0 && WriteDeltaEntities( unreliableStream, &fullFrame, deltaSeqNr, clientDelta ) )
	{
		if ( frame->eventsSize != 0 )
		{
			unreliableStream->WriteByte( svc_event );
			unreliableStream->WriteBuf( frame->events, frame->eventsSize );
		}
	}
	else
	{
		unreliableStream->WriteByte( svc_packetentities );
		unreliableStream->WriteShort( (int)frame->entitynum );
		if ( CompressFrame( &fullFrame, unreliableStream ) <= 0 )
			m_System->DPrintf( "WARNING! World::WriteFrame: could write entities!\n" );
		else if ( frame->eventsSize != 0 )
		{
			unreliableStream->WriteByte( svc_event );
			unreliableStream->WriteBuf( frame->events, frame->eventsSize );
		}
	}

	packed = (frame_t*)frames.FindExactKey( (float)( lastFrameSeqnr + 1 ) );
	while ( packed != NULL && packed->seqnr <= frame->seqnr )
	{
		if ( reliableStream->sizeError )
			return;
		if ( packed->reliableDataSize != 0 && (unsigned int)reliableStream->SpaceLeft() > packed->reliableDataSize )
			reliableStream->WriteBuf( packed->reliableData, packed->reliableDataSize );
		if ( packed->userMessagesSize != 0 && (unsigned int)reliableStream->SpaceLeft() > packed->userMessagesSize )
			reliableStream->WriteBuf( packed->userMessages, packed->userMessagesSize );
		if ( packed->seqnr + 8 > frame->seqnr )
		{
			if ( packed->unreliableDataSize != 0 &&
					(unsigned int)unreliableStream->SpaceLeft() > packed->unreliableDataSize )
				unreliableStream->WriteBuf( packed->unreliableData, packed->unreliableDataSize );
			if ( packed->voiceDataSize != 0 && (unsigned int)unreliableStream->SpaceLeft() > packed->voiceDataSize )
				unreliableStream->WriteBuf( packed->voiceData, packed->voiceDataSize );
		}
		packed = (frame_t*)frames.GetNext();
	}
}

float World::GetBufferedGameTime()
{
	frame_t* first;
	frame_t* last;

	if ( !IsActive() || frames.IsEmpty() )
		return 0.0f;
	first = (frame_t*)frames.GetFirst();
	last = (frame_t*)frames.GetLast();
	return last->time - first->time;
}

char* World::GetStatusLine()
{
	static char string[256];
	double buffered;
	float ratio;
	char* clock;

	if ( !IsActive() )
	{
		strcpy( string, "World not active.\n" );
		return string;
	}
	buffered = GetBufferedGameTime();
	ratio = (float)cacheHits / (float)( cacheHits + cacheFaults );
	clock = COM_FormatTime( (float)worldTime );
	snprintf( string, sizeof( string ),
			"Game \"%s\", Map \"%s\", Time %s, Players %i\nFrame cache use %.1f, Buffered time %.0f .\n",
			gamedir, levelname, clock, GetNumPlayers(), ratio, buffered );
	return string;
}

int World::RemoveFrames( unsigned int startSeqNr, unsigned int endSeqNr )
{
	frame_t* frame;
	frame_t* endFrame;
	unsigned int endSeq;
	unsigned int nextSeq;
	float firstFrameTime;
	float endTime;
	int seqNrOffset;

	if ( startSeqNr > endSeqNr )
		return 0;
	frame = (frame_t*)frames.FindExactKey( (float)startSeqNr );
	endFrame = (frame_t*)frames.FindExactKey( (float)endSeqNr );
	if ( frame == NULL || endFrame == NULL )
		return 0;

	endSeq = endFrame->seqnr;
	endTime = endFrame->time;
	firstFrameTime = frame->time;
	seqNrOffset = (int)( endSeq + 1 - frame->seqnr );
	nextSeq = frame->seqnr;
	while ( frame != NULL && endSeq >= frame->seqnr )
	{
		frame_t* dying;

		++nextSeq;
		dying = frame;
		frames.Remove( dying );
		framesByTime.Remove( dying );
		free( dying->data );
		free( dying );
		frame = (frame_t*)frames.FindExactKey( (float)nextSeq );
	}

	if ( frames.GetFirst() != frame )
	{
		ClearEntityCache();
		if ( frame != NULL )
		{
			float timeOffset;

			timeOffset = endTime - firstFrameTime;
			while ( frame != NULL )
			{
				frame_t* current;

				++nextSeq;
				current = frame;
				RearrangeFrame( current, seqNrOffset, timeOffset );
				frame = (frame_t*)frames.FindExactKey( (float)nextSeq );
			}
		}
		ClearEntityCache();
	}
	return seqNrOffset;
}

int World::AddFrame( frame_t* newFrame )
{
	frame_t* currentFrame;
	unsigned int packed;
	unsigned int bytes;
	unsigned char* data;
	unsigned char* cursor;

	if ( newFrame == NULL )
	{
		m_System->Errorf( "World::AddFrame: newFrame == NULL.\n" );
		return 0;
	}

	++sequenceNr;
	currentFrame = (frame_t*)Mem_ZeroMalloc( sizeof( frame_t ) );
	currentFrame->seqnr = sequenceNr;
	currentFrame->time = newFrame->time;
	if ( worldTime <= newFrame->time )
	{
		worldTime = newFrame->time;
		if ( state == WORLD_CONNECTING )
			ConnectionComplete();
	}
	else
	{
		m_System->DPrintf( "Fixing frame time, delta %.3f\n", worldTime - newFrame->time );
		ReorderFrameTimes( currentFrame->time - 0.05f );
		worldTime = currentFrame->time;
		if ( state == WORLD_CONNECTING )
			ConnectionComplete();
	}

	packed = newFrame->entitiesSize;
	if ( packed != 0 )
	{
		int compressed;

		if ( newFrame->delta != 0 )
		{
			m_System->Errorf( "World::AddFrame: only uncompressed frames accepted.\n" );
			return 0;
		}
		BitBuffer tempStream( entityBuffer, WORLD_ENTITYBUFFER_BYTES );
		DELTA_SetLargeTimeBufferSize( true );
		memset( entityBuffer, 0, WORLD_ENTITYBUFFER_BYTES );
		compressed = CompressFrame( newFrame, &tempStream );
		DELTA_SetLargeTimeBufferSize( false );
		if ( tempStream.sizeError )
		{
			m_System->Printf( "WARNING! World::AddFrame: couldn't compress frame entities.\n" );
			return 0;
		}
		packed = (unsigned int)compressed;
	}

	bytes = newFrame->clientDataSize + newFrame->eventsSize + newFrame->reliableDataSize +
		newFrame->unreliableDataSize + newFrame->voiceDataSize + packed + newFrame->demoDataSize +
		newFrame->userMessagesSize + ( newFrame->demoInfo == NULL ? 4u : 440u );
	data = (unsigned char*)malloc( bytes );
	cursor = data;
	currentFrame->delta = WORLD_FULLFRAME_DELTA;
	currentFrame->data = data;

	if ( newFrame->entitiesSize != 0 )
	{
		memcpy( cursor, entityBuffer, packed );
		currentFrame->entities = cursor;
		currentFrame->entitiesSize = packed;
		currentFrame->entitynum = newFrame->entitynum;
		cursor += packed;
	}
	if ( newFrame->clientDataSize != 0 )
	{
		memcpy( cursor, newFrame->clientData, newFrame->clientDataSize );
		currentFrame->clientData = cursor;
		currentFrame->clientDataSize = newFrame->clientDataSize;
		cursor += newFrame->clientDataSize;
	}
	if ( newFrame->eventsSize != 0 )
	{
		memcpy( cursor, newFrame->events, newFrame->eventsSize );
		currentFrame->events = cursor;
		currentFrame->eventsSize = newFrame->eventsSize;
		currentFrame->eventnum = newFrame->eventnum;
		cursor += newFrame->eventsSize;
	}
	if ( newFrame->reliableDataSize != 0 )
	{
		memcpy( cursor, newFrame->reliableData, newFrame->reliableDataSize );
		currentFrame->reliableData = cursor;
		currentFrame->reliableDataSize = newFrame->reliableDataSize;
		cursor += newFrame->reliableDataSize;
	}
	if ( newFrame->unreliableDataSize != 0 )
	{
		memcpy( cursor, newFrame->unreliableData, newFrame->unreliableDataSize );
		currentFrame->unreliableData = cursor;
		currentFrame->unreliableDataSize = newFrame->unreliableDataSize;
		cursor += newFrame->unreliableDataSize;
	}
	if ( newFrame->voiceDataSize != 0 )
	{
		memcpy( cursor, newFrame->voiceData, newFrame->voiceDataSize );
		currentFrame->voiceData = cursor;
		currentFrame->voiceDataSize = newFrame->voiceDataSize;
		cursor += newFrame->voiceDataSize;
	}
	if ( newFrame->userMessagesSize != 0 )
	{
		memcpy( cursor, newFrame->userMessages, newFrame->userMessagesSize );
		currentFrame->userMessages = cursor;
		currentFrame->userMessagesSize = newFrame->userMessagesSize;
		cursor += newFrame->userMessagesSize;
	}
	if ( newFrame->demoDataSize != 0 )
	{
		memcpy( cursor, newFrame->demoData, newFrame->demoDataSize );
		currentFrame->demoData = cursor;
		currentFrame->demoDataSize = newFrame->demoDataSize;
		cursor += newFrame->demoDataSize;
	}
	if ( newFrame->demoInfo != NULL )
	{
		memcpy( cursor, newFrame->demoInfo, WORLD_DEMOINFO_BYTES );
		currentFrame->demoInfo = cursor;
	}

	frames.Add( currentFrame, (float)currentFrame->seqnr );
	framesByTime.Add( currentFrame, (float)worldTime );
	CheckFrameBufferSize();
	FireSignal( 3u, &sequenceNr );
	return (int)sequenceNr;
}

void World::CheckFrameBufferSize()
{
	frame_t* last;
	frame_t* first;
	frame_t* closest;
	unsigned int endSeq;

	if ( maxBufferLength <= 0.0f )
		return;
	last = (frame_t*)frames.GetLast();
	if ( last == NULL )
		return;
	first = (frame_t*)frames.GetFirst();
	if ( first == NULL )
		return;
	closest = (frame_t*)framesByTime.FindClosestKey( last->time - maxBufferLength );
	if ( closest == NULL )
		return;
	endSeq = closest->seqnr - 1;
	RemoveFrames( first->seqnr, endSeq );
}

void World::ClearFrames()
{
	frame_t* frame;

	frame = (frame_t*)frames.GetFirst();
	while ( frame != NULL )
	{
		frame_t* next;

		next = (frame_t*)frames.GetNext();
		if ( frame->data != NULL )
			free( frame->data );
		free( frame );
		frame = next;
	}
	frames.Clear( false );
	framesByTime.Clear( false );
}

void World::Reset()
{
	ClearUserMessages();
	ClearResources();
	ClearBaseline();
	ClearInstancedBaseline();
	ClearLightStyles();
	ClearPlayers();
	ClearEntityCache();
	ClearServerInfo();
	ClearFrames();
	signonData.Clear();
	worldModel.Clear();
	sequenceNr = 0;
	viewentity = 0;
	worldTime = 0.0;
	startTime = 0.0;
	voiceEnabled = false;
	memset( clientfallback, 0, sizeof( clientfallback ) );
	allowCheats = false;
	isHLTV = false;
	m_IsPaused = false;
	gameServerAddress.Clear();
	g_DownloadURL[0] = 0;
}

IBSPModel* World::GetWorldModel()
{
	if ( !worldModel.IsValid() )
		worldModel.Load( levelname, true );
	return &worldModel;
}

bool World::SaveAsDemo( char* filename, IDirector* director )
{
	NetChannel demoChannel;
	DemoFile demoFile;
	client_data_t cdata;
	frame_t* frame;
	unsigned int lastSeq;
	unsigned int clientDelta;
	double lastFrameTime;
	bool started;

	frame = (frame_t*)frames.GetFirst();
	if ( frame == NULL )
		return false;

	memset( &cdata, 0, sizeof( cdata ) );
	worldTime = frame->time;
	demoChannel.Create( m_System, NULL, NULL );
	demoFile.Init( this, NULL, &demoChannel );
	started = demoFile.StartRecording( filename );
	if ( !started )
		return false;

	m_System->Printf( "Saving game to \"%s\". This can take some time...\n", filename );
	clientDelta = 0;
	lastSeq = 0;
	lastFrameTime = 0.0;
	for ( ;; )
	{
		unsigned int seq;

		worldTime = frame->time;
		seq = frame->seqnr;
		if ( lastSeq == 0 || seq < lastSeq )
		{
			lastSeq = seq - 1;
			if ( seq <= lastSeq )
				continue;
		}
		else if ( seq <= lastSeq )
		{
			continue;
		}

		demoChannel.unreliableStream.WriteByte( svc_time );
		demoChannel.unreliableStream.WriteFloat( frame->time );
		WriteFrame( frame, lastSeq, &demoChannel.reliableStream, &demoChannel.unreliableStream,
				lastSeq, clientDelta, true );
		if ( director != NULL )
			director->WriteCommands( &demoChannel.reliableStream, (float)lastFrameTime, frame->time );
		clientDelta = (unsigned char)demoChannel.outgoing_sequence;
		lastSeq = frame->seqnr;
		if ( demoChannel.reliableStream.sizeError )
		{
			m_System->Printf( "WARNING! World::SaveAsDemo: reliable data overflow.\n" );
			started = false;
			break;
		}
		if ( demoChannel.unreliableStream.sizeError )
			demoChannel.unreliableStream.Clear();
		demoFile.WriteDemoMessage( &demoChannel.unreliableStream, &demoChannel.reliableStream );
		demoChannel.TransmitOutgoing();
		demoFile.WriteUpdateClientData( &cdata );
		lastFrameTime = frame->time;
		frame = (frame_t*)frames.FindExactKey( (float)( lastSeq + 1 ) );
		if ( frame == NULL )
		{
			demoFile.CloseFile();
			demoChannel.Clear();
			break;
		}
	}
	return started;
}
