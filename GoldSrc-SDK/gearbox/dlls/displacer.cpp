#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "displacer.h"

#ifdef CLIENT_DLL
extern int giOldWeapons;
#else
#include "op4weap.h"
#include "ofctf.h"
#include "rope.h"
extern cvar_t oldweapons;
extern edict_t* EntSelectSpawnPoint( CBaseEntity* player );
#endif

#define DISPLACER_PRIMARY_AMMO	20
#define DISPLACER_SECONDARY_AMMO	60

enum
{
	DISPLACER_IDLE1 = 0,
	DISPLACER_IDLE2,
	DISPLACER_SPINUP,
	DISPLACER_FIRE = 4,
	DISPLACER_DRAW,
	DISPLACER_HOLSTER
};

LINK_ENTITY_TO_CLASS( weapon_displacer, CDisplacer );
#ifndef CLIENT_DLL
LINK_ENTITY_TO_CLASS( info_displacer_xen_target, CPointEntity );
LINK_ENTITY_TO_CLASS( info_displacer_earth_target, CPointEntity );
#endif

void CDisplacer::Precache()
{
	PRECACHE_MODEL( "models/v_displacer.mdl" );
	PRECACHE_MODEL( "models/w_displacer.mdl" );
	PRECACHE_MODEL( "models/p_displacer.mdl" );
	PRECACHE_SOUND( "weapons/displacer_fire.wav" );
	PRECACHE_SOUND( "weapons/displacer_self.wav" );
	PRECACHE_SOUND( "weapons/displacer_spin.wav" );
	PRECACHE_SOUND( "weapons/displacer_spin2.wav" );
	PRECACHE_SOUND( "buttons/button11.wav" );
	m_iSpriteTexture = PRECACHE_MODEL( "sprites/shockwave.spr" );
	UTIL_PrecacheOther( "displacer_ball" );
}

void CDisplacer::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_displacer" );
	Precache();
	m_iId = WEAPON_DISPLACER;
	SET_MODEL( ENT( pev ), "models/w_displacer.mdl" );
	m_iDefaultAmmo = 40;
	FallInit();
}

int CDisplacer::GetItemInfo( ItemInfo* info )
{
	info->pszName = STRING( pev->classname );
	info->pszAmmo1 = "uranium";
	info->iMaxAmmo1 = URANIUM_MAX_CARRY;
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = WEAPON_NOCLIP;
	info->iFlags = 0;
	info->iSlot = 5;
	info->iPosition = 1;
	info->iId = m_iId = WEAPON_DISPLACER;
	info->iWeight = 10;
	return 1;
}

int CDisplacer::AddToPlayer( CBasePlayer* player )
{
	if ( !CBasePlayerWeapon::AddToPlayer( player ) )
		return FALSE;
	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, player->pev );
		WRITE_BYTE( m_iId );
	MESSAGE_END();
	return TRUE;
}

void CDisplacer::IncrementAmmo( CBasePlayer* player )
{
	if ( player->GiveAmmo( 1, "uranium", URANIUM_MAX_CARRY ) >= 0 )
		EMIT_SOUND( ENT( player->pev ), CHAN_STATIC, "ctf/pow_backpack.wav", 0.5, ATTN_NORM );
}

BOOL CDisplacer::Deploy()
{
	return DefaultDeploy( "models/v_displacer.mdl", "models/p_displacer.mdl", DISPLACER_DRAW, "egon", 0 );
}

void CDisplacer::Holster( int skiplocal )
{
	m_fInReload = FALSE;
	DestroyChargeEffect();
	STOP_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/displacer_spin.wav" );
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	float time = UTIL_WeaponTimeBase();
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 5 ) + time;
	SendWeaponAnim( DISPLACER_HOLSTER, 1, 0 );
	if ( m_pfnThink == (void ( CBaseEntity::* )())&CDisplacer::SpinupThink )
	{
		m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -(DISPLACER_PRIMARY_AMMO) );
		SetThink( NULL );
	}
	else if ( m_pfnThink == (void ( CBaseEntity::* )())&CDisplacer::AltSpinupThink )
	{
		m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -(DISPLACER_SECONDARY_AMMO) );
		SetThink( NULL );
	}
}

void CDisplacer::PrimaryAttack()
{
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) < DISPLACER_PRIMARY_AMMO )
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "buttons/button11.wav", RANDOM_FLOAT( 0.8, 0.9 ), ATTN_NORM );
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		return;
	}
	SetThink( &CDisplacer::SpinupThink );
	pev->nextthink = gpGlobals->time;
	m_bMode = 0;
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/displacer_spin.wav", RANDOM_FLOAT( 0.8, 0.9 ), ATTN_NORM );
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 2.5;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.5;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.5;
}

