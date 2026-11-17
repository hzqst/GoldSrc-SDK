// Standalone regression tests for the shared Q_ftol float-to-int conversion.
// Windows: cl /std:c++17 /EHsc /W4 /WX tests\q_ftol_tests.cpp /Fe:q_ftol_tests.exe
// POSIX: c++ -std=c++17 tests/q_ftol_tests.cpp -o q_ftol_tests
#include "../GoldSrc-SDK/common/q_ftol.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

#define CHECK_EQUAL( expected, actual ) do { if ( ( expected ) != ( actual ) ) { \
	std::fprintf( stderr, "FAIL line %d: %s != %s\n", __LINE__, #expected, #actual ); std::exit( 1 ); } } while ( false )

static constexpr int INTEGER_MIN = ( std::numeric_limits<int>::min )();
static constexpr int INTEGER_MAX = ( std::numeric_limits<int>::max )();
static constexpr int ANGLE_MASK = 65535;

// Volatile keeps the compiler from folding the conversions at compile time.
static int Convert( double value )
{
	volatile double input = value;
	return Q_ftol( input );
}

int main()
{
	CHECK_EQUAL( 0, Convert( 0.0 ) );
	CHECK_EQUAL( 1, Convert( 1.9 ) );
	CHECK_EQUAL( -1, Convert( -1.9 ) );
	CHECK_EQUAL( INTEGER_MAX, Convert( 2147483647.5 ) );
	CHECK_EQUAL( INTEGER_MIN, Convert( -2147483648.0 ) );
	CHECK_EQUAL( INTEGER_MIN + 1, Convert( -2147483647.5 ) );

	CHECK_EQUAL( INTEGER_MIN, Convert( 2147483648.0 ) );
	CHECK_EQUAL( INTEGER_MIN, Convert( -2147483649.0 ) );
	CHECK_EQUAL( INTEGER_MIN, Convert( 1e300 ) );
	CHECK_EQUAL( INTEGER_MIN, Convert( -1e300 ) );
	CHECK_EQUAL( INTEGER_MIN, Convert( std::numeric_limits<double>::infinity() ) );
	CHECK_EQUAL( INTEGER_MIN, Convert( -std::numeric_limits<double>::infinity() ) );
	CHECK_EQUAL( INTEGER_MIN, Convert( std::numeric_limits<double>::quiet_NaN() ) );
	CHECK_EQUAL( INTEGER_MIN, Convert( static_cast<double>( std::numeric_limits<float>::quiet_NaN() ) ) );

	// anglemod relies on the out-of-range result masking to a zero angle.
	CHECK_EQUAL( 0, Convert( std::numeric_limits<double>::quiet_NaN() ) & ANGLE_MASK );

	std::puts( "q_ftol tests passed" );
	return 0;
}
