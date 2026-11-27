/*** 
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== weapons.cpp ========================================================

  functions governing the selection/use of weapons for players

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"
#include "game.h"
#include "bot/bot.h"
#include "bot/cs_bot_manager.h"
#include "hltv.h"

extern int gEvilImpulse101;
extern int gmsgCurWeapon;
extern int gmsgWeapPickup;
extern int gmsgBrass;
extern int gmsgReloadSound;
extern int gmsgBombPickup;
extern int gmsgBombDrop;

short g_sModelIndexLaser;
const char* g_pModelNameLaser = "sprites/laserbeam.spr";
short g_sModelIndexLaserDot;
short g_sModelIndexFireball;
short g_sModelIndexSmoke;
short g_sModelIndexWExplosion;
short g_sModelIndexBubbles;
short g_sModelIndexBloodDrop;
short g_sModelIndexBloodSpray;
short g_sModelIndexSmokePuff;
short g_sModelIndexFireball2;
short g_sModelIndexFireball3;
short g_sModelIndexFireball4;
short g_sModelIndexRadio;
short g_sModelIndexCTGhost;
short g_sModelIndexTGhost;
short g_sModelIndexC4Glow;
int giAmmoIndex;

ItemInfo CBasePlayerItem::ItemInfoArray[MAX_WEAPONS];
AmmoInfo CBasePlayerItem::AmmoInfoArray[MAX_AMMO_SLOTS];
MULTIDAMAGE gMultiDamage;

TYPEDESCRIPTION CBasePlayerItem::m_SaveData[] =
{
	DEFINE_FIELD( CBasePlayerItem, m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayerItem, m_pNext, FIELD_CLASSPTR ),
	//DEFINE_FIELD( CBasePlayerItem, m_fKnown, FIELD_INTEGER ),Reset to zero on load
	DEFINE_FIELD( CBasePlayerItem, m_iId, FIELD_INTEGER )
	// DEFINE_FIELD( CBasePlayerItem, m_iIdPrimary, FIELD_INTEGER ),
	// DEFINE_FIELD( CBasePlayerItem, m_iIdSecondary, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBasePlayerItem, CBaseAnimating );

TYPEDESCRIPTION CBasePlayerWeapon::m_SaveData[] =
{
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextSecondaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iPrimaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iSecondaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iClip, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iDefaultAmmo, FIELD_INTEGER )
};

IMPLEMENT_SAVERESTORE( CBasePlayerWeapon, CBasePlayerItem );

TYPEDESCRIPTION CWeaponBox::m_SaveData[] =
{
	DEFINE_ARRAY( CWeaponBox, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgiszAmmo, FIELD_STRING, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( CWeaponBox, m_cAmmoTypes, FIELD_INTEGER )
};

IMPLEMENT_SAVERESTORE( CWeaponBox, CBaseEntity );

//=========================================================
// MaxAmmoCarry - pass in a name and this function will tell
// you the maximum amount of that type of ammunition that a 
// player can carry.
//=========================================================
int MaxAmmoCarry( int iszName )
{
	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo1
			&& !strcmp( STRING( iszName ), CBasePlayerItem::ItemInfoArray[i].pszAmmo1 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo1;
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo2
			&& !strcmp( STRING( iszName ), CBasePlayerItem::ItemInfoArray[i].pszAmmo2 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo2;
	}
	ALERT( at_console, "MaxAmmoCarry() doesn't recognize '%s'!\n", STRING( iszName ) );
	return -1;
}


/*
==============================================================================

MULTI-DAMAGE

Collects multiple small damages into a single damage

==============================================================================
*/

//
// ClearMultiDamage - resets the global multi damage accumulator
//
void ClearMultiDamage()
{
	gMultiDamage.pEntity = NULL;
	gMultiDamage.amount = 0;
	gMultiDamage.type = 0;
}

void ApplyMultiDamage( entvars_t* pevInflictor, entvars_t* pevAttacker )
{
	if ( gMultiDamage.pEntity )
		gMultiDamage.pEntity->TakeDamage( pevInflictor, pevAttacker, gMultiDamage.amount, gMultiDamage.type );
}


// GLOBALS USED:
//		gMultiDamage

void AddMultiDamage( entvars_t* pevInflictor, CBaseEntity* pEntity, float flDamage, int bitsDamageType )
{
	if ( !pEntity )
		return;
	gMultiDamage.type |= bitsDamageType;
	if ( pEntity != gMultiDamage.pEntity )
	{
		ApplyMultiDamage( pevInflictor, pevInflictor ); // UNDONE: wrong attacker!
		gMultiDamage.pEntity = pEntity;
		gMultiDamage.amount = 0;
	}
	gMultiDamage.amount += flDamage;
}

/*
================
SpawnBlood
================
*/
void SpawnBlood( Vector vecSpot, int bloodColor, float flDamage )
{
	UTIL_BloodDrips( vecSpot, g_vecAttackDir, bloodColor, (int)flDamage );
}

int DamageDecal( CBaseEntity* pEntity, int bitsDamageType )
{
	if ( pEntity )
		return pEntity->DamageDecal( bitsDamageType );
	return RANDOM_LONG( DECAL_GUNSHOT4, DECAL_GUNSHOT5 );
}

void DecalGunshot( TraceResult*, int, bool, entvars_t*, bool )
{
}

void EjectBrass( const Vector& vecOrigin, const Vector& vecLeft, const Vector& vecVelocity,
	float rotation, int model, int soundtype, int entityIndex )
{
	UTIL_PlayerByIndex( entityIndex );
	bool bIsCZero = UTIL_IsGame( "czero" );
	MESSAGE_BEGIN( MSG_PVS, gmsgBrass, vecOrigin );
	if ( !bIsCZero )
		WRITE_BYTE( TE_MODEL );
	WRITE_COORD( vecOrigin.x );
	WRITE_COORD( vecOrigin.y );
	WRITE_COORD( vecOrigin.z );
	if ( !bIsCZero )
	{
		WRITE_COORD( vecLeft.x );
		WRITE_COORD( vecLeft.y );
		WRITE_COORD( vecLeft.z );
	}
	WRITE_COORD( vecVelocity.x );
	WRITE_COORD( vecVelocity.y );
	WRITE_COORD( vecVelocity.z );
	WRITE_ANGLE( rotation );
	WRITE_SHORT( model );
	WRITE_BYTE( soundtype );
	if ( !bIsCZero )
		WRITE_BYTE( 25 );
	WRITE_BYTE( entityIndex );
	MESSAGE_END();
}

void EjectBrass2( const Vector& vecOrigin, const Vector& vecVelocity, float rotation,
	int model, int, entvars_t* pev )
{
	MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, ENT( pev ) );
	WRITE_BYTE( TE_MODEL );
	WRITE_COORD( vecOrigin.x );
	WRITE_COORD( vecOrigin.y );
	WRITE_COORD( vecOrigin.z );
	WRITE_COORD( vecVelocity.x );
	WRITE_COORD( vecVelocity.y );
	WRITE_COORD( vecVelocity.z );
	WRITE_ANGLE( rotation );
	WRITE_SHORT( model );
	WRITE_BYTE( 0 );
	WRITE_BYTE( 5 );
	MESSAGE_END();
}

// Precaches the ammo and queues the ammo info for sending to clients
void AddAmmoNameToAmmoRegistry( const char* szAmmoname )
{
	// make sure it's not already in the registry
	for ( int i = 0; i < MAX_AMMO_SLOTS; ++i )
	{
		if ( CBasePlayerItem::AmmoInfoArray[i].pszName
			&& !stricmp( CBasePlayerItem::AmmoInfoArray[i].pszName, szAmmoname ) )
			return;
	}
	if ( giAmmoIndex + 1 >= MAX_AMMO_SLOTS )
	{
		ALERT( at_error, "Too many ammo types (max %d), ignoring %s\n", MAX_AMMO_SLOTS - 1, szAmmoname );
		return;
	}
	giAmmoIndex++;
	CBasePlayerItem::AmmoInfoArray[giAmmoIndex].pszName = szAmmoname;
	CBasePlayerItem::AmmoInfoArray[giAmmoIndex].iId = giAmmoIndex;   // yes, this info is redundant
}


