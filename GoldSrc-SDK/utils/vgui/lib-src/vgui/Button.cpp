#include "VGUI_Button.h"
#include "VGUI_ActionSignal.h"
#include "VGUI_ButtonController.h"
#include "VGUI_ButtonGroup.h"
#include "VGUI_InputSignal.h"
#include "VGUI_KeyCode.h"
#include "VGUI_Scheme.h"
#include "VGUI_TreeFolder.h"

namespace
{

using namespace vgui;

class FooDefaultButtonController : public ButtonController, public InputSignal
{
public:
	FooDefaultButtonController( Button* button )
	{
		_button = button;
	}

	virtual void addSignals( Button* button ) override;
	virtual void removeSignals( Button* button ) override;

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
	Button*	_button;
};

void FooDefaultButtonController::addSignals( Button* button )
{
	button->addInputSignal( this );
}

void FooDefaultButtonController::removeSignals( Button* button )
{
	button->removeInputSignal( this );
}

void FooDefaultButtonController::cursorMoved( int, int, Panel* )
{
}

void FooDefaultButtonController::cursorEntered( Panel* )
{
}

void FooDefaultButtonController::cursorExited( Panel* )
{
}

void FooDefaultButtonController::mousePressed( MouseCode code, Panel* )
{
	if ( _button->isEnabled() )
	{
		if ( _button->isMouseClickEnabled( code ) )
		{
			_button->setSelected( true );
			_button->repaint();
		}
	}
}

void FooDefaultButtonController::mouseDoublePressed( MouseCode, Panel* )
{
}

void FooDefaultButtonController::mouseReleased( MouseCode code, Panel* )
{
	if ( _button->isEnabled() )
	{
		if ( _button->isMouseClickEnabled( code ) )
		{
			_button->setSelected( false );
			_button->fireActionSignal();
			_button->repaint();
		}
	}
}

void FooDefaultButtonController::mouseWheeled( int, Panel* )
{
}

void FooDefaultButtonController::keyPressed( KeyCode, Panel* )
{
}

void FooDefaultButtonController::keyTyped( KeyCode, Panel* )
{
}

void FooDefaultButtonController::keyReleased( KeyCode, Panel* )
{
}

void FooDefaultButtonController::keyFocusTicked( Panel* )
{
}

}

