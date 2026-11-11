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

#include "Panel.h"

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Thin line used to divide sections in dialogs
//-----------------------------------------------------------------------------
class Divider : public Panel
{
	DECLARE_CLASS_SIMPLE( Divider, Panel );

public:
	Divider( Panel* parent, const char* name );
	~Divider() override;

protected:
	void ApplySchemeSettings( IScheme* pScheme ) override;
};
} // namespace vgui
