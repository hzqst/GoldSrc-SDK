#include "sound.h"
#include "conprint.h"
#include "common.h"
#include "cvar.h"
#include "host.h"
#include "sys.h"
#include "zone.h"
#include "bspfile.h"
#include "protocol.h"
#include "client.h"
#include "cmd.h"
#include "gl_model.h"
#include "vgui_intwrap2.h"
#include "../external/SDL2/SDL.h"
#include "../common/const.h"
#include "mem.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#endif

#include <climits>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>

extern "C" void VOX_Shutdown();
extern "C" void SNDDMA_Shutdown();
extern "C" qboolean SNDDMA_Init();
extern "C" void S_FreeChannel( channel_t* channel );
extern "C" qboolean Wavstream_Init();
#ifdef _WIN32
extern LPDIRECTSOUNDBUFFER pDSBuf;
extern DWORD gSndBufSize;
#endif

constexpr int SOUND_UNSIGNED_SAMPLE_BITS = 8;
constexpr int SOUND_UNSIGNED_SILENCE = 128;
constexpr int SOUND_BITS_PER_BYTE = 8;
constexpr int SOUND_BUFFER_LOCK_RETRIES = 10000;

volatile dma_t* shm;
volatile dma_t sn;
wavstream_t wavstreams[MAX_CHANNELS];
// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t channels[MAX_CHANNELS];
qboolean fakedma;
qboolean snd_initialized;
int sound_started;
int total_channels;
int paintedtime; // sample PAIRS
int s_rawend;
portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];
constexpr int MAX_SFX = 1024;
sfx_t* known_sfx;
int num_sfx;
cvar_t hisound = { const_cast<char*>( "hisound" ), const_cast<char*>( "1.0" ), FCVAR_ARCHIVE, 0.0f, nullptr };
cvar_t loadas8bit = { const_cast<char*>( "loadas8bit" ), const_cast<char*>( "0" ), 0, 0.0f, nullptr };
cvar_t volume = {
	const_cast<char*>( "volume" ), const_cast<char*>( "0.7" ), FCVAR_ARCHIVE | FCVAR_FILTERSTUFFTEXT, 0.0f, nullptr };

sfx_t* S_FindName( char* name, int* inCache )
{
	if ( !name )
		Sys_Error( "S_FindName: NULL\n" );
	if ( Q_strlen( name ) >= SOUND_NAME_BYTES )
		Sys_Error( "Sound name too long: %s", name );

	sfx_t* available = nullptr;
	for ( int i = 0; i < num_sfx; ++i )
	{
		sfx_t* sound = &known_sfx[i];
		if ( !Q_strcasecmp( sound->name, name ) )
		{
			if ( inCache )
				*inCache = Cache_Check( &sound->cache ) != nullptr;
			if ( sound->servercount > 0 )
				sound->servercount = gHostSpawnCount;
			return sound;
		}
		if ( !available && sound->servercount > 0 && sound->servercount != gHostSpawnCount )
			available = sound;
	}

	if ( available )
	{
		if ( Cache_Check( &available->cache ) )
			Cache_Free( &available->cache );
	}
	else
	{
		if ( num_sfx == MAX_SFX )
			Sys_Error( "S_FindName: out of sfx_t" );
		available = &known_sfx[num_sfx++];
	}
	Q_strncpy( available->name, name, sizeof( available->name ) - 1 );
	available->name[sizeof( available->name ) - 1] = 0;
	if ( inCache )
		*inCache = 0;
	available->servercount = gHostSpawnCount;
	return available;
}

extern "C" void S_Startup()
{
	if ( snd_initialized )
	{
		if ( fakedma || SNDDMA_Init() )
		{
			sound_started = 1;
		}
		else
		{
			Con_Printf( "S_Startup: SNDDMA_Init failed.\n" );
			sound_started = 0;
		}
	}
}

extern "C" void S_Shutdown()
{
	VOX_Shutdown();
	if ( sound_started )
	{
		if ( shm )
			shm->gamealive = 0;
		shm = nullptr;
		sound_started = 0;
		if ( !fakedma )
			SNDDMA_Shutdown();
	}
}

extern "C" void S_ClearBuffer()
{
#ifdef _WIN32
	if ( !sound_started || !shm || ( !shm->buffer && !pDSBuf ) )
#else
	if ( !sound_started || !shm || !shm->buffer )
#endif
		return;

	int clear = shm->samplebits == SOUND_UNSIGNED_SAMPLE_BITS ? SOUND_UNSIGNED_SILENCE : 0;
#ifdef _WIN32
	if ( pDSBuf )
	{
		void* data;
		DWORD size;
		int retries = 0;
		HRESULT result;

		while ( ( result = pDSBuf->Lock( 0, gSndBufSize, &data, &size, nullptr, nullptr, 0 ) ) != DS_OK )
		{
			if ( result != DSERR_BUFFERLOST )
			{
				Con_Printf( "S_ClearBuffer: DS::Lock Sound Buffer Failed\n" );
				S_Shutdown();
				return;
			}
			if ( ++retries > SOUND_BUFFER_LOCK_RETRIES )
			{
				Con_Printf( "S_ClearBuffer: DS: couldn't restore buffer\n" );
				S_Shutdown();
				return;
			}
		}

		Q_memset( data, clear, shm->samplebits * shm->samples / SOUND_BITS_PER_BYTE );
		pDSBuf->Unlock( data, size, nullptr, 0 );
	}
	else
#endif
	{
		Q_memset( shm->buffer, clear, shm->samplebits * shm->samples / SOUND_BITS_PER_BYTE );
	}
}

void S_StopAllSounds( qboolean clear )
{
	if ( !sound_started )
		return;
	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
	for ( int index = 0; index < MAX_CHANNELS; ++index )
	{
		if ( channels[index].sfx )
			S_FreeChannel( &channels[index] );
	}
	Q_memset( channels, 0, sizeof( channels ) );
	Wavstream_Init();
	if ( clear )
		S_ClearBuffer();
}

void S_StopAllSoundsC()
{
	S_StopAllSounds( 1 );
}

