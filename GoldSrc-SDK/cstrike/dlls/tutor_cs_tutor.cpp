#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "bot_util.h"
#include "tutor.h"
#include "tutor_cs_tutor.h"
#include "tutor_cs_states.h"
#include "career_tasks.h"
#include "hostage/hostage.h"
#include "bot/cs_bot_manager.h"

char* MP_COM_Parse( char* data );
char* MP_COM_GetToken();

void CCSTutor::CheckBuyZoneMessages()
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL || m_currentlyShownMessageID == BUY_TIME_BEGIN )
		return;

	CBasePlayerItem* secondary = player->m_rgpPlayerItems[2];
	if ( player->m_rgpPlayerItems[1] != NULL )
	{
		if ( player->NeedsPrimaryAmmo() && player->CanAffordPrimaryAmmo() )
		{
			TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_PRIMARY_AMMO, NULL, NULL );
			return;
		}
	}
	else if ( player->CanAffordPrimary() )
	{
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_PRIMARY_WEAPON, NULL, NULL );
		return;
	}

	if ( secondary != NULL && player->NeedsSecondaryAmmo() && player->CanAffordSecondaryAmmo() )
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_SECONDARY_AMMO, NULL, NULL );
	else if ( player->NeedsArmor() && player->CanAffordArmor() )
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_ARMOR, NULL, NULL );
	else if ( player->NeedsDefuseKit() && player->CanAffordDefuseKit() )
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_DEFUSE_KIT, NULL, NULL );
	else if ( player->NeedsGrenade() && player->CanAffordGrenade() )
		TheTutor->OnEvent( EVENT_TUTOR_NEED_TO_BUY_GRENADE, NULL, NULL );
	else
		TheTutor->OnEvent( EVENT_TUTOR_NOT_BUYING_ANYTHING, NULL, NULL );
}

void CCSTutor::CheckForNeedToReload( bool isPassiveCheck )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL || !player->IsAlive() )
		return;

	CBasePlayerItem* item = player->m_pActiveItem;
	if ( item == NULL || item->IsWeapon() != TRUE )
		return;

	ItemInfo info;
	item->GetItemInfo( &info );
	if ( (unsigned int)info.iSlot > 1 )
		return;

	CBasePlayerWeapon* weapon = (CBasePlayerWeapon*)item;
	if ( player->AmmoInventory( weapon->m_iPrimaryAmmoType ) <= 0 )
	{
		if ( weapon->m_iClip == 0 )
		{
			if ( !isPassiveCheck )
			{
				TutorMessage* message = GetTutorMessageDefinition( YOU_ARE_OUT_OF_AMMO );
				if ( message != NULL )
					message->m_lastCloseTime = 0;
			}
			CreateAndAddEventToList( YOU_ARE_OUT_OF_AMMO, NULL, NULL );
		}
		return;
	}

	if ( isPassiveCheck )
	{
		if ( 2 * weapon->m_iClip >= CBasePlayerItem::ItemInfoArray[item->m_iId].iMaxClip || weapon->m_fInReload )
			return;
		CreateAndAddEventToList( YOU_SHOULD_RELOAD, NULL, NULL );
	}
	else if ( 5 * weapon->m_iClip < CBasePlayerItem::ItemInfoArray[item->m_iId].iMaxClip && !weapon->m_fInReload )
	{
		TutorMessage* message = GetTutorMessageDefinition( YOU_SHOULD_RELOAD );
		if ( message != NULL )
			message->m_lastCloseTime = 0;
		CreateAndAddEventToList( YOU_SHOULD_RELOAD, NULL, NULL );
	}
	else
	{
		TutorMessage* message = GetTutorMessageDefinition( YOU_SHOULD_RELOAD );
		if ( message != NULL )
			message->m_lastCloseTime = gpGlobals->time;
	}
}

TutorMessageID CCSTutor::CheckForInBombZone()
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL || player->m_iTeam != CT )
		return TUTOR_NUM_MESSAGES;

	CBaseEntity* bombsite = NULL;
	while ( (bombsite = UTIL_FindEntityByClassname( bombsite, "func_bomb_target" )) != NULL )
	{
		if ( IsEntityInBombsite( bombsite, player ) && IsBombPlantedInBombsite( bombsite ) )
			return YOU_SEE_PLANTED_BOMB_CT;
	}
	bombsite = NULL;
	while ( (bombsite = UTIL_FindEntityByClassname( bombsite, "info_bomb_target" )) != NULL )
	{
		if ( IsEntityInBombsite( bombsite, player ) && IsBombPlantedInBombsite( bombsite ) )
			return YOU_SEE_PLANTED_BOMB_CT;
	}
	return TUTOR_NUM_MESSAGES;
}

void CCSTutor::CheckForTimeRunningOut()
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;

	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	if ( !rules->m_bFreezePeriod && rules->m_iRoundTimeSecs - gpGlobals->time + rules->m_fRoundCount <= 30.0f )
	{
		if ( rules->m_bMapHasBombTarget )
		{
			if ( player->m_iTeam == TERRORIST )
				CreateAndAddEventToList( TIME_RUNNING_OUT_DE_T, NULL, NULL );
			else if ( player->m_iTeam == CT )
				CreateAndAddEventToList( TIME_RUNNING_OUT_DE_CT, NULL, NULL );
		}
		else if ( UTIL_FindEntityByClassname( NULL, "hostage_entity" ) != NULL )
		{
			if ( player->m_iTeam == TERRORIST )
				CreateAndAddEventToList( TIME_RUNNING_OUT_CS_T, NULL, NULL );
			else if ( player->m_iTeam == CT )
				CreateAndAddEventToList( TIME_RUNNING_OUT_CS_CT, NULL, NULL );
		}
	}
}

void CCSTutor::CheckForHostageViewable()
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;

	bool found = false;
	CHostage* hostage = NULL;
	while ( (hostage = (CHostage*)UTIL_FindEntityByClassname( hostage, "hostage_entity" )) != NULL )
	{
		if ( hostage->pev->takedamage == DAMAGE_YES && !hostage->IsFollowingSomeone() )
		{
			if ( IsEntityInViewOfPlayer( hostage, player ) && !found )
			{
				if ( player->m_iTeam == CT )
				{
					TutorMessage* message = GetTutorMessageDefinition( YOU_SEE_HOSTAGE_CT );
					if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
						return;
					found = true;
					CreateAndAddEventToList( YOU_SEE_HOSTAGE_CT, NULL, NULL );
				}
				else
				{
					found = true;
					if ( player->m_iTeam == TERRORIST )
					{
						TutorMessage* message = GetTutorMessageDefinition( YOU_SEE_HOSTAGE_T );
						if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
							return;
						CreateAndAddEventToList( YOU_SEE_HOSTAGE_T, NULL, NULL );
					}
				}
			}
		}
	}
}

bool CCSTutor::CheckForAllHostagesFollowingSomeone()
{
	CHostage* hostage = NULL;
	while ( (hostage = (CHostage*)UTIL_FindEntityByClassname( hostage, "hostage_entity" )) != NULL )
	{
		if ( hostage->IsAlive() && (hostage->pev->takedamage != DAMAGE_YES || !hostage->IsFollowingSomeone()) )
		{
			UTIL_FindEntityByClassname( hostage, "hostage_entity" );
			return false;
		}
	}
	return true;
}

CBaseEntity* CCSTutor::GetEntityForMessageID( int id, CBaseEntity* last )
{
	switch ( id )
	{
	case YOU_SEE_FRIEND:
	case YOU_SEE_ENEMY:
		return UTIL_FindEntityByClassname( last, "player" );
	case YOU_SEE_FRIEND_CORPSE:
	{
		CBasePlayer* player = UTIL_GetLocalPlayer();
		if ( player == NULL || m_clientCorpseList.empty() )
			return NULL;
		if ( last == NULL )
			return (CBaseEntity*)m_clientCorpseList.front();

		ClientCorpseList::iterator corpse = m_clientCorpseList.begin();
		while ( corpse != m_clientCorpseList.end() && (CBaseEntity*)*corpse != last )
			++corpse;
		if ( corpse == m_clientCorpseList.end() )
			return NULL;
		while ( ++corpse != m_clientCorpseList.end() )
		{
			if ( (*corpse)->m_team == player->m_iTeam )
				return (CBaseEntity*)*corpse;
		}
		return NULL;
	}
	case YOU_SEE_LOOSE_BOMB_T:
	case YOU_SEE_LOOSE_BOMB_CT:
	case YOU_SEE_BOMB_CARRIER_T:
	case YOU_SEE_BOMB_CARRIER_CT:
		return UTIL_FindEntityByClassname( last, "weapon_c4" );
	case YOU_SEE_PLANTED_BOMB_T:
	case YOU_SEE_PLANTED_BOMB_CT:
		return UTIL_FindEntityByClassname( last, "grenade" );
	case YOU_SEE_LOOSE_WEAPON:
		return UTIL_FindEntityByClassname( last, "weaponbox" );
	case YOU_SEE_LOOSE_DEFUSER:
		return UTIL_FindEntityByClassname( last, "item_thighpack" );
	case YOU_SEE_HOSTAGE_T:
	case YOU_SEE_HOSTAGE_CT:
	case YOU_SEE_HOSTAGE_CT_EXAMINE:
		return UTIL_FindEntityByClassname( last, "hostage_entity" );
	default:
		return NULL;
	}
}

