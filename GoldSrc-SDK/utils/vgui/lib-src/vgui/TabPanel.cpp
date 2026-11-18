#include "VGUI_TabPanel.h"
#include "ActionSignalForwarder.h"

#include "VGUI_ActionSignal.h"
#include "VGUI_Border.h"
#include "VGUI_Button.h"
#include "VGUI_ButtonGroup.h"
#include "VGUI_ToggleButton.h"

using namespace vgui;

namespace
{

class FooClientBorder : public Border
{
public:
	virtual void paintBorder( Panel* panel );
};

class FooTabAreaBorder : public Border
{
public:
	virtual void paintBorder( Panel* panel );
};

class FooTabPanelTab : public ToggleButton, public ActionSignal
{
public:
	FooTabPanelTab( TabPanel* tabPanel, const char* text ) : ToggleButton( text, 0, 0 ), _tabPanel( tabPanel )
	{
		addActionSignal( new ActionSignalForwarder( this ) );
	}

	virtual void paintBackground() override;
	virtual bool isWithin( int x, int y ) override;
	virtual void actionPerformed( Panel* panel ) override;

private:
	TabPanel*	_tabPanel;
};

void FooClientBorder::paintBorder( Panel* panel )
{
	int wide;
	int tall;

	panel->getSize( wide, tall );
	drawSetColor( Scheme::sc_white );
	drawFilledRect( 0, 0, wide - 1, 1 );
	drawFilledRect( 0, 1, 1, tall - 1 );
	drawSetColor( Scheme::sc_secondary1 );
	drawFilledRect( 0, tall - 1, wide, tall );
	drawFilledRect( wide - 1, 0, wide, tall - 1 );
}

void FooTabAreaBorder::paintBorder( Panel* panel )
{
	int wide;
	int tall;

	panel->getSize( wide, tall );
	drawSetColor( Scheme::sc_white );
	drawFilledRect( 0, tall - 1, wide, tall );
}

void FooTabPanelTab::actionPerformed( Panel* )
{
	_tabPanel->setSelectedTab( this );
}

void FooTabPanelTab::paintBackground()
{
	int wide;
	int tall;

	getPaintSize( wide, tall );
	if ( isSelected() )
	{
		drawSetColor( Scheme::sc_secondary3 );
		drawFilledRect( 0, 0, wide, tall );
		drawSetColor( Scheme::sc_secondary1 );
		drawFilledRect( 0, 0, wide, 1 );
		drawFilledRect( 0, 1, 1, tall - 1 );
		drawFilledRect( wide - 1, 1, wide, tall - 1 );
		drawSetColor( Scheme::sc_white );
		drawFilledRect( 1, 1, wide - 1, 2 );
		drawFilledRect( 1, 2, 2, tall );
		drawFilledRect( 0, tall - 1, 1, tall );
		drawFilledRect( wide - 1, tall - 1, wide, tall );
	}
	else
	{
		drawSetColor( Scheme::sc_secondary2 );
		drawFilledRect( 0, 0, wide, tall );
		drawSetColor( Scheme::sc_secondary1 );
		drawFilledRect( 0, 0, wide, 1 );
		drawFilledRect( 0, 1, 1, tall - 1 );
		drawFilledRect( wide - 1, 1, wide, tall - 1 );
		drawSetColor( Scheme::sc_secondary3 );
		drawFilledRect( 1, 1, wide - 1, 2 );
		drawFilledRect( 1, 2, 2, tall );
		drawFilledRect( 0, tall - 1, 1, tall );
		drawFilledRect( wide - 1, tall - 1, wide, tall );
		drawSetColor( Scheme::sc_white );
		drawFilledRect( 0, tall - 1, wide, tall );
	}
}

bool FooTabPanelTab::isWithin( int x, int y )
{
	return Panel::isWithin( x, y );
}

}

TabPanel::TabPanel( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	_tabPlacement = tp_top;

	_clientArea = new Panel( 5, 5, wide - 10, tall - 10 );
	_clientArea->setParent( this );
	_clientArea->setBorder( new FooClientBorder() );

	_tabArea = new Panel( 5, 5, wide, 5 );
	_tabArea->setParent( this );
	_tabArea->setBorder( new FooTabAreaBorder() );

	_selectedPanel = NULL;
	_selectedTab = NULL;
	_buttonGroup = new ButtonGroup();
}

#if VGUI_ABI_COMPAT_MODE != 1
TabPanel::TabPanel( const TabPanel& other ) : Panel( other )
{
	_buttonGroup = new ButtonGroup();
	copyFrom( other );
}

// ButtonGroup has no virtual destructor; this is the ButtonGroup the
// constructor made.
TabPanel::~TabPanel()
{
	delete _buttonGroup;
}

