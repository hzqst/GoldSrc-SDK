//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef EV_HLDMH
#define EV_HLDMH

// bullet types
typedef enum
{
	BULLET_NONE = 0,
	BULLET_PLAYER_9MM, // glock
	BULLET_PLAYER_MP5, // mp5
	BULLET_PLAYER_357, // python
	BULLET_PLAYER_BUCKSHOT, // shotgun
	BULLET_PLAYER_CROWBAR, // crowbar swipe
	BULLET_MONSTER_9MM,
	BULLET_MONSTER_MP5,
	BULLET_MONSTER_12MM,
	BULLET_PLAYER_45ACP,
	BULLET_PLAYER_338MAG,
	BULLET_PLAYER_762MM,
	BULLET_PLAYER_556MM,
	BULLET_PLAYER_50AE,
	BULLET_PLAYER_57MM,
	BULLET_PLAYER_357SIG,
	BULLET_PLAYER_BLOWTORCH
} Bullet;

struct event_args_s;
struct pmtrace_s;
struct physent_s;

struct materials_t
{
	char m_chMaterialType;
	char m_szTextName[32];
	int m_iGibMultiplier;
	int m_iDebriSpriteNum;
	float m_flSize;
	float m_flScaleSpeed;
	float m_flBrightness;
	float m_flFadeSpeed;
	float m_flLife;
	float m_flGravity;
	float m_flAfterDampGravity;
	float m_flDampingVelocity;
	int m_iFrame;
	Vector m_vVelocity;
	float m_flVelMod;
	int m_iRendermode;
	Vector m_vColor;
	float m_flDampingTime;
	int m_iCollisionFlags;
	int m_iAfterDampFlags;
	int m_iSpawnDebris;
	int m_iSpawnSmoke;
	int m_iSpawnSparks;
	int m_iSpawnImpacts;
	Vector m_vSmokeColor;
	char m_szBulletImpactName[32];
	float m_flMinScale;
	float m_flMaxScale;
	int m_iDecalNum;
	int m_bRandomFlip;
	int m_iIndex;
	char m_szOriginalName[32];
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

void MAT_AddMetalSpark( Vector origin, Vector normal );
void EV_DebriSystem( char textureType, pmtrace_s* trace, int bulletType, int materialIndex );
void CreateCorpse( Vector origin, Vector angles, const char* model, float animationTime, int sequence, int body );

char* EV_HLDM_DamageDecal( physent_s* entity );
char EV_HLDM_PlayTextureSound( int index, pmtrace_s* trace, float* start, float* end, int bulletType );
void EV_HLDM_GunshotDecalTrace( pmtrace_s* trace, char* decalName, char textureType );
void EV_HLDM_DecalGunshot( pmtrace_s* trace, int bulletType, float scale, int red, int green, int blue,
	bool stuck, char textureType, int materialIndex );
int EV_HLDM_CheckTracer( int index, float* source, float* end, float* forward, float* right,
	int bulletType, int frequency, int* count );
void EV_HLDM_FireBullets( int index, float* forward, float* right, float* up, int shots,
	float* source, float* direction, float* spread, float distance, int bulletType,
	int tracerFrequency, int* tracerCount, int penetration );
void EV_HLDM_CreateLight( float* origin, float radius, int red, int green, int blue, float duration );
void EV_HLDM_CreateSmoke( float* origin, float* direction, int speed, float scale, int red, int green,
	int blue, int smokeType, float* baseVelocity, bool wind, int frameRate );

extern "C"
{
	void EV_FireUSP( event_args_s* arguments );
	void EV_FireMP5N( event_args_s* arguments );
	void EV_FireAK47( event_args_s* arguments );
	void EV_FireAug( event_args_s* arguments );
	void EV_FireDeagle( event_args_s* arguments );
	void EV_FireG3SG1( event_args_s* arguments );
	void EV_FireSG550( event_args_s* arguments );
	void EV_FireGlock18( event_args_s* arguments );
	void EV_FireM249( event_args_s* arguments );
	void EV_FireM60( event_args_s* arguments );
	void EV_FireM3( event_args_s* arguments );
	void EV_FireM4A1( event_args_s* arguments );
	void EV_FireMac10( event_args_s* arguments );
	void EV_FireP90( event_args_s* arguments );
	void EV_FireP228( event_args_s* arguments );
	void EV_FireAWP( event_args_s* arguments );
	void EV_FireScout( event_args_s* arguments );
	void EV_FireSG552( event_args_s* arguments );
	void EV_FireTMP( event_args_s* arguments );
	void EV_FireFiveSeven( event_args_s* arguments );
	void EV_FireUMP45( event_args_s* arguments );
	void EV_FireXM1014( event_args_s* arguments );
	void EV_FireEliteLeft( event_args_s* arguments );
	void EV_FireEliteRight( event_args_s* arguments );
	void EV_Knife( event_args_s* arguments );
	void EV_FireGalil( event_args_s* arguments );
	void EV_FireCamera( event_args_s* arguments );
	void EV_FireFiberOpticCamera( event_args_s* arguments );
	void EV_FireFamas( event_args_s* arguments );
	void EV_RemoveAllDecals( event_args_s* arguments );
	void EV_FireShieldGun( event_args_s* arguments );
	void EV_HolsterBlowtorch( event_args_s* arguments );
	void EV_IdleBlowtorch( event_args_s* arguments );
	void EV_FireBlowtorch( event_args_s* arguments );
	void EV_FireRadioControlledBomb( event_args_s* arguments );
	void EV_FireLaws( event_args_s* arguments );
	void EV_FireBriefcase( event_args_s* arguments );
	void EV_FireMedkit( event_args_s* arguments );
	void EV_FireSyringe( event_args_s* arguments );
	void EV_FireRadio( event_args_s* arguments );
	void EV_FireZipline( event_args_s* arguments );
	void EV_CreateGlass( event_args_s* arguments );
	void EV_GrenadeExplosion( event_args_s* arguments );
	void EV_CreateSmoke( event_args_s* arguments );
	void EV_FireGlock1( event_args_s* arguments );
	void EV_FireGlock2( event_args_s* arguments );
	void EV_FireShotGunSingle( event_args_s* arguments );
	void EV_FireShotGunDouble( event_args_s* arguments );
	void EV_FireMP5( event_args_s* arguments );
	void EV_FirePython( event_args_s* arguments );
	void EV_FireGauss( event_args_s* arguments );
	void EV_SpinGauss( event_args_s* arguments );
	void EV_TrainPitchAdjust( event_args_s* arguments );
	void EV_VehiclePitchAdjust( event_args_s* arguments );
}

#endif
