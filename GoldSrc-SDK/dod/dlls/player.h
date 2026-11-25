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

#ifndef PLAYER_H
#define PLAYER_H

#include "pm_materials.h"
#include "unisignals.h"
#include "hintmessage.h"

#ifndef MAX_AMMO_SLOTS
#define MAX_AMMO_SLOTS 32
#endif

#define PLAYER_FATAL_FALL_SPEED 1024 // approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED 580 // approx 20 feet
#define DAMAGE_FOR_FALL_SPEED ((float)100 / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED)) // damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED 200
#define PLAYER_FALL_PUNCH_THRESHHOLD 350

//
// Player PHYSICS FLAGS bits
//
#define PFLAG_ONLADDER (1 << 0)
#define PFLAG_ONSWING (1 << 0)
#define PFLAG_ONTRAIN (1 << 1)
#define PFLAG_ONBARNACLE (1 << 2)
#define PFLAG_DUCKING (1 << 3)		// In the process of ducking, but totally squatted yet
#define PFLAG_USING (1 << 4)		// Using a continuous entity
#define PFLAG_OBSERVER (1 << 5)		// player is locked in stationary cam mode. Spectators can move, observers can't.
#define	SOUND_FLASHLIGHT_ON		"items/flashlight1.wav"
#define	SOUND_FLASHLIGHT_OFF	"items/flashlight1.wav"
#define TEAM_NAME_LENGTH 16
#define SBAR_STRING_SIZE 128

typedef enum
{
	PLAYER_IDLE,
	PLAYER_WALK,
	PLAYER_JUMP,
	PLAYER_SUPERJUMP,
	PLAYER_DIE,
	PLAYER_ATTACK1,
	PLAYER_PRONE1,
	PLAYER_PRONE2,
	PLAYER_PRONE3,
	PLAYER_RELOAD,
	PLAYER_SIGNAL,
	PLAYER_ATTACK2,
	PLAYER_ROLLGRENADE,
	PLAYER_RPGSHOULDER,
	PLAYER_EXPLODE_BACK,
	PLAYER_EXPLODE_FORWARD,
	PLAYER_BUT_SWING
} PLAYER_ANIM;

typedef enum
{
	JUSTJOINED,
	READYTOGO,
	JOINED
} JoinState;

enum
{
	SBAR_ID_TARGETNAME = 1,
	SBAR_ID_TARGETHEALTH,
	SBAR_ID_TARGETARMOR,
	SBAR_END
};

#define MAX_ID_RANGE 2048

class CObject;
class CBasePlayerWeapon;

