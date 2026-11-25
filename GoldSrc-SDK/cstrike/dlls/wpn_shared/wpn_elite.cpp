#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_elite, CELITE );

void CELITE::Spawn()
{
	Precache();
	m_iId = WEAPON_ELITE;
	SET_MODEL( ENT( pev ), "models/w_elite.mdl" );
	m_iDefaultAmmo = 30;
	m_flAccuracy = 0.88f;
	FallInit();
}

void CELITE::Precache()
{
	PRECACHE_MODEL( "models/v_elite.mdl" );
	PRECACHE_MODEL( "models/w_elite.mdl" );
	PRECACHE_SOUND( "weapons/elite_fire.wav" );
	PRECACHE_SOUND( "weapons/elite_reloadstart.wav" );
	PRECACHE_SOUND( "weapons/elite_leftclipin.wav" );
	PRECACHE_SOUND( "weapons/elite_clipout.wav" );
	PRECACHE_SOUND( "weapons/elite_sliderelease.wav" );
	PRECACHE_SOUND( "weapons/elite_rightclipin.wav" );
	PRECACHE_SOUND( "weapons/elite_deploy.wav" );
	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );
	m_usFireELITE_LEFT = PRECACHE_EVENT( 1, "events/elite_left.sc" );
	m_usFireELITE_RIGHT = PRECACHE_EVENT( 1, "events/elite_right.sc" );
}

int CELITE::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = 120;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 1;
	p->iPosition = 5;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_ELITE;
	p->iWeight = 5;
	return 1;
}

BOOL CELITE::Deploy()
{
	m_flAccuracy = 0.88f;
	if ( !( m_iClip & 1 ) )
		m_iWeaponState |= WPNSTATE_ELITE_LEFT;
	return DefaultDeploy( "models/v_elite.mdl", "models/p_elite.mdl", 15, "dualpistols", UseDecrement() );
}

void CELITE::PrimaryAttack()
{
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		ELITEFire( (float)( 1.3 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		ELITEFire( (float)( 0.175 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		ELITEFire( (float)( 0.08 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
	else
		ELITEFire( (float)( 0.1 * ( 1 - m_flAccuracy ) ), 0.2f, FALSE );
}

void CELITE::ELITEFire( float flSpread, float flCycleTime, BOOL )
{
	if ( ++m_iShotsFired > 1 )
		return;
	float flTimeDiff = gpGlobals->time - m_flLastFire;
	if ( m_flLastFire != 0 )
	{
		m_flAccuracy = (float)( m_flAccuracy - ( 0.325 - flTimeDiff ) * 0.275 );
		if ( m_flAccuracy > 0.88 )
			m_flAccuracy = 0.88f;
		else if ( !( m_flAccuracy >= 0.55 ) )
			m_flAccuracy = 0.55f;
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

	float flDelay = GetNextAttackDelay( (float)( flCycleTime - 0.125 ) );
	--m_iClip;
	m_flNextSecondaryAttack = flDelay;
	m_flNextPrimaryAttack = flDelay;
	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition();
	unsigned short usEvent;
	if ( m_iWeaponState & WPNSTATE_ELITE_LEFT )
	{
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_iWeaponState &= ~WPNSTATE_ELITE_LEFT;
		vecSrc = vecSrc - gpGlobals->v_right * 5;
		usEvent = m_usFireELITE_LEFT;
	}
	else
	{
		m_pPlayer->SetAnimation( PLAYER_ATTACK2 );
		m_iWeaponState |= WPNSTATE_ELITE_LEFT;
		vecSrc = vecSrc + gpGlobals->v_right * 5;
		usEvent = m_usFireELITE_RIGHT;
	}
	Vector vecDir = m_pPlayer->FireBullets3( vecSrc, vecAiming, flSpread, 8192, 1, BULLET_PLAYER_9MM,
		36, 0.75f, m_pPlayer->pev, true, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), usEvent, 0, (float*)&g_vecZero, (float*)&g_vecZero,
		flTimeDiff, vecDir.x, (int)( vecDir.y * 100 ), m_iClip, 0, 0 );
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	m_flTimeWeaponIdle = 2;
	m_pPlayer->pev->punchangle.x -= 2;
}

void CELITE::Reload()
{
	if ( m_pPlayer->ammo_9mm > 0 && DefaultReload( 30, 14, 4.5f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.88f;
	}
}

void CELITE::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( (float)AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 && m_iClip != 0 )
	{
		m_flTimeWeaponIdle = 60;
		SendWeaponAnim( m_iClip == 1, UseDecrement() );
	}
}
