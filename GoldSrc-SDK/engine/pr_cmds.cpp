#include "info.h"
#include "cmodel.h"
#include "cvar.h"
#include "server.h"
#include "common.h"
#include "cmd.h"
#include "conprint.h"
#include "sys.h"
#include "host.h"
#include "engine_edict.h"
#include "protocol.h"
#include "engine_apiproxy.h"
#include "client.h"
#include "pr_dlls.h"
#include "sound.h"
#include "delta.h"
#include "net.h"
#include "isteamgameserver_c_api.h"
#include "../common/event_flags.h"
#include "inst_baseline.h"
#include "gl_model.h"
#include "render.h"
#include "world.h"
#include "pm_defs.h"
#include "../common/const.h"
#include "eiface.h"

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\pr_cmds.c" )

#define svc_setview 5
#define svc_print 8
#define svc_lightstyle 12
#define svc_spawnstatic 20
#define svc_event_reliable 21
#define svc_temp_entity 23
#define svc_centerprint 26
#define svc_spawnstaticsound 29
#define svc_crosshairangle 47
#define svc_soundfade 48
#define svc_sendcvarvalue 57
#define svc_sendcvarvalue2 58

#define SND_SPAWNING ( 1 << 8 ) // we're spawning, used in some cases for ambients

#define MAX_USER_MSG_DATA 192
#define MAX_MAP_LEAFS 8192
#define LOADFILE_HEAP 5
#define STUFFCMD_BUFFER_BYTES 1024
#define AUTH_ID_BUFFERS 5
#define AUTH_ID_BYTES 64
#define RELIABLE_EVENT_BUFFER_BYTES 1024
#define EVENT_INDEX_BITS 10
#define EVENT_DELAY_BITS 16
#define EVENT_MULTICAST_PAS 4

#define GROUP_OP_AND 0
#define GROUP_OP_NAND 1

#define AUTH_IDTYPE_STEAM 1
#define AUTH_IDTYPE_LOCAL 3

#define IA 16807
#define IM 2147483647
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV ( 1 + ( IM - 1 ) / NTAB )

#define AM ( 1.0 / IM )
#define EPS 1.2e-7
#define RNMX ( 1.0 - EPS )

#define MAX_RANDOM_RANGE 0x7FFFFFFFUL

trace_t SV_Trace_Toss( edict_t* ent, edict_t* ignore );
trace_t SV_ClipMoveToEntity( edict_t* ent, const float* start, const float* mins, const float* maxs, const float* end );
qboolean SV_movetest( edict_t* ent, float* move, qboolean relink );
qboolean SV_movestep( edict_t* ent, float* move, qboolean relink );
qboolean SV_CheckBottom( edict_t* ent );
msurface_t* SurfaceAtPoint( model_t* pModel, mnode_t* node, vec_t* start, vec_t* end );
void R_MarkLeaves();
void SV_SkipUpdates();
void Host_ClientCommands( char* fmt, ... );
void SV_RunCmd( usercmd_t* ucmd, int random_seed );
void SV_PreRunCmd();
int SV_PointLeafnum( vec_t* p );
qboolean SV_ValidClientMulticast( client_t* client, int soundLeaf, int to );

extern cvar_t sv_aim;
extern vec3_t player_mins[MAX_PLAYER_HULLS];
extern vec3_t player_maxs[MAX_PLAYER_HULLS];
extern mleaf_t* r_oldviewleaf;
extern int g_userid;
extern playermove_t g_svmove;
extern delta_t* g_peventdelta;

int PF_checkbottom_I( edict_t* pEdict )
{
	return SV_CheckBottom( pEdict );
}

edict_t* FindEntityInSphere( edict_t* pEdictStartSearchAfter, const float* org, float rad )
{
	edict_t* ent;
	float eorg;
	int e, i, j;
	float distSquared, radSquared;

	if ( pEdictStartSearchAfter )
		e = NUM_FOR_EDICT( pEdictStartSearchAfter );
	else
		e = 0;

	radSquared = rad * rad;

	for ( i = e + 1; i < sv.num_edicts; i++ )
	{
		ent = &sv.edicts[i];
		if ( ent->free || !ent->v.classname )
			continue;

		if ( i <= svs.maxclients && !svs.clients[i - 1].active )
			continue;

		distSquared = 0;
		for ( j = 0; j < 3 && distSquared <= radSquared; j++ )
		{
			if ( org[j] < ent->v.absmin[j] )
				eorg = org[j] - ent->v.absmin[j];
			else if ( org[j] > ent->v.absmax[j] )
				eorg = org[j] - ent->v.absmax[j];
			else
				eorg = 0;

			distSquared += eorg * eorg;
		}

		if ( !( distSquared > radSquared ) )
			return ent;
	}

	return sv.edicts;
}

void PF_aim_I( edict_t* ent, float, float* rgflReturn )
{
	edict_t* check;
	vec3_t start, dir, end, bestdir;
	int i, j;
	trace_t tr;
	float dist, bestdist;

	if ( !ent || ( ent->v.flags & FL_FAKECLIENT ) )
	{
		VectorCopy( gGlobalVariables.v_forward, rgflReturn );
		return;
	}

	// try sending a trace straight
	VectorCopy( gGlobalVariables.v_forward, dir );
	VectorAdd( ent->v.origin, ent->v.view_ofs, start );
	VectorMA( start, 2048, dir, end );
	tr = SV_Move( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, FALSE );
	if ( tr.ent && tr.ent->v.takedamage == DAMAGE_AIM && ( ent->v.team <= 0 || ent->v.team != tr.ent->v.team ) )
	{
		VectorCopy( gGlobalVariables.v_forward, rgflReturn );
		return;
	}

	// try all possible entities
	VectorCopy( dir, bestdir );
	bestdist = sv_allow_autoaim.value ? sv_aim.value : 0;

	for ( i = 1; i < sv.num_edicts; i++ )
	{
		check = &sv.edicts[i];
		if ( check->v.takedamage != DAMAGE_AIM || ( check->v.flags & FL_FAKECLIENT ) || check == ent )
			continue;
		if ( ent->v.team > 0 && ent->v.team == check->v.team )
			continue; // don't aim at teammate

		for ( j = 0; j < 3; j++ )
			end[j] = (float)( ( check->v.maxs[j] + check->v.mins[j] ) * 0.75 + check->v.origin[j] + ent->v.view_ofs[j] * 0.0 );
		VectorSubtract( end, start, dir );
		VectorNormalize( dir );
		dist = DotProduct( dir, gGlobalVariables.v_forward );
		if ( dist < bestdist )
			continue; // to far to turn

		tr = SV_Move( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, FALSE );
		if ( tr.ent == check )
		{
			// can shoot at this one
			bestdist = dist;
			VectorCopy( dir, bestdir );
		}
	}

	VectorCopy( bestdir, rgflReturn );
}

void SV_SetGlobalTrace( trace_t* ptrace )
{
	gGlobalVariables.trace_allsolid = (float)ptrace->allsolid;
	gGlobalVariables.trace_startsolid = (float)ptrace->startsolid;
	gGlobalVariables.trace_fraction = ptrace->fraction;
	gGlobalVariables.trace_inwater = (float)ptrace->inwater;
	gGlobalVariables.trace_inopen = (float)ptrace->inopen;
	VectorCopy( ptrace->endpos, gGlobalVariables.trace_endpos );
	VectorCopy( ptrace->plane.normal, gGlobalVariables.trace_plane_normal );
	gGlobalVariables.trace_plane_dist = ptrace->plane.dist;
	gGlobalVariables.trace_ent = ptrace->ent ? ptrace->ent : sv.edicts;
	gGlobalVariables.trace_hitgroup = ptrace->hitgroup;
}

void PF_traceline_Shared( const float* v1, const float* v2, int nomonsters, edict_t* ent )
{
	trace_t trace;

	trace = SV_Move( v1, vec3_origin, vec3_origin, v2, nomonsters, ent, FALSE );
	gGlobalVariables.trace_flags = 0;
	SV_SetGlobalTrace( &trace );
}

void PF_traceline_DLL( const float* v1, const float* v2, int fNoMonsters, edict_t* pentToSkip, TraceResult* ptr )
{
	PF_traceline_Shared( v1, v2, fNoMonsters, pentToSkip ? pentToSkip : sv.edicts );

	ptr->fAllSolid = (int)gGlobalVariables.trace_allsolid;
	ptr->fStartSolid = (int)gGlobalVariables.trace_startsolid;
	ptr->fInOpen = (int)gGlobalVariables.trace_inopen;
	ptr->fInWater = (int)gGlobalVariables.trace_inwater;
	ptr->flFraction = gGlobalVariables.trace_fraction;
	ptr->flPlaneDist = gGlobalVariables.trace_plane_dist;
	ptr->pHit = gGlobalVariables.trace_ent;
	VectorCopy( gGlobalVariables.trace_endpos, ptr->vecEndPos );
	VectorCopy( gGlobalVariables.trace_plane_normal, ptr->vecPlaneNormal );
	ptr->iHitgroup = gGlobalVariables.trace_hitgroup;
}

void PF_traceline_I( const float* v1, const float* v2, int fNoMonsters, edict_t* pentToSkip, TraceResult* ptr )
{
	PF_traceline_DLL( v1, v2, fNoMonsters, pentToSkip, ptr );
}

void PF_TraceToss_Shared( edict_t* ent, edict_t* ignore )
{
	trace_t trace;

	trace = SV_Trace_Toss( ent, ignore );
	SV_SetGlobalTrace( &trace );
}

