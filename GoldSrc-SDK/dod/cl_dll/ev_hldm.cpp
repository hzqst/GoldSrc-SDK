/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_materials.h"
extern "C"
{
#include "pm_shared.h"
}
#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "in_defs.h"
#include "com_model.h"
#include "triangleapi.h"
#include "screenfade.h"
#include "dod_shared.h"
#include "tri.h"
#include "DOD_Camera.h"
#include "vgui/idodviewport.h"
#include "voice_status.h"

#define SND_CHANGE_PITCH (1 << 7)

extern cvar_t* cl_dynamiclights;
extern cvar_t* cl_particlefx;
extern class TeamFortressViewport* gViewPort;

static float fl_TimeMusicLeft = gHUD.m_flTime - 1;
static int tracerCount[32];
static cvar_t* r_decals;

cvar_t* cl_numshotrubble;
TEMPENTITY* g_DeadPlayerModels[64];
int MG42_DownAnims[9] = { 56, 56, 55, 54, 53, 52, 51, 50, 49 };
int MG42_UpAnims[9] = { 47, 47, 46, 45, 44, 43, 42, 41, 40 };
int ThirtyCal_DownAnims[9] = { 56, 56, 55, 54, 53, 52, 51, 50, 49 };
int ThirtyCal_UpAnims[9] = { 47, 47, 46, 45, 44, 43, 42, 41, 40 };
int g_iSwing;
BEAM* pBeam;
BEAM* pBeam2;
float lastOverheatTime;

void EV_CreateTracer( float* start, float* end );
qboolean EV_IsLocal( int idx );
void EV_GetGunPosition( event_args_t* args, float* pos, float* origin );
void EV_MuzzleFlash( int idx, int gunType );
void VectorAngles( const float* forward, float* angles );
cl_entity_t* GetViewEntity();

void EV_BasicPuff( pmtrace_t* trace, float scale );

void EV_WaterHit( pmtrace_t* trace, float )
{
	float time = gEngfuncs.GetClientTime();
	model_t* ripple = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/ripple.spr" ) );
	for ( int i = 0; i < 3; ++i )
	{
		Vector origin( trace->endpos );
		origin.x += gEngfuncs.pfnRandomFloat( -6, 6 );
		origin.x += gEngfuncs.pfnRandomFloat( -6, 6 );
		CDoDParticle* particle = CDoDParticle::Create( origin, Vector( 90, 0, 0 ), ripple,
			30, 150, "dod_particle", false );
		if ( !particle )
			return;
		particle->m_iRendermode = kRenderTransAdd;
		particle->m_flFadeSpeed = 4;
		particle->SetLightFlag( LIGHT_NONE );
		particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		particle->m_flScaleSpeed = 2;
		particle->m_flDieTime = time + 2;
		particle->m_vColor = Vector( 255, 255, 255 );
		particle->m_iPFlags = 64;
	}
	model_t* puff = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/bazookapuff.spr" ) );
	if ( !puff )
		return;
	for ( int i = 0; i < 5; ++i )
	{
		Vector origin( trace->endpos );
		origin.z += i * 5;
		CDoDParticle* particle = CDoDParticle::Create( origin, Vector( 90, 0, 0 ), puff,
			40, 80, "dod_particle", false );
		if ( !particle )
			return;
		particle->m_vVelocity.x = gEngfuncs.pfnRandomFloat( -14, 14 );
		particle->m_vVelocity.y = gEngfuncs.pfnRandomFloat( -14, 14 );
		particle->m_vVelocity.z = gEngfuncs.pfnRandomFloat( 100, 110 );
		particle->m_vAVelocity.z = gEngfuncs.pfnRandomFloat( -2, 2 );
		particle->SetCollisionFlags( TRI_COLLIDEWORLD | TRI_COLLIDEKILL | TRI_WATERTRACE );
		particle->SetLightFlag( LIGHT_NONE );
		particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		particle->m_iRendermode = kRenderTransAdd;
		particle->m_flMass = 1;
		particle->m_flGravity = 0.4f;
		particle->m_vColor = Vector( 255, 255, 255 );
		particle->m_flDieTime = time + 1;
	}
	model_t* splash = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/adrian/water_big.spr" ) );
	if ( !splash )
		return;
	float size = gEngfuncs.pfnRandomFloat( 30, 40 );
	Vector origin( trace->endpos );
	origin.z -= 6;
	CDoDParticle* particle = CDoDParticle::Create( origin, Vector( 0, 0, 0 ), splash,
		size, 150, "dod_particle", false );
	if ( !particle )
		return;
	particle->m_flStretchY = 1.4f;
	particle->m_iRendermode = kRenderTransAdd;
	particle->m_iPFlags = 128;
	particle->SetLightFlag( LIGHT_NONE );
	particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 10;
	particle->m_vColor = Vector( 255, 255, 255 );
	particle->m_flGravity = 0.5f;
	particle->m_flFadeSpeed = 10;
	particle->m_vVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( 110, 130 ) );
}

void CreateFlyingRubble( Vector origin, bool large, float velocityX, float velocityY,
	float velocityZ, float size, int textureType, Vector normal )
{
	if ( !g_pParticleMan || g_RubbleQueue.Full() )
		return;
	const char* name = large ? "sprites/rock_big.spr" : "sprites/rock_small.spr";
	if ( textureType )
		name = "sprites/dirt.spr";
	model_t* model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( name ) );
	if ( !model )
	{
		gEngfuncs.Con_DPrintf( "Couldn't load Sprite: %s\n", name );
		return;
	}
	CDoDParticle* particle = CDoDParticle::Create( origin, normal, model, size, 190, "dod_particle", true );
	if ( !particle )
		return;
	g_RubbleQueue.Add( gHUD.m_flTime );
	particle->m_vVelocity = Vector( velocityX, velocityY, velocityZ );
	particle->m_vAngles.z = gEngfuncs.pfnRandomFloat( 0, 360 );
	particle->SetCollisionFlags( TRI_COLLIDEWORLD | TRI_COLLIDEALL | TRI_WATERTRACE );
	particle->m_iRendermode = kRenderTransTexture;
	particle->SetLightFlag( LIGHT_INTENSITY );
	particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
	particle->m_flScaleSpeed = 0;
	particle->m_flDampingTime = 0;
	particle->m_iFrame = 0;
	particle->m_flMass = 1;
	particle->m_flGravity = 0.5f;
	particle->m_iPFlags = 5120;
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 7;
}

void EV_CreteRubble( pmtrace_t* trace, float scale )
{
	Vector origin;
	VectorMA( trace->endpos, 2, trace->plane.normal, origin );
	if ( !cl_numshotrubble || cl_numshotrubble->value <= 0 )
		return;
	int count = gEngfuncs.pfnRandomLong( 0, (int)cl_numshotrubble->value );
	for ( int i = 0; i < count; ++i )
	{
		Vector normal( trace->plane.normal );
		int velocityY = gEngfuncs.pfnRandomLong( -300, 300 );
		int velocityX = gEngfuncs.pfnRandomLong( -300, 300 );
		CreateFlyingRubble( origin, false, velocityX, velocityY, 75, scale, 0, normal );
	}
}

void EV_PlaySurfaceHitSound( pmtrace_t* trace, int, char type )
{
	const char* format;
	int count = 2;
	switch ( type )
	{
	case 'A':
	case 'D': format = "weapons/hit_sand%d.wav"; break;
	case 'E': format = "weapons/hit_leaves%d.wav"; break;
	case 'G':
	case 'M': format = "weapons/hit_metal%d.wav"; break;
	case 'H': format = "weapons/hit_heavymetal%d.wav"; break;
	case 'K': return;
	case 'N': format = "weapons/hit_snow%d.wav"; break;
	case 'P': format = "weapons/hit_grass%d.wav"; break;
	case 'S': format = "weapons/hit_water%d.wav"; break;
	case 'W': format = "weapons/hit_wood%d.wav"; break;
	case 'Y': format = "weapons/hit_glass%d.wav"; break;
	default: format = "weapons/ric%d.wav"; count = 11; break;
	}
	char sound[44];
	sprintf( sound, format, gEngfuncs.pfnRandomLong( 1, count ) );
	gEngfuncs.pEventAPI->EV_PlaySound( -1, trace->endpos, CHAN_AUTO, sound, 1, ATTN_NORM, 0, PITCH_NORM );
}

void CreateSpark( Vector origin, Vector normal, const char* spriteName )
{
	model_t* model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( spriteName ) );
	if ( !model )
	{
		gEngfuncs.Con_DPrintf( "Couldn't load Sprite: %s\n", spriteName );
		return;
	}
	Vector angles;
	VectorAngles( normal, angles );
	CDoDParticle* particle = CDoDParticle::Create( origin, Vector( 0, 0, 1 ), model,
		2.5f, 255, "dod_particle", true );
	if ( !particle )
		return;
	particle->m_flGravity = 0;
	particle->m_vVelocity = Vector( 0, 0, 0 );
	particle->m_vAngles = angles;
	particle->m_iFramerate = 0;
	particle->m_flDieTime = 0.05f;
	particle->m_iRendermode = kRenderTransAdd;
	particle->m_flSize = 2.5f;
	particle->m_vAVelocity = Vector( 0, 0, 0 );
	particle->m_iPFlags = 64;
	particle->SetLightFlag( LIGHT_NONE );
	particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
}

void EV_MetalHit( pmtrace_t* trace )
{
	Vector origin( trace->endpos );
	Vector normal( trace->plane.normal );
	int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/metalspark.spr" );
	int count = gEngfuncs.pfnRandomLong( 4, 6 );
	float length = gEngfuncs.pfnRandomFloat( 12, 14 );
	if ( !sprite )
		return;
	Vector sparkNormal, direction, end;
	float increment = 0;
	for ( int i = 0; i < count; ++i )
	{
		if ( i == 0 )
		{
			sparkNormal.x = normal.x + gEngfuncs.pfnRandomFloat( -0.2f, 0.2f );
			sparkNormal.y = normal.y + gEngfuncs.pfnRandomFloat( -0.2f, 0.2f );
			sparkNormal.z = normal.z + gEngfuncs.pfnRandomFloat( -0.2f, 0.2f );
			VectorMA( origin, length, sparkNormal, end );
			direction = sparkNormal;
			length *= 0.75f;
			increment = length / count;
		}
		else
		{
			direction.x = sparkNormal.x + gEngfuncs.pfnRandomFloat( -0.2f, 0.2f );
			direction.y = sparkNormal.y + gEngfuncs.pfnRandomFloat( -0.2f, 0.2f );
			direction.z = sparkNormal.z + gEngfuncs.pfnRandomFloat( -0.2f, 0.2f );
			length -= increment;
			VectorNormalize( direction );
			VectorMA( origin, length + length, direction, end );
		}
		gEngfuncs.pEfxAPI->R_BeamPoints( origin, end, sprite, 0.05f, 0.8f, 0,
			255, 0, 0, 0, 250, 220, 192 );
	}
	CreateSpark( origin, normal, "sprites/smallspark.spr" );
	if ( cl_dynamiclights->value > 0 )
	{
		dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );
		light->origin = origin;
		light->radius = 55;
		light->color.r = 240;
		light->color.g = 240;
		light->color.b = 180;
		light->die = gHUD.m_flTime + 0.1;
		light->decay = 5;
	}
}

