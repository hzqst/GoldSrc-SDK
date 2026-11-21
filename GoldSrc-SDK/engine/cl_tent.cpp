// cl_tent.c -- client side temporary entities
#include "client.h"
#include "common.h"
#include "gl_model.h"
#include "conprint.h"
#include "enginecallbacks.h"
#include "../common/r_efx.h"
#include "../pm_shared/pm_movevars.h"
#include "../common/entity_types.h"
#include "sound.h"
#include "cvar.h"
#include "pm_defs.h"
#include "decals.h"
#include "tmessage.h"
#include "sys.h"

#include <cmath>
#include <cstdlib>

#define MAX_TEMP_ENTITIES 500
#define SHARD_VOLUME 12.0 // on shard ever n^3 units

extern unsigned short* host_basepal;
extern cvar_t violence_ablood;
extern vec3_t r_origin, vpn, vright, vup, modelorg;
extern cl_entity_t* currententity;
extern qboolean filterMode;
extern float filterColorRed, filterColorGreen, filterColorBlue;

int CL_AddVisibleEntity( cl_entity_t* entity );
void CL_TempEntPlaySound( TEMPENTITY* entity, float damping );
model_t* CL_TentModel( const char* name );
void AppendTEntity( cl_entity_t* entity );
void R_AttachTentToPlayer2( int client, model_t* model, float zoffset, float life );
qboolean CL_ShowTEBlood();
short R_LookupColor( byte red, byte green, byte blue );
void PM_ParticleLine( vec_t* start, vec_t* end, int color, float life, float verticalVelocity );
void PM_DrawBBox( vec_t* mins, vec_t* maxs, vec_t* origin, int color, float life );
void R_CustomDecalShoot( texture_t* texture, int player, int entity, int modelIndex, vec_t* position, int flags );
void R_ParticleLine( float* start, float* end, byte red, byte green, byte blue, float life );

TEMPENTITY gTempEnts[MAX_TEMP_ENTITIES];
TEMPENTITY* gpTempEntFree;
TEMPENTITY* gpTempEntActive;

cvar_t egon_amplitude = { "egon_amplitude", "0.0" };

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t* R_GetSpriteFrame( msprite_t* pSprite, int frame )
{
	if ( !pSprite )
	{
		Con_Printf( "Sprite:  no pSprite!!!\n" );
		return NULL;
	}

	if ( !pSprite->numframes )
	{
		Con_Printf( "Sprite:  pSprite has no frames!!!\n" );
		return NULL;
	}

	if ( frame < 0 || frame >= pSprite->numframes )
	{
		Con_DPrintf( "Sprite: no such frame %d\n", frame );
		frame = 0;
	}

	if ( pSprite->frames[frame].type != SPR_SINGLE )
		return NULL;

	return pSprite->frames[frame].frameptr;
}

void CL_TempEntInit( void )
{
	int i;

	Q_memset( gTempEnts, 0, sizeof( gTempEnts ) );

	for ( i = 0; i < MAX_TEMP_ENTITIES - 1; i++ )
		gTempEnts[i].next = &gTempEnts[i + 1];
	gTempEnts[MAX_TEMP_ENTITIES - 1].next = NULL;

	gpTempEntFree = gTempEnts;
	gpTempEntActive = NULL;
}

void CL_TempEntUpdate( void )
{
	if ( !cl.worldmodel )
		return;

	ClientDLL_TempEntUpdate( cl.time - cl.oldtime, cl.time, movevars.gravity, &gpTempEntFree, &gpTempEntActive,
		CL_AddVisibleEntity, CL_TempEntPlaySound );
}

sfx_t* cl_sfx_glass1;
sfx_t* cl_sfx_glass2;
sfx_t* cl_sfx_glass3;
sfx_t* cl_sfx_metal1;
sfx_t* cl_sfx_metal2;
sfx_t* cl_sfx_metal3;
sfx_t* cl_sfx_flesh1;
sfx_t* cl_sfx_flesh2;
sfx_t* cl_sfx_flesh3;
sfx_t* cl_sfx_flesh4;
sfx_t* cl_sfx_flesh5;
sfx_t* cl_sfx_flesh6;
sfx_t* cl_sfx_wood1;
sfx_t* cl_sfx_wood2;
sfx_t* cl_sfx_wood3;
sfx_t* cl_sfx_ric1;
sfx_t* cl_sfx_ric2;
sfx_t* cl_sfx_ric3;
sfx_t* cl_sfx_ric4;
sfx_t* cl_sfx_ric5;
sfx_t* cl_sfx_pl_shell1;
sfx_t* cl_sfx_pl_shell2;
sfx_t* cl_sfx_pl_shell3;
sfx_t* cl_sfx_concrete1;
sfx_t* cl_sfx_concrete2;
sfx_t* cl_sfx_concrete3;
sfx_t* cl_sfx_sshell1;
sfx_t* cl_sfx_sshell2;
sfx_t* cl_sfx_sshell3;

int CL_AddVisibleEntity( cl_entity_t* pEntity )
{
	vec3_t mins, maxs;

	if ( !pEntity->model )
		return 0;

	VectorAdd( pEntity->origin, pEntity->model->mins, mins );
	VectorAdd( pEntity->origin, pEntity->model->maxs, maxs );

	if ( !PVSNode( cl.worldmodel->nodes, mins, maxs ) )
		return 0;

	VectorCopy( pEntity->angles, pEntity->curstate.angles );
	VectorCopy( pEntity->angles, pEntity->latched.prevangles );

	ClientDLL_AddEntity( ET_TEMPENTITY, pEntity );
	return 1;
}

void CL_TempEntPlaySound( TEMPENTITY* pTemp, float damp )
{
	sfx_t* rgsfx[6] = {};
	int count = 3;
	qboolean isShell = FALSE;
	float fvol = 0.8f;
	int speed, pitch;
	double frac;

	switch ( pTemp->hitSound )
	{
	case BOUNCE_GLASS:
		rgsfx[0] = cl_sfx_glass1;
		rgsfx[1] = cl_sfx_glass2;
		rgsfx[2] = cl_sfx_glass3;
		break;
	case BOUNCE_METAL:
		rgsfx[0] = cl_sfx_metal1;
		rgsfx[1] = cl_sfx_metal2;
		rgsfx[2] = cl_sfx_metal3;
		break;
	case BOUNCE_FLESH:
		rgsfx[0] = cl_sfx_flesh1;
		rgsfx[1] = cl_sfx_flesh2;
		rgsfx[2] = cl_sfx_flesh3;
		rgsfx[3] = cl_sfx_flesh4;
		rgsfx[4] = cl_sfx_flesh5;
		rgsfx[5] = cl_sfx_flesh6;
		count = 6;
		break;
	case BOUNCE_WOOD:
		rgsfx[0] = cl_sfx_wood1;
		rgsfx[1] = cl_sfx_wood2;
		rgsfx[2] = cl_sfx_wood3;
		break;
	case BOUNCE_SHRAP:
		rgsfx[0] = cl_sfx_ric1;
		rgsfx[1] = cl_sfx_ric2;
		rgsfx[2] = cl_sfx_ric3;
		rgsfx[3] = cl_sfx_ric4;
		rgsfx[4] = cl_sfx_ric5;
		count = 5;
		break;
	case BOUNCE_SHELL:
		rgsfx[0] = cl_sfx_pl_shell1;
		rgsfx[1] = cl_sfx_pl_shell2;
		rgsfx[2] = cl_sfx_pl_shell3;
		isShell = TRUE;
		break;
	case BOUNCE_CONCRETE:
		rgsfx[0] = cl_sfx_concrete1;
		rgsfx[1] = cl_sfx_concrete2;
		rgsfx[2] = cl_sfx_concrete3;
		break;
	case BOUNCE_SHOTSHELL:
		rgsfx[0] = cl_sfx_sshell1;
		rgsfx[1] = cl_sfx_sshell2;
		rgsfx[2] = cl_sfx_sshell3;
		fvol = 0.5f;
		isShell = TRUE;
		break;
	default:
		return;
	}

	speed = abs( (int)pTemp->entity.baseline.origin[2] );

	if ( isShell )
	{
		if ( speed < 200 && RandomLong( 0, 3 ) )
			return;
	}
	else if ( RandomLong( 0, 5 ) )
		return;

	if ( !( damp > 0 ) )
		return;

	frac = (float)speed / ( isShell ? 350.0 : 450.0 );
	if ( frac > 1.0 )
		frac = 1.0;
	fvol = (float)( frac * fvol );

	pitch = PITCH_NORM;
	if ( !RandomLong( 0, 3 ) && !isShell )
		pitch = RandomLong( 90, 124 );

	S_StartDynamicSound( -1, CHAN_AUTO, rgsfx[RandomLong( 0, count - 1 )], pTemp->entity.origin, fvol, 1.0f, 0, pitch );
}

cvar_t tracerSpeed = { "tracerspeed", "6000" };
cvar_t tracerOffset = { "traceroffset", "30" };
cvar_t tracerLength = { "tracerlength", "0.8" };
cvar_t tracerRed = { "tracerred", "0.8" };
cvar_t tracerGreen = { "tracergreen", "0.8" };
cvar_t tracerBlue = { "tracerblue", "0.4" };
cvar_t tracerAlpha = { "traceralpha", "0.5" };

sfx_t* cl_sfx_r_exp1;
sfx_t* cl_sfx_r_exp2;
sfx_t* cl_sfx_r_exp3;
sfx_t* cl_sfx_geiger1;
sfx_t* cl_sfx_geiger2;
sfx_t* cl_sfx_geiger3;
sfx_t* cl_sfx_geiger4;
sfx_t* cl_sfx_geiger5;
sfx_t* cl_sfx_geiger6;
model_t* cl_sprite_dot;
model_t* cl_sprite_lightning;
model_t* cl_sprite_white;
model_t* cl_sprite_glow;
model_t* cl_sprite_ricochet;
model_t* cl_sprite_shell;
model_t* cl_sprite_muzzleflash[3];

void CL_InitTEnts( void )
{
	Cvar_RegisterVariable( &tracerSpeed );
	Cvar_RegisterVariable( &tracerOffset );
	Cvar_RegisterVariable( &tracerLength );
	Cvar_RegisterVariable( &tracerRed );
	Cvar_RegisterVariable( &tracerGreen );
	Cvar_RegisterVariable( &tracerBlue );
	Cvar_RegisterVariable( &tracerAlpha );

	cl_sfx_ric1 = S_PrecacheSound( "weapons/ric1.wav" );
	cl_sfx_ric2 = S_PrecacheSound( "weapons/ric2.wav" );
	cl_sfx_ric3 = S_PrecacheSound( "weapons/ric3.wav" );
	cl_sfx_ric4 = S_PrecacheSound( "weapons/ric4.wav" );
	cl_sfx_ric5 = S_PrecacheSound( "weapons/ric5.wav" );
	cl_sfx_r_exp1 = S_PrecacheSound( "weapons/explode3.wav" );
	cl_sfx_r_exp2 = S_PrecacheSound( "weapons/explode4.wav" );
	cl_sfx_r_exp3 = S_PrecacheSound( "weapons/explode5.wav" );
	cl_sfx_pl_shell1 = S_PrecacheSound( "player/pl_shell1.wav" );
	cl_sfx_pl_shell2 = S_PrecacheSound( "player/pl_shell2.wav" );
	cl_sfx_pl_shell3 = S_PrecacheSound( "player/pl_shell3.wav" );
	cl_sfx_sshell1 = S_PrecacheSound( "weapons/sshell1.wav" );
	cl_sfx_sshell2 = S_PrecacheSound( "weapons/sshell2.wav" );
	cl_sfx_sshell3 = S_PrecacheSound( "weapons/sshell3.wav" );
	cl_sfx_wood1 = S_PrecacheSound( "debris/wood1.wav" );
	cl_sfx_wood2 = S_PrecacheSound( "debris/wood2.wav" );
	cl_sfx_wood3 = S_PrecacheSound( "debris/wood3.wav" );
	cl_sfx_metal1 = S_PrecacheSound( "debris/metal1.wav" );
	cl_sfx_metal2 = S_PrecacheSound( "debris/metal2.wav" );
	cl_sfx_metal3 = S_PrecacheSound( "debris/metal3.wav" );
	cl_sfx_glass1 = S_PrecacheSound( "debris/glass1.wav" );
	cl_sfx_glass2 = S_PrecacheSound( "debris/glass2.wav" );
	cl_sfx_glass3 = S_PrecacheSound( "debris/glass3.wav" );
	cl_sfx_concrete1 = S_PrecacheSound( "debris/concrete1.wav" );
	cl_sfx_concrete2 = S_PrecacheSound( "debris/concrete2.wav" );
	cl_sfx_concrete3 = S_PrecacheSound( "debris/concrete3.wav" );
	cl_sfx_flesh1 = S_PrecacheSound( "debris/flesh1.wav" );
	cl_sfx_flesh2 = S_PrecacheSound( "debris/flesh2.wav" );
	cl_sfx_flesh3 = S_PrecacheSound( "debris/flesh3.wav" );
	cl_sfx_flesh4 = S_PrecacheSound( "debris/flesh5.wav" );
	cl_sfx_flesh5 = S_PrecacheSound( "debris/flesh6.wav" );
	cl_sfx_flesh6 = S_PrecacheSound( "debris/flesh7.wav" );
	cl_sfx_geiger1 = S_PrecacheSound( "player/geiger1.wav" );
	cl_sfx_geiger2 = S_PrecacheSound( "player/geiger2.wav" );
	cl_sfx_geiger3 = S_PrecacheSound( "player/geiger3.wav" );
	cl_sfx_geiger4 = S_PrecacheSound( "player/geiger4.wav" );
	cl_sfx_geiger5 = S_PrecacheSound( "player/geiger5.wav" );
	cl_sfx_geiger6 = S_PrecacheSound( "player/geiger6.wav" );

	cl_sprite_dot = CL_TentModel( "sprites/dot.spr" );
	cl_sprite_lightning = CL_TentModel( "sprites/lgtning.spr" );
	cl_sprite_white = CL_TentModel( "sprites/white.spr" );
	cl_sprite_glow = CL_TentModel( "sprites/animglow01.spr" );
	cl_sprite_muzzleflash[0] = CL_TentModel( "sprites/muzzleflash1.spr" );
	cl_sprite_muzzleflash[1] = CL_TentModel( "sprites/muzzleflash2.spr" );
	cl_sprite_muzzleflash[2] = CL_TentModel( "sprites/muzzleflash3.spr" );
	cl_sprite_ricochet = CL_TentModel( "sprites/richo1.spr" );
	cl_sprite_shell = CL_TentModel( "sprites/shellchrome.spr" );

	CL_TempEntInit();
}

