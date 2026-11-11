//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "MDIClientWnd.h"

BEGIN_MESSAGE_MAP( CMDIClientWnd, CWnd )
ON_WM_PAINT()
END_MESSAGE_MAP()

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CMDIClientWnd::CMDIClientWnd()
{
}

CMDIClientWnd::~CMDIClientWnd()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMDIClientWnd::OnPaint()
{
	CPaintDC dc( this );
}
