//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Basic random number generator
//
// $NoKeywords: $
//=============================================================================

#include "Random.h"

#include <time.h>

#define IA 16807
#define IM 2147483647
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV ( 1 + ( IM - 1 ) / NTAB )

// fran1 -- return a random floating-point number on the interval [0,1)
//
#define AM ( 1.0 / IM )
#define EPS 1.2e-7
#define RNMX ( 1.0 - EPS )
#define MAX_RANDOM_RANGE 0x7FFFFFFFu

static long idum;

void SeedRandomNumberGenerator( long seed )
{
	idum = seed ? seed : -(long)time( NULL );
	if ( idum > 1000 )
		idum = -idum;
	else if ( idum > -1000 )
		idum -= 22261048;
}

int ran1()
{
	static long iy;
	static long iv[NTAB];
	long quotient;
	int index;
	if ( idum <= 0 || !iy )
	{
		if ( -idum < 1 )
			idum = 1;
		else
			idum = -idum;
		for ( index = NTAB + 7; index >= 0; --index )
		{
			quotient = idum / IQ;
			idum = IA * ( idum - quotient * IQ ) - IR * quotient;
			if ( idum < 0 )
				idum += IM;
			if ( index < NTAB )
				iv[index] = idum;
		}
		iy = iv[0];
	}
	quotient = idum / IQ;
	idum = IA * ( idum - quotient * IQ ) - IR * quotient;
	if ( idum < 0 )
		idum += IM;
	index = iy / NDIV;
	iy = iv[index];
	iv[index] = idum;
	return iy;
}

float fran1()
{
	float value = (float)AM * ran1();
	if ( value > RNMX )
		return (float)RNMX;
	return value;
}

float RandomFloat( float low, float high )
{
	if ( !idum )
		SeedRandomNumberGenerator();
	return fran1() * ( high - low ) + low;
}

int RandomLong( int low, int high )
{
	if ( !idum )
		SeedRandomNumberGenerator();
	unsigned int range = high - low + 1;
	if ( !range || range - 1 > MAX_RANDOM_RANGE )
		return low;
	unsigned int maximum = MAX_RANDOM_RANGE - ( ( MAX_RANDOM_RANGE + 1 ) % range );
	unsigned int value;
	do
	{
		value = ran1();
	}
	while ( value > maximum );
	return low + value % range;
}
