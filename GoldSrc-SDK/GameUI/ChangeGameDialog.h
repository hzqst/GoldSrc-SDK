//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"

namespace vgui2
{
class ListPanel;
}

//-----------------------------------------------------------------------------
// Purpose: Dialogs for use to change current loaded mod
//-----------------------------------------------------------------------------
class CChangeGameDialog : public vgui2::Frame
{
	typedef vgui2::Frame BaseClass;

public:
	CChangeGameDialog( vgui2::Panel* parent );
	~CChangeGameDialog() override;

	void OnCommand( const char* command ) override;
	void OnClose() override;

private:
	void LoadModList();

	vgui2::ListPanel*	m_pModList;
};
