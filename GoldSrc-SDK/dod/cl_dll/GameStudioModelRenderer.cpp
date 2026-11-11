//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "studio_util.h"
#include "r_studioint.h"
#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
#include "dod_shared.h"
#include "Exports.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

//
// Override the StudioModelRender virtual member functions here to implement custom bone
// setup, blending, etc.
//

// Global engine <-> studio model rendering code interface
extern engine_studio_api_t IEngineStudio;
void DoD_GetSequence( int* seq, int* gaitseq );
void DoD_GetOrientation( float* origin, float* angles );

float g_flStartScaleTime;
int iPrevRenderState;
int iRenderStateChanged;
int iNumBipodAnims;
int iNumSandbagAnims;
int iBipodAnims[64];
int iSandbagAnims[64];

static client_anim_state_t g_state;
static client_anim_state_t g_clientstate;

// The renderer object, created on the stack.
CGameStudioModelRenderer g_StudioRenderer;

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CGameStudioModelRenderer::CGameStudioModelRenderer()
{
}

void AddToBipodAnimTable( int seq )
{
	if ( iNumBipodAnims < 64 )
		iBipodAnims[iNumBipodAnims++] = seq;
}

void AddToSandbagAnimTable( int seq )
{
	if ( iNumSandbagAnims < 64 )
		iSandbagAnims[iNumSandbagAnims++] = seq;
}

void BuildDeployAnimTable( void* pmodel )
{
	if ( !pmodel )
		return;
	iNumBipodAnims = 0;
	iNumSandbagAnims = 0;
	studiohdr_t* pHeader = (studiohdr_t*)pmodel;
	mstudioseqdesc_t* pSeq = (mstudioseqdesc_t*)( (byte*)pHeader + pHeader->seqindex );
	for ( int i = 0; i < pHeader->numseq; i++, pSeq++ )
	{
		if ( !memcmp( pSeq->label, "bipod_", 6 ) )
			AddToBipodAnimTable( i );
		else if ( !memcmp( pSeq->label, "sandbag_", 8 ) )
			AddToSandbagAnimTable( i );
	}
}

bool IsBipodAnimation( int seq, void* pmodel )
{
	if ( !iNumBipodAnims )
		BuildDeployAnimTable( pmodel );
	for ( int i = 0; i < iNumBipodAnims; i++ )
	{
		if ( iBipodAnims[i] == seq )
			return true;
	}
	return false;
}

bool IsSandbagAnimation( int seq, void* pmodel )
{
	if ( !iNumSandbagAnims )
		BuildDeployAnimTable( pmodel );
	for ( int i = 0; i < iNumSandbagAnims; i++ )
	{
		if ( iSandbagAnims[i] == seq )
			return true;
	}
	return false;
}

mstudioanim_t* CGameStudioModelRenderer::LookupAnimation( mstudioseqdesc_t* pseqdesc, int index )
{
	mstudioanim_t* pAnim = StudioGetAnim( m_pRenderModel, pseqdesc );
	if ( index >= 0 && index < pseqdesc->numblends )
		pAnim += m_pStudioHeader->numbones * index;
	return pAnim;
}

void CGameStudioModelRenderer::StudioPlayerBlend( mstudioseqdesc_t*, int* pBlend, float* pPitch )
{
	float pitch = 3 * *pPitch;
	*pBlend = (int)pitch;
	if ( *pBlend <= -45 )
		*pBlend = 255;
	else if ( *pBlend >= 45 )
		*pBlend = 0;
	else
	{
		float blend = ( 45.0 - *pBlend ) * 255.0 / 90.0;
		*pBlend = (int)blend;
	}
	*pPitch = 0;
}

void CGameStudioModelRenderer::CalculatePitchBlend( entity_state_t* )
{
	mstudioseqdesc_t* pSeq = (mstudioseqdesc_t*)( (byte*)m_pStudioHeader + m_pStudioHeader->seqindex )
		+ m_pCurrentEntity->curstate.sequence;
	int blend;
	StudioPlayerBlend( pSeq, &blend, &m_pCurrentEntity->angles[PITCH] );
	m_pCurrentEntity->latched.prevangles[PITCH] = m_pCurrentEntity->angles[PITCH];
	m_pCurrentEntity->curstate.blending[1] = (byte)blend;
	m_pCurrentEntity->latched.prevblending[1] = m_pCurrentEntity->curstate.blending[1];
	m_pCurrentEntity->latched.prevseqblending[1] = m_pCurrentEntity->curstate.blending[1];
}

