#ifndef VGUI_COMPAT_TEARDOWN_H
#define VGUI_COMPAT_TEARDOWN_H

#include <VGUI.h>
#include "vgui_compat_cleanup.h"

#if VGUI_ABI_COMPAT_MODE == 1

namespace vgui
{
class Bitmap;
class Panel;
}

void VGuiCompat_CleanupPanelMembers(vgui::Panel *panel);
void VGuiCompat_DeletePanelTree(vgui::Panel *panel);

#endif // VGUI_ABI_COMPAT_MODE == 1

#endif // VGUI_COMPAT_TEARDOWN_H