// Precaches the weapon and queues the weapon info for sending to clients
void UTIL_PrecacheOtherWeapon2( const char* szClassname )
{
	edict_t* pent = CREATE_NAMED_ENTITY( MAKE_STRING( szClassname ) );
	if ( FNullEnt( pent ) )
	{
		ALERT( at_console, "NULL Ent in UTIL_PrecacheOtherWeapon\n" );
		return;
	}
	CBaseEntity* pEntity = CBaseEntity::Instance( VARS( pent ) );
	if ( pEntity )
	{
		pEntity->Precache();
		ItemInfo info;
		memset( &info, 0, sizeof( info ) );
		if ( ((CBasePlayerItem*)pEntity)->GetItemInfo( &info ) )
		{
			if ( info.iId < 0 || info.iId >= MAX_WEAPONS )
			{
				ALERT( at_error, "Invalid weapon ID %d for %s\n", info.iId, szClassname );
				REMOVE_ENTITY( pent );
				return;
			}
			CBasePlayerItem::ItemInfoArray[info.iId] = info;
			if ( info.pszAmmo1 && *info.pszAmmo1 )
				AddAmmoNameToAmmoRegistry( info.pszAmmo1 );
			if ( info.pszAmmo2 && *info.pszAmmo2 )
				AddAmmoNameToAmmoRegistry( info.pszAmmo2 );
		}
	}
	REMOVE_ENTITY( pent );
}

void CBasePlayerItem::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector( -24, -24, 0 );
	pev->absmax = pev->origin + Vector( 24, 24, 16 );
}


//=========================================================
// Sets up movetype, size, solidtype for a new weapon. 
//=========================================================
void CBasePlayerItem::FallInit()
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	SetTouch( &CBasePlayerItem::DefaultTouch );
	SetThink( &CBasePlayerItem::FallThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CBasePlayerItem::FallThink()
{
	pev->nextthink = gpGlobals->time + 0.1;
	if ( pev->flags & FL_ONGROUND )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		if ( !FNullEnt( pev->owner ) )
			EMIT_SOUND_DYN( ENT( pev ), CHAN_VOICE, "items/weapondrop1.wav", 1, ATTN_NORM, 0, RANDOM_LONG( 0, 29 ) + 95 );

		// lie flat
		pev->angles.x = 0;
		pev->angles.z = 0;
		Materialize();
	}
}

//=========================================================
// Materialize - make a CBasePlayerItem visible and tangible
//=========================================================
void CBasePlayerItem::Materialize()
{
	if ( pev->effects & EF_NODRAW )
	{
		if ( g_pGameRules->IsMultiplayer() )
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}
	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin( pev, pev->origin ); // link into world.
	SetTouch( &CBasePlayerItem::DefaultTouch );
	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( !CanDrop() )
			SetTouch( NULL );
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 1;
	}
	else
		SetThink( NULL );
}

//=========================================================
// AttemptToMaterialize - the item is trying to rematerialize,
// should it do so now or wait longer?
//=========================================================
void CBasePlayerItem::AttemptToMaterialize()
{
	float time = g_pGameRules->FlWeaponTryRespawn( this );
	if ( time == 0 )
		Materialize();
	else
		pev->nextthink = gpGlobals->time + time;
}

void CBasePlayerItem::CheckRespawn()
{
	g_pGameRules->WeaponShouldRespawn( this );
}

//=========================================================
// Respawn- this item is already in the world, but it is
// invisible and intangible. Make it visible and tangible.
//=========================================================
CBaseEntity* CBasePlayerItem::Respawn()
{
	CBaseEntity* pNew = CBaseEntity::Create( (char*)STRING( pev->classname ),
		g_pGameRules->VecWeaponRespawnSpot( this ), pev->angles, pev->owner );
	if ( pNew )
	{
		pNew->pev->effects |= EF_NODRAW;
		pNew->SetTouch( NULL );
		pNew->SetThink( &CBasePlayerItem::AttemptToMaterialize );
		DROP_TO_FLOOR( ENT( pev ) );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNew->pev->nextthink = g_pGameRules->FlWeaponRespawnTime( this );
	}
	else
		ALERT( at_console, "Respawn failed to create %s!\n", STRING( pev->classname ) );
	return pNew;
}

void CBasePlayerItem::DefaultTouch( CBaseEntity* pOther )
{
	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
		return;
	CBasePlayer* pPlayer = (CBasePlayer*)pOther;
	if ( pPlayer->m_bIsVIP && m_iId != WEAPON_P228 && m_iId != WEAPON_USP
		&& m_iId != WEAPON_GLOCK18 && m_iId != WEAPON_DEAGLE && m_iId != WEAPON_KNIFE )
		return;
	if ( g_pGameRules->CanHavePlayerItem( pPlayer, this ) )
	{
		if ( pOther->AddPlayerItem( this ) )
		{
			AttachToPlayer( pPlayer );
			SetThink( NULL );
			EMIT_SOUND( ENT( pPlayer->pev ), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );
		}
		SUB_UseTargets( pOther, USE_TOGGLE, 0 );
	}
	else if ( gEvilImpulse101 )
		UTIL_Remove( this );
}

int CBasePlayerItem::AddToPlayer( CBasePlayer* pPlayer )
{
	m_pPlayer = pPlayer;
	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, ENT( pPlayer->pev ) );
	WRITE_BYTE( m_iId );
	MESSAGE_END();
	return TRUE;
}

void CBasePlayerItem::AttachToPlayer( CBasePlayer* pPlayer )
{
	pev->movetype = MOVETYPE_FOLLOW;
	pev->solid = SOLID_NOT;
	pev->aiment = ENT( pPlayer->pev );
	pev->effects = EF_NODRAW; // ??
	pev->modelindex = 0; // server won't send down to clients if modelindex == 0
	pev->model = iStringNull;
	pev->owner = ENT( pPlayer->pev );
	pev->nextthink = gpGlobals->time + 0.1;
	SetTouch( NULL );
}

void CBasePlayerItem::DestroyItem()
{
	if ( m_pPlayer )
		m_pPlayer->RemovePlayerItem( this );
	Kill();
}

void CBasePlayerItem::Drop()
{
	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CBasePlayerItem::Kill()
{
	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CBasePlayerItem::Holster( int )
{
	m_pPlayer->pev->viewmodel = iStringNull;
	m_pPlayer->pev->weaponmodel = iStringNull;
}

int CBasePlayerWeapon::AddToPlayer( CBasePlayer* pPlayer )
{
	m_pPlayer = pPlayer;
	pPlayer->pev->weapons |= 1 << m_iId;
	if ( !m_iPrimaryAmmoType )
	{
		m_iPrimaryAmmoType = CBasePlayer::GetAmmoIndex( pszAmmo1() );
		m_iSecondaryAmmoType = CBasePlayer::GetAmmoIndex( pszAmmo2() );
	}
	if ( !AddWeapon() )
		return FALSE;
	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, ENT( pPlayer->pev ) );
	WRITE_BYTE( m_iId );
	MESSAGE_END();
	return TRUE;
}

int CBasePlayerWeapon::AddDuplicate( CBasePlayerItem* pOriginal )
{
	if ( m_iDefaultAmmo )
		return ExtractAmmo( (CBasePlayerWeapon*)pOriginal );
	// a dead player dropped this.
	return ExtractClipAmmo( (CBasePlayerWeapon*)pOriginal );
}

BOOL CBasePlayerWeapon::AddPrimaryAmmo( int iCount, char* szName, int iMaxClip, int iMaxCarry )
{
	int ammo;
	if ( iMaxClip <= 0 )
	{
		m_iClip = WEAPON_NOCLIP;
		ammo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	}
	else if ( m_iClip )
		ammo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	else
	{
		m_iClip = min( iMaxClip, iCount );
		ammo = m_pPlayer->GiveAmmo( iCount - m_iClip, szName, iMaxCarry );
	}
	if ( ammo <= 0 )
		return FALSE;
	m_iPrimaryAmmoType = ammo;
	if ( m_pPlayer->HasPlayerItem( this ) )
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM );
	return TRUE;
}

BOOL CBasePlayerWeapon::AddSecondaryAmmo( int iCount, char* szName, int iMaxCarry )
{
	int ammo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	if ( ammo <= 0 )
		return FALSE;
	m_iSecondaryAmmoType = ammo;
	EMIT_SOUND( ENT( pev ), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM );
	return TRUE;
}

