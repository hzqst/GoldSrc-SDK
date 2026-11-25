#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

const Vector g_vecZero( 0, 0, 0 );
float cb_prematch_time;
int gmsgWeapPickup;
enginefuncs_t g_engfuncs;
globalvars_t* gpGlobals;

CBaseEntity::~CBaseEntity()
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
void CBaseEntity::TeamFortress_EMPExplode( entvars_t* grenade, float damage, float radius )
{
}
void CBaseEntity::TeamFortress_EMPRemove()
{
}
void CBaseEntity::TeamFortress_CalcEMPDmgRad( float& damage, float& radius )
{
}
void CBaseEntity::TeamFortress_TakeConcussionBlast( entvars_t* grenade, float bounce )
{
}
void CBaseEntity::TeamFortress_TakeEMPBlast( entvars_t* grenade )
{
}
const char* CBaseEntity::DB_GetItemName()
{
	return "";
}
int CBaseEntity::Save( CSave& save )
{
	return 1;
}
int CBaseEntity::Restore( CRestore& restore )
{
	return 1;
}
void CBaseEntity::KeyValue( KeyValueData* data )
{
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
BOOL CBaseEntity::IsInWorld()
{
	return TRUE;
}
int CBaseEntity::ShouldToggle( USE_TYPE type, BOOL state )
{
	return 0;
}
int CBaseEntity::DamageDecal( int damageType )
{
	return -1;
}
BOOL CBaseEntity::IsAlly( CBaseEntity* other )
{
	return TRUE;
}
CBaseEntity* CBaseEntity::GetNextTarget()
{
	return NULL;
}
CBaseEntity* CBaseEntity::FindTimer( int type )
{
	return NULL;
}
CBaseEntity* CBaseEntity::CreateTimer( int type )
{
	return NULL;
}
void CBaseEntity::Timer_Tranquilisation()
{
}
CBaseEntity* CBaseEntity::Create( char* name, const Vector& origin, const Vector& angles, edict_t* owner )
{
	return NULL;
}
void CBaseEntity::SUB_Remove()
{
}
void* CBaseEntity::operator new( size_t size, entvars_t* vars )
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
void CBaseEntity::TraceAttack( entvars_t* attacker, float damage, Vector direction, TraceResult* trace, int damageType )
{
}
void CBaseEntity::TraceBleed( float damage, Vector direction, TraceResult* trace, int damageType )
{
}
void CBaseEntity::FireBullets( ULONG shots, Vector source, Vector aiming, Vector spread, float range,
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
BOOL CBaseAnimating::GetSequenceFlags()
{
	return FALSE;
}
void CBaseAnimating::DispatchAnimEvents( float interval )
{
}
float CBaseAnimating::SetBoneController( int controller, float value )
{
	return 0;
}
void CBaseAnimating::InitBoneControllers()
{
}
float CBaseAnimating::SetBlending( int blender, float value )
{
	return 0;
}
void CBaseAnimating::GetBonePosition( int bone, Vector& origin, Vector& angles )
{
}
void CBaseAnimating::GetAttachment( int attachment, Vector& origin, Vector& angles )
{
}
int CBaseAnimating::FindTransition( int ending, int goal, int* direction )
{
	return -1;
}
void CBaseAnimating::GetAutomovement( Vector& origin, Vector& angles, float interval )
{
}
void CBaseAnimating::SetBodygroup( int group, int value )
{
}
int CBaseAnimating::GetBodygroup( int group )
{
	return 0;
}

CBaseEntity* CBaseMonster::CheckTraceHullAttack( float range, int damage, int damageType )
{
	return NULL;
}
void CBaseMonster::Eat( float duration )
{
}
BOOL CBaseMonster::FShouldEat()
{
	return TRUE;
}
void CBaseMonster::BarnacleVictimBitten( entvars_t* barnacle )
{
}
void CBaseMonster::BarnacleVictimReleased()
{
}
void CBaseMonster::Listen()
{
}
float CBaseMonster::FLSoundVolume( CSound* sound )
{
	return 0;
}
BOOL CBaseMonster::FValidateHintType( short hint )
{
	return FALSE;
}
void CBaseMonster::Look( int range )
{
}
int CBaseMonster::ISoundMask()
{
	return 0;
}
CSound* CBaseMonster::PBestSound()
{
	return NULL;
}
CSound* CBaseMonster::PBestScent()
{
	return NULL;
}
void CBaseMonster::MonsterThink()
{
}
void CBaseMonster::MonsterUse( CBaseEntity* activator, CBaseEntity* caller, USE_TYPE type, float value )
{
}
int CBaseMonster::IgnoreConditions()
{
	return 0;
}
void CBaseMonster::RouteClear()
{
}
void CBaseMonster::RouteNew()
{
}
BOOL CBaseMonster::FRouteClear()
{
	return FALSE;
}
BOOL CBaseMonster::FRefreshRoute()
{
	return FALSE;
}
BOOL CBaseMonster::MoveToEnemy( Activity activity, float waitTime )
{
	return FALSE;
}
BOOL CBaseMonster::MoveToLocation( Activity activity, float waitTime, const Vector& goal )
{
	return FALSE;
}
BOOL CBaseMonster::MoveToTarget( Activity activity, float waitTime )
{
	return FALSE;
}
BOOL CBaseMonster::MoveToNode( Activity activity, float waitTime, const Vector& goal )
{
	return FALSE;
}
void CBaseMonster::RouteSimplify( CBaseEntity* target )
{
}
BOOL CBaseMonster::FBecomeProne()
{
	return TRUE;
}
BOOL CBaseMonster::CheckRangeAttack1( float dot, float range )
{
	return FALSE;
}
BOOL CBaseMonster::CheckRangeAttack2( float dot, float range )
{
	return FALSE;
}
BOOL CBaseMonster::CheckMeleeAttack1( float dot, float range )
{
	return FALSE;
}
BOOL CBaseMonster::CheckMeleeAttack2( float dot, float range )
{
	return FALSE;
}
void CBaseMonster::CheckAttacks( CBaseEntity* target, float range )
{
}
BOOL CBaseMonster::FCanCheckAttacks()
{
	return FALSE;
}
int CBaseMonster::CheckEnemy( CBaseEntity* enemy )
{
	return 0;
}
void CBaseMonster::PushEnemy( CBaseEntity* enemy, Vector& lastPosition )
{
}
BOOL CBaseMonster::PopEnemy()
{
	return FALSE;
}
void CBaseMonster::SetActivity( Activity activity )
{
}
void CBaseMonster::SetSequenceByName( char* sequence )
{
}
int CBaseMonster::CheckLocalMove( const Vector& start, const Vector& end, CBaseEntity* target, float* range )
{
	return 0;
}
float CBaseMonster::OpenDoorAndWait( entvars_t* door )
{
	return 0;
}
void CBaseMonster::AdvanceRoute( float range )
{
}
int CBaseMonster::RouteClassify( int flags )
{
	return 0;
}
BOOL CBaseMonster::BuildRoute( const Vector& goal, int flags, CBaseEntity* target )
{
	return FALSE;
}
void CBaseMonster::InsertWaypoint( Vector location, int flags )
{
}
BOOL CBaseMonster::FTriangulate( const Vector& start, const Vector& end, float range,
	CBaseEntity* target, Vector* apex )
{
	return FALSE;
}
void CBaseMonster::Move( float interval )
{
}
BOOL CBaseMonster::ShouldAdvanceRoute( float range )
{
	return FALSE;
}
void CBaseMonster::MoveExecute( CBaseEntity* target, const Vector& direction, float interval )
{
}
void CBaseMonster::MonsterInit()
{
}
void CBaseMonster::MonsterInitThink()
{
}
void CBaseMonster::StartMonster()
{
}
void CBaseMonster::MovementComplete()
{
}
int CBaseMonster::TaskIsRunning()
{
	return 0;
}
int CBaseMonster::IRelationship( CBaseEntity* target )
{
	return 0;
}
BOOL CBaseMonster::FindCover( Vector threat, Vector viewOffset, float minimum, float maximum )
{
	return FALSE;
}
BOOL CBaseMonster::BuildNearestRoute( Vector threat, Vector viewOffset, float minimum, float maximum )
{
	return FALSE;
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
float CBaseMonster::FlYawDiff()
{
	return 0;
}
float CBaseMonster::ChangeYaw( int speed )
{
	return 0;
}
float CBaseMonster::VecToYaw( Vector direction )
{
	return 0;
}
void CBaseMonster::SetEyePosition()
{
}
void CBaseMonster::HandleAnimEvent( MonsterEvent_t* event )
{
}
Vector CBaseMonster::GetGunPosition()
{
	return g_vecZero;
}
void CBaseMonster::TraceAttack( entvars_t* attacker, float damage, Vector direction, TraceResult* trace, int damageType )
{
}
void CBaseMonster::MakeDamageBloodDecal( int amount, float noise, TraceResult* trace, const Vector& direction )
{
}
BOOL CBaseMonster::FGetNodeRoute( Vector destination )
{
	return TRUE;
}
int CBaseMonster::FindHintNode()
{
	return -1;
}
void CBaseMonster::ReportAIState()
{
}
void CBaseMonster::KeyValue( KeyValueData* data )
{
}
BOOL CBaseMonster::FCheckAITrigger()
{
	return FALSE;
}
int CBaseMonster::CanPlaySequence( BOOL disregardState, int interrupt )
{
	return FALSE;
}
BOOL CBaseMonster::FindLateralCover( const Vector& threat, const Vector& viewOffset )
{
	return FALSE;
}
Vector CBaseMonster::ShootAtEnemy( const Vector& origin )
{
	return g_vecZero;
}
BOOL CBaseMonster::FacingIdeal()
{
	return FALSE;
}
BOOL CBaseMonster::FCanActiveIdle()
{
	return FALSE;
}
void CBaseMonster::CorpseFallThink()
{
}
void CBaseMonster::MonsterInitDead()
{
}
BOOL CBaseMonster::BBoxFlat()
{
	return TRUE;
}
BOOL CBaseMonster::GetEnemy()
{
	return FALSE;
}
CBaseEntity* CBaseMonster::DropItem( char* name, const Vector& position, const Vector& angles )
{
	return NULL;
}
BOOL CBaseMonster::ShouldFadeOnDeath()
{
	return FALSE;
}
void CBaseMonster::RadiusDamage( entvars_t* inflictor, entvars_t* attacker, float damage,
	int classIgnore, int damageType )
{
}
void CBaseMonster::RadiusDamage( Vector source, entvars_t* inflictor, entvars_t* attacker, float damage,
	int classIgnore, int damageType )
{
}
void CBaseMonster::FadeMonster()
{
}
void CBaseMonster::GibMonster()
{
}
BOOL CBaseMonster::HasHumanGibs()
{
	return FALSE;
}
BOOL CBaseMonster::HasAlienGibs()
{
	return FALSE;
}
Activity CBaseMonster::GetDeathActivity()
{
	return ACT_DIE_HEADSHOT;
}
MONSTERSTATE CBaseMonster::GetIdealState()
{
	return MONSTERSTATE_ALERT;
}
Schedule_t* CBaseMonster::GetScheduleOfType( int type )
{
	return NULL;
}
Schedule_t* CBaseMonster::GetSchedule()
{
	return NULL;
}
void CBaseMonster::RunTask( Task_t* task )
{
}
void CBaseMonster::StartTask( Task_t* task )
{
}
Schedule_t* CBaseMonster::ScheduleFromName( const char* name )
{
	return NULL;
}
void CBaseMonster::BecomeDead()
{
}
void CBaseMonster::RunAI()
{
}
void CBaseMonster::Killed( entvars_t* inflictor, entvars_t* attacker, int gib )
{
}
int CBaseMonster::TakeHealth( float health, int damageType )
{
	return 0;
}
int CBaseMonster::TakeDamage( entvars_t* inflictor, entvars_t* attacker, float damage, int damageType )
{
	return 0;
}
int CBaseMonster::Save( CSave& save )
{
	return 1;
}
int CBaseMonster::Restore( CRestore& restore )
{
	return 1;
}
void CBaseMonster::PlaySentence( const char* sentence, float duration, float volume, float attenuation )
{
}
void CBaseMonster::PlayScriptedSentence( const char* sentence, float duration, float volume,
	float attenuation, BOOL concurrent, CBaseEntity* listener )
{
}
void CBaseMonster::SentenceStop()
{
}

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CBasePlayer::CBasePlayer()
{
}
CBasePlayer::~CBasePlayer()
{
}
void CBasePlayer::PainSound()
{
}
void CBasePlayer::DeathSound()
{
}
int CBasePlayer::TakeHealth( float health, int damageType )
{
	return 0;
}
void CBasePlayer::TraceAttack( entvars_t* attacker, float damage, Vector direction, TraceResult* trace, int damageType )
{
}
int CBasePlayer::TakeDamage( entvars_t* inflictor, entvars_t* attacker, float damage, int damageType )
{
	return 0;
}
void CBasePlayer::PackDeadPlayerItems()
{
}
void CBasePlayer::RemoveAllItems( BOOL removeSuit )
{
}
void CBasePlayer::SetAnimation( PLAYER_ANIM animation )
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
void CBasePlayer::StartObserver( Vector position, Vector angles )
{
}
void CBasePlayer::PlayerUse()
{
}
void CBasePlayer::Jump()
{
}
void CBasePlayer::Duck()
{
}
int CBasePlayer::Classify()
{
	return 0;
}
void CBasePlayer::PreThink()
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
void CBasePlayer::SetSuitUpdate( char* name, int group, int repeatTime )
{
}
void CBasePlayer::UpdatePlayerSound()
{
}
void CBasePlayer::PostThink()
{
}
void CBasePlayer::Precache()
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
BOOL CBasePlayer::FlashlightIsOn()
{
	return FALSE;
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
void CBasePlayer::ImpulseCommands()
{
}
void CBasePlayer::CheatImpulseCommands( int impulse )
{
}
int CBasePlayer::AddPlayerItem( CBasePlayerItem* item )
{
	return FALSE;
}
int CBasePlayer::RemovePlayerItem( CBasePlayerItem* item )
{
	return FALSE;
}
void CBasePlayer::ItemPreFrame()
{
}
void CBasePlayer::ItemPostFrame()
{
}
int CBasePlayer::AmmoInventory( int index )
{
	if ( (unsigned int)index >= (unsigned int)MAX_AMMO_SLOTS )
		return -1;
	return m_rgAmmo[index];
}
int CBasePlayer::GetAmmoIndex( const char* name )
{
	return -1;
}
void CBasePlayer::SendAmmoUpdate()
{
}
void CBasePlayer::UpdateClientData()
{
}
void CBasePlayer::UpdateLowPriorityClientData()
{
}
BOOL CBasePlayer::FBecomeProne()
{
	return TRUE;
}
void CBasePlayer::BarnacleVictimBitten( entvars_t* barnacle )
{
}
void CBasePlayer::BarnacleVictimReleased()
{
}
int CBasePlayer::Illumination()
{
	return 0;
}
void CBasePlayer::EnableControl( BOOL control )
{
}
Vector CBasePlayer::GetAutoaimVector( float delta )
{
	return g_vecZero;
}
Vector CBasePlayer::AutoaimDeflection( Vector& source, float range, float delta )
{
	return g_vecZero;
}
void CBasePlayer::ResetAutoaim()
{
}
void CBasePlayer::SetCustomDecalFrames( int frames )
{
}
int CBasePlayer::GetCustomDecalFrames()
{
	return -1;
}
void CBasePlayer::DropPlayerItem( char* name )
{
}
BOOL CBasePlayer::HasPlayerItem( CBasePlayerItem* item )
{
	return FALSE;
}
BOOL CBasePlayer::HasNamedPlayerItem( char* name )
{
	return FALSE;
}
BOOL CBasePlayer::SwitchWeapon( CBasePlayerItem* itemWeapon )
{
	return FALSE;
}
BOOL CBasePlayer::SwitchWeapon( const char* name )
{
	return FALSE;
}
int CBasePlayer::GiveAmmo( int amount, char* name, int maximum, int* index )
{
	return -1;
}
void CBasePlayer::GiveNamedItem( char* name )
{
}
void CBasePlayer::PlayStepSound( int step, float volume )
{
}
void CBasePlayer::UpdateStepSound()
{
}
void CBasePlayer::CheckPowerups()
{
}
BOOL CBasePlayer::GiveTFAmmo( int shells, int nails, int rockets, int cells )
{
	return TRUE;
}
BOOL CBasePlayer::EngineerUse( CBasePlayer* player )
{
	return TRUE;
}
void CBasePlayer::TeamFortress_CalcEMPDmgRad( float& damage, float& radius )
{
}
void CBasePlayer::TeamFortress_TakeConcussionBlast( entvars_t* grenade, float bounce )
{
}
void CBasePlayer::TeamFortress_TakeEMPBlast( entvars_t* grenade )
{
}
void CBasePlayer::TeamFortress_Concuss( entvars_t* grenade )
{
}
void CBasePlayer::Ignite( entvars_t* inflictor, entvars_t* attacker )
{
}
BOOL CBasePlayer::CantChange( int playerClass, BOOL showMessage )
{
	return FALSE;
}
void CBasePlayer::ChangeClass( int playerClass )
{
}
void CBasePlayer::TeamFortress_PrintClassName( int playerClass )
{
}
void CBasePlayer::TeamFortress_SetEquipment()
{
}
void CBasePlayer::TeamFortress_SetHealth()
{
}
void CBasePlayer::TeamFortress_SetSkin()
{
}
BOOL CBasePlayer::IsLegalClass( int playerClass )
{
	return TRUE;
}
void CBasePlayer::TeamFortress_DisplayLegalClasses()
{
}
void CBasePlayer::TeamFortress_ExecClassScript()
{
}
void CBasePlayer::TeamFortress_ExecMapScript()
{
}
void CBasePlayer::TeamFortress_TeamShowMemberClasses()
{
}
BOOL CBasePlayer::TeamFortress_TeamSet( int team )
{
	return TRUE;
}
BOOL CBasePlayer::TeamFortress_TeamPutPlayerInTeam()
{
	return TRUE;
}
void CBasePlayer::Engineer_RemoveBuildings()
{
}
void CBasePlayer::TF_AddFrags( int frags )
{
}
void CBasePlayer::TeamFortress_RemoveTimers()
{
}
void CBasePlayer::TeamFortress_SetupRespawn( BOOL suicide )
{
}
void CBasePlayer::TeamFortress_CheckClassStats()
{
}
void CBasePlayer::ForceRespawn()
{
}
void CBasePlayer::DropGoalItems()
{
}
void CBasePlayer::UseSpecialSkill()
{
}
void CBasePlayer::TeamFortress_ThrowPrimedGrenade()
{
}
void CBasePlayer::TeamFortress_PrimeGrenade( int type )
{
}
void CBasePlayer::CheckUncoveringSpies( CBasePlayer* player )
{
}
void CBasePlayer::ClientHearVox( const char* sentence )
{
}
void CBasePlayer::PlayerTouch( CBaseEntity* other )
{
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
void CBasePlayerItem::CheckRespawn()
{
}
CBaseEntity* CBasePlayerItem::Respawn()
{
	return NULL;
}
void CBasePlayerItem::DefaultTouch( CBaseEntity* other )
{
}
void CBasePlayerItem::DestroyItem()
{
}
int CBasePlayerItem::AddToPlayer( CBasePlayer* player )
{
	return TRUE;
}
void CBasePlayerItem::Drop()
{
}
void CBasePlayerItem::Kill()
{
}
void CBasePlayerItem::Holster( int skiplocal )
{
}
void CBasePlayerItem::AttachToPlayer( CBasePlayer* player )
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
float CBasePlayerWeapon::GetNextAttackDelay( float delay )
{
	return delay;
}
int CBasePlayerWeapon::AddDuplicate( CBasePlayerItem* original )
{
	return 0;
}
int CBasePlayerWeapon::AddToPlayer( CBasePlayer* player )
{
	return FALSE;
}
int CBasePlayerWeapon::UpdateClientData( CBasePlayer* player )
{
	return 0;
}
BOOL CBasePlayerWeapon::AddPrimaryAmmo( int amount, char* name, int clip, int maximum )
{
	return TRUE;
}
BOOL CBasePlayerWeapon::AddSecondaryAmmo( int amount, char* name, int maximum )
{
	return TRUE;
}
BOOL CBasePlayerWeapon::IsUseable()
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
int CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon* itemWeapon )
{
	return 0;
}
int CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon* itemWeapon )
{
	return 0;
}
void CBasePlayerWeapon::RetireWeapon()
{
}
void CBasePlayerWeapon::DB_LogShots( int shots )
{
}
int CBasePlayerWeapon::DefaultReload( int clip, int animation, float delay )
{
	return TRUE;
}

void CBasePlayerAmmo::Spawn()
{
}
CBaseEntity* CBasePlayerAmmo::Respawn()
{
	return this;
}
void CBasePlayerAmmo::Materialize()
{
}
void CBasePlayerAmmo::DefaultTouch( CBaseEntity* other )
{
}

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CGrenade::CGrenade()
{
}
void CGrenade::BounceSound()
{
}
void CGrenade::Explode( Vector source, Vector aim )
{
}
void CGrenade::Explode( TraceResult* trace, int damageType )
{
}
void CGrenade::Killed( entvars_t* inflictor, entvars_t* attacker, int gib )
{
}
void CGrenade::Spawn()
{
}
void CGrenade::setModel()
{
}
BOOL CGrenade::setBirthdayModel()
{
	return FALSE;
}

void CCrowbar::Spawn()
{
}
void CCrowbar::Precache()
{
}
int CCrowbar::GetItemInfo( ItemInfo* info )
{
	return 1;
}
BOOL CCrowbar::Deploy()
{
	return TRUE;
}
void CCrowbar::Holster( int skiplocal )
{
}
void CCrowbar::PrimaryAttack()
{
}
void CCrowbar::Smack()
{
}
void CCrowbar::SwingAgain()
{
}
int CCrowbar::Swing( int first )
{
	return 0;
}

void CLaserSpot::Spawn()
{
}
void CLaserSpot::Precache()
{
}
void CLaserSpot::Suspend( float duration )
{
}
void CLaserSpot::Revive()
{
}
CTFNailgunNail* CTFNailgunNail::CreateNail( int model, Vector origin, Vector angles,
	CBaseEntity* owner, CBaseEntity* ignore, int damageType )
{
	return NULL;
}
CTFNailgunNail* CTFNailgunNail::CreateSuperNail( Vector origin, Vector angles,
	CBaseEntity* owner, CBaseEntity* ignore )
{
	return NULL;
}
CTFNailgunNail* CTFNailgunNail::CreateTranqNail( Vector origin, Vector angles,
	CBaseEntity* owner, CBaseEntity* ignore )
{
	return NULL;
}
CTFNailgunNail* CTFNailgunNail::CreateRailgunNail( Vector origin, Vector angles,
	CBaseEntity* owner, CBaseEntity* ignore )
{
	return NULL;
}
CTFRpgRocket* CTFRpgRocket::CreateRpgRocket( Vector origin, Vector angles,
	CBaseEntity* owner, CTFRpg* launcher )
{
	return NULL;
}
void CTFRpgRocket::Spawn()
{
}
void CTFRpgRocket::Precache()
{
}
void CTFRpgRocket::RocketTouch( CBaseEntity* other )
{
}
void CTFRpgRocket::RadiusDamage( entvars_t* inflictor, entvars_t* attacker, float damage,
	int classIgnore, int damageType )
{
}
CTFIncendiaryCRocket* CTFIncendiaryCRocket::CreateRpgRocket( Vector origin, Vector angles,
	CBaseEntity* owner, CTFIncendiaryC* launcher )
{
	return NULL;
}
void CTFIncendiaryCRocket::Spawn()
{
}
void CTFIncendiaryCRocket::Precache()
{
}
void CTFIncendiaryCRocket::RadiusDamage( entvars_t* inflictor, entvars_t* attacker, float damage,
	int classIgnore, int damageType )
{
}
CTFFlamethrowerBurst* CTFFlamethrowerBurst::CreateBurst( Vector origin, Vector angles,
	CBaseEntity* owner, CTFFlamethrower* launcher )
{
	return NULL;
}
CTFGrenade* CTFGrenade::CreateTFGrenade( Vector origin, Vector angles,
	CBaseEntity* owner, CTFGrenadeLauncher* launcher )
{
	return NULL;
}
CTFGrenade* CTFGrenade::CreateTFPipebomb( Vector origin, Vector angles,
	CBaseEntity* owner, CTFGrenadeLauncher* launcher )
{
	return NULL;
}

edict_t* DBG_EntOfVars( const entvars_t* vars )
{
	return NULL;
}
void DBG_AssertFunction( BOOL expression, const char* expressionText, const char* file, int line, const char* message )
{
}
void UTIL_PrecacheOther( const char* classname )
{
}
void UTIL_BloodDrips( const Vector& origin, const Vector& direction, int color, int amount )
{
}
void UTIL_DecalTrace( TraceResult* trace, int decal )
{
}
void UTIL_GunshotDecalTrace( TraceResult* trace, int decal )
{
}
void UTIL_MakeVectors( const Vector& angles )
{
}
BOOL UTIL_IsValidEntity( edict_t* entity )
{
	return TRUE;
}
void UTIL_SetOrigin( entvars_t* vars, const Vector& origin )
{
}
BOOL UTIL_GetNextBestWeapon( CBasePlayer* player, CBasePlayerItem* current )
{
	return TRUE;
}
void UTIL_LogPrintf( char* format, ... )
{
}
void UTIL_ClientPrintAll( int destination, const char* message, const char* first,
	const char* second, const char* third, const char* fourth )
{
}
void ClientPrint( entvars_t* client, int destination, const char* message, const char* first,
	const char* second, const char* third, const char* fourth )
{
}
int ShouldSimplify( int type )
{
	return TRUE;
}
int TrainSpeed( int speed, int maximum )
{
	return 0;
}
CBaseEntity* FindEntityForward( CBaseEntity* entity )
{
	return NULL;
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
void SpawnBlood( Vector origin, int color, float damage )
{
}
int DamageDecal( CBaseEntity* entity, int damageType )
{
	return 0;
}
void DecalGunshot( TraceResult* trace, int bulletType )
{
}
void EjectBrass( const Vector& origin, const Vector& velocity, float rotation, int model, int soundType )
{
}
void AddAmmoNameToAmmoRegistry( const char* name )
{
}
