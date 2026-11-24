#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "bot_util.h"
#include "cs_bot_chatter.h"
#include "cs_bot.h"
#include "cs_bot_manager.h"
#include "nav_area.h"
#include <algorithm>
#include <ctype.h>

extern char *MP_COM_Parse( char *data );
extern char *MP_COM_GetToken( void );

BotPhraseManager *TheBotPhrases = NULL;
IntervalTimer BotChatterInterface::m_radioSilenceInterval[2];
CountdownTimer BotChatterInterface::m_encourageTimer;

namespace
{
	const CountCriteria ANY_COUNT = 0xFFFF;
	const CountCriteria MANY_ENEMIES = 4;
}

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
BotSpeakable::BotSpeakable( void )
{
	m_file = NULL;
}

BotSpeakable::~BotSpeakable()
{
	delete [] m_file;
}

BotPhrase::BotPhrase( unsigned int id, bool isPlace )
{
	m_name = NULL;
	m_id = id;
	m_radioEvent = EVENT_INVALID;
	m_isImportant = false;
	m_placeCriteria = ANY_PLACE;
	m_countCriteria = ANY_COUNT;
	m_numVoiceBanks = 0;
	m_isPlace = isPlace;
	InitVoiceBank( 0 );
}

BotPhrase::~BotPhrase()
{
	for ( size_t bank = 0; bank < m_voiceBank.size(); ++bank )
	{
		BotVoiceBank *voiceBank = m_voiceBank[bank];
		for ( size_t i = 0; i < voiceBank->size(); ++i )
			delete (*voiceBank)[i];
		delete voiceBank;
	}
	delete[] m_name;
}

void BotPhrase::InitVoiceBank( int voiceBank )
{
	while ( voiceBank >= m_numVoiceBanks )
	{
		m_count.push_back( 0 );
		m_index.push_back( 0 );
		m_voiceBank.push_back( new BotVoiceBank );
		++m_numVoiceBanks;
	}
}

void BotPhrase::Randomize( void )
{
	for ( size_t bank = 0; bank < m_voiceBank.size(); ++bank )
	{
		BotVoiceBank &voiceBank = *m_voiceBank[bank];
		for ( size_t i = 1; i < voiceBank.size(); ++i )
		{
			size_t other = static_cast<size_t>(rand()) % (i + 1);
			if ( other != i )
				std::swap( voiceBank[i], voiceBank[other] );
		}
	}
}

const char *BotPhrase::GetSpeakable( int voiceBank, float *duration ) const
{
	if ( voiceBank >= 0 && voiceBank < m_numVoiceBanks && m_count[voiceBank] != 0 )
	{
		int first = m_index[voiceBank];
		do
		{
			const BotSpeakable *speakable = (*m_voiceBank[voiceBank])[m_index[voiceBank]++];
			if ( m_index[voiceBank] >= m_count[voiceBank] )
				m_index[voiceBank] = 0;

			if ( speakable->m_place != ANY_PLACE && speakable->m_place != m_placeCriteria )
				continue;
			if ( speakable->m_count != ANY_COUNT && speakable->m_count != std::min( m_countCriteria, MANY_ENEMIES ) )
				continue;

			if ( duration )
				*duration = speakable->m_duration;
			return speakable->m_file;
		}
		while ( m_index[voiceBank] != first );
	}

	if ( duration )
		*duration = 0.0f;
	return NULL;
}

BotPhraseManager::BotPhraseManager( void )
{
	m_placeCount = 0;
}

