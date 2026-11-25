#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "m249.h"

#define M249_VISIBLE_BELT_ROUNDS	8
#define M249_LAST_ROUND_BODY		8
#define M249_BELT_BODY_OFFSET		10

#define M249_FIRE_INTERVAL			0.067
#define M249_FIRE_ANIM_TIME			0.2
#define M249_RELOAD_DELAY			1.0
#define M249_RELOAD_FINISH_DELAY	1.33f
#define M249_RELOAD_DURATION		3.78f

#define M249_RECOIL_STRENGTH		35
#define M249_RECOIL_SPEED_MARGIN	100.0

#ifdef CLIENT_DLL
extern int giOldWeapons;
#else
extern cvar_t oldweapons;
#endif

enum
{
	M249_IDLE = 0,
	M249_FIDGET,
	M249_RELOAD,
	M249_RELOAD_FINISH,
	M249_HOLSTER,
	M249_DRAW
};

LINK_ENTITY_TO_CLASS( weapon_m249, CM249 );

#ifndef CLIENT_DLL
TYPEDESCRIPTION CM249::m_SaveData[] =
{
	DEFINE_FIELD( CM249, m_flNextAnimTime, FIELD_FLOAT ),
	DEFINE_FIELD( CM249, m_flReloadStartTime, FIELD_FLOAT ),
	DEFINE_FIELD( CM249, m_flReloadStart, FIELD_FLOAT ),
	DEFINE_FIELD( CM249, m_iReloading, FIELD_INTEGER ),
	DEFINE_FIELD( CM249, m_iFire, FIELD_INTEGER ),
	DEFINE_FIELD( CM249, m_iSmoke, FIELD_INTEGER ),
	DEFINE_FIELD( CM249, m_iLink, FIELD_INTEGER ),
	DEFINE_FIELD( CM249, m_iShell, FIELD_INTEGER )
};

IMPLEMENT_SAVERESTORE( CM249, CBasePlayerWeapon );
#endif

void CM249::Precache()
{
	PRECACHE_MODEL( "models/v_saw.mdl" );
	PRECACHE_MODEL( "models/w_saw.mdl" );
	PRECACHE_MODEL( "models/p_saw.mdl" );

	m_iShell = PRECACHE_MODEL( "models/saw_shell.mdl" );
	m_iLink = PRECACHE_MODEL( "models/saw_link.mdl" );
	m_iSmoke = PRECACHE_MODEL( "sprites/wep_smoke_01.spr" );
	m_iFire = PRECACHE_MODEL( "sprites/xfire.spr" );

	PRECACHE_SOUND( "weapons/saw_reload.wav" );
	PRECACHE_SOUND( "weapons/saw_reload2.wav" );
	PRECACHE_SOUND( "weapons/saw_fire1.wav" );

	m_usFireM249 = PRECACHE_EVENT( 1, "events/m249.sc" );
}

void CM249::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_m249" );
	Precache();

	SET_MODEL( ENT( pev ), "models/w_saw.mdl" );
	m_iId = WEAPON_M249;
	m_iDefaultAmmo = M249_MAX_CLIP;
	m_iAlternatingEject = 0;

	FallInit();
}

int CM249::GetItemInfo( ItemInfo* info )
{
	info->pszName = STRING( pev->classname );

	info->pszAmmo1 = "556";
	info->iMaxAmmo1 = _556_MAX_CARRY;
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = M249_MAX_CLIP;

	info->iSlot = 5;
	info->iPosition = 0;
	info->iFlags = 0;
	info->iId = m_iId = WEAPON_M249;
	info->iWeight = 20;

	return 1;
}

int CM249::AddToPlayer( CBasePlayer* player )
{
	if ( !CBasePlayerWeapon::AddToPlayer( player ) )
		return FALSE;

	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, player->pev );
		WRITE_BYTE( m_iId );
	MESSAGE_END();

	return TRUE;
}

void CM249::IncrementAmmo( CBasePlayer* player )
{
	if ( player->GiveAmmo( 1, "556", _556_MAX_CARRY ) >= 0 )
		EMIT_SOUND( ENT( player->pev ), CHAN_STATIC, "ctf/pow_backpack.wav", 0.5, ATTN_NORM );
}

BOOL CM249::Deploy()
{
	return DefaultDeploy( "models/v_saw.mdl", "models/p_saw.mdl", M249_DRAW, "mp5", 0 );
}

int CM249::SecondaryAmmoIndex()
{
	return m_iSecondaryAmmoType;
}

void CM249::Holster( int )
{
	SetThink( NULL );
	SendWeaponAnim( M249_HOLSTER, 0, pev->body );

	m_iReloading = FALSE;
	m_fInReload = FALSE;

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}


