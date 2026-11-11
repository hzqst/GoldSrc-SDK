 //========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================


#pragma once

#include "Panel.h"

namespace vgui2 { class Menu; }

class CCommandMenu : public vgui2::Panel
{
public:
	CCommandMenu( vgui2::Panel* parent, const char* filename );
	~CCommandMenu() override;
	void OnCommand( const char* command ) override;
	void UpdateCommandMenu();
	void ShowCommandMenu();
	void HideCommandMenu();
	void ClearCommandMenu();
	void PositionCommandMenu();
	void SetMenuVisible( bool visible );
	bool SlotInput( int slot );

private:
	int			m_iTeamNumber;
	char		m_szMap[256];
	char		m_szFilename[256];
	float		m_flCommandMenuOpened;
	vgui2::Menu*	m_pMenu;
};