void PF_TraceToss_DLL( edict_t* pent, edict_t* pentToIgnore, TraceResult* ptr )
{
	PF_TraceToss_Shared( pent, pentToIgnore ? pentToIgnore : sv.edicts );

	ptr->fAllSolid = (int)gGlobalVariables.trace_allsolid;
	ptr->fStartSolid = (int)gGlobalVariables.trace_startsolid;
	ptr->fInOpen = (int)gGlobalVariables.trace_inopen;
	ptr->fInWater = (int)gGlobalVariables.trace_inwater;
	ptr->flFraction = gGlobalVariables.trace_fraction;
	ptr->flPlaneDist = gGlobalVariables.trace_plane_dist;
	ptr->pHit = gGlobalVariables.trace_ent;
	VectorCopy( gGlobalVariables.trace_endpos, ptr->vecEndPos );
	VectorCopy( gGlobalVariables.trace_plane_normal, ptr->vecPlaneNormal );
	ptr->iHitgroup = gGlobalVariables.trace_hitgroup;
}

vec3_t gHullMins[4] =
{
	{ 0.0f, 0.0f, 0.0f },
	{ -16.0f, -16.0f, -36.0f },
	{ -32.0f, -32.0f, -32.0f },
	{ -16.0f, -16.0f, -18.0f },
};

vec3_t gHullMaxs[4] =
{
	{ 0.0f, 0.0f, 0.0f },
	{ 16.0f, 16.0f, 36.0f },
	{ 32.0f, 32.0f, 32.0f },
	{ 16.0f, 16.0f, 18.0f },
};

void TraceHull( const float* v1, const float* v2, int fNoMonsters, int hullNumber, edict_t* pentToSkip, TraceResult* ptr )
{
	trace_t trace;

	if ( hullNumber < 0 || hullNumber > 3 )
		hullNumber = 0;

	trace = SV_Move( v1, gHullMins[hullNumber], gHullMaxs[hullNumber], v2, fNoMonsters, pentToSkip, FALSE );

	ptr->fAllSolid = trace.allsolid;
	ptr->fStartSolid = trace.startsolid;
	ptr->fInOpen = trace.inopen;
	ptr->fInWater = trace.inwater;
	ptr->flFraction = trace.fraction;
	ptr->flPlaneDist = trace.plane.dist;
	ptr->pHit = trace.ent;
	VectorCopy( trace.endpos, ptr->vecEndPos );
	VectorCopy( trace.plane.normal, ptr->vecPlaneNormal );
	ptr->iHitgroup = trace.hitgroup;
}

void PF_tracehull_I( const float* v1, const float* v2, int fNoMonsters, int hullNumber, edict_t* pentToSkip, TraceResult* ptr )
{
	trace_t trace;

	if ( hullNumber < 0 || hullNumber >= MAX_PLAYER_HULLS )
		hullNumber = 0;

	trace = SV_Move( v1, gHullMins[hullNumber], gHullMaxs[hullNumber], v2, fNoMonsters, pentToSkip, FALSE );

	ptr->fAllSolid = trace.allsolid;
	ptr->fStartSolid = trace.startsolid;
	ptr->fInOpen = trace.inopen;
	ptr->fInWater = trace.inwater;
	ptr->flFraction = trace.fraction;
	ptr->flPlaneDist = trace.plane.dist;
	ptr->pHit = trace.ent;
	VectorCopy( trace.endpos, ptr->vecEndPos );
	VectorCopy( trace.plane.normal, ptr->vecPlaneNormal );
	ptr->iHitgroup = trace.hitgroup;
}

int PF_walkmove_I( edict_t* ent, float yaw, float dist, int iMode )
{
	vec3_t move;

	if ( !( ent->v.flags & ( FL_ONGROUND | FL_FLY | FL_SWIM ) ) )
		return 0;

	yaw = (float)( yaw * M_PI * 2 / 360 );
	move[0] = (float)( cos( (double)yaw ) * dist );
	move[1] = (float)( sin( (double)yaw ) * dist );
	move[2] = 0;

	if ( iMode == WALKMOVE_WORLDONLY )
		return SV_movetest( ent, move, TRUE );

	return SV_movestep( ent, move, iMode != WALKMOVE_CHECKONLY );
}

int PF_droptofloor_I( edict_t* ent )
{
	vec3_t end;
	trace_t trace;
	qboolean monsterClip;

	monsterClip = ( ent->v.flags & FL_MONSTERCLIP ) ? TRUE : FALSE;

	VectorCopy( ent->v.origin, end );
	end[2] -= 256;

	trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent, monsterClip );

	if ( trace.allsolid )
		return -1;

	if ( trace.fraction == 1 )
		return 0;

	VectorCopy( trace.endpos, ent->v.origin );
	SV_LinkEdict( ent, FALSE );
	ent->v.flags |= FL_ONGROUND;
	ent->v.groundentity = trace.ent;
	return 1;
}

int PF_pointcontents_I( const float* rgflVector )
{
	return SV_PointContents( rgflVector );
}

const char* TraceTexture( edict_t* pTextureEntity, const float* v1, const float* v2 )
{
	model_t* pmodel;
	msurface_t* psurf;
	int firstnode;
	vec3_t start, end;

	if ( pTextureEntity )
	{
		vec3_t offset;
		hull_t* phull;
		int modelindex;

		modelindex = pTextureEntity->v.modelindex;
		if ( modelindex < 0 || modelindex >= MAX_MODELS || !sv.model_precache[modelindex] )
			return NULL;

		pmodel = sv.models[modelindex];
		if ( !pmodel || pmodel->type != mod_brush )
			return NULL;

		phull = SV_HullForBsp( pTextureEntity, vec3_origin, vec3_origin, offset );
		firstnode = phull->firstclipnode;
		VectorSubtract( v1, offset, start );
		VectorSubtract( v2, offset, end );

		if ( pTextureEntity->v.angles[0] != 0 || pTextureEntity->v.angles[1] != 0 || pTextureEntity->v.angles[2] != 0 )
		{
			vec3_t forward, right, up;
			vec3_t temp;

			AngleVectors( pTextureEntity->v.angles, forward, right, up );

			VectorCopy( start, temp );
			start[0] = DotProduct( temp, forward );
			start[1] = -DotProduct( temp, right );
			start[2] = DotProduct( temp, up );

			VectorCopy( end, temp );
			end[0] = DotProduct( temp, forward );
			end[1] = -DotProduct( temp, right );
			end[2] = DotProduct( temp, up );
		}
	}
	else
	{
		pmodel = sv.worldmodel;
		firstnode = 0;
		VectorCopy( v1, start );
		VectorCopy( v2, end );
	}

	mnode_t* node = Mod_DrawingHullNode( pmodel, firstnode );
	if ( !node )
		return NULL;

	psurf = SurfaceAtPoint( pmodel, node, start, end );
	return psurf ? psurf->texinfo->texture->name : NULL;
}

int TraceMonsterHull( edict_t* pEdict, const float* v1, const float* v2, int fNoMonsters, edict_t* pentToSkip, TraceResult* ptr )
{
	trace_t trace;
	qboolean monsterClip;

	monsterClip = ( pEdict->v.flags & FL_MONSTERCLIP ) ? TRUE : FALSE;
	trace = SV_Move( v1, pEdict->v.mins, pEdict->v.maxs, v2, fNoMonsters, pentToSkip, monsterClip );

	if ( ptr )
	{
		ptr->fAllSolid = trace.allsolid;
		ptr->fStartSolid = trace.startsolid;
		ptr->fInOpen = trace.inopen;
		ptr->fInWater = trace.inwater;
		ptr->flFraction = trace.fraction;
		ptr->flPlaneDist = trace.plane.dist;
		ptr->pHit = trace.ent;
		VectorCopy( trace.endpos, ptr->vecEndPos );
		VectorCopy( trace.plane.normal, ptr->vecPlaneNormal );
		ptr->iHitgroup = trace.hitgroup;
	}

	if ( trace.allsolid || trace.fraction != 1 )
		return TRUE;

	return FALSE;
}

void TraceModel( const float* v1, const float* v2, int hullNumber, edict_t* pent, TraceResult* ptr )
{
	trace_t trace;
	int oldSolid = 0, oldMovetype = 0;
	model_t* pmodel;

	if ( hullNumber < 0 || hullNumber >= MAX_PLAYER_HULLS )
		hullNumber = 0;

	pmodel = sv.models[pent->v.modelindex];
	if ( pmodel && pmodel->type == mod_brush )
	{
		oldSolid = pent->v.solid;
		oldMovetype = pent->v.movetype;
		pent->v.solid = SOLID_BSP;
		pent->v.movetype = MOVETYPE_PUSH;
	}

	trace = SV_ClipMoveToEntity( pent, v1, gHullMins[hullNumber], gHullMaxs[hullNumber], v2 );

	if ( pmodel && pmodel->type == mod_brush )
	{
		pent->v.solid = oldSolid;
		pent->v.movetype = oldMovetype;
	}

	ptr->fAllSolid = trace.allsolid;
	ptr->fStartSolid = trace.startsolid;
	ptr->fInOpen = trace.inopen;
	ptr->fInWater = trace.inwater;
	ptr->flFraction = trace.fraction;
	ptr->flPlaneDist = trace.plane.dist;
	ptr->pHit = trace.ent;
	VectorCopy( trace.endpos, ptr->vecEndPos );
	VectorCopy( trace.plane.normal, ptr->vecPlaneNormal );
	ptr->iHitgroup = trace.hitgroup;
}

void TraceSphere( const float*, const float*, int, float, edict_t*, TraceResult* )
{
	Sys_Error( "TraceSphere not yet implemented!\n" );
}

void PF_SetClientKeyValue_I( int clientIndex, char* infobuffer, char* key, char* value )
{
	client_t* client;

	if ( infobuffer == localinfo || infobuffer == Info_Serverinfo() || clientIndex < 1 || clientIndex > svs.maxclients )
		return;

	if ( !Q_strcmp( Info_ValueForKey( infobuffer, key ), value ) )
		return;

	Info_SetValueForStarKey( infobuffer, key, value, MAX_INFO_STRING );

	client = &svs.clients[clientIndex - 1];
	client->sendinfo = TRUE;
	client->sendinfo_time = 0;
}

