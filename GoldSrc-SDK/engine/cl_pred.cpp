#include "engine_apiproxy.h"
#include "client.h"
#include "conprint.h"
#include "mathlib.h"
#include "common.h"
#include "host.h"
#include "pm_defs.h"
#include "sys.h"
#include "../common/con_nprint.h"
#include "../pm_shared/pm_movevars.h"

#include <cmath>
#include <cstring>

#define MAX_FORWARD 6

extern playermove_t g_clmove;
extern cvar_t cl_idealpitchscale;

cvar_t cl_showerror = { "cl_showerror", "0" };
cvar_t cl_nosmooth = { "cl_nosmooth", "0" };
cvar_t cl_smoothtime = { "cl_smoothtime", "0.1", FCVAR_ARCHIVE };

static double cl_correction_time;
static int oldphysent;
static int oldvisent;
static int pushed;
static int g_lastground = -1;
int g_lastupdate_sequence = -1;
int g_i;

void CL_CheckPredictionError()
{
	static int pos;
	int frame, cmd;
	vec3_t delta;
	float len;

	frame = cl.parsecountmod;
	cmd = cls.netchan.incoming_acknowledged & CL_UPDATE_MASK;

	VectorSubtract( cl.frames[frame].playerstate[cl.playernum].origin, cl.predicted_origins[cmd], delta );

	len = Length( delta );
	if ( len > 64 )
	{
		VectorClear( cl.prediction_error );
		return;
	}

	if ( cl_showerror.value && len > 0.5f )
	{
		pos++;
		Con_NPrintf( ( pos & 3 ) + 10, "pred. error %.3f units", len );
	}

	VectorCopy( cl.frames[frame].playerstate[cl.playernum].origin, cl.predicted_origins[cmd] );
	VectorCopy( delta, cl.prediction_error );

	if ( len > 0.25f && cl.maxclients != 1 )
		cl_correction_time = cl_smoothtime.value;
}

/*
==============
CL_InitPrediction
==============
*/
void CL_InitPrediction()
{
	Cvar_RegisterVariable( &cl_smoothtime );
	Cvar_RegisterVariable( &cl_nosmooth );
	Cvar_RegisterVariable( &cl_showerror );
}

void CL_PushPMStates()
{
	if ( pushed )
	{
		Con_Printf( "CL_PushPMStates called with pushed stack\n" );
		return;
	}

	pushed = 1;
	oldphysent = pmove->numphysent;
	oldvisent = pmove->numvisent;
}

void CL_PopPMStates()
{
	if ( !pushed )
	{
		Con_Printf( "CL_PopPMStates called without stack\n" );
		return;
	}

	pushed--;
	pmove->numphysent = oldphysent;
	pmove->numvisent = oldvisent;
}

void CL_SetLastUpdate()
{
	g_lastupdate_sequence = cls.netchan.incoming_sequence;
}

