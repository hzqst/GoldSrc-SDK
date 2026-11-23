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
#include "parsemsg.h"
#include "../public/game_controls/iviewport.h"
#include <stdlib.h>

DECLARE_MESSAGE( m_TextMessage, TextMsg );

int CHudTextMessage::Init()
{
	HOOK_MESSAGE( TextMsg );
	gHUD.AddHudElem( this );
	Reset();
	return 1;
}

char* CHudTextMessage::LocaliseTextString( const char* message, char* buffer, int size )
{
	if ( size <= 0 )
		return buffer;

	char* destination = buffer;
	char* destinationEnd = buffer + size - 1;
	for ( const char* source = message; *source && destination < destinationEnd; )
	{
		if ( *source == '#' )
		{
			// cut msg name out of string
			static char word_buf[255];
			char* word = word_buf;
			const char* start = source++;
			while ( ( ( *source >= 'A' && *source <= 'z' ) || ( *source >= '0' && *source <= '9' ) ) && word < word_buf + sizeof( word_buf ) - 1 )
				*word++ = *source++;
			*word = 0;
			client_textmessage_t* text = TextMessageGet( word_buf );
			if ( !text || !text->pMessage )
			{
				source = start;
				*destination++ = *source++;
				continue;
			}
			for ( const char* textSource = text->pMessage; *textSource && destination < destinationEnd; textSource++ )
				*destination++ = *textSource;
		}
		else
		{
			*destination++ = *source++;
		}
	}
	*destination = 0;
	return buffer;
}

// As above, but with a local static buffer
char* CHudTextMessage::BufferedLocaliseTextString( const char* message )
{
	static char dst_buffer[1024];
	LocaliseTextString( message, dst_buffer, sizeof( dst_buffer ) );
	return dst_buffer;
}

// Simplified version of LocaliseTextString;  assumes string is only one word
char* CHudTextMessage::LookupString( const char* message, int* destination )
{
	if ( !message )
		return "";
	if ( *message == '#' )
	{
		client_textmessage_t* text = TextMessageGet( message + 1 );
		if ( text && text->pMessage )
		{
			if ( destination && text->effect < 0 )
				*destination = -text->effect;
			return (char*)text->pMessage;
		}
	}
	return (char*)message;
}

void StripEndNewlineFromString( char* text )
{
	if ( text && *text )
	{
		int last = strlen( text ) - 1;
		if ( text[last] == '\r' || text[last] == '\n' )
			text[last] = 0;
	}
}

// converts all '\r' characters to '\n', so that the engine can deal with the properly
// returns a pointer to str
char* ConvertCRtoNL( char* text )
{
	for ( char* scan = text; *scan; scan++ )
	{
		if ( *scan == '\r' )
			*scan = '\n';
	}
	return text;
}

int CHudTextMessage::MsgFunc_TextMsg( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int destination = READ_BYTE();
	int client = -1;
	if ( destination == 5 )
	{
		const char* index = READ_STRING();
		if ( index )
			client = atoi( index );
	}
	static char szMsg[1024];
	strncpy( szMsg, READ_STRING(), sizeof( szMsg ) );
	szMsg[sizeof( szMsg ) - 1] = 0;
	int arguments = 0;
	for ( char* scan = szMsg; *scan; scan++ )
	{
		if ( *scan != '%' )
			continue;
		if ( scan[1] == '%' )
			scan++;
		else if ( arguments < 4 && scan[1] == 's' )
		{
			arguments++;
			scan++;
		}
		else
			*scan = ' ';
	}
	static char szBuf[6][128];
	safe_strcpy( szBuf[0], LookupString( szMsg, &destination ), sizeof( szBuf[0] ) );
	for ( int i = 1; i <= 4; i++ )
	{
		safe_strcpy( szBuf[i], LookupString( READ_STRING() ), sizeof( szBuf[i] ) );
		StripEndNewlineFromString( szBuf[i] );
	}
	if ( gViewPortInterface && !gViewPortInterface->AllowedToPrintText() )
		return 1;
	char* output = szBuf[5];
	switch ( destination )
	{
	case HUD_PRINTNOTIFY:
		output[0] = 1;
		safe_sprintf( output + 1, 127, szBuf[0], szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		ConsolePrint( ConvertCRtoNL( output ) );
		break;
	case HUD_PRINTCONSOLE:
		safe_sprintf( output, 128, szBuf[0], szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		ConsolePrint( ConvertCRtoNL( output ) );
		break;
	case HUD_PRINTTALK:
	case 5:
		gHUD.m_SayText.SayTextPrint( szBuf[0], 128, destination == 5 ? client : -1,
			szBuf[1], szBuf[2], szBuf[3], szBuf[4] );
		break;
	case HUD_PRINTCENTER:
		StripEndNewlineFromString( szBuf[0] );
		gHUD.m_VGUI2Print.VGUI2HudPrintArgs( szBuf[0], szBuf[1], szBuf[2], szBuf[3], szBuf[4],
			-1, (int)( ScreenHeight * 0.35 ), 1, 0.706, 0.118 );
		break;
	}
	return 1;
}
