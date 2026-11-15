#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "schedule.h"
#include "squadtalkmonster.h"
#include "animation.h"
#include "defaultai.h"
#include "weapons.h"
#include "soundent.h"
#include "effects.h"
#include "customentity.h"

#define MEDIC_EAGLE 1
#define MEDIC_9MMHANDGUN 2
#define MEDIC_NEEDLE 4
#define MEDIC_HANDGRENADE 8
#define MEDIC_SENTENCE_VOLUME 0.35f
#define bits_COND_MEDIC_NOFIRE bits_COND_SPECIAL1

enum
{
	SCHED_MEDIC_SUPPRESS = LAST_TALKMONSTER_SCHEDULE + 1,
	SCHED_MEDIC_ESTABLISH_LINE_OF_FIRE,
	SCHED_MEDIC_COVER_AND_RELOAD,
	SCHED_MEDIC_SWEEP,
	SCHED_MEDIC_FOUND_ENEMY,
	SCHED_MEDIC_REPEL,
	SCHED_MEDIC_REPEL_ATTACK,
	SCHED_MEDIC_REPEL_LAND,
	SCHED_MEDIC_WAIT_FACE_ENEMY,
	SCHED_MEDIC_TAKECOVER_FAILED,
	SCHED_MEDIC_HEAL_TARGET,
	SCHED_MEDIC_ELOF_FAIL
};

enum
{
	TASK_MEDIC_FACE_TOSS_DIR = LAST_TALKMONSTER_TASK + 1,
	TASK_MEDIC_SPEAK_SENTENCE,
	TASK_MEDIC_CHECK_FIRE
};

enum
{
	MEDIC_SENT_NONE = -1,
	MEDIC_SENT_GREN,
	MEDIC_SENT_ALERT,
	MEDIC_SENT_MONSTER,
	MEDIC_SENT_COVER,
	MEDIC_SENT_THROW,
	MEDIC_SENT_CHARGE,
	MEDIC_SENT_TAUNT
} MEDIC_SENTENCE_TYPES;

int g_fMedicAllyQuestion;

class COFMedicAlly : public COFSquadTalkMonster
{
public:
	void Spawn();
	void Precache();
	void SetYawSpeed();
	int Classify();
	int ISoundMask();
	int ObjectCaps() { return COFSquadTalkMonster::ObjectCaps() | FCAP_IMPULSE_USE; }
	void HandleAnimEvent( MonsterEvent_t* event );
	BOOL FCanCheckAttacks();
	BOOL CheckMeleeAttack1( float dot, float distance );
	BOOL CheckRangeAttack1( float dot, float distance );
	BOOL CheckRangeAttack2( float dot, float distance );
	void CheckAmmo();
	void SetActivity( Activity activity );
	void StartTask( Task_t* task );
	void RunTask( Task_t* task );
	void MonsterThink();
	void DeathSound();
	void PainSound();
	void IdleSound();
	void AlertSound();
	Vector GetGunPosition();
	void Shoot();
	void PrescheduleThink();
	void GibMonster();
	void SpeakSentence();
	int Save( CSave& save );
	int Restore( CRestore& restore );
	CBaseEntity* Kick();
	Schedule_t* GetSchedule();
	Schedule_t* GetScheduleOfType( int type );
	MONSTERSTATE GetIdealState();
	void TraceAttack( entvars_t* attacker, float damage, Vector direction, TraceResult* trace, int damageType );
	int TakeDamage( entvars_t* inflictor, entvars_t* attacker, float damage, int damageType );
	void Killed( entvars_t* attacker, int gib );
	void KeyValue( KeyValueData* data );
	void DeclineFollowing();
	void TalkInit();
	BOOL FOkToSpeak();
	void JustSpoke();
	BOOL HealMe( COFSquadTalkMonster* target );
	void HealerActivate( CBaseMonster* target );
	void EXPORT HealOff();
	void EXPORT HealerUse( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE useType, float value );
	CUSTOM_SCHEDULES;
	static TYPEDESCRIPTION m_SaveData[];
	static const char* pMedicSentences[];

