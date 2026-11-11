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

class KeyValues;

namespace vgui2
{
class IScheme;

//-----------------------------------------------------------------------------
// Purpose: Interface to panel borders
//			Borders have a close relationship with panels
//			They are the edges of the panel.
//-----------------------------------------------------------------------------
class IBorder
{
public:
	virtual void Paint( VPANEL panel );
	virtual void Paint( int x0, int y0, int x1, int y1 );
	virtual void Paint( int x0, int y0, int x1, int y1, int breakSide, int breakStart, int breakEnd );

	virtual void SetInset( int left, int top, int right, int bottom );
	virtual void GetInset( int& left, int& top, int& right, int& bottom );

	virtual void ApplySchemeSettings( IScheme* scheme, KeyValues* settings );

	virtual const char* GetName();
	virtual void SetName( const char* name );
};
} // namespace vgui
