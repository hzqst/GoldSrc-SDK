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
 
===== generic grenade.cpp ========================================================

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "soundent.h"
#include "decals.h"
#include "skill.h"
#include "career_tasks.h"
#include "bot/bot.h"
#include "bot/bot_manager.h"
#include "GameEvent.h"
#include "hltv.h"

extern int gmsgScenarioIcon;
extern int gmsgBombPickup;

void Broadcast( const char* sentence );


//===================grenade


LINK_ENTITY_TO_CLASS( grenade, CGrenade );

TYPEDESCRIPTION CGrenade::m_SaveData[] =
{
	DEFINE_FIELD( CGrenade, m_fAttenu, FIELD_FLOAT ),
	DEFINE_FIELD( CGrenade, m_flNextFreq, FIELD_TIME ),
	DEFINE_FIELD( CGrenade, m_flC4Blow, FIELD_TIME ),
	DEFINE_FIELD( CGrenade, m_flNextFreqInterval, FIELD_TIME ),
	DEFINE_FIELD( CGrenade, m_flNextBeep, FIELD_TIME ),
	DEFINE_FIELD( CGrenade, m_flDefuseCountDown, FIELD_TIME ),
	DEFINE_FIELD( CGrenade, m_flNextBlink, FIELD_TIME ),
	DEFINE_FIELD( CGrenade, m_pentCurBombTarget, FIELD_EDICT ),
	DEFINE_FIELD( CGrenade, m_sBeepName, FIELD_POINTER ),
	DEFINE_FIELD( CGrenade, m_bIsC4, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGrenade, m_bStartDefuse, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGrenade, m_SGSmoke, FIELD_INTEGER ),
	DEFINE_FIELD( CGrenade, m_bJustBlew, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGrenade, m_bLightSmoke, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGrenade, m_usEvent, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CGrenade, CBaseToggle );

static void AnnounceFlashInterval( float interval, float offset )
{
	if ( !UTIL_IsGame( "czero" ) )
		return;

	MESSAGE_BEGIN( MSG_ALL, gmsgScenarioIcon );
		WRITE_BYTE( 1 );
		WRITE_STRING( "bombticking" );
		WRITE_BYTE( 255 );
		WRITE_SHORT( (int)( interval * 100 ) );
		WRITE_SHORT( (int)( offset * 100 ) );
	MESSAGE_END();
}

void CGrenade::Spawn()
{
	m_iBounceCount = 0;
	pev->movetype = MOVETYPE_BOUNCE;
	MAKE_STRING_CLASS( "grenade", pev );
	m_bIsC4 = false;
	pev->solid = SOLID_BBOX;
	SET_MODEL( ENT( pev ), "models/grenade.mdl" );
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	pev->dmg = 30;
	m_fRegisteredSound = FALSE;
}

CGrenade* CGrenade::ShootTimed( entvars_t* pevOwner, Vector vecStart, Vector vecVelocity, float time )
{
	CGrenade* pGrenade = GetClassPtr( (CGrenade*)NULL );
	pGrenade->Spawn();
	UTIL_SetOrigin( pGrenade->pev, vecStart );
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = pevOwner->angles;
	pGrenade->pev->owner = ENT( pevOwner );
	pGrenade->SetTouch( &CGrenade::BounceTouch );	// Bounce if touched

	// Take one second off of the desired detonation time and set the think to PreDetonate. PreDetonate
	// will insert a DANGER sound into the world sound list and delay detonation for one second so that 
	// the grenade explodes after the exact amount of time specified in the call to ShootTimed(). 

	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink( &CGrenade::TumbleThink );
	pGrenade->pev->nextthink = (float)( gpGlobals->time + 0.1 );
	if ( time < 0.1 )
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = g_vecZero;
	}
	pGrenade->pev->sequence = RANDOM_LONG( 3, 6 );
	pGrenade->pev->framerate = 1;
	pGrenade->m_bJustBlew = true;
	pGrenade->pev->gravity = 0.5;
	pGrenade->pev->friction = 0.8f;
	SET_MODEL( ENT( pGrenade->pev ), "models/w_flashbang.mdl" );
	pGrenade->pev->dmg = 35;
	return pGrenade;
}

CGrenade* CGrenade::ShootTimed2( entvars_t* pevOwner, Vector vecStart, Vector vecVelocity,
		float time, int iTeam, unsigned short usEvent )
{
	CGrenade* pGrenade = GetClassPtr( (CGrenade*)NULL );
	pGrenade->Spawn();
	UTIL_SetOrigin( pGrenade->pev, vecStart );
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = pevOwner->angles;
	pGrenade->pev->owner = ENT( pevOwner );
	pGrenade->m_usEvent = usEvent;
	pGrenade->SetTouch( &CGrenade::BounceTouch );
	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink( &CGrenade::TumbleThink );
	pGrenade->pev->nextthink = (float)( gpGlobals->time + 0.1 );
	pGrenade->pev->sequence = RANDOM_LONG( 3, 6 );
	pGrenade->pev->framerate = 1;
	pGrenade->m_bJustBlew = true;
	pGrenade->pev->gravity = 0.55f;
	pGrenade->pev->friction = 0.7f;
	pGrenade->m_iTeam = iTeam;
	SET_MODEL( ENT( pGrenade->pev ), "models/w_hegrenade.mdl" );
	pGrenade->pev->dmg = 100;
	return pGrenade;
}

