#include "VGUI_ListPanel.h"

#include "VGUI_IntChangeSignal.h"
#include "VGUI_Label.h"
#include "VGUI_ScrollBar.h"
#include "VGUI_StackLayout.h"

namespace
{

using namespace vgui;

class FooDefaultListPanelSignal : public IntChangeSignal
{
public:
	FooDefaultListPanelSignal( ListPanel* listPanel )
	{
		_listPanel = listPanel;
	}

	virtual void intChanged( int value, Panel* panel ) override;

protected:
	ListPanel*	_listPanel;
};

void FooDefaultListPanelSignal::intChanged( int value, Panel* )
{
	_listPanel->setPixelScroll( -value );
}

// Panel does not own its layout, and a StackLayout keeps no state between
// layouts, so every ListPanel shares this one instead of making one nothing
// frees.
Layout* itemLayout()
{
	static StackLayout layout( 1, false );
	return &layout;
}

}

namespace vgui
{

ListPanel::ListPanel( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	_vpanel = new Panel( 0, 0, wide - 15, tall * 2 );
	_vpanel->setParent( this );
	_vpanel->setLayout( itemLayout() );
	_vpanel->setBgColor( 0, 0, 100, 0 );

	_scroll = new ScrollBar( wide - 15, 0, 15, tall, true );
	_scroll->setParent( this );
	_scroll->addIntChangeSignal( new FooDefaultListPanelSignal( this ) );
}

void ListPanel::addItem( Panel* panel )
{
	int x;
	int y;
	int wide;
	int tall;
	int vwide;
	int vtall;

	panel->setParent( _vpanel );
	_vpanel->invalidateLayout( true );
	_vpanel->getChild( _vpanel->getChildCount() - 1 )->getBounds( x, y, wide, tall );
	_vpanel->getSize( vwide, vtall );
	_vpanel->setSize( vwide, y + tall );
	_scroll->setRange( 0, y + tall - _size[1] );
}

void ListPanel::performLayout()
{
	int x;
	int y;
	int wide;
	int tall;
	int vwide;
	int vtall;

	Panel* child = _vpanel->getChild( _vpanel->getChildCount() - 1 );
	if ( child != NULL )
	{
		child->getBounds( x, y, wide, tall );
		_vpanel->getSize( vwide, vtall );
		_vpanel->setSize( _size[0] - 15, vtall );
		_scroll->setBounds( _size[0] - 15, 0, 15, _size[1] );
		_scroll->setRange( 0, y + tall - _size[1] );
	}
}

void ListPanel::setPixelScroll( int value )
{
	_vpanel->setPos( 0, value );
	repaint();
	_vpanel->repaint();
}

void ListPanel::setSize( int wide, int tall )
{
	Panel::setSize( wide, tall );
	invalidateLayout( false );
}

void ListPanel::translatePixelScroll( int delta )
{
	int x;
	int y;

	_vpanel->getPos( x, y );
	_vpanel->setPos( 0, y + delta );
	repaint();
	_vpanel->repaint();
}

void ListPanel::addString( const char* str )
{
	addItem( new Label( str, 0, 0, 80, 20 ) );
}

void ListPanel::paintBackground()
{
	Panel::paintBackground();
}

}
