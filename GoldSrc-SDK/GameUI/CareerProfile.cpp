#include "CareerProfile.h"
#include "CareerGame.h"
#include "CareerDefinitions.h"
#include "GameUI_Interface.h"
#include "StringUtils.h"
#include "Taskbar.h"

#include "filesystem.h"
#include "MessageBox.h"
#include "parsemsg.h"
#include "vgui_controls/Controls.h"
#include "../game_shared/simple_checksum.h"
#include "checksum_md5.h"
#include "wrect.h"
#include "cl_dll.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <memory>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

#ifdef MessageBox
#undef MessageBox
#endif

// READ_STRING reads profile strings into char[2048]
#define CAREER_PROFILE_NAME_BYTES 2048
// writecfg formats its argument into 257 bytes before it appends ".cfg"
#define CAREER_CONFIG_NAME_BYTES 257

static unsigned char* ReadFile( const char* filename, int& length )
{
	FileHandle_t file = vgui2::filesystem()->Open( filename, "rb", NULL );
	if ( !file )
	{
		length = 0;
		return NULL;
	}
	unsigned int fileSize = vgui2::filesystem()->Size( file );
	if ( fileSize > INT_MAX )
	{
		vgui2::filesystem()->Close( file );
		length = 0;
		return NULL;
	}
	length = (int)fileSize;
	unsigned char* data = new unsigned char[length];
	int bytesRead = vgui2::filesystem()->Read( data, length, file );
	vgui2::filesystem()->Close( file );
	if ( bytesRead != length )
	{
		delete[] data;
		length = 0;
		return NULL;
	}
	return data;
}

static std::string LegacyProfileKey( const char* name )
{
	const size_t legacyNameBytes = 31;
	std::string key( name, strlen( name ) < legacyNameBytes ? strlen( name ) : legacyNameBytes );
	for ( char& c : key )
	{
		if ( !strchr( "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-0123456789", c ) )
			c = '_';
	}
	return key;
}

// The c22ae8c key, read and migrated like the native key
static std::string HashedProfileKey( const char* name )
{
	MD5Context_t context;
	unsigned char digest[16];
	MD5Init( &context );
	MD5Update( &context, (const unsigned char*)name, (unsigned int)strlen( name ) );
	MD5Final( digest, &context );
	std::string key = "v2-";
	char hex[3];
	for ( size_t i = 0; i < sizeof( digest ); ++i )
	{
		sprintf( hex, "%02x", digest[i] );
		key += hex;
	}
	return key;
}

// Matches CareerProfileStorage::ValidateName in VGUI2Extension: valid UTF-8 without
// control characters or the characters that split console commands
static bool IsSafeProfileName( const char* name )
{
	const unsigned char* text = (const unsigned char*)name;
	size_t length = strlen( name );
	if ( !length || length >= CAREER_PROFILE_NAME_BYTES )
		return false;
	for ( size_t i = 0; i < length; )
	{
		unsigned int first = text[i++];
		unsigned int code = first;
		unsigned int minimum = 0;
		size_t tail = 0;
		if ( first >= 0xC2 && first <= 0xDF )
		{
			code = first & 0x1F;
			minimum = 0x80;
			tail = 1;
		}
		else if ( first >= 0xE0 && first <= 0xEF )
		{
			code = first & 0x0F;
			minimum = 0x800;
			tail = 2;
		}
		else if ( first >= 0xF0 && first <= 0xF4 )
		{
			code = first & 0x07;
			minimum = 0x10000;
			tail = 3;
		}
		else if ( first >= 0x80 )
			return false;
		if ( tail > length - i )
			return false;
		for ( ; tail; --tail )
		{
			unsigned int next = text[i++];
			if ( ( next & 0xC0 ) != 0x80 )
				return false;
			code = ( code << 6 ) | ( next & 0x3F );
		}
		if ( code < minimum || code > 0x10FFFF || ( code >= 0xD800 && code <= 0xDFFF ) )
			return false;
		if ( code < 0x20 || ( code >= 0x7F && code <= 0x9F ) || code == '"' || code == ';' || code == '\\' )
			return false;
	}
	return true;
}

