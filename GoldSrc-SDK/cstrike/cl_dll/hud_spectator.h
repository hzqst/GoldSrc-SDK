//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SPECTATOR_H
#define SPECTATOR_H

#include "cl_entity.h"
#include "interpolation.h"

#define INSET_OFF			0
#define INSET_CHASE_FREE		1
#define INSET_IN_EYE			2
#define INSET_MAP_FREE		3
#define INSET_MAP_CHASE		4
#define MAX_SPEC_HUD_MESSAGES	8
#define OVERVIEW_TILE_SIZE	128		// don't change this
#define OVERVIEW_MAX_LAYERS	1
#define MAX_OVERVIEW_ENTITIES	128
#define MAX_CAM_WAYPOINTS		32

typedef struct overviewInfo_s
{
	char map[64];	// cl.levelname or empty
	Vector origin;
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

typedef struct cameraWayPoint_s
{
	float time;
	Vector position;
	Vector angle;
	float fov;
	int flags;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
} cameraWayPoint_t;

class CHudSpectator : public CHudBase
{
public:
	void Reset() override;
	int ToggleInset( bool allowOff );
	void ToggleInsetVisibility();
	void CheckSettings();
	void InitHUDData() override;
	bool AddOverviewEntityToList( HSPRITE sprite, cl_entity_t* entity, double killTime );
	void DeathMessage( int victim );
	bool AddOverviewEntity( int type, cl_entity_t* entity, const char* modelName );
	void CheckOverviewEntities();
	void DrawOverview();
	void DrawOverviewEntities();
	void GetMapPosition( float* position );
	void DrawOverviewLayer();
	void LoadMapSprites();
	bool ParseOverviewFile();
	bool IsActivePlayer( cl_entity_t* entity );
	void SetModes( int mainMode, int insetMode );
	void HandleButtonsDown( int buttons );
	void HandleButtonsUp( int buttons );
	void FindNextPlayer( bool reverse );
	void FindPlayer( const char* name );
	void DirectorMessage( int size, void* buffer );
	void SetSpectatorStartPosition();
	int Init() override;
	int VidInit() override;
	int Draw( float time ) override;
	void AddWaypoint( float time, Vector position, Vector angle, float fov, int flags );
	void SetCameraView( Vector position, Vector angle, float fov );
	float GetFOV();
	bool GetDirectorCamera( Vector& position, Vector& angle );
	void SetWayInterpolation( cameraWayPoint_t* previous, cameraWayPoint_t* start,
		cameraWayPoint_t* end, cameraWayPoint_t* next );

	int m_iDrawCycle;
	client_textmessage_t m_HUDMessages[MAX_SPEC_HUD_MESSAGES];
	char m_HUDMessageText[MAX_SPEC_HUD_MESSAGES][128];
	int m_lastHudMessage;
	overviewInfo_t m_OverviewData;
	overviewEntity_t m_OverviewEntities[MAX_OVERVIEW_ENTITIES];
	int m_iObserverFlags;
	int m_iSpectatorNumber;
	float m_mapZoom;		// zoom the user currently uses
	Vector m_mapOrigin;
	cvar_t* m_drawnames;
	cvar_t* m_drawcone;
	cvar_t* m_drawstatus;
	cvar_t* m_autoDirector;
	cvar_t* m_pip;
	cvar_t* m_scoreboard;
	cvar_t* m_mode;
	qboolean m_chatEnabled;
	qboolean m_IsInterpolating;
	int m_ChaseEntity;	// if != 0, follow this entity with viewangles
	int m_WayPoint;		// current waypoint 1
	int m_NumWayPoints;	// current number of waypoints
	Vector m_cameraOrigin;
	Vector m_cameraAngles;
	CInterpolation m_WayInterpolation;
	Vector m_vPlayerPos[MAX_PLAYERS];
	HSPRITE m_hsprPlayerBlue;
	HSPRITE m_hsprPlayerRed;
	HSPRITE m_hsprPlayer;
	HSPRITE m_hsprPlayerVIP;
	HSPRITE m_hsprPlayerC4;
	HSPRITE m_hsprCamera;
	HSPRITE m_hsprPlayerDead;
	HSPRITE m_hsprViewcone;
	HSPRITE m_hsprUnkownMap;
	HSPRITE m_hsprBeam;
	HSPRITE m_hsprBomb;
	HSPRITE m_hsprHostage;
	HSPRITE m_hsprBackpack;
	model_s* m_MapSprite;
	float m_flNextObserverInput;
	float m_FOV;
	float m_zoomDelta;
	float m_moveDelta;
	int m_lastPrimaryObject;
	int m_lastSecondaryObject;
	float m_lastAutoDirector;
	cameraWayPoint_t m_CamPath[MAX_CAM_WAYPOINTS];
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

void VectorAngles( const float* forward, float* angles );
extern "C" void NormalizeAngles( float* angles );

#endif