void CGameStudioModelRenderer::CalculateYawBlend( entity_state_t* pplayer )
{
	StudioEstimateGait( pplayer );
	if ( m_pPlayerInfo->gaitsequence == 15 || m_pPlayerInfo->gaitsequence == 16
		|| m_pCurrentEntity->curstate.sequence == 17 || m_pCurrentEntity->curstate.sequence == 18 )
		return;
	float yaw = (float)fmod( m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw, 360.0 );
	if ( yaw < -180 )
		yaw += 360;
	else if ( yaw > 180 )
		yaw -= 360;
	if ( m_flGaitMovement != 0 )
	{
		if ( yaw > 120 )
		{
			m_pPlayerInfo->gaityaw -= 180;
			m_flGaitMovement = -m_flGaitMovement;
			yaw -= 180;
		}
		else if ( yaw < -120 )
		{
			m_pPlayerInfo->gaityaw += 180;
			m_flGaitMovement = -m_flGaitMovement;
			yaw += 180;
		}
	}
	float blend = (float)( yaw / 90.0 * 128.0 + 127.0 );
	if ( blend > 255 )
		m_pCurrentEntity->curstate.blending[0] = 0;
	else if ( blend < 0 )
		m_pCurrentEntity->curstate.blending[0] = 255;
	else
	{
		float inverted = 255 - blend;
		m_pCurrentEntity->curstate.blending[0] = (byte)(int)inverted;
	}
	m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->curstate.blending[0];
	m_pCurrentEntity->latched.prevseqblending[0] = m_pCurrentEntity->curstate.blending[0];
	m_pCurrentEntity->angles[YAW] = m_pPlayerInfo->gaityaw;
	if ( m_pCurrentEntity->angles[YAW] < 0 )
		m_pCurrentEntity->angles[YAW] += 360;
	m_pCurrentEntity->latched.prevangles[YAW] = m_pCurrentEntity->angles[YAW];
}

