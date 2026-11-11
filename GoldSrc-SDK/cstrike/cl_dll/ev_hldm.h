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
	BULLET_PLAYER_357SIG
} Bullet;

enum glock_e
{
	GLOCK_IDLE1, GLOCK_IDLE2, GLOCK_IDLE3, GLOCK_SHOOT, GLOCK_SHOOT_EMPTY,
	GLOCK_RELOAD, GLOCK_RELOAD_NOT_EMPTY, GLOCK_DRAW, GLOCK_HOLSTER, GLOCK_ADD_SILENCER
};

enum shotgun_e
{
	SHOTGUN_IDLE, SHOTGUN_FIRE, SHOTGUN_FIRE2, SHOTGUN_RELOAD, SHOTGUN_PUMP,
	SHOTGUN_START_RELOAD, SHOTGUN_DRAW, SHOTGUN_HOLSTER, SHOTGUN_IDLE4, SHOTGUN_IDLE_DEEP
};

enum mp5_e
{
	MP5_LONGIDLE, MP5_IDLE1, MP5_LAUNCH, MP5_RELOAD, MP5_DEPLOY, MP5_FIRE1, MP5_FIRE2, MP5_FIRE3
};

enum python_e
{
	PYTHON_IDLE1, PYTHON_FIDGET, PYTHON_FIRE1, PYTHON_RELOAD, PYTHON_HOLSTER,
	PYTHON_DRAW, PYTHON_IDLE2, PYTHON_IDLE3
};

enum gauss_e
{
	GAUSS_IDLE, GAUSS_IDLE2, GAUSS_FIDGET, GAUSS_SPINUP, GAUSS_SPIN, GAUSS_FIRE,
	GAUSS_FIRE2, GAUSS_HOLSTER, GAUSS_DRAW
};

enum usp_e
{
	USP_IDLE, USP_SHOOT1, USP_SHOOT2, USP_SHOOT3, USP_SHOOT_EMPTY, USP_RELOAD,
	USP_DRAW, USP_ATTACH_SILENCER, USP_UNSIL_IDLE, USP_UNSIL_SHOOT1,
	USP_UNSIL_SHOOT2, USP_UNSIL_SHOOT3, USP_UNSIL_SHOOT_EMPTY, USP_UNSIL_RELOAD,
	USP_UNSIL_DRAW, USP_DETACH_SILENCER
};

enum usp_shield_e
{
	USP_SHIELD_IDLE, USP_SHIELD_SHOOT1, USP_SHIELD_SHOOT2, USP_SHIELD_SHOOT_EMPTY,
	USP_SHIELD_RELOAD, USP_SHIELD_DRAW, USP_SHIELD_UP_IDLE, USP_SHIELD_UP, USP_SHIELD_DOWN
};

enum mp5n_e
{
	MP5N_IDLE1, MP5N_RELOAD, MP5N_DRAW, MP5N_SHOOT1, MP5N_SHOOT2, MP5N_SHOOT3
};

enum ak47_e
{
	AK47_IDLE1, AK47_RELOAD, AK47_DRAW, AK47_SHOOT1, AK47_SHOOT2, AK47_SHOOT3
};

enum aug_e
{
	AUG_IDLE1, AUG_RELOAD, AUG_DRAW, AUG_SHOOT1, AUG_SHOOT2, AUG_SHOOT3
};

enum deagle_e
{
	DEAGLE_IDLE1, DEAGLE_SHOOT1, DEAGLE_SHOOT2, DEAGLE_SHOOT_EMPTY, DEAGLE_RELOAD, DEAGLE_DRAW
};

enum g3sg1_e
{
	G3SG1_IDLE, G3SG1_SHOOT, G3SG1_SHOOT2, G3SG1_RELOAD, G3SG1_DRAW
};

enum glock18_e
{
	GLOCK18_IDLE1, GLOCK18_IDLE2, GLOCK18_IDLE3, GLOCK18_SHOOT, GLOCK18_SHOOT2,
	GLOCK18_SHOOT3, GLOCK18_SHOOT_EMPTY, GLOCK18_RELOAD, GLOCK18_DRAW,
	GLOCK18_HOLSTER, GLOCK18_ADD_SILENCER, GLOCK18_DRAW2, GLOCK18_RELOAD2
};

