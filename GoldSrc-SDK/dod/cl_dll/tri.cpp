//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "event_api.h"
#include "pmtrace.h"
#include "pm_defs.h"
#include "r_studioint.h"
#include "Exports.h"
#include "tri.h"

extern engine_studio_api_t IEngineStudio;
extern cvar_t *cl_particlefx;
extern cvar_t *cl_fog;
extern float g_flGravity;
extern Vector v_origin, v_angles, v_lastAngles;
extern void V_GetDeathCam( cl_entity_t *ent1, cl_entity_t *ent2, float *angle, float *origin );
extern int EV_HLDM_WaterEntryPoint( pmtrace_t *trace, float *src, float *result );

float g_flWeatherTime;
int g_iWeatherType;
cvar_t *cl_weatherdis;
Vector CDoDParticle::m_vGlobalWind;

void UpdateRain()
{
	if ( !gHUD.m_Weather.GetRainSprite() )
		return;
	static float negdist = -650;
	Vector weatherOrigin = gHUD.m_vecOrigin + Vector( 0, 0, 36 );
	for ( int i = 0; i < 350; i++ )
	{
		Vector origin = weatherOrigin;
		origin.x += gEngfuncs.pfnRandomFloat( negdist, 650 );
		origin.y += gEngfuncs.pfnRandomFloat( negdist, 650 );
		origin.z += gEngfuncs.pfnRandomFloat( 100, 300 );
		Vector end( origin.x, origin.y, 8192.0f );
		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( origin, end, PM_WORLD_ONLY, -1, &tr );
		const char *texture = gEngfuncs.pEventAPI->EV_TraceTexture( tr.ent, tr.endpos, end );
		if ( texture && texture[0] == 's' && texture[1] == 'k' && texture[2] == 'y' )
			gHUD.m_Weather.CreateRainParticle( origin );
	}
}

void UpdateSnow()
{
	if ( !gHUD.m_Weather.GetSnowSprite() )
		return;
	static float negdist = -400;
	for ( int i = 0; i < 150; i++ )
	{
		Vector origin = gHUD.m_vecOrigin;
		origin.x += gEngfuncs.pfnRandomFloat( negdist, 400 );
		origin.y += gEngfuncs.pfnRandomFloat( negdist, 400 );
		origin.z += gEngfuncs.pfnRandomFloat( 100, 300 );
		Vector end( origin.x, origin.y, 8192.0f );
		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( origin, end, PM_WORLD_ONLY, -1, &tr );
		const char *texture = gEngfuncs.pEventAPI->EV_TraceTexture( tr.ent, tr.endpos, end );
		if ( texture && texture[0] == 's' && texture[1] == 'k' && texture[2] == 'y' )
			gHUD.m_Weather.CreateSnowParticle( origin );
	}
}

Vector GetViewAngles()
{
	Vector angles;
	if ( g_iUser1 )
	{
		if ( g_iUser3 > 0 && g_iUser2 > 0 )
		{
			Vector origin;
			cl_entity_t *ent2 = gEngfuncs.GetEntityByIndex( g_iUser3 );
			cl_entity_t *ent1 = gEngfuncs.GetEntityByIndex( g_iUser2 );
			V_GetDeathCam( ent1, ent2, angles, origin );
		}
		else if ( g_iUser1 == 1 || g_iUser1 == 4 )
			angles = v_angles;
		else if ( g_iUser1 == 2 || g_iUser1 == 3 )
			gEngfuncs.GetViewAngles( angles );
		else
			angles = v_lastAngles;
	}
	else
	{
		gEngfuncs.GetViewAngles( angles );
		if ( gHUD.IsInMortarDeploy() )
			angles.x = 0;
	}
	return angles;
}

