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
// hud_redraw.cpp
//

#include "hud.h"
#include "cl_util.h"
#include "../../cl_dll/bench.h"
#include "../public/game_controls/legacy_iviewport.h"
#include "z_weather.h"

extern cvar_t* sensitivity;
void LoadLevelMaterials();

#define MAX_LOGO_FRAMES 56

int grgLogoFrame[MAX_LOGO_FRAMES] =
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31
};

Queue g_ShellQueue;
Queue g_SmokePuffQueue;

Queue::~Queue()
{
	Update( 99999 );
}

void CHud::Think()
{
	for ( HUDLIST* element = m_pHudList; element; element = element->pNext )
	{
		if ( element->p->m_iFlags & HUD_ACTIVE )
			element->p->Think();
	}
	int fov = (int)HUD_GetFOV();
	m_iFOV = fov ? fov : (int)default_fov->value;

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == default_fov->value )
		m_flMouseSensitivity = 0;
	else
	{
		float scale = fov / default_fov->value * sensitivity->value;
		m_flMouseSensitivity = CVAR_GET_FLOAT( "zoom_sensitivity_ratio" ) * scale;
	}
	if ( !m_iFOV )
	{
		m_iFOV = (int)default_fov->value;
		if ( m_iFOV < 90 )
			m_iFOV = 90;
	}
	if ( m_flMaterialCheckTime && m_flTime >= m_flMaterialCheckTime && *gEngfuncs.pfnGetLevelName() )
	{
		if ( !strcmp( gEngfuncs.pfnGetLevelName(), m_szLastBSP ) )
			m_flMaterialCheckTime = (float)( m_flTime + 0.1 );
		else
		{
			strcpy( m_szLastBSP, gEngfuncs.pfnGetLevelName() );
			LoadLevelMaterials();
			g_Environment.SetupGrass();
		}
		if ( !strcmp( gEngfuncs.pfnGetLevelName(), m_szLastBSP ) )
			m_flMaterialCheckTime = 0;
	}
	Bench_CheckStart();
}

bool CHud::DrawHudNightVision( float time )
{
	bool on = m_NightVision.IsOn();
	if ( on )
	{
		if ( !m_hsprLogo )
			m_hsprLogo = LoadSprite( "sprites/nvgreen-add.spr" );
		SPR_Set( m_hsprLogo, 100, 100, 100 );
		float frame = time * 20 + 0.0f;
		gEngfuncs.pfnSPR_DrawGeneric( (int)frame % 16, 0, 0, NULL, 1, 1, ScreenWidth, ScreenHeight );
	}
	return on;
}

bool CHud::DrawHudSniperLaser( float )
{
	return m_SniperLaser.IsOn();
}

bool CHud::DrawHudFiberCamera( float time )
{
	bool on = m_FiberCamera.IsOn();
	if ( on )
	{
		if ( !m_hsprLogo )
			m_hsprLogo = LoadSprite( "sprites/fo-noise.spr" );
		SPR_Set( m_hsprLogo, 255, 0, 0 );
		float frame = time * 20 + 0.0f;
		gEngfuncs.pfnSPR_DrawGeneric( (int)frame % 16, 0, 0, NULL, 1, 1, ScreenWidth, ScreenHeight );
	}
	return on;
}

bool CHud::DrawHudSecurityCamera( float time )
{
	if ( !m_securityCamera )
		return false;
	if ( !m_hsprLogo )
		m_hsprLogo = LoadSprite( "sprites/sc-noise.spr" );
	SPR_Set( m_hsprLogo, 255, 0, 0 );
	float frame = time * 20 + 0.0f;
	gEngfuncs.pfnSPR_DrawGeneric( (int)frame % 16, 0, 0, NULL, 1, 1, ScreenWidth, ScreenHeight );
	return true;
}

void CHud::DrawHudParticles( float time )
{
	m_Particles.Draw( time );
}

bool CHud::DrawHudScoreboardAndTakeScreenshots( float time, int intermission )
{
	static float m_flShotTime;
	if ( gViewPortInterface )
	{
		if ( m_iIntermission && !intermission )
		{
			m_iIntermission = 0;
			gViewPortInterface->HideCommandMenu();
			gViewPortInterface->HideScoreBoard();
			gViewPortInterface->UpdateSpectatorPanel();
		}
		else if ( !m_iIntermission && intermission )
		{
			m_iIntermission = intermission;
			gViewPortInterface->HideCommandMenu();
			gViewPortInterface->HideAllVGUIMenu();
			gViewPortInterface->ShowScoreBoard();
			gViewPortInterface->UpdateSpectatorPanel();
			if ( CVAR_GET_FLOAT( "hud_takesshots" ) )
				m_flShotTime = (float)( time + 1.0 );
		}
	}
	if ( m_flShotTime && time > m_flShotTime )
	{
		EngineClientCmd( "snapshot\n" );
		m_flShotTime = 0;
	}
	m_iIntermission = intermission;
	return true;
}

