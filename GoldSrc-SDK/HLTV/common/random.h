//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Generalized 32-bit random number generator
//			Range is 0x00000000 - 0x7FFFFFFF
//
// $NoKeywords: $
//=============================================================================

#pragma once

extern "C"
{
	void		SeedRandomNumberGenerator();
	float		RandomFloat( float flLow, float flHigh );
	int			RandomLong( int lLow, int lHigh );
}
