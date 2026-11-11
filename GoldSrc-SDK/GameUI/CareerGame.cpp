#include "CareerGame.h"
#include "CareerBaseFrame.h"
#include "CareerDefinitions.h"
#include "CvarLatch.h"
#include "GameUI_Interface.h"
#include "MusicManager.h"
#include "Taskbar.h"

#include "../game_shared/shared_util.h"

#include "wrect.h"
#include "cl_dll.h"
#include "parsemsg.h"
#include "Random.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "../cstrike/game_shared/bot/bot_profile.h"
#include "FileSystem.h"
#include "vgui_controls/Controls.h"
#include "vgui/IScheme.h"

#ifndef _WIN32
#define stricmp strcasecmp
#endif

CCareerGame* TheCareerGame;
bool CCareerGame::m_checksumsOk = true;
EXPOSE_SINGLE_INTERFACE( CCareerGame, ICareerUI, "CareerUI001" );

int __MsgFunc_CZCareer( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	const char* message = READ_STRING();
	if ( !strcmp( message, "START" ) )
	{
		TheCareerGame->StartMatch();
		TheCareerGame->SetRoundTimeLength( READ_SHORT() );
		return 1;
	}
	if ( !strcmp( message, "MATCH" ) )
	{
		int ctWins = READ_LONG();
		int tWins = READ_LONG();
		TheCareerGame->StopMatch( ctWins, tWins );
		return 1;
	}
	if ( !strcmp( message, "GOGOGO" ) )
	{
		ITaskVec* tasks = TheCareerGame->GetTaskVecForMap( TheCareerGame->GetLastMap() );
		if ( tasks )
		{
			for ( ITaskVec::iterator it = tasks->begin(); it != tasks->end(); ++it )
				( *it )->StartRound();
		}
		return 1;
	}

	bool completed = !strcmp( message, "TASKDONE" );
	bool partial = !strcmp( message, "TASKPART" );
	bool undone = !strcmp( message, "TASKUNDONE" );
	if ( !completed && !partial && !undone )
		return 1;

	int index = READ_BYTE();
	int count = partial ? READ_SHORT() : 0;
	ITaskVec* tasks = TheCareerGame->GetTaskVecForMap( TheCareerGame->GetLastMap() );
	if ( !tasks )
		return 1;
	ITaskVec::iterator it = tasks->begin();
	while ( it != tasks->end() && --index > 0 )
		++it;
	if ( it == tasks->end() )
		return 1;

	if ( completed )
		( *it )->Set( true );
	else if ( partial )
		( *it )->SetPartial( count );
	else
		( *it )->Reset();
	return 1;
}

void Career_Printf( const char*, ... )
{
}

CCareerCharacter::CCareerCharacter( const BotProfile* profile )
	: m_pProfile( profile )
{
}

void CCareerCharacter::Print()
{
}

char* CCareerCharacter::Save( char* buffer, int& length, int hired )
{
	if ( hired )
		return BufPrintf( buffer, length, "hire \"%s\"\n", m_pProfile->GetName() );
	return buffer;
}

int CCareerCharacter::GetPoints() const
{
	return m_pProfile ? m_pProfile->GetCost() : 0;
}

const char* CCareerCharacter::GetName() const
{
	return m_pProfile ? m_pProfile->GetName() : "";
}

bool CCareerCharacter::operator<( const CCareerCharacter& other ) const
{
	if ( !m_pProfile )
		return false;
	if ( GetPoints() != other.GetPoints() )
		return GetPoints() < other.GetPoints();
	if ( GetRange( GetSkill() ) != GetRange( other.GetSkill() ) )
		return GetSkill() < other.GetSkill();
	if ( GetRange( GetTeamwork() ) != GetRange( other.GetTeamwork() ) )
		return GetTeamwork() < other.GetTeamwork();
	if ( GetRange( GetAggression() ) != GetRange( other.GetAggression() ) )
		return GetAggression() < other.GetAggression();
	return strcmp( GetName(), other.GetName() ) < 0;
}

int CCareerCharacter::GetRange( float value ) const
{
	if ( value < 0.9f )
	{
		if ( value < 0.65f )
		{
			if ( value < 0.35f )
				return value >= 0.1f;
			return 2;
		}
		return 3;
	}
	return 4;
}

