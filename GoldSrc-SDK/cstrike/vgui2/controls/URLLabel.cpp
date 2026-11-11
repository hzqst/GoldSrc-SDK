//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "URLLabel.h"
#include "controls.h"
#include "vgui/Cursor.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"
#include <cstring>
#include <cstdio>

namespace vgui2
{
URLLabel::URLLabel( Panel* parent, const char* panelName, const char* text, const char* pszURL )
	: Label( parent, panelName, text )
{
	m_pszURL = NULL;
	m_bUnderline = false;
	m_iURLSize = 0;

	if ( pszURL && pszURL[0] )
		SetURL( pszURL );
}

URLLabel::URLLabel( Panel* parent, const char* panelName, const wchar_t* wszText, const char* pszURL )
	: Label( parent, panelName, wszText )
{
	m_pszURL = NULL;
	m_bUnderline = false;
	m_iURLSize = 0;

	if ( pszURL && pszURL[0] )
		SetURL( pszURL );
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
URLLabel::~URLLabel()
{
	if ( m_pszURL )
		delete[] m_pszURL;
}

//-----------------------------------------------------------------------------
// Purpose: sets the URL
//-----------------------------------------------------------------------------
void URLLabel::SetURL( const char* pszURL )
{
	int iNewURLSize = (int)strlen( pszURL );
	if ( iNewURLSize > m_iURLSize || !m_pszURL )
	{
		if ( m_pszURL )
			delete[] m_pszURL;
		m_pszURL = new char[iNewURLSize + 1];
	}

	strcpy( m_pszURL, pszURL );
	m_iURLSize = iNewURLSize;
}

void URLLabel::OnMousePressed( MouseCode code )
{
	if ( code == MOUSE_LEFT && m_pszURL )
		system()->ShellExecute( "open", m_pszURL );
}

//-----------------------------------------------------------------------------
// Purpose: scheme settings
//-----------------------------------------------------------------------------
void URLLabel::ApplySchemeSettings( IScheme* pScheme )
{
	bool hdProportional = IsHDProportional();
	bool proportional = IsProportional();
	SetFont( pScheme->GetFontEx( "DefaultUnderline", proportional, hdProportional ) );

	Label::ApplySchemeSettings( pScheme );

	SetCursor( dc_hand );
}

//-----------------------------------------------------------------------------
// Purpose: Applies resouce settings
//-----------------------------------------------------------------------------
void URLLabel::ApplySettings( KeyValues* inResourceData )
{
	Label::ApplySettings( inResourceData );

	const char* pszURL = inResourceData->GetString( "URLText", NULL );
	if ( !pszURL )
		return;

	if ( pszURL[0] != '#' )
	{
		SetURL( pszURL );
		return;
	}

	const wchar_t* ws = localize()->Find( pszURL + 1 );
	if ( !ws )
		return;

	char localizedUrl[512];
	localize()->ConvertUnicodeToANSI( ws, localizedUrl, sizeof( localizedUrl ) );
	SetURL( localizedUrl );
}

//-----------------------------------------------------------------------------
// Purpose: saves them to disk
//-----------------------------------------------------------------------------
void URLLabel::GetSettings( KeyValues* outResourceData )
{
	Label::GetSettings( outResourceData );

	if ( m_pszURL )
		outResourceData->SetString( "URLText", m_pszURL );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a description of the label string
//-----------------------------------------------------------------------------
const char* URLLabel::GetDescription()
{
	static char buf[1024];
	_snprintf( buf, sizeof( buf ), "%s, string URLText", Label::GetDescription() );
	return buf;
}
}
