/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
==========================
This file contains "stubs" of class member implementations so that we can predict certain
 weapons client side.  From time to time you might find that you need to implement part of the
 these functions.  If so, cut it from here, paste it in hl_weapons.cpp or somewhere else and
 add in the functionality you need.
==========================
*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

class CGameRules;
CGameRules* g_pGameRules;
const Vector g_vecZero( 0, 0, 0 );
int gmsgWeapPickup = 0;
enginefuncs_t g_engfuncs;
globalvars_t* gpGlobals;
ItemInfo CBasePlayerItem::ItemInfoArray[MAX_WEAPONS];

Vector PGetGunPosition( Vector origin );

void EMIT_SOUND_DYN( edict_t* entity, int channel, const char* sample, float volume, float attenuation, int flags, int pitch )
{
}

void DBG_AssertFunction( BOOL expression, const char* text, const char* file, int line, const char* message )
{
}

void UTIL_LogPrintf( char* format, ... )
{
}

void UTIL_ClientPrintAll( int destination, const char* name, const char* first, const char* second, const char* third, const char* fourth )
{
}

void ClientPrint( entvars_t* client, int destination, const char* name, const char* first, const char* second, const char* third, const char* fourth )
{
}

void UTIL_TraceHull( const Vector& start, const Vector& end, IGNORE_MONSTERS ignore, int hull, edict_t* entity, TraceResult* trace )
{
}

BOOL UTIL_IsValidEntity( edict_t* entity )
{
	return TRUE;
}

CBaseEntity* UTIL_PlayerByIndex( int index )
{
	return NULL;
}

CBaseEntity* UTIL_FindEntityByClassname( CBaseEntity* start, const char* name )
{
	return NULL;
}

CBaseEntity* FindEntityForward( CBaseEntity* entity )
{
	return NULL;
}

Vector UTIL_GetAimVector( edict_t* entity, float delta )
{
	return g_vecZero;
}

float TEXTURETYPE_PlaySound( TraceResult* trace, Vector source, Vector end, int bulletType )
{
	return 0;
}

int TrainSpeed( int speed, int maximum )
{
	return 0;
}

void ClearMultiDamage()
{
}

void ApplyMultiDamage( entvars_t* inflictor, entvars_t* attacker )
{
}

void AddMultiDamage( entvars_t* inflictor, CBaseEntity* entity, float damage, int damageType )
{
}

void SpawnBlood( Vector position, int color, float damage )
{
}

int DamageDecal( CBaseEntity* entity, int damageType )
{
	return 0;
}

void EjectBrass( const Vector& origin, const Vector& velocity, float rotation, int model, int soundType )
{
}

void DecalGunshot( TraceResult* trace, int bulletType )
{
}

void DecalGunshot( TraceResult* trace, int bulletType, bool clientOnly, entvars_t* shooter, bool hitMetal )
{
}

edict_t* DBG_EntOfVars( const entvars_t* vars )
{
	return NULL;
}

void UTIL_PrecacheOther( const char* className )
{
}

void UTIL_BloodDrips( const Vector& origin, const Vector& direction, int color, int amount )
{
}

void UTIL_DecalTrace( TraceResult* trace, int decalNumber )
{
}

void UTIL_GunshotDecalTrace( TraceResult* trace, int decalNumber )
{
}

void UTIL_SetOrigin( entvars_t* vars, const Vector& origin )
{
}

void Broadcast( const char* message )
{
}

void AddAmmoNameToAmmoRegistry( const char* name )
{
}

int CBaseEntity::TakeHealth( float health, int damageType )
{
	return 1;
}

int CBaseEntity::TakeDamage( entvars_t* inflictor, entvars_t* attacker, float damage, int damageType )
{
	return 1;
}

void CBaseEntity::TraceAttack( entvars_t* attacker, float damage, Vector direction, TraceResult* trace, int damageType )
{
}

void CBaseEntity::TraceBleed( float damage, Vector direction, TraceResult* trace, int damageType )
{
}

int CBaseEntity::Save( CSave& save )
{
	return 1;
}

int CBaseEntity::Restore( CRestore& restore )
{
	return 1;
}

void CBaseEntity::SetObjectCollisionBox()
{
}

int CBaseEntity::Intersects( CBaseEntity* other )
{
	return 0;
}

void CBaseEntity::MakeDormant()
{
}

int CBaseEntity::IsDormant()
{
	return 0;
}

void CBaseEntity::SUB_Remove()
{
}

BOOL CBaseEntity::IsInWorld()
{
	return TRUE;
}

int CBaseEntity::ShouldToggle( USE_TYPE useType, BOOL currentState )
{
	return 0;
}

int CBaseEntity::DamageDecal( int damageType )
{
	return -1;
}

CBaseEntity* CBaseEntity::GetNextTarget()
{
	return NULL;
}

