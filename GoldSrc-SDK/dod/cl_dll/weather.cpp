#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "pmtrace.h"
#include "tri.h"

extern cvar_t *cl_particlefx;

void CDoDRainDrop::Think( float time )
{
	m_flBrightness = 130;
	CDoDParticle::Think( time );
}

void CDoDSnowFlake::Think( float time )
{
	if ( time > m_flFadeOutTime )
	{
		g_pParticleMan->CoreFade( this, time );
		m_flOldTime = time;
	}
	else
	{
		float delta = time - m_flOldTime;
		if ( m_bSpiral )
		{
			double spiral = sin( 5.0 * time + (int)this );
			m_vOrigin.x += m_vVelocity.x * delta + spiral + spiral;
		}
		else
			m_vOrigin.x += m_vVelocity.x * delta;
		m_vOrigin.y += m_vVelocity.y * delta;
		m_vOrigin.z += m_vVelocity.z * delta;
		CheckCollision( time );
		m_flOldTime = time;
	}
}

void CDoDSnowFlake::Touch( Vector, Vector, int )
{
	if ( m_bTouched )
		return;
	SetRenderFlag( RENDER_FACEPLAYER );
	m_bTouched = true;
	m_flOriginalBrightness = m_flBrightness;
	m_bSpiral = false;
	m_vVelocity = Vector( 0, 0, 0 );
	m_iRendermode = kRenderTransAdd;
	m_flFadeSpeed = 0;
	m_flScaleSpeed = 0;
	m_flDampingTime = 0;
	m_iFrame = 0;
	m_flMass = 1;
	m_flGravity = 0;
	m_vColor = Vector( 128, 128, 128 );
	m_flDieTime = gEngfuncs.GetClientTime() + 0.5;
	m_iPFlags = 0;
	m_flTimeCreated = gEngfuncs.GetClientTime();
}

CDoDSnowFlake *CDoDSnowFlake::Create( Vector org, Vector normal, model_s *sprite, float size, float brightness, const char *classname )
{
	if ( !sprite )
	{
		gEngfuncs.Con_DPrintf( "CDoDParticle::Create called with a null sprite\n" );
		return NULL;
	}
	if ( !(cl_particlefx->value > 0) )
		return NULL;
	CDoDSnowFlake *particle = new CDoDSnowFlake;
	particle->InitializeSprite( org, normal, sprite, size, brightness );
	particle->m_bInsideSmoke = false;
	particle->m_bSpawnInside = false;
	particle->m_bDampMod = false;
	particle->m_bAffectedByForce = false;
	particle->m_flOldTime = particle->m_flTimeCreated;
	particle->m_iPFlags = 0;
	particle->m_iNumFrames = sprite->numframes;
	if ( classname )
	{
		strncpy( particle->m_szClassname, classname, sizeof( particle->m_szClassname ) - 1 );
		particle->m_szClassname[sizeof( particle->m_szClassname ) - 1] = '\0';
	}
	return particle;
}

void CWeatherManager::CreateRainParticle( float *origin )
{
	float time = gEngfuncs.GetClientTime();
	CDoDRainDrop *particle = new CDoDRainDrop;
	particle->InitializeSprite( origin, Vector( 0, 0, 0 ), m_pRainSprite, 2, 1 );
	particle->m_bInsideSmoke = false;
	particle->m_bSpawnInside = false;
	particle->m_bDampMod = false;
	particle->m_bAffectedByForce = false;
	particle->m_flStretchY = 40;
	particle->m_vVelocity.x = gEngfuncs.pfnRandomFloat( -50, 50 );
	particle->m_vVelocity.y = gEngfuncs.pfnRandomFloat( -50, 50 );
	particle->m_vVelocity.z = gEngfuncs.pfnRandomFloat( -500, -1500 );
	particle->m_flGravity = 0;
	Vector velocity = particle->m_vVelocity.Normalize();
	Vector end;
	VectorMA( origin, 2000, velocity, end );
	pmtrace_t *tr = gEngfuncs.PM_TraceLine( origin, end, 0, 2, -1 );
	Vector distance = Vector( tr->endpos ) - Vector( origin );
	particle->SetCollisionFlags( TRI_WATERTRACE | TRI_COLLIDEKILL | TRI_COLLIDEWORLD );
	particle->m_iPFlags = DOD_PARTICLE_FIXED_PITCH | DOD_PARTICLE_WATER_RIPPLE | DOD_PARTICLE_RAIN_ANGLE | DOD_PARTICLE_WATER_SPLASH;
	particle->m_iRendermode = kRenderTransAlpha;
	particle->SetLightFlag( LIGHT_NONE );
	particle->SetCullFlag( CULL_FRUSTUM_PLANE | CULL_PVS );
	particle->m_vColor = Vector( 255, 255, 255 );
	particle->m_flDieTime = distance.Length() / particle->m_vVelocity.Length() + time;
	particle->AddGlobalWind();
}

void CWeatherManager::CreateSnowParticle( float *origin )
{
	CDoDSnowFlake *particle = CDoDSnowFlake::Create( origin, Vector( 0, 0, 0 ), m_pSnowSprite, 2, 1, "dod_snow" );
	if ( !particle )
		return;
	particle->m_vVelocity.x = gEngfuncs.pfnRandomFloat( -30, 30 );
	particle->m_vVelocity.y = gEngfuncs.pfnRandomFloat( -30, 30 );
	particle->m_vVelocity.z = -110;
	float type = gEngfuncs.pfnRandomFloat( 0, 1 );
	if ( type < 0.1 )
	{
		particle->m_vVelocity.x *= 0.5;
		particle->m_vVelocity.y *= 0.5;
	}
	else if ( type < 0.2 )
		particle->m_vVelocity.z = -65;
	else if ( type < 0.3 )
		particle->m_vVelocity.z = -75;
	particle->m_flFadeOutTime = gEngfuncs.GetClientTime() + 5;
	particle->m_iRendermode = kRenderTransAdd;
	particle->SetLightFlag( LIGHT_NONE );
	particle->SetCullFlag( CULL_FRUSTUM_SPHERE | CULL_PVS );
	particle->SetRenderFlag( RENDER_FACEPLAYER );
	particle->m_flScaleSpeed = 0;
	particle->m_flDampingTime = 0;
	particle->m_iFrame = 0;
	particle->m_flMass = 1;
	particle->m_flGravity = 0;
	particle->m_flBounceFactor = 0;
	particle->m_flBrightness = 128;
	particle->m_vColor = Vector( 128, 128, 128 );
	particle->m_iPFlags = DOD_PARTICLE_SNOW;
	particle->m_bTouched = false;
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 15;
	particle->m_bSpiral = gEngfuncs.pfnRandomLong( 0, 2 ) <= 0;
	particle->AddGlobalWind();
	particle->SetCollisionFlags( TRI_COLLIDEKILL | TRI_COLLIDEWORLD );
}

int CWeatherManager::Init()
{
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );
	g_iWeatherType = 0;
	g_flWeatherTime = 0;
	return 1;
}

int CWeatherManager::VidInit()
{
	m_pRainSprite = (model_t *)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/rain.spr" ) );
	m_pSnowSprite = (model_t *)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/snowflake.spr" ) );
	m_pSplashSprite = (model_t *)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/wsplash3.spr" ) );
	m_pRippleSprite = (model_t *)gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/ripple.spr" ) );
	g_flWeatherTime = 0;
	g_iWeatherType = 0;
	return 1;
}
