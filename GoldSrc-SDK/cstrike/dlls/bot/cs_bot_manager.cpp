#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "gamerules.h"
#include "game.h"
#include "client.h"
#include "debug.h"
#include "weapontype.h"
#include "cs_bot_manager.h"
#include "cs_bot_chatter.h"
#include "cs_bot.h"
#include "buildslave_assert.h"

#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\dlls\\bot\\cs_bot_manager.cpp" )

float CCSBotManager::m_flNextCVarCheck = 0.0f;
NavEditCmdType CCSBotManager::m_editCmd = EDIT_NONE;
bool CCSBotManager::m_isMapDataLoaded = false;
bool CCSBotManager::m_isLearningMap = false;
bool CCSBotManager::m_isAnalysisRequested = false;

void UTIL_DrawBox( Extent* extent, int lifetime, int red, int green, int blue )
{
	const Vector corner[8] =
	{
		Vector( extent->lo.x, extent->lo.y, extent->lo.z ),
		Vector( extent->hi.x, extent->lo.y, extent->lo.z ),
		Vector( extent->hi.x, extent->hi.y, extent->lo.z ),
		Vector( extent->lo.x, extent->hi.y, extent->lo.z ),
		Vector( extent->lo.x, extent->lo.y, extent->hi.z ),
		Vector( extent->hi.x, extent->lo.y, extent->hi.z ),
		Vector( extent->hi.x, extent->hi.y, extent->hi.z ),
		Vector( extent->lo.x, extent->hi.y, extent->hi.z )
	};
	static const int edge[] = { 1, 2, 3, 4, -1, 5, 6, 7, 8, -5, 1, -5, 2, -6, 3, -7, 4, -8, 0 };
	int previous = edge[0];
	bool move = false;
	for ( int i = 1; edge[i] != 0; ++i )
	{
		int next = edge[i];
		if ( !move )
		{
			if ( next < 0 )
			{
				next = -next;
				move = true;
			}
			UTIL_DrawBeamPoints( corner[previous - 1], corner[next - 1], lifetime,
				static_cast<byte>( red ), static_cast<byte>( green ), static_cast<byte>( blue ) );
			UTIL_DrawBeamPoints( corner[next - 1], corner[previous - 1], lifetime,
				static_cast<byte>( red ), static_cast<byte>( green ), static_cast<byte>( blue ) );
		}
		else
		{
			move = false;
		}
		previous = next;
	}
}

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CCSBotManager::CCSBotManager()
{
	m_zoneCount = 0;
	m_flNextCVarCheck = 0.0f;
	m_looseBomb = NULL;
	m_looseBombArea = NULL;
	m_isBombPlanted = false;
	m_bombDefuser = NULL;
	m_navPlace = 0;
	m_roundStartTimestamp = 0.0f;
	m_bServerActive = false;
	m_isLearningMap = false;
	m_isAnalysisRequested = false;
	m_editCmd = EDIT_NONE;

	TheBotPhrases = new BotPhraseManager;
	TheBotPhrases->Initialize( "BotChatter.db", 0 );
	TheBotProfiles = new BotProfileManager;
	TheBotProfiles->FindVoiceBankIndex( "BotChatter.db" );

	int length;
	const char* packList = g_engfuncs.pfnIsCareerMatch() ? "MissionPacks/BotPackList.db" : "BotPackList.db";
	char* data = reinterpret_cast<char*>( LOAD_FILE_FOR_ME( const_cast<char*>( packList ), &length ) );
	if ( data != NULL )
	{
		for ( const char* cursor = SharedParse( data ); cursor != NULL; cursor = SharedParse( cursor ) )
		{
			char* filename = CloneString( SharedGetToken() );
			TheBotProfiles->Init( filename );
			delete[] filename;
		}
		FREE_FILE( data );
	}
	else
	{
		TheBotProfiles->Init( "BotProfile.db" );
	}

	const BotProfileManager::VoiceBankList* banks = TheBotProfiles->GetVoiceBanks();
	for ( unsigned int bank = 1; bank < banks->size(); ++bank )
		TheBotPhrases->Initialize( (*banks)[bank], bank );
}

CCSBotManager::~CCSBotManager()
{
	DestroyNavigationMap();
	delete TheBotPhrases;
	TheBotPhrases = NULL;
	delete TheBotProfiles;
	TheBotProfiles = NULL;
}

void CCSBotManager::StartFrame()
{
	CBotManager::StartFrame();
	MonitorBotCVars();
	if ( cv_bot_debug.value == 5.0f )
	{
		for ( int i = 0; i < m_zoneCount; ++i )
			UTIL_DrawBox( &m_zone[i].m_extent, 1, 255, 100, 0 );
	}
}

void CCSBotManager::RestartRound()
{
	CBotManager::RestartRound();
	m_looseBomb = NULL;
	m_looseBombArea = NULL;
	m_isBombPlanted = false;
	m_earliestBombPlantTimestamp = gpGlobals->time + RANDOM_FLOAT( 10.0f, 30.0f );
	m_bombDefuser = NULL;
	m_editCmd = EDIT_NONE;
	ResetRadioMessageTimestamps();
	m_lastSeenEnemyTimestamp = -9999.9f;
	m_roundStartTimestamp = gpGlobals->time + CVAR_GET_FLOAT( "mp_freezetime" );
	m_isDefenseRushing = RANDOM_FLOAT( 0.0f, 100.0f ) <= 33.3f;
	TheBotPhrases->OnRoundRestart();
	m_isRoundOver = false;
	m_isRespawnStarted = false;
	m_canRespawn = true;
}

