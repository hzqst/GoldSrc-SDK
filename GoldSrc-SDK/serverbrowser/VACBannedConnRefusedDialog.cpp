//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "VACBannedConnRefusedDialog.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVACBannedConnRefusedDialog::CVACBannedConnRefusedDialog( vgui2::VPANEL hVParent, const char *name )
	: BaseClass( NULL, name, true, false )
{
	SetParent( hVParent );
	SetSize( 480, 220 );
	SetSizeable( false );
	LoadControlSettings( "servers/VACBannedConnRefusedDialog.res", NULL );
	MoveToCenterOfScreen();
}
