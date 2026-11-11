#include "beam_color.h"

#if defined( SW_RENDERER )

#include "r_local.h"
#include "d_local.h"
#include "client.h"
#include "common.h"
#include "mathlib.h"
#include "zone.h"
#include "conprint.h"
#include "engine_color.h"
#include "render.h"
#include "cvar.h"
#include "enginecallbacks.h"
#include "../common/triangleapi.h"
#include "customentity.h"
#include "../pm_shared/pm_movevars.h"
#include "../common/beamdef.h"
#include "../common/particledef.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

// Max simultaneous beams
#define MAX_BEAMS 128
#define MAX_PARTICLES 2048 // default max # of particles at one
										//  time
#define ABSOLUTE_MIN_PARTICLES 512 // no fewer than this no matter what's
										//  on the command line

#define NOISE_DIVISIONS 128

extern unsigned short* host_basepal;
extern model_t* cl_sprite_dot;
extern cvar_t tracerRed, tracerGreen, tracerBlue, tracerAlpha;
extern cvar_t tracerLength;
extern cvar_t egon_amplitude;

static void R_TracerDraw();
void R_BeamDrawList();
void R_BeamDraw( BEAM* pbeam, float frametime );
void R_BeamSetup( BEAM* pbeam, vec_t* start, vec_t* end, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed );
void SetBeamAttributes( BEAM* pbeam, float r, float g, float b, float framerate, int startFrame );
void Noise( float* noise, int divs );
cl_entity_t* R_GetBeamAttachmentEntity( int index );
float* R_BeamGetAttachmentPoint( cl_entity_t* ent, int attachment );
int R_BeamCull( vec_t* start, vec_t* end, int pvsOnly );
particle_t* R_TracerParticles( vec_t* org, vec_t* vel, float life );
void R_DrawSegs(
	vec_t* source, vec_t* delta, float width, float scale, float freq, float speed, int segments, int flags );
void R_DrawTorus( vec_t* source, vec_t* delta, float width, float scale, float freq, float speed, int segments );
void R_DrawBeamFollow( BEAM* pbeam );
void R_DrawRing( vec_t* source, vec_t* delta, float width, float amplitude, float freq, float speed, int segments );

static BEAM* gpActiveBeams;
static BEAM* gpFreeBeams;
particle_t* free_particles;
int cl_numbeamentities;
cl_entity_t* cl_beamentities[MAX_BEAMENTS];
particle_t* active_particles;

int ramp1[8] = { 111, 109, 107, 105, 103, 101, 99, 97 };

static BEAM* gBeams;
int r_numparticles;
particle_t* particles;
particle_t* gpActiveTracers;

int ramp2[8] = { 111, 110, 109, 108, 107, 106, 104, 102 };
int ramp3[8] = { 109, 107, 6, 5, 4, 3, 0, 0 };

int gSparkRamp[9] = { 254, 253, 252, 111, 110, 109, 108, 103, 96 };

color24 gTracerColors[12] = {
	{ 255, 255, 255 },
	{ 255, 0, 0 },
	{ 0, 255, 0 },
	{ 0, 0, 255 },
	{ 0, 0, 0 },
	{ 255, 167, 17 },
	{ 255, 130, 90 },
	{ 55, 60, 144 },
	{ 255, 130, 90 },
	{ 255, 140, 90 },
	{ 200, 130, 90 },
	{ 255, 120, 70 },
};
static const float gTracerSize[10] = { 1.5f, 0.5f, 1, 1, 1, 1, 1, 1, 1, 1 };

float gNoise[NOISE_DIVISIONS + 1];

vec3_t avelocities[NUMVERTEXNORMALS];
vec3_t avelocity = { 23, 7, 3 };
vec3_t r_pright, r_pup, r_ppn;
float timescale = 0.01f;
float partstep = 0.01f;
float beamlength = 16;

int R_TriangleFakeTexture( float r, float g, float b, float a )
{
	static byte fakeTex;
	static word fakePal[4] = { 128, 128, 128, 128 };
	fakePal[0] = (byte)(int)( b * a * 255 );
	fakePal[1] = (byte)(int)( g * a * 255 );
	fakePal[2] = (byte)(int)( a * r * 255 );
	fakePal[3] = 0;
	R_TriangleSetTexture( &fakeTex, 1, 1, fakePal );
	return 1;
}

void R_FreeDeadParticles( particle_t** ppparticles )
{
	particle_t* p;

	while ( *ppparticles )
	{
		p = *ppparticles;
		if ( !( cl.time > p->die ) )
		{
			ppparticles = &p->next;
			continue;
		}

		if ( p->deathfunc )
			p->deathfunc( p );
		p->deathfunc = NULL;

		*ppparticles = p->next;
		p->next = free_particles;
		free_particles = p;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Iterates through active list and kills beams associated with deadEntity
// Input  : deadEntity -
//-----------------------------------------------------------------------------
void R_KillDeadBeams( int deadEntity )
{
	BEAM* pbeam;
	BEAM* pnewlist;
	BEAM* pnext;
	particle_t* pHead;

	pbeam = gpActiveBeams; // Old list.
	pnewlist = NULL; // New list.

	while ( pbeam )
	{
		pnext = pbeam->next;
		if ( pbeam->startEntity != deadEntity ) // Link into new list.
		{
			pbeam->next = pnewlist;
			pnewlist = pbeam;

			pbeam = pnext;
			continue;
		}

		pbeam->flags &= ~( FBEAM_STARTENTITY | FBEAM_ENDENTITY );
		if ( pbeam->type != TE_BEAMFOLLOW )
		{
			// Die Die Die!
			pbeam->die = (float)( cl.time - 0.1 );

			// Kill off particles
			for ( pHead = pbeam->particles; pHead; pHead = pHead->next )
				pHead->die = (float)( cl.time - 0.1 );

			// Free particles that have died off.
			R_FreeDeadParticles( &pbeam->particles );

			// Clear us out
			Q_memset( pbeam, 0, sizeof( *pbeam ) );

			// Now link into free list;
			pbeam->next = gpFreeBeams;
			gpFreeBeams = pbeam;
		}
		else
		{
			// Stay active
			pbeam->next = pnewlist;
			pnewlist = pbeam;
		}
		pbeam = pnext;
	}

	// We now have a new list with the bogus stuff released.
	gpActiveBeams = pnewlist;
}

/*
===============
R_RunParticleEffect

===============
*/
void R_RunParticleEffect( vec_t* org, vec_t* dir, int color, int count )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		if ( count == 1024 )
		{
			// rocket explosion
			p->die = (float)( cl.time + 5.0 );
			p->color = (short)ramp1[0];
			p->packedColor = hlRGB( host_basepal, p->color );
			p->ramp = (float)RandomLong( 0, 3 );
			p->type = ( i & 1 ) ? pt_explode : pt_explode2;

			for ( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + RandomFloat( -16, 16 );
				p->vel[j] = RandomFloat( -256, 256 );
			}
		}
		else
		{
			p->die = (float)( cl.time + RandomFloat( 0, 0.4f ) );
			p->color = (short)( ( color & ~7 ) + RandomLong( 0, 7 ) );
			p->packedColor = hlRGB( host_basepal, p->color );
			p->type = pt_slowgrav;

			for ( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + RandomFloat( -8, 8 );
				p->vel[j] = dir[j] * 15;
			}
		}
	}
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect()
{
	vec3_t org, dir;
	int i, count, msgcount, color;

	for ( i = 0; i < 3; i++ )
		org[i] = MSG_ReadCoord( &net_message );
	for ( i = 0; i < 3; i++ )
		dir[i] = (float)( MSG_ReadChar() * ( 1.0 / 16 ) );
	msgcount = MSG_ReadByte();
	color = MSG_ReadByte();

	if ( msgcount == 255 )
		count = 1024;
	else
		count = msgcount;

	R_RunParticleEffect( org, dir, color, count );
}

void R_BeamInit()
{
	gBeams = (BEAM*)Hunk_AllocName( MAX_BEAMS * sizeof( BEAM ), "lightning" );
}

void R_BeamClear()
{
	int i;

	gpActiveBeams = NULL;
	gpFreeBeams = &gBeams[0];

	Q_memset( gBeams, 0, MAX_BEAMS * sizeof( BEAM ) );

	for ( i = 0; i < MAX_BEAMS; i++ )
		gBeams[i].next = &gBeams[i + 1];
	gBeams[MAX_BEAMS - 1].next = NULL;
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles()
{
	int i;

	i = COM_CheckParm( "-particles" );
	if ( i )
		r_numparticles = Q_atoi( com_argv[i + 1] );
	else
		r_numparticles = MAX_PARTICLES;

	if ( r_numparticles < ABSOLUTE_MIN_PARTICLES )
		r_numparticles = ABSOLUTE_MIN_PARTICLES;

	particles = (particle_t*)Hunk_AllocName( r_numparticles * sizeof( particle_t ), "particles" );

	R_BeamInit();
}

/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles()
{
	int i;

	free_particles = &particles[0];
	active_particles = NULL;
	gpActiveTracers = NULL;

	for ( i = 0; i < r_numparticles; i++ )
		particles[i].next = &particles[i + 1];
	particles[r_numparticles - 1].next = NULL;

	R_BeamClear();
}

particle_t* R_AllocParticle( void ( *callback )( particle_t* particle, float frametime ) )
{
	particle_t* p;

	if ( !free_particles )
	{
		Con_Printf( "Not enough free particles\n" );
		return NULL;
	}

	p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;

	p->type = pt_clientcustom;
	p->callback = callback;
	p->color = 0;
	p->deathfunc = NULL;
	p->die = (float)cl.time;
	VectorCopy( vec3_origin, p->org );
	VectorCopy( vec3_origin, p->vel );
	p->packedColor = 0;
	p->ramp = 0;

	return p;
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles()
{
	particle_t* p;
	float grav;
	int i;
	float time1, time2, time3;
	float dvel;
	float frametime;
	D_StartParticles();
	VectorScale( vright, xscaleshrink, r_pright );
	VectorScale( vup, yscaleshrink, r_pup );
	VectorCopy( vpn, r_ppn );

	frametime = (float)( cl.time - cl.oldtime );
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = (float)( frametime * movevars.gravity * 0.05 );
	dvel = frametime * 4;

	R_FreeDeadParticles( &active_particles );

	for ( p = active_particles; p; p = p->next )
	{
		if ( p->type != pt_blob )
			D_DrawParticle( p );

		if ( p->type != pt_clientcustom )
		{
			for ( i = 0; i < 3; i++ )
				p->org[i] += p->vel[i] * frametime;
		}

		switch ( p->type )
		{
		case pt_grav:
			p->vel[2] -= grav * 20;
			break;

		case pt_slowgrav:
			p->vel[2] = grav;
			break;

		case pt_fire:
			p->ramp += time1;

			if ( p->ramp < 6 )
			{
				p->color = (short)ramp3[(int)p->ramp];
				p->packedColor = hlRGB( host_basepal, p->color );
			}
			else
			{
				p->die = -1;
			}

			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;

			if ( p->ramp < 8 )
			{
				p->color = (short)ramp1[(int)p->ramp];
				p->packedColor = hlRGB( host_basepal, p->color );
			}
			else
			{
				p->die = -1;
			}

			for ( i = 0; i < 3; i++ )
				p->vel[i] += p->vel[i] * dvel;

			p->vel[2] -= grav;
			break;

		case pt_explode2:
			p->ramp += time3;

			if ( p->ramp < 8 )
			{
				p->color = (short)ramp2[(int)p->ramp];
				p->packedColor = hlRGB( host_basepal, p->color );
			}
			else
			{
				p->die = -1;
			}

			for ( i = 0; i < 3; i++ )
				p->vel[i] -= p->vel[i] * frametime;

			p->vel[2] -= grav;
			break;

		case pt_blob:
		case pt_blob2:
			p->ramp += time2;

			if ( p->ramp >= 9 )
				p->ramp = 0;

			p->color = (short)gSparkRamp[(int)p->ramp];
			p->packedColor = hlRGB( host_basepal, p->color );

			p->vel[0] = (float)( p->vel[0] - p->vel[0] * 0.5 * frametime );
			p->vel[1] = (float)( p->vel[1] - p->vel[1] * 0.5 * frametime );
			p->vel[2] -= grav * 5;

			if ( RandomLong( 0, 3 ) )
				p->type = pt_blob;
			else
				p->type = pt_blob2;
			break;

		case pt_vox_slowgrav:
			p->vel[2] -= grav * 4;
			break;

		case pt_vox_grav:
			p->vel[2] -= grav * 8;
			break;

		case pt_clientcustom:
			if ( p->callback )
				p->callback( p, frametime );
			break;

		default:
			break;
		}
	}

	R_TracerDraw();
	R_BeamDrawList();
	D_EndParticles();
}

int ScreenTransform( vec_t* point, vec_t* screen )
{
	float w;
	vec3_t local;
	VectorSubtract( point, r_origin, local );
	TransformVector( local, screen );
	w = screen[2];

	if ( w != 0.0f )
	{
		w = (float)( 1.0 / w );
		screen[0] *= w;
		screen[1] *= w;
	}

	return w <= 0.0f;
}

void WorldTransform( vec_t*, vec_t* )
{
}

static void R_TracerDraw()
{
	int i;
	float scale;
	float attenuation;
	float frametime;
	float clipDist;
	float gravity;
	float size;
	float fraction, dist1, dist2;
	vec3_t start, end;
	particle_t* p;
	vec3_t screenLast, screen;
	vec3_t tmp, normal;
	color24* pColor;
	qboolean draw;

	if ( !gpActiveTracers )
		return;

	gTracerColors[4].r = (byte)(int)( tracerRed.value * tracerAlpha.value * 255 );
	gTracerColors[4].g = (byte)(int)( tracerGreen.value * tracerAlpha.value * 255 );
	gTracerColors[4].b = (byte)(int)( tracerBlue.value * tracerAlpha.value * 255 );

	R_FreeDeadParticles( &gpActiveTracers );

	frametime = (float)( cl.time - cl.oldtime );

	if ( !tri.SpriteTexture( cl_sprite_dot, 0 ) )
		return;

	gravity = frametime * movevars.gravity;
	size = DotProduct( r_origin, vpn );
	scale = fmaxf( 0.0f, (float)( 1.0 - frametime * 0.9 ) );

	tri.RenderMode( kRenderTransAdd );
	tri.CullFace( TRI_NONE );

	for ( p = gpActiveTracers; p; p = p->next )
	{
		pColor = &gTracerColors[p->color];

		if ( p->die - 0.1 <= cl.time )
			attenuation = (float)( ( p->die - cl.time ) * p->ramp );
		else
			attenuation = (float)( p->ramp * 0.1 );

		VectorCopy( p->org, start );
		VectorMA( p->org, attenuation, p->vel, end );

		draw = TRUE;

		if ( ScreenTransform( start, screen ) || ScreenTransform( end, screenLast ) )
		{
			dist1 = DotProduct( start, vpn ) - size;
			dist2 = DotProduct( end, vpn ) - size;
			clipDist = dist2 - dist1;

			if ( ( dist1 > 0 || dist2 > 0 ) && clipDist >= 0.01 )
			{
				fraction = dist1 / clipDist;

				if ( dist1 > 0 )
				{
					for ( i = 0; i < 3; i++ )
						end[i] = start[i] + ( end[i] - start[i] ) * fraction;
				}
				else
				{
					for ( i = 0; i < 3; i++ )
						start[i] += ( end[i] - start[i] ) * fraction;
				}
			}
			else
			{
				draw = FALSE;
			}
		}

		if ( draw )
		{
			// Transform point into screen space
			ScreenTransform( start, screen );
			ScreenTransform( end, screenLast );

			// Build world-space normal to screen-space direction vector
			tmp[0] = screenLast[0] - screen[0];
			tmp[1] = screenLast[1] - screen[1];
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );

			VectorScale( vup, tmp[0] * gTracerSize[p->type], normal );
			VectorMA( normal, -tmp[1] * gTracerSize[p->type], vright, normal );

			tri.Begin( TRI_QUADS );
			tri.Color4ub( pColor->r, pColor->g, pColor->b, (byte)p->packedColor );

			tri.Brightness( 0 );
			tri.TexCoord2f( 0, 0 );
			tri.Vertex3f( start[0] + normal[0], start[1] + normal[1], start[2] + normal[2] );

			tri.Brightness( 1 );
			tri.TexCoord2f( 0, 1 );
			tri.Vertex3f( end[0] + normal[0], end[1] + normal[1], end[2] + normal[2] );

			tri.Brightness( 1 );
			tri.TexCoord2f( 1, 1 );
			tri.Vertex3f( end[0] - normal[0], end[1] - normal[1], end[2] - normal[2] );

			tri.Brightness( 0 );
			tri.TexCoord2f( 1, 0 );
			tri.Vertex3f( start[0] - normal[0], start[1] - normal[1], start[2] - normal[2] );

			tri.End();
		}

		VectorMA( p->org, frametime, p->vel, p->org );

		if ( p->type == pt_grav )
		{
			p->vel[0] *= scale;
			p->vel[1] *= scale;
			p->vel[2] -= gravity;

			p->packedColor = (short)(int)( ( p->die - cl.time ) * 2 * 255 );
			if ( p->packedColor > 255 )
				p->packedColor = 255;
		}
		else if ( p->type == pt_slowgrav )
		{
			p->vel[2] = (float)( gravity * 0.05 );
		}
	}

	tri.CullFace( TRI_FRONT );
	tri.RenderMode( kRenderNormal );
}

void R_DrawBeamEntList( float frametime )
{
	int i;
	int beamType;
	BEAM beam;
	cl_entity_t* ent;

	for ( i = 0; i < cl_numbeamentities; i++ )
	{
		ent = cl_beamentities[i];

		// Set up the beam.
		beamType = ent->curstate.rendermode & 0x0F;

		R_BeamSetup( &beam, ent->origin, ent->curstate.angles, ent->curstate.modelindex, 0, ent->curstate.scale,
			(float)( ent->curstate.body * 0.01 ), (float)( CL_FxBlend( ent ) * ( 1.0 / 255.0 ) ),
			ent->curstate.animtime );

		SetBeamAttributes( &beam, (float)( ent->curstate.rendercolor.r * ( 1.0 / 255.0 ) ),
			(float)( ent->curstate.rendercolor.g * ( 1.0 / 255.0 ) ),
			(float)( ent->curstate.rendercolor.b * ( 1.0 / 255.0 ) ), 0, (int)ent->curstate.frame );

		// Handle code from relinking.
		switch ( beamType )
		{
		case BEAM_ENTPOINT:
			beam.type = TE_BEAMPOINTS;
			beam.flags = FBEAM_ENDENTITY;
			beam.startEntity = 0;
			beam.endEntity = ent->curstate.skin;
			break;

		case BEAM_ENTS:
			beam.type = TE_BEAMPOINTS;
			beam.flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;
			beam.startEntity = ent->curstate.sequence;
			beam.endEntity = ent->curstate.skin;
			break;
		}

		beam.flags |= ent->curstate.rendermode & ( FBEAM_SINENOISE | FBEAM_SOLID | FBEAM_SHADEIN | FBEAM_SHADEOUT );
		beam.pFollowModel = NULL;

		// Draw it
		R_BeamDraw( &beam, frametime );
	}
}

void R_BeamDrawList()
{
	BEAM* pbeam;
	BEAM* pkill;
	float frametime;

	if ( !gpActiveBeams && !cl_numbeamentities )
		return;

	frametime = (float)( cl.time - cl.oldtime );

	tri.CullFace( TRI_NONE );

	// kill all the ones hanging direcly off the base pointer
	while ( gpActiveBeams && !( gpActiveBeams->flags & FBEAM_FOREVER ) && cl.time > gpActiveBeams->die )
	{
		pkill = gpActiveBeams;
		gpActiveBeams = pkill->next;
		pkill->next = gpFreeBeams;
		gpFreeBeams = pkill;
	}

	for ( pbeam = gpActiveBeams; pbeam; pbeam = pbeam->next )
	{
		while ( pbeam->next && !( pbeam->next->flags & FBEAM_FOREVER ) && !( cl.time < pbeam->next->die ) )
		{
			pkill = pbeam->next;
			pbeam->next = pkill->next;
			pkill->next = gpFreeBeams;
			gpFreeBeams = pkill;
		}

		R_BeamDraw( pbeam, frametime );
	}

	R_DrawBeamEntList( frametime );

	tri.CullFace( TRI_FRONT );
	tri.RenderMode( kRenderNormal );
}
void SineNoise( float* noise, int divs )
{
	int i;
	float freq;
	float step = (float)( M_PI / divs );

	freq = 0;
	for ( i = 0; i < divs; i++ )
	{
		noise[i] = (float)sin( (double)freq );
		freq += step;
	}
}

void R_DrawDisk( vec_t* source, vec_t* delta, float, float, float freq, float speed, int segments )
{
	int i;
	float div, length, fraction, vLast, vStep;
	vec3_t point;
	float w;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS )
		segments = NOISE_DIVISIONS;

	div = (float)( 1.0 / ( segments - 1 ) );

	// Don't lose all of the noise/texture on short beams
	length = fmaxf( 0.5f, (float)( Length( delta ) * 0.01 ) );

	vStep = length * div; // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = (float)fmod( (double)( freq * speed ), 1.0 );

	w = freq * delta[2];

	for ( i = 0; i < segments; i++ )
	{
		VectorCopy( source, point );

		fraction = i * div;

		tri.Brightness( 1 );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( point );

		point[0] = (float)( sin( fraction * 2 * M_PI ) * w + source[0] );
		point[1] = (float)( cos( fraction * 2 * M_PI ) * w + source[1] );
		point[2] = source[2];

		tri.Brightness( 1 );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( point );

		vLast += vStep; // Advance texture scroll (v axis only)
		vLast = (float)fmod( (double)vLast, 1.0 );
	}
}

void R_DrawCylinder( vec_t* source, vec_t* delta, float width, float, float freq, float speed, int segments )
{
	int i;
	float div, length, fraction, vLast, vStep;
	double s, c;
	vec3_t point;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS )
		segments = NOISE_DIVISIONS;

	div = (float)( 1.0 / ( segments - 1 ) );

	// Don't lose all of the noise/texture on short beams
	length = fmaxf( 0.5f, (float)( Length( delta ) * 0.01 ) );

	vStep = length * div; // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = (float)fmod( (double)( freq * speed ), 1.0 );

	for ( i = 0; i < segments; i++ )
	{
		fraction = i * div;
		s = sin( fraction * 2 * M_PI ) * freq;
		c = cos( fraction * 2 * M_PI ) * freq;

		point[0] = (float)( s * delta[2] + source[0] );
		point[1] = (float)( c * delta[2] + source[1] );
		point[2] = source[2] + width;

		tri.Brightness( 0 );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( point );

		point[0] = (float)( s * ( delta[2] + width ) + source[0] );
		point[1] = (float)( c * ( delta[2] + width ) + source[1] );
		point[2] = source[2] - width;

		tri.Brightness( 1 );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( point );

		vLast += vStep; // Advance texture scroll (v axis only)
		vLast = (float)fmod( (double)vLast, 1.0 );
	}
}

