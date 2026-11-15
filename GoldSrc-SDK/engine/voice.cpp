/*==============================================================================
   Copyright (c) Valve, LLC. 1999
   All Rights Reserved.  Proprietary.
--------------------------------------------------------------------------------
   Author : DSpeyrer
   Purpose: 
--------------------------------------------------------------------------------
 * $Log:                      $
 * 
 * $NoKeywords:
==============================================================================*/

#include "voice.h"
#include "CircularBuffer.h"
#include "client.h"
#include "conprint.h"
#include "cvar.h"
#include "filesystem.h"
#include "ivoicecodec.h"
#include "ivoicerecord.h"
#include "sys.h"
#include "voice_gain.h"
#include "voice_mixer_controls.h"
#include "voice_wavefile.h"

#include "../external/steamworks/isteamuser.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>
#if defined( _WIN32 )
#include <cassert>
#if defined( NDEBUG )
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );
#endif
#endif

bool VoiceSE_Init();
void VoiceSE_Term();
void VoiceSE_RegisterCvars();
void VoiceSE_StartOverdrive();
void VoiceSE_EndOverdrive();
void VoiceSE_StartChannel( int channel );
void VoiceSE_EndChannel( int channel );
void VoiceSE_InitMouth( int entity );
void VoiceSE_CloseMouth( int entity );
void VoiceSE_MoveMouth( int entity, short* samples, int sampleCount );
void VoiceSE_Idle( float frametime );

void Voice_EndAllChannels();

#if !defined( _WIN32 )
IVoiceRecord* CreateVoiceRecord( int sampleRate );
#endif

#define VOICE_SAMPLE_RATE 11025

#define VOICE_RECEIVE_BUFFER_SIZE ( VOICE_SAMPLE_RATE * 2 )

#define MAX_WAVEFILEDATA_LEN ( 1024 * 1024 )

#define MAX_RECORD_SAMPLES 8192

#define VOICE_CHANNEL_ERROR -1
#define VOICE_CHANNEL_IN_TWEAK_MODE -2

// Special entity index used for tweak mode.
#define TWEAKMODE_ENTITYINDEX -500

// Special channel index passed to Voice_AddIncomingData when in tweak mode.
#define TWEAKMODE_CHANNELINDEX -100

#define VOICE_LOCAL_PLAYER -1
#define VOICE_SERVER_ACK -2

#define TIME_PADDING 0.2f // Time between receiving the first voice packet and actually starting
													// to play the sound. This accounts for frametime differences on the clients
													// and the server.

#define LOCALPLAYERTALKING_TIMEOUT 0.2f // How long it takes for the client to decide the server isn't sending acks
													// of voice data back.

//
// Used for storing incoming voice data from an entity.
//
class CVoiceChannel
{
public:
	CVoiceChannel();
	// Called when someone speaks and a new voice channel is setup to hold the data.
	void Init( int nEntity );

	int m_iEntity; // Number of the entity speaking on this channel (index into cl_entities).
													// This is -1 when the channel is unused.
	CSizedCircularBuffer<VOICE_RECEIVE_BUFFER_SIZE> m_Buffer; // Circular buffer containing the voice data.
	// Used for upsampling..
	double m_LastFraction; // Fraction between m_LastSample and the next sample.
	short m_LastSample;
	bool m_bStarved; // Set to true when the channel runs out of data for the mixer.
													// The channel is killed at that point.
	float m_TimePad; // Set to TIME_PADDING when the first voice packet comes in from a sender.
													// We add time padding (for frametime differences)
													// by waiting a certain amount of time before starting to output the sound.
	IVoiceCodec* m_pVoiceCodec; // Each channel gets is own IVoiceCodec instance so the codec can maintain state.
	CAutoGain m_AutoGain; // Gain we're applying to this channel
	CVoiceChannel* m_pNext;
};

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CVoiceChannel::CVoiceChannel()
{
	m_iEntity = -1;
	m_pVoiceCodec = NULL;
}

CVoiceChannel g_VoiceChannels[VOICE_NUM_CHANNELS];

int g_nVoiceFadeSamples = 1; // Calculated each frame from the cvar.
float g_VoiceFadeMul = 1.0f; // 1 / (g_nVoiceFadeSamples - 1).
bool g_bVoiceAtLeastPartiallyInitted;
IMixerControls* g_pMixerControls;
CSysModule* g_hVoiceCodecDLL;
static double g_CompressTime;
static double g_DecompressTime;
static double g_GainTime;
static double g_UpsampleTime;

static qboolean g_bLocalPlayerTalkingAck;
static float g_LocalPlayerTalkingTimeout;
bool g_bInTweakMode;
static bool g_bUsingSteamVoice = true;
static IVoiceRecord* g_pVoiceRecord;
static IVoiceCodec* g_pEncodeCodec;
static qboolean g_bVoiceRecording;
static int g_VoiceVolume;

char* g_pUncompressedFileData;
int g_nUncompressedDataBytes;
const char* g_pUncompressedDataFilename;
char* g_pDecompressedFileData;
int g_nDecompressedDataBytes;
const char* g_pDecompressedDataFilename;
char* g_pMicInputFileData;
int g_nMicInputFileBytes;
int g_CurMicInputFileByte;
double g_MicStartTime;

// The format we sample voice in.
WAVEFORMATEX g_VoiceSampleFormat =
{
	WAVE_FORMAT_PCM, // wFormatTag
	1, // nChannels
	VOICE_SAMPLE_RATE, // nSamplesPerSec
	VOICE_SAMPLE_RATE * 2, // nAvgBytesPerSec
	2, // nBlockAlign
	16, // wBitsPerSample
	sizeof( WAVEFORMATEX ) // cbSize
};

cvar_t voice_loopback = { "voice_loopback", "0" };
cvar_t voice_maxgain = { "voice_maxgain", "5" };
cvar_t voice_avggain = { "voice_avggain", "0.5" };
cvar_t voice_scale = { "voice_scale", "1", FCVAR_ARCHIVE };
cvar_t voice_fadeouttime = { "voice_fadeouttime", "0.1" };
cvar_t voice_profile = { "voice_profile", "0" };
cvar_t voice_showchannels = { "voice_showchannels", "0" };
cvar_t voice_showincoming = { "voice_showincoming", "0" };
cvar_t voice_enable = { "voice_enable", "1", FCVAR_ARCHIVE | FCVAR_FILTERSTUFFTEXT };
cvar_t voice_dsound = { "voice_dsound", "1" };
cvar_t voice_forcemicrecord = { "voice_forcemicrecord", "1", FCVAR_ARCHIVE };

void CVoiceChannel::Init( int nEntity )
{
	m_iEntity = nEntity;
	m_bStarved = false;
	m_Buffer.Flush();
	m_LastFraction = 0.999;
	m_TimePad = TIME_PADDING;
	m_LastSample = 0;
	m_AutoGain.Reset( 128, voice_maxgain.value, voice_avggain.value, voice_scale.value );
}

//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Assigns a channel to an entity by searching for either a channel
//			already assigned to that entity or picking the least recently used
//			channel. If the LRU channel is picked, it is flushed and all other
//			channels are aged.
// Input  : nEntity - entity number to assign to a channel.
// Output : A channel index to which the entity has been assigned.
//------------------------------------------------------------------------------
int Voice_AssignChannel( int nEntity )
{
	if ( g_bInTweakMode )
		return VOICE_CHANNEL_IN_TWEAK_MODE;

	for ( int i = 0; i < VOICE_NUM_CHANNELS; i++ )
	{
		CVoiceChannel* pChannel = &g_VoiceChannels[i];
		if ( pChannel->m_iEntity == nEntity )
			return i;

		if ( pChannel->m_iEntity == -1 )
		{
			if ( pChannel->m_pVoiceCodec )
				pChannel->m_pVoiceCodec->ResetState();

			pChannel->Init( nEntity );
			VoiceSE_StartOverdrive();
			return i;
		}
	}

	return VOICE_CHANNEL_ERROR;
}