void CL_RunUsercmd(
	local_state_t* from, local_state_t* to, usercmd_t* u, qboolean runfuncs, double* elapsed, unsigned int randomSeed )
{
	usercmd_t cmd;

	cmd = *u;

	// split up very long moves
	if ( u->msec > 50 )
	{
		local_state_t temp;

		cmd.msec /= 2;
		CL_RunUsercmd( from, &temp, &cmd, runfuncs, elapsed, randomSeed );

		cmd.impulse = 0;
		cmd.weaponselect = 0;
		CL_RunUsercmd( &temp, to, &cmd, runfuncs, elapsed, randomSeed );
		return;
	}

	pmove->time = (float)( *elapsed * 1000.0 );
	pmove->server = FALSE;
	pmove->multiplayer = cl.maxclients > 1;
	pmove->runfuncs = runfuncs;
	pmove->frametime = (float)( cmd.msec / 1000.0 );

	memcpy( to, from, sizeof( *to ) );

	pmove->player_index = from->playerstate.number - 1;
	pmove->flags = from->client.flags;
	pmove->bInDuck = from->client.bInDuck;
	pmove->flTimeStepSound = from->client.flTimeStepSound;
	pmove->flDuckTime = (float)from->client.flDuckTime;
	pmove->flSwimTime = (float)from->client.flSwimTime;
	pmove->waterjumptime = (float)from->client.waterjumptime;
	pmove->waterlevel = from->client.waterlevel;
	pmove->watertype = from->client.watertype;
	pmove->onground = from->client.flags & FL_ONGROUND;
	pmove->deadflag = from->client.deadflag;
	VectorCopy( from->client.velocity, pmove->velocity );
	VectorCopy( from->client.view_ofs, pmove->view_ofs );
	VectorCopy( from->playerstate.origin, pmove->origin );
	VectorCopy( from->playerstate.basevelocity, pmove->basevelocity );
	VectorCopy( from->client.punchangle, pmove->punchangle );
	VectorCopy( from->playerstate.angles, pmove->angles );
	VectorCopy( from->playerstate.angles, pmove->oldangles );
	pmove->friction = from->playerstate.friction;
	pmove->usehull = from->playerstate.usehull;
	pmove->oldbuttons = from->playerstate.oldbuttons;
	pmove->dead = cl.stats[STAT_HEALTH] <= 0;
	pmove->spectator = cls.spectator != FALSE;
	pmove->movetype = from->playerstate.movetype;
	pmove->gravity = from->playerstate.gravity;
	pmove->effects = from->playerstate.effects;
	pmove->iStepLeft = from->playerstate.iStepLeft;
	pmove->flFallVelocity = from->playerstate.flFallVelocity;

	Q_strncpy( pmove->physinfo, cls.physinfo, sizeof( pmove->physinfo ) - 1 );
	pmove->physinfo[sizeof( pmove->physinfo ) - 1] = 0;

	pmove->maxspeed = movevars.maxspeed;
	pmove->clientmaxspeed = from->client.maxspeed;
	pmove->cmd = cmd;

	pmove->iuser1 = from->client.iuser1;
	pmove->iuser2 = from->client.iuser2;
	pmove->iuser3 = from->client.iuser3;
	pmove->iuser4 = from->client.iuser4;
	pmove->fuser1 = from->client.fuser1;
	pmove->fuser2 = from->client.fuser2;
	pmove->fuser3 = from->client.fuser3;
	pmove->fuser4 = from->client.fuser4;
	VectorCopy( from->client.vuser1, pmove->vuser1 );
	VectorCopy( from->client.vuser2, pmove->vuser2 );
	VectorCopy( from->client.vuser3, pmove->vuser3 );
	VectorCopy( from->client.vuser4, pmove->vuser4 );

	pmove->PM_PlaySound = PM_CL_PlaySound;
	pmove->PM_TraceTexture = PM_CL_TraceTexture;
	pmove->PM_PlaybackEventFull = PM_CL_PlaybackEventFull;

	ClientDLL_MoveClient( pmove );

	g_lastground = pmove->onground;
	if ( g_lastground > 0 )
		g_lastground = pmove->physents[g_lastground].info;

	to->client.flags = pmove->flags;
	to->client.bInDuck = pmove->bInDuck;
	to->client.flTimeStepSound = pmove->flTimeStepSound;
	to->client.flDuckTime = (int)pmove->flDuckTime;
	to->client.flSwimTime = (int)pmove->flSwimTime;
	to->client.waterjumptime = (int)pmove->waterjumptime;
	to->client.waterlevel = pmove->waterlevel;
	to->client.watertype = pmove->watertype;
	to->client.maxspeed = pmove->clientmaxspeed;
	to->client.deadflag = pmove->deadflag;
	VectorCopy( pmove->velocity, to->client.velocity );
	VectorCopy( pmove->view_ofs, to->client.view_ofs );
	VectorCopy( pmove->origin, to->playerstate.origin );
	VectorCopy( pmove->basevelocity, to->playerstate.basevelocity );
	VectorCopy( pmove->punchangle, to->client.punchangle );
	to->playerstate.oldbuttons = pmove->cmd.buttons;
	to->playerstate.friction = pmove->friction;
	to->playerstate.movetype = pmove->movetype;
	to->playerstate.effects = pmove->effects;
	to->playerstate.usehull = pmove->usehull;
	to->playerstate.iStepLeft = pmove->iStepLeft;
	to->playerstate.flFallVelocity = pmove->flFallVelocity;

	to->client.iuser1 = pmove->iuser1;
	to->client.iuser2 = pmove->iuser2;
	to->client.iuser3 = pmove->iuser3;
	to->client.iuser4 = pmove->iuser4;
	to->client.fuser1 = pmove->fuser1;
	to->client.fuser2 = pmove->fuser2;
	to->client.fuser3 = pmove->fuser3;
	to->client.fuser4 = pmove->fuser4;
	VectorCopy( pmove->vuser1, to->client.vuser1 );
	VectorCopy( pmove->vuser2, to->client.vuser2 );
	VectorCopy( pmove->vuser3, to->client.vuser3 );
	VectorCopy( pmove->vuser4, to->client.vuser4 );

	ClientDLL_PostRunCmd( from, to, &cmd, runfuncs, *elapsed, randomSeed );

	*elapsed += cmd.msec / 1000.0;
}

