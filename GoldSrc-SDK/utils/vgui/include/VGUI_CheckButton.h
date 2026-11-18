//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_CHECKBUTTON_H
#define VGUI_CHECKBUTTON_H

#include<VGUI.h>
#include<VGUI_ToggleButton.h>

namespace vgui
{

class VGUIAPI CheckButton : public ToggleButton
{
public:
	CheckButton(const char* text,int x,int y,int wide,int tall);
	CheckButton(const char* text,int x,int y);
#if VGUI_ABI_COMPAT_MODE != 1
	// Owns the image its constructors give it; a copy gets its own.
	CheckButton(const CheckButton& other);
	virtual ~CheckButton();
	CheckButton& operator=(const CheckButton& other);
#endif
protected:
	virtual void paintBackground();
#if VGUI_ABI_COMPAT_MODE != 1
private:
	Image* _ownImage;
#endif
};

}

#endif