CBaseEntity* CBaseEntity::Create( char* name, const Vector& origin, const Vector& angles, edict_t* owner )
{
	return NULL;
}

BOOL CBaseEntity::FVisible( CBaseEntity* entity )
{
	return FALSE;
}

BOOL CBaseEntity::FVisible( const Vector& origin )
{
	return FALSE;
}
void CBaseEntity::FireBullets( ULONG shots, Vector source, Vector direction, Vector spread, float distance,
	int bulletType, int tracerFrequency, int damage, entvars_t* attacker )
{
}

void CBaseDelay::KeyValue( KeyValueData* data )
{
}

int CBaseDelay::Save( CSave& save )
{
	return 1;
}

int CBaseDelay::Restore( CRestore& restore )
{
	return 1;
}

void CBaseToggle::KeyValue( KeyValueData* data )
{
}

int CBaseToggle::Save( CSave& save )
{
	return 1;
}

int CBaseToggle::Restore( CRestore& restore )
{
	return 1;
}

int CBaseAnimating::Save( CSave& save )
{
	return 1;
}

int CBaseAnimating::Restore( CRestore& restore )
{
	return 1;
}

float CBaseAnimating::StudioFrameAdvance( float interval )
{
	return 0;
}

void CBaseAnimating::DispatchAnimEvents( float interval )
{
}

float CBaseAnimating::SetBlending( int blender, float value )
{
	return 0;
}

void CBaseAnimating::GetAttachment( int attachment, Vector& origin, Vector& angles )
{
}

int CBaseAnimating::FindTransition( int endingSequence, int goalSequence, int* direction )
{
	return -1;
}

void CBaseAnimating::GetAutomovement( Vector& origin, Vector& angles, float interval )
{
}

void CBaseAnimating::SetBodygroup( int group, int value )
{
}

int CBaseAnimating::LookupActivity( int activity )
{
	return 0;
}

int CBaseAnimating::LookupActivityHeaviest( int activity )
{
	return 0;
}

int CBaseAnimating::LookupSequence( const char* label )
{
	return 0;
}

void CBaseAnimating::ResetSequenceInfo()
{
}

int CBaseAnimating::GetSequenceFlags()
{
	return 0;
}

float CBaseAnimating::SetBoneController( int controller, float value )
{
	return 0;
}

void CBaseAnimating::InitBoneControllers()
{
}

void CBaseAnimating::GetBonePosition( int bone, Vector& origin, Vector& angles )
{
}

int CBaseAnimating::GetBodygroup( int group )
{
	return 0;
}

void CBaseMonster::KeyValue( KeyValueData* data )
{
}

void CBaseMonster::TraceAttack( entvars_t* attacker, float damage, Vector direction, TraceResult* trace, int damageType )
{
}

int CBaseMonster::TakeDamage( entvars_t* inflictor, entvars_t* attacker, float damage, int damageType )
{
	return 0;
}

int CBaseMonster::TakeHealth( float health, int damageType )
{
	return 0;
}

void CBaseMonster::Killed( entvars_t* attacker, int gib )
{
}

float CBaseMonster::ChangeYaw( int speed )
{
	return 0;
}

BOOL CBaseMonster::HasHumanGibs()
{
	return FALSE;
}

BOOL CBaseMonster::HasAlienGibs()
{
	return FALSE;
}

void CBaseMonster::FadeMonster()
{
}

void CBaseMonster::GibMonster()
{
}

Activity CBaseMonster::GetDeathActivity()
{
	return ACT_DIE_HEADSHOT;
}

void CBaseMonster::BecomeDead()
{
}

BOOL CBaseMonster::ShouldFadeOnDeath()
{
	return FALSE;
}

int CBaseMonster::IRelationship( CBaseEntity* target )
{
	return 0;
}

void CBaseMonster::ReportAIState()
{
}

void CBaseMonster::MonsterInitDead()
{
}

void CBaseMonster::Look( int distance )
{
}

CBaseEntity* CBaseMonster::BestVisibleEnemy()
{
	return NULL;
}

BOOL CBaseMonster::FInViewCone( CBaseEntity* entity )
{
	return FALSE;
}

BOOL CBaseMonster::FInViewCone( Vector* origin )
{
	return FALSE;
}

void CBaseMonster::MakeIdealYaw( Vector target )
{
}

void CBaseMonster::RadiusDamage( entvars_t* inflictor, entvars_t* attacker, float damage, int ignoreClass, int damageType )
{
}

void CBaseMonster::RadiusDamage( Vector source, entvars_t* inflictor, entvars_t* attacker, float damage, int ignoreClass, int damageType )
{
}

CBaseEntity* CBaseMonster::CheckTraceHullAttack( float distance, int damage, int damageType )
{
	return NULL;
}