// Stop all sounds for entity on a channel.
void S_StopSound( int entnum, int entchannel )
{
	for ( int index = NUM_AMBIENTS; index < total_channels; ++index )
	{
		if ( channels[index].entnum == entnum && channels[index].entchannel == entchannel )
			S_FreeChannel( &channels[index] );
	}
}

int S_AlterChannel( int entnum, int entchannel, sfx_t* sfx, int vol, int pitch, int flags )
{
	bool sentence = sfx->name[0] == CHAR_SENTENCE;
	for ( int index = NUM_AMBIENTS; index < total_channels; ++index )
	{
		channel_t* channel = &channels[index];
		if ( channel->entnum != entnum || channel->entchannel != entchannel )
			continue;
		if ( sentence )
		{
			if ( !channel->sfx || channel->isentence < 0 )
				continue;
		}
		else if ( channel->sfx != sfx )
		{
			continue;
		}
		if ( flags & SND_CHANGE_PITCH )
			channel->pitch = pitch;
		if ( flags & SND_CHANGE_VOL )
			channel->master_vol = vol;
		if ( flags & SND_STOP )
			S_FreeChannel( channel );
		return 1;
	}
	return 0;
}

extern "C" void VOX_Init();
void SND_InitScaletable();
void S_Play();
void S_PlayVol();
void S_Say();
void S_Say_Reliable();
void S_SoundList();
void S_SoundInfo_f();

extern cvar_t ambient_level;
extern cvar_t ambient_fade;
extern cvar_t snd_show;
extern cvar_t _snd_mixahead;

cvar_t s_show = { const_cast<char*>( "s_show" ), const_cast<char*>( "0" ), 0, 0.0f, nullptr };
cvar_t nosound = { const_cast<char*>( "nosound" ), const_cast<char*>( "0" ), 0, 0.0f, nullptr };
cvar_t suitvolume = { const_cast<char*>( "suitvolume" ), const_cast<char*>( "0.25" ), FCVAR_ARCHIVE, 0.0f, nullptr };
cvar_t bgmvolume = { const_cast<char*>( "bgmvolume" ), const_cast<char*>( "1" ), FCVAR_ARCHIVE, 0.0f, nullptr };
cvar_t MP3Volume = { const_cast<char*>( "MP3Volume" ), const_cast<char*>( "0.8" ),
	FCVAR_ARCHIVE | FCVAR_FILTERSTUFFTEXT, 0.0f, nullptr };
cvar_t MP3FadeTime = { const_cast<char*>( "MP3FadeTime" ), const_cast<char*>( "2.0" ), FCVAR_ARCHIVE, 0.0f, nullptr };
cvar_t snd_noextraupdate = { const_cast<char*>( "snd_noextraupdate" ), const_cast<char*>( "1" ), 0, 0.0f, nullptr };
cvar_t speak_enable = {
	const_cast<char*>( "speak_enabled" ), const_cast<char*>( "1" ), FCVAR_FILTERSTUFFTEXT, 0.0f, nullptr };

constexpr int SOUND_MINIMUM_16BIT_MEMORY = 8 * 1024 * 1024;
constexpr int SOUND_DMA_SPEED = 22050;
constexpr int SOUND_SIMULATED_CHANNELS = 2;
constexpr int SOUND_SIMULATED_SAMPLE_BITS = 16;
constexpr int SOUND_SIMULATED_SAMPLES = 32768;
constexpr int SOUND_SIMULATED_BUFFER_BYTES = SOUND_SIMULATED_SAMPLES * sizeof( short );

void S_Init()
{
	Con_DPrintf( "Sound Initialization\n" );
	VOX_Init();
	if ( COM_CheckParm( "-nosound" ) )
		return;
	if ( COM_CheckParm( "-simsound" ) )
		fakedma = 1;
	Cmd_AddCommand( "play", S_Play );
	Cmd_AddCommand( "playvol", S_PlayVol );
	Cmd_AddCommand( "speak", S_Say );
	Cmd_AddCommand( "spk", S_Say_Reliable );
	Cmd_AddCommand( "stopsound", S_StopAllSoundsC );
	Cmd_AddCommand( "soundlist", S_SoundList );
	Cmd_AddCommand( "soundinfo", S_SoundInfo_f );
	Cvar_RegisterVariable( &s_show );
	Cvar_RegisterVariable( &nosound );
	Cvar_RegisterVariable( &volume );
	Cvar_RegisterVariable( &suitvolume );
	Cvar_RegisterVariable( &hisound );
	Cvar_RegisterVariable( &loadas8bit );
	Cvar_RegisterVariable( &bgmvolume );
	Cvar_RegisterVariable( &MP3Volume );
	Cvar_RegisterVariable( &MP3FadeTime );
	Cvar_RegisterVariable( &ambient_level );
	Cvar_RegisterVariable( &ambient_fade );
	Cvar_RegisterVariable( &snd_noextraupdate );
	Cvar_RegisterVariable( &snd_show );
	Cvar_RegisterVariable( &_snd_mixahead );
	Cvar_RegisterVariable( &speak_enable );
	if ( host_parms.memsize < SOUND_MINIMUM_16BIT_MEMORY )
	{
		Cvar_Set( "loadas8bit", "1" );
		Con_DPrintf( "loading all sounds as 8bit\n" );
	}
	snd_initialized = 1;
	S_Startup();
	SND_InitScaletable();
	known_sfx = static_cast<sfx_t*>( Hunk_AllocName( MAX_SFX * sizeof( sfx_t ), "sfx_t" ) );
	num_sfx = 0;
	if ( fakedma )
	{
		shm = static_cast<volatile dma_t*>( Hunk_AllocName( sizeof( dma_t ), "shm" ) );
		shm->splitbuffer = 0;
		shm->samplebits = SOUND_SIMULATED_SAMPLE_BITS;
		shm->speed = SOUND_DMA_SPEED;
		shm->channels = SOUND_SIMULATED_CHANNELS;
		shm->samples = SOUND_SIMULATED_SAMPLES;
		shm->samplepos = 0;
		shm->soundalive = 1;
		shm->gamealive = 1;
		shm->submission_chunk = 1;
		shm->buffer = static_cast<unsigned char*>( Hunk_AllocName( SOUND_SIMULATED_BUFFER_BYTES, "shmbuf" ) );
		Con_DPrintf( "Sound sampling rate: %i\n", shm->speed );
	}
	S_StopAllSounds( 1 );
	SX_Init();
	Wavstream_Init();
}

