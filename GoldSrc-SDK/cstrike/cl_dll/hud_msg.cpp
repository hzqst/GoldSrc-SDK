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
//  hud_msg.cpp
//

#include <math.h>
#include <string.h>
#include <algorithm>
#include <memory>
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "ev_hldm.h"
#include "eventscripts.h"
#include "z_weather.h"
#include "fog.h"
#include "buy_presets.h"
#include "../public/game_controls/iviewport.h"
#include "hud_iga.h"

void CAM_ToFirstPerson();
extern "C" void HUD_VoiceStatus( int entity, qboolean talking );
void ShowCreditsMenu( bool disconnectAfter );
extern int g_iFreezeTimeOver;
extern bool g_bUnassigned;
extern cvar_t* cl_fog_density;
extern cvar_t* cl_fog_r;
extern cvar_t* cl_fog_g;
extern cvar_t* cl_fog_b;

void CHud::MsgFunc_InitHUD( const char*, int, void* )
{
	if ( gViewPortInterface )
		gViewPortInterface->OnLevelChange();
	for ( HUDLIST* item = m_pHudList; item; item = item->pNext )
	{
		if ( item->p )
			item->p->InitHUDData();
	}
	g_Environment.Initialize();
	g_iFreezeTimeOver = 0;
	g_bUnassigned = true;
	m_autoBuyStringSent = false;
	m_rebuyStringSent = false;
	if ( !TheBuyPresets )
		TheBuyPresets = new BuyPresetManager;
	TheBuyPresets->ResetItems();
	memset( &g_FogParameters, 0, sizeof( g_FogParameters ) );
	if ( cl_fog_density )
		gEngfuncs.Cvar_SetValue( cl_fog_density->name, 0 );
	if ( cl_fog_r )
		gEngfuncs.Cvar_SetValue( cl_fog_r->name, g_FogParameters.color[0] );
	if ( cl_fog_g )
		gEngfuncs.Cvar_SetValue( cl_fog_g->name, g_FogParameters.color[1] );
	if ( cl_fog_b )
		gEngfuncs.Cvar_SetValue( cl_fog_b->name, g_FogParameters.color[2] );
	for ( int i = 0; i <= MAX_PLAYERS; i++ )
		g_PlayerExtraInfo[i].location[0] = 0;
}

void CHud::MsgFunc_Fog( const char*, int size, void* buffer )
{
	memset( &g_FogParameters, 0, sizeof( g_FogParameters ) );
	BEGIN_READ( buffer, size );
	for ( int i = 0; i < 3; i++ )
		g_FogParameters.color[i] = (float)READ_BYTE();
	unsigned int density = (unsigned char)READ_BYTE();
	density |= (unsigned char)READ_BYTE() << 8;
	density |= (unsigned char)READ_BYTE() << 16;
	density |= READ_BYTE() << 24;
	if ( READ_OK() )
		memcpy( &g_FogParameters.density, &density, sizeof( density ) );
	else
		memset( &g_FogParameters, 0, sizeof( g_FogParameters ) );
	g_FogParameters.affectsSkyBox = false;
	if ( cl_fog_density )
		gEngfuncs.Cvar_SetValue( cl_fog_density->name, g_FogParameters.density );
	if ( cl_fog_r )
		gEngfuncs.Cvar_SetValue( cl_fog_r->name, g_FogParameters.color[0] );
	if ( cl_fog_g )
		gEngfuncs.Cvar_SetValue( cl_fog_g->name, g_FogParameters.color[1] );
	if ( cl_fog_b )
		gEngfuncs.Cvar_SetValue( cl_fog_b->name, g_FogParameters.color[2] );
}

int CHud::MsgFunc_ResetHUD( const char*, int, void* )
{
	for ( HUDLIST* item = m_pHudList; item; item = item->pNext )
	{
		if ( item->p )
			item->p->Reset();
	}

	// reset sensitivity
	m_flMouseSensitivity = 0;
	for ( int i = 0; i < MAX_PLAYERS; i++ )
		m_PlayerFOV[i] = default_fov->value >= 90 ? (int)default_fov->value : 90;
	m_iConcussionEffect = 0;
	for ( int entity : m_botVoiceEntities )
		HUD_VoiceStatus( entity, false );
	m_botVoiceEntities.clear();
	if ( g_pClientIGA )
		g_pClientIGA->Reset();
	return 1;
}