void CCSBotManager::ServerActivate()
{
	DestroyNavigationMap();
	m_isMapDataLoaded = false;
	m_zoneCount = 0;
	m_scenario = SCENARIO_DEATHMATCH;
	ValidateMapData();
	RestartRound();
	m_isLearningMap = false;
	m_isAnalysisRequested = false;
	m_bServerActive = true;
	AddServerCommands();
	TheBotPhrases->OnMapChange();
}

void CCSBotManager::ServerDeactivate()
{
	m_bServerActive = false;
}

void CCSBotManager::ValidateMapData()
{
	if ( m_isMapDataLoaded || !UTIL_IsGame( "czero" ) )
		return;
	m_isMapDataLoaded = true;
	if ( LoadNavigationMap() != NAV_OK )
	{
		CONSOLE_ECHO( "Failed to load navigation map.\n" );
		return;
	}
	CONSOLE_ECHO( "Navigation map loaded.\n" );
	m_zoneCount = 0;
	m_scenario = SCENARIO_DEATHMATCH;
	if ( gpGlobals->maxEntities <= 1 )
		return;

	for ( int i = 1; i < gpGlobals->maxEntities; ++i )
	{
		CBaseEntity* entity = CBaseEntity::Instance( INDEXENT( i ) );
		if ( entity == NULL )
			continue;

		const char* classname = STRING( entity->pev->classname );
		bool isZone = false;
		bool legacy = false;
		if ( FStrEq( classname, "func_bomb_target" ) )
		{
			m_scenario = SCENARIO_DEFUSE_BOMB;
			isZone = true;
		}
		else if ( FStrEq( classname, "info_bomb_target" ) )
		{
			m_scenario = SCENARIO_DEFUSE_BOMB;
			isZone = true;
			legacy = true;
		}
		else if ( FStrEq( classname, "func_hostage_rescue" ) )
		{
			m_scenario = SCENARIO_RESCUE_HOSTAGES;
			isZone = true;
		}
		else if ( FStrEq( classname, "info_hostage_rescue" ) )
		{
			m_scenario = SCENARIO_RESCUE_HOSTAGES;
			isZone = true;
			legacy = true;
		}
		else if ( FStrEq( classname, "hostage_entity" ) )
		{
			m_scenario = SCENARIO_RESCUE_HOSTAGES;
		}
		else if ( FStrEq( classname, "func_vip_safetyzone" ) )
		{
			m_scenario = SCENARIO_ESCORT_VIP;
			isZone = true;
		}
		if ( !isZone )
			continue;
		if ( m_zoneCount >= ARRAYSIZE( m_zone ) )
		{
			CONSOLE_ECHO( "Warning: Too many zones, some will be ignored.\n" );
			continue;
		}
		Zone& zone = m_zone[m_zoneCount];
		zone.m_entity = entity;
		zone.m_index = m_zoneCount++;
		zone.m_isLegacy = legacy;
		zone.m_center = legacy ? entity->pev->origin : 0.5f * (entity->pev->absmin + entity->pev->absmax);
	}

	if ( m_zoneCount == 0 && m_scenario == SCENARIO_RESCUE_HOSTAGES )
	{
		CBaseEntity* spawn = NULL;
		while ( (spawn = UTIL_FindEntityByClassname( spawn, "info_player_start" )) != NULL && !FNullEnt( spawn->pev ) )
		{
			if ( m_zoneCount >= ARRAYSIZE( m_zone ) )
			{
				CONSOLE_ECHO( "Warning: Too many zones, some will be ignored.\n" );
				continue;
			}
			Zone& zone = m_zone[m_zoneCount];
			zone.m_entity = spawn;
			zone.m_index = m_zoneCount++;
			zone.m_isLegacy = true;
			zone.m_center = spawn->pev->origin;
		}
	}

	const Vector legacyRadius( 256.0f, 256.0f, 256.0f );
	for ( int i = 0; i < m_zoneCount; ++i )
	{
		Zone& zone = m_zone[i];
		if ( zone.m_isLegacy )
		{
			zone.m_extent.lo = zone.m_center - legacyRadius;
			zone.m_extent.hi = zone.m_center + legacyRadius;
		}
		else
		{
			zone.m_extent.lo = zone.m_entity->pev->absmin;
			zone.m_extent.hi = zone.m_entity->pev->absmax;
		}
		zone.m_areaCount = 0;
		zone.m_extent.lo.z -= 50.0f;
		zone.m_extent.hi.z += 50.0f;
		for ( NavAreaList::const_iterator iter = TheNavAreaList.begin(); iter != TheNavAreaList.end(); ++iter )
		{
			const Extent* extent = (*iter)->GetExtent();
			if ( extent->hi.x >= zone.m_extent.lo.x && zone.m_extent.hi.x >= extent->lo.x &&
				extent->hi.y >= zone.m_extent.lo.y && zone.m_extent.hi.y >= extent->lo.y &&
				extent->hi.z >= zone.m_extent.lo.z && zone.m_extent.hi.z >= extent->lo.z )
			{
				zone.m_area[zone.m_areaCount++] = *iter;
				if ( zone.m_areaCount == ARRAYSIZE( zone.m_area ) )
					break;
			}
		}
	}
}

