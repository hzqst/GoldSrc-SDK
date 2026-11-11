//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

// This is the voice recording interface. It provides 16-bit signed mono data from 
// a mic at some sample rate.
class IVoiceRecord
{
protected:
	virtual ~IVoiceRecord() = default;

public:
	// Use this to delete the object.
	virtual void Release() = 0;

	// Start/stop capturing.
	virtual bool RecordStart() = 0;
	virtual void RecordStop() = 0;

	// Idle processing.
	virtual void Idle() = 0;

	virtual int GetRecordedData( short* samples, int maxSamples ) = 0;
};

IVoiceRecord*	CreateVoiceRecord_WaveIn( int sampleRate );

IVoiceRecord*	CreateVoiceRecord_DSound( int sampleRate );
