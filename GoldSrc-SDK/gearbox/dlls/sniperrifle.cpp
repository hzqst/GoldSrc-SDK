#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "sniperrifle.h"

#ifndef CLIENT_DLL
extern cvar_t oldweapons;
#else
extern int giOldWeapons;
#endif

enum
{
	SNIPER_DRAW = 0,
	SNIPER_IDLE = 1,
	SNIPER_RELOAD_EMPTY = 4,
	SNIPER_RELOAD_FINISH = 5,
	SNIPER_RELOAD = 6,
	SNIPER_IDLE_EMPTY = 7,
	SNIPER_HOLSTER = 8
};

LINK_ENTITY_TO_CLASS( weapon_sniperrifle, CSniperRifle );

#ifndef CLIENT_DLL
TYPEDESCRIPTION CSniperRifle::m_SaveData[] =
{
	DEFINE_FIELD( CSniperRifle, m_flReloadStart, FIELD_TIME ),
	DEFINE_FIELD( CSniperRifle, m_iReloading, FIELD_INTEGER )
};

IMPLEMENT_SAVERESTORE( CSniperRifle, CBasePlayerWeapon );
#endif

void CSniperRifle::Precache()
{
	PRECACHE_MODEL( "models/w_m40a1.mdl" );
	PRECACHE_MODEL( "models/v_m40a1.mdl" );
	PRECACHE_MODEL( "models/p_m40a1.mdl" );
	PRECACHE_SOUND( "weapons/sniper_fire.wav" );
	PRECACHE_SOUND( "weapons/sniper_zoom.wav" );
	PRECACHE_SOUND( "weapons/sniper_reload_first_seq.wav" );
	PRECACHE_SOUND( "weapons/sniper_reload_second_seq.wav" );
	PRECACHE_SOUND( "weapons/sniper_miss.wav" );
	PRECACHE_SOUND( "weapons/sniper_bolt1.wav" );
	PRECACHE_SOUND( "weapons/sniper_bolt2.wav" );
	m_usSniper = PRECACHE_EVENT( 1, "events/sniper.sc" );
}

void CSniperRifle::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_sniperrifle" );
	Precache();
	m_iId = WEAPON_SNIPERRIFLE;
	SET_MODEL( ENT( pev ), "models/w_m40a1.mdl" );
	m_iDefaultAmmo = 5;
	FallInit();
}

int CSniperRifle::GetItemInfo( ItemInfo* info )
{
	info->pszName = STRING( pev->classname );
	info->pszAmmo1 = "762";
	info->iMaxAmmo1 = _762_MAX_CARRY;
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = SNIPERRIFLE_MAX_CLIP;
	info->iSlot = 5;
	info->iPosition = 2;
	info->iFlags = 0;
	info->iId = m_iId = WEAPON_SNIPERRIFLE;
	info->iWeight = 10;
	return 1;
}

int CSniperRifle::AddToPlayer( CBasePlayer* player )
{
	if ( !CBasePlayerWeapon::AddToPlayer( player ) )
		return FALSE;
	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, player->pev );
		WRITE_BYTE( m_iId );
	MESSAGE_END();
	return TRUE;
}

void CSniperRifle::IncrementAmmo( CBasePlayer* player )
{
	if ( player->GiveAmmo( 1, "762", _762_MAX_CARRY ) >= 0 )
		EMIT_SOUND( ENT( player->pev ), CHAN_STATIC, "ctf/pow_backpack.wav", 0.5, ATTN_NORM );
}

BOOL CSniperRifle::Deploy()
{
	return DefaultDeploy( "models/v_m40a1.mdl", "models/p_m40a1.mdl", SNIPER_DRAW, "bow", 0 );
}

void CSniperRifle::Holster( int skiplocal )
{
	m_fInReload = FALSE;
	if ( m_fInZoom )
		SecondaryAttack();
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.25;
	SendWeaponAnim( SNIPER_HOLSTER, 1, 0 );
}

