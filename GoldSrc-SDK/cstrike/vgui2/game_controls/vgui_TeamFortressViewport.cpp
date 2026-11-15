//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Client DLL VGUI2 Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

#include "vgui_TeamFortressViewport.h"
#include "ClientScoreBoardDialog.h"
#include "SpectatorGUI.h"
#include "clientmotd.h"
#include "teammenu.h"
#include "classmenu.h"
#include "commandmenu.h"
#include "vguitextwindow.h"
#include "TutorWindowManager.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "igameuifuncs.h"
#include "keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\game_controls\\vgui_TeamFortressViewport.cpp" )

IViewPort* gViewPortInterface;
IViewPortMsgs* gViewPortMsgs;
IGameUIFuncs* gameuifuncs;
vgui2::Panel* g_lastPanel;
extern void DuckMessage( const char* str );

void HudMessage( char* text )
{
	gViewPortInterface->GetClientDllInterface()->MessageHud( NULL, (int)strlen( text ) + 1, text );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if a particular class is valid for this map
//-----------------------------------------------------------------------------
int GetValidClasses( int playerClass )
{
	return gViewPortInterface->GetValidClasses( playerClass );
}

TeamFortressViewport::CBackGroundPanel::CBackGroundPanel( vgui2::Panel* parent ) :
	vgui2::Frame( parent, "ViewPortBackGround", true, false )
{
	SetScheme( "ClientScheme" );
	SetTitleBarVisible( false );
	SetMoveable( false );
	SetSizeable( false );
	SetProportional( true );
}

void TeamFortressViewport::CBackGroundPanel::OnMousePressed( vgui2::MouseCode )
{
}

vgui2::VPANEL TeamFortressViewport::CBackGroundPanel::IsWithinTraverse( int, int, bool )
{
	return 0;
}

void TeamFortressViewport::CBackGroundPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	SetBgColor( scheme->GetColor( "ViewportBG", Color( 0, 0, 0, 0 ) ) );
}

void TeamFortressViewport::CBackGroundPanel::PerformLayout()
{
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	SetBounds( 0, 0, wide, tall );
	vgui2::Frame::PerformLayout();
}

TeamFortressViewport::CSpecHelpWindow::CSpecHelpWindow( vgui2::Panel* parent, ISpectatorInterface* spectator ) :
	CVGUITextWindow( parent ), m_SpecGUI( spectator )
{
}

void TeamFortressViewport::CSpecHelpWindow::SetSpectatorInterface( ISpectatorInterface* spectator )
{
	m_SpecGUI = spectator;
}

void TeamFortressViewport::CSpecHelpWindow::OnClose()
{
	m_SpecGUI->Activate();
	gViewPortInterface->HideBackGround();
	vgui2::Frame::OnClose();
}

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
TeamFortressViewport::TeamFortressViewport() : m_bInitialized( false ), m_iNumTeams( 0 ),
	m_pClientDllInterface( NULL )
{
	gViewPortInterface = this;
	gViewPortMsgs = this;
	m_szServerName[0] = 0;
}

TeamFortressViewport::~TeamFortressViewport()
{
	m_bInitialized = false;
	for ( int i = 0; i < 5; i++ )
	{
		if ( m_sTeamNames[i] )
			free( m_sTeamNames[i] );
	}
}

void TeamFortressViewport::Initialize( CreateInterfaceFn* factories, int )
{
	m_FactoryList[0] = Sys_GetFactoryThis();
	m_FactoryList[1] = factories[0];
	m_FactoryList[2] = factories[1];
	m_FactoryList[3] = factories[2];
	m_FactoryList[4] = factories[3];
	vgui2::VGuiControls_Init( "ClientUI", m_FactoryList, 5 );
	vgui2::scheme()->LoadSchemeFromFile( "Resource/ClientScheme.res", "ClientScheme" );
	vgui2::scheme()->LoadSchemeFromFile( "Resource/TutorScheme.res", "TutorScheme" );
	gameuifuncs = (IGameUIFuncs*)factories[0]( "VENGINE_GAMEUIFUNCS_VERSION001", NULL );
}