void EV_SnowHit( pmtrace_t* trace, float scale )
{
	model_t* model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/bazookapuff.spr" ) );
	if ( !model )
		return;
	for ( int i = 0; i < 5; ++i )
	{
		CDoDParticle* particle = CDoDParticle::Create( Vector( trace->endpos ), Vector( 90, 0, 0 ),
			model, 10 * scale, 140, "dod_particle", false );
		if ( !particle )
			return;
		particle->m_vVelocity.x = gEngfuncs.pfnRandomFloat( -10, 10 );
		particle->m_vVelocity.y = gEngfuncs.pfnRandomFloat( -10, 10 );
		particle->m_vVelocity.z = gEngfuncs.pfnRandomFloat( 40, 80 );
		particle->m_vAVelocity.z = gEngfuncs.pfnRandomFloat( -5, 5 );
		particle->m_iRendermode = kRenderTransAdd;
		particle->m_flMass = 1;
		particle->SetLightFlag( LIGHT_COLOR );
		particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		particle->m_flGravity = 0.4f;
		particle->m_vColor = Vector( 255, 255, 255 );
		particle->SetCollisionFlags( TRI_COLLIDEWORLD | TRI_COLLIDEKILL | TRI_WATERTRACE );
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 1;
	}
}

void EV_LeavesHit( pmtrace_t* trace, float scale )
{
	model_t* foliage = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/debris_foliage.spr" ) );
	model_t* leaf1 = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/debris_leaf1.spr" ) );
	model_t* leaf2 = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/debris_leaf2.spr" ) );
	if ( !leaf1 || !foliage || !leaf2 )
		return;
	int count = gEngfuncs.pfnRandomLong( 2, 3 );
	for ( int i = 0; i < count; ++i )
	{
		CDoDParticle* particle = CDoDParticle::Create( Vector( trace->endpos ), Vector( 0, 0, 0 ),
			foliage, 4 * scale, 255, "dod_particle", false );
		if ( !particle )
			return;
		Vector normal;
		normal.x = trace->plane.normal[0] + gEngfuncs.pfnRandomFloat( -1, 1 );
		normal.y = trace->plane.normal[1] + gEngfuncs.pfnRandomFloat( -1, 1 );
		normal.z = trace->plane.normal[2] + gEngfuncs.pfnRandomFloat( -1, 1 );
		Vector end;
		VectorMA( trace->endpos, 100, normal, end );
		particle->m_vVelocity = Vector( trace->endpos ) - end;
		particle->m_vAVelocity.z = 1000;
		particle->m_vAngles.z = gEngfuncs.pfnRandomFloat( 0, 90 );
		particle->SetCollisionFlags( TRI_COLLIDEWORLD | TRI_SPIRAL );
		particle->SetLightFlag( LIGHT_COLOR );
		particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		particle->m_iRendermode = kRenderTransAlpha;
		particle->m_flGravity = 0.2f;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 2;
		particle->m_flDampingTime = gEngfuncs.GetClientTime() + 0.1;
		particle->AddGlobalWind();
	}
	count = gEngfuncs.pfnRandomLong( 0, 1 );
	for ( int i = 0; i < count; ++i )
	{
		int leaf = gEngfuncs.pfnRandomLong( 0, 1 );
		CDoDParticle* particle = CDoDParticle::Create( Vector( trace->endpos ), Vector( 0, 0, 0 ),
			leaf ? leaf2 : leaf1, 8 * scale, 255, "dod_particle", false );
		if ( !particle )
			return;
		Vector normal;
		normal.x = trace->plane.normal[0] + gEngfuncs.pfnRandomFloat( -1, 1 );
		normal.y = trace->plane.normal[1] + gEngfuncs.pfnRandomFloat( -1, 1 );
		normal.z = trace->plane.normal[2] + gEngfuncs.pfnRandomFloat( -1, 1 );
		Vector end;
		VectorMA( trace->endpos, 100, normal, end );
		particle->m_vVelocity = Vector( trace->endpos ) - end;
		particle->m_vAVelocity.x = gEngfuncs.pfnRandomFloat( 0, 90 );
		particle->m_vAVelocity.y = gEngfuncs.pfnRandomFloat( 0, 90 );
		particle->m_vAVelocity.z = gEngfuncs.pfnRandomFloat( 0, 90 );
		particle->m_vAngles.x = gEngfuncs.pfnRandomFloat( 0, 90 );
		particle->m_vAngles.y = gEngfuncs.pfnRandomFloat( 0, 90 );
		particle->m_vAngles.z = gEngfuncs.pfnRandomFloat( 0, 90 );
		particle->SetCollisionFlags( TRI_COLLIDEWORLD | TRI_SPIRAL );
		particle->SetLightFlag( LIGHT_COLOR );
		particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		particle->m_iRendermode = kRenderTransAlpha;
		particle->m_flGravity = 0.2f;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 2;
		particle->m_flDampingTime = gEngfuncs.GetClientTime() + 0.1;
		particle->m_iPFlags = 64;
		particle->AddGlobalWind();
	}
}

char EV_FindHitTexture( float* start, float* end )
{
	pmtrace_t* trace = gEngfuncs.PM_TraceLine( start, end, 0, 2, -1 );
	const char* texture = gEngfuncs.pEventAPI->EV_TraceTexture( trace->ent, start, end );
	if ( !texture )
		return CHAR_TEX_CONCRETE;
	char name[64];
	strcpy( name, texture );
	char* typeName = name;
	if ( *typeName == '+' || *typeName == '-' )
		typeName += 2;
	if ( *typeName == '!' || *typeName == '{' || *typeName == '~' || *typeName == ' ' )
		++typeName;
	char buffer[76];
	strcpy( buffer, typeName );
	buffer[12] = 0;
	return PM_FindTextureType( buffer );
}

char EV_TexNameToType( char* texture )
{
	if ( !texture )
		return CHAR_TEX_CONCRETE;
	char name[64];
	strcpy( name, texture );
	char* typeName = name;
	if ( *typeName == '+' || *typeName == '-' )
		typeName += 2;
	if ( *typeName == '!' || *typeName == '{' || *typeName == '~' || *typeName == ' ' )
		++typeName;
	char buffer[76];
	strcpy( buffer, typeName );
	buffer[12] = 0;
	return PM_FindTextureType( buffer );
}

int EV_GetWeaponBody()
{
	int team = g_iTeamNumber;
	if ( g_iUser1 == OBS_IN_EYE )
		team = gEngfuncs.GetEntityByIndex( g_iUser2 )->curstate.team;
	return team == 1 ? gHUD.m_bBritish : 2;
}

void EV_ResetAnimationEvents( int index )
{
	cl_entity_t* ent = gEngfuncs.GetEntityByIndex( index );
	ent->baseline.iuser1 = 0;
	ent->baseline.iuser2 = 0;
	ent->baseline.iuser3 = 0;
	ent->baseline.iuser4 = 0;
}

extern "C" void EV_RoundReset( event_args_t* )
{
	if ( !r_decals )
		r_decals = gEngfuncs.pfnGetCvarPointer( "r_decals" );
	if ( r_decals )
	{
		int count = (int)r_decals->value;
		for ( int i = 0; i < count; i++ )
			gEngfuncs.pEfxAPI->R_DecalRemoveAll( i );
	}
	if ( g_DeadPlayerModels[0] )
	{
		for ( int i = 0; i < 64; i++ )
		{
			if ( g_DeadPlayerModels[i] )
			{
				g_DeadPlayerModels[i]->die = 0;
				g_DeadPlayerModels[i] = NULL;
			}
		}
	}
}

void RemoveBody( TEMPENTITY* ent, float frametime, float currentTime )
{
	float stay = gHUD.cl_corpsestay ? gHUD.cl_corpsestay->value : 0;
	if ( currentTime >= ent->entity.curstate.fuser2 + stay )
		ent->entity.origin.z -= frametime * 5;
}

void HitBody( TEMPENTITY* ent, pmtrace_t* trace )
{
	if ( trace->plane.normal[2] > 0 )
		ent->flags |= 0x00800000;
}

void CreateCorpse( Vector origin, Vector angles, const char* model, float animTime, int sequence, int body )
{
	float time = gEngfuncs.GetClientTime();
	gEngfuncs.pEventAPI->EV_FindModelIndex( model );
	int modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( model );
	Vector velocity( 0, 0, 0 );
	TEMPENTITY* ent = gEngfuncs.pEfxAPI->R_TempModel( origin, velocity, angles, 100, modelIndex, 0 );
	if ( !ent )
		return;
	ent->flags |= FTENT_CLIENTCUSTOM | FTENT_PERSIST | FTENT_SPRANIMATE | FTENT_COLLIDEWORLD | 0x400000;
	ent->entity.curstate.framerate = 1;
	ent->frameMax = 255;
	ent->entity.curstate.animtime = animTime;
	ent->entity.curstate.frame = 0;
	ent->entity.curstate.renderamt = 255;
	ent->entity.curstate.sequence = sequence;
	ent->entity.curstate.body = body;
	ent->entity.curstate.fuser1 = gHUD.m_flTime + 1;
	ent->entity.curstate.fuser2 = gHUD.m_flTime + 1;
	float stay = gHUD.cl_corpsestay ? gHUD.cl_corpsestay->value : 0;
	ent->callback = RemoveBody;
	ent->hitcallback = HitBody;
	ent->bounceFactor = 0;
	ent->die = ent->entity.curstate.fuser1 + stay + 5;
	bool replaced = false;
	for ( int i = 0; i < 64; i++ )
	{
		TEMPENTITY* old = g_DeadPlayerModels[i];
		if ( !old || old == ent )
		{
			g_DeadPlayerModels[i] = ent;
			return;
		}
		if ( time > old->die )
		{
			old->die = 0;
			replaced = true;
			g_DeadPlayerModels[i] = ent;
		}
	}
	if ( !replaced )
		ent->die = ent->entity.curstate.fuser2 + 5;
}

int EV_HLDM_CheckTracer( int idx, float*, float* end, float*, float*,
	int, int tracerFreq, int* count, float* tracerOrigin )
{
	if ( idx > 0 )
		gEngfuncs.GetMaxClients();
	if ( tracerFreq && ( *count )++ % tracerFreq == 0 )
	{
		EV_CreateTracer( tracerOrigin, end );
		return tracerFreq != 1;
	}
	return FALSE;
}

float EV_HLDM_WaterHeight( Vector position, float minz, float maxz )
{
	Vector mid( position.x, position.y, minz );
	if ( gEngfuncs.PM_PointContents( mid, NULL ) != CONTENTS_WATER )
		return minz;
	mid.z = maxz;
	if ( gEngfuncs.PM_PointContents( mid, NULL ) == CONTENTS_WATER )
		return maxz;
	float low = minz;
	while ( maxz - low > 1 )
	{
		mid.z = low + ( maxz - low ) * 0.5;
		if ( gEngfuncs.PM_PointContents( mid, NULL ) == CONTENTS_WATER )
			low = mid.z;
		else
			maxz = mid.z;
	}
	return mid.z;
}

void EV_HLDM_BubbleTrails( Vector from, Vector to, int count )
{
	float height = EV_HLDM_WaterHeight( from, from.z, from.z + 256 ) - from.z;
	if ( height < 8 )
	{
		float top = EV_HLDM_WaterHeight( to, to.z, to.z + 256 );
		float depth = top - to.z;
		if ( depth < 8 )
			return;
		height = depth + to.z - from.z;
	}
	int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/bubble.spr" );
	if ( count > 255 )
		count = 255;
	gEngfuncs.pEfxAPI->R_BubbleTrail( from, to, height, model, count, 8 );
}

int EV_TFC_IsAllyTeam( int team1, int team2 )
{
	return team1 == team2;
}

