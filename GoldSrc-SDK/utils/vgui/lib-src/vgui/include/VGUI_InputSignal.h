#ifndef VGUI_INPUTSIGNAL_H
#define VGUI_INPUTSIGNAL_H

#include "VGUI.h"
#include "VGUI_KeyCode.h"
#include "VGUI_MouseCode.h"

namespace vgui
{

class Panel;

class VGUIAPI InputSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	// Panels delete their input signals through this interface.
	virtual ~InputSignal() {}
#endif

	virtual void cursorMoved( int x, int y, Panel* panel ) = 0;
	virtual void cursorEntered( Panel* panel ) = 0;
	virtual void cursorExited( Panel* panel ) = 0;

	virtual void mousePressed( MouseCode code, Panel* panel ) = 0;
	virtual void mouseDoublePressed( MouseCode code, Panel* panel ) = 0;
	virtual void mouseReleased( MouseCode code, Panel* panel ) = 0;
	virtual void mouseWheeled( int delta, Panel* panel ) = 0;

	virtual void keyPressed( KeyCode code, Panel* panel ) = 0;
	virtual void keyTyped( KeyCode code, Panel* panel ) = 0;
	virtual void keyReleased( KeyCode code, Panel* panel ) = 0;
	virtual void keyFocusTicked( Panel* panel ) = 0;

	InputSignal& operator=( const InputSignal& ) = default;
};

}

#endif
