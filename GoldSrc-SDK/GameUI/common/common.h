//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Revision: $
// $NoKeywords: $
//
// common.h  -- general definitions -- shared by client and server portions of host
//=============================================================================

#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include "../../engine/win32_types.h"

typedef unsigned int	COLORREF;
typedef BITMAPINFO* LPBITMAPINFO;
#endif
