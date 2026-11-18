//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_FRAMESIGNAL_H
#define VGUI_FRAMESIGNAL_H

#include<VGUI.h>

namespace vgui
{

class Frame;

class VGUIAPI FrameSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	// Not deleted by the objects it is added to; lets its owner delete it
	// through this interface.
	virtual ~FrameSignal() {}
#endif
	virtual void closing(Frame* frame)=0;
	virtual void minimizing(Frame* frame,bool toTray)=0;
};

}

#endif