// Keys from the newest scheme to the oldest; unsafe names have no u8hex key
static std::vector<std::string> ProfileKeys( const char* name )
{
	std::vector<std::string> keys;
	std::string key = CCareerGame::GetSaveGamePlayerName( name );
	if ( !key.empty() )
		keys.push_back( key );
	keys.push_back( HashedProfileKey( name ) );
	keys.push_back( LegacyProfileKey( name ) );
	return keys;
}

static std::string ProfilePath( const std::string& key )
{
	return std::string( TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory() ) +
		"/CZProfile-" + key + ".czp";
}

static std::unique_ptr<CCareerProfileData> ReadOwnedProfile( CCareerGame* game,
	const std::string& path, const char* owner )
{
	int length = 0;
	std::unique_ptr<unsigned char[]> data( ReadFile( path.c_str(), length ) );
	std::unique_ptr<CCareerProfileData> profile( new CCareerProfileData );
	if ( !data || !game->ParseSavedProfile( data.get(), length, profile.get() ) || profile->name != owner )
		return nullptr;
	return profile;
}

static bool WriteProfileFile( const std::string& path, const unsigned char* data, int length )
{
	FileHandle_t file = vgui2::filesystem()->Open( path.c_str(), "wb", NULL );
	if ( !file )
		return false;
	bool written = vgui2::filesystem()->Write( data, length, file ) == length && vgui2::filesystem()->IsOk( file );
	vgui2::filesystem()->Close( file );
	return written;
}

static bool ConfigHasOwner( const std::string& path, const char* owner );

static bool SameProfilePath( const std::string& left, const std::string& right )
{
#ifdef _WIN32
	return _stricmp( left.c_str(), right.c_str() ) == 0;
#else
	return left == right;
#endif
}

static bool CanWriteProfile( CCareerGame* game, const char* name )
{
	std::string key = CCareerGame::GetSaveGamePlayerName( name );
	if ( key.empty() )
		return false;
	std::string path = ProfilePath( key );
	std::string config = "Save/" + key + ".cfg";
	if ( vgui2::filesystem()->FileExists( path.c_str() ) )
		return ReadOwnedProfile( game, path, name ) &&
			( !vgui2::filesystem()->FileExists( config.c_str() ) || ConfigHasOwner( config, name ) );
	// An orphaned config cannot establish ownership of a newly created profile.
	return !vgui2::filesystem()->FileExists( ( "Save/" + key + ".cfg" ).c_str() );
}

static bool ConfigHasOwner( const std::string& path, const char* owner )
{
	int length = 0;
	std::unique_ptr<unsigned char[]> data( ReadFile( path.c_str(), length ) );
	if ( !data )
		return false;
	std::string contents( (const char*)data.get(), length );
	if ( contents.find( '\0' ) != std::string::npos )
		return false;
	std::vector<char> token( (size_t)length + 1, '\0' );
	bool found = false;
	for ( size_t start = 0; start < contents.size(); )
	{
		// Match the engine command buffer: newline, or semicolon outside quotes.
		size_t end = start;
		bool inQuotes = false;
		for ( ; end < contents.size(); ++end )
		{
			if ( contents[end] == '"' )
				inQuotes = !inQuotes;
			if ( contents[end] == '\n' || ( !inQuotes && contents[end] == ';' ) )
				break;
		}
		const size_t maxEngineCommandBytes = 1024;
		if ( end - start >= maxEngineCommandBytes || inQuotes )
			return false;
		std::string line = contents.substr( start, end - start );
		bool quoted = false;
		char* cursor = vgui2::filesystem()->ParseFile( &line[0], token.data(), &quoted );
		// Generated profile configs have no nested scripts or aliases to establish ownership.
		if ( !stricmp( token.data(), "exec" ) || !stricmp( token.data(), "alias" ) )
			return false;
		if ( cursor && !stricmp( token.data(), "name" ) )
		{
			vgui2::filesystem()->ParseFile( cursor, token.data(), &quoted );
			if ( strcmp( token.data(), owner ) )
				return false;
			found = true;
		}
		start = end + 1;
	}
	return found;
}

