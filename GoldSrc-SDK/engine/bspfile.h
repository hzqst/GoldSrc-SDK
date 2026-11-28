#pragma once

#include "const.h"
#include "mathlib.h"
#include "../utils/common/bspfile.h"

#include <cstddef>
#include <cstring>

#define Q1BSP_VERSION 29

// Some BSPs append a marker whose first word is not a bounding coordinate.
inline bool Mod_IsTrailingBSPModelMarker( const dmodel_t& model, int index, int count )
{
	if ( index == 0 || index != count - 1 )
		return false;

	constexpr size_t originZ = offsetof( dmodel_t, origin ) + 2 * sizeof( float );
	constexpr size_t afterTag = originZ + sizeof( float );
	constexpr unsigned char tag[] = { 0x00, 0x3c, 0x1c, 0x46 }; // 9999.0f, little endian
	constexpr unsigned char zeros[sizeof( dmodel_t )] = {};
	const auto* bytes = reinterpret_cast<const unsigned char*>( &model );
	return std::memcmp( bytes + originZ, tag, sizeof( tag ) ) == 0 &&
		std::memcmp( bytes + sizeof( float ), zeros, originZ - sizeof( float ) ) == 0 &&
		std::memcmp( bytes + afterTag, zeros, sizeof( dmodel_t ) - afterTag ) == 0;
}
