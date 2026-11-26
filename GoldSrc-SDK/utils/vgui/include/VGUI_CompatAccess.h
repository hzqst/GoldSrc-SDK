//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Controlled friend access and typed destroyer registry for VGUI1
//          ABI-compat mode teardown. Preserves original class sizes, vtables,
//          and vgui.dll exports without base-pointer delete UB.
//
//=============================================================================

#ifndef VGUI_COMPAT_ACCESS_H
#define VGUI_COMPAT_ACCESS_H

#include <VGUI.h>

#if VGUI_ABI_COMPAT_MODE == 1

#pragma push_macro("fabs")
#undef fabs
#include <unordered_map>
#pragma pop_macro("fabs")

#include <VGUI_ActionSignal.h>
#include <VGUI_App.h>
#include <VGUI_Bitmap.h>
#include <VGUI_Border.h>
#include <VGUI_Button.h>
#include <VGUI_ButtonController.h>
#include <VGUI_ChangeSignal.h>
#include <VGUI_CompatPanelAccess.h>
#include <VGUI_Dar.h>
#include <VGUI_FocusChangeSignal.h>
#include <VGUI_HeaderPanel.h>
#include <VGUI_InputSignal.h>
#include <VGUI_IntChangeSignal.h>
#include <VGUI_Label.h>
#include <VGUI_Panel.h>
#include <VGUI_ScrollBar.h>
#include <VGUI_Slider.h>
#include <VGUI_TablePanel.h>
#include <VGUI_TextImage.h>
#include <VGUI_TextEntry.h>

namespace vgui
{

struct VGuiCompatAccess : VGuiCompatPanelAccess
{
	struct DestroyEntry
	{
		void* derivedPtr;
		void (*destroy)(void*);
	};

	static std::unordered_map<const void*, DestroyEntry>& destroyRegistry()
	{
		static std::unordered_map<const void*, DestroyEntry> s_registry;
		return s_registry;
	}

	template <class Derived>
	static void deleteTyped(void* ptr)
	{
		delete static_cast<Derived*>(ptr);
	}

	template <class Base, class Derived>
	static Derived* registerOwned(Derived* obj)
	{
		if (obj != NULL)
		{
			const Base* basePtr = obj;
			DestroyEntry entry;
			entry.derivedPtr = static_cast<void*>(obj);
			entry.destroy = &deleteTyped<Derived>;
			destroyRegistry()[static_cast<const void*>(basePtr)] = entry;
		}
		return obj;
	}

	// Destroys an owned polymorphic object whose base lacks a virtual destructor
	// under VGUI_ABI_COMPAT_MODE == 1:
	//  - If registered at creation/registration, calls delete on the exact Derived*.
	//  - Otherwise (for trivially-destructible default helpers allocated inside
	//    vgui.dll), releases the most-derived storage directly via ::operator delete
	//    per [expr.dynamic.cast]/7 and [basic.life]/5 without a base-pointer delete.
	template <class Base>
	static void destroyOwned(Base* basePtr)
	{
		if (basePtr == NULL)
			return;

		std::unordered_map<const void*, DestroyEntry>& reg = destroyRegistry();
		std::unordered_map<const void*, DestroyEntry>::iterator it = reg.find(static_cast<const void*>(basePtr));
		if (it != reg.end())
		{
			DestroyEntry entry = it->second;
			reg.erase(it);
			entry.destroy(entry.derivedPtr);
			return;
		}

		void* mostDerived = dynamic_cast<void*>(basePtr);
		::operator delete(mostDerived);
	}

	static void releaseBitmapPixels(Bitmap* bitmap)
	{
		if (bitmap == NULL)
			return;

		// Negative size releases _rgba without allocating a zero-length buffer.
		bitmap->setSize(-1, -1);
	}

	static void releaseTextImage(TextImage* textImage)
	{
		if (textImage == NULL)
			return;

		delete[] textImage->_text;
		textImage->_text = NULL;
		textImage->_textBufferLen = 0;
	}

	static void detachFromParent(Panel* panel)
	{
		if (panel == NULL)
			return;

		if (panel->_parent != NULL)
			panel->_parent->removeChild(panel);
		panel->_parent = NULL;
		panel->_surfaceBase = NULL;
	}

	static void cleanupPanelStorage(Panel* panel)
	{
		if (panel == NULL)
			return;

		forgetPanel(App::getInstance(), panel);

		Border* border = panel->_border;
		panel->_border = NULL;
		destroyOwned(border);

		for (int i = 0; i < panel->_inputSignalDar.getCount(); i++)
		{
			InputSignal* signal = panel->_inputSignalDar[i];
			if (dynamic_cast<void*>(signal) != dynamic_cast<void*>(panel))
				destroyOwned(signal);
		}

		releasePanelDars(panel);
		panel->_parent = NULL;
		panel->_surfaceBase = NULL;
		panel->_layout = NULL;
	}

	// Snapshots and detaches all children, releasing _childDar storage.
	// Caller must delete[] the returned array when outCount > 0.
	static Panel** takeChildren(Panel* panel, int& outCount)
	{
		outCount = 0;
		if (panel == NULL)
			return NULL;

		int count = panel->_childDar.getCount();
		Panel** children = count > 0 ? new Panel*[count] : NULL;
		for (int i = 0; i < count; i++)
		{
			children[i] = panel->_childDar[i];
			if (children[i] != NULL)
			{
				children[i]->_parent = NULL;
				children[i]->_surfaceBase = NULL;
			}
		}

		releaseDarStorage(panel->_childDar);
		outCount = count;
		return children;
	}

