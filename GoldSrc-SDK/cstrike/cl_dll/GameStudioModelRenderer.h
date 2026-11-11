//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef GAMESTUDIOMODELRENDERER_H
#define GAMESTUDIOMODELRENDERER_H

enum BoneIndex
{
	BONE_HEAD = 0,
	BONE_PELVIS,
	BONE_SPINE1,
	BONE_SPINE2,
	BONE_SPINE3,
	BONE_MAX
};

/*
====================
CGameStudioModelRenderer

====================
*/
class CGameStudioModelRenderer : public CStudioModelRenderer
{
public:
	CGameStudioModelRenderer( void );
	int StudioDrawPlayer( int flags, entity_state_t* player );
	void StudioSetupBones( void );
	void StudioFxTransform( cl_entity_t* entity, float transform[3][4] );
	void StudioPlayerBlend( mstudioseqdesc_t* sequence, int* blend, float* pitch );
	void StudioEstimateGait( entity_state_t* player );
	void StudioProcessGait( entity_state_t* player );
	virtual int _StudioDrawPlayer( int flags, entity_state_t* player );
	virtual void CalculateYawBlend( entity_state_t* player );
	virtual void CalculatePitchBlend( entity_state_t* player );

	mstudioanim_t* LookupAnimation( mstudioseqdesc_t* sequence, int index );
	void SavePlayerState( entity_state_t* player );
	void RestorePlayerState( entity_state_t* player );
	void SetupClientAnimation( entity_state_t* player );
	void CachePlayerBoneIndices( void );
	int GetPlayerBoneIndex( BoneIndex bone );
	bool GetPlayerBoneWorldPosition( BoneIndex bone, Vector& position );

private:
	int m_nPlayerGaitSequences[32];
	bool m_bLocal;
	int m_boneIndexCache[BONE_MAX];
	bool m_isBoneCacheValid;
};

extern CGameStudioModelRenderer g_StudioRenderer;

#endif // GAMESTUDIOMODELRENDERER_H
