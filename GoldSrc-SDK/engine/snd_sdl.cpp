#include "sound.h"
#include "../external/SDL2/SDL.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int g_fUseDInput;

extern "C" void SetMouseEnable( qboolean )
{
}

#if !defined( _WIN32 )

#define SND_BUFFER_SIZE 0x10000

extern "C" int setenv( const char* name, const char* value, int overwrite );

extern int snd_blocked;

static unsigned char* sndBuffers;
static SDL_AudioDeviceID m_devId;
static bool bInitialized;
static int snd_sent_position;
static int sample16;
static int snd_buffer_count;

extern "C" void Snd_ReleaseBuffer()
{
	if ( --snd_buffer_count == 0 )
	{
		S_ClearBuffer();
		S_Shutdown();
	}
}

extern "C" void Snd_AcquireBuffer()
{
	if ( ++snd_buffer_count == 1 )
		S_Startup();
}

extern "C" void S_BlockSound()
{
	if ( ++snd_blocked == 1 )
		SDL_PauseAudioDevice( m_devId, 1 );
}

extern "C" void S_UnblockSound()
{
	if ( --snd_blocked == 0 )
		SDL_PauseAudioDevice( m_devId, 0 );
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
extern "C" void SNDDMA_Shutdown()
{
	if ( !bInitialized )
		return;

	bInitialized = false;
	if ( m_devId )
	{
		SDL_CloseAudioDevice( m_devId );
		m_devId = 0;
	}
	SDL_QuitSubSystem( SDL_INIT_AUDIO );
	free( sndBuffers );
	sndBuffers = NULL;
}

static void AudioCallbackEntry( void*, Uint8* stream, int len )
{
	int pos, size;

	if ( !m_devId )
	{
		puts( "SDLAUDIO: uhoh, no audio device!" );
		return;
	}

	pos = snd_sent_position;
	while ( len > 0 )
	{
		size = SND_BUFFER_SIZE;
		if ( len <= SND_BUFFER_SIZE )
			size = len;

		*stream = 0;
		*sndBuffers = 0;
		if ( pos + size >= SND_BUFFER_SIZE )
		{
			size = SND_BUFFER_SIZE - pos;
			memcpy( stream, sndBuffers + pos, (size_t)size );
			pos = 0;
			snd_sent_position = 0;
		}
		else
		{
			memcpy( stream, sndBuffers + pos, (size_t)size );
			pos += size;
			snd_sent_position = pos;
		}
		stream += size;
		len -= size;
	}
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/
extern "C" qboolean SNDDMA_Init()
{
	SDL_AudioSpec desired, obtained;

	if ( bInitialized )
		return TRUE;

	bInitialized = true;
	sn.channels = 2;
	sn.samplebits = 16;
	sn.speed = 11025;
	sn.dmaspeed = 22050;
	shm = &sn;
	snd_sent_position = 0;
	setenv( "PULSE_PROP_application.name", "Half-Life 1", 1 );
	setenv( "PULSE_PROP_media.role", "game", 1 );

	if ( SDL_WasInit( SDL_INIT_AUDIO ) || SDL_InitSubSystem( SDL_INIT_AUDIO ) == 0 )
	{
		memset( &desired, 0, sizeof( desired ) );
		desired.freq = shm->dmaspeed;
		desired.format = AUDIO_S16LSB;
		desired.channels = (Uint8)shm->channels;
		desired.samples = 1024;
		desired.callback = AudioCallbackEntry;
		m_devId = SDL_OpenAudioDevice( NULL, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE );
		if ( m_devId )
		{
			sndBuffers = (unsigned char*)calloc( SND_BUFFER_SIZE, 1 );
			shm->soundalive = TRUE;
			shm->splitbuffer = FALSE;
			shm->samples = SND_BUFFER_SIZE / ( shm->samplebits / 8 );
			shm->samplepos = 0;
			shm->submission_chunk = 1;
			shm->buffer = sndBuffers;
			sample16 = shm->samplebits / 8 - 1;
			SDL_PauseAudioDevice( m_devId, 0 );
			return TRUE;
		}
	}

	puts( "SDL_InitSubSystem(SDL_INIT_AUDIO) failed." );
	bInitialized = false;
	return FALSE;
}

extern "C" void SNDDMA_Submit()
{
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
extern "C" int SNDDMA_GetDMAPos()
{
	return ( snd_sent_position >> sample16 ) & ( shm->samples - 1 );
}

#endif