void EV_BasicPuff( pmtrace_t* trace, float )
{
	for ( int i = 0; i < 4; i++ )
	{
		Vector color( 175, 175, 175 );
		Vector velocity( trace->plane.normal );
		float offset = (float)gEngfuncs.pfnRandomLong( 1, 3 );
		Vector origin = Vector( trace->endpos ) + Vector( trace->plane.normal ) * offset;
		CreateDebrisWallPuff( origin, velocity, color, i );
	}
}

void EV_DirtHit( pmtrace_t* trace, float scale )
{
	model_t* sprite = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/debris_dirt3.spr" ) );
	if ( !sprite )
		return;
	gEngfuncs.pfnRandomLong( 2, 3 );
	for ( int i = 0; i < 3; i++ )
	{
		CDoDParticle* p = CDoDParticle::Create( Vector( trace->endpos ), Vector( 0, 0, 0 ),
			sprite, scale * 4, 160, "dod_particle", false );
		if ( !p )
			return;
		p->m_flStretchX = 2;
		p->m_flStretchY = 6;
		Vector dir;
		dir.x = gEngfuncs.pfnRandomFloat( -1, 1 );
		dir.y = gEngfuncs.pfnRandomFloat( -1, 1 );
		dir.z = 0;
		dir = dir.Normalize();
		p->m_vVelocity = Vector( 0, 0, i == 0 ? 100 : 90 );
		if ( i == 1 )
			p->m_vVelocity = p->m_vVelocity + dir * 20;
		else if ( i == 2 )
			p->m_vVelocity = p->m_vVelocity + dir * -20;
		p->SetLightFlag( LIGHT_NONE );
		p->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		p->m_iPFlags = 128;
		if ( i == 0 )
			p->m_flStretchY = 7;
		p->m_iRendermode = kRenderTransAdd;
		p->m_vColor = Vector( 83, 72, 55 );
		float time = gEngfuncs.GetClientTime();
		p->m_flGravity = 0.6f;
		p->m_flDieTime = time + 2;
	}
}

void EV_SandHit( pmtrace_t* trace, float scale )
{
	model_t* sprite = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/debris_dirt3.spr" ) );
	if ( !sprite )
		return;
	gEngfuncs.pfnRandomLong( 2, 3 );
	for ( int i = 0; i < 3; i++ )
	{
		CDoDParticle* p = CDoDParticle::Create( Vector( trace->endpos ), Vector( 0, 0, 0 ),
			sprite, scale * 4, 140, "dod_particle", false );
		if ( !p )
			continue;
		p->m_flStretchX = 2;
		p->m_flStretchY = 6;
		Vector dir;
		dir.x = gEngfuncs.pfnRandomFloat( -1, 1 );
		dir.y = gEngfuncs.pfnRandomFloat( -1, 1 );
		dir.z = 0;
		dir = dir.Normalize();
		p->m_vVelocity = Vector( 0, 0, i == 0 ? 100 : 90 );
		if ( i == 1 )
			p->m_vVelocity = p->m_vVelocity + dir * 20;
		else if ( i == 2 )
			p->m_vVelocity = p->m_vVelocity + dir * -20;
		p->SetLightFlag( LIGHT_NONE );
		p->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		p->m_iPFlags = 128;
		if ( i == 0 )
			p->m_flStretchY = 7;
		p->m_iRendermode = kRenderTransAdd;
		p->m_vColor = Vector( 185, 177, 149 );
		float time = gEngfuncs.GetClientTime();
		p->m_flGravity = 0.6f;
		p->m_flDieTime = time + 2;
	}
}

void EV_GrassHit( pmtrace_t* trace, float scale )
{
	model_t* sprite = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/debris_grass1.spr" ) );
	if ( !sprite )
		return;
	int count = gEngfuncs.pfnRandomLong( 4, 6 );
	for ( int i = 0; i < count; i++ )
	{
		CDoDParticle* p = CDoDParticle::Create( Vector( trace->endpos ), Vector( 0, 0, 0 ),
			sprite, scale * 3, 255, "dod_particle", false );
		if ( !p )
			continue;
		p->m_vVelocity.x = gEngfuncs.pfnRandomFloat( -20, 20 );
		p->m_vVelocity.y = gEngfuncs.pfnRandomFloat( -20, 20 );
		p->m_vVelocity.z = gEngfuncs.pfnRandomFloat( 150, 180 );
		p->m_vAVelocity.z = 1000;
		p->m_vAngles.z = gEngfuncs.pfnRandomFloat( 0, 90 );
		p->SetCollisionFlags( TRI_COLLIDEWORLD | TRI_COLLIDEKILL );
		p->m_iRendermode = kRenderTransAlpha;
		p->SetLightFlag( LIGHT_COLOR );
		p->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		float time = gEngfuncs.GetClientTime();
		p->m_flGravity = 0.7f;
		p->m_flDieTime = time + 2;
	}
}

void EV_WoodChips( pmtrace_t* trace, float scale )
{
	model_t* sprite = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/debris_wood1.spr" ) );
	if ( !sprite )
		return;
	int count = gEngfuncs.pfnRandomLong( 1, 2 );
	for ( int i = 0; i < count; i++ )
	{
		CDoDParticle* p = CDoDParticle::Create( Vector( trace->endpos ), Vector( 0, 0, 0 ),
			sprite, scale * 4, 255, "dod_particle", false );
		if ( !p )
			return;
		Vector normal;
		normal.x = trace->plane.normal[0] + gEngfuncs.pfnRandomFloat( -1, 1 );
		normal.y = trace->plane.normal[1] + gEngfuncs.pfnRandomFloat( -1, 1 );
		normal.z = trace->plane.normal[2] + gEngfuncs.pfnRandomFloat( -1, 1 );
		Vector end = Vector( trace->endpos ) + normal * 200;
		p->m_vVelocity = Vector( trace->endpos ) - end;
		p->m_vAVelocity.z = 1000;
		p->m_vAngles.z = gEngfuncs.pfnRandomFloat( 0, 90 );
		p->SetCollisionFlags( TRI_COLLIDEWORLD );
		p->m_iRendermode = kRenderTransAlpha;
		p->SetLightFlag( LIGHT_COLOR );
		p->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		float time = gEngfuncs.GetClientTime();
		p->m_flGravity = 0.6f;
		p->m_flDieTime = time + 2;
	}
	EV_BasicPuff( trace, scale * 0.5 );
}

void EV_GlassShards( pmtrace_t* trace, float scale )
{
	model_t* sprite = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/debris_glass.spr" ) );
	if ( !sprite )
		return;
	int count = gEngfuncs.pfnRandomLong( 3, 4 );
	for ( int i = 0; i < count; i++ )
	{
		CDoDParticle* p = CDoDParticle::Create( Vector( trace->endpos ), Vector( 0, 0, 0 ),
			sprite, scale + scale, 255, "dod_particle", false );
		if ( !p )
			return;
		Vector normal;
		normal.x = trace->plane.normal[0] + gEngfuncs.pfnRandomFloat( -1, 1 );
		normal.y = trace->plane.normal[1] + gEngfuncs.pfnRandomFloat( -1, 1 );
		normal.z = trace->plane.normal[2] + gEngfuncs.pfnRandomFloat( -1, 1 );
		Vector end = Vector( trace->endpos ) + normal * 400;
		p->m_vVelocity = Vector( trace->endpos ) - end;
		p->m_vAVelocity.z = 1000;
		p->m_vAngles.z = gEngfuncs.pfnRandomFloat( 0, 90 );
		p->SetCollisionFlags( TRI_COLLIDEWORLD );
		p->m_iRendermode = kRenderTransAlpha;
		p->SetLightFlag( LIGHT_COLOR );
		p->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		float time = gEngfuncs.GetClientTime();
		p->m_flGravity = 0.6f;
		p->m_flDieTime = time + 2;
	}
}

void EV_TileHit( pmtrace_t* trace, float scale )
{
	model_t* sprite = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/debris_tile1.spr" ) );
	if ( !sprite )
		return;
	int count = gEngfuncs.pfnRandomLong( 3, 4 );
	for ( int i = 0; i < count; i++ )
	{
		CDoDParticle* p = CDoDParticle::Create( Vector( trace->endpos ), Vector( 0, 0, 0 ),
			sprite, scale * 1.5, 255, "dod_particle", false );
		if ( !p )
			return;
		Vector normal;
		normal.x = trace->plane.normal[0] + gEngfuncs.pfnRandomFloat( -1, 1 );
		normal.y = trace->plane.normal[1] + gEngfuncs.pfnRandomFloat( -1, 1 );
		normal.z = trace->plane.normal[2] + gEngfuncs.pfnRandomFloat( -1, 1 );
		Vector end = Vector( trace->endpos ) + normal * 400;
		p->m_vVelocity = Vector( trace->endpos ) - end;
		p->m_vAVelocity.z = 1000;
		p->m_vAngles.z = gEngfuncs.pfnRandomFloat( 0, 90 );
		p->SetCollisionFlags( TRI_COLLIDEWORLD );
		p->m_iRendermode = kRenderTransAlpha;
		p->SetLightFlag( LIGHT_COLOR );
		p->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		float time = gEngfuncs.GetClientTime();
		p->m_flGravity = 0.6f;
		p->m_flDieTime = time + 2;
	}
	EV_BasicPuff( trace, scale * 0.5 );
}

extern "C" void EV_Bubbles( event_args_t* args )
{
	Vector mins( args->origin );
	Vector maxs( args->angles );
	gEngfuncs.pEfxAPI->R_Bubbles( mins, maxs, args->fparam1, args->iparam1, args->iparam2, args->fparam2 );
}

extern "C" void EV_BubbleTrails( event_args_t* args )
{
	Vector from( args->origin );
	Vector to( args->angles );
	gEngfuncs.pEfxAPI->R_BubbleTrail( from, to, args->fparam1, args->iparam1, args->iparam2, args->fparam2 );
}

extern "C" void EV_BulletTracers( event_args_t* args )
{
	Vector from( args->origin );
	Vector to( args->angles );
	gEngfuncs.pEfxAPI->R_TracerEffect( from, to );
}

extern "C" void EV_BloodSprite( event_args_t* args )
{
	Vector origin( args->origin );
	gEngfuncs.pEfxAPI->R_BloodSprite( origin, args->iparam2, args->iparam1, (int)args->fparam1, args->fparam2 );
}

extern "C" void EV_BloodStream( event_args_t* args )
{
	Vector origin( args->angles );
	Vector velocity( 0, 0, 0 );
	int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/shot-dust.spr" );
	velocity.z += 4;
	origin.z += 2;
	float scale = (float)gEngfuncs.pfnRandomLong( 2, 4 );
	TEMPENTITY* ent = gEngfuncs.pEfxAPI->R_TempSprite( origin, velocity, scale, model,
		kRenderTransAlpha, 0, 0.35f, 1, FTENT_FADEOUT | FTENT_SPRANIMATE );
	if ( !ent )
		return;
	ent->fadeSpeed = 4;
	ent->entity.curstate.renderamt = 800;
	ent->entity.curstate.framerate = 15;
	ent->entity.curstate.rendercolor.r = 180;
	ent->entity.curstate.rendercolor.g = 176;
	ent->entity.curstate.rendercolor.b = 148;
	model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/blood-narrow.spr" );
	velocity.z += 4;
	origin.z += 2;
	scale = (float)gEngfuncs.pfnRandomLong( 0, 0 );
	ent = gEngfuncs.pEfxAPI->R_TempSprite( origin, velocity, scale, model,
		kRenderTransAlpha, 0, 0.35f, 1, FTENT_FADEOUT | FTENT_SPRANIMATE );
	if ( !ent )
		return;
	ent->fadeSpeed = 4;
	ent->entity.curstate.framerate = 15;
	ent->entity.curstate.renderamt = 800;
	ent->entity.curstate.rendercolor.r = 75;
	ent->entity.curstate.rendercolor.g = 0;
	ent->entity.curstate.rendercolor.b = 0;
}