enum glock18_shield_e
{
	GLOCK18_SHIELD_IDLE1, GLOCK18_SHIELD_SHOOT, GLOCK18_SHIELD_SHOOT2,
	GLOCK18_SHIELD_SHOOT_EMPTY, GLOCK18_SHIELD_RELOAD, GLOCK18_SHIELD_DRAW,
	GLOCK18_SHIELD_IDLE, GLOCK18_SHIELD_UP, GLOCK18_SHIELD_DOWN
};

enum m249_e
{
	M249_IDLE1, M249_SHOOT1, M249_SHOOT2, M249_RELOAD, M249_DRAW
};

enum m3_e
{
	M3_IDLE, M3_FIRE1, M3_FIRE2, M3_RELOAD, M3_PUMP, M3_START_RELOAD, M3_DRAW, M3_HOLSTER
};

enum m4a1_e
{
	M4A1_IDLE, M4A1_SHOOT1, M4A1_SHOOT2, M4A1_SHOOT3, M4A1_RELOAD, M4A1_DRAW,
	M4A1_ATTACH_SILENCER, M4A1_UNSIL_IDLE, M4A1_UNSIL_SHOOT1, M4A1_UNSIL_SHOOT2,
	M4A1_UNSIL_SHOOT3, M4A1_UNSIL_RELOAD, M4A1_UNSIL_DRAW, M4A1_DETACH_SILENCER
};

enum mac10_e
{
	MAC10_IDLE1, MAC10_RELOAD, MAC10_DRAW, MAC10_SHOOT1, MAC10_SHOOT2, MAC10_SHOOT3
};

enum p228_e
{
	P228_IDLE, P228_SHOOT1, P228_SHOOT2, P228_SHOOT3, P228_SHOOT_EMPTY, P228_RELOAD, P228_DRAW
};

enum p228_shield_e
{
	P228_SHIELD_IDLE, P228_SHIELD_SHOOT1, P228_SHIELD_SHOOT2, P228_SHIELD_SHOOT_EMPTY,
	P228_SHIELD_RELOAD, P228_SHIELD_DRAW, P228_SHIELD_IDLE_UP, P228_SHIELD_UP, P228_SHIELD_DOWN
};

enum p90_e
{
	P90_IDLE1, P90_RELOAD, P90_DRAW, P90_SHOOT1, P90_SHOOT2, P90_SHOOT3
};

enum xm1014_e
{
	XM1014_IDLE, XM1014_FIRE1, XM1014_FIRE2, XM1014_RELOAD, XM1014_PUMP, XM1014_START_RELOAD, XM1014_DRAW
};

enum scout_e
{
	SCOUT_IDLE, SCOUT_SHOOT, SCOUT_SHOOT2, SCOUT_RELOAD, SCOUT_DRAW
};

enum awp_e
{
	AWP_IDLE, AWP_SHOOT, AWP_SHOOT2, AWP_SHOOT3, AWP_RELOAD, AWP_DRAW
};

enum sg552_e
{
	SG552_IDLE1, SG552_RELOAD, SG552_DRAW, SG552_SHOOT1, SG552_SHOOT2, SG552_SHOOT3
};

enum tmp_e
{
	TMP_IDLE1, TMP_RELOAD, TMP_DRAW, TMP_SHOOT1, TMP_SHOOT2, TMP_SHOOT3
};

enum elite_e
{
	ELITE_IDLE, ELITE_IDLE_LEFTEMPTY, ELITE_SHOOTLEFT1, ELITE_SHOOTLEFT2,
	ELITE_SHOOTLEFT3, ELITE_SHOOTLEFT4, ELITE_SHOOTLEFT5, ELITE_SHOOTLEFTLAST,
	ELITE_SHOOTRIGHT1, ELITE_SHOOTRIGHT2, ELITE_SHOOTRIGHT3, ELITE_SHOOTRIGHT4,
	ELITE_SHOOTRIGHT5, ELITE_SHOOTRIGHTLAST, ELITE_RELOAD, ELITE_DRAW
};

