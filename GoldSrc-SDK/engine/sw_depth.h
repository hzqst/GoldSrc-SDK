#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

using zbuf_t = std::int32_t;

constexpr double SW_DEPTH_SCALE = 2147483648.0;

// Keep integer-valued span accumulators wide until each pixel is stored/tested.
// Clamping the initial value would distort spans crossing the near boundary.
inline double D_DepthAccumulator( double zi )
{
	return std::trunc( zi * SW_DEPTH_SCALE );
}

inline zbuf_t D_ClampDepth( double depth )
{
	if ( std::isnan( depth ) )
		return 0;
	if ( depth >= (std::numeric_limits<zbuf_t>::max)() )
		return (std::numeric_limits<zbuf_t>::max)();
	if ( depth <= (std::numeric_limits<zbuf_t>::min)() )
		return (std::numeric_limits<zbuf_t>::min)();
	return static_cast<zbuf_t>( depth );
}

inline zbuf_t D_DepthFromZi( double zi )
{
	return D_ClampDepth( D_DepthAccumulator( zi ) );
}

// Fog and depth visualization retain their original unsigned 16-bit scale.
inline std::uint16_t D_LegacyDepth( zbuf_t depth )
{
	return static_cast<std::uint16_t>( static_cast<std::uint32_t>( depth ) >> 16 );
}