	BOOL	m_lastAttackCheck;
	float	m_flPlayerDamage;
	float	m_flNextGrenadeCheck;
	float	m_flNextPainTime;
	float	m_flLastEnemySightTime;
	Vector	m_vecTossVelocity;
	BOOL	m_fThrowGrenade;
	BOOL	m_fStanding;
	BOOL	m_fFirstEncounter;
	int		m_cClipSize;
	int		m_iHealCharge;
	BOOL	m_fUseHealing;
	BOOL	m_fHealing;
	float	m_flLastUseTime;
	EHANDLE	m_hNewTargetEnt;
	BOOL	m_fQueueFollow;
	BOOL	m_fHealAudioPlaying;
	float	m_flFollowCheckTime;
	BOOL	m_fFollowChecking;
	BOOL	m_fFollowChecked;
	float	m_flLastRejectAudio;
	int		m_iBlackOrWhite;
	BOOL	m_fGunHolstered;
	BOOL	m_fHypoHolstered;
	BOOL	m_fHealActive;
	int		m_iWeaponIdx;
	float	m_flLastShot;
	int		m_iBrassShell;
	int		m_iSentence;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

LINK_ENTITY_TO_CLASS( monster_human_medic_ally, COFMedicAlly );

TYPEDESCRIPTION COFMedicAlly::m_SaveData[] =
{
	DEFINE_FIELD( COFMedicAlly, m_lastAttackCheck, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_flPlayerDamage, FIELD_FLOAT ),
	DEFINE_FIELD( COFMedicAlly, m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( COFMedicAlly, m_flNextPainTime, FIELD_TIME ),
	DEFINE_FIELD( COFMedicAlly, m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( COFMedicAlly, m_fThrowGrenade, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_fStanding, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_fFirstEncounter, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_cClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( COFMedicAlly, m_iSentence, FIELD_INTEGER ),
	DEFINE_FIELD( COFMedicAlly, m_flFollowCheckTime, FIELD_FLOAT ),
	DEFINE_FIELD( COFMedicAlly, m_fFollowChecking, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_fFollowChecked, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_flLastRejectAudio, FIELD_FLOAT ),
	DEFINE_FIELD( COFMedicAlly, m_iBlackOrWhite, FIELD_INTEGER ),
	DEFINE_FIELD( COFMedicAlly, m_iHealCharge, FIELD_INTEGER ),
	DEFINE_FIELD( COFMedicAlly, m_fUseHealing, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_fHealing, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_flLastUseTime, FIELD_TIME ),
	DEFINE_FIELD( COFMedicAlly, m_hNewTargetEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( COFMedicAlly, m_iBlackOrWhite, FIELD_INTEGER ),
	DEFINE_FIELD( COFMedicAlly, m_fGunHolstered, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_fHypoHolstered, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_fHealActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFMedicAlly, m_iWeaponIdx, FIELD_INTEGER ),
	DEFINE_FIELD( COFMedicAlly, m_flLastShot, FIELD_TIME )
};

IMPLEMENT_SAVERESTORE( COFMedicAlly, COFSquadTalkMonster );

const char* COFMedicAlly::pMedicSentences[] =
{
	"FG_GREN", "FG_ALERT", "FG_MONSTER", "FG_COVER", "FG_THROW", "FG_CHARGE", "FG_TAUNT"
};

extern Task_t tlGruntAllyFail[4];
extern Task_t tlGruntAllyCombatFail[4];
extern Task_t tlGruntAllyVictoryDance[8];
extern Task_t tlGruntAllyFoundEnemy[3];
extern Task_t tlGruntAllyCombatFace1[5];
extern Task_t tlGruntAllySignalSuppress[18];
extern Task_t tlGruntAllySuppress[16];
extern Task_t tlGruntAllyWaitInCover[3];
extern Task_t tlGruntAllyTakeCover1[9];
extern Task_t tlGruntAllyGrenadeCover1[8];
extern Task_t tlGruntAllyTossGrenadeCover1[3];
extern Task_t tlGruntAllyTakeCoverFromBestSound[7];
extern Task_t tlGruntAllyHideReload[8];
extern Task_t tlGruntAllySweep[4];
extern Task_t tlGruntAllyRangeAttack1A[13];
extern Task_t tlGruntAllyRangeAttack1B[13];
extern Task_t tlGruntAllyRangeAttack2[4];
extern Task_t tlGruntAllyRepel[3];
extern Task_t tlGruntAllyRepelAttack[3];
extern Task_t tlGruntAllyRepelLand[6];
extern Task_t tlGruntAllyIdleStand[4];
extern Task_t tlGruntAllyFaceTarget[4];
extern Task_t tlGruntAllyFollow[2];

Task_t tlMedicAllyEstablishLineOfFire[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_MEDIC_ELOF_FAIL },
	{ TASK_GET_PATH_TO_ENEMY, 0 },
	{ TASK_MEDIC_SPEAK_SENTENCE, 0 },
	{ TASK_RUN_PATH, 0 },
	{ TASK_WAIT_FOR_MOVEMENT, 0 }
};

Schedule_t slMedicAllyFail[] =
{
	{ tlGruntAllyFail, ARRAYSIZE( tlGruntAllyFail ), bits_COND_CAN_ATTACK, 0, "Grunt Fail" }
};
Schedule_t slMedicAllyCombatFail[] =
{
	{ tlGruntAllyCombatFail, ARRAYSIZE( tlGruntAllyCombatFail ),
		bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2, 0, "Grunt Combat Fail" }
};
Schedule_t slMedicAllyVictoryDance[] =
{
	{ tlGruntAllyVictoryDance, ARRAYSIZE( tlGruntAllyVictoryDance ),
		bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE, 0, "GruntVictoryDance" }
};
Schedule_t slMedicAllyEstablishLineOfFire[] =
{
	{ tlMedicAllyEstablishLineOfFire, ARRAYSIZE( tlMedicAllyEstablishLineOfFire ),
		bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD | bits_COND_CAN_ATTACK | bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER, "GruntEstablishLineOfFire" }
};
Schedule_t slMedicAllyFoundEnemy[] =
{
	{ tlGruntAllyFoundEnemy, ARRAYSIZE( tlGruntAllyFoundEnemy ),
		bits_COND_HEAR_SOUND, bits_SOUND_DANGER, "GruntFoundEnemy" }
};
Schedule_t slMedicAllyCombatFace[] =
{
	{ tlGruntAllyCombatFace1, ARRAYSIZE( tlGruntAllyCombatFace1 ),
		bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD | bits_COND_CAN_RANGE_ATTACK1 | bits_COND_CAN_RANGE_ATTACK2,
		0, "Combat Face" }
};
Schedule_t slMedicAllySignalSuppress[] =
{
	{ tlGruntAllySignalSuppress, ARRAYSIZE( tlGruntAllySignalSuppress ),
		bits_COND_ENEMY_DEAD | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND |
		bits_COND_MEDIC_NOFIRE | bits_COND_NO_AMMO_LOADED, bits_SOUND_DANGER, "SignalSuppress" }
};
Schedule_t slMedicAllySuppress[] =
{
	{ tlGruntAllySuppress, ARRAYSIZE( tlGruntAllySuppress ),
		bits_COND_ENEMY_DEAD | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND |
		bits_COND_MEDIC_NOFIRE | bits_COND_NO_AMMO_LOADED, bits_SOUND_DANGER, "Suppress" }
};
Schedule_t slMedicAllyWaitInCover[] =
{
	{ tlGruntAllyWaitInCover, ARRAYSIZE( tlGruntAllyWaitInCover ),
		bits_COND_NEW_ENEMY | bits_COND_HEAR_SOUND | bits_COND_CAN_ATTACK, bits_SOUND_DANGER, "GruntWaitInCover" }
};
Schedule_t slMedicAllyTakeCover[] =
{
	{ tlGruntAllyTakeCover1, ARRAYSIZE( tlGruntAllyTakeCover1 ), 0, 0, "TakeCover" }
};
Schedule_t slMedicAllyGrenadeCover[] =
{
	{ tlGruntAllyGrenadeCover1, ARRAYSIZE( tlGruntAllyGrenadeCover1 ), 0, 0, "GrenadeCover" }
};
Schedule_t slMedicAllyTossGrenadeCover[] =
{
	{ tlGruntAllyTossGrenadeCover1, ARRAYSIZE( tlGruntAllyTossGrenadeCover1 ), 0, 0, "TossGrenadeCover" }
};
Schedule_t slMedicAllyTakeCoverFromBestSound[] =
{
	{ tlGruntAllyTakeCoverFromBestSound, ARRAYSIZE( tlGruntAllyTakeCoverFromBestSound ),
		0, 0, "GruntTakeCoverFromBestSound" }
};
Schedule_t slMedicAllyHideReload[] =
{
	{ tlGruntAllyHideReload, ARRAYSIZE( tlGruntAllyHideReload ),
		bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND, bits_SOUND_DANGER, "GruntHideReload" }
};
Schedule_t slMedicAllySweep[] =
{
	{ tlGruntAllySweep, ARRAYSIZE( tlGruntAllySweep ),
		bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_RANGE_ATTACK2 | bits_COND_HEAR_SOUND,
		bits_SOUND_WORLD | bits_SOUND_DANGER | bits_SOUND_PLAYER, "Grunt Sweep" }
};
Schedule_t slMedicAllyRangeAttack1A[] =
{
	{ tlGruntAllyRangeAttack1A, ARRAYSIZE( tlGruntAllyRangeAttack1A ),
		bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD | bits_COND_HEAVY_DAMAGE | bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAR_SOUND | bits_COND_MEDIC_NOFIRE | bits_COND_NO_AMMO_LOADED,
		bits_SOUND_DANGER, "Range Attack1A" }
};
Schedule_t slMedicAllyRangeAttack1B[] =
{
	{ tlGruntAllyRangeAttack1B, ARRAYSIZE( tlGruntAllyRangeAttack1B ),
		bits_COND_NEW_ENEMY | bits_COND_ENEMY_DEAD | bits_COND_HEAVY_DAMAGE | bits_COND_ENEMY_OCCLUDED |
		bits_COND_HEAR_SOUND | bits_COND_MEDIC_NOFIRE | bits_COND_NO_AMMO_LOADED,
		bits_SOUND_DANGER, "Range Attack1B" }
};
Schedule_t slMedicAllyRangeAttack2[] =
{
	{ tlGruntAllyRangeAttack2, ARRAYSIZE( tlGruntAllyRangeAttack2 ), 0, 0, "RangeAttack2" }
};
Schedule_t slMedicAllyRepel[] =
{
	{ tlGruntAllyRepel, ARRAYSIZE( tlGruntAllyRepel ),
		bits_COND_SEE_ENEMY | bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND, bits_SOUND_DANGER | bits_SOUND_COMBAT | bits_SOUND_PLAYER, "Repel" }
};
Schedule_t slMedicAllyRepelAttack[] =
{
	{ tlGruntAllyRepelAttack, ARRAYSIZE( tlGruntAllyRepelAttack ), bits_COND_ENEMY_OCCLUDED, 0, "Repel Attack" }
};
Schedule_t slMedicAllyRepelLand[] =
{
	{ tlGruntAllyRepelLand, ARRAYSIZE( tlGruntAllyRepelLand ),
		bits_COND_SEE_ENEMY | bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE |
		bits_COND_HEAR_SOUND, bits_SOUND_DANGER | bits_SOUND_COMBAT | bits_SOUND_PLAYER, "Repel Land" }
};
Schedule_t slMedicAllyIdleStand[] =
{
	{ tlGruntAllyIdleStand, ARRAYSIZE( tlGruntAllyIdleStand ),
		bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND |
		bits_COND_SMELL | bits_COND_PROVOKED,
		bits_SOUND_COMBAT | bits_SOUND_DANGER | bits_SOUND_MEAT | bits_SOUND_CARCASS | bits_SOUND_GARBAGE, "IdleStand" }
};
Schedule_t slMedicAllyFaceTarget[] =
{
	{ tlGruntAllyFaceTarget, ARRAYSIZE( tlGruntAllyFaceTarget ),
		bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND |
		bits_COND_CLIENT_PUSH | bits_COND_PROVOKED, bits_SOUND_DANGER, "FaceTarget" }
};
Schedule_t slMedicAllyFollow[] =
{
	{ tlGruntAllyFollow, ARRAYSIZE( tlGruntAllyFollow ),
		bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED, bits_SOUND_DANGER, "Follow" }
};

Task_t tlMedicAllyDrawNeedle[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_PLAY_SEQUENCE_FACE_TARGET, (float)ACT_ARM },
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_TARGET_CHASE },
	{ TASK_MOVE_TO_TARGET_RANGE, 50 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_MEDIC_SPEAK_SENTENCE, 0 }
};
Schedule_t slMedicAllyDrawNeedle[] =
{
	{ tlMedicAllyDrawNeedle, ARRAYSIZE( tlMedicAllyDrawNeedle ), 0, 0, "Draw Needle" }
};
Task_t tlMedicAllyNewHealTarget[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_TARGET_CHASE },
	{ TASK_MOVE_TO_TARGET_RANGE, 50 },
	{ TASK_FACE_IDEAL, 0 },
	{ TASK_MEDIC_SPEAK_SENTENCE, 0 }
};
Schedule_t slMedicAllyNewHealTarget[] =
{
	{ tlMedicAllyNewHealTarget, ARRAYSIZE( tlMedicAllyNewHealTarget ), 0, 0, "Draw Needle" }
};
Task_t tlMedicAllyHealTarget[] =
{
	{ TASK_MELEE_ATTACK2, 0 },
	{ TASK_WAIT, 0.2f },
	{ TASK_TLK_HEADRESET, 0 }
};
Schedule_t slMedicAllyHealTarget[] =
{
	{ tlMedicAllyHealTarget, ARRAYSIZE( tlMedicAllyHealTarget ), 0, 0, "Medic Ally Heal Target" }
};
Task_t tlMedicAllyDrawGun[] =
{
	{ TASK_PLAY_SEQUENCE, (float)ACT_DISARM },
	{ TASK_WAIT_FOR_MOVEMENT, 0 }
};
Schedule_t slMedicAllyDrawGun[] =
{
	{ tlMedicAllyDrawGun, ARRAYSIZE( tlMedicAllyDrawGun ), 0, 0, "Draw Gun" }
};

DEFINE_CUSTOM_SCHEDULES( COFMedicAlly )
{
	slMedicAllyFollow, slMedicAllyFaceTarget, slMedicAllyIdleStand, slMedicAllyFail,
	slMedicAllyCombatFail, slMedicAllyVictoryDance, slMedicAllyEstablishLineOfFire,
	slMedicAllyFoundEnemy, slMedicAllyCombatFace, slMedicAllySignalSuppress,
	slMedicAllySuppress, slMedicAllyWaitInCover, slMedicAllyTakeCover, slMedicAllyGrenadeCover,
	slMedicAllyTossGrenadeCover, slMedicAllyTakeCoverFromBestSound, slMedicAllyHideReload,
	slMedicAllySweep, slMedicAllyRangeAttack1A, slMedicAllyRangeAttack1B, slMedicAllyRangeAttack2,
	slMedicAllyRepel, slMedicAllyRepelAttack, slMedicAllyRepelLand, slMedicAllyDrawNeedle,
	slMedicAllyHealTarget, slMedicAllyNewHealTarget, slMedicAllyDrawGun
};

IMPLEMENT_CUSTOM_SCHEDULES( COFMedicAlly, COFSquadTalkMonster );

int COFMedicAlly::Classify()
{
	return CLASS_HUMAN_FRIENDLY;
}

int COFMedicAlly::ISoundMask()
{
	return bits_SOUND_WORLD | bits_SOUND_COMBAT | bits_SOUND_PLAYER | bits_SOUND_DANGER |
		bits_SOUND_MEAT | bits_SOUND_CARCASS | bits_SOUND_GARBAGE;
}

void COFMedicAlly::SetYawSpeed()
{
	switch ( m_Activity )
	{
	case ACT_IDLE:
	case ACT_RUN: pev->yaw_speed = 150; break;
	case ACT_WALK:
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT: pev->yaw_speed = 180; break;
	case ACT_RANGE_ATTACK1:
	case ACT_RANGE_ATTACK2:
	case ACT_MELEE_ATTACK1:
	case ACT_MELEE_ATTACK2: pev->yaw_speed = 120; break;
	case ACT_GLIDE:
	case ACT_FLY: pev->yaw_speed = 30; break;
	default: pev->yaw_speed = 90; break;
	}
}

void COFMedicAlly::TalkInit()
{
	COFAllyMonster::TalkInit();
	m_szGrp[TLK_ANSWER] = "FG_ANSWER";
	m_szGrp[TLK_QUESTION] = "FG_QUESTION";
	m_szGrp[TLK_IDLE] = "FG_IDLE";
	m_szGrp[TLK_STARE] = "FG_STARE";
	m_szGrp[TLK_USE] = "FG_OK";
	m_szGrp[TLK_UNUSE] = "FG_WAIT";
	m_szGrp[TLK_STOP] = "FG_STOP";
	m_szGrp[TLK_NOSHOOT] = "FG_SCARED";
	m_szGrp[TLK_HELLO] = "FG_HELLO";
	m_szGrp[TLK_PHELLO] = NULL;
	m_szGrp[TLK_PIDLE] = NULL;
	m_szGrp[TLK_PQUESTION] = "FG_PQUEST";
	m_szGrp[TLK_PLHURT1] = "!FG_CUREA";
	m_szGrp[TLK_PLHURT2] = "!FG_CUREB";
	m_szGrp[TLK_PLHURT3] = "!FG_CUREC";
	m_szGrp[TLK_SMELL] = "FG_SMELL";
	m_szGrp[TLK_WOUND] = "FG_WOUND";
	m_szGrp[TLK_MORTAL] = "FG_MORTAL";
	m_voicePitch = 100;
}

void COFMedicAlly::Precache()
{
	PRECACHE_MODEL( "models/hgrunt_medic.mdl" );
	COFMedicAlly::TalkInit();
	PRECACHE_SOUND( "hgrunt/gr_mgun1.wav" );
	PRECACHE_SOUND( "hgrunt/gr_mgun2.wav" );
	PRECACHE_SOUND( "fgrunt/death1.wav" );
	PRECACHE_SOUND( "fgrunt/death2.wav" );
	PRECACHE_SOUND( "fgrunt/death3.wav" );
	PRECACHE_SOUND( "fgrunt/death4.wav" );
	PRECACHE_SOUND( "fgrunt/death5.wav" );
	PRECACHE_SOUND( "fgrunt/death6.wav" );
	PRECACHE_SOUND( "fgrunt/pain1.wav" );
	PRECACHE_SOUND( "fgrunt/pain2.wav" );
	PRECACHE_SOUND( "fgrunt/pain3.wav" );
	PRECACHE_SOUND( "fgrunt/pain4.wav" );
	PRECACHE_SOUND( "fgrunt/pain5.wav" );
	PRECACHE_SOUND( "fgrunt/pain6.wav" );
	PRECACHE_SOUND( "hgrunt/gr_reload1.wav" );
	PRECACHE_SOUND( "weapons/desert_eagle_fire.wav" );
	PRECACHE_SOUND( "weapons/desert_eagle_reload.wav" );
	PRECACHE_SOUND( "weapons/sbarrel1.wav" );
	PRECACHE_SOUND( "fgrunt/medic_give_shot.wav" );
	PRECACHE_SOUND( "fgrunt/medical.wav" );
	PRECACHE_SOUND( "zombie/claw_miss2.wav" );
	m_voicePitch = 105;
	m_iBrassShell = PRECACHE_MODEL( "models/shell.mdl" );
	COFAllyMonster::Precache();
}

void COFMedicAlly::Spawn()
{
	Precache();
	SET_MODEL( edict(), "models/hgrunt_medic.mdl" );
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_RED;
	pev->effects = 0;
	pev->health = gSkillData.medicAllyHealth;
	pev->view_ofs = Vector( 0, 0, 50 );
	m_MonsterState = MONSTERSTATE_NONE;
	m_flFieldOfView = -0.7f;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	m_flNextPainTime = gpGlobals->time;
	m_HackedGunPos = Vector( 0, 0, 55 );
	m_iSentence = MEDIC_SENT_NONE;
	m_afCapability = bits_CAP_SQUAD | bits_CAP_TURN_HEAD | bits_CAP_HEAR | bits_CAP_DOORS_GROUP;
	m_fEnemyEluded = FALSE;
	m_fFirstEncounter = TRUE;
	pev->body = 0;
	m_iHealCharge = (int)gSkillData.medicAllyHeal;
	m_flLastUseTime = 0;
	m_fGunHolstered = FALSE;
	m_fHypoHolstered = TRUE;
	m_fHealActive = FALSE;
	m_fQueueFollow = FALSE;
	m_fUseHealing = FALSE;
	m_fHealing = FALSE;
	m_hNewTargetEnt = NULL;
	m_fFollowChecked = FALSE;
	m_fFollowChecking = FALSE;
	if ( !pev->weapons )
		pev->weapons = MEDIC_9MMHANDGUN;
	if ( m_iBlackOrWhite == -1 )
		m_iBlackOrWhite = RANDOM_LONG( 0, 99 ) % 2;
	if ( pev->weapons & MEDIC_9MMHANDGUN )
	{
		pev->body = m_iBlackOrWhite + 2;
		m_iWeaponIdx = 1;
		m_cClipSize = 9;
	}
	else if ( pev->weapons & MEDIC_EAGLE )
	{
		pev->body = m_iBlackOrWhite;
		m_iWeaponIdx = 0;
		m_cClipSize = 9;
	}
	else if ( pev->weapons & MEDIC_NEEDLE )
	{
		pev->body = m_iBlackOrWhite + 4;
		m_iWeaponIdx = 2;
		m_cClipSize = 1;
		m_fGunHolstered = TRUE;
		m_fHypoHolstered = FALSE;
	}
	else
	{
		pev->body = m_iBlackOrWhite + 6;
		m_iWeaponIdx = 3;
		m_cClipSize = 0;
	}
	m_cAmmoLoaded = m_cClipSize;
	m_flLastShot = gpGlobals->time;
	pev->skin = 0;
	if ( m_iBlackOrWhite == 1 )
		m_voicePitch = 95;
	COFAllyMonster::g_talkWaitTime = 0;
	MonsterInit();
	SetUse( &COFMedicAlly::HealerUse );
}

void COFMedicAlly::KeyValue( KeyValueData* data )
{
	if ( FStrEq( data->szKeyName, "head" ) )
	{
		m_iBlackOrWhite = atoi( data->szValue );
		data->fHandled = TRUE;
	}
	else
		COFAllyMonster::KeyValue( data );
}

void COFMedicAlly::DeclineFollowing()
{
	PlaySentence( "FG_POK", 2, VOL_NORM, ATTN_NORM );
}

MONSTERSTATE COFMedicAlly::GetIdealState()
{
	return COFSquadTalkMonster::GetIdealState();
}

BOOL COFMedicAlly::FOkToSpeak()
{
	return gpGlobals->time > COFAllyMonster::g_talkWaitTime &&
		( !( pev->spawnflags & SF_MONSTER_GAG ) || m_MonsterState == MONSTERSTATE_COMBAT );
}

void COFMedicAlly::JustSpoke()
{
	COFAllyMonster::g_talkWaitTime = gpGlobals->time + RANDOM_FLOAT( 1.5, 2 );
	m_iSentence = MEDIC_SENT_NONE;
}

void COFMedicAlly::SpeakSentence()
{
	if ( m_iSentence != MEDIC_SENT_NONE && FOkToSpeak() )
	{
		SENTENCEG_PlayRndSz( edict(), pMedicSentences[m_iSentence], MEDIC_SENTENCE_VOLUME,
			ATTN_NORM, 0, m_voicePitch );
		JustSpoke();
	}
}

void COFMedicAlly::AlertSound()
{
	if ( m_hEnemy != NULL && FOkToSpeak() )
		PlaySentence( "FG_ATTACK", RANDOM_FLOAT( 2.8, 3.2 ), VOL_NORM, ATTN_IDLE );
}

void COFMedicAlly::PainSound()
{
	if ( gpGlobals->time <= m_flNextPainTime )
		return;
	const char* sounds[] =
	{
		"fgrunt/pain3.wav", "fgrunt/pain4.wav", "fgrunt/pain5.wav",
		"fgrunt/pain1.wav", "fgrunt/pain2.wav", "fgrunt/pain6.wav"
	};
	int choice = RANDOM_LONG( 0, 7 );
	if ( choice < ARRAYSIZE( sounds ) )
		EMIT_SOUND( edict(), CHAN_VOICE, sounds[choice], 1, ATTN_NORM );
	m_flNextPainTime = gpGlobals->time + 1;
}

void COFMedicAlly::DeathSound()
{
	const char* sounds[] =
	{
		"fgrunt/gr_death1.wav", "fgrunt/gr_death2.wav", "fgrunt/gr_death3.wav",
		"fgrunt/gr_death4.wav", "fgrunt/gr_death5.wav", "fgrunt/gr_death6.wav"
	};
	EMIT_SOUND( edict(), CHAN_VOICE, sounds[RANDOM_LONG( 0, 5 )], 1, ATTN_IDLE );
}

void COFMedicAlly::IdleSound()
{
	if ( !FOkToSpeak() || ( !g_fMedicAllyQuestion && !RANDOM_LONG( 0, 1 ) ) )
		return;
	if ( g_fMedicAllyQuestion )
	{
		if ( g_fMedicAllyQuestion == 1 )
			SENTENCEG_PlayRndSz( edict(), "FG_CLEAR", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
		else if ( g_fMedicAllyQuestion == 2 )
			SENTENCEG_PlayRndSz( edict(), "FG_ANSWER", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
		g_fMedicAllyQuestion = 0;
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			SENTENCEG_PlayRndSz( edict(), "FG_CHECK", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
			g_fMedicAllyQuestion = 1;
			break;
		case 1:
			SENTENCEG_PlayRndSz( edict(), "FG_QUEST", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
			g_fMedicAllyQuestion = 2;
			break;
		case 2:
			SENTENCEG_PlayRndSz( edict(), "FG_IDLE", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
			break;
		}
	}
	JustSpoke();
}

BOOL COFMedicAlly::FCanCheckAttacks()
{
	return !HasConditions( bits_COND_ENEMY_TOOFAR );
}

void COFMedicAlly::CheckAmmo()
{
	if ( m_cAmmoLoaded <= 0 )
		SetConditions( bits_COND_NO_AMMO_LOADED );
}

Vector COFMedicAlly::GetGunPosition()
{
	return pev->origin + Vector( 0, 0, m_fStanding ? 60 : 48 );
}

#pragma warning( push )
#pragma warning( disable : 4701 4703 )
BOOL COFMedicAlly::CheckMeleeAttack1( float dot, float distance )
{
	if ( m_hEnemy == NULL )
		return FALSE;
	CBaseMonster* enemy = m_hEnemy->MyMonsterPointer();
	if ( !enemy )
		return FALSE;
	return distance <= 64 && dot >= 0.7 && enemy->Classify() != CLASS_ALIEN_BIOWEAPON &&
		enemy->Classify() != CLASS_PLAYER_BIOWEAPON;
}
#pragma warning( pop )

BOOL COFMedicAlly::CheckRangeAttack1( float dot, float distance )
{
	if ( distance > 1024 || dot < 0.5 )
		return FALSE;
	Vector source = pev->origin + Vector( 0, 0, 55 );
	CBaseEntity* enemy = m_hEnemy;
	Vector end = enemy->BodyTarget( source ) - enemy->pev->origin + m_vecEnemyLKP;
	TraceResult trace;
	UTIL_TraceLine( source, end, dont_ignore_monsters, edict(), &trace );
	m_lastAttackCheck = trace.flFraction == 1 || ( trace.pHit && CBaseEntity::Instance( trace.pHit ) == enemy );
	return m_lastAttackCheck;
}

BOOL COFMedicAlly::CheckRangeAttack2( float, float )
{
	if ( !( pev->weapons & MEDIC_HANDGRENADE ) )
		return FALSE;
	if ( m_flGroundSpeed != 0 )
	{
		m_fThrowGrenade = FALSE;
		return FALSE;
	}
	if ( gpGlobals->time < m_flNextGrenadeCheck )
		return m_fThrowGrenade;
	if ( !( m_hEnemy->pev->flags & FL_ONGROUND ) && m_hEnemy->pev->waterlevel == 0 &&
		m_vecEnemyLKP.z > pev->absmax.z )
	{
		m_fThrowGrenade = FALSE;
		return FALSE;
	}
	Vector target;
	if ( pev->weapons & MEDIC_HANDGRENADE )
	{
		if ( RANDOM_LONG( 0, 1 ) )
			target = Vector( m_hEnemy->pev->origin.x, m_hEnemy->pev->origin.y, m_hEnemy->pev->absmin.z );
		else
			target = m_vecEnemyLKP;
	}
	else
	{
		target = m_hEnemy->BodyTarget( pev->origin ) - m_hEnemy->pev->origin + m_vecEnemyLKP;
		if ( HasConditions( bits_COND_SEE_ENEMY ) )
			target = target + m_hEnemy->pev->velocity * ( ( target - pev->origin ).Length() / gSkillData.medicAllyGrenadeSpeed );
	}
	if ( InSquad() && SquadMemberInRange( target, 256 ) )
	{
		m_fThrowGrenade = FALSE;
		m_flNextGrenadeCheck = gpGlobals->time + 1;
	}
	if ( ( target - pev->origin ).Length2D() <= 256 )
	{
		m_fThrowGrenade = FALSE;
		m_flNextGrenadeCheck = gpGlobals->time + 1;
		return FALSE;
	}
	Vector velocity;
	if ( pev->weapons & MEDIC_HANDGRENADE )
		velocity = VecCheckToss( pev, GetGunPosition(), target, 0.5 );
	else
		velocity = VecCheckThrow( pev, GetGunPosition(), target, gSkillData.medicAllyGrenadeSpeed, 0.5 );
	if ( velocity != g_vecZero )
	{
		m_vecTossVelocity = velocity;
		m_fThrowGrenade = TRUE;
		m_flNextGrenadeCheck = gpGlobals->time + ( ( pev->weapons & MEDIC_HANDGRENADE ) ? 0 : 0.3 );
		return TRUE;
	}
	m_fThrowGrenade = FALSE;
	m_flNextGrenadeCheck = gpGlobals->time + 1;
	return FALSE;
}

void COFMedicAlly::PrescheduleThink()
{
	if ( InSquad() && m_hEnemy != NULL )
	{
		if ( HasConditions( bits_COND_SEE_ENEMY ) )
			MySquadLeader()->m_flLastEnemySightTime = gpGlobals->time;
		else if ( gpGlobals->time - MySquadLeader()->m_flLastEnemySightTime > 5 )
			MySquadLeader()->m_fEnemyEluded = TRUE;
	}
}

void COFMedicAlly::Shoot()
{
	if ( m_hEnemy == NULL || gpGlobals->time - m_flLastShot <= 0.11 )
		return;
	Vector source = GetGunPosition();
	Vector direction = ShootAtEnemy( source );
	UTIL_MakeVectors( pev->angles );
	int pitch = RANDOM_LONG( 0, 20 );
	pitch = pitch <= 10 ? pitch - 5 : 0;
	if ( pev->weapons & MEDIC_9MMHANDGUN )
	{
		FireBullets( 1, source, direction, VECTOR_CONE_1DEGREES, 1024, BULLET_MONSTER_9MM );
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/pl_gun3.wav", 1, ATTN_NORM, 0, 100 + pitch );
	}
	else if ( pev->weapons & MEDIC_EAGLE )
	{
		FireBullets( 1, source, direction, VECTOR_CONE_1DEGREES, 1024, BULLET_PLAYER_357 );
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/desert_eagle_fire.wav", 1, ATTN_NORM, 0, 100 + pitch );
	}
	pev->effects |= EF_MUZZLEFLASH;
	m_cAmmoLoaded--;
	SetBlending( 0, UTIL_VecToAngles( direction ).x );
	m_flLastShot = gpGlobals->time;
}

CBaseEntity* COFMedicAlly::Kick()
{
	UTIL_MakeVectors( pev->angles );
	Vector source = pev->origin + Vector( 0, 0, pev->size.z * 0.5 );
	TraceResult trace;
	UTIL_TraceHull( source, source + gpGlobals->v_forward * 70, dont_ignore_monsters, head_hull, edict(), &trace );
	return trace.pHit ? CBaseEntity::Instance( trace.pHit ) : NULL;
}

void COFMedicAlly::HandleAnimEvent( MonsterEvent_t* event )
{
	switch ( event->event )
	{
	case 2:
		EMIT_SOUND( edict(), CHAN_WEAPON, pev->weapons & MEDIC_EAGLE ?
			"weapons/desert_eagle_reload.wav" : "hgrunt/gr_reload1.wav", 1, ATTN_NORM );
		m_cAmmoLoaded = m_cClipSize;
		ClearConditions( bits_COND_NO_AMMO_LOADED );
		break;
	case 3:
		{
			CBaseEntity* target = Kick();
			if ( target )
			{
				UTIL_MakeVectors( pev->angles );
				target->pev->punchangle.x = 15;
				target->pev->velocity = target->pev->velocity + gpGlobals->v_forward * 100 + gpGlobals->v_up * 50;
				target->TakeDamage( pev, pev, gSkillData.medicAllyDmgKick, DMG_CLUB );
			}
		}
		break;
	case 4:
		Shoot();
		break;
	case 7:
		UTIL_MakeVectors( pev->angles );
		CGrenade::ShootTimed( pev, GetGunPosition(), m_vecTossVelocity, 3.5 );
		m_fThrowGrenade = FALSE;
		m_flNextGrenadeCheck = gpGlobals->time + 6;
		break;
	case 9:
		UTIL_MakeVectors( pev->angles );
		CGrenade::ShootTimed( pev, pev->origin + gpGlobals->v_forward * 17 - gpGlobals->v_right * 27 + gpGlobals->v_up * 6,
			g_vecZero, 3 );
		break;
	case 10:
		if ( FOkToSpeak() )
		{
			SENTENCEG_PlayRndSz( edict(), "FG_ALERT", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
			JustSpoke();
		}
		COFAllyMonster::HandleAnimEvent( event );
		break;
	case 11:
		{
			Vector origin, angles;
			GetAttachment( 0, origin, angles );
			pev->body = m_iBlackOrWhite + 6;
			DropItem( pev->weapons & MEDIC_9MMHANDGUN ? "weapon_9mmhandgun" : "weapon_eagle", origin, angles );
			m_iWeaponIdx = 3;
		}
		break;
	case 15:
		pev->body = m_iBlackOrWhite + 6;
		m_fGunHolstered = TRUE;
		break;
	case 16:
		pev->body = m_iBlackOrWhite + 4;
		m_fHypoHolstered = FALSE;
		break;
	case 17:
		pev->body = m_iBlackOrWhite + 6;
		m_fHypoHolstered = TRUE;
		break;
	case 18:
		pev->body = m_iBlackOrWhite + ( ( pev->weapons & MEDIC_9MMHANDGUN ) ? 2 : 0 );
		m_fGunHolstered = FALSE;
		break;
	default:
		COFAllyMonster::HandleAnimEvent( event );
		break;
	}
}

void COFMedicAlly::SetActivity( Activity activity )
{
	GET_MODEL_PTR( edict() );
	int sequence;
	switch ( activity )
	{
	case ACT_IDLE:
		if ( m_MonsterState == MONSTERSTATE_COMBAT )
			activity = ACT_IDLE_ANGRY;
		sequence = LookupActivity( activity );
		break;
	case ACT_WALK:
		sequence = LookupActivity( pev->health > 20 ? ACT_WALK : ACT_WALK_HURT );
		break;
	case ACT_RUN:
		sequence = LookupActivity( pev->health > 20 ? ACT_RUN : ACT_RUN_HURT );
		break;
	case ACT_RANGE_ATTACK1:
		sequence = LookupSequence( m_fStanding ? "standing_mp5" : "crouching_mp5" );
		break;
	default:
		CBaseMonster::SetActivity( activity );
		return;
	}
	m_Activity = activity;
	if ( sequence < 0 )
	{
		ALERT( at_console, "%s has no sequence for act:%d\n", STRING( pev->classname ), activity );
		pev->sequence = 0;
		return;
	}
	if ( pev->sequence != sequence || !m_fSequenceLoops )
		pev->frame = 0;
	pev->sequence = sequence;
	ResetSequenceInfo();
	SetYawSpeed();
}

void COFMedicAlly::MonsterThink()
{
	if ( m_fFollowChecking && !m_fFollowChecked && gpGlobals->time - m_flFollowCheckTime > 0.5 )
	{
		m_fFollowChecking = FALSE;
		CBaseEntity* player = UTIL_FindEntityByClassname( NULL, "player" );
		COFSquadTalkMonster::FollowerUse( player, player, USE_TOGGLE, 0 );
	}
	CBaseMonster::MonsterThink();
}

void COFMedicAlly::HealerUse( CBaseEntity* activator, CBaseEntity*, USE_TYPE, float )
{
	if ( m_useTime > gpGlobals->time || m_flLastUseTime > gpGlobals->time || activator == m_hEnemy )
		return;
	if ( !m_fFollowChecked )
	{
		if ( !m_fFollowChecking )
		{
			m_fFollowChecking = TRUE;
			m_flFollowCheckTime = gpGlobals->time;
			return;
		}
		if ( gpGlobals->time - m_flFollowCheckTime < 0.3 )
			return;
		m_fFollowChecked = TRUE;
		m_fFollowChecking = FALSE;
	}
	BOOL changedTarget = FALSE;
	if ( !m_fUseHealing && m_hTargetEnt != NULL && m_fHealing )
	{
		if ( activator->pev->health >= activator->pev->max_health )
			return;
		changedTarget = TRUE;
		COFSquadTalkMonster* oldTarget = m_hTargetEnt->MySquadTalkMonsterPointer();
		m_fHealing = FALSE;
		if ( oldTarget )
			oldTarget->m_hWaitMedic = NULL;
	}
	if ( m_iHealCharge > 0 && activator->IsAlive() && activator->pev->max_health > activator->pev->health )
	{
		if ( !m_fHealing )
		{
			if ( m_hTargetEnt != NULL && m_hTargetEnt->IsPlayer() )
				StopFollowing( FALSE );
			m_hTargetEnt = activator;
			m_fHealing = TRUE;
			m_fUseHealing = TRUE;
			ClearSchedule();
			m_fHealAudioPlaying = FALSE;
			if ( changedTarget )
				ChangeSchedule( slMedicAllyNewHealTarget );
			else
			{
				SENTENCEG_PlayRndSz( edict(), "MG_HEAL", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
				ChangeSchedule( slMedicAllyDrawNeedle );
			}
		}
		int amount = m_fHealActive ? 2 : 1;
		if ( activator->TakeHealth( (float)amount, DMG_GENERIC ) )
			m_iHealCharge -= amount;
	}
	else
	{
		m_fFollowChecked = FALSE;
		m_fFollowChecking = FALSE;
		if ( gpGlobals->time - m_flLastRejectAudio > 4 && m_iHealCharge <= 0 && !m_fHealing )
		{
			m_flLastRejectAudio = gpGlobals->time;
			SENTENCEG_PlayRndSz( edict(), "MG_NOTHEAL", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
		}
	}
	m_flLastUseTime = gpGlobals->time + 0.2;
}

void COFMedicAlly::HealerActivate( CBaseMonster* target )
{
	if ( m_hTargetEnt != NULL )
	{
		COFSquadTalkMonster* oldTarget = m_hTargetEnt->MySquadTalkMonsterPointer();
		if ( oldTarget )
			oldTarget->m_hWaitMedic = NULL;
		( (COFSquadTalkMonster*)target )->m_hWaitMedic = this;
		m_hTargetEnt = target;
		m_fHealing = FALSE;
		ClearSchedule();
		ChangeSchedule( slMedicAllyNewHealTarget );
		return;
	}
	if ( m_iHealCharge > 0 && target->IsAlive() && target->pev->max_health > target->pev->health && !m_fHealing )
	{
		if ( m_hTargetEnt != NULL && m_hTargetEnt->IsPlayer() )
			StopFollowing( FALSE );
		m_hTargetEnt = target;
		if ( target->MySquadTalkMonsterPointer() )
			( (COFSquadTalkMonster*)target )->m_hWaitMedic = this;
		m_fHealing = TRUE;
		ClearSchedule();
		EMIT_SOUND( edict(), CHAN_VOICE, "fgrunt/medical.wav", 1, ATTN_NORM );
		ChangeSchedule( slMedicAllyDrawNeedle );
	}
}

BOOL COFMedicAlly::HealMe( COFSquadTalkMonster* target )
{
	if ( target )
	{
		if ( m_hTargetEnt != NULL )
		{
			if ( m_hTargetEnt->IsPlayer() )
				return FALSE;
			COFSquadTalkMonster* current = m_hTargetEnt->MySquadTalkMonsterPointer();
			if ( current && current->MySquadLeader() == MySquadLeader() )
				return FALSE;
			COFSquadTalkMonster* requested = target->MySquadTalkMonsterPointer();
			if ( requested && requested->MySquadLeader() != MySquadLeader() )
				return FALSE;
		}
		if ( m_MonsterState != MONSTERSTATE_COMBAT && m_iHealCharge != 0 )
		{
			HealerActivate( target );
			return TRUE;
		}
	}
	else
	{
		if ( m_hTargetEnt != NULL )
		{
			COFSquadTalkMonster* current = m_hTargetEnt->MySquadTalkMonsterPointer();
			if ( current )
				current->m_hWaitMedic = NULL;
		}
		m_hTargetEnt = NULL;
		if ( m_movementGoal == MOVEGOAL_TARGETENT )
			RouteClear();
		ClearSchedule();
		ChangeSchedule( slMedicAllyDrawGun );
	}
	return FALSE;
}

void COFMedicAlly::HealOff()
{
	m_fHealing = FALSE;
	if ( m_movementGoal == MOVEGOAL_TARGETENT )
		RouteClear();
	m_hTargetEnt = NULL;
	ClearSchedule();
	SetThink( NULL );
	pev->nextthink = 0;
}

void COFMedicAlly::StartTask( Task_t* task )
{
	TaskBegin();
	switch ( task->iTask )
	{
	case TASK_RUN_PATH:
	case TASK_WALK_PATH:
		Forget( bits_MEMORY_INCOVER );
		COFAllyMonster::StartTask( task );
		break;
	case TASK_FACE_IDEAL:
	case TASK_FACE_ENEMY:
		COFAllyMonster::StartTask( task );
		if ( pev->movetype == MOVETYPE_FLY )
			m_IdealActivity = ACT_GLIDE;
		break;
	case TASK_MELEE_ATTACK2:
		m_IdealActivity = ACT_MELEE_ATTACK2;
		if ( !m_fHealAudioPlaying )
		{
			EMIT_SOUND( edict(), CHAN_WEAPON, "fgrunt/medic_give_shot.wav", 1, ATTN_NORM );
			m_fHealAudioPlaying = TRUE;
		}
		break;
	case TASK_RELOAD:
		m_IdealActivity = ACT_RELOAD;
		break;
	case TASK_WAIT_FOR_MOVEMENT:
		if ( m_fHealing )
		{
			BOOL hadTarget = m_hTargetEnt != NULL;
			if ( hadTarget )
			{
				COFSquadTalkMonster* target = m_hTargetEnt->MySquadTalkMonsterPointer();
				if ( target )
					target->m_hWaitMedic = NULL;
			}
			m_fHealing = FALSE;
			m_fUseHealing = FALSE;
			STOP_SOUND( edict(), CHAN_WEAPON, "fgrunt/medic_give_shot.wav" );
			m_fFollowChecked = FALSE;
			m_fFollowChecking = FALSE;
			if ( m_movementGoal == MOVEGOAL_TARGETENT )
				RouteClear();
			m_fHealActive = FALSE;
			if ( !hadTarget )
			{
				m_IdealActivity = ACT_DISARM;
				break;
			}
			m_hTargetEnt = NULL;
		}
		COFAllyMonster::StartTask( task );
		break;
	case TASK_MEDIC_FACE_TOSS_DIR:
		break;
	case TASK_MEDIC_SPEAK_SENTENCE:
		SpeakSentence();
		TaskComplete();
		break;
	case TASK_MEDIC_CHECK_FIRE:
		if ( !NoFriendlyFire() )
			SetConditions( bits_COND_MEDIC_NOFIRE );
		TaskComplete();
		break;
	default:
		COFAllyMonster::StartTask( task );
		break;
	}
}

void COFMedicAlly::RunTask( Task_t* task )
{
	if ( task->iTask == TASK_MEDIC_FACE_TOSS_DIR )
	{
		MakeIdealYaw( pev->origin + m_vecTossVelocity * 64 );
		ChangeYaw( (int)pev->yaw_speed );
		if ( FacingIdeal() )
			TaskComplete();
	}
	else if ( task->iTask == TASK_MELEE_ATTACK2 )
	{
		if ( !m_fSequenceFinished )
			return;
		if ( m_fUseHealing )
		{
			if ( gpGlobals->time - m_flLastUseTime > 0.3 )
				m_Activity = ACT_RESET;
		}
		else
		{
			m_fHealActive = TRUE;
			if ( m_hTargetEnt != NULL )
			{
				CBaseEntity* target = m_hTargetEnt;
				float amount = min( target->pev->max_health - target->pev->health, 5.0f );
				if ( amount == 0 || !target->TakeHealth( amount, DMG_GENERIC ) )
					m_Activity = ACT_RESET;
				else
					m_iHealCharge = (int)( m_iHealCharge - amount );
			}
			else
				m_Activity = m_fHealing ? ACT_MELEE_ATTACK2 : ACT_RESET;
		}
		TaskComplete();
	}
	else
		COFAllyMonster::RunTask( task );
}

int COFMedicAlly::TakeDamage( entvars_t* inflictor, entvars_t* attacker, float damage, int damageType )
{
	int result = COFSquadTalkMonster::TakeDamage( inflictor, attacker, damage, damageType );
	if ( !IsAlive() || pev->deadflag == DEAD_DYING )
		return result;
	Forget( bits_MEMORY_INCOVER );
	if ( m_MonsterState != MONSTERSTATE_PRONE && ( attacker->flags & FL_CLIENT ) )
	{
		m_flPlayerDamage += damage;
		if ( m_hEnemy != NULL )
		{
			if ( !m_hEnemy->IsPlayer() && pev->deadflag == DEAD_NO )
				PlaySentence( "FG_SHOT", 4, VOL_NORM, ATTN_NORM );
		}
		else
		{
			BOOL facing = FALSE;
			if ( gpGlobals->time - m_flLastHitByPlayer < 4 && m_iPlayerHits > 2 )
			{
				if ( m_afMemory & bits_MEMORY_SUSPICIOUS )
					facing = TRUE;
				else
				{
					Vector direction = pev->origin - attacker->origin;
					direction.z = 0;
					direction = direction.Normalize();
					Vector angles = attacker->v_angle;
					angles.x = 0;
					Vector forward;
					UTIL_MakeVectorsPrivate( angles, forward, NULL, NULL );
					facing = DotProduct( direction, forward ) > 0.96;
				}
			}
			if ( facing )
			{
				PlaySentence( "FG_MAD", 4, VOL_NORM, ATTN_NORM );
				Remember( bits_MEMORY_PROVOKED );
				StopFollowing( TRUE );
			}
			else
			{
				PlaySentence( "FG_SHOT", 4, VOL_NORM, ATTN_NORM );
				Remember( bits_MEMORY_SUSPICIOUS );
				if ( gpGlobals->time - m_flLastHitByPlayer >= 4 )
					m_iPlayerHits = 0;
				else
					m_iPlayerHits++;
			}
		}
	}
	return result;
}

void COFMedicAlly::TraceAttack( entvars_t* attacker, float damage, Vector direction, TraceResult* trace, int damageType )
{
	switch ( trace->iHitgroup )
	{
	case HITGROUP_CHEST:
	case HITGROUP_STOMACH:
		if ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_BLAST ) )
			damage *= 0.5;
		break;
	case 11:
		if ( damageType & ( DMG_BULLET | DMG_SLASH | DMG_CLUB ) )
		{
			damage -= 20;
			if ( damage <= 0 )
			{
				UTIL_Ricochet( trace->vecEndPos, 1 );
				damage = 0.01f;
			}
		}
		trace->iHitgroup = HITGROUP_HEAD;
		break;
	}
	CBaseMonster::TraceAttack( attacker, damage, direction, trace, damageType );
}

void COFMedicAlly::Killed( entvars_t* attacker, int gib )
{
	if ( m_hTargetEnt != NULL )
	{
		COFSquadTalkMonster* target = m_hTargetEnt->MySquadTalkMonsterPointer();
		if ( target )
			target->m_hWaitMedic = NULL;
	}
	SetUse( NULL );
	COFSquadTalkMonster::Killed( attacker, gib );
}

void COFMedicAlly::GibMonster()
{
	if ( m_iWeaponIdx != 3 )
	{
		Vector origin, angles;
		GetAttachment( 0, origin, angles );
		CBaseEntity* gun = DropItem( pev->weapons & MEDIC_9MMHANDGUN ? "weapon_9mmhandgun" : "weapon_eagle", origin, angles );
		if ( gun )
		{
			float vertical = RANDOM_FLOAT( 200, 300 );
			float lateral = RANDOM_FLOAT( -100, 100 );
			gun->pev->velocity.x = RANDOM_FLOAT( -100, 100 );
			gun->pev->velocity.y = lateral;
			gun->pev->velocity.z = vertical;
			float spin = RANDOM_FLOAT( 200, 400 );
			gun->pev->avelocity = Vector( 0, spin, 0 );
		}
		m_iWeaponIdx = 3;
	}
	CBaseMonster::GibMonster();
}

Schedule_t* COFMedicAlly::GetScheduleOfType( int type )
{
	switch ( type )
	{
	case SCHED_IDLE_STAND:
		{
			Schedule_t* schedule = COFSquadTalkMonster::GetScheduleOfType( type );
			return schedule == slIdleStand ? slMedicAllyIdleStand : schedule;
		}
	case SCHED_TARGET_FACE:
		{
			Schedule_t* schedule = COFSquadTalkMonster::GetScheduleOfType( type );
			return schedule == slIdleStand ? slMedicAllyFaceTarget : schedule;
		}
	case SCHED_TARGET_CHASE:
		return slMedicAllyFollow;
	case SCHED_COMBAT_FACE:
		return slMedicAllyCombatFace;
	case SCHED_VICTORY_DANCE:
		return InSquad() && !IsLeader() ? slMedicAllyFail : slMedicAllyVictoryDance;
	case SCHED_TAKE_COVER_FROM_ENEMY:
		if ( InSquad() && g_iSkillLevel == SKILL_HARD && HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) &&
			OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
		{
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "FG_THROW", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}
			return slMedicAllyTossGrenadeCover;
		}
		return slMedicAllyTakeCover;
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		return slMedicAllyTakeCoverFromBestSound;
	case SCHED_RANGE_ATTACK1:
		if ( RANDOM_LONG( 0, 9 ) == 0 )
			m_fStanding = RANDOM_LONG( 0, 1 );
		return m_fStanding ? slMedicAllyRangeAttack1B : slMedicAllyRangeAttack1A;
	case SCHED_RANGE_ATTACK2:
		return slMedicAllyRangeAttack2;
	case SCHED_FAIL:
		return m_hEnemy != NULL ? slMedicAllyCombatFail : slMedicAllyFail;
	case SCHED_CANT_FOLLOW:
		return slMedicAllyFail;
	case SCHED_MEDIC_SUPPRESS:
		if ( m_hEnemy->IsPlayer() && m_fFirstEncounter )
		{
			m_fFirstEncounter = FALSE;
			return slMedicAllySignalSuppress;
		}
		return slMedicAllySuppress;
	case SCHED_MEDIC_ESTABLISH_LINE_OF_FIRE:
		return slMedicAllyEstablishLineOfFire;
	case SCHED_MEDIC_COVER_AND_RELOAD:
		return slMedicAllyHideReload;
	case SCHED_MEDIC_SWEEP:
		return slMedicAllySweep;
	case SCHED_MEDIC_FOUND_ENEMY:
		return slMedicAllyFoundEnemy;
	case SCHED_MEDIC_REPEL:
		if ( pev->velocity.z > -128 )
			pev->velocity.z -= 32;
		return slMedicAllyRepel;
	case SCHED_MEDIC_REPEL_ATTACK:
		if ( pev->velocity.z > -128 )
			pev->velocity.z -= 32;
		return slMedicAllyRepelAttack;
	case SCHED_MEDIC_REPEL_LAND:
		return slMedicAllyRepelLand;
	case SCHED_MEDIC_WAIT_FACE_ENEMY:
		return slMedicAllyWaitInCover;
	case SCHED_MEDIC_TAKECOVER_FAILED:
		if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) && OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
			return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
		return GetScheduleOfType( SCHED_FAIL );
	case SCHED_MEDIC_HEAL_TARGET:
		return slMedicAllyHealTarget;
	case SCHED_MEDIC_ELOF_FAIL:
		return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
	default:
		return COFSquadTalkMonster::GetScheduleOfType( type );
	}
}

Schedule_t* COFMedicAlly::GetSchedule()
{
	m_iSentence = MEDIC_SENT_NONE;
	if ( pev->movetype == MOVETYPE_FLY && m_MonsterState != MONSTERSTATE_PRONE )
	{
		if ( pev->flags & FL_ONGROUND )
		{
			pev->movetype = MOVETYPE_STEP;
			return GetScheduleOfType( SCHED_MEDIC_REPEL_LAND );
		}
		return GetScheduleOfType( m_MonsterState == MONSTERSTATE_COMBAT ? SCHED_MEDIC_REPEL_ATTACK : SCHED_MEDIC_REPEL );
	}
	if ( m_fHealing )
	{
		if ( m_hTargetEnt != NULL )
		{
			CBaseEntity* target = m_hTargetEnt;
			if ( ( target->pev->origin - pev->origin ).Length2D() <= 50 &&
				( !m_fUseHealing || gpGlobals->time - m_flLastUseTime <= 0.25 ) && m_iHealCharge != 0 &&
				target->IsAlive() && target->pev->health != target->pev->max_health )
			{
				return slMedicAllyHealTarget;
			}
		}
		return slMedicAllyDrawGun;
	}
	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound* sound = PBestSound();
		if ( sound && ( sound->m_iType & bits_SOUND_DANGER ) )
		{
			if ( FOkToSpeak() )
			{
				SENTENCEG_PlayRndSz( edict(), "FG_GREN", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
				JustSpoke();
			}
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
		}
	}
	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
		PlaySentence( "FG_KILL", 4, VOL_NORM, ATTN_NORM );
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_ALERT:
		if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			return GetScheduleOfType( SCHED_SMALL_FLINCH );
		if ( m_hEnemy == NULL && IsFollowing() && m_hTargetEnt != NULL )
		{
			if ( m_hTargetEnt->IsAlive() )
				return GetScheduleOfType( HasConditions( bits_COND_CLIENT_PUSH ) ? SCHED_MOVE_AWAY_FOLLOW : SCHED_TARGET_FACE );
			StopFollowing( FALSE );
		}
		else
		{
			if ( HasConditions( bits_COND_CLIENT_PUSH ) )
				return GetScheduleOfType( SCHED_MOVE_AWAY );
			TrySmellTalk();
		}
		break;
	case MONSTERSTATE_COMBAT:
		if ( HasConditions( bits_COND_ENEMY_DEAD ) || !m_fHypoHolstered )
			break;
		if ( HasAllConditions( bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE ) )
		{
			if ( !InSquad() )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );
			MySquadLeader()->m_fEnemyEluded = FALSE;
			if ( !IsLeader() )
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			if ( FOkToSpeak() )
			{
				if ( m_hEnemy != NULL && m_hEnemy->IsPlayer() )
					SENTENCEG_PlayRndSz( edict(), "FG_ALERT", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
				else if ( m_hEnemy != NULL && m_hEnemy->Classify() != CLASS_PLAYER_ALLY &&
					m_hEnemy->Classify() != CLASS_HUMAN_PASSIVE && m_hEnemy->Classify() != CLASS_MACHINE )
				{
					SENTENCEG_PlayRndSz( edict(), "FG_MONST", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
				}
				JustSpoke();
			}
			return GetScheduleOfType( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) ?
				SCHED_MEDIC_SUPPRESS : SCHED_MEDIC_ESTABLISH_LINE_OF_FIRE );
		}
		if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		if ( HasConditions( bits_COND_NO_AMMO_LOADED ) )
			return GetScheduleOfType( SCHED_MEDIC_COVER_AND_RELOAD );
		if ( HasConditions( bits_COND_LIGHT_DAMAGE ) )
		{
			if ( RANDOM_LONG( 0, 99 ) <= 90 && m_hEnemy != NULL )
			{
				if ( FOkToSpeak() )
					m_iSentence = MEDIC_SENT_COVER;
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
			}
			return GetScheduleOfType( SCHED_SMALL_FLINCH );
		}
		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );
		if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
		{
			if ( InSquad() && MySquadLeader()->m_fEnemyEluded && !HasConditions( bits_COND_ENEMY_FACING_ME ) )
			{
				MySquadLeader()->m_fEnemyEluded = FALSE;
				return GetScheduleOfType( SCHED_MEDIC_FOUND_ENEMY );
			}
			if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK1 );
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
		}
		if ( HasConditions( bits_COND_ENEMY_OCCLUDED ) )
		{
			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK2 ) && OccupySlot( bits_SLOTS_HGRUNT_GRENADE ) )
			{
				if ( FOkToSpeak() )
				{
					SENTENCEG_PlayRndSz( edict(), "FG_THROW", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
					JustSpoke();
				}
				return GetScheduleOfType( SCHED_RANGE_ATTACK2 );
			}
			if ( OccupySlot( bits_SLOTS_HGRUNT_ENGAGE ) )
			{
				if ( FOkToSpeak() )
					m_iSentence = MEDIC_SENT_CHARGE;
				return GetScheduleOfType( SCHED_MEDIC_ESTABLISH_LINE_OF_FIRE );
			}
			if ( !IsFollowing() )
			{
				if ( FOkToSpeak() && RANDOM_LONG( 0, 1 ) )
				{
					SENTENCEG_PlayRndSz( edict(), "FG_TAUNT", MEDIC_SENTENCE_VOLUME, ATTN_NORM, 0, m_voicePitch );
					JustSpoke();
				}
				return GetScheduleOfType( SCHED_STANDOFF );
			}
		}
		if ( !IsFollowing() && HasConditions( bits_COND_SEE_ENEMY ) && !HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_MEDIC_ESTABLISH_LINE_OF_FIRE );
		break;
	default:
		break;
	}
	return CBaseMonster::GetSchedule();
}

