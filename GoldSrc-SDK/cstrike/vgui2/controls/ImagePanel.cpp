//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ImagePanel.h"
#include "controls.h"

#include "vgui/IBorder.h"
#include "vgui/IImage.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

#include <cstdio>
#include <cstring>

namespace vgui2
{
ImagePanel::ImagePanel( Panel* parent, const char* name ) : Panel( parent, name ), m_FillColor( 0, 0, 0, 0 )
{
	m_pImage = NULL;
	m_pszImageName = NULL;
	m_pszColorName = NULL;
	m_bScaleImage = false;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ImagePanel::~ImagePanel()
{
	if ( m_pszImageName )
		delete[] m_pszImageName;
	if ( m_pszColorName )
		delete[] m_pszColorName;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ImagePanel::SetImage( IImage* image )
{
	m_pImage = image;

	if ( image )
	{
		int wide, tall;
		if ( m_bScaleImage )
		{
			// scaling, force the image size to be our size
			GetSize( wide, tall );
			m_pImage->SetSize( wide, tall );
		}
		else
		{
			// not scaling, so set our size to the image size
			image->GetSize( wide, tall );
			SetSize( wide, tall );
		}
	}

	Repaint();
}

void ImagePanel::SetImage( IImage* image, bool bScale )
{
	m_bScaleImage = bScale;
	SetImage( image );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IImage* ImagePanel::GetImage()
{
	return m_pImage;
}

void ImagePanel::GetImageSize( int& width, int& height )
{
	if ( !m_pImage )
	{
		width = 0;
		height = 0;
		return;
	}

	m_pImage->GetSize( width, height );
}

void ImagePanel::SetImageSize( int width, int height )
{
	if ( m_pImage )
		m_pImage->SetSize( width, height );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ImagePanel::PaintBackground()
{
	if ( m_FillColor.a() )
	{
		// draw the specified fill color
		int wide, tall;
		GetSize( wide, tall );

		surface()->DrawSetColor( m_FillColor );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}

	if ( m_pImage )
	{
		surface()->DrawSetColor( 255, 255, 255, 255 );
		m_pImage->SetPos( 0, 0 );
		m_pImage->Paint();
	}
}

void ImagePanel::OnSizeChanged( int newWide, int newTall )
{
	if ( m_bScaleImage )
		SetImageSize( newWide, newTall );

	Panel::OnSizeChanged( newWide, newTall );
}

//-----------------------------------------------------------------------------
// Purpose: Gets control settings for editing
//-----------------------------------------------------------------------------
void ImagePanel::GetSettings( KeyValues* outResourceData )
{
	Panel::GetSettings( outResourceData );

	if ( m_pszImageName )
		outResourceData->SetString( "image", m_pszImageName );
	if ( m_pszColorName )
		outResourceData->SetString( "fillcolor", m_pszColorName );
	if ( GetBorder() )
		outResourceData->SetString( "border", GetBorder()->GetName() );

	outResourceData->SetInt( "scaleImage", m_bScaleImage );
}

//-----------------------------------------------------------------------------
// Purpose: Applies designer settings from res file
//-----------------------------------------------------------------------------
void ImagePanel::ApplySettings( KeyValues* inResourceData )
{
	if ( m_pszImageName )
		delete[] m_pszImageName;
	if ( m_pszColorName )
		delete[] m_pszColorName;
	m_pszImageName = NULL;
	m_pszColorName = NULL;

	m_bScaleImage = inResourceData->GetInt( "scaleImage", 0 ) != 0;

	const char* imageName = inResourceData->GetString( "image", "" );
	if ( imageName[0] )
	{
		m_pszImageName = new char[strlen( imageName ) + 1];
		strcpy( m_pszImageName, imageName );
		InvalidateLayout( false, true );
	}

	const char* pszFillColor = inResourceData->GetString( "fillcolor", "" );
	if ( pszFillColor[0] )
	{
		int r = 0, g = 0, b = 0, a = 0;
		m_pszColorName = new char[strlen( pszFillColor ) + 1];
		strcpy( m_pszColorName, pszFillColor );

		if ( sscanf( pszFillColor, "%d %d %d %d", &r, &g, &b, &a ) >= 3 )
		{
			// it's a direct color
			m_FillColor.SetColor( r, g, b, a );
		}
		else
		{
			IScheme* pScheme = scheme()->GetIScheme( GetScheme() );
			m_FillColor = pScheme->GetColor( pszFillColor, Color( 0, 0, 0, 0 ) );
		}
	}

	const char* pszBorder = inResourceData->GetString( "border", "" );
	if ( pszBorder[0] )
	{
		IScheme* pScheme = scheme()->GetIScheme( GetScheme() );
		SetBorder( pScheme->GetBorder( pszBorder ) );
	}

	Panel::ApplySettings( inResourceData );
}

void ImagePanel::ApplySchemeSettings( IScheme* pScheme )
{
	Panel::ApplySchemeSettings( pScheme );

	if ( m_pszImageName && m_pszImageName[0] )
		SetImage( scheme()->GetImage( m_pszImageName, m_bScaleImage ) );
}


//-----------------------------------------------------------------------------
// Purpose: Describes editing details
//-----------------------------------------------------------------------------
const char* ImagePanel::GetDescription()
{
	static char buf[1024];
	_snprintf( buf, sizeof( buf ), "%s, string image, string border, string fillcolor, bool scaleImage",
		Panel::GetDescription() );
	return buf;
}
}