void CCSBotManager::MonitorBotCVars()
{
	if ( cv_bot_nav_edit.value != 0.0f )
	{
		EditNavAreas( m_editCmd );
		m_editCmd = EDIT_NONE;
	}
	if ( m_flNextCVarCheck <= gpGlobals->time )
	{
		if ( cv_bot_show_danger.value != 0.0f )
			DrawDanger();
		MaintainBotQuota();
		m_flNextCVarCheck = gpGlobals->time + 0.3f;
	}
}

void CCSBotManager::MaintainBotQuota()
{
	if ( m_isLearningMap )
		return;
	int humans = UTIL_HumansInGame();
	int activeHumans = UTIL_HumansInGame( true );
	if ( humans == 0 && !IS_DEDICATED_SERVER() )
		return;

	int quota = static_cast<int>( cv_bot_quota_match.value > 0.0f ? cv_bot_quota_match.value * activeHumans : cv_bot_quota.value );
	if ( cv_bot_join_after_player.value > 0.0f && activeHumans == 0 )
		quota = 0;
	int available = gpGlobals->maxClients - humans;
	if ( cv_bot_auto_vacate.value > 0.0f )
		--available;
	quota = min( quota, available );
	CHalfLifeMultiplay* rules = static_cast<CHalfLifeMultiplay*>( g_pGameRules );
	if ( UTIL_BotsInGame() < quota )
	{
		if ( !rules->TeamFull( TERRORIST ) || !rules->TeamFull( CT ) )
			BotAddCommand( BOT_TEAM_ANY, false );
		return;
	}
	if ( UTIL_BotsInGame() == quota )
	{
		if ( !rules->IsCareer() )
			return;
		if ( FStrEq( humans_join_team.string, "CT" ) )
		{
			if ( rules->m_iNumCT <= 6 )
				return;
			UTIL_KickBotFromTeam( CT );
		}
		else
		{
			if ( rules->m_iNumTerrorist <= 6 )
				return;
			UTIL_KickBotFromTeam( TERRORIST );
		}
		CVAR_SET_FLOAT( "bot_quota", cv_bot_quota.value - 1.0f );
		return;
	}
	if ( UTIL_KickBotFromTeam( UNASSIGNED ) )
		return;
	if ( rules->m_iNumTerrorist > rules->m_iNumCT ||
		(rules->m_iNumTerrorist >= rules->m_iNumCT &&
		(rules->m_iNumTerroristWins > rules->m_iNumCTWins ||
		(rules->m_iNumTerroristWins >= rules->m_iNumCTWins && RANDOM_LONG( 0, 1 ) != 0))) )
	{
		if ( !UTIL_KickBotFromTeam( TERRORIST ) )
			UTIL_KickBotFromTeam( CT );
	}
	else if ( !UTIL_KickBotFromTeam( CT ) )
	{
		UTIL_KickBotFromTeam( TERRORIST );
	}
}

BOOL CCSBotManager::ClientCommand( CBasePlayer* player, const char* )
{
	if ( player != NULL && UTIL_GetLocalPlayer() != NULL )
		UTIL_GetLocalPlayer();
	return FALSE;
}

void CCSBotManager::AddServerCommand( const char* command )
{
	g_engfuncs.pfnAddServerCommand( const_cast<char*>( command ), Bot_ServerCommand );
}

void CCSBotManager::AddServerCommands()
{
	static bool firstTime = true;
	if ( !firstTime )
		return;
	firstTime = false;
	if ( !UTIL_IsGame( "czero" ) )
		return;

	AddServerCommand( "bot_about" );
	AddServerCommand( "bot_add" );
	AddServerCommand( "bot_add_t" );
	AddServerCommand( "bot_add_ct" );
	AddServerCommand( "bot_kill" );
	AddServerCommand( "bot_kick" );
	AddServerCommand( "bot_knives_only" );
	AddServerCommand( "bot_pistols_only" );
	AddServerCommand( "bot_snipers_only" );
	AddServerCommand( "bot_all_weapons" );
	AddServerCommand( "entity_dump" );
	AddServerCommand( "bot_nav_delete" );
	AddServerCommand( "bot_nav_split" );
	AddServerCommand( "bot_nav_merge" );
	AddServerCommand( "bot_nav_mark" );
	AddServerCommand( "bot_nav_begin_area" );
	AddServerCommand( "bot_nav_end_area" );
	AddServerCommand( "bot_nav_connect" );
	AddServerCommand( "bot_nav_disconnect" );
	AddServerCommand( "bot_nav_splice" );
	AddServerCommand( "bot_nav_crouch" );
	AddServerCommand( "bot_nav_jump" );
	AddServerCommand( "bot_nav_precise" );
	AddServerCommand( "bot_nav_no_jump" );
	AddServerCommand( "bot_nav_analyze" );
	AddServerCommand( "bot_nav_strip" );
	AddServerCommand( "bot_nav_save" );
	AddServerCommand( "bot_nav_load" );
	AddServerCommand( "bot_nav_use_place" );
	AddServerCommand( "bot_nav_place_floodfill" );
	AddServerCommand( "bot_nav_place_pick" );
	AddServerCommand( "bot_nav_toggle_place_mode" );
	AddServerCommand( "bot_nav_toggle_place_painting" );
	AddServerCommand( "bot_goto_mark" );
	AddServerCommand( "bot_memory_usage" );
	AddServerCommand( "bot_nav_mark_unnamed" );
	AddServerCommand( "bot_nav_warp" );
	AddServerCommand( "bot_nav_corner_select" );
	AddServerCommand( "bot_nav_corner_raise" );
	AddServerCommand( "bot_nav_corner_lower" );
	AddServerCommand( "bot_nav_check_consistency" );
}

