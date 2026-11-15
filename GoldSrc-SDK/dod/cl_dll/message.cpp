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
#include "vgui/idodviewport.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "../../engine/controls.h"
#include "../../vgui2/controls/controls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

DECLARE_MESSAGE( m_Message, HudText );
DECLARE_MESSAGE( m_Message, GameTitle );

unsigned int _engineFont;
client_textmessage_t g_pCustomMessage;
char* g_pCustomName = "Custom";
char g_pCustomText[1024];

int CHudMessage::Init()
{
	HOOK_MESSAGE( HudText );
	HOOK_MESSAGE( GameTitle );
	gHUD.AddHudElem( this );
	m_Fonts[0] = m_Fonts[1] = m_Fonts[2] = 0;
	Reset();
	return 1;
}

int CHudMessage::VidInit()
{
	m_HUD_title_half = gHUD.GetSpriteIndex( "title_half" );
	m_HUD_title_life = gHUD.GetSpriteIndex( "title_life" );
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( vgui2::scheme()->GetDefaultScheme() );
	if ( !_engineFont )
		_engineFont = scheme->GetFont( "EngineFont", true );
	m_Fonts[0] = scheme->GetFont( "CreditsFont", true );
	m_Fonts[1] = scheme->GetFont( "Legacy_CreditsFont", true );
	m_Fonts[2] = scheme->GetFont( "EngineFont", true );
	return 1;
}

void CHudMessage::Reset()
{
	memset( m_pMessages, 0, sizeof( m_pMessages ) );
	memset( m_startTime, 0, sizeof( m_startTime ) );
	m_gameTitleTime = 0;
	m_pGameTitle = NULL;
}

unsigned int CHudMessage::GetFont()
{
	return m_Fonts[0];
}

float CHudMessage::FadeBlend( float fadein, float fadeout, float hold, float time )
{
	if ( time < 0 )
		return 0;
	if ( time < fadein )
		return 1 - (fadein - time) / fadein;
	if ( time > fadein + hold )
		return fadeout > 0 ? 1 - (time - (fadein + hold)) / fadeout : 0;
	return 1;
}

int CHudMessage::XPosition( float x, int width, int totalWidth )
{
	int position;
	if ( x == -1 )
		position = (ScreenWidth - width) / 2;
	else if ( x < 0 )
		position = (1.0 + x) * ScreenWidth - totalWidth;
	else
		position = x * ScreenWidth;
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
		position = (ScreenHeight - height) * 0.5;
	else if ( y < 0 )
		position = (1.0 + y) * ScreenHeight - height;
	else
		position = y * ScreenHeight;
	if ( position + height > ScreenHeight )
		position = ScreenHeight - height;
	else if ( position < 0 )
		position = 0;
	return position;
}

void CHudMessage::MessageScanNextChar( unsigned int font )
{
	client_textmessage_t* message = m_parms.pMessage;
	int red = message->r1;
	int green = message->g1;
	int blue = message->b1;
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
			red = green = blue = 0;
		else if ( m_parms.time > m_parms.fadeTime )
			blend = m_parms.fadeBlend;
		else
		{
			float delta = m_parms.time - m_parms.charTime;
			if ( delta <= message->fxtime )
			{
				destRed = message->r2;
				destGreen = message->g2;
				destBlue = message->b2;
				blend = 255 - (delta * (1.0 / message->fxtime) * 255.0 + 0.5);
			}
		}
		break;
	}
	blend = min( max( blend, 0 ), 255 );
	m_parms.r = (red * (255 - blend) + destRed * blend) >> 8;
	m_parms.g = (green * (255 - blend) + destGreen * blend) >> 8;
	m_parms.b = (blue * (255 - blend) + destBlue * blend) >> 8;
	if ( message->effect == 1 && m_parms.charTime != 0 && m_parms.x >= 0 && m_parms.y >= 0 )
		gEngfuncs.pfnVGUI2DrawCharacterAdd( m_parms.x, m_parms.y, m_parms.text,
			message->r2, message->g2, message->b2, font );
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
			m_parms.fadeBlend = (message->fadein - m_parms.time) * (1.0 / message->fadein) * 255;
		else if ( m_parms.time > m_parms.fadeTime )
			m_parms.fadeBlend = message->fadeout > 0 ? (m_parms.time - m_parms.fadeTime) / message->fadeout * 255 : 255;
		else
			m_parms.fadeBlend = 0;	// Pure source (on)
		m_parms.charTime = 0;
		if ( message->effect == 1 && rand() % 100 < 10 )
			m_parms.charTime = 1;
		break;
	case 2:
		m_parms.fadeTime = message->fadein * m_parms.length + message->holdtime;
		if ( m_parms.time > m_parms.fadeTime && message->fadeout > 0 )
			m_parms.fadeBlend = (m_parms.time - m_parms.fadeTime) / message->fadeout * 255;
		else
			m_parms.fadeBlend = 0;
		break;
	}
}

