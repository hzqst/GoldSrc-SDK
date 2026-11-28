#include "VGUI_Font.h"

#include "UTF8Text.h"

using namespace vgui;

void Font::getTextSize( const char* text, int& wide, int& tall )
{
	wide = 0;
	tall = 0;
	if ( text == NULL )
	{
		return;
	}
	tall = getTall();
	int lineWide = 0;
	while ( *text != '\0' )
	{
		if ( *text == '\n' )
		{
			lineWide = 0;
			tall += getTall();
			++text;
		}
		else
		{
			int a;
			int b;
			int c;
			int length = UTF8Text_getCharABCwide( this, text, a, b, c );
			if ( length == 0 )
			{
				getCharABCwide( (unsigned char)*text, a, b, c );
				length = 1;
			}
			text += length;
			lineWide += a + b + c;
			if ( lineWide > wide )
			{
				wide = lineWide;
			}
		}
	}
}

int Font::getId()
{
	return _id;
}

BaseFontPlat* Font::getPlat()
{
	return _plat;
}