//=========================================================
// called by the new item with the existing item as parameter
//
// if we call ExtractAmmo(), it's because the player is picking up this type of weapon for 
// the first time. If it is spawned by the world, m_iDefaultAmmo will have a default ammo amount in it.
// if  this is a weapon dropped by a dying player, has 0 m_iDefaultAmmo, which means only the ammo in 
// the weapon clip comes along. 
//=========================================================
int CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon* pWeapon )
{
	int result = FALSE;
	if ( pszAmmo1() )
	{
		// blindly call with m_iDefaultAmmo. It's either going to be a value or zero. If it is zero,
		// we only get the ammo in the weapon's clip, which is what we want. 
		result = pWeapon->AddPrimaryAmmo( m_iDefaultAmmo, (char*)pszAmmo1(), iMaxClip(), iMaxAmmo1() );
		m_iDefaultAmmo = 0;
	}
	if ( pszAmmo2() )
		result = pWeapon->AddSecondaryAmmo( 0, (char*)pszAmmo2(), iMaxAmmo2() );
	return result;
}

int CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon* pWeapon )
{
	int ammo = m_iClip == WEAPON_NOCLIP ? 0 : m_iClip;
	return pWeapon->m_pPlayer->GiveAmmo( ammo, (char*)pszAmmo1(), iMaxAmmo1() );
}

BOOL CanAttack( float attack_time, float curtime, BOOL isPredicted )
{
	if ( isPredicted )
		return attack_time <= 0;
	return curtime >= attack_time;
}

BOOL CBasePlayerWeapon::CanDeploy()
{
	return TRUE;
}

BOOL CBasePlayerWeapon::IsUseable()
{
	return m_iClip > 0 || iMaxAmmo1() == -1 || m_pPlayer->AmmoInventory( PrimaryAmmoIndex() ) > 0;
}

BOOL CBasePlayerWeapon::DefaultDeploy( char* szViewModel, char* szWeaponModel,
	int iAnim, char* szAnimExt, int skiplocal )
{
	if ( !CanDeploy() )
		return FALSE;
	m_pPlayer->TabulateAmmo();
	m_pPlayer->pev->viewmodel = MAKE_STRING( szViewModel );
	m_pPlayer->pev->weaponmodel = MAKE_STRING( szWeaponModel );
	model_name = m_pPlayer->pev->viewmodel;
	strcpy( m_pPlayer->m_szAnimExtention, szAnimExt );
	SendWeaponAnim( iAnim, skiplocal );
	m_pPlayer->m_flNextAttack = 0.75;
	m_flTimeWeaponIdle = 1.5;
	m_flLastFireTime = 0;
	m_flDecreaseShotsFired = gpGlobals->time;
	m_pPlayer->m_iFOV = 90;
	m_pPlayer->pev->fov = 90;
	m_pPlayer->m_bResumeZoom = false;
	m_pPlayer->m_iLastZoom = 90;
	return TRUE;
}

BOOL CBasePlayerWeapon::DefaultReload( int iClipSize, int iAnim, float fDelay )
{
	int ammo = m_pPlayer->AmmoInventory( m_iPrimaryAmmoType );
	if ( ammo <= 0 )
		return FALSE;
	if ( min( iClipSize - m_iClip, ammo ) <= 0 )
		return FALSE;
	m_pPlayer->m_flNextAttack = fDelay;
	ReloadSound();
	SendWeaponAnim( iAnim, UseDecrement() );
	m_fInReload = TRUE;
	m_flTimeWeaponIdle = fDelay + 0.5;
	return TRUE;
}

BOOL CBasePlayerWeapon::PlayEmptySound()
{
	if ( m_iPlayEmptySound )
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON,
			IsSecondaryWeapon( m_iId ) ? "weapons/dryfire_pistol.wav" : "weapons/dryfire_rifle.wav", 0.8, ATTN_NORM );
	return FALSE;
}

void CBasePlayerWeapon::ResetEmptySound()
{
	m_iPlayEmptySound = 1;
}

void CBasePlayerWeapon::SendWeaponAnim( int iAnim, int skiplocal )
{
	m_pPlayer->pev->weaponanim = iAnim;
	if ( skiplocal && ENGINE_CANSKIP( ENT( m_pPlayer->pev ) ) )
		return;
	MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, ENT( m_pPlayer->pev ) );
	WRITE_BYTE( iAnim );						// sequence number
	WRITE_BYTE( pev->body );					// weaponmodel bodygroup.
	MESSAGE_END();
}

int CBasePlayerWeapon::PrimaryAmmoIndex()
{
	return m_iPrimaryAmmoType;
}

//=========================================================
//=========================================================
int CBasePlayerWeapon::SecondaryAmmoIndex()
{
	return -1;
}

void CBasePlayerWeapon::RetireWeapon()
{
	// first, no viewmodel at all.
	m_pPlayer->pev->viewmodel = iStringNull;
	m_pPlayer->pev->weaponmodel = iStringNull;
	//m_pPlayer->pev->viewmodelindex = NULL;

	g_pGameRules->GetNextBestWeapon( m_pPlayer, this );
}

void CBasePlayerWeapon::Holster( int )
{
	m_fInReload = FALSE;
	m_pPlayer->pev->viewmodel = iStringNull;
	m_pPlayer->pev->weaponmodel = iStringNull;
}

float CBasePlayerWeapon::GetNextAttackDelay( float delay )
{
	if ( m_flLastFireTime == 0 || m_flNextPrimaryAttack == -1 )
	{
		m_flPrevPrimaryAttack = delay;
		m_flLastFireTime = gpGlobals->time;
	}
	m_flLastFireTime = gpGlobals->time;
	m_flPrevPrimaryAttack = delay;
	return delay;
}

void CBasePlayerWeapon::KickBack( float up_base, float lateral_base, float up_modifier,
	float lateral_modifier, float up_max, float lateral_max, int direction_change )
{
	float up;
	float lateral;
	if ( m_iShotsFired == 1 )
	{
		up = up_base;
		lateral = lateral_base;
	}
	else
	{
		up = up_base + up_modifier * m_iShotsFired;
		lateral = lateral_base + m_iShotsFired * lateral_modifier;
	}
	m_pPlayer->pev->punchangle.x -= up;
	if ( m_pPlayer->pev->punchangle.x < -up_max )
		m_pPlayer->pev->punchangle.x = -up_max;
	if ( m_iDirection == 1 )
	{
		m_pPlayer->pev->punchangle.y += lateral;
		if ( m_pPlayer->pev->punchangle.y > lateral_max )
			m_pPlayer->pev->punchangle.y = lateral_max;
	}
	else
	{
		m_pPlayer->pev->punchangle.y -= lateral;
		if ( m_pPlayer->pev->punchangle.y < -lateral_max )
			m_pPlayer->pev->punchangle.y = -lateral_max;
	}
	if ( !RANDOM_LONG( 0, direction_change ) )
		m_iDirection = 1 - m_iDirection;
}

void CBasePlayerWeapon::SetPlayerShieldAnim()
{
	if ( m_pPlayer->HasShield() )
		strcpy( m_pPlayer->m_szAnimExtention, m_iWeaponState & WPNSTATE_SHIELD_DRAWN ? "shield" : "shieldgun" );
}

void CBasePlayerWeapon::ResetPlayerShieldAnim()
{
	if ( m_pPlayer->HasShield() && (m_iWeaponState & WPNSTATE_SHIELD_DRAWN) )
		strcpy( m_pPlayer->m_szAnimExtention, "shieldgun" );
}

bool CBasePlayerWeapon::ShieldSecondaryFire( int iUpAnim, int iDownAnim )
{
	if ( !m_pPlayer->HasShield() )
		return false;
	if ( m_iWeaponState & WPNSTATE_SHIELD_DRAWN )
	{
		m_iWeaponState &= ~WPNSTATE_SHIELD_DRAWN;
		SendWeaponAnim( iDownAnim, UseDecrement() );
		strcpy( m_pPlayer->m_szAnimExtention, "shieldgun" );
		m_fMaxSpeed = 250;
		m_pPlayer->m_bShieldDrawn = false;
	}
	else
	{
		m_iWeaponState |= WPNSTATE_SHIELD_DRAWN;
		SendWeaponAnim( iUpAnim, UseDecrement() );
		strcpy( m_pPlayer->m_szAnimExtention, "shielded" );
		m_fMaxSpeed = 180;
		m_pPlayer->m_bShieldDrawn = true;
	}
	m_pPlayer->UpdateShieldCrosshair( !(m_iWeaponState & WPNSTATE_SHIELD_DRAWN) );
	m_pPlayer->ResetMaxSpeed();
	m_flNextSecondaryAttack = 0.4;
	m_flNextPrimaryAttack = 0.4;
	m_flTimeWeaponIdle = 0.6;
	return true;
}

bool CBasePlayerWeapon::HasSecondaryAttack()
{
	if ( m_pPlayer->HasShield() )
		return true;
	return (unsigned int)m_iId > 30 || !(0x54B85CE2u & (1u << m_iId));
}