bool CCSBotManager::BotAddCommand( BotProfileTeamType team, bool isFromConsole )
{
	if ( m_isLearningMap || g_engfuncs.pfnCheckParm( "-nobots", NULL ) != 0 )
		return false;
	const BotProfile* profile;
	if ( isFromConsole && CMD_ARGC() > 1 )
	{
		bool career = g_pGameRules != NULL && static_cast<CHalfLifeMultiplay*>( g_pGameRules )->IsCareer();
		if ( UTIL_IsNameTaken( CMD_ARGV( 1 ), career ) )
		{
			CONSOLE_ECHO( "Error - %s is already in the game.\n", CMD_ARGV( 1 ) );
			return true;
		}
		profile = TheBotProfiles->GetProfile( CMD_ARGV( 1 ), team );
		if ( profile == NULL )
		{
			CONSOLE_ECHO( "Error - no profile for '%s' exists.\n", CMD_ARGV( 1 ) );
			return true;
		}
	}
	else
	{
		if ( team == BOT_TEAM_ANY )
		{
			if ( !stricmp( cv_bot_join_team.string, "T" ) )
				team = BOT_TEAM_T;
			else if ( !stricmp( cv_bot_join_team.string, "CT" ) )
				team = BOT_TEAM_CT;
			else
			{
				TeamName selected = SelectDefaultTeam();
				if ( selected == TERRORIST )
					team = BOT_TEAM_T;
				else if ( selected == CT )
					team = BOT_TEAM_CT;
				else
					team = BOT_TEAM_ANY;
			}
		}
		BotDifficultyType difficulty = BOT_EASY;
		if ( cv_bot_difficulty.value >= 0.9f )
		{
			difficulty = BOT_NORMAL;
			if ( cv_bot_difficulty.value >= 1.9f )
				difficulty = cv_bot_difficulty.value >= 2.9f ? BOT_EXPERT : BOT_HARD;
		}
		profile = TheBotProfiles->GetRandomProfile( difficulty, team );
		if ( profile == NULL )
		{
			CONSOLE_ECHO( "All bot profiles at this difficulty level are in use.\n" );
			return true;
		}
	}
	if ( AddBot( profile, team ) && isFromConsole )
		CVAR_SET_FLOAT( "bot_quota", cv_bot_quota.value + 1.0f );
	return true;
}

bool CCSBotManager::AddBot( const BotProfile* profile, BotProfileTeamType requestedTeam )
{
	if ( !UTIL_IsGame( "czero" ) )
		return false;
	CHalfLifeMultiplay* rules = static_cast<CHalfLifeMultiplay*>( g_pGameRules );
	TeamName team = UNASSIGNED;
	if ( requestedTeam == BOT_TEAM_T )
		team = TERRORIST;
	else if ( requestedTeam == BOT_TEAM_CT )
		team = CT;
	else if ( requestedTeam == BOT_TEAM_ANY )
	{
		if ( !stricmp( cv_bot_join_team.string, "T" ) )
			team = TERRORIST;
		else if ( !stricmp( cv_bot_join_team.string, "CT" ) )
			team = CT;
		else
			team = SelectDefaultTeam();
	}
	if ( team == UNASSIGNED || rules->TeamFull( team ) )
	{
		CONSOLE_ECHO( "Could not add bot to the game: Team is full\n" );
		return false;
	}
	if ( rules->TeamStacked( team, UNASSIGNED ) )
	{
		CONSOLE_ECHO( "Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round).\n" );
		return false;
	}
	CCSBot* bot = CreateBot<CCSBot>( profile );
	if ( bot == NULL )
		return false;
	ClientPutInServer( bot->edict() );
	SET_CLIENT_KEY_VALUE( bot->entindex(), GET_USERINFO( bot->edict() ), "*bot", "1" );
	bot->m_iMenu = Menu_ChooseTeam;
	bot->m_iJoiningState = PICKINGTEAM;
	if ( HandleMenu_ChooseTeam( bot, team ) )
	{
		int skin = profile->GetSkin();
		HandleMenu_ChooseAppearance( bot, skin == 0 ? 6 : skin );
		if ( IS_DEDICATED_SERVER() )
			UTIL_DPrintf( "Added bot %s to server\n", STRING( bot->pev->netname ) );
		return true;
	}
	SERVER_COMMAND( UTIL_VarArgs( "kick \"%s\"\n", STRING( bot->pev->netname ) ) );
	CONSOLE_ECHO( "Could not add bot to the game.\n" );
	return false;
}

