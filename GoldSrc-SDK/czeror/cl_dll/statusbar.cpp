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
// statusbar.cpp
//
// generic text status bar, set by game dll
// runs across bottom of screen
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <stdlib.h>

extern float g_ColorYellow[3];
float* GetClientColor( int clientIndex );

DECLARE_MESSAGE( m_StatusBar, StatusText )
DECLARE_MESSAGE( m_StatusBar, StatusValue )

int CHudStatusBar::Init()
{
	gHUD.AddHudElem( this );
	HOOK_MESSAGE( StatusText );
	HOOK_MESSAGE( StatusValue );
	Reset();
	CVAR_CREATE( "hud_centerid", "0", FCVAR_ARCHIVE );
	return 1;
}

int CHudStatusBar::VidInit()
{
	// Load sprites here

	return 1;
}

void CHudStatusBar::Reset()
{
	m_iFlags &= ~HUD_ACTIVE;  // start out inactive
	for ( int i = 0; i < MAX_STATUSBAR_LINES; ++i )
		m_szStatusText[i][0] = 0;
	memset( m_iStatusValues, 0, sizeof( m_iStatusValues ) );
	m_iStatusValues[0] = 1;  // 0 is the special index, which always returns true

	// reset our colors for the status bar lines (yellow is default)
	for ( int i = 0; i < MAX_STATUSBAR_LINES; ++i )
		m_pflNameColors[i] = g_ColorYellow;
}

int CHudStatusBar::MsgFunc_StatusText( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	unsigned int line = READ_BYTE();
	if ( line < MAX_STATUSBAR_LINES )
	{
		strncpy( m_szStatusText[line], READ_STRING(), sizeof( m_szStatusText[line] ) );
		m_szStatusText[line][sizeof( m_szStatusText[line] ) - 1] = 0;
		m_iFlags |= HUD_ACTIVE;
		m_bReparseString = TRUE;
	}
	return 1;
}

int CHudStatusBar::MsgFunc_StatusValue( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int index = READ_BYTE();
	if ( index >= 1 && index < MAX_STATUSBAR_VALUES )
	{
		m_iStatusValues[index] = READ_SHORT();
		m_bReparseString = TRUE;
	}
	return 1;
}

void CHudStatusBar::ParseStatusString( int line_num )
{
	// localise string first
	char szBuffer[MAX_STATUSTEXT_LENGTH];
	memset( szBuffer, 0, sizeof szBuffer );
	CHudTextMessage::LocaliseTextString( m_szStatusText[line_num], szBuffer, MAX_STATUSTEXT_LENGTH );

	// parse m_szStatusText & m_iStatusValues into m_szStatusBar
	memset( m_szStatusBar[line_num], 0, MAX_STATUSTEXT_LENGTH );
	char* src = szBuffer;
	char* dst = m_szStatusBar[line_num];

	char *src_start = src, *dst_start = dst;

	while ( *src != 0 )
	{
		while ( *src == '\n' )
			src++; // skip over any newlines

		if ( ( ( src - src_start ) >= MAX_STATUSTEXT_LENGTH ) || ( ( dst - dst_start ) >= MAX_STATUSTEXT_LENGTH - 1 ) )
			break;

		int index = atoi( src );
		// should we draw this line?
		if ( ( index >= 0 && index < MAX_STATUSBAR_VALUES ) && ( m_iStatusValues[index] != 0 ) )
		{ // parse this line and append result to the status bar
			while ( *src >= '0' && *src <= '9' )
				src++;

			if ( *src == '\n' || *src == 0 )
				continue; // no more left in this text line

			// copy the text, char by char, until we hit a % or a \n
			while ( *src != '\n' && *src != 0 )
			{
				if ( ( dst - dst_start ) >= MAX_STATUSTEXT_LENGTH - 1 )
					break;

				if ( *src != '%' )
				{ // just copy the character
					*dst = *src;
					dst++, src++;
				}
				else
				{
					// get the descriptor
					char valtype = *( ++src ); // move over %

					// if it's a %, draw a % sign
					if ( valtype == '%' )
					{
						*dst = valtype;
						dst++, src++;
						continue;
					}

					if ( valtype == 0 )
						break;

					// move over descriptor, then get and move over the index
					index = atoi( ++src );
					while ( *src >= '0' && *src <= '9' )
						src++;

					if ( index >= 0 && index < MAX_STATUSBAR_VALUES )
					{
						int indexval = m_iStatusValues[index];

						// get the string to substitute in place of the %XX
						char szRepString[MAX_PLAYER_NAME_LENGTH];
						szRepString[0] = 0;
						switch ( valtype )
						{
						case 'p': // player name
							if ( indexval < 1 || indexval > MAX_PLAYERS )
							{
								strcpy( szRepString, "******" );
								break;
							}
							gEngfuncs.pfnGetPlayerInfo( indexval, &g_PlayerInfoList[indexval] );
							if ( g_PlayerInfoList[indexval].name != NULL )
							{
								strncpy( szRepString, g_PlayerInfoList[indexval].name, MAX_PLAYER_NAME_LENGTH );
								szRepString[MAX_PLAYER_NAME_LENGTH - 1] = 0;
								m_pflNameColors[line_num] = GetClientColor( indexval );
							}
							else
							{
								strcpy( szRepString, "******" );
							}

							break;
						case 'i': // number
							sprintf( szRepString, "%d", indexval );
							break;

						default:
							szRepString[0] = 0;
						}

						for ( char* cp = szRepString; *cp != 0 && ( ( dst - dst_start ) < MAX_STATUSTEXT_LENGTH - 1 ); cp++, dst++ )
							*dst = *cp;
					}
				}
			}
		}
		else
		{
			// skip to next line of text
			while ( *src != 0 && *src != '\n' )
				src++;
		}
	}
	*dst = 0;
}

int CHudStatusBar::Draw( float )
{
	if ( m_bReparseString )
	{
		for ( int i = 0; i < MAX_STATUSBAR_LINES; ++i )
		{
			m_pflNameColors[i] = g_ColorYellow;
			ParseStatusString( i );
		}
		m_bReparseString = FALSE;
	}
	int start = ScreenHeight - (int)( ( (float)ScreenHeight / 480.0f ) * 36.0f + 0.5f );
	for ( int i = 0; i < MAX_STATUSBAR_LINES; ++i )
	{
		int width, height;
		GetConsoleStringSize( m_szStatusBar[i], &width, &height );
		int x = 4;
		int y = start - ( i * height + 4 );
		if ( i == 1 && CVAR_GET_FLOAT( "hud_centerid" ) != 0 )
		{
			int available = ScreenWidth - width;
			if ( available < 2 )
				available = 2;
			x = available / 2;
			y = (int)(float)( CVAR_GET_FLOAT( "hud_centerid" ) * height + (float)( ScreenHeight / 2 ) );
		}
		if ( m_pflNameColors[i] )
			gEngfuncs.pfnDrawSetTextColor( m_pflNameColors[i][0], m_pflNameColors[i][1], m_pflNameColors[i][2] );
		DrawConsoleString( x, y, m_szStatusBar[i] );
	}
	return 1;
}
