#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "sound.h"
#include "mem.h"
#include "common.h"
#include "conprint.h"
#include "client.h"
#include "cvar.h"
#include "filesystem.h"
#include "zone.h"
#include "../common/const.h"
#include "../common/Sequence.h"

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

#if defined( _WIN32 )
#include <cassert>
#if defined( NDEBUG )
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );
#endif
#endif

typedef struct voxword
{
	int volume;
	int pitch;
	int start;
	int end;
	int cbtrim;
	int fKeepCached;
	int samplefrac;
	int timecompress;
	sfx_t* sfx;
} voxword_t;

constexpr int MAX_VOX_SENTENCES = 16;
constexpr int MAX_VOX_WORDS = 32;
static_assert( sizeof( voxword_t ) == 36 );

voxword_t rgrgvoxword[MAX_VOX_SENTENCES][MAX_VOX_WORDS];
char* rgpparseword[MAX_VOX_WORDS];
static char szsentences[] = "sound/sentences.txt";
static char voxcomma[] = "_comma";
static char voxperiod[] = "_period";

sentenceEntry_s* SequenceGetSentenceByIndex( unsigned int index );
extern "C" void VoiceSE_NotifyFreeChannel( int channel );

typedef short sample_t;

typedef struct dlyline_s
{
	int cdelaysamplesmax;
	int lp;
	int idelayinput;
	int idelayoutput;
	int idelayoutputxf;
	int xfade;
	int delaysamples;
	int delayfeed;
	int lp0;
	int lp1;
	int lp2;
	int mod;
	int modcur;
	HANDLE hdelayline;
	sample_t* lpdelayline;
} dlyline_t;

static_assert( sizeof( dlyline_t ) == 60 );

constexpr int CSXDLYMAX = 4;
constexpr int ISXMONODLY = 0;
constexpr double SXDLY_MAX = 0.4;
constexpr double SXDLY_FEEDBACK_SCALE = 255.0;
constexpr int SOUND_VOLUME_LEVELS = 32;
constexpr int SOUND_SAMPLE_VALUES = 256;
constexpr int SOUND_VOLUME_SCALE = 8;
constexpr int MOUTH_AVERAGE_SAMPLES = 10;
constexpr int MOUTH_SAMPLE_STEP = 80;
constexpr int MOUTH_SAMPLE_VARIATION_MASK = 31;

dlyline_t rgsxdly[CSXDLYMAX];
cvar_t sxdly_delay = { "room_delay", "0", 0, 0.0f, nullptr };
cvar_t sxdly_feedback = { "room_feedback", "0.2", 0, 0.0f, nullptr };
cvar_t sxdly_lp = { "room_dlylp", "1.0", 0, 0.0f, nullptr };
float sxdly_delayprev;
int sxhires;
int snd_scaletable[SOUND_VOLUME_LEVELS][SOUND_SAMPLE_VALUES];

void SXDLY_Free( int idelay )
{
	dlyline_t* delayLine = &rgsxdly[idelay];
	if ( delayLine->lpdelayline )
	{
#if defined( _WIN32 )
		GlobalUnlock( delayLine->hdelayline );
		GlobalFree( delayLine->hdelayline );
#else
		std::free( delayLine->hdelayline );
#endif
		delayLine->hdelayline = nullptr;
		delayLine->lpdelayline = nullptr;
	}
}

qboolean SXDLY_Init( int idelay, float delay )
{
	if ( delay > SXDLY_MAX )
		delay = static_cast<float>( SXDLY_MAX );
	SXDLY_Free( idelay );
	if ( delay == 0.0f )
		return 1;

	dlyline_t* delayLine = &rgsxdly[idelay];
	delayLine->cdelaysamplesmax = ( static_cast<int>( static_cast<float>( shm->speed ) * delay ) << sxhires ) + 1;
	int size = delayLine->cdelaysamplesmax * static_cast<int>( sizeof( sample_t ) );
#if defined( _WIN32 )
	HGLOBAL handle = GlobalAlloc( GMEM_MOVEABLE | GMEM_SHARE, static_cast<SIZE_T>( size ) );
	if ( !handle )
	{
		Con_SafePrintf( "Sound FX: Out of memory.\n" );
		return 0;
	}

	sample_t* samples = static_cast<sample_t*>( GlobalLock( handle ) );
	if ( !samples )
	{
		Con_SafePrintf( "Sound FX: Failed to lock.\n" );
		GlobalFree( handle );
		return 0;
	}
#else
	sample_t* samples = static_cast<sample_t*>( std::malloc( static_cast<size_t>( size ) ) );
	if ( !samples )
	{
		Con_SafePrintf( "Sound FX: Out of memory.\n" );
		return 0;
	}
#endif

	Q_memset( samples, 0, size );
	int outputIndex = delayLine->cdelaysamplesmax - delayLine->delaysamples;
#if defined( _WIN32 )
	delayLine->hdelayline = handle;
#else
	delayLine->hdelayline = samples;
#endif
	delayLine->lpdelayline = samples;
	delayLine->idelayoutput = outputIndex;
	delayLine->idelayinput = 0;
	delayLine->xfade = 0;
	delayLine->lp = 1;
	delayLine->mod = 0;
	delayLine->modcur = 0;
	delayLine->lp2 = 0;
	delayLine->lp1 = 0;
	delayLine->lp0 = 0;
	return 1;
}

void SXDLY_CheckNewDelayVal()
{
	dlyline_t* delayLine = &rgsxdly[ISXMONODLY];
	if ( sxdly_delay.value != sxdly_delayprev )
	{
		if ( sxdly_delay.value == 0.0f )
		{
			SXDLY_Free( ISXMONODLY );
			sxdly_delayprev = sxdly_delay.value;
		}
		else
		{
			double delay = sxdly_delay.value < SXDLY_MAX ? sxdly_delay.value : SXDLY_MAX;
			delayLine->delaysamples = static_cast<int>( static_cast<double>( shm->speed ) * delay ) << sxhires;
			if ( !delayLine->lpdelayline )
				SXDLY_Init( ISXMONODLY, static_cast<float>( SXDLY_MAX ) );
			if ( delayLine->lpdelayline )
			{
				Q_memset( delayLine->lpdelayline, 0, delayLine->cdelaysamplesmax * sizeof( sample_t ) );
				delayLine->lp0 = 0;
				delayLine->lp1 = 0;
				delayLine->lp2 = 0;
			}
			delayLine->idelayinput = 0;
			delayLine->idelayoutput = delayLine->cdelaysamplesmax - delayLine->delaysamples;
			sxdly_delayprev = sxdly_delay.value;
			if ( delayLine->delaysamples == 0 )
				SXDLY_Free( ISXMONODLY );
		}
	}
	delayLine->lp = static_cast<int>( sxdly_lp.value );
	delayLine->delayfeed = static_cast<int>( static_cast<float>( sxdly_feedback.value * SXDLY_FEEDBACK_SCALE ) );
}

void SND_InitScaletable()
{
	for ( int volume = 0; volume < SOUND_VOLUME_LEVELS; ++volume )
	{
		for ( int sample = 0; sample < SOUND_SAMPLE_VALUES; ++sample )
			snd_scaletable[volume][sample] = SOUND_VOLUME_SCALE * volume * static_cast<signed char>( sample );
	}
}

void SND_ForceInitMouth( int entnum )
{
	if ( entnum >= 0 && entnum < cl.max_edicts )
	{
		cl_entities[entnum].mouth.mouthopen = 0;
		cl_entities[entnum].mouth.sndavg = 0;
		cl_entities[entnum].mouth.sndcount = 0;
	}
}

void SND_ForceCloseMouth( int entnum )
{
	if ( entnum >= 0 && entnum < cl.max_edicts )
		cl_entities[entnum].mouth.mouthopen = 0;
}

void SND_InitMouth( int entnum, int entchannel )
{
	if ( ( entchannel == CHAN_VOICE || entchannel == CHAN_STREAM ) && entnum > 0 && entnum < cl.max_edicts )
	{
		cl_entities[entnum].mouth.mouthopen = 0;
		cl_entities[entnum].mouth.sndavg = 0;
		cl_entities[entnum].mouth.sndcount = 0;
	}
}

void SND_MoveMouth16( int entnum, short* pdata, int count )
{
	if ( entnum < 0 || entnum >= cl.max_edicts )
		return;

	mouth_t& mouth = cl_entities[entnum].mouth;
	int sampleCount = mouth.sndcount;
	int average = 0;
	for ( int position = 0; position < count && sampleCount < MOUTH_AVERAGE_SAMPLES; )
	{
		int sample = static_cast<signed char>( pdata[position] >> CHAR_BIT );
		++sampleCount;
		average += std::abs( sample );
		position += MOUTH_SAMPLE_STEP + ( static_cast<unsigned char>( sample ) & MOUTH_SAMPLE_VARIATION_MASK );
	}

	int total = mouth.sndavg + average;
	mouth.sndcount = static_cast<byte>( sampleCount );
	mouth.sndavg = total;
	if ( mouth.sndcount >= MOUTH_AVERAGE_SAMPLES )
	{
		mouth.sndavg = 0;
		mouth.sndcount = 0;
		mouth.mouthopen = static_cast<byte>( total / MOUTH_AVERAGE_SAMPLES );
	}
}