void TeamFortressViewport::Start()
{
	m_pBackGround = new CBackGroundPanel( NULL );
	m_pBackGround->SetZPos( -20 ); // send it to the back 
	m_pBackGround->SetVisible( false );
	m_pPrivateClientScoreBoard = new CClientScoreBoardDialog( NULL );
	m_pPrivateClientScoreBoard->MakeReadyForUse();
	m_pClientScoreBoard = m_pPrivateClientScoreBoard;
	m_pMOTD = new CClientMOTD( NULL );
	m_pPrivateSpectatorGUI = new CSpectatorGUI( NULL );
	m_pSpectatorGUI = m_pPrivateSpectatorGUI;
	m_pTeamMenu = new CTeamMenu( NULL );
	m_pClassMenu = new CClassMenu( NULL, true );
	m_pSpecHelp = new CSpecHelpWindow( NULL, m_pSpectatorGUI );
	m_pClassHelp = new CVGUITextWindow( NULL );
	m_pMapBriefing = new CVGUITextWindow( NULL );
	m_pTutorWindowManager = new TutorWindowManager;
	m_iCurrentTeamNumber = 0;
	m_pCommandMenu = new CCommandMenu( NULL, "commandmenu.txt" );
	HideAllVGUIMenu();
	m_pClientScoreBoard->Reset();
	m_iGotAllMOTD = 1;
	m_iRandomPC = 0;
	memset( m_iValidClasses, 0, sizeof( m_iValidClasses ) );
	m_flScoreBoardLastUpdated = 0;
	m_sMapName[0] = 0;
	m_szServerName[0] = 0;
	memset( m_sTeamNames, 0, sizeof( m_sTeamNames ) );
	m_bInitialized = true;
}

void TeamFortressViewport::Shutdown()
{
}

void TeamFortressViewport::SetParent( vgui2::VPANEL parent )
{
	m_pBackGround->SetParent( parent );
	m_pClientScoreBoard->SetParent( parent );
	m_pMOTD->SetParent( parent );
	m_pSpectatorGUI->SetParent( parent );
	m_pTeamMenu->SetParent( parent );
	m_pClassMenu->SetParent( parent );
	m_pSpecHelp->SetParent( parent );
	m_pClassHelp->SetParent( parent );
	m_pMapBriefing->SetParent( parent );
	m_pCommandMenu->SetParent( parent );
	m_pClientScoreBoard->SetMouseInputEnabled( false ); // the SetParent() call resets this	
}

bool TeamFortressViewport::UseVGUI1()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: called when the engine shows the base client VGUI panel (i.e when entering a new level or exiting GameUI )
//-----------------------------------------------------------------------------
void TeamFortressViewport::ActivateClientUI()
{
}

//-----------------------------------------------------------------------------
// Purpose: called when the engine hides the base client VGUI panel (i.e when the GameUI is comming up ) 
//-----------------------------------------------------------------------------
void TeamFortressViewport::HideClientUI()
{
}

void TeamFortressViewport::SetNumberOfTeams( int teams )
{
	m_iNumTeams = teams;
}

//-----------------------------------------------------------------------------
// Purpose: handle showing and hiding of the command menu
//-----------------------------------------------------------------------------
VGuiLibraryInterface_t* TeamFortressViewport::GetClientDllInterface()
{
	return m_pClientDllInterface;
}

void TeamFortressViewport::SetClientDllInterface( VGuiLibraryInterface_t* callbacks )
{
	m_pClientDllInterface = callbacks;
	ASSERT_AT( m_pClientDllInterface, 924 );
}

int TeamFortressViewport::GetAllowSpectators()
{
	return m_iAllowSpectators;
}

int TeamFortressViewport::GetValidClasses( int team )
{
	return m_iValidClasses[team];
}