void CL_TempEntPrepare( TEMPENTITY* pTemp, model_t* model )
{
	Q_memset( &pTemp->entity, 0, sizeof( pTemp->entity ) );

	pTemp->flags = FTENT_NONE;
	pTemp->entity.curstate.colormap = 0;
	pTemp->die = (float)( cl.time + 0.75 );
	pTemp->entity.model = model;
	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->fadeSpeed = 0.5f;
	pTemp->hitSound = 0;
	pTemp->clientIndex = -1;
	pTemp->bounceFactor = 1.0f;
	pTemp->hitcallback = NULL;
	pTemp->callback = NULL;
}

TEMPENTITY* CL_TempEntAlloc( vec_t* org, model_t* model )
{
	TEMPENTITY* pTemp;

	if ( !gpTempEntFree )
	{
		Con_DPrintf( "Overflow %d temporary ents!\n", MAX_TEMP_ENTITIES );
		return NULL;
	}

	if ( !model )
	{
		Con_DPrintf( "efx.CL_TempEntAlloc: No model\n" );
		return NULL;
	}

	pTemp = gpTempEntFree;
	gpTempEntFree = pTemp->next;

	CL_TempEntPrepare( pTemp, model );

	pTemp->priority = TENTPRIORITY_LOW;
	VectorCopy( org, pTemp->entity.origin );

	pTemp->next = gpTempEntActive;
	gpTempEntActive = pTemp;

	return pTemp;
}

TEMPENTITY* CL_TempEntAllocNoModel( vec_t* org )
{
	TEMPENTITY* pTemp;

	if ( !gpTempEntFree )
	{
		Con_DPrintf( "Overflow %d temporary ents!\n", MAX_TEMP_ENTITIES );
		return NULL;
	}

	pTemp = gpTempEntFree;
	gpTempEntFree = pTemp->next;

	CL_TempEntPrepare( pTemp, NULL );

	pTemp->flags |= FTENT_NOMODEL;
	pTemp->priority = TENTPRIORITY_LOW;
	VectorCopy( org, pTemp->entity.origin );

	pTemp->next = gpTempEntActive;
	gpTempEntActive = pTemp;

	return pTemp;
}

TEMPENTITY* CL_TempEntAllocHigh( vec_t* org, model_t* model )
{
	TEMPENTITY* pTemp;

	if ( !model )
	{
		Con_DPrintf( "temporary ent model invalid\n" );
		return NULL;
	}

	pTemp = gpTempEntFree;
	if ( pTemp )
	{
		gpTempEntFree = pTemp->next;
		pTemp->next = gpTempEntActive;
		gpTempEntActive = pTemp;
	}
	else
	{
		// no temporary ents free, so find the first active low-priority temp ent
		// and overwrite it.
		for ( pTemp = gpTempEntActive; pTemp; pTemp = pTemp->next )
		{
			if ( pTemp->priority == TENTPRIORITY_LOW )
				break;
		}

		if ( !pTemp )
		{
			Con_DPrintf( "Couldn't alloc a high priority TENT!\n" );
			return NULL;
		}
	}

	CL_TempEntPrepare( pTemp, model );

	pTemp->priority = TENTPRIORITY_HIGH;
	VectorCopy( org, pTemp->entity.origin );

	return pTemp;
}

TEMPENTITY* R_DefaultSprite( float* pos, int spriteIndex, float framerate )
{
	TEMPENTITY* pTemp;
	int frameCount;
	model_t* pSprite;

	// don't spawn while paused
	if ( cl.time == cl.oldtime )
		return NULL;

	pSprite = CL_GetModelByIndex( spriteIndex );
	if ( !spriteIndex || !pSprite || pSprite->type != mod_sprite )
	{
		Con_DPrintf( "No Sprite %d!\n", spriteIndex );
		return NULL;
	}

	frameCount = ModelFrameCount( pSprite );

	pTemp = efx.CL_TempEntAlloc( pos, pSprite );
	if ( !pTemp )
		return NULL;

	pTemp->flags |= FTENT_SPRANIMATE;
	pTemp->frameMax = (float)frameCount;

	if ( framerate == 0 )
		framerate = 10;

	pTemp->entity.curstate.framerate = framerate;
	pTemp->entity.curstate.frame = 0;
	pTemp->die = (float)( frameCount / framerate + cl.time );
	VectorCopy( pos, pTemp->entity.origin );

	return pTemp;
}

TEMPENTITY* R_TempSprite( float* pos, float* dir, float scale, int modelIndex, int rendermode, int renderfx, float a,
	float life, int flags )
{
	TEMPENTITY* pTemp;
	model_t* model;
	int frameCount;
	int renderamt;

	if ( !modelIndex )
		return NULL;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return NULL;
	}

	frameCount = ModelFrameCount( model );

	pTemp = efx.CL_TempEntAlloc( pos, model );
	if ( !pTemp )
		return NULL;

	pTemp->entity.curstate.rendermode = rendermode;
	pTemp->entity.curstate.renderfx = renderfx;
	pTemp->entity.curstate.framerate = 10;
	pTemp->frameMax = (float)frameCount;
	pTemp->entity.curstate.rendercolor.r = 255;
	pTemp->entity.curstate.rendercolor.g = 255;
	pTemp->entity.curstate.rendercolor.b = 255;

	renderamt = (int)( a * 255.0f );
	pTemp->entity.curstate.renderamt = renderamt;
	pTemp->entity.baseline.renderamt = renderamt;

	pTemp->flags |= flags;
	pTemp->entity.curstate.scale = scale;

	VectorCopy( dir, pTemp->entity.baseline.origin );
	VectorCopy( pos, pTemp->entity.origin );

	if ( life == 0 )
		pTemp->die = (float)( frameCount * 0.1 + cl.time + 1.0 );
	else
		pTemp->die = (float)( life + cl.time );

	pTemp->entity.curstate.frame = 0;
	return pTemp;
}

void R_RicochetSprite( float* pos, model_t* pmodel, float, float scale )
{
	TEMPENTITY* pTemp;

	pTemp = efx.CL_TempEntAlloc( pos, pmodel );
	if ( !pTemp )
		return;

	pTemp->entity.curstate.scale = scale;
	pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.curstate.renderamt = 200;
	pTemp->entity.baseline.renderamt = 200;
	pTemp->flags = FTENT_FADEOUT;
	VectorClear( pTemp->entity.baseline.origin );
	VectorCopy( pos, pTemp->entity.origin );
	pTemp->fadeSpeed = 8;
	pTemp->entity.curstate.frame = 0;
	pTemp->die = (float)cl.time;
	pTemp->entity.angles[ROLL] = (float)( 45 * RandomLong( 0, 7 ) );
}

void R_Sprite_Spray( vec_t* pos, vec_t* dir, int modelIndex, int count, int speed, int iRand )
{
	TEMPENTITY* pTemp;
	model_t* pModel;
	float noise;
	float znoise;
	int frameCount;
	int i;

	noise = (float)iRand / 100.0f;

	// more vertical displacement
	znoise = (float)( noise * 1.5 );
	if ( znoise > 1 )
		znoise = 1;

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	frameCount = ModelFrameCount( pModel );

	for ( i = 0; i < count; i++ )
	{
		pTemp = efx.CL_TempEntAlloc( pos, pModel );
		if ( !pTemp )
			return;

		pTemp->flags |= FTENT_FADEOUT | FTENT_SLOWGRAVITY;
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = 255;
		pTemp->entity.baseline.renderamt = 255;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.framerate = 0.5f;
		pTemp->fadeSpeed = 2;

		// make the spittle fly the direction indicated, but mix in some noise.
		pTemp->entity.baseline.origin[0] = RandomFloat( -noise, noise ) + dir[0];
		pTemp->entity.baseline.origin[1] = RandomFloat( -noise, noise ) + dir[1];
		pTemp->entity.baseline.origin[2] = RandomFloat( 0, znoise ) + dir[2];
		VectorScale( pTemp->entity.baseline.origin, RandomFloat( (float)( speed * 0.8 ), (float)( speed * 1.2 ) ),
			pTemp->entity.baseline.origin );

		VectorCopy( pos, pTemp->entity.origin );

		pTemp->die = (float)( cl.time + 0.35 );
		pTemp->entity.curstate.frame = (float)RandomLong( 0, frameCount - 1 );
	}
}

void R_Sprite_Trail( int, vec_t* start, vec_t* end, int modelIndex, int count, float life, float size,
	float amplitude, int renderamt, float speed )
{
	TEMPENTITY* pTemp;
	model_t* model;
	int i, j, frameCount;
	vec3_t delta, pos, dir;
	float frac;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	frameCount = ModelFrameCount( model );

	VectorSubtract( end, start, delta );
	VectorCopy( delta, dir );
	VectorNormalize( dir );

	amplitude /= 256.0f;

	for ( i = 0; i < count; i++ )
	{
		frac = 0;
		if ( i != 0 )
			frac = (float)( i / ( (double)count - 1.0 ) );
		VectorMA( start, frac, delta, pos );

		pTemp = efx.CL_TempEntAlloc( pos, model );
		if ( !pTemp )
			return;

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_SPRCYCLE | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		VectorScale( dir, speed, pTemp->entity.baseline.origin );
		for ( j = 0; j < 3; j++ )
			pTemp->entity.baseline.origin[j] += (float)RandomLong( -127, 128 ) * amplitude;

		pTemp->entity.curstate.rendermode = kRenderGlow;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.renderamt = renderamt;
		pTemp->entity.curstate.scale = size;
		pTemp->die = (float)( life + cl.time + RandomFloat( 0, 4 ) );
		pTemp->entity.curstate.frame = (float)RandomLong( 0, frameCount - 1 );
		pTemp->entity.baseline.renderamt = renderamt;
		pTemp->entity.curstate.rendercolor.r = 255;
		pTemp->entity.curstate.rendercolor.g = 255;
		pTemp->entity.curstate.rendercolor.b = 255;
		pTemp->frameMax = (float)frameCount;
	}
}