	static void cleanupLabel(Label* label)
	{
		if (label == NULL)
			return;

		releaseTextImage(label->_textImage);
		delete label->_textImage;
		label->_textImage = NULL;
		label->_image = NULL;
	}

	static void cleanupButton(Button* button)
	{
		if (button == NULL)
			return;

		// Detach the button controller before destroying _inputSignalDar;
		// controllers such as FooDefaultButtonController inherit InputSignal as a secondary base.
		if (button->_buttonController != NULL)
		{
			ButtonController* controller = button->_buttonController;
			button->_buttonController = NULL;
			controller->removeSignals(button);
			destroyOwned(controller);
		}

		for (int i = 0; i < button->_actionSignalDar.getCount(); i++)
		{
			destroyOwned(button->_actionSignalDar[i]);
		}
		releaseDarStorage(button->_actionSignalDar);
	}

	static void cleanupScrollBar(ScrollBar* scrollBar)
	{
		if (scrollBar == NULL)
			return;

		for (int i = 0; i < scrollBar->_intChangeSignalDar.getCount(); i++)
		{
			destroyOwned(scrollBar->_intChangeSignalDar[i]);
		}
		releaseDarStorage(scrollBar->_intChangeSignalDar);
		scrollBar->_button[0] = NULL;
		scrollBar->_button[1] = NULL;
		scrollBar->_slider = NULL;
	}

	static void cleanupSlider(Slider* slider)
	{
		if (slider == NULL)
			return;

		for (int i = 0; i < slider->_intChangeSignalDar.getCount(); i++)
		{
			destroyOwned(slider->_intChangeSignalDar[i]);
		}
		releaseDarStorage(slider->_intChangeSignalDar);
	}

	static void cleanupHeaderPanel(HeaderPanel* headerPanel)
	{
		if (headerPanel == NULL)
			return;

		releaseDarStorage(headerPanel->_sliderPanelDar);
		releaseDarStorage(headerPanel->_sectionPanelDar);
		for (int i = 0; i < headerPanel->_changeSignalDar.getCount(); i++)
		{
			destroyOwned(headerPanel->_changeSignalDar[i]);
		}
		releaseDarStorage(headerPanel->_changeSignalDar);
		headerPanel->_sectionLayer = NULL;
	}

	static void cleanupTablePanel(TablePanel* tablePanel)
	{
		if (tablePanel == NULL)
			return;

		releaseDarStorage(tablePanel->_columnDar);
		if (tablePanel->_fakeInputPanel != NULL)
		{
			Panel* fakeInputPanel = tablePanel->_fakeInputPanel;
			tablePanel->_fakeInputPanel = NULL;
			detachFromParent(fakeInputPanel);
			cleanupPanelStorage(fakeInputPanel);
			::operator delete(dynamic_cast<void*>(fakeInputPanel));
		}
		tablePanel->_editableCellPanel = NULL;
	}

	// A privately owned cell editor has only TextEntry's built-in focus signal.
	static void cleanupCellTextEntry(TextEntry* textEntry)
	{
		if (textEntry == NULL)
			return;
		textEntry->removeInputSignal(textEntry);
		for (int i = 0; i < textEntry->_focusChangeSignalDar.getCount(); i++)
			destroyOwned(textEntry->_focusChangeSignalDar[i]);
		for (int i = 0; i < textEntry->_actionSignalDar.getCount(); i++)
			destroyOwned(textEntry->_actionSignalDar[i]);
		releaseDarStorage(textEntry->_actionSignalDar);
		releaseDarStorage(textEntry->_lineDar);
		cleanupPanelStorage(textEntry);
	}
};

template <class Derived>
inline Derived* VGuiCompat_OwnBorder(Derived* border)
{
	return VGuiCompatAccess::registerOwned<Border>(border);
}

template <class Derived>
inline Derived* VGuiCompat_OwnInputSignal(Derived* signal)
{
	return VGuiCompatAccess::registerOwned<InputSignal>(signal);
}

template <class Derived>
inline Derived* VGuiCompat_OwnActionSignal(Derived* signal)
{
	return VGuiCompatAccess::registerOwned<ActionSignal>(signal);
}

template <class Derived>
inline Derived* VGuiCompat_OwnIntChangeSignal(Derived* signal)
{
	return VGuiCompatAccess::registerOwned<IntChangeSignal>(signal);
}

template <class Derived>
inline Derived* VGuiCompat_OwnChangeSignal(Derived* signal)
{
	return VGuiCompatAccess::registerOwned<ChangeSignal>(signal);
}

template <class Derived>
inline Derived* VGuiCompat_OwnButtonController(Derived* controller)
{
	return VGuiCompatAccess::registerOwned<ButtonController>(controller);
}

} // namespace vgui

#else // VGUI_ABI_COMPAT_MODE != 1

namespace vgui
{

template <class Derived>
inline Derived* VGuiCompat_OwnBorder(Derived* border)
{
	return border;
}

template <class Derived>
inline Derived* VGuiCompat_OwnInputSignal(Derived* signal)
{
	return signal;
}

template <class Derived>
inline Derived* VGuiCompat_OwnActionSignal(Derived* signal)
{
	return signal;
}

template <class Derived>
inline Derived* VGuiCompat_OwnIntChangeSignal(Derived* signal)
{
	return signal;
}

template <class Derived>
inline Derived* VGuiCompat_OwnChangeSignal(Derived* signal)
{
	return signal;
}

template <class Derived>
inline Derived* VGuiCompat_OwnButtonController(Derived* controller)
{
	return controller;
}

} // namespace vgui

#endif // VGUI_ABI_COMPAT_MODE == 1

#endif // VGUI_COMPAT_ACCESS_H
