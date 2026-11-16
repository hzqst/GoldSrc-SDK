//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

// Load PCM samples from a RIFF/WAVE file. Outputs are changed only on success.
bool	ReadWaveFile( const char* filename, char*& data, int& dataBytes, int& bitsPerSample, int& channels,
			int& samplesPerSecond );
bool	WriteWaveFile( const char* filename, const char* data, int dataBytes, int bitsPerSample, int channels,
			int samplesPerSecond );