void CSniperRifle::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.0;
		return;
	}
	if ( !m_iClip )
	{
		PlayEmptySound();
		return;
	}
	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_iClip--;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector source = m_pPlayer->GetGunPosition();
	Vector aiming = m_pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );
#ifdef CLIENT_DLL
	bIsMultiplayer();
#else
	g_pGameRules->IsMultiplayer();
#endif
	Vector spread = m_pPlayer->FireBulletsPlayer( 1, source, aiming, g_vecZero, 8192,
		BULLET_PLAYER_762, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( UseDecrement(), m_pPlayer->edict(), m_usSniper, 0.0,
		(float*)&g_vecZero, (float*)&g_vecZero, spread.x, spread.y, m_iClip,
		max( 0, m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) ), 0, 0 );
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 2.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
}

void CSniperRifle::SetZoom()
{
	if ( m_pPlayer->pev->fov == 0 )
	{
		m_pPlayer->m_iFOV = 18;
		m_pPlayer->pev->fov = 18;
		m_fInZoom = TRUE;
	}
	else
	{
		m_pPlayer->m_iFOV = 0;
		m_pPlayer->pev->fov = 0;
		m_fInZoom = FALSE;
	}
}

void CSniperRifle::SecondaryAttack()
{
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/sniper_zoom.wav", 1.0, ATTN_NORM );
	m_fInZoom = !m_fInZoom;
	SetZoom();
	pev->nextthink = UTIL_WeaponTimeBase() + 0.1f;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

void CSniperRifle::Reload()
{
	if ( m_pPlayer->ammo_762 <= 0 )
		return;
	if ( !m_iClip )
	{
		if ( DefaultReload( SNIPERRIFLE_MAX_CLIP, SNIPER_RELOAD_EMPTY, 2.324f, 1 ) )
		{
			if ( m_fInZoom )
				SetZoom();
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 4.102;
			m_flReloadStart = gpGlobals->time;
			m_iReloading = TRUE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4.102;
			return;
		}
	}
	else if ( DefaultReload( SNIPERRIFLE_MAX_CLIP, SNIPER_RELOAD, 2.324f, 1 ) )
	{
		if ( m_fInZoom )
			SetZoom();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 2.324;
	}
	m_iReloading = FALSE;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4.102;
}

void CSniperRifle::Reload2()
{
	SendWeaponAnim( SNIPER_RELOAD_FINISH, 1, 0 );
	SetThink( NULL );
}

void CSniperRifle::WeaponIdle()
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );
	ResetEmptySound();
	if ( m_iReloading && gpGlobals->time >= m_flReloadStart + 2.324f )
	{
		SendWeaponAnim( SNIPER_RELOAD_FINISH, 1, 0 );
		m_iReloading = FALSE;
	}
	if ( UTIL_WeaponTimeBase() > m_flTimeWeaponIdle )
	{
		SendWeaponAnim( m_iClip ? SNIPER_IDLE : SNIPER_IDLE_EMPTY, 1, 0 );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4.348;
	}
}

BOOL CSniperRifle::UseDecrement()
{
#ifdef CLIENT_DLL
	return giOldWeapons != 1;
#else
	return oldweapons.value != 1.0f;
#endif
}

class CSniperRifleAmmo : public CBasePlayerAmmo
{
public:
	void Spawn()
	{
		Precache();
		SET_MODEL( ENT( pev ), "models/w_m40a1clip.mdl" );
		CBasePlayerAmmo::Spawn();
	}
	void Precache()
	{
		PRECACHE_MODEL( "models/w_m40a1clip.mdl" );
		PRECACHE_SOUND( "items/9mmclip1.wav" );
	}
	BOOL AddAmmo( CBaseEntity* other )
	{
		if ( other->GiveAmmo( 5, "762", _762_MAX_CARRY ) == -1 )
			return FALSE;
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1.0, ATTN_NORM );
		return TRUE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_762, CSniperRifleAmmo );
