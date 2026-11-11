/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// teamplay_gamerules.cpp
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "items.h"
#include "gamerules.h"
#include "game.h"
#include "skill.h"
#include "client.h"
#include "career_tasks.h"
#include "tutor.h"
#include "debug.h"
#include "trains.h"
#include "hltv.h"
#include "bot/bot_util.h"
#include "bot/bot.h"
#include "bot/cs_bot.h"
#include "hostage/hostage.h"

#include <ctype.h>

extern BOOL g_fGameOver;
extern int gmsgGameMode;
extern int gmsgTeamScore;
extern int gmsgSendAudio;
extern int gmsgCZCareer;
extern int gmsgCZCareerHUD;
extern int gmsgServerName;
extern int gmsgMOTD;
extern int g_iHostageNumber;
extern int gmsgScoreInfo;
extern int gmsgTeamInfo;
extern int gmsgLocation;
extern int gmsgShowTimer;
extern int gmsgHLTV;
extern int gmsgForceCam;
extern int gmsgAllowSpec;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgViewMode;
extern int gmsgShadowIdx;
extern int gmsgHealthInfo;
extern int gmsgAccount;
extern int gmsgRadar;
extern int gmsgBombDrop;
extern unsigned short g_iShadowSprite;
extern unsigned short m_usResetDecals;
extern float g_flResetTime;
extern float g_flTimeLimit;

void ClearBodyQue();

CHalfLifeMultiplay* g_pMPGameRules = NULL;
cvar_t* sv_clienttrace = NULL;

class CCStrikeGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	bool CanPlayerHearPlayer( CBasePlayer* pListener, CBasePlayer* pSender ) override;
};

static CCStrikeGameMgrHelper g_GameMgrHelper;

bool CCStrikeGameMgrHelper::CanPlayerHearPlayer( CBasePlayer* pListener, CBasePlayer* pSender )
{
	if ( !pSender->IsPlayer() || pListener->m_iTeam != pSender->m_iTeam )
		return false;
	BOOL listenerAlive = pListener->IsAlive();
	BOOL senderAlive = pSender->IsAlive();
	return pListener->pev->iuser1 || !listenerAlive || senderAlive;
}

class CMapInfo : public CPointEntity
{
public:
	void KeyValue( KeyValueData* pkvd ) override;
	void Spawn() override;

	int		m_iBuyingStatus;
	float	m_flBombRadius;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

LINK_ENTITY_TO_CLASS( info_map_parameters, CMapInfo );

void CMapInfo::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
}

void CMapInfo::KeyValue( KeyValueData* pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "buying" ) )
	{
		m_iBuyingStatus = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "bombradius" ) )
	{
		m_flBombRadius = (float)atoi( pkvd->szValue );
		if ( m_flBombRadius > 2048 )
			m_flBombRadius = 2048;
		pkvd->fHandled = TRUE;
	}
}

static void ReadMultiplayCvars( CHalfLifeMultiplay* pRules )
{
	pRules->m_iRoundTime = (int)( CVAR_GET_FLOAT( "mp_roundtime" ) * 60 );
	pRules->m_iC4Timer = (int)CVAR_GET_FLOAT( "mp_c4timer" );
	pRules->m_iIntroRoundTime = (int)CVAR_GET_FLOAT( "mp_freezetime" );
	pRules->m_iLimitTeams = (int)CVAR_GET_FLOAT( "mp_limitteams" );
	if ( pRules->m_iRoundTime > 540 )
	{
		CVAR_SET_FLOAT( "mp_roundtime", 9 );
		pRules->m_iRoundTime = 540;
	}
	else if ( pRules->m_iRoundTime < 60 )
	{
		CVAR_SET_FLOAT( "mp_roundtime", 1 );
		pRules->m_iRoundTime = 60;
	}
	if ( pRules->m_iIntroRoundTime > 60 )
	{
		CVAR_SET_FLOAT( "mp_freezetime", 60 );
		pRules->m_iIntroRoundTime = 60;
	}
	else if ( pRules->m_iIntroRoundTime < 0 )
	{
		CVAR_SET_FLOAT( "mp_freezetime", 0 );
		pRules->m_iIntroRoundTime = 0;
	}
	if ( pRules->m_iC4Timer > 90 )
	{
		CVAR_SET_FLOAT( "mp_c4timer", 90 );
		pRules->m_iC4Timer = 90;
	}
	else if ( pRules->m_iC4Timer < 10 )
	{
		CVAR_SET_FLOAT( "mp_c4timer", 10 );
		pRules->m_iC4Timer = 10;
	}
	if ( pRules->m_iLimitTeams > 20 )
	{
		CVAR_SET_FLOAT( "mp_limitteams", 20 );
		pRules->m_iLimitTeams = 20;
	}
	else if ( pRules->m_iLimitTeams < 0 )
	{
		CVAR_SET_FLOAT( "mp_limitteams", 0 );
		pRules->m_iLimitTeams = 0;
	}
}

void SV_Continue_f();
void SV_CareerMatchLimit_f();
void SV_CareerAddTask_f();
void SV_Career_EndRound_f();
void SV_Career_Restart_f();
void SV_Tutor_Toggle_f();

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

CHalfLifeMultiplay::CHalfLifeMultiplay()
{
	CareerSessionRestoreSettings();
	m_VoiceGameMgr.Init( &g_GameMgrHelper, gpGlobals->maxClients );
	RefreshSkillData();
	g_fGameOver = FALSE;
	m_iLoserBonus = 1400;
	m_iNumConsecutiveCTLoses = 0;
	m_iNumConsecutiveTerroristLoses = 0;
	m_iC4Guy = 0;
	m_pVIP = NULL;
	m_bMapHasCameras = 2;
	m_flIntermissionEndTime = 0;
	m_flIntermissionStartTime = 0;
	m_fTeamCount = 0;
	m_iAccountCT = 0;
	m_iAccountTerrorist = 0;
	m_iHostagesRescued = 0;
	m_iRoundWinStatus = 0;
	m_iNumCTWins = 0;
	m_iNumTerroristWins = 0;
	m_iNumCT = 0;
	m_iNumTerrorist = 0;
	m_iNumSpawnableCT = 0;
	m_iNumSpawnableTerrorist = 0;
	m_bBombDefused = false;
	m_bTargetBombed = false;
	m_bFreezePeriod = TRUE;
	m_bLevelInitialized = false;
	m_tmNextPeriodicThink = 0;
	m_iNumEscapers = 0;
	m_iConsecutiveVIP = 0;
	m_iUnBalancedRounds = 0;
	m_iNumEscapeRounds = 0;
	m_bFirstConnected = false;
	m_bCompleteReset = false;
	m_flRequiredEscapeRatio = 0.5f;
	m_bCTCantBuy = false;
	m_bTCantBuy = false;
	m_flBombRadius = 500;
	m_iTotalGunCount = 0;
	m_iTotalGrenadeCount = 0;
	m_iTotalArmourCount = 0;
	m_bRoundTerminating = false;
	m_bBombDropped = FALSE;
	g_iHostageNumber = 0;
	m_iMaxRounds = (int)CVAR_GET_FLOAT( "mp_maxrounds" );
	if ( m_iMaxRounds < 0 )
	{
		m_iMaxRounds = 0;
		CVAR_SET_FLOAT( "mp_maxrounds", 0 );
	}
	m_iTotalRoundsPlayed = 0;
	m_iMaxRoundsWon = (int)CVAR_GET_FLOAT( "mp_winlimit" );
	if ( m_iMaxRoundsWon < 0 )
	{
		m_iMaxRoundsWon = 0;
		CVAR_SET_FLOAT( "mp_winlimit", 0 );
	}
	memset( m_iMapVotes, 0, sizeof( m_iMapVotes ) );
	m_iLastPick = 1;
	m_iStoredSpectValue = (int)allow_spectators.value;
	m_bMapHasEscapeZone = false;
	m_iMapHasVIPSafetyZone = 0;
	m_bMapHasBombZone = false;
	m_bMapHasRescueZone = false;
	memset( VIPQueue, 0, sizeof( VIPQueue ) );
	CVAR_SET_FLOAT( "cl_himodels", 0 );
	ReadMultiplayCvars( this );
	m_iIntroRoundTime += 2;
	m_fMaxIdlePeriod = (float)( 2 * m_iRoundTime );
	float idleTimeout = CVAR_GET_FLOAT( "mp_autokick_timeout" );
	if ( idleTimeout > 0 )
		m_fMaxIdlePeriod = idleTimeout;
	m_bInCareerGame = false;
	m_iRoundTimeSecs = m_iIntroRoundTime;

	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that 
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if ( IS_DEDICATED_SERVER() )
		CVAR_SET_FLOAT( "pausable", 0 );
	else if ( IsCareer() )
	{
		CVAR_SET_FLOAT( "pausable", 1 );
		CVAR_SET_FLOAT( "sv_aim", 0 );
		CVAR_SET_FLOAT( "sv_maxspeed", 322 );
		CVAR_SET_FLOAT( "sv_cheats", 0 );
		CVAR_SET_FLOAT( "mp_windifference", 2 );
		m_bInCareerGame = true;
		UTIL_LogPrintf( "Career Start\n" );
		// this code has been moved into engine, to only run server.cfg once
	}
	else
	{
		CVAR_SET_FLOAT( "pausable", 0 );
		const char* cfg = CVAR_GET_STRING( "lservercfgfile" );
		if ( cfg && *cfg )
		{
			char command[256];
			ALERT( at_console, "Executing listen server config file\n" );
			sprintf( command, "exec %s\n", cfg );
			SERVER_COMMAND( command );
		}
	}
	m_fRoundCount = 0;
	m_fIntroRoundCount = 0;
	InstallBotControl();
	InstallHostageManager();
	m_bSkipSpawn = m_bInCareerGame;
	static bool installedCommands = false;
	if ( !installedCommands )
	{
		if ( UTIL_IsGame( "czero" ) )
		{
			g_engfuncs.pfnAddServerCommand( "career_continue", SV_Continue_f );
			g_engfuncs.pfnAddServerCommand( "career_matchlimit", SV_CareerMatchLimit_f );
			g_engfuncs.pfnAddServerCommand( "career_add_task", SV_CareerAddTask_f );
			g_engfuncs.pfnAddServerCommand( "career_endround", SV_Career_EndRound_f );
			g_engfuncs.pfnAddServerCommand( "career_restart", SV_Career_Restart_f );
			g_engfuncs.pfnAddServerCommand( "tutor_toggle", SV_Tutor_Toggle_f );
		}
		g_engfuncs.pfnAddServerCommand( "perf_test", loopPerformance );
		g_engfuncs.pfnAddServerCommand( "print_ent", printEntities );
		installedCommands = true;
	}
	m_fCareerRoundMenuTime = 0;
	m_fCareerMatchMenuTime = 0;
	m_iCareerMatchWins = 0;
	m_iRoundWinDifference = (int)CVAR_GET_FLOAT( "mp_windifference" );
	CCareerTaskManager::Create();
	if ( m_iRoundWinDifference <= 0 )
	{
		m_iRoundWinDifference = 1;
		CVAR_SET_FLOAT( "mp_windifference", 1 );
	}
	sv_clienttrace = CVAR_GET_POINTER( "sv_clienttrace" );
	InstallTutor( CVAR_GET_FLOAT( "tutor_enable" ) > 0 );
	g_pMPGameRules = this;
	CareerSessionRestoreRules( this );
}

void SV_CareerAddTask_f()
{
	if ( CMD_ARGC() == 7 )
	{
		const char* task = CMD_ARGV( 1 );
		const char* weapon = CMD_ARGV( 2 );
		int count = atoi( CMD_ARGV( 3 ) );
		bool mustLive = atoi( CMD_ARGV( 4 ) ) != 0;
		bool crossRounds = atoi( CMD_ARGV( 5 ) ) != 0;
		bool complete = atoi( CMD_ARGV( 6 ) ) != 0;
		TheCareerTasks->AddTask( task, weapon, count, mustLive, crossRounds, complete );
	}
}

void SV_Career_Restart_f()
{
	if ( g_engfuncs.pfnIsCareerMatch() )
		( (CHalfLifeMultiplay*)g_pGameRules )->CareerRestart();
}

void SV_CareerMatchLimit_f()
{
	if ( CMD_ARGC() == 3 && g_engfuncs.pfnIsCareerMatch() )
	{
		int difference = atoi( CMD_ARGV( 2 ) );
		int wins = atoi( CMD_ARGV( 1 ) );
		( (CHalfLifeMultiplay*)g_pGameRules )->SetCareerMatchLimit( wins, difference );
	}
}

void SV_Continue_f()
{
	CHalfLifeMultiplay* pRules = (CHalfLifeMultiplay*)g_pGameRules;
	if ( g_engfuncs.pfnIsCareerMatch() && pRules->m_fTeamCount > 100000 )
	{
		pRules->m_fTeamCount = gpGlobals->time;
		MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
			WRITE_STRING( "GOGOGO" );
		MESSAGE_END();
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
			if ( pPlayer && !pPlayer->IsBot() )
				pPlayer->m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
		}
	}
}

void SV_Career_EndRound_f()
{
	CHalfLifeMultiplay* pRules = (CHalfLifeMultiplay*)g_pGameRules;
	if ( g_engfuncs.pfnIsCareerMatch() && pRules->IsInCareerRound() )
	{
		CBasePlayer* pLocal = UTIL_GetLocalPlayer();
		if ( pLocal )
		{
			SERVER_COMMAND( "kill\n" );
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
				if ( pPlayer && !FNullEnt( pPlayer->pev ) && pPlayer->IsBot() && pPlayer->m_iTeam == pLocal->m_iTeam )
					SERVER_COMMAND( UTIL_VarArgs( "bot_kill \"%s\"\n", STRING( pPlayer->pev->netname ) ) );
			}
		}
	}
}

char* GetTeam( int teamNo )
{
	switch ( teamNo )
	{
	case TERRORIST:
		return "TERRORIST";
	case CT:
		return "CT";
	case SPECTATOR:
		return "SPECTATOR";
	default:
		return "";
	}
}

