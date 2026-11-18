#ifndef VGUI_DEFAULTBORDER_H
#define VGUI_DEFAULTBORDER_H

#include "VGUI.h"

namespace vgui
{

class Border;

// The border a built-in widget gives a panel it makes. With
// VGUI_ABI_COMPAT_MODE == 1 a Panel never frees its border, and these borders
// keep no state but the inset and color their constructor sets, so every
// caller shares one TBorder. Otherwise ~Panel deletes the border, so each call
// makes a new one.
template <class TBorder>
Border* defaultBorder()
{
#if VGUI_ABI_COMPAT_MODE == 1
	static TBorder border;
	return &border;
#else
	return new TBorder();
#endif
}

}

#endif
