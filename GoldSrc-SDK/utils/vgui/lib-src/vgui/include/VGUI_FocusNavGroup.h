#ifndef VGUI_FOCUSNAVGROUP_H
#define VGUI_FOCUSNAVGROUP_H

#include "VGUI.h"
#include "VGUI_Dar.h"

namespace vgui
{

class Panel;

class VGUIAPI FocusNavGroup
{
	friend class Panel;

public:
	FocusNavGroup();
#if VGUI_ABI_COMPAT_MODE != 1
	// A panel is in the group's list exactly while it points to the group: it
	// leaves the list when it changes group or is deleted, and a deleted group
	// leaves its panels without a group. A copy starts with no panels.
	FocusNavGroup( const FocusNavGroup& other );
	virtual ~FocusNavGroup();
#endif

protected:
	virtual void addPanel( Panel* panel );
	virtual void requestFocusPrev();
	virtual void requestFocusNext();
	virtual void setCurrentPanel( Panel* panel );
#if VGUI_ABI_COMPAT_MODE != 1
	// Not virtual, so the vtable keeps its original layout.
	void removePanel( Panel* panel );
#endif

protected:
	Dar<Panel*>	_panelDar;
	int			_currentIndex;

public:
#if VGUI_ABI_COMPAT_MODE != 1
	FocusNavGroup& operator=( const FocusNavGroup& other );
#else
	FocusNavGroup& operator=( const FocusNavGroup& ) = default;
#endif
};

}

#endif