void CBaseMonster::MakeDamageBloodDecal( int count, float noise, TraceResult* trace, const Vector& direction )
{
}

BOOL CBaseMonster::FCheckAITrigger()
{
	return FALSE;
}

void CBaseMonster::CorpseFallThink()
{
}

int CBasePlayer::TakeHealth( float health, int damageType )
{
	return 0;
}

int CBasePlayer::TakeDamage( entvars_t* inflictor, entvars_t* attacker, float damage, int damageType )
{
	return 0;
}

void CBasePlayer::TraceAttack( entvars_t* attacker, float damage, Vector direction, TraceResult* trace, int damageType )
{
}

BOOL CBasePlayer::FBecomeProne()
{
	return TRUE;
}

int CBasePlayer::Illumination()
{
	return 0;
}

void CBasePlayer::RoundRespawn()
{
}

void CBasePlayer::SetAnimation( PLAYER_ANIM animation )
{
}

BOOL CBasePlayer::SwitchWeapon( CBasePlayerItem* item )
{
	return FALSE;
}

BOOL CBasePlayer::AddPlayerItem( CBasePlayerItem* item )
{
	return FALSE;
}

BOOL CBasePlayer::RemovePlayerItem( CBasePlayerItem* item )
{
	return FALSE;
}

int CBasePlayer::AmmoInventory( int ammo )
{
	if ( (unsigned int)ammo >= (unsigned int)MAX_AMMO_SLOTS )
		return -1;
	return m_rgAmmo[ammo];
}

void CBasePlayer::SetSuitUpdate( char* name, int group, int noRepeat )
{
}

void CBasePlayer::StartObserver( Vector position, Vector angles )
{
}

int CBasePlayer::FlashlightIsOn()
{
	return 0;
}

Vector CBasePlayer::GetGunPosition()
{
	return PGetGunPosition( pev->origin );
}

void CBasePlayer::AddPointsToTeam( int score, BOOL allowNegative )
{
}

Vector CBasePlayer::GetAutoaimVector( float delta )
{
	return g_vecZero;
}

Vector CBasePlayer::AutoaimDeflection( Vector& source, float distance, float delta )
{
	return g_vecZero;
}

int CBasePlayer::GetCustomDecalFrames()
{
	return -1;
}

void CBasePlayer::UpdateShieldCrosshair( bool show )
{
}

void CBasePlayer::Duck()
{
}

void CBasePlayer::Jump()
{
}

int CBasePlayer::Save( CSave& save )
{
	return 0;
}

int CBasePlayer::Restore( CRestore& restore )
{
	return 0;
}

void CBasePlayer::Blind( float until, float hold, float fade, int alpha )
{
}

void CBasePlayer::Radio( const char* sound, const char* text, short pitch, bool showIcon )
{
}

const char* CBasePlayer::TeamID()
{
	return "";
}

int CBasePlayer::Classify()
{
	return 0;
}

int CBasePlayer::GiveAmmo( int count, char* name, int maximum )
{
	return -1;
}

void CBasePlayer::Precache()
{
}

void CBasePlayer::PreThink()
{
}

void CBasePlayer::PostThink()
{
}

void CBasePlayer::AddPoints( int score, BOOL allowNegative )
{
}

void CBasePlayer::UpdateClientData()
{
}

void CBasePlayer::ImpulseCommands()
{
}

void CBasePlayer::DeathSound()
{
}

void CBasePlayer::RemoveAllItems( BOOL removeSuit )
{
}

void CBasePlayer::WaterMove()
{
}

BOOL CBasePlayer::IsOnLadder()
{
	return FALSE;
}

void CBasePlayer::PlayerDeathThink()
{
}

void CBasePlayer::StartDeathCam()
{
}

void CBasePlayer::PlayerUse()
{
}

void CBasePlayer::CheckTimeBasedDamage()
{
}

void CBasePlayer::UpdateGeigerCounter()
{
}

void CBasePlayer::CheckSuitUpdate()
{
}

void CBasePlayer::UpdatePlayerSound()
{
}

void CBasePlayer::RenewItems()
{
}

void CBasePlayer::SelectNextItem( int item )
{
}

BOOL CBasePlayer::HasWeapons()
{
	return FALSE;
}

void CBasePlayer::SelectPrevItem( int item )
{
}

void CBasePlayer::FlashlightTurnOn()
{
}

void CBasePlayer::FlashlightTurnOff()
{
}

void CBasePlayer::ForceClientDllUpdate()
{
}

void CBasePlayer::CheatImpulseCommands( int impulse )
{
}

void CBasePlayer::ItemPreFrame()
{
}

void CBasePlayer::ItemPostFrame()
{
}

int CBasePlayer::GetAmmoIndex( const char* name )
{
	return -1;
}

void CBasePlayer::SendAmmoUpdate()
{
}

void CBasePlayer::BarnacleVictimBitten( entvars_t* barnacle )
{
}

void CBasePlayer::BarnacleVictimReleased()
{
}

void CBasePlayer::EnableControl( BOOL control )
{
}

void CBasePlayer::ResetAutoaim()
{
}

void CBasePlayer::SetCustomDecalFrames( int frames )
{
}

void CBasePlayer::DropPlayerItem( const char* name )
{
}

BOOL CBasePlayer::HasPlayerItem( CBasePlayerItem* item )
{
	return FALSE;
}

void CBasePlayer::SetBombIcon( int flash )
{
}

void CBasePlayerAmmo::Spawn()
{
}

void CBasePlayerAmmo::Materialize()
{
}

void CBasePlayerAmmo::DefaultTouch( CBaseEntity* entity )
{
}

CBaseEntity* CBasePlayerAmmo::Respawn()
{
	return this;
}

int CBasePlayerItem::AddToPlayer( CBasePlayer* player )
{
	return 1;
}

int CBasePlayerItem::Save( CSave& save )
{
	return 1;
}

int CBasePlayerItem::Restore( CRestore& restore )
{
	return 1;
}

void CBasePlayerItem::SetObjectCollisionBox()
{
}

void CBasePlayerItem::CheckRespawn()
{
}

void CBasePlayerItem::AttachToPlayer( CBasePlayer* player )
{
}

void CBasePlayerItem::Drop()
{
}

void CBasePlayerItem::Kill()
{
}

void CBasePlayerItem::Holster( int skipLocal )
{
}

CBaseEntity* CBasePlayerItem::Respawn()
{
	return NULL;
}

void CBasePlayerItem::FallInit()
{
}

void CBasePlayerItem::FallThink()
{
}

void CBasePlayerItem::Materialize()
{
}

void CBasePlayerItem::AttemptToMaterialize()
{
}

void CBasePlayerItem::DefaultTouch( CBaseEntity* other )
{
}

void CBasePlayerItem::DestroyItem()
{
}

int CBasePlayerWeapon::Save( CSave& save )
{
	return 1;
}

int CBasePlayerWeapon::Restore( CRestore& restore )
{
	return 1;
}

int CBasePlayerWeapon::AddToPlayer( CBasePlayer* player )
{
	return 0;
}

int CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon* weapon )
{
	return 0;
}

int CBasePlayerWeapon::AddDuplicate( CBasePlayerItem* item )
{
	return 0;
}

int CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon* weapon )
{
	return 0;
}

void CBasePlayerWeapon::RetireWeapon()
{
	UTIL_GetNextBestWeapon( m_pPlayer, this );
}

BOOL CBasePlayerWeapon::AddPrimaryAmmo( int count, char* name, int maxClip, int maxCarry )
{
	return TRUE;
}

BOOL CBasePlayerWeapon::AddSecondaryAmmo( int count, char* name, int maximum )
{
	return TRUE;
}

int CBasePlayerWeapon::PrimaryAmmoIndex()
{
	return -1;
}

int CBasePlayerWeapon::SecondaryAmmoIndex()
{
	return -1;
}

int CBasePlayerWeapon::UpdateClientData( CBasePlayer* player )
{
	return 0;
}

float CBasePlayerWeapon::GetNextAttackDelay( float delay )
{
	return delay;
}
void CBasePlayerWeapon::KickBack( float upBase, float lateralBase, float upModifier, float lateralModifier,
	float upMax, float lateralMax, int directionChange )
{
}

BOOL CBasePlayerWeapon::IsUseable()
{
	return TRUE;
}

CGrenade* CGrenade::ShootTimed( entvars_t* owner, Vector start, Vector velocity, float time )
{
	return NULL;
}

CGrenade* CGrenade::ShootTimed2( entvars_t* owner, Vector start, Vector velocity, float time, int team, unsigned short eventIndex )
{
	return NULL;
}

CGrenade* CGrenade::ShootSmokeGrenade( entvars_t* owner, Vector start, Vector velocity, float time, unsigned short eventIndex )
{
	return NULL;
}

CGrenade* CGrenade::ShootSatchelCharge( entvars_t* owner, Vector start, Vector angles )
{
	return NULL;
}

void CGrenade::BounceSound()
{
}

void CGrenade::Use( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE useType, float value )
{
}

int CGrenade::Save( CSave& save )
{
	return 1;
}

int CGrenade::Restore( CRestore& restore )
{
	return 1;
}

void CGrenade::Spawn()
{
}

void CGrenade::Killed( entvars_t* attacker, int gib )
{
}

void CGrenade::Explode( TraceResult* trace, int damageType )
{
}

void CGrenade::Explode( Vector source, Vector direction )
{
}

void CHintMessageQueue::Reset()
{
}
