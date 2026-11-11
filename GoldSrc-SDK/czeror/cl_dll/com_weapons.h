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

#include "cl_dll.h"

struct edict_s;
struct local_state_s;

void COM_Log( char* filename, char* format, ... );
int CL_IsDead();
float UTIL_SharedRandomFloat( unsigned int seed, float low, float high );
int UTIL_SharedRandomLong( unsigned int seed, int low, int high );
int HUD_GetWeaponAnim();
void HUD_SendWeaponAnim( int animation, int body, int force );
void HUD_PlaySound( char* sound, float volume );
void HUD_PlaybackEvent( int flags, const edict_s* invoker, unsigned short eventIndex, float delay,
	float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
void HUD_SetMaxSpeed( const edict_s* entity, float speed );
int stub_PrecacheModel( char* name );
int stub_PrecacheSound( char* name );
unsigned short stub_PrecacheEvent( int type, const char* name );
const char* stub_NameForFunction( uint32 function );
void stub_SetModel( edict_s* entity, const char* model );

extern cvar_t* cl_lw;
extern int g_runfuncs;
extern vec3_t v_angles;
extern float g_lastFOV;
extern local_state_s* g_finalstate;

#endif