void CCSBotManager::ClientDisconnect( CBasePlayer* player )
{
	if ( player == NULL || !player->IsBot() )
		return;
	static_cast<CCSBot*>( player )->Disconnect();
	edict_t* entity = player->edict();
	if ( player->pev->classname != 0 )
		RemoveEntityHashValue( player->pev, STRING( player->pev->classname ), CLASSNAME );
	FREE_PRIVATE( entity );
	CBasePlayer* replacement = GetClassPtr( reinterpret_cast<CBasePlayer*>( VARS( entity ) ) );
	AddEntityHashValue( replacement->pev, STRING( replacement->pev->classname ), CLASSNAME );
	replacement->pev->flags = FL_DORMANT;
}

unsigned int CCSBotManager::GetPlayerPriority( CBasePlayer* player ) const
{
	if ( !player->IsPlayer() )
		return static_cast<unsigned int>( -1 );
	if ( !player->IsBot() )
		return 0;
	CCSBot* bot = static_cast<CCSBot*>( player );
	if ( m_scenario == SCENARIO_RESCUE_HOSTAGES && player->m_iTeam == CT && bot->m_hostageEscortCount != 0 )
		return 1;
	if ( m_scenario == SCENARIO_ESCORT_VIP && player->m_iTeam == CT && player->m_bIsVIP )
		return 1;
	if ( m_scenario == SCENARIO_DEFUSE_BOMB && player->m_iTeam == TERRORIST && player->m_bHasC4 )
		return 1;
	return bot->GetID() + 1;
}

void PrintAllEntities()
{
	for ( int i = 1; i < gpGlobals->maxEntities; ++i )
	{
		edict_t* entity = INDEXENT( i );
		if ( entity != NULL && entity->v.classname != 0 )
			CONSOLE_ECHO( "  %s\n", STRING( entity->v.classname ) );
	}
}

