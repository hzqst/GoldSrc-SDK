// cl_ents.c -- entity parsing and management
#include "client.h"
#include "common.h"
#include "conprint.h"
#include "delta.h"
#include "enginecallbacks.h"
#include "gl_model.h"
#include "host.h"
#include "mem.h"
#include "pm_defs.h"
#include "protocol.h"
#include "server.h"
#include "sys.h"
#include "../common/con_nprint.h"
#include "../common/entity_types.h"
#include "../common/particledef.h"
#include "../common/r_efx.h"
#if defined( SW_RENDERER )
#include "engine_color.h"
extern word* host_basepal;
#endif

#include <cstdio>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\cl_ents.c" )

#define MAX_PACKET_ENTITIES 1024

#define FLASHLIGHT_DISTANCE 2000

#define EF_ROCKET 1 // leave a trail
#define EF_GRENADE 2 // leave a trail
#define EF_GIB 4 // leave a trail
#define EF_TRACER 16 // green split trail
#define EF_ZOMGIB 32 // small blood trail
#define EF_TRACER2 64 // orange split trail + rotate
#define EF_TRACER3 128 // purple trail

extern local_state_t spectatorState;
extern double parsecounttime;
extern particle_t* free_particles;
extern particle_t* active_particles;
extern cvar_t cl_solid_players;
extern cvar_t chase_active;
extern cvar_t cl_fixmodelinterpolationartifacts;
extern cvar_t cl_needinstanced;
extern cvar_t r_bmodelinterp;
extern cvar_t ex_interp;
extern playermove_t g_clmove;
extern vec3_t r_playerViewportAngles;
extern int r_dlightchanged;
extern mleaf_t* r_oldviewleaf;

double g_flLatency;

typedef struct
{
	int movetype;
	int solid;
	int usehull;
	qboolean active;
	vec3_t origin; // predicted origin
	vec3_t angles;
} predicted_player;

static predicted_player predicted_players[MAX_CLIENTS];

void CL_ComputePlayerOrigin( cl_entity_t* ent );
void CL_UpdatePositions( cl_entity_t* ent );
void CL_ResetPositions( cl_entity_t* ent );
qboolean CL_FindInterpolationUpdates(
	cl_entity_t* ent, float targettime, position_history_t** ph0, position_history_t** ph1, int* ph0Index );
cl_entity_t* CL_EntityNum( int num );
qboolean CL_IsPlayerIndex( int index );
void CL_ProcessEntityUpdate( cl_entity_t* ent );
void CL_ResetLatchedState( int pnum, packet_entities_t* pack, cl_entity_t* ent );
void CL_ProcessPlayerState( int playerindex, entity_state_t* state );
void CL_ClearPacket( packet_entities_t* pack );
void CL_SetPacketFlag( packet_entities_t* pack, int index, int flag );
void CL_ResetFrameStats( frame_t* frame );
void CL_LinkCustomEntity( cl_entity_t* ent, entity_state_t* state );
void CL_LinkPlayers();
void CL_TouchLight( dlight_t* dl );
void CL_AddModelEffects( int keynum, cl_entity_t* ent, model_s* model );
void CL_TempEntUpdate();
void R_KillDeadBeams( int deadEntity );
void R_UpdateLatchedVars( cl_entity_t* ent );
void R_ResetLatched( cl_entity_t* ent, qboolean full_reset );
void R_MarkLeaves();
void PVSMark( model_t* model, byte* ppvs );

void CL_AddStateToEntlist( physent_t* pe, entity_state_t* state )
{
	model_t* model;

	pe->info = state->number;
	if ( CL_IsPlayerIndex( state->number ) )
		pe->player = state->number;

	VectorCopy( state->origin, pe->origin );
	VectorCopy( state->angles, pe->angles );
	VectorCopy( state->mins, pe->mins );
	VectorCopy( state->maxs, pe->maxs );

	pe->frame = state->frame;
	pe->sequence = state->sequence;
	pe->rendermode = state->rendermode;
	pe->skin = state->skin;
	pe->solid = state->solid;
	Q_memcpy( pe->controller, state->controller, sizeof( pe->controller ) );
	Q_memcpy( pe->blending, state->blending, sizeof( pe->blending ) );
	pe->movetype = state->movetype;
	pe->takedamage = DAMAGE_NO;
	pe->blooddecal = 0;

	if ( pe->solid == SOLID_BBOX )
	{
		pe->model = NULL;
		pe->studiomodel = NULL;

		model = CL_GetModelByIndex( state->modelindex );
		Q_strncpy( pe->name, model->name, sizeof( pe->name ) );
		pe->name[sizeof( pe->name ) - 1] = 0;

		if ( model->flags & STUDIO_TRACE_HITBOX )
			pe->studiomodel = model;
	}
	else
	{
		if ( pe->solid != SOLID_BSP && CL_GetModelByIndex( state->modelindex )->type == mod_studio )
		{
			pe->model = NULL;
			model = CL_GetModelByIndex( state->modelindex );
			pe->studiomodel = model;
		}
		else
		{
			model = CL_GetModelByIndex( state->modelindex );
			pe->model = model;
			pe->studiomodel = NULL;
		}

		Q_strncpy( pe->name, model->name, sizeof( pe->name ) );
		pe->name[sizeof( pe->name ) - 1] = 0;
	}

	pe->iuser1 = state->iuser1;
	pe->iuser2 = state->iuser2;
	pe->iuser3 = state->iuser3;
	pe->iuser4 = state->iuser4;
	pe->fuser1 = state->fuser1;
	pe->fuser2 = state->fuser2;
	pe->fuser3 = state->fuser3;
	pe->fuser4 = state->fuser4;
	VectorCopy( state->vuser1, pe->vuser1 );
	VectorCopy( state->vuser2, pe->vuser2 );
	VectorCopy( state->vuser3, pe->vuser3 );
	VectorCopy( state->vuser4, pe->vuser4 );

	pe->team = state->team;
	pe->classnumber = state->playerclass;
}

//======================================================================
/*
===============
CL_SetSolid

Builds all the pmove physents for the current frame
===============
*/
static bool CL_IsPredictionModel( const model_t* model )
{
	return model->hulls[1].firstclipnode || model->type == mod_studio
		 || ( model->type == mod_brush && model->name[0] != '*' && model != cl.worldmodel );
}

