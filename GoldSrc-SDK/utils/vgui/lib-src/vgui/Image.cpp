#include "VGUI_Image.h"

#include "VGUI.h"
#include "VGUI_Panel.h"

namespace vgui
{

#if VGUI_ABI_COMPAT_MODE != 1
Image::~Image()
{
}

void* Image::operator new( size_t size )
{
	return ::operator new( size );
}

void Image::operator delete( void* memory )
{
	::operator delete( memory );
}
#endif

Image::Image()
{
	_panel = NULL;
	setPos( 0, 0 );
	setSize( 0, 0 );
	setColor( Color( 255, 255, 255, 0 ) );
}

void Image::setPos( int x, int y )
{
	_pos[0] = x;
	_pos[1] = y;
}

void Image::getPos( int& x, int& y )
{
	x = _pos[0];
	y = _pos[1];
}

void Image::getSize( int& wide, int& tall )
{
	wide = _size[0];
	tall = _size[1];
}

void Image::setColor( Color color )
{
	_color = color;
}

void Image::getColor( Color& color )
{
	color = _color;
}

void Image::setSize( int wide, int tall )
{
	_size[0] = wide;
	_size[1] = tall;
}

void Image::drawSetColor( Scheme::SchemeColor sc )
{
	_panel->drawSetColor( sc );
}

void Image::drawSetColor( int r, int g, int b, int a )
{
	_panel->drawSetColor( r, g, b, a );
}

void Image::drawFilledRect( int x0, int y0, int x1, int y1 )
{
	_panel->drawFilledRect( x0 + _pos[0], y0 + _pos[1], x1 + _pos[0], y1 + _pos[1] );
}

void Image::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	_panel->drawOutlinedRect( x0 + _pos[0], y0 + _pos[1], x1 + _pos[0], y1 + _pos[1] );
}

void Image::drawSetTextFont( Scheme::SchemeFont sf )
{
	_panel->drawSetTextFont( sf );
}

void Image::drawSetTextFont( Font* font )
{
	_panel->drawSetTextFont( font );
}

void Image::drawSetTextColor( Scheme::SchemeColor sc )
{
	_panel->drawSetTextColor( sc );
}

void Image::drawSetTextColor( int r, int g, int b, int a )
{
	_panel->drawSetTextColor( r, g, b, a );
}

void Image::drawSetTextPos( int x, int y )
{
	_panel->drawSetTextPos( x + _pos[0], y + _pos[1] );
}

void Image::drawPrintText( const char* str, int textLen )
{
	_panel->drawPrintText( str, textLen );
}

void Image::drawPrintText( int x, int y, const char* str, int textLen )
{
	_panel->drawPrintText( x + _pos[0], y + _pos[1], str, textLen );
}

void Image::drawPrintChar( char ch )
{
	_panel->drawPrintChar( ch );
}

void Image::drawPrintChar( int x, int y, char ch )
{
	_panel->drawPrintChar( x + _pos[0], y + _pos[1], ch );
}

void Image::drawSetTextureRGBA( int id, const char* rgba, int wide, int tall )
{
	_panel->drawSetTextureRGBA( id, rgba, wide, tall );
}

void Image::drawSetTexture( int id )
{
	_panel->drawSetTexture( id );
}

void Image::drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	_panel->drawTexturedRect( x0, y0, x1, y1 );
}

void Image::paint( Panel* )
{
}

void Image::doPaint( Panel* panel )
{
	_panel = panel;
	paint( panel );
	_panel = NULL;
}

}
