// snd_mem.c: sound caching
#include "sound.h"
#include "client.h"
#include "common.h"
#include "conprint.h"
#include "cvar.h"
#include "sys.h"
#include "zone.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <memory>

#define CHAR_STREAM		'*'		// as one of 1st 2 chars in name, indicates streaming wav data
#define CHAR_USERVOX	'?'		// as one of 1st 2 chars in name, indicates user realtime voice data

extern cvar_t hisound;
extern cvar_t loadas8bit;
extern cvar_t fs_precache_timings;

sfxcache_t* VoiceSE_GetSFXCache( sfx_t* sound );
qboolean GetWavinfo( char* name, byte* wav, int length, wavinfo_t* info );
void ResampleSfx( sfx_t* sound, int inputRate, int inputWidth, byte* data, int dataBytes );
extern "C" void Wavstream_Close( int index );

byte* iff_data;
byte* iff_end;
byte* last_chunk;
/*
===============================================================================

WAV loading

===============================================================================
*/


byte* data_p;
int iff_chunk_len;

static constexpr int WAV_CHUNK_HEADER_BYTES = 8;
static constexpr int WAV_RIFF_HEADER_BYTES = 12;
static constexpr int WAV_PCM_FORMAT_BYTES = 16;
static constexpr int WAV_CUE_BYTES = 28;
static constexpr int WAV_LOOP_LIST_BYTES = 24;
static constexpr int WAV_MAX_CHUNK_BYTES = 100 * 1024 * 1024;

static bool WavChunkHasBytes( int bytes )
{
	return data_p && bytes >= 0 && iff_chunk_len >= bytes &&
		iff_end - data_p >= WAV_CHUNK_HEADER_BYTES &&
		bytes <= iff_end - data_p - WAV_CHUNK_HEADER_BYTES;
}

int GetLittleLong()
{
	int val;

	val = (int)( (std::uint32_t)data_p[0] | ( (std::uint32_t)data_p[1] << 8 ) |
		( (std::uint32_t)data_p[2] << 16 ) | ( (std::uint32_t)data_p[3] << 24 ) );
	data_p += 4;
	return val;
}

short GetLittleShort()
{
	short val;

	val = (short)( data_p[0] + ( data_p[1] << 8 ) );
	data_p += 2;
	return val;
}

void FindNextChunk( char* name )
{
	while ( last_chunk < iff_end )
	{
		if ( iff_end - last_chunk < WAV_CHUNK_HEADER_BYTES )
			break;
		data_p = last_chunk + 4;
		iff_chunk_len = GetLittleLong();
		if ( iff_chunk_len < 0 )
			break;

		if ( iff_chunk_len > WAV_MAX_CHUNK_BYTES )
			break;

		data_p = last_chunk;
		auto remaining = iff_end - data_p - WAV_CHUNK_HEADER_BYTES;
		last_chunk = iff_end;
		if ( iff_chunk_len <= remaining )
		{
			last_chunk = data_p + WAV_CHUNK_HEADER_BYTES + iff_chunk_len;
			if ( ( iff_chunk_len & 1 ) && last_chunk < iff_end )
			{
				// Shipped Cool Edit WAVs such as death6.wav can omit the data pad.
				bool unpaddedList = false;
				if ( !Q_strncmp( (char*)data_p, "data", 4 ) &&
					iff_end - last_chunk >= WAV_CHUNK_HEADER_BYTES &&
					!Q_strncmp( (char*)last_chunk, "LIST", 4 ) )
				{
					byte* chunk = data_p;
					data_p = last_chunk + 4;
					int listBytes = GetLittleLong();
					data_p = chunk;
					unpaddedList = listBytes >= 4 && listBytes <= iff_end - last_chunk - WAV_CHUNK_HEADER_BYTES;
				}
				if ( !unpaddedList )
					++last_chunk;
			}
		}
		if ( !Q_strncmp( (char*)data_p, name, 4 ) )
			return;
	}

	// didn't find the chunk
	data_p = NULL;
}