void CBasePlayerWeapon::HandleInfiniteAmmo()
{
	if ( (int)infiniteAmmo.value == 1 )
		m_iClip = iMaxClip();
	else if ( (int)infiniteAmmo.value == 2 )
	{
		if ( pszAmmo1() )
			m_pPlayer->SetAmmoInventory( PrimaryAmmoIndex(), iMaxAmmo1() );
		if ( pszAmmo2() )
			m_pPlayer->SetAmmoInventory( SecondaryAmmoIndex(), iMaxAmmo2() );
	}
}

void CBasePlayerWeapon::FireRemaining( int& shotsFired, float& shootTime, BOOL bIsGlock )
{
	if ( --m_iClip < 0 )
	{
		m_iClip = 0;
		shotsFired = 3;
		shootTime = 0;
		return;
	}
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecDir;
	if ( bIsGlock )
	{
		vecDir = m_pPlayer->FireBullets3( vecSrc, gpGlobals->v_forward, 0.05, 8192, 1,
			BULLET_PLAYER_9MM, 18, 0.9, m_pPlayer->pev, true, m_pPlayer->random_seed );
		--m_pPlayer->ammo_9mm;
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, ENT( m_pPlayer->pev ), m_usFireGlock18, 0,
			(float*)&g_vecZero, (float*)&g_vecZero, vecDir.x, vecDir.y,
			(int)( m_pPlayer->pev->punchangle.x * 10000 ),
			(int)( m_pPlayer->pev->punchangle.y * 10000 ), m_iClip == 0, 0 );
	}
	else
	{
		vecDir = m_pPlayer->FireBullets3( vecSrc, gpGlobals->v_forward, m_fBurstSpread, 8192, 2,
			BULLET_PLAYER_556MM, 30, 0.96, m_pPlayer->pev, false, m_pPlayer->random_seed );
		--m_pPlayer->ammo_556nato;
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, ENT( m_pPlayer->pev ), m_usFireFamas, 0,
			(float*)&g_vecZero, (float*)&g_vecZero, vecDir.x, vecDir.y,
			(int)( m_pPlayer->pev->punchangle.x * 10000000 ),
			(int)( m_pPlayer->pev->punchangle.y * 10000000 ), 0, 0 );
	}
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	if ( ++shotsFired == 3 )
		shootTime = 0;
	else
		shootTime = gpGlobals->time + 0.1f;
}

void CBasePlayerWeapon::EjectBrassLate()
{
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecForward = gpGlobals->v_forward * 25;
	Vector vecUp = gpGlobals->v_up * RANDOM_FLOAT( 100, 150 );
	Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 50, 70 )
		+ m_pPlayer->pev->velocity + vecUp + vecForward;
	Vector vecLeft = gpGlobals->v_right * -9;
	Vector vecOrigin = pev->origin + m_pPlayer->pev->view_ofs
		+ gpGlobals->v_up * -9 + gpGlobals->v_forward * 16;
	int sound = (m_iId == WEAPON_XM1014 || m_iId == WEAPON_M3) ? TE_BOUNCE_SHOTSHELL : TE_BOUNCE_SHELL;
	EjectBrass( vecOrigin, vecLeft, vecShellVelocity, pev->angles.y, m_iShellId,
		sound, ENTINDEX( ENT( m_pPlayer->pev ) ) );
}

int CBasePlayerWeapon::UpdateClientData( CBasePlayer* pPlayer )
{
	int state = 0;
	bool send = !pPlayer->m_fWeapon;
	if ( pPlayer->m_pActiveItem == this )
	{
		state = pPlayer->m_fOnTarget ? 0x40 : 1;
		if ( this != pPlayer->m_pClientActiveItem )
			send = true;
	}
	else if ( this == pPlayer->m_pClientActiveItem )
		send = true;
	if ( m_iClip != m_iClientClip || m_iClientWeaponState != state
		|| pPlayer->m_iFOV != pPlayer->m_iClientFOV || send )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, ENT( pPlayer->pev ) );
		WRITE_BYTE( state );
		WRITE_BYTE( m_iId );
		WRITE_BYTE( m_iClip );
		MESSAGE_END();
		m_iClientWeaponState = state;
		m_iClientClip = m_iClip;
		pPlayer->m_fWeapon = TRUE;
	}
	if ( m_pNext )
		m_pNext->UpdateClientData( pPlayer );
	return TRUE;
}

void CBasePlayerWeapon::ItemPostFrame()
{
	int buttons = m_pPlayer->pev->button;
	if ( !HasSecondaryAttack() )
		buttons &= ~IN_ATTACK2;

	if ( m_flGlock18Shoot != 0 )
		FireRemaining( m_iGlock18ShotsFired, m_flGlock18Shoot, TRUE );
	else if ( gpGlobals->time > m_flFamasShoot && m_flFamasShoot != 0 )
		FireRemaining( m_iFamasShotsFired, m_flFamasShoot, FALSE );

	if ( m_flNextPrimaryAttack <= 0 && m_pPlayer->m_bResumeZoom )
	{
		m_pPlayer->m_iFOV = m_pPlayer->m_iLastZoom;
		m_pPlayer->pev->fov = (float)m_pPlayer->m_iFOV;
		if ( m_pPlayer->m_iFOV == m_pPlayer->m_iLastZoom )
			m_pPlayer->m_bResumeZoom = false;
	}
	if ( m_pPlayer->m_flEjectBrass != 0 && gpGlobals->time >= m_pPlayer->m_flEjectBrass )
	{
		m_pPlayer->m_flEjectBrass = 0;
		EjectBrassLate();
	}
	if ( !(m_pPlayer->pev->button & IN_ATTACK) )
		m_flLastFireTime = 0;

	if ( m_pPlayer->HasShield() && m_fInReload && (m_pPlayer->pev->button & IN_ATTACK2) )
	{
		SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
		m_fInReload = FALSE;
		m_pPlayer->m_flNextAttack = 0;
	}
	if ( m_fInReload && m_pPlayer->m_flNextAttack <= 0 )
	{
		int reserve = m_pPlayer->AmmoInventory( m_iPrimaryAmmoType );
		if ( reserve > 0 )
		{
			int ammo = min( iMaxClip() - m_iClip, reserve );
			m_iClip += ammo;
			m_pPlayer->SetAmmoInventory( m_iPrimaryAmmoType, reserve - ammo );
			m_pPlayer->TabulateAmmo();
		}
		m_fInReload = FALSE;
	}

	if ( (buttons & IN_ATTACK2) && !(m_flNextSecondaryAttack > 0) )
	{
		if ( pszAmmo2() && m_pPlayer->AmmoInventory( SecondaryAmmoIndex() ) <= 0 )
			m_fFireOnEmpty = TRUE;
		SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ( (m_pPlayer->pev->button & IN_ATTACK) && m_flNextPrimaryAttack <= 0 )
	{
		if ( (m_iClip == 0 && pszAmmo1())
			|| (iMaxClip() == WEAPON_NOCLIP && pszAmmo1() && m_pPlayer->AmmoInventory( PrimaryAmmoIndex() ) <= 0) )
			m_fFireOnEmpty = TRUE;
		m_pPlayer->TabulateAmmo();
		if ( (m_pPlayer->m_bCanShoot && g_pGameRules->IsMultiplayer()
			&& !g_pGameRules->IsFreezePeriod() && !m_pPlayer->m_bIsDefusing) || !g_pGameRules->IsMultiplayer() )
			PrimaryAttack();
	}
	else if ( (m_pPlayer->pev->button & IN_RELOAD) && iMaxClip() != WEAPON_NOCLIP
		&& !m_fInReload && m_flNextPrimaryAttack < 0 && m_flFamasShoot == 0 && m_flGlock18Shoot == 0 )
	{
		if ( !(m_iWeaponState & WPNSTATE_SHIELD_DRAWN) )
			Reload();
	}
	else if ( !(buttons & (IN_ATTACK | IN_ATTACK2)) )
	{
		if ( m_bDelayFire )
		{
			m_bDelayFire = false;
			if ( m_iShotsFired > 15 )
				m_iShotsFired = 15;
			m_flDecreaseShotsFired = gpGlobals->time + 0.4;
		}
		// no fire buttons down

		m_fFireOnEmpty = FALSE;
		if ( IsSecondaryWeapon( m_iId ) )
			m_iShotsFired = 0;
		else if ( m_iShotsFired > 0 && gpGlobals->time > m_flDecreaseShotsFired )
		{
			m_flDecreaseShotsFired = gpGlobals->time + 0.0225;
			--m_iShotsFired;
		}
		if ( !IsUseable() && m_flNextPrimaryAttack < 0 )
		{
			HandleInfiniteAmmo();
			WeaponIdle();
			return;
		}
		if ( !(m_iWeaponState & WPNSTATE_SHIELD_DRAWN) && m_iClip == 0
			&& !(iFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < 0
			&& m_flFamasShoot == 0 && m_flGlock18Shoot == 0 )
		{
			Reload();
			return;
		}
		HandleInfiniteAmmo();
		WeaponIdle();
		return;
	}
	HandleInfiniteAmmo();

	// catch all
	if ( ShouldWeaponIdle() )
		WeaponIdle();
}

void CBasePlayerWeapon::ReloadSound()
{
	Vector origin = m_pPlayer->pev->origin;
	CBaseEntity* pEntity = UTIL_FindEntityByClassname( NULL, "player" );
	while ( pEntity && pEntity->pev->flags != FL_DORMANT )
	{
		if ( pEntity != m_pPlayer )
		{
			float distance = (origin - pEntity->pev->origin).Length();
			if ( distance <= 512 )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgReloadSound, NULL, ENT( pEntity->pev ) );
				WRITE_BYTE( (byte)(int)((1.0 - distance * 0.001953125) * 255.0) );
				WRITE_BYTE( strcmp( STRING( pev->classname ), "weapon_m3" )
					&& strcmp( STRING( pev->classname ), "weapon_xm1014" ) ? 1 : 0 );
				MESSAGE_END();
			}
		}
		pEntity = UTIL_FindEntityByClassname( pEntity, "player" );
	}
}

