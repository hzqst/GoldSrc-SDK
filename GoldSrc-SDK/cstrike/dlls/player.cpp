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
/*

===== player.cpp ========================================================

  functions dealing with the player

*/

#include <vector>
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "game.h"
#include "client.h"
#include "nodes.h"
#include "monsters.h"
#include "hitgroups.h"
#include "saverestore.h"
#include "career_tasks.h"
#include "tutor.h"
#include "decals.h"
#include "shake.h"
#include "bot/cs_bot_chatter.h"
#include "nav_area.h"
#include "bot/cs_bot_manager.h"
#include "skill.h"
#include "hostage/hostage.h"
#include "trains.h"
#include "soundent.h"
#include "studio.h"
#include "pm_shared.h"
#include "hltv.h"

#define TRAIN_ACTIVE		0x80
#define TRAIN_NEW		0xC0
#define TRAIN_OFF		0
#define TRAIN_NEUTRAL	1
#define TRAIN_SLOW		2
#define TRAIN_MEDIUM		3
#define TRAIN_FAST		4
#define TRAIN_BACK		5
#define FLASH_DRAIN_TIME	1.2 //100 units/3 minutes
#define FLASH_CHARGE_TIME	0.2 // 100 units/20 seconds  (seconds per unit)

typedef CareerTaskList::iterator CareerTaskListIt;
extern bool UseBotArgs;
extern bool g_skipCareerInitialSpawn;
extern const char* BotArgs[4];
char* MP_COM_Parse( char* data );
char* MP_COM_GetToken();
void LogAttack( CBasePlayer* pAttacker, CBasePlayer* pVictim, int teamAttack, int healthHit,
	int armorHit, int newHealth, int newArmor, const char* weaponName );

// the world node graph
extern CGraph WorldGraph;
extern int gDisplayTitle;
extern BOOL g_fGameOver;

BOOL gInitHUD = TRUE;
int gEvilImpulse101;
int giPrecacheGrunt;
int gmsgShake;
int gmsgFade;
int gmsgFlashlight;
int gmsgFlashBattery;
int gmsgResetHUD;
int gmsgInitHUD;
int gmsgViewMode;
int gmsgShowGameTitle;
int gmsgCurWeapon;
int gmsgHealth;
int gmsgDamage;
int gmsgBattery;
int gmsgTrain;
int gmsgLogo;
int gmsgWeaponList;
int gmsgAmmoX;
int gmsgHudText;
int gmsgHudTextArgs;
int gmsgDeathMsg;
int gmsgScoreAttrib;
int gmsgScoreInfo;
int gmsgTeamInfo;
int gmsgTeamScore;
int gmsgGameMode;
int gmsgMOTD;
int gmsgServerName;
int gmsgAmmoPickup;
int gmsgWeapPickup;
int gmsgItemPickup;
int gmsgHideWeapon;
int gmsgSayText;
int gmsgTextMsg;
int gmsgSetFOV;
int gmsgShowMenu;
int gmsgSendAudio;
int gmsgRoundTime;
int gmsgMoney;
int gmsgBlinkAcct;
int gmsgArmorType;
int gmsgStatusValue;
int gmsgStatusText;
int gmsgStatusIcon;
int gmsgBarTime;
int gmsgReloadSound;
int gmsgCrosshair;
int gmsgNVGToggle;
int gmsgRadar;
int gmsgAccount;
int gmsgHealthInfo;
int gmsgSpectator;
int gmsgVGUIMenu;
int gmsgCZCareer;
int gmsgCZCareerHUD;
int gmsgTaskTime;
int gmsgTutorText;
int gmsgTutorLine;
int gmsgShadowIdx;
int gmsgTutorState;
int gmsgTutorClose;
int gmsgAllowSpec;
int gmsgBombDrop;
int gmsgBombPickup;
int gmsgHostagePos;
int gmsgHostageK;
int gmsgGeigerRange;
int gmsgSendCorpse;
int gmsgHLTV;
int gmsgSpecHealth;
int gmsgForceCam;
int gmsgADStop;
int gmsgReceiveW;
int gmsgScenarioIcon;
int gmsgBotVoice;
int gmsgBuyClose;
int gmsgItemStatus;
int gmsgLocation;
int gmsgSpecHealth2;
int gmsgBarTime2;
int gmsgBotProgress;
int gmsgBrass;
int gmsgFog;
int gmsgShowTimer;
cvar_t* sv_aim;
char g_szMapBriefingText[512];

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 *
 * ENTITY_METHOD(PlayerDie)
 */
entvars_t* g_pevLastInflictor;  // Set in combat.cpp.  Used to pass the damage inflictor for death messages.
CBaseEntity* g_pLastSpawn;
CBaseEntity* g_pLastCTSpawn;
CBaseEntity* g_pLastTerroristSpawn;

struct ZombieSpawn
{
	CBaseEntity* entity;
	CountdownTimer useableTimer;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

ZombieSpawn zombieSpawn[256];
int zombieSpawnCount;
static CBaseEntity* g_pSelectedZombieSpawn;

LINK_ENTITY_TO_CLASS( player, CBasePlayer );

// Global Savedata for player
TYPEDESCRIPTION CBasePlayer::m_playerSaveData[] =
{
	DEFINE_FIELD( CBasePlayer, m_flFlashLightTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iFlashBattery, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_afButtonLast, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_afButtonPressed, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_afButtonReleased, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, m_rgItems, FIELD_INTEGER, 4 ),
	DEFINE_FIELD( CBasePlayer, m_afPhysicsFlags, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_flTimeStepSound, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flSwimTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flDuckTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flWallJumpTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flSuitUpdate, FIELD_TIME ),
	DEFINE_ARRAY( CBasePlayer, m_rgSuitPlayList, FIELD_INTEGER, CSUITPLAYLIST ),
	DEFINE_FIELD( CBasePlayer, m_iSuitPlayNext, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, m_rgiSuitNoRepeat, FIELD_INTEGER, CSUITNOREPEAT ),
	DEFINE_ARRAY( CBasePlayer, m_rgflSuitNoRepeatTime, FIELD_TIME, CSUITNOREPEAT ),
	DEFINE_FIELD( CBasePlayer, m_lastDamageAmount, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( CBasePlayer, m_pActiveItem, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_pLastItem, FIELD_CLASSPTR ),
	DEFINE_ARRAY( CBasePlayer, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_FIELD( CBasePlayer, m_idrowndmg, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_idrownrestored, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_tSneaking, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iTrain, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_bitsHUDDamage, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_flFallVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, m_iTargetVolume, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iWeaponVolume, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iExtraSoundTypes, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iWeaponFlash, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_fLongJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fInitHUD, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_tbdPrev, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_pTank, FIELD_EHANDLE ),
	DEFINE_FIELD( CBasePlayer, m_iHideHUD, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iFOV, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_flDisplayHistory, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iJoiningState, FIELD_INTEGER )
};

int CBasePlayer::Save( CSave& save )
{
	if ( !CBaseToggle::Save( save ) )
		return 0;
	return save.WriteFields( "PLAYER", this, m_playerSaveData, ARRAYSIZE( m_playerSaveData ) );
}

extern edict_t* EntSelectSpawnPoint( CBaseEntity* player );

int CBasePlayer::Restore( CRestore& restore )
{
	if ( !CBaseToggle::Restore( restore ) )
		return 0;
	int result = restore.ReadFields( "PLAYER", this, m_playerSaveData, ARRAYSIZE( m_playerSaveData ) );
	SAVERESTOREDATA* saveData = (SAVERESTOREDATA*)gpGlobals->pSaveData;
	if ( !saveData->fUseLandmark )
	{
		ALERT( at_console, "No Landmark:%s\n", saveData->szLandmarkName );
		edict_t* spot = EntSelectSpawnPoint( this );
		pev->origin = spot->v.origin + Vector( 0, 0, 1 );
		pev->angles = spot->v.angles;
	}
	pev->v_angle.z = 0;	// Clear out roll
	pev->angles = pev->v_angle;
	pev->fixangle = 1;           // turn this way immediately
	m_bloodColor = BLOOD_COLOR_RED;
	m_modelIndexPlayer = pev->modelindex;
	if ( pev->flags & FL_DUCKING )
		UTIL_SetSize( pev, Vector( -16, -16, -18 ), Vector( 16, 16, 32 ) );
	else
		UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );
	TabulateAmmo();
	m_flDisplayHistory &= 0xFF907443;
	SetScoreboardAttributes( NULL );
	return result;
}

void WriteSigonMessages()
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		ItemInfo& info = CBasePlayerItem::ItemInfoArray[i];
		if ( !info.iId )
			continue;
		MESSAGE_BEGIN( MSG_INIT, gmsgWeaponList );
		WRITE_STRING( info.pszName != NULL ? info.pszName : "Empty" );
		WRITE_BYTE( CBasePlayer::GetAmmoIndex( info.pszAmmo1 ) );
		WRITE_BYTE( info.iMaxAmmo1 );
		WRITE_BYTE( CBasePlayer::GetAmmoIndex( info.pszAmmo2 ) );
		WRITE_BYTE( info.iMaxAmmo2 );
		WRITE_BYTE( info.iSlot );
		WRITE_BYTE( info.iPosition );
		WRITE_BYTE( info.iId );
		WRITE_BYTE( info.iFlags );
		MESSAGE_END();
	}
}

void LinkUserMessages()
{
	if ( gmsgCurWeapon != 0 )
		return;
	gmsgCurWeapon = REG_USER_MSG( "CurWeapon", 3 );
	gmsgGeigerRange = REG_USER_MSG( "Geiger", 1 );
	gmsgFlashlight = REG_USER_MSG( "Flashlight", 2 );
	gmsgFlashBattery = REG_USER_MSG( "FlashBat", 1 );
	gmsgHealth = REG_USER_MSG( "Health", 1 );
	gmsgDamage = REG_USER_MSG( "Damage", 12 );
	gmsgBattery = REG_USER_MSG( "Battery", 2 );
	gmsgTrain = REG_USER_MSG( "Train", 1 );
	gmsgHudText = REG_USER_MSG( "HudTextPro", -1 );
	REG_USER_MSG( "HudText", -1 );
	gmsgSayText = REG_USER_MSG( "SayText", -1 );
	gmsgTextMsg = REG_USER_MSG( "TextMsg", -1 );
	gmsgWeaponList = REG_USER_MSG( "WeaponList", -1 );
	gmsgResetHUD = REG_USER_MSG( "ResetHUD", 0 );		// called every respawn
	gmsgInitHUD = REG_USER_MSG( "InitHUD", 0 );		// called every time a new player joins the server
	gmsgViewMode = REG_USER_MSG( "ViewMode", 0 );
	gmsgShowGameTitle = REG_USER_MSG( "GameTitle", 1 );
	gmsgDeathMsg = REG_USER_MSG( "DeathMsg", -1 );
	gmsgScoreAttrib = REG_USER_MSG( "ScoreAttrib", 2 );
	gmsgScoreInfo = REG_USER_MSG( "ScoreInfo", 9 );
	gmsgTeamInfo = REG_USER_MSG( "TeamInfo", -1 );  // sets the name of a player's team
	gmsgTeamScore = REG_USER_MSG( "TeamScore", -1 );  // sets the score of a team on the scoreboard
	gmsgGameMode = REG_USER_MSG( "GameMode", 1 );
	gmsgMOTD = REG_USER_MSG( "MOTD", -1 );
	gmsgServerName = REG_USER_MSG( "ServerName", -1 );
	gmsgAmmoPickup = REG_USER_MSG( "AmmoPickup", 2 );
	gmsgWeapPickup = REG_USER_MSG( "WeapPickup", 1 );
	gmsgItemPickup = REG_USER_MSG( "ItemPickup", -1 );
	gmsgHideWeapon = REG_USER_MSG( "HideWeapon", 1 );
	gmsgSetFOV = REG_USER_MSG( "SetFOV", 1 );
	gmsgShowMenu = REG_USER_MSG( "ShowMenu", -1 );
	gmsgShake = REG_USER_MSG( "ScreenShake", 6 );
	gmsgFade = REG_USER_MSG( "ScreenFade", 10 );
	gmsgAmmoX = REG_USER_MSG( "AmmoX", 2 );
	gmsgSendAudio = REG_USER_MSG( "SendAudio", -1 );
	gmsgRoundTime = REG_USER_MSG( "RoundTime", 2 );
	gmsgMoney = REG_USER_MSG( "Money", 5 );
	gmsgArmorType = REG_USER_MSG( "ArmorType", 1 );
	gmsgBlinkAcct = REG_USER_MSG( "BlinkAcct", 1 );
	gmsgStatusValue = REG_USER_MSG( "StatusValue", -1 );
	gmsgStatusText = REG_USER_MSG( "StatusText", -1 );
	gmsgStatusIcon = REG_USER_MSG( "StatusIcon", -1 );
	gmsgBarTime = REG_USER_MSG( "BarTime", 2 );
	gmsgReloadSound = REG_USER_MSG( "ReloadSound", 2 );
	gmsgCrosshair = REG_USER_MSG( "Crosshair", 1 );
	gmsgNVGToggle = REG_USER_MSG( "NVGToggle", 1 );
	gmsgRadar = REG_USER_MSG( "Radar", 7 );
	gmsgSpectator = REG_USER_MSG( "Spectator", 2 );
	gmsgVGUIMenu = REG_USER_MSG( "VGUIMenu", -1 );
	gmsgTutorText = REG_USER_MSG( "TutorText", -1 );
	gmsgTutorLine = REG_USER_MSG( "TutorLine", -1 );
	gmsgTutorState = REG_USER_MSG( "TutorState", -1 );
	gmsgTutorClose = REG_USER_MSG( "TutorClose", -1 );
	gmsgAllowSpec = REG_USER_MSG( "AllowSpec", 1 );
	gmsgBombDrop = REG_USER_MSG( "BombDrop", 7 );
	gmsgBombPickup = REG_USER_MSG( "BombPickup", 0 );
	gmsgSendCorpse = REG_USER_MSG( "ClCorpse", -1 );
	gmsgHostagePos = REG_USER_MSG( "HostagePos", 8 );
	gmsgHostageK = REG_USER_MSG( "HostageK", 1 );
	gmsgHLTV = REG_USER_MSG( "HLTV", 2 );
	gmsgSpecHealth = REG_USER_MSG( "SpecHealth", 1 );
	gmsgForceCam = REG_USER_MSG( "ForceCam", 3 );
	gmsgADStop = REG_USER_MSG( "ADStop", 0 );
	gmsgReceiveW = REG_USER_MSG( "ReceiveW", 1 );
	gmsgCZCareer = REG_USER_MSG( "CZCareer", -1 );
	gmsgCZCareerHUD = REG_USER_MSG( "CZCareerHUD", -1 );
	gmsgShadowIdx = REG_USER_MSG( "ShadowIdx", 4 );
	gmsgTaskTime = REG_USER_MSG( "TaskTime", 4 );
	gmsgScenarioIcon = REG_USER_MSG( "Scenario", -1 );
	gmsgBotVoice = REG_USER_MSG( "BotVoice", 2 );
	gmsgBuyClose = REG_USER_MSG( "BuyClose", 0 );
	gmsgSpecHealth2 = REG_USER_MSG( "SpecHealth2", 2 );
	gmsgBarTime2 = REG_USER_MSG( "BarTime2", 4 );
	gmsgItemStatus = REG_USER_MSG( "ItemStatus", 1 );
	gmsgLocation = REG_USER_MSG( "Location", -1 );
	gmsgBotProgress = REG_USER_MSG( "BotProgress", -1 );
	gmsgBrass = REG_USER_MSG( "Brass", -1 );
	gmsgFog = REG_USER_MSG( "Fog", 7 );
	gmsgShowTimer = REG_USER_MSG( "ShowTimer", 0 );
	gmsgHudTextArgs = REG_USER_MSG( "HudTextArgs", -1 );
	gmsgAccount = REG_USER_MSG( "Account", 5 );
	gmsgHealthInfo = REG_USER_MSG( "HealthInfo", 5 );
}

int CBasePlayer::Classify()
{
	return CLASS_PLAYER;
}

int CBasePlayer::TakeHealth( float health, int damageType )
{
	return CBaseMonster::TakeHealth( health, damageType );
}

BOOL CBasePlayer::HasWeapons()
{
	for ( int i = 0; i < ARRAYSIZE( m_rgpPlayerItems ); i++ )
	{
		if ( m_rgpPlayerItems[i] != NULL )
			return TRUE;
	}
	return FALSE;
}

BOOL CBasePlayer::IsOnLadder()
{
	return pev->movetype == MOVETYPE_FLY;
}

BOOL CBasePlayer::FBecomeProne()
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return TRUE;
}

void CBasePlayer::BarnacleVictimReleased()
{
	m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
}

void CBasePlayer::BarnacleVictimBitten( entvars_t* barnacle )
{
	TakeDamage( barnacle, barnacle, pev->health + pev->armorvalue, DMG_SLASH | DMG_ALWAYSGIB );
}

int CBasePlayer::GetAmmoIndex( const char* name )
{
	if ( name == NULL )
		return -1;

	for ( int i = 1; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( CBasePlayerItem::AmmoInfoArray[i].pszName != NULL &&
			!stricmp( name, CBasePlayerItem::AmmoInfoArray[i].pszName ) )
			return i;
	}
	return -1;
}

int CBasePlayer::AmmoInventory( int index )
{
	return index == -1 ? -1 : m_rgAmmo[index];
}

int CBasePlayer::Illumination()
{
	int illumination = m_iWeaponFlash + GETENTITYILLUM( edict() );
	return illumination > 255 ? 255 : illumination;
}

void CBasePlayer::ItemPreFrame()
{
	if ( m_flNextAttack <= 0 && m_pActiveItem != NULL )
		m_pActiveItem->ItemPreFrame();
}

bool CBasePlayer::HasShield()
{
	return m_bOwnsShield;
}

void CBasePlayer::RemoveShield()
{
	if ( m_bOwnsShield )
	{
		m_bOwnsShield = false;
		m_bHasPrimary = false;
		m_bShieldDrawn = false;
		pev->gamestate = 1;
		m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
	}
}

void CBasePlayer::UpdateShieldCrosshair( bool draw )
{
	if ( draw )
		m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
	else
		m_iHideHUD |= HIDEHUD_CROSSHAIR;
}

void CBasePlayer::ResetAutoaim()
{
	if ( m_vecAutoAim.x != 0 || m_vecAutoAim.y != 0 )
	{
		m_vecAutoAim = g_vecZero;
		SET_CROSSHAIRANGLE( edict(), 0, 0 );
	}
	m_fOnTarget = FALSE;
}

Vector CBasePlayer::AutoaimDeflection( Vector&, float, float )
{
	m_fOnTarget = FALSE;
	return g_vecZero;
}

void CBasePlayer::ResetStamina()
{
	pev->fuser1 = 0;
	pev->fuser3 = 0;
	pev->fuser2 = 0;
}

void CBasePlayer::CheckPowerups( entvars_t* vars )
{
	if ( vars->health > 0 )
		vars->modelindex = m_modelIndexPlayer;
}

void CBasePlayer::EnableControl( BOOL control )
{
	if ( control )
		pev->flags &= ~FL_FROZEN;
	else
		pev->flags |= FL_FROZEN;
}

//Player ID
void CBasePlayer::InitStatusBar()
{
	m_flStatusBarDisappearDelay = 0;
	m_SbarString0[0] = '\0';
}

void CBasePlayer::SetSuitUpdate( char*, int, int )
{
}

BOOL CBasePlayer::FlashlightIsOn()
{
	return pev->effects & EF_DIMLIGHT;
}

Vector CBasePlayer::GetGunPosition()
{
	return pev->origin + pev->view_ofs;
}

void CBasePlayer::RemoveLevelText()
{
	MESSAGE_BEGIN( MSG_ONE, gmsgShowMenu, NULL, pev );
	WRITE_SHORT( 0 );
	WRITE_CHAR( 0 );
	WRITE_BYTE( 0 );
	WRITE_STRING( "" );
	MESSAGE_END();
}

void CBasePlayer::ResetMenu()
{
	MESSAGE_BEGIN( MSG_ONE, gmsgShowMenu, NULL, pev );
	WRITE_SHORT( 0 );
	WRITE_CHAR( 0 );
	WRITE_BYTE( 0 );
	WRITE_STRING( "" );
	MESSAGE_END();
}

void CBasePlayer::SetNewPlayerModel( const char* modelName )
{
	SET_MODEL( edict(), modelName );
	m_modelIndexPlayer = pev->modelindex;
}

BOOL CBasePlayer::ShouldDoLargeFlinch( int hitGroup, int gunType )
{
	if ( ( pev->flags & FL_DUCKING ) || hitGroup == HITGROUP_LEFTLEG || hitGroup == HITGROUP_RIGHTLEG )
		return FALSE;
	if ( (unsigned int)gunType > 28 )
		return FALSE;
	return ( ( 1 << gunType ) & 0x1D64E108 ) != 0;
}

void CBasePlayer::ForceClientDllUpdate()
{
	m_iClientHealth = -1;
	m_fInitHUD = TRUE;
	m_iTrain |= 0xC0;
	m_iClientBattery = -1;
	m_fWeapon = FALSE;
	UpdateClientData();
	HandleSignals();
}

int CBasePlayer::GetCustomDecalFrames()
{
	return m_nCustomSprayFrames;
}

void CBasePlayer::SetCustomDecalFrames( int frames )
{
	m_nCustomSprayFrames = (unsigned int)( frames - 1 ) <= 6 ? frames : -1;
}

void CBasePlayer::Duck()
{
	if ( pev->button & IN_DUCK )
		SetAnimation( PLAYER_WALK );
}

void CBasePlayer::Blind( float duration, float holdTime, float fadeTime, int alpha )
{
	m_blindUntilTime = gpGlobals->time + duration;
	m_blindHoldTime = holdTime;
	m_blindStartTime = gpGlobals->time;
	m_blindFadeTime = fadeTime;
	m_blindAlpha = alpha;
}

const char* CBasePlayer::TeamID()
{
	return pev == NULL ? "" : m_szTeamName;
}

void CBasePlayer::SendFOV( int fov )
{
	pev->fov = (float)fov;
	m_iClientFOV = (int)pev->fov;
	m_iFOV = (int)pev->fov;
	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
	WRITE_BYTE( fov );
	MESSAGE_END();
}

BOOL CBasePlayer::IsArmored( int hitGroup )
{
	if ( !m_iKevlar )
		return FALSE;
	if ( hitGroup == HITGROUP_HEAD )
		return m_iKevlar == 2;
	return hitGroup >= HITGROUP_GENERIC && hitGroup <= HITGROUP_RIGHTARM;
}

BOOL CBasePlayer::IsBombGuy()
{
	if ( g_pGameRules->IsMultiplayer() )
		return m_bHasC4;
	return FALSE;
}

void CBasePlayer::AddAccount( int amount, bool trackChange )
{
	m_iAccount += amount;
	if ( m_iAccount < 0 )
		m_iAccount = 0;
	else if ( m_iAccount > 16000 )
		m_iAccount = 16000;
	MESSAGE_BEGIN( MSG_ONE, gmsgMoney, NULL, pev );
	WRITE_LONG( m_iAccount );
	WRITE_BYTE( trackChange );
	MESSAGE_END();
}

void CBasePlayer::GiveNamedItem( const char* name )
{
	edict_t* item = CREATE_NAMED_ENTITY( MAKE_STRING( name ) );
	if ( FNullEnt( item ) )
	{
		ALERT( at_console, "NULL Ent in GiveNamedItem!\n" );
		return;
	}
	item->v.origin = pev->origin;
	item->v.spawnflags |= SF_NORESPAWN;
	item->v.effects |= EF_NODRAW;
	DispatchSpawn( item );
	DispatchTouch( item, edict() );
}

void CBasePlayer::RenewItems()
{
}

void CBasePlayer::SelectPrevItem( int )
{
}

void CBasePlayer::OnTouchingWeapon( CWeaponBox* )
{
}

void CBasePlayer::Precache()
{
	// in the event that the player JUST spawned, and the level node graph
	// was loaded, fix all of the node graph pointers before the game starts.

	// !!!BUGBUG - now that we have multiplayer, this needs to be moved!
	if ( WorldGraph.m_fGraphPresent && !WorldGraph.m_fGraphPointersSet )
	{
		if ( WorldGraph.FSetGraphPointers() )
			ALERT( at_console, "**Graph Pointers Set!\n" );
		else
			ALERT( at_console, "**Graph pointers were not set!\n" );
	}
	m_igeigerRangePrev = 1000;
	m_bitsDamageType = 0;
	m_bitsHUDDamage = -1;
	m_iClientBattery = -1;
	m_iTrain = 0xC0;
	m_flgeigerRange = 1000;
	if ( !gmsgCurWeapon )
		LinkUserMessages();
	m_iUpdateTime = 5;  // won't update for 1/2 a second
	if ( gInitHUD )
		m_fInitHUD = TRUE;
}

void CBasePlayer::DeathSound()
{
	// water death sounds
	/*
	if (pev->waterlevel == 3)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/h2odeath.wav", 1, ATTN_NONE);
		return;
	}
	*/

	// temporarily using pain sounds for death sounds
	switch ( RANDOM_LONG( 1, 4 ) )
	{
	case 1:
		EMIT_SOUND( edict(), CHAN_VOICE, "player/die1.wav", VOL_NORM, ATTN_NORM );
		break;
	case 2:
		EMIT_SOUND( edict(), CHAN_VOICE, "player/die2.wav", VOL_NORM, ATTN_NORM );
		break;
	case 3:
		EMIT_SOUND( edict(), CHAN_VOICE, "player/die3.wav", VOL_NORM, ATTN_NORM );
		break;
	case 4:
		EMIT_SOUND( edict(), CHAN_VOICE, "player/death6.wav", VOL_NORM, ATTN_NORM );
		break;
	}
}

bool CBasePlayer::HintMessage( const char* message, BOOL displayIfDead, BOOL overrideHints )
{
	if ( !displayIfDead && !IsAlive() )
		return false;
	if ( overrideHints || m_bShowHints )
		return m_hintMessageQueue.AddMessage( message, 6, true, NULL );
	return true;
}

BOOL CBasePlayer::HasPlayerItem( CBasePlayerItem* checkItem )
{
	CBasePlayerItem* item = m_rgpPlayerItems[checkItem->iItemSlot()];
	while ( item != NULL )
	{
		if ( !strcmp( STRING( item->pev->classname ), STRING( checkItem->pev->classname ) ) )
			return TRUE;
		item = item->m_pNext;
	}
	return FALSE;
}

BOOL CBasePlayer::HasNamedPlayerItem( const char* name )
{
	for ( int i = 0; i < ARRAYSIZE( m_rgpPlayerItems ); i++ )
	{
		for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item != NULL; item = item->m_pNext )
		{
			if ( !strcmp( name, STRING( item->pev->classname ) ) )
				return TRUE;
		}
	}
	return FALSE;
}

void CBasePlayer::ThrowWeapon( char* name )
{
	for ( int i = 0; i < 5; i++ )
	{
		for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item != NULL; item = item->m_pNext )
		{
			if ( !strcmp( name, STRING( item->pev->classname ) ) )
			{
				DropPlayerItem( name );
				return;
			}
		}
	}
}

void CBasePlayer::ResetMaxSpeed()
{
	if ( pev->iuser1 )
		pev->maxspeed = 900;
	else if ( g_pGameRules->IsMultiplayer() && g_pGameRules->IsFreezePeriod() )
		pev->maxspeed = 1;
	else if ( m_bIsVIP )
		pev->maxspeed = 227;
	else if ( m_pActiveItem == NULL )
		pev->maxspeed = 240;
	else
		pev->maxspeed = m_pActiveItem->GetMaxSpeed();
}

void CBasePlayer::SendAmmoUpdate()
{
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( m_rgAmmo[i] != m_rgAmmoLast[i] )
		{
			m_rgAmmoLast[i] = m_rgAmmo[i];
			int ammo = m_rgAmmo[i];
			if ( ammo > 254 )
				ammo = 254;
			else if ( ammo < 0 )
				ammo = 0;
			MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pev );
			WRITE_BYTE( i );
			WRITE_BYTE( ammo );
			MESSAGE_END();
		}
	}
}

void CBasePlayer::AddPoints( int score, BOOL allowNegative )
{
	if ( !allowNegative && score < 0 )
	{
		if ( pev->frags < 0 )		// Can't go more negative
			return;
		if ( -score > pev->frags )	// Will this go negative?
			score = (int)-pev->frags;
	}
	pev->frags += score;
	MESSAGE_BEGIN( MSG_BROADCAST, gmsgScoreInfo );
	WRITE_BYTE( entindex() );
	WRITE_SHORT( (int)pev->frags );
	WRITE_SHORT( m_iDeaths );
	WRITE_SHORT( 0 );
	WRITE_SHORT( m_iTeam );
	MESSAGE_END();
}

void CBasePlayer::AddPointsToTeam( int score, BOOL allowNegative )
{
	int index = entindex();
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity* player = UTIL_PlayerByIndex( i );
		if ( player != NULL && i != index && g_pGameRules->PlayerRelationship( this, player ) == GR_TEAMMATE )
			player->AddPoints( score, allowNegative );
	}
}

void CBasePlayer::FlashlightTurnOn()
{
	if ( g_pGameRules->FAllowFlashlight() && ( pev->weapons & ( 1 << WEAPON_SUIT ) ) )
	{
		EMIT_SOUND( edict(), CHAN_WEAPON, "items/flashlight1.wav", VOL_NORM, ATTN_NORM );
		pev->effects |= EF_DIMLIGHT;
		MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
		WRITE_BYTE( 1 );
		WRITE_BYTE( m_iFlashBattery );
		MESSAGE_END();
		m_flFlashLightTime = gpGlobals->time + 1.2f;
	}
}

void CBasePlayer::FlashlightTurnOff()
{
	EMIT_SOUND( edict(), CHAN_WEAPON, "items/flashlight1.wav", VOL_NORM, ATTN_NORM );
	pev->effects &= ~EF_DIMLIGHT;
	MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( m_iFlashBattery );
	MESSAGE_END();
	m_flFlashLightTime = gpGlobals->time + 0.2f;
}

void CBasePlayer::UpdateGeigerCounter()
{
	if ( m_flgeigerDelay <= gpGlobals->time )
	{
		m_flgeigerDelay = gpGlobals->time + 0.25f;
		int range = (unsigned char)(int)( m_flgeigerRange * 0.25f );
		if ( range != m_igeigerRangePrev )
		{
			m_igeigerRangePrev = range;
			MESSAGE_BEGIN( MSG_ONE, gmsgGeigerRange, NULL, pev );
			WRITE_BYTE( range );
			MESSAGE_END();
		}

		// reset counter and semaphore
		if ( RANDOM_LONG( 0, 3 ) == 0 )
			m_flgeigerRange = 1000;
	}
}

