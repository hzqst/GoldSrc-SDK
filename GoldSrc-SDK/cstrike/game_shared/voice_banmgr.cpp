//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "voice_banmgr.h"

#include <stdio.h>
#include <string.h>

#define BANMGR_FILEVERSION 1

const char* g_pBanMgrFilename = "voice_ban.dt";

unsigned char HashPlayerID( const char playerID[16] )
{
	unsigned char hash = 0;
	for ( int i = 0; i < 16; ++i )
		hash += (unsigned char)playerID[i];
	return hash;
}

CVoiceBanMgr::CVoiceBanMgr()
{
	Clear();
}

CVoiceBanMgr::~CVoiceBanMgr()
{
	Term();
}

bool CVoiceBanMgr::Init( const char* gameDirectory )
{
	Term();
	char filename[512];
#ifdef _WIN32
	_snprintf( filename, sizeof( filename ), "%s/%s", gameDirectory, g_pBanMgrFilename );
#else
	snprintf( filename, sizeof( filename ), "%s/%s", gameDirectory, g_pBanMgrFilename );
#endif
	filename[sizeof( filename ) - 1] = 0;
	FILE* file = fopen( filename, "rb" );
	if ( file )
	{
		int version;
		fread( &version, 1, sizeof( version ), file );
		if ( version == BANMGR_FILEVERSION )
		{
			fseek( file, 0, SEEK_END );
			int count = (int)( (unsigned int)( ftell( file ) - sizeof( version ) ) / 16 );
			fseek( file, sizeof( version ), SEEK_SET );
			for ( int i = 0; i < count; ++i )
			{
				char playerID[16];
				fread( playerID, 1, sizeof( playerID ), file );
				AddBannedPlayer( playerID );
			}
		}
		fclose( file );
	}
	return true;
}

void CVoiceBanMgr::Term()
{
	// Free all the player structures.
	for ( int i = 0; i < 256; ++i )
	{
		BannedPlayer* head = &m_PlayerHash[i];
		BannedPlayer* player = head->m_pNext;
		while ( player != head )
		{
			BannedPlayer* next = player->m_pNext;
			delete player;
			player = next;
		}
	}
	Clear();
}

void CVoiceBanMgr::SaveState( const char* gameDirectory )
{
	char filename[512];
#ifdef _WIN32
	_snprintf( filename, sizeof( filename ), "%s/%s", gameDirectory, g_pBanMgrFilename );
#else
	snprintf( filename, sizeof( filename ), "%s/%s", gameDirectory, g_pBanMgrFilename );
#endif
	filename[sizeof( filename ) - 1] = 0;
	FILE* file = fopen( filename, "wb" );
	if ( file )
	{
		int version = BANMGR_FILEVERSION;
		fwrite( &version, 1, sizeof( version ), file );
		for ( int i = 0; i < 256; ++i )
		{
			BannedPlayer* head = &m_PlayerHash[i];
			for ( BannedPlayer* player = head->m_pNext; player != head; player = player->m_pNext )
				fwrite( player->m_PlayerID, 1, sizeof( player->m_PlayerID ), file );
		}
		fclose( file );
	}
}

bool CVoiceBanMgr::GetPlayerBan( const char playerID[16] )
{
	return InternalFindPlayerSquelch( playerID ) != NULL;
}

void CVoiceBanMgr::SetPlayerBan( const char playerID[16], bool squelch )
{
	if ( squelch )
	{
		if ( !GetPlayerBan( playerID ) )
			AddBannedPlayer( playerID );
	}
	else
	{
		BannedPlayer* player = InternalFindPlayerSquelch( playerID );
		if ( player )
		{
			player->m_pPrev->m_pNext = player->m_pNext;
			player->m_pNext->m_pPrev = player->m_pPrev;
			delete player;
		}
	}
}

void CVoiceBanMgr::ForEachBannedPlayer( void ( *callback )( char playerID[16] ) )
{
	for ( int i = 0; i < 256; ++i )
	{
		BannedPlayer* head = &m_PlayerHash[i];
		for ( BannedPlayer* player = head->m_pNext; player != head; player = player->m_pNext )
			callback( player->m_PlayerID );
	}
}

void CVoiceBanMgr::Clear()
{
	// Tie off the hash table entries.
	for ( int i = 0; i < 256; ++i )
		m_PlayerHash[i].m_pNext = m_PlayerHash[i].m_pPrev = &m_PlayerHash[i];
}

CVoiceBanMgr::BannedPlayer* CVoiceBanMgr::InternalFindPlayerSquelch( const char playerID[16] )
{
	BannedPlayer* head = &m_PlayerHash[HashPlayerID( playerID )];
	for ( BannedPlayer* player = head->m_pNext; player != head; player = player->m_pNext )
	{
		if ( memcmp( playerID, player->m_PlayerID, 16 ) == 0 )
			return player;
	}
	return NULL;
}

CVoiceBanMgr::BannedPlayer* CVoiceBanMgr::AddBannedPlayer( const char playerID[16] )
{
	BannedPlayer* player = new BannedPlayer;
	if ( !player )
		return NULL;
	BannedPlayer* head = &m_PlayerHash[HashPlayerID( playerID )];
	memcpy( player->m_PlayerID, playerID, 16 );
	player->m_pNext = head;
	player->m_pPrev = head->m_pPrev;
	player->m_pPrev->m_pNext = head->m_pPrev = player;
	return player;
}
