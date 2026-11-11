//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "ISurface.h"
#include "vgui/KeyCode.h"
#include "vgui/MouseCode.h"

namespace vgui2
{
class IInput : public IBaseInterface
{
public:
	virtual void SetMouseFocus( VPANEL panel );
	virtual void SetMouseCapture( VPANEL panel );
	virtual void GetKeyCodeText( KeyCode code, char* text, int bufferSize );
	virtual VPANEL GetFocus();
	virtual VPANEL GetMouseOver(); // returns the panel the mouse is currently over, ignoring mouse capture

	// mouse state
	virtual void SetCursorPos( int x, int y );
	virtual void GetCursorPos( int& x, int& y );
	virtual bool WasMousePressed( MouseCode code );
	virtual bool WasMouseDoublePressed( MouseCode code );
	virtual bool IsMouseDown( MouseCode code );
	// cursor override
	virtual void SetCursorOveride( HCursor cursor );
	virtual HCursor GetCursorOveride();
	// key state
	virtual bool WasMouseReleased( MouseCode code );

	virtual bool WasKeyPressed( KeyCode code );
	virtual bool IsKeyDown( KeyCode code );
	virtual bool WasKeyTyped( KeyCode code );
	virtual bool WasKeyReleased( KeyCode code );

	virtual VPANEL GetAppModalSurface();
	// set the modal dialog panel.
	// all events will go only to this panel and its children.
	virtual void SetAppModalSurface( VPANEL panel );
	virtual void ReleaseAppModalSurface();

	virtual void GetCursorPosition( int& x, int& y );
};

IInput* input();
} // namespace vgui
