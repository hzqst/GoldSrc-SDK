//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Server list control used by the game list pages
//
// $NoKeywords: $
//=============================================================================

#include "BaseGamesPage.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameListPanel::CGameListPanel( CBaseGamesPage *pOuter, const char *pName ) : BaseClass( pOuter, pName )
{
	m_pOuter = pOuter;
}

//-----------------------------------------------------------------------------
// Purpose: Forward KEY_ENTER to the parent page
//-----------------------------------------------------------------------------
void CGameListPanel::OnKeyCodeTyped( vgui2::KeyCode code )
{
	// Let the outer class handle it
	if ( code == vgui2::KEY_ENTER && m_pOuter->OnGameListEnterPressed() )
		return;

	BaseClass::OnKeyCodeTyped( code );
}
