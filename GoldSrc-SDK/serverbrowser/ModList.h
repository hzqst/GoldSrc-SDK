//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: List of game modifications the server browser knows about
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "utlvector.h"
#include "../external/steamworks/steamclientpublic.h"
#include "tier0/platform.h"

#include "vgui/VGUI.h"

//-----------------------------------------------------------------------------
// Purpose: Holds the list of known mods and their game IDs
//-----------------------------------------------------------------------------
class CModList
{
	struct mod_t
	{
		bool operator==( const CModList::mod_t &rhs ) const;

		char description[64];
		char gamedir[64];
		CGameID m_GameID;
		int m_InternalAppId;
	};

public:
	CModList();

	// returns the mod index for the specified game, or -1 if not found
	int GetIndex( const CGameID &iAppID ) const;

	// adds a vgui panel to be notified when the mod list changes
	void AddVGUIListener( vgui2::VPANEL panel );

	// returns number of mods
	int ModCount();

	// returns information about a mod
	const char *GetModName( int index );
	const char *GetModDir( int index );
	const CGameID &GetAppID( int index ) const;

	// returns the mod name for the associated gamedir
	const char *GetModNameForModDir( const CGameID &gameID );

private:
	static int ModNameCompare( const CModList::mod_t *pLeft, const CModList::mod_t *pRight );

	void ParseInstalledMods();
	void ParseSteamMods();
	int LoadAppConfiguration( uint32 nAppID );

	CUtlVector<CModList::mod_t> m_ModList;
	CUtlVector<vgui2::VPANEL> m_VGUIListeners;
};

// singleton accessor
CModList &ModList( void );
