#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "resman.h"
#include "dalinfo.h"
#include "rmstatbuffer.h"
#include "A3dGeom.h"
#include "dal_a2d.h"

#include <string.h>
#include <new>
#include <stdlib.h>
#include <stdio.h>

#define A3D_PLATFORM_LIST_BLOCK_NODES	10
#define A3D_PLATFORM_MAX_FREQUENCY	100000

static const IID IID_A3dRenderBuffer =
	{ 0xe04814d8, 0x26fc, 0x11d2, { 0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00 } };
static const IID IID_A3dBufferControl =
	{ 0xcbd91fa8, 0x41c6, 0x11d2, { 0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00 } };
static const IID IID_A3dSoftwareBufferControl =
	{ 0x03509d01, 0x74d0, 0x11d2, { 0x97, 0x1b, 0x00, 0x60, 0x08, 0x8d, 0x87, 0x88 } };
static const IID IID_A3dPlatformAudio =
	{ 0xcbd91fa7, 0x41c6, 0x11d2, { 0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00 } };
static const IID IID_A3dPlatformDeviceBackend =
	{ 0xa3c62f2a, 0x2671, 0x11d2, { 0xbb, 0x48, 0x00, 0x60, 0x08, 0x2f, 0x3c, 0x00 } };

ResMan::ResMan()
{
	memset( &m_SoftwareBuffers, 0, sizeof( m_SoftwareBuffers ) );
	m_SoftwareBuffers.cBlockNodes = A3D_PLATFORM_LIST_BLOCK_NODES;
	memset( &m_PendingBuffers, 0, sizeof( m_PendingBuffers ) );
	m_PendingBuffers.cBlockNodes = A3D_PLATFORM_LIST_BLOCK_NODES;
	memset( &m_HardwareBuffers, 0, sizeof( m_HardwareBuffers ) );
	m_HardwareBuffers.cBlockNodes = A3D_PLATFORM_LIST_BLOCK_NODES;
	m_pA3dBackend = NULL;
	m_dwReserved = 0;
	m_pDefaultPool = NULL;
	m_pHrtfPool = NULL;
	m_pSecondaryPool = NULL;
	m_dwResourceManagerMode = A3D_RESOURCE_MODE_OFF;
	m_hUpdateEvent = INVALID_HANDLE_VALUE;
	m_dwInitialized = 0;
	m_dwThreadId = 0;
	m_hThread = NULL;
	m_dwWaitTime = 0;
	m_dwUpdateInterval = 0;
	m_dwPreviousUpdate = 0;
	m_cReferences = 0;
}

ULONG STDMETHODCALLTYPE ResMan::AddRef()
{
	return InterlockedIncrement( &m_cReferences );
}