BotPhraseManager::~BotPhraseManager()
{
	for ( BotPhraseList::iterator it = m_list.begin(); it != m_list.end(); ++it )
		delete *it;
	for ( BotPhraseList::iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
		delete *it;
	m_list.clear();
	m_placeList.clear();
}

bool BotPhraseManager::Initialize( const char *filename, int voiceBank )
{
	int length;
	char *file = reinterpret_cast<char *>(g_engfuncs.pfnLoadFileForMe( const_cast<char *>(filename), &length ));
	if ( file == NULL )
	{
		if ( UTIL_IsGame( "czero" ) )
			CONSOLE_ECHO( "WARNING: Cannot access bot phrase database '%s'\n", filename );
		return false;
	}

	char baseDirectory[128] = {};
	unsigned int nextID = 1;
	char *data = file;
	while ( (data = MP_COM_Parse( data )) != NULL )
	{
		const char *token = MP_COM_GetToken();
		if ( stricmp( token, "BaseDir" ) == 0 )
		{
			data = MP_COM_Parse( data );
			if ( data == NULL )
			{
				CONSOLE_ECHO( "Error parsing '%s' - expected identifier\n", filename );
				g_engfuncs.pfnFreeFile( file );
				return false;
			}
			strncpy( baseDirectory, MP_COM_GetToken(), sizeof(baseDirectory) );
			baseDirectory[sizeof(baseDirectory) - 1] = '\0';
			continue;
		}

		bool isPlace = stricmp( token, "Place" ) == 0;
		if ( !isPlace && stricmp( token, "Chatter" ) != 0 )
			continue;

		BotPhrase *phrase;
		if ( voiceBank == 0 )
			phrase = new BotPhrase( nextID++, isPlace );
		else
			phrase = NULL;

		data = MP_COM_Parse( data );
		if ( data == NULL )
		{
			CONSOLE_ECHO( "Error parsing '%s' - expected identifier\n", filename );
			delete phrase;
			g_engfuncs.pfnFreeFile( file );
			return false;
		}
		token = MP_COM_GetToken();
		if ( voiceBank != 0 )
		{
			phrase = const_cast<BotPhrase *>(isPlace ? GetPlace( token ) : GetPhrase( token ));
			if ( phrase == NULL )
			{
				CONSOLE_ECHO( "Error parsing '%s' - phrase '%s' is invalid\n", filename, token );
				g_engfuncs.pfnFreeFile( file );
				return false;
			}
		}
		else
		{
			phrase->m_name = new char[token ? strlen( token ) + 1 : 1];
			if ( token )
				strcpy( phrase->m_name, token );
			else
				phrase->m_name[0] = '\0';
		}

		phrase->InitVoiceBank( voiceBank );
		PlaceCriteria place = ANY_PLACE;
		CountCriteria count = ANY_COUNT;
		GameEventType radioEvent = EVENT_INVALID;
		bool isImportant = false;
		for (;;)
		{
			data = MP_COM_Parse( data );
			if ( data == NULL )
			{
				CONSOLE_ECHO( "Error parsing %s - expected 'End'\n", filename );
				g_engfuncs.pfnFreeFile( file );
				return false;
			}
			token = MP_COM_GetToken();
			if ( token == NULL )
			{
				g_engfuncs.pfnFreeFile( file );
				return false;
			}
			if ( stricmp( token, "End" ) == 0 )
				break;
			if ( stricmp( token, "Important" ) == 0 )
			{
				isImportant = true;
				continue;
			}

			if ( stricmp( token, "Place" ) == 0 || stricmp( token, "Count" ) == 0 || stricmp( token, "Radio" ) == 0 )
			{
				bool isPlaceCriterion = stricmp( token, "Place" ) == 0;
				bool isCountCriterion = stricmp( token, "Count" ) == 0;
				data = MP_COM_Parse( data );
				if ( data == NULL )
				{
					if ( isPlaceCriterion )
						CONSOLE_ECHO( "Error parsing %s - expected Place name\n", filename );
					else if ( isCountCriterion )
						CONSOLE_ECHO( "Error parsing %s - expected Count value\n", filename );
					else
						CONSOLE_ECHO( "Error parsing %s - expected radio event\n", filename );
					g_engfuncs.pfnFreeFile( file );
					return false;
				}
				token = MP_COM_GetToken();
				if ( isPlaceCriterion )
				{
					if ( stricmp( token, "ANY" ) == 0 )
						place = ANY_PLACE;
					else if ( stricmp( token, "UNDEFINED" ) == 0 )
						place = UNDEFINED_PLACE;
					else
						place = TheBotPhrases->NameToID( token );
				}
				else if ( isCountCriterion )
					count = stricmp( token, "Many" ) == 0 ? MANY_ENEMIES : atoi( token );
				else
				{
					radioEvent = NameToGameEvent( token );
					if ( radioEvent < EVENT_RADIO_COVER_ME || radioEvent > EVENT_RADIO_ENEMY_DOWN )
					{
						CONSOLE_ECHO( "Error parsing %s - invalid radio event '%s'\n", filename, token );
						g_engfuncs.pfnFreeFile( file );
						return false;
					}
				}
				continue;
			}

			BotSpeakable *speakable = new BotSpeakable;
			char path[128];
			const char *name = token;
			if ( baseDirectory[0] )
			{
				snprintf( path, sizeof(path), "%s%s", baseDirectory, token );
				name = path;
			}
			speakable->m_file = new char[name ? strlen( name ) + 1 : 1];
			if ( name )
				strcpy( speakable->m_file, name );
			else
				speakable->m_file[0] = '\0';
			speakable->m_place = place;
			speakable->m_count = count;
			snprintf( path, sizeof(path), "sound\\%s", speakable->m_file );
			speakable->m_duration = static_cast<float>(g_engfuncs.pfnGetApproxWavePlayLen( path ) / 1000.0);
			if ( speakable->m_duration <= 0.0f )
			{
				CONSOLE_ECHO( "Warning: Couldn't get duration of phrase '%s'\n", path );
				speakable->m_duration = 1.0f;
			}
			phrase->m_voiceBank[voiceBank]->push_back( speakable );
			++phrase->m_count[voiceBank];
		}
		if ( voiceBank == 0 )
		{
			phrase->m_radioEvent = radioEvent;
			phrase->m_isImportant = isImportant;
		}
		if ( isPlace )
			m_placeList.push_back( phrase );
		else
			m_list.push_back( phrase );
	}
	g_engfuncs.pfnFreeFile( file );
	return true;
}

void BotPhraseManager::OnMapChange( void )
{
	m_placeCount = 0;
}

void BotPhraseManager::OnRoundRestart( void )
{
	m_placeCount = 0;
	for ( BotPhraseList::iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
		(*it)->Randomize();
	for ( BotPhraseList::iterator it = m_list.begin(); it != m_list.end(); ++it )
		(*it)->Randomize();
}

unsigned int BotPhraseManager::NameToID( const char *name ) const
{
	for ( BotPhraseList::const_iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
	{
		if ( stricmp( (*it)->GetName(), name ) == 0 )
			return (*it)->GetID();
	}
	for ( BotPhraseList::const_iterator it = m_list.begin(); it != m_list.end(); ++it )
	{
		if ( stricmp( (*it)->GetName(), name ) == 0 )
			return (*it)->GetID();
	}
	return 0;
}

const char *BotPhraseManager::IDToName( unsigned int id ) const
{
	for ( BotPhraseList::const_iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
	{
		if ( (*it)->GetID() == id )
			return (*it)->GetName();
	}
	for ( BotPhraseList::const_iterator it = m_list.begin(); it != m_list.end(); ++it )
	{
		if ( (*it)->GetID() == id )
			return (*it)->GetName();
	}
	return NULL;
}

const BotPhrase *BotPhraseManager::GetPhrase( const char *name ) const
{
	for ( BotPhraseList::const_iterator it = m_list.begin(); it != m_list.end(); ++it )
	{
		if ( stricmp( (*it)->GetName(), name ) == 0 )
			return *it;
	}
	return NULL;
}

const BotPhrase *BotPhraseManager::GetPlace( const char *name ) const
{
	if ( name == NULL )
		return NULL;
	for ( BotPhraseList::const_iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
	{
		if ( stricmp( (*it)->GetName(), name ) == 0 )
			return *it;
	}
	return NULL;
}

const BotPhrase *BotPhraseManager::GetPlace( unsigned int id ) const
{
	if ( id == 0 )
		return NULL;
	for ( BotPhraseList::const_iterator it = m_placeList.begin(); it != m_placeList.end(); ++it )
	{
		if ( (*it)->GetID() == id )
			return *it;
	}
	return NULL;
}

int BotPhraseManager::FindPlaceIndex( Place place ) const
{
	for ( int i = 0; i < m_placeCount; ++i )
	{
		if ( m_placeStatementHistory[i].placeID == place )
			return i;
	}
	if ( m_placeCount >= 64 )
		return -1;

	int index = m_placeCount;
	m_placeStatementHistory[index].placeID = place;
	m_placeStatementHistory[index].timer.Invalidate();
	++m_placeCount;
	return index;
}

float BotPhraseManager::GetPlaceStatementInterval( Place place ) const
{
	int index = FindPlaceIndex( place );
	if ( index < 0 || index >= m_placeCount )
		return 99999.9f;
	return m_placeStatementHistory[index].timer.GetElapsedTime();
}

void BotPhraseManager::ResetPlaceStatementInterval( Place place )
{
	int index = FindPlaceIndex( place );
	if ( index >= 0 && index < m_placeCount )
		m_placeStatementHistory[index].timer.Start();
}

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
BotStatement::BotStatement( BotChatterInterface *owner, BotStatementType type, float lifetime )
{
	m_owner = owner;
	m_next = NULL;
	m_prev = NULL;
	m_type = type;
	m_subjectID = -1;
	m_place = UNDEFINED_PLACE;
	m_meme = NULL;
	m_creationTimestamp = gpGlobals->time;
	m_startTime = gpGlobals->time;
	m_expireTime = gpGlobals->time + lifetime;
	m_speakingStartTime = 0.0f;
	m_isSpeaking = false;
	m_nextPhraseTime = 0.0f;
	m_currentPhrase = -1;
	m_phraseCount = 0;
	m_conditionCount = 0;
}

BotStatement::~BotStatement()
{
	if ( m_meme )
		::operator delete( m_meme );
}

void BotStatement::AppendPhrase( const BotPhrase *phrase )
{
	if ( phrase && m_phraseCount < 4 )
	{
		m_phrase[m_phraseCount].isPhrase = true;
		m_phrase[m_phraseCount].phrase = phrase;
		++m_phraseCount;
	}
}

void BotStatement::AppendPhrase( ContextType context )
{
	if ( m_phraseCount < 4 )
	{
		m_phrase[m_phraseCount].isPhrase = false;
		m_phrase[m_phraseCount].context = context;
		++m_phraseCount;
	}
}

void BotStatement::AddCondition( ConditionType condition )
{
	if ( m_conditionCount < 4 )
		m_condition[m_conditionCount++] = condition;
}

void BotStatement::AttachMeme( BotMeme *meme )
{
	m_meme = meme;
}

CCSBot *BotStatement::GetOwner( void ) const
{
	return m_owner->GetOwner();
}

Place BotStatement::GetPlace( void ) const
{
	if ( m_place )
		return m_place;
	for ( int i = 0; i < m_phraseCount; ++i )
	{
		if ( m_phrase[i].isPhrase && m_phrase[i].phrase->IsPlace() )
			return m_phrase[i].phrase->GetID();
	}
	return UNDEFINED_PLACE;
}

bool BotStatement::IsImportant( void ) const
{
	for ( int i = 0; i < m_phraseCount; ++i )
	{
		if ( m_phrase[i].isPhrase )
		{
			if ( m_phrase[i].phrase->IsImportant() )
				return true;
		}
		else if ( m_phrase[i].context == CURRENT_ENEMY_COUNT )
			return true;
	}
	return false;
}

bool BotStatement::HasCount( void ) const
{
	for ( int i = 0; i < m_phraseCount; ++i )
	{
		if ( !m_phrase[i].isPhrase && m_phrase[i].context == CURRENT_ENEMY_COUNT )
			return true;
	}
	return false;
}

bool BotStatement::IsRedundant( const BotStatement *statement ) const
{
	if ( m_type == BOT_STATEMENT_CRITICAL || m_type == BOT_STATEMENT_HELP ||
		m_type == BOT_STATEMENT_PLAN || m_type == BOT_STATEMENT_RESPONSE )
		return false;
	if ( m_type != statement->m_type )
		return false;
	Place place = GetPlace();
	Place otherPlace = statement->GetPlace();
	if ( place && otherPlace && place == otherPlace )
		return true;
	if ( !place && !otherPlace && m_subjectID == -1 && statement->m_subjectID == -1 )
		return true;
	return m_subjectID != -1 && statement->m_subjectID != -1 && m_subjectID == statement->m_subjectID;
}

void BotStatement::Convert( const BotStatement *statement )
{
	if ( m_type == BOT_STATEMENT_PLAN && statement->m_type == BOT_STATEMENT_PLAN )
	{
		static const BotPhrase *agree = TheBotPhrases->GetPhrase( "AgreeWithPlan" );
		if ( m_phrase[0].phrase != agree && m_phrase[0].phrase == statement->m_phrase[0].phrase )
		{
			float time = gpGlobals->time;
			if ( m_place == statement->m_place )
			{
				m_phrase[0].phrase = agree;
				m_startTime = time + RANDOM_FLOAT( 0.5f, 1.0f );
			}
			else
				m_startTime = time + RANDOM_FLOAT( 3.0f, 4.0f );
		}
	}
}

void BotChatterInterface::RemoveStatement( BotStatement *statement )
{
	if ( statement->m_next )
		statement->m_next->m_prev = statement->m_prev;
	if ( statement->m_prev )
		statement->m_prev->m_next = statement->m_next;
	else
		m_statementList = statement->m_next;
	delete statement;
}

void BotChatterInterface::Affirmative( void )
{
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_RESPONSE, 3.0f );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "Affirmative" ) );
	AddStatement( statement );
}

void BotChatterInterface::Negative( void )
{
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_RESPONSE, 3.0f );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "Negative" ) );
	AddStatement( statement );
}

