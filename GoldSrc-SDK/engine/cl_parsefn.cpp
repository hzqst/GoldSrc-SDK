#include "cd.h"
#include "client.h"
#include "cmd.h"
#include "common.h"
#include "conprint.h"
#include "cvar.h"
#include "decals.h"
#include "delta.h"
#include "host.h"
#include "mem.h"
#include "protocol.h"
#include "server.h"
#include "sound.h"
#include "sys.h"
#include "vgui_intwrap2.h"
#include "vid.h"
#include "voice.h"
#include "../common/con_nprint.h"
#include "../common/event_flags.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#define EVENT_COUNT_BITS	5
#define EVENT_INDEX_BITS	10
#define EVENT_ENTITY_BITS	11
#define EVENT_DELAY_BITS	16

#define PING_SLOT_BITS		5
#define PING_VALUE_BITS		12
#define PING_LOSS_BITS		7

#define STEAMLOGINFAILURE_NONE					0
#define STEAMLOGINFAILURE_BADTICKET				1
#define STEAMLOGINFAILURE_NOSTEAMLOGIN			2
#define STEAMLOGINFAILURE_VACBANNED				3
#define STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE	4
#define STEAMLOGINFAILURE_CONNECTIONLOST		5
#define STEAMLOGINFAILURE_NOCONNECTION			6

extern cvar_t cl_clockreset;
extern cvar_t cl_fixtimerate;
extern cvar_t cl_showevents;

typedef struct event_hook_s
{
	struct event_hook_s* next;
	char* name;
	void ( *pfnEvent )( event_args_t* args );
} event_hook_t;

static double g_clockdelta;
static event_hook_t* g_pEventHooks;

void SCR_CenterPrint( char* text );
void SCR_EndLoadingPlaque();
void AddNewUserMsg();
void CL_UpdateUserinfo();
void CL_ParseMovevars();
void CL_ParseStaticSound();
void CL_ParseSoundFade();
void CL_ParseHLTV();
void CL_ParseClientdata();
void CL_ParseStatic();
void CL_ParseBaseline();
void CL_ParseTEnt();
void CL_SendResourceListBlock();
void CL_ParseCustomization();
void R_ParseParticleEffect();

void CL_Parse_NewUserMsg()
{
	AddNewUserMsg();
}

void CL_Parse_UpdateUserInfo()
{
	CL_UpdateUserinfo();
}

void CL_Parse_NewMoveVars()
{
	CL_ParseMovevars();
}

void CL_Parse_SpawnStaticSound()
{
	CL_ParseStaticSound();
}

void CL_Parse_SoundFade()
{
	CL_ParseSoundFade();
}

void CL_Parse_HLTV()
{
	CL_ParseHLTV();
}

void CL_Parse_Damage()
{
}

void CL_Parse_KilledMonster()
{
}

void CL_Parse_FoundSecret()
{
}

void CL_Parse_Time()
{
	double dt;

	cl.mtime[1] = cl.mtime[0];
	cl.mtime[0] = MSG_ReadFloat();

	if ( cl.maxclients == 1 )
		cl.time = cl.mtime[0];

	dt = cl.time - cl.mtime[0];
	if ( fabs( dt ) > cl_clockreset.value )
	{
		cl.time = cl.mtime[0];
		g_clockdelta = 0.0;
	}
	else if ( dt != 0.0 )
	{
		g_clockdelta = dt;
	}

	if ( cl.oldtime > cl.time )
		cl.oldtime = cl.time;
}