extern "C" void Wavstream_Close( int index )
{
	wavstream_t* stream = &wavstreams[index];
	if ( stream->hFile )
		FS_Close( stream->hFile );
	Q_memset( stream, 0, sizeof( *stream ) );
	stream->hFile = nullptr;
}

extern "C" void SND_CloseMouth( channel_t* channel )
{
	if ( channel->entnum > 0 && ( channel->entchannel == CHAN_VOICE || channel->entchannel == CHAN_STREAM )
		 && channel->entnum < cl.max_edicts )
		cl_entities[channel->entnum].mouth.mouthopen = 0;
}

extern "C" void S_FreeChannel( channel_t* channel )
{
	if ( channel->entchannel == CHAN_STREAM )
	{
		Wavstream_Close( static_cast<int>( channel - channels ) );
		Cache_Free( &channel->sfx->cache );
	}
	else if ( channel->entchannel >= CHAN_NETWORKVOICE_BASE && channel->entchannel <= CHAN_NETWORKVOICE_END )
		VoiceSE_NotifyFreeChannel( channel->entchannel );
	if ( channel->isentence >= 0 )
		rgrgvoxword[channel->isentence][0].sfx = nullptr;
	channel->isentence = -1;
	channel->sfx = nullptr;
	SND_CloseMouth( channel );
}
extern "C" void VOX_ReadSentenceFile();

char* rgpszrawsentence[MAX_SENTENCES];
int cszrawsentences;

extern "C" void VOX_Shutdown()
{
	for ( int i = 0; i < cszrawsentences; ++i )
		Mem_Free( rgpszrawsentence[i] );
	cszrawsentences = 0;
}

extern "C" void VOX_Init()
{
	Q_memset( rgrgvoxword, 0, sizeof( rgrgvoxword ) );
	VOX_ReadSentenceFile();
}

extern "C" void VOX_ReadSentenceFile()
{
	VOX_Shutdown();
	int fileLength;
	char* buffer = reinterpret_cast<char*>( COM_LoadFileForMe( szsentences, &fileLength ) );
	if ( !buffer )
	{
		Con_DPrintf( "VOX_ReadSentenceFile: Couldn't load %s\n", szsentences );
		return;
	}
	char* end = buffer + fileLength;
	char* cursor = buffer;
	int count = 0;
	while ( cursor < end )
	{
		while ( cursor < end && ( *cursor == '\t' || *cursor == '\n' || *cursor == '\r' || *cursor == ' ' ) )
			++cursor;
		if ( cursor >= end )
			break;
		char* name = nullptr;
		char* words = cursor;
		if ( *cursor != '/' )
		{
			name = cursor;
			while ( words < end && *words != ' ' && *words != '\t' )
				++words;
			if ( words < end )
				*words++ = 0;
			cursor = words;
		}
		while ( cursor < end && *cursor != '\n' && *cursor != '\r' )
			++cursor;
		if ( cursor < end )
			*cursor++ = 0;
		if ( name )
		{
			std::size_t nameBytes = std::strlen( name ) + 1;
			std::size_t wordsBytes = std::strlen( words ) + 1;
			rgpszrawsentence[count] = static_cast<char*>( Mem_Malloc( nameBytes + wordsBytes ) );
			Q_memcpy( rgpszrawsentence[count], name, nameBytes );
			Q_memcpy( rgpszrawsentence[count] + nameBytes, words, wordsBytes );
			if ( ++count >= MAX_SENTENCES )
				break;
		}
	}
	cszrawsentences = count;
	Mem_Free( buffer );
}

extern "C" char* VOX_LookupString( char* pszin, int* psentencenum )
{
	if ( pszin[0] == '#' )
	{
		unsigned int index = static_cast<unsigned int>( std::atoi( pszin + 1 ) );
		sentenceEntry_s* sentence = SequenceGetSentenceByIndex( index );
		if ( sentence )
			return sentence->data;
	}
	for ( int i = 0; i < cszrawsentences; ++i )
	{
		if ( Q_strcasecmp( pszin, rgpszrawsentence[i] ) )
			continue;
		if ( psentencenum )
			*psentencenum = i;
		char* sentence = rgpszrawsentence[i] + Q_strlen( rgpszrawsentence[i] ) + 1;
		while ( *sentence == '\t' || *sentence == ' ' )
			++sentence;
		return sentence;
	}
	return nullptr;
}

extern "C" char* VOX_GetDirectory( char* path, char* sentence, int size )
{
	int skipped = 0;
	char* slash = sentence + Q_strlen( sentence ) - 1;
	while ( *slash != '/' && slash > sentence )
	{
		--slash;
		++skipped;
	}
	if ( *slash == '/' )
	{
		int count = Q_strlen( sentence ) - skipped;
		if ( count >= size )
		{
			Con_DPrintf( "VOX_GetDirectory: invalid directory in: %s\n", sentence );
			return nullptr;
		}
		Q_memcpy( path, sentence, count );
		path[count] = 0;
		return slash + 1;
	}
	Q_strcpy( path, "vox/" );
	return sentence;
}

extern "C" void VOX_ParseString( char* sentence )
{
	Q_memset( rgpparseword, 0, sizeof( rgpparseword ) );
	if ( !sentence )
		return;
	rgpparseword[0] = sentence;
	int count = 1;
	char* cursor = sentence;
	while ( count < MAX_VOX_WORDS )
	{
		while ( *cursor && *cursor != ' ' && *cursor != ',' && *cursor != '.' )
		{
			if ( *cursor == '(' )
			{
				++cursor;
				while ( *cursor && *cursor != ')' )
					++cursor;
				if ( !*cursor )
					return;
				++cursor;
				if ( !*cursor )
					return;
				break;
			}
			++cursor;
		}
		if ( !*cursor )
			return;
		char next = cursor[1];
		if ( ( *cursor == ',' || *cursor == '.' ) && next && next != '\n' && next != '\r' )
		{
			rgpparseword[count++] = *cursor == '.' ? voxperiod : voxcomma;
			if ( count >= MAX_VOX_WORDS )
				return;
			next = cursor[1];
		}
		*cursor++ = 0;
		if ( !next )
			return;
		while ( *cursor == ',' || *cursor == '.' || *cursor == ' ' )
			++cursor;
		if ( !*cursor )
			return;
		rgpparseword[count++] = cursor;
	}
}

extern "C" int VOX_ParseWordParams( char* word, voxword_t* parameters, int first )
{
	constexpr int WORD_PARAMETER_NUMBER_BYTES = 8;
	constexpr int DEFAULT_WORD_VOLUME = 100;
	constexpr int DEFAULT_WORD_PITCH = -1;
	constexpr int DEFAULT_WORD_END = 100;
	static voxword_t defaults;
	if ( first )
	{
		defaults.fKeepCached = 0;
		defaults.pitch = DEFAULT_WORD_PITCH;
		defaults.volume = DEFAULT_WORD_VOLUME;
		defaults.start = 0;
		defaults.end = DEFAULT_WORD_END;
		defaults.timecompress = 0;
	}
	*parameters = defaults;
	if ( word[Q_strlen( word ) - 1] != ')' )
		return 1;
	char* cursor = word;
	while ( *cursor != '(' && *cursor != ')' )
		++cursor;
	if ( *cursor == ')' )
		return 0;
	*cursor++ = 0;
	while ( *cursor )
	{
		while ( *cursor && *cursor != ')' && *cursor != 'e' && *cursor != 'p' && *cursor != 's' && *cursor != 't'
				&& *cursor != 'v' )
			++cursor;
		if ( !*cursor || *cursor == ')' )
			break;
		char parameter = *cursor;
		char number[WORD_PARAMETER_NUMBER_BYTES];
		Q_memset( number, 0, sizeof( number ) );
		if ( !std::isdigit( cursor[1] ) )
			break;
		++cursor;
		int count = 0;
		while ( count < WORD_PARAMETER_NUMBER_BYTES - 1 && std::isdigit( *cursor ) )
			number[count++] = *cursor++;
		int value = Q_atoi( number );
		switch ( parameter )
		{
		case 'e':
			parameters->end = value;
			break;
		case 'p':
			parameters->pitch = value;
			break;
		case 's':
			parameters->start = value;
			break;
		case 't':
			parameters->timecompress = value;
			break;
		case 'v':
			parameters->volume = value;
			break;
		}
	}
	if ( Q_strlen( word ) )
		return 1;
	defaults = *parameters;
	return 0;
}

extern "C" int VOX_IFindEmptySentence()
{
	for ( int i = 0; i < MAX_VOX_SENTENCES; ++i )
	{
		if ( !rgrgvoxword[i][0].sfx )
			return i;
	}
	Con_DPrintf( "Sentence or Pitch shift ignored. > 16 playing!\n" );
	return -1;
}