void CBasePlayerAmmo::Spawn()
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );
	UTIL_SetOrigin( pev, pev->origin );
	SetTouch( &CBasePlayerAmmo::DefaultTouch );
	if ( g_pGameRules->IsMultiplayer() )
	{
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 2;
	}
}

void CBasePlayerAmmo::DefaultTouch( CBaseEntity* pOther )
{
	if ( !pOther->IsPlayer() )
		return;
	if ( AddAmmo( pOther ) )
	{
		if ( g_pGameRules->AmmoShouldRespawn( this ) == GR_AMMO_RESPAWN_YES )
		{
			Respawn();
			return;
		}
	}
	else if ( !gEvilImpulse101 )
		return;
	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}

CBaseEntity* CBasePlayerAmmo::Respawn()
{
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );
	UTIL_SetOrigin( pev, g_pGameRules->VecAmmoRespawnSpot( this ) ); // move to wherever I'm supposed to repawn.
	SetThink( &CBasePlayerAmmo::Materialize );
	pev->nextthink = g_pGameRules->FlAmmoRespawnTime( this );
	return this;
}

void CBasePlayerAmmo::Materialize()
{
	if ( pev->effects & EF_NODRAW )
	{
		if ( g_pGameRules->IsMultiplayer() )
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}
	SetTouch( &CBasePlayerAmmo::DefaultTouch );
}

void W_Precache()
{
	memset( CBasePlayerItem::ItemInfoArray, 0, sizeof( CBasePlayerItem::ItemInfoArray ) );
	memset( CBasePlayerItem::AmmoInfoArray, 0, sizeof( CBasePlayerItem::AmmoInfoArray ) );
	giAmmoIndex = 0;

	// custom items...

	// common world objects
	UTIL_PrecacheOther( "item_suit" );
	UTIL_PrecacheOther( "item_battery" );
	UTIL_PrecacheOther( "item_antidote" );
	UTIL_PrecacheOther( "item_security" );
	UTIL_PrecacheOther( "item_longjump" );
	UTIL_PrecacheOther( "item_kevlar" );
	UTIL_PrecacheOther( "item_assaultsuit" );
	UTIL_PrecacheOther( "item_thighpack" );
	UTIL_PrecacheOtherWeapon2( "weapon_awp" );
	UTIL_PrecacheOther( "ammo_338magnum" );
	UTIL_PrecacheOtherWeapon2( "weapon_g3sg1" );
	UTIL_PrecacheOtherWeapon2( "weapon_ak47" );
	UTIL_PrecacheOtherWeapon2( "weapon_scout" );
	UTIL_PrecacheOther( "ammo_762nato" );
	UTIL_PrecacheOtherWeapon2( "weapon_m249" );
	UTIL_PrecacheOther( "ammo_556natobox" );
	UTIL_PrecacheOtherWeapon2( "weapon_m4a1" );
	UTIL_PrecacheOtherWeapon2( "weapon_sg552" );
	UTIL_PrecacheOtherWeapon2( "weapon_aug" );
	UTIL_PrecacheOtherWeapon2( "weapon_sg550" );
	UTIL_PrecacheOther( "ammo_556nato" );
	UTIL_PrecacheOtherWeapon2( "weapon_m3" );
	UTIL_PrecacheOtherWeapon2( "weapon_xm1014" );
	UTIL_PrecacheOther( "ammo_buckshot" );
	UTIL_PrecacheOtherWeapon2( "weapon_usp" );
	UTIL_PrecacheOtherWeapon2( "weapon_mac10" );
	UTIL_PrecacheOtherWeapon2( "weapon_ump45" );
	UTIL_PrecacheOther( "ammo_45acp" );
	UTIL_PrecacheOtherWeapon2( "weapon_fiveseven" );
	UTIL_PrecacheOtherWeapon2( "weapon_p90" );
	UTIL_PrecacheOther( "ammo_57mm" );
	UTIL_PrecacheOtherWeapon2( "weapon_deagle" );
	UTIL_PrecacheOther( "ammo_50ae" );
	UTIL_PrecacheOtherWeapon2( "weapon_p228" );
	UTIL_PrecacheOther( "ammo_357sig" );
	UTIL_PrecacheOtherWeapon2( "weapon_knife" );
	UTIL_PrecacheOtherWeapon2( "weapon_glock18" );
	UTIL_PrecacheOtherWeapon2( "weapon_mp5navy" );
	UTIL_PrecacheOtherWeapon2( "weapon_tmp" );
	UTIL_PrecacheOtherWeapon2( "weapon_elite" );
	UTIL_PrecacheOther( "ammo_9mm" );
	UTIL_PrecacheOtherWeapon2( "weapon_flashbang" );
	UTIL_PrecacheOtherWeapon2( "weapon_hegrenade" );
	UTIL_PrecacheOtherWeapon2( "weapon_smokegrenade" );
	UTIL_PrecacheOtherWeapon2( "weapon_c4" );
	UTIL_PrecacheOtherWeapon2( "weapon_galil" );
	UTIL_PrecacheOtherWeapon2( "weapon_famas" );
	if ( g_pGameRules->IsDeathmatch() )
		UTIL_PrecacheOther( "weaponbox" ); // container for dropped deathmatch weapons
	g_sModelIndexFireball = PRECACHE_MODEL( "sprites/zerogxplode.spr" ); // fireball
	g_sModelIndexWExplosion = PRECACHE_MODEL( "sprites/WXplo1.spr" ); // underwater fireball
	g_sModelIndexSmoke = PRECACHE_MODEL( "sprites/steam1.spr" ); // smoke
	g_sModelIndexBubbles = PRECACHE_MODEL( "sprites/bubble.spr" ); //bubbles
	g_sModelIndexBloodSpray = PRECACHE_MODEL( "sprites/bloodspray.spr" ); // initial blood
	g_sModelIndexBloodDrop = PRECACHE_MODEL( "sprites/blood.spr" ); // splattered blood 
	g_sModelIndexSmokePuff = PRECACHE_MODEL( "sprites/smokepuff.spr" );
	g_sModelIndexFireball2 = PRECACHE_MODEL( "sprites/eexplo.spr" );
	g_sModelIndexFireball3 = PRECACHE_MODEL( "sprites/fexplo.spr" );
	g_sModelIndexFireball4 = PRECACHE_MODEL( "sprites/fexplo1.spr" );
	g_sModelIndexRadio = PRECACHE_MODEL( "sprites/radio.spr" );
	g_sModelIndexCTGhost = PRECACHE_MODEL( "sprites/b-tele1.spr" );
	g_sModelIndexTGhost = PRECACHE_MODEL( "sprites/c-tele1.spr" );
	g_sModelIndexC4Glow = PRECACHE_MODEL( "sprites/ledglow.spr" );
	g_sModelIndexLaser = PRECACHE_MODEL( (char*)g_pModelNameLaser );
	g_sModelIndexLaserDot = PRECACHE_MODEL( "sprites/laserdot.spr" );


	// used by explosions
	PRECACHE_MODEL( "models/grenade.mdl" );
	PRECACHE_MODEL( "sprites/explode1.spr" );
	PRECACHE_SOUND( "weapons/debris1.wav" ); // explosion aftermaths
	PRECACHE_SOUND( "weapons/debris2.wav" ); // explosion aftermaths
	PRECACHE_SOUND( "weapons/debris3.wav" ); // explosion aftermaths
	PRECACHE_SOUND( "weapons/grenade_hit1.wav" ); //grenade
	PRECACHE_SOUND( "weapons/grenade_hit2.wav" ); //grenade
	PRECACHE_SOUND( "weapons/grenade_hit3.wav" ); //grenade
	PRECACHE_SOUND( "weapons/bullet_hit1.wav" );	// hit by bullet
	PRECACHE_SOUND( "weapons/bullet_hit2.wav" );	// hit by bullet
	PRECACHE_SOUND( "items/weapondrop1.wav" );
	PRECACHE_SOUND( "weapons/generic_reload.wav" );
}


