// sound.h -- client sound i/o functions

#pragma once

#include <stdint.h>

#include "mathlib.h"
#include "quakedef.h"
#include "filesystem.h"
#include "cvar.h"

#define SOUND_NAME_BYTES 64
#define MAX_SENTENCES 2048

#define MAX_CHANNELS 128
#define MAX_DYNAMIC_CHANNELS 32
#define MAX_RAW_SAMPLES 8192
#define SOUND_CALLBACK_CACHE_LENGTH ( 1 << 30 )

#define CHAR_SENTENCE '!' // as one of 1st 2 chars in name, indicates sentence wav

#ifndef CHAN_NETWORKVOICE_BASE
#define CHAN_NETWORKVOICE_BASE 7
#endif

#ifndef PITCH_NORM
#define PITCH_NORM 100
//-----------------------------------------------------------------------------

#endif

typedef struct
{
	int left;
	int right;
} portable_samplepair_t;

typedef struct sfx_s
{
	char name[SOUND_NAME_BYTES];
	cache_user_t cache;
	int servercount;
} sfx_t;

typedef struct sfxcache_s
{
	int length;
	intptr_t loopstart; // Callback sounds store a function pointer here.
	int speed;
	int width;
	int stereo;
	byte data[1]; // variable sized
} sfxcache_t;

typedef struct
{
	qboolean gamealive;
	qboolean soundalive;
	qboolean splitbuffer;
	int channels;
	int samples;		  // mono samples in buffer
	int submission_chunk; // don't mix less than this #
	int samplepos;		  // in mono samples
	int samplebits;
	int speed;
	int dmaspeed;
	unsigned char* buffer;
} dma_t;

typedef struct
{
	sfx_t* sfx;		 // sfx number
	int leftvol;	 // 0-255 volume
	int rightvol;	 // 0-255 volume
	int end;		 // end time in global paintsamples
	int pos;		 // sample position in sfx
	int looping;	 // where to loop, -1 = no looping
	int entnum;		 // to allow overriding a specific sound
	int entchannel; //
	vec3_t origin;	 // origin of sound effect
	vec_t dist_mult; // distance multiplier (attenuation/clipK)
	int master_vol;	 // 0-255 master volume
	int isentence; // true if playing linked sentence
	int iword;
	int pitch;
} channel_t;

typedef struct
{
	int rate;
	int width;
	int channels;
	int loopstart;
	int samples;
	int dataofs; // chunk starts this many bytes from file start
} wavinfo_t;

typedef struct wavstream_s
{
	int csamplesplayed;
	int csamplesinmem;
	FileHandle_t hFile;
	wavinfo_t info;
	int lastposloaded;
} wavstream_t;

extern volatile dma_t* shm;
extern volatile dma_t sn;
extern qboolean fakedma;
extern qboolean snd_initialized;
extern int sound_started;

extern channel_t channels[MAX_CHANNELS];
extern wavstream_t wavstreams[MAX_CHANNELS];
extern int total_channels;
extern int paintedtime;

extern int s_rawend;
extern portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];

extern char* rgpszrawsentence[MAX_SENTENCES];

extern "C" void	S_Startup();
extern "C" void	S_Shutdown();
extern "C" void	S_ClearBuffer();
extern "C" void	S_ClearPrecache();
void			S_Update( vec_t* origin, vec_t* forward, vec_t* right, vec_t* up );
void			S_ExtraUpdate();
void			S_StartDynamicSound( int entity, int channel, sfx_t* sound, vec_t* origin, float volume, float attenuation,
					int flags, int pitch );
void			S_StartStaticSound( int entity, int channel, sfx_t* sound, vec_t* origin, float volume, float attenuation,
					int flags, int pitch );
int				S_AlterChannel( int entity, int channel, sfx_t* sound, int volume, int pitch, int flags );
void			S_StopSound( int entity, int channel );
void			S_StopAllSounds( qboolean clear );
void			S_StopAllSoundsC();
sfx_t*			S_FindName( char* name, int* inCache );
sfx_t*			S_PrecacheSound( char* name );
void			S_BeginPrecaching();
void			S_EndPrecaching();
sfx_t*			CL_LookupSound( const char* name );
void			SetCareerAudioState( int paused );

sfxcache_t*		S_LoadSound( sfx_t* sound, channel_t* channel );

void			S_PaintChannels( int endtime );
void			SX_Init();
void			SX_Free();