static bool MigrateLegacyProfile( CCareerGame* game, const char* name )
{
	std::string key = CCareerGame::GetSaveGamePlayerName( name );
	if ( key.empty() )
		return false;
	std::string path = ProfilePath( key );
	bool canonicalExists = vgui2::filesystem()->FileExists( path.c_str() );
	if ( canonicalExists && !ReadOwnedProfile( game, path, name ) )
		return false;
	// Copy from the newest older key that this name owns
	std::vector<std::string> keys = ProfileKeys( name );
	std::vector<std::string>::const_iterator older = keys.begin() + 1;
	while ( older != keys.end() && !ReadOwnedProfile( game, ProfilePath( *older ), name ) )
		++older;
	if ( older == keys.end() )
		return true;
	const std::string& legacyKey = *older;
	std::string legacyPath = ProfilePath( legacyKey );
	if ( !canonicalExists && !CanWriteProfile( game, name ) )
		return false;
	int length = 0;
	if ( !canonicalExists )
	{
		std::unique_ptr<unsigned char[]> data( ReadFile( legacyPath.c_str(), length ) );
		if ( !data || !WriteProfileFile( path, data.get(), length ) )
			return false;
	}
	std::string legacyConfig = "Save/" + legacyKey + ".cfg";
	std::string configPath = "Save/" + key + ".cfg";
	if ( !vgui2::filesystem()->FileExists( configPath.c_str() ) && ConfigHasOwner( legacyConfig, name ) )
	{
		std::unique_ptr<unsigned char[]> config( ReadFile( legacyConfig.c_str(), length ) );
		vgui2::filesystem()->CreateDirHierarchy( "Save", NULL );
		if ( !config || !WriteProfileFile( configPath, config.get(), length ) )
			return false;
	}
	return true;
}

void CCareerProfileDifficultyData::Reset()
{
	played = false;
	points = 0;
	hired.clear();
	maps.clear();
	lastMap = "";
	pointsFlashed = 0;
}

void CCareerProfileData::Reset()
{
	name = "";
	sliderPos = 0;
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; ++i )
	{
		medals[i] = false;
		difficulty[i].Reset();
	}
	memset( tutorData, 0, sizeof( tutorData ) );
}

bool CCareerGame::ParseMedalChunk( CCareerProfileData* save, int version )
{
	if ( (unsigned int)version - 1 >= CAREER_PROFILE_VERSION )
		return false;
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; ++i )
		save->medals[i] = READ_BYTE() != 0;
	return READ_OK() != 0;
}

bool CCareerGame::ParseDifficultyChunk( CCareerProfileDifficultyData* save, int version )
{
	if ( (unsigned int)version - 1 >= CAREER_PROFILE_VERSION )
		return false;
	save->played = READ_BYTE() != 0;
	if ( save->played )
	{
		save->points = READ_BYTE();
		int count = READ_BYTE();
		if ( (unsigned int)count > 5 )
			return false;
		for ( int i = 0; i < count; ++i )
			save->hired.push_back( READ_STRING() );
		count = READ_BYTE();
		if ( count < 3 || count % 3 )
			return false;
		for ( int i = 0; i < count; ++i )
		{
			SavedMap map;
			map.name = READ_STRING();
			map.defeated = READ_BYTE() != 0;
			save->maps.push_back( map );
		}
		save->lastMap = READ_STRING();
		save->pointsFlashed = version > 2 ? READ_BYTE() : 0;
	}
	return READ_OK() != 0;
}

