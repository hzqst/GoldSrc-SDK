#include "highlightbutton.h"

#include "Controls.h"
#include "vgui/IScheme.h"

CHighlightButton::CHighlightButton( vgui2::Panel* parent, const char* panelName, const char* text )
	: vgui2::Button( parent, panelName, text ), m_armedBorder( NULL )
{
}

CHighlightButton::~CHighlightButton()
{
}

void CHighlightButton::SetArmedBorder( vgui2::IBorder* border )
{
	m_armedBorder = border;
	InvalidateLayout( false, false );
}

void CHighlightButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Button::ApplySchemeSettings( scheme );
	SetDefaultBorder( scheme->GetBorder( "HighlightButtonBorder" ) );
	SetDepressedBorder( scheme->GetBorder( "HighlightButtonDepressedBorder" ) );
	m_armedBorder = scheme->GetBorder( "HighlightButtonArmedBorder" );
	SetFgColor( GetSchemeColor( "WorldMap/ResumeTextColor", scheme ) );
	SetDisabledFgColor1( GetSchemeColor( "WorldMap/ResumeDisabledFgColor1", scheme ) );
	SetDisabledFgColor2( GetSchemeColor( "WorldMap/ResumeDisabledFgColor2", scheme ) );
	SetDefaultColor( GetSchemeColor( "WorldMap/ResumeTextColor", scheme ),
		GetSchemeColor( "WorldMap/ResumeDefaultBgColor", scheme ) );
	SetArmedColor( GetSchemeColor( "WorldMap/ResumeTextColor", scheme ),
		GetSchemeColor( "WorldMap/ResumeArmedBgColor", scheme ) );
	SetDepressedColor( GetSchemeColor( "WorldMap/ResumeTextColor", scheme ),
		GetSchemeColor( "WorldMap/ResumeDepressedBgColor", scheme ) );
}

vgui2::IBorder* CHighlightButton::GetBorder( bool depressed, bool armed, bool selected, bool keyfocus )
{
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
	SetKeyFocusBorder( scheme->GetBorder( "WorldMapMissionButtonKeyFocusBorder" ) );
	if ( !depressed && armed && IsEnabled() )
		return m_armedBorder;
	return vgui2::Button::GetBorder( depressed, armed, selected, keyfocus );
}
