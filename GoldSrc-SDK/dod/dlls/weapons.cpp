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
#include "weapons.h"
#include "gamerules.h"
#include "decals.h"
#include "../cl_dll/dod_shared.h"

extern int gEvilImpulse101;
extern int gmsgCurWeapon;
extern int gmsgReloadDone;
extern Vector g_vecAttackDir;

short g_sModelIndexLaser;
const char* g_pModelNameLaser = "sprites/laserbeam.spr";
short g_sModelIndexLaserDot;
short g_sModelIndexFireball;
short g_sModelIndexSmoke;
short g_sModelIndexBubbles;
short g_sModelIndexBloodDrop;
short g_sModelIndexBloodSpray;
unsigned short m_usPlayWhizz;
int giAmmoIndex;
ItemInfo CBasePlayerItem::ItemInfoArray[MAX_WEAPONS];
AmmoInfo CBasePlayerItem::AmmoInfoArray[MAX_AMMO_SLOTS];
MULTIDAMAGE gMultiDamage;
char* szRespawnWeapons[3] = { "weapon_bazooka", "weapon_pschreck", "weapon_piat" };

enum
{
	WEAPON_CLASS_ROCKET = 512,
	WEAPON_CLASS_SCOPED = 32,
	WEAPON_CLASS_MORTAR = 1024,
	WEAPON_STATE_RELOAD_DEPLOY = 2,
	ROUND_ACTIVE = 1
};