void R_BeamSetup( BEAM* pbeam, vec_t* start, vec_t* end, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed )
{
	model_t* sprite;

	sprite = CL_GetModelByIndex( modelIndex );
	if ( !sprite )
		return;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->modelIndex = modelIndex;
	pbeam->frame = 0;
	pbeam->frameRate = 0;
	pbeam->frameCount = ModelFrameCount( sprite );

	VectorCopy( start, pbeam->source );
	VectorCopy( end, pbeam->target );
	VectorSubtract( end, start, pbeam->delta );

	pbeam->freq = (float)( cl.time * speed );
	pbeam->die = (float)( cl.time + life );
	pbeam->amplitude = amplitude;
	pbeam->speed = speed;
	pbeam->width = width;
	pbeam->brightness = brightness;

	if ( amplitude < 0.5 )
		pbeam->segments = (int)( Length( pbeam->delta ) * 0.075 + 3 ); // one per 16 pixels
	else
		pbeam->segments = (int)( Length( pbeam->delta ) * 0.25 + 3 ); // one per 4 pixels

	pbeam->flags = 0;
	pbeam->pFollowModel = NULL;
}

void R_BeamDraw( BEAM* pbeam, float frametime )
{
	model_t* sprite;
	cl_entity_t* ent;
	float* attachment;
	vec3_t difference;
	vec3_t org, vel;
	float factor, length;
	particle_t* p;
	int frame;

	if ( pbeam->modelIndex < 0 )
	{
		pbeam->die = (float)cl.time;
		return;
	}

	sprite = CL_GetModelByIndex( pbeam->modelIndex );
	if ( !sprite )
		return;

	if ( pbeam->flags & FBEAM_SOLID )
		tri.RenderMode( kRenderNormal );
	else
		tri.RenderMode( kRenderTransAdd );

	pbeam->freq += frametime;

	// OPTIMIZE: Do this every frame?
	// UNDONE: Do this differentially somehow?
	// Generate fractal noise
	gNoise[0] = 0;
	gNoise[NOISE_DIVISIONS] = 0;
	if ( pbeam->amplitude != 0 )
	{
		if ( pbeam->flags & FBEAM_SINENOISE )
			SineNoise( gNoise, NOISE_DIVISIONS );
		else
			Noise( gNoise, NOISE_DIVISIONS );
	}

	// update end points
	if ( pbeam->flags & ( FBEAM_STARTENTITY | FBEAM_ENDENTITY ) )
	{
		if ( pbeam->flags & FBEAM_STARTENTITY )
		{
			ent = R_GetBeamAttachmentEntity( pbeam->startEntity );
			if ( !ent )
				return;

			if ( ent->model && ( !pbeam->pFollowModel || pbeam->pFollowModel == ent->model ) )
			{
				attachment = R_BeamGetAttachmentPoint( ent, pbeam->startEntity );
				VectorCopy( attachment, pbeam->source );
				pbeam->flags |= FBEAM_STARTVISIBLE;

				if ( !pbeam->pFollowModel )
					pbeam->pFollowModel = ent->model;
			}
			else if ( !( pbeam->flags & FBEAM_FOREVER ) )
			{
				pbeam->flags &= ~FBEAM_STARTENTITY;
			}
		}

		if ( pbeam->flags & FBEAM_ENDENTITY )
		{
			ent = R_GetBeamAttachmentEntity( pbeam->endEntity );
			if ( !ent )
				return;

			if ( !ent->model )
			{
				if ( !( pbeam->flags & FBEAM_FOREVER ) )
				{
					pbeam->flags &= ~FBEAM_ENDENTITY;
					pbeam->die = (float)cl.time;
				}
				return;
			}

			attachment = R_BeamGetAttachmentPoint( ent, pbeam->endEntity );
			VectorCopy( attachment, pbeam->target );
			pbeam->flags |= FBEAM_ENDVISIBLE;
		}

		// If we've never seen the start entity, don't display
		if ( ( pbeam->flags & FBEAM_STARTENTITY ) && !( pbeam->flags & FBEAM_STARTVISIBLE ) )
			return;

		// Compute segments from the new endpoints
		VectorSubtract( pbeam->target, pbeam->source, difference );
		if ( Length( difference ) > 0.0000001 )
			VectorCopy( difference, pbeam->delta );

		if ( pbeam->amplitude < 0.5 )
			pbeam->segments = (int)( Length( pbeam->delta ) * 0.075 + 3 ); // one per 16 pixels
		else
			pbeam->segments = (int)( Length( pbeam->delta ) * 0.25 + 3 ); // one per 4 pixels
	}

	if ( pbeam->type == TE_BEAMPOINTS && !R_BeamCull( pbeam->source, pbeam->target, FALSE ) )
		return;

	frame = (int)( pbeam->frameRate * cl.time + pbeam->frame ) % pbeam->frameCount;
	if ( !tri.SpriteTexture( sprite, frame ) )
		return;

	if ( ( pbeam->flags & FBEAM_SINENOISE ) && egon_amplitude.value > 0 )
	{
		factor = pbeam->amplitude * egon_amplitude.value;

		VectorMA( pbeam->target, (float)( sin( (double)( pbeam->freq * 10 ) ) * factor ), vup, org );
		VectorMA( org, (float)( cos( (double)( pbeam->freq * 10 ) ) * factor ), vright, org );

		VectorSubtract( pbeam->source, org, vel );
		length = Length( vel );
		if ( length != 0 )
			VectorScale( vel, (float)( 1000.0 / length ), vel );

		p = R_TracerParticles( org, vel, (float)( length * 0.001 ) );
		if ( p )
			p->color = 7;
	}

	// update life cycle
	ScopedLegacyBeamColors legacyColors( ( pbeam->flags & FBEAM_LEGACY_COLOR255 ) != 0 );
	pbeam->t = (float)( pbeam->freq + ( pbeam->die - cl.time ) );
	if ( pbeam->t != 0 )
		pbeam->t = (float)( 1.0 - pbeam->freq / pbeam->t );

	if ( pbeam->flags & FBEAM_FADEIN )
		tri.Color4f( pbeam->r, pbeam->g, pbeam->b, pbeam->t * pbeam->brightness );
	else if ( pbeam->flags & FBEAM_FADEOUT )
		tri.Color4f( pbeam->r, pbeam->g, pbeam->b, (float)( ( 1.0 - pbeam->t ) * pbeam->brightness ) );
	else
		tri.Color4f( pbeam->r, pbeam->g, pbeam->b, pbeam->brightness );

	switch ( pbeam->type )
	{
	case TE_BEAMPOINTS:
		tri.Begin( TRI_QUADS );
		R_DrawSegs( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments, pbeam->flags );
		break;

	case TE_BEAMTORUS:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawTorus( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments );
		break;

	case TE_BEAMDISK:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawDisk( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments );
		break;

	case TE_BEAMCYLINDER:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawCylinder( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments );
		break;

	case TE_BEAMFOLLOW:
		tri.Begin( TRI_QUADS );
		R_DrawBeamFollow( pbeam );
		break;

	case TE_BEAMRING:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawRing( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments );
		break;

	default:
		return;
	}

	tri.End();
}

void R_DrawRing( vec_t* source, vec_t* delta, float width, float amplitude, float freq, float speed, int segments )
{
	int i, j, noiseIndex, noiseStep;
	float div, length, fraction, factor, vLast, vStep;
	vec3_t last1, last2, point, screen, screenLast, tmp, normal;
	vec3_t center, xaxis, yaxis, mins, maxs;
	float radius, x, y, scale;

	if ( segments < 2 )
		return;

	segments = (int)( segments * M_PI );

	if ( segments > NOISE_DIVISIONS * 8 )
		segments = NOISE_DIVISIONS * 8;

	// Don't lose all of the noise/texture on short beams
	length = fmaxf( 0.5f, (float)( Length( delta ) * 0.01 * M_PI ) );

	div = (float)( 1.0 / ( segments - 1 ) );

	vStep = (float)( length * div / 8.0 ); // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = (float)fmod( (double)( freq * speed ), 1.0 );
	scale = (float)( amplitude * length / 8.0 );

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)( NOISE_DIVISIONS * div * 65536.0 ) * 8;
	noiseIndex = 0;

	VectorScale( delta, 0.5f, delta );
	VectorAdd( source, delta, center );

	VectorCopy( delta, xaxis );
	radius = Length( xaxis );

	mins[0] = center[0] - radius;
	mins[1] = center[1] - radius;
	mins[2] = center[2] - scale;
	maxs[0] = center[0] + radius;
	maxs[1] = center[1] + radius;
	maxs[2] = center[2] + scale;

	if ( !cl.worldmodel )
		return;

	// Is that box in PVS && frustum?
	if ( !PVSNode( cl.worldmodel->nodes, mins, maxs ) || R_CullBox( mins, maxs ) )
		return;

	yaxis[0] = xaxis[1];
	yaxis[1] = -xaxis[0];
	yaxis[2] = 0;
	VectorNormalize( yaxis );
	VectorScale( yaxis, radius, yaxis );

	j = segments / 8;

	for ( i = 0; i < segments + 1; i++ )
	{
		fraction = i * div;
		x = (float)sin( fraction * 2 * M_PI );
		y = (float)cos( fraction * 2 * M_PI );

		point[0] = yaxis[0] * y + xaxis[0] * x + center[0];
		point[1] = yaxis[1] * y + xaxis[1] * x + center[1];
		point[2] = yaxis[2] * y + xaxis[2] * x + center[2];

		// Distort using noise
		factor = gNoise[( noiseIndex >> 16 ) & ( NOISE_DIVISIONS - 1 )] * scale;
		VectorMA( point, factor, vup, point );

		// Rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
		factor = (float)( factor * cos( fraction * M_PI * 3 * 8 + freq ) );
		VectorMA( point, factor, vright, point );

		// Transform source into screen space
		ScreenTransform( point, screen );

		if ( i != 0 )
		{
			// Build world-space normal to screen-space direction vector
			tmp[0] = screen[0] - screenLast[0];
			tmp[1] = screen[1] - screenLast[1];
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );
			VectorScale( vup, tmp[0], normal ); // Build point along noraml line (normal is -y, x)
			VectorMA( normal, -tmp[1], vright, normal );

			// Make a wide line
			VectorMA( point, width, normal, last1 );
			VectorMA( point, -width, normal, last2 );

			vLast += vStep; // Advance texture scroll (v axis only)
			tri.TexCoord2f( 1, vLast );
			tri.Vertex3fv( last2 );
			tri.TexCoord2f( 0, vLast );
			tri.Vertex3fv( last1 );
		}

		screenLast[0] = screen[0];
		screenLast[1] = screen[1];

		noiseIndex += noiseStep;
		vLast = (float)fmod( (double)vLast, 1.0 );

		j--;
		if ( j == 0 && amplitude != 0 )
		{
			j = segments / 8;
			Noise( gNoise, NOISE_DIVISIONS );
		}
	}
}

void R_DrawBeamFollow( BEAM* pbeam )
{
	particle_t* pnew;
	particle_t* p;
	vec3_t delta, start;
	float fraction;
	float div;
	float frametime;
	vec3_t last1, last2, screen, screenLast, tmp, normal;

	R_FreeDeadParticles( &pbeam->particles );

	p = pbeam->particles;
	pnew = NULL;

	div = 0;
	if ( pbeam->flags & FBEAM_STARTENTITY )
	{
		if ( p )
		{
			VectorSubtract( p->org, pbeam->source, delta );
			div = Length( delta );
			if ( div >= 32 && free_particles )
			{
				pnew = free_particles;
				free_particles = pnew->next;
			}
		}
		else if ( free_particles )
		{
			pnew = free_particles;
			free_particles = pnew->next;
		}
	}

	if ( pnew )
	{
		VectorCopy( pbeam->source, pnew->org );
		VectorClear( pnew->vel );
		pnew->die = (float)( cl.time + pbeam->amplitude );
		pbeam->die = (float)( cl.time + pbeam->amplitude );

		pnew->next = p;
		pbeam->particles = pnew;
		p = pnew;
	}

	if ( !p )
		return;

	if ( !pnew && div != 0 )
	{
		VectorCopy( pbeam->source, start );
		ScreenTransform( pbeam->source, screenLast );
	}
	else if ( p->next )
	{
		VectorCopy( p->org, start );
		ScreenTransform( p->org, screenLast );
		p = p->next;
	}
	else
	{
		return;
	}

	ScreenTransform( p->org, screen );

	// Build world-space normal to screen-space direction vector
	tmp[0] = screen[0] - screenLast[0];
	tmp[1] = screen[1] - screenLast[1];
	// We don't need Z, we're in screen space
	tmp[2] = 0;
	VectorNormalize( tmp );
	VectorScale( vup, tmp[0], normal );
	VectorMA( normal, -tmp[1], vright, normal );

	// Make a wide line
	VectorMA( start, pbeam->width, normal, last1 );
	VectorMA( start, -pbeam->width, normal, last2 );

	div = (float)( 1.0 / pbeam->amplitude );
	fraction = (float)( ( pbeam->die - cl.time ) * div );

	for ( ; p; p = p->next )
	{
		tri.Brightness( fraction );
		tri.TexCoord2f( 0, 0 );
		tri.Vertex3fv( last1 );

		tri.Brightness( fraction );
		tri.TexCoord2f( 1, 0 );
		tri.Vertex3fv( last2 );

		// Transform source into screen space
		ScreenTransform( p->org, screen );

		// Build world-space normal to screen-space direction vector
		tmp[0] = screen[0] - screenLast[0];
		tmp[1] = screen[1] - screenLast[1];
		// We don't need Z, we're in screen space
		tmp[2] = 0;
		VectorNormalize( tmp );
		VectorScale( vup, tmp[0], normal );
		VectorMA( normal, -tmp[1], vright, normal );

		// Make a wide line
		VectorMA( p->org, pbeam->width, normal, last1 );
		VectorMA( p->org, -pbeam->width, normal, last2 );

		if ( p->next )
			fraction = (float)( ( p->die - cl.time ) * div );
		else
			fraction = 0;

		tri.Brightness( fraction );
		tri.TexCoord2f( 1, 1 );
		tri.Vertex3fv( last2 );

		tri.Brightness( fraction );
		tri.TexCoord2f( 0, 1 );
		tri.Vertex3fv( last1 );

		screenLast[0] = screen[0];
		screenLast[1] = screen[1];
	}

	// Drift popcorn trail if there is a velocity
	frametime = (float)( cl.time - cl.oldtime );
	for ( p = pbeam->particles; p; p = p->next )
		VectorMA( p->org, frametime, p->vel, p->org );
}

