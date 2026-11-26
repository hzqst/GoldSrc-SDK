#ifndef VGUI_COMPAT_PANEL_ACCESS_H
#define VGUI_COMPAT_PANEL_ACCESS_H

#include <VGUI.h>

#if VGUI_ABI_COMPAT_MODE == 1
#include <VGUI_App.h>
#include <VGUI_Dar.h>
#include <VGUI_Panel.h>

namespace vgui
{
struct VGuiCompatPanelAccess
{
	template <class T>
	static void releaseDarStorage(Dar<T>& dar)
	{
		delete[] dar._data;
		dar._data = NULL;
		dar._count = 0;
		dar._capacity = 0;
	}

	// Mirrors App::panelDeleted() when VGUI_ABI_COMPAT_MODE == 1 omits it,
	// preventing dangling App focus/capture references across client reloads.
	static void forgetPanel(App* app, Panel* panel)
	{
		if (app == NULL || panel == NULL)
			return;

		if (app->_mouseCapture == panel)
			app->setMouseCapture(NULL);
		if (app->_mouseArenaPanel == panel)
			app->setMouseArena(0, 0, 0, 0, false);
		if (app->_keyFocus == panel)
			app->_keyFocus = NULL;
		if (app->_oldMouseFocus == panel)
			app->_oldMouseFocus = NULL;
		if (app->_mouseFocus == panel)
			app->_mouseFocus = NULL;
		if (app->_wantedKeyFocus == panel)
			app->_wantedKeyFocus = NULL;
	}

	static void releasePanelDars(Panel* panel)
	{
		if (panel == NULL)
			return;

		releaseDarStorage(panel->_childDar);
		releaseDarStorage(panel->_inputSignalDar);
		releaseDarStorage(panel->_repaintSignalDar);
		releaseDarStorage(panel->_focusChangeSignalDar);
	}
};
}
#endif

#endif
