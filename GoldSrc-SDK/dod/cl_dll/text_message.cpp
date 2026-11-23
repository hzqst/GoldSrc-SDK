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
// text_message.cpp
//
// implementation of CHudTextMessage class
//
// this class routes messages through titles.txt for localisation
//

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"

#include "../public/game_controls/iviewport.h"

extern cvar_t* cl_hud_msgs;

DECLARE_MESSAGE( m_TextMessage, TextMsg );
DECLARE_MESSAGE( m_TextMessage, CapMsg );

int CHudTextMessage::Init(void)
{
	HOOK_MESSAGE( TextMsg );
	HOOK_MESSAGE( CapMsg );

	gHUD.AddHudElem( this );

	Reset();

	return 1;
};

// Searches through the string for any msg names (indicated by a '#')
// any found are looked up in titles.txt and the new message substituted
// the new value is pushed into dst_buffer
char *CHudTextMessage::LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size )
{
	if ( buffer_size <= 0 )
		return dst_buffer;

	char *dst = dst_buffer;
	char *dst_end = dst_buffer + buffer_size - 1;
	for ( const char *src = msg; *src != 0 && dst < dst_end; )
	{
		if ( *src == '#' )
		{
			// cut msg name out of string
			static char word_buf[255];
			char *wdst = word_buf;
			const char *word_start = src;
			for ( ++src ; ((*src >= 'A' && *src <= 'z') || (*src >= '0' && *src <= '9')) && wdst < word_buf + sizeof( word_buf ) - 1; wdst++, src++ )
			{
				*wdst = *src;
			}
			*wdst = 0;

			// lookup msg name in titles.txt
			client_textmessage_t *clmsg = TextMessageGet( word_buf );
			if ( !clmsg || !(clmsg->pMessage) )
			{
				src = word_start;
				*dst = *src;
				dst++, src++;
				continue;
			}

			// copy string into message over the msg name
			for ( const char *wsrc = clmsg->pMessage; *wsrc != 0 && dst < dst_end; wsrc++, dst++ )
			{
				*dst = *wsrc;
			}
		}
		else
		{
			*dst = *src;
			dst++, src++;
		}
	}

	*dst = 0; // ensure null termination
	return dst_buffer;
}

// As above, but with a local static buffer
char *CHudTextMessage::BufferedLocaliseTextString( const char *msg )
{
	static char dst_buffer[1024];
	LocaliseTextString( msg, dst_buffer, 1024 );
	return dst_buffer;
}

// Simplified version of LocaliseTextString;  assumes string is only one word
char *CHudTextMessage::LookupString( const char *msg, int *msg_dest )
{
	if ( !msg )
		return "";

	// '#' character indicates this is a reference to a string in titles.txt, and not the string itself
	if ( msg[0] == '#' )
	{
		// this is a message name, so look up the real message
		client_textmessage_t *clmsg = TextMessageGet( msg+1 );

		if ( !clmsg || !(clmsg->pMessage) )
			return (char*)msg; // lookup failed, so return the original string

		if ( msg_dest )
		{
			// check to see if titles.txt info overrides msg destination
			// if clmsg->effect is less than 0, then clmsg->effect holds -1 * message_destination
			if ( clmsg->effect < 0 )  //
				*msg_dest = -clmsg->effect;
		}

		return (char*)clmsg->pMessage;
	}
	else
	{  // nothing special about this message, so just return the same string
		return (char*)msg;
	}
}