bool CBasePlayer::IsObservingPlayer( CBasePlayer* player )
{
	if ( player == NULL || pev->flags == FL_DORMANT || FNullEnt( player->edict() ) )
		return false;
	return pev->iuser1 == 4 && pev->iuser2 == player->entindex();
}

void CBasePlayer::MenuPrint( CBasePlayer* player, const char* message )
{
	char portion[51];
	while ( strlen( message ) > 49 )
	{
		strncpy( portion, message, 50 );
		message += 50;
		portion[50] = '\0';
		MESSAGE_BEGIN( MSG_ONE, gmsgShowMenu, NULL, player->pev );
		WRITE_SHORT( 0xFFFF );
		WRITE_CHAR( -1 );
		WRITE_BYTE( 1 );
		WRITE_STRING( portion );
		MESSAGE_END();
	}
	MESSAGE_BEGIN( MSG_ONE, gmsgShowMenu, NULL, player->pev );
	WRITE_SHORT( 0xFFFF );
	WRITE_CHAR( -1 );
	WRITE_BYTE( 0 );
	WRITE_STRING( message );
	MESSAGE_END();
}

WeaponStruct g_weaponStruct[32] =
{
	{ WEAPON_NONE, 0, 0, 0, 0 },
	{ WEAPON_P228, P228_PRICE, 3, 2, AMMO_357SIG_PRICE },
	{ WEAPON_SCOUT, SCOUT_PRICE, 3, 1, AMMO_762MM_PRICE },
	{ WEAPON_XM1014, XM1014_PRICE, 3, 1, AMMO_BUCKSHOT_PRICE },
	{ WEAPON_MAC10, MAC10_PRICE, 2, 1, AMMO_45ACP_PRICE },
	{ WEAPON_AUG, AUG_PRICE, 1, 1, AMMO_556MM_PRICE },
	{ WEAPON_ELITE, ELITE_PRICE, 2, 2, AMMO_9MM_PRICE },
	{ WEAPON_FIVESEVEN, FIVESEVEN_PRICE, 3, 2, AMMO_57MM_PRICE },
	{ WEAPON_UMP45, UMP45_PRICE, 1, 1, AMMO_45ACP_PRICE },
	{ WEAPON_SG550, SG550_PRICE, 1, 1, AMMO_556MM_PRICE },
	{ WEAPON_USP, USP_PRICE, 3, 2, AMMO_45ACP_PRICE },
	{ WEAPON_GLOCK18, GLOCK18_PRICE, 3, 2, AMMO_9MM_PRICE },
	{ WEAPON_MP5N, MP5NAVY_PRICE, 3, 1, AMMO_9MM_PRICE },
	{ WEAPON_AWP, AWP_PRICE, 3, 1, AMMO_338MAG_PRICE },
	{ WEAPON_M249, M249_PRICE, 3, 1, AMMO_556MM_PRICE },
	{ WEAPON_M3, M3_PRICE, 3, 1, AMMO_BUCKSHOT_PRICE },
	{ WEAPON_M4A1, M4A1_PRICE, 1, 1, AMMO_556MM_PRICE },
	{ WEAPON_TMP, TMP_PRICE, 1, 1, AMMO_9MM_PRICE },
	{ WEAPON_G3SG1, G3SG1_PRICE, 2, 1, AMMO_762MM_PRICE },
	{ WEAPON_DEAGLE, DEAGLE_PRICE, 3, 2, AMMO_50AE_PRICE },
	{ WEAPON_SG552, SG552_PRICE, 2, 1, AMMO_556MM_PRICE },
	{ WEAPON_AK47, AK47_PRICE, 2, 1, AMMO_762MM_PRICE },
	{ WEAPON_P90, P90_PRICE, 3, 1, AMMO_57MM_PRICE },
	{ WEAPON_FAMAS, FAMAS_PRICE, 1, 1, AMMO_556MM_PRICE },
	{ WEAPON_GALIL, 2250, 2, 1, AMMO_556MM_PRICE },
	{ WEAPON_SHIELDGUN, SHIELDGUN_PRICE, 1, 1, 0 }
};

static const char *const shotgunAutoBuyString = "xm1014 m3";
static const char *const sniperrifleAutoBuyString = "awp sg550 g3sg1 scout";
static const char *const smgAutoBuyString = "p90 ump45 mp5 tmp mac10";
static const char *const machinegunAutoBuyString = "m249";
static const char *const pistolAutoBuyString = "deagle elites fn57 usp glock p228 shield";
static const char *const rifleAutoBuyString = "sg552 aug ak47 m4a1 galil famas";
static const char *const grenadeAutoBuyString = "hegren";
static const char *const flashkillAutoBuyString = "flash flash";

bool CBasePlayer::CanAffordPrimary()
{
	int side;
	if (m_iTeam == CT)
		side = 1;
	else if (m_iTeam == TERRORIST)
		side = 2;
	else
		return false;

	for (int i = 0; i < ARRAYSIZE(g_weaponStruct); i++)
	{
		if ((g_weaponStruct[i].m_side & side) && g_weaponStruct[i].m_slot == 1 && m_iAccount >= g_weaponStruct[i].m_price)
			return true;
	}
	return false;
}

bool CBasePlayer::CanAffordPrimaryAmmo()
{
	CBasePlayerItem *primary = m_rgpPlayerItems[1];
	if (primary == NULL)
		return false;

	for (int i = 0; i < ARRAYSIZE(g_weaponStruct); i++)
	{
		if (g_weaponStruct[i].m_type == primary->m_iId && m_iAccount >= g_weaponStruct[i].m_ammoPrice)
			return true;
	}
	return false;
}

bool CBasePlayer::CanAffordSecondaryAmmo()
{
	CBasePlayerItem *secondary = m_rgpPlayerItems[2];
	if (secondary == NULL)
		return false;

	for (int i = 0; i < ARRAYSIZE(g_weaponStruct); i++)
	{
		if (g_weaponStruct[i].m_type == secondary->m_iId && m_iAccount >= g_weaponStruct[i].m_ammoPrice)
			return true;
	}
	return false;
}

bool CBasePlayer::CanAffordArmor()
{
	if (m_iKevlar == 1 && pev->armorvalue == 100)
		return m_iAccount >= HELMET_PRICE;
	return m_iAccount >= KEVLAR_PRICE;
}

bool CBasePlayer::CanAffordDefuseKit()
{
	return m_iAccount >= DEFUSEKIT_PRICE;
}

bool CBasePlayer::CanAffordGrenade()
{
	return m_iAccount >= FLASHBANG_PRICE;
}

bool CBasePlayer::NeedsPrimaryAmmo()
{
	CBasePlayerWeapon *primary = (CBasePlayerWeapon *)m_rgpPlayerItems[1];
	return primary != NULL && primary->m_iId != WEAPON_SHIELDGUN &&
		m_rgAmmo[primary->m_iPrimaryAmmoType] < CBasePlayerItem::ItemInfoArray[primary->m_iId].iMaxAmmo1;
}

bool CBasePlayer::NeedsSecondaryAmmo()
{
	CBasePlayerWeapon *secondary = (CBasePlayerWeapon *)m_rgpPlayerItems[2];
	return secondary != NULL &&
		m_rgAmmo[secondary->m_iPrimaryAmmoType] < CBasePlayerItem::ItemInfoArray[secondary->m_iId].iMaxAmmo1;
}

bool CBasePlayer::NeedsArmor()
{
	return m_iKevlar == 0 || pev->armorvalue < 50;
}

bool CBasePlayer::NeedsDefuseKit()
{
	if (m_bHasDefuser || m_iTeam != CT)
		return false;
	return ((CHalfLifeMultiplay *)g_pGameRules)->m_bMapHasBombTarget;
}

bool CBasePlayer::NeedsGrenade()
{
	int ammoIndex = GetAmmoIndex("HEGrenade");
	if (ammoIndex != -1 && m_rgAmmo[ammoIndex] != 0)
		return false;
	ammoIndex = GetAmmoIndex("Flashbang");
	if (ammoIndex != -1 && m_rgAmmo[ammoIndex] != 0)
		return false;
	ammoIndex = GetAmmoIndex("SmokeGrenade");
	return ammoIndex == -1 || m_rgAmmo[ammoIndex] == 0;
}

void CBasePlayer::ClientCommand(const char *command, const char *arg1, const char *arg2, const char *arg3)
{
	UseBotArgs = true;
	BotArgs[0] = command;
	BotArgs[1] = arg1;
	BotArgs[2] = arg2;
	BotArgs[3] = arg3;
	::ClientCommand(ENT(pev));
	UseBotArgs = false;
}

const char *GetBuyStringForWeaponClass(int weaponClass)
{
	switch (weaponClass)
	{
	case WEAPONCLASS_PISTOL:
		return pistolAutoBuyString;
	case WEAPONCLASS_GRENADE:
		return grenadeAutoBuyString;
	case WEAPONCLASS_SUBMACHINEGUN:
		return smgAutoBuyString;
	case WEAPONCLASS_SHOTGUN:
		return shotgunAutoBuyString;
	case WEAPONCLASS_MACHINEGUN:
		return machinegunAutoBuyString;
	case WEAPONCLASS_RIFLE:
		return rifleAutoBuyString;
	case WEAPONCLASS_SNIPERRIFLE:
		return sniperrifleAutoBuyString;
	default:
		return NULL;
	}
}

bool IsPrimaryWeaponClass(int weaponClass)
{
	return weaponClass >= WEAPONCLASS_SUBMACHINEGUN && weaponClass <= WEAPONCLASS_SNIPERRIFLE;
}

bool IsPrimaryWeaponId(int weaponId)
{
	const char *alias = WeaponIDToAlias(weaponId);
	return alias != NULL && IsPrimaryWeaponClass(AliasToWeaponClass(alias));
}

bool IsSecondaryWeaponClass(int weaponClass)
{
	return weaponClass == WEAPONCLASS_PISTOL;
}

bool IsSecondaryWeaponId(int weaponId)
{
	const char *alias = WeaponIDToAlias(weaponId);
	return alias != NULL && IsSecondaryWeaponClass(AliasToWeaponClass(alias));
}

const char *GetWeaponAliasFromName(const char *weaponName)
{
	if (memcmp("weapon_", weaponName, 7) == 0)
		return weaponName + 7;
	return weaponName;
}

bool CurrentWeaponSatisfies(CBasePlayerWeapon *weapon, int weaponId, int weaponClass)
{
	if (weapon == NULL)
		return false;
	const char *alias = GetWeaponAliasFromName(CBasePlayerItem::ItemInfoArray[weapon->m_iId].pszName);
	if (weaponId != 0 && weaponId == AliasToWeaponID(alias))
		return true;
	return weaponClass != 0 && weaponClass == AliasToWeaponClass(alias);
}

void CBasePlayer::ClearAutoBuyData()
{
	m_autoBuyString[0] = '\0';
}

void CBasePlayer::AddAutoBuyData(const char *data)
{
	int length = strlen(m_autoBuyString);
	if (length <= 254)
	{
		if (length > 0)
		{
			m_autoBuyString[length++] = ' ';
			m_autoBuyString[length] = '\0';
		}
		strncat(m_autoBuyString, data, sizeof(m_autoBuyString) - length);
	}
}

void CBasePlayer::InitRebuyData(const char *data)
{
	if (data != NULL && strlen(data) <= 256)
	{
		if (m_rebuyString != NULL)
		{
			delete[] m_rebuyString;
			m_rebuyString = NULL;
		}
		m_rebuyString = new char[strlen(data) + 1];
		strcpy(m_rebuyString, data);
		m_rebuyString[strlen(data)] = '\0';
	}
}

void CBasePlayer::PrioritizeAutoBuyString(char *autobuyString, const char *priorityString)
{
	if (autobuyString == NULL || priorityString == NULL)
		return;

	char priorityToken[32];
	char newString[256];
	int newLength = 0;
	const char *priority = priorityString;
	while (*priority != '\0')
	{
		int tokenLength = 0;
		while (*priority != '\0' && *priority != ' ')
			priorityToken[tokenLength++] = *priority++;
		priorityToken[tokenLength] = '\0';
		while (*priority == ' ')
			priority++;
		if (priorityToken[0] == '\0')
			continue;

		char *token = strstr(autobuyString, priorityToken);
		if (token != NULL)
		{
			while (*token != '\0' && *token != ' ')
			{
				newString[newLength++] = *token;
				*token++ = ' ';
			}
			newString[newLength++] = ' ';
		}
	}

	char *token = autobuyString;
	while (*token != '\0')
	{
		while (*token == ' ')
			token++;
		while (*token != '\0' && *token != ' ')
			newString[newLength++] = *token++;
		newString[newLength++] = ' ';
	}
	newString[newLength] = '\0';
	strcpy(autobuyString, newString);
}

AutoBuyInfoStruct *CBasePlayer::GetAutoBuyCommandInfo(const char *command)
{
	int i = 0;
	AutoBuyInfoStruct *info = g_autoBuyInfo;
	while (info->m_class != 0)
	{
		info = &g_autoBuyInfo[i++];
		if (stricmp(info->m_command, command) == 0)
			return info;
	}
	return NULL;
}

bool CBasePlayer::ShouldExecuteAutoBuyCommand(const AutoBuyInfoStruct *commandInfo, bool boughtPrimary, bool boughtSecondary)
{
	if (commandInfo == NULL)
		return false;
	if (boughtPrimary && (commandInfo->m_class & (AUTOBUYCLASS_PRIMARY | AUTOBUYCLASS_AMMO)) == AUTOBUYCLASS_PRIMARY)
		return false;
	if (boughtSecondary && (commandInfo->m_class & (AUTOBUYCLASS_SECONDARY | AUTOBUYCLASS_AMMO)) == AUTOBUYCLASS_SECONDARY)
		return false;
	return true;
}

void CBasePlayer::PostAutoBuyCommandProcessing(const AutoBuyInfoStruct *commandInfo, bool &boughtPrimary, bool &boughtSecondary)
{
	if (commandInfo == NULL)
		return;
	CBasePlayerItem *primary = m_rgpPlayerItems[1];
	if (primary != NULL)
	{
		if (stricmp(STRING(primary->pev->classname), commandInfo->m_classname) == 0)
		{
			boughtPrimary = true;
			return;
		}
	}
	else if ((commandInfo->m_class & AUTOBUYCLASS_SHIELD) && m_bOwnsShield)
	{
		boughtPrimary = true;
		return;
	}
	CBasePlayerItem *secondary = m_rgpPlayerItems[2];
	if (secondary != NULL && stricmp(STRING(secondary->pev->classname), commandInfo->m_classname) == 0)
		boughtSecondary = true;
}

void CBasePlayer::ParseAutoBuyString(const char *string, bool &boughtPrimary, bool &boughtSecondary)
{
	if (string == NULL)
		return;
	const char *c = string;
	while (*c != '\0')
	{
		char command[32];
		int length = 0;
		while (*c != '\0' && *c != ' ' && length < sizeof(command) - 1)
			command[length++] = *c++;
		if (*c == ' ')
			c++;
		command[length] = '\0';
		if (command[0] == '\0')
			continue;
		for (int i = 0; command[i] != '\0'; i++)
		{
			if (command[i] == ' ')
			{
				command[i] = '\0';
				break;
			}
		}
		if (command[0] == '\0')
			continue;
		AutoBuyInfoStruct *commandInfo = GetAutoBuyCommandInfo(command);
		if (ShouldExecuteAutoBuyCommand(commandInfo, boughtPrimary, boughtSecondary))
		{
			ClientCommand(command, NULL, NULL, NULL);
			PostAutoBuyCommandProcessing(commandInfo, boughtPrimary, boughtSecondary);
		}
	}
}

void CBasePlayer::BuildRebuyStruct()
{
	if (m_bIsInRebuy)
		return;
	CBasePlayerWeapon *primary = (CBasePlayerWeapon *)m_rgpPlayerItems[1];
	CBasePlayerWeapon *secondary = (CBasePlayerWeapon *)m_rgpPlayerItems[2];
	if (primary != NULL)
	{
		m_rebuyStruct.m_primaryWeapon = primary->m_iId;
		m_rebuyStruct.m_primaryAmmo = m_rgAmmo[primary->m_iPrimaryAmmoType];
	}
	else
	{
		m_rebuyStruct.m_primaryWeapon = m_bOwnsShield ? WEAPON_SHIELDGUN : WEAPON_NONE;
		m_rebuyStruct.m_primaryAmmo = 0;
	}
	if (secondary != NULL)
	{
		m_rebuyStruct.m_secondaryWeapon = secondary->m_iId;
		m_rebuyStruct.m_secondaryAmmo = m_rgAmmo[secondary->m_iPrimaryAmmoType];
	}
	else
	{
		m_rebuyStruct.m_secondaryWeapon = WEAPON_NONE;
		m_rebuyStruct.m_secondaryAmmo = 0;
	}
	int ammoIndex = GetAmmoIndex("HEGrenade");
	m_rebuyStruct.m_heGrenade = ammoIndex == -1 ? 0 : m_rgAmmo[ammoIndex];
	ammoIndex = GetAmmoIndex("Flashbang");
	m_rebuyStruct.m_flashbang = ammoIndex == -1 ? 0 : m_rgAmmo[ammoIndex];
	ammoIndex = GetAmmoIndex("SmokeGrenade");
	m_rebuyStruct.m_smokeGrenade = ammoIndex == -1 ? 0 : m_rgAmmo[ammoIndex];
	m_rebuyStruct.m_defuser = m_bHasDefuser;
	m_rebuyStruct.m_nightVision = m_bHasNightVision;
	m_rebuyStruct.m_armor = m_iKevlar;
}

void CBasePlayer::RebuyPrimaryWeapon()
{
	if (m_rgpPlayerItems[1] == NULL && m_rebuyStruct.m_primaryWeapon != WEAPON_NONE)
	{
		const char *alias = WeaponIDToAlias(m_rebuyStruct.m_primaryWeapon);
		if (alias != NULL)
			ClientCommand(alias, NULL, NULL, NULL);
	}
}

void CBasePlayer::RebuyPrimaryAmmo()
{
	CBasePlayerWeapon *primary = (CBasePlayerWeapon *)m_rgpPlayerItems[1];
	if (primary != NULL && m_rebuyStruct.m_primaryAmmo > m_rgAmmo[primary->m_iPrimaryAmmoType])
		ClientCommand("primammo", NULL, NULL, NULL);
}

void CBasePlayer::RebuySecondaryWeapon()
{
	if (m_rebuyStruct.m_secondaryWeapon != WEAPON_NONE)
	{
		const char *alias = WeaponIDToAlias(m_rebuyStruct.m_secondaryWeapon);
		if (alias != NULL)
			ClientCommand(alias, NULL, NULL, NULL);
	}
}

void CBasePlayer::RebuySecondaryAmmo()
{
	CBasePlayerWeapon *secondary = (CBasePlayerWeapon *)m_rgpPlayerItems[2];
	if (secondary != NULL && m_rebuyStruct.m_secondaryAmmo > m_rgAmmo[secondary->m_iPrimaryAmmoType])
		ClientCommand("secammo", NULL, NULL, NULL);
}

void CBasePlayer::RebuyHEGrenade()
{
	int ammoIndex = GetAmmoIndex("HEGrenade");
	if (ammoIndex == -1)
		return;
	int count = m_rebuyStruct.m_heGrenade - m_rgAmmo[ammoIndex];
	for (int i = 0; i < count; i++)
		ClientCommand("hegren", NULL, NULL, NULL);
}

void CBasePlayer::RebuyFlashbang()
{
	int ammoIndex = GetAmmoIndex("Flashbang");
	if (ammoIndex == -1)
		return;
	int count = m_rebuyStruct.m_flashbang - m_rgAmmo[ammoIndex];
	for (int i = 0; i < count; i++)
		ClientCommand("flash", NULL, NULL, NULL);
}

void CBasePlayer::RebuySmokeGrenade()
{
	int ammoIndex = GetAmmoIndex("SmokeGrenade");
	if (ammoIndex == -1)
		return;
	int count = m_rebuyStruct.m_smokeGrenade - m_rgAmmo[ammoIndex];
	for (int i = 0; i < count; i++)
		ClientCommand("sgren", NULL, NULL, NULL);
}

void CBasePlayer::RebuyDefuser()
{
	if (m_rebuyStruct.m_defuser != 0 && !m_bHasDefuser)
		ClientCommand("defuser", NULL, NULL, NULL);
}

void CBasePlayer::RebuyNightVision()
{
	if (m_rebuyStruct.m_nightVision != 0 && !m_bHasNightVision)
		ClientCommand("nvgs", NULL, NULL, NULL);
}

void CBasePlayer::RebuyArmor()
{
	if (m_rebuyStruct.m_armor != 0 && m_rebuyStruct.m_armor > m_iKevlar)
	{
		if (m_rebuyStruct.m_armor == 1)
			ClientCommand("vest", NULL, NULL, NULL);
		else
			ClientCommand("vesthelm", NULL, NULL, NULL);
	}
}

void CBasePlayer::Rebuy()
{
	m_bIsInRebuy = true;
	char *data = MP_COM_Parse(m_rebuyString);
	const char *token = MP_COM_GetToken();
	while (data != NULL)
	{
		if (stricmp(token, "primaryWeapon") == 0)
			RebuyPrimaryWeapon();
		else if (stricmp(token, "primaryAmmo") == 0)
			RebuyPrimaryAmmo();
		else if (stricmp(token, "secondaryWeapon") == 0)
			RebuySecondaryWeapon();
		else if (stricmp(token, "secondaryAmmo") == 0)
			RebuySecondaryAmmo();
		else if (stricmp(token, "hegrenade") == 0)
			RebuyHEGrenade();
		else if (stricmp(token, "flashbang") == 0)
			RebuyFlashbang();
		else if (stricmp(token, "smokegrenade") == 0)
			RebuySmokeGrenade();
		else if (stricmp(token, "defuser") == 0)
			RebuyDefuser();
		else if (stricmp(token, "nightvision") == 0)
			RebuyNightVision();
		else if (stricmp(token, "armor") == 0)
			RebuyArmor();
		data = MP_COM_Parse(data);
		token = MP_COM_GetToken();
	}
	m_bIsInRebuy = false;
	if (TheTutor != NULL)
		TheTutor->OnEvent(EVENT_PLAYER_LEFT_BUY_ZONE, NULL, NULL);
}

const char *CBasePlayer::PickFlashKillWeaponString()
{
	CareerTaskList *tasks = TheCareerTasks->GetTasks();
	for (CareerTaskListIt it = tasks->begin(); it != tasks->end(); ++it)
	{
		CCareerTask *task = *it;
		if (!task->IsComplete() && stricmp(task->GetTaskName(), "killblind") == 0)
			return flashkillAutoBuyString;
	}
	return NULL;
}

const char *CBasePlayer::PickGrenadeKillWeaponString()
{
	CareerTaskList *tasks = TheCareerTasks->GetTasks();
	for (CareerTaskListIt it = tasks->begin(); it != tasks->end(); ++it)
	{
		CCareerTask *task = *it;
		if (!task->IsComplete() && task->GetWeaponId() == WEAPON_HEGRENADE)
			return grenadeAutoBuyString;
	}
	return NULL;
}

const char *CBasePlayer::PickPrimaryCareerTaskWeapon()
{
	if (TheCareerTasks == NULL)
		return NULL;
	CBasePlayerWeapon *primary = (CBasePlayerWeapon *)m_rgpPlayerItems[1];
	std::vector<CCareerTask *> taskVector;
	CareerTaskList *tasks = TheCareerTasks->GetTasks();
	for (CareerTaskListIt it = tasks->begin(); it != tasks->end(); ++it)
	{
		CCareerTask *task = *it;
		if (task->IsComplete() || task->GetWeaponId() == WEAPON_HEGRENADE)
			continue;
		if (!IsPrimaryWeaponId(task->GetWeaponId()) && !IsPrimaryWeaponClass(task->GetWeaponClassId()))
			continue;
		if (CurrentWeaponSatisfies(primary, task->GetWeaponId(), task->GetWeaponClassId()))
		{
			if (IsPrimaryWeaponId(task->GetWeaponId()))
				return WeaponIDToAlias(task->GetWeaponId());
			return GetBuyStringForWeaponClass(task->GetWeaponClassId());
		}
		taskVector.push_back(task);
	}

	if (taskVector.size() > 1)
	{
		int index = RANDOM_LONG(0, taskVector.size() - 1);
		CCareerTask *task = taskVector[0];
		taskVector[0] = taskVector[index];
		taskVector[index] = task;
	}
	static char buf[256];
	buf[0] = '\0';
	for (unsigned int i = 0; i < taskVector.size(); i++)
	{
		CCareerTask *task = taskVector[i];
		if (IsPrimaryWeaponId(task->GetWeaponId()))
			strncat(buf, WeaponIDToAlias(task->GetWeaponId()), sizeof(buf) - strlen(buf) - 1);
		else if (IsPrimaryWeaponClass(task->GetWeaponClassId()))
			strncat(buf, GetBuyStringForWeaponClass(task->GetWeaponClassId()), sizeof(buf) - strlen(buf) - 1);
		strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
	}
	return buf[0] != '\0' ? buf : NULL;
}

const char *CBasePlayer::PickSecondaryCareerTaskWeapon()
{
	if (TheCareerTasks == NULL)
		return NULL;
	CBasePlayerWeapon *secondary = (CBasePlayerWeapon *)m_rgpPlayerItems[2];
	std::vector<CCareerTask *> taskVector;
	CareerTaskList *tasks = TheCareerTasks->GetTasks();
	for (CareerTaskListIt it = tasks->begin(); it != tasks->end(); ++it)
	{
		CCareerTask *task = *it;
		if (task->IsComplete() || task->GetWeaponId() == WEAPON_HEGRENADE)
			continue;
		if (!IsSecondaryWeaponId(task->GetWeaponId()) && !IsSecondaryWeaponClass(task->GetWeaponClassId()))
			continue;
		if (CurrentWeaponSatisfies(secondary, task->GetWeaponId(), task->GetWeaponClassId()))
		{
			if (IsSecondaryWeaponId(task->GetWeaponId()))
				return WeaponIDToAlias(task->GetWeaponId());
			return GetBuyStringForWeaponClass(task->GetWeaponClassId());
		}
		taskVector.push_back(task);
	}

	if (taskVector.size() > 1)
	{
		int index = RANDOM_LONG(0, taskVector.size() - 1);
		CCareerTask *task = taskVector[0];
		taskVector[0] = taskVector[index];
		taskVector[index] = task;
	}
	static char buf[256];
	buf[0] = '\0';
	for (unsigned int i = 0; i < taskVector.size(); i++)
	{
		CCareerTask *task = taskVector[i];
		if (IsSecondaryWeaponId(task->GetWeaponId()))
			strncat(buf, WeaponIDToAlias(task->GetWeaponId()), sizeof(buf) - strlen(buf) - 1);
		else if (IsSecondaryWeaponClass(task->GetWeaponClassId()))
			strncat(buf, pistolAutoBuyString, sizeof(buf) - strlen(buf) - 1);
		strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
	}
	return buf[0] != '\0' ? buf : NULL;
}

void CBasePlayer::AutoBuy()
{
	bool boughtPrimary = false;
	bool boughtSecondary = false;
	ParseAutoBuyString(PickFlashKillWeaponString(), boughtPrimary, boughtSecondary);
	ParseAutoBuyString(PickGrenadeKillWeaponString(), boughtPrimary, boughtSecondary);

	char prioritizedString[256];
	const char *primary = PickPrimaryCareerTaskWeapon();
	if (primary != NULL)
	{
		strcpy(prioritizedString, primary);
		PrioritizeAutoBuyString(prioritizedString, m_autoBuyString);
		ParseAutoBuyString(prioritizedString, boughtPrimary, boughtSecondary);
	}
	const char *secondary = PickSecondaryCareerTaskWeapon();
	if (secondary != NULL)
	{
		strcpy(prioritizedString, secondary);
		PrioritizeAutoBuyString(prioritizedString, m_autoBuyString);
		ParseAutoBuyString(prioritizedString, boughtPrimary, boughtSecondary);
	}
	ParseAutoBuyString(m_autoBuyString, boughtPrimary, boughtSecondary);
	ParseAutoBuyString(PickFlashKillWeaponString(), boughtPrimary, boughtSecondary);
	if (TheTutor != NULL)
		TheTutor->OnEvent(EVENT_PLAYER_LEFT_BUY_ZONE, NULL, NULL);
}


//==============================================
// !!!UNDONE:ultra temporary SprayCan entity to apply
// decal frame at a time. For PreAlpha CD
//==============================================
class CSprayCan : public CBaseEntity
{
public:
	void Spawn( entvars_t* owner );
	void Think() override;
	int ObjectCaps() override { return FCAP_DONT_SAVE; }
};

void CSprayCan::Spawn( entvars_t* owner )
{
	pev->origin = owner->origin + Vector( 0, 0, 32 );
	pev->angles = owner->v_angle;
	pev->owner = ENT( owner );
	pev->frame = 0;
	pev->nextthink = gpGlobals->time + 0.1f;
	EMIT_SOUND( edict(), CHAN_VOICE, "player/sprayer.wav", VOL_NORM, ATTN_NORM );
}

void CSprayCan::Think()
{
	CBasePlayer* player = (CBasePlayer*)GET_PRIVATE( pev->owner );
	int frames = player != NULL ? player->GetCustomDecalFrames() : -1;
	int playerIndex = ENTINDEX( pev->owner );
	TraceResult trace;
	UTIL_MakeVectors( pev->angles );
	UTIL_TraceLine( pev->origin, pev->origin + gpGlobals->v_forward * 128,
		ignore_monsters, pev->owner, &trace );
	if ( frames == -1 )
	{
		UTIL_DecalTrace( &trace, DECAL_LAMBDA6 );
		UTIL_Remove( this );
	}
	else
	{
		UTIL_PlayerDecalTrace( &trace, playerIndex, (int)pev->frame, TRUE );
		if ( pev->frame++ >= frames - 1 )
			UTIL_Remove( this );
	}
	pev->nextthink = gpGlobals->time + 0.1f;
}

class CBloodSplat : public CBaseEntity
{
public:
	void Spawn( entvars_t* owner );
	void Spray();
};

