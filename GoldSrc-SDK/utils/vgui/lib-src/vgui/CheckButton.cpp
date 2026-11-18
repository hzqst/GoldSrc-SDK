#include "VGUI_CheckButton.h"
#include "VGUI_Image.h"

namespace
{

using namespace vgui;

class FooCheckImage : public Image
{
public:
	FooCheckImage( CheckButton* checkButton )
	{
		_checkButton = checkButton;
		setSize( 20, 20 );
	}

	virtual void paint( Panel* panel ) override;

protected:
	CheckButton*	_checkButton;
};

void FooCheckImage::paint( Panel* )
{
	drawSetTextFont( Scheme::sf_secondary1 );
	drawSetTextColor( Scheme::sc_white );
	drawPrintChar( 0, 0, 'g' );
	drawSetTextColor( Scheme::sc_secondary2 );
	drawPrintChar( 0, 0, 'c' );
	drawSetTextColor( Scheme::sc_white );
	drawPrintChar( 0, 0, 'd' );
	drawSetTextColor( Scheme::sc_black );
	drawPrintChar( 0, 0, 'e' );
	drawSetTextColor( Scheme::sc_secondary3 );
	drawPrintChar( 0, 0, 'f' );
	drawSetTextColor( Scheme::sc_black );

	if ( _checkButton->isSelected() )
	{
		drawSetTextColor( Scheme::sc_black );
		drawPrintChar( 0, 0, 'a' );
	}
}

}

namespace vgui
{

CheckButton::CheckButton( const char* text, int x, int y, int wide, int tall )
	: ToggleButton( text, x, y, wide, tall )
{
	setTextAlignment( Label::a_east );
#if VGUI_ABI_COMPAT_MODE != 1
	_ownImage = new FooCheckImage( this );
	setImage( _ownImage );
#else
	setImage( new FooCheckImage( this ) );
#endif
}

CheckButton::CheckButton( const char* text, int x, int y ) : ToggleButton( text, x, y )
{
	setTextAlignment( Label::a_east );
#if VGUI_ABI_COMPAT_MODE != 1
	_ownImage = new FooCheckImage( this );
	setImage( _ownImage );
#else
	setImage( new FooCheckImage( this ) );
#endif

	int wide;
	int tall;
	getContentSize( wide, tall );
	setSize( wide, tall );
}

#if VGUI_ABI_COMPAT_MODE != 1
CheckButton::CheckButton( const CheckButton& other ) : ToggleButton( other )
{
	_ownImage = new FooCheckImage( this );
	setImage( _ownImage );
}

// Label does not own its image; free the one the constructor made, whether or
// not it is still shown.
CheckButton::~CheckButton()
{
	delete _ownImage;
}

// Keeps this button's own image.
CheckButton& CheckButton::operator=( const CheckButton& other )
{
	if ( this != &other )
	{
		ToggleButton::operator=( other );
		setImage( _ownImage );
	}

	return *this;
}
#endif

void CheckButton::paintBackground()
{
	int wide;
	int tall;

	getPaintSize( wide, tall );
	drawSetColor( Scheme::sc_secondary3 );
	drawFilledRect( 0, 0, wide, tall );
}

}
