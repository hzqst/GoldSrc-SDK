//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "vgui/VGUI.h"
#include "Frame.h"
#include "vgui/MessageMap.h"

class CVACBannedConnRefusedDialog;

//-----------------------------------------------------------------------------
// Purpose: Tells the user they were refused by a secure server because of a VAC ban
//-----------------------------------------------------------------------------
class CVACBannedConnRefusedDialog : public vgui2::Frame
{
public:
	DECLARE_CLASS_SIMPLE_NAMED( CVACBannedConnRefusedDialog, vgui2::Frame, "vgui::Frame" );

	CVACBannedConnRefusedDialog( vgui2::VPANEL hVParent, const char *name );
};
