#include "VGUI_Font.h"

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
	for ( ; *text != '\0'; ++text )
	{
		if ( *text == '\n' )
		{
			lineWide = 0;
			tall += getTall();
		}
		else
		{
			int a;
			int b;
			int c;
			getCharABCwide( (unsigned char)*text, a, b, c );
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