class CBasePlayer : public CBaseMonster
{
public:
	int m_iPlayerSpeed;
	int random_seed;    // See that is shared between client & server for shared weapons code
	int m_iPlayerSound; // the index of the sound list slot reserved for this player
	int m_iTargetVolume; // ideal sound volume. 
	int m_iWeaponVolume; // how loud the player's weapon is right now.
	int m_iExtraSoundTypes; // additional classification for this weapon's sound
	float m_flStopExtraSoundTime;
	int m_afButtonLast;
	int m_afButtonPressed;
	int m_afButtonReleased;
	edict_t *m_pentSndLast;			// last sound entity to modify player room type
	float m_flSndRoomtype;		// last roomtype set by sound entity
	float m_flSndRange;			// dist from player to sound entity
	float m_flFallVelocity;
	int m_fKnownItem;
	int m_fNewAmmo;			// True when a new item has been added
	unsigned int m_afPhysicsFlags;	// physics flags - set when 'normal' physics should be revisited or overriden
	float m_fNextSuicideTime; // the time after which the player can next use the suicide command
	float m_flTimeWeaponIdle;
	char m_szTextureName[13];
	char m_chTextureType;		// current texture type
	int m_idrowndmg;			// track drowning damage taken
	int m_idrownrestored;		// track drowning damage restored
	int m_bitsHUDDamage;		// Damage bits for the current fame. These get sent to 
												// the hude via the DAMAGE message
	BOOL m_fInitHUD;				// True when deferred HUD restart msg needs to be sent
	BOOL m_fGameHUDInitialized;
	int m_iTrain;				// Train control position
	BOOL m_fWeapon;				// Set this to FALSE to force a reset of the current weapon HUD info
	JoinState m_iJoiningState;
	EHANDLE m_pTank;				// the tank which the player is currently controlling,  NULL if no tank
	float m_fDeadTime;			// the time at which the player died  (used in PlayerDeathThink())
	int m_iUpdateTime;		// stores the number of frame ticks before sending HUD update messages
	int m_iHideHUD;
	float m_iRespawnFrames;
	int m_iClientHealth;
	int m_iClientHideHUD;
	int m_iClientFOV;
	int m_nCustomSprayFrames;
	float m_flNextDecalTime;
	CBasePlayerItem *m_rgpPlayerItems[6];
	CBasePlayerItem *m_pActiveItem;
	CBasePlayerItem *m_pClientActiveItem;  // client version of the active item
	CBasePlayerItem *m_pLastItem;
	int m_rgAmmo[MAX_AMMO_SLOTS];
	int m_rgAmmoLast[MAX_AMMO_SLOTS];
	int m_rgItems[5];
	char m_szTeamName[TEAM_NAME_LENGTH];
	int m_irdytospawn;
	int m_imissedwave;
	int m_ilastteam;
	int m_ijustjoined;
	int m_chatMsg;
	int m_deathMsg;
	int m_handSignal;
	float m_nextRunThink;
	float m_flIdleTimer;
	float m_inextprone;
	int m_voteid;
	int m_iFOV;
	float m_fGunDropTime;
	int m_iNextClass;
	bool m_bIsRandomClass;
	EHANDLE m_hObserverTarget;
	float m_flNextObserverInput;
	int m_iObserverWeapon;	// weapon of current tracked target
	int m_iObserverLastMode; // last used observer mode
	int m_bIsObserver;
	BOOL HasObject;
	CObject *m_pObject;
	char *m_sPlayerModel;
	float m_fNextStamina;
	int m_nMGAmmoCount;
	int m_nGenericAmmoCount;
	int m_iClassSpeed;
	float m_fSpeedFactor;
	bool m_bSlowedByHit;
	float m_flUnslowTime;
	float m_flNextVoice;
	float m_flNextHand;
	int lastMenuSelected;
	int m_izSBarState[SBAR_END];
	float m_flNextSBarUpdateTime;
	float m_flStatusBarDisappearDelay;
	char m_SbarString0[SBAR_STRING_SIZE];
	char m_SbarString1[SBAR_STRING_SIZE];
	EHANDLE m_hLastIDTarget;
	float m_flLastTalkTime;
	int i_cutScenes;
	int i_seenAllieSpawnScene;
	int i_seenAxisSpawnScene;
	int i_seenAllieWinScene;
	int i_seenAxisWinScene;
	int i_seenAllieLooseScene;
	int i_seenAxisLooseScene;
	int i_seenAllieDrawScene;
	int i_seenAxisDrawScene;
	CUnifiedSignals m_signals;
	int m_iCapAreaIndex;
	int m_iCapAreaIconIndex;
	int m_iObjectAreaIndex;
	float m_fHandleSignalsTime;
	int m_iObjScore;
	int m_iDeaths;
	int m_iNumTKs;
	BOOL m_bBazookaDeployed;
	int m_iMinimap;
	bool m_bShowHints;
	bool m_bAutoReload;
	CHintMessageQueue m_hintMessageQueue;
	int m_flDisplayHistory;
	char m_szAnimExtention[32];
	char m_szAnimReloadExt[32];
	int m_iGaitsequence;
	float m_flGaitframe;
	float m_flGaityaw;
	Vector m_prevgaitorigin;
	float m_flPitch;
	float m_flYaw;
	float m_flGaitMovement;
	float m_flYawModifier;
	int m_iCurrentAnimationState;
	int m_iCurrentAnimationSequence;
	int m_pszSavedWeaponModel;
	int m_ianimupdate;
	float m_flNextMapMarkerTime;
	int m_iMapMarker;
	int m_iWeapons2;