//*********************************************************
// weaponbox code:
//*********************************************************

LINK_ENTITY_TO_CLASS( weaponbox, CWeaponBox );

void CWeaponBox::Precache()
{
	PRECACHE_MODEL( "models/w_weaponbox.mdl" );
}

void CWeaponBox::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	m_bIsBomb = false;
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	SET_MODEL( ENT( pev ), "models/w_weaponbox.mdl" );
}

void CWeaponBox::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector( -16, -16, 0 );
	pev->absmax = pev->origin + Vector( 16, 16, 16 );
}

void CWeaponBox::Kill()
{
	for ( int i = 0; i < MAX_ITEM_TYPES; ++i )
	{
		for ( CBasePlayerItem* pItem = m_rgpPlayerItems[i]; pItem; pItem = pItem->m_pNext )
		{
			pItem->SetThink( &CBaseEntity::SUB_Remove );
			pItem->pev->nextthink = gpGlobals->time + 0.1;
		}
	}

	// remove the box
	UTIL_Remove( this );
}

BOOL CWeaponBox::HasWeapon( CBasePlayerItem* pCheckItem )
{
	for ( CBasePlayerItem* pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()]; pItem; pItem = pItem->m_pNext )
	{
		if ( !strcmp( STRING( pItem->pev->classname ), STRING( pCheckItem->pev->classname ) ) )
			return TRUE;
	}
	return FALSE;
}

//=========================================================
// CWeaponBox - PackWeapon: Add this weapon to the box
//=========================================================
BOOL CWeaponBox::PackWeapon( CBasePlayerItem* pWeapon )
{
	// is one of these weapons already packed in this box?
	if ( HasWeapon( pWeapon ) )
		return FALSE;
	if ( pWeapon->m_pPlayer )
	{
		if ( pWeapon == pWeapon->m_pPlayer->m_pActiveItem )
			pWeapon->Holster();
		if ( !pWeapon->m_pPlayer->RemovePlayerItem( pWeapon ) )
			return FALSE;
	}
	int slot = pWeapon->iItemSlot();
	pWeapon->m_pNext = m_rgpPlayerItems[slot];
	m_rgpPlayerItems[slot] = pWeapon;
	pWeapon->pev->spawnflags |= SF_NORESPAWN; // never respawn
	pWeapon->pev->movetype = MOVETYPE_NONE;
	pWeapon->pev->solid = SOLID_NOT;
	pWeapon->pev->effects = EF_NODRAW;
	pWeapon->pev->modelindex = 0;
	pWeapon->pev->model = iStringNull;
	pWeapon->pev->owner = ENT( pev );
	pWeapon->m_pPlayer = NULL;
	pWeapon->SetThink( NULL ); // crowbar may be trying to swing again, etc.
	pWeapon->SetTouch( NULL );
	return TRUE;
}

BOOL CWeaponBox::IsEmpty()
{
	for ( int i = 0; i < MAX_ITEM_TYPES; ++i )
	{
		if ( m_rgpPlayerItems[i] )
			return FALSE;
	}
	for ( int i = 0; i < MAX_AMMO_SLOTS; ++i )
	{
		if ( m_rgiszAmmo[i] )
			return FALSE;
	}
	return TRUE;
}

//=========================================================
// CWeaponBox - GiveAmmo
//=========================================================
int CWeaponBox::GiveAmmo( int iCount, char* szName, int iMax, int* pIndex )
{
	for ( int i = 1; i < MAX_AMMO_SLOTS; ++i )
	{
		if ( !m_rgiszAmmo[i] )
		{
			if ( pIndex )
				*pIndex = i;
			m_rgiszAmmo[i] = MAKE_STRING( szName );
			m_rgAmmo[i] = iCount;
			return i;
		}
		if ( !stricmp( szName, STRING( m_rgiszAmmo[i] ) ) )
		{
			if ( pIndex )
				*pIndex = i;
			int count = min( iMax - m_rgAmmo[i], iCount );
			if ( iCount && count <= 0 )
				return -1;
			m_rgAmmo[i] += count;
			return i;
		}
	}
	ALERT( at_console, "out of named ammo slots\n" );
	return MAX_AMMO_SLOTS;
}

BOOL CWeaponBox::PackAmmo( int iszName, int iCount )
{
	if ( !iszName )
	{
		// error here
		ALERT( at_console, "NULL String in PackAmmo!\n" );
		return FALSE;
	}
	int max = MaxAmmoCarry( iszName );
	if ( max == -1 || iCount <= 0 )
		return FALSE;
	GiveAmmo( iCount, (char*)STRING( iszName ), max );
	return TRUE;
}

//=========================================================
//=========================================================
void CWeaponBox::KeyValue( KeyValueData* pkvd )
{
	if ( m_cAmmoTypes >= MAX_AMMO_SLOTS )
	{
		ALERT( at_console, "WeaponBox too full! only %d ammotypes allowed\n", MAX_AMMO_SLOTS );
		return;
	}
	PackAmmo( ALLOC_STRING( pkvd->szKeyName ), atoi( pkvd->szValue ) );
	++m_cAmmoTypes;
	pkvd->fHandled = TRUE;
}

void CWeaponBox::BombThink()
{
	if ( !m_bIsBomb )
		return;
	CBaseEntity* pEntity = NULL;
	while ( (pEntity = UTIL_FindEntityByClassname( pEntity, "player" )) != NULL )
	{
		if ( FNullEnt( pEntity->edict() ) )
			break;
		if ( pEntity->IsPlayer() && pEntity->pev->flags != FL_DORMANT )
		{
			CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pEntity->pev );
			if ( pPlayer->pev->deadflag == DEAD_NO && pPlayer->m_iTeam == TERRORIST )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgBombDrop, NULL, pPlayer->edict() );
				WRITE_COORD( pev->origin.x );
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z );
				WRITE_BYTE( 0 );
				MESSAGE_END();
			}
		}
	}
	pev->nextthink = gpGlobals->time + 1;
}

