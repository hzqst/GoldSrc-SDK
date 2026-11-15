#include "missionradiobutton.h"

#include "TextImage.h"
#include "Controls.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"

CMissionRadioButton::CMissionRadioButton( vgui2::Panel* parent, const char* panelName, const char* text )
	: vgui2::RadioButton( parent, panelName, text ), m_iMarlettTall( 0 )
{
}

CMissionRadioButton::~CMissionRadioButton()
{
}

void CMissionRadioButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::RadioButton::ApplySchemeSettings( scheme );
	SetFgColor( GetSchemeColor( "Mission/ButtonTextColor", scheme ) );
	SetDisabledFgColor1( GetSchemeColor( "Mission/ButtonDisabledFgColor1", scheme ) );
	SetDisabledFgColor2( GetSchemeColor( "Mission/ButtonDisabledFgColor2", scheme ) );
	SetDefaultColor( GetSchemeColor( "Mission/ButtonDefaultFgColor", scheme ),
		GetSchemeColor( "Mission/ButtonDefaultBgColor", scheme ) );
	SetArmedColor( GetSchemeColor( "Mission/ButtonArmedFgColor", scheme ),
		GetSchemeColor( "Mission/ButtonArmedBgColor", scheme ) );
	SetDepressedColor( GetSchemeColor( "Mission/ButtonDepressedFgColor", scheme ),
		GetSchemeColor( "Mission/ButtonDepressedBgColor", scheme ) );
	SetKeyFocusBorder( scheme->GetBorder( "WorldMapMissionButtonKeyFocusBorder" ) );
	m_iMarlettTall = vgui2::surface()->GetFontTall( scheme->GetFont( "Marlett", true ) );
}

void CMissionRadioButton::Paint()
{
	vgui2::IImage* image = GetImageAtIndex( 0 );
	vgui2::TextImage* textImage = dynamic_cast<vgui2::TextImage*>( GetImageAtIndex( 1 ) );
	wchar_t text[9];
	textImage->GetText( text, 2 );
	int fontTall = vgui2::surface()->GetFontTall( textImage->GetFont() );
	vgui2::surface()->GetFontAscent( textImage->GetFont(), text[0] );
	int textWide;
	textImage->GetDrawWidth( textWide );
	int imageWide, imageTall;
	image->GetSize( imageWide, imageTall );
	imageTall = m_iMarlettTall;
	int wide, tall;
	GetSize( wide, tall );
	int textOffset = vgui2::scheme()->GetProportionalScaledValue( 8 );
	image->SetPos( 0, tall - imageTall );
	image->Paint();
	int x = imageWide + textOffset;
	int y = tall - fontTall - ( imageTall - fontTall ) / 2;
	textImage->SetPos( x, y );
	if ( !IsEnabled() )
	{
		textImage->SetPos( x + 1, y + 1 );
		textImage->SetColor( GetDisabledFgColor1() );
		textImage->Paint();
		vgui2::surface()->DrawFlushText();
		textImage->SetPos( x, y );
		textImage->SetColor( GetDisabledFgColor2() );
	}
	else
	{
		textImage->SetColor( GetFgColor() );
	}
	textImage->Paint();
}