void CL_Parse_Disconnect()
{
	const char* reason;

	reason = MSG_ReadString();
	SCR_EndLoadingPlaque();

	if ( reason && reason[0] )
	{
		COM_ExplainDisconnection( true, "#GameUI_DisconnectedFromServerExtended" );
		COM_ExtendedExplainDisconnection( true, reason );

		if ( !_strnicmp( reason, "Invalid VALVE CD Key\n", sizeof( "Invalid VALVE CD Key\n" ) - 1 ) )
			ValidateCDKey( 1, 1 );
		else if ( !_strnicmp( reason, "Invalid STEAM UserID Ticket\n", sizeof( "Invalid STEAM UserID Ticket\n" ) - 1 ) )
			VGUI2_OnDisconnectFromServer( STEAMLOGINFAILURE_BADTICKET );
		else if ( !Q_strcasecmp( reason, "No Steam logon\n" ) )
			VGUI2_OnDisconnectFromServer( STEAMLOGINFAILURE_NOSTEAMLOGIN );
		else if ( !Q_strcasecmp( reason, "Steam connection lost\n" ) )
			VGUI2_OnDisconnectFromServer( STEAMLOGINFAILURE_CONNECTIONLOST );
		else if ( !Q_strcasecmp( reason, "Unable to connect to Steam\n" ) )
			VGUI2_OnDisconnectFromServer( STEAMLOGINFAILURE_NOCONNECTION );
		else if ( !Q_strcasecmp( reason, "This Steam account is being used in another location\n" ) )
			VGUI2_OnDisconnectFromServer( STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE );
		else if ( !Q_strcasecmp( reason, "VAC banned from secure server\n" ) )
			VGUI2_OnDisconnectFromServer( STEAMLOGINFAILURE_VACBANNED );
		else
			VGUI2_OnDisconnectFromServer( STEAMLOGINFAILURE_NONE );
	}
	else
	{
		COM_ExplainDisconnection( true, "#GameUI_DisconnectedFromServer" );
	}

	Host_EndGame( "Server disconnected\n" );
}

void CL_Parse_ClientData()
{
	CL_ParseClientdata();
}

void CL_Parse_Particle()
{
	R_ParseParticleEffect();
}

void CL_Parse_SpawnStatic()
{
	CL_ParseStatic();
}

void CL_Parse_SpawnBaseline()
{
	CL_ParseBaseline();
}

void CL_Parse_TempEntity()
{
	CL_ParseTEnt();
}

void CL_Parse_ResourceList()
{
	CL_ParseResourceList();
}

void CL_Parse_ResourceRequest()
{
	CL_SendResourceListBlock();
}

void CL_Parse_Customization()
{
	CL_ParseCustomization();
}

void CL_Parse_ResourceLocation()
{
	CL_ParseResourceLocation();
}

void CL_Parse_Director()
{
	int size;

	size = MSG_ReadByte();
	ClientDLL_DirectorMessage( size, net_message.data + msg_readcount );
	msg_readcount += size;
}

void CL_Send_CvarValue()
{
	const char* name;
	cvar_t* var;

	name = MSG_ReadString();
	MSG_WriteByte( &cls.netchan.message, clc_cvarvalue );

	if ( strlen( name ) >= 255 )
	{
		MSG_WriteString( &cls.netchan.message, "Bad CVAR request" );
		return;
	}

	var = Cvar_FindVar( name );
	if ( !var )
		MSG_WriteString( &cls.netchan.message, "Bad CVAR request" );
	else if ( var->flags & FCVAR_PRIVILEGED )
		MSG_WriteString( &cls.netchan.message, "CVAR is privileged" );
	else if ( var->flags & FCVAR_SERVER )
		MSG_WriteString( &cls.netchan.message, "CVAR is server-only" );
	else if ( var->flags & FCVAR_PROTECTED )
		MSG_WriteString( &cls.netchan.message, "CVAR is protected" );
	else
		MSG_WriteString( &cls.netchan.message, var->string );
}

void CL_Send_CvarValue2()
{
	int requestID;
	const char* name;
	cvar_t* var;

	requestID = MSG_ReadLong();
	name = MSG_ReadString();

	MSG_WriteByte( &cls.netchan.message, clc_cvarvalue2 );
	MSG_WriteLong( &cls.netchan.message, requestID );
	MSG_WriteString( &cls.netchan.message, name );

	if ( strlen( name ) >= 255 )
	{
		MSG_WriteString( &cls.netchan.message, "Bad CVAR request" );
		return;
	}

	var = Cvar_FindVar( name );
	if ( !var )
		MSG_WriteString( &cls.netchan.message, "Bad CVAR request" );
	else if ( var->flags & FCVAR_PRIVILEGED )
		MSG_WriteString( &cls.netchan.message, "CVAR is privileged" );
	else if ( var->flags & FCVAR_SERVER )
		MSG_WriteString( &cls.netchan.message, "CVAR is server-only" );
	else if ( var->flags & FCVAR_PROTECTED )
		MSG_WriteString( &cls.netchan.message, "CVAR is protected" );
	else
		MSG_WriteString( &cls.netchan.message, var->string );
}

