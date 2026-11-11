//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "CommandCheckButton.h"

#include "GameUI_Interface.h"
#include "wrect.h"
#include "cl_dll.h"

#include <stdlib.h>
#include <string.h>

CCommandCheckButton::CCommandCheckButton( vgui2::Panel* parent, const char* panelName, const char* text,
		const char* downCommand, const char* upCommand ) : vgui2::CheckButton( parent, panelName, text )
{
	m_pszDown = downCommand ? strdup( downCommand ) : NULL;
	m_pszUp = upCommand ? strdup( upCommand ) : NULL;
}

CCommandCheckButton::~CCommandCheckButton()
{
	free( m_pszDown );
	free( m_pszUp );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CCommandCheckButton::SetSelected( bool state )
{
	vgui2::CheckButton::SetSelected( state );
	char* command = NULL;
	if ( ( IsSelected() && ( command = m_pszDown ) != NULL )
		|| ( !IsSelected() && ( command = m_pszUp ) != NULL ) )
	{
		engine->pfnClientCmd( command );
		engine->pfnClientCmd( (char*)"\n" );
	}
}
