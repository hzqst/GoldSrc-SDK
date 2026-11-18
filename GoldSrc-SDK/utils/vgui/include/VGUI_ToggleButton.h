//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_TOGGLEBUTTON_H
#define VGUI_TOGGLEBUTTON_H

#include<VGUI.h>
#include<VGUI_Button.h>

namespace vgui
{

class VGUIAPI ToggleButton : public Button
{
public:
	ToggleButton(const char* text,int x,int y,int wide,int tall);
	ToggleButton(const char* text,int x,int y);
#if VGUI_ABI_COMPAT_MODE != 1
	// A copy gets the toggle controller, not Button's default one.
	ToggleButton(const ToggleButton& other);
#endif
};

}

#endif