void R_Sprite_Smoke( TEMPENTITY* pTemp, float scale )
{
	byte color;

	if ( !pTemp )
		return;

	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[2] = 30;

	color = (byte)RandomLong( 20, 35 );
	pTemp->entity.curstate.rendercolor.r = color;
	pTemp->entity.curstate.rendercolor.g = color;
	pTemp->entity.curstate.rendercolor.b = color;

	pTemp->entity.origin[2] += 20;
	pTemp->entity.curstate.scale = scale;
}

//-----------------------------------------------------------------------------
// Purpose: Create some simple physically simulated models
//-----------------------------------------------------------------------------
TEMPENTITY* R_TempModel( float* pos, float* dir, float* angles, float life, int modelIndex, int soundtype )
{
	TEMPENTITY* pTemp;
	model_t* model;
	int frameCount;

	if ( !modelIndex )
		return NULL;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return NULL;
	}

	frameCount = ModelFrameCount( model );

	pTemp = efx.CL_TempEntAlloc( pos, model );
	if ( !pTemp )
		return NULL;

	VectorCopy( angles, pTemp->entity.angles );

	pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_GRAVITY;
	pTemp->frameMax = (float)frameCount;

	switch ( soundtype )
	{
	case TE_BOUNCE_NULL:
		pTemp->hitSound = 0;
		break;
	case TE_BOUNCE_SHELL:
	case TE_BOUNCE_SHOTSHELL:
		pTemp->hitSound = soundtype == TE_BOUNCE_SHELL ? BOUNCE_SHELL : BOUNCE_SHOTSHELL;
		pTemp->entity.baseline.angles[0] = RandomFloat( -512, 511 );
		pTemp->entity.baseline.angles[1] = RandomFloat( -256, 255 );
		pTemp->entity.baseline.angles[2] = RandomFloat( -256, 255 );
		pTemp->flags |= FTENT_ROTATE;
		if ( soundtype == TE_BOUNCE_SHOTSHELL )
			pTemp->flags |= FTENT_SLOWGRAVITY;
		break;
	}

	VectorCopy( dir, pTemp->entity.baseline.origin );
	pTemp->die = (float)( life + cl.time );

	if ( model->type == mod_sprite )
		pTemp->entity.curstate.frame = (float)RandomLong( 0, frameCount - 1 );
	else
		pTemp->entity.curstate.body = RandomLong( 0, frameCount - 1 );

	return pTemp;
}

void R_BreakModel( float* pos, float* size, float* dir, float random, float life, int count, int modelIndex,
	char flags )
{
	int i, j, frameCount;
	TEMPENTITY* pTemp;
	model_t* pModel;
	int type;
	float volume;
	vec3_t spot;

	if ( !modelIndex )
		return;

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
		return;

	type = flags & BREAK_TYPEMASK;
	frameCount = ModelFrameCount( pModel );

	if ( count == 0 )
	{
		// assume surface (not volume)
		volume = size[0] * size[1] + size[1] * size[2] + size[2] * size[0];
		count = (int)( volume / ( 3 * SHARD_VOLUME * SHARD_VOLUME ) );
	}

	// Limit to 100 pieces
	if ( count > 100 )
		count = 100;

	for ( i = 0; i < count; i++ )
	{
		// fill up the box with stuff
		for ( j = 0; j < 3; j++ )
			spot[j] = RandomFloat( -0.5f, 0.5f ) * size[j] + pos[j];

		pTemp = efx.CL_TempEntAlloc( spot, pModel );
		if ( !pTemp )
			return;

		// keep track of break_type, so we know how to play sound on collision
		pTemp->hitSound = type;

		if ( pModel->type == mod_sprite )
			pTemp->entity.curstate.frame = (float)RandomLong( 0, frameCount - 1 );
		else if ( pModel->type == mod_studio )
			pTemp->entity.curstate.body = RandomLong( 0, frameCount - 1 );

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY | FTENT_FADEOUT;

		if ( RandomLong( 0, 255 ) < 200 )
		{
			pTemp->flags |= FTENT_ROTATE;
			for ( j = 0; j < 3; j++ )
				pTemp->entity.baseline.angles[j] = RandomFloat( -256, 255 );
		}

		if ( RandomLong( 0, 255 ) < 100 && ( flags & BREAK_SMOKE ) )
			pTemp->flags |= FTENT_SMOKETRAIL;

		if ( type == BREAK_GLASS || ( flags & BREAK_TRANS ) )
		{
			pTemp->entity.curstate.renderamt = 128;
			pTemp->entity.curstate.rendermode = kRenderTransTexture;
			pTemp->entity.baseline.renderamt = 128;
		}
		else
		{
			pTemp->entity.curstate.rendermode = kRenderNormal;
			pTemp->entity.baseline.renderamt = 255; // Set this for fadeout
		}

		pTemp->entity.baseline.origin[0] = RandomFloat( -random, random ) + dir[0];
		pTemp->entity.baseline.origin[1] = RandomFloat( -random, random ) + dir[1];
		pTemp->entity.baseline.origin[2] = RandomFloat( 0, random ) + dir[2];

		pTemp->die = (float)( life + cl.time + RandomFloat( 0, 1 ) ); // Add an extra 0-1 secs of life
	}
}

void R_Bubbles( vec_t* mins, vec_t* maxs, float height, int modelIndex, int count, float speed )
{
	TEMPENTITY* pTemp;
	model_t* model;
	int i, j, frameCount;
	double angle;
	float zspeed, life;
	vec3_t origin;

	if ( !modelIndex )
		return;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	frameCount = ModelFrameCount( model );

	for ( i = 0; i < count; i++ )
	{
		for ( j = 0; j < 3; j++ )
			origin[j] = (float)RandomLong( (int)mins[j], (int)maxs[j] );

		pTemp = efx.CL_TempEntAlloc( origin, model );
		if ( !pTemp )
			return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = RandomLong( -3, 3 );
		pTemp->entity.baseline.origin[0] = (float)( cos( angle ) * speed );
		pTemp->entity.baseline.origin[1] = (float)( sin( angle ) * speed );

		zspeed = (float)RandomLong( 80, 140 );
		pTemp->entity.baseline.origin[2] = zspeed;

		life = ( height - ( origin[2] - mins[2] ) ) / zspeed;
		pTemp->die = (float)( life + cl.time - 0.1 );
		pTemp->entity.curstate.frame = (float)RandomLong( 0, frameCount - 1 );

		// Set sprite scale
		pTemp->entity.curstate.scale = (float)( 1.0 / RandomFloat( 2, 5 ) );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = 255;
	}
}

void R_BubbleTrail( vec_t* start, vec_t* end, float height, int modelIndex, int count, float speed )
{
	TEMPENTITY* pTemp;
	model_t* model;
	int i, j, frameCount;
	double angle;
	float dist, zspeed, life;
	vec3_t origin;

	if ( !modelIndex )
		return;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	frameCount = ModelFrameCount( model );

	for ( i = 0; i < count; i++ )
	{
		dist = RandomFloat( 0, 1 );
		for ( j = 0; j < 3; j++ )
			origin[j] = ( end[j] - start[j] ) * dist + start[j];

		pTemp = efx.CL_TempEntAlloc( origin, model );
		if ( !pTemp )
			return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = RandomLong( -3, 3 );
		pTemp->entity.baseline.origin[0] = (float)( cos( angle ) * speed );
		pTemp->entity.baseline.origin[1] = (float)( sin( angle ) * speed );

		zspeed = (float)RandomLong( 80, 140 );
		pTemp->entity.baseline.origin[2] = zspeed;

		life = ( height - ( origin[2] - start[2] ) ) / zspeed;
		pTemp->die = (float)( life + cl.time - 0.1 );
		pTemp->entity.curstate.frame = (float)RandomLong( 0, frameCount - 1 );

		// Set sprite scale
		pTemp->entity.curstate.scale = (float)( 1.0 / RandomFloat( 2, 5 ) );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = 255;
	}
}

void R_Projectile( vec_t* origin, vec_t* velocity, int modelIndex, int life, int owner,
	void ( *hitcallback )( tempent_s*, pmtrace_s* ) )
{
	TEMPENTITY* pTemp;
	model_t* pmodel;
	int frameCount;

	pmodel = CL_GetModelByIndex( modelIndex );
	if ( !pmodel )
		return;

	pTemp = efx.CL_TempEntAllocHigh( origin, pmodel );
	if ( !pTemp )
	{
		Con_Printf( "Couldn't allocate temp ent in R_Projectile!\n" );
		return;
	}

	if ( owner < 0 || owner > cl.maxclients )
	{
		Con_Printf( "Bad ent in R_Projectile!\n" );
		return;
	}

	pTemp->flags |= FTENT_COLLIDEALL | FTENT_PERSIST | FTENT_COLLIDEKILL;
	VectorCopy( velocity, pTemp->entity.baseline.origin );
	pTemp->clientIndex = (short)owner;

	if ( pTemp->entity.model->type == mod_sprite )
	{
		frameCount = ModelFrameCount( pmodel );

		pTemp->flags |= FTENT_SPRANIMATE;
		pTemp->frameMax = (float)frameCount;

		if ( frameCount >= 10 )
		{
			pTemp->entity.curstate.framerate = (float)( frameCount / life );
		}
		else
		{
			pTemp->flags |= FTENT_SPRANIMATELOOP;
			pTemp->entity.curstate.framerate = 10;
		}
	}
	else
	{
		pTemp->frameMax = 0;
		VectorNormalize( velocity );
		VectorAngles( velocity, pTemp->entity.angles );
	}

	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.baseline.renderamt = 255;
	pTemp->entity.curstate.frame = 0;
	pTemp->entity.curstate.body = 0;
	pTemp->entity.curstate.scale = 1.0f;
	pTemp->entity.curstate.rendercolor.r = 255;
	pTemp->entity.curstate.rendercolor.g = 255;
	pTemp->entity.curstate.rendercolor.b = 255;
	pTemp->hitcallback = hitcallback;
	pTemp->die = (float)( life + cl.time );
}

void R_AttachTentToPlayer( int client, int modelIndex, float zoffset, float life )
{
	model_t* pModel;

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	R_AttachTentToPlayer2( client, pModel, zoffset, life );
}

void R_KillAttachedTents( int client )
{
	TEMPENTITY* pTemp;

	if ( client < 0 || client > cl.maxclients )
	{
		Con_Printf( "Bad client in R_KillAttachedTents()!\n" );
		return;
	}

	for ( pTemp = gpTempEntActive; pTemp; pTemp = pTemp->next )
	{
		if ( ( pTemp->flags & FTENT_PLYRATTACHMENT ) && pTemp->clientIndex == client )
			pTemp->die = (float)cl.time;
	}
}

void R_FizzEffect( cl_entity_t* pent, int modelIndex, int density )
{
	TEMPENTITY* pTemp;
	model_t* model;
	int i, width, depth, count, frameCount;
	float maxHeight, speed, xspeed, yspeed, zspeed;
	double angle;
	vec3_t origin;

	if ( !pent->model || !modelIndex )
		return;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	count = density + 1;

	maxHeight = pent->model->maxs[2] - pent->model->mins[2];
	width = (int)( pent->model->maxs[0] - pent->model->mins[0] );
	depth = (int)( pent->model->maxs[1] - pent->model->mins[1] );

	speed = (float)( pent->curstate.rendercolor.r * 256.0 + pent->curstate.rendercolor.g );
	if ( pent->curstate.rendercolor.b )
		speed = -speed;

	angle = pent->angles[YAW] * M_PI / 180;
	xspeed = (float)( cos( angle ) * speed );
	yspeed = (float)( sin( angle ) * speed );

	frameCount = ModelFrameCount( model );

	for ( i = 0; i < count; i++ )
	{
		origin[0] = (float)RandomLong( 0, width - 1 ) + pent->model->mins[0];
		origin[1] = (float)RandomLong( 0, depth - 1 ) + pent->model->mins[1];
		origin[2] = pent->model->mins[2];

		pTemp = efx.CL_TempEntAlloc( origin, model );
		if ( !pTemp )
			return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];

		zspeed = (float)RandomLong( 80, 140 );
		pTemp->entity.baseline.origin[0] = xspeed;
		pTemp->entity.baseline.origin[1] = yspeed;
		pTemp->entity.baseline.origin[2] = zspeed;
		pTemp->die = (float)( maxHeight / zspeed + cl.time - 0.1 );
		pTemp->entity.curstate.frame = (float)RandomLong( 0, frameCount - 1 );

		// Set sprite scale
		pTemp->entity.curstate.scale = (float)( 1.0 / RandomFloat( 2, 5 ) );
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = 255;
	}
}

