//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================


// This class manages the (persistent) list of squelched players.
#pragma once

class CVoiceBanMgr
{
public:
	CVoiceBanMgr();
	~CVoiceBanMgr();

	// Init loads the list of squelched players from disk.
	bool Init( const char* gameDirectory );
	void Term();
	void SaveState( const char* gameDirectory );
	bool GetPlayerBan( const char playerID[16] );
	void SetPlayerBan( const char playerID[16], bool squelch );
	void ForEachBannedPlayer( void ( *callback )( char playerID[16] ) );

protected:
	class BannedPlayer
	{
	public:
		char m_PlayerID[16];
		BannedPlayer* m_pPrev;
		BannedPlayer* m_pNext;
	};

	void Clear();
	BannedPlayer* InternalFindPlayerSquelch( const char playerID[16] );
	BannedPlayer* AddBannedPlayer( const char playerID[16] );

	BannedPlayer m_PlayerHash[256];
};