const char* CCareerCharacter::GetSkinThumbnailFname() const
{
#ifdef _WIN32
	static char imageFilename[_MAX_PATH];
#else
	static char imageFilename[PATH_MAX];
#endif
	if ( TheBotProfiles->GetCustomSkinModelname( m_pProfile->GetSkin() ) )
	{
		const char* model = TheBotProfiles->GetCustomSkinModelname( m_pProfile->GetSkin() );
#ifdef _WIN32
		_snprintf( imageFilename, sizeof( imageFilename ), "gfx/thumbnails/skins/%s.tga", model );
#else
		snprintf( imageFilename, sizeof( imageFilename ), "gfx/thumbnails/skins/%s.tga", model );
#endif
		if ( vgui2::filesystem()->FileExists( imageFilename ) )
		{
			imageFilename[strlen( imageFilename ) - 4] = 0;
			return imageFilename;
		}
	}

	if ( TheCareerGame->PlayAsCT() )
	{
		switch ( m_pProfile->GetSkin() )
		{
		case 1: return "gfx/thumbnails/skins/urban";
		case 2: return "gfx/thumbnails/skins/gsg9";
		case 3: return "gfx/thumbnails/skins/sas";
		case 4: return "gfx/thumbnails/skins/gign";
		case 5: return "gfx/thumbnails/skins/spetsnaz";
		default: return "gfx/thumbnails/skins/ct_random";
		}
	}
	switch ( m_pProfile->GetSkin() )
	{
	case 1: return "gfx/thumbnails/skins/terror";
	case 2: return "gfx/thumbnails/skins/leet";
	case 3: return "gfx/thumbnails/skins/arctic";
	case 4: return "gfx/thumbnails/skins/guerilla";
	case 5: return "gfx/thumbnails/skins/militia";
	default: return "gfx/thumbnails/skins/t_random";
	}
}

const char* CCareerCharacter::GetWeaponPreferenceAsString( int index ) const
{
	return m_pProfile ? m_pProfile->GetWeaponPreferenceAsString( index ) : NULL;
}

int CCareerCharacter::GetWeaponPreferenceCount() const
{
	return m_pProfile ? m_pProfile->GetWeaponPreferenceCount() : 0;
}

float CCareerCharacter::GetSkill() const
{
	return m_pProfile ? m_pProfile->GetSkill() : 0;
}

float CCareerCharacter::GetTeamwork() const
{
	return m_pProfile ? m_pProfile->GetTeamwork() : 0;
}

