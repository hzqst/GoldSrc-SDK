#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#ifndef CLIENT_DLL
#include "bot.h"
#include "GameEvent.h"
#endif

LINK_ENTITY_TO_CLASS( weapon_sg550, CSG550 );

void CSG550::Spawn()
{
	Precache();
	m_iId = WEAPON_SG550;
	SET_MODEL( ENT( pev ), "models/w_sg550.mdl" );
	m_iDefaultAmmo = 30;
	m_flLastFire = 0;
	FallInit();
}

void CSG550::Precache()
{
	PRECACHE_MODEL( "models/v_sg550.mdl" );
	PRECACHE_MODEL( "models/w_sg550.mdl" );
	PRECACHE_SOUND( "weapons/sg550-1.wav" );
	PRECACHE_SOUND( "weapons/sg550_boltpull.wav" );
	PRECACHE_SOUND( "weapons/sg550_clipin.wav" );
	PRECACHE_SOUND( "weapons/sg550_clipout.wav" );
	PRECACHE_SOUND( "weapons/zoom.wav" );
	m_iShellId = m_iShell = PRECACHE_MODEL( "models/rshell.mdl" );
	m_usFireSG550 = PRECACHE_EVENT( 1, "events/sg550.sc" );
}

int CSG550::GetItemInfo( ItemInfo* p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "556Nato";
	p->iMaxAmmo1 = 90;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 0;
	p->iPosition = 16;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SG550;
	p->iWeight = 20;
	return 1;
}

float CSG550::GetMaxSpeed()
{
	return m_pPlayer->m_iFOV == 90 ? 210.0f : 150.0f;
}

BOOL CSG550::Deploy()
{
	return DefaultDeploy( "models/v_sg550.mdl", "models/p_sg550.mdl", 4, "rifle", UseDecrement() );
}

void CSG550::SecondaryAttack()
{
	switch ( m_pPlayer->m_iFOV )
	{
	case 90:
		m_pPlayer->m_iFOV = 40;
		m_pPlayer->pev->fov = 40;
		break;
	case 40:
		m_pPlayer->m_iFOV = 15;
		m_pPlayer->pev->fov = 15;
		break;
	case 15:
		m_pPlayer->m_iFOV = 90;
		m_pPlayer->pev->fov = 90;
		break;
	}
	m_pPlayer->ResetMaxSpeed();
#ifndef CLIENT_DLL
	if ( TheBots )
		TheBots->OnEvent( EVENT_WEAPON_ZOOMED, m_pPlayer );
#endif
	EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/zoom.wav", 0.2f, 2.4f, 0, PITCH_NORM );
	m_flNextSecondaryAttack = 0.3f;
}

void CSG550::PrimaryAttack()
{
	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		SG550Fire( (float)( 0.45 * ( 1 - m_flAccuracy ) ), 0.25f, FALSE );
	else if ( m_pPlayer->pev->velocity.Length2D() > 0 )
		SG550Fire( 0.15f, 0.25f, FALSE );
	else if ( m_pPlayer->pev->flags & FL_DUCKING )
		SG550Fire( (float)( 0.04 * ( 1 - m_flAccuracy ) ), 0.25f, FALSE );
	else
		SG550Fire( (float)( 0.05 * ( 1 - m_flAccuracy ) ), 0.25f, FALSE );
}

void CSG550::SG550Fire( float flSpread, float flCycleTime, BOOL )
{
	if ( m_pPlayer->pev->fov == 90 )
		flSpread = (float)( flSpread + 0.025 );
	if ( m_flLastFire != 0 )
	{
		float flAccuracy = (float)( ( gpGlobals->time - m_flLastFire ) * 0.35 + 0.65 );
		m_flAccuracy = flAccuracy <= 0.98 ? flAccuracy : 0.98f;
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
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	m_pPlayer->m_iWeaponVolume = BIG_EXPLOSION_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	Vector vecDir = m_pPlayer->FireBullets3( m_pPlayer->GetGunPosition(), gpGlobals->v_forward,
		flSpread, 8192, 2, BULLET_PLAYER_556MM, 70, 0.98f, m_pPlayer->pev, true, m_pPlayer->random_seed );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireSG550, 0, (float*)&g_vecZero, (float*)&g_vecZero,
		vecDir.x, vecDir.y, (int)( m_pPlayer->pev->punchangle.x * 100 ),
		(int)( m_pPlayer->pev->punchangle.y * 100 ), 5, 0 );
	float flDelay = GetNextAttackDelay( flCycleTime );
	m_flNextSecondaryAttack = flDelay;
	m_flNextPrimaryAttack = flDelay;
	if ( m_iClip == 0 && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	m_flTimeWeaponIdle = 1.8f;
	m_pPlayer->pev->punchangle.x -= UTIL_SharedRandomFloat( m_pPlayer->random_seed + 4, 0.75f, 1.25f ) +
		m_pPlayer->pev->punchangle.x / 4;
	m_pPlayer->pev->punchangle.y = UTIL_SharedRandomFloat( m_pPlayer->random_seed + 5, -0.75f, 0.75f ) +
		m_pPlayer->pev->punchangle.y;
}

void CSG550::Reload()
{
	if ( m_pPlayer->ammo_556nato > 0 && DefaultReload( 30, 3, 3.35f ) )
	{
		m_pPlayer->SetAnimation( PLAYER_RELOAD );
		if ( m_pPlayer->pev->fov != 90 )
		{
			m_pPlayer->pev->fov = 15;
			m_pPlayer->m_iFOV = 15;
			SecondaryAttack();
		}
	}
}

void CSG550::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( (float)AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle <= 0 && m_iClip != 0 )
	{
		m_flTimeWeaponIdle = 60;
		SendWeaponAnim( 0, UseDecrement() );
	}
}
