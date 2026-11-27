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
// Message.cpp
//
// implementation of CHudMessage class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "../../vgui2/controls/controls.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include <wchar.h>

namespace vgui2
{
ILocalize* localize();
}

DECLARE_MESSAGE( m_Message, HudText )
DECLARE_MESSAGE( m_Message, GameTitle )

vgui2::HFont _font;
client_textmessage_t g_pCustomMessage;
char* g_pCustomName = (char*)"Custom";
char g_pCustomText[1024];

int CHudMessage::Init()
{
	HOOK_MESSAGE( HudText );
	HOOK_MESSAGE( GameTitle );
	gHUD.AddHudElem( this );
	Reset();
	return 1;
}

int CHudMessage::VidInit()
{
	m_HUD_title_half = gHUD.GetSpriteIndex( "title_half" );
	m_HUD_title_life = gHUD.GetSpriteIndex( "title_life" );
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( vgui2::scheme()->GetDefaultScheme() );
	if ( !_font )
		_font = scheme->GetFont( "CreditsFont", true );
	return 1;
}

void CHudMessage::Reset()
{
	memset( m_pMessages, 0, sizeof( m_pMessages ) );
	memset( m_startTime, 0, sizeof( m_startTime ) );
	m_gameTitleTime = 0;
	m_pGameTitle = NULL;
}

float CHudMessage::FadeBlend( float fadein, float fadeout, float hold, float localTime )
{
	if ( localTime < 0 )
		return 0;
	if ( localTime < fadein )
		return 1 - ( fadein - localTime ) / fadein;
	float fadeTime = fadein + hold;
	if ( localTime <= fadeTime )
		return 1;
	if ( fadeout > 0 )
		return 1 - ( localTime - fadeTime ) / fadeout;
	return 0;
}

int CHudMessage::XPosition( float x, int width, int totalWidth )
{
	int position;
	if ( x == -1 )
		position = ( ScreenWidth - width ) / 2;
	else if ( x < 0 )
		position = (int)( ( 1.0 + x ) * ScreenWidth - totalWidth );
	else
		position = (int)( x * ScreenWidth );
	if ( position + width > ScreenWidth )
		position = ScreenWidth - width;
	else if ( position < 0 )
		position = 0;
	return position;
}

int CHudMessage::YPosition( float y, int height )
{
	int position;
	if ( y == -1 )
		position = (int)( ( ScreenHeight - height ) * 0.5 );
	else if ( y < 0 )
		position = (int)( ( 1.0 + y ) * ScreenHeight - height );
	else
		position = (int)( y * ScreenHeight );
	if ( position + height > ScreenHeight )
		position = ScreenHeight - height;
	else if ( position < 0 )
		position = 0;
	return position;
}

void CHudMessage::MessageScanNextChar()
{
	client_textmessage_t* message = m_parms.pMessage;
	int srcRed = message->r1, srcGreen = message->g1, srcBlue = message->b1;
	int destRed = 0, destGreen = 0, destBlue = 0;
	int blend = 0;
	switch ( message->effect )
	{
	// Fade-in / Fade-out
	case 0:
	case 1:
		blend = m_parms.fadeBlend;
		break;
	case 2:
		m_parms.charTime += message->fadein;
		if ( m_parms.charTime > m_parms.time )
			srcRed = srcGreen = srcBlue = 0;
		else
		{
			float deltaTime = m_parms.time - m_parms.charTime;
			if ( m_parms.time > m_parms.fadeTime )
				blend = m_parms.fadeBlend;
			else if ( deltaTime <= message->fxtime )
			{
				destRed = message->r2;
				destGreen = message->g2;
				destBlue = message->b2;
				blend = (int)( 255 - ( deltaTime * ( 1.0 / message->fxtime ) * 255.0 + 0.5 ) );
			}
		}
		break;
	}
	if ( blend > 255 )
		blend = 255;
	else if ( blend < 0 )
		blend = 0;
	m_parms.r = ( srcRed * ( 255 - blend ) + destRed * blend ) >> 8;
	m_parms.g = ( srcGreen * ( 255 - blend ) + destGreen * blend ) >> 8;
	m_parms.b = ( srcBlue * ( 255 - blend ) + destBlue * blend ) >> 8;
	if ( message->effect == 1 && m_parms.charTime != 0 && m_parms.x >= 0 && m_parms.y >= 0 )
		gEngfuncs.pfnVGUI2DrawCharacterAdd( m_parms.x, m_parms.y, m_parms.text,
			message->r2, message->g2, message->b2, _font );
}