bool CCareerGame::ParseTutorMessageDecayChunk( CCareerProfileData* save, int version )
{
	if ( (unsigned int)version - 1 >= CAREER_PROFILE_VERSION || !save )
		return false;
	memset( save->tutorData, 0, sizeof( save->tutorData ) );
	if ( version != 1 )
	{
		int count = READ_LONG();
		for ( int i = 0; i < count; ++i )
		{
			int value = READ_LONG();
			if ( i < MAX_CAREER_TUTOR_MESSAGES )
				save->tutorData[i] = value;
		}
	}
	return true;
}

bool CCareerGame::ParseSavedProfile( const unsigned char* data, int dataLength, CCareerProfileData* save )
{
	if ( !save || dataLength <= 4 || !data )
		return false;
	save->Reset();
	BEGIN_READ( (void*)data, dataLength );
	const char* tag = READ_STRING();
	if ( !tag || strcmp( tag, "CZP" ) )
		return false;
	int version = READ_LONG();
	if ( (unsigned int)version - 1 >= CAREER_PROFILE_VERSION )
		return false;
	save->name = READ_STRING();
	save->sliderPos = READ_LONG();
	if ( version == CAREER_PROFILE_VERSION )
	{
		save->lastDifficulty = (CareerDifficultyType)READ_BYTE();
		if ( (unsigned int)save->lastDifficulty >= MAX_CAREER_DIFFICULTY )
			save->lastDifficulty = CAREER_DIFFICULTY_EASY;
	}
	else
	{
		save->lastDifficulty = CAREER_DIFFICULTY_EASY;
	}
	bool valid = READ_OK() != 0;
	if ( valid )
		valid = ParseMedalChunk( save, version );
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; ++i )
	{
		if ( valid )
			valid = ParseDifficultyChunk( &save->difficulty[i], version );
	}
	if ( valid )
		valid = ParseTutorMessageDecayChunk( save, version );
	unsigned int checksum = (unsigned int)READ_LONG();
#ifdef _WIN32
	if ( !valid )
		return false;
#endif
	unsigned int expected = ComputeSimpleChecksum( data, dataLength - sizeof( int ) );
	return valid && checksum == expected;
}

std::string CCareerGame::GetSaveGamePlayerName( const char* name )
{
	if ( !name )
	{
		char nameCvar[] = "name";
		name = engine->pfnGetCvarString( nameCvar );
	}
	if ( !name || !IsSafeProfileName( name ) )
		return "";
	// Same key as VGUI2Extension: u8hex- and the lowercase hex of every name byte
	static const char hexDigits[] = "0123456789abcdef";
	std::string key = "u8hex-";
	for ( const unsigned char* c = (const unsigned char*)name; *c; ++c )
	{
		key += hexDigits[*c >> 4];
		key += hexDigits[*c & 0x0F];
	}
	// A longer writecfg argument would be truncated into the config of another key
	if ( ( "Save/" + key ).size() >= CAREER_CONFIG_NAME_BYTES )
		return "";
	return key;
}

bool CCareerGame::IsValidProfileName( const char* name )
{
	return name && !GetSaveGamePlayerName( name ).empty();
}

void CCareerGame::ShowInvalidProfileNameMessage()
{
	// Each name byte takes two hex digits after "Save/u8hex-" in the writecfg argument
	const size_t maxNameBytes = ( CAREER_CONFIG_NAME_BYTES - 1 - strlen( "Save/u8hex-" ) ) / 2;
	char text[256];
	UTIL_FormatString( text, sizeof( text ), "This name cannot be used for a career profile. It must be valid UTF-8, "
		"must not contain control characters, quotes, semicolons or backslashes, and must be at most %u bytes.",
		(unsigned int)maxNameBytes );
	vgui2::MessageBox* message = new vgui2::MessageBox( "Career profile", text, g_pTaskbar );
	message->DoModal( NULL );
}

void CCareerGame::ApplyTutorMessageData( int* tutorData )
{
	engine->pfnProcessTutorMessageDecayBuffer( tutorData, MAX_CAREER_TUTOR_MESSAGES );
}

