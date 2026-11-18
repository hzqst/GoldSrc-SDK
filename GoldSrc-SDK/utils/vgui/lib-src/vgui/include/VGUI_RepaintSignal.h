#ifndef VGUI_REPAINTSIGNAL_H
#define VGUI_REPAINTSIGNAL_H

#include "VGUI.h"

namespace vgui
{

class Panel;

class RepaintSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	// Not deleted by the objects it is added to; lets its owner delete it
	// through this interface.
	virtual ~RepaintSignal() {}
#endif

	virtual void panelRepainted( Panel* panel ) = 0;
};

}

#endif
