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
//=========================================================
// GameRules
//=========================================================

//#include "weapons.h"
//#include "items.h"
#ifndef GAMERULES_H
#define GAMERULES_H

#include "voice_gamemgr.h"

class CBasePlayerItem;
class CBasePlayer;
class CBasePlayerAmmo;
class CItem;

// weapon respawning return codes
enum
{
	GR_NONE = 0,
	GR_WEAPON_RESPAWN_YES,
	GR_WEAPON_RESPAWN_NO,
	GR_AMMO_RESPAWN_YES,
	GR_AMMO_RESPAWN_NO,
	GR_ITEM_RESPAWN_YES,
	GR_ITEM_RESPAWN_NO,
	GR_PLR_DROP_GUN_ALL,
	GR_PLR_DROP_GUN_ACTIVE,
	GR_PLR_DROP_GUN_NO,
	GR_PLR_DROP_AMMO_ALL,
	GR_PLR_DROP_AMMO_ACTIVE,
	GR_PLR_DROP_AMMO_NO
};

// Player relationship return codes
enum
{
	GR_NOTTEAMMATE = 0,
	GR_TEAMMATE,
	GR_ENEMY,
	GR_ALLY,
	GR_NEUTRAL
};

class CGameRules
{
public:
	virtual ~CGameRules() {}
	virtual void RefreshSkillData();
	virtual void Think() = 0;
	virtual BOOL IsAllowedToSpawn(CBaseEntity *pEntity) = 0;  // Can this item spawn (eg monsters don't spawn in deathmatch).
	virtual BOOL FAllowFlashlight() = 0; // Are players allowed to switch on their flashlight?
	virtual BOOL FShouldSwitchWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon) = 0; // should the player switch to this weapon?
	virtual BOOL GetNextBestWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon) = 0; // I can't use this weapon anymore, get me the next best one.
	virtual BOOL IsMultiplayer() = 0;
	virtual BOOL IsDeathmatch() = 0;
	virtual BOOL IsTeamplay() { return FALSE; }
	virtual BOOL IsCoOp() = 0;
	virtual const char *GetGameDescription();

// Client connection/disconnection
	virtual BOOL ClientConnected(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128]) = 0; // a client just connected to the server (player hasn't spawned yet)
	virtual void InitHUD(CBasePlayer *pPlayer) = 0;		// the client dll is ready for updating
	virtual void ClientDisconnected(edict_t *pClient) = 0; // a client just disconnected from the server
	virtual void UpdateGameMode(CBasePlayer *pPlayer) {}  // the client needs to be informed of the current game mode

// Client damage rules
	virtual float FlPlayerFallDamage(CBasePlayer *pPlayer) = 0; // this client just hit the ground after a fall. How much damage?
	virtual BOOL FPlayerCanTakeDamage(CBasePlayer *pPlayer, CBaseEntity *pAttacker) { return TRUE; } // can this player take damage from this attacker?
	virtual BOOL ShouldAutoAim(CBasePlayer *pPlayer, edict_t *target) { return TRUE; }

// Client spawn/respawn control
	virtual void PlayerSpawn(CBasePlayer *pPlayer) = 0; // called by CBasePlayer::Spawn just before releasing player into the game
	virtual void PlayerThink(CBasePlayer *pPlayer) = 0; // called by CBasePlayer::PreThink every frame, before physics are run and after keys are accepted
	virtual BOOL FPlayerCanRespawn(CBasePlayer *pPlayer) = 0; // is this player allowed to respawn now?
	virtual float FlPlayerSpawnTime(CBasePlayer *pPlayer) = 0; // When in the future will this player be able to spawn?
	virtual edict_t *GetPlayerSpawnSpot(CBasePlayer *pPlayer); // Place this player on their spawnspot and face them the proper direction.
	virtual BOOL AllowAutoTargetCrosshair() { return TRUE; }
	virtual BOOL ClientCommand_DeadOrAlive(CBasePlayer *pPlayer, const char *pcmd) { return FALSE; }
	virtual BOOL ClientCommand(CBasePlayer *pPlayer, const char *pcmd) { return FALSE; }
	virtual void ClientUserInfoChanged(CBasePlayer *pPlayer, char *infobuffer) {}		// the player has changed userinfo;  can change it now