void S_BeginPrecaching()
{
	cl.fPrecaching = 1;
}

void S_EndPrecaching()
{
	cl.fPrecaching = 0;
}

extern "C" void S_ClearPrecache()
{
}

extern cvar_t fs_lazy_precache;
extern "C" sfxcache_t* VOX_LoadSound( channel_t* channel, char* name );
extern "C" void VOX_MakeSingleWordSentence( channel_t* channel, int pitch );
extern "C" void VOX_TrimStartEndTimes( channel_t* channel, sfxcache_t* cache );
void SND_Spatialize( channel_t* channel );
void SND_InitMouth( int entnum, int entchannel );
int RandomLong( int low, int high );

constexpr char CHAR_STREAM = '*';
constexpr char CHAR_DRYMIX = '#';
constexpr int SOUND_MASTER_VOLUME_MAX = 255;
constexpr float SOUND_MASTER_VOLUME_SCALE = 255.0f;
constexpr float SOUND_TIMESCALE_PITCH_GAIN = 0.5f;
constexpr double SOUND_IDENTICAL_START_DELAY = 0.1;
constexpr int SOUND_CHANNEL_OVERRIDE_ALL = -1;
float sound_nominal_clip_dist = 1000.0f;

int SND_FStreamIsPlaying( sfx_t* sfx )
{
	for ( int index = NUM_AMBIENTS; index < total_channels; ++index )
	{
		if ( channels[index].sfx == sfx )
			return 1;
	}
	return 0;
}

channel_t* SND_PickDynamicChannel( int entnum, int entchannel, sfx_t* sfx )
{
	if ( entchannel == CHAN_STREAM && SND_FStreamIsPlaying( sfx ) )
		return nullptr;

	int firstToDie = -1;
	int lifeLeft = std::numeric_limits<int>::max();
	for ( int index = NUM_AMBIENTS; index < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ++index )
	{
		channel_t* channel = &channels[index];
		if ( channel->entchannel == CHAN_STREAM && wavstreams[index].hFile )
		{
			if ( entchannel == CHAN_VOICE )
				return nullptr;
			continue;
		}
		if ( entchannel != CHAN_AUTO && channel->entnum == entnum
			 && ( channel->entchannel == entchannel || entchannel == SOUND_CHANNEL_OVERRIDE_ALL ) )
		{
			firstToDie = index;
			break;
		}
		if ( channel->entnum == cl.viewentity && entnum != cl.viewentity && channel->sfx )
			continue;
		int remaining = channel->end - paintedtime;
		if ( remaining < lifeLeft )
		{
			lifeLeft = remaining;
			firstToDie = index;
		}
	}
	if ( firstToDie == -1 )
		return nullptr;

	channel_t* channel = &channels[firstToDie];
	if ( channel->sfx )
	{
		S_FreeChannel( channel );
		channel->sfx = nullptr;
	}
	return channel;
}

channel_t* SND_PickStaticChannel( int, int, sfx_t* sfx )
{
	if ( sfx->name[0] == CHAR_STREAM && SND_FStreamIsPlaying( sfx ) )
		return nullptr;
	for ( int index = NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; index < total_channels; ++index )
	{
		if ( !channels[index].sfx )
			return &channels[index];
	}
	// no empty slots, alloc a new static sound channel
	if ( total_channels == MAX_CHANNELS )
	{
		Con_DPrintf( "total_channels == MAX_CHANNELS\n" );
		return nullptr;
	}
	return &channels[total_channels++];
}

sfx_t* S_PrecacheSound( char* name )
{
	if ( !sound_started || nosound.value != 0.0f || Q_strlen( name ) >= SOUND_NAME_BYTES )
		return nullptr;
	if ( name[0] == CHAR_STREAM || name[0] == CHAR_SENTENCE )
		return S_FindName( name, nullptr );

	sfx_t* sound = S_FindName( name, nullptr );
	if ( fs_lazy_precache.value == 0.0f )
		S_LoadSound( sound, nullptr );
	return sound;
}