void BotChatterInterface::Clear( Place place )
{
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 10.0f );
	statement->AppendPhrase( TheBotPhrases->GetPlace( place ) );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "Clear" ) );
	AddStatement( statement );
}

void BotChatterInterface::Scared( void )
{
	if ( m_scaredTimer.IsLessThen( 10.0f ) )
		return;
	m_scaredTimer.Start();
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_EMOTE, 1.0f );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "ScaredEmote" ) );
	statement->AddCondition( BotStatement::IS_ATTACKING );
	AddStatement( statement );
}

void BotChatterInterface::ReportIn( void )
{
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_REQUEST, 10.0f );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "RequestReport" ) );
	statement->AddCondition( BotStatement::RADIO_SILENCE );
	statement->AttachMeme( new BotRequestReportMeme );
	AddStatement( statement );
}

void BotChatterInterface::RequestBombLocation( void )
{
	if ( m_hasRequestedBombLocation )
		return;
	m_hasRequestedBombLocation = true;
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_REQUEST, 10.0f );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "WhereIsTheBomb" ) );
	statement->AttachMeme( new BotWhereBombMeme );
	AddStatement( statement );
}

void BotChatterInterface::Say( const char *phrase, float lifetime, float delay )
{
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_SAY, lifetime );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( phrase ) );
	if ( delay > 0.0f )
		statement->SetStartTime( gpGlobals->time + delay );
	AddStatement( statement );
}

void BotChatterInterface::Encourage( const char *phrase, float interval, float lifetime )
{
	if ( m_encourageTimer.IsElapsed() )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_SAY, lifetime );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( phrase ) );
		AddStatement( statement );
		m_encourageTimer.Start( interval );
	}
}

void BotChatterInterface::TalkingToHostages( void )
{
}

void BotChatterInterface::OnEvent( GameEventType, CBaseEntity *, CBaseEntity * )
{
}

