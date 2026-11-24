#include "pman_main.h"
#include "pman_frustum.h"
#include "pman_misc.h"

#undef vec3_t
#include "mathlib.h"
#include "triangleapi.h"
#include "event_api.h"
#include "pm_defs.h"

#include <math.h>
#include <string.h>

const Vector g_vecZero = Vector( 0, 0, 0 );

void CCoreTriangleEffect::InitializeSprite( Vector origin, Vector normal, model_s* sprite, float size,
		float brightness )
{
	m_flOriginalSize = 10;
	m_flSize = 10;
	m_flScaleSpeed = 0;
	m_flContractSpeed = 0;

	m_flStretchX = 1;
	m_flStretchY = 1;

	m_flNextPVSCheck = gEngfuncs.GetClientTime();
	SetParticlePVS( true );

	m_flOriginalBrightness = 255;
	m_flBrightness = 255;
	m_flFadeSpeed = -1;

	m_flDieTime = 0;

	m_iFramerate = 0;
	m_iNumFrames = 0;
	m_iFrame = 0;

	m_flDampingTime = 0;

	m_pTexture = NULL;

	m_flGravity = 0;

	m_iCollisionFlags = 0;

	m_flNextCollisionTime = 0;

	strcpy( m_szClassname, "particle" );

	m_iRendermode = kRenderNormal;

	m_iRenderFlags = 0;
	m_flBounceFactor = 1;

	m_bInWater = false;
	m_bAffectedByForce = false;

	Vector zero = Vector( 0, 0, 0 );

	m_vVelocity = zero;
	m_vOriginalAngles = zero;
	m_vAngles = zero;
	m_vAVelocity = zero;

	m_vColor.x = 255;
	m_vColor.y = 255;
	m_vColor.z = 255;

	m_flTimeCreated = gEngfuncs.GetClientTime();

	m_vOrigin = origin;
	m_vPrevOrigin = origin;
	m_vAngles = normal;

	m_pTexture = sprite;

	m_flOriginalSize = size;
	m_flSize = size;
	m_flOriginalBrightness = brightness;
	m_flBrightness = brightness;

	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( normal, forward, right, up );
	right = right * size;
	up = up * size;
	m_vLowLeft = origin - right * 0.5f - up * 0.5f;
	m_vLowRight = right + right + m_vLowLeft;
	m_vTopLeft = up + up + m_vLowLeft;

	m_vOriginalAngles = m_vAngles;
}

bool CCoreTriangleEffect::CheckVisibility()
{
	float radius = m_flSize / 5.0f;
	if ( gEngfuncs.GetClientTime() >= m_flNextPVSCheck )
	{
		Vector mins = m_vOrigin - Vector( radius, radius, radius );
		Vector maxs = m_vOrigin + Vector( radius, radius, radius );
		m_bInPVS = gEngfuncs.pTriAPI->BoxInPVS( mins, maxs ) != 0;
		m_flNextPVSCheck = gEngfuncs.GetClientTime() + 0.1f;
	}

	if ( m_iRenderFlags & CULL_FRUSTUM_SPHERE )
	{
		if ( !g_cFrustum.SphereInsideFrustum( m_vOrigin.x, m_vOrigin.y, m_vOrigin.z, radius ) )
			return false;
	}
	else if ( m_iRenderFlags & CULL_FRUSTUM_PLANE )
	{
		if ( !g_cFrustum.PlaneInsideFrustum( m_vOrigin.x, m_vOrigin.y, m_vOrigin.z, radius ) )
			return false;
	}
	else if ( m_iRenderFlags & CULL_FRUSTUM_POINT )
	{
		if ( !g_cFrustum.PointInsideFrustum( m_vOrigin.x, m_vOrigin.y, m_vOrigin.z ) )
			return false;
	}

	return m_bInPVS || !( m_iRenderFlags & CULL_PVS );
}