void StripEndNewlineFromString( char *str )
{
	int s = strlen( str ) - 1;
	if ( str[s] == '\n' || str[s] == '\r' )
		str[s] = 0;
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
char* ConvertCRtoNL( char *str )
{
	for ( char *ch = str; *ch != 0; ch++ )
		if ( *ch == '\r' )
			*ch = '\n';
	return str;
}

// Message handler for text messages
// displays a string, looking them up from the titles.txt file, which can be localised
// parameters:
//   byte:   message direction  ( HUD_PRINTCONSOLE, HUD_PRINTNOTIFY, HUD_PRINTCENTER, HUD_PRINTTALK )
//   string: message
// optional parameters:
//   string: message parameter 1
//   string: message parameter 2
//   string: message parameter 3
//   string: message parameter 4
// any string that starts with the character '#' is a message name, and is used to look up the real message in titles.txt
// the next (optional) one to four strings are parameters for that string (which can also be message names if they begin with '#')
int CHudTextMessage::MsgFunc_TextMsg( const char*, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int msg_dest = READ_BYTE();

#define MSG_BUF_SIZE 128
	static char szBuf[6][MSG_BUF_SIZE];
	char *msg_text = LookupString( READ_STRING(), &msg_dest );
	msg_text = strcpy( szBuf[0], msg_text );

	// keep reading strings and using C format strings for subsituting the strings into the localised text string
	char *sstr1 = LookupString( READ_STRING() );
	sstr1 = strcpy( szBuf[1], sstr1 );
	StripEndNewlineFromString( sstr1 );  // these strings are meant for subsitution into the main strings, so cull the automatic end newlines
	char *sstr2 = LookupString( READ_STRING() );
	sstr2 = strcpy( szBuf[2], sstr2 );
	StripEndNewlineFromString( sstr2 );
	char *sstr3 = LookupString( READ_STRING() );
	sstr3 = strcpy( szBuf[3], sstr3 );
	StripEndNewlineFromString( sstr3 );
	char *sstr4 = LookupString( READ_STRING() );
	sstr4 = strcpy( szBuf[4], sstr4 );
	StripEndNewlineFromString( sstr4 );
	char *psz = szBuf[5];

	if ( gViewPortInterface && !gViewPortInterface->AllowedToPrintText() )
		return 1;

	switch ( msg_dest )
	{
	case HUD_PRINTCENTER:
		StripEndNewlineFromString( msg_text );
		gHUD.m_VGUI2Print.VGUI2HudPrintArgs( msg_text, sstr1, sstr2, sstr3, sstr4, -1,
			ScreenWidth / 2 - (int)(ScreenHeight / 480.0 * 20 + 0.5), 1, 1, 1 );
		break;

	case HUD_PRINTNOTIFY:
		psz[0] = 1;  // mark this message to go into the notify buffer
		sprintf( psz+1, msg_text, sstr1, sstr2, sstr3, sstr4 );
		ConsolePrint( ConvertCRtoNL( psz ) );
		break;

	case HUD_PRINTTALK:
		ConvertCRtoNL( psz );
		if ( cl_hud_msgs->value == 1 )
		{
			if ( strstr( psz, "has left the game" ) || strstr( psz, "has joined the game" ) )
				gHUD.m_SayText.SayTextPrint( psz, 128, -1, NULL, NULL, NULL, NULL );
		}
		else if ( cl_hud_msgs->value == 2 )
			gHUD.m_SayText.SayTextPrint( msg_text, 128, -1, sstr1, sstr2, sstr3, sstr4 );
		break;

	case HUD_PRINTCONSOLE:
		sprintf( psz, msg_text, sstr1, sstr2, sstr3, sstr4 );
		ConsolePrint( ConvertCRtoNL( psz ) );
		break;
	}

	return 1;
}

char* StripCRFromEnd( char* text )
{
	if ( text )
	{
		for ( char* ch = text; *ch; ch++ )
		{
			if ( *ch == '\r' )
				*ch = 0;
		}
	}
	return text;
}

int CHudTextMessage::MsgFunc_CapMsg( const char*, int size, void* data )
{
	BEGIN_READ( data, size );
	int client = READ_BYTE();
	char* player = "";
	char* message = "#MAP_GROUP_CAP";
	if ( client >= 1 && client <= 32 )
	{
		gEngfuncs.pfnGetPlayerInfo( client, &g_PlayerInfoList[client] );
		player = g_PlayerInfoList[client].name;
	}
	if ( client != 0 )
		message = "#MAP_PLAYER_CAP";

	char point[128];
	char temporary[256];
	snprintf( point, sizeof( point ), "%s", READ_STRING() );
	if ( point[0] == '#' )
		strcpy( point, BufferedLocaliseTextString( point ) );
	else
	{
		sprintf( temporary, "#%s", point );
		char* localized = BufferedLocaliseTextString( temporary );
		if ( strcmp( localized, temporary ) )
			strcpy( point, localized );
	}
	char* team = READ_BYTE() == 1 ? "#THE_ALLIES" : "#THE_AXIS";
	StripCRFromEnd( player );
	StripCRFromEnd( point );
	gHUD.m_SayText.SayTextPrint( message, 256, -1, team, point, player, NULL );
	return 1;
}
