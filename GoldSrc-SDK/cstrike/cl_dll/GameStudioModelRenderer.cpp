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
#include "r_studioint.h"
#include "studio_util.h"
#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
#include "../../cl_dll/Exports.h"
#include <stddef.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\cl_dll\\GameStudioModelRenderer.cpp" )

//
// Override the StudioModelRender virtual member functions here to implement custom bone
// setup, blending, etc.
//

// Global engine <-> studio model rendering code interface
extern engine_studio_api_t IEngineStudio;
extern void CounterStrike_GetSequence( int* sequence, int* gaitsequence );
extern void CounterStrike_GetOrientation( float* origin, float* angles );
extern char* sPlayerModelFiles[];
extern cvar_t* cl_shadows;

cvar_t* cl_minmodels;
cvar_t* cl_min_ct;
cvar_t* cl_min_t;
float g_flStartScaleTime;
int iPrevRenderState;
int iRenderStateChanged;

typedef struct
{
	Vector origin;
	Vector angles;
	Vector realangles;
	float animtime;
	float frame;
	int sequence;
	int gaitsequence;
	float framerate;
	int m_fSequenceLoops;
	int m_fSequenceFinished;
	byte controller[4];
	byte blending[2];
	latchedvars_t lv;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
} client_anim_state_t;

static client_anim_state_t g_state;
static client_anim_state_t g_clientstate;

// The renderer object, created on the stack.
CGameStudioModelRenderer g_StudioRenderer;

static const char* s_boneNames[BONE_MAX] =
{
	"Bip01 Head",
	"Bip01 Pelvis",
	"Bip01 Spine1",
	"Bip01 Spine2",
	"Bip01 Spine3"
};

void GetSequenceInfo( void* model, client_anim_state_t* state, float* frameRate, float* groundSpeed )
{
	if ( !model )
		return;

	studiohdr_t* header = (studiohdr_t*)model;
	if ( state->sequence < 0 || state->sequence >= header->numseq )
	{
		*frameRate = 0;
		*groundSpeed = 0;
		return;
	}

	mstudioseqdesc_t* sequence = (mstudioseqdesc_t*)((byte*)header + header->seqindex) + state->sequence;
	if ( sequence->numframes > 1 )
	{
		*frameRate = 256.0f * sequence->fps / (sequence->numframes - 1);
		*groundSpeed = Length( sequence->linearmovement );
		*groundSpeed = *groundSpeed * sequence->fps / (sequence->numframes - 1);
	}
	else
	{
		*frameRate = 256;
		*groundSpeed = 0;
	}
}

int GetSequenceFlags( void* model, client_anim_state_t* state )
{
	if ( !model )
		return 0;
	studiohdr_t* header = (studiohdr_t*)model;
	if ( state->sequence < 0 || state->sequence >= header->numseq )
		return 0;
	return ((mstudioseqdesc_t*)((byte*)header + header->seqindex))[state->sequence].flags;
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
	state->frame += interval * frameRate * state->framerate;
	state->animtime = gEngfuncs.GetClientTime();
	if ( !(state->frame >= 0 && state->frame < 256) )
	{
		if ( state->m_fSequenceLoops )
			state->frame -= (int)(state->frame / 256) * 256;
		else
			state->frame = state->frame >= 0 ? 255 : 0;
		state->m_fSequenceFinished = 1;
	}
	return interval;
}

bool BIsValidCTModelIndex( int index )
{
	return index == 2 || index == 4 || index == 7 || index == 9 || index == 10;
}

bool BIsValidTModelIndex( int index )
{
	return index == 1 || index == 5 || index == 6 || index == 8 || index == 11;
}

bool WeaponHasAttachments( entity_state_t* player )
{
	if ( !player )
		return false;
	model_t* model = IEngineStudio.GetModelByIndex( player->weaponmodel );
	return ((studiohdr_t*)IEngineStudio.Mod_Extradata( model ))->numattachments != 0;
}