// Keeps this panel's own ButtonGroup.
TabPanel& TabPanel::operator=( const TabPanel& other )
{
	if ( this != &other )
	{
		Panel::operator=( other );
		copyFrom( other );
	}

	return *this;
}

// Like the original copy, the tab and client areas stay shared.
void TabPanel::copyFrom( const TabPanel& other )
{
	_tabPlacement = other._tabPlacement;
	_tabArea = other._tabArea;
	_clientArea = other._clientArea;
	_selectedTab = other._selectedTab;
	_selectedPanel = other._selectedPanel;
}
#endif

void TabPanel::setSize( int wide, int tall )
{
	Panel::setSize( wide, tall );
	recomputeLayout();
}

Panel* TabPanel::addTab( const char* text )
{
	FooTabPanelTab* tab = new FooTabPanelTab( this, text );
	_tabArea->insertChildAt( tab, 0 );
	tab->setButtonGroup( _buttonGroup );

	const int initialClientSize = 20;
	Panel* client = new Panel( 0, 0, initialClientSize, initialClientSize );
	_clientArea->insertChildAt( client, 0 );
	client->setVisible( false );
	if ( _selectedTab == NULL )
	{
		_selectedTab = tab;
		_selectedPanel = client;
		client->setVisible( true );
		tab->setSelected( true );
	}

	recomputeLayout();
	return client;
}

void TabPanel::recomputeLayout()
{
	if ( _tabArea->getChildCount() != 0 )
	{
		if ( _tabPlacement == tp_top )
		{
			recomputeLayoutTop();
		}
		repaint();
	}
}

void TabPanel::setSelectedTab( Panel* tab )
{
	int i;

	if ( tab != NULL && tab != _selectedTab )
	{
		for ( i = 0; i < _tabArea->getChildCount(); ++i )
		{
			if ( tab == _tabArea->getChild( i ) )
			{
				_selectedPanel->setVisible( false );
				_selectedPanel = _clientArea->getChild( i );
				_selectedPanel->setVisible( true );
				_selectedTab = tab;
				break;
			}
		}
		recomputeLayout();
	}
}

void TabPanel::recomputeLayoutTop()
{
	const int margin = 5;
	const int horizontalOverlap = 1;
	const int verticalOverlap = 4;
	int paintWide;
	int paintTall;
	getPaintSize( paintWide, paintTall );

	int x = margin;
	int y = 0;
	int right = paintWide - margin;
	int rowCount = 0;
	for ( int index = _tabArea->getChildCount() - 1; index >= 0; --index )
	{
		int wide;
		int tall;
		_tabArea->getChild( index )->getPreferredSize( wide, tall );
		_tabArea->getChild( index )->setSize( wide, tall );
		if ( x + wide <= right )
		{
			++rowCount;
		}
		else
		{
			if ( rowCount != 0 )
			{
				const int extra = (right - x) / rowCount;
				const int remainder = (right - x) % rowCount;
				const int rowEnd = rowCount + index;
				int shift = 0;
				for ( int column = rowEnd; column > index; --column )
				{
					const int add = extra + ( column == rowEnd ? remainder : 0 );
					int childWide;
					int childTall;
					int childX;
					int childY;
					_tabArea->getChild( column )->getPreferredSize( childWide, childTall );
					_tabArea->getChild( column )->getPos( childX, childY );
					_tabArea->getChild( column )->setBounds( childX + shift, childY, childWide + add, childTall );
					shift += add;
				}
			}
			rowCount = 1;
			x = margin;
			right -= margin;
			y -= tall - verticalOverlap;
		}
		_tabArea->getChild( index )->setPos( x, y );
		x += wide - horizontalOverlap;
	}

	int baseY;
	_tabArea->getChild( 0 )->getPos( x, baseY );
	for ( int index = 0; index < _tabArea->getChildCount(); ++index )
	{
		int childX;
		int childY;
		_tabArea->getChild( index )->getPos( childX, childY );
		_tabArea->getChild( index )->setPos( childX, childY - baseY );
	}

	int wide;
	int tall;
	_tabArea->getChild( 0 )->getSize( wide, tall );
	_tabArea->setBounds( 0, margin, paintWide, tall - baseY );
	_tabArea->getSize( wide, tall );
	_clientArea->setBounds( 0, tall + verticalOverlap, paintWide, paintTall - tall - margin );

	_clientArea->getSize( wide, tall );
	for ( int index = 0; index < _clientArea->getChildCount(); ++index )
	{
		_clientArea->getChild( index )->setBounds( margin, margin, wide - 2 * margin, tall - 2 * margin );
		_clientArea->getChild( index )->invalidateLayout( false );
	}
}
