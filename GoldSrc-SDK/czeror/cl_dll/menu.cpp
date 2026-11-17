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
// menu.cpp
//
// generic menu handler
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "../public/game_controls/legacy_iviewport.h"

#define MAX_MENU_STRING 512

char g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];

int KB_ConvertString( char* input, char** output );
DECLARE_MESSAGE( m_Menu, ShowMenu );

int CHudMenu::Init()
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( ShowMenu );
	InitHUDData();
	return 1;
}

void CHudMenu::InitHUDData()
{
	m_fMenuDisplayed = 0;
	m_bitsValidSlots = 0;
	Reset();
}

void CHudMenu::Reset()
{
	g_szPrelocalisedMenuString[0] = 0;
	m_fWaitingForMore = 0;
}

int CHudMenu::VidInit()
{
	return 1;
}

int CHudMenu::Draw( float )
{
	if ( m_flShutoffTime > 0 && gHUD.m_flTime >= m_flShutoffTime )
	{  // times up, shutoff
		m_fMenuDisplayed = 0;
		m_iFlags &= ~HUD_ACTIVE;
		return 1;
	}
	if ( gViewPortInterface && gViewPortInterface->IsScoreBoardVisible() )
		return 1;
	SCREENINFO info;
	info.iSize = sizeof( info );
	gEngfuncs.pfnGetScreenInfo( &info );
	int lines = 0;
	for ( int i = 0; i < MAX_MENU_STRING && g_szMenuString[i]; i++ )
	{
		if ( g_szMenuString[i] == '\n' )
			lines++;
	}
	int height = info.iCharHeight >= 12 ? info.iCharHeight : 12;
	int y = ScreenHeight / 2 - lines / 2 * height - ( 3 * height + height / 3 );
	int index = 0;
	while ( g_szMenuString[index] )
	{
		gHUD.DrawHudString( 20, y, 320, g_szMenuString + index, 255, 255, 255 );
		y += height;
		while ( g_szMenuString[index] && g_szMenuString[index] != '\n' )
		{
			if ( ++index == MAX_MENU_STRING )
				return 1;
		}
		if ( g_szMenuString[index] == '\n' && ++index >= MAX_MENU_STRING )
			break;
	}
	return 1;
}

// selects an item from the menu
void CHudMenu::SelectMenuItem( int item )
{
	// if menu_item is in a valid slot,  send a menuselect command to the server
	if ( item > 0 && ( m_bitsValidSlots & ( 1 << ( item - 1 ) ) ) )
	{
		char command[32];
		sprintf( command, "menuselect %d\n", item );
		EngineClientCmd( command );

		// remove the menu
		m_fMenuDisplayed = 0;
		m_iFlags &= ~HUD_ACTIVE;
	}
}


// Message handler for ShowMenu message
// takes four values:
//		short: a bitfield of keys that are valid input
//		char : the duration, in seconds, the menu should stay up. -1 means is stays until something is chosen.
//		byte : a boolean, TRUE if there is more string yet to be received before displaying the menu, FALSE if it's the last string
//		string: menu string to display
// if this message is never received, then scores will simply be the combined totals of the players.
int CHudMenu::MsgFunc_ShowMenu( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_bitsValidSlots = READ_SHORT();
	int duration = READ_CHAR();
	int more = READ_BYTE();
	m_flShutoffTime = duration > 0 ? duration + gHUD.m_flTime : -1;
	if ( m_bitsValidSlots )
	{
		if ( m_fWaitingForMore )
			strncat( g_szPrelocalisedMenuString, READ_STRING(), MAX_MENU_STRING - strlen( g_szPrelocalisedMenuString ) - 1 );
		else
			strncpy( g_szPrelocalisedMenuString, READ_STRING(), MAX_MENU_STRING );
		g_szPrelocalisedMenuString[MAX_MENU_STRING - 1] = 0;  // ensure null termination (strncat/strncpy does not)
		if ( !more )
		{
			strncpy( g_szMenuString, gHUD.m_TextMessage.BufferedLocaliseTextString( g_szPrelocalisedMenuString ), MAX_MENU_STRING );
			g_szMenuString[MAX_MENU_STRING - 1] = 0;
			char* converted = NULL;
			if ( KB_ConvertString( g_szMenuString, &converted ) )
			{
				strncpy( g_szMenuString, converted, MAX_MENU_STRING );
				g_szMenuString[MAX_MENU_STRING - 1] = 0;
				free( converted );
			}
		}
		m_fMenuDisplayed = 1;
		m_iFlags |= HUD_ACTIVE;
	}
	else
	{
		m_fMenuDisplayed = 0; // no valid slots means that the menu should be turned off
		m_iFlags &= ~HUD_ACTIVE;
	}
	m_fWaitingForMore = more;
	return 1;
}
