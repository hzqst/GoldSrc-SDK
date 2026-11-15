#include "wrect.h"
#include "cl_dll.h"
#include "cl_entity.h"
#include "cvardef.h"
#include "parsemsg.h" // BEGIN_READ(), ...
#include "voice_status.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"game_shared\\voice_status.cpp" )

static CVoiceStatus* g_pInternalVoiceStatus = NULL;

// ---------------------------------------------------------------------- //
// The voice manager for the client.
// ---------------------------------------------------------------------- //
CVoiceStatus g_VoiceStatus;
int g_BannedPlayerPrintCount;

CVoiceStatus* GetClientVoice()
{
	return &g_VoiceStatus;
}

int __MsgFunc_VoiceMask( const char*, int size, void* buffer )
{
	if ( g_pInternalVoiceStatus )
		g_pInternalVoiceStatus->HandleVoiceMaskMsg( size, buffer );
	return 1;
}

int __MsgFunc_ReqState( const char*, int size, void* buffer )
{
	if ( g_pInternalVoiceStatus )
		g_pInternalVoiceStatus->HandleReqStateMsg( size, buffer );
	return 1;
}

void ForEachBannedPlayer( char playerID[16] )
{
	char text[256];
	sprintf( text, "Ban %d: %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n",
		g_BannedPlayerPrintCount++,
		(unsigned int)(int)playerID[0], (unsigned int)(int)playerID[1],
		(unsigned int)(int)playerID[2], (unsigned int)(int)playerID[3],
		(unsigned int)(int)playerID[4], (unsigned int)(int)playerID[5],
		(unsigned int)(int)playerID[6], (unsigned int)(int)playerID[7],
		(unsigned int)(int)playerID[8], (unsigned int)(int)playerID[9],
		(unsigned int)(int)playerID[10], (unsigned int)(int)playerID[11],
		(unsigned int)(int)playerID[12], (unsigned int)(int)playerID[13],
		(unsigned int)(int)playerID[14], (unsigned int)(int)playerID[15] );
#ifdef _WIN32
	strupr( text );
#endif
	gEngfuncs.pfnConsolePrint( text );
}

void ShowBannedCallback()
{
	if ( g_pInternalVoiceStatus )
	{
		g_BannedPlayerPrintCount = 0;
		gEngfuncs.pfnConsolePrint( "------- BANNED PLAYERS -------\n" );
		g_pInternalVoiceStatus->GetBanMgr()->ForEachBannedPlayer( ForEachBannedPlayer );
		gEngfuncs.pfnConsolePrint( "------------------------------\n" );
	}
}

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CVoiceStatus::CVoiceStatus()
{
	m_bBanMgrInitialized = false;
	m_LastUpdateServerState = 0;
	m_bTalking = false;
	m_bServerAcked = false;
	m_bServerModEnable = -1;
	m_pchGameDir = NULL;
}

CVoiceStatus::~CVoiceStatus()
{
	g_pInternalVoiceStatus = NULL;
	if ( m_pchGameDir )
	{
		if ( m_bBanMgrInitialized )
			m_BanMgr.SaveState( m_pchGameDir );
		free( m_pchGameDir );
	}
}

void CVoiceStatus::Init( IVoiceStatusHelper* helper )
{
	// Setup the voice_modenable cvar.
	gEngfuncs.pfnRegisterVariable( "voice_modenable", "1", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "voice_clientdebug", "0", 0 );
	gEngfuncs.pfnAddCommand( "voice_showbanned", ShowBannedCallback );
	const char* directory = gEngfuncs.pfnGetGameDirectory();
	m_pchGameDir = (char*)malloc( strlen( directory ) + 1 );
	if ( m_pchGameDir )
	{
		strcpy( m_pchGameDir, directory );
	}

	if ( m_pchGameDir )
	{
		m_BanMgr.Init( m_pchGameDir );
		m_bBanMgrInitialized = true;
	}

	ASSERT_AT( !g_pInternalVoiceStatus, 145 );
	g_pInternalVoiceStatus = this;
	m_bInSquelchMode = false;
	m_pHelper = helper;
	gEngfuncs.pfnHookUserMsg( "VoiceMask", __MsgFunc_VoiceMask );
	gEngfuncs.pfnHookUserMsg( "ReqState", __MsgFunc_ReqState );
	GetClientVoiceHud()->Init( helper, this );
}

void CVoiceStatus::Frame( double )
{
	// check server banned players once per second
	if ( gEngfuncs.GetClientTime() - m_LastUpdateServerState > 1 )
		UpdateServerState( false );
}

void CVoiceStatus::StartSquelchMode()
{
	if ( !m_bInSquelchMode )
		m_bInSquelchMode = true;
}

void CVoiceStatus::StopSquelchMode()
{
	m_bInSquelchMode = false;
}

bool CVoiceStatus::IsInSquelchMode()
{
	return m_bInSquelchMode;
}

void CVoiceStatus::UpdateServerState( bool force )
{
	if ( !gEngfuncs.pfnGetLevelName()[0] )
	{
		if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) != 0 )
			gEngfuncs.pfnConsolePrint( "CVoiceStatus::UpdateServerState: pLevelName[0]==0\n" );
		return;
	}
	int enabled = gEngfuncs.pfnGetCvarFloat( "voice_modenable" ) != 0;
	if ( force || m_bServerModEnable != enabled )
	{
		m_bServerModEnable = enabled;
		char command[256];
		_snprintf( command, sizeof( command ), "VModEnable %d", enabled );
		gEngfuncs.pfnServerCmd( command );
		if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) != 0 )
		{
			char text[256];
			sprintf( text, "CVoiceStatus::UpdateServerState: Sending '%s'\n", command );
			gEngfuncs.pfnConsolePrint( text );
		}
	}
	char command[2048];
	sprintf( command, "vban" );
	bool changed = false;
	for ( int word = 0; word < VOICE_MAX_PLAYERS_DW; ++word )
	{
		uint32 serverMask = 0;
		uint32 banMask = 0;
		for ( int player = 0; player < 32; ++player )
		{
			char playerID[16];
			if ( !gEngfuncs.GetPlayerUniqueID( player + 1, playerID ) )
				continue;
			if ( m_BanMgr.GetPlayerBan( playerID ) )
				banMask |= 1u << player;
			if ( m_ServerBannedPlayers[word * 32 + player] )
				serverMask |= 1u << player;
		}
		if ( serverMask != banMask )
			changed = true;
		char text[256];
		sprintf( text, " %x", banMask );
		strcat( command, text );
	}
	if ( changed || force )
	{
		if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) != 0 )
		{
			char text[256];
			sprintf( text, "CVoiceStatus::UpdateServerState: Sending '%s'\n", command );
			gEngfuncs.pfnConsolePrint( text );
		}
		gEngfuncs.pfnServerCmdUnreliable( command );	// Tell the server..
	}
	else if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) != 0 )
	{
		gEngfuncs.pfnConsolePrint( "CVoiceStatus::UpdateServerState: no change\n" );
	}
	m_LastUpdateServerState = gEngfuncs.GetClientTime();
}

int CVoiceStatus::GetSpeakerStatus( int player )
{
	bool talking = m_VoicePlayers[player] != 0;
	char playerID[16];
	if ( !gEngfuncs.GetPlayerUniqueID( player + 1, playerID ) )
		return VOICE_NEVERSPOKEN;
	if ( m_BanMgr.GetPlayerBan( playerID ) )
		return VOICE_BANNED;
	if ( !m_VoiceEnabledPlayers[player] )
		return VOICE_NEVERSPOKEN;
	return talking ? VOICE_TALKING : VOICE_NOTTALKING;
}