void R_FunnelSprite( float* org, int modelIndex, int reverse )
{
	TEMPENTITY* pTemp;
	model_t* model;
	int i, j, frameCount;
	vec3_t dir, dest;
	float dist, vel;

	if ( !modelIndex )
	{
		Con_Printf( "No modelindex for funnel!!\n" );
		return;
	}

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	frameCount = ModelFrameCount( model );

	for ( i = -256; i <= 256; i += 64 )
	{
		for ( j = -256; j <= 256; j += 64 )
		{
			pTemp = efx.CL_TempEntAlloc( org, model );
			if ( !pTemp )
				return;

			if ( reverse )
			{
				VectorCopy( org, pTemp->entity.origin );

				dest[0] = (float)i + org[0];
				dest[1] = (float)j + org[1];
				dest[2] = RandomFloat( 100, 800 ) + org[2];

				// send particle heading to org at a random speed
				VectorSubtract( dest, pTemp->entity.origin, dir );

				// velocity based on how far particle has to travel away from org
				vel = (float)( dest[2] * 0.125 );
			}
			else
			{
				pTemp->entity.origin[0] = (float)i + org[0];
				pTemp->entity.origin[1] = (float)j + org[1];
				pTemp->entity.origin[2] = RandomFloat( 100, 800 ) + org[2];

				// send particle heading to org at a random speed
				VectorSubtract( org, pTemp->entity.origin, dir );

				// velocity based on how far particle starts from org
				vel = (float)( pTemp->entity.origin[2] * 0.125 );
			}

			pTemp->entity.curstate.rendermode = kRenderGlow;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
			pTemp->entity.curstate.renderamt = 200;
			pTemp->frameMax = (float)frameCount;
			pTemp->entity.baseline.renderamt = 200;
			pTemp->entity.curstate.framerate = RandomFloat( 0.1f, 0.4f );
			pTemp->flags = FTENT_FADEOUT | FTENT_ROTATE;

			dist = VectorNormalize( dir );

			if ( vel < 64 )
				vel = 64;
			vel += RandomFloat( 64, 128 );

			VectorScale( dir, vel, pTemp->entity.baseline.origin );
			pTemp->fadeSpeed = 2;
			pTemp->die = (float)( dist / vel - 0.5 + cl.time );
		}
	}
}

void R_MuzzleFlash( float* pos1, int type )
{
	TEMPENTITY* pTemp;
	int index, frameCount;
	float scale;

	index = ( type % 10 ) % 3;
	scale = (float)( ( type / 10 ) * 0.1 );
	if ( scale == 0 )
		scale = 0.5f;

	frameCount = ModelFrameCount( cl_sprite_muzzleflash[index] );

	pTemp = efx.CL_TempEntAlloc( pos1, cl_sprite_muzzleflash[index] );
	if ( !pTemp )
		return;

	pTemp->entity.curstate.scale = scale;
	pTemp->entity.curstate.rendermode = kRenderTransAdd;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.curstate.rendercolor.r = 255;
	pTemp->entity.curstate.rendercolor.g = 255;
	pTemp->entity.curstate.rendercolor.b = 255;
	VectorCopy( pos1, pTemp->entity.origin );
	pTemp->die = (float)( cl.time + 0.01 );
	pTemp->entity.curstate.frame = (float)RandomLong( 0, frameCount - 1 );
	pTemp->frameMax = (float)frameCount;
	VectorClear( pTemp->entity.angles );

	if ( index == 0 )
		pTemp->entity.angles[ROLL] = (float)RandomLong( 0, 20 ); // Rifle flash
	else
		pTemp->entity.angles[ROLL] = (float)RandomLong( 0, 359 );

	AppendTEntity( &pTemp->entity );
}

void R_Sprite_Explode( TEMPENTITY* pTemp, float scale, int flags )
{
	if ( !pTemp )
		return;

	if ( flags & TE_EXPLFLAG_NOADDITIVE )
	{
		// solid sprite
		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.curstate.renderamt = 255;
	}
	else
	{
		pTemp->entity.curstate.rendermode = kRenderTransAdd;
		pTemp->entity.curstate.renderamt = 180;
	}

	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin[2] = 8;
	pTemp->entity.curstate.rendercolor.r = 0;
	pTemp->entity.curstate.rendercolor.g = 0;
	pTemp->entity.curstate.rendercolor.b = 0;
	pTemp->entity.origin[2] += 10;
	pTemp->entity.curstate.scale = scale;
}

void R_TempSphereModel( float* pos, float speed, float life, int count, int modelIndex )
{
	int i, j, frameCount;
	TEMPENTITY* pTemp;
	model_t* model;

	if ( !modelIndex )
		return;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	frameCount = ModelFrameCount( model );

	for ( i = 0; i < count; i++ )
	{
		pTemp = efx.CL_TempEntAlloc( pos, model );
		if ( !pTemp )
			return;

		pTemp->entity.curstate.body = RandomLong( 0, frameCount - 1 );

		if ( RandomLong( 0, 255 ) < 10 )
			pTemp->flags |= FTENT_SLOWGRAVITY;
		else
			pTemp->flags |= FTENT_GRAVITY;

		if ( RandomLong( 0, 255 ) < 220 )
		{
			pTemp->flags |= FTENT_ROTATE;
			for ( j = 0; j < 3; j++ )
				pTemp->entity.baseline.angles[j] = RandomFloat( -256, -255 );
		}

		if ( RandomLong( 0, 255 ) < 100 )
			pTemp->flags |= FTENT_SMOKETRAIL;

		pTemp->flags |= FTENT_FLICKER | FTENT_COLLIDEWORLD;
		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.curstate.effects = i & 31;

		for ( j = 0; j < 3; j++ )
			pTemp->entity.baseline.origin[j] = RandomFloat( -1, 1 );
		VectorNormalize( pTemp->entity.baseline.origin );
		VectorScale( pTemp->entity.baseline.origin, speed, pTemp->entity.baseline.origin );

		pTemp->die = (float)( life + cl.time );
	}
}

void R_Sprite_WallPuff( TEMPENTITY* pTemp, float scale )
{
	if ( !pTemp )
		return;

	pTemp->entity.curstate.scale = scale;
	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.curstate.rendercolor.r = 0;
	pTemp->entity.curstate.rendercolor.g = 0;
	pTemp->entity.curstate.rendercolor.b = 0;
	pTemp->die = (float)( cl.time + 0.01 );
	pTemp->entity.curstate.frame = 0;
	pTemp->entity.angles[ROLL] = (float)RandomLong( 0, 359 );
}

void R_SparkEffect( float* pos, int count, int velocityMin, int velocityMax )
{
	efx.R_SparkStreaks( pos, count, velocityMin, velocityMax );
	efx.R_RicochetSprite( pos, cl_sprite_ricochet, 0.1f, RandomFloat( 0.5f, 1.0f ) );
}

void R_BloodSprite( vec_t* org, int colorindex, int modelIndex, int modelIndex2, float size )
{
	int i, j, splatter, frameCount;
	TEMPENTITY* pTemp;
	model_t* model;
	model_t* model2;
	color24 impactColor, splatterColor;

	colorindex = 4 * ( colorindex + RandomLong( 1, 3 ) );
	impactColor.r = (byte)host_basepal[colorindex + 2];
	impactColor.g = (byte)host_basepal[colorindex + 1];
	impactColor.b = (byte)host_basepal[colorindex];
	splatterColor.r = (byte)host_basepal[colorindex - 2];
	splatterColor.g = (byte)host_basepal[colorindex - 3];
	splatterColor.b = (byte)host_basepal[colorindex - 4];

	if ( modelIndex2 )
	{
		model2 = CL_GetModelByIndex( modelIndex2 );
		if ( model2 )
		{
			frameCount = ModelFrameCount( model2 );

			splatter = RandomLong( 1, 8 );
			splatter += RandomLong( 1, 8 );
			splatter = (int)( (float)splatter + size );

			for ( i = 0; i < splatter; i++ )
			{
				pTemp = efx.CL_TempEntAlloc( org, model2 );
				if ( !pTemp )
					continue;

				pTemp->entity.curstate.rendermode = kRenderNormal;
				pTemp->entity.curstate.renderfx = kRenderFxClampMinScale;
				pTemp->entity.baseline.renderamt = 250;
				pTemp->entity.curstate.renderamt = 250;
				pTemp->entity.curstate.scale = RandomFloat( (float)( size / 15.0 ), (float)( size / 25.0 ) );
				pTemp->entity.curstate.rendercolor = splatterColor;
				pTemp->flags = FTENT_ROTATE | FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD;

				pTemp->entity.baseline.origin[0] = RandomFloat( -96, 95 );
				pTemp->entity.baseline.origin[1] = RandomFloat( -96, 95 );
				pTemp->entity.baseline.origin[2] = RandomFloat( -32, 95 );

				for ( j = 0; j < 3; j++ )
					pTemp->entity.baseline.angles[j] = RandomFloat( -256, -255 );

				pTemp->entity.curstate.framerate = 0;
				pTemp->die = (float)( RandomFloat( 1, 2 ) + cl.time );

				pTemp->entity.curstate.frame = (float)RandomLong( 1, frameCount - 1 );
				if ( pTemp->entity.curstate.frame > 8.0f )
					pTemp->entity.curstate.frame = (float)( frameCount - 1 );

				pTemp->entity.angles[ROLL] = (float)RandomLong( 0, 360 );
				pTemp->bounceFactor = 0;
			}
		}
	}

	if ( !modelIndex )
		return;

	model = CL_GetModelByIndex( modelIndex );
	if ( !model )
		return;

	frameCount = ModelFrameCount( model );

	pTemp = efx.CL_TempEntAllocHigh( org, model );
	if ( !pTemp )
		return;

	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.curstate.renderfx = kRenderFxClampMinScale;
	pTemp->entity.baseline.renderamt = 250;
	pTemp->entity.curstate.renderamt = 250;
	pTemp->entity.curstate.scale = RandomFloat( (float)( size / 25.0 ), (float)( size / 35.0 ) );
	pTemp->entity.curstate.rendercolor = impactColor;
	pTemp->flags = FTENT_SPRANIMATE;
	VectorClear( pTemp->entity.baseline.origin );

	pTemp->entity.curstate.framerate = (float)( 4 * frameCount );
	pTemp->entity.curstate.frame = 0;
	pTemp->frameMax = (float)frameCount;
	pTemp->die = (float)( frameCount / pTemp->entity.curstate.framerate + cl.time );

	pTemp->entity.angles[ROLL] = (float)RandomLong( 0, 360 );
	pTemp->bounceFactor = 0;
}

void R_MultiGunshot( vec_t* org, vec_t* dir, vec_t* noise, int count, int decalCount, int* decalIndices )
{
	pmtrace_t trace;
	vec3_t angles, right, up;
	vec3_t spreadRight, spreadUp, vecDir, vecEnd;
	float x, y;
	int i, decal, entity, texture;

	VectorAngles( dir, angles );
	AngleVectors( angles, NULL, right, up );

	for ( i = 0; i < count; i++ )
	{
		// get circular gaussian spread
		do
		{
			x = RandomFloat( -0.5f, 0.5f ) + RandomFloat( -0.5f, 0.5f );
			y = RandomFloat( -0.5f, 0.5f ) + RandomFloat( -0.5f, 0.5f );
		} while ( x * x + y * y > 1.0f );

		decal = RandomLong( 0, decalCount - 1 );

		VectorScale( right, x * noise[0], spreadRight );
		VectorScale( up, y * noise[1], spreadUp );
		VectorAdd( spreadRight, spreadUp, vecDir );
		VectorAdd( vecDir, dir, vecDir );
		VectorMA( org, 4096, vecDir, vecEnd );

		pmove->usehull = 2;
		trace = PM_PlayerTrace( org, vecEnd, PM_STUDIO_IGNORE, -1 );

		if ( trace.fraction != 1.0f )
		{
			if ( i & 2 )
				efx.R_RicochetSound( trace.endpos );

			efx.R_BulletImpactParticles( trace.endpos );

			if ( r_decals.value )
			{
				entity = pmove->physents[trace.ent].info;
				texture = efx.Draw_DecalIndex( decalIndices[decal] );
				efx.R_DecalShoot( texture, entity, 0, trace.endpos, 0 );
			}
		}
	}
}

