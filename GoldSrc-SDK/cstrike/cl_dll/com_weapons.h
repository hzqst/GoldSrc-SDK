//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// com_weapons.h
// Shared weapons common function prototypes
#ifndef COM_WEAPONSH
#define COM_WEAPONSH

#include "hud_iface.h"

float UTIL_SharedRandomFloat( unsigned int seed, float low, float high );
int UTIL_SharedRandomLong( unsigned int seed, int low, int high );
int HUD_GetWeaponAnim();
int HUD_GetWeapon();
void HUD_SendWeaponAnim( int animation, int weaponId, int body, int force );
void HUD_PlaySound( char* sound, float volume );
void HUD_PlaybackEvent( int flags, const struct edict_s* invoker, unsigned short eventIndex,
	float delay, float* origin, float* angles, float floatParam1, float floatParam2,
	int intParam1, int intParam2, int boolParam1, int boolParam2 );
void HUD_SetMaxSpeed( const struct edict_s* entity, float speed );
int stub_PrecacheModel( char* model );
int stub_PrecacheSound( char* sound );
unsigned short stub_PrecacheEvent( int type, const char* name );
const char* stub_NameForFunction( uint32 function );
void stub_SetModel( struct edict_s* entity, const char* model );

extern cvar_t* cl_lw;
extern int g_runfuncs;
extern vec3_t v_angles;
extern float g_lastFOV;
extern struct local_state_s* g_finalstate;

#endif