void CM249::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	if ( m_iClip <= 0 )
	{
		if ( m_fInReload )
			return;

		if ( m_fFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15f;
		}
		else
		{
			Reload();
		}

		return;
	}

	if ( m_iClip == 1 )
		pev->body = M249_LAST_ROUND_BODY;
	else if ( m_iClip <= M249_VISIBLE_BELT_ROUNDS )
		pev->body = M249_BELT_BODY_OFFSET - m_iClip;
	else
		pev->body = 0;

	m_iAlternatingEject = !m_iAlternatingEject;
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	m_iClip--;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_flNextAnimTime = UTIL_WeaponTimeBase() + M249_FIRE_ANIM_TIME;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

#ifdef CLIENT_DLL
	BOOL multiplayer = bIsMultiplayer();
#else
	BOOL multiplayer = g_pGameRules->IsMultiplayer();
#endif

	Vector vecSpread = multiplayer ? VECTOR_CONE_6DEGREES : VECTOR_CONE_4DEGREES;
	if ( m_pPlayer->pev->button & ( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT ) )
		vecSpread = multiplayer ? VECTOR_CONE_15DEGREES : VECTOR_CONE_10DEGREES;

	if ( m_pPlayer->pev->button & IN_DUCK )
		vecSpread = multiplayer ? VECTOR_CONE_3DEGREES : VECTOR_CONE_2DEGREES;

	Vector vecShot = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, vecSpread, 8192,
		BULLET_PLAYER_556, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( UseDecrement(), m_pPlayer->edict(), m_usFireM249, 0.0,
		(float*)&g_vecZero, (float*)&g_vecZero, vecShot.x, vecShot.y, m_iAlternatingEject, pev->body, 0, 0 );

	if ( !m_iClip && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + M249_FIRE_INTERVAL;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + M249_FIRE_ANIM_TIME;

#ifndef CLIENT_DLL
	m_pPlayer->pev->punchangle.x = RANDOM_FLOAT( -2, 2 );
	m_pPlayer->pev->punchangle.y = RANDOM_FLOAT( -1, 1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	float verticalSpeed = m_pPlayer->pev->velocity.z;
	Vector vecRecoil = gpGlobals->v_forward * M249_RECOIL_STRENGTH;

	float maxSpeed = CVAR_GET_FLOAT( "sv_maxspeed" );
	if ( maxSpeed < 10 )
		maxSpeed = 320;

	if ( !g_pGameRules->IsDeathmatch() )
	{
		m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - vecRecoil;
		m_pPlayer->pev->velocity.z = verticalSpeed;
	}
	else
	{
		if ( m_pPlayer->pev->velocity.x > -( maxSpeed + M249_RECOIL_SPEED_MARGIN ) )
			m_pPlayer->pev->velocity.x -= vecRecoil.x;

		if ( m_pPlayer->pev->velocity.y > -( maxSpeed + M249_RECOIL_SPEED_MARGIN ) )
			m_pPlayer->pev->velocity.y -= vecRecoil.y;

		m_pPlayer->pev->velocity.z -= vecRecoil.z;
	}
#endif
}

void CM249::SecondaryAttack()
{
}


void CM249::Reload()
{
	if ( !DefaultReload( M249_MAX_CLIP, M249_RELOAD, M249_RELOAD_DELAY, 0 ) )
		return;

	m_iReloading = TRUE;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + M249_RELOAD_DURATION;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + M249_RELOAD_DURATION;
	m_flReloadStart = gpGlobals->time;
}

void CM249::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_iReloading && gpGlobals->time >= m_flReloadStart + M249_RELOAD_FINISH_DELAY )
	{
		m_iReloading = FALSE;
		pev->body = 0;
		SendWeaponAnim( M249_RELOAD_FINISH, 0, pev->body );
	}

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int animation;
	if ( UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 ) <= 0.95 )
	{
		animation = M249_IDLE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	}
	else
	{
		animation = M249_FIDGET;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 6.16;
	}

	SendWeaponAnim( animation, 0, pev->body );
}

BOOL CM249::UseDecrement()
{
#ifdef CLIENT_DLL
	return giOldWeapons != 1;
#else
	return oldweapons.value != 1.0f;
#endif
}


class CM249Ammo : public CBasePlayerAmmo
{
public:
	void Spawn()
	{
		Precache();
		SET_MODEL( ENT( pev ), "models/w_saw_clip.mdl" );
		CBasePlayerAmmo::Spawn();
	}

	void Precache()
	{
		PRECACHE_MODEL( "models/w_saw_clip.mdl" );
		PRECACHE_SOUND( "items/9mmclip1.wav" );
	}

	BOOL AddAmmo( CBaseEntity* other )
	{
		if ( other->GiveAmmo( M249_MAX_CLIP, "556", _556_MAX_CARRY ) == -1 )
			return FALSE;

		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1.0, ATTN_NORM );

		return TRUE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_556, CM249Ammo );
