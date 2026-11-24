#ifndef CS_BOT_MANAGER_H
#define CS_BOT_MANAGER_H

#include "bot.h"
#include "bot_profile.h"
#include "nav.h"
#include "nav_area.h"

class CCSBot;
class CNavArea;

class CCSBotManager : public CBotManager
{
public:
	CCSBotManager();
	~CCSBotManager() override;
	void ClientDisconnect( CBasePlayer* player ) override;
	BOOL ClientCommand( CBasePlayer* player, const char* command ) override;
	void ServerActivate() override;
	void ServerDeactivate() override;
	void ServerCommand( const char* command ) override;
	void AddServerCommand( const char* command ) override;
	void AddServerCommands() override;
	void RestartRound() override;
	void StartFrame() override;
	void OnEvent( GameEventType event, CBaseEntity* entity = NULL, CBaseEntity* other = NULL ) override;
	unsigned int GetPlayerPriority( CBasePlayer* player ) const override;
	virtual bool IsImportantPlayer( CBasePlayer* player ) const;

	enum GameScenarioType
	{
		SCENARIO_DEATHMATCH,
		SCENARIO_DEFUSE_BOMB,
		SCENARIO_RESCUE_HOSTAGES,
		SCENARIO_ESCORT_VIP
	};

	struct Zone
	{
		CBaseEntity* m_entity;
		CNavArea* m_area[16];
		int m_areaCount;
		Vector m_center;
		bool m_isLegacy;
		int m_index;
		Extent m_extent;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
	};

	void ValidateMapData();
	const Zone* GetZone( const Vector* position ) const;
	const Zone* GetZone( int index ) const { return &m_zone[index]; }
	const Zone* GetClosestZone( const Vector* position ) const;
	CNavArea* GetRandomAreaInZone( const Zone* zone ) const;
	const Vector* GetRandomPositionInZone( const Zone* zone ) const;
	bool IsOnDefense( const CBasePlayer* player ) const;
	bool IsOnOffense( const CBasePlayer* player ) const;
	bool IsWeaponUseable( CBasePlayerItem* weapon );
	void SetLooseBomb( CBaseEntity* bomb );
	float GetBombTimeLeft() const;
	float GetRadioMessageTimestamp( GameEventType event, int team ) const;
	float GetRadioMessageInterval( GameEventType event, int team ) const;
	void SetRadioMessageTimestamp( GameEventType event, int team );
	void ResetRadioMessageTimestamps();
	bool AddBot( const BotProfile* profile, BotProfileTeamType team );
	bool BotAddCommand( BotProfileTeamType team, bool isFromConsole );
	void MaintainBotQuota();
	void MonitorBotCVars();

	GameScenarioType GetScenario() const { return m_scenario; }
	int GetZoneCount() const { return m_zoneCount; }
	const Zone* GetRandomZone() const
	{
		if ( m_zoneCount == 0 )
			return NULL;
		return &m_zone[RANDOM_LONG( 0, m_zoneCount - 1 )];
	}
	bool IsBombPlanted() const { return m_isBombPlanted; }
	bool IsRoundOver() const { return m_isRoundOver; }
	float GetElapsedRoundTime() const { return gpGlobals->time - m_roundStartTimestamp; }
	float GetRoundStartTimestamp() const { return m_roundStartTimestamp; }
	Place GetNavPlace() const { return m_navPlace; }
	void SetNavPlace( Place place ) { m_navPlace = place; }
	CBasePlayer* GetBombDefuser() const { return m_bombDefuser; }
	CBaseEntity* GetLooseBomb() { return m_looseBomb; }
	CNavArea* GetLooseBombArea() const { return m_looseBombArea; }
	bool IsDefenseRushing() const { return m_isDefenseRushing; }
	static bool IsLearningMap() { return m_isLearningMap; }
	static bool IsAnalysisRequested() { return m_isAnalysisRequested; }

private:
	friend class CCSBot;
	friend class HuntState;
	friend class IdleState;
	friend class HideState;
	GameScenarioType m_scenario;
	Zone m_zone[4];
	int m_zoneCount;
	bool m_isBombPlanted;
	float m_bombPlantTimestamp;
	float m_earliestBombPlantTimestamp;
	CBasePlayer* m_bombDefuser;
	EHANDLE m_looseBomb;
	CNavArea* m_looseBombArea;
	bool m_isRoundOver;
	float m_radioMsgTimestamp[24][2];
	float m_lastSeenEnemyTimestamp;
	float m_roundStartTimestamp;
	bool m_isDefenseRushing;
	Place m_navPlace;
	CountdownTimer m_respawnTimer;
	bool m_isRespawnStarted;
	bool m_canRespawn;
	bool m_bServerActive;

	static float m_flNextCVarCheck;
	static NavEditCmdType m_editCmd;
	static bool m_isMapDataLoaded;
	static bool m_isLearningMap;
	static bool m_isAnalysisRequested;
};

inline CCSBotManager* TheCSBots()
{
	return static_cast<CCSBotManager*>( TheBots );
}

#endif
