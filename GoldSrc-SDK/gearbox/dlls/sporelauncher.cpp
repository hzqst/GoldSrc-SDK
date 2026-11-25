#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "sporelauncher.h"
#include "ofctfitems.h"
#ifndef CLIENT_DLL
#include "op4weap.h"
#endif

#ifdef CLIENT_DLL
extern int giOldWeapons;
#else
extern cvar_t oldweapons;
extern int gEvilImpulse101;
#endif

LINK_ENTITY_TO_CLASS( weapon_sporelauncher, CSporeLauncher );

enum sporelauncher_e
{
	SPORELAUNCHER_IDLE,
	SPORELAUNCHER_FIDGET,
	SPORELAUNCHER_RELOAD_START,
	SPORELAUNCHER_RELOAD,
	SPORELAUNCHER_RELOAD_END,
	SPORELAUNCHER_FIRE,
	SPORELAUNCHER_HOLSTER,
	SPORELAUNCHER_DRAW,
	SPORELAUNCHER_IDLE2
};

#ifndef CLIENT_DLL
TYPEDESCRIPTION CSporeLauncher::m_SaveData[] =
{
	DEFINE_FIELD( CSporeLauncher, m_cActiveRockets, FIELD_INTEGER )
};
IMPLEMENT_SAVERESTORE( CSporeLauncher, CBasePlayerWeapon );
#endif

void CSporeLauncher::Precache()
{
	PRECACHE_MODEL( "models/w_spore_launcher.mdl" );
	PRECACHE_MODEL( "models/v_spore_launcher.mdl" );
	PRECACHE_MODEL( "models/p_spore_launcher.mdl" );
	PRECACHE_SOUND( "weapons/splauncher_fire.wav" );
	PRECACHE_SOUND( "weapons/splauncher_altfire.wav" );
	PRECACHE_SOUND( "weapons/splauncher_bounce.wav" );
	PRECACHE_SOUND( "weapons/splauncher_reload.wav" );
	PRECACHE_SOUND( "weapons/splauncher_pet.wav" );
	UTIL_PrecacheOther( "spore" );
	m_usFireSpore = PRECACHE_EVENT( 1, "events/spore.sc" );
}

void CSporeLauncher::Spawn()
{
	Precache();
	m_iId = WEAPON_SPORELAUNCHER;
	SET_MODEL( ENT( pev ), "models/w_spore_launcher.mdl" );
	m_iDefaultAmmo = 5;
	FallInit();
	pev->sequence = 0;
	pev->animtime = gpGlobals->time;
	pev->framerate = 1;
}

int CSporeLauncher::GetItemInfo( ItemInfo* info )
{
	info->pszName = STRING( pev->classname );
	info->pszAmmo1 = "spores";
	info->iMaxAmmo1 = SPORE_MAX_CARRY;
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = SPORELAUNCHER_MAX_CLIP;
	info->iSlot = 6;
	info->iPosition = 0;
	info->iId = m_iId = WEAPON_SPORELAUNCHER;
	info->iFlags = 0;
	info->iWeight = 20;
	return 1;
}

int CSporeLauncher::AddToPlayer( CBasePlayer* player )
{
	if ( !CBasePlayerWeapon::AddToPlayer( player ) )
		return FALSE;
	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, player->pev );
		WRITE_BYTE( m_iId );
	MESSAGE_END();
	return TRUE;
}

BOOL CSporeLauncher::Deploy()
{
	return DefaultDeploy( "models/v_spore_launcher.mdl", "models/p_spore_launcher.mdl", SPORELAUNCHER_DRAW, "rpg" );
}

void CSporeLauncher::Holster( int )
{
	m_fInSpecialReload = 0;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( SPORELAUNCHER_HOLSTER );
}

BOOL CSporeLauncher::UseDecrement()
{
#ifdef CLIENT_DLL
	return giOldWeapons != 1;
#else
	return oldweapons.value != 1;
#endif
}