void CHudMessage::MessageScanStart()
{
	client_textmessage_t* message = m_parms.pMessage;
	switch ( message->effect )
	{
	case 0:
	case 1:
		m_parms.fadeTime = message->fadein + message->holdtime;
		if ( m_parms.time < message->fadein )
			m_parms.fadeBlend = (int)( ( message->fadein - m_parms.time ) * ( 1.0 / message->fadein ) * 255 );
		else if ( m_parms.time > m_parms.fadeTime )
		{
			if ( message->fadeout > 0 )
				m_parms.fadeBlend = (int)( ( m_parms.time - m_parms.fadeTime ) / message->fadeout * 255 );
			else
				m_parms.fadeBlend = 255; // Pure dest (off)
		}
		else
			m_parms.fadeBlend = 0;	// Pure source (on)
		m_parms.charTime = 0;
		if ( message->effect == 1 && rand() % 100 < 10 )
			m_parms.charTime = 1;
		break;
	case 2:
		m_parms.fadeTime = message->fadein * m_parms.length + message->holdtime;
		if ( m_parms.time > m_parms.fadeTime && message->fadeout > 0 )
			m_parms.fadeBlend = (int)( ( m_parms.time - m_parms.fadeTime ) / message->fadeout * 255 );
		else
			m_parms.fadeBlend = 0;
		break;
	}
}

void CHudMessage::MessageDrawScan( client_textmessage_t* message, float time )
{
	wchar_t textBuffer[1024];
	char token[256];
	const wchar_t* text;
	size_t length = strlen( message->pMessage );
	// Count lines
	m_parms.lines = 1;
	m_parms.time = time;
	m_parms.pMessage = message;
	m_parms.totalWidth = 0;
	if ( message->pMessage[0] == '#' )
	{
		strncpy( token, message->pMessage, sizeof( token ) - 1 );
		token[sizeof( token ) - 1] = 0;
		if ( ( message->pMessage[length - 1] == '\r' || message->pMessage[length - 1] == '\n' )
			&& length - 1 < sizeof( token ) - 1 )
			token[length - 1] = 0;
		text = vgui2::localize()->Find( token );
		if ( !text )
		{
			gEngfuncs.Con_DPrintf( "ERROR: Missing %s from the czero_english.txt file!\n", message->pMessage );
			return;
		}
	}
	else
	{
		vgui2::localize()->ConvertANSIToUnicode( message->pMessage, textBuffer, sizeof( textBuffer ) );
		text = textBuffer;
	}
	int width = 0;
	int characterCount = 0;
	for ( const wchar_t* current = text; *current; ++current, ++characterCount )
	{
		if ( *current == '\n' )
		{
			++m_parms.lines;
			if ( width > m_parms.totalWidth )
				m_parms.totalWidth = width;
			width = 0;
		}
		else
		{
			int a, b, c;
			vgui2::surface()->GetCharABCwide( _font, *current, a, b, c );
			width += a + b + c;
		}
	}
	m_parms.length = characterCount;
	m_parms.totalHeight = m_parms.lines * vgui2::surface()->GetFontTall( _font );
	m_parms.y = YPosition( message->y, m_parms.totalHeight );
	if ( message->pMessage[0] == '#' )
	{
		text = vgui2::localize()->Find( token );
		float holdtime = (float)wcslen( text ) * 0.05f + 1;
		if ( holdtime > message->holdtime )
			message->holdtime = holdtime;
	}
	else
	{
		vgui2::localize()->ConvertANSIToUnicode( message->pMessage, textBuffer, sizeof( textBuffer ) );
		text = textBuffer;
	}
	m_parms.charTime = 0;
	MessageScanStart();
	for ( int i = 0; i < m_parms.lines; ++i )
	{
		wchar_t line[256];
		m_parms.lineLength = 0;
		m_parms.width = 0;
		while ( *text && *text != '\n' && m_parms.lineLength < 255 )
		{
			int a, b, c;
			line[m_parms.lineLength] = *text;
			vgui2::surface()->GetCharABCwide( _font, *text, a, b, c );
			m_parms.width += a + b + c;
			++m_parms.lineLength;
			++text;
		}
		++text;
		line[m_parms.lineLength] = 0;
		m_parms.x = XPosition( message->x, m_parms.width, m_parms.totalWidth );
		for ( int j = 0; j < m_parms.lineLength; ++j )
		{
			int a, b, c;
			m_parms.text = line[j];
			vgui2::surface()->GetCharABCwide( _font, m_parms.text, a, b, c );
			int next = m_parms.x + a + b + c;
			MessageScanNextChar();
			if ( m_parms.x >= 0 && m_parms.y >= 0 && next <= ScreenWidth )
				gEngfuncs.pfnVGUI2DrawCharacterAdd( m_parms.x, m_parms.y, m_parms.text,
					m_parms.r, m_parms.g, m_parms.b, _font );
			m_parms.x = next;
		}
		m_parms.y += vgui2::surface()->GetFontTall( _font );
	}
}