void S_StartDynamicSound(
	int entnum, int entchannel, sfx_t* sfx, vec_t* origin, float fvol, float attenuation, int flags, int pitch )
{
	if ( !sound_started || !sfx || nosound.value != 0.0f )
		return;
	if ( sfx->name[0] == CHAR_STREAM )
		entchannel = CHAN_STREAM;
	if ( entchannel == CHAN_STREAM && pitch != PITCH_NORM )
	{
		Con_DPrintf( "Warning: pitch shift ignored on stream sound %s\n", sfx->name );
		pitch = PITCH_NORM;
	}
	int channelVolume = static_cast<int>( fvol * SOUND_MASTER_VOLUME_SCALE );
	if ( channelVolume > SOUND_MASTER_VOLUME_MAX )
	{
		Con_DPrintf( "S_StartDynamicSound: %s volume > 255", sfx->name );
		channelVolume = SOUND_MASTER_VOLUME_MAX;
	}
	if ( flags & ( SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH ) )
	{
		if ( S_AlterChannel( entnum, entchannel, sfx, channelVolume, pitch, flags ) || ( flags & SND_STOP ) )
			return;
	// fall through - if we're not trying to stop the sound, 
	// and we didn't find it (it's not playing), go ahead and start it up
	}
	if ( pitch == 0 )
	{
		Con_DPrintf( "Warning: S_StartDynamicSound Ignored, called with pitch 0" );
		return;
	}
	float pitchScale = sys_timescale.value + 1.0f;
	pitchScale *= SOUND_TIMESCALE_PITCH_GAIN;
	pitchScale *= static_cast<float>( pitch );
	int effectivePitch = static_cast<int>( pitchScale );
	channel_t* channel = SND_PickDynamicChannel( entnum, entchannel, sfx );
	if ( !channel )
		return;
	bool sentence = sfx->name[0] == CHAR_SENTENCE || sfx->name[0] == CHAR_DRYMIX;
	Q_memset( channel, 0, sizeof( *channel ) );
	float distanceMultiplier = attenuation / sound_nominal_clip_dist;
	channel->origin[0] = origin[0];
	channel->origin[1] = origin[1];
	channel->origin[2] = origin[2];
	channel->master_vol = channelVolume;
	channel->entnum = entnum;
	channel->entchannel = entchannel;
	channel->pitch = effectivePitch;
	channel->isentence = -1;
	channel->dist_mult = distanceMultiplier;
	SND_Spatialize( channel );
	if ( !channel->leftvol && !channel->rightvol && entchannel != CHAN_STREAM )
	{
		channel->sfx = nullptr;
		return;		// not audible at all
	}
	sfxcache_t* cache;
	if ( sentence )
	{
		char name[SOUND_NAME_BYTES];
		Q_strncpy( name, sfx->name + 1, sizeof( name ) - 1 );
		name[sizeof( name ) - 1] = 0;
		cache = VOX_LoadSound( channel, name );
	}
	else
	{
		cache = S_LoadSound( sfx, channel );
		channel->sfx = sfx;
	}
	if ( !cache )
	{
		channel->sfx = nullptr;
		return;		// couldn't load the sound's data
	}
	channel->pos = 0;
	channel->end = cache->length + paintedtime;
	if ( !sentence && effectivePitch != PITCH_NORM )
		VOX_MakeSingleWordSentence( channel, effectivePitch );
	VOX_TrimStartEndTimes( channel, cache );
	SND_InitMouth( entnum, entchannel );
	for ( int index = NUM_AMBIENTS; index < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ++index )
	{
		channel_t* other = &channels[index];
		if ( other != channel && other->sfx == sfx && other->pos == 0 )
		{
			int skip =
				RandomLong( 0, static_cast<int>( static_cast<double>( shm->speed ) * SOUND_IDENTICAL_START_DELAY ) );
			if ( skip >= channel->end )
				skip = channel->end - 1;
			channel->pos += skip;
			channel->end -= skip;
			break;
		}
	}
}

void S_StartStaticSound(
	int entnum, int entchannel, sfx_t* sfx, vec_t* origin, float fvol, float attenuation, int flags, int pitch )
{
	if ( !sfx )
		return;
	if ( sfx->name[0] == CHAR_STREAM )
		entchannel = CHAN_STREAM;
	if ( entchannel == CHAN_STREAM && pitch != PITCH_NORM )
	{
		Con_DPrintf( "Warning: pitch shift ignored on stream sound %s\n", sfx->name );
		pitch = PITCH_NORM;
	}
	int channelVolume = static_cast<int>( fvol * SOUND_MASTER_VOLUME_SCALE );
	if ( channelVolume > SOUND_MASTER_VOLUME_MAX )
	{
		Con_DPrintf( "S_StartStaticSound: %s volume > 255", sfx->name );
		channelVolume = SOUND_MASTER_VOLUME_MAX;
	}
	if ( flags & ( SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH ) )
	{
		if ( S_AlterChannel( entnum, entchannel, sfx, channelVolume, pitch, flags ) || ( flags & SND_STOP ) )
			return;
	}
	if ( pitch == 0 )
	{
		Con_DPrintf( "Warning: S_StartStaticSound Ignored, called with pitch 0" );
		return;
	}
	float pitchScale = sys_timescale.value + 1.0f;
	pitchScale *= SOUND_TIMESCALE_PITCH_GAIN;
	pitchScale *= static_cast<float>( pitch );
	int effectivePitch = static_cast<int>( pitchScale );
	channel_t* channel = SND_PickStaticChannel( entnum, entchannel, sfx );
	if ( !channel )
		return;
	bool sentence = sfx->name[0] == CHAR_SENTENCE || sfx->name[0] == CHAR_DRYMIX;
	sfxcache_t* cache;
	if ( sentence )
	{
		char name[SOUND_NAME_BYTES];
		Q_strncpy( name, sfx->name + 1, sizeof( name ) - 1 );
		name[sizeof( name ) - 1] = 0;
		cache = VOX_LoadSound( channel, name );
	}
	else
	{
		cache = S_LoadSound( sfx, channel );
		channel->sfx = sfx;
		channel->isentence = -1;
	}
	if ( !cache )
	{
		channel->sfx = nullptr;
		return;
	}
	float distanceMultiplier = attenuation / sound_nominal_clip_dist;
	channel->origin[0] = origin[0];
	channel->origin[1] = origin[1];
	channel->origin[2] = origin[2];
	channel->master_vol = channelVolume;
	channel->pos = 0;
	channel->dist_mult = distanceMultiplier;
	channel->end = cache->length + paintedtime;
	channel->entnum = entnum;
	channel->pitch = effectivePitch;
	channel->entchannel = entchannel;
	if ( !sentence && effectivePitch != PITCH_NORM )
		VOX_MakeSingleWordSentence( channel, effectivePitch );
	VOX_TrimStartEndTimes( channel, cache );
	SND_Spatialize( channel );
}

extern vec3_t listener_origin;
extern int cszrawsentences;
extern portable_samplepair_t paintbuffer[];

void SX_RoomFX( int count, int filter, int timeEffects );

constexpr int SOUND_CONSOLE_FILENAME_BYTES = 256;
constexpr int SOUND_CONSOLE_COPY_LIMIT = SOUND_CONSOLE_FILENAME_BYTES - sizeof( ".wav" );
constexpr int SOUND_PLAY_ENTITY_SEED = 345;
constexpr int SOUND_PLAY_VOLUME_ENTITY_SEED = 543;
constexpr int SOUND_SAY_BUFFER_BYTES = 256;
constexpr int SOUND_DSP_PROFILE_SAMPLES = 512;
constexpr int SOUND_DSP_PROFILE_ITERATIONS = 10000;
constexpr int SOUND_DSP_PROFILE_RANDOM_MAX = 2999;
constexpr int SOUND_PAINT_PROFILE_SAMPLES = SOUND_DSP_PROFILE_SAMPLES * SOUND_DSP_PROFILE_ITERATIONS;
constexpr int SOUND_SAY_PROFILE_ENTITY_SEED = 543;
constexpr int SOUND_SAY_OVERRIDE_CHANNEL = -1;
constexpr int SOUND_STATS_MAX_CLIENTS = 2;
constexpr char SOUND_CUSTOM_SENTENCE_NAME[] = "xxtestxx";

