/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
// hud.cpp
//
// implementation of CHud class
//

#include <stdlib.h>
#include <string.h>
#include "hud.h"
#include "cl_util.h"
#include "demo.h"
#include "demo_api.h"
#include "parsemsg.h"
#include "buy_presets.h"
#include "../public/game_controls/iviewport.h"
#include "../public/game_controls/iviewportmsgs.h"
#include "VGUI/icstrikeviewport.h"
#include "VGUI/icstrikeviewportmsgs.h"
#include "../game_shared/voice_status.h"
#include "particleman.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "../../engine/controls.h"
#include "../../vgui2/controls/controls.h"

hud_player_info_t g_PlayerInfoList[MAX_PLAYERS + 1];	   // player info from the engine
extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS + 1];   // additional player info sent directly to the client dll
cvar_t* cl_lw;
cvar_t* cl_tutor;
cvar_t* cl_career_difficulty;
cvar_t* cl_fog_density;
cvar_t* cl_fog_r;
cvar_t* cl_fog_g;
cvar_t* cl_fog_b;
float g_lastFOV;

extern cvar_t* sensitivity;
extern cvar_t* cl_righthand;
extern cvar_t* cl_minmodels;
extern cvar_t* cl_dynamiccrosshair;
extern cvar_t* cl_min_ct;
extern cvar_t* cl_min_t;
extern cvar_t* cl_weather;
extern cvar_t* cl_shadows;
extern cvar_t* cl_radartype;
extern vgui2::HFont _font;

const char* GetStringTeamColor( int team );
void StudioSetShadowSprite( int sprite );
void ShowCareerMatchEndMenu( int ctWins, int tWins, int neededWins, int winBy, int roundResult );
void ShowCareerRoundEndMenu( int ctWins, int tWins, int neededWins, int winBy, int roundResult );

class CCStrikeVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	void GetPlayerTextColor( int player, int color[3] ) override
	{
		color[0] = color[1] = color[2] = 255;
		if ( (unsigned int)player <= MAX_PLAYERS )
		{
			vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( gViewPortInterface->GetViewPortScheme() );
			Color teamColor = scheme->GetColor( GetStringTeamColor( g_PlayerExtraInfo[player].teamnumber ), Color( 255, 255, 255, 0 ) );
			color[0] = teamColor.r();
			color[1] = teamColor.g();
			color[2] = teamColor.b();
		}
	}
	int GetAckIconHeight() override
	{
		return 3 * gHUD.m_iFontHeight + 6;
	}
	bool CanShowSpeakerLabels() override
	{
		return gViewPortInterface ? !gViewPortInterface->IsScoreBoardVisible() : false;
	}
	const char* GetPlayerLocation( int player ) override
	{
		return (unsigned int)player <= MAX_PLAYERS ? g_PlayerExtraInfo[player].location : "";
	}
};

static CCStrikeVoiceStatusHelper g_VoiceStatusHelper;

int __MsgFunc_Logo( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_Logo( name, size, buffer );
}

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_ResetHUD( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_ResetHUD( name, size, buffer );
}

int __MsgFunc_GameMode( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_GameMode( name, size, buffer );
}

int __MsgFunc_InitHUD( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_InitHUD( name, size, buffer );
	return 1;
}

int __MsgFunc_ViewMode( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_ViewMode( name, size, buffer );
	return 1;
}

int __MsgFunc_SetFOV( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_SetFOV( name, size, buffer );
}

int __MsgFunc_HLTV( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_HLTV( name, size, buffer );
}

int CHud::MsgFunc_SpecHealth( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int health = READ_BYTE();
	gHUD.m_Health.m_iPlayerLastPointedAt = g_iUser2;
	g_PlayerExtraInfo[g_iUser2].health = health;
	gViewPortInterface->UpdateSpectatorPanel();
	return 1;
}

int CHud::MsgFunc_SpecHealth2( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int health = READ_BYTE();
	int player = READ_BYTE();
	gHUD.m_Health.m_iPlayerLastPointedAt = g_iUser2;
	g_PlayerExtraInfo[player].health = health;
	gViewPortInterface->UpdateSpectatorPanel();
	return 1;
}

int __MsgFunc_SpecHealth( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_SpecHealth( name, size, buffer );
}

int __MsgFunc_SpecHealth2( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_SpecHealth2( name, size, buffer );
}