void CHud::MsgFunc_ViewMode( const char*, int, void* )
{
	CAM_ToFirstPerson();
}

void CHud::MsgFunc_BombDrop( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	extra_player_info_t& bomb = g_PlayerExtraInfo[33];
	bomb.origin.x = READ_COORD();
	bomb.origin.y = READ_COORD();
	bomb.origin.z = READ_COORD();
	bomb.playerclass = READ_BYTE();
	if ( bomb.playerclass == 1 )
		m_bShowTimer = false;
	bomb.radarflashon = 1;
	bomb.radarflash = gHUD.m_flTime;
	bomb.radarflashes = 99999;
	strcpy( bomb.teamname, "TERRORIST" );
	bomb.dead = false;
}

void CHud::MsgFunc_BombPickup( const char*, int, void* )
{
	g_PlayerExtraInfo[33].radarflash = -1;
	g_PlayerExtraInfo[33].radarflashon = 0;
	g_PlayerExtraInfo[33].radarflashes = 0;
}

void CHud::MsgFunc_ShowTimer( const char*, int, void* )
{
	m_bShowTimer = true;
}

void CHud::MsgFunc_ADStop( const char*, int, void* )
{
	gHUD.m_Spectator.m_autoDirector->value = 0;
}

void CHud::MsgFunc_HostagePos( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int flash = READ_BYTE();
	int index = READ_BYTE();
	if ( index <= 24 )
	{
		hostage_info_t& hostage = g_HostageInfo[index];
		hostage.origin.x = READ_COORD();
		hostage.origin.y = READ_COORD();
		hostage.origin.z = READ_COORD();
		if ( flash == 1 )
		{
			hostage.radarflashon = 1;
			hostage.radarflashes = 99999;
			hostage.radarflash = gHUD.m_flTime;
		}
		strcpy( hostage.teamname, "CT" );
		hostage.dead = false;
	}
}

void CHud::MsgFunc_HostageK( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int index = READ_BYTE();
	if ( index <= 24 )
	{
		g_HostageInfo[index].radarflash = gHUD.m_flTime;
		g_HostageInfo[index].radarflashes = 15;
		g_HostageInfo[index].dead = true;
	}
}

int CHud::MsgFunc_GameMode( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_Teamplay = READ_BYTE();
	return 1;
}

int CHud::MsgFunc_Damage( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	READ_BYTE();
	READ_BYTE();
	READ_COORD();
	READ_COORD();
	READ_COORD();
	return 1;
}

int CHud::MsgFunc_Concuss( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iConcussionEffect = READ_BYTE();
	if ( m_iConcussionEffect )
		m_StatusIcons.EnableIcon( "dmg_concuss", 255, 160, 0, false );
	else
		m_StatusIcons.DisableIcon( "dmg_concuss" );
	return 1;
}

int CHud::MsgFunc_ReloadSound( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	float volume = READ_BYTE() / 255.0;
	bool generic = READ_BYTE() != 0;
	gEngfuncs.pfnPlaySoundByName( generic ? "weapons/generic_reload.wav" : "weapons/generic_shot_reload.wav", volume );
	return 1;
}

