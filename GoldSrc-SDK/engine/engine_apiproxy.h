#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "quakedef.h"
#include "../common/const.h"
#include "wrect.h"

typedef int ( *pfnUserMsgHook )( const char*, int, void* );

#include "cdll_int.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

struct triangleapi_s;

extern cldll_func_dst_t g_cldstAddrs;
extern qboolean fClientLoaded;
extern triangleapi_s tri;

void		NullDst();
void		ClientDLL_ClearStates();
void		ClientDLL_ReadDemoBuffer( int size, byte* buffer );
void		ClientDLL_DemoUpdateClientData( client_data_t* data );

void		hudGetPlayerInfo( int entityNumber, hud_player_info_t* info );
int			hudGetScreenInfo( SCREENINFO* info );
int			hudCheckParm( char* parameter, char** next );

cvar_s*		hudRegisterVariable( char* name, char* value, int flags );
float		hudGetCvarFloat( char* name );
char*		hudGetCvarString( char* name );
int			hudAddCommand( char* name, void ( *function )() );
int			hudHookUserMsg( char* name, pfnUserMsgHook hook );
int			hudServerCmd( char* text );
int			hudServerCmdUnreliable( char* text );
int			hudClientCmd( char* text );
int			hudFilteredClientCmd( char* text );

void		hudPlaySoundByName( char* name, float volume );
void		hudPlaySoundByIndex( int index, float volume );
void		hudPlaySoundByNameAtLocation( char* name, float volume, float* origin );
void		hudPlaySoundVoiceByName( char* name, float volume, int pitch );
void		hudPlaySoundByNameAtPitch( char* name, float volume, int pitch );

void		hudDrawConsoleStringLen( const char* text, int* width, int* height );
void		hudConsolePrint( const char* text );
void		hudCenterPrint( const char* text );
