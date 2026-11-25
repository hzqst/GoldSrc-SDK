#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"
#include "penguingrenade.h"

#ifdef CLIENT_DLL
extern int giOldWeapons;
#else
extern cvar_t oldweapons;
#endif

LINK_ENTITY_TO_CLASS( weapon_penguin, CPenguin );

enum penguin_e
{
	PENGUIN_IDLE,
	PENGUIN_FIDGET1,
	PENGUIN_FIDGET2,
	PENGUIN_HOLSTER,
	PENGUIN_DRAW,
	PENGUIN_THROW
};

void CPenguin::Spawn()
{
	Precache();
	m_iId = WEAPON_PENGUIN;
	SET_MODEL( ENT( pev ), "models/w_penguinnest.mdl" );
	FallInit();
	m_iDefaultAmmo = 3;
	pev->sequence = 1;
	pev->animtime = gpGlobals->time;
	pev->framerate = 1;
}

void CPenguin::Precache()
{
	PRECACHE_MODEL( "models/w_penguinnest.mdl" );
	PRECACHE_MODEL( "models/v_penguin.mdl" );
	PRECACHE_MODEL( "models/p_penguin.mdl" );
	PRECACHE_SOUND( "squeek/sqk_hunt2.wav" );
	PRECACHE_SOUND( "squeek/sqk_hunt3.wav" );
	UTIL_PrecacheOther( "monster_penguin" );
	m_usPenguinFire = PRECACHE_EVENT( 1, "events/penguinfire.sc" );
}

int CPenguin::GetItemInfo( ItemInfo* info )
{
	info->pszName = STRING( pev->classname );
	info->pszAmmo1 = "Penguins";
	info->iMaxAmmo1 = PENGUIN_MAX_CARRY;
	info->pszAmmo2 = NULL;
	info->iMaxAmmo2 = -1;
	info->iMaxClip = WEAPON_NOCLIP;
	info->iSlot = 4;
	info->iPosition = 4;
	info->iId = m_iId = WEAPON_PENGUIN;
	info->iWeight = 5;
	info->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	return 1;
}

BOOL CPenguin::Deploy()
{
	EMIT_SOUND( edict(), CHAN_VOICE, RANDOM_FLOAT( 0, 1 ) <= 0.5 ?
		"squeek/sqk_hunt2.wav" : "squeek/sqk_hunt3.wav", 1, ATTN_NORM );
	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	return DefaultDeploy( "models/v_penguin.mdl", "models/p_penguin.mdl", PENGUIN_DRAW, "penguin" );
}

void CPenguin::Holster( int )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) > 0 )
	{
		SendWeaponAnim( PENGUIN_HOLSTER );
		EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM );
	}
	else
	{
		m_pPlayer->pev->weapons &= ~( 1 << WEAPON_PENGUIN );
		SetThink( &CBasePlayerItem::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

BOOL CPenguin::UseDecrement()
{
#ifdef CLIENT_DLL
	return giOldWeapons != 1;
#else
	return oldweapons.value != 1;
#endif
}

void CPenguin::SecondaryAttack()
{
}

void CPenguin::PrimaryAttack()
{
	if ( m_pPlayer->AmmoInventory( m_iPrimaryAmmoType ) <= 0 )
		return;
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector source = m_pPlayer->pev->origin;
	if ( m_pPlayer->pev->flags & FL_DUCKING )
		source.z += 18;
	TraceResult trace;
	UTIL_TraceLine( source + gpGlobals->v_forward * 20, source + gpGlobals->v_forward * 64,
		dont_ignore_monsters, NULL, &trace );
	PLAYBACK_EVENT_FULL( UseDecrement() ? FEV_NOTHOST : 0, m_pPlayer->edict(), m_usPenguinFire, 0,
		(float*)&g_vecZero, (float*)&g_vecZero, 0, 0, 0, 0, 0, 0 );
	if ( trace.fAllSolid || trace.fStartSolid || trace.flFraction <= 0.25 )
		return;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
#ifndef CLIENT_DLL
	CBaseEntity* penguin = CBaseEntity::Create( "monster_penguin", trace.vecEndPos, m_pPlayer->pev->v_angle, m_pPlayer->edict() );
	penguin->pev->velocity = gpGlobals->v_forward * 200 + m_pPlayer->pev->velocity;
#endif
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, RANDOM_FLOAT( 0, 1 ) <= 0.5 ?
		"squeek/sqk_hunt2.wav" : "squeek/sqk_hunt3.wav", 1, ATTN_NORM, 0, 105 );
	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_pPlayer->AddAmmoInventory( m_iPrimaryAmmoType, -1 );
	m_fJustThrown = 1;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.9;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.8;
}

void CPenguin::WeaponIdle()
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;
	if ( m_fJustThrown )
	{
		m_fJustThrown = 0;
		if ( m_pPlayer->AmmoInventory( PrimaryAmmoIndex() ) <= 0 )
			RetireWeapon();
		else
		{
			SendWeaponAnim( PENGUIN_DRAW );
			float time = UTIL_WeaponTimeBase();
			m_flTimeWeaponIdle = time + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		}
		return;
	}
	float random = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	int animation;
	if ( random <= 0.75 )
	{
		animation = PENGUIN_IDLE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.75;
	}
	else if ( random <= 0.875 )
	{
		animation = PENGUIN_FIDGET1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4.375;
	}
	else
	{
		animation = PENGUIN_FIDGET2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5;
	}
	SendWeaponAnim( animation );
}