static bool StuffTextHasCommand( const char* text, const char* command, bool lineBreaks )
{
	const char* match;
	char prev;

	match = Q_stristr( text, command );
	if ( !match )
		return false;

	if ( match == text )
		return true;

	prev = match[-1];
	return prev == ' ' || prev == ';' || ( lineBreaks && ( prev == '\n' || prev == '\r' ) );
}

qboolean ValidStuffText( const char* cmd )
{
	const char* pchTokCmd;
	const char* pos;
	const char* pos1;
	bool bGoodCommand;
	int i, len;

	if ( Host_IsSinglePlayerGame() && !cls.demoplayback )
		return true;

	Cmd_TokenizeString( cmd );
	pchTokCmd = Cmd_Argv( 0 );
	bGoodCommand = true;

	if ( pchTokCmd )
	{
		bGoodCommand = Q_stristr( pchTokCmd, "connect" ) != pchTokCmd;
		bGoodCommand &= Q_stristr( pchTokCmd, "bind" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "_set" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "unbind" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "retry" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "quit" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "_restart" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "motd_write" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "motdfile" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "kill" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "exit" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "writecfg" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "cl_filterstuffcmd" ) == NULL;
		bGoodCommand &= Q_stristr( pchTokCmd, "unbindall" ) == NULL;
		if ( !bGoodCommand )
			return false;
	}

	bGoodCommand &= _strnicmp( cmd, "alias ", sizeof( "alias " ) - 1 ) != 0;

	pos1 = Q_stristr( cmd, "reconnect" );
	pos = Q_stristr( cmd, "connect " );
	if ( pos && pos1 && pos == pos1 + ( sizeof( "re" ) - 1 ) )
		pos = Q_stristr( pos + 1, "connect " );
	if ( !pos1 || ( pos && pos != pos1 + ( sizeof( "re" ) - 1 ) ) )
		bGoodCommand &= !StuffTextHasCommand( pos, "connect ", true );

	bGoodCommand &= !StuffTextHasCommand( cmd, "motd_write", true );
	bGoodCommand &= !StuffTextHasCommand( cmd, "motdfile", true );
	bGoodCommand &= !StuffTextHasCommand( cmd, "retry", true );
	bGoodCommand &= !StuffTextHasCommand( cmd, "_set", true );
	bGoodCommand &= Q_stristr( cmd, "bind " ) == NULL;
	bGoodCommand &= Q_stristr( cmd, "unbind " ) == NULL;
	bGoodCommand &= !StuffTextHasCommand( cmd, "quit", true );
	bGoodCommand &= Q_stristr( cmd, "_restart" ) == NULL;
	bGoodCommand &= !StuffTextHasCommand( cmd, "kill", true );
	bGoodCommand &= Q_stristr( cmd, "exit" ) == NULL;
	if ( !g_bIsTFC )
		bGoodCommand &= Q_stristr( cmd, "exec" ) == NULL;
	bGoodCommand &= Q_stristr( cmd, "writecfg" ) == NULL;
	bGoodCommand &= Q_stristr( cmd, "cl_filterstuffcmd" ) == NULL;
	bGoodCommand &= Q_stristr( cmd, "unbindall" ) == NULL;

	if ( cl_filterstuffcmd.value > 0.0f )
	{
		bGoodCommand &= Q_stristr( cmd, "ex_interp " ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "say " ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "developer" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "timerefresh" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "rate" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "cd " ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "fps_max" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "speak_enabled" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "voice_enable" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "sensitivity" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "sys_ticrate" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "setinfo" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "removedemo" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "volume" ) == NULL;
		bGoodCommand &= Q_stristr( cmd, "mp3volume" ) == NULL;
		bGoodCommand &= !StuffTextHasCommand( cmd, "cl_", false );
		bGoodCommand &= !StuffTextHasCommand( cmd, "gl_", false );
		bGoodCommand &= !StuffTextHasCommand( cmd, "m_", false );
		bGoodCommand &= !StuffTextHasCommand( cmd, "r_", false );
		bGoodCommand &= !StuffTextHasCommand( cmd, "hud_", false );

		len = (int)strlen( cmd );
		for ( i = 0; i < len; i++ )
		{
			if ( (signed char)cmd[i] < ' ' && cmd[i] != '\n' )
				bGoodCommand = false;
		}
	}

	return bGoodCommand;
}