void CCSTutor::CheckForBombViewable()
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;

	CGrenade* bomb = (CGrenade*)UTIL_FindEntityByClassname( NULL, "grenade" );
	if ( bomb != NULL && bomb->m_bIsC4 && IsEntityInViewOfPlayer( bomb, player ) )
	{
		if ( player->m_iTeam == CT || player->m_iTeam == TERRORIST )
		{
			TutorMessageID id = player->m_iTeam == CT ? YOU_SEE_PLANTED_BOMB_CT : YOU_SEE_PLANTED_BOMB_T;
			TutorMessage* message = GetTutorMessageDefinition( id );
			if ( message == NULL || message->m_class != TUTORMESSAGECLASS_EXAMINE )
				CreateAndAddEventToList( id, NULL, NULL );
		}
		return;
	}

	CBasePlayer* carrier = NULL;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer* other = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( other != NULL && other->m_bHasC4 )
		{
			carrier = other;
			break;
		}
	}

	CBaseEntity* looseBomb = UTIL_FindEntityByClassname( NULL, "weapon_c4" );
	if ( looseBomb != NULL && IsEntityInViewOfPlayer( looseBomb, player ) )
	{
		TutorMessageID id;
		if ( carrier != NULL )
		{
			if ( carrier == player )
				return;
			if ( player->m_iTeam == CT )
				id = YOU_SEE_BOMB_CARRIER_CT;
			else if ( player->m_iTeam == TERRORIST )
				id = YOU_SEE_BOMB_CARRIER_T;
			else
				return;
		}
		else if ( player->m_iTeam == CT )
			id = YOU_SEE_LOOSE_BOMB_CT;
		else if ( player->m_iTeam == TERRORIST )
			id = YOU_SEE_LOOSE_BOMB_T;
		else
			return;

		TutorMessage* message = GetTutorMessageDefinition( id );
		if ( message == NULL || message->m_class != TUTORMESSAGECLASS_EXAMINE )
			CreateAndAddEventToList( id, NULL, NULL );
	}
}

void CCSTutor::CheckForBombsiteViewable()
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;

	TutorMessageID id = CheckForInBombZone();
	if ( id != TUTOR_NUM_MESSAGES )
	{
		CreateAndAddEventToList( id, NULL, NULL );
		return;
	}

	if ( player->m_iTeam == CT )
		id = YOU_SEE_BOMBSITE_CT;
	else if ( player->m_iTeam == TERRORIST )
	{
		if ( player->m_bHasC4 )
			id = YOU_SEE_BOMBSITE_T_BOMB;
		else if ( TheBots == NULL || !TheCSBots()->IsBombPlanted() )
			id = YOU_SEE_BOMBSITE_T;
	}

	TutorMessage* message = GetTutorMessageDefinition( id );
	if ( message != NULL && message->m_class == TUTORMESSAGECLASS_EXAMINE )
		return;

	CBaseEntity* bombsite = NULL;
	while ( (bombsite = UTIL_FindEntityByClassname( bombsite, "func_bomb_target" )) != NULL )
	{
		if ( IsBombsiteInViewOfPlayer( bombsite, player ) )
		{
			CreateAndAddEventToList( id, NULL, NULL );
			UTIL_FindEntityByClassname( bombsite, "func_bomb_target" );
			UTIL_FindEntityByClassname( NULL, "info_bomb_target" );
			return;
		}
	}
	bombsite = NULL;
	while ( (bombsite = UTIL_FindEntityByClassname( bombsite, "info_bomb_target" )) != NULL )
	{
		if ( IsBombsiteInViewOfPlayer( bombsite, player ) )
		{
			CreateAndAddEventToList( id, NULL, NULL );
			UTIL_FindEntityByClassname( bombsite, "info_bomb_target" );
			return;
		}
	}
}

void CCSTutor::CheckExamineMessages( float time )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;

	for ( int i = 0; i < TUTOR_NUM_MESSAGES; ++i )
	{
		TutorMessageID id = (TutorMessageID)i;
		TutorMessage* message = GetTutorMessageDefinition( id );
		if ( message == NULL || message->m_class != TUTORMESSAGECLASS_EXAMINE )
			continue;

		CBaseEntity* entity = GetEntityForMessageID( id, NULL );
		while ( entity != NULL )
		{
			bool looking;
			if ( id == YOU_SEE_FRIEND_CORPSE || id == YOU_SEE_ENEMY_CORPSE )
				looking = IsPlayerLookingAtPosition( (Vector*)entity, player );
			else if ( id == YOU_SEE_FRIEND || id == YOU_SEE_ENEMY || id == YOU_SEE_HOSTAGE_CT_EXAMINE )
				looking = IsPlayerLookingAtEntity( entity, player );
			else
				looking = IsPlayerLookingAtPosition( &entity->pev->origin, player );

			if ( looking )
			{
				if ( message->m_examineStartTime == -1.0f )
					message->m_examineStartTime = time;
				else if ( time - message->m_examineStartTime > cv_tutor_examine_time.value )
				{
					bool valid = true;
					if ( id == YOU_SEE_FRIEND )
						valid = entity->IsPlayer() && entity->IsAlive() && ((CBasePlayer*)entity)->m_iTeam == player->m_iTeam;
					else if ( id == YOU_SEE_ENEMY )
					{
						valid = entity->IsPlayer() && entity->IsAlive();
						if ( valid )
						{
							TeamName team = ((CBasePlayer*)entity)->m_iTeam;
							if ( team == CT )
								valid = player->m_iTeam == TERRORIST;
							else if ( team == TERRORIST )
								valid = player->m_iTeam == CT;
						}
					}
					else if ( id == YOU_SEE_HOSTAGE_CT_EXAMINE )
						valid = entity->pev->takedamage == DAMAGE_YES && !((CHostage*)entity)->IsFollowingSomeone();

					if ( valid )
						CreateAndAddEventToList( id, entity, NULL );
				}
				break;
			}
			entity = GetEntityForMessageID( id, entity );
		}
		if ( entity == NULL )
			message->m_examineStartTime = -1.0f;
	}
}

char* TutorIdentifierList[TUTOR_NUM_MESSAGES] =
{
	"YOU_FIRED_A_SHOT",
	"YOU_SHOULD_RELOAD",
	"YOU_ARE_OUT_OF_AMMO",
	"YOU_KILLED_A_TEAMMATE",
	"YOU_KILLED_PLAYER",
	"YOU_KILLED_PLAYER_ONE_LEFT",
	"YOU_KILLED_LAST_ENEMY",
	"YOU_KILLED_PLAYER_HEADSHOT",
	"YOU_KILLED_PLAYER_HEADSHOT_ONE_LEFT",
	"YOU_KILLED_LAST_ENEMY_HEADSHOT",
	"YOU_DIED",
	"YOU_DIED_HEADSHOT",
	"YOU_FELL_TO_YOUR_DEATH",
	"YOU_WERE_JUST_HURT",
	"YOU_ARE_BLIND_FROM_FLASHBANG",
	"YOU_ATTACKED_TEAMMATE",
	"BUY_TIME_BEGIN",
	"BOMB_PLANTED_T",
	"BOMB_PLANTED_CT",
	"TEAMMATE_KILLED",
	"TEAMMATE_KILLED_ONE_LEFT",
	"LAST_TEAMMATE_KILLED",
	"ENEMY_KILLED",
	"ENEMY_KILLED_ONE_LEFT",
	"LAST_ENEMY_KILLED",
	"YOU_SPAWNED",
	"YOU_SEE_FRIEND",
	"YOU_SEE_ENEMY",
	"YOU_SEE_FRIEND_CORPSE",
	"YOU_SEE_ENEMY_CORPSE",
	"YOU_SEE_LOOSE_BOMB_T",
	"YOU_SEE_LOOSE_BOMB_CT",
	"YOU_SEE_BOMB_CARRIER_T",
	"YOU_SEE_BOMB_CARRIER_CT",
	"YOU_SEE_PLANTED_BOMB_T",
	"YOU_SEE_PLANTED_BOMB_CT",
	"YOU_ARE_BOMB_CARRIER",
	"YOU_SEE_LOOSE_WEAPON",
	"YOU_SEE_LOOSE_DEFUSER",
	"YOU_SEE_BOMBSITE_T",
	"YOU_SEE_BOMBSITE_CT",
	"YOU_SEE_BOMBSITE_T_BOMB",
	"YOU_SEE_HOSTAGE_T",
	"YOU_SEE_HOSTAGE_CT",
	"YOU_SEE_HOSTAGE_CT_EXAMINE",
	"YOU_USED_HOSTAGE_MORE_LEFT",
	"YOU_USED_HOSTAGE_NO_MORE_LEFT",
	"ALL_HOSTAGES_FOLLOWING_T",
	"ALL_HOSTAGES_FOLLOWING_CT",
	"HOSTAGE_RESCUED_T",
	"HOSTAGE_RESCUED_CT",
	"YOU_RESCUED_HOSTAGE",
	"YOU_ARE_IN_BOMBSITE_T",
	"YOU_ARE_IN_BOMBSITE_CT",
	"YOU_ARE_IN_BOMBSITE_T_BOMB",
	"ALL_HOSTAGES_RESCUED_T",
	"ALL_HOSTAGES_RESCUED_CT",
	"YOU_DAMAGED_HOSTAGE",
	"YOU_KILLED_HOSTAGE",
	"ALL_HOSTAGES_DEAD",
	"YOU_HAVE_BEEN_SHOT_AT",
	"TIME_RUNNING_OUT_DE_T",
	"TIME_RUNNING_OUT_DE_CT",
	"TIME_RUNNING_OUT_CS_T",
	"TIME_RUNNING_OUT_CS_CT",
	"DEFUSING_WITHOUT_KIT",
	"BOMB_DEFUSED_T",
	"BOMB_DEFUSED_CT",
	"YOU_DEFUSED_BOMB",
	"BOMB_EXPLODED_T",
	"BOMB_EXPLODED_CT",
	"ROUND_START_DE_T",
	"ROUND_START_DE_CT",
	"ROUND_START_CS_T",
	"ROUND_START_CS_CT",
	"ROUND_OVER",
	"ROUND_DRAW",
	"CT_WIN",
	"T_WIN",
	"DEATH_CAMERA_START",
	"RADIO_COVER_ME",
	"RADIO_YOU_TAKE_THE_POINT",
	"RADIO_HOLD_THIS_POSITION",
	"RADIO_REGROUP_TEAM",
	"RADIO_FOLLOW_ME",
	"RADIO_TAKING_FIRE",
	"RADIO_GO_GO_GO",
	"RADIO_TEAM_FALL_BACK",
	"RADIO_STICK_TOGETHER_TEAM",
	"RADIO_GET_IN_POSITION_AND_WAIT",
	"RADIO_STORM_THE_FRONT",
	"RADIO_REPORT_IN_TEAM",
	"RADIO_AFFIRMATIVE",
	"RADIO_ENEMY_SPOTTED",
	"RADIO_NEED_BACKUP",
	"RADIO_SECTOR_CLEAR",
	"RADIO_IN_POSITION",
	"RADIO_REPORTING_IN",
	"RADIO_GET_OUT_OF_THERE",
	"RADIO_NEGATIVE",
	"RADIO_ENEMY_DOWN",
	"BUY_NEED_PRIMARY",
	"BUY_NEED_PRIMARY_AMMO",
	"BUY_NEED_SECONDARY_AMMO",
	"BUY_NEED_ARMOR",
	"BUY_NEED_DEFUSE_KIT",
	"BUY_NEED_GRENADE",
	"CAREER_TASK_DONE_MORE_LEFT",
	"CAREER_TASK_DONE_ONE_LEFT",
	"CAREER_TASK_DONE_ALL_DONE",
	"HINT_BEGIN",
	"HINT_1",
	"HINT_2",
	"HINT_3",
	"HINT_4",
	"HINT_5",
	"HINT_10",
	"HINT_11",
	"HINT_12",
	"HINT_13",
	"HINT_14",
	"HINT_15",
	"HINT_20",
	"HINT_21",
	"HINT_22",
	"HINT_23",
	"HINT_24",
	"HINT_25",
	"HINT_26",
	"HINT_30",
	"HINT_31",
	"HINT_32",
	"HINT_33",
	"HINT_34",
	"HINT_40",
	"HINT_50",
	"HINT_51",
	"HINT_52",
	"HINT_53",
	"HINT_60",
	"HINT_61",
	"HINT_70",
	"HINT_71",
	"HINT_72",
	"HINT_73",
	"HINT_END",
	"INGAME_HINT_BEGIN",
	"INGAME_HINT_1",
	"INGAME_HINT_2",
	"INGAME_HINT_END"
};

CCSTutor::CCSTutor()
{
	m_stateSystem = new CCSTutorStateSystem;
	m_nextViewableCheckTime = 0;
	m_currentlyShownMessageID = TUTOR_NUM_MESSAGES;
	m_currentMessageEvent = NULL;
	m_messageTypeMask = TUTORMESSAGETYPE_ALL;
	m_lastScenarioEvent = NULL;
	m_haveSpawned = false;
	m_lastHintShown = HINT_BEGIN;
	m_lastInGameHintShown = INGAME_HINT_BEGIN;
	ReadTutorMessageFile();
	ApplyPersistentDecay();
	ResetPlayerDeathInfo();
}

CCSTutor::~CCSTutor()
{
	if ( m_stateSystem != NULL )
	{
		delete m_stateSystem;
		m_stateSystem = NULL;
	}
	if ( m_currentMessageEvent != NULL )
	{
		DeleteEvent( m_currentMessageEvent );
		m_currentMessageEvent = NULL;
	}
	if ( m_lastScenarioEvent != NULL )
	{
		DeleteEvent( m_lastScenarioEvent );
		m_lastScenarioEvent = NULL;
	}
	ClearCurrentEvent( true, true );
	ClearEventList();
}

TutorMessage* CCSTutor::GetTutorMessageDefinition( int id )
{
	if ( (unsigned int)id >= TUTOR_NUM_MESSAGES )
		return NULL;
	TutorMessageMap::iterator it = m_messageMap.find( TutorIdentifierList[id] );
	return it != m_messageMap.end() ? it->second : NULL;
}

void CCSTutor::ApplyPersistentDecay()
{
	for ( int i = 0; i < TUTOR_NUM_MESSAGES; i++ )
	{
		TutorMessage* definition = GetTutorMessageDefinition( i );
		if ( definition != NULL )
		{
			int timesShown = g_engfuncs.pfnGetTimesTutorMessageShown( i );
			if ( timesShown != -1 )
				definition->m_timesShown = timesShown;
		}
	}
}

void CCSTutor::ResetPlayerDeathInfo()
{
	for ( int i = 0; i < ARRAYSIZE( m_playerDeathInfo ); i++ )
	{
		m_playerDeathInfo[i].m_hasBeenShown = false;
		m_playerDeathInfo[i].m_event = NULL;
	}
}

void CCSTutor::PurgeMessages()
{
	ClearCurrentEvent( true, true );
	ClearEventList();
	if ( m_lastScenarioEvent != NULL )
	{
		DeleteEvent( m_lastScenarioEvent );
		m_lastScenarioEvent = NULL;
	}
}

void CCSTutor::AddToEventList( TutorMessageEvent* event )
{
	if ( event != NULL )
	{
		event->SetNext( m_eventList );
		m_eventList = event;
	}
}

void CCSTutor::DeleteEventFromEventList( TutorMessageEvent* event )
{
	if ( event == NULL )
		return;
	if ( m_eventList == event )
	{
		m_eventList = event->GetNext();
		return;
	}
	for ( TutorMessageEvent* previous = m_eventList; previous != NULL; previous = previous->GetNext() )
	{
		if ( previous->GetNext() == event )
		{
			previous->SetNext( event->GetNext() );
			return;
		}
	}
}

void CCSTutor::ClearEventList()
{
	while ( m_eventList != NULL )
	{
		TutorMessageEvent* event = m_eventList;
		m_eventList = event->GetNext();
		DeleteEvent( event );
	}
}

void CCSTutor::DeleteEvent( TutorMessageEvent* event )
{
	for ( int i = 0; i < ARRAYSIZE( m_playerDeathInfo ); i++ )
	{
		if ( m_playerDeathInfo[i].m_event == event )
			m_playerDeathInfo[i].m_event = NULL;
	}
	delete event;
}

void CCSTutor::ProcessShownDeathsForEvent( TutorMessageEvent* event )
{
	if ( event == NULL )
		return;
	for ( int i = 0; i < ARRAYSIZE( m_playerDeathInfo ); i++ )
	{
		if ( m_playerDeathInfo[i].m_event == event )
		{
			m_playerDeathInfo[i].m_hasBeenShown = true;
			m_playerDeathInfo[i].m_event = NULL;
		}
	}
}

void CCSTutor::TransferDeathEvents( TutorMessageEvent* oldEvent, TutorMessageEvent* newEvent )
{
	for ( int i = 0; i < ARRAYSIZE( m_playerDeathInfo ); i++ )
	{
		if ( m_playerDeathInfo[i].m_event == oldEvent )
			m_playerDeathInfo[i].m_event = newEvent;
	}
}

void CCSTutor::ClearCurrentEvent( bool closeWindow, bool processDeaths )
{
	TutorMessage* definition = GetTutorMessageDefinition( m_currentlyShownMessageID );
	if ( definition != NULL )
		definition->m_lastCloseTime = gpGlobals->time;
	if ( m_currentMessageEvent != NULL && processDeaths )
		ProcessShownDeathsForEvent( m_currentMessageEvent );
	if ( closeWindow )
		CloseCurrentWindow();
	m_currentlyShownMessageID = TUTOR_NUM_MESSAGES;
	m_currentlyShownMessageCloseTime = 0;
	m_currentlyShownMessageMinimumCloseTime = 0;
	if ( m_currentMessageEvent != NULL )
	{
		DeleteEvent( m_currentMessageEvent );
		m_currentMessageEvent = NULL;
	}
}

TutorMessageEvent* CCSTutor::GetTutorMessageUpdateEvent()
{
	TutorMessage* definition = GetTutorMessageDefinition( m_currentlyShownMessageID );
	if ( definition == NULL || definition->m_keepOld == TUTORMESSAGEKEEPOLDTYPE_DONT_KEEP_OLD )
		return NULL;
	for ( TutorMessageEvent* event = m_eventList; event != NULL; event = event->GetNext() )
	{
		if ( DoMessagesHaveSameID( event->GetID(), m_currentlyShownMessageID ) )
			return event;
	}
	return NULL;
}

bool CCSTutor::GetDuplicateMessagesFromEventList( TutorMessageEvent*& event1, TutorMessageEvent*& event2 )
{
	for ( event1 = m_eventList; event1 != NULL; event1 = event1->GetNext() )
	{
		for ( event2 = event1->GetNext(); event2 != NULL; event2 = event2->GetNext() )
		{
			if ( DoMessagesHaveSameID( event1->GetID(), event2->GetID() ) )
				return true;
		}
	}
	return false;
}

void CCSTutor::CancelEvent( TutorMessageID id )
{
	if ( m_currentlyShownMessageID == id )
		ClearCurrentEvent( true, true );
	TutorMessageEvent* event = m_eventList;
	while ( event != NULL )
	{
		TutorMessageEvent* next = event->GetNext();
		if ( event->GetID() == id )
		{
			DeleteEventFromEventList( event );
			DeleteEvent( event );
		}
		event = next;
	}
	if ( m_lastScenarioEvent != NULL && m_lastScenarioEvent->GetID() == id )
	{
		DeleteEvent( m_lastScenarioEvent );
		m_lastScenarioEvent = NULL;
	}
}

void CCSTutor::CheckForInactiveEvents( float time )
{
	TutorMessageEvent* event = m_eventList;
	while ( event != NULL )
	{
		TutorMessageEvent* next = event->GetNext();
		if ( !event->IsActive( time ) )
		{
			DeleteEventFromEventList( event );
			DeleteEvent( event );
		}
		event = next;
	}
	TutorMessageEvent* event1;
	TutorMessageEvent* event2;
	while ( GetDuplicateMessagesFromEventList( event1, event2 ) )
	{
		float timeActive = event1->GetTimeActive( time );
		if ( !(event2->GetTimeActive( time ) <= timeActive) )
		{
			TransferDeathEvents( event2, event1 );
			DeleteEventFromEventList( event2 );
			DeleteEvent( event2 );
		}
		else
		{
			TransferDeathEvents( event1, event2 );
			DeleteEventFromEventList( event1 );
			DeleteEvent( event1 );
		}
	}
}

bool CCSTutor::ShouldShowMessageEvent( TutorMessageEvent* event, float time )
{
	if ( event == NULL )
		return false;
	TutorMessage* definition = GetTutorMessageDefinition( event->GetID() );
	if ( definition == NULL )
		return false;
	if ( definition->m_class == TUTORMESSAGECLASS_NORMAL && definition->m_decay != 0 &&
		definition->m_timesShown >= definition->m_decay )
		return false;
	if ( !(definition->m_type & m_messageTypeMask) || definition->m_minRepeatInterval > time - definition->m_lastCloseTime )
		return false;
	if ( (unsigned int)m_currentlyShownMessageID < TUTOR_NUM_MESSAGES )
	{
		if ( definition->m_interruptFlag != TUTORMESSAGEINTERRUPTFLAG_NOW_DAMMIT &&
			time <= m_currentlyShownMessageMinimumCloseTime )
			return false;
		TutorMessage* current = GetTutorMessageDefinition( m_currentlyShownMessageID );
		if ( DoMessagesHaveSameID( event->GetID(), m_currentlyShownMessageID ) &&
			current->m_keepOld != TUTORMESSAGEKEEPOLDTYPE_DONT_KEEP_OLD )
			return false;
		if ( definition->m_priority <= current->m_priority )
		{
			if ( definition->m_priority != current->m_priority )
				return false;
			float timeActive = event->GetTimeActive( time );
			return timeActive > m_currentMessageEvent->GetTimeActive( time );
		}
	}
	return true;
}

bool CCSTutor::ShouldUpdateCurrentMessage( TutorMessageID id )
{
	return DoMessagesHaveSameID( id, m_currentlyShownMessageID ) &&
		GetTutorMessageDefinition( id )->m_keepOld != TUTORMESSAGEKEEPOLDTYPE_DONT_KEEP_OLD;
}

void CCSTutor::UpdateCurrentMessage( TutorMessageEvent* event )
{
	TransferDeathEvents( m_currentMessageEvent, event );
	if ( m_currentMessageEvent != NULL )
	{
		DeleteEvent( m_currentMessageEvent );
		m_currentMessageEvent = NULL;
	}
	m_currentMessageEvent = event;
	m_currentlyShownMessageID = (TutorMessageID)event->GetID();
	TutorMessage* definition = GetTutorMessageDefinition( event->GetID() );
	if ( definition != NULL )
	{
		CBasePlayer* player = UTIL_GetLocalPlayer();
		if ( player != NULL )
		{
			m_currentlyShownMessageCloseTime = definition->m_duration + gpGlobals->time;
			if ( definition->m_keepOld == TUTORMESSAGEKEEPOLDTYPE_UPDATE_CONTENT )
			{
				CloseCurrentWindow();
				DisplayMessageToPlayer( player, event->GetID(), definition->m_text, event );
			}
		}
	}
}

void CCSTutor::CheckForWindowClose( float time )
{
	if ( (unsigned int)m_currentlyShownMessageID < TUTOR_NUM_MESSAGES && time > m_currentlyShownMessageCloseTime )
	{
		TutorMessageEvent* event = GetTutorMessageUpdateEvent();
		if ( event != NULL )
		{
			UpdateCurrentMessage( event );
			DeleteEventFromEventList( event );
		}
		else
		{
			ClearCurrentEvent( true, true );
		}
	}
}

void CCSTutor::CheckForContentUpdate()
{
	if ( (unsigned int)m_currentlyShownMessageID < TUTOR_NUM_MESSAGES )
	{
		TutorMessage* definition = GetTutorMessageDefinition( m_currentlyShownMessageID );
		if ( definition != NULL && definition->m_keepOld == TUTORMESSAGEKEEPOLDTYPE_UPDATE_CONTENT )
		{
			TutorMessageEvent* event = GetTutorMessageUpdateEvent();
			if ( event != NULL )
			{
				UpdateCurrentMessage( event );
				DeleteEventFromEventList( event );
			}
		}
	}
}

void CCSTutor::ComputeDisplayTimesForMessage()
{
	float now = gpGlobals->time;
	TutorMessage* definition = GetTutorMessageDefinition( m_currentlyShownMessageID );
	if ( definition != NULL )
	{
		m_currentlyShownMessageMinimumCloseTime = cv_tutor_message_minimum_display_time.value;
		m_currentlyShownMessageCloseTime = definition->m_duration + now;
		float characterTime = g_engfuncs.pfnGetLocalizedStringLength( definition->m_text ) *
			cv_tutor_message_character_display_time_coefficient.value;
		if ( characterTime > m_currentlyShownMessageMinimumCloseTime )
			m_currentlyShownMessageMinimumCloseTime = characterTime;
		float minimumTime = definition->m_minDisplayTimeOverride;
		if ( minimumTime <= m_currentlyShownMessageMinimumCloseTime )
			minimumTime = m_currentlyShownMessageMinimumCloseTime;
		m_currentlyShownMessageMinimumCloseTime = minimumTime + now;
		if ( m_currentlyShownMessageMinimumCloseTime > m_currentlyShownMessageCloseTime )
			m_currentlyShownMessageCloseTime = m_currentlyShownMessageMinimumCloseTime;
	}
	else
	{
		m_currentlyShownMessageCloseTime = now;
	}
}

void CCSTutor::ConstructMessageAndDisplay()
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL || player->IsBot() == TRUE )
		return;
	int id = m_currentMessageEvent->GetID();
	if ( (unsigned int)id < TUTOR_NUM_MESSAGES )
	{
		TutorMessage* definition = GetTutorMessageDefinition( id );
		if ( definition != NULL )
		{
			definition->m_timesShown++;
			ComputeDisplayTimesForMessage();
			DisplayMessageToPlayer( player, id, definition->m_text, m_currentMessageEvent );
		}
	}
}

void CCSTutor::CheckForInterruptingMessageEvent( float time )
{
	TutorMessage* oldMessage = GetTutorMessageDefinition( m_currentlyShownMessageID );
	TutorMessageEvent* oldEvent = m_currentMessageEvent;
	bool newEvent = false;
	for ( TutorMessageEvent* event = m_eventList; event != NULL; event = event->GetNext() )
	{
		if ( ShouldShowMessageEvent( event, time ) )
		{
			ShowTutorMessage( event );
			newEvent = true;
		}
	}
	if ( newEvent )
	{
		if ( oldEvent != NULL )
		{
			ProcessShownDeathsForEvent( oldEvent );
			DeleteEvent( oldEvent );
		}
		CloseCurrentWindow();
		if ( oldMessage != NULL )
			oldMessage->m_lastCloseTime = time;
		DeleteEventFromEventList( m_currentMessageEvent );
		ConstructMessageAndDisplay();
	}
}

void CCSTutor::ShowTutorMessage( TutorMessageEvent* event )
{
	int id = event->GetID();
	if ( (unsigned int)id < TUTOR_NUM_MESSAGES && GetTutorMessageDefinition( id ) != NULL )
	{
		m_currentlyShownMessageID = (TutorMessageID)id;
		m_currentMessageEvent = event;
		m_currentlyShownMessageCloseTime = 0;
		m_currentlyShownMessageMinimumCloseTime = 0;
	}
}

bool CCSTutor::HasCurrentWindowBeenActiveLongEnough( float time )
{
	if ( (unsigned int)m_currentlyShownMessageID < TUTOR_NUM_MESSAGES )
		return time > m_currentlyShownMessageMinimumCloseTime;
	return true;
}

void CCSTutor::CreateAndAddEventToList( TutorMessageID id, CBaseEntity* entity, CBaseEntity* other )
{
	TutorMessageEvent* event = CreateTutorMessageEvent( id, entity, other );
	if ( event == NULL )
		return;
	TutorMessage* definition = GetTutorMessageDefinition( id );
	if ( definition != NULL && definition->m_type == TUTORMESSAGETYPE_SCENARIO )
	{
		if ( m_lastScenarioEvent != NULL )
		{
			DeleteEvent( m_lastScenarioEvent );
			m_lastScenarioEvent = NULL;
		}
		m_lastScenarioEvent = CreateTutorMessageEvent( id, entity, other );
	}
	AddToEventList( event );
}

TutorMessageEvent* CCSTutor::CreateTutorMessageEvent( TutorMessageID id, CBaseEntity*, CBaseEntity* )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return NULL;
	TutorMessage* definition = GetTutorMessageDefinition( id );
	if ( definition == NULL )
		return NULL;
	TutorMessageEvent* event = new TutorMessageEvent( id, definition->m_duplicateID, gpGlobals->time,
		(float)definition->m_lifetime, definition->m_priority );
	if ( event == NULL )
		return NULL;
	char numTStr[16];
	char numLeftStr[2048];
	int numT;
	int numCT;
	switch ( id )
	{
	case YOU_KILLED_PLAYER:
	case YOU_KILLED_PLAYER_HEADSHOT:
	case ENEMY_KILLED:
		GetNumPlayersAliveOnTeams( numT, numCT );
		snprintf( numTStr, sizeof( numTStr ), "%i", numT );
		snprintf( numLeftStr, 16, "%i", numCT );
		if ( player->m_iTeam == CT )
			event->AddParameter( numTStr );
		else if ( player->m_iTeam == TERRORIST )
			event->AddParameter( numLeftStr );
		[[fallthrough]];
	case YOU_KILLED_PLAYER_ONE_LEFT:
	case YOU_KILLED_LAST_ENEMY:
	case YOU_KILLED_PLAYER_HEADSHOT_ONE_LEFT:
	case YOU_KILLED_LAST_ENEMY_HEADSHOT:
	case ENEMY_KILLED_ONE_LEFT:
	case LAST_ENEMY_KILLED:
		if ( player->m_iTeam == CT )
			ConstructRecentDeathsList( TERRORIST, numLeftStr, sizeof( numLeftStr ), event );
		else if ( player->m_iTeam == TERRORIST )
			ConstructRecentDeathsList( CT, numLeftStr, sizeof( numLeftStr ), event );
		else
			return event;
		event->AddParameter( numLeftStr );
		break;
	case TEAMMATE_KILLED:
		GetNumPlayersAliveOnTeams( numT, numCT );
		if ( player->IsAlive() )
		{
			if ( player->m_iTeam == TERRORIST )
				numT--;
			else if ( player->m_iTeam == CT )
				numCT--;
		}
		snprintf( numTStr, sizeof( numTStr ), "%i", numT );
		snprintf( numLeftStr, 16, "%i", numCT );
		if ( player->m_iTeam == CT )
			event->AddParameter( numLeftStr );
		else if ( player->m_iTeam == TERRORIST )
			event->AddParameter( numTStr );
		[[fallthrough]];
	case YOU_KILLED_A_TEAMMATE:
	case TEAMMATE_KILLED_ONE_LEFT:
	case LAST_TEAMMATE_KILLED:
		ConstructRecentDeathsList( player->m_iTeam, numLeftStr, sizeof( numLeftStr ), event );
		event->AddParameter( numLeftStr );
		break;
	case CAREER_TASK_DONE_MORE_LEFT:
		sprintf( numLeftStr, "%d", TheCareerTasks != NULL ? TheCareerTasks->GetNumRemainingTasks() : 0 );
		event->AddParameter( numLeftStr );
		break;
	default:
		break;
	}
	return event;
}

void CCSTutor::GetNumPlayersAliveOnTeams( int& terrorists, int& cts )
{
	terrorists = 0;
	cts = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* player = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( player != NULL && player->IsAlive() )
		{
			if ( player->m_iTeam == CT )
				cts++;
			else if ( player->m_iTeam == TERRORIST )
				terrorists++;
		}
	}
}

void CCSTutor::ConstructRecentDeathsList( TeamName team, char* buffer, int buflen, TutorMessageEvent* event )
{
	if ( buffer == NULL || buflen == 0 )
		return;
	buffer[0] = '\0';
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* player = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( player != NULL && player->m_iTeam == team && !player->IsAlive() && !m_playerDeathInfo[i - 1].m_hasBeenShown )
		{
			strcat( buffer, "  " );
			strcat( buffer, "%n" );
			char scratch[32];
			sprintf( scratch, "%d", i );
			strcat( buffer, scratch );
			strcat( buffer, "\n" );
			m_playerDeathInfo[i - 1].m_event = event;
		}
	}
	for ( int i = strlen( buffer ) - 1; i >= 0 && buffer[i] == '\n'; i-- )
		buffer[i] = '\0';
}

void CCSTutor::CheckHintMessages( float time )
{
	if ( !(m_deadAirStartTime > 0 && time - m_deadAirStartTime > cv_tutor_hint_interval_time.value) )
		return;
	m_lastHintShown = (TutorMessageID)(m_lastHintShown + 1);
	for ( ;; )
	{
		while ( UTIL_FindEntityByClassname( NULL, "hostage_entity" ) != NULL )
		{
			if ( m_lastHintShown < HINT_BOMB_START || m_lastHintShown > HINT_BOMB_END )
				goto hintSelected;
			m_lastHintShown = (TutorMessageID)(m_lastHintShown + 1);
		}
		if ( !((CHalfLifeMultiplay*)g_pGameRules)->m_bMapHasBombTarget ||
			m_lastHintShown < HINT_HOSTAGE_START || m_lastHintShown > HINT_HOSTAGE_END )
			break;
		m_lastHintShown = (TutorMessageID)(m_lastHintShown + 1);
		if ( m_lastHintShown == HINT_END )
			m_lastHintShown = HINT_1;
	}
hintSelected:
	if ( m_lastHintShown < HINT_1 || m_lastHintShown >= HINT_END )
		m_lastHintShown = HINT_1;
	if ( GetTutorMessageDefinition( m_lastHintShown ) != NULL )
		CreateAndAddEventToList( m_lastHintShown, NULL, NULL );
}

void CCSTutor::CheckInGameHintMessages( float time )
{
	if ( m_deadAirStartTime > 0 && time - m_deadAirStartTime > cv_tutor_hint_interval_time.value )
	{
		m_lastInGameHintShown = (TutorMessageID)(m_lastInGameHintShown + 1);
		if ( m_lastInGameHintShown > INGAME_HINT_BEGIN && m_lastInGameHintShown < INGAME_HINT_END &&
			GetTutorMessageDefinition( m_lastInGameHintShown ) != NULL )
			CreateAndAddEventToList( m_lastInGameHintShown, NULL, NULL );
	}
}

void CCSTutor::LookupHotKey( TutorMessageID, int, wchar_t*, int )
{
}

void CCSTutor::TutorThink( float time )
{
	if ( time >= m_nextViewableCheckTime )
	{
		CheckForBombViewable();
		CheckForLooseWeaponViewable();
		CheckForLooseDefuserViewable();
		CheckForTimeRunningOut();
		CheckForBombsiteViewable();
		CheckForHostageViewable();
		CheckExamineMessages( time );
		CheckHintMessages( time );
		CheckInGameHintMessages( time );
		CheckForNeedToReload( true );
		if ( m_haveSpawned )
		{
			CBasePlayer* player = UTIL_GetLocalPlayer();
			if ( player != NULL && player->CanPlayerBuy( false ) )
			{
				m_messageTypeMask = TUTORMESSAGETYPE_BUY;
				CreateAndAddEventToList( BUY_TIME_BEGIN, NULL, NULL );
				m_haveSpawned = false;
			}
		}
		CBasePlayer* player = UTIL_GetLocalPlayer();
		if ( player != NULL && player->CanPlayerBuy( false ) && m_messageTypeMask == TUTORMESSAGETYPE_BUY )
		{
			CheckBuyZoneMessages();
		}
		else
		{
			player = UTIL_GetLocalPlayer();
			if ( (player == NULL || !player->CanPlayerBuy( false )) && m_messageTypeMask == TUTORMESSAGETYPE_BUY )
				OnEvent( EVENT_PLAYER_LEFT_BUY_ZONE, NULL, NULL );
		}
		m_nextViewableCheckTime = time + cv_tutor_viewable_check_interval.value;
	}
	CheckForInactiveEvents( time );
	CheckForWindowClose( time );
	CheckForContentUpdate();
	CheckForInterruptingMessageEvent( time );
}

bool CCSTutor::IsHostageMap()
{
	return UTIL_FindEntityByClassname( NULL, "hostage_entity" ) != NULL;
}

bool CCSTutor::IsBombMap()
{
	return ((CHalfLifeMultiplay*)g_pGameRules)->m_bMapHasBombTarget;
}

void CCSTutor::HandleRadioCoverMe( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_COVER_ME, entity, other );
}

void CCSTutor::HandleRadioYouTakeThePoint( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_YOU_TAKE_THE_POINT, entity, other );
}

void CCSTutor::HandleRadioHoldThisPosition( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_HOLD_THIS_POSITION, entity, other );
}

void CCSTutor::HandleRadioRegroupTeam( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_REGROUP_TEAM, entity, other );
}

void CCSTutor::HandleRadioFollowMe( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_FOLLOW_ME, entity, other );
}

void CCSTutor::HandleRadioTakingFire( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_TAKING_FIRE, entity, other );
}

void CCSTutor::HandleRadioGoGoGo( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_GO_GO_GO, entity, other );
}

void CCSTutor::HandleRadioTeamFallBack( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_TEAM_FALL_BACK, entity, other );
}

void CCSTutor::HandleRadioStickTogetherTeam( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_STICK_TOGETHER_TEAM, entity, other );
}

void CCSTutor::HandleRadioGetInPositionAndWait( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_GET_IN_POSITION_AND_WAIT, entity, other );
}

void CCSTutor::HandleRadioStormTheFront( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_STORM_THE_FRONT, entity, other );
}

void CCSTutor::HandleRadioReportInTeam( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_REPORT_IN_TEAM, entity, other );
}

void CCSTutor::HandleRadioAffirmative( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_AFFIRMATIVE, entity, other );
}

void CCSTutor::HandleRadioEnemySpotted( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_ENEMY_SPOTTED, entity, other );
}

void CCSTutor::HandleRadioNeedBackup( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_NEED_BACKUP, entity, other );
}

void CCSTutor::HandleRadioSectorClear( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_SECTOR_CLEAR, entity, other );
}

void CCSTutor::HandleRadioInPosition( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_IN_POSITION, entity, other );
}

void CCSTutor::HandleRadioReportingIn( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_REPORTING_IN, entity, other );
}

void CCSTutor::HandleRadioGetOutOfThere( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_GET_OUT_OF_THERE, entity, other );
}

void CCSTutor::HandleRadioNegative( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_NEGATIVE, entity, other );
}

void CCSTutor::HandleRadioEnemyDown( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( RADIO_ENEMY_DOWN, entity, other );
}

void CCSTutor::HandleNotBuyingAnything( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( BUY_TIME_BEGIN, entity, other );
}

void CCSTutor::HandleNeedToBuyPrimaryWeapon( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( BUY_NEED_PRIMARY, entity, other );
}

void CCSTutor::HandleNeedToBuyPrimaryAmmo( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( BUY_NEED_PRIMARY_AMMO, entity, other );
}

void CCSTutor::HandleNeedToBuySecondaryAmmo( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( BUY_NEED_SECONDARY_AMMO, entity, other );
}

void CCSTutor::HandleNeedToBuyArmor( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( BUY_NEED_ARMOR, entity, other );
}

void CCSTutor::HandleNeedToBuyDefuseKit( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( BUY_NEED_DEFUSE_KIT, entity, other );
}

void CCSTutor::HandleNeedToBuyGrenade( CBaseEntity* entity, CBaseEntity* other )
{
	CreateAndAddEventToList( BUY_NEED_GRENADE, entity, other );
}