namespace vgui
{

void Button::init()
{
	_buttonController = NULL;
	_buttonGroup = NULL;
	_armed = false;
	_selected = false;
	_buttonBorderEnabled = true;
	_mouseClickMask = 0;
	setMouseClickEnabled( MOUSE_LEFT, true );
	setButtonController( new FooDefaultButtonController( this ) );
}

Button::Button( const char* text, int x, int y, int wide, int tall ) : Label( text, x, y, wide, tall )
{
	init();
}

Button::Button( const char* text, int x, int y ) : Label( text, x, y )
{
	init();
}

#if VGUI_ABI_COMPAT_MODE != 1
Button::Button( const Button& other ) : Label( other ), _actionSignalDar( other._actionSignalDar )
{
	_text = NULL;
	_armed = other._armed;
	_selected = other._selected;
	_buttonBorderEnabled = other._buttonBorderEnabled;
	_mouseClickMask = other._mouseClickMask;
	_buttonGroup = NULL;
	_buttonController = NULL;
	setButtonController( new FooDefaultButtonController( this ) );
}

Button::~Button()
{
	// Take the controller's signals off first so that ~Panel does not delete
	// a controller that registered itself as one.
	if ( _buttonController != NULL )
	{
		_buttonController->removeSignals( this );
		delete _buttonController;
	}
}

// Keeps this button's own controller and ButtonGroup.
Button& Button::operator=( const Button& other )
{
	if ( this != &other )
	{
		Label::operator=( other );
		_armed = other._armed;
		_selected = other._selected;
		_buttonBorderEnabled = other._buttonBorderEnabled;
		_actionSignalDar = other._actionSignalDar;
		_mouseClickMask = other._mouseClickMask;
	}

	return *this;
}
#endif

void Button::setButtonBorderEnabled( bool state )
{
	_buttonBorderEnabled = state;
	repaint();
}

void Button::setSelectedDirect( bool state )
{
	_selected = state;
	repaint();
}

void Button::setArmed( bool state )
{
	_armed = state;
	repaint();
}

bool Button::isArmed()
{
	return _armed;
}

bool Button::isSelected()
{
	return _selected;
}

void Button::setButtonGroup( ButtonGroup* buttonGroup )
{
	_buttonGroup = buttonGroup;

	if ( buttonGroup != NULL )
	{
		buttonGroup->addButton( this );
	}
}

bool Button::isMouseClickEnabled( MouseCode code )
{
	return ( _mouseClickMask & ( 1 << ( code + 1 ) ) ) != 0;
}

void Button::fireActionSignal()
{
	for ( int i = 0; i < _actionSignalDar.getCount(); i++ )
	{
		_actionSignalDar[i]->actionPerformed( this );
	}
}

void Button::addActionSignal( ActionSignal* s )
{
	if ( s != NULL )
	{
		_actionSignalDar.putElement( s );
	}
}

void Button::setMouseClickEnabled( MouseCode code, bool state )
{
	if ( state )
	{
		_mouseClickMask |= 1 << ( code + 1 );
	}
	else
	{
		_mouseClickMask &= ~( 1 << ( code + 1 ) );
	}
}

void Button::paintBackground()
{
	int wide;
	int tall;

	getPaintSize( wide, tall );

	if ( isSelected() )
	{
		drawSetColor( Scheme::sc_secondary2 );
		drawFilledRect( 0, 0, wide, tall );
	}
	else
	{
		drawSetColor( Scheme::sc_secondary3 );
		drawFilledRect( 0, 0, wide, tall );

		if ( _buttonBorderEnabled )
		{
			drawSetColor( Scheme::sc_secondary1 );
			drawFilledRect( 0, 0, _size[0] - 1, 1 );
			drawFilledRect( 2, _size[1] - 2, _size[0] - 1, _size[1] - 1 );
			drawFilledRect( 0, 1, 1, _size[1] - 1 );
			drawFilledRect( _size[0] - 2, 2, _size[0] - 1, _size[1] - 2 );

			drawSetColor( Scheme::sc_white );
			drawFilledRect( 1, 1, _size[0] - 2, 2 );
			drawFilledRect( 1, _size[1] - 1, _size[0], _size[1] );
			drawFilledRect( 1, 2, 2, _size[1] - 2 );
			drawFilledRect( _size[0] - 1, 1, _size[0], _size[1] - 1 );
		}
	}

	if ( isArmed() )
	{
		drawSetColor( Scheme::sc_white );
		drawFilledRect( 0, 0, wide, 2 );
		drawFilledRect( 0, 2, 2, tall );

		drawSetColor( Scheme::sc_secondary2 );
		drawFilledRect( 2, tall - 2, wide, tall );
		drawFilledRect( wide - 2, 2, wide, tall - 1 );

		drawSetColor( Scheme::sc_secondary1 );
		drawFilledRect( 1, tall - 1, wide, tall );
		drawFilledRect( wide - 1, 1, wide, tall - 1 );
	}
}

void Button::setSelected( bool state )
{
	if ( _buttonGroup != NULL )
	{
		_buttonGroup->setSelected( this );
	}
	else
	{
		setSelectedDirect( state );
	}
}

void Button::setButtonController( ButtonController* buttonController )
{
	if ( _buttonController != NULL )
	{
		_buttonController->removeSignals( this );
#if VGUI_ABI_COMPAT_MODE != 1
		if ( _buttonController != buttonController )
			delete _buttonController;
#endif
	}

	_buttonController = buttonController;
	buttonController->addSignals( this );
}

void Button::doClick()
{
	setSelected( true );
	fireActionSignal();
	setSelected( false );
}

Panel* Button::createPropertyPanel()
{
	Panel* propertyPanel;
	TreeFolder* folder;

	propertyPanel = Label::createPropertyPanel();
	folder = new TreeFolder( "Button" );
	propertyPanel->addChild( folder );
	folder->addChild( new Label( "setSelected" ) );
	folder->addChild( new Label( "setArmed" ) );
	return propertyPanel;
}

}
