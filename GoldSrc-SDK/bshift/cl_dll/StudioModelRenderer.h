//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

/*
====================
CStudioModelRenderer

====================
*/
class CStudioModelRenderer
{
public:
	CStudioModelRenderer();
	virtual ~CStudioModelRenderer();
	virtual void Init();
	// Public Interfaces

	virtual int StudioDrawModel( int flags );
	virtual int StudioDrawPlayer( int flags, entity_state_s* player );

	virtual mstudioanim_t* StudioGetAnim( model_t* model, mstudioseqdesc_t* sequence );
	virtual void StudioSetUpTransform( int trivialAccept );
	virtual void StudioSetupBones();
	virtual void StudioCalcAttachments();
	virtual void StudioSaveBones();
	virtual void StudioMergeBones( model_t* model );
	virtual float StudioEstimateInterpolant();
	virtual float StudioEstimateFrame( mstudioseqdesc_t* sequence );

	// Apply special effects to transform matrix
	virtual void StudioFxTransform( cl_entity_t* ent, float transform[3][4] );

	// Spherical interpolation of bones
	virtual void StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s );
	virtual void StudioCalcBoneAdj( float dadt, float* adj, const byte* controller1,
		const byte* controller2, byte mouthopen );
	virtual void StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t* bone,
		mstudioanim_t* animation, float* adj, float* q );
	virtual void StudioCalcBonePosition( int frame, float s, mstudiobone_t* bone,
		mstudioanim_t* animation, float* adj, float* pos );
	virtual void StudioCalcRotations( float pos[][3], vec4_t* q, mstudioseqdesc_t* sequence,
		mstudioanim_t* animation, float frame );
	virtual void StudioRenderModel();
	virtual void StudioRenderFinal();
	virtual void StudioRenderFinal_Software();
	virtual void StudioRenderFinal_Hardware();
	virtual void StudioPlayerBlend( mstudioseqdesc_t* sequence, int* blend, float* pitch );
	virtual void StudioEstimateGait( entity_state_t* player );
	virtual void StudioProcessGait( entity_state_t* player );

public:

	// Client clock
	double			m_clTime;
	// Old Client clock
	double			m_clOldTime;

	// Do interpolation?
	int				m_fDoInterp;
	// Do gait estimation?
	int				m_fGaitEstimation;

	// Current render frame #
	int				m_nFrameCount;

	// Cvars that studio model code needs to reference
	//
	// Use high quality models?
	cvar_t*			m_pCvarHiModels;
	// Developer debug output desired?
	cvar_t*			m_pCvarDeveloper;
	// Draw entities bone hit boxes, etc?
	cvar_t*			m_pCvarDrawEntities;

	// The entity which we are currently rendering.
	cl_entity_t*	m_pCurrentEntity;

	// The model for the entity being rendered
	model_t*		m_pRenderModel;

	// Player info for current player, if drawing a player
	player_info_t*	m_pPlayerInfo;

	// The index of the player being drawn
	int				m_nPlayerIndex;

	// The player's gait movement
	float			m_flGaitMovement;

	// Pointer to header block for studio model data
	studiohdr_t*	m_pStudioHeader;

	// Pointers to current body part and submodel
	mstudiobodyparts_t* m_pBodyPart;
	mstudiomodel_t*	m_pSubModel;

	// Palette substition for top and bottom of model
	int				m_nTopColor;
	int				m_nBottomColor;

	//
	// Sprite model used for drawing studio model chrome
	model_t*		m_pChromeSprite;

	// Caching
	// Number of bones in bone cache
	int				m_nCachedBones;
	// Names of cached bones
	char			m_nCachedBoneNames[MAXSTUDIOBONES][32];
	// Cached bone & light transformation matrices
	float			m_rgCachedBoneTransform[MAXSTUDIOBONES][3][4];
	float			m_rgCachedLightTransform[MAXSTUDIOBONES][3][4];

	float			m_fSoftwareXScale;
	float			m_fSoftwareYScale;

	// Current view vectors and render origin
	float			m_vUp[3];
	float			m_vRight[3];
	float			m_vNormal[3];
	float			m_vRenderOrigin[3];

	// Model render counters ( from engine )
	int*			m_pStudioModelCount;
	int*			m_pModelsDrawn;

	// Matrices
	// Model to world transformation
	float			( *m_protationmatrix )[3][4];
	// Model to view transformation
	float			( *m_paliastransform )[3][4];

	// Concatenated bone and light transforms
	float			( *m_pbonetransform )[MAXSTUDIOBONES][3][4];
	float			( *m_plighttransform )[MAXSTUDIOBONES][3][4];
};