void BotChatterInterface::FriendlyFire( void )
{
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_FRIENDLY_FIRE, 1.0f );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "FriendlyFire" ) );
	float time = gpGlobals->time;
	statement->SetStartTime( time + RANDOM_FLOAT( 0.3f, 0.5f ) );
	AddStatement( statement );
}

void BotChatterInterface::KilledFriend( void )
{
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_KILLED_FRIEND, 2.0f );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "KilledFriend" ) );
	float time = gpGlobals->time;
	statement->SetStartTime( time + RANDOM_FLOAT( 0.5f, 1.0f ) );
	AddStatement( statement );
}

const Vector *GetRandomSpotAtPlace( Place place )
{
	int count = 0;
	for ( NavAreaList::const_iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it )
	{
		if ( (*it)->GetPlace() == place )
			++count;
	}
	if ( count )
	{
		int selection = RANDOM_LONG( 0, count - 1 );
		for ( NavAreaList::const_iterator it = TheNavAreaList.begin(); it != TheNavAreaList.end(); ++it )
		{
			if ( (*it)->GetPlace() == place && selection == 0 )
				return (*it)->GetCenter();
		}
	}
	return NULL;
}

void BotChatterInterface::HeardNoise( const Vector *position )
{
	if ( !TheCSBots()->IsRoundOver() && m_noiseTimer.IsElapsed() )
	{
		m_noiseTimer.Start( 20.0f );
		if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 33.0f )
		{
			BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 5.0f );
			statement->AppendPhrase( TheBotPhrases->GetPhrase( "HeardNoise" ) );
			statement->SetPlace( TheNavAreaGrid.GetPlace( position ) );
			AddStatement( statement );
		}
	}
}

void BotChatterInterface::HostageDown( void )
{
	if ( !TheCSBots()->IsRoundOver() )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 3.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "HostageDown" ) );
		AddStatement( statement );
	}
}

void BotChatterInterface::HostagesTaken( void )
{
	if ( !TheCSBots()->IsRoundOver() )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 3.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "HostagesTaken" ) );
		AddStatement( statement );
	}
}

void BotChatterInterface::HostagesBeingTaken( void )
{
	if ( !TheCSBots()->IsRoundOver() )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 3.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "HostagesBeingTaken" ) );
		statement->AttachMeme( new BotHostageBeingTakenMeme );
		AddStatement( statement );
	}
}

void BotChatterInterface::AnnouncePlan( const char *phrase, Place place )
{
	if ( !TheCSBots()->IsRoundOver() )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_PLAN, 10.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( phrase ) );
		statement->SetPlace( place );
		float roundStart = TheCSBots()->GetRoundStartTimestamp();
		statement->SetStartTime( roundStart + RANDOM_FLOAT( 2.0f, 3.0f ) );
		AddStatement( statement );
	}
}

void BotChatterInterface::GuardingHostages( Place place, bool isPlan )
{
	if ( TheCSBots()->IsRoundOver() || m_guardTimer.IsLessThen( 20.0f ) )
		return;
	if ( isPlan )
		AnnouncePlan( "GoingToGuardHostages", place );
	else
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_SAY, 3.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "GuardingHostages" ) );
		AddStatement( statement );
	}
}

void BotChatterInterface::GuardingHostageEscapeZone( bool isPlan )
{
	if ( TheCSBots()->IsRoundOver() || m_guardTimer.IsLessThen( 20.0f ) )
		return;
	if ( isPlan )
		AnnouncePlan( "GoingToGuardHostageEscapeZone", UNDEFINED_PLACE );
	else
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_SAY, 3.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "GuardingHostageEscapeZone" ) );
		AddStatement( statement );
	}
}

void BotChatterInterface::GoingToPlantTheBomb( Place place )
{
	if ( TheCSBots()->IsRoundOver() || m_guardTimer.IsLessThen( 10.0f ) )
		return;
	m_guardTimer.Start();
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_CRITICAL, 10.0f );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "GoingToPlantBomb" ) );
	statement->SetPlace( place );
	statement->AttachMeme( new BotFollowMeme );
	AddStatement( statement );
}

void BotChatterInterface::EscortingHostages( void )
{
	if ( TheCSBots()->IsRoundOver() || !m_escortingTimer.IsElapsed() )
		return;
	m_escortingTimer.Start( 10.0f );
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_PLAN, 5.0f );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "EscortingHostages" ) );
	AddStatement( statement );
}

void BotChatterInterface::BombsiteClear( int bombsite )
{
	const CCSBotManager::Zone *zone = TheCSBots()->GetZone( bombsite );
	if ( zone )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 10.0f );
		statement->AppendPhrase( TheBotPhrases->GetPlace( TheNavAreaGrid.GetPlace( &zone->m_center ) ) );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "BombsiteClear" ) );
		statement->AttachMeme( new BotBombsiteStatusMeme( bombsite, 0 ) );
		AddStatement( statement );
	}
}

void BotChatterInterface::FoundPlantedBomb( int bombsite )
{
	const CCSBotManager::Zone *zone = TheCSBots()->GetZone( bombsite );
	if ( zone )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 3.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "PlantedBombPlace" ) );
		statement->SetPlace( TheNavAreaGrid.GetPlace( &zone->m_center ) );
		statement->AttachMeme( new BotBombsiteStatusMeme( bombsite, 1 ) );
		AddStatement( statement );
	}
}

BotChatterInterface::BotChatterInterface( CCSBot *bot )
{
	m_bot = bot;
	m_statementList = NULL;
	static int nextPitch = 0;
	switch ( nextPitch )
	{
	case 0:
		m_pitch = RANDOM_LONG( 105, 110 );
		break;
	case 1:
		m_pitch = RANDOM_LONG( 95, 105 );
		break;
	case 2:
		m_pitch = RANDOM_LONG( 85, 95 );
		break;
	}
	nextPitch = (nextPitch + 1) % 3;
	Reset();
}

BotChatterInterface::~BotChatterInterface()
{
	BotStatement* statement = m_statementList;
	while ( statement )
	{
		BotStatement* next = statement->m_next;
		delete statement;
		statement = next;
	}
}

void BotChatterInterface::Reset( void )
{
	BotStatement *statement = m_statementList;
	while ( statement )
	{
		BotStatement *next = statement->m_next;
		if ( statement->m_type != BOT_STATEMENT_PERSISTENT )
			RemoveStatement( statement );
		statement = next;
	}
	m_hasSeenEnemy = false;
	m_firstSeenEnemyTimestamp = 0.0f;
	m_hasReportedEnemies = false;
	m_hasRequestedBombLocation = false;
	m_encourageTimer.Invalidate();
	m_needBackupTimer.Invalidate();
	m_statementTimer.Invalidate();
	m_looseBombTimer.Invalidate();
	m_noiseTimer.Invalidate();
	m_scaredTimer.Invalidate();
	m_guardTimer.Invalidate();
	m_escortingTimer.Invalidate();
	ResetRadioSilenceDuration();
}

float BotChatterInterface::GetRadioSilenceDuration( void )
{
	if ( m_bot->m_iTeam != TERRORIST && m_bot->m_iTeam != CT )
		return 99999.9f;

	return m_radioSilenceInterval[m_bot->m_iTeam - TERRORIST].GetElapsedTime();
}

void BotChatterInterface::ResetRadioSilenceDuration( void )
{
	if ( m_bot->m_iTeam != TERRORIST && m_bot->m_iTeam != CT )
		return;

	m_radioSilenceInterval[m_bot->m_iTeam - TERRORIST].Start();
}

bool BotChatterInterface::ShouldSpeak( void ) const
{
	if ( m_bot->GetFriendsRemaining() == 0 )
		return false;
	return std::min( m_bot->GetNearbyFriendCount(), m_bot->GetFriendsRemaining() ) != m_bot->GetFriendsRemaining();
}

bool BotStatement::IsObsolete( void ) const
{
	if ( GetOwner()->GetGameState()->IsRoundOver() && m_type != BOT_STATEMENT_EMOTE )
		return true;
	return gpGlobals->time > m_expireTime;
}

bool BotStatement::IsValid( void ) const
{
	for ( int i = 0; i < m_conditionCount; ++i )
	{
		if ( m_condition[i] == IS_ATTACKING && !GetOwner()->IsAttacking() )
			return false;
		if ( m_condition[i] == ENEMIES_REMAINING && GetOwner()->GetEnemiesRemaining() == 0 )
			return false;
	}
	return true;
}

bool BotChatterInterface::NeedBackup( void )
{
	if ( m_needBackupTimer.IsLessThen( 10.0f ) )
		return false;
	m_needBackupTimer.Start();
	if ( m_bot->GetFriendsRemaining() )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_HELP, 10.0f );
		Place place = m_bot->GetPlace();
		statement->AppendPhrase( TheBotPhrases->GetPlace( place ) );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "Help" ) );
		statement->AttachMeme( new BotHelpMeme( place ) );
		AddStatement( statement );
	}
	else
		Scared();
	return true;
}

void BotChatterInterface::PinnedDown( void )
{
	if ( m_needBackupTimer.IsLessThen( 10.0f ) )
		return;
	m_needBackupTimer.Start();
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_HELP, 10.0f );
	Place place = m_bot->GetPlace();
	statement->AppendPhrase( TheBotPhrases->GetPlace( place ) );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "PinnedDown" ) );
	statement->AttachMeme( new BotHelpMeme( place ) );
	statement->AddCondition( BotStatement::IS_ATTACKING );
	AddStatement( statement );
}

void BotChatterInterface::EnemySpotted( void )
{
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_ENEMY_SPOTTED, 10.0f );
	statement->AppendPhrase( TheBotPhrases->GetPlace( m_bot->GetLastEnemyPlace() ) );
	statement->AppendPhrase( BotStatement::ACCUMULATE_ENEMIES );
	statement->AppendPhrase( BotStatement::CURRENT_ENEMY_COUNT );
	statement->AddCondition( BotStatement::IS_ATTACKING );
	AddStatement( statement );
}

void BotChatterInterface::KilledMyEnemy( int enemyID )
{
	if ( std::min( m_bot->GetNearbyEnemyCount(), m_bot->GetEnemiesRemaining() ) > 1 )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_KILLED_ENEMY, 3.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "KilledMyEnemy" ) );
		statement->SetSubject( enemyID );
		AddStatement( statement );
	}
}

void BotChatterInterface::EnemiesRemaining( void )
{
	if ( std::min( m_bot->GetNearbyEnemyCount(), m_bot->GetEnemiesRemaining() ) <= 1 )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_REMAINING_COUNT, 5.0f );
		statement->AppendPhrase( BotStatement::REMAINING_ENEMY_COUNT );
		float time = gpGlobals->time;
		statement->SetStartTime( time + RANDOM_FLOAT( 2.0f, 4.0f ) );
		AddStatement( statement );
	}
}

void BotChatterInterface::ReportEnemies( void )
{
	if ( !m_bot->IsAlive() )
		return;
	int count = std::min( m_bot->GetNearbyEnemyCount(), m_bot->GetEnemiesRemaining() );
	if ( count )
	{
		if ( !m_hasSeenEnemy )
		{
			m_hasSeenEnemy = true;
			m_firstSeenEnemyTimestamp = gpGlobals->time;
		}
		if ( !m_hasReportedEnemies )
		{
			if ( !m_bot->IsOutnumbered() || !NeedBackup() )
				m_bot->GetChatter()->EnemySpotted();
			m_hasReportedEnemies = true;
		}
	}
	else
	{
		m_hasSeenEnemy = false;
		m_hasReportedEnemies = false;
	}
}

void BotChatterInterface::AddStatement( BotStatement *statement, bool mustSpeak )
{
	if ( !statement )
		return;
	if ( cv_bot_chatter.string )
	{
		int mode = toupper( static_cast<unsigned char>(cv_bot_chatter.string[0]) );
		if ( mode == 'O' || (mode == 'M' && !statement->IsImportant()) )
		{
			delete statement;
			return;
		}
	}
	if ( !m_bot->IsAlive() && !mustSpeak )
	{
		delete statement;
		return;
	}
	if ( statement->m_phraseCount == 0 )
	{
		delete statement;
		return;
	}
	for ( BotStatement *other = m_statementList; other; other = other->m_next )
	{
		if ( statement->IsRedundant( other ) )
		{
			m_bot->PrintIfWatched( "I tried to say something I'm already saying.\n" );
			delete statement;
			return;
		}
	}

	BotStatement *prev = NULL;
	BotStatement *next = m_statementList;
	while ( next && next->m_startTime <= statement->m_startTime )
	{
		prev = next;
		next = next->m_next;
	}
	statement->m_next = next;
	statement->m_prev = prev;
	if ( next )
		next->m_prev = statement;
	if ( prev )
		prev->m_next = statement;
	else
		m_statementList = statement;
}