void CBloodSplat::Spawn( entvars_t* owner )
{
	pev->origin = owner->origin + Vector( 0, 0, 32 );
	pev->angles = owner->v_angle;
	pev->owner = ENT( owner );
	SetThink( &CBloodSplat::Spray );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CBloodSplat::Spray()
{
	if ( g_Language != LANGUAGE_GERMAN )
	{
		TraceResult trace;
		UTIL_MakeVectors( pev->angles );
		UTIL_TraceLine( pev->origin, pev->origin + gpGlobals->v_forward * 128,
			ignore_monsters, pev->owner, &trace );
		UTIL_BloodDecalTrace( &trace, BLOOD_COLOR_RED );
	}
	SetThink( &CBloodSplat::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1f;
}

//=========================================================
// Dead HEV suit prop
//=========================================================
class CDeadHEV : public CBaseMonster
{
public:
	void Spawn() override;
	int Classify() override { return CLASS_HUMAN_MILITARY; }
	void KeyValue( KeyValueData* data ) override;
	int m_iPose;
	static char* m_szPoses[4];
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

char* CDeadHEV::m_szPoses[4] = { "deadback", "deadsitting", "deadstomach", "deadtable" };

LINK_ENTITY_TO_CLASS( monster_hevsuit_dead, CDeadHEV );

void CDeadHEV::KeyValue( KeyValueData* data )
{
	if ( FStrEq( data->szKeyName, "pose" ) )
	{
		m_iPose = atoi( data->szValue );
		data->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( data );
}

void CDeadHEV::Spawn()
{
	PRECACHE_MODEL( "models/player.mdl" );
	SET_MODEL( edict(), "models/player.mdl" );
	pev->effects = 0;
	pev->yaw_speed = 8;
	pev->sequence = 0;
	pev->body = 1;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->sequence = LookupSequence( m_szPoses[m_iPose] );
	if ( pev->sequence == -1 )
	{
		ALERT( at_console, "Dead hevsuit with bad pose\n" );
		pev->sequence = 0;
		pev->effects = EF_BRIGHTFIELD;
	}

	// Corpses have less health
	pev->health = 8;
	MonsterInitDead();
}

class CStripWeapons : public CPointEntity
{
public:
	void Use( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE useType, float value ) override;
};

LINK_ENTITY_TO_CLASS( player_weaponstrip, CStripWeapons );

void CStripWeapons::Use( CBaseEntity* activator, CBaseEntity*, USE_TYPE, float )
{
	CBasePlayer* player = NULL;
	if ( activator != NULL && activator->IsPlayer() )
		player = (CBasePlayer*)activator;
	else if ( !g_pGameRules->IsDeathmatch() )
		player = (CBasePlayer*)CBaseEntity::Instance( INDEXENT( 1 ) );
	if ( player != NULL )
		player->RemoveAllItems( FALSE );
}

class CRevertSaved : public CPointEntity
{
public:
	void Use( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE useType, float value ) override;
	void EXPORT MessageThink();
	void EXPORT LoadThink();
	void KeyValue( KeyValueData* data ) override;
	int Save( CSave& save ) override;
	int Restore( CRestore& restore ) override;
	static TYPEDESCRIPTION m_SaveData[];

private:
	float m_messageTime;
	float m_loadTime;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

LINK_ENTITY_TO_CLASS( player_loadsaved, CRevertSaved );

TYPEDESCRIPTION CRevertSaved::m_SaveData[] =
{
	DEFINE_FIELD( CRevertSaved, m_messageTime, FIELD_FLOAT ),	// These are not actual times, but durations, so save as floats
	DEFINE_FIELD( CRevertSaved, m_loadTime, FIELD_FLOAT )
};

IMPLEMENT_SAVERESTORE( CRevertSaved, CPointEntity );

void CRevertSaved::KeyValue( KeyValueData* data )
{
	if ( FStrEq( data->szKeyName, "duration" ) )
	{
		pev->dmg_take = (float)atof( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "holdtime" ) )
	{
		pev->dmg_save = (float)atof( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "messagetime" ) )
	{
		m_messageTime = (float)atof( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "loadtime" ) )
	{
		m_loadTime = (float)atof( data->szValue );
		data->fHandled = TRUE;
	}
	else
		data->fHandled = FALSE;
}

void CRevertSaved::Use( CBaseEntity*, CBaseEntity*, USE_TYPE, float )
{
	UTIL_ScreenFadeAll( pev->rendercolor, pev->dmg_take, pev->dmg_save, (int)pev->renderamt, FFADE_OUT );
	pev->nextthink = gpGlobals->time + m_messageTime;
	SetThink( &CRevertSaved::MessageThink );
}

void CRevertSaved::MessageThink()
{
	UTIL_ShowMessageAll( STRING( pev->message ), false );
	float delay = m_loadTime - m_messageTime;
	if ( delay <= 0 )
		LoadThink();
	else
	{
		pev->nextthink = gpGlobals->time + delay;
		SetThink( &CRevertSaved::LoadThink );
	}
}

void CRevertSaved::LoadThink()
{
	if ( !gpGlobals->deathmatch )
		SERVER_COMMAND( "reload\n" );
}


//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission : public CPointEntity
{
	void Spawn() override;
	void Think() override;
};

LINK_ENTITY_TO_CLASS( info_intermission, CInfoIntermission );

void CInfoIntermission::Spawn()
{
	UTIL_SetOrigin( pev, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
	pev->nextthink = gpGlobals->time + 2; // let targets spawn!
}

void CInfoIntermission::Think()
{
	edict_t* target = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );
	if ( !FNullEnt( target ) )
	{
		pev->v_angle = UTIL_VecToAngles( ( target->v.origin - pev->origin ).Normalize() );
		pev->v_angle.x = -pev->v_angle.x;
	}
}

void SendItemStatus( CBasePlayer* player )
{
	int status = ( 2 * player->m_bHasDefuser ) | (int)player->m_bHasNightVision;
	MESSAGE_BEGIN( MSG_ONE, gmsgItemStatus, NULL, player->pev );
	WRITE_BYTE( status );
	MESSAGE_END();
}

int TrainSpeed( int speed, int maximum )
{
	if ( speed < 0 )
		return 5;
	if ( speed == 0 )
		return 1;
	double ratio = (double)speed / maximum;
	if ( ratio < 0.33 )
		return 2;
	return ratio >= 0.66 ? 4 : 3;
}

CBaseEntity* FindEntityForward( CBaseEntity* entity )
{
	TraceResult trace;
	UTIL_MakeVectors( entity->pev->v_angle );
	Vector start = entity->pev->origin + entity->pev->view_ofs;
	UTIL_TraceLine( start, start + gpGlobals->v_forward * 8192,
		dont_ignore_monsters, entity->edict(), &trace );
	if ( trace.flFraction != 1 && !FNullEnt( trace.pHit ) )
		return CBaseEntity::Instance( trace.pHit );
	return NULL;
}

BOOL IsSpawnPointValid( CBaseEntity* player, CBaseEntity* spot )
{
	if ( !spot->IsTriggered( player ) )
		return FALSE;
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityInSphere( entity, spot->pev->origin, 64 ) ) != NULL )
	{
		// if ent is a client, don't spawn on 'em
		if ( entity->IsPlayer() && entity != player )
			return FALSE;
	}
	return TRUE;
}

void FixPlayerCrouchStuck( edict_t* player )
{
	TraceResult trace;

	// Move up as many as 18 pixels if the player is stuck.
	for ( int i = 0; i < 18; i++ )
	{
		UTIL_TraceHull( player->v.origin, player->v.origin, dont_ignore_monsters, head_hull, player, &trace );
		if ( !trace.fStartSolid )
			break;
		player->v.origin.z++;
	}
}

Vector VecVelocityForDamage( float damage )
{
	Vector velocity;
	velocity.z = RANDOM_FLOAT( 200, 300 );
	velocity.y = RANDOM_FLOAT( -100, 100 );
	velocity.x = RANDOM_FLOAT( -100, 100 );
	if ( damage > -50 )
		velocity = velocity * 0.7f;
	else if ( damage > -200 )
		velocity = velocity * 2;
	else
		velocity = velocity * 10;
	return velocity;
}

float GetPlayerPitch( const edict_t* edict )
{
	if ( edict == NULL )
		return 0;
	CBasePlayer* player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( (entvars_t*)&edict->v ) );
	return player != NULL ? player->m_flPitch : 0;
}

float GetPlayerYaw( const edict_t* edict )
{
	if ( edict == NULL )
		return 0;
	CBasePlayer* player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( (entvars_t*)&edict->v ) );
	return player != NULL ? player->m_flYaw : 0;
}

int GetPlayerGaitsequence( const edict_t* edict )
{
	if ( edict == NULL )
		return 1;
	CBasePlayer* player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( (entvars_t*)&edict->v ) );
	return player != NULL ? player->m_iGaitsequence : 1;
}

void CBasePlayer::SetProgressBarTime( int time )
{
	if ( time != 0 )
	{
		m_progressStart = gpGlobals->time;
		m_progressEnd = gpGlobals->time + time;
	}
	else
	{
		m_progressStart = 0;
		m_progressEnd = 0;
	}
	MESSAGE_BEGIN( MSG_ONE, gmsgBarTime, NULL, pev );
	WRITE_SHORT( time );
	MESSAGE_END();
	int index = entindex();
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
		if ( player->pev->iuser1 == 4 && player->pev->iuser2 == index )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBarTime, NULL, player->pev );
			WRITE_SHORT( time );
			MESSAGE_END();
		}
	}
}

void CBasePlayer::SetProgressBarTime2( int time, float elapsed )
{
	float duration;
	float progress;
	if ( time != 0 )
	{
		m_progressStart = gpGlobals->time - elapsed;
		m_progressEnd = gpGlobals->time + time - elapsed;
		progress = elapsed * 100;
		duration = m_progressEnd - m_progressStart;
	}
	else
	{
		m_progressStart = 0;
		m_progressEnd = 0;
		duration = 0;
		progress = 0;
	}
	MESSAGE_BEGIN( MSG_ONE, gmsgBarTime2, NULL, pev );
	WRITE_SHORT( time );
	progress /= duration;
	WRITE_SHORT( (short)(int)progress );
	MESSAGE_END();
	int index = entindex();
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
		if ( player->pev->iuser1 == 4 && player->pev->iuser2 == index )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBarTime2, NULL, player->pev );
			WRITE_SHORT( time );
			WRITE_SHORT( (short)(int)progress );
			MESSAGE_END();
		}
	}
}

/*
===========
TabulateAmmo
This function is used to find and store 
all the ammo we have into the ammo vars.
============
*/
void CBasePlayer::TabulateAmmo()
{
	ammo_buckshot = AmmoInventory( GetAmmoIndex( "buckshot" ) );
	ammo_9mm = AmmoInventory( GetAmmoIndex( "9mm" ) );
	ammo_556nato = AmmoInventory( GetAmmoIndex( "556Nato" ) );
	ammo_556natobox = AmmoInventory( GetAmmoIndex( "556NatoBox" ) );
	ammo_762nato = AmmoInventory( GetAmmoIndex( "762Nato" ) );
	ammo_45acp = AmmoInventory( GetAmmoIndex( "45acp" ) );
	ammo_50ae = AmmoInventory( GetAmmoIndex( "50AE" ) );
	ammo_338mag = AmmoInventory( GetAmmoIndex( "338Magnum" ) );
	ammo_57mm = AmmoInventory( GetAmmoIndex( "57mm" ) );
	ammo_357sig = AmmoInventory( GetAmmoIndex( "357SIG" ) );
}

int CBasePlayer::GiveAmmo( int count, char* name, int maximum )
{
	if ( pev->flags & FL_SPECTATOR || name == NULL || !g_pGameRules->CanHaveAmmo( this, name, maximum ) )
		return -1;
	int index = GetAmmoIndex( name );
	if ( index == -1 )
		return -1;
	int added = maximum - m_rgAmmo[index];
	if ( added > count )
		added = count;
	if ( added <= 0 )
		return index;
	m_rgAmmo[index] += added;
	if ( gmsgAmmoPickup )  // make sure the ammo messages have been linked first
	{
		// Send the message that ammo has been picked up
		MESSAGE_BEGIN( MSG_ONE, gmsgAmmoPickup, NULL, pev );
		WRITE_BYTE( GetAmmoIndex( name ) );
		WRITE_BYTE( added );
		MESSAGE_END();
	}
	TabulateAmmo();
	return index;
}

BOOL CBasePlayer::SwitchWeapon( CBasePlayerItem* weapon )
{
	if ( !weapon->CanDeploy() )
		return FALSE;
	ResetAutoaim();
	if ( m_pActiveItem != NULL )
		m_pActiveItem->Holster();
	m_pLastItem = m_pActiveItem;
	m_pActiveItem = weapon;
	weapon->Deploy();
	if ( weapon->m_pPlayer != NULL )
		weapon->m_pPlayer->ResetMaxSpeed();
	if ( m_bOwnsShield )
		m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
	return TRUE;
}

int CBasePlayer::AddPlayerItem( CBasePlayerItem* item )
{
	CBasePlayerItem* existing = m_rgpPlayerItems[item->iItemSlot()];
	while ( existing != NULL )
	{
		if ( FStrEq( STRING( existing->pev->classname ), STRING( item->pev->classname ) ) )
		{
			if ( item->AddDuplicate( existing ) )
			{
				g_pGameRules->PlayerGotWeapon( this, item );
				item->CheckRespawn();
				existing->UpdateItemInfo();
				if ( m_pActiveItem != NULL )
					m_pActiveItem->UpdateItemInfo();
				item->Kill();
				return FALSE;
			}
			if ( gEvilImpulse101 )
				item->Kill();
			return FALSE;
		}
		existing = existing->m_pNext;
	}
	if ( item->AddToPlayer( this ) )
	{
		g_pGameRules->PlayerGotWeapon( this, item );
		if ( item->iItemSlot() == 1 )
			m_bHasPrimary = true;
		item->CheckRespawn();
		item->m_pNext = m_rgpPlayerItems[item->iItemSlot()];
		m_rgpPlayerItems[item->iItemSlot()] = item;
		if ( m_bOwnsShield )
			pev->gamestate = 0;
		if ( g_pGameRules->FShouldSwitchWeapon( this, item ) && !m_bShieldDrawn )
			SwitchWeapon( item );
		return TRUE;
	}
	if ( gEvilImpulse101 )
		item->Kill();
	return FALSE;
}

int CBasePlayer::RemovePlayerItem( CBasePlayerItem* item )
{
	if ( m_pActiveItem == item )
	{
		ResetAutoaim();
		m_pActiveItem->pev->nextthink = 0;
		m_pActiveItem->SetThink( NULL );
		m_pActiveItem = NULL;
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
	}
	if ( m_pLastItem == item )
		m_pLastItem = NULL;
	CBasePlayerItem* previous = m_rgpPlayerItems[item->iItemSlot()];
	if ( previous == item )
	{
		m_rgpPlayerItems[item->iItemSlot()] = item->m_pNext;
		return TRUE;
	}
	while ( previous != NULL )
	{
		if ( previous->m_pNext == item )
		{
			previous->m_pNext = item->m_pNext;
			return TRUE;
		}
		previous = previous->m_pNext;
	}
	return FALSE;
}

void CBasePlayer::SelectItem( const char* name )
{
	if ( name == NULL )
		return;
	for ( int i = 0; i < ARRAYSIZE( m_rgpPlayerItems ); i++ )
	{
		for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item != NULL; item = item->m_pNext )
		{
			if ( !FStrEq( STRING( item->pev->classname ), name ) )
				continue;
			if ( m_pActiveItem == item )
				return;
			ResetAutoaim();
			if ( m_pActiveItem != NULL )
				m_pActiveItem->Holster();
			m_pLastItem = m_pActiveItem;
			m_pActiveItem = item;
			( (CBasePlayerWeapon*)item )->m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
			m_bShieldDrawn = false;
			m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
			m_pActiveItem->Deploy();
			m_pActiveItem->UpdateItemInfo();
			ResetMaxSpeed();
			return;
		}
	}
}

void CBasePlayer::SelectNextItem( int slot )
{
	CBasePlayerItem* item = m_rgpPlayerItems[slot];
	if ( item == NULL )
		return;
	if ( item == m_pActiveItem )
	{
		CBasePlayerItem* next = item->m_pNext;
		if ( next == NULL )
			return;
		CBasePlayerItem* tail = next;
		while ( tail->m_pNext != NULL )
			tail = tail->m_pNext;
		tail->m_pNext = item;
		m_pActiveItem->m_pNext = NULL;
		item = next;
		m_rgpPlayerItems[slot] = next;
	}
	ResetAutoaim();
	if ( m_pActiveItem != NULL )
		m_pActiveItem->Holster();
	if ( m_bOwnsShield )
	{
		if ( m_pActiveItem != NULL )
			( (CBasePlayerWeapon*)m_pActiveItem )->m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
		m_bShieldDrawn = false;
	}
	m_pLastItem = m_pActiveItem;
	m_pActiveItem = item;
	m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
	m_pActiveItem->Deploy();
	m_pActiveItem->UpdateItemInfo();
	ResetMaxSpeed();
}

void CBasePlayer::SelectLastItem()
{
	if ( m_pActiveItem != NULL && !m_pActiveItem->CanHolster() )
		return;
	if ( m_pLastItem == NULL || m_pLastItem == m_pActiveItem )
	{
		if ( m_rgpPlayerItems[1] != NULL && m_rgpPlayerItems[1] != m_pActiveItem )
			m_pLastItem = m_rgpPlayerItems[1];
		else if ( m_rgpPlayerItems[2] != NULL && m_rgpPlayerItems[2] != m_pActiveItem )
			m_pLastItem = m_rgpPlayerItems[2];
		else if ( m_rgpPlayerItems[3] != NULL && m_rgpPlayerItems[3] != m_pActiveItem )
			m_pLastItem = m_rgpPlayerItems[3];
	}
	if ( m_pLastItem == NULL || m_pLastItem == m_pActiveItem )
		return;
	ResetAutoaim();
	if ( m_pActiveItem != NULL )
	{
		m_pActiveItem->Holster();
		if ( m_bOwnsShield && m_pActiveItem != NULL )
			( (CBasePlayerWeapon*)m_pActiveItem )->m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	}
	if ( m_bOwnsShield )
		m_bShieldDrawn = false;
	CBasePlayerItem* item = m_pActiveItem;
	m_pActiveItem = m_pLastItem;
	m_pLastItem = item;
	m_pActiveItem->Deploy();
	m_pActiveItem->UpdateItemInfo();
	m_iHideHUD &= ~HIDEHUD_CROSSHAIR;
	ResetMaxSpeed();
}

void CBasePlayer::ItemPostFrame()
{
	if ( m_pTank )
		return;
	if ( m_pActiveItem != NULL && m_bOwnsShield &&
		( (CBasePlayerWeapon*)m_pActiveItem )->m_fInReload && ( pev->button & IN_ATTACK2 ) )
		m_flNextAttack = 0;
	else if ( m_flNextAttack > 0 )
		return;
	ImpulseCommands();
	if ( m_pActiveItem != NULL )
		m_pActiveItem->ItemPostFrame();
}

void CBasePlayer::SetScoreboardAttributes( CBasePlayer* destination )
{
	if ( destination == NULL )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* player = (CBasePlayer*)UTIL_PlayerByIndex( i );
			if ( player != NULL && !FNullEnt( player->edict() ) )
				SetScoreboardAttributes( player );
		}
		return;
	}
	int attributes = pev->deadflag != DEAD_NO;
	if ( m_bHasC4 )
		attributes |= 2;
	if ( m_bIsVIP )
		attributes |= 4;
	if ( m_bHasDefuser )
		attributes |= 8;
	if ( gmsgScoreAttrib )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgScoreAttrib, NULL, destination->pev );
		WRITE_BYTE( entindex() );
		WRITE_BYTE( attributes );
		MESSAGE_END();
	}
}

void CBasePlayer::HostageUsed()
{
	if ( m_flDisplayHistory & 0x10 )
		return;
	if ( m_iTeam == TERRORIST )
		HintMessage( "#Hint_use_hostage_to_stop_him", FALSE, FALSE );
	else if ( m_iTeam == CT )
		HintMessage( "#Hint_lead_hostage_to_rescue_point", FALSE, FALSE );
	m_flDisplayHistory |= 0x10;
}

void CBasePlayer::CheckSuitUpdate()
{
	if ( !( pev->weapons & ( 1 << WEAPON_SUIT ) ) )
		return;
	int next = m_iSuitPlayNext;

	// if in range of radiation source, ping geiger counter
	UpdateGeigerCounter();
	if ( g_pGameRules->IsMultiplayer() || !( gpGlobals->time >= m_flSuitUpdate && m_flSuitUpdate > 0 ) )
		return;
	for ( int i = 0; i < CSUITPLAYLIST; i++ )
	{
		int index = ( next + i ) % CSUITPLAYLIST;
		int sentence = m_rgSuitPlayList[index];
		if ( sentence == 0 )
			continue;
		m_rgSuitPlayList[index] = 0;
		if ( sentence > 0 )
		{
			char name[18];
			name[0] = '!';
			strcpy( name + 1, gszallsentencenames[sentence] );
			EMIT_SOUND_SUIT( edict(), name );
		}
		else
			EMIT_GROUPID_SUIT( edict(), -sentence );
		m_flSuitUpdate = gpGlobals->time + 3.5f;
		return;
	}
	// queue is empty, don't check 
	m_flSuitUpdate = 0;
}

bool CBasePlayer::IsLookingAtPosition( const Vector* position, float tolerance ) const
{
	Vector angles = UTIL_VecToAngles( *position - ( pev->origin + pev->view_ofs ) );
	float yaw = angles.y - pev->v_angle.y;
	float pitch = 360 - angles.x - pev->v_angle.x;
	while ( yaw < -180 )
		yaw += 360;
	while ( yaw > 180 )
		yaw -= 360;
	while ( pitch < -180 )
		pitch += 360;
	while ( pitch > 180 )
		pitch -= 360;
	return tolerance > abs( (int)yaw ) && tolerance > abs( (int)pitch );
}

void CBasePlayer::SyncRoundTimer()
{
	int remaining = 0;
	float timeRemaining = 0;
	if ( g_pGameRules->IsMultiplayer() )
	{
		CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
		timeRemaining = rules->m_iRoundTimeSecs - gpGlobals->time + rules->m_fRoundCount;
		remaining = (int)timeRemaining;
		if ( remaining < 0 )
			remaining = 0;
	}
	MESSAGE_BEGIN( MSG_ONE, gmsgRoundTime, NULL, pev );
	WRITE_SHORT( remaining );
	MESSAGE_END();
	if ( !g_pGameRules->IsMultiplayer() )
		return;
	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	if ( rules->IsFreezePeriod() && TheTutor != NULL && pev->iuser1 == 0 && IsAlive() )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgBlinkAcct, NULL, pev );
		WRITE_BYTE( 30 );
		MESSAGE_END();
	}
	if ( !rules->IsCareer() )
		return;
	if ( timeRemaining != 0 )
		timeRemaining = TheCareerTasks->GetTaskTime() - ( gpGlobals->time - rules->m_fRoundCount );
	int taskTime = timeRemaining < 0 ? 0 : (int)timeRemaining;
	if ( rules->IsFreezePeriod() )
		taskTime = -1;
	if ( TheCareerTasks->GetFinishedTaskTime() != 0 )
		taskTime = -TheCareerTasks->GetFinishedTaskTime();
	bool counting = !rules->IsFreezePeriod() && TheCareerTasks->GetFinishedTaskTime() == 0;
	int fadeOutDelay = 0;
	if ( !rules->IsFreezePeriod() && ( TheCareerTasks->GetFinishedTaskTime() != 0 ||
		!( TheCareerTasks->GetRoundElapsedTime() < TheCareerTasks->GetTaskTime() ) ) )
		fadeOutDelay = 3;
	if ( TheCareerTasks->GetFinishedTaskTime() != 0 &&
		rules->m_iTotalRoundsPlayed != TheCareerTasks->GetFinishedTaskRound() )
		return;
	MESSAGE_BEGIN( MSG_ONE, gmsgTaskTime, NULL, pev );
	WRITE_SHORT( taskTime );
	WRITE_BYTE( counting );
	WRITE_BYTE( fadeOutDelay );
	MESSAGE_END();
}

class CWShield : public CBaseEntity
{
public:
	void SetCantBePickedUpByUser( CBaseEntity* entity, float time )
	{
		m_hEntToIgnoreTouchesFrom = entity;
		m_flTimeToIgnoreTouches = gpGlobals->time + time;
	}

private:
	EHANDLE m_hEntToIgnoreTouchesFrom;
	float m_flTimeToIgnoreTouches;
	void Spawn() override;
	void EXPORT Touch( CBaseEntity* other ) override;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

LINK_ENTITY_TO_CLASS( weapon_shield, CWShield );

void CWShield::Spawn()
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	SET_MODEL( edict(), "models/w_shield.mdl" );
}

void CWShield::Touch( CBaseEntity* other )
{
	if ( !other->IsPlayer() || other->pev->deadflag != DEAD_NO )
		return;
	if ( m_hEntToIgnoreTouchesFrom.Get() != NULL && other == (CBaseEntity*)m_hEntToIgnoreTouchesFrom )
	{
		if ( m_flTimeToIgnoreTouches > gpGlobals->time )
			return;
		m_hEntToIgnoreTouchesFrom = NULL;
	}
	CBasePlayer* player = (CBasePlayer*)other;
	if ( player->HasShield() )
		return;
	if ( player->m_rgpPlayerItems[2] != NULL && player->m_rgpPlayerItems[2]->m_iId == WEAPON_ELITE )
		return;
	if ( player->m_pActiveItem != NULL && !player->m_pActiveItem->CanHolster() )
		return;
	if ( player->m_bIsVIP )
		return;
	player->GiveShield( true );
	EMIT_SOUND( edict(), CHAN_WEAPON, "items/gunpickup2.wav", VOL_NORM, ATTN_NORM );
	UTIL_Remove( this );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CBasePlayer::GiveShield( bool deploy )
{
	m_bOwnsShield = true;
	m_bHasPrimary = true;
	if ( m_pActiveItem != NULL && deploy )
	{
		CBasePlayerWeapon* weapon = (CBasePlayerWeapon*)m_pActiveItem;
		if ( m_rgAmmo[weapon->m_iPrimaryAmmoType] > 0 )
			m_pActiveItem->Holster();
		if ( !m_pActiveItem->Deploy() )
			( (CBasePlayerWeapon*)m_pActiveItem )->RetireWeapon();
	}
	pev->gamestate = 0;
}

void CBasePlayer::DropShield( bool deploy )
{
	if ( !m_bOwnsShield || ( m_pActiveItem != NULL && !m_pActiveItem->CanHolster() ) )
		return;
	if ( m_pActiveItem != NULL )
	{
		CBasePlayerWeapon* weapon = (CBasePlayerWeapon*)m_pActiveItem;
		if ( ( weapon->m_iId == WEAPON_SMOKEGRENADE || weapon->m_iId == WEAPON_FLASHBANG ||
			weapon->m_iId == WEAPON_HEGRENADE ) && m_rgAmmo[weapon->m_iPrimaryAmmoType] <= 0 )
			g_pGameRules->GetNextBestWeapon( this, m_pActiveItem );
		if ( m_pActiveItem != NULL )
		{
			if ( m_pActiveItem->m_flStartThrow != 0 )
				m_pActiveItem->Holster();
			if ( m_pActiveItem != NULL )
			{
				weapon = (CBasePlayerWeapon*)m_pActiveItem;
				if ( weapon->m_fInReload )
				{
					weapon->m_fInReload = FALSE;
					m_flNextAttack = 0;
				}
				if ( m_bShieldDrawn && m_bOwnsShield && m_pActiveItem != NULL )
					( (CBasePlayerWeapon*)m_pActiveItem )->SecondaryAttack();
			}
		}
	}
	m_bShieldDrawn = false;
	RemoveShield();
	if ( m_pActiveItem != NULL && deploy )
		m_pActiveItem->Deploy();
	UTIL_MakeVectors( pev->angles );
	CWShield* shield = (CWShield*)CBaseEntity::Create( "weapon_shield",
		pev->origin + gpGlobals->v_forward * 10, pev->angles, edict() );
	shield->pev->angles.x = 0;
	shield->pev->angles.z = 0;
	shield->pev->velocity = gpGlobals->v_forward * 400;
	shield->SetThink( &CBaseEntity::SUB_Remove );
	shield->pev->nextthink = gpGlobals->time + 300;
	shield->SetCantBePickedUpByUser( this, 2 );
}

void CBasePlayer::RemoveAllItems( BOOL removeSuit )
{
	bool clearProgress = false;
	if ( m_bHasDefuser )
	{
		m_bHasDefuser = false;
		pev->body = 0;
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_STRING( "defuser" );
		MESSAGE_END();
		SendItemStatus( this );
		clearProgress = true;
	}
	if ( m_bHasC4 )
	{
		m_bHasC4 = false;
		pev->body = 0;
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_STRING( "c4" );
		MESSAGE_END();
		clearProgress = true;
	}
	RemoveShield();
	if ( clearProgress )
		SetProgressBarTime( 0 );
	if ( m_pActiveItem != NULL )
	{
		ResetAutoaim();
		m_pActiveItem->Holster();
	}
	m_pLastItem = NULL;
	for ( int i = 0; i < ARRAYSIZE( m_rgpPlayerItems ); i++ )
	{
		m_pActiveItem = m_rgpPlayerItems[i];
		while ( m_pActiveItem != NULL )
		{
			CBasePlayerItem* next = m_pActiveItem->m_pNext;
			m_pActiveItem->Drop();
			m_pActiveItem = next;
		}
		m_rgpPlayerItems[i] = NULL;
	}
	m_pActiveItem = NULL;
	m_bHasPrimary = false;
	pev->viewmodel = 0;
	pev->weaponmodel = 0;
	if ( removeSuit )
		pev->weapons = 0;
	else
		pev->weapons &= 1 << WEAPON_SUIT;
	memset( m_rgAmmo, 0, sizeof( m_rgAmmo ) );
	UpdateClientData();
	// send Selected Weapon Message to our client
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 0 );
	MESSAGE_END();
}

