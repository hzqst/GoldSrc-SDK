#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

#ifndef CLIENT_DLL
#include "bot/bot.h"
#endif

enum ump45_e
{
	UMP45_IDLE1,
	UMP45_RELOAD,
	UMP45_DRAW,
	UMP45_SHOOT1,
	UMP45_SHOOT2,
	UMP45_SHOOT3,
};

LINK_ENTITY_TO_CLASS( weapon_ump45, CUMP45 );

void CUMP45::Spawn()
{
	Precache();
	m_iId = WEAPON_UMP45;
	SET_MODEL( ENT( pev ), "models/w_ump45.mdl" );
	m_iDefaultAmmo = 25;
	m_flAccuracy = 0;
	m_bDelayFire = false;
	FallInit();
}

void CUMP45::Precache()
{
	PRECACHE_MODEL( "models/v_ump45.mdl" );
	PRECACHE_MODEL( "models/w_ump45.mdl" );
	PRECACHE_SOUND( "weapons/ump45-1.wav" );
	PRECACHE_SOUND( "weapons/ump45_clipout.wav" );
	PRECACHE_SOUND( "weapons/ump45_clipin.wav" );
	PRECACHE_SOUND( "weapons/ump45_boltslap.wav" );
	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );
	m_usFireUMP45 = PRECACHE_EVENT( 1, "events/ump45.sc" );
}

int CUMP45::GetItemInfo( ItemInfo* info )
{
	info->pszAmmo1 = "45ACP";
	info->iMaxAmmo1 = 100;
	info->pszName = STRING( pev->classname );
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = 25;
	info->iSlot = 0;
	info->iPosition = 15;
	info->iFlags = 0;
	info->iId = m_iId = WEAPON_UMP45;
	info->iWeight = 25;
	return 1;
}

BOOL CUMP45::Deploy()
{
	iShellOn = 1;
	m_flAccuracy = 0;
	m_bDelayFire = false;
	return DefaultDeploy( "models/v_ump45.mdl", "models/p_ump45.mdl", UMP45_DRAW, "carbine", UseDecrement() );
}

void CUMP45::PrimaryAttack()
{
	if ( m_pPlayer->pev->flags & FL_ONGROUND )
		UMP45Fire( 0.04 * m_flAccuracy, 0.1f, FALSE );
	else
		UMP45Fire( 0.24 * m_flAccuracy, 0.1f, FALSE );
}

void CUMP45::UMP45Fire( float spread, float cycleTime, BOOL useAutoAim )
{
	m_bDelayFire = true;
	++m_iShotsFired;
	m_flAccuracy = ( m_iShotsFired * m_iShotsFired / 210 ) + 0.5;
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
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector direction = gpGlobals->v_forward;
	Vector source = m_pPlayer->GetGunPosition();
	direction = m_pPlayer->FireBullets3( source, direction, spread, 8192, 1,
		BULLET_PLAYER_45ACP, 30, 0.82f, m_pPlayer->pev, false, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireUMP45, 0,
		(float*)&g_vecZero, (float*)&g_vecZero, direction.x, direction.y, (int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ), 0, 0 );
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( cycleTime );
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", 0, 0 );
	m_flTimeWeaponIdle = 2;
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		KickBack( 0.125f, 0.65f, 0.55f, 0.0475f, 5.5f, 4, 10 );
	else if ( !( m_pPlayer->pev->velocity.Length2D() <= 0 ) )
		KickBack( 0.55f, 0.3f, 0.225f, 0.03f, 3.5f, 2.5f, 10 );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		KickBack( 0.25f, 0.175f, 0.125f, 0.02f, 2.25f, 1.25f, 10 );
	else
		KickBack( 0.275f, 0.2f, 0.15f, 0.0225f, 2.5f, 1.5f, 10 );
}

void CUMP45::Reload()
{
	if ( m_pPlayer->ammo_45acp > 0 && DefaultReload( 25, UMP45_RELOAD, 3.5f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0;
		m_iShotsFired = 0;
	}
}

void CUMP45::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 )
	{
		m_flTimeWeaponIdle = 20;
		SendWeaponAnim( UMP45_IDLE1, UseDecrement() );
	}
}
