//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_BUTTONGROUP_H
#define VGUI_BUTTONGROUP_H

#include<VGUI.h>
#include<VGUI_Dar.h>

namespace vgui
{

class Button;

class VGUIAPI ButtonGroup
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	ButtonGroup();
	ButtonGroup(const ButtonGroup& other);
	virtual ~ButtonGroup();
	ButtonGroup& operator=(const ButtonGroup& other);
#endif
	virtual void addButton(Button* button);
	virtual void setSelected(Button* button);
#if VGUI_ABI_COMPAT_MODE != 1
	void         removeButton(Button* button);
#endif
protected:
	Dar<Button*> _buttonDar;
};

}

#endif