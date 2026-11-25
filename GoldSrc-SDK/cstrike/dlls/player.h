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

#include "unisignals.h"
#include "hintmessage.h"
#include "weapontype.h"

#ifndef MAX_AMMO_SLOTS
#define MAX_AMMO_SLOTS 32
#endif

#define PLAYER_FATAL_FALL_SPEED		1100
#define PLAYER_MAX_SAFE_FALL_SPEED	500
#define DAMAGE_FOR_FALL_SPEED		((float)100 / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED))
#define PLAYER_MIN_BOUNCE_SPEED		350
#define PLAYER_FALL_PUNCH_THRESHHOLD	250

//
// Player PHYSICS FLAGS bits
//
#define PFLAG_ONLADDER				(1 << 0)
#define PFLAG_ONSWING				(1 << 0)
#define PFLAG_ONTRAIN				(1 << 1)
#define PFLAG_ONBARNACLE				(1 << 2)
#define PFLAG_DUCKING				(1 << 3)		// In the process of ducking, but totally squatted yet
#define PFLAG_USING					(1 << 4)		// Using a continuous entity
#define PFLAG_OBSERVER				(1 << 5)		// player is locked in stationary cam mode. Spectators can move, observers can't.

//
// generic player
//
//-----------------------------------------------------
//This is Half-Life player entity
//-----------------------------------------------------
#define CSUITPLAYLIST				4		// max of 4 suit sentences queued up at any time
#define CSUITNOREPEAT				32
#define SUIT_NEXT_IN_30SEC			30
#define SUIT_NEXT_IN_1MIN				60
#define TEAM_NAME_LENGTH				16
#define SBAR_STRING_SIZE				128
#define AUTOAIM_2DEGREES				0.0348994967025
#define AUTOAIM_5DEGREES				0.08715574274766
#define AUTOAIM_8DEGREES				0.1391731009601
#define AUTOAIM_10DEGREES				0.1736481776669

typedef enum
{
	PLAYER_IDLE,
	PLAYER_WALK,
	PLAYER_JUMP,
	PLAYER_SUPERJUMP,
	PLAYER_DIE,
	PLAYER_ATTACK1,
	PLAYER_ATTACK2,
	PLAYER_FLINCH,
	PLAYER_LARGE_FLINCH,
	PLAYER_RELOAD,
	PLAYER_HOLDBOMB
} PLAYER_ANIM;

enum _Menu
{
	Menu_OFF,
	Menu_ChooseTeam,
	Menu_IGChooseTeam,
	Menu_ChooseAppearance,
	Menu_Buy,
	Menu_BuyPistol,
	Menu_BuyRifle,
	Menu_BuyMachineGun,
	Menu_BuyShotgun,
	Menu_BuySubMachineGun,
	Menu_BuyItem,
	Menu_Radio1,
	Menu_Radio2,
	Menu_Radio3,
	Menu_ClientBuy
};

enum TeamName
{
	UNASSIGNED,
	TERRORIST,
	CT,
	SPECTATOR
};

enum ModelName
{
	MODEL_UNASSIGNED,
	MODEL_URBAN,
	MODEL_TERROR,
	MODEL_LEET,
	MODEL_ARCTIC,
	MODEL_GSG9,
	MODEL_GIGN,
	MODEL_SAS,
	MODEL_GUERILLA,
	MODEL_VIP,
	MODEL_MILITIA,
	MODEL_SPETSNAZ
};

enum JoinState
{
	JOINED,
	SHOWLTEXT,
	READINGLTEXT,
	SHOWTEAMSELECT,
	PICKINGTEAM,
	GETINTOGAME
};

struct RebuyStruct
{
	int m_primaryWeapon;
	int m_primaryAmmo;
	int m_secondaryWeapon;
	int m_secondaryAmmo;
	int m_heGrenade;
	int m_flashbang;
	int m_smokeGrenade;
	int m_defuser;
	int m_nightVision;
	int m_armor;
};