int __MsgFunc_Concuss( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_Concuss( name, size, buffer );
}

int __MsgFunc_ReloadSound( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_ReloadSound( name, size, buffer );
}

int __MsgFunc_BombDrop( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_BombDrop( name, size, buffer );
	return 1;
}

int __MsgFunc_BombPickup( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_BombPickup( name, size, buffer );
	return 1;
}

int __MsgFunc_ADStop( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_ADStop( name, size, buffer );
	return 1;
}

int __MsgFunc_HostagePos( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_HostagePos( name, size, buffer );
	return 1;
}

int __MsgFunc_HostageK( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_HostageK( name, size, buffer );
	return 1;
}

int __MsgFunc_BotVoice( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_BotVoice( name, size, buffer );
}

int __MsgFunc_Brass( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_Brass( name, size, buffer );
}

int __MsgFunc_Fog( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_Fog( name, size, buffer );
	return 1;
}

int __MsgFunc_ShowTimer( const char* name, int size, void* buffer )
{
	gHUD.MsgFunc_ShowTimer( name, size, buffer );
	return 1;
}

int __MsgFunc_TeamNames( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TeamNames( name, size, buffer ) : 0;
}

int __MsgFunc_Feign( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_Feign( name, size, buffer ) : 0;
}

int __MsgFunc_Detpack( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_Detpack( name, size, buffer ) : 0;
}

int __MsgFunc_VGUIMenu( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_VGUIMenu( name, size, buffer ) : 0;
}

int __MsgFunc_TutorText( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TutorText( name, size, buffer ) : 0;
}

int __MsgFunc_TutorLine( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TutorLine( name, size, buffer ) : 0;
}

int __MsgFunc_TutorState( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TutorState( name, size, buffer ) : 0;
}

int __MsgFunc_TutorClose( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TutorClose( name, size, buffer ) : 0;
}

int __MsgFunc_MOTD( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_MOTD( name, size, buffer ) : 0;
}

int __MsgFunc_BuildSt( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_BuildSt( name, size, buffer ) : 0;
}

int __MsgFunc_RandomPC( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_RandomPC( name, size, buffer ) : 0;
}

int __MsgFunc_ServerName( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_ServerName( name, size, buffer ) : 0;
}

int __MsgFunc_ScoreInfo( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gCSViewPortMsgs->MsgFunc_ScoreInfo( name, size, buffer ) : 0;
}

int __MsgFunc_TeamScore( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TeamScore( name, size, buffer ) : 0;
}

int __MsgFunc_TeamInfo( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TeamInfo( name, size, buffer ) : 0;
}

int __MsgFunc_Location( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gCSViewPortMsgs->MsgFunc_Location( name, size, buffer ) : 0;
}

int __MsgFunc_Spectator( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_Spectator( name, size, buffer ) : 0;
}

int __MsgFunc_AllowSpec( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_AllowSpec( name, size, buffer ) : 0;
}

int __MsgFunc_ForceCam( const char* name, int size, void* buffer )
{
	return gCSViewPortMsgs ? gCSViewPortMsgs->MsgFunc_ForceCam( name, size, buffer ) : 0;
}

int __MsgFunc_BuyClose( const char*, int, void* )
{
	if ( gViewPortInterface )
		gViewPortInterface->HideVGUIMenu( 28 );
	return 0;
}

int __MsgFunc_ShadowIdx( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	StudioSetShadowSprite( READ_LONG() );
	return 0;
}

int __MsgFunc_BotProgress( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int action = (char)READ_BYTE();
	switch ( action )
	{
	case 0:
	{
		int progress = READ_BYTE();
		char* text = READ_STRING();
		if ( !gCSViewPortInterface->IsProgressBarVisible() )
			gCSViewPortInterface->StartProgressBar( text, 100, 0, false );
		gCSViewPortInterface->UpdateProgressBar( text, progress );
		break;
	}
	case 1:
		gCSViewPortInterface->StartProgressBar( READ_STRING(), 100, 0, false );
		break;
	case 2:
		gCSViewPortInterface->StopProgressBar();
		break;
	}
	return 0;
}