void CL_GetTargetTime( float* target )
{
	*target = (float)realtime;
}

void CL_InterpolateVector( float frac, vec_t* src, vec_t* dest, vec_t* output )
{
	int i;

	for ( i = 0; i < 3; i++ )
		output[i] = src[i] + frac * ( dest[i] - src[i] );
}

void CL_SetIdealPitch()
{
	float angleval, sinval, cosval;
	pmtrace_t tr;
	vec3_t top, bottom;
	float z[MAX_FORWARD];
	int i, j;
	int step, dir, steps;

	if ( cl.onground == -1 )
		return;

	angleval = (float)( cl.simangles[YAW] * M_PI * 2 / 360 );
	sinval = (float)sin( (double)angleval );
	cosval = (float)cos( (double)angleval );

	for ( i = 0; i < MAX_FORWARD; i++ )
	{
		top[0] = cl.simorg[0] + cosval * ( i + 3 ) * 12;
		top[1] = cl.simorg[1] + sinval * ( i + 3 ) * 12;
		top[2] = cl.simorg[2] + cl.viewheight[2];

		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;

		tr = PM_PlayerTrace( top, bottom, PM_STUDIO_BOX, -1 );
		if ( tr.allsolid )
			return; // looking at a wall, leave ideal the way it was

		if ( tr.fraction == 1 )
			return; // near a dropoff

		z[i] = top[2] + tr.fraction * ( bottom[2] - top[2] );
	}

	dir = 0;
	steps = 0;
	for ( j = 1; j < MAX_FORWARD; j++ )
	{
		step = (int)( z[j] - z[j - 1] );
		if ( !step )
			continue;

		if ( dir && step != dir )
			return; // mixed changes

		steps++;
		dir = step;
	}

	if ( !dir )
	{
		cl.idealpitch = 0;
		return;
	}

	if ( steps < 2 )
		return;

	cl.idealpitch = -dir * cl_idealpitchscale.value;
}