#ifndef CLIENT_DLL
LINK_ENTITY_TO_CLASS( monster_penguin, CPenguinGrenade );

float CPenguinGrenade::m_flNextBounceSoundTime;

TYPEDESCRIPTION CPenguinGrenade::m_SaveData[] =
{
	DEFINE_FIELD( CPenguinGrenade, m_flDie, FIELD_TIME ),
	DEFINE_FIELD( CPenguinGrenade, m_vecTarget, FIELD_VECTOR ),
	DEFINE_FIELD( CPenguinGrenade, m_flNextHunt, FIELD_TIME ),
	DEFINE_FIELD( CPenguinGrenade, m_flNextHit, FIELD_TIME ),
	DEFINE_FIELD( CPenguinGrenade, m_posPrev, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CPenguinGrenade, m_hOwner, FIELD_EHANDLE )
};
IMPLEMENT_SAVERESTORE( CPenguinGrenade, CGrenade );

int CPenguinGrenade::Classify()
{
	if ( m_iMyClass )
		return m_iMyClass;
	if ( m_hEnemy )
	{
		m_iMyClass = CLASS_INSECT;
		int classification = m_hEnemy->Classify();
		m_iMyClass = 0;
		if ( classification >= CLASS_PLAYER && classification <= CLASS_HUMAN_MILITARY )
			return CLASS_ALIEN_MILITARY;
	}
	return CLASS_ALIEN_BIOWEAPON;
}

int CPenguinGrenade::IRelationship( CBaseEntity* target )
{
	if ( target->Classify() == CLASS_ALIEN_MILITARY )
		return R_DL;
	if ( target->Classify() == CLASS_PLAYER_ALLY )
		return R_AL;
	return CBaseMonster::IRelationship( target );
}

void CPenguinGrenade::Precache()
{
	PRECACHE_MODEL( "models/w_penguin.mdl" );
	PRECACHE_SOUND( "squeek/sqk_blast1.wav" );
	PRECACHE_SOUND( "common/bodysplat.wav" );
	PRECACHE_SOUND( "squeek/sqk_die1.wav" );
	PRECACHE_SOUND( "squeek/sqk_hunt1.wav" );
	PRECACHE_SOUND( "squeek/sqk_hunt2.wav" );
	PRECACHE_SOUND( "squeek/sqk_hunt3.wav" );
	PRECACHE_SOUND( "squeek/sqk_deploy1.wav" );
}

void CPenguinGrenade::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	SET_MODEL( ENT( pev ), "models/w_penguin.mdl" );
	UTIL_SetSize( pev, Vector( -4, -4, 0 ), Vector( 4, 4, 8 ) );
	UTIL_SetOrigin( pev, pev->origin );
	SetTouch( &CPenguinGrenade::SuperBounceTouch );
	SetThink( &CPenguinGrenade::HuntThink );
	pev->nextthink = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time + 1000000;
	pev->flags |= FL_MONSTER;
	pev->takedamage = DAMAGE_AIM;
	pev->health = gSkillData.snarkHealth;
	pev->gravity = 0.5;
	pev->friction = 0.5;
	pev->dmg = gSkillData.plrDmgHandGrenade;
	m_flDie = gpGlobals->time + 15;
	m_flFieldOfView = 0;
	if ( pev->owner )
		m_hOwner = Instance( pev->owner );
	m_flNextBounceSoundTime = gpGlobals->time;
	pev->sequence = 3;
	ResetSequenceInfo();
}

void CPenguinGrenade::Killed( entvars_t*, int )
{
	if ( m_hOwner )
		pev->owner = m_hOwner->edict();
	CGrenade::Detonate();
	UTIL_BloodDrips( pev->origin, g_vecZero, BloodColor(), 80 );
	if ( m_hOwner )
		pev->owner = m_hOwner->edict();
}

void CPenguinGrenade::GibMonster()
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "common/bodysplat.wav", 0.75, ATTN_NORM, 0, 200 );
}