void R_DrawSegs(
	vec_t* source, vec_t* delta, float width, float scale, float freq, float speed, int segments, int flags )
{
	int i, noiseIndex, noiseStep;
	float div, length, fraction, factor, vLast, vStep, brightness;
	vec3_t last1, last2, point, screen, screenLast, tmp, normal;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS )
		segments = NOISE_DIVISIONS;

	div = (float)( 1.0 / ( segments - 1 ) );

	// Don't lose all of the noise/texture on short beams
	length = fmaxf( 0.5f, (float)( Length( delta ) * 0.01 ) );

	vStep = length * div; // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = (float)fmod( (double)( freq * speed ), 1.0 );

	if ( flags & FBEAM_SINENOISE )
	{
		if ( segments < 16 )
			segments = 16;

		scale *= 100;
		length = (float)( segments * 0.1 );
	}
	else
	{
		scale *= length;
	}

	// Transform source into screen space
	ScreenTransform( source, screenLast );
	VectorMA( source, div, delta, point );
	ScreenTransform( point, screen );

	// Build world-space normal to screen-space direction vector
	tmp[0] = screen[0] - screenLast[0];
	tmp[1] = screen[1] - screenLast[1];
	// We don't need Z, we're in screen space
	tmp[2] = 0;
	VectorNormalize( tmp );
	VectorScale( vup, tmp[0], normal ); // Build point along noraml line (normal is -y, x)
	VectorMA( normal, -tmp[1], vright, normal );

	// Make a wide line
	VectorMA( source, width, normal, last1 );
	VectorMA( source, -width, normal, last2 );

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)( NOISE_DIVISIONS * div * 65536.0 );
	noiseIndex = noiseStep;

	// Sine noise beams have different length calculations
	if ( flags & FBEAM_SINENOISE )
		noiseIndex = 0;

	brightness = 1.0f;
	if ( flags & FBEAM_SHADEIN )
		brightness = 0;

	for ( i = 1; i < segments; i++ )
	{
		fraction = i * div;

		tri.Brightness( brightness );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( last1 );
		tri.Brightness( brightness );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( last2 );

		if ( flags & FBEAM_SHADEIN )
			brightness = fraction;
		else if ( flags & FBEAM_SHADEOUT )
			brightness = (float)( 1.0 - fraction );

		VectorMA( source, fraction, delta, point );

		// Distort using noise
		if ( scale != 0 )
		{
			factor = gNoise[( noiseIndex >> 16 ) & ( NOISE_DIVISIONS - 1 )] * scale;

			if ( flags & FBEAM_SINENOISE )
			{
				VectorMA( point, (float)( factor * sin( fraction * M_PI * length + freq ) ), vup, point );
				VectorMA( point, (float)( factor * cos( fraction * M_PI * length + freq ) ), vright, point );
			}
			else
			{
				VectorMA( point, factor, vup, point );
				VectorMA( point, (float)( factor * cos( fraction * M_PI * 3 + freq ) ), vright, point );
			}
		}

		// Transform point into screen space
		ScreenTransform( point, screen );

		// Build world-space normal to screen-space direction vector
		tmp[0] = screen[0] - screenLast[0];
		tmp[1] = screen[1] - screenLast[1];
		// We don't need Z, we're in screen space
		tmp[2] = 0;
		VectorNormalize( tmp );
		VectorScale( vup, tmp[0], normal ); // Build point along noraml line (normal is -y, x)
		VectorMA( normal, -tmp[1], vright, normal );

		// Make a wide line
		VectorMA( point, width, normal, last1 );
		VectorMA( point, -width, normal, last2 );

		vLast += vStep; // Advance texture scroll (v axis only)
		tri.Brightness( brightness );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( last2 );
		tri.Brightness( brightness );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( last1 );

		screenLast[0] = screen[0];
		screenLast[1] = screen[1];

		noiseIndex += noiseStep;
		vLast = (float)fmod( (double)vLast, 1.0 );
	}
}

void R_DrawTorus( vec_t* source, vec_t* delta, float width, float scale, float freq, float speed, int segments )
{
	int i, noiseIndex, noiseStep;
	float div, length, fraction, factor, vLast, vStep;
	vec3_t last1, last2, point, screen, screenLast, tmp, normal;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS )
		segments = NOISE_DIVISIONS;

	div = (float)( 1.0 / ( segments - 1 ) );

	// Don't lose all of the noise/texture on short beams
	length = fmaxf( 0.5f, (float)( Length( delta ) * 0.01 ) );

	vStep = length * div; // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = (float)fmod( (double)( freq * speed ), 1.0 );
	scale *= length;

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)( NOISE_DIVISIONS * div * 65536.0 );
	noiseIndex = 0;

	for ( i = 0; i <= segments; i++ )
	{
		fraction = i * div;

		point[0] = (float)( sin( fraction * 2 * M_PI ) * freq * delta[2] + source[0] );
		point[1] = (float)( cos( fraction * 2 * M_PI ) * freq * delta[2] + source[1] );
		point[2] = source[2];

		// Distort using noise
		factor = gNoise[( noiseIndex >> 16 ) & ( NOISE_DIVISIONS - 1 )] * scale;
		VectorMA( point, factor, vup, point );

		factor = (float)( factor * cos( fraction * M_PI * 3 + freq ) );
		VectorMA( point, factor, vright, point );

		// Transform source into screen space
		ScreenTransform( point, screen );

		if ( i != 0 )
		{
			// Build world-space normal to screen-space direction vector
			tmp[0] = screen[0] - screenLast[0];
			tmp[1] = screen[1] - screenLast[1];
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );
			VectorScale( vup, tmp[0], normal ); // Build point along noraml line (normal is -y, x)
			VectorMA( normal, -tmp[1], vright, normal );

			// Make a wide line
			VectorMA( point, width, normal, last1 );
			VectorMA( point, -width, normal, last2 );

			vLast += vStep; // Advance texture scroll (v axis only)
			tri.TexCoord2f( 1, vLast );
			tri.Vertex3fv( last2 );
			tri.TexCoord2f( 0, vLast );
			tri.Vertex3fv( last1 );
		}

		screenLast[0] = screen[0];
		screenLast[1] = screen[1];

		noiseIndex += noiseStep;
		vLast = (float)fmod( (double)vLast, 1.0 );
	}
}

// Fractal noise generator, power of 2 wavelength
void Noise( float* noise, int divs )
{
	int div2;

	if ( divs < 2 )
		return;

	div2 = divs / 2;

	// Noise is normalized to +/- scale
	noise[div2] = (float)( ( noise[0] + noise[divs] ) * 0.5 + (double)RandomFloat( -0.125f, 0.125f ) * (float)divs );
	if ( div2 > 1 )
	{
		Noise( &noise[div2], div2 );
		Noise( noise, div2 );
	}
}

int R_BeamCull( vec_t* start, vec_t* end, int pvsOnly )
{
	vec3_t mins, maxs;
	int i;

	if ( !cl.worldmodel )
		return 0;

	for ( i = 0; i < 3; i++ )
	{
		if ( start[i] < end[i] )
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}

		// Don't let it be zero sized
		if ( mins[i] == maxs[i] )
			maxs[i] += 1;
	}

	// Check bbox
	if ( PVSNode( cl.worldmodel->nodes, mins, maxs ) )
	{
		if ( pvsOnly || !R_CullBox( mins, maxs ) )
			return 1; // Beam is visible
	}

	// Beam is not visible
	return 0;
}

cl_entity_t* R_GetBeamAttachmentEntity( int index )
{
	if ( index < 0 )
		return ClientDLL_GetUserEntity( BEAMENT_ENTITY( -index ) );

	return &cl_entities[BEAMENT_ENTITY( index )];
}

float* R_BeamGetAttachmentPoint( cl_entity_t* ent, int attachment )
{
	int point;

	point = BEAMENT_ATTACHMENT( abs( attachment ) );
	if ( point )
		return ent->attachment[point - 1];

	if ( ent->index == cl.playernum + 1 )
		return cl.simorg;

	return ent->origin;
}

BEAM* R_BeamAlloc()
{
	BEAM* pbeam;

	if ( !gpFreeBeams )
		return NULL;

	pbeam = gpFreeBeams;
	gpFreeBeams = pbeam->next;
	pbeam->next = gpActiveBeams;
	gpActiveBeams = pbeam;

	return pbeam;
}

BEAM* R_BeamLightning(
	vec_t* start, vec_t* end, int modelIndex, float life, float width, float amplitude, float brightness, float speed )
{
	BEAM* pbeam;

	pbeam = R_BeamAlloc();
	if ( !pbeam )
		return NULL;

	pbeam->die = (float)cl.time;

	if ( modelIndex < 0 )
		return NULL;

	R_BeamSetup( pbeam, start, end, modelIndex, life, width, amplitude, brightness, speed );

	return pbeam;
}

void SetBeamAttributes( BEAM* pbeam, float r, float g, float b, float framerate, int startFrame )
{
	pbeam->frameRate = framerate;
	pbeam->frame = (float)startFrame;

	pbeam->r = r;
	pbeam->g = g;
	pbeam->b = b;
}

BEAM* R_BeamEnts( int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM* pbeam;
	cl_entity_t* start;
	cl_entity_t* end;

	start = R_GetBeamAttachmentEntity( startEnt );
	end = R_GetBeamAttachmentEntity( endEnt );
	if ( !start || !end )
		return NULL;

	if ( life != 0 && ( !start->model || !end->model ) )
		return NULL;

	pbeam = R_BeamLightning( vec3_origin, vec3_origin, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = endEnt;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

BEAM* R_BeamEntPoint( int startEnt, vec_t* end, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM* pbeam;
	cl_entity_t* start;

	start = R_GetBeamAttachmentEntity( startEnt );
	if ( !start )
		return NULL;

	if ( life != 0 && !start->model )
		return NULL;

	pbeam = R_BeamLightning( vec3_origin, end, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->flags = FBEAM_STARTENTITY;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = 0;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

BEAM* R_BeamPoints( vec_t* start, vec_t* end, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM* pbeam;

	// Don't start temporary beams out of the PVS
	if ( life != 0 && !R_BeamCull( start, end, TRUE ) )
		return NULL;

	pbeam = R_BeamLightning( start, end, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

BEAM* R_BeamCirclePoints( int type, vec_t* start, vec_t* end, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM* pbeam;

	pbeam = R_BeamLightning( start, end, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = type;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

BEAM* R_BeamFollow( int startEnt, int modelIndex, float life, float width, float r, float g, float b, float brightness )
{
	BEAM* pbeam;

	pbeam = R_BeamLightning( vec3_origin, vec3_origin, modelIndex, life, width, life, brightness, 1 );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMFOLLOW;
	pbeam->flags = FBEAM_STARTENTITY;

	pbeam->startEntity = startEnt;

	SetBeamAttributes( pbeam, r, g, b, 1, 0 );

	return pbeam;
}

BEAM* R_BeamRing( int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM* pbeam;
	cl_entity_t* start;
	cl_entity_t* end;

	start = R_GetBeamAttachmentEntity( startEnt );
	end = R_GetBeamAttachmentEntity( endEnt );
	if ( !start || !end )
		return NULL;

	if ( life != 0 && ( !start->model || !end->model ) )
		return NULL;

	pbeam = R_BeamLightning( vec3_origin, vec3_origin, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMRING;
	pbeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = endEnt;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}
/*
===============
R_ParticleExplosion

===============
*/
void R_ParticleExplosion( vec_t* org )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < 1024; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + 5.0 );
		p->color = (short)ramp1[0];
		p->packedColor = hlRGB( host_basepal, p->color );
		p->ramp = (float)RandomLong( 0, 3 );
		p->type = ( i & 1 ) ? pt_explode : pt_explode2;

		do
		{
			for ( j = 0; j < 3; j++ )
				p->vel[j] = RandomFloat( -512, 512 );
		} while ( DotProduct( p->vel, p->vel ) > 512 * 512 );

		for ( j = 0; j < 3; j++ )
			p->org[j] = org[j] + p->vel[j] * 0.25f;
	}
}

/*
===============
R_ParticleExplosion2

===============
*/
void R_ParticleExplosion2( vec_t* org, int colorStart, int colorLength )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < 512; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + 0.3 );
		p->color = (short)( colorStart + i % colorLength );
		p->packedColor = hlRGB( host_basepal, p->color );
		p->type = pt_blob;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + RandomFloat( -16, 16 );
			p->vel[j] = RandomFloat( -256, 256 );
		}
	}
}

/*
===============
R_BlobExplosion

===============
*/
void R_BlobExplosion( vec_t* org )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < 1024; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + RandomFloat( 1, 1.4f ) );

		if ( i & 1 )
		{
			p->type = pt_blob;
			p->color = (short)RandomLong( 66, 71 );
		}
		else
		{
			p->type = pt_blob2;
			p->color = (short)RandomLong( 150, 155 );
		}
		p->packedColor = hlRGB( host_basepal, p->color );

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + RandomFloat( -16, 16 );
			p->vel[j] = RandomFloat( -256, 256 );
		}
	}
}

/*
===============
R_LavaSplash

===============
*/
void R_LavaSplash( vec_t* org )
{
	int i, j;
	particle_t* p;
	float vel;
	vec3_t dir;

	for ( i = -16; i < 16; i++ )
	{
		for ( j = -16; j < 16; j++ )
		{
			if ( !free_particles )
				return;

			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)( cl.time + RandomFloat( 2, 2.62f ) );
			p->color = (short)RandomLong( 224, 231 );
			p->type = pt_slowgrav;
			p->packedColor = hlRGB( host_basepal, p->color );

			dir[0] = j * 8 + RandomFloat( 0, 7 );
			dir[1] = i * 8 + RandomFloat( 0, 7 );
			dir[2] = 256;

			p->org[0] = org[0] + dir[0];
			p->org[1] = org[1] + dir[1];
			p->org[2] = org[2] + RandomFloat( 0, 63 );

			VectorNormalize( dir );
			vel = RandomFloat( 50, 113 );
			VectorScale( dir, vel, p->vel );
		}
	}
}

void R_TeleportSplash( vec_t* org )
{
	int i, j, k;
	particle_t* p;
	float vel;
	vec3_t dir;

	for ( i = -16; i < 16; i += 4 )
	{
		for ( j = -16; j < 16; j += 4 )
		{
			for ( k = -24; k < 32; k += 4 )
			{
				if ( !free_particles )
					return;

				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = (float)( cl.time + RandomFloat( 0.2f, 0.34f ) );
				p->color = (short)RandomLong( 7, 14 );
				p->packedColor = hlRGB( host_basepal, p->color );
				p->type = pt_slowgrav;

				dir[0] = (float)( j * 8 );
				dir[1] = (float)( i * 8 );
				dir[2] = (float)( k * 8 );

				p->org[0] = org[0] + i + RandomFloat( 0, 3 );
				p->org[1] = org[1] + j + RandomFloat( 0, 3 );
				p->org[2] = org[2] + k + RandomFloat( 0, 3 );

				VectorNormalize( dir );
				vel = RandomFloat( 50, 113 );
				VectorScale( dir, vel, p->vel );
			}
		}
	}
}

particle_t* R_TracerParticles( vec_t* org, vec_t* vel, float life )
{
	particle_t* p;

	if ( !free_particles )
		return NULL;

	p = free_particles;
	free_particles = p->next;
	p->next = gpActiveTracers;
	gpActiveTracers = p;

	p->die = (float)( cl.time + life );
	p->color = 4;
	p->packedColor = 255;
	p->type = pt_static;
	p->ramp = tracerLength.value;

	VectorCopy( org, p->org );
	VectorCopy( vel, p->vel );

	return p;
}

void R_Implosion( vec_t* end, float radius, int count, float life )
{
	int i;
	float dist;
	vec3_t start, temp, vel;

	dist = (float)( radius / 100.0 );

	for ( i = 0; i < count; i++ )
	{
		temp[0] = RandomFloat( -100, 100 ) * dist;
		temp[1] = RandomFloat( -100, 100 ) * dist;
		temp[2] = RandomFloat( 0, 100 ) * dist;

		VectorAdd( temp, end, start );
		VectorScale( temp, (float)( -1.0 / life ), vel );

		R_TracerParticles( start, vel, life );
	}
}