#ifdef _MSC_VER
/*
====================
CGameStudioModelRenderer

====================
*/
#pragma warning( suppress : 26495 )
#endif
CGameStudioModelRenderer::CGameStudioModelRenderer( void )
{
	m_bLocal = false;
}

mstudioanim_t* CGameStudioModelRenderer::LookupAnimation( mstudioseqdesc_t* sequence, int index )
{
	mstudioanim_t* panim = StudioGetAnim( m_pRenderModel, sequence );
	ASSERT_AT( panim, 119 );
	if ( index >= 0 && index < sequence->numblends )
		panim += m_pStudioHeader->numbones * index;
	return panim;
}

void CGameStudioModelRenderer::SavePlayerState( entity_state_t* player )
{
	cl_entity_t* ent = IEngineStudio.GetCurrentEntity();
	ASSERT_AT( ent, 806 );
	if ( !ent )
		return;
	g_state.animtime = ent->curstate.animtime;
	g_state.frame = ent->curstate.frame;
	g_state.framerate = ent->curstate.framerate;
	g_state.angles = ent->curstate.angles;
	g_state.origin = ent->curstate.origin;
	g_state.realangles = ent->angles;
	g_state.sequence = ent->curstate.sequence;
	g_state.gaitsequence = player->gaitsequence;
	memcpy( g_state.blending, ent->curstate.blending, sizeof( g_state.blending ) );
	memcpy( g_state.controller, ent->curstate.controller, sizeof( g_state.controller ) );
	memcpy( &g_state.lv, &ent->latched, offsetof( latchedvars_t, prevblending ) + sizeof( g_state.lv.prevblending ) );
}

void CGameStudioModelRenderer::RestorePlayerState( entity_state_t* player )
{
	cl_entity_t* ent = IEngineStudio.GetCurrentEntity();
	ASSERT_AT( ent, 979 );
	if ( !ent )
		return;
	g_clientstate.animtime = ent->curstate.animtime;
	g_clientstate.frame = ent->curstate.frame;
	g_clientstate.framerate = ent->curstate.framerate;
	g_clientstate.angles = ent->curstate.angles;
	g_clientstate.origin = ent->curstate.origin;
	g_clientstate.realangles = ent->angles;
	g_clientstate.sequence = ent->curstate.sequence;
	g_clientstate.gaitsequence = player->gaitsequence;
	memcpy( g_clientstate.blending, ent->curstate.blending, sizeof( g_clientstate.blending ) );
	memcpy( g_clientstate.controller, ent->curstate.controller, sizeof( g_clientstate.controller ) );
	memcpy( &g_clientstate.lv, &ent->latched, offsetof( latchedvars_t, prevblending ) + sizeof( g_clientstate.lv.prevblending ) );
	ent->curstate.animtime = g_state.animtime;
	ent->curstate.frame = g_state.frame;
	ent->curstate.framerate = g_state.framerate;
	VectorCopy( g_state.angles, ent->curstate.angles );
	VectorCopy( g_state.origin, ent->curstate.origin );
	VectorCopy( g_state.realangles, ent->angles );
	ent->curstate.sequence = g_state.sequence;
	player->gaitsequence = g_state.gaitsequence;
	memcpy( ent->curstate.blending, g_state.blending, sizeof( g_state.blending ) );
	memcpy( ent->curstate.controller, g_state.controller, sizeof( g_state.controller ) );
	memcpy( &ent->latched, &g_state.lv, offsetof( latchedvars_t, prevblending ) + sizeof( g_state.lv.prevblending ) );
}

int CGameStudioModelRenderer::StudioDrawPlayer( int flags, entity_state_t* player )
{
	if ( !m_bLocal || IEngineStudio.GetCurrentEntity() != gEngfuncs.GetLocalPlayer() )
		return _StudioDrawPlayer( flags, player );
	SavePlayerState( player );
	SetupClientAnimation( player );
	int result = _StudioDrawPlayer( flags, player );
	RestorePlayerState( player );
	return result;
}