void CSporeLauncher::PrimaryAttack()
{
	if ( m_iClip )
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
#ifndef CLIENT_DLL
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector source = gpGlobals->v_forward * 16 + m_pPlayer->GetGunPosition() + gpGlobals->v_right * 8 - gpGlobals->v_up * 8;
		Vector angles = m_pPlayer->GetAutoaimVectorFromPoint( source, AUTOAIM_10DEGREES );
		CSpore* spore = CSpore::CreateSpore( source, angles, m_pPlayer, 1, FALSE, FALSE );
		UTIL_MakeVectors( angles );
		spore->pev->velocity = spore->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );
#endif
		PLAYBACK_EVENT_FULL( UseDecrement() ? FEV_NOTHOST : 0, m_pPlayer->edict(), m_usFireSpore, 0,
			(float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, 0, 0, 0 );
		m_iClip--;
	}
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
	m_fInSpecialReload = 0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
}

void CSporeLauncher::SecondaryAttack()
{
	if ( m_iClip )
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
#ifndef CLIENT_DLL
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector source = gpGlobals->v_forward * 16 + m_pPlayer->GetGunPosition() + gpGlobals->v_right * 8 - gpGlobals->v_up * 8;
		Vector angles = m_pPlayer->GetAutoaimVectorFromPoint( source, AUTOAIM_10DEGREES );
		CSpore* spore = CSpore::CreateSpore( source, angles, m_pPlayer, 2, FALSE, FALSE );
		UTIL_MakeVectors( angles );
		spore->pev->velocity = gpGlobals->v_forward * 800 + m_pPlayer->pev->velocity;
#endif
		PLAYBACK_EVENT_FULL( UseDecrement() ? FEV_NOTHOST : 0, m_pPlayer->edict(), m_usFireSpore, 0,
			(float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, 0, 0, 0 );
		m_iClip--;
	}
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
	m_fInSpecialReload = 0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
}

void CSporeLauncher::Reload()
{
	int maximum = SPORELAUNCHER_MAX_CLIP * ( m_pPlayer->m_iItems & CTF_ITEM_BACKPACK ? 2 : 1 );
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 || m_iClip == maximum || m_flNextPrimaryAttack > UTIL_WeaponTimeBase() )
		return;
	if ( m_fInSpecialReload == 0 )
	{
		SendWeaponAnim( SPORELAUNCHER_RELOAD_START );
		m_fInSpecialReload = 1;
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.66;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.66;
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.66;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.66;
	}
	else if ( m_fInSpecialReload == 1 )
	{
		if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
			return;
		m_fInSpecialReload = 2;
		EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/splauncher_reload.wav", 0.7f, ATTN_NORM );
		SendWeaponAnim( SPORELAUNCHER_RELOAD );
		m_flNextReload = UTIL_WeaponTimeBase() + 1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0f;
	}
	else
	{
		m_iClip++;
		m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -1 );
		m_fInSpecialReload = 1;
	}
}