extern "C" void EV_Smoke( event_args_t* args )
{
	Vector angles( 0, 270, 0 );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	CreateExplosionSmokeInside( Vector( args->origin ) );
}

extern "C" void EV_Pain( event_args_t* args )
{
	int idx = args->entindex;
	Vector origin( args->origin );
	int sound = gEngfuncs.pfnRandomLong( 1, 11 );
	char name[32];
	sprintf( name, "player/damage%d.wav", sound );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_VOICE, name, 1, ATTN_IDLE, 0, PITCH_NORM );
}

extern "C" void EV_BodyDamage( event_args_t* args )
{
	int idx = args->entindex;
	Vector origin( args->origin );
	if ( args->iparam1 == 0 )
	{
		float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, "player/helmet_hit.wav",
			volume, ATTN_NORM, 0, PITCH_NORM );
	}
}

extern "C" void EV_SparkShower( event_args_t* args )
{
	Vector origin( args->origin );
	gEngfuncs.pEfxAPI->R_SparkShower( origin );
}

extern "C" void EV_PlayWhizz( event_args_t* args )
{
	if ( EV_IsLocal( args->entindex ) )
		gEngfuncs.pfnPlaySoundByName( "player/whizz.wav", 1 );
}

extern "C" void EV_DoDCamera( event_args_t* args )
{
	int camera = args->iparam1;
	int widescreen = args->bparam1;
	if ( !EV_IsLocal( args->entindex ) )
		return;
	if ( camera == 1 )
		gHUD.m_Scope.SetScope( WEAPON_BINOCULARS );
	else if ( camera == 2 )
		gHUD.m_Scope.SetScope( WEAPON_SPRING );
	else
	{
		gHUD.m_Scope.SetScope( 0 );
		if ( gViewPort )
			gDoDViewPortInterface->setWidescreenVisible( widescreen != 0 );
	}
}

extern "C" void EV_TrainPitchAdjust( event_args_t* args )
{
	int idx = args->entindex;
	int stop = args->bparam1;
	Vector origin( args->origin );
	unsigned short parameters = (unsigned short)args->iparam1;
	int sound = parameters >> 12 & 7;
	const char* name;
	switch ( sound )
	{
	case 1: name = "plats/ttrain1.wav"; break;
	case 2: name = "plats/ttrain2.wav"; break;
	case 3: name = "plats/ttrain3.wav"; break;
	case 4: name = "plats/ttrain4.wav"; break;
	case 5: name = "plats/ttrain6.wav"; break;
	case 6: name = "plats/ttrain7.wav"; break;
	default: return;
	}
	if ( stop )
		gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, name );
	else
	{
		int pitch = ( parameters >> 6 & 0x3f ) * 10;
		float volume = ( parameters & 0x3f ) / 40.0;
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, name, volume,
			ATTN_NORM, SND_CHANGE_PITCH, pitch );
	}
}

void EV_HLDM_DoDSurfaceFX( pmtrace_t* trace, int bulletType, char type )
{
	float scale = 1.5f;
	switch ( bulletType )
	{
	case BULLET_PLAYER_COLT:
	case BULLET_PLAYER_LUGER:
	case BULLET_PLAYER_WEBLEY:
		scale = 1.2f;
		break;
	case BULLET_PLAYER_GARAND:
	case BULLET_PLAYER_SCOPEDKAR:
	case BULLET_PLAYER_THOMPSON:
	case BULLET_PLAYER_SPRING:
	case BULLET_PLAYER_KAR:
	case BULLET_PLAYER_MP40:
	case BULLET_PLAYER_M1CARBINE:
	case BULLET_PLAYER_GREASEGUN:
	case BULLET_PLAYER_K43:
	case BULLET_PLAYER_ENFIELD:
	case BULLET_PLAYER_STEN:
		scale = 1.6f;
		break;
	case BULLET_PLAYER_MP44:
	case BULLET_PLAYER_BAR:
	case BULLET_PLAYER_MG42:
	case BULLET_PLAYER_MG34:
	case BULLET_PLAYER_30CAL:
	case BULLET_PLAYER_FG42:
	case BULLET_PLAYER_BREN:
		scale = 2.2f;
		break;
	}
	switch ( type )
	{
	case 'A': EV_SandHit( trace, scale ); return;
	case 'B':
	case 'C':
	case 'R': EV_CreteRubble( trace, scale ); break;
	case 'D': EV_DirtHit( trace, scale ); return;
	case 'E': EV_LeavesHit( trace, scale ); return;
	case 'G':
	case 'H':
	case 'M': EV_MetalHit( trace ); return;
	case 'K': return;
	case 'L': EV_CreteRubble( trace, scale ); EV_CreteRubble( trace, scale ); break;
	case 'N': EV_SnowHit( trace, scale ); return;
	case 'P': EV_GrassHit( trace, scale ); return;
	case 'S': EV_WaterHit( trace, scale ); return;
	case 'T': EV_TileHit( trace, scale ); return;
	case 'W': EV_WoodChips( trace, scale ); return;
	case 'Y': EV_GlassShards( trace, scale ); return;
	}
	EV_BasicPuff( trace, scale );
}

int EV_HLDM_WaterEntryPoint( pmtrace_t* trace, float* source, float* result )
{
	int contents = gEngfuncs.PM_PointContents( trace->endpos, NULL );
	if ( contents == gEngfuncs.PM_PointContents( source, NULL ) )
		return 0;
	Vector start( source );
	Vector end( trace->endpos );
	Vector direction = end - start;
	while ( direction.Length() > 4 )
	{
		direction = direction * 0.5f;
		Vector mid = start + direction;
		int startContents = gEngfuncs.PM_PointContents( start, NULL );
		if ( startContents == gEngfuncs.PM_PointContents( mid, NULL ) )
			start = mid;
		else
			end = mid;
		direction = end - start;
	}
	VectorCopy( start, result );
	return 1;
}

