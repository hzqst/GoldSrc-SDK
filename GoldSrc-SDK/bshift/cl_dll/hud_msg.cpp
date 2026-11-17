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
#include "particleman.h"
#include "game_controls/iviewport.h"

extern IParticleMan* g_pParticleMan;
extern BEAM* pBeam;
extern BEAM* pBeam2;

void CAM_ToFirstPerson();

/// USER-DEFINED SERVER MESSAGE HANDLERS

int CHud::MsgFunc_ResetHUD( const char* pszName, int iSize, void* pbuf )
{
	ASSERT( iSize == 0 );

	for ( HUDLIST* pList = m_pHudList; pList; pList = pList->pNext )
	{
		if ( pList->p )
			pList->p->Reset();
	}

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;
	return 1;
}

void CHud::MsgFunc_ViewMode( const char* pszName, int iSize, void* pbuf )
{
	CAM_ToFirstPerson();
}

void CHud::MsgFunc_InitHUD( const char* pszName, int iSize, void* pbuf )
{
	if ( gViewPortInterface )
		gViewPortInterface->OnLevelChange();

	for ( HUDLIST* pList = m_pHudList; pList; pList = pList->pNext )
	{
		if ( pList->p )
			pList->p->InitHUDData();
	}

	if ( g_pParticleMan )
		g_pParticleMan->ResetParticles();
	//Probably not a good place to put this.

	pBeam = pBeam2 = NULL;
}

int CHud::MsgFunc_GameMode( const char* pszName, int iSize, void* pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_Teamplay = READ_BYTE();
	return 1;
}

int CHud::MsgFunc_Damage( const char* pszName, int iSize, void* pbuf )
{
	BEGIN_READ( pbuf, iSize );
	READ_BYTE();
	READ_BYTE();
	for ( int i = 0; i < 3; i++ )
		READ_COORD();

	// TODO: kick viewangles,  show damage visually

	return 1;
}

int CHud::MsgFunc_Concuss( const char* pszName, int iSize, void* pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iConcussionEffect = READ_BYTE();
	if ( m_iConcussionEffect )
		m_StatusIcons.EnableIcon( "dmg_concuss", 255, 160, 0 );
	else
		m_StatusIcons.DisableIcon( "dmg_concuss" );
	return 1;
}