int PF_IsMapValid_I( char* mapname )
{
	char cBuf[260];

	if ( !mapname || !mapname[0] )
		return 0;

	snprintf( cBuf, sizeof( cBuf ), "maps/%.32s.bsp", mapname );
	return FS_FileExists( cBuf );
}

int PF_GetPlayerUserId( edict_t* e )
{
	client_t* client;
	int i;

	if ( !sv.active || !e )
		return -1;

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( client->edict == e )
			return client->userid;
	}

	return -1;
}

unsigned int PF_GetPlayerWONId( edict_t* )
{
	return UINT_MAX;
}

const char* PF_GetPhysicsKeyValue( const edict_t* pClient, const char* key )
{
	int entnum;

	entnum = NUM_FOR_EDICT( pClient );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to PF_GetPhysicsKeyValue a non-client\n" );
		return com_empty_string;
	}

	return Info_ValueForKey( svs.clients[entnum - 1].physinfo, key );
}

void PF_SetPhysicsKeyValue( const edict_t* pClient, const char* key, const char* value )
{
	int entnum;

	entnum = NUM_FOR_EDICT( pClient );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to PF_SetPhysicsKeyValue a non-client\n" );
		return;
	}

	Info_SetValueForKey( svs.clients[entnum - 1].physinfo, key, value, MAX_INFO_STRING );
}

const char* PF_GetPhysicsInfoString( const edict_t* pClient )
{
	int entnum;

	entnum = NUM_FOR_EDICT( pClient );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to PF_GetPhysicsInfoString a non-client\n" );
		return com_empty_string;
	}

	return svs.clients[entnum - 1].physinfo;
}

mnode_t* PVSNode( mnode_t* node, float* emins, float* emaxs )
{
	mplane_t* splitplane;
	int sides;
	mnode_t* splitNode;

	if ( node->visframe != r_visframecount )
		return NULL;

	if ( node->contents < 0 )
		return node->contents != CONTENTS_SOLID ? node : NULL;

	splitplane = node->plane;
	if ( splitplane->type < 3 )
	{
		if ( splitplane->dist <= emins[splitplane->type] )
			sides = 1;
		else if ( splitplane->dist >= emaxs[splitplane->type] )
			sides = 2;
		else
			sides = 3;
	}
	else
	{
		sides = BoxOnPlaneSide( emins, emaxs, splitplane );
	}

	if ( sides & 1 )
	{
		splitNode = PVSNode( node->children[0], emins, emaxs );
		if ( splitNode )
			return splitNode;
	}

	if ( sides & 2 )
		return PVSNode( node->children[1], emins, emaxs );

	return NULL;
}

int g_groupmask;
int g_groupop;

void PF_SetGroupMask( int mask, int op )
{
	g_groupmask = mask;
	g_groupop = op;
}

int PF_CreateInstancedBaseline( int classname, entity_state_t* baseline )
{
	int i;

	i = sv.instance_baselines->number;
	if ( i >= MAX_EXTRA_BASELINES - 1 )
		return 0;

	sv.instance_baselines->classname[i] = classname;
	sv.instance_baselines->number++;
	memcpy( &sv.instance_baselines->baseline[i], baseline, sizeof( entity_state_t ) );

	return sv.instance_baselines->number;
}

static byte gMsgData[512];
static sizebuf_t gMsgBuffer = { "MessageBegin/End", 0, gMsgData, sizeof( gMsgData ), 0 };
static qboolean gMsgStarted;
static edict_t* gMsgEntity;
static int gMsgDest;
static int gMsgType;
static vec3_t gMsgOrigin;

int PF_IsDedicatedServer()
{
	return g_bIsDedicatedServer;
}

char* PF_InfoKeyValue_I( char* infobuffer, char* key )
{
	return (char*)Info_ValueForKey( infobuffer, key );
}

void PF_RemoveKey_I( char* s, const char* key )
{
	Info_RemoveKey( s, key );
}

void PF_Cvar_DirectSet( cvar_t* var, char* value )
{
	Cvar_DirectSet( var, value );
}

qboolean ValidCmd( const char* pCmd )
{
	int len;

	len = Q_strlen( pCmd );
	return len && ( pCmd[len - 1] == ';' || pCmd[len - 1] == '\n' );
}

void PF_localcmd_I( char* str )
{
	if ( !ValidCmd( str ) )
	{
		Con_Printf( "Error, bad server command %s\n", str );
		return;
	}

	Cbuf_AddText( str );
}

void PF_localexec_I()
{
	Cbuf_Execute();
}

void PF_MessageBegin_I( int msg_dest, int msg_type, const float* pOrigin, edict_t* ed )
{
	if ( msg_dest == MSG_ONE || msg_dest == MSG_ONE_UNRELIABLE )
	{
		if ( !ed )
			Sys_Error( "MSG_ONE or MSG_ONE_UNRELIABLE with no target entity\n" );
	}
	else if ( ed )
	{
		Sys_Error( "Invalid message;  cannot use broadcast message with a target entity" );
	}

	if ( gMsgStarted )
		Sys_Error( "New message started when msg '%d' has not been sent yet", gMsgType );

	if ( msg_type == 0 )
		Sys_Error( "Tried to create a message with a bogus message type ( 0 )" );

	gMsgType = msg_type;
	gMsgStarted = TRUE;
	gMsgEntity = ed;
	gMsgDest = msg_dest;

	if ( msg_dest == MSG_PVS || msg_dest == MSG_PAS )
	{
		if ( pOrigin )
			VectorCopy( pOrigin, gMsgOrigin );

		Host_IsSinglePlayerGame();
	}

	gMsgBuffer.cursize = 0;
	gMsgBuffer.flags = FSB_ALLOWOVERFLOW;
}

void PF_WriteByte_I( int iValue )
{
	if ( !gMsgStarted )
		Sys_Error( "WRITE_BYTE called with no active message\n" );

	MSG_WriteByte( &gMsgBuffer, iValue );
}

void PF_WriteChar_I( int iValue )
{
	if ( !gMsgStarted )
		Sys_Error( "WRITE_CHAR called with no active message\n" );

	MSG_WriteByte( &gMsgBuffer, iValue );
}

void PF_WriteShort_I( int iValue )
{
	if ( !gMsgStarted )
		Sys_Error( "WRITE_SHORT called with no active message\n" );

	MSG_WriteShort( &gMsgBuffer, iValue );
}

void PF_WriteLong_I( int iValue )
{
	if ( !gMsgStarted )
		Sys_Error( "PF_WriteLong_I called with no active message\n" );

	MSG_WriteLong( &gMsgBuffer, iValue );
}

void PF_WriteAngle_I( float flValue )
{
	if ( !gMsgStarted )
		Sys_Error( "PF_WriteAngle_I called with no active message\n" );

	MSG_WriteAngle( &gMsgBuffer, flValue );
}

void PF_WriteCoord_I( float flValue )
{
	if ( !gMsgStarted )
		Sys_Error( "PF_WriteCoord_I called with no active message\n" );

	MSG_WriteCoord( &gMsgBuffer, flValue );
}

void PF_WriteString_I( const char* sz )
{
	if ( !gMsgStarted )
		Sys_Error( "PF_WriteString_I called with no active message\n" );

	MSG_WriteString( &gMsgBuffer, sz );
}

void PF_WriteEntity_I( int iValue )
{
	if ( !gMsgStarted )
		Sys_Error( "PF_WriteEntity_I called with no active message\n" );

	MSG_WriteShort( &gMsgBuffer, iValue );
}

float PF_vectoyaw_I( const float* rgflVector )
{
	float yaw;

	if ( rgflVector[1] == 0 && rgflVector[0] == 0 )
		return 0;

	yaw = (float)(int)( atan2( (double)rgflVector[1], (double)rgflVector[0] ) * 180 / M_PI );
	if ( yaw < 0 )
		yaw += 360;

	return yaw;
}

void PF_vectoangles_I( const float* rgflVectorIn, float* rgflVectorOut )
{
	VectorAngles( rgflVectorIn, rgflVectorOut );
}

float PF_Time()
{
	return (float)Sys_FloatTime();
}
static int32 idum;

void SeedRandomNumberGenerator()
{
	idum = (int32)( 0u - (uint32)time( NULL ) );
	if ( idum > 1000 )
		idum = -idum;
	else if ( idum > -1000 )
		idum -= 22261048;
}

int32 ran1()
{
	int j;
	int32 k;
	static int32 iy;
	static int32 iv[NTAB];

	if ( idum <= 0 || !iy )
	{
		idum = (int32)( 0u - (uint32)idum );
		if ( idum < 1 )
			idum = 1;

		for ( j = NTAB + 7; j >= 0; j-- )
		{
			k = idum / IQ;
			idum = IA * ( idum - k * IQ ) - IR * k;
			if ( idum < 0 )
				idum += IM;
			if ( j < NTAB )
				iv[j] = idum;
		}

		iy = iv[0];
	}

	k = idum / IQ;
	idum = IA * ( idum - k * IQ ) - IR * k;
	if ( idum < 0 )
		idum += IM;

	j = iy / NDIV;
	iy = iv[j];
	iv[j] = idum;

	return iy;
}

float fran1()
{
	float temp = (float)( ran1() * AM );

	if ( temp > RNMX )
		return (float)RNMX;

	return temp;
}

float RandomFloat( float flLow, float flHigh )
{
	float fl;

	g_engdstAddrs.pfnRandomFloat( &flLow, &flHigh );

	fl = fran1();
	return fl * ( flHigh - flLow ) + flLow; // float in [low,high)
}

