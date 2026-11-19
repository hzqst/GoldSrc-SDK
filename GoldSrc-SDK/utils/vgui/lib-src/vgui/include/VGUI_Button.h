#ifndef VGUI_BUTTON_H
#define VGUI_BUTTON_H

#include "VGUI.h"
#include "VGUI_Label.h"
#include "VGUI_Dar.h"
#include "VGUI_MouseCode.h"

namespace vgui
{

class ButtonGroup;
class ActionSignal;
class ButtonController;

class VGUIAPI Button : public Label
{
#if VGUI_ABI_COMPAT_MODE != 1
	friend class ButtonGroup;
#endif

public:
	Button( const char* text, int x, int y, int wide, int tall );
	Button( const char* text, int x, int y );
#if VGUI_ABI_COMPAT_MODE != 1
	// A Button owns its controller. A copy gets the default controller and is
	// not in any ButtonGroup.
	Button( const Button& other );
	virtual ~Button() override;
#endif

	virtual void setSelected( bool state );
	virtual void setSelectedDirect( bool state );
	virtual void setArmed( bool state );
	virtual bool isSelected();
	virtual void doClick();

	virtual void addActionSignal( ActionSignal* s );
	virtual void setButtonGroup( ButtonGroup* buttonGroup );
	virtual bool isArmed();
	virtual void setButtonBorderEnabled( bool state );

	virtual void setMouseClickEnabled( MouseCode code, bool state );
	virtual bool isMouseClickEnabled( MouseCode code );
	virtual void fireActionSignal();

protected:
	virtual void setButtonController( ButtonController* buttonController );

public:
	virtual Panel* createPropertyPanel() override;

protected:
	virtual void paintBackground() override;

private:
	void init();

protected:
	char*				_text;
	bool				_armed;
	bool				_selected;
	bool				_buttonBorderEnabled;

	// Owns its signals with VGUI_ABI_COMPAT_MODE != 1.
	OwningDar<ActionSignal*>	_actionSignalDar;
	int					_mouseClickMask;
	ButtonGroup*		_buttonGroup;
	ButtonController*	_buttonController;

public:
#if VGUI_ABI_COMPAT_MODE != 1
	Button& operator=( const Button& other );
#else
	Button& operator=( const Button& ) = default;
#endif
};

}

#endif
