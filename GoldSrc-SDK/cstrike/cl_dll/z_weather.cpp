#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "cl_entity.h"
#include "event_api.h"
#include "pmtrace.h"
#include "pm_defs.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "tri.h"
#include "z_weather.h"
#include "parsemsg.h"

extern engine_studio_api_t IEngineStudio;
extern Vector g_vPlayerVelocity;
extern int g_iUser1;
extern int g_iUser2;
extern "C" int PM_IsThereGrassTexture( void );
extern "C" char PM_FindTextureType( char* name );

CEnvironment g_Environment;
cvar_t* cl_weather;

int CHudHealth::MsgFunc_ReceiveW( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	g_Environment.SetWeatherType( READ_BYTE() );
	return 1;
}

int WaterEntryPoint( pmtrace_t* trace, float* source, float* result )
{
	if ( gEngfuncs.PM_PointContents( trace->endpos, NULL ) == gEngfuncs.PM_PointContents( source, NULL ) )
		return 0;

	Vector start( source ), end( trace->endpos );
	Vector direction = end - start;
	while ( direction.Length() > 4 )
	{
		Vector middle = start + direction * 0.5f;
		if ( gEngfuncs.PM_PointContents( start, NULL ) == gEngfuncs.PM_PointContents( middle, NULL ) )
			start = middle;
		else
			end = middle;
		direction = end - start;
	}
	start.CopyToArray( result );
	return 1;
}

float AngleMod( float angle )
{
	if ( angle < 0 )
	{
		float turns = angle / 360.0;
		return angle + 360 * ( (int)turns + 1 );
	}
	if ( !(angle < 360) )
	{
		float turns = angle / 360.0;
		return angle - 360 * (int)turns;
	}
	return angle;
}

float VecToYaw( Vector direction )
{
	if ( direction.x == 0 && direction.y == 0 )
		return 0;
	float yaw = (int)( atan2( direction.y, direction.x ) * 180 / 3.1415 );
	if ( yaw < 0 )
		yaw += 360;
	return yaw;
}