extern "C" sfxcache_t* VOX_LoadSound( channel_t* channel, char* name )
{
	constexpr int SENTENCE_DIRECTORY_BYTES = 32;
	constexpr int SENTENCE_BUFFER_BYTES = 512;
	if ( !name )
		return nullptr;
	voxword_t words[MAX_VOX_WORDS];
	char buffer[SENTENCE_BUFFER_BYTES];
	Q_memset( words, 0, sizeof( words ) );
	Q_memset( buffer, 0, sizeof( buffer ) );
	char* sentence = VOX_LookupString( name, nullptr );
	if ( !sentence )
	{
		Con_DPrintf( "VOX_LoadSound: no sentence named %s\n", name );
		return nullptr;
	}
	char directory[SENTENCE_DIRECTORY_BYTES];
	sentence = VOX_GetDirectory( directory, sentence, sizeof( directory ) );
	if ( !sentence )
	{
		Con_DPrintf( "VOX_LoadSound: failed getting directory for %s\n", name );
		return nullptr;
	}
	if ( static_cast<unsigned int>( Q_strlen( sentence ) ) > sizeof( buffer ) - 1 )
	{
		Con_DPrintf( "VOX_LoadSound: sentence is too long %s\n", sentence );
		return nullptr;
	}
	Q_strncpy( buffer, sentence, sizeof( buffer ) - 1 );
	buffer[sizeof( buffer ) - 1] = 0;
	VOX_ParseString( buffer );
	int count = 0;
	int index = 0;
	while ( rgpparseword[index] )
	{
		if ( VOX_ParseWordParams( rgpparseword[index], &words[count], index == 0 ) )
		{
			char path[SOUND_NAME_BYTES];
			_snprintf( path, sizeof( path ), "%s%s.wav", directory, rgpparseword[index] );
			path[sizeof( path ) - 1] = 0;
			if ( Q_strlen( path ) >= sizeof( path ) )
				continue;
			words[count].sfx = S_FindName( path, &words[count].fKeepCached );
			++count;
		}
		++index;
	}
	int slot = VOX_IFindEmptySentence();
	if ( slot < 0 )
		return nullptr;
	sfx_t* sound = words[0].sfx;
	count = 0;
	while ( words[count].sfx )
	{
		rgrgvoxword[slot][count] = words[count];
		++count;
	}
	rgrgvoxword[slot][count].sfx = nullptr;
	channel->isentence = slot;
	channel->iword = 0;
	channel->sfx = rgrgvoxword[slot][0].sfx;
	sfxcache_t* cache = S_LoadSound( sound, nullptr );
	if ( !cache )
		S_FreeChannel( channel );
	return cache;
}

extern "C" qboolean Wavstream_Init()
{
	for ( wavstream_t& stream : wavstreams )
	{
		Q_memset( &stream, 0, sizeof( stream ) );
		stream.hFile = nullptr;
	}
	return true;
}

extern "C" void VOX_SetChanVol( channel_t* channel )
{
	constexpr int FULL_VOX_VOLUME = 100;
	constexpr double VOLUME_PERCENT_SCALE = 100.0;
	if ( channel->isentence < 0 )
		return;
	voxword_t* word = &rgrgvoxword[channel->isentence][channel->iword];
	if ( word->volume == FULL_VOX_VOLUME )
		return;
	float scale = static_cast<float>( static_cast<float>( word->volume ) / VOLUME_PERCENT_SCALE );
	if ( scale != 1.0f )
	{
		float left = static_cast<float>( channel->leftvol );
		channel->rightvol = static_cast<int>( static_cast<float>( channel->rightvol ) * scale );
		channel->leftvol = static_cast<int>( left * scale );
	}
}

extern "C" void VOX_TrimStartEndTimes( channel_t* channel, sfxcache_t* cache )
{
	constexpr double SAMPLE_PERCENT_SCALE = 100.0;
	constexpr int QUIET_SAMPLE_LIMIT = 2;
	constexpr int MAX_TRIM_SEARCH_SAMPLES = 255;
	constexpr int NORMAL_WORD_PITCH = 100;
	constexpr int SAMPLE_FRACTION_BITS = 8;
	if ( channel->isentence < 0 )
		return;
	voxword_t* word = &rgrgvoxword[channel->isentence][channel->iword];
	float start = static_cast<float>( word->start );
	float end = static_cast<float>( word->end );
	word->cbtrim = cache->length;
	if ( !( start < end ) )
		return;
	if ( start > 0.0f && start < SAMPLE_PERCENT_SCALE )
	{
		double offset = start / SAMPLE_PERCENT_SCALE * static_cast<double>( cache->length );
		channel->pos += static_cast<int>( offset );
		int position = channel->pos;
		for ( int i = 0; i < MAX_TRIM_SEARCH_SAMPLES && position < cache->length; ++i, ++position )
		{
			int sample = static_cast<signed char>( cache->data[position] );
			if ( sample >= -QUIET_SAMPLE_LIMIT && sample <= QUIET_SAMPLE_LIMIT )
			{
				channel->pos = position;
				channel->end -= i + static_cast<int>( offset );
				break;
			}
		}
		if ( word->pitch != NORMAL_WORD_PITCH )
		{
			std::uint32_t sampleOffset = static_cast<std::uint32_t>( channel->pos ) << SAMPLE_FRACTION_BITS;
			word->samplefrac = static_cast<int>( static_cast<std::uint32_t>( word->samplefrac ) + sampleOffset );
		}
	}
	if ( end > 0.0f && end < SAMPLE_PERCENT_SCALE )
	{
		double offset = ( SAMPLE_PERCENT_SCALE - end ) / SAMPLE_PERCENT_SCALE * static_cast<double>( cache->length );
		channel->end -= static_cast<int>( offset );
		int position = cache->length - static_cast<int>( offset );
		for ( int i = 0; i < MAX_TRIM_SEARCH_SAMPLES && position > channel->pos; ++i, --position )
		{
			int sample = static_cast<signed char>( cache->data[position] );
			if ( sample >= -QUIET_SAMPLE_LIMIT && sample <= QUIET_SAMPLE_LIMIT )
			{
				channel->end -= i;
				word->cbtrim = word->cbtrim - i - static_cast<int>( offset );
				break;
			}
		}
	}
}

extern "C" void VOX_MakeSingleWordSentence( channel_t* channel, int pitch )
{
	constexpr int FULL_WORD_PERCENT = 100;
	int slot = VOX_IFindEmptySentence();
	if ( slot < 0 )
	{
		channel->pitch = FULL_WORD_PERCENT;
		channel->isentence = -1;
		return;
	}
	voxword_t word;
	word.volume = FULL_WORD_PERCENT;
	word.pitch = FULL_WORD_PERCENT;
	word.start = 0;
	word.end = FULL_WORD_PERCENT;
	word.fKeepCached = 1;
	word.samplefrac = 0;
	word.timecompress = 0;
	word.sfx = channel->sfx;
	rgrgvoxword[slot][1].sfx = nullptr;
	Q_memcpy( &rgrgvoxword[slot][0], &word, sizeof( word ) );
	channel->iword = 0;
	channel->pitch = pitch;
	channel->isentence = slot;
}

static_assert( sizeof( portable_samplepair_t ) == 8 );

constexpr int ISXRVB = 1;
constexpr int CSXRVBMAX = 2;
constexpr int ISXSTEREODLY = 3;
constexpr double SXSTE_MAX = 0.1;
constexpr double SXRVB_MAX = 0.1;
constexpr double SXRVB_SECOND_DELAY_SCALE = 0.71;
constexpr int SOUND_11K = 11025;
constexpr int RVB_MODRATE1 = 500;
constexpr int RVB_MODRATE2 = 700;
constexpr int RVB_XFADE = 32;
constexpr int STEREO_DELAY_XFADE = 128;
constexpr int CSXLPMAX = 10;
constexpr int SOUND_PAINTBUFFER_CAPACITY = 1026;
constexpr int LOWPASS_GAIN_SHIFT = 2;
constexpr int AMPLITUDE_GAIN_SHIFT = 8;
constexpr int AMPLITUDE_TARGET_MIN = 32;
constexpr int AMPLITUDE_TARGET_MAX = 255;
constexpr int SOUND_SAMPLE_MIN = SHRT_MIN;
constexpr int SOUND_SAMPLE_MAX = SHRT_MAX;

portable_samplepair_t paintbuffer[SOUND_PAINTBUFFER_CAPACITY];
int rgsxlp[CSXLPMAX];
int sxamodl;
int sxamodr;
int sxamodlt;
int sxamodrt;
int sxmod1;
int sxmod2;
int sxmod1cur;
int sxmod2cur;

cvar_t sxste_delay = { "room_left", "0", 0, 0.0f, nullptr };
cvar_t sxrvb_size = { "room_size", "0", 0, 0.0f, nullptr };
cvar_t sxrvb_feedback = { "room_refl", "0.7", 0, 0.0f, nullptr };
cvar_t sxrvb_lp = { "room_rvblp", "1.0", 0, 0.0f, nullptr };
cvar_t sxmod_lowpass = { "room_lp", "0", 0, 0.0f, nullptr };
cvar_t sxmod_mod = { "room_mod", "0", 0, 0.0f, nullptr };
float sxste_delayprev;
float sxrvb_sizeprev;

int RandomLong( int low, int high );