BotStatement *BotChatterInterface::GetActiveStatement( void )
{
	BotStatement *active = NULL;
	float timestamp = 1000000000.0f;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>(UTIL_PlayerByIndex( i ));
		if ( player == NULL || FNullEnt( player->pev ) || STRING( player->pev->netname )[0] == '\0' )
			continue;
		if ( !player->IsBot() && !player->IsAlive() )
			continue;
		if ( player->m_iTeam != m_bot->m_iTeam || !player->IsBot() )
			continue;
		CCSBot *bot = static_cast<CCSBot *>(player);
		for ( BotStatement *statement = bot->GetChatter()->m_statementList; statement; statement = statement->m_next )
		{
			if ( statement->m_isSpeaking )
				return statement;
			if ( statement->m_startTime < timestamp )
			{
				active = statement;
				timestamp = statement->m_creationTimestamp;
			}
		}
	}
	if ( active && active->m_startTime <= gpGlobals->time )
		return active;
	return NULL;
}

void BotChatterInterface::Update( void )
{
	ReportEnemies();
	if ( ShouldSpeak() && m_bot->GetEnemiesRemaining() > 0 && GetRadioSilenceDuration() > 30.0f )
		ReportIn();
	BotStatement *active = GetActiveStatement();
	if ( active && active->GetOwner() == m_bot && !active->Update() )
		RemoveStatement( active );
	active = GetActiveStatement();
	if ( active && active->GetOwner() == m_bot )
		active = NULL;
	BotStatement *statement = m_statementList;
	while ( statement )
	{
		BotStatement *next = statement->m_next;
		if ( !statement->IsValid() )
			RemoveStatement( statement );
		else if ( !statement->m_isSpeaking )
		{
			if ( statement->IsObsolete() )
			{
				m_bot->PrintIfWatched( "Statement obsolete - removing.\n" );
				RemoveStatement( statement );
			}
			else if ( active )
			{
				if ( statement->m_type == BOT_STATEMENT_PLAN && active->m_type == BOT_STATEMENT_PLAN )
					statement->Convert( active );
				if ( statement->IsRedundant( active ) )
				{
					m_bot->PrintIfWatched( "Teammate said what I was going to say - shutting up.\n" );
					RemoveStatement( statement );
				}
			}
		}
		statement = next;
	}
}

void BotMeme::Transmit( CCSBot *sender ) const
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *player = static_cast<CBasePlayer *>(UTIL_PlayerByIndex( i ));
		if ( player == NULL || FNullEnt( player->pev ) || STRING( player->pev->netname )[0] == '\0' || player == sender )
			continue;
		if ( !player->IsBot() && !player->IsAlive() )
			continue;
		if ( player->m_iTeam == sender->m_iTeam && player->IsBot() )
			Interpret( sender, static_cast<CCSBot *>(player) );
	}
}

void BotHelpMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	receiver->RespondToHelpRequest( sender, m_place, 3000.0f );
}

void BotWhereBombMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	int bombsite = receiver->GetGameState()->GetPlantedBombsite();
	if ( bombsite != -1 )
		receiver->GetChatter()->FoundPlantedBomb( bombsite );
}

void BotRequestReportMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	receiver->GetChatter()->ReportingIn();
}

void BotBombsiteStatusMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	if ( m_state == 0 )
		receiver->GetGameState()->ClearBombsite( m_bombsite );
	else
		receiver->GetGameState()->MarkBombsiteAsPlanted( m_bombsite );
	if ( receiver->GetTask() == CCSBot::TASK_FIND_TICKING_BOMB )
	{
		receiver->Idle();
		receiver->GetChatter()->Affirmative();
	}
}

void BotAllHostagesGoneMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	receiver->GetGameState()->AllHostagesGone();
	BotStatement *statement = new BotStatement( receiver->GetChatter(), BOT_STATEMENT_SAY, 3.0f );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "Affirmative" ) );
	receiver->GetChatter()->AddStatement( statement );
}

void BotHostageBeingTakenMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	receiver->GetGameState()->SetHostageTaken( true );
	if ( !receiver->IsBusy() )
	{
		receiver->Idle();
		BotStatement *statement = new BotStatement( receiver->GetChatter(), BOT_STATEMENT_SAY, 3.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "Affirmative" ) );
		receiver->GetChatter()->AddStatement( statement );
	}
}

void BotBombStatusMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	if ( m_state == CSGameState::BOMB_LOOSE )
	{
		receiver->GetGameState()->UpdateLooseBomb( &m_position );
		if ( receiver->GetTask() == CCSBot::TASK_FETCH_BOMB )
		{
			receiver->Idle();
			receiver->GetChatter()->Affirmative();
		}
	}
	else if ( m_state == CSGameState::BOMB_CARRIED )
	{
		receiver->GetGameState()->UpdateBomber( &m_position );
		if ( !receiver->IsRogue() && receiver->IsHunting() &&
			std::min( receiver->GetNearbyEnemyCount(), receiver->GetEnemiesRemaining() ) == 0 )
			receiver->RespondToHelpRequest( sender, TheNavAreaGrid.GetPlace( &m_position ), -1.0f );
	}
}

void BotChatterInterface::PlantingTheBomb( Place place )
{
	if ( !TheCSBots()->IsRoundOver() )
	{
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_CRITICAL, 10.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "PlantingBomb" ) );
		statement->SetPlace( place );
		statement->AttachMeme( new BotDefendHereMeme( &m_bot->pev->origin ) );
		AddStatement( statement );
	}
}

void BotChatterInterface::TheyPickedUpTheBomb( void )
{
	if ( !TheCSBots()->IsRoundOver() && m_bot->GetGameState()->GetBombState() == CSGameState::BOMB_LOOSE )
	{
		m_bot->GetGameState()->UpdateBomber( &m_bot->pev->origin );
		BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 10.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "TheyPickedUpTheBomb" ) );
		statement->AttachMeme( new BotBombStatusMeme( CSGameState::BOMB_CARRIED, &m_bot->pev->origin ) );
		AddStatement( statement );
	}
}

void BotChatterInterface::SpottedBomber( CBasePlayer *bomber )
{
	if ( m_bot->GetGameState()->GetBombState() == CSGameState::BOMB_CARRIED )
	{
		const Vector *position = m_bot->GetGameState()->GetBombPosition();
		if ( position && (bomber->pev->origin - *position).LengthSquared() < 1000000.0f )
			return;
	}
	m_bot->GetGameState()->UpdateBomber( &bomber->pev->origin );
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 10.0f );
	Place place = TheNavAreaGrid.GetPlace( &bomber->pev->origin );
	statement->AppendPhrase( TheBotPhrases->GetPlace( place ) );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "SpottedBomber" ) );
	statement->SetSubject( bomber->entindex() );
	statement->AttachMeme( new BotBombStatusMeme( CSGameState::BOMB_CARRIED, &bomber->pev->origin ) );
	AddStatement( statement );
}

