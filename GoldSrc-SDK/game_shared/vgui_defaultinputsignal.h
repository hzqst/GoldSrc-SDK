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

	// Forwards every input event to an InputSignal it does not own. A Panel
	// deletes the input signals added to it, so an object that handles input
	// for several panels, or for its own child panels, adds one of these to
	// each panel instead of itself.
	class CInputSignalForwarder : public vgui::InputSignal
	{
	public:
		CInputSignalForwarder( InputSignal* pTarget ) : m_pTarget( pTarget ) {}

		virtual void cursorMoved( int x, int y, Panel* panel ) { m_pTarget->cursorMoved( x, y, panel ); }
		virtual void cursorEntered( Panel* panel ) { m_pTarget->cursorEntered( panel ); }
		virtual void cursorExited( Panel* panel ) { m_pTarget->cursorExited( panel ); }
		virtual void mousePressed( MouseCode code, Panel* panel ) { m_pTarget->mousePressed( code, panel ); }
		virtual void mouseDoublePressed( MouseCode code, Panel* panel ) { m_pTarget->mouseDoublePressed( code, panel ); }
		virtual void mouseReleased( MouseCode code, Panel* panel ) { m_pTarget->mouseReleased( code, panel ); }
		virtual void mouseWheeled( int delta, Panel* panel ) { m_pTarget->mouseWheeled( delta, panel ); }
		virtual void keyPressed( KeyCode code, Panel* panel ) { m_pTarget->keyPressed( code, panel ); }
		virtual void keyTyped( KeyCode code, Panel* panel ) { m_pTarget->keyTyped( code, panel ); }
		virtual void keyReleased( KeyCode code, Panel* panel ) { m_pTarget->keyReleased( code, panel ); }
		virtual void keyFocusTicked( Panel* panel ) { m_pTarget->keyFocusTicked( panel ); }

	private:
		InputSignal* m_pTarget;
	};
}

#endif // VGUI_DEFAULTINPUTSIGNAL_H
