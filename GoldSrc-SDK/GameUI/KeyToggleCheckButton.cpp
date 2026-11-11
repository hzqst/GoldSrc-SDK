//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "KeyToggleCheckButton.h"

#include "GameUI_Interface.h"
#include "igameuifuncs.h"
#include "wrect.h"
#include "cl_dll.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CKeyToggleCheckButton::CKeyToggleCheckButton( vgui2::Panel* parent, const char* panelName, const char* text,
		const char* keyName, const char* commandName ) : vgui2::CheckButton( parent, panelName, text )
{
	m_bStartValue = false;
	m_pszKeyName = keyName ? strdup( keyName ) : NULL;
	m_pszCmdName = commandName ? strdup( commandName ) : NULL;
	if ( m_pszKeyName )
	{
		gameuifuncs->IsKeyDown( m_pszKeyName, m_bStartValue );
		if ( IsSelected() != m_bStartValue )
			vgui2::CheckButton::SetSelected( m_bStartValue );
	}
}

CKeyToggleCheckButton::~CKeyToggleCheckButton()
{
	free( m_pszKeyName );
	free( m_pszCmdName );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
/*
void CKeyToggleCheckButton::SetSelected( bool state )
{
	BaseClass::SetSelected( state );

	if ( !m_pszCmdName || !m_pszCmdName[ 0 ] ) 
		return;

	if ( m_bNoCommand )
		return;

	char szCommand[ 256 ];

	sprintf( szCommand, "%c%s\n", IsSelected() ? '+' : '-',
		m_pszCmdName );

	engine->pfnClientCmd( szCommand );
}*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKeyToggleCheckButton::Reset()
{
	gameuifuncs->IsKeyDown( m_pszKeyName, m_bStartValue );
	if ( IsSelected() != m_bStartValue )
		SetSelected( m_bStartValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKeyToggleCheckButton::ApplyChanges()
{
	if ( !m_pszCmdName || !m_pszCmdName[0] )
		return;

	char command[256];
	sprintf( command, "%c%s\n", IsSelected() ? '+' : '-', m_pszCmdName );
	engine->pfnClientCmd( command );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CKeyToggleCheckButton::HasBeenModified()
{
	return m_bStartValue != IsSelected();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKeyToggleCheckButton::Paint()
{
	vgui2::Button::Paint();
	bool isDown;
	if ( m_pszKeyName && gameuifuncs->IsKeyDown( m_pszKeyName, isDown ) && m_bStartValue != isDown )
	{
		SetSelected( isDown );
		m_bStartValue = isDown;
	}
}