int TeamFortressViewport::GetNumberOfTeams()
{
	return m_iNumTeams;
}

bool TeamFortressViewport::GetIsFeigning()
{
	return m_bIsFeigning;
}

int TeamFortressViewport::GetIsSettingDetpack()
{
	return m_iIsSettingDetpack;
}

int TeamFortressViewport::GetBuildState()
{
	return m_iBuildState;
}

int TeamFortressViewport::IsRandomPC()
{
	return m_iRandomPC;
}

char* TeamFortressViewport::GetTeamName( int team )
{
	return m_sTeamNames[team];
}

const char* TeamFortressViewport::GetServerName()
{
	return m_szServerName;
}

int TeamFortressViewport::GetViewPortScheme()
{
	return m_pBackGround->GetScheme();
}

vgui2::VPANEL TeamFortressViewport::GetViewPortPanel()
{
	return m_pBackGround->GetVPanel();
}

void TeamFortressViewport::HideBackGround()
{
	m_pBackGround->SetVisible( false );
}

int TeamFortressViewport::GetSpectatorBottomBarHeight()
{
	return m_pSpectatorGUI->GetBottomBarHeight();
}

int TeamFortressViewport::GetSpectatorTopBarHeight()
{
	return m_pSpectatorGUI->GetTopBarHeight();
}

bool TeamFortressViewport::IsAnyVGUIMenuActive()
{
	return m_pBackGround->IsVisible();
}

VGuiLibraryTeamInfo_t TeamFortressViewport::GetPlayerTeamInfo( int player )
{
	return m_pClientScoreBoard->GetPlayerTeamInfo( player );
}

int TeamFortressViewport::GetCurrentMenuID()
{
	if ( !m_PendingDialogs.m_heap.Count() )
		return -1;
	m_PendingDialogs.m_current = m_PendingDialogs.m_heap[0];
	return m_PendingDialogs.m_current;
}

const char* TeamFortressViewport::GetMapName()
{
	if ( !m_sMapName[0] )
		m_pClientDllInterface->COM_FileBase( (char*)m_pClientDllInterface->GetLevelName(), m_sMapName );
	return m_sMapName;
}

void TeamFortressViewport::MakeSafeName( const char* oldName, char* newName, int capacity )
{
	if ( !oldName || !newName || capacity <= 0 )
		return;
	int count = 0;
	for ( const char* name = oldName; *name && count < capacity - 1; name++ )
	{
		if ( name == oldName && *name == '#' )
			newName[count++] = '*';
		else if ( *name == '&' )
		{
			if ( count >= capacity - 2 )
				break;
			newName[count] = '&';
			newName[count + 1] = '&';
			count += 2;
		}
		else
			newName[count++] = *name;
	}
	newName[count] = 0;
}

void TeamFortressViewport::GetAllPlayersInfo()
{
}


//-----------------------------------------------------------------------------
// Purpose: updates the scoreboard when something changes
//-----------------------------------------------------------------------------
void TeamFortressViewport::UpdateScoreBoard()
{
	GetAllPlayersInfo();
	m_pClientScoreBoard->RebuildTeams( m_szServerName, m_pClientDllInterface->TeamPlay() != 0,
		m_pSpectatorGUI->IsVisible() );
}

bool TeamFortressViewport::CanShowScoreBoard()
{
	return m_pClientDllInterface->GetMaxPlayers() > 1;
}

void TeamFortressViewport::ShowScoreBoard()
{
	if ( !CanShowScoreBoard() )
		return;
	GetAllPlayersInfo();
	if ( m_bInitialized )
	{
		m_pClientScoreBoard->Activate( false );
		m_pClientScoreBoard->MoveToFront();
	}
}