int CountPlayers()
{
	int count = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		if ( UTIL_PlayerByIndex( i ) )
			++count;
	}
	return count;
}

void Broadcast( const char* sentence )
{
	if ( sentence )
	{
		char text[32];
		strcpy( text, "%!MRAD_" );
		strcat( text, UTIL_VarArgs( "%s", sentence ) );
		MESSAGE_BEGIN( MSG_BROADCAST, gmsgSendAudio );
			WRITE_BYTE( 0 );
			WRITE_STRING( text );
			WRITE_SHORT( PITCH_NORM );
		MESSAGE_END();
	}
}

void SV_Tutor_Toggle_f()
{
	CVAR_SET_FLOAT( "tutor_enable", !CVAR_GET_FLOAT( "tutor_enable" ) );
}

BOOL CHalfLifeMultiplay::IsMultiplayer()
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsDeathmatch()
{
	return TRUE;
}

BOOL CHalfLifeMultiplay::IsCoOp()
{
	return (int)gpGlobals->coop;
}

BOOL CHalfLifeMultiplay::IsCareer()
{
	return g_engfuncs.pfnIsCareerMatch();
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsAllowedToSpawn( CBaseEntity* pEntity )
{
//	if ( pEntity->pev->flags & FL_MONSTER )
//		return FALSE;

	return TRUE;
}

BOOL CHalfLifeMultiplay::FAllowFlashlight()
{
	static cvar_t* mp_flashlight = NULL;
	if ( !mp_flashlight )
	{
		mp_flashlight = CVAR_GET_POINTER( "mp_flashlight" );
		if ( !mp_flashlight )
			return FALSE;
	}
	return mp_flashlight->value != 0;
}

BOOL CHalfLifeMultiplay::FAllowMonsters()
{
	return CVAR_GET_FLOAT( "mp_allowmonsters" ) != 0;
}

void CHalfLifeMultiplay::RefreshSkillData()
{
// load all default values
	CGameRules::RefreshSkillData();

// override some values for multiplay.

	// suitcharger
	gSkillData.suitchargerCapacity = 30;
	gSkillData.plrDmg357 = 40;
	gSkillData.plrDmgM203Grenade = 100;
	gSkillData.plrDmgRPG = 120;

	// Glock Round
	gSkillData.plrDmg9MM = 12;
	gSkillData.plrDmgMP5 = 12;
	gSkillData.plrDmgBuckshot = 20;
	gSkillData.plrDmgCrossbowClient = 20;
}

BOOL CHalfLifeMultiplay::FShouldSwitchWeapon( CBasePlayer* pPlayer, CBasePlayerItem* pWeapon )
{
	if ( !pWeapon->CanDeploy() )
		return FALSE;
	CBasePlayerItem* pActive = pPlayer->m_pActiveItem;
	if ( !pActive )
		return TRUE;
	if ( !pPlayer->m_iAutoWepSwitch ||
			( pPlayer->m_iAutoWepSwitch == 2 && ( pPlayer->m_afButtonLast & ( IN_ATTACK | IN_ATTACK2 ) ) ) ||
			!pActive->CanHolster() )
		return FALSE;
	return pWeapon->iWeight() > pActive->iWeight();
}

BOOL CHalfLifeMultiplay::GetNextBestWeapon( CBasePlayer* pPlayer, CBasePlayerItem* pCurrentWeapon )
{
	if ( !pCurrentWeapon->CanHolster() )
		return FALSE;
	int bestWeight = -1;
	CBasePlayerItem* pBest = NULL;
	for ( int slot = 0; slot < MAX_ITEM_TYPES; ++slot )
	{
		for ( CBasePlayerItem* pWeapon = pPlayer->m_rgpPlayerItems[slot]; pWeapon; pWeapon = pWeapon->m_pNext )
		{
			if ( pWeapon != pCurrentWeapon && pWeapon->iWeight() > bestWeight && pWeapon->CanDeploy() )
			{
				pBest = pWeapon;
				bestWeight = pWeapon->iWeight();
			}
		}
	}
	if ( !pBest )
		return FALSE;
	pPlayer->SwitchWeapon( pBest );
	return TRUE;
}

BOOL CHalfLifeMultiplay::ClientConnected(
		edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128] )
{
	m_VoiceGameMgr.ClientConnected( pEntity );
	return TRUE;
}

BOOL CHalfLifeMultiplay::ClientCommand_DeadOrAlive( CBasePlayer* pPlayer, const char* pcmd )
{
	return m_VoiceGameMgr.ClientCommand( pPlayer, pcmd );
}

BOOL CHalfLifeMultiplay::ClientCommand( CBasePlayer* pPlayer, const char* pcmd )
{
	return FALSE;
}

void CHalfLifeMultiplay::ClientUserInfoChanged( CBasePlayer* pPlayer, char* infobuffer )
{
	pPlayer->SetPlayerModel( pPlayer->m_bHasC4 );
	pPlayer->SetPrefsFromUserinfo( infobuffer );
}

void CHalfLifeMultiplay::UpdateGameMode( CBasePlayer* pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
		WRITE_BYTE( 1 );  // game mode none
	MESSAGE_END();
}

float CHalfLifeMultiplay::FlPlayerFallDamage( CBasePlayer* pPlayer )
{
	const float safeFallSpeed = 500;
	const float fatalFallSpeed = 1100;
	pPlayer->m_flFallVelocity -= safeFallSpeed;
	return pPlayer->m_flFallVelocity * 100 / ( fatalFallSpeed - safeFallSpeed ) * 1.25f;
}

void CHalfLifeMultiplay::PlayerSpawn( CBasePlayer* pPlayer )
{
	if ( pPlayer->m_bJustConnected )
		return;
	int autoSwitch = pPlayer->m_iAutoWepSwitch;
	pPlayer->m_iAutoWepSwitch = 1;
	pPlayer->pev->weapons |= ( 1U << WEAPON_SUIT );
	bool noEquip = true;
	CBaseEntity* pEquip = NULL;
	while ( ( pEquip = UTIL_FindEntityByClassname( pEquip, "game_player_equip" ) ) != NULL )
	{
		noEquip = false;
		pEquip->Touch( pPlayer );
	}
	if ( ( !pPlayer->m_bNotKilled && noEquip ) || pPlayer->m_bIsVIP )
		pPlayer->GiveDefaultItems();
	pPlayer->SetPlayerModel( false );
	pPlayer->m_iAutoWepSwitch = autoSwitch;
}

BOOL CHalfLifeMultiplay::FPlayerCanRespawn( CBasePlayer* pPlayer )
{
	if ( pPlayer->m_iNumSpawns > 0 )
		return FALSE;
	m_iNumCT = CountTeamPlayers( CT );
	m_iNumTerrorist = CountTeamPlayers( TERRORIST );
	if ( m_iNumTerrorist > 0 && m_iNumCT > 0 && gpGlobals->time > m_fRoundCount + 20 )
	{
		if ( fadetoblack.value != 0 )
			UTIL_ScreenFade( pPlayer, g_vecZero, 3, 3, 255, FFADE_OUT | FFADE_STAYOUT );
		return FALSE;
	}
	return pPlayer->m_iMenu != Menu_ChooseAppearance;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlPlayerSpawnTime( CBasePlayer* pPlayer )
{
	return gpGlobals->time; //now!
}

edict_t* CHalfLifeMultiplay::GetPlayerSpawnSpot( CBasePlayer* pPlayer )
{
	edict_t* pSpot = CGameRules::GetPlayerSpawnSpot( pPlayer );
	if ( IsMultiplayer() && pSpot->v.target )
		FireTargets( STRING( pSpot->v.target ), pPlayer, pPlayer, USE_TOGGLE, 0 );
	return pSpot;
}

BOOL CHalfLifeMultiplay::AllowAutoTargetCrosshair()
{
	return FALSE;
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeMultiplay::IPointsForKill( CBasePlayer* pAttacker, CBasePlayer* pKilled )
{
	return 1;
}

BOOL CHalfLifeMultiplay::CanHavePlayerItem( CBasePlayer* pPlayer, CBasePlayerItem* pWeapon )
{
	return CGameRules::CanHavePlayerItem( pPlayer, pWeapon );
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeMultiplay::PlayerGotWeapon( CBasePlayer* pPlayer, CBasePlayerItem* pWeapon )
{
}

int CHalfLifeMultiplay::WeaponShouldRespawn( CBasePlayerItem* pWeapon )
{
	return ( pWeapon->pev->spawnflags & SF_NORESPAWN ) ? GR_WEAPON_RESPAWN_NO : GR_WEAPON_RESPAWN_YES;
}

float CHalfLifeMultiplay::FlWeaponRespawnTime( CBasePlayerItem* pWeapon )
{
	return gpGlobals->time + 20;
}

float CHalfLifeMultiplay::FlWeaponTryRespawn( CBasePlayerItem* pWeapon )
{
	if ( pWeapon && pWeapon->m_iId && ( pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD ) &&
			NUMBER_OF_ENTITIES() >= gpGlobals->maxEntities - 100 )

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime( pWeapon );
	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecWeaponRespawnSpot( CBasePlayerItem* pWeapon )
{
	return pWeapon->pev->origin;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::CanHaveItem( CBasePlayer* pPlayer, CItem* pItem )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotItem( CBasePlayer* pPlayer, CItem* pItem )
{
}

int CHalfLifeMultiplay::ItemShouldRespawn( CItem* pItem )
{
	return ( pItem->pev->spawnflags & SF_NORESPAWN ) ? GR_ITEM_RESPAWN_NO : GR_ITEM_RESPAWN_YES;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeMultiplay::FlItemRespawnTime( CItem* pItem )
{
	return gpGlobals->time + 30;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecItemRespawnSpot( CItem* pItem )
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotAmmo( CBasePlayer* pPlayer, char* szName, int iCount )
{
}

int CHalfLifeMultiplay::AmmoShouldRespawn( CBasePlayerAmmo* pAmmo )
{
	return ( pAmmo->pev->spawnflags & SF_NORESPAWN ) ? GR_AMMO_RESPAWN_NO : GR_AMMO_RESPAWN_YES;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlAmmoRespawnTime( CBasePlayerAmmo* pAmmo )
{
	return gpGlobals->time + 20;
}

//=========================================================
//=========================================================
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot( CBasePlayerAmmo* pAmmo )
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlHealthChargerRechargeTime()
{
	return 60;
}

float CHalfLifeMultiplay::FlHEVChargerRechargeTime()
{
	return 30;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerWeapons( CBasePlayer* pPlayer )
{
	return GR_PLR_DROP_GUN_ACTIVE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerAmmo( CBasePlayer* pPlayer )
{
	return GR_PLR_DROP_AMMO_ACTIVE;
}

BOOL CHalfLifeMultiplay::TeamFull( int team )
{
	if ( team == TERRORIST )
		return m_iNumTerrorist >= m_iSpawnPointCount_Terrorist;
	if ( team == CT )
		return m_iNumCT >= m_iSpawnPointCount_CT;
	return FALSE;
}

BOOL CHalfLifeMultiplay::TeamStacked( int newTeam, int currentTeam )
{
	if ( newTeam == currentTeam || !m_iLimitTeams )
		return FALSE;
	int adjustment = ( currentTeam == UNASSIGNED || currentTeam == SPECTATOR ) ? 0 : -1;
	if ( newTeam == TERRORIST )
		return m_iNumTerrorist >= m_iNumCT + m_iLimitTeams + adjustment;
	if ( newTeam == CT )
		return m_iNumCT >= m_iNumTerrorist + m_iLimitTeams + adjustment;
	return FALSE;
}

BOOL CHalfLifeMultiplay::IsThereABomber()
{
	CBaseEntity* pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
	{
		if ( FNullEnt( pEntity->edict() ) )
			break;
		CBasePlayer* pPlayer = (CBasePlayer*)pEntity;
		if ( pPlayer->m_iTeam != CT && pPlayer->IsBombGuy() )
			return TRUE;
	}
	return FALSE;
}

BOOL CHalfLifeMultiplay::IsThereABomb()
{
	CBaseEntity* pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "grenade" ) ) != NULL )
	{
		if ( ( (CGrenade*)pEntity )->m_bIsC4 )
			return TRUE;
	}
	return UTIL_FindEntityByClassname( NULL, "weapon_c4" ) != NULL;
}

bool CHalfLifeMultiplay::IsBombPlanted()
{
	if ( !m_bMapHasBombTarget )
		return false;
	CBaseEntity* pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "grenade" ) ) != NULL )
	{
		if ( ( (CGrenade*)pEntity )->m_bIsC4 )
			return true;
	}
	return false;
}

void CHalfLifeMultiplay::CheckMapConditions()
{
	if ( UTIL_FindEntityByClassname( NULL, "func_bomb_target" ) )
	{
		m_bMapHasBombTarget = true;
		m_bMapHasBombZone = true;
	}
	else
	{
		m_bMapHasBombTarget = UTIL_FindEntityByClassname( NULL, "info_bomb_target" ) != NULL;
		m_bMapHasBombZone = false;
	}
	m_bMapHasRescueZone = UTIL_FindEntityByClassname( NULL, "func_hostage_rescue" ) != NULL;
	m_bMapHasBuyZone = UTIL_FindEntityByClassname( NULL, "func_buyzone" ) != NULL;
	m_bMapHasEscapeZone = UTIL_FindEntityByClassname( NULL, "func_escapezone" ) != NULL;
	m_iMapHasVIPSafetyZone = UTIL_FindEntityByClassname( NULL, "func_vip_safetyzone" ) ? 1 : 2;
}

void CHalfLifeMultiplay::RemoveGuns()
{
	CBaseEntity* pEntity = NULL;

	//Find a weaponbox that includes an Egon, then destroy it
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "weaponbox" ) ) != NULL )
		( (CWeaponBox*)pEntity )->Kill();
	pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "weapon_shield" ) ) != NULL )
	{
		pEntity->SetThink( &CBaseEntity::SUB_Remove );
		pEntity->pev->nextthink = gpGlobals->time + 0.1f;
	}
}

