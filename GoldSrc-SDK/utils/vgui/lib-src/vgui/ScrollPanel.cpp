#include "VGUI_ScrollPanel.h"
#include "VGUI_ScrollBar.h"
#include "VGUI_IntChangeSignal.h"

#if VGUI_ABI_COMPAT_MODE == 1
#include <type_traits>
#endif

#define SCROLLBAR_THICKNESS	16

namespace
{

using namespace vgui;

class ChangeHandler : public IntChangeSignal
{
public:
	ChangeHandler( ScrollPanel* scrollPanel ) : _scrollPanel( scrollPanel )
	{
	}

	virtual void intChanged( int, Panel* ) override
	{
		_scrollPanel->recomputeScroll();
	}

protected:
	ScrollPanel*	_scrollPanel;
};

#if VGUI_ABI_COMPAT_MODE == 1
static_assert( std::is_trivially_destructible<ChangeHandler>::value,
	"VGUI_ABI_COMPAT_MODE == 1 teardown releases ChangeHandler storage directly" );
#endif

}

namespace vgui
{

ScrollPanel::ScrollPanel( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	setPaintBorderEnabled( true );
	setPaintBackgroundEnabled( false );
	setPaintEnabled( false );

	_clientClip = new Panel( 0, 0, wide - SCROLLBAR_THICKNESS, tall - SCROLLBAR_THICKNESS );
	_clientClip->setParent( this );
	_clientClip->setBgColor( Color( 0, 128, 0, 0 ) );
	_clientClip->setPaintBorderEnabled( true );
	_clientClip->setPaintBackgroundEnabled( false );
	_clientClip->setPaintEnabled( false );

	_client = new Panel( 0, 0, 2 * wide, 2 * tall );
	_client->setParent( _clientClip );
	_client->setPaintBorderEnabled( true );
	_client->setPaintBackgroundEnabled( false );
	_client->setPaintEnabled( false );

	_horizontalScrollBar = new ScrollBar( 0, tall - SCROLLBAR_THICKNESS,
		wide - SCROLLBAR_THICKNESS, SCROLLBAR_THICKNESS, false );
	_horizontalScrollBar->setParent( this );
	_horizontalScrollBar->addIntChangeSignal( new ChangeHandler( this ) );
	_horizontalScrollBar->setVisible( false );

	_verticalScrollBar = new ScrollBar( wide - SCROLLBAR_THICKNESS, 0,
		SCROLLBAR_THICKNESS, tall - SCROLLBAR_THICKNESS, true );
	_verticalScrollBar->setParent( this );
	_verticalScrollBar->addIntChangeSignal( new ChangeHandler( this ) );
	_verticalScrollBar->setVisible( false );

	_autoVisible[0] = true;
	_autoVisible[1] = true;
	validate();
}

void ScrollPanel::setScrollBarVisible( bool horizontal, bool vertical )
{
	_horizontalScrollBar->setVisible( horizontal );
	_verticalScrollBar->setVisible( vertical );
	validate();
}

Panel* ScrollPanel::getClient()
{
	return _client;
}

Panel* ScrollPanel::getClientClip()
{
	return _clientClip;
}

void ScrollPanel::setScrollBarAutoVisible( bool horizontal, bool vertical )
{
	_autoVisible[0] = horizontal;
	_autoVisible[1] = vertical;
	validate();
}

void ScrollPanel::setScrollValue( int horizontal, int vertical )
{
	_horizontalScrollBar->setValue( horizontal );
	_verticalScrollBar->setValue( vertical );
	recomputeScroll();
}

void ScrollPanel::getScrollValue( int& horizontal, int& vertical )
{
	horizontal = _horizontalScrollBar->getValue();
	vertical = _verticalScrollBar->getValue();
}

ScrollBar* ScrollPanel::getHorizontalScrollBar()
{
	return _horizontalScrollBar;
}

ScrollBar* ScrollPanel::getVerticalScrollBar()
{
	return _verticalScrollBar;
}

void ScrollPanel::recomputeScroll()
{
	int horizontal;
	int vertical;
	getScrollValue( horizontal, vertical );
	_client->setPos( -horizontal, -vertical );
	_clientClip->repaint();
}

void ScrollPanel::setSize( int wide, int tall )
{
	Panel::setSize( wide, tall );
	getPaintSize( wide, tall );

	if ( _autoVisible[0] )
	{
		_horizontalScrollBar->setVisible( !_horizontalScrollBar->hasFullRange() );
	}
	if ( _autoVisible[1] )
	{
		_verticalScrollBar->setVisible( !_verticalScrollBar->hasFullRange() );
	}
	if ( _verticalScrollBar->isVisible() )
	{
		wide -= _verticalScrollBar->getWide();
	}
	if ( _horizontalScrollBar->isVisible() )
	{
		tall -= _horizontalScrollBar->getTall();
	}

	_verticalScrollBar->setBounds( wide, 0, _verticalScrollBar->getWide(), tall );
	_horizontalScrollBar->setBounds( 0, tall, wide, _horizontalScrollBar->getTall() );
	_clientClip->setSize( wide, tall );
	recomputeClientSize();
	repaint();
}

void ScrollPanel::recomputeClientSize()
{
	int wide = 0;
	int tall = 0;
	for ( int i = 0; i < _client->getChildCount(); ++i )
	{
		Panel* child = _client->getChild( i );
		if ( child->isVisible() )
		{
			int x;
			int y;
			int childWide;
			int childTall;
			child->getPos( x, y );
			child->getVirtualSize( childWide, childTall );
			x += childWide;
			y += childTall;
			if ( x > wide )
			{
				wide = x;
			}
			if ( y > tall )
			{
				tall = y;
			}
		}
	}
	_client->setSize( wide, tall );
	const int clipWide = _clientClip->getWide();
	_horizontalScrollBar->setRange( 0, _client->getWide() - clipWide );
	_horizontalScrollBar->setRangeWindow( _client->getWide() );
	const int clipTall = _clientClip->getTall();
	_verticalScrollBar->setRange( 0, _client->getTall() - clipTall );
	_verticalScrollBar->setRangeWindow( _client->getTall() );
}

void ScrollPanel::validate()
{
	_horizontalScrollBar->setRangeWindowEnabled( true );
	_verticalScrollBar->setRangeWindowEnabled( true );
	int wide;
	int tall;
	getSize( wide, tall );

	setSize( wide, tall );
	setSize( wide, tall );
	setSize( wide, tall );
	setSize( wide, tall );
}

}
