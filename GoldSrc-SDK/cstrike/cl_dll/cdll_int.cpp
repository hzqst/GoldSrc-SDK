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

#include <string.h>
#include "../../public/interface.h"
#include "hud.h"
#include "cl_util.h"
#include "../../cl_dll/Exports.h"
#include "particleman.h"
#include "tri.h"
#include "cl_dll/IGameClientExports.h"
#include "../public/game_controls/iviewport.h"
#include "../game_shared/voice_status.h"

extern "C"
{
#include "pm_shared.h"
}

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\cl_dll\\cdll_int.cpp" )

class IFileSystem;
class ICareerUI;
class IGameUI;
class IGameConsole;
class IMusicManager;
class IBaseUI;
class IFriendsUser;

cl_enginefunc_t gEngfuncs;
CHud gHUD;
modfuncs_t g_modfuncs;
cldll_func_dst_t* g_pcldstAddrs;
CSysModule* g_hFileSystemModule;
IFileSystem* g_pFileSystem;
CSysModule* g_hGameUIModule;
ICareerUI* g_pCareerUI;
IGameUI* g_pGameUI;
IGameConsole* g_pGameConsole;
IMusicManager* g_pMusicManager;
IBaseUI* g_pBaseUI;
CSysModule* g_hTrackerModule;
IFriendsUser* g_pFriendsUser;
CSysModule* g_hParticleManModule;
IParticleMan* g_pParticleMan;

char* sPlayerModelFiles[] =
{
	"models/player.mdl",
	"models/player/leet/leet.mdl",
	"models/player/gign/gign.mdl",
	"models/player/vip/vip.mdl",
	"models/player/gsg9/gsg9.mdl",
	"models/player/guerilla/guerilla.mdl",
	"models/player/arctic/arctic.mdl",
	"models/player/sas/sas.mdl",
	"models/player/terror/terror.mdl",
	"models/player/urban/urban.mdl",
	"models/player/spetsnaz/spetsnaz.mdl",
	"models/player/militia/militia.mdl"
};

void InitInput();
void ShutdownInput();
void IN_Commands();
void EV_HookEvents();

void CL_UnloadParticleMan()
{
	Sys_UnloadModule( g_hParticleManModule );

	g_pParticleMan = NULL;
	g_hParticleManModule = NULL;
}

void CL_LoadParticleMan()
{
	char szPDir[MAX_PATH];
	szPDir[0] = 0;

	if ( gEngfuncs.COM_ExpandFilename( PARTICLEMAN_DLLNAME, szPDir, sizeof( szPDir ) ) == FALSE )
	{
		g_pParticleMan = NULL;
		g_hParticleManModule = NULL;
		return;
	}

	g_hParticleManModule = Sys_LoadModule( szPDir );
	CreateInterfaceFn particleManFactory = Sys_GetFactory( g_hParticleManModule );

	if ( particleManFactory == NULL )
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
		g_pParticleMan->AddCustomParticleClassSize( sizeof( CPartSmokeGrenade ) );
	}
}

