//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Allows matching of initialization and shutdown function calls
//
// $NoKeywords: $
//=============================================================================
#pragma once

void	TraceInit( const char* i, const char* s, int listnum );
void	TraceShutdown( const char* s, int listnum );
