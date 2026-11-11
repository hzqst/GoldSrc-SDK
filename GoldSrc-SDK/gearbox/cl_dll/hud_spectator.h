//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "cl_entity.h"

#define INSET_OFF 0
#define INSET_CHASE_FREE 1
#define INSET_IN_EYE 2
#define INSET_MAP_FREE 3
#define INSET_MAP_CHASE 4

#define OVERVIEW_TILE_SIZE 128		// don't change this
#define OVERVIEW_MAX_LAYERS 1
#define MAX_OVERVIEW_ENTITIES 128

typedef struct overviewInfo_s
{
	vec3_t origin;		// center of map
	float zoom;		// zoom of map images
	int layers;		// how may layers do we have
	float layersHeights[OVERVIEW_MAX_LAYERS];
	char layersImages[OVERVIEW_MAX_LAYERS][255];
	qboolean rotated;	// are map images rotated (90 degrees) ?
	int insetWindowX;
	int insetWindowY;
	int insetWindowHeight;
	int insetWindowWidth;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
} overviewInfo_t;

typedef struct overviewEntity_s
{
	HSPRITE hSprite;
	cl_entity_t* entity;
	double killTime;
} overviewEntity_t;

class CHudSpectator : public CHudBase
{
public:
	int Init();
	int VidInit();
	int Draw( float time );
	void FindNextPlayer( bool reverse );
	void HandleButtonsDown( int buttons );
	void HandleButtonsUp( int buttons );
	void SetModes( int mainMode, int insetMode );
	bool IsActivePlayer( cl_entity_t* entity );
	bool ParseOverviewFile();
	void SetSpectatorStartPosition();
	void DirectorEvent( unsigned char command, unsigned int primary, unsigned int secondary, unsigned int flags );
	void LoadMapSprites();
	void DrawOverviewLayer();
	void GetMapPosition( float* position );
	void DrawOverviewEntities();
	void DrawOverview();
	void CheckOverviewEntities();
	bool AddOverviewEntity( int type, cl_entity_t* entity, const char* modelName );
	void DeathMessage( int victim );
	bool AddOverviewEntityToList( HSPRITE sprite, cl_entity_t* entity, double killTime );

	int m_iMainMode;
	int m_iInsetMode;
	int m_iDrawCycle;
	overviewInfo_t m_OverviewData;
	overviewEntity_t m_OverviewEntities[MAX_OVERVIEW_ENTITIES];
	int m_iObserverTarget;
	float m_mapZoom;		// zoom the user currently uses
	vec3_t m_mapOrigin;	// origin where user rotates around
	vec3_t m_mapAngles;
	cvar_t* m_drawnames;

private:
	vec3_t m_vPlayerPos[MAX_PLAYERS];
	HSPRITE m_hsprPlayerBlue;
	HSPRITE m_hsprPlayerRed;
	HSPRITE m_hsprPlayer;
	HSPRITE m_hsprCamera;
	HSPRITE m_hsprPlayerDead;
	HSPRITE m_hsprViewcone;
	HSPRITE m_hsprUnkownMap;
	HSPRITE m_hsprBeam;
	HSPRITE m_hCrosshair;
	model_s* m_MapSprite;
	float m_flNextObserverInput;
	float m_zoomDelta;
	float m_moveDelta;
	int m_lastPrimaryObject;
	int m_lastSecondaryObject;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};