void CL_PredictMove( qboolean repredicting )
{
	static vec3_t lastsimorg;
	int i, seq;
	float f, targettime;
	double elapsed;
	qboolean runfuncs, teleported;
	frame_t* frame;
	local_state_t *from, *to;
	cmd_t *fromcmd, *tocmd;
	cl_entity_t* ground;
	vec3_t delta;

	if ( cls.state != ca_active || cls.spectator )
		return;

	pmove = &g_clmove;
	CL_SetUpPlayerPrediction( FALSE, FALSE );

	CL_GetTargetTime( &targettime );

	if ( cl.intermission || !cl.validsequence )
		return;

	if ( cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged + 1 >= CL_UPDATE_MASK )
		return;

	VectorCopy( cl.viewangles, cl.simangles );

	if ( cls.demoplayback )
		return;

	// this is the last frame received from the server
	frame = &cl.frames[cl.parsecountmod];

	from = &cl.predicted_frames[cl.parsecountmod];
	from->playerstate = frame->playerstate[cl.playernum];
	from->client = frame->clientdata;
	Q_memcpy( from->weapondata, frame->weapondata, (int)sizeof( from->weapondata ) );

	if ( frame->invalid )
		return;

	elapsed = frame->time;
	cl.onground = -1;

	fromcmd = &cl.commands[cls.netchan.incoming_acknowledged & CL_UPDATE_MASK];
	tocmd = NULL;
	to = NULL;

	CL_PushPMStates();
	CL_SetSolidPlayers( cl.playernum );

	// predict forward until cl.time <= to->senttime
	for ( i = 1; i < CL_UPDATE_BACKUP - 1; i++ )
	{
		seq = cls.netchan.incoming_acknowledged + i;
		if ( seq >= cls.netchan.outgoing_sequence + !repredicting )
			break;

		tocmd = &cl.commands[seq & CL_UPDATE_MASK];
		to = &cl.predicted_frames[( cl.parsecountmod + i ) & CL_UPDATE_MASK];

		runfuncs = !repredicting && !tocmd->processedfuncs;
		CL_RunUsercmd( from, to, &tocmd->cmd, runfuncs, &elapsed, (unsigned int)seq );
		tocmd->processedfuncs = TRUE;

		VectorCopy( to->playerstate.origin, cl.predicted_origins[seq & CL_UPDATE_MASK] );

		if ( tocmd->senttime >= targettime )
			break;

		from = to;
		fromcmd = tocmd;
	}

	g_i = i;

	CL_PopPMStates();

	if ( i >= CL_UPDATE_MASK )
		return;

	if ( !to )
	{
		if ( !repredicting )
			return; // net hasn't deliver packets in a long time...

		to = from;
		tocmd = fromcmd;
	}

	// now interpolate some fraction of the final frame
	if ( tocmd->senttime == fromcmd->senttime )
	{
		f = 0;
	}
	else
	{
		f = ( targettime - fromcmd->senttime ) / ( tocmd->senttime - fromcmd->senttime );
		if ( f < 0 )
			f = 0;
	}

	teleported = FALSE;
	for ( i = 0; i < 3; i++ )
	{
		if ( fabs( to->playerstate.origin[i] - from->playerstate.origin[i] ) > 128 )
		{
			teleported = TRUE;
			break;
		}
	}

	if ( teleported )
	{
		// teleported, so don't lerp
		VectorCopy( to->client.velocity, cl.simvel );
		VectorCopy( to->playerstate.origin, cl.simorg );
		VectorCopy( to->client.punchangle, cl.punchangle );
		VectorCopy( to->client.view_ofs, cl.viewheight );
	}
	else
	{
		for ( i = 0; i < 3; i++ )
		{
			cl.simorg[i] = from->playerstate.origin[i]
				+ f * ( to->playerstate.origin[i] - from->playerstate.origin[i] );
			cl.simvel[i] = from->client.velocity[i] + f * ( to->client.velocity[i] - from->client.velocity[i] );
			cl.punchangle[i] = from->client.punchangle[i]
				+ f * ( to->client.punchangle[i] - from->client.punchangle[i] );

			if ( from->playerstate.usehull == to->playerstate.usehull )
				cl.viewheight[i] = from->client.view_ofs[i] + f * ( to->client.view_ofs[i] - from->client.view_ofs[i] );
			else
				cl.viewheight[i] = to->client.view_ofs[i];
		}
	}

	cl.onground = ( to->client.flags & FL_ONGROUND ) ? g_lastground : -1;
	cl.waterlevel = to->client.waterlevel;
	cl.usehull = to->playerstate.usehull;
	cl.stats[STAT_WEAPON] = to->client.viewmodel;

	cl.moving = 0;
	if ( cl.onground > 0 && cl.onground < cl.num_entities )
	{
		ground = &cl_entities[cl.onground];
		VectorSubtract( ground->curstate.origin, ground->prevstate.origin, delta );
		delta[2] = 0;
		if ( Length( delta ) > 0 )
			cl.moving = 1;
	}

	if ( cl.moving )
	{
		cl_correction_time = 0;
	}
	else if ( cl_correction_time > 0 && !cl_nosmooth.value && cl_smoothtime.value )
	{
		if ( !repredicting )
			cl_correction_time -= host_frametime;

		if ( cl_smoothtime.value <= 0 )
			Cvar_DirectSet( &cl_smoothtime, "0.1" );

		if ( cl_correction_time < 0 )
			cl_correction_time = 0;

		if ( !( cl_smoothtime.value > cl_correction_time ) )
			cl_correction_time = cl_smoothtime.value;

		VectorSubtract( cl.simorg, lastsimorg, delta );
		f = (float)( cl_correction_time / cl_smoothtime.value );
		VectorScale( delta, (float)( 1.0 - f ), delta );
		VectorAdd( lastsimorg, delta, cl.simorg );
	}

	VectorCopy( cl.simorg, lastsimorg );

	CL_SetIdealPitch();
}

void CL_RedoPrediction()
{
	if ( cls.netchan.incoming_sequence != g_lastupdate_sequence )
	{
		CL_PredictMove( TRUE );
		CL_CheckPredictionError();
	}
}