void CCoreTriangleEffect::Draw()
{
	float size = m_flSize;

	if ( m_flDieTime == gEngfuncs.GetClientTime() )
		return;

	Vector light, color;
	float intensity = 0;

	if ( !( m_iRenderFlags & LIGHT_NONE ) )
	{
		gEngfuncs.pTriAPI->LightAtPoint( m_vOrigin, light );
		intensity = ( light.x + light.y + light.z ) / 3.0f;
	}

	if ( m_iRenderFlags & ( RENDER_FACEPLAYER | RENDER_FACEPLAYER_ROTATEZ ) )
	{
		m_vAngles.x = g_vViewAngles.x;
		m_vAngles.y = g_vViewAngles.y;

		if ( !( m_iRenderFlags & RENDER_FACEPLAYER_ROTATEZ ) )
			m_vAngles.z = g_vViewAngles.z;
	}

	if ( m_iRenderFlags & LIGHT_NONE )
	{
		color = m_vColor;
	}
	else if ( m_iRenderFlags & LIGHT_COLOR )
	{
		color.x = light.x / m_vColor.x * 255.0f;
		color.y = light.y / m_vColor.y * 255.0f;
		color.z = light.z / m_vColor.z * 255.0f;
	}
	else if ( m_iRenderFlags & LIGHT_INTENSITY )
	{
		color.x = intensity / m_vColor.x * 255.0f;
		color.y = intensity / m_vColor.y * 255.0f;
		color.z = intensity / m_vColor.z * 255.0f;
	}
	else
	{
		color = m_vColor;
	}

	if ( color.x > 255 )
		color.x = 255;

	if ( color.y > 255 )
		color.y = 255;

	if ( color.z > 255 )
		color.z = 255;

	if ( color.x < 0 )
		color.x = 0;

	if ( color.y < 0 )
		color.y = 0;

	if ( color.z < 0 )
		color.z = 0;

	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors( m_vAngles, forward, right, up );
	right = right * ( size * m_flStretchX );
	Vector lowLeft = m_vOrigin - right * 0.5f - up * size * 0.5f;
	Vector lowRight = lowLeft + right;
	up = up * ( size * m_flStretchY );
	Vector topLeft = lowLeft + up;
	Vector topRight = lowRight + up;

	gEngfuncs.pTriAPI->SpriteTexture( m_pTexture, m_iFrame );
	gEngfuncs.pTriAPI->RenderMode( m_iRendermode );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	gEngfuncs.pTriAPI->Color4f( color.x / 255.0f, color.y / 255.0f, color.z / 255.0f, m_flBrightness / 255.0f );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
	gEngfuncs.pTriAPI->Vertex3fv( topLeft );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
	gEngfuncs.pTriAPI->Vertex3fv( lowLeft );
	gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
	gEngfuncs.pTriAPI->Vertex3fv( lowRight );
	gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
	gEngfuncs.pTriAPI->Vertex3fv( topRight );
	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
}

void CCoreTriangleEffect::Animate( float time )
{
	if ( !m_iFramerate || !m_iNumFrames )
		return;

	float frame = ( ( time - m_flTimeCreated ) * m_iFramerate ) + 0.0f;
	m_iFrame = (int)frame % m_iNumFrames;
}

void CCoreTriangleEffect::AnimateAndDie( float time )
{
	if ( !m_iFramerate || !m_iNumFrames )
		return;

	float frame = ( ( time - m_flTimeCreated ) * m_iFramerate ) + 0.0f;

	if ( frame > m_iNumFrames )
		m_flDieTime = time;
	else
		m_iFrame = (int)frame;

	if ( m_iFrame > m_iNumFrames )
		m_iFrame = m_iNumFrames - 1;
}

void CCoreTriangleEffect::Expand( float time )
{
	if ( m_flScaleSpeed == 0 )
		return;

	m_flSize = m_flOriginalSize + ( time - m_flTimeCreated ) * ( m_flScaleSpeed * TRIANGLE_FPS );
	if ( m_flSize < 0.0001f )
		m_flDieTime = time;
}

void CCoreTriangleEffect::Contract( float time )
{
	if ( m_flContractSpeed == 0 )
		return;

	m_flSize = m_flOriginalSize - ( time - m_flTimeCreated ) * ( m_flContractSpeed * TRIANGLE_FPS );
	if ( m_flSize < 0.0001f )
		m_flDieTime = time;
}

void CCoreTriangleEffect::Fade( float time )
{
	if ( m_flFadeSpeed < -0.5f )
		return;

	if ( m_flFadeSpeed == 0 )
		m_flBrightness = ( 1.0f - ( time - m_flTimeCreated ) / ( m_flDieTime - m_flTimeCreated ) )
			* m_flOriginalBrightness;
	else
		m_flBrightness = m_flOriginalBrightness - ( time - m_flTimeCreated ) * ( m_flFadeSpeed * TRIANGLE_FPS );

	if ( m_flBrightness < 1 )
		m_flDieTime = time;
}

void CCoreTriangleEffect::Spin( float time )
{
	if ( m_vAVelocity == Vector( 0, 0, 0 ) )
		return;

	m_vAngles = m_vAngles + m_vAVelocity;

	Vector point = m_vAVelocity;
	float scale = VectorNormalize( point );
	float degrees = ( time - m_flTimeCreated ) * ( scale * TRIANGLE_FPS );
	m_vAngles = m_vOriginalAngles + m_vAVelocity * degrees;
}

void CCoreTriangleEffect::CalculateVelocity( float time )
{
	float frametime = time - g_flOldTime;
	float gravity = -frametime * g_flGravity * m_flGravity;

	if ( m_vVelocity == Vector( 0, 0, 0 ) && gravity == 0 )
		return;

	if ( m_iCollisionFlags & TRI_SPIRAL )
	{
		float fastFreq = time * 5;

		m_vOrigin.x = (float)( 2.0 * sin( (double)( fastFreq + (int)this ) ) + m_vVelocity.x * frametime + m_vOrigin.x );
		m_vOrigin.y = (float)( sin( time * 7.5 + (int)this ) + m_vVelocity.y * frametime + m_vOrigin.y );
		m_vOrigin.z += m_vVelocity.z * frametime;
	}
	else
	{
		for ( int i = 0; i < 3; i++ )
			m_vOrigin[i] += m_vVelocity[i] * frametime;
	}

	m_vVelocity.z += gravity;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4701 )