static const char* WeaponWorldModel( int weapon )
{
	switch ( weapon )
	{
	case WEAPON_P228: return "models/w_p228.mdl";
	case WEAPON_SCOUT: return "models/w_scout.mdl";
	case WEAPON_HEGRENADE: return "models/w_hegrenade.mdl";
	case WEAPON_XM1014: return "models/w_xm1014.mdl";
	case WEAPON_C4: return "models/w_backpack.mdl";
	case WEAPON_MAC10: return "models/w_mac10.mdl";
	case WEAPON_AUG: return "models/w_aug.mdl";
	case WEAPON_SMOKEGRENADE: return "models/w_smokegrenade.mdl";
	case WEAPON_ELITE: return "models/w_elite.mdl";
	case WEAPON_FIVESEVEN: return "models/w_fiveseven.mdl";
	case WEAPON_UMP45: return "models/w_ump45.mdl";
	case WEAPON_SG550: return "models/w_sg550.mdl";
	case WEAPON_GALIL: return "models/w_galil.mdl";
	case WEAPON_FAMAS: return "models/w_famas.mdl";
	case WEAPON_USP: return "models/w_usp.mdl";
	case WEAPON_GLOCK18: return "models/w_glock18.mdl";
	case WEAPON_AWP: return "models/w_awp.mdl";
	case WEAPON_MP5N: return "models/w_mp5.mdl";
	case WEAPON_M249: return "models/w_m249.mdl";
	case WEAPON_M3: return "models/w_m3.mdl";
	case WEAPON_M4A1: return "models/w_m4a1.mdl";
	case WEAPON_TMP: return "models/w_tmp.mdl";
	case WEAPON_G3SG1: return "models/w_g3sg1.mdl";
	case WEAPON_FLASHBANG: return "models/w_flashbang.mdl";
	case WEAPON_DEAGLE: return "models/w_deagle.mdl";
	case WEAPON_SG552: return "models/w_sg552.mdl";
	case WEAPON_AK47: return "models/w_ak47.mdl";
	case WEAPON_KNIFE: return "models/w_knife.mdl";
	case WEAPON_P90: return "models/w_p90.mdl";
	case WEAPON_SHIELDGUN: return "models/w_shield.mdl";
	default: return NULL;
	}
}

static void packPlayerItem( CBasePlayer* player, CBasePlayerItem* item, bool packAmmo )
{
	if ( item == NULL )
		return;
	const char* model = WeaponWorldModel( item->m_iId );
	if ( model == NULL )
	{
		ALERT( at_console, "CBasePlayer::PackDeadPlayerItems(): Unhandled item- not creating weaponbox\n" );
		return;
	}
	CWeaponBox* box = (CWeaponBox*)CBaseEntity::Create( "weaponbox", player->pev->origin,
		player->pev->angles, player->edict() );
	box->pev->angles.x = 0;
	box->pev->angles.z = 0;
	box->pev->velocity = player->pev->velocity * 0.75f;
	box->SetThink( &CWeaponBox::Kill );
	box->pev->nextthink = gpGlobals->time + 300;
	box->PackWeapon( item );
	if ( packAmmo )
		box->PackAmmo( MAKE_STRING( CBasePlayerItem::ItemInfoArray[item->m_iId].pszAmmo1 ),
			player->m_rgAmmo[item->PrimaryAmmoIndex()] );
	SET_MODEL( box->edict(), model );
}

void CBasePlayer::PackDeadPlayerItems()
{
	int weapons = g_pGameRules->DeadPlayerWeapons( this );
	bool packAmmo = g_pGameRules->DeadPlayerAmmo( this ) != GR_PLR_DROP_AMMO_NO;
	if ( weapons != GR_PLR_DROP_GUN_NO )
	{
		bool hadShield = m_bOwnsShield;
		if ( hadShield )
			DropShield( true );
		CBasePlayerItem* best = NULL;
		int weight = 0;
		for ( int i = 0; i < ARRAYSIZE( m_rgpPlayerItems ); i++ )
		{
			for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item != NULL; item = item->m_pNext )
			{
				if ( item->iItemSlot() <= 2 && !hadShield )
				{
					ItemInfo info;
					if ( item->GetItemInfo( &info ) && info.iWeight > weight )
					{
						best = item;
						weight = info.iWeight;
					}
				}
				else if ( item->iItemSlot() == 4 && UTIL_IsGame( "czero" ) )
					packPlayerItem( this, item, true );
			}
		}
		packPlayerItem( this, best, packAmmo );
	}
	RemoveAllItems( TRUE ); // now strip off everything that wasn't handled by the code above.
}

CBasePlayer* CBasePlayer::GetNextRadioRecipient( CBasePlayer* start )
{
	CBaseEntity* entity = start;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			return NULL;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
		if ( entity->IsPlayer() )
		{
			if ( entity->pev->flags != FL_DORMANT && player != NULL && player->m_iTeam == m_iTeam )
				return player;
		}
		else if ( player != NULL )
		{
			int mode = player->pev->iuser1;
			if ( ( mode == 1 || mode == 2 || mode == 4 ) && player->m_hObserverTarget )
			{
				CBasePlayer* target = (CBasePlayer*)CBaseEntity::Instance( player->m_hObserverTarget->pev );
				if ( target != NULL && target->m_iTeam == m_iTeam )
					return player;
			}
		}
	}
	return NULL;
}

void CBasePlayer::Radio( const char* message, const char* verbose, short pitch, bool showIcon )
{
	if ( !IsPlayer() || ( pev->deadflag != DEAD_NO && !IsBot() ) )
		return;
	CBasePlayer* player = NULL;
	while ( ( player = GetNextRadioRecipient( player ) ) != NULL )
	{
		if ( player->m_bIgnoreRadio )
			continue;
		MESSAGE_BEGIN( MSG_ONE, gmsgSendAudio, NULL, player->pev );
		WRITE_BYTE( entindex() );
		WRITE_STRING( message );
		WRITE_SHORT( pitch );
		MESSAGE_END();
		if ( verbose != NULL )
		{
			Place place = TheNavAreaGrid.GetPlace( &pev->origin );
			const char* location = TheBotPhrases->IDToName( place );
			if ( location != NULL )
				ClientPrint( player->pev, HUD_PRINTRADIO, NumAsString( entindex() ),
					"#Game_radio_location", STRING( pev->netname ), location, verbose );
			else
				ClientPrint( player->pev, HUD_PRINTRADIO, NumAsString( entindex() ),
					"#Game_radio", STRING( pev->netname ), verbose, NULL );
		}
		if ( showIcon )
		{
			MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, player->pev );
			WRITE_BYTE( TE_PLAYERATTACHMENT );
			WRITE_BYTE( entindex() );
			WRITE_COORD( 35 );
			WRITE_SHORT( g_sModelIndexRadio );
			WRITE_SHORT( 15 );
			MESSAGE_END();
		}
	}
}

void CBasePlayer::UpdateLocation( bool force )
{
	if ( !force && !( m_flLastUpdateTime < gpGlobals->time + 2 ) )
		return;
	const char* location = "";
	if ( pev->deadflag == DEAD_NO && UTIL_IsGame( "czero" ) )
	{
		Place place = TheNavAreaGrid.GetPlace( &pev->origin );
		const char* name = TheBotPhrases->IDToName( place );
		if ( name != NULL )
			location = name;
	}
	if ( *location == '\0' || ( m_lastLocation[0] != '\0' && !strcmp( location, m_lastLocation + 1 ) ) )
		return;
	m_flLastUpdateTime = gpGlobals->time;
	_snprintf( m_lastLocation, sizeof( m_lastLocation ), "#%s", location );
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* player = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( player == NULL )
			continue;
		if ( player->m_iTeam == m_iTeam || player->m_iTeam == SPECTATOR )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgLocation, NULL, player->pev );
			WRITE_BYTE( entindex() );
			WRITE_STRING( m_lastLocation );
			MESSAGE_END();
		}
		else if ( force )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgLocation, NULL, player->pev );
			WRITE_BYTE( entindex() );
			WRITE_STRING( "" );
			MESSAGE_END();
		}
	}
}

bool CBasePlayer::CanPlayerBuy( bool display )
{
	if ( !g_pGameRules->IsMultiplayer() )
		return ( (CHalfLifeTraining*)g_pGameRules )->PlayerCanBuy( this );
	if ( pev->deadflag != DEAD_NO || !( m_signals.GetState() & SIGNAL_BUY ) )
		return false;
	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	int buyTime = (int)( CVAR_GET_FLOAT( "mp_buytime" ) * 60 );
	if ( buyTime < 15 )
	{
		CVAR_SET_FLOAT( "mp_buytime", 0.25f );
		buyTime = 15;
	}
	if ( gpGlobals->time - rules->m_fRoundCount > buyTime )
	{
		if ( display )
			ClientPrint( pev, HUD_PRINTCENTER, "#Cant_buy", UTIL_dtos1( buyTime ) );
		return false;
	}
	if ( m_bIsVIP )
	{
		if ( display )
			ClientPrint( pev, HUD_PRINTCENTER, "#VIP_cant_buy" );
		return false;
	}
	if ( rules->m_bCTCantBuy && m_iTeam == CT )
	{
		if ( display )
			ClientPrint( pev, HUD_PRINTCENTER, "#CT_cant_buy" );
		return false;
	}
	if ( rules->m_bTCantBuy && m_iTeam == TERRORIST )
	{
		if ( display )
			ClientPrint( pev, HUD_PRINTCENTER, "#Terrorist_cant_buy" );
		return false;
	}
	return true;
}

void CBasePlayer::MakeVIP()
{
	pev->body = 0;
	m_iModelName = MODEL_VIP;
	g_engfuncs.pfnSetClientKeyValue( entindex(), g_engfuncs.pfnGetInfoKeyBuffer( edict() ),
		(char*)"model", (char*)"vip" );
	UTIL_LogPrintf( "\"%s<%i><%s><CT>\" triggered \"Became_VIP\"\n",
		STRING( pev->netname ), GETPLAYERUSERID( edict() ), GETPLAYERAUTHID( edict() ) );
	m_iTeam = CT;
	m_bIsVIP = true;
	m_bNotKilled = false;
	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	rules->m_pVIP = this;
	rules->m_iConsecutiveVIP = 1;
}

void CBasePlayer::RoundRespawn()
{
	m_canSwitchObserverModes = true;
	if ( m_bJustKilledTeammate && CVAR_GET_FLOAT( "mp_tkpunish" ) != 0 )
	{
		m_bJustKilledTeammate = false;
		CLIENT_COMMAND( edict(), "kill\n" );
		m_bPunishedForTK = true;
	}
	if ( m_iMenu != Menu_ChooseAppearance )
	{
		respawn( pev, FALSE );
		pev->button = 0;
		pev->nextthink = -1;
	}
	if ( m_pActiveItem != NULL && m_pActiveItem->iItemSlot() == 4 )
		SwitchWeapon( m_pActiveItem );
	m_lastLocation[0] = '\0';
}

void CBasePlayer::StartDeathCam()
{
	if ( pev->view_ofs.x != g_vecZero.x || pev->view_ofs.y != g_vecZero.y || pev->view_ofs.z != g_vecZero.z )
	{
		StartObserver( pev->origin, pev->angles );
		if ( TheBots != NULL )
			TheBots->OnEvent( EVENT_DEATH_CAMERA_START, this, NULL );
	}
}

void CBasePlayer::SmartRadio()
{
}

void CBasePlayer::GiveDefaultItems()
{
	RemoveAllItems( FALSE );
	m_bHasPrimary = false;
	if ( m_iTeam == CT )
	{
		GiveNamedItem( "weapon_knife" );
		GiveNamedItem( "weapon_usp" );
		GiveAmmo( m_bIsVIP ? 12 : 24, (char*)"45acp", MAX_AMMO_45ACP );
	}
	else if ( m_iTeam == TERRORIST )
	{
		GiveNamedItem( "weapon_knife" );
		GiveNamedItem( "weapon_glock18" );
		GiveAmmo( 40, (char*)"9mm", MAX_AMMO_9MM );
	}
}

void CBasePlayer::SetPrefsFromUserinfo( char* buffer )
{
	const char* value = g_engfuncs.pfnInfoKeyValue( buffer, (char*)"_cl_autowepswitch" );
	m_iAutoWepSwitch = *value ? atoi( value ) : 1;
	value = g_engfuncs.pfnInfoKeyValue( buffer, (char*)"_vgui_menus" );
	m_bVGUIMenus = *value == '\0' || atoi( value ) != 0;
	value = g_engfuncs.pfnInfoKeyValue( buffer, (char*)"_ah" );
	m_bShowHints = *value == '\0' || atoi( value ) != 0;
}

void CBasePlayer::SetBombIcon( BOOL flash )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
	if ( m_bHasC4 )
	{
		WRITE_BYTE( flash ? 2 : 1 );
		WRITE_STRING( "c4" );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 160 );
		WRITE_BYTE( 0 );
	}
	else
	{
		WRITE_BYTE( 0 );
		WRITE_STRING( "c4" );
	}
	MESSAGE_END();
	SetScoreboardAttributes( NULL );
}

void CBasePlayer::DropPlayerItem( const char* name )
{
	bool named = *name != '\0';
	if ( m_bIsVIP )
	{
		ClientPrint( pev, HUD_PRINTCENTER, "#Weapon_Cannot_Be_Dropped" );
		return;
	}
	if ( !named )
	{
		if ( m_bOwnsShield )
		{
			DropShield( true );
			return;
		}
		name = NULL;
	}
	CBasePlayerItem* item = NULL;
	for ( int i = 0; i < ARRAYSIZE( m_rgpPlayerItems ); i++ )
	{
		for ( CBasePlayerItem* candidate = m_rgpPlayerItems[i]; candidate != NULL; candidate = candidate->m_pNext )
		{
			if ( name != NULL ? FStrEq( name, STRING( candidate->pev->classname ) ) : candidate == m_pActiveItem )
			{
				item = candidate;
				break;
			}
		}
		if ( item != NULL )
			break;
	}
	if ( item == NULL )
		return;
	if ( !item->CanDrop() )
	{
		ClientPrint( pev, HUD_PRINTCENTER, "#Weapon_Cannot_Be_Dropped" );
		return;
	}
	pev->weapons &= ~( 1 << item->m_iId );
	g_pGameRules->GetNextBestWeapon( this, item );
	UTIL_MakeVectors( pev->angles );
	if ( item->iItemSlot() == 1 )
		m_bHasPrimary = false;
	bool bomb = FStrEq( STRING( item->pev->classname ), "weapon_c4" );
	if ( bomb )
	{
		m_bHasC4 = false;
		pev->body = 0;
		SetBombIcon( FALSE );
		item->m_pPlayer->SetProgressBarTime( 0 );
		if ( ( (CHalfLifeMultiplay*)g_pGameRules )->m_fTeamCount == 0 )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><TERRORIST>\" triggered \"Dropped_The_Bomb\"\n",
				STRING( pev->netname ), GETPLAYERUSERID( edict() ), GETPLAYERAUTHID( edict() ) );
			g_pGameRules->m_bBombDropped = TRUE;
			CBaseEntity* entity = NULL;
			while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
			{
				if ( FNullEnt( entity->edict() ) )
					break;
				if ( !entity->IsPlayer() || entity->pev->flags == FL_DORMANT )
					continue;
				CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
				if ( player->pev->deadflag == DEAD_NO && player->m_iTeam == TERRORIST )
				{
					ClientPrint( player->pev, HUD_PRINTCENTER, "#Game_bomb_drop", STRING( pev->netname ) );
					MESSAGE_BEGIN( MSG_ONE, gmsgBombDrop, NULL, player->pev );
					WRITE_COORD( pev->origin.x );
					WRITE_COORD( pev->origin.y );
					WRITE_COORD( pev->origin.z );
					WRITE_BYTE( 0 );
					MESSAGE_END();
				}
			}
		}
	}
	CWeaponBox* box = (CWeaponBox*)CBaseEntity::Create( "weaponbox",
		pev->origin + gpGlobals->v_forward * 10, pev->angles, edict() );
	box->pev->angles.x = 0;
	box->pev->angles.z = 0;
	box->SetThink( &CWeaponBox::Kill );
	box->pev->nextthink = gpGlobals->time + 300;
	box->PackWeapon( item );
	box->pev->velocity = gpGlobals->v_forward * 400;
	box->m_bIsBomb = false;
	if ( FStrEq( STRING( item->pev->classname ), "weapon_c4" ) )
	{
		box->m_bIsBomb = true;
		box->SetThink( &CWeaponBox::BombThink );
		box->pev->nextthink = gpGlobals->time + 1;
		CCSBotManager* bots = (CCSBotManager*)TheBots;
		bots->SetLooseBomb( box );
		bots->OnEvent( EVENT_BOMB_DROPPED, NULL, NULL );
	}
	ItemInfo& info = CBasePlayerItem::ItemInfoArray[item->m_iId];
	if ( ( info.iFlags & ITEM_FLAG_EXHAUSTIBLE ) && info.pszAmmo1 != NULL )
	{
		int index = GetAmmoIndex( info.pszAmmo1 );
		if ( index != -1 )
		{
			box->PackAmmo( MAKE_STRING( info.pszAmmo1 ), m_rgAmmo[index] > 0 );
			m_rgAmmo[index] = 0;
		}
	}
	const char* model = WeaponWorldModel( item->m_iId );
	if ( model != NULL )
		SET_MODEL( box->edict(), model );
	else
		ALERT( at_console, "CBasePlayer::PackDeadPlayerItems(): Unhandled item- not creating weaponbox\n" );
}

void InitZombieSpawns()
{
	g_pSelectedZombieSpawn = NULL;
	zombieSpawnCount = 0;
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "info_player_start" ) ) != NULL )
	{
		if ( entity->pev->origin.x != 0 || entity->pev->origin.y != 0 || entity->pev->origin.z != 0 )
		{
			zombieSpawn[zombieSpawnCount].entity = entity;
			zombieSpawn[zombieSpawnCount].useableTimer.Invalidate();
			zombieSpawnCount++;
		}
	}
}

CBaseEntity* FindZombieSpawn( CBaseEntity*, bool )
{
	return NULL;
}

Vector CBasePlayer::GetAutoaimVector( float delta )
{
	if ( g_iSkillLevel == SKILL_HARD )
	{
		UTIL_MakeVectors( pev->v_angle + pev->punchangle );
		return gpGlobals->v_forward;
	}
	Vector source = GetGunPosition();
	m_vecAutoAim = Vector( 0, 0, 0 );
	BOOL oldTarget = m_fOnTarget;
	Vector angles = AutoaimDeflection( source, 8192, delta );

	// update ontarget if changed
	if ( !g_pGameRules->AllowAutoTargetCrosshair() )
		m_fOnTarget = FALSE;
	else if ( oldTarget != m_fOnTarget )
		m_pActiveItem->UpdateItemInfo();
	if ( angles.x > 180 )
		angles.x -= 360;
	if ( angles.x < -180 )
		angles.x += 360;
	if ( angles.y > 180 )
		angles.y -= 360;
	if ( angles.y < -180 )
		angles.y += 360;
	if ( angles.x > 25 )
		angles.x = 25;
	else if ( !( angles.x >= -25 ) )
		angles.x = -25;
	if ( angles.y > 12 )
		angles.y = 12;
	else if ( !( angles.y >= -12 ) )
		angles.y = -12;
	if ( g_iSkillLevel == SKILL_EASY )
		m_vecAutoAim = m_vecAutoAim * 0.67f + angles * 0.33f;
	else
		m_vecAutoAim = angles * 0.9f;
	if ( sv_aim != NULL && !( sv_aim->value <= 0 ) &&
		( m_vecAutoAim.x != m_lastx || m_vecAutoAim.y != m_lasty ) )
	{
		SET_CROSSHAIRANGLE( edict(), -m_vecAutoAim.x, m_vecAutoAim.y );
		m_lastx = (int)m_vecAutoAim.x;
		m_lasty = (int)m_vecAutoAim.y;
	}

	// ALERT( at_console, "%f %f\n", angles.x, angles.y );

	UTIL_MakeVectors( pev->v_angle + pev->punchangle + m_vecAutoAim );
	return gpGlobals->v_forward;
}

const char* GetWeaponName( entvars_t* inflictor, entvars_t* killer )
{
	const char* weapon;
	if ( killer->flags & FL_CLIENT )
	{
		ENTINDEX( ENT( killer ) );
		if ( inflictor == NULL )
			return "world";
		if ( killer == inflictor )
		{
			CBasePlayer* player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( killer ) );
			if ( player == NULL || player->m_pActiveItem == NULL )
				return "world";
			weapon = CBasePlayerItem::ItemInfoArray[player->m_pActiveItem->m_iId].pszName;
		}
		else
			weapon = STRING( inflictor->classname );
	}
	else
		weapon = STRING( inflictor->classname );
	if ( !memcmp( weapon, "weapon_", 7 ) )
		weapon += 7;
	else if ( !memcmp( weapon, "monster_", 8 ) )
		weapon += 8;
	else if ( !memcmp( weapon, "func_", 5 ) )
		weapon += 5;
	return weapon;
}

void CBasePlayer::Reset()
{
	pev->frags = 0;
	m_iDeaths = 0;
	m_iAccount = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsgMoney, NULL, pev );
	WRITE_LONG( m_iAccount );
	WRITE_BYTE( 0 );
	MESSAGE_END();
	m_bNotKilled = false;
	RemoveShield();
	CheckStartMoney();
	AddAccount( (int)startmoney.value, true );
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
	WRITE_BYTE( entindex() );
	WRITE_SHORT( 0 );
	WRITE_SHORT( 0 );
	WRITE_SHORT( 0 );
	WRITE_SHORT( m_iTeam );
	MESSAGE_END();
}

void CBasePlayer::Jump()
{
	if ( ( pev->flags & FL_WATERJUMP ) || pev->waterlevel > 1 || !( m_afButtonPressed & IN_JUMP ) ||
		!( pev->flags & FL_ONGROUND ) || pev->groundentity == NULL )
		return;
	UTIL_MakeVectors( pev->angles );

	// ClearBits(pev->flags, FL_ONGROUND);		// don't stairwalk

	SetAnimation( PLAYER_JUMP );
	if ( ( pev->flags & FL_DUCKING ) || ( m_afPhysicsFlags & PFLAG_DUCKING ) )
	{
		if ( m_fLongJump && ( pev->button & IN_DUCK ) && gpGlobals->time - m_flDuckTime < 1 &&
			pev->velocity.Length() > 50 )
			SetAnimation( PLAYER_SUPERJUMP );
	}
	entvars_t* ground = VARS( pev->groundentity );
	if ( ground->flags & FL_CONVEYOR )
		pev->velocity = pev->velocity + pev->basevelocity;
	if ( FClassnameIs( ground, "func_tracktrain" ) || FClassnameIs( ground, "func_train" ) ||
		FClassnameIs( ground, "func_vehicle" ) )
		pev->velocity = pev->velocity + ground->velocity;
}

void CBasePlayer::CheckTimeBasedDamage()
{
	if ( !( m_bitsDamageType & DMG_TIMEBASED ) || abs( (int)( gpGlobals->time - m_tbdPrev ) ) < 2 )
		return;
	m_tbdPrev = gpGlobals->time;
	for ( int i = 0; i < CDMG_TIMEBASED; i++ )
	{
		int damageBit = DMG_PARALYZE << i;
		if ( !( m_bitsDamageType & damageBit ) )
			continue;
		BYTE duration = 2;
		if ( i == itbd_Poison )
		{
			TakeDamage( pev, pev, POISON_DAMAGE, DMG_GENERIC );
			duration = POISON_DURATION;
		}
		else if ( i == itbd_DrownRecover )
		{
			if ( m_idrowndmg > m_idrownrestored )
			{
				int restored = m_idrowndmg - m_idrownrestored;
				if ( restored > 10 )
					restored = 10;
				TakeHealth( (float)restored, DMG_GENERIC );
				m_idrownrestored += restored;
			}
			duration = 4;
		}
		if ( m_rgbTimeBasedDamage[i] == 0 )
		{
			m_rgbTimeBasedDamage[i] = duration;
			continue;
		}
		if ( ( ( i == itbd_NerveGas && m_rgbTimeBasedDamage[i] < NERVEGAS_DURATION ) ||
			( i == itbd_Poison && m_rgbTimeBasedDamage[i] < POISON_DURATION ) ) && m_rgItems[ITEM_ANTIDOTE] != 0 )
		{
			m_rgItems[ITEM_ANTIDOTE]--;
			m_rgbTimeBasedDamage[i] = 0;
			m_bitsDamageType &= ~damageBit;
		}
		else if ( --m_rgbTimeBasedDamage[i] == 0 )
			m_bitsDamageType &= ~damageBit;
	}
}

void CBasePlayer::WaterMove()
{
	if ( pev->movetype == MOVETYPE_NONE || pev->movetype == MOVETYPE_NOCLIP || pev->health < 0 )
		return;

	// waterlevel 0 - not in water
	// waterlevel 1 - feet in water
	// waterlevel 2 - waist in water
	// waterlevel 3 - head in water

	if ( pev->waterlevel == 3 )
	{
		m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
		// stop restoring damage while underwater
		m_bitsDamageType &= ~DMG_DROWNRECOVER;
		if ( gpGlobals->time <= pev->air_finished )
			m_bitsDamageType &= ~DMG_DROWN;
		else if ( gpGlobals->time > pev->pain_finished )
		{
			pev->dmg++;
			if ( pev->dmg > 5 )
				pev->dmg = 5;
			TakeDamage( VARS( ENT( 0 ) ), VARS( ENT( 0 ) ), pev->dmg, DMG_DROWN );
			pev->pain_finished = gpGlobals->time + 1;
			m_idrowndmg = (int)( m_idrowndmg + pev->dmg );
		}
	}
	else
	{
		if ( gpGlobals->time > pev->air_finished )
			EMIT_SOUND( edict(), CHAN_VOICE, "player/pl_wade1.wav", VOL_NORM, ATTN_NORM );
		else if ( !( gpGlobals->time + 9 <= pev->air_finished ) )
			EMIT_SOUND( edict(), CHAN_VOICE, "player/pl_wade2.wav", VOL_NORM, ATTN_NORM );
		pev->air_finished = gpGlobals->time + 12;
		pev->dmg = 2;

		// if we took drowning damage, give it back slowly
		if ( m_idrowndmg > m_idrownrestored )
		{
			m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
			m_bitsDamageType &= ~DMG_DROWN;
			// set drowning damage bit.  hack - dmg_drownrecover actually
			// makes the time based damage code 'give back' health over time.
			// make sure counter is cleared so we start count correctly.

			// NOTE: this actually causes the count to continue restarting
			// until all drowning damage is healed.

			m_bitsDamageType |= DMG_DROWNRECOVER;
		}
	}
	if ( pev->waterlevel == 0 )
	{
		if ( pev->flags & FL_INWATER )
			pev->flags &= ~FL_INWATER;
		return;
	}
	float airFinished = pev->air_finished;
	float currentTime = gpGlobals->time;
	if ( RANDOM_LONG( 0, 31 ) == 0 && (int)( airFinished - currentTime ) <= RANDOM_LONG( 0, 11 ) )
	{
		switch ( RANDOM_LONG( 0, 3 ) )
		{
		case 0:
			EMIT_SOUND( edict(), CHAN_BODY, "player/pl_swim1.wav", 0.8f, ATTN_NORM );
			break;
		case 1:
			EMIT_SOUND( edict(), CHAN_BODY, "player/pl_swim2.wav", 0.8f, ATTN_NORM );
			break;
		case 2:
			EMIT_SOUND( edict(), CHAN_BODY, "player/pl_swim3.wav", 0.8f, ATTN_NORM );
			break;
		case 3:
			EMIT_SOUND( edict(), CHAN_BODY, "player/pl_swim4.wav", 0.8f, ATTN_NORM );
			break;
		}
	}
	if ( pev->watertype == CONTENTS_LAVA )		// do damage
	{
		if ( gpGlobals->time > pev->dmgtime )
			TakeDamage( VARS( ENT( 0 ) ), VARS( ENT( 0 ) ), (float)( 10 * pev->waterlevel ), DMG_BURN );
	}
	else if ( pev->watertype == CONTENTS_SLIME )		// do damage
	{
		pev->dmgtime = gpGlobals->time + 1;
		TakeDamage( VARS( ENT( 0 ) ), VARS( ENT( 0 ) ), (float)( 4 * pev->waterlevel ), DMG_ACID );
	}
	if ( !( pev->flags & FL_INWATER ) )
	{
		pev->flags |= FL_INWATER;
		pev->dmgtime = 0;
	}
}

static CBaseEntity* SelectTeamSpawn( CBaseEntity* player, CBaseEntity* last, const char* classname )
{
	CBaseEntity* first = UTIL_FindEntityByClassname( last, classname );
	if ( first == NULL || FNullEnt( first->edict() ) )
		first = UTIL_FindEntityByClassname( first, classname );
	CBaseEntity* spot = first;
	do
	{
		if ( spot != NULL && IsSpawnPointValid( player, spot ) &&
			( spot->pev->origin.x != 0 || spot->pev->origin.y != 0 || spot->pev->origin.z != 0 ) )
			return spot;
		spot = UTIL_FindEntityByClassname( spot, classname );
	} while ( spot != first );
	if ( spot == NULL || FNullEnt( spot->edict() ) )
		return NULL;
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityInSphere( entity, first->pev->origin, 64 ) ) != NULL )
	{
		if ( entity->IsPlayer() && player->edict() != entity->edict() )
			entity->TakeDamage( VARS( INDEXENT( 0 ) ), VARS( INDEXENT( 0 ) ), 300, DMG_GENERIC );
	}
	return first;
}

edict_t* EntSelectSpawnPoint( CBaseEntity* entity )
{
	CBasePlayer* player = (CBasePlayer*)entity;
	CBaseEntity* spot = NULL;

// choose a info_player_deathmatch point
	if ( g_pGameRules->IsCoOp() )
	{
		spot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_coop" );
		if ( spot == NULL || FNullEnt( spot->edict() ) )
			spot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_start" );
		if ( spot != NULL && !FNullEnt( spot->edict() ) )
			goto SelectedSpawn;
	}
	else
	{
		if ( g_pGameRules->IsDeathmatch() && player->m_bIsVIP )
		{
			spot = UTIL_FindEntityByClassname( NULL, "info_vip_start" );
			if ( spot != NULL && !FNullEnt( spot->edict() ) )
				goto SelectedSpawn;
			spot = SelectTeamSpawn( player, g_pLastCTSpawn, "info_player_start" );
		}
		else if ( g_pGameRules->IsDeathmatch() && player->m_iTeam == CT )
			spot = SelectTeamSpawn( player, g_pLastCTSpawn, "info_player_start" );
		else if ( g_pGameRules->IsDeathmatch() && player->m_iTeam == TERRORIST )
			spot = SelectTeamSpawn( player, g_pLastTerroristSpawn, "info_player_deathmatch" );
		if ( spot != NULL )
			goto SelectedSpawn;
	}
	if ( gpGlobals->startspot != 0 && *STRING( gpGlobals->startspot ) != '\0' )
		spot = UTIL_FindEntityByTargetname( NULL, STRING( gpGlobals->startspot ) );
	else
		spot = UTIL_FindEntityByClassname( NULL, "info_player_deathmatch" );

SelectedSpawn:
	if ( spot == NULL || FNullEnt( spot->edict() ) )
	{
		ALERT( at_error, "PutClientInServer: no info_player_start on level" );
		return INDEXENT( 0 );
	}
	if ( player->m_iTeam == TERRORIST )
		g_pLastTerroristSpawn = spot;
	else
		g_pLastCTSpawn = spot;
	return spot->edict();
}