void CL_Parse_StuffText()
{
	const char* text;
	const char* pchTokCmd;

	text = MSG_ReadString();
	Cmd_TokenizeString( text );
	pchTokCmd = Cmd_Argv( 0 );

	if ( pchTokCmd && !_stricmp( pchTokCmd, "allow_autoaim" ) )
	{
		if ( Cmd_Argv( 1 ) )
			Cvar_DirectSet( &sv_allow_autoaim, *Cmd_Argv( 1 ) == '0' ? "0" : "1" );
		return;
	}

	if ( pchTokCmd && !_stricmp( pchTokCmd, "allow_shaders" ) )
	{
		if ( Cmd_Argv( 1 ) )
			Cvar_DirectSet( &sv_allow_shaders, *Cmd_Argv( 1 ) == '0' ? "0" : "1" );
		return;
	}

	if ( ValidStuffText( text ) )
		Cbuf_AddTextToUntrustedBuffer( text );
	else
		Con_Printf( "Server tried to send invalid command:\"%s\"\n", text );
}

void CL_Parse_SetView()
{
	cl.viewentity = MSG_ReadShort();
}

void CL_Parse_Print()
{
	Con_Printf( "%s", MSG_ReadString() );
}

void CL_Parse_SetAngle()
{
	int i;

	for ( i = 0; i < 3; i++ )
		cl.viewangles[i] = MSG_ReadHiresAngle();
}

void CL_Parse_CenterPrint()
{
	SCR_CenterPrint( MSG_ReadString() );
}

void CL_Parse_Intermission()
{
	cl.intermission = 1;
	vid.recalc_refdef = 1;
}

void CL_Parse_WeaponAnim()
{
	cl.weaponstarttime = 0.0f;
	cl.weaponsequence = MSG_ReadByte();
	cl.viewent.curstate.body = MSG_ReadByte();
}

void CL_Parse_RoomType()
{
	Cvar_SetValue( "room_type", (float)MSG_ReadShort() );
}

void CL_Parse_AddAngle()
{
	cl.viewangles[YAW] += MSG_ReadHiresAngle();
}

void CL_Parse_ServerInfo()
{
	CL_ParseServerInfo();
	vid.recalc_refdef = 1;
	sys_timescale.value = 1.0f;
}

void CL_Parse_SetPause()
{
	cl.paused = MSG_ReadByte();

	if ( !VGuiWrap2_IsInCareerMatch() )
	{
		if ( cl.paused )
			CDAudio_Pause();
		else
			CDAudio_Resume();
	}
}

void CL_Parse_SignonNum()
{
	int i;

	i = MSG_ReadByte();
	if ( i <= cls.signon )
	{
		Con_Printf( "Received signon %i when at %i\n", i, cls.signon );
		CL_Disconnect_f();
		return;
	}

	cls.signon = i;
	CL_SignonReply();
}

void CL_Parse_Finale()
{
	cl.intermission = 2;
	vid.recalc_refdef = 1;
	SCR_CenterPrint( MSG_ReadString() );
}

