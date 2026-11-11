//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

class IMixerControls
{
protected:
	virtual ~IMixerControls() = default;

public:
	enum Control
	{
		// Microphone boost is a boolean switch that sound cards support which boosts the input signal by about +20dB.
		// If this isn't on, the mic is usually way too quiet.
		MicBoost,
		// Volume values are 0-1.
		MicVolume,
		// Mic playback muting. You usually want this set to false, otherwise the sound card echoes whatever you say into the mic.
		MicMute,

		NumControls
	};

	virtual void Release() = 0;

	virtual bool GetValue_Float( Control control, float& value ) = 0;
	virtual bool SetValue_Float( Control control, float value ) = 0;

	virtual bool SelectMicrophoneForWaveInput() = 0;
};

// Allocates a set of mixer controls.
IMixerControls*	GetMixerControls();
