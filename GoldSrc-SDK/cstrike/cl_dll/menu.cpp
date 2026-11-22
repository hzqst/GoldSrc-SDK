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
#include "../public/game_controls/iviewport.h"

#define MAX_MENU_STRING 512

char g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];


/*=================================
  ParseEscapeToken

  Interprets the given escape token (backslash followed by a letter). The
  first character of the token must be a backslash.  The second character
  specifies the operation to perform:

   \w : White text (this is the default)
   \d : Dim (gray) text
   \y : Yellow text
   \r : Red text
   \R : Right-align (just for the remainder of the current line)
=================================*/

static int menu_r, menu_g, menu_b, menu_x, menu_ralign;

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
	for ( int i = 0; i < MAX_PLAYERS; i++ )
	{
		g_PlayerExtraInfo[i].radarflash = -1;
		g_PlayerExtraInfo[i].radarflashon = 0;
		g_PlayerExtraInfo[i].radarflashes = 0;
		g_PlayerExtraInfo[i].dead = true;
	}
	for ( int i = 0; i < 24; i++ )
	{
		g_HostageInfo[i].radarflash = -1;
		g_HostageInfo[i].radarflashon = 0;
		g_HostageInfo[i].radarflashes = 0;
		g_HostageInfo[i].dead = true;
	}
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
	menu_r = menu_g = menu_b = 255;
	menu_x = 20;
	menu_ralign = 0;
	const char* text = g_szMenuString;
	while ( *text )
	{
		if ( *text == '\\' )
		{
			text++;
			switch ( *text )
			{
			case 0:
				return 1;
			case 'w':
				menu_r = menu_g = menu_b = 255;
				break;
			case 'd':
				menu_r = menu_g = menu_b = 100;
				break;
			case 'y':
				menu_r = 255;
				menu_g = 210;
				menu_b = 64;
				break;
			case 'r':
				menu_r = 210;
				menu_g = 24;
				menu_b = 0;
				break;
			case 'R':
				menu_x = ScreenWidth / 2;
				menu_ralign = 1;
				break;
			}
			text++;
		}
		else if ( *text == '\n' )
		{
			menu_ralign = 0;
			menu_x = 20;
			y += height + 2;
			text++;
		}
		else
		{
			char menubuf[80];
			const char* start = text;
			while ( *text && *text != '\n' && *text != '\\' )
				text++;
			int length = text - start;
			strncpy( menubuf, start, length < 80 ? length : 80 );
			menubuf[length < 79 ? length : 79] = 0;
			if ( menu_ralign )
				menu_x = gHUD.DrawHudStringReverse( menu_x, y, 0, menubuf, menu_r, menu_g, menu_b );
			else
				menu_x = gHUD.DrawHudString( menu_x, y, 320, menubuf, menu_r, menu_g, menu_b );
		}
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
			char* converted = NULL;
			if ( KB_ConvertString( g_szMenuString, &converted ) )
			{
				strncpy( g_szMenuString, converted, MAX_MENU_STRING );
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
