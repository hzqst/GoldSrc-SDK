//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#if !defined( GAMESTUDIOMODELRENDERER_H )
#define GAMESTUDIOMODELRENDERER_H
#if defined( _WIN32 )
#pragma once
#endif

struct client_anim_state_t
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
};

/*
====================
CGameStudioModelRenderer

====================
*/
class CGameStudioModelRenderer : public CStudioModelRenderer
{
public:
	CGameStudioModelRenderer();
	void StudioSetupBones() override;
	void StudioEstimateGait( entity_state_t* pplayer ) override;
	void StudioProcessGait( entity_state_t* pplayer ) override;
	int StudioDrawPlayer( int flags, entity_state_t* pplayer ) override;
	void StudioFxTransform( cl_entity_t* ent, float transform[3][4] ) override;
	void StudioPlayerBlend( mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch ) override;
	virtual void CalculateYawBlend( entity_state_t* pplayer );
	virtual void CalculatePitchBlend( entity_state_t* pplayer );

private:
	void SavePlayerState( entity_state_t* pplayer );
	void SetupClientAnimation( entity_state_t* pplayer );
	void RestorePlayerState( entity_state_t* pplayer );
	mstudioanim_t* LookupAnimation( mstudioseqdesc_t* pseqdesc, int index );
	model_t* SwapPWpnModels( cl_entity_t* ent, model_t* model );
	void AnimatePWpnModels( cl_entity_t* ent );
	void StudioClientEvents();
	int m_nPlayerGaitSequences[32];
};

#endif // GAMESTUDIOMODELRENDERER_H