// Client kills/scoring
	virtual int IPointsForKill(CBasePlayer *pAttacker, CBasePlayer *pKilled) = 0; // how many points do I award whoever kills this player?
	virtual void PlayerKilled(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor) = 0; // Called each time a player dies
	virtual void DeathNotice(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor) = 0; // Call this from within a GameRules class to report an obituary.
// Weapon retrieval
	virtual BOOL CanHavePlayerItem(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon); // The player is touching an CBasePlayerItem, do I give it to him?
	virtual void PlayerGotWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon) = 0; // Called each time a player picks up a weapon from the ground

// Weapon spawn/respawn control
	virtual int WeaponShouldRespawn(CBasePlayerItem *pWeapon) = 0; // should this weapon respawn?
	virtual float FlWeaponRespawnTime(CBasePlayerItem *pWeapon) = 0; // when may this weapon respawn?
	virtual float FlWeaponTryRespawn(CBasePlayerItem *pWeapon) = 0; // can i respawn now,  and if not, when should i try again?
	virtual Vector VecWeaponRespawnSpot(CBasePlayerItem *pWeapon) = 0; // where in the world should this weapon respawn?

// Item retrieval
	virtual BOOL CanHaveItem(CBasePlayer *pPlayer, CItem *pItem) = 0; // is this player allowed to take this item?
	virtual void PlayerGotItem(CBasePlayer *pPlayer, CItem *pItem) = 0; // call each time a player picks up an item (battery, healthkit, longjump)

// Item spawn/respawn control
	virtual int ItemShouldRespawn(CItem *pItem) = 0; // Should this item respawn?
	virtual float FlItemRespawnTime(CItem *pItem) = 0; // when may this item respawn?
	virtual Vector VecItemRespawnSpot(CItem *pItem) = 0; // where in the world should this item respawn?

// Ammo retrieval
	virtual BOOL CanHaveAmmo(CBasePlayer *pPlayer, const char *pszAmmoName, int iMaxCarry); // can this player take more of this ammo?
	virtual void PlayerGotAmmo(CBasePlayer *pPlayer, char *szName, int iCount) = 0; // called each time a player picks up some ammo in the world

// Ammo spawn/respawn control
	virtual int AmmoShouldRespawn(CBasePlayerAmmo *pAmmo) = 0; // should this ammo item respawn?
	virtual float FlAmmoRespawnTime(CBasePlayerAmmo *pAmmo) = 0; // when should this ammo item respawn?
	virtual Vector VecAmmoRespawnSpot(CBasePlayerAmmo *pAmmo) = 0; // where in the world should this ammo item respawn?
	virtual float FlHealthChargerRechargeTime() = 0;
	virtual float FlHEVChargerRechargeTime() { return 0; }

// What happens to a dead player's weapons
	virtual int DeadPlayerWeapons(CBasePlayer *pPlayer) = 0; // what do I do with a player's weapons when he's killed?

// What happens to a dead player's ammo	
	virtual int DeadPlayerAmmo(CBasePlayer *pPlayer) = 0; // Do I drop ammo when the player dies? How much?

// Teamplay stuff
	virtual const char *GetTeamID(CBaseEntity *pEntity) = 0; // what team is this entity on?
	virtual int PlayerRelationship(CBasePlayer *pPlayer, CBaseEntity *pTarget) = 0; // What is the player's relationship with this entity?
	virtual int GetTeamIndex(const char *pTeamName) { return -1; }
	virtual const char *GetIndexedTeamName(int teamIndex) { return ""; }
	virtual BOOL IsValidTeam(const char *pTeamName) { return TRUE; }
	virtual void ChangePlayerTeam(CBasePlayer *pPlayer, const char *pTeamName, BOOL bKill, BOOL bGib) {}
	virtual const char *SetDefaultPlayerTeam(CBasePlayer *pPlayer) { return ""; }
	virtual BOOL PlayTextureSounds() { return TRUE; }
	virtual BOOL FAllowMonsters() = 0;
	virtual void EndMultiplayerGame() {}
	virtual BOOL IsFreezePeriod() { return m_bFreezePeriod; }
	virtual void ServerDeactivate() {}
	virtual void CheckMapConditions() {}

	BOOL m_bFreezePeriod;
	BOOL m_bBombDropped;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};


