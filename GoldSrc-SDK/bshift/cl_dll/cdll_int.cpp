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
#include "interface.h"
#include "Exports.h"
#include "particleman.h"
#include "game_controls/iviewport.h"
#include "cl_dll/IGameClientExports.h"
//#include "vgui_schememanager.h"

extern "C"
{
#include "pm_shared.h"
}

#include <string.h>

cl_enginefunc_t gEngfuncs;
CHud gHUD;
CSysModule* g_hParticleManModule = NULL;
CSysModule* g_hTrackerModule = NULL;
IParticleMan* g_pParticleMan = NULL;
cldll_func_dst_t* g_pcldstAddrs;

class IFriendsUser;
IFriendsUser* g_pFriendsUser = NULL;

void InitInput();
void EV_HookEvents();
void IN_Commands();
void CL_LoadParticleMan();

int CL_DLLEXPORT HUD_GetHullBounds( int hullnumber, float*, float* )
{
	return (unsigned int)hullnumber <= 2;
}

int CL_DLLEXPORT HUD_ConnectionlessPacket( const struct netadr_s*, const char*, char*, int* response_buffer_size )
{

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

extern "C" int CL_DLLEXPORT HUD_GetPlayerTeam( int )
{
	return 0;
}

extern "C" CreateInterfaceFn CL_DLLEXPORT ClientFactory()
{
	return Sys_GetFactoryThis();
}

void CL_DLLEXPORT HUD_PlayerMoveInit( struct playermove_s* ppmove )
{
//	RecClClientMoveInit(ppmove);

	PM_Init( ppmove );
}

char CL_DLLEXPORT HUD_PlayerMoveTexture( char* name )
{
//	RecClClientTextureType(name);

	return PM_FindTextureType( name );
}

void CL_DLLEXPORT HUD_PlayerMove( struct playermove_s* ppmove, int server )
{
//	RecClClientMove(ppmove, server);

	PM_Move( ppmove, server );
}

int CL_DLLEXPORT Initialize( cl_enginefunc_t* pEnginefuncs, int iVersion )
{
	gEngfuncs = *pEnginefuncs;

//	RecClInitialize(pEnginefuncs, iVersion);

	if ( iVersion != CLDLL_INTERFACE_VERSION )
		return 0;

	memcpy( &gEngfuncs, pEnginefuncs, sizeof( gEngfuncs ) );
	EV_HookEvents();
	CL_LoadParticleMan();

	char szDir[512];
	if ( !gEngfuncs.COM_ExpandFilename( "Friends/TrackerUI.dll", szDir, sizeof( szDir ) ) )
	{
		g_pFriendsUser = NULL;
		g_hTrackerModule = NULL;
		return 1;
	}

	g_hTrackerModule = Sys_LoadModule( szDir );
	CreateInterfaceFn trackerFactory = Sys_GetFactory( g_hTrackerModule );
	if ( !trackerFactory )
	{
		g_pFriendsUser = NULL;
		g_hTrackerModule = NULL;
		return 1;
	}

	g_pFriendsUser = (IFriendsUser*)trackerFactory( "FriendsUser001", NULL );
	return 1;
}

int CL_DLLEXPORT HUD_VidInit()
{
	gHUD.VidInit();
	return 1;
}

/*
==========================
	HUD_Init

Called whenever the client connects
to a server.  Reinitializes all 
the hud variables.
==========================
*/

void CL_DLLEXPORT HUD_Init()
{
//	RecClHudInit();
	InitInput();
	gHUD.Init();
}


/*
==========================
	HUD_Redraw

called every screen frame to
redraw the HUD.
===========================
*/

int CL_DLLEXPORT HUD_Redraw( float time, int intermission )
{
//	RecClHudRedraw(time, intermission);

	gHUD.Redraw( time, intermission );
	return 1;
}


/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/

int CL_DLLEXPORT HUD_UpdateClientData( client_data_t* pcldata, float flTime )
{
//	RecClHudUpdateClientData(pcldata, flTime);

	IN_Commands();
	return gHUD.UpdateClientData( pcldata, flTime );
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/

void CL_DLLEXPORT HUD_Reset()
{
//	RecClHudReset();

	gHUD.VidInit();
}

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/

void CL_DLLEXPORT HUD_Frame( double time )
{
	GetClientVoice()->Frame( time );
}


/*
==========================
HUD_VoiceStatus

Called when a player starts or stops talking.
==========================
*/

void CL_DLLEXPORT HUD_VoiceStatus( int entindex, qboolean bTalking )
{
	GetClientVoice()->UpdateSpeakerStatus( entindex, bTalking != 0 );
	gViewPortInterface->UpdateScoreBoard();
}

/*
==========================
HUD_DirectorMessage

Called when a director event message was received
==========================
*/

void CL_DLLEXPORT HUD_DirectorMessage( int iSize, void* pbuf )
{
//	RecClDirectorMessage(iSize, pbuf);

	gHUD.m_Spectator.DirectorMessage( iSize, pbuf );
}

void CL_UnloadParticleMan()
{
	Sys_UnloadModule( g_hParticleManModule );
	g_pParticleMan = NULL;
	g_hParticleManModule = NULL;
}

void CL_LoadParticleMan()
{
	char szPDir[512];
	if ( !gEngfuncs.COM_ExpandFilename( PARTICLEMAN_DLLNAME, szPDir, sizeof( szPDir ) ) )
	{
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}

	g_hParticleManModule = Sys_LoadModule( szPDir );
	CreateInterfaceFn particleManFactory = Sys_GetFactory( g_hParticleManModule );
	if ( !particleManFactory )
	{
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}

	g_pParticleMan = (IParticleMan*)particleManFactory( PARTICLEMAN_INTERFACE, NULL );
	if ( g_pParticleMan )
	{
		g_pParticleMan->SetUp( &gEngfuncs );

		// Add custom particle classes here BEFORE calling anything else or you will die.
		g_pParticleMan->AddCustomParticleClassSize( sizeof( CBaseParticle ) );
	}
}

extern "C" void CL_DLLEXPORT F( void* pv )
{
	cldll_func_t* pcldll_func = (cldll_func_t*)pv;

	// Hack!
	g_pcldstAddrs = (cldll_func_dst_t*)pcldll_func->pHudVidInitFunc;
	cldll_func_t cldll_func =
	{
		Initialize,
		HUD_Init,
		HUD_VidInit,
		HUD_Redraw,
		HUD_UpdateClientData,
		HUD_Reset,
		HUD_PlayerMove,
		HUD_PlayerMoveInit,
		HUD_PlayerMoveTexture,
		IN_ActivateMouse,
		IN_DeactivateMouse,
		IN_MouseEvent,
		IN_ClearStates,
		IN_Accumulate,
		CL_CreateMove,
		CL_IsThirdPerson,
		CL_CameraOffset,
		KB_Find,
		CAM_Think,
		V_CalcRefdef,
		HUD_AddEntity,
		HUD_CreateEntities,
		HUD_DrawNormalTriangles,
		HUD_DrawTransparentTriangles,
		HUD_StudioEvent,
		HUD_PostRunCmd,
		HUD_Shutdown,
		HUD_TxferLocalOverrides,
		HUD_ProcessPlayerState,
		HUD_TxferPredictionData,
		Demo_ReadBuffer,
		HUD_ConnectionlessPacket,
		HUD_GetHullBounds,
		HUD_Frame,
		HUD_Key_Event,
		HUD_TempEntUpdate,
		HUD_GetUserEntity,
		HUD_VoiceStatus,
		HUD_DirectorMessage,
		HUD_GetStudioModelInterface,
		HUD_ChatInputPosition,
		HUD_GetPlayerTeam,
		(CLIENTFACTORY)ClientFactory
	};
	*pcldll_func = cldll_func;
}

//-----------------------------------------------------------------------------
// Purpose: Exports functions that are used by the gameUI for UI dialogs
//-----------------------------------------------------------------------------
class CClientExports : public IGameClientExports
{
public:
	const char* GetServerHostName() override
	{
		return gViewPortInterface ? gViewPortInterface->GetServerName() : "";
	}

	bool IsPlayerGameVoiceMuted( int playerIndex ) override
	{
		return GetClientVoice() ? GetClientVoice()->IsPlayerBlocked( playerIndex ) : false;
	}

	void MutePlayerGameVoice( int playerIndex ) override
	{
		if ( GetClientVoice() )
			GetClientVoice()->SetPlayerBlockedState( playerIndex, true );
	}

	void UnmutePlayerGameVoice( int playerIndex ) override
	{
		if ( GetClientVoice() )
			GetClientVoice()->SetPlayerBlockedState( playerIndex, false );
	}
};

EXPOSE_SINGLE_INTERFACE( CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION );
