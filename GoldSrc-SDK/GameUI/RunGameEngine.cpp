//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "common/IRunGameEngine.h"
#include "GameUI_Interface.h"

#include "wrect.h"
#include "cl_dll.h"

#include <stdio.h>
#include <string.h>

class CRunGameEngine : public IRunGameEngine
{
public:
	bool IsRunning() override;
	bool AddTextCommand( const char* text ) override;
	ERunResult RunEngine( int appID, const char* gameDir, const char* commandLineParams ) override;
	bool IsInGame() override;
	bool GetGameInfo( char* infoBuffer, int bufferSize ) override;
	void SetTrackerUserID( int trackerID, const char* trackerName ) override;
	int GetPlayerCount() override;
	unsigned int GetPlayerFriendsID( int playerIndex ) override;
	const char* GetPlayerName( int trackerID ) override;
	const char* GetPlayerFriendsName( int friendsID ) override;
	unsigned int GetEngineBuildNumber() override;
	const char* GetProductVersionString() override;
	unsigned int GetPlayerUserID( int playerIndex ) override;
};

EXPOSE_SINGLE_INTERFACE( CRunGameEngine, IRunGameEngine, "RunGameEngine006" );

bool CRunGameEngine::IsRunning()
{
	return true;
}

bool CRunGameEngine::AddTextCommand( const char* text )
{
	engine->pfnClientCmd( (char*)text );
	return true;
}

IRunGameEngine::ERunResult CRunGameEngine::RunEngine( int, const char*, const char* )
{
	return k_ERunResultNotInitialized;
}

bool CRunGameEngine::IsInGame()
{
	return engine->pfnGetLevelName() && engine->pfnGetLevelName()[0];
}

bool CRunGameEngine::GetGameInfo( char*, int )
{
	return false;
}

void CRunGameEngine::SetTrackerUserID( int trackerID, const char* trackerName )
{
	char buffer[32];
	sprintf( buffer, "%d", trackerID );
	engine->PlayerInfo_SetValueForKey( "*fid", buffer );
	engine->PlayerInfo_SetValueForKey( "*fname", (char*)trackerName );

	const char* name = engine->pfnGetCvarString( "name" );
	if ( name && trackerName && trackerName[0] && !strcmp( name, "Player" ) )
		engine->Cvar_Set( "name", (char*)trackerName );
}

int CRunGameEngine::GetPlayerCount()
{
	return engine->GetMaxClients();
}

unsigned int CRunGameEngine::GetPlayerFriendsID( int )
{
	return 0;
}

const char* CRunGameEngine::GetPlayerName( int trackerID )
{
	int playerIndex = engine->GetPlayerForTrackerID( trackerID );
	if ( !playerIndex )
		return NULL;

	hud_player_info_t playerInfo;
	engine->pfnGetPlayerInfo( playerIndex, &playerInfo );
	return playerInfo.name;
}

const char* CRunGameEngine::GetPlayerFriendsName( int )
{
	return NULL;
}

unsigned int CRunGameEngine::GetEngineBuildNumber()
{
	return 0;
}

const char* CRunGameEngine::GetProductVersionString()
{
	return NULL;
}

unsigned int CRunGameEngine::GetPlayerUserID( int playerIndex )
{
	int trackerID = engine->GetTrackerIDForPlayer( playerIndex );
	return trackerID > 0 ? trackerID : 0;
}