#endif
void CCoreTriangleEffect::CheckCollision( float time )
{
	if ( m_iCollisionFlags == 0 )
		return;

	m_flNextCollisionTime = time;

	if ( !( m_iCollisionFlags & ( TRI_COLLIDEWORLD | TRI_COLLIDEALL | TRI_WATERTRACE ) ) )
		return;

	Vector traceNormal;
	float traceFraction = 1;
	Vector endpos;
	int entIndex;

	if ( m_iCollisionFlags & TRI_COLLIDEALL )
	{
		pmtrace_t pmtrace;

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( m_vPrevOrigin, m_vOrigin, PM_STUDIO_BOX, -1, &pmtrace );

		if ( pmtrace.fraction != 1 )
		{
			gEngfuncs.pEventAPI->EV_GetPhysent( pmtrace.ent );
			entIndex = pmtrace.ent;
			traceFraction = pmtrace.fraction;
			traceNormal = Vector( pmtrace.plane.normal );
			endpos = Vector( pmtrace.endpos );
		}
	}
	else if ( m_iCollisionFlags & TRI_COLLIDEWORLD )
	{
		pmtrace_t pmtrace;

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( m_vPrevOrigin, m_vOrigin, PM_STUDIO_BOX | PM_WORLD_ONLY, -1, &pmtrace );

		traceFraction = pmtrace.fraction;
		if ( traceFraction != 1 )
		{
			traceNormal = Vector( pmtrace.plane.normal );

			VectorScale( m_vVelocity, 0.6f, m_vVelocity );

			if ( m_vVelocity.Length() < 10 )
			{
				m_iCollisionFlags = 0;
				m_vVelocity = g_vecZero;
				m_vAVelocity = g_vecZero;
				m_vOrigin = Vector( pmtrace.endpos );
			}

			endpos = Vector( pmtrace.endpos );
			entIndex = pmtrace.ent;
		}
	}

	if ( traceFraction != 1 )
	{
		float proj, damp;
		float frametime = time - g_flOldTime;
		float gravity = -frametime * g_flGravity * m_flGravity;

		VectorMA( m_vPrevOrigin, frametime * traceFraction, m_vVelocity, m_vOrigin );

		damp = m_flBounceFactor * 0.5f;

		if ( traceNormal.z > 0.9f && m_vVelocity.z <= 0 && m_vVelocity.z >= gravity * 3 )
		{
			damp = 0;

			for ( int i = 0; i < 3; i++ )
			{
				m_vVelocity[i] = 0;
				m_vAVelocity[i] = 0;
			}

			m_iCollisionFlags = 0;
		}

		if ( m_iCollisionFlags & TRI_COLLIDEKILL )
		{
			m_flDieTime = gEngfuncs.GetClientTime();
		}
		else
		{
			if ( damp != 0 )
			{
				proj = (float)( -2.0 * ( m_vVelocity.x * traceNormal.x + m_vVelocity.y * traceNormal.y
					+ m_vVelocity.z * traceNormal.z ) );
				m_vVelocity = m_vVelocity + traceNormal * proj;
				m_vAngles.y = -m_vAngles.y;
			}

			if ( damp != 1 )
			{
				m_vVelocity = m_vVelocity * damp;
				m_vAngles = m_vAngles * 0.9f;
			}
		}

		Touch( endpos, traceNormal, entIndex );
	}
	else if ( m_iCollisionFlags & TRI_WATERTRACE )
	{
		int contents = gEngfuncs.PM_PointContents( m_vOrigin, NULL );

		if ( contents == CONTENTS_WATER && !m_bInWater )
		{
			Touch( m_vOrigin, Vector( 0, 0, 1 ), 0 );
			m_bInWater = true;

			if ( m_iCollisionFlags & TRI_COLLIDEKILL )
				m_flDieTime = gEngfuncs.GetClientTime();
		}
	}

	m_vPrevOrigin = m_vOrigin;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

void CCoreTriangleEffect::Touch( Vector, Vector, int )
{
}

void CCoreTriangleEffect::Die()
{
}

void CCoreTriangleEffect::Force()
{
}

void CCoreTriangleEffect::Think( float time )
{
	if ( m_iCollisionFlags & TRI_ANIMATEDIE )
		AnimateAndDie( time );
	else
		Animate( time );

	Contract( time );
	Expand( time );
	Fade( time );
	Spin( time );
	CalculateVelocity( time );
	CheckCollision( time );
}