void SXDLY_CheckNewStereoDelayVal()
{
	dlyline_t* delayLine = &rgsxdly[ISXSTEREODLY];
	if ( sxste_delay.value != sxste_delayprev )
	{
		if ( sxste_delay.value == 0.0f )
		{
			SXDLY_Free( ISXSTEREODLY );
			sxste_delayprev = 0.0f;
		}
		else
		{
			double delay = sxste_delay.value < SXSTE_MAX ? sxste_delay.value : SXSTE_MAX;
			int samples = static_cast<int>( static_cast<double>( shm->speed ) * delay ) << sxhires;
			if ( !delayLine->lpdelayline )
			{
				delayLine->delaysamples = samples;
				SXDLY_Init( ISXSTEREODLY, static_cast<float>( SXSTE_MAX ) );
			}
			if ( samples != delayLine->delaysamples )
			{
				delayLine->idelayoutputxf = delayLine->idelayinput - samples;
				if ( delayLine->idelayoutputxf < 0 )
					delayLine->idelayoutputxf += delayLine->cdelaysamplesmax;
				delayLine->xfade = STEREO_DELAY_XFADE;
			}
			sxste_delayprev = sxste_delay.value;
			delayLine->mod = 0;
			delayLine->modcur = 0;
			if ( delayLine->delaysamples == 0 )
				SXDLY_Free( ISXSTEREODLY );
		}
	}
}

void SXRVB_CheckNewReverbVal()
{
	if ( sxrvb_size.value != sxrvb_sizeprev )
	{
		sxrvb_sizeprev = sxrvb_size.value;
		if ( sxrvb_size.value == 0.0f )
		{
			SXDLY_Free( ISXRVB );
			SXDLY_Free( ISXRVB + 1 );
		}
		else
		{
			for ( int i = ISXRVB; i < ISXRVB + CSXRVBMAX; ++i )
			{
				dlyline_t* delayLine = &rgsxdly[i];
				double requested = sxrvb_size.value;
				int modulationRate = RVB_MODRATE1;
				if ( i == ISXRVB + 1 )
				{
					requested *= SXRVB_SECOND_DELAY_SCALE;
					modulationRate = RVB_MODRATE2;
				}
				delayLine->mod = ( modulationRate * ( shm->speed / SOUND_11K ) ) << sxhires;
				double delay = requested < SXRVB_MAX ? requested : SXRVB_MAX;
				int samples = static_cast<int>( static_cast<double>( shm->speed ) * delay ) << sxhires;
				int modulation = delayLine->mod;
				if ( !delayLine->lpdelayline )
				{
					delayLine->delaysamples = samples;
					SXDLY_Init( i, static_cast<float>( SXRVB_MAX ) );
				}
				delayLine->mod = modulation;
				delayLine->modcur = modulation;
				if ( samples != delayLine->delaysamples )
				{
					delayLine->idelayoutputxf = delayLine->idelayinput - samples;
					if ( delayLine->idelayoutputxf < 0 )
						delayLine->idelayoutputxf += delayLine->cdelaysamplesmax;
					delayLine->xfade = RVB_XFADE;
				}
				if ( delayLine->delaysamples == 0 )
					SXDLY_Free( i );
			}
		}
	}
	rgsxdly[ISXRVB].lp = static_cast<int>( sxrvb_lp.value );
	rgsxdly[ISXRVB + 1].lp = static_cast<int>( sxrvb_lp.value );
	rgsxdly[ISXRVB].delayfeed = static_cast<int>( static_cast<float>( sxrvb_feedback.value * SXDLY_FEEDBACK_SCALE ) );
	rgsxdly[ISXRVB + 1].delayfeed =
		static_cast<int>( static_cast<float>( sxrvb_feedback.value * SXDLY_FEEDBACK_SCALE ) );
}

void SXRVB_DoAMod( int count )
{
	if ( sxmod_lowpass.value == 0.0f && sxmod_mod.value == 0.0f )
		return;

	bool lowpass = sxmod_lowpass.value != 0.0f;
	bool modulate = sxmod_mod.value != 0.0f;
	portable_samplepair_t* sample = paintbuffer;
	while ( count-- )
	{
		int left = sample->left;
		int right = sample->right;
		if ( lowpass )
		{
			int inputLeft = left;
			int inputRight = right;
			left = ( left + rgsxlp[4] + rgsxlp[3] + rgsxlp[2] + rgsxlp[1] + rgsxlp[0] ) >> LOWPASS_GAIN_SHIFT;
			right = ( right + rgsxlp[9] + rgsxlp[8] + rgsxlp[7] + rgsxlp[5] + rgsxlp[6] ) >> LOWPASS_GAIN_SHIFT;
			rgsxlp[4] = rgsxlp[5];
			rgsxlp[9] = inputRight;
			rgsxlp[0] = rgsxlp[1];
			rgsxlp[1] = rgsxlp[2];
			rgsxlp[2] = rgsxlp[3];
			rgsxlp[3] = inputLeft;
			rgsxlp[5] = rgsxlp[6];
			rgsxlp[6] = rgsxlp[7];
			rgsxlp[7] = rgsxlp[8];
			rgsxlp[8] = inputRight;
		}
		if ( modulate )
		{
			if ( --sxmod1cur < 0 )
				sxmod1cur = sxmod1;
			if ( !sxmod1 )
				sxamodlt = RandomLong( AMPLITUDE_TARGET_MIN, AMPLITUDE_TARGET_MAX );
			if ( --sxmod2cur < 0 )
				sxmod2cur = sxmod2;
			if ( !sxmod2 )
				sxamodlt = RandomLong( AMPLITUDE_TARGET_MIN, AMPLITUDE_TARGET_MAX );
			left = ( sxamodl * left ) >> AMPLITUDE_GAIN_SHIFT;
			right = ( sxamodr * right ) >> AMPLITUDE_GAIN_SHIFT;
			if ( sxamodl < sxamodlt )
				++sxamodl;
			else if ( sxamodl > sxamodlt )
				--sxamodl;
			if ( sxamodr < sxamodrt )
				++sxamodr;
			else if ( sxamodr > sxamodrt )
				--sxamodr;
		}
		sample->left = ( std::clamp )( left, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX );
		sample->right = ( std::clamp )( right, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX );
		++sample;
	}
}

constexpr int REVERB_XFADE_SHIFT = 5;
constexpr int REVERB_RANDOM_MAX = 255;
constexpr int REVERB_RANDOM_SHIFT = 9;
constexpr int REVERB_FEEDBACK_SHIFT = 8;
constexpr int REVERB_OUTPUT_GAIN = 11;
constexpr int REVERB_OUTPUT_SHIFT = 6;

void SXRVB_DoReverb( int count )
{
	if ( !rgsxdly[ISXRVB].lpdelayline )
		return;
	portable_samplepair_t* buffer = paintbuffer;
	while ( count-- )
	{
		int left = buffer->left;
		int right = buffer->right;
		int average = ( left + right ) >> 1;
		int output = 0;
		for ( int index = ISXRVB; index < ISXRVB + CSXRVBMAX; ++index )
		{
			dlyline_t* delayLine = &rgsxdly[index];
			if ( --delayLine->modcur < 0 )
				delayLine->modcur = delayLine->mod;
			if ( index != ISXRVB && !delayLine->lpdelayline )
				continue;
			sample_t delayed = delayLine->lpdelayline[delayLine->idelayoutput];
			if ( delayLine->xfade || delayed || left || right )
			{
				if ( !delayLine->xfade && !delayLine->mod )
				{
					delayLine->idelayoutputxf =
						delayLine->idelayoutput
						+ ( ( RandomLong( 0, REVERB_RANDOM_MAX ) * delayLine->delaysamples ) >> REVERB_RANDOM_SHIFT );
					if ( delayLine->idelayoutputxf >= delayLine->cdelaysamplesmax )
						delayLine->idelayoutputxf -= delayLine->cdelaysamplesmax;
					delayLine->xfade = RVB_XFADE;
				}
				if ( delayLine->xfade )
				{
					int crossfade =
						( delayLine->lpdelayline[delayLine->idelayoutputxf] * ( RVB_XFADE - delayLine->xfade ) )
						>> REVERB_XFADE_SHIFT;
					delayed =
						static_cast<sample_t>( ( ( delayed * delayLine->xfade ) >> REVERB_XFADE_SHIFT ) + crossfade );
					if ( ++delayLine->idelayoutputxf >= delayLine->cdelaysamplesmax )
						delayLine->idelayoutputxf = 0;
					if ( --delayLine->xfade == 0 )
						delayLine->idelayoutput = delayLine->idelayoutputxf;
				}
				int value = average;
				if ( delayed )
					value = ( std::clamp )( average + ( ( delayLine->delayfeed * delayed ) >> REVERB_FEEDBACK_SHIFT ),
						SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX );
				int filtered = value;
				if ( delayLine->lp )
				{
					filtered = ( value + delayLine->lp0 ) >> 1;
					delayLine->lp0 = value;
				}
				delayLine->lpdelayline[delayLine->idelayinput] = static_cast<sample_t>( filtered );
				output += filtered;
			}
			else
			{
				delayLine->lp0 = 0;
				delayLine->lpdelayline[delayLine->idelayinput] = 0;
			}
			if ( ++delayLine->idelayinput >= delayLine->cdelaysamplesmax )
				delayLine->idelayinput = 0;
			if ( ++delayLine->idelayoutput >= delayLine->cdelaysamplesmax )
				delayLine->idelayoutput = 0;
		}
		int wet = ( output * REVERB_OUTPUT_GAIN ) >> REVERB_OUTPUT_SHIFT;
		buffer->left = ( std::clamp )( left + wet, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX );
		buffer->right = ( std::clamp )( right + wet, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX );
		++buffer;
	}
}

