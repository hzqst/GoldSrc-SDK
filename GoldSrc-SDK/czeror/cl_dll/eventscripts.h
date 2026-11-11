//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// eventscripts.h
#ifndef EVENTSCRIPTSH
#define EVENTSCRIPTSH

// defaults for clientinfo messages
#define DEFAULT_VIEWHEIGHT 28
#define VEC_DUCK_VIEW 12

struct event_args_s;
struct cl_entity_s;

void EV_EjectBrass( float* origin, float* velocity, float rotation, int model, int soundType, int index, int angleVelocity );
void EV_GetGunPosition( event_args_s* arguments, float* position, float* origin );
void EV_GetDefaultShellInfo( event_args_s* arguments, float* origin, float* velocity, float* shellVelocity,
	float* shellOrigin, float* forward, float* right, float* up, float forwardScale, float upScale,
	float rightScale, bool reverseDirection );
qboolean EV_IsLocal( int index );
qboolean EV_IsPlayer( int index );
void EV_CreateTracer( float* start, float* end );
cl_entity_s* GetEntity( int index );
cl_entity_s* GetViewEntity();
void EV_MuzzleFlash();

#endif