float EV_HLDM_PlayTextureSound( int, pmtrace_t* trace, float* source, float* end, int )
{
	int entity = gEngfuncs.pEventAPI->EV_IndexFromTrace( trace );
	char type = CHAR_TEX_CONCRETE;
	if ( entity > 0 )
		gEngfuncs.GetMaxClients();
	else if ( entity == 0 )
	{
		const char* texture = gEngfuncs.pEventAPI->EV_TraceTexture( trace->ent, source, end );
		type = EV_TexNameToType( (char*)texture );
	}
	const char* sounds[4];
	float volume = 0.9f;
	float barVolume;
	int count;
	switch ( type )
	{
	case 'D':
		barVolume = 0.1f; count = 3;
		sounds[0] = "player/pl_dirt1.wav"; sounds[1] = "player/pl_dirt2.wav"; sounds[2] = "player/pl_dirt3.wav";
		break;
	case 'G':
		barVolume = 0.5f; count = 4;
		sounds[0] = "player/pl_grate1.wav"; sounds[1] = "player/pl_duct1.wav";
		sounds[2] = "player/pl_duct2.wav"; sounds[3] = "player/pl_grate4.wav";
		break;
	case 'H':
	case 'M':
		barVolume = 0.3f; count = 2;
		sounds[0] = "player/pl_metal1.wav"; sounds[1] = "player/pl_metal2.wav";
		break;
	case 'K': return 0;
	case 'S':
		barVolume = 0; count = 4;
		sounds[0] = "player/pl_slosh1.wav"; sounds[1] = "player/pl_slosh3.wav";
		sounds[2] = "player/pl_slosh2.wav"; sounds[3] = "player/pl_slosh4.wav";
		break;
	case 'T':
		barVolume = 0.2f; count = 4; volume = 0.8f;
		sounds[0] = "player/pl_tile1.wav"; sounds[1] = "player/pl_tile3.wav";
		sounds[2] = "player/pl_tile2.wav"; sounds[3] = "player/pl_tile4.wav";
		break;
	case 'W':
		barVolume = 0.2f; count = 3;
		sounds[0] = "debris/wood1.wav"; sounds[1] = "debris/wood2.wav"; sounds[2] = "debris/wood3.wav";
		break;
	case 'Y':
		barVolume = 0.2f; count = 3; volume = 0.8f;
		sounds[0] = "debris/glass1.wav"; sounds[1] = "debris/glass2.wav"; sounds[2] = "debris/glass3.wav";
		break;
	default:
		barVolume = 0.6f; count = 2;
		sounds[0] = "player/pl_step1.wav"; sounds[1] = "player/pl_step2.wav";
		break;
	}
	int pitch = gEngfuncs.pfnRandomLong( 0, 15 ) + 96;
	int sound = gEngfuncs.pfnRandomLong( 0, count - 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( 0, trace->endpos, CHAN_STATIC, sounds[sound], volume, ATTN_NORM, 0, pitch );
	return barVolume;
}

void EV_HLDM_GunshotDecalTrace( pmtrace_t* trace, char*, float* source, float* end, int bulletType )
{
	float depth = EV_HLDM_WaterHeight( Vector( source ), source[2], source[2] + 256 ) - source[2];
	if ( depth < 8 )
	{
		float endDepth = EV_HLDM_WaterHeight( Vector( trace->endpos ), trace->endpos[2], trace->endpos[2] + 256 ) - trace->endpos[2];
		depth = endDepth < 8 ? 0 : endDepth + trace->endpos[2] - source[2];
	}
	int entity = gEngfuncs.pEventAPI->EV_IndexFromTrace( trace );
	char type = 'F';
	if ( entity <= 0 || entity > gEngfuncs.GetMaxClients() )
	{
		pmtrace_t trace2;
		gEngfuncs.pEventAPI->EV_PlayerTrace( trace->endpos, end, PM_STUDIO_BOX, -1, &trace2 );
		const char* texture = gEngfuncs.pEventAPI->EV_TraceTexture( trace2.ent, trace->endpos, end );
		type = 'K';
		if ( texture )
		{
			type = EV_TexNameToType( (char*)texture );
			if ( depth != 0 )
			{
				type = 'S';
				Vector location;
				if ( EV_HLDM_WaterEntryPoint( trace, source, location ) )
					VectorCopy( location, trace->endpos );
			}
		}
	}
	physent_t* ent = gEngfuncs.pEventAPI->EV_GetPhysent( trace->ent );
	static char decalname[32];
	if ( ent->classnumber == 1 || type == 'Y' )
	{
		type = 'Y';
		sprintf( decalname, "{break%i", gEngfuncs.pfnRandomLong( 0, 2 ) + 1 );
	}
	else
	{
		switch ( type )
		{
		case 'A': case 'D': case 'E': case 'F': case 'G': case 'H': case 'K':
		case 'L': case 'M': case 'N': case 'P': case 'R': case 'S':
			decalname[0] = 0; break;
		case 'B': case 'C': sprintf( decalname, "{cement%i", gEngfuncs.pfnRandomLong( 1, 2 ) ); break;
		case 'T': sprintf( decalname, "{crack%i", gEngfuncs.pfnRandomLong( 1, 4 ) ); break;
		case 'W': case 'Z': sprintf( decalname, "{wood%i", gEngfuncs.pfnRandomLong( 1, 3 ) ); break;
		default: sprintf( decalname, "{generic%i", gEngfuncs.pfnRandomLong( 1, 3 ) ); break;
		}
	}
	EV_PlaySurfaceHitSound( trace, bulletType, type );
	if ( ent->solid == SOLID_BSP || ent->movetype == MOVETYPE_PUSHSTEP )
	{
		if ( !r_decals )
			r_decals = gEngfuncs.pfnGetCvarPointer( "r_decals" );
		if ( decalname[0] && r_decals && r_decals->value != 0 )
		{
			int index = gEngfuncs.pEventAPI->EV_IndexFromTrace( trace );
			int decal = gEngfuncs.pEfxAPI->Draw_DecalIndex( gEngfuncs.pEfxAPI->Draw_DecalIndexFromName( decalname ) );
			gEngfuncs.pEfxAPI->R_DecalShoot( decal, index, 0, trace->endpos, 0 );
		}
		EV_HLDM_DoDSurfaceFX( trace, bulletType, type );
	}
}

void EV_HLDM_DecalGunshot( pmtrace_t* trace, int bulletType, float* source, float* end )
{
	physent_t* ent = gEngfuncs.pEventAPI->EV_GetPhysent( trace->ent );
	if ( ent && ent->solid == SOLID_BSP )
		EV_HLDM_GunshotDecalTrace( trace, (char*)" ", source, end, bulletType );
}

void EV_HLDM_FireBullets( int idx, float* forward, float* right, float* up, int shots,
	float* source, float* shooting, float* spread, float distance, int bulletType,
	int tracerFrequency, int* count )
{
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	cl_entity_t* player = gEngfuncs.GetEntityByIndex( idx );
	Vector shotRight( right );
	for ( int i = 1; i <= shots; ++i )
	{
		Vector end = Vector( source ) + ( Vector( shooting ) + shotRight * spread[0] + Vector( up ) * spread[1] ) * distance;
		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetSolidPlayers( idx - 1 );
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_PlayerTrace( source, end, PM_STUDIO_BOX, -1, &trace );
		Vector tracerOrigin = Vector( source ) + Vector( 0, 0, -4 ) + Vector( right ) * 2 + Vector( forward ) * 16;
		if ( player->curstate.gaitsequence == 15 || player->curstate.gaitsequence == 16 )
			tracerOrigin = Vector( source ) + Vector( 0, 0, -16 ) + Vector( right ) * 12 + Vector( forward ) * 50;
		EV_HLDM_CheckTracer( idx, source, trace.endpos, forward, right, bulletType,
			tracerFrequency, count, tracerOrigin );
		if ( player != local )
		{
			Vector playerDirection = player->curstate.origin - local->curstate.origin;
			Vector bulletDirection = Vector( trace.endpos ) - Vector( source );
			pmtrace_t trace2;
			gEngfuncs.pEventAPI->EV_PlayerTrace( source, local->curstate.origin, PM_STUDIO_BOX, -1, &trace2 );
			if ( trace2.fraction > 0.9f )
			{
				Vector cross = CrossProduct( bulletDirection, playerDirection );
				if ( cross.Length() != 0 )
				{
					float angle = acosf( DotProduct( bulletDirection, playerDirection ) / ( bulletDirection.Length() * playerDirection.Length() ) );
					float perpendicular = sin( angle ) * playerDirection.Length();
					if ( perpendicular < 100 && bulletDirection.Length() > cos( angle ) * playerDirection.Length() )
					{
						char sound[32];
						sprintf( sound, "weapons/whizz%d.wav", gEngfuncs.pfnRandomLong( 1, 17 ) );
						gEngfuncs.pfnPlaySoundByName( sound, gEngfuncs.pfnRandomFloat( 0.95f, 1 ) );
					}
				}
			}
		}
		if ( trace.fraction != 1 )
		{
			EV_HLDM_PlayTextureSound( idx, &trace, source, end, bulletType );
			EV_HLDM_DecalGunshot( &trace, bulletType, source, end );
		}
		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
}

void EV_MortarShellCallback( tempent_s* ent, float, float )
{
	Vector direction = Vector( ent->entity.baseline.origin ) * -1;
	if ( ent->entity.baseline.iuser1 <= 0 && ent->entity.baseline.origin[2] < 0 )
	{
		int pitch = gEngfuncs.pfnRandomLong( 0, 10 ) + 95;
		float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
		gEngfuncs.pEventAPI->EV_PlaySound( 0, ent->entity.origin, CHAN_VOICE,
			"weapons/mortar_incoming.wav", volume, ATTN_NORM, 0, pitch );
		ent->entity.baseline.iuser1 = 1;
	}
	VectorAngles( direction, ent->entity.angles );
}

void EV_RocketTrailCallback( tempent_s* ent, float, float currenttime )
{
	if ( currenttime >= ent->entity.baseline.fuser2 )
	{
		ent->entity.baseline.fuser2 = gEngfuncs.GetClientTime() + 0.02;
		model_t* model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/adrian/bazooka_smoke.spr" ) );
		Vector origin = ent->entity.origin;
		float scale = gEngfuncs.pfnRandomFloat( 40, 55 );
		float offsetX = gEngfuncs.pfnRandomFloat( -5, 5 );
		float offsetY = gEngfuncs.pfnRandomFloat( -5, 5 );
		float offsetZ = gEngfuncs.pfnRandomFloat( -5, 5 );
		Vector velocity( ent->entity.baseline.vuser1 );
		if ( ent->entity.baseline.iuser4 != 0 )
			--ent->entity.baseline.iuser4;
		else
		{
			if ( gEngfuncs.pfnRandomLong( 0, 3 ) <= 1 )
				VectorClear( ent->entity.baseline.vuser1 );
			else
			{
				float velocityZ = gEngfuncs.pfnRandomFloat( -25, 25 );
				float velocityY = gEngfuncs.pfnRandomFloat( -25, 25 );
				ent->entity.baseline.vuser1[0] = gEngfuncs.pfnRandomLong( -25, 25 );
				ent->entity.baseline.vuser1[1] = (int)velocityY;
				ent->entity.baseline.vuser1[2] = (int)velocityZ;
			}
			ent->entity.baseline.iuser4 = gEngfuncs.pfnRandomLong( 5, 15 );
		}
		if ( ent->entity.baseline.iuser4 < 0 )
			ent->entity.baseline.iuser4 = 0;
		if ( model )
		{
			origin.x += offsetX;
			origin.y += offsetY;
			origin.z += offsetZ;
			CDoDRocketTrail* particle = CDoDRocketTrail::Create( origin, Vector( 0, 0, 1 ),
				model, scale, 200, "dod_trailsmoke" );
			if ( !particle )
				return;
			particle->m_flGravity = -0.01f;
			particle->m_iRendermode = kRenderTransAlpha;
			particle->SetCollisionFlags( TRI_COLLIDEWORLD );
			particle->m_flSize = scale;
			particle->SetLightFlag( LIGHT_NONE );
			particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
			particle->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -1.5f, 1.5f ) );
			particle->m_flMass = gEngfuncs.pfnRandomFloat( 2, 3 );
			particle->m_flDieTime = gEngfuncs.GetClientTime() + 10;
			particle->m_iFrame = (int)gEngfuncs.pfnRandomFloat( 10, 15 );
			particle->m_flScaleSpeed = 2;
			particle->m_vColor = Vector( 25, 25, 25 );
			particle->m_bRocketTrail = true;
			particle->m_flFadeSpeed = -1;
			particle->m_vVelocity = velocity;
		}
	}
	if ( ent->entity.baseline.fuser1 > currenttime )
		return;
	if ( ent->entity.origin == ent->entity.attachment[0] )
		ent->die = gEngfuncs.GetClientTime();
	else
		ent->entity.attachment[0] = ent->entity.origin;
	if ( cl_dynamiclights->value > 0 )
	{
		dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );
		light->origin = ent->entity.origin;
		light->radius = 160;
		light->dark = 1;
		light->die = gEngfuncs.GetClientTime() + 0.001;
		light->color.r = 255;
		light->color.g = 255;
		light->color.b = 255;
	}
}

void EV_OverheatCallback( tempent_s* ent, float, float currenttime )
{
	if ( currenttime < ent->entity.baseline.fuser2 )
		return;
	ent->entity.baseline.fuser2 = gEngfuncs.GetClientTime() + 0.2;
	model_t* model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/puff.spr" ) );
	Vector start, end;
	if ( !EV_IsLocal( ent->clientIndex ) )
	{
		cl_entity_t* player = gEngfuncs.GetEntityByIndex( ent->clientIndex );
		if ( !player || !player->player )
		{
			ent->die = gEngfuncs.GetClientTime();
			return;
		}
		Vector forward, right, up;
		gEngfuncs.pfnAngleVectors( player->curstate.angles, forward, right, up );
		if ( player->curstate.gaitsequence == 15 || player->curstate.gaitsequence == 16 )
		{
			VectorMA( player->origin, 15, forward, start );
			VectorMA( player->origin, 60, forward, end );
			VectorMA( start, -6, up, start );
			VectorMA( end, -6, up, end );
		}
		else
		{
			VectorMA( player->origin, 5, forward, start );
			VectorMA( player->origin, 30, forward, end );
			VectorMA( start, 13, up, start );
			VectorMA( end, 13, up, end );
		}
	}
	else
	{
		if ( g_flWeaponHeat < 94 )
		{
			ent->die = gEngfuncs.GetClientTime();
			return;
		}
		if ( gHUD.m_vecVelocity.Length() > 20 )
			return;
		cl_entity_t* view = GetViewEntity();
		if ( !view )
		{
			ent->die = gEngfuncs.GetClientTime();
			return;
		}
		start = view->attachment[0];
		end = view->attachment[1];
	}
	Vector direction = start - end;
	Vector origin;
	VectorMA( end, gEngfuncs.pfnRandomFloat( 0.2f, 0.8f ), direction, origin );
	float size = gEngfuncs.pfnRandomFloat( 3, 3 );
	Vector velocity;
	velocity.x = gEngfuncs.pfnRandomFloat( -5, 5 );
	velocity.y = gEngfuncs.pfnRandomFloat( -5, 5 );
	velocity.z = gEngfuncs.pfnRandomFloat( 0, 3 );
	origin.z += gEngfuncs.pfnRandomFloat( 2, 3 );
	if ( !model )
		return;
	CDoDParticle* particle = CDoDParticle::Create( origin, Vector( 90, 0, 0 ), model,
		size, 180, "dod_particle", false );
	if ( !particle )
		return;
	particle->m_iRendermode = kRenderTransAlpha;
	particle->SetLightFlag( LIGHT_NONE );
	particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 10;
	particle->m_vColor = Vector( 255, 255, 255 );
	particle->m_flGravity = -0.015f;
	particle->m_flFadeSpeed = 2;
	particle->m_flScaleSpeed = 1;
	particle->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -1.5f, 1.5f ) );
	particle->m_vVelocity = velocity;
}

extern "C" void EV_RocketTrail( event_args_t* args )
{
	int index = args->entindex;
	TEMPENTITY* ent = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel( args->origin );
	if ( !ent )
		return;
	ent->flags |= FTENT_CLIENTCUSTOM | FTENT_NOMODEL | FTENT_PLYRATTACHMENT | FTENT_PERSIST | FTENT_COLLIDEKILL;
	ent->callback = EV_RocketTrailCallback;
	ent->clientIndex = index;
	ent->die = gEngfuncs.GetClientTime() + 10;
	ent->entity.baseline.fuser1 = gEngfuncs.GetClientTime() + 0.5;
	ent->entity.baseline.fuser2 = gEngfuncs.GetClientTime();
}