void CBasePlayer::SetPlayerModel( BOOL )
{
	char* buffer = g_engfuncs.pfnGetInfoKeyBuffer( edict() );
	const char* model = "urban";
	if ( m_iTeam == CT )
	{
		switch ( m_iModelName )
		{
		case MODEL_URBAN:
			break;
		case MODEL_GSG9:
			model = "gsg9";
			break;
		case MODEL_GIGN:
			model = "gign";
			break;
		case MODEL_SAS:
			model = "sas";
			break;
		case MODEL_VIP:
			model = "vip";
			break;
		case MODEL_SPETSNAZ:
			if ( UTIL_IsGame( "czero" ) )
			{
				model = "spetsnaz";
				break;
			}
			[[fallthrough]];
		default:
			if ( IsBot() && TheBotProfiles->GetCustomSkinModelname( m_iModelName ) != NULL )
				model = TheBotProfiles->GetCustomSkinModelname( m_iModelName );
			break;
		}
	}
	else if ( m_iTeam == TERRORIST )
	{
		model = "terror";
		switch ( m_iModelName )
		{
		case MODEL_TERROR:
			break;
		case MODEL_LEET:
			model = "leet";
			break;
		case MODEL_ARCTIC:
			model = "arctic";
			break;
		case MODEL_GUERILLA:
			model = "guerilla";
			break;
		case MODEL_MILITIA:
			if ( UTIL_IsGame( "czero" ) )
			{
				model = "militia";
				break;
			}
			[[fallthrough]];
		default:
			if ( IsBot() && TheBotProfiles->GetCustomSkinModelname( m_iModelName ) != NULL )
				model = TheBotProfiles->GetCustomSkinModelname( m_iModelName );
			break;
		}
	}
	if ( strcmp( g_engfuncs.pfnInfoKeyValue( buffer, (char*)"model" ), model ) )
		g_engfuncs.pfnSetClientKeyValue( entindex(), buffer, (char*)"model", (char*)model );
}

void CBasePlayer::SendHostagePos()
{
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "hostage_entity" ) ) != NULL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHostagePos, NULL, pev );
		WRITE_BYTE( 1 );
		WRITE_BYTE( ( (CHostage*)entity )->m_iHostageIndex );
		WRITE_COORD( entity->pev->origin.x );
		WRITE_COORD( entity->pev->origin.y );
		WRITE_COORD( entity->pev->origin.z );
		MESSAGE_END();
	}
	SendHostageIcons();
}

void CBasePlayer::SendHostageIcons()
{
	if ( !UTIL_IsGame( "czero" ) )
		return;
	int count = 0;
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "hostage_entity" ) ) != NULL )
		count += entity->pev->deadflag == DEAD_NO;
	if ( count > 4 )
		count = 4;
	MESSAGE_BEGIN( MSG_ONE, gmsgScenarioIcon, NULL, pev );
	if ( count != 0 )
	{
		char icon[16];
		_snprintf( icon, sizeof( icon ), "hostage%d", count );
		WRITE_BYTE( 1 );
		WRITE_STRING( icon );
		WRITE_BYTE( 0 );
	}
	else
		WRITE_BYTE( 0 );
	MESSAGE_END();
}

void CBasePlayer::SendWeatherInfo()
{
	CBaseEntity* rain = UTIL_FindEntityByClassname( NULL, "env_rain" );
	CBaseEntity* rainBrush = UTIL_FindEntityByClassname( NULL, "func_rain" );
	if ( rain != NULL || rainBrush != NULL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgReceiveW, NULL, pev );
		WRITE_BYTE( 1 );
		MESSAGE_END();
		return;
	}
	CBaseEntity* snow = UTIL_FindEntityByClassname( NULL, "env_snow" );
	CBaseEntity* snowBrush = UTIL_FindEntityByClassname( NULL, "func_snow" );
	if ( snow != NULL || snowBrush != NULL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgReceiveW, NULL, pev );
		WRITE_BYTE( 2 );
		MESSAGE_END();
	}
}

/*
============
ImpulseCommands
============
*/
void CBasePlayer::ImpulseCommands()
{

	// Handle use events
	PlayerUse();
	int impulse = pev->impulse;
	switch ( impulse )
	{
	case 100:
		if ( FlashlightIsOn() )
			FlashlightTurnOff();
		else
			FlashlightTurnOn();
		break;
	case 201:
		if ( m_flNextDecalTime > gpGlobals->time )
			break;
		{
			UTIL_MakeVectors( pev->v_angle );
			TraceResult trace;
			Vector start = pev->origin + pev->view_ofs;
			UTIL_TraceLine( start, start + gpGlobals->v_forward * 128, ignore_monsters, edict(), &trace );
			if ( trace.flFraction != 1 )
			{
				float time = gpGlobals->time;
				m_flNextDecalTime = CVAR_GET_FLOAT( "decalfrequency" ) + time;
				CSprayCan* spray = GetClassPtr( (CSprayCan*)NULL );
				spray->Spawn( pev );
			}
		}
		break;
	case 99:
		if ( gmsgLogo != 0 )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgLogo, NULL, pev );
			WRITE_BYTE( 0 );
			MESSAGE_END();
			gmsgLogo = 0;
		}
		else
		{
			gmsgLogo = REG_USER_MSG( "Logo", 1 );
			MESSAGE_BEGIN( MSG_ONE, gmsgLogo, NULL, pev );
			WRITE_BYTE( 1 );
			MESSAGE_END();
		}
		break;
	default:
		// check all of the cheat impulse commands now
		CheatImpulseCommands( impulse );
		break;
	}
	pev->impulse = 0;
}

static bool CanSeeUseable( CBasePlayer* player, CBaseEntity* entity )
{
	Vector eye = player->pev->origin + player->pev->view_ofs;
	if ( !FClassnameIs( entity->pev, "hostage_entity" ) )
		return true;
	Vector head = entity->pev->origin + Vector( 0, 0, 64.799995f );
	Vector chest = entity->pev->origin + Vector( 0, 0, 36 );
	Vector knees = entity->pev->origin + Vector( 0, 0, 18 );
	TraceResult trace;
	UTIL_TraceLine( eye, chest, ignore_monsters, ignore_glass, entity->edict(), &trace );
	if ( trace.flFraction >= 1 )
		return true;
	UTIL_TraceLine( eye, head, ignore_monsters, ignore_glass, entity->edict(), &trace );
	if ( trace.flFraction >= 1 )
		return true;
	UTIL_TraceLine( eye, knees, ignore_monsters, ignore_glass, entity->edict(), &trace );
	return trace.flFraction >= 1;
}

// The player stops driving the func_vehicle they drive, if any. The vehicle keeps its driver only while it is this
// player, so a player who never drove it does not clear another player's record.
void CBasePlayer::StopDrivingVehicle()
{
	CFuncVehicle* vehicle = (CFuncVehicle*)(CBaseEntity*)m_hDrivenVehicle;
	if ( vehicle && (CBaseEntity*)vehicle->m_hDriver == this )
		vehicle->m_hDriver = NULL;
	m_hDrivenVehicle = NULL;
}

void CBasePlayer::PlayerUse()
{
	if ( !( ( m_afButtonReleased | m_afButtonPressed | pev->button ) & IN_USE ) )
		return;
	bool hasNavigation = !TheNavAreaList.empty();
	if ( m_afButtonPressed & IN_USE )
	{
		if ( m_pTank )
		{
			// Stop controlling the tank
			// TODO: Send HUD Update
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
			return;
		}
		CBaseEntity* ground = CBaseEntity::Instance( pev->groundentity );
		if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
		{
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = 0xC0;
			StopDrivingVehicle();
			return;
		}
		if ( ground != NULL && !( pev->button & IN_JUMP ) && ( pev->flags & FL_ONGROUND ) &&
			( ground->ObjectCaps() & FCAP_DIRECTIONAL_USE ) && ground->OnControls( pev ) )
		{
			m_afPhysicsFlags |= PFLAG_ONTRAIN;
			m_iTrain = TrainSpeed( (int)ground->pev->speed, ground->pev->impulse ) | 0xC0;
			if ( ground->Classify() == CLASS_VEHICLE )
			{
				EMIT_SOUND( edict(), CHAN_ITEM, "plats/vehicle_ignition.wav", 0.8f, ATTN_NORM );
				( (CFuncVehicle*)ground )->m_hDriver = this;
				m_hDrivenVehicle = ground;
			}
			else
				EMIT_SOUND( edict(), CHAN_ITEM, "plats/train_use1.wav", 0.8f, ATTN_NORM );
			return;
		}
	}
	UTIL_MakeVectors( pev->v_angle );
	CBaseEntity* hit = NULL;
	float bestDot = 0.7f;
	if ( hasNavigation )
	{
		TraceResult trace;
		Vector eye = pev->origin + pev->view_ofs;
		UTIL_TraceLine( eye, eye + gpGlobals->v_forward * 1000, dont_ignore_monsters, edict(), &trace );
		if ( trace.flFraction < 1 )
		{
			CBaseEntity* entity = CBaseEntity::Instance( trace.pHit );
			if ( entity != NULL && FClassnameIs( entity->pev, "hostage_entity" ) && CanSeeUseable( this, entity ) )
				hit = entity;
		}
		if ( hit == NULL )
		{
			CBaseEntity* entity = NULL;
			while ( ( entity = UTIL_FindEntityInSphere( entity, pev->origin, 1000 ) ) != NULL )
			{
				if ( !FClassnameIs( entity->pev, "hostage_entity" ) )
					continue;
				Vector direction = ( VecBModelOrigin( entity->pev ) - ( pev->origin + pev->view_ofs ) ).Normalize();
				float dot = DotProduct( direction, gpGlobals->v_forward );
				if ( dot > bestDot && CanSeeUseable( this, entity ) )
				{
					bestDot = dot;
					hit = entity;
				}
			}
		}
	}
	if ( hit == NULL )
	{
		CBaseEntity* entity = NULL;
		while ( ( entity = UTIL_FindEntityInSphere( entity, pev->origin, 64 ) ) != NULL )
		{
			if ( !( entity->ObjectCaps() & ( FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE ) ) )
				continue;
			Vector direction = ( VecBModelOrigin( entity->pev ) - ( pev->origin + pev->view_ofs ) ).Normalize();
			float dot = DotProduct( direction, gpGlobals->v_forward );
			if ( dot > bestDot )
			{
				bestDot = dot;
				hit = entity;
//				ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
			}
//			ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
		}
	}
	if ( hit != NULL && ( !hasNavigation || CanSeeUseable( this, hit ) ) )
	{
		//!!!UNDONE: traceline here to prevent USEing buttons through walls			
		int caps = hit->ObjectCaps();
		if ( m_afButtonPressed & IN_USE )
			EMIT_SOUND( edict(), CHAN_ITEM, "common/wpn_select.wav", 0.4f, ATTN_NORM );
		if ( ( ( pev->button & IN_USE ) && ( caps & FCAP_CONTINUOUS_USE ) ) ||
			( ( m_afButtonPressed & IN_USE ) && ( caps & ( FCAP_IMPULSE_USE | FCAP_ONOFF_USE ) ) ) )
		{
			if ( caps & FCAP_CONTINUOUS_USE )
				m_afPhysicsFlags |= PFLAG_USING;
			hit->Use( this, this, USE_SET, 1 );
			return;
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		if ( ( m_afButtonReleased & IN_USE ) && ( hit->ObjectCaps() & FCAP_ONOFF_USE ) )	// BUGBUG This is an "off" use
		{
			hit->Use( this, this, USE_SET, 0 );
			return;
		}
	}
	if ( m_afButtonPressed & IN_USE )
		EMIT_SOUND( edict(), CHAN_ITEM, "common/wpn_denyselect.wav", 0.4f, ATTN_NORM );
}

void CBasePlayer::StartObserver( Vector position, Vector angles )
{
	// clear any clientside entities attached to this player
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
	WRITE_BYTE( TE_KILLPLAYERATTACHMENTS );
	WRITE_BYTE( (unsigned char)entindex() );
	MESSAGE_END();
	if ( m_pActiveItem != NULL )
		m_pActiveItem->Holster();
	if ( m_pTank )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 255 );
	MESSAGE_END();
	pev->fov = 0;
	m_iClientFOV = 0;
	m_iFOV = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
	WRITE_BYTE( 0 );
	MESSAGE_END();
	m_iHideHUD = HIDEHUD_WEAPONS | HIDEHUD_HEALTH;
	m_afPhysicsFlags |= PFLAG_OBSERVER;
	pev->effects = EF_NODRAW;
	pev->view_ofs = g_vecZero;
	pev->v_angle = angles;
	pev->angles = pev->v_angle;
	pev->fixangle = 1;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetOrigin( pev, position );
	m_afPhysicsFlags &= ~PFLAG_DUCKING;
	pev->flags &= ~FL_DUCKING;
	pev->health = 1;
	m_iObserverC4State = 0;
	m_iObserverWeapon = 0;
	m_bObserverHasDefuser = false;
	m_flNextObserverInput = 0;
	pev->iuser1 = 0;
	static int iFirstTime = 1;
	if ( iFirstTime && g_pGameRules != NULL && ( (CHalfLifeMultiplay*)g_pGameRules )->IsCareer() && !IsBot() )
	{
		Observer_SetMode( 1 );
		CLIENT_COMMAND( edict(), "spec_autodirector_internal 1\n" );
		iFirstTime = 0;
	}
	else
		Observer_SetMode( m_iObserverLastMode );
	ResetMaxSpeed();
	MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );
	WRITE_BYTE( entindex() );
	WRITE_BYTE( 1 );
	MESSAGE_END();
}

void CBasePlayer::PlayerDeathThink()
{
	if ( m_iJoiningState != JOINED )
		return;
	if ( pev->flags & FL_ONGROUND )
	{
		float speed = pev->velocity.Length() - 20;
		if ( speed <= 0 )
			pev->velocity = g_vecZero;
		else
			pev->velocity = pev->velocity.Normalize() * speed;
	}
	if ( HasWeapons() )
		PackDeadPlayerItems();
	if ( pev->modelindex != 0 && !m_fSequenceFinished && pev->deadflag == DEAD_DYING )
	{
		StudioFrameAdvance();
		return;
	}
	if ( pev->movetype != MOVETYPE_NONE && ( pev->flags & FL_ONGROUND ) )
		pev->movetype = MOVETYPE_NONE;
	if ( pev->deadflag == DEAD_DYING )
	{
		m_fDeadTime = gpGlobals->time;
		pev->deadflag = DEAD_DEAD;
	}
	pev->framerate = 0;
	pev->effects |= EF_NOINTERP;
	int buttons = pev->button;
	if ( g_pGameRules->IsMultiplayer() && gpGlobals->time > m_fDeadTime + 3 && !( m_afPhysicsFlags & PFLAG_OBSERVER ) )
	{
		SpawnClientSideCorpse();
		StartDeathCam();
	}
	if ( pev->deadflag == DEAD_DEAD )
	{
		if ( !( buttons & ~IN_SCORE ) && m_iTeam != SPECTATOR && m_iTeam != UNASSIGNED )
		{
			if ( g_pGameRules->FPlayerCanRespawn( this ) )
			{
				pev->deadflag = DEAD_RESPAWNABLE;
				if ( g_pGameRules->IsMultiplayer() )
					( (CHalfLifeMultiplay*)g_pGameRules )->CheckWinConditions();
			}
			pev->nextthink = gpGlobals->time + 0.1f;
		}
	}
	else if ( pev->deadflag == DEAD_RESPAWNABLE )
	{
		respawn( pev, FALSE );
		pev->button = 0;
		pev->nextthink = -1;
	}
}

void CBasePlayer::Disappear()
{
	if ( m_pTank )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}
	CSound* sound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
	if ( sound != NULL )
		sound->Reset();
	m_fSequenceFinished = TRUE;
	pev->modelindex = m_modelIndexPlayer;
	pev->view_ofs = Vector( 0, 0, -8 );
	pev->deadflag = DEAD_DYING;
	pev->solid = SOLID_NOT;
	pev->flags &= ~FL_ONGROUND;
	m_iClientHealth = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pev );
	WRITE_BYTE( m_iClientHealth );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 255 );
	WRITE_BYTE( 255 );
	MESSAGE_END();
	pev->fov = 0;
	m_iClientFOV = 0;
	m_iFOV = 0;
	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
	WRITE_BYTE( 0 );
	MESSAGE_END();
	( (CHalfLifeMultiplay*)g_pGameRules )->CheckWinConditions();
	m_bNotKilled = false;
	if ( m_bHasC4 )
	{
		DropPlayerItem( "weapon_c4" );
		SetProgressBarTime( 0 );
	}
	else if ( m_bHasDefuser )
	{
		m_bHasDefuser = false;
		pev->body = 0;
		GiveNamedItem( "item_thighpack" );
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_STRING( "defuser" );
		MESSAGE_END();
		SendItemStatus( this );
		SetProgressBarTime( 0 );
	}
	MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
	WRITE_BYTE( 0 );
	WRITE_STRING( "buyzone" );
	MESSAGE_END();
	if ( m_iMenu >= Menu_Buy && m_iMenu <= Menu_BuyItem )
		CLIENT_COMMAND( edict(), "slot10\n" );
	else if ( m_iMenu == Menu_ClientBuy )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgBuyClose, NULL, pev );
		MESSAGE_END();
	}
	SetThink( &CBasePlayer::PlayerDeathThink );
	pev->nextthink = gpGlobals->time + 0.1f;
	pev->angles.x = 0;
	pev->angles.z = 0;
}

void CBasePlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	if ( !pev->modelindex )
		return;
	if ( playerAnim == PLAYER_FLINCH || playerAnim == PLAYER_LARGE_FLINCH )
	{
		if ( m_bOwnsShield )
			return;
	}
	else if ( m_flFlinchTime >= gpGlobals->time && pev->health > 0 )
		return;
	float speed = pev->velocity.Length2D();
	int hopSequence = LookupActivity( ACT_HOP );
	int leapSequence = LookupActivity( ACT_LEAP );
	if ( pev->flags & FL_FROZEN )
	{
		speed = 0;
		playerAnim = PLAYER_IDLE;
	}
	switch ( playerAnim )
	{
	case PLAYER_JUMP:
	case PLAYER_SUPERJUMP:
	case PLAYER_ATTACK1:
	case PLAYER_ATTACK2:
	case PLAYER_RELOAD:
		if ( m_Activity == ACT_DIESIMPLE || m_Activity == ACT_HOVER || m_Activity == ACT_SWIM )
			m_IdealActivity = m_Activity;
		else if ( playerAnim == PLAYER_JUMP )
		{
			m_IdealActivity = ACT_HOP;
			TheBots->OnEvent( EVENT_PLAYER_JUMPED, this, NULL );
		}
		else if ( playerAnim == PLAYER_SUPERJUMP )
			m_IdealActivity = ACT_LEAP;
		else if ( playerAnim == PLAYER_RELOAD )
		{
			m_IdealActivity = ACT_RELOAD;
			TheBots->OnEvent( EVENT_WEAPON_RELOADED, this, NULL );
		}
		else
		{
			m_IdealActivity = playerAnim == PLAYER_ATTACK1 ? ACT_RANGE_ATTACK1 : ACT_RANGE_ATTACK2;
			TheBots->OnEvent( EVENT_WEAPON_FIRED, this, NULL );
		}
		break;
	case PLAYER_DIE:
		m_IdealActivity = ACT_DIESIMPLE;
		DeathSound();
		break;
	case PLAYER_FLINCH:
		m_IdealActivity = ACT_FLINCH;
		m_Activity = ACT_FLINCH;
		break;
	case PLAYER_LARGE_FLINCH:
		m_IdealActivity = ACT_LARGE_FLINCH;
		m_Activity = ACT_LARGE_FLINCH;
		break;
	case PLAYER_HOLDBOMB:
		m_IdealActivity = ACT_HOLDBOMB;
		break;
	case PLAYER_IDLE:
	case PLAYER_WALK:
		if ( !(pev->flags & FL_ONGROUND) && (m_Activity == ACT_HOP || m_Activity == ACT_LEAP) )
			m_IdealActivity = m_Activity;
		else if ( pev->waterlevel > 1 )
			m_IdealActivity = speed == 0 ? ACT_HOVER : ACT_SWIM;
		else
			m_IdealActivity = ACT_WALK;
		break;
	default:
		break;
	}
	int desiredSequence;
	char animation[64];
	switch ( m_IdealActivity )
	{
	case ACT_HOP:
	case ACT_LEAP:
		if ( m_IdealActivity == m_Activity )
			return;
		strcpy( animation, "ref_" );
		switch ( m_Activity )
		{
		case ACT_RANGE_ATTACK2: strcat( animation, "shoot2_" ); break;
		case ACT_RELOAD: strcat( animation, "reload_" ); break;
		case ACT_RANGE_ATTACK1: strcat( animation, "shoot_" ); break;
		default: strcat( animation, "aim_" ); break;
		}
		strcat( animation, m_szAnimExtention );
		desiredSequence = LookupSequence( animation );
		if ( desiredSequence == -1 )
			desiredSequence = 0;
		if ( desiredSequence != pev->sequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
			if ( !m_fSequenceLoops )
				pev->effects |= EF_NOINTERP;
		}
		pev->gaitsequence = LookupActivity( m_IdealActivity == ACT_LEAP ? ACT_LEAP : ACT_HOP );
		m_Activity = m_IdealActivity;
		break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
		m_flLastFired = gpGlobals->time;
		if ( m_IdealActivity == ACT_RANGE_ATTACK1 )
			strcpy( animation, (pev->flags & FL_DUCKING) ? "crouch_shoot_" : "ref_shoot_" );
		else
			strcpy( animation, (pev->flags & FL_DUCKING) ? "crouch_shoot2_" : "ref_shoot2_" );
		strcat( animation, m_szAnimExtention );
		desiredSequence = LookupSequence( animation );
		if ( desiredSequence == -1 )
			desiredSequence = 0;
		pev->sequence = desiredSequence;
		pev->frame = 0;
		ResetSequenceInfo();
		m_Activity = m_IdealActivity;
		break;
	case ACT_RELOAD:
		strcpy( animation, (pev->flags & FL_DUCKING) ? "crouch_reload_" : "ref_reload_" );
		strcat( animation, m_szAnimExtention );
		desiredSequence = LookupSequence( animation );
		if ( desiredSequence == -1 )
			desiredSequence = 0;
		if ( desiredSequence != pev->sequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
			if ( !m_fSequenceLoops )
				pev->effects |= EF_NOINTERP;
		}
		m_Activity = m_IdealActivity;
		break;
	case ACT_DIESIMPLE:
		if ( m_Activity == ACT_DIESIMPLE )
			return;
		m_Activity = ACT_DIESIMPLE;
		m_flDeathThrowTime = 0;
		m_iThrowDirection = THROW_NONE;
		switch ( m_LastHitGroup )
		{
		case HITGROUP_GENERIC:
			switch ( RANDOM_LONG( 0, 8 ) )
			{
			case 0:
				desiredSequence = LookupActivity( ACT_DIE_HEADSHOT );
				m_iThrowDirection = THROW_BACKWARD;
				break;
			case 1:
			case 7:
				desiredSequence = LookupActivity( ACT_DIE_GUTSHOT );
				break;
			case 2:
				desiredSequence = LookupActivity( ACT_DIE_BACKSHOT );
				m_iThrowDirection = THROW_HITVEL;
				break;
			case 4:
				desiredSequence = LookupActivity( ACT_DIEBACKWARD );
				m_iThrowDirection = THROW_HITVEL;
				break;
			case 5:
				desiredSequence = LookupActivity( ACT_DIEFORWARD );
				m_iThrowDirection = THROW_FORWARD;
				break;
			case 6: desiredSequence = LookupActivity( ACT_DIE_CHESTSHOT ); break;
			case 8: desiredSequence = LookupActivity( ACT_DIE_HEADSHOT ); break;
			default: desiredSequence = LookupActivity( ACT_DIESIMPLE ); break;
			}
			break;
		case HITGROUP_HEAD:
			{
				int throwChoice = RANDOM_LONG( 0, 8 ) + (m_bHighDamage ? 1 : 0);
				m_bHeadshotKilled = true;
				if ( throwChoice == 1 || throwChoice == 2 )
					m_iThrowDirection = THROW_BACKWARD;
				else if ( throwChoice == 3 || throwChoice == 4 )
					m_iThrowDirection = THROW_FORWARD;
				else if ( throwChoice == 5 || throwChoice == 6 )
					m_iThrowDirection = THROW_HITVEL;
				desiredSequence = LookupActivity( ACT_DIE_HEADSHOT );
			}
			break;
		case HITGROUP_CHEST: desiredSequence = LookupActivity( ACT_DIE_CHESTSHOT ); break;
		case HITGROUP_STOMACH: desiredSequence = LookupActivity( ACT_DIE_GUTSHOT ); break;
		case HITGROUP_LEFTARM: desiredSequence = LookupSequence( "left" ); break;
		case HITGROUP_RIGHTARM:
			m_iThrowDirection = RANDOM_LONG( 0, 1 ) ? THROW_HITVEL : THROW_HITVEL_MINUS_AIRVEL;
			desiredSequence = LookupSequence( "right" );
			break;
		default: desiredSequence = LookupActivity( ACT_DIESIMPLE ); break;
		}
		if ( pev->flags & FL_DUCKING )
		{
			desiredSequence = LookupSequence( "crouch_die" );
			m_iThrowDirection = THROW_BACKWARD;
		}
		else if ( m_bKilledByBomb || m_bKilledByGrenade )
		{
			UTIL_MakeVectors( pev->angles );
			double blastDirection = (double)gpGlobals->v_forward.x * m_vBlastVector.x +
				(double)gpGlobals->v_forward.y * m_vBlastVector.y + (double)gpGlobals->v_forward.z * m_vBlastVector.z;
			if ( !(blastDirection > 0) )
				desiredSequence = RANDOM_LONG( 0, 1 ) ? LookupSequence( "crouch_die" ) : LookupActivity( ACT_DIE_HEADSHOT );
			else
				desiredSequence = LookupSequence( "left" );
			if ( m_bKilledByBomb )
				m_iThrowDirection = THROW_BOMB;
			else if ( m_bKilledByGrenade )
				m_iThrowDirection = THROW_GRENADE;
		}
		if ( desiredSequence == -1 )
			desiredSequence = 0;
		if ( desiredSequence != pev->sequence )
		{
			pev->gaitsequence = 0;
			pev->sequence = desiredSequence;
			pev->frame = 0;
			ResetSequenceInfo();
		}
		return;
	case ACT_FLINCH:
	case ACT_LARGE_FLINCH:
		m_Activity = m_IdealActivity;
		if ( (playerAnim == PLAYER_FLINCH || playerAnim == PLAYER_LARGE_FLINCH) && m_bOwnsShield )
			return;
		switch ( m_LastHitGroup )
		{
		case HITGROUP_GENERIC:
			desiredSequence = LookupSequence( RANDOM_LONG( 0, 1 ) ? "head_flinch" : "gut_flinch" );
			break;
		case HITGROUP_HEAD:
		case HITGROUP_CHEST: desiredSequence = LookupSequence( "head_flinch" ); break;
		case HITGROUP_SHIELD: desiredSequence = 0; break;
		default: desiredSequence = LookupSequence( "gut_flinch" ); break;
		}
		if ( desiredSequence == -1 )
			desiredSequence = 0;
		break;
	case ACT_HOLDBOMB:
		strcpy( animation, (pev->flags & FL_DUCKING) ? "crouch_aim_" : "ref_aim_" );
		strcat( animation, m_szAnimExtention );
		desiredSequence = LookupSequence( animation );
		if ( desiredSequence == -1 )
			desiredSequence = 0;
		m_Activity = m_IdealActivity;
		break;
	case ACT_WALK:
		if ( (m_Activity != ACT_FLINCH && m_Activity != ACT_LARGE_FLINCH &&
			m_Activity != ACT_RANGE_ATTACK1 && m_Activity != ACT_RANGE_ATTACK2 && m_Activity != ACT_RELOAD) || m_fSequenceFinished )
		{
			if ( speed > 135 && gpGlobals->time > m_flLastFired + 4.0 )
			{
				strcpy( animation, "run_" );
				strcat( animation, m_szAnimExtention );
				desiredSequence = LookupSequence( animation );
				if ( desiredSequence == -1 )
				{
					strcpy( animation, (pev->flags & FL_DUCKING) ? "crouch_aim_" : "ref_aim_" );
					strcat( animation, m_szAnimExtention );
					desiredSequence = LookupSequence( animation );
					m_Activity = ACT_RUN;
					if ( desiredSequence == -1 )
						desiredSequence = 0;
					pev->gaitsequence = LookupActivity( ACT_RUN );
				}
				else
				{
					m_Activity = ACT_RUN;
					pev->gaitsequence = desiredSequence;
				}
				if ( m_Activity == ACT_RUN && pev->velocity.Length() > 150 )
					TheBots->OnEvent( EVENT_PLAYER_FOOTSTEP, this, NULL );
			}
			else
			{
				strcpy( animation, (pev->flags & FL_DUCKING) ? "crouch_aim_" : "ref_aim_" );
				strcat( animation, m_szAnimExtention );
				desiredSequence = LookupSequence( animation );
				if ( desiredSequence == -1 )
					desiredSequence = 0;
				m_Activity = ACT_WALK;
			}
		}
		else
			desiredSequence = pev->sequence;
		pev->gaitsequence = LookupActivity( speed <= 135 ? ACT_WALK : ACT_RUN );
		break;
	default:
		if ( m_Activity == m_IdealActivity )
			return;
		m_Activity = m_IdealActivity;
		desiredSequence = LookupActivity( m_IdealActivity );
		if ( desiredSequence != pev->sequence )
		{
			pev->gaitsequence = 0;
			pev->sequence = desiredSequence;
			pev->frame = 0;
			ResetSequenceInfo();
		}
		return;
	}
	if ( pev->gaitsequence != hopSequence && pev->gaitsequence != leapSequence )
	{
		if ( pev->flags & FL_DUCKING )
			pev->gaitsequence = LookupActivity( speed == 0 ? ACT_CROUCHIDLE : ACT_CROUCH );
		else if ( speed <= 135 )
			pev->gaitsequence = LookupActivity( speed <= 0 ? ACT_IDLE : ACT_WALK );
		else
		{
			if ( gpGlobals->time > m_flLastFired + 4.0 && m_Activity != ACT_FLINCH && m_Activity != ACT_LARGE_FLINCH )
			{
				strcpy( animation, "run_" );
				strcat( animation, m_szAnimExtention );
				desiredSequence = LookupSequence( animation );
				if ( desiredSequence != -1 )
				{
					m_Activity = ACT_RUN;
					pev->gaitsequence = desiredSequence;
				}
				else
				{
					strcpy( animation, (pev->flags & FL_DUCKING) ? "crouch_aim_" : "ref_aim_" );
					strcat( animation, m_szAnimExtention );
					desiredSequence = LookupSequence( animation );
					m_Activity = ACT_RUN;
					pev->gaitsequence = LookupActivity( ACT_RUN );
				}
			}
			else
				pev->gaitsequence = LookupActivity( ACT_RUN );
		}
	}
	if ( desiredSequence != pev->sequence )
	{
		pev->sequence = desiredSequence;
		pev->frame = 0;
		ResetSequenceInfo();
	}
}