void CSporeLauncher::WeaponIdle()
{
	int maximum = SPORELAUNCHER_MAX_CLIP * ( m_pPlayer->m_iItems & CTF_ITEM_BACKPACK ? 2 : 1 );
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( UTIL_WeaponTimeBase() <= m_flTimeWeaponIdle )
		return;
	if ( m_fInSpecialReload )
	{
		if ( m_iClip < maximum && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
			Reload();
		else
		{
			SendWeaponAnim( SPORELAUNCHER_RELOAD_END );
			m_fInSpecialReload = 0;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.83;
		}
		return;
	}
	if ( !m_iClip && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
	{
		Reload();
		return;
	}
	float random = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	int animation;
	if ( random <= 0.75 )
	{
		animation = SPORELAUNCHER_IDLE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2;
	}
	else if ( random > 0.95 )
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4;
		EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/splauncher_pet.wav", 0.7f, ATTN_NORM );
		animation = SPORELAUNCHER_FIDGET;
	}
	else
	{
		animation = SPORELAUNCHER_IDLE2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4;
	}
	SendWeaponAnim( animation );
}

void CSporeLauncher::IncrementAmmo( CBasePlayer* player )
{
	if ( player->GiveAmmo( 1, "spores", SPORE_MAX_CARRY ) >= 0 )
		EMIT_SOUND( player->edict(), CHAN_STATIC, "ctf/pow_backpack.wav", 0.5, ATTN_NORM );
}

#ifndef CLIENT_DLL
LINK_ENTITY_TO_CLASS( ammo_spore, CSporeAmmo );

TYPEDESCRIPTION CSporeAmmo::m_SaveData[] =
{
	DEFINE_FIELD( CSporeAmmo, m_flRegrowTime, FIELD_TIME ),
	DEFINE_FIELD( CSporeAmmo, m_flAnimTime, FIELD_TIME )
};
IMPLEMENT_SAVERESTORE( CSporeAmmo, CBasePlayerAmmo );

void CSporeAmmo::Precache()
{
	PRECACHE_MODEL( "models/spore_ammo.mdl" );
	PRECACHE_SOUND( "weapons/spore_ammo.wav" );
}

void CSporeAmmo::Spawn()
{
	Precache();
	SET_MODEL( ENT( pev ), "models/spore_ammo.mdl" );
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize( pev, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ) );
	UTIL_SetOrigin( pev, pev->origin );
	SetTouch( &CBasePlayerAmmo::DefaultTouch );
	pev->angles.x -= 90;
	pev->origin.z += 16;
	pev->sequence = 3;
	pev->animtime = gpGlobals->time;
	pev->nextthink = gpGlobals->time + 4;
	pev->frame = 0;
	pev->framerate = 1;
	pev->health = 1;
	pev->body = 1;
	pev->takedamage = DAMAGE_AIM;
	pev->solid = SOLID_BBOX;
	SetTouch( &CSporeAmmo::SporeTouch );
	SetThink( &CSporeAmmo::Idling );
}

BOOL CSporeAmmo::AddAmmo( CBaseEntity* other )
{
	if ( other->GiveAmmo( 1, "spores", SPORE_MAX_CARRY ) == -1 )
		return FALSE;
	EMIT_SOUND( edict(), CHAN_ITEM, "weapons/spore_ammo.wav", 1, ATTN_NORM );
	return TRUE;
}

void CSporeAmmo::SporeTouch( CBaseEntity* other )
{
	if ( !other->IsPlayer() || !pev->body )
		return;
	if ( AddAmmo( other ) )
	{
		pev->body = 0;
		pev->sequence = 4;
		pev->animtime = gpGlobals->time;
		pev->frame = 0;
		pev->nextthink = gpGlobals->time + 0.66;
	}
	else if ( gEvilImpulse101 )
	{
		SetTouch( NULL );
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CSporeAmmo::Idling()
{
	switch ( pev->sequence )
	{
	case 3:
		pev->sequence = 5;
		pev->animtime = gpGlobals->time;
		pev->frame = 0;
		break;
	case 4:
		pev->sequence = 0;
		pev->animtime = gpGlobals->time;
		pev->frame = 0;
		pev->nextthink = gpGlobals->time + 10;
		break;
	case 0:
		pev->body = 1;
		pev->sequence = 3;
		pev->animtime = gpGlobals->time;
		pev->frame = 0;
		pev->nextthink = gpGlobals->time + 4;
		break;
	}
}

int CSporeAmmo::TakeDamage( entvars_t*, entvars_t*, float, int )
{
	if ( !pev->body )
		return 0;
	pev->body = 0;
	pev->sequence = 4;
	pev->animtime = gpGlobals->time;
	pev->frame = 0;
	pev->nextthink = gpGlobals->time + 0.66;
	Vector angles = pev->angles;
	angles.x -= 90;
	angles.x += RANDOM_FLOAT( -20, 20 );
	angles.y += RANDOM_FLOAT( -20, 20 );
	angles.z += RANDOM_FLOAT( -20, 20 );
	CSpore* spore = CSpore::CreateSpore( pev->origin, angles, this, 2, FALSE, TRUE );
	UTIL_MakeVectors( angles );
	spore->pev->velocity = gpGlobals->v_forward * 800;
	return 0;
}
#endif
