#ifndef VGUI_ACTIONSIGNAL_H
#define VGUI_ACTIONSIGNAL_H

#include "VGUI.h"

namespace vgui
{

class Panel;

class VGUIAPI ActionSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	// Buttons and TextEntries delete their action signals through this
	// interface.
	virtual ~ActionSignal() {}
#endif

	virtual void actionPerformed( Panel* panel ) = 0;

	ActionSignal& operator=( const ActionSignal& ) = default;
};

}

#endif