void CL_SetSolidEntities()
{
	int i;
	physent_t* pe;
	packet_entities_t* pack;
	entity_state_t* state;
	model_t* model;
	vec3_t size;

	pmove = &g_clmove;

	Q_memset( &pmove->physents[0], 0, sizeof( physent_t ) );
	Q_memset( &pmove->visents[0], 0, sizeof( physent_t ) );

	pe = &pmove->physents[0];
	pe->model = cl.worldmodel;
	if ( cl.worldmodel )
	{
		Q_strncpy( pe->name, cl.worldmodel->name, sizeof( pe->name ) );
		pe->name[sizeof( pe->name ) - 1] = 0;
	}
	VectorCopy( vec3_origin, pe->origin );
	pe->info = 0;
	pe->solid = SOLID_BSP;
	pe->movetype = MOVETYPE_NONE;
	pe->takedamage = DAMAGE_YES;
	pe->blooddecal = 0;

	pmove->numphysent = 1;
	pmove->numvisent = 1;
	pmove->visents[0] = pmove->physents[0];
	pmove->nummoveent = 0;

	pack = &cl.frames[cl.parsecountmod].packet_entities;
	for ( i = 0; i < pack->num_entities; i++ )
	{
		state = &pack->entities[i];

		if ( CL_IsPlayerIndex( state->number ) || !state->modelindex )
			continue;

		model = CL_GetModelByIndex( state->modelindex );
		if ( !model )
			continue;

		if ( state->owner
			 && static_cast<unsigned int>( state->owner ) - 1u == static_cast<unsigned int>( cl.playernum ) )
			continue;

		if ( CL_IsPredictionModel( model ) && pmove->numvisent < MAX_PHYSENTS )
			CL_AddStateToEntlist( &pmove->visents[pmove->numvisent++], state );

		if ( state->solid == SOLID_TRIGGER || ( state->solid == SOLID_NOT && state->skin >= CONTENTS_EMPTY ) )
			continue;

		if ( state->mins[2] == 0.0f && state->maxs[2] == 1.0f )
			continue;

		VectorSubtract( state->maxs, state->mins, size );
		if ( Length( size ) == 0.0f || !CL_IsPredictionModel( model ) )
			continue;

		if ( state->solid == SOLID_NOT && state->skin == CONTENTS_LADDER )
		{
			if ( pmove->nummoveent >= MAX_MOVEENTS )
			{
				Con_DPrintf( "CL_SetSolidEntities:  pmove->nummoveent >= MAX_MOVEENTS\n" );
				continue;
			}
			pe = &pmove->moveents[pmove->nummoveent++];
		}
		else
		{
			if ( pmove->numphysent >= MAX_PHYSENTS )
			{
				Con_DPrintf( "CL_SetSolidEntities:  pmove->numphysent >= MAX_PHYSENTS\n" );
				continue;
			}
			pe = &pmove->physents[pmove->numphysent++];
		}

		CL_AddStateToEntlist( pe, state );
	}
}

qboolean CL_IsPlayerIndex( int index )
{
	return index >= 1 && index <= cl.maxclients;
}

qboolean CL_ParametricMove( cl_entity_t* ent )
{
	float frac, dt, t;
	vec3_t delta;

	if ( ent->curstate.starttime == 0.0f || ent->curstate.impacttime == 0.0f )
		return FALSE;

	dt = ent->curstate.impacttime - ent->curstate.starttime;
	VectorSubtract( ent->curstate.endpos, ent->curstate.startpos, delta );

	if ( dt != 0.0f )
	{
		t = (float)cl.time;
		if ( ent->lastmove > cl.time )
			t = ent->lastmove;

		frac = ( t - ent->curstate.starttime ) / dt;
		if ( frac > 1.0f )
			frac = 1.0f;
		if ( frac < 0.0f )
			frac = 0.0f;

		VectorMA( ent->curstate.startpos, frac, delta, ent->curstate.origin );
		ent->lastmove = t;
	}

	VectorCopy( ent->curstate.origin, ent->origin );

	VectorNormalize( delta );
	if ( Length( delta ) > 0.0f )
		VectorAngles( delta, ent->curstate.angles );

	return TRUE;
}

void CL_ProcessPacket( frame_t* frame )
{
	int i, pnum;
	entity_state_t* state;
	cl_entity_t* ent;

	for ( i = 0; i < frame->packet_entities.num_entities; i++ )
	{
		state = &frame->packet_entities.entities[i];
		state->messagenum = cl.parsecount;
		state->msg_time = (float)cl.mtime[0];

		ent = &cl_entities[state->number];
		ent->player = CL_IsPlayerIndex( state->number );

		if ( state->number - 1 == cl.playernum )
			ClientDLL_TxferLocalOverrides( state, &frame->clientdata );

		ent->prevstate = ent->curstate;
		ent->curstate = *state;

		CL_ProcessEntityUpdate( ent );
		CL_ResetLatchedState( i, &frame->packet_entities, ent );

		if ( ent->player )
		{
			pnum = state->number - 1;
			CL_ProcessPlayerState( pnum, state );
			if ( pnum == cl.playernum )
				CL_CheckPredictionError();
		}
	}
}

int CL_ParseDeltaHeader( qboolean* remove, qboolean* custom, int* numbase, qboolean* newbl, int* newblindex,
	qboolean full, int* offset )
{
	int num;

	*custom = FALSE;
	*newbl = FALSE;

	if ( full )
	{
		*remove = FALSE;
		if ( MSG_ReadBits( 1 ) )
			num = *numbase + 1;
		else if ( MSG_ReadBits( 1 ) )
			num = MSG_ReadBits( 11 );
		else
			num = *numbase + MSG_ReadBits( 6 );
	}
	else
	{
		*remove = MSG_ReadBits( 1 );
		if ( MSG_ReadBits( 1 ) )
			num = MSG_ReadBits( 11 );
		else
			num = *numbase + MSG_ReadBits( 6 );
	}

	*numbase = num;

	if ( !*remove )
	{
		*custom = MSG_ReadBits( 1 );

		if ( cl.instanced_baseline_number )
		{
			*newbl = MSG_ReadBits( 1 );
			if ( *newbl )
				*newblindex = MSG_ReadBits( 6 );
		}
		else
		{
			*newbl = FALSE;
		}

		if ( !full || *newbl )
		{
			*offset = 0;
		}
		else
		{
			*offset = MSG_ReadBits( 1 );
			if ( *offset )
				*offset = MSG_ReadBits( 6 );
		}
	}

	return num;
}