void CEnvironment::SetupGrass( void )
{
	model_s* second = const_cast<model_s*>( gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/grass_03.spr" ) ) );
	model_s* first = const_cast<model_s*>( gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/grass_01.spr" ) ) );
	SetGrassSprites( first, second );
	m_bGrassActive = PM_IsThereGrassTexture() != 0;
}

void CEnvironment::Initialize( void )
{
	m_vWeatherOrigin = Vector( 0, 0, 0 );
	m_flWeatherTime = 0;
	m_vGrassOrigin = Vector( 0, 0, 0 );
	m_iWeatherType = 0;
	m_bGrassActive = false;
	SetupGrass();

	float windY = gEngfuncs.pfnRandomFloat( -80, 80 );
	float windX = gEngfuncs.pfnRandomFloat( -80, 80 );
	m_vWind = Vector( windX, windY, 0.0f );
	windY = gEngfuncs.pfnRandomFloat( -80, 80 );
	windX = gEngfuncs.pfnRandomFloat( -80, 80 );
	m_vDesiredWindDirection = Vector( windX, windY, 0.0f );
	m_flNextWindChangeTime = gEngfuncs.GetClientTime();
	m_pRainSprite = const_cast<model_s*>( gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/rain.spr" ) ) );
	m_pSnowSprite = const_cast<model_s*>( gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/snowflake.spr" ) ) );
	m_pRipple = const_cast<model_s*>( gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/effects/ripple.spr" ) ) );
	m_pRainSplash = const_cast<model_s*>( gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/wsplash3.spr" ) ) );
	m_flWeatherValue = cl_weather->value;
}

void CEnvironment::UpdateWind( void )
{
	float time = gEngfuncs.GetClientTime();
	if ( !(gEngfuncs.GetClientTime() < m_flNextWindChangeTime) )
	{
		float windY = gEngfuncs.pfnRandomFloat( -80, 80 );
		float windX = gEngfuncs.pfnRandomFloat( -80, 80 );
		m_vDesiredWindDirection = Vector( windX, windY, 0.0f );
		float changeTime = gEngfuncs.GetClientTime();
		m_flNextWindChangeTime = changeTime + gEngfuncs.pfnRandomFloat( 15, 30 );
		m_flDesiredWindSpeed = m_vDesiredWindDirection.Length();
		m_flIdealYaw = VecToYaw( m_vDesiredWindDirection.Normalize() );
	}

	Vector direction = m_vWind.Normalize();
	Vector angles;
	VectorAngles( direction, angles );
	float yaw = AngleMod( angles.y );
	if ( m_flIdealYaw != yaw )
	{
		float speed = ( time - m_flOldTime ) * 0.5 * 10;
		float move = m_flIdealYaw - yaw;
		if ( m_flIdealYaw > yaw )
		{
			if ( move >= 180 )
				move -= 360;
		}
		else if ( move <= -180 )
			move += 360;
		if ( move > 0 )
			move = min( move, speed );
		else
			move = max( move, -speed );
		angles.y = AngleMod( yaw + move );
	}
	gEngfuncs.pfnAngleVectors( angles, direction, NULL, NULL );
	m_vWind = direction * m_flDesiredWindSpeed;
}

void CEnvironment::Update( void )
{
	Vector origin = gHUD.m_vecOrigin + Vector( 0, 0, 36 );
	if ( g_iUser1 != 3 && g_iUser1 > 0 )
	{
		cl_entity_t* target = gEngfuncs.GetEntityByIndex( g_iUser2 );
		if ( target )
			origin = target->origin + Vector( 0, 0, 36 );
	}
	if ( cl_weather->value > 3 )
		gEngfuncs.Cvar_SetValue( "cl_weather", 3 );
	m_flWeatherValue = cl_weather->value;
	if ( !IEngineStudio.IsHardware() )
		m_flWeatherValue = 0;
	m_vWeatherOrigin = origin;
	UpdateWind();
	if ( m_bGrassActive && m_flGrassTime <= gEngfuncs.GetClientTime() &&
		( m_vWeatherOrigin - m_vGrassOrigin ).Length() >= 100 )
		UpdateGrass();
	if ( !(m_flWeatherTime > gEngfuncs.GetClientTime()) )
	{
		if ( m_iWeatherType == 2 )
			UpdateSnow();
		if ( m_iWeatherType == 1 )
			UpdateRain();
	}
	m_flOldTime = gEngfuncs.GetClientTime();
}

void CEnvironment::UpdateRain( void )
{
	m_flWeatherTime = gEngfuncs.GetClientTime() + 0.3;
	Vector movement = g_vPlayerVelocity.Normalize() * g_vPlayerVelocity.Length();
	int windParticle = 0;
	for ( int i = 0; i < 150.0 * m_flWeatherValue; ++i )
	{
		float distance = gEngfuncs.pfnRandomFloat( 0, 400 );
		int side = gEngfuncs.pfnRandomLong( 0, 5 );
		Vector origin = m_vWeatherOrigin;
		origin.x += side > 2 ? distance : -distance;
		origin.y += gEngfuncs.pfnRandomFloat( -400, 400 );
		origin.z += gEngfuncs.pfnRandomFloat( 100, 300 );
		origin = origin + movement;
		Vector end( origin.x, origin.y, 8000.0f );
		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( origin, end, PM_WORLD_ONLY, -1, &trace );
		const char* texture = gEngfuncs.pEventAPI->EV_TraceTexture( trace.ent, trace.endpos, end );
		if ( texture && !strncmp( texture, "sky", 3 ) )
		{
			CreateRaindrop( origin );
			if ( windParticle == 15 )
			{
				Vector windOrigin( origin.x, origin.y, gHUD.m_vecOrigin.z );
				if ( gEngfuncs.pTriAPI->BoxInPVS( windOrigin, windOrigin ) )
				{
					end.z = 8000;
					gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
					gEngfuncs.pEventAPI->EV_PlayerTrace( windOrigin, end, PM_WORLD_ONLY, -1, &trace );
					texture = gEngfuncs.pEventAPI->EV_TraceTexture( trace.ent, trace.endpos, end );
					if ( texture && !strncmp( texture, "sky", 3 ) )
					{
						end.z = -8000;
						gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
						gEngfuncs.pEventAPI->EV_PlayerTrace( windOrigin, end, PM_WORLD_ONLY, -1, &trace );
						CreateWindParticle( trace.endpos );
					}
				}
				windParticle = 1;
			}
			else
				++windParticle;
		}
	}
}

void CEnvironment::UpdateSnow( void )
{
	m_flWeatherTime = gEngfuncs.GetClientTime() + 0.7;
	gEngfuncs.GetLocalPlayer();
	Vector movement = g_vPlayerVelocity.Normalize() * g_vPlayerVelocity.Length();
	for ( int i = 0; i < 150.0 * m_flWeatherValue; ++i )
	{
		float distance = gEngfuncs.pfnRandomFloat( 0, 300 );
		int side = gEngfuncs.pfnRandomLong( 0, 5 );
		Vector origin = m_vWeatherOrigin;
		origin.x += side > 2 ? distance : -distance;
		origin.y += gEngfuncs.pfnRandomFloat( -300, 300 );
		origin.z += gEngfuncs.pfnRandomFloat( 100, 300 );
		origin = origin + movement;
		Vector end( origin.x, origin.y, 8000.0f );
		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( origin, end, PM_WORLD_ONLY, -1, &trace );
		const char* texture = gEngfuncs.pEventAPI->EV_TraceTexture( trace.ent, trace.endpos, end );
		if ( texture && !strncmp( texture, "sky", 3 ) )
			CreateSnowFlake( origin );
	}
}

void CEnvironment::UpdateGrass( void )
{
	if ( !(m_flGrassTime <= gEngfuncs.GetClientTime() && (m_vWeatherOrigin - m_vGrassOrigin).Length() >= 100) )
		return;
	m_vGrassOrigin = m_vWeatherOrigin;
	m_flGrassTime = gEngfuncs.GetClientTime() + 0.3f;
	for ( int i = 0; i < 35.0 * m_flWeatherValue; ++i )
	{
		float distance = gEngfuncs.pfnRandomFloat( 800, 850 );
		int side = gEngfuncs.pfnRandomLong( 0, 5 );
		Vector origin = m_vGrassOrigin;
		origin.x += side > 2 ? distance : -distance;
		origin.y += gEngfuncs.pfnRandomFloat( -800, 800 );
		origin.z += 36;
		Vector end( origin.x, origin.y, -8000.0f );
		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( origin, end, PM_WORLD_ONLY, -1, &trace );
		float yaw = gEngfuncs.pfnRandomFloat( 0, 359 );
		const char* texture = gEngfuncs.pEventAPI->EV_TraceTexture( trace.ent, trace.endpos, end );
		if ( texture )
		{
			char textureName[64], materialName[64];
			strcpy( textureName, texture );
			char* name = textureName;
			if ( *name == '+' || *name == '-' )
				name += 2;
			if ( *name == '~' || *name == '{' || *name == ' ' || *name == '!' )
				++name;
			strcpy( materialName, name );
			materialName[12] = 0;
			if ( PM_FindTextureType( materialName ) == 'X' )
				CreateGrassPiece( trace.endpos, Vector( 0.0f, yaw, 0.0f ) );
		}
	}
}

void CEnvironment::CreateRaindrop( float* origin )
{
	if ( !g_Environment.GetRainSprite() )
		return;
	CPartRainDrop* particle = new ( std::nothrow ) CPartRainDrop;
	if ( !particle )
		return;
	float time = gEngfuncs.GetClientTime();
	particle->InitializeSprite( origin, Vector( 0, 0, 0 ), g_Environment.GetRainSprite(), 2, 1 );
	strcpy( particle->m_szClassname, "particle_rain" );
	particle->m_flStretchY = 40;
	particle->m_vVelocity.x = gEngfuncs.pfnRandomFloat( 1, 2 ) * m_vWind.x;
	particle->m_vVelocity.y = gEngfuncs.pfnRandomFloat( 1, 2 ) * m_vWind.y;
	particle->m_vVelocity.z = gEngfuncs.pfnRandomFloat( -500, -1800 );
	particle->SetCollisionFlags( TRI_COLLIDEWORLD | TRI_WATERTRACE | TRI_COLLIDEKILL );
	particle->m_flGravity = 0;
	particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_PLANE );
	particle->SetLightFlag( LIGHT_NONE );
	particle->m_iRendermode = kRenderTransAlpha;
	particle->m_vColor = Vector( 255, 255, 255 );
	particle->m_bTouched = false;
	particle->m_flDieTime = time + 1;
}

void CEnvironment::CreateSnowFlake( float* origin )
{
	if ( !g_Environment.GetSnowSprite() )
		return;
	CPartSnowFlake* particle = new ( std::nothrow ) CPartSnowFlake;
	if ( !particle )
		return;
	float size = gEngfuncs.pfnRandomFloat( 2, 2.5 );
	particle->InitializeSprite( origin, Vector( 0, 0, 0 ), g_Environment.GetSnowSprite(), size, 1 );
	strcpy( particle->m_szClassname, "snow_particle" );
	particle->m_iNumFrames = g_Environment.GetSnowSprite()->numframes;
	particle->m_vVelocity.x = m_vWind.x / gEngfuncs.pfnRandomFloat( 1, 2 );
	particle->m_vVelocity.y = m_vWind.y / gEngfuncs.pfnRandomFloat( 1, 2 );
	particle->m_vVelocity.z = gEngfuncs.pfnRandomFloat( -100, -200 );
	particle->SetCollisionFlags( TRI_COLLIDEWORLD );
	float variation = gEngfuncs.pfnRandomFloat( 0, 1 );
	if ( !(variation >= 0.1) )
	{
		particle->m_vVelocity.x *= 0.5;
		particle->m_vVelocity.y *= 0.5;
	}
	else if ( variation < 0.2 )
		particle->m_vVelocity.z = -65;
	else if ( variation < 0.3 )
		particle->m_vVelocity.z = -75;
	particle->m_iRendermode = kRenderTransAdd;
	particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
	particle->SetLightFlag( LIGHT_NONE );
	particle->SetRenderFlag( RENDER_FACEPLAYER );
	particle->m_flScaleSpeed = 0;
	particle->m_flDampingTime = 0;
	particle->m_iFrame = 0;
	particle->m_flMass = 1;
	particle->m_flGravity = 0;
	particle->m_flBounceFactor = 0;
	particle->m_vColor = Vector( 128, 128, 128 );
	particle->m_bTouched = false;
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 3;
	particle->m_bSpiral = gEngfuncs.pfnRandomLong( 0, 1 ) != 0;
	float time = gEngfuncs.GetClientTime();
	particle->m_flSpiralTime = time + gEngfuncs.pfnRandomLong( 2, 4 );
}

void CEnvironment::CreateGrassPiece( float* origin, Vector normal )
{
	if ( !g_pParticleMan )
		return;
	model_s* sprite = GetGrassSprites();
	if ( !sprite )
		return;
	float size = gEngfuncs.pfnRandomFloat( 60, 75 ) * 0.5;
	CPartGrassPiece* particle = new ( std::nothrow ) CPartGrassPiece;
	if ( !particle )
		return;
	origin[2] += 10;
	particle->InitializeSprite( origin, normal, sprite, size, 1 );
	particle->m_iAfterDampFlags = TRI_COLLIDEWORLD;
	particle->m_flGravity = 0;
	particle->m_iRendermode = kRenderTransAlpha;
	particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_PLANE );
	particle->SetLightFlag( LIGHT_COLOR );
	particle->m_vVelocity = Vector( 0, 0, 0 );
	particle->m_vAVelocity = Vector( 0, 0, 0 );
	particle->m_vAngles = normal;
	particle->m_flSize = size;
	particle->m_flFadeSpeed = 0;
	float time = gEngfuncs.GetClientTime();
	particle->m_vColor = Vector( 125, 125, 125 );
	particle->m_flFadeSpeed = -1;
	particle->m_flDieTime = time + 99999;
}

void CEnvironment::CreateWindParticle( float* origin )
{
	model_s* sprite = const_cast<model_s*>( gEngfuncs.GetSpritePointer( gEngfuncs.pfnSPR_Load( "sprites/gas_puff_01.spr" ) ) );
	if ( !sprite )
		return;
	CPartWind* particle = new ( std::nothrow ) CPartWind;
	if ( !particle )
		return;
	origin[2] += 10;
	float size = gEngfuncs.pfnRandomFloat( 50, 75 );
	particle->InitializeSprite( origin, Vector( 0, 0, 0 ), sprite, size, 1 );
	strcpy( particle->m_szClassname, "wind_particle" );
	particle->m_iNumFrames = sprite->numframes;
	particle->m_iFrame = gEngfuncs.pfnRandomLong( sprite->numframes / 2, sprite->numframes );
	particle->m_vVelocity.x = m_vWind.x / gEngfuncs.pfnRandomFloat( 1, 2 );
	particle->m_vVelocity.y = m_vWind.y / gEngfuncs.pfnRandomFloat( 1, 2 );
	particle->SetCollisionFlags( TRI_COLLIDEWORLD );
	particle->m_flGravity = 0;
	if ( gEngfuncs.pfnRandomFloat( 0, 1 ) < 0.1 )
	{
		particle->m_vVelocity.x *= 0.5;
		particle->m_vVelocity.y *= 0.5;
	}
	particle->m_iRendermode = kRenderTransAlpha;
	particle->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
	particle->SetLightFlag( LIGHT_NONE );
	particle->SetRenderFlag( RENDER_FACEPLAYER_ROTATEZ );
	particle->m_vAVelocity.z = gEngfuncs.pfnRandomFloat( -1, 1 );
	particle->m_flScaleSpeed = 0.4f;
	particle->m_flDampingTime = 0;
	particle->m_iFrame = 0;
	particle->m_flMass = 1;
	particle->m_flBounceFactor = 0;
	particle->m_vColor = Vector( 128, 128, 128 );
	particle->m_flFadeSpeed = -1;
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 6;
}

void CPartRainDrop::Think( float time )
{
	Vector angles, forward, right, up;
	gEngfuncs.GetViewAngles( angles );
	gEngfuncs.pfnAngleVectors( angles, forward, right, up );
	m_vAngles.y = angles.y;
	m_vAngles.z = atan( DotProduct( m_vVelocity, right ) / m_vVelocity.z ) * 57.29746936176985;
	if ( m_flBrightness < 155 )
		m_flBrightness += 6.5;
	CBaseParticle::Think( time );
}

void CPartRainDrop::Touch( Vector, Vector normal, int )
{
	if ( m_bTouched )
		return;
	m_bTouched = true;
	Vector start = m_vOrigin + Vector( 0, 0, 32 );
	Vector end = m_vOrigin - Vector( 0, 0, 16 );
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_WORLD_ONLY, -1, &trace );
	Vector rippleAngles;
	Vector direction( normal.x, normal.y, -normal.z );
	VectorAngles( direction, rippleAngles );
	Vector point;
	if ( WaterEntryPoint( &trace, start, point ) )
	{
		CBaseParticle* ripple = new ( std::nothrow ) CBaseParticle;
		if ( !ripple )
			return;
		ripple->InitializeSprite( point, rippleAngles, g_Environment.GetRippleSprite(), 15, 110 );
		ripple->m_iRendermode = kRenderTransAdd;
		ripple->m_flFadeSpeed = 2;
		ripple->m_flScaleSpeed = 1;
		ripple->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		ripple->SetLightFlag( LIGHT_INTENSITY );
		ripple->m_vColor = Vector( 255, 255, 255 );
		ripple->m_flDieTime = gEngfuncs.GetClientTime() + 2;
	}
	else
	{
		float time = gEngfuncs.GetClientTime();
		CBaseParticle* splash = new ( std::nothrow ) CBaseParticle;
		if ( !splash )
			return;
		float size = gEngfuncs.pfnRandomLong( 20, 25 );
		splash->InitializeSprite( m_vOrigin + normal, Vector( 90, 0, 0 ), g_Environment.GetSplashSprite(), size, 125 );
		splash->m_iRendermode = kRenderTransAdd;
		splash->m_flMass = 1;
		splash->m_flGravity = 0.1f;
		splash->SetCullFlag( CULL_PVS | CULL_FRUSTUM_SPHERE );
		splash->SetLightFlag( LIGHT_INTENSITY );
		splash->m_vColor = Vector( 255, 255, 255 );
		splash->m_iNumFrames = g_Environment.GetSplashSprite()->numframes - 1;
		splash->m_iFramerate = gEngfuncs.pfnRandomLong( 30, 45 );
		splash->m_flDieTime = time + 0.3;
		splash->SetCollisionFlags( TRI_ANIMATEDIE );
		splash->SetRenderFlag( RENDER_FACEPLAYER );
	}
}

void CPartSnowFlake::Think( float time )
{
	float oldTime = g_Environment.GetOldTime();
	if ( m_flBrightness < 130 && !m_bTouched )
		m_flBrightness += 4.5;
	CBaseParticle::Fade( time );
	CBaseParticle::Spin( time );
	if ( gEngfuncs.GetClientTime() >= m_flSpiralTime )
	{
		float currentTime = gEngfuncs.GetClientTime();
		int interval = gEngfuncs.pfnRandomLong( 2, 4 );
		m_bSpiral = !m_bSpiral;
		m_flSpiralTime = currentTime + interval;
	}
	if ( m_bSpiral && !m_bTouched )
	{
		float elapsed = time - oldTime;
		m_vOrigin.x += m_vVelocity.x * elapsed + sin( time * 5.0 + (int)(intptr_t)this ) * 2 * 0.3;
		m_vOrigin.y += m_vVelocity.y * elapsed;
		m_vOrigin.z += m_vVelocity.z * elapsed;
	}
	else
		CBaseParticle::CalculateVelocity( time );
	CBaseParticle::CheckCollision( time );
}

void CPartSnowFlake::Touch( Vector, Vector, int )
{
	if ( m_bTouched )
		return;
	m_flOriginalBrightness = m_flBrightness;
	SetRenderFlag( RENDER_FACEPLAYER );
	m_bTouched = true;
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
	m_flTimeCreated = gEngfuncs.GetClientTime();
}

void CPartGrassPiece::Think( float time )
{
	float distance = (m_vOrigin - Vector( gEngfuncs.GetLocalPlayer()->origin )).Length();
	if ( distance > 855 )
		m_flDieTime = time;
	if ( distance > 500 )
	{
		float brightness = 255 - fabs( 500 - distance );
		m_flBrightness = brightness > 0 ? brightness : 1;
	}
	CBaseParticle::Think( time );
}

void CPartWind::Think( float time )
{
	if ( m_flDieTime - time <= 3 )
	{
		if ( m_flBrightness > 0 )
			m_flBrightness -= (time - m_flTimeCreated) * 0.4;
		if ( m_flBrightness < 0 )
		{
			m_flBrightness = 0;
			m_flDieTime = gEngfuncs.GetClientTime();
		}
	}
	else if ( m_flBrightness < 105 )
		m_flBrightness += (time - m_flTimeCreated) * 5 + 4;
	CBaseParticle::Think( time );
}
