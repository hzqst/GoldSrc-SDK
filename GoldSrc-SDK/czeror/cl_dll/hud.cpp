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

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "demo_api.h"
#include "demo.h"
#include "../game_shared/voice_status.h"
#include "../public/game_controls/legacy_iviewport.h"
#include "../public/game_controls/legacy_iviewportmsgs.h"
#include "vgui/iczeroviewport.h"
#include "../../vgui2/controls/controls.h"
#include "vgui/IScheme.h"

extern cvar_t* sensitivity;
extern cvar_t *cl_righthand, *cl_dynamiccrosshair, *cl_weather;
extern float g_demozoom;
const char* GetStringTeamColor( int team );

cvar_t *cl_effectsdetail, *cl_minframerate, *mat_autocreate, *mat_writerrorfile;
cvar_t *cl_levellocks, *cl_newgame, *cl_lw, *tfc_newmodels;
float g_lastFOV;

class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	void GetPlayerTextColor( int index, int color[3] ) override
	{
		color[0] = color[1] = color[2] = 255;
		if ( index >= 0 && index <= MAX_PLAYERS )
		{
			int team = g_PlayerExtraInfo[index].teamnumber;
			vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( gViewPortInterface->GetViewPortScheme() );
			if ( team < 0 )
				team = 0;
			Color teamColor = scheme->GetColor( GetStringTeamColor( team ), Color( 255, 255, 255, 0 ) );
			color[0] = teamColor.r();
			color[1] = teamColor.g();
			color[2] = teamColor.b();
		}
	}
	int GetAckIconHeight() override { return 3 * gHUD.m_iFontHeight + 6; }
	bool CanShowSpeakerLabels() override
	{
		return gViewPortInterface ? !gViewPortInterface->IsScoreBoardVisible() : false;
	}
};

static CHLVoiceStatusHelper g_VoiceStatusHelper;