static float ReadUnsignedByteFloat( void )
{
	return (float)(unsigned int)MSG_ReadByte();
}

typedef struct
{
	int modelIndex;
	int startFrame;
	float framerate;
	float life;
	float width;
	float amplitude;
	float red;
	float green;
	float blue;
	float brightness;
	float speed;
} beam_parameters_t;

static beam_parameters_t ReadBeamParameters( qboolean scaleWidth )
{
	beam_parameters_t beam;

	beam.modelIndex = MSG_ReadShort();
	beam.startFrame = MSG_ReadByte();
	beam.framerate = (float)( ReadUnsignedByteFloat() * 0.1 );
	beam.life = (float)( ReadUnsignedByteFloat() * 0.1 );
	beam.width = ReadUnsignedByteFloat();
	beam.amplitude = (float)MSG_ReadByte();
	beam.red = (float)( ReadUnsignedByteFloat() * ( 1.0 / 255.0 ) );
	beam.green = (float)( ReadUnsignedByteFloat() * ( 1.0 / 255.0 ) );
	beam.blue = (float)( ReadUnsignedByteFloat() * ( 1.0 / 255.0 ) );
	beam.brightness = (float)( ReadUnsignedByteFloat() * ( 1.0 / 255.0 ) );
	beam.speed = (float)(int)(float)( MSG_ReadByte() * 0.1 );

	if ( scaleWidth )
		beam.width = (float)( beam.width * 0.1 );
	beam.amplitude = (float)( beam.amplitude * 0.01 );

	return beam;
}

void CL_ParseTextMessage( void )
{
	int channel;
	client_textmessage_t* pNetMessage;

	channel = MSG_ReadByte() % MAX_NETWORK_TEXT_MESSAGES;
	pNetMessage = &gNetworkTextMessage[channel];

	pNetMessage->x = (float)( MSG_ReadShort() / 8192.0 );
	pNetMessage->y = (float)( MSG_ReadShort() / 8192.0 );
	pNetMessage->effect = MSG_ReadByte();
	pNetMessage->r1 = (byte)MSG_ReadByte();
	pNetMessage->g1 = (byte)MSG_ReadByte();
	pNetMessage->b1 = (byte)MSG_ReadByte();
	pNetMessage->a1 = (byte)MSG_ReadByte();
	pNetMessage->r2 = (byte)MSG_ReadByte();
	pNetMessage->g2 = (byte)MSG_ReadByte();
	pNetMessage->b2 = (byte)MSG_ReadByte();
	pNetMessage->a2 = (byte)MSG_ReadByte();
	pNetMessage->fadein = (float)( MSG_ReadWord() / 256.0 );
	pNetMessage->fadeout = (float)( MSG_ReadWord() / 256.0 );
	pNetMessage->holdtime = (float)( MSG_ReadWord() / 256.0 );

	if ( pNetMessage->effect == 2 )
		pNetMessage->fxtime = (float)( MSG_ReadWord() / 256.0 );

	pNetMessage->pName = gNetworkMessageNames[channel];
	pNetMessage->pMessage = gNetworkTextMessageBuffer[channel];

	Q_strncpy( gNetworkTextMessageBuffer[channel], MSG_ReadString(), NETWORK_TEXT_MESSAGE_BYTES - 1 );
	gNetworkTextMessageBuffer[channel][NETWORK_TEXT_MESSAGE_BYTES - 1] = 0;

	CL_HudMessage( pNetMessage->pName );
}

void R_Explosion( float* pos, int model, float scale, float framerate, int flags )
{
	TEMPENTITY* pTemp;
	dlight_t* dl;
	sfx_t* sounds[3];
	int sound;

	if ( scale != 0 )
	{
		pTemp = R_DefaultSprite( pos, model, framerate );
		efx.R_Sprite_Explode( pTemp, scale, flags );

		if ( !( flags & TE_EXPLFLAG_NOPARTICLES ) )
			efx.R_FlickerParticles( pos );

		if ( !( flags & TE_EXPLFLAG_NODLIGHTS ) )
		{
			// big flash
			dl = efx.CL_AllocDlight( 0 );
			VectorCopy( pos, dl->origin );
			dl->radius = 200;
			dl->color.r = 250;
			dl->color.g = 250;
			dl->color.b = 150;
			dl->decay = 800;
			dl->die = (float)( cl.time + 0.01 );

			// red glow
			dl = efx.CL_AllocDlight( 0 );
			VectorCopy( pos, dl->origin );
			dl->radius = 150;
			dl->color.r = 255;
			dl->color.g = 190;
			dl->color.b = 40;
			dl->decay = 200;
			dl->die = (float)( cl.time + 1.0 );
		}
	}

	if ( !( flags & TE_EXPLFLAG_NOSOUND ) )
	{
		sound = RandomLong( 0, 2 );
		sounds[0] = cl_sfx_r_exp1;
		sounds[1] = cl_sfx_r_exp2;
		sounds[2] = cl_sfx_r_exp3;

		if ( sound >= 0 && sound < 3 )
			S_StartDynamicSound( -1, CHAN_AUTO, sounds[sound], pos, 1.0f, 0.3f, 0, PITCH_NORM );
	}
}

