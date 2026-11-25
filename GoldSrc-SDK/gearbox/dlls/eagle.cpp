#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "eagle.h"

#ifdef CLIENT_DLL
extern int giOldWeapons;
#else
extern cvar_t oldweapons;

class CEagleLaser : public CBaseEntity
{
public:
	void Spawn();
	void Precache();
	int ObjectCaps() { return FCAP_DONT_SAVE; }
	static CEagleLaser* CreateSpot();
	void Suspend( float suspendTime );
	void EXPORT Revive();
};

LINK_ENTITY_TO_CLASS( eagle_laser, CEagleLaser );

TYPEDESCRIPTION CEagle::m_SaveData[] =
{
	DEFINE_FIELD( CEagle, m_iLaserActive, FIELD_INTEGER ),
	DEFINE_FIELD( CEagle, m_fSpotVisible, FIELD_INTEGER )
};

IMPLEMENT_SAVERESTORE( CEagle, CBasePlayerWeapon );

void CEagleLaser::Precache()
{
	PRECACHE_MODEL( "sprites/laserdot.spr" );
}

void CEagleLaser::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->rendermode = kRenderGlow;
	pev->renderfx = kRenderFxNoDissipation;
	pev->renderamt = 255;
	pev->scale = 0.5;
	SET_MODEL( ENT( pev ), "sprites/laserdot.spr" );
	UTIL_SetOrigin( pev, pev->origin );
}

CEagleLaser* CEagleLaser::CreateSpot()
{
	CEagleLaser* spot = GetClassPtr( (CEagleLaser*)NULL );
	spot->Spawn();
	spot->pev->classname = MAKE_STRING( "eagle_laser" );
	return spot;
}

void CEagleLaser::Suspend( float suspendTime )
{
	pev->effects |= EF_NODRAW;
	SetThink( &CEagleLaser::Revive );
	pev->nextthink = gpGlobals->time + suspendTime;
}

void CEagleLaser::Revive()
{
	pev->effects &= ~EF_NODRAW;
	SetThink( NULL );
}
#endif

enum
{
	EAGLE_IDLE1 = 0,
	EAGLE_IDLE2,
	EAGLE_IDLE3,
	EAGLE_IDLE_LASER1,
	EAGLE_IDLE_LASER2,
	EAGLE_RELOAD_EMPTY = 7,
	EAGLE_RELOAD,
	EAGLE_DRAW,
	EAGLE_HOLSTER
};

LINK_ENTITY_TO_CLASS( weapon_eagle, CEagle );

void CEagle::Precache()
{
	PRECACHE_MODEL( "models/v_desert_eagle.mdl" );
	PRECACHE_MODEL( "models/w_desert_eagle.mdl" );
	PRECACHE_MODEL( "models/p_desert_eagle.mdl" );
	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );
	PRECACHE_SOUND( "weapons/desert_eagle_fire.wav" );
	PRECACHE_SOUND( "weapons/desert_eagle_reload.wav" );
	PRECACHE_SOUND( "weapons/desert_eagle_sight.wav" );
	PRECACHE_SOUND( "weapons/desert_eagle_sight2.wav" );
	m_usFireEagle = PRECACHE_EVENT( 1, "events/eagle.sc" );
}

void CEagle::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_eagle" );
	Precache();
	m_iId = WEAPON_EAGLE;
	SET_MODEL( ENT( pev ), "models/w_desert_eagle.mdl" );
	m_iDefaultAmmo = 7;
	FallInit();
}

int CEagle::GetItemInfo( ItemInfo* info )
{
	info->pszName = STRING( pev->classname );
	info->pszAmmo1 = "357";
	info->iMaxAmmo1 = _357_MAX_CARRY;
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = EAGLE_MAX_CLIP;
	info->iSlot = 1;
	info->iPosition = 2;
	info->iFlags = 0;
	info->iId = m_iId = WEAPON_EAGLE;
	info->iWeight = 15;
	return 1;
}

int CEagle::AddToPlayer( CBasePlayer* player )
{
	if ( !CBasePlayerWeapon::AddToPlayer( player ) )
		return FALSE;
	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, player->pev );
		WRITE_BYTE( m_iId );
	MESSAGE_END();
	return TRUE;
}

void CEagle::IncrementAmmo( CBasePlayer* player )
{
	if ( player->GiveAmmo( 1, "357", _357_MAX_CARRY ) >= 0 )
		EMIT_SOUND( ENT( player->pev ), CHAN_STATIC, "ctf/pow_backpack.wav", 0.5, ATTN_NORM );
}

BOOL CEagle::Deploy()
{
	m_fSpotVisible = TRUE;
	return DefaultDeploy( "models/v_desert_eagle.mdl", "models/p_desert_eagle.mdl", EAGLE_DRAW, "onehanded", 0 );
}

