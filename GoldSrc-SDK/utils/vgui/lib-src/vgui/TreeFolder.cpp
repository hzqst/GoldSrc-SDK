#include "VGUI_TreeFolder.h"
#include "VGUI_InputSignal.h"
#include "VGUI_Label.h"
#include "VGUI_Layout.h"
#include "VGUI_Scheme.h"

namespace
{

using namespace vgui;

class FooTabFolderVerticalLayout : public Layout
{
public:
	FooTabFolderVerticalLayout( int hgap, int vgap ) : Layout()
	{
		_hgap = hgap;
		_vgap = vgap;
	}

	virtual void performLayout( Panel* panel ) override;

protected:
	int	_hgap;
	int	_vgap;
};

class FooTreeFolderDefaultHandler : public InputSignal
{
public:
	FooTreeFolderDefaultHandler( TreeFolder* treeFolder )
	{
		_treeFolder = treeFolder;
	}

	virtual void cursorMoved( int x, int y, Panel* panel ) override;
	virtual void cursorEntered( Panel* panel ) override;
	virtual void cursorExited( Panel* panel ) override;

	virtual void mousePressed( MouseCode code, Panel* panel ) override;
	virtual void mouseDoublePressed( MouseCode code, Panel* panel ) override;
	virtual void mouseReleased( MouseCode code, Panel* panel ) override;
	virtual void mouseWheeled( int delta, Panel* panel ) override;

	virtual void keyPressed( KeyCode code, Panel* panel ) override;
	virtual void keyTyped( KeyCode code, Panel* panel ) override;
	virtual void keyReleased( KeyCode code, Panel* panel ) override;
	virtual void keyFocusTicked( Panel* panel ) override;

protected:
	TreeFolder*	_treeFolder;
};

void FooTabFolderVerticalLayout::performLayout( Panel* panel )
{
	TreeFolder* folder;
	int maxx;
	int wide;
	int tall;
	int xx;
	int yy;

	folder = dynamic_cast<TreeFolder*>( panel );
	maxx = 0;
	yy = 0;
	for ( int i = 0; i < panel->getChildCount(); i++ )
	{
		Panel* child;
		TreeFolder* childFolder;

		child = panel->getChild( i );
		childFolder = dynamic_cast<TreeFolder*>( child );
		if ( childFolder != NULL )
		{
			childFolder->invalidateLayout( true );
		}

		child->getSize( wide, tall );
		if ( i == 0 )
		{
			xx = 0;
		}
		else
		{
			xx = _hgap;
		}

		child->setPos( xx, yy );
		if ( maxx < wide + xx )
		{
			maxx = wide + xx;
		}

		yy += _vgap + tall;
	}

	if ( folder != NULL )
	{
		if ( !folder->isOpened() )
		{
			Panel* child;

			child = panel->getChild( 0 );
			if ( child != NULL )
			{
				child->getSize( wide, tall );
				folder->setSize( wide, tall );
			}
		}
		else
		{
			panel->setSize( maxx + 2, yy );
		}
	}
}

void FooTreeFolderDefaultHandler::cursorMoved( int, int, Panel* )
{
}

void FooTreeFolderDefaultHandler::cursorEntered( Panel* )
{
}

void FooTreeFolderDefaultHandler::cursorExited( Panel* )
{
}

void FooTreeFolderDefaultHandler::mousePressed( MouseCode, Panel* )
{
	_treeFolder->setOpened( !_treeFolder->isOpened() );
}

void FooTreeFolderDefaultHandler::mouseDoublePressed( MouseCode, Panel* )
{
}

void FooTreeFolderDefaultHandler::mouseReleased( MouseCode, Panel* )
{
}

void FooTreeFolderDefaultHandler::mouseWheeled( int, Panel* )
{
}

void FooTreeFolderDefaultHandler::keyPressed( KeyCode, Panel* )
{
}

void FooTreeFolderDefaultHandler::keyTyped( KeyCode, Panel* )
{
}

void FooTreeFolderDefaultHandler::keyReleased( KeyCode, Panel* )
{
}

void FooTreeFolderDefaultHandler::keyFocusTicked( Panel* )
{
}

// Panel does not own its layout, and this one keeps no state between layouts,
// so every TreeFolder shares it instead of making one nothing frees.
Layout* folderLayout()
{
	static FooTabFolderVerticalLayout layout( 30, 3 );
	return &layout;
}

}

namespace vgui
{

void TreeFolder::init( const char* name )
{
	Label* label;

	_opened = false;
	label = new Label( name, 0, 0 );
	label->addInputSignal( new FooTreeFolderDefaultHandler( this ) );
	label->setParent( this );
	setLayout( folderLayout() );
}

TreeFolder::TreeFolder( const char* name ) : Panel( 0, 0, 500, 500 )
{
	init( name );
}

TreeFolder::TreeFolder( const char* name, int x, int y ) : Panel( x, y, 500, 500 )
{
	init( name );
}

void TreeFolder::setOpened( bool state )
{
	TreeFolder* top;
	Panel* trav;

	if ( _opened == state )
	{
		return;
	}

	_opened = state;
	top = NULL;
	trav = this;
	while ( trav != NULL )
	{
		TreeFolder* folder;

		folder = dynamic_cast<TreeFolder*>( trav );
		if ( folder != NULL )
		{
			top = folder;
			folder->invalidateLayout( true );
		}

		trav = trav->getParent();
	}

	if ( top != NULL )
	{
		top->repaintParent();
	}
}

bool TreeFolder::isOpened()
{
	return _opened;
}

void TreeFolder::paintBackground()
{
	int x0;
	int x1;
	int y0;
	int y1;
	int i;

	x0 = 15;
	x1 = 30;
	y0 = 10;
	y1 = y0;
	drawSetColor( Scheme::SchemeColor::sc_black );
	for ( i = 1; i < getChildCount(); i++ )
	{
		Panel* child;
		int x;
		int y;
		int wide;
		int tall;

		child = getChild( i );
		child->getBounds( x, y, wide, tall );
		if ( dynamic_cast<TreeFolder*>( child ) != NULL )
		{
			y += y0;
		}
		else
		{
			y += tall / 2;
		}

		drawFilledRect( x0, y, x1, y + 1 );
		y1 = y;
	}

	drawFilledRect( x0, y0, x0 + 1, y1 );

	for ( i = 1; i < getChildCount(); i++ )
	{
		Panel* child;
		int x;
		int y;
		int wide;
		int tall;
		TreeFolder* folder;

		child = getChild( i );
		child->getBounds( x, y, wide, tall );

		folder = dynamic_cast<TreeFolder*>( child );
		if ( folder == NULL )
		{
			continue;
		}

		y += y0;
		drawSetColor( Scheme::SchemeColor::sc_white );
		drawFilledRect( 10, y - 5, 21, y + 6 );
		drawSetColor( Scheme::SchemeColor::sc_black );
		drawOutlinedRect( 10, y - 5, 21, y + 6 );
		drawFilledRect( 12, y, 19, y + 1 );
		if ( !folder->isOpened() )
		{
			drawFilledRect( x0, y - 3, x0 + 1, y + 4 );
		}
	}
}

void TreeFolder::setOpenedTraverse( bool state )
{
	int i;

	setOpened( true );
	for ( i = 0; i < getChildCount(); i++ )
	{
		Panel* child;
		TreeFolder* folder;

		child = getChild( i );
		folder = dynamic_cast<TreeFolder*>( child );
		if ( folder != NULL )
		{
			folder->setOpenedTraverse( state );
		}
	}
}

}