void CWeaponBox::Touch( CBaseEntity* pOther )
{
	if ( !(pev->flags & FL_ONGROUND) || !pOther->IsPlayer() || !pOther->IsAlive() )
		return;
	CBasePlayer* pPlayer = (CBasePlayer*)pOther;
	if ( pPlayer->m_bIsVIP || pPlayer->m_bShieldDrawn )
		return;
	pPlayer->OnTouchingWeapon( this );
	bool bEmitSound = false;
	bool bRemove = true;
	for ( int slot = 0; slot < MAX_ITEM_TYPES; ++slot )
	{
		CBasePlayerItem* pItem = m_rgpPlayerItems[slot];
		while ( pItem )
		{
			if ( (pPlayer->HasShield() && pItem->m_iId == WEAPON_ELITE)
				|| (pPlayer->IsBot() && !TheCSBots()->IsWeaponUseable( pItem )) )
				return;
			if ( !strcmp( STRING( pItem->pev->classname ), "weapon_c4" ) )
			{
				if ( pPlayer->m_iTeam != TERRORIST || pPlayer->pev->deadflag != DEAD_NO )
					return;
				if ( pPlayer->m_bShowHints && !(pPlayer->m_flDisplayHistory & 0x1000) )
				{
					pPlayer->m_flDisplayHistory |= 0x1000;
					pPlayer->HintMessage( "#Hint_you_have_the_bomb", FALSE, FALSE );
				}
				else
					ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "#Got_bomb" );
				UTIL_LogPrintf( "\"%s<%i><%s><TERRORIST>\" triggered \"Got_The_Bomb\"\n",
					STRING( pPlayer->pev->netname ), GETPLAYERUSERID( pPlayer->edict() ), GETPLAYERAUTHID( pPlayer->edict() ) );
				g_pGameRules->m_bBombDropped = FALSE;
				MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
				WRITE_BYTE( 9 );
				WRITE_BYTE( DRC_CMD_EVENT );
				WRITE_SHORT( pPlayer->entindex() );
				WRITE_SHORT( entindex() );
				WRITE_LONG( 6 );
				MESSAGE_END();
				pPlayer->m_bHasC4 = true;
				pPlayer->SetBombIcon( FALSE );
				pPlayer->pev->body = 1;
				CBaseEntity* pEntity = NULL;
				while ( (pEntity = UTIL_FindEntityByClassname( pEntity, "player" )) != NULL )
				{
					if ( FNullEnt( pEntity->edict() ) )
						break;
					if ( pEntity->IsPlayer() && pEntity->pev->flags != FL_DORMANT )
					{
						CBasePlayer* pRecipient = GetClassPtr( (CBasePlayer*)pEntity->pev );
						if ( pRecipient->pev->deadflag == DEAD_NO && pRecipient->m_iTeam == TERRORIST )
						{
							if ( pRecipient != pPlayer )
								ClientPrint( pRecipient->pev, HUD_PRINTCENTER, "#Game_bomb_pickup", STRING( pPlayer->pev->netname ) );
							MESSAGE_BEGIN( MSG_ONE, gmsgBombPickup, NULL, pRecipient->edict() );
							MESSAGE_END();
						}
					}
				}
				TheCSBots()->SetLooseBomb( NULL );
				TheBots->OnEvent( EVENT_BOMB_PICKED_UP, pPlayer, NULL );
			}
			if ( (slot == 1 || slot == 2) && pPlayer->m_rgpPlayerItems[slot] )
			{
				pItem = m_rgpPlayerItems[slot]->m_pNext;
				bRemove = false;
				continue;
			}
			if ( slot == 4 )
			{
				CBasePlayerWeapon* pGrenade = (CBasePlayerWeapon*)m_rgpPlayerItems[4];
				if ( pGrenade->IsWeapon() )
				{
					int limit = 1;
					const char* name = NULL;
					switch ( pGrenade->m_iId )
					{
					case WEAPON_HEGRENADE: name = "weapon_hegrenade"; break;
					case WEAPON_SMOKEGRENADE: name = "weapon_smokegrenade"; break;
					case WEAPON_FLASHBANG: name = "weapon_flashbang"; limit = 2; break;
					}
					int currentGrenades = pPlayer->AmmoInventory( pGrenade->m_iPrimaryAmmoType );
					if ( name && currentGrenades >= 0 && currentGrenades < limit )
					{
						pPlayer->GiveNamedItem( name );
						bEmitSound = true;
						pItem = m_rgpPlayerItems[4]->m_pNext;
						m_rgpPlayerItems[4] = pItem;
					}
					else
					{
						pItem = pItem->m_pNext;
						bRemove = false;
					}
				}
				else
				{
					pItem = pGrenade->m_pNext;
					bRemove = false;
				}
				continue;
			}
			if ( slot == 1 && pPlayer->HasShield() )
			{
				pItem = m_rgpPlayerItems[1]->m_pNext;
				bRemove = false;
				continue;
			}
			if ( pPlayer->AddPlayerItem( pItem ) )
			{
				pItem->AttachToPlayer( pPlayer );
				bEmitSound = true;
			}
			pItem = m_rgpPlayerItems[slot]->m_pNext;
			m_rgpPlayerItems[slot] = pItem;
		}
	}
	if ( bRemove )
	{
		for ( int i = 0; i < MAX_AMMO_SLOTS; ++i )
		{
			if ( m_rgiszAmmo[i] )
			{
				// there's some ammo of this type. 
				pPlayer->GiveAmmo( m_rgAmmo[i], (char*)STRING( m_rgiszAmmo[i] ), MaxAmmoCarry( m_rgiszAmmo[i] ) );

				//ALERT ( at_console, "Gave %d rounds of %s\n", m_rgAmmo[i], STRING(m_rgiszAmmo[i]) );

				// now empty the ammo from the weaponbox since we just gave it to the player
				m_rgiszAmmo[i] = iStringNull;
				m_rgAmmo[i] = 0;
			}
		}
	}
	if ( bEmitSound )
		EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );
	if ( bRemove )
	{
		SetTouch( NULL );
		UTIL_Remove( this );
	}
}

enum
{
	ARMOURY_MP5NAVY = 0,
	ARMOURY_TMP,
	ARMOURY_P90,
	ARMOURY_MAC10,
	ARMOURY_AK47,
	ARMOURY_SG552,
	ARMOURY_M4A1,
	ARMOURY_AUG,
	ARMOURY_SCOUT,
	ARMOURY_G3SG1,
	ARMOURY_AWP,
	ARMOURY_M3,
	ARMOURY_XM1014,
	ARMOURY_M249,
	ARMOURY_FLASHBANG,
	ARMOURY_HEGRENADE,
	ARMOURY_KEVLAR,
	ARMOURY_ASSAULT,
	ARMOURY_SMOKEGRENADE
};

class CArmoury : public CBaseEntity
{
public:
	void KeyValue( KeyValueData* pkvd );
	void Spawn();
	void Restart();
	void Precache();
	void EXPORT ArmouryTouch( CBaseEntity* pOther );

	int m_iItem;
	int m_iCount;
	int m_iInitialCount;
	bool m_bAlreadyCounted;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

LINK_ENTITY_TO_CLASS( armoury_entity, CArmoury );

void CArmoury::KeyValue( KeyValueData* pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "item" ) )
	{
		m_iItem = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "count" ) )
	{
		m_iCount = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		pkvd->fHandled = FALSE;
}

void CArmoury::Precache()
{
	switch ( m_iItem )
	{
	case ARMOURY_MP5NAVY: PRECACHE_MODEL( "models/w_mp5.mdl" ); break;
	case ARMOURY_TMP: PRECACHE_MODEL( "models/w_tmp.mdl" ); break;
	case ARMOURY_P90: PRECACHE_MODEL( "models/w_p90.mdl" ); break;
	case ARMOURY_MAC10: PRECACHE_MODEL( "models/w_mac10.mdl" ); break;
	case ARMOURY_AK47: PRECACHE_MODEL( "models/w_ak47.mdl" ); break;
	case ARMOURY_SG552: PRECACHE_MODEL( "models/w_sg552.mdl" ); break;
	case ARMOURY_M4A1: PRECACHE_MODEL( "models/w_m4a1.mdl" ); break;
	case ARMOURY_AUG: PRECACHE_MODEL( "models/w_aug.mdl" ); break;
	case ARMOURY_SCOUT: PRECACHE_MODEL( "models/w_scout.mdl" ); break;
	case ARMOURY_G3SG1: PRECACHE_MODEL( "models/w_g3sg1.mdl" ); break;
	case ARMOURY_AWP: PRECACHE_MODEL( "models/w_awp.mdl" ); break;
	case ARMOURY_M3: PRECACHE_MODEL( "models/w_m3.mdl" ); break;
	case ARMOURY_XM1014: PRECACHE_MODEL( "models/w_xm1014.mdl" ); break;
	case ARMOURY_M249: PRECACHE_MODEL( "models/w_m249.mdl" ); break;
	case ARMOURY_FLASHBANG: PRECACHE_MODEL( "models/w_flashbang.mdl" ); break;
	case ARMOURY_HEGRENADE: PRECACHE_MODEL( "models/w_hegrenade.mdl" ); break;
	case ARMOURY_ASSAULT: PRECACHE_MODEL( "models/w_assault.mdl" ); break;
	case ARMOURY_SMOKEGRENADE: PRECACHE_MODEL( "models/w_smokegrenade.mdl" ); break;
	default: PRECACHE_MODEL( "models/w_kevlar.mdl" ); break;
	}
}

