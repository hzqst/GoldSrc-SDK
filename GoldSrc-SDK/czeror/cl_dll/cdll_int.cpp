/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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
//
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include "hud.h"
#include "cl_util.h"
#include "netadr.h"
#include "../../engine/interface.h"
#include "../../public/particleman.h"
#include "../game_shared/voice_status.h"
#include "../public/game_controls/legacy_iviewport.h"
//#include "vgui_schememanager.h"

extern "C"
{
#include "pm_shared.h"
}

void InitInput();
void ShutdownInput();
void IN_Commands();
void EV_HookEvents();
void CL_LoadParticleMan();
void CL_UnloadParticleMan();

cl_enginefunc_t gEngfuncs;
CHud gHUD;
IParticleMan* g_pParticleMan;
CSysModule* g_hParticleManModule;

extern "C" EXPORT int HUD_GetHullBounds( int hull, float*, float* )
{
	return (unsigned int)hull <= 2;
}

extern "C" EXPORT int HUD_ConnectionlessPacket( const netadr_s*, const char*, char*, int* responseSize )
{
	*responseSize = 0;
	return 0;
}

extern "C" EXPORT void HUD_PlayerMoveInit( playermove_s* move )
{
//	RecClClientMoveInit(ppmove);

	PM_Init( move );
}

extern "C" EXPORT char HUD_PlayerMoveTexture( char* name )
{
//	RecClClientTextureType(name);

	return PM_FindTextureType( name );
}

extern "C" EXPORT void HUD_PlayerMove( playermove_s* move, int server )
{
//	RecClClientMove(ppmove, server);

	PM_Move( move, server );
}

extern "C" EXPORT int Initialize( cl_enginefunc_t* engineFunctions, int version )
{
	memcpy( &gEngfuncs, engineFunctions, sizeof( gEngfuncs ) );
	if ( version != CLDLL_INTERFACE_VERSION )
		return 0;
	memcpy( &gEngfuncs, engineFunctions, sizeof( gEngfuncs ) );
	EV_HookEvents();
	CL_LoadParticleMan();

	// get tracker interface, if any
	return 1;
}

extern "C" EXPORT int HUD_VidInit()
{
	gHUD.VidInit();
	return 1;
}

extern "C" EXPORT void HUD_Init()
{
//	RecClHudInit();
	InitInput();
	gHUD.Init();
}

extern "C" EXPORT void HUD_Shutdown()
{
	ShutdownInput();
	CL_UnloadParticleMan();
}

extern "C" EXPORT int HUD_Redraw( float time, int intermission )
{
//	RecClHudRedraw(time, intermission);

	gHUD.Redraw( time, intermission );
	return 1;
}

extern "C" EXPORT int HUD_UpdateClientData( client_data_t* data, float time )
{
//	RecClHudUpdateClientData(pcldata, flTime);

	IN_Commands();
	return gHUD.UpdateClientData( data, time );
}

extern "C" EXPORT void HUD_Reset()
{
//	RecClHudReset();

	gHUD.VidInit();
}

extern "C" EXPORT void HUD_Frame( double time )
{
	GetClientVoice()->Frame( time );
}

extern "C" EXPORT void HUD_VoiceStatus( int index, qboolean talking )
{
	GetClientVoice()->UpdateSpeakerStatus( index, talking != 0 );
	gViewPortInterface->UpdateScoreBoard();
}

extern "C" EXPORT void HUD_DirectorMessage( int size, void* buffer )
{
//	RecClDirectorMessage(iSize, pbuf);

	gHUD.m_Spectator.DirectorMessage( size, buffer );
}

extern "C" EXPORT CreateInterfaceFn ClientFactory()
{
	return Sys_GetFactoryThis();
}

void CL_LoadParticleMan()
{
	char path[512];
	if ( !gEngfuncs.COM_ExpandFilename( PARTICLEMAN_DLLNAME, path, sizeof( path ) ) )
	{
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}
	g_hParticleManModule = Sys_LoadModule( path );
	CreateInterfaceFn factory = Sys_GetFactory( g_hParticleManModule );
	if ( !factory )
	{
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}
	g_pParticleMan = (IParticleMan*)factory( PARTICLEMAN_INTERFACE, NULL );
	if ( g_pParticleMan )
	{
		g_pParticleMan->SetUp( &gEngfuncs );
		g_pParticleMan->SetVariables( gEngfuncs.hudGetServerGravityValue(), gHUD.m_vecAngles );
		g_pParticleMan->AddCustomParticleClassSize( 316 );
		g_pParticleMan->AddCustomParticleClassSize( 284 );
	}
}

void CL_UnloadParticleMan()
{
	Sys_UnloadModule( g_hParticleManModule );
	g_pParticleMan = NULL;
	g_hParticleManModule = NULL;
}
