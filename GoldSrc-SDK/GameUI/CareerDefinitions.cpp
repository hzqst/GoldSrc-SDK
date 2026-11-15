#include "CareerDefinitions.h"
#include "CareerGameTasks.h"
#include "GameUI_Interface.h"

#include "filesystem.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "../game_shared/shared_util.h"
#include "../cstrike/game_shared/bot/bot_profile.h"

#ifdef _WIN32
#define CAREER_MAX_PATH _MAX_PATH
#else
#include <strings.h>
#define stricmp strcasecmp
#define CAREER_MAX_PATH PATH_MAX
#endif

CareerDefinitionManager* TheCareerDefinitions;

static CharacterVec ParseProfilesFromString( BotProfileManager* manager, const char* names )
{
	char* namesCopy = CloneString( names );
	CharacterVec characters;
	SharedSetQuoteChar( '\'' );
	const char* cursor = namesCopy;
	while ( SharedTokenWaiting( cursor ) )
	{
		cursor = SharedParse( cursor );
		const char* token = SharedGetToken();
		if ( !token )
			continue;
		const BotProfile* profile = manager->GetProfile( token, BOT_TEAM_ANY );
		if ( profile )
			characters.push_back( new CCareerCharacter( profile ) );
	}
	delete[] namesCopy;
	SharedSetQuoteChar( '"' );
	return characters;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 26495 )
