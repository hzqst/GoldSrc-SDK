//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: List of game modifications the server browser knows about
//
// $NoKeywords: $
//=============================================================================

#include "ModList.h"
#include "ServerBrowserDialog.h"


//-----------------------------------------------------------------------------
// Purpose: Sort function used to order mods by their description
//-----------------------------------------------------------------------------
int CModList::ModNameCompare( const CModList::mod_t *pLeft, const CModList::mod_t *pRight )
{
	return stricmp( pLeft->description, pRight->description );
}

//-----------------------------------------------------------------------------
// Purpose: Singleton accessor
//-----------------------------------------------------------------------------
CModList &ModList()
{
	static CModList s_ModList;
	return s_ModList;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CModList::CModList()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns number of mods
//-----------------------------------------------------------------------------
int CModList::ModCount()
{
	return m_ModList.Count();
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *CModList::GetModName( int index )
{
	return m_ModList[index].description;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char *CModList::GetModDir( int index )
{
	return m_ModList[index].gamedir;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const CGameID &CModList::GetAppID( int index ) const
{
	return m_ModList[index].m_GameID;
}

//-----------------------------------------------------------------------------
// Purpose: returns the mod index for the specified game, or -1 if not found
//-----------------------------------------------------------------------------
int CModList::GetIndex( const CGameID &iAppID ) const
{
	for ( int i = 0; i < m_ModList.Count(); i++ )
	{
		if ( m_ModList[i].m_GameID == iAppID )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: returns the mod name for the specified game, falling back to the
//			active game name of the server browser
//-----------------------------------------------------------------------------
const char *CModList::GetModNameForModDir( const CGameID &gameID )
{
	int iModIndex = GetIndex( gameID );
	if ( iModIndex >= 0 )
		return m_ModList[iModIndex].description;

	if ( !ServerBrowserDialog().GetActiveModName() )
		return "";

	return ServerBrowserDialog().GetActiveGameName();
}

//-----------------------------------------------------------------------------
// Purpose: adds a vgui panel to be notified when the mod list changes
//-----------------------------------------------------------------------------
void CModList::AddVGUIListener( vgui2::VPANEL panel )
{
	m_VGUIListeners.AddToTail( panel );
}

//-----------------------------------------------------------------------------
// Purpose: Steam mod enumeration is not used by this build
//-----------------------------------------------------------------------------
void CModList::ParseSteamMods()
{
}

//-----------------------------------------------------------------------------
// Purpose: Application configuration is not loaded by this build; always fails
//-----------------------------------------------------------------------------
int CModList::LoadAppConfiguration( uint32 nAppID )
{
	return -1;
}