void CCareerGame::WriteMedalChunk( CCareerProfileData* save )
{
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; ++i )
		m_writer->WriteByte( save->medals[i] );
}

void CCareerGame::WriteDifficultyChunk( CCareerProfileDifficultyData* save )
{
	m_writer->WriteByte( save->played );
	if ( !save->played )
		return;
	m_writer->WriteByte( (unsigned char)save->points );
	m_writer->WriteByte( (unsigned char)save->hired.size() );
	for ( StringVector::iterator it = save->hired.begin(); it != save->hired.end(); ++it )
		m_writer->WriteString( it->c_str() );
	m_writer->WriteByte( (unsigned char)save->maps.size() );
	for ( SavedMapVector::iterator it = save->maps.begin(); it != save->maps.end(); ++it )
	{
		m_writer->WriteString( it->name.c_str() );
		m_writer->WriteByte( it->defeated );
	}
	m_writer->WriteString( save->lastMap.c_str() );
	m_writer->WriteByte( (unsigned char)save->pointsFlashed );
}

void CCareerGame::WriteTutorMessageDecayChunk( bool isNewProfile )
{
	int buffer[MAX_CAREER_TUTOR_MESSAGES];
	if ( isNewProfile )
		memset( buffer, 0, sizeof( buffer ) );
	else
		engine->pfnConstructTutorMessageDecayBuffer( buffer, MAX_CAREER_TUTOR_MESSAGES );
	m_writer->WriteLong( MAX_CAREER_TUTOR_MESSAGES );
	for ( int i = 0; i < MAX_CAREER_TUTOR_MESSAGES; ++i )
		m_writer->WriteLong( buffer[i] );
}

int CCareerGame::CreateProfileBuffer( CCareerProfileData* save, unsigned char* data,
	int dataLength, bool isNewProfile )
{
	m_writer->Init( data, dataLength );
	m_writer->WriteString( "CZP" );
	m_writer->WriteLong( CAREER_PROFILE_VERSION );
	m_writer->WriteString( save->name.c_str() );
	m_writer->WriteLong( save->sliderPos );
	m_writer->WriteByte( (unsigned char)save->lastDifficulty );
	WriteMedalChunk( save );
	for ( int i = 0; i < MAX_CAREER_DIFFICULTY; ++i )
		WriteDifficultyChunk( &save->difficulty[i] );
	WriteTutorMessageDecayChunk( isNewProfile );
	unsigned int checksum = ComputeSimpleChecksum( data, m_writer->GetSpaceUsed() );
	m_writer->WriteLong( (int)checksum );
	return m_writer->GetSpaceUsed();
}

void CCareerGame::UpdateMedalChunk()
{
	memcpy( m_currentProfile->medals, m_medals, sizeof( m_medals ) );
}

void CCareerGame::ApplySavedProfile( CCareerProfileData* save, CareerDifficultyType difficulty )
{
	if ( !save )
		return;
	engine->Cvar_Set( "name", (char*)save->name.c_str() );
	m_botPoolSliderPosition = save->sliderPos;
	memcpy( m_medals, save->medals, sizeof( m_medals ) );
	if ( save->difficulty[difficulty].played )
		ApplyDifficultyChunk( &save->difficulty[difficulty] );
	ApplyTutorMessageData( save->tutorData );
}

void CCareerGame::ApplyDifficultyChunk( CCareerProfileDifficultyData* save )
{
	m_reputationPoints = save->points;
	for ( StringVector::iterator it = save->hired.begin(); it != save->hired.end(); ++it )
		Hire( it->c_str() );
	for ( SavedMapVector::iterator it = save->maps.begin(); it != save->maps.end(); ++it )
	{
		CCareerMap* map = GetPlayableMap( it->name.c_str() );
		if ( !map )
		{
			char mapName[64];
			if ( UTIL_FormatString( mapName, sizeof( mapName ), "%s_cz", it->name.c_str() ) )
				map = GetPlayableMap( mapName );
		}
		if ( map )
			map->SetDefeated( it->defeated );
	}
	SetLastMap( save->lastMap.c_str() );
	m_reputationPointsFlashed = save->pointsFlashed;
}