typedef struct sx_preset_s
{
	float room_lp;
	float room_mod;
	float room_size;
	float room_refl;
	float room_rvblp;
	float room_delay;
	float room_feedback;
	float room_dlylp;
	float room_left;
} sx_preset_t;

static_assert( sizeof( sx_preset_t ) == 36 );

constexpr int CSXROOM = 29;
constexpr int ROOM_EFFECTS_WATERLEVEL_THRESHOLD = 2;

sx_preset_t rgsxpre[CSXROOM] = { { 0, 0, 0, 0, 1, 0, 0, 2, 0 }, { 0, 0, 0, 0, 1, 0.065f, 0.1f, 0, 0.01f },
	{ 0, 0, 0, 0, 1, 0.02f, 0.75f, 0, 0.01f }, { 0, 0, 0, 0, 1, 0.03f, 0.78f, 0, 0.02f },
	{ 0, 0, 0, 0, 1, 0.06f, 0.77f, 0, 0.03f }, { 0, 0, 0.05f, 0.85f, 1, 0.008f, 0.96f, 2, 0.01f },
	{ 0, 0, 0.05f, 0.88f, 1, 0.01f, 0.98f, 2, 0.02f }, { 0, 0, 0.05f, 0.92f, 1, 0.015f, 0.995f, 2, 0.04f },
	{ 0, 0, 0.05f, 0.84f, 1, 0, 0, 2, 0.012f }, { 0, 0, 0.05f, 0.9f, 1, 0, 0, 2, 0.008f },
	{ 0, 0, 0.05f, 0.95f, 1, 0, 0, 2, 0.004f }, { 0, 0, 0.05f, 0.7f, 0, 0, 0, 2, 0.012f },
	{ 0, 0, 0.055f, 0.78f, 0, 0, 0, 2, 0.008f }, { 0, 0, 0.05f, 0.86f, 0, 0, 0, 2, 0.002f },
	{ 1, 0, 0, 0, 1, 0, 0, 2, 0.01f }, { 1, 0, 0, 0, 1, 0.06f, 0.85f, 2, 0.02f },
	{ 1, 0, 0, 0, 1, 0.2f, 0.6f, 2, 0.05f }, { 0, 0, 0.05f, 0.8f, 1, 0, 0.48f, 2, 0.016f },
	{ 0, 0, 0.06f, 0.9f, 1, 0, 0.52f, 2, 0.01f }, { 0, 0, 0.07f, 0.94f, 1, 0.3f, 0.6f, 2, 0.008f },
	{ 0, 0, 0, 0, 1, 0.3f, 0.42f, 2, 0 }, { 0, 0, 0, 0, 1, 0.35f, 0.48f, 2, 0 }, { 0, 0, 0, 0, 1, 0.38f, 0.6f, 2, 0 },
	{ 0, 0, 0.05f, 0.9f, 1, 0.2f, 0.28f, 0, 0 }, { 0, 0, 0.07f, 0.9f, 1, 0.3f, 0.4f, 0, 0 },
	{ 0, 0, 0.09f, 0.9f, 1, 0.35f, 0.5f, 0, 0 }, { 0, 1, 0.01f, 0.9f, 0, 0, 0, 2, 0.05f },
	{ 0, 0, 0, 0, 1, 0.009f, 0.999f, 2, 0.04f }, { 0, 0, 0.001f, 0.999f, 0, 0.2f, 0.8f, 2, 0.05f } };

cvar_t sxroom_type = { "room_type", "0", 0, 0.0f, nullptr };
cvar_t sxroomwater_type = { "waterroom_type", "14", 0, 0.0f, nullptr };
cvar_t sxroom_off = { "room_off", "0", FCVAR_ARCHIVE, 0.0f, nullptr };
float sxroom_typeprev;
int sxhiresprev;
extern cvar_t hisound;

constexpr int DELAY_FEEDBACK_SHIFT = 8;
constexpr int DELAY_OUTPUT_GAIN_SHIFT = 2;
constexpr int STEREO_DELAY_XFADE_SHIFT = 7;
constexpr int STEREO_DELAY_RANDOM_SHIFT = 9;
constexpr int STEREO_DELAY_RANDOM_MAX = 255;

void SXDLY_DoDelay( int count )
{
	dlyline_t* delayLine = &rgsxdly[ISXMONODLY];
	if ( !delayLine->lpdelayline )
		return;
	portable_samplepair_t* buffer = paintbuffer;
	while ( count-- )
	{
		sample_t delayed = delayLine->lpdelayline[delayLine->idelayoutput];
		int left = buffer->left;
		int right = buffer->right;
		if ( delayed || left || right )
		{
			int value = ( ( left + right ) >> 1 ) + ( ( delayed * delayLine->delayfeed ) >> DELAY_FEEDBACK_SHIFT );
			value = ( std::clamp )( value, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX );
			int filtered = value;
			if ( delayLine->lp )
			{
				filtered = ( delayLine->lp0 + delayLine->lp1 + value * 2 ) >> LOWPASS_GAIN_SHIFT;
				delayLine->lp0 = delayLine->lp1;
				delayLine->lp1 = value;
			}
			delayLine->lpdelayline[delayLine->idelayinput] = static_cast<sample_t>( filtered );
			int output = filtered >> DELAY_OUTPUT_GAIN_SHIFT;
			buffer->left = ( std::clamp )( left + output, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX );
			buffer->right = ( std::clamp )( right + output, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX );
		}
		else
		{
			delayLine->lp1 = 0;
			delayLine->lp0 = 0;
			delayLine->lpdelayline[delayLine->idelayinput] = 0;
		}
		if ( ++delayLine->idelayinput >= delayLine->cdelaysamplesmax )
			delayLine->idelayinput = 0;
		if ( ++delayLine->idelayoutput >= delayLine->cdelaysamplesmax )
			delayLine->idelayoutput = 0;
		++buffer;
	}
}

void SXDLY_DoStereoDelay( int count )
{
	dlyline_t* delayLine = &rgsxdly[ISXSTEREODLY];
	if ( !delayLine->lpdelayline )
		return;
	portable_samplepair_t* buffer = paintbuffer;
	while ( count-- )
	{
		if ( delayLine->mod && --delayLine->modcur < 0 )
			delayLine->modcur = delayLine->mod;
		sample_t delayed = delayLine->lpdelayline[delayLine->idelayoutput];
		int left = buffer->left;
		if ( delayLine->xfade || delayed || left )
		{
			if ( !delayLine->xfade && !delayLine->modcur && delayLine->mod )
			{
				delayLine->idelayoutputxf = delayLine->idelayoutput
											+ ( ( RandomLong( 0, STEREO_DELAY_RANDOM_MAX ) * delayLine->delaysamples )
												>> STEREO_DELAY_RANDOM_SHIFT );
				if ( delayLine->idelayoutputxf >= delayLine->cdelaysamplesmax )
					delayLine->idelayoutputxf -= delayLine->cdelaysamplesmax;
				delayLine->xfade = STEREO_DELAY_XFADE;
			}
			if ( delayLine->xfade )
			{
				int crossfade =
					( delayLine->lpdelayline[delayLine->idelayoutputxf] * ( STEREO_DELAY_XFADE - delayLine->xfade ) )
					>> STEREO_DELAY_XFADE_SHIFT;
				delayed =
					static_cast<sample_t>( ( ( delayed * delayLine->xfade ) >> STEREO_DELAY_XFADE_SHIFT ) + crossfade );
				if ( ++delayLine->idelayoutputxf >= delayLine->cdelaysamplesmax )
					delayLine->idelayoutputxf = 0;
				if ( --delayLine->xfade == 0 )
					delayLine->idelayoutput = delayLine->idelayoutputxf;
			}
			delayLine->lpdelayline[delayLine->idelayinput] =
				static_cast<sample_t>( ( std::clamp )( left, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX ) );
			buffer->left = delayed;
		}
		else
		{
			delayLine->lpdelayline[delayLine->idelayinput] = 0;
		}
		if ( ++delayLine->idelayinput >= delayLine->cdelaysamplesmax )
			delayLine->idelayinput = 0;
		if ( ++delayLine->idelayoutput >= delayLine->cdelaysamplesmax )
			delayLine->idelayoutput = 0;
		++buffer;
	}
}