void CHalfLifeMultiplay::UpdateTeamScores()
{
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamScore );
		WRITE_STRING( "CT" );
		WRITE_SHORT( m_iNumCTWins );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamScore );
		WRITE_STRING( "TERRORIST" );
		WRITE_SHORT( m_iNumTerroristWins );
	MESSAGE_END();
}

bool CHalfLifeMultiplay::IsInCareerRound()
{
	return m_fTeamCount == 0 && m_fCareerRoundMenuTime == 0 && m_fCareerMatchMenuTime == 0;
}

void CHalfLifeMultiplay::SetCareerMatchLimit( int minWins, int winDifference )
{
	if ( IsCareer() && !m_iCareerMatchWins )
	{
		m_iCareerMatchWins = minWins;
		m_iRoundWinDifference = winDifference;
	}
}

void CHalfLifeMultiplay::CareerRestart()
{
	g_fGameOver = FALSE;
	if ( m_fTeamCount == 0 )
		m_fTeamCount = gpGlobals->time + 1;
	m_bCompleteReset = true;
	m_fCareerRoundMenuTime = 0;
	m_fCareerMatchMenuTime = 0;
	TheCareerTasks->Reset( false );
	m_bSkipSpawn = false;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( pPlayer && !FNullEnt( pPlayer->pev ) && !pPlayer->IsBot() )
			pPlayer->ForceClientDllUpdate();
	}
}

void CHalfLifeMultiplay::ServerDeactivate()
{
	if ( IsCareer() )
	{
		CVAR_SET_FLOAT( "pausable", 0 );
		CVAR_SET_FLOAT( "mp_windifference", 1 );
		UTIL_LogPrintf( "Career End\n" );
	}
}

void CHalfLifeMultiplay::MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound( int team )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( pPlayer && !FNullEnt( pPlayer->pev ) && pPlayer->m_iTeam == team && pPlayer->pev->health > 0 &&
				pPlayer->pev->deadflag == DEAD_NO )
			pPlayer->m_bReceivesNoMoneyNextRound = true;
	}
}

void TerminateRound( float delay, int winStatus )
{
	CHalfLifeMultiplay* pRules = (CHalfLifeMultiplay*)g_pGameRules;
	pRules->m_iRoundWinStatus = winStatus;
	pRules->m_bRoundTerminating = true;
	pRules->m_fTeamCount = gpGlobals->time + delay;
	if ( winStatus != 3 && g_engfuncs.pfnIsCareerMatch() && !pRules->m_fCareerMatchMenuTime )
		pRules->QueueCareerRoundEndMenu( delay, winStatus );
}

int CHalfLifeMultiplay::PlayerRelationship( CBasePlayer* pPlayer, CBaseEntity* pTarget )
{
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;
	return GetClassPtr( (CBasePlayer*)pPlayer->pev )->m_iTeam == GetClassPtr( (CBasePlayer*)pTarget->pev )->m_iTeam;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FPlayerCanTakeDamage( CBasePlayer* pPlayer, CBaseEntity* pAttacker )
{
	if ( pAttacker && PlayerRelationship( pPlayer, pAttacker ) == GR_TEAMMATE )
		return CVAR_GET_FLOAT( "mp_friendlyfire" ) != 0 || pAttacker == pPlayer;
	return TRUE;
}

void CHalfLifeMultiplay::SwapAllPlayers()
{
	CBaseEntity* pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
	{
		if ( FNullEnt( pEntity->edict() ) )
			break;
		if ( pEntity->pev->flags != FL_DORMANT )
			GetClassPtr( (CBasePlayer*)pEntity->pev )->SwitchTeam();
	}
	short wins = m_iNumTerroristWins;
	m_iNumTerroristWins = m_iNumCTWins;
	m_iNumCTWins = wins;
	UpdateTeamScores();
}

void CHalfLifeMultiplay::ResetAllMapVotes()
{
	CBaseEntity* pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
	{
		if ( FNullEnt( pEntity->edict() ) )
			break;
		CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pEntity->pev );
		if ( pPlayer->m_iTeam != UNASSIGNED )
			pPlayer->m_iMapVote = 0;
	}
	memset( m_iMapVotes, 0, sizeof( m_iMapVotes ) );
}

void CHalfLifeMultiplay::StackVIPQueue()
{
	for ( int i = 2; i >= 0; --i )
	{
		if ( !VIPQueue[i] )
		{
			VIPQueue[i] = VIPQueue[i + 1];
			VIPQueue[i + 1] = NULL;
		}
		if ( !VIPQueue[i + 1] )
		{
			VIPQueue[i + 1] = VIPQueue[i + 2];
			VIPQueue[i + 2] = NULL;
		}
	}
}

bool CHalfLifeMultiplay::IsVIPQueueEmpty()
{
	for ( int i = 0; i < 5; ++i )
	{
		if ( VIPQueue[i] && VIPQueue[i]->m_iTeam != CT )
			VIPQueue[i] = NULL;
	}
	StackVIPQueue();
	for ( int i = 0; i < 5; ++i )
	{
		if ( VIPQueue[i] )
			return false;
	}
	return true;
}

void CHalfLifeMultiplay::SendMOTDToClient( edict_t* client )
{
	int length;
	char* file = (char*)LOAD_FILE_FOR_ME( (char*)CVAR_GET_STRING( "motdfile" ), &length );

	// send the server name
	MESSAGE_BEGIN( MSG_ONE, gmsgServerName, NULL, client );
		WRITE_STRING( CVAR_GET_STRING( "hostname" ) );
	MESSAGE_END();
	if ( file && *file )
	{
		char* text = file;
		int sent = 0;
		do
		{
			char chunk[61];
			if ( strlen( text ) < 60 )
				strcpy( chunk, text );
			else
			{
				strncpy( chunk, text, 60 );
				chunk[60] = 0;
			}
			sent += strlen( chunk );
			if ( sent > 1535 )
				*text = 0;
			else
				text = file + sent;
			MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, client );
				WRITE_BYTE( *text == 0 );
				WRITE_STRING( chunk );
			MESSAGE_END();
			if ( !*text )
				break;
		} while ( sent <= 1535 );
	}
	FREE_FILE( file );
}

void EndRoundMessage( const char* sentence, int event )
{
	CHalfLifeMultiplay* pRules = (CHalfLifeMultiplay*)g_pGameRules;
	UTIL_ClientPrintAll( HUD_PRINTCENTER, sentence );
	if ( event >= 0 && event <= 15 && ( ( 1 << event ) & 0xA21A ) )
	{
		TheBots->OnEvent( EVENT_TERRORISTS_WIN );
		if ( pRules )
			UTIL_LogPrintf( "Team \"%s\" triggered \"%s\" (CT \"%i\") (T \"%i\")\n", "TERRORIST", sentence + 1,
					pRules->m_iNumCTWins, pRules->m_iNumTerroristWins );
	}
	else if ( event >= 0 && event <= 15 && ( ( 1 << event ) & 0x59E4 ) )
	{
		TheBots->OnEvent( EVENT_CTS_WIN );
		if ( pRules )
			UTIL_LogPrintf( "Team \"%s\" triggered \"%s\" (CT \"%i\") (T \"%i\")\n", "CT", sentence + 1,
					pRules->m_iNumCTWins, pRules->m_iNumTerroristWins );
	}
	else
	{
		TheBots->OnEvent( EVENT_ROUND_DRAW );
		if ( pRules )
			UTIL_LogPrintf( "World triggered \"%s\" (CT \"%i\") (T \"%i\")\n", sentence + 1, pRules->m_iNumCTWins,
					pRules->m_iNumTerroristWins );
	}
	UTIL_LogPrintf( "World triggered \"Round_End\"\n" );
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerThink( CBasePlayer* pPlayer )
{
	if ( g_fGameOver )
	{
		if ( !IsCareer() && ( pPlayer->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) ) )
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}
	if ( !pPlayer->m_bCanShoot && !IsFreezePeriod() )
		pPlayer->m_bCanShoot = true;
	if ( pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->IsWeapon() &&
			( (CBasePlayerWeapon*)pPlayer->m_pActiveItem->GetWeaponPtr() )->m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
		pPlayer->m_bCanShoot = false;
	if ( pPlayer->m_iMenu == Menu_ChooseTeam || pPlayer->m_iJoiningState != SHOWTEAMSELECT )
		return;
	int team;
	if ( !stricmp( humans_join_team.string, "T" ) )
		team = TERRORIST;
	else if ( !stricmp( humans_join_team.string, "CT" ) )
		team = CT;
	else
	{
		if ( allow_spectators.value == 0 )
			ShowVGUIMenu( pPlayer, 2, 19, "#Team_Select" );
		else
			ShowVGUIMenu( pPlayer, 2, 51, "#Team_Select_Spect" );
		pPlayer->m_iMenu = Menu_ChooseTeam;
		pPlayer->m_iJoiningState = PICKINGTEAM;
		return;
	}
	pPlayer->m_iMenu = Menu_ChooseTeam;
	pPlayer->m_iJoiningState = PICKINGTEAM;
	if ( !pPlayer->IsBot() )
	{
		HandleMenu_ChooseTeam( pPlayer, team );
		if ( IsCareer() )
			HandleMenu_ChooseAppearance( pPlayer, 6 );
	}
}

bool CHalfLifeMultiplay::AddToVIPQueue( CBasePlayer* pPlayer )
{
	IsVIPQueueEmpty();
	if ( pPlayer->m_iTeam != CT )
		return false;
	int i;
	for ( i = 0; i < 5; ++i )
	{
		if ( VIPQueue[i] == pPlayer )
		{
			ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "#Game_in_position", UTIL_dtos1( i + 1 ) );
			return false;
		}
	}
	for ( i = 0; i < 5; ++i )
	{
		if ( !VIPQueue[i] )
			break;
	}
	if ( i == 5 )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "#All_VIP_Slots_Full" );
		return false;
	}
	VIPQueue[i] = pPlayer;
	StackVIPQueue();
	ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "#Game_added_position", UTIL_dtos1( i + 1 ) );
	return true;
}

void CHalfLifeMultiplay::ResetCurrentVIP()
{
	char* info = g_engfuncs.pfnGetInfoKeyBuffer( m_pVIP->edict() );
	const char* model;
	switch ( RANDOM_LONG( 0, UTIL_IsGame( "czero" ) ? 5 : 4 ) )
	{
	case 1:
		m_pVIP->m_iModelName = MODEL_GSG9;
		model = "gsg9";
		break;
	case 2:
		m_pVIP->m_iModelName = MODEL_SAS;
		model = "sas";
		break;
	case 3:
		m_pVIP->m_iModelName = MODEL_GIGN;
		model = "gign";
		break;
	case 4:
		if ( UTIL_IsGame( "czero" ) )
		{
			m_pVIP->m_iModelName = MODEL_SPETSNAZ;
			model = "spetsnaz";
			break;
		}
		[[fallthrough]];
	default:
		m_pVIP->m_iModelName = MODEL_URBAN;
		model = "urban";
		break;
	}
	SET_CLIENT_KEY_VALUE( m_pVIP->entindex(), info, "model", (char*)model );
	m_pVIP->m_bIsVIP = false;
	m_pVIP->m_bNotKilled = false;
}

void CHalfLifeMultiplay::BalanceTeams()
{
	int team;
	int count;
	if ( m_iMapHasVIPSafetyZone == 1 )
	{
		int total = m_iNumCT + m_iNumTerrorist;
		int desiredCT = ( total & 1 ) ? (int)( total * 0.55 ) + 1 : total / 2;
		if ( desiredCT > m_iNumCT )
		{
			team = TERRORIST;
			count = desiredCT - m_iNumCT;
		}
		else
		{
			int desiredT = total - desiredCT;
			if ( m_iNumTerrorist >= desiredT )
				return;
			team = CT;
			count = desiredT - m_iNumTerrorist;
		}
	}
	else if ( m_iNumCT > m_iNumTerrorist )
	{
		team = CT;
		count = ( m_iNumCT - m_iNumTerrorist ) / 2;
	}
	else if ( m_iNumTerrorist > m_iNumCT )
	{
		team = TERRORIST;
		count = ( m_iNumTerrorist - m_iNumCT ) / 2;
	}
	else
		return;
	if ( count > 4 )
		count = 4;
	for ( int i = 1; i <= count; ++i )
	{
		CBasePlayer* pSwitch = NULL;
		int highestUserID = 0;
		CBaseEntity* pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
		{
			if ( FNullEnt( pEntity->edict() ) )
				break;
			if ( pEntity->pev->flags == FL_DORMANT )
				continue;
			CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pEntity->pev );
			if ( pPlayer->m_iTeam == team && GETPLAYERUSERID( pPlayer->edict() ) > highestUserID && pPlayer != m_pVIP )
			{
				pSwitch = pPlayer;
				highestUserID = GETPLAYERUSERID( pPlayer->edict() );
			}
		}
		if ( pSwitch )
			pSwitch->SwitchTeam();
	}
}

