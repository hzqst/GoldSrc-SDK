//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: VGUI1 ABI-compat teardown helpers for game_shared controls.
//
//=============================================================================

#ifndef VGUI_COMPAT_CLEANUP_H
#define VGUI_COMPAT_CLEANUP_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI.h>
#include <VGUI_CompatAccess.h>

#if VGUI_ABI_COMPAT_MODE == 1

namespace vgui
{

template <class T>
inline void VGuiGameShared_ReleaseDarStorage(Dar<T>& dar)
{
	VGuiCompatAccess::releaseDarStorage(dar);
}

inline void VGuiGameShared_ReleaseBitmapPixels(Bitmap* bitmap)
{
	VGuiCompatAccess::releaseBitmapPixels(bitmap);
}

inline void VGuiGameShared_CleanupPanelStorage(Panel* panel)
{
	VGuiCompatAccess::cleanupPanelStorage(panel);
}

inline void VGuiGameShared_CleanupLabelMembers(Label* label)
{
	if (label == NULL)
		return;

	VGuiCompatAccess::cleanupLabel(label);
	VGuiCompatAccess::cleanupPanelStorage(label);
}

inline void VGuiGameShared_CleanupButtonMembers(Button* button)
{
	if (button == NULL)
		return;

	VGuiCompatAccess::cleanupButton(button);
	VGuiGameShared_CleanupLabelMembers(button);
}

} // namespace vgui

#endif // VGUI_ABI_COMPAT_MODE == 1

#endif // VGUI_COMPAT_CLEANUP_H
