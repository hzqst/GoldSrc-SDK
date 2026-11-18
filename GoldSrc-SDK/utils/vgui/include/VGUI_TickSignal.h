//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_TICKSIGNAL_H
#define VGUI_TICKSIGNAL_H

#include<VGUI.h>

namespace vgui
{
class VGUIAPI TickSignal
 {
 public:
#if VGUI_ABI_COMPAT_MODE != 1
  // Not deleted by the objects it is added to; lets its owner delete it
  // through this interface.
  virtual ~TickSignal() {}
#endif
  virtual void ticked()=0;
 };
}

#endif