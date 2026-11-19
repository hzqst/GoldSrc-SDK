#ifndef VGUI_BUTTONGROUP_H
#define VGUI_BUTTONGROUP_H

#include "VGUI.h"
#include "VGUI_Dar.h"

namespace vgui
{

class Button;

class VGUIAPI ButtonGroup
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	// A button is in the group's list exactly while it points to the group: it
	// leaves the list when it changes group or is deleted, and a deleted group
	// leaves its buttons without a group. The group does not own its buttons.
	// A copy starts with no buttons.
	ButtonGroup();
	ButtonGroup( const ButtonGroup& other );
	virtual ~ButtonGroup();
#endif

	virtual void addButton( Button* button );
	virtual void setSelected( Button* button );
#if VGUI_ABI_COMPAT_MODE != 1
	// Not virtual, so the vtable keeps its original layout.
	void removeButton( Button* button );
#endif

protected:
	Dar<Button*>	_buttonDar;

public:
#if VGUI_ABI_COMPAT_MODE != 1
	ButtonGroup& operator=( const ButtonGroup& other );
#else
	ButtonGroup& operator=( const ButtonGroup& ) = default;
#endif
};

}

#endif