//=========================================================
// CHalfLifeRules - rules for the single player Half-Life 
// game.
//=========================================================
class CHalfLifeRules : public CGameRules
{
public:
	CHalfLifeRules();
	void Think();
	BOOL IsAllowedToSpawn(CBaseEntity *pEntity);
	BOOL FAllowFlashlight() { return TRUE; }
	BOOL FShouldSwitchWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon);
	BOOL GetNextBestWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon);
	BOOL IsMultiplayer();
	BOOL IsDeathmatch();
	BOOL IsCoOp();
	BOOL ClientConnected(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128]);
	void InitHUD(CBasePlayer *pPlayer);
	void ClientDisconnected(edict_t *pClient);
	float FlPlayerFallDamage(CBasePlayer *pPlayer);
	void PlayerSpawn(CBasePlayer *pPlayer);
	void PlayerThink(CBasePlayer *pPlayer);
	BOOL FPlayerCanRespawn(CBasePlayer *pPlayer);
	float FlPlayerSpawnTime(CBasePlayer *pPlayer);
	edict_t *GetPlayerSpawnSpot(CBasePlayer *pPlayer);
	BOOL AllowAutoTargetCrosshair();
	int IPointsForKill(CBasePlayer *pAttacker, CBasePlayer *pKilled);
	void PlayerKilled(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor);
	void DeathNotice(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor);
	void PlayerGotWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon);
	int WeaponShouldRespawn(CBasePlayerItem *pWeapon);
	float FlWeaponRespawnTime(CBasePlayerItem *pWeapon);
	float FlWeaponTryRespawn(CBasePlayerItem *pWeapon);
	Vector VecWeaponRespawnSpot(CBasePlayerItem *pWeapon);
	BOOL CanHaveItem(CBasePlayer *pPlayer, CItem *pItem);
	void PlayerGotItem(CBasePlayer *pPlayer, CItem *pItem);
	int ItemShouldRespawn(CItem *pItem);
	float FlItemRespawnTime(CItem *pItem);
	Vector VecItemRespawnSpot(CItem *pItem);
	void PlayerGotAmmo(CBasePlayer *pPlayer, char *szName, int iCount);
	int AmmoShouldRespawn(CBasePlayerAmmo *pAmmo);
	float FlAmmoRespawnTime(CBasePlayerAmmo *pAmmo);
	Vector VecAmmoRespawnSpot(CBasePlayerAmmo *pAmmo);
	float FlHealthChargerRechargeTime();
	int DeadPlayerWeapons(CBasePlayer *pPlayer);
	int DeadPlayerAmmo(CBasePlayer *pPlayer);
	BOOL FAllowMonsters();
	const char *GetTeamID(CBaseEntity *pEntity) { return ""; }
	int PlayerRelationship(CBasePlayer *pPlayer, CBaseEntity *pTarget);
};