void CreateDebrisWallPuff( Vector origin, Vector velocity, Vector color, int puff )
{
	model_t *sprite = (model_t *)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( puff == 0 ? "sprites/large_smoke_01_ind.spr" : "sprites/puff.spr" ) );
	if ( !sprite )
	{
		gEngfuncs.Con_DPrintf( "Couldn't load Sprite: sprites/large_smoke_01_ind.spr\n" );
		return;
	}
	if ( !IEngineStudio.IsHardware() )
		return;
	float size = gEngfuncs.pfnRandomFloat( 25, 35 );
	float brightness = 200;
	if ( puff > 0 )
		size = gEngfuncs.pfnRandomFloat( 5 * puff, 10 * puff );
	else if ( puff == 0 )
		brightness = 255;
	float distance = (v_origin - origin).Length();
	if ( distance > 1000 )
	{
		size *= 2.5;
		brightness = 255;
	}
	else if ( distance > 500 )
	{
		size *= (distance - 500) / 500 + 1;
		brightness = 225;
	}
	TriangleWallPuff *particle = new TriangleWallPuff;
	particle->InitializeSprite( origin, Vector( 0, 0, 1 ), sprite, size, brightness );
	particle->SetCollisionFlags( TRI_COLLIDEWORLD );
	particle->m_flGravity = -0.1;
	particle->SetLightFlag( LIGHT_INTENSITY );
	particle->SetRenderFlag( RENDER_FACEPLAYER_ROTATEZ );
	particle->m_iRendermode = kRenderTransAlpha;
	particle->m_flSize = size;
	particle->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -1.5, 1.5 ) );
	particle->m_flActivateTime = gEngfuncs.GetClientTime() + 0.1;
	particle->m_flFadeSpeed = 9.5;
	particle->m_flMass = gEngfuncs.pfnRandomFloat( 2, 3 );
	if ( puff <= 0 )
	{
		particle->m_vVelocity = velocity * 95;
		particle->m_flScaleSpeed = 0.9;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 3;
	}
	else
	{
		particle->m_vVelocity = velocity * (30 * puff);
		particle->m_iNumFrames = 9;
		particle->m_iFramerate = 15;
		particle->m_flScaleSpeed = 0.5;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 0.7;
	}
	particle->m_vColor = color;
}

void CreateExplosionSmoke( Vector origin, Vector velocity, bool insideSmoke, bool spawnInside, bool blowable )
{
	if ( !g_pParticleMan )
		return;
	model_t *sprite = (model_t *)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/gas_puff_01.spr" ) );
	if ( !sprite )
	{
		gEngfuncs.Con_DPrintf( "Couldn't load Sprite: %s\n", "sprites/gas_puff_01.spr" );
		return;
	}
	CDoDParticle *particle = CDoDParticle::Create( origin, Vector( 0, 0, 1 ), sprite,
		gEngfuncs.pfnRandomFloat( 175, 225 ), 190, "dod_particle", false );
	if ( !particle )
		return;
	particle->m_iPFlags |= DOD_PARTICLE_SMOKE;
	particle->SetCollisionFlags( TRI_COLLIDEWORLD );
	particle->m_flGravity = 0;
	particle->m_iRendermode = kRenderTransAlpha;
	particle->m_vRingOrigin = origin;
	particle->m_vVelocity = velocity;
	particle->m_flSize = 0;
	particle->m_iFrame = gEngfuncs.pfnRandomLong( 0, 1 );
	particle->m_flScaleSpeed = 0.1;
	if ( insideSmoke )
	{
		particle->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -0.8, 0.8 ) );
		particle->m_flDampingTime = 0;
		float time = gEngfuncs.GetClientTime();
		float life = gEngfuncs.pfnRandomFloat( 6, 9 );
		particle->m_bInsideSmoke = true;
		particle->m_bSpawnInside = spawnInside;
		particle->m_flScaleSpeed = 0.05;
		particle->m_flFadeSpeed = 0.4;
		particle->m_flDieTime = time + life;
	}
	else
	{
		particle->m_vAVelocity = Vector( 0.0f, 0.0f, gEngfuncs.pfnRandomFloat( -1, 1 ) );
		particle->m_flDampingTime = gEngfuncs.GetClientTime() + 0.001;
		particle->m_bInsideSmoke = false;
		particle->m_bSpawnInside = spawnInside;
		particle->m_bDampMod = false;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 25;
	}
	particle->m_flMass = gEngfuncs.pfnRandomFloat( 1, 1.5 );
	particle->m_vColor.x = gEngfuncs.pfnRandomFloat( 87, 102 );
	particle->m_vColor.y = gEngfuncs.pfnRandomFloat( 83, 98 );
	particle->m_vColor.z = gEngfuncs.pfnRandomFloat( 78, 93 );
	particle->SetLightFlag( LIGHT_NONE );
	particle->SetCullFlag( CULL_FRUSTUM_PLANE | CULL_PVS );
	particle->m_bAffectedByForce = !blowable;
	particle->AddGlobalWind();
}

