//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#pragma once

#include <cstddef>
#include "../public/tier1/interface.h"

#define VENGINE_LAUNCHER_API_VERSION "VENGINE_LAUNCHER_API_VERSION002"

void* Sys_GetProcAddress( void* module, const char* name );

#if !defined( CLIENT_DLL )
extern "C" DLL_EXPORT void* F( void** engineInterface );
#endif