int32 RandomLong( int32 lLow, int32 lHigh )
{
	uint32 maxAcceptable;
	uint32 diff;
	uint32 x;
	uint32 n;

	g_engdstAddrs.pfnRandomLong( &lLow, &lHigh );

	diff = (uint32)lHigh - (uint32)lLow;
	x = diff + 1;
	if ( !x || diff > MAX_RANDOM_RANGE )
		return lLow;

	// The following maps a uniform distribution on the interval [0,MAX_RANDOM_RANGE]
	// to a smaller, client-specified range of [0,x-1] in a way that doesn't bias
	// the uniform distribution unfavorably. Even for a worst case x, the loop is
	// guaranteed to be taken no more than half the time, so for that worst case x,
	// the average number of times through the loop is 2. For cases where x is
	// much smaller than MAX_RANDOM_RANGE, the average number of times through the
	// loop is very close to 1.
	maxAcceptable = MAX_RANDOM_RANGE - ( ( MAX_RANDOM_RANGE + 1 ) % x );

	do
	{
		n = (uint32)ran1();
	} while ( n > maxAcceptable );

	return (int32)( (uint32)lLow + n % x );
}

void SetMinMaxSize( edict_t* e, const float* mins, const float* maxs, qboolean )
{
	int i;
	vec3_t rmin, rmax;

	for ( i = 0; i < 3; i++ )
	{
		if ( mins[i] > maxs[i] )
			Host_Error( "backwards mins/maxs" );
	}

	VectorCopy( mins, rmin );
	VectorCopy( maxs, rmax );

	// set derived values
	VectorCopy( rmin, e->v.mins );
	VectorCopy( rmax, e->v.maxs );
	VectorSubtract( maxs, mins, e->v.size );

	SV_LinkEdict( e, FALSE );
}

void PF_setsize_I( edict_t* e, const float* rgflMin, const float* rgflMax )
{
	SetMinMaxSize( e, rgflMin, rgflMax, FALSE );
}

void PF_makevectors_I( const float* rgflVector )
{
	AngleVectors( rgflVector, gGlobalVariables.v_forward, gGlobalVariables.v_right, gGlobalVariables.v_up );
}

void PF_setorigin_I( edict_t* e, const float* org )
{
	if ( !e )
		return;

	VectorCopy( org, e->v.origin );
	SV_LinkEdict( e, FALSE );
}

void PF_changeyaw_I( edict_t* ent )
{
	float ideal, current, move, speed;

	current = anglemod( ent->v.angles[1] );
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;

	if ( current == ideal )
		return;

	move = ideal - current;
	if ( ideal > current )
	{
		if ( move >= 180 )
			move = move - 360;
	}
	else
	{
		if ( move <= -180 )
			move = move + 360;
	}

	if ( move > 0 )
	{
		if ( move > speed )
			move = speed;
	}
	else
	{
		if ( move < -speed )
			move = -speed;
	}

	ent->v.angles[1] = anglemod( current + move );
}

void PF_changepitch_I( edict_t* ent )
{
	float ideal, current, move, speed;

	current = anglemod( ent->v.angles[0] );
	ideal = ent->v.idealpitch;
	speed = ent->v.pitch_speed;

	if ( current == ideal )
		return;

	move = ideal - current;
	if ( ideal > current )
	{
		if ( move >= 180 )
			move = move - 360;
	}
	else
	{
		if ( move <= -180 )
			move = move + 360;
	}

	if ( move > 0 )
	{
		if ( move > speed )
			move = speed;
	}
	else
	{
		if ( move < -speed )
			move = -speed;
	}

	ent->v.angles[0] = anglemod( current + move );
}

sizebuf_t* WriteDest_Parm( int dest )
{
	int entnum;

	switch ( dest )
	{
	case MSG_BROADCAST:
		return &sv.datagram;
	case MSG_ONE:
	case MSG_ONE_UNRELIABLE:
		entnum = NUM_FOR_EDICT( gMsgEntity );
		if ( entnum < 1 || entnum > svs.maxclients )
			Host_Error( "WriteDest_Parm: not a client" );
		if ( dest == MSG_ONE )
			return &svs.clients[entnum - 1].netchan.message;
		return &svs.clients[entnum - 1].datagram;
	case MSG_ALL:
		return &sv.reliable_datagram;
	case MSG_INIT:
		return &sv.signon;
	case MSG_PVS:
	case MSG_PAS:
		return &sv.multicast;
	case MSG_SPEC:
		return &sv.spectator;
	default:
		Host_Error( "WriteDest_Parm: bad destination=%d", dest );
	}
}

void PF_MessageEnd_I()
{
	qboolean MsgIsVarLength = FALSE;
	sizebuf_t* pBuffer;
	UserMsg* pUserMsg;
	client_t* client;
	int entnum;

	if ( !gMsgStarted )
		Sys_Error( "MESSAGE_END called with no active message\n" );

	gMsgStarted = FALSE;

	if ( gMsgEntity && ( gMsgEntity->v.flags & FL_FAKECLIENT ) )
		return;

	if ( gMsgBuffer.flags & FSB_OVERFLOWED )
		Sys_Error( "MESSAGE_END called, but message buffer from .dll had overflowed\n" );

	if ( gMsgType > svc_lastmsg )
	{
		pUserMsg = sv_gpUserMsgs;
		while ( pUserMsg && pUserMsg->iMsg != gMsgType )
			pUserMsg = pUserMsg->next;

		if ( !pUserMsg && gMsgDest == MSG_INIT )
		{
			pUserMsg = sv_gpNewUserMsgs;
			while ( pUserMsg && pUserMsg->iMsg != gMsgType )
				pUserMsg = pUserMsg->next;
		}

		if ( !pUserMsg )
		{
			Con_DPrintf( "PF_MessageEnd_I:  Unknown User Msg %d\n", gMsgType );
			return;
		}

		if ( pUserMsg->iSize == -1 )
		{
			MsgIsVarLength = TRUE;

			if ( gMsgBuffer.cursize > MAX_USER_MSG_DATA )
			{
				Host_Error( "PF_MessageEnd_I:  Refusing to send user message %s of %i bytes to client, user message size limit is %i bytes\n",
					pUserMsg->szName, gMsgBuffer.cursize, MAX_USER_MSG_DATA );
			}
		}
		else if ( pUserMsg->iSize != gMsgBuffer.cursize )
		{
			Sys_Error( "User Msg '%s': %d bytes written, expected %d\n", pUserMsg->szName, gMsgBuffer.cursize, pUserMsg->iSize );
		}
	}

	pBuffer = WriteDest_Parm( gMsgDest );
	if ( gMsgDest == MSG_BROADCAST && gMsgBuffer.cursize + pBuffer->cursize > pBuffer->maxsize )
		return;

	if ( !pBuffer->data )
		return;

	if ( gMsgType > svc_lastmsg && ( gMsgDest == MSG_ONE || gMsgDest == MSG_ONE_UNRELIABLE ) )
	{
		entnum = NUM_FOR_EDICT( gMsgEntity );
		if ( entnum < 1 || entnum > svs.maxclients )
			Host_Error( "WriteDest_Parm: not a client" );

		client = &svs.clients[entnum - 1];
		if ( !client->hasusrmsgs || !( client->active || client->spawned ) || client->fakeclient )
			return;
	}

	MSG_WriteByte( pBuffer, gMsgType );
	if ( MsgIsVarLength )
		MSG_WriteByte( pBuffer, gMsgBuffer.cursize );
	MSG_WriteBuf( pBuffer, gMsgBuffer.cursize, gMsgBuffer.data );

	switch ( gMsgDest )
	{
	case MSG_PVS:
		SV_Multicast( gMsgEntity, gMsgOrigin, MSG_FL_PVS, FALSE );
		break;
	case MSG_PAS:
		SV_Multicast( gMsgEntity, gMsgOrigin, MSG_FL_PAS, FALSE );
		break;
	case MSG_PVS_R:
	case MSG_PAS_R:
		SV_Multicast( gMsgEntity, gMsgOrigin, MSG_FL_PAS, TRUE );
		break;
	}
}

msurface_t* SurfaceAtPoint( model_t* pModel, mnode_t* node, vec_t* start, vec_t* end )
{
	float front, back, frac;
	int side;
	mplane_t* plane;
	vec3_t mid;
	msurface_t* surf;
	int s, t, ds, dt;
	int i;
	mtexinfo_t* tex;

	if ( node->contents < 0 )
		return NULL;

	plane = node->plane;
	front = DotProduct( start, plane->normal ) - plane->dist;
	back = DotProduct( end, plane->normal ) - plane->dist;

	side = front < 0;

	// If they're both on the same side of the plane, don't bother to split
	// just check the appropriate child
	if ( ( back < 0 ) == ( front < 0 ) )
		return SurfaceAtPoint( pModel, node->children[side], start, end );

	// calculate mid point
	frac = front / ( front - back );
	for ( i = 0; i < 3; i++ )
		mid[i] = start[i] + ( end[i] - start[i] ) * frac;

	// go down front side
	surf = SurfaceAtPoint( pModel, node->children[side], start, mid );
	if ( surf )
		return surf;

	// check for impact on this node
	for ( i = 0; i < node->numsurfaces; i++ )
	{
		surf = &pModel->surfaces[node->firstsurface + i];
		tex = surf->texinfo;

		s = (int)( DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3] );
		t = (int)( DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3] );

		if ( s < surf->texturemins[0] || t < surf->texturemins[1] )
			continue;

		ds = (int)( (unsigned int)s - (unsigned int)surf->texturemins[0] );
		dt = (int)( (unsigned int)t - (unsigned int)surf->texturemins[1] );

		// Check this surface to see if there's an intersection
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		return surf;
	}

	// go down back side
	return SurfaceAtPoint( pModel, node->children[!side], mid, end );
}

qboolean PR_IsEmptyString( const char* s )
{
	return s[0] <= ' ';
}

int PF_precache_generic_I( char* s )
{
	int i;

	if ( !s )
	{
		Host_Error( "PF_precache_generic_I: NULL pointer" );
	}

	if ( PR_IsEmptyString( s ) )
	{
		Con_Printf( "PF_precache_generic_I: Bad string '%s'", s );
		return 0;
	}

	if ( sv.state != ss_loading )
	{
		for ( i = 0; i < MAX_GENERIC; i++ )
		{
			if ( sv.generic_precache[i] && !Q_strcmp( sv.generic_precache[i], s ) )
				return i;
		}
		Host_Error( "PF_precache_generic_I: '%s' Precache can only be done in spawn functions", s );
	}

	for ( i = 0; i < MAX_GENERIC; i++ )
	{
		if ( !sv.generic_precache[i] )
		{
			sv.generic_precache[i] = s;
			return i;
		}

		if ( !Q_strcasecmp( sv.generic_precache[i], s ) )
			return i;
	}

	Host_Error( "PF_precache_generic_I: Generic item '%s' failed to precache because the item count is over the %d limit.\n"
				"Reduce the number of brush models and/or regular models in the map to correct this.",
		s, MAX_GENERIC );
}