void CHalfLifeMultiplay::GoToIntermission()
{
	if ( g_fGameOver )
		return;  // intermission has already been triggered, so ignore.
	CHalfLifeMultiplay* pRules = (CHalfLifeMultiplay*)g_pGameRules;
	if ( pRules )
	{
		UTIL_LogPrintf( "Team \"CT\" scored \"%i\" with \"%i\" players\n", pRules->m_iNumCTWins, pRules->m_iNumCT );
		UTIL_LogPrintf( "Team \"TERRORIST\" scored \"%i\" with \"%i\" players\n", pRules->m_iNumTerroristWins,
				pRules->m_iNumTerrorist );
	}
	if ( IsCareer() )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
			WRITE_STRING( "MATCH" );
			WRITE_LONG( m_iNumCTWins );
			WRITE_LONG( m_iNumTerroristWins );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ALL, gmsgCZCareerHUD );
			WRITE_STRING( "MATCH" );
			WRITE_LONG( m_iNumCTWins );
			WRITE_LONG( m_iNumTerroristWins );
			WRITE_BYTE( m_iCareerMatchWins );
			WRITE_BYTE( m_iRoundWinDifference );
			WRITE_BYTE( m_iRoundWinStatus );
		MESSAGE_END();
		UTIL_LogPrintf( "Career Match %d %d %d %d\n", m_iRoundWinStatus, m_iNumCTWins, m_iNumTerroristWins,
				TheCareerTasks->AreAllTasksComplete() );
	}
	MESSAGE_BEGIN( MSG_ALL, SVC_INTERMISSION );
	MESSAGE_END();
	if ( IsCareer() )
		SERVER_COMMAND( "setpause\n" );
	int chatTime = (int)CVAR_GET_FLOAT( "mp_chattime" );
	if ( chatTime <= 0 )
		CVAR_SET_STRING( "mp_chattime", "1" );
	else if ( chatTime > 120 )
		CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( 120 ) );
	m_flIntermissionEndTime = (int)mp_chattime.value + gpGlobals->time;
	m_iEndIntermissionButtonHit = FALSE;
	m_iSpawnPointCount_Terrorist = 0;
	m_flIntermissionStartTime = gpGlobals->time;
	m_iSpawnPointCount_CT = 0;
	m_bLevelInitialized = false;
	g_fGameOver = TRUE;
}

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s* next;
	char mapname[32];
	int minplayers;
	int maxplayers;
	char rulebuffer[1024];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	mapcycle_item_t* items;
	mapcycle_item_t* next_item;
} mapcycle_t;

static char mp_com_token[1500];

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle( mapcycle_t* cycle )
{
	mapcycle_item_t* start = cycle->items;
	if ( start )
	{
		mapcycle_item_t* item = start->next;
		while ( item != start )
		{
			mapcycle_item_t* next = item->next;
			delete item;
			item = next;
		}
		delete cycle->items;
	}
	cycle->items = NULL;
	cycle->next_item = NULL;
}

char* MP_COM_GetToken()
{
	return mp_com_token;
}

char* MP_COM_Parse( char* data )
{
	mp_com_token[0] = 0;
	if ( !data )
		return NULL;
	int c;
	for ( ;; )
	{
		while ( ( c = *data ) <= ' ' )
		{
			if ( !c )
				return NULL;
			++data;
		}
		if ( c != '/' || data[1] != '/' )
			break;
		while ( *data && *data != '\n' )
			++data;
	}
	int length = 0;
	if ( c == '"' )
	{
		++data;
		for ( ;; )
		{
			c = *data++;
			if ( c == '"' || !c )
			{
				mp_com_token[length] = 0;
				return data;
			}
			mp_com_token[length++] = (char)c;
		}
	}
	if ( c == '{' || c == '}' || c == '(' || c == ')' || c == '\'' || c == ',' )
	{
		mp_com_token[0] = (char)c;
		mp_com_token[1] = 0;
		return data + 1;
	}
	do
	{
		mp_com_token[length++] = (char)c;
		c = *++data;
		if ( c == '{' || c == '}' || c == '(' || c == ')' || c == '\'' || c == ',' )
			break;
	} while ( c > ' ' );
	mp_com_token[length] = 0;
	return data;
}

int MP_COM_TokenWaiting( char* buffer )
{
	while ( *buffer && *buffer != '\n' )
	{
		if ( !isspace( *buffer ) || isalnum( *buffer ) )
			return 1;
		++buffer;
	}
	return 0;
}

int ReloadMapCycleFile( char* filename, mapcycle_t* cycle )
{
	int length;
	char* file = (char*)LOAD_FILE_FOR_ME( filename, &length );
	if ( file && length )
	{
		char* text = file;
		for ( ;; )
		{
			char rules[1024];
			memset( rules, 0, sizeof( rules ) );
			text = MP_COM_Parse( text );
			if ( !mp_com_token[0] )
				break;
			char map[32];
			strncpy( map, mp_com_token, sizeof( map ) );
			map[sizeof( map ) - 1] = 0;
			bool hasRules = false;
			if ( MP_COM_TokenWaiting( text ) )
			{
				text = MP_COM_Parse( text );
				if ( mp_com_token[0] )
				{
					hasRules = true;
					strncpy( rules, mp_com_token, sizeof( rules ) );
					rules[sizeof( rules ) - 1] = 0;
				}
			}
			if ( !IS_MAP_VALID( map ) )
			{
				ALERT( at_console, "Skipping %s from mapcycle, not a valid map\n", map );
				continue;
			}
			mapcycle_item_t* item = new mapcycle_item_t;
			strcpy( item->mapname, map );
			item->minplayers = 0;
			item->maxplayers = 0;
			memset( item->rulebuffer, 0, sizeof( item->rulebuffer ) );
			if ( hasRules )
			{
				const char* value = g_engfuncs.pfnInfoKeyValue( rules, "minplayers" );
				if ( value && *value )
				{
					item->minplayers = atoi( value );
					if ( item->minplayers < 0 )
						item->minplayers = 0;
					if ( item->minplayers > gpGlobals->maxClients )
						item->minplayers = gpGlobals->maxClients;
				}
				value = g_engfuncs.pfnInfoKeyValue( rules, "maxplayers" );
				if ( value && *value )
				{
					item->maxplayers = atoi( value );
					if ( item->maxplayers < 0 )
						item->maxplayers = 0;
					if ( item->maxplayers > gpGlobals->maxClients )
						item->maxplayers = gpGlobals->maxClients;
				}
				g_engfuncs.pfnInfo_RemoveKey( rules, "minplayers" );
				g_engfuncs.pfnInfo_RemoveKey( rules, "maxplayers" );
				strcpy( item->rulebuffer, rules );
			}
			item->next = cycle->items;
			cycle->items = item;
		}
		FREE_FILE( file );
	}
	mapcycle_item_t* item = cycle->items;
	mapcycle_item_t* reversed = NULL;
	while ( item )
	{
		mapcycle_item_t* next = item->next;
		item->next = reversed;
		reversed = item;
		item = next;
	}
	cycle->items = reversed;
	if ( !reversed )
		return 0;
	item = reversed;
	while ( item->next )
		item = item->next;
	item->next = reversed;
	cycle->next_item = reversed;
	return 1;
}

int GetMapCount()
{
	static mapcycle_t mapcycle2;
	char* filename = (char*)CVAR_GET_STRING( "mapcyclefile" );
	DestroyMapCycle( &mapcycle2 );
	ReloadMapCycleFile( filename, &mapcycle2 );
	if ( !mapcycle2.next_item )
		return 0;
	int count = 0;
	mapcycle_item_t* item = mapcycle2.next_item;
	do
	{
		item = item->next;
		++count;
	} while ( item && item != mapcycle2.next_item );
	return count;
}

void ExtractCommandString( char* text, char* command )
{
	if ( *text == '\\' )
		++text;
	for ( ;; )
	{
		char key[512];
		char value[512];	// use two buffers so compares
		char* out = key;
		while ( *text != '\\' )
		{
			if ( !*text )
				return;
			*out++ = *text++;
		}
		*out = 0;
		++text;
		out = value;
		while ( *text && *text != '\\' )
			*out++ = *text++;
		*out = 0;
		strcat( command, key );
		if ( value[0] )
		{
			strcat( command, " " );
			strcat( command, value );
		}
		strcat( command, "\n" );
		if ( !*text )
			return;
		++text;
	}
}

void CHalfLifeMultiplay::DisplayMaps( CBasePlayer* pPlayer, int vote )
{
	static mapcycle_t mapcycle2;
	char* filename = (char*)CVAR_GET_STRING( "mapcyclefile" );
	DestroyMapCycle( &mapcycle2 );
	ReloadMapCycleFile( filename, &mapcycle2 );
	mapcycle_item_t* item = mapcycle2.next_item;
	if ( !item )
		return;
	char* selected = NULL;
	int index = 0;
	do
	{
		++index;
		if ( pPlayer )
		{
			const char* count = UTIL_dtos2( m_iMapVotes[index - 1] );
			const char* number = UTIL_dtos1( index );
			ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, m_iMapVotes[index - 1] == 1 ? "#Vote" : "#Votes", number,
					item->mapname, count );
		}
		if ( index == vote )
			selected = item->mapname;
		item = item->next;
	} while ( item && item != mapcycle2.next_item );
	if ( selected && vote )
	{
		if ( !strcmp( selected, STRING( gpGlobals->mapname ) ) )
		{
			if ( timelimit.value != 0 )
			{
				timelimit.value += 30;
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "#Map_Vote_Extend" );
			}
			ResetAllMapVotes();
		}
		else
			CHANGE_LEVEL( selected, NULL );
	}
}

void CHalfLifeMultiplay::ProcessMapVote( CBasePlayer* pPlayer, int vote )
{
	int players = 0;
	int votes = 0;
	CBaseEntity* pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
	{
		if ( FNullEnt( pEntity->edict() ) )
			break;
		CBasePlayer* pVoter = GetClassPtr( (CBasePlayer*)pEntity->pev );
		if ( pVoter->m_iTeam != UNASSIGNED )
		{
			++players;
			if ( pVoter->m_iMapVote == vote )
				++votes;
		}
	}
	m_iMapVotes[vote - 1] = votes;
	float ratio = mapvoteratio.value;
	if ( ratio > 1 )
	{
		CVAR_SET_STRING( "mp_mapvoteratio", "1.0" );
		ratio = 1;
	}
	else if ( ratio < 0.35 )
	{
		CVAR_SET_STRING( "mp_mapvoteratio", "0.35" );
		ratio = 0.35f;
	}
	int required = players > 2 ? (int)( players * ratio + 0.5 ) : 2;
	if ( votes >= required )
		DisplayMaps( NULL, vote );
	else
	{
		DisplayMaps( pPlayer, 0 );
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "#Game_required_votes", UTIL_dtos1( required ) );
	}
}

bool CHalfLifeMultiplay::HasRoundTimeExpired()
{
	if ( m_iRoundTimeSecs - gpGlobals->time + m_fRoundCount > 0 || m_iRoundWinStatus )
		return false;
	if ( m_bMapHasBombTarget )
	{
		CBaseEntity* pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "grenade" ) ) != NULL )
		{
			if ( ( (CGrenade*)pEntity )->m_bIsC4 )
				return false;
		}
	}
	if ( cv_bot_nav_edit.value == 0 || IS_DEDICATED_SERVER() )
		return true;
	return UTIL_HumansInGame( false ) != 1;
}

void CHalfLifeMultiplay::PickNextVIP()
{
	if ( !IsVIPQueueEmpty() )
	{
		if ( m_pVIP )
			ResetCurrentVIP();
		for ( int i = 0; i < 5; ++i )
		{
			if ( VIPQueue[i] )
			{
				m_pVIP = VIPQueue[i];
				m_pVIP->MakeVIP();
				VIPQueue[i] = NULL;
				StackVIPQueue();
				m_iConsecutiveVIP = 0;
				return;
			}
		}
		return;
	}
	CBaseEntity* pEntity = NULL;
	if ( m_iConsecutiveVIP > 2 )
	{
		if ( ++m_iLastPick > m_iNumCT )
			m_iLastPick = 1;
		CBasePlayer* pPrevious = NULL;
		int index = 1;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
		{
			if ( FNullEnt( pEntity->edict() ) )
				break;
			if ( pEntity->pev->flags & FL_DORMANT )
				continue;
			CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pEntity->pev );
			if ( pPlayer->m_iTeam == CT )
			{
				if ( index == m_iLastPick )
				{
					if ( m_pVIP != pPlayer || !pPrevious )
						pPrevious = pPlayer;
					if ( m_pVIP )
						ResetCurrentVIP();
					pPrevious->MakeVIP();
					m_iConsecutiveVIP = 0;
					return;
				}
				++index;
				pPrevious = pPlayer;
			}
			else if ( pPlayer->m_iTeam != SPECTATOR )
				pPrevious = pPlayer;
		}
	}
	else if ( !m_pVIP )
	{
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
		{
			if ( FNullEnt( pEntity->edict() ) )
				break;
			if ( pEntity->pev->flags == FL_DORMANT )
				continue;
			CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pEntity->pev );
			if ( pPlayer->m_iTeam == CT )
			{
				pPlayer->MakeVIP();
				m_iConsecutiveVIP = 0;
				return;
			}
		}
	}
}

void CHalfLifeMultiplay::CleanUpMap()
{
	const char* restartClasses[] = { "cycler_sprite", "light", "func_breakable", "func_door", "func_water",
			"func_door_rotating", "func_tracktrain", "func_vehicle", "func_train", "armoury_entity", "ambient_generic",
			"env_sprite" };
	for ( int i = 0; i < ARRAYSIZE( restartClasses ); ++i )
	{
		CBaseEntity* pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, restartClasses[i] ) ) != NULL )
			pEntity->Restart();
	}
	CBaseEntity* pEntity = UTIL_FindEntityByClassname( NULL, "grenade" );
	int grenades = 0;
	while ( pEntity )
	{
		++grenades;
		UTIL_Remove( pEntity );
		pEntity = UTIL_FindEntityByClassname( pEntity, "grenade" );
		if ( grenades > 19 )
			break;
	}
	pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "item_thighpack" ) ) != NULL )
		UTIL_Remove( pEntity );
	RemoveGuns();
	PLAYBACK_EVENT_FULL( FEV_GLOBAL | FEV_RELIABLE, NULL, m_usResetDecals, 0, (float*)&g_vecZero.x,
			(float*)&g_vecZero.x, 0, 0, 0, 0, 0, 0 );
}