static void ParseMessageParameters( char*& messageData, TutorMessage* message )
{
	messageData = MP_COM_Parse( messageData );
	const char* token = MP_COM_GetToken();
	while ( messageData != NULL )
	{
		if ( stricmp( token, "End" ) == 0 )
			break;
		if ( stricmp( token, "String" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			message->m_text = strdup( MP_COM_GetToken() );
		}
		else if ( stricmp( token, "Duration" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			message->m_duration = (unsigned char)atoi( MP_COM_GetToken() );
		}
		else if ( stricmp( token, "Priority" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			message->m_priority = (unsigned char)atoi( MP_COM_GetToken() );
		}
		else if ( stricmp( token, "KeepOld" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			const char* value = MP_COM_GetToken();
			if ( stricmp( value, "true" ) == 0 )
				message->m_keepOld = TUTORMESSAGEKEEPOLDTYPE_KEEP_OLD;
			else if ( stricmp( value, "updatecontent" ) == 0 )
				message->m_keepOld = TUTORMESSAGEKEEPOLDTYPE_UPDATE_CONTENT;
			else
				message->m_keepOld = TUTORMESSAGEKEEPOLDTYPE_DONT_KEEP_OLD;
		}
		else if ( stricmp( token, "Class" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			message->m_class = stricmp( MP_COM_GetToken(), "Examine" ) == 0 ? TUTORMESSAGECLASS_EXAMINE : TUTORMESSAGECLASS_NORMAL;
		}
		else if ( stricmp( token, "Decay" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			message->m_decay = (unsigned char)atoi( MP_COM_GetToken() );
		}
		else if ( stricmp( token, "Type" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			const char* value = MP_COM_GetToken();
			if ( stricmp( value, "FriendDeath" ) == 0 )
				message->m_type = TUTORMESSAGETYPE_FRIEND_DEATH;
			else if ( stricmp( value, "EnemyDeath" ) == 0 )
				message->m_type = TUTORMESSAGETYPE_ENEMY_DEATH;
			else if ( stricmp( value, "Scenario" ) == 0 )
				message->m_type = TUTORMESSAGETYPE_SCENARIO;
			else if ( stricmp( value, "Buy" ) == 0 )
				message->m_type = TUTORMESSAGETYPE_BUY;
			else if ( stricmp( value, "Career" ) == 0 )
				message->m_type = TUTORMESSAGETYPE_CAREER;
			else if ( stricmp( value, "Hint" ) == 0 )
				message->m_type = TUTORMESSAGETYPE_HINT;
			else if ( stricmp( value, "InGameHint" ) == 0 )
				message->m_type = TUTORMESSAGETYPE_INGAME_HINT;
			else if ( stricmp( value, "EndGame" ) == 0 )
				message->m_type = TUTORMESSAGETYPE_END_GAME;
			else
				message->m_type = TUTORMESSAGETYPE_DEFAULT;
		}
		else if ( stricmp( token, "Lifetime" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			message->m_lifetime = atoi( MP_COM_GetToken() );
		}
		else if ( stricmp( token, "DuplicateID" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			message->m_duplicateID = atoi( MP_COM_GetToken() );
		}
		else if ( stricmp( token, "Interrupt" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			message->m_interruptFlag = stricmp( MP_COM_GetToken(), "Now" ) == 0 ?
				TUTORMESSAGEINTERRUPTFLAG_NOW_DAMMIT : TUTORMESSAGEINTERRUPTFLAG_DEFAULT;
		}
		else if ( stricmp( token, "MinDisplayTimeOverride" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			message->m_minDisplayTimeOverride = (float)atof( MP_COM_GetToken() );
		}
		else if ( stricmp( token, "MinRepeatInterval" ) == 0 )
		{
			messageData = MP_COM_Parse( messageData );
			message->m_minRepeatInterval = (float)atof( MP_COM_GetToken() );
		}
		messageData = MP_COM_Parse( messageData );
		token = MP_COM_GetToken();
	}
}

void CCSTutor::ReadTutorMessageFile()
{
	int messageFileLen = 0;
	char* messageFile = (char*)LOAD_FILE_FOR_ME( "tutordata.txt", &messageFileLen );
	char* messageData = messageFile;
	if ( messageFile != NULL )
	{
		TutorMessage defaults;
		defaults.m_duplicateID = 0;
		defaults.m_keepOld = TUTORMESSAGEKEEPOLDTYPE_DONT_KEEP_OLD;
		defaults.m_duration = 1;
		defaults.m_priority = 0;
		defaults.m_class = TUTORMESSAGECLASS_NORMAL;
		defaults.m_type = TUTORMESSAGETYPE_DEFAULT;
		defaults.m_decay = 10;
		defaults.m_lifetime = 10;
		defaults.m_interruptFlag = TUTORMESSAGEINTERRUPTFLAG_DEFAULT;
		defaults.m_minDisplayTimeOverride = 0;
		defaults.m_minRepeatInterval = 0;
		for ( ;; )
		{
			messageData = MP_COM_Parse( messageData );
			if ( messageData == NULL )
				break;
			const char* token = MP_COM_GetToken();
			if ( stricmp( token, "TutorMessage" ) == 0 )
			{
				messageData = MP_COM_Parse( messageData );
				std::string identifier = MP_COM_GetToken();
				TutorMessage* message = new TutorMessage;
				message->m_text = NULL;
				message->m_examineStartTime = -1.0f;
				message->m_timesShown = 0;
				message->m_duplicateID = defaults.m_duplicateID;
				message->m_minDisplayTimeOverride = defaults.m_minDisplayTimeOverride;
				message->m_lastCloseTime = 0;
				message->m_keepOld = defaults.m_keepOld;
				message->m_duration = defaults.m_duration;
				message->m_priority = defaults.m_priority;
				message->m_class = defaults.m_class;
				message->m_type = defaults.m_type;
				message->m_decay = defaults.m_decay;
				message->m_lifetime = defaults.m_lifetime;
				message->m_interruptFlag = defaults.m_interruptFlag;
				ParseMessageParameters( messageData, message );
				m_messageMap[identifier] = message;
			}
			else if ( stricmp( token, "Defaults" ) == 0 )
			{
				ParseMessageParameters( messageData, &defaults );
			}
		}
		FREE_FILE( messageFile );
	}
	else if ( UTIL_IsGame( "czero" ) )
	{
		CONSOLE_ECHO( "Warning: Cannot access tutor message file tutordata.txt\n" );
	}
}

void CCSTutor::HandleTWin( CBaseEntity*, CBaseEntity* )
{
	CreateAndAddEventToList( T_WIN, NULL, NULL );
	ResetPlayerDeathInfo();
}

void CCSTutor::HandleCTWin( CBaseEntity*, CBaseEntity* )
{
	CreateAndAddEventToList( CT_WIN, NULL, NULL );
	ResetPlayerDeathInfo();
}

void CCSTutor::HandleRoundDraw( CBaseEntity*, CBaseEntity* )
{
	if ( ((CHalfLifeMultiplay*)g_pGameRules)->m_iTotalRoundsPlayed != 0 )
		CreateAndAddEventToList( ROUND_DRAW, NULL, NULL );
	ResetPlayerDeathInfo();
}

void CCSTutor::HandleAutoBuy( CBaseEntity*, CBaseEntity* )
{
	if ( m_currentlyShownMessageID == BUY_TIME_BEGIN )
		ClearCurrentEvent( true, true );
}

void CCSTutor::HandleBuyMenuOpenned( CBaseEntity*, CBaseEntity* )
{
	if ( m_currentlyShownMessageID == BUY_TIME_BEGIN )
	{
		ClearCurrentEvent( true, true );
		CheckBuyZoneMessages();
	}
}

void CCSTutor::HandleBuyTimeStart( CBaseEntity*, CBaseEntity* )
{
}

void CCSTutor::HandleBeingShotAt( CBaseEntity* entity, CBaseEntity* other )
{
	if ( entity != NULL && entity->IsPlayer() )
	{
		CBasePlayer* player = UTIL_GetLocalPlayer();
		if ( player != NULL && entity == player && player->IsAlive() )
			CreateAndAddEventToList( YOU_HAVE_BEEN_SHOT_AT, entity, other );
	}
}

void CCSTutor::HandleWeaponFired( CBaseEntity* entity, CBaseEntity* )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player != NULL && player->IsAlive() && entity == player )
		CheckForNeedToReload( false );
}

void CCSTutor::HandleWeaponReloaded( CBaseEntity* entity, CBaseEntity* )
{
	if ( entity != NULL && entity->IsPlayer() && entity == UTIL_GetLocalPlayer() )
		CancelEvent( YOU_SHOULD_RELOAD );
}

void CCSTutor::HandleBombDefusing( CBaseEntity* entity, CBaseEntity* )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player != NULL && entity != NULL && entity->IsPlayer() && player == entity && !player->m_bHasDefuser )
		CreateAndAddEventToList( DEFUSING_WITHOUT_KIT, NULL, NULL );
}

void CCSTutor::HandleBombExploded( CBaseEntity*, CBaseEntity* )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player != NULL )
	{
		if ( player->m_iTeam == CT )
			CreateAndAddEventToList( BOMB_EXPLODED_CT, NULL, NULL );
		else if ( player->m_iTeam == TERRORIST )
			CreateAndAddEventToList( BOMB_EXPLODED_T, NULL, NULL );
	}
}

void CCSTutor::HandleAllHostagesRescued( CBaseEntity*, CBaseEntity* )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player != NULL )
	{
		if ( player->m_iTeam == CT )
			CreateAndAddEventToList( ALL_HOSTAGES_RESCUED_CT, NULL, NULL );
		else if ( player->m_iTeam == TERRORIST )
			CreateAndAddEventToList( ALL_HOSTAGES_RESCUED_T, NULL, NULL );
	}
}

void CCSTutor::HandlePlayerSpawned( CBaseEntity* entity, CBaseEntity* other )
{
	if ( !entity->IsPlayer() )
		return;
	if ( entity == UTIL_GetLocalPlayer() )
	{
		m_haveSpawned = true;
		m_lastInGameHintShown = INGAME_HINT_BEGIN;
		CreateAndAddEventToList( YOU_SPAWNED, entity, other );
	}
}

void CCSTutor::HandleHostageDamaged( CBaseEntity* entity, CBaseEntity* other )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player != NULL && entity != NULL && other != NULL && other->IsPlayer() && player == other )
		CreateAndAddEventToList( YOU_DAMAGED_HOSTAGE, NULL, NULL );
}

void CCSTutor::HandleDeathCameraStart( CBaseEntity* entity, CBaseEntity* )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player != NULL && entity != NULL && entity->IsPlayer() && player == entity )
	{
		m_messageTypeMask = TUTORMESSAGETYPE_END_GAME | TUTORMESSAGETYPE_HINT |
			TUTORMESSAGETYPE_ENEMY_DEATH | TUTORMESSAGETYPE_FRIEND_DEATH;
		CreateAndAddEventToList( DEATH_CAMERA_START, NULL, NULL );
	}
}

void CCSTutor::HandlePlayerBlindedByFlashbang( CBaseEntity* entity, CBaseEntity* )
{
	if ( entity != NULL && entity->IsPlayer() )
	{
		CBasePlayer* player = UTIL_GetLocalPlayer();
		if ( player != NULL && entity == player )
			CreateAndAddEventToList( YOU_ARE_BLIND_FROM_FLASHBANG, NULL, NULL );
	}
}