void CL_ParseTEnt( void )
{
	vec3_t pos, end, dir;
	int i, type;

	type = MSG_ReadByte();

	switch ( type )
	{
	case TE_BEAMPOINTS:
	{
		beam_parameters_t beam;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			end[i] = MSG_ReadCoord( &net_message );
		beam = ReadBeamParameters( TRUE );

		efx.R_BeamPoints( pos, end, beam.modelIndex, beam.life, beam.width, beam.amplitude, beam.brightness, beam.speed,
			beam.startFrame, beam.framerate, beam.red, beam.green, beam.blue );
		break;
	}

	case TE_BEAMENTPOINT:
	{
		beam_parameters_t beam;
		int startEnt;

		startEnt = MSG_ReadShort();
		for ( i = 0; i < 3; i++ )
			end[i] = MSG_ReadCoord( &net_message );
		beam = ReadBeamParameters( TRUE );

		efx.R_BeamEntPoint( startEnt, end, beam.modelIndex, beam.life, beam.width, beam.amplitude, beam.brightness,
			beam.speed, beam.startFrame, beam.framerate, beam.red, beam.green, beam.blue );
		break;
	}

	case TE_BEAMENTS:
	{
		beam_parameters_t beam;
		int startEnt, endEnt;

		startEnt = MSG_ReadShort();
		endEnt = MSG_ReadShort();
		beam = ReadBeamParameters( TRUE );

		efx.R_BeamEnts( startEnt, endEnt, beam.modelIndex, beam.life, beam.width, beam.amplitude, beam.brightness,
			beam.speed, beam.startFrame, beam.framerate, beam.red, beam.green, beam.blue );
		break;
	}

	case TE_IMPLOSION:
	{
		float radius, life;
		int count;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		radius = (float)MSG_ReadByte();
		count = MSG_ReadByte();
		life = (float)( MSG_ReadByte() / 10.0 );

		efx.R_Implosion( pos, radius, count, life );
		break;
	}

	case TE_SPRITETRAIL:
	{
		int modelIndex, count;
		float life, size, amplitude, speed;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			end[i] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		life = (float)( ReadUnsignedByteFloat() * 0.1 );
		size = ReadUnsignedByteFloat();
		if ( size == 0 )
			size = 1.0f;
		else
			size = (float)( size * 0.1 );
		amplitude = (float)( MSG_ReadByte() * 10.0 );
		speed = (float)(int)(float)( MSG_ReadByte() * 10.0 );

		efx.R_Sprite_Trail( type, pos, end, modelIndex, count, life, size, amplitude, 255, speed );
		break;
	}

	case TE_SPRITE:
	case TE_GLOWSPRITE:
	{
		int modelIndex;
		float life, scale, a;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();

		life = 0;
		if ( type == TE_GLOWSPRITE )
			life = (float)( ReadUnsignedByteFloat() * 0.1 );

		scale = (float)( MSG_ReadByte() * 0.1 );
		a = (float)( ReadUnsignedByteFloat() * ( 1.0 / 255.0 ) );

		if ( type == TE_SPRITE )
			efx.R_TempSprite( pos, vec3_origin, scale, modelIndex, kRenderTransAdd, kRenderFxNone, a, life,
				FTENT_SPRANIMATE );
		else
			efx.R_TempSprite( pos, vec3_origin, scale, modelIndex, kRenderGlow, kRenderFxNoDissipation, a, life,
				FTENT_FADEOUT );
		break;
	}

	case TE_BEAMSPRITE:
	{
		int beamIndex, spriteIndex;
		float brightness;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			end[i] = MSG_ReadCoord( &net_message );
		beamIndex = MSG_ReadShort();
		spriteIndex = MSG_ReadShort();
		brightness = RandomFloat( 0.5f, 0.655f );

		efx.R_BeamPoints( pos, end, beamIndex, 0.01f, 0.4f, 0.0f, brightness, 5.0f, 0, 0.0f, 1.0f, 0.0f, 0.0f );
		efx.R_TempSprite( end, vec3_origin, 0.1f, spriteIndex, kRenderTransAdd, kRenderFxNone, 0.35f, 0.01f, 0 );
		break;
	}

	case TE_BEAMTORUS:
	case TE_BEAMDISK:
	case TE_BEAMCYLINDER:
	{
		beam_parameters_t beam;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			end[i] = MSG_ReadCoord( &net_message );
		beam = ReadBeamParameters( FALSE );

		efx.R_BeamCirclePoints( type, pos, end, beam.modelIndex, beam.life, beam.width, beam.amplitude, beam.brightness,
			beam.speed, beam.startFrame, beam.framerate, beam.red, beam.green, beam.blue );
		break;
	}

	case TE_BEAMRING:
	{
		beam_parameters_t beam;
		int startEnt, endEnt;

		startEnt = MSG_ReadShort();
		endEnt = MSG_ReadShort();
		beam = ReadBeamParameters( TRUE );

		efx.R_BeamRing( startEnt, endEnt, beam.modelIndex, beam.life, beam.width, beam.amplitude, beam.brightness,
			beam.speed, beam.startFrame, beam.framerate, beam.red, beam.green, beam.blue );
		break;
	}

	case TE_BEAMFOLLOW:
	{
		int startEnt, modelIndex;
		float life, width, r, g, b, brightness;

		startEnt = MSG_ReadShort();
		modelIndex = MSG_ReadShort();
		life = (float)( ReadUnsignedByteFloat() * 0.1 );
		width = ReadUnsignedByteFloat();
		r = (float)( ReadUnsignedByteFloat() * ( 1.0 / 255.0 ) );
		g = (float)( ReadUnsignedByteFloat() * ( 1.0 / 255.0 ) );
		b = (float)( ReadUnsignedByteFloat() * ( 1.0 / 255.0 ) );
		brightness = (float)( ReadUnsignedByteFloat() * ( 1.0 / 255.0 ) );

		efx.R_BeamFollow( startEnt, modelIndex, life, width, r, g, b, brightness );
		break;
	}

	case TE_STREAK_SPLASH:
	{
		int color, count, iRand;
		float speed;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			dir[i] = MSG_ReadCoord( &net_message );
		color = MSG_ReadByte();
		count = MSG_ReadShort();
		speed = (float)MSG_ReadShort();
		iRand = MSG_ReadShort();

		efx.R_StreakSplash( pos, dir, color, count, speed, -iRand, iRand );
		break;
	}

	case TE_DLIGHT:
	{
		dlight_t* dl;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );

		dl = efx.CL_AllocDlight( 0 );
		VectorCopy( pos, dl->origin );
		dl->radius = (float)( 10 * MSG_ReadByte() );
		dl->color.r = (byte)MSG_ReadByte();
		dl->color.g = (byte)MSG_ReadByte();
		dl->color.b = (byte)MSG_ReadByte();
		dl->die = (float)( MSG_ReadByte() * 0.1 + cl.time );
		dl->decay = (float)( 10 * MSG_ReadByte() );
		break;
	}

	case TE_ELIGHT:
	{
		dlight_t* dl;
		float life, decay;

		dl = efx.CL_AllocElight( MSG_ReadShort() );
		for ( i = 0; i < 3; i++ )
			dl->origin[i] = MSG_ReadCoord( &net_message );
		dl->radius = MSG_ReadCoord( &net_message );
		dl->color.r = (byte)MSG_ReadByte();
		dl->color.g = (byte)MSG_ReadByte();
		dl->color.b = (byte)MSG_ReadByte();
		life = (float)( MSG_ReadByte() * 0.1 );
		dl->die = (float)( life + cl.time );
		decay = MSG_ReadCoord( &net_message );
		dl->decay = decay;
		if ( life != 0 )
			dl->decay = decay / life;
		break;
	}

	case TE_TEXTMESSAGE:
		CL_ParseTextMessage();
		break;

	case TE_LINE:
	case TE_BOX:
	{
		float life;
		byte r, g, b;
		int color;
		vec3_t mins, maxs;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			end[i] = MSG_ReadCoord( &net_message );
		life = (float)( MSG_ReadShort() * 0.1 );
		r = (byte)MSG_ReadByte();
		g = (byte)MSG_ReadByte();
		b = (byte)MSG_ReadByte();

		if ( type == TE_LINE )
		{
			R_ParticleLine( pos, end, r, g, b, life );
		}
		else
		{
			color = R_LookupColor( r, g, b );

			for ( i = 0; i < 3; i++ )
			{
				dir[i] = (float)( ( pos[i] + end[i] ) * 0.5 );
				mins[i] = pos[i] - dir[i];
				maxs[i] = end[i] - dir[i];
			}
			PM_DrawBBox( mins, maxs, dir, color, life );
		}
		break;
	}

	case TE_GUNSHOT:			// bullet hitting wall
		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );

		efx.R_RunParticleEffect( pos, vec3_origin, 0, 20 );
		efx.R_RicochetSound( pos );
		break;

	case TE_EXPLOSION:			// rocket explosion
	{
		int modelIndex, flags;
		float scale, framerate;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		scale = (float)( MSG_ReadByte() * 0.1 );
		framerate = (float)MSG_ReadByte();
		flags = MSG_ReadByte();

		R_Explosion( pos, modelIndex, scale, framerate, flags );
		break;
	}

	case TE_TAREXPLOSION:			// tarbaby explosion
		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );

		efx.R_BlobExplosion( pos );
		S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_r_exp1, pos, 1.0f, 1.0f, 0, PITCH_NORM );
		break;

	case TE_SMOKE:
	{
		TEMPENTITY* pTemp;
		int modelIndex;
		float scale, framerate;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		scale = (float)( MSG_ReadByte() * 0.1 );
		framerate = (float)MSG_ReadByte();

		pTemp = R_DefaultSprite( pos, modelIndex, framerate );
		efx.R_Sprite_Smoke( pTemp, scale );
		break;
	}

	case TE_TRACER:
	case TE_SHOWLINE:
		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			end[i] = MSG_ReadCoord( &net_message );

		if ( type == TE_TRACER )
			efx.R_TracerEffect( pos, end );
		else
			efx.R_ShowLine( pos, end );
		break;

	case TE_LIGHTNING:
	{
		int life, modelIndex;
		float width, amplitude;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			end[i] = MSG_ReadCoord( &net_message );
		life = MSG_ReadByte();
		width = (float)MSG_ReadByte();
		amplitude = (float)MSG_ReadByte();
		modelIndex = MSG_ReadShort();

		efx.R_BeamLightning( pos, end, modelIndex, (float)( life * 0.1 ), (float)( width * 0.1 ),
			(float)( amplitude * 0.01 ), 0.6f, 3.5f );
		break;
	}

	case TE_SPARKS:
	case TE_LAVASPLASH:
	case TE_TELEPORT:
		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );

		if ( type == TE_SPARKS )
			efx.R_SparkEffect( pos, 8, -200, 200 );
		else if ( type == TE_LAVASPLASH )
			efx.R_LavaSplash( pos );
		else
			efx.R_TeleportSplash( pos );
		break;

	case TE_EXPLOSION2:				// color mapped explosion
	{
		dlight_t* dl;
		int colorStart, colorLength;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		colorStart = MSG_ReadByte();
		colorLength = MSG_ReadByte();

		efx.R_ParticleExplosion2( pos, colorStart, colorLength );

		dl = efx.CL_AllocDlight( 0 );
		VectorCopy( pos, dl->origin );
		dl->radius = 350;
		dl->decay = 300;
		dl->die = (float)( cl.time + 0.5 );

		S_StartDynamicSound( -1, CHAN_AUTO, cl_sfx_r_exp1, pos, 1.0f, 0.6f, 0, PITCH_NORM );
		break;
	}

	case TE_BSPDECAL:
	case TE_DECAL:
	case TE_WORLDDECAL:
	case TE_WORLDDECALHIGH:
	case TE_DECALHIGH:
	{
		int decal, entity, modelIndex, flags;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );

		entity = 0;
		modelIndex = 0;
		flags = 0;

		if ( type == TE_BSPDECAL )
		{
			flags = FDECAL_PERMANENT;
			decal = MSG_ReadShort();
			entity = MSG_ReadShort();
			if ( entity )
				modelIndex = MSG_ReadShort();
		}
		else
		{
			decal = MSG_ReadByte();
			if ( type == TE_DECAL || type == TE_DECALHIGH )
				entity = MSG_ReadShort();
			if ( type == TE_WORLDDECALHIGH || type == TE_DECALHIGH )
				decal += 256;
		}

		if ( !CL_GetEntityIfValid( entity ) )
		{
			Con_DPrintf( "Decal: invalid entity %i\n", entity );
			break;
		}

		if ( r_decals.value )
			efx.R_DecalShoot( efx.Draw_DecalIndex( decal ), entity, modelIndex, pos, flags );
		break;
	}

	case TE_KILLBEAM:
		efx.R_BeamKill( MSG_ReadShort() );
		break;

	case TE_LARGEFUNNEL:
	{
		int modelIndex, reverse;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		reverse = MSG_ReadShort();

		efx.R_LargeFunnel( pos, reverse );
		efx.R_FunnelSprite( pos, modelIndex, reverse );
		break;
	}

	case TE_BLOODSTREAM:
	case TE_BLOOD:				// bullets hitting body
	{
		int color, speed;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			dir[i] = MSG_ReadCoord( &net_message );
		color = MSG_ReadByte();
		speed = MSG_ReadByte();

		if ( CL_ShowTEBlood() )
		{
			if ( type == TE_BLOODSTREAM )
				efx.R_BloodStream( pos, dir, color, speed );
			else
				efx.R_Blood( pos, dir, color, speed );
		}
		break;
	}

	case TE_FIZZ:
	{
		int entity, modelIndex, density;

		entity = MSG_ReadShort();
		modelIndex = MSG_ReadShort();
		density = MSG_ReadByte();

		cl_entity_t* fizzEntity = CL_GetEntityIfValid( entity );
		if ( !fizzEntity )
		{
			Con_DPrintf( "Bubble: invalid entity %i\n", entity );
			break;
		}
		efx.R_FizzEffect( fizzEntity, modelIndex, density );
		break;
	}

	case TE_MODEL:
	{
		vec3_t angles;
		int modelIndex, soundtype;
		float life;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			dir[i] = MSG_ReadCoord( &net_message );
		angles[PITCH] = 0;
		angles[YAW] = MSG_ReadAngle();
		angles[ROLL] = 0;
		modelIndex = MSG_ReadShort();
		soundtype = MSG_ReadByte();
		life = (float)( MSG_ReadByte() * 0.1 );

		efx.R_TempModel( pos, dir, angles, life, modelIndex, soundtype );
		break;
	}

	case TE_EXPLODEMODEL:
	{
		int modelIndex, count;
		float speed, life;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		speed = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		count = MSG_ReadShort();
		life = (float)( MSG_ReadByte() * 0.1 );

		efx.R_TempSphereModel( pos, speed, life, count, modelIndex );
		break;
	}

	case TE_BREAKMODEL:
	{
		vec3_t size;
		int modelIndex, count;
		float random, life;
		char flags;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			size[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			dir[i] = MSG_ReadCoord( &net_message );
		random = (float)( MSG_ReadByte() * 10.0 );
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		life = (float)( MSG_ReadByte() * 0.1 );
		flags = (char)MSG_ReadByte();

		efx.R_BreakModel( pos, size, dir, random, life, count, modelIndex, flags );
		break;
	}

	case TE_GUNSHOTDECAL:
	{
		sfx_t* sounds[5];
		int entity, decal, iRand;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		entity = MSG_ReadShort();
		decal = MSG_ReadByte();

		if ( !CL_GetEntityIfValid( entity ) )
		{
			Con_DPrintf( "Decal: invalid entity %i\n", entity );
			break;
		}

		efx.R_BulletImpactParticles( pos );

		iRand = RandomLong( 0, 0x7FFF );
		if ( iRand < 0x3FFF )
		{
			sounds[0] = cl_sfx_ric1;
			sounds[1] = cl_sfx_ric2;
			sounds[2] = cl_sfx_ric3;
			sounds[3] = cl_sfx_ric4;
			sounds[4] = cl_sfx_ric5;
			S_StartDynamicSound( -1, CHAN_AUTO, sounds[iRand % 5], pos, 1.0f, 1.0f, 0, PITCH_NORM );
		}

		if ( r_decals.value )
			efx.R_DecalShoot( efx.Draw_DecalIndex( decal ), entity, 0, pos, 0 );
		break;
	}

	case TE_SPRITE_SPRAY:
	{
		int modelIndex, count, speed, iRand;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			dir[i] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		speed = MSG_ReadByte();
		iRand = MSG_ReadByte();

		efx.R_Sprite_Spray( pos, dir, modelIndex, count, 2 * speed, iRand );
		break;
	}

	case TE_ARMOR_RICOCHET:
	{
		float scale;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		scale = (float)( MSG_ReadByte() * 0.1 );

		efx.R_RicochetSprite( pos, cl_sprite_ricochet, 0.1f, scale );
		efx.R_RicochetSound( pos );
		break;
	}

	case TE_PLAYERDECAL:
	{
		unsigned int playernum;
		customization_t* pCust;
		cachewad_t* pWad;
		texture_t* pTexture;
		int entity, decal;

		playernum = (unsigned int)( MSG_ReadByte() - 1 );
		pCust = playernum < MAX_CLIENTS ? cl.players[playernum].customdata.pNext : NULL;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		entity = MSG_ReadShort();
		decal = MSG_ReadByte();

		if ( !CL_GetEntityIfValid( entity ) )
		{
			Con_DPrintf( "Decal: invalid entity %i\n", entity );
			break;
		}

		pTexture = NULL;
		if ( pCust && pCust->pBuffer && pCust->pInfo && ( pCust->resource.ucFlags & RES_CUSTOM )
			 && pCust->resource.type == t_decal && pCust->bTranslated )
		{
			pWad = (cachewad_t*)pCust->pInfo;
			if ( decal >= pWad->lumpCount - 1 )
				decal = pWad->lumpCount - 1;
			pCust->nUserData1 = decal;
			pTexture = (texture_t*)Draw_CustomCacheGet( pWad, pCust->pBuffer, pCust->resource.nDownloadSize, decal );
		}

		if ( r_decals.value )
		{
			if ( pCust )
			{
				if ( pTexture )
					R_CustomDecalShoot( pTexture, (int)playernum, entity, 0, pos, FDECAL_CUSTOM );
			}
			else
			{
				efx.R_DecalShoot( efx.Draw_DecalIndex( decal ), entity, 0, pos, FDECAL_CUSTOM );
			}
		}
		break;
	}

	case TE_BUBBLES:
	case TE_BUBBLETRAIL:
	{
		int modelIndex, count;
		float height, speed;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			end[i] = MSG_ReadCoord( &net_message );
		height = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		speed = MSG_ReadCoord( &net_message );

		if ( type == TE_BUBBLES )
			efx.R_Bubbles( pos, end, height, modelIndex, count, speed );
		else
			efx.R_BubbleTrail( pos, end, height, modelIndex, count, speed );
		break;
	}

	case TE_BLOODSPRITE:
	{
		int modelIndex, modelIndex2, color;
		float size;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		modelIndex2 = MSG_ReadShort();
		color = MSG_ReadByte();
		size = (float)MSG_ReadByte();

		if ( CL_ShowTEBlood() )
			efx.R_BloodSprite( pos, color, modelIndex, modelIndex2, size );
		break;
	}

	case TE_PROJECTILE:
	{
		int modelIndex, life, owner;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			dir[i] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		life = MSG_ReadByte();
		owner = MSG_ReadByte();

		efx.R_Projectile( pos, dir, modelIndex, life, owner, NULL );
		break;
	}

	case TE_SPRAY:
	{
		int modelIndex, count, speed, iRand, rendermode;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			dir[i] = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		speed = MSG_ReadByte();
		iRand = MSG_ReadByte();
		rendermode = MSG_ReadByte();

		efx.R_Spray( pos, dir, modelIndex, count, speed, iRand, rendermode );
		break;
	}

	case TE_PLAYERSPRITES:
	{
		int entity, modelIndex, count, size;

		entity = MSG_ReadShort();
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		size = MSG_ReadByte();

		efx.R_PlayerSprites( entity, modelIndex, count, size );
		break;
	}

	case TE_PARTICLEBURST:
	{
		int size, color;
		float life;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		size = MSG_ReadShort();
		color = MSG_ReadByte();
		life = (float)( MSG_ReadByte() / 10.0 );

		efx.R_ParticleBurst( pos, size, color, life );
		break;
	}

	case TE_FIREFIELD:
	{
		int size, modelIndex, count, flags;
		float life;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		size = MSG_ReadShort();
		modelIndex = MSG_ReadShort();
		count = MSG_ReadByte();
		flags = MSG_ReadByte();
		life = (float)( MSG_ReadByte() / 10.0 );

		efx.R_FireField( pos, size, modelIndex, count, flags, life );
		break;
	}

	case TE_PLAYERATTACHMENT:
	{
		int client, modelIndex;
		float zoffset, life;

		client = MSG_ReadByte();
		zoffset = MSG_ReadCoord( &net_message );
		modelIndex = MSG_ReadShort();
		life = (float)MSG_ReadShort() / 10.0f;

		efx.R_AttachTentToPlayer( client, modelIndex, zoffset, life );
		break;
	}

	case TE_KILLPLAYERATTACHMENTS:
		efx.R_KillAttachedTents( MSG_ReadByte() );
		break;

	case TE_MULTIGUNSHOT:
	{
		vec3_t noise;
		int count, decal;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			dir[i] = (float)( MSG_ReadCoord( &net_message ) / 10.0 );
		noise[0] = (float)( MSG_ReadCoord( &net_message ) / 100.0 );
		noise[1] = (float)( MSG_ReadCoord( &net_message ) / 100.0 );
		noise[2] = 0;
		count = MSG_ReadByte();
		decal = MSG_ReadByte();

		efx.R_MultiGunshot( pos, dir, noise, count, 1, &decal );
		break;
	}

	case TE_USERTRACER:
	{
		float life, length;
		int color;

		for ( i = 0; i < 3; i++ )
			pos[i] = MSG_ReadCoord( &net_message );
		for ( i = 0; i < 3; i++ )
			dir[i] = MSG_ReadCoord( &net_message );
		life = (float)( MSG_ReadByte() / 10.0 );
		color = MSG_ReadByte();
		length = (float)( MSG_ReadByte() / 10.0 );

		efx.R_UserTracerParticle( pos, dir, life, color, length, 0, NULL );
		break;
	}

	default:
		Sys_Error( "CL_ParseTEnt: bad type" );
		break;
	}
}