void CCSBotManager::ServerCommand( const char* command )
{
	if ( !m_bServerActive || !UTIL_IsGame( "czero" ) )
		return;
	if ( FStrEq( command, "bot_about" ) )
	{
		char message[400];
		sprintf( message,
			"\n--------------------------------------------------------------------------\n"
			"The Official Counter-Strike Bot V%d.%02d\n"
			"Created by Michael S. Booth\n"
			"Web: www.turtlerockstudios.com\\csbot\n"
			"E-mail: csbot@turtlerockstudios.com\n"
			"--------------------------------------------------------------------------\n\n", 1, 50 );
		CONSOLE_ECHO( message );
		HintMessageToAllPlayers( message );
		return;
	}
	if ( FStrEq( command, "bot_add" ) )
	{
		BotAddCommand( BOT_TEAM_ANY, true );
		return;
	}
	if ( FStrEq( command, "bot_add_t" ) )
	{
		BotAddCommand( BOT_TEAM_T, true );
		return;
	}
	if ( FStrEq( command, "bot_add_ct" ) )
	{
		BotAddCommand( BOT_TEAM_CT, true );
		return;
	}
	if ( FStrEq( command, "bot_kill" ) || FStrEq( command, "bot_kick" ) )
	{
		bool all = CMD_ARGC() == 1 || !stricmp( CMD_ARGV( 1 ), "all" );
		bool kick = FStrEq( command, "bot_kick" );
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBaseEntity* entity = UTIL_PlayerByIndex( i );
			if ( entity == NULL || entity->pev == NULL || FNullEnt( entity->pev ) )
				continue;
			const char* name = STRING( entity->pev->netname );
			CBasePlayer* player = static_cast<CBasePlayer*>( entity );
			if ( *name == '\0' || !player->IsBot() || (!all && stricmp( CMD_ARGV( 1 ), name )) )
				continue;
			if ( kick )
			{
				SERVER_COMMAND( UTIL_VarArgs( "kick \"%s\"\n", name ) );
				CVAR_SET_FLOAT( "bot_quota", cv_bot_quota.value - 1.0f );
			}
			else
			{
				entity->TakeDamage( entity->pev, entity->pev, 9999.9f, DMG_CRUSH );
			}
		}
		if ( kick && (all || cv_bot_quota.value < 0.0f) )
			CVAR_SET_FLOAT( "bot_quota", 0.0f );
		return;
	}
	bool allWeapons = FStrEq( command, "bot_all_weapons" );
	bool pistolsOnly = FStrEq( command, "bot_pistols_only" );
	bool snipersOnly = FStrEq( command, "bot_snipers_only" );
	if ( allWeapons || pistolsOnly || snipersOnly || FStrEq( command, "bot_knives_only" ) )
	{
		CVAR_SET_FLOAT( "bot_allow_pistols", allWeapons || pistolsOnly || snipersOnly ? 1.0f : 0.0f );
		CVAR_SET_FLOAT( "bot_allow_shotguns", allWeapons ? 1.0f : 0.0f );
		CVAR_SET_FLOAT( "bot_allow_sub_machine_guns", allWeapons ? 1.0f : 0.0f );
		CVAR_SET_FLOAT( "bot_allow_rifles", allWeapons ? 1.0f : 0.0f );
		CVAR_SET_FLOAT( "bot_allow_machine_guns", allWeapons ? 1.0f : 0.0f );
		CVAR_SET_FLOAT( "bot_allow_grenades", allWeapons ? 1.0f : 0.0f );
		CVAR_SET_FLOAT( "bot_allow_snipers", allWeapons || snipersOnly ? 1.0f : 0.0f );
		CVAR_SET_FLOAT( "bot_allow_shield", allWeapons ? 1.0f : 0.0f );
		return;
	}
	if ( FStrEq( command, "entity_dump" ) )
	{
		PrintAllEntities();
		return;
	}
	static const struct
	{
		const char* command;
		NavEditCmdType edit;
	} editCommand[] =
	{
		{ "bot_nav_delete", EDIT_DELETE },
		{ "bot_nav_split", EDIT_SPLIT },
		{ "bot_nav_merge", EDIT_MERGE },
		{ "bot_nav_mark", EDIT_MARK },
		{ "bot_nav_begin_area", EDIT_BEGIN_AREA },
		{ "bot_nav_end_area", EDIT_END_AREA },
		{ "bot_nav_connect", EDIT_CONNECT },
		{ "bot_nav_disconnect", EDIT_DISCONNECT },
		{ "bot_nav_splice", EDIT_SPLICE },
		{ "bot_nav_crouch", EDIT_ATTRIB_CROUCH },
		{ "bot_nav_jump", EDIT_ATTRIB_JUMP },
		{ "bot_nav_precise", EDIT_ATTRIB_PRECISE },
		{ "bot_nav_no_jump", EDIT_ATTRIB_NO_JUMP },
		{ "bot_nav_toggle_place_mode", EDIT_TOGGLE_PLACE_MODE },
		{ "bot_nav_place_floodfill", EDIT_PLACE_FLOODFILL },
		{ "bot_nav_place_pick", EDIT_PLACE_PICK },
		{ "bot_nav_toggle_place_painting", EDIT_TOGGLE_PLACE_PAINTING },
		{ "bot_nav_mark_unnamed", EDIT_MARK_UNNAMED },
		{ "bot_nav_warp", EDIT_WARP_TO_MARK },
		{ "bot_nav_corner_select", EDIT_SELECT_CORNER },
		{ "bot_nav_corner_raise", EDIT_RAISE_CORNER },
		{ "bot_nav_corner_lower", EDIT_LOWER_CORNER }
	};
	for ( int i = 0; i < ARRAYSIZE( editCommand ); ++i )
	{
		if ( FStrEq( command, editCommand[i].command ) )
		{
			m_editCmd = editCommand[i].edit;
			return;
		}
	}
	if ( FStrEq( command, "bot_nav_analyze" ) )
	{
		m_isAnalysisRequested = true;
	}
	else if ( FStrEq( command, "bot_nav_strip" ) )
	{
		StripNavigationAreas();
	}
	else if ( FStrEq( command, "bot_nav_save" ) )
	{
		char filename[256];
		GET_GAME_DIR( filename );
		strcat( filename, "\\" );
		strcat( filename, TheBots->GetNavMapFilename() );
		if ( SaveNavigationMap( filename ) )
			CONSOLE_ECHO( "Navigation map '%s' saved.\n", filename );
		else
			CONSOLE_ECHO( "ERROR: Cannot save navigation map '%s'.\n", filename );
	}
	else if ( FStrEq( command, "bot_nav_load" ) )
	{
		ValidateMapData();
	}
	else if ( FStrEq( command, "bot_nav_use_place" ) )
	{
		const BotPhraseList* places = TheBotPhrases->GetPlaceList();
		if ( CMD_ARGC() == 1 )
		{
			int count = 0;
			for ( BotPhraseList::const_iterator iter = places->begin(); iter != places->end(); ++iter )
			{
				if ( (*iter)->GetID() == m_navPlace )
					CONSOLE_ECHO( "--> %-26s", (*iter)->GetName() );
				else
					CONSOLE_ECHO( "%-30s", (*iter)->GetName() );
				if ( ++count % 3 == 0 )
					CONSOLE_ECHO( "\n" );
			}
			CONSOLE_ECHO( "\n" );
			return;
		}
		const BotPhrase* match = NULL;
		bool ambiguous = false;
		for ( BotPhraseList::const_iterator iter = places->begin(); iter != places->end(); ++iter )
		{
			const char* name = (*iter)->GetName();
			if ( !strnicmp( name, CMD_ARGV( 1 ), strlen( CMD_ARGV( 1 ) ) ) )
			{
				if ( !stricmp( name, CMD_ARGV( 1 ) ) )
				{
					match = *iter;
					ambiguous = false;
					break;
				}
				if ( match != NULL )
					ambiguous = true;
				else
					match = *iter;
			}
		}
		if ( ambiguous )
			CONSOLE_ECHO( "Ambiguous\n" );
		else if ( match != NULL )
		{
			CONSOLE_ECHO( "Current place set to '%s'\n", match->GetName() );
			m_navPlace = match->GetID();
		}
	}
	else if ( FStrEq( command, "bot_goto_mark" ) )
	{
		CNavArea* marked = GetMarkedArea();
		if ( marked == NULL )
			return;
		CBaseEntity* entity = NULL;
		while ( (entity = UTIL_FindEntityByClassname( entity, "player" )) != NULL && !FNullEnt( entity->pev ) )
		{
			if ( entity->IsPlayer() && entity->pev->flags != FL_DORMANT )
			{
				CBasePlayer* player = GetClassPtr( reinterpret_cast<CBasePlayer*>( entity->pev ) );
				if ( player->IsBot() )
				{
					static_cast<CCSBot*>( player )->MoveTo( marked->GetCenter(), FASTEST_ROUTE );
					return;
				}
			}
		}
	}
	else if ( FStrEq( command, "bot_memory_usage" ) )
	{
		CONSOLE_ECHO( "Memory usage:\n" );
		CONSOLE_ECHO( "  %d bytes per bot\b", static_cast<int>( sizeof( CCSBot ) ) );
		int areas = TheNavAreaGrid.GetNavAreaCount();
		CONSOLE_ECHO( "  %d Navigation Areas @ %d bytes each = %d bytes\n", areas,
			static_cast<int>( sizeof( CNavArea ) ), areas * static_cast<int>( sizeof( CNavArea ) ) );
		int spots = static_cast<int>( TheHidingSpotList.size() );
		CONSOLE_ECHO( "  %d Hiding Spots @ %d bytes each = %d bytes\n", spots,
			static_cast<int>( sizeof( HidingSpot ) ), spots * static_cast<int>( sizeof( HidingSpot ) ) );
		int encounterBytes = 0;
		for ( NavAreaList::const_iterator area = TheNavAreaList.begin(); area != TheNavAreaList.end(); ++area )
		{
			const SpotEncounterList& encounters = (*area)->m_spotEncounterList;
			for ( SpotEncounterList::const_iterator encounter = encounters.begin(); encounter != encounters.end(); ++encounter )
			{
				SpotOrderList order = encounter->spotList;
				encounterBytes += static_cast<int>( sizeof( SpotEncounter ) + order.size() * sizeof( SpotOrder ) );
			}
		}
		CONSOLE_ECHO( "  Encounter Spot data = %d bytes\n", encounterBytes );
	}
	else if ( FStrEq( command, "bot_nav_check_consistency" ) )
	{
		if ( CMD_ARGC() == 2 )
			SanityCheckNavigationMap( CMD_ARGV( 1 ) );
		else
			CONSOLE_ECHO( "usage: bot_nav_check_consistency <filename>\n" );
	}
}

