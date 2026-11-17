//======== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Random number generator
//
// $Workfile: $
// $NoKeywords: $
//=============================================================================

#pragma once

void SeedRandomNumberGenerator( long seed = 0 );
int ran1();
float fran1();
float RandomFloat( float low, float high );
int RandomLong( int low, int high );
