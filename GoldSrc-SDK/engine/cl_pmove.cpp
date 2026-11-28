#include "client.h"
#include "cmodel.h"
#include "cl_demo.h"
#include "common.h"
#include "gl_model.h"
#include "mathlib.h"
#include "pm_defs.h"
#include "protocol.h"
#include "sound.h"
#include "../common/event_flags.h"

#include <cstdio>

msurface_t* SurfaceAtPoint( model_t* model, mnode_t* node, vec_t* start, vec_t* end );

float g_laststepgap;

void PM_CL_PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, float* origin,
	float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	event_args_t args;

	if ( flags & FEV_SERVER )
		return;

	Q_memset( &args, 0, sizeof( args ) );

	VectorCopy( origin, args.origin );
	VectorCopy( angles, args.angles );
	args.fparam1 = fparam1;
	args.fparam2 = fparam2;
	args.iparam1 = iparam1;
	args.iparam2 = iparam2;
	args.bparam1 = bparam1;
	args.bparam2 = bparam2;

	CL_ClientQueueEvent( flags, eventindex, clientindex, delay, &args );
}

const char* PM_CL_TraceTexture( int ground, float* vstart, float* vend )
{
	physent_t* pe;
	model_t* pmodel;
	msurface_t* psurf;
	int firstnode;
	vec3_t start, end, offset, temp;
	vec3_t forward, right, up;

	if ( ground < 0 || ground >= pmove->numphysent )
		return NULL;

	pe = &pmove->physents[ground];
	pmodel = pe->model;
	if ( !pmodel )
		return NULL;

	firstnode = 0;

	if ( ground > 0 )
	{
		VectorAdd( pmodel->hulls[0].clip_mins, pe->origin, offset );
		VectorSubtract( vstart, offset, start );
		VectorSubtract( vend, offset, end );

		firstnode = pmodel->hulls[0].firstclipnode;

		if ( pe->angles[0] || pe->angles[1] || pe->angles[2] )
		{
			AngleVectors( pe->angles, forward, right, up );

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
		VectorCopy( vstart, start );
		VectorCopy( vend, end );
	}

	mnode_t* node = Mod_DrawingHullNode( pmodel, firstnode );
	if ( !node )
		return NULL;

	psurf = SurfaceAtPoint( pmodel, node, start, end );
	if ( !psurf )
		return NULL;

	return psurf->texinfo->texture->name;
}

void PM_CL_PlaySound( int channel, const char* sample, float volume, float attenuation, int flags, int pitch )
{
	static float last;
	sfx_t sentence;
	sfx_t* sfx;

	if ( !pmove->runfuncs )
		return;

	g_laststepgap = (float)( cl.time - last );
	last = (float)cl.time;

	if ( flags & SND_SENTENCE )
	{
		snprintf( sentence.name, sizeof( sentence.name ), "!%s", rgpszrawsentence[Q_atoi( sample + 1 )] );
		sfx = &sentence;
	}
	else
	{
		sfx = CL_LookupSound( sample );
	}

	if ( cls.demorecording )
		CL_DemoQueueSound( channel, sample, volume, attenuation, flags, pitch );

	if ( channel == CHAN_STATIC )
		S_StartStaticSound( pmove->player_index + 1, channel, sfx, pmove->origin, volume, attenuation, flags, pitch );
	else
		S_StartDynamicSound( pmove->player_index + 1, channel, sfx, pmove->origin, volume, attenuation, flags, pitch );
}