void CEagle::Holster( int skiplocal )
{
	m_fInReload = FALSE;
#ifndef CLIENT_DLL
	if ( m_pLaser )
	{
		m_pLaser->Killed( NULL, GIB_NEVER );
		m_pLaser = NULL;
		m_fSpotVisible = FALSE;
	}
#endif
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	SendWeaponAnim( EAGLE_HOLSTER, 1, 0 );
}

void CEagle::SecondaryAttack()
{
	m_iLaserActive = !m_iLaserActive;
#ifndef CLIENT_DLL
	if ( !m_iLaserActive && m_pLaser )
	{
		m_pLaser->Killed( NULL, 0 );
		m_pLaser = NULL;
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/desert_eagle_sight2.wav", 1.0, ATTN_NORM );
	}
#endif
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

void CEagle::UpdateLaser()
{
#ifndef CLIENT_DLL
	if ( !m_iLaserActive || !m_fSpotVisible )
		return;
	if ( !m_pLaser )
	{
		m_pLaser = CEagleLaser::CreateSpot();
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/desert_eagle_sight.wav", 1.0, ATTN_NORM );
	}
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector source = m_pPlayer->GetGunPosition();
	Vector end = source + gpGlobals->v_forward * 8192;
	TraceResult trace;
	UTIL_TraceLine( source, end, dont_ignore_monsters, m_pPlayer->edict(), &trace );
	UTIL_SetOrigin( m_pLaser->pev, trace.vecEndPos );
#endif
}

void CEagle::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}
	if ( m_iClip <= 0 )
	{
		if ( !m_fInReload )
		{
			if ( m_fFireOnEmpty )
			{
				PlayEmptySound();
				m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.2;
			}
			else
			{
				Reload();
			}
		}
		return;
	}
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	m_iClip--;
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
#ifndef CLIENT_DLL
	if ( m_pLaser && m_iLaserActive )
		m_pLaser->Suspend( 0.6f );
#endif
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector source = m_pPlayer->GetGunPosition();
	Vector aiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	float accuracy = m_iLaserActive ? 0.001f : 0.1f;
	Vector spread = m_pPlayer->FireBulletsPlayer( 1, source, aiming, Vector( accuracy, accuracy, accuracy ),
		8192, BULLET_PLAYER_EAGLE, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	if ( m_iLaserActive )
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
	else
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.22;
	PLAYBACK_EVENT_FULL( UseDecrement(), m_pPlayer->edict(), m_usFireEagle, 0.0,
		(float*)&g_vecZero, (float*)&g_vecZero, spread.x, spread.y, 0, 0, 0, 0 );
	if ( !m_iClip && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	float time = UTIL_WeaponTimeBase();
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 ) + time;
#ifndef CLIENT_DLL
	if ( m_iLaserActive && m_fSpotVisible )
		UpdateLaser();
#endif
}

void CEagle::Reload()
{
	if ( m_pPlayer->ammo_357 <= 0 )
		return;
	if ( !DefaultReload( EAGLE_MAX_CLIP, m_iClip ? EAGLE_RELOAD : EAGLE_RELOAD_EMPTY, 1.5, 1 ) )
		return;
#ifndef CLIENT_DLL
	if ( m_pLaser && m_iLaserActive )
	{
		m_pLaser->Suspend( 1.6f );
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.5;
	}
#endif
	float time = UTIL_WeaponTimeBase();
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 ) + time;
}

void CEagle::WeaponIdle()
{
#ifndef CLIENT_DLL
	if ( m_iLaserActive && m_fSpotVisible )
		UpdateLaser();
#endif
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() || !m_iClip )
		return;
	int animation;
	float random = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	if ( m_iLaserActive )
	{
		if ( random > 0.5 )
		{
			animation = EAGLE_IDLE_LASER2;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
		}
		else
		{
			animation = EAGLE_IDLE_LASER1;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.5;
		}
	}
	else if ( random <= 0.3 )
	{
		animation = EAGLE_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.5;
	}
	else if ( random > 0.6 )
	{
		animation = EAGLE_IDLE3;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 49.0 / 30.0;
	}
	else
	{
		animation = EAGLE_IDLE2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.5;
	}
	SendWeaponAnim( animation, 1, 0 );
}

BOOL CEagle::UseDecrement()
{
#ifdef CLIENT_DLL
	return giOldWeapons != 1;
#else
	return oldweapons.value != 1.0f;
#endif
}

class CEagleAmmo : public CBasePlayerAmmo
{
public:
	void Spawn()
	{
		Precache();
		SET_MODEL( ENT( pev ), "models/w_9mmclip.mdl" );
		CBasePlayerAmmo::Spawn();
	}
	void Precache()
	{
		PRECACHE_MODEL( "models/w_9mmclip.mdl" );
		PRECACHE_SOUND( "items/9mmclip1.wav" );
	}
	BOOL AddAmmo( CBaseEntity* other )
	{
		if ( other->GiveAmmo( 7, "357", _357_MAX_CARRY ) == -1 )
			return FALSE;
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1.0, ATTN_NORM );
		return TRUE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_eagleclip, CEagleAmmo );