void CreateExplosionSmokeInside( Vector origin )
{
	Vector angles( 0, 0, 0 ), forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	VectorNormalize( forward );
	float distance = gEngfuncs.pfnRandomFloat( 90, 110 );
	CreateExplosionSmoke( origin + forward * distance + up * 45, Vector( 0, 0, 0 ), true, false, true );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	VectorNormalize( forward );
	CreateExplosionSmoke( origin + forward * 90 + up * 60, Vector( 0, 0, 0 ), true, false, true );
	angles.y = 45;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	VectorNormalize( forward );
	CreateExplosionSmoke( origin + forward * 45 + up * 55, Vector( 0, 0, 0 ), true, false, true );
	angles.y = 270;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	VectorNormalize( forward );
	CreateExplosionSmoke( origin + forward * 45 + up * 55, Vector( 0, 0, 0 ), true, false, true );
	CreateExplosionSmoke( origin, Vector( 0, 0, 0 ), true, false, true );
}

void CDoDParticle::AddGlobalWind()
{
	m_vVelocity.x += m_vGlobalWind.x;
	m_vVelocity.y += m_vGlobalWind.y;
}

void CDoDParticle::SetGlobalWind( float *wind )
{
	VectorCopy( wind, m_vGlobalWind );
}

void CDoDParticle::Die()
{
	if ( (m_iPFlags & DOD_PARTICLE_WATER_RIPPLE) && gHUD.m_Weather.GetRippleSprite() )
	{
		Vector start = m_vOrigin + Vector( 0, 0, 32 );
		Vector end = m_vOrigin - Vector( 0, 0, 16 );
		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_NORMAL, -1, &tr );
		Vector point;
		if ( EV_HLDM_WaterEntryPoint( &tr, start, point ) )
		{
			CDoDParticle *ripple = Create( point, Vector( 90, 0, 0 ), gHUD.m_Weather.GetRippleSprite(), 15, 254, "dod_particle", false );
			if ( ripple )
			{
				ripple->m_iRendermode = kRenderTransAdd;
				ripple->m_flFadeSpeed = 4;
				ripple->m_flScaleSpeed = 2;
				ripple->SetLightFlag( LIGHT_COLOR );
				ripple->SetCullFlag( CULL_FRUSTUM_SPHERE | CULL_PVS );
				ripple->m_vColor = Vector( 255, 255, 255 );
				ripple->m_iPFlags = DOD_PARTICLE_FIXED_ANGLES;
				ripple->m_flDieTime = gEngfuncs.GetClientTime() + 2;
			}
		}
	}
	g_pParticleMan->CoreDie( this );
}

void CDoDParticle::Force()
{
	if ( !m_bAffectedByForce )
	{
		CreateExplosionSmoke( m_vOrigin, Vector( 0, 0, 0 ), false, false, false );
		m_bAffectedByForce = true;
	}
}

void CDoDParticle::Think( float time )
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
			m_flBrightness -= (time - m_flTimeCreated) * 0.1;
		if ( m_flBrightness < 0 )
		{
			m_flBrightness = 0;
			m_flDieTime = gEngfuncs.GetClientTime();
		}
	}
	else
	{
		if ( m_flBrightness < 255 && m_flFadeSpeed < -0.5 )
			m_flBrightness += 5 * (time - m_flTimeCreated) + (m_bInsideSmoke ? 8 : 10);
		if ( m_flBrightness > 255 )
			m_flBrightness = 255;
	}
	if ( (m_iPFlags & DOD_PARTICLE_SHRINK) && m_flDieTime - time <= 3 )
	{
		m_flSize -= 0.02;
		if ( m_flSize < 0 )
		{
			m_flSize = 0;
			m_flDieTime = gEngfuncs.GetClientTime();
		}
	}
	g_pParticleMan->CoreThink( this, time );
	Vector angles;
	angles.x = GetViewAngles().x;
	angles.y = GetViewAngles().y;
	angles.z = GetViewAngles().z;
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	if ( m_iPFlags & DOD_PARTICLE_RAIN_ANGLE )
		m_vAngles.z = atan( DotProduct( m_vVelocity, right ) / m_vVelocity.z ) * (180.0 / 3.14159265358979323846);
	if ( !(m_iPFlags & DOD_PARTICLE_FIXED_ANGLES) )
	{
		if ( !(m_iPFlags & DOD_PARTICLE_FIXED_PITCH) )
			m_vAngles.x = angles.x;
		m_vAngles.y = angles.y;
	}
	if ( m_flDampingTime && time >= m_flDampingTime && !m_bDampMod )
	{
		m_flDampingTime = gEngfuncs.GetClientTime() + 0.2;
		m_bDampMod = true;
		if ( m_bSpawnInside )
			CreateExplosionSmokeInside( m_vRingOrigin );
	}
	if ( m_flDampingTime && time >= m_flDampingTime && m_bDampMod )
	{
		m_flDampingTime = 0;
		m_vVelocity = m_vVelocity * 0.035;
	}
	if ( m_iPFlags & DOD_PARTICLE_SMOKE_RING )
	{
		float size = (25 - (m_flDieTime - time)) / 0.2f * 300 / 100 * 100;
		if ( size > 300 )
			size = 300;
		m_flSize = size;
	}
}

