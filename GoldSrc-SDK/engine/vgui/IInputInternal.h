//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "IInput.h"

namespace vgui2
{
typedef int HInputContext;

class IInputInternal : public IInput
{
public:
	virtual void RunFrame();
	virtual void UpdateMouseFocus( int x, int y );
	// called when a panel becomes invalid
	virtual void PanelDeleted( VPANEL panel );

	// inputs into vgui input handling 
	virtual void InternalCursorMoved( int x, int y ); //expects input in surface space
	virtual void InternalMousePressed( MouseCode code );
	virtual void InternalMouseDoublePressed( MouseCode code );
	virtual void InternalMouseReleased( MouseCode code );
	virtual void InternalMouseWheeled( int delta );

	virtual void InternalKeyCodePressed( KeyCode code );
	virtual void InternalKeyCodeTyped( KeyCode code );
	virtual void InternalKeyTyped( wchar_t ch );
	virtual void InternalKeyCodeReleased( KeyCode code );

	virtual HInputContext CreateInputContext();
	virtual void DestroyInputContext( HInputContext context );
	virtual void AssociatePanelWithInputContext( HInputContext context, VPANEL panel );
	virtual void ActivateInputContext( HInputContext context );
};
} // namespace vgui