void CDisplacer::SecondaryAttack()
{
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) < DISPLACER_SECONDARY_AMMO )
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "buttons/button11.wav", RANDOM_FLOAT( 0.8, 0.9 ), ATTN_NORM );
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		return;
	}
	SetThink( &CDisplacer::AltSpinupThink );
	pev->nextthink = gpGlobals->time;
	m_bMode = 0;
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/displacer_spin2.wav", RANDOM_FLOAT( 0.8, 0.9 ), ATTN_NORM );
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.5;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.5;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
}

void CDisplacer::SpinupThink()
{
	if ( m_bMode == 0 )
	{
		SendWeaponAnim( DISPLACER_SPINUP, 1, 0 );
		m_bMode = 1;
		CreateChargeEffect();
		m_flStartTime = gpGlobals->time;
		m_iSoundState = 0;
	}
	if ( m_bMode == 1 )
	{
		if ( gpGlobals->time > m_flStartTime + 0.9 )
		{
			m_bMode = 3;
			SetThink( &CDisplacer::FireThink );
			pev->nextthink = gpGlobals->time + 0.1;
		}
		if ( m_bMode == 1 )
		{
			float counter = ( gpGlobals->time - m_flStartTime ) * 100.0 + 50.0;
			m_iImplodeCounter = (int)counter;
		}
	}
	if ( m_iImplodeCounter > 250 )
		m_iImplodeCounter = 250;
	m_iSoundState = SND_CHANGE_PITCH;
	pev->nextthink = gpGlobals->time + 0.1;
}

void CDisplacer::AltSpinupThink()
{
	if ( m_bMode == 0 )
	{
		SendWeaponAnim( DISPLACER_SPINUP, 1, 0 );
		m_bMode = 1;
		CreateChargeEffect();
		m_flStartTime = gpGlobals->time;
		m_iSoundState = 0;
	}
	if ( m_bMode == 1 )
	{
		if ( gpGlobals->time > m_flStartTime + 0.9 )
		{
			m_bMode = 3;
			SetThink( &CDisplacer::AltFireThink );
			pev->nextthink = gpGlobals->time + 0.1;
		}
		if ( m_bMode == 1 )
		{
			float counter = ( gpGlobals->time - m_flStartTime ) * 100.0 + 50.0;
			m_iImplodeCounter = (int)counter;
		}
	}
	if ( m_iImplodeCounter > 250 )
		m_iImplodeCounter = 250;
	m_iSoundState = SND_CHANGE_PITCH;
	pev->nextthink = gpGlobals->time + 0.1;
}

void CDisplacer::FireThink()
{
	m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -(DISPLACER_PRIMARY_AMMO) );
	DestroyChargeEffect();
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	SendWeaponAnim( DISPLACER_FIRE, 1, 0 );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/displacer_fire.wav", RANDOM_FLOAT( 0.8, 0.9 ), ATTN_NORM );
#ifndef CLIENT_DLL
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector source = m_pPlayer->GetGunPosition() - gpGlobals->v_right * 2;
	Vector aiming = m_pPlayer->GetAutoaimVectorFromPoint( source, AUTOAIM_10DEGREES );
	CDisplacerBall::CreateDisplacerBall( source, aiming, m_pPlayer );
	if ( !m_iClip && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
	m_pPlayer->pev->punchangle.x -= 2.0;
#endif
	SetThink( NULL );
}

