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

#include "AnimatingImagePanel.h"
#include "controls.h"
#include "vgui/IImage.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vgui2
{
AnimatingImagePanel::AnimatingImagePanel( Panel* parent, const char* name )
	: Panel( parent, name ), m_iCurrentImage( 0 ), m_iNextFrameTime( 0 ), m_iFrameTimeMillis( 100 ),
	  m_pImageName( NULL ), m_bFiltered( false )
{
	m_bAnimating = false;
	ivgui()->AddTickSignal( _vpanel, 0 );
}

AnimatingImagePanel::~AnimatingImagePanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: Add an image to the end of the list of animations
//-----------------------------------------------------------------------------
void AnimatingImagePanel::AddImage( IImage* image )
{
	m_Frames.AddToTail( image );

	if ( image )
	{
		int wide, tall;
		image->GetSize( wide, tall );
		SetSize( wide, tall );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Load a set of animations by name.
// Input: 
//		baseName: is the name of the animations without thier frame number or 
//			file extension, (e.g. c1.tga becomes just c.)
//		framecount: number of frames in the animation
//-----------------------------------------------------------------------------
void AnimatingImagePanel::LoadAnimation( const char* baseName, int frameCount )
{
	char imageName[512];

	m_Frames.RemoveAll();
	for ( int i = 1; i <= frameCount; i++ )
	{
		if ( strlen( baseName ) + 11 > sizeof( imageName ) )
			break;
		sprintf( imageName, "%s%d", baseName, i );
		AddImage( scheme()->GetImage( imageName, m_bFiltered ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Starts the image doing its animation
//-----------------------------------------------------------------------------
void AnimatingImagePanel::StartAnimation()
{
	m_bAnimating = true;
//	ivgui()->AddTickSignal(GetVPanel());
}

//-----------------------------------------------------------------------------
// Purpose: Stops the images animation
//-----------------------------------------------------------------------------
void AnimatingImagePanel::StopAnimation()
{
	m_bAnimating = false;
//	ivgui()->RemoveTickSignal(GetVPanel());
}

void AnimatingImagePanel::ResetAnimation( int frame )
{
	if ( frame < 0 || frame >= m_Frames.Count() )
		frame = 0;

	m_iCurrentImage = frame;
	Repaint();
}

void AnimatingImagePanel::OnTick()
{
	if ( !m_bAnimating )
		return;

	if ( system()->GetTimeMillis() < m_iNextFrameTime )
		return;

	m_iNextFrameTime = system()->GetTimeMillis() + 100;

	m_iCurrentImage++;
	if ( m_iCurrentImage < 0 || m_iCurrentImage >= m_Frames.Count() )
		m_iCurrentImage = 0;

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Layout the panel for drawing.
//-----------------------------------------------------------------------------
void AnimatingImagePanel::PerformLayout()
{
	Panel::PerformLayout();
	Repaint();
}

void AnimatingImagePanel::PaintBackground()
{
	if ( m_iCurrentImage < 0 || m_iCurrentImage >= m_Frames.Count() )
		return;

	IImage* image = m_Frames[m_iCurrentImage];
	if ( !image )
		return;

	surface()->DrawSetColor( 255, 255, 255, 255 );
	image->Paint();
}

void AnimatingImagePanel::GetSettings( KeyValues* settings )
{
	Panel::GetSettings( settings );

	if ( m_pImageName )
		settings->SetString( "image", m_pImageName );
}

//-----------------------------------------------------------------------------
// Purpose: Applies resouce settings
//-----------------------------------------------------------------------------
void AnimatingImagePanel::ApplySettings( KeyValues* settings )
{
	Panel::ApplySettings( settings );

	const char* imageName = settings->GetString( "image", NULL );
	if ( !imageName )
		return;

	if ( m_pImageName )
		delete[] m_pImageName;
	m_pImageName = new char[strlen( imageName ) + 1];
	strcpy( m_pImageName, imageName );

	LoadAnimation( m_pImageName, settings->GetInt( "frames", 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Get editing details
//-----------------------------------------------------------------------------
const char* AnimatingImagePanel::GetDescription()
{
	static char buf[1024];
	_snprintf( buf, sizeof( buf ), "%s, string image", Panel::GetDescription() );
	return buf;
}
}