void R_RocketTrail( vec_t* start, vec_t* end, int type )
{
	vec3_t vec, right, up;
	float len;
	int j;
	particle_t* p;
	float dec;
	double angle, radius;
	float s, c;

	static int tracercount;

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	if ( type == 7 )
	{
		dec = 1;
		VectorMatrix( vec, right, up );
	}
	else if ( type < 128 )
	{
		dec = 3;
	}
	else
	{
		dec = 1;
		type -= 128;
	}

	VectorScale( vec, dec, vec );

	while ( len > 0 )
	{
		len -= dec;

		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( vec3_origin, p->vel );

		p->die = (float)( cl.time + 2.0 );

		switch ( type )
		{
		case 0: // rocket trail
			p->ramp = (float)RandomLong( 0, 3 );
			p->color = (short)ramp3[(int)p->ramp];
			p->type = pt_fire;
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			break;

		case 1: // smoke smoke
			p->ramp = (float)RandomLong( 2, 5 );
			p->color = (short)ramp3[(int)p->ramp];
			p->type = pt_fire;
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			break;

		case 2: // blood
			p->type = pt_grav;
			p->color = (short)RandomLong( 67, 74 );
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			break;

		case 3:
		case 5: // tracer
			p->die = (float)( cl.time + 0.5 );
			p->type = pt_static;

			if ( type == 3 )
				p->color = (short)( 52 + ( tracercount & 4 ) * 2 );
			else
				p->color = (short)( 230 + ( tracercount & 4 ) * 2 );

			VectorCopy( start, p->org );
			tracercount++;

			if ( tracercount & 1 )
			{
				p->vel[0] = vec[1] * 30;
				p->vel[1] = -vec[0] * 30;
			}
			else
			{
				p->vel[0] = -vec[1] * 30;
				p->vel[1] = vec[0] * 30;
			}
			break;

		case 4: // slight blood
			p->type = pt_grav;
			p->color = (short)RandomLong( 67, 70 );
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			len -= 3;
			break;

		case 6: // voor trail
			p->ramp = (float)RandomLong( 0, 3 );
			p->color = (short)ramp3[(int)p->ramp];
			p->type = pt_fire;
			VectorCopy( start, p->org );
			break;

		case 7:
			angle = RandomLong( 0, 0xFFFF );
			radius = RandomLong( 8, 16 );
			s = (float)( sin( angle ) * radius );
			c = (float)( cos( angle ) * radius );

			for ( j = 0; j < 3; j++ )
#ifdef _MSC_VER
#pragma warning( suppress: 6001 )
#endif
				p->org[j] = start[j] + right[j] * s + up[j] * c;

			VectorSubtract( start, p->org, p->vel );
			VectorScale( p->vel, 2, p->vel );
			VectorMA( p->vel, RandomFloat( 96, 111 ), vec, p->vel );

			p->die = (float)( cl.time + 2.0 );
			p->ramp = (float)RandomLong( 0, 3 );
			p->color = (short)ramp3[(int)p->ramp];
			p->type = pt_explode2;
			break;

		default:
			break;
		}

		p->packedColor = hlRGB( host_basepal, p->color );

		VectorAdd( start, vec, start );
	}
}

void R_FlickerParticles( vec_t* org )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < 15; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j];
			p->vel[j] = RandomFloat( -32, 32 );
		}

		p->vel[2] = RandomFloat( 80, 143 );

		p->ramp = 0;
		p->color = 254;
		p->packedColor = hlRGB( host_basepal, p->color );
		p->type = pt_blob2;
		p->die = (float)( cl.time + 2.0 );
	}
}

void R_SparkStreaks( vec_t* pos, int count, int velocityMin, int velocityMax )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = gpActiveTracers;
		gpActiveTracers = p;

		p->die = (float)( cl.time + RandomFloat( 0.1f, 0.5f ) );
		p->color = 5;
		p->packedColor = 255;
		p->type = pt_grav;
		p->ramp = 0.5f;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = pos[j];
			p->vel[j] = RandomFloat( (float)velocityMin, (float)velocityMax );
		}
	}
}

void R_StreakSplash( vec_t* pos, vec_t* dir, int color, int count, float speed, int velocityMin, int velocityMax )
{
	int i, j;
	particle_t* p;
	vec3_t initialVelocity;

	VectorScale( dir, speed, initialVelocity );

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = gpActiveTracers;
		gpActiveTracers = p;

		p->die = (float)( cl.time + RandomFloat( 0.1f, 0.5f ) );
		p->color = (short)color;
		p->packedColor = 255;
		p->type = pt_grav;
		p->ramp = 1;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = pos[j];
			p->vel[j] = initialVelocity[j] + RandomFloat( (float)velocityMin, (float)velocityMax );
		}
	}
}

void R_UserTracerParticle( vec_t* org, vec_t* vel, float life, int colorIndex, float length, unsigned char context,
	void ( *deathfunc )( particle_t* particle ) )
{
	particle_t* p;

	if ( colorIndex < 0 )
	{
		Con_Printf( "UserTracer with color < 0\n" );
		return;
	}

	if ( colorIndex > 12 )
	{
		Con_Printf( "UserTracer with color > %d\n", 12 );
		return;
	}

	if ( !free_particles )
		return;

	p = free_particles;
	free_particles = p->next;
	p->next = gpActiveTracers;
	gpActiveTracers = p;

	p->die = (float)( cl.time + life );
	p->color = (short)colorIndex;
	p->packedColor = 255;
	p->type = pt_static;
	p->ramp = length;

	VectorCopy( org, p->org );
	VectorCopy( vel, p->vel );

	p->deathfunc = deathfunc;
	p->context = context;
}

void R_LargeFunnel( vec_t* org, int reverse )
{
	int i, j;
	particle_t* p;
	float vel;
	vec3_t dir, dest;
	float flDist;

	for ( i = -256; i <= 256; i += 32 )
	{
		for ( j = -256; j <= 256; j += 32 )
		{
			if ( !free_particles )
				return;

			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			if ( reverse )
			{
				VectorCopy( org, p->org );

				dest[0] = org[0] + i;
				dest[1] = org[1] + j;
				dest[2] = org[2] + RandomFloat( 100, 800 );

				// send particle heading to dest at a random speed
				VectorSubtract( dest, p->org, dir );

				vel = dest[2] * 0.125f; // velocity based on how far particle starts from org
			}
			else
			{
				p->org[0] = org[0] + i;
				p->org[1] = org[1] + j;
				p->org[2] = org[2] + RandomFloat( 100, 800 );

				// send particle heading to org at a random speed
				VectorSubtract( org, p->org, dir );

				vel = p->org[2] * 0.125f; // velocity based on how far particle starts from org
			}

			p->color = 244;
			p->packedColor = hlRGB( host_basepal, p->color );
			p->type = pt_static;

			flDist = VectorNormalize( dir ); // save the distance

			if ( vel < 64 )
				vel = 64;

			vel += RandomFloat( 64, 128 );
			VectorScale( dir, vel, p->vel );

			// die right when you get there
			p->die = (float)( cl.time + flDist / vel );
		}
	}
}

void R_ParticleBurst( vec_t* pos, int size, int color, float life )
{
	int i, j;
	particle_t* p;
	float vel;
	vec3_t dir, dest;

	for ( i = 0; i < 1024; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->color = (short)( color + RandomLong( 0, 10 ) );
		p->packedColor = hlRGB( host_basepal, p->color );
		p->type = pt_static;

		VectorCopy( pos, p->org );

		for ( j = 0; j < 3; j++ )
			dest[j] = pos[j] + RandomFloat( (float)-size, (float)size );

		VectorSubtract( dest, p->org, dir );
		vel = VectorNormalize( dir ) / life;

		p->die = (float)( cl.time + life + RandomFloat( -0.5f, 0.5f ) );
		VectorScale( dir, vel, p->vel );
	}
}

void R_Blood( vec_t* org, vec_t* dir, int pcolor, int speed )
{
	vec3_t dirCopy;
	vec3_t orgCopy;
	float arc;
	int count;
	int count2;
	int j;
	particle_t* p;
	int pspeed;

	VectorNormalize( dir );

	pspeed = speed * 3;

	arc = 0.06f;
	for ( count = 0; count < speed / 2; count++ )
	{
		for ( j = 0; j < 3; j++ )
			orgCopy[j] = org[j] + RandomFloat( -3, 3 );

		for ( j = 0; j < 3; j++ )
			dirCopy[j] = dir[j] + RandomFloat( -arc, arc );

		for ( count2 = 0; count2 < 8; count2++ )
		{
			if ( !free_particles )
				return;

			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)( cl.time + 1.5 );
			p->color = (short)( pcolor + RandomLong( 0, 9 ) );
			p->packedColor = hlRGB( host_basepal, p->color );
			p->type = pt_vox_grav;

			for ( j = 0; j < 3; j++ )
				p->org[j] = orgCopy[j] + RandomFloat( -1, 1 );

			VectorScale( dirCopy, (float)pspeed, p->vel );
		}

		pspeed -= speed;
	}
}

void R_BloodStream( vec_t* org, vec_t* dir, int pcolor, int speed )
{
	vec3_t dirCopy;
	float arc;
	int count;
	int count2;
	int j;
	particle_t* p;
	float num;
	int speedCopy = speed;

	VectorNormalize( dir );

	arc = 0.05f;
	for ( count = 0; count < 100; count++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + 2.0 );
		p->color = (short)( pcolor + RandomLong( 0, 9 ) );
		p->packedColor = hlRGB( host_basepal, p->color );
		p->type = pt_vox_grav;

		VectorCopy( org, p->org );
		VectorCopy( dir, dirCopy );
		dirCopy[2] -= arc;

		arc = (float)( arc - 0.005 );

		VectorScale( dirCopy, (float)speedCopy, p->vel );
		speedCopy = (int)( speedCopy - 0.00001 );
	}

	arc = 0.075f;
	for ( count = 0; count < speed / 5; count++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + 3.0 );
		p->color = (short)( pcolor + RandomLong( 0, 9 ) );
		p->packedColor = hlRGB( host_basepal, p->color );
		p->type = pt_vox_slowgrav;

		VectorCopy( org, p->org );
		VectorCopy( dir, dirCopy );
		dirCopy[2] -= arc;

		arc = (float)( arc - 0.005 );

		num = RandomFloat( 0, 1 );
		speedCopy = (int)( speed * num );
		num = (float)( num * 1.7 );

		VectorScale( dirCopy, num, dirCopy ); // randomize a bit
		VectorScale( dirCopy, (float)speedCopy, p->vel );

		for ( count2 = 0; count2 < 2; count2++ )
		{
			if ( !free_particles )
				return;

			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)( cl.time + 3.0 );
			p->color = (short)( pcolor + RandomLong( 0, 9 ) );
			p->packedColor = hlRGB( host_basepal, p->color );
			p->type = pt_vox_slowgrav;

			for ( j = 0; j < 3; j++ )
				p->org[j] = org[j] + RandomFloat( -1, 1 );

			VectorCopy( dir, dirCopy );
			dirCopy[2] -= arc;

			VectorScale( dirCopy, num, dirCopy ); // randomize a bit
			VectorScale( dirCopy, (float)speedCopy, p->vel );
		}
	}
}

void R_BulletImpactParticles( vec_t* pos )
{
	int i, j;
	particle_t* p;
	vec3_t dir;
	float dist, vel;
	int steps, count, color;

	VectorSubtract( pos, r_origin, dir );
	dist = Length( dir );
	if ( dist > 1000 )
		dist = 1000;

	steps = (int)( ( 1000 - dist ) / 100 );
	if ( steps == 0 )
		steps = 1;

	count = steps * 4;
	color = steps * 30 / 100;

	R_SparkStreaks( pos, 2, -200, 200 );

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( pos, p->org );

		for ( j = 0; j < 3; j++ )
			p->vel[j] = RandomFloat( -1, 1 );

		vel = RandomFloat( 50, 100 );
		VectorScale( p->vel, vel, p->vel );

		p->type = pt_grav;
		p->color = (short)color;
		p->packedColor = hlRGB( host_basepal, color + RandomLong( 0, 5 ) );
		p->die = (float)( cl.time + 0.5 );
	}
}

void R_ShowLine( vec_t* start, vec_t* end )
{
	vec3_t vec;
	float len;
	particle_t* p;

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	VectorScale( vec, 5, vec );

	while ( len > 0 )
	{
		len -= 5;

		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( vec3_origin, p->vel );

		p->color = 75;
		p->packedColor = hlRGB( host_basepal, p->color );
		p->type = pt_static;
		p->die = (float)( cl.time + 30 );

		VectorCopy( start, p->org );
		VectorAdd( start, vec, start );
	}
}

void ParticleLine( float x1, float y1, float z1, float x2, float y2, float z2 )
{
	vec3_t start, vec;
	float len;
	particle_t* p;

	vec[0] = x2 - x1;
	vec[1] = y2 - y1;
	vec[2] = z2 - z1;
	len = VectorNormalize( vec );
	VectorScale( vec, 5, vec );

	if ( len <= 0 )
		return;

	len -= 5;

	start[0] = x1;
	start[1] = y1;
	start[2] = z1;

	for ( ;; )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( vec3_origin, p->vel );
		p->die = (float)( cl.time + 30 );
		VectorCopy( start, p->org );
		p->color = 75;
		p->packedColor = hlRGB( host_basepal, p->color );
		p->type = pt_static;

		if ( len <= 0 )
			break;

		VectorAdd( start, vec, start );
		len -= 5;
	}
}

void ParticleBox( vec_t* mins, vec_t* maxs )
{
	vec3_t start, end;

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];
	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = mins[2];
	R_ShowLine( start, end );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = mins[2];
	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = mins[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];
	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = mins[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = mins[2];
	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = mins[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = maxs[2];
	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = maxs[2];
	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = maxs[2];
	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = maxs[2];
	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];
	end[0] = mins[0];
	end[1] = mins[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = mins[2];
	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = maxs[0];
	start[1] = maxs[1];
	start[2] = mins[2];
	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = mins[2];
	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );
}

void R_GetPackedColor( short* packed, short color )
{
	if ( !packed )
	{
		Con_Printf( "R_GetPackedColor called without packed!\n" );
		return;
	}

	*packed = hlRGB( host_basepal, color );
}

short R_LookupColor( unsigned char r, unsigned char g, unsigned char b )
{
	int i;
	short best;
	float diff, bestdiff;
	float dr, dg, db;
	unsigned short* pb;

	bestdiff = 999999;
	best = -1;

	for ( i = 0; i < 256; i++ )
	{
		pb = &host_basepal[i * 4];

		dr = (float)( r - pb[2] );
		dg = (float)( g - pb[1] );
		db = (float)( b - pb[0] );

		diff = dg * 0.5f * dg + dr * 0.3f * dr + db * 0.2f * db;
		if ( bestdiff > diff )
		{
			bestdiff = diff;
			best = (short)i;
		}
	}

	return best;
}

/*
===============
R_EntityParticles
===============
*/
void R_EntityParticles( cl_entity_t* ent )
{
	int i, j;
	particle_t* p;
	float angle;
	float sp, sy, cp, cy;
	vec3_t forward;
	float dist;

	dist = 64;

	if ( !avelocities[0][0] )
	{
		for ( i = 0; i < NUMVERTEXNORMALS; i++ )
		{
			for ( j = 0; j < 3; j++ )
				avelocities[i][j] = RandomFloat( 0, 2.55f );
		}
	}

	for ( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		angle = (float)( cl.time * avelocities[i][0] );
		sy = (float)sin( (double)angle );
		cy = (float)cos( (double)angle );
		angle = (float)( cl.time * avelocities[i][1] );
		sp = (float)sin( (double)angle );
		cp = (float)cos( (double)angle );

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + 0.01 );
		p->color = 111;
		p->packedColor = hlRGB( host_basepal, p->color );
		p->type = pt_explode;

		for ( j = 0; j < 3; j++ )
			p->org[j] = ent->origin[j] + r_avertexnormals[i][j] * dist + forward[j] * beamlength;
	}
}

void R_ReadPointFile_f()
{
	FILE* f;
	vec3_t org;
	int c;
	particle_t* p;
	char base[256];
	char name[MAX_PATH];

	if ( !cl.worldmodel )
		return;

	COM_StripExtension( cl.worldmodel->name, base );
	snprintf( name, sizeof( name ), "%s.pts", base );
	COM_FixSlashes( name );

	f = fopen( name, "rb" );
	if ( !f )
	{
		Con_Printf( "couldn't open %s\n", name );
		return;
	}

	Con_Printf( "Reading %s...\n", name );
	c = 0;
	while ( fscanf( f, "%f %f %f\n", &org[0], &org[1], &org[2] ) == 3 )
	{
		c++;

		if ( !free_particles )
		{
			Con_Printf( "Not enough free particles\n" );
			break;
		}

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = 99999;
		p->color = (short)( -c & 15 );
		p->packedColor = hlRGB( host_basepal, p->color );
		p->type = pt_static;

		VectorCopy( org, p->org );
		VectorCopy( vec3_origin, p->vel );
	}

	fclose( f );
	Con_Printf( "%i points read\n", c );
}

void R_BeamKill( int deadEntity )
{
	BEAM* pbeam;

	for ( pbeam = gpActiveBeams; pbeam; pbeam = pbeam->next )
	{
		if ( ( pbeam->flags & FBEAM_STARTENTITY ) && pbeam->startEntity == deadEntity )
		{
			pbeam->flags &= ~( FBEAM_STARTENTITY | FBEAM_FOREVER );

			if ( pbeam->type != TE_BEAMFOLLOW )
				pbeam->die = (float)cl.time;
		}

		if ( ( pbeam->flags & FBEAM_ENDENTITY ) && pbeam->endEntity == deadEntity )
		{
			pbeam->flags &= ~( FBEAM_ENDENTITY | FBEAM_FOREVER );
			pbeam->die = (float)cl.time;
		}
	}
}

#else

#include "client.h"
#include "common.h"
#include "mathlib.h"
#include "zone.h"
#include "conprint.h"
#include "glquake.h"
#include "gl_model.h"
#include "render.h"
#include "cvar.h"
#include "enginecallbacks.h"
#include "../common/triangleapi.h"
#include "customentity.h"
#include "../pm_shared/pm_movevars.h"
#include "../common/beamdef.h"
#include "../common/particledef.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

// Max simultaneous beams
#define MAX_BEAMS 128
#define MAX_PARTICLES 2048 // default max # of particles at one
										//  time
#define ABSOLUTE_MIN_PARTICLES 512 // no fewer than this no matter what's
										//  on the command line

#define NOISE_DIVISIONS 128

extern int particletexture;
extern vec3_t r_origin, vpn, vup, vright;
extern qboolean filterMode;
extern float filterColorRed, filterColorGreen, filterColorBlue, filterBrightness;
extern unsigned short* host_basepal;
extern float gWorldToScreen[16];
extern float gScreenToWorld[16];
extern model_t* cl_sprite_dot;
extern cvar_t tracerRed, tracerGreen, tracerBlue, tracerAlpha;
extern cvar_t tracerLength;
extern cvar_t egon_amplitude;
extern float r_avertexnormals[NUMVERTEXNORMALS][3];

