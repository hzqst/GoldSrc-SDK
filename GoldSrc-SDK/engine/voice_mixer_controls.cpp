//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "voice_mixer_controls.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <malloc.h>
#include <string.h>

class CMixerControls : public IMixerControls
{
public:
	CMixerControls();
	~CMixerControls() override;
	void Release() override;
	bool GetValue_Float( Control control, float& value ) override;
	bool SetValue_Float( Control control, float value ) override;
	bool SelectMicrophoneForWaveInput() override;

private:
	struct ControlInfo
	{
		DWORD m_dwControlID;
		DWORD m_cMultipleItems;
		bool m_bFound;
	};

	void Clear();
	bool Init();
	void FindMicSelectControl( DWORD dwLineID, DWORD nControls );

	HMIXER m_hMixer;

	DWORD m_dwMicSelectControlID;
	DWORD m_dwMicSelectMultipleItems;
	DWORD m_dwMicSelectControlType;
	DWORD m_dwMicSelectIndex;

	ControlInfo m_ControlInfos[NumControls];
};

CMixerControls::CMixerControls()
{
	m_hMixer = NULL;
	m_dwMicSelectControlID = 0xFFFFFFFF;
	memset( m_ControlInfos, 0, sizeof( m_ControlInfos ) );
	Init();
}

CMixerControls::~CMixerControls()
{
	Clear();
}

void CMixerControls::Clear()
{
	if ( m_hMixer )
		mixerClose( m_hMixer );

	m_hMixer = NULL;
	memset( m_ControlInfos, 0, sizeof( m_ControlInfos ) );
}

void CMixerControls::Release()
{
	delete this;
}

bool CMixerControls::GetValue_Float( Control control, float& value )
{
	if ( (unsigned int)control >= NumControls || !m_ControlInfos[control].m_bFound )
		return false;

	DWORD dwValue = control;

	MIXERCONTROLDETAILS details;
	details.cbStruct = sizeof( details );
	details.dwControlID = m_ControlInfos[control].m_dwControlID;
	details.cChannels = 1;
	details.cMultipleItems = m_ControlInfos[control].m_cMultipleItems;
	details.cbDetails = sizeof( dwValue );
	details.paDetails = &dwValue;
	if ( mixerGetControlDetailsA( (HMIXEROBJ)m_hMixer, &details, MIXER_GETCONTROLDETAILSF_VALUE ) != MMSYSERR_NOERROR )
		return false;

	if ( control == MicVolume )
		value = (float)dwValue / 65535.0f;
	else
		value = dwValue ? 1.0f : 0.0f;

	return true;
}

bool CMixerControls::SetValue_Float( Control control, float value )
{
	if ( (unsigned int)control >= NumControls || !m_ControlInfos[control].m_bFound )
		return false;

	DWORD dwValue;
	if ( control == MicVolume )
		dwValue = (DWORD)(__int64)( value * 65535.0f );
	else
		dwValue = value != 0.0f;

	MIXERCONTROLDETAILS details;
	details.cbStruct = sizeof( details );
	details.dwControlID = m_ControlInfos[control].m_dwControlID;
	details.cChannels = 1;
	details.cMultipleItems = m_ControlInfos[control].m_cMultipleItems;
	details.cbDetails = sizeof( dwValue );
	details.paDetails = &dwValue;
	return mixerSetControlDetails( (HMIXEROBJ)m_hMixer, &details, MIXER_SETCONTROLDETAILSF_VALUE ) == MMSYSERR_NOERROR;
}

bool CMixerControls::SelectMicrophoneForWaveInput()
{
	if ( m_dwMicSelectControlID == 0xFFFFFFFF )
		return false;

	DWORD* pSelected = (DWORD*)_alloca( sizeof( DWORD ) * m_dwMicSelectMultipleItems );

	MIXERCONTROLDETAILS details;
	details.cbStruct = sizeof( details );
	details.dwControlID = m_dwMicSelectControlID;
	details.cChannels = 1;
	details.cMultipleItems = m_dwMicSelectMultipleItems;
	details.cbDetails = sizeof( DWORD );
	details.paDetails = pSelected;
	if ( mixerGetControlDetailsA( (HMIXEROBJ)m_hMixer, &details, MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE )
		 != MMSYSERR_NOERROR )
		return false;

	if ( m_dwMicSelectControlType == MIXERCONTROL_CONTROLTYPE_MUX )
		memset( pSelected, 0, sizeof( DWORD ) * m_dwMicSelectMultipleItems );

	pSelected[m_dwMicSelectIndex] = 1;

	details.dwControlID = m_dwMicSelectControlID;
	details.cMultipleItems = m_dwMicSelectMultipleItems;
	return mixerSetControlDetails(
			   (HMIXEROBJ)m_hMixer, &details, MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE )
		   == MMSYSERR_NOERROR;
}

