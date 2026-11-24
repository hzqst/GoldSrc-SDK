/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

// Tracktrain spawn flags
#pragma once

#define SF_TRACKTRAIN_NOPITCH		0x0001
#define SF_TRACKTRAIN_NOCONTROL		0x0002
#define SF_TRACKTRAIN_FORWARDONLY	0x0004
#define SF_TRACKTRAIN_PASSABLE		0x0008

#define SF_PATH_DISABLED				0x0001
#define SF_PATH_FIREONCE				0x0002
#define SF_PATH_ALTREVERSE			0x0004
#define SF_PATH_DISABLE_TRAIN		0x0008
#define SF_PATH_ALTERNATE			0x8000

#define SF_CORNER_WAITFORTRIG		0x0001
#define SF_CORNER_TELEPORT			0x0002
#define SF_CORNER_FIREONCE			0x0004

class CBasePlayer;

//#define PATH_SPARKLE_DEBUG		1	// This makes a particle effect around path_track entities for debugging
class CPathTrack : public CPointEntity
{
public:
	void Spawn() override;
	void Activate() override;
	void KeyValue( KeyValueData* pkvd ) override;
	void SetPrevious( CPathTrack* pPrevious );
	void Link();
	void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value ) override;
	CPathTrack* ValidPath( CPathTrack* pPath, int testFlag );
	void Project( CPathTrack* pStart, CPathTrack* pEnd, Vector* origin, float dist );
	static CPathTrack* Instance( edict_t* pent );
	CPathTrack* LookAhead( Vector* origin, float dist, int move );
	CPathTrack* Nearest( Vector origin );
	CPathTrack* GetNext();
	CPathTrack* GetPrevious();
	int Save( CSave& save ) override;
	int Restore( CRestore& restore ) override;
	static TYPEDESCRIPTION m_SaveData[];

	float		m_length;
	string_t	m_altName;
	CPathTrack*	m_pnext;
	CPathTrack*	m_pprevious;
	CPathTrack*	m_paltpath;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class CFuncTrackTrain : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void Blocked( CBaseEntity* pOther ) override;
	void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value ) override;
	void KeyValue( KeyValueData* pkvd ) override;
	void EXPORT Next();
	void EXPORT Find();
	void EXPORT NearestPath();
	void EXPORT DeadEnd();
	void NextThink( float thinkTime, BOOL alwaysThink );
	void SetTrack( CPathTrack* track ) { m_ppath = track->Nearest( pev->origin ); }
	void SetControls( entvars_t* pevControls );
	BOOL OnControls( entvars_t* pev ) override;
	void StopSound();
	void UpdateSound();
	static CFuncTrackTrain* Instance( edict_t* pent );
	int Save( CSave& save ) override;
	int Restore( CRestore& restore ) override;
	static TYPEDESCRIPTION m_SaveData[];
	int ObjectCaps() override
	{
		return ( CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ) | FCAP_DIRECTIONAL_USE;
	}
	void OverrideReset() override;
	void Restart() override;

	CPathTrack*	m_ppath;
	float		m_length;
	float		m_height;
	float		m_speed;
	float		m_dir;
	float		m_startSpeed;
	Vector		m_controlMins;
	Vector		m_controlMaxs;
	int			m_soundPlaying;
	int			m_sounds;
	float		m_flVolume;
	float		m_flBank;
	float		m_oldSpeed;
	float		m_fTurnAngle;
	float		m_flSteeringWheelDecay;
	float		m_flAcceleratorDecay;

private:
	unsigned short m_usAdjustPitch;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class CFuncVehicle : public CBaseEntity
{
public:
	void Spawn() override;
	void Precache() override;
	void Blocked( CBaseEntity* pOther ) override;
	void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value ) override;
	void KeyValue( KeyValueData* pkvd ) override;
	void EXPORT Next();
	void EXPORT Find();
	void EXPORT NearestPath();
	void EXPORT DeadEnd();
	void NextThink( float thinkTime, BOOL alwaysThink );
	int Classify() override;
	void CollisionDetection();
	void TerrainFollowing();
	void CheckTurning();
	void SetTrack( CPathTrack* track ) { m_ppath = track->Nearest( pev->origin ); }
	void SetControls( entvars_t* pevControls );
	BOOL OnControls( entvars_t* pev ) override;
	void StopSound();
	void UpdateSound();
	static CFuncVehicle* Instance( edict_t* pent );
	int Save( CSave& save ) override;
	int Restore( CRestore& restore ) override;
	static TYPEDESCRIPTION m_SaveData[];
	int ObjectCaps() override
	{
		return ( CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ) | FCAP_DIRECTIONAL_USE;
	}
	void OverrideReset() override;

	CPathTrack*	m_ppath;
	float		m_length;
	float		m_width;
	float		m_height;
	float		m_speed;
	float		m_dir;
	float		m_startSpeed;
	Vector		m_controlMins;
	Vector		m_controlMaxs;
	int			m_soundPlaying;
	int			m_sounds;
	int			m_acceleration;
	float		m_flVolume;
	float		m_flBank;
	float		m_oldSpeed;
	int			m_iTurnAngle;
	float		m_flSteeringWheelDecay;
	float		m_flAcceleratorDecay;
	float		m_flTurnStartTime;
	float		m_flLaunchTime;				//Time at which the vehicle has become airborne
	float		m_flLastNormalZ;
	float		m_flCanTurnNow;
	float		m_flUpdateSound;
	Vector		m_vFrontLeft;
	Vector		m_vFront;
	Vector		m_vFrontRight;
	Vector		m_vBackLeft;
	Vector		m_vBack;
	Vector		m_vBackRight;
	Vector		m_vSurfaceNormal;
	Vector		m_vVehicleDirection;
	// The player driving the vehicle, who gets the frags of the players and hostages it runs over.
	// A handle and not a CBasePlayer* as in the SDK: the driver can disconnect while it is set.
	EHANDLE		m_hDriver;

	// GOOSEMAN
	void Restart() override;

private:
	unsigned short m_usAdjustPitch;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};
