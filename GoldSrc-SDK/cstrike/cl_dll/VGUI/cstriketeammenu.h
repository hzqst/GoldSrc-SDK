//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CSTRIKETEAMMENU_H
#define CSTRIKETEAMMENU_H

#include "game_controls/teammenu.h"

//-----------------------------------------------------------------------------
// Purpose: Displays the team menu
//-----------------------------------------------------------------------------
class CCSTeamMenu : public CTeamMenu
{
public:
	CCSTeamMenu( vgui2::Panel* parent );
	~CCSTeamMenu() override;
	void Update( const char* mapName, bool allowSpectators, const char** teamNames, int numTeams );

private:
	void OnCommand( const char* command ) override;
	void SetVisibleButton( const char* name, bool state );
	bool m_bVIPMap;
};

#endif // CSTEAMMENU_H
