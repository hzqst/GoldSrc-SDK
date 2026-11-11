//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "const.h"
#include "ivoicetweak.h"

#define VOICE_NUM_CHANNELS 5

extern IVoiceTweak g_VoiceTweakAPI;

qboolean	Voice_Init( const char* codec, int quality );
void		Voice_RegisterCvars();

qboolean	Voice_RecordStart( const char* uncompressedFile, const char* decompressedFile,
				//! If this is non-null, the voice manager will use this file for input instead of the mic.
				const char* microphoneInputFile );
qboolean	Voice_RecordStop();
qboolean	Voice_IsRecording();
qboolean	Voice_GetLoopback();
void		Voice_LocalPlayerTalkingAck();

//! Call this to reserve a voice channel for the specified entity to talk into.
//! \return A channel index for use with Voice_AddIncomingData or VOICE_CHANNEL_ERROR on error.
int			Voice_AssignChannel( int entity );
int			Voice_GetChannel( int entity );
int			Voice_AddIncomingData( int channel, const char* data, int bytes, int sequence );
void		Voice_NotifyFreeChannel( int channel );