void CGameStudioModelRenderer::StudioEstimateGait( entity_state_t* pplayer )
{
	int sequence = m_pCurrentEntity->curstate.sequence;
	bool fixedYaw = m_pPlayerInfo->gaitsequence == 15 || m_pPlayerInfo->gaitsequence == 16
		|| sequence == 17 || sequence == 18;
	float dt = (float)( m_clTime - m_clOldTime );
	dt = max( 0.0f, min( 1.0f, dt ) );
	if ( dt == 0 || m_pPlayerInfo->renderframe == m_nFrameCount )
	{
		m_flGaitMovement = 0;
		return;
	}
	vec3_t velocity;
	if ( m_fGaitEstimation )
	{
		VectorSubtract( m_pCurrentEntity->origin, m_pPlayerInfo->prevgaitorigin, velocity );
		VectorCopy( m_pCurrentEntity->origin, m_pPlayerInfo->prevgaitorigin );
		m_flGaitMovement = Length( velocity );
		if ( dt <= 0 || m_flGaitMovement / dt < 5 )
		{
			m_flGaitMovement = 0;
			velocity[0] = velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy( pplayer->velocity, velocity );
		m_flGaitMovement = Length( velocity ) * dt;
	}
	void* pModel = IEngineStudio.Mod_Extradata( m_pCurrentEntity->model );
	if ( IsSandbagAnimation( m_pCurrentEntity->curstate.sequence, pModel ) )
	{
		m_pPlayerInfo->gaityaw = m_pCurrentEntity->angles[YAW];
		return;
	}
	if ( velocity[0] == 0 && velocity[1] == 0 )
	{
		if ( ( sequence < 19 || sequence > 24 ) && !fixedYaw )
		{
			float yaw = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
			float quotient = yaw / 360;
			float diff = yaw - (int)quotient * 360;
			if ( diff > 180 )
				diff -= 360;
			if ( diff < -180 )
				diff += 360;
			float wrapped = (float)fmod( yaw, 360.0 );
			if ( wrapped < -180 )
				wrapped += 360;
			else if ( wrapped > 180 )
				wrapped -= 360;
			if ( wrapped > -5 && wrapped < 5 )
				m_pCurrentEntity->baseline.fuser1 = 0.05f;
			else if ( wrapped < -90 || wrapped > 90 )
				m_pCurrentEntity->baseline.fuser1 = 3.5f;
			else
				m_pCurrentEntity->baseline.fuser1 = 0;
			float yawDiff = dt < 0.25 ? diff * ( dt * m_pCurrentEntity->baseline.fuser1 ) : diff * dt;
			if ( abs( (int)yawDiff ) < 0.1 )
				yawDiff = 0;
			m_pPlayerInfo->gaityaw += yawDiff;
			quotient = m_pPlayerInfo->gaityaw / 360;
			m_pPlayerInfo->gaityaw -= (int)quotient * 360;
			m_flGaitMovement = 0;
		}
		else
		{
			m_pPlayerInfo->gaityaw += m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
		}
	}
	else if ( !fixedYaw )
	{
		m_pPlayerInfo->gaityaw = (float)( atan2( velocity[1], velocity[0] ) * 180 / M_PI );
		m_pPlayerInfo->gaityaw = max( -180.0f, min( 180.0f, m_pPlayerInfo->gaityaw ) );
	}
}

void CGameStudioModelRenderer::StudioProcessGait( entity_state_t* pplayer )
{
	CalculateYawBlend( pplayer );
	CalculatePitchBlend( pplayer );
	float dt = (float)( m_clTime - m_clOldTime );
	dt = max( 0.0f, min( 1.0f, dt ) );
	mstudioseqdesc_t* pSeq = (mstudioseqdesc_t*)( (byte*)m_pStudioHeader + m_pStudioHeader->seqindex )
		+ pplayer->gaitsequence;
	if ( pSeq->linearmovement[0] > 0 )
		m_pPlayerInfo->gaitframe += m_flGaitMovement / pSeq->linearmovement[0] * pSeq->numframes;
	else
		m_pPlayerInfo->gaitframe += dt * pSeq->fps * m_pCurrentEntity->curstate.framerate;
	float frames = m_pPlayerInfo->gaitframe / pSeq->numframes;
	m_pPlayerInfo->gaitframe -= (int)frames * pSeq->numframes;
	if ( m_pPlayerInfo->gaitframe < 0 )
		m_pPlayerInfo->gaitframe += pSeq->numframes;
}

void CGameStudioModelRenderer::StudioFxTransform( cl_entity_t* ent, float transform[3][4] )
{
	if ( ent->curstate.renderfx == kRenderFxDistort || ent->curstate.renderfx == kRenderFxHologram )
	{
		if ( gEngfuncs.pfnRandomLong( 0, 49 ) == 0 )
		{
			int axis = gEngfuncs.pfnRandomLong( 0, 1 );
			if ( axis == 1 )
				axis = 2;
			float scale = gEngfuncs.pfnRandomFloat( 1, 1.484f );
			VectorScale( transform[axis], scale, transform[axis] );
		}
		else if ( gEngfuncs.pfnRandomLong( 0, 49 ) == 0 )
		{
			gEngfuncs.pfnRandomLong( 0, 1 );
			float offset = gEngfuncs.pfnRandomFloat( -10, 10 );
			int axis = gEngfuncs.pfnRandomLong( 0, 2 );
			transform[axis][3] += offset;
		}
	}
	else if ( ent->curstate.renderfx == kRenderFxExplode )
	{
		if ( iRenderStateChanged )
		{
			iRenderStateChanged = 0;
			g_flStartScaleTime = (float)m_clTime;
		}
		float delta = (float)( m_clTime - g_flStartScaleTime );
		if ( delta > 0 )
		{
			float scale = delta > 2 ? 0.001f : (float)( 1.0 - delta * 0.5 );
			for ( int i = 0; i < 3; i++ )
				VectorScale( transform[i], scale, transform[i] );
		}
	}
}

void CGameStudioModelRenderer::SavePlayerState( entity_state_t* pplayer )
{
	cl_entity_t* ent = IEngineStudio.GetCurrentEntity();
	if ( !ent )
		return;
	VectorCopy( ent->curstate.angles, g_state.angles );
	VectorCopy( ent->curstate.origin, g_state.origin );
	VectorCopy( ent->angles, g_state.realangles );
	g_state.sequence = ent->curstate.sequence;
	g_state.gaitsequence = pplayer->gaitsequence;
	g_state.animtime = ent->curstate.animtime;
	g_state.frame = ent->curstate.frame;
	g_state.framerate = ent->curstate.framerate;
	memcpy( g_state.blending, ent->curstate.blending, sizeof( g_state.blending ) );
	memcpy( g_state.controller, ent->curstate.controller, sizeof( g_state.controller ) );
	g_state.lv = ent->latched;
}

void GetSequenceInfo( void* pmodel, client_anim_state_t* state, float* frameRate, float* groundSpeed )
{
	if ( !pmodel )
		return;
	studiohdr_t* pHeader = (studiohdr_t*)pmodel;
	if ( state->sequence >= pHeader->numseq || state->sequence < 0 )
	{
		*frameRate = *groundSpeed = 0;
		return;
	}
	mstudioseqdesc_t* pSeq = (mstudioseqdesc_t*)( (byte*)pHeader + pHeader->seqindex ) + state->sequence;
	if ( pSeq->numframes > 1 )
	{
		*frameRate = 256 * pSeq->fps / ( pSeq->numframes - 1 );
		*groundSpeed = Length( pSeq->linearmovement );
		*groundSpeed = *groundSpeed * pSeq->fps / ( pSeq->numframes - 1 );
	}
	else
	{
		*frameRate = 256;
		*groundSpeed = 0;
	}
}

int GetSequenceFlags( void* pmodel, client_anim_state_t* state )
{
	if ( !pmodel )
		return 0;
	studiohdr_t* pHeader = (studiohdr_t*)pmodel;
	if ( state->sequence >= pHeader->numseq || state->sequence < 0 )
		return 0;
	mstudioseqdesc_t* pSeq = (mstudioseqdesc_t*)( (byte*)pHeader + pHeader->seqindex ) + state->sequence;
	return pSeq->flags;
}

float StudioFrameAdvance( client_anim_state_t* state, float frameRate, float interval )
{
	if ( interval == 0 )
	{
		interval = gEngfuncs.GetClientTime() - state->animtime;
		if ( interval <= 0.001 )
		{
			state->animtime = gEngfuncs.GetClientTime();
			return 0;
		}
	}
	if ( state->animtime == 0 )
		interval = 0;
	state->frame += frameRate * interval * state->framerate;
	state->animtime = gEngfuncs.GetClientTime();
	if ( state->frame < 0 || state->frame >= 256 )
	{
		if ( state->m_fSequenceLoops )
			state->frame -= (int)( state->frame / 256 ) * 256;
		else
			state->frame = state->frame < 0 ? 0 : 255;
		state->m_fSequenceFinished = 1;
	}
	return interval;
}

void CGameStudioModelRenderer::SetupClientAnimation( entity_state_t* pplayer )
{
	cl_entity_t* ent = IEngineStudio.GetCurrentEntity();
	if ( !ent )
		return;
	static double oldtime;
	double time = gEngfuncs.GetClientTime();
	double interval = max( 0.0, min( 1.0, time - oldtime ) );
	oldtime = time;
	int sequence = g_clientstate.sequence;
	g_clientstate.framerate = 1;
	DoD_GetSequence( &g_clientstate.sequence, &g_clientstate.gaitsequence );
	DoD_GetOrientation( g_clientstate.origin, g_clientstate.angles );
	g_clientstate.realangles = g_clientstate.angles;
	if ( g_clientstate.sequence != sequence )
	{
		g_clientstate.frame = 0;
		g_clientstate.lv.prevsequence = sequence;
		g_clientstate.lv.sequencetime = g_clientstate.animtime;
		memcpy( g_clientstate.lv.prevseqblending, g_clientstate.blending, sizeof( g_clientstate.blending ) );
		memcpy( g_clientstate.lv.prevcontroller, g_clientstate.controller, sizeof( g_clientstate.controller ) );
	}
	void* pModel = IEngineStudio.Mod_Extradata( ent->model );
	float frameRate, groundSpeed;
	GetSequenceInfo( pModel, &g_clientstate, &frameRate, &groundSpeed );
	g_clientstate.m_fSequenceLoops = GetSequenceFlags( pModel, &g_clientstate ) & STUDIO_LOOPING;
	StudioFrameAdvance( &g_clientstate, frameRate, (float)interval );
	VectorCopy( g_clientstate.realangles, ent->angles );
	VectorCopy( g_clientstate.angles, ent->curstate.angles );
	VectorCopy( g_clientstate.origin, ent->curstate.origin );
	ent->curstate.sequence = g_clientstate.sequence;
	pplayer->gaitsequence = g_clientstate.gaitsequence;
	ent->curstate.frame = g_clientstate.frame;
	ent->curstate.animtime = g_clientstate.animtime;
	ent->curstate.framerate = g_clientstate.framerate;
	memcpy( ent->curstate.blending, g_clientstate.blending, sizeof( g_clientstate.blending ) );
	memcpy( ent->curstate.controller, g_clientstate.controller, sizeof( g_clientstate.controller ) );
	ent->latched = g_clientstate.lv;
}

void CGameStudioModelRenderer::RestorePlayerState( entity_state_t* pplayer )
{
	cl_entity_t* ent = IEngineStudio.GetCurrentEntity();
	if ( !ent )
		return;
	VectorCopy( ent->curstate.angles, g_clientstate.angles );
	VectorCopy( ent->curstate.origin, g_clientstate.origin );
	VectorCopy( ent->angles, g_clientstate.realangles );
	g_clientstate.sequence = ent->curstate.sequence;
	g_clientstate.gaitsequence = pplayer->gaitsequence;
	g_clientstate.animtime = ent->curstate.animtime;
	g_clientstate.frame = ent->curstate.frame;
	g_clientstate.framerate = ent->curstate.framerate;
	memcpy( g_clientstate.blending, ent->curstate.blending, sizeof( g_clientstate.blending ) );
	memcpy( g_clientstate.controller, ent->curstate.controller, sizeof( g_clientstate.controller ) );
	g_clientstate.lv = ent->latched;
	VectorCopy( g_state.angles, ent->curstate.angles );
	VectorCopy( g_state.origin, ent->curstate.origin );
	VectorCopy( g_state.realangles, ent->angles );
	ent->curstate.sequence = g_state.sequence;
	pplayer->gaitsequence = g_state.gaitsequence;
	ent->curstate.animtime = g_state.animtime;
	ent->curstate.frame = g_state.frame;
	ent->curstate.framerate = g_state.framerate;
	memcpy( ent->curstate.blending, g_state.blending, sizeof( g_state.blending ) );
	memcpy( ent->curstate.controller, g_state.controller, sizeof( g_state.controller ) );
	ent->latched = g_state.lv;
}

void CGameStudioModelRenderer::StudioSetupBones()
{
	if ( !m_pCurrentEntity->player )
	{
		CStudioModelRenderer::StudioSetupBones();
		return;
	}
	if ( m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq || m_pCurrentEntity->curstate.sequence < 0 )
		m_pCurrentEntity->curstate.sequence = 0;
	mstudioseqdesc_t* pSeq = (mstudioseqdesc_t*)( (byte*)m_pStudioHeader + m_pStudioHeader->seqindex )
		+ m_pCurrentEntity->curstate.sequence;
	mstudioanim_t* pAnim = StudioGetAnim( m_pRenderModel, pSeq );
	float frame = StudioEstimateFrame( pSeq );
	static float pos[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES];
	static float pos2[MAXSTUDIOBONES][3];
	static vec4_t q2[MAXSTUDIOBONES];
	static float pos3[MAXSTUDIOBONES][3];
	static vec4_t q3[MAXSTUDIOBONES];
	static float pos4[MAXSTUDIOBONES][3];
	static vec4_t q4[MAXSTUDIOBONES];
	if ( pSeq->numblends == 9 )
	{
		float s = m_pCurrentEntity->curstate.blending[0];
		float t = m_pCurrentEntity->curstate.blending[1];
		int index = 0;
		if ( s > 127 )
		{
			index++;
			s = 2 * ( s - 127 );
		}
		else
			s *= 2;
		if ( t > 127 )
		{
			index += 3;
			t = 2 * ( t - 127 );
		}
		else
			t *= 2;
		if ( index )
			pAnim = LookupAnimation( pSeq, index );
		StudioCalcRotations( pos, q, pSeq, pAnim, frame );
		pAnim = LookupAnimation( pSeq, index + 1 );
		StudioCalcRotations( pos2, q2, pSeq, pAnim, frame );
		pAnim = LookupAnimation( pSeq, index + 3 );
		StudioCalcRotations( pos3, q3, pSeq, pAnim, frame );
		pAnim = LookupAnimation( pSeq, index + 4 );
		StudioCalcRotations( pos4, q4, pSeq, pAnim, frame );
		s /= 255;
		StudioSlerpBones( q, pos, q2, pos2, s );
		StudioSlerpBones( q3, pos3, q4, pos4, s );
		t /= 255;
		StudioSlerpBones( q, pos, q3, pos3, t );
	}
	else
	{
		StudioCalcRotations( pos, q, pSeq, pAnim, frame );
		if ( pSeq->numblends > 1 )
		{
			pAnim += m_pStudioHeader->numbones;
			StudioCalcRotations( pos2, q2, pSeq, pAnim, frame );
			float dadt = StudioEstimateInterpolant();
			float s = (float)( 1.0 - ( m_pCurrentEntity->curstate.blending[1] * dadt
				+ ( 1.0 - dadt ) * m_pCurrentEntity->latched.prevblending[1] ) / 255.0 );
			StudioSlerpBones( q, pos, q2, pos2, s );
		}
	}
	if ( m_fDoInterp && m_pCurrentEntity->latched.sequencetime
		&& m_pCurrentEntity->latched.sequencetime + 0.2 > m_clTime
		&& m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq )
	{
		static float pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		pSeq = (mstudioseqdesc_t*)( (byte*)m_pStudioHeader + m_pStudioHeader->seqindex )
			+ m_pCurrentEntity->latched.prevsequence;
		pAnim = StudioGetAnim( m_pRenderModel, pSeq );
		if ( pSeq->numblends == 9 )
		{
			float s = m_pCurrentEntity->latched.prevseqblending[0];
			float t = m_pCurrentEntity->latched.prevseqblending[1];
			int index = 0;
			if ( s > 127 )
			{
				index++;
				s = 2 * ( s - 127 );
			}
			else
				s *= 2;
			if ( t > 127 )
			{
				index += 3;
				t = 2 * ( t - 127 );
			}
			else
				t *= 2;
			if ( index )
				pAnim = LookupAnimation( pSeq, index );
			StudioCalcRotations( pos1b, q1b, pSeq, pAnim, m_pCurrentEntity->latched.prevframe );
			pAnim = LookupAnimation( pSeq, index + 1 );
			StudioCalcRotations( pos2, q2, pSeq, pAnim, m_pCurrentEntity->latched.prevframe );
			pAnim = LookupAnimation( pSeq, index + 3 );
			StudioCalcRotations( pos3, q3, pSeq, pAnim, m_pCurrentEntity->latched.prevframe );
			pAnim = LookupAnimation( pSeq, index + 4 );
			StudioCalcRotations( pos4, q4, pSeq, pAnim, m_pCurrentEntity->latched.prevframe );
			s /= 255;
			StudioSlerpBones( q1b, pos1b, q2, pos2, s );
			StudioSlerpBones( q3, pos3, q4, pos4, s );
			t /= 255;
			StudioSlerpBones( q1b, pos1b, q3, pos3, t );
		}
		else
		{
			StudioCalcRotations( pos1b, q1b, pSeq, pAnim, m_pCurrentEntity->latched.prevframe );
			if ( pSeq->numblends > 1 )
			{
				pAnim += m_pStudioHeader->numbones;
				StudioCalcRotations( pos2, q2, pSeq, pAnim, m_pCurrentEntity->latched.prevframe );
				float s = m_pCurrentEntity->latched.prevseqblending[0] / 255.0f;
				StudioSlerpBones( q1b, pos1b, q2, pos2, s );
				if ( pSeq->numblends == 4 )
				{
					pAnim += m_pStudioHeader->numbones;
					StudioCalcRotations( pos3, q3, pSeq, pAnim, m_pCurrentEntity->latched.prevframe );
					pAnim += m_pStudioHeader->numbones;
					StudioCalcRotations( pos4, q4, pSeq, pAnim, m_pCurrentEntity->latched.prevframe );
					s = m_pCurrentEntity->latched.prevseqblending[0] / 255.0f;
					StudioSlerpBones( q3, pos3, q4, pos4, s );
					s = m_pCurrentEntity->latched.prevseqblending[1] / 255.0f;
					StudioSlerpBones( q1b, pos1b, q3, pos3, s );
				}
			}
		}
		float s = (float)( 1.0 - ( m_clTime - m_pCurrentEntity->latched.sequencetime ) / 0.2 );
		StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		m_pCurrentEntity->latched.prevframe = frame;
	}
	mstudiobone_t* pBones = (mstudiobone_t*)( (byte*)m_pStudioHeader + m_pStudioHeader->boneindex );
	int sequence = m_pCurrentEntity->curstate.sequence;
	if ( ( sequence < 19 || sequence > 24 ) && m_pPlayerInfo->gaitsequence > 0 && sequence != 7 && sequence != 8 )
	{
		if ( m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq )
			m_pPlayerInfo->gaitsequence = 0;
		pSeq = (mstudioseqdesc_t*)( (byte*)m_pStudioHeader + m_pStudioHeader->seqindex )
			+ m_pPlayerInfo->gaitsequence;
		pAnim = StudioGetAnim( m_pRenderModel, pSeq );
		StudioCalcRotations( pos2, q2, pSeq, pAnim, m_pPlayerInfo->gaitframe );
		for ( int i = 0; i < m_pStudioHeader->numbones && i < 8; i++ )
		{
			VectorCopy( pos2[i], pos[i] );
			memcpy( q[i], q2[i], sizeof( q[i] ) );
		}
	}
	float boneMatrix[3][4];
	for ( int i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		QuaternionMatrix( q[i], boneMatrix );
		boneMatrix[0][3] = pos[i][0];
		boneMatrix[1][3] = pos[i][1];
		boneMatrix[2][3] = pos[i][2];
		if ( pBones[i].parent == -1 )
		{
			if ( IEngineStudio.IsHardware() )
			{
				ConcatTransforms( *m_protationmatrix, boneMatrix, ( *m_pbonetransform )[i] );
				MatrixCopy( ( *m_pbonetransform )[i], ( *m_plighttransform )[i] );
			}
			else
			{
				ConcatTransforms( *m_paliastransform, boneMatrix, ( *m_pbonetransform )[i] );
				ConcatTransforms( *m_protationmatrix, boneMatrix, ( *m_plighttransform )[i] );
			}
			StudioFxTransform( m_pCurrentEntity, ( *m_pbonetransform )[i] );
		}
		else
		{
			ConcatTransforms( ( *m_pbonetransform )[pBones[i].parent], boneMatrix, ( *m_pbonetransform )[i] );
			ConcatTransforms( ( *m_plighttransform )[pBones[i].parent], boneMatrix, ( *m_plighttransform )[i] );
		}
	}
}

model_t* CGameStudioModelRenderer::SwapPWpnModels( cl_entity_t*, model_t* pWeaponModel )
{
	mstudioseqdesc_t* pSeq = (mstudioseqdesc_t*)( (byte*)m_pStudioHeader + m_pStudioHeader->seqindex )
		+ m_pCurrentEntity->curstate.sequence;
	float frame = StudioEstimateFrame( pSeq );
	if ( m_pCurrentEntity->index < 1 || m_pCurrentEntity->index > 63 )
		return pWeaponModel;
	pmodel_fx_t* pFx = &g_PModelFxInfo[m_pCurrentEntity->index];
	void* pModel = IEngineStudio.Mod_Extradata( m_pCurrentEntity->model );
	bool bipod = IsBipodAnimation( m_pCurrentEntity->curstate.sequence, pModel );
	bool sandbag = false;
	if ( !bipod )
		sandbag = IsSandbagAnimation( m_pCurrentEntity->curstate.sequence, pModel );
	for ( int i = 0; i < 41; i++ )
	{
		p_wpninfo_s* pInfo = &P_WpnInfo[i];
		int sequence = m_pCurrentEntity->curstate.sequence;
		bool switching = pFx->bSwitch && ( pFx->iSwitchFrame == -1 || pFx->iSwitchFrame > frame );
		const char* pTarget = NULL;
		if ( bipod || sandbag )
		{
			if ( switching && sequence == pFx->iSwitchSeq )
			{
				pTarget = bipod ? pInfo->pronereload : pInfo->sandbagreload;
				if ( !*pTarget || ( stricmp( pWeaponModel->name, bipod ? pInfo->bipoddown : pInfo->bipodup )
					&& stricmp( pWeaponModel->name, bipod ? pInfo->bipodup : pInfo->bipoddown ) ) )
					pTarget = NULL;
			}
			else if ( sequence != pFx->iSwitchSeq )
			{
				pFx->bSwitch = false;
				pFx->iSwitchFrame = -1;
				if ( pInfo->bipoddown[0] && ( !stricmp( pWeaponModel->name, bipod ? pInfo->pronereload : pInfo->sandbagreload )
					|| !stricmp( pWeaponModel->name, pInfo->bipodup ) ) )
					pTarget = pInfo->bipoddown;
			}
		}
		else if ( switching && sequence == pFx->iSwitchSeq )
		{
			if ( pInfo->pmodel[0] && !stricmp( pWeaponModel->name, pInfo->pmodel ) )
				pTarget = pInfo->leftreload;
		}
		else if ( sequence != pFx->iSwitchSeq )
		{
			pFx->bSwitch = false;
			pFx->iSwitchFrame = -1;
			if ( pInfo->leftreload[0] && !stricmp( pWeaponModel->name, pInfo->leftreload ) )
				pTarget = pInfo->pmodel;
		}
		if ( pTarget )
		{
			int modelIndex;
			gEngfuncs.CL_LoadModel( pTarget, &modelIndex );
			return IEngineStudio.GetModelByIndex( modelIndex );
		}
	}
	return pWeaponModel;
}

void CGameStudioModelRenderer::AnimatePWpnModels( cl_entity_t* )
{
}

void CGameStudioModelRenderer::StudioClientEvents()
{
	mstudioseqdesc_t* pSeq = (mstudioseqdesc_t*)( (byte*)m_pStudioHeader + m_pStudioHeader->seqindex )
		+ m_pCurrentEntity->curstate.sequence;
	if ( !pSeq->numevents )
		return;
	mstudioevent_t* pEvent = (mstudioevent_t*)( (byte*)m_pStudioHeader + pSeq->eventindex );
	float frame = StudioEstimateFrame( pSeq );
	float start = (float)( frame - m_pCurrentEntity->curstate.framerate * gHUD.m_flTimeDelta * pSeq->fps );
	if ( m_pCurrentEntity->latched.sequencetime == m_pCurrentEntity->curstate.animtime && !( pSeq->flags & STUDIO_LOOPING ) )
		start = -0.01f;
	if ( m_pCurrentEntity->curstate.sequence != m_pCurrentEntity->prevstate.sequence )
	{
		m_pCurrentEntity->baseline.iuser1 = 0;
		m_pCurrentEntity->baseline.iuser2 = 0;
		m_pCurrentEntity->baseline.iuser3 = 0;
		m_pCurrentEntity->baseline.iuser4 = 0;
	}
	for ( int i = 0; i < pSeq->numevents; i++, pEvent++ )
	{
		if ( pEvent->event < 5000 )
			continue;
		int* pFired = NULL;
		switch ( i )
		{
		case 0: pFired = &m_pCurrentEntity->baseline.iuser1; break;
		case 1: pFired = &m_pCurrentEntity->baseline.iuser2; break;
		case 2: pFired = &m_pCurrentEntity->baseline.iuser3; break;
		case 3: pFired = &m_pCurrentEntity->baseline.iuser4; break;
		default: break;
		}
		if ( pFired && m_pCurrentEntity->player )
		{
			if ( frame >= pEvent->frame && !*pFired )
			{
				HUD_StudioEvent( pEvent, m_pCurrentEntity );
				*pFired = 1;
			}
		}
		else if ( pEvent->frame > start && frame >= pEvent->frame )
		{
			if ( m_pCurrentEntity->player )
				gEngfuncs.Con_DPrintf( "firing event %d\n", pEvent->frame );
			HUD_StudioEvent( pEvent, m_pCurrentEntity );
		}
	}
}

int CGameStudioModelRenderer::StudioDrawPlayer( int flags, entity_state_t* pplayer )
{
	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );
	IEngineStudio.GetViewInfo( m_vRenderOrigin, m_vUp, m_vRight, m_vNormal );
	IEngineStudio.GetAliasScale( &m_fSoftwareXScale, &m_fSoftwareYScale );
	m_nPlayerIndex = pplayer->number - 1;
	if ( m_nPlayerIndex < 0 || m_nPlayerIndex >= gEngfuncs.GetMaxClients() )
		return 0;
	m_pRenderModel = IEngineStudio.SetupPlayerModel( m_nPlayerIndex );
	if ( !m_pRenderModel )
		return 0;
	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata( m_pRenderModel );
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );
	if ( m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq || m_pCurrentEntity->curstate.sequence < 0 )
		m_pCurrentEntity->curstate.sequence = 0;
	if ( pplayer->sequence >= m_pStudioHeader->numseq || pplayer->sequence < 0 )
		pplayer->sequence = 0;
	if ( m_pCurrentEntity->curstate.gaitsequence >= m_pStudioHeader->numseq || m_pCurrentEntity->curstate.gaitsequence < 0 )
		m_pCurrentEntity->curstate.gaitsequence = 0;
	if ( pplayer->gaitsequence >= m_pStudioHeader->numseq || pplayer->gaitsequence < 0 )
		pplayer->gaitsequence = 0;
	if ( pplayer->gaitsequence )
	{
		vec3_t angles;
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
		VectorCopy( m_pCurrentEntity->angles, angles );
		StudioProcessGait( pplayer );
		m_pPlayerInfo->gaitsequence = pplayer->gaitsequence;
		m_pPlayerInfo = NULL;
		StudioSetUpTransform( 0 );
		VectorCopy( angles, m_pCurrentEntity->angles );
	}
	else
	{
		memset( m_pCurrentEntity->curstate.controller, 127, sizeof( m_pCurrentEntity->curstate.controller ) );
		memcpy( m_pCurrentEntity->latched.prevcontroller, m_pCurrentEntity->curstate.controller,
			sizeof( m_pCurrentEntity->curstate.controller ) );
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
		CalculatePitchBlend( pplayer );
		CalculateYawBlend( pplayer );
		m_pPlayerInfo->gaitsequence = 0;
		StudioSetUpTransform( 0 );
	}
	if ( flags & STUDIO_RENDER )
	{
		if ( !IEngineStudio.StudioCheckBBox() )
			return 0;
		( *m_pModelsDrawn )++;
		( *m_pStudioModelCount )++;
		if ( !m_pStudioHeader->numbodyparts )
			return 1;
	}
	m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
	StudioSetupBones();
	StudioSaveBones();
	m_pPlayerInfo->renderframe = m_nFrameCount;
	m_pPlayerInfo = NULL;
	if ( flags & STUDIO_EVENTS )
	{
		StudioCalcAttachments();
		StudioClientEvents();
		if ( m_pCurrentEntity->index > 0 )
		{
			cl_entity_t* ent = gEngfuncs.GetEntityByIndex( m_pCurrentEntity->index );
			memcpy( ent->attachment, m_pCurrentEntity->attachment, sizeof( ent->attachment ) );
		}
	}
	if ( flags & STUDIO_RENDER )
	{
		alight_t lighting;
		vec3_t dir;
		lighting.plightvec = dir;
		IEngineStudio.StudioDynamicLight( m_pCurrentEntity, &lighting );
		IEngineStudio.StudioEntityLight( &lighting );
		IEngineStudio.StudioSetupLighting( &lighting );
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
		m_nTopColor = max( 0, min( 360, m_pPlayerInfo->topcolor ) );
		m_nBottomColor = max( 0, min( 360, m_pPlayerInfo->bottomcolor ) );
		IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );
		StudioRenderModel();
		m_pPlayerInfo = NULL;
		if ( pplayer->weaponmodel )
		{
			cl_entity_t saveent = *m_pCurrentEntity;
			model_t* pWeaponModel = IEngineStudio.GetModelByIndex( pplayer->weaponmodel );
			if ( pWeaponModel )
			{
				model_t* pModel = SwapPWpnModels( m_pCurrentEntity, pWeaponModel );
				if ( pModel )
					pWeaponModel = pModel;
			}
			m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata( pWeaponModel );
			IEngineStudio.StudioSetHeader( m_pStudioHeader );
			if ( m_pCurrentEntity->player )
			{
				pmodel_fx_t* pFx = &g_PModelFxInfo[m_pCurrentEntity->index];
				if ( pFx->bAnim )
				{
					if ( pplayer->sequence == pFx->iAnimSeq )
					{
						if ( pplayer->frame >= pFx->iAnimFrame )
						{
							m_pCurrentEntity->curstate.frame = (float)pFx->iAnimFrame;
							m_pCurrentEntity->curstate.sequence = pFx->iAnimTargetSeq;
							pFx->bAnim = false;
						}
					}
					else
					{
						pFx->bAnim = false;
					}
				}
				else
				{
					m_pCurrentEntity->curstate.frame = 0;
					m_pCurrentEntity->curstate.sequence = 0;
				}
				m_pCurrentEntity->player = 0;
			}
			StudioMergeBones( pWeaponModel );
			IEngineStudio.StudioSetupLighting( &lighting );
			StudioCalcAttachments();
			m_pCurrentEntity->curstate.animtime = gEngfuncs.GetClientTime() + 1;
			StudioClientEvents();
			m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata( pWeaponModel );
			IEngineStudio.StudioSetHeader( m_pStudioHeader );
			StudioRenderModel();
			*m_pCurrentEntity = saveent;
		}
	}
	return 1;
}