extern "C" void EV_MortarShell( event_args_t* args )
{
	int modelIndex;
	model_t* model = gEngfuncs.CL_LoadModel( "models/w_piat_rocket.mdl", &modelIndex );
	TEMPENTITY* ent = gEngfuncs.pEfxAPI->CL_TempEntAlloc( args->origin, model );
	if ( !ent )
		return;
	ent->flags = FTENT_CLIENTCUSTOM | FTENT_PERSIST | 0x300000;
	ent->callback = EV_MortarShellCallback;
	ent->die = gEngfuncs.GetClientTime() + 15;
	VectorScale( args->angles, 1000, ent->entity.baseline.origin );
	ent->entity.baseline.iuser1 = 0;
	ent->entity.baseline.gravity = args->fparam1;
}

extern "C" void EV_Overheat( event_args_t* args )
{
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_ITEM,
		"weapons/mgoverheat.wav", volume, ATTN_NORM, 0, pitch );
	float elapsed = gEngfuncs.GetClientTime() - lastOverheatTime;
	lastOverheatTime = gEngfuncs.GetClientTime();
	if ( !( elapsed < 0 || elapsed >= 1 ) || cl_particlefx->value < 2 )
		return;
	TEMPENTITY* ent = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel( args->origin );
	if ( !ent )
		return;
	ent->flags |= FTENT_CLIENTCUSTOM | FTENT_NOMODEL | FTENT_PLYRATTACHMENT | FTENT_PERSIST;
	ent->callback = EV_OverheatCallback;
	ent->clientIndex = args->entindex;
	ent->die = gEngfuncs.GetClientTime() + 2.5;
	ent->entity.baseline.fuser1 = gEngfuncs.GetClientTime() + 0.5;
	ent->entity.baseline.fuser2 = gEngfuncs.GetClientTime();
}

void EV_DirtTrailCallback( tempent_s* ent, float, float currenttime )
{
	if ( currenttime < ent->entity.baseline.fuser2 )
		return;
	ent->entity.baseline.fuser2 = gEngfuncs.GetClientTime();
	model_t* model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/adrian/bazooka_smoke.spr" ) );
	Vector origin = ent->entity.origin;
	float size = ent->entity.baseline.fuser1;
	ent->entity.baseline.fuser1 = size - 7;
	if ( ent->entity.baseline.fuser1 < 10 )
		ent->entity.baseline.fuser1 = 10;
	float offsetX = gEngfuncs.pfnRandomFloat( -10, 10 );
	float offsetY = gEngfuncs.pfnRandomFloat( -10, 10 );
	float offsetZ = gEngfuncs.pfnRandomFloat( -10, 10 );
	Vector velocity( ent->entity.baseline.origin );
	float speed = velocity.Length() * 0.9f;
	velocity = velocity.Normalize() * speed;
	VectorCopy( velocity, ent->entity.baseline.origin );
	if ( !model )
		return;
	origin.x += offsetX;
	origin.y += offsetY;
	origin.z += offsetZ;
	CDoDRocketTrail* particle = CDoDRocketTrail::Create( origin, Vector( 0, 0, 1 ), model,
		size, 135, "dod_trailsmoke" );
	if ( !particle )
		return;
	particle->m_flGravity = gEngfuncs.pfnRandomFloat( 0.05f, 0.1f );
	particle->SetCollisionFlags( TRI_COLLIDEWORLD | TRI_COLLIDEKILL );
	particle->m_iRendermode = kRenderTransAlpha;
	particle->SetLightFlag( LIGHT_NONE );
	particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
	particle->m_flSize = size;
	particle->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -1.5f, 1.5f ) );
	particle->m_flMass = gEngfuncs.pfnRandomFloat( 2, 3 );
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 8;
	particle->m_iFrame = (int)gEngfuncs.pfnRandomFloat( 10, 15 );
	particle->m_vColor = Vector( 155, 155, 140 );
	particle->m_bRocketTrail = false;
}

void EV_DirtRubble( event_args_t* args )
{
	TEMPENTITY* ent = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel( args->origin );
	if ( !ent )
		return;
	ent->flags |= FTENT_GRAVITY | FTENT_PERSIST | FTENT_COLLIDEKILL | FTENT_NOMODEL | FTENT_CLIENTCUSTOM;
	ent->callback = EV_DirtTrailCallback;
	ent->die = gEngfuncs.GetClientTime() + 2;
	ent->entity.baseline.fuser2 = gEngfuncs.GetClientTime();
	ent->entity.baseline.fuser1 = gEngfuncs.pfnRandomFloat( 175, 200 );
	ent->entity.baseline.origin[0] = gEngfuncs.pfnRandomLong( 0, 1 ) ? 500 : -500;
	ent->entity.baseline.origin[1] = gEngfuncs.pfnRandomLong( 0, 1 ) ? 500 : -500;
	ent->entity.baseline.origin[2] = gEngfuncs.pfnRandomFloat( 500, 1000 );
}

void P_ExplosionFlash( event_args_t* args )
{
	model_t* model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/grenade_flash.spr" ) );
	if ( !model )
	{
		gEngfuncs.Con_DPrintf( "Couldn't load Sprite: %s\n", "sprites/grenade_flash.spr" );
		return;
	}
	gEngfuncs.GetLocalPlayer();
	Vector origin( args->origin );
	origin.z += 16;
	CDoDParticle* particle = CDoDParticle::Create( origin, Vector( 0, 0, 0 ), model,
		100, 234, "dod_particle", false );
	if ( !particle )
		return;
	particle->SetCollisionFlags( TRI_ANIMATEDIE );
	particle->m_iRendermode = kRenderTransAdd;
	particle->SetLightFlag( LIGHT_NONE );
	particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
	particle->m_flScaleSpeed = 2;
	particle->m_iFrame = 0;
	particle->m_iFramerate = 60;
	particle->m_iPFlags = 0;
}

void PlayExplosionSound( float* origin )
{
	int sound = gEngfuncs.pfnRandomLong( 0, 2 );
	const char* name;
	if ( sound == 0 )
		name = "weapons/explode3.wav";
	else if ( sound == 1 )
		name = "weapons/explode4.wav";
	else
		name = "weapons/explode5.wav";
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( 0, origin, CHAN_WEAPON, name, volume, ATTN_NORM, 0, pitch );
}

void PlayMortarExplosionSound( float* origin )
{
	int sound = gEngfuncs.pfnRandomLong( 0, 2 );
	const char* name;
	if ( sound == 0 )
		name = "weapons/mortar_hit1.wav";
	else if ( sound == 1 )
		name = "weapons/mortar_hit2.wav";
	else
		name = "weapons/mortar_hit3.wav";
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( 0, origin, CHAN_WEAPON, name, volume, ATTN_NORM, 0, pitch );
}

void EV_BaseExplosion( event_args_t* args )
{
	if ( args->bparam2 )
		PlayMortarExplosionSound( args->origin );
	else
		PlayExplosionSound( args->origin );
	P_ExplosionFlash( args );
	if ( cl_dynamiclights->value > 0 )
	{
		dlight_t* light = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );
		light->origin = Vector( args->origin );
		light->radius = 400;
		light->decay = 275;
		light->color.r = 248;
		light->color.g = 255;
		light->color.b = 120;
		light->die = gHUD.m_flTime + 3;
	}
}

void P_ExplosionSmoke( event_args_t* args )
{
	Vector angles( 0, 270, 0 );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	CreateExplosionSmokeInside( Vector( args->origin ) );
}

void P_Rubble( event_args_t* args )
{
	int count = gEngfuncs.pfnRandomLong( 32, 42 );
	for ( int i = 0; i <= count; ++i )
	{
		Vector normal( 0, 0, 0 );
		float velocityZ = gEngfuncs.pfnRandomFloat( 350, 450 );
		float velocityY = gEngfuncs.pfnRandomFloat( -500, 500 );
		float velocityX = gEngfuncs.pfnRandomFloat( -500, 500 );
		CreateFlyingRubble( Vector( args->origin ), false, velocityX, velocityY, velocityZ, 2.5f, 0, normal );
	}
	count = gEngfuncs.pfnRandomLong( 8, 16 );
	for ( int i = 0; i <= count; ++i )
	{
		Vector normal( 0, 0, 0 );
		float velocityZ = gEngfuncs.pfnRandomFloat( 200, 350 );
		float velocityY = gEngfuncs.pfnRandomFloat( -250, 250 );
		float velocityX = gEngfuncs.pfnRandomFloat( -250, 250 );
		CreateFlyingRubble( Vector( args->origin ), true, velocityX, velocityY, velocityZ, 4, 0, normal );
	}
}

void EV_WaterExplosion( event_args_t* args )
{
	Vector normal( 0, 0, 1 );
	Vector angles, right, up;
	VectorAngles( normal, angles );
	gEngfuncs.pfnAngleVectors( angles, NULL, right, up );
	right = right.Normalize();
	up = up.Normalize();
	model_t* model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/adrian/dirt_puff.spr" ) );
	Vector origin( args->origin );
	origin.z = EV_HLDM_WaterHeight( origin, origin.z, origin.z + 512 );
	for ( int height = 0; height < 350; height += 10 )
	{
		float size = gEngfuncs.pfnRandomFloat( 100, 150 );
		float offsetX = gEngfuncs.pfnRandomFloat( -35, 35 );
		float offsetY = gEngfuncs.pfnRandomFloat( -35, 35 );
		if ( !model )
			continue;
		Vector particleOrigin = origin + Vector( offsetX, offsetY, (float)height );
		CDoDDirtExploDust* particle = CDoDDirtExploDust::Create( particleOrigin, normal, model,
			size, 255, "dod_dirtexplo" );
		if ( !particle )
			continue;
		particle->m_iRendermode = kRenderTransAlpha;
		particle->m_flGravity = 0.2f;
		particle->m_flSize = size;
		particle->SetLightFlag( LIGHT_NONE );
		particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		particle->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -1.5f, 1.5f ) );
		particle->m_flMass = gEngfuncs.pfnRandomFloat( 2, 3 );
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 10;
		particle->m_iFrame = (int)gEngfuncs.pfnRandomFloat( 0, 3 );
		particle->m_flScaleSpeed = gEngfuncs.pfnRandomFloat( 1.5f, 2.8f );
		particle->m_vColor = Vector( 200, 200, 225 );
		float upSpeed = gEngfuncs.pfnRandomFloat( 25, 50 );
		upSpeed *= gEngfuncs.pfnRandomLong( -1, 1 );
		Vector velocity = up * upSpeed;
		float rightSpeed = gEngfuncs.pfnRandomFloat( 25, 50 );
		rightSpeed *= gEngfuncs.pfnRandomLong( -1, 1 );
		velocity = velocity + right * rightSpeed;
		float normalSpeed = gEngfuncs.pfnRandomFloat( 300, 450 );
		particle->m_vVelocity = normal * normalSpeed + velocity;
		particle->m_flActivateTime = gEngfuncs.pfnRandomFloat( 0.95f, 0.96f );
		particle->m_bFire = false;
		particle->m_flFadeSpeed = -1;
	}
	model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/adrian/water_big.spr" ) );
	float size = gEngfuncs.pfnRandomFloat( 275, 300 );
	Vector splashOrigin = origin;
	splashOrigin.z += 0.5f * size - 10;
	CDoDParticle* splash = CDoDParticle::Create( splashOrigin, Vector( 90, 0, 0 ), model,
		size, 255, "dod_particle", false );
	if ( !splash )
		return;
	splash->m_iRendermode = kRenderTransAdd;
	splash->SetLightFlag( LIGHT_NONE );
	splash->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
	splash->m_flDieTime = gEngfuncs.GetClientTime() + 10;
	splash->m_vColor = Vector( 255, 255, 255 );
	splash->m_flGravity = 0.5f;
	splash->m_flFadeSpeed = 10;
	splash->m_vVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( 100, 125 ) );
}