void SX_RoomFX( int count, int fFilter, int fTimefx )
{
	if ( sxroom_off.value != 0.0f )
		return;

	sxhires = hisound.value != 0.0f ? 1 : 0;
	if ( sxhires != sxhiresprev )
	{
		sxhiresprev = sxhires;
		sxroom_typeprev = -1.0f;
		sxdly_delayprev = -1.0f;
		sxrvb_sizeprev = -1.0f;
		sxste_delayprev = -1.0f;
	}

	float roomType = cl.waterlevel > ROOM_EFFECTS_WATERLEVEL_THRESHOLD ? sxroomwater_type.value : sxroom_type.value;
	if ( roomType == sxroom_typeprev )
	{
		if ( roomType == 0.0f )
			return;
	}
	else
	{
		sxroom_typeprev = roomType;
		int index = static_cast<int>( roomType );
		if ( static_cast<unsigned int>( index ) < static_cast<unsigned int>( CSXROOM ) )
		{
			Cvar_SetValue( "room_lp", rgsxpre[index].room_lp );
			Cvar_SetValue( "room_mod", rgsxpre[index].room_mod );
			Cvar_SetValue( "room_size", rgsxpre[index].room_size );
			Cvar_SetValue( "room_refl", rgsxpre[index].room_refl );
			Cvar_SetValue( "room_rvblp", rgsxpre[index].room_rvblp );
			Cvar_SetValue( "room_delay", rgsxpre[index].room_delay );
			Cvar_SetValue( "room_feedback", rgsxpre[index].room_feedback );
			Cvar_SetValue( "room_dlylp", rgsxpre[index].room_dlylp );
			Cvar_SetValue( "room_left", rgsxpre[index].room_left );
		}
		SXRVB_CheckNewReverbVal();
		SXDLY_CheckNewDelayVal();
		SXDLY_CheckNewStereoDelayVal();
	}
	SXRVB_CheckNewReverbVal();
	SXDLY_CheckNewDelayVal();
	SXDLY_CheckNewStereoDelayVal();
	if ( fFilter )
		SXRVB_DoAMod( count );
	if ( fTimefx )
	{
		SXRVB_DoReverb( count );
		SXDLY_DoDelay( count );
		SXDLY_DoStereoDelay( count );
	}
}

constexpr int AMPLITUDE_MODRATE1 = 350;
constexpr int AMPLITUDE_MODRATE2 = 450;

void SX_Init()
{
	Q_memset( rgsxdly, 0, sizeof( rgsxdly ) );
	Q_memset( rgsxlp, 0, sizeof( rgsxlp ) );
	sxdly_delayprev = -1.0f;
	sxrvb_sizeprev = -1.0f;
	sxste_delayprev = -1.0f;
	sxroom_typeprev = -1.0f;
	sxamodr = AMPLITUDE_TARGET_MAX;
	sxamodl = AMPLITUDE_TARGET_MAX;
	sxamodrt = AMPLITUDE_TARGET_MAX;
	sxamodlt = AMPLITUDE_TARGET_MAX;
	sxhires = hisound.value == 0.0f ? 0 : 1;
	sxhiresprev = sxhires;
	if ( shm )
	{
		sxmod1 = AMPLITUDE_MODRATE1 * ( shm->speed / SOUND_11K );
		sxmod2 = AMPLITUDE_MODRATE2 * ( shm->speed / SOUND_11K );
	}
	sxmod1cur = sxmod1;
	sxmod2cur = sxmod2;
	Cvar_RegisterVariable( &sxdly_delay );
	Cvar_RegisterVariable( &sxdly_feedback );
	Cvar_RegisterVariable( &sxdly_lp );
	Cvar_RegisterVariable( &sxrvb_size );
	Cvar_RegisterVariable( &sxrvb_feedback );
	Cvar_RegisterVariable( &sxrvb_lp );
	Cvar_RegisterVariable( &sxste_delay );
	Cvar_RegisterVariable( &sxmod_lowpass );
	Cvar_RegisterVariable( &sxmod_mod );
	Cvar_RegisterVariable( &sxroom_type );
	Cvar_RegisterVariable( &sxroomwater_type );
	Cvar_RegisterVariable( &sxroom_off );
}

void SX_Free()
{
	for ( dlyline_t& delayLine : rgsxdly )
	{
		if ( delayLine.lpdelayline )
		{
#if defined( _WIN32 )
			GlobalUnlock( delayLine.hdelayline );
			GlobalFree( delayLine.hdelayline );
#else
			std::free( delayLine.hdelayline );
#endif
			delayLine.hdelayline = nullptr;
			delayLine.lpdelayline = nullptr;
		}
	}
}

void ResampleSfx( sfx_t* sound, int inputRate, int inputWidth, byte* data, int dataBytes );

void Wavstream_GetNextChunk( channel_t* channel, sfx_t* sound )
{
	constexpr int STREAM_BUFFER_BYTES = 32768;
	wavstream_t& stream = wavstreams[channel - channels];
	sfxcache_t* cache = static_cast<sfxcache_t*>( Cache_Check( &sound->cache ) );
	stream.lastposloaded = stream.info.dataofs + stream.csamplesplayed;
	int bytesRead;
	byte* data = COM_LoadFileLimit( nullptr, stream.lastposloaded, STREAM_BUFFER_BYTES, &bytesRead, &stream.hFile );
	stream.csamplesinmem = bytesRead;
	int samplesLeft = stream.info.samples - stream.csamplesplayed;
	if ( stream.csamplesinmem > samplesLeft )
		stream.csamplesinmem = samplesLeft;
	cache->length = stream.csamplesinmem;
	ResampleSfx( sound, cache->speed, cache->width, data, bytesRead );
}

qboolean S_CheckWavEnd( channel_t* channel, sfxcache_t** soundCache, int time, int channelIndex )
{
	sfxcache_t* cache = *soundCache;
	if ( cache->length == SOUND_CALLBACK_CACHE_LENGTH )
		return 0;
	if ( cache->loopstart >= 0 )
	{
		channel->pos = cache->loopstart;
		channel->end = cache->length - channel->pos + time;
		if ( channel->isentence >= 0 )
			rgrgvoxword[channel->isentence][channel->iword].samplefrac = 0;
		return 0;
	}
	if ( channel->entchannel == CHAN_STREAM )
	{
		wavstream_t& stream = wavstreams[channelIndex];
		if ( stream.csamplesplayed < stream.info.samples )
		{
			Wavstream_GetNextChunk( channel, channel->sfx );
			channel->pos = 0;
			channel->end = cache->length + time;
			return 0;
		}
	}
	else if ( channel->isentence >= 0 )
	{
		voxword_t* word = &rgrgvoxword[channel->isentence][channel->iword];
		if ( word->sfx && !word->fKeepCached )
			Cache_Free( &word->sfx->cache );
		channel->sfx = rgrgvoxword[channel->isentence][channel->iword + 1].sfx;
		if ( channel->sfx )
		{
			cache = S_LoadSound( channel->sfx, channel );
			*soundCache = cache;
			if ( cache )
			{
				channel->pos = 0;
				channel->end = cache->length + time;
				++channel->iword;
				VOX_TrimStartEndTimes( channel, cache );
				return 0;
			}
		}
	}
	S_FreeChannel( channel );
	return 1;
}