void CBasePlayer::StudioPlayerBlend( int* blend, float* pitch )
{
	float scaledPitch = (float)(3.0 * *pitch);
	*blend = (int)scaledPitch;
	if ( *blend <= -45 )
		*blend = 255;
	else if ( *blend >= 45 )
		*blend = 0;
	else
	{
		float blendValue = (float)((45.0 - *blend) * 255.0 / 90.0);
		*blend = (int)blendValue;
	}
	*pitch = 0;
}

void CBasePlayer::CalculatePitchBlend()
{
	float pitch = pev->angles.x;
	int blend;
	StudioPlayerBlend( &blend, &pitch );
	pev->blending[1] = (byte)blend;
	m_flPitch = (float)blend;
}

void CBasePlayer::StudioEstimateGait()
{
	float dt = gpGlobals->frametime;
	if ( dt < 0 )
		dt = 0;
	else if ( dt > 1.0 )
		dt = 1;

	if ( dt == 0 )
	{
		m_flGaitMovement = 0;
		return;
	}

	Vector est_velocity = pev->origin - m_prevgaitorigin;
	m_prevgaitorigin = pev->origin;
	m_flGaitMovement = est_velocity.Length();

	if ( dt <= 0 || m_flGaitMovement / dt < 5 )
	{
		m_flGaitMovement = 0;
		est_velocity.x = 0;
		est_velocity.y = 0;
	}

	if ( est_velocity.x == 0 && est_velocity.y == 0 )
	{
		float flYawDiff = pev->angles.y - m_flGaityaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if ( flYawDiff > 180 )
			flYawDiff -= 360;
		if ( flYawDiff < -180 )
			flYawDiff += 360;

		float flYaw = fmod( pev->angles.y - m_flGaityaw, 360.0 );
		if ( flYaw < -180 )
			flYaw += 360;
		else if ( flYaw > 180 )
			flYaw -= 360;

		if ( flYaw > -5.0f && flYaw < 5.0f )
			m_flYawModifier = 0.05f;
		if ( flYaw < -90.0f || flYaw > 90.0f )
			m_flYawModifier = 3.5f;

		if ( dt < 0.25 )
			flYawDiff *= dt * m_flYawModifier;
		else
			flYawDiff *= dt;

		if ( fabs( flYawDiff ) < 0.1 )
			flYawDiff = 0;

		m_flGaityaw += flYawDiff;
		m_flGaityaw -= (int)(m_flGaityaw / 360) * 360;
		m_flGaitMovement = 0;
	}
	else
	{
		m_flGaityaw = atan2( (double)est_velocity.y, (double)est_velocity.x ) * 180 / M_PI;
		if ( m_flGaityaw > 180 )
			m_flGaityaw = 180;
		else if ( m_flGaityaw < -180 )
			m_flGaityaw = -180;
	}
}

void CBasePlayer::CalculateYawBlend()
{
	StudioEstimateGait();

	float flYaw = fmod( pev->angles.y - m_flGaityaw, 360.0 );
	if ( flYaw < -180 )
		flYaw += 360;
	else if ( flYaw > 180 )
		flYaw -= 360;

	if ( m_flGaitMovement != 0 )
	{
		if ( flYaw > 120 )
		{
			m_flGaityaw -= 180;
			m_flGaitMovement = -m_flGaitMovement;
			flYaw -= 180;
		}
		else if ( flYaw < -120 )
		{
			m_flGaityaw += 180;
			m_flGaitMovement = -m_flGaitMovement;
			flYaw += 180;
		}
	}

	float blend_yaw = (flYaw / 90.0) * 128.0 + 127.0;
	if ( blend_yaw > 255.0f )
		blend_yaw = 255.0f;
	if ( blend_yaw < 0.0f )
		blend_yaw = 0.0f;
	blend_yaw = 255.0 - blend_yaw;

	pev->blending[0] = (int)blend_yaw;
	m_flYaw = blend_yaw;
}

void CBasePlayer::StudioProcessGait()
{
	float dt = gpGlobals->frametime;
	if ( dt < 0 )
		dt = 0;
	else if ( dt > 1.0 )
		dt = 1;

	CalculateYawBlend();
	CalculatePitchBlend();

	studiohdr_t* model = (studiohdr_t*)GET_MODEL_PTR( edict() );
	if ( !model )
		return;

	mstudioseqdesc_t* sequence = (mstudioseqdesc_t*)((byte*)model + model->seqindex) + pev->gaitsequence;
	if ( sequence->linearmovement.x > 0 )
		m_flGaitframe += (m_flGaitMovement / sequence->linearmovement.x) * sequence->numframes;
	else
		m_flGaitframe += dt * sequence->fps * pev->framerate;

	m_flGaitframe = m_flGaitframe - (int)(m_flGaitframe / sequence->numframes) * sequence->numframes;
	if ( m_flGaitframe < 0 )
		m_flGaitframe += sequence->numframes;
}

void CBasePlayer::UpdatePlayerSound()
{
	CSound* sound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
	if ( !sound )
	{
		ALERT( at_console, "Client lost reserved sound!\n" );
		return;
	}
	sound->m_iType = bits_SOUND_NONE;
	int bodyVolume = 0;
	if ( pev->flags & FL_ONGROUND )
	{
		bodyVolume = (int)pev->velocity.Length();
		if ( bodyVolume > 512 )
			bodyVolume = 512;
	}
	if ( pev->button & IN_JUMP )
		bodyVolume += 100;
	if ( m_iWeaponVolume > bodyVolume )
	{
		m_iTargetVolume = m_iWeaponVolume;

		// OR in the bits for COMBAT sound if the weapon is being louder than the player. 
		sound->m_iType |= bits_SOUND_COMBAT;
	}
	else
		m_iTargetVolume = bodyVolume;
	m_iWeaponVolume = (int)(m_iWeaponVolume - 250 * gpGlobals->frametime);
	int volume = sound->m_iVolume;
	if ( volume < m_iTargetVolume )
		volume = m_iTargetVolume;
	else if ( volume > m_iTargetVolume )
	{
		volume = (int)(volume - 250 * gpGlobals->frametime);
		if ( volume < m_iTargetVolume )
			volume = 0;
	}
	if ( m_fNoPlayerSound )
		volume = 0;
	if ( gpGlobals->time > m_flStopExtraSoundTime )
		m_iExtraSoundTypes = 0;
	sound->m_vecOrigin = pev->origin;
	sound->m_iVolume = volume;
	sound->m_iType |= bits_SOUND_PLAYER | m_iExtraSoundTypes;
	m_iWeaponFlash = (int)(m_iWeaponFlash - 256 * gpGlobals->frametime);
	if ( m_iWeaponFlash < 0 )
		m_iWeaponFlash = 0;
	UTIL_MakeVectors( pev->angles );
	gpGlobals->v_forward.z = 0;
}

bool CBasePlayer::IsHittingShield(const Vector &vecDirection, TraceResult *ptr)
{
	if (m_pActiveItem && m_pActiveItem->m_iId == WEAPON_C4)
		return false;
	if (m_bOwnsShield && ptr->iHitgroup != HITGROUP_SHIELD)
	{
		if (m_bShieldDrawn)
		{
			UTIL_MakeVectors(pev->angles);
			(void)vecDirection.Length2D();
			return false;
		}
		return false;
	}
	return m_bOwnsShield;
}

//=========================================================
// TraceAttack
//=========================================================
void CBasePlayer::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	bool shieldHit = IsHittingShield(vecDir, ptr);
	CBasePlayer *pAttacker = (CBasePlayer *)CBaseEntity::Instance(pevAttacker);
	bool damageAllowed = m_iTeam != pAttacker->m_iTeam || CVAR_GET_FLOAT("mp_friendlyfire") != 0;
	if (pev->takedamage == DAMAGE_NO)
		return;
	m_LastHitGroup = ptr->iHitgroup;
	bool bleed = false;
	if (shieldHit)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, RANDOM_LONG(0, 1) ? "weapons/ric_metal-1.wav" : "weapons/ric_metal-2.wav", VOL_NORM, ATTN_NORM);
		UTIL_Sparks(ptr->vecEndPos);
		pev->punchangle.x = RANDOM_FLOAT(-0.15f, 0.15f) * 0;
		pev->punchangle.z = RANDOM_FLOAT(-0.15f, 0.15f) * 0;
		if (pev->punchangle.x < 4)
			pev->punchangle.x = -4;
		if (pev->punchangle.z < -5)
			pev->punchangle.z = -5;
		else if (pev->punchangle.z > 5)
			pev->punchangle.z = 5;
		flDamage = 0;
	}
	else
	{
		switch (ptr->iHitgroup)
		{
		case HITGROUP_HEAD:
			flDamage *= 4;
			if (m_iKevlar == 2)
			{
				MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, ptr->vecEndPos);
					WRITE_BYTE(TE_STREAK_SPLASH);
					WRITE_COORD(ptr->vecEndPos.x);
					WRITE_COORD(ptr->vecEndPos.y);
					WRITE_COORD(ptr->vecEndPos.z);
					WRITE_COORD(ptr->vecPlaneNormal.x);
					WRITE_COORD(ptr->vecPlaneNormal.y);
					WRITE_COORD(ptr->vecPlaneNormal.z);
					WRITE_BYTE(5);
					WRITE_SHORT(22);
					WRITE_SHORT(25);
					WRITE_SHORT(65);
				MESSAGE_END();
			}
			else if (damageAllowed)
			{
				pev->punchangle.x = flDamage * -0.5f;
				if (pev->punchangle.x < -12)
					pev->punchangle.x = -12;
				pev->punchangle.z = RANDOM_FLOAT(-1, 1) * flDamage;
				if (pev->punchangle.z < -9)
					pev->punchangle.z = -9;
				else if (pev->punchangle.z > 9)
					pev->punchangle.z = 9;
				bleed = true;
			}
			break;
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
			if (ptr->iHitgroup == HITGROUP_STOMACH)
				flDamage *= 1.25f;
			if (!m_iKevlar && damageAllowed)
			{
				pev->punchangle.x = flDamage * -0.1;
				if (pev->punchangle.x < -4)
					pev->punchangle.x = -4;
				bleed = true;
			}
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			bleed = !m_iKevlar && damageAllowed;
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			flDamage *= 0.75f;
			bleed = damageAllowed;
			break;
		default:
			bleed = damageAllowed;
			break;
		}
		if (bleed)
		{
			BloodSplat(ptr->vecEndPos, vecDir, ptr->iHitgroup, (int)(flDamage * 5));
			SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage); // a little surface blood.
			TraceBleed(flDamage, vecDir, ptr, bitsDamageType);
		}
	}
	AddMultiDamage(pevAttacker, this, flDamage, bitsDamageType);
}

void CBasePlayer::Pain(int hitgroup, bool hasArmour)
{
	int sound = RANDOM_LONG(0, 2);
	const char *sample;
	if (hitgroup == HITGROUP_HEAD)
	{
		if (m_iKevlar == 2)
			sample = "player/bhit_helmet-1.wav";
		else if (sound == 0)
			sample = "player/headshot1.wav";
		else if (sound == 1)
			sample = "player/headshot2.wav";
		else
			sample = "player/headshot3.wav";
	}
	else if (hitgroup != HITGROUP_LEFTLEG && hitgroup != HITGROUP_RIGHTLEG && hasArmour)
		sample = "player/bhit_kevlar-1.wav";
	else if (sound == 0)
		sample = "player/bhit_flesh-1.wav";
	else if (sound == 1)
		sample = "player/bhit_flesh-2.wav";
	else
		sample = "player/bhit_flesh-3.wav";
	EMIT_SOUND(ENT(pev), CHAN_VOICE, sample, VOL_NORM, ATTN_NORM);
}

void CBasePlayer::SpawnClientSideCorpse(void)
{
	const char *model = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(edict()), "model");
	MESSAGE_BEGIN(MSG_ALL, gmsgSendCorpse);
		WRITE_STRING(model);
		WRITE_LONG((int)(128.0 * pev->origin.x));
		WRITE_LONG((int)(128.0 * pev->origin.y));
		WRITE_LONG((int)(128.0 * pev->origin.z));
		WRITE_COORD(pev->angles.x);
		WRITE_COORD(pev->angles.y);
		WRITE_COORD(pev->angles.z);
		WRITE_LONG((int)((pev->animtime - gpGlobals->time) * 100));
		WRITE_BYTE(pev->sequence);
		WRITE_BYTE(pev->body);
		WRITE_BYTE(m_iTeam);
		WRITE_BYTE(entindex());
	MESSAGE_END();
	m_canSwitchObserverModes = true;
	if (TheTutor)
		TheTutor->OnEvent(GameEventType::EVENT_CLIENT_CORPSE_SPAWNED, this, NULL);
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4700 4701)
#endif
int CBasePlayer::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	if (bitsDamageType & (DMG_EXPLOSION | DMG_BLAST | DMG_FALL))
		m_LastHitGroup = HITGROUP_GENERIC;
	else if (m_LastHitGroup == HITGROUP_SHIELD && (bitsDamageType & DMG_BULLET))
		return 0;

	float shieldBonus = m_bOwnsShield ? 0.2f : 0;
	float armorRatio = m_bIsVIP ? 0.25f : 0.5f;
	bool explosive = (bitsDamageType & (DMG_EXPLOSION | DMG_BLAST)) != 0;
	CBasePlayer *pAttacker = NULL;
	int teamAttack = 0;
	int armorHit = 0;
	int healthHit;
	if (!explosive)
	{
		if (!g_pGameRules->FPlayerCanTakeDamage(this, CBaseEntity::Instance(pevAttacker)) && !FClassnameIs(pevInflictor, "grenade"))
			return 0;
		float armorBonus = (bitsDamageType & DMG_BLAST) && g_pGameRules->IsMultiplayer() ? 1.0f : 0.5f;
		float armorScale = armorBonus == 1.0f ? 1.0f : 2.0f;
		if (!IsAlive())
			return 0;
		CBaseEntity *pEntity = GetClassPtr((CBaseEntity *)pevAttacker);
		if (pEntity && pEntity->IsPlayer())
		{
			pAttacker = GetClassPtr((CBasePlayer *)pevAttacker);
			if (pAttacker != this && pAttacker->m_iTeam == m_iTeam)
			{
				if (!(m_flDisplayHistory & 0x200))
				{
					m_flDisplayHistory |= 0x200;
					if (pAttacker->IsAlive() && pAttacker->m_bShowHints)
						pAttacker->m_hintMessageQueue.AddMessage("#Hint_try_not_to_injure_teammates", 6, true, NULL);
				}
				if (gpGlobals->time > pAttacker->m_flLastAttackedTeammate + 0.6)
				{
					pAttacker->m_flLastAttackedTeammate = gpGlobals->time;
					CBaseEntity *pPlayer = UTIL_FindEntityByClassname(NULL, "player");
					while (pPlayer && !FNullEnt(pPlayer->pev))
					{
						CBasePlayer *pTeammate = GetClassPtr((CBasePlayer *)pPlayer->pev);
						if (pTeammate->m_iTeam == m_iTeam)
							ClientPrint(pTeammate->pev, HUD_PRINTTALK, "#Game_teammate_attack", STRING(pAttacker->pev->netname));
						pPlayer = UTIL_FindEntityByClassname(pPlayer, "player");
					}
				}
				teamAttack = 1;
			}
			if (pAttacker->m_iTeam == m_iTeam)
				flDamage *= 0.35;
			unsigned int gunType;
			if (pAttacker->m_pActiveItem)
			{
				gunType = pAttacker->m_pActiveItem->m_iId;
				armorRatio += shieldBonus;
				switch (gunType)
				{
				case WEAPON_P228: armorRatio *= 1.25; break;
				case WEAPON_SCOUT:
				case WEAPON_KNIFE: armorRatio *= 1.7; break;
				case WEAPON_MAC10: armorRatio *= 0.95; break;
				case WEAPON_AUG:
				case WEAPON_FAMAS:
				case WEAPON_M4A1:
				case WEAPON_SG552: armorRatio *= 1.4; break;
				case WEAPON_ELITE:
				case WEAPON_GLOCK18: armorRatio *= 1.05; break;
				case WEAPON_FIVESEVEN:
				case WEAPON_M249:
				case WEAPON_DEAGLE:
				case WEAPON_P90: armorRatio *= 1.5; break;
				case WEAPON_SG550: armorRatio *= 1.45; break;
				case WEAPON_GALIL:
				case WEAPON_AK47: armorRatio *= 1.55; break;
				case WEAPON_AWP: armorRatio *= 1.95; break;
				case WEAPON_G3SG1: armorRatio *= 1.65; break;
				}
			}
			bool largeFlinch = false;
			if (!(pev->flags & FL_DUCKING) && m_LastHitGroup != HITGROUP_LEFTLEG && m_LastHitGroup != HITGROUP_RIGHTLEG)
			{
				switch (gunType)
				{
				case WEAPON_SCOUT:
				case WEAPON_AUG:
				case WEAPON_SG550:
				case WEAPON_GALIL:
				case WEAPON_FAMAS:
				case WEAPON_AWP:
				case WEAPON_M3:
				case WEAPON_M4A1:
				case WEAPON_G3SG1:
				case WEAPON_DEAGLE:
				case WEAPON_SG552:
				case WEAPON_AK47:
					largeFlinch = true;
					break;
				}
			}
			if (largeFlinch)
			{
				if (pev->velocity.Length() < 300)
				{
					pev->velocity = pev->velocity + (pev->origin - pAttacker->pev->origin).Normalize() * 170;
					m_flVelocityModifier = 0.65f;
				}
				SetAnimation(PLAYER_LARGE_FLINCH);
			}
			else
			{
				m_flVelocityModifier = 0.5f;
				m_bHighDamage = flDamage > (m_LastHitGroup == HITGROUP_HEAD ? 60 : 20);
				SetAnimation(PLAYER_FLINCH);
			}
		}
		healthHit = m_lastDamageAmount = (int)flDamage;
		if (pev->armorvalue != 0 && !(bitsDamageType & (DMG_FALL | DMG_DROWN)) && IsArmored(m_LastHitGroup))
		{
			float healthDamage = armorRatio * flDamage;
			float armorDamage = (flDamage - healthDamage) * armorBonus;
			if (armorDamage > pev->armorvalue)
			{
				healthDamage = flDamage - pev->armorvalue * armorScale;
				pev->armorvalue = 0;
				armorHit = (int)armorDamage;
			}
			else
			{
				float oldArmor = pev->armorvalue;
				pev->armorvalue -= armorDamage >= 0 ? armorDamage : 1;
				armorHit = (int)oldArmor - (int)pev->armorvalue;
			}
			if (pev->armorvalue <= 0)
				m_iKevlar = 0;
			Pain(m_LastHitGroup, true);
			healthHit = (int)healthDamage;
		}
		else
			Pain(m_LastHitGroup, false);
	}
	else
	{
		if (!IsAlive())
			return 0;
		if ((bitsDamageType & DMG_EXPLOSION) && FClassnameIs(GetClassPtr((CBaseEntity *)pevInflictor)->pev, "grenade"))
		{
			CGrenade *pGrenade = GetClassPtr((CGrenade *)pevInflictor);
			if (CVAR_GET_FLOAT("mp_friendlyfire") == 0)
			{
				if (pGrenade->m_iTeam == m_iTeam && pevAttacker != VARS(edict()))
					return 0;
			}
			else
			{
				teamAttack = pGrenade->m_iTeam == m_iTeam;
				pAttacker = dynamic_cast<CBasePlayer *>(CBaseEntity::Instance(pevAttacker));
			}
		}
		if (!FNullEnt(pevInflictor))
			m_vBlastVector = pev->origin - pevInflictor->origin;
		float healthDamage = flDamage;
		if (pev->armorvalue != 0 && IsArmored(m_LastHitGroup))
		{
			healthDamage = armorRatio * flDamage;
			float armorDamage = (flDamage - healthDamage) * 0.5f;
			if (armorDamage > pev->armorvalue)
			{
				healthDamage = flDamage - pev->armorvalue;
				pev->armorvalue = 0;
				armorHit = (int)armorDamage;
			}
			else
			{
				float oldArmor = pev->armorvalue;
				pev->armorvalue -= armorDamage >= 0 ? armorDamage : 1;
				armorHit = (int)oldArmor - (int)pev->armorvalue;
			}
			if (pev->armorvalue <= 0)
				m_iKevlar = 0;
			Pain(m_LastHitGroup, true);
		}
		else
			Pain(m_LastHitGroup, false);
		healthHit = m_lastDamageAmount = (int)healthDamage;
		if (healthHit < pev->health)
		{
			SetAnimation(PLAYER_FLINCH);
			Pain(m_LastHitGroup, false);
		}
		else if (bitsDamageType & DMG_BLAST)
			m_bKilledByBomb = true;
		else if (bitsDamageType & DMG_EXPLOSION)
			m_bKilledByGrenade = true;
	}

	LogAttack(pAttacker, this, teamAttack, healthHit, armorHit, (int)pev->health - healthHit, (int)pev->armorvalue, GetWeaponName(pevInflictor, pevAttacker));
	int tookDamage = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, (float)healthHit, bitsDamageType);
	if (tookDamage > 0)
	{
		if (TheBots)
			TheBots->OnEvent(GameEventType::EVENT_PLAYER_TOOK_DAMAGE, this, pAttacker);
		if (((CHalfLifeMultiplay *)g_pGameRules)->IsCareer())
		{
			for (int i = 1; i <= gpGlobals->maxClients; ++i)
			{
				CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (pPlayer && !pPlayer->IsBot() && pevAttacker == pPlayer->pev && m_iTeam != pPlayer->m_iTeam)
					TheCareerTasks->HandleEnemyInjury(GetWeaponName(pevInflictor, pevAttacker), pPlayer->m_bOwnsShield, pPlayer);
			}
		}
	}
	for (int i = 0; i < CDMG_TIMEBASED; ++i)
	{
		if (bitsDamageType & (DMG_PARALYZE << i))
			m_rgbTimeBasedDamage[i] = 0;
	}

	// tell director about it
	MESSAGE_BEGIN(MSG_SPEC, SVC_DIRECTOR);
		WRITE_BYTE(9);	// command length in bytes
		WRITE_BYTE(DRC_CMD_EVENT);	// take damage event
		WRITE_SHORT(entindex());
		WRITE_SHORT(ENTINDEX(ENT(pevInflictor)));	// index number of secondary entity
		WRITE_LONG(5);   // eventflags (priority and flags)
	MESSAGE_END();
	MESSAGE_BEGIN(MSG_SPEC, gmsgHLTV);
		WRITE_BYTE(entindex());
		WRITE_BYTE(pev->health <= 0 ? 128 : ((int)pev->health | 128));
	MESSAGE_END();
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (pPlayer && (CBaseEntity *)pPlayer->m_hObserverTarget == this)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgSpecHealth, NULL, pPlayer->edict());
				WRITE_BYTE(pev->health > 0 ? (int)pev->health : 0);
			MESSAGE_END();
		}
	}
	if (!explosive)
	{
		m_bitsDamageType |= bitsDamageType;
		m_bitsHUDDamage = -1;
	}
	return tookDamage;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
								// Better solution:  Add as parameter to all Killed() functions.

void CBasePlayer::Killed(entvars_t *pevAttacker, int iGib)
{
	StopDrivingVehicle();
	m_canSwitchObserverModes = false;
	if (m_LastHitGroup == HITGROUP_HEAD)
		m_bHeadshotKilled = true;
	TheBots->OnEvent(GameEventType::EVENT_PLAYER_DIED, this, CBaseEntity::Instance(pevAttacker));
	if (((CHalfLifeMultiplay *)g_pGameRules)->IsCareer())
	{
		if (!IsBot())
			TheCareerTasks->HandleEvent(GameEventType::EVENT_DIE, NULL, this);
		TheCareerTasks->HandleDeath(m_iTeam, this);
		if (!m_bKilledByBomb)
		{
			CBaseEntity *pKiller = CBaseEntity::Instance(pevAttacker);
			bool killerHasShield = pKiller->IsPlayer() && ((CBasePlayer *)pKiller)->m_bOwnsShield;
			bool wasBlind = IsBot() && m_blindUntilTime > gpGlobals->time;
			for (int i = 1; i <= gpGlobals->maxClients; ++i)
			{
				CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (pPlayer && !pPlayer->IsBot() && pevAttacker == pPlayer->pev && m_iTeam != pPlayer->m_iTeam)
					TheCareerTasks->HandleEnemyKill(wasBlind, GetWeaponName(g_pevLastInflictor, pevAttacker), m_bHeadshotKilled, killerHasShield, this, pPlayer);
			}
		}
	}
	if (!m_bKilledByBomb)
		g_pGameRules->PlayerKilled(this, pevAttacker, g_pevLastInflictor);
	MESSAGE_BEGIN(MSG_ONE, gmsgNVGToggle, NULL, edict());
		WRITE_BYTE(0);
	MESSAGE_END();
	m_bNightVisionOn = false;
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (pPlayer && pPlayer->pev->flags != FL_DORMANT && !FNullEnt(pev)
			&& pPlayer->pev->iuser1 == OBS_IN_EYE && pPlayer->pev->iuser2 == entindex())
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgNVGToggle, NULL, pPlayer->edict());
				WRITE_BYTE(0);
			MESSAGE_END();
			pPlayer->m_bNightVisionOn = false;
		}
	}
	if (m_pTank)
	{
		m_pTank->Use(this, this, USE_OFF, 0);
		m_pTank = NULL;
	}
	CSound *pSound = CSoundEnt::SoundPointerForIndex(CSoundEnt::ClientSoundIndex(edict()));
	if (pSound)
		pSound->Reset();
	SetAnimation(PLAYER_DIE);
	if (m_pActiveItem && m_pActiveItem->m_pPlayer)
	{
		CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon *)m_pActiveItem;
		switch (m_pActiveItem->m_iId)
		{
		case WEAPON_HEGRENADE:
			if ((pev->button & IN_ATTACK) && m_rgAmmo[pWeapon->m_iPrimaryAmmoType] != 0)
				CGrenade::ShootTimed2(pev, pev->origin + pev->view_ofs, pev->angles, 1.5f, m_iTeam, ((CHEGrenade *)m_pActiveItem)->m_usCreateExplosion);
			break;
		case WEAPON_SMOKEGRENADE:
			if ((pev->button & IN_ATTACK) && m_rgAmmo[pWeapon->m_iPrimaryAmmoType] != 0)
				CGrenade::ShootSmokeGrenade(pev, pev->origin + pev->view_ofs, pev->angles, 1.5f, ((CSmokeGrenade *)m_pActiveItem)->m_usCreateSmoke);
			break;
		case WEAPON_FLASHBANG:
			if ((pev->button & IN_ATTACK) && m_rgAmmo[pWeapon->m_iPrimaryAmmoType] != 0)
				CGrenade::ShootTimed(pev, pev->origin + pev->view_ofs, pev->angles, 1.5f);
			break;
		}
	}
	pev->modelindex = m_modelIndexPlayer;
	pev->deadflag = DEAD_DYING;
	pev->movetype = MOVETYPE_TOSS;
	pev->takedamage = DAMAGE_NO;
	pev->gamestate = 1;
	m_bShieldDrawn = false;
	pev->flags &= ~FL_ONGROUND;
	if (fadetoblack.value == 0)
	{
		pev->iuser1 = OBS_CHASE_FREE;
		pev->iuser2 = entindex();
		pev->iuser3 = ENTINDEX(ENT(pevAttacker));
		m_hObserverTarget = UTIL_PlayerByIndex(pev->iuser3);
		MESSAGE_BEGIN(MSG_ONE, gmsgADStop, NULL, edict());
		MESSAGE_END();
	}
	else
		UTIL_ScreenFade(this, g_vecZero, 3, 3, 255, FFADE_OUT | FFADE_STAYOUT);
	SetScoreboardAttributes(NULL);
	if (m_iThrowDirection != THROW_NONE)
	{
		switch (m_iThrowDirection)
		{
		case THROW_FORWARD:
		case THROW_BACKWARD:
			UTIL_MakeVectors(pev->angles);
			pev->velocity = gpGlobals->v_forward * (m_iThrowDirection == THROW_FORWARD ? RANDOM_FLOAT(100, 200) : RANDOM_FLOAT(-100, -200));
			pev->velocity.z = RANDOM_FLOAT(50, 100);
			break;
		case THROW_HITVEL:
			if (FClassnameIs(pevAttacker, "player"))
			{
				UTIL_MakeVectors(pevAttacker->angles);
				pev->velocity = gpGlobals->v_forward * RANDOM_FLOAT(200, 300);
				pev->velocity.z = RANDOM_FLOAT(200, 300);
			}
			break;
		case THROW_BOMB:
		{
			float speed = 2300 - m_vBlastVector.Length();
			pev->velocity = m_vBlastVector / m_vBlastVector.Length() * speed * 0.25f;
			pev->velocity.z = (2300 - m_vBlastVector.Length()) / 2.75f;
			break;
		}
		case THROW_GRENADE:
		{
			float speed = 500 - m_vBlastVector.Length();
			pev->velocity = m_vBlastVector / m_vBlastVector.Length() * speed;
			pev->velocity.z = (350 - m_vBlastVector.Length()) * 1.5f;
			break;
		}
		case THROW_HITVEL_MINUS_AIRVEL:
			if (FClassnameIs(pevAttacker, "player"))
			{
				UTIL_MakeVectors(pevAttacker->angles);
				pev->velocity = gpGlobals->v_forward * RANDOM_FLOAT(200, 300);
			}
			break;
		}
		pev->angles.y = UTIL_VecToAngles(-pev->velocity).y;
		pev->v_angle.y = pev->angles.y;
		m_iThrowDirection = THROW_NONE;
	}
	m_iClientHealth = 0;
	MESSAGE_BEGIN(MSG_ONE, gmsgHealth, NULL, edict());
		WRITE_BYTE(m_iClientHealth);
	MESSAGE_END();

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN(MSG_ONE, gmsgCurWeapon, NULL, edict());
		WRITE_BYTE(0);
		WRITE_BYTE(255);
		WRITE_BYTE(255);
	MESSAGE_END();
	pev->fov = 0;
	m_iClientFOV = 0;
	m_iFOV = 0;
	MESSAGE_BEGIN(MSG_ONE, gmsgSetFOV, NULL, edict());
		WRITE_BYTE(0);
	MESSAGE_END();
	((CHalfLifeMultiplay *)g_pGameRules)->CheckWinConditions();
	m_bNotKilled = false;
	if (m_bHasC4)
	{
		DropPlayerItem("weapon_c4");
		SetProgressBarTime(0);
	}
	else if (m_bHasDefuser)
	{
		m_bHasDefuser = false;
		pev->body = 0;
		GiveNamedItem("item_thighpack");
		MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, NULL, edict());
			WRITE_BYTE(0);
			WRITE_STRING("defuser");
		MESSAGE_END();
		SendItemStatus(this);
	}
	if (m_bIsDefusing)
		SetProgressBarTime(0);
	m_bIsDefusing = false;
	MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, NULL, edict());
		WRITE_BYTE(0);
		WRITE_STRING("buyzone");
	MESSAGE_END();
	if (m_iMenu >= Menu_Buy && m_iMenu <= Menu_BuyItem)
		CLIENT_COMMAND(edict(), "slot10\n");
	else if (m_iMenu == Menu_ClientBuy)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgBuyClose, NULL, edict());
		MESSAGE_END();
	}
	SetThink(&CBasePlayer::PlayerDeathThink);
	pev->nextthink = gpGlobals->time + 0.1;
	pev->solid = SOLID_NOT;
	if (m_bPunishedForTK)
	{
		m_bPunishedForTK = false;
		m_hintMessageQueue.AddMessage("#Hint_cannot_play_because_tk", 6, true, NULL);
	}

	// UNDONE: Put this in, but add FFADE_PERMANENT and make fade time 8.8 instead of 4.12
	// UTIL_ScreenFade( edict(), Vector(128,0,0), 6, 15, 255, FFADE_OUT | FFADE_MODULATE );

	if ((pev->health < -9000 && iGib != GIB_NEVER) || iGib == GIB_ALWAYS)
	{
		pev->solid = SOLID_NOT;
		GibMonster();	// This clears pev->model
		pev->effects |= EF_NODRAW;
		((CHalfLifeMultiplay *)g_pGameRules)->CheckWinConditions();
		return;
	}
	DeathSound();
	pev->angles.x = 0;
	pev->angles.z = 0;
	if (!(m_flDisplayHistory & 0x400000))
	{
		m_hintMessageQueue.AddMessage("#Spec_Duck", 6, true, NULL);
		m_flDisplayHistory |= 0x400000;
	}
}