extern "C" int EXPORT Initialize( cl_enginefunc_t* pEnginefuncs, int iVersion )
{
	memcpy( &gEngfuncs, pEnginefuncs, sizeof( cl_enginefunc_t ) );

//	RecClInitialize(pEnginefuncs, iVersion);

	if ( iVersion != CLDLL_INTERFACE_VERSION )
		return 0;

	EV_HookEvents();
	CL_LoadParticleMan();

#ifdef _WIN32
	const char* szFsModule = "filesystem_stdio.dll";
	const char* szGameUIModule = "cl_dlls\\GameUI.dll";
#else
	const char* szFsModule = "filesystem_stdio.so";
	const char* szGameUIModule = "cl_dlls/gameui.so";
#endif
	char szFSDir[MAX_PATH];
	szFSDir[0] = 0;
	if ( gEngfuncs.COM_ExpandFilename( szFsModule, szFSDir, sizeof( szFSDir ) ) == FALSE )
		return false;

	g_hFileSystemModule = Sys_LoadModule( szFSDir );
	ASSERT_AT( g_hFileSystemModule, 244 );
	if ( !g_hFileSystemModule )
		return false;

	CreateInterfaceFn fileSystemFactory = Sys_GetFactory( g_hFileSystemModule );
	if ( !fileSystemFactory )
		return false;

	g_pFileSystem = (IFileSystem*)fileSystemFactory( "VFileSystem009", NULL );
	ASSERT_AT( g_pFileSystem, 257 );
	if ( !g_pFileSystem )
		return false;

	char szDir[512];
	if ( gEngfuncs.COM_ExpandFilename( szGameUIModule, szDir, sizeof( szDir ) ) )
	{
		g_hGameUIModule = Sys_LoadModule( szDir );
		if ( !g_hGameUIModule )
		{
			ASSERT_AT( g_hGameUIModule, 283 );
			return false;
		}

		CreateInterfaceFn gameUIFactory = Sys_GetFactory( g_hGameUIModule );
		if ( !gameUIFactory )
			return false;

		g_pGameConsole = (IGameConsole*)gameUIFactory( "GameConsole003", NULL );
		g_pGameUI = (IGameUI*)gameUIFactory( "GameUI007", NULL );
		g_pCareerUI = (ICareerUI*)gameUIFactory( "CareerUI001", NULL );
		g_pMusicManager = (IMusicManager*)gameUIFactory( "MusicManager001", NULL );
		ASSERT_AT( g_pGameConsole, 297 );
		ASSERT_AT( g_pGameUI, 298 );
		ASSERT_AT( g_pCareerUI, 299 );
		ASSERT_AT( g_pMusicManager, 300 );

		// get tracker interface, if any
		if ( gEngfuncs.COM_ExpandFilename( "Bin/TrackerUI.dll", szDir, sizeof( szDir ) ) )
		{
			g_hTrackerModule = Sys_LoadModule( szDir );
			CreateInterfaceFn trackerFactory = Sys_GetFactory( g_hTrackerModule );
			if ( trackerFactory )
			{
				g_pFriendsUser = (IFriendsUser*)trackerFactory( "FriendsUser001", NULL );
				return true;
			}
		}
	}

	g_pFriendsUser = NULL;
	g_hTrackerModule = NULL;

	return true;
}

extern "C" int EXPORT HUD_GetHullBounds( int hullnumber, float* mins, float* maxs )
{
//	RecClGetHullBounds(hullnumber, mins, maxs);

	int iret = 0;

	switch ( hullnumber )
	{
	case 0: // Normal player
		mins = Vector( -16, -16, -36 );
		maxs = Vector( 16, 16, 36 );
		iret = 1;
		break;
	case 1: // Crouched player
		mins = Vector( -16, -16, -18 );
		maxs = Vector( 16, 16, 18 );
		iret = 1;
		break;
	case 2: // Point based hull
		mins = Vector( 0, 0, 0 );
		maxs = Vector( 0, 0, 0 );
		iret = 1;
		break;
	}

	return iret;
}

extern "C" int EXPORT HUD_ConnectionlessPacket( const netadr_s*, const char*, char*, int* responseSize )
{
	*responseSize = 0;
	return 0;
}

extern "C" int EXPORT HUD_GetPlayerTeam( int player )
{
	return (unsigned int)player <= MAX_PLAYERS ? g_PlayerExtraInfo[player].team_id : 0;
}

extern "C" void EXPORT HUD_PlayerMoveInit( playermove_s* move )
{
//	RecClClientMoveInit(ppmove);

	PM_Init( move );
}

extern "C" char EXPORT HUD_PlayerMoveTexture( char* name )
{
//	RecClClientTextureType(name);

	return PM_FindTextureType( name );
}

extern "C" void EXPORT HUD_PlayerMove( playermove_s* move, int server )
{
//	RecClClientMove(ppmove, server);

	PM_Move( move, server );
}

extern "C" int EXPORT HUD_VidInit()
{
	gHUD.VidInit();
	return 1;
}

extern "C" void EXPORT HUD_Reset()
{
//	RecClHudReset();

	gHUD.VidInit();
}

extern "C" void EXPORT HUD_Init()
{
//	RecClHudInit();
	InitInput();
	gHUD.Init();
}