void CHalfLifeMultiplay::GiveC4()
{
	int teamCount = m_iNumTerrorist;
	++m_iC4Guy;
	bool humansOnly = false;
	if ( cv_bot_defer_to_human.value > 0 )
	{
		int humans = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
			if ( pPlayer && !FNullEnt( pPlayer->edict() ) && pPlayer->pev->deadflag == DEAD_NO &&
					pPlayer->m_iTeam == TERRORIST && !pPlayer->IsBot() )
				++humans;
		}
		if ( humans )
		{
			teamCount = humans;
			humansOnly = true;
		}
	}
	if ( m_iC4Guy > teamCount )
		m_iC4Guy = 1;
	CBaseEntity* pEntity = NULL;
	int index = 0;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
	{
		if ( FNullEnt( pEntity->edict() ) )
			break;
		if ( !pEntity->IsPlayer() || pEntity->pev->flags == FL_DORMANT )
			continue;
		CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pEntity->pev );
		if ( pPlayer->pev->deadflag != DEAD_NO || pPlayer->m_iTeam != TERRORIST || ( humansOnly && pPlayer->IsBot() ) )
			continue;
		if ( ++index == m_iC4Guy )
		{
			pPlayer->m_bHasC4 = true;
			pPlayer->GiveNamedItem( "weapon_c4" );
			pPlayer->SetBombIcon( false );
			pPlayer->pev->body = 1;
			pPlayer->m_flDisplayHistory |= 0x1000;
			pPlayer->HintMessage( "#Hint_you_have_the_bomb", false, true );
			UTIL_LogPrintf( "\"%s<%i><%s><TERRORIST>\" triggered \"Spawned_With_The_Bomb\"\n",
					STRING( pPlayer->pev->netname ), GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ) );
			g_pGameRules->m_bBombDropped = FALSE;
		}
	}
	if ( IsThereABomber() )
		return;
	m_iC4Guy = 0;
	pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
	{
		if ( FNullEnt( pEntity->edict() ) )
			break;
		if ( !pEntity->IsPlayer() || pEntity->pev->flags == FL_DORMANT )
			continue;
		CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pEntity->pev );
		if ( pPlayer->pev->deadflag == DEAD_NO && pPlayer->m_iTeam == TERRORIST )
		{
			pPlayer->m_bHasC4 = true;
			pPlayer->GiveNamedItem( "weapon_c4" );
			pPlayer->SetBombIcon( false );
			pPlayer->pev->body = 1;
			pPlayer->m_flDisplayHistory |= 0x1000;
			pPlayer->HintMessage( "#Hint_you_have_the_bomb", false, true );
			UTIL_LogPrintf( "\"%s<%i><%s><TERRORIST>\" triggered \"Spawned_With_The_Bomb\"\n",
					STRING( pPlayer->pev->netname ), GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ) );
			g_pGameRules->m_bBombDropped = FALSE;
			return;
		}
	}
}

void CHalfLifeMultiplay::ClientDisconnected( edict_t* client )
{
	CBasePlayer* pPlayer = client ? (CBasePlayer*)GET_PRIVATE( client ) : NULL;
	if ( pPlayer )
	{
		pPlayer->has_disconnected = true;
		pPlayer->pev->deadflag = DEAD_DEAD;
		pPlayer->SetScoreboardAttributes( NULL );
		if ( pPlayer->m_bHasC4 )
			pPlayer->DropPlayerItem( "weapon_c4" );
		if ( pPlayer->m_bHasDefuser )
			pPlayer->DropPlayerItem( "item_thighpack" );
		if ( pPlayer->m_bIsVIP )
			m_pVIP = NULL;
		pPlayer->m_iCurrentKickVote = 0;
		if ( pPlayer->m_iMapVote )
		{
			int& votes = m_iMapVotes[pPlayer->m_iMapVote - 1];
			if ( --votes < 0 )
				votes = 0;
		}
		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX( client ) );
			WRITE_SHORT( 0 );
			WRITE_SHORT( 0 );
			WRITE_SHORT( 0 );
			WRITE_SHORT( 0 );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
			WRITE_BYTE( ENTINDEX( client ) );
			WRITE_STRING( "UNASSIGNED" );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ALL, gmsgLocation );
			WRITE_BYTE( ENTINDEX( client ) );
			WRITE_STRING( "" );
		MESSAGE_END();
		FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected\n", STRING( pPlayer->pev->netname ),
				GETPLAYERUSERID( pPlayer->edict() ), GETPLAYERAUTHID( pPlayer->edict() ), GetTeam( pPlayer->m_iTeam ) );
		pPlayer->RemoveAllItems( TRUE ); // destroy all of the players weapons and items
		if ( pPlayer->m_pObserver )
			pPlayer->m_pObserver->SUB_Remove();
		CBaseEntity* pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
		{
			if ( FNullEnt( pEntity->edict() ) )
				break;
			CBasePlayer* pObserver = (CBasePlayer*)pEntity;
			if ( pObserver->pev && pObserver != pPlayer && (CBaseEntity*)pObserver->m_hObserverTarget == pPlayer )
			{
				int mode = pObserver->pev->iuser1;
				pObserver->pev->iuser1 = 0;
				pObserver->Observer_SetMode( mode );
			}
		}
	}
	CheckWinConditions();
}

void CHalfLifeMultiplay::QueueCareerRoundEndMenu( float delay, int winner )
{
	if ( m_fCareerMatchMenuTime != 0 )
		return;
	m_fCareerRoundMenuTime = gpGlobals->time + delay;
	bool humansAreTerrorists = strcmp( humans_join_team.string, "CT" ) != 0;
	if ( !humansAreTerrorists )
	{
		int total = 0;
		int alive = 0;
		int followingHumans = 0;
		CBaseEntity* pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "hostage_entity" ) ) != NULL )
		{
			++total;
			if ( pEntity->pev->takedamage != DAMAGE_YES )
				continue;
			CHostage* pHostage = (CHostage*)pEntity;
			CBasePlayer* pLeader = pHostage->IsFollowingSomeone() ? (CBasePlayer*)pHostage->GetLeader() : NULL;
			if ( pHostage->pev->takedamage != DAMAGE_YES )
				continue;
			if ( pHostage->IsFollowingSomeone() && pHostage->pev->takedamage == DAMAGE_YES && pLeader &&
					!pLeader->IsBot() )
			{
				++followingHumans;
				TheCareerTasks->HandleEvent( EVENT_HOSTAGE_RESCUED, pLeader, NULL );
			}
			else if ( pHostage->pev->takedamage == DAMAGE_YES )
				++alive;
		}
		if ( !alive && m_iHostagesRescued + followingHumans >= total * 0.5 )
			TheCareerTasks->HandleEvent( EVENT_ALL_HOSTAGES_RESCUED, NULL, NULL );
	}
	if ( winner == 1 )
		TheCareerTasks->HandleEvent( humansAreTerrorists ? EVENT_ROUND_LOSS : EVENT_ROUND_WIN, NULL, NULL );
	else if ( winner == 2 )
		TheCareerTasks->HandleEvent( humansAreTerrorists ? EVENT_ROUND_WIN : EVENT_ROUND_LOSS, NULL, NULL );
	else
		TheCareerTasks->HandleEvent( EVENT_ROUND_DRAW, NULL, NULL );
	if ( m_fCareerMatchMenuTime == 0 && m_iCareerMatchWins )
	{
		bool canCTsWin =
				m_iNumCTWins >= m_iCareerMatchWins && m_iNumCTWins - m_iNumTerroristWins >= m_iRoundWinDifference;
		bool canTerroristsWin = m_iNumTerroristWins >= m_iCareerMatchWins &&
								m_iNumTerroristWins - m_iNumCTWins >= m_iRoundWinDifference;
		bool complete = TheCareerTasks->AreAllTasksComplete();
		if ( !( canTerroristsWin && ( complete || !humansAreTerrorists ) ) &&
				!( canCTsWin && ( complete || humansAreTerrorists ) ) )
			return;
		m_fCareerRoundMenuTime = 0;
		m_fCareerMatchMenuTime = gpGlobals->time + 3;
	}
}

void CHalfLifeMultiplay::RestartRound()
{
	TheBots->RestartRound();
	if ( g_pHostages )
		g_pHostages->RestartRound();
	++m_iTotalRoundsPlayed;
	ClearBodyQue();
	CVAR_SET_FLOAT( "sv_accelerate", 5 );
	CVAR_SET_FLOAT( "sv_friction", 4 );
	CVAR_SET_FLOAT( "sv_stopspeed", 75 );
	m_iNumCT = CountTeamPlayers( CT );
	m_iNumTerrorist = CountTeamPlayers( TERRORIST );
	if ( m_bMapHasBombTarget )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgBombPickup );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ALL, gmsgShowTimer );
		MESSAGE_END();
	}
	g_pGameRules->m_bBombDropped = FALSE;
	MESSAGE_BEGIN( MSG_SPEC, gmsgHLTV );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 228 );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_SPEC, gmsgHLTV );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
	MESSAGE_END();
	if ( CVAR_GET_FLOAT( "mp_autoteambalance" ) != 0 && m_iUnBalancedRounds > 0 )
		BalanceTeams();
	if ( m_iNumCT - m_iNumTerrorist > 1 || m_iNumTerrorist - m_iNumCT > 1 )
		++m_iUnBalancedRounds;
	else
		m_iUnBalancedRounds = 0;
	if ( CVAR_GET_FLOAT( "mp_autoteambalance" ) != 0 && m_iUnBalancedRounds == 1 )
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "#Auto_Team_Balance_Next_Round" );
	if ( m_bCompleteReset )
	{
		if ( timelimit.value < 0 )
			CVAR_SET_FLOAT( "mp_timelimit", 0 );
		g_flResetTime = gpGlobals->time;
		if ( timelimit.value != 0 )
			g_flTimeLimit = timelimit.value * 60 + gpGlobals->time;
		m_iTotalRoundsPlayed = 0;
		m_iMaxRounds = (int)CVAR_GET_FLOAT( "mp_maxrounds" );
		if ( m_iMaxRounds < 0 )
		{
			m_iMaxRounds = 0;
			CVAR_SET_FLOAT( "mp_maxrounds", 0 );
		}
		m_iMaxRoundsWon = (int)CVAR_GET_FLOAT( "mp_winlimit" );
		if ( m_iMaxRoundsWon < 0 )
		{
			m_iMaxRoundsWon = 0;
			CVAR_SET_FLOAT( "mp_winlimit", 0 );
		}
		m_iNumConsecutiveCTLoses = 0;
		m_iNumTerroristWins = 0;
		m_iNumCTWins = 0;
		m_iNumConsecutiveTerroristLoses = 0;
		UpdateTeamScores();
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->pev && !FNullEnt( pPlayer->pev ) )
				pPlayer->Reset();
		}
		TheBots->OnEvent( EVENT_NEW_MATCH );
	}
	m_bFreezePeriod = TRUE;
	m_bRoundTerminating = false;
	ReadMultiplayCvars( this );
	m_fMaxIdlePeriod = (float)( 2 * m_iRoundTime );
	float idleTimeout = CVAR_GET_FLOAT( "mp_autokick_timeout" );
	if ( idleTimeout > 0 )
		m_fMaxIdlePeriod = idleTimeout;
	m_iRoundTimeSecs = m_iIntroRoundTime;
	CMapInfo* pMapInfo = (CMapInfo*)UTIL_FindEntityByClassname( NULL, "info_map_parameters" );
	if ( pMapInfo )
	{
		switch ( pMapInfo->m_iBuyingStatus )
		{
		case 0:
			m_bCTCantBuy = false;
			m_bTCantBuy = false;
			ALERT( at_console, "EVERYONE CAN BUY!\n" );
			break;
		case 1:
			m_bCTCantBuy = false;
			m_bTCantBuy = true;
			ALERT( at_console, "Only CT's can buy!!\n" );
			break;
		case 2:
			m_bCTCantBuy = true;
			m_bTCantBuy = false;
			ALERT( at_console, "Only T's can buy!!\n" );
			break;
		case 3:
			m_bCTCantBuy = true;
			m_bTCantBuy = true;
			ALERT( at_console, "No one can buy!!\n" );
			break;
		default:
			m_bCTCantBuy = false;
			m_bTCantBuy = false;
			break;
		}
		m_flBombRadius = pMapInfo->m_flBombRadius;
	}
	if ( UTIL_FindEntityByClassname( NULL, "func_bomb_target" ) )
	{
		m_bMapHasBombTarget = true;
		m_bMapHasBombZone = true;
	}
	else
	{
		m_bMapHasBombTarget = UTIL_FindEntityByClassname( NULL, "info_bomb_target" ) != NULL;
		m_bMapHasBombZone = false;
	}
	m_bMapHasRescueZone = UTIL_FindEntityByClassname( NULL, "func_hostage_rescue" ) != NULL;
	m_bMapHasBuyZone = UTIL_FindEntityByClassname( NULL, "func_buyzone" ) != NULL;
	if ( UTIL_FindEntityByClassname( NULL, "func_escapezone" ) )
	{
		m_iNumEscapers = 0;
		m_bMapHasEscapeZone = true;
		m_iHaveEscaped = 0;
		if ( m_iNumEscapeRounds > 2 )
		{
			SwapAllPlayers();
			m_iNumEscapeRounds = 1;
		}
		else
			++m_iNumEscapeRounds;
	}
	else
		m_bMapHasEscapeZone = false;
	if ( UTIL_FindEntityByClassname( NULL, "func_vip_safetyzone" ) )
	{
		PickNextVIP();
		++m_iConsecutiveVIP;
		m_iMapHasVIPSafetyZone = 1;
	}
	else
		m_iMapHasVIPSafetyZone = 2;
	int hostageBonus = 0;
	CHostage* pHostage = (CHostage*)UTIL_FindEntityByClassname( NULL, "hostage_entity" );
	while ( pHostage )
	{
		if ( pHostage->pev->solid != SOLID_NOT )
		{
			hostageBonus += 150;
			if ( pHostage->pev->deadflag == DEAD_DEAD )
				pHostage->pev->deadflag = DEAD_RESPAWNABLE;
		}
		pHostage->RePosition();
		pHostage = (CHostage*)UTIL_FindEntityByClassname( pHostage, "hostage_entity" );
		if ( hostageBonus > 1999 )
			break;
	}
	if ( m_iRoundWinStatus == 2 )
	{
		if ( m_iNumConsecutiveTerroristLoses > 1 )
			m_iLoserBonus = 1500;
		m_iNumConsecutiveTerroristLoses = 0;
		++m_iNumConsecutiveCTLoses;
	}
	else if ( m_iRoundWinStatus == 1 )
	{
		if ( m_iNumConsecutiveCTLoses > 1 )
			m_iLoserBonus = 1500;
		m_iNumConsecutiveCTLoses = 0;
		++m_iNumConsecutiveTerroristLoses;
	}
	if ( ( m_iNumConsecutiveTerroristLoses > 1 || m_iNumConsecutiveCTLoses > 1 ) && m_iLoserBonus <= 2999 )
		m_iLoserBonus += 500;
	if ( m_iRoundWinStatus == 2 )
	{
		m_iAccountTerrorist += hostageBonus;
		m_iAccountCT += m_iLoserBonus;
	}
	else if ( m_iRoundWinStatus == 1 )
	{
		m_iAccountCT += hostageBonus;
		if ( !m_bMapHasEscapeZone )
			m_iAccountTerrorist += m_iLoserBonus;
	}
	m_iAccountCT += 750 * m_iHostagesRescued;
	m_fRoundCount = gpGlobals->time;
	m_fIntroRoundCount = gpGlobals->time;
	if ( m_bCompleteReset )
	{
		m_iAccountCT = 0;
		m_iAccountTerrorist = 0;
		m_iNumTerroristWins = 0;
		m_iNumCTWins = 0;
		m_iNumConsecutiveTerroristLoses = 0;
		m_iNumConsecutiveCTLoses = 0;
		m_iLoserBonus = 1400;
	}
	CBaseEntity* pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
	{
		if ( FNullEnt( pEntity->edict() ) )
			break;
		if ( pEntity->pev->flags == FL_DORMANT )
			continue;
		CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pEntity->pev );
		pPlayer->m_iNumSpawns = 0;
		pPlayer->m_bTeamChanged = false;
		if ( !pPlayer->IsPlayer() )
			pPlayer->SyncRoundTimer();
		if ( pPlayer->m_iTeam == CT )
		{
			if ( !pPlayer->m_bReceivesNoMoneyNextRound )
				pPlayer->AddAccount( m_iAccountCT, true );
		}
		else if ( pPlayer->m_iTeam == TERRORIST )
		{
			++m_iNumEscapers;
			if ( !pPlayer->m_bReceivesNoMoneyNextRound )
				pPlayer->AddAccount( m_iAccountTerrorist, true );
			if ( m_bMapHasEscapeZone )
				pPlayer->m_bNotKilled = false;
		}
		if ( pPlayer->m_iTeam != UNASSIGNED && pPlayer->m_iTeam != SPECTATOR )
		{
			if ( pPlayer->m_bHasC4 )
				pPlayer->DropPlayerItem( "weapon_c4" );
			pPlayer->RoundRespawn();
		}
	}
	CleanUpMap();
	if ( m_bMapHasBombTarget )
		GiveC4();
	TheBots->OnEvent( EVENT_BUY_TIME_START );
	m_flIntermissionEndTime = 0;
	m_flIntermissionStartTime = 0;
	m_fTeamCount = 0;
	m_iAccountCT = 0;
	m_iAccountTerrorist = 0;
	m_iHostagesRescued = 0;
	m_iHostagesTouched = 0;
	m_iRoundWinStatus = 0;
	m_bBombDefused = false;
	m_bTargetBombed = false;
	m_bLevelInitialized = false;
	m_bCompleteReset = false;
}

