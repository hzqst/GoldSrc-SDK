//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for HLTV spectator proxies
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "InternetGames.h"

namespace vgui2
{
class Panel;
} // namespace vgui2

//-----------------------------------------------------------------------------
// Purpose: Spectator games list
//-----------------------------------------------------------------------------
class CSpectateGames : public CInternetGames
{
public:
	CSpectateGames( vgui2::Panel *parent );

protected:
	// filters by spectator games
	virtual void GetNewServerList();
};