unsigned short EV_Precache( int type, const char* psz )
{
	int i;
	int filesize;
	char szpath[GAME_DIRECTORY_BYTES];
	byte* pscript;
	event_t* ev;

	if ( !psz )
	{
		Host_Error( "EV_Precache: NULL pointer" );
	}

	if ( PR_IsEmptyString( psz ) )
	{
		Host_Error( "EV_Precache: Bad string '%s'", psz );
	}

	if ( sv.state != ss_loading )
	{
		for ( i = 1; i < MAX_EVENT_SCRIPTS; i++ )
		{
			if ( sv.event_precache[i].filename && !Q_strcasecmp( sv.event_precache[i].filename, psz ) )
				return (unsigned short)i;
		}
		Host_Error( "EV_Precache: '%s' Precache can only be done in spawn functions", psz );
	}

	i = 1;
	while ( sv.event_precache[i].filename )
	{
		if ( !Q_strcasecmp( sv.event_precache[i].filename, psz ) )
			return (unsigned short)i;

		if ( ++i == MAX_EVENT_SCRIPTS )
		{
			Host_Error( "EV_Precache: '%s' overflow", psz );
		}
	}

	if ( type != 1 )
	{
		Host_Error( "EV_Precache:  only file type 1 supported currently\n" );
	}

	snprintf( szpath, sizeof( szpath ), "%s", psz );
	COM_FixSlashes( szpath );

	pscript = COM_LoadFile( szpath, LOADFILE_HEAP, &filesize );
	if ( !pscript )
	{
		Host_Error( "EV_Precache:  file %s missing from server\n", psz );
	}

	ev = &sv.event_precache[i];
	ev->filesize = filesize;
	ev->filename = psz;
	ev->pszScript = (const char*)pscript;
	ev->index = (unsigned short)i;

	return (unsigned short)i;
}

void PF_SetKeyValue_I( char* infobuffer, char* key, char* value )
{
	if ( infobuffer == localinfo )
	{
		Info_SetValueForKey( infobuffer, key, value, 32768 );
		return;
	}

	if ( infobuffer != Info_Serverinfo() )
		Sys_Error( "Can't set client keys with SetKeyValue" );

	Info_SetValueForKey( infobuffer, key, value, MAX_SERVERINFO_STRING );
}

int PF_modelindex( const char* pstr )
{
	return SV_ModelIndex( pstr );
}

int ModelFrames( int modelIndex )
{
	if ( modelIndex <= 0 || modelIndex >= MAX_MODELS )
	{
		Con_DPrintf( "Bad sprite index!\n" );
		return 1;
	}

	return ModelFrameCount( sv.models[modelIndex] );
}

void PF_setmodel_I( edict_t* e, const char* m )
{
	int i;
	model_t* mod;

	// check to see if model was properly precached
	for ( i = 0; sv.model_precache[i]; i++ )
	{
		if ( !Q_strcasecmp( sv.model_precache[i], m ) )
			break;
	}

	if ( !sv.model_precache[i] )
		Host_Error( "no precache: %s\n", m );

	e->v.model = (int)( m - pr_strings );
	e->v.modelindex = i;

	mod = sv.models[i];
	if ( mod )
		SetMinMaxSize( e, mod->mins, mod->maxs, FALSE );
	else
		SetMinMaxSize( e, vec3_origin, vec3_origin, TRUE );
}

int PF_precache_model_I( char* s )
{
	int i;
	qboolean optional;

	if ( !s )
	{
		Host_Error( "PF_precache_model_I: NULL pointer" );
	}

	if ( PR_IsEmptyString( s ) )
	{
		Host_Error( "PF_precache_model_I: Bad string '%s'", s );
	}

	optional = s[0] == '!';
	if ( optional )
		s++;

	if ( sv.state != ss_loading )
	{
		for ( i = 0; i < MAX_MODELS; i++ )
		{
			if ( sv.model_precache[i] && !Q_strcasecmp( sv.model_precache[i], s ) )
				return i;
		}
		Host_Error( "PF_precache_model_I: '%s' Precache can only be done in spawn functions", s );
	}

	i = 0;
	while ( sv.model_precache[i] )
	{
		if ( !Q_strcasecmp( sv.model_precache[i], s ) )
			return i;

		if ( ++i == MAX_MODELS )
		{
			Host_Error( "PF_precache_model_I: Model '%s' failed to precache because the item count is over the %d limit.\n"
						"Reduce the number of brush models and/or regular models in the map to correct this.",
				s, MAX_MODELS );
		}
	}

	sv.model_precache[i] = s;
	sv.models[i] = Mod_ForName( s, TRUE, TRUE );
	if ( !optional )
		sv.model_precache_flags[i] |= RES_FATALIFMISSING;

	return i;
}

int PF_precache_sound_I( char* s )
{
	int i;

	if ( !s )
	{
		Host_Error( "PF_precache_sound_I: NULL pointer" );
	}

	if ( PR_IsEmptyString( s ) )
	{
		Host_Error( "PF_precache_sound_I: Bad string '%s'", s );
	}

	if ( s[0] == '!' )
	{
		Host_Error( "PF_precache_sound_I: '%s' do not precache sentence names!", s );
	}

	if ( sv.state != ss_loading )
	{
		for ( i = 0; i < MAX_SOUNDS; i++ )
		{
			if ( sv.sound_precache[i] && !Q_strcasecmp( sv.sound_precache[i], s ) )
				return i;
		}
		Host_Error( "PF_precache_sound_I: '%s' Precache can only be done in spawn functions", s );
	}

	sv.sound_precache_hashedlookup_built = FALSE;

	i = 0;
	while ( sv.sound_precache[i] )
	{
		if ( !Q_strcasecmp( sv.sound_precache[i], s ) )
			return i;

		if ( ++i == MAX_SOUNDS )
		{
			Host_Error( "PF_precache_sound_I: Sound '%s' failed to precache because the item count is over the %d limit.\n"
						"Reduce the number of brush models and/or regular models in the map to correct this.",
				s, MAX_SOUNDS );
		}
	}

	sv.sound_precache[i] = s;
	return i;
}

void PF_makestatic_I( edict_t* ent )
{
	int i;

	MSG_WriteByte( &sv.signon, svc_spawnstatic );
	MSG_WriteShort( &sv.signon, (short)SV_ModelIndex( pr_strings + ent->v.model ) );
	MSG_WriteByte( &sv.signon, ent->v.sequence );
	MSG_WriteByte( &sv.signon, (int)ent->v.frame );
	MSG_WriteShort( &sv.signon, ent->v.colormap );
	MSG_WriteByte( &sv.signon, ent->v.skin );

	for ( i = 0; i < 3; i++ )
	{
		MSG_WriteCoord( &sv.signon, ent->v.origin[i] );
		MSG_WriteAngle( &sv.signon, ent->v.angles[i] );
	}

	MSG_WriteByte( &sv.signon, ent->v.rendermode );
	if ( ent->v.rendermode != kRenderNormal )
	{
		MSG_WriteByte( &sv.signon, (int)ent->v.renderamt );
		MSG_WriteByte( &sv.signon, (int)ent->v.rendercolor[0] );
		MSG_WriteByte( &sv.signon, (int)ent->v.rendercolor[1] );
		MSG_WriteByte( &sv.signon, (int)ent->v.rendercolor[2] );
		MSG_WriteByte( &sv.signon, ent->v.renderfx );
	}

	// throw the entity away now
	ED_Free( ent );
}

void PF_sound_I( edict_t* entity, int channel, const char* sample, float volume, float attenuation, int fFlags, int pitch )
{
	if ( volume < 0.0 || volume > 255.0 )
	{
		double dvolume = volume;
		uint64 bits;

		memcpy( &bits, &dvolume, sizeof( bits ) );
		Sys_Error( "EMIT_SOUND: volume = %i", (int)(uint32)bits );
	}

	if ( attenuation < 0.0 || attenuation > 4.0 )
		Sys_Error( "EMIT_SOUND: attenuation = %f", attenuation );

	if ( channel < 0 || channel > CHAN_NETWORKVOICE_BASE )
		Sys_Error( "EMIT_SOUND: channel = %i", channel );

	if ( pitch < 0 || pitch > 255 )
		Sys_Error( "EMIT_SOUND: pitch = %i", pitch );

	SV_StartSound( 0, entity, channel, sample, (int)( volume * 255 ), attenuation, fFlags, pitch );
}

void PF_ambientsound_I( edict_t* entity, float* pos, const char* samp, float vol, float attenuation, int fFlags, int pitch )
{
	int i, soundnum;
	short flags;
	short ent;
	sizebuf_t* pout;

	if ( !samp )
	{
		Con_Printf( "PF_ambientsound_I: NULL sample\n" );
		return;
	}

	flags = (short)fFlags;

	if ( samp[0] == CHAR_SENTENCE )
	{
		flags = (short)( fFlags | SND_SENTENCE );

		soundnum = Q_atoi( samp + 1 );
		if ( soundnum >= MAX_SENTENCES )
		{
			Con_Printf( "invalid sentence number: %s", samp + 1 );
			return;
		}
	}
	else
	{
		// check to see if samp was properly precached
		for ( soundnum = 0; sv.sound_precache[soundnum]; soundnum++ )
		{
			if ( !Q_strcasecmp( sv.sound_precache[soundnum], samp ) )
				break;
		}

		if ( !sv.sound_precache[soundnum] )
		{
			Con_Printf( "no precache: %s\n", samp );
			return;
		}
	}

	ent = (short)NUM_FOR_EDICT( entity );

	pout = &sv.datagram;
	if ( flags & SND_SPAWNING )
		pout = &sv.signon;

	MSG_WriteByte( pout, svc_spawnstaticsound );
	for ( i = 0; i < 3; i++ )
		MSG_WriteCoord( pout, pos[i] );

	MSG_WriteShort( pout, soundnum );
	MSG_WriteByte( pout, (int)(float)( vol * 255.0 ) );
	MSG_WriteByte( pout, (int)(float)( attenuation * 64.0 ) );
	MSG_WriteShort( pout, ent );
	MSG_WriteByte( pout, pitch );
	MSG_WriteByte( pout, flags );
}