void CL_Parse_CDTrack()
{
	cl.cdtrack = MSG_ReadByte();
	cl.looptrack = MSG_ReadByte();

	if ( COM_CheckParm( "-nocdaudio" ) )
		return;

	if ( ( cls.demoplayback || cls.demorecording ) && cls.forcetrack != -1 )
		CDAudio_Play( cls.forcetrack, true );
	else
		CDAudio_Play( cl.cdtrack, true );
}

void CL_Parse_DeltaPacketEntities()
{
	cls.playerbits = 0;
	CL_ParsePacketEntities( true, &cls.playerbits );
	CL_SetSolidEntities();
}

void CL_Parse_Choke()
{
	cl.frames[cls.netchan.incoming_sequence & CL_UPDATE_MASK].choked = true;
}

void CL_Parse_CrosshairAngle()
{
	cl.crosshairangle[0] = (float)( MSG_ReadChar() * 0.2 );
	cl.crosshairangle[1] = (float)( MSG_ReadChar() * 0.2 );
}

void CL_Parse_Timescale()
{
	sys_timescale.value = MSG_ReadFloat();

	if ( ( !cls.spectator && !cls.demoplayback ) || sys_timescale.value < 0.1 )
		sys_timescale.value = 1.0f;
}

void CL_Parse_VoiceInit()
{
	int quality;

	MSG_ReadString();
	quality = MSG_ReadByte();
	Voice_Init( "", quality );
}

void CL_Parse_Version()
{
	int i;

	i = MSG_ReadLong();
	if ( i != PROTOCOL_VERSION )
		Host_Error( "CL_Parse_Version: Server is protocol %i instead of %i\n", i, PROTOCOL_VERSION );
}

void CL_Parse_StopSound()
{
	int i;

	i = MSG_ReadShort();
	S_StopSound( i >> 3, i & 7 );
}

void CL_Parse_CutScene()
{
	cl.intermission = 3;
	vid.recalc_refdef = 1;
	SCR_CenterPrint( MSG_ReadString() );
}

void CL_Parse_Restore()
{
	CL_Restore( MSG_ReadString() );
}

void CL_Parse_PacketEntities()
{
	cls.playerbits = 0;
	CL_ParsePacketEntities( false, &cls.playerbits );
	CL_SetSolidEntities();
}

void CL_Parse_LightStyle()
{
	int i;
	lightstyle_t* ls;

	i = MSG_ReadByte();
	if ( i >= MAX_LIGHTSTYLES )
		Sys_Error( "svc_lightstyle > MAX_LIGHTSTYLES" );

	ls = &cl_lightstyle[i];
	Q_strncpy( ls->map, MSG_ReadString(), sizeof( ls->map ) - 1 );
	ls->map[sizeof( ls->map ) - 1] = 0;
	ls->length = Q_strlen( ls->map );
}

void CL_Parse_Pings()
{
	int slot, ping, loss;

	MSG_StartBitReading( &net_message );

	while ( MSG_ReadBits( 1 ) )
	{
		slot = (int)MSG_ReadBits( PING_SLOT_BITS );
		ping = (int)MSG_ReadBits( PING_VALUE_BITS );
		loss = (int)MSG_ReadBits( PING_LOSS_BITS );

		cl.players[slot].ping = ping;
		cl.players[slot].packet_loss = loss;
	}

	MSG_EndBitReading( &net_message );
}

void CL_Parse_DecalName()
{
	int index;
	char* name;

	index = MSG_ReadByte();
	name = MSG_ReadString();
	Draw_DecalSetName( index, name );
}

void CL_Set_ServerExtraInfo()
{
	char* gamedir;
	int cheats;
	char value[32];

	gamedir = MSG_ReadString();
	cheats = MSG_ReadByte() != 0;

	snprintf( value, sizeof( value ), "%d", cheats );
	Cvar_DirectSet( &sv_cheats, value );
	COM_AddDefaultDir( gamedir );
}

void CL_ParseFileTxferFailed()
{
	const char* filename;

	filename = MSG_ReadString();
	if ( !filename || !filename[0] )
	{
		Host_Error( "CL_ParseFileTxferFailed:  empty filename\n" );
		return;
	}

	if ( !cls.demoplayback )
		CL_ProcessFile( false, filename );
}