bool TeamFortressViewport::IsScoreBoardVisible()
{
	return m_bInitialized && m_pClientScoreBoard->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: Hide the scoreboard
//-----------------------------------------------------------------------------
void TeamFortressViewport::HideScoreBoard()
{
	// Prevent removal of scoreboard during intermission
	if ( m_pClientDllInterface->InIntermission() || !m_bInitialized )
		return;
	m_pClientScoreBoard->SetVisible( false );
	if ( m_PendingDialogs.m_heap.Count() > 0 )
	{
		m_PendingDialogs.m_current = m_PendingDialogs.m_heap[0];
		m_pBackGround->SetVisible( true );
		if ( m_pSpectatorGUI->IsGUIVisible() )
			HideSpectatorGUI();
		DisplayVGUIMenu( m_PendingDialogs.m_current );
	}
	m_pClientDllInterface->GameVoice_StopSquelchMode();
}

//-----------------------------------------------------------------------------
// Purpose: passes death msgs to the scoreboard to display specially
//-----------------------------------------------------------------------------
void TeamFortressViewport::DeathMsg( int killer, int victim )
{
	m_pClientScoreBoard->DeathMsg( killer, victim );
	if ( victim == m_pClientDllInterface->GetLocalPlayerIndex() )
		UpdateSpectatorPanel();
}

int TeamFortressViewport::KeyInput( int down, int keynum, const char* )
{
	if ( m_iUser1 && down && ( keynum == K_KP_ENTER || keynum == K_ENTER ) )
		ShowVGUIMenu( MENU_TEAM );
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Activate's the player special ability
//			called when the player hits their "special" key
//-----------------------------------------------------------------------------
void TeamFortressViewport::InputPlayerSpecial()
{
	if ( m_bInitialized )
		m_pClientDllInterface->CallEnghudClientCmd( "_special" );
}

void TeamFortressViewport::ChatInputPosition( int*, int* y )
{
	if ( !m_pClientDllInterface->SpectatorMode() && !m_pClientDllInterface->IsSpectateOnly() )
		return;
	if ( m_pClientDllInterface->PipInsetOff() )
		*y = m_pSpectatorGUI->GetTopBarHeight() + 5;
	else
	{
		int x, top, dummy;
		m_pClientDllInterface->InsetValues( x, top, dummy, dummy );
		*y = top + 5;
	}
}

void TeamFortressViewport::SpectatorGUIEnableInsetView( int enabled )
{
	m_pSpectatorGUI->EnableInsetView( enabled );
}

void TeamFortressViewport::SetSpectatorBanner( const char* image )
{
	m_pSpectatorGUI->SetImage( image );
}

bool TeamFortressViewport::IsSpectatorGUIVisible()
{
	return m_bInitialized && m_pSpectatorGUI->IsGUIVisible();
}

bool TeamFortressViewport::IsSpectatorBarVisible()
{
	return m_bInitialized && m_pSpectatorGUI->IsActiveGUIVisible();
}

void TeamFortressViewport::ShowSpectatorGUI()
{
	if ( m_bInitialized )
	{
		m_pBackGround->SetVisible( false );
		m_pSpecHelp->SetSpectatorInterface( m_pSpectatorGUI );
		m_pSpectatorGUI->Activate();
	}
}

void TeamFortressViewport::ShowSpectatorGUIBar()
{
	if ( m_bInitialized )
	{
		m_pBackGround->SetVisible( false );
		m_pSpectatorGUI->ShowBottomBar();
	}
}

void TeamFortressViewport::HideSpectatorGUI()
{
	if ( m_bInitialized )
	{
		if ( m_pSpectatorGUI->IsGUIVisible() && m_pClientDllInterface->IsSpectator() )
			DuckMessage( "#Spec_Duck" );
		m_pSpectatorGUI->HideGUI();
	}
}

void TeamFortressViewport::DeactivateSpectatorGUI()
{
	if ( m_bInitialized )
	{
		m_pSpectatorGUI->DeactivateGUI();
		DuckMessage( "#Spec_Duck" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the spectator panel with new player info
//-----------------------------------------------------------------------------
void TeamFortressViewport::UpdateSpectatorPanel()
{
	char text[128], map[128];
	_snprintf( text, sizeof( text ), "#Spec_Mode%d", m_pClientDllInterface->SpectatorMode() );
	m_pClientDllInterface->CheckSettings();
	int target = -1;
	if ( m_pClientDllInterface->SpectatorTarget() > 0 &&
		m_pClientDllInterface->SpectatorTarget() <= m_pClientDllInterface->GetMaxPlayers() &&
		m_pClientDllInterface->SpectatorMode() != 3 )
		target = m_pClientDllInterface->SpectatorTarget();
	if ( ( m_pClientDllInterface->SpectatorMode() != 5 || m_pClientDllInterface->PipInsetOff() ) && target != -1 )
	{
		VGuiLibraryPlayer_t info = m_pClientDllInterface->CallEnghudGetPlayerInfo( target );
		if ( target > 0 && info.name )
		{
			strncpy( text, info.name, sizeof( text ) );
			text[sizeof( text ) - 1] = 0;
		}
	}
	m_pClientDllInterface->COM_FileBase( (char*)m_pClientDllInterface->GetLevelName(), map );
	if ( target > 0 )
	{
		m_pSpectatorGUI->Update( text, target, m_pClientDllInterface->SpectatorMode(),
			m_pClientDllInterface->IsSpectateOnly(), m_pClientDllInterface->SpectatorNumber(), map );
		m_pSpectatorGUI->UpdateSpectatorPlayerList();
	}
}

void TeamFortressViewport::ShowTutorTextWindow( const wchar_t* text, int id, int messageClass, int spectator )
{
	if ( m_bInitialized )
		m_pTutorWindowManager->NewTutorWindow( text, id, messageClass, spectator );
}

void TeamFortressViewport::ShowTutorLine( int entity, int id )
{
	if ( m_bInitialized )
		m_pTutorWindowManager->NewTutorLine( entity, id );
}

void TeamFortressViewport::ShowTutorState( const wchar_t* )
{
}

void TeamFortressViewport::CloseTutorTextWindow()
{
	if ( m_bInitialized )
		m_pTutorWindowManager->CloseTutorTextWindow();
}

bool TeamFortressViewport::IsTutorTextWindowOpen() const
{
	return m_bInitialized && m_pTutorWindowManager->IsTutorTextWindowOpen();
}

void TeamFortressViewport::ShowCommandMenu()
{
	m_pCommandMenu->ShowCommandMenu();
}

void TeamFortressViewport::UpdateCommandMenu()
{
	m_pCommandMenu->UpdateCommandMenu();
}

void TeamFortressViewport::HideCommandMenu()
{
	m_pCommandMenu->HideCommandMenu();
}

int TeamFortressViewport::IsCommandMenuVisible()
{
	return m_pCommandMenu->IsVisible();
}

bool TeamFortressViewport::SlotInput( int slot )
{
	return m_pCommandMenu && m_pCommandMenu->SlotInput( slot );
}

bool TeamFortressViewport::AllowedToPrintText()
{
	int menu = GetCurrentMenuID();
	return menu != MENU_TEAM && menu != MENU_CLASS && menu != MENU_INTRO && menu != MENU_CLASSHELP;
}

void TeamFortressViewport::OnLevelChange()
{
	m_sMapName[0] = 0;
	m_flScoreBoardLastUpdated = 0;
	HideAllVGUIMenu();
	m_pClientScoreBoard->Reset();
	if ( m_pSpectatorGUI )
		m_pSpectatorGUI->SetVisible( false );
	if ( m_pClientScoreBoard )
		m_pClientScoreBoard->SetVisible( false );
}

//-----------------------------------------------------------------------------
// Purpose: Hides all the VGUI menus at once (except the scoreboard, as it is used in intermissions)
//-----------------------------------------------------------------------------
void TeamFortressViewport::HideAllVGUIMenu()
{
	m_pTeamMenu->SetVisible( false );
	m_pClassMenu->SetVisible( false );
	m_pMOTD->SetVisible( false );
	m_pSpecHelp->SetVisible( false );
	m_pClassHelp->SetVisible( false );
	m_pMapBriefing->SetVisible( false );
	m_pSpectatorGUI->SetVisible( false );
	if ( m_pTutorWindowManager )
		m_pTutorWindowManager->CloseAllWindows();
	m_pCommandMenu->HideCommandMenu();
	m_PendingDialogs.m_heap.Purge();
}

bool TeamFortressViewport::IsVGUIMenuActive( int menu )
{
	switch ( menu )
	{
	case MENU_TEAM: return m_pTeamMenu->IsVisible() || m_pClientScoreBoard->IsVisible();
	case MENU_CLASS: return m_pClassMenu->IsVisible() || m_pClientScoreBoard->IsVisible();
	case MENU_MAPBRIEFING: return m_pMapBriefing->IsVisible();
	case MENU_INTRO: return m_pMOTD->IsVisible() || m_pClientScoreBoard->IsVisible();
	case MENU_CLASSHELP: return m_pClassHelp->IsVisible();
	case MENU_SPECHELP: return m_pSpecHelp->IsVisible();
	default: return false;
	}
}

void TeamFortressViewport::HideVGUIMenu( int menu )
{
	switch ( menu )
	{
	case MENU_TEAM: m_pTeamMenu->SetVisible( false ); break;
	case MENU_CLASS: m_pClassMenu->SetVisible( false ); break;
	case MENU_MAPBRIEFING: m_pMapBriefing->SetVisible( false ); break;
	case MENU_INTRO: m_pMOTD->SetVisible( false ); break;
	case MENU_CLASSHELP: m_pClassHelp->SetVisible( false ); break;
	case MENU_SPECHELP: m_pSpecHelp->SetVisible( false ); break;
	}
}

void TeamFortressViewport::ShowVGUIMenu( int menu )
{
	if ( !m_bInitialized || m_pClientDllInterface->IsDemoPlayingBack() ||
		( m_pClientDllInterface->InIntermission() && menu != MENU_INTRO ) )
		return;
	for ( int i = 0; i < m_PendingDialogs.m_heap.Count(); i++ )
	{
		if ( m_PendingDialogs.m_heap[i] == menu )
			return;
	}
	bool first = !m_PendingDialogs.m_heap.Count();
	m_PendingDialogs.m_heap.AddToTail( menu );
	if ( first )
	{
		m_pBackGround->SetVisible( true );
		if ( m_pSpectatorGUI->IsGUIVisible() )
			HideSpectatorGUI();
		DisplayVGUIMenu( menu );
	}
}

void TeamFortressViewport::DisplayVGUIMenu( int menu )
{
	m_pBackGround->SetVisible( true );
	if ( m_pSpectatorGUI->IsGUIVisible() )
		HideSpectatorGUI();
	char title[32];
	switch ( menu )
	{
	case MENU_TEAM:
		m_pClientDllInterface->COM_FileBase( (char*)m_pClientDllInterface->GetLevelName(), m_sMapName );
		m_pTeamMenu->Update( m_sMapName, m_iAllowSpectators != 0, (const char**)m_sTeamNames, m_iNumTeams );
		m_pTeamMenu->Activate();
		break;
	case MENU_CLASS:
		m_pClassMenu->Update( m_iValidClasses, 5 );
		m_pClassMenu->Activate();
		break;
	case MENU_MAPBRIEFING:
	{
		char filename[64];
		if ( m_sMapName[0] )
		{
			strcpy( filename, "maps/" );
			strcat( filename, m_sMapName );
			strcat( filename, ".txt" );
		}
		else
		{
			const char* level = m_pClientDllInterface->GetLevelName();
			if ( !level )
				return;
			strcpy( filename, level );
			*strchr( filename, '.' ) = 0;
			strcat( filename, ".txt" );
			strcpy( m_sMapName, level );
			char* extension = strchr( m_sMapName, '.' );
			if ( extension )
				*extension = 0;
			char* slash = strchr( m_sMapName, '/' );
			if ( slash )
				memmove( m_sMapName, slash + 1, strlen( slash ) + 1 );
		}
		const char* text = (const char*)m_pClientDllInterface->COM_LoadFile( filename, 5, NULL );
		strncpy( title, m_sMapName, sizeof( title ) );
		title[sizeof( title ) - 1] = 0;
		m_pMapBriefing->Activate( title, text );
		break;
	}
	case MENU_INTRO:
		strncpy( title, m_szServerName[0] ? m_szServerName : "Half-Life", sizeof( title ) );
		title[sizeof( title ) - 1] = 0;
		m_pMOTD->Activate( title, m_szMOTD );
		break;
	case MENU_SPECHELP:
		m_pSpectatorGUI->SetVisible( false );
		m_pSpecHelp->Activate( "#Spec_Help_Title", "#Spec_Help_Text" );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: think function, called in hud_redraw.cpp:Think()
//-----------------------------------------------------------------------------
void TeamFortressViewport::OnTick()
{
	if ( m_pClientDllInterface->SpectatorMode() != m_iUser1 ||
		m_pClientDllInterface->SpectatorTarget() != m_iUser2 )
	{
		m_iUser1 = m_pClientDllInterface->SpectatorMode();
		m_iUser2 = m_pClientDllInterface->SpectatorTarget();
		UpdateSpectatorPanel();
	}
	if ( m_pClientDllInterface->SpectatorMode() && !IsSpectatorGUIVisible() && !IsAnyVGUIMenuActive() )
	{
		GetAllPlayersInfo();
		ShowSpectatorGUI();
	}
	else if ( !m_pClientDllInterface->SpectatorMode() && IsSpectatorGUIVisible() )
		HideSpectatorGUI();
	if ( IsSpectatorGUIVisible() )
		GetAllPlayersInfo();

/*	else if ( false spec panel needs an update )
	{
		UpdateSpectatorPanel();
	}
	

	if ( IsSpectatorGUIVisible() )
	{
		GetAllPlayersInfo();
	} */

	// Update the Scoreboard, if it's visible
	if ( m_pClientScoreBoard->IsVisible() && m_pClientDllInterface->HudTime() > m_flScoreBoardLastUpdated )
	{
		m_flScoreBoardLastUpdated = m_pClientDllInterface->HudTime() + 1.0f;
		GetAllPlayersInfo();
		m_pClientScoreBoard->Update( m_szServerName, m_pClientDllInterface->TeamPlay() != 0,
			m_pSpectatorGUI->IsVisible() );
	}
	int count = m_PendingDialogs.m_heap.Count();
	if ( count > 1 && !IsVGUIMenuActive( GetCurrentMenuID() ) )
	{
		m_PendingDialogs.m_heap.Remove( 0 );
		int menu = GetCurrentMenuID();
		m_pBackGround->SetVisible( true );
		if ( m_pSpectatorGUI->IsGUIVisible() )
			HideSpectatorGUI();
		DisplayVGUIMenu( menu );
	}
	else
	{
		if ( m_PendingDialogs.m_heap.Count() == 1 && !IsVGUIMenuActive( GetCurrentMenuID() ) )
		{
			m_PendingDialogs.m_current = m_PendingDialogs.m_heap[0];
			m_PendingDialogs.m_heap.Remove( 0 );
			m_pBackGround->SetVisible( false );
		}
		else if ( !m_PendingDialogs.m_heap.Count() && m_pBackGround->IsVisible() )
			m_pBackGround->SetVisible( false );
	}
	m_pTutorWindowManager->Think( m_pClientDllInterface->HudTime() );
}