void R_TracerEffect( vec_t* start, vec_t* end )
{
	vec3_t temp, vel;
	float len;

	if ( tracerSpeed.value <= 0 )
		tracerSpeed.value = 3;

	VectorSubtract( end, start, temp );
	len = Length( temp );

	VectorScale( temp, 1.0f / len, temp );
	VectorScale( temp, (float)RandomLong( -10, 9 ) + tracerOffset.value, vel );
	VectorAdd( start, vel, start );
	VectorScale( temp, tracerSpeed.value, vel );

	efx.R_TracerParticles( start, vel, len / tracerSpeed.value );
}

void R_Spray( vec_t* pos, vec_t* dir, int modelIndex, int count, int speed, int spread, int rendermode )
{
	TEMPENTITY* pTemp;
	model_t* pModel;
	float noise;
	float znoise;
	int lastFrame;
	int i;

	noise = (float)spread / 100.0f;

	// more vertical displacement
	znoise = (float)( noise * 1.5 );
	if ( znoise > 1 )
		znoise = 1;

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	lastFrame = ModelFrameCount( pModel ) - 1;

	for ( i = 0; i < count; i++ )
	{
		pTemp = efx.CL_TempEntAlloc( pos, pModel );
		if ( !pTemp )
			return;

		pTemp->entity.curstate.rendermode = rendermode;
		pTemp->entity.curstate.renderamt = 255;
		pTemp->entity.baseline.renderamt = 255;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.framerate = 1.0f;
		pTemp->flags |= FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD;

		if ( lastFrame > 1 )
		{
			pTemp->flags |= FTENT_SPRANIMATE;
			pTemp->entity.curstate.framerate = 10;
			pTemp->die = (float)( lastFrame * 0.1 + cl.time );
		}
		else
		{
			pTemp->die = (float)( cl.time + 0.35 );
		}

		// make the spittle fly the direction indicated, but mix in some noise.
		pTemp->entity.baseline.origin[0] = RandomFloat( -noise, noise ) + dir[0];
		pTemp->entity.baseline.origin[1] = RandomFloat( -noise, noise ) + dir[1];
		pTemp->entity.baseline.origin[2] = RandomFloat( 0, znoise ) + dir[2];
		VectorScale( pTemp->entity.baseline.origin, RandomFloat( (float)( speed * 0.8 ), (float)( speed * 1.2 ) ),
			pTemp->entity.baseline.origin );

		VectorCopy( pos, pTemp->entity.origin );

		pTemp->entity.curstate.frame = 0;
		pTemp->frameMax = (float)lastFrame;
	}
}

void R_PlayerSprites( int client, int modelIndex, int count, int size )
{
	TEMPENTITY* pTemp;
	cl_entity_t* pEnt;
	model_t* pModel;
	vec3_t position, dir;
	float minRate;
	int lastFrame;
	int i;

	minRate = (float)( 1.0 - size / 100.0 );

	if ( client < 1 || client > cl.maxclients )
	{
		Con_Printf( "Bad ent in R_PlayerSprites!\n" );
		return;
	}

	pEnt = &cl_entities[client];

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	lastFrame = ModelFrameCount( pModel ) - 1;

	for ( i = 0; i < count; i++ )
	{
		position[0] = RandomFloat( -10, 10 ) + pEnt->origin[0];
		position[1] = RandomFloat( -10, 10 ) + pEnt->origin[1];
		position[2] = RandomFloat( -20, 36 ) + pEnt->origin[2];

		pTemp = efx.CL_TempEntAlloc( position, pModel );
		if ( !pTemp )
			return;

		VectorSubtract( pTemp->entity.origin, pEnt->origin, pTemp->tentOffset );

		if ( i != 0 )
		{
			pTemp->flags |= FTENT_PLYRATTACHMENT;
			pTemp->clientIndex = (short)client;
		}
		else
		{
			VectorSubtract( position, pEnt->origin, dir );
			VectorNormalize( dir );
			VectorScale( dir, 60, dir );
			pTemp->entity.baseline.origin[0] = dir[0];
			pTemp->entity.baseline.origin[1] = dir[1];
			pTemp->entity.baseline.origin[2] = RandomFloat( 20, 60 );
		}

		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.curstate.renderamt = 255;
		pTemp->entity.baseline.renderamt = 255;
		pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		pTemp->entity.curstate.framerate = RandomFloat( minRate, 1.0f );

		if ( lastFrame > 1 )
		{
			pTemp->flags |= FTENT_SPRANIMATE;
			pTemp->entity.curstate.framerate = 20;
			pTemp->die = (float)( lastFrame * 0.05 + cl.time );
		}
		else
		{
			pTemp->die = (float)( cl.time + 0.35 );
		}

		pTemp->entity.curstate.frame = 0;
		pTemp->frameMax = (float)lastFrame;
	}
}

TEMPENTITY* CL_AllocCustomTempEntity(
	float* origin, model_t* model, int high, void ( *callback )( tempent_s*, float frametime, float currenttime ) )
{
	TEMPENTITY* pTemp;

	if ( high )
		pTemp = efx.CL_TempEntAllocHigh( origin, model );
	else
		pTemp = efx.CL_TempEntAlloc( origin, model );

	if ( pTemp )
	{
		pTemp->flags |= FTENT_CLIENTCUSTOM;
		pTemp->callback = callback;
		pTemp->die = (float)cl.time;
	}

	return pTemp;
}

void R_RicochetSound( vec_t* pos )
{
	sfx_t* sound;

	switch ( RandomLong( 0, 4 ) )
	{
	case 0:
		sound = cl_sfx_ric1;
		break;
	case 1:
		sound = cl_sfx_ric2;
		break;
	case 2:
		sound = cl_sfx_ric3;
		break;
	case 3:
		sound = cl_sfx_ric4;
		break;
	case 4:
		sound = cl_sfx_ric5;
		break;
	default:
		return;
	}

	S_StartDynamicSound( -1, CHAN_AUTO, sound, pos, 1.0f, 1.0f, 0, PITCH_NORM );
}

void R_RocketFlare( float* pos )
{
	TEMPENTITY* pTemp;
	int frameCount;

	// don't spawn while paused
	if ( cl.time == cl.oldtime )
		return;

	frameCount = ModelFrameCount( cl_sprite_glow );

	pTemp = efx.CL_TempEntAlloc( pos, cl_sprite_glow );
	if ( !pTemp )
		return;

	pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.curstate.framerate = 1.0f;
	pTemp->frameMax = (float)frameCount;
	VectorCopy( pos, pTemp->entity.origin );
	pTemp->die = (float)( cl.time + 0.01 );
	pTemp->entity.curstate.frame = (float)RandomLong( 0, frameCount - 1 );
}

void R_SparkShower( float* pos )
{
	TEMPENTITY* pTemp;

	pTemp = efx.CL_TempEntAllocNoModel( pos );
	if ( !pTemp )
		return;

	pTemp->entity.baseline.origin[0] = RandomFloat( -300, 300 );
	pTemp->entity.baseline.origin[1] = RandomFloat( -300, 300 );
	pTemp->entity.baseline.origin[2] = RandomFloat( -200, 200 );
	pTemp->flags |= FTENT_SPARKSHOWER | FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY;
	VectorClear( pTemp->entity.baseline.angles );
	pTemp->die = (float)( cl.time + 0.5 );
	pTemp->entity.curstate.framerate = RandomFloat( 0.5f, 1.5f );

	pTemp->entity.curstate.scale = (float)cl.time;
}

void R_FireField( vec_t* org, int radius, int modelIndex, int count, int flags, float life )
{
	TEMPENTITY* pTemp;
	model_t* pModel;
	vec3_t pos;
	float duration;
	int lastFrame;
	int i;

	pModel = CL_GetModelByIndex( modelIndex );
	if ( !pModel )
	{
		Con_Printf( "No model %d!\n", modelIndex );
		return;
	}

	lastFrame = ModelFrameCount( pModel ) - 1;

	for ( i = 0; i < count; i++ )
	{
		pos[0] = RandomFloat( (float)-radius, (float)radius ) + org[0];
		pos[1] = RandomFloat( (float)-radius, (float)radius ) + org[1];
		if ( flags & TEFIRE_FLAG_PLANAR )
			pos[2] = org[2];
		else
			pos[2] = RandomFloat( (float)-radius, (float)radius ) + org[2];

		pTemp = efx.CL_TempEntAlloc( pos, pModel );
		if ( !pTemp )
			return;

		if ( flags & TEFIRE_FLAG_ALPHA )
		{
			pTemp->entity.curstate.rendermode = kRenderTransAlpha;
			pTemp->entity.curstate.renderamt = 128;
			pTemp->entity.baseline.renderamt = 128;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		}
		else if ( flags & TEFIRE_FLAG_ADDITIVE )
		{
			pTemp->entity.curstate.rendermode = kRenderTransAdd;
			pTemp->entity.curstate.renderamt = 180;
		}
		else
		{
			pTemp->entity.curstate.rendermode = kRenderNormal;
			pTemp->entity.curstate.renderamt = 255;
			pTemp->entity.baseline.renderamt = 255;
			pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
		}

		pTemp->entity.curstate.framerate = RandomFloat( 0.75f, 1.25f );

		duration = RandomFloat( -0.25f, 0.5f ) + life;
		pTemp->die = (float)( duration + cl.time );

		if ( lastFrame > 1 )
		{
			pTemp->flags |= FTENT_SPRANIMATE;

			if ( flags & TEFIRE_FLAG_LOOP )
			{
				pTemp->entity.curstate.framerate = 15;
				pTemp->flags |= FTENT_SPRANIMATELOOP;
			}
			else
			{
				pTemp->entity.curstate.framerate = (float)lastFrame / duration;
			}
		}

		pTemp->entity.curstate.frame = 0;
		VectorClear( pTemp->entity.baseline.origin );
		pTemp->frameMax = (float)lastFrame;

		if ( ( flags & TEFIRE_FLAG_ALLFLOAT ) || ( ( flags & TEFIRE_FLAG_SOMEFLOAT ) && RandomLong( 0, 1 ) == 0 ) )
		{
			pTemp->entity.baseline.origin[2] = RandomFloat( 10, 30 );
		}
	}
}