void CCSTutor::HandlePlayerLeftBuyZone( CBaseEntity*, CBaseEntity* )
{
	m_messageTypeMask = TUTORMESSAGETYPE_ALL & ~(TUTORMESSAGETYPE_BUY | TUTORMESSAGETYPE_HINT);
	ClearEventList();
	ClearCurrentEvent( true, true );
	if ( m_lastScenarioEvent != NULL )
	{
		m_lastScenarioEvent->SetActivationTime( gpGlobals->time );
		AddToEventList( m_lastScenarioEvent );
		m_lastScenarioEvent = NULL;
	}
}

void CCSTutor::HandleClientCorpseSpawned( CBaseEntity* entity, CBaseEntity* )
{
	if ( entity != NULL && entity->IsPlayer() )
	{
		ClientCorpseStruct* corpse = new ClientCorpseStruct;
		corpse->m_position = entity->pev->origin;
		corpse->m_team = ((CBasePlayer*)entity)->m_iTeam;
		m_clientCorpseList.push_back( corpse );
	}
}

bool CCSTutor::CanLocalPlayerBuyStuff()
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	return player != NULL && player->CanPlayerBuy( false );
}

void CCSTutor::CheckForAllHostagesDead()
{
	bool alive = false;
	CBaseEntity* hostage = UTIL_FindEntityByClassname( NULL, "hostage_entity" );
	while ( hostage != NULL )
	{
		alive = hostage->IsAlive() != FALSE;
		hostage = UTIL_FindEntityByClassname( hostage, "hostage_entity" );
		if ( alive )
			break;
	}
	if ( !alive )
		CreateAndAddEventToList( ALL_HOSTAGES_DEAD, NULL, NULL );
}

void CCSTutor::CheckForLooseWeaponViewable()
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;
	TutorMessage* definition = GetTutorMessageDefinition( YOU_SEE_LOOSE_WEAPON );
	if ( definition != NULL && definition->m_class == TUTORMESSAGECLASS_EXAMINE )
		return;
	bool found = false;
	CBaseEntity* weapon = NULL;
	while ( (weapon = UTIL_FindEntityByClassname( weapon, "weaponbox" )) != NULL )
	{
		if ( IsEntityInViewOfPlayer( weapon, player ) && !found )
		{
			found = true;
			CreateAndAddEventToList( YOU_SEE_LOOSE_WEAPON, NULL, NULL );
		}
	}
}

void CCSTutor::CheckForLooseDefuserViewable()
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL || player->m_iTeam != CT )
		return;
	TutorMessage* definition = GetTutorMessageDefinition( YOU_SEE_LOOSE_DEFUSER );
	if ( definition != NULL && definition->m_class == TUTORMESSAGECLASS_EXAMINE )
		return;
	bool found = false;
	CBaseEntity* defuser = NULL;
	while ( (defuser = UTIL_FindEntityByClassname( defuser, "item_thighpack" )) != NULL )
	{
		if ( IsEntityInViewOfPlayer( defuser, player ) && !found )
		{
			found = true;
			CreateAndAddEventToList( YOU_SEE_LOOSE_DEFUSER, NULL, NULL );
		}
	}
}

bool CCSTutor::IsBombPlantedInBombsite( CBaseEntity* bombsite )
{
	CGrenade* bomb = NULL;
	while ( (bomb = (CGrenade*)UTIL_FindEntityByClassname( bomb, "grenade" )) != NULL )
	{
		if ( bomb->m_bIsC4 )
			return IsEntityInBombsite( bombsite, bomb );
	}
	return false;
}

void CCSTutor::HandleShotFired( Vector source, Vector target )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player != NULL )
	{
		float distance = (source - player->pev->origin).Length();
		if ( distance > 32.0f )
		{
			float angle = (float)acos( (double)(distance / (target - source).Length()) );
			float nearDistance = (float)(sin( (double)angle ) * distance);
			if ( nearDistance <= 300.0f )
				OnEvent( EVENT_BEING_SHOT_AT, player, NULL );
		}
	}
}

void CCSTutor::HandleRoundStart( CBaseEntity* entity, CBaseEntity* other )
{
	m_roundStartTime = gpGlobals->time;
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;
	if ( IsBombMap() )
	{
		if ( player->m_iTeam == CT )
			CreateAndAddEventToList( ROUND_START_DE_CT, NULL, NULL );
		else if ( player->m_iTeam == TERRORIST )
		{
			if ( player->m_bHasC4 )
				CreateAndAddEventToList( YOU_ARE_BOMB_CARRIER, entity, other );
			else
				CreateAndAddEventToList( ROUND_START_DE_T, NULL, NULL );
		}
	}
	else if ( IsHostageMap() )
	{
		if ( player->m_iTeam == CT )
			CreateAndAddEventToList( ROUND_START_CS_CT, NULL, NULL );
		else if ( player->m_iTeam == TERRORIST )
			CreateAndAddEventToList( ROUND_START_CS_T, NULL, NULL );
	}
}

void CCSTutor::HandleBombPlanted( CBaseEntity* entity, CBaseEntity* other )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;
	if ( player->IsAlive() && player->m_iTeam == CT )
		CreateAndAddEventToList( BOMB_PLANTED_CT, entity, other );
	else
		CreateAndAddEventToList( BOMB_PLANTED_T, entity, other );
}

void CCSTutor::HandleBombDefused( CBaseEntity* entity, CBaseEntity* )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;
	if ( entity != NULL && entity->IsPlayer() && player == entity )
		CreateAndAddEventToList( YOU_DEFUSED_BOMB, NULL, NULL );
	else if ( player->m_iTeam == CT )
		CreateAndAddEventToList( BOMB_DEFUSED_CT, NULL, NULL );
	else if ( player->m_iTeam == TERRORIST )
		CreateAndAddEventToList( BOMB_DEFUSED_T, NULL, NULL );
	else
		CreateAndAddEventToList( ROUND_OVER, NULL, NULL );
}

void CCSTutor::HandleHostageUsed( CBaseEntity* entity, CBaseEntity* )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL || entity == NULL || !entity->IsPlayer() )
		return;
	if ( CheckForAllHostagesFollowingSomeone() )
	{
		if ( player == entity )
			CreateAndAddEventToList( YOU_USED_HOSTAGE_NO_MORE_LEFT, NULL, NULL );
		else if ( player->m_iTeam == CT )
			CreateAndAddEventToList( ALL_HOSTAGES_FOLLOWING_CT, NULL, NULL );
		else if ( player->m_iTeam == TERRORIST )
			CreateAndAddEventToList( ALL_HOSTAGES_FOLLOWING_T, NULL, NULL );
	}
	else if ( player == entity )
	{
		CreateAndAddEventToList( YOU_USED_HOSTAGE_MORE_LEFT, NULL, NULL );
	}
}

void CCSTutor::HandleHostageKilled( CBaseEntity* entity, CBaseEntity* other )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;
	CheckForAllHostagesDead();
	if ( entity != NULL && other != NULL && other->IsPlayer() )
	{
		if ( player == other )
			CreateAndAddEventToList( YOU_KILLED_HOSTAGE, NULL, NULL );
		if ( CheckForAllHostagesFollowingSomeone() )
		{
			if ( player->m_iTeam == CT )
				CreateAndAddEventToList( ALL_HOSTAGES_FOLLOWING_CT, NULL, NULL );
			else if ( player->m_iTeam == TERRORIST )
				CreateAndAddEventToList( ALL_HOSTAGES_FOLLOWING_T, NULL, NULL );
		}
	}
}

void CCSTutor::HandleHostageRescued( CBaseEntity* entity, CBaseEntity* )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL || entity == NULL || !entity->IsPlayer() )
		return;
	if ( player->m_iTeam == TERRORIST )
		CreateAndAddEventToList( HOSTAGE_RESCUED_T, NULL, NULL );
	else if ( player->m_iTeam == CT )
	{
		if ( player == entity )
			CreateAndAddEventToList( YOU_RESCUED_HOSTAGE, NULL, NULL );
		else
			CreateAndAddEventToList( HOSTAGE_RESCUED_CT, NULL, NULL );
	}
}

void CCSTutor::HandleCareerTaskDone( CBaseEntity* entity, CBaseEntity* other )
{
	int remaining = TheCareerTasks != NULL ? TheCareerTasks->GetNumRemainingTasks() : 0;
	if ( remaining > 0 )
	{
		if ( remaining == 1 )
			CreateAndAddEventToList( CAREER_TASK_DONE_ONE_LEFT, entity, other );
		else
			CreateAndAddEventToList( CAREER_TASK_DONE_MORE_LEFT, entity, other );
	}
	else
	{
		CreateAndAddEventToList( CAREER_TASK_DONE_ALL_DONE, entity, other );
	}
}

void CCSTutor::HandlePlayerTookDamage( CBaseEntity* entity, CBaseEntity* other )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;
	CBasePlayer* victim = entity != NULL && entity->IsPlayer() == TRUE ? (CBasePlayer*)entity : NULL;
	CBasePlayer* attacker = other != NULL && other->IsPlayer() == TRUE ? (CBasePlayer*)other : NULL;
	if ( victim == player )
		CreateAndAddEventToList( YOU_WERE_JUST_HURT, NULL, NULL );
	else if ( attacker == player && victim != NULL && player->m_iTeam == victim->m_iTeam )
		CreateAndAddEventToList( YOU_ATTACKED_TEAMMATE, NULL, NULL );
}

void CCSTutor::HandleWeaponFiredOnEmpty( CBaseEntity* entity, CBaseEntity* )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player != NULL && entity != NULL && entity->IsPlayer() && player == entity &&
		player->m_pActiveItem->IsWeapon() == TRUE )
	{
		CBasePlayerWeapon* weapon = (CBasePlayerWeapon*)player->m_pActiveItem;
		if ( player->AmmoInventory( weapon->m_iPrimaryAmmoType ) <= 0 )
		{
			TutorMessage* definition = GetTutorMessageDefinition( YOU_ARE_OUT_OF_AMMO );
			if ( definition != NULL )
				definition->m_lastCloseTime = 0;
			CreateAndAddEventToList( YOU_ARE_OUT_OF_AMMO, NULL, NULL );
		}
	}
}

