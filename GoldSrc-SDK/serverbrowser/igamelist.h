//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Interface implemented by every server list page
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "../external/steamworks/steamclientpublic.h"
#include "../external/steamworks/matchmakingtypes.h"

class IGameList;

//-----------------------------------------------------------------------------
// Purpose: Data describing a single server in a game list
//-----------------------------------------------------------------------------
struct serverdisplay_t
{
	serverdisplay_t();
	bool operator==( const serverdisplay_t & ) const;

	int m_iListID;				// the VGUI2 list panel index for displaying this server
	int m_iServerID;			// the matchmaking interface index for this server
	bool m_bDoNotRefresh;
	servernetadr_t m_netAdr;

	// server strings with control characters removed, shared by the list and its sort functions
	char m_szName[k_cbMaxGameServerName];
	char m_szMap[k_cbMaxGameServerMapName];
	char m_szGameDescription[k_cbMaxGameServerGameDescription];
};

//-----------------------------------------------------------------------------
// Purpose: Interface to accessing a game list
//-----------------------------------------------------------------------------
class IGameList
{
public:
	enum InterfaceItem_e
	{
		FILTERS = 0,
		GETNEWLIST = 1,
		ADDSERVER = 2,
		ADDCURRENTSERVER = 3,
	};

	// returns true if the game list supports the specified ui elements
	virtual bool SupportsItem( IGameList::InterfaceItem_e ) = 0;

	// starts the servers refreshing
	virtual void StartRefresh() = 0;

	// gets a new server list
	virtual void GetNewServerList() = 0;

	// stops current refresh/GetNewServerList()
	virtual void StopRefresh() = 0;

	// returns true if the list is currently refreshing servers
	virtual bool IsRefreshing() = 0;

	// gets information about specified server
	virtual serverdisplay_t *GetServer( int ) = 0;
	virtual gameserveritem_t *GetServerInfo( int ) = 0;

	// called when Connect button is pressed
	virtual void OnBeginConnect() = 0;

	// invalid server index
	virtual int GetInvalidServerListID() = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
inline serverdisplay_t::serverdisplay_t()
{
	m_iListID = -1;
	m_iServerID = -1;
	m_bDoNotRefresh = true;
	m_netAdr.Init( 0, 0, 0 );
	m_szName[0] = m_szMap[0] = m_szGameDescription[0] = 0;
}