void GL_Bind( int texture );
static void R_TracerDraw();
void R_BeamDrawList();
void R_BeamDraw( BEAM* pbeam, float frametime );
void R_BeamSetup( BEAM* pbeam, vec_t* start, vec_t* end, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed );
void SetBeamAttributes( BEAM* pbeam, float r, float g, float b, float framerate, int startFrame );
void Noise( float* noise, int divs );
cl_entity_t* R_GetBeamAttachmentEntity( int index );
float* R_BeamGetAttachmentPoint( cl_entity_t* ent, int attachment );
int R_BeamCull( vec_t* start, vec_t* end, int pvsOnly );
particle_t* R_TracerParticles( vec_t* org, vec_t* vel, float life );
void R_DrawSegs(
	vec_t* source, vec_t* delta, float width, float scale, float freq, float speed, int segments, int flags );
void R_DrawTorus( vec_t* source, vec_t* delta, float width, float scale, float freq, float speed, int segments );
void R_DrawBeamFollow( BEAM* pbeam );
void R_DrawRing( vec_t* source, vec_t* delta, float width, float amplitude, float freq, float speed, int segments );
qboolean R_CullBox( vec_t* mins, vec_t* maxs );

static BEAM* gpActiveBeams;
static BEAM* gpFreeBeams;
particle_t* free_particles;
int cl_numbeamentities;
cl_entity_t* cl_beamentities[MAX_BEAMENTS];
particle_t* active_particles;

int ramp1[8] = { 111, 109, 107, 105, 103, 101, 99, 97 };

static BEAM* gBeams;
int r_numparticles;
particle_t* particles;
particle_t* gpActiveTracers;

int ramp2[8] = { 111, 110, 109, 108, 107, 106, 104, 102 };
int ramp3[8] = { 109, 107, 6, 5, 4, 3, 0, 0 };

int gSparkRamp[9] = { 254, 253, 252, 111, 110, 109, 108, 103, 96 };

color24 gTracerColors[12] = {
	{ 255, 255, 255 },
	{ 255, 0, 0 },
	{ 0, 255, 0 },
	{ 0, 0, 255 },
	{ 0, 0, 0 },
	{ 255, 167, 17 },
	{ 255, 130, 90 },
	{ 55, 60, 144 },
	{ 255, 130, 90 },
	{ 255, 140, 90 },
	{ 200, 130, 90 },
	{ 255, 120, 70 },
};
static float gTracerSize[10] = { 1.5f, 0.5f, 1, 1, 1, 1, 1, 1, 1, 1 };

float gNoise[NOISE_DIVISIONS + 1];

vec3_t avelocities[NUMVERTEXNORMALS];
float beamlength = 16;

void R_FreeDeadParticles( particle_t** ppparticles )
{
	particle_t* p;

	while ( *ppparticles )
	{
		p = *ppparticles;
		if ( !( cl.time > p->die ) )
		{
			ppparticles = &p->next;
			continue;
		}

		if ( p->deathfunc )
			p->deathfunc( p );
		p->deathfunc = NULL;

		*ppparticles = p->next;
		p->next = free_particles;
		free_particles = p;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Iterates through active list and kills beams associated with deadEntity
// Input  : deadEntity -
//-----------------------------------------------------------------------------
void R_KillDeadBeams( int deadEntity )
{
	BEAM* pbeam;
	BEAM* pnewlist;
	BEAM* pnext;
	particle_t* pHead;

	pbeam = gpActiveBeams; // Old list.
	pnewlist = NULL; // New list.

	while ( pbeam )
	{
		pnext = pbeam->next;
		if ( pbeam->startEntity != deadEntity ) // Link into new list.
		{
			pbeam->next = pnewlist;
			pnewlist = pbeam;

			pbeam = pnext;
			continue;
		}

		pbeam->flags &= ~( FBEAM_STARTENTITY | FBEAM_ENDENTITY );
		if ( pbeam->type != TE_BEAMFOLLOW )
		{
			// Die Die Die!
			pbeam->die = (float)( cl.time - 0.1 );

			// Kill off particles
			for ( pHead = pbeam->particles; pHead; pHead = pHead->next )
				pHead->die = (float)( cl.time - 0.1 );

			// Free particles that have died off.
			R_FreeDeadParticles( &pbeam->particles );

			// Clear us out
			Q_memset( pbeam, 0, sizeof( *pbeam ) );

			// Now link into free list;
			pbeam->next = gpFreeBeams;
			gpFreeBeams = pbeam;
		}
		else
		{
			// Stay active
			pbeam->next = pnewlist;
			pnewlist = pbeam;
		}
		pbeam = pnext;
	}

	// We now have a new list with the bogus stuff released.
	gpActiveBeams = pnewlist;
}

/*
===============
R_RunParticleEffect

===============
*/
void R_RunParticleEffect( vec_t* org, vec_t* dir, int color, int count )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		if ( count == 1024 )
		{
			// rocket explosion
			p->die = (float)( cl.time + 5.0 );
			p->color = (short)ramp1[0];
			p->packedColor = 0;
			p->ramp = (float)RandomLong( 0, 3 );
			p->type = ( i & 1 ) ? pt_explode : pt_explode2;

			for ( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + RandomFloat( -16, 16 );
				p->vel[j] = RandomFloat( -256, 256 );
			}
		}
		else
		{
			p->die = (float)( cl.time + RandomFloat( 0, 0.4f ) );
			p->color = (short)( ( color & ~7 ) + RandomLong( 0, 7 ) );
			p->packedColor = 0;
			p->type = pt_slowgrav;

			for ( j = 0; j < 3; j++ )
			{
				p->org[j] = org[j] + RandomFloat( -8, 8 );
				p->vel[j] = dir[j] * 15;
			}
		}
	}
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect()
{
	vec3_t org, dir;
	int i, count, msgcount, color;

	for ( i = 0; i < 3; i++ )
		org[i] = MSG_ReadCoord( &net_message );
	for ( i = 0; i < 3; i++ )
		dir[i] = (float)( MSG_ReadChar() * ( 1.0 / 16 ) );
	msgcount = MSG_ReadByte();
	color = MSG_ReadByte();

	if ( msgcount == 255 )
		count = 1024;
	else
		count = msgcount;

	R_RunParticleEffect( org, dir, color, count );
}

void R_BeamInit()
{
	gBeams = (BEAM*)Hunk_AllocName( MAX_BEAMS * sizeof( BEAM ), "lightning" );
}

void R_BeamClear()
{
	int i;

	gpActiveBeams = NULL;
	gpFreeBeams = &gBeams[0];

	Q_memset( gBeams, 0, MAX_BEAMS * sizeof( BEAM ) );

	for ( i = 0; i < MAX_BEAMS; i++ )
		gBeams[i].next = &gBeams[i + 1];
	gBeams[MAX_BEAMS - 1].next = NULL;
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles()
{
	int i;

	i = COM_CheckParm( "-particles" );
	if ( i )
		r_numparticles = Q_atoi( com_argv[i + 1] );
	else
		r_numparticles = MAX_PARTICLES;

	if ( r_numparticles < ABSOLUTE_MIN_PARTICLES )
		r_numparticles = ABSOLUTE_MIN_PARTICLES;

	particles = (particle_t*)Hunk_AllocName( r_numparticles * sizeof( particle_t ), "particles" );

	R_BeamInit();
}

/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles()
{
	int i;

	free_particles = &particles[0];
	active_particles = NULL;
	gpActiveTracers = NULL;

	for ( i = 0; i < r_numparticles; i++ )
		particles[i].next = &particles[i + 1];
	particles[r_numparticles - 1].next = NULL;

	R_BeamClear();
}

particle_t* R_AllocParticle( void ( *callback )( particle_t* particle, float frametime ) )
{
	particle_t* p;

	if ( !free_particles )
	{
		Con_Printf( "Not enough free particles\n" );
		return NULL;
	}

	p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;

	p->type = pt_clientcustom;
	p->callback = callback;
	p->color = 0;
	p->deathfunc = NULL;
	p->die = (float)cl.time;
	VectorCopy( vec3_origin, p->org );
	VectorCopy( vec3_origin, p->vel );
	p->packedColor = 0;
	p->ramp = 0;

	return p;
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles()
{
	particle_t* p;
	float grav;
	int i;
	float time1, time2, time3;
	float dvel;
	float frametime;
	float scale;
	vec3_t up, right;
	unsigned short* pb;
	byte rgba[4];

	GL_Bind( particletexture );
	qglEnable( GL_ALPHA_TEST );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglBegin( GL_TRIANGLES );

	VectorScale( vup, 1.5f, up );
	VectorScale( vright, 1.5f, right );

	frametime = (float)( cl.time - cl.oldtime );
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = (float)( frametime * movevars.gravity * 0.05 );
	dvel = frametime * 4;

	R_FreeDeadParticles( &active_particles );

	for ( p = active_particles; p; p = p->next )
	{
		if ( p->type != pt_blob )
		{
			// hack a scale up to keep particles from disapearing
			scale = ( p->org[0] - r_origin[0] ) * vpn[0] + ( p->org[1] - r_origin[1] ) * vpn[1]
				  + ( p->org[2] - r_origin[2] ) * vpn[2];
			if ( scale >= 20 )
				scale = (float)( 1 + scale * 0.004 );
			else
				scale = 1;

			pb = &host_basepal[p->color * 4];
			if ( filterMode )
			{
				rgba[0] = (byte)(int)( pb[2] * filterColorRed * filterBrightness );
				rgba[1] = (byte)(int)( pb[1] * filterColorGreen * filterBrightness );
				rgba[2] = (byte)(int)( pb[0] * filterColorBlue * filterBrightness );
				rgba[3] = (byte)(int)( 255 * filterBrightness * filterBrightness );
			}
			else
			{
				rgba[0] = (byte)pb[2];
				rgba[1] = (byte)pb[1];
				rgba[2] = (byte)pb[0];
				rgba[3] = 255;
			}

			qglColor4ubv( rgba );
			qglTexCoord2f( 0, 0 );
			qglVertex3fv( p->org );
			qglTexCoord2f( 1, 0 );
			qglVertex3f( p->org[0] + up[0] * scale, p->org[1] + up[1] * scale, p->org[2] + up[2] * scale );
			qglTexCoord2f( 0, 1 );
			qglVertex3f( p->org[0] + right[0] * scale, p->org[1] + right[1] * scale, p->org[2] + right[2] * scale );
		}

		if ( p->type != pt_clientcustom )
		{
			for ( i = 0; i < 3; i++ )
				p->org[i] += p->vel[i] * frametime;
		}

		switch ( p->type )
		{
		case pt_grav:
			p->vel[2] -= grav * 20;
			break;

		case pt_slowgrav:
			p->vel[2] = grav;
			break;

		case pt_fire:
			p->ramp += time1;

			if ( p->ramp < 6 )
			{
				p->color = (short)ramp3[(int)p->ramp];
				p->packedColor = 0;
			}
			else
			{
				p->die = -1;
			}

			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;

			if ( p->ramp < 8 )
			{
				p->color = (short)ramp1[(int)p->ramp];
				p->packedColor = 0;
			}
			else
			{
				p->die = -1;
			}

			for ( i = 0; i < 3; i++ )
				p->vel[i] += p->vel[i] * dvel;

			p->vel[2] -= grav;
			break;

		case pt_explode2:
			p->ramp += time3;

			if ( p->ramp < 8 )
			{
				p->color = (short)ramp2[(int)p->ramp];
				p->packedColor = 0;
			}
			else
			{
				p->die = -1;
			}

			for ( i = 0; i < 3; i++ )
				p->vel[i] -= p->vel[i] * frametime;

			p->vel[2] -= grav;
			break;

		case pt_blob:
		case pt_blob2:
			p->ramp += time2;

			if ( p->ramp >= 9 )
				p->ramp = 0;

			p->color = (short)gSparkRamp[(int)p->ramp];
			p->packedColor = 0;

			p->vel[0] = (float)( p->vel[0] - p->vel[0] * 0.5 * frametime );
			p->vel[1] = (float)( p->vel[1] - p->vel[1] * 0.5 * frametime );
			p->vel[2] -= grav * 5;

			if ( RandomLong( 0, 3 ) )
				p->type = pt_blob;
			else
				p->type = pt_blob2;
			break;

		case pt_vox_slowgrav:
			p->vel[2] -= grav * 4;
			break;

		case pt_vox_grav:
			p->vel[2] -= grav * 8;
			break;

		case pt_clientcustom:
			if ( p->callback )
				p->callback( p, frametime );
			break;

		default:
			break;
		}
	}

	qglEnd();

	R_TracerDraw();
	R_BeamDrawList();

	qglDisable( GL_BLEND );
	qglDisable( GL_ALPHA_TEST );
}

int ScreenTransform( vec_t* point, vec_t* screen )
{
	float w;

	screen[0] = gWorldToScreen[0] * point[0] + gWorldToScreen[4] * point[1] + gWorldToScreen[8] * point[2]
			  + gWorldToScreen[12];
	screen[1] = gWorldToScreen[1] * point[0] + gWorldToScreen[5] * point[1] + gWorldToScreen[9] * point[2]
			  + gWorldToScreen[13];
	w = gWorldToScreen[3] * point[0] + gWorldToScreen[7] * point[1] + gWorldToScreen[11] * point[2]
	  + gWorldToScreen[15];

	if ( w != 0.0f )
	{
		w = (float)( 1.0 / w );
		screen[0] *= w;
		screen[1] *= w;
	}

	return w <= 0.0f;
}

void WorldTransform( vec_t* screen, vec_t* point )
{
	int i;
	float w;

	for ( i = 0; i < 3; i++ )
	{
		point[i] = gScreenToWorld[i] * screen[0] + gScreenToWorld[i + 4] * screen[1] + gScreenToWorld[i + 8] * screen[2]
				 + gScreenToWorld[i + 12];
	}

	w = gScreenToWorld[3] * screen[0] + gScreenToWorld[7] * screen[1] + gScreenToWorld[11] * screen[2]
	  + gScreenToWorld[15];

	if ( w != 0.0f )
	{
		w = (float)( 1.0 / w );
		VectorScale( point, w, point );
	}
}

static void R_TracerDraw()
{
	int i;
	float scale;
	float attenuation;
	float frametime;
	float clipDist;
	float gravity;
	float size;
	float fraction, dist1, dist2;
	vec3_t start, end;
	particle_t* p;
	vec3_t screenLast, screen;
	vec3_t tmp, normal;
	color24* pColor;
	qboolean draw;

	if ( !gpActiveTracers )
		return;

	gTracerColors[4].r = (byte)(int)( tracerRed.value * tracerAlpha.value * 255 );
	gTracerColors[4].g = (byte)(int)( tracerGreen.value * tracerAlpha.value * 255 );
	gTracerColors[4].b = (byte)(int)( tracerBlue.value * tracerAlpha.value * 255 );

	R_FreeDeadParticles( &gpActiveTracers );

	frametime = (float)( cl.time - cl.oldtime );

	if ( !tri.SpriteTexture( cl_sprite_dot, 0 ) )
		return;

	gravity = frametime * movevars.gravity;
	size = DotProduct( r_origin, vpn );
	scale = fmaxf( 0.0f, (float)( 1.0 - frametime * 0.9 ) );

	tri.RenderMode( kRenderTransAdd );
	tri.CullFace( TRI_NONE );

	for ( p = gpActiveTracers; p; p = p->next )
	{
		pColor = &gTracerColors[p->color];

		if ( p->die - 0.1 <= cl.time )
			attenuation = (float)( ( p->die - cl.time ) * p->ramp );
		else
			attenuation = (float)( p->ramp * 0.1 );

		VectorCopy( p->org, start );
		VectorMA( p->org, attenuation, p->vel, end );

		draw = TRUE;

		if ( ScreenTransform( start, screen ) || ScreenTransform( end, screenLast ) )
		{
			dist1 = DotProduct( start, vpn ) - size;
			dist2 = DotProduct( end, vpn ) - size;
			clipDist = dist2 - dist1;

			if ( ( dist1 > 0 || dist2 > 0 ) && clipDist >= 0.01 )
			{
				fraction = dist1 / clipDist;

				if ( dist1 > 0 )
				{
					for ( i = 0; i < 3; i++ )
						end[i] = start[i] + ( end[i] - start[i] ) * fraction;
				}
				else
				{
					for ( i = 0; i < 3; i++ )
						start[i] += ( end[i] - start[i] ) * fraction;
				}
			}
			else
			{
				draw = FALSE;
			}
		}

		if ( draw )
		{
			// Transform point into screen space
			ScreenTransform( start, screen );
			ScreenTransform( end, screenLast );

			// Build world-space normal to screen-space direction vector
			tmp[0] = screenLast[0] - screen[0];
			tmp[1] = screenLast[1] - screen[1];
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );

			VectorScale( vup, tmp[0] * gTracerSize[p->type], normal );
			VectorMA( normal, -tmp[1] * gTracerSize[p->type], vright, normal );

			tri.Begin( TRI_QUADS );
			tri.Color4ub( pColor->r, pColor->g, pColor->b, (byte)p->packedColor );

			tri.Brightness( 0 );
			tri.TexCoord2f( 0, 0 );
			tri.Vertex3f( start[0] + normal[0], start[1] + normal[1], start[2] + normal[2] );

			tri.Brightness( 1 );
			tri.TexCoord2f( 0, 1 );
			tri.Vertex3f( end[0] + normal[0], end[1] + normal[1], end[2] + normal[2] );

			tri.Brightness( 1 );
			tri.TexCoord2f( 1, 1 );
			tri.Vertex3f( end[0] - normal[0], end[1] - normal[1], end[2] - normal[2] );

			tri.Brightness( 0 );
			tri.TexCoord2f( 1, 0 );
			tri.Vertex3f( start[0] - normal[0], start[1] - normal[1], start[2] - normal[2] );

			tri.End();
		}

		VectorMA( p->org, frametime, p->vel, p->org );

		if ( p->type == pt_grav )
		{
			p->vel[0] *= scale;
			p->vel[1] *= scale;
			p->vel[2] -= gravity;

			p->packedColor = (short)(int)( ( p->die - cl.time ) * 2 * 255 );
			if ( p->packedColor > 255 )
				p->packedColor = 255;
		}
		else if ( p->type == pt_slowgrav )
		{
			p->vel[2] = (float)( gravity * 0.05 );
		}
	}

	tri.CullFace( TRI_FRONT );
	tri.RenderMode( kRenderNormal );
}

void R_DrawBeamEntList( float frametime )
{
	int i;
	int beamType;
	BEAM beam;
	cl_entity_t* ent;

	for ( i = 0; i < cl_numbeamentities; i++ )
	{
		ent = cl_beamentities[i];

		// Set up the beam.
		beamType = ent->curstate.rendermode & 0x0F;

		R_BeamSetup( &beam, ent->origin, ent->curstate.angles, ent->curstate.modelindex, 0, ent->curstate.scale,
			(float)( ent->curstate.body * 0.01 ), (float)( CL_FxBlend( ent ) * ( 1.0 / 255.0 ) ),
			ent->curstate.animtime );

		SetBeamAttributes( &beam, (float)( ent->curstate.rendercolor.r * ( 1.0 / 255.0 ) ),
			(float)( ent->curstate.rendercolor.g * ( 1.0 / 255.0 ) ),
			(float)( ent->curstate.rendercolor.b * ( 1.0 / 255.0 ) ), 0, (int)ent->curstate.frame );

		// Handle code from relinking.
		switch ( beamType )
		{
		case BEAM_ENTPOINT:
			beam.type = TE_BEAMPOINTS;
			beam.flags = FBEAM_ENDENTITY;
			beam.startEntity = 0;
			beam.endEntity = ent->curstate.skin;
			break;

		case BEAM_ENTS:
			beam.type = TE_BEAMPOINTS;
			beam.flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;
			beam.startEntity = ent->curstate.sequence;
			beam.endEntity = ent->curstate.skin;
			break;
		}

		beam.flags |= ent->curstate.rendermode & ( FBEAM_SINENOISE | FBEAM_SOLID | FBEAM_SHADEIN | FBEAM_SHADEOUT );
		beam.pFollowModel = NULL;

		// Draw it
		R_BeamDraw( &beam, frametime );
	}
}

void R_BeamDrawList()
{
	BEAM* pbeam;
	BEAM* pkill;
	float frametime;

	if ( !gpActiveBeams && !cl_numbeamentities )
		return;

	frametime = (float)( cl.time - cl.oldtime );

	qglDisable( GL_ALPHA_TEST );
	qglDepthMask( GL_FALSE );
	tri.CullFace( TRI_NONE );

	// kill all the ones hanging direcly off the base pointer
	while ( gpActiveBeams && !( gpActiveBeams->flags & FBEAM_FOREVER ) && cl.time > gpActiveBeams->die )
	{
		pkill = gpActiveBeams;
		gpActiveBeams = pkill->next;
		pkill->next = gpFreeBeams;
		gpFreeBeams = pkill;
	}

	for ( pbeam = gpActiveBeams; pbeam; pbeam = pbeam->next )
	{
		while ( pbeam->next && !( pbeam->next->flags & FBEAM_FOREVER ) && !( cl.time < pbeam->next->die ) )
		{
			pkill = pbeam->next;
			pbeam->next = pkill->next;
			pkill->next = gpFreeBeams;
			gpFreeBeams = pkill;
		}

		R_BeamDraw( pbeam, frametime );
	}

	R_DrawBeamEntList( frametime );

	qglDepthMask( GL_TRUE );
	tri.CullFace( TRI_FRONT );
	tri.RenderMode( kRenderNormal );
}
void SineNoise( float* noise, int divs )
{
	int i;
	float freq;
	float step = (float)( M_PI / divs );

	freq = 0;
	for ( i = 0; i < divs; i++ )
	{
		noise[i] = (float)sin( (double)freq );
		freq += step;
	}
}

void R_DrawDisk(
	vec_t* source, vec_t* delta, float, float, float freq, float speed, int segments )
{
	int i;
	float div, length, fraction, vLast, vStep;
	vec3_t point;
	float w;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS ) // UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS;

	div = (float)( 1.0 / ( segments - 1 ) );

	// Don't lose all of the noise/texture on short beams
	length = fmaxf( 0.5f, (float)( Length( delta ) * 0.01 ) );

	vStep = length * div; // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = (float)fmod( (double)( freq * speed ), 1.0 );

	w = freq * delta[2];

	for ( i = 0; i < segments; i++ )
	{
		VectorCopy( source, point );

		fraction = i * div;

		tri.Brightness( 1 );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( point );

		point[0] = (float)( sin( fraction * 2 * M_PI ) * w + source[0] );
		point[1] = (float)( cos( fraction * 2 * M_PI ) * w + source[1] );
		point[2] = source[2];

		tri.Brightness( 1 );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( point );

		vLast += vStep; // Advance texture scroll (v axis only)
		vLast = (float)fmod( (double)vLast, 1.0 );
	}
}

void R_DrawCylinder(
	vec_t* source, vec_t* delta, float width, float, float freq, float speed, int segments )
{
	int i;
	float div, length, fraction, vLast, vStep;
	double s, c;
	vec3_t point;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS ) // UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS;

	div = (float)( 1.0 / ( segments - 1 ) );

	// Don't lose all of the noise/texture on short beams
	length = fmaxf( 0.5f, (float)( Length( delta ) * 0.01 ) );

	vStep = length * div; // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = (float)fmod( (double)( freq * speed ), 1.0 );

	for ( i = 0; i < segments; i++ )
	{
		fraction = i * div;
		s = sin( fraction * 2 * M_PI ) * freq;
		c = cos( fraction * 2 * M_PI ) * freq;

		point[0] = (float)( s * delta[2] + source[0] );
		point[1] = (float)( c * delta[2] + source[1] );
		point[2] = source[2] + width;

		tri.Brightness( 0 );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( point );

		point[0] = (float)( s * ( delta[2] + width ) + source[0] );
		point[1] = (float)( c * ( delta[2] + width ) + source[1] );
		point[2] = source[2] - width;

		tri.Brightness( 1 );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( point );

		vLast += vStep; // Advance texture scroll (v axis only)
		vLast = (float)fmod( (double)vLast, 1.0 );
	}
}