#endif
CareerDifficultyDefinition::CareerDifficultyDefinition()
{
	m_botManager = NULL;
	m_vdfFilename = NULL;
	m_botFilename = NULL;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CareerDifficultyDefinition::~CareerDifficultyDefinition()
{
	Reset();
}

void CareerDifficultyDefinition::Reset()
{
	delete m_botManager;
	delete m_vdfFilename;
	delete m_botFilename;
	for ( CharacterVec::iterator it = m_availableCharacters.begin(); it != m_availableCharacters.end(); ++it )
		delete *it;
	m_availableCharacters.clear();
	m_botManager = NULL;
	m_vdfFilename = NULL;
	m_botFilename = NULL;
	for ( MapsVec::iterator it = m_maps.begin(); it != m_maps.end(); ++it )
	{
		std::vector<CCareerMap*> maps = *it;
		for ( std::vector<CCareerMap*>::iterator map = maps.begin(); map != maps.end(); ++map )
			delete *map;
	}
	m_maps.clear();
}

void CareerDifficultyDefinition::Init( const char* careerName, const char* filename, const char* botFilename )
{
	for ( CharacterVec::iterator it = m_availableCharacters.begin(); it != m_availableCharacters.end(); ++it )
		delete *it;
	m_availableCharacters.clear();
	char path[CAREER_MAX_PATH];
#ifdef _WIN32
	_snprintf( path, sizeof( path ), "MissionPacks/%s/%s", careerName, botFilename );
#else
	snprintf( path, sizeof( path ), "MissionPacks/%s/%s", careerName, botFilename );
#endif
	if ( !vgui2::filesystem()->FileExists( path ) )
	{
#ifdef _WIN32
		_snprintf( path, sizeof( path ), "BotCampaignProfile.db" );
#else
		snprintf( path, sizeof( path ), "BotCampaignProfile.db" );
#endif
	}
	m_botManager = new BotProfileManager;
	m_botManager->Init( path, NULL );
	KeyValues* data = new KeyValues( "CareerGame" );
	m_botFilename = CloneString( path );
#ifdef _WIN32
	_snprintf( path, sizeof( path ), "MissionPacks/%s/%s", careerName, filename );
#else
	snprintf( path, sizeof( path ), "MissionPacks/%s/%s", careerName, filename );
#endif
	if ( !data->LoadFromFile( vgui2::filesystem(), path, NULL ) )
	{
#ifdef _WIN32
		_snprintf( path, sizeof( path ), "CareerGame%s", filename );
#else
		snprintf( path, sizeof( path ), "CareerGame%s", filename );
#endif
		if ( !data->LoadFromFile( vgui2::filesystem(), path, NULL ) )
		{
			data->deleteThis();
			m_basePoints = 2;
			m_matchNumWins = 3;
			m_matchWinBy = 2;
			m_roundTimeMins = 5;
			m_bombCountdownTimeSecs = 45;
			m_freezeTimeSecs = 5;
			return;
		}
	}

	m_basePoints = 2;
	m_matchNumWins = 3;
	m_matchWinBy = 2;

	if ( data )
	{
		m_basePoints = data->GetInt( "InitialPoints", m_basePoints );
		m_matchNumWins = data->GetInt( "MatchWins", m_matchNumWins );
		m_matchWinBy = data->GetInt( "MatchWinBy", m_matchWinBy );
		m_roundTimeMins = data->GetInt( "RoundTimeInMinutes", 5 );
		m_bombCountdownTimeSecs = data->GetInt( "BombCountdownTimeTimeInSeconds", 45 );
		m_freezeTimeSecs = data->GetInt( "FreezeTimeTimeInSeconds", 5 );
		m_availableCharacters = ParseProfilesFromString( m_botManager, data->GetString( "Characters", "" ) );
		KeyValues* costs = data->FindKey( "CostAvailability", false );
		if ( costs )
		{
			char number[16];
			sprintf( number, "%d", 1 );
			int available = costs->GetInt( number, 0 );
			m_costAvailability.push_back( 0 );
			for ( int cost = 1; available; )
			{
				m_costAvailability.push_back( available );
				sprintf( number, "%d", ++cost );
				available = costs->GetInt( number, 0 );
			}
		}

		KeyValues* maps = data->FindKey( "Maps", false );
		if ( maps )
		{
			KeyValues* key = maps->GetFirstSubKey();
			while ( key )
			{
				std::vector<CCareerMap*> group;
				while ( key && group.size() < 3 )
				{
					const char* mapName = key->GetName();
					CharacterVec bots = ParseProfilesFromString( m_botManager, key->GetString( "bots", "" ) );
					if ( bots.empty() )
					{
						key = key->GetNextKey();
						break;
					}
					bool friendlyFire = key->GetInt( "FriendlyFire", 0 ) != 0;
					SharedSetQuoteChar( '\'' );
					CCareerMap* map = new CCareerMap( mapName, bots, friendlyFire );
					ITaskVec tasks;
					const char* cursor = key->GetString( "tasks", "" );
					if ( cursor )
					{
						while ( SharedTokenWaiting( cursor ) )
						{
							cursor = SharedParse( cursor );
							const char* token = SharedGetToken();
							if ( token )
							{
								char* taskName = CloneString( token );
								tasks.push_back( new CCareerUITask( taskName ) );
								delete[] taskName;
							}
							if ( !cursor )
								break;
						}
					}
					map->SetTasks( tasks );
					SharedSetQuoteChar( '"' );
					map->SetMinEnemyCount( atoi( key->GetString( "minEnemies", "3" ) ) );
					map->SetThreshold( atoi( key->GetString( "threshold", "9999" ) ) );
					map->SetTeammatesAllowed( key->GetInt( "teammates", 1 ) != 0 );
					const char* commands = key->GetString( "commands", NULL );
					if ( commands )
						map->SetExtraInitialization( commands );
					group.push_back( map );
					key = key->GetNextKey();
				}
				if ( group.size() == 3 )
				{
					m_maps.push_back( group );
				}
				else
				{
					for ( std::vector<CCareerMap*>::iterator it = group.begin(); it != group.end(); ++it )
						delete *it;
				}
			}
		}
		data->deleteThis();
	}
}

CareerDefinition::CareerDefinition()
{
	m_isCT = true;
	m_description = NULL;
	m_title = NULL;
	m_author = NULL;
	m_url = NULL;
	m_asciiTitle = NULL;
	m_botFilename = NULL;
	m_careerName = NULL;
	m_saveDir = NULL;
	m_versionString = NULL;
}

void CareerDefinition::Reset()
{
	m_isCT = true;
	delete[] m_author;
	delete[] m_title;
	delete[] m_description;
	delete[] m_url;
	delete[] m_careerName;
	delete[] m_saveDir;
	delete[] m_asciiTitle;
	delete[] m_botFilename;
	delete[] m_versionString;
	m_description = NULL;
	m_title = NULL;
	m_author = NULL;
	m_url = NULL;
	m_asciiTitle = NULL;
	m_botFilename = NULL;
	m_careerName = NULL;
	m_saveDir = NULL;
	m_versionString = NULL;
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; ++i )
		m_difficulty[i].Reset();
}