float CCareerCharacter::GetAggression() const
{
	return m_pProfile ? m_pProfile->GetAggression() : 0;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
CCareerMap::CCareerMap( const char* name, const CharacterVec& bots, bool friendlyFire )
{
	strncpy( m_name, name, sizeof( m_name ) );
	m_name[sizeof( m_name ) - 1] = 0;
	m_bots = bots;
	m_defeated = false;
	m_friendlyFire = friendlyFire;
	m_extra = NULL;
	m_allowTeammates = true;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CCareerMap::~CCareerMap()
{
	for ( ITaskVec::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it )
		operator delete( *it );
	m_tasks.clear();
	delete[] m_extra;
	for ( CharacterVec::iterator it = m_bots.begin(); it != m_bots.end(); ++it )
		delete *it;
	m_bots.clear();
}

int CCareerMap::GetNumBotNames() const
{
	return (int)m_bots.size();
}

const char* CCareerMap::GetBotName( int index ) const
{
	if ( index < 0 || (unsigned int)index >= m_bots.size() )
		return NULL;
	return m_bots[index]->GetName();
}

void CCareerMap::SetExtraInitialization( const char* extra )
{
	delete[] m_extra;
	m_extra = NULL;
	if ( extra )
		m_extra = CloneString( extra );
}

bool CCareerMap::IsFriendlyFireOn() const
{
	return m_friendlyFire;
}

int CCareerMap::GetCurrentEnemyCount() const
{
	int points = TheCareerGame->GetPoints();
	int count = m_minEnemyCount;
	if ( points > m_threshold )
		count += points - m_threshold;
	if ( (unsigned int)count > m_bots.size() )
		return (int)m_bots.size();
	return count;
}

void CCareerMap::ResetTaskCompletion()
{
	for ( ITaskVec::iterator it = m_tasks.begin(); it != m_tasks.end(); ++it )
		( *it )->Reset();
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CCareerGame::CCareerGame()
{
	m_pLastMap = NULL;
	m_initDone = false;
	m_lastMapName[0] = 0;
	m_reputationPoints = 0;
	m_reputationPointsFlashed = 0;
	m_queuedLoadMenu = false;
	m_queuedName.clear();
	m_isCT = 1;
	m_currentProfile = new CCareerProfileData;
	m_writer = new BufferWriter;
	m_difficulty = CAREER_DIFFICULTY_EASY;
}

CCareerGame::~CCareerGame()
{
	delete m_currentProfile;
	delete m_writer;
	delete TheCareerDefinitions;
	TheCareerDefinitions = NULL;
}

void CCareerGame::Create()
{
	if ( !TheCareerGame )
	{
		vgui2::scheme()->LoadSchemeFromFile( "Resource/CareerScheme.res", "CareerScheme" );
		vgui2::scheme()->LoadSchemeFromFile( "Resource/CareerBoxScheme.res", "CareerBoxScheme" );
		engine->pfnHookUserMsg( "CZCareer", __MsgFunc_CZCareer );
	}
	TheCareerGame = &__g_CCareerGame_singleton;
	CareerDefinitionManager::Create();
	TheBotProfiles = NULL;
	CareerDefinition* career = TheCareerDefinitions->GetActiveCareerDefinition();
	if ( career )
		TheBotProfiles = career->GetBotProfileManager( TheCareerGame->GetDifficulty() );
}

int CCareerGame::GetPointsRemaining()
{
	int spent = 0;
	for ( CharacterVec::iterator it = m_hiredCharacters.begin(); it != m_hiredCharacters.end(); ++it )
		spent += ( *it )->GetPoints();
	return m_reputationPoints - spent;
}

int CCareerGame::GetHighestCostAvailable( int reputationPoints )
{
	if ( reputationPoints < 0 )
		reputationPoints = m_reputationPoints;
	for ( unsigned int i = 1; i < m_costAvailability.size(); ++i )
	{
		if ( m_costAvailability[i] && m_costAvailability[i] > reputationPoints )
			return i - 1;
	}
	return 1000;
}

void CCareerGame::PrintMaps()
{
}

void CCareerGame::DefeatMap( const char* name )
{
	CCareerMap* map = GetPlayableMap( name );
	if ( map )
		map->SetDefeated( true );
}

bool CCareerGame::SetLastMap( const char* name )
{
	m_lastMapName[0] = 0;
	m_pLastMap = GetPlayableMap( name );
	if ( !m_pLastMap )
		return false;
	strncpy( m_lastMapName, name, sizeof( m_lastMapName ) );
	m_lastMapName[sizeof( m_lastMapName ) - 1] = 0;
	return true;
}

CCareerMap* CCareerGame::GetPlayableMap( const char* name )
{
	for ( MapsVec::iterator it = m_maps.begin(); it != m_maps.end(); ++it )
	{
		bool passed = true;
		for ( int i = 0; i < CAREER_MAPS_PER_TRIPLET; ++i )
		{
			CCareerMap* map = ( *it )[i];
			if ( !stricmp( name, map->GetName() ) )
				return map;
			if ( !map->IsDefeated() )
				passed = false;
		}
		if ( !passed )
			break;
	}
	return NULL;
}

CCareerMap* CCareerGame::GetMap( const char* name )
{
	if ( !name )
		return NULL;
	for ( MapsVec::iterator it = m_maps.begin(); it != m_maps.end(); ++it )
	{
		for ( int i = 0; i < CAREER_MAPS_PER_TRIPLET; ++i )
		{
			if ( !stricmp( name, ( *it )[i]->GetName() ) )
				return ( *it )[i];
		}
	}
	return NULL;
}

int CCareerGame::GetMapTripletNum( const char* name )
{
	if ( !name )
		return -1;
	for ( unsigned int group = 0; group < m_maps.size(); ++group )
	{
		for ( int i = 0; i < CAREER_MAPS_PER_TRIPLET; ++i )
		{
			if ( !stricmp( name, m_maps[group][i]->GetName() ) )
				return group;
		}
	}
	return -1;
}

int CCareerGame::GetMapTripletOffset( const char* name )
{
	if ( !name )
		return -1;
	for ( MapsVec::iterator it = m_maps.begin(); it != m_maps.end(); ++it )
	{
		for ( int i = 0; i < CAREER_MAPS_PER_TRIPLET; ++i )
		{
			if ( !stricmp( name, ( *it )[i]->GetName() ) )
				return i;
		}
	}
	return -1;
}

bool CCareerGame::DoesCurrentMapUnlockTriplet()
{
	for ( unsigned int group = 0; group < m_maps.size(); ++group )
	{
		bool found = false;
		bool passed = true;
		for ( int i = 0; i < CAREER_MAPS_PER_TRIPLET; ++i )
		{
			CCareerMap* map = m_maps[group][i];
			if ( map == m_pLastMap )
				found = true;
			else if ( !map->IsDefeated() )
				passed = false;
		}
		if ( found )
			return passed && !m_wasMapDefeatedAtStart && group != m_maps.size() - 1;
	}
	return false;
}

bool CCareerGame::DoesWinUnlockAll()
{
	for ( unsigned int group = 0; group < m_maps.size(); ++group )
	{
		bool found = false;
		bool passed = true;
		for ( int i = 0; i < CAREER_MAPS_PER_TRIPLET; ++i )
		{
			CCareerMap* map = m_maps[group][i];
			if ( map == m_pLastMap )
				found = true;
			else if ( !map->IsDefeated() )
				passed = false;
		}
		if ( found )
			return passed && !m_wasMapDefeatedAtStart && group == m_maps.size() - 1;
	}
	return false;
}

CCareerMap* CCareerGame::GetFirstUndefeatedMap()
{
	for ( MapsVec::iterator it = m_maps.begin(); it != m_maps.end(); ++it )
	{
		for ( int i = 0; i < CAREER_MAPS_PER_TRIPLET; ++i )
		{
			if ( !( *it )[i]->IsDefeated() )
				return ( *it )[i];
		}
	}
	return NULL;
}

ITaskVec* CCareerGame::GetTaskVecForMap( const char* name )
{
	CCareerMap* map = GetPlayableMap( name );
	return map ? map->GetTasks() : NULL;
}

int CCareerGame::GetCurrentMapTriplet( MapInfo* maps )
{
	int group = GetMapTripletNum( m_lastMapName );
	if ( group < 0 )
		return -1;
	std::vector<CCareerMap*> triplet = m_maps[group];
	for ( int i = 0; i < CAREER_MAPS_PER_TRIPLET; ++i )
	{
		maps[i].name = triplet[i]->GetName();
		maps[i].defeated = triplet[i]->IsDefeated();
	}
	return GetMapTripletOffset( m_lastMapName );
}

bool CCareerGame::IsPlayingMatch()
{
	return engine->pfnIsCareerMatch() != 0;
}

ITaskVec* CCareerGame::GetCurrentTaskVec()
{
	return m_pLastMap ? m_pLastMap->GetTasks() : NULL;
}

void CCareerGame::StartMatch()
{
	m_isPlaying = true;
	m_needAutosave = true;
}

int CCareerGame::GetNumTasksOnCurrentMap()
{
	return (int)m_pLastMap->GetTasks()->size();
}

ICareerTask* CCareerGame::GetCurrentTask( int index )
{
	ITaskVec* tasks = m_pLastMap->GetTasks();
	int i = 0;
	for ( ITaskVec::iterator it = tasks->begin(); it != tasks->end(); ++it, ++i )
	{
		if ( i == index )
			return *it;
	}
	return NULL;
}

void CCareerGame::StartLoading()
{
}

void CCareerGame::AddSavedCvar( const char* cvarName, const char* cvarVal, bool wait )
{
	m_savedCvars.push_back( new CvarLatch( cvarName, cvarVal ) );
	char command[128];
#ifdef _WIN32
	_snprintf( command, sizeof( command ), "%s%s \"%s\"\n", wait ? "wait\n" : "", cvarName, cvarVal );
#else
	snprintf( command, sizeof( command ), "%s%s \"%s\"\n", wait ? "wait\n" : "", cvarName, cvarVal );
#endif
	engine->pfnClientCmd( command );
}

void CCareerGame::FinishMatchStart()
{
	CCareerMap* map = m_pLastMap;
	unsigned int count = map->GetCurrentEnemyCount();
	const CharacterVec& enemies = map->GetBotList();
	char command[1024];
	for ( CharacterVec::const_iterator it = enemies.begin(); it != enemies.end(); ++it )
	{
		sprintf( command, PlayAsCT() ? "wait\nwait\nbot_add_t \"%s\"\n" : "wait\nwait\nbot_add_ct \"%s\"\n",
				( *it )->GetName() );
		engine->pfnClientCmd( command );
		if ( !--count )
			break;
	}
	if ( m_pLastMap->AreTeammatesAllowed() )
	{
		for ( CharacterVec::iterator it = m_hiredCharacters.begin(); it != m_hiredCharacters.end(); ++it )
		{
			sprintf( command, PlayAsCT() ? "wait\nwait\nbot_add_ct \"%s\"\n" : "wait\nwait\nbot_add_t \"%s\"\n",
					( *it )->GetName() );
			engine->pfnClientCmd( command );
		}
	}
	const char* extra = m_pLastMap->GetExtraInitialization();
	if ( extra )
	{
#ifdef _WIN32
		_snprintf( command, sizeof( command ), "wait\nwait\n%s\n", extra );
#else
		snprintf( command, sizeof( command ), "wait\nwait\n%s\n", extra );
#endif
		engine->pfnClientCmd( command );
	}
}

void CCareerGame::SetBotPoolSliderPosition( int position )
{
	m_botPoolSliderPosition = position;
}

int CCareerGame::GetBotPoolSliderPosition()
{
	return m_botPoolSliderPosition;
}

void CCareerGame::SendMatchCommand()
{
	if ( !m_pLastMap || !TheCareerDefinitions->GetActiveCareerDefinition() )
		return;
	char password[9];
	for ( int i = 0; i < (int)sizeof( password ) - 1; ++i )
		password[i] = (char)( '0' + RandomLong( 0, 9 ) );
	password[sizeof( password ) - 1] = 0;
	Career_Printf( "Career Password is [%s]\n", password );

	unsigned int maxPlayers = m_pLastMap->GetCurrentEnemyCount() + 1;
	if ( m_pLastMap->AreTeammatesAllowed() )
		maxPlayers += (unsigned int)m_hiredCharacters.size();
	m_savedCvars.push_back( new CvarLatch( "sv_lan", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "maxplayers", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "sv_password", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "hostname", NULL ) );
	char command[1024];
	sprintf( command, "disconnect\nsv_lan 1\nsetmaster enable\nmaxplayers %i\nsv_password \"%s\"\n"
			"hostname \"%s\"\ncd fadeout\ncareer %s\n",
			(int)maxPlayers, password, "Counter-Strike: Condition Zero", m_lastMapName );
	engine->pfnClientCmd( command );
	engine->pfnClientCmd( "wait\nwait\n" );

	m_savedCvars.push_back( new CvarLatch( "mp_startmoney", "800" ) );
	m_savedCvars.push_back( new CvarLatch( "mp_limitteams", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "humans_join_team", "any" ) );
	m_savedCvars.push_back( new CvarLatch( "mp_autoteambalance", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "bot_auto_vacate", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "bot_quota", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "bot_join_after_player", "1" ) );
	sprintf( command, "mp_startmoney 10000\nmp_limitteams 0\nhumans_join_team \"%s\"\nwait\n"
			"mp_autoteambalance 0\nwait\nbot_auto_vacate 0\nbot_quota 0\nwait\n\nwait\nenable_career_mode\n",
			PlayAsCT() ? "CT" : "T" );
	engine->pfnClientCmd( command );
	m_savedCvars.push_back( new CvarLatch( "bot_defer_to_human", NULL ) );
	sprintf( command, "wait\nbot_defer_to_human 1\nwait\ncareer_matchlimit %d %d\n",
			m_matchNumWins, m_matchWinBy );
	engine->pfnClientCmd( command );
	sprintf( command, "wait\nmp_roundtime %d\nmp_c4timer %d\nmp_freezetime %d\n",
			m_roundTime, m_bombCountdownTime, m_freezeTime );
	engine->pfnClientCmd( command );
	AddSavedCvar( "bot_traceview", "0", true );
	AddSavedCvar( "bot_show_nav", "0", true );
	AddSavedCvar( "bot_show_danger", "0", true );
	AddSavedCvar( "bot_nav_edit", "0", true );
	AddSavedCvar( "bot_walk", "0", true );
	AddSavedCvar( "bot_debug", "0", true );
	AddSavedCvar( "bot_quota_match", "0", true );
	AddSavedCvar( "bot_allow_rogues", "1", true );
	AddSavedCvar( "bot_allow_pistols", "1", true );
	AddSavedCvar( "bot_allow_shotguns", "1", true );
	AddSavedCvar( "bot_allow_sub_machine_guns", "1", true );
	AddSavedCvar( "bot_allow_rifles", "1", true );
	AddSavedCvar( "bot_allow_machine_guns", "1", true );
	AddSavedCvar( "bot_allow_grenades", "1", true );
	AddSavedCvar( "bot_allow_snipers", "1", true );
	AddSavedCvar( "bot_allow_shield", "1", true );
	m_savedCvars.push_back( new CvarLatch( "mp_roundtime", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "mp_c4timer", NULL ) );
	m_savedCvars.push_back( new CvarLatch( "mp_freezetime", NULL ) );
	sprintf( command, "wait\nmp_roundtime %d\nmp_c4timer %d\nmp_freezetime %d\n",
			m_roundTime, m_bombCountdownTime, m_freezeTime );
	engine->pfnClientCmd( command );

	m_winfastLength = 0;
	ITaskVec* tasks = m_pLastMap->GetTasks();
	for ( ITaskVec::iterator it = tasks->begin(); it != m_pLastMap->GetTasks()->end(); ++it )
	{
		ICareerTask* task = *it;
		bool complete = task->IsComplete();
		bool inARow = task->InARow();
		bool survive = task->MustSurvive();
		int repeat = task->GetRepeat();
		const char* weapon = task->GetWeaponName();
		const char* name = task->GetTaskName();
		sprintf( command, "wait\ncareer_add_task %s \"%s\" %d %d %d %d\n",
				name, weapon, repeat, (int)survive, (int)inARow, (int)complete );
		if ( !stricmp( task->GetTaskName(), "winfast" ) )
			m_winfastLength = task->GetRepeat();
		engine->pfnClientCmd( command );
	}
	m_savedCvars.push_back( new CvarLatch( "mp_friendlyfire", "0" ) );
	engine->pfnClientCmd( m_pLastMap->IsFriendlyFireOn() ? "wait\nmp_friendlyfire 1\n" : "wait\nmp_friendlyfire 0\n" );
	m_savedCvars.push_back( new CvarLatch( "bot_zombie", "0" ) );
	engine->pfnClientCmd( "wait\nbot_zombie 0\n" );
	m_savedCvars.push_back( new CvarLatch( "bot_stop", "0" ) );
	engine->pfnClientCmd( "wait\nbot_stop 0\n" );
	m_savedCvars.push_back( new CvarLatch( "hostage_stop", "0" ) );
	engine->pfnClientCmd( "wait\nhostage_stop 0\n" );
	if ( m_pLastMap->IsDefeated() )
	{
		m_matchReputationGain = 0;
		m_matchMapsUnlocked = 0;
	}
	else
	{
		m_matchReputationGain = 1;
		m_matchMapsUnlocked = DoesCurrentMapUnlockTriplet();
	}
	m_needAutosave = true;
	m_wasMapDefeatedAtStart = m_pLastMap->IsDefeated();
	float tutorEnable = engine->pfnGetCvarFloat( "tutor_enable" );
	ExecuteProfileConfig();
#ifdef _WIN32
	_snprintf( command, sizeof( command ), "wait\ntutor_enable %d\n", tutorEnable > 0.0f );
#else
	snprintf( command, sizeof( command ), "wait\ntutor_enable %d\n", tutorEnable > 0.0f );
#endif
	engine->pfnClientCmd( command );
	SaveProfile( false );
	vgui2::filesystem()->CreateDirHierarchy( "MissionPacks", NULL );
	FileHandle_t file = vgui2::filesystem()->Open( "MissionPacks/BotPackList.db", "w", NULL );
	if ( file )
	{
		CareerDefinition* career = TheCareerDefinitions->GetActiveCareerDefinition();
		vgui2::filesystem()->FPrintf( file, (char*)"\"%s\"\n", career->GetDifficulty( m_difficulty )->GetBotProfileDBFilename() );
		vgui2::filesystem()->Close( file );
	}
}

void CCareerGame::StopMatch( int ctWins, int tWins )
{
	if ( !m_isPlaying || !m_pLastMap )
		return;
	if ( ctWins || tWins )
	{
		m_ctWins = ctWins;
		m_tWins = tWins;
	}

	bool tasksCompleted = true;
	int numTasks = GetNumTasksOnCurrentMap();
	for ( int i = 0; i < numTasks; ++i )
	{
		ICareerTask* task = GetCurrentTask( i );
		if ( !task || !task->IsComplete() )
			tasksCompleted = false;
	}
	int ownWins = m_isCT ? m_ctWins : m_tWins;
	int enemyWins = m_isCT ? m_tWins : m_ctWins;
	if ( tasksCompleted && ownWins - enemyWins >= m_matchWinBy && ownWins >= m_matchNumWins )
	{
		CCareerMap* currentMap = m_pLastMap;
		if ( DoesWinUnlockAll() )
			m_medals[m_difficulty] = true;
		CCareerMap* nextMap = currentMap;
		if ( m_pLastMap && !m_pLastMap->IsDefeated() )
		{
			++m_reputationPoints;
			m_pLastMap->SetDefeated( true );
			nextMap = GetFirstUndefeatedMap();
			if ( !nextMap )
				nextMap = m_pLastMap;
		}
		SetLastMap( nextMap->GetName() );
		currentMap->ResetTaskCompletion();
		SaveProfile( true );
		ITaskVec* tasks = currentMap->GetTasks();
		for ( ITaskVec::iterator it = tasks->begin(); it != tasks->end(); ++it )
			( *it )->Set( false );
		SetLastMap( currentMap->GetName() );
	}
	else
	{
		SaveProfile( true );
	}

	if ( ctWins || tWins )
	{
		m_needAutosave = false;
	}
	else
	{
		m_isPlaying = false;
		engine->pfnResetTutorMessageDecayData();
		for ( unsigned int i = 0; i < m_savedCvars.size(); ++i )
			delete m_savedCvars[i];
		m_savedCvars.clear();
		engine->pfnClientCmd( "exec config.cfg\n" );
	}
}

bool CCareerGame::Hire( const char* name )
{
	for ( CharacterVec::iterator it = m_availableCharacters.begin(); it != m_availableCharacters.end(); ++it )
	{
		CCareerCharacter* character = *it;
		if ( stricmp( name, character->GetName() ) )
			continue;
		if ( GetPointsRemaining() < character->GetPoints() )
		{
			Career_Printf( "%s refuses to join your team.\n", character->GetName() );
			return false;
		}
		Career_Printf( "%s has joined your team.\n", character->GetName() );
		m_hiredCharacters.push_back( character );
		m_availableCharacters.erase( it );
		std::sort( m_hiredCharacters.begin(), m_hiredCharacters.end() );
		return true;
	}
	Career_Printf( "%s is not available for hiring!\n", name );
	return false;
}

bool CCareerGame::Fire( const char* name )
{
	for ( CharacterVec::iterator it = m_hiredCharacters.begin(); it != m_hiredCharacters.end(); ++it )
	{
		CCareerCharacter* character = *it;
		if ( stricmp( name, character->GetName() ) )
			continue;
		Career_Printf( "%s has left your team.\n", character->GetName() );
		m_availableCharacters.push_back( character );
		m_hiredCharacters.erase( it );
		std::sort( m_availableCharacters.begin(), m_availableCharacters.end() );
		return true;
	}
	Career_Printf( "%s is not on your team!\n", name );
	return false;
}

void CCareerGame::ClearRoster()
{
	for ( CharacterVec::iterator it = m_hiredCharacters.begin(); it != m_hiredCharacters.end(); ++it )
		m_availableCharacters.push_back( *it );
	std::sort( m_availableCharacters.begin(), m_availableCharacters.end() );
	m_hiredCharacters.clear();
}

void CCareerGame::Init()
{
	m_hiredCharacters.clear();
	m_availableCharacters.clear();
	m_maps.clear();
	m_initDone = true;
	strcpy( m_lastMapName, "de_dust" );
	m_costAvailability.clear();
	m_basePoints = 0;
	m_matchNumWins = 7;
	m_matchWinBy = 3;
	m_roundTime = 5;
	m_bombCountdownTime = 45;
	m_freezeTime = 5;
	m_isPlaying = false;
	CareerDefinition* career = TheCareerDefinitions->GetActiveCareerDefinition();
	if ( career )
	{
		CareerDifficultyDefinition* difficulty = career->GetDifficulty( m_difficulty );
		m_isCT = career->IsCT();
		m_basePoints = difficulty->GetBasePoints();
		m_matchNumWins = difficulty->GetMatchNumWins();
		m_matchWinBy = difficulty->GetMatchWinBy();
		m_roundTime = difficulty->GetRoundTimeMins();
		m_bombCountdownTime = difficulty->GetBombCountdownTimeSecs();
		m_freezeTime = difficulty->GetFreezeTimeSecs();
		m_availableCharacters = *difficulty->GetCharacters();
		m_costAvailability = *difficulty->GetCharacterCostAvailibility();
		m_maps = *difficulty->GetMaps();
	}
	Reset();
}

void CCareerGame::SetDifficulty( CareerDifficultyType difficulty )
{
	m_difficulty = difficulty;
	Init();
}

void CCareerGame::Reset()
{
	m_botPoolSliderPosition = 0;
	m_canUnpause = true;
	m_queuedLoadMenu = false;
	m_queuedName.clear();
	m_pLastMap = NULL;
	m_lastMapName[0] = 0;
	m_matchReputationGain = 0;
	m_matchMapsUnlocked = 0;
	m_needAutosave = false;
	m_winfastLength = 0;
	m_ctWins = 0;
	m_tWins = 0;
	memset( m_medals, 0, sizeof( m_medals ) );
	ClearRoster();
	if ( !m_initDone )
	{
		m_difficulty = CAREER_DIFFICULTY_EASY;
		Init();
	}
	m_reputationPoints = m_basePoints;
	m_wasMapDefeatedAtStart = false;
	m_reputationPointsFlashed = 0;
	bool selected = false;
	for ( MapsVec::iterator it = m_maps.begin(); it != m_maps.end(); ++it )
	{
		for ( int i = 0; i < CAREER_MAPS_PER_TRIPLET; ++i )
		{
			CCareerMap* map = ( *it )[i];
			map->SetDefeated( false );
			map->ResetTaskCompletion();
			if ( !selected )
				SetLastMap( map->GetName() );
			selected = true;
		}
	}
}

void CCareerGame::OnRoundEndMenuOpen( bool won )
{
	m_canUnpause = false;
	if ( TheMusicManager )
		TheMusicManager->StartTrack( won ? TRACKTYPE_ROUNDWIN : TRACKTYPE_ROUNDLOSE, false );
}

void CCareerGame::OnMatchEndMenuOpen( bool won )
{
	m_canUnpause = false;
	if ( TheMusicManager )
		TheMusicManager->StartTrack( won ? TRACKTYPE_MATCHWIN : TRACKTYPE_MATCHLOSE, false );
}

void CCareerGame::OnRoundEndMenuClose( bool stillPlaying )
{
	m_canUnpause = true;
	if ( TheMusicManager )
		TheMusicManager->FadeOutTrack();
	if ( !stillPlaying )
	{
		m_queuedLoadMenu = true;
		char nameCvar[] = "name";
		m_queuedName = m_currentProfile->name.empty() ? engine->pfnGetCvarString( nameCvar ) : m_currentProfile->name;
	}
}

void CCareerGame::OnMatchEndMenuClose( bool stillPlaying )
{
	m_canUnpause = true;
	if ( TheMusicManager )
		TheMusicManager->FadeOutTrack();
	if ( stillPlaying )
	{
		m_needAutosave = true;
	}
	else
	{
		m_queuedLoadMenu = true;
		char nameCvar[] = "name";
		m_queuedName = m_currentProfile->name.empty() ? engine->pfnGetCvarString( nameCvar ) : m_currentProfile->name;
	}
}

int CCareerGame::GetRoundTimeLength()
{
	return m_roundTimeLength;
}

void CCareerGame::Update()
{
	if ( !m_queuedLoadMenu )
		return;

	const char* level = engine->pfnGetLevelName();
	if ( level && level[0] )
		return;

	engine->Cvar_Set( "name", const_cast<char*>( m_queuedName.c_str() ) );
	CCareerBaseFrame* frame = (CCareerBaseFrame*)g_pTaskbar->OnOpenResumeCareerGameDialog( GetDifficulty() );
	if ( frame )
		frame->ShowAutosaveNotice();

	m_queuedLoadMenu = false;
	m_queuedName.clear();
}