void CGameStudioModelRenderer::StudioPlayerBlend( mstudioseqdesc_t*, int* blend, float* pitch )
{
	*blend = (int)(3.0f * *pitch);
	if ( *blend <= -45 )
		*blend = 255;
	else if ( *blend >= 45 )
		*blend = 0;
	else
		*blend = (int)((45.0 - *blend) * 255.0 / 90.0);
	*pitch = 0;
}

void CGameStudioModelRenderer::CalculatePitchBlend( entity_state_t* )
{
	mstudioseqdesc_t* sequence = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;
	int blend;
	StudioPlayerBlend( sequence, &blend, &m_pCurrentEntity->angles[PITCH] );
	m_pCurrentEntity->latched.prevangles[PITCH] = m_pCurrentEntity->angles[PITCH];
	m_pCurrentEntity->curstate.blending[1] = (byte)blend;
	m_pCurrentEntity->latched.prevblending[1] = m_pCurrentEntity->curstate.blending[1];
	m_pCurrentEntity->latched.prevseqblending[1] = m_pCurrentEntity->curstate.blending[1];
}

void CGameStudioModelRenderer::StudioProcessGait( entity_state_t* player )
{
	CalculateYawBlend( player );
	CalculatePitchBlend( player );
	float dt = m_clTime - m_clOldTime;
	if ( dt <= 0 )
		dt = 0;
	else if ( !(dt < 1) )
		dt = 1;
	mstudioseqdesc_t* sequence = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex) + player->gaitsequence;
	if ( sequence->linearmovement[0] > 0 )
		m_pPlayerInfo->gaitframe += m_flGaitMovement / sequence->linearmovement[0] * sequence->numframes;
	else
		m_pPlayerInfo->gaitframe += dt * sequence->fps * m_pCurrentEntity->curstate.framerate;
	m_pPlayerInfo->gaitframe -= (int)(m_pPlayerInfo->gaitframe / sequence->numframes) * sequence->numframes;
	if ( m_pPlayerInfo->gaitframe < 0 )
		m_pPlayerInfo->gaitframe += sequence->numframes;
}

void CGameStudioModelRenderer::StudioEstimateGait( entity_state_t* player )
{
	float dt = m_clTime - m_clOldTime;
	if ( dt <= 0 || m_pPlayerInfo->renderframe == m_nFrameCount )
	{
		m_flGaitMovement = 0;
		return;
	}
	if ( !(dt < 1) )
		dt = 1;
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
		VectorCopy( player->velocity, velocity );
		m_flGaitMovement = Length( velocity ) * dt;
	}

	if ( m_pCurrentEntity->curstate.sequence > 100 )
	{
		m_pPlayerInfo->gaityaw += m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
		return;
	}
	if ( velocity[0] == 0 && velocity[1] == 0 )
	{
		float difference = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
		difference -= (int)(difference / 360) * 360;
		if ( difference > 180 )
			difference -= 360;
		if ( difference < -180 )
			difference += 360;
		float yaw = fmod( m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw, 360.0 );
		if ( yaw < -180 )
			yaw += 360;
		else if ( yaw > 180 )
			yaw -= 360;
		if ( yaw > -5 && yaw < 5 )
			m_pCurrentEntity->baseline.fuser1 = 0.05f;
		if ( yaw < -90 || yaw > 90 )
			m_pCurrentEntity->baseline.fuser1 = 3.5f;
		if ( dt < 0.25 )
			difference *= dt * m_pCurrentEntity->baseline.fuser1;
		else
			difference *= dt;
		if ( abs( (int)difference ) < 0.1 )
			difference = 0;
		m_pPlayerInfo->gaityaw += difference;
		m_pPlayerInfo->gaityaw -= (int)(m_pPlayerInfo->gaityaw / 360) * 360;
		m_flGaitMovement = 0;
	}
	else
	{
		m_pPlayerInfo->gaityaw = atan2( velocity[1], velocity[0] ) * 180 / M_PI;
		if ( m_pPlayerInfo->gaityaw > 180 )
			m_pPlayerInfo->gaityaw = 180;
		if ( m_pPlayerInfo->gaityaw < -180 )
			m_pPlayerInfo->gaityaw = -180;
	}
}

