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

void EV_EjectBrass( float* origin, float* velocity, float rotation, int model, int soundType, int index, int angularVelocity );
void EV_GetGunPosition( struct event_args_s* args, float* position, float* origin );
void EV_GetDefaultShellInfo( struct event_args_s* args, float* origin, float* velocity,
	float* shellVelocity, float* shellOrigin, float* forward, float* right, float* up,
	float forwardScale, float upScale, float rightScale, bool reverseDirection );
qboolean EV_IsLocal( int index );
qboolean EV_IsPlayer( int index );
void EV_CreateTracer( float* start, float* end );
struct cl_entity_s* GetEntity( int index );
struct cl_entity_s* GetViewEntity();
void EV_MuzzleFlash();

#endif // EVENTSCRIPTSH