void CBasePlayer::UpdateClientData()
{
	if ( m_fInitHUD )
	{
		gInitHUD = FALSE;
		m_fInitHUD = FALSE;
		m_signals.Update();
		MESSAGE_BEGIN( MSG_ONE, gmsgResetHUD, NULL, pev );
		MESSAGE_END();
		if ( !m_fGameHUDInitialized )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgInitHUD, NULL, pev );
			MESSAGE_END();
			CClientFog* fog = (CClientFog*)UTIL_FindEntityByClassname( NULL, "env_fog" );
			if ( fog != NULL )
			{
				int red = (int)fog->pev->rendercolor.x;
				int green = (int)fog->pev->rendercolor.y;
				int blue = (int)fog->pev->rendercolor.z;
				red = red > 254 ? 255 : (red < 0 ? 0 : red);
				green = green > 254 ? 255 : (green < 0 ? 0 : green);
				blue = blue > 254 ? 255 : (blue < 0 ? 0 : blue);
				signed char density[sizeof( float )];
				memcpy( density, &fog->m_fDensity, sizeof( density ) );
				MESSAGE_BEGIN( MSG_ONE, gmsgFog, NULL, pev );
				WRITE_BYTE( red );
				WRITE_BYTE( green );
				WRITE_BYTE( blue );
				WRITE_BYTE( density[0] );
				WRITE_BYTE( density[1] );
				WRITE_BYTE( density[2] );
				WRITE_BYTE( density[3] );
				MESSAGE_END();
			}
			g_pGameRules->InitHUD( this );
			m_fGameHUDInitialized = TRUE;
			if ( g_pGameRules->IsMultiplayer() )
				FireTargets( "game_playerjoin", this, this, USE_TOGGLE, 0 );
			m_iObserverLastMode = OBS_ROAMING;
			m_iObserverC4State = 0;
			m_bObserverHasDefuser = false;
			m_bObserverAutoDirector = false;
		}
		FireTargets( "game_playerspawn", this, this, USE_TOGGLE, 0 );
		MESSAGE_BEGIN( MSG_ONE, gmsgMoney, NULL, pev );
		WRITE_LONG( m_iAccount );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		if ( m_bHasDefuser )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( "defuser" );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 160 );
			WRITE_BYTE( 0 );
			MESSAGE_END();
		}
		SetBombIcon( false );
		SyncRoundTimer();
		SendHostagePos();
		SendWeatherInfo();
		if ( g_pGameRules->IsMultiplayer() )
		{
			CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
			MESSAGE_BEGIN( MSG_ONE, gmsgTeamScore, NULL, pev );
			WRITE_STRING( "CT" );
			WRITE_SHORT( rules->m_iNumCTWins );
			MESSAGE_END();
			MESSAGE_BEGIN( MSG_ONE, gmsgTeamScore, NULL, pev );
			WRITE_STRING( "TERRORIST" );
			WRITE_SHORT( rules->m_iNumTerroristWins );
			MESSAGE_END();
		}
	}
	if ( m_iHideHUD != m_iClientHideHUD )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHideWeapon, NULL, pev );
		WRITE_BYTE( m_iHideHUD );
		MESSAGE_END();
		m_iClientHideHUD = m_iHideHUD;
	}
	if ( m_iFOV != m_iClientFOV )
	{
		pev->fov = (float)m_iFOV;
		MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
		WRITE_BYTE( m_iFOV );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_SPEC, gmsgHLTV );
		WRITE_BYTE( entindex() );
		WRITE_BYTE( m_iFOV );
		MESSAGE_END();

		// cache FOV change at end of function, so weapon updates can see that FOV has changed
	}

	// HACKHACK -- send the message to display the game title
	if ( gDisplayTitle )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgShowGameTitle, NULL, pev );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		gDisplayTitle = 0;
	}
	if ( (int)pev->health != m_iClientHealth )
	{
		int health = (int)pev->health;
		if ( pev->health > 255.0f )
			health = 255;
		else if ( pev->health < 0 )
			health = 0;
		if ( pev->health > 0 && pev->health <= 1.0f )
			health = 1;
		MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pev );
		WRITE_BYTE( health );
		MESSAGE_END();
		m_iClientHealth = (int)pev->health;
	}
	if ( (int)pev->armorvalue != m_iClientBattery )
	{
		m_iClientBattery = (int)pev->armorvalue;
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgBattery, NULL, pev );
		WRITE_SHORT( (int)pev->armorvalue );
		MESSAGE_END();
	}
	if ( pev->dmg_take != 0 || pev->dmg_save != 0 || m_bitsHUDDamage != m_bitsDamageType )
	{
		// Comes from inside me if not set
		Vector damageOrigin = pev->origin;
		if ( pev->dmg_inflictor != NULL )
		{
			CBaseEntity* inflictor = CBaseEntity::Instance( pev->dmg_inflictor );
			if ( inflictor != NULL )
				damageOrigin = inflictor->Center();
		}

		// only send down damage type that have hud art
		int visibleDamageBits = m_bitsDamageType & DMG_SHOWNHUD;
		MESSAGE_BEGIN( MSG_ONE, gmsgDamage, NULL, pev );
		WRITE_BYTE( (int)pev->dmg_save );
		WRITE_BYTE( (int)pev->dmg_take );
		WRITE_LONG( visibleDamageBits );
		WRITE_COORD( damageOrigin.x );
		WRITE_COORD( damageOrigin.y );
		WRITE_COORD( damageOrigin.z );
		MESSAGE_END();
		pev->dmg_take = 0;
		pev->dmg_save = 0;
		m_bitsHUDDamage = m_bitsDamageType;

		// Clear off non-time-based damage indicators
		m_bitsDamageType &= DMG_TIMEBASED;
	}
	if ( m_flFlashLightTime != 0 && !(gpGlobals->time < m_flFlashLightTime) )
	{
		if ( pev->effects & EF_DIMLIGHT )
		{
			if ( m_iFlashBattery )
			{
				m_flFlashLightTime = (float)(gpGlobals->time + FLASH_DRAIN_TIME);
				--m_iFlashBattery;
				if ( !m_iFlashBattery )
					FlashlightTurnOff();
			}
		}
		else if ( m_iFlashBattery > 99 )
		{
			m_flFlashLightTime = 0;
		}
		else
		{
			m_flFlashLightTime = (float)(gpGlobals->time + FLASH_CHARGE_TIME);
			++m_iFlashBattery;
		}
		MESSAGE_BEGIN( MSG_ONE, gmsgFlashBattery, NULL, pev );
		WRITE_BYTE( m_iFlashBattery );
		MESSAGE_END();
	}
	if ( m_iTrain & TRAIN_NEW )
	{
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgTrain, NULL, pev );
		WRITE_BYTE( m_iTrain & 0xF );
		MESSAGE_END();
		m_iTrain &= ~TRAIN_NEW;
	}
	for ( int i = 0; i < ARRAYSIZE( m_rgAmmo ); ++i )
	{
		if ( m_rgAmmo[i] != m_rgAmmoLast[i] )
		{
			m_rgAmmoLast[i] = m_rgAmmo[i];
			MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pev );
			WRITE_BYTE( i );
			WRITE_BYTE( m_rgAmmo[i] > 254 ? 254 : (m_rgAmmo[i] < 0 ? 0 : m_rgAmmo[i]) );
			MESSAGE_END();
		}
	}
	for ( int i = 0; i < ARRAYSIZE( m_rgpPlayerItems ); ++i )
	{
		if ( m_rgpPlayerItems[i] != NULL )  // each item updates it's successors
			m_rgpPlayerItems[i]->UpdateClientData( this );
	}

	// Cache and client weapon change
	m_pClientActiveItem = m_pActiveItem;
	m_iClientFOV = m_iFOV;

	// Update Status Bar
	if ( gpGlobals->time > m_flNextSBarUpdateTime )
	{
		UpdateStatusBar();
		m_flNextSBarUpdateTime = gpGlobals->time + 0.2;
	}
	if ( !(m_flDisplayHistory & 0x8000) && m_pActiveItem != NULL && m_pActiveItem->IsWeapon() )
	{
		if ( !(CBasePlayerItem::ItemInfoArray[m_pActiveItem->m_iId].iFlags & ITEM_FLAG_EXHAUSTIBLE) )
		{
			CBasePlayerWeapon* weapon = (CBasePlayerWeapon*)m_pActiveItem;
			if ( weapon->m_iPrimaryAmmoType != -1 && m_rgAmmo[weapon->m_iPrimaryAmmoType] == 0 && weapon->m_iClip == 0 )
			{
				m_flDisplayHistory |= 0x8000;
				if ( IsAlive() && m_bShowHints )
					m_hintMessageQueue.AddMessage( "#Hint_out_of_ammo", 6.0f, true, NULL );
			}
		}
	}
	float time = gpGlobals->time;
	if ( time > m_tmHandleSignals )
	{
		m_tmHandleSignals = gpGlobals->time + 0.5f;
		HandleSignals();
		time = gpGlobals->time;
	}
	if ( pev->deadflag == DEAD_NO && time > m_tmNextRadarUpdate )
	{
		m_tmNextRadarUpdate = time + 1.0f;
		if ( (pev->origin - m_vLastOrigin).Length() >= 64.0f )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CBaseEntity* entity = UTIL_PlayerByIndex( i );
				if ( entity != NULL && entindex() != i )
				{
					CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
					if ( (unsigned int)entity->pev->flags != FL_DORMANT && entity->pev->deadflag == DEAD_NO && player->m_iTeam == m_iTeam )
					{
						MESSAGE_BEGIN( MSG_ONE, gmsgRadar, NULL, entity->pev );
						WRITE_BYTE( entindex() );
						WRITE_COORD( pev->origin.x );
						WRITE_COORD( pev->origin.y );
						WRITE_COORD( pev->origin.z );
						MESSAGE_END();
					}
				}
			}
		}
		m_vLastOrigin = pev->origin;
		time = gpGlobals->time;
	}
	if ( (unsigned int)(m_iTeam - TERRORIST) <= CT - TERRORIST )
	{
		bool accountChanged = m_iLastAccount != m_iAccount;
		bool healthChanged = m_iLastClientHealth != m_iClientHealth;
		float nextUpdate = m_tmNextAccountHealthUpdate;
		if ( time > nextUpdate || healthChanged || accountChanged )
		{
			if ( time > nextUpdate )
				m_tmNextAccountHealthUpdate = time + 5.0f;
			for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			{
				CBaseEntity* entity = UTIL_PlayerByIndex( i );
				if ( entity == NULL )
					continue;
				CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
				int healthMode = (int)scoreboard_showhealth.value;
				int moneyMode = (int)scoreboard_showmoney.value;
				bool sendHealth = healthMode == m_iTeam || healthMode == 4 ||
					(healthMode == 3 && player->m_iTeam == m_iTeam) ||
					(healthMode == 5 && (player->m_iTeam == SPECTATOR || player->m_iTeam == m_iTeam));
				bool sendAccount = moneyMode == m_iTeam || moneyMode == 4 ||
					(moneyMode == 3 && player->m_iTeam == m_iTeam) ||
					(moneyMode == 5 && (player->m_iTeam == SPECTATOR || player->m_iTeam == m_iTeam));
				if ( !(time <= nextUpdate) )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgHealthInfo, NULL, entity->pev );
					WRITE_BYTE( entindex() );
					WRITE_LONG( sendHealth ? m_iClientHealth : -1 );
					MESSAGE_END();
					MESSAGE_BEGIN( MSG_ONE, gmsgAccount, NULL, entity->pev );
					WRITE_BYTE( entindex() );
					WRITE_LONG( sendAccount ? m_iAccount : -1 );
					MESSAGE_END();
				}
				else
				{
					if ( sendHealth && healthChanged )
					{
						MESSAGE_BEGIN( MSG_ONE, gmsgHealthInfo, NULL, entity->pev );
						WRITE_BYTE( entindex() );
						WRITE_LONG( m_iClientHealth );
						MESSAGE_END();
					}
					if ( sendAccount && accountChanged )
					{
						MESSAGE_BEGIN( MSG_ONE, gmsgAccount, NULL, entity->pev );
						WRITE_BYTE( entindex() );
						WRITE_LONG( m_iAccount );
						MESSAGE_END();
					}
				}
			}
			m_iLastAccount = m_iAccount;
			m_iLastClientHealth = m_iClientHealth;
		}
	}
}

void CBasePlayer::HandleSignals()
{
	if ( g_pGameRules->IsMultiplayer() )
	{
		CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
		if ( !rules->m_bMapHasBuyZone )
		{
			const char* spawnClass = NULL;
			if ( m_iTeam == TERRORIST )
				spawnClass = "info_player_deathmatch";
			else if ( m_iTeam == CT )
				spawnClass = "info_player_start";
			if ( spawnClass != NULL )
			{
				CBaseEntity* spawn = NULL;
				while ( (spawn = UTIL_FindEntityByClassname( spawn, spawnClass )) != NULL )
				{
					if ( (spawn->pev->origin - pev->origin).Length() < 200.0f )
					{
						m_signals.Signal( SIGNAL_BUY );
						break;
					}
				}
			}
		}
		if ( !rules->m_bMapHasBombZone )
		{
			CBaseEntity* target = NULL;
			while ( (target = UTIL_FindEntityByClassname( target, "info_bomb_target" )) != NULL )
			{
				if ( (target->pev->origin - pev->origin).Length() <= 256.0f )
				{
					m_signals.Signal( SIGNAL_BOMB );
					break;
				}
			}
		}
		if ( !rules->m_bMapHasRescueZone )
		{
			CBaseEntity* rescue = NULL;
			while ( (rescue = UTIL_FindEntityByClassname( rescue, "info_hostage_rescue" )) != NULL )
			{
				if ( (rescue->pev->origin - pev->origin).Length() <= 256.0f )
				{
					m_signals.Signal( SIGNAL_RESCUE );
					break;
				}
			}
		}
	}

	int changed = m_signals.Update();
	int state = m_signals.GetState();
	if ( changed & SIGNAL_BUY )
	{
		if ( state & SIGNAL_BUY )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( "buyzone" );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 160 );
			WRITE_BYTE( 0 );
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_STRING( "buyzone" );
			MESSAGE_END();
			if ( (unsigned int)m_iMenu >= Menu_Buy && (unsigned int)m_iMenu <= Menu_BuyItem )
				CLIENT_COMMAND( edict(), "slot10\n" );
			else if ( m_iMenu == Menu_ClientBuy )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgBuyClose, NULL, pev );
				MESSAGE_END();
			}
		}
	}
	if ( changed & SIGNAL_BOMB )
	{
		if ( state & SIGNAL_BOMB )
		{
			if ( m_bHasC4 && !(m_flDisplayHistory & 0x10000) )
			{
				m_flDisplayHistory |= 0x10000;
				if ( IsAlive() && m_bShowHints )
					m_hintMessageQueue.AddMessage( "#Hint_you_are_in_targetzone", 6.0f, true, NULL );
			}
			SetBombIcon( true );
		}
		else
		{
			SetBombIcon( false );
		}
	}
	if ( changed & SIGNAL_RESCUE )
	{
		if ( state & SIGNAL_RESCUE )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( "rescue" );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 160 );
			WRITE_BYTE( 0 );
			MESSAGE_END();
			if ( m_iTeam == CT && !(m_flDisplayHistory & 0x20000) )
			{
				m_flDisplayHistory |= 0x20000;
				if ( IsAlive() && m_bShowHints )
					m_hintMessageQueue.AddMessage( "#Hint_hostage_rescue_zone", 6.0f, true, NULL );
			}
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_STRING( "rescue" );
			MESSAGE_END();
			if ( (unsigned int)m_iMenu >= Menu_Buy && (unsigned int)m_iMenu <= Menu_BuyItem )
				CLIENT_COMMAND( edict(), "slot10\n" );
			else if ( m_iMenu == Menu_ClientBuy )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgBuyClose, NULL, pev );
				MESSAGE_END();
			}
		}
	}
	if ( changed & SIGNAL_ESCAPE )
	{
		if ( state & SIGNAL_ESCAPE )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( "escape" );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 160 );
			WRITE_BYTE( 0 );
			MESSAGE_END();
			if ( m_iTeam == CT && !(m_flDisplayHistory & 0x40000) )
			{
				m_flDisplayHistory |= 0x40000;
				if ( IsAlive() && m_bShowHints )
					m_hintMessageQueue.AddMessage( "#Hint_terrorist_escape_zone", 6.0f, true, NULL );
			}
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_STRING( "escape" );
			MESSAGE_END();
			if ( (unsigned int)m_iMenu >= Menu_Buy && (unsigned int)m_iMenu <= Menu_BuyItem )
				CLIENT_COMMAND( edict(), "slot10\n" );
			else if ( m_iMenu == Menu_ClientBuy )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgBuyClose, NULL, pev );
				MESSAGE_END();
			}
		}
	}
	if ( changed & SIGNAL_VIPSAFETY )
	{
		if ( state & SIGNAL_VIPSAFETY )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( "vipsafety" );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 160 );
			WRITE_BYTE( 0 );
			MESSAGE_END();
			if ( m_iTeam == CT && !(m_flDisplayHistory & 0x80000) )
			{
				m_flDisplayHistory |= 0x80000;
				if ( m_bShowHints )
					m_hintMessageQueue.AddMessage( "#Hint_ct_vip_zone", 6.0f, true, NULL );
			}
			else if ( m_iTeam == TERRORIST && !(m_flDisplayHistory & 0x80000) )
			{
				m_flDisplayHistory |= 0x80000;
				if ( m_bShowHints )
					m_hintMessageQueue.AddMessage( "#Hint_terrorist_vip_zone", 6.0f, true, NULL );
			}
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_STRING( "vipsafety" );
			MESSAGE_END();
			if ( (unsigned int)m_iMenu >= Menu_Buy && (unsigned int)m_iMenu <= Menu_BuyItem )
				CLIENT_COMMAND( edict(), "slot10\n" );
			else if ( m_iMenu == Menu_ClientBuy )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgBuyClose, NULL, pev );
				MESSAGE_END();
			}
		}
	}
}

void CBasePlayer::UpdateStatusBar()
{
	int newState[SBAR_END] = { 0 };
	char text[SBAR_STRING_SIZE];
	strcpy( text, m_SbarString0 );
	UTIL_MakeVectors( pev->v_angle + pev->punchangle );
	Vector source = EyePosition();
	float range = pev->flags & FL_SPECTATOR ? 8192.0f : 2048.0f;
	Vector target = source + gpGlobals->v_forward * range;
	TraceResult trace;
	UTIL_TraceLine( source, target, dont_ignore_monsters, edict(), &trace );
	if ( trace.flFraction != 1.0f )
	{
		if ( !FNullEnt( trace.pHit ) )
		{
			CBaseEntity* entity = CBaseEntity::Instance( trace.pHit );
			int classification = entity->Classify();
			if ( TheBots->IsLineBlockedBySmoke( &pev->origin, &entity->pev->origin ) ||
				m_blindUntilTime > gpGlobals->time || classification != CLASS_PLAYER )
			{
				if ( entity->Classify() == CLASS_HUMAN_PASSIVE )
				{
					strcpy( text, playerid.value != 2.0f || pev->iuser1 != 0 ? "1 %c1  %h: %i3%%" : " " );
					newState[SBAR_ID_TARGETTYPE] = 3;
					newState[SBAR_ID_TARGETHEALTH] = (int)(entity->pev->health / entity->pev->max_health * 100.0f);
					if ( !(m_flDisplayHistory & 4) && trace.flFraction > 0.1 )
					{
						m_flDisplayHistory |= 4;
						if ( m_iTeam == TERRORIST && m_bShowHints )
							m_hintMessageQueue.AddMessage( "#Hint_prevent_hostage_rescue", 6.0f, true, NULL );
						else if ( m_iTeam == CT && m_bShowHints )
							m_hintMessageQueue.AddMessage( "#Hint_rescue_the_hostages", 6.0f, true, NULL );
					}
					else if ( m_iTeam == CT && !(m_flDisplayHistory & 8) && trace.flFraction <= 0.1 )
					{
						m_flDisplayHistory |= 12;
						HintMessage( "#Hint_press_use_so_hostage_will_follow", false, false );
					}
				}
			}
			else
			{
				CBasePlayer* player = (CBasePlayer*)entity;
				newState[SBAR_ID_TARGETNAME] = player->entindex();
				bool showHealth = true;
				if ( player->m_iTeam == m_iTeam )
				{
					newState[SBAR_ID_TARGETTYPE] = 1;
					if ( playerid.value == 2.0f && pev->iuser1 == 0 )
						strcpy( text, " " );
					else
						strcpy( text, "1 %c1: %p2\n2  %h: %i3%%" );
				}
				else
				{
					newState[SBAR_ID_TARGETTYPE] = 2;
					if ( pev->iuser1 == 0 )
					{
						strcpy( text, playerid.value == 1.0f || playerid.value == 2.0f ? " " : "1 %c1: %p2" );
						showHealth = false;
						if ( !(m_flDisplayHistory & 0x100) )
						{
							m_flDisplayHistory |= 0x100;
							if ( IsAlive() && m_bShowHints )
								m_hintMessageQueue.AddMessage( "#Hint_spotted_an_enemy", 6.0f, true, NULL );
						}
					}
					else
					{
						strcpy( text, "1 %c1: %p2\n2  %h: %i3%%" );
					}
				}
				if ( showHealth )
				{
					newState[SBAR_ID_TARGETHEALTH] = (int)(entity->pev->health / entity->pev->max_health * 100.0f);
					if ( !(m_flDisplayHistory & 0x80) && !(pev->flags & FL_SPECTATOR) )
					{
						m_flDisplayHistory |= 0x80;
						if ( IsAlive() && m_bShowHints )
							m_hintMessageQueue.AddMessage( "#Hint_spotted_a_friend", 6.0f, true, NULL );
					}
				}
			}
			m_flStatusBarDisappearDelay = gpGlobals->time + 2.0f;
		}
	}
	else if ( m_flStatusBarDisappearDelay > gpGlobals->time )
	{
		newState[SBAR_ID_TARGETTYPE] = m_izSBarState[SBAR_ID_TARGETTYPE];
		newState[SBAR_ID_TARGETNAME] = m_izSBarState[SBAR_ID_TARGETNAME];
		newState[SBAR_ID_TARGETHEALTH] = m_izSBarState[SBAR_ID_TARGETHEALTH];
	}

	bool forceResend = false;
	if ( strcmp( text, m_SbarString0 ) != 0 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusText, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_STRING( text );
		MESSAGE_END();
		strcpy( m_SbarString0, text );
		forceResend = true;
	}

	// Check values and send if they don't match
	for ( int i = 1; i < SBAR_END; ++i )
	{
		if ( newState[i] != m_izSBarState[i] || forceResend )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusValue, NULL, pev );
			WRITE_BYTE( i );
			WRITE_SHORT( newState[i] );
			MESSAGE_END();
			m_izSBarState[i] = newState[i];
		}
	}
}

void CBasePlayer::Spawn()
{
	m_iGaitsequence = 0;
	m_flGaitframe = 0;
	m_flGaityaw = 0;
	m_prevgaitorigin = g_vecZero;
	m_flGaitMovement = 0;
	m_progressStart = 0;
	m_progressEnd = 0;
	MAKE_STRING_CLASS( "player", pev );
	pev->health = 100;
	if ( !m_bNotKilled )
	{
		pev->armorvalue = 0;
		m_iKevlar = 0;
	}
	pev->maxspeed = 1000;
	pev->takedamage = DAMAGE_AIM;
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_WALK;
	pev->max_health = pev->health;
	pev->flags &= FL_PROXY;	// keep proxy flag sey by engine
	pev->flags |= FL_CLIENT;
	pev->air_finished = gpGlobals->time + 12;
	pev->dmg = 2;				// initial water damage
	pev->effects = 0;
	pev->deadflag = DEAD_NO;
	pev->dmg_take = 0;
	pev->dmg_save = 0;
	m_bitsHUDDamage = -1;
	m_iClientFOV = 0;
	m_pentCurBombTarget = NULL;
	m_bitsDamageType = 0;
	m_afPhysicsFlags = 0;
	m_fLongJump = FALSE; // no longjump module. 
	pev->gamestate = !m_bOwnsShield;
	ResetStamina();
	pev->friction = 1;
	pev->gravity = 1;
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );
	m_hintMessageQueue.Reset();
	m_iLastZoom = 90;
	m_iRadioMessages = 60;
	m_iChaseTarget = 1;
	m_flDisplayHistory &= 0xFFFFEBBD;
	m_flVelocityModifier = 1;
	m_flLastTalk = 0;
	m_flIdleCheckTime = 0;
	m_flRadioTime = 0;
	m_bHasC4 = false;
	m_bKilledByBomb = false;
	m_bKilledByGrenade = false;
	m_tmHandleSignals = 0;
	m_fCamSwitch = 0;
	m_bEscaped = false;
	m_tmNextRadarUpdate = gpGlobals->time;
	m_tmNextAccountHealthUpdate = gpGlobals->time;
	m_iCurrentKickVote = 0;
	m_vLastOrigin = g_vecZero;
	m_flNextVoteTime = 0;
	m_bJustKilledTeammate = false;
	SET_VIEW( edict(), edict() );
	m_hObserverTarget = NULL;
	pev->iuser3 = 0;
	pev->iuser2 = 0;
	pev->iuser1 = 0;
	m_idrowndmg = 0;
	m_idrownrestored = 0;
	m_bitsDamageType &= ~(DMG_DROWN | DMG_DROWNRECOVER);
	m_flLastFired = -15;
	m_bHeadshotKilled = false;
	m_bReceivesNoMoneyNextRound = false;
	m_bShieldDrawn = false;
	m_blindUntilTime = 0;
	m_blindStartTime = 0;
	m_blindHoldTime = 0;
	m_blindFadeTime = 0;
	m_blindAlpha = 0;
	m_canSwitchObserverModes = true;
	m_lastLocation[0] = 0;
	m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
	if ( m_iObserverC4State )
	{
		m_iObserverC4State = 0;
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_STRING( "c4" );
		MESSAGE_END();
	}
	if ( m_bObserverHasDefuser )
	{
		m_bObserverHasDefuser = false;
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_STRING( "defuser" );
		MESSAGE_END();
	}
	MESSAGE_BEGIN( MSG_ONE, SVC_ROOMTYPE, NULL, pev );
	WRITE_SHORT( (int)CVAR_GET_FLOAT( "room_type" ) );
	MESSAGE_END();
	m_bCanShoot = !g_pGameRules->IsFreezePeriod();
	memset( m_vRecentPath, 0, sizeof( m_vRecentPath ) );
	++m_iNumSpawns;
	m_SbarString0[0] = 0;
	if ( m_pActiveItem && !pev->viewmodel )
	{
		switch ( m_pActiveItem->m_iId )
		{
		case WEAPON_SCOUT: pev->viewmodel = MAKE_STRING( "models/v_scout.mdl" ); break;
		case WEAPON_SG550: pev->viewmodel = MAKE_STRING( "models/v_sg550.mdl" ); break;
		case WEAPON_AWP: pev->viewmodel = MAKE_STRING( "models/v_awp.mdl" ); break;
		case WEAPON_G3SG1: pev->viewmodel = MAKE_STRING( "models/v_g3sg1.mdl" ); break;
		}
	}
	m_iFOV = 90;
	m_flNextDecalTime = 0; // let this player decal as soon as he spawns.
	m_bloodColor = BLOOD_COLOR_RED;
												// are recieved by all clients

	m_flTimeStepSound = 0;
	m_iStepLeft = 0;
	m_flFieldOfView = 0.5f;
	m_flNextAttack = 0;
	m_flgeigerDelay = gpGlobals->time + 2;
	StartSneaking();
	m_iFlashBattery = 99;
	m_flFlashLightTime = 1; // force first message
	pev->body = m_bHasDefuser ? 1 : 0;
	if ( m_bMissionBriefing )
	{
		ResetMenu();
		m_bMissionBriefing = false;
	}
	m_flFallVelocity = 0;
	if ( !g_skipCareerInitialSpawn )
		g_pGameRules->GetPlayerSpawnSpot( this );
	if ( !pev->modelindex )
	{
		SET_MODEL( edict(), "models/player.mdl" );
		m_modelIndexPlayer = pev->modelindex;
	}
	pev->sequence = LookupActivity( ACT_IDLE );
	if ( pev->flags & FL_DUCKING )
		UTIL_SetSize( pev, Vector( -16, -16, -18 ), Vector( 16, 16, 32 ) );
	else
		UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );
	pev->view_ofs = Vector( 0, 0, 17 );
	Precache();
	m_HackedGunPos = Vector( 0, 32, 0 );
	if ( m_iPlayerSound == SOUNDLIST_EMPTY )
		ALERT( at_console, "Couldn't alloc player sound slot!\n" );
	m_fNoPlayerSound = FALSE;
	m_pLastItem = NULL;
	m_fWeapon = FALSE;
	m_iHideHUD &= ~(HIDEHUD_WEAPONS | HIDEHUD_HEALTH | HIDEHUD_TIMER | HIDEHUD_MONEY);
	m_pClientActiveItem = NULL;
	m_iClientBattery = -1;
	m_fInitHUD = TRUE;
	if ( !m_bNotKilled )
	{
		m_iClientHideHUD = -1;  // force this to be recalculated
		memset( m_rgAmmo, 0, sizeof( m_rgAmmo ) );
		m_bHasPrimary = false;
		m_bHasNightVision = false;
		SendItemStatus( this );
	}
	else
		memset( m_rgAmmoLast, -1, sizeof( m_rgAmmoLast ) );
	MESSAGE_BEGIN( MSG_ONE, gmsgNVGToggle, NULL, pev );
	WRITE_BYTE( 0 );
	MESSAGE_END();
	m_bNightVisionOn = false;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer* player = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( player && player->pev->flags != FL_DORMANT && !FNullEnt( pev ) &&
			player->pev->iuser1 == OBS_IN_EYE && player->pev->iuser2 == entindex() )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgNVGToggle, NULL, player->edict() );
			WRITE_BYTE( 0 );
			MESSAGE_END();
			player->m_bNightVisionOn = false;
		}
	}
	m_lasty = 0;
	m_lastx = 0;
	g_pGameRules->PlayerSpawn( this );
	m_bNotKilled = true;
	m_bIsDefusing = false;
	SetProgressBarTime( 0 );
	ResetMaxSpeed();
	UTIL_SetOrigin( pev, pev->origin );
	if ( m_bIsVIP )
	{
		m_iKevlar = 2;
		pev->armorvalue = 200;
		m_hintMessageQueue.AddMessage( "#Hint_you_are_the_vip", 6, true, NULL );
	}
	SetScoreboardAttributes( NULL );
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
	WRITE_BYTE( entindex() );
	switch ( m_iTeam )
	{
	case CT: WRITE_STRING( "CT" ); break;
	case TERRORIST: WRITE_STRING( "TERRORIST" ); break;
	case SPECTATOR: WRITE_STRING( "SPECTATOR" ); break;
	default: WRITE_STRING( "UNASSIGNED" ); break;
	}
	MESSAGE_END();
	UpdateLocation( true );
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
	WRITE_BYTE( entindex() );
	WRITE_SHORT( (int)pev->frags );
	WRITE_SHORT( m_iDeaths );
	WRITE_SHORT( 0 );
	WRITE_SHORT( m_iTeam );
	MESSAGE_END();
	if ( m_bHasChangedName )
	{
		char* info = g_engfuncs.pfnGetInfoKeyBuffer( edict() );
		if ( strcmp( m_szNewName, g_engfuncs.pfnInfoKeyValue( info, "name" ) ) )
			g_engfuncs.pfnSetClientKeyValue( entindex(), info, "name", m_szNewName );
		m_bHasChangedName = false;
		m_szNewName[0] = 0;
	}
	UTIL_ScreenFade( this, g_vecZero, 0.001f, 0, 0, 0 );
	SyncRoundTimer();
	if ( TheBots )
		TheBots->OnEvent( EVENT_PLAYER_SPAWNED, this, NULL );
	sv_aim = CVAR_GET_POINTER( "sv_aim" );
	m_allowAutoFollowTime = 0;
	for ( int i = 0; i < ARRAYSIZE( m_flLastCommandTime ); ++i )
		m_flLastCommandTime[i] = -1;
}