void CDisplacer::AltFireThink()
{
	DestroyChargeEffect();
#ifndef CLIENT_DLL
	if ( m_pPlayer->isOnRope() )
	{
		m_pPlayer->pev->movetype = MOVETYPE_WALK;
		m_pPlayer->pev->solid = SOLID_SLIDEBOX;
		m_pPlayer->setOnRopeState( false );
		m_pPlayer->mRope->detachObject();
		m_pPlayer->setRope( NULL );
		m_pPlayer->mIsClimbing = 0;
	}
#endif
	STOP_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/displacer_spin.wav" );
	Vector previousOrigin = m_pPlayer->pev->origin;
	m_pPlayer->m_DisplacerReturn = previousOrigin;
	m_pPlayer->m_flDisplacerSndRoomtype = m_pPlayer->m_flSndRoomtype;
#ifndef CLIENT_DLL
	if ( g_pGameRules->IsCTF() && m_pPlayer->m_pFlag != NULL )
		( (CTFGoalFlag*)(CBaseEntity*)m_pPlayer->m_pFlag )->DropFlag( m_pPlayer );
	edict_t* target;
	if ( !g_pGameRules->IsMultiplayer() || g_pGameRules->IsCoOp() )
	{
		target = FIND_ENTITY_BY_CLASSNAME( NULL, "info_displacer_xen_target" );
	}
	else
	{
		target = EntSelectSpawnPoint( m_pPlayer );
		Vector end = VARS( target )->origin;
		end.z -= 100;
		TraceResult trace;
		UTIL_TraceLine( VARS( target )->origin, end, ignore_monsters, edict(), &trace );
		m_pPlayer->pev->origin = trace.vecEndPos;
		m_pPlayer->pev->origin.z += 36.0;
		m_pPlayer->pev->origin.z += 1.0;
	}
	if ( !target || !OFFSET( target ) )
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "buttons/button11.wav", RANDOM_FLOAT( 0.8, 0.9 ), ATTN_NORM );
		return;
	}
	m_pPlayer->pev->flags &= ~FL_ONGROUND;
	m_pPlayer->pev->origin = VARS( target )->origin;
	if ( !g_pGameRules->IsMultiplayer() || g_pGameRules->IsCoOp() )
	{
		m_pPlayer->pev->origin.z += 36.0;
		m_pPlayer->pev->origin.z += 1.0;
	}
	UTIL_SetOrigin( m_pPlayer->pev, m_pPlayer->pev->origin );
	m_pPlayer->pev->angles = VARS( target )->angles;
	m_pPlayer->pev->v_angle = VARS( target )->angles;
	m_pPlayer->pev->fixangle = 1;
	m_pPlayer->pev->basevelocity = g_vecZero;
	m_pPlayer->pev->velocity = m_pPlayer->pev->basevelocity;
#endif
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.0;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 2.0;
	SetThink( NULL );
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/displacer_self.wav", RANDOM_FLOAT( 0.8, 0.9 ), ATTN_NORM );
	m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -(DISPLACER_SECONDARY_AMMO) );
#ifndef CLIENT_DLL
	CDisplacerBall::CreateDisplacerBall( previousOrigin, Vector( 90, 0, 0 ), m_pPlayer );
	if ( !m_iClip && m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );
#endif
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
#ifndef CLIENT_DLL
	if ( !g_pGameRules->IsMultiplayer() )
		m_pPlayer->pev->gravity = 0.6f;
#endif
}

void CDisplacer::CreateChargeEffect()
{
#ifndef CLIENT_DLL
	for ( int i = 0; i < 4; i++ )
	{
		m_pNoseBeam[i] = CBeam::BeamCreate( "sprites/lgtning.spr", 16 );
		m_pNoseBeam[i]->EntsInit( m_pPlayer->entindex(), m_pPlayer->entindex() );
		m_pNoseBeam[i]->SetNoise( 60 );
		m_pNoseBeam[i]->SetWidth( 10 );
		m_pNoseBeam[i]->SetColor( 96, 128, 16 );
		m_pNoseBeam[i]->SetScrollRate( 30 );
		m_pNoseBeam[i]->SetBrightness( 190 );
	}
	m_pNoseBeam[0]->SetStartAttachment( 2 );
	m_pNoseBeam[0]->SetEndAttachment( 3 );
	m_pNoseBeam[1]->SetStartAttachment( 3 );
	m_pNoseBeam[1]->SetEndAttachment( 4 );
	m_pNoseBeam[2]->SetStartAttachment( 4 );
	m_pNoseBeam[2]->SetEndAttachment( 2 );
#endif
}

void CDisplacer::DestroyChargeEffect()
{
#ifndef CLIENT_DLL
	for ( int i = 0; i < 4; i++ )
	{
		if ( m_pNoseBeam[i] )
		{
			UTIL_Remove( m_pNoseBeam[i] );
			m_pNoseBeam[i] = NULL;
		}
	}
#endif
}

void CDisplacer::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	if ( m_flSoundDelay && gpGlobals->time >= m_flSoundDelay )
		m_flSoundDelay = 0;
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;
	int animation = RANDOM_FLOAT( 0, 1 ) <= 0.5 ? DISPLACER_IDLE1 : DISPLACER_IDLE2;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.0;
	SendWeaponAnim( animation, 1, 0 );
}

void CDisplacer::Reload()
{
}

void CDisplacer::Attack()
{
}

void CDisplacer::InXenThink()
{
}

void CDisplacer::ReturnThink()
{
}

BOOL CDisplacer::UseDecrement()
{
#ifdef CLIENT_DLL
	return giOldWeapons != 1;
#else
	return oldweapons.value != 1.0f;
#endif
}