void CL_ClientQueueEvent( int flags, int index, int, float delay, event_args_t* args )
{
	CL_QueueEvent( flags, index, delay, args );
}

event_hook_t* CL_FindEventHook( const char* name )
{
	event_hook_t* hook;

	if ( !name )
		return NULL;

	for ( hook = g_pEventHooks; hook; hook = hook->next )
	{
		if ( hook->name && !Q_strcasecmp( name, hook->name ) )
			return hook;
	}

	return NULL;
}

void CL_ResetEvent( event_info_t* ei )
{
	ei->index = 0;
	Q_memset( &ei->args, 0, sizeof( ei->args ) );
	ei->fire_time = 0.0f;
	ei->flags = 0;
}

void CL_DescribeEvent( int slot, const char* eventname )
{
	if ( cl_showevents.value != 0.0f && eventname )
		Con_NPrintf( slot & 31, "%i %f %s", slot, cl.time, eventname );
}

void CL_FireEvents()
{
	int i;
	event_info_t* ei;
	const char* name;
	event_hook_t* hook;

	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &cl.events.ei[i];
		if ( !ei->index )
			continue;

		if ( ei->fire_time != 0.0f && ei->fire_time > cl.time )
			continue;

		name = cl.event_precache[ei->index].filename;
		hook = CL_FindEventHook( name );
		if ( hook )
		{
			CL_DescribeEvent( i, name );
			hook->pfnEvent( &ei->args );
		}
		else
		{
			Con_DPrintf( "CL_FireEvents:  Unknown event %i:%s\n", ei->index, name );
		}

		CL_ResetEvent( ei );
	}
}

void CL_HookEvent( char* name, void ( *pfnEvent )( event_args_t* args ) )
{
	event_hook_t* hook;

	g_engdstAddrs.pfnHookEvent( &name, &pfnEvent );

	if ( !name || !name[0] )
	{
		Con_Printf( "CL_HookEvent:  Must provide a valid event name\n" );
		return;
	}

	if ( !pfnEvent )
	{
		Con_Printf( "CL_HookEvent:  Must provide an event hook callback\n" );
		return;
	}

	hook = CL_FindEventHook( name );
	if ( hook )
	{
		Con_DPrintf( "CL_HookEvent:  Called on existing hook, updating event hook\n" );
		hook->pfnEvent = pfnEvent;
		return;
	}

	hook = (event_hook_t*)Mem_ZeroMalloc( sizeof( event_hook_t ) );
	hook->name = Mem_Strdup( name );
	hook->pfnEvent = pfnEvent;

	hook->next = g_pEventHooks;
	g_pEventHooks = hook;
}

void CL_InitEventSystem()
{
	g_pEventHooks = NULL;
}

void CL_FlushEventSystem()
{
	event_hook_t* hook;
	event_hook_t* next;

	for ( hook = g_pEventHooks; hook; hook = next )
	{
		next = hook->next;
		Mem_Free( hook->name );
		Mem_Free( hook );
	}

	g_pEventHooks = NULL;
}

event_info_t* CL_FindEmptyEvent()
{
	int i;

	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		if ( !cl.events.ei[i].index )
			return &cl.events.ei[i];
	}

	return NULL;
}

event_info_t* CL_FindUnreliableEvent()
{
	int i;

	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		if ( !cl.events.ei[i].index || !( cl.events.ei[i].flags & FEV_RELIABLE ) )
			return &cl.events.ei[i];
	}

	return NULL;
}

void CL_QueueEvent( int flags, int index, float delay, event_args_t* args )
{
	event_info_t* ei;

	ei = CL_FindEmptyEvent();
	if ( !ei && ( flags & FEV_RELIABLE ) )
		ei = CL_FindUnreliableEvent();
	if ( !ei )
		return;

	ei->index = (unsigned short)index;
	ei->packet_index = 0;
	ei->args = *args;
	ei->fire_time = delay != 0.0f ? (float)( cl.time + delay ) : 0.0f;
	ei->flags = flags;
}