int __MsgFunc_CZCareerHUD( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	const char* action = READ_STRING();
	if ( !strcmp( action, "MATCH" ) )
	{
		int ctWins = READ_LONG();
		int tWins = READ_LONG();
		int neededWins = READ_BYTE();
		int winBy = READ_BYTE();
		int result = READ_BYTE();
		ShowCareerMatchEndMenu( ctWins, tWins, neededWins, winBy, result );
	}
	else if ( !strcmp( action, "ROUND" ) )
	{
		int ctWins = READ_LONG();
		int tWins = READ_LONG();
		int neededWins = READ_BYTE();
		int winBy = READ_BYTE();
		int result = READ_BYTE();
		ShowCareerRoundEndMenu( ctWins, tWins, neededWins, winBy, result );
	}
	return 0;
}

void __CmdFunc_ShowCommandMenu()
{
	if ( gViewPortInterface )
		gViewPortInterface->ShowCommandMenu();
}

void __CmdFunc_HideCommandMenu()
{
	if ( gViewPortInterface )
		gViewPortInterface->HideCommandMenu();
}

void __CmdFunc_InputPlayerSpecial()
{
	if ( gViewPortInterface )
		gViewPortInterface->InputPlayerSpecial();
}

void __CmdFunc_ToggleGunSmoke()
{
	gHUD.m_bRenderGunSmoke = !gHUD.m_bRenderGunSmoke;
}

void __CmdFunc_AutoBuy()
{
	gHUD.AutoBuy();
}

void __CmdFunc_Rebuy()
{
	gHUD.Rebuy();
}

void __CmdFunc_RunCredits()
{
	gHUD.RunCredits();
}

void __CmdFunc_ClientBuy()
{
	if ( gViewPortInterface && gHUD.m_StatusIcons.InBuyZone()
		&& !g_PlayerExtraInfo[gEngfuncs.GetLocalPlayer()->index].dead )
	{
		const char* menus = gEngfuncs.LocalPlayerInfo_ValueForKey( "_vgui_menus" );
		if ( menus && *menus && !atoi( menus ) )
			gEngfuncs.pfnServerCmd( "buy\n" );
		else
		{
			gViewPortInterface->ShowVGUIMenu( 28 );
			gEngfuncs.pfnServerCmd( "client_buy_open\n" );
		}
	}
}