void FindChunk( char* name )
{
	last_chunk = iff_data;
	FindNextChunk( name );
}

sfxcache_t* S_LoadStreamSound( sfx_t* s, channel_t* ch )
{
	char namebuffer[256];
	char wavname[SOUND_NAME_BYTES];
	byte* data;
	wavinfo_t info;
	sfxcache_t* sc;
	wavstream_t* stream;
	qboolean ffirstload;
	int cbread;

	if ( cl.fPrecaching )
		return NULL;

	Q_strncpy( wavname, s->name + 1, sizeof( wavname ) - 1 );
	wavname[sizeof( wavname ) - 1] = 0;

	// see if still in memory
	sc = (sfxcache_t*)Cache_Check( &s->cache );
	stream = &wavstreams[ch - channels];
	if ( sc && stream->hFile )
		return sc;
	const auto closeStream = [ch]( wavstream_t* )
	{
		Wavstream_Close( (int)( ch - channels ) );
	};
	std::unique_ptr<wavstream_t, decltype( closeStream )> streamCleanup( stream, closeStream );

	// load it in
	Q_strcpy( namebuffer, "sound" );
	if ( wavname[0] != '/' )
		strncat( namebuffer, "/", sizeof( namebuffer ) - strlen( namebuffer ) - 1 );
	strncat( namebuffer, wavname, sizeof( namebuffer ) - strlen( namebuffer ) - 1 );

	ffirstload = stream->hFile == NULL;

	data = COM_LoadFileLimit( namebuffer, stream->lastposloaded, 0x8000, &cbread, &stream->hFile );
	if ( !data )
	{
		Con_DPrintf( "S_LoadStreamSound: Couldn't load %s\n", namebuffer );
		return NULL;
	}

	if ( ffirstload )
	{
		if ( !GetWavinfo( s->name, data, cbread, &info ) )
		{
			Con_DPrintf( "Failed loading %s\n", wavname );
			return NULL;
		}

		if ( info.channels != 1 )
		{
			Con_DPrintf( "%s is a stereo sample\n", wavname );
			return NULL;
		}

		if ( info.width != 1 )
		{
			Con_DPrintf( "%s is a 16 bit sample\n", wavname );
			return NULL;
		}

		if ( shm && info.rate != shm->speed )
		{
			Con_DPrintf( "%s ignored, not stored at playback sample rate!\n", wavname );
			return NULL;
		}

		stream->csamplesinmem = cbread - info.dataofs;
	}
	else
	{
		info = stream->info;
		stream->csamplesinmem = cbread;
	}

	if ( stream->csamplesinmem > info.samples - stream->csamplesplayed )
		stream->csamplesinmem = info.samples - stream->csamplesplayed;
	stream->info = info;

	if ( !sc )
	{
		sc = (sfxcache_t*)Cache_Alloc( &s->cache, ( cbread < 0x8000 ? cbread : 0x8000 ) + sizeof( sfxcache_t ), wavname );
		if ( !sc )
			return NULL;
	}

	sc->length = stream->csamplesinmem;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	if ( ffirstload )
		ResampleSfx( s, info.rate, info.width, data + info.dataofs, cbread - info.dataofs );
	else
		ResampleSfx( s, info.rate, info.width, data, cbread );

	(void)streamCleanup.release();
	return sc;
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4701 )
#endif
/*
==============
S_LoadSound
==============
*/
sfxcache_t* S_LoadSound( sfx_t* s, channel_t* ch )
{
	char namebuffer[256];
	byte stackbuf[1 * 1024]; // avoid dirtying the cache heap
	byte* data;
	wavinfo_t info;
	int len;
	int samples, size;
	float stepscale;
	sfxcache_t* sc;
	FileHandle_t hFile;
	double start;

	if ( s->name[0] == CHAR_STREAM )
		return S_LoadStreamSound( s, ch );

	if ( s->name[0] == CHAR_USERVOX )
		return VoiceSE_GetSFXCache( s );

	// see if still in memory
	sc = (sfxcache_t*)Cache_Check( &s->cache );
	if ( sc )
	{
		if ( hisound.value || !shm || sc->speed <= shm->speed )
			return sc;
		Cache_Free( &s->cache );
	}

	if ( fs_precache_timings.value )
		start = Sys_FloatTime();

	// load it in
	Q_strcpy( namebuffer, "sound" );
	if ( s->name[0] != '/' )
		strncat( namebuffer, "/", sizeof( namebuffer ) - 1 - strlen( namebuffer ) );
	strncat( namebuffer, s->name, sizeof( namebuffer ) - 1 - strlen( namebuffer ) );

	data = NULL;
	hFile = FS_Open( namebuffer, "rb" );
	if ( hFile )
	{
		data = (byte*)FS_GetReadBuffer( hFile, &len );
		if ( !data )
		{
			len = FS_Size( hFile );
			data = (byte*)Hunk_TempAlloc( len + 1 );
			FS_Read( data, len, 1, hFile );
			FS_Close( hFile );
			hFile = NULL;
		}
	}

	if ( !data )
	{
		namebuffer[0] = 0;
		if ( s->name[0] != '/' )
			strncat( namebuffer, "/", sizeof( namebuffer ) - 1 - strlen( namebuffer ) );
		strncat( namebuffer, s->name, sizeof( namebuffer ) - 1 - strlen( namebuffer ) );
		namebuffer[sizeof( namebuffer ) - 1] = 0;

//	Con_Printf ("loading %s\n",namebuffer);

		data = COM_LoadStackFile( namebuffer, stackbuf, sizeof( stackbuf ), &len );
		if ( !data )
		{
			Con_DPrintf( "S_LoadSound: Couldn't load %s\n", namebuffer );
			return NULL;
		}
	}

	const auto releaseReadBuffer = [data]( void* file )
	{
		FS_ReleaseReadBuffer( file, data );
		FS_Close( file );
	};
	std::unique_ptr<void, decltype( releaseReadBuffer )> fileCleanup( hFile, releaseReadBuffer );

	if ( !GetWavinfo( s->name, data, len, &info ) )
	{
		Con_DPrintf( "Failed loading %s\n", s->name );
		return NULL;
	}

	if ( info.channels != 1 )
	{
		Con_DPrintf( "%s is a stereo sample\n", s->name );
		return NULL;
	}

	if ( !info.rate )
	{
		Con_DPrintf( "Invalid rate: %s\n", s->name );
		return NULL;
	}

	if ( info.dataofs >= len )
	{
		Con_DPrintf( "%s has invalid data offset\n", s->name );
		return NULL;
	}

	stepscale = 1.0f;
	if ( shm && info.rate != shm->speed && !( info.rate == shm->speed * 2 && hisound.value > 0.0f ) )
	{
		stepscale = (float)info.rate / (float)shm->speed;
		if ( stepscale == 0.0f )
		{
			Con_DPrintf( "Invalid stepscale: %s\n", s->name );
			return NULL;
		}
	}

	samples = (int)( (float)info.samples / stepscale );
	if ( samples )
	{
		if ( INT_MAX / samples < 1 )
		{
			Con_DPrintf( "Invalid length (s/c): %s\n", s->name );
			return NULL;
		}

		if ( INT_MAX / samples < info.width )
		{
			Con_DPrintf( "Invalid length (s/c/w): %s\n", s->name );
			return NULL;
		}
	}

	size = samples * info.width;

	if ( (unsigned int)( INT_MAX - size ) < sizeof( sfxcache_t ) )
		Con_DPrintf( "Invalid length (s/c/w/sfx): %s\n", s->name );

	sc = (sfxcache_t*)Cache_Alloc( &s->cache, size + sizeof( sfxcache_t ), s->name );
	if ( !sc )
		return NULL;

	sc->length = info.samples;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;

	ResampleSfx( s, sc->speed, sc->width, data + info.dataofs, len - info.dataofs );

	fileCleanup.reset();

	if ( fs_precache_timings.value )
	{
		float elapsed = (float)( Sys_FloatTime() - start );
		Con_DPrintf( "fs_precache_timings: loaded sound %s in time %.3f sec\n", namebuffer, elapsed );
	}

	return sc;
}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif

/*
============
GetWavinfo
============
*/
qboolean GetWavinfo( char* name, byte* wav, int wavlength, wavinfo_t* info )
{
	int samples;

	if ( !name || !wav || !info || wavlength < WAV_RIFF_HEADER_BYTES )
		return FALSE;

	Q_memset( info, 0, sizeof( *info ) );

	iff_data = wav;
	iff_end = wav + wavlength;

	// find "RIFF" chunk
	FindChunk( "RIFF" );
	if ( !WavChunkHasBytes( 4 ) || Q_strncmp( (char*)( data_p + 8 ), "WAVE", 4 ) )
	{
		Con_Printf( "Missing RIFF/WAVE chunks\n" );
		return FALSE;
	}

	byte* riffStart = data_p;
	int riffLength = iff_chunk_len;
	if ( riffLength < iff_end - data_p - WAV_CHUNK_HEADER_BYTES )
		iff_end = data_p + WAV_CHUNK_HEADER_BYTES + riffLength;

	// get "fmt " chunk
	iff_data = data_p + 12;
// DumpChunks ();

	FindChunk( "fmt " );
	if ( !WavChunkHasBytes( WAV_PCM_FORMAT_BYTES ) || !WavChunkHasBytes( iff_chunk_len ) )
	{
		Con_Printf( "Missing fmt chunk\n" );
		return FALSE;
	}

	data_p += 8;
	if ( GetLittleShort() != 1 )
	{
		Con_Printf( "Microsoft PCM format only\n" );
		return FALSE;
	}

	info->channels = GetLittleShort();
	info->rate = GetLittleLong();
	data_p += 4 + 2;
	int bits = GetLittleShort();
	info->width = bits / 8;
	if ( ( bits != 8 && bits != 16 ) || info->rate <= 0 || info->channels <= 0 )
	{
		Con_Printf( "Invalid sample width\n" );
		return FALSE;
	}

	// get cue chunk
	FindChunk( "cue " );
	if ( data_p )
	{
		if ( !WavChunkHasBytes( WAV_CUE_BYTES ) || !WavChunkHasBytes( iff_chunk_len ) )
			return FALSE;
		data_p += 32;
		info->loopstart = GetLittleLong();
		if ( info->loopstart < 0 )
			return FALSE;
//		Con_Printf("loopstart=%d\n", sfx->loopstart);

		// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk( "LIST" );
		if ( data_p && !WavChunkHasBytes( iff_chunk_len ) )
			return FALSE;
		if ( WavChunkHasBytes( WAV_LOOP_LIST_BYTES ) && !Q_strncmp( (char*)( data_p + 28 ), "mark", 4 ) )
		{
			// this is not a proper parse, but it works with cooledit...
			data_p += 24;
			int loopSamples = GetLittleLong();
			if ( loopSamples < 0 || loopSamples > INT_MAX - info->loopstart )
				return FALSE;
			info->samples = info->loopstart + loopSamples;
		}
	}
	else
		info->loopstart = -1;

	// find data chunk
	FindChunk( "data" );
	if ( !data_p )
	{
		Con_Printf( "Missing data chunk\n" );
		return FALSE;
	}

	if ( name[0] != CHAR_STREAM && !WavChunkHasBytes( iff_chunk_len ) )
		return FALSE;
	if ( (std::uint64_t)( data_p - riffStart ) + WAV_CHUNK_HEADER_BYTES + (std::uint64_t)iff_chunk_len >
		(std::uint64_t)riffLength + WAV_CHUNK_HEADER_BYTES )
		return FALSE;
	data_p += 4;
	samples = GetLittleLong() / info->width;

	if ( info->samples )
	{
		if ( samples < info->samples )
		{
			Con_Printf( "Sound %s has a bad loop length\n", name );
			return FALSE;
		}
	}
	else
		info->samples = samples;
	if ( info->loopstart >= info->samples )
		return FALSE;

	info->dataofs = (int)( data_p - wav );

	if ( info->channels < 0 || info->rate < 0 || info->width < 0 || info->samples < 0 )
	{
		Con_Printf( "Invalid wav info: %s\n", name );
		return FALSE;
	}

	return TRUE;
}