void CGameStudioModelRenderer::CalculateYawBlend( entity_state_t* player )
{
	StudioEstimateGait( player );
	float yaw = fmod( m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw, 360.0 );
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
	float blend = yaw / 90 * 128 + 127;
	int value = 0;
	if ( blend < 255 )
		value = blend > 0 ? (int)(255.0f - blend) : 255;
	m_pCurrentEntity->curstate.blending[0] = (byte)value;
	m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->curstate.blending[0];
	m_pCurrentEntity->latched.prevseqblending[0] = m_pCurrentEntity->curstate.blending[0];
	m_pCurrentEntity->angles[YAW] = m_pPlayerInfo->gaityaw;
	if ( m_pCurrentEntity->angles[YAW] < 0 )
		m_pCurrentEntity->angles[YAW] += 360;
	m_pCurrentEntity->latched.prevangles[YAW] = m_pCurrentEntity->angles[YAW];
}

void CGameStudioModelRenderer::StudioFxTransform( cl_entity_t* entity, float transform[3][4] )
{
	switch ( entity->curstate.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if ( gEngfuncs.pfnRandomLong( 0, 49 ) == 0 )
		{
			int axis = gEngfuncs.pfnRandomLong( 0, 1 );
			if ( axis == 1 )
				axis = 2;
			VectorScale( transform[axis], gEngfuncs.pfnRandomFloat( 1, 1.484f ), transform[axis] );
		}
		else if ( gEngfuncs.pfnRandomLong( 0, 49 ) == 0 )
		{
			gEngfuncs.pfnRandomLong( 0, 1 );
			float offset = gEngfuncs.pfnRandomFloat( -10, 10 );
			transform[gEngfuncs.pfnRandomLong( 0, 2 )][3] += offset;
		}
		break;
	case kRenderFxExplode:
		if ( iRenderStateChanged )
		{
			g_flStartScaleTime = m_clTime;
			iRenderStateChanged = 0;
		}
		{
			float elapsed = m_clTime - g_flStartScaleTime;
			if ( elapsed > 0 )
			{
				float scale = elapsed > 2 ? 0.001f : 1.0f - elapsed * 0.5f;
				for ( int row = 0; row < 3; ++row )
					for ( int column = 0; column < 3; ++column )
						transform[row][column] *= scale;
			}
		}
		break;
	}
}

#pragma warning( push )
#pragma warning( disable : 4701 )
void CGameStudioModelRenderer::SetupClientAnimation( entity_state_t* player )
{
	cl_entity_t* ent = IEngineStudio.GetCurrentEntity();
	ASSERT_AT( ent, 919 );
	if ( !ent )
		return;
	static double oldtime;
	double time = gEngfuncs.GetClientTime();
	double elapsed = time - oldtime;
	if ( elapsed <= 0 )
		elapsed = 0;
	else if ( !(elapsed < 1) )
		elapsed = 1;
	oldtime = time;
	int sequence = g_clientstate.sequence;
	g_clientstate.framerate = 1;
	CounterStrike_GetSequence( &g_clientstate.sequence, &g_clientstate.gaitsequence );
	CounterStrike_GetOrientation( g_clientstate.origin, g_clientstate.angles );
	g_clientstate.realangles = g_clientstate.angles;
	if ( sequence != g_clientstate.sequence )
	{
		g_clientstate.frame = 0;
		g_clientstate.lv.prevsequence = sequence;
		memcpy( g_clientstate.lv.prevseqblending, g_clientstate.blending, sizeof( g_clientstate.blending ) );
		g_clientstate.lv.sequencetime = g_clientstate.animtime;
		memcpy( g_clientstate.lv.prevcontroller, g_clientstate.controller, sizeof( g_clientstate.controller ) );
	}
	void* model = IEngineStudio.Mod_Extradata( ent->model );
	float frameRate;
	float groundSpeed;
	GetSequenceInfo( model, &g_clientstate, &frameRate, &groundSpeed );
	g_clientstate.m_fSequenceLoops = GetSequenceFlags( model, &g_clientstate ) & STUDIO_LOOPING;
	StudioFrameAdvance( &g_clientstate, frameRate, elapsed );
	VectorCopy( g_clientstate.realangles, ent->angles );
	VectorCopy( g_clientstate.angles, ent->curstate.angles );
	VectorCopy( g_clientstate.origin, ent->curstate.origin );
	ent->curstate.sequence = g_clientstate.sequence;
	player->gaitsequence = g_clientstate.gaitsequence;
	ent->curstate.animtime = g_clientstate.animtime;
	ent->curstate.frame = g_clientstate.frame;
	ent->curstate.framerate = g_clientstate.framerate;
	memcpy( ent->curstate.blending, g_clientstate.blending, sizeof( g_clientstate.blending ) );
	memcpy( ent->curstate.controller, g_clientstate.controller, sizeof( g_clientstate.controller ) );
	memcpy( &ent->latched, &g_clientstate.lv, offsetof( latchedvars_t, prevblending ) + sizeof( g_clientstate.lv.prevblending ) );
}
#pragma warning( pop )

