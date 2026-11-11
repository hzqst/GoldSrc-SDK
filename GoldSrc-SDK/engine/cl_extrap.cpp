#include "client.h"
#include "common.h"
#include "cvar.h"
#include "mathlib.h"

cvar_t ex_interp = { "ex_interp", "0.1", FCVAR_ARCHIVE | FCVAR_FILTERSTUFFTEXT };

void CL_InitExtrap( void )
{
	Cvar_RegisterVariable( &ex_interp );
}

void CL_UpdatePositions( cl_entity_t* ent )
{
	position_history_t* ph;

	ent->current_position = ( ent->current_position + 1 ) & HISTORY_MASK;

	ph = &ent->ph[ent->current_position];
	VectorCopy( ent->curstate.angles, ph->angles );
	VectorCopy( ent->curstate.origin, ph->origin );
	ph->animtime = ent->curstate.animtime;
}

void CL_ResetPositions( cl_entity_t* ent )
{
	position_history_t store;

	if ( !ent )
		return;

	store = ent->ph[ent->current_position];

	ent->current_position = 1;
	Q_memset( ent->ph, 0, sizeof( ent->ph ) );
	ent->ph[1] = store;
	ent->ph[0] = store;
}

qboolean CL_FindInterpolationUpdates(
	cl_entity_t* ent, float targettime, position_history_t** ph0, position_history_t** ph1, int* ph0Index )
{
	qboolean extrapolate;
	int i, i0, i1, imod;
	float at;

	imod = ent->current_position;
	i0 = imod & HISTORY_MASK;
	i1 = ( imod - 1 ) & HISTORY_MASK;
	int oldest = i0;

	extrapolate = TRUE;

	if ( ent->ph[i0].animtime >= targettime )
	{
		for ( i = 1; i < HISTORY_MAX; i++ )
		{
			at = ent->ph[( imod - i ) & HISTORY_MASK].animtime;
			if ( at == 0 )
				break;
			oldest = ( imod - i ) & HISTORY_MASK;

			if ( !( at >= targettime ) )
			{
				i0 = ( imod - i + 1 ) & HISTORY_MASK;
				i1 = ( imod - i ) & HISTORY_MASK;
				extrapolate = FALSE;
				break;
			}
		}
	}

	if ( extrapolate && ent->ph[oldest].animtime != 0.0f
		 && ( targettime <= ent->ph[oldest].animtime || ent->ph[i1].animtime == 0.0f ) )
	{
		i0 = i1 = oldest;
		extrapolate = FALSE;
	}

	if ( ph0 )
		*ph0 = &ent->ph[i0];
	if ( ph1 )
		*ph1 = &ent->ph[i1];
	if ( ph0Index )
		*ph0Index = i0;

	return extrapolate;
}

void CL_PureOrigin( cl_entity_t* ent, float targettime, vec_t* outorigin, vec_t* outangles )
{
	position_history_t* ph0;
	position_history_t* ph1;
	vec3_t delta, origin, angles;
	float frac;

	CL_FindInterpolationUpdates( ent, targettime, &ph0, &ph1, NULL );
	if ( !ph0 || !ph1 )
		return;

	if ( ph0->animtime == 0 )
	{
		VectorCopy( ph1->origin, outorigin );
		VectorCopy( ph1->angles, outangles );
		return;
	}

	VectorSubtract( ph0->origin, ph1->origin, delta );

	frac = 1.0f;
	if ( ph0->animtime != ph1->animtime )
		frac = ( targettime - ph1->animtime ) / ( ph0->animtime - ph1->animtime );

	if ( frac < 0 )
		frac = 0;
	else if ( frac > 1.2 )
		frac = (float)1.2;

	VectorMA( ph1->origin, frac, delta, origin );
	InterpolateAngles( ph1->angles, ph0->angles, angles, frac );

	VectorCopy( origin, outorigin );
	VectorCopy( angles, outangles );
}

void CL_ComputePlayerOrigin( cl_entity_t* ent )
{
	if ( !ent->player || ent->index - 1 == cl.playernum )
		return;

	CL_InterpolateEntity( ent );
}

void CL_InterpolateEntity( cl_entity_t* ent )
{
	vec3_t origin, angles;

	CL_PureOrigin( ent, (float)( cl.time - ex_interp.value ), origin, angles );

	VectorCopy( angles, ent->angles );
	VectorCopy( origin, ent->origin );
}
