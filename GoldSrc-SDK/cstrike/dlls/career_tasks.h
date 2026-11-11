#ifndef CAREER_TASKS_H
#define CAREER_TASKS_H

#include <list>
#include "GameEvent.h"

class CBasePlayer;
class CCareerTask;
class CHalfLifeMultiplay;

void CareerSessionPrepareForReset();
void CareerSessionOnServerDeactivate();
void CareerSessionRestoreSettings();
void CareerSessionRestoreRules( CHalfLifeMultiplay* rules );
void CareerSessionRestoreTasks();
void CareerSessionRestoreBots();
typedef std::list<CCareerTask*> CareerTaskList;

class CCareerTask
{
	friend class CCareerTaskManager;
public:
	CCareerTask( const char* taskName, GameEventType event, const char* weaponName, int n,
		bool mustLive, bool crossRounds, int id, bool isComplete );
	static CCareerTask* NewTask( const char* taskName, GameEventType event, const char* weaponName, int n,
		bool mustLive, bool crossRounds, int id, bool isComplete );

	virtual void OnEvent( GameEventType event, CBasePlayer* pAttacker, CBasePlayer* pVictim );
	void OnWeaponKill( int weaponId, int weaponClassId, bool headshot, bool killerHasShield,
		CBasePlayer* pAttacker, CBasePlayer* pVictim );
	void OnWeaponInjury( int weaponId, int weaponClassId, bool attackerHasShield, CBasePlayer* pAttacker );

	bool IsComplete() { return m_isComplete; }
	const char* GetTaskName() { return m_name; }
	int GetWeaponId() { return m_weaponId; }
	int GetWeaponClassId() { return m_weaponClassId; }
	virtual void Reset();
	bool IsValidFor( CBasePlayer* pPlayer ) const;
	void SendPartialNotification();
	virtual bool IsTaskCompletableThisRound() const { return true; }

protected:
	bool			m_isComplete;
	const char*		m_name;
	int				m_id;
	GameEventType	m_event;
	int				m_eventsNeeded;
	int				m_eventsSeen;

	bool			m_mustLive;
	bool			m_crossRounds;
	bool			m_diedThisRound;
	int				m_weaponId;
	int				m_weaponClassId;
	bool			m_rescuer;
	bool			m_defuser;
	bool			m_vip;
};

class CCareerTaskManager
{
public:
	static void Create();
	CCareerTaskManager();
	void Reset( bool deleteTasks );
	void AddTask( const char* taskName, const char* weaponName, int eventCount,
		bool mustLive, bool crossRounds, bool isComplete );
	void SyncToPlayer( CBasePlayer* player ) const;

	void HandleEvent( GameEventType event, CBasePlayer* pAttacker = NULL, CBasePlayer* pVictim = NULL );
	void HandleEnemyKill( bool wasBlind, const char* weaponName, bool headshot, bool killerHasShield,
		CBasePlayer* pAttacker, CBasePlayer* pVictim );
	void HandleWeaponKill( int weaponId, int weaponClassId, bool headshot, bool killerHasShield,
		CBasePlayer* pAttacker, CBasePlayer* pVictim );
	void HandleDeath( int team, CBasePlayer* pPlayer );
	void HandleWeaponInjury( int weaponId, int weaponClassId, bool attackerHasShield, CBasePlayer* pAttacker );
	void HandleEnemyInjury( const char* weaponName, bool attackerHasShield, CBasePlayer* pAttacker );

	bool AreAllTasksComplete();
	int GetNumRemainingTasks();
	float GetRoundElapsedTime() { return gpGlobals->time - m_roundStartTime; }
	int GetTaskTime() { return m_taskTime; }
	void SetFinishedTaskTime( int time );
	int GetFinishedTaskTime() { return m_finishedTaskTime; }
	int GetFinishedTaskRound() { return m_finishedTaskRound; }
	CareerTaskList* GetTasks() { return &m_tasks; }
	void LatchRoundEndMessage() { m_shouldLatchRoundEndMessage = true; }
	void UnlatchRoundEndMessage();

private:
	CareerTaskList	m_tasks;
	int				m_nextId;
	float			m_roundStartTime;
	int				m_taskTime;
	int				m_finishedTaskTime;
	int				m_finishedTaskRound;
	GameEventType	m_roundEndMessage;
	bool			m_shouldLatchRoundEndMessage;
};

extern CCareerTaskManager* TheCareerTasks;

#endif