void CVoiceStatus::HandleVoiceMaskMsg( int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	for ( int word = 0; word < VOICE_MAX_PLAYERS_DW; ++word )
	{
		m_AudiblePlayers.SetDWord( word, (uint32)READ_LONG() );
		m_ServerBannedPlayers.SetDWord( word, (uint32)READ_LONG() );
		if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) != 0 )
		{
			char text[256];
			gEngfuncs.pfnConsolePrint( "CVoiceStatus::HandleVoiceMaskMsg\n" );
			sprintf( text, "    - m_AudiblePlayers[%d] = %lu\n", word, (unsigned long)m_AudiblePlayers.GetDWord( word ) );
			gEngfuncs.pfnConsolePrint( text );
			sprintf( text, "    - m_ServerBannedPlayers[%d] = %lu\n", word, (unsigned long)m_ServerBannedPlayers.GetDWord( word ) );
			gEngfuncs.pfnConsolePrint( text );
		}
	}
	m_bServerModEnable = READ_BYTE();
}

void CVoiceStatus::HandleReqStateMsg( int, void* )
{
	if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) != 0 )
		gEngfuncs.pfnConsolePrint( "CVoiceStatus::HandleReqStateMsg\n" );
	UpdateServerState( true );
}

void CVoiceStatus::UpdateSpeakerStatus( int player, bool talking )
{
	const char* levelName = gEngfuncs.pfnGetLevelName();
	if ( !levelName || !levelName[0] )
		return;
	if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) != 0 )
	{
		char text[256];
		_snprintf( text, sizeof( text ), "CVoiceStatus::UpdateSpeakerStatus: ent %d talking = %d\n", player, talking );
		gEngfuncs.pfnConsolePrint( text );
	}

	// Is it the local player talking?
	if ( player == -1 )
	{
		m_bTalking = talking;
		if ( talking )
			gEngfuncs.pfnClientCmd( "voice_modenable 1" );
		if ( !gEngfuncs.GetLocalPlayer() )
			return;
		int localPlayer = gEngfuncs.GetLocalPlayer()->index;
		GetClientVoiceHud()->UpdateSpeakerStatus( -2, talking );
		m_VoicePlayers[localPlayer - 1] = m_bTalking;
		m_VoiceEnabledPlayers[localPlayer - 1] = true;
	}
	else if ( player == -2 )
	{
		m_bServerAcked = talking;
	}
	else if ( player >= 0 && player <= VOICE_MAX_PLAYERS )
	{
		if ( player == 0 )
			return;
		GetClientVoiceHud()->UpdateSpeakerStatus( player, talking );
		m_VoicePlayers[player - 1] = talking;
		if ( talking )
			m_VoiceEnabledPlayers[player - 1] = true;
	}
	GetClientVoiceHud()->RepositionLabels();
}



//-----------------------------------------------------------------------------
// Purpose: returns true if the target client has been banned
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVoiceStatus::IsPlayerBlocked( int player )
{
	char playerID[16];
	if ( !gEngfuncs.GetPlayerUniqueID( player, playerID ) )
		return false;
	return m_BanMgr.GetPlayerBan( playerID );
}

bool CVoiceStatus::IsPlayerAudible( int player )
{
	return m_AudiblePlayers[player - 1] != 0;
}

//-----------------------------------------------------------------------------
// Purpose: blocks/unblocks the target client from being heard
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CVoiceStatus::SetPlayerBlockedState( int player, bool blocked )
{
	if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) != 0 )
		gEngfuncs.pfnConsolePrint( "CVoiceStatus::SetPlayerBlockedState part 1\n" );
	char playerID[16];
	if ( !gEngfuncs.GetPlayerUniqueID( player, playerID ) )
		return;
	if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) != 0 )
		gEngfuncs.pfnConsolePrint( "CVoiceStatus::SetPlayerBlockedState part 2\n" );
	if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) != 0 )
	{
		char text[256];
		sprintf( text, "CVoiceStatus::SetPlayerBlockedState: setting player %d ban to %d\n", player, !m_BanMgr.GetPlayerBan( playerID ) );
		gEngfuncs.pfnConsolePrint( text );
	}
	m_BanMgr.SetPlayerBan( playerID, blocked );
	UpdateServerState( false );
}
