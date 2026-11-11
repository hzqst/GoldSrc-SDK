//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "voice_common.h"
#include "voice_banmgr.h"



// This is provided by each mod to access data that may not be the same across mods.
class IVoiceStatusHelper
{
public:
	virtual ~IVoiceStatusHelper() {}

	// Get RGB color for voice status text about this player.
	virtual void GetPlayerTextColor( int player, int color[3] ) = 0;

	// Return the height above the bottom that the voice ack icons should be drawn at.
	virtual int GetAckIconHeight() = 0;

	// Return true if the voice manager is allowed to show speaker labels
	// (mods usually return false when the scoreboard is up).
	virtual bool CanShowSpeakerLabels() = 0;

	// return a pre-translated string for the player's location.  Defaults to empty string for games without locations
	virtual const char* GetPlayerLocation( int ) { return ""; }
};

class IVoiceStatus
{
public:

	// returns the state of this player using the enum above
	virtual int GetSpeakerStatus( int player ) = 0;
	virtual bool IsTalking() = 0;
	virtual bool ServerAcked() = 0;
};

class IVoiceHud
{
public:
	virtual int Init( IVoiceStatusHelper* helper, IVoiceStatus* status ) = 0;

	// ackPosition is the bottom position of where CVoiceStatus will draw the voice acknowledgement labels.
	virtual int VidInit() = 0;

	// Call from the HUD_CreateEntities function so it can add sprites above player heads.
	virtual void CreateEntities() = 0;
	virtual void UpdateLocation( int player, const char* location ) = 0;
	virtual void UpdateSpeakerStatus( int player, bool talking ) = 0;
	virtual void RepositionLabels() = 0;
};

class CVoiceStatus : public IVoiceStatusHelper, public IVoiceStatus
{
public:
	CVoiceStatus();
	~CVoiceStatus() override;

	void Init( IVoiceStatusHelper* helper );
	void UpdateSpeakerStatus( int player, bool talking );
	int GetSpeakerStatus( int player ) override;
	void HandleVoiceMaskMsg( int size, void* buffer );
	void HandleReqStateMsg( int size, void* buffer );
	void Frame( double frameTime );

		// When you enter squelch mode, pass in 
	void StartSquelchMode();
	void StopSquelchMode();
	bool IsInSquelchMode();
	bool IsPlayerBlocked( int player );
	bool IsPlayerAudible( int player );
	void SetPlayerBlockedState( int player, bool blocked );
	void UpdateServerState( bool force );

	bool CanShowSpeakerLabels() override { return m_pHelper->CanShowSpeakerLabels(); }
	void GetPlayerTextColor( int player, int color[3] ) override { m_pHelper->GetPlayerTextColor( player, color ); }
	int GetAckIconHeight() override { return m_pHelper->GetAckIconHeight(); }
	bool IsTalking() override { return m_bTalking; }
	bool ServerAcked() override { return m_bServerAcked; }
	CVoiceBanMgr* GetBanMgr() { return &m_BanMgr; }

	enum
	{
		VOICE_TALKING = 1, // start from one because ImageList's don't use pos 0
		VOICE_BANNED,
		VOICE_NEVERSPOKEN,
		VOICE_NOTTALKING
	}; // various voice states

private:
	float m_LastUpdateServerState;		// Last time we called this function.
	int m_bServerModEnable;				// What we've sent to the server about our "voice_modenable" cvar.
	CPlayerBitVec m_VoicePlayers;		// Who is currently talking. Indexed by client index.

	// This is the gamerules-defined list of players that you can hear. It is based on what teams people are on 
	// and is totally separate from the ban list. Indexed by client index.
	CPlayerBitVec m_AudiblePlayers;

	// Players who have spoken at least once in the game so far
	CPlayerBitVec m_VoiceEnabledPlayers;

	// This is who the server THINKS we have banned (it can become incorrect when a new player arrives on the server).
	// It is checked periodically, and the server is told to squelch or unsquelch the appropriate players.
	CPlayerBitVec m_ServerBannedPlayers;
	IVoiceStatusHelper* m_pHelper;		// Each mod provides an implementation of this.

	// Squelch mode stuff.
	bool m_bInSquelchMode;
	bool m_bTalking;				// Set to true when the client thinks it's talking.
	bool m_bServerAcked;			// Set to true when the server knows the client is talking.
	CVoiceBanMgr m_BanMgr;				// Tracks which users we have squelched and don't want to hear.
	bool m_bBanMgrInitialized;

		// Cache the game directory for use when we shut down
	char* m_pchGameDir;
};

CVoiceStatus*	GetClientVoice();

// Get the (global) voice manager. 
IVoiceHud*		GetClientVoiceHud();