void CCareerGame::UpdateDifficultyChunk()
{
	CCareerProfileDifficultyData& save = m_currentProfile->difficulty[m_difficulty];
	save.played = true;
	save.points = m_reputationPoints;
	save.hired.clear();
	for ( CharacterVec::iterator it = m_hiredCharacters.begin(); it != m_hiredCharacters.end(); ++it )
	{
		if ( *it )
			save.hired.push_back( ( *it )->GetName() );
	}
	save.maps.clear();
	for ( MapsVec::iterator group = m_maps.begin(); group != m_maps.end(); ++group )
	{
		for ( std::vector<CCareerMap*>::iterator it = group->begin(); it != group->end(); ++it )
		{
			SavedMap map;
			map.defeated = ( *it )->IsDefeated();
			map.name = ( *it )->GetName();
			save.maps.push_back( map );
		}
	}
	save.lastMap = m_lastMapName;
	save.pointsFlashed = m_reputationPointsFlashed;
}

bool CCareerGame::UpdateSavedProfile()
{
	m_currentProfile->sliderPos = m_botPoolSliderPosition;
	m_currentProfile->lastDifficulty = m_difficulty;
	UpdateMedalChunk();
	UpdateDifficultyChunk();
	return true;
}

ProfileVector CCareerGame::BuildProfileList()
{
	std::string saveDirectory = TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory();
	std::string directory = saveDirectory + "/CZProfile-*.czp";
	ProfileVector profiles;
	FileFindHandle_t handle;
	const char* name = vgui2::filesystem()->FindFirst( directory.c_str(), &handle, "GAMECONFIG" );
	while ( name )
	{
		std::string filename = saveDirectory + "/" + name;
		int length = 0;
		unsigned char* data = ReadFile( filename.c_str(), length );
		CCareerProfileData* profile = NULL;
		if ( data )
		{
			profile = new CCareerProfileData;
			if ( !ParseSavedProfile( data, length, profile ) )
			{
				delete profile;
				profile = NULL;
			}
			delete[] data;
		}
		if ( profile )
		{
			// List each name once, from the newest key that has a profile
			bool validPath = false;
			std::vector<std::string> keys = ProfileKeys( profile->name.c_str() );
			for ( const std::string& key : keys )
			{
				std::string path = ProfilePath( key );
				if ( SameProfilePath( path, filename ) )
				{
					validPath = true;
					break;
				}
				if ( vgui2::filesystem()->FileExists( path.c_str() ) )
					break;
			}
			if ( validPath )
			{
				profile->filetime = vgui2::filesystem()->GetFileTime( filename.c_str() );
				profiles.push_back( profile );
			}
			else
				delete profile;
		}
		name = vgui2::filesystem()->FindNext( handle );
	}
	vgui2::filesystem()->FindClose( handle );
	return profiles;
}

CCareerProfileData* CCareerGame::GetProfileForName( const char* name )
{
	if ( !name )
		name = engine->pfnGetCvarString( (char*)"name" );
	if ( !name )
		return NULL;
	std::vector<std::string> keys = ProfileKeys( name );
	for ( const std::string& key : keys )
	{
		std::string path = ProfilePath( key );
		if ( vgui2::filesystem()->FileExists( path.c_str() ) )
			return ReadOwnedProfile( this, path, name ).release();
	}
	return NULL;
}

void CCareerGame::DeleteProfile( const char* name )
{
	if ( !name )
		return;
	std::vector<std::string> keys = ProfileKeys( name );
	for ( const std::string& key : keys )
	{
		std::string path = ProfilePath( key );
		if ( ReadOwnedProfile( this, path, name ) )
		{
			vgui2::filesystem()->RemoveFile( path.c_str(), NULL );
			std::string config = "Save/" + key + ".cfg";
			if ( ConfigHasOwner( config, name ) )
				vgui2::filesystem()->RemoveFile( config.c_str(), NULL );
		}
	}
}

