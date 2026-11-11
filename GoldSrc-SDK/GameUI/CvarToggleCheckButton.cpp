//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "CvarToggleCheckButton.h"

#include "GameUI_Interface.h"

#include "wrect.h"
#include "cl_dll.h"
#include "vgui/KeyValues.h"

#include <stdlib.h>
#include <string.h>

CCvarToggleCheckButton::CCvarToggleCheckButton( vgui2::Panel* parent, const char* panelName, const char* text,
		const char* cvarName )
	: vgui2::CheckButton( parent, panelName, text )
{
	m_bStartValue = false;
	if ( cvarName )
	{
		m_pszCvarName = strdup( cvarName );
		if ( m_pszCvarName )
		{
			m_bStartValue = engine->pfnGetCvarFloat( m_pszCvarName ) > 0.0f;
			SetSelected( m_bStartValue );
		}
	}
	else
	{
		m_pszCvarName = NULL;
	}

	AddActionSignalTarget( this );
}

CCvarToggleCheckButton::~CCvarToggleCheckButton()
{
	free( m_pszCvarName );
}

void CCvarToggleCheckButton::Paint()
{
	if ( m_pszCvarName && m_pszCvarName[0] )
	{
		bool state = engine->pfnGetCvarFloat( m_pszCvarName ) > 0.0f;
		if ( state != m_bStartValue )
		//if ( value != IsSelected() )
		{
			SetSelected( state );
			m_bStartValue = state;
		}
	}

	vgui2::Button::Paint();
}

void CCvarToggleCheckButton::ApplyChanges()
{
	m_bStartValue = IsSelected();
	if ( m_pszCvarName )
		engine->Cvar_SetValue( m_pszCvarName, m_bStartValue ? 1.0f : 0.0f );
}

void CCvarToggleCheckButton::Reset()
{
	if ( !m_pszCvarName )
		return;

	m_bStartValue = engine->pfnGetCvarFloat( m_pszCvarName ) > 0.0f;
	SetSelected( m_bStartValue );
}

bool CCvarToggleCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartValue;
}

void CCvarToggleCheckButton::SetSelected( bool state )
{
	vgui2::CheckButton::SetSelected( state );
/*
	// Look up current value
	bool value = state;

	engine->Cvar_SetValue( m_pszCvarName, value ? 1.0f : 0.0f );*/
}

void CCvarToggleCheckButton::OnButtonChecked()
{
	if ( IsSelected() != m_bStartValue )
		PostActionSignal( new KeyValues( "ControlModified" ) );
}