int CGameStudioModelRenderer::_StudioDrawPlayer( int flags, entity_state_t* player )
{
	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );
	IEngineStudio.GetViewInfo( m_vRenderOrigin, m_vUp, m_vRight, m_vNormal );
	IEngineStudio.GetAliasScale( &m_fSoftwareXScale, &m_fSoftwareYScale );
	m_nPlayerIndex = player->number - 1;
	if ( m_nPlayerIndex < 0 || m_nPlayerIndex >= gEngfuncs.GetMaxClients() )
		return 0;
	if ( !cl_minmodels || cl_minmodels->value <= 0 )
		m_pRenderModel = IEngineStudio.SetupPlayerModel( m_nPlayerIndex );
	else
	{
		int modelindex;
		extra_player_info_t* info = &g_PlayerExtraInfo[player->number];
		if ( info->teamnumber == 1 )
		{
			int model = cl_min_t ? (int)cl_min_t->value : 1;
			if ( !BIsValidTModelIndex( model ) )
				model = 1;
			m_pRenderModel = gEngfuncs.CL_LoadModel( sPlayerModelFiles[model], &modelindex );
		}
		else if ( info->teamnumber == 2 )
		{
			int model = cl_min_ct ? (int)cl_min_ct->value : 2;
			if ( !BIsValidCTModelIndex( model ) )
				model = 2;
			if ( info->vip )
				model = 3;
			m_pRenderModel = gEngfuncs.CL_LoadModel( sPlayerModelFiles[model], &modelindex );
		}
		else
			m_pRenderModel = NULL;
	}
	if ( !m_pRenderModel )
		return 0;
	m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata( m_pRenderModel );
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );
	if ( m_pCurrentEntity->curstate.sequence < 0 || m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq )
		m_pCurrentEntity->curstate.sequence = 0;
	if ( player->sequence < 0 || player->sequence >= m_pStudioHeader->numseq )
		player->sequence = 0;
	if ( m_pCurrentEntity->curstate.gaitsequence < 0 || m_pCurrentEntity->curstate.gaitsequence >= m_pStudioHeader->numseq )
		m_pCurrentEntity->curstate.gaitsequence = 0;
	if ( player->gaitsequence < 0 || player->gaitsequence >= m_pStudioHeader->numseq )
		player->gaitsequence = 0;
	else if ( player->gaitsequence )
	{
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
		vec3_t angles;
		VectorCopy( m_pCurrentEntity->angles, angles );
		StudioProcessGait( player );
		m_pPlayerInfo->gaitsequence = player->gaitsequence;
		m_pPlayerInfo = NULL;
		StudioSetUpTransform( 0 );
		VectorCopy( angles, m_pCurrentEntity->angles );
		goto transformReady;
	}
	memset( m_pCurrentEntity->curstate.controller, 127, sizeof( m_pCurrentEntity->curstate.controller ) );
	memcpy( m_pCurrentEntity->latched.prevcontroller, m_pCurrentEntity->curstate.controller, sizeof( m_pCurrentEntity->curstate.controller ) );
	m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
	CalculatePitchBlend( player );
	CalculateYawBlend( player );
	m_pPlayerInfo->gaitsequence = 0;
	StudioSetUpTransform( 0 );

