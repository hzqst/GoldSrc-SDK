//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_BUTTONCONTROLLER_H
#define VGUI_BUTTONCONTROLLER_H

#include<VGUI.h>

namespace vgui
{

class Button;

class VGUIAPI ButtonController
{
public:	
#if VGUI_ABI_COMPAT_MODE != 1
	// Buttons delete their controller through this interface.
	virtual ~ButtonController() {}
#endif
	virtual void addSignals(Button* button)=0;
	virtual void removeSignals(Button* button)=0;
};

}

#endif
