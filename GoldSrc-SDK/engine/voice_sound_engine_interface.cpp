#include "client.h"
#include "common.h"
#include "cvar.h"
#include "sound.h"
#include "voice.h"

#include <math.h>
#if defined( _WIN32 )
#include <cassert>
#if defined( NDEBUG )
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );
#endif
#endif

void SND_ForceInitMouth( int entity );
void SND_ForceCloseMouth( int entity );
void SND_MoveMouth16( int entity, short* samples, int sampleCount );
int Voice_GetOutputData( int channel, char* buffer, int bufferBytes, int samplePosition, int sampleCount );
extern float g_SND_VoiceOverdrive;

class VoiceSE_SFX
{
public:
	sfx_t m_SFX;
	sfxcache_t m_SFXCache;
	short m_PrevUpsampleValue;
};

static VoiceSE_SFX g_VoiceSE_SFX[VOICE_NUM_CHANNELS];

static float g_VoiceOverdriveDuration;
static bool g_bVoiceOverdriveOn;

static cvar_t voice_overdrive = { "voice_overdrive", "2" };
static cvar_t voice_overdrivefadetime = { "voice_overdrivefadetime", "0.4" };

void VoiceSE_RegisterCvars()
{
	Cvar_RegisterVariable( &voice_overdrive );
	Cvar_RegisterVariable( &voice_overdrivefadetime );
}

void VoiceSE_Term()
{
}

void VoiceSE_EndOverdrive()
{
	g_bVoiceOverdriveOn = false;
}

void VoiceSE_StartOverdrive()
{
	g_bVoiceOverdriveOn = true;
}

void VoiceSE_InitMouth( int entity )
{
	SND_ForceInitMouth( entity );
}

void VoiceSE_CloseMouth( int entity )
{
	SND_ForceCloseMouth( entity );
}

void VoiceSE_StartChannel( int iChannel )
{
	if ( (unsigned int)iChannel >= VOICE_NUM_CHANNELS )
	{
#if defined( _WIN32 )
		_wassert( L"!\"Shouldn't ever get here\"", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\voice_sound_engine_interface.cpp", 172 );
#endif
		return;
	}

	VoiceSE_SFX* pVoice = &g_VoiceSE_SFX[iChannel];
	pVoice->m_PrevUpsampleValue = 0;
	Q_memset( &pVoice->m_SFX, 0, sizeof( pVoice->m_SFX ) );
	Q_strcpy( pVoice->m_SFX.name, "?VoiceData" );

	S_StartDynamicSound(
		cl.viewentity, iChannel + CHAN_NETWORKVOICE_BASE, &pVoice->m_SFX, vec3_origin, 1.0f, 1.0f, 0, PITCH_NORM );
}

void VoiceSE_EndChannel( int iChannel )
{
	S_StopSound( cl.viewentity, iChannel + CHAN_NETWORKVOICE_BASE );
}

extern "C" void VoiceSE_NotifyFreeChannel( int iChannel )
{
#if defined( _WIN32 )
	if ( iChannel < CHAN_NETWORKVOICE_BASE )
		_wassert( L"iChannel >= CHAN_NETWORKVOICE_BASE", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\voice_sound_engine_interface.cpp", 239 );
#endif
	Voice_NotifyFreeChannel( iChannel - CHAN_NETWORKVOICE_BASE );
}

void VoiceSE_Idle( float frametime )
{
	if ( g_bVoiceOverdriveOn )
	{
		if ( g_VoiceOverdriveDuration == voice_overdrivefadetime.value )
			return;

		float duration = g_VoiceOverdriveDuration + frametime;
		g_VoiceOverdriveDuration = voice_overdrivefadetime.value < duration ? voice_overdrivefadetime.value : duration;
	}
	else
	{
		if ( g_VoiceOverdriveDuration == 0.0f )
			return;

		float duration = g_VoiceOverdriveDuration - frametime;
		g_VoiceOverdriveDuration = duration < 0.0f ? 0.0f : duration;
	}

	float percent = g_VoiceOverdriveDuration / voice_overdrivefadetime.value;
	percent = (float)( 0.5 - cos( percent * 3.1415926535 ) * 0.5 ); // Smooth it out..
	g_SND_VoiceOverdrive = percent * ( voice_overdrive.value - 1.0f ) + 1.0f;
}

void VoiceSE_MoveMouth( int entity, short* samples, int sampleCount )
{
	SND_MoveMouth16( entity, samples, sampleCount );
}

sfxcache_t* VoiceSE_GetSFXCache( sfx_t* pSFX )
{
	for ( int i = 0; i < VOICE_NUM_CHANNELS; i++ )
	{
		if ( &g_VoiceSE_SFX[i].m_SFX == pSFX )
			return &g_VoiceSE_SFX[i].m_SFXCache;
	}

#if defined( _WIN32 )
	_wassert( L"!\"Shouldn't get here!\"", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\voice_sound_engine_interface.cpp", 204 );
#endif
	return NULL;
}

int VoiceSE_GetSoundDataCallback(
	sfxcache_t* pCache, char* pCopyBuffer, int maxOutDataSize, int samplePos, int sampleCount )
{
	int iChannel;
	for ( iChannel = 0; iChannel < VOICE_NUM_CHANNELS; iChannel++ )
	{
		if ( &g_VoiceSE_SFX[iChannel].m_SFXCache == pCache )
			break;
	}

	if ( iChannel == VOICE_NUM_CHANNELS )
	{
#if defined( _WIN32 )
		_wassert( L"!\"Shouldn't get here!\"", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\voice_sound_engine_interface.cpp", 103 );
#endif
		return 0;
	}

	VoiceSE_SFX* pVoice = &g_VoiceSE_SFX[iChannel];
	int nSamples = Voice_GetOutputData( iChannel, pCopyBuffer, maxOutDataSize, samplePos, sampleCount >> 1 );
	short* pSamples = (short*)pCopyBuffer;
	int nOutSamples = nSamples * 2;

	for ( int i = nSamples - 1; i > 0; i-- )
	{
		int sample = pSamples[i];
		int prevSample = pSamples[i - 1];
		pSamples[i * 2 + 1] = (short)sample;
		pSamples[i * 2] = (short)( ( sample + prevSample ) >> 1 );
	}

	if ( nOutSamples >= 2 )
	{
		int sample = pSamples[0];
		pSamples[1] = (short)sample;
		pSamples[0] = (short)( ( sample + pVoice->m_PrevUpsampleValue ) >> 1 );
	}

	pVoice->m_PrevUpsampleValue = pSamples[nOutSamples - 1];
	return nOutSamples;
}

bool VoiceSE_Init()
{
	if ( !sound_started )
		return false;

	Q_memset( g_VoiceSE_SFX, 0, sizeof( g_VoiceSE_SFX ) );

	for ( int i = 0; i < VOICE_NUM_CHANNELS; i++ )
	{
		sfxcache_t* pCache = &g_VoiceSE_SFX[i].m_SFXCache;
		pCache->length = SOUND_CALLBACK_CACHE_LENGTH;
		pCache->loopstart = reinterpret_cast<intptr_t>( VoiceSE_GetSoundDataCallback );
		pCache->speed = 22050;
		pCache->width = 2;
	}

	return true;
}