void CL_FlushEntityPacket( qboolean needbitencodingframe )
{
	int num;
	int numbase = 0;
	qboolean remove = FALSE;
	qboolean custom = FALSE;
	entity_state_t olde, newe;
	delta_t **ppplayer, **ppentity, **ppcustom;
	con_nprint_t np = { 0, 1.0f, { 1.0f, 0.2f, 0.0f } };

	ppplayer = DELTA_LookupRegistration( "entity_state_player_t" );
	ppentity = DELTA_LookupRegistration( "entity_state_t" );
	ppcustom = DELTA_LookupRegistration( "custom_entity_state_t" );

#if defined( _WIN32 )
	ASSERT_AT( ppentity && *ppentity && ppcustom && *ppcustom && ppplayer && *ppplayer, 333 );
#endif

	Con_NXPrintf( &np, "WARNING:  CL_FlushEntityPacket" );

	Q_memset( &olde, 0, sizeof( olde ) );

	cl.validsequence = 0; // can't render a frame
	cl.frames[cl.parsecountmod].invalid = TRUE;

	if ( needbitencodingframe )
		MSG_StartBitReading( &net_message );

	// read it all, but ignore it
	while ( 1 )
	{
		if ( MSG_PeekBits( 16 ) )
		{
			custom = FALSE;
			remove = MSG_ReadBits( 1 );
			if ( MSG_ReadBits( 1 ) )
				numbase = MSG_ReadBits( 11 );
			else
				numbase += MSG_ReadBits( 6 );

			if ( !remove )
			{
				custom = MSG_ReadBits( 1 );
				if ( cl.instanced_baseline_number && MSG_ReadBits( 1 ) )
					MSG_ReadBits( 6 );
			}
			num = numbase;
		}
		else
		{
			num = MSG_ReadBits( 16 );
		}

		if ( msg_badread )
			Host_EndGame( "msg_badread in packetentities\n" );

		if ( !num )
			break; // done

		if ( !remove )
		{
			DELTA_ParseDelta( (byte*)&olde, (byte*)&newe,
				custom ? *ppcustom : ( CL_IsPlayerIndex( num ) ? *ppplayer : *ppentity ), sizeof( entity_state_t ) );
		}
	}

	if ( needbitencodingframe )
		MSG_EndBitReading( &net_message );
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities( qboolean delta, int* playerbits )
{
	int count, from, bits;
	int oldindex, newindex;
	int newnum, oldnum;
	int numbase = 0;
	int newblindex = 0;
	int offset = 0;
	qboolean remove = FALSE;
	qboolean custom = FALSE;
	qboolean newbl = FALSE;
	qboolean full;
	frame_t* frame;
	packet_entities_t *oldp, *newp, dummy;
	entity_state_t *baseline, *state;
	delta_t **ppentity, **ppplayer, **ppcustom, *pdesc;

	ppentity = DELTA_LookupRegistration( "entity_state_t" );
	ppplayer = DELTA_LookupRegistration( "entity_state_player_t" );
	ppcustom = DELTA_LookupRegistration( "custom_entity_state_t" );

#if defined( _WIN32 )
	ASSERT_AT( ppentity && *ppentity && ppcustom && *ppcustom && ppplayer && *ppplayer, 929 );
#endif

	if ( cls.signon == 1 )
	{
		// We are done with signon sequence.
		cls.signon = 2;
		CL_SignonReply();
	}

	frame = &cl.frames[cl.parsecountmod];
	frame->invalid = FALSE;
	newp = &frame->packet_entities;

	count = MSG_ReadShort();

	if ( newp->entities )
		Mem_Free( newp->entities );
	newp->entities = (entity_state_t*)Mem_ZeroMalloc( sizeof( entity_state_t ) * ( count < 1 ? 1 : count ) );
	newp->num_entities = count;
	Q_memset( newp->flags, 0, sizeof( newp->flags ) );

	if ( delta )
	{
		from = (byte)MSG_ReadByte();
	}
	else
	{
		from = -1;
		if ( cls.demowaiting )
		{
			cls.demowaiting = FALSE;
			MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
			MSG_WriteString( &cls.netchan.message, "fullupdate" );
		}
	}

	full = FALSE;

	if ( !cl.max_edicts )
	{
		CL_FlushEntityPacket( TRUE );
		CL_ClearPacket( newp );
		return;
	}

	if ( from == -1 )
	{
		Q_memset( &dummy, 0, sizeof( dummy ) );
		dummy.num_entities = 0;
		oldp = &dummy;
		full = TRUE;
	}
	else
	{
		if ( (byte)( cls.netchan.incoming_sequence - from ) >= CL_UPDATE_MASK )
		{
			// we can't use this, it is too old
			CL_FlushEntityPacket( TRUE );
			CL_ClearPacket( newp );
			return;
		}
		oldp = &cl.frames[from & CL_UPDATE_MASK].packet_entities;
	}

	cl.validsequence = cls.netchan.incoming_sequence;

	oldindex = 0;
	newindex = 0;

	MSG_StartBitReading( &net_message );

	while ( 1 )
	{
		if ( MSG_PeekBits( 16 ) )
			newnum = CL_ParseDeltaHeader( &remove, &custom, &numbase, &newbl, &newblindex, full, &offset );
		else
			newnum = MSG_ReadBits( 16 );

		if ( !newnum )
			break; // done

		oldnum = oldindex < oldp->num_entities ? oldp->entities[oldindex].number : 9999;

		while ( newnum > oldnum )
		{
			if ( full )
			{
				Con_Printf( "WARNING: oldcopy invalid on non-delta compressed update\n" );
				CL_FlushEntityPacket( FALSE );
				CL_ClearPacket( newp );
				break;
			}

//Con_Printf ("copy %i\n", oldp->entities[oldindex].number);
			// copy one of the old entities over to the new packet unchanged
			if ( newindex >= MAX_PACKET_ENTITIES )
				Host_EndGame( "CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES" );
			newp->entities[newindex++] = oldp->entities[oldindex++];

			oldnum = oldindex < oldp->num_entities ? oldp->entities[oldindex].number : 9999;
		}

		if ( newnum < oldnum )
		{
			// new from baseline
			if ( remove )
			{
				if ( full )
				{
					Con_Printf( "WARNING: remove invalid on non-delta compressed update\n" );
					CL_FlushEntityPacket( FALSE );
					CL_ClearPacket( newp );
					break;
				}
				continue;
			}

//Con_Printf ("copy %i\n", oldnum);
			if ( newindex >= MAX_PACKET_ENTITIES )
				Host_EndGame( "CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES" );

			if ( newbl )
				baseline = &cl.instanced_baseline[newblindex];
			else if ( offset )
				baseline = &newp->entities[newindex - offset];
			else
				baseline = &cl_entities[newnum].baseline;

			CL_EntityNum( newnum );

			state = &newp->entities[newindex];
			pdesc = custom ? *ppcustom : ( CL_IsPlayerIndex( newnum ) ? *ppplayer : *ppentity );
			bits = DELTA_ParseDelta( (byte*)baseline, (byte*)state, pdesc, sizeof( entity_state_t ) );

			state->entityType = custom ? ENTITY_BEAM : ENTITY_NORMAL;
			if ( !custom && CL_IsPlayerIndex( newnum ) && playerbits )
				*playerbits += bits;
			state->number = newnum;

			if ( cl_needinstanced.value && newnum > cl.highentity && !newbl )
			{
				Con_DPrintf( "Non-instanced %i %s without baseline\n", newnum,
					state->modelindex ? cl.model_precache[state->modelindex]->name : "?" );
			}

			if ( !full || !cls.passive )
				CL_SetPacketFlag( newp, newindex, TRUE );
			newindex++;
		}
		else if ( newnum == oldnum )
		{
			// delta from previous
			if ( full )
			{
				cl.validsequence = 0;
				Con_Printf( "WARNING: delta on full update" );
			}

			if ( remove )
			{
				R_KillDeadBeams( oldnum );
				oldindex++;
				continue;
			}

			state = &newp->entities[newindex];
			pdesc = custom ? *ppcustom : ( CL_IsPlayerIndex( newnum ) ? *ppplayer : *ppentity );
			bits = DELTA_ParseDelta( (byte*)&oldp->entities[oldindex], (byte*)state, pdesc, sizeof( entity_state_t ) );

			state->entityType = custom ? ENTITY_BEAM : ENTITY_NORMAL;
			if ( !custom && CL_IsPlayerIndex( newnum ) && playerbits )
				*playerbits += bits;
			newindex++;
			state->number = newnum;
			oldindex++;
		}
	}

	// copy all the rest of the entities from the old packet
	while ( oldindex < oldp->num_entities )
	{
		if ( newindex >= MAX_PACKET_ENTITIES )
			Host_EndGame( "CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES" );
		newp->entities[newindex++] = oldp->entities[oldindex++];
	}

	MSG_EndBitReading( &net_message );

	if ( msg_badread )
		Host_EndGame( "msg_badread in packetentities\n" );

	CL_ProcessPacket( frame );
}

/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata()
{
	static int last_incoming_sequence;
	static int last_command_ack;
	int i, incoming, command_ack;
	frame_t *frame, *fromframe;
	cmd_t* cmd;
	local_state_t* pstate;
	clientdata_t nullcd;
	weapon_data_t nullwd;
	byte* from;
	delta_t **ppdelta, **ppweapon;

	ppdelta = DELTA_LookupRegistration( "clientdata_t" );
	ppweapon = DELTA_LookupRegistration( "weapon_data_t" );

#if defined( _WIN32 )
	ASSERT_AT( ppdelta != NULL && *ppdelta != NULL && ppweapon != NULL && *ppweapon != NULL, 738 );
#endif

	Q_memset( &nullcd, 0, sizeof( nullcd ) );

	incoming = cls.netchan.incoming_sequence;
	command_ack = cls.netchan.incoming_acknowledged;

	for ( i = last_incoming_sequence + 1; i < incoming; i++ )
	{
		frame = &cl.frames[i & CL_UPDATE_MASK];
		if ( frame->receivedtime >= 0.0 )
		{
			frame->receivedtime = -1.0;
			frame->latency = 0.0;
		}
	}

	cl.parsecount = incoming;
	cl.parsecountmod = incoming & CL_UPDATE_MASK;

	frame = &cl.frames[cl.parsecountmod];
	frame->time = cl.mtime[0];

	cmd = &cl.commands[command_ack & CL_UPDATE_MASK];
	parsecounttime = cmd->senttime;
	cmd->receivedtime = (float)realtime;
	frame->receivedtime = realtime;

	CL_ResetFrameStats( frame );

	if ( last_command_ack != -1 )
	{
		if ( cls.spectator )
		{
			ClientDLL_TxferPredictionData( &spectatorState.playerstate, &spectatorState.playerstate,
				&spectatorState.client, &spectatorState.client, spectatorState.weapondata, spectatorState.weapondata );
		}
		else
		{
			pstate = &cl.predicted_frames[( last_incoming_sequence + command_ack - last_command_ack ) & CL_UPDATE_MASK];
			ClientDLL_TxferPredictionData( &frame->playerstate[cl.playernum], &pstate->playerstate,
				&frame->clientdata, &pstate->client, frame->weapondata, pstate->weapondata );
		}
	}

	last_command_ack = command_ack;
	last_incoming_sequence = cls.netchan.incoming_sequence;

	frame->latency = realtime - parsecounttime;
	g_flLatency = frame->latency;

	if ( cls.spectator )
	{
		cl.stats[STAT_HEALTH] = 1;
		return;
	}

	MSG_StartBitReading( &net_message );

	fromframe = NULL;
	if ( MSG_ReadBits( 1 ) )
	{
		fromframe = &cl.frames[MSG_ReadBits( 8 ) & CL_UPDATE_MASK];
		from = (byte*)&fromframe->clientdata;
	}
	else
	{
		from = (byte*)&nullcd;
	}

	DELTA_ParseDelta( from, (byte*)&frame->clientdata, *ppdelta, sizeof( clientdata_t ) );

	Q_memset( &nullwd, 0, sizeof( nullwd ) );

	while ( MSG_ReadBits( 1 ) )
	{
		i = MSG_ReadBits( 6 );
		if ( fromframe )
			from = (byte*)&fromframe->weapondata[i];
		else
			from = (byte*)&nullwd;

		DELTA_ParseDelta( from, (byte*)&frame->weapondata[i], *ppweapon, sizeof( weapon_data_t ) );
	}

	MSG_EndBitReading( &net_message );

	Q_strncpy( cls.physinfo, frame->clientdata.physinfo, sizeof( cls.physinfo ) - 1 );
	cls.physinfo[sizeof( cls.physinfo ) - 1] = 0;

	cl.stats[STAT_HEALTH] = (int)frame->clientdata.health;
	cl.weapons = frame->clientdata.weapons;
	cl.maxspeed = frame->clientdata.maxspeed;
	cl.pushmsec = frame->clientdata.pushmsec;
}

int CL_InterpolateModel( cl_entity_t* e )
{
	int i;
	float t, targettime, dt, frac;
	vec3_t delta, origin, angles;
	position_history_t *ph0, *ph1;

	VectorCopy( e->curstate.origin, e->origin );
	VectorCopy( e->curstate.angles, e->angles );

	if ( cls.timedemo )
		return 1;

	if ( !e->model || ( e->model->name[0] == '*' && !r_bmodelinterp.value ) )
		return 1;

	if ( cl.maxclients == 1 || ( cl.moving && cl.onground == e->index ) )
		return 1;

	targettime = (float)( cl.time - ex_interp.value );
	CL_FindInterpolationUpdates( e, targettime, &ph0, &ph1, NULL );
	if ( !ph0 || !ph1 )
		return 0;

	dt = targettime - ph1->animtime;
	if ( dt < 0.0f )
		return 0;

	if ( ph1->animtime == 0.0f
		 || ( VectorCompare( ph1->origin, vec3_origin ) && !VectorCompare( ph0->origin, vec3_origin ) ) )
	{
		VectorCopy( ph0->origin, e->origin );
		VectorCopy( ph0->angles, e->angles );
		return 0;
	}

	if ( ph0->animtime == ph1->animtime )
	{
		VectorCopy( ph0->origin, e->origin );
		VectorCopy( ph0->angles, e->angles );
		return 1;
	}

	VectorSubtract( ph0->origin, ph1->origin, delta );

	frac = dt / ( ph0->animtime - ph1->animtime );
	if ( frac < 0.0f )
		return 0;
	if ( frac > 1.0f )
		frac = 1.0f;

	VectorMA( ph1->origin, frac, delta, origin );

	for ( i = 0; i < 3; i++ )
	{
		t = ph0->angles[i] - ph1->angles[i];
		if ( t > 180 )
			t -= 360;
		else if ( t < -180 )
			t += 360;

		angles[i] = t * frac + ph1->angles[i];
	}

	COM_NormalizeAngles( angles );

	VectorCopy( origin, e->origin );
	VectorCopy( angles, e->angles );

	return 1;
}

void CL_SetUpPlayerPrediction( qboolean, qboolean bIncludeLocalClient )
{
	int j;
	frame_t* frame;
	predicted_player* pplayer;
	entity_state_t* state;
	cl_entity_t* ent;

	pmove = &g_clmove;

	frame = &cl.frames[cl.parsecountmod];

	for ( j = 0; j < MAX_CLIENTS; j++ )
	{
		pplayer = &predicted_players[j];
		state = &frame->playerstate[j];

		pplayer->active = FALSE;

		if ( state->messagenum != cl.parsecount || !state->modelindex )
			continue;

		if ( ( state->effects & EF_NODRAW ) && !bIncludeLocalClient && j == cl.playernum )
			continue;

		pplayer->movetype = state->movetype;
		pplayer->solid = state->solid;
		pplayer->usehull = state->usehull;
		pplayer->active = TRUE;

		// note that the local player is special, since he moves locally
		// we use his last predicted postition
		if ( j == cl.playernum )
		{
			VectorCopy( state->origin, pplayer->origin );
			VectorCopy( state->angles, pplayer->angles );
		}
		else
		{
			ent = &cl_entities[j + 1];
			CL_ComputePlayerOrigin( ent );
			VectorCopy( ent->origin, pplayer->origin );
			VectorCopy( ent->angles, pplayer->angles );
		}
	}
}

/*
===============
CL_SetSolid

Builds all the pmove physents for the current frame
Note that CL_SetUpPlayerPrediction() must be called first!
pmove must be setup with world and solid entity hulls before calling
(via CL_PredictMove)
===============
*/
void CL_SetSolidPlayers( int playernum )
{
	int j;
	frame_t* frame;
	predicted_player* pplayer;
	entity_state_t* state;
	physent_t* pent;

	if ( !cl_solid_players.value )
		return;

	frame = &cl.frames[cl.parsecountmod];

	for ( j = 0; j < MAX_CLIENTS; j++ )
	{
		pplayer = &predicted_players[j];

		if ( playernum == -1 )
		{
			if ( j != cl.playernum && !pplayer->active )
				continue; // not present this frame
		}
		else if ( !pplayer->active || j == playernum )
		{
			continue;
		}

		if ( !pplayer->solid )
			continue;

		state = &frame->playerstate[j];
		pent = &pmove->physents[pmove->numphysent];

		Q_memset( pent, 0, sizeof( physent_t ) );
		pent->player = 1;
		pent->model = NULL;
		VectorCopy( pplayer->origin, pent->origin );
		VectorCopy( player_mins[pplayer->usehull], pent->mins );
		VectorCopy( player_maxs[pplayer->usehull], pent->maxs );
		snprintf( pent->name, sizeof( pent->name ), "player %i", j );
		pent->solid = pplayer->solid;
		pent->skin = 0;
		pent->info = j + 1;
		pent->movetype = pplayer->movetype;
		pent->blooddecal = 0;
		pent->takedamage = DAMAGE_YES;
		VectorCopy( pplayer->angles, pent->angles );

		pent->iuser1 = state->iuser1;
		pent->iuser2 = state->iuser2;
		pent->iuser3 = state->iuser3;
		pent->iuser4 = state->iuser4;
		pent->fuser1 = state->fuser1;
		pent->fuser2 = state->fuser2;
		pent->fuser3 = state->fuser3;
		pent->fuser4 = state->fuser4;
		VectorCopy( state->vuser1, pent->vuser1 );
		VectorCopy( state->vuser2, pent->vuser2 );
		VectorCopy( state->vuser3, pent->vuser3 );
		VectorCopy( state->vuser4, pent->vuser4 );

		Q_memcpy( pent->blending, state->blending, sizeof( pent->blending ) );
		Q_memcpy( pent->controller, state->controller, sizeof( pent->controller ) );
		pent->frame = state->frame;
		pent->rendermode = state->rendermode;
		pent->sequence = state->sequence;
		pent->team = state->team;
		pent->classnumber = state->playerclass;

		pmove->numphysent++;
	}
}

/*
===============
CL_LinkPacketEntities

===============
*/
void CL_LinkPacketEntities()
{
	int i, j;
	packet_entities_t* pack;
	entity_state_t* s1;
	cl_entity_t* ent;
	dlight_t* dl;
	float animtime;
	qboolean parametric, nointerp;

	pack = &cl.frames[cl.parsecountmod].packet_entities;

	for ( i = 0; i < pack->num_entities; i++ )
	{
		s1 = &pack->entities[i];

		if ( CL_IsPlayerIndex( s1->number ) || !s1->modelindex || ( s1->effects & EF_NODRAW ) )
			continue;

		ent = &cl_entities[s1->number];
		ent->curstate = *s1;

		if ( !ent->model )
		{
			Con_Printf( "Tried to link edict %i without model\n", ent->index );
			continue;
		}

		parametric = ent->curstate.starttime != 0.0f && ent->curstate.impacttime != 0.0f;

		if ( !parametric )
		{
			if ( ent->curstate.animtime == ent->prevstate.animtime
				 && !VectorCompare( ent->curstate.origin, ent->prevstate.origin ) )
				ent->lastmove = (float)( cl.time + 0.2 );

			animtime = ent->curstate.animtime;

			if ( ( ent->curstate.eflags & EFLAG_SLERP ) && animtime != 0.0f && ent->model->type == mod_studio )
			{
				if ( cl.time > ent->lastmove )
				{
					ent->curstate.movetype = MOVETYPE_STEP;
				}
				else
				{
					ent->curstate.movetype = MOVETYPE_NONE;
					R_ResetLatched( ent, TRUE );

					if ( cl_fixmodelinterpolationartifacts.value >= 1.0f )
					{
						ent->curstate.animtime = animtime;
						ent->latched.prevanimtime = animtime;
					}

					VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
					VectorCopy( ent->curstate.angles, ent->latched.prevangles );
				}
			}
		}

		if ( ent->model->name[0] == '*' )
		{
			CL_InterpolateModel( ent );
		}
		else
		{
			nointerp = ent->curstate.movetype == MOVETYPE_NONE || ent->curstate.movetype == MOVETYPE_WALK
					   || ent->curstate.movetype == MOVETYPE_FLY || ent->curstate.movetype == MOVETYPE_STEP;

			if ( ent->curstate.movetype == MOVETYPE_STEP && ( g_bIsCStrike || g_bIsCZero )
				 && cls.netchan.remote_address.type != NA_LOOPBACK )
				nointerp = FALSE;

			if ( parametric || nointerp )
			{
				if ( parametric )
					CL_ParametricMove( ent );

				VectorCopy( ent->curstate.origin, ent->origin );
				VectorCopy( ent->curstate.angles, ent->angles );
			}
			else if ( !CL_InterpolateModel( ent ) )
			{
				continue;
			}
		}

		if ( !( s1->entityType & ENTITY_NORMAL ) )
		{
			CL_LinkCustomEntity( ent, s1 );
			continue;
		}

		if ( cl_numvisedicts == MAX_VISEDICTS )
			return; // object list is full

		if ( ent->curstate.aiment )
			ent->curstate.movetype = MOVETYPE_FOLLOW;

		if ( ent->curstate.effects & EF_NOINTERP )
			R_ResetLatched( ent, FALSE );

		if ( CL_EntityTeleported( ent ) )
		{
			VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
			VectorCopy( ent->curstate.angles, ent->latched.prevangles );
			CL_ResetPositions( ent );
		}

		if ( ent->curstate.effects & EF_BRIGHTFIELD )
			efx.R_EntityParticles( ent );

		if ( ent->index != 1 )
		{
			if ( ent->curstate.effects & EF_BRIGHTLIGHT )
			{
				dl = efx.CL_AllocDlight( ent->index );
				VectorCopy( ent->origin, dl->origin );
				dl->origin[2] += 16;
				dl->color.r = dl->color.g = dl->color.b = 250;
				dl->radius = RandomFloat( 400, 431 );
				dl->die = (float)( cl.time + 0.001 );
			}

			if ( ent->curstate.effects & EF_DIMLIGHT )
			{
				dl = efx.CL_AllocDlight( ent->index );
				VectorCopy( ent->origin, dl->origin );
				dl->color.r = dl->color.g = dl->color.b = 100;
				dl->radius = RandomFloat( 200, 231 );
				dl->die = (float)( cl.time + 0.001 );
			}
		}

		if ( ent->curstate.effects & EF_LIGHT )
		{
#if !defined( SW_RENDERER )
			dl = efx.CL_AllocDlight( s1->number );
			VectorCopy( ent->origin, dl->origin );
			dl->radius = 200;
			dl->color.r = dl->color.g = dl->color.b = 100;
			dl->die = (float)( cl.time + 0.001 );
#endif
			efx.R_RocketFlare( ent->origin );
		}

		CL_AddModelEffects( s1->number, ent, ent->model );

		for ( j = 0; j < 4; j++ )
			VectorCopy( ent->origin, ent->attachment[j] );

		ClientDLL_AddEntity( ET_NORMAL, ent );
	}
}

qboolean CL_EntityTeleported( cl_entity_t* ent )
{
	int i;
	vec3_t delta;

	VectorSubtract( ent->curstate.origin, ent->prevstate.origin, delta );

	for ( i = 0; i < 3; i++ )
	{
		if ( delta[i] > 128 )
			return TRUE;
	}

	return FALSE;
}

void CL_PlayerFlashlight()
{
	cl_entity_t* ent;
	dlight_t* dl;
	pmtrace_t trace;
	vec3_t forward, end;
	float falloff;

	ent = &cl_entities[cl.playernum + 1];

	if ( ( ent->curstate.effects & ( EF_BRIGHTLIGHT | EF_DIMLIGHT ) ) && cl.worldmodel )
	{
		if ( cl.pLight && cl.pLight->key == 1 )
			dl = cl.pLight;
		else
			dl = efx.CL_AllocDlight( 1 );

		if ( ent->curstate.effects & EF_BRIGHTLIGHT )
		{
			dl->color.r = dl->color.g = dl->color.b = 250;
			dl->radius = 400;
			VectorCopy( ent->origin, dl->origin );
			dl->origin[2] += 16;
		}
		else
		{
			AngleVectors( r_playerViewportAngles, forward, NULL, NULL );

			VectorAdd( ent->origin, cl.viewheight, dl->origin );
			VectorMA( dl->origin, FLASHLIGHT_DISTANCE, forward, end );

			pmove->usehull = 2;
			trace = PM_PlayerTrace( dl->origin, end, PM_STUDIO_BOX, -1 );

			if ( trace.ent > 0 && pmove->physents[trace.ent].studiomodel )
			{
				VectorCopy( pmove->physents[trace.ent].origin, dl->origin );
			}
			else
			{
				VectorCopy( trace.endpos, dl->origin );
			}

			falloff = trace.fraction * FLASHLIGHT_DISTANCE;
			if ( falloff < 500 )
				falloff = 1.0f;
			else
				falloff = (float)( 500.0 / falloff );

			falloff *= falloff;
			dl->radius = 80;
			dl->color.r = dl->color.g = dl->color.b = (byte)(int)( falloff * 255 );
		}

		cl.pLight = dl;
		dl->die = (float)( cl.time + 0.2 );
		CL_TouchLight( dl );
	}
	else
	{
		if ( cl.pLight && cl.pLight->key == 1 )
			cl.pLight->die = (float)cl.time;

		cl.pLight = NULL;
	}

	if ( cl_entities[cl.viewentity].curstate.effects & EF_MUZZLEFLASH )
		cl.viewent.curstate.effects |= EF_MUZZLEFLASH;
}

/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
static float CL_LerpPoint()
{
	float dt;

	if ( cl.mtime[0] - cl.mtime[1] == 0.0 || cls.timedemo )
	{
		dt = (float)( cl.time - cl.oldtime );
		cl.time = cl.mtime[0];
		if ( cls.demoplayback )
			cl.oldtime = cl.mtime[0] - dt;
		return 1.0f;
	}

	if ( !( ex_interp.value > 0.001 ) )
		return 1.0f;

	return (float)( ( cl.time - cl.mtime[0] ) / ex_interp.value );
}

static void CL_MoveAiments()
{
	int i;
	cl_entity_t* ent;

	for ( i = 0; i < cl_numvisedicts; i++ )
	{
		ent = cl_visedicts[i];
		if ( ent->curstate.aiment && ent->curstate.movetype == MOVETYPE_FOLLOW )
			VectorCopy( cl_entities[ent->curstate.aiment].origin, ent->origin );
	}
}

void CL_EmitEntities()
{
	mleaf_t* leaf;

	cl_numvisedicts = 0;
	cl_numbeamentities = 0;

	if ( cls.state != ca_active || !cl.validsequence || cl.frames[cl.parsecountmod].invalid )
		return;

	// Compute last interpolation amount
	cl.commands[( cls.netchan.outgoing_sequence - 1 ) & CL_UPDATE_MASK].frame_lerp = CL_LerpPoint();

	if ( cls.spectator )
	{
		leaf = Mod_PointInLeaf( cl.simorg, cl.worldmodel );
		PVSMark( cl.worldmodel, Mod_LeafPVS( leaf, cl.worldmodel ) );
	}

	CL_LinkPlayers();
	CL_LinkPacketEntities();
	CL_MoveAiments();
	ClientDLL_CreateEntities();
	CL_TempEntUpdate();

	if ( cls.spectator )
	{
		r_oldviewleaf = NULL;
		R_MarkLeaves();
	}

	CL_FireEvents();
}

void CL_Particle( float* origin, int color, float life, int zpos, int zvel )
{
	particle_t* p;

	p = free_particles;
	if ( !p )
		return;

	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;

	p->color = (short)color;
#if defined( SW_RENDERER )
	p->packedColor = hlRGB( host_basepal, (short)color );
#else
	p->packedColor = 0;
#endif
	p->type = pt_static;
	p->die = (float)( life + cl.time );

	VectorCopy( vec3_origin, p->vel );
	p->vel[2] += (float)zvel;

	VectorCopy( origin, p->org );
	p->org[2] += (float)zpos;
}

qboolean CL_CompareTimestamps( float t1, float t2 )
{
	int iTime1 = (int)( 1000.0 * t1 );
	int iTime2 = (int)( 1000.0 * t2 );

	return (unsigned int)( iTime1 - iTime2 + 1 ) <= 2;
}

void CL_ProcessEntityUpdate( cl_entity_t* ent )
{
	int i;
	float t;

	ent->index = ent->curstate.number;
	ent->model = CL_GetModelByIndex( ent->curstate.modelindex );
	COM_NormalizeAngles( ent->curstate.angles );

	if ( ( ent->model && ent->model->name[0] == '*' )
		 || ( ent->curstate.movetype != MOVETYPE_WALK && ent->curstate.movetype != MOVETYPE_STEP
			  && ent->curstate.movetype != MOVETYPE_FLY && ent->curstate.movetype != MOVETYPE_NONE
			  && ent->curstate.starttime == 0.0f && ent->curstate.impacttime == 0.0f ) )
	{
		t = ent->curstate.msg_time;
		ent->curstate.animtime = ent->curstate.msg_time;
	}
	else
	{
		t = ent->curstate.animtime;
	}

	if ( !CL_CompareTimestamps( t, ent->prevstate.animtime ) || ent->curstate.movetype == MOVETYPE_NONE )
	{
		R_UpdateLatchedVars( ent );
		CL_UpdatePositions( ent );
	}

	if ( ent->player )
		ent->curstate.angles[PITCH] = (float)( ent->curstate.angles[PITCH] / -3.0 );

	VectorCopy( ent->curstate.origin, ent->origin );
	CL_ParametricMove( ent );
	VectorCopy( ent->curstate.angles, ent->angles );

	for ( i = 0; i < 4; i++ )
		VectorCopy( ent->curstate.origin, ent->attachment[i] );
}

void CL_ClearPacket( packet_entities_t* pack )
{
	if ( pack->entities )
		Mem_Free( pack->entities );

	pack->entities = NULL;
	pack->num_entities = 0;
}

void CL_ResetLatchedState( int pnum, packet_entities_t* pack, cl_entity_t* ent )
{
	if ( !( pack->flags[pnum >> 3] & ( 1 << ( pnum & 7 ) ) ) )
		return;

	VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
	VectorCopy( ent->curstate.angles, ent->latched.prevangles );

	R_ResetLatched( ent, TRUE );
	CL_ResetPositions( ent );

	if ( ent->curstate.starttime != 0.0f && ent->curstate.impacttime != 0.0f )
		ent->lastmove = (float)cl.time;
}

void CL_ProcessPlayerState( int playerindex, entity_state_t* state )
{
	entity_state_t* dst;

	dst = &cl.frames[cl.parsecountmod].playerstate[playerindex];
	dst->number = state->number;
	dst->msg_time = (float)cl.mtime[0];
	dst->messagenum = cl.parsecount;

	ClientDLL_ProcessPlayerState( dst, state );
}

void CL_SetPacketFlag( packet_entities_t* pack, int index, int flag )
{
	if ( flag )
	{
#if defined( _WIN32 )
		ASSERT_AT( index < MAX_PACKET_ENTITIES, 680 );
#endif
		pack->flags[index >> 3] |= (byte)( 1 << ( index & 7 ) );
	}
	else
		pack->flags[index >> 3] &= (byte)~( 1 << ( index & 7 ) );
}

void CL_ResetFrameStats( frame_t* frame )
{
	frame->clientbytes = 0;
	frame->packetentitybytes = 0;
	frame->tentitybytes = 0;
	frame->playerinfobytes = 0;
	frame->soundbytes = 0;
	frame->usrbytes = 0;
	frame->eventbytes = 0;
	frame->voicebytes = 0;
	frame->msgbytes = 0;
}

void CL_PrintEntity( cl_entity_t* )
{
}

void CL_TouchLight( dlight_t* dl )
{
	int i;

	i = dl - cl_dlights;
	if ( (unsigned int)i < MAX_DLIGHTS )
		r_dlightchanged |= 1 << i;
}

void CL_AddModelEffects( int keynum, cl_entity_t* ent, model_s* model )
{
	int flags;
	vec3_t oldorg;
	dlight_t* dl;

	if ( !model )
		return;

	VectorCopy( ent->latched.prevorigin, oldorg );

	flags = ent->model->flags;

	if ( flags & EF_GIB )
	{
		efx.R_RocketTrail( oldorg, ent->origin, 2 );
	}
	else if ( flags & EF_ZOMGIB )
	{
		efx.R_RocketTrail( oldorg, ent->origin, 4 );
	}
	else if ( flags & EF_TRACER )
	{
		efx.R_RocketTrail( oldorg, ent->origin, 3 );
	}
	else if ( flags & EF_TRACER2 )
	{
		efx.R_RocketTrail( oldorg, ent->origin, 5 );
	}
	else if ( flags & EF_ROCKET )
	{
		efx.R_RocketTrail( oldorg, ent->origin, 0 );

		dl = efx.CL_AllocDlight( keynum );
		VectorCopy( ent->origin, dl->origin );
		dl->radius = 200;
		dl->color.r = dl->color.g = dl->color.b = 200;
		dl->die = (float)( cl.time + 0.01 );
	}
	else if ( flags & EF_GRENADE )
	{
		efx.R_RocketTrail( oldorg, ent->origin, 1 );
	}
	else if ( flags & EF_TRACER3 )
	{
		efx.R_RocketTrail( oldorg, ent->origin, 6 );
	}
}

void CL_LinkCustomEntity( cl_entity_t* ent, entity_state_t* state )
{
	ent->curstate.movetype = state->modelindex;

	if ( ent->model->type != mod_sprite )
		Sys_Error( "Bad model on beam ( not sprite ) " );

	ent->prevstate = ent->curstate;
	ent->latched.prevsequence = ent->curstate.sequence;
	VectorCopy( ent->origin, ent->latched.prevorigin );
	VectorCopy( ent->angles, ent->latched.prevangles );

	if ( !ClientDLL_AddEntity( ET_BEAM, &cl_entities[ent->index] ) )
		Con_DPrintf( "Overflow beam entity list!\n" );
}

/*
=============
CL_LinkPlayers

Create visible entities in the correct position
for all current players
=============
*/
void CL_LinkPlayers()
{
	int i, j;
	frame_t* frame;
	entity_state_t* state;
	cl_entity_t* ent;
	dlight_t* dl;

	frame = &cl.frames[cl.parsecountmod];

	for ( j = 0; j < MAX_CLIENTS; j++ )
	{
		state = &frame->playerstate[j];

		if ( state->messagenum != cl.parsecount )
			continue; // not present this frame

		if ( !ClientDLL_IsThirdPerson() && cl.viewentity - 1 == j && !chase_active.value )
			continue;

		if ( !state->modelindex || ( state->effects & EF_NODRAW ) )
			continue;

		if ( cl_numvisedicts >= MAX_VISEDICTS )
			break; // object list is full

		ent = &cl_entities[j + 1];
		ent->index = j + 1;

		if ( j == cl.playernum )
		{
			VectorCopy( state->origin, ent->origin );
			VectorCopy( state->origin, ent->curstate.origin );
			VectorCopy( state->origin, ent->prevstate.origin );
			VectorCopy( ent->curstate.angles, ent->angles );
		}

		if ( ent->curstate.effects & EF_NOINTERP )
			R_ResetLatched( ent, FALSE );

		if ( CL_EntityTeleported( ent ) )
		{
			VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
			VectorCopy( ent->curstate.angles, ent->latched.prevangles );
			CL_ResetPositions( ent );
		}

		if ( j == cl.playernum )
		{
			VectorCopy( cl.simorg, ent->origin );
		}
		else
		{
			VectorCopy( ent->curstate.origin, ent->origin );
			VectorCopy( ent->curstate.angles, ent->angles );
			CL_ComputePlayerOrigin( ent );
		}

		if ( cl.viewentity - 1 != j )
		{
			if ( ent->curstate.effects & EF_BRIGHTLIGHT )
			{
				dl = efx.CL_AllocDlight( 4 );
				VectorCopy( ent->origin, dl->origin );
				dl->origin[2] += 16;
				dl->color.r = dl->color.g = dl->color.b = 250;
				dl->radius = RandomFloat( 400, 431 );
				dl->die = (float)( cl.time + 0.001 );
			}

			if ( ent->curstate.effects & EF_DIMLIGHT )
			{
				dl = efx.CL_AllocDlight( 4 );
				VectorCopy( ent->origin, dl->origin );
				dl->color.r = dl->color.g = dl->color.b = 100;
				dl->radius = RandomFloat( 200, 231 );
				dl->die = (float)( cl.time + 0.001 );
			}
		}

		for ( i = 0; i < 4; i++ )
			VectorCopy( ent->origin, ent->attachment[i] );

		ClientDLL_AddEntity( ET_PLAYER, ent );
	}

	CL_PlayerFlashlight();
}