void CCSBotManager::SetLooseBomb( CBaseEntity* bomb )
{
	m_looseBomb = bomb;
	m_looseBombArea = bomb != NULL ? TheNavAreaGrid.GetNearestNavArea( &bomb->pev->origin ) : NULL;
}

void CCSBotManager::OnEvent( GameEventType event, CBaseEntity* entity, CBaseEntity* other )
{
	switch ( event )
	{
	case EVENT_BOMB_PLANTED:
		m_isBombPlanted = true;
		m_bombPlantTimestamp = gpGlobals->time;
		break;
	case EVENT_BOMB_DEFUSING:
		m_bombDefuser = static_cast<CBasePlayer*>( entity );
		break;
	case EVENT_BOMB_DEFUSE_ABORTED:
		m_bombDefuser = NULL;
		break;
	case EVENT_BOMB_DEFUSED:
		m_isBombPlanted = false;
		m_bombDefuser = NULL;
		break;
	case EVENT_TERRORISTS_WIN:
	case EVENT_CTS_WIN:
	case EVENT_ROUND_DRAW:
		m_isRoundOver = true;
		break;
	case EVENT_RADIO_ENEMY_SPOTTED:
		m_lastSeenEnemyTimestamp = gpGlobals->time;
		break;
	default:
		break;
	}
	CBotManager::OnEvent( event, entity, other );
}

bool CCSBotManager::IsWeaponUseable( CBasePlayerItem* weapon )
{
	if ( weapon == NULL )
		return false;
	if ( weapon->m_iId == WEAPON_C4 )
		return true;

	WeaponClassType weaponClass = WeaponIDToWeaponClass( weapon->m_iId );
	if ( cv_bot_allow_shotguns.value <= 0.0f && weaponClass == WEAPONCLASS_SHOTGUN )
		return false;
	if ( cv_bot_allow_machine_guns.value <= 0.0f && weaponClass == WEAPONCLASS_MACHINEGUN )
		return false;
	if ( cv_bot_allow_rifles.value <= 0.0f && weaponClass == WEAPONCLASS_RIFLE )
		return false;
	if ( cv_bot_allow_shotguns.value <= 0.0f && weaponClass == WEAPONCLASS_SHOTGUN )
		return false;
	if ( cv_bot_allow_snipers.value <= 0.0f && weaponClass == WEAPONCLASS_SNIPERRIFLE )
		return false;
	if ( cv_bot_allow_sub_machine_guns.value <= 0.0f && weaponClass == WEAPONCLASS_SUBMACHINEGUN )
		return false;
	if ( cv_bot_allow_shield.value <= 0.0f && weapon->m_iId == WEAPON_SHIELDGUN )
		return false;
	if ( cv_bot_allow_pistols.value <= 0.0f && weaponClass == WEAPONCLASS_PISTOL )
		return false;
	return cv_bot_allow_grenades.value > 0.0f || weaponClass != WEAPONCLASS_GRENADE;
}