void CPenguinGrenade::HuntThink()
{
	if ( !IsInWorld() )
	{
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;
	if ( gpGlobals->time >= m_flDie )
	{
		g_vecAttackDir = pev->velocity.Normalize();
		pev->health = -1;
		Killed( pev, 0 );
		return;
	}
	if ( pev->waterlevel )
	{
		if ( pev->movetype == MOVETYPE_BOUNCE )
			pev->movetype = MOVETYPE_FLY;
		pev->velocity = pev->velocity * 0.9f;
		pev->velocity.z += 8;
	}
	else
	{
		pev->movetype = MOVETYPE_FLY;
		pev->movetype = MOVETYPE_BOUNCE;
	}
	if ( m_flNextHunt > gpGlobals->time )
		return;
	m_flNextHunt = gpGlobals->time + 2;
	Vector flat = pev->velocity;
	flat.z = 0;
	flat.Normalize();
	UTIL_MakeVectors( pev->angles );
	if ( !m_hEnemy || !m_hEnemy->IsAlive() )
	{
		Look( 512 );
		m_hEnemy = BestVisibleEnemy();
	}
	if ( m_flDie - gpGlobals->time <= 0.5 && m_flDie - gpGlobals->time >= 0.3 )
	{
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, "squeek/sqk_die1.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG( 0, 63 ) );
		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 256, 0.25 );
	}
	if ( m_hEnemy )
	{
		if ( FVisible( m_hEnemy ) )
			m_vecTarget = ( m_hEnemy->EyePosition() - pev->origin ).Normalize();
		float adjustment = 50 / ( pev->velocity.Length() + 10 );
		if ( adjustment > 1.2 )
			adjustment = 1.2f;
		pev->velocity = pev->velocity * adjustment + m_vecTarget * 300;
	}
	if ( pev->flags & FL_ONGROUND )
		pev->avelocity = g_vecZero;
	else if ( pev->avelocity == g_vecZero )
	{
		pev->avelocity.x = RANDOM_FLOAT( -100, 100 );
		pev->avelocity.z = RANDOM_FLOAT( -100, 100 );
	}
	if ( ( pev->origin - m_posPrev ).Length() < 1 )
	{
		pev->velocity.x = RANDOM_FLOAT( -100, 100 );
		pev->velocity.y = RANDOM_FLOAT( -100, 100 );
	}
	m_posPrev = pev->origin;
	pev->angles = UTIL_VecToAngles( pev->velocity );
	pev->angles.z = 0;
	pev->angles.x = 0;
}

void CPenguinGrenade::SuperBounceTouch( CBaseEntity* other )
{
	TraceResult trace = UTIL_GetGlobalTrace();
	if ( pev->owner && other->edict() == pev->owner )
		return;
	pev->owner = NULL;
	pev->angles.x = 0;
	pev->angles.z = 0;
	if ( m_flNextHit > gpGlobals->time )
		return;
	float pitch = 155 - ( m_flDie - gpGlobals->time ) / 15.0 * 60;
	if ( other->pev->takedamage && gpGlobals->time > m_flNextAttack )
	{
		BOOL fullDamage = TRUE;
		if ( g_pGameRules->IsMultiplayer() )
		{
			CBaseEntity* owner = CBaseEntity::Instance( pev->owner );
			CBasePlayer* player = owner->IsPlayer() ? (CBasePlayer*)owner : NULL;
			if ( other->IsPlayer() && player )
				fullDamage = player != other && g_pGameRules->FPlayerCanTakeDamage( (CBasePlayer*)other, player );
		}
		if ( trace.pHit == other->edict() && trace.pHit->v.modelindex != pev->modelindex )
		{
			ClearMultiDamage();
			other->TraceAttack( pev, gSkillData.snarkDmgBite, gpGlobals->v_forward, &trace, DMG_SLASH );
			ApplyMultiDamage( pev, m_hOwner ? m_hOwner->pev : pev );
			pev->dmg += fullDamage ? gSkillData.plrDmgHandGrenade : gSkillData.plrDmgHandGrenade / 5;
			if ( pev->dmg > 500 )
				pev->dmg = 500;
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "squeek/sqk_deploy1.wav", 1, ATTN_NORM, 0, (int)pitch );
			m_flNextAttack = gpGlobals->time + 0.5;
		}
	}
	m_flNextHit = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time;
	if ( g_pGameRules->IsMultiplayer() && gpGlobals->time < m_flNextBounceSoundTime )
		return;
	if ( pev->flags & FL_ONGROUND )
	{
		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 100, 0.1f );
	}
	else
	{
		float random = RANDOM_FLOAT( 0, 1 );
		const char* sound = random <= 0.33 ? "squeek/sqk_hunt1.wav" :
			random <= 0.66 ? "squeek/sqk_hunt2.wav" : "squeek/sqk_hunt3.wav";
		EMIT_SOUND_DYN( edict(), CHAN_VOICE, sound, 1, ATTN_NORM, 0, (int)pitch );
		CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 256, 0.25 );
	}
	m_flNextBounceSoundTime = gpGlobals->time + 0.5;
}

void CPenguinGrenade::Smoke()
{
	if ( UTIL_PointContents( pev->origin ) == CONTENTS_WATER )
	{
		UTIL_Bubbles( pev->origin - Vector( 64, 64, 64 ), pev->origin + Vector( 64, 64, 64 ), 100 );
	}
	else
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( (int)( ( pev->dmg - 50 ) * 0.8 ) );
			WRITE_BYTE( 12 );
		MESSAGE_END();
	}
	UTIL_Remove( this );
}
#endif
