//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"
#include "Taskbar.h"
#include "GameUI_Interface.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/Controls.h"
#include "../engine/controls.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTaskFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CTaskFrame, vgui::Frame );

public:
	CTaskFrame( vgui2::Panel* parent, const char* panelName, bool showTaskbarIcon = true,
			bool forceProportional = false ) : vgui2::Frame( parent, panelName, showTaskbarIcon, forceProportional )
	{
	}

	~CTaskFrame() override
	{
	}

	void SetTitle( const char* title, bool surfaceTitle = true ) override
	{
		vgui2::Frame::SetTitle( title, surfaceTitle );
		if ( g_pTaskbar )
		{
			wchar_t text[1024];
			const wchar_t* localized = vgui2::localize()->Find( title );
			if ( !localized )
			{
				vgui2::localize()->ConvertANSIToUnicode( title, text, sizeof( text ) );
				localized = text;
			}
			g_pTaskbar->SetTitle( GetVPanel(), localized );
		}
	}
};
