#include "vgui_interface.h"
#include "game_controls/viewport_player_info.h"
#include "hud.h"
#include "cl_util.h"
#include "demo_api.h"
#include "parsemsg.h"
#include "voice_status.h"
#include "vgui_TeamFortressViewport.h"
#include "tfcscoreboard.h"
#include "tfccreatecommandmenu.h"

void COM_FileBase( const char* path, char* name );

extern hud_player_info_t g_PlayerInfoList[MAX_PLAYERS + 1];
extern extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS + 1];
extern TFCCCreateCommandComboMenu g_CreateCommandMenu;

int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1 = 0;
int g_iUser2 = 0;
int g_iUser3 = 0;

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

int PipInsetOff()
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
	return gEngfuncs.GetMaxClients();
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
	gEngfuncs.pfnGetPlayerInfo( index, &g_PlayerInfoList[index] );
	VGuiLibraryPlayer_t player;
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
	NULL,
	GetPlayerInfoFunc,
	GetMaxPlayers,
	StopSquelch,
	IsDemoPlayingBack,
	Cvar_GetFloat,
	[]( int player, int frags, int deaths, int playerClass, int team )
	{
		UpdateViewportPlayerScore( g_PlayerExtraInfo, player, frags, deaths, playerClass, team );
	},
	[]( int player, const char* team )
	{
		UpdateViewportPlayerTeam( g_PlayerExtraInfo, player, team );
	}
};

class TFViewPort : public TeamFortressViewport
{
public:
	void Start() override;
	int MsgFunc_TeamNames( const char* name, int size, void* buffer ) override;
	int MsgFunc_ResetFade( const char* name, int size, void* buffer ) override;
	int MsgFunc_SpecFade( const char* name, int size, void* buffer ) override;
	void InputPlayerSpecial() override;
	void DisplayVGUIMenu( int menu ) override;

private:
	CClientScoreBoardDialog* CreateScoreBoard() override;
	TFCCClientScoreBoardDialog* m_pTFCClientScoreBoard;

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

void TFViewPort::Start()
{
	SetClientDllInterface( &clientInterface );
	TeamFortressViewport::Start();
}

CClientScoreBoardDialog* TFViewPort::CreateScoreBoard()
{
	m_pTFCClientScoreBoard = new TFCCClientScoreBoardDialog( NULL );
	return m_pTFCClientScoreBoard;
}

int TFViewPort::MsgFunc_TeamNames( const char* name, int size, void* buffer )
{
	int result = TeamFortressViewport::MsgFunc_TeamNames( name, size, buffer );
	m_pClientScoreBoard->Reset();
	return result;
}

int TFViewPort::MsgFunc_ResetFade( const char*, int, void* )
{
	return 1;
}

int TFViewPort::MsgFunc_SpecFade( const char*, int, void* )
{
	return 1;
}

void TFViewPort::InputPlayerSpecial()
{
	TeamFortressViewport::InputPlayerSpecial();
}

void TFViewPort::DisplayVGUIMenu( int menu )
{
	TeamFortressViewport::DisplayVGUIMenu( menu );
	if ( menu == 5 )
		gViewPortInterface->HideBackGround();
}

EXPOSE_SINGLE_INTERFACE( TFViewPort, IClientVGUI, CLIENTVGUI_INTERFACE_VERSION );

CCreateCommandComboMenu* CreateCommandCombo()
{
	return &g_CreateCommandMenu;
}

int GetMaxTeams()
{
	return 4;
}

extern "C" void HUD_ChatInputPosition( int* x, int* y )
{
	if ( gViewPortInterface )
		gViewPortInterface->ChatInputPosition( x, y );
}
