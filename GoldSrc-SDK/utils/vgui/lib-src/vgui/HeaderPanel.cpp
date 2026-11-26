#include "VGUI_HeaderPanel.h"

#include "VGUI_App.h"
#include "VGUI_ChangeSignal.h"
#include "VGUI_InputSignal.h"
#include "VGUI_KeyCode.h"
#include "VGUI_Scheme.h"

#if VGUI_ABI_COMPAT_MODE == 1
#include <type_traits>
#endif

namespace
{

using namespace vgui;

class HeaderPanelSignal : public InputSignal
{
public:
	HeaderPanelSignal( HeaderPanel* headerPanel )
	{
		_headerPanel = headerPanel;
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
	HeaderPanel*	_headerPanel;
};

#if VGUI_ABI_COMPAT_MODE == 1
static_assert( std::is_trivially_destructible<HeaderPanelSignal>::value,
	"VGUI_ABI_COMPAT_MODE == 1 teardown releases HeaderPanelSignal storage directly" );
#endif

void HeaderPanelSignal::cursorMoved( int x, int y, Panel* panel )
{
	_headerPanel->privateCursorMoved( x, y, panel );
}

void HeaderPanelSignal::cursorEntered( Panel* )
{
}

void HeaderPanelSignal::cursorExited( Panel* )
{
}

void HeaderPanelSignal::mousePressed( MouseCode code, Panel* panel )
{
	_headerPanel->privateMousePressed( code, panel );
}

void HeaderPanelSignal::mouseDoublePressed( MouseCode, Panel* )
{
}

void HeaderPanelSignal::mouseReleased( MouseCode code, Panel* panel )
{
	_headerPanel->privateMouseReleased( code, panel );
}

void HeaderPanelSignal::mouseWheeled( int, Panel* )
{
}

void HeaderPanelSignal::keyPressed( KeyCode, Panel* )
{
}

void HeaderPanelSignal::keyTyped( KeyCode, Panel* )
{
}

void HeaderPanelSignal::keyReleased( KeyCode, Panel* )
{
}

void HeaderPanelSignal::keyFocusTicked( Panel* )
{
}

}

namespace vgui
{

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
HeaderPanel::HeaderPanel( int x, int y, int wide, int tall ) : Panel( x, y, wide, tall )
{
	_sliderWide = 11;
	_dragging = false;
	_sectionLayer = new Panel( 0, 0, wide, tall );
	_sectionLayer->setPaintBorderEnabled( false );
	_sectionLayer->setPaintBackgroundEnabled( false );
	_sectionLayer->setPaintEnabled( false );
	_sectionLayer->setParent( this );
}

void HeaderPanel::performLayout()
{
	int wide;
	int tall;

	getPaintSize( wide, tall );
	_sectionLayer->setBounds( 0, 0, wide, tall );

	if ( _sectionPanelDar.getCount() > 0 )
	{
		int x = 0;

		for ( int i = 0; i < _sectionPanelDar.getCount(); i++ )
		{
			int sx;
			int sy;
			_sliderPanelDar[i]->getPos( sx, sy );

			int x1 = sx + _sliderWide / 2;
			_sectionPanelDar[i]->setBounds( x, 0, x1 - x, tall );
			x = x1;
		}
	}
}

void HeaderPanel::addSectionPanel( Panel* panel )
{
	int x;
	int y = 0;
	int wide = 0;
	int tall = 0;

	invalidateLayout( true );

	x = 0;
	for ( int i = 0; i < _sectionPanelDar.getCount(); i++ )
	{
		_sectionPanelDar[i]->getBounds( x, y, wide, tall );
		x += _sliderWide + wide;
	}

	_sectionPanelDar.addElement( panel );
	panel->setPos( x, 0 );
	panel->setParent( _sectionLayer );
	panel->setBounds( x, y, wide, tall );

	getPaintSize( wide, tall );

	Panel* slider = new Panel( 0, 0, _sliderWide, tall );
	slider->setPaintBorderEnabled( false );
	slider->setPaintBackgroundEnabled( false );
	slider->setPaintEnabled( false );
	slider->setPos( x + wide, 0 );
	slider->addInputSignal( new HeaderPanelSignal( this ) );
	slider->setCursor( getApp()->getScheme()->getCursor( Scheme::scu_sizewe ) );
	slider->setParent( this );
	_sliderPanelDar.addElement( slider );

	invalidateLayout( false );
	fireChangeSignal();
	repaint();
}

void HeaderPanel::setSliderPos( int sliderIndex, int pos )
{
	_sliderPanelDar[sliderIndex]->setPos( pos - _sliderWide / 2, 0 );
	invalidateLayout( false );
	fireChangeSignal();
	repaint();
}

int HeaderPanel::getSectionCount()
{
	return _sectionPanelDar.getCount();
}

void HeaderPanel::getSectionExtents( int sectionIndex, int& x0, int& x1 )
{
	int x;
	int y;
	int wide;
	int tall;

	_sectionPanelDar[sectionIndex]->getBounds( x, y, wide, tall );
	x0 = x;
	x1 = wide + x;
}

void HeaderPanel::addChangeSignal( ChangeSignal* s )
{
	_changeSignalDar.putElement( s );
}

void HeaderPanel::fireChangeSignal()
{
	invalidateLayout( true );

	for ( int i = 0; i < _changeSignalDar.getCount(); i++ )
	{
		_changeSignalDar[i]->valueChanged( this );
	}
}

void HeaderPanel::privateCursorMoved( int x, int y, Panel* )
{
	if ( _dragging )
	{
		getApp()->getCursorPos( x, y );
		screenToLocal( x, y );
		setSliderPos( _dragSliderIndex, _dragSliderStartPos + x - _dragSliderStartX );
		invalidateLayout( false );
		repaint();
	}
}

void HeaderPanel::privateMousePressed( MouseCode, Panel* panel )
{
	int mx;
	int my;

	getApp()->getCursorPos( mx, my );
	screenToLocal( mx, my );

	for ( int i = 0; i < _sliderPanelDar.getCount(); i++ )
	{
		if ( panel == _sliderPanelDar[i] )
		{
			int x;
			int y;
			panel->getPos( x, y );
			_dragging = true;
			_dragSliderIndex = i;
			_dragSliderStartPos = x + _sliderWide / 2;
			_dragSliderStartX = mx;
			panel->setAsMouseCapture( true );
			return;
		}
	}
}

void HeaderPanel::privateMouseReleased( MouseCode, Panel* panel )
{
	_dragging = false;
	panel->setAsMouseCapture( false );
}

}
