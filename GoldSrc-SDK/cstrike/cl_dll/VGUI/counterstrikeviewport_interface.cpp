//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Client DLL interface routines for client dll library
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

// client dll/engine defines
#include "hud.h"
#include "cl_dll/IVGuiClientDll.h"
#include "voice_status.h"
#include "demo_api.h"
#include "cvardef.h"
#include <string.h>

void COM_FileBase( const char* input, char* output );

int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1;
int g_iUser2;
int g_iUser3;
hostage_info_t g_HostageInfo[25];
bool g_bUnassigned = true;

int InIntermission()
{
	return gHUD.m_iIntermission;
}

void FindNextPlayer( bool reverse )
{
	gHUD.m_Spectator.FindNextPlayer( reverse );
}

void FindPlayer( const char* name )
{
	gHUD.m_Spectator.FindPlayer( name );
}

int PipInsetOff()
{
	return gHUD.m_Spectator.m_pip->value == INSET_OFF;
}

void InsetValues( int& x, int& y, int& wide, int& tall )
{
	x = gHUD.m_Spectator.m_OverviewData.insetWindowX;
	y = gHUD.m_Spectator.m_OverviewData.insetWindowY;
	wide = gHUD.m_Spectator.m_OverviewData.insetWindowWidth;
	tall = gHUD.m_Spectator.m_OverviewData.insetWindowHeight;
}

void CheckSettings()
{
	gHUD.m_Spectator.CheckSettings();
}

int SpectatorNumber()
{
	return gHUD.m_Spectator.m_iSpectatorNumber;
}

bool IsSpectator()
{
	return gEngfuncs.IsSpectateOnly() != 0;
}

int SpectatorModeVGUI()
{
	return g_iUser1;
}

int SpectatorTarget()
{
	return g_iUser2;
}

bool IsSpectateOnly()
{
	return gEngfuncs.IsSpectateOnly() != 0;
}

float HudTime()
{
	return gHUD.m_flTime;
}

void MessageAdd( void* message )
{
	gHUD.m_Message.MessageAdd( (client_textmessage_t*)message );
}

void MessageHud( const char* name, int size, void* buffer )
{
	gHUD.m_TextMessage.MsgFunc_TextMsg( name, size, buffer );
}

int TeamPlay()
{
	return gHUD.m_Teamplay;
}

void ClientCmdFunc( const char* command )
{
	gEngfuncs.pfnClientCmd( (char*)command );
}

int TeamNumber()
{
	return g_iTeamNumber;
}

const char* GetLevelName()
{
	return gEngfuncs.pfnGetLevelName();
}

int GetLocalPlayerIndex()
{
	return gEngfuncs.GetLocalPlayer()->index;
}
//#include "cl_util.h" // COM_* functions
//#include "demo.h"
//#include "demo_api.h" // demo_api_s

//-----------------------------------------------------------------------------
// Purpose: various wrapper functions for the interface 
//-----------------------------------------------------------------------------
char* COM_ParseFile( char* data, char* token )
{
	return gEngfuncs.COM_ParseFile( data, token );
}

void* COM_LoadFile( char* path, int usehunk, int* length )
{
	return gEngfuncs.COM_LoadFile( path, usehunk, length );
}

void COM_FreeFile( void* buffer )
{
	gEngfuncs.COM_FreeFile( buffer );
}

void COM_FileBase( char* input, char* output )
{
	COM_FileBase( (const char*)input, output );
}

VGuiLibraryPlayer_t GetPlayerInfoFunc( int index )
{
	VGuiLibraryPlayer_t info;
	memset( &info, 0, sizeof( info ) );

	if ( (unsigned int)index <= MAX_PLAYERS )
	{
		gEngfuncs.pfnGetPlayerInfo( index, &g_PlayerInfoList[index] );
		info.name = g_PlayerInfoList[index].name;
		info.ping = g_PlayerInfoList[index].ping;
		info.packetloss = g_PlayerInfoList[index].packetloss;
		info.thisplayer = g_PlayerInfoList[index].thisplayer != 0;
		info.teamname = g_PlayerExtraInfo[index].teamname;
		info.teamnumber = g_PlayerExtraInfo[index].teamnumber;
		info.frags = g_PlayerExtraInfo[index].frags;
		info.deaths = g_PlayerExtraInfo[index].deaths;
		info.playerclass = g_PlayerExtraInfo[index].playerclass;
		info.health = g_PlayerExtraInfo[index].health;
		info.dead = g_PlayerExtraInfo[index].dead;
		info.m_nSteamID = g_PlayerInfoList[index].m_nSteamID;
	}

	return info;
}

void StopSquelch()
{
	GetClientVoice()->StopSquelchMode();
}

bool IsDemoPlayingBack()
{
	return gEngfuncs.pDemoAPI->IsPlayingback() != 0;
}

float Cvar_GetFloat( const char* name )
{
	return gEngfuncs.pfnGetCvarFloat( (char*)name );
}

int GetMaxTeams()
{
	return MAX_TEAMS;
}

int GetMaxPlayers()
{
	const int maxClients = gEngfuncs.GetMaxClients();
	return maxClients < MAX_PLAYERS ? maxClients : MAX_PLAYERS;
}



//-----------------------------------------------------------------------------
// Purpose: the interface class that passes functions to the client library
//-----------------------------------------------------------------------------
VGuiLibraryInterface_t clientInterface =
{
	InIntermission,
	FindNextPlayer,
	FindPlayer,
	PipInsetOff,
	InsetValues,
	CheckSettings,
	SpectatorNumber,
	IsSpectator,
	SpectatorModeVGUI,
	SpectatorTarget,
	IsSpectateOnly,
	HudTime,
	MessageAdd,
	MessageHud,
	TeamPlay,
	ClientCmdFunc,
	TeamNumber,
	GetLevelName,
	GetLocalPlayerIndex,
	COM_ParseFile,
	COM_LoadFile,
	COM_FreeFile,
	COM_FileBase,
	gEngfuncs.Con_DPrintf,
	GetPlayerInfoFunc,
	GetMaxPlayers,
	StopSquelch,
	IsDemoPlayingBack,
	Cvar_GetFloat
};