void CBasePlayer::PostThink()
{
	if ( !g_fGameOver && IsAlive() )
	{
		if ( m_pTank )
		{
			if ( !m_pTank->OnControls( pev ) || pev->weaponmodel )
			{
				m_pTank->Use( this, this, USE_OFF, 0 );
				m_pTank = NULL;
			}
			else
				m_pTank->Use( this, this, USE_SET, 2 );
		}
		if ( !m_pTank )
		{
			if ( m_pActiveItem && m_bOwnsShield && ((CBasePlayerWeapon*)m_pActiveItem)->m_fInReload &&
				(pev->button & IN_ATTACK2) )
				m_flNextAttack = 0;
			if ( !(m_flNextAttack > 0) )
			{
				ImpulseCommands();
				if ( m_pActiveItem )
					m_pActiveItem->ItemPostFrame();
			}
		}
		if ( pev->flags & FL_ONGROUND )
		{
			if ( pev->health > 0 && m_flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD )
			{
				if ( pev->watertype != CONTENTS_WATER && m_flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED )
				{
					float damage = g_pGameRules->FlPlayerFallDamage( this );
					if ( damage > pev->health )
						EMIT_SOUND( edict(), CHAN_ITEM, "common/bodysplat.wav", VOL_NORM, ATTN_NORM );
					if ( damage > 0 )
					{
						m_LastHitGroup = HITGROUP_GENERIC;
						entvars_t* attacker = VARS( INDEXENT( 0 ) );
						entvars_t* inflictor = VARS( INDEXENT( 0 ) );
						TakeDamage( inflictor, attacker, damage, DMG_FALL );
						pev->punchangle.x = 0;
						TheBots->OnEvent( EVENT_PLAYER_LANDED_FROM_HEIGHT, this, NULL );
					}
				}
				if ( IsAlive() )
					SetAnimation( PLAYER_WALK );
			}
			if ( pev->flags & FL_ONGROUND )
			{
				if ( m_flFallVelocity > 64 && !g_pGameRules->IsMultiplayer() )
					CSoundEnt::InsertSound( bits_SOUND_PLAYER, pev->origin, (int)m_flFallVelocity, 0.2f );
				m_flFallVelocity = 0;
			}
		}
		if ( IsAlive() )
		{
			if ( pev->velocity.x == 0 && pev->velocity.y == 0 )
			{
				if ( pev->gaitsequence != 5 )
					SetAnimation( PLAYER_IDLE );
			}
			else if ( (pev->flags & FL_ONGROUND) || pev->waterlevel > 1 )
				SetAnimation( PLAYER_WALK );
		}
		StudioFrameAdvance();
		if ( pev->health > 0 )
			pev->modelindex = m_modelIndexPlayer;
		UpdatePlayerSound();
	}
	for ( int i = 0; i < ARRAYSIZE( m_rgpPlayerItems ); ++i )
	{
		for ( CBasePlayerItem* item = m_rgpPlayerItems[i]; item; item = item->m_pNext )
		{
			CBasePlayerWeapon* weapon = (CBasePlayerWeapon*)item->GetWeaponPtr();
			if ( weapon && weapon->UseDecrement() )
			{
				double nextAttack = (double)weapon->m_flNextPrimaryAttack - gpGlobals->frametime;
				weapon->m_flNextPrimaryAttack = (float)(!(nextAttack < -1) ? nextAttack : -1);
				nextAttack = (double)weapon->m_flNextSecondaryAttack - gpGlobals->frametime;
				weapon->m_flNextSecondaryAttack = (float)(!(nextAttack < -0.001f) ? nextAttack : -0.001f);
				if ( weapon->m_flTimeWeaponIdle != 1000 )
				{
					nextAttack = (double)weapon->m_flTimeWeaponIdle - gpGlobals->frametime;
					weapon->m_flTimeWeaponIdle = (float)(!(nextAttack < -0.001f) ? nextAttack : -0.001f);
				}
			}
		}
	}
	double nextAttack = (double)m_flNextAttack - gpGlobals->frametime;
	m_flNextAttack = (float)(!(nextAttack < -0.001) ? nextAttack : -0.001f);
	m_afButtonLast = pev->button;
	m_iGaitsequence = pev->gaitsequence;
	StudioProcessGait();
}

void CBasePlayer::PreThink()
{
	int changed = m_afButtonLast ^ pev->button;
	if ( changed )
		m_fLastMovement = gpGlobals->time;
	m_afButtonPressed = changed & pev->button;
	m_afButtonReleased = changed & ~pev->button;
	m_hintMessageQueue.Update( *this );
	g_pGameRules->PlayerThink( this );
	if ( g_fGameOver )
		return;         // intermission or finale
	if ( m_iJoiningState != JOINED )
		JoiningThink();
	if ( m_bMissionBriefing && (m_afButtonPressed & (IN_ATTACK | IN_ATTACK2)) )
	{
		m_afButtonPressed &= ~(IN_ATTACK | IN_ATTACK2);
		ResetMenu();
		m_bMissionBriefing = false;
	}
	UTIL_MakeVectors( pev->v_angle );
	if ( m_flNextAttack <= 0 && m_pActiveItem )
		m_pActiveItem->ItemPreFrame();
	WaterMove();
	if ( pev->flags & FL_ONGROUND )
	{
		if ( m_flVelocityModifier < 1 )
		{
			m_flVelocityModifier = (float)(m_flVelocityModifier + 0.01);
			pev->velocity = pev->velocity * m_flVelocityModifier;
		}
		if ( m_flVelocityModifier > 1 )
			m_flVelocityModifier = 1;
	}
	if ( gpGlobals->time >= m_flIdleCheckTime || m_flIdleCheckTime == 0 )
	{
		m_flIdleCheckTime = gpGlobals->time + 5;
		if ( gpGlobals->time - m_fLastMovement > ((CHalfLifeMultiplay*)g_pGameRules)->m_fMaxIdlePeriod &&
			!IsBot() && CVAR_GET_FLOAT( "mp_autokick" ) != 0 )
		{
			char* team = GetTeam( m_iTeam );
			const char* authId = GETPLAYERAUTHID( edict() );
			int userId = GETPLAYERUSERID( edict() );
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" triggered \"Game_idle_kick\" (auto)\n",
				STRING( pev->netname ), userId, authId, team );
			UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "#Game_idle_kick", STRING( pev->netname ) );
			SERVER_COMMAND( UTIL_VarArgs( "kick \"%s\"\n", STRING( pev->netname ) ) );
			m_fLastMovement = gpGlobals->time;
		}
	}
	if ( g_pGameRules && g_pGameRules->FAllowFlashlight() )
		m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	else
		m_iHideHUD |= HIDEHUD_FLASHLIGHT;


	// JOHN: checks if new client data (for HUD and view control) needs to be sent to the client
	UpdateClientData();
	CheckTimeBasedDamage();
	CheckSuitUpdate();
	if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
		pev->flags |= FL_ONTRAIN;
	else
		pev->flags &= ~FL_ONTRAIN;
	if ( pev->iuser1 && (m_afPhysicsFlags & PFLAG_OBSERVER) )
	{
		Observer_HandleButtons();
		Observer_CheckTarget();
		Observer_CheckProperties();
		return;
	}
	if ( pev->deadflag > DEAD_NO && pev->deadflag != DEAD_RESPAWNABLE )
	{
		PlayerDeathThink();
		return;
	}
	CBaseEntity* ground = CBaseEntity::Instance( pev->groundentity );
	pev->iuser4 = ground && ground->Classify() == CLASS_VEHICLE;
	if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
	{
		CBaseEntity* train = CBaseEntity::Instance( pev->groundentity );
		bool valid;
		if ( train )
			valid = (pev->flags & FL_ONGROUND) && !(train->pev->spawnflags & SF_TRACKTRAIN_NOCONTROL);
		else
		{
			TraceResult trace;
			UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -38 ), ignore_monsters, edict(), &trace );
			train = trace.flFraction != 1 && trace.pHit ? CBaseEntity::Instance( trace.pHit ) : NULL;
			valid = train && (train->ObjectCaps() & FCAP_DIRECTIONAL_USE) && train->OnControls( pev );
		}
		if ( !valid )
		{
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = TRAIN_NEW;
			StopDrivingVehicle();
			return;
		}
		pev->velocity = g_vecZero;
		bool updateTrain = false;
		if ( train->Classify() == CLASS_VEHICLE )
		{
			if ( pev->button & IN_FORWARD )
			{
				train->Use( this, this, USE_SET, 1 );
				updateTrain = true;
			}
			if ( pev->button & IN_BACK )
			{
				train->Use( this, this, USE_SET, -1 );
				updateTrain = true;
			}
			if ( pev->button & IN_MOVELEFT )
			{
				train->Use( this, this, USE_SET, 20 );
				updateTrain = true;
			}
			if ( pev->button & IN_MOVERIGHT )
			{
				train->Use( this, this, USE_SET, 30 );
				updateTrain = true;
			}
		}
		else if ( m_afButtonPressed & IN_FORWARD )
		{
			train->Use( this, this, USE_SET, 1 );
			updateTrain = true;
		}
		else if ( m_afButtonPressed & IN_BACK )
		{
			train->Use( this, this, USE_SET, -1 );
			updateTrain = true;
		}
		if ( updateTrain )
			m_iTrain = TrainSpeed( (int)train->pev->speed, train->pev->impulse ) | TRAIN_NEW;
	}
	else if ( m_iTrain & TRAIN_ACTIVE )
		m_iTrain = TRAIN_NEW;
	if ( pev->button & IN_JUMP )
		Jump();
	if ( (pev->button & IN_DUCK) || (pev->flags & FL_DUCKING) || (m_afPhysicsFlags & PFLAG_DUCKING) )
		Duck();
	if ( !(pev->flags & FL_ONGROUND) )
		m_flFallVelocity = -pev->velocity.z;

	// StudioFrameAdvance( );//!!!HACKHACK!!! Can't be hit by traceline when not animating?

	// Clear out ladder pointer
	m_hEnemy = NULL;
	if ( m_afPhysicsFlags & PFLAG_ONBARNACLE )
		pev->velocity = g_vecZero;
	if ( !(m_flDisplayHistory & 2) && CanPlayerBuy( false ) )
	{
		if ( IsAlive() && m_bShowHints )
			m_hintMessageQueue.AddMessage( "#Hint_press_buy_to_purchase", 6, true, NULL );
		m_flDisplayHistory |= 2;
	}
	UpdateLocation( false );
}

void CBasePlayer::JoiningThink()
{
	switch ( m_iJoiningState )
	{
	case SHOWLTEXT:
		ShowMenu( this, 0, 0, FALSE, "" );
		m_iJoiningState = SHOWTEAMSELECT;
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, edict() );
			WRITE_BYTE( 0 );
			WRITE_STRING( "defuser" );
		MESSAGE_END();
		m_bHasDefuser = false;
		m_bMissionBriefing = false;
		m_fLastMovement = gpGlobals->time;
		SendItemStatus( this );
		break;
	case JOINED:
		return;
	case READINGLTEXT:
		if ( m_afButtonPressed & (IN_ATTACK | IN_JUMP | IN_ATTACK2) )
		{
			m_afButtonPressed &= ~(IN_ATTACK | IN_JUMP | IN_ATTACK2);
			ShowMenu( this, 0, 0, FALSE, "" );
			m_iJoiningState = SHOWTEAMSELECT;
		}
		break;
	case GETINTOGAME:
		{
			m_iTeamKills = 0;
			m_iFOV = 90;
			m_bNotKilled = false;
			m_iIgnoreGlobalChat = 0;
			memset( &m_rebuyStruct, 0, sizeof( m_rebuyStruct ) );
			m_bIsInRebuy = false;
			m_bJustConnected = false;
			m_fLastMovement = gpGlobals->time;
			ResetMaxSpeed();
			CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
			m_iJoiningState = JOINED;
			if ( rules->m_bMapHasEscapeZone && m_iTeam == CT )
			{
				m_iAccount = 0;
				CheckStartMoney();
				AddAccount( (int)startmoney.value, true );
			}
			if ( g_pGameRules->FPlayerCanRespawn( this ) )
			{
				Spawn();
				rules->CheckWinConditions();
				if ( rules->m_fTeamCount == 0 && rules->m_bMapHasBombTarget &&
					!rules->IsThereABomber() && !rules->IsThereABomb() )
					rules->GiveC4();
				if ( m_iTeam == TERRORIST )
					++rules->m_iNumEscapers;
			}
			else
			{
				pev->deadflag = DEAD_RESPAWNABLE;
				MAKE_STRING_CLASS( "player", pev );
				pev->flags &= FL_PROXY | FL_FAKECLIENT;
				pev->flags |= FL_SPECTATOR | FL_CLIENT;
				edict_t* spawn = rules->GetPlayerSpawnSpot( this );
				Vector origin = pev->origin;
				Vector angles = spawn->v.angles;
				StartObserver( origin, angles );
				rules->CheckWinConditions();
				MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
					WRITE_BYTE( entindex() );
					WRITE_STRING( m_iTeam == CT ? "CT" : (m_iTeam == TERRORIST ? "TERRORIST" :
						(m_iTeam == SPECTATOR ? "SPECTATOR" : "UNASSIGNED")) );
				MESSAGE_END();
				MESSAGE_BEGIN( MSG_ALL, gmsgLocation );
					WRITE_BYTE( entindex() );
					WRITE_STRING( "" );
				MESSAGE_END();
				MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
					WRITE_BYTE( entindex() );
					WRITE_SHORT( (int)pev->frags );
					WRITE_SHORT( m_iDeaths );
					WRITE_SHORT( 0 );
					WRITE_SHORT( m_iTeam );
				MESSAGE_END();
				const int spectatorDuckHint = 1 << 22;
				if ( !(m_flDisplayHistory & spectatorDuckHint) )
				{
					m_hintMessageQueue.AddMessage( "#Spec_Duck", 6, true, NULL );
					m_flDisplayHistory |= spectatorDuckHint;
				}
			}
			return;
		}
	default:
		break;
	}
	if ( m_pIntroCamera && gpGlobals->time >= m_fIntroCamTime )
	{
		m_pIntroCamera = UTIL_FindEntityByClassname( m_pIntroCamera, "trigger_camera" );
		if ( !m_pIntroCamera )
			m_pIntroCamera = UTIL_FindEntityByClassname( NULL, "trigger_camera" );
		CBaseEntity* target = UTIL_FindEntityByTargetname( NULL, STRING( m_pIntroCamera->pev->target ) );
		if ( target )
		{
			Vector direction = (target->pev->origin - m_pIntroCamera->pev->origin).Normalize();
			Vector cameraAngles = UTIL_VecToAngles( direction );
			cameraAngles.x = -cameraAngles.x;
			UTIL_SetOrigin( pev, m_pIntroCamera->pev->origin );
			pev->angles = cameraAngles;
			pev->v_angle = pev->angles;
			pev->velocity = g_vecZero;
			pev->punchangle = g_vecZero;
			pev->fixangle = 1;
			pev->view_ofs = g_vecZero;
			m_fIntroCamTime = gpGlobals->time + 6;
		}
		else
			m_pIntroCamera = NULL;
	}
}

void CBasePlayer::SwitchTeam()
{
	TeamName oldTeam = m_iTeam;
	char* modelName = NULL;
	if ( oldTeam == CT )
	{
		m_iTeam = TERRORIST;
		switch ( m_iModelName )
		{
		case MODEL_URBAN: m_iModelName = MODEL_LEET; modelName = "leet"; break;
		case MODEL_GSG9: m_iModelName = MODEL_TERROR; modelName = "terror"; break;
		case MODEL_GIGN: m_iModelName = MODEL_GUERILLA; modelName = "guerilla"; break;
		case MODEL_SAS: m_iModelName = MODEL_ARCTIC; modelName = "arctic"; break;
		case MODEL_SPETSNAZ:
			if ( UTIL_IsGame( "czero" ) )
			{
				m_iModelName = MODEL_MILITIA;
				modelName = "militia";
				break;
			}
			[[fallthrough]];
		default:
			if ( !IsBot() || !TheBotProfiles->GetCustomSkinModelname( m_iModelName ) )
			{
				m_iModelName = MODEL_TERROR;
				modelName = "terror";
			}
			break;
		}
	}
	else if ( oldTeam == TERRORIST )
	{
		m_iTeam = CT;
		switch ( m_iModelName )
		{
		case MODEL_TERROR: m_iModelName = MODEL_GSG9; modelName = "gsg9"; break;
		case MODEL_LEET: m_iModelName = MODEL_URBAN; modelName = "urban"; break;
		case MODEL_ARCTIC: m_iModelName = MODEL_SAS; modelName = "sas"; break;
		case MODEL_GUERILLA: m_iModelName = MODEL_GIGN; modelName = "gign"; break;
		case MODEL_MILITIA:
			if ( UTIL_IsGame( "czero" ) )
			{
				m_iModelName = MODEL_SPETSNAZ;
				modelName = "spetsnaz";
				break;
			}
			[[fallthrough]];
		default:
			if ( !IsBot() || !TheBotProfiles->GetCustomSkinModelname( m_iModelName ) )
			{
				m_iModelName = MODEL_URBAN;
				modelName = "urban";
			}
			break;
		}
	}
	if ( modelName )
		g_engfuncs.pfnSetClientKeyValue( entindex(), g_engfuncs.pfnGetInfoKeyBuffer( edict() ), "model", modelName );
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( entindex() );
		WRITE_STRING( m_iTeam == CT ? "CT" : (m_iTeam == TERRORIST ? "TERRORIST" :
			(m_iTeam == SPECTATOR ? "SPECTATOR" : "UNASSIGNED")) );
	MESSAGE_END();
	TheBots->OnEvent( EVENT_PLAYER_CHANGED_TEAM, this, NULL );
	UpdateLocation( true );
	if ( m_iTeam != UNASSIGNED )
		SetScoreboardAttributes( NULL );
	const char* name = STRING( pev->netname );
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, m_iTeam == TERRORIST ? "#Game_join_terrorist_auto" : "#Game_join_ct_auto",
		name[0] ? name : "<unconnected>" );
	if ( m_bHasDefuser )
	{
		m_bHasDefuser = false;
		pev->body = 0;
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, edict() );
			WRITE_BYTE( 0 );
			WRITE_STRING( "defuser" );
		MESSAGE_END();
		SendItemStatus( this );
		SetProgressBarTime( 0 );
		for ( int slot = 0; slot < MAX_ITEM_TYPES; ++slot )
		{
			m_pActiveItem = m_rgpPlayerItems[slot];
			if ( m_pActiveItem && FStrEq( STRING( m_pActiveItem->pev->classname ), "item_thighpack" ) )
			{
				m_pActiveItem->Drop();
				m_rgpPlayerItems[slot] = NULL;
			}
		}
	}
	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" joined team \"%s\" (auto)\n", STRING( pev->netname ),
		GETPLAYERUSERID( edict() ), GETPLAYERAUTHID( edict() ), GetTeam( oldTeam ), GetTeam( m_iTeam ) );
	if ( IsBot() )
	{
		const BotProfile* profile = ((CBot*)this)->GetProfile();
		if ( profile && ((m_iTeam == CT && !profile->IsValidForTeam( BOT_TEAM_CT )) ||
			(m_iTeam == TERRORIST && !profile->IsValidForTeam( BOT_TEAM_T ))) )
			SERVER_COMMAND( UTIL_VarArgs( "kick \"%s\"\n", STRING( pev->netname ) ) );
	}
}

void CBasePlayer::ThrowPrimary()
{
	ThrowWeapon( "weapon_m249" );
	ThrowWeapon( "weapon_g3sg1" );
	ThrowWeapon( "weapon_sg550" );
	ThrowWeapon( "weapon_awp" );
	ThrowWeapon( "weapon_mp5navy" );
	ThrowWeapon( "weapon_tmp" );
	ThrowWeapon( "weapon_p90" );
	ThrowWeapon( "weapon_ump45" );
	ThrowWeapon( "weapon_m4a1" );
	ThrowWeapon( "weapon_m3" );
	ThrowWeapon( "weapon_sg552" );
	ThrowWeapon( "weapon_scout" );
	ThrowWeapon( "weapon_galil" );
	ThrowWeapon( "weapon_famas" );
	if ( m_bOwnsShield && (m_pActiveItem == NULL || m_pActiveItem->CanHolster()) )
		DropShield( true );
}

void LogAttack( CBasePlayer* attacker, CBasePlayer* victim, int teamAttack, int healthHit,
	int armorHit, int newHealth, int newArmor, const char* weaponName )
{
	int detail = (int)logdetail.value;
	if ( !victim || !detail || !attacker )
		return;
	if ( teamAttack ? !(detail & 2) : !(detail & 1) )
		return;
	char* victimTeam = GetTeam( victim->m_iTeam );
	const char* victimAuth = GETPLAYERAUTHID( victim->edict() );
	int victimId = GETPLAYERUSERID( victim->edict() );
	const char* victimName = STRING( victim->pev->netname );
	char* attackerTeam = GetTeam( attacker->m_iTeam );
	const char* attackerAuth = GETPLAYERAUTHID( attacker->edict() );
	int attackerId = GETPLAYERUSERID( attacker->edict() );
	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" attacked \"%s<%i><%s><%s>\" with \"%s\" "
		"(damage \"%d\") (damage_armor \"%d\") (health \"%d\") (armor \"%d\")\n",
		STRING( attacker->pev->netname ), attackerId, attackerAuth, attackerTeam,
		victimName, victimId, victimAuth, victimTeam, weaponName, healthHit, armorHit, newHealth, newArmor );
}

void CBasePlayer::CheatImpulseCommands( int impulse )
{
	if ( CVAR_GET_FLOAT( "sv_cheats" ) == 0 )
		return;

	CBaseEntity* entity;
	TraceResult trace;
	switch ( impulse )
	{
	case 76:
		if ( giPrecacheGrunt )
		{
			UTIL_MakeVectors( Vector( 0, pev->v_angle.y, 0 ) );
			Create( "monster_human_grunt", pev->origin + gpGlobals->v_forward * 128, pev->angles );
		}
		else
		{
			giPrecacheGrunt = 1;
			ALERT( at_console, "You must now restart to use Grunt-o-matic.\n" );
		}
		break;
	case 101:
		gEvilImpulse101 = TRUE;
		AddAccount( 16000, true );
		ALERT( at_console, "Crediting %s with $16000\n", STRING( pev->netname ) );
		break;
	case 102:
		// Gibbage!!!
		CGib::SpawnRandomGibs( pev, 1, 1 );
		break;
	case 103:
		entity = FindEntityForward( this );
		if ( entity != NULL )
		{
			CBaseMonster* monster = entity->MyMonsterPointer();
			if ( monster != NULL )
				monster->ReportAIState();
		}
		break;
	case 104:
		// Dump all of the global state varaibles (and global entity names)
		gGlobalState.DumpGlobals();
		break;
	case 105: // player makes no sound for monsters to hear.
		if ( m_fNoPlayerSound )
		{
			ALERT( at_console, "Player is audible\n" );
			m_fNoPlayerSound = FALSE;
		}
		else
		{
			ALERT( at_console, "Player is silent\n" );
			m_fNoPlayerSound = TRUE;
		}
		break;
	case 106:
		entity = FindEntityForward( this );
		if ( entity != NULL )
		{
			ALERT( at_console, "Classname: %s", STRING( entity->pev->classname ) );
			if ( entity->pev->targetname )
				ALERT( at_console, " - Targetname: %s\n", STRING( entity->pev->targetname ) );
			else
				ALERT( at_console, " - TargetName: No Targetname\n" );
			ALERT( at_console, "Model: %s\n", STRING( entity->pev->model ) );
			if ( entity->pev->globalname )
				ALERT( at_console, "Globalname: %s\n", STRING( entity->pev->globalname ) );
		}
		break;
	case 107:
	{
		edict_t* world = INDEXENT( 0 );
		Vector source = pev->origin + pev->view_ofs;
		Vector target = source + gpGlobals->v_forward * 1024;
		UTIL_TraceLine( source, target, ignore_monsters, edict(), &trace );
		if ( trace.pHit != NULL )
			world = trace.pHit;
		const char* texture = TRACE_TEXTURE( world, source, target );
		if ( texture != NULL )
			ALERT( at_console, "Texture: %s\n", texture );
		break;
	}
	case 195:
		Create( "node_viewer_fly", pev->origin, pev->angles );
		break;
	case 196:
		Create( "node_viewer_large", pev->origin, pev->angles );
		break;
	case 197:
		Create( "node_viewer_human", pev->origin, pev->angles );
		break;
	case 199:
		ALERT( at_console, "%d\n", WorldGraph.FindNearestNode( pev->origin, 1 ) );
		WorldGraph.ShowNodeConnections( WorldGraph.FindNearestNode( pev->origin, 1 ) );
		break;
	case 202: // Random blood splatter
		UTIL_MakeVectors( pev->v_angle );
		UTIL_TraceLine( pev->origin + pev->view_ofs,
			pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, edict(), &trace );
		if ( trace.flFraction != 1.0f )
		{
			CBloodSplat* blood = GetClassPtr( (CBloodSplat*)NULL );
			blood->Spawn( pev );
		}
		break;
	case 203: // remove creature.
		entity = FindEntityForward( this );
		if ( entity != NULL && entity->pev->takedamage != 0 )
			entity->SetThink( &CBaseEntity::SUB_Remove );
		break;
	case 204:
		UTIL_BloodDrips( pev->origin, Vector( 0, 0, 1 ), BLOOD_COLOR_RED, 2000 );
		for ( int range = 1; range < 4; ++range )
		{
			float bloodRange = range * 50.0f;
			for ( int i = 0; i < 50; ++i )
			{
				Vector direction;
				direction.x = RANDOM_FLOAT( -1, 1 );
				direction.y = RANDOM_FLOAT( -1, 1 );
				direction.z = RANDOM_FLOAT( -1, 1 );
				if ( direction.x == 0 && direction.y == 0 && direction.z == 0 )
				{
					direction.z = -1;
				}
				else
				{
					direction.NormalizeInPlace();
				}
				Vector source = pev->origin + pev->view_ofs;
				Vector target = source + direction * bloodRange;
				UTIL_TraceLine( source, target, ignore_monsters, edict(), &trace );
				if ( trace.flFraction < 1.0f )
					UTIL_BloodDecalTrace( &trace, BLOOD_COLOR_RED );
			}
		}
		break;
	default:
		break;
	}
}
