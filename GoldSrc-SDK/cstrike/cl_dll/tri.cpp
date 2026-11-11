//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "../../cl_dll/Exports.h"
#include "tri.h"
#include "z_weather.h"

extern engine_studio_api_t IEngineStudio;
extern cvar_t* cl_fog_density;
extern cvar_t* cl_fog_r;
extern cvar_t* cl_fog_g;
extern cvar_t* cl_fog_b;
extern int g_iWaterLevel;
extern int g_iUser1;
extern float g_flGravity;
extern Vector v_angles;

FogParameters g_FogParameters;

void CreateGasSmoke( Vector origin, Vector velocity, bool insideSmoke, bool spawnInside, bool blowable )
{
	if ( !g_pParticleMan )
		return;

	const char* spriteName = IEngineStudio.IsHardware() ?
		"sprites/gas_puff_01.spr" : "sprites/black_smoke4.spr";
	model_s* sprite = const_cast<model_s*>( gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( spriteName ) ) );
	if ( !sprite )
	{
		gEngfuncs.Con_DPrintf( "Couldn't load Sprite: sprites/gas_puff_01.spr\n" );
		return;
	}

	float scale = gEngfuncs.pfnRandomFloat( 175, 225 );
	CPartSmokeGrenade* smoke = new CPartSmokeGrenade;
	if ( !smoke )
		return;

	smoke->InitializeSprite( origin, Vector( 0, 0, 1 ), sprite, scale, 190 );
	smoke->SetCollisionFlags( TRI_COLLIDEWORLD );
	smoke->m_flGravity = 0;
	smoke->m_iRendermode = kRenderTransAlpha;
	smoke->m_vRingOrigin = origin;
	smoke->m_vVelocity = velocity;
	smoke->m_flSize = scale;
	smoke->m_flScaleSpeed = 0.1f;
	smoke->m_iFrame = IEngineStudio.IsHardware() ?
		gEngfuncs.pfnRandomLong( 0, 1 ) : gEngfuncs.pfnRandomLong( 15, 25 );

	if ( insideSmoke )
	{
		smoke->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -0.8f, 0.8f ) );
		smoke->m_flDampingTime = 0;
		smoke->m_bInsideSmoke = true;
		smoke->m_flScaleSpeed = 0.05f;
		smoke->m_bSpawnInside = spawnInside;
		smoke->m_flDieTime = gEngfuncs.GetClientTime() + 30;
	}
	else
	{
		smoke->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -1, 1 ) );
		smoke->m_flDampingTime = gEngfuncs.GetClientTime() + 0.001;
		smoke->m_bInsideSmoke = false;
		smoke->m_bSpawnInside = spawnInside;
		smoke->m_bDampMod = false;
		smoke->m_flDieTime = gEngfuncs.GetClientTime() + 20;
	}

	smoke->m_flMass = gEngfuncs.pfnRandomFloat( 1, 1.5 );
	float shade = gEngfuncs.pfnRandomFloat( 155, 175 );
	smoke->m_bAffectedByForce = false;
	smoke->m_flFadeSpeed = -1;
	smoke->m_vColor = Vector( shade, shade, shade );
	smoke->SetCullFlag( CULL_PVS | CULL_FRUSTUM_PLANE );
	smoke->SetLightFlag( LIGHT_NONE );
	smoke->SetRenderFlag( RENDER_FACEPLAYER_ROTATEZ );
	if ( !blowable )
		smoke->m_bAffectedByForce = true;
}