bool CCareerGame::CreateBlankProfile()
{
	CCareerProfileData profile;
	profile.Reset();
	char nameCvar[] = "name";
	profile.name = engine->pfnGetCvarString( nameCvar );
	if ( profile.name.empty() || !MigrateLegacyProfile( this, profile.name.c_str() ) ||
		!CanWriteProfile( this, profile.name.c_str() ) )
		return false;
	profile.sliderPos = 0;
	profile.lastDifficulty = CAREER_DIFFICULTY_EASY;
	memset( profile.medals, 0, sizeof( profile.medals ) );
	vgui2::filesystem()->CreateDirHierarchy( TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory(), NULL );
	std::unique_ptr<unsigned char[]> data( new unsigned char[MAX_CAREER_PROFILE_DATA] );
	int length = CreateProfileBuffer( &profile, data.get(), MAX_CAREER_PROFILE_DATA, true );
	return WriteProfileFile( ProfilePath( GetSaveGamePlayerName( profile.name.c_str() ) ), data.get(), length );
}

bool CCareerGame::SaveProfile( bool updateConfig )
{
	if ( !m_needAutosave )
		return true;
	const char* name = m_currentProfile->name.c_str();
	if ( !name[0] || !MigrateLegacyProfile( this, name ) || !CanWriteProfile( this, name ) )
		return false;
	std::string key = GetSaveGamePlayerName( name );
	bool saved = m_needAutosave;
	std::unique_ptr<unsigned char[]> data( new unsigned char[MAX_CAREER_PROFILE_DATA] );
	vgui2::filesystem()->CreateDirHierarchy( TheCareerDefinitions->GetActiveCareerDefinition()->GetSaveDirectory(), NULL );
	UpdateSavedProfile();
	int length = CreateProfileBuffer( m_currentProfile, data.get(), MAX_CAREER_PROFILE_DATA, false );
	saved = WriteProfileFile( ProfilePath( key ), data.get(), length );
	if ( saved && updateConfig && m_currentProfile->name == engine->pfnGetCvarString( (char*)"name" ) )
	{
		std::string command = "writecfg Save/" + key + "\n";
		engine->pfnClientCmd( (char*)command.c_str() );
	}
	return saved;
}

void CCareerGame::ExecuteProfileConfig()
{
	const char* name = m_currentProfile->name.c_str();
	std::string key = GetSaveGamePlayerName( name );
	if ( !key.empty() && ReadOwnedProfile( this, ProfilePath( key ), name ) &&
		ConfigHasOwner( "Save/" + key + ".cfg", name ) )
	{
		std::string command = "wait\nexec Save/" + key + ".cfg\n";
		engine->pfnClientCmd( (char*)command.c_str() );
	}
}

bool CCareerGame::LoadProfile( CareerDifficultyType difficulty )
{
	const char* name = engine->pfnGetCvarString( (char*)"name" );
	if ( !name )
		return false;
	std::string key = GetSaveGamePlayerName( name );
	if ( key.empty() )
	{
		ShowInvalidProfileNameMessage();
		return false;
	}
	if ( !MigrateLegacyProfile( this, name ) )
		return false;
	std::string filename = ProfilePath( key );
	int length;
	unsigned char* data = ReadFile( filename.c_str(), length );
	if ( !data )
		return !vgui2::filesystem()->FileExists( filename.c_str() );
	SetDifficulty( difficulty );
	Reset();
	bool valid = ParseSavedProfile( data, length, m_currentProfile ) && m_currentProfile->name == name;
	delete[] data;
	if ( !valid )
	{
		vgui2::MessageBox* message = new vgui2::MessageBox( "#Career_CorruptSaveTitle", "#Career_CorruptSaveText", g_pTaskbar );
		message->DoModal( NULL );
		return false;
	}
	ApplySavedProfile( m_currentProfile, difficulty );
	return true;
}
