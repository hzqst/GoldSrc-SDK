#pragma once

#include <climits>

#include "modelgen.h"
#include "../utils/sprgen/spritegn.h"

constexpr int SPRITE_PALETTE_COLORS = 256;
constexpr int SPRITE_COLOR_BYTES = 3;
constexpr int SPRITE_PALETTE_OFFSET = sizeof( dsprite_t ) + sizeof( unsigned short );

inline int SpritePaletteColorCount( const void* buffer, int length )
{
	if ( length < SPRITE_PALETTE_OFFSET )
		return 0;

	const auto* count = static_cast<const unsigned char*>( buffer ) + sizeof( dsprite_t );
	const int colors = count[0] | ( count[1] << 8 );
	if ( colors > SPRITE_PALETTE_COLORS || colors * SPRITE_COLOR_BYTES > length - SPRITE_PALETTE_OFFSET )
		return 0;
	return colors;
}

inline bool SpriteFrameCountFits( int count, int remainingBytes, int firstFrameStorage,
	int additionalFrameStorage )
{
	return count > 0 && count <= remainingBytes / (int)sizeof( dspriteframetype_t ) &&
		count - 1 <= ( INT_MAX - firstFrameStorage ) / additionalFrameStorage;
}