void S_SoundInfo_f()
{
	if ( !sound_started || !shm )
	{
		Con_Printf( "sound system not started\n" );
		return;
	}

	Con_Printf( "%5d stereo\n", shm->channels - 1 );
	Con_Printf( "%5d samples\n", shm->samples );
	Con_Printf( "%5d samplepos\n", shm->samplepos );
	Con_Printf( "%5d samplebits\n", shm->samplebits );
	Con_Printf( "%5d submission_chunk\n", shm->submission_chunk );
	Con_Printf( "%5d speed\n", shm->speed );
	std::uintptr_t bufferAddress = reinterpret_cast<std::uintptr_t>( shm->buffer );
	Con_Printf( "0x%x dma buffer\n", bufferAddress );
	Con_Printf( "%5d total_channels\n", total_channels );
}

void S_SoundList()
{
	unsigned int total = 0;
	sfx_t* sound = known_sfx;
	for ( int index = 0; index < num_sfx; ++index, ++sound )
	{
		sfxcache_t* cache = static_cast<sfxcache_t*>( Cache_Check( &sound->cache ) );
		if ( !cache )
			continue;

		unsigned int size = ( static_cast<unsigned int>( cache->stereo ) + 1 )
							* static_cast<unsigned int>( cache->width ) * static_cast<unsigned int>( cache->length );
		total += size;
		Con_Printf( cache->loopstart >= 0 ? "L" : " " );
		Con_Printf( "(%2db) %6i : %s\n", static_cast<int>( static_cast<unsigned int>( cache->width ) * CHAR_BIT ),
			static_cast<int>( size ), sound->name );
	}
	Con_Printf( "Total resident: %i\n", static_cast<int>( total ) );
}

void S_Play()
{
	static int hash = SOUND_PLAY_ENTITY_SEED;
	if ( speak_enable.value == 0.0f )
		return;

	for ( int index = 1; index < Cmd_Argc(); ++index )
	{
		char name[SOUND_CONSOLE_FILENAME_BYTES];
		Q_strncpy( name, Cmd_Argv( index ), SOUND_CONSOLE_COPY_LIMIT );
		name[SOUND_CONSOLE_COPY_LIMIT] = 0;
		if ( !Q_strrchr( const_cast<char*>( Cmd_Argv( index ) ), '.' ) )
			Q_strcat( name, ".wav" );
		sfx_t* sound = S_PrecacheSound( name );
		S_StartDynamicSound( hash++, CHAN_AUTO, sound, listener_origin, VOL_NORM, 1.0f, 0, PITCH_NORM );
	}
}

void S_PlayVol()
{
	static int hash = SOUND_PLAY_VOLUME_ENTITY_SEED;
	if ( speak_enable.value == 0.0f )
		return;

	for ( int index = 1; index < Cmd_Argc(); index += 2 )
	{
		char name[SOUND_CONSOLE_FILENAME_BYTES];
		Q_strncpy( name, Cmd_Argv( index ), SOUND_CONSOLE_COPY_LIMIT );
		name[SOUND_CONSOLE_COPY_LIMIT] = 0;
		if ( !Q_strrchr( const_cast<char*>( Cmd_Argv( index ) ), '.' ) )
			Q_strcat( name, ".wav" );
		sfx_t* sound = S_PrecacheSound( name );
		float playVolume = Q_atof( Cmd_Argv( index + 1 ) );
		S_StartDynamicSound( hash++, CHAN_AUTO, sound, listener_origin, playVolume, 1.0f, 0, PITCH_NORM );
	}
}

#if !defined( _WIN32 )
extern "C" time_t timeGetTime()
{
	return time( nullptr );
}
#endif

