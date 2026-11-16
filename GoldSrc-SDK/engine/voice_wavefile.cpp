//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "voice_wavefile.h"
#include "filesystem.h"
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <new>

constexpr int WAVE_TAG_BYTES = 4;
constexpr int WAVE_FORMAT_BYTES = 16;
constexpr int WAVE_HEADER_BYTES = 44;
constexpr int WAVE_FORMAT_PCM = 1;

static bool ReadDWord( FileHandle_t fp, uint32_t& value )
{
	return FS_Read( &value, sizeof( value ), 1, fp ) == sizeof( value );
}

static bool ReadTag( FileHandle_t fp, char ( &tag )[WAVE_TAG_BYTES] )
{
	return FS_Read( tag, sizeof( tag ), 1, fp ) == sizeof( tag );
}

struct WaveFormatChunk
{
	uint16_t formatTag;
	uint16_t channels;
	uint32_t samplesPerSec;
	uint32_t avgBytesPerSec;
	uint16_t blockAlign;
	uint16_t bitsPerSample;
};
static_assert( sizeof( WaveFormatChunk ) == WAVE_FORMAT_BYTES, "PCM format chunk layout" );

static void WriteDWord( FileHandle_t fp, uint32_t val )
{
	FS_Write( &val, sizeof( val ), 1, fp );
}

static void WriteWord( FileHandle_t fp, unsigned short val )
{
	FS_Write( &val, sizeof( val ), 1, fp );
}

static bool ReadWaveChunks( FileHandle_t fp, unsigned int end,
	char*& pData, int& nDataBytes, int& wBitsPerSample, int& nChannels, int& nSamplesPerSec )
{
	bool formatRead = false;
	WaveFormatChunk format = {};
	while ( FS_Tell( fp ) <= end && end - FS_Tell( fp ) >= WAVE_TAG_BYTES + sizeof( uint32_t ) )
	{
		char tag[WAVE_TAG_BYTES];
		uint32_t bytes;
		if ( !ReadTag( fp, tag ) || !ReadDWord( fp, bytes ) )
			return false;
		unsigned int start = FS_Tell( fp );
		unsigned int remaining = end - start;
		if ( bytes > remaining )
			return false;
		if ( memcmp( tag, "fmt ", WAVE_TAG_BYTES ) == 0 )
		{
			if ( bytes < sizeof( format ) || FS_Read( &format, sizeof( format ), 1, fp ) != sizeof( format ) )
				return false;
			formatRead = true;
		}
		else if ( memcmp( tag, "data", WAVE_TAG_BYTES ) == 0 )
		{
			if ( !formatRead || format.formatTag != WAVE_FORMAT_PCM || !format.channels
				 || !format.samplesPerSec || format.samplesPerSec > INT_MAX
				 || !format.bitsPerSample || format.bitsPerSample % 8 != 0 || !bytes )
				return false;
			char* buffer = new ( std::nothrow ) char[bytes];
			if ( !buffer )
				return false;
			if ( FS_Read( buffer, (int)bytes, 1, fp ) != (int)bytes )
			{
				delete[] buffer;
				return false;
			}
			pData = buffer;
			nDataBytes = (int)bytes;
			wBitsPerSample = format.bitsPerSample;
			nChannels = format.channels;
			nSamplesPerSec = (int)format.samplesPerSec;
			return true;
		}
		// RIFF chunks have an even size; the final chunk can omit its pad byte.
		if ( ( bytes & 1 ) && bytes < remaining )
			++bytes;
		FS_Seek( fp, (int)( start + bytes ), FILESYSTEM_SEEK_HEAD );
	}
	return false;
}

bool ReadWaveFile(
	const char* pFilename, char*& pData, int& nDataBytes, int& wBitsPerSample, int& nChannels, int& nSamplesPerSec )
{
	FileHandle_t fp = FS_Open( pFilename, "rb" );
	if ( !fp )
		return false;
	unsigned int fileBytes = FS_Size( fp );
	bool read = false;
	if ( fileBytes >= WAVE_HEADER_BYTES && fileBytes <= INT_MAX )
	{
		char riff[WAVE_TAG_BYTES], wave[WAVE_TAG_BYTES];
		uint32_t riffBytes;
		if ( ReadTag( fp, riff ) && ReadDWord( fp, riffBytes ) && ReadTag( fp, wave )
			 && memcmp( riff, "RIFF", WAVE_TAG_BYTES ) == 0 && memcmp( wave, "WAVE", WAVE_TAG_BYTES ) == 0
			 && riffBytes >= WAVE_TAG_BYTES && riffBytes <= fileBytes - 2 * WAVE_TAG_BYTES )
		{
			read = ReadWaveChunks( fp, riffBytes + 2 * WAVE_TAG_BYTES,
				pData, nDataBytes, wBitsPerSample, nChannels, nSamplesPerSec );
		}
	}
	FS_Close( fp );
	return read;
}

bool WriteWaveFile(
	const char* pFilename, const char* pData, int nDataBytes, int wBitsPerSample, int nChannels, int nSamplesPerSec )
{
	FileHandle_t fp = FS_Open( pFilename, "wb" );
	if ( !fp )
		return false;

	// Write the RIFF chunk.
	FS_Write( "RIFF", 4, 1, fp );
	WriteDWord( fp, 0 );
	FS_Write( "WAVE", 4, 1, fp );

	// Write the FORMAT chunk.
	FS_Write( "fmt ", 4, 1, fp );
	WriteDWord( fp, 0x10 );
	WriteWord( fp, 1 ); // WAVE_FORMAT_PCM
	WriteWord( fp, (unsigned short)nChannels );
	WriteDWord( fp, nSamplesPerSec );
	WriteDWord( fp, ( wBitsPerSample / 8 ) * nChannels * nSamplesPerSec );
	WriteWord( fp, (unsigned short)( ( wBitsPerSample / 8 ) * nChannels ) );
	WriteWord( fp, (unsigned short)wBitsPerSample );

	// Write the DATA chunk.
	FS_Write( "data", 4, 1, fp );
	WriteDWord( fp, nDataBytes );
	FS_Write( pData, nDataBytes, 1, fp );

	// Go back and write the length of the riff file.
	uint32_t dwVal = FS_Tell( fp ) - 8;
	FS_Seek( fp, 4, FILESYSTEM_SEEK_HEAD );
	WriteDWord( fp, dwVal );

	FS_Close( fp );
	return true;
}
