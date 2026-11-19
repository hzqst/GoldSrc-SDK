//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_FOCUSNAVGROUP_H
#define VGUI_FOCUSNAVGROUP_H

#include<VGUI.h>
#include<VGUI_Dar.h>

namespace vgui
{

class Panel;

class VGUIAPI FocusNavGroup
{
public:
	FocusNavGroup();
#if VGUI_ABI_COMPAT_MODE != 1
	FocusNavGroup(const FocusNavGroup& other);
	virtual ~FocusNavGroup();
	FocusNavGroup& operator=(const FocusNavGroup& other);
#endif
protected:
	virtual void addPanel(Panel* panel);
	virtual void requestFocusPrev();
	virtual void requestFocusNext();
	virtual void setCurrentPanel(Panel* panel);
#if VGUI_ABI_COMPAT_MODE != 1
	void         removePanel(Panel* panel);
#endif
protected:
	Dar<Panel*> _panelDar;
	int         _currentIndex;
friend class Panel;
};
}

#endif