#include "hud.h"
#include "cl_util.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "../../engine/controls.h"
#include "../../vgui2/controls/controls.h"
#include <wchar.h>

int CHudVGUI2Print::Init()
{
	gHUD.AddHudElem( this );
	m_Fonts[0] = m_Fonts[1] = m_Fonts[2] = 0;
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudVGUI2Print::VidInit()
{
	m_flVGUI2StringTime = 0;
	vgui2::IScheme *scheme = vgui2::scheme()->GetIScheme( vgui2::scheme()->GetScheme( "ClientScheme" ) );
	m_Fonts[0] = scheme->GetFont( "HudFontSmall", true );
	m_Fonts[1] = scheme->GetFont( "HudFont", true );
	m_Fonts[2] = scheme->GetFont( "HudFontBig", true );
	return 1;
}

unsigned int CHudVGUI2Print::GetFont()
{
	int font = (int)gHUD.cl_hudfont->value;
	return m_Fonts[font == 0 ? 0 : font == 2 ? 2 : 1];
}

int CHudVGUI2Print::Draw( float )
{
	if ( m_flVGUI2StringTime > gEngfuncs.GetClientTime() )
		DrawVGUI2String( m_wCharBuf, m_iX, m_iY, m_fR, m_fG, m_fB );
	return 1;
}

int CHudVGUI2Print::DrawVGUI2String( wchar_t *text, int x, int y, float red, float green, float blue )
{
	unsigned int font = GetFont();
	if ( !font )
		return 0;
	int length = (int)wcslen( text );
	int a, b, c;
	if ( x == -1 )
	{
		int halfwidth = 0;
		for ( int i = 0; i < length / 2; ++i )
		{
			vgui2::surface()->GetCharABCwide( font, text[i], a, b, c );
			halfwidth += a + b + c;
		}
		x = ScreenWidth / 2 - halfwidth;
	}
	if ( y == -1 )
		y = (ScreenHeight - vgui2::surface()->GetFontTall( font )) / 2;
	gEngfuncs.pfnDrawSetTextColor( 0, 0, 0 );
	int position = x;
	for ( int i = 0; i < length; ++i )
	{
		gEngfuncs.pfnVGUI2DrawCharacter( position + 1, y + 1, text[i], font );
		vgui2::surface()->GetCharABCwide( font, text[i], a, b, c );
		position += a + b + c;
	}
	gEngfuncs.pfnDrawSetTextColor( red, green, blue );
	position = x;
	for ( int i = 0; i < length; ++i )
	{
		gEngfuncs.pfnVGUI2DrawCharacter( position, y, text[i], font );
		vgui2::surface()->GetCharABCwide( font, text[i], a, b, c );
		position += a + b + c;
	}
	return position;
}

int CHudVGUI2Print::DrawVGUI2String( char *text, int x, int y, float red, float green, float blue )
{
	wchar_t *localized = vgui2::localize()->Find( text );
	if ( localized )
		return DrawVGUI2String( localized, x, y, red, green, blue );
	wchar_t buffer[512];
	vgui2::localize()->ConvertANSIToUnicode( text, buffer, sizeof( buffer ) );
	return DrawVGUI2String( buffer, x, y, red, green, blue );
}

int CHudVGUI2Print::DrawVGUI2StringReverse( wchar_t *text, int x, int y, float red, float green, float blue )
{
	unsigned int font = GetFont();
	if ( !font )
		return 0;
	int length = (int)wcslen( text );
	if ( y == -1 )
		y = (ScreenHeight - vgui2::surface()->GetFontTall( font )) / 2;
	gEngfuncs.pfnDrawSetTextColor( red, green, blue );
	for ( int i = length - 1; i >= 0; --i )
	{
		int a, b, c;
		vgui2::surface()->GetCharABCwide( font, text[i], a, b, c );
		x -= a + b + c;
		gEngfuncs.pfnVGUI2DrawCharacter( x, y, text[i], font );
	}
	return x;
}

int CHudVGUI2Print::DrawVGUI2StringReverse( char *text, int x, int y, float red, float green, float blue )
{
	wchar_t *localized = vgui2::localize()->Find( text );
	if ( localized )
		return DrawVGUI2StringReverse( localized, x, y, red, green, blue );
	wchar_t buffer[512];
	vgui2::localize()->ConvertANSIToUnicode( text, buffer, sizeof( buffer ) );
	return DrawVGUI2StringReverse( buffer, x, y, red, green, blue );
}

void CHudVGUI2Print::VGUI2HudPrint( char *text, int x, int y, float red, float green, float blue )
{
	wchar_t *localized = vgui2::localize()->Find( text );
	if ( localized )
	{
		wcsncpy( m_wCharBuf, localized, sizeof( m_wCharBuf ) / sizeof( m_wCharBuf[0] ) - 1 );
		m_wCharBuf[sizeof( m_wCharBuf ) / sizeof( m_wCharBuf[0] ) - 1] = 0;
		m_iX = x;
		m_iY = y;
		m_fR = red;
		m_fG = green;
		m_fB = blue;
		m_flVGUI2StringTime = gEngfuncs.GetClientTime() + 4;
	}
}

void CHudVGUI2Print::VGUI2HudPrintArgs( char *text, char *arg1, char *arg2, char *arg3, char *arg4,
	int x, int y, float red, float green, float blue )
{
	wchar_t *localized = vgui2::localize()->Find( text );
	wchar_t first[64], second[64], third[64], fourth[64];
	vgui2::localize()->ConvertANSIToUnicode( arg1, first, sizeof( first ) );
	vgui2::localize()->ConvertANSIToUnicode( arg2, second, sizeof( second ) );
	vgui2::localize()->ConvertANSIToUnicode( arg3, third, sizeof( third ) );
	vgui2::localize()->ConvertANSIToUnicode( arg4, fourth, sizeof( fourth ) );
	m_iX = x;
	m_iY = y;
	m_fR = red;
	m_fG = green;
	m_fB = blue;
	m_flVGUI2StringTime = gEngfuncs.GetClientTime() + 4;
	if ( localized )
		vgui2::localize()->ConstructString( m_wCharBuf, sizeof( m_wCharBuf ), localized, 4, first, second, third, fourth );
	else
		vgui2::localize()->ConvertANSIToUnicode( text, m_wCharBuf, sizeof( m_wCharBuf ) );
}

int CHudVGUI2Print::GetHudFontHeight()
{
	unsigned int font = GetFont();
	return font ? vgui2::surface()->GetFontTall( font ) : 0;
}

void CHudVGUI2Print::GetStringSize( const wchar_t *text, int *width, int *height )
{
	if ( width )
		*width = 0;
	if ( height )
		*height = 0;
	unsigned int font = GetFont();
	if ( !font )
		return;
	int length = (int)wcslen( text );
	if ( width )
	{
		for ( int i = 0; i < length; ++i )
		{
			int a, b, c;
			vgui2::surface()->GetCharABCwide( font, text[i], a, b, c );
			*width += a + b + c;
		}
	}
	if ( height )
		*height = vgui2::surface()->GetFontTall( font );
}