void PF_particle_I( const float* org, const float* dir, float color, float count )
{
	SV_StartParticle( org, dir, (int)color, (int)count );
}

void PF_lightstyle_I( int style, char* val )
{
	client_t* client;
	int j;

	// change the string in sv
	sv.lightstyles[style] = val;

	// send message to all clients on this server
	if ( sv.state != ss_active )
		return;

	for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
	{
		if ( ( client->active || client->spawned ) && !client->fakeclient )
		{
			MSG_WriteByte( &client->netchan.message, svc_lightstyle );
			MSG_WriteByte( &client->netchan.message, style );
			MSG_WriteString( &client->netchan.message, val );
		}
	}
}

int PF_DecalIndex( const char* name )
{
	int i;

	for ( i = 0; i < sv_decalnamecount; i++ )
	{
		if ( !Q_strcasecmp( sv_decalnames[i].name, name ) )
			return i;
	}

	return -1;
}

void PF_StaticDecal( const float* origin, int decalIndex, int entityIndex, int modelIndex )
{
	MSG_WriteByte( &sv.signon, svc_temp_entity );
	MSG_WriteByte( &sv.signon, TE_BSPDECAL );
	MSG_WriteCoord( &sv.signon, origin[0] );
	MSG_WriteCoord( &sv.signon, origin[1] );
	MSG_WriteCoord( &sv.signon, origin[2] );
	MSG_WriteShort( &sv.signon, decalIndex );
	MSG_WriteShort( &sv.signon, entityIndex );
	if ( entityIndex )
		MSG_WriteShort( &sv.signon, modelIndex );
}

void PF_BuildSoundMsg_I( edict_t* entity, int channel, const char* sample, float volume, float attenuation, int fFlags,
	int pitch, int msg_dest, int msg_type, const float* pOrigin, edict_t* ed )
{
	PF_MessageBegin_I( msg_dest, msg_type, pOrigin, ed );
	SV_BuildSoundMsg( entity, channel, sample, (int)volume, attenuation, fFlags, pitch, pOrigin, &gMsgBuffer );
	PF_MessageEnd_I();
}

int iGetIndex( const char* pszField )
{
	char sz[512];

	Q_strncpy( sz, pszField, sizeof( sz ) - 1 );
	sz[sizeof( sz ) - 1] = 0;
	Q_strlwr( sz );

	if ( !Q_strcmp( sz, "classname" ) )
		return offsetof( entvars_t, classname );
	if ( !Q_strcmp( sz, "model" ) )
		return offsetof( entvars_t, model );
	if ( !Q_strcmp( sz, "viewmodel" ) )
		return offsetof( entvars_t, viewmodel );
	if ( !Q_strcmp( sz, "weaponmodel" ) )
		return offsetof( entvars_t, weaponmodel );
	if ( !Q_strcmp( sz, "netname" ) )
		return offsetof( entvars_t, netname );
	if ( !Q_strcmp( sz, "target" ) )
		return offsetof( entvars_t, target );
	if ( !Q_strcmp( sz, "targetname" ) )
		return offsetof( entvars_t, targetname );
	if ( !Q_strcmp( sz, "message" ) )
		return offsetof( entvars_t, message );
	if ( !Q_strcmp( sz, "noise" ) )
		return offsetof( entvars_t, noise );
	if ( !Q_strcmp( sz, "noise1" ) )
		return offsetof( entvars_t, noise1 );
	if ( !Q_strcmp( sz, "noise2" ) )
		return offsetof( entvars_t, noise2 );
	if ( !Q_strcmp( sz, "noise3" ) )
		return offsetof( entvars_t, noise3 );
	if ( !Q_strcmp( sz, "globalname" ) )
		return offsetof( entvars_t, globalname );

	return -1;
}

edict_t* FindEntityByString( edict_t* pEdictStartSearchAfter, const char* pszField, const char* pszValue )
{
	int e;
	int iField;
	edict_t* ed;
	const char* t;

	iField = iGetIndex( pszField );
	if ( iField == -1 || !pszValue )
		return NULL;

	if ( pEdictStartSearchAfter )
		e = NUM_FOR_EDICT( pEdictStartSearchAfter );
	else
		e = 0;

	for ( e++; e < sv.num_edicts; e++ )
	{
		ed = &sv.edicts[e];
		if ( ed->free )
			continue;

		t = pr_strings + *(string_t*)( (byte*)&ed->v + iField );
		if ( !t || t == pr_strings )
			continue;

		if ( !Q_strcmp( t, pszValue ) )
			return ed;
	}

	return sv.edicts;
}

edict_t* PF_Spawn_I()
{
	return ED_Alloc();
}

void PF_Remove_I( edict_t* ed )
{
	ED_Free( ed );
}

edict_t* CreateNamedEntity( int className )
{
	edict_t* pedict;
	ENTITYINIT pEntityInit;

	if ( !className )
		Sys_Error( "Spawned a NULL entity!" );

	pedict = ED_Alloc();
	pedict->v.classname = className;

	pEntityInit = GetEntityInit( pr_strings + className );
	if ( !pEntityInit )
	{
		ED_Free( pedict );
		Con_DPrintf( "Can't create entity: %s\n", pr_strings + className );
		return NULL;
	}

	pEntityInit( &pedict->v );
	return pedict;
}

int GetEntityIllum( edict_t* pEnt )
{
	int iIndex;
	colorVec* cvFloorColor;

	if ( !pEnt )
		return -1;

	iIndex = NUM_FOR_EDICT( pEnt );
	if ( iIndex <= svs.maxclients )
	{
		return pEnt->v.light_level;
	}

	if ( cls.state != ca_connected && cls.state != ca_uninitialized && cls.state != ca_active )
		return 128;

	cl_entity_t* entity = CL_GetEntityIfValid( iIndex );
	if ( !entity )
		return 128;
	cvFloorColor = &entity->cvFloorColor;
	return (int)( cvFloorColor->r + cvFloorColor->g + cvFloorColor->b ) / 3;
}

void ServerPrint( const char* text )
{
	Con_Printf( "%s", text );
}

void ClientPrintf( edict_t* entity, PRINT_TYPE type, const char* text )
{
	int index = NUM_FOR_EDICT( entity );
	if ( index <= 0 || index > svs.maxclients )
	{
		Con_Printf( "tried to sprint to a non-client\n" );
		return;
	}

	client_t* client = &svs.clients[index - 1];
	if ( client->fakeclient )
		return;

	if ( type == print_center )
		MSG_WriteChar( &client->netchan.message, svc_centerprint );
	else if ( type == print_console || type == print_chat )
		MSG_WriteByte( &client->netchan.message, svc_print );
	else
	{
		Con_Printf( "invalid PRINT_TYPE %i\n", type );
		return;
	}
	MSG_WriteString( &client->netchan.message, text );
}

void PF_stuffcmd_I( edict_t* entity, char* format, ... )
{
	static char text[STUFFCMD_BUFFER_BYTES];
	int index = NUM_FOR_EDICT( entity );
	va_list args;
	va_start( args, format );
	vsnprintf( text, sizeof( text ), format, args );
	va_end( args );
	text[sizeof( text ) - 1] = 0;

	if ( index <= 0 || index > svs.maxclients )
	{
		Con_Printf( "\n!!!\n\nStuffCmd:  Some entity tried to stuff '%s' to console buffer of entity %i when maxclients was set to %i, ignoring\n\n",
			text, index, svs.maxclients );
		return;
	}

	int length = Q_strlen( text );
	if ( !length || ( text[length - 1] != '\n' && text[length - 1] != ';' ) )
	{
		Con_Printf( "Tried to stuff bad command %s\n", text );
		return;
	}

	client_t* savedClient = host_client;
	host_client = &svs.clients[index - 1];
	Host_ClientCommands( "%s", text );
	host_client = savedClient;
}

int PF_NumberOfEntities_I()
{
	int count = 0;
	for ( int index = 0; index < sv.num_edicts; index++ )
	{
		if ( !sv.edicts[index].free )
			count++;
	}
	return count;
}

char* PF_GetInfoKeyBuffer_I( edict_t* entity )
{
	if ( !entity )
		return localinfo;
	int index = NUM_FOR_EDICT( entity );
	if ( !index )
		return Info_Serverinfo();
	if ( index <= MAX_CLIENTS )
		return svs.clients[index - 1].userinfo;
	return (char*)com_empty_string;
}

void PF_setview_I( const edict_t* clientEntity, const edict_t* viewEntity )
{
	int index = NUM_FOR_EDICT( clientEntity );
	if ( index <= 0 || index > svs.maxclients )
		Host_Error( "PF_setview_I: not a client" );

	client_t* client = &svs.clients[index - 1];
	if ( client->fakeclient )
		return;
	client->pViewEntity = viewEntity;
	// A restored game can set the view before the local client's netchan is ready.
	if ( !( client->active || client->spawned ) )
		return;
	MSG_WriteByte( &client->netchan.message, svc_setview );
	MSG_WriteShort( &client->netchan.message, NUM_FOR_EDICT( viewEntity ) );
}

