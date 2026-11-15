#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "A3dSource.h"
#include "A3d3.h"
#include "matrix.h"
#include "rotation.h"
#include "vector_math.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>

static const float sourceEqCurve[7] =
{
	3.7264376f,
	17.449827f,
	43.63063f,
	52.999165f,
	27.087683f,
	0.7774167f,
	3.7731748f
};

DWORD g_iSourceUpdateGroup;

static const IID IID_A3dSourcePrivate =
{
	0xdf7a8824, 0x555b, 0x11d2, { 0xa8, 0xe6, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 }
};

CA3dSource::CA3dSource( IDirectSound* pDirectSound, CA3d* pA3d, DWORD dwType )
{
	m_cReferences = 0;
	m_pDirectSound = pDirectSound;
	m_pDirectSound->AddRef();

	m_wBitsPerSample = 0;
	m_nWaveSize = 0;
	m_nSamplesPerSec = 0;
	m_nBlockAlign = 0;
	m_nChannels = 0;
	m_wFormatTag = 0;
	m_nAvgBytesPerSec = 0;
	m_flPlaybackPitch = 1.0f;
	m_pA3d = pA3d;
	m_pBuffer = NULL;
	m_pBuffer3d = NULL;
	m_pSourceBackend = NULL;
	m_dwRenderMode = A3DSOURCE_RENDERMODE_DEFAULT;
	m_flPitch = 1.0f;
	m_flGain = 1.0f;
	m_flEq = 1.0f;
	m_flEqFactor = 1.0f;
	m_flDopplerScale = 1.0f;
	m_flDistanceModelScale = 1.0f;
	m_flMinDistance = 1.0f;
	m_flMaxDistance = 5000.0f;
	m_dwMaxDistanceMute = 0;
	m_flStereoSpread = 0.0f;
	m_flDopplerFactor = 1.0f;
	m_flDistanceGain[0] = 1.0f;
	m_flDistanceGain[1] = 1.0f;
	m_flDistanceEq = 1.0f;
	m_flReservedFactor = 1.0f;
	m_flTargetOcclusion[0] = 1.0f;
	m_flTargetOcclusion[1] = 1.0f;
	m_flOcclusion[0] = 1.0f;
	m_flOcclusion[1] = 1.0f;
	m_flOcclusionStep[0] = 0.0f;
	m_flOcclusionStep[1] = 0.0f;
	m_flOcclusionFactor = 0.0f;
	m_flInnerConeAngle = 0.0f;
	m_flOuterConeAngle = 0.0f;
	m_flOuterConeGain = 1.0f;
	m_flConeGain = 1.0f;
	m_dwType = dwType;
	m_nPlaybackState = 0;
	m_dwLoopingFlags = 0;
	memset( m_vRenderPosition, 0, sizeof( m_vRenderPosition ) );
	m_vRenderPosition[3] = 1.0f;
	memset( m_vPosition, 0, sizeof( m_vPosition ) );
	m_vPosition[3] = 1.0f;
	memset( m_vOrientationAngles, 0, sizeof( m_vOrientationAngles ) );
	memset( m_vVelocity, 0, sizeof( m_vVelocity ) );
	memset( m_vDirection, 0, sizeof( m_vDirection ) );
	m_vDirection[2] = 1.0f;
	memset( m_vUp, 0, sizeof( m_vUp ) );
	m_vUp[1] = 1.0f;

	if ( pA3d->m_bListTransform )
		memcpy( m_Transform, pA3d->m_ListMatrix, sizeof( m_Transform ) );
	else
		Matrix_Identity( m_Transform );

	m_pReflections[0] = NULL;
	m_pReflections[1] = NULL;
	m_nReflections[0] = 0;
	m_nReflections[1] = 0;
	m_iReflectionBuffer = 0;
	m_pRenderStates[0] = (A3DCTRL_SRC_SUPER*)calloc( 1, sizeof( A3DCTRL_SRC_SUPER ) );
	m_pRenderStates[1] = (A3DCTRL_SRC_SUPER*)calloc( 1, sizeof( A3DCTRL_SRC_SUPER ) );
	m_iRenderState = 0;
	m_pRenderState = m_pRenderStates[0];
	m_iUpdateGroup = g_iSourceUpdateGroup;
	g_iSourceUpdateGroup = g_iSourceUpdateGroup == 0;
	m_bWithinReflectionRange = TRUE;
	m_bRegistered = TRUE;
	m_nPlaybackCounter = 0;
}

