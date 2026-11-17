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

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"
#include "triangleapi.h"
#include "../../engine/interface.h"
#include "../../public/particleman.h"
#include "../public/game_controls/legacy_iviewport.h"
#include "z_weather.h"
#include "effects/e_effectclasses.h"

void CAM_ToFirstPerson();
void InitViewModelGlass();
extern BEAM *pBeam, *pBeam2;

float g_iFogColor[3];
float g_flFogDensity;
float g_flFogStartDistance;
float g_flFogStopDistance;
int g_iFogSkybox;
bool g_bRenderFog;

int CHud::MsgFunc_ResetHUD( const char*, int, void* )
{
	for ( HUDLIST* element = m_pHudList; element; element = element->pNext )
	{
		if ( element->p )
			element->p->Reset();
	}

	// reset sensitivity
	m_flMouseSensitivity = 0;
	m_iConcussionEffect = 0;
	InitViewModelGlass();
	gEngfuncs.pfnSetFilterMode( 0 );
	gEngfuncs.pfnSetFilterColor( 0, 0, 0 );
	gEngfuncs.pfnSetFilterBrightness( 0 );
	return 1;
}

void CHud::MsgFunc_ViewMode( const char*, int, void* )
{
	CAM_ToFirstPerson();
}

void CHud::MsgFunc_InitHUD( const char*, int size, void* buffer )
{
	if ( gViewPortInterface )
		gViewPortInterface->OnLevelChange();
	HUDLIST* list = m_pHudList;
	g_iFogSkybox = 0;
	gHUD.m_flMaterialCheckTime = 0.1f;
	VectorClear( g_iFogColor );
	g_SmokePuffQueue.count = 0;
	g_flFogDensity = 0;
	g_flFogStartDistance = 1000;
	g_flFogStopDistance = 3000;
	g_bRenderFog = false;
	gEngfuncs.pTriAPI->FogParams( 0, 0 );
	gEngfuncs.pTriAPI->Fog( g_iFogColor, -1, -1, g_bRenderFog );
	g_Environment.Initialize();
	for ( int i = 0; i < 40; ++i )
		g_GlassPanes[i].ClearAll();
	BEGIN_READ( buffer, size );
	for ( int i = 0; i < 3; ++i )
		g_iFogColor[i] = (float)READ_SHORT();
	g_flFogDensity = (float)( READ_COORD() / 1000.0 );
	g_flFogStartDistance = READ_COORD();
	g_flFogStopDistance = READ_COORD();
	g_iFogSkybox = READ_SHORT();
	g_bRenderFog = g_flFogDensity != 0;
	g_pParticleMan->ResetParticles();
	for ( ; list; list = list->pNext )
	{
		if ( list->p )
			list->p->InitHUDData();
	}
	//Probably not a good place to put this.
	pBeam = pBeam2 = NULL;
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
		m_StatusIcons.EnableIcon( (char*)"dmg_concuss", 255, 160, 0 );
	else
		m_StatusIcons.DisableIcon( (char*)"dmg_concuss" );
	return 1;
}

int CHud::MsgFunc_SecurityCam( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_securityCamera = READ_BYTE();
	return 1;
}