class COFMedicAllyRepel : public CBaseMonster
{
public:
	void Spawn();
	void Precache();
	void KeyValue( KeyValueData* data );
	void EXPORT RepelUse( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE useType, float value );
	int m_iSpriteTexture;
	int m_iBlackOrWhite;
	int m_iszUse;
	int m_iszUnUse;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

LINK_ENTITY_TO_CLASS( monster_medic_ally_repel, COFMedicAllyRepel );

void COFMedicAllyRepel::Spawn()
{
	Precache();
	pev->solid = SOLID_NOT;
	SetUse( &COFMedicAllyRepel::RepelUse );
}

void COFMedicAllyRepel::Precache()
{
	UTIL_PrecacheOther( "monster_human_grunt_ally" );
	m_iSpriteTexture = PRECACHE_MODEL( "sprites/rope.spr" );
}

void COFMedicAllyRepel::KeyValue( KeyValueData* data )
{
	if ( FStrEq( data->szKeyName, "head" ) )
	{
		m_iBlackOrWhite = atoi( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "UseSentence" ) )
	{
		m_iszUse = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else if ( FStrEq( data->szKeyName, "UnUseSentence" ) )
	{
		m_iszUnUse = ALLOC_STRING( data->szValue );
		data->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( data );
}

void COFMedicAllyRepel::RepelUse( CBaseEntity*, CBaseEntity*, USE_TYPE, float )
{
	TraceResult trace;
	UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -4096 ), dont_ignore_monsters, edict(), &trace );
	CBaseEntity* entity = Create( "monster_human_grunt_ally", pev->origin, pev->angles );
	COFMedicAlly* medic = (COFMedicAlly*)entity->MySquadTalkMonsterPointer();
	medic->pev->weapons = pev->weapons;
	medic->pev->netname = pev->netname;
	medic->pev->spawnflags |= pev->spawnflags & ( SF_SQUADMONSTER_LEADER | SF_MONSTER_PREDISASTER |
		SF_MONSTER_GAG | SF_MONSTER_HITMONSTERCLIP | SF_MONSTER_WAIT_TILL_SEEN | SF_MONSTER_WAIT_FOR_SCRIPT );
	if ( FClassnameIs( medic->pev, "monster_human_medic_ally" ) )
		medic->m_iBlackOrWhite = m_iBlackOrWhite;
	medic->m_iszUse = m_iszUse;
	medic->m_iszUnUse = m_iszUnUse;
	medic->Spawn();
	medic->pev->movetype = MOVETYPE_FLY;
	medic->pev->velocity = Vector( 0, 0, RANDOM_FLOAT( -196, -128 ) );
	medic->SetActivity( ACT_GLIDE );
	medic->m_vecLastPosition = trace.vecEndPos;
	CBeam* rope = CBeam::BeamCreate( "sprites/rope.spr", 10 );
	rope->PointEntInit( pev->origin + Vector( 0, 0, 112 ), medic->entindex() );
	rope->SetFlags( BEAM_FSOLID );
	rope->SetColor( 255, 255, 255 );
	rope->SetThink( &CBaseEntity::SUB_Remove );
	rope->pev->nextthink = gpGlobals->time + -4096.0 * trace.flFraction / medic->pev->velocity.z + 0.5;
	UTIL_Remove( this );
}