extern "C" void EXPORT HUD_Shutdown()
{
	ShutdownInput();
	CL_UnloadParticleMan();
	if ( g_hTrackerModule )
		Sys_UnloadModule( g_hTrackerModule );
	if ( g_hFileSystemModule )
		Sys_UnloadModule( g_hFileSystemModule );
	if ( g_hGameUIModule )
		Sys_UnloadModule( g_hGameUIModule );
	g_pGameConsole = NULL;
	g_pGameUI = NULL;
	g_pCareerUI = NULL;
	g_pMusicManager = NULL;
	g_pBaseUI = NULL;
}

extern "C" int EXPORT HUD_Redraw( float time, int intermission )
{
//	RecClHudRedraw(time, intermission);

	gHUD.Redraw( time, intermission );
	return 1;
}

extern "C" int EXPORT HUD_UpdateClientData( client_data_t* data, float time )
{
//	RecClHudUpdateClientData(pcldata, flTime);

	IN_Commands();
	return gHUD.UpdateClientData( data, time );
}

extern "C" void EXPORT HUD_Frame( double time )
{
	GetClientVoice()->Frame( time );
}

extern "C" void EXPORT HUD_VoiceStatus( int entity, qboolean talking )
{
	if ( entity >= 0 && entity < gEngfuncs.GetMaxClients() )
	{
		if ( talking )
		{
			g_PlayerExtraInfo[entity].radarflash = gHUD.m_flTime;
			g_PlayerExtraInfo[entity].radarflashon = 1;
			g_PlayerExtraInfo[entity].radarflashes = 99999;
		}
		else
		{
			g_PlayerExtraInfo[entity].radarflash = -1;
			g_PlayerExtraInfo[entity].radarflashon = 0;
			g_PlayerExtraInfo[entity].radarflashes = 0;
		}
	}
	GetClientVoice()->UpdateSpeakerStatus( entity, talking != 0 );
}

extern "C" void EXPORT HUD_DirectorMessage( int size, void* buffer )
{
//	RecClDirectorMessage(iSize, pbuf);

	gHUD.m_Spectator.DirectorMessage( size, buffer );
}

extern "C" CreateInterfaceFn EXPORT ClientFactory()
{
	return Sys_GetFactoryThis();
}

extern "C" void EXPORT F( void* buffer )
{
	cldll_func_t* destination = (cldll_func_t*)buffer;
	memcpy( &g_modfuncs, (void*)destination->pInitFunc, sizeof( g_modfuncs ) );
	g_pcldstAddrs = (cldll_func_dst_t*)destination->pHudVidInitFunc;
	cldll_func_t callbacks =
	{
		Initialize, HUD_Init, HUD_VidInit, HUD_Redraw, HUD_UpdateClientData, HUD_Reset,
		HUD_PlayerMove, HUD_PlayerMoveInit, HUD_PlayerMoveTexture, IN_ActivateMouse,
		IN_DeactivateMouse, IN_MouseEvent, IN_ClearStates, IN_Accumulate, CL_CreateMove,
		CL_IsThirdPerson, CL_CameraOffset, KB_Find, CAM_Think, V_CalcRefdef, HUD_AddEntity,
		HUD_CreateEntities, HUD_DrawNormalTriangles, HUD_DrawTransparentTriangles,
		HUD_StudioEvent, HUD_PostRunCmd, HUD_Shutdown, HUD_TxferLocalOverrides,
		HUD_ProcessPlayerState, HUD_TxferPredictionData, Demo_ReadBuffer,
		HUD_ConnectionlessPacket, HUD_GetHullBounds, HUD_Frame, HUD_Key_Event,
		HUD_TempEntUpdate, HUD_GetUserEntity, HUD_VoiceStatus, HUD_DirectorMessage,
		HUD_GetStudioModelInterface, HUD_ChatInputPosition, HUD_GetPlayerTeam, (CLIENTFACTORY)ClientFactory
	};
	*destination = callbacks;
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
	bool IsPlayerGameVoiceMuted( int player ) override
	{
		return GetClientVoice() ? GetClientVoice()->IsPlayerBlocked( player ) : false;
	}
	void MutePlayerGameVoice( int player ) override
	{
		if ( GetClientVoice() )
			GetClientVoice()->SetPlayerBlockedState( player, true );
	}
	void UnmutePlayerGameVoice( int player ) override
	{
		if ( GetClientVoice() )
			GetClientVoice()->SetPlayerBlockedState( player, false );
	}
};

EXPOSE_SINGLE_INTERFACE( CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION );
