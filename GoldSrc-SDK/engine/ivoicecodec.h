//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Define the IVoiceCodec interface.
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "interface.h"

class IVoiceCodec : public IBaseInterface
{
protected:
	virtual ~IVoiceCodec() = default;

public:
	// Initialize the object. The uncompressed format is always 8-bit signed mono.
	virtual bool Init( int quality ) = 0;
	// Use this to delete the object.
	virtual void Release() = 0;

	virtual int Compress( const char* uncompressed, int samples, char* compressed, int maxCompressedBytes,
		bool final ) = 0;
	virtual int Decompress( const char* compressed, int compressedBytes, char* uncompressed,
		int maxUncompressedBytes ) = 0;

	// Some codecs maintain state between Compress and Decompress calls. This should clear that state.
	virtual bool ResetState() = 0;
};