int VoiceTweak_StartVoiceTweakMode()
{
	// If we're already in voice tweak mode, return an error.
	if ( g_bInTweakMode )
	{
#if defined( _WIN32 )
		_wassert( L"!\"VoiceTweak_StartVoiceTweakMode called while already in tweak mode.\"", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\voice.cpp", 1087 );
#endif
		return 0;
	}
	if ( !g_pMixerControls )
		return 0;

	Voice_EndAllChannels();

	Voice_RecordStop();
	if ( g_pEncodeCodec )
		g_pEncodeCodec->ResetState();

	if ( g_bUsingSteamVoice && SteamUser() )
	{
		SteamUser()->StartVoiceRecording();
		g_bVoiceRecording = TRUE;
	}
	else if ( g_pVoiceRecord )
		g_bVoiceRecording = g_pVoiceRecord->RecordStart();
	else
		g_bVoiceRecording = FALSE;

	if ( g_bVoiceRecording )
		ClientDLL_VoiceStatus( VOICE_LOCAL_PLAYER, true );

	if ( !g_bInTweakMode )
		Voice_AssignChannel( TWEAKMODE_ENTITYINDEX );

	g_bInTweakMode = true;
	return 1;
}

void Voice_EndChannel( int iChannel, bool bTellVoiceSE )
{
#if defined( _WIN32 )
	if ( (unsigned int)iChannel >= VOICE_NUM_CHANNELS )
		_wassert( L"iChannel >= 0 && iChannel < VOICE_NUM_CHANNELS", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\voice.cpp", 482 );
#endif
	if ( bTellVoiceSE )
		VoiceSE_EndChannel( iChannel );

	CVoiceChannel* pChannel = &g_VoiceChannels[iChannel];
	if ( pChannel->m_iEntity >= 0 )
	{
		ClientDLL_VoiceStatus( pChannel->m_iEntity, false );
		VoiceSE_CloseMouth( pChannel->m_iEntity );
	}

	pChannel->m_iEntity = -1;
}

void Voice_NotifyFreeChannel( int iChannel )
{
	Voice_EndChannel( iChannel, false );
}

double UpsampleIntoBuffer(
	const short* pSrc, int nSrcSamples, CCircularBuffer* pBuffer, double fraction, double step )
{
	while ( fraction < nSrcSamples - 1 )
	{
		int iSample = (int)fraction;
		int first = pSrc[iSample];
		int second = pSrc[iSample + 1];
		short sample = (short)(int)( ( fraction - floor( fraction ) ) * ( second - first ) + first );

		pBuffer->Write( &sample, sizeof( sample ) );
		fraction += step;
	}

	return fraction - floor( fraction );
}

int Voice_AddIncomingData( int nChannel, const char* pchData, int nCount, int )
{
	// If in tweak mode, we call this during Idle with -1 as the channel, so any channel data from the network
	// gets rejected.
	if ( g_bInTweakMode )
	{
		g_VoiceVolume = 0;
		if ( nChannel != TWEAKMODE_CHANNELINDEX )
			return 0;

		nChannel = 0;
	}

	if ( !nCount )
		return 0;
	if ( (unsigned int)nChannel >= VOICE_NUM_CHANNELS )
	{
#if defined( _WIN32 )
		_wassert( L"false", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\voice.cpp", 325 );
#endif
		return 0;
	}

	CVoiceChannel* pChannel = &g_VoiceChannels[nChannel];
	pChannel->m_bStarved = false;	// This only really matters if you call Voice_AddIncomingData between the time the mixer
									// asks for data and Voice_Idle is called.

	double startTime = 0.0;
	if ( voice_profile.value )
		startTime = Sys_FloatTime();

	// Decompress.
	short decompressed[4096];
	int nDecompressed = 0;
	if ( g_bUsingSteamVoice )
	{
		uint32 nBytesWritten = 0;
		if ( SteamUser()->DecompressVoice(
				 pchData, nCount, decompressed, sizeof( decompressed ), &nBytesWritten, VOICE_SAMPLE_RATE )
				 != k_EVoiceResultOK
			 || !nBytesWritten )
			return 0;

		nDecompressed = nBytesWritten / sizeof( short );
	}
	else if ( pChannel->m_pVoiceCodec )
	{
		nDecompressed =
			pChannel->m_pVoiceCodec->Decompress( pchData, nCount, (char*)decompressed, sizeof( decompressed ) );
	}

	if ( g_bInTweakMode )
	{
		for ( int i = 0; i < nDecompressed; i++ )
		{
			int val = decompressed[i];
			if ( val < 0 )
				val = -val;
			if ( val > g_VoiceVolume )
				g_VoiceVolume = val;
		}

		g_VoiceVolume &= 0xFE00;
	}

	if ( voice_profile.value )
	{
		g_DecompressTime += Sys_FloatTime() - startTime;
		startTime = Sys_FloatTime();
	}

	pChannel->m_AutoGain.ProcessSamples( decompressed, nDecompressed );

	if ( voice_profile.value )
	{
		g_GainTime += Sys_FloatTime() - startTime;
		startTime = Sys_FloatTime();
	}

	double step = (double)g_VoiceSampleFormat.nSamplesPerSec / VOICE_SAMPLE_RATE;
	pChannel->m_LastFraction =
		UpsampleIntoBuffer( decompressed, nDecompressed, &pChannel->m_Buffer, pChannel->m_LastFraction, step );

	pChannel->m_LastSample = ( (const signed char*)decompressed )[nDecompressed];

	if ( voice_profile.value )
		g_UpsampleTime += Sys_FloatTime() - startTime;

	if ( g_pDecompressedFileData )
	{
		unsigned int nBytes = nDecompressed * sizeof( short );
		unsigned int nRemaining = MAX_WAVEFILEDATA_LEN - g_nDecompressedDataBytes;
		if ( nBytes > nRemaining )
			nBytes = nRemaining;

		memcpy( g_pDecompressedFileData + g_nDecompressedDataBytes, decompressed, nBytes );
		g_nDecompressedDataBytes += nBytes;
	}

	if ( voice_showincoming.value )
		Con_Printf( "Voice - %d incoming samples added to channel %d\n", nDecompressed, nChannel );

	return nChannel;
}

void VoiceTweak_EndVoiceTweakMode()
{
	if ( !g_bInTweakMode )
	{
#if defined( _WIN32 )
		_wassert( L"!\"VoiceTweak_EndVoiceTweakMode called when not in tweak mode.\"", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\voice.cpp", 1106 );
#endif
		return;
	}

	g_bInTweakMode = false;
	Voice_RecordStop();
}

void VoiceTweak_SetControlFloat( VoiceTweakControl iControl, float value )
{
	if ( !g_pMixerControls )
		return;

	switch ( iControl )
	{
	case MicrophoneVolume:
		g_pMixerControls->SetValue_Float( IMixerControls::MicVolume, value );
		break;
	case OtherSpeakerScale:
		voice_scale.value = value;
		break;
	case MicBoost:
		g_pMixerControls->SetValue_Float( IMixerControls::MicBoost, value );
		break;
	}
}

float VoiceTweak_GetControlFloat( VoiceTweakControl iControl )
{
	if ( !g_pMixerControls )
		return 0.0f;

	float value = 1.0f;
	switch ( iControl )
	{
	case MicrophoneVolume:
		g_pMixerControls->GetValue_Float( IMixerControls::MicVolume, value );
		break;
	case OtherSpeakerScale:
		value = voice_scale.value;
		break;
	case MicBoost:
		g_pMixerControls->GetValue_Float( IMixerControls::MicBoost, value );
		break;
	}

	return value;
}

int VoiceTweak_GetSpeakingVolume()
{
	return g_VoiceVolume;
}

IVoiceTweak g_VoiceTweakAPI =
{
	VoiceTweak_StartVoiceTweakMode,
	VoiceTweak_EndVoiceTweakMode,
	VoiceTweak_SetControlFloat,
	VoiceTweak_GetControlFloat,
	VoiceTweak_GetSpeakingVolume,
};

void Voice_RegisterCvars()
{
	Cvar_RegisterVariable( &voice_loopback );
	Cvar_RegisterVariable( &voice_maxgain );
	Cvar_RegisterVariable( &voice_avggain );
	Cvar_RegisterVariable( &voice_scale );
	Cvar_RegisterVariable( &voice_fadeouttime );
	Cvar_RegisterVariable( &voice_profile );
	Cvar_RegisterVariable( &voice_showchannels );
	Cvar_RegisterVariable( &voice_showincoming );
	Cvar_RegisterVariable( &voice_enable );
	Cvar_RegisterVariable( &voice_dsound );
	Cvar_RegisterVariable( &voice_forcemicrecord );
	VoiceSE_RegisterCvars();
}

qboolean Voice_GetLoopback()
{
	return voice_loopback.value != 0.0f;
}

void Voice_LocalPlayerTalkingAck()
{
	if ( !g_bLocalPlayerTalkingAck )
		ClientDLL_VoiceStatus( VOICE_SERVER_ACK, true );

	g_bLocalPlayerTalkingAck = TRUE;
	g_LocalPlayerTalkingTimeout = 0.0f;
}

qboolean Voice_IsRecording()
{
	return g_bVoiceRecording && !g_bInTweakMode;
}

qboolean Voice_RecordStart( const char* pUncompressedFile, const char* pDecompressedFile, const char* pMicInputFile )
{
	Voice_RecordStop();

	if ( g_pEncodeCodec )
		g_pEncodeCodec->ResetState();

	if ( pMicInputFile )
	{
		int wBitsPerSample, nChannels, nSamplesPerSec;
		ReadWaveFile(
			pMicInputFile, g_pMicInputFileData, g_nMicInputFileBytes, wBitsPerSample, nChannels, nSamplesPerSec );
		g_CurMicInputFileByte = 0;
		g_MicStartTime = Sys_FloatTime();
	}

	if ( pUncompressedFile )
	{
		g_pUncompressedFileData = new char[MAX_WAVEFILEDATA_LEN];
		g_nUncompressedDataBytes = 0;
		g_pUncompressedDataFilename = pUncompressedFile;
	}

	if ( pDecompressedFile )
	{
		g_pDecompressedFileData = new char[MAX_WAVEFILEDATA_LEN];
		g_nDecompressedDataBytes = 0;
		g_pDecompressedDataFilename = pDecompressedFile;
	}

	if ( g_bUsingSteamVoice && SteamUser() )
	{
		SteamUser()->StartVoiceRecording();
		g_bVoiceRecording = TRUE;
	}
	else if ( g_pVoiceRecord )
		g_bVoiceRecording = g_pVoiceRecord->RecordStart();
	else
		g_bVoiceRecording = FALSE;

	if ( g_bVoiceRecording )
		ClientDLL_VoiceStatus( VOICE_LOCAL_PLAYER, true );

	return g_bVoiceRecording;
}

qboolean Voice_RecordStop()
{
	// Write the files out for debugging.
	if ( g_pMicInputFileData )
	{
		delete[] g_pMicInputFileData;
		g_pMicInputFileData = NULL;
	}

	if ( g_pUncompressedFileData )
	{
		WriteWaveFile( g_pUncompressedDataFilename, g_pUncompressedFileData, g_nUncompressedDataBytes,
			g_VoiceSampleFormat.wBitsPerSample, g_VoiceSampleFormat.nChannels, g_VoiceSampleFormat.nSamplesPerSec );
		delete[] g_pUncompressedFileData;
		g_pUncompressedFileData = NULL;
	}

	if ( g_pDecompressedFileData )
	{
		WriteWaveFile( g_pDecompressedDataFilename, g_pDecompressedFileData, g_nDecompressedDataBytes,
			g_VoiceSampleFormat.wBitsPerSample, g_VoiceSampleFormat.nChannels, g_VoiceSampleFormat.nSamplesPerSec );
		delete[] g_pDecompressedFileData;
		g_pDecompressedFileData = NULL;
	}

	if ( g_bUsingSteamVoice && SteamUser() )
		SteamUser()->StopVoiceRecording();
	else if ( g_pVoiceRecord )
		g_pVoiceRecord->RecordStop();

	if ( g_bVoiceRecording )
		ClientDLL_VoiceStatus( VOICE_LOCAL_PLAYER, false );

	g_bVoiceRecording = FALSE;
	return TRUE;
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
int Voice_GetCompressedData( char* pchDest, int nCount, qboolean bFinal )
{
	if ( g_bUsingSteamVoice )
	{
		uint32 nCompressBytes = 0;
		uint32 nUncompressBytes = 0;
		uint32 nAvailCompressed = 0;
		uint32 nAvailUncompressed = 0;
		if ( SteamUser()->GetAvailableVoice(
				 &nAvailCompressed, &nAvailUncompressed, g_pUncompressedFileData ? VOICE_SAMPLE_RATE : 0 )
			 == k_EVoiceResultOK )
		{
			SteamUser()->GetVoice( true, pchDest, nCount, &nCompressBytes, g_pUncompressedFileData != NULL,
				g_pUncompressedFileData, MAX_WAVEFILEDATA_LEN - g_nUncompressedDataBytes, &nUncompressBytes,
				VOICE_SAMPLE_RATE );

			if ( g_pUncompressedFileData )
				g_nUncompressedDataBytes += nUncompressBytes;
		}

		return nCompressBytes;
	}

	if ( !g_pVoiceRecord || !g_pEncodeCodec )
		return 0;

	short tempData[MAX_RECORD_SAMPLES];
	int nMaxSamples = nCount / 2;
	if ( nMaxSamples > MAX_RECORD_SAMPLES )
		nMaxSamples = MAX_RECORD_SAMPLES;

	int nSamples = g_pVoiceRecord->GetRecordedData( tempData, nMaxSamples );

	if ( g_pMicInputFileData )
	{
		double curTime = Sys_FloatTime();
		int nShouldGet = (int)( ( curTime - g_MicStartTime ) * g_VoiceSampleFormat.nSamplesPerSec );
		int nRemaining = ( g_nMicInputFileBytes - g_CurMicInputFileByte ) / 2;
		if ( nShouldGet > nRemaining )
			nShouldGet = nRemaining;

		nSamples = nShouldGet < MAX_RECORD_SAMPLES ? nShouldGet : MAX_RECORD_SAMPLES;
		memcpy( tempData, g_pMicInputFileData + g_CurMicInputFileByte, nSamples * sizeof( short ) );
		g_MicStartTime = curTime;
		g_CurMicInputFileByte += nSamples * sizeof( short );
	}

	double startTime = 0.0;
	if ( voice_profile.value )
		startTime = Sys_FloatTime();

	int nCompressedBytes =
		g_pEncodeCodec->Compress( (const char*)tempData, nSamples, pchDest, nCount, bFinal != FALSE );

	if ( voice_profile.value )
		g_CompressTime += Sys_FloatTime() - startTime;

	if ( g_pUncompressedFileData )
	{
		unsigned int nBytes = nSamples * sizeof( short );
		unsigned int nAvail = MAX_WAVEFILEDATA_LEN - g_nUncompressedDataBytes;
		if ( nBytes > nAvail )
			nBytes = nAvail;

		memcpy( g_pUncompressedFileData + g_nUncompressedDataBytes, tempData, nBytes );
		g_nUncompressedDataBytes += nBytes;
	}

	return nCompressedBytes;
}

void Voice_EndAllChannels()
{
	for ( int i = 0; i < VOICE_NUM_CHANNELS; i++ )
		Voice_EndChannel( i, true );
}

CVoiceChannel* GetVoiceChannel( int iChannel, qboolean )
{
	if ( (unsigned int)iChannel >= VOICE_NUM_CHANNELS )
		return NULL;

	return &g_VoiceChannels[iChannel];
}

//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Determines which channel has been assigened to a given entity.
// Input  : nEntity - entity number.
// Output : The index of the channel assigned to the entity, VOICE_CHANNEL_ERROR
//			if no channel is currently assigned to the given entity.
//------------------------------------------------------------------------------
int Voice_GetChannel( int nEntity )
{
	for ( int i = 0; i < VOICE_NUM_CHANNELS; i++ )
	{
		if ( g_VoiceChannels[i].m_iEntity == nEntity )
			return i;
	}

	return VOICE_CHANNEL_ERROR;
}

void Voice_Deinit()
{
	// This call tends to be expensive and when voice is not enabled it will continually
	// call in here, so avoid the work if possible.
	if ( !g_bVoiceAtLeastPartiallyInitted )
		return;

	Voice_EndAllChannels();
	Voice_RecordStop();

	for ( int i = 0; i < VOICE_NUM_CHANNELS; i++ )
	{
		CVoiceChannel* pChannel = &g_VoiceChannels[i];
		if ( pChannel->m_pVoiceCodec )
		{
			pChannel->m_pVoiceCodec->Release();
			pChannel->m_pVoiceCodec = NULL;
		}
	}

	if ( g_pEncodeCodec )
	{
		g_pEncodeCodec->Release();
		g_pEncodeCodec = NULL;
	}

	if ( g_hVoiceCodecDLL )
	{
		Sys_UnloadModule( g_hVoiceCodecDLL );
		g_hVoiceCodecDLL = NULL;
	}

	if ( g_pVoiceRecord )
	{
		g_pVoiceRecord->Release();
		g_pVoiceRecord = NULL;
	}

	VoiceSE_Term();

	if ( g_pMixerControls )
	{
		g_pMixerControls->Release();
		g_pMixerControls = NULL;
	}

	g_bVoiceAtLeastPartiallyInitted = false;
}

void Voice_UpdateVoiceTweakMode()
{
	char uchVoiceData[2048];
	int nDataLength = Voice_GetCompressedData( uchVoiceData, sizeof( uchVoiceData ), FALSE );
	Voice_AddIncomingData( TWEAKMODE_CHANNELINDEX, uchVoiceData, nDataLength, 0 );
}

void Voice_Idle( float frametime )
{
	if ( !voice_enable.value )
	{
		Voice_Deinit();
		return;
	}

	if ( g_bLocalPlayerTalkingAck )
	{
		g_LocalPlayerTalkingTimeout += frametime;
		if ( g_LocalPlayerTalkingTimeout > LOCALPLAYERTALKING_TIMEOUT )
		{
			g_bLocalPlayerTalkingAck = FALSE;
			ClientDLL_VoiceStatus( VOICE_SERVER_ACK, false );
		}
	}

	int nFadeSamples = (int)( voice_fadeouttime.value * (float)VOICE_SAMPLE_RATE );
	if ( nFadeSamples < 2 )
		nFadeSamples = 2;
	g_nVoiceFadeSamples = nFadeSamples;
	g_VoiceFadeMul = 1.0f / (float)( nFadeSamples - 1 );

	if ( g_pVoiceRecord )
		g_pVoiceRecord->Idle();

	if ( g_bInTweakMode )
		Voice_UpdateVoiceTweakMode();

	// Age the channels.
	int nActive = 0;
	for ( int i = 0; i < VOICE_NUM_CHANNELS; i++ )
	{
		CVoiceChannel* pChannel = &g_VoiceChannels[i];
		if ( pChannel->m_iEntity == -1 )
			continue;

		if ( pChannel->m_bStarved && !g_bInTweakMode )
		{
			Voice_EndChannel( i, true );
			continue;
		}

		float oldpad = pChannel->m_TimePad;
		pChannel->m_TimePad -= frametime;
		if ( oldpad > 0.0f && pChannel->m_TimePad <= 0.0f )
		{
			// Start its audio.
			VoiceSE_StartChannel( i );
			ClientDLL_VoiceStatus( pChannel->m_iEntity, true );
			VoiceSE_InitMouth( pChannel->m_iEntity );
		}

		nActive++;
	}

	if ( !nActive && !g_bInTweakMode )
		VoiceSE_EndOverdrive();

	VoiceSE_Idle( frametime );

	if ( voice_showchannels.value >= 1.0f )
	{
		for ( int i = 0; i < VOICE_NUM_CHANNELS; i++ )
		{
			CVoiceChannel* pChannel = &g_VoiceChannels[i];
			if ( pChannel->m_iEntity == -1 )
				continue;

			Con_Printf( "Voice - chan %d, ent %d, bufsize: %d\n", i, pChannel->m_iEntity,
				pChannel->m_Buffer.GetReadAvailable() );
		}
	}

	if ( voice_profile.value )
	{
		Con_Printf( "Voice - compress: %7.2fu, decompress: %7.2fu, gain: %7.2fu, upsample: %7.2fu, total: %7.2fu\n",
			g_CompressTime * 1000000.0, g_DecompressTime * 1000000.0, g_GainTime * 1000000.0, g_UpsampleTime * 1000000.0,
			( g_DecompressTime + g_CompressTime + g_GainTime + g_UpsampleTime ) * 1000000.0 );

		g_UpsampleTime = 0.0;
		g_GainTime = 0.0;
		g_DecompressTime = 0.0;
		g_CompressTime = 0.0;
	}
}

qboolean Voice_Init( const char* pCodecName, int quality )
{
	if ( !voice_enable.value )
		return FALSE;

	Voice_Deinit();
	g_bVoiceAtLeastPartiallyInitted = true;

	if ( !VoiceSE_Init() )
		return FALSE;

	g_bUsingSteamVoice = SteamUser() != NULL;
	if ( !g_bUsingSteamVoice && pCodecName[0] )
	{
#if !defined( _WIN32 )
		int sampleRate = g_VoiceSampleFormat.nSamplesPerSec;
		g_pVoiceRecord = CreateVoiceRecord( sampleRate );
		if ( !g_pVoiceRecord )
			Con_Printf(
				"Unable to initialize wave input. You won't be able to speak to other players through a microphone.\n" );

		char dllName[256];
		snprintf( dllName, sizeof( dllName ), "%s.so", pCodecName );
#else
		int sampleRate = g_VoiceSampleFormat.nSamplesPerSec;
		if ( !voice_dsound.value )
		{
			g_pVoiceRecord = CreateVoiceRecord_WaveIn( sampleRate );
			if ( !g_pVoiceRecord )
			{
				Con_Printf( "Unable to initialize waveIn. Trying DirectSound." );
				g_pVoiceRecord = CreateVoiceRecord_DSound( sampleRate );
				if ( !g_pVoiceRecord )
					Con_Printf(
						"Unable to initialize wave input. You won't be able to speak to other players through a microphone.\n" );
			}
		}
		else
		{
			g_pVoiceRecord = CreateVoiceRecord_DSound( sampleRate );
			if ( !g_pVoiceRecord )
			{
				Con_Printf( "Unable to initialize DirectSoundCapture. Trying waveIn." );
				g_pVoiceRecord = CreateVoiceRecord_WaveIn( sampleRate );
				if ( !g_pVoiceRecord )
					Con_Printf(
						"Unable to initialize wave input. You won't be able to speak to other players through a microphone.\n" );
			}
		}

		// Get the codec.
		char dllName[256];
		_snprintf( dllName, sizeof( dllName ), "%s.dll", pCodecName );
#endif

		if ( !g_hVoiceCodecDLL )
		{
			FS_GetLocalCopy( dllName );
			g_hVoiceCodecDLL = Sys_LoadModule( dllName );
		}

		if ( !g_hVoiceCodecDLL )
		{
			Con_Printf( "Unable to load voice codec DLL %s.  Voice disabled.\n", dllName );
			Voice_Deinit();
			return FALSE;
		}

		CreateInterfaceFn createCodecFn = Sys_GetFactory( g_hVoiceCodecDLL );
		if ( !createCodecFn )
		{
			Con_Printf( "Unable to get a factory for voice codec %s.  Voice disabled.\n", pCodecName );
			Voice_Deinit();
			return FALSE;
		}

		g_pEncodeCodec = (IVoiceCodec*)createCodecFn( pCodecName, NULL );
		if ( !g_pEncodeCodec )
		{
			Con_Printf( "Unable to create interface for voice codec %s.  Voice disabled.\n", pCodecName );
			Voice_Deinit();
			return FALSE;
		}

		if ( !g_pEncodeCodec->Init( quality ) )
		{
			Con_Printf( "Unable to initialize voice codec %s.  Voice disabled.\n", pCodecName );
			Voice_Deinit();
			return FALSE;
		}

		for ( int i = 0; i < VOICE_NUM_CHANNELS; i++ )
		{
			CVoiceChannel* pChannel = &g_VoiceChannels[i];
			pChannel->m_pVoiceCodec = (IVoiceCodec*)createCodecFn( pCodecName, NULL );
			if ( !pChannel->m_pVoiceCodec || !pChannel->m_pVoiceCodec->Init( quality ) )
			{
				Con_Printf(
					"Unable to get voice codec (%s) interface for channel %d.  Voice disabled.\n", pCodecName, i );
				Voice_Deinit();
				return FALSE;
			}
		}
	}

	g_pMixerControls = GetMixerControls();
	if ( voice_forcemicrecord.value && g_pMixerControls )
		g_pMixerControls->SelectMicrophoneForWaveInput();

	return TRUE;
}

int Voice_GetOutputData( int iChannel, char* copyBuf, int copyBufSize, int, int samplesWanted )
{
	CVoiceChannel* pChannel = &g_VoiceChannels[iChannel];
	int nMaxSamples = copyBufSize / 2;

	int nBytes = pChannel->m_Buffer.GetReadAvailable();
	if ( nBytes > nMaxSamples )
		nBytes = nMaxSamples;
	if ( nBytes > samplesWanted * 2 )
		nBytes = samplesWanted * 2;

	int nSamplesGotten = pChannel->m_Buffer.Read( copyBuf, nBytes ) / 2;
	int nSamplesLeft = pChannel->m_Buffer.GetReadAvailable() / 2;
	short* pSamples = (short*)copyBuf;

	if ( nSamplesLeft < g_nVoiceFadeSamples )
	{
		int iFadeStart = nSamplesGotten + nSamplesLeft - g_nVoiceFadeSamples;
		if ( iFadeStart < 0 )
			iFadeStart = 0;

		int iFadeOffset = g_nVoiceFadeSamples - nSamplesLeft - nSamplesGotten;
		if ( iFadeOffset < 0 )
			iFadeOffset = 0;

		int nFadeSamples = nSamplesGotten - iFadeStart;
		for ( int i = 0; i < nFadeSamples; i++ )
		{
			float percent = (float)( i + iFadeOffset ) * g_VoiceFadeMul;
			pSamples[iFadeStart + i] = (short)(int)( ( 1.0f - percent ) * (float)pSamples[iFadeStart + i] );
		}
	}

	if ( nSamplesGotten < samplesWanted )
	{
		int nPadded = samplesWanted < nMaxSamples ? samplesWanted : nMaxSamples;
		memset( copyBuf + nSamplesGotten * 2, 0, ( nPadded - nSamplesGotten ) * 2 );
		nSamplesGotten = nPadded;
	}

	if ( !pChannel->m_Buffer.GetReadAvailable() )
		pChannel->m_bStarved = true;

	if ( voice_showchannels.value >= 2.0f )
		Con_Printf( "Voice - mixed %d samples from channel %d\n", nSamplesGotten, iChannel );

	VoiceSE_MoveMouth( pChannel->m_iEntity, pSamples, nSamplesGotten );
	return nSamplesGotten;
}

//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Flushes a given receive channel.
// Input  : nChannel - index of channel to flush.
//------------------------------------------------------------------------------
void Voice_FlushChannel( int iChannel )
{
	if ( (unsigned int)iChannel < VOICE_NUM_CHANNELS )
		g_VoiceChannels[iChannel].m_Buffer.Flush();
}
