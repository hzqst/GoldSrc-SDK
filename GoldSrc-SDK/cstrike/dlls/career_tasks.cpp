#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "game.h"
#include "career_tasks.h"
#include "tutor.h"
#include "hostage/hostage.h"
#include "bot/bot_util.h"
#include "bot/cs_bot_manager.h"
#include <string>
#include <vector>

extern int gmsgCZCareer;

CCareerTaskManager* TheCareerTasks = NULL;

namespace
{
struct CareerTaskDefinition
{
	std::string name, weapon;
	int count;
	bool mustLive, crossRounds, initiallyComplete;
};

struct CareerBotDefinition
{
	std::string profile;
	BotProfileTeamType team;
};

struct CareerSetting
{
	std::string name, value;
};

struct CareerRestartSession
{
	bool pending = false;
	bool settingsRestored = false;
	std::string map;
	int wins = 0, difference = 0;
	float botQuota = 0, botQuotaMatch = 0;
	std::vector<CareerTaskDefinition> tasks;
	std::vector<CareerBotDefinition> bots;
	std::vector<CareerSetting> settings;
};

std::vector<CareerTaskDefinition> activeTaskDefinitions;
CareerRestartSession restartSession;

bool IsPlainCareerRestart()
{
	const char* command = CMD_ARGV( 0 );
	return UTIL_IsGame( "czero" ) && g_engfuncs.pfnIsCareerMatch()
		&& command && !stricmp( command, "restart" );
}

bool IsRestartSessionMap()
{
	return restartSession.pending && UTIL_IsGame( "czero" ) && g_engfuncs.pfnIsCareerMatch()
		&& restartSession.map == STRING( gpGlobals->mapname );
}
}

// ResetGlobalState runs before the engine drops fake clients and destroys the old rules.
void CareerSessionPrepareForReset()
{
	restartSession = CareerRestartSession{};
	if ( !IsPlainCareerRestart() || !g_pGameRules )
		return;
	CHalfLifeMultiplay* rules = static_cast<CHalfLifeMultiplay*>( g_pGameRules );
	restartSession.pending = true;
	restartSession.map = STRING( gpGlobals->mapname );
	restartSession.wins = rules->m_iCareerMatchWins;
	restartSession.difference = rules->m_iRoundWinDifference;
	restartSession.tasks = activeTaskDefinitions;
	restartSession.botQuota = CVAR_GET_FLOAT( "bot_quota" );
	restartSession.botQuotaMatch = CVAR_GET_FLOAT( "bot_quota_match" );
	const char* settingNames[] = {
		"humans_join_team", "mp_startmoney", "mp_limitteams", "mp_autoteambalance",
		"mp_roundtime", "mp_c4timer", "mp_freezetime", "mp_friendlyfire", "mp_windifference",
		"bot_auto_vacate", "bot_join_after_player", "bot_defer_to_human", "bot_difficulty",
		"bot_join_team", "bot_allow_rogues", "bot_allow_pistols", "bot_allow_shotguns",
		"bot_allow_sub_machine_guns", "bot_allow_rifles", "bot_allow_machine_guns",
		"bot_allow_grenades", "bot_allow_snipers", "bot_allow_shield"
	};
	for ( const char* name : settingNames )
	{
		cvar_t* setting = CVAR_GET_POINTER( name );
		if ( setting )
			restartSession.settings.push_back( { name, setting->string } );
	}
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer* player = static_cast<CBasePlayer*>( UTIL_PlayerByIndex( i ) );
		if ( !player || !player->IsBot() || ( player->m_iTeam != CT && player->m_iTeam != TERRORIST ) )
			continue;
		const BotProfile* profile = static_cast<CBot*>( player )->GetProfile();
		if ( profile )
			restartSession.bots.push_back( { profile->GetName(), player->m_iTeam == CT ? BOT_TEAM_CT : BOT_TEAM_T } );
	}
}

