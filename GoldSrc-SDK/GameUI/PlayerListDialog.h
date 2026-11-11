//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"

namespace vgui2
{
class Button;
class ListPanel;
}

namespace vgui = vgui2;

//-----------------------------------------------------------------------------
// Purpose: List of players, their ingame-name and their tracker-name
//-----------------------------------------------------------------------------
class CPlayerListDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CPlayerListDialog, vgui::Frame );

public:
	CPlayerListDialog( vgui2::Panel* parent );
	~CPlayerListDialog() override;

	void Activate() override;

protected:
	void OnCommand( const char* command ) override;
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

private:
	void RefreshPlayerProperties();
	void ToggleMuteStateOfSelectedUser();

	vgui2::ListPanel*	m_pPlayerList;
	vgui2::Button*		m_pMuteButton;
};