TYPEDESCRIPTION CBasePlayerItem::m_SaveData[] =
{
	DEFINE_FIELD( CBasePlayerItem, m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayerItem, m_pNext, FIELD_CLASSPTR ),
	//DEFINE_FIELD( CBasePlayerItem, m_fKnown, FIELD_INTEGER ),Reset to zero on load
	DEFINE_FIELD( CBasePlayerItem, m_iId, FIELD_INTEGER ),
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
	DEFINE_FIELD( CBasePlayerWeapon, m_iDefaultAmmo, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBasePlayerWeapon, CBasePlayerItem );

TYPEDESCRIPTION CWeaponBox::m_SaveData[] =
{
	DEFINE_ARRAY( CWeaponBox, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgiszAmmo, FIELD_STRING, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( CWeaponBox, m_cAmmoTypes, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CWeaponBox, CBaseEntity );

//=========================================================
// MaxAmmoCarry - pass in a name and this function will tell
// you the maximum amount of that type of ammunition that a 
// player can carry.
//=========================================================
int MaxAmmoCarry( int iszName )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo1 &&
			!strcmp( STRING( iszName ), CBasePlayerItem::ItemInfoArray[i].pszAmmo1 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo1;
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo2 &&
			!strcmp( STRING( iszName ), CBasePlayerItem::ItemInfoArray[i].pszAmmo2 ) )
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


//
// ApplyMultiDamage - inflicts contents of global multi damage register on gMultiDamage.pEntity
//
// GLOBALS USED:
//		gMultiDamage

void ApplyMultiDamage( entvars_t *pevInflictor, entvars_t *pevAttacker )
{
	Vector		vecSpot1;//where blood comes from
	Vector		vecDir;//direction blood should go
	TraceResult	tr;

	if ( !gMultiDamage.pEntity )
		return;

	gMultiDamage.pEntity->TakeDamage( pevInflictor, pevAttacker, gMultiDamage.amount, gMultiDamage.type );
}


// GLOBALS USED:
//		gMultiDamage

void AddMultiDamage( entvars_t* pevInflictor, CBaseEntity* pEntity, float damage, int damageType )
{
	if ( !pEntity )
		return;
	gMultiDamage.type |= damageType;
	if ( pEntity != gMultiDamage.pEntity )
	{
		ApplyMultiDamage( pevInflictor, pevInflictor ); // UNDONE: wrong attacker!
		gMultiDamage.pEntity = pEntity;
		gMultiDamage.amount = 0;
	}
	gMultiDamage.amount += damage;
}

/*
================
SpawnBlood
================
*/
void SpawnBlood( Vector spot, int bloodColor, float damage )
{
	UTIL_BloodDrips( spot, g_vecAttackDir, bloodColor, (int)damage );
}

int DamageDecal( CBaseEntity *pEntity, int bitsDamageType )
{
	if ( !pEntity )
		return (DECAL_GUNSHOT1 + RANDOM_LONG(0,4));

	return pEntity->DamageDecal( bitsDamageType );
}

void DecalGunshot( TraceResult* pTrace, int bulletType )
{
	// Is the entity valid
	if ( !UTIL_IsValidEntity( pTrace->pHit ) )
		return;
	if ( VARS( pTrace->pHit )->solid == SOLID_BSP || VARS( pTrace->pHit )->movetype == MOVETYPE_PUSHSTEP )
	{
		CBaseEntity* pEntity = NULL;
		// Decal the wall with a gunshot
		if ( !FNullEnt( pTrace->pHit ) )
			pEntity = CBaseEntity::Instance( pTrace->pHit );
		if ( bulletType == BULLET_PLAYER_CROWBAR )
			UTIL_DecalTrace( pTrace, DamageDecal( pEntity, DMG_CLUB ) );
		else
			UTIL_GunshotDecalTrace( pTrace, DamageDecal( pEntity, DMG_BULLET ) );
	}
}



//
// EjectBrass - tosses a brass shell from passed origin at passed velocity
//
void EjectBrass( const Vector& origin, const Vector& velocity, float rotation, int model, int sound )
{
	// FIX: when the player shoots, their gun isn't in the same position as it is on the model other players see.

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
	WRITE_BYTE( TE_MODEL );
	WRITE_COORD( origin.x );
	WRITE_COORD( origin.y );
	WRITE_COORD( origin.z );
	WRITE_COORD( velocity.x );
	WRITE_COORD( velocity.y );
	WRITE_COORD( velocity.z );
	WRITE_ANGLE( rotation );
	WRITE_SHORT( model );
	WRITE_BYTE( sound );
	WRITE_BYTE( 25 ); // 2.5 seconds
	MESSAGE_END();
}

void AddAmmoNameToAmmoRegistry( const char* name )
{
	// make sure it's not already in the registry
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( CBasePlayerItem::AmmoInfoArray[i].pszName &&
			!stricmp( CBasePlayerItem::AmmoInfoArray[i].pszName, name ) )
			return;
	}
	if ( giAmmoIndex + 1 >= MAX_AMMO_SLOTS )
	{
		ALERT( at_error, "Too many ammo types (max %d), ignoring %s\n", MAX_AMMO_SLOTS - 1, name );
		return;
	}
	giAmmoIndex++;
	CBasePlayerItem::AmmoInfoArray[giAmmoIndex].pszName = name;
	CBasePlayerItem::AmmoInfoArray[giAmmoIndex].iId = giAmmoIndex;   // yes, this info is redundant
}


// Precaches the weapon and queues the weapon info for sending to clients
void UTIL_PrecacheOtherWeapon( const char *szClassname )
{
	edict_t	*pent;

	pent = CREATE_NAMED_ENTITY( MAKE_STRING( szClassname ) );
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in UTIL_PrecacheOtherWeapon\n" );
		return;
	}

	CBaseEntity *pEntity = CBaseEntity::Instance (VARS( pent ));

	if (pEntity)
	{
		ItemInfo II;
		pEntity->Precache( );
		memset( &II, 0, sizeof II );
		if ( ((CBasePlayerItem*)pEntity)->GetItemInfo( &II ) )
		{
			if ( II.iId < 0 || II.iId >= MAX_WEAPONS )
			{
				ALERT( at_error, "Invalid weapon ID %d for %s\n", II.iId, szClassname );
				REMOVE_ENTITY( pent );
				return;
			}

			CBasePlayerItem::ItemInfoArray[II.iId] = II;

			if ( II.pszAmmo1 && *II.pszAmmo1 )
			{
				AddAmmoNameToAmmoRegistry( II.pszAmmo1 );
			}

			if ( II.pszAmmo2 && *II.pszAmmo2 )
			{
				AddAmmoNameToAmmoRegistry( II.pszAmmo2 );
			}

			memset( &II, 0, sizeof II );
		}
	}

	REMOVE_ENTITY(pent);
}

void W_Precache()
{
	memset( CBasePlayerItem::ItemInfoArray, 0, sizeof( CBasePlayerItem::ItemInfoArray ) );
	memset( CBasePlayerItem::AmmoInfoArray, 0, sizeof( CBasePlayerItem::AmmoInfoArray ) );
	giAmmoIndex = 0;
	UTIL_PrecacheOtherWeapon( "weapon_thompson" );
	UTIL_PrecacheOtherWeapon( "weapon_m1carbine" );
	UTIL_PrecacheOtherWeapon( "weapon_garand" );
	UTIL_PrecacheOtherWeapon( "weapon_scopedkar" );
	UTIL_PrecacheOtherWeapon( "weapon_luger" );
	UTIL_PrecacheOtherWeapon( "weapon_spring" );
	UTIL_PrecacheOtherWeapon( "weapon_colt" );
	UTIL_PrecacheOtherWeapon( "weapon_amerknife" );
	UTIL_PrecacheOtherWeapon( "weapon_gerknife" );
	UTIL_PrecacheOtherWeapon( "weapon_spade" );
	UTIL_PrecacheOtherWeapon( "weapon_kar" );
	UTIL_PrecacheOtherWeapon( "weapon_mp44" );
	UTIL_PrecacheOtherWeapon( "weapon_mp40" );
	UTIL_PrecacheOtherWeapon( "weapon_mg42" );
	UTIL_PrecacheOtherWeapon( "weapon_mg34" );
	UTIL_PrecacheOtherWeapon( "weapon_30cal" );
	UTIL_PrecacheOtherWeapon( "weapon_bar" );

	// hand grenade
	UTIL_PrecacheOtherWeapon( "weapon_handgrenade" );
	UTIL_PrecacheOtherWeapon( "weapon_handgrenade_ex" );
	UTIL_PrecacheOtherWeapon( "weapon_stickgrenade" );
	UTIL_PrecacheOtherWeapon( "weapon_stickgrenade_ex" );
	UTIL_PrecacheOtherWeapon( "weapon_greasegun" );
	UTIL_PrecacheOtherWeapon( "weapon_fg42" );
	UTIL_PrecacheOtherWeapon( "weapon_k43" );
	UTIL_PrecacheOtherWeapon( "weapon_enfield" );
	UTIL_PrecacheOtherWeapon( "weapon_sten" );
	UTIL_PrecacheOtherWeapon( "weapon_bren" );
	UTIL_PrecacheOtherWeapon( "weapon_webley" );
	UTIL_PrecacheOtherWeapon( "weapon_bazooka" );
	UTIL_PrecacheOtherWeapon( "weapon_pschreck" );
	UTIL_PrecacheOtherWeapon( "weapon_piat" );
	UTIL_PrecacheOther( "shell_bazooka" );
	UTIL_PrecacheOther( "shell_piat" );
	UTIL_PrecacheOther( "shell_pschreck" );
	UTIL_PrecacheOther( "ammo_garand" );
	UTIL_PrecacheOther( "ammo_m1carbine" );
	UTIL_PrecacheOther( "ammo_kar" );
	UTIL_PrecacheOther( "ammo_scopedkar" );
	UTIL_PrecacheOther( "ammo_spring" );
	UTIL_PrecacheOther( "ammo_thompson" );
	UTIL_PrecacheOther( "ammo_mp40" );
	UTIL_PrecacheOther( "ammo_mp44" );
	UTIL_PrecacheOther( "ammo_bar" );
	UTIL_PrecacheOther( "ammo_mg42" );
	UTIL_PrecacheOther( "ammo_mg34" );
	UTIL_PrecacheOther( "ammo_30cal" );
	UTIL_PrecacheOther( "ammo_colt" );
	UTIL_PrecacheOther( "ammo_luger" );
	UTIL_PrecacheOther( "ammo_greasegun" );
	UTIL_PrecacheOther( "ammo_fg42" );
	UTIL_PrecacheOther( "ammo_k43" );
	UTIL_PrecacheOther( "ammo_enfield" );
	UTIL_PrecacheOther( "ammo_sten" );
	UTIL_PrecacheOther( "ammo_bren" );
	UTIL_PrecacheOther( "ammo_webley" );
	UTIL_PrecacheOther( "ammo_bazooka" );
	UTIL_PrecacheOther( "ammo_pschreck" );
	UTIL_PrecacheOther( "ammo_piat" );
	UTIL_PrecacheOther( "ammo_bazooka" );
	UTIL_PrecacheOther( "ammo_pschreck" );
	UTIL_PrecacheOther( "ammo_piat" );
	if ( g_pGameRules->IsDeathmatch() )
		UTIL_PrecacheOther( "weaponbox" ); // container for dropped deathmatch weapons
	g_sModelIndexFireball = PRECACHE_MODEL( "sprites/zerogxplode.spr" ); // fireball
	g_sModelIndexSmoke = PRECACHE_MODEL( "sprites/steam1.spr" ); // smoke
	g_sModelIndexBubbles = PRECACHE_MODEL( "sprites/bubble.spr" ); //bubbles
	g_sModelIndexBloodSpray = PRECACHE_MODEL( "sprites/bloodspray.spr" ); // initial blood
	g_sModelIndexBloodDrop = PRECACHE_MODEL( "sprites/blood-narrow.spr" );
	PRECACHE_MODEL( "sprites/explode1.spr" );
	m_usPlayWhizz = PRECACHE_EVENT( 1, "events/misc/whizz.sc" );
	PRECACHE_SOUND( "weapons/grenade_hit1.wav" ); //grenade
	PRECACHE_SOUND( "weapons/grenade_hit2.wav" ); //grenade
	PRECACHE_SOUND( "weapons/grenade_hit3.wav" ); //grenade
	PRECACHE_SOUND( "items/weapondrop.wav" );
}

void CBasePlayerItem::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector( -24, -24, 0 );
	pev->absmax = pev->origin + Vector( 24, 24, 16 );
}

int CBasePlayerItem::AddToPlayer( CBasePlayer* pPlayer )
{
	m_pPlayer = pPlayer;
	return TRUE;
}

void CBasePlayerItem::Holster( int skiplocal )
{
	m_pPlayer->pev->viewmodel = 0;
	m_pPlayer->pev->weaponmodel = 0;
}


//=========================================================
// Sets up movetype, size, solidtype for a new weapon. 
//=========================================================
void CBasePlayerItem::FallInit( void )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0) );//pointsize until it lands on the ground.

	SetUse( NULL );
	SetTouch( &CBasePlayerItem::DefaultTouch );

	m_vRespawnOrigin = pev->origin;
	m_vRespawnAngles = pev->angles;

	SetThink( &CBasePlayerItem::FallThink );

	pev->nextthink = UTIL_WeaponTimeBase() + 0.1;
}

void CBasePlayerItem::FallThink ( void )
{
	pev->nextthink = UTIL_WeaponTimeBase() + 0.1;

	if ( pev->flags & FL_ONGROUND )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		if ( !FNullEnt( pev->owner ) )
		{
			int pitch = 95 + RANDOM_LONG(0,29);
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "items/weapondrop.wav", 1, ATTN_NORM, 0, pitch);
		}

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
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}
	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin( this, pev->origin );
	SetUse( NULL );
	SetTouch( &CBasePlayerItem::DefaultTouch );
	m_bReadyToRespawn = FALSE;
	SetThink( NULL );
}

void CBasePlayerItem::AttemptToMaterialize()
{
	float delay = g_pGameRules->FlWeaponTryRespawn( this );
	if ( delay == 0 )
		Materialize();
	else
		pev->nextthink = UTIL_WeaponTimeBase() + delay;
}

//=========================================================
// CheckRespawn - a player is taking this weapon, should 
// it respawn?
//=========================================================
void CBasePlayerItem::CheckRespawn ( void )
{
	switch ( g_pGameRules->WeaponShouldRespawn( this ) )
	{
	case GR_WEAPON_RESPAWN_YES:
		Respawn();
		break;
	}
}

//=========================================================
// Respawn- this item is already in the world, but it is
// invisible and intangible. Make it visible and tangible.
//=========================================================
CBaseEntity* CBasePlayerItem::Respawn( void )
{
	// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	CBaseEntity *pNewWeapon = CBaseEntity::Create( (char *)STRING( pev->classname ), g_pGameRules->VecWeaponRespawnSpot( this ), pev->angles, pev->owner );

	if ( pNewWeapon )
	{
		pNewWeapon->pev->effects |= EF_NODRAW;// invisible for now
		pNewWeapon->SetTouch( NULL );// no touch
		pNewWeapon->SetThink( &CBasePlayerItem::AttemptToMaterialize );

		CBasePlayerItem *item = (CBasePlayerItem *)pNewWeapon;
		item->m_bReadyToRespawn = TRUE;

		DROP_TO_FLOOR ( ENT(pev) );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->pev->nextthink = g_pGameRules->FlWeaponRespawnTime( this );
	}
	else
	{
		ALERT ( at_console, "Respawn failed to create %s!\n", STRING( pev->classname ) );
	}

	return pNewWeapon;
}

void CBasePlayerItem::RoundRespawn()
{
	if ( m_bReadyToRespawn )
	{
		SetThink( &CBasePlayerItem::AttemptToMaterialize );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CBasePlayerItem::DefaultTouch( CBaseEntity *pOther )
{
	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	// can I have this?
	if ( !g_pGameRules->CanHavePlayerItem( pPlayer, this ) )
	{
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( this );
		}
		return;
	}

	if (pOther->AddPlayerItem( this ))
	{
		AttachToPlayer( pPlayer );
		EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/weaponpickup.wav", 1, ATTN_NORM);
	}

	SUB_UseTargets( pOther, USE_TOGGLE, 0 ); // UNDONE: when should this happen?
}

void CBasePlayerItem::PickupUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	// can I have this?
	if ( !g_pGameRules->CanHavePlayerItem( pPlayer, this ) )
	{
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( this );
		}
		return;
	}

	if (pActivator->AddPlayerItem( this ))
	{
		AttachToPlayer( pPlayer );
		EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/weaponpickup.wav", 1, ATTN_NORM);
	}
}

void CBasePlayerItem::AttachToPlayer( CBasePlayer* pPlayer )
{
	pev->movetype = MOVETYPE_FOLLOW;
	pev->solid = SOLID_NOT;
	pev->aiment = pPlayer->edict();
	pev->effects = EF_NODRAW; // ??
	pev->modelindex = 0; // server won't send down to clients if modelindex == 0
	pev->model = 0;
	pev->owner = pPlayer->edict();
	pev->nextthink = UTIL_WeaponTimeBase() + 0.1;
	SetTouch( NULL );
	ItemInfo info;
	GetItemInfo( &info );
	if ( !(info.iFlags & WEAPON_FLAG_OVERHEAT) )
		SetThink( NULL );
}

void CBasePlayerItem::SpawnReplacement( void )
{
	if ( m_bTriggerSpawnOnDie )
	{
		CBaseEntity *pEntity = UTIL_FindEntityByClassname( NULL, STRING( pev->classname ) );

		while ( pEntity )
		{
			CBasePlayerItem *pItem = (CBasePlayerItem *)pEntity;

			if ( pItem->m_bReadyToRespawn )
			{
				pItem->SetThink( &CBasePlayerItem::AttemptToMaterialize );
				pItem->pev->nextthink = gpGlobals->time + 0.1;
				pEntity = NULL;
			}
			else
			{
				pEntity = UTIL_FindEntityByClassname( pEntity, STRING( pev->classname ) );
			}
		}
	}
}

void CBasePlayerItem::Drop()
{
	SpawnReplacement();
	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = UTIL_WeaponTimeBase() + 0.1;
}

void CBasePlayerItem::Kill()
{
	SpawnReplacement();
	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = UTIL_WeaponTimeBase() + 0.1;
}

void CBasePlayerItem::DestroyItem()
{
	if ( m_pPlayer )
		m_pPlayer->RemovePlayerItem( this );
	Kill();
}

BOOL CBasePlayerWeapon::CanDeploy()
{
	return TRUE;
}

int CBasePlayerWeapon::Classify()
{
	return 0;
}

//=========================================================
// IsUseable - this function determines whether or not a 
// weapon is useable by the player in its current state. 
// (does it have ammo loaded? do I have any ammo for the 
// weapon?, etc)
//=========================================================
BOOL CBasePlayerWeapon :: IsUseable( void )
{
	if ( m_iClip <= 0 )
	{
		if ( iMaxAmmo1() != -1 && m_pPlayer->AmmoInventory( PrimaryAmmoIndex() ) <= 0 )
		{
			// clip is empty (or nonexistant) and the player has no more ammo of this type.
			return FALSE;
		}
	}

	return TRUE;
}

int CBasePlayerWeapon::AddToPlayer( CBasePlayer *pPlayer )
{
	int bResult = CBasePlayerItem::AddToPlayer( pPlayer );

	pPlayer->SetHasWeapon( m_iId );

	if ( !m_iPrimaryAmmoType )
	{
		m_iPrimaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo1() );
		m_iSecondaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo2() );
	}

	if (bResult)
		return AddWeapon( );
	return FALSE;
}

int CBasePlayerWeapon::AddDuplicate( CBasePlayerItem* pItem )
{
	if ( m_iDefaultAmmo )
		return ExtractAmmo( (CBasePlayerWeapon*)pItem );
	return ExtractClipAmmo( (CBasePlayerWeapon*)pItem );
}

BOOL CBasePlayerWeapon :: AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry )
{
	int iIdAmmo;

	if (iMaxClip < 1)
	{
		m_iClip = -1;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	}
	else if (m_iClip == 0)
	{
		int i;
		i = min( m_iClip + iCount, iMaxClip ) - m_iClip;
		m_iClip += i;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount - i, szName, iMaxCarry );
	}
	else
	{
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	}

	// m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = iMaxCarry; // hack for testing

	if (iIdAmmo > 0)
	{
		m_iPrimaryAmmoType = iIdAmmo;
		if (m_pPlayer->HasPlayerItem( this ) )
		{
			// play the "got ammo" sound only if we gave some ammo to a player that already had this gun.
			// if the player is just getting this gun for the first time, DefaultTouch will play the "picked up gun" sound for us.
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/ammopickup.wav", 1, ATTN_NORM);
		}
	}

	return iIdAmmo > 0 ? TRUE : FALSE;
}

BOOL CBasePlayerWeapon :: AddSecondaryAmmo( int iCount, char *szName, int iMax )
{
	int iIdAmmo;

	iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMax );

	//m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] = iMax; // hack for testing

	if (iIdAmmo > 0)
	{
		m_iSecondaryAmmoType = iIdAmmo;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/ammopickup.wav", 1, ATTN_NORM);
	}
	return iIdAmmo > 0 ? TRUE : FALSE;
}

//=========================================================
// called by the new item with the existing item as parameter
//
// if we call ExtractAmmo(), it's because the player is picking up this type of weapon for 
// the first time. If it is spawned by the world, m_iDefaultAmmo will have a default ammo amount in it.
// if  this is a weapon dropped by a dying player, has 0 m_iDefaultAmmo, which means only the ammo in 
// the weapon clip comes along. 
//=========================================================
int CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon *pWeapon )
{
	int			iReturn = FALSE;

	if ( pszAmmo1() != NULL )
	{
		// blindly call with m_iDefaultAmmo. It's either going to be a value or zero. If it is zero,
		// we only get the ammo in the weapon's clip, which is what we want.
		if ( m_iId == WEAPON_ENFIELD )
			iReturn = pWeapon->AddPrimaryAmmo( m_iDefaultAmmo, (char *)pszAmmo1(), 10, iMaxAmmo1() );
		else
			iReturn = pWeapon->AddPrimaryAmmo( m_iDefaultAmmo, (char *)pszAmmo1(), iMaxClip(), iMaxAmmo1() );
		m_iDefaultAmmo = 0;
	}

	if ( pszAmmo2() != NULL )
	{
		iReturn = pWeapon->AddSecondaryAmmo( 0, (char *)pszAmmo2(), iMaxAmmo2() );
	}

	return iReturn;
}

//=========================================================
// called by the new item's class with the existing item as parameter
//=========================================================
int CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon *pWeapon )
{
	int			iAmmo;

	if ( m_iClip == WEAPON_NOCLIP )
	{
		iAmmo = 0;// guns with no clips always come empty if they are second-hand
	}
	else
	{
		iAmmo = m_iClip;
	}

	return pWeapon->m_pPlayer->GiveAmmo( iAmmo, (char *)pszAmmo1(), iMaxAmmo1() ); // , &m_iPrimaryAmmoType
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

BOOL CBasePlayerWeapon::PlayEmptySound()
{
	if ( m_iPlayEmptySound )
		m_iPlayEmptySound = FALSE;
	return FALSE;
}

void CBasePlayerWeapon::ResetEmptySound()
{
	m_iPlayEmptySound = TRUE;
}

void CBasePlayerWeapon::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = FALSE; // cancel any reload in progress.
	m_fInAttack = FALSE;
	m_pPlayer->pev->viewmodel = 0;
	m_pPlayer->pev->weaponmodel = 0;
}

void CBasePlayerWeapon::RetireWeapon()
{
	m_pPlayer->pev->viewmodel = 0;
	m_pPlayer->pev->weaponmodel = 0;
	//m_pPlayer->pev->viewmodelindex = NULL;

	g_pGameRules->GetNextBestWeapon( m_pPlayer, this );
}

BOOL CBasePlayerWeapon::DefaultDeploy( char* viewModel, char* weaponModel, int anim,
	char* animExt, char* animExt2, int skiplocal )
{
	if ( !CanDeploy() )
		return FALSE;
	m_pPlayer->TabulateAmmo();
	m_pPlayer->pev->viewmodel = MAKE_STRING( viewModel );
	m_pPlayer->pev->weaponmodel = MAKE_STRING( weaponModel );
	strcpy( m_pPlayer->m_szAnimExtention, animExt );
	strcpy( m_pPlayer->m_szAnimReloadExt, animExt2 );
	m_pPlayer->m_iCurrentAnimationState = 0;
	m_pPlayer->m_fGunDropTime = 0;
	SendWeaponAnim( anim, skiplocal );
	m_pPlayer->SetAnimation( PLAYER_IDLE );
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1;
	return TRUE;
}

BOOL CBasePlayerWeapon::TimedDeploy( char* viewModel, char* weaponModel, int anim,
	char* animExt, char* animExt2, float idleTime, float attackTime, int skiplocal )
{
	if ( !CanDeploy() )
		return FALSE;
	m_pPlayer->TabulateAmmo();
	m_pPlayer->pev->viewmodel = MAKE_STRING( viewModel );
	m_pPlayer->pev->weaponmodel = MAKE_STRING( weaponModel );
	strcpy( m_pPlayer->m_szAnimExtention, animExt );
	strcpy( m_pPlayer->m_szAnimReloadExt, animExt2 );
	m_pPlayer->m_iCurrentAnimationState = 0;
	m_pPlayer->m_fGunDropTime = 0;
	SendWeaponAnim( anim, skiplocal );
	m_pPlayer->SetAnimation( PLAYER_IDLE );
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + attackTime;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + idleTime;
	return TRUE;
}

void CBasePlayerWeapon::SendWeaponAnim( int iAnim, int skiplocal )
{
	m_pPlayer->pev->weaponanim = iAnim;

	int team = m_pPlayer->pev->team;
	int playerclass = m_pPlayer->pev->playerclass;

	if ( skiplocal && ENGINE_CANSKIP( m_pPlayer->edict() ) )
		return;

	if ( team == 1 )
	{
		if ( playerclass >= PLAYERCLASS_BRIT_LIGHTINF && playerclass <= PLAYERCLASS_BRIT_MORTAR )
			pev->body = 1;
		else
			pev->body = 0;
	}
	else
		pev->body = 2;

	MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, m_pPlayer->pev );
		WRITE_BYTE( iAnim );						// sequence number
		WRITE_BYTE( pev->body );					// weaponmodel bodygroup.
	MESSAGE_END();
}

BOOL CBasePlayerWeapon::DefaultReload( int iClipSize, int iAnim, float fDelay )
{
	int ammo = m_pPlayer->AmmoInventory( m_iPrimaryAmmoType );
	if ( ammo <= 0 )
		return FALSE;

	int j;

	if ( m_iId == WEAPON_ENFIELD )
		j = min( iClipSize * 2 - m_iClip, ammo );
	else
		j = min( iClipSize - m_iClip, ammo );

	if ( j <= 0 )
		return FALSE;

	if ( m_iClip >= ammo )
		return FALSE;

	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/reloadgarand.wav", RANDOM_FLOAT( 0.8f, 0.9f ), ATTN_NORM );

	m_pPlayer->SetAnimation( PLAYER_RELOAD );

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + fDelay;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + fDelay;

	//!!UNDONE -- reload sound goes here !!!
	SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );

	m_fInReload = TRUE;

	ItemInfo ii;
	GetItemInfo( &ii );

	if ( ii.iFlags & WEAPON_FLAG_OVERHEAT )
		m_flWeaponHeat = 0;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + max( 3.0f, fDelay );
	return TRUE;
}

void CBasePlayerWeapon::ItemPostFrame( void )
{
	if ( m_pPlayer->pev->flags & FL_IMMUNE_LAVA )
		return;

	if ( m_fInReload && UTIL_WeaponTimeBase() >= m_flNextPrimaryAttack &&
		( m_iId != WEAPON_HANDGRENADE || m_iId != WEAPON_HANDGRENADE_EX || m_iId != WEAPON_STICKGRENADE || m_iId != WEAPON_STICKGRENADE_EX ) )
	{
		// complete the reload.
		int ammo = m_pPlayer->AmmoInventory( m_iPrimaryAmmoType );
		int j = 0;

		if ( ammo > 0 )
		{
			if ( m_iId == WEAPON_ENFIELD )
			{
				int i;
				i = min( 5, ammo );
				(void)i;
				j = min( 10 - m_iClip, 5 );
			}
			else
			{
				j = min( iMaxClip() - m_iClip, ammo );
			}
		}

		if ( Classify() == WEAPON_CLASS_ROCKET && ( m_iWeaponState & WEAPON_STATE_RELOAD_DEPLOY ) )
		{
			CRocketWeapon *pRocketWpn = (CRocketWeapon *)this;

			if ( pRocketWpn )
			{
				m_iWeaponState &= ~WEAPON_STATE_RELOAD_DEPLOY;
				pRocketWpn->ReSlow();
			}
		}

		// Add them to the clip
		m_iClip += j;

		MESSAGE_BEGIN( MSG_ONE, gmsgReloadDone, NULL, m_pPlayer->pev );
		MESSAGE_END();

		if ( ammo > 0 )
			m_pPlayer->SetAmmoInventory( m_iPrimaryAmmoType, ammo - j );

		m_pPlayer->TabulateAmmo();

		m_fInReload = FALSE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
	}

	if ( m_fInAttack && !( m_pPlayer->pev->button & IN_ATTACK ) )
		m_fInAttack = FALSE;

	ItemInfo ii;
	GetItemInfo( &ii );

	if ( ( m_pPlayer->pev->button & IN_ATTACK2 ) && UTIL_WeaponTimeBase() >= m_flNextSecondaryAttack )
	{
		if ( pszAmmo2() && m_pPlayer->AmmoInventory( SecondaryAmmoIndex() ) <= 0 )
		{
			m_fFireOnEmpty = TRUE;
		}

		m_pPlayer->TabulateAmmo();

		if ( GetRoundState() == 1 )
			SecondaryAttack();

		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ( ( m_pPlayer->pev->button & IN_ATTACK ) &&
		( m_iId == WEAPON_HANDGRENADE || m_iId == WEAPON_HANDGRENADE_EX || m_iId == WEAPON_STICKGRENADE || m_iId == WEAPON_STICKGRENADE_EX ) &&
		m_flNextPrimaryAttack <= gpGlobals->time )
	{
		m_pPlayer->TabulateAmmo();

		if ( GetRoundState() == 1 )
			PrimaryAttack();
	}
	else if ( ( m_pPlayer->pev->button & IN_ATTACK ) && UTIL_WeaponTimeBase() >= m_flNextPrimaryAttack )
	{
		if ( ( m_iClip == 0 && pszAmmo1() ) || ( iMaxClip() == -1 && pszAmmo1() && m_pPlayer->AmmoInventory( PrimaryAmmoIndex() ) <= 0 ) )
		{
			m_fFireOnEmpty = TRUE;
		}

		m_pPlayer->TabulateAmmo();

		if ( GetRoundState() == 1 )
			PrimaryAttack();
	}
	else if ( m_pPlayer->pev->button & IN_RELOAD && iMaxClip() != WEAPON_NOCLIP && !m_fInReload )
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		if ( m_pPlayer->pev->waterlevel <= 2 )
			Reload();
	}
	else if ( !( m_pPlayer->pev->button & ( IN_ATTACK | IN_ATTACK2 ) ) )
	{
		// no fire buttons down
		m_fFireOnEmpty = FALSE;

		// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
		if ( m_iClip == 0 && !( iFlags() & ITEM_FLAG_NOAUTORELOAD ) && m_flNextPrimaryAttack < 0.0 && m_pPlayer->m_bAutoReload &&
			( ( !m_pPlayer->IsProneDeployed() && !m_pPlayer->IsSandbagDeployed() ) || Classify() != WEAPON_CLASS_MACHINEGUN ) &&
			m_pPlayer->pev->waterlevel <= 2 )
		{
			Reload();
			return;
		}

		WeaponIdle();
		return;
	}

	// catch all
	if ( ShouldWeaponIdle() )
	{
		WeaponIdle();
	}
}

int CBasePlayerWeapon::UpdateClientData( CBasePlayer *pPlayer )
{
	BOOL bSend = FALSE;
	int state = 0;

	if ( pPlayer->m_pActiveItem == this )
		state = 1;

	// Forcing send of all data!
	if ( !pPlayer->m_fWeapon )
	{
		bSend = TRUE;
	}

	// This is the current or last weapon, so the state will need to be updated
	if ( this == pPlayer->m_pActiveItem ||
		 this == pPlayer->m_pClientActiveItem )
	{
		if ( pPlayer->m_pActiveItem != pPlayer->m_pClientActiveItem )
		{
			bSend = TRUE;
		}
	}

	// If the ammo, state, or fov has changed, update the weapon
	if ( m_iClip != m_iClientClip ||
		 state != m_iClientWeaponState ||
		 pPlayer->m_iFOV != pPlayer->m_iClientFOV )
	{
		bSend = TRUE;
	}

	if ( bSend )
	{
		int clip = m_iClip;

		if ( clip == -1 )
		{
			int ammoIndex = CBasePlayer::GetAmmoIndex( pszAmmo1() );
			clip = m_pPlayer->AmmoInventory( ammoIndex );
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pPlayer->pev );
			WRITE_BYTE( state );
			WRITE_BYTE( m_iId );
			WRITE_BYTE( clip );
		MESSAGE_END();

		m_iClientClip = m_iClip;
		m_iClientWeaponState = state;
		pPlayer->m_fWeapon = TRUE;
	}

	if ( m_pNext )
		m_pNext->UpdateClientData( pPlayer );

	return 1;
}

bool CBasePlayerWeapon::PlayerIsWaterSniping( void )
{
	if ( m_pPlayer->pev->waterlevel == 3 || ( m_pPlayer->pev->waterlevel > 0 && m_pPlayer->IsProne() ) )
		return true;

	return false;
}

Vector CBasePlayerWeapon::Aim( float recoil, CBasePlayer* pPlayer, int bulletType )
{
	float x = UTIL_SharedRandomFloat( bulletType, recoil, 0 );
	float y = UTIL_SharedRandomFloat( bulletType, recoil, 0 );
	float z = UTIL_SharedRandomFloat( bulletType, recoil, 0 );
	return Vector( x, y, z );
}

float CBasePlayerWeapon::flAim( float spread, CBasePlayer* pPlayer )
{
	return spread;
}

void CBasePlayerWeapon::RemoveStamina( float removeAmount, CBasePlayer *pOther )
{
	if ( !( pOther->pev->button & IN_DUCK ) && !pOther->IsProne() )
		pOther->SetStamina( pOther->GetStamina() - removeAmount );
}

int CBasePlayerWeapon::GetFOV()
{
	return m_pPlayer->m_iFOV;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4701 6001 )
#endif
int CBasePlayerWeapon::ChangeFOV( int fov )
{
	int newFov;

	if ( m_pPlayer->m_iFOV <= 0 )
	{
		newFov = fov;
	}
	else if ( m_pPlayer->m_iFOV > 0 )
	{
		newFov = 0;
		m_pPlayer->UpdateScope( 0 );
	}

	return newFov;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

int CBasePlayerWeapon::ZoomOut()
{
	if ( m_pPlayer->pev->vuser1.z != 0 )
		return m_pPlayer->m_iFOV;
	m_pPlayer->CheckPlayerSpeed();
	m_pPlayer->UpdateScope( 0 );
	return 0;
}

int CBasePlayerWeapon::ZoomIn()
{
	if ( Classify() == WEAPON_CLASS_SCOPED || Classify() == WEAPON_CLASS_MORTAR )
	{
		m_pPlayer->UpdateScope( m_iId );
		return 20;
	}
	return 0;
}

void CBasePlayerWeapon::UpdateZoomSpeed()
{
	if ( Classify() == WEAPON_CLASS_SCOPED )
		m_pPlayer->CheckPlayerSpeed();
}

void CBasePlayerWeapon::ThinkZoomOut()
{
	m_pPlayer->m_iFOV = ZoomOut();
	UpdateZoomSpeed();
}

void CBasePlayerWeapon::ThinkZoomIn()
{
	if ( m_pPlayer->pev->velocity.Length() <= 45 && !(m_pPlayer->pev->button & IN_ATTACK) )
	{
		m_pPlayer->m_iFOV = ZoomIn();
		UpdateZoomSpeed();
	}
}

int CBasePlayerWeapon::GetRoundState()
{
	return g_pGameRules->RoundState();
}

BOOL CanAttack( float attack_time, float curtime, BOOL isPredicted )
{
	if ( !isPredicted )
	{
		return ( attack_time <= curtime ) ? TRUE : FALSE;
	}
	else
	{
		return ( attack_time <= 0.0 ) ? TRUE : FALSE;
	}
}

void CBasePlayerWeapon::PostMortarValue( float value )
{
}

void CBasePlayerWeapon::SendMortarFireCommand( char* command )
{
}

void CBasePlayerWeapon::PrintState()
{
	ALERT( at_console, "primary:  %f\n", m_flNextPrimaryAttack );
	ALERT( at_console, "idle   :  %f\n", m_flTimeWeaponIdle );

//	ALERT( at_console, "nextrl :  %f\n", m_flNextReload );
//	ALERT( at_console, "nextpum:  %f\n", m_flPumpTime );

//	ALERT( at_console, "m_frt  :  %f\n", m_fReloadTime );
	ALERT( at_console, "m_finre:  %i\n", m_fInReload );
	ALERT( at_console, "m_iclip:  %i\n", m_iClip );
	ALERT( at_console, "m_iWeaponState:  %i\n", m_iWeaponState );
}

void CBasePlayerAmmo::Spawn()
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 16 ) );
	UTIL_SetOrigin( this, pev->origin );
	SetTouch( &CBasePlayerAmmo::DefaultTouch );
}

void CBasePlayerAmmo::Materialize()
{
	if ( pev->effects & EF_NODRAW )
	{
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}
	SetTouch( &CBasePlayerAmmo::DefaultTouch );
}

CBaseEntity* CBasePlayerAmmo::Respawn()
{
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );
	UTIL_SetOrigin( this, g_pGameRules->VecAmmoRespawnSpot( this ) ); // move to wherever I'm supposed to repawn.
	SetThink( &CBasePlayerAmmo::Materialize );
	pev->nextthink = g_pGameRules->FlAmmoRespawnTime( this );
	return this;
}

void CBasePlayerAmmo :: DefaultTouch( CBaseEntity *pOther )
{
	if ( !( pev->flags & FL_ONGROUND ) )
		return;

	if ( !pOther->IsPlayer() )
		return;

	if ( pev->team && pev->team != pOther->pev->team )
		return;

	if ( !FNullEnt( pev->owner ) && pev->owner == pOther->edict() )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)pOther;

		if ( pPlayer )
			pPlayer->m_nGenericAmmoCount++;

		EMIT_SOUND( pOther->edict(), CHAN_ITEM, "items/weaponpickup.wav", 1, ATTN_NORM );
		SetTouch( NULL );
		UTIL_Remove( this );
	}
	else if ( AddAmmo( pOther ) )
	{
		if ( g_pGameRules->AmmoShouldRespawn( this ) == GR_AMMO_RESPAWN_YES )
		{
			Respawn();
		}
		else
		{
			SetTouch( NULL );
			SetThink( &CBaseEntity::SUB_Remove );
			pev->nextthink = UTIL_WeaponTimeBase() + 0.1;
		}
	}
	else if ( gEvilImpulse101 )
	{
		// evil impulse 101 hack, kill always
		SetTouch( NULL );
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = UTIL_WeaponTimeBase() + 0.1;
	}
}

bool CBasePlayerAmmo::GiveGenericAmmo( CBaseEntity *pOther )
{
	bool bAmmoGiven = false;

	if ( !pOther->IsPlayer() )
		return bAmmoGiven;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		CBasePlayerItem *pItem = pPlayer->m_rgpPlayerItems[i];

		while ( pItem )
		{
			if ( pItem->Classify() & 0x3f0 )
			{
				ItemInfo ii;
				pItem->GetItemInfo( &ii );

				int iGive = ii.iMaxClip;

				if ( iGive == WEAPON_NOCLIP )
					iGive = P_WpnInfo[ii.iId].ammo_default;

				bAmmoGiven = ( pPlayer->GiveAmmo( iGive, (char *)ii.pszAmmo1, ii.iMaxAmmo1 ) != -1 );
				return bAmmoGiven;
			}

			pItem = pItem->m_pNext;
		}
	}

	return bAmmoGiven;
}

LINK_ENTITY_TO_CLASS( ammo_generic_american, GenericAmmoAmerican );
LINK_ENTITY_TO_CLASS( ammo_generic_german, GenericAmmoGerman );
LINK_ENTITY_TO_CLASS( ammo_generic_british, GenericAmmoBritish );
LINK_ENTITY_TO_CLASS( dod_ammo, CDodAmmoBox );

void CDodAmmoBox::Precache()
{
	PRECACHE_MODEL( "models/allied_ammo.mdl" );
	PRECACHE_MODEL( "models/axis_ammo.mdl" );
}

void CDodAmmoBox::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	SET_MODEL( ENT(pev), "models/allied_ammo.mdl");
}

void CDodAmmoBox::KillThink()
{
	UTIL_Remove( this );
}

void CDodAmmoBox::Touch( CBaseEntity *pOther )
{
	if ( !(pev->flags & FL_ONGROUND ) )
		return;

	if ( !pOther->IsPlayer() )
		return;

	if ( !pOther->IsAlive() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	if ( pPlayer == (CBaseEntity *)m_hOwner )
	{
		pPlayer->m_nGenericAmmoCount++;
	}
	else
	{
		CBasePlayerWeapon *pWeapon = pPlayer->GetPrimaryWeapon();

		if ( !pWeapon )
			return;

		int iAmmoIndex = CBasePlayer::GetAmmoIndex( pWeapon->pszAmmo1() );

		ItemInfo ii;
		pWeapon->GetItemInfo( &ii );

		int iClips = 0;

		switch ( pWeapon->Classify() )
		{
		case 16:
		case WEAPON_CLASS_SCOPED:
		case 64:
			iClips = 2;
			break;
		case WEAPON_CLASS_MACHINEGUN:
			iClips = 1;
			break;
		case WEAPON_CLASS_ROCKET:
			iClips = 1;
			break;
		default:
			return;
		}

		while ( iClips > 0 )
		{
			int iCurAmmo = pPlayer->AmmoInventory( iAmmoIndex );
			if ( iCurAmmo >= 0 && iCurAmmo + ii.iMaxClip <= ii.iMaxAmmo1 )
				pPlayer->SetAmmoInventory( iAmmoIndex, iCurAmmo + ii.iMaxClip );

			iClips--;
		}

		CBaseEntity *pOwnerEnt = m_hOwner;
		CBasePlayer *pOwner = (CBasePlayer *)pOwnerEnt;
		pOwner->AddObjScore( 1, false );
	}

	EMIT_SOUND( pOther->edict(), CHAN_ITEM, "items/weaponpickup.wav", 1, ATTN_NORM );
	SetTouch( NULL );
	UTIL_Remove( this );
}


//*********************************************************
// weaponbox code:
//*********************************************************

LINK_ENTITY_TO_CLASS( weaponbox, CWeaponBox );

void CWeaponBox::Precache()
{
	PRECACHE_MODEL( "models/allied_ammo.mdl" );
	PRECACHE_MODEL( "models/axis_ammo.mdl" );
}

//=========================================================
// CWeaponBox - Spawn 
//=========================================================
void CWeaponBox::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	m_bTouch = true;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	SET_MODEL( ENT(pev), "models/allied_ammo.mdl");
}

void CWeaponBox::SetObjectCollisionBox()
{
	pev->absmin = pev->origin + Vector( -16, -16, 0 );
	pev->absmax = pev->origin + Vector( 16, 16, 16 );
}

//=========================================================
//=========================================================
void CWeaponBox::KeyValue( KeyValueData *pkvd )
{
	if ( m_cAmmoTypes < MAX_AMMO_SLOTS )
	{
		PackAmmo( ALLOC_STRING(pkvd->szKeyName), atoi(pkvd->szValue) );
		m_cAmmoTypes++;// count this new ammo type.

		pkvd->fHandled = TRUE;
	}
	else
	{
		ALERT ( at_console, "WeaponBox too full! only %d ammotypes allowed\n", MAX_AMMO_SLOTS );
	}
}