void CareerSessionOnServerDeactivate()
{
	if ( !IsPlainCareerRestart() )
		restartSession = CareerRestartSession{};
}

void CareerSessionRestoreSettings()
{
	if ( !IsRestartSessionMap() )
	{
		restartSession = CareerRestartSession{};
		return;
	}
	for ( const CareerSetting& setting : restartSession.settings )
		CVAR_SET_STRING( setting.name.c_str(), setting.value.c_str() );
	// No random quota replacement may run before the named roster is rebuilt.
	CVAR_SET_FLOAT( "bot_quota", 0 );
	CVAR_SET_FLOAT( "bot_quota_match", 0 );
	restartSession.settingsRestored = true;
}

void CareerSessionRestoreRules( CHalfLifeMultiplay* rules )
{
	if ( restartSession.settingsRestored && IsRestartSessionMap() )
	{
		rules->SetCareerMatchLimit( restartSession.wins, restartSession.difference );
		CVAR_SET_FLOAT( "mp_windifference", restartSession.difference );
	}
}

void CareerSessionRestoreTasks()
{
	if ( restartSession.settingsRestored && IsRestartSessionMap() )
	{
		for ( const CareerTaskDefinition& task : restartSession.tasks )
			TheCareerTasks->AddTask( task.name.c_str(), task.weapon.c_str(), task.count,
				task.mustLive, task.crossRounds, task.initiallyComplete );
	}
}

void CareerSessionRestoreBots()
{
	if ( !restartSession.settingsRestored || !IsRestartSessionMap() )
		return;
	int restored = 0;
	for ( const CareerBotDefinition& bot : restartSession.bots )
	{
		const BotProfile* profile = TheBotProfiles->GetProfile( bot.profile.c_str(), bot.team );
		if ( profile && TheCSBots()->AddBot( profile, bot.team ) )
			++restored;
		else
			ALERT( at_console, "Could not restore Career bot %s\n", bot.profile.c_str() );
	}
	if ( restored == static_cast<int>( restartSession.bots.size() ) )
	{
		CVAR_SET_FLOAT( "bot_quota", restartSession.botQuota );
		CVAR_SET_FLOAT( "bot_quota_match", restartSession.botQuotaMatch );
	}
	else
	{
		// Keep missing named bots from being silently replaced with random profiles.
		CVAR_SET_FLOAT( "bot_quota", restored );
		CVAR_SET_FLOAT( "bot_quota_match", 0 );
	}
	restartSession = CareerRestartSession{};
}

class CPreventDefuseTask : public CCareerTask
{
public:
	CPreventDefuseTask( const char* taskName, GameEventType event, const char* weaponName, int n,
		bool mustLive, bool crossRounds, int id, bool isComplete );
	static CCareerTask* NewTask( const char* taskName, GameEventType event, const char* weaponName, int n,
		bool mustLive, bool crossRounds, int id, bool isComplete );
	void OnEvent( GameEventType event, CBasePlayer* pAttacker, CBasePlayer* pVictim );
	void Reset();
	bool IsTaskCompletableThisRound() const;

private:
	bool m_bombPlantedThisRound;
	bool m_defuseStartedThisRound;
};

typedef CCareerTask* ( *TaskFactoryFunction )( const char*, GameEventType, const char*, int, bool, bool, int, bool );

struct TaskInfo
{
	const char*				taskName;
	GameEventType			event;
	TaskFactoryFunction		factory;
};

