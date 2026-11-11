//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#pragma once

#include "interface.h"

//-----------------------------------------------------------------------------
// Purpose: Interface to running the game engine
//-----------------------------------------------------------------------------
class IRunGameEngine : public IBaseInterface
{
public:
	enum ERunResult
	{
		k_ERunResultOkay = 0,
		k_ERunResultModNotInstalled,
		k_ERunResultAppNotFound,
		k_ERunResultNotInitialized
	};

	// Returns true if the engine is running, false otherwise.
	virtual bool IsRunning() = 0;
	// Adds text to the engine command buffer. Only works if IsRunning()
	// returns true on success, false on failure
	virtual bool AddTextCommand( const char* text ) = 0;
	// runs the engine with the specified command line parameters.  Only works if !IsRunning()
	// returns true on success, false on failure
	virtual ERunResult RunEngine( int appID, const char* gameDir, const char* commandLineParams ) = 0;
	// returns true if the player is currently connected to a game server
	virtual bool IsInGame() = 0;
	// gets information about the server the engine is currently connected to
	// returns true on success, false on failure
	virtual bool GetGameInfo( char* infoBuffer, int bufferSize ) = 0;
	virtual void SetTrackerUserID( int trackerID, const char* trackerName ) = 0;
	// this next section could probably moved to another interface
	// iterates users
	// returns the number of user
	virtual int GetPlayerCount() = 0;
	virtual unsigned int GetPlayerFriendsID( int playerIndex ) = 0;
	virtual const char* GetPlayerName( int trackerID ) = 0;
	virtual const char* GetPlayerFriendsName( int friendsID ) = 0;
	virtual unsigned int GetEngineBuildNumber() = 0;
	virtual const char* GetProductVersionString() = 0;
	virtual unsigned int GetPlayerUserID( int playerIndex ) = 0;
};