void CHud::Init()
{
	HOOK_MESSAGE( Logo );
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( HLTV );
	HOOK_MESSAGE( SpecHealth );
	HOOK_MESSAGE( Concuss );
	HOOK_MESSAGE( ReloadSound );
	HOOK_MESSAGE( BombDrop );
	HOOK_MESSAGE( BombPickup );
	HOOK_MESSAGE( ADStop );
	HOOK_MESSAGE( HostagePos );
	HOOK_MESSAGE( HostageK );
	HOOK_MESSAGE( CZCareerHUD );
	HOOK_MESSAGE( ShadowIdx );
	HOOK_MESSAGE( BotVoice );
	HOOK_MESSAGE( Brass );
	HOOK_MESSAGE( Fog );
	HOOK_MESSAGE( ShowTimer );
	HOOK_COMMAND( "+commandmenu", ShowCommandMenu );
	HOOK_COMMAND( "-commandmenu", HideCommandMenu );
	HOOK_COMMAND( "gunsmoke", ToggleGunSmoke );
	HOOK_COMMAND( "autobuy", AutoBuy );
	HOOK_COMMAND( "rebuy", Rebuy );
	HOOK_COMMAND( "buy", ClientBuy );
	if ( IsGame( "czero" ) )
		HOOK_COMMAND( "credits", RunCredits );
	HOOK_MESSAGE( TeamNames );
	HOOK_MESSAGE( Feign );
	HOOK_MESSAGE( Detpack );
	HOOK_MESSAGE( MOTD );
	HOOK_MESSAGE( BuildSt );
	HOOK_MESSAGE( RandomPC );
	HOOK_MESSAGE( ServerName );
	HOOK_MESSAGE( ScoreInfo );
	HOOK_MESSAGE( TeamScore );
	HOOK_MESSAGE( TeamInfo );
	HOOK_MESSAGE( Location );
	HOOK_MESSAGE( Spectator );
	HOOK_MESSAGE( AllowSpec );
	HOOK_MESSAGE( ForceCam );

	// VGUI Menus
	HOOK_MESSAGE( VGUIMenu );
	HOOK_MESSAGE( TutorText );
	HOOK_MESSAGE( TutorLine );
	HOOK_MESSAGE( TutorState );
	HOOK_MESSAGE( TutorClose );
	HOOK_MESSAGE( BuyClose );
	HOOK_MESSAGE( SpecHealth2 );
	HOOK_MESSAGE( BotProgress );
	if ( !TheBuyPresets )
		TheBuyPresets = new BuyPresetManager;
	TheBuyPresets->ResetItems();
	gEngfuncs.pfnRegisterVariable( "hud_takesshots", "0", FCVAR_ARCHIVE );
	m_iLogo = 0;
	m_iFOV = 0;
	m_bShowTimer = true;
	m_autoBuyStringSent = true;
	m_rebuyStringSent = true;
	gEngfuncs.pfnRegisterVariable( "zoom_sensitivity_ratio", "1.2", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "max_shells", "120", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "max_smokepuffs", "120", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "fastsprites", "0", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "cl_corpsestay", "600", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "_cl_autowepswitch", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	gEngfuncs.pfnRegisterVariable( "scoreboard_showhealth", "1", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "scoreboard_showmoney", "1", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "scoreboard_shortheaders", "0", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "scoreboard_showavatars", "1", FCVAR_ARCHIVE );
	default_fov = gEngfuncs.pfnRegisterVariable( "default_fov", "90", FCVAR_ARCHIVE );
	cl_lw = gEngfuncs.pfnGetCvarPointer( "cl_lw" );
	m_pCvarStealMouse = gEngfuncs.pfnRegisterVariable( "hud_capturemouse", "1", FCVAR_ARCHIVE );
	m_pCvarDraw = gEngfuncs.pfnRegisterVariable( "hud_draw", "1", FCVAR_ARCHIVE );
	m_fGLTEXSORT = gEngfuncs.pfnGetCvarFloat( "gl_texsort" );
	if ( gEngfuncs.pfnGetCvarFloat( "gl_alphamin" ) == 0 )
		m_bSoftwaremode = true;
	m_flGLZMAX = gEngfuncs.pfnGetCvarFloat( "gl_zmax" );
	cl_righthand = gEngfuncs.pfnRegisterVariable( "cl_righthand", "1", FCVAR_ARCHIVE );
	cl_minmodels = gEngfuncs.pfnRegisterVariable( "cl_minmodels", "0", FCVAR_ARCHIVE );
	cl_dynamiccrosshair = gEngfuncs.pfnRegisterVariable( "cl_dynamiccrosshair", "1", FCVAR_ARCHIVE );
	cl_min_ct = gEngfuncs.pfnRegisterVariable( "cl_min_ct", "2", FCVAR_ARCHIVE );
	cl_min_t = gEngfuncs.pfnRegisterVariable( "cl_min_t", "1", FCVAR_ARCHIVE );
	cl_weather = gEngfuncs.pfnRegisterVariable( "cl_weather", "1", FCVAR_ARCHIVE );
	if ( gEngfuncs.pfnGetCvarFloat( "developer" ) > 0 )
	{
		cl_fog_density = gEngfuncs.pfnRegisterVariable( "cl_fog_density", "0", 0 );
		cl_fog_r = gEngfuncs.pfnRegisterVariable( "cl_fog_r", "0", 0 );
		cl_fog_g = gEngfuncs.pfnRegisterVariable( "cl_fog_g", "0", 0 );
		cl_fog_b = gEngfuncs.pfnRegisterVariable( "cl_fog_b", "0", 0 );
	}
	cl_shadows = gEngfuncs.pfnRegisterVariable( "cl_shadows", "1", FCVAR_ARCHIVE );
	cl_radartype = gEngfuncs.pfnRegisterVariable( "cl_radartype", "0", FCVAR_ARCHIVE );
	if ( IsGame( "czero" ) )
	{
		cl_tutor = gEngfuncs.pfnRegisterVariable( "tutor_enable", "1", FCVAR_ARCHIVE );
		cl_career_difficulty = gEngfuncs.pfnRegisterVariable( "cl_career_difficulty", "0", FCVAR_ARCHIVE );
	}
	m_pSpriteList = NULL;

	// Clear any old HUD list
	while ( m_pHudList )
	{
		HUDLIST* item = m_pHudList;
		m_pHudList = item->pNext;
		free( item );
	}
	m_flTime = 1;
	m_NightVision.Init();
	m_sniperScope.Init();
	m_Ammo.Init();
	m_Health.Init();
	m_SayText.Init();
	m_Spectator.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_Message.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	m_accountBalance.Init();
	m_roundTimer.Init();
	m_progressBar.Init();
	m_careerTaskTimer.Init();
	m_scenarioStatus.Init();
	m_Menu.Init();
	m_VGUI2Print.Init();
	GetClientVoice()->Init( &g_VoiceStatusHelper );
	MsgFunc_ResetHUD( NULL, 0, NULL );
	m_bRenderGunSmoke = false;
	m_Teamplay = 1;
	g_ShellQueue.m_flDuration = 2;
	g_SmokePuffQueue.m_flDuration = 1;
}

void CHud::VidInit()
{
	m_scrinfo.iSize = sizeof( m_scrinfo );
	gEngfuncs.pfnGetScreenInfo( &m_scrinfo );
	if ( g_pParticleMan )
		g_pParticleMan->ResetParticles();

	// ----------
	// Load Sprites
	// ---------
//	m_hsprFont = LoadSprite("sprites/%d_font.spr");

	m_hsprLogo = 0;
	m_hsprCursor = 0;
	m_iRes = ScreenWidth >= 640 ? 640 : 320;
	char spriteName[256];


	// Only load this once
	if ( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = gEngfuncs.pfnSPR_GetList( "sprites/hud.txt", &m_iSpriteCountAllRes );
		if ( m_pSpriteList )
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			for ( int i = 0; i < m_iSpriteCountAllRes; i++ )
			{
				if ( m_pSpriteList[i].iRes == m_iRes )
					m_iSpriteCount++;
			}

			// allocated memory for sprite handle arrays
			m_rghSprites = new HSPRITE[m_iSpriteCount];
			m_rgrcRects = new wrect_t[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];
			int index = 0;
			for ( int i = 0; i < m_iSpriteCountAllRes; i++ )
			{
				client_sprite_t& sprite = m_pSpriteList[i];
				if ( sprite.iRes != m_iRes )
					continue;
				sprintf( spriteName, "sprites/%s.spr", sprite.szSprite );
				m_rghSprites[index] = SPR_Load( spriteName );
				m_rgrcRects[index] = sprite.rc;
				strncpy( m_rgszSpriteNames + index * MAX_SPRITE_NAME_LENGTH, sprite.szName, MAX_SPRITE_NAME_LENGTH );
				index++;
			}
		}
	}
	else
	{
		int index = 0;
		for ( int i = 0; i < m_iSpriteCountAllRes; i++ )
		{
			if ( m_pSpriteList[i].iRes == m_iRes )
			{
				sprintf( spriteName, "sprites/%s.spr", m_pSpriteList[i].szSprite );
				m_rghSprites[index++] = SPR_Load( spriteName );
			}
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );
	m_NightVision.VidInit();
	m_sniperScope.VidInit();
	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Spectator.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_Message.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_StatusIcons.VidInit();
	m_accountBalance.VidInit();
	m_roundTimer.VidInit();
	m_progressBar.VidInit();
	m_careerTaskTimer.VidInit();
	m_scenarioStatus.VidInit();
	m_flCheatCheckTime = -1;
	m_flExpensiveCheckTime = -1;
	m_iFontHeight = GetSpriteRect( m_HUD_number_0 ).bottom - GetSpriteRect( m_HUD_number_0 ).top;
	m_iFontEngineHeight = vgui2::surface()->GetFontTall( _font );
	GetClientVoiceHud()->VidInit();
	m_VGUI2Print.VidInit();
}

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CHud::CHud() : m_pHudList( NULL ), m_iSpriteCount( 0 )
{
}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
CHud::~CHud()
{
	delete[] m_rghSprites;
	delete[] m_rgrcRects;
	delete[] m_rgszSpriteNames;
	while ( m_pHudList )
	{
		HUDLIST* element = m_pHudList;
		m_pHudList = element->pNext;
		free( element );
	}
	if ( m_pSpriteList )
	{
		gEngfuncs.COM_FreeFile( m_pSpriteList );
		m_pSpriteList = NULL;
	}
	g_ShellQueue.Update( 99999 );
}

void CHud::AddHudElem( CHudBase* element )
{
	if ( !element )
		return;
	HUDLIST* entry = (HUDLIST*)malloc( sizeof( HUDLIST ) );
	if ( !entry )
		return;
	memset( entry, 0, sizeof( HUDLIST ) );
	entry->p = element;
	if ( !m_pHudList )
	{
		m_pHudList = entry;
	}
	else
	{
		HUDLIST* last = m_pHudList;
		while ( last->pNext )
			last = last->pNext;
		last->pNext = entry;
	}
}

int CHud::GetSpriteIndex( const char* name )
{
	// look through the loaded sprite name list for SpriteName
	for ( int i = 0; i < m_iSpriteCount; i++ )
	{
		if ( !strncmp( name, m_rgszSpriteNames + i * MAX_SPRITE_NAME_LENGTH, MAX_SPRITE_NAME_LENGTH ) )
			return i;
	}
	return -1; // invalid sprite
}

void COM_FileBase( const char* input, char* output )
{
	int length = strlen( input );
	int end = length - 1;
	while ( end && input[end] != '.' && input[end] != '/' && input[end] != '\\' )
		end--;
	if ( input[end] != '.' )
		end = length - 1;
	else
		end--;
	int start = length - 1;
	while ( start >= 0 && input[start] != '/' && input[start] != '\\' )
		start--;
	if ( input[start] != '/' && input[start] != '\\' )
		start = 0;
	else
		start++;
	length = end - start + 1;
	strncpy( output, input + start, length );
	output[length] = 0;
}

int CHud::IsGame( const char* game )
{
	const char* directory = gEngfuncs.pfnGetGameDirectory();
	if ( !directory || !*directory )
		return 0;
	char name[1024];
	COM_FileBase( directory, name );
	return !stricmp( name, game );
}

float HUD_GetFOV()
{
	if ( gEngfuncs.pDemoAPI->IsRecording() )
	{
		unsigned char buffer[100];
		*(float*)buffer = g_lastFOV;
		Demo_WriteBuffer( TYPE_ZOOM, sizeof( float ), buffer );
	}
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		g_lastFOV = g_demozoom;
	return g_lastFOV;
}

int CHud::MsgFunc_SetFOV( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int fov = READ_BYTE();
	int defaultFOV = (int)default_fov->value;
	if ( !IsGame( "tfc" ) || !cl_lw || cl_lw->value == 0 )
	{
		g_lastFOV = (float)fov;
		m_iFOV = fov ? fov : defaultFOV;
		if ( !fov || fov == defaultFOV )
		{
			// reset to saved sensitivity
			m_flMouseSensitivity = 0;
		}
		else
		{
			float scale = (float)fov / defaultFOV * sensitivity->value;
			m_flMouseSensitivity = gEngfuncs.pfnGetCvarFloat( "zoom_sensitivity_ratio" ) * scale;
		}
	}
	return 1;
}

int CHud::MsgFunc_HLTV( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	int value = READ_BYTE();
	if ( !gEngfuncs.IsSpectateOnly() )
	{
		gEngfuncs.Con_DPrintf( "Warning! Received unexpected HLTV user message.\n" );
		return 1;
	}
	if ( value & 0x80 )
	{
		value &= 0x7f;
		if ( player )
			g_PlayerExtraInfo[player].health = value;
		else
		{
			for ( int i = 0; i < MAX_PLAYERS; i++ )
				g_PlayerExtraInfo[i].health = value;
		}
	}
	else
	{
		if ( !value )
		{
			value = (int)default_fov->value;
			if ( value < 90 )
				value = 90;
		}
		if ( player )
			m_PlayerFOV[player] = value;
		else
		{
			for ( int i = 0; i < MAX_PLAYERS; i++ )
				m_PlayerFOV[i] = value;
		}
	}
	return 1;
}

int CHud::MsgFunc_Logo( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );

	// update Train data
	m_iLogo = READ_BYTE();
	return 1;
}

float CHud::GetSensitivity()
{
	return m_flMouseSensitivity;
}

void CHud::VGUI2HudPrint( char* text, int x, int y, int r, int g, int b )
{
	m_VGUI2Print.VGUI2HudPrint( text, x, y, (float)r, (float)g, (float)b );
}

bool IsBombPlanted()
{
	return !gHUD.m_bShowTimer;
}