/*
================
ResampleSfx
================
*/
void ResampleSfx( sfx_t* sfx, int inrate, int inwidth, byte* data, int datasize )
{
	int outcount;
	int srcsample;
	float stepscale;
	int i;
	int sample, samplefrac, fracstep;
	unsigned int lastsample;
	sfxcache_t* sc;

	sc = (sfxcache_t*)Cache_Check( &sfx->cache );
	if ( !sc )
		return;

	stepscale = shm ? (float)inrate / (float)shm->speed : 1.0f; // this is usually 0.5, 1, or 2

	if ( shm && stepscale == 2.0f && hisound.value )
	{
		outcount = sc->length;
		stepscale = 1.0f;
	}
	else
	{
		outcount = (int)( (float)sc->length / stepscale );
		sc->length = outcount;
		if ( sc->loopstart != -1 )
			sc->loopstart = (int)( (float)sc->loopstart / stepscale );

		if ( shm )
			sc->speed = shm->speed;
	}

	sc->width = loadas8bit.value ? 1 : inwidth;
	sc->stereo = 0;

	// resample / decimate to the current source rate
	if ( stepscale == 1.0f && inwidth == 1 && sc->width == 1 )
	{
		// fast special case
		if ( outcount > datasize )
		{
			Con_DPrintf( "ResampleSfx: %s has invalid sample count: %d datasize: %d\n", sfx->name, outcount, datasize );
			memset( sc->data, 0, outcount );
			return;
		}

		for ( i = 0; i < outcount; i++ )
			sc->data[i] = (byte)( data[i] - 128 );
		return;
	}

	// general case
	if ( stepscale != 1.0f && stepscale != 2.0f )
		Con_DPrintf( "WARNING! %s is causing runtime sample conversion!\n", sfx->name );

	fracstep = (int)( stepscale * 256 );

	if ( outcount > 0 )
	{
		lastsample = (unsigned int)( outcount - 1 ) * (unsigned int)fracstep;
		if ( lastsample > INT_MAX || ( lastsample >> 8 ) >= (unsigned int)datasize )
		{
			Con_DPrintf( "ResampleSfx: %s has invalid resample count: %d step: %d datasize: %d\n", sfx->name, outcount,
				fracstep, datasize );
			memset( sc->data, 0, outcount );
			return;
		}
	}

	samplefrac = 0;
	// check first 2 characters

	for ( i = 0; i < outcount; i++ )
	{
		srcsample = samplefrac >> 8;
		samplefrac += fracstep;

		if ( inwidth == 2 )
			sample = LittleShort( ( (short*)data )[srcsample] );
		else
			sample = ( (int)data[srcsample] - 128 ) * 256;

		if ( sc->width == 2 )
			( (short*)sc->data )[i] = (short)sample;
		else
			sc->data[i] = (byte)( sample >> 8 );
	}
}

void DumpChunks()
{
	char str[5];

	str[4] = 0;
	data_p = iff_data;
	do
	{
		Q_memcpy( str, data_p, 4 );
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		Con_Printf( "0x%x : %s (%d)\n", (uintptr_t)( data_p - 4 ), str, iff_chunk_len );
		data_p += (int)( ( (unsigned int)iff_chunk_len + 1 ) & ~1u );
	} while ( data_p < iff_end );
}