HRESULT STDMETHODCALLTYPE ResMan::QueryInterface( REFIID iid, void** ppObject )
{
	if ( !ppObject )
		return E_INVALIDARG;

	IUnknown* pObject;
	if ( !memcmp( &iid, &IID_IUnknown, sizeof( IID ) ) ||
		!memcmp( &iid, &IID_A3dPlatformAudio, sizeof( IID ) ) ||
		!memcmp( &iid, &IID_IDirectSound, sizeof( IID ) ) )
	{
		pObject = (IDirectSound*)this;
	}
	else if ( !memcmp( &iid, &IID_A3dPlatformDeviceBackend, sizeof( IID ) ) )
	{
		pObject = (IA3dDal*)this;
	}
	else if ( !memcmp( &iid, &IID_IA3d, sizeof( IID ) ) ||
		!memcmp( &iid, &IID_IA3d2, sizeof( IID ) ) )
	{
		pObject = (IA3d2*)this;
	}
	else
	{
		*ppObject = NULL;
		return E_NOINTERFACE;
	}

	*ppObject = pObject;
	pObject->AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE ResMan::Release()
{
	if ( !InterlockedDecrement( &m_cReferences ) )
		return 0;

	return m_cReferences;
}

HRESULT STDMETHODCALLTYPE ResMan::GetCaps( LPDSCAPS pCaps )
{
	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

	return pDirectSound->GetCaps( pCaps );
}

HRESULT STDMETHODCALLTYPE ResMan::SetCooperativeLevel( HWND hWindow, DWORD dwLevel )
{
	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

	return pDirectSound->SetCooperativeLevel( hWindow, dwLevel );
}

HRESULT STDMETHODCALLTYPE ResMan::Compact()
{
	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

	return pDirectSound->Compact();
}

HRESULT STDMETHODCALLTYPE ResMan::GetSpeakerConfig( LPDWORD pdwConfig )
{
	if ( !pdwConfig )
		return E_POINTER;

	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

	return pDirectSound->GetSpeakerConfig( pdwConfig );
}

HRESULT STDMETHODCALLTYPE ResMan::SetSpeakerConfig( DWORD dwConfig )
{
	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

	return pDirectSound->SetSpeakerConfig( dwConfig );
}

HRESULT STDMETHODCALLTYPE ResMan::Initialize( LPCGUID pDevice )
{
	IDirectSound* pDirectSound = NULL;
	HRESULT hr = ( (IA3dDal*)this )->GetDS( &pDirectSound );
	if ( FAILED( hr ) )
		return hr;
	if ( !pDirectSound )
		return E_FAIL;

	return pDirectSound->Initialize( pDevice );
}

HRESULT STDMETHODCALLTYPE ResMan::CreateSoundBuffer( LPCDSBUFFERDESC pDesc,
		IDirectSoundBuffer** ppBuffer, IUnknown* pOuter )
{
	if ( !m_dwInitialized )
		return DSERR_UNINITIALIZED;
	if ( pOuter )
		return DSERR_INVALIDPARAM;

	a3d_platform_buffer_desc_t desc;
	memcpy( &desc, pDesc, sizeof( desc ) );
	if ( desc.dwFlags & DSBCAPS_PRIMARYBUFFER )
		return CreatePrimaryBuffer( &desc, ppBuffer, NULL );

	desc.dwFlags |= DSBCAPS_LOCHARDWARE | DSBCAPS_CTRL3D;
	return CreateSecondaryBuffer( &desc, ppBuffer, NULL );
}

HRESULT ResMan::CreatePrimaryBuffer( a3d_platform_buffer_desc_t* pDesc,
		IDirectSoundBuffer** ppBuffer, IUnknown* pOuter )
{
	BYTE bMode = 0;
	pDesc->dwFlags |= DSBCAPS_PRIMARYBUFFER;
	return ( (IA3dDal*)this )->CreateSoundBufferEx( pDesc, &bMode, ppBuffer, pOuter );
}

HRESULT STDMETHODCALLTYPE ResMan::InitializeEx( DWORD dwFirst, DWORD dwSecond,
		void* pConfiguration, void* pBackend )
{
	if ( !pBackend )
		return E_POINTER;

	return StartRenderer( dwFirst, dwSecond, pConfiguration, pBackend );
}

HRESULT STDMETHODCALLTYPE ResMan::CreateSoundBufferEx( const a3d_platform_buffer_desc_t* pDesc,
		BYTE* pbMode, IDirectSoundBuffer** ppBuffer, IUnknown* pOuter )
{
	if ( !pDesc || !pbMode || !ppBuffer )
		return E_POINTER;
	if ( pOuter )
		return E_INVALIDARG;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->CreateSoundBufferEx(
		pDesc, pbMode, ppBuffer, NULL );
}

HRESULT STDMETHODCALLTYPE ResMan::GetDS( IDirectSound** ppDirectSound )
{
	if ( !m_dwInitialized )
		return DSERR_UNINITIALIZED;
	if ( !ppDirectSound )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->GetDS( ppDirectSound );
}

HRESULT STDMETHODCALLTYPE ResMan::GetA3dCaps( void* pFirst, void* pSecond )
{
	if ( !pFirst || !pSecond )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->GetA3dCaps( pFirst, pSecond );
}

HRESULT STDMETHODCALLTYPE ResMan::GetDriverInfo( void* pFirst, void* pSecond,
		void* pThird, void* pFourth )
{
	if ( !pFirst || !pSecond || !pThird || !pFourth )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->GetDriverInfo(
		pFirst, pSecond, pThird, pFourth );
}

HRESULT STDMETHODCALLTYPE ResMan::GetDSDriverDesc( void* pFirst, void* pSecond )
{
	if ( !pFirst || !pSecond )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->GetDSDriverDesc( pFirst, pSecond );
}

HRESULT STDMETHODCALLTYPE ResMan::QueryFunctionality( DWORD dwFirst, void* pSecond )
{
	if ( !pSecond )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->QueryFunctionality( dwFirst, pSecond );
}

HRESULT STDMETHODCALLTYPE ResMan::DeviceCompatibility5( void* pFirst, void* pSecond, void* pThird )
{
	if ( !pFirst || !pSecond || !pThird )
		return E_POINTER;

	return ( (IA3dDal*)m_pDefaultPool->GetIDal() )->DeviceCompatibility5( pFirst, pSecond, pThird );
}

HRESULT STDMETHODCALLTYPE ResMan::SetOutputMode( DWORD dwFront, DWORD dwBack, DWORD dwQuad )
{
	if ( dwFront <= OUTPUT_HEADPHONES || dwFront >= OUTPUT_SPEAKERS_NARROW ||
		dwBack <= OUTPUT_HEADPHONES || dwBack >= OUTPUT_SPEAKERS_NARROW ||
		dwQuad <= OUTPUT_MODE_STEREO || dwQuad >= OUTPUT_MODE_QUAD )
		return E_INVALIDARG;

	return m_pA3dBackend->SetOutputMode( dwFront, dwBack, dwQuad );
}

HRESULT STDMETHODCALLTYPE ResMan::GetOutputMode( LPDWORD pdwFront, LPDWORD pdwBack, LPDWORD pdwQuad )
{
	if ( !pdwFront || !pdwBack || !pdwQuad )
		return E_POINTER;

	return m_pA3dBackend->GetOutputMode( pdwFront, pdwBack, pdwQuad );
}

HRESULT STDMETHODCALLTYPE ResMan::SetResourceManagerMode( DWORD dwMode )
{
	if ( dwMode > A3D_RESOURCE_MODE_LAST )
		return E_INVALIDARG;

	m_dwResourceManagerMode = dwMode;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ResMan::GetResourceManagerMode( LPDWORD pdwMode )
{
	if ( !pdwMode )
		return E_POINTER;

	*pdwMode = m_dwResourceManagerMode;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ResMan::SetHFAbsorbFactor( FLOAT flFactor )
{
	return m_pA3dBackend->SetHFAbsorbFactor( flFactor );
}

HRESULT STDMETHODCALLTYPE ResMan::GetHFAbsorbFactor( FLOAT* pflFactor )
{
	return m_pA3dBackend->GetHFAbsorbFactor( pflFactor );
}

HRESULT STDMETHODCALLTYPE ResMan::RegisterVersion( DWORD dwVersion )
{
	return m_pA3dBackend->RegisterVersion( dwVersion );
}

HRESULT STDMETHODCALLTYPE ResMan::GetSoftwareCaps( LPA3DCAPS_SOFTWARE pCaps )
{
	return m_pA3dBackend->GetSoftwareCaps( pCaps );
}

HRESULT STDMETHODCALLTYPE ResMan::GetHardwareCaps( LPA3DCAPS_HARDWARE pCaps )
{
	return m_pA3dBackend->GetHardwareCaps( pCaps );
}

DWORD ResMan::GetSoftwareStreamCount()
{
	return m_pDefaultPool->m_Streams.cStreams;
}

DWORD ResMan::GetFreeSoftwareVoices()
{
	return m_pDefaultPool->GetNumAvailable();
}

DWORD ResMan::CountPlayingSoftwareVoices()
{
	DWORD dwCount = 0;
	DalBufferInfo* pStream = NULL;
	a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pHead;
	while ( pNode )
	{
		ResManBuffer* pBuffer = pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwStatus;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwStatus );
		pBuffer->GetRenderStream( &pStream );
		if ( pStream && pStream->m_pPool == m_pDefaultPool && dwStatus == A3D_BUFFER_STATE_PLAYING )
			dwCount++;
	}

	return dwCount;
}

DWORD ResMan::CountPlayingBuffers()
{
	DWORD dwCount = 0;
	a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pHead;
	while ( pNode )
	{
		ResManBuffer* pBuffer = pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwStatus;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwStatus );
		if ( dwStatus == A3D_BUFFER_STATE_PLAYING )
			dwCount++;
	}

	return dwCount;
}

ResManStreamBuffer::ResManStreamBuffer( IUnknown* pOwner, DWORD dwBufferKind )
{
	m_pOwner = pOwner;
	m_pOwner->AddRef();
	m_cReferences = 0;
	m_pBufferToken = NULL;
	m_pStream = NULL;
	m_pRenderData = NULL;
	m_lVolume = 0;
	m_hData = NULL;
	m_lPan = 0;
	m_dwControl = 0;
	m_dwOriginalFrequency = 0;
	m_dwReadPosition = 0;
	m_dwFlags = 0;
	m_dwNotificationCount = 0;
	m_pNotifications = NULL;
	m_hNotificationMutex = NULL;
	m_dwBufferKind = dwBufferKind;
	m_dwFrequency = DSBFREQUENCY_MIN;
	m_dwOverridePosition = A3D_BUFFER_POSITION_UNSET;
	m_dwStatus = A3D_BUFFER_STATE_STOPPED;
}

ResManStreamBuffer::~ResManStreamBuffer()
{
	if ( m_hNotificationMutex )
	{
		CloseHandle( m_hNotificationMutex );
		m_hNotificationMutex = NULL;
	}

	if ( m_pOwner )
	{
		m_pOwner->Release();
		m_pOwner = NULL;
	}
}

HRESULT STDMETHODCALLTYPE ResManBuffer::QueryInterface( REFIID iid, void** ppObject )
{
	if ( !ppObject )
		return E_INVALIDARG;

	IUnknown* pObject;
	if ( !memcmp( &iid, &IID_IUnknown, sizeof( IID ) ) ||
		!memcmp( &iid, &IID_IDirectSoundBuffer, sizeof( IID ) ) )
	{
		pObject = (IDirectSoundBuffer*)this;
	}
	else if ( !memcmp( &iid, &IID_A3dRenderBuffer, sizeof( IID ) ) )
	{
		pObject = (IA3dDalBuffer*)this;
	}
	else if ( !memcmp( &iid, &IID_A3dBufferControl, sizeof( IID ) ) )
	{
		pObject = (IResManBuffer*)this;
	}
	else if ( !memcmp( &iid, &IID_IDirectSoundNotify, sizeof( IID ) ) )
	{
		pObject = (IDirectSoundBuffer*)this;
	}
	else if ( !memcmp( &iid, &IID_A3dSoftwareBufferControl, sizeof( IID ) ) )
	{
		pObject = (IResManBuffer*)this;
	}
	else
	{
		*ppObject = NULL;
		return E_NOINTERFACE;
	}

	*ppObject = pObject;
	pObject->AddRef();
	return S_OK;
}

ULONG STDMETHODCALLTYPE ResManBuffer::AddRef()
{
	return InterlockedIncrement( &m_cReferences );
}

ULONG STDMETHODCALLTYPE ResManBuffer::Release()
{
	if ( !InterlockedDecrement( &m_cReferences ) )
	{
		m_dwStatus = A3D_BUFFER_STATE_RELEASED;
		return 0;
	}

	return m_cReferences;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetCaps( LPDSBCAPS )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetFormat( LPWAVEFORMATEX, DWORD, LPDWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Initialize( LPDIRECTSOUND, LPCDSBUFFERDESC )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetFormat( LPCWAVEFORMATEX )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Unlock( LPVOID, DWORD, LPVOID, DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Restore()
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetA3dSuperCtrl( void* pData, DWORD dwFlags )
{
	if ( !m_pStream )
		return E_FAIL;

	m_pRenderData = pData;
	( (IA3dPlatformRenderBuffer*)m_pStream->GetIDalBuffer() )->SetA3dSuperCtrl( pData, dwFlags );
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetAllocationStatus( DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetWave( DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetDriverInfo( DWORD, DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetNewBuffer( DWORD, DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetA3dDirectCtrl( DWORD, DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetStatusEx( DWORD )
{
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetCurrentPosition( LPDWORD pdwReadPosition,
		LPDWORD pdwWritePosition )
{
	if ( m_dwOverridePosition != A3D_BUFFER_POSITION_UNSET )
	{
		if ( pdwReadPosition )
			*pdwReadPosition = m_dwOverridePosition;
	}
	else
	{
		DWORD dwPosition = m_dwReadPosition;
		if ( m_pStream )
		{
			DWORD dwStreamPosition;
			m_pStream->GetAudioBytesQueued( &dwStreamPosition );
			if ( dwStreamPosition > dwPosition )
				dwPosition += m_Desc.dwBufferBytes - dwStreamPosition;
			else
				dwPosition -= dwStreamPosition;
		}

		*pdwReadPosition = dwPosition;
	}

	if ( pdwWritePosition )
		*pdwWritePosition = *pdwReadPosition;

	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetVolume( LPLONG plVolume )
{
	if ( !plVolume )
		return DSERR_INVALIDPARAM;

	*plVolume = m_lVolume;
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetPan( LPLONG plPan )
{
	if ( !plPan )
		return DSERR_INVALIDPARAM;

	*plPan = m_lPan;
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetFrequency( LPDWORD pdwFrequency )
{
	*pdwFrequency = m_dwFrequency;
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::GetStatus( LPDWORD pdwStatus )
{
	*pdwStatus = 0;
	if ( m_dwStatus == A3D_BUFFER_STATE_PLAYING )
		*pdwStatus = DSBSTATUS_PLAYING;
	if ( m_dwFlags == A3D_RENDER_LOOPING )
		*pdwStatus |= DSBSTATUS_LOOPING;

	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Lock( DWORD dwOffset, DWORD dwBytes, LPVOID* ppFirst,
		LPDWORD pdwFirstBytes, LPVOID* ppSecond, LPDWORD pdwSecondBytes, DWORD dwFlags )
{
	if ( !ppFirst || !pdwFirstBytes || dwBytes > m_Desc.dwBufferBytes )
		return DSERR_INVALIDPARAM;

	if ( ( dwFlags & DSBLOCK_FROMWRITECURSOR ) == DSBLOCK_FROMWRITECURSOR )
		dwOffset = m_dwReadPosition;

	*ppFirst = (BYTE*)m_hData + dwOffset;
	if ( dwOffset + dwBytes <= m_Desc.dwBufferBytes )
	{
		*pdwFirstBytes = dwBytes;
		if ( ppSecond && pdwSecondBytes )
		{
			*ppSecond = NULL;
			*pdwSecondBytes = 0;
		}
	}
	else
	{
		*pdwFirstBytes = m_Desc.dwBufferBytes - dwOffset;
		if ( ppSecond && pdwSecondBytes )
		{
			*ppSecond = m_hData;
			*pdwSecondBytes = dwBytes - *pdwFirstBytes;
		}
	}

	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetCurrentPosition( DWORD dwPosition )
{
	if ( dwPosition > m_Desc.dwBufferBytes )
		return DSERR_INVALIDPARAM;

	m_dwOverridePosition = dwPosition;
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Play( DWORD dwReserved, DWORD dwPriority, DWORD dwFlags )
{
	if ( dwReserved || dwPriority )
		return DSERR_INVALIDPARAM;

	if ( m_dwStatus != A3D_BUFFER_STATE_PLAYING && m_dwBufferKind == 1 )
	{
		ResMan* pOwner = (ResMan*)(IDirectSound*)m_pOwner;
		DWORD dwStreams = pOwner->GetSoftwareStreamCount();
		DWORD dwPlaying = 0;
		GetTickCount();
		if ( dwStreams )
			dwPlaying = pOwner->CountPlayingSoftwareVoices();
		if ( dwPlaying < dwStreams )
		{
			dwStreams += pOwner->GetFreeSoftwareVoices();
			if ( dwPlaying > dwStreams )
				return E_FAIL;
		}
	}

	m_dwStatus = A3D_BUFFER_STATE_PLAYING;
	if ( dwFlags & DSBPLAY_LOOPING )
		m_dwFlags |= A3D_RENDER_LOOPING;
	else
		m_dwFlags &= ~A3D_RENDER_LOOPING;

	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetVolume( LONG lVolume )
{
	if ( lVolume < DSBVOLUME_MIN || lVolume > DSBVOLUME_MAX )
		return DSERR_INVALIDPARAM;

	m_lVolume = lVolume;
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetPan( LONG lPan )
{
	if ( lPan < DSBPAN_LEFT || lPan > DSBPAN_RIGHT )
		return DSERR_INVALIDPARAM;

	m_lPan = lPan;
	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::SetFrequency( DWORD dwFrequency )
{
	if ( ( dwFrequency < DSBFREQUENCY_MIN || dwFrequency > A3D_PLATFORM_MAX_FREQUENCY ) && dwFrequency )
		return DSERR_INVALIDPARAM;

	if ( dwFrequency == DSBFREQUENCY_ORIGINAL )
		m_dwFrequency = m_dwOriginalFrequency;
	else
		m_dwFrequency = dwFrequency;

	return DS_OK;
}

HRESULT STDMETHODCALLTYPE ResManBuffer::Stop()
{
	m_dwStatus = A3D_BUFFER_STATE_STOPPED;
	return DS_OK;
}

HRESULT ResManBuffer::SetStream( DalBufferInfo* pStream )
{
	m_pStream = pStream;
	return DS_OK;
}

HRESULT ResManBuffer::Duplicate( const ResManBuffer* pBuffer )
{
	if ( pBuffer )
	{
		m_Desc = pBuffer->m_Desc;
		m_pBufferToken = pBuffer->m_pBufferToken;
		m_hData = pBuffer->m_hData;
		m_dwFlags = pBuffer->m_dwFlags;
	}

	return DS_OK;
}

HRESULT ResManBuffer::Update( DWORD dwTime )
{
	if ( !m_pStream )
		return E_ABORT;

	if ( m_dwOverridePosition != A3D_BUFFER_POSITION_UNSET )
	{
		m_dwReadPosition = m_dwOverridePosition;
		m_dwOverridePosition = A3D_BUFFER_POSITION_UNSET;
	}

	DWORD dwSoundStatus;
	m_pStream->GetDSBuffer()->GetStatus( &dwSoundStatus );
	if ( !( dwSoundStatus & DSBSTATUS_PLAYING ) )
		m_pStream->Reset();

	DWORD dwAvailable;
	if ( m_pStream->Refresh( &dwAvailable ) )
	{
		DWORD dwPreviousPosition = m_dwReadPosition;
		DWORD dwPosition = m_dwReadPosition;
		m_pStream->Write( dwTime, (const BYTE*)m_hData, m_Desc.dwBufferBytes, &dwPosition, m_dwFlags );
		m_dwReadPosition = dwPosition;

		if ( m_hNotificationMutex )
		{
			WaitForSingleObject( m_hNotificationMutex, INFINITE );
			for ( DWORD i = 0; i < m_dwNotificationCount; i++ )
			{
				DWORD dwOffset = m_pNotifications[i].dwOffset;
				if ( dwOffset >= dwPreviousPosition && dwOffset < dwPosition )
					SetEvent( m_pNotifications[i].hEventNotify );
			}
			ReleaseMutex( m_hNotificationMutex );
		}

		if ( m_dwReadPosition >= m_Desc.dwBufferBytes && !( m_dwFlags & A3D_RENDER_LOOPING ) &&
			!( dwSoundStatus & DSBSTATUS_PLAYING ) )
		{
			Stop();
			SetCurrentPosition( 0 );
		}

		if ( m_dwStatus == A3D_BUFFER_STATE_PLAYING && !( dwSoundStatus & DSBSTATUS_PLAYING ) )
			m_pStream->GetDSBuffer()->Play( 0, 0, DSBPLAY_LOOPING );
	}

	return DS_OK;
}

#define A3D_RENDER_WAIT_MILLISECONDS		10
#define A3D_RENDER_UPDATE_MILLISECONDS	100

static const IID IID_A3dHardwareBufferControl =
	{ 0x2e56ef65, 0x74ce, 0x11d2, { 0x97, 0x1b, 0x00, 0x60, 0x08, 0x8d, 0x87, 0x88 } };

HRESULT ResMan::StartRenderer( DWORD dwFirst, DWORD dwSecond,
		void* pConfiguration, void* pBackend )
{
	HRESULT hr = InitResMan( dwFirst, dwSecond, pConfiguration, pBackend );
	if ( FAILED( hr ) )
		return hr;

	m_pA3dBackend->SetResourceManagerMode( A3D_RESOURCE_MODE_OFF );
	m_dwWaitTime = A3D_RENDER_WAIT_MILLISECONDS;
	m_dwUpdateInterval = A3D_RENDER_UPDATE_MILLISECONDS;
	m_hUpdateEvent = CreateEventA( NULL, FALSE, FALSE, NULL );
	if ( !m_hUpdateEvent )
		return E_FAIL;

	m_hThread = CreateThread( NULL, 0, RenderThread, this, 0, &m_dwThreadId );
	if ( !m_hThread )
	{
		CloseHandle( m_hUpdateEvent );
		m_hUpdateEvent = INVALID_HANDLE_VALUE;
		return E_FAIL;
	}

	m_dwInitialized = TRUE;
	return S_OK;
}

DWORD WINAPI ResMan::RenderThread( LPVOID pContext )
{
	ResMan* pAudio = (ResMan*)pContext;
	WaitForSingleObject( pAudio->m_hUpdateEvent, pAudio->m_dwWaitTime );
	for ( ;; )
	{
		DWORD dwTime = GetTickCount();
		if ( dwTime - pAudio->m_dwPreviousUpdate > pAudio->m_dwUpdateInterval )
		{
			pAudio->m_dwPreviousUpdate = dwTime;
			pAudio->FreeReleasedStreams( dwTime );
			pAudio->HaltStoppedStreams( dwTime );
			pAudio->EnsureStreamCapacity( dwTime );
			pAudio->BuildPendingBuffers( dwTime );
			pAudio->AssignStreams( dwTime );
			pAudio->m_pDefaultPool->Update( dwTime );
			pAudio->m_pHrtfPool->Update( dwTime );
			pAudio->m_pSecondaryPool->Update( dwTime );
			pAudio->DebugBufferCounts();
		}
		pAudio->ProcessPlayingBuffers( dwTime );
		WaitForSingleObject( pAudio->m_hUpdateEvent, pAudio->m_dwWaitTime );
	}
}

HRESULT ResMan::DetachStream( DWORD dwTime, ResManBuffer* pBuffer,
		DalBufferInfo* pStream )
{
	pStream->GetDSBuffer()->Stop();
	pStream->GetDSBuffer()->SetCurrentPosition( 0 );
	pStream->m_dwUnusedSince = dwTime;
	pStream->m_dwState = 1;
	pStream->m_pOwner = NULL;
	pBuffer->SetStream( NULL );
	return S_OK;
}

HRESULT ResMan::EnsurePoolCapacity( DWORD dwRequired, LPDWORD pdwCommitted,
		DalInfo* pPool )
{
	DWORD dwCommitted = *pdwCommitted;
	DWORD dwPoolStreams = pPool->m_Streams.cStreams;
	HRESULT hr = S_OK;
	if ( dwRequired > dwCommitted + dwPoolStreams )
	{
		DWORD dwNeeded = dwRequired - dwPoolStreams - dwCommitted;
		for ( DWORD i = 0; i < dwNeeded; i++ )
		{
			hr = pPool->CreateDalBuffer( NULL );
			if ( FAILED( hr ) )
				break;
		}
	}

	*pdwCommitted += pPool->m_Streams.cStreams;
	return hr;
}

HRESULT ResMan::CommitStreams( DWORD dwTime, DalInfo* pPool,
		LPLONG plRemaining, a3d_platform_buffer_node_t** ppNext )
{
	for ( DWORD i = 0; i < (DWORD)pPool->m_Streams.cStreams; i++ )
	{
		LONG nRemaining = (LONG)( (DWORD)*plRemaining - 1 );
		*plRemaining = nRemaining;
		if ( nRemaining < 0 )
			break;

		a3d_platform_buffer_node_t* pNode = *ppNext;
		*ppNext = pNode->pNext;
		ResManBuffer* pBuffer = pNode->pBuffer;
		DalBufferInfo* pStream;
		pBuffer->GetRenderStream( &pStream );
		if ( pStream && pStream->m_pPool == pPool )
			continue;
		if ( pStream )
			DetachStream( dwTime, pBuffer, pStream );

		pPool->FindUnused( &pStream );
		if ( !pStream )
		{
			ResManBuffer* pPreviousOwner = NULL;
			pNode = m_PendingBuffers.pTail;
			while ( pNode )
			{
				ResManBuffer* pOther = pNode->pBuffer;
				pNode = pNode->pPrevious;
				DalBufferInfo* pOtherStream;
				pOther->GetRenderStream( &pOtherStream );
				if ( pOtherStream && pOtherStream->m_pPool == pPool )
				{
					pPreviousOwner = pOther;
					pStream = pOtherStream;
				}
			}

			if ( !pPreviousOwner || !pStream )
				break;
			DetachStream( dwTime, pPreviousOwner, pStream );
		}

		if ( pStream )
		{
			pStream->m_pOwner = pBuffer;
			pStream->m_dwUnusedSince = dwTime;
			pBuffer->SetStream( pStream );
		}
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ResMan::DuplicateSoundBuffer( IDirectSoundBuffer* pBuffer,
		IDirectSoundBuffer** ppDuplicate )
{
	IUnknown* pHardwareControl = NULL;
	IUnknown* pSoftwareControl = NULL;
	pBuffer->QueryInterface( IID_A3dHardwareBufferControl, (void**)&pHardwareControl );
	pBuffer->QueryInterface( IID_A3dSoftwareBufferControl, (void**)&pSoftwareControl );

	if ( pHardwareControl )
	{
		ResManStatBuffer* pDuplicate = new ( std::nothrow ) ResManStatBuffer(
			(IDirectSound*)this, m_dwResourceManagerMode );
		if ( !pDuplicate )
		{
			pHardwareControl->Release();
			return E_OUTOFMEMORY;
		}

		DalBufferInfo* pOriginalStream;
		DalBufferInfo* pStream;
		HRESULT hr = pDuplicate->Duplicate( (ResManBuffer*)pBuffer );
		if ( FAILED( hr ) ||
			FAILED( hr = pDuplicate->QueryInterface( IID_IDirectSoundBuffer, (void**)ppDuplicate ) ) ||
			FAILED( hr = ( (ResManBuffer*)pBuffer )->GetRenderStream( &pOriginalStream ) ) ||
			FAILED( hr = m_pDefaultPool->CreateDalBuffer( pOriginalStream, &pStream ) ) )
		{
			delete pDuplicate;
			pHardwareControl->Release();
			return hr;
		}

		pDuplicate->SetStream( pStream );
		a3d_platform_buffer_node_t* pPrevious = m_HardwareBuffers.pTail;
		if ( !m_HardwareBuffers.pFreeNodes )
		{
			CPlex* pBlock = CPlex::Create(
				(CPlex**)&m_HardwareBuffers.pBlocks, m_HardwareBuffers.cBlockNodes,
				sizeof( a3d_platform_buffer_node_t ) );
			a3d_platform_buffer_node_t* pNodes = (a3d_platform_buffer_node_t*)( pBlock + 1 );
			for ( LONG i = m_HardwareBuffers.cBlockNodes - 1; i >= 0; i-- )
			{
				pNodes[i].pNext = m_HardwareBuffers.pFreeNodes;
				m_HardwareBuffers.pFreeNodes = &pNodes[i];
			}
		}

		a3d_platform_buffer_node_t* pNode = m_HardwareBuffers.pFreeNodes;
		m_HardwareBuffers.pFreeNodes = pNode->pNext;
		pNode->pPrevious = pPrevious;
		pNode->pNext = NULL;
		m_HardwareBuffers.cBuffers++;
		pNode->pBuffer = pDuplicate;
		a3d_platform_buffer_node_t* pTail = m_HardwareBuffers.pTail;
		if ( pTail )
			pTail->pNext = pNode;
		else
			m_HardwareBuffers.pHead = pNode;
		m_HardwareBuffers.pTail = pNode;
		pHardwareControl->Release();
	}

	if ( pSoftwareControl )
	{
		ResManStreamBuffer* pDuplicate = new ( std::nothrow ) ResManStreamBuffer(
			(IDirectSound*)this, m_dwResourceManagerMode );
		if ( !pDuplicate )
		{
			pSoftwareControl->Release();
			return E_OUTOFMEMORY;
		}

		HRESULT hr = pDuplicate->Duplicate( (ResManBuffer*)pBuffer );
		if ( SUCCEEDED( hr ) )
			hr = pDuplicate->QueryInterface( IID_IDirectSoundBuffer, (void**)ppDuplicate );
		if ( FAILED( hr ) )
		{
			delete pDuplicate;
			pSoftwareControl->Release();
			return hr;
		}

		a3d_platform_buffer_node_t* pPrevious = m_SoftwareBuffers.pTail;
		if ( !m_SoftwareBuffers.pFreeNodes )
		{
			CPlex* pBlock = CPlex::Create(
				(CPlex**)&m_SoftwareBuffers.pBlocks, m_SoftwareBuffers.cBlockNodes,
				sizeof( a3d_platform_buffer_node_t ) );
			a3d_platform_buffer_node_t* pNodes = (a3d_platform_buffer_node_t*)( pBlock + 1 );
			for ( LONG i = m_SoftwareBuffers.cBlockNodes - 1; i >= 0; i-- )
			{
				pNodes[i].pNext = m_SoftwareBuffers.pFreeNodes;
				m_SoftwareBuffers.pFreeNodes = &pNodes[i];
			}
		}

		a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pFreeNodes;
		m_SoftwareBuffers.pFreeNodes = pNode->pNext;
		pNode->pPrevious = pPrevious;
		pNode->pNext = NULL;
		m_SoftwareBuffers.cBuffers++;
		pNode->pBuffer = pDuplicate;
		a3d_platform_buffer_node_t* pTail = m_SoftwareBuffers.pTail;
		if ( pTail )
			pTail->pNext = pNode;
		else
			m_SoftwareBuffers.pHead = pNode;
		m_SoftwareBuffers.pTail = pNode;
		pSoftwareControl->Release();
	}

	return DS_OK;
}

HRESULT ResMan::CreateSecondaryBuffer( a3d_platform_buffer_desc_t* pDesc,
		IDirectSoundBuffer** ppBuffer, IUnknown* )
{
	if ( m_dwResourceManagerMode != A3D_RESOURCE_MODE_OFF )
	{
		void* pBufferToken = malloc( sizeof( DWORD ) );
		if ( !pBufferToken )
			return E_OUTOFMEMORY;

		ResManStreamBuffer* pBuffer = new ( std::nothrow ) ResManStreamBuffer(
			(IDirectSound*)this, m_dwResourceManagerMode );
		if ( !pBuffer )
		{
			free( pBufferToken );
			return E_OUTOFMEMORY;
		}

		HRESULT hr = pBuffer->Init( pDesc, pBufferToken );
		if ( SUCCEEDED( hr ) )
			hr = pBuffer->QueryInterface( IID_IDirectSoundBuffer, (void**)ppBuffer );
		if ( FAILED( hr ) )
		{
			delete pBuffer;
			free( pBufferToken );
			return hr;
		}

		a3d_platform_buffer_node_t* pPrevious = m_SoftwareBuffers.pTail;
		if ( !m_SoftwareBuffers.pFreeNodes )
		{
			CPlex* pBlock = CPlex::Create(
				(CPlex**)&m_SoftwareBuffers.pBlocks, m_SoftwareBuffers.cBlockNodes,
				sizeof( a3d_platform_buffer_node_t ) );
			a3d_platform_buffer_node_t* pNodes = (a3d_platform_buffer_node_t*)( pBlock + 1 );
			for ( LONG i = m_SoftwareBuffers.cBlockNodes - 1; i >= 0; i-- )
			{
				pNodes[i].pNext = m_SoftwareBuffers.pFreeNodes;
				m_SoftwareBuffers.pFreeNodes = &pNodes[i];
			}
		}

		a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pFreeNodes;
		m_SoftwareBuffers.pFreeNodes = pNode->pNext;
		pNode->pPrevious = pPrevious;
		pNode->pNext = NULL;
		m_SoftwareBuffers.cBuffers++;
		pNode->pBuffer = pBuffer;
		a3d_platform_buffer_node_t* pTail = m_SoftwareBuffers.pTail;
		if ( pTail )
			pTail->pNext = pNode;
		else
			m_SoftwareBuffers.pHead = pNode;
		m_SoftwareBuffers.pTail = pNode;
	}
	else
	{
		void* pBufferToken = malloc( sizeof( DWORD ) );
		if ( !pBufferToken )
			return E_OUTOFMEMORY;

		ResManStatBuffer* pBuffer = new ( std::nothrow ) ResManStatBuffer(
			(IDirectSound*)this, m_dwResourceManagerMode );
		if ( !pBuffer )
		{
			free( pBufferToken );
			return E_OUTOFMEMORY;
		}

		HRESULT hr = pBuffer->Init( pDesc, pBufferToken );
		if ( SUCCEEDED( hr ) )
			hr = pBuffer->QueryInterface( IID_IDirectSoundBuffer, (void**)ppBuffer );
		if ( FAILED( hr ) )
		{
			delete pBuffer;
			free( pBufferToken );
			return hr;
		}

		DalBufferInfo* pStream;
		hr = m_pDefaultPool->CreateStaticDalBuffer( pDesc, &pStream );
		if ( FAILED( hr ) )
		{
			delete pBuffer;
			free( pBufferToken );
			return hr;
		}

		pBuffer->SetStream( pStream );
		a3d_platform_buffer_node_t* pPrevious = m_HardwareBuffers.pTail;
		if ( !m_HardwareBuffers.pFreeNodes )
		{
			CPlex* pBlock = CPlex::Create(
				(CPlex**)&m_HardwareBuffers.pBlocks, m_HardwareBuffers.cBlockNodes,
				sizeof( a3d_platform_buffer_node_t ) );
			a3d_platform_buffer_node_t* pNodes = (a3d_platform_buffer_node_t*)( pBlock + 1 );
			for ( LONG i = m_HardwareBuffers.cBlockNodes - 1; i >= 0; i-- )
			{
				pNodes[i].pNext = m_HardwareBuffers.pFreeNodes;
				m_HardwareBuffers.pFreeNodes = &pNodes[i];
			}
		}

		a3d_platform_buffer_node_t* pNode = m_HardwareBuffers.pFreeNodes;
		m_HardwareBuffers.pFreeNodes = pNode->pNext;
		pNode->pPrevious = pPrevious;
		pNode->pNext = NULL;
		m_HardwareBuffers.cBuffers++;
		pNode->pBuffer = pBuffer;
		a3d_platform_buffer_node_t* pTail = m_HardwareBuffers.pTail;
		if ( pTail )
			pTail->pNext = pNode;
		else
			m_HardwareBuffers.pHead = pNode;
		m_HardwareBuffers.pTail = pNode;
	}

	return DS_OK;
}

#define A3D_PLATFORM_RENDER_BUFFER_BYTES		88200
#define A3D_PLATFORM_RENDER_SAMPLE_RATE		44100
#define A3D_PLATFORM_RENDER_SAMPLE_BITS		16
#define A3D_PLATFORM_RENDER_UPDATE_THRESHOLD	44100

static const CLSID CLSID_A3dPlatformBackend =
	{ 0x442d12a1, 0x2641, 0x11d2, { 0x90, 0xfb, 0x00, 0x60, 0x08, 0xa1, 0xf4, 0x41 } };

HRESULT ResMan::InitResMan( DWORD dwFirst, DWORD dwSecond,
		void* pConfiguration, void* pBackend )
{
	if ( m_pDefaultPool )
		return E_FAIL;
	if ( m_pHrtfPool )
		return E_FAIL;
	if ( m_pSecondaryPool )
		return E_FAIL;

	CoInitialize( NULL );
	HRESULT hr = CoCreateInstance( CLSID_A3dPlatformBackend, NULL, CLSCTX_INPROC_SERVER,
		IID_IA3d2, (void**)&m_pA3dBackend );
	if ( FAILED( hr ) )
		return hr;

	IA3dDal* pDeviceBackend;
	hr = m_pA3dBackend->QueryInterface( IID_A3dPlatformDeviceBackend, (void**)&pDeviceBackend );
	if ( FAILED( hr ) )
		return hr;
	hr = pDeviceBackend->InitializeEx( dwFirst, dwSecond, pConfiguration, pBackend );
	if ( FAILED( hr ) )
		return hr;

	a3d_render_description_t description = { 1, A3D_PLATFORM_RENDER_BUFFER_BYTES,
		A3D_PLATFORM_RENDER_SAMPLE_RATE, A3D_PLATFORM_RENDER_SAMPLE_BITS,
		A3D_PLATFORM_RENDER_UPDATE_THRESHOLD };
	m_pDefaultPool = new ( std::nothrow ) DalInfo( pDeviceBackend, &description );
	if ( !m_pDefaultPool )
		return E_OUTOFMEMORY;
	m_pDefaultPool->RefreshCapacity();

	DAL_A2D* pMixer = new ( std::nothrow ) DAL_A2D;
	if ( !pMixer )
		return E_OUTOFMEMORY;
	IDirectSound* pDirectSound = NULL;
	m_pDefaultPool->GetIDal()->GetDS( &pDirectSound );
	pMixer->Init( pDirectSound );

	description = { 1, A3D_PLATFORM_RENDER_BUFFER_BYTES, A3D_PLATFORM_RENDER_SAMPLE_RATE,
		A3D_PLATFORM_RENDER_SAMPLE_BITS, A3D_PLATFORM_RENDER_UPDATE_THRESHOLD };
	m_pHrtfPool = new ( std::nothrow ) DalInfo( (IA3dDal*)pMixer, &description );
	if ( !m_pHrtfPool )
		return E_OUTOFMEMORY;
	m_pHrtfPool->RefreshCapacity();

	DAL_EMU* pTiming = new ( std::nothrow ) DAL_EMU;
	if ( !pTiming )
		return E_OUTOFMEMORY;
	description = { 0, A3D_PLATFORM_RENDER_BUFFER_BYTES, A3D_PLATFORM_RENDER_SAMPLE_RATE,
		A3D_PLATFORM_RENDER_SAMPLE_BITS, A3D_PLATFORM_RENDER_UPDATE_THRESHOLD };
	m_pSecondaryPool = new ( std::nothrow ) DalInfo( (IA3dDal*)pTiming, &description );
	if ( !m_pSecondaryPool )
		return E_OUTOFMEMORY;
	m_pSecondaryPool->RefreshCapacity();
	return S_OK;
}

void ResMan::DebugBufferCounts()
{
	static DWORD s_dwUpdateCount;
	if ( ++s_dwUpdateCount != 20 )
		return;

	s_dwUpdateCount = 0;
	LONG cSecondaryStreams = m_pSecondaryPool->m_Streams.cStreams;
	LONG cHardwareBuffers = m_HardwareBuffers.cBuffers;
	LONG cSoftwareBuffers = m_SoftwareBuffers.cBuffers;
	LONG cHrtfStreams = m_pHrtfPool->m_Streams.cStreams;
	LONG cDefaultStreams = m_pDefaultPool->m_Streams.cStreams;
	DWORD dwPlaying = CountPlayingBuffers();
	char szCounts[256];
	sprintf( szCounts, "Buffer Counts: STRM=%d STAT=%d P=%d W95=%d A2D=%d EMU=%d\n",
		cSoftwareBuffers, cHardwareBuffers, (LONG)dwPlaying, cDefaultStreams, cHrtfStreams, cSecondaryStreams );
	OutputDebugStringA( szCounts );
}

HRESULT ResMan::FreeReleasedStreams( DWORD dwTime )
{
	DWORD dwState;
	a3d_platform_buffer_node_t* pNode = m_HardwareBuffers.pHead;
	while ( pNode != NULL )
	{
		a3d_platform_buffer_node_t* pNext = pNode->pNext;
		ResManBuffer* pBuffer = pNode->pBuffer;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState == A3D_BUFFER_STATE_RELEASED )
		{
			if ( pNode == m_HardwareBuffers.pHead )
				m_HardwareBuffers.pHead = pNode->pNext;
			else
				pNode->pPrevious->pNext = pNode->pNext;

			if ( pNode == m_HardwareBuffers.pTail )
				m_HardwareBuffers.pTail = pNode->pPrevious;
			else
				pNode->pNext->pPrevious = pNode->pPrevious;

			pNode->pNext = m_HardwareBuffers.pFreeNodes;
			m_HardwareBuffers.pFreeNodes = pNode;
			if ( --m_HardwareBuffers.cBuffers == 0 )
			{
				m_HardwareBuffers.cBuffers = 0;
				m_HardwareBuffers.pFreeNodes = NULL;
				m_HardwareBuffers.pTail = NULL;
				m_HardwareBuffers.pHead = NULL;
				CPlex* pBlocksToFree = (CPlex*)m_HardwareBuffers.pBlocks;
				if ( pBlocksToFree )
					pBlocksToFree->FreeDataChain();
				m_HardwareBuffers.pBlocks = NULL;
			}

			if ( pBuffer != NULL )
			{
				( (ResManStatBuffer*)pBuffer )->~ResManStatBuffer();
				::operator delete( pBuffer );
			}
		}

		pNode = pNext;
	}

	DalBufferInfo* pStream = NULL;
	pNode = m_SoftwareBuffers.pHead;
	while ( pNode != NULL )
	{
		a3d_platform_buffer_node_t* pNext = pNode->pNext;
		ResManBuffer* pBuffer = pNode->pBuffer;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState == A3D_BUFFER_STATE_RELEASED )
		{
			pBuffer->GetRenderStream( &pStream );
			if ( pStream != NULL )
				DetachStream( dwTime, pBuffer, pStream );

			if ( pNode == m_SoftwareBuffers.pHead )
				m_SoftwareBuffers.pHead = pNode->pNext;
			else
				pNode->pPrevious->pNext = pNode->pNext;

			if ( pNode == m_SoftwareBuffers.pTail )
				m_SoftwareBuffers.pTail = pNode->pPrevious;
			else
				pNode->pNext->pPrevious = pNode->pPrevious;

			pNode->pNext = m_SoftwareBuffers.pFreeNodes;
			m_SoftwareBuffers.pFreeNodes = pNode;
			if ( --m_SoftwareBuffers.cBuffers == 0 )
			{
				m_SoftwareBuffers.cBuffers = 0;
				m_SoftwareBuffers.pFreeNodes = NULL;
				m_SoftwareBuffers.pTail = NULL;
				m_SoftwareBuffers.pHead = NULL;
				CPlex* pBlocksToFree = (CPlex*)m_SoftwareBuffers.pBlocks;
				if ( pBlocksToFree )
					pBlocksToFree->FreeDataChain();
				m_SoftwareBuffers.pBlocks = NULL;
			}

			if ( pBuffer != NULL )
			{
				( (ResManStreamBuffer*)pBuffer )->~ResManStreamBuffer();
				::operator delete( pBuffer );
			}
		}

		pNode = pNext;
	}

	return S_OK;
}

HRESULT ResMan::HaltStoppedStreams( DWORD dwTime )
{
	a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pHead;
	while ( pNode != NULL )
	{
		ResManBuffer* pBuffer = pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwState;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState == A3D_BUFFER_STATE_STOPPED )
		{
			DalBufferInfo* pStream;
			pBuffer->GetRenderStream( &pStream );
			if ( pStream != NULL )
				DetachStream( dwTime, pBuffer, pStream );
		}
	}

	return S_OK;
}

HRESULT ResMan::BuildPendingBuffers( DWORD dwTime )
{
	(void)dwTime;

	m_PendingBuffers.cBuffers = 0;
	m_PendingBuffers.pFreeNodes = NULL;
	m_PendingBuffers.pTail = NULL;
	m_PendingBuffers.pHead = NULL;
	CPlex* pBlocksToFree = (CPlex*)m_PendingBuffers.pBlocks;
	if ( pBlocksToFree )
		pBlocksToFree->FreeDataChain();
	m_PendingBuffers.pBlocks = NULL;

	a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pHead;
	while ( pNode != NULL )
	{
		ResManBuffer* pBuffer = pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwState;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState != A3D_BUFFER_STATE_PLAYING )
			continue;

		a3d_platform_buffer_node_t* pTail = m_PendingBuffers.pTail;
		if ( m_PendingBuffers.pFreeNodes == NULL )
		{
			CPlex* pBlock = CPlex::Create(
				(CPlex**)&m_PendingBuffers.pBlocks, m_PendingBuffers.cBlockNodes,
				sizeof( a3d_platform_buffer_node_t ) );
			a3d_platform_buffer_node_t* pNodes = (a3d_platform_buffer_node_t*)( pBlock + 1 );
			for ( LONG i = (LONG)( (DWORD)m_PendingBuffers.cBlockNodes - 1 ); i >= 0; i-- )
			{
				pNodes[i].pNext = m_PendingBuffers.pFreeNodes;
				m_PendingBuffers.pFreeNodes = &pNodes[i];
			}
		}

		a3d_platform_buffer_node_t* pPending = m_PendingBuffers.pFreeNodes;
		m_PendingBuffers.pFreeNodes = pPending->pNext;
		pPending->pPrevious = pTail;
		pPending->pNext = NULL;
		m_PendingBuffers.cBuffers++;
		pPending->pBuffer = pBuffer;
		if ( m_PendingBuffers.pTail != NULL )
			m_PendingBuffers.pTail->pNext = pPending;
		else
			m_PendingBuffers.pHead = pPending;
		m_PendingBuffers.pTail = pPending;
	}

	return S_OK;
}

HRESULT ResMan::EnsureStreamCapacity( DWORD dwTime )
{
	(void)dwTime;

	DWORD dwAllocated = 0;
	DWORD dwPlaying = CountPlayingBuffers();
	EnsurePoolCapacity( dwPlaying, &dwAllocated, m_pDefaultPool );
	EnsurePoolCapacity( dwPlaying, &dwAllocated, m_pHrtfPool );
	EnsurePoolCapacity( dwPlaying, &dwAllocated, m_pSecondaryPool );
	return S_OK;
}

HRESULT ResMan::AssignStreams( DWORD dwTime )
{
	LONG cRemaining = m_PendingBuffers.cBuffers;
	a3d_platform_buffer_node_t* pNext = m_PendingBuffers.pHead;
	CommitStreams( dwTime, m_pDefaultPool, &cRemaining, &pNext );
	CommitStreams( dwTime, m_pHrtfPool, &cRemaining, &pNext );
	CommitStreams( dwTime, m_pSecondaryPool, &cRemaining, &pNext );
	return S_OK;
}

HRESULT ResMan::ProcessPlayingBuffers( DWORD dwTime )
{
	a3d_platform_buffer_node_t* pNode = m_SoftwareBuffers.pHead;
	while ( pNode != NULL )
	{
		ResManBuffer* pBuffer = pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwState;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState == A3D_BUFFER_STATE_PLAYING )
			pBuffer->Update( dwTime );
	}

	pNode = m_HardwareBuffers.pHead;
	while ( pNode != NULL )
	{
		ResManStatBuffer* pBuffer = (ResManStatBuffer*)pNode->pBuffer;
		pNode = pNode->pNext;
		DWORD dwState;
		( (IA3dPlatformBufferControl*)pBuffer )->GetBufferState( &dwState );
		if ( dwState == A3D_BUFFER_STATE_PLAYING )
			pBuffer->ProcessNotifications( dwTime );
	}

	return S_OK;
}