void BotChatterInterface::SpottedLooseBomb( CBaseEntity *bomb )
{
	if ( TheCSBots()->IsRoundOver() || m_bot->GetGameState()->GetBombState() == CSGameState::BOMB_LOOSE )
		return;
	m_bot->GetGameState()->UpdateLooseBomb( &bomb->pev->origin );
	if ( !m_looseBombTimer.IsElapsed() )
		return;
	m_looseBombTimer.Start( 10.0f );
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 10.0f );
	statement->AppendPhrase( TheBotPhrases->GetPlace( TheNavAreaGrid.GetPlace( &bomb->pev->origin ) ) );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "SpottedLooseBomb" ) );
	if ( TheCSBots()->GetLooseBomb() )
		statement->AttachMeme( new BotBombStatusMeme( CSGameState::BOMB_LOOSE, &bomb->pev->origin ) );
	AddStatement( statement );
}

void BotChatterInterface::GuardingLooseBomb( CBaseEntity *bomb )
{
	if ( TheCSBots()->IsRoundOver() )
		return;
	m_bot->GetGameState()->UpdateLooseBomb( &bomb->pev->origin );
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 10.0f );
	statement->AppendPhrase( TheBotPhrases->GetPlace( TheNavAreaGrid.GetPlace( &bomb->pev->origin ) ) );
	statement->AppendPhrase( TheBotPhrases->GetPhrase( "GuardingLooseBomb" ) );
	if ( TheCSBots()->GetLooseBomb() )
		statement->AttachMeme( new BotBombStatusMeme( CSGameState::BOMB_LOOSE, &bomb->pev->origin ) );
	AddStatement( statement );
}

void BotChatterInterface::CelebrateWin( void )
{
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_EMOTE, 15.0f );
	float time = gpGlobals->time;
	statement->SetStartTime( time + RANDOM_FLOAT( 2.0f, 5.0f ) );
	float roundTime = TheCSBots()->GetElapsedRoundTime();
	const char *phrase = NULL;
	if ( m_bot->GetFriendsRemaining() )
	{
		if ( roundTime >= 45.0f )
		{
			if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 10.0f )
				phrase = "WonRound";
		}
		else if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 33.3f )
			phrase = "WonRoundQuickly";
	}
	else if ( roundTime >= 45.0f )
	{
		if ( RANDOM_FLOAT( 0.0f, 100.0f ) < 33.3f )
			phrase = "LastManStanding";
	}
	else
		phrase = "WonRoundQuickly";
	if ( phrase )
		statement->AppendPhrase( TheBotPhrases->GetPhrase( phrase ) );
	AddStatement( statement );
}

void BotChatterInterface::OnDeath( void )
{
	if ( m_statementList && m_statementList->m_isSpeaking )
	{
		bool canSpeak = true;
		if ( cv_bot_chatter.string )
		{
			int mode = toupper( static_cast<unsigned char>(cv_bot_chatter.string[0]) );
			canSpeak = mode != 'R' && mode != 'O';
		}
		if ( canSpeak )
		{
			static const BotPhrase *pain = TheBotPhrases->GetPhrase( "pain" );
			if ( pain )
			{
				const BotProfile *profile = m_bot->GetProfile();
				m_bot->Radio( pain->GetSpeakable( profile->GetVoiceBank(), NULL ), NULL, static_cast<short>(profile->GetVoicePitch()), true );
				m_bot->GetChatter()->ResetRadioSilenceDuration();
			}
		}
	}
	Reset();
}

bool BotStatement::Update( void )
{
	CCSBot *bot = GetOwner();
	if ( bot->GetFriendsRemaining() == 0 && m_type != BOT_STATEMENT_EMOTE )
		return false;
	if ( !m_isSpeaking )
	{
		m_isSpeaking = true;
		m_speakingStartTime = gpGlobals->time;
	}
	if ( m_currentPhrase >= 0 && m_phrase[m_currentPhrase].context == ACCUMULATE_ENEMIES )
	{
		int count = std::min( bot->GetNearbyEnemyCount(), bot->GetEnemiesRemaining() );
		if ( count > 3 || gpGlobals->time - m_speakingStartTime > 2.0f )
			m_nextPhraseTime = 0.0f;
	}
	if ( gpGlobals->time <= m_nextPhraseTime )
		return true;
	if ( ++m_currentPhrase == m_phraseCount )
	{
		if ( m_meme )
			m_meme->Transmit( bot );
		return false;
	}

	const BotPhrase *phrase = NULL;
	if ( m_phrase[m_currentPhrase].isPhrase )
		phrase = m_phrase[m_currentPhrase].phrase;
	else
	{
		switch ( m_phrase[m_currentPhrase].context )
		{
		case CURRENT_ENEMY_COUNT:
		{
			int enemies = std::min( bot->GetNearbyEnemyCount(), bot->GetEnemiesRemaining() );
			int friends = std::min( bot->GetNearbyFriendCount(), bot->GetFriendsRemaining() );
			if ( enemies - 1 > friends )
			{
				phrase = TheBotPhrases->GetPhrase( "Help" );
				m_meme = new BotHelpMeme( UNDEFINED_PLACE );
			}
			else if ( enemies > 1 )
			{
				phrase = TheBotPhrases->GetPhrase( "EnemySpotted" );
				phrase->SetCountCriteria( enemies );
			}
			break;
		}
		case REMAINING_ENEMY_COUNT:
		{
			unsigned int remaining = bot->GetEnemiesRemaining();
			if ( remaining <= 3 )
			{
				static const char *speak[] = { "NoEnemiesLeft", "OneEnemyLeft", "TwoEnemiesLeft", "ThreeEnemiesLeft" };
				phrase = TheBotPhrases->GetPhrase( speak[remaining] );
			}
			break;
		}
		case SHORT_DELAY:
		{
			float time = gpGlobals->time;
			m_nextPhraseTime = time + RANDOM_FLOAT( 0.1f, 0.5f );
			return true;
		}
		case LONG_DELAY:
		{
			float time = gpGlobals->time;
			m_nextPhraseTime = time + RANDOM_FLOAT( 1.0f, 2.0f );
			return true;
		}
		case ACCUMULATE_ENEMIES:
			m_nextPhraseTime = 100000000.0f;
			return true;
		}
	}
	if ( phrase == NULL )
	{
		m_nextPhraseTime = 0.0f;
		return true;
	}

	float duration;
	if ( cv_bot_chatter.string && toupper( static_cast<unsigned char>(cv_bot_chatter.string[0]) ) == 'R' )
	{
		if ( phrase->GetRadioEquivalent() != EVENT_INVALID )
		{
			bot->GetChatter()->ResetRadioSilenceDuration();
			bot->SendRadioMessage( phrase->GetRadioEquivalent() );
			duration = 2.0f;
		}
		else
		{
			m_nextPhraseTime = 0.0f;
			return true;
		}
	}
	else
	{
		phrase->SetPlaceCriteria( m_place );
		const BotProfile *profile = bot->GetProfile();
		const char *speakable = phrase->GetSpeakable( profile->GetVoiceBank(), &duration );
		if ( phrase->IsPlace() )
		{
			if ( TheBotPhrases->GetPlaceStatementInterval( phrase->GetID() ) < 20.0f )
			{
				m_nextPhraseTime = gpGlobals->time + duration + 0.1f;
				return true;
			}
			TheBotPhrases->ResetPlaceStatementInterval( phrase->GetID() );
		}
		if ( speakable )
		{
			bot->Radio( speakable, NULL, static_cast<short>(profile->GetVoicePitch()), false );
			bot->GetChatter()->ResetRadioSilenceDuration();
			bot->StartVoiceFeedback( duration + 1.0f );
		}
		else if ( phrase->GetRadioEquivalent() != EVENT_INVALID )
		{
			bot->SendRadioMessage( phrase->GetRadioEquivalent() );
			bot->GetChatter()->ResetRadioSilenceDuration();
			duration = 2.0f;
		}
		else
			m_nextPhraseTime = 0.0f;
	}
	m_nextPhraseTime = gpGlobals->time + duration + 0.1f;
	return true;
}