void CDoDParticle::Touch( Vector, Vector normal, int )
{
	if ( !(m_iPFlags & DOD_PARTICLE_WATER_SPLASH) )
		return;
	float time = gEngfuncs.GetClientTime();
	model_t *sprite = gHUD.m_Weather.GetSplashSprite();
	if ( !sprite )
		return;
	CBaseParticle *particle = new CBaseParticle;
	float size = gEngfuncs.pfnRandomLong( 25, 30 );
	particle->InitializeSprite( m_vOrigin + normal, Vector( 90, 0, 0 ), sprite, size, 150 );
	particle->m_iRendermode = kRenderTransAdd;
	particle->m_flMass = 1;
	particle->m_flGravity = 0.1;
	particle->SetLightFlag( LIGHT_INTENSITY );
	particle->SetCullFlag( CULL_FRUSTUM_SPHERE | CULL_PVS );
	particle->m_vColor = Vector( 255, 255, 255 );
	particle->m_iNumFrames = sprite->numframes;
	particle->m_iFramerate = gEngfuncs.pfnRandomLong( 15, 60 );
	particle->m_flDieTime = time + 0.3;
	particle->SetCollisionFlags( TRI_ANIMATEDIE );
	particle->SetRenderFlag( RENDER_FACEPLAYER );
}

CDoDParticle *CDoDParticle::Create( Vector org, Vector normal, model_s *sprite, float size, float brightness, const char *classname, bool distCull )
{
	if ( !sprite )
	{
		gEngfuncs.Con_DPrintf( "CDoDParticle::Create called with a null sprite\n" );
		return NULL;
	}
	if ( !(cl_particlefx->value > 0) || !IEngineStudio.IsHardware() )
		return NULL;
	if ( cl_weatherdis->value < 100 )
		cl_weatherdis->value = 100;
	if ( distCull )
	{
		cl_entity_t *player = gEngfuncs.GetLocalPlayer();
		if ( !player )
			return NULL;
		Vector distance( org.x - player->curstate.origin[0], org.y - player->curstate.origin[1], org.z - org.z );
		if ( !(distance.Length() <= cl_weatherdis->value) )
			return NULL;
	}
	CDoDParticle *particle = new CDoDParticle;
	particle->InitializeSprite( org, normal, sprite, size, brightness );
	particle->m_bInsideSmoke = false;
	particle->m_bSpawnInside = false;
	particle->m_bDampMod = false;
	particle->m_bAffectedByForce = false;
	particle->m_iPFlags = 0;
	particle->m_iNumFrames = sprite->numframes;
	strcpy( particle->m_szClassname, classname );
	return particle;
}

void TriangleWallPuff::Think( float time )
{
	if ( m_iFrame <= 7 )
		g_pParticleMan->CoreAnimate( this, time );
	g_pParticleMan->CoreFade( this, time );
	if ( m_flBrightness < 1 )
		m_flBrightness = 1;
	g_pParticleMan->CoreExpand( this, time );
	g_pParticleMan->CoreSpin( this, time );
	g_pParticleMan->CoreCheckCollision( this, time );
	g_pParticleMan->CoreCalculateVelocity( this, time );
	if ( m_flActivateTime && time >= m_flActivateTime )
	{
		m_flActivateTime = 0;
		m_flGravity = -0.02;
		m_vAVelocity = Vector( 0.0f, 0.0f, 0.4f );
		m_vVelocity = m_vVelocity * 0.1;
	}
}

