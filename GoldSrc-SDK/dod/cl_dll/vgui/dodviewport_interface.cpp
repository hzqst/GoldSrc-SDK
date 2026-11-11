#include "hud.h"
#include "cl_util.h"
#include "demo_api.h"
#include "voice_status.h"
#include "game_controls/iviewport.h"
#include "game_controls/CreateCommandMenu.h"

void COM_FileBase( const char* path, char* name );

bool g_bUnassigned = true;
int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1;
int g_iUser2;
int g_iUser3;
CCreateCommandComboMenu g_CreateCommandMenu;

void* COM_LoadFile( char* path, int usehunk, int* length )
{
	return gEngfuncs.COM_LoadFile( path, usehunk, length );
}

char* COM_ParseFile( char* data, char* token )
{
	return gEngfuncs.COM_ParseFile( data, token );
}

void COM_FreeFile( void* buffer )
{
	gEngfuncs.COM_FreeFile( buffer );
}

int InIntermission()
{
	return gHUD.m_iIntermission;
}

bool PipInsetOff()
{
	return gHUD.m_Spectator.m_pip->value == 0;
}

void InsetValues( int& x, int& y, int& wide, int& tall )
{
	x = gHUD.m_Spectator.m_OverviewData.insetWindowX;
	y = gHUD.m_Spectator.m_OverviewData.insetWindowY;
	wide = gHUD.m_Spectator.m_OverviewData.insetWindowWidth;
	tall = gHUD.m_Spectator.m_OverviewData.insetWindowHeight;
}

int SpectatorNumber()
{
	return gHUD.m_Spectator.m_iSpectatorNumber;
}

float HudTime()
{
	return gHUD.m_flTime;
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

int GetMaxPlayers()
{
	const int maxClients = gEngfuncs.GetMaxClients();
	const int capacity = sizeof( g_PlayerInfoList ) / sizeof( g_PlayerInfoList[0] ) - 1;
	return maxClients < capacity ? maxClients : capacity;
}

int GetMaxTeams()
{
	return 64;
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

int GetLocalPlayerIndex()
{
	return gEngfuncs.GetLocalPlayer()->index;
}

bool IsSpectateOnly()
{
	return gEngfuncs.IsSpectateOnly() != 0;
}

bool IsDemoPlayingBack()
{
	return gEngfuncs.pDemoAPI->IsPlayingback() != 0;
}

float Cvar_GetFloat( const char* name )
{
	return gEngfuncs.pfnGetCvarFloat( (char*)name );
}

VGuiLibraryPlayer_t GetPlayerInfoFunc( int index )
{
	VGuiLibraryPlayer_t player = {};
	if ( (unsigned int)index < sizeof( g_PlayerInfoList ) / sizeof( g_PlayerInfoList[0] ) )
	{
		gEngfuncs.pfnGetPlayerInfo( index, &g_PlayerInfoList[index] );
		player.name = g_PlayerInfoList[index].name;
		player.ping = g_PlayerInfoList[index].ping;
		player.packetloss = g_PlayerInfoList[index].packetloss;
		player.thisplayer = g_PlayerInfoList[index].thisplayer != 0;
		player.teamname = g_PlayerExtraInfo[index].teamname;
		player.teamnumber = g_PlayerExtraInfo[index].teamnumber;
		player.frags = g_PlayerExtraInfo[index].frags;
		player.deaths = g_PlayerExtraInfo[index].deaths;
		player.playerclass = g_PlayerExtraInfo[index].playerclass;
		player.health = g_PlayerExtraInfo[index].health;
		player.dead = g_PlayerExtraInfo[index].dead;
	}
	return player;
}

void StopSquelch()
{
	GetClientVoice()->StopSquelchMode();
}

void MessageHud( const char* name, int size, void* buffer )
{
	gHUD.m_TextMessage.MsgFunc_TextMsg( name, size, buffer );
}

void MessageAdd( void* message )
{
	gHUD.m_Message.MessageAdd( (client_textmessage_t*)message );
}

void CheckSettings()
{
	gHUD.m_Spectator.CheckSettings();
}

void FindPlayer( const char* name )
{
	gHUD.m_Spectator.FindPlayer( name );
}

void FindNextPlayer( bool reverse )
{
	gHUD.m_Spectator.FindNextPlayer( reverse );
}

void COM_FileBase( char* path, char* name )
{
	COM_FileBase( (const char*)path, name );
}

CCreateCommandComboMenu* CreateCommandCombo()
{
	return &g_CreateCommandMenu;
}

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