void CHalfLifeMultiplay::CheckWinConditions()
{
	if ( m_bFirstConnected && m_iRoundWinStatus )
		return;
	m_iNumSpawnableCT = 0;
	m_iNumSpawnableTerrorist = 0;
	m_iNumCT = 0;
	m_iNumTerrorist = 0;
	m_iHaveEscaped = 0;
	int aliveTerrorists = 0;
	int aliveCTs = 0;
	int deadTerrorists = 0;
	int deadCTs = 0;
	CBaseEntity* pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "player" ) ) != NULL )
	{
		if ( FNullEnt( pEntity->edict() ) )
			break;
		CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pEntity->pev );
		if ( pEntity->pev->flags == FL_DORMANT )
			continue;
		if ( pPlayer->m_iTeam == TERRORIST )
		{
			++m_iNumTerrorist;
			if ( pPlayer->m_iMenu != Menu_ChooseAppearance )
			{
				++m_iNumSpawnableTerrorist;
				pPlayer->IsBot();
			}
			pPlayer->IsBot();
			if ( pPlayer->pev->deadflag != DEAD_NO )
				++deadTerrorists;
			else
				++aliveTerrorists;
			if ( pPlayer->m_bEscaped )
				++m_iHaveEscaped;
		}
		else if ( pPlayer->m_iTeam == CT )
		{
			++m_iNumCT;
			if ( pPlayer->m_iMenu != Menu_ChooseAppearance )
			{
				++m_iNumSpawnableCT;
				pPlayer->IsBot();
			}
			pPlayer->IsBot();
			if ( pPlayer->pev->deadflag != DEAD_NO )
				++deadCTs;
			else
				++aliveCTs;
		}
	}
	bool neededPlayers = !m_iNumSpawnableTerrorist || !m_iNumSpawnableCT;
	if ( neededPlayers )
	{
		UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#Game_scoring" );
		m_bFirstConnected = false;
	}
	else if ( !m_bFirstConnected )
	{
		CBasePlayer* pLastPlayer;
		if ( !IsCareer() || ( ( pLastPlayer = (CBasePlayer*)UTIL_PlayerByIndex( gpGlobals->maxClients ) ) != NULL &&
									pLastPlayer->IsBot() ) )
		{
			UTIL_LogPrintf( "World triggered \"Game_Commencing\"\n" );
			m_bFreezePeriod = FALSE;
			m_bCompleteReset = true;
			EndRoundMessage( "#Game_Commencing", 10 );
			TerminateRound( IsCareer() ? 0.0f : 3.0f, 3 );
			m_bFirstConnected = true;
			TheBots->OnEvent( EVENT_GAME_COMMENCE );
		}
		return;
	}
	if ( m_iMapHasVIPSafetyZone == 1 && m_pVIP )
	{
		if ( m_pVIP->m_bEscaped )
		{
			Broadcast( "ctwin" );
			m_iAccountCT += 3500;
			if ( !neededPlayers )
			{
				++m_iNumCTWins;
				UpdateTeamScores();
			}
			MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
				WRITE_BYTE( 9 );
				WRITE_BYTE( DRC_CMD_EVENT );
				WRITE_SHORT( m_pVIP->entindex() );
				WRITE_SHORT( 0 );
				WRITE_LONG( DRC_FLAG_FINAL | DRC_FLAG_PRIO_MASK );
			MESSAGE_END();
			EndRoundMessage( "#VIP_Escaped", 2 );
			TheBots->OnEvent( EVENT_VIP_ESCAPED );
			TerminateRound( 5, 1 );
			return;
		}
		if ( m_pVIP->pev->deadflag != DEAD_NO )
		{
			Broadcast( "terwin" );
			m_iAccountTerrorist += 3250;
			if ( !neededPlayers )
			{
				++m_iNumTerroristWins;
				UpdateTeamScores();
			}
			EndRoundMessage( "#VIP_Assassinated", 3 );
			TheBots->OnEvent( EVENT_VIP_ASSASSINATED );
			TerminateRound( 5, 2 );
			return;
		}
	}
	if ( m_bMapHasEscapeZone )
	{
		float escaped = (float)m_iHaveEscaped / (float)m_iNumEscapers;
		if ( escaped >= m_flRequiredEscapeRatio )
		{
			Broadcast( "terwin" );
			m_iAccountTerrorist += 3150;
			if ( !neededPlayers )
			{
				++m_iNumTerroristWins;
				UpdateTeamScores();
			}
			EndRoundMessage( "#Terrorists_Escaped", 4 );
			TerminateRound( 5, 2 );
			return;
		}
		if ( !aliveTerrorists )
		{
			if ( m_flRequiredEscapeRatio > escaped )
			{
				Broadcast( "ctwin" );
				m_iAccountCT = (int)( ( 1.0f - escaped ) * 3500.0f + m_iAccountCT );
				if ( !neededPlayers )
				{
					++m_iNumCTWins;
					UpdateTeamScores();
				}
				EndRoundMessage( "#CTs_PreventEscape", 5 );
				TerminateRound( 5, 1 );
				return;
			}
			if ( deadTerrorists && m_iNumSpawnableCT > 0 )
			{
				Broadcast( "ctwin" );
				m_iAccountCT = (int)( ( 1.0f - escaped ) * 3250.0f + m_iAccountCT );
				if ( !neededPlayers )
				{
					++m_iNumCTWins;
					UpdateTeamScores();
				}
				EndRoundMessage( "#Escaping_Terrorists_Neutralized", 6 );
				TerminateRound( 5, 1 );
				return;
			}
		}
	}
	if ( m_bTargetBombed && m_bMapHasBombTarget )
	{
		Broadcast( "terwin" );
		m_iAccountTerrorist += 3500;
		if ( !neededPlayers )
		{
			++m_iNumTerroristWins;
			UpdateTeamScores();
		}
		EndRoundMessage( "#Target_Bombed", 1 );
		TerminateRound( 5, 2 );
		return;
	}
	if ( !m_bTargetBombed && m_bBombDefused && m_bMapHasBombTarget )
	{
		Broadcast( "ctwin" );
		m_iAccountCT += 3250;
		m_iAccountTerrorist += 800;
		if ( !neededPlayers )
		{
			++m_iNumCTWins;
			UpdateTeamScores();
		}
		EndRoundMessage( "#Bomb_Defused", 7 );
		TerminateRound( 5, 1 );
		return;
	}
	if ( m_iNumCT > 0 && m_iNumSpawnableCT > 0 && m_iNumTerrorist > 0 && m_iNumSpawnableTerrorist > 0 )
	{
		if ( deadTerrorists && !aliveTerrorists && aliveCTs )
		{
			bool bombPlanted = false;
			pEntity = NULL;
			while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "grenade" ) ) != NULL )
			{
				CGrenade* pGrenade = (CGrenade*)pEntity;
				if ( pGrenade->m_bIsC4 && !pGrenade->m_bJustBlew )
					bombPlanted = true;
			}
			if ( bombPlanted )
				return;
			Broadcast( "ctwin" );
			m_iAccountCT += m_bMapHasBombTarget ? 3250 : 3000;
			if ( !neededPlayers )
			{
				++m_iNumCTWins;
				UpdateTeamScores();
			}
			EndRoundMessage( "#CTs_Win", 8 );
			TerminateRound( 5, 1 );
			return;
		}
		if ( !aliveCTs && deadCTs )
		{
			Broadcast( "terwin" );
			m_iAccountTerrorist += m_bMapHasBombTarget ? 3250 : 3000;
			if ( !neededPlayers )
			{
				++m_iNumTerroristWins;
				UpdateTeamScores();
			}
			EndRoundMessage( "#Terrorists_Win", 9 );
			TerminateRound( 5, 2 );
			return;
		}
	}
	else if ( !aliveTerrorists && !aliveCTs )
	{
		EndRoundMessage( "#Round_Draw", 10 );
		Broadcast( "rounddraw" );
		TerminateRound( 5, 3 );
		return;
	}
	int totalHostages = 0;
	bool hostageAlive = false;
	pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "hostage_entity" ) ) != NULL )
	{
		++totalHostages;
		if ( pEntity->pev->takedamage == DAMAGE_YES )
			hostageAlive = true;
	}
	if ( !totalHostages || hostageAlive || m_iHostagesRescued < totalHostages * 0.5 )
		return;
	Broadcast( "ctwin" );
	m_iAccountCT += 2500;
	if ( !neededPlayers )
	{
		++m_iNumCTWins;
		UpdateTeamScores();
	}
	EndRoundMessage( "#All_Hostages_Rescued", 11 );
	TheBots->OnEvent( EVENT_ALL_HOSTAGES_RESCUED );
	if ( IsCareer() )
		TheCareerTasks->HandleEvent( EVENT_ALL_HOSTAGES_RESCUED, NULL, NULL );
	TerminateRound( 5, 1 );
}

void CHalfLifeMultiplay::ChangeLevel()
{
	static char szPreviousMapCycleFile[256];
	static mapcycle_t mapcycle;
	char nextMap[32];
	char firstMap[32];
	char commands[1500];
	char rules[1500];
	int minPlayers = 0;
	int maxPlayers = 0;
	strcpy( firstMap, "hldm1" );
	char* filename = (char*)CVAR_GET_STRING( "mapcyclefile" );
	commands[0] = 0;
	rules[0] = 0;
	int players = CountPlayers();
	bool cycleMaps = true;
	if ( stricmp( filename, szPreviousMapCycleFile ) )
	{
		strncpy( szPreviousMapCycleFile, filename, sizeof( szPreviousMapCycleFile ) - 1 );
		szPreviousMapCycleFile[sizeof( szPreviousMapCycleFile ) - 1] = 0;
		DestroyMapCycle( &mapcycle );
		if ( !ReloadMapCycleFile( filename, &mapcycle ) || !mapcycle.items )
		{
			ALERT( at_console, "Unable to load map cycle file %s\n", filename );
			cycleMaps = false;
		}
	}
	if ( cycleMaps && mapcycle.items )
	{
		strcpy( nextMap, STRING( gpGlobals->mapname ) );
		strcpy( firstMap, STRING( gpGlobals->mapname ) );
		bool found = false;
		mapcycle_item_t* item;

		// Traverse list
		for ( item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next )
		{
			bool keepLooking = false;
			if ( item->minplayers )
			{
				if ( players >= item->minplayers )
				{
					found = true;
					minPlayers = item->minplayers;
				}
				else
					keepLooking = true;
			}
			if ( item->maxplayers )
			{
				if ( players <= item->maxplayers )
				{
					found = true;
					maxPlayers = item->maxplayers;
				}
				else
					keepLooking = true;
			}
			if ( keepLooking )
				continue;
			found = true;
			break;
		}
		if ( !found )
			item = mapcycle.next_item;

		// Increment next item pointer
		mapcycle.next_item = item->next;
		strcpy( nextMap, item->mapname );
		ExtractCommandString( item->rulebuffer, commands );
		strcpy( rules, item->rulebuffer );
	}
	if ( !IS_MAP_VALID( nextMap ) )
		strcpy( nextMap, firstMap );
	g_fGameOver = TRUE;
	ALERT( at_console, "CHANGE LEVEL: %s\n", nextMap );
	if ( minPlayers || maxPlayers )
		ALERT( at_console, "PLAYER COUNT:  min %i max %i current %i\n", minPlayers, maxPlayers, players );
	if ( rules[0] )
		ALERT( at_console, "RULES:  %s\n", rules );
	CHANGE_LEVEL( nextMap, NULL );
	if ( commands[0] )
		SERVER_COMMAND( commands );
}