int VOX_FPaintPitchChannelFrom8Offs( portable_samplepair_t* output, channel_t* channel, sfxcache_t* cache, int count,
	int pitch, int timecompress, int offset )
{
	constexpr int MAX_SOUND_VOLUME = 255;
	constexpr int VOLUME_TABLE_SHIFT = 3;
	constexpr int SAMPLE_FRACTION_BITS = 8;
	constexpr float SAMPLE_FRACTION_SCALE = 256.0f;
	constexpr double PITCH_PERCENT_SCALE = 100.0;
	constexpr int TIME_COMPRESS_SEGMENTS = 8;
	constexpr int TIME_COMPRESS_PERCENT_SCALE = 100;
	constexpr int MAX_COMPRESSION_SCAN = 255;
	constexpr int QUIET_SAMPLE_LIMIT = 2;
	constexpr int PAINTBUFFER_SAMPLES = 512;
	if ( channel->isentence < 0 )
		return 0;
	if ( channel->leftvol > MAX_SOUND_VOLUME )
		channel->leftvol = MAX_SOUND_VOLUME;
	if ( channel->rightvol > MAX_SOUND_VOLUME )
		channel->rightvol = MAX_SOUND_VOLUME;
	int* leftScale = snd_scaletable[channel->leftvol >> VOLUME_TABLE_SHIFT];
	int* rightScale = snd_scaletable[channel->rightvol >> VOLUME_TABLE_SHIFT];
	portable_samplepair_t* buffer = &output[offset];
	voxword_t& word = rgrgvoxword[channel->isentence][channel->iword];
	float stepScale = static_cast<float>( static_cast<double>( pitch ) / PITCH_PERCENT_SCALE );
	int fractionStep = static_cast<int>( stepScale * SAMPLE_FRACTION_SCALE );
	int sampleFraction = word.samplefrac;
	int sourceSample = sampleFraction >> SAMPLE_FRACTION_BITS;
	sampleFraction += fractionStep;
	int painted = 0;

	if ( timecompress == 0 )
	{
		while ( painted < count && sourceSample < word.cbtrim )
		{
			byte sample = cache->data[sourceSample];
			buffer[painted].left += leftScale[sample];
			buffer[painted].right += rightScale[sample];
			sourceSample = sampleFraction >> SAMPLE_FRACTION_BITS;
			sampleFraction += fractionStep;
			++painted;
		}
	}
	else
	{
		int chunkSize = word.cbtrim / TIME_COMPRESS_SEGMENTS;
		int skipSamples = chunkSize * timecompress / TIME_COMPRESS_PERCENT_SCALE;
		while ( painted < count && sourceSample < word.cbtrim )
		{
			int chunkPosition = sourceSample % chunkSize;
			if ( sourceSample >= skipSamples && chunkPosition < skipSamples )
			{
				int scanned = 0;
				while ( painted < count && sourceSample < word.cbtrim && scanned < MAX_COMPRESSION_SCAN
						&& cache->data[sourceSample] > QUIET_SAMPLE_LIMIT )
				{
					byte sample = cache->data[sourceSample];
					buffer[painted].left += leftScale[sample];
					buffer[painted].right += rightScale[sample];
					sourceSample = sampleFraction >> SAMPLE_FRACTION_BITS;
					sampleFraction += fractionStep;
					++painted;
					++scanned;
				}
				if ( painted > PAINTBUFFER_SAMPLES )
					Con_DPrintf( "timecompress scan forward: overwrote paintbuffer!" );
				if ( sourceSample >= word.cbtrim || painted >= count )
					break;
				while ( true )
				{
					chunkPosition = sourceSample % chunkSize;
					if ( chunkPosition < skipSamples )
					{
						int skipped = skipSamples - chunkPosition;
						sourceSample += skipped;
						sampleFraction += skipped << SAMPLE_FRACTION_BITS;
					}
					int scannedSilentBoundary = 0;
					while ( sourceSample < word.cbtrim && scannedSilentBoundary < MAX_COMPRESSION_SCAN
							&& cache->data[sourceSample] > QUIET_SAMPLE_LIMIT )
					{
						sourceSample = sampleFraction >> SAMPLE_FRACTION_BITS;
						sampleFraction += fractionStep;
						++scannedSilentBoundary;
					}
					if ( sourceSample >= word.cbtrim )
						break;
					chunkPosition = sourceSample % chunkSize;
					if ( chunkPosition >= skipSamples )
						break;
				}
				if ( sourceSample >= word.cbtrim )
					break;
			}
			int remainingChunkSamples = chunkSize - sourceSample % chunkSize;
			int playCount = ( remainingChunkSamples << SAMPLE_FRACTION_BITS ) / fractionStep;
			if ( playCount + painted > count )
				playCount = count - painted;
			if ( playCount == 0 )
				playCount = 1;
			for ( int i = 0; i < playCount && painted < count && sourceSample < word.cbtrim; ++i )
			{
				byte sample = cache->data[sourceSample];
				buffer[painted].left += leftScale[sample];
				buffer[painted].right += rightScale[sample];
				sourceSample = sampleFraction >> SAMPLE_FRACTION_BITS;
				sampleFraction += fractionStep;
				++painted;
			}
		}
	}
	word.samplefrac = sampleFraction - fractionStep;
	int position = sampleFraction >> SAMPLE_FRACTION_BITS;
	int oldPosition = channel->pos;
	channel->pos = position;
	channel->end += oldPosition - position + painted;
	return sourceSample >= word.cbtrim;
}

float g_SND_VoiceOverdrive = 1.0f;

typedef int ( *soundDataCallback_t )(
	sfxcache_t* cache, char* output, int outputBytes, int samplePosition, int sampleCount );

void SND_MoveMouth( channel_t* channel, sfxcache_t* cache, int count )
{
	mouth_t& mouth = cl_entities[channel->entnum].mouth;
	int sampleCount = mouth.sndcount;
	int average = 0;
	byte* data = &cache->data[channel->pos];
	for ( int position = 0; position < count && sampleCount < MOUTH_AVERAGE_SAMPLES; )
	{
		int sample = static_cast<signed char>( data[position] );
		++sampleCount;
		average += std::abs( sample );
		position += MOUTH_SAMPLE_STEP + ( data[position] & MOUTH_SAMPLE_VARIATION_MASK );
	}
	int total = mouth.sndavg + average;
	mouth.sndcount = static_cast<byte>( sampleCount );
	mouth.sndavg = total;
	if ( mouth.sndcount >= MOUTH_AVERAGE_SAMPLES )
	{
		mouth.sndavg = 0;
		mouth.sndcount = 0;
		mouth.mouthopen = static_cast<byte>( total / MOUTH_AVERAGE_SAMPLES );
	}
}

void SND_PaintChannelFrom8( channel_t* channel, sfxcache_t* cache, int count )
{
	constexpr int MAX_SOUND_VOLUME = 255;
	constexpr int VOLUME_TABLE_SHIFT = 3;
	if ( channel->leftvol > MAX_SOUND_VOLUME )
		channel->leftvol = MAX_SOUND_VOLUME;
	if ( channel->rightvol > MAX_SOUND_VOLUME )
		channel->rightvol = MAX_SOUND_VOLUME;
	int* leftScale = snd_scaletable[channel->leftvol >> VOLUME_TABLE_SHIFT];
	int* rightScale = snd_scaletable[channel->rightvol >> VOLUME_TABLE_SHIFT];
	int position = channel->pos;
	if ( count > 0 )
	{
		byte* data = &cache->data[position];
		for ( int i = 0; i < count; ++i )
		{
			int sample = data[i];
			paintbuffer[i].left += leftScale[sample];
			paintbuffer[i].right += rightScale[sample];
		}
	}
	channel->pos = position + count;
}

static void SND_PaintChannelFrom8Offs(
	portable_samplepair_t* output, channel_t* channel, sfxcache_t* cache, int count, int offset )
{
	constexpr int MAX_SOUND_VOLUME = 255;
	constexpr int VOLUME_TABLE_SHIFT = 3;
	if ( channel->leftvol > MAX_SOUND_VOLUME )
		channel->leftvol = MAX_SOUND_VOLUME;
	if ( channel->rightvol > MAX_SOUND_VOLUME )
		channel->rightvol = MAX_SOUND_VOLUME;
	int* leftScale = snd_scaletable[channel->leftvol >> VOLUME_TABLE_SHIFT];
	int* rightScale = snd_scaletable[channel->rightvol >> VOLUME_TABLE_SHIFT];
	byte* data = &cache->data[channel->pos];
	for ( int i = 0; i < count; ++i )
	{
		output[offset + i].left += leftScale[data[i]];
		output[offset + i].right += rightScale[data[i]];
	}
}

static void SND_PaintChannelFrom16Offs(
	portable_samplepair_t* output, channel_t* channel, short* data, int count, int offset )
{
	constexpr int SOUND_VOLUME_SHIFT = 8;
	for ( int i = 0; i < count; ++i )
	{
		output[offset + i].left += ( data[i] * channel->leftvol ) >> SOUND_VOLUME_SHIFT;
		output[offset + i].right += ( data[i] * channel->rightvol ) >> SOUND_VOLUME_SHIFT;
	}
}

