#include "VGUI_Label.h"
#include "ActionSignalForwarder.h"
#include "VGUI_ActionSignal.h"
#include "VGUI_Color.h"
#include "VGUI_FlowLayout.h"
#include "VGUI_Font.h"
#include "VGUI_Image.h"
#include "VGUI_TextEntry.h"
#include "VGUI_TextImage.h"
#include "VGUI_TreeFolder.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace
{

using namespace vgui;

class FooDumb : public Panel, public ActionSignal
{
public:
	FooDumb( Label* label );

	virtual void actionPerformed( Panel* panel ) override;

private:
	Label*		_label;
	TextEntry*	_textEntry;
};

FooDumb::FooDumb( Label* label ) : Panel( 0, 0, 200, 20 )
{
	_label = label;

	// Panel does not own its layout, and a FlowLayout keeps no state between
	// layouts, so every FooDumb shares this one instead of making one nothing
	// frees.
	static FlowLayout layout( 2 );
	setLayout( &layout );

	Label* textLabel = new Label( "setText" );
	textLabel->setParent( this );

	_textEntry = new TextEntry( "", 0, 0, 80, 20 );
	_textEntry->setParent( this );
	_textEntry->addActionSignal( new ActionSignalForwarder( this ) );
}

void FooDumb::actionPerformed( Panel* )
{
	char buf[256];

	_textEntry->getText( 0, buf, 256 );
	_label->setText( buf );
	_label->repaint();
}

}