void CHalfLifeMultiplay::Think()
{
	MonitorTutorStatus();
	m_VoiceGameMgr.Update( gpGlobals->frametime );
	if ( sv_clienttrace->value != 1 )
		CVAR_SET_FLOAT( "sv_clienttrace", 1 );
	if ( m_fRoundCount == 0 )
	{
		m_fRoundCount = gpGlobals->time;
		m_fIntroRoundCount = gpGlobals->time;
	}
	if ( m_flForceCameraValue != forcecamera.value || m_flForceChaseCamValue != forcechasecam.value ||
			m_flFadeToBlackValue != fadetoblack.value )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgForceCam );
			WRITE_BYTE( forcecamera.value != 0 );
			WRITE_BYTE( forcechasecam.value != 0 );
			WRITE_BYTE( fadetoblack.value != 0 );
		MESSAGE_END();
		m_flForceCameraValue = forcecamera.value;
		m_flForceChaseCamValue = forcechasecam.value;
		m_flFadeToBlackValue = fadetoblack.value;
	}
	if ( g_fGameOver )
	{
		int chatTime = (int)CVAR_GET_FLOAT( "mp_chattime" );
		if ( chatTime <= 0 )
			CVAR_SET_STRING( "mp_chattime", "1" );
		else if ( chatTime > 120 )
			CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( 120 ) );
		m_flIntermissionEndTime = m_flIntermissionStartTime + mp_chattime.value;
		if ( gpGlobals->time > m_flIntermissionEndTime && !IsCareer() &&
				( !UTIL_HumansInGame( false ) || m_iEndIntermissionButtonHit ||
						gpGlobals->time > m_flIntermissionStartTime + 120 ) )
			ChangeLevel(); // intermission is over
		return;
	}
	bool negativeTimeLimit = timelimit.value < 0;
	if ( negativeTimeLimit )
		CVAR_SET_FLOAT( "mp_timelimit", 0 );
	if ( !IsCareer() )
	{
		if ( !negativeTimeLimit && timelimit.value != 0 )
		{
			g_flTimeLimit = timelimit.value * 60 + g_flResetTime;
			if ( gpGlobals->time >= g_flTimeLimit )
			{
				ALERT( at_console, "Changing maps because time limit has been met\n" );
				GoToIntermission();
				return;
			}
		}
		if ( m_iMaxRounds && m_iTotalRoundsPlayed >= m_iMaxRounds )
		{
			ALERT( at_console, "Changing maps due to maximum rounds have been met\n" );
			GoToIntermission();
			return;
		}
		if ( m_iMaxRoundsWon && ( m_iNumCTWins >= m_iMaxRoundsWon || m_iNumTerroristWins >= m_iMaxRoundsWon ) &&
				abs( m_iNumCTWins - m_iNumTerroristWins ) >= m_iRoundWinDifference )
		{
			ALERT( at_console, "Changing maps...one team has won the specified number of rounds\n" );
			GoToIntermission();
			return;
		}
	}
	else if ( m_fCareerMatchMenuTime > 0 && gpGlobals->time > m_fCareerMatchMenuTime )
	{
		if ( m_fCareerMatchMenuTime + 10 > gpGlobals->time )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
				if ( pPlayer && pPlayer->IsBot() && ( (CCSBot*)pPlayer )->IsUsingVoice() )
					return;
			}
		}
		UTIL_CareerDPrintf( "Ending career match...one team has won the specified number of rounds\n" );
		MESSAGE_BEGIN( MSG_ALL, gmsgCZCareer );
			WRITE_STRING( "MATCH" );
			WRITE_LONG( m_iNumCTWins );
			WRITE_LONG( m_iNumTerroristWins );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ALL, gmsgCZCareerHUD );
			WRITE_STRING( "MATCH" );
			WRITE_LONG( m_iNumCTWins );
			WRITE_LONG( m_iNumTerroristWins );
			WRITE_BYTE( m_iCareerMatchWins );
			WRITE_BYTE( m_iRoundWinDifference );
			WRITE_BYTE( m_iRoundWinStatus );
		MESSAGE_END();
		UTIL_LogPrintf( "Career Match %d %d %d %d\n", m_iRoundWinStatus, m_iNumCTWins, m_iNumTerroristWins,
				TheCareerTasks->AreAllTasksComplete() );
		SERVER_COMMAND( "setpause\n" );
		return;
	}
	if ( (double)m_iStoredSpectValue != allow_spectators.value )
	{
		m_iStoredSpectValue = (int)allow_spectators.value;
		MESSAGE_BEGIN( MSG_ALL, gmsgAllowSpec );
			WRITE_BYTE( (int)allow_spectators.value );
		MESSAGE_END();
	}
	if ( IsFreezePeriod() )
	{
		if ( m_iRoundTimeSecs - gpGlobals->time + m_fRoundCount <= 0 )
		{
			UTIL_LogPrintf( "World triggered \"Round_Start\"\n" );
			m_bFreezePeriod = FALSE;
			char ctSentence[40];
			char terroristSentence[40];
			switch ( RANDOM_LONG( 0, 3 ) )
			{
			case 0:
				strcpy( ctSentence, "%!MRAD_MOVEOUT" );
				strcpy( terroristSentence, "%!MRAD_MOVEOUT" );
				break;
			case 1:
				strcpy( ctSentence, "%!MRAD_LETSGO" );
				strcpy( terroristSentence, "%!MRAD_LETSGO" );
				break;
			case 2:
				strcpy( ctSentence, "%!MRAD_LOCKNLOAD" );
				strcpy( terroristSentence, "%!MRAD_LOCKNLOAD" );
				break;
			default:
				strcpy( ctSentence, "%!MRAD_GO" );
				strcpy( terroristSentence, "%!MRAD_GO" );
				break;
			}
			if ( m_bMapHasEscapeZone )
			{
				strcpy( ctSentence, "%!MRAD_ELIM" );
				strcpy( terroristSentence, "%!MRAD_GETOUT" );
			}
			else if ( m_iMapHasVIPSafetyZone == 1 )
			{
				strcpy( ctSentence, "%!MRAD_VIP" );
				strcpy( terroristSentence, "%!MRAD_LOCKNLOAD" );
			}
			m_fRoundCount = gpGlobals->time;
			m_iRoundTimeSecs = m_iRoundTime;
			TheCareerTasks->HandleEvent( EVENT_ROUND_START, NULL, NULL );
			bool ctPlayed = false;
			bool terroristPlayed = false;
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
				if ( !pPlayer || pPlayer->pev->flags == FL_DORMANT )
					continue;
				if ( pPlayer->m_iJoiningState == JOINED )
				{
					if ( pPlayer->m_iTeam == CT && !ctPlayed )
					{
						ctPlayed = true;
						pPlayer->Radio( ctSentence, NULL, PITCH_NORM, true );
					}
					else if ( pPlayer->m_iTeam == TERRORIST && !terroristPlayed )
					{
						pPlayer->Radio( terroristSentence, NULL, PITCH_NORM, true );
						terroristPlayed = true;
					}
					if ( pPlayer->m_iTeam != SPECTATOR )
					{
						pPlayer->ResetMaxSpeed();
						pPlayer->m_bCanShoot = true;
					}
				}
				pPlayer->SyncRoundTimer();
			}
			TheBots->OnEvent( EVENT_ROUND_START );
			TheCareerTasks->HandleEvent( EVENT_ROUND_START, NULL, NULL );
		}
	}
	else if ( HasRoundTimeExpired() )
	{
		UTIL_FindEntityByClassname( NULL, "grenade" );
		if ( m_bMapHasBombTarget )
		{
			Broadcast( "ctwin" );
			m_iAccountCT += 3250;
			++m_iNumCTWins;
			EndRoundMessage( "#Target_Saved", 12 );
			TerminateRound( 5, 1 );
			UpdateTeamScores();
			MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound( TERRORIST );
		}
		else if ( UTIL_FindEntityByClassname( NULL, "hostage_entity" ) )
		{
			Broadcast( "terwin" );
			m_iAccountTerrorist += 3250;
			++m_iNumTerroristWins;
			EndRoundMessage( "#Hostages_Not_Rescued", 13 );
			TerminateRound( 5, 2 );
			UpdateTeamScores();
			MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound( CT );
		}
		else if ( m_bMapHasEscapeZone )
		{
			Broadcast( "ctwin" );
			++m_iNumCTWins;
			EndRoundMessage( "#Terrorists_Not_Escaped", 14 );
			TerminateRound( 5, 1 );
			UpdateTeamScores();
		}
		else if ( m_iMapHasVIPSafetyZone == 1 )
		{
			Broadcast( "terwin" );
			m_iAccountTerrorist += 3250;
			++m_iNumTerroristWins;
			EndRoundMessage( "#VIP_Not_Escaped", 15 );
			TerminateRound( 5, 2 );
			UpdateTeamScores();
		}
		m_fRoundCount = gpGlobals->time + 60;
	}
	if ( m_fTeamCount != 0 && !( gpGlobals->time < m_fTeamCount ) )
	{
		if ( IsCareer() && m_fCareerRoundMenuTime != 0 )
		{
			bool talking = false;
			if ( m_fTeamCount + 10 > gpGlobals->time )
			{
				for ( int i = 1; i <= gpGlobals->maxClients; ++i )
				{
					CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
					if ( pPlayer && pPlayer->IsBot() && ( (CCSBot*)pPlayer )->IsUsingVoice() )
					{
						talking = true;
						break;
					}
				}
			}
			if ( !talking )
			{
				if ( !m_fCareerMatchMenuTime && m_iCareerMatchWins )
				{
					bool ctWon = m_iNumCTWins >= m_iCareerMatchWins &&
								 m_iNumCTWins - m_iNumTerroristWins >= m_iRoundWinDifference;
					bool terroristWon = m_iNumTerroristWins >= m_iCareerMatchWins &&
										m_iNumTerroristWins - m_iNumCTWins >= m_iRoundWinDifference;
					bool humanTeamWon = strcmp( humans_join_team.string, "CT" ) == 0 ? ctWon : terroristWon;
					if ( TheCareerTasks->AreAllTasksComplete() && humanTeamWon )
					{
						m_fCareerRoundMenuTime = 0;
						m_fCareerMatchMenuTime = gpGlobals->time;
						return;
					}
				}
				m_bFreezePeriod = TRUE;
				for ( int i = 1; i <= gpGlobals->maxClients; ++i )
				{
					CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
					if ( !pPlayer || pPlayer->IsBot() )
						continue;
					MESSAGE_BEGIN( MSG_ONE, gmsgCZCareerHUD, NULL, pPlayer->edict() );
						WRITE_STRING( "ROUND" );
						WRITE_LONG( m_iNumCTWins );
						WRITE_LONG( m_iNumTerroristWins );
						WRITE_BYTE( m_iCareerMatchWins );
						WRITE_BYTE( m_iRoundWinDifference );
						WRITE_BYTE( m_iRoundWinStatus );
					MESSAGE_END();
					pPlayer->m_afPhysicsFlags |= PFLAG_ONBARNACLE;
					m_fTeamCount = gpGlobals->time + 100000;
					UTIL_LogPrintf( "Career Round %d %d %d %d\n", m_iRoundWinStatus, m_iNumCTWins, m_iNumTerroristWins,
							TheCareerTasks->AreAllTasksComplete() );
					break;
				}
				m_fCareerRoundMenuTime = 0;
			}
		}
		else
			RestartRound();
		if ( TheTutor )
			TheTutor->PurgeMessages();
	}
	if ( !m_bLevelInitialized )
	{
		m_iSpawnPointCount_Terrorist = 0;
		m_iSpawnPointCount_CT = 0;
		CBaseEntity* pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "info_player_deathmatch" ) ) != NULL )
			++m_iSpawnPointCount_Terrorist;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "info_player_start" ) ) != NULL )
			++m_iSpawnPointCount_CT;
		m_bLevelInitialized = true;
	}
	CareerSessionRestoreBots();
	if ( gpGlobals->time > m_tmNextPeriodicThink )
	{
		int delay = (int)restartround.value;
		if ( !delay )
			delay = (int)sv_restart.value;
		if ( delay > 0 )
		{
			if ( delay > 60 )
				delay = 60;
			UTIL_LogPrintf( "World triggered \"Restart_Round_(%i_%s)\"\n", delay, delay == 1 ? "second" : "seconds" );
			CHalfLifeMultiplay* pRules = (CHalfLifeMultiplay*)g_pGameRules;
			if ( pRules )
			{
				UTIL_LogPrintf(
						"Team \"CT\" scored \"%i\" with \"%i\" players\n", pRules->m_iNumCTWins, pRules->m_iNumCT );
				UTIL_LogPrintf( "Team \"TERRORIST\" scored \"%i\" with \"%i\" players\n", pRules->m_iNumTerroristWins,
						pRules->m_iNumTerrorist );
			}
			const char* units = delay == 1 ? "SECOND" : "SECONDS";
			UTIL_ClientPrintAll( HUD_PRINTCENTER, "#Game_will_restart_in", UTIL_dtos1( delay ), units );
			UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#Game_will_restart_in_console", UTIL_dtos1( delay ), units );
			m_bCompleteReset = true;
			m_fTeamCount = gpGlobals->time + delay;
			CVAR_SET_FLOAT( "sv_restartround", 0 );
			CVAR_SET_FLOAT( "sv_restart", 0 );
			CareerRestart();
		}
		m_tmNextPeriodicThink = gpGlobals->time + 1;
		if ( g_psv_accelerate->value != 5 )
			CVAR_SET_FLOAT( "sv_accelerate", 5 );
		if ( g_psv_friction->value != 4 )
			CVAR_SET_FLOAT( "sv_friction", 4 );
		if ( g_psv_stopspeed->value != 75 )
			CVAR_SET_FLOAT( "sv_stopspeed", 75 );
		m_iMaxRounds = (int)maxrounds.value;
		if ( m_iMaxRounds < 0 )
		{
			m_iMaxRounds = 0;
			CVAR_SET_FLOAT( "mp_maxrounds", 0 );
		}
		if ( (int)winlimit.value < 0 )
		{
			m_iMaxRoundsWon = 0;
			CVAR_SET_FLOAT( "mp_winlimit", 0 );
		}
		else
			m_iMaxRoundsWon = (int)winlimit.value;
	}
}

