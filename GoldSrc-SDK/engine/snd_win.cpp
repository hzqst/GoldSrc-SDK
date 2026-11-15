#include "const.h"
#include "snd_win.h"
#include "sound.h"
#include "common.h"
#include "conprint.h"
#include "filesystem.h"
#include "../external/SDL2/SDL.h"
#include "../external/SDL2/SDL_syswm.h"

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

// 64K is > 1 second at 16-bit, 22050 Hz
#define WAV_BUFFERS				64
#define WAV_MASK				( WAV_BUFFERS - 1 )
#define WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

#define SOUND_DMA_SPEED			22050

enum sndinitstat
{
	SIS_SUCCESS,
	SIS_FAILURE,
	SIS_NOTAVAIL
};

typedef HRESULT( WINAPI* LPDIRECTSOUNDCREATE )( LPCGUID, LPDIRECTSOUND*, LPUNKNOWN );

extern SDL_Window** pmainwindow;
extern qboolean Win32AtLeastV4;
extern "C" sndinitstat SNDDMA_InitDirect();
extern "C" qboolean SNDDMA_InitWav();

static qboolean wavonly;
static qboolean dsound_init;
static qboolean wav_init;
static qboolean snd_firsttime = TRUE, snd_isdirect, snd_iswave;
static qboolean primary_format_set;

static int snd_buffer_count;

static int sample16;
static int snd_sent, snd_completed;

LPDIRECTSOUNDBUFFER pDSBuf;
LPDIRECTSOUNDBUFFER pDSPBuf;
LPDIRECTSOUND pDS;
DWORD gSndBufSize;
HWAVEOUT hWaveOut;
HGLOBAL hWaveHdr;
HGLOBAL hData;
LPSTR lpData;
LPWAVEHDR lpWaveHdr;
HINSTANCE hInstDS;
LPDIRECTSOUNDCREATE pDirectSoundCreate;
MMTIME mmstarttime;

