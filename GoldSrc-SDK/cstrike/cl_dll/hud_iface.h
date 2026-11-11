//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef HUD_IFACEH
#define HUD_IFACEH

#include "wrect.h"
#include "cdll_int.h"

typedef int ( *pfnUserMsgHook )( const char* name, int size, void* buffer );
extern cl_enginefunc_t gEngfuncs;

#endif
