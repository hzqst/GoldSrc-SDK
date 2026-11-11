//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "GameConsole.h"
#include "GameConsoleDialog.h"
#include "GameUI_Interface.h"
#include "LoadingDialog.h"
#include "Taskbar.h"

#include "wrect.h"
#include "cl_dll.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PostMessage
#undef PostMessage
#endif

CGameConsole g_GameConsole;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameConsole, IGameConsole, "GameConsole003", g_GameConsole );

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameConsole::CGameConsole()
{
	m_bInitialized = false;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameConsole::~CGameConsole()
{
#ifdef _WIN32
	m_bInitialized = false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: activates the console, makes it visible and brings it to the foreground
//-----------------------------------------------------------------------------
void CGameConsole::Activate()
{
	if ( !m_bInitialized || LoadingDialog() )
		return;

	vgui2::surface()->RestrictPaintToSinglePanel( NULL );
	m_pConsole->Activate();
}

void CGameConsole::Initialize()
{
	CGameConsoleDialog* console = new CGameConsoleDialog;
	console->MakeReadyForUse();
	m_pConsole = console;
	m_pConsole->SetParent( g_pTaskbar->GetVPanel() );

	int screenWide, screenTall;
	vgui2::surface()->GetScreenSize( screenWide, screenTall );

	int inset = m_pConsole->GetProportionalScaledValue( 40 );
	int wide = m_pConsole->GetProportionalScaledValue( 560 );
	int tall = m_pConsole->GetProportionalScaledValue( getenv( "SteamDeck" ) ? 260 : 400 );

	if ( screenWide - 2 * inset < wide )
		wide = screenWide - 2 * inset;
	if ( screenTall - 2 * inset < tall )
		tall = screenTall - 2 * inset;

	m_pConsole->SetBounds( inset, inset, wide, tall );
	m_bInitialized = true;
	gEngfuncs.pfnAddCommand( "condump", OnCmdCondump );
}

//-----------------------------------------------------------------------------
// Purpose: hides the console
//-----------------------------------------------------------------------------
void CGameConsole::Hide()
{
	if ( m_bInitialized )
		m_pConsole->Hide();
}

//-----------------------------------------------------------------------------
// Purpose: clears the console
//-----------------------------------------------------------------------------
void CGameConsole::Clear()
{
	if ( m_bInitialized )
		m_pConsole->Clear();
}

bool CGameConsole::IsConsoleVisible()
{
	return m_bInitialized && m_pConsole->IsVisible();
}

void CGameConsole::Printf( const char* format, ... )
{
	if ( !m_bInitialized )
		return;

	char message[4096];
	va_list args;
	va_start( args, format );
#ifdef _WIN32
	_vsnprintf( message, sizeof( message ), format, args );
#else
	vsnprintf( message, sizeof( message ), format, args );
#endif
	va_end( args );
	message[sizeof( message ) - 1] = 0;
	m_pConsole->Print( message );
}

void CGameConsole::DPrintf( const char* format, ... )
{
	if ( !m_bInitialized )
		return;

	char message[4096];
	va_list args;
	va_start( args, format );
#ifdef _WIN32
	_vsnprintf( message, sizeof( message ), format, args );
#else
	vsnprintf( message, sizeof( message ), format, args );
#endif
	va_end( args );
	message[sizeof( message ) - 1] = 0;
	m_pConsole->DPrint( message );
}

void CGameConsole::SetParent( int parent )
{
	if ( m_bInitialized )
		m_pConsole->SetParent( (vgui2::VPANEL)parent );
}

//-----------------------------------------------------------------------------
// Purpose: activates the console after a delay
//-----------------------------------------------------------------------------
void CGameConsole::ActivateDelayed( float time )
{
	if ( m_bInitialized )
		m_pConsole->PostMessage( m_pConsole, new KeyValues( "Activate" ), time );
}

void CGameConsole::OnCmdCondump()
{
	g_GameConsole.m_pConsole->DumpConsoleTextToFile();
}