void CCSTutor::HandlePlayerDied( CBaseEntity* entity, CBaseEntity* other )
{
	CBasePlayer* player = UTIL_GetLocalPlayer();
	if ( player == NULL )
		return;
	CBasePlayer* victim = entity->IsPlayer() ? (CBasePlayer*)entity : NULL;
	CBasePlayer* attacker = other->IsPlayer() ? (CBasePlayer*)other : NULL;
	if ( victim == player && attacker == NULL )
	{
		if ( player->m_bKilledByBomb )
			CreateAndAddEventToList( YOU_DIED, entity, other );
		else
			CreateAndAddEventToList( YOU_FELL_TO_YOUR_DEATH, NULL, NULL );
		return;
	}
	if ( victim == NULL || attacker == NULL )
		return;
	if ( victim == attacker && victim == player )
	{
		CreateAndAddEventToList( YOU_DIED, entity, other );
		return;
	}
	int numT;
	int numCT;
	GetNumPlayersAliveOnTeams( numT, numCT );
	if ( player == attacker )
	{
		if ( victim->m_iTeam == player->m_iTeam )
		{
			CreateAndAddEventToList( YOU_KILLED_A_TEAMMATE, entity, other );
			return;
		}
		if ( player->m_iTeam != CT && player->m_iTeam != TERRORIST )
			return;
		int enemies = player->m_iTeam == CT ? numT : numCT;
		TutorMessageID id;
		if ( victim->m_bHeadshotKilled )
		{
			if ( enemies == 0 )
				id = YOU_KILLED_LAST_ENEMY_HEADSHOT;
			else if ( enemies == 1 )
				id = YOU_KILLED_PLAYER_HEADSHOT_ONE_LEFT;
			else
				id = YOU_KILLED_PLAYER_HEADSHOT;
		}
		else
		{
			if ( enemies == 0 )
				id = YOU_KILLED_LAST_ENEMY;
			else if ( enemies == 1 )
				id = YOU_KILLED_PLAYER_ONE_LEFT;
			else
				id = YOU_KILLED_PLAYER;
		}
		CreateAndAddEventToList( id, entity, other );
		return;
	}
	if ( victim == player )
	{
		CreateAndAddEventToList( victim->m_bHeadshotKilled ? YOU_DIED_HEADSHOT : YOU_DIED, entity, other );
		return;
	}
	if ( player->m_iTeam != CT && player->m_iTeam != TERRORIST )
		return;
	if ( player->m_iTeam == victim->m_iTeam )
	{
		int friends = player->m_iTeam == CT ? numCT : numT;
		if ( player->IsAlive() )
		{
			if ( friends == 1 )
				CreateAndAddEventToList( LAST_TEAMMATE_KILLED, entity, other );
			else if ( friends == 2 )
				CreateAndAddEventToList( TEAMMATE_KILLED_ONE_LEFT, entity, other );
			else
				CreateAndAddEventToList( TEAMMATE_KILLED, entity, other );
		}
		else if ( friends == 1 )
		{
			CreateAndAddEventToList( TEAMMATE_KILLED_ONE_LEFT, entity, other );
		}
		else if ( friends > 1 )
		{
			CreateAndAddEventToList( TEAMMATE_KILLED, entity, other );
		}
	}
	else
	{
		int enemies = player->m_iTeam == CT ? numT : numCT;
		if ( enemies == 0 )
			CreateAndAddEventToList( LAST_ENEMY_KILLED, entity, other );
		else if ( enemies == 1 )
			CreateAndAddEventToList( ENEMY_KILLED_ONE_LEFT, entity, other );
		else
			CreateAndAddEventToList( ENEMY_KILLED, entity, other );
	}
}

void CCSTutor::CallEventHandler( GameEventType event, CBaseEntity* entity, CBaseEntity* other )
{
	switch ( event )
	{
	case EVENT_WEAPON_FIRED:
		HandleWeaponFired( entity, other );
		break;
	case EVENT_WEAPON_FIRED_ON_EMPTY:
		HandleWeaponFiredOnEmpty( entity, other );
		break;
	case EVENT_WEAPON_RELOADED:
		HandleWeaponReloaded( entity, other );
		break;
	case EVENT_BEING_SHOT_AT:
		HandleBeingShotAt( entity, other );
		break;
	case EVENT_PLAYER_BLINDED_BY_FLASHBANG:
		HandlePlayerBlindedByFlashbang( entity, other );
		break;
	case EVENT_PLAYER_DIED:
		HandlePlayerDied( entity, other );
		break;
	case EVENT_PLAYER_TOOK_DAMAGE:
		HandlePlayerTookDamage( entity, other );
		break;
	case EVENT_HOSTAGE_DAMAGED:
		HandleHostageDamaged( entity, other );
		break;
	case EVENT_HOSTAGE_KILLED:
		HandleHostageKilled( entity, other );
		break;
	case EVENT_BOMB_PLANTED:
		HandleBombPlanted( entity, other );
		break;
	case EVENT_BOMB_DEFUSING:
		HandleBombDefusing( entity, other );
		break;
	case EVENT_BOMB_DEFUSED:
		HandleBombDefused( entity, other );
		break;
	case EVENT_BOMB_EXPLODED:
		HandleBombExploded( entity, other );
		break;
	case EVENT_HOSTAGE_USED:
		HandleHostageUsed( entity, other );
		break;
	case EVENT_HOSTAGE_RESCUED:
		HandleHostageRescued( entity, other );
		break;
	case EVENT_ALL_HOSTAGES_RESCUED:
		HandleAllHostagesRescued( entity, other );
		break;
	case EVENT_TERRORISTS_WIN:
		HandleTWin( entity, other );
		break;
	case EVENT_CTS_WIN:
		HandleCTWin( entity, other );
		break;
	case EVENT_ROUND_DRAW:
		HandleRoundDraw( entity, other );
		break;
	case EVENT_ROUND_START:
		HandleRoundStart( entity, other );
		break;
	case EVENT_PLAYER_SPAWNED:
		if ( entity->IsPlayer() == TRUE && entity == UTIL_GetLocalPlayer() )
		{
			m_haveSpawned = true;
			m_lastInGameHintShown = INGAME_HINT_BEGIN;
			CreateAndAddEventToList( YOU_SPAWNED, entity, other );
		}
		break;
	case EVENT_PLAYER_LEFT_BUY_ZONE:
		HandlePlayerLeftBuyZone( entity, other );
		break;
	case EVENT_DEATH_CAMERA_START:
		HandleDeathCameraStart( entity, other );
		break;
	case EVENT_TUTOR_BUY_MENU_OPENNED:
		HandleBuyMenuOpenned( entity, other );
		break;
	case EVENT_TUTOR_AUTOBUY:
		HandleAutoBuy( entity, other );
		break;
	case EVENT_TUTOR_NOT_BUYING_ANYTHING:
		HandleNotBuyingAnything( entity, other );
		break;
	case EVENT_TUTOR_NEED_TO_BUY_PRIMARY_WEAPON:
		HandleNeedToBuyPrimaryWeapon( entity, other );
		break;
	case EVENT_TUTOR_NEED_TO_BUY_PRIMARY_AMMO:
		HandleNeedToBuyPrimaryAmmo( entity, other );
		break;
	case EVENT_TUTOR_NEED_TO_BUY_SECONDARY_AMMO:
		HandleNeedToBuySecondaryAmmo( entity, other );
		break;
	case EVENT_TUTOR_NEED_TO_BUY_ARMOR:
		HandleNeedToBuyArmor( entity, other );
		break;
	case EVENT_TUTOR_NEED_TO_BUY_DEFUSE_KIT:
		HandleNeedToBuyDefuseKit( entity, other );
		break;
	case EVENT_TUTOR_NEED_TO_BUY_GRENADE:
		HandleNeedToBuyGrenade( entity, other );
		break;
	case EVENT_CAREER_TASK_DONE:
		HandleCareerTaskDone( entity, other );
		break;
	case EVENT_RADIO_COVER_ME:
		HandleRadioCoverMe( entity, other );
		break;
	case EVENT_RADIO_YOU_TAKE_THE_POINT:
		HandleRadioYouTakeThePoint( entity, other );
		break;
	case EVENT_RADIO_HOLD_THIS_POSITION:
		HandleRadioHoldThisPosition( entity, other );
		break;
	case EVENT_RADIO_REGROUP_TEAM:
		HandleRadioRegroupTeam( entity, other );
		break;
	case EVENT_RADIO_FOLLOW_ME:
		HandleRadioFollowMe( entity, other );
		break;
	case EVENT_RADIO_TAKING_FIRE:
		HandleRadioTakingFire( entity, other );
		break;
	case EVENT_RADIO_GO_GO_GO:
		HandleRadioGoGoGo( entity, other );
		break;
	case EVENT_RADIO_TEAM_FALL_BACK:
		HandleRadioTeamFallBack( entity, other );
		break;
	case EVENT_RADIO_STICK_TOGETHER_TEAM:
		HandleRadioStickTogetherTeam( entity, other );
		break;
	case EVENT_RADIO_GET_IN_POSITION_AND_WAIT:
		HandleRadioGetInPositionAndWait( entity, other );
		break;
	case EVENT_RADIO_STORM_THE_FRONT:
		HandleRadioStormTheFront( entity, other );
		break;
	case EVENT_RADIO_REPORT_IN_TEAM:
		HandleRadioReportInTeam( entity, other );
		break;
	case EVENT_RADIO_AFFIRMATIVE:
		HandleRadioAffirmative( entity, other );
		break;
	case EVENT_RADIO_ENEMY_SPOTTED:
		HandleRadioEnemySpotted( entity, other );
		break;
	case EVENT_RADIO_NEED_BACKUP:
		HandleRadioNeedBackup( entity, other );
		break;
	case EVENT_RADIO_SECTOR_CLEAR:
		HandleRadioSectorClear( entity, other );
		break;
	case EVENT_RADIO_IN_POSITION:
		HandleRadioInPosition( entity, other );
		break;
	case EVENT_RADIO_REPORTING_IN:
		HandleRadioReportingIn( entity, other );
		break;
	case EVENT_RADIO_GET_OUT_OF_THERE:
		HandleRadioGetOutOfThere( entity, other );
		break;
	case EVENT_RADIO_NEGATIVE:
		HandleRadioNegative( entity, other );
		break;
	case EVENT_RADIO_ENEMY_DOWN:
		HandleRadioEnemyDown( entity, other );
		break;
	default:
		break;
	}
}