void CreateGasInside( Vector origin )
{
	Vector forward, right, up;
	for ( int yaw = 0; yaw < 360; yaw += 45 )
	{
		gEngfuncs.pfnAngleVectors( Vector( 0, yaw, 0 ), forward, right, up );
		VectorNormalize( forward );
		Vector position = origin + forward * gEngfuncs.pfnRandomFloat( 90, 110 ) + up * 45;
		CreateGasSmoke( position, Vector( 0, 0, 0 ), true, false, true );
	}
	for ( int yaw = 0; yaw < 360; yaw += 45 )
	{
		gEngfuncs.pfnAngleVectors( Vector( 0, yaw, 0 ), forward, right, up );
		VectorNormalize( forward );
		CreateGasSmoke( origin + forward * 90 + up * 60, Vector( 0, 0, 0 ), true, false, true );
	}
	gEngfuncs.pfnAngleVectors( Vector( 0, 45, 0 ), forward, right, up );
	VectorNormalize( forward );
	CreateGasSmoke( origin + forward * 45 + up * 55, Vector( 0, 0, 0 ), true, false, true );
	gEngfuncs.pfnAngleVectors( Vector( 0, 270, 0 ), forward, right, up );
	VectorNormalize( forward );
	CreateGasSmoke( origin + forward * 45 + up * 55, Vector( 0, 0, 0 ), true, false, true );
	CreateGasSmoke( origin, Vector( 0, 0, 0 ), true, false, true );
}

void RenderFog( void )
{
	FogParameters fog = g_FogParameters;
	if ( cl_fog_density )
		fog.density = cl_fog_density->value;
	if ( cl_fog_r )
		fog.color[0] = cl_fog_r->value;
	if ( cl_fog_g )
		fog.color[1] = cl_fog_g->value;
	if ( cl_fog_b )
		fog.color[2] = cl_fog_b->value;

	gEngfuncs.pTriAPI->FogParams( fog.density, fog.affectsSkyBox );
	gEngfuncs.pTriAPI->Fog( fog.color, 100, 2000, g_iWaterLevel <= 1 && fog.density > 0 );
}

void CPartSmokeGrenade::Force( void )
{
	if ( !m_bAffectedByForce )
	{
		CreateGasSmoke( m_vOrigin, Vector( 0, 0, 0 ), false, false, false );
		m_bAffectedByForce = true;
	}
}

void CPartSmokeGrenade::Think( float time )
{
	float remaining = m_flDieTime - time;
	if ( m_flBrightness >= 255 && remaining < 3 )
	{
		m_flScaleSpeed = 0;
		m_vAVelocity = Vector( 0, 0, 0 );
		m_flTimeCreated = time;
	}
	if ( remaining <= 5 )
	{
		if ( m_flBrightness > 0 )
			m_flBrightness -= ( time - m_flTimeCreated ) * 0.1f;
		if ( m_flBrightness < 0 )
		{
			m_flBrightness = 0;
			m_flDieTime = gEngfuncs.GetClientTime();
		}
	}
	else
	{
		if ( m_flBrightness < 255 && m_flFadeSpeed == 0 )
			m_flBrightness += ( time - m_flTimeCreated ) * 5 + ( m_bInsideSmoke ? 8 : 10 );
		if ( m_flBrightness > 255 )
			m_flBrightness = 255;
	}
	CBaseParticle::Think( time );

	if ( m_flDampingTime != 0 && time >= m_flDampingTime )
	{
		if ( !m_bDampMod )
		{
			m_flDampingTime = gEngfuncs.GetClientTime() + 0.2;
			m_bDampMod = true;
			if ( m_bSpawnInside )
				CreateGasInside( m_vRingOrigin );
		}
		if ( m_flDampingTime != 0 && time >= m_flDampingTime && m_bDampMod )
		{
			m_flDampingTime = 0;
			m_vVelocity = m_vVelocity * 0.035f;
		}
	}
}

CPartSmokeGrenade* CPartSmokeGrenade::Create( Vector origin, Vector normal, model_s* sprite,
	float size, float brightness, const char* )
{
	CPartSmokeGrenade* particle = new CPartSmokeGrenade;
	if ( !sprite || !particle )
		return NULL;
	particle->InitializeSprite( origin, normal, sprite, size, brightness );
	return particle;
}

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void CL_DLLEXPORT HUD_DrawNormalTriangles( void )
{
//	RecClDrawNormalTriangles();

	gHUD.m_Spectator.DrawOverview();
}

void CL_DLLEXPORT HUD_DrawTransparentTriangles( void )
{
	RenderFog();
	if ( g_pParticleMan )
	{
		g_pParticleMan->SetVariables( g_flGravity,
			g_iUser1 == 0 || g_iUser1 == 3 ? gHUD.m_vecAngles : v_angles );
		g_pParticleMan->Update();
		g_Environment.Update();
	}
}