void CHudMessage::MessageDrawScan( client_textmessage_t* message, float time, unsigned int font )
{
	m_parms.lines = 1;
	m_parms.pMessage = message;
	m_parms.time = time;
	m_parms.totalWidth = 0;
	wchar_t textBuffer[1024];
	char token[80];
	wchar_t* text;
	if ( message->pMessage[0] == '#' )
	{
		snprintf( token, sizeof( token ), "%s", message->pMessage );
		int length = strlen( token ) - 1;
		if ( token[length] == '\r' )
			token[length] = 0;
		text = vgui2::localize()->Find( token );
		if ( !text )
		{
			gEngfuncs.Con_DPrintf( "ERROR: Missing %s from the dod_english.txt file!\n", message->pMessage );
			return;
		}
	}
	else
	{
		vgui2::localize()->ConvertANSIToUnicode( message->pMessage, textBuffer, sizeof( textBuffer ) );
		text = textBuffer;
	}

	int length = 0, width = 0;
	int a, b, c;
	for ( wchar_t* ch = text; *ch; ch++ )
	{
		if ( *ch == '\n' )
		{
			m_parms.lines++;
			m_parms.totalWidth = max( m_parms.totalWidth, width );
			width = 0;
		}
		else
		{
			vgui2::surface()->GetCharABCwide( font, *ch, a, b, c );
			width += a + b + c;
		}
		length++;
	}
	m_parms.length = length;
	m_parms.totalHeight = m_parms.lines * vgui2::surface()->GetFontTall( font );
	m_parms.y = YPosition( message->y, m_parms.totalHeight );
	if ( message->pMessage[0] == '#' )
		text = vgui2::localize()->Find( token );
	else
	{
		vgui2::localize()->ConvertANSIToUnicode( message->pMessage, textBuffer, sizeof( textBuffer ) );
		text = textBuffer;
	}
	m_parms.charTime = 0;
	MessageScanStart();
	for ( int i = 0; i < m_parms.lines; i++ )
	{
		wchar_t line[80];
		m_parms.lineLength = 0;
		m_parms.width = 0;
		while ( *text && *text != '\n' && m_parms.lineLength < (int)sizeof( line ) )
		{
			line[m_parms.lineLength++] = *text;
			vgui2::surface()->GetCharABCwide( font, *text++, a, b, c );
			m_parms.width += a + b + c;
		}
		text++;
		line[m_parms.lineLength] = 0;
		m_parms.x = XPosition( message->x, m_parms.width, m_parms.totalWidth );
		for ( int j = 0; j < m_parms.lineLength; j++ )
		{
			m_parms.text = line[j];
			vgui2::surface()->GetCharABCwide( font, m_parms.text, a, b, c );
			int next = m_parms.x + a + b + c;
			MessageScanNextChar( font );
			if ( m_parms.x >= 0 && m_parms.y >= 0 && next <= ScreenWidth )
				gEngfuncs.pfnVGUI2DrawCharacterAdd( m_parms.x, m_parms.y, m_parms.text,
					m_parms.r, m_parms.g, m_parms.b, font );
			m_parms.x = next;
		}
		m_parms.y += vgui2::surface()->GetFontTall( font );
	}
}