int CHudMessage::Draw( float time )
{
	int drawn = 0;
	float endTime = 0;
	if ( m_gameTitleTime > 0 )
	{
		float localTime = gHUD.m_flTime - m_gameTitleTime;

		// Maybe timer isn't set yet
		if ( m_gameTitleTime > gHUD.m_flTime )
			m_gameTitleTime = gHUD.m_flTime;
		if ( localTime > m_pGameTitle->fadein + m_pGameTitle->holdtime + m_pGameTitle->fadeout )
			m_gameTitleTime = 0;
		else
		{
			float brightness = FadeBlend( m_pGameTitle->fadein, m_pGameTitle->fadeout,
				m_pGameTitle->holdtime, localTime );
			wrect_t& half = gHUD.GetSpriteRect( m_HUD_title_half );
			wrect_t& life = gHUD.GetSpriteRect( m_HUD_title_life );
			int halfWidth = half.right - half.left;
			int fullWidth = halfWidth + life.right - life.left;
			int fullHeight = half.bottom - half.top;
			int x = XPosition( m_pGameTitle->x, fullWidth, fullWidth );
			int y = YPosition( m_pGameTitle->y, fullHeight );
			SPR_Set( gHUD.GetSprite( m_HUD_title_half ), (int)( brightness * m_pGameTitle->r1 ),
				(int)( brightness * m_pGameTitle->g1 ), (int)( brightness * m_pGameTitle->b1 ) );
			SPR_DrawAdditive( 0, x, y, &half );
			SPR_Set( gHUD.GetSprite( m_HUD_title_life ), (int)( brightness * m_pGameTitle->r1 ),
				(int)( brightness * m_pGameTitle->g1 ), (int)( brightness * m_pGameTitle->b1 ) );
			SPR_DrawAdditive( 0, x + halfWidth, y, &life );
			drawn = 1;
		}
	}
	// Fixup level transitions
	for ( int i = 0; i < MAX_HUD_MESSAGES; ++i )
	{
		if ( m_pMessages[i] && m_startTime[i] > gHUD.m_flTime )
			m_startTime[i] = (float)( gHUD.m_flTime + m_parms.time - m_startTime[i] + 0.2 );
	}
	for ( int i = 0; i < MAX_HUD_MESSAGES; ++i )
	{
		client_textmessage_t* message = m_pMessages[i];
		if ( !message )
			continue;
		switch ( message->effect )
		{
		case 0:
		case 1:
			endTime = m_startTime[i] + message->fadein + message->fadeout + message->holdtime;
			break;
		case 2:
			endTime = m_startTime[i] + message->fadein * m_parms.length + message->fadeout + message->holdtime;
			break;
		}
		if ( time <= endTime )
		{
			MessageDrawScan( message, time - m_startTime[i] );
			++drawn;
		}
		else
			m_pMessages[i] = NULL;
	}

	// Remember the time -- to fix up level transitions
	m_parms.time = gHUD.m_flTime;
	// Don't call until we get another message
	if ( !drawn )
		m_iFlags &= ~HUD_ACTIVE;
	return 1;
}

void CHudMessage::MessageAdd( const char* name, float time )
{
	for ( int i = 0; i < MAX_HUD_MESSAGES; ++i )
	{
		if ( m_pMessages[i] )
			continue;
		client_textmessage_t* message = TextMessageGet( name[0] == '#' ? name + 1 : name );
		if ( !message )
		{
			g_pCustomMessage.effect = 2;
			g_pCustomMessage.r1 = g_pCustomMessage.g1 = g_pCustomMessage.b1 = g_pCustomMessage.a1 = 100;
			g_pCustomMessage.r2 = 240;
			g_pCustomMessage.g2 = 110;
			g_pCustomMessage.b2 = g_pCustomMessage.a2 = 0;
			g_pCustomMessage.x = -1;		// Centered
			g_pCustomMessage.y = 0.7f;
			g_pCustomMessage.fadein = 0.01f;
			g_pCustomMessage.fadeout = 1.5f;
			g_pCustomMessage.fxtime = 0.25f;
			g_pCustomMessage.holdtime = 5;
			g_pCustomMessage.pName = g_pCustomName;
			strncpy( g_pCustomText, name, sizeof( g_pCustomText ) - 1 );
			g_pCustomText[ sizeof( g_pCustomText ) - 1 ] = '\0';
			g_pCustomMessage.pMessage = g_pCustomText;
			message = &g_pCustomMessage;
		}
		m_pMessages[i] = message;
		m_startTime[i] = time;
		return;
	}
}

int CHudMessage::MsgFunc_HudText( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	MessageAdd( READ_STRING(), gHUD.m_flTime );
	m_parms.time = gHUD.m_flTime;
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudMessage::MsgFunc_GameTitle( const char*, int, void* )
{
	m_pGameTitle = TextMessageGet( "GAMETITLE" );
	if ( m_pGameTitle )
	{
		m_gameTitleTime = gHUD.m_flTime;
		m_iFlags |= HUD_ACTIVE;
	}
	return 1;
}

void CHudMessage::MessageAdd( client_textmessage_t* message )
{
	m_parms.time = gHUD.m_flTime;
	m_iFlags |= HUD_ACTIVE;
	for ( int i = 0; i < MAX_HUD_MESSAGES; ++i )
	{
		if ( !m_pMessages[i] )
		{
			m_pMessages[i] = message;
			m_startTime[i] = gHUD.m_flTime;
			return;
		}
	}
}
