//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Basic header for using vgui
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_H
#define VGUI_H

#ifdef _WIN32
#pragma once
#endif

namespace vgui2
{

typedef unsigned int VPANEL;

enum
{
	SIDE_LEFT	= 0,
	SIDE_TOP,
	SIDE_RIGHT,
	SIDE_BOTTOM
};

}

#endif // VGUI_H
