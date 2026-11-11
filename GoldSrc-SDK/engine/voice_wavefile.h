//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

// Load in a wave file. This isn't very flexible and is only guaranteed to work with files
// saved with WriteWaveFile.
bool	ReadWaveFile( const char* filename, char*& data, int& dataBytes, int& bitsPerSample, int& channels,
			int& samplesPerSecond );
bool	WriteWaveFile( const char* filename, const char* data, int dataBytes, int bitsPerSample, int channels,
			int samplesPerSecond );