int __MsgFunc_Logo( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_Logo( name, size, buffer );
}

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_ResetHUD( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_ResetHUD( name, size, buffer );
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

int __MsgFunc_Concuss( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_Concuss( name, size, buffer );
}

int __MsgFunc_GameMode( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_GameMode( name, size, buffer );
}

int __MsgFunc_SecurityCam( const char* name, int size, void* buffer )
{
	return gHUD.MsgFunc_SecurityCam( name, size, buffer );
}

int __MsgFunc_ValClass( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_ValClass( name, size, buffer ) : 0;
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
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_ScoreInfo( name, size, buffer ) : 0;
}

int __MsgFunc_TeamScore( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TeamScore( name, size, buffer ) : 0;
}

int __MsgFunc_TeamInfo( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_TeamInfo( name, size, buffer ) : 0;
}

int __MsgFunc_Spectator( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_Spectator( name, size, buffer ) : 0;
}

int __MsgFunc_AllowSpec( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_AllowSpec( name, size, buffer ) : 0;
}

int __MsgFunc_VGUIMenu( const char* name, int size, void* buffer )
{
	return gViewPortMsgs ? gViewPortMsgs->MsgFunc_VGUIMenu( name, size, buffer ) : 0;
}

void __CmdFunc_InputPlayerSpecial()
{
	if ( gViewPortInterface )
		gViewPortInterface->InputPlayerSpecial();
}

void __CmdFunc_OpenWorldMap()
{
	if ( gCzeroViewPortInterface )
		gCzeroViewPortInterface->ShowWorldMap();
}

void CHud::Init()
{
	HOOK_MESSAGE( Logo );
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( Concuss );
	HOOK_MESSAGE( SecurityCam );
	HOOK_COMMAND( "special", InputPlayerSpecial );
	HOOK_COMMAND( "worldmap", OpenWorldMap );
	HOOK_MESSAGE( ValClass );
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
	HOOK_MESSAGE( Spectator );
	HOOK_MESSAGE( AllowSpec );
	HOOK_MESSAGE( VGUIMenu );
	CVAR_CREATE( "hud_takesshots", "0", FCVAR_ARCHIVE );
	m_iFOV = 0;
	m_iLogo = 0;
	CVAR_CREATE( "zoom_sensitivity_ratio", "1.2", FCVAR_ARCHIVE );
	CVAR_CREATE( "max_shells", "120", FCVAR_ARCHIVE );
	CVAR_CREATE( "max_smokepuffs", "120", FCVAR_ARCHIVE );
	CVAR_CREATE( "fastsprites", "0", FCVAR_ARCHIVE );
	CVAR_CREATE( "cl_corpsestay", "600", FCVAR_ARCHIVE );
	CVAR_CREATE( "_cl_autowepswitch", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	default_fov = CVAR_CREATE( "default_fov", "90", FCVAR_ARCHIVE );
	m_pCvarStealMouse = CVAR_CREATE( "hud_capturemouse", "1", FCVAR_ARCHIVE );
	m_pCvarDraw = CVAR_CREATE( "hud_draw", "1", FCVAR_ARCHIVE );
	cl_lw = gEngfuncs.pfnGetCvarPointer( "cl_lw" );
	mat_writerrorfile = CVAR_CREATE( "mat_writerrorfile", "0", 0 );
	mat_autocreate = CVAR_CREATE( "mat_autocreate", "0", 0 );
	cl_righthand = CVAR_CREATE( "cl_righthand", "1", FCVAR_ARCHIVE );
	cl_dynamiccrosshair = CVAR_CREATE( "cl_dynamiccrosshair", "1", FCVAR_ARCHIVE );
	cl_levellocks = CVAR_CREATE( "cl_levellocks", "1", FCVAR_ARCHIVE );
	cl_weather = CVAR_CREATE( "cl_weather", "2", FCVAR_ARCHIVE );
	cl_newgame = CVAR_CREATE( "cl_newgame", "0", 0 );
	m_pSpriteList = NULL;

	// Clear any old HUD list
	while ( m_pHudList )
	{
		HUDLIST* element = m_pHudList;
		m_pHudList = element->pNext;
		free( element );
	}
	m_flTime = 1;
	m_SayText.Init();
	m_Spectator.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_NightVision.Init();
	m_SniperLaser.Init();
	m_FiberCamera.Init();
	m_scope.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_StatusBar.Init();
	m_StatusIcons.Init();
	m_Particles.Init();
	m_Ammo.Init();
	m_Health.Init();
	m_HudIcons.Init();
	m_progressBar.Init();
	m_levelLocks.Init();
	m_WorldMap.Init();
	m_TextMessage.Init();
	m_Message.Init();
	m_Menu.Init();
	m_Benchmark.Init();
	GetClientVoice()->Init( &g_VoiceStatusHelper );
	MsgFunc_ResetHUD( NULL, 0, NULL );
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
}

void CHud::VidInit()
{
	m_scrinfo.iSize = sizeof( m_scrinfo );
	GetScreenInfo( &m_scrinfo );

	// ----------
	// Load Sprites
	// ---------
//	m_hsprFont = LoadSprite("sprites/%d_font.spr");

	m_hsprLogo = 0;
	m_hsprCursor = 0;
	m_iRes = ScreenWidth >= 640 ? 640 : 320;


	// Only load this once
	if ( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList( "sprites/hud.txt", &m_iSpriteCountAllRes );
		if ( m_pSpriteList )
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			for ( int i = 0; i < m_iSpriteCountAllRes; ++i )
			{
				if ( m_pSpriteList[i].iRes == m_iRes )
					++m_iSpriteCount;
			}

			// allocated memory for sprite handle arrays
			m_rghSprites = new HSPRITE[m_iSpriteCount];
			m_rgrcRects = new wrect_t[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];
			int index = 0;
			for ( int i = 0; i < m_iSpriteCountAllRes; ++i )
			{
				client_sprite_t& sprite = m_pSpriteList[i];
				if ( sprite.iRes != m_iRes )
					continue;
				char path[256];
				sprintf( path, "sprites/%s.spr", sprite.szSprite );
				m_rghSprites[index] = SPR_Load( path );
				m_rgrcRects[index] = sprite.rc;
				strncpy( m_rgszSpriteNames + index * MAX_SPRITE_NAME_LENGTH, sprite.szName, MAX_SPRITE_NAME_LENGTH );
				++index;
			}
		}
	}
	else
	{
		int index = 0;
		for ( int i = 0; i < m_iSpriteCountAllRes; ++i )
		{
			if ( m_pSpriteList[i].iRes != m_iRes )
				continue;
			char path[256];
			sprintf( path, "sprites/%s.spr", m_pSpriteList[i].szSprite );
			m_rghSprites[index++] = SPR_Load( path );
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );
	m_iFontHeight = GetSpriteRect( m_HUD_number_0 ).bottom - GetSpriteRect( m_HUD_number_0 ).top;
	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Spectator.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_NightVision.VidInit();
	m_SniperLaser.VidInit();
	m_FiberCamera.VidInit();
	m_scope.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_StatusIcons.VidInit();
	m_Benchmark.VidInit();
	m_Particles.VidInit();
	m_HudIcons.VidInit();
	m_progressBar.VidInit();
	m_WorldMap.VidInit();
	m_Message.VidInit();
	GetClientVoiceHud()->VidInit();
}

int CHud::GetSpriteIndex( const char* name )
{
	// look through the loaded sprite name list for SpriteName
	for ( int i = 0; i < m_iSpriteCount; ++i )
	{
		if ( !strncmp( name, m_rgszSpriteNames + i * MAX_SPRITE_NAME_LENGTH, MAX_SPRITE_NAME_LENGTH ) )
			return i;
	}
	return -1; // invalid sprite
}

void CHud::AddHudElem( CHudBase* element )
{
	HUDLIST *pdl, *ptemp;

//phudelem->Think();

	if ( !element )
		return;

	pdl = (HUDLIST*)malloc( sizeof( HUDLIST ) );
	if ( !pdl )
		return;

	memset( pdl, 0, sizeof( HUDLIST ) );
	pdl->p = element;

	if ( !m_pHudList )
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while ( ptemp->pNext )
		ptemp = ptemp->pNext;

	ptemp->pNext = pdl;
}

void COM_FileBase( const char* path, char* name )
{
	int length = (int)strlen( path );
	if ( !length )
	{
		name[0] = 0;
		return;
	}
	int end = length - 1;
	while ( end && path[end] != '.' && path[end] != '/' && path[end] != '\\' )
		--end;
	if ( path[end] != '.' )
		end = length - 1;
	else
		--end;
	int start = length - 1;
	while ( start >= 0 && path[start] != '/' && path[start] != '\\' )
		--start;
	if ( path[start] != '/' && path[start] != '\\' )
		start = 0;
	else
		++start;
	length = end - start + 1;
	strncpy( name, path + start, length );
	name[length] = 0;
}

int HUD_IsGame( const char* game )
{
	const char* directory = gEngfuncs.pfnGetGameDirectory();
	if ( !directory || !*directory )
		return 0;
	char name[1024];
	COM_FileBase( directory, name );
	return !_stricmp( name, game );
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
float HUD_GetFOV()
{
	if ( gEngfuncs.pDemoAPI->IsRecording() )
	{
		unsigned char buffer[100];
		memcpy( buffer, &g_lastFOV, sizeof( g_lastFOV ) );
		Demo_WriteBuffer( TYPE_ZOOM, sizeof( g_lastFOV ), buffer );
	}
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		g_lastFOV = g_demozoom;
	return g_lastFOV;
}

int CHud::MsgFunc_Logo( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );

	// update Train data
	m_iLogo = READ_BYTE();
	return 1;
}

int CHud::MsgFunc_SetFOV( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int fov = READ_BYTE();
	int defaultFov = (int)CVAR_GET_FLOAT( "default_fov" );
	if ( HUD_IsGame( "tfc" ) && cl_lw && cl_lw->value )
		return 1;
	g_lastFOV = (float)fov;
	m_iFOV = fov ? fov : defaultFov;
	if ( m_iFOV == defaultFov )
		m_flMouseSensitivity = 0;
	else
	{
		float scale = (float)fov / defaultFov * sensitivity->value;
		m_flMouseSensitivity = CVAR_GET_FLOAT( "zoom_sensitivity_ratio" ) * scale;
	}
	return 1;
}

float CHud::GetSensitivity()
{
	return m_flMouseSensitivity;
}

bool CHud::IsLevelLocked( int level )
{
	return ( m_levelLockBits >> level ) & 1;
}
