#include "VGUI_RadioButton.h"
#include "VGUI_Image.h"

namespace
{

using namespace vgui;

class FooRadioImage : public Image
{
public:
	FooRadioImage( RadioButton* radioButton )
	{
		_radioButton = radioButton;
		setSize( 20, 20 );
	}

	virtual void paint( Panel* panel ) override;

protected:
	RadioButton*	_radioButton;
};

void FooRadioImage::paint( Panel* )
{
	drawSetTextFont( Scheme::sf_secondary1 );
	drawSetTextColor( Scheme::sc_white );
	drawPrintChar( 0, 0, 'n' );
	drawSetTextColor( Scheme::sc_secondary2 );
	drawPrintChar( 0, 0, 'j' );
	drawSetTextColor( Scheme::sc_white );
	drawPrintChar( 0, 0, 'k' );
	drawSetTextColor( Scheme::sc_black );
	drawPrintChar( 0, 0, 'l' );
	drawSetTextColor( Scheme::sc_secondary3 );
	drawPrintChar( 0, 0, 'm' );

	if ( _radioButton->isSelected() )
	{
		drawSetTextColor( Scheme::sc_black );
		drawPrintChar( 0, 0, 'h' );
	}
}

}

namespace vgui
{

RadioButton::RadioButton( const char* text, int x, int y, int wide, int tall )
	: ToggleButton( text, x, y, wide, tall )
{
	setTextAlignment( Label::a_east );
#if VGUI_ABI_COMPAT_MODE != 1
	_ownImage = new FooRadioImage( this );
	setImage( _ownImage );
#else
	setImage( new FooRadioImage( this ) );
#endif
}

RadioButton::RadioButton( const char* text, int x, int y ) : ToggleButton( text, x, y )
{
	setTextAlignment( Label::a_east );
#if VGUI_ABI_COMPAT_MODE != 1
	_ownImage = new FooRadioImage( this );
	setImage( _ownImage );
#else
	setImage( new FooRadioImage( this ) );
#endif

	int wide;
	int tall;
	getContentSize( wide, tall );
	setSize( wide, tall );
}

#if VGUI_ABI_COMPAT_MODE != 1
RadioButton::RadioButton( const RadioButton& other ) : ToggleButton( other )
{
	_ownImage = new FooRadioImage( this );
	setImage( _ownImage );
}

// Label does not own its image; free the one the constructor made, whether or
// not it is still shown.
RadioButton::~RadioButton()
{
	delete _ownImage;
}

// Keeps this button's own image.
RadioButton& RadioButton::operator=( const RadioButton& other )
{
	if ( this != &other )
	{
		ToggleButton::operator=( other );
		setImage( _ownImage );
	}

	return *this;
}
#endif

void RadioButton::paintBackground()
{
	int wide;
	int tall;

	getPaintSize( wide, tall );
	drawSetColor( Scheme::sc_secondary3 );
	drawFilledRect( 0, 0, wide, tall );
}

}