int CL_FxBlend( cl_entity_t* ent )
{
	int blend;
	float offset, dist;
	vec3_t tmp;

	offset = (float)( ent->curstate.number * 363.0 );

	switch ( ent->curstate.renderfx )
	{
	case kRenderFxPulseSlow:
		blend = (int)( ent->curstate.renderamt + sin( cl.time * 2.0 + offset ) * 16.0 );
		break;
	case kRenderFxPulseFast:
		blend = (int)( ent->curstate.renderamt + sin( cl.time * 8.0 + offset ) * 16.0 );
		break;
	case kRenderFxPulseSlowWide:
		blend = (int)( ent->curstate.renderamt + sin( cl.time * 2.0 + offset ) * 64.0 );
		break;
	case kRenderFxPulseFastWide:
		blend = (int)( ent->curstate.renderamt + sin( cl.time * 8.0 + offset ) * 64.0 );
		break;
	case kRenderFxFadeSlow:
		if ( ent->curstate.renderamt > 0 )
			ent->curstate.renderamt -= 1;
		else
			ent->curstate.renderamt = 0;
		blend = ent->curstate.renderamt;
		break;
	case kRenderFxFadeFast:
		if ( ent->curstate.renderamt > 3 )
			ent->curstate.renderamt -= 4;
		else
			ent->curstate.renderamt = 0;
		blend = ent->curstate.renderamt;
		break;
	case kRenderFxSolidSlow:
		if ( ent->curstate.renderamt < 255 )
			ent->curstate.renderamt += 1;
		else
			ent->curstate.renderamt = 255;
		blend = ent->curstate.renderamt;
		break;
	case kRenderFxSolidFast:
		if ( ent->curstate.renderamt < 252 )
			ent->curstate.renderamt += 4;
		else
			ent->curstate.renderamt = 255;
		blend = ent->curstate.renderamt;
		break;
	case kRenderFxStrobeSlow:
		blend = (int)( sin( cl.time * 4.0 + offset ) * 20.0 ) >= 0 ? ent->curstate.renderamt : 0;
		break;
	case kRenderFxStrobeFast:
		blend = (int)( sin( cl.time * 16.0 + offset ) * 20.0 ) >= 0 ? ent->curstate.renderamt : 0;
		break;
	case kRenderFxStrobeFaster:
		blend = (int)( sin( cl.time * 36.0 + offset ) * 20.0 ) >= 0 ? ent->curstate.renderamt : 0;
		break;
	case kRenderFxFlickerSlow:
		blend = (int)( ( sin( cl.time * 17.0 + offset ) + sin( cl.time * 2.0 ) ) * 20.0 ) >= 0
			? ent->curstate.renderamt
			: 0;
		break;
	case kRenderFxFlickerFast:
		blend = (int)( ( sin( cl.time * 23.0 + offset ) + sin( cl.time * 16.0 ) ) * 20.0 ) >= 0
			? ent->curstate.renderamt
			: 0;
		break;
	case kRenderFxDistort:
	case kRenderFxHologram:
		VectorSubtract( ent->origin, r_origin, tmp );
		dist = DotProduct( tmp, vpn );

		// Turn off distance fade
		if ( ent->curstate.renderfx == kRenderFxDistort )
			dist = 1;
		else if ( dist <= 0 )
			return 0;

		ent->curstate.renderamt = 180;
		if ( dist <= 100 )
			blend = 180;
		else
			blend = (int)( ( 1.0 - ( dist - 100 ) * 0.0025 ) * 180 );
		blend += RandomLong( -32, 31 );
		break;
	default:
		blend = ent->curstate.renderamt;
		break;
	}

	if ( blend > 255 )
		blend = 255;
	else if ( blend < 0 )
		blend = 0;

	return blend;
}

void R_GetSpriteAxes( cl_entity_t* pEntity, int type, vec_t* forward, vec_t* right, vec_t* up )
{
	int i;
	float angle;
	float sr, cr;
	vec3_t tvec;

	// Automatically roll parallel sprites if requested
	if ( pEntity->angles[ROLL] != 0 && type == SPR_VP_PARALLEL )
		type = SPR_VP_PARALLEL_ORIENTED;

	switch ( type )
	{
	case SPR_FACING_UPRIGHT:
		// generate the sprite's axes, with vup straight up in worldspace, and
		// r_spritedesc.vright perpendicular to modelorg.
		// This will not work if the view direction is very close to straight up or
		// down, because the cross product will be between two nearly parallel
		// vectors and starts to approach an undefined state, so we don't draw if
		// the two vectors are less than 1 degree apart
		for ( i = 0; i < 3; i++ )
			tvec[i] = -modelorg[i];
		VectorNormalize( tvec );
		if ( tvec[2] > 0.999848 || tvec[2] < -0.999848 ) // cos(1 degree) = 0.999848
			return;
		up[0] = 0;
		up[1] = 0;
		up[2] = 1;
		right[0] = tvec[1];
		right[1] = -tvec[0];
		right[2] = 0;
		VectorNormalize( right );
		forward[0] = -right[1];
		forward[1] = right[0];
		forward[2] = 0;
		break;

	case SPR_VP_PARALLEL:
		// generate the sprite's axes, completely parallel to the viewplane. There
		// are no problem situations, because the sprite is always in the same
		// position relative to the viewer
		VectorCopy( vup, up );
		VectorCopy( vright, right );
		VectorCopy( vpn, forward );
		break;

	case SPR_VP_PARALLEL_UPRIGHT:
		if ( vpn[2] > 0.999848 || vpn[2] < -0.999848 )
			return;
		up[0] = 0;
		up[1] = 0;
		up[2] = 1;
		right[0] = vpn[1];
		right[1] = -vpn[0];
		right[2] = 0;
		VectorNormalize( right );
		forward[0] = -right[1];
		forward[1] = right[0];
		forward[2] = 0;
		break;

	case SPR_ORIENTED:
		// generate the sprite's axes, according to the sprite's world orientation
		AngleVectors( pEntity->angles, forward, right, up );
		break;

	case SPR_VP_PARALLEL_ORIENTED:
		// generate the sprite's axes, parallel to the viewplane, but rotated in
		// that plane around the center according to the sprite entity's roll
		// angle. So vpn stays the same, but vright and vup rotate
		angle = (float)( currententity->angles[ROLL] * ( M_PI / 180.0 ) );
		sr = (float)sin( (double)angle );
		cr = (float)cos( (double)angle );

		for ( i = 0; i < 3; i++ )
		{
			forward[i] = vpn[i];
			right[i] = cr * vright[i] + sr * vup[i];
			up[i] = cr * vup[i] - sr * vright[i];
		}
		break;

	default:
		Sys_Error( "R_DrawSprite: Bad sprite type %d", type );
	}
}

void R_SpriteColor( colorVec* pColor, cl_entity_t* pEntity, int alpha )
{
	int a;

	if ( pEntity->curstate.rendermode == kRenderGlow || pEntity->curstate.rendermode == kRenderTransAdd )
		a = alpha;
	else
		a = 256;

	if ( pEntity->curstate.rendercolor.r || pEntity->curstate.rendercolor.g || pEntity->curstate.rendercolor.b )
	{
		pColor->r = ( a * pEntity->curstate.rendercolor.r ) >> 8;
		pColor->g = ( a * pEntity->curstate.rendercolor.g ) >> 8;
		pColor->b = ( a * pEntity->curstate.rendercolor.b ) >> 8;

		if ( filterMode )
		{
			pColor->r = (int)( pColor->r * filterColorRed );
			pColor->g = (int)( pColor->g * filterColorGreen );
			pColor->b = (int)( pColor->b * filterColorBlue );
		}
	}
	else
	{
		pColor->r = pColor->g = pColor->b = ( 255 * a ) >> 8;
	}
}

float* R_GetAttachmentPoint( int entity, int attachment )
{
	cl_entity_t* pEntity = CL_GetEntityIfValid( entity );
	if ( !pEntity )
		return NULL;
	if ( attachment > 0 && attachment <= ARRAYSIZE( pEntity->attachment ) )
		return pEntity->attachment[attachment - 1];

	return pEntity->origin;
}

qboolean CL_ShowTEBlood( void )
{
	return violence_hblood.value || violence_ablood.value;
}

model_t* CL_TentModel( const char* name )
{
	model_t* pModel;

	pModel = Mod_ForName( name, TRUE, FALSE );
	Mod_MarkClient( pModel );

	return pModel;
}

void R_ParticleLine( float* start, float* end, byte r, byte g, byte b, float life )
{
	int color;

	color = R_LookupColor( r, g, b );
	PM_ParticleLine( start, end, color, life, 0 );
}

void R_ParticleBox( float* mins, float* maxs, byte r, byte g, byte b, float life )
{
	int i, color;
	vec3_t mid, tmin, tmax;

	color = R_LookupColor( r, g, b );

	for ( i = 0; i < 3; i++ )
	{
		mid[i] = (float)( ( maxs[i] + mins[i] ) * 0.5 );
		tmax[i] = maxs[i] - mid[i];
		tmin[i] = mins[i] - mid[i];
	}

	PM_DrawBBox( tmin, tmax, mid, color, life );
}

void R_AttachTentToPlayer2( int client, model_t* pModel, float zoffset, float life )
{
	TEMPENTITY* pTemp;
	cl_entity_t* pClient;
	vec3_t position;

	if ( client < 0 || client > cl.maxclients )
	{
		Con_Printf( "Bad client in R_AttachTentToPlayer()!\n" );
		return;
	}

	pClient = &cl_entities[client];

	if ( pClient->curstate.messagenum != cl.parsecount )
		return;

	VectorCopy( pClient->origin, position );
	position[2] += zoffset;

	pTemp = efx.CL_TempEntAllocHigh( position, pModel );
	if ( !pTemp )
	{
		Con_Printf( "No temp ent.\n" );
		return;
	}

	pTemp->entity.curstate.framerate = 1.0f;
	pTemp->tentOffset[0] = 0;
	pTemp->tentOffset[1] = 0;
	pTemp->tentOffset[2] = zoffset;
	pTemp->entity.curstate.rendermode = kRenderNormal;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.baseline.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->clientIndex = (short)client;
	pTemp->flags |= FTENT_PLYRATTACHMENT | FTENT_PERSIST;
	pTemp->die = (float)( life + cl.time );

	if ( pTemp->entity.model->type == mod_sprite )
	{
		pTemp->entity.curstate.framerate = 10;
		pTemp->flags |= FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP;
		pTemp->frameMax = (float)ModelFrameCount( pModel );
	}
	else
	{
		pTemp->frameMax = 0;
	}

	pTemp->entity.curstate.frame = 0;
}

void CL_FxTransform( cl_entity_t* ent, float ( *transform )[4] )
{
	int axis;
	float scale, offset;

	switch ( ent->curstate.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if ( RandomLong( 0, 49 ) == 0 )
		{
			axis = RandomLong( 0, 1 );
			if ( axis == 1 ) // Choose between x & z
				axis = 2;
			VectorScale( transform[axis], RandomFloat( 1.0f, 1.484f ), transform[axis] );
		}
		else if ( RandomLong( 0, 49 ) == 0 )
		{
			RandomLong( 0, 1 );
			offset = RandomFloat( -10, 10 );
			axis = RandomLong( 0, 2 );
			transform[axis][3] += offset;
		}
		break;

	case kRenderFxExplode:
		scale = (float)( ( cl.time - ent->curstate.animtime ) * 10.0 + 1.0 );
		if ( scale > 2.0f )
			scale = 2.0f;
		for ( axis = 0; axis < 3; axis++ )
			transform[axis][1] *= scale;
		break;
	}
}