static const TaskInfo taskInfo[] =
{
	{ "defuse", EVENT_BOMB_DEFUSED, CCareerTask::NewTask },
	{ "plant", EVENT_BOMB_PLANTED, CCareerTask::NewTask },
	{ "rescue", EVENT_HOSTAGE_RESCUED, CCareerTask::NewTask },
	{ "killall", EVENT_KILL_ALL, CCareerTask::NewTask },
	{ "kill", EVENT_KILL, CCareerTask::NewTask },
	{ "killwith", EVENT_KILL, CCareerTask::NewTask },
	{ "killblind", EVENT_KILL_FLASHBANGED, CCareerTask::NewTask },
	{ "killvip", EVENT_KILL, CCareerTask::NewTask },
	{ "headshot", EVENT_HEADSHOT, CCareerTask::NewTask },
	{ "headshotwith", EVENT_HEADSHOT, CCareerTask::NewTask },
	{ "winfast", EVENT_ROUND_WIN, CCareerTask::NewTask },
	{ "rescue", EVENT_HOSTAGE_RESCUED, CCareerTask::NewTask },
	{ "rescueall", EVENT_ALL_HOSTAGES_RESCUED, CCareerTask::NewTask },
	{ "injure", EVENT_PLAYER_TOOK_DAMAGE, CCareerTask::NewTask },
	{ "injurewith", EVENT_PLAYER_TOOK_DAMAGE, CCareerTask::NewTask },
	{ "killdefuser", EVENT_KILL, CCareerTask::NewTask },
	{ "stoprescue", EVENT_KILL, CCareerTask::NewTask },
	{ "defendhostages", EVENT_ROUND_WIN, CCareerTask::NewTask },
	{ "hostagessurvive", EVENT_ROUND_WIN, CCareerTask::NewTask },
	{ "preventdefuse", EVENT_ROUND_WIN, CPreventDefuseTask::NewTask },
	{ NULL, EVENT_INVALID, CCareerTask::NewTask }
};

CCareerTask::CCareerTask( const char* taskName, GameEventType event, const char* weaponName, int n,
	bool mustLive, bool crossRounds, int id, bool isComplete )
{
	m_isComplete = isComplete;
	m_name = taskName;
	m_id = id;
	m_event = event;
	m_eventsNeeded = n;
	m_eventsSeen = 0;
	m_mustLive = mustLive;
	m_crossRounds = crossRounds;
	m_diedThisRound = false;
	m_weaponId = AliasToWeaponID( weaponName );
	m_weaponClassId = AliasToWeaponClass( weaponName );
	m_rescuer = !strcmp( taskName, "stoprescue" );
	m_defuser = !strcmp( taskName, "killdefuser" );
	m_vip = !stricmp( taskName, "killvip" );

	if ( event == EVENT_ALL_HOSTAGES_RESCUED )
	{
		m_mustLive = true;
		m_crossRounds = false;
	}

	if ( m_isComplete )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
			WRITE_STRING( "TASKDONE" );
			WRITE_BYTE( m_id );
		MESSAGE_END();
	}
}

CCareerTask* CCareerTask::NewTask( const char* taskName, GameEventType event, const char* weaponName, int n,
	bool mustLive, bool crossRounds, int id, bool isComplete )
{
	return new CCareerTask( taskName, event, weaponName, n, mustLive, crossRounds, id, isComplete );
}

void CCareerTask::SendPartialNotification()
{
	MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
		WRITE_STRING( "TASKPART" );
		WRITE_BYTE( m_id );
		WRITE_SHORT( m_eventsSeen );
	MESSAGE_END();
	UTIL_LogPrintf( "Career Task Partial %d %d\n", m_id, m_eventsSeen );
}

void CCareerTask::Reset()
{
	m_eventsSeen = 0;
	m_isComplete = false;

	MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
		WRITE_STRING( "TASKUNDONE" );
		WRITE_BYTE( m_id );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
		WRITE_STRING( "TASKPART" );
		WRITE_BYTE( m_id );
		WRITE_SHORT( m_eventsSeen );
	MESSAGE_END();
}