/*
==================
FreeSound
==================
*/
static void FreeSound()
{
	int i;

	if ( pDSBuf )
	{
		pDSBuf->Stop();
		pDSBuf->Release();
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if ( pDSPBuf && pDSBuf != pDSPBuf )
		pDSPBuf->Release();

	if ( pDS )
	{
		SDL_SysWMinfo wminfo;

		SDL_VERSION( &wminfo.version );
		SDL_GetWindowWMInfo( *pmainwindow, &wminfo );
		pDS->SetCooperativeLevel( wminfo.info.win.window, DSSCL_NORMAL );
		pDS->Release();
	}

	if ( hWaveOut )
	{
		waveOutReset( hWaveOut );

		if ( lpWaveHdr )
		{
			for ( i = 0; i < WAV_BUFFERS; i++ )
				waveOutUnprepareHeader( hWaveOut, &lpWaveHdr[i], sizeof( WAVEHDR ) );
		}

		waveOutClose( hWaveOut );
		hWaveOut = NULL;

		if ( hWaveHdr )
		{
			GlobalUnlock( hWaveHdr );
			GlobalFree( hWaveHdr );
		}

		if ( hData )
		{
			GlobalUnlock( hData );
			GlobalFree( hData );
		}
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = NULL;
	hData = NULL;
	hWaveHdr = NULL;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = FALSE;
	wav_init = FALSE;

	if ( hInstDS )
	{
		pDirectSoundCreate = NULL;
		FreeLibrary( hInstDS );
		hInstDS = NULL;
	}
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
extern "C" void SNDDMA_Shutdown()
{
	FreeSound();
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
	sndinitstat stat;

	if ( COM_CheckParm( "-wavonly" ) )
		wavonly = TRUE;

	dsound_init = wav_init = FALSE;

	stat = SIS_FAILURE; // assume DirectSound won't initialize

	/* Init DirectSound */
	if ( !wavonly && Win32AtLeastV4 && ( snd_firsttime || snd_isdirect ) )
	{
		stat = SNDDMA_InitDirect();
		if ( stat == SIS_SUCCESS )
		{
			snd_isdirect = TRUE;
			if ( snd_firsttime )
				Con_DPrintf( "DirectSound initialized\n" );
		}
		else
		{
			snd_isdirect = FALSE;
			Con_DPrintf( "DirectSound failed to init\n" );
		}
	}

	// if DirectSound didn't succeed in initializing, try to initialize
	// waveOut sound, unless DirectSound failed because the hardware is
	// already allocated (in which case the user has already chosen not
	// to have sound)
	if ( !dsound_init && stat != SIS_NOTAVAIL && ( snd_firsttime || snd_iswave ) )
	{
		snd_iswave = SNDDMA_InitWav();
		if ( snd_iswave )
		{
			if ( snd_firsttime )
				Con_DPrintf( "Wave sound initialized\n" );
		}
		else
		{
			Con_DPrintf( "Wave sound failed to init\n" );
		}
	}

	snd_firsttime = FALSE;
	snd_buffer_count = 1;

	return dsound_init || wav_init;
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4701 )
#endif
/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
extern "C" sndinitstat SNDDMA_InitDirect()
{
	DSBCAPS dsbcaps;
	DWORD dwSize, dwWrite;
	DSBUFFERDESC dsbuf;
	DSCAPS dscaps;
	WAVEFORMATEX format, pformat;
	HRESULT hresult;
	SDL_SysWMinfo wminfo;
	int reps;

	Q_memset( (void*)&sn, 0, sizeof( sn ) );

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;
	shm->dmaspeed = SOUND_DMA_SPEED;

	Q_memset( &format, 0, sizeof( format ) );
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = (WORD)shm->channels;
	format.wBitsPerSample = (WORD)shm->samplebits;
	format.nSamplesPerSec = shm->dmaspeed;
	format.cbSize = 0;
	format.nBlockAlign = (WORD)( format.wBitsPerSample * format.nChannels / 8 );
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	if ( !hInstDS )
	{
		hInstDS = (HINSTANCE)FS_LoadLibrary( "dsound.dll" );
		if ( !hInstDS )
		{
			Con_SafePrintf( "Couldn't load dsound.dll\n" );
			return SIS_FAILURE;
		}

		pDirectSoundCreate = (LPDIRECTSOUNDCREATE)GetProcAddress( hInstDS, "DirectSoundCreate" );
		if ( !pDirectSoundCreate )
		{
			Con_SafePrintf( "Couldn't get DS proc addr\n" );
			return SIS_FAILURE;
		}
	}

	if ( pDirectSoundCreate( NULL, &pDS, NULL ) != DS_OK )
	{
		Con_DPrintf( "DirectSound create failed.\n" );
		return SIS_FAILURE;
	}

	dscaps.dwSize = sizeof( dscaps );
	if ( pDS->GetCaps( &dscaps ) != DS_OK )
		Con_SafePrintf( "Couldn't get DS caps\n" );

	if ( dscaps.dwFlags & DSCAPS_EMULDRIVER )
	{
		Con_SafePrintf( "No DirectSound driver installed\n" );
		FreeSound();
		return SIS_FAILURE;
	}

	if ( pmainwindow )
	{
		SDL_VERSION( &wminfo.version );
		SDL_GetWindowWMInfo( *pmainwindow, &wminfo );

		if ( pDS->SetCooperativeLevel( wminfo.info.win.window, DSSCL_EXCLUSIVE ) != DS_OK )
		{
			Con_SafePrintf( "Set coop level failed\n" );
			FreeSound();
			return SIS_FAILURE;
		}
	}

	// get access to the primary buffer, if possible, so we can set the
	// sound hardware format
	Q_memset( &dsbuf, 0, sizeof( dsbuf ) );
	dsbuf.dwSize = sizeof( dsbuf );
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D | DSBCAPS_CTRLVOLUME;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	Q_memset( &dsbcaps, 0, sizeof( dsbcaps ) );
	dsbcaps.dwSize = sizeof( dsbcaps );
	primary_format_set = FALSE;

	if ( !COM_CheckParm( "-snoforceformat" ) && pDS->CreateSoundBuffer( &dsbuf, &pDSPBuf, NULL ) == DS_OK )
	{
		pformat = format;

		if ( pDSPBuf->SetFormat( &pformat ) != DS_OK )
		{
			if ( snd_firsttime )
				Con_DPrintf( "Set primary sound buffer format: no\n" );
		}
		else
		{
			if ( snd_firsttime )
				Con_DPrintf( "Set primary sound buffer format: yes\n" );

			primary_format_set = TRUE;
		}
	}

	if ( primary_format_set && COM_CheckParm( "-primarysound" ) )
	{
		if ( pDS->SetCooperativeLevel( wminfo.info.win.window, DSSCL_WRITEPRIMARY ) != DS_OK )
		{
			Con_SafePrintf( "Set coop level failed\n" );
			FreeSound();
			return SIS_FAILURE;
		}

		if ( pDSPBuf->GetCaps( &dsbcaps ) != DS_OK )
		{
			Con_Printf( "DS:GetCaps failed\n" );
			return SIS_FAILURE;
		}

		pDSBuf = pDSPBuf;
		Con_SafePrintf( "Using primary sound buffer\n" );
	}
	else
	{
		// create the secondary buffer we'll actually work with
		Q_memset( &dsbuf, 0, sizeof( dsbuf ) );
		dsbuf.dwSize = sizeof( dsbuf );
		dsbuf.lpwfxFormat = &format;
		dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;

		Q_memset( &dsbcaps, 0, sizeof( dsbcaps ) );
		dsbcaps.dwSize = sizeof( dsbcaps );

		if ( pDS->CreateSoundBuffer( &dsbuf, &pDSBuf, NULL ) != DS_OK )
		{
			Con_SafePrintf( "DS:CreateSoundBuffer Failed" );
			FreeSound();
			return SIS_FAILURE;
		}

		shm->channels = format.nChannels;
		shm->samplebits = format.wBitsPerSample;
		shm->dmaspeed = format.nSamplesPerSec;

		if ( pDSBuf->GetCaps( &dsbcaps ) != DS_OK )
		{
			Con_SafePrintf( "DS:GetCaps failed\n" );
			FreeSound();
			return SIS_FAILURE;
		}

		if ( snd_firsttime )
			Con_SafePrintf( "Using secondary sound buffer\n" );
	}

	// Make sure mixer is active
	if ( pDSBuf->Play( 0, 0, DSBPLAY_LOOPING ) != DS_OK )
		Con_DPrintf( "Failed to start play\n" );

	if ( snd_firsttime )
		Con_DPrintf( "   %d channel(s)\n"
					 "   %d bits/sample\n"
					 "   %d bytes/sec\n",
			shm->channels, shm->samplebits, shm->speed );

	gSndBufSize = dsbcaps.dwBufferBytes;

	// initialize the buffer
	reps = 0;

	while ( ( hresult = pDSBuf->Lock( 0, gSndBufSize, (void**)&lpData, &dwSize, NULL, NULL, 0 ) ) != DS_OK )
	{
		if ( hresult != DSERR_BUFFERLOST )
		{
			Con_SafePrintf( "SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n" );
			FreeSound();
			return SIS_FAILURE;
		}

		if ( ++reps > 10000 )
		{
			Con_SafePrintf( "SNDDMA_InitDirect: DS: couldn't restore buffer\n" );
			FreeSound();
			return SIS_FAILURE;
		}
	}

	Q_memset( lpData, 0, dwSize );
//		lpData[4] = lpData[5] = 0x7f;	// force a pop for debugging
	pDSBuf->Unlock( lpData, dwSize, NULL, 0 );

	/* we don't want anyone to access the buffer directly w/o locking it first. */
	lpData = NULL;

	pDSBuf->Stop();
	pDSBuf->GetCurrentPosition( &mmstarttime.u.sample, &dwWrite );
	pDSBuf->Play( 0, 0, DSBPLAY_LOOPING );

	shm->soundalive = TRUE;
	shm->splitbuffer = FALSE;
	shm->samples = gSndBufSize / ( shm->samplebits / 8 );
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char*)lpData;
	sample16 = shm->samplebits / 8 - 1;

	dsound_init = TRUE;

	return SIS_SUCCESS;
}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif

/*
==================
SNDDM_InitWav

Crappy windows multimedia base
==================
*/
extern "C" qboolean SNDDMA_InitWav()
{
	WAVEFORMATEX format;
	MMRESULT hr;
	int i;

	shm = &sn;
	snd_sent = 0;
	snd_completed = 0;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;
	shm->dmaspeed = SOUND_DMA_SPEED;

	Q_memset( &format, 0, sizeof( format ) );
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = (WORD)shm->channels;
	format.wBitsPerSample = (WORD)shm->samplebits;
	format.nSamplesPerSec = shm->dmaspeed;
	format.cbSize = 0;
	format.nBlockAlign = (WORD)( format.wBitsPerSample * format.nChannels / 8 );
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	hr = waveOutOpen( &hWaveOut, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL );
	if ( hr != MMSYSERR_NOERROR )
	{
		Con_Printf( "The sound hardware is in use by another app.\nSound not available ( %i ).\n", hr );
		return FALSE;
	}

	gSndBufSize = SECONDARY_BUFFER_SIZE;
	hData = GlobalAlloc( GMEM_MOVEABLE | GMEM_SHARE, SECONDARY_BUFFER_SIZE );
	if ( !hData )
	{
		Con_SafePrintf( "Sound: Out of memory.\n" );
		FreeSound();
		return FALSE;
	}

	lpData = (LPSTR)GlobalLock( hData );
	if ( !lpData )
	{
		Con_SafePrintf( "Sound: Failed to lock.\n" );
		FreeSound();
		return FALSE;
	}
	Q_memset( lpData, 0, gSndBufSize );

	hWaveHdr = GlobalAlloc( GMEM_MOVEABLE | GMEM_SHARE, WAV_BUFFERS * sizeof( WAVEHDR ) );
	if ( !hWaveHdr )
	{
		Con_SafePrintf( "Sound: Failed to Alloc header.\n" );
		FreeSound();
		return FALSE;
	}

	lpWaveHdr = (LPWAVEHDR)GlobalLock( hWaveHdr );
	if ( !lpWaveHdr )
	{
		Con_SafePrintf( "Sound: Failed to lock header.\n" );
		FreeSound();
		return FALSE;
	}

	Q_memset( lpWaveHdr, 0, WAV_BUFFERS * sizeof( WAVEHDR ) );

	/* After allocation, set up and prepare headers. */
	for ( i = 0; i < WAV_BUFFERS; i++ )
	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData = lpData + i * WAV_BUFFER_SIZE;

		if ( waveOutPrepareHeader( hWaveOut, &lpWaveHdr[i], sizeof( WAVEHDR ) ) != MMSYSERR_NOERROR )
		{
			Con_SafePrintf( "Sound: failed to prepare wave headers\n" );
			FreeSound();
			return FALSE;
		}
	}

	shm->soundalive = TRUE;
	shm->splitbuffer = FALSE;
	shm->samples = gSndBufSize / ( shm->samplebits / 8 );
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char*)lpData;
	sample16 = shm->samplebits / 8 - 1;

	wav_init = TRUE;

	return TRUE;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4701 )