void PF_crosshairangle_I( const edict_t* clientEntity, float pitch, float yaw )
{
	int index = NUM_FOR_EDICT( clientEntity );
	if ( index <= 0 || index > svs.maxclients )
		return;
	client_t* client = &svs.clients[index - 1];
	if ( client->fakeclient )
		return;

	if ( pitch > 180.0f )
		pitch -= 360.0f;
	if ( pitch < -180.0f )
		pitch += 360.0f;
	if ( yaw > 180.0f )
		yaw -= 360.0f;
	if ( yaw < -180.0f )
		yaw += 360.0f;

	MSG_WriteByte( &client->netchan.message, svc_crosshairangle );
	MSG_WriteChar( &client->netchan.message, (int)( pitch * 5.0f ) );
	MSG_WriteChar( &client->netchan.message, (int)( yaw * 5.0f ) );
}

edict_t* PF_CreateFakeClient_I( const char* name )
{
	int index;
	for ( index = 0; index < svs.maxclients; index++ )
	{
		client_t* client = &svs.clients[index];
		if ( !client->active && !client->spawned && !client->connected )
			break;
	}
	if ( index == svs.maxclients )
		return NULL;

	client_t* client = &svs.clients[index];
	edict_t* entity = EDICT_NUM( index + 1 );
	if ( client->frames )
		SV_ClearFrames( &client->frames );
	Q_memset( client, 0, sizeof( *client ) );
	client->resourcesneeded.pPrev = &client->resourcesneeded;
	client->resourcesneeded.pNext = &client->resourcesneeded;
	client->resourcesonhand.pPrev = &client->resourcesonhand;
	client->resourcesonhand.pNext = &client->resourcesonhand;
	Q_strncpy( client->name, name, sizeof( client->name ) );
	client->name[sizeof( client->name ) - 1] = 0;
	client->active = TRUE;
	client->fakeclient = TRUE;
	client->edict = entity;
	client->spawned = TRUE;
	client->userid = g_userid++;
	client->fully_connected = TRUE;
	client->connected = TRUE;
	client->uploading = FALSE;
	entity->v.flags = FL_CLIENT | FL_FAKECLIENT;
	entity->v.netname = (string_t)( client->name - pr_strings );
	entity->v.pContainingEntity = entity;
	Info_SetValueForKey( client->userinfo, "name", name, sizeof( client->userinfo ) );
	Info_SetValueForKey( client->userinfo, "model", "gordon", sizeof( client->userinfo ) );
	Info_SetValueForKey( client->userinfo, "topcolor", "1", sizeof( client->userinfo ) );
	Info_SetValueForKey( client->userinfo, "bottomcolor", "1", sizeof( client->userinfo ) );
	client->sendinfo = TRUE;
	SV_ExtractFromUserinfo( client );
	uint64 steamID = ISteamGameServer_CreateUnauthenticatedUserConnection();
	client->network_userid.idtype = AUTH_IDTYPE_STEAM;
	client->network_userid.m_SteamID = steamID;
	ISteamGameServer_BUpdateUserData( steamID, name, 0 );
	return entity;
}

void PF_RunPlayerMove_I( edict_t* fakeClient, const float* viewAngles, float forwardMove,
	float sideMove, float upMove, unsigned short buttons, byte impulse, byte msec )
{
	edict_t* savedPlayer = sv_player;
	client_t* savedClient = host_client;
	sv_player = fakeClient;
	host_client = &svs.clients[NUM_FOR_EDICT( fakeClient ) - 1];
	host_client->svtimebase = sv.time + host_frametime - (double)msec / 1000.0;
	usercmd_t command;
	Q_memset( &command, 0, sizeof( command ) );
	command.impulse = impulse;
	command.msec = msec;
	VectorCopy( viewAngles, command.viewangles );
	command.forwardmove = forwardMove;
	command.sidemove = sideMove;
	command.upmove = upMove;
	pmove = &g_svmove;
	command.buttons = buttons;
	command.lightlevel = 0;
	SV_PreRunCmd();
	SV_RunCmd( &command, 0 );
	host_client->lastcmd = command;
	sv_player = savedPlayer;
	host_client = savedClient;
}

void PF_setspawnparms_I( edict_t* entity )
{
	int index = NUM_FOR_EDICT( entity );
	if ( index <= 0 || index > svs.maxclients )
		Host_Error( "Entity is not a client" );
}

void PF_changelevel_I( char* level, char* landmark )
{
	static int lastSpawnCount;
	if ( svs.spawncount == lastSpawnCount )
		return;
	lastSpawnCount = svs.spawncount;
	SV_SkipUpdates();
	Cbuf_AddText( landmark ? va( "changelevel2 %s %s\n", level, landmark ) : va( "changelevel %s\n", level ) );
}

void PF_FadeVolume( const edict_t* entity, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds )
{
	int index = NUM_FOR_EDICT( entity );
	if ( index <= 0 || index > svs.maxclients )
	{
		Con_Printf( "tried to PF_FadeVolume a non-client\n" );
		return;
	}
	client_t* client = &svs.clients[index - 1];
	if ( client->fakeclient )
		return;
	MSG_WriteChar( &client->netchan.message, svc_soundfade );
	MSG_WriteByte( &client->netchan.message, (byte)fadePercent );
	MSG_WriteByte( &client->netchan.message, (byte)holdTime );
	MSG_WriteByte( &client->netchan.message, (byte)fadeOutSeconds );
	MSG_WriteByte( &client->netchan.message, (byte)fadeInSeconds );
}

void PF_SetClientMaxspeed( const edict_t* entity, float maxSpeed )
{
	int index = NUM_FOR_EDICT( entity );
	if ( index <= 0 || index > svs.maxclients )
		Con_Printf( "tried to PF_SetClientMaxspeed a non-client\n" );
	else
		( (edict_t*)entity )->v.maxspeed = maxSpeed;
}

const char* PF_GetPlayerAuthId( edict_t* entity )
{
	static char authIDs[AUTH_ID_BUFFERS][AUTH_ID_BYTES];
	static int bufferIndex = -1;
	bufferIndex = ( bufferIndex + 1 ) % AUTH_ID_BUFFERS;
	char* authID = authIDs[bufferIndex];
	authID[0] = 0;
	if ( !sv.active || !entity )
		return authID;

	for ( int index = 0; index < svs.maxclients; index++ )
	{
		client_t* client = &svs.clients[index];
		if ( client->edict != entity )
			continue;
		if ( client->fakeclient )
			Q_strcpy( authID, "BOT" );
		else if ( client->network_userid.idtype == AUTH_IDTYPE_LOCAL )
			Q_strcpy( authID, "HLTV" );
		else
			snprintf( authID, AUTH_ID_BYTES - 1, "%s", SV_GetClientIDString( client ) );
	}
	authID[AUTH_ID_BYTES - 1] = 0;
	return authID;
}

int PF_GetCurrentPlayer()
{
	int index = (int)( host_client - svs.clients );
	return index < 0 || index >= svs.maxclients ? -1 : index;
}

int PF_CanSkipPlayer( const edict_t* entity )
{
	int index = NUM_FOR_EDICT( entity );
	if ( index > 0 && index <= svs.maxclients )
		return svs.clients[index - 1].lw != 0;
	Con_Printf( "tried to PF_CanSkipPlayer a non-client\n" );
	return 0;
}

void PF_ForceUnmodified( FORCE_TYPE type, float* mins, float* maxs, const char* filename )
{
	if ( !filename )
		Host_Error( "PF_ForceUnmodified: NULL pointer" );
	if ( filename[0] <= ' ' )
		Host_Error( "PF_ForceUnmodified: Bad string '%s'", filename );

	if ( sv.state != ss_loading )
	{
		for ( int index = 0; index < MAX_CONSISTENCY_FILES; index++ )
		{
			consistency_t* entry = &sv.consistency_list[index];
			if ( entry->filename && !Q_stricmp( entry->filename, filename ) )
				return;
		}
		Host_Error( "ForceUnmodified: '%s' Precache can only be done in spawn functions", filename );
	}

	for ( int index = 0; index < MAX_CONSISTENCY_FILES; index++ )
	{
		consistency_t* entry = &sv.consistency_list[index];
		if ( entry->filename )
		{
			if ( !Q_stricmp( entry->filename, filename ) )
				return;
			continue;
		}
		entry->filename = (char*)filename;
		entry->check_type = type;
		if ( mins )
			VectorCopy( mins, entry->mins );
		if ( maxs )
			VectorCopy( maxs, entry->maxs );
		return;
	}
	Host_Error( "ForceUnmodified: '%s' overflow", filename );
}

void PF_GetPlayerStats( const edict_t* entity, int* ping, int* packetLoss )
{
	*ping = 0;
	*packetLoss = 0;
	int index = NUM_FOR_EDICT( entity );
	if ( index <= 0 || index > svs.maxclients )
	{
		Con_Printf( "tried to PF_GetPlayerStats a non-client\n" );
		return;
	}
	client_t* client = &svs.clients[index - 1];
	*ping = (int)( 1000.0 * client->latency );
	*packetLoss = (int)client->packet_loss;
}

void QueryClientCvarValue( const edict_t* player, const char* cvarName )
{
	int index = NUM_FOR_EDICT( player );
	if ( index <= 0 || index > svs.maxclients )
	{
		if ( gNewDLLFunctions.pfnCvarValue )
			gNewDLLFunctions.pfnCvarValue( player, "Bad Player" );
		Con_Printf( "tried to QueryClientCvarValue a non-client\n" );
		return;
	}
	sizebuf_t* message = &svs.clients[index - 1].netchan.message;
	MSG_WriteChar( message, svc_sendcvarvalue );
	MSG_WriteString( message, cvarName );
}

void QueryClientCvarValue2( const edict_t* player, const char* cvarName, int requestID )
{
	int index = NUM_FOR_EDICT( player );
	if ( index <= 0 || index > svs.maxclients )
	{
		if ( gNewDLLFunctions.pfnCvarValue2 )
			gNewDLLFunctions.pfnCvarValue2( player, requestID, cvarName, "Bad Player" );
		Con_Printf( "tried to QueryClientCvarValue a non-client\n" );
		return;
	}
	sizebuf_t* message = &svs.clients[index - 1].netchan.message;
	MSG_WriteChar( message, svc_sendcvarvalue2 );
	MSG_WriteLong( message, requestID );
	MSG_WriteString( message, cvarName );
}

