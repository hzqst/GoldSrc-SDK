//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// com_weapons.h
// Shared weapons common function prototypes
#ifndef COM_WEAPONS_H
#define COM_WEAPONS_H

#include "hud_iface.h"
#include "Exports.h"

void			COM_Log( char* filename, char* format, ... );
int				CL_IsDead();
float			UTIL_SharedRandomFloat( unsigned int seed, float low, float high );
int				UTIL_SharedRandomLong( unsigned int seed, int low, int high );
int				HUD_GetWeaponAnim();
void			HUD_SendWeaponAnim( int animation, int force );
void			HUD_PlaySound( char* sound, float volume );
void			HUD_PlaybackEvent( int flags, const struct edict_s* invoker, unsigned short event,
	float delay, float* origin, float* angles, float first, float second,
	int firstInt, int secondInt, int firstBool, int secondBool );
void			HUD_SetMaxSpeed( const struct edict_s* entity, float speed );
int				stub_PrecacheModel( char* name );
int				stub_PrecacheSound( char* name );
unsigned short	stub_PrecacheEvent( int type, const char* name );
const char*		stub_NameForFunction( uint32 function );
void			stub_SetModel( struct edict_s* entity, const char* name );

extern cvar_t* cl_lw;
extern int g_runfuncs;
extern struct local_state_s* g_finalstate;

#endif
