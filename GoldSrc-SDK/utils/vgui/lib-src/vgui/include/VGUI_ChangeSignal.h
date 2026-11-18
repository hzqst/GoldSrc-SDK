#ifndef VGUI_CHANGESIGNAL_H
#define VGUI_CHANGESIGNAL_H

#include "VGUI.h"

namespace vgui
{

class Panel;

class VGUIAPI ChangeSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	// HeaderPanels delete their change signals through this interface.
	virtual ~ChangeSignal() {}
#endif

	virtual void valueChanged( Panel* panel ) = 0;

	ChangeSignal& operator=( const ChangeSignal& ) = default;
};

}

#endif