transformReady:
	if ( flags & STUDIO_RENDER )
	{
		if ( !IEngineStudio.StudioCheckBBox() )
			return 0;
		++*m_pModelsDrawn;
		++*m_pStudioModelCount;
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
		if ( !(flags & STUDIO_RENDER) || !player->weaponmodel || !WeaponHasAttachments( player ) )
		{
			StudioCalcAttachments();
			IEngineStudio.StudioClientEvents();
			if ( m_pCurrentEntity->index > 0 )
				memcpy( gEngfuncs.GetEntityByIndex( m_pCurrentEntity->index )->attachment,
					m_pCurrentEntity->attachment, sizeof( m_pCurrentEntity->attachment ) );
		}
	}
	if ( flags & STUDIO_RENDER )
	{
		alight_t lighting;
		vec3_t lightDirection;
		lighting.plightvec = lightDirection;
		IEngineStudio.StudioDynamicLight( m_pCurrentEntity, &lighting );
		IEngineStudio.StudioEntityLight( &lighting );
		IEngineStudio.StudioSetupLighting( &lighting );
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
		m_nTopColor = m_pPlayerInfo->topcolor;
		if ( m_nTopColor < 0 )
			m_nTopColor = 0;
		else if ( m_nTopColor > 360 )
			m_nTopColor = 360;
		m_nBottomColor = m_pPlayerInfo->bottomcolor;
		if ( m_nBottomColor < 0 )
			m_nBottomColor = 0;
		else if ( m_nBottomColor > 360 )
			m_nBottomColor = 360;
		IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );
		StudioRenderModel();
		m_pPlayerInfo = NULL;
		if ( player->weaponmodel )
		{
			studiohdr_t* header = m_pStudioHeader;
			cl_entity_t saved = *m_pCurrentEntity;
			model_t* weapon = IEngineStudio.GetModelByIndex( player->weaponmodel );
			m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata( weapon );
			IEngineStudio.StudioSetHeader( m_pStudioHeader );
			StudioMergeBones( weapon );
			IEngineStudio.StudioSetupLighting( &lighting );
			StudioRenderModel();
			StudioCalcAttachments();
			if ( m_pCurrentEntity->index > 0 )
				memcpy( saved.attachment, m_pCurrentEntity->attachment, sizeof( vec3_t ) * m_pStudioHeader->numattachments );
			*m_pCurrentEntity = saved;
			m_pStudioHeader = header;
			IEngineStudio.StudioSetHeader( m_pStudioHeader );
			if ( flags & STUDIO_EVENTS )
				IEngineStudio.StudioClientEvents();
		}
		if ( cl_shadows->value != 0 )
		{
			Vector position;
			if ( GetPlayerBoneWorldPosition( BONE_PELVIS, position ) )
				StudioDrawShadow( position, 20 );
		}
	}
	return 1;
}

