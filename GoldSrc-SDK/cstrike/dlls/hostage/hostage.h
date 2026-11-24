#ifndef HOSTAGE_H
#define HOSTAGE_H

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "basemonster.h"
#include "GameEvent.h"

class CHostageImprov;
class CLocalNav;
class CBasePlayer;

enum HostageChatterType
{
	HOSTAGE_CHATTER_START_FOLLOW,
	HOSTAGE_CHATTER_STOP_FOLLOW,
	HOSTAGE_CHATTER_INTIMIDATED,
	HOSTAGE_CHATTER_PAIN,
	HOSTAGE_CHATTER_SCARED_OF_GUNFIRE,
	HOSTAGE_CHATTER_SCARED_OF_MURDER,
	HOSTAGE_CHATTER_LOOK_OUT,
	HOSTAGE_CHATTER_PLEASE_RESCUE_ME,
	HOSTAGE_CHATTER_SEE_RESCUE_ZONE,
	HOSTAGE_CHATTER_IMPATIENT_FOR_RESCUE,
	HOSTAGE_CHATTER_CTS_WIN,
	HOSTAGE_CHATTER_TERRORISTS_WIN,
	HOSTAGE_CHATTER_RESCUED,
	HOSTAGE_CHATTER_WARN_NEARBY,
	HOSTAGE_CHATTER_WARN_SPOTTED,
	HOSTAGE_CHATTER_CALL_TO_RESCUER,
	HOSTAGE_CHATTER_RETREAT,
	HOSTAGE_CHATTER_COUGH,
	HOSTAGE_CHATTER_BLINDED,
	HOSTAGE_CHATTER_SAW_HE_GRENADE,
	HOSTAGE_CHATTER_DEATH_CRY,

	NUM_HOSTAGE_CHATTER_TYPES
};

enum state
{
	FOLLOW,
	STAND,
	DUCK,
	SCARED,
	IDLE,
	FOLLOWPATH
};

class CHostage : public CBaseMonster
{
public:
	~CHostage();
	void Spawn() override;
	void Precache() override;
	void EXPORT IdleThink();
	void Remove();
	void RePosition();
	void SetActivity( int activity );
	int ObjectCaps() override;
	void Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value ) override;
	int TakeDamage( entvars_t *inflictor, entvars_t *attacker, float damage, int damageType ) override;
	void Touch( CBaseEntity *other ) override;
	int Classify() override;
	int GetActivity() { return m_Activity; }
	int BloodColor() override;
	float GetModifiedDamage( float damage, int hitGroup );
	void SetFlinchActivity();
	void SetDeathActivity();
	void PlayPainSound();
	void PlayFollowRescueSound();
	void AnnounceDeath( CBasePlayer *player );
	void ApplyHostagePenalty( CBasePlayer *player );
	void GiveCTTouchBonus( CBasePlayer *player );
	void SendHostagePositionMsg();
	void SendHostageEventMsg();
	void DoFollow();
	BOOL IsOnLadder();
	void PointAt( const Vector &position );
	void MoveToward( const Vector &position );
	void NavReady();
	void Wiggle();
	void PreThink();
	bool IsFollowingSomeone();
	CBaseEntity *GetLeader();
	bool IsFollowing( const CBaseEntity *entity );
	bool IsValid() { return pev->takedamage == DAMAGE_YES; }
	bool IsDead() { return pev->deadflag == DEAD_DEAD; }
	bool IsAtHome() const { return (pev->origin - m_vStart).LengthSquared() < 400; }
	const Vector &GetHomePosition() const { return m_vStart; }

	int m_Activity;
	BOOL m_bTouched;
	BOOL m_bRescueMe;
	float m_flFlinchTime;
	float m_flNextChange;
	float m_flMarkPosition;
	int m_iModel;
	int m_iSkin;
	float m_flNextRadarTime;
	state m_State;
	Vector m_vStart;
	Vector m_vStartAngles;
	Vector m_vPathToFollow[20];
	int m_iWaypoint;
	CBasePlayer *m_target;
	CLocalNav *m_LocalNav;
	int nTargetNode;
	Vector vecNodes[100];
	EHANDLE m_hStoppedTargetEnt;
	float m_flNextFullThink;
	float m_flPathCheckInterval;
	float m_flLastPathCheck;
	int m_nPathNodes;
	BOOL m_fHasPath;
	float m_flPathAcquired;
	Vector m_vOldPos;
	int m_iHostageIndex;
	BOOL m_bStuck;
	float m_flStuckTime;
	CHostageImprov *m_improv;
	enum ModelType
	{
		REGULAR_GUY,
		OLD_GUY,
		BLACK_GUY,
		GOOFY_GUY
	};
	ModelType m_whichModel;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

class SimpleChatter
{
public:
	SimpleChatter();
	~SimpleChatter();
	void AddSound( HostageChatterType type, char *filename );
	float PlaySound( CBaseEntity *entity, HostageChatterType type );

private:
	struct SoundFile
	{
		char *filename;
		float duration;
	};
	struct ChatterSet
	{
		SoundFile file[32];
		int count;
		int index;
		bool needsShuffle;
	};
	char *GetSound( HostageChatterType type, float *duration );
	void Shuffle( ChatterSet *set );
	ChatterSet m_chatter[NUM_HOSTAGE_CHATTER_TYPES];
};

class CHostageManager
{
	friend class CHostageImprov;
	friend class CCSBot;
public:
	CHostageManager();
	void ServerActivate();
	void ServerDeactivate();
	void RestartRound();
	void AddHostage( CHostage *hostage );
	SimpleChatter *GetChatter() { return &m_chatter; }
	bool IsNearbyHostageTalking( CHostageImprov *improv ) const;
	bool IsNearbyHostageJumping( CHostageImprov *improv ) const;
	void OnEvent( GameEventType event, CBaseEntity *entity, CBaseEntity *other );
	CHostage *GetClosestHostage( const Vector &position, float *range )
	{
		float closestRange = 100000000;
		CHostage *closest = NULL;
		for ( int i = 0; i < m_hostageCount; ++i )
		{
			float distance = (m_hostage[i]->pev->origin - position).Length();
			if ( closestRange > distance )
			{
				closestRange = distance;
				closest = m_hostage[i];
			}
		}
		if ( range )
			*range = closestRange;
		return closest;
	}

private:
	CHostage *m_hostage[12];
	int m_hostageCount;
	SimpleChatter m_chatter;
};

extern CHostageManager *g_pHostages;
extern cvar_t cv_hostage_debug;
extern cvar_t cv_hostage_stop;

void Hostage_RegisterCVars();
void InstallHostageManager();

#endif