int hudCheckParm( char* parameter, char** next )
{
	g_engdstAddrs.CheckParm( &parameter, &next );
	int index = COM_CheckParm( parameter );
	if ( next )
		*next = index && index < com_argc - 1 ? com_argv[index + 1] : NULL;
	return index;
}

int EngCheckParm( const char* parameter, char** next )
{
	return hudCheckParm( (char*)parameter, next );
}

//============================================================================

byte checkpvs[MAX_MAP_LEAFS / 8];
int c_invis;
int c_notvis;

int PF_newcheckclient( int check )
{
// cycle to the next one

	if ( check < 1 )
		check = 1;
	if ( check > svs.maxclients )
		check = svs.maxclients;

	int index = check + 1;
	if ( index > svs.maxclients )
		index = 1;
	edict_t* entity = &sv.edicts[index];
	while ( index != check )
	{
		if ( !entity->free && entity->pvPrivateData && !( entity->v.flags & FL_NOTARGET ) )
			break;
		if ( ++index > svs.maxclients )
			index = 1;
		entity = &sv.edicts[index];
	}

	vec3_t view;
// get the PVS for the entity
	VectorAdd( entity->v.origin, entity->v.view_ofs, view );
	mleaf_t* leaf = Mod_PointInLeaf( view, sv.worldmodel );
	byte* visible = Mod_LeafPVS( leaf, sv.worldmodel );
	Q_memcpy( checkpvs, visible, ( sv.worldmodel->numleafs + 7 ) >> 3 );
	return index;
}

edict_t* PF_checkclient_I( edict_t* entity )
{
	if ( sv.time - sv.lastchecktime >= 0.1 )
	{
		sv.lastcheck = PF_newcheckclient( sv.lastcheck );
		sv.lastchecktime = sv.time;
	}
	edict_t* clientEntity = &sv.edicts[sv.lastcheck];
	if ( clientEntity->free || !clientEntity->pvPrivateData )
		return sv.edicts;

	vec3_t view;
	VectorAdd( entity->v.origin, entity->v.view_ofs, view );
	mleaf_t* leaf = Mod_PointInLeaf( view, sv.worldmodel );
	int index = (int)( leaf - sv.worldmodel->leafs ) - 1;
	if ( index < 0 || !( checkpvs[index >> 3] & ( 1 << ( index & 7 ) ) ) )
	{
		c_notvis++;
		return sv.edicts;
	}
	c_invis++;
	return clientEntity;
}

void PVSMark( model_t* model, byte* visible )
{
	r_visframecount++;
	for ( int index = 0; index < model->numleafs; index++ )
	{
		if ( !( visible[index >> 3] & ( 1 << ( index & 7 ) ) ) )
			continue;
		mnode_t* node = (mnode_t*)&model->leafs[index + 1];
		while ( node && node->visframe != r_visframecount )
		{
			node->visframe = r_visframecount;
			node = node->parent;
		}
	}
}

edict_t* PVSFindEntities( edict_t* player )
{
	vec3_t view;
	VectorAdd( player->v.origin, player->v.view_ofs, view );
	mleaf_t* leaf = Mod_PointInLeaf( view, sv.worldmodel );
	PVSMark( sv.worldmodel, Mod_LeafPVS( leaf, sv.worldmodel ) );

	edict_t* chain = sv.edicts;
	for ( int index = 1; index < sv.num_edicts; index++ )
	{
		edict_t* entity = &sv.edicts[index];
		if ( entity->free )
			continue;
		edict_t* bounds = entity;
		if ( entity->v.movetype == MOVETYPE_FOLLOW && entity->v.aiment )
			bounds = entity->v.aiment;
		if ( PVSNode( sv.worldmodel->nodes, bounds->v.absmin, bounds->v.absmax ) )
		{
			entity->v.chain = chain;
			chain = entity;
		}
	}
	if ( cl.worldmodel )
	{
		r_oldviewleaf = NULL;
		R_MarkLeaves();
	}
	return chain;
}

void EV_PlayReliableEvent( client_t* client, int entityIndex, unsigned short eventIndex,
	float delay, event_args_t* args )
{
	if ( client->fakeclient )
		return;

	byte data[RELIABLE_EVENT_BUFFER_BYTES];
	sizebuf_t message = {};
	message.buffername = "Reliable Event";
	message.data = data;
	message.maxsize = sizeof( data );
	ASSERT_AT( g_peventdelta, 1553 );
	event_args_t emptyArgs = {};
	event_args_t eventArgs = *args;
	eventArgs.entindex = entityIndex;

	MSG_WriteByte( &message, svc_event_reliable );
	MSG_StartBitWriting( &message );
	MSG_WriteBits( eventIndex, EVENT_INDEX_BITS );
	DELTA_WriteDelta( (byte*)&emptyArgs, (byte*)&eventArgs, TRUE, g_peventdelta, NULL );
	MSG_WriteBits( delay != 0.0f, 1 );
	if ( delay != 0.0f )
		MSG_WriteBits( (unsigned int)(int)( delay * 100.0 ), EVENT_DELAY_BITS );
	MSG_EndBitWriting( &message );
	if ( message.cursize + client->netchan.message.cursize <= client->netchan.message.maxsize )
		SZ_Write( &client->netchan.message, message.data, message.cursize );
	else
		Netchan_CreateFragments( TRUE, &client->netchan, &message );
}

void EV_Playback( int flags, const edict_t* invoker, unsigned short eventIndex, float delay,
	float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	if ( flags & FEV_CLIENT )
		return;

	event_args_t args = {};
	if ( !VectorCompare( origin, vec3_origin ) )
	{
		args.flags |= FEVENT_ORIGIN;
		VectorCopy( origin, args.origin );
	}
	if ( !VectorCompare( angles, vec3_origin ) )
	{
		args.flags |= FEVENT_ANGLES;
		VectorCopy( angles, args.angles );
	}
	args.fparam1 = fparam1;
	args.fparam2 = fparam2;
	args.iparam1 = iparam1;
	args.iparam2 = iparam2;
	args.bparam1 = bparam1;
	args.bparam2 = bparam2;
	if ( !eventIndex || eventIndex >= MAX_EVENT_SCRIPTS )
	{
		Con_DPrintf( "EV_Playback:  index out of range %i\n", eventIndex );
		return;
	}
	if ( !sv.event_precache[eventIndex].pszScript )
	{
		Con_DPrintf( "EV_Playback:  no event for index %i\n", eventIndex );
		return;
	}

	int invokerIndex = -1;
	vec3_t eventOrigin;
	if ( invoker )
	{
		VectorCopy( invoker->v.origin, eventOrigin );
		invokerIndex = NUM_FOR_EDICT( invoker );
		if ( invokerIndex > 0 && invokerIndex <= svs.maxclients && ( invoker->v.flags & FL_DUCKING ) )
			args.ducking = TRUE;
		if ( !( args.flags & FEVENT_ORIGIN ) )
			VectorCopy( invoker->v.origin, args.origin );
		if ( !( args.flags & FEVENT_ANGLES ) )
			VectorCopy( invoker->v.angles, args.angles );
	}
	else
		VectorCopy( args.origin, eventOrigin );
	int leaf = SV_PointLeafnum( eventOrigin );

	for ( int index = 0; index < svs.maxclients; index++ )
	{
		client_t* client = &svs.clients[index];
		if ( !client->active || !client->spawned || !client->connected || !client->fully_connected || client->fakeclient )
			continue;
		if ( invoker )
		{
			if ( invoker->v.groupinfo && client->edict->v.groupinfo )
			{
				int commonGroups = invoker->v.groupinfo & client->edict->v.groupinfo;
				if ( g_groupop == GROUP_OP_AND && !commonGroups )
					continue;
				if ( g_groupop == GROUP_OP_NAND && commonGroups )
					continue;
			}
			if ( !( flags & FEV_GLOBAL ) && !SV_ValidClientMulticast( client, leaf, EVENT_MULTICAST_PAS ) )
				continue;
		}
		if ( client == host_client && ( flags & FEV_NOTHOST ) && client->lw )
			continue;
		if ( ( flags & FEV_HOSTONLY ) && invoker != client->edict )
			continue;

		if ( flags & FEV_RELIABLE )
		{
			EV_PlayReliableEvent( client, invoker ? NUM_FOR_EDICT( invoker ) : 0, eventIndex, delay, &args );
			continue;
		}

		event_info_t* event = NULL;
		if ( flags & FEV_UPDATE )
		{
			for ( int slot = 0; slot < MAX_EVENT_QUEUE; slot++ )
			{
				event_info_t* queued = &client->events.ei[slot];
				if ( queued->index == eventIndex && invokerIndex != -1 && queued->entity_index == invokerIndex )
				{
					event = queued;
					break;
				}
			}
		}
		if ( !event )
		{
			for ( int slot = 0; slot < MAX_EVENT_QUEUE; slot++ )
			{
				if ( !client->events.ei[slot].index )
				{
					event = &client->events.ei[slot];
					break;
				}
			}
		}
		if ( !event )
			continue;
		event->packet_index = -1;
		event->index = eventIndex;
		event->entity_index = (short)invokerIndex;
		event->args = args;
		event->fire_time = delay;
	}
}

void EV_SV_Playback( int flags, int clientIndex, unsigned short eventIndex, float delay,
	float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	if ( flags & FEV_CLIENT )
		return;
	if ( clientIndex < 0 || clientIndex >= svs.maxclients )
		Host_Error( "EV_SV_Playback:  Client index %i out of range\n", clientIndex );
	EV_Playback( flags, svs.clients[clientIndex].edict, eventIndex, delay,
		origin, angles, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2 );
}