void S_Say()
{
	static int hash = SOUND_SAY_PROFILE_ENTITY_SEED;
	if ( nosound.value != 0.0f || !sound_started || Cmd_Argc() < 2 || speak_enable.value == 0.0f )
		return;

	char name[SOUND_SAY_BUFFER_BYTES];
	Q_strncpy( name, Cmd_Argv( 1 ), sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = 0;
	// DEBUG - test performance of dsp code
	if ( !Q_strcmp( name, "dsp" ) )
	{
		for ( int index = 0; index < SOUND_DSP_PROFILE_SAMPLES; ++index )
		{
			paintbuffer[index].left = RandomLong( 0, SOUND_DSP_PROFILE_RANDOM_MAX );
			paintbuffer[index].right = RandomLong( 0, SOUND_DSP_PROFILE_RANDOM_MAX );
		}
		Con_Printf( "Start profiling 10,000 calls to DSP\n" );
		DWORD start = timeGetTime();
		for ( int index = 0; index < SOUND_DSP_PROFILE_ITERATIONS; ++index )
			SX_RoomFX( SOUND_DSP_PROFILE_SAMPLES, 1, 1 );
		Con_Printf( "%d milliseconds \n", static_cast<int>( timeGetTime() - start ) );
		return;
	}
	if ( !Q_strcmp( name, "paint" ) )
	{
		int previousPaintedTime = paintedtime;
		Con_Printf( "Start profiling S_PaintChannels\n" );
		sfx_t* sound = S_PrecacheSound( const_cast<char*>( "ambience/labdrone1.wav" ) );
		S_StartDynamicSound( hash++, CHAN_AUTO, sound, listener_origin, VOL_NORM, 1.0f, 0, PITCH_NORM );
		DWORD start = timeGetTime();
		S_PaintChannels( paintedtime + SOUND_PAINT_PROFILE_SAMPLES );
		Con_Printf( "%d milliseconds \n", static_cast<int>( timeGetTime() - start ) );
		paintedtime = previousPaintedTime;
		return;
	}
	// DEBUG

	bool temporary = name[0] != CHAR_SENTENCE;
	sfx_t* sound;
	if ( temporary )
	{
		_snprintf( name, sizeof( name ), "xxtestxx %s", Cmd_Argv( 1 ) );
		name[sizeof( SOUND_CUSTOM_SENTENCE_NAME ) - 1] = 0;
		rgpszrawsentence[cszrawsentences] = name;
		++cszrawsentences;
		sound = S_PrecacheSound( const_cast<char*>( "!xxtestxx" ) );
	}
	else
	{
		sound = S_FindName( name, nullptr );
	}
	if ( !sound )
	{
		Con_Printf( temporary ? "S_Say: can't cache %s\n" : "S_Say: can't find sentence name %s\n", name );
		return;
	}
	S_StartDynamicSound( cl.viewentity, SOUND_SAY_OVERRIDE_CHANNEL, sound, vec3_origin, VOL_NORM, 1.0f, 0, PITCH_NORM );
	if ( temporary )
		rgpszrawsentence[--cszrawsentences] = nullptr;
}

void S_Say_Reliable()
{
	if ( nosound.value != 0.0f || !sound_started || Cmd_Argc() < 2 || speak_enable.value == 0.0f )
		return;

	char name[SOUND_SAY_BUFFER_BYTES];
	Q_strncpy( name, Cmd_Argv( 1 ), sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = 0;
	bool temporary = name[0] != CHAR_SENTENCE;
	sfx_t* sound;
	if ( temporary )
	{
		_snprintf( name, sizeof( name ), "xxtestxx %s", Cmd_Argv( 1 ) );
		name[sizeof( SOUND_CUSTOM_SENTENCE_NAME ) - 1] = 0;
		rgpszrawsentence[cszrawsentences] = name;
		++cszrawsentences;
		sound = S_PrecacheSound( const_cast<char*>( "!xxtestxx" ) );
	}
	else
	{
		sound = S_FindName( name, nullptr );
	}
	if ( !sound )
	{
		Con_Printf(
			temporary ? "S_Say_Reliable: can't cache %s\n" : "S_Say_Reliable: can't find sentence name %s\n", name );
		return;
	}
	S_StartStaticSound( cl.viewentity, CHAN_STATIC, sound, vec3_origin, VOL_NORM, 1.0f, 0, PITCH_NORM );
	if ( temporary )
		rgpszrawsentence[--cszrawsentences] = nullptr;
}

void S_PrintStats()
{
	if ( s_show.value == 0.0f || cl.maxclients > SOUND_STATS_MAX_CLIENTS )
		return;

	for ( int index = 0; index < total_channels; ++index )
	{
		channel_t* channel = &channels[index];
		int row = index & ( MAX_DYNAMIC_CHANNELS - 1 );
		if ( !channel->sfx )
			Con_NPrintf( row, const_cast<char*>( "" ) );
		else
			Con_NPrintf( row, const_cast<char*>( "%s %03i %02i %20s" ), vstr( channel->origin ), channel->entnum,
				channel->entchannel, channel->sfx->name );
	}
}

extern "C" void VOX_SetChanVol( channel_t* channel );
void S_Update_();

void S_ExtraUpdate()
{
	if ( !VGuiWrap2_IsGameUIVisible() )
	{
		SDL_PumpEvents();
		ClientDLL_IN_Accumulate();
	}
	if ( snd_noextraupdate.value == 0.0f )
		S_Update_();
}

vec3_t listener_origin;
vec3_t listener_forward;
vec3_t listener_right;
vec3_t listener_up;
int snd_blocked;
static qboolean snd_ambient = 1;
sfx_t* ambient_sfx[NUM_AMBIENTS];
cvar_t ambient_level = { const_cast<char*>( "ambient_level" ), const_cast<char*>( "0.3" ), 0, 0.0f, nullptr };
cvar_t ambient_fade = { const_cast<char*>( "ambient_fade" ), const_cast<char*>( "100" ), 0, 0.0f, nullptr };
cvar_t snd_show = { const_cast<char*>( "snd_show" ), const_cast<char*>( "0" ), FCVAR_SPONLY, 0.0f, nullptr };
cvar_t _snd_mixahead = {
	const_cast<char*>( "_snd_mixahead" ), const_cast<char*>( "0.1" ), FCVAR_ARCHIVE, 0.0f, nullptr };

constexpr float AMBIENT_SOUND_THRESHOLD = 8.0f;

void SND_Spatialize( channel_t* channel )
{
// anything coming from the view entity will allways be full volume
	if ( channel->entnum == cl.viewentity )
	{
		channel->leftvol = channel->master_vol;
		channel->rightvol = channel->master_vol;
		VOX_SetChanVol( channel );
		return;
	}

	if ( channel->entnum > 0 && channel->entnum < cl.num_entities )
	{
		cl_entity_t* entity = cl_entities + channel->entnum;
		if ( entity && entity->model && entity->curstate.messagenum == cl.parsecount )
		{
			channel->origin[0] = entity->origin[0];
			channel->origin[1] = entity->origin[1];
			channel->origin[2] = entity->origin[2];
			if ( entity->model->type == mod_brush )
			{
				channel->origin[0] = static_cast<float>(
					static_cast<double>( channel->origin[0] )
					+ static_cast<double>( entity->model->mins[0] + entity->model->maxs[0] ) * 0.5 );
				channel->origin[1] = static_cast<float>(
					static_cast<double>( channel->origin[1] )
					+ static_cast<double>( entity->model->mins[1] + entity->model->maxs[1] ) * 0.5 );
				channel->origin[2] = static_cast<float>(
					static_cast<double>( channel->origin[2] )
					+ static_cast<double>( entity->model->mins[2] + entity->model->maxs[2] ) * 0.5 );
			}
		}
	}

	vec3_t source = { channel->origin[0] - listener_origin[0], channel->origin[1] - listener_origin[1],
		channel->origin[2] - listener_origin[2] };
	float distance = VectorNormalize( source ) * channel->dist_mult;
	float dot = source[1] * listener_right[1] + source[0] * listener_right[0] + source[2] * listener_right[2];
	float rightScale;
	float leftScale;
	if ( shm && shm->channels == 1 )
	{
		rightScale = 1.0f;
		leftScale = 1.0f;
	}
	else
	{
		rightScale = static_cast<float>( 1.0 + static_cast<double>( dot ) );
		leftScale = static_cast<float>( 1.0 - static_cast<double>( dot ) );
	}

	double attenuation = 1.0 - static_cast<double>( distance );
	float masterVolume = static_cast<float>( channel->master_vol );
	float scale = static_cast<float>( static_cast<double>( rightScale ) * attenuation );
	channel->rightvol = static_cast<int>( scale * masterVolume );
	scale = static_cast<float>( static_cast<double>( leftScale ) * attenuation );
	channel->leftvol = static_cast<int>( scale * masterVolume );
	VOX_SetChanVol( channel );
	if ( channel->rightvol < 0 )
		channel->rightvol = 0;
	if ( channel->leftvol < 0 )
		channel->leftvol = 0;
}

void S_UpdateAmbientSounds()
{
	if ( !snd_ambient || !cl.worldmodel )
		return;
	mleaf_t* leaf = Mod_PointInLeaf( listener_origin, cl.worldmodel );
	if ( !leaf || ambient_level.value == 0.0f )
	{
		for ( int index = 0; index < NUM_AMBIENTS; ++index )
			channels[index].sfx = nullptr;
		return;
	}

	for ( int index = 0; index < NUM_AMBIENTS; ++index )
	{
		channel_t* channel = &channels[index];
		channel->sfx = ambient_sfx[index];
		float ambientVolume = ambient_level.value * leaf->ambient_sound_level[index];
		if ( ambientVolume < AMBIENT_SOUND_THRESHOLD )
			ambientVolume = 0.0f;
		if ( ambientVolume > static_cast<float>( channel->master_vol ) )
		{
			channel->master_vol = static_cast<int>( static_cast<double>( channel->master_vol )
													+ static_cast<double>( ambient_fade.value ) * host_frametime );
			if ( static_cast<float>( channel->master_vol ) > ambientVolume )
				channel->master_vol = static_cast<int>( ambientVolume );
		}
		else if ( static_cast<float>( channel->master_vol ) > ambientVolume )
		{
			channel->master_vol = static_cast<int>( static_cast<double>( channel->master_vol )
													- static_cast<double>( ambient_fade.value ) * host_frametime );
			if ( ambientVolume > static_cast<float>( channel->master_vol ) )
				channel->master_vol = static_cast<int>( ambientVolume );
		}
		channel->rightvol = channel->master_vol;
		channel->leftvol = channel->master_vol;
	}
}

void S_Update( vec_t* origin, vec_t* forward, vec_t* right, vec_t* up )
{
	if ( !sound_started || snd_blocked > 0 )
		return;
	listener_origin[0] = origin[0];
	listener_origin[1] = origin[1];
	listener_origin[2] = origin[2];
	listener_forward[0] = forward[0];
	listener_forward[1] = forward[1];
	listener_forward[2] = forward[2];
	listener_right[0] = right[0];
	listener_right[1] = right[1];
	listener_right[2] = right[2];
	listener_up[0] = up[0];
	listener_up[1] = up[1];
	listener_up[2] = up[2];
	S_UpdateAmbientSounds();
	for ( int index = NUM_AMBIENTS; index < total_channels; ++index )
	{
		channel_t* channel = &channels[index];
		if ( channel->sfx )
			SND_Spatialize( channel ); // respatialize channel
	}

	if ( snd_show.value != 0.0f )
	{
		int total = 0;
		for ( int index = 0; index < total_channels; ++index )
		{
			channel_t* channel = &channels[index];
			if ( channel->sfx && ( channel->leftvol || channel->rightvol ) )
			{
				Con_Printf( "%3i %3i %s\n", channel->leftvol, channel->rightvol, channel->sfx->name );
				++total;
			}
		}
		Con_Printf( "----(%i)----\n", total );
	}
	S_Update_();
}

extern "C" int SNDDMA_GetDMAPos();
extern "C" void SNDDMA_Submit();

int soundtime; // sample PAIRS
static int buffers;
static int oldsamplepos;

constexpr int SOUND_TIME_RESET_LIMIT = 0x40000000;

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4701 )
#endif
void GetSoundtime()
{
	int fullSamples;
	if ( shm )
		fullSamples = shm->samples / shm->channels;
	int samplePosition = SNDDMA_GetDMAPos();
	if ( samplePosition < oldsamplepos )
	{
		++buffers;
		if ( paintedtime > SOUND_TIME_RESET_LIMIT )
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullSamples;
			S_StopAllSounds( 1 );
		}
	}
	oldsamplepos = samplePosition;
	if ( shm )
		soundtime = buffers * fullSamples + samplePosition / shm->channels;
}
void S_Update_()
{
	int endtime, bufferSamples;

	if ( !sound_started || snd_blocked > 0 )
		return;

// Get soundtime, which tells how many samples have
// been played out of the dma buffer since sound system startup.

	GetSoundtime();
	if ( shm )
	{
		endtime = static_cast<int>(
			static_cast<float>( soundtime ) + static_cast<float>( shm->dmaspeed ) * _snd_mixahead.value );
		bufferSamples = shm->samples >> ( shm->channels - 1 );
	}
	if ( endtime - soundtime > bufferSamples )
		endtime = soundtime + bufferSamples;

#ifdef _WIN32
	if ( pDSBuf )
	{
		DWORD status;
		if ( pDSBuf->GetStatus( &status ) != DS_OK )
			Con_Printf( "Couldn't get sound buffer status\n" );
		if ( status & DSBSTATUS_BUFFERLOST )
			pDSBuf->Restore();
		if ( !( status & DSBSTATUS_PLAYING ) )
			pDSBuf->Play( 0, 0, DSBPLAY_LOOPING );
	}
#endif
	S_PaintChannels( static_cast<int>( static_cast<unsigned int>( endtime ) >> 1 ) );
	SNDDMA_Submit();
}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif
constexpr int SOUND_CONFIG_PATH_BYTES = 260;
constexpr int SOUND_CONFIG_HEAP_LOAD = 5;
constexpr float SOUND_RAW_VOLUME_SCALE = 256.0f;
constexpr int SOUND_RAW_BYTE_RANGE = 256;

