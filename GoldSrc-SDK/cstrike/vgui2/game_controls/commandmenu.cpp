//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "commandmenu.h"
#include "CreateCommandMenu.h"
#include "../../../vgui2/controls/Menu.h"
#include "vgui_controls/Controls.h"
#include "iviewport.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include <string.h>

CCommandMenu::CCommandMenu( vgui2::Panel* parent, const char* filename ) : vgui2::Panel( parent, filename )
{
	MakePopup( true, false );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
	SetProportional( true );
	SetPaintBackgroundEnabled( false );
	SetVisible( false );
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	SetBounds( 0, 0, wide, tall );
	m_iTeamNumber = gViewPortInterface->GetClientDllInterface()->TeamNumber();
	m_pMenu = NULL;
	m_flCommandMenuOpened = 0;
	m_szMap[0] = 0;
	strncpy( m_szFilename, filename, sizeof( m_szFilename ) - 1 );
	m_szFilename[sizeof( m_szFilename ) - 1] = 0;
}

CCommandMenu::~CCommandMenu()
{
	ClearCommandMenu();
}

void CCommandMenu::OnCommand( const char* command )
{
	if ( !strcmp( command, "MAPBRIEFING" ) )
		gViewPortInterface->ShowVGUIMenu( 4 );
	else if ( !strcmp( command, "CLASSDESC" ) )
		gViewPortInterface->ShowVGUIMenu( 6 );
	else if ( !strcmp( command, "SERVERINFO" ) )
		gViewPortInterface->ShowVGUIMenu( 5 );
	else
		gViewPortInterface->GetClientDllInterface()->CallEnghudClientCmd( command );
	SetMenuVisible( false );
}

void CCommandMenu::ClearCommandMenu()
{
	if ( !m_pMenu )
		return;

	m_pMenu->MarkForDeletion();
	SetMenuVisible( false );
	m_pMenu = NULL;
}

void CCommandMenu::PositionCommandMenu()
{
	if ( !m_pMenu )
		return;

	int wide, tall, screenWide, screenTall;
	m_pMenu->GetSize( wide, tall );
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	int y = ( screenTall - tall ) / 2;
	if ( y < 0 )
		y = 0;
	m_pMenu->SetPos( 0, y );
}

void CCommandMenu::UpdateCommandMenu()
{
	ClearCommandMenu();
	vgui2::Menu* menu = CreateCommandCombo()->CreateCommandMenu( m_szFilename, m_iTeamNumber, this );
	if ( !menu )
		return;

	menu->MakeReadyForUse();
	m_pMenu = menu;
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
	PositionCommandMenu();
	m_pMenu->SetKeyBoardInputEnabled( false );
	m_pMenu->SetBgColor( scheme ? GetBgColor() : Color( 0, 0, 0, 200 ) );
	SetMenuVisible( true );
}

void CCommandMenu::ShowCommandMenu()
{
	if ( IsVisible() )
		return;

	bool update = false;
	if ( m_iTeamNumber != gViewPortInterface->GetClientDllInterface()->TeamNumber() )
	{
		update = true;
		m_iTeamNumber = gViewPortInterface->GetClientDllInterface()->TeamNumber();
	}
	const char* map = gViewPortInterface->GetClientDllInterface()->GetLevelName();
	if ( map && strcmp( map, m_szMap ) )
	{
		strncpy( m_szMap, map, sizeof( m_szMap ) - 1 );
		m_szMap[sizeof( m_szMap ) - 1] = 0;
		update = true;
	}
	if ( update )
		UpdateCommandMenu();
	if ( m_pMenu )
	{
		SetMenuVisible( true );
		m_flCommandMenuOpened = gViewPortInterface->GetClientDllInterface()->HudTime();
	}
}

void CCommandMenu::HideCommandMenu()
{
	if ( m_pMenu && m_flCommandMenuOpened + 0.3 <= gViewPortInterface->GetClientDllInterface()->HudTime() )
		SetMenuVisible( false );
}

void CCommandMenu::SetMenuVisible( bool visible )
{
	SetVisible( visible );
	m_pMenu->SetVisible( visible );
}

bool CCommandMenu::SlotInput( int )
{
	return false;
}