void R_BeamSetup( BEAM* pbeam, vec_t* start, vec_t* end, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed )
{
	model_t* sprite;

	sprite = CL_GetModelByIndex( modelIndex );
	if ( !sprite )
		return;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->modelIndex = modelIndex;
	pbeam->frame = 0;
	pbeam->frameRate = 0;
	pbeam->frameCount = ModelFrameCount( sprite );

	VectorCopy( start, pbeam->source );
	VectorCopy( end, pbeam->target );
	VectorSubtract( end, start, pbeam->delta );

	pbeam->freq = (float)( cl.time * speed );
	pbeam->die = (float)( cl.time + life );
	pbeam->amplitude = amplitude;
	pbeam->speed = speed;
	pbeam->width = width;
	pbeam->brightness = brightness;

	if ( amplitude < 0.5 )
		pbeam->segments = (int)( Length( pbeam->delta ) * 0.075 + 3 ); // one per 16 pixels
	else
		pbeam->segments = (int)( Length( pbeam->delta ) * 0.25 + 3 ); // one per 4 pixels

	pbeam->flags = 0;
	pbeam->pFollowModel = NULL;
}

void R_BeamDraw( BEAM* pbeam, float frametime )
{
	model_t* sprite;
	cl_entity_t* ent;
	float* attachment;
	vec3_t difference;
	vec3_t org, vel;
	float factor, length;
	particle_t* p;
	int frame;

	if ( pbeam->modelIndex < 0 )
	{
		pbeam->die = (float)cl.time;
		return;
	}

	sprite = CL_GetModelByIndex( pbeam->modelIndex );
	if ( !sprite )
		return;

	if ( pbeam->flags & FBEAM_SOLID )
		tri.RenderMode( kRenderNormal );
	else
		tri.RenderMode( kRenderTransAdd );

	pbeam->freq += frametime;

	// OPTIMIZE: Do this every frame?
	// UNDONE: Do this differentially somehow?
	// Generate fractal noise
	gNoise[0] = 0;
	gNoise[NOISE_DIVISIONS] = 0;
	if ( pbeam->amplitude != 0 )
	{
		if ( pbeam->flags & FBEAM_SINENOISE )
			SineNoise( gNoise, NOISE_DIVISIONS );
		else
			Noise( gNoise, NOISE_DIVISIONS );
	}

	// update end points
	if ( pbeam->flags & ( FBEAM_STARTENTITY | FBEAM_ENDENTITY ) )
	{
		if ( pbeam->flags & FBEAM_STARTENTITY )
		{
			ent = R_GetBeamAttachmentEntity( pbeam->startEntity );
			if ( !ent )
				return;

			if ( ent->model && ( !pbeam->pFollowModel || pbeam->pFollowModel == ent->model ) )
			{
				attachment = R_BeamGetAttachmentPoint( ent, pbeam->startEntity );
				VectorCopy( attachment, pbeam->source );
				pbeam->flags |= FBEAM_STARTVISIBLE;

				if ( !pbeam->pFollowModel )
					pbeam->pFollowModel = ent->model;
			}
			else if ( !( pbeam->flags & FBEAM_FOREVER ) )
			{
				pbeam->flags &= ~FBEAM_STARTENTITY;
			}
		}

		if ( pbeam->flags & FBEAM_ENDENTITY )
		{
			ent = R_GetBeamAttachmentEntity( pbeam->endEntity );
			if ( !ent )
				return;

			if ( !ent->model )
			{
				if ( !( pbeam->flags & FBEAM_FOREVER ) )
				{
					pbeam->flags &= ~FBEAM_ENDENTITY;
					pbeam->die = (float)cl.time;
				}
				return;
			}

			attachment = R_BeamGetAttachmentPoint( ent, pbeam->endEntity );
			VectorCopy( attachment, pbeam->target );
			pbeam->flags |= FBEAM_ENDVISIBLE;
		}

		// If we've never seen the start entity, don't display
		if ( ( pbeam->flags & FBEAM_STARTENTITY ) && !( pbeam->flags & FBEAM_STARTVISIBLE ) )
			return;

		// Compute segments from the new endpoints
		VectorSubtract( pbeam->target, pbeam->source, difference );
		if ( Length( difference ) > 0.0000001 )
			VectorCopy( difference, pbeam->delta );

		if ( pbeam->amplitude < 0.5 )
			pbeam->segments = (int)( Length( pbeam->delta ) * 0.075 + 3 ); // one per 16 pixels
		else
			pbeam->segments = (int)( Length( pbeam->delta ) * 0.25 + 3 ); // one per 4 pixels
	}

	if ( pbeam->type == TE_BEAMPOINTS && !R_BeamCull( pbeam->source, pbeam->target, FALSE ) )
		return;

	frame = (int)( pbeam->frameRate * cl.time + pbeam->frame ) % pbeam->frameCount;
	if ( !tri.SpriteTexture( sprite, frame ) )
		return;

	if ( ( pbeam->flags & FBEAM_SINENOISE ) && egon_amplitude.value > 0 )
	{
		factor = pbeam->amplitude * egon_amplitude.value;

		VectorMA( pbeam->target, (float)( sin( (double)( pbeam->freq * 10 ) ) * factor ), vup, org );
		VectorMA( org, (float)( cos( (double)( pbeam->freq * 10 ) ) * factor ), vright, org );

		VectorSubtract( pbeam->source, org, vel );
		length = Length( vel );
		if ( length != 0 )
			VectorScale( vel, (float)( 1000.0 / length ), vel );

		p = R_TracerParticles( org, vel, (float)( length * 0.001 ) );
		if ( p )
			p->color = 7;
	}

	// update life cycle
	ScopedLegacyBeamColors legacyColors( ( pbeam->flags & FBEAM_LEGACY_COLOR255 ) != 0 );
	pbeam->t = (float)( pbeam->freq + ( pbeam->die - cl.time ) );
	if ( pbeam->t != 0 )
		pbeam->t = (float)( 1.0 - pbeam->freq / pbeam->t );

	if ( pbeam->flags & FBEAM_FADEIN )
		tri.Color4f( pbeam->r, pbeam->g, pbeam->b, pbeam->t * pbeam->brightness );
	else if ( pbeam->flags & FBEAM_FADEOUT )
		tri.Color4f( pbeam->r, pbeam->g, pbeam->b, (float)( ( 1.0 - pbeam->t ) * pbeam->brightness ) );
	else
		tri.Color4f( pbeam->r, pbeam->g, pbeam->b, pbeam->brightness );

	switch ( pbeam->type )
	{
	case TE_BEAMPOINTS:
		tri.Begin( TRI_QUADS );
		R_DrawSegs( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments, pbeam->flags );
		break;

	case TE_BEAMTORUS:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawTorus( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments );
		break;

	case TE_BEAMDISK:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawDisk( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments );
		break;

	case TE_BEAMCYLINDER:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawCylinder( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments );
		break;

	case TE_BEAMFOLLOW:
		tri.Begin( TRI_QUADS );
		R_DrawBeamFollow( pbeam );
		break;

	case TE_BEAMRING:
		tri.Begin( TRI_QUAD_STRIP );
		R_DrawRing( pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed,
			pbeam->segments );
		break;

	default:
		return;
	}

	tri.End();
}

void R_DrawRing( vec_t* source, vec_t* delta, float width, float amplitude, float freq, float speed, int segments )
{
	int i, j, noiseIndex, noiseStep;
	float div, length, fraction, factor, vLast, vStep;
	vec3_t last1, last2, point, screen, screenLast, tmp, normal;
	vec3_t center, xaxis, yaxis, mins, maxs;
	float radius, x, y, scale;

	if ( segments < 2 )
		return;

	segments = (int)( segments * M_PI );

	if ( segments > NOISE_DIVISIONS * 8 ) // UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS * 8;

	// Don't lose all of the noise/texture on short beams
	length = fmaxf( 0.5f, (float)( Length( delta ) * 0.01 * M_PI ) );

	div = (float)( 1.0 / ( segments - 1 ) );

	vStep = (float)( length * div / 8.0 ); // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = (float)fmod( (double)( freq * speed ), 1.0 );
	scale = (float)( amplitude * length / 8.0 );

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)( NOISE_DIVISIONS * div * 65536.0 ) * 8;
	noiseIndex = 0;

	VectorScale( delta, 0.5f, delta );
	VectorAdd( source, delta, center );

	VectorCopy( delta, xaxis );
	radius = Length( xaxis );

	mins[0] = center[0] - radius;
	mins[1] = center[1] - radius;
	mins[2] = center[2] - scale;
	maxs[0] = center[0] + radius;
	maxs[1] = center[1] + radius;
	maxs[2] = center[2] + scale;

	if ( !cl.worldmodel )
		return;

	// Is that box in PVS && frustum?
	if ( !PVSNode( cl.worldmodel->nodes, mins, maxs ) || R_CullBox( mins, maxs ) )
		return;

	yaxis[0] = xaxis[1];
	yaxis[1] = -xaxis[0];
	yaxis[2] = 0;
	VectorNormalize( yaxis );
	VectorScale( yaxis, radius, yaxis );

	j = segments / 8;

	for ( i = 0; i < segments + 1; i++ )
	{
		fraction = i * div;
		x = (float)sin( fraction * 2 * M_PI );
		y = (float)cos( fraction * 2 * M_PI );

		point[0] = yaxis[0] * y + xaxis[0] * x + center[0];
		point[1] = yaxis[1] * y + xaxis[1] * x + center[1];
		point[2] = yaxis[2] * y + xaxis[2] * x + center[2];

		// Distort using noise
		factor = gNoise[( noiseIndex >> 16 ) & ( NOISE_DIVISIONS - 1 )] * scale;
		VectorMA( point, factor, vup, point );

		// Rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
		factor = (float)( factor * cos( fraction * M_PI * 3 * 8 + freq ) );
		VectorMA( point, factor, vright, point );

		// Transform source into screen space
		ScreenTransform( point, screen );

		if ( i != 0 )
		{
			// Build world-space normal to screen-space direction vector
			tmp[0] = screen[0] - screenLast[0];
			tmp[1] = screen[1] - screenLast[1];
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );
			VectorScale( vup, tmp[0], normal ); // Build point along noraml line (normal is -y, x)
			VectorMA( normal, -tmp[1], vright, normal );

			// Make a wide line
			VectorMA( point, width, normal, last1 );
			VectorMA( point, -width, normal, last2 );

			vLast += vStep; // Advance texture scroll (v axis only)
			tri.TexCoord2f( 1, vLast );
			tri.Vertex3fv( last2 );
			tri.TexCoord2f( 0, vLast );
			tri.Vertex3fv( last1 );
		}

		screenLast[0] = screen[0];
		screenLast[1] = screen[1];

		noiseIndex += noiseStep;
		vLast = (float)fmod( (double)vLast, 1.0 );

		j--;
		if ( j == 0 && amplitude != 0 )
		{
			j = segments / 8;
			Noise( gNoise, NOISE_DIVISIONS );
		}
	}
}

void R_DrawBeamFollow( BEAM* pbeam )
{
	particle_t* pnew;
	particle_t* p;
	vec3_t delta, start;
	float fraction;
	float div;
	float frametime;
	vec3_t last1, last2, screen, screenLast, tmp, normal;

	R_FreeDeadParticles( &pbeam->particles );

	p = pbeam->particles;
	pnew = NULL;

	div = 0;
	if ( pbeam->flags & FBEAM_STARTENTITY )
	{
		if ( p )
		{
			VectorSubtract( p->org, pbeam->source, delta );
			div = Length( delta );
			if ( div >= 32 && free_particles )
			{
				pnew = free_particles;
				free_particles = pnew->next;
			}
		}
		else if ( free_particles )
		{
			pnew = free_particles;
			free_particles = pnew->next;
		}
	}

	if ( pnew )
	{
		VectorCopy( pbeam->source, pnew->org );
		VectorClear( pnew->vel );
		pnew->die = (float)( cl.time + pbeam->amplitude );
		pbeam->die = (float)( cl.time + pbeam->amplitude );

		pnew->next = p;
		pbeam->particles = pnew;
		p = pnew;
	}

	if ( !p )
		return;

	if ( !pnew && div != 0 )
	{
		VectorCopy( pbeam->source, start );
		ScreenTransform( pbeam->source, screenLast );
	}
	else if ( p->next )
	{
		VectorCopy( p->org, start );
		ScreenTransform( p->org, screenLast );
		p = p->next;
	}
	else
	{
		return;
	}

	ScreenTransform( p->org, screen );

	// Build world-space normal to screen-space direction vector
	tmp[0] = screen[0] - screenLast[0];
	tmp[1] = screen[1] - screenLast[1];
	// We don't need Z, we're in screen space
	tmp[2] = 0;
	VectorNormalize( tmp );
	VectorScale( vup, tmp[0], normal );
	VectorMA( normal, -tmp[1], vright, normal );

	// Make a wide line
	VectorMA( start, pbeam->width, normal, last1 );
	VectorMA( start, -pbeam->width, normal, last2 );

	div = (float)( 1.0 / pbeam->amplitude );
	fraction = (float)( ( pbeam->die - cl.time ) * div );

	for ( ; p; p = p->next )
	{
		tri.Brightness( fraction );
		tri.TexCoord2f( 0, 0 );
		tri.Vertex3fv( last1 );

		tri.Brightness( fraction );
		tri.TexCoord2f( 1, 0 );
		tri.Vertex3fv( last2 );

		// Transform source into screen space
		ScreenTransform( p->org, screen );

		// Build world-space normal to screen-space direction vector
		tmp[0] = screen[0] - screenLast[0];
		tmp[1] = screen[1] - screenLast[1];
		// We don't need Z, we're in screen space
		tmp[2] = 0;
		VectorNormalize( tmp );
		VectorScale( vup, tmp[0], normal );
		VectorMA( normal, -tmp[1], vright, normal );

		// Make a wide line
		VectorMA( p->org, pbeam->width, normal, last1 );
		VectorMA( p->org, -pbeam->width, normal, last2 );

		if ( p->next )
			fraction = (float)( ( p->die - cl.time ) * div );
		else
			fraction = 0;

		tri.Brightness( fraction );
		tri.TexCoord2f( 1, 1 );
		tri.Vertex3fv( last2 );

		tri.Brightness( fraction );
		tri.TexCoord2f( 0, 1 );
		tri.Vertex3fv( last1 );

		screenLast[0] = screen[0];
		screenLast[1] = screen[1];
	}

	// Drift popcorn trail if there is a velocity
	frametime = (float)( cl.time - cl.oldtime );
	for ( p = pbeam->particles; p; p = p->next )
		VectorMA( p->org, frametime, p->vel, p->org );
}

