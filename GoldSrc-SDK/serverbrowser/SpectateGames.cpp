//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list page for HLTV spectator proxies
//
// $NoKeywords: $
//=============================================================================

#include "SpectateGames.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSpectateGames::CSpectateGames( vgui2::Panel *parent )
	: CInternetGames( parent, "SpectateGames", eSpectatorServer )
{
}

//-----------------------------------------------------------------------------
// Purpose: Requests the server list, restricted to spectator proxies
//-----------------------------------------------------------------------------
void CSpectateGames::GetNewServerList()
{
	MatchMakingKeyValuePair_t filter( "proxy", "1" );
	m_vecServerFilters.AddToTail( filter );

	CInternetGames::GetNewServerList();
}