void CGameStudioModelRenderer::StudioSetupBones( void )
{
	if ( !m_pCurrentEntity->player )
	{
		CStudioModelRenderer::StudioSetupBones();
		return;
	}
	static float pos[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES];
	static float pos2[MAXSTUDIOBONES][3];
	static vec4_t q2[MAXSTUDIOBONES];
	static float pos3[MAXSTUDIOBONES][3];
	static vec4_t q3[MAXSTUDIOBONES];
	static float pos4[MAXSTUDIOBONES][3];
	static vec4_t q4[MAXSTUDIOBONES];
	static float pos1b[MAXSTUDIOBONES][3];
	static vec4_t q1b[MAXSTUDIOBONES];
	if ( m_pCurrentEntity->curstate.sequence < 0 || m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq )
		m_pCurrentEntity->curstate.sequence = 0;
	mstudioseqdesc_t* sequences = (mstudioseqdesc_t*)((byte*)m_pStudioHeader + m_pStudioHeader->seqindex);
	mstudioseqdesc_t* sequence = sequences + m_pCurrentEntity->curstate.sequence;
	mstudioanim_t* animation = StudioGetAnim( m_pRenderModel, sequence );
	if ( m_pPlayerInfo )
	{
		int index = m_pCurrentEntity->index - 1;
		if ( m_nPlayerGaitSequences[index] != 6 && m_pPlayerInfo->gaitsequence == 6 )
			m_pPlayerInfo->gaitframe = 0;
		m_nPlayerGaitSequences[index] = m_pPlayerInfo->gaitsequence;
	}
	float frame = StudioEstimateFrame( sequence );
	if ( m_pPlayerInfo->gaitsequence == 3 )
	{
		byte& blend = m_pCurrentEntity->curstate.blending[0];
		blend = blend > 26 ? blend - 26 : 0;
		m_pCurrentEntity->latched.prevseqblending[0] = blend;
	}
	auto blendSequence = [&]( mstudioseqdesc_t* descriptor, mstudioanim_t* first, float sample,
		byte x, byte y, float output[][3], vec4_t* rotation )
	{
		if ( descriptor->numblends != 9 )
		{
			StudioCalcRotations( output, rotation, descriptor, first, sample );
			return;
		}
		int column = x > 127 ? 1 : 0;
		int row = y > 127 ? 3 : 0;
		int index = row + column;
		float horizontal = (x - (column ? 127 : 0)) * 2.0f / 255.0f;
		float vertical = (y - (row ? 127 : 0)) * 2.0f / 255.0f;
		StudioCalcRotations( output, rotation, descriptor, LookupAnimation( descriptor, index ), sample );
		StudioCalcRotations( pos2, q2, descriptor, LookupAnimation( descriptor, index + 1 ), sample );
		StudioCalcRotations( pos3, q3, descriptor, LookupAnimation( descriptor, index + 3 ), sample );
		StudioCalcRotations( pos4, q4, descriptor, LookupAnimation( descriptor, index + 4 ), sample );
		StudioSlerpBones( rotation, output, q2, pos2, horizontal );
		StudioSlerpBones( q3, pos3, q4, pos4, horizontal );
		StudioSlerpBones( rotation, output, q3, pos3, vertical );
	};
	blendSequence( sequence, animation, frame, m_pCurrentEntity->curstate.blending[0],
		m_pCurrentEntity->curstate.blending[1], pos, q );
	if ( m_fDoInterp && m_pCurrentEntity->latched.sequencetime != 0 &&
		m_pCurrentEntity->latched.sequencetime + 0.2 > m_clTime &&
		m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq )
	{
		sequence = sequences + m_pCurrentEntity->latched.prevsequence;
		animation = StudioGetAnim( m_pRenderModel, sequence );
		blendSequence( sequence, animation, m_pCurrentEntity->latched.prevframe,
			m_pCurrentEntity->latched.prevseqblending[0], m_pCurrentEntity->latched.prevseqblending[1], pos1b, q1b );
		float blend = 1.0 - (m_clTime - m_pCurrentEntity->latched.sequencetime) / 0.2;
		StudioSlerpBones( q, pos, q1b, pos1b, blend );
	}
	else
		m_pCurrentEntity->latched.prevframe = frame;

	mstudiobone_t* bones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);
	int currentSequence = m_pCurrentEntity->curstate.sequence;
	if ( m_pPlayerInfo && currentSequence != 8 && currentSequence != 9 && currentSequence <= 100 )
	{
		if ( m_pPlayerInfo->gaitsequence < 0 || m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq )
			m_pPlayerInfo->gaitsequence = 0;
		sequence = sequences + m_pPlayerInfo->gaitsequence;
		animation = StudioGetAnim( m_pRenderModel, sequence );
		StudioCalcRotations( pos2, q2, sequence, animation, m_pPlayerInfo->gaitframe );
		bool copy = true;
		for ( int i = 0; i < m_pStudioHeader->numbones; ++i )
		{
			if ( !strcmp( bones[i].name, "Bip01 Spine" ) )
				copy = false;
			else if ( !strcmp( bones[bones[i].parent].name, "Bip01 Pelvis" ) || copy )
			{
				VectorCopy( pos2[i], pos[i] );
				memcpy( q[i], q2[i], sizeof( q[i] ) );
				copy = true;
			}
		}
	}
	for ( int i = 0; i < m_pStudioHeader->numbones; ++i )
	{
		float matrix[3][4];
		QuaternionMatrix( q[i], matrix );
		matrix[0][3] = pos[i][0];
		matrix[1][3] = pos[i][1];
		matrix[2][3] = pos[i][2];
		if ( bones[i].parent == -1 )
		{
			if ( IEngineStudio.IsHardware() )
			{
				ConcatTransforms( (*m_protationmatrix), matrix, (*m_pbonetransform)[i] );
				MatrixCopy( (*m_pbonetransform)[i], (*m_plighttransform)[i] );
			}
			else
			{
				ConcatTransforms( (*m_paliastransform), matrix, (*m_pbonetransform)[i] );
				ConcatTransforms( (*m_protationmatrix), matrix, (*m_plighttransform)[i] );
			}
			StudioFxTransform( m_pCurrentEntity, (*m_pbonetransform)[i] );
		}
		else
		{
			ConcatTransforms( (*m_pbonetransform)[bones[i].parent], matrix, (*m_pbonetransform)[i] );
			ConcatTransforms( (*m_plighttransform)[bones[i].parent], matrix, (*m_plighttransform)[i] );
		}
	}
}

