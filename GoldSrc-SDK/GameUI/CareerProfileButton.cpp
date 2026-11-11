#include "CareerProfileButton.h"
#include "CareerProfileFrame.h"
#include "GameUI_Interface.h"

#include "TextImage.h"
#include "vgui/IScheme.h"
#include "vgui_controls/Controls.h"

#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
CProfileButton::CProfileButton( CCareerProfileFrame* parent, const char* name,
		const wchar_t* text, bool* medals )
	: vgui2::Button( NULL, name, "" )
{
	SetSize( GetWide(), 30 );
	m_textImage = new vgui2::TextImage( text );
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; i++ )
	{
		char filename[64];
#ifdef _WIN32
		_snprintf( filename, sizeof( filename ), "gfx/career/medals/Medal.0%d_%s_small",
			i + 1, medals[i] ? "Active" : "Inactive" );
#else
		snprintf( filename, sizeof( filename ), "gfx/career/medals/Medal.0%d_%s_small",
			i + 1, medals[i] ? "Active" : "Inactive" );
#endif
		m_image[i] = vgui2::scheme()->GetImage( filename, false );
	}
	m_name.clear();
	m_active = false;
	m_pParent = parent;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CProfileButton::~CProfileButton()
{
}

void CProfileButton::SetProfileName( const char* name )
{
	m_name = name;
}

void CProfileButton::OnMouseDoublePressed( vgui2::MouseCode mouseCode )
{
	if ( !IsEnabled() )
		return;
	if ( mouseCode != vgui2::MOUSE_LEFT || !m_pParent )
	{
		vgui2::Button::OnMouseDoublePressed( mouseCode );
		return;
	}
	m_pParent->OnCommand( ( "SelectProfile " + m_name ).c_str() );
	m_pParent->OnCommand( "ResumeProfile" );
}

void CProfileButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Button::ApplySchemeSettings( scheme );
	m_preTextPad = 10;
	m_postTextPad = 10;
	m_interImagePad = 2;
	m_postImagePad = 10;
	m_textImage->SetColor( scheme->GetColor( "ProfileButtonFG", Color( 0, 0, 0, 0 ) ) );
	m_textImage->SetFont( scheme->GetFontEx( "DefaultShadow", true, IsHDProportional() ) );
	m_textImage->ResizeImageToContent();
	m_bgColor = scheme->GetColor( "ProfileButtonBG", Color( 0, 0, 0, 0 ) );
	m_activeBgColor = scheme->GetColor( "ProfileButtonActiveBG", Color( 0, 0, 0, 0 ) );
	SetArmedSound( "sound/UI/buttonrollover.wav" );
	SetDepressedSound( "sound/UI/buttonclick.wav" );
	SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
	m_depressedBorder = scheme->GetBorder( "CareerButtonDepressedBorder" );
	m_armedBorder = scheme->GetBorder( "CareerButtonArmedBorder" );
}

vgui2::IBorder* CProfileButton::GetBorder( bool depressed, bool armed, bool, bool )
{
	if ( IsEnabled() )
	{
		if ( !depressed && armed )
			return m_armedBorder;
		if ( depressed )
			return m_depressedBorder;
	}
	return NULL;
}

Color CProfileButton::GetBgColor()
{
	return m_active ? m_activeBgColor : m_bgColor;
}

void CProfileButton::Paint()
{
	int wide, tall;
	GetSize( wide, tall );
	int x = wide - m_postImagePad;
	for ( int i = MAX_CAREER_DIFFICULTY - 1; i >= 0; i-- )
	{
		int imageWide, imageTall;
		m_image[i]->GetSize( imageWide, imageTall );
		x -= imageWide;
		m_image[i]->SetPos( x, ( tall - imageTall ) / 2 );
		m_image[i]->Paint();
		if ( i )
			x -= m_interImagePad;
	}
	x -= m_postTextPad;
	int textWide, textTall;
	m_textImage->GetSize( textWide, textTall );
	if ( x - m_preTextPad < textWide )
		m_textImage->SetSize( x - m_preTextPad, GetTall() );
	m_textImage->SetPos( x - textWide, ( tall - textTall ) / 2 );
	m_textImage->Paint();
}
