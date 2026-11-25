#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_deagle, CDEAGLE );

void CDEAGLE::Spawn()
{
	Precache();
	m_iId = WEAPON_DEAGLE;
	SET_MODEL( ENT( pev ), "models/w_deagle.mdl" );
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_iDefaultAmmo = 7;
	m_flAccuracy = 0.9f;
	m_fMaxSpeed = 250;
	FallInit();
}

void CDEAGLE::Precache()
{
	PRECACHE_MODEL( "models/v_deagle.mdl" );
	PRECACHE_MODEL( "models/shield/v_shield_deagle.mdl" );
	PRECACHE_MODEL( "models/w_deagle.mdl" );
	PRECACHE_SOUND( "weapons/deagle-1.wav" );
	PRECACHE_SOUND( "weapons/deagle-2.wav" );
	PRECACHE_SOUND( "weapons/de_clipout.wav" );
	PRECACHE_SOUND( "weapons/de_clipin.wav" );
	PRECACHE_SOUND( "weapons/de_deploy.wav" );
	m_iShell = PRECACHE_MODEL( "models/pshell.mdl" );
	m_usFireDeagle = PRECACHE_EVENT( 1, "events/deagle.sc" );
}

int CDEAGLE::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "50AE";
	p->iMaxAmmo1 = 35;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 7;
	p->iSlot = 1;
	p->iPosition = 1;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_DEAGLE;
	p->iWeight = 7;
	return 1;
}

BOOL CDEAGLE::Deploy()
{
	m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
	m_flAccuracy = 0.9f;
	m_pPlayer->m_bShieldDrawn = false;
	m_fMaxSpeed = 250;
	if ( m_pPlayer->HasShield() )
		return DefaultDeploy( "models/shield/v_shield_deagle.mdl", "models/shield/p_shield_deagle.mdl",
			5, "shieldgun", UseDecrement() );
	return DefaultDeploy( "models/v_deagle.mdl", "models/p_deagle.mdl", 5, "onehanded", UseDecrement() );
}

void CDEAGLE::SecondaryAttack()
{
	ShieldSecondaryFire( 7, 8 );
}

void CDEAGLE::PrimaryAttack()
{
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		DEAGLEFire( (float)( 1.5 * ( 1 - m_flAccuracy ) ), 0.3f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		DEAGLEFire( (float)( 0.25 * ( 1 - m_flAccuracy ) ), 0.3f, FALSE );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		DEAGLEFire( (float)( 0.115 * ( 1 - m_flAccuracy ) ), 0.3f, FALSE );
	else
		DEAGLEFire( (float)( 0.13 * ( 1 - m_flAccuracy ) ), 0.3f, FALSE );
}

void CDEAGLE::DEAGLEFire( float flSpread, float flCycleTime, BOOL )
{
	if ( ++m_iShotsFired > 1 )
		return;
	if ( m_flLastFire != 0 )
	{
		m_flAccuracy = (float)( m_flAccuracy - ( 0.4 - ( gpGlobals->time - m_flLastFire ) ) * 0.35 );
		if ( m_flAccuracy > 0.9 )
			m_flAccuracy = 0.9f;
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

	--m_iClip;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	SetPlayerShieldAnim();
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	Vector vecDir = m_pPlayer->FireBullets3( m_pPlayer->GetGunPosition(), gpGlobals->v_forward,
		flSpread, 4096, 2, BULLET_PLAYER_50AE, 54, 0.81f, m_pPlayer->pev, true, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireDeagle, 0,
		(float*)&g_vecZero, (float*)&g_vecZero,
		vecDir.x, vecDir.y, (int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ), m_iClip == 0, 0 );
	float flDelay = GetNextAttackDelay( (float)( flCycleTime - 0.075 ) );
	m_flNextSecondaryAttack = flDelay;
	m_flNextPrimaryAttack = flDelay;
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	m_flTimeWeaponIdle = 1.8f;
	m_pPlayer->pev->punchangle.x -= 2;
	ResetPlayerShieldAnim();
}

void CDEAGLE::Reload()
{
	if ( m_pPlayer->ammo_50ae > 0 && DefaultReload( 7, 4, 2.2f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		m_flAccuracy = 0.9f;
	}
}

void CDEAGLE::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( (float)AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 )
	{
		m_flTimeWeaponIdle = 20;
		if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
			SendWeaponAnim( 6, UseDecrement() );
	}
}
