#include "VGUI_Menu.h"
#include "VGUI_RaisedBorder.h"
#include "VGUI_StackLayout.h"

using namespace vgui;

namespace
{

// Panel does not own its layout, and a StackLayout keeps no state between
// layouts, so every Menu shares this one instead of making one nothing frees.
Layout* menuLayout()
{
	static StackLayout layout( 1, true );
	return &layout;
}

}

void Menu::addMenuItem( Panel* panel )
{
	addChild( panel );
}

Menu::Menu( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	setBorder( new RaisedBorder() );
	setLayout( menuLayout() );
}

Menu::Menu( int wide, int tall ) : Panel( 0, 0, wide, tall )
{
	setBorder( new RaisedBorder() );
	setLayout( menuLayout() );
}