void EV_DirtExplosion( event_args_t* args )
{
	Vector start( args->origin );
	Vector end = start + Vector( 0, 0, -64 );
	pmtrace_t trace = *gEngfuncs.PM_TraceLine( start, end, PM_WORLD_ONLY, 2, -1 );
	model_t* model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/adrian/dirt_puff.spr" ) );
	Vector normal( trace.plane.normal );
	Vector angles, right, up;
	VectorAngles( normal, angles );
	gEngfuncs.pfnAngleVectors( angles, NULL, right, up );
	right = right.Normalize();
	up = up.Normalize();
	for ( int height = 0; height < 350; height += 10 )
	{
		Vector origin( trace.endpos );
		float size = gEngfuncs.pfnRandomFloat( 150, 225 );
		float offsetX = gEngfuncs.pfnRandomFloat( -15, 15 );
		float offsetY = gEngfuncs.pfnRandomFloat( -15, 15 );
		if ( !model )
			continue;
		origin.x += offsetX;
		origin.y += offsetY;
		origin.z += height;
		CDoDDirtExploDust* particle = CDoDDirtExploDust::Create( origin, Vector( 0, 0, 1 ), model,
			size, 225, "dod_dirtexplo" );
		if ( !particle )
			continue;
		particle->m_iRendermode = kRenderTransAlpha;
		particle->SetCollisionFlags( TRI_COLLIDEWORLD | TRI_COLLIDEKILL );
		particle->m_flSize = size;
		particle->SetLightFlag( LIGHT_NONE );
		particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		particle->m_flGravity = 0.25f;
		particle->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -1.5f, 1.5f ) );
		particle->m_flMass = gEngfuncs.pfnRandomFloat( 2, 3 );
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 10;
		particle->m_iFrame = 0;
		particle->m_flScaleSpeed = gEngfuncs.pfnRandomFloat( 0, 1.7f );
		particle->m_vColor = Vector( 155, 155, 140 );
		float upSpeed = gEngfuncs.pfnRandomFloat( 25, 50 );
		upSpeed *= gEngfuncs.pfnRandomLong( -1, 1 );
		Vector velocity = up * upSpeed;
		float rightSpeed = gEngfuncs.pfnRandomFloat( 25, 50 );
		rightSpeed *= gEngfuncs.pfnRandomLong( -1, 1 );
		velocity = velocity + right * rightSpeed;
		float normalSpeed = gEngfuncs.pfnRandomFloat( 300, 450 );
		particle->m_vVelocity = normal * normalSpeed + velocity;
		particle->m_flActivateTime = gEngfuncs.pfnRandomFloat( 0.9f, 0.99f );
		particle->m_bFire = false;
		particle->m_flFadeSpeed = -1;
	}
	model = (model_t*)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/adrian/explosion_flash.spr" ) );
	float size = gEngfuncs.pfnRandomFloat( 100, 125 );
	CDoDParticle* flash = CDoDParticle::Create( start, Vector( 90, 0, 0 ), model,
		size, 255, "dod_particle", false );
	if ( !flash )
		return;
	flash->m_iRendermode = kRenderTransAlpha;
	flash->SetLightFlag( LIGHT_NONE );
	flash->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
	flash->m_flDieTime = gEngfuncs.GetClientTime() + 0.1f;
	flash->m_vColor = Vector( 255, 255, 200 );
	flash->m_flGravity = 0;
	flash->m_flFadeSpeed = 36;
	flash->m_vAngles.z = gEngfuncs.pfnRandomFloat( 0, 360 );
	P_Rubble( args );
	int count = gEngfuncs.pfnRandomLong( 4, 6 );
	for ( int i = 0; i < count; ++i )
		EV_DirtRubble( args );
}

void EV_SandExplosion( event_args_t* args )
{
	EV_DirtExplosion( args );
}

extern "C" void EV_Explosion( event_args_t* args )
{
	if ( args->bparam1 )
	{
		EV_WaterExplosion( args );
		int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
		float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
		gEngfuncs.pEventAPI->EV_PlaySound( 0, args->origin, CHAN_WEAPON,
			"weapons/explodewater.wav", volume, ATTN_NORM, 0, pitch );
		return;
	}
	Vector end = Vector( args->origin ) + Vector( 0, 0, -256 );
	pmtrace_t* trace = gEngfuncs.PM_TraceLine( args->origin, end, 0, 2, -1 );
	const char* texture = gEngfuncs.pEventAPI->EV_TraceTexture( trace->ent, args->origin, end );
	char type = EV_TexNameToType( (char*)texture );
	if ( !texture || type == 'B' || type == 'C' || type == 'L' || type == 'R' )
	{
		P_Rubble( args );
		P_ExplosionSmoke( args );
	}
	else if ( type == 'A' )
		EV_DirtExplosion( args );
	else
	{
		if ( type == 'D' )
			EV_DirtExplosion( args );
		P_ExplosionSmoke( args );
	}
	EV_BaseExplosion( args );
}

extern "C" void EV_USVoice( event_args_t* args )
{
	int idx = args->entindex;
	int command = args->iparam1;
	int british = args->iparam2;
	Vector origin( args->origin );
	if ( GetClientVoice() && GetClientVoice()->IsPlayerBlocked( idx ) )
		return;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	const char* sound = british ? s_BRITVoiceFiles[command] : s_USVoiceFiles[command];
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_VOICE, sound, volume, ATTN_NORM, 0, PITCH_NORM );
	cl_entity_t* player = gEngfuncs.GetEntityByIndex( idx );
	hud_player_info_t* info = &g_PlayerInfoList[idx];
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	gEngfuncs.pfnGetPlayerInfo( idx, info );
	if ( player->curstate.team == g_iTeamNumber && g_iUser1 == 0 && player->curstate.team == local->curstate.team )
	{
		Vector distance = origin - local->curstate.origin;
		if ( distance.Length() <= 1100 )
		{
			gEngfuncs.pfnGetPlayerInfo( idx, info );
			char pattern[256];
			sprintf( pattern, "%c%s%s%s\n", 2, "(%s1) ", g_PlayerInfoList[idx].name, ": %s2" );
			char* text = s_VoiceCommands[command][3];
			if ( !text || !text[0] || !gHUD.m_bBritish )
				text = s_VoiceCommands[command][1];
			gHUD.m_SayText.SayTextPrint( pattern, 256, idx, "#VOICE", text, NULL, NULL );
			gEngfuncs.pfnGetPlayerInfo( idx, info );
			gHUD.m_Spectator.AddVoiceIconToPlayerEnt( idx );
		}
	}
}

extern "C" void EV_GERVoice( event_args_t* args )
{
	int idx = args->entindex;
	int command = args->iparam1;
	Vector origin( args->origin );
	if ( GetClientVoice() && GetClientVoice()->IsPlayerBlocked( idx ) )
		return;
	if ( command == 27 && gHUD.m_bBritish )
		command = 28;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_VOICE, s_GERVoiceFiles[command], volume, ATTN_NORM, 0, PITCH_NORM );
	cl_entity_t* player = gEngfuncs.GetEntityByIndex( idx );
	cl_entity_t* local = gEngfuncs.GetLocalPlayer();
	if ( player->curstate.team == g_iTeamNumber && g_iUser1 == 0 && player->curstate.team == local->curstate.team )
	{
		Vector distance = origin - local->curstate.origin;
		if ( distance.Length() <= 1100 )
		{
			gEngfuncs.pfnGetPlayerInfo( idx, &g_PlayerInfoList[idx] );
			char pattern[256];
			sprintf( pattern, "%c%s%s%s\n", 2, "(%s1) ", g_PlayerInfoList[idx].name, ": %s2" );
			char* text;
			if ( s_VoiceCommands[command][2][0] )
				text = CHudTextMessage::BufferedLocaliseTextString( s_VoiceCommands[command][2] );
			else
				text = CHudTextMessage::BufferedLocaliseTextString( s_VoiceCommands[command][1] );
			gHUD.m_SayText.SayTextPrint( pattern, 256, idx, "#VOICE", text, NULL, NULL );
			gEngfuncs.pfnGetPlayerInfo( idx, &g_PlayerInfoList[idx] );
			gHUD.m_Spectator.AddVoiceIconToPlayerEnt( idx );
		}
	}
}

extern "C" void EV_RoundReleaseSound( event_args_t* args )
{
	int idx = args->entindex;
	Vector origin( args->origin );
	if ( !EV_IsLocal( idx ) || g_iVuser1z != 0 )
		return;
	unsigned int choice = args->iparam2;
	if ( choice > 3 )
		return;
	const char* sound;
	if ( args->iparam1 == 1 && args->bparam1 )
		sound = choice & 1 ? "player/britstartround2.wav" : "player/britstartround.wav";
	else if ( args->iparam1 == 1 && !args->bparam1 )
		sound = choice & 1 ? "player/usstartround2.wav" : "player/usstartround.wav";
	else if ( args->iparam1 == 2 )
		sound = choice & 1 ? "player/gerstartround2.wav" : "player/gerstartround.wav";
	else
		return;
	if ( sound )
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, sound, 1, ATTN_NORM, 0, PITCH_NORM );
}

extern "C" void EV_FireMelee( event_args_t *args )
{
	int animation = args->iparam1;
	bool slash = args->bparam1 != 0;
	bool hit = args->bparam2 != 0;
	if ( EV_IsLocal( args->entindex ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, EV_GetWeaponBody() );
	if ( slash )
	{
		gEngfuncs.pfnRandomLong( 0, 1 );
		int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
		gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_WEAPON,
			"weapons/knife_slash1.wav", 1, ATTN_NORM, 0, pitch );
	}
	if ( hit )
	{
		gEngfuncs.pfnRandomLong( 0, 2 );
		int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
		gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_WEAPON,
			"weapons/knife_hit1.wav", 1, ATTN_NORM, 0, pitch );
	}
	EV_ResetAnimationEvents( args->entindex );
}

