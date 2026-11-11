//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BitmapImagePanel.h"
#include "controls.h"
#include "vgui/IBorder.h"
#include "vgui/IImage.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vgui2
{
CBitmapImagePanel::CBitmapImagePanel( Panel* parent, const char* panelName, const char* filename )
	: Panel( parent, panelName )
{
	m_pImage = NULL;

	SetBounds( 0, 0, 100, 100 );

	m_pszImageName = NULL;
	m_pszColorName = NULL;
	m_hardwareFiltered = false;
	m_preserveAspectRatio = false;
	m_contentAlignment = Label::a_center;

	if ( filename && filename[0] )
	{
		m_pImage = scheme()->GetImage( filename, false );
		m_pszImageName = new char[strlen( filename ) + 1];
		strcpy( m_pszImageName, filename );
	}

	m_bgColor = Color( 255, 255, 255, 255 );
}

void CBitmapImagePanel::setTexture( const char* filename, bool hardwareFiltered )
{
	m_hardwareFiltered = hardwareFiltered;

	if ( m_pszImageName )
	{
		delete[] m_pszImageName;
		m_pszImageName = NULL;
	}

	if ( filename && filename[0] )
	{
		m_pImage = scheme()->GetImage( filename, m_hardwareFiltered );
		m_pszImageName = new char[strlen( filename ) + 1];
		strcpy( m_pszImageName, filename );
	}
	else
	{
		m_pImage = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBitmapImagePanel::SetContentAlignment( Label::Alignment alignment )
{
	m_contentAlignment = alignment;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBitmapImagePanel::ComputeImagePosition( int& x, int& y, int& w, int& h )
{
	if ( !m_pImage )
	{
		x = y = w = h = 0;
		return;
	}

	if ( !m_preserveAspectRatio )
	{
		x = y = 0;
		GetSize( w, h );
		return;
	}

	int panelWide, panelTall;
	GetSize( panelWide, panelTall );

	int imageWide, imageTall;
	m_pImage->GetSize( imageWide, imageTall );

	if ( panelWide <= 0 || panelTall <= 0 || imageWide <= 0 || imageTall <= 0 )
	{
		x = y = 0;
		w = panelWide;
		h = panelTall;
		return;
	}

	float xScale = (float)panelWide / (float)imageWide;
	float yScale = (float)panelTall / (float)imageTall;
	float scale = ( xScale > yScale ) ? yScale : xScale;

	w = (int)( imageWide * scale );
	h = (int)( imageTall * scale );

	switch ( m_contentAlignment )
	{
	case Label::a_north:
		x = ( panelWide - w ) / 2;
		y = 0;
		break;
	case Label::a_west:
		x = 0;
		y = ( panelTall - h ) / 2;
		break;
	case Label::a_center:
		x = ( panelWide - w ) / 2;
		y = ( panelTall - h ) / 2;
		break;
	case Label::a_southeast:
		x = panelWide - w;
		y = panelTall - h;
		break;
	case Label::a_northeast:
	case Label::a_southwest:
		x = panelWide - w;
		y = 0;
		break;
	case Label::a_east:
	case Label::a_south:
		x = panelWide - w;
		y = ( panelTall - h ) / 2;
		break;
	case Label::a_northwest:
	default:
		x = y = 0;
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBitmapImagePanel::PaintBackground()
{
	if ( !m_pImage )
		return;

	int x, y, w, h;
	ComputeImagePosition( x, y, w, h );

	m_pImage->SetPos( x, y );
	m_pImage->SetSize( w, h );
	m_pImage->SetColor( m_bgColor );
	surface()->DrawSetColor( m_bgColor );
	m_pImage->Paint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBitmapImagePanel::PaintBorder()
{
	int x, y, w, h;
	ComputeImagePosition( x, y, w, h );

	IBorder* border = GetBorder();
	if ( border )
		border->Paint( x, y, x + w, y + h, -1, 0, 0 );
}

void CBitmapImagePanel::GetSettings( KeyValues* outResourceData )
{
	Panel::GetSettings( outResourceData );

	if ( m_pszImageName )
		outResourceData->SetString( "image", m_pszImageName );
	if ( m_pszColorName )
		outResourceData->SetString( "imagecolor", m_pszColorName );

	const char* alignment;
	switch ( m_contentAlignment )
	{
	case Label::a_northwest:
		alignment = "north-west";
		break;
	case Label::a_north:
		alignment = "north";
		break;
	case Label::a_northeast:
		alignment = "north-east";
		break;
	case Label::a_east:
		alignment = "east";
		break;
	case Label::a_southwest:
		alignment = "south-west";
		break;
	case Label::a_south:
		alignment = "south";
		break;
	case Label::a_southeast:
		alignment = "south-east";
		break;
	case Label::a_center:
	default:
		alignment = "center";
		break;
	}

	outResourceData->SetString( "imageAlignment", alignment );
	outResourceData->SetInt( "preserveAspectRatio", m_preserveAspectRatio );
	outResourceData->SetInt( "filtered", m_hardwareFiltered );
}

void CBitmapImagePanel::ApplySettings( KeyValues* inResourceData )
{
	if ( m_pszImageName )
	{
		delete[] m_pszImageName;
		m_pszImageName = NULL;
	}

	if ( m_pszColorName )
	{
		delete[] m_pszColorName;
		m_pszColorName = NULL;
	}

	const char* imageName = inResourceData->GetString( "image", "" );
	if ( imageName[0] )
		setTexture( imageName, true );

	const char* colorName = inResourceData->GetString( "imagecolor", "" );
	if ( colorName[0] )
	{
		m_pszColorName = new char[strlen( colorName ) + 1];
		strcpy( m_pszColorName, colorName );
		InvalidateLayout( false, true ); // force ApplySchemeSettings to run
	}

	const char* alignmentString = inResourceData->GetString( "imageAlignment", "" );
	if ( alignmentString && alignmentString[0] )
	{
		if ( !_stricmp( alignmentString, "north-west" ) )
			SetContentAlignment( Label::a_northwest );
		else if ( !_stricmp( alignmentString, "north" ) )
			SetContentAlignment( Label::a_north );
		else if ( !_stricmp( alignmentString, "north-east" ) )
			SetContentAlignment( Label::a_northeast );
		else if ( !_stricmp( alignmentString, "west" ) )
			SetContentAlignment( Label::a_west );
		else if ( !_stricmp( alignmentString, "center" ) )
			SetContentAlignment( Label::a_center );
		else if ( !_stricmp( alignmentString, "east" ) )
			SetContentAlignment( Label::a_east );
		else if ( !_stricmp( alignmentString, "south-west" ) )
			SetContentAlignment( Label::a_southwest );
		else if ( !_stricmp( alignmentString, "south" ) )
			SetContentAlignment( Label::a_south );
		else if ( !_stricmp( alignmentString, "south-east" ) )
			SetContentAlignment( Label::a_southeast );
	}

	const char* preserveAspectRatio = inResourceData->GetString( "preserveAspectRatio", "" );
	if ( preserveAspectRatio && preserveAspectRatio[0] )
		m_preserveAspectRatio = strtol( preserveAspectRatio, NULL, 10 ) != 0;

	const char* filtered = inResourceData->GetString( "filtered", "" );
	if ( filtered && filtered[0] )
		m_hardwareFiltered = strtol( filtered, NULL, 10 ) != 0;

	Panel::ApplySettings( inResourceData );
}

//-----------------------------------------------------------------------------
// Purpose:  load the image, this is done just before this control is displayed
//-----------------------------------------------------------------------------
void CBitmapImagePanel::ApplySchemeSettings( IScheme* pScheme )
{
	Panel::ApplySchemeSettings( pScheme );

	if ( m_pszColorName )
		m_bgColor = pScheme->GetColor( m_pszColorName, m_bgColor );
}

//-----------------------------------------------------------------------------
// Purpose: Describes editing details
//-----------------------------------------------------------------------------
const char* CBitmapImagePanel::GetDescription()
{
	static char buf[1024];
	snprintf( buf, sizeof( buf ),
		"%s, string image, string imagecolor, alignment imageAlignment, int preserveAspectRatio, int filtered",
		Panel::GetDescription() );
	return buf;
}
}