void S_AmbientOff()
{
	snd_ambient = 0;
}

void S_AmbientOn()
{
	snd_ambient = 1;
}

qboolean S_QuerySoundSetting( const char* pszCvar, qboolean bDefault )
{
	char path[SOUND_CONFIG_PATH_BYTES];
	strcpy( path, "config.cfg" );
	byte* file = COM_LoadFile( path, SOUND_CONFIG_HEAP_LOAD, nullptr );
	if ( file )
	{
		const char* data = reinterpret_cast<char*>( file );
		for ( ;; )
		{
			data = COM_Parse( data );
			if ( Q_strlen( com_token ) <= 0 )
				break;
			if ( Q_stricmp( com_token, "bind" ) != 0 )
			{
				if ( com_token[0] != '+' )
				{
					if ( Q_stricmp( com_token, pszCvar ) == 0 )
					{
						COM_Parse( data );
						bDefault = strtol( com_token, nullptr, 10 ) != 0;
						break;
					}
					data = COM_Parse( data );
				}
			}
			else
			{
				const char* next = COM_Parse( data );
				data = COM_Parse( next );
			}
		}
		Mem_Free( file );
	}
	return bDefault;
}

void S_FlushSoundData( int fDumpLores, int fDumpHires )
{
	channel_t* channel = channels;
	for ( int index = 0; index < num_sfx; ++index )
	{
		sfxcache_t* cache = static_cast<sfxcache_t*>( Cache_Check( &known_sfx[index].cache ) );
		if ( !cache )
			continue;

		bool keep = false;
		if ( total_channels > 0 )
		{
			for ( int scanned = 0; scanned < total_channels; ++scanned, ++channel )
			{
				if ( !channel->sfx || channel->entchannel != CHAN_STREAM || channel->sfx != &known_sfx[index] )
					continue;
				keep = true;
				break;
			}
		}
		if ( keep )
			continue;

		if ( fDumpLores && cache->speed <= shm->speed )
			Cache_Free( &known_sfx[index].cache );
		if ( fDumpHires && cache->speed > shm->speed )
			Cache_Free( &known_sfx[index].cache );
	}
}

