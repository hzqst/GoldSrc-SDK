//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Basic random number generator
//
// $NoKeywords: $
//=============================================================================

#include "HLTV/common/random.h"

#include <time.h>

#define IA					16807
#define IM					2147483647
#define IQ					127773
#define IR					2836
#define NTAB				32
#define NDIV				( 1 + ( IM - 1 ) / NTAB )

// fran1 -- return a random floating-point number on the interval [0,1)
//
#define AM					( 1.0 / IM )
#define EPS					1.2e-7
#define RNMX				( 1.0 - EPS )

#define MAX_RANDOM_RANGE	0x7FFFFFFFu

static int idum = 0;

extern "C" void SeedRandomNumberGenerator()
{
	idum = (int)-time( NULL );

	if ( 1000 < idum )
	{
		idum = -idum;
	}
	else if ( -1000 < idum )
	{
		idum -= 22261048;
	}
}

int ran1()
{
	int j;
	int k;

	static int iy = 0;
	static int iv[NTAB];

	if ( idum <= 0 || !iy )
	{
		if ( -idum < 1 )
		{
			idum = 1;
		}
		else
		{
			idum = -idum;
		}

		for ( j = NTAB + 7; j >= 0; j-- )
		{
			k = idum / IQ;
			idum = IA * ( idum - k * IQ ) - IR * k;

			if ( idum < 0 )
			{
				idum += IM;
			}

			if ( j < NTAB )
			{
				iv[j] = idum;
			}
		}

		iy = iv[0];
	}

	k = idum / IQ;
	idum = IA * ( idum - k * IQ ) - IR * k;

	if ( idum < 0 )
	{
		idum += IM;
	}

	j = iy / NDIV;
	iy = iv[j];
	iv[j] = idum;

	return iy;
}

float fran1()
{
	float temp = (float)AM * ran1();

	if ( temp > RNMX )
	{
		return (float)RNMX;
	}

	return temp;
}

extern "C" float RandomFloat( float flLow, float flHigh )
{
	if ( idum == 0 )
	{
		SeedRandomNumberGenerator();
	}

	float fl = fran1(); // float in [0,1)
	return ( fl * ( flHigh - flLow ) ) + flLow; // float in [low,high)
}

extern "C" int RandomLong( int lLow, int lHigh )
{
	if ( idum == 0 )
	{
		SeedRandomNumberGenerator();
	}

	unsigned int maxAcceptable;
	unsigned int x = lHigh - lLow + 1;
	unsigned int n;

	if ( x <= 0 || MAX_RANDOM_RANGE < x - 1 )
	{
		return lLow;
	}

	// The following maps a uniform distribution on the interval [0,MAX_RANDOM_RANGE]
	// to a smaller, client-specified range of [0,x-1] in a way that doesn't bias
	// the uniform distribution unfavorably. Even for a worst case x, the loop is
	// guaranteed to be taken no more than half the time, so for that worst case x,
	// the average number of times through the loop is 2. For cases where x is
	// much smaller than MAX_RANDOM_RANGE, the average number of times through the
	// loop is very close to 1.
	//
	maxAcceptable = MAX_RANDOM_RANGE - ( ( MAX_RANDOM_RANGE + 1 ) % x );

	do
	{
		n = ran1();
	}
	while ( n > maxAcceptable );

	return lLow + ( n % x );
}