void S_MixChannelsToPaintbuffer( int end, int fPaintHiresSounds, int voiceOnly )
{
	constexpr int VOICE_READ_BUFFER_BYTES = 4096;
	constexpr int NORMAL_SOUND_PITCH = 100;
	constexpr int PCM_8BIT_BYTES = 1;
	constexpr int PCM_16BIT_BYTES = 2;
	int hiresShift = fPaintHiresSounds != 0;
	char voiceBuffer[VOICE_READ_BUFFER_BYTES];
	for ( int index = 0; index < total_channels; ++index )
	{
		channel_t* channel = &channels[index];
		if ( !channel->sfx )
			continue;
		sfxcache_t* cache = S_LoadSound( channel->sfx, channel );
		int soundLength = cache->length;
		bool callbackSound = soundLength == SOUND_CALLBACK_CACHE_LENGTH;
		if ( callbackSound ? !voiceOnly : voiceOnly != 0 )
			continue;
		if ( !channel->leftvol && !channel->rightvol )
		{
			if ( cache->loopstart < 0 )
				S_FreeChannel( channel );
			continue;
		}
		if ( fPaintHiresSounds ? cache->speed <= shm->speed : cache->speed != shm->speed )
			continue;

		int time = paintedtime;
		int pitch = channel->pitch;
		int timecompress = 0;
		if ( channel->isentence >= 0 )
		{
			voxword_t& word = rgrgvoxword[channel->isentence][channel->iword];
			if ( word.pitch > 0 )
				pitch += word.pitch - NORMAL_SOUND_PITCH;
			timecompress = word.timecompress;
		}
		while ( time < end )
		{
			int waveEnd = channel->end;
			if ( fPaintHiresSounds )
				waveEnd += ( cache->length >> 1 ) - cache->length;
			int count = waveEnd < end ? waveEnd - time : end - time;
			if ( callbackSound && ( cache->width != PCM_16BIT_BYTES || cache->stereo != 0 ) )
				break;
			count <<= hiresShift;
			int offset = ( time - paintedtime ) << hiresShift;
			int pitchEnded = 0;
			if ( count > 0 )
			{
				if ( cache->width == PCM_8BIT_BYTES && channel->entnum > 0
					 && ( channel->entchannel == CHAN_VOICE || channel->entchannel == CHAN_STREAM ) )
					SND_MoveMouth( channel, cache, count );
				int leftVolume = channel->leftvol;
				int rightVolume = channel->rightvol;
				if ( !callbackSound )
				{
					channel->leftvol = static_cast<int>( static_cast<float>( leftVolume ) / g_SND_VoiceOverdrive );
					channel->rightvol = static_cast<int>( static_cast<float>( rightVolume ) / g_SND_VoiceOverdrive );
				}
				if ( cache->width != PCM_8BIT_BYTES )
				{
					short* data;
					if ( callbackSound )
					{
						soundDataCallback_t callback = reinterpret_cast<soundDataCallback_t>( cache->loopstart );
						count = callback( cache, voiceBuffer, sizeof( voiceBuffer ), channel->pos, count );
						data = reinterpret_cast<short*>( voiceBuffer );
					}
					else
						data = reinterpret_cast<short*>( cache->data ) + channel->pos;
					if ( count > 0 )
						SND_PaintChannelFrom16Offs( paintbuffer, channel, data, count, offset );
					channel->pos += count;
				}
				else if ( ( pitch != NORMAL_SOUND_PITCH || timecompress != 0 ) && channel->isentence >= 0 )
					pitchEnded = VOX_FPaintPitchChannelFrom8Offs(
						paintbuffer, channel, cache, count, pitch, timecompress, offset );
				else if ( time != paintedtime )
				{
					SND_PaintChannelFrom8Offs( paintbuffer, channel, cache, count, offset );
					channel->pos += count;
				}
				else
					SND_PaintChannelFrom8( channel, cache, count );
				channel->leftvol = leftVolume;
				channel->rightvol = rightVolume;
				time += count >> hiresShift;
				if ( channel->entchannel == CHAN_STREAM )
					wavstreams[index].csamplesplayed += count;
			}
			if ( ( time >= waveEnd || pitchEnded ) && S_CheckWavEnd( channel, &cache, time, index ) )
				break;
		}
	}
}

extern LPDIRECTSOUNDBUFFER pDSBuf;
extern DWORD gSndBufSize;
extern cvar_t volume;

static int s_careerAudioPaused;

void CareerAudio_Command_f()
{
	s_careerAudioPaused = 0;
}
int* snd_p;
short* snd_out;
int snd_vol;
int snd_linear_count;

void SetCareerAudioState( int paused )
{
	s_careerAudioPaused = paused;
}

void Snd_WriteLinearBlastStereo16()
{
	constexpr int SOUND_VOLUME_SHIFT = 8;
	for ( int i = 0; i < snd_linear_count; i += 2 )
	{
		int left = ( snd_p[i] * snd_vol ) >> SOUND_VOLUME_SHIFT;
		int right = ( snd_p[i + 1] * snd_vol ) >> SOUND_VOLUME_SHIFT;
		snd_out[i] = static_cast<short>( ( std::clamp )( left, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX ) );
		snd_out[i + 1] = static_cast<short>( ( std::clamp )( right, SOUND_SAMPLE_MIN, SOUND_SAMPLE_MAX ) );
	}
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4701 )
#endif
void S_TransferStereo16( int end )
{
	constexpr float SOUND_VOLUME_SCALE_FLOAT = 256.0f;
	constexpr int STEREO_CHANNELS = 2;
	constexpr int SOUND_BUFFER_LOCK_RETRIES = 10000;
	void* data;
	DWORD dataSize;
	void* secondData;
	DWORD secondSize;

	snd_vol = static_cast<int>( volume.value * SOUND_VOLUME_SCALE_FLOAT );
	snd_p = reinterpret_cast<int*>( paintbuffer );
	int position = paintedtime * STEREO_CHANNELS;
	int endPosition = end * STEREO_CHANNELS;

	if ( pDSBuf )
	{
		int retries = 0;
		HRESULT result;
		while ( ( result = pDSBuf->Lock( 0, gSndBufSize, &data, &dataSize, &secondData, &secondSize, 0 ) ) != DS_OK )
		{
			if ( result != DSERR_BUFFERLOST )
			{
				Con_Printf( "S_TransferStereo16: DS::Lock Sound Buffer Failed\n" );
				S_Shutdown();
				S_Startup();
				return;
			}
			if ( ++retries > SOUND_BUFFER_LOCK_RETRIES )
			{
				Con_Printf( "S_TransferStereo16: DS: couldn't restore buffer\n" );
				S_Shutdown();
				S_Startup();
				return;
			}
		}
	}
	else
	{
		data = shm->buffer;
	}

	while ( position < endPosition )
	{
		int frameCount = shm->samples >> 1;
		int frame = position & ( frameCount - 1 );
		snd_out = static_cast<short*>( data ) + frame * STEREO_CHANNELS;
		int count = frameCount - frame;
		if ( count > endPosition - position )
			count = endPosition - position;
		snd_linear_count = count * STEREO_CHANNELS;
		// write a linear blast of samples
		Snd_WriteLinearBlastStereo16();
		snd_p += snd_linear_count;
		position += snd_linear_count >> 1;
	}

	if ( pDSBuf )
		pDSBuf->Unlock( data, dataSize, nullptr, 0 );
}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif

void S_TransferPaintBuffer( int endtime )
{
#if defined( _WIN32 )
	if ( shm->samplebits != 16 || shm->channels != 2 )
		_wassert( L"shm->samplebits == 16 && shm->channels == 2", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\snd_mix.c", 456 );
	S_TransferStereo16( endtime );
#else
	constexpr float SOUND_VOLUME_SCALE_FLOAT = 256.0f;
	constexpr int STEREO_CHANNELS = 2;
	snd_p = reinterpret_cast<int*>( paintbuffer );
	snd_vol = static_cast<int>( SOUND_VOLUME_SCALE_FLOAT * volume.value );
	int position = paintedtime * STEREO_CHANNELS;
	int endPosition = endtime * STEREO_CHANNELS;
	unsigned char* buffer = shm->buffer;
	while ( position < endPosition )
	{
		int frameCount = shm->samples >> 1;
		int frame = position & ( frameCount - 1 );
		int count = frameCount - frame;
		if ( count > endPosition - position )
			count = endPosition - position;
		snd_linear_count = count * STEREO_CHANNELS;
		snd_out = reinterpret_cast<short*>( buffer ) + frame * STEREO_CHANNELS;
		Snd_WriteLinearBlastStereo16();
		position += snd_linear_count >> 1;
		snd_p += snd_linear_count;
	}
#endif
}

void S_PaintChannels( int endtime )
{
	constexpr int PAINTBUFFER_SAMPLES = 512;
	constexpr int STEREO_CHANNELS = 2;
	static portable_samplepair_t paintprev;
	while ( paintedtime < endtime )
	{
		int end = endtime;
		if ( endtime - paintedtime > PAINTBUFFER_SAMPLES )
			end = paintedtime + PAINTBUFFER_SAMPLES;
		int count = end - paintedtime;
		Q_memset( paintbuffer, 0, count * sizeof( *paintbuffer ) );
		if ( !cl.paused && !s_careerAudioPaused )
			S_MixChannelsToPaintbuffer( end, 0, 0 );
		if ( hisound.value == 0.0f )
			SX_RoomFX( count, 1, 1 );
		for ( int i = count - 1; i > 0; --i )
		{
			paintbuffer[2 * i + 1] = paintbuffer[i];
			paintbuffer[2 * i].left = ( paintbuffer[i - 1].left + paintbuffer[i].left ) >> 1;
			paintbuffer[2 * i].right = ( paintbuffer[i - 1].right + paintbuffer[i].right ) >> 1;
		}
		paintbuffer[1] = paintbuffer[0];
		paintbuffer[0].left = ( paintprev.left + paintbuffer[0].left ) >> 1;
		paintbuffer[0].right = ( paintprev.right + paintbuffer[0].right ) >> 1;
		paintprev = paintbuffer[count * STEREO_CHANNELS - 1];
		if ( hisound.value > 0.0f )
		{
			if ( !cl.paused && !s_careerAudioPaused )
				S_MixChannelsToPaintbuffer( end, 1, 0 );
			SX_RoomFX( count * STEREO_CHANNELS, 1, 1 );
		}
		if ( !cl.paused && !s_careerAudioPaused )
			S_MixChannelsToPaintbuffer( end, 1, 1 );
		if ( s_rawend >= paintedtime )
		{
			int rawEnd = end < s_rawend ? end : s_rawend;
			for ( int time = paintedtime; time < rawEnd; ++time )
			{
				portable_samplepair_t& raw = s_rawsamples[time & ( MAX_RAW_SAMPLES - 1 )];
				paintbuffer[time - paintedtime].left += raw.left / STEREO_CHANNELS;
				paintbuffer[time - paintedtime].right += raw.right / STEREO_CHANNELS;
			}
		}
		// transfer out according to DMA format
		S_TransferPaintBuffer( end );
		paintedtime = end;
	}
}
