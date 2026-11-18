#include "VGUI_TextPanel.h"

namespace vgui
{

TextPanel::TextPanel( const char* text, int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	_textImage = new TextImage( text );
	_textImage->setSize( wide, tall );
}

#if VGUI_ABI_COMPAT_MODE != 1
TextPanel::TextPanel( const TextPanel& other ) : Panel( other )
{
	_textImage = new TextImage( *other._textImage );
}

TextPanel::~TextPanel()
{
	delete _textImage;
}

TextPanel& TextPanel::operator=( const TextPanel& other )
{
	if ( this != &other )
	{
		Panel::operator=( other );
		*_textImage = *other._textImage;
	}

	return *this;
}
#endif

TextImage* TextPanel::getTextImage()
{
	return _textImage;
}

void TextPanel::setText( const char* text )
{
	_textImage->setText( text );
}

void TextPanel::setFont( Scheme::SchemeFont schemeFont )
{
	_textImage->setFont( schemeFont );
}

void TextPanel::setFont( Font* font )
{
	_textImage->setFont( font );
}

void TextPanel::paint()
{
	_textImage->doPaint( this );
}

void TextPanel::setSize( int wide, int tall )
{
	Panel::setSize( wide, tall );
	getPaintSize( wide, tall );
	_textImage->setSize( wide, tall );
}

void TextPanel::setFgColor( int r, int g, int b, int a )
{
	Panel::setFgColor( r, g, b, a );
	_textImage->setColor( Color( r, g, b, a ) );
}

void TextPanel::setFgColor( Scheme::SchemeColor sc )
{
	Panel::setFgColor( sc );
	_textImage->setColor( Color( sc ) );
}

}