bool CMixerControls::Init()
{
	Clear();

	if ( mixerOpen( &m_hMixer, 0, 0, 0, 0 ) != MMSYSERR_NOERROR )
	{
		Clear();
		return false;
	}

	MIXERCAPSA caps;
	if ( mixerGetDevCapsA( (UINT_PTR)m_hMixer, &caps, sizeof( caps ) ) != MMSYSERR_NOERROR )
	{
		Clear();
		return false;
	}

	for ( DWORD iDest = 0; iDest < caps.cDestinations; iDest++ )
	{
		MIXERLINEA destLine;
		destLine.cbStruct = sizeof( destLine );
		destLine.dwDestination = iDest;
		if ( mixerGetLineInfoA( (HMIXEROBJ)m_hMixer, &destLine, MIXER_GETLINEINFOF_DESTINATION ) != MMSYSERR_NOERROR )
			continue;

		if ( destLine.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN )
			FindMicSelectControl( destLine.dwLineID, destLine.cControls );

		for ( DWORD iSource = 0; iSource < destLine.cConnections; iSource++ )
		{
			MIXERLINEA sourceLine;
			sourceLine.cbStruct = sizeof( sourceLine );
			sourceLine.dwDestination = iDest;
			sourceLine.dwSource = iSource;
			if ( mixerGetLineInfoA( (HMIXEROBJ)m_hMixer, &sourceLine, MIXER_GETLINEINFOF_SOURCE ) != MMSYSERR_NOERROR )
				continue;

			MIXERCONTROLA* pControls = (MIXERCONTROLA*)_alloca( sizeof( MIXERCONTROLA ) * sourceLine.cControls );

			MIXERLINECONTROLSA lineControls;
			lineControls.cbStruct = sizeof( lineControls );
			lineControls.dwLineID = sourceLine.dwLineID;
			lineControls.cControls = sourceLine.cControls;
			lineControls.cbmxctrl = sizeof( MIXERCONTROLA );
			lineControls.pamxctrl = pControls;
			if ( mixerGetLineControlsA( (HMIXEROBJ)m_hMixer, &lineControls, MIXER_GETLINECONTROLSF_ALL )
				 != MMSYSERR_NOERROR )
				continue;

			if ( sourceLine.dwComponentType != MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE )
				continue;

			for ( DWORD iControl = 0; iControl < sourceLine.cControls; iControl++ )
			{
				const MIXERCONTROLA* pControl = &pControls[iControl];

				if ( pControl->dwControlType == MIXERCONTROL_CONTROLTYPE_ONOFF
					 && ( strstr( pControl->szShortName, "Gain" ) || strstr( pControl->szShortName, "Boost" )
						  || strstr( pControl->szShortName, "+20d" ) ) )
				{
					m_ControlInfos[MicBoost].m_bFound = true;
					m_ControlInfos[MicBoost].m_dwControlID = pControl->dwControlID;
					m_ControlInfos[MicBoost].m_cMultipleItems = pControl->cMultipleItems;
				}

				Control control = NumControls;
				if ( destLine.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_SPEAKERS
					 && pControl->dwControlType == MIXERCONTROL_CONTROLTYPE_MUTE )
					control = MicMute;
				else if ( destLine.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN
						  && pControl->dwControlType == MIXERCONTROL_CONTROLTYPE_VOLUME )
					control = MicVolume;

				if ( control != NumControls )
				{
					m_ControlInfos[control].m_bFound = true;
					m_ControlInfos[control].m_dwControlID = pControl->dwControlID;
					m_ControlInfos[control].m_cMultipleItems = pControl->cMultipleItems;
				}
			}
		}
	}

	return true;
}

void CMixerControls::FindMicSelectControl( DWORD dwLineID, DWORD nControls )
{
	m_dwMicSelectControlID = 0xFFFFFFFF;

	MIXERCONTROLA* pControls = (MIXERCONTROLA*)_alloca( sizeof( MIXERCONTROLA ) * nControls );

	MIXERLINECONTROLSA lineControls;
	lineControls.cbStruct = sizeof( lineControls );
	lineControls.dwLineID = dwLineID;
	lineControls.cControls = nControls;
	lineControls.cbmxctrl = sizeof( MIXERCONTROLA );
	lineControls.pamxctrl = pControls;
	if ( mixerGetLineControlsA( (HMIXEROBJ)m_hMixer, &lineControls, MIXER_GETLINECONTROLSF_ALL ) != MMSYSERR_NOERROR )
		return;

	for ( DWORD i = 0; i < nControls; i++ )
	{
		const MIXERCONTROLA* pControl = &pControls[i];
		if ( pControl->dwControlType != MIXERCONTROL_CONTROLTYPE_MIXER
			 && pControl->dwControlType != MIXERCONTROL_CONTROLTYPE_MUX )
			continue;

		m_dwMicSelectControlID = pControl->dwControlID;
		m_dwMicSelectControlType = pControl->dwControlType;
		m_dwMicSelectMultipleItems = pControl->cMultipleItems;
		m_dwMicSelectIndex = i;

		MIXERCONTROLDETAILS_LISTTEXTA* pItems = (MIXERCONTROLDETAILS_LISTTEXTA*)_alloca(
			sizeof( MIXERCONTROLDETAILS_LISTTEXTA ) * m_dwMicSelectMultipleItems );

		MIXERCONTROLDETAILS details;
		details.cbStruct = sizeof( details );
		details.dwControlID = m_dwMicSelectControlID;
		details.cChannels = 1;
		details.cMultipleItems = m_dwMicSelectMultipleItems;
		details.cbDetails = sizeof( MIXERCONTROLDETAILS_LISTTEXTA );
		details.paDetails = pItems;
		if ( mixerGetControlDetailsA(
				 (HMIXEROBJ)m_hMixer, &details, MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_LISTTEXT )
			 != MMSYSERR_NOERROR )
			return;

		// determine which controls the Microphone source line
		for ( DWORD iItem = 0; iItem < m_dwMicSelectMultipleItems; iItem++ )
		{
			MIXERLINEA line;
			line.cbStruct = sizeof( line );
			line.dwLineID = pItems[iItem].dwParam1;
			if ( mixerGetLineInfoA( (HMIXEROBJ)m_hMixer, &line, MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_LINEID )
					 == MMSYSERR_NOERROR
				 && line.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE )
			{
				m_dwMicSelectIndex = iItem;
				return;
			}
		}

		return;
	}
}

IMixerControls* GetMixerControls()
{
	return new CMixerControls;
}