namespace vgui
{

Label::Label( int textBufferLen, const char* text, int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	init( textBufferLen, text, false );
}

Label::Label( const char* text, int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	init( (int)strlen( text ) + 1, text, false );
}

Label::Label( const char* text, int x, int y ) : Panel( x, y, 10, 10 )
{
	init( (int)strlen( text ) + 1, text, true );
}

Label::Label( const char* text ) : Panel( 0, 0, 10, 10 )
{
	init( (int)strlen( text ) + 1, text, true );
}

Label::Label() : Panel( 0, 0, 10, 10 )
{
	init( 1, "", true );
}

#if VGUI_ABI_COMPAT_MODE != 1
// A Label owns its TextImage; copies get their own so that the destructor
// never deletes a shared one. _image is borrowed and stays shared.
Label::Label( const Label& other ) : Panel( other )
{
	_textEnabled = other._textEnabled;
	_imageEnabled = other._imageEnabled;
	_contentFitted = other._contentFitted;
	_textAlignment = other._textAlignment;
	_contentAlignment = other._contentAlignment;
	_textImage = other._textImage != NULL ? new TextImage( *other._textImage ) : NULL;
	_image = other._image;
}

Label::~Label()
{
	delete _textImage;
}

Label& Label::operator=( const Label& other )
{
	if ( this != &other )
	{
		Panel::operator=( other );

		TextImage* textImage = other._textImage != NULL ? new TextImage( *other._textImage ) : NULL;
		delete _textImage;

		_textEnabled = other._textEnabled;
		_imageEnabled = other._imageEnabled;
		_contentFitted = other._contentFitted;
		_textAlignment = other._textAlignment;
		_contentAlignment = other._contentAlignment;
		_textImage = textImage;
		_image = other._image;
	}

	return *this;
}
#endif

void Label::init( int textBufferLen, const char* text, bool textFitted )
{
	_contentAlignment = Alignment::a_center;
	_contentFitted = textFitted;
	_textAlignment = Alignment::a_center;
	_textImage = new TextImage( textBufferLen, text );
	_textImage->setColor( Color( Scheme::sc_black ) );
	_image = NULL;
	setText( textBufferLen, text );
}

void Label::setImage( Image* image )
{
	_image = image;
	recomputeMinimumSize();

	if ( image != NULL )
	{
		repaint();
	}
}

void Label::setText( int textBufferLen, const char* text )
{
	_textImage->setText( textBufferLen, text );
	recomputeMinimumSize();
	repaint();
}

void Label::setText( const char* format, ... )
{
	char buf[4096];
	va_list argptr;

	va_start( argptr, format );
	vsnprintf( buf, 4096, format, argptr );
	va_end( argptr );
	buf[4095] = 0;
	setText( (int)strlen( buf ) + 1, buf );
}

void Label::setFont( Scheme::SchemeFont schemeFont )
{
	_textImage->setFont( schemeFont );
	recomputeMinimumSize();
	repaint();
}

void Label::setFont( Font* font )
{
	_textImage->setFont( font );
	recomputeMinimumSize();
	repaint();
}

void Label::getTextSize( int& wide, int& tall )
{
	_textImage->getSize( wide, tall );
}

void Label::getContentSize( int& wide, int& tall )
{
	int tx0;
	int ty0;
	int tx1;
	int ty1;
	int ix0;
	int iy0;
	int ix1;
	int iy1;
	int minX;
	int minY;
	int maxX;
	int maxY;

	computeAlignment( tx0, ty0, tx1, ty1, ix0, iy0, ix1, iy1, minX, minY, maxX, maxY );
	wide = maxX - minX;
	tall = maxY - minY;
}

void Label::setTextAlignment( Alignment alignment )
{
	_textAlignment = alignment;
	recomputeMinimumSize();
	repaint();
}

void Label::setContentAlignment( Alignment alignment )
{
	_contentAlignment = alignment;
	recomputeMinimumSize();
	repaint();
}

void Label::setContentFitted( bool state )
{
	_contentFitted = state;
	recomputeMinimumSize();
	repaint();
}

Panel* Label::createPropertyPanel()
{
	Panel* propertyPanel = Panel::createPropertyPanel();
	TreeFolder* folder = new TreeFolder( "Label" );
	propertyPanel->addChild( folder );
	folder->addChild( new FooDumb( this ) );
	folder->addChild( new Label( "setContentAlignment" ) );
	return propertyPanel;
}

void Label::setFgColor( Scheme::SchemeColor sc )
{
	Panel::setFgColor( sc );

	Color color( sc );
	_textImage->setColor( color );
	repaint();
}

void Label::setFgColor( int r, int g, int b, int a )
{
	Panel::setFgColor( r, g, b, a );

	Color color( r, g, b, a );
	_textImage->setColor( color );
	repaint();
}

void Label::computeAlignment( int& tx0, int& ty0, int& tx1, int& ty1, int& ix0, int& iy0, int& ix1, int& iy1,
		int& minX, int& minY, int& maxX, int& maxY )
{
	int wide;
	int tall;
	getPaintSize( wide, tall );

	int tWide;
	int tTall;
	getTextSize( tWide, tTall );

	int iWide = 0;
	int iTall = 0;
	if ( _image != NULL )
	{
		_image->getSize( iWide, iTall );
	}

	ix0 = 0;
	iy0 = 0;
	ix1 = iWide;
	iy1 = iTall;

	tx0 = 0;
	ty0 = 0;
	tx1 = tWide;
	ty1 = tTall;

	switch ( _textAlignment )
	{
	case a_northwest:
	case a_west:
	case a_southwest:
		tx0 = ix0 - tWide;
		break;
	case a_north:
	case a_center:
	case a_south:
		tx0 = ( ( ix1 - ix0 ) / 2 ) - ( ( tx1 - tx0 ) / 2 );
		break;
	case a_northeast:
	case a_east:
	case a_southeast:
		tx0 = ix1;
		break;
	}

	switch ( _textAlignment )
	{
	case a_northwest:
	case a_north:
	case a_northeast:
		ty0 = iy0 - tTall;
		break;
	case a_west:
	case a_center:
	case a_east:
		ty0 = ( ( iy1 - iy0 ) / 2 ) - ( ( ty1 - ty0 ) / 2 );
		break;
	case a_southwest:
	case a_south:
	case a_southeast:
		ty0 = iy1;
		break;
	}

	tx1 = tx0 + tWide;
	ty1 = ty0 + tTall;

	minX = tx0;
	minY = ty0;
	if ( ix0 < minX )
	{
		minX = ix0;
	}
	if ( iy0 < minY )
	{
		minY = iy0;
	}

	maxX = tx1;
	maxY = ty1;
	if ( ix1 > maxX )
	{
		maxX = ix1;
	}
	if ( iy1 > maxY )
	{
		maxY = iy1;
	}

	tx0 -= minX;
	ty0 -= minY;
	tx1 -= minX;
	ty1 -= minY;
	ix0 -= minX;
	iy0 -= minY;
	ix1 -= minX;
	iy1 -= minY;
	maxX -= minX;
	maxY -= minY;
	minX = 0;
	minY = 0;

	int offX = 0;
	int offY = 0;

	switch ( _contentAlignment )
	{
	case a_northwest:
	case a_west:
	case a_southwest:
		offX = 0;
		break;
	case a_north:
	case a_center:
	case a_south:
		offX = ( wide / 2 ) - ( ( maxX - minX ) / 2 );
		break;
	case a_northeast:
	case a_east:
	case a_southeast:
		offX = wide - ( maxX - minX );
		break;
	}

	switch ( _contentAlignment )
	{
	case a_northwest:
	case a_north:
	case a_northeast:
		offY = 0;
		break;
	case a_west:
	case a_center:
	case a_east:
		offY = ( tall / 2 ) - ( ( maxY - minY ) / 2 );
		break;
	case a_southwest:
	case a_south:
	case a_southeast:
		offY = tall - ( maxY - minY );
		break;
	}

	tx0 += offX;
	ty0 += offY;
	tx1 += offX;
	ty1 += offY;
	ix0 += offX;
	iy0 += offY;
	ix1 += offX;
	iy1 += offY;
	minX += offX;
	minY += offY;
	maxX += offX;
	maxY += offY;

	minX -= 4;
	minY -= 4;
	maxX += 4;
	maxY += 4;
}

void Label::paint()
{
	int tx0;
	int ty0;
	int tx1;
	int ty1;
	int ix0;
	int iy0;
	int ix1;
	int iy1;
	int minX;
	int minY;
	int maxX;
	int maxY;

	computeAlignment( tx0, ty0, tx1, ty1, ix0, iy0, ix1, iy1, minX, minY, maxX, maxY );
	if ( _image != NULL )
	{
		_image->setPos( ix0, iy0 );
		_image->doPaint( this );
	}
	if ( _textImage != NULL )
	{
		_textImage->setPos( tx0, ty0 );
		_textImage->doPaint( this );
	}
	if ( hasFocus() )
	{
		drawSetColor( Scheme::sc_primary2 );
		drawOutlinedRect( tx0, ty0, tx1, ty1 );
	}
}

void Label::recomputeMinimumSize()
{
	int wide;
	int tall;

	getContentSize( wide, tall );
	setPreferredSize( wide, tall );
	if ( _contentFitted )
	{
		setSize( wide, tall );
	}
}

}