void CHalfLifeMultiplay::DeathNotice( CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor )
{
	(void)CBaseEntity::Instance( pKiller );
	int killerIndex = 0;
	const char* weapon;
	if ( pKiller->flags & FL_CLIENT )
	{
		killerIndex = ENTINDEX( pKiller->pContainingEntity );
		weapon = "world";
		if ( pInflictor )
		{
			if ( pKiller == pInflictor )
			{
				CBasePlayer* pPlayer = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( ENT( pKiller ) ) );
				if ( pPlayer && pPlayer->m_pActiveItem )
					weapon = pPlayer->m_pActiveItem->pszName();
			}
			else
				weapon = STRING( pInflictor->classname );
		}
	}
	else
		weapon = STRING( pInflictor->classname );
	if ( !strncmp( weapon, "weapon_", 7 ) )
		weapon += 7;
	else if ( !strncmp( weapon, "monster_", 8 ) )
		weapon += 8;
	else if ( !strncmp( weapon, "func_", 5 ) )
		weapon += 5;
	bool headshot = pVictim->m_bHeadshotKilled;
	if ( !TheTutor )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
			WRITE_BYTE( killerIndex );
			WRITE_BYTE( pVictim->entindex() );
			WRITE_BYTE( headshot );
			WRITE_STRING( weapon );
		MESSAGE_END();
	}
	if ( !strcmp( weapon, "egon" ) )
		weapon = "gluon gun";
	else if ( !strcmp( weapon, "gauss" ) )
		weapon = "tau_cannon";
	if ( pKiller == pVictim->pev )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n", STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ), GETPLAYERAUTHID( pKiller->pContainingEntity ),
				GetTeam( pVictim->m_iTeam ), weapon );
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		CBasePlayer* pPlayer = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( ENT( pKiller ) ) );
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n", STRING( pKiller->netname ),
				GETPLAYERUSERID( pKiller->pContainingEntity ), GETPLAYERAUTHID( pKiller->pContainingEntity ),
				pPlayer ? GetTeam( pPlayer->m_iTeam ) : "", STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ), GETPLAYERAUTHID( pVictim->edict() ), GetTeam( pVictim->m_iTeam ),
				weapon );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n", STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ), GETPLAYERAUTHID( pVictim->edict() ), GetTeam( pVictim->m_iTeam ),
				weapon );
	}
	CheckWinConditions();
	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE( 9 );	// command length in bytes
		WRITE_BYTE( DRC_CMD_EVENT );	// player killed
		WRITE_SHORT( pVictim->entindex() );
		WRITE_SHORT( pInflictor ? ENTINDEX( pInflictor->pContainingEntity ) : ENTINDEX( pKiller->pContainingEntity ) );
		WRITE_LONG( headshot ? DRC_FLAG_DRAMATIC | DRC_FLAG_SLOWMOTION | 9 : DRC_FLAG_DRAMATIC | 7 );
	MESSAGE_END();
}

void CHalfLifeMultiplay::PlayerKilled( CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor )
{
	const int newTrainState = 0xc0;
	const int friendKilledHint = 1 << 10;
	const int enemyKilledHint = 1 << 11;
	DeathNotice( pVictim, pKiller, pInflictor );
	++pVictim->m_iDeaths;
	pVictim->m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
	pVictim->m_bNotKilled = false;
	pVictim->m_bEscaped = false;
	pVictim->m_iTrain = newTrainState;
	SET_VIEW( pVictim->edict(), pVictim->edict() );

	CBaseEntity* pKillerEntity = CBaseEntity::Instance( pKiller );
	CBasePlayer* pScoringPlayer = NULL;
	if ( pKillerEntity )
	{
		if ( pKillerEntity->Classify() == CLASS_PLAYER )
			pScoringPlayer = (CBasePlayer*)pKillerEntity;
		else if ( pKillerEntity->Classify() == CLASS_VEHICLE )
		{
			pScoringPlayer = ( (CFuncVehicle*)pKillerEntity )->m_pDriver;
			if ( pScoringPlayer )
			{
				pKiller = pScoringPlayer->pev;
				pKillerEntity = pScoringPlayer;
			}
		}
	}
	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );

	if ( !pKillerEntity || pKiller == pVictim->pev || !pKillerEntity->IsPlayer() )
		pKiller->frags -= 1;
	else
	{
		CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pKiller );
		if ( pPlayer->m_iTeam == pVictim->m_iTeam )
		{
			pKiller->frags -= IPointsForKill( pScoringPlayer, pVictim );
			pPlayer->AddAccount( -3300, true );
			++pPlayer->m_iTeamKills;
			pPlayer->m_bJustKilledTeammate = true;
			ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "#Killed_Teammate" );
			ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "#Game_teammate_kills", UTIL_dtos1( pPlayer->m_iTeamKills ) );
			if ( pPlayer->m_iTeamKills == 3 && CVAR_GET_FLOAT( "mp_autokick" ) != 0 )
			{
				ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "#Banned_For_Killing_Teamates" );
				int userID = GETPLAYERUSERID( pPlayer->edict() );
				if ( userID != -1 )
					SERVER_COMMAND( UTIL_VarArgs( "kick # %d\n", userID ) );
			}
			if ( !( pPlayer->m_flDisplayHistory & friendKilledHint ) )
			{
				pPlayer->m_flDisplayHistory |= friendKilledHint;
				pPlayer->HintMessage( "#Hint_careful_around_teammates", false, false );
			}
		}
		else
		{
			pKiller->frags += IPointsForKill( pScoringPlayer, pVictim );
			if ( pVictim->m_bIsVIP )
			{
				pPlayer->HintMessage( "#Hint_reward_for_killing_vip", true, false );
				pPlayer->AddAccount( 2500, true );
				MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
					WRITE_BYTE( 9 );
					WRITE_BYTE( DRC_CMD_EVENT );
					WRITE_SHORT( pVictim->entindex() );
					WRITE_SHORT( ENTINDEX( pInflictor->pContainingEntity ) );
					WRITE_LONG( DRC_FLAG_FINAL | DRC_FLAG_DRAMATIC | DRC_FLAG_PRIO_MASK );
				MESSAGE_END();
				const char* authID = GETPLAYERAUTHID( pPlayer->edict() );
				int userID = GETPLAYERUSERID( pPlayer->edict() );
				UTIL_LogPrintf( "\"%s<%i><%s><TERRORIST>\" triggered \"Assassinated_The_VIP\"\n",
						STRING( pPlayer->pev->netname ), userID, authID );
			}
			else
				pPlayer->AddAccount( 300, true );
			if ( !( pPlayer->m_flDisplayHistory & enemyKilledHint ) )
			{
				pPlayer->m_flDisplayHistory |= enemyKilledHint;
				pPlayer->HintMessage( "#Hint_win_round_by_killing_enemy", false, false );
			}
		}
		FireTargets( "game_playerkill", pKillerEntity, pKillerEntity, USE_TOGGLE, 0 );
	}

	MESSAGE_BEGIN( MSG_BROADCAST, gmsgScoreInfo );
		WRITE_BYTE( pVictim->entindex() );
		WRITE_SHORT( (int)pVictim->pev->frags );
		WRITE_SHORT( pVictim->m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( pVictim->m_iTeam );
	MESSAGE_END();

	pKillerEntity = CBaseEntity::Instance( pKiller );
	if ( pKillerEntity && pKillerEntity->Classify() == CLASS_PLAYER )
	{
		CBasePlayer* pPlayer = (CBasePlayer*)pKillerEntity;
		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( pPlayer->entindex() );
			WRITE_SHORT( (int)pPlayer->pev->frags );
			WRITE_SHORT( pPlayer->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( pPlayer->m_iTeam );
		MESSAGE_END();

		// let the killer paint another decal as soon as he'd like.
		pPlayer->m_flNextDecalTime = gpGlobals->time;
	}
}

void CHalfLifeMultiplay::InitHUD( CBasePlayer* pl )
{
	const char* authID = GETPLAYERAUTHID( pl->edict() );
	int userID = GETPLAYERUSERID( pl->edict() );
	UTIL_LogPrintf( "\"%s<%i><%s><>\" entered the game\n", STRING( pl->pev->netname ), userID, authID );
	UpdateGameMode( pl );
	if ( CVAR_GET_FLOAT( "sv_cheats" ) == 0 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgViewMode, NULL, pl->edict() );
		MESSAGE_END();
	}
	MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
		WRITE_BYTE( pl->entindex() );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( pl->m_iTeam );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ONE, gmsgShadowIdx, NULL, pl->edict() );
		WRITE_LONG( g_iShadowSprite );
	MESSAGE_END();
	if ( IsCareer() )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCZCareer, NULL, pl->edict() );
			WRITE_STRING( "START" );
			WRITE_SHORT( m_iRoundTime );
		MESSAGE_END();
		if ( !pl->IsBot() )
			TheCareerTasks->SyncToPlayer( pl );
	}
	else
		SendMOTDToClient( pl->edict() );

	// loop through all active players and send their score info to the new client
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
				WRITE_BYTE( i );	// client number
				WRITE_SHORT( (int)pPlayer->pev->frags );
				WRITE_SHORT( pPlayer->m_iDeaths );
				WRITE_SHORT( 0 );
				WRITE_SHORT( pPlayer->m_iTeam );
			MESSAGE_END();
		}
	}
	MESSAGE_BEGIN( MSG_ONE, gmsgTeamScore, NULL, pl->edict() );
		WRITE_STRING( "TERRORIST" );
		WRITE_SHORT( m_iNumTerroristWins );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ONE, gmsgTeamScore, NULL, pl->edict() );
		WRITE_STRING( "CT" );
		WRITE_SHORT( m_iNumCTWins );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ONE, gmsgAllowSpec, NULL, pl->edict() );
		WRITE_BYTE( (int)allow_spectators.value );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ONE, gmsgForceCam, NULL, pl->edict() );
		WRITE_BYTE( forcecamera.value != 0 );
		WRITE_BYTE( forcechasecam.value != 0 );
		WRITE_BYTE( fadetoblack.value != 0 );
	MESSAGE_END();
	if ( g_fGameOver )
	{
		MESSAGE_BEGIN( MSG_ONE, SVC_INTERMISSION, NULL, pl->edict() );
		MESSAGE_END();
	}
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;
		MESSAGE_BEGIN( MSG_ONE, gmsgTeamInfo, NULL, pl->edict() );
			WRITE_BYTE( pPlayer->entindex() );
			switch ( pPlayer->m_iTeam )
			{
			case CT:
				WRITE_STRING( "CT" );
				break;
			case TERRORIST:
				WRITE_STRING( "TERRORIST" );
				break;
			case SPECTATOR:
				WRITE_STRING( "SPECTATOR" );
				break;
			default:
				WRITE_STRING( "UNASSIGNED" );
				break;
			}
		MESSAGE_END();
		pPlayer->SetScoreboardAttributes( pl );
		if ( pl->entindex() != i && pPlayer->pev->flags != FL_DORMANT && pPlayer->pev->deadflag == DEAD_NO &&
				pPlayer->m_iTeam == pl->m_iTeam )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgRadar, NULL, pl->edict() );
				WRITE_BYTE( pPlayer->entindex() );
				WRITE_COORD( pPlayer->pev->origin.x );
				WRITE_COORD( pPlayer->pev->origin.y );
				WRITE_COORD( pPlayer->pev->origin.z );
			MESSAGE_END();
		}
		int healthPolicy = (int)scoreboard_showhealth.value;
		int moneyPolicy = (int)scoreboard_showmoney.value;
		bool sendHealth = healthPolicy == pPlayer->m_iTeam || healthPolicy == 4 ||
						  ( healthPolicy == 3 && pl->m_iTeam == pPlayer->m_iTeam ) ||
						  ( healthPolicy == 5 && ( pl->m_iTeam == pPlayer->m_iTeam || pl->m_iTeam == SPECTATOR ) );
		bool sendMoney = moneyPolicy == pPlayer->m_iTeam || moneyPolicy == 4 ||
						 ( moneyPolicy == 3 && pl->m_iTeam == pPlayer->m_iTeam ) ||
						 ( moneyPolicy == 5 && ( pl->m_iTeam == pPlayer->m_iTeam || pl->m_iTeam == SPECTATOR ) );
		MESSAGE_BEGIN( MSG_ONE, gmsgHealthInfo, NULL, pl->edict() );
			WRITE_BYTE( pPlayer->entindex() );
			WRITE_LONG( sendHealth ? pPlayer->m_iClientHealth : -1 );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ONE, gmsgAccount, NULL, pl->edict() );
			WRITE_BYTE( pPlayer->entindex() );
			WRITE_LONG( sendMoney ? pPlayer->m_iAccount : -1 );
		MESSAGE_END();
	}
	if ( g_pGameRules->m_bBombDropped )
	{
		CBaseEntity* pBomb = UTIL_FindEntityByClassname( NULL, "weapon_c4" );
		if ( pBomb )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBombDrop, NULL, pl->edict() );
				WRITE_COORD( pBomb->pev->origin.x );
				WRITE_COORD( pBomb->pev->origin.y );
				WRITE_COORD( pBomb->pev->origin.z );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}
	}
}