int CHudMessage::Draw( float time )
{
	int drawn = 0;
	float endTime = NAN;
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
			int width = halfWidth + life.right - life.left;
			int x = XPosition( m_pGameTitle->x, width, width );
			int y = YPosition( m_pGameTitle->y, half.bottom - half.top );
			SPR_Set( gHUD.GetSprite( m_HUD_title_half ), brightness * m_pGameTitle->r1,
				brightness * m_pGameTitle->g1, brightness * m_pGameTitle->b1 );
			SPR_DrawAdditive( 0, x, y, &half );
			SPR_Set( gHUD.GetSprite( m_HUD_title_life ), brightness * m_pGameTitle->r1,
				brightness * m_pGameTitle->g1, brightness * m_pGameTitle->b1 );
			SPR_DrawAdditive( 0, x + halfWidth, y, &life );
			drawn = 1;
		}
	}
	// Fixup level transitions
	for ( int i = 0; i < maxHUDMessages; i++ )
	{
		if ( m_pMessages[i].pMessage && (m_startTime[i] > gHUD.m_flTime || m_startTime[i] == 1) )
			m_startTime[i] = gHUD.m_flTime + m_parms.time - m_startTime[i] + 0.2f;
	}
	for ( int i = 0; i < maxHUDMessages; i++ )
	{
		client_textmessage_t* message = m_pMessages[i].pMessage;
		if ( !message )
			continue;
		switch ( message->effect )
		{
		case 0:
		case 1:
			endTime = m_startTime[i] + message->fadein + message->fadeout + message->holdtime;
			break;
		case 2:
			endTime = m_startTime[i] + message->fadein * strlen( message->pMessage ) + message->fadeout + message->holdtime;
			break;
		}
		if ( time <= endTime )
		{
			MessageDrawScan( message, time - m_startTime[i], m_pMessages[i].font );
			drawn++;
		}
		else
		{
			m_pMessages[i].pMessage = NULL;
			m_pMessages[i].font = 0;
		}
	}

	// Remember the time -- to fix up level transitions
	m_parms.time = gHUD.m_flTime;
	// Don't call until we get another message
	if ( !drawn )
		m_iFlags &= ~HUD_ACTIVE;
	return 1;
}

void CHudMessage::MessageAdd( const char* name, float time, int hint, unsigned int font )
{
	for ( int i = 0; i < maxHUDMessages; i++ )
	{
		if ( m_pMessages[i].pMessage )
			continue;
		client_textmessage_t* message = TextMessageGet( name[0] == '#' ? name + 1 : name );
		if ( name[0] == '#' && message && hint )
		{
			message->effect = 2;
			message->r1 = message->b1 = 40;
			message->g1 = 255;
			message->a1 = 200;
			message->r2 = message->b2 = 0;
			message->g2 = 255;
			message->a2 = 200;
			message->x = -1;
			message->y = 0.7f;
			message->fadein = 0.01f;
			message->fadeout = 0.7f;
			message->fxtime = 0.07f;
			message->holdtime = max( (float)(strlen( message->pMessage ) / 25), 1.0f );
		}
		m_pMessages[i].pMessage = message;
		m_pMessages[i].font = font;
		m_startTime[i] = time;
		return;
	}
}

void CHudMessage::MessageAdd( client_textmessage_t* message )
{
	m_parms.time = gHUD.m_flTime;
	m_iFlags |= HUD_ACTIVE;
	for ( int i = 0; i < maxHUDMessages; i++ )
	{
		if ( !m_pMessages[i].pMessage )
		{
			m_pMessages[i].pMessage = message;
			m_pMessages[i].font = m_Fonts[0];
			m_startTime[i] = gHUD.m_flTime;
			return;
		}
	}
}

int CHudMessage::MsgFunc_HudText( const char*, int size, void* data )
{
	BEGIN_READ( data, size );
	const char* text = READ_STRING();
	int hint = READ_BYTE();
	if ( hint > 0 && gDoDViewPortInterface )
		gDoDViewPortInterface->ShowHintMessage( text );
	else
		MessageAdd( text, gHUD.m_flTime, hint, m_Fonts[0] );
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

void CHudMessage::HintMessageAdd( const char* text )
{
	if ( gDoDViewPortInterface )
		gDoDViewPortInterface->ShowHintMessage( text );
	m_parms.time = gHUD.m_flTime;
	m_iFlags |= HUD_ACTIVE;
}