void CGameStudioModelRenderer::CachePlayerBoneIndices( void )
{
	if ( m_isBoneCacheValid )
		return;
	mstudiobone_t* bones = (mstudiobone_t*)((byte*)m_pStudioHeader + m_pStudioHeader->boneindex);
	for ( int i = 0; i < BONE_MAX; ++i )
	{
		m_boneIndexCache[i] = -1;
		for ( int bone = 0; bone < m_pStudioHeader->numbones; ++bone )
		{
			if ( !strcmp( bones[bone].name, s_boneNames[i] ) )
			{
				m_boneIndexCache[i] = bone;
				break;
			}
		}
	}
	m_isBoneCacheValid = true;
}

int CGameStudioModelRenderer::GetPlayerBoneIndex( BoneIndex bone )
{
	return m_boneIndexCache[bone];
}

bool CGameStudioModelRenderer::GetPlayerBoneWorldPosition( BoneIndex bone, Vector& position )
{
	CachePlayerBoneIndices();
	int index = m_boneIndexCache[bone];
	if ( index < 0 || index >= m_pStudioHeader->numbones )
		return false;
	position.x = m_rgCachedBoneTransform[index][0][3];
	position.y = m_rgCachedBoneTransform[index][1][3];
	position.z = m_rgCachedBoneTransform[index][2][3];
	return true;
}

/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	g_StudioRenderer.Init();
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

int R_StudioDrawPlayer( int flags, entity_state_t* player )
{
	return g_StudioRenderer.StudioDrawPlayer( flags, player );
}

// The simple drawing interface we'll pass back to the engine
r_studio_interface_t studio =
{
	STUDIO_INTERFACE_VERSION,
	R_StudioDrawModel,
	R_StudioDrawPlayer
};

int CL_DLLEXPORT HUD_GetStudioModelInterface( int version, r_studio_interface_t** renderer, engine_studio_api_t* engine )
{
//	RecClStudioInterface(version, ppinterface, pstudio);

	if ( version != STUDIO_INTERFACE_VERSION )
		return 0;
	*renderer = &studio;
	memcpy( &IEngineStudio, engine, sizeof( IEngineStudio ) );

	// Initialize local variables, etc.
	R_StudioInit();

	// Success
	return 1;
}
