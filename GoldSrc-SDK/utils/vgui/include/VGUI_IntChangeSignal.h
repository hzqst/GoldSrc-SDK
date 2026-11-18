//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_INTCHANGESIGNAL_H
#define VGUI_INTCHANGESIGNAL_H

#include<VGUI.h>

namespace vgui
{

class Panel;

class VGUIAPI IntChangeSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	// Sliders and ScrollBars delete their int change signals through this
	// interface.
	virtual ~IntChangeSignal() {}
#endif
	virtual void intChanged(int value,Panel* panel)=0;
};

}

#endif