CGrenade* CGrenade::ShootSmokeGrenade( entvars_t* pevOwner, Vector vecStart, Vector vecVelocity,
		float time, unsigned short usEvent )
{
	CGrenade* pGrenade = GetClassPtr( (CGrenade*)NULL );
	pGrenade->Spawn();
	UTIL_SetOrigin( pGrenade->pev, vecStart );
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = pevOwner->angles;
	pGrenade->pev->owner = ENT( pevOwner );
	pGrenade->m_bLightSmoke = false;
	pGrenade->m_bDetonated = false;
	pGrenade->SetTouch( &CGrenade::BounceTouch );
	pGrenade->m_usEvent = usEvent;
	pGrenade->pev->dmgtime = gpGlobals->time + time;
	pGrenade->SetThink( &CGrenade::SG_TumbleThink );
	pGrenade->pev->nextthink = (float)( gpGlobals->time + 0.1 );
	if ( time < 0.1 )
	{
		pGrenade->pev->nextthink = gpGlobals->time;
		pGrenade->pev->velocity = g_vecZero;
	}
	pGrenade->pev->sequence = RANDOM_LONG( 3, 6 );
	pGrenade->pev->framerate = 1;
	pGrenade->m_bJustBlew = true;
	pGrenade->pev->gravity = 0.5;
	pGrenade->pev->friction = 0.8f;
	pGrenade->m_SGSmoke = 0;
	SET_MODEL( ENT( pGrenade->pev ), "models/w_smokegrenade.mdl" );
	pGrenade->pev->dmg = 35;
	return pGrenade;
}

CGrenade* CGrenade::ShootContact( entvars_t* pevOwner, Vector vecStart, Vector vecVelocity )
{
	CGrenade* pGrenade = GetClassPtr( (CGrenade*)NULL );
	pGrenade->Spawn();
	// contact grenades arc lower
	pGrenade->pev->gravity = 0.5; // lower gravity since grenade is aerodynamic and engine doesn't know it.
	UTIL_SetOrigin( pGrenade->pev, vecStart );
	pGrenade->pev->velocity = vecVelocity;
	pGrenade->pev->angles = UTIL_VecToAngles( vecVelocity );
	pGrenade->pev->owner = ENT( pevOwner );

	// make monsters afaid of it while in the air
	pGrenade->SetThink( &CGrenade::DangerSoundThink );
	pGrenade->pev->nextthink = gpGlobals->time;

	// Tumble in air
	pGrenade->pev->avelocity.x = RANDOM_FLOAT( -100, -500 );

	// Explode on contact
	pGrenade->SetTouch( &CGrenade::ExplodeTouch );
	pGrenade->pev->dmg = gSkillData.plrDmgM203Grenade;
	pGrenade->m_bJustBlew = true;
	return pGrenade;
}

CGrenade* CGrenade::ShootSatchelCharge( entvars_t* pevOwner, Vector vecStart, Vector vecAngles )
{
	CGrenade* pGrenade = GetClassPtr( (CGrenade*)NULL );
	pGrenade->pev->movetype = MOVETYPE_TOSS;
	MAKE_STRING_CLASS( "grenade", pGrenade->pev );
	pGrenade->pev->solid = SOLID_BBOX;
	SET_MODEL( ENT( pGrenade->pev ), "models/w_c4.mdl" );
	UTIL_SetSize( pGrenade->pev, Vector( -3, -6, 0 ), Vector( 3, 6, 8 ) );
	pGrenade->pev->dmg = 100;
	UTIL_SetOrigin( pGrenade->pev, vecStart );
	pGrenade->pev->velocity = g_vecZero;
	pGrenade->pev->angles = vecAngles;
	pGrenade->pev->owner = ENT( pevOwner );
	pGrenade->SetThink( &CGrenade::C4Think );
	pGrenade->SetTouch( &CGrenade::C4Touch );
	pGrenade->pev->spawnflags = 1;
	pGrenade->pev->nextthink = (float)( gpGlobals->time + 0.1 );
	CHalfLifeMultiplay* pRules = (CHalfLifeMultiplay*)g_pGameRules;
	pGrenade->m_flC4Blow = gpGlobals->time + pRules->m_iC4Timer;
	pGrenade->m_flNextFreqInterval = (float)( pRules->m_iC4Timer / 4 );
	pGrenade->m_flNextFreq = gpGlobals->time;
	pGrenade->m_iCurWave = 0;
	pGrenade->m_fAttenu = 0;
	pGrenade->m_sBeepName = NULL;
	pGrenade->m_flNextBeep = (float)( gpGlobals->time + 0.5 );
	pGrenade->m_bIsC4 = true;
	pGrenade->m_fNextDefuse = 0;
	pGrenade->m_bStartDefuse = false;
	pGrenade->m_flNextBlink = gpGlobals->time + 2;
	pGrenade->pev->friction = 0.9f;
	pGrenade->m_bJustBlew = false;
	CBasePlayer* pPlayer = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( pevOwner ) );
	if ( pPlayer && pPlayer->IsPlayer() )
		pGrenade->m_pentCurBombTarget = pPlayer->m_pentCurBombTarget;
	else
		pGrenade->m_pentCurBombTarget = NULL;
	return pGrenade;
}

void CGrenade::UseSatchelCharges( entvars_t* pevOwner, SATCHELCODE code )
{
	if ( !pevOwner )
		return;

	CBaseEntity* pOwner = CBaseEntity::Instance( pevOwner );
	edict_t* pentOwner = pOwner->edict();
	edict_t* pentGrenade = NULL;
	while ( !FNullEnt( pentGrenade = FIND_ENTITY_BY_CLASSNAME( pentGrenade, "grenade" ) ) )
	{
		CBaseEntity* pGrenade = CBaseEntity::Instance( pentGrenade );
		if ( pGrenade && ( pGrenade->pev->spawnflags & 1 ) && pGrenade->pev->owner == pentOwner )
		{
			if ( code == SATCHEL_DETONATE )
				pGrenade->Use( pOwner, pOwner, USE_ON, 0 );
			else
				pGrenade->pev->owner = NULL;
		}
	}
}

void CGrenade::BounceSound()
{
	if ( pev->dmg > 50 )
	{
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/he_bounce-1.wav", 0.25, ATTN_NORM );
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/grenade_hit1.wav", 0.25, ATTN_NORM );
			break;
		case 1:
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/grenade_hit2.wav", 0.25, ATTN_NORM );
			break;
		case 2:
			EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/grenade_hit3.wav", 0.25, ATTN_NORM );
			break;
		}
	}
}

void CGrenade::SlideTouch( CBaseEntity* pOther )
{
	// don't hit the guy that launched this grenade
	if ( pOther->edict() == pev->owner )
		return;
	if ( pev->flags & FL_ONGROUND )
		pev->velocity = pev->velocity * 0.95f;
	else
		BounceSound();
}

void CGrenade::BounceTouch( CBaseEntity* pOther )
{
	// don't hit the guy that launched this grenade
	if ( pOther->edict() == pev->owner )
		return;
	if ( FClassnameIs( pOther->pev, "func_breakable" ) && pOther->pev->rendermode != kRenderNormal )
	{
		pev->velocity = pev->velocity * -2;
		return;
	}
	if ( !m_fRegisteredSound )
	{
		Vector vecVelocity = pev->velocity;
		vecVelocity.z = (float)( vecVelocity.z * 0.7 );
		if ( vecVelocity.Length() <= 60 )
		{
			//ALERT( at_console, "Grenade Registered!: %f\n", vecTestVelocity.Length() );

			// grenade is moving really slow. It's probably very close to where it will ultimately stop moving. 
			// go ahead and emit the danger sound.

			// register a radius louder than the explosion, so we make sure everyone gets out of the way
			CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin, (int)( pev->dmg / 0.4 ), 0.3f );
			m_fRegisteredSound = TRUE;
		}
	}
	if ( pev->flags & FL_ONGROUND )
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.8f;
		pev->sequence = RANDOM_LONG( 1, 1 );
	}
	else
	{
		if ( m_iBounceCount <= 4 )
			BounceSound();
		if ( m_iBounceCount > 9 )
		{
			pev->groundentity = ENT( 0 );
			pev->flags |= FL_ONGROUND;
			pev->velocity = g_vecZero;
		}
		++m_iBounceCount;
	}
	pev->framerate = pev->velocity.Length() / 200;
	if ( pev->framerate > 1 )
		pev->framerate = 1;
	else if ( pev->framerate < 0.5 )
		pev->framerate = 0;
}


//
// Contact grenade, explode when it touches something
// 
void CGrenade::ExplodeTouch( CBaseEntity* pOther )
{
	pev->enemy = pOther->edict();
	Vector vecSpot = pev->origin - pev->velocity.Normalize() * 32;
	TraceResult tr;
	UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, edict(), &tr );
	Explode( &tr, DMG_BLAST );
}

void CGrenade::C4Touch( CBaseEntity* )
{
}

void CGrenade::DetonateUse( CBaseEntity*, CBaseEntity*, USE_TYPE, float )
{
	SetThink( &CGrenade::Detonate );
	pev->nextthink = gpGlobals->time;
}

void CGrenade::PreDetonate()
{
	CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin, 400, 0.3f );
	SetThink( &CGrenade::Detonate );
	pev->nextthink = gpGlobals->time + 1;
}

void CGrenade::TumbleThink()
{
	if ( !IsInWorld() )
	{
		UTIL_Remove( this );
		return;
	}
	StudioFrameAdvance();
	pev->nextthink = (float)( gpGlobals->time + 0.1 );
	if ( gpGlobals->time > pev->dmgtime - 1 )
	{
		Vector vecSpot = pev->origin + pev->velocity * ( pev->dmgtime - gpGlobals->time );
		CSoundEnt::InsertSound( bits_SOUND_DANGER, vecSpot, 400, 0.1f );
	}
	if ( gpGlobals->time >= pev->dmgtime )
	{
		if ( pev->dmg > 40 )
			SetThink( &CGrenade::Detonate3 );
		else
			SetThink( &CGrenade::Detonate );
	}
	if ( pev->waterlevel )
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2f;
	}
}

void CGrenade::SG_TumbleThink()
{
	if ( !IsInWorld() )
	{
		UTIL_Remove( this );
		return;
	}
	if ( pev->flags & FL_ONGROUND )
		pev->velocity = pev->velocity * 0.95f;
	StudioFrameAdvance();
	pev->nextthink = (float)( gpGlobals->time + 0.1 );
	if ( gpGlobals->time > pev->dmgtime - 1 )
	{
		Vector vecSpot = pev->origin + pev->velocity * ( pev->dmgtime - gpGlobals->time );
		CSoundEnt::InsertSound( bits_SOUND_DANGER, vecSpot, 400, 0.1f );
	}
	if ( gpGlobals->time >= pev->dmgtime && ( pev->flags & FL_ONGROUND ) )
		SetThink( &CGrenade::SG_Detonate );
	if ( pev->waterlevel )
	{
		pev->velocity = pev->velocity * 0.5;
		pev->framerate = 0.2f;
	}
}

void CGrenade::DangerSoundThink()
{
	if ( !IsInWorld() )
	{
		UTIL_Remove( this );
		return;
	}
	CSoundEnt::InsertSound( bits_SOUND_DANGER, pev->origin + pev->velocity * 0.5,
		(int)pev->velocity.Length(), 0.2f );
	pev->nextthink = (float)( gpGlobals->time + 0.2 );
	if ( pev->waterlevel )
		pev->velocity = pev->velocity * 0.5;
}

void CGrenade::Killed( entvars_t*, int )
{
	TraceResult tr;
	UTIL_TraceLine( pev->origin + Vector( 0, 0, 8 ), pev->origin + Vector( 0, 0, -32 ),
		ignore_monsters, edict(), &tr );
	Explode( &tr, DMG_BLAST );
}

void CGrenade::Explode( Vector, Vector )
{
	TraceResult tr;
	UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -32 ), ignore_monsters, edict(), &tr );
	Explode( &tr, DMG_BLAST );
}

void CGrenade::Detonate()
{
	Vector vecSpot = pev->origin + Vector( 0, 0, 8 );
	TraceResult tr;
	UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -40 ), ignore_monsters, edict(), &tr );
	Explode( &tr, DMG_BLAST );
}

void CGrenade::Detonate2()
{
	Vector vecSpot = pev->origin + Vector( 0, 0, 8 );
	TraceResult tr;
	UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -40 ), ignore_monsters, edict(), &tr );
	Explode2( &tr, DMG_BLAST );
}

void CGrenade::Detonate3()
{
	Vector vecSpot = pev->origin + Vector( 0, 0, 8 );
	TraceResult tr;
	UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -40 ), ignore_monsters, edict(), &tr );
	Explode3( &tr, DMG_EXPLOSION );
}

void CGrenade::Explode( TraceResult* pTrace, int bitsDamageType )
{
	pev->model = 0;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	if ( pTrace->flFraction != 1 )
		pev->origin = pTrace->vecEndPos + pTrace->vecPlaneNormal * ( pev->dmg - 24 ) * 0.6f;
	int iContents = UTIL_PointContents( pev->origin );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 3 );
	entvars_t* pevAttacker = pev->owner ? VARS( pev->owner ) : NULL;
	TheBots->OnEvent( EVENT_FLASHBANG_GRENADE_EXPLODED, CBaseEntity::Instance( pev->owner ),
		(CBaseEntity*)&pev->origin );
	pev->owner = NULL;
	RadiusFlash( pev->origin, pev, pevAttacker, 4, CLASS_NONE, bitsDamageType );
	if ( RANDOM_FLOAT( 0, 1 ) >= 0.5 )
		UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
	else
		UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
	RANDOM_FLOAT( 0, 1 );
	if ( RANDOM_LONG( 0, 1 ) )
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/flashbang-1.wav", 0.55f, ATTN_NORM );
	else
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/flashbang-2.wav", 0.55f, ATTN_NORM );
	pev->effects |= EF_NODRAW;
	SetThink( &CGrenade::Smoke );
	pev->velocity = g_vecZero;
	pev->nextthink = (float)( gpGlobals->time + 0.3 );
	if ( iContents != CONTENTS_WATER )
	{
		int count = RANDOM_LONG( 0, 3 );
		for ( int i = 0; i < count; ++i )
			CBaseEntity::Create( "spark_shower", pev->origin, pTrace->vecPlaneNormal, NULL );
	}
}

void CGrenade::SG_Explode( TraceResult* pTrace, int )
{
	pev->model = 0;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	if ( pTrace->flFraction != 1 )
		pev->origin = pTrace->vecEndPos + pTrace->vecPlaneNormal * ( pev->dmg - 24 ) * 0.6f;
	int iContents = UTIL_PointContents( pev->origin );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 3 );
	pev->owner = NULL;
	if ( RANDOM_FLOAT( 0, 1 ) >= 0.5 )
		UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
	else
		UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
	RANDOM_FLOAT( 0, 1 );
	if ( RANDOM_LONG( 0, 1 ) )
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/flashbang-1.wav", 0.55f, ATTN_NORM );
	else
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/flashbang-2.wav", 0.55f, ATTN_NORM );
	SetThink( &CGrenade::SG_Smoke );
	pev->nextthink = (float)( gpGlobals->time + 0.1 );
	if ( iContents != CONTENTS_WATER )
	{
		int count = RANDOM_LONG( 0, 3 );
		for ( int i = 0; i < count; ++i )
			CBaseEntity::Create( "spark_shower", pev->origin, pTrace->vecPlaneNormal, NULL );
	}
}

void CGrenade::Explode2( TraceResult* pTrace, int bitsDamageType )
{
	CHalfLifeMultiplay* pRules = (CHalfLifeMultiplay*)g_pGameRules;
	pev->model = 0;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	UTIL_ScreenShake( pTrace->vecEndPos, 25, 150, 1, 3000 );
	pRules->m_bTargetBombed = true;
	if ( ( (CHalfLifeMultiplay*)g_pGameRules )->IsCareer() )
		TheCareerTasks->LatchRoundEndMessage();
	m_bJustBlew = true;
	pRules->CheckWinConditions();
	if ( pTrace->flFraction != 1 )
		pev->origin = pTrace->vecEndPos + pTrace->vecPlaneNormal * ( pev->dmg - 24 ) * 0.6f;
	int iContents = UTIL_PointContents( pev->origin );
	for ( int i = 0; i < 4; ++i )
	{
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SPRITE );
			if ( !i )
			{
				WRITE_COORD( pev->origin.x );
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z - 10 );
			}
			else
			{
				WRITE_COORD( pev->origin.x + RANDOM_FLOAT( -512, 512 ) );
				WRITE_COORD( pev->origin.y + RANDOM_FLOAT( -512, 512 ) );
				WRITE_COORD( pev->origin.z + RANDOM_FLOAT( -10, 10 ) );
			}
			WRITE_SHORT( i == 1 ? g_sModelIndexFireball2 : i == 3 ? g_sModelIndexFireball : g_sModelIndexFireball3 );
			WRITE_BYTE( (int)( ( pev->dmg - 275 ) * 0.6 ) );
			WRITE_BYTE( i == 3 ? 17 : 150 );
		MESSAGE_END();
	}
	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/c4_explode1.wav", 1, 0.25 );
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 3 );
	entvars_t* pevAttacker = pev->owner ? VARS( pev->owner ) : NULL;
	pev->owner = NULL;
	RadiusDamage( pev, pevAttacker, pRules->m_flBombRadius, CLASS_NONE, bitsDamageType );
	if ( ( (CHalfLifeMultiplay*)g_pGameRules )->IsCareer() )
		TheCareerTasks->UnlatchRoundEndMessage();
	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE( 9 );
		WRITE_BYTE( DRC_CMD_EVENT );
		WRITE_SHORT( entindex() );
		WRITE_SHORT( 0 );
		WRITE_LONG( 15 | DRC_FLAG_FINAL );
	MESSAGE_END();
	if ( RANDOM_FLOAT( 0, 1 ) >= 0.5 )
		UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
	else
		UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
	RANDOM_FLOAT( 0, 1 );
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris1.wav", 0.55f, ATTN_NORM );
		break;
	case 1:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris2.wav", 0.55f, ATTN_NORM );
		break;
	case 2:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris3.wav", 0.55f, ATTN_NORM );
		break;
	}
	pev->effects |= EF_NODRAW;
	SetThink( &CGrenade::Smoke2 );
	pev->velocity = g_vecZero;
	pev->nextthink = (float)( gpGlobals->time + 0.85 );
	if ( iContents != CONTENTS_WATER )
	{
		int count = RANDOM_LONG( 0, 3 );
		for ( int i = 0; i < count; ++i )
			CBaseEntity::Create( "spark_shower", pev->origin, pTrace->vecPlaneNormal, NULL );
	}
}

void CGrenade::Explode3( TraceResult* pTrace, int bitsDamageType )
{
	pev->model = 0;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	if ( pTrace->flFraction != 1 )
		pev->origin = pTrace->vecEndPos + pTrace->vecPlaneNormal * ( pev->dmg - 24 ) * 0.6f;
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 20 );
		WRITE_SHORT( g_sModelIndexFireball3 );
		WRITE_BYTE( 25 );
		WRITE_BYTE( 30 );
		WRITE_BYTE( 0 );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x + RANDOM_FLOAT( -64, 64 ) );
		WRITE_COORD( pev->origin.y + RANDOM_FLOAT( -64, 64 ) );
		WRITE_COORD( pev->origin.z + RANDOM_FLOAT( 30, 35 ) );
		WRITE_SHORT( g_sModelIndexFireball2 );
		WRITE_BYTE( 30 );
		WRITE_BYTE( 30 );
		WRITE_BYTE( 0 );
	MESSAGE_END();
	CSoundEnt::InsertSound( bits_SOUND_COMBAT, pev->origin, 1024, 3 );
	entvars_t* pevAttacker = pev->owner ? VARS( pev->owner ) : NULL;
	TheBots->OnEvent( EVENT_HE_GRENADE_EXPLODED, CBaseEntity::Instance( pev->owner ), NULL );
	pev->owner = NULL;
	RadiusDamage( pev, pevAttacker, pev->dmg, CLASS_NONE, bitsDamageType );
	if ( RANDOM_FLOAT( 0, 1 ) >= 0.5 )
		UTIL_DecalTrace( pTrace, DECAL_SCORCH2 );
	else
		UTIL_DecalTrace( pTrace, DECAL_SCORCH1 );
	RANDOM_FLOAT( 0, 1 );
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris1.wav", 0.55f, ATTN_NORM );
		break;
	case 1:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris2.wav", 0.55f, ATTN_NORM );
		break;
	case 2:
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/debris3.wav", 0.55f, ATTN_NORM );
		break;
	}
	pev->effects |= EF_NODRAW;
	SetThink( &CGrenade::Smoke3_C );
	pev->velocity = g_vecZero;
	pev->nextthink = (float)( gpGlobals->time + 0.55 );
	int count = RANDOM_LONG( 0, 3 );
	for ( int i = 0; i < count; ++i )
		CBaseEntity::Create( "spark_shower", pev->origin, pTrace->vecPlaneNormal, NULL );
}

void CGrenade::SG_Detonate()
{
	Vector vecSpot = pev->origin + Vector( 0, 0, 8 );
	TraceResult tr;
	UTIL_TraceLine( vecSpot, vecSpot + Vector( 0, 0, -40 ), ignore_monsters, edict(), &tr );
	TheBots->OnEvent( EVENT_SMOKE_GRENADE_EXPLODED, CBaseEntity::Instance( pev->owner ), NULL );
	TheBots->AddGrenade( WEAPON_SMOKEGRENADE, this );
	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/sg_explode.wav", 1, ATTN_NORM );
	edict_t* pentGrenade = NULL;
	while ( !FNullEnt( pentGrenade = FIND_ENTITY_BY_CLASSNAME( pentGrenade, "grenade" ) ) )
	{
		CBaseEntity* pGrenade = CBaseEntity::Instance( pentGrenade );
		if ( pGrenade )
		{
			float distance = ( pGrenade->pev->origin - pev->origin ).Length();
			if ( distance != 0 && distance <= 250 && gpGlobals->time > pGrenade->pev->dmgtime )
				m_bLightSmoke = true;
		}
	}
	m_bDetonated = true;
	PLAYBACK_EVENT_FULL( 0, NULL, m_usEvent, 0, pev->origin, (float*)&g_vecZero,
		0, 0, 0, 1, m_bLightSmoke, 0 );
	m_vSmokeDetonate = pev->origin;
	pev->velocity.x = RANDOM_FLOAT( -175, 175 );
	pev->velocity.y = RANDOM_FLOAT( -175, 175 );
	pev->velocity.z = RANDOM_FLOAT( 250, 350 );
	pev->nextthink = gpGlobals->time + 0.1f;
	SetThink( &CGrenade::SG_Smoke );
}

void CGrenade::SG_Smoke()
{
	if ( UTIL_PointContents( pev->origin ) == CONTENTS_WATER )
	{
		UTIL_Bubbles( pev->origin - Vector( 64, 64, 64 ), pev->origin + Vector( 64, 64, 64 ), 100 );
	}
	else
	{
		UTIL_MakeVectors( pev->angles );
		Vector vecVelocity = gpGlobals->v_forward * RANDOM_FLOAT( 3, 8 );
		int angle = m_angle;
		m_angle = ( m_angle + 30 ) % 360;
		vecVelocity = RotateVectorZ( vecVelocity, angle );
		float speed = RANDOM_FLOAT( 1.5, 3.5 ) * 100;
		PLAYBACK_EVENT_FULL( 0, NULL, m_usEvent, 0, pev->origin, m_vSmokeDetonate,
			vecVelocity.x, vecVelocity.y, (int)speed, 4, m_bLightSmoke, 6 );
	}
	if ( m_SGSmoke <= 20 )
	{
		pev->nextthink = gpGlobals->time + 1;
		++m_SGSmoke;
		SetThink( &CGrenade::SG_Smoke );
		return;
	}
	pev->effects |= EF_NODRAW;
	TheBots->RemoveGrenade( this );
	UTIL_Remove( this );
}

void CGrenade::Smoke()
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
			WRITE_BYTE( 25 );
			WRITE_BYTE( 6 );
		MESSAGE_END();
	}
	UTIL_Remove( this );
}

void CGrenade::Smoke2()
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
			WRITE_BYTE( 150 );
			WRITE_BYTE( 8 );
		MESSAGE_END();
	}
	UTIL_Remove( this );
}

void CGrenade::Smoke3_A()
{
	if ( UTIL_PointContents( pev->origin ) == CONTENTS_WATER )
	{
		UTIL_Bubbles( pev->origin - Vector( 64, 64, 64 ), pev->origin + Vector( 64, 64, 64 ), 100 );
	}
	else
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x + RANDOM_FLOAT( -128, 128 ) );
			WRITE_COORD( pev->origin.y + RANDOM_FLOAT( -128, 128 ) );
			WRITE_COORD( pev->origin.z + RANDOM_FLOAT( -10, 10 ) );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( (int)(float)( RANDOM_FLOAT( 0, 10 ) + 15.0 ) );
			WRITE_BYTE( 12 );
		MESSAGE_END();
	}
	UTIL_Remove( this );
}

void CGrenade::Smoke3_B()
{
	if ( UTIL_PointContents( pev->origin ) == CONTENTS_WATER )
	{
		UTIL_Bubbles( pev->origin - Vector( 64, 64, 64 ), pev->origin + Vector( 64, 64, 64 ), 100 );
	}
	else
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x + RANDOM_FLOAT( -128, 128 ) );
			WRITE_COORD( pev->origin.y + RANDOM_FLOAT( -128, 128 ) );
			WRITE_COORD( pev->origin.z + RANDOM_FLOAT( -10, 10 ) );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( (int)(float)( RANDOM_FLOAT( 0, 10 ) + 15.0 ) );
			WRITE_BYTE( 10 );
		MESSAGE_END();
	}
	pev->nextthink = (float)( gpGlobals->time + 0.15 );
	SetThink( &CGrenade::Smoke3_A );
}

void CGrenade::Smoke3_C()
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
			WRITE_COORD( pev->origin.z - 5 );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( (int)(float)( RANDOM_FLOAT( 0, 10 ) + 35.0 ) );
			WRITE_BYTE( 5 );
		MESSAGE_END();
	}
	UTIL_Remove( this );
}

void CGrenade::Use( CBaseEntity* pActivator, CBaseEntity*, USE_TYPE, float )
{
	if ( !m_bIsC4 )
		return;
	CBasePlayer* pPlayer = GetClassPtr( (CBasePlayer*)pActivator->pev );
	if ( pPlayer->m_iTeam != CT )
		return;
	if ( m_bStartDefuse )
	{
		m_fNextDefuse = (float)( gpGlobals->time + 0.5 );
		return;
	}
	g_engfuncs.pfnSetClientMaxspeed( pPlayer->edict(), 1 );
	TheBots->OnEvent( EVENT_BOMB_DEFUSING, pActivator, NULL );
	if ( ( (CHalfLifeMultiplay*)g_pGameRules )->IsCareer() )
		TheCareerTasks->HandleEvent( EVENT_BOMB_DEFUSING, NULL, NULL );
	if ( pPlayer->m_bHasDefuser )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><CT>\" triggered \"Begin_Bomb_Defuse_With_Kit\"\n",
			STRING( pPlayer->pev->netname ), GETPLAYERUSERID( pPlayer->edict() ),
			GETPLAYERAUTHID( pPlayer->edict() ) );
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "#Defusing_Bomb_With_Defuse_Kit" );
		EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, "weapons/c4_disarm.wav", 1, ATTN_NORM );
		pPlayer->m_bIsDefusing = true;
		m_pBombDefuser = pActivator;
		m_bStartDefuse = true;
		m_flDefuseCountDown = gpGlobals->time + 5;
		m_fNextDefuse = (float)( gpGlobals->time + 0.5 );
		pPlayer->SetProgressBarTime( 5 );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><CT>\" triggered \"Begin_Bomb_Defuse_Without_Kit\"\n",
			STRING( pPlayer->pev->netname ), GETPLAYERUSERID( pPlayer->edict() ),
			GETPLAYERAUTHID( pPlayer->edict() ) );
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "#Defusing_Bomb_Without_Defuse_Kit" );
		EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, "weapons/c4_disarm.wav", 1, ATTN_NORM );
		pPlayer->m_bIsDefusing = true;
		m_pBombDefuser = pActivator;
		m_bStartDefuse = true;
		m_flDefuseCountDown = gpGlobals->time + 10;
		m_fNextDefuse = (float)( gpGlobals->time + 0.5 );
		pPlayer->SetProgressBarTime( 10 );
	}
}

void CGrenade::C4Think()
{
	if ( !IsInWorld() )
	{
		UTIL_Remove( this );
		return;
	}
	pev->nextthink = (float)( gpGlobals->time + 0.12 );
	if ( gpGlobals->time >= m_flNextFreq )
	{
		m_flNextFreq = gpGlobals->time + m_flNextFreqInterval;
		m_flNextFreqInterval = (float)( 0.9 * m_flNextFreqInterval );
		switch ( m_iCurWave )
		{
		case 0:
			m_sBeepName = "weapons/c4_beep1.wav";
			m_fAttenu = 1.5;
			AnnounceFlashInterval( 1.4f, 0 );
			break;
		case 1:
			m_sBeepName = "weapons/c4_beep2.wav";
			m_fAttenu = 1;
			AnnounceFlashInterval( 0.7f, 0.1f );
			break;
		case 2:
			m_sBeepName = "weapons/c4_beep3.wav";
			m_fAttenu = 0.8f;
			AnnounceFlashInterval( 0.4f, 0 );
			break;
		case 3:
			m_sBeepName = "weapons/c4_beep4.wav";
			m_fAttenu = 0.5;
			AnnounceFlashInterval( 0.3f, 0 );
			break;
		case 4:
			m_sBeepName = "weapons/c4_beep5.wav";
			m_fAttenu = 0.2f;
			AnnounceFlashInterval( 0.2f, 0 );
			break;
		}
		++m_iCurWave;
	}
	if ( gpGlobals->time >= m_flNextBeep )
	{
		m_flNextBeep = (float)( gpGlobals->time + 1.4 );
		EMIT_SOUND( ENT( pev ), CHAN_VOICE, m_sBeepName, 1, m_fAttenu );
		TheBots->OnEvent( EVENT_BOMB_BEEP, this, NULL );
	}
	if ( gpGlobals->time >= m_flNextBlink )
	{
		m_flNextBlink = gpGlobals->time + 2;
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_GLOWSPRITE );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z + 5 );
			WRITE_SHORT( g_sModelIndexC4Glow );
			WRITE_BYTE( 1 );
			WRITE_BYTE( 3 );
			WRITE_BYTE( 255 );
		MESSAGE_END();
	}
	if ( gpGlobals->time >= m_flC4Blow )
	{
		TheBots->OnEvent( EVENT_BOMB_EXPLODED, NULL, NULL );
		MESSAGE_BEGIN( MSG_ALL, gmsgScenarioIcon );
			WRITE_BYTE( 0 );
		MESSAGE_END();
		if ( m_pentCurBombTarget )
		{
			CBaseEntity* pTarget = CBaseEntity::Instance( m_pentCurBombTarget );
			if ( pTarget )
				pTarget->Use( CBaseEntity::Instance( pev->owner ), this, USE_TOGGLE, 0 );
		}
		CBasePlayer* pOwner = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( pev->owner ) );
		if ( pOwner )
			pOwner->pev->frags += 3;
		MESSAGE_BEGIN( MSG_ALL, gmsgBombPickup );
		MESSAGE_END();
		g_pGameRules->m_bBombDropped = false;
		if ( pev->waterlevel )
			UTIL_Remove( this );
		else
			SetThink( &CGrenade::Detonate2 );
	}
	if ( !m_bStartDefuse )
		return;
	if ( (CBaseEntity*)m_pBombDefuser && m_flDefuseCountDown > gpGlobals->time )
	{
		int onGround = m_pBombDefuser->pev->flags & FL_ONGROUND;
		if ( gpGlobals->time <= m_fNextDefuse && onGround )
			return;
		if ( !onGround )
			ClientPrint( m_pBombDefuser->pev, HUD_PRINTCENTER, "#C4_Defuse_Must_Be_On_Ground" );
		( (CBasePlayer*)(CBaseEntity*)m_pBombDefuser )->ResetMaxSpeed();
		( (CBasePlayer*)(CBaseEntity*)m_pBombDefuser )->m_bIsDefusing = false;
		( (CBasePlayer*)(CBaseEntity*)m_pBombDefuser )->SetProgressBarTime( 0 );
		m_pBombDefuser = NULL;
		m_bStartDefuse = false;
		m_flDefuseCountDown = 0;
		TheBots->OnEvent( EVENT_BOMB_DEFUSE_ABORTED, NULL, NULL );
		return;
	}
	if ( !(CBaseEntity*)m_pBombDefuser || m_pBombDefuser->pev->deadflag != DEAD_NO )
	{
		if ( (CBaseEntity*)m_pBombDefuser )
		{
			( (CBasePlayer*)(CBaseEntity*)m_pBombDefuser )->ResetMaxSpeed();
			( (CBasePlayer*)(CBaseEntity*)m_pBombDefuser )->m_bIsDefusing = false;
		}
		m_bStartDefuse = false;
		m_pBombDefuser = NULL;
		TheBots->OnEvent( EVENT_BOMB_DEFUSE_ABORTED, NULL, NULL );
		return;
	}
	TheBots->OnEvent( EVENT_BOMB_DEFUSED, (CBaseEntity*)m_pBombDefuser, NULL );
	Broadcast( "BOMBDEF" );
	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE( 9 );
		WRITE_BYTE( DRC_CMD_EVENT );
		WRITE_SHORT( m_pBombDefuser->entindex() );
		WRITE_SHORT( 0 );
		WRITE_LONG( 15 | DRC_FLAG_FINAL | DRC_FLAG_FACEPLAYER | DRC_FLAG_DRAMATIC );
	MESSAGE_END();
	UTIL_LogPrintf( "\"%s<%i><%s><CT>\" triggered \"Defused_The_Bomb\"\n",
		STRING( m_pBombDefuser->pev->netname ), GETPLAYERUSERID( m_pBombDefuser->edict() ),
		GETPLAYERAUTHID( m_pBombDefuser->edict() ) );
	UTIL_EmitAmbientSound( ENT( pev ), pev->origin, "weapons/c4_beep5.wav", 0, 0, SND_STOP, 0 );
	EMIT_SOUND( m_pBombDefuser->edict(), CHAN_WEAPON, "weapons/c4_disarmed.wav", 1, ATTN_NORM );
	UTIL_Remove( this );
	m_bJustBlew = true;
	( (CBasePlayer*)(CBaseEntity*)m_pBombDefuser )->ResetMaxSpeed();
	( (CBasePlayer*)(CBaseEntity*)m_pBombDefuser )->m_bIsDefusing = false;
	MESSAGE_BEGIN( MSG_ALL, gmsgScenarioIcon );
		WRITE_BYTE( 0 );
	MESSAGE_END();
	if ( ( (CHalfLifeMultiplay*)g_pGameRules )->IsCareer() )
	{
		CBasePlayer* pDefuser = (CBasePlayer*)(CBaseEntity*)m_pBombDefuser;
		if ( !pDefuser->IsBot() )
			TheCareerTasks->HandleEvent( EVENT_BOMB_DEFUSED, pDefuser, NULL );
	}
	( (CHalfLifeMultiplay*)g_pGameRules )->m_bBombDefused = true;
	( (CHalfLifeMultiplay*)g_pGameRules )->CheckWinConditions();
	m_pBombDefuser->pev->frags += 3;
	MESSAGE_BEGIN( MSG_ALL, gmsgBombPickup );
	MESSAGE_END();
	g_pGameRules->m_bBombDropped = false;
	m_pBombDefuser = NULL;
	m_bStartDefuse = false;
}

//======================end grenade

