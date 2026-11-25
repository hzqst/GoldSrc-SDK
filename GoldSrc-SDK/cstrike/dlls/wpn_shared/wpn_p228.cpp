#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_p228, CP228 );

void CP228::Spawn()
{
	Precache();
	m_iId = WEAPON_P228;
	SET_MODEL( ENT( pev ), "models/w_p228.mdl" );
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_iDefaultAmmo = 13;
	m_flAccuracy = 0.9f;
	FallInit();
}

void CP228::Precache()
{
	PRECACHE_MODEL( "models/v_p228.mdl" );
	PRECACHE_MODEL( "models/w_p228.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_p228.mdl" );
	PRECACHE_SOUND( "weapons/p228-1.wav" );
	PRECACHE_SOUND( "weapons/p228_clipout.wav" );
	PRECACHE_SOUND( "weapons/p228_clipin.wav" );
	PRECACHE_SOUND( "weapons/p228_sliderelease.wav" );
	PRECACHE_SOUND( "weapons/p228_slidepull.wav" );
	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );
	m_usFireP228 = PRECACHE_EVENT( 1, "events/p228.sc" );
}

int CP228::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "357SIG";
	p->iMaxAmmo1 = 52;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 13;
	p->iSlot = 1;
	p->iPosition = 3;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_P228;
	p->iWeight = 5;
	return 1;
}

BOOL CP228::Deploy()
{
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_flAccuracy = 0.9f;
	m_fMaxSpeed = 250;
	m_pPlayer->m_bShieldDrawn = false;
	if ( m_pPlayer->HasShield() )
		return DefaultDeploy( "models/shield/v_shield_p228.mdl", "models/shield/p_shield_p228.mdl",
			5, "shieldgun", UseDecrement() );
	return DefaultDeploy( "models/v_p228.mdl", "models/p_p228.mdl", 6, "onehanded", UseDecrement() );
}

void CP228::SecondaryAttack()
{
	ShieldSecondaryFire( 7, 8 );
}

void CP228::PrimaryAttack()
{
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		P228Fire( (float)( 1.5 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		P228Fire( (float)( 0.255 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		P228Fire( (float)( 0.075 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else
		P228Fire( (float)( 0.15 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
}

void CP228::P228Fire( float flSpread, float flCycleTime, BOOL )
{
	if ( ++m_iShotsFired > 1 )
		return;
	if ( m_flLastFire != 0 )
	{
		m_flAccuracy = (float)( m_flAccuracy - ( 0.325 - ( gpGlobals->time - m_flLastFire ) ) * 0.3 );
		if ( m_flAccuracy > 0.9 )
			m_flAccuracy = 0.9f;
		else if ( !( m_flAccuracy >= 0.6 ) )
			m_flAccuracy = 0.6f;
	}
	m_flLastFire = gpGlobals->time;

	if ( m_iClip <= 0 )
	{
		if ( m_fFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = GetNextAttackDelay( 0.2f );
		}
#ifndef CLIENT_DLL
		if ( TheBots )
			TheBots->OnEvent( EVENT_WEAPON_FIRED_ON_EMPTY, m_pPlayer );
#endif
		return;
	}

	--m_iClip;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	SetPlayerShieldAnim();
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	Vector vecDir = m_pPlayer->FireBullets3( m_pPlayer->GetGunPosition(), gpGlobals->v_forward,
		flSpread, 4096, 1, BULLET_PLAYER_357SIG, 32, 0.8f, m_pPlayer->pev, true, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireP228, 0, (float*)&g_vecZero, (float*)&g_vecZero,
		vecDir.x, vecDir.y, (int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ), m_iClip == 0, 0 );
	float flDelay = GetNextAttackDelay( (float)( flCycleTime - 0.05 ) );
	m_flNextSecondaryAttack = flDelay;
	m_flNextPrimaryAttack = flDelay;
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	m_flTimeWeaponIdle = 2;
	ResetPlayerShieldAnim();
#ifndef CLIENT_DLL
	m_pPlayer->pev->punchangle.x -= 2;
#endif
}

void CP228::Reload()
{
	if ( m_pPlayer->ammo_357sig <= 0 )
		return;
	if ( DefaultReload( 13, m_pPlayer->HasShield() ? 4 : 5, 2.7f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.9f;
	}
}

void CP228::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( (float)AUTOAIM_10DEGREES );
	if ( !( m_flTimeWeaponIdle <= 0 ) )
		return;
	if ( m_pPlayer->HasShield() )
	{
		m_flTimeWeaponIdle = 20;
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
			SendWeaponAnim( 6, UseDecrement() );
	}
	else if ( m_iClip != 0 )
	{
		m_flTimeWeaponIdle = 3.0625;
		SendWeaponAnim( 0, UseDecrement() );
	}
}
