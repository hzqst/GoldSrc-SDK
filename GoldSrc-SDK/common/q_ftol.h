// q_ftol.h
#pragma once

#include <limits>

// Truncating float-to-int conversion with defined behavior for every input.
// NaN, infinity and values outside the int range yield INT_MIN, the value the
// x86 cvttsd2si instruction produces. No floating-point exception is raised.
inline int Q_ftol( double f )
{
	constexpr double INTEGER_LIMIT = 2147483648.0;
	if ( f >= -INTEGER_LIMIT && f < INTEGER_LIMIT )
		return static_cast<int>( f );
	return ( std::numeric_limits<int>::min )();
}
