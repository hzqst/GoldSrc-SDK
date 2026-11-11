//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_DEFAULTINPUTSIGNAL_H
#define VGUI_DEFAULTINPUTSIGNAL_H
#ifdef _WIN32
#pragma once
#endif

#include "VGUI_InputSignal.h"

namespace vgui
{
	// This class derives from vgui::InputSignal and implements empty defaults for all of its functions.
	class CDefaultInputSignal : public vgui::InputSignal
	{
	public:
		virtual void cursorMoved( int, int, Panel* ) {}
		virtual void cursorEntered( Panel* ) {}
		virtual void cursorExited( Panel* ) {}
		virtual void mousePressed( MouseCode, Panel* ) {}
		virtual void mouseDoublePressed( MouseCode, Panel* ) {}
		virtual void mouseReleased( MouseCode, Panel* ) {}
		virtual void mouseWheeled( int, Panel* ) {}
		virtual void keyPressed( KeyCode, Panel* ) {}
		virtual void keyTyped( KeyCode, Panel* ) {}
		virtual void keyReleased( KeyCode, Panel* ) {}
		virtual void keyFocusTicked( Panel* ) {}
	};
}

#endif // VGUI_DEFAULTINPUTSIGNAL_H