void CCareerTask::OnWeaponKill( int weaponId, int weaponClassId, bool headshot, bool killerHasShield,
	CBasePlayer* pAttacker, CBasePlayer* pVictim )
{
	if ( m_isComplete || ( m_event != EVENT_KILL && ( m_event != EVENT_HEADSHOT || !headshot ) ) || !pVictim )
		return;
	if ( ( m_defuser && !pVictim->m_bIsDefusing ) || ( m_vip && !pVictim->m_bIsVIP ) )
		return;
	if ( m_rescuer )
	{
		int hostages = 0;
		CBaseEntity* pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "hostage_entity" ) ) != NULL )
		{
			CHostage* pHostage = (CHostage*)pEntity;
			if ( pHostage->pev->takedamage == DAMAGE_YES && pHostage->IsFollowingSomeone() &&
				pHostage->GetLeader() == pVictim )
				++hostages;
		}
		if ( !hostages )
			return;
	}

	if ( m_weaponId == WEAPON_SHIELDGUN )
	{
		if ( !killerHasShield )
			return;
	}
	else if ( m_weaponId )
	{
		if ( m_weaponId != weaponId )
			return;
	}
	else if ( !m_weaponClassId || m_weaponClassId != weaponClassId )
		return;
	++m_eventsSeen;
	SendPartialNotification();
}

void CCareerTask::OnWeaponInjury( int weaponId, int weaponClassId, bool attackerHasShield, CBasePlayer* pAttacker )
{
	if ( m_isComplete || m_event != EVENT_PLAYER_TOOK_DAMAGE )
		return;
	if ( m_weaponId == WEAPON_SHIELDGUN )
	{
		if ( !attackerHasShield )
			return;
	}
	else if ( m_weaponId )
	{
		if ( m_weaponId != weaponId )
			return;
	}
	else if ( !m_weaponClassId || m_weaponClassId != weaponClassId )
		return;
	++m_eventsSeen;
	SendPartialNotification();
}