CareerDefinition::~CareerDefinition()
{
	Reset();
}

BotProfileManager* CareerDefinition::GetBotProfileManager( CareerDifficultyType difficulty )
{
	return m_difficulty[difficulty].GetBotProfileManager();
}

bool CareerDefinition::Init( const char* careerName )
{
	m_isCT = true;
	m_careerName = CloneString( careerName );
	char path[CAREER_MAX_PATH];
	if ( !stricmp( careerName, "TurtleRockCounterTerrorist" ) )
	{
		m_saveDir = CloneString( "Save" );
	}
	else
	{
#ifdef _WIN32
		_snprintf( path, sizeof( path ), "MissionPacks/%s/Save", careerName );
#else
		snprintf( path, sizeof( path ), "MissionPacks/%s/Save", careerName );
#endif
		m_saveDir = CloneString( path );
	}
#ifdef _WIN32
	_snprintf( path, sizeof( path ), "MissionPacks/%s/overview.vdf", careerName );
#else
	snprintf( path, sizeof( path ), "MissionPacks/%s/overview.vdf", careerName );
#endif
	KeyValues* data = new KeyValues( "MissionPack" );
	if ( data->LoadFromFile( vgui2::filesystem(), path, NULL ) )
	{
		m_author = CloneWString( data->GetWString( "Author", L"<unknown>" ) );
		m_title = CloneWString( data->GetWString( "Title", L"<unknown>" ) );
		m_asciiTitle = CloneString( data->GetString( "GameName", data->GetString( "Title", "<unknown>" ) ) );
		m_description = CloneWString( data->GetWString( "Description", L"" ) );
		m_url = CloneString( data->GetString( "URL", "" ) );
		m_isCT = !stricmp( "CT", data->GetString( "Team", "CT" ) );
		m_versionString = CloneString( data->GetString( "Version", "1" ) );
		m_bgStartColor = data->GetColor( "BGColor1" );
		if ( m_bgStartColor.GetRawColor() == 0 )
			m_bgStartColor = Color( 60, 60, 60, 255 );
		m_bgStartColor[3] = 255;
		m_bgEndColor = data->GetColor( "BGColor2" );
		if ( m_bgEndColor.GetRawColor() == 0 )
			m_bgEndColor = Color( 60, 60, 60, 255 );
		m_bgEndColor[3] = 255;
		m_textColor = data->GetColor( "TextColor" );
		if ( m_textColor.GetRawColor() == 0 )
			m_textColor = Color( 28, 207, 61, 255 );
		m_textColor[3] = 255;
		const char* botFilename = data->GetString( "BotProfile", "" );
		if ( vgui2::filesystem()->FileExists( SharedVarArgs( "MissionPacks/%s/%s", careerName, botFilename ) ) )
			m_botFilename = CloneString( botFilename );
		m_difficulty[CAREER_DIFFICULTY_EASY].Init( careerName, "Easy.vdf", botFilename );
		m_difficulty[CAREER_DIFFICULTY_NORMAL].Init( careerName, "Normal.vdf", botFilename );
		m_difficulty[CAREER_DIFFICULTY_HARD].Init( careerName, "Hard.vdf", botFilename );
		m_difficulty[CAREER_DIFFICULTY_EXPERT].Init( careerName, "Expert.vdf", botFilename );
	}
	data->deleteThis();
	if ( !m_versionString )
		m_versionString = CloneString( "1" );
	return m_title && m_title[0] && m_asciiTitle && m_asciiTitle[0];
}