enum ThrowDirection
{
	THROW_NONE,
	THROW_FORWARD,
	THROW_BACKWARD,
	THROW_HITVEL,
	THROW_BOMB,
	THROW_GRENADE,
	THROW_HITVEL_MINUS_AIRVEL
};

enum TrackCommands
{
	CMD_SAY,
	CMD_SAYTEAM,
	CMD_FULLUPDATE,
	CMD_VOTE,
	CMD_VOTEMAP,
	CMD_LISTMAPS,
	CMD_LISTPLAYERS,
	CMD_NIGHTVISION,
	COMMANDS_TO_TRACK
};

class CBasePlayerWeapon;
class CWeaponBox;

class CBasePlayer : public CBaseMonster
{
public:
	enum sbar_data
	{
		SBAR_ID_TARGETTYPE = 1,
		SBAR_ID_TARGETNAME,
		SBAR_ID_TARGETHEALTH,
		SBAR_END
	};

	enum MusicState
	{
		SILENT,
		CALM,
		INTENSE
	};

	int random_seed;
	unsigned short m_usPlayerBleed;
	EHANDLE m_hObserverTarget;
	float m_flNextObserverInput;
	int m_iObserverWeapon;	// weapon of current tracked target
	int m_iObserverC4State;
	bool m_bObserverHasDefuser;
	int m_iObserverLastMode;
	float m_flFlinchTime;
	float m_flAnimTime;
	bool m_bHighDamage;
	float m_flVelocityModifier;
	int m_iLastZoom;
	bool m_bResumeZoom;
	float m_flEjectBrass;
	int m_iKevlar;
	bool m_bNotKilled;
	TeamName m_iTeam;
	int m_iAccount;
	bool m_bHasPrimary;
	float m_flDeathThrowTime;
	int m_iThrowDirection;
	float m_flLastTalk;
	bool m_bJustConnected;
	bool m_bContextHelp;
	JoinState m_iJoiningState;
	CBaseEntity* m_pIntroCamera;
	float m_fIntroCamTime;
	float m_fLastMovement;
	bool m_bMissionBriefing;
	bool m_bTeamChanged;
	ModelName m_iModelName;
	int m_iTeamKills;
	int m_iIgnoreGlobalChat;
	bool m_bHasNightVision;
	bool m_bNightVisionOn;
	Vector m_vRecentPath[20];
	float m_flIdleCheckTime;
	float m_flRadioTime;
	int m_iRadioMessages;
	bool m_bIgnoreRadio;
	bool m_bHasC4;
	bool m_bHasDefuser;
	bool m_bKilledByBomb;
	Vector m_vBlastVector;
	bool m_bKilledByGrenade;
	CHintMessageQueue m_hintMessageQueue;
	int32 m_flDisplayHistory;
	_Menu m_iMenu;
	int m_iChaseTarget;
	CBaseEntity* m_pChaseTarget;
	float m_fCamSwitch;
	bool m_bEscaped;
	bool m_bIsVIP;
	float m_tmNextRadarUpdate;
	Vector m_vLastOrigin;
	int m_iCurrentKickVote;
	float m_flNextVoteTime;
	bool m_bJustKilledTeammate;
	int m_iHostagesKilled;
	int m_iMapVote;
	bool m_bCanShoot;
	float m_flLastFired;
	float m_flLastAttackedTeammate;
	bool m_bHeadshotKilled;
	bool m_bPunishedForTK;
	bool m_bReceivesNoMoneyNextRound;
	int m_iTimeCheckAllowed;
	bool m_bHasChangedName;
	char m_szNewName[32];
	bool m_bIsDefusing;
	float m_tmHandleSignals;
	CUnifiedSignals m_signals;
	edict_t* m_pentCurBombTarget;
	int m_iPlayerSound; // the index of the sound list slot reserved for this player
	int m_iTargetVolume; // ideal sound volume. 
	int m_iWeaponVolume; // how loud the player's weapon is right now.
	int m_iExtraSoundTypes; // additional classification for this weapon's sound
	int m_iWeaponFlash; // brightness of the weapon flash
	float m_flStopExtraSoundTime;
	float m_flFlashLightTime;	// Time until next battery draw/Recharge
	int m_iFlashBattery;		// Flashlight Battery Draw
	int m_afButtonLast;
	int m_afButtonPressed;
	int m_afButtonReleased;
	edict_t* m_pentSndLast;			// last sound entity to modify player room type
	float m_flSndRoomtype;		// last roomtype set by sound entity
	float m_flSndRange;			// dist from player to sound entity
	float m_flFallVelocity;
	int m_rgItems[4];
	int m_fNewAmmo;			// True when a new item has been added
	unsigned int m_afPhysicsFlags;	// physics flags - set when 'normal' physics should be revisited or overriden
	float m_fNextSuicideTime; // the time after which the player can next use the suicide command


// these are time-sensitive things that we keep track of
	float m_flTimeStepSound;	// when the last stepping sound was made
	float m_flTimeWeaponIdle; // when to play another weapon idle animation.
	float m_flSwimTime;		// how long player has been underwater
	float m_flDuckTime;		// how long we've been ducking
	float m_flWallJumpTime;	// how long until next walljump
	float m_flSuitUpdate;					// when to play next suit update
	int m_rgSuitPlayList[4]; // next sentencenum to play for suit update
	int m_iSuitPlayNext;				// next sentence slot for queue storage;
	int m_rgiSuitNoRepeat[32];
	float m_rgflSuitNoRepeatTime[32];
	int m_lastDamageAmount;		// Last damage taken
	float m_tbdPrev;				// Time-based damage timer
	float m_flgeigerRange;		// range to nearest radiation source
	float m_flgeigerDelay;		// delay per update of range msg to client
	int m_igeigerRangePrev;
	int m_iStepLeft;			// alternate left/right foot stepping sound
	char m_szTextureName[17];	// current texture name we're standing on
	char m_chTextureType;		// current texture type
	int m_idrowndmg;			// track drowning damage taken
	int m_idrownrestored;		// track drowning damage restored
	int m_bitsHUDDamage;		// Damage bits for the current fame. These get sent to 
												// the hude via the DAMAGE message
	BOOL m_fInitHUD;				// True when deferred HUD restart msg needs to be sent
	BOOL m_fGameHUDInitialized;
	int m_iTrain;				// Train control position
	BOOL m_fWeapon;				// Set this to FALSE to force a reset of the current weapon HUD info
	EHANDLE m_pTank;				// the tank which the player is currently controlling,  NULL if no tank
	float m_fDeadTime;			// the time at which the player died  (used in PlayerDeathThink())
	BOOL m_fNoPlayerSound;	// a debugging feature. Player makes no sound if this is true. 
	BOOL m_fLongJump; // does this player have the longjump module?
	float m_tSneaking;
	int m_iUpdateTime;		// stores the number of frame ticks before sending HUD update messages
	int m_iClientHealth;	// the health currently known by the client.  If this changes, send a new
	int m_iClientBattery;	// the Battery currently known by the client.  If this changes, send a new
	int m_iHideHUD;		// the players hud weapon info is to be hidden
	int m_iClientHideHUD;
	int m_iFOV;			// field of view
	int m_iClientFOV;	// client's known FOV
	int m_iNumSpawns;
	CBaseEntity* m_pObserver;
	CBasePlayerItem* m_rgpPlayerItems[6];
	CBasePlayerItem* m_pActiveItem;
	CBasePlayerItem* m_pClientActiveItem;  // client version of the active item
	CBasePlayerItem* m_pLastItem;
	int m_rgAmmo[MAX_AMMO_SLOTS];
	int m_rgAmmoLast[MAX_AMMO_SLOTS];
	Vector m_vecAutoAim;
	BOOL m_fOnTarget;
	int m_iDeaths;
	int m_izSBarState[SBAR_END];
	float m_flNextSBarUpdateTime;
	float m_flStatusBarDisappearDelay;
	char m_SbarString0[SBAR_STRING_SIZE];
	int m_lastx;
	int m_lasty;
	int m_nCustomSprayFrames; // Custom clan logo frames for this player
	float m_flNextDecalTime; // next time this player can spray a decal
	char m_szTeamName[TEAM_NAME_LENGTH];
	int m_modelIndexPlayer;
	char m_szAnimExtention[32];
	int m_iGaitsequence;
	float m_flGaitframe;
	float m_flGaityaw;
	Vector m_prevgaitorigin;
	float m_flPitch;
	float m_flYaw;
	float m_flGaitMovement;
	int m_iAutoWepSwitch;
	bool m_bVGUIMenus;
	bool m_bShowHints;
	bool m_bShieldDrawn;
	bool m_bOwnsShield;
	bool m_bWasFollowing;
	float m_flNextFollowTime;
	float m_flYawModifier;
	float m_blindUntilTime;
	float m_blindStartTime;
	float m_blindHoldTime;
	float m_blindFadeTime;
	int m_blindAlpha;
	float m_allowAutoFollowTime;
	char m_autoBuyString[256];
	char* m_rebuyString;
	RebuyStruct m_rebuyStruct;
	bool m_bIsInRebuy;
	float m_flLastUpdateTime;
	char m_lastLocation[32];
	float m_progressStart;
	float m_progressEnd;
	bool m_bObserverAutoDirector;
	bool m_canSwitchObserverModes;
	float m_heartBeatTime;
	float m_intenseTimestamp;
	float m_silentTimestamp;
	MusicState m_musicState;
	float m_flLastCommandTime[COMMANDS_TO_TRACK];
	int m_iLastAccount;
	int m_iLastClientHealth;
	float m_tmNextAccountHealthUpdate;
	// After the members of the original layout. The vehicle can outlive the player's ground contact.
	EHANDLE m_hDrivenVehicle;

