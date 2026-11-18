#ifndef VGUI_FOCUSCHANGESIGNAL_H
#define VGUI_FOCUSCHANGESIGNAL_H

#include "VGUI.h"

namespace vgui
{

class Panel;

class VGUIAPI FocusChangeSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	// Not deleted by the objects it is added to; lets its owner delete it
	// through this interface.
	virtual ~FocusChangeSignal() {}
#endif

	virtual void focusChanged( bool lost, Panel* panel ) = 0;

	FocusChangeSignal& operator=( const FocusChangeSignal& ) = default;
};

}

#endif