CareerDefinitionManager::CareerDefinitionManager()
{
	char directory[CAREER_MAX_PATH];
	sprintf( directory, "MissionPacks/*" );
	FileFindHandle_t handle;
	const char* name = vgui2::filesystem()->FindFirst( directory, &handle, "GAMECONFIG" );
	while ( name )
	{
		if ( name[0] != '.' && vgui2::filesystem()->FindIsDirectory( handle ) )
		{
			CareerDefinition* definition = new CareerDefinition;
			if ( definition->Init( name ) )
			{
				Career_Printf( "Adding MissionPack %s (save dir %s)\n", name, definition->GetSaveDirectory() );
				m_careers.AddElement( definition );
			}
			else
			{
				delete definition;
			}
		}
		name = vgui2::filesystem()->FindNext( handle );
	}
	vgui2::filesystem()->FindClose( handle );
	m_activeCareer = NULL;
	KeyValues* info = new KeyValues( "MissionPackInfo" );
	if ( info->LoadFromFile( vgui2::filesystem(), "MissionPackInfo.vdf", NULL ) )
		m_activeCareer = GetCareerDefinition( info->GetString( "ActiveMissionPack", "TurtleRockCounterTerrorist" ) );
	info->deleteThis();
	if ( !m_activeCareer && m_careers.GetCount() )
		SetActiveCareerDefinition( m_careers[0]->GetCareerName() );
	if ( m_activeCareer )
		TheBotProfiles = m_activeCareer->GetBotProfileManager( TheCareerGame->GetDifficulty() );
}

CareerDefinitionManager::~CareerDefinitionManager()
{
	for ( int i = 0; i < m_careers.GetCount(); ++i )
		delete m_careers[i];
	m_careers.RemoveAll();
}

void CareerDefinitionManager::Create()
{
	if ( !TheCareerDefinitions )
		TheCareerDefinitions = new CareerDefinitionManager;
}

CareerDefinition* CareerDefinitionManager::GetCareerDefinition( const char* name )
{
	for ( int i = 0; i < m_careers.GetCount(); ++i )
	{
		if ( !stricmp( name, m_careers[i]->GetCareerName() ) )
			return m_careers[i];
	}
	return NULL;
}

int CareerDefinitionManager::GetNumCareers()
{
	return m_careers.GetCount();
}

CareerDefinition* CareerDefinitionManager::GetCareer( int index )
{
	return m_careers[index];
}

void CareerDefinitionManager::SetActiveCareerDefinition( const char* name )
{
	m_activeCareer = GetCareerDefinition( name );
	KeyValues* info = new KeyValues( "MissionPackInfo" );
	info->LoadFromFile( vgui2::filesystem(), "MissionPackInfo.vdf", NULL );
	if ( m_activeCareer )
	{
		info->SetString( "ActiveMissionPack", m_activeCareer->GetCareerName() );
		TheBotProfiles = m_activeCareer->GetBotProfileManager( TheCareerGame->GetDifficulty() );
	}
	info->SaveToFile( vgui2::filesystem(), "MissionPackInfo.vdf", "GAMECONFIG" );
	info->deleteThis();
}

void CareerDefinitionManager::ReloadCareerDefinition( const char* name )
{
	CareerDefinition* definition = GetCareerDefinition( name );
	if ( definition )
	{
		definition->Reset();
		if ( !definition->Init( name ) )
		{
			Career_Printf( "Removing MissionPack %s\n", name );
			m_careers.RemoveElement( definition );
			delete definition;
		}
	}
	else
	{
		definition = new CareerDefinition;
		if ( definition->Init( name ) )
		{
			Career_Printf( "Adding MissionPack %s (save dir %s)\n", name, definition->GetSaveDirectory() );
			m_careers.AddElement( definition );
		}
	}
}