void CCareerTask::OnEvent( GameEventType event, CBasePlayer* pAttacker, CBasePlayer* pVictim )
{
	if ( m_isComplete )
		return;

	if ( event == m_event )
	{
		if ( ( m_defuser && !pVictim->m_bIsDefusing ) || ( m_vip && !pVictim->m_bIsVIP ) )
			return;
		if ( m_rescuer )
		{
			int hostages = 0;
			CBaseEntity* pEntity = NULL;
			while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "hostage_entity" ) ) != NULL )
			{
				CHostage* pHostage = (CHostage*)pEntity;
				if ( pHostage->pev->takedamage == DAMAGE_YES && pHostage->IsFollowingSomeone() &&
					pHostage->GetLeader() == pVictim )
					++hostages;
			}
			if ( !hostages )
				return;
		}

		if ( ( event == EVENT_KILL || event == EVENT_HEADSHOT || event == EVENT_PLAYER_TOOK_DAMAGE ) &&
			( m_weaponId || m_weaponClassId ) )
		{
		}
		else if ( event == EVENT_ROUND_WIN )
		{
			if ( !strcmp( m_name, "defendhostages" ) )
			{
				int rescued = 0;
				CBaseEntity* pHostage = NULL;
				while ( ( pHostage = UTIL_FindEntityByClassname( pHostage, "hostage_entity" ) ) != NULL )
				{
					if ( pHostage->pev->takedamage != DAMAGE_YES && pHostage->pev->deadflag != DEAD_DEAD )
						++rescued;
				}
				if ( !rescued )
				{
					++m_eventsSeen;
					SendPartialNotification();
				}
			}
			else if ( !strcmp( m_name, "hostagessurvive" ) )
			{
				int dead = 0;
				CBaseEntity* pHostage = NULL;
				while ( ( pHostage = UTIL_FindEntityByClassname( pHostage, "hostage_entity" ) ) != NULL )
				{
					if ( pHostage->pev->deadflag == DEAD_DEAD )
						++dead;
				}
				if ( !dead )
				{
					++m_eventsSeen;
					SendPartialNotification();
				}
			}
			else if ( !strcmp( m_name, "winfast" ) )
			{
				if ( TheCareerTasks->GetRoundElapsedTime() <= m_eventsNeeded )
				{
					m_eventsSeen = m_eventsNeeded;
					SendPartialNotification();
				}
			}
			else if ( IsTaskCompletableThisRound() )
			{
				++m_eventsSeen;
				SendPartialNotification();
			}
		}
		else
		{
			++m_eventsSeen;
			SendPartialNotification();
		}

		if ( !m_mustLive && m_eventsSeen >= m_eventsNeeded && IsTaskCompletableThisRound() )
		{
			CBasePlayer* pPlayer = UTIL_GetLocalPlayer();
			EMIT_SOUND_DYN( pPlayer->edict(), CHAN_VOICE, "events/task_complete.wav", VOL_NORM, ATTN_NORM, 0, PITCH_NORM );
			m_isComplete = true;
			MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
				WRITE_STRING( "TASKDONE" );
				WRITE_BYTE( m_id );
			MESSAGE_END();
			if ( TheTutor )
				TheTutor->OnEvent( EVENT_CAREER_TASK_DONE );
			UTIL_LogPrintf( "Career Task Done %d\n", m_id );
			if ( m_event == EVENT_ROUND_WIN && !strcmp( m_name, "winfast" ) )
			{
				TheCareerTasks->SetFinishedTaskTime( (int)TheCareerTasks->GetRoundElapsedTime() );
				UTIL_GetLocalPlayer()->SyncRoundTimer();
			}
			return;
		}
	}

	if ( event >= EVENT_ROUND_DRAW && event <= EVENT_ROUND_LOSS )
	{
		if ( m_mustLive )
		{
			if ( m_eventsSeen >= m_eventsNeeded && !m_diedThisRound && IsTaskCompletableThisRound() )
			{
				CBasePlayer* pPlayer = UTIL_GetLocalPlayer();
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_VOICE, "events/task_complete.wav",
					VOL_NORM, ATTN_NORM, 0, PITCH_NORM );
				m_isComplete = true;
				MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
					WRITE_STRING( "TASKDONE" );
					WRITE_BYTE( m_id );
				MESSAGE_END();
				UTIL_LogPrintf( "Career Task Done %d\n", m_id );
				if ( m_event == EVENT_ROUND_WIN && !strcmp( m_name, "winfast" ) )
				{
					TheCareerTasks->SetFinishedTaskTime( (int)TheCareerTasks->GetRoundElapsedTime() );
					UTIL_GetLocalPlayer()->SyncRoundTimer();
				}
				if ( TheTutor )
					TheTutor->OnEvent( EVENT_CAREER_TASK_DONE );
			}
			m_diedThisRound = false;
			m_eventsSeen = 0;
			SendPartialNotification();
		}
	}
	else if ( event == EVENT_DIE && ( m_mustLive || m_crossRounds ) )
	{
		m_eventsSeen = 0;
		SendPartialNotification();
		m_diedThisRound = true;
	}
}

CPreventDefuseTask::CPreventDefuseTask( const char* taskName, GameEventType event, const char* weaponName, int n,
	bool mustLive, bool crossRounds, int id, bool isComplete )
	: CCareerTask( taskName, event, weaponName, n, mustLive, crossRounds, id, isComplete )
{
	m_bombPlantedThisRound = false;
	m_defuseStartedThisRound = false;
}

CCareerTask* CPreventDefuseTask::NewTask( const char* taskName, GameEventType event, const char* weaponName, int n,
	bool mustLive, bool crossRounds, int id, bool isComplete )
{
	return new CPreventDefuseTask( taskName, event, weaponName, n, mustLive, crossRounds, id, isComplete );
}

void CPreventDefuseTask::Reset()
{
	m_bombPlantedThisRound = false;
	m_defuseStartedThisRound = false;
	CCareerTask::Reset();
}

