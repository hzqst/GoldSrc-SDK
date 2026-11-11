//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "CvarTextEntry.h"

#include "GameUI_Interface.h"

#include "wrect.h"
#include "cl_dll.h"
#include "vgui/KeyValues.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

CCvarTextEntry::CCvarTextEntry( vgui2::Panel* parent, const char* panelName, const char* cvarName )
	: vgui2::TextEntry( parent, panelName )
{
	if ( cvarName )
	{
		m_pszCvarName = strdup( cvarName );
		m_pszStartValue[0] = 0;
		if ( m_pszCvarName )
		{
			const char* value = engine->pfnGetCvarString( m_pszCvarName );
			if ( value && value[0] )
			{
				SetText( value );
				strncpy( m_pszStartValue, value, sizeof( m_pszStartValue ) );
				m_pszStartValue[sizeof( m_pszStartValue ) - 1] = 0;
			}
		}
	}
	else
	{
		m_pszCvarName = NULL;
		m_pszStartValue[0] = 0;
	}

	AddActionSignalTarget( this );
}

CCvarTextEntry::~CCvarTextEntry()
{
	if ( m_pszCvarName )
		free( m_pszCvarName );
}

void CCvarTextEntry::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::TextEntry::ApplySchemeSettings( scheme );
	if ( GetMaximumCharCount() < 0 || GetMaximumCharCount() > (int)sizeof( m_pszStartValue ) )
		SetMaximumCharCount( sizeof( m_pszStartValue ) - 1 );
}

void CCvarTextEntry::ApplyChanges( bool immediate )
{
	if ( !m_pszCvarName )
		return;

	char text[sizeof( m_pszStartValue )];
	GetText( text, sizeof( text ) );
	if ( !text[0] )
		return;

	if ( immediate )
	{
		engine->Cvar_Set( m_pszCvarName, text );
	}
	else
	{
		char command[256];
		sprintf( command, "%s \"%s\"\n", m_pszCvarName, text );
		engine->pfnClientCmd( command );
	}

	strncpy( m_pszStartValue, text, sizeof( m_pszStartValue ) );
	m_pszStartValue[sizeof( m_pszStartValue ) - 1] = 0;
}

void CCvarTextEntry::Reset()
{
	const char* value = engine->pfnGetCvarString( m_pszCvarName );
	if ( value && value[0] )
	{
		SetText( value );
		strncpy( m_pszStartValue, value, sizeof( m_pszStartValue ) );
		m_pszStartValue[sizeof( m_pszStartValue ) - 1] = 0;
	}
}

bool CCvarTextEntry::HasBeenModified()
{
	char text[sizeof( m_pszStartValue )];
	GetText( text, sizeof( text ) );
	return stricmp( text, m_pszStartValue ) != 0;
}

void CCvarTextEntry::OnTextChanged()
{
	if ( !m_pszCvarName )
		return;

	char text[sizeof( m_pszStartValue )];
	GetText( text, sizeof( text ) );
	if ( stricmp( text, m_pszStartValue ) )
		PostActionSignal( new KeyValues( "ControlModified" ) );
}
