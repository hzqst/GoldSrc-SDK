#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

#ifndef CLIENT_DLL
#include "bot/bot.h"
#endif

enum famas_e
{
	FAMAS_IDLE1,
	FAMAS_RELOAD,
	FAMAS_DRAW,
	FAMAS_SHOOT1,
	FAMAS_SHOOT2,
	FAMAS_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_famas, CFamas );

void CFamas::Spawn()
{
	Precache();
	m_iId = WEAPON_FAMAS;
	SET_MODEL( ENT( pev ), "models/w_famas.mdl" );
	m_iDefaultAmmo = 25;
	m_iFamasShotsFired = 0;
	m_flFamasShoot = 0;
	FallInit();
}

void CFamas::Precache()
{
	PRECACHE_MODEL( "models/v_famas.mdl" );
	PRECACHE_MODEL( "models/w_famas.mdl" );
	PRECACHE_SOUND( "weapons/famas-1.wav" );
	PRECACHE_SOUND( "weapons/famas-2.wav" );
	PRECACHE_SOUND( "weapons/famas_clipout.wav" );
	PRECACHE_SOUND( "weapons/famas_clipin.wav" );
	PRECACHE_SOUND( "weapons/famas_boltpull.wav" );
	PRECACHE_SOUND( "weapons/famas_boltslap.wav" );
	PRECACHE_SOUND( "weapons/famas_forearm.wav" );
	PRECACHE_SOUND( "weapons/famas-burst.wav" );
	m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );
	m_usFireFamas = PRECACHE_EVENT( 1, "events/famas.sc" );
}

int CFamas::GetItemInfo( ItemInfo* info )
{
	info->pszAmmo1 = "556Nato";
	info->iMaxAmmo1 = 90;
	info->pszName = STRING( pev->classname );
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = 25;
	info->iSlot = 0;
	info->iPosition = 18;
	info->iFlags = 0;
	info->iId = m_iId = WEAPON_FAMAS;
	info->iWeight = 75;
	return 1;
}

BOOL CFamas::Deploy()
{
	m_iShotsFired = 0;
	iShellOn = 1;
	m_iFamasShotsFired = 0;
	m_flAccuracy = 0.2f;
	m_flFamasShoot = 0;
	return DefaultDeploy( "models/v_famas.mdl", "models/p_famas.mdl", FAMAS_DRAW, "carbine", UseDecrement() );
}

void CFamas::SecondaryAttack()
{
	if ( m_iWeaponState & WPNSTATE_FAMAS_BURST_MODE )
	{
		ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#Switch_To_FullAuto" );
		m_iWeaponState &= ~WPNSTATE_FAMAS_BURST_MODE;
	}
	else
	{
		ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, "#Switch_To_BurstFire" );
		m_iWeaponState |= WPNSTATE_FAMAS_BURST_MODE;
	}
	m_flNextSecondaryAttack = 0.3f;
}

void CFamas::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.15f );
		return;
	}
	BOOL burst = ( m_iWeaponState & WPNSTATE_FAMAS_BURST_MODE ) != 0;
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		FamasFire( 0.3 * m_flAccuracy + 0.03, 0.0825f, FALSE, burst );
	else if ( m_pPlayer->pev->velocity.Length2D() <= 140 )
		FamasFire( 0.02 * m_flAccuracy, 0.0825f, FALSE, burst );
	else
		FamasFire( 0.07 * m_flAccuracy + 0.03, 0.0825f, FALSE, burst );
}

void CFamas::FamasFire( float spread, float cycleTime, BOOL useAutoAim, BOOL burst )
{
	if ( burst )
	{
		m_iFamasShotsFired = 0;
		cycleTime = 0.55f;
	}
	else
		spread = spread + 0.01;
	m_bDelayFire = true;
	++m_iShotsFired;
	m_flAccuracy = ( m_iShotsFired * m_iShotsFired * m_iShotsFired / 215 ) + 0.3;
	if ( m_flAccuracy > 1 )
		m_flAccuracy = 1;
	if ( m_iClip <= 0 )
	{
		if ( m_fFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.2f );
		}
#ifndef CLIENT_DLL
		if ( TheBots )
			TheBots->OnEvent( EVENT_WEAPON_FIRED_ON_EMPTY, m_pPlayer, NULL );
#endif
		return;
	}
	--m_iClip;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector direction = gpGlobals->v_forward;
	Vector source = m_pPlayer->GetGunPosition();
	if ( burst )
	{
		direction = m_pPlayer->FireBullets3( source, direction, spread, 8192, 2,
			BULLET_PLAYER_556MM, 34, 0.96f, m_pPlayer->pev, false, m_pPlayer->random_seed );
	}
	else
	{
		direction = m_pPlayer->FireBullets3( source, direction, spread, 8192, 2,
			BULLET_PLAYER_556MM, 30, 0.96f, m_pPlayer->pev, false, m_pPlayer->random_seed );
	}
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireFamas, 0,
		(float*)&g_vecZero, (float*)&g_vecZero, direction.x, direction.y, (int)( m_pPlayer->pev->punchangle.x * 10000000 ),
		(int)( m_pPlayer->pev->punchangle.y * 10000000 ), 0, 0 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( cycleTime );
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", 0, 0 );
	m_flTimeWeaponIdle = 1.1f;
	if ( !( m_pPlayer->pev->velocity.Length2D() <= 0 ) )
		KickBack( 1, 0.45f, 0.275f, 0.05f, 4, 2.5f, 7 );
	else if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		KickBack( 1.25f, 0.45f, 0.22f, 0.18f, 5.5f, 4, 5 );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		KickBack( 0.575f, 0.325f, 0.2f, 0.011f, 3.25f, 2, 8 );
	else
		KickBack( 0.625f, 0.375f, 0.25f, 0.0125f, 3.5f, 2.25f, 8 );
	if ( burst )
	{
		m_flFamasShoot = gpGlobals->time + 0.05;
		++m_iFamasShotsFired;
		m_fBurstSpread = spread;
	}
}

void CFamas::Reload()
{
	if ( m_pPlayer->ammo_556nato > 0 && DefaultReload( 25, FAMAS_RELOAD, 3.3f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		if ( m_pPlayer->m_iFOV != 90 )
			SecondaryAttack();
		m_flAccuracy = 0;
		m_iShotsFired = 0;
		m_bDelayFire = false;
	}
}

void CFamas::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 )
	{
		m_flTimeWeaponIdle = 20;
		SendWeaponAnim( FAMAS_IDLE1, UseDecrement() );
	}
}