void CPreventDefuseTask::OnEvent( GameEventType event, CBasePlayer* pAttacker, CBasePlayer* pVictim )
{
	if ( m_isComplete )
		return;

	if ( event == EVENT_BOMB_PLANTED )
		m_bombPlantedThisRound = true;
	else if ( event == EVENT_BOMB_DEFUSING )
		m_defuseStartedThisRound = true;

	CCareerTask::OnEvent( event, pAttacker, pVictim );
	if ( event >= EVENT_ROUND_DRAW && event <= EVENT_ROUND_LOSS )
	{
		m_bombPlantedThisRound = false;
		m_defuseStartedThisRound = false;
	}
}

bool CPreventDefuseTask::IsTaskCompletableThisRound() const
{
	return m_bombPlantedThisRound && !m_defuseStartedThisRound;
}

CCareerTaskManager::CCareerTaskManager()
{
	m_taskTime = 0;
	Reset( true );
}

void CCareerTaskManager::Create()
{
	if ( TheCareerTasks )
		TheCareerTasks->Reset( true );
	else
		TheCareerTasks = new CCareerTaskManager;
}

void CCareerTaskManager::Reset( bool deleteTasks )
{
	if ( deleteTasks )
	{
		activeTaskDefinitions.clear();
		for ( CareerTaskList::iterator i = m_tasks.begin(); i != m_tasks.end(); ++i )
			delete *i;
		m_tasks.clear();
		m_nextId = 0;
	}
	else
	{
		for ( CareerTaskList::iterator i = m_tasks.begin(); i != m_tasks.end(); ++i )
			( *i )->Reset();
	}

	m_roundStartTime = gpGlobals->time + CVAR_GET_FLOAT( "mp_freezetime" );
	m_finishedTaskTime = 0;
	m_finishedTaskRound = 0;
	m_shouldLatchRoundEndMessage = false;
}

void CCareerTaskManager::AddTask( const char* taskName, const char* weaponName, int eventCount,
	bool mustLive, bool crossRounds, bool isComplete )
{
	activeTaskDefinitions.push_back( { taskName, weaponName, eventCount, mustLive, crossRounds, isComplete } );
	++m_nextId;
	for ( const TaskInfo* pInfo = taskInfo; pInfo->taskName; ++pInfo )
	{
		if ( !stricmp( pInfo->taskName, taskName ) )
		{
			m_tasks.push_back( pInfo->factory( pInfo->taskName, pInfo->event, weaponName, eventCount,
				mustLive, crossRounds, m_nextId, isComplete ) );
			if ( pInfo->event == EVENT_ROUND_WIN && !strcmp( taskName, "winfast" ) )
			{
				m_taskTime = eventCount;
				if ( isComplete )
					m_finishedTaskTime = eventCount;
			}
			return;
		}
	}

	MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
		WRITE_STRING( "TASKDONE" );
		WRITE_BYTE( m_nextId );
	MESSAGE_END();
}

void CCareerTaskManager::SyncToPlayer( CBasePlayer* player ) const
{
	auto task = m_tasks.begin();
	for ( int id = 1; id <= m_nextId; ++id )
	{
		// Unknown task types are intentionally auto-completed by AddTask.
		bool complete = true;
		int partial = 0;
		if ( task != m_tasks.end() && ( *task )->m_id == id )
		{
			complete = ( *task )->m_isComplete;
			partial = ( *task )->m_eventsSeen;
			++task;
		}
		MESSAGE_BEGIN( MSG_ONE, gmsgCZCareer, NULL, player->edict() );
			WRITE_STRING( complete ? "TASKDONE" : "TASKUNDONE" );
			WRITE_BYTE( id );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ONE, gmsgCZCareer, NULL, player->edict() );
			WRITE_STRING( "TASKPART" );
			WRITE_BYTE( id );
			WRITE_SHORT( partial );
		MESSAGE_END();
	}
}

