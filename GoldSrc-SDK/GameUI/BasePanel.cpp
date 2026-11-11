//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BasePanel.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"
#include "Taskbar.h"

#include "FileSystem.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#include <strings.h>
#define stricmp strcasecmp
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>

CBasePanel::CBasePanel()
	: vgui2::Panel( NULL, "BasePanel" )
{
	m_eBackgroundState = BACKGROUND_NONE;
	m_iBaseResX = 0;
	m_iBaseResY = 0;
}

CBasePanel::~CBasePanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: sets how the game background should render
//-----------------------------------------------------------------------------
void CBasePanel::SetBackgroundRenderState( EBackgroundState state )
{
	m_eBackgroundState = state;
}

//-----------------------------------------------------------------------------
// Purpose: Notifies the task bar that a new top-level panel has been created
//-----------------------------------------------------------------------------
void CBasePanel::OnChildAdded( vgui2::VPANEL child )
{
	if ( g_pTaskbar )
		g_pTaskbar->AddTask( child );
}

vgui2::VPANEL CBasePanel::IsWithinTraverse( int x, int y, bool )
{
	if ( !IsVisible() )
		return 0;

	return vgui2::Panel::IsWithinTraverse( x, y, false );
}

//-----------------------------------------------------------------------------
// Purpose: paints the main background image
//-----------------------------------------------------------------------------
void CBasePanel::PaintBackground()
{
	const char* level = engine->pfnGetLevelName();
	int alpha;
	if ( level && level[0] && m_eBackgroundState != BACKGROUND_CAREERLOAD )
	{
		alpha = 128;
	}
	else
	{
		switch ( m_eBackgroundState )
		{
		case BACKGROUND_DESKTOPIMAGE:
		case BACKGROUND_LOADING:
			DrawBackgroundImage();
			return;

		case BACKGROUND_BLACK:
		case BACKGROUND_CAREERLOAD:
			alpha = 255;
			break;

		default:
			return;
		}
	}

	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	vgui2::surface()->DrawSetColor( 0, 0, 0, alpha );
	vgui2::surface()->DrawFilledRect( 0, 0, wide, tall );
}

void CBasePanel::DrawBackgroundImage()
{
	if ( m_iBaseResX <= 0 || m_iBaseResY <= 0 )
		return;

//	int xpos, ypos;
	int wide, tall;
	GetSize( wide, tall );
	int screenWide, screenTall;
	vgui2::surface()->GetScreenSize( screenWide, screenTall );

	float xScale = (float)screenWide / m_iBaseResX;
	float yScale = (float)screenTall / m_iBaseResY;
	float fitScale = yScale;
	if ( screenWide > m_iBaseResX * yScale )
		fitScale = xScale;

	int fitOffsetX = screenWide / 2 - (int)( m_iBaseResX * fitScale * 0.5f );
	int fitOffsetY = screenTall / 2 - (int)( m_iBaseResY * fitScale * 0.5f );
	for ( int i = 0; i < m_ImageID.Count(); i++ )
	{
		bimage_t& image = m_ImageID[i];
		int x, y, right, bottom;
		if ( image.scaled )
		{
			x = image.x ? (int)ceil( image.x * xScale ) : 0;
			y = image.y ? (int)ceil( image.y * yScale ) : 0;
			right = (int)ceil( ( image.x + image.width ) * xScale );
			bottom = (int)ceil( ( image.y + image.height ) * yScale );
		}
		else if ( image.fit )
		{
			x = fitOffsetX + ( image.x ? (int)ceil( image.x * fitScale ) : 0 );
			y = fitOffsetY + ( image.y ? (int)ceil( image.y * fitScale ) : 0 );
			right = x + (int)ceil( image.width * fitScale );
			bottom = y + (int)ceil( image.height * fitScale );
		}
		else
		{
			x = image.x;
			y = image.y;
			right = x + image.width;
			bottom = y + image.height;
		}

		vgui2::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui2::surface()->DrawSetTexture( image.imageID );
		vgui2::surface()->DrawTexturedRect( x, y, right, bottom );
	}
/*	xpos += bimage.width;
		}
		ypos += m_ImageID[y][0].height;
	}
	*/
}

void CBasePanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Panel::ApplySchemeSettings( scheme );
	const char* layoutFile = ModInfo()->BHasHDBackground() ?
		"resource/HD_BackgroundLayout.txt" : "resource/BackgroundLayout.txt";
	FileHandle_t file = vgui2::filesystem()->Open( layoutFile, "rt" );
	if ( !file )
		return;

	int size = vgui2::filesystem()->Size( file );
#ifdef _MSC_VER
#pragma warning( suppress: 6255 )
#endif
	char* buffer = (char*)alloca( size + 1 );
	vgui2::filesystem()->Read( buffer, size, file );
	vgui2::filesystem()->Close( file );
	buffer[size] = 0;

	char token[512];
	char* cursor = buffer;
	while ( cursor && cursor[0] )
	{
		cursor = vgui2::filesystem()->ParseFile( cursor, token, NULL );
		if ( !cursor || !token[0] )
			break;

		if ( !stricmp( token, "resolution" ) )
		{
			cursor = vgui2::filesystem()->ParseFile( cursor, token, NULL );
			m_iBaseResX = atoi( token );
			cursor = vgui2::filesystem()->ParseFile( cursor, token, NULL );
			m_iBaseResY = atoi( token );
		}
		else
		{
			bimage_t& image = m_ImageID[m_ImageID.AddToTail()];
			image.imageID = vgui2::surface()->CreateNewTextureID();
			char* extension = strstr( token, ".tga" );
			if ( extension )
				*extension = 0;
			vgui2::surface()->DrawSetTextureFile( image.imageID, token, 1, false );
			vgui2::surface()->DrawGetTextureSize( image.imageID, image.width, image.height );

			cursor = vgui2::filesystem()->ParseFile( cursor, token, NULL );
			image.scaled = !stricmp( token, "scaled" );
			image.fit = !stricmp( token, "fit" );
			cursor = vgui2::filesystem()->ParseFile( cursor, token, NULL );
			image.x = atoi( token );
			cursor = vgui2::filesystem()->ParseFile( cursor, token, NULL );
			image.y = atoi( token );
		}
	}
}