#endif
extern "C" int SNDDMA_GetDMAPos()
{
	MMTIME mmtime;
	DWORD dwWrite;
	int s;

	if ( dsound_init )
	{
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->GetCurrentPosition( &mmtime.u.sample, &dwWrite );
		s = mmtime.u.sample - mmstarttime.u.sample;
	}
	else if ( wav_init )
	{
		s = snd_sent * WAV_BUFFER_SIZE;
	}

	s >>= sample16;
	s &= shm->samples - 1;

	return s;
}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
extern "C" void SNDDMA_Submit()
{
	LPWAVEHDR h;

	if ( !wav_init )
		return;

	//
	// find which sound blocks have completed
	//
	while ( 1 )
	{
		if ( snd_completed == snd_sent )
		{
			Con_DPrintf( "Sound overrun\n" );
			break;
		}

		if ( !( lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE ) )
			break;

		snd_completed++; // this buffer has been played
	}

	while ( ( ( snd_sent - snd_completed ) >> sample16 ) < 8 )
	{
		h = &lpWaveHdr[snd_sent & WAV_MASK];

		snd_sent++;

		if ( waveOutWrite( hWaveOut, h, sizeof( WAVEHDR ) ) != MMSYSERR_NOERROR )
		{
			Con_SafePrintf( "Failed to write block to device\n" );
			FreeSound();
			return;
		}
	}
}

extern "C" void Snd_ReleaseBuffer()
{
	if ( snd_isdirect )
	{
		if ( --snd_buffer_count == 0 )
		{
			S_ClearBuffer();
			S_Shutdown();
		}
	}
}

extern "C" void Snd_AcquireBuffer()
{
	if ( snd_isdirect )
	{
		if ( ++snd_buffer_count == 1 )
			S_Startup();
	}
}
