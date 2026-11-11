//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BackgroundMenuButton.h"

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBackgroundMenuButton::CBackgroundMenuButton( vgui2::Panel* parent, const char* name )
	: vgui2::Button( parent, name, "" )
{
	m_pImage = NULL;
	m_pMouseOverImage = NULL;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CBackgroundMenuButton::~CBackgroundMenuButton()
{
}

void CBackgroundMenuButton::SetImages( vgui2::IImage* image, vgui2::IImage* mouseOverImage )
{
	m_pMouseOverImage = mouseOverImage;
	m_pImage = image;
	SetArmed( IsArmed() );
}

void CBackgroundMenuButton::SetArmed( bool state )
{
	SetImageAtIndex( 0, state ? m_pMouseOverImage : m_pImage, 0 );
	vgui2::Button::SetArmed( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBackgroundMenuButton::OnCommand( const char* command )
{
	vgui2::Button::OnCommand( command );
}

void CBackgroundMenuButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Button::ApplySchemeSettings( scheme );
	// hack some colors in
	SetFgColor( Color( 255, 255, 255, 255 ) );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetDefaultColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 0 ) );
	SetArmedColor( Color( 255, 255, 0, 255 ), Color( 0, 0, 0, 0 ) );
	SetDepressedColor( Color( 255, 255, 0, 255 ), Color( 0, 0, 0, 0 ) );
	SetContentAlignment( vgui2::Label::a_west );
	SetBorder( NULL );
	SetDefaultBorder( NULL );
	SetDepressedBorder( NULL );
	SetKeyFocusBorder( NULL );
	SetTextInset( 0, 0 );
	SetArmedSound( "sound/UI/buttonrollover.wav" );
	SetDepressedSound( "sound/UI/buttonclick.wav" );
	SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
}
