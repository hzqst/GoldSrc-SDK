#include "VGUI_ToggleButton.h"
#include "VGUI_ButtonController.h"
#include "VGUI_InputSignal.h"

namespace
{

using namespace vgui;

class FooDefaultToggleButtonController : public ButtonController, public InputSignal
{
public:
	FooDefaultToggleButtonController( ToggleButton* button )
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
	ToggleButton*	_button;
};

void FooDefaultToggleButtonController::addSignals( Button* button )
{
	button->addInputSignal( this );
}

void FooDefaultToggleButtonController::removeSignals( Button* button )
{
	button->removeInputSignal( this );
}

void FooDefaultToggleButtonController::cursorMoved( int, int, Panel* )
{
}

void FooDefaultToggleButtonController::cursorEntered( Panel* )
{
}

void FooDefaultToggleButtonController::cursorExited( Panel* )
{
}

void FooDefaultToggleButtonController::mousePressed( MouseCode, Panel* )
{
	_button->setSelected( !_button->isSelected() );
	_button->fireActionSignal();
	_button->repaint();
}

void FooDefaultToggleButtonController::mouseDoublePressed( MouseCode, Panel* )
{
}

void FooDefaultToggleButtonController::mouseReleased( MouseCode, Panel* )
{
}

void FooDefaultToggleButtonController::mouseWheeled( int, Panel* )
{
}

void FooDefaultToggleButtonController::keyPressed( KeyCode, Panel* )
{
}

void FooDefaultToggleButtonController::keyTyped( KeyCode, Panel* )
{
}

void FooDefaultToggleButtonController::keyReleased( KeyCode, Panel* )
{
}

void FooDefaultToggleButtonController::keyFocusTicked( Panel* )
{
}

}

namespace vgui
{

ToggleButton::ToggleButton( const char* text, int x, int y, int wide, int tall )
	: Button( text, x, y, wide, tall )
{
	setButtonController( new FooDefaultToggleButtonController( this ) );
}

ToggleButton::ToggleButton( const char* text, int x, int y ) : Button( text, x, y )
{
	setButtonController( new FooDefaultToggleButtonController( this ) );
}

#if VGUI_ABI_COMPAT_MODE != 1
ToggleButton::ToggleButton( const ToggleButton& other ) : Button( other )
{
	setButtonController( new FooDefaultToggleButtonController( this ) );
}
#endif

}