CA3dSource::~CA3dSource()
{
	CA3dSource::Stop();
	if ( m_bRegistered )
		m_pA3d->RemoveSource( this );

	if ( m_pSourceBackend )
	{
		m_pSourceBackend->Release();
		m_pSourceBackend = NULL;
	}
	if ( m_pBuffer3d )
	{
		m_pBuffer3d->Release();
		m_pBuffer3d = NULL;
	}
	if ( m_pBuffer )
	{
		m_pBuffer->Release();
		m_pBuffer = NULL;
	}
	if ( m_pDirectSound )
	{
		m_pDirectSound->Release();
		m_pDirectSound = NULL;
	}
}

HRESULT STDMETHODCALLTYPE CA3dSource::QueryInterface( REFIID iid, void** ppObject )
{
	if ( !ppObject )
		return E_INVALIDARG;

	*ppObject = NULL;
	if ( !memcmp( &iid, &IID_IUnknown, sizeof( IID ) ) )
		*ppObject = (IA3dSource202*)this;
	else if ( !memcmp( &iid, &IID_A3dSourcePrivate, sizeof( IID ) ) )
		*ppObject = (IA3dSourcePrivate*)this;

	return *ppObject ? S_OK : E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CA3dSource::AddRef()
{
	return ++m_cReferences;
}

ULONG STDMETHODCALLTYPE CA3dSource::Release()
{
	if ( !--m_cReferences )
	{
		delete this;
		return 0;
	}

	return m_cReferences;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetWaveSize()
{
	return m_nWaveSize;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetType( LPDWORD pdwType )
{
	*pdwType = m_dwType;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::Rewind()
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetWaveTime( A3DVAL )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetWaveTime( LPA3DVAL )
{
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetWavePosition( DWORD dwPosition )
{
	if ( m_pBuffer )
		m_pBuffer->SetCurrentPosition( dwPosition );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetWavePosition( LPDWORD pdwPosition )
{
	DWORD dwWritePosition;

	if ( m_pBuffer )
		m_pBuffer->GetCurrentPosition( pdwPosition, &dwWritePosition );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetPosition3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	float position[3] = { x, y, z };
	return SetPosition3fv( position );
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetPosition3f( LPA3DVAL pX, LPA3DVAL pY, LPA3DVAL pZ )
{
	float position[3];
	GetPosition3fv( position );
	*pX = position[0];
	*pY = position[1];
	*pZ = position[2];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetPosition3fv( LPA3DVAL pPosition )
{
	m_vPosition[0] = pPosition[0];
	m_vPosition[1] = pPosition[1];
	m_vPosition[2] = pPosition[2];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetPosition3fv( LPA3DVAL pPosition )
{
	pPosition[0] = m_vPosition[0];
	pPosition[1] = m_vPosition[1];
	pPosition[2] = m_vPosition[2];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetOrientationAngles3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	float angles[3] = { x, y, z };
	return SetOrientationAngles3fv( angles );
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetOrientationAngles3f( LPA3DVAL pX, LPA3DVAL pY, LPA3DVAL pZ )
{
	float angles[3];
	GetOrientationAngles3fv( angles );
	*pX = angles[0];
	*pY = angles[1];
	*pZ = angles[2];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetOrientationAngles3fv( LPA3DVAL pAngles )
{
	m_vOrientationAngles[0] = pAngles[0];
	m_vOrientationAngles[1] = pAngles[1];
	m_vOrientationAngles[2] = pAngles[2];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetOrientationAngles3fv( LPA3DVAL pAngles )
{
	pAngles[0] = m_vOrientationAngles[0];
	pAngles[1] = m_vOrientationAngles[1];
	pAngles[1] = m_vOrientationAngles[2];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetOrientation6f( A3DVAL dirX, A3DVAL dirY, A3DVAL dirZ,
		A3DVAL upX, A3DVAL upY, A3DVAL upZ )
{
	float orientation[6] = { dirX, dirY, dirZ, upX, upY, upZ };
	return SetOrientation6fv( orientation );
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetOrientation6f( LPA3DVAL pDirX, LPA3DVAL pDirY, LPA3DVAL pDirZ,
		LPA3DVAL pUpX, LPA3DVAL pUpY, LPA3DVAL pUpZ )
{
	float orientation[6];
	GetOrientation6fv( orientation );
	*pDirX = orientation[0];
	*pDirY = orientation[1];
	*pDirZ = orientation[2];
	*pUpX = orientation[3];
	*pUpY = orientation[4];
	*pUpZ = orientation[5];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetOrientation6fv( LPA3DVAL pOrientation )
{
	m_vDirection[0] = pOrientation[0];
	m_vDirection[1] = pOrientation[1];
	m_vDirection[2] = pOrientation[2];
	m_vUp[0] = pOrientation[3];
	m_vUp[1] = pOrientation[4];
	m_vUp[2] = pOrientation[5];

	float angles[3];
	Vector_ToSphericalDegrees( m_vDirection, angles );
	m_vOrientationAngles[0] = angles[0];
	m_vOrientationAngles[1] = angles[1];
	m_vOrientationAngles[2] = 0.0f;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetOrientation6fv( LPA3DVAL pOrientation )
{
	pOrientation[0] = m_vDirection[0];
	pOrientation[1] = m_vDirection[1];
	pOrientation[2] = m_vDirection[2];
	pOrientation[3] = m_vUp[0];
	pOrientation[4] = m_vUp[1];
	pOrientation[5] = m_vUp[2];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetVelocity3f( A3DVAL x, A3DVAL y, A3DVAL z )
{
	float velocity[3] = { x, y, z };
	return SetVelocity3fv( velocity );
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetVelocity3f( LPA3DVAL pX, LPA3DVAL pY, LPA3DVAL pZ )
{
	float velocity[3];
	GetVelocity3fv( velocity );
	*pX = velocity[0];
	*pY = velocity[1];
	*pZ = velocity[2];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetVelocity3fv( LPA3DVAL pVelocity )
{
	m_vVelocity[0] = pVelocity[0];
	m_vVelocity[1] = pVelocity[1];
	m_vVelocity[2] = pVelocity[2];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetVelocity3fv( LPA3DVAL pVelocity )
{
	pVelocity[0] = m_vVelocity[0];
	pVelocity[1] = m_vVelocity[1];
	pVelocity[2] = m_vVelocity[2];
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetCone( A3DVAL flInnerAngle, A3DVAL flOuterAngle, A3DVAL flOuterGain )
{
	m_flInnerConeAngle = flInnerAngle;
	m_flOuterConeAngle = flOuterAngle;

	if ( !( flOuterGain >= 0.0f && flOuterGain <= 1.0f ) )
		return E_INVALIDARG;

	m_flOuterConeGain = flOuterGain;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetCone( LPA3DVAL pInnerAngle, LPA3DVAL pOuterAngle, LPA3DVAL pOuterGain )
{
	*pInnerAngle = m_flInnerConeAngle;
	*pOuterAngle = m_flOuterConeAngle;
	*pOuterGain = m_flOuterConeGain;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetMinMaxDistance( A3DVAL flMinDistance, A3DVAL flMaxDistance,
		DWORD dwMaxDistanceMute )
{
	m_flMinDistance = flMinDistance > A3D_EPSILON ? flMinDistance : A3D_EPSILON;
	m_flMaxDistance = flMaxDistance > A3D_EPSILON ? flMaxDistance : A3D_EPSILON;
	m_dwMaxDistanceMute = dwMaxDistanceMute;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetMinMaxDistance( LPA3DVAL pMinDistance, LPA3DVAL pMaxDistance,
		LPDWORD pdwMaxDistanceMute )
{
	*pMinDistance = m_flMinDistance;
	*pMaxDistance = m_flMaxDistance;
	*pdwMaxDistanceMute = m_dwMaxDistanceMute;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetGain( A3DVAL flGain )
{
	if ( !( flGain >= 0.0f && flGain <= 1.0f ) )
		return E_INVALIDARG;

	m_flGain = flGain;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetGain( LPA3DVAL pGain )
{
	if ( !pGain )
		return E_INVALIDARG;

	*pGain = m_flGain;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetPitch( A3DVAL flPitch )
{
	m_flPitch = flPitch;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetPitch( LPA3DVAL pPitch )
{
	*pPitch = m_flPitch;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetDopplerScale( A3DVAL flScale )
{
	m_flDopplerScale = flScale;
	if ( !( flScale >= 0.0f ) )
		m_flDopplerScale = 0.0f;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetDopplerScale( LPA3DVAL pScale )
{
	*pScale = m_flDopplerScale;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetDistanceModelScale( A3DVAL flScale )
{
	m_flDistanceModelScale = flScale;
	if ( !( flScale >= 0.0f ) )
		m_flDistanceModelScale = 0.0f;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetDistanceModelScale( LPA3DVAL pScale )
{
	*pScale = m_flDistanceModelScale;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetEq( A3DVAL flEq )
{
	double x = flEq;
	double square = x * x;
	float cube = (float)( x * square );
	float fourth = (float)( x * cube );
	float fifth = (float)( x * fourth );
	double sixth = x * fifth;
	float roundedSixth = (float)sixth;
	m_flEq = flEq;

	double flFactor = x * sourceEqCurve[0] - square * sourceEqCurve[1];
	flFactor = flFactor + cube * (double)sourceEqCurve[2];
	flFactor = flFactor - fourth * (double)sourceEqCurve[3];
	flFactor = flFactor + fifth * (double)sourceEqCurve[4];
	flFactor = flFactor + sixth * sourceEqCurve[5];
	flFactor = flFactor - ( x * roundedSixth ) * sourceEqCurve[6];
	m_flEqFactor = (float)flFactor;

	if ( !( flFactor >= 0.0 ) )
		m_flEqFactor = 0.0f;
	else if ( m_flEqFactor > 1.0f )
		m_flEqFactor = 1.0f;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetEq( LPA3DVAL pEq )
{
	*pEq = m_flEq;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetRenderMode( DWORD dwMode )
{
	m_dwRenderMode = dwMode;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetRenderMode( LPDWORD pdwMode )
{
	*pdwMode = m_dwRenderMode;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetOcclusionFactor( LPA3DVAL pFactor )
{
	*pFactor = m_flOcclusionFactor;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetAudibility( LPA3DVAL pAudibility )
{
	A3DCTRL_SRC_SUPER* pState = m_pRenderStates[m_iRenderState == 0];
	*pAudibility = pState->flAudibility;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::CompatibilityOperation( DWORD )
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::InitializeReflectionSlots( A3dSourceReflection** ppReflections )
{
	if ( !ppReflections )
		return E_INVALIDARG;

	for ( int i = 0; i < MAX_A3D_SOURCE_REFLECTIONS; i++ )
	{
		ppReflections[i]->iSlot = i;
		ppReflections[i]->dwTag = 0;
	}

	return S_OK;
}

void CA3dSource::GetWorldTransform( float matrix[16] )
{
	memcpy( matrix, m_Transform, sizeof( m_Transform ) );
	Matrix_Translate( m_vPosition, matrix );
	float axis[3] = { 0.0f, 1.0f, 0.0f };
	Matrix_Rotate( m_vOrientationAngles[0], axis, matrix );
	axis[0] = 1.0f;
	axis[1] = 0.0f;
	Matrix_Rotate( m_vOrientationAngles[1], axis, matrix );
	axis[0] = 0.0f;
	axis[2] = 1.0f;
	Matrix_Rotate( m_vOrientationAngles[2], axis, matrix );
}

void CA3dSource::GetWorldVelocity( float velocity[3] )
{
	float matrix[16];
	memcpy( matrix, m_Transform, sizeof( matrix ) );
	matrix[12] = 0.0f;
	matrix[13] = 0.0f;
	matrix[14] = 0.0f;
	Matrix_TransformVector( m_vVelocity, matrix, velocity );
}

#define A3D_COMPAT_MIN_SAMPLE_RATE		22000
#define A3D_COMPAT_BUFFER_SAMPLE_RATE	22050
#define A3D_COMPAT_SAMPLE_RATE_SCALE		4.5351473e-5f

static const IID IID_A3dRenderBuffer =
{
	0xe04814d8, 0x26fc, 0x11d2, { 0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00 }
};

typedef struct wave_file_header_s
{
	FOURCC	id;
	DWORD	size;
	FOURCC	type;
} wave_file_header_t;

typedef struct wave_file_chunk_s
{
	FOURCC	id;
	DWORD	size;
} wave_file_chunk_t;

HRESULT STDMETHODCALLTYPE CA3dSource::LoadWaveFile( LPSTR pFilename )
{
	int hFile = _open( pFilename, _O_BINARY | _O_RDONLY );
	if ( hFile == -1 )
		return A3DERROR_FAILED_FILE_OPEN;

	wave_file_header_t header;
	int nRead = _read( hFile, &header, sizeof( header ) );
	while ( nRead )
	{
		if ( nRead != (int)sizeof( header ) )
		{
			_close( hFile );
			return A3DERROR_UNRECOGNIZED_FORMAT;
		}

		if ( header.type == mmioFOURCC( 'W', 'A', 'V', 'E' ) )
			break;

		_lseek( hFile, (long)header.size, SEEK_CUR );
		nRead = _read( hFile, &header, sizeof( header ) );
	}

	wave_file_chunk_t chunk;
	_read( hFile, &chunk, sizeof( chunk ) );
	if ( chunk.id != mmioFOURCC( 'f', 'm', 't', ' ' ) )
	{
		_close( hFile );
		return A3DERROR_UNRECOGNIZED_FORMAT;
	}

	DWORD dwFormatPosition = (DWORD)_tell( hFile );
	PCMWAVEFORMAT format;
	_read( hFile, &format, sizeof( format ) );
	_lseek( hFile, (long)( dwFormatPosition + chunk.size ), SEEK_SET );
	_read( hFile, &chunk, sizeof( chunk ) );
	if ( chunk.id != mmioFOURCC( 'd', 'a', 't', 'a' ) )
	{
		_close( hFile );
		return A3DERROR_UNRECOGNIZED_FORMAT;
	}

	m_nSamplesPerSec = format.wf.nSamplesPerSec;
	m_nBlockAlign = format.wf.nBlockAlign;
	m_wBitsPerSample = format.wBitsPerSample;
	m_nChannels = format.wf.nChannels;
	m_wFormatTag = format.wf.wFormatTag;
	m_nWaveSize = chunk.size;
	m_nAvgBytesPerSec = format.wf.nAvgBytesPerSec;
	if ( FAILED( AllocateWaveData( (INT)chunk.size ) ) )
	{
		_close( hFile );
		return A3DERROR_FAILED_ALLOCATE_WAVEDATA;
	}

	LPVOID pBuffer1 = NULL;
	LPVOID pBuffer2 = NULL;
	DWORD dwSize1;
	DWORD dwSize2;
	if ( FAILED( m_pBuffer->Lock( 0, m_nWaveSize, &pBuffer1, &dwSize1, &pBuffer2, &dwSize2, 0 ) ) )
	{
		if ( !pBuffer1 )
			m_pBuffer->Unlock( NULL, m_nWaveSize, pBuffer2, 0 );

		if ( m_pBuffer )
		{
			m_pBuffer->Release();
			m_pBuffer = NULL;
		}
		_close( hFile );
		return A3DERROR_FAILED_LOCK_BUFFER;
	}

	memset( pBuffer1, 0, m_nWaveSize );
	_read( hFile, pBuffer1, chunk.size );
	_close( hFile );
	if ( FAILED( m_pBuffer->Unlock( pBuffer1, m_nWaveSize, pBuffer2, 0 ) ) )
	{
		if ( m_pBuffer )
		{
			m_pBuffer->Release();
			m_pBuffer = NULL;
		}
		return A3DERROR_FAILED_UNLOCK_BUFFER;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::AllocateWaveData( INT nSize )
{
	if ( m_pSourceBackend )
	{
		m_pSourceBackend->Release();
		m_pSourceBackend = NULL;
	}

	if ( m_pBuffer3d )
	{
		m_pBuffer3d->Release();
		m_pBuffer3d = NULL;
	}

	if ( m_pBuffer )
	{
		m_pBuffer->Release();
		m_pBuffer = NULL;
	}

	if ( !m_wBitsPerSample )
		return A3DERROR_NEEDS_FORMAT_INFORMATION;

	DSBUFFERDESC1 desc = {};
	desc.dwSize = sizeof( desc );
	desc.dwFlags = DSBCAPS_STATIC | DSBCAPS_LOCHARDWARE | DSBCAPS_CTRL3D | DSBCAPS_CTRLFREQUENCY |
		DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2;
	if ( m_pA3d->m_bCompat1011 )
		desc.dwFlags |= DSBCAPS_GLOBALFOCUS;
	desc.dwBufferBytes = nSize;

	WAVEFORMATEX format;
	format.wFormatTag = m_wFormatTag;
	format.nChannels = m_nChannels;
	format.nSamplesPerSec = m_nSamplesPerSec;
	if ( m_pA3d->m_bCompat1011 && m_nSamplesPerSec < A3D_COMPAT_MIN_SAMPLE_RATE )
	{
		format.nSamplesPerSec = A3D_COMPAT_BUFFER_SAMPLE_RATE;
		m_flPlaybackPitch = (float)( (double)m_nSamplesPerSec * A3D_COMPAT_SAMPLE_RATE_SCALE );
	}
	format.nAvgBytesPerSec = m_nAvgBytesPerSec;
	format.nBlockAlign = m_nBlockAlign;
	format.wBitsPerSample = m_wBitsPerSample;
	format.cbSize = 0;
	desc.lpwfxFormat = &format;

	if ( FAILED( m_pDirectSound->CreateSoundBuffer( (LPCDSBUFFERDESC)&desc, &m_pBuffer, NULL ) ) )
		return A3DERROR_FAILED_CREATE_SOUNDBUFFER;

	if ( FAILED( m_pBuffer->QueryInterface( IID_IDirectSound3DBuffer, (void**)&m_pBuffer3d ) ) )
		return A3DERROR_FAILED_QUERY_3DINTERFACE;

	if ( FAILED( m_pBuffer->QueryInterface( IID_A3dRenderBuffer, (void**)&m_pSourceBackend ) ) )
		m_pSourceBackend = NULL;

	InitializeRenderState();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::Play( INT nPlayMode )
{
	if ( !m_pBuffer )
		return A3DERROR_NO_WAVE_DATA;

	DWORD dwFlags;
	if ( nPlayMode == A3D_SINGLE )
		dwFlags = 0;
	else if ( nPlayMode == A3D_LOOPED )
		dwFlags = DSBPLAY_LOOPING;
	else
		return A3DERROR_UNKNOWN_PLAYMODE;

	if ( m_pA3d->m_dwCompatValue1000 )
	{
		m_nPlaybackState = 0;
		float position[3] = { 0.0f, 0.0f, 0.0f };
		UpdateRenderState( position, position );
		SubmitRenderState();
		if ( m_pA3d->m_bCompat1011 )
		{
			DWORD dwPosition;
			m_pBuffer->GetCurrentPosition( &dwPosition, &dwPosition );
			if ( FAILED( m_pBuffer->Play( 0, 0, dwFlags ) ) )
				return A3DERROR_FAILED_PLAY;
			SubmitRenderState();
		}
		else if ( FAILED( m_pBuffer->Play( 0, 0, dwFlags ) ) )
		{
			return A3DERROR_FAILED_PLAY;
		}
		m_nPlaybackState = 2;
	}
	else
	{
		m_nPlaybackState = 1;
	}

	m_dwLoopingFlags = dwFlags;
	m_nPlaybackCounter = 0;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::Stop()
{
	if ( !m_pBuffer )
		return A3DERROR_NO_WAVE_DATA;

	m_nPlaybackState = 0;
	if ( m_pA3d->m_bCompat1011 )
	{
		ResetReflectionChannels();
		SubmitRenderState();
		DisableReflectionChannels();
		SubmitRenderState();
	}

	if ( FAILED( m_pBuffer->Stop() ) )
		return A3DERROR_FAILED_STOP;

	return S_OK;
}

typedef struct riff_header_s
{
	FOURCC	id;
	DWORD	size;
	FOURCC	type;
} riff_header_t;

typedef struct riff_chunk_s
{
	FOURCC	id;
	DWORD	size;
} riff_chunk_t;

HRESULT STDMETHODCALLTYPE CA3dSource::LoadWaveData( LPVOID pData, DWORD dwSize )
{
	if ( !pData )
		return E_INVALIDARG;

	if ( dwSize < sizeof( riff_header_t ) )
		return A3DERROR_UNRECOGNIZED_FORMAT;

	riff_header_t header;
	DWORD dwPosition = 0;
	for ( ;; )
	{
		memcpy( &header, (BYTE*)pData + dwPosition, sizeof( header ) );
		dwPosition += sizeof( header );
		if ( header.type == mmioFOURCC( 'W', 'A', 'V', 'E' ) )
			break;

		dwPosition += header.size;
		if ( dwPosition > dwSize - sizeof( header ) )
			return A3DERROR_UNRECOGNIZED_FORMAT;
	}

	riff_chunk_t chunk;
	if ( dwSize - dwPosition < sizeof( chunk ) )
		return A3DERROR_UNRECOGNIZED_FORMAT;
	memcpy( &chunk, (BYTE*)pData + dwPosition, sizeof( chunk ) );
	dwPosition += sizeof( chunk );
	if ( chunk.id != mmioFOURCC( 'f', 'm', 't', ' ' ) )
		return A3DERROR_UNRECOGNIZED_FORMAT;

	PCMWAVEFORMAT format;
	if ( dwSize - dwPosition < sizeof( format ) )
		return A3DERROR_UNRECOGNIZED_FORMAT;
	memcpy( &format, (BYTE*)pData + dwPosition, sizeof( format ) );
	dwPosition += chunk.size;
	if ( dwPosition > dwSize - sizeof( chunk ) )
		return A3DERROR_UNRECOGNIZED_FORMAT;
	memcpy( &chunk, (BYTE*)pData + dwPosition, sizeof( chunk ) );
	dwPosition += sizeof( chunk );
	if ( chunk.id != mmioFOURCC( 'd', 'a', 't', 'a' ) )
		return A3DERROR_UNRECOGNIZED_FORMAT;
	if ( chunk.size > dwSize - dwPosition )
		return A3DERROR_UNRECOGNIZED_FORMAT;

	m_wBitsPerSample = format.wBitsPerSample;
	m_nSamplesPerSec = format.wf.nSamplesPerSec;
	m_nBlockAlign = format.wf.nBlockAlign;
	m_nChannels = format.wf.nChannels;
	m_wFormatTag = format.wf.wFormatTag;
	m_nWaveSize = chunk.size;
	m_nAvgBytesPerSec = format.wf.nAvgBytesPerSec;
	if ( FAILED( AllocateWaveData( (INT)chunk.size ) ) )
		return A3DERROR_FAILED_ALLOCATE_WAVEDATA;

	LPVOID pBuffer1 = NULL;
	LPVOID pBuffer2 = NULL;
	DWORD dwSize1;
	DWORD dwSize2;
	if ( FAILED( m_pBuffer->Lock( 0, m_nWaveSize, &pBuffer1, &dwSize1, &pBuffer2, &dwSize2, 0 ) ) )
	{
		if ( !pBuffer1 )
			m_pBuffer->Unlock( NULL, m_nWaveSize, pBuffer2, 0 );

		if ( m_pBuffer )
		{
			m_pBuffer->Release();
			m_pBuffer = NULL;
		}
		return A3DERROR_FAILED_LOCK_BUFFER;
	}

	memcpy( pBuffer1, (BYTE*)pData + dwPosition, m_nWaveSize );
	if ( FAILED( m_pBuffer->Unlock( pBuffer1, m_nWaveSize, pBuffer2, 0 ) ) )
	{
		if ( m_pBuffer )
		{
			m_pBuffer->Release();
			m_pBuffer = NULL;
		}
		return A3DERROR_FAILED_UNLOCK_BUFFER;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::FreeWaveData()
{
	if ( !m_pBuffer )
		return A3DERROR_NO_WAVE_DATA;

	m_pBuffer->Release();
	m_pBuffer = NULL;

	if ( m_pBuffer3d )
	{
		m_pBuffer3d->Release();
		m_pBuffer3d = NULL;
	}

	if ( m_pSourceBackend )
	{
		m_pSourceBackend->Release();
		m_pSourceBackend = NULL;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetWaveFormat( LPVOID pFormat )
{
	if ( !pFormat )
		return E_INVALIDARG;

	WAVEFORMATEX* pWaveFormat = (WAVEFORMATEX*)pFormat;
	m_wFormatTag = pWaveFormat->wFormatTag;
	m_nChannels = pWaveFormat->nChannels;
	m_nSamplesPerSec = pWaveFormat->nSamplesPerSec;
	m_nAvgBytesPerSec = pWaveFormat->nAvgBytesPerSec;
	m_nBlockAlign = pWaveFormat->nBlockAlign;
	m_wBitsPerSample = pWaveFormat->wBitsPerSample;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetWaveFormat( LPVOID pFormat )
{
	if ( !pFormat )
		return E_INVALIDARG;

	WAVEFORMATEX* pWaveFormat = (WAVEFORMATEX*)pFormat;
	pWaveFormat->wFormatTag = m_wFormatTag;
	pWaveFormat->nChannels = m_nChannels;
	pWaveFormat->nSamplesPerSec = m_nSamplesPerSec;
	pWaveFormat->nAvgBytesPerSec = m_nAvgBytesPerSec;
	pWaveFormat->nBlockAlign = m_nBlockAlign;
	pWaveFormat->wBitsPerSample = m_wBitsPerSample;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::Lock( DWORD dwOffset, DWORD dwSize, LPVOID* ppBuffer1,
		LPDWORD pdwSize1, LPVOID* ppBuffer2, LPDWORD pdwSize2, DWORD dwFlags )
{
	if ( !m_pBuffer )
		return A3DERROR_NO_WAVE_DATA;

	if ( FAILED( m_pBuffer->Lock( dwOffset, dwSize, ppBuffer1, pdwSize1, ppBuffer2, pdwSize2, dwFlags ) ) )
		return A3DERROR_FAILED_LOCK_BUFFER;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::Unlock( LPVOID pBuffer1, DWORD dwSize1, LPVOID pBuffer2, DWORD dwSize2 )
{
	if ( !m_pBuffer )
		return A3DERROR_NO_WAVE_DATA;

	if ( FAILED( m_pBuffer->Unlock( pBuffer1, dwSize1, pBuffer2, dwSize2 ) ) )
		return A3DERROR_FAILED_UNLOCK_BUFFER;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CA3dSource::SetPriority( A3DVAL priority )
{
	m_flPriority = priority;
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetPriority( LPA3DVAL pPriority )
{
	if ( !pPriority )
		return E_INVALIDARG;

	*pPriority = m_flPriority;
	return A3DERROR_UNIMPLEMENTED_FUNCTION;
}

HRESULT STDMETHODCALLTYPE CA3dSource::GetStatus( LPDWORD pdwStatus )
{
	if ( !pdwStatus )
		return E_INVALIDARG;

	if ( m_pBuffer )
		m_pBuffer->GetStatus( pdwStatus );
	else
		*pdwStatus = 0;

	if ( m_nPlaybackState == 1 )
		*pdwStatus |= A3DSTATUS_WAITING_FOR_FLUSH;

	return S_OK;
}