void CL_CalcPlayerVelocity( int index, float* velocity )
{
	cl_entity_t* ent;
	vec3_t delta;
	float dt;

	VectorClear( velocity );

	if ( index < 1 || index > cl.maxclients )
		return;

	if ( index - 1 == cl.playernum )
	{
		VectorCopy( cl.frames[cl.parsecountmod].clientdata.velocity, velocity );
		return;
	}

	ent = &cl_entities[index];
	VectorSubtract( ent->curstate.velocity, ent->prevstate.velocity, delta );

	dt = ent->curstate.animtime - ent->prevstate.animtime;
	if ( dt != 0.0f )
		VectorScale( delta, 1.0f / dt, velocity );
	else
		VectorCopy( ent->curstate.velocity, velocity );
}

void CL_ParseEvent()
{
	delta_t** fields;
	event_args_t nullargs, args;
	packet_entities_t* pack;
	entity_state_t* state;
	int i, count, index, packetIndex;
	float delay;

	delay = 0.0f;
	fields = DELTA_LookupRegistration( "event_t" );

	Q_memset( &nullargs, 0, sizeof( nullargs ) );

	MSG_StartBitReading( &net_message );

	count = (int)MSG_ReadBits( EVENT_COUNT_BITS );
	for ( i = 0; i < count; i++ )
	{
		index = (int)MSG_ReadBits( EVENT_INDEX_BITS );

		packetIndex = -1;
		if ( MSG_ReadBits( 1 ) )
			packetIndex = (int)MSG_ReadBits( EVENT_ENTITY_BITS );

		// The server sends no arguments when they are all zero, so do not keep the previous event's
		Q_memset( &args, 0, sizeof( args ) );
		if ( MSG_ReadBits( 1 ) )
			DELTA_ParseDelta( (byte*)&nullargs, (byte*)&args, *fields, sizeof( args ) );

		if ( MSG_ReadBits( 1 ) )
			delay = (float)( MSG_ReadBits( EVENT_DELAY_BITS ) / 100.0 );

		if ( packetIndex == -1 )
			continue;

		pack = &cl.frames[cl.parsecountmod].packet_entities;
		if ( packetIndex >= pack->num_entities )
		{
			if ( args.entindex )
			{
				if ( args.entindex >= 1 && args.entindex <= cl.maxclients )
					args.angles[PITCH] = (float)( args.angles[PITCH] * -3.0 );
			}
			else
			{
				Con_DPrintf( "CL_ParseEvent:  Received non-packet entity index 0 for event\n" );
			}
		}
		else
		{
			state = &pack->entities[packetIndex];
			args.entindex = state->number;

			if ( VectorCompare( args.origin, vec3_origin ) )
				VectorCopy( state->origin, args.origin );

			if ( VectorCompare( args.angles, vec3_origin ) )
				VectorCopy( state->angles, args.angles );

			COM_NormalizeAngles( args.angles );

			if ( state->number >= 1 && state->number <= cl.maxclients )
			{
				args.angles[PITCH] = (float)( args.angles[PITCH] * -3.0 );
				CL_CalcPlayerVelocity( state->number, args.velocity );
				args.ducking = state->usehull == 1;
			}
		}

		CL_QueueEvent( 0, index, delay, &args );
	}

	MSG_EndBitReading( &net_message );
}

void CL_Parse_ReliableEvent()
{
	delta_t** fields;
	event_args_t nullargs, args;
	int index;
	float delay;

	fields = DELTA_LookupRegistration( "event_t" );

	Q_memset( &nullargs, 0, sizeof( nullargs ) );
	Q_memset( &args, 0, sizeof( args ) );

	MSG_StartBitReading( &net_message );

	index = (int)MSG_ReadBits( EVENT_INDEX_BITS );
	DELTA_ParseDelta( (byte*)&nullargs, (byte*)&args, *fields, sizeof( args ) );

	delay = 0.0f;
	if ( MSG_ReadBits( 1 ) )
		delay = (float)( MSG_ReadBits( EVENT_DELAY_BITS ) / 100.0 );

	if ( args.entindex >= 1 && args.entindex <= cl.maxclients )
		args.angles[PITCH] = (float)( args.angles[PITCH] * -3.0 );

	CL_QueueEvent( FEV_RELIABLE, index, delay, &args );

	MSG_EndBitReading( &net_message );
}