//=========================================================
// CHalfLifeMultiplay - rules for the basic half life multiplayer
// competition
//=========================================================
class CHalfLifeMultiplay : public CGameRules
{
public:
	CHalfLifeMultiplay();
	void Think();
	void RefreshSkillData();
	BOOL IsAllowedToSpawn(CBaseEntity *pEntity);
	BOOL FAllowFlashlight();
	BOOL FShouldSwitchWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon);
	BOOL GetNextBestWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon);
	BOOL IsMultiplayer();
	BOOL IsDeathmatch();
	BOOL IsCoOp();
	BOOL IsCareer();
	void QueueCareerRoundEndMenu(float delay, int winner);
	void SetCareerMatchLimit(int minWins, int winDifference);
	bool IsInCareerRound();
	void CareerRestart();
	bool ShouldSkipSpawn() const { return m_bSkipSpawn; }
	void MarkSpawnSkipped() { m_bSkipSpawn = true; }
	BOOL ClientConnected(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128]);
	void InitHUD(CBasePlayer *pPlayer);
	void ClientDisconnected(edict_t *pClient);
	void UpdateGameMode(CBasePlayer *pPlayer);
	float FlPlayerFallDamage(CBasePlayer *pPlayer);
	BOOL FPlayerCanTakeDamage(CBasePlayer *pPlayer, CBaseEntity *pAttacker);
	void PlayerSpawn(CBasePlayer *pPlayer);
	void PlayerThink(CBasePlayer *pPlayer);
	BOOL FPlayerCanRespawn(CBasePlayer *pPlayer);
	float FlPlayerSpawnTime(CBasePlayer *pPlayer);
	edict_t *GetPlayerSpawnSpot(CBasePlayer *pPlayer);
	BOOL AllowAutoTargetCrosshair();
	int IPointsForKill(CBasePlayer *pAttacker, CBasePlayer *pKilled);
	void PlayerKilled(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor);
	void DeathNotice(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor);
	void PlayerGotWeapon(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon);
	BOOL CanHavePlayerItem(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon);
	int WeaponShouldRespawn(CBasePlayerItem *pWeapon);
	float FlWeaponRespawnTime(CBasePlayerItem *pWeapon);
	float FlWeaponTryRespawn(CBasePlayerItem *pWeapon);
	Vector VecWeaponRespawnSpot(CBasePlayerItem *pWeapon);
	BOOL CanHaveItem(CBasePlayer *pPlayer, CItem *pItem);
	void PlayerGotItem(CBasePlayer *pPlayer, CItem *pItem);
	int ItemShouldRespawn(CItem *pItem);
	float FlItemRespawnTime(CItem *pItem);
	Vector VecItemRespawnSpot(CItem *pItem);
	void PlayerGotAmmo(CBasePlayer *pPlayer, char *szName, int iCount);
	int AmmoShouldRespawn(CBasePlayerAmmo *pAmmo);
	float FlAmmoRespawnTime(CBasePlayerAmmo *pAmmo);
	Vector VecAmmoRespawnSpot(CBasePlayerAmmo *pAmmo);
	float FlHealthChargerRechargeTime();
	float FlHEVChargerRechargeTime();
	int DeadPlayerWeapons(CBasePlayer *pPlayer);
	int DeadPlayerAmmo(CBasePlayer *pPlayer);
	const char *GetTeamID(CBaseEntity *pEntity) { return ""; }
	int PlayerRelationship(CBasePlayer *pPlayer, CBaseEntity *pTarget);
	BOOL PlayTextureSounds() { return FALSE; }
	BOOL FAllowMonsters();
	void EndMultiplayerGame() { GoToIntermission(); }
	void ServerDeactivate();
	BOOL ClientCommand_DeadOrAlive(CBasePlayer *pPlayer, const char *pcmd);
	BOOL ClientCommand(CBasePlayer *pPlayer, const char *pcmd);
	void ClientUserInfoChanged(CBasePlayer *pPlayer, char *infobuffer);
	void PlayerJoinedTeam(CBasePlayer *pPlayer);
	virtual void CleanUpMap();
	virtual void RestartRound();
	virtual void CheckWinConditions();
	void CheckMapConditions();
	virtual void RemoveGuns();
	virtual void GiveC4();
	float TimeRemaining();
	BOOL TeamFull(int team);
	BOOL TeamStacked(int newTeam, int currentTeam);
	bool IsVIPQueueEmpty();
	bool AddToVIPQueue(CBasePlayer *pPlayer);
	void PickNextVIP();
	void StackVIPQueue();
	void ResetCurrentVIP();
	void BalanceTeams();
	void SwapAllPlayers();
	void UpdateTeamScores();
	void DisplayMaps(CBasePlayer *pPlayer, int start);
	void ResetAllMapVotes();
	void ProcessMapVote(CBasePlayer *pPlayer, int map);
	BOOL IsThereABomber();
	BOOL IsThereABomb();
	bool IsMatchStarted();

	CVoiceGameMgr m_VoiceGameMgr;
	float m_fTeamCount;
	float m_flCheckWinConditions;
	float m_fRoundCount;
	int m_iRoundTime;
	int m_iRoundTimeSecs;
	int m_iIntroRoundTime;
	float m_fIntroRoundCount;
	int m_iAccountTerrorist;
	int m_iAccountCT;
	int m_iNumTerrorist;
	int m_iNumCT;
	int m_iNumSpawnableTerrorist;
	int m_iNumSpawnableCT;
	int m_iSpawnPointCount_Terrorist;
	int m_iSpawnPointCount_CT;
	int m_iHostagesRescued;
	int m_iHostagesTouched;
	int m_iRoundWinStatus;
	short m_iNumCTWins;
	short m_iNumTerroristWins;
	bool m_bTargetBombed;
	bool m_bBombDefused;
	bool m_bMapHasBombTarget;
	bool m_bMapHasBombZone;
	bool m_bMapHasBuyZone;
	bool m_bMapHasRescueZone;
	bool m_bMapHasEscapeZone;
	int m_iMapHasVIPSafetyZone;
	int m_bMapHasCameras;
	int m_iC4Timer;
	int m_iC4Guy;
	int m_iLoserBonus;
	int m_iNumConsecutiveCTLoses;
	int m_iNumConsecutiveTerroristLoses;
	float m_fMaxIdlePeriod;
	int m_iLimitTeams;
	bool m_bLevelInitialized;
	bool m_bRoundTerminating;
	bool m_bCompleteReset;
	float m_flRequiredEscapeRatio;
	int m_iNumEscapers;
	int m_iHaveEscaped;
	bool m_bCTCantBuy;
	bool m_bTCantBuy;
	float m_flBombRadius;
	int m_iConsecutiveVIP;
	int m_iTotalGunCount;
	int m_iTotalGrenadeCount;
	int m_iTotalArmourCount;
	int m_iUnBalancedRounds;
	int m_iNumEscapeRounds;
	int m_iMapVotes[100];
	int m_iLastPick;
	int m_iMaxMapTime;
	int m_iMaxRounds;
	int m_iTotalRoundsPlayed;
	int m_iMaxRoundsWon;
	int m_iStoredSpectValue;
	float m_flForceCameraValue;
	float m_flForceChaseCamValue;
	float m_flFadeToBlackValue;
	CBasePlayer *m_pVIP;
	CBasePlayer *VIPQueue[5];
	float m_flIntermissionEndTime;
	float m_flIntermissionStartTime;
	BOOL m_iEndIntermissionButtonHit;
	float m_tmNextPeriodicThink;
	bool m_bFirstConnected;
	bool m_bInCareerGame;
	float m_fCareerRoundMenuTime;
	int m_iCareerMatchWins;
	int m_iRoundWinDifference;
	float m_fCareerMatchMenuTime;
	bool m_bSkipSpawn;