void CCareerTaskManager::HandleEvent( GameEventType event, CBasePlayer* pAttacker, CBasePlayer* pVictim )
{
	if ( event == EVENT_ROUND_START )
		m_roundStartTime = gpGlobals->time;
	else if ( event >= EVENT_ROUND_DRAW && event <= EVENT_ROUND_LOSS && m_shouldLatchRoundEndMessage )
	{
		m_roundEndMessage = event;
		return;
	}

	for ( CareerTaskList::iterator i = m_tasks.begin(); i != m_tasks.end(); ++i )
		( *i )->OnEvent( event, pAttacker, pVictim );
}

void CCareerTaskManager::HandleWeaponKill( int weaponId, int weaponClassId, bool headshot, bool killerHasShield,
	CBasePlayer* pAttacker, CBasePlayer* pVictim )
{
	for ( CareerTaskList::iterator i = m_tasks.begin(); i != m_tasks.end(); ++i )
		( *i )->OnWeaponKill( weaponId, weaponClassId, headshot, killerHasShield, pAttacker, pVictim );
}

void CCareerTaskManager::HandleEnemyKill( bool wasBlind, const char* weaponName, bool headshot, bool killerHasShield,
	CBasePlayer* pAttacker, CBasePlayer* pVictim )
{
	int weaponClass = AliasToWeaponClass( weaponName );
	int weapon = AliasToWeaponID( weaponName );
	HandleWeaponKill( weapon, weaponClass, headshot, killerHasShield, pAttacker, pVictim );
	HandleEvent( EVENT_KILL, pAttacker, pVictim );

	if ( headshot )
		HandleEvent( EVENT_HEADSHOT, pAttacker, pVictim );
	if ( wasBlind )
		HandleEvent( EVENT_KILL_FLASHBANGED, pAttacker, pVictim );
}

void CCareerTaskManager::HandleWeaponInjury( int weaponId, int weaponClassId, bool attackerHasShield,
	CBasePlayer* pAttacker )
{
	for ( CareerTaskList::iterator i = m_tasks.begin(); i != m_tasks.end(); ++i )
		( *i )->OnWeaponInjury( weaponId, weaponClassId, attackerHasShield, pAttacker );
}

void CCareerTaskManager::HandleEnemyInjury( const char* weaponName, bool attackerHasShield, CBasePlayer* pAttacker )
{
	int weaponClass = AliasToWeaponClass( weaponName );
	int weapon = AliasToWeaponID( weaponName );
	HandleWeaponInjury( weapon, weaponClass, attackerHasShield, pAttacker );
	HandleEvent( EVENT_PLAYER_TOOK_DAMAGE );
}

void CCareerTaskManager::HandleDeath( int team, CBasePlayer* pPlayer )
{
	int enemyTeam = !strcmp( humans_join_team.string, "CT" ) ? TERRORIST : CT;
	if ( enemyTeam != team )
		return;

	int alive = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer* pOther = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( pOther && pOther->m_iTeam == enemyTeam && pOther->IsAlive() )
			++alive;
	}
	if ( !alive )
		HandleEvent( EVENT_KILL_ALL );
}

bool CCareerTaskManager::AreAllTasksComplete()
{
	for ( CareerTaskList::iterator i = m_tasks.begin(); i != m_tasks.end(); ++i )
	{
		if ( !( *i )->IsComplete() )
			return false;
	}
	return true;
}

int CCareerTaskManager::GetNumRemainingTasks()
{
	int remaining = 0;
	for ( CareerTaskList::iterator i = m_tasks.begin(); i != m_tasks.end(); ++i )
	{
		if ( !( *i )->IsComplete() )
			++remaining;
	}
	return remaining;
}

void CCareerTaskManager::SetFinishedTaskTime( int time )
{
	m_finishedTaskTime = time;
	m_finishedTaskRound = ( (CHalfLifeMultiplay*)g_pGameRules )->m_iTotalRoundsPlayed;
}

void CCareerTaskManager::UnlatchRoundEndMessage()
{
	m_shouldLatchRoundEndMessage = false;
	HandleEvent( m_roundEndMessage );
}