enum fiveseven_e
{
	FIVESEVEN_IDLE, FIVESEVEN_SHOOT1, FIVESEVEN_SHOOT2, FIVESEVEN_SHOOT_EMPTY,
	FIVESEVEN_RELOAD, FIVESEVEN_DRAW
};

enum ump45_e
{
	UMP45_IDLE1, UMP45_RELOAD, UMP45_DRAW, UMP45_SHOOT1, UMP45_SHOOT2, UMP45_SHOOT3
};

enum sg550_e
{
	SG550_IDLE, SG550_SHOOT, SG550_SHOOT2, SG550_RELOAD, SG550_DRAW
};

void EV_HLDM_GunshotDecalTrace( struct pmtrace_s* trace, char* decalName, char textureType );
void EV_HLDM_DecalGunshot( struct pmtrace_s* trace, int bulletType, float scale,
	int red, int green, int blue, bool createSparks, char textureType );
int EV_HLDM_CheckTracer( int index, float* source, float* end, float* forward, float* right,
	int bulletType, int tracerFrequency, int* tracerCount );
void EV_HLDM_FireBullets( int index, float* forward, float* right, float* up, int shots,
	float* source, float* direction, float* spread, float distance, int bulletType,
	int tracerFrequency, int* tracerCount, int penetration );
void EV_HLDM_CreateSmoke( float* origin, float* direction, int speed, float scale,
	int red, int green, int blue, int smokeType, float* baseVelocity, bool wind, int frameRate );
void CreateCorpse( Vector origin, Vector angles, const char* model, float animationTime, int sequence, int body );

extern "C"
{
	void EV_FireGlock1( struct event_args_s* args );
	void EV_FireGlock2( struct event_args_s* args );
	void EV_FireShotGunSingle( struct event_args_s* args );
	void EV_FireShotGunDouble( struct event_args_s* args );
	void EV_FireMP5( struct event_args_s* args );
	void EV_FirePython( struct event_args_s* args );
	void EV_FireGauss( struct event_args_s* args );
	void EV_SpinGauss( struct event_args_s* args );
	void EV_TrainPitchAdjust( struct event_args_s* args );
	void EV_VehiclePitchAdjust( struct event_args_s* args );
	void EV_FireUSP( struct event_args_s* args );
	void EV_FireMP5N( struct event_args_s* args );
	void EV_FireAK47( struct event_args_s* args );
	void EV_FireAug( struct event_args_s* args );
	void EV_FireDeagle( struct event_args_s* args );
	void EV_FireG3SG1( struct event_args_s* args );
	void EV_FireSG550( struct event_args_s* args );
	void EV_FireGlock18( struct event_args_s* args );
	void EV_FireM249( struct event_args_s* args );
	void EV_FireM3( struct event_args_s* args );
	void EV_FireM4A1( struct event_args_s* args );
	void EV_FireMac10( struct event_args_s* args );
	void EV_FireP90( struct event_args_s* args );
	void EV_FireP228( struct event_args_s* args );
	void EV_FireAWP( struct event_args_s* args );
	void EV_FireScout( struct event_args_s* args );
	void EV_FireSG552( struct event_args_s* args );
	void EV_FireTMP( struct event_args_s* args );
	void EV_FireFiveSeven( struct event_args_s* args );
	void EV_FireUMP45( struct event_args_s* args );
	void EV_FireXM1014( struct event_args_s* args );
	void EV_FireEliteLeft( struct event_args_s* args );
	void EV_FireEliteRight( struct event_args_s* args );
	void EV_Knife( struct event_args_s* args );
	void EV_FireGalil( struct event_args_s* args );
	void EV_FireFamas( struct event_args_s* args );
	void EV_RemoveAllDecals( struct event_args_s* args );
	void EV_CreateSmoke( struct event_args_s* args );
}

#endif // EV_HLDMH