protected:
	virtual void ChangeLevel();
	virtual void GoToIntermission();
	void SendMOTDToClient(edict_t *client);

private:
	bool HasRoundTimeExpired();
	bool IsBombPlanted();
	void MarkLivingPlayersOnTeamAsNotReceivingMoneyNextRound(int team);
};

//=========================================================
// CMultiplayBusters 
// Rules for a multiplayer mode that makes you feel good
//=========================================================
class CHalfLifeTraining : public CHalfLifeMultiplay
{
public:
	CHalfLifeTraining();
	void HostageDied();
	bool PlayerCanBuy(CBasePlayer *pPlayer);
	BOOL IsMultiplayer();
	BOOL IsDeathmatch() { return FALSE; }
	void InitHUD(CBasePlayer *pPlayer) {}
	edict_t *GetPlayerSpawnSpot(CBasePlayer *pPlayer);
	void PlayerSpawn(CBasePlayer *pPlayer);
	int ItemShouldRespawn(CItem *pItem);
	BOOL FPlayerCanRespawn(CBasePlayer *pPlayer);
	void PlayerThink(CBasePlayer *pPlayer);
	void PlayerKilled(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor);
	void CheckWinConditions();
	void CheckMapConditions();

	float FillAccountTime;
	float ServerRestartTime;
	BOOL fInBuyArea;
	BOOL fVisitedBuyArea;
	bool fVGUIMenus;
};

CGameRules *InstallGameRules();
void ShutdownGameRules();
char *GetTeam(int teamNo);
int GetMapCount();
extern DLL_GLOBAL CGameRules *g_pGameRules;

#endif // GAMERULES_H