bool CHud::DrawHudElements( float time, int intermission )
{
	if ( !m_pCvarDraw->value )
		return false;
	for ( HUDLIST* element = m_pHudList; element; element = element->pNext )
	{
		if ( Bench_Active() )
		{
			if ( element->p == &m_Benchmark && ( m_Benchmark.m_iFlags & HUD_ACTIVE )
				&& !( m_iHideHUDDisplay & HIDEHUD_ALL ) )
				m_Benchmark.Draw( time );
		}
		else if ( intermission )
		{
			if ( element->p->m_iFlags & HUD_INTERMISSION )
				element->p->Draw( time );
		}
		else if ( ( element->p->m_iFlags & HUD_ACTIVE ) && !( m_iHideHUDDisplay & HIDEHUD_ALL ) )
			element->p->Draw( time );
	}
	return true;
}

bool CHud::DrawHudDemoLogo( float time )
{
	if ( !m_iLogo )
		return false;
	if ( !m_hsprLogo )
		m_hsprLogo = LoadSprite( "sprites/%d_logo.spr" );
	SPR_Set( m_hsprLogo, 250, 250, 250 );
	int x = ScreenWidth - SPR_Width( m_hsprLogo, 0 );
	int y = SPR_Height( m_hsprLogo, 0 ) / 2;
	float frame = time * 20;
	SPR_DrawAdditive( grgLogoFrame[(int)frame % MAX_LOGO_FRAMES] - 1, x, y, NULL );
	return true;
}

int CHud::Redraw( float time, int intermission )
{
	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = time;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;

	// Clock was reset, reset delta
	if ( m_flTimeDelta < 0 )
		m_flTimeDelta = 0;
	DrawHudParticles( time );
	DrawHudScoreboardAndTakeScreenshots( time, intermission );
	DrawHudNightVision( time );
	DrawHudFiberCamera( time );
	DrawHudSecurityCamera( time );
	DrawHudElements( time, intermission );
	DrawHudDemoLogo( time );

	/*
	if ( g_iVisibleMouse )
	{
		void IN_GetMousePos( int *mx, int *my );
		int mx, my;

		IN_GetMousePos( &mx, &my );
		
		if (m_hsprCursor == 0)
		{
			char sz[256];
			sprintf( sz, "sprites/cursor.spr" );
			m_hsprCursor = SPR_Load( sz );
		}

		SPR_Set(m_hsprCursor, 250, 250, 250 );
		
		// Draw the logo at 20 fps
		SPR_DrawAdditive( 0, mx, my, NULL );
	}
	*/

	return 1;
}

void ScaleColors( int& r, int& g, int& b, int a )
{
	float scale = (float)a / 255;
	r = (int)( r * scale );
	g = (int)( g * scale );
	b = (int)( b * scale );
}

int CHud::DrawHudString( int x, int y, int maxX, char* text, int r, int g, int b )
{
	for ( ; *text && *text != '\n'; ++text )
	{
		int next = x + m_scrinfo.charWidths[(int)*text];
		if ( next > maxX )
			return x;
		TextMessageDrawChar( x, y, *text, r, g, b );
		x = next;
	}
	return x;
}

int CHud::DrawHudStringReverse( int x, int y, int minX, char* text, int r, int g, int b )
{
	char* end = text + strlen( text );
	while ( end != text )
	{
		--end;
		int next = x - m_scrinfo.charWidths[(int)*end];
		if ( next < minX )
			return x;
		x = next;
		TextMessageDrawChar( x, y, *end, r, g, b );
	}
	return x;
}

int CHud::DrawHudNumberString( int x, int y, int minX, int number, int r, int g, int b )
{
	char text[32];
	sprintf( text, "%d", number );
	return DrawHudStringReverse( x, y, minX, text, r, g, b );
}

int CHud::DrawHudNumber( int x, int y, int flags, int number, int r, int g, int b )
{
	int width = GetSpriteRect( m_HUD_number_0 ).right - GetSpriteRect( m_HUD_number_0 ).left;
	if ( number > 0 )
	{
		// SPR_Draw 100's
		if ( number >= 100 )
		{
			int index = m_HUD_number_0 + number / 100;
			SPR_Set( GetSprite( index ), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect( index ) );
			x += width;
		}
		else if ( flags & DHN_3DIGITS )
			x += width;
		if ( number >= 10 )
		{
			int index = m_HUD_number_0 + number % 100 / 10;
			SPR_Set( GetSprite( index ), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect( index ) );
			x += width;
		}
		else if ( flags & ( DHN_2DIGITS | DHN_3DIGITS ) )
			x += width;
		int index = m_HUD_number_0 + number % 10;
		SPR_Set( GetSprite( index ), r, g, b );
		SPR_DrawAdditive( 0, x, y, &GetSpriteRect( index ) );
		x += width;
	}
	else if ( flags & DHN_DRAWZERO )
	{
		SPR_Set( GetSprite( m_HUD_number_0 ), r, g, b );
		if ( flags & DHN_3DIGITS )
			x += width;
		if ( flags & ( DHN_2DIGITS | DHN_3DIGITS ) )
			x += width;

		// SPR_Draw ones

		SPR_DrawAdditive( 0, x, y, &GetSpriteRect( m_HUD_number_0 ) );
		x += width;
	}
	return x;
}

int CHud::GetNumWidth( int number, int flags )
{
	if ( flags & DHN_3DIGITS )
		return 3;
	if ( flags & DHN_2DIGITS )
		return 2;
	if ( number <= 0 )
		return flags & DHN_DRAWZERO;
	if ( number < 10 )
		return 1;
	if ( number < 100 )
		return 2;
	return 3;
}