void R_DrawSegs(
	vec_t* source, vec_t* delta, float width, float scale, float freq, float speed, int segments, int flags )
{
	int i, noiseIndex, noiseStep;
	float div, length, fraction, factor, vLast, vStep, brightness;
	vec3_t last1, last2, point, screen, screenLast, tmp, normal;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS ) // UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS;

	div = (float)( 1.0 / ( segments - 1 ) );

	// Don't lose all of the noise/texture on short beams
	length = fmaxf( 0.5f, (float)( Length( delta ) * 0.01 ) );

	vStep = length * div; // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = (float)fmod( (double)( freq * speed ), 1.0 );

	if ( flags & FBEAM_SINENOISE )
	{
		if ( segments < 16 )
			segments = 16;

		scale *= 100;
		length = (float)( segments * 0.1 );
	}
	else
	{
		scale *= length;
	}

	// Transform source into screen space
	ScreenTransform( source, screenLast );
	VectorMA( source, div, delta, point );
	ScreenTransform( point, screen );

	// Build world-space normal to screen-space direction vector
	tmp[0] = screen[0] - screenLast[0];
	tmp[1] = screen[1] - screenLast[1];
	// We don't need Z, we're in screen space
	tmp[2] = 0;
	VectorNormalize( tmp );
	VectorScale( vup, tmp[0], normal ); // Build point along noraml line (normal is -y, x)
	VectorMA( normal, -tmp[1], vright, normal );

	// Make a wide line
	VectorMA( source, width, normal, last1 );
	VectorMA( source, -width, normal, last2 );

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)( NOISE_DIVISIONS * div * 65536.0 );
	noiseIndex = noiseStep;

	// Sine noise beams have different length calculations
	if ( flags & FBEAM_SINENOISE )
		noiseIndex = 0;

	brightness = 1.0f;
	if ( flags & FBEAM_SHADEIN )
		brightness = 0;

	for ( i = 1; i < segments; i++ )
	{
		fraction = i * div;

		tri.Brightness( brightness );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( last1 );
		tri.Brightness( brightness );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( last2 );

		if ( flags & FBEAM_SHADEIN )
			brightness = fraction;
		else if ( flags & FBEAM_SHADEOUT )
			brightness = (float)( 1.0 - fraction );

		VectorMA( source, fraction, delta, point );

		// Distort using noise
		if ( scale != 0 )
		{
			factor = gNoise[( noiseIndex >> 16 ) & ( NOISE_DIVISIONS - 1 )] * scale;

			if ( flags & FBEAM_SINENOISE )
			{
				VectorMA( point, (float)( factor * sin( fraction * M_PI * length + freq ) ), vup, point );
				VectorMA( point, (float)( factor * cos( fraction * M_PI * length + freq ) ), vright, point );
			}
			else
			{
				VectorMA( point, factor, vup, point );
				VectorMA( point, (float)( factor * cos( fraction * M_PI * 3 + freq ) ), vright, point );
			}
		}

		// Transform point into screen space
		ScreenTransform( point, screen );

		// Build world-space normal to screen-space direction vector
		tmp[0] = screen[0] - screenLast[0];
		tmp[1] = screen[1] - screenLast[1];
		// We don't need Z, we're in screen space
		tmp[2] = 0;
		VectorNormalize( tmp );
		VectorScale( vup, tmp[0], normal ); // Build point along noraml line (normal is -y, x)
		VectorMA( normal, -tmp[1], vright, normal );

		// Make a wide line
		VectorMA( point, width, normal, last1 );
		VectorMA( point, -width, normal, last2 );

		vLast += vStep; // Advance texture scroll (v axis only)
		tri.Brightness( brightness );
		tri.TexCoord2f( 1, vLast );
		tri.Vertex3fv( last2 );
		tri.Brightness( brightness );
		tri.TexCoord2f( 0, vLast );
		tri.Vertex3fv( last1 );

		screenLast[0] = screen[0];
		screenLast[1] = screen[1];

		noiseIndex += noiseStep;
		vLast = (float)fmod( (double)vLast, 1.0 );
	}
}

void R_DrawTorus( vec_t* source, vec_t* delta, float width, float scale, float freq, float speed, int segments )
{
	int i, noiseIndex, noiseStep;
	float div, length, fraction, factor, vLast, vStep;
	vec3_t last1, last2, point, screen, screenLast, tmp, normal;

	if ( segments < 2 )
		return;

	if ( segments > NOISE_DIVISIONS ) // UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS;

	div = (float)( 1.0 / ( segments - 1 ) );

	// Don't lose all of the noise/texture on short beams
	length = fmaxf( 0.5f, (float)( Length( delta ) * 0.01 ) );

	vStep = length * div; // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = (float)fmod( (double)( freq * speed ), 1.0 );
	scale *= length;

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)( NOISE_DIVISIONS * div * 65536.0 );
	noiseIndex = 0;

	for ( i = 0; i <= segments; i++ )
	{
		fraction = i * div;

		point[0] = (float)( sin( fraction * 2 * M_PI ) * freq * delta[2] + source[0] );
		point[1] = (float)( cos( fraction * 2 * M_PI ) * freq * delta[2] + source[1] );
		point[2] = source[2];

		// Distort using noise
		factor = gNoise[( noiseIndex >> 16 ) & ( NOISE_DIVISIONS - 1 )] * scale;
		VectorMA( point, factor, vup, point );

		factor = (float)( factor * cos( fraction * M_PI * 3 + freq ) );
		VectorMA( point, factor, vright, point );

		// Transform source into screen space
		ScreenTransform( point, screen );

		if ( i != 0 )
		{
			// Build world-space normal to screen-space direction vector
			tmp[0] = screen[0] - screenLast[0];
			tmp[1] = screen[1] - screenLast[1];
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize( tmp );
			VectorScale( vup, tmp[0], normal ); // Build point along noraml line (normal is -y, x)
			VectorMA( normal, -tmp[1], vright, normal );

			// Make a wide line
			VectorMA( point, width, normal, last1 );
			VectorMA( point, -width, normal, last2 );

			vLast += vStep; // Advance texture scroll (v axis only)
			tri.TexCoord2f( 1, vLast );
			tri.Vertex3fv( last2 );
			tri.TexCoord2f( 0, vLast );
			tri.Vertex3fv( last1 );
		}

		screenLast[0] = screen[0];
		screenLast[1] = screen[1];

		noiseIndex += noiseStep;
		vLast = (float)fmod( (double)vLast, 1.0 );
	}
}

// Fractal noise generator, power of 2 wavelength
void Noise( float* noise, int divs )
{
	int div2;

	if ( divs < 2 )
		return;

	div2 = divs / 2;

	// Noise is normalized to +/- scale
	noise[div2] = (float)( ( noise[0] + noise[divs] ) * 0.5 + (double)RandomFloat( -0.125f, 0.125f ) * (float)divs );
	if ( div2 > 1 )
	{
		Noise( &noise[div2], div2 );
		Noise( noise, div2 );
	}
}

int R_BeamCull( vec_t* start, vec_t* end, int pvsOnly )
{
	vec3_t mins, maxs;
	int i;

	if ( !cl.worldmodel )
		return 0;

	for ( i = 0; i < 3; i++ )
	{
		if ( start[i] < end[i] )
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}

		// Don't let it be zero sized
		if ( mins[i] == maxs[i] )
			maxs[i] += 1;
	}

	// Check bbox
	if ( PVSNode( cl.worldmodel->nodes, mins, maxs ) )
	{
		if ( pvsOnly || !R_CullBox( mins, maxs ) )
			return 1; // Beam is visible
	}

	// Beam is not visible
	return 0;
}

cl_entity_t* R_GetBeamAttachmentEntity( int index )
{
	if ( index < 0 )
		return ClientDLL_GetUserEntity( BEAMENT_ENTITY( -index ) );

	return &cl_entities[BEAMENT_ENTITY( index )];
}

float* R_BeamGetAttachmentPoint( cl_entity_t* ent, int attachment )
{
	int point;

	point = BEAMENT_ATTACHMENT( abs( attachment ) );
	if ( point )
		return ent->attachment[point - 1];

	if ( ent->index == cl.playernum + 1 )
		return cl.simorg;

	return ent->origin;
}

BEAM* R_BeamAlloc()
{
	BEAM* pbeam;

	if ( !gpFreeBeams )
		return NULL;

	pbeam = gpFreeBeams;
	gpFreeBeams = pbeam->next;
	pbeam->next = gpActiveBeams;
	gpActiveBeams = pbeam;

	return pbeam;
}

BEAM* R_BeamLightning(
	vec_t* start, vec_t* end, int modelIndex, float life, float width, float amplitude, float brightness, float speed )
{
	BEAM* pbeam;

	pbeam = R_BeamAlloc();
	if ( !pbeam )
		return NULL;

	pbeam->die = (float)cl.time;

	if ( modelIndex < 0 )
		return NULL;

	R_BeamSetup( pbeam, start, end, modelIndex, life, width, amplitude, brightness, speed );

	return pbeam;
}

void SetBeamAttributes( BEAM* pbeam, float r, float g, float b, float framerate, int startFrame )
{
	pbeam->frameRate = framerate;
	pbeam->frame = (float)startFrame;

	pbeam->r = r;
	pbeam->g = g;
	pbeam->b = b;
}

BEAM* R_BeamEnts( int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM* pbeam;
	cl_entity_t* start;
	cl_entity_t* end;

	start = R_GetBeamAttachmentEntity( startEnt );
	end = R_GetBeamAttachmentEntity( endEnt );
	if ( !start || !end )
		return NULL;

	if ( life != 0 && ( !start->model || !end->model ) )
		return NULL;

	pbeam = R_BeamLightning( vec3_origin, vec3_origin, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = endEnt;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

BEAM* R_BeamEntPoint( int startEnt, vec_t* end, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM* pbeam;
	cl_entity_t* start;

	start = R_GetBeamAttachmentEntity( startEnt );
	if ( !start )
		return NULL;

	if ( life != 0 && !start->model )
		return NULL;

	pbeam = R_BeamLightning( vec3_origin, end, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->flags = FBEAM_STARTENTITY;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = 0;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

BEAM* R_BeamPoints( vec_t* start, vec_t* end, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM* pbeam;

	// Don't start temporary beams out of the PVS
	if ( life != 0 && !R_BeamCull( start, end, TRUE ) )
		return NULL;

	pbeam = R_BeamLightning( start, end, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

BEAM* R_BeamCirclePoints( int type, vec_t* start, vec_t* end, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM* pbeam;

	pbeam = R_BeamLightning( start, end, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = type;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}

BEAM* R_BeamFollow( int startEnt, int modelIndex, float life, float width, float r, float g, float b, float brightness )
{
	BEAM* pbeam;

	pbeam = R_BeamLightning( vec3_origin, vec3_origin, modelIndex, life, width, life, brightness, 1 );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMFOLLOW;
	pbeam->flags = FBEAM_STARTENTITY;

	pbeam->startEntity = startEnt;

	SetBeamAttributes( pbeam, r, g, b, 1, 0 );

	return pbeam;
}

BEAM* R_BeamRing( int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude,
	float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BEAM* pbeam;
	cl_entity_t* start;
	cl_entity_t* end;

	start = R_GetBeamAttachmentEntity( startEnt );
	end = R_GetBeamAttachmentEntity( endEnt );
	if ( !start || !end )
		return NULL;

	if ( life != 0 && ( !start->model || !end->model ) )
		return NULL;

	pbeam = R_BeamLightning( vec3_origin, vec3_origin, modelIndex, life, width, amplitude, brightness, speed );
	if ( !pbeam )
		return NULL;

	pbeam->type = TE_BEAMRING;
	pbeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;

	if ( life == 0 )
		pbeam->flags |= FBEAM_FOREVER;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = endEnt;

	SetBeamAttributes( pbeam, r, g, b, framerate, startFrame );

	return pbeam;
}
/*
===============
R_ParticleExplosion

===============
*/
void R_ParticleExplosion( vec_t* org )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < 1024; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + 5.0 );
		p->color = (short)ramp1[0];
		p->packedColor = 0;
		p->ramp = (float)RandomLong( 0, 3 );
		p->type = ( i & 1 ) ? pt_explode : pt_explode2;

		do
		{
			for ( j = 0; j < 3; j++ )
				p->vel[j] = RandomFloat( -512, 512 );
		} while ( DotProduct( p->vel, p->vel ) > 512 * 512 );

		for ( j = 0; j < 3; j++ )
			p->org[j] = org[j] + p->vel[j] * 0.25f;
	}
}

/*
===============
R_ParticleExplosion2

===============
*/
void R_ParticleExplosion2( vec_t* org, int colorStart, int colorLength )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < 512; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + 0.3 );
		p->packedColor = 0;
		p->color = (short)( colorStart + i % colorLength );
		p->type = pt_blob;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + RandomFloat( -16, 16 );
			p->vel[j] = RandomFloat( -256, 256 );
		}
	}
}

/*
===============
R_BlobExplosion

===============
*/
void R_BlobExplosion( vec_t* org )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < 1024; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + RandomFloat( 1, 1.4f ) );

		if ( i & 1 )
		{
			p->type = pt_blob;
			p->color = (short)RandomLong( 66, 71 );
		}
		else
		{
			p->type = pt_blob2;
			p->color = (short)RandomLong( 150, 155 );
		}
		p->packedColor = 0;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j] + RandomFloat( -16, 16 );
			p->vel[j] = RandomFloat( -256, 256 );
		}
	}
}

/*
===============
R_LavaSplash

===============
*/
void R_LavaSplash( vec_t* org )
{
	int i, j;
	particle_t* p;
	float vel;
	vec3_t dir;

	for ( i = -16; i < 16; i++ )
	{
		for ( j = -16; j < 16; j++ )
		{
			if ( !free_particles )
				return;

			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)( cl.time + RandomFloat( 2, 2.62f ) );
			p->color = (short)RandomLong( 224, 231 );
			p->type = pt_slowgrav;
			p->packedColor = 0;

			dir[0] = j * 8 + RandomFloat( 0, 7 );
			dir[1] = i * 8 + RandomFloat( 0, 7 );
			dir[2] = 256;

			p->org[0] = org[0] + dir[0];
			p->org[1] = org[1] + dir[1];
			p->org[2] = org[2] + RandomFloat( 0, 63 );

			VectorNormalize( dir );
			vel = RandomFloat( 50, 113 );
			VectorScale( dir, vel, p->vel );
		}
	}
}

void R_TeleportSplash( vec_t* org )
{
	int i, j, k;
	particle_t* p;
	float vel;
	vec3_t dir;

	for ( i = -16; i < 16; i += 4 )
	{
		for ( j = -16; j < 16; j += 4 )
		{
			for ( k = -24; k < 32; k += 4 )
			{
				if ( !free_particles )
					return;

				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = (float)( cl.time + RandomFloat( 0.2f, 0.34f ) );
				p->color = (short)RandomLong( 7, 14 );
				p->packedColor = 0;
				p->type = pt_slowgrav;

				dir[0] = (float)( j * 8 );
				dir[1] = (float)( i * 8 );
				dir[2] = (float)( k * 8 );

				p->org[0] = org[0] + i + RandomFloat( 0, 3 );
				p->org[1] = org[1] + j + RandomFloat( 0, 3 );
				p->org[2] = org[2] + k + RandomFloat( 0, 3 );

				VectorNormalize( dir );
				vel = RandomFloat( 50, 113 );
				VectorScale( dir, vel, p->vel );
			}
		}
	}
}

particle_t* R_TracerParticles( vec_t* org, vec_t* vel, float life )
{
	particle_t* p;

	if ( !free_particles )
		return NULL;

	p = free_particles;
	free_particles = p->next;
	p->next = gpActiveTracers;
	gpActiveTracers = p;

	p->die = (float)( cl.time + life );
	p->color = 4;
	p->packedColor = 255;
	p->type = pt_static;
	p->ramp = tracerLength.value;

	VectorCopy( org, p->org );
	VectorCopy( vel, p->vel );

	return p;
}

void R_Implosion( vec_t* end, float radius, int count, float life )
{
	int i;
	float dist;
	vec3_t start, temp, vel;

	dist = (float)( radius / 100.0 );

	for ( i = 0; i < count; i++ )
	{
		temp[0] = RandomFloat( -100, 100 ) * dist;
		temp[1] = RandomFloat( -100, 100 ) * dist;
		temp[2] = RandomFloat( 0, 100 ) * dist;

		VectorAdd( temp, end, start );
		VectorScale( temp, (float)( -1.0 / life ), vel );

		R_TracerParticles( start, vel, life );
	}
}