void CArmoury::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );
	UTIL_SetOrigin( pev, pev->origin );
	SetTouch( &CArmoury::ArmouryTouch );
	switch ( m_iItem )
	{
	case ARMOURY_MP5NAVY: SET_MODEL( edict(), "models/w_mp5.mdl" ); break;
	case ARMOURY_TMP: SET_MODEL( edict(), "models/w_tmp.mdl" ); break;
	case ARMOURY_P90: SET_MODEL( edict(), "models/w_p90.mdl" ); break;
	case ARMOURY_MAC10: SET_MODEL( edict(), "models/w_mac10.mdl" ); break;
	case ARMOURY_AK47: SET_MODEL( edict(), "models/w_ak47.mdl" ); break;
	case ARMOURY_SG552: SET_MODEL( edict(), "models/w_sg552.mdl" ); break;
	case ARMOURY_M4A1: SET_MODEL( edict(), "models/w_m4a1.mdl" ); break;
	case ARMOURY_AUG: SET_MODEL( edict(), "models/w_aug.mdl" ); break;
	case ARMOURY_SCOUT: SET_MODEL( edict(), "models/w_scout.mdl" ); break;
	case ARMOURY_G3SG1: SET_MODEL( edict(), "models/w_g3sg1.mdl" ); break;
	case ARMOURY_AWP: SET_MODEL( edict(), "models/w_awp.mdl" ); break;
	case ARMOURY_M3: SET_MODEL( edict(), "models/w_m3.mdl" ); break;
	case ARMOURY_XM1014: SET_MODEL( edict(), "models/w_xm1014.mdl" ); break;
	case ARMOURY_M249: SET_MODEL( edict(), "models/w_m249.mdl" ); break;
	case ARMOURY_FLASHBANG: SET_MODEL( edict(), "models/w_flashbang.mdl" ); break;
	case ARMOURY_HEGRENADE: SET_MODEL( edict(), "models/w_hegrenade.mdl" ); break;
	case ARMOURY_ASSAULT: SET_MODEL( edict(), "models/w_assault.mdl" ); break;
	case ARMOURY_SMOKEGRENADE: SET_MODEL( edict(), "models/w_smokegrenade.mdl" ); break;
	default: SET_MODEL( edict(), "models/w_kevlar.mdl" ); break;
	}
	if ( m_iCount <= 0 )
		m_iCount = 1;
	m_iInitialCount = m_iCount;
	m_bAlreadyCounted = false;
}

void CArmoury::Restart()
{
	CHalfLifeMultiplay* pRules = (CHalfLifeMultiplay*)g_pGameRules;
	if ( m_iItem == ARMOURY_FLASHBANG || m_iItem == ARMOURY_HEGRENADE )
	{
		if ( !m_bAlreadyCounted )
		{
			m_bAlreadyCounted = true;
			pRules->m_iTotalGrenadeCount += m_iInitialCount;
			m_iCount = m_iInitialCount;
		}
		else
		{
			m_iCount = (int)((double)(m_iInitialCount / pRules->m_iTotalGrenadeCount) * pRules->m_iNumTerrorist * 1.75);
			if ( m_iCount <= 0 )
				m_iCount = 1;
		}
	}
	else if ( m_iItem == ARMOURY_KEVLAR || m_iItem == ARMOURY_ASSAULT )
	{
		if ( !m_bAlreadyCounted )
		{
			m_bAlreadyCounted = true;
			pRules->m_iTotalArmourCount += m_iInitialCount;
			m_iCount = m_iInitialCount;
		}
		else
		{
			float count = (float)(m_iInitialCount / pRules->m_iTotalArmourCount) * pRules->m_iNumTerrorist;
			m_iCount = (int)count;
			if ( m_iCount <= 0 )
				m_iCount = 1;
		}
	}
	else
	{
		if ( !m_bAlreadyCounted )
		{
			m_bAlreadyCounted = true;
			pRules->m_iTotalGunCount += m_iInitialCount;
			m_iCount = m_iInitialCount;
		}
		else
		{
			m_iCount = (int)((double)(m_iInitialCount / pRules->m_iTotalGunCount) * pRules->m_iNumTerrorist * 0.85);
			if ( m_iCount <= 0 )
				m_iCount = 1;
		}
	}
	pev->effects &= ~EF_NODRAW;
}

void CArmoury::ArmouryTouch( CBaseEntity* pOther )
{
	if ( !pOther->IsPlayer() )
		return;
	CBasePlayer* pPlayer = (CBasePlayer*)pOther;
	if ( pPlayer->m_bIsVIP || m_iCount <= 0 )
	{
		if ( !pPlayer->m_bIsVIP && m_iCount == 0 )
			pev->effects |= EF_NODRAW;
		return;
	}
	if ( m_iItem <= ARMOURY_M249 )
	{
		if ( pPlayer->m_bHasPrimary )
			return;
		--m_iCount;
		switch ( m_iItem )
		{
		case ARMOURY_MP5NAVY:
			pPlayer->GiveNamedItem( "weapon_mp5navy" );
			pPlayer->GiveAmmo( 60, "9mm", MAX_AMMO_9MM );
			break;
		case ARMOURY_TMP:
			pPlayer->GiveNamedItem( "weapon_tmp" );
			pPlayer->GiveAmmo( 60, "9mm", MAX_AMMO_9MM );
			break;
		case ARMOURY_P90:
			pPlayer->GiveNamedItem( "weapon_p90" );
			pPlayer->GiveAmmo( 50, "57mm", MAX_AMMO_57MM );
			break;
		case ARMOURY_MAC10:
			pPlayer->GiveNamedItem( "weapon_mac10" );
			pPlayer->GiveAmmo( 60, "45acp", MAX_AMMO_45ACP );
			break;
		case ARMOURY_AK47:
			pPlayer->GiveNamedItem( "weapon_ak47" );
			pPlayer->GiveAmmo( 60, "762Nato", MAX_AMMO_762NATO );
			break;
		case ARMOURY_SG552:
			pPlayer->GiveNamedItem( "weapon_sg552" );
			pPlayer->GiveAmmo( 60, "556Nato", MAX_AMMO_556NATO );
			break;
		case ARMOURY_M4A1:
			pPlayer->GiveNamedItem( "weapon_m4a1" );
			pPlayer->GiveAmmo( 60, "556Nato", MAX_AMMO_556NATO );
			break;
		case ARMOURY_AUG:
			pPlayer->GiveNamedItem( "weapon_aug" );
			pPlayer->GiveAmmo( 60, "556Nato", MAX_AMMO_556NATO );
			break;
		case ARMOURY_SCOUT:
			pPlayer->GiveNamedItem( "weapon_scout" );
			pPlayer->GiveAmmo( 30, "762Nato", MAX_AMMO_762NATO );
			break;
		case ARMOURY_G3SG1:
			pPlayer->GiveNamedItem( "weapon_g3sg1" );
			pPlayer->GiveAmmo( 30, "762Nato", MAX_AMMO_762NATO );
			break;
		case ARMOURY_AWP:
			pPlayer->GiveNamedItem( "weapon_awp" );
			pPlayer->GiveAmmo( 20, "338Magnum", MAX_AMMO_338MAGNUM );
			break;
		case ARMOURY_M3:
			pPlayer->GiveNamedItem( "weapon_m3" );
			pPlayer->GiveAmmo( 24, "buckshot", MAX_AMMO_BUCKSHOT );
			break;
		case ARMOURY_XM1014:
			pPlayer->GiveNamedItem( "weapon_xm1014" );
			pPlayer->GiveAmmo( 24, "buckshot", MAX_AMMO_BUCKSHOT );
			break;
		case ARMOURY_M249:
			pPlayer->GiveNamedItem( "weapon_m249" );
			pPlayer->GiveAmmo( 60, "556NatoBox", MAX_AMMO_556NATOBOX );
			break;
		}
	}
	else
	{
		switch ( m_iItem )
		{
		case ARMOURY_FLASHBANG:
			if ( pPlayer->AmmoInventory( CBasePlayer::GetAmmoIndex( "Flashbang" ) ) > 1 )
				return;
			pPlayer->GiveNamedItem( "weapon_flashbang" );
			break;
		case ARMOURY_HEGRENADE:
			if ( pPlayer->AmmoInventory( CBasePlayer::GetAmmoIndex( "HEGrenade" ) ) > 0 )
				return;
			pPlayer->GiveNamedItem( "weapon_hegrenade" );
			break;
		case ARMOURY_KEVLAR:
			if ( pPlayer->m_iKevlar == 1 )
				return;
			pPlayer->GiveNamedItem( "item_kevlar" );
			break;
		case ARMOURY_ASSAULT:
			if ( pPlayer->m_iKevlar == 2 )
				return;
			pPlayer->GiveNamedItem( "item_assaultsuit" );
			break;
		case ARMOURY_SMOKEGRENADE:
			if ( pPlayer->AmmoInventory( CBasePlayer::GetAmmoIndex( "SmokeGrenade" ) ) > 0 )
				return;
			pPlayer->GiveNamedItem( "weapon_smokegrenade" );
			break;
		default:
			return;
		}
		--m_iCount;
	}
	if ( m_iCount == 0 )
		pev->effects |= EF_NODRAW;
}