////////////////////////////////////
// Hooks to class implementation
////////////////////////////////////

/*
====================
R_StudioDrawPlayer

====================
*/
int R_StudioDrawPlayer( int flags, entity_state_t* pplayer )
{
	return g_StudioRenderer.StudioDrawPlayer( flags, pplayer );
}

/*
====================
R_StudioDrawModel

====================
*/
int R_StudioDrawModel( int flags )
{
	return g_StudioRenderer.StudioDrawModel( flags );
}

/*
====================
R_StudioInit

====================
*/
void R_StudioInit()
{
	g_StudioRenderer.Init();
}

// The simple drawing interface we'll pass back to the engine
r_studio_interface_t studio =
{
	STUDIO_INTERFACE_VERSION,
	R_StudioDrawModel,
	R_StudioDrawPlayer
};

/*
====================
HUD_GetStudioModelInterface

Export this function for the engine to use the studio renderer class to render objects.
====================
*/
int CL_DLLEXPORT HUD_GetStudioModelInterface( int version, r_studio_interface_t** ppinterface, engine_studio_api_t* pstudio )
{
	g_pcldstAddrs->pStudioInterface( &version, &ppinterface, &pstudio );
//	RecClStudioInterface(version, ppinterface, pstudio);

	if ( version != STUDIO_INTERFACE_VERSION )
		return 0;

	// Point the engine to our callbacks
	*ppinterface = &studio;

	// Copy in engine helper functions
	memcpy( &IEngineStudio, pstudio, sizeof( IEngineStudio ) );

	// Initialize local variables, etc.
	R_StudioInit();

	// Success
	return 1;
}