//=========================================================
// CWeaponBox - Touch: try to add my contents to the toucher
// if the toucher is a player.
//=========================================================
void CWeaponBox::Touch( CBaseEntity *pOther )
{
	if ( !(pev->flags & FL_ONGROUND ) )
	{
		return;
	}

	if ( !pOther->IsPlayer() )
	{
		// only players may touch a weaponbox.
		return;
	}

	if ( !pOther->IsAlive() )
	{
		// no dead guys.
		return;
	}

	if ( !m_bTouch )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	bool itemsGiven = false;
	int i;

	// go through my weapons and try to give the usable ones to the player.
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		CBasePlayerItem *pItem = m_rgpPlayerItems[ i ];

		while ( pItem )
		{
			CBasePlayerItem *pNext = pItem->m_pNext;

			if ( g_pGameRules->CanHavePlayerItem( pPlayer, pItem ) )
			{
				if ( pPlayer->AddPlayerItem( pItem ) )
				{
					pItem->AttachToPlayer( pPlayer );
				}

				m_rgpPlayerItems[ i ] = m_rgpPlayerItems[ i ]->m_pNext;// unlink this weapon from the box
				pItem = m_rgpPlayerItems[ i ];
				itemsGiven = true;
			}

			pItem = pNext;
		}
	}

	if ( !itemsGiven )
		return;

	// dole out ammo
	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			int iGiven;

			// there's some ammo of this type.
			iGiven = pPlayer->GiveAmmo( m_rgAmmo[ i ], (char *)STRING( m_rgiszAmmo[ i ] ), m_rgAmmo[ i ] );

			//ALERT ( at_console, "Gave %d rounds of %s\n", m_rgAmmo[i], STRING(m_rgiszAmmo[i]) );

			// now empty the ammo from the weaponbox since we just gave it to the player
			m_rgiszAmmo[ i ] = iStringNull;
			m_rgAmmo[ i ] = 0;
		}
	}

	EMIT_SOUND( pOther->edict(), CHAN_ITEM, "items/weaponpickup.wav", 1, ATTN_NORM );
	SetTouch(NULL);
	UTIL_Remove(this);
}

//=========================================================
// CWeaponBox - Kill - the think function that removes the
// box from the world.
//=========================================================
void CWeaponBox::Kill( void )
{
	ALERT( at_aiconsole, "weapon box killed.\n" );

	CBasePlayerItem *pWeapon;
	int i;

	// destroy the weapons
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pWeapon = m_rgpPlayerItems[ i ];

		while ( pWeapon )
		{
			pWeapon->SpawnReplacement();
			pWeapon->SetThink(&CBaseEntity::SUB_Remove);
			pWeapon->pev->nextthink = UTIL_WeaponTimeBase() + 0.1;
			pWeapon = pWeapon->m_pNext;
		}
	}

	// remove the box
	UTIL_Remove( this );
}

void CWeaponBox::RoundRespawn()
{
	Kill();
}

void CWeaponBox::ActivateThink()
{
	m_bTouch = true;
	SetThink( &CWeaponBox::Kill );
	pev->nextthink = gpGlobals->time + 20;
}

//=========================================================
// CWeaponBox::HasWeapon - is a weapon of this type already
// packed in this box?
//=========================================================
BOOL CWeaponBox::HasWeapon( CBasePlayerItem *pCheckItem )
{
	CBasePlayerItem *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while (pItem)
	{
		if (FClassnameIs( pItem->pev, STRING( pCheckItem->pev->classname) ))
		{
			return TRUE;
		}
		pItem = pItem->m_pNext;
	}

	return FALSE;
}

//=========================================================
// CWeaponBox - PackWeapon: Add this weapon to the box
//=========================================================
BOOL CWeaponBox::PackWeapon( CBasePlayerItem *pWeapon )
{
	// is one of these weapons already packed in this box?
	if ( HasWeapon( pWeapon ) )
	{
		return FALSE;// box can only hold one of each weapon type
	}

	if ( pWeapon->m_pPlayer )
	{
		if ( pWeapon->m_pPlayer->m_pActiveItem == pWeapon )
		{
			pWeapon->Holster();
		}

		if ( !pWeapon->m_pPlayer->RemovePlayerItem( pWeapon ) )
		{
			// failed to unhook the weapon from the player!
			return FALSE;
		}
	}

	int iWeaponSlot = pWeapon->iItemSlot();

	if ( m_rgpPlayerItems[ iWeaponSlot ] )
	{
		// there's already one weapon in this slot, so link this into the slot's column
		pWeapon->m_pNext = m_rgpPlayerItems[ iWeaponSlot ];
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
	}
	else
	{
		// first weapon we have for this slot
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
		pWeapon->m_pNext = NULL;
	}

	pWeapon->pev->spawnflags |= SF_NORESPAWN;// never respawn
	pWeapon->pev->movetype = MOVETYPE_NONE;
	pWeapon->pev->solid = SOLID_NOT;
	pWeapon->pev->effects = EF_NODRAW;
	pWeapon->pev->modelindex = 0;
	pWeapon->pev->model = iStringNull;
	pWeapon->pev->owner = edict();

	ItemInfo ii;
	pWeapon->GetItemInfo( &ii );

	if ( !( ii.iFlags & WEAPON_FLAG_OVERHEAT ) )
		pWeapon->SetThink( NULL );// crowbar may be trying to swing again, etc.

	pWeapon->SetTouch( NULL );
	pWeapon->m_pPlayer = NULL;

	//ALERT ( at_console, "packed %s\n", STRING(pWeapon->pev->classname) );

	return TRUE;
}

//=========================================================
// CWeaponBox - PackAmmo
//=========================================================
BOOL CWeaponBox::PackAmmo( int iszName, int iCount )
{
	int iMaxCarry;

	if ( FStringNull( iszName ) )
	{
		// error here
		ALERT ( at_console, "NULL String in PackAmmo!\n" );
		return FALSE;
	}

	iMaxCarry = iCount;

	if ( iMaxCarry != -1 && iCount > 0 )
	{
		//ALERT ( at_console, "Packed %d rounds of %s\n", iCount, STRING(iszName) );
		GiveAmmo( iCount, (char *)STRING( iszName ), iMaxCarry );
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CWeaponBox - GiveAmmo
//=========================================================
int CWeaponBox::GiveAmmo( int iCount, char *szName, int iMax, int *pIndex/* = NULL*/ )
{
	int i;

	for (i = 1; i < MAX_AMMO_SLOTS && !FStringNull( m_rgiszAmmo[i] ); i++)
	{
		if (stricmp( szName, STRING( m_rgiszAmmo[i])) == 0)
		{
			if (pIndex)
				*pIndex = i;

			int iAdd = min( iCount, iMax - m_rgAmmo[i]);
			if (iCount == 0 || iAdd > 0)
			{
				m_rgAmmo[i] += iAdd;

				return i;
			}
			return -1;
		}
	}
	if (i < MAX_AMMO_SLOTS)
	{
		if (pIndex)
			*pIndex = i;

		m_rgiszAmmo[i] = MAKE_STRING( szName );
		m_rgAmmo[i] = iCount;

		return i;
	}
	ALERT( at_console, "out of named ammo slots\n");
	return i;
}

//=========================================================
// CWeaponBox::IsEmpty - is there anything in this box?
//=========================================================
BOOL CWeaponBox::IsEmpty( void )
{
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			return FALSE;
		}
	}

	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			// still have a bit of this type of ammo
			return FALSE;
		}
	}

	return TRUE;
}