void CL_AdjustClock()
{
	double msec, sign, adjust;
	float fixrate;

	if ( g_clockdelta == 0.0 )
		return;

	fixrate = cl_fixtimerate.value;
	if ( fixrate == 0.0f )
		return;

	if ( fabs( g_clockdelta ) >= 0.001 )
	{
		if ( fixrate < 0.0f )
		{
			cl_fixtimerate.value = 7.5f;
			fixrate = 7.5f;
		}

		msec = (int)( g_clockdelta * 1000.0 );
		sign = msec < 0.0 ? 1.0 : -1.0;
		msec = fabs( msec );

		adjust = fixrate;
		if ( adjust > msec )
			adjust = msec;
		adjust = sign * ( adjust / 1000.0 );

		if ( fabs( g_clockdelta ) > fabs( adjust ) )
		{
			g_clockdelta += adjust;
			cl.time += adjust;
		}

		if ( cl.oldtime > cl.time )
			cl.oldtime = cl.time;
	}
}

qboolean AllowedCommand( const char* pCmdString, const char* pBannedString )
{
	return !StuffTextHasCommand( pCmdString, pBannedString, true );
}

void CL_Parse_VoiceData()
{
	char received[8192];
	int start, client, entity, channel;
	unsigned int size;
	frame_t* frame;

	start = msg_readcount;
	client = MSG_ReadByte();
	entity = client + 1;

	size = (unsigned int)MSG_ReadShort();
	if ( size > sizeof( received ) )
		size = sizeof( received );
	MSG_ReadBuf( (int)size, received );

	frame = &cl.frames[cl.parsecountmod];
	frame->voicebytes = (unsigned short)( msg_readcount + frame->voicebytes - start );

	if ( entity < 0 || entity >= cl.num_entities )
		return;

	if ( entity == cl.playernum + 1 )
		Voice_LocalPlayerTalkingAck();

	if ( !size )
		return;

	channel = Voice_GetChannel( entity );
	if ( channel == -1 )
		channel = Voice_AssignChannel( entity );

	if ( channel == -1 )
		Con_DPrintf( "CL_ParseVoiceData: Voice_AssignChannel failed for client %d!\n", client );
	else
		Voice_AddIncomingData( channel, received, (int)size, cls.netchan.incoming_sequence );
}

void CL_Exec()
{
	static const char* sClassCfgs[] = { "", "exec scout.cfg\n", "exec sniper.cfg\n", "exec soldier.cfg\n",
		"exec demoman.cfg\n", "exec medic.cfg\n", "exec hwguy.cfg\n", "exec pyro.cfg\n", "exec spy.cfg\n",
		"exec engineer.cfg\n", "", "exec civilian.cfg\n" };
#if defined( _WIN32 )
	char mapName[MAX_PATH];
#else
	char mapName[4096];
#endif
	char exec[1024];
	int execType;
	unsigned int classIndex;

	execType = MSG_ReadByte();
	if ( execType == 0 )
	{
		if ( !g_bIsTFC )
			return;

		Cbuf_AddText( "exec mapdefault.cfg\n" );

		if ( (unsigned int)Q_strlen( cl.levelname ) < sizeof( mapName ) - 1 )
		{
			COM_FileBase( cl.levelname, mapName );
			if ( mapName[0] )
			{
				snprintf( exec, sizeof( exec ), "exec %s.cfg\n", mapName );
				Cbuf_AddText( exec );
			}
		}
	}
	else if ( execType == 1 )
	{
		classIndex = (unsigned int)MSG_ReadByte();
		if ( g_bIsTFC && classIndex < sizeof( sClassCfgs ) / sizeof( sClassCfgs[0] ) )
			Cbuf_AddText( sClassCfgs[classIndex] );
	}
}