bool CCSBotManager::IsOnDefense( const CBasePlayer* player ) const
{
	switch ( m_scenario )
	{
	case SCENARIO_RESCUE_HOSTAGES:
	case SCENARIO_ESCORT_VIP:
		return player->m_iTeam == TERRORIST;
	case SCENARIO_DEFUSE_BOMB:
		return player->m_iTeam == CT;
	default:
		return false;
	}
}

bool CCSBotManager::IsOnOffense( const CBasePlayer* player ) const
{
	return !IsOnDefense( player );
}

bool CCSBotManager::IsImportantPlayer( CBasePlayer* player ) const
{
	if ( m_scenario == SCENARIO_DEFUSE_BOMB )
		return player->m_iTeam == TERRORIST && player->IsBombGuy();
	return m_scenario == SCENARIO_ESCORT_VIP && player->m_iTeam == CT && player->m_bIsVIP;
}

float CCSBotManager::GetBombTimeLeft() const
{
	return static_cast<CHalfLifeMultiplay*>( g_pGameRules )->m_iC4Timer - (gpGlobals->time - m_bombPlantTimestamp);
}

const CCSBotManager::Zone* CCSBotManager::GetZone( const Vector* position ) const
{
	for ( int i = 0; i < m_zoneCount; ++i )
	{
		const Extent& extent = m_zone[i].m_extent;
		if ( position->x >= extent.lo.x && position->x <= extent.hi.x &&
			position->y >= extent.lo.y && position->y <= extent.hi.y &&
			position->z >= extent.lo.z && position->z <= extent.hi.z )
			return &m_zone[i];
	}
	return NULL;
}

const CCSBotManager::Zone* CCSBotManager::GetClosestZone( const Vector* position ) const
{
	const Zone* closest = NULL;
	float closestDistance = 1000000000.0f;
	for ( int i = 0; i < m_zoneCount; ++i )
	{
		float distance = (m_zone[i].m_center - *position).LengthSquared();
		if ( distance < closestDistance )
		{
			closestDistance = distance;
			closest = &m_zone[i];
		}
	}
	return closest;
}

CNavArea* CCSBotManager::GetRandomAreaInZone( const Zone* zone ) const
{
	if ( zone->m_areaCount == 0 )
	{
		ASSERT_AT( false && "CCSBotManager::GetRandomAreaInZone: No areas for this zone", 1462 );
		return NULL;
	}
	int i = RANDOM_LONG( 0, zone->m_areaCount - 1 );
	return zone->m_area[i];
}

const Vector* CCSBotManager::GetRandomPositionInZone( const Zone* zone ) const
{
	static Vector position;
	if ( zone == NULL || zone->m_areaCount == 0 )
		return NULL;
	CNavArea* area = zone->m_area[RANDOM_LONG( 0, zone->m_areaCount - 1 )];
	if ( zone->m_isLegacy )
	{
		area->GetClosestPointOnArea( &zone->m_center, &position );
	}
	else
	{
		const Extent* extent = area->GetExtent();
		float lowX = zone->m_extent.lo.x;
		float lowY = zone->m_extent.lo.y;
		float highX = extent->hi.x;
		float highY = extent->hi.y;
		if ( lowX <= extent->lo.x )
			lowX = extent->lo.x;
		if ( lowY <= extent->lo.y )
			lowY = extent->lo.y;
		if ( highX > zone->m_extent.hi.x )
			highX = zone->m_extent.hi.x;
		if ( highY > zone->m_extent.hi.y )
			highY = zone->m_extent.hi.y;
		position.x = 0.5f * (lowX + highX);
		position.y = 0.5f * (lowY + highY);
		position.z = area->GetZ( &position );
	}
	return &position;
}

float CCSBotManager::GetRadioMessageTimestamp( GameEventType event, int team ) const
{
	if ( event > EVENT_START_RADIO_1 && event < EVENT_END_RADIO )
		return m_radioMsgTimestamp[event - EVENT_START_RADIO_1][team - 1];
	return 0.0f;
}

float CCSBotManager::GetRadioMessageInterval( GameEventType event, int team ) const
{
	if ( event > EVENT_START_RADIO_1 && event < EVENT_END_RADIO )
		return gpGlobals->time - m_radioMsgTimestamp[event - EVENT_START_RADIO_1][team - 1];
	return 100000000.0f;
}

void CCSBotManager::SetRadioMessageTimestamp( GameEventType event, int team )
{
	if ( event > EVENT_START_RADIO_1 && event < EVENT_END_RADIO )
		m_radioMsgTimestamp[event - EVENT_START_RADIO_1][team - 1] = gpGlobals->time;
}

void CCSBotManager::ResetRadioMessageTimestamps()
{
	for ( int team = 0; team < 2; ++team )
	{
		for ( int event = 0; event < EVENT_END_RADIO - EVENT_START_RADIO_1; ++event )
			m_radioMsgTimestamp[event][team] = 0.0f;
	}
}