	~CBasePlayer() { delete[] m_rebuyString; }
	void SpawnClientSideCorpse();
	void Observer_FindNextPlayer( bool reverse, const char* name );
	CBaseEntity* Observer_IsValidTarget( int index, bool sameTeam );
	void Observer_HandleButtons();
	void Observer_SetMode( int mode );
	void Observer_CheckTarget();
	void Observer_CheckProperties();
	int IsObserver();
	void PlantC4();
	void Radio( const char* sound, const char* text, short pitch, bool showIcon );
	CBasePlayer* GetNextRadioRecipient( CBasePlayer* start );
	void SmartRadio();
	void ThrowWeapon( char* name );
	void ThrowPrimary();
	void AddAccount( int amount, bool trackChange );
	void Disappear();
	void MakeVIP();
	bool CanPlayerBuy( bool display );
	void SwitchTeam();
	void TabulateAmmo();
	void Spawn() override;
	void Pain( int hitgroup, bool hasArmour );
	virtual void Jump();
	virtual void Duck();
	virtual void PreThink();
	virtual void PostThink();
	virtual Vector GetGunPosition();
	int TakeHealth( float health, int damageType ) override;
	void TraceAttack( entvars_t* attacker, float damage, Vector direction, TraceResult* trace, int damageType ) override;
	int TakeDamage( entvars_t* inflictor, entvars_t* attacker, float damage, int damageType ) override;
	void Killed( entvars_t* attacker, int gib ) override;
	Vector BodyTarget( const Vector& ) override
	{
		Vector offset = pev->view_ofs * RANDOM_FLOAT( 0.5f, 1.1f );
		return Center() + offset;
	}
	void StartSneaking() override { m_tSneaking = gpGlobals->time - 1; }
	void StopSneaking() override { m_tSneaking = gpGlobals->time + 30; }
	BOOL IsSneaking() override { return gpGlobals->time >= m_tSneaking; }
	BOOL IsAlive() override { return pev->deadflag == DEAD_NO && pev->health > 0; }
	BOOL ShouldFadeOnDeath() override { return FALSE; }
	BOOL IsPlayer() override { return !( pev->flags & FL_SPECTATOR ); }
	virtual BOOL IsBot() { return FALSE; }
	BOOL IsBombGuy();
	bool IsLookingAtPosition( const Vector* position, float tolerance ) const;
	BOOL IsNetClient() override { return TRUE; }
	const char* TeamID() override;
	int Save( CSave& save ) override;
	int Restore( CRestore& restore ) override;
	static TYPEDESCRIPTION m_playerSaveData[];
	void Reset();
	void SetScoreboardAttributes( CBasePlayer* destination );
	void RenewItems();
	void PackDeadPlayerItems();
	void GiveDefaultItems();
	void RemoveAllItems( BOOL removeSuit );
	void SetBombIcon( int flash );
	void SetProgressBarTime( int duration );
	void SetProgressBarTime2( int duration, float elapsed );
	void SetPlayerModel( int team );
	void SetNewPlayerModel( const char* model );
	BOOL SwitchWeapon( CBasePlayerItem* weapon );
	void CheckPowerups( entvars_t* pev );
	bool CanAffordPrimary();
	bool CanAffordPrimaryAmmo();
	bool CanAffordSecondaryAmmo();
	bool CanAffordArmor();
	bool CanAffordDefuseKit();
	bool CanAffordGrenade();
	bool NeedsPrimaryAmmo();
	bool NeedsSecondaryAmmo();
	bool NeedsArmor();
	bool NeedsDefuseKit();
	bool NeedsGrenade();
	virtual void UpdateClientData();
	int ObjectCaps() override { return 0; }
	void Precache() override;
	BOOL IsOnLadder();
	BOOL FlashlightIsOn();
	void FlashlightTurnOn();
	void FlashlightTurnOff();
	void UpdatePlayerSound();
	void DeathSound();
	int Classify() override;
	void SetAnimation( PLAYER_ANIM animation );
	void SetWeaponAnimType( const char* extension );
	virtual void ImpulseCommands();
	void CheatImpulseCommands( int impulse );
	virtual void RoundRespawn();
	void StartDeathCam();
	void StartObserver( Vector position, Vector angle );
	void AddPoints( int score, BOOL allowNegative ) override;
	void AddPointsToTeam( int score, BOOL allowNegative ) override;
	void HandleSignals();
	int AddPlayerItem( CBasePlayerItem* item ) override;
	int RemovePlayerItem( CBasePlayerItem* item ) override;
	void DropPlayerItem( const char* name );
	BOOL HasPlayerItem( CBasePlayerItem* item );
	BOOL HasNamedPlayerItem( const char* name );
	BOOL HasWeapons();
	void SelectPrevItem( int item );
	void SelectNextItem( int item );
	void SelectLastItem();
	void SelectItem( const char* name );
	void ItemPreFrame();
	void ItemPostFrame();
	void GiveNamedItem( const char* name );
	void EnableControl( BOOL control );
	void ResetMaxSpeed() override;
	bool HintMessage( const char* message, BOOL displayWhenDead, BOOL overrideHints );
	int GiveAmmo( int count, char* name, int max ) override;
	void SendAmmoUpdate();
	void SendFOV( int fov );
	void WaterMove();
	void EXPORT PlayerDeathThink();
	void PlayerUse();
	void StopDrivingVehicle();
	void HostageUsed();
	void JoiningThink();
	void RemoveLevelText();
	void MenuPrint( CBasePlayer* player, const char* text );
	void ResetMenu();
	void SyncRoundTimer();
	void CheckSuitUpdate();
	void SetSuitUpdate( char* name, int group, int noRepeat );
	void UpdateGeigerCounter();
	void CheckTimeBasedDamage();
	BOOL FBecomeProne() override;
	void BarnacleVictimBitten( entvars_t* barnacle );
	void BarnacleVictimReleased();
	static int GetAmmoIndex( const char* name );
	int AmmoInventory( int ammo ) const;
	bool SetAmmoInventory( int ammo, int amount )
	{
		if ( (unsigned int)ammo >= (unsigned int)MAX_AMMO_SLOTS )
			return false;
		m_rgAmmo[ammo] = amount;
		return true;
	}
	bool AddAmmoInventory( int ammo, int delta )
	{
		if ( (unsigned int)ammo >= (unsigned int)MAX_AMMO_SLOTS )
			return false;
		m_rgAmmo[ammo] += delta;
		return true;
	}
	int *AmmoInventoryPtr( int ammo )
	{
		if ( (unsigned int)ammo >= (unsigned int)MAX_AMMO_SLOTS )
			return NULL;
		return &m_rgAmmo[ammo];
	}
	int Illumination() override;
	void ResetAutoaim();
	virtual Vector GetAutoaimVector( float delta );
	Vector AutoaimDeflection( Vector& source, float distance, float delta );
	void ForceClientDllUpdate();
	void DeathMessage( entvars_t* killer );
	void SetCustomDecalFrames( int frames );
	int GetCustomDecalFrames();
	void InitStatusBar();
	void UpdateStatusBar();
	void StudioEstimateGait();
	void StudioPlayerBlend( int* blend, float* pitch );
	void CalculatePitchBlend();
	void CalculateYawBlend();
	void StudioProcessGait();
	void SendHostagePos();
	void SendHostageIcons();
	void ResetStamina();
	BOOL IsArmored( int hitgroup );
	BOOL ShouldDoLargeFlinch( int hitgroup, int weapon );
	void SetPrefsFromUserinfo( char* info );
	void SendWeatherInfo();
	void UpdateShieldCrosshair( bool show );
	bool HasShield();
	bool IsProtectedByShield();
	void RemoveShield();
	void DropShield( bool deploy );
	void GiveShield( bool deploy );
	bool IsHittingShield( const Vector& direction, TraceResult* trace );
	bool IsReloading();
	bool IsBlind() const;
	virtual void Blind( float untilTime, float holdTime, float fadeTime, int alpha );
	virtual void OnTouchingWeapon( CWeaponBox* weapon );
	bool IsAutoFollowAllowed() const;
	void InhibitAutoFollow( float duration );
	void AllowAutoFollow();
	void ClearAutoBuyData();
	void AddAutoBuyData( const char* data );
	void AutoBuy();
	void ClientCommand( const char* command, const char* argument1, const char* argument2, const char* argument3 );
	void PrioritizeAutoBuyString( char* autobuy, const char* priority );
	const char* PickPrimaryCareerTaskWeapon();
	const char* PickSecondaryCareerTaskWeapon();
	const char* PickFlashKillWeaponString();
	const char* PickGrenadeKillWeaponString();
	bool ShouldExecuteAutoBuyCommand( const AutoBuyInfoStruct* command, bool boughtPrimary, bool boughtSecondary );
	void PostAutoBuyCommandProcessing( const AutoBuyInfoStruct* command, bool& boughtPrimary, bool& boughtSecondary );
	void ParseAutoBuyString( const char* data, bool& boughtPrimary, bool& boughtSecondary );
	AutoBuyInfoStruct* GetAutoBuyCommandInfo( const char* command );
	void InitRebuyData( const char* data );
	void BuildRebuyStruct();
	void Rebuy();
	void RebuyPrimaryWeapon();
	void RebuyPrimaryAmmo();
	void RebuySecondaryWeapon();
	void RebuySecondaryAmmo();
	void RebuyHEGrenade();
	void RebuyFlashbang();
	void RebuySmokeGrenade();
	void RebuyDefuser();
	void RebuyNightVision();
	void RebuyArmor();
	void UpdateLocation( bool force );
	void SetObserverAutoDirector( bool enabled );
	bool IsObservingPlayer( CBasePlayer* player );
	bool CanSwitchObserverModes() const;
	void Intense();
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

extern int gmsgNVGToggle;
extern int gmsgSpecHealth2;
extern int gmsgCrosshair;
extern int gmsgSetFOV;
extern int gmsgCurWeapon;
extern int gmsgStatusIcon;
extern int gmsgScenarioIcon;
extern int gmsgBombPickup;
int GetForceCamera();
void SendItemStatus( CBasePlayer* player );
const char* GetWeaponName( entvars_t* inflictor, entvars_t* killer );

#endif