	void Spawn(void);
	void Precache(void);
	virtual void PreThink(void);
	virtual void PostThink(void);
	void TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);
	int TakeHealth(float flHealth, int bitsDamageType);
	void Killed(entvars_t *pevAttacker, int iGib);
	virtual void Jump(void);
	virtual void Duck(void);
	void WaterMove(void);
	virtual void UpdateClientData(void);
	void SetCustomDecalFrames(int nFrames);
	int GetCustomDecalFrames(void);
	int AddPlayerItem(CBasePlayerItem *pItem);
	int RemovePlayerItem(CBasePlayerItem *pItem);
	void DropPlayerItem(char *pszItemName, bool bForce);
	BOOL HasPlayerItem(CBasePlayerItem *pItem);
	BOOL HasNamedPlayerItem(const char *pszItemName);
	BOOL HasWeapons(void); // do I have ANY weapons?
	void SelectPrevItem(int iItem);
	void SelectNextItem(int iItem);
	void SelectLastItem(void);
	void SelectItem(const char *pstr);
	void ItemPreFrame(void);
	void ItemPostFrame(void);
	Vector GetGunPosition(void);
	edict_t *GiveNamedItem(const char *pszName);
	static int GetAmmoIndex(const char *psz);
	int AmmoInventory(int iAmmoIndex);
	bool SetAmmoInventory(int iAmmoIndex, int amount)
	{
		if ( (unsigned int)iAmmoIndex >= (unsigned int)MAX_AMMO_SLOTS )
			return false;
		m_rgAmmo[iAmmoIndex] = amount;
		return true;
	}
	bool AddAmmoInventory(int iAmmoIndex, int delta)
	{
		if ( (unsigned int)iAmmoIndex >= (unsigned int)MAX_AMMO_SLOTS )
			return false;
		m_rgAmmo[iAmmoIndex] += delta;
		return true;
	}
	int *AmmoInventoryPtr(int iAmmoIndex)
	{
		if ( (unsigned int)iAmmoIndex >= (unsigned int)MAX_AMMO_SLOTS )
			return NULL;
		return &m_rgAmmo[iAmmoIndex];
	}
	void TabulateAmmo(void);
	int GiveAmmo(int iAmount, const char *szName, int iMax);
	void SendAmmoUpdate(void);
	Vector BodyTarget(const Vector &posSrc)
	{
		return Center() + pev->view_ofs * RANDOM_FLOAT(0.5, 1.1);
	}
	BOOL IsAlive(void) { return pev->deadflag == DEAD_NO && pev->health > 0; }
	BOOL ShouldFadeOnDeath(void) { return FALSE; }
	BOOL IsPlayer(void) { return TRUE; }
	BOOL IsNetClient(void) { return TRUE; }
	BOOL IsOnLadder(void) { return ( pev->movetype == MOVETYPE_FLY ); }
	int Classify(void) { return CLASS_PLAYER; }
	int ObjectCaps(void) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	const char *TeamID(void) { if ( pev == NULL ) return ""; return m_szTeamName; }
	int Save(CSave &save);
	int Restore(CRestore &restore);
	static TYPEDESCRIPTION m_playerSaveData[];
	CBasePlayerWeapon *GetPrimaryWeapon(void);
	void PackDeadPlayerItems(void);
	void RemoveAllItems(BOOL removeSuit);
	BOOL SwitchWeapon(CBasePlayerItem *pWeapon, int iDeploy);
	void DelayedSpawn(void);
	void EXPORT SpawnThink(void);
	void EXPORT ServerSpawnCheck(void);
	int PlayerStatus(void);
	void EXPORT ClientDisconnect(void);
	void UTIL_EdictScreenFade(edict_t *pEdict, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags);
	void PushPlayer(void);
	void UpdatePlayerSound(void);
	void DeathSound(void);

	// custom player functions
	virtual void ImpulseCommands(void);
	void EnableControl(BOOL fControl);
	void PlayerUse(void);
	void EXPORT PlayerDeathThink(void);
	BOOL FBecomeProne(void);
	void ForceClientDllUpdate(void);
	void DeathMessage(entvars_t *pevKiller);
	void VoteMenu(int iMenu, int iChoice);
	void ForceZoomOut(void);
	void UpdateScope(int iScope);
	bool ShouldGunLower(void);
	void AddObjectIcon(char *pszIcon);
	void RemoveObjectIcon(void);
	bool PlayerHasExplodingGrenades(void);
	BOOL FlashlightIsOn(void);
	void FlashlightTurnOn(void);
	void FlashlightTurnOff(void);
	void StopObserver(void);
	int IsObserver(void) { return pev->iuser1 > 0; }
	void Observer_SpectatePlayer(int iPlayer);

	// Spectator camera
	void Observer_FindNextPlayer(bool bReverse);
	void Observer_HandleButtons(void);
	void Observer_SetMode(int iMode);
	void Observer_CheckTarget(void);
	void Observer_CheckProperties(void);
	bool Observer_PlayerValidTarget(CBaseEntity *pTarget, bool bSameTeam);
	void StartDeathCam(void);
	void StartObserver(Vector vecPosition, Vector vecViewAngle);
	void GetObject(CObject *pObject);
	void DropObject(void);
	void Stamina(void);
	float GetStamina(void) { return pev->fuser4; }
	void SetStamina(float flStamina)
	{
		if (flStamina < 0)
			flStamina = 0;
		if (flStamina > 100)
			flStamina = 100;
		pev->fuser4 = flStamina;
	}
	void DropGenericAmmo(void);
	BOOL DropItemByClass(int iClass);
	bool CheckPlayerSpeed(void);
	bool SetSpeed(int iSpeed);
	void ShowMenu(int bitsValidSlots, int nDisplayTime, char *pszText, int iNeedMore, int iFlags);
	void VoiceCom(int iCommand);
	void HandSignal(int iSignal);
	void ClearVoiceMenu(void);
	void VoiceMenu(int iMenu);

	//Player ID
	void InitStatusBar(void);
	void UpdateStatusBar(void);
	void PopHelmet(Vector *vecdir);
	void SetClientAreaIcon(int iIcon, bool bVisible);
	void HandleSignals(void);
	void AddFrags(int iScore, int bAllowNegativeScore);
	void AddObjScore(int iScore, int bAllowNegativeScore);
	void AddPoints(int score, BOOL bAllowNegativeScore);
	void AddPointsToTeam(int score, BOOL bAllowNegativeScore);
	bool HintMessage(const char *pszMessage, int iFlags);
	void SetPrefsFromUserinfo(char *infobuffer);
	void StudioEstimateGait(void);
	void StudioPlayerBlend(int *pBlend, float *pPitch);
	void CalculatePitchBlend(void);
	void CalculateYawBlend(void);
	void StudioProcessGait(void);
	Activity SetIdealActivity(Activity activity);
	void SetAnimation(PLAYER_ANIM playerAnim);
	void SetWeaponAnimType(const char *pszExtention);
	void FireMapMarker(void);
	void NextMapMarker(void);
	void PrevMapMarker(void);
	void SetMapMarker(int iMarker);
	void SpawnClientSideCorpse(void);
	bool HasWeapons(int iWeapon);
	void SetHasWeapon(int iWeapon);
	void ClearHasWeapon(int iWeapon);
	void RemoveAllWeaponFlags(void);
	int GetWeapons2(void) { return m_iWeapons2; }
	bool IsInMGDeploy(void);
	bool IsProneDeployed(void);
	bool IsInSandbagZone(void);
	bool IsSandbagDeployed(void);
	bool IsProne(void);
	void SetProneState(int iState);
	void SetSandbagState(int iState);
	int GetProneState(void) { return pev->iuser3; }
	void MortarDeploy(void);
	void MortarUnDeploy(void);
	bool IsInMortarDeploy(void);
	void GoProne(void);
	void UnProne(void);
	bool IsDucking(void);
	bool IsParatrooper(void);
	bool IsBritish(void);
	void MortarFireCommand(unsigned char iCommand);
	void SendObserverTargetWeaponAnim(void);
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

extern int gmsgHudText;
extern BOOL gInitHUD;

#endif // PLAYER_H
