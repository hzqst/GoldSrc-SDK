//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include <stdint.h>

typedef uint32_t CRC32_t;

extern const CRC32_t pulCRCTable[256];

void CRC32_Init( CRC32_t* crc );
void CRC32_Final( CRC32_t* crc );
void CRC32_ProcessByte( CRC32_t* crc, unsigned char value );
void CRC32_ProcessBuffer( CRC32_t* crc, void* buffer, int length );
