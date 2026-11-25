#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

enum flashbang_e
{
	FLASHBANG_IDLE,
	FLASHBANG_PINPULL,
	FLASHBANG_THROW,
	FLASHBANG_DRAW,
};

LINK_ENTITY_TO_CLASS( weapon_flashbang, CFlashbang );

void CFlashbang::Spawn()
{
	Precache();
	m_iId = WEAPON_FLASHBANG;
	SET_MODEL( ENT( pev ), "models/w_flashbang.mdl" );
	pev->dmg = 4;
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_iDefaultAmmo = 1;
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
	FallInit();
}

void CFlashbang::Precache()
{
	PRECACHE_MODEL( "models/v_flashbang.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_flashbang.mdl" );
	PRECACHE_SOUND( "weapons/flashbang-1.wav" );
	PRECACHE_SOUND( "weapons/flashbang-2.wav" );
	PRECACHE_SOUND( "weapons/pinpull.wav" );
}

int CFlashbang::GetItemInfo( ItemInfo* info )
{
	info->pszAmmo1 = "Flashbang";
	info->iMaxAmmo1 = 2;
	info->pszName = STRING( pev->classname );
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = WEAPON_NOCLIP;
	info->iSlot = 3;
	info->iPosition = 2;
	info->iId = m_iId = WEAPON_FLASHBANG;
	info->iWeight = 1;
	info->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	return 1;
}

BOOL CFlashbang::CanDeploy()
{
	return m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0;
}

BOOL CFlashbang::Deploy()
{
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_flReleaseThrow = -1;
	m_fMaxSpeed = 250;
	m_pPlayer->m_bShieldDrawn = false;
	if ( m_pPlayer->HasShield() )
		return DefaultDeploy( "models/shield/v_shield_flashbang.mdl", "models/shield/p_shield_flashbang.mdl", FLASHBANG_DRAW, "shieldgren", UseDecrement() );
	return DefaultDeploy( "models/v_flashbang.mdl", "models/p_flashbang.mdl", FLASHBANG_DRAW, "grenade", UseDecrement() );
}

void CFlashbang::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = 0.5f;
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
	{
		m_pPlayer->pev->weapons &= ~( 1 << WEAPON_FLASHBANG );
		DestroyItem();
	}
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
}

void CFlashbang::PrimaryAttack()
{
	if ( !( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) && m_flStartThrow == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
	{
		m_flReleaseThrow = 0;
		m_flStartThrow = gpGlobals->time;
		SendWeaponAnim( FLASHBANG_PINPULL, UseDecrement() );
		m_flTimeWeaponIdle = 0.5f;
	}
}

void CFlashbang::SecondaryAttack()
{
	if ( m_pPlayer->HasShield() && m_flStartThrow <= 0 )
	{
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
		{
			m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
			SendWeaponAnim( GRENADE_SHIELD_DOWN, UseDecrement() );
			strcpy( m_pPlayer->m_szAnimExtention, "shieldgren" );
			m_fMaxSpeed = 250;
			m_pPlayer->m_bShieldDrawn = false;
		}
		else
		{
			m_iWeaponState |= WPNSTATE_SHIELD_DRAWN;
			SendWeaponAnim( GRENADE_SHIELD_UP, UseDecrement() );
			strcpy( m_pPlayer->m_szAnimExtention, "shielded" );
			m_fMaxSpeed = 180;
			m_pPlayer->m_bShieldDrawn = true;
		}
		m_pPlayer->UpdateShieldCrosshair( !( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) );
		m_pPlayer->ResetMaxSpeed();
		m_flNextSecondaryAttack = 0.4f;
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.4f );
		m_flTimeWeaponIdle = 0.6f;
	}
}

bool CFlashbang::ShieldSecondaryFire( int iUpAnim, int iDownAnim )
{
	if ( !m_pPlayer->HasShield() )
		return false;
	if ( m_flStartThrow > 0 )
		return false;
	if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
	{
		m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
		SendWeaponAnim( iDownAnim, UseDecrement() );
		strcpy( m_pPlayer->m_szAnimExtention, "shieldgren" );
		m_fMaxSpeed = 250;
		m_pPlayer->m_bShieldDrawn = false;
	}
	else
	{
		m_iWeaponState |= WPNSTATE_SHIELD_DRAWN;
		SendWeaponAnim( iUpAnim, UseDecrement() );
		strcpy( m_pPlayer->m_szAnimExtention, "shielded" );
		m_fMaxSpeed = 180;
		m_pPlayer->m_bShieldDrawn = true;
	}
	m_pPlayer->UpdateShieldCrosshair( !( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) );
	m_pPlayer->ResetMaxSpeed();
	m_flNextSecondaryAttack = 0.4f;
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.4f );
	m_flTimeWeaponIdle = 0.6f;
	return true;
}

void CFlashbang::SetPlayerShieldAnim()
{
	if ( m_pPlayer->HasShield() )
	{
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
			strcpy( m_pPlayer->m_szAnimExtention, "shield" );
		else
			strcpy( m_pPlayer->m_szAnimExtention, "shieldgren" );
	}
}

void CFlashbang::ResetPlayerShieldAnim()
{
	if ( m_pPlayer->HasShield() && ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN ) )
		strcpy( m_pPlayer->m_szAnimExtention, "shieldgren" );
}

void CFlashbang::WeaponIdle()
{
	if ( m_flReleaseThrow == 0 && m_flStartThrow != 0 )
	{
		m_flReleaseThrow = gpGlobals->time;
		if ( !( m_flTimeWeaponIdle <= 0 ) )
			return;
	}
	else if ( m_flTimeWeaponIdle > 0 )
		return;

	if ( m_flStartThrow != 0 )
	{
		m_pPlayer->Radio( "%!MRAD_FIREINHOLE", "#Fire_in_the_hole", 100, true );
		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
		if ( angThrow.x < 0 )
			angThrow.x = angThrow.x * ( ( 90.0 - 10.0 ) / 90.0 ) - 10.0;
		else
			angThrow.x = angThrow.x * ( ( 90.0 + 10.0 ) / 90.0 ) - 10.0;
		float velocity = ( 90 - angThrow.x ) * 6;
		if ( velocity > 750 )
			velocity = 750;
		UTIL_MakeVectors( angThrow );
		Vector source = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;
		Vector throwVelocity = gpGlobals->v_forward * velocity + m_pPlayer->pev->velocity;
		CGrenade::ShootTimed( m_pPlayer->pev, source, throwVelocity, 1.5f );
		SendWeaponAnim( FLASHBANG_THROW, UseDecrement() );
		SetPlayerShieldAnim();
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_flStartThrow = 0;
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.5f );
		m_flTimeWeaponIdle = 0.75f;
		m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -1 );
		if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = GetNextAttackDelay( 0.5f );
		ResetPlayerShieldAnim();
	}
	else if ( m_flReleaseThrow > 0 )
	{
		m_flStartThrow = 0;
		RetireWeapon();
	}
	else if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
	{
		float choice = RANDOM_FLOAT( 0, 1 );
		if ( m_pPlayer->HasShield() )
		{
			m_flTimeWeaponIdle = 20;
			if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
				SendWeaponAnim( GRENADE_SHIELD_IDLE, UseDecrement() );
		}
		else
		{
			if ( choice <= 0.75 )
				m_flTimeWeaponIdle = RANDOM_FLOAT( 10, 15 );
			else
				m_flTimeWeaponIdle = 2.5f;
			SendWeaponAnim( FLASHBANG_IDLE, UseDecrement() );
		}
	}
}
