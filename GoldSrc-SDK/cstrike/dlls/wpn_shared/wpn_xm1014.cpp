#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_xm1014, CXM1014 );

void CXM1014::Spawn()
{
	Precache();
	m_iId = WEAPON_XM1014;
	SET_MODEL( ENT( pev ), "models/w_xm1014.mdl" );
	m_iDefaultAmmo = 7;
	FallInit();
}

void CXM1014::Precache()
{
	PRECACHE_MODEL( "models/v_xm1014.mdl" );
	PRECACHE_MODEL( "models/w_xm1014.mdl" );
	m_iShellId = m_iShell = PRECACHE_MODEL( "models/shotgunshell.mdl" );
	PRECACHE_SOUND( "weapons/xm1014-1.wav" );
	PRECACHE_SOUND( "weapons/reload1.wav" );
	PRECACHE_SOUND( "weapons/reload3.wav" );
	m_usFireXM1014 = PRECACHE_EVENT( 1, "events/xm1014.sc" );
}

int CXM1014::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = 32;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 7;
	p->iSlot = 0;
	p->iPosition = 12;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_XM1014;
	p->iWeight = 20;
	return 1;
}

BOOL CXM1014::Deploy()
{
	return DefaultDeploy( "models/v_xm1014.mdl", "models/p_xm1014.mdl", 6, "m249", UseDecrement() );
}

BOOL CXM1014::PlayEmptySound()
{
	BOOL bPlayed = CBasePlayerWeapon::PlayEmptySound();
	m_iPlayEmptySound = 0;
	return bPlayed;
}

void CXM1014::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.15f );
		return;
	}
	if ( m_iClip <= 0 )
	{
		if ( !m_fInSpecialReload )
		{
			PlayEmptySound();
#ifndef CLIENT_DLL
			if ( TheBots )
				TheBots->OnEvent( EVENT_WEAPON_FIRED_ON_EMPTY, m_pPlayer );
#endif
		}
		Reload();
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	--m_iClip;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition();
	m_pPlayer->FireBullets( 6, vecSrc, vecAiming, Vector( 0.0725f, 0.0725f, 0 ),
		3048, BULLET_PLAYER_BUCKSHOT, 0 );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireXM1014, 0,
		(float*)&g_vecZero, (float*)&g_vecZero,
		m_vVecAiming.x, m_vVecAiming.y, 7, (int)( m_vVecAiming.x * 100 ), m_iClip == 0, 0 );
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	if ( m_iClip != 0 )
		m_flPumpTime = 0.125;
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.25f );
	m_flNextSecondaryAttack = 0.25;
	m_fInSpecialReload = 0;
	m_flTimeWeaponIdle = m_iClip ? 2.25f : 0.75f;
	int iPunch;
	if ( m_pPlayer->pev->flags & FL_ONGROUND )
		iPunch = UTIL_SharedRandomLong( m_pPlayer->random_seed + 1, 3, 5 );
	else
		iPunch = UTIL_SharedRandomLong( m_pPlayer->random_seed + 1, 7, 10 );
	m_pPlayer->pev->punchangle.x -= (float)iPunch;
}

void CXM1014::Reload()
{
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 || m_iClip == 7 || !( m_flNextPrimaryAttack <= 0 ) )
		return;
	ResetEmptySound();
	if ( !m_fInSpecialReload )
	{
		m_fInSpecialReload = 1;
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		SendWeaponAnim( 5, UseDecrement() );
		m_pPlayer->m_flNextAttack = 0.55f;
		m_flTimeWeaponIdle = 0.55f;
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.55f );
		m_flNextSecondaryAttack = 0.55f;
	}
	else if ( m_fInSpecialReload == 1 )
	{
		if ( m_flTimeWeaponIdle <= 0 )
		{
			m_fInSpecialReload = 2;
			const char* sample = RANDOM_LONG( 0, 1 ) ? "weapons/reload1.wav" : "weapons/reload3.wav";
			int pitch = 85 + RANDOM_LONG( 0, 31 );
			EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, sample, 1, ATTN_NORM, 0, pitch );
			SendWeaponAnim( 3, UseDecrement() );
			m_flNextReload = 0.3f;
			m_flTimeWeaponIdle = 0.3f;
		}
	}
	else if ( m_flTimeWeaponIdle <= 0 )
	{
		m_fInSpecialReload = 1;
		++m_iClip;
		m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -1 );
		--m_pPlayer->ammo_buckshot;
	}
}

void CXM1014::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( (float)AUTOAIM_5DEGREES );
	if ( m_flPumpTime < 0 )
		m_flPumpTime = 0;
	if ( m_flTimeWeaponIdle >= 0 )
		return;
	if ( m_fInSpecialReload )
	{
		if ( m_iClip == 7 || m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		{
			SendWeaponAnim( 4, UseDecrement() );
			m_fInSpecialReload = 0;
			m_flTimeWeaponIdle = 1.5;
		}
		else
		{
			Reload();
		}
		return;
	}
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
	{
		Reload();
		return;
	}
	SendWeaponAnim( 0, UseDecrement() );
	m_flTimeWeaponIdle = 20;
}