void CDoDRocketTrail::Think( float time )
{
	if ( m_flBrightness > 0 )
	{
		if ( m_bRocketTrail )
		{
			m_flBrightness -= gEngfuncs.pfnRandomFloat( 0.2, 0.4 ) * (time - m_flTimeCreated);
			m_vColor.x += 0.55;
			m_vColor.y += 0.55;
			m_vColor.z += 0.55;
			if ( m_vColor.x < 0 ) m_vColor.x = 0;
			if ( m_vColor.y < 0 ) m_vColor.y = 0;
			if ( m_vColor.z < 0 ) m_vColor.z = 0;
		}
		else
			m_flBrightness -= gEngfuncs.pfnRandomFloat( 1.5, 2 ) * (time - m_flTimeCreated);
	}
	if ( m_flBrightness < 0 )
	{
		m_flBrightness = 0;
		m_flDieTime = gEngfuncs.GetClientTime();
	}
	g_pParticleMan->CoreThink( this, time );
	Vector angles = GetViewAngles();
	m_vAngles.x = angles.x;
	m_vAngles.y = angles.y;
}

CDoDRocketTrail *CDoDRocketTrail::Create( Vector org, Vector normal, model_s *sprite, float size, float brightness, const char * )
{
	if ( !sprite )
	{
		gEngfuncs.Con_DPrintf( "CDoDRocketTrail::Create called with a null sprite\n" );
		return NULL;
	}
	if ( !IEngineStudio.IsHardware() )
		return NULL;
	CDoDRocketTrail *particle = new CDoDRocketTrail;
	particle->InitializeSprite( org, normal, sprite, size, brightness );
	particle->m_iNumFrames = sprite->numframes;
	particle->m_vVelocity.x += CDoDParticle::m_vGlobalWind.x;
	particle->m_vVelocity.y += CDoDParticle::m_vGlobalWind.y;
	return particle;
}

void CDoDDirtExploDust::Think( float time )
{
	if ( m_flBrightness > 0 )
		m_flBrightness -= gEngfuncs.pfnRandomFloat( m_bFire ? 1 : 0.8, m_bFire ? 2.5 : 0.9 ) * (time - m_flTimeCreated);
	if ( m_flBrightness < 0 )
	{
		m_flBrightness = 0;
		m_flDieTime = gEngfuncs.GetClientTime();
	}
	m_vVelocity = m_vVelocity * m_flActivateTime;
	g_pParticleMan->CoreThink( this, time );
	Vector angles = GetViewAngles();
	m_vAngles.x = angles.x;
	m_vAngles.y = angles.y;
}

CDoDDirtExploDust *CDoDDirtExploDust::Create( Vector org, Vector normal, model_s *sprite, float size, float brightness, const char * )
{
	if ( !sprite )
	{
		gEngfuncs.Con_DPrintf( "CDoDDirtExploDust::Create called with a null sprite\n" );
		return NULL;
	}
	if ( !IEngineStudio.IsHardware() )
		return NULL;
	CDoDDirtExploDust *particle = new CDoDDirtExploDust;
	particle->InitializeSprite( org, normal, sprite, size, brightness );
	particle->m_iNumFrames = sprite->numframes;
	particle->m_vVelocity.x += CDoDParticle::m_vGlobalWind.x;
	particle->m_vVelocity.y += CDoDParticle::m_vGlobalWind.y;
	return particle;
}

void CL_DLLEXPORT HUD_DrawNormalTriangles()
{
	g_pcldstAddrs->pDrawNormalTriangles();
}

void CL_DLLEXPORT HUD_DrawTransparentTriangles()
{
	g_pcldstAddrs->pDrawTransparentTriangles();
	if ( g_iOnlyClientDraw )
	{
		if ( g_iOnlyClientDraw == 1 && gHUD.GetMinimapState() > 0 )
		{
			gHUD.m_ObjectiveIcons.CalcIconLocations();
			gHUD.m_DoDMap.DrawOverview();
		}
		return;
	}
	if ( cl_fog->value )
		RenderDoDFog();
	if ( cl_particlefx->value >= 1 && IEngineStudio.IsHardware() )
	{
		g_pParticleMan->SetVariables( g_flGravity, g_iUser1 == 0 || g_iUser1 == 3 ? gHUD.m_vecAngles : v_angles );
		g_pParticleMan->Update();
		float time = gEngfuncs.GetClientTime();
		if ( time > g_flWeatherTime && cl_particlefx->value >= 2 )
		{
			if ( g_iWeatherType == 1 )
			{
				UpdateRain();
				g_flWeatherTime = time + 0.3;
			}
			else if ( g_iWeatherType == 2 )
			{
				UpdateSnow();
				g_flWeatherTime = time + 0.7;
			}
		}
	}
	if ( gHUD.m_iFOV <= 89 )
		gHUD.m_Scope.DrawTriApiScope();
	gHUD.m_MortarHud.DrawPredictedMortarImpactSite();
}