void BotChatterInterface::ReportingIn( void )
{
	BotStatement *statement = new BotStatement( this, BOT_STATEMENT_INFORMATION, 10.0f );
	Place place = m_bot->GetPlace();
	statement->AppendPhrase( TheBotPhrases->GetPlace( place ) );
	switch ( m_bot->GetTask() )
	{
	case CCSBot::TASK_PLANT_BOMB:
		m_bot->GetChatter()->GoingToPlantTheBomb( UNDEFINED_PLACE );
		break;
	case CCSBot::TASK_DEFUSE_BOMB:
	{
		BotStatement *defusing = new BotStatement( m_bot->GetChatter(), BOT_STATEMENT_SAY, 3.0f );
		defusing->AppendPhrase( TheBotPhrases->GetPhrase( "DefusingBomb" ) );
		m_bot->GetChatter()->AddStatement( defusing );
		break;
	}
	case CCSBot::TASK_GUARD_LOOSE_BOMB:
	{
		CBaseEntity *bomb = TheCSBots()->GetLooseBomb();
		if ( bomb )
		{
			statement->AppendPhrase( TheBotPhrases->GetPhrase( "GuardingLooseBomb" ) );
			statement->AttachMeme( new BotBombStatusMeme( CSGameState::BOMB_LOOSE, &bomb->pev->origin ) );
		}
		break;
	}
	case CCSBot::TASK_RESCUE_HOSTAGES:
		m_bot->GetChatter()->EscortingHostages();
		break;
	case CCSBot::TASK_GUARD_HOSTAGES:
		m_bot->GetChatter()->GuardingHostages( UNDEFINED_PLACE, !m_bot->IsAtHidingSpot() );
		break;
	case CCSBot::TASK_GUARD_HOSTAGE_RESCUE_ZONE:
		m_bot->GetChatter()->GuardingHostageEscapeZone( !m_bot->IsAtHidingSpot() );
		break;
	}

	if ( m_bot->IsAttacking() )
	{
		if ( m_bot->IsOutnumbered() )
		{
			statement->AppendPhrase( TheBotPhrases->GetPhrase( "Help" ) );
			statement->AttachMeme( new BotHelpMeme( place ) );
		}
		else
			statement->AppendPhrase( TheBotPhrases->GetPhrase( "InCombat" ) );
	}
	else
	{
		statement->SetStartTime( gpGlobals->time + 2.0f );
		float elapsed = gpGlobals->time - m_bot->GetEnemyLastSeenTimestamp();
		float deathTimestamp = m_bot->GetLastEnemyDeathTimestamp();
		if ( deathTimestamp < 10.0f && deathTimestamp >= elapsed + 0.5f )
			statement->AppendPhrase( TheBotPhrases->GetPhrase( "EnemyDown" ) );
		else if ( elapsed < 10.0f )
			statement->AppendPhrase( TheBotPhrases->GetPhrase( "EnemySpotted" ) );
		else
			statement->AppendPhrase( TheBotPhrases->GetPhrase( "Clear" ) );
	}
	AddStatement( statement );
}

void BotFollowMeme::Interpret( CCSBot *sender, CCSBot *receiver ) const
{
	if ( receiver->IsRogue() || receiver->IsBusy() )
		return;
	PathCost cost( receiver, SAFEST_ROUTE );
	CNavArea *area = TheNavAreaGrid.GetNearestNavArea( &sender->pev->origin );
	float distance = NavAreaTravelDistance( receiver->GetLastKnownArea(), area, cost );
	if ( distance >= 0.0f && distance <= 1000.0f )
	{
		receiver->Follow( sender );
		BotStatement *statement = new BotStatement( receiver->GetChatter(), BOT_STATEMENT_SAY, 3.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "CoveringFriend" ) );
		receiver->GetChatter()->AddStatement( statement );
	}
}

void BotDefendHereMeme::Interpret( CCSBot *, CCSBot *receiver ) const
{
	if ( receiver->IsRogue() || receiver->IsBusy() )
		return;
	Place place = TheNavAreaGrid.GetPlace( &m_position );
	const Vector *spot = NULL;
	if ( place )
		spot = FindRandomHidingSpot( receiver, place, receiver->IsSniper() );
	if ( spot )
	{
		receiver->SetTask( CCSBot::TASK_HOLD_POSITION );
		receiver->SetTaskEntity( NULL );
		receiver->Hide( spot, -1.0f, false );
	}
	else
	{
		receiver->SetTask( CCSBot::TASK_HOLD_POSITION );
		receiver->SetTaskEntity( NULL );
		CNavArea *area = TheNavAreaGrid.GetNearestNavArea( &m_position );
		receiver->Hide( area, -1.0f, 750.0f, false );
		BotStatement *statement = new BotStatement( receiver->GetChatter(), BOT_STATEMENT_SAY, 3.0f );
		statement->AppendPhrase( TheBotPhrases->GetPhrase( "Affirmative" ) );
		receiver->GetChatter()->AddStatement( statement );
	}
}