int CHud::MsgFunc_Brass( const char*, int size, void* buffer )
{
	int conditionZero = IsGame( "czero" );
	BEGIN_READ( buffer, size );
	if ( !conditionZero )
		READ_BYTE();
	Vector origin;
	origin.x = READ_COORD();
	origin.y = READ_COORD();
	origin.z = READ_COORD();
	if ( !conditionZero )
	{
		READ_COORD();
		READ_COORD();
		READ_COORD();
	}
	Vector velocity;
	velocity.x = READ_COORD();
	velocity.y = READ_COORD();
	velocity.z = READ_COORD();
	float angle = READ_ANGLE();
	int model = READ_SHORT();
	int soundType = READ_BYTE();
	if ( !conditionZero )
		READ_BYTE();
	int index = READ_BYTE();
	float rotation = 3.1416 * angle / 180.0;
	float sine = (float)sin( rotation );
	float cosine = (float)cos( rotation );
	float offsetX = -9 * sine;
	float offsetY = 9 * cosine;
	float rightHand = gEngfuncs.pfnGetCvarFloat( "cl_righthand" );
	int local = EV_IsLocal( index );
	if ( (int)rightHand || !local )
	{
		velocity.x += -120 * sine;
		velocity.y += 120 * cosine;
		offsetX = -offsetX;
		offsetY = -offsetY;
	}
	origin.x += offsetX;
	origin.y += offsetY;
	EV_EjectBrass( origin, velocity, rotation, model, soundType, 0, 1 );
	return 1;
}

int CHud::MsgFunc_BotVoice( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	qboolean talking = READ_BYTE();
	int entity = READ_BYTE();
	HUD_VoiceStatus( entity, talking );
	if ( talking )
		m_botVoiceEntities.push_back( entity );
	else
	{
		std::vector<int>::iterator item = std::find( m_botVoiceEntities.begin(), m_botVoiceEntities.end(), entity );
		if ( item != m_botVoiceEntities.end() )
			m_botVoiceEntities.erase( item );
	}
	return 0;
}

void CHud::SendAutoBuyString()
{
	char* file = (char*)gEngfuncs.COM_LoadFile( "autobuy.txt", 5, NULL );
	if ( !file )
		return;
	auto releaseFile = []( void* buffer ) { gEngfuncs.COM_FreeFile( buffer ); };
	std::unique_ptr<void, decltype( releaseFile )> fileCleanup( file, releaseFile );
	char command[256] = "cl_setautobuy \"";
	char token[1024];
	char* scan = gEngfuncs.COM_ParseFile( file, token );
	while ( scan )
	{
		strncat( command, token, sizeof( command ) - strlen( command ) - 1 );
		scan = gEngfuncs.COM_ParseFile( scan, token );
		if ( scan )
			strncat( command, " ", sizeof( command ) - strlen( command ) - 1 );
	}
	strncat( command, "\"", sizeof( command ) - strlen( command ) - 1 );
	command[sizeof( command ) - 1] = 0;
	gEngfuncs.pfnClientCmd( command );
}

void CHud::SendRebuyString()
{
	char* file = (char*)gEngfuncs.COM_LoadFile( "rebuy.txt", 5, NULL );
	if ( !file )
		return;
	auto releaseFile = []( void* buffer ) { gEngfuncs.COM_FreeFile( buffer ); };
	std::unique_ptr<void, decltype( releaseFile )> fileCleanup( file, releaseFile );
	char command[256] = "cl_setrebuy \"";
	char token[1024];
	bool first = true;
	for ( char* scan = gEngfuncs.COM_ParseFile( file, token ); scan; scan = gEngfuncs.COM_ParseFile( scan, token ) )
	{
		if ( stricmp( token, "ExtraCash" ) )
		{
			if ( !first )
				strncat( command, " ", sizeof( command ) - strlen( command ) - 1 );
			strncat( command, token, sizeof( command ) - strlen( command ) - 1 );
			first = false;
		}
	}
	strncat( command, "\"", sizeof( command ) - strlen( command ) - 1 );
	command[sizeof( command ) - 1] = 0;
	gEngfuncs.pfnClientCmd( command );
}

void CHud::AutoBuy()
{
	if ( m_autoBuyStringSent )
		gEngfuncs.pfnClientCmd( "cl_autobuy" );
	else
	{
		SendAutoBuyString();
		m_autoBuyStringSent = true;
	}
}

void CHud::Rebuy()
{
	if ( m_rebuyStringSent )
		gEngfuncs.pfnClientCmd( "cl_rebuy" );
	else
	{
		SendRebuyString();
		m_rebuyStringSent = true;
	}
}

void CHud::RunCredits()
{
	ShowCreditsMenu( false );
}
