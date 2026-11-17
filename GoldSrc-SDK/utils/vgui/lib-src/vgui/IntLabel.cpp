#include "VGUI_IntLabel.h"

#include <stdio.h>
#include <string.h>

namespace vgui
{

IntLabel::IntLabel( int value, int x, int y, int wide, int tall ) : Label( NULL, x, y, wide, tall )
{
	_value = 0;
	setValue( value );
}

void IntLabel::setValue( int value )
{
	if ( _value != value )
	{
		_value = value;
		repaint();
	}
}

void IntLabel::intChanged( int value, Panel* )
{
	setValue( value );
}

void IntLabel::paintBackground()
{
	char buf[50];

	snprintf( buf, sizeof( buf ), "%d", _value );
	Panel::paintBackground();
	drawSetTextFont( Scheme::sf_primary1 );
	drawSetTextColor( Scheme::sc_black );
	drawPrintText( 0, 0, buf, (int)strlen( buf ) );
}

}