void S_TouchSound( char* name )
{
	if ( !sound_started )
		return;

	sfx_t* sound = S_FindName( name, nullptr );
	Cache_Check( &sound->cache );
}

void S_RawSamples( int samples, int sampleRate, int width, int sampleChannels, byte* data, float sampleVolume )
{
	int start = s_rawend;
	if ( s_rawend < paintedtime )
	{
		s_rawend = paintedtime;
		start = paintedtime;
	}

	float scale = static_cast<float>( sampleRate ) / static_cast<float>( shm->speed );
	int intVolume = static_cast<int>( sampleVolume * SOUND_RAW_VOLUME_SCALE );
	if ( sampleChannels == 2 && width == 2 )
	{
		int position = start;
		int source = 0;
		if ( samples > source )
		{
			do
			{
				int sampleIndex = position;
				++position;
				int offset = source * 4;
				int slot = sampleIndex & ( MAX_RAW_SAMPLES - 1 );
				s_rawend = position;
				s_rawsamples[slot].left = intVolume * *reinterpret_cast<short*>( &data[offset] );
				s_rawsamples[slot].right = intVolume * *reinterpret_cast<short*>( &data[offset + 2] );
				source = static_cast<int>( static_cast<float>( position - start ) * scale );
			} while ( source < samples );
		}
	}
	else if ( width == 2 && sampleChannels == 1 )
	{
		int source = 0;
		if ( samples > source )
		{
			int position = start;
			do
			{
				int sampleIndex = position;
				++position;
				s_rawend = position;
				short* sample = reinterpret_cast<short*>( &data[source * 2] );
				int slot = sampleIndex & ( MAX_RAW_SAMPLES - 1 );
				s_rawsamples[slot].left = intVolume * *sample;
				s_rawsamples[slot].right = intVolume * *sample;
				source = static_cast<int>( static_cast<float>( position - start ) * scale );
			} while ( samples > source );
		}
	}
	else if ( sampleChannels == 2 && width == 1 )
	{
		int byteVolume = intVolume * SOUND_RAW_BYTE_RANGE;
		int source = 0;
		if ( samples > source )
		{
			int position = start;
			do
			{
				int sampleIndex = position;
				++position;
				int offset = source * 2;
				int slot = sampleIndex & ( MAX_RAW_SAMPLES - 1 );
				s_rawend = position;
				s_rawsamples[slot].left = byteVolume * ( data[offset] - SOUND_UNSIGNED_SILENCE );
				s_rawsamples[slot].right = byteVolume * ( data[offset + 1] - SOUND_UNSIGNED_SILENCE );
				source = static_cast<int>( static_cast<float>( position - start ) * scale );
			} while ( samples > source );
		}
	}
	else if ( sampleChannels == 1 && width == 1 )
	{
		int byteVolume = intVolume * SOUND_RAW_BYTE_RANGE;
		int source = 0;
		if ( samples > source )
		{
			int position = start;
			do
			{
				int sampleIndex = position;
				++position;
				byte* sample = &data[source];
				int slot = sampleIndex & ( MAX_RAW_SAMPLES - 1 );
				s_rawend = position;
				s_rawsamples[slot].left = byteVolume * ( *sample - SOUND_UNSIGNED_SILENCE );
				s_rawsamples[slot].right = byteVolume * ( *sample - SOUND_UNSIGNED_SILENCE );
				source = static_cast<int>( static_cast<float>( position - start ) * scale );
			} while ( samples > source );
		}
	}
}

void S_LocalSound( char* sound )
{
	if ( nosound.value != 0.0f || !sound_started )
		return;

	sfx_t* sfx = S_PrecacheSound( sound );
	if ( !sfx )
	{
		Con_Printf( "S_LocalSound: can't cache %s\n", sound );
		return;
	}
	S_StartDynamicSound( cl.viewentity, -1, sfx, vec3_origin, 1.0f, 1.0f, 0, PITCH_NORM );
}
