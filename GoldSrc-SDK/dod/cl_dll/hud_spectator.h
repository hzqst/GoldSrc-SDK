//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef HUD_SPECTATOR_H
#define HUD_SPECTATOR_H

#include "cl_entity.h"

#define INSET_OFF 0
#define INSET_CHASE_FREE 1
#define INSET_IN_EYE 2
#define INSET_MAP_FREE 3
#define INSET_MAP_CHASE 4
#define MAX_SPEC_HUD_MESSAGES 8
#define OVERVIEW_TILE_SIZE 128		// don't change this
#define OVERVIEW_MAX_LAYERS 1
#define MAX_OVERVIEW_ENTITIES 128

typedef struct overviewInfo_s
{
	char map[64];	// cl.levelname or empty
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
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
} overviewInfo_t;

typedef struct overviewEntity_s
{
	HSPRITE hSprite;
	cl_entity_t* entity;
	double killTime;
	bool b_dodMapTag;
	Vector origin;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
} overviewEntity_t;

const char* GetSpectatorLabel( int mode );
void VectorAngles( const float* forward, float* angles );
void NormalizeAngles( float* angles );

class CHudSpectator : public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	int Draw( float time ) override;
	void InitHUDData() override;
	void Reset() override;
	void SetModes( int mainMode, int insetMode );
	void CheckSettings();
	int ToggleInset( bool allowOff );
	void HandleButtonsDown( int buttons );
	void HandleButtonsUp( int buttons );
	void FindNextPlayer( bool reverse );
	void FindPlayer( const char* name );
	void DirectorMessage( int size, void* buffer );
	void SetSpectatorStartPosition();
	bool IsActivePlayer( cl_entity_t* entity );
	bool ParseOverviewFile();
	void LoadMapSprites();
	void DrawOverviewLayer();
	void DrawOverviewEntities();
	void DrawOverview();
	void CheckOverviewEntities();
	bool AddOverviewEntity( int type, cl_entity_t* entity, const char* modelname );
	bool AddOverviewEntityToList( HSPRITE sprite, cl_entity_t* entity, double killTime );
	bool AddOverviewEntityToMap( HSPRITE sprite, cl_entity_t* entity, double killTime, Vector origin );
	void DeathMessage( int victim );
	HSPRITE GetMarkerSPR( int marker );
	void AddVoiceIconToPlayerEnt( int index );
	void ClearVoiceIconFlags();
	bool ShouldSetVoiceIcon( int index );

	int m_hsprMapMarkers[15];
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
	cvar_t* default_fov;
	HSPRITE m_hsprAllieZone;
	HSPRITE m_hsprAxisZone;
	HSPRITE m_hsprCapZone;
	HSPRITE m_hsprTnT;
	HSPRITE m_hsprBanglr;
	HSPRITE m_hsprGrenade;
	HSPRITE m_hsprStick;
	HSPRITE m_hsprCustom;
	HSPRITE m_hsprVoiceIcon;
	HSPRITE m_hsprSpeakerIcon;
	HSPRITE m_hsprAllieLight;
	HSPRITE m_hsprAxisLight;
	HSPRITE m_hsprBritLight;
	qboolean m_chatEnabled;
	Vector m_cameraOrigin;
	Vector m_cameraAngles;
	model_t* m_MapSprite;
	Vector m_vPlayerPos[64];
	HSPRITE m_hsprUnkownMap;
	HSPRITE m_hsprBeam;
	HSPRITE m_hsprCamera;
	HSPRITE m_hsprPlayer;
	HSPRITE m_hsprCameraAllies;
	HSPRITE m_hsprCameraAxis;
	HSPRITE m_hsprCameraBrit;
	HSPRITE m_hsprCameraSpec;
	bool m_bAddDrawIconNextFrame[64];
	HSPRITE m_hsprPlayerDead;
	HSPRITE m_hsprViewcone;
	float m_flNextObserverInput;
	float m_zoomDelta;
	float m_moveDelta;
	int m_lastPrimaryObject;
	int m_lastSecondaryObject;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

#endif