extern "C" void EV_FireColt( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	int empty = args->bparam1;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( empty ? 6 : 1, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_COLT );
	}
	EV_MuzzleFlash( idx, 1 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/colt_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_COLT, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireLuger( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	int empty = args->bparam1;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( empty ? 4 : 3, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_LUGER );
	}
	EV_MuzzleFlash( idx, 1 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/luger_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_LUGER, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireMG42( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	bool callAnimation = args->bparam1 == 0;
	int remaining = args->iparam1;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	gEngfuncs.pEventAPI->EV_FindModelIndex( "models/shell_mg42.mdl" );
	if ( EV_IsLocal( idx ) && callAnimation )
	{
		remaining = min( remaining, 8 );
		bool deployed = gHUD.IsInMGDeploy();
		int body = EV_GetWeaponBody();
		int animation = deployed ? MG42_DownAnims[remaining] : MG42_UpAnims[remaining];
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, body );
		gHUD.DoRecoil( WEAPON_MG42 );
	}
	if ( callAnimation )
	{
		EV_MuzzleFlash( idx, 4 );
		int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
		float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/mg42_shoot.wav", volume, ATTN_NORM, 0, pitch );
	}
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 2, source, aiming, spread, 8192, BULLET_PLAYER_MG42, 3, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireMG34( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	int remaining = args->iparam1;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( EV_IsLocal( idx ) )
	{
		int animation = gHUD.IsInMGDeploy() ? (remaining <= 0 ? 5 : 4) : (remaining <= 0 ? 11 : 10);
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_MG34 );
	}
	EV_MuzzleFlash( idx, 4 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/mg34_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 2, source, aiming, spread, 8192, BULLET_PLAYER_MG34, 3, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_Fire30CAL( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	int remaining = args->iparam1;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( EV_IsLocal( idx ) )
	{
		remaining = min( remaining, 8 );
		bool deployed = gHUD.IsInMGDeploy();
		int body = EV_GetWeaponBody();
		int animation = deployed ? ThirtyCal_DownAnims[remaining] : ThirtyCal_UpAnims[remaining];
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, body );
		gHUD.DoRecoil( WEAPON_30CAL );
	}
	EV_MuzzleFlash( idx, 4 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/30cal_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_30CAL, 3, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireGarand( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	int empty = args->bparam1;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( args->iparam1 == 1 )
	{
		if ( EV_IsLocal( idx ) )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( empty ? 10 : 9, EV_GetWeaponBody() );
		int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
		float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/knifeswing.wav", volume, ATTN_NORM, 0, pitch );
	}
	else
	{
		if ( EV_IsLocal( idx ) )
		{
			int animation = 4;
			if ( empty ) animation = gEngfuncs.pfnRandomLong( 0, 2 ) + 1;
			gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, EV_GetWeaponBody() );
			gHUD.DoRecoil( WEAPON_GARAND );
		}
		EV_MuzzleFlash( idx, 2 );
		int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
		float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/garand_shoot.wav", volume, ATTN_NORM, 0, pitch );
		if ( !empty )
		{
			float clipVolume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
			gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/garand_reload_clipding.wav", clipVolume, ATTN_NORM, 0, 100 );
		}
		Vector source;
		EV_GetGunPosition( args, source, origin );
		Vector aiming = forward;
		Vector spread( spreadX, spreadY, spreadY );
		EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_GARAND, 0, &tracerCount[idx] );
		EV_ResetAnimationEvents( idx );
	}
}

extern "C" void EV_FireM1Carbine( event_args_t *args )
{
	float spreadY = args->fparam2, spreadX = args->fparam1;
	GetViewEntity();
	int idx = args->entindex;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 3, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_M1CARBINE );
	}
	EV_MuzzleFlash( idx, 2 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/carbine_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_M1CARBINE, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireScopedKar( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_SCOPEDKAR );
	}
	EV_MuzzleFlash( idx, 2 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/kar_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_SCOPEDKAR, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireThompson( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( EV_IsLocal( idx ) )
	{
		int body = EV_GetWeaponBody();
		int animation = gEngfuncs.pfnRandomLong( 0, 1 ) + 3;
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, body );
		gHUD.DoRecoil( WEAPON_THOMPSON );
	}
	EV_MuzzleFlash( idx, 3 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/thompson_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_THOMPSON, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireMP44( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 3, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_MP44 );
	}
	EV_MuzzleFlash( idx, 3 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/mp44_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_MP44, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireGreaseGun( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( EV_IsLocal( idx ) )
	{
		int body = EV_GetWeaponBody();
		int animation = gEngfuncs.pfnRandomLong( 0, 1 ) + 3;
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, body );
		gHUD.DoRecoil( WEAPON_GREASEGUN );
	}
	EV_MuzzleFlash( idx, 3 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/greasegun_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_GREASEGUN, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireFG42( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( EV_IsLocal( idx ) )
	{
		int animation = gHUD.IsInMGDeploy() ? 7 : 3;
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_FG42 );
	}
	EV_MuzzleFlash( idx, 3 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/fg42_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_FG42, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireK43( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( args->iparam1 == 1 )
	{
		if ( EV_IsLocal( idx ) )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( 6, EV_GetWeaponBody() );
		int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
		float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/knifeswing.wav", volume, ATTN_NORM, 0, pitch );
	}
	else
	{
		if ( EV_IsLocal( idx ) )
		{
			int body = EV_GetWeaponBody();
			int animation = gEngfuncs.pfnRandomLong( 0, 1 ) + 1;
			gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, body );
			gHUD.DoRecoil( WEAPON_K43 );
		}
		EV_MuzzleFlash( idx, 2 );
		int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
		float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/k43_shoot.wav", volume, ATTN_NORM, 0, pitch );
		Vector source;
		EV_GetGunPosition( args, source, origin );
		Vector aiming = forward;
		Vector spread( spreadX, spreadY, spreadY );
		EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_K43, 0, &tracerCount[idx] );
		EV_ResetAnimationEvents( idx );
	}
}

extern "C" void EV_FireEnfield( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( args->iparam1 != 1 )
	{
		if ( EV_IsLocal( idx ) )
		{
			gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, EV_GetWeaponBody() );
			gHUD.DoRecoil( WEAPON_ENFIELD );
		}
		EV_MuzzleFlash( idx, 2 );
		int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
		float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/enfield_shoot.wav", volume, ATTN_NORM, 0, pitch );
		Vector source;
		EV_GetGunPosition( args, source, origin );
		Vector aiming = forward;
		Vector spread( spreadX, spreadY, spreadY );
		EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_ENFIELD, 0, &tracerCount[idx] );
		EV_ResetAnimationEvents( idx );
		return;
	}
	int swing = (int)gEngfuncs.pfnRandomFloat( 0, 3 );
	const char *sound;
	switch ( swing )
	{
	case 0:
	case 2:
		sound = "weapons/knifeswing.wav";
		break;
	case 1:
	case 3:
		sound = "weapons/knifeswing2.wav";
		break;
	default:
		return;
	}
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, sound, volume, ATTN_NORM, 0, pitch );
	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 5, EV_GetWeaponBody() );
}

extern "C" void EV_FireSten( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 3, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_STEN );
	}
	EV_MuzzleFlash( idx, 3 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/sten_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_STEN, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireBren( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
	{
		int animation = gHUD.IsInMGDeploy() ? 7 : 3;
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_BREN );
	}
	EV_MuzzleFlash( idx, 3 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/bren_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_BREN, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireWebley( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_WEBLEY );
	}
	EV_MuzzleFlash( idx, 1 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/webley_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_WEBLEY, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_PopHelmet( event_args_t *args )
{
	Vector origin( args->origin );
	Vector velocity( args->angles );
	int helmet = args->iparam1;
	if ( helmet <= 6 )
	{
		int model = gEngfuncs.pEventAPI->EV_FindModelIndex( sHelmetModels[helmet] );
		if ( model )
		{
			Vector angles( args->fparam1, args->fparam2, 0.0f );
			TEMPENTITY *ent = gEngfuncs.pEfxAPI->R_TempModel( origin, velocity, angles, 5, model, TE_BOUNCE_SHELL );
			if ( ent ) ent->flags |= FTENT_ROTATE | FTENT_COLLIDEWORLD | FTENT_HITSOUND;
		}
	}
}

extern "C" void EV_Knife( event_args_t *args )
{
	int idx = args->entindex;
	Vector origin( args->origin );
	int slash = (int)gEngfuncs.pfnRandomFloat( 0, 1 );
	if ( slash == 0 || slash == 1 )
	{
		int pitch = (int)(gEngfuncs.pfnRandomFloat( 0, 15 ) + 94);
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON,
			slash == 0 ? "weapons/knife_slash1.wav" : "weapons/knife_slash2.wav", 1, ATTN_NORM, 0, pitch );
	}
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, EV_GetWeaponBody() );
		int swing = g_iSwing++ % 2;
		if ( swing == 0 )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, EV_GetWeaponBody() );
		else if ( swing == 1 )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( 2, EV_GetWeaponBody() );
	}
}

extern "C" void EV_FireMP40( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	GetViewEntity();
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 3, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_MP40 );
	}
	EV_MuzzleFlash( idx, 3 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/mp40_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_MP40, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireSpring( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
	{
		int body = EV_GetWeaponBody();
		int animation = gEngfuncs.pfnRandomLong( 0, 1 ) + 1;
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, body );
		gHUD.DoRecoil( WEAPON_SPRING );
	}
	EV_MuzzleFlash( idx, 2 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/spring_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_SPRING, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireKar( event_args_t *args )
{
	int idx = args->entindex;
	int fireMode = args->iparam1;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( fireMode == 1 )
	{
		int swing = (int)gEngfuncs.pfnRandomFloat( 0, 3 );
		const char *sound;
		int animation;
		switch ( swing )
		{
		case 0:
		case 2:
			sound = "weapons/knifeswing.wav";
			animation = 4;
			break;
		case 1:
		case 3:
			sound = "weapons/knifeswing2.wav";
			animation = 5;
			break;
		default:
			return;
		}
		int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
		float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, sound, volume, ATTN_NORM, 0, pitch );
		if ( EV_IsLocal( idx ) )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, EV_GetWeaponBody() );
		return;
	}
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_KAR );
	}
	EV_MuzzleFlash( idx, 2 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/kar_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_KAR, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireBAR( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	GetViewEntity();
	if ( EV_IsLocal( idx ) )
	{
		int animation = gHUD.IsInMGDeploy() ? 7 : 3;
		gEngfuncs.pEventAPI->EV_WeaponAnimation( animation, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_BAR );
	}
	EV_MuzzleFlash( idx, 4 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/bar_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_BAR, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}

extern "C" void EV_FireBazooka( event_args_t *args )
{
	if ( EV_IsLocal( args->entindex ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 3, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_BAZOOKA );
	}
	EV_MuzzleFlash( args->entindex, 4 );
	EV_ResetAnimationEvents( args->entindex );
}

extern "C" void EV_FirePschreck( event_args_t *args )
{
	if ( EV_IsLocal( args->entindex ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 3, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_PSCHRECK );
	}
	EV_MuzzleFlash( args->entindex, 4 );
	EV_ResetAnimationEvents( args->entindex );
}

extern "C" void EV_FirePIAT( event_args_t *args )
{
	if ( EV_IsLocal( args->entindex ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 6, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_PIAT );
	}
	EV_MuzzleFlash( args->entindex, 4 );
	EV_ResetAnimationEvents( args->entindex );
}

extern "C" void EV_FireMortar( event_args_t *args )
{
	if ( EV_IsLocal( args->entindex ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 3, EV_GetWeaponBody() );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( args->entindex, args->origin, CHAN_WEAPON,
		"weapons/mortar_shoot.wav", volume, ATTN_NORM, 0, pitch );
	EV_MuzzleFlash( args->entindex, 4 );
	EV_ResetAnimationEvents( args->entindex );
}

extern "C" void EV_FireScopedEnfield( event_args_t *args )
{
	int idx = args->entindex;
	float spreadX = args->fparam1, spreadY = args->fparam2;
	Vector origin( args->origin ), angles( args->angles );
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( 1, EV_GetWeaponBody() );
		gHUD.DoRecoil( WEAPON_ENFIELD );
	}
	EV_MuzzleFlash( idx, 2 );
	int pitch = gEngfuncs.pfnRandomLong( 0, 3 ) + 98;
	float volume = gEngfuncs.pfnRandomFloat( 0.92f, 1 );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/enfieldsniper_shoot.wav", volume, ATTN_NORM, 0, pitch );
	Vector source;
	EV_GetGunPosition( args, source, origin );
	Vector aiming = forward;
	Vector spread( spreadX, spreadY, spreadY );
	EV_HLDM_FireBullets( idx, forward, right, up, 1, source, aiming, spread, 8192, BULLET_PLAYER_ENFIELD, 0, &tracerCount[idx] );
	EV_ResetAnimationEvents( idx );
}