void R_RocketTrail( vec_t* start, vec_t* end, int type )
{
	vec3_t vec, right, up;
	float len;
	int j;
	particle_t* p;
	float dec;
	double angle, radius;
	float s, c;

	static int tracercount;

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );

	if ( type == 7 )
	{
		dec = 1;
		VectorMatrix( vec, right, up );
	}
	else if ( type < 128 )
	{
		dec = 3;
	}
	else
	{
		dec = 1;
		type -= 128;
	}

	VectorScale( vec, dec, vec );

	while ( len > 0 )
	{
		len -= dec;

		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( vec3_origin, p->vel );

		p->die = (float)( cl.time + 2.0 );

		switch ( type )
		{
		case 0: // rocket trail
			p->ramp = (float)RandomLong( 0, 3 );
			p->color = (short)ramp3[(int)p->ramp];
			p->type = pt_fire;
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			break;

		case 1: // smoke smoke
			p->ramp = (float)RandomLong( 2, 5 );
			p->color = (short)ramp3[(int)p->ramp];
			p->type = pt_fire;
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			break;

		case 2: // blood
			p->type = pt_grav;
			p->color = (short)RandomLong( 67, 74 );
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			break;

		case 3:
		case 5: // tracer
			p->die = (float)( cl.time + 0.5 );
			p->type = pt_static;

			if ( type == 3 )
				p->color = (short)( 52 + ( tracercount & 4 ) * 2 );
			else
				p->color = (short)( 230 + ( tracercount & 4 ) * 2 );

			VectorCopy( start, p->org );
			tracercount++;

			if ( tracercount & 1 )
			{
				p->vel[0] = vec[1] * 30;
				p->vel[1] = -vec[0] * 30;
			}
			else
			{
				p->vel[0] = -vec[1] * 30;
				p->vel[1] = vec[0] * 30;
			}
			break;

		case 4: // slight blood
			p->type = pt_grav;
			p->color = (short)RandomLong( 67, 70 );
			for ( j = 0; j < 3; j++ )
				p->org[j] = start[j] + RandomFloat( -3, 3 );
			len -= 3;
			break;

		case 6: // voor trail
			p->ramp = (float)RandomLong( 0, 3 );
			p->color = (short)ramp3[(int)p->ramp];
			p->type = pt_fire;
			VectorCopy( start, p->org );
			break;

		case 7:
			angle = RandomLong( 0, 0xFFFF );
			radius = RandomLong( 8, 16 );
			s = (float)( sin( angle ) * radius );
			c = (float)( cos( angle ) * radius );

			for ( j = 0; j < 3; j++ )
#ifdef _MSC_VER
#pragma warning( suppress: 6001 )
#endif
				p->org[j] = start[j] + right[j] * s + up[j] * c;

			VectorSubtract( start, p->org, p->vel );
			VectorScale( p->vel, 2, p->vel );
			VectorMA( p->vel, RandomFloat( 96, 111 ), vec, p->vel );

			p->die = (float)( cl.time + 2.0 );
			p->ramp = (float)RandomLong( 0, 3 );
			p->color = (short)ramp3[(int)p->ramp];
			p->type = pt_explode2;
			break;

		default:
			break;
		}

		p->packedColor = 0;

		VectorAdd( start, vec, start );
	}
}

void R_FlickerParticles( vec_t* org )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < 15; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = org[j];
			p->vel[j] = RandomFloat( -32, 32 );
		}

		p->vel[2] = RandomFloat( 80, 143 );

		p->ramp = 0;
		p->color = 254;
		p->packedColor = 0;
		p->type = pt_blob2;
		p->die = (float)( cl.time + 2.0 );
	}
}

void R_SparkStreaks( vec_t* pos, int count, int velocityMin, int velocityMax )
{
	int i, j;
	particle_t* p;

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = gpActiveTracers;
		gpActiveTracers = p;

		p->die = (float)( cl.time + RandomFloat( 0.1f, 0.5f ) );
		p->color = 5;
		p->packedColor = 255;
		p->type = pt_grav;
		p->ramp = 0.5f;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = pos[j];
			p->vel[j] = RandomFloat( (float)velocityMin, (float)velocityMax );
		}
	}
}

void R_StreakSplash( vec_t* pos, vec_t* dir, int color, int count, float speed, int velocityMin, int velocityMax )
{
	int i, j;
	particle_t* p;
	vec3_t initialVelocity;

	VectorScale( dir, speed, initialVelocity );

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = gpActiveTracers;
		gpActiveTracers = p;

		p->die = (float)( cl.time + RandomFloat( 0.1f, 0.5f ) );
		p->color = (short)color;
		p->packedColor = 255;
		p->type = pt_grav;
		p->ramp = 1;

		for ( j = 0; j < 3; j++ )
		{
			p->org[j] = pos[j];
			p->vel[j] = initialVelocity[j] + RandomFloat( (float)velocityMin, (float)velocityMax );
		}
	}
}

void R_UserTracerParticle( vec_t* org, vec_t* vel, float life, int colorIndex, float length, unsigned char context,
	void ( *deathfunc )( particle_t* particle ) )
{
	particle_t* p;

	if ( colorIndex < 0 )
	{
		Con_Printf( "UserTracer with color < 0\n" );
		return;
	}

	if ( colorIndex > 12 )
	{
		Con_Printf( "UserTracer with color > %d\n", 12 );
		return;
	}

	if ( !free_particles )
		return;

	p = free_particles;
	free_particles = p->next;
	p->next = gpActiveTracers;
	gpActiveTracers = p;

	p->die = (float)( cl.time + life );
	p->color = (short)colorIndex;
	p->packedColor = 255;
	p->type = pt_static;
	p->ramp = length;

	VectorCopy( org, p->org );
	VectorCopy( vel, p->vel );

	p->deathfunc = deathfunc;
	p->context = context;
}

void R_LargeFunnel( vec_t* org, int reverse )
{
	int i, j;
	particle_t* p;
	float vel;
	vec3_t dir, dest;
	float flDist;

	for ( i = -256; i <= 256; i += 32 )
	{
		for ( j = -256; j <= 256; j += 32 )
		{
			if ( !free_particles )
				return;

			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			if ( reverse )
			{
				VectorCopy( org, p->org );

				dest[0] = org[0] + i;
				dest[1] = org[1] + j;
				dest[2] = org[2] + RandomFloat( 100, 800 );

				// send particle heading to dest at a random speed
				VectorSubtract( dest, p->org, dir );

				vel = dest[2] * 0.125f; // velocity based on how far particle starts from org
			}
			else
			{
				p->org[0] = org[0] + i;
				p->org[1] = org[1] + j;
				p->org[2] = org[2] + RandomFloat( 100, 800 );

				// send particle heading to org at a random speed
				VectorSubtract( org, p->org, dir );

				vel = p->org[2] * 0.125f; // velocity based on how far particle starts from org
			}

			p->color = 244;
			p->packedColor = 0;
			p->type = pt_static;

			flDist = VectorNormalize( dir ); // save the distance

			if ( vel < 64 )
				vel = 64;

			vel += RandomFloat( 64, 128 );
			VectorScale( dir, vel, p->vel );

			// die right when you get there
			p->die = (float)( cl.time + flDist / vel );
		}
	}
}

void R_ParticleBurst( vec_t* pos, int size, int color, float life )
{
	int i, j;
	particle_t* p;
	float vel;
	vec3_t dir, dest;

	for ( i = 0; i < 1024; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->color = (short)( color + RandomLong( 0, 10 ) );
		p->packedColor = 0;
		p->type = pt_static;

		VectorCopy( pos, p->org );

		for ( j = 0; j < 3; j++ )
			dest[j] = pos[j] + RandomFloat( (float)-size, (float)size );

		VectorSubtract( dest, p->org, dir );
		vel = VectorNormalize( dir ) / life;

		p->die = (float)( cl.time + life + RandomFloat( -0.5f, 0.5f ) );
		VectorScale( dir, vel, p->vel );
	}
}

void R_Blood( vec_t* org, vec_t* dir, int pcolor, int speed )
{
	vec3_t dirCopy;
	vec3_t orgCopy;
	float arc;
	int count;
	int count2;
	int j;
	particle_t* p;
	int pspeed;

	VectorNormalize( dir );

	pspeed = speed * 3;

	arc = 0.06f;
	for ( count = 0; count < speed / 2; count++ )
	{
		for ( j = 0; j < 3; j++ )
			orgCopy[j] = org[j] + RandomFloat( -3, 3 );

		for ( j = 0; j < 3; j++ )
			dirCopy[j] = dir[j] + RandomFloat( -arc, arc );

		for ( count2 = 0; count2 < 8; count2++ )
		{
			if ( !free_particles )
				return;

			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)( cl.time + 1.5 );
			p->color = (short)( pcolor + RandomLong( 0, 9 ) );
			p->packedColor = 0;
			p->type = pt_vox_grav;

			for ( j = 0; j < 3; j++ )
				p->org[j] = orgCopy[j] + RandomFloat( -1, 1 );

			VectorScale( dirCopy, (float)pspeed, p->vel );
		}

		pspeed -= speed;
	}
}

void R_BloodStream( vec_t* org, vec_t* dir, int pcolor, int speed )
{
	vec3_t dirCopy;
	float arc;
	int count;
	int count2;
	int j;
	particle_t* p;
	float num;
	int speedCopy = speed;

	VectorNormalize( dir );

	arc = 0.05f;
	for ( count = 0; count < 100; count++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + 2.0 );
		p->color = (short)( pcolor + RandomLong( 0, 9 ) );
		p->packedColor = 0;
		p->type = pt_vox_grav;

		VectorCopy( org, p->org );
		VectorCopy( dir, dirCopy );
		dirCopy[2] -= arc;

		arc = (float)( arc - 0.005 );

		VectorScale( dirCopy, (float)speedCopy, p->vel );
		speedCopy = (int)( speedCopy - 0.00001 );
	}

	arc = 0.075f;
	for ( count = 0; count < speed / 5; count++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + 3.0 );
		p->color = (short)( pcolor + RandomLong( 0, 9 ) );
		p->packedColor = 0;
		p->type = pt_vox_slowgrav;

		VectorCopy( org, p->org );
		VectorCopy( dir, dirCopy );
		dirCopy[2] -= arc;

		arc = (float)( arc - 0.005 );

		num = RandomFloat( 0, 1 );
		speedCopy = (int)( speed * num );
		num = (float)( num * 1.7 );

		VectorScale( dirCopy, num, dirCopy ); // randomize a bit
		VectorScale( dirCopy, (float)speedCopy, p->vel );

		for ( count2 = 0; count2 < 2; count2++ )
		{
			if ( !free_particles )
				return;

			p = free_particles;
			free_particles = p->next;
			p->next = active_particles;
			active_particles = p;

			p->die = (float)( cl.time + 3.0 );
			p->color = (short)( pcolor + RandomLong( 0, 9 ) );
			p->packedColor = 0;
			p->type = pt_vox_slowgrav;

			for ( j = 0; j < 3; j++ )
				p->org[j] = org[j] + RandomFloat( -1, 1 );

			VectorCopy( dir, dirCopy );
			dirCopy[2] -= arc;

			VectorScale( dirCopy, num, dirCopy ); // randomize a bit
			VectorScale( dirCopy, (float)speedCopy, p->vel );
		}
	}
}

void R_BulletImpactParticles( vec_t* pos )
{
	int i, j;
	particle_t* p;
	vec3_t dir;
	float dist, vel;
	int steps, count, color;

	VectorSubtract( pos, r_origin, dir );
	dist = Length( dir );
	if ( dist > 1000 )
		dist = 1000;

	steps = (int)( ( 1000 - dist ) / 100 );
	if ( steps == 0 )
		steps = 1;

	count = steps * 4;
	color = steps * 30 / 100;

	R_SparkStreaks( pos, 2, -200, 200 );

	for ( i = 0; i < count; i++ )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( pos, p->org );

		for ( j = 0; j < 3; j++ )
			p->vel[j] = RandomFloat( -1, 1 );

		vel = RandomFloat( 50, 100 );
		VectorScale( p->vel, vel, p->vel );

		p->type = pt_grav;
		p->color = (short)color;
		p->packedColor = 0;
		p->die = (float)( cl.time + 0.5 );
	}
}

void R_ShowLine( vec_t* start, vec_t* end )
{
	vec3_t vec;
	float len;
	particle_t* p;

	VectorSubtract( end, start, vec );
	len = VectorNormalize( vec );
	VectorScale( vec, 5, vec );

	while ( len > 0 )
	{
		len -= 5;

		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( vec3_origin, p->vel );

		p->color = 75;
		p->packedColor = 0;
		p->type = pt_static;
		p->die = (float)( cl.time + 30 );

		VectorCopy( start, p->org );
		VectorAdd( start, vec, start );
	}
}

void ParticleLine( float x1, float y1, float z1, float x2, float y2, float z2 )
{
	vec3_t start, vec;
	float len;
	particle_t* p;

	vec[0] = x2 - x1;
	vec[1] = y2 - y1;
	vec[2] = z2 - z1;
	len = VectorNormalize( vec );
	VectorScale( vec, 5, vec );

	if ( len <= 0 )
		return;

	len -= 5;

	start[0] = x1;
	start[1] = y1;
	start[2] = z1;

	for ( ;; )
	{
		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy( vec3_origin, p->vel );
		p->die = (float)( cl.time + 30 );
		VectorCopy( start, p->org );
		p->color = 75;
		p->packedColor = 0;
		p->type = pt_static;

		if ( len <= 0 )
			break;

		VectorAdd( start, vec, start );
		len -= 5;
	}
}

void ParticleBox( vec_t* mins, vec_t* maxs )
{
	vec3_t start, end;

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];
	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = mins[2];
	R_ShowLine( start, end );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = mins[2];
	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = mins[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];
	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = mins[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = mins[2];
	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = mins[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = maxs[2];
	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = maxs[2];
	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = maxs[2];
	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = maxs[2];
	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = mins[1];
	start[2] = mins[2];
	end[0] = mins[0];
	end[1] = mins[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = mins[0];
	start[1] = maxs[1];
	start[2] = mins[2];
	end[0] = mins[0];
	end[1] = maxs[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = maxs[0];
	start[1] = maxs[1];
	start[2] = mins[2];
	end[0] = maxs[0];
	end[1] = maxs[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );

	start[0] = maxs[0];
	start[1] = mins[1];
	start[2] = mins[2];
	end[0] = maxs[0];
	end[1] = mins[1];
	end[2] = maxs[2];
	R_ShowLine( start, end );
}

void R_GetPackedColor( short* packed, short )
{
	if ( !packed )
	{
		Con_Printf( "R_GetPackedColor called without packed!\n" );
		return;
	}

	*packed = 0;
}

short R_LookupColor( unsigned char r, unsigned char g, unsigned char b )
{
	int i;
	short best;
	float diff, bestdiff;
	float dr, dg, db;
	unsigned short* pb;

	bestdiff = 999999;
	best = -1;

	for ( i = 0; i < 256; i++ )
	{
		pb = &host_basepal[i * 4];

		dr = (float)( r - pb[2] );
		dg = (float)( g - pb[1] );
		db = (float)( b - pb[0] );

		diff = dg * 0.5f * dg + dr * 0.3f * dr + db * 0.2f * db;
		if ( bestdiff > diff )
		{
			bestdiff = diff;
			best = (short)i;
		}
	}

	return best;
}

/*
===============
R_EntityParticles
===============
*/
void R_EntityParticles( cl_entity_t* ent )
{
	int i, j;
	particle_t* p;
	float angle;
	float sp, sy, cp, cy;
	vec3_t forward;
	float dist;

	dist = 64;

	if ( !avelocities[0][0] )
	{
		for ( i = 0; i < NUMVERTEXNORMALS; i++ )
		{
			for ( j = 0; j < 3; j++ )
				avelocities[i][j] = RandomFloat( 0, 2.55f );
		}
	}

	for ( i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		angle = (float)( cl.time * avelocities[i][0] );
		sy = (float)sin( (double)angle );
		cy = (float)cos( (double)angle );
		angle = (float)( cl.time * avelocities[i][1] );
		sp = (float)sin( (double)angle );
		cp = (float)cos( (double)angle );

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		if ( !free_particles )
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = (float)( cl.time + 0.01 );
		p->color = 111;
		p->packedColor = 0;
		p->type = pt_explode;

		for ( j = 0; j < 3; j++ )
			p->org[j] = ent->origin[j] + r_avertexnormals[i][j] * dist + forward[j] * beamlength;
	}
}

void R_ReadPointFile_f()
{
	FILE* f;
	vec3_t org;
	int c;
	particle_t* p;
	char base[256];
	char name[MAX_PATH];

	if ( !cl.worldmodel )
		return;

	COM_StripExtension( cl.worldmodel->name, base );
	snprintf( name, sizeof( name ), "%s.pts", base );
	COM_FixSlashes( name );

	f = fopen( name, "rb" );
	if ( !f )
	{
		Con_Printf( "couldn't open %s\n", name );
		return;
	}

	Con_Printf( "Reading %s...\n", name );
	c = 0;
	while ( fscanf( f, "%f %f %f\n", &org[0], &org[1], &org[2] ) == 3 )
	{
		c++;

		if ( !free_particles )
		{
			Con_Printf( "Not enough free particles\n" );
			break;
		}

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->packedColor = 0;
		p->die = 99999;
		p->color = (short)( -c & 15 );
		p->type = pt_static;

		VectorCopy( org, p->org );
		VectorCopy( vec3_origin, p->vel );
	}

	fclose( f );
	Con_Printf( "%i points read\n", c );
}

void R_BeamKill( int deadEntity )
{
	BEAM* pbeam;

	for ( pbeam = gpActiveBeams; pbeam; pbeam = pbeam->next )
	{
		if ( ( pbeam->flags & FBEAM_STARTENTITY ) && pbeam->startEntity == deadEntity )
		{
			pbeam->flags &= ~( FBEAM_STARTENTITY | FBEAM_FOREVER );

			if ( pbeam->type != TE_BEAMFOLLOW )
				pbeam->die = (float)cl.time;
		}

		if ( ( pbeam->flags & FBEAM_ENDENTITY ) && pbeam->endEntity == deadEntity )
		{
			pbeam->flags &= ~( FBEAM_ENDENTITY | FBEAM_FOREVER );
			pbeam->die = (float)cl.time;
		}
	}
}

#endif
