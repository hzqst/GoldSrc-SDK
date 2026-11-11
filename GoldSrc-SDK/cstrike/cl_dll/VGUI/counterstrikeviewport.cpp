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

// client dll/engine defines
#include "hud.h"
#include "../../../cl_dll/Exports.h"
#include "counterstrikeviewport.h"
#include "cstriketeammenu.h"
#include "cstrikeclassmenu.h"
#include "cstrikespectatorgui.h"
#include "cstrikeclientscoreboard.h"
#include "buymenu.h"
#include "buypreset_editmenu.h"
#include "buypreset_mainmenu.h"
#include "careerroundendmenu.h"
#include "careermatchendmenu.h"
#include "creditsmenu.h"
#include "CSProgressBar.h"
#include "CareerBox.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "game_controls/CreateCommandMenu.h"
#include "game_controls/mouseoverpanelbutton.h"
#include "game_controls/TutorWindowManager.h"
#include "game_controls/clientmotd.h"
#include "game_controls/vguitextwindow.h"
#include "vgui_controls/Controls.h"
#include "vgui/IScheme.h"
#include "voice_status.h"
#include "parsemsg.h" // BEGIN_READ, READ_*
#include "r_studioint.h"
#include "steam_api.h"
#include <stdlib.h>
#include <string.h>

extern engine_studio_api_t IEngineStudio;

ICSViewPort* gCSViewPortInterface;
ICSViewPortMsgs* gCSViewPortMsgs;
static CCreateCommandComboMenu g_CreateCommandMenu;

EXPOSE_SINGLE_INTERFACE( CounterStrikeViewport, IClientVGUI, "VClientVGUI001" );

CCreateCommandComboMenu* CreateCommandCombo()
{
	return &g_CreateCommandMenu;
}

void CL_DLLEXPORT HUD_ChatInputPosition( int* x, int* y )
{
	if ( gViewPortInterface )
	{
		gViewPortInterface->ChatInputPosition( x, y );
	}
}

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CounterStrikeViewport::CounterStrikeViewport()
{
	gCSViewPortInterface = this;
	m_bOldStyleMenus = false;
	m_inClientUI = false;
	m_pPrivateCSClientScoreBoard = NULL;
	gCSViewPortMsgs = this;
}

//-----------------------------------------------------------------------------
// Purpose: destructor
//-----------------------------------------------------------------------------
CounterStrikeViewport::~CounterStrikeViewport()
{
}

void CounterStrikeViewport::Initialize( CreateInterfaceFn* factories, int count )
{
	TeamFortressViewport::Initialize( factories, count );
	vgui2::scheme()->LoadSchemeFromFile( "Resource/CareerClientScheme.res", "CareerClientScheme" );
	g_pBaseUI = (IBaseUI*)factories[0]( "BaseUI001", NULL );
}

void CounterStrikeViewport::Start()
{
	// construct our panels
	m_pClassMenu = NULL;
	m_pTeamMenu = new CCSTeamMenu( NULL );
	m_pBuyMenu = NULL; // buy menu needs money info, so create it at the last possible moment
	SetClientDllInterface( &clientInterface );
	TeamFortressViewport::Start();
	m_pCSBackGround = new CCSBackGroundPanel( NULL );
	m_pBackGround = m_pCSBackGround;
	m_pPrivateSpectatorGUI = new CCSSpectatorGUI( NULL );
	m_pSpectatorGUI = m_pPrivateSpectatorGUI; // override spectator interface so the viewport uses our spectatorUI
	m_pPrivateCSClientScoreBoard = new CCSClientScoreBoardDialog( NULL );
	m_pClientScoreBoard = m_pPrivateCSClientScoreBoard;
	m_pCareerRoundEndMenu = NULL;
	m_pCareerMatchEndMenu = NULL;
	m_pCreditsMenu = NULL;
	m_pBuyPresetMainMenu = NULL;
	m_pBuyPresetEditMenu = NULL;
	m_pProgressBar = NULL;
	m_bCenteredPersistentWindows = false;
	if ( IEngineStudio.IsHardware )
	{
		m_pTutorWindowManager->m_hardwareMode = IEngineStudio.IsHardware();
	}
	if ( !SteamAPI_Init() )
	{
		gEngfuncs.Con_DPrintf( "Unable to load steam library.\n" );
		exit( -1 );
	}
}

void CounterStrikeViewport::Shutdown()
{
}

void CounterStrikeViewport::SetParent( vgui2::VPANEL parent )
{
	TeamFortressViewport::SetParent( parent );

	if ( m_pTeamMenu )
		m_pTeamMenu->SetParent( parent );
	m_pCSBackGround->SetParent( parent );
	if ( m_pClassMenu )
		m_pClassMenu->SetParent( parent );
	if ( m_pPrivateCSClientScoreBoard )
		m_pPrivateCSClientScoreBoard->SetParent( parent );
	if ( m_pCareerRoundEndMenu )
		m_pCareerRoundEndMenu->SetParent( parent );
	if ( m_pCareerMatchEndMenu )
		m_pCareerMatchEndMenu->SetParent( parent );
	if ( m_pBuyPresetEditMenu )
		m_pBuyPresetEditMenu->SetParent( parent );
	if ( m_pCreditsMenu )
		m_pCreditsMenu->SetParent( parent );
	if ( m_pBuyPresetMainMenu )
		m_pBuyPresetMainMenu->SetParent( parent );
}

int CounterStrikeViewport::KeyInput( int, int, const char* )
{
// DON'T run the base class code here, enter shouldn't bring up the team menu in CS

	return 1;
}

void CounterStrikeViewport::OnLevelChange()
{
	TeamFortressViewport::OnLevelChange();
	const char* menus = gEngfuncs.LocalPlayerInfo_ValueForKey( "_vgui_menus" );
	m_bOldStyleMenus = menus && strlen( menus ) && !atoi( menus );
	CCareerBaseBox::CloseOpenBoxes( true );
	if ( IEngineStudio.IsHardware )
	{
		m_pTutorWindowManager->m_hardwareMode = IEngineStudio.IsHardware();
	}
	UpdateScheme();
	GetAllPlayersInfo();
}

//-----------------------------------------------------------------------------
// Purpose: hides all vgui menus
//-----------------------------------------------------------------------------
void CounterStrikeViewport::HideAllVGUIMenu()
{
	TeamFortressViewport::HideAllVGUIMenu();

	if ( m_pClassMenu )
		m_pClassMenu->SetVisible( false );
	m_pTeamMenu->SetVisible( false );
	if ( m_pBuyMenu )
		m_pBuyMenu->SetVisible( false );
	if ( m_pCareerRoundEndMenu )
		m_pCareerRoundEndMenu->SetVisible( false );
	if ( m_pCareerMatchEndMenu )
		m_pCareerMatchEndMenu->SetVisible( false );
	if ( m_pCreditsMenu )
		m_pCreditsMenu->SetVisible( false );
	if ( m_pProgressBar )
		m_pProgressBar->SetVisible( false );
	if ( m_pBuyPresetMainMenu )
		m_pBuyPresetMainMenu->SetVisible( false );
	if ( m_pBuyPresetEditMenu )
		m_pBuyPresetEditMenu->SetVisible( false );
}

void CounterStrikeViewport::HideVGUIMenu( int menu )
{
	switch ( menu )
	{
	case MENU_TEAM:
		if ( m_pTeamMenu )
			m_pTeamMenu->SetVisible( false );
		break;
	case 26:
	case 27:
		if ( m_pClassMenu )
			m_pClassMenu->SetVisible( false );
		break;
	case 28:
	case 34:
		if ( m_pBuyMenu )
			m_pBuyMenu->SetVisible( false );
		break;
	case 35:
		if ( m_pCareerRoundEndMenu )
			m_pCareerRoundEndMenu->SetVisible( false );
		break;
	case 36:
		if ( m_pCareerMatchEndMenu )
			m_pCareerMatchEndMenu->SetVisible( false );
		break;
	case 37:
		if ( m_pCreditsMenu )
			m_pCreditsMenu->SetVisible( false );
		break;
	case 38:
		if ( m_pBuyPresetMainMenu )
			m_pBuyPresetMainMenu->SetVisible( false );
		[[fallthrough]];
	case 39:
		if ( m_pBuyPresetEditMenu )
			m_pBuyPresetEditMenu->SetVisible( false );
		[[fallthrough]];
	default:
		TeamFortressViewport::HideVGUIMenu( menu );
		break;
	}
	TeamFortressViewport::OnTick();
}

bool CounterStrikeViewport::IsVGUIMenuActive( int menu )
{
	vgui2::Panel* panel;
	switch ( menu )
	{
	case MENU_TEAM:
		return m_pTeamMenu && ( m_pTeamMenu->IsVisible() || m_pClientScoreBoard->IsVisible() );
	case 26:
	case 27:
		return m_pClassMenu && ( m_pClassMenu->IsVisible() || m_pClientScoreBoard->IsVisible() );
	case 28:
	case 34: panel = m_pBuyMenu; break;
	case 35: panel = m_pCareerRoundEndMenu; break;
	case 36: panel = m_pCareerMatchEndMenu; break;
	case 37: panel = m_pCreditsMenu; break;
	case 38: panel = m_pBuyPresetMainMenu; break;
	case 39: panel = m_pBuyPresetEditMenu; break;
	default: return TeamFortressViewport::IsVGUIMenuActive( menu );
	}
	return panel && panel->IsVisible();
}

void CounterStrikeViewport::UpdateScheme()
{
	const char* scheme = "ClientScheme";
	if ( g_pCareerUI )
		scheme = g_pCareerUI->IsPlayingMatch() ? "CareerClientScheme" : "ClientScheme";

	if ( m_pPrivateCSClientScoreBoard )
		m_pPrivateCSClientScoreBoard->InvalidateLayout( false, true );

	m_pCSBackGround->SetScheme( scheme );
	m_pCSBackGround->InvalidateLayout( false, true );

	if ( m_pClassMenu )
	{
		m_pClassMenu->SetScheme( scheme );
		m_pClassMenu->InvalidateLayout( false, true );
	}

	m_pTeamMenu->SetScheme( scheme );
	m_pTeamMenu->InvalidateLayout( false, true );

	if ( m_pBuyMenu )
	{
		m_pBuyMenu->SetScheme( scheme );
		m_pBuyMenu->InvalidateLayout( false, true );
	}

	if ( m_pCareerRoundEndMenu )
	{
		m_pCareerRoundEndMenu->SetScheme( scheme );
		m_pCareerRoundEndMenu->InvalidateLayout( false, true );
	}

	if ( m_pCareerMatchEndMenu )
	{
		m_pCareerMatchEndMenu->SetScheme( scheme );
		m_pCareerMatchEndMenu->InvalidateLayout( false, true );
	}

	if ( m_pCreditsMenu )
	{
		m_pCreditsMenu->SetScheme( scheme );
		m_pCreditsMenu->InvalidateLayout( false, true );
	}

	if ( m_pProgressBar )
	{
		m_pProgressBar->SetScheme( scheme );
		m_pProgressBar->InvalidateLayout( false, true );
		m_pProgressBar->SetVisible( false );
	}

	if ( m_pBuyPresetMainMenu )
	{
		m_pBuyPresetMainMenu->SetScheme( scheme );
		m_pBuyPresetMainMenu->InvalidateLayout( false, true );
	}

	if ( m_pBuyPresetEditMenu )
	{
		m_pBuyPresetEditMenu->SetScheme( scheme );
		m_pBuyPresetEditMenu->InvalidateLayout( false, true );
	}
}

int CounterStrikeViewport::MsgFunc_ForceCam( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iForceCamera = READ_BYTE();
	m_iForceChaseCam = READ_BYTE();
	m_iFadeToBlack = READ_BYTE();
	return 1;
}

int CounterStrikeViewport::GetForceCamera()
{
	if ( m_iFadeToBlack )
	{
		return 2;
	}
	return m_iForceChaseCam ? m_iForceChaseCam : m_iForceCamera;
}

int CounterStrikeViewport::MsgFunc_Location( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	short player = (short)READ_BYTE();
	if ( player > 0 && player <= MAX_PLAYERS )
	{
		strncpy( g_PlayerExtraInfo[player].location, READ_STRING(), 32 );
		GetClientVoiceHud()->UpdateLocation( player, g_PlayerExtraInfo[player].location );
	}
	return 1;
}

int CounterStrikeViewport::MsgFunc_ScoreInfo( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	short player = (short)READ_BYTE();
	short frags = (short)READ_SHORT();
	short deaths = (short)READ_SHORT();
	short playerClass = (short)READ_SHORT();
	READ_SHORT();
	if ( player > 0 && player <= MAX_PLAYERS )
	{
		g_PlayerExtraInfo[player].frags = frags;
		g_PlayerExtraInfo[player].deaths = deaths;
		g_PlayerExtraInfo[player].playerclass = playerClass;
		if ( g_PlayerExtraInfo[player].teamnumber < 0 )
			g_PlayerExtraInfo[player].teamnumber = 0;

		if ( m_pTeamMenu )
		{
			m_pTeamMenu->Update( GetMapName(), m_iAllowSpectators != 0,
				(const char**)m_sTeamNames, m_iNumTeams );
		}
		GetAllPlayersInfo();
		m_pClientScoreBoard->Update( GetServerName(), gHUD.m_Teamplay != 0, m_pSpectatorGUI->IsVisible() );
		m_pSpectatorGUI->UpdateSpectatorPlayerList();
	}
	return 1;
}

int CounterStrikeViewport::MsgFunc_TeamInfo( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	short player = (short)READ_BYTE();
	if ( player > 0 && player <= MAX_PLAYERS )
	{
		strncpy( g_PlayerExtraInfo[player].teamname, READ_STRING(), 16 );
		if ( !stricmp( g_PlayerExtraInfo[player].teamname, "CT" ) )
		{
			g_PlayerExtraInfo[player].teamnumber = 2;
		}
		else
		{
			g_PlayerExtraInfo[player].teamnumber = !stricmp( g_PlayerExtraInfo[player].teamname, "TERRORIST" );
		}
	}
	GetAllPlayersInfo();
	m_pClientScoreBoard->RebuildTeams( GetServerName(), gHUD.m_Teamplay != 0, m_pSpectatorGUI->IsVisible() );
	return 1;
}

int CounterStrikeViewport::MsgFunc_TeamScore( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	const char* teamName = READ_STRING();

	// find the team matching the name
	int team;
	for ( team = 1; team <= m_iNumTeams; team++ )
	{
		if ( !strnicmp( teamName, m_pClientScoreBoard->GetTeamName( team ), strlen( teamName ) ) )
			break;
	}

	if ( team > m_iNumTeams && team < MAX_TEAMS && team < (int)ARRAYSIZE( m_sTeamNames ) )
	{
		m_iNumTeams = GetNumberOfTeams() + 1;

		if ( m_sTeamNames[team] && strlen( m_sTeamNames[team] ) < strlen( teamName ) + 1 )
		{
			free( m_sTeamNames[team] );
			m_sTeamNames[team] = NULL;
		}
		if ( !m_sTeamNames[team] )
			m_sTeamNames[team] = (char*)malloc( strlen( teamName ) + 1 );
		strcpy( m_sTeamNames[team], teamName );

		m_pClientScoreBoard->SetTeamName( team, teamName );
	}

	// use this new score data instead of combined player scores
	//g_TeamInfo[i].scores_overriden = TRUE;
	//g_TeamInfo[i].frags = ;
	//g_TeamInfo[i].deaths = 
	m_pClientScoreBoard->SetTeamDetails( team, READ_SHORT(), 0 );
	return 1;
}

bool CounterStrikeViewport::IsProgressBarVisible()
{
	return m_pProgressBar && m_pProgressBar->IsVisible();
}

void CounterStrikeViewport::StartProgressBar( const char* title, int ticks, int progress, bool timeBased )
{
	if ( !m_pProgressBar )
	{
		m_pProgressBar = new CCSProgressBar( NULL );
		m_pProgressBar->SetParent( GetViewPortPanel() );
	}
	m_pProgressBar->Init( title, ticks, progress, timeBased );
	m_pProgressBar->Activate();
}

void CounterStrikeViewport::UpdateProgressBar( const char* status, int progress )
{
	if ( m_pProgressBar )
	{
		m_pProgressBar->Update( status, progress );
	}
}

void CounterStrikeViewport::StopProgressBar()
{
	if ( m_pProgressBar )
	{
		m_pProgressBar->SetVisible( false );
	}
}

bool CounterStrikeViewport::IsAnyVGUIMenuActive()
{
	return m_pBackGround->IsVisible() || IsProgressBarVisible() || CCareerBaseBox::IsBoxOpen();
}

void CounterStrikeViewport::UpdateBuyPresets()
{
	if ( IsVGUIMenuActive( 28 ) && m_pBuyMenu )
	{
		m_pBuyMenu->UpdateBuyPresets( false );
	}
}

void CounterStrikeViewport::ActivateClientUI()
{
	UpdateBuyPresets();
	CCareerBaseBox::CloseOpenBoxes( false );
	m_inClientUI = true;
}

void CounterStrikeViewport::HideClientUI()
{
	m_inClientUI = false;
}

//-----------------------------------------------------------------------------
// Purpose: this gets called each frame, used to update spectator gui label
//-----------------------------------------------------------------------------
void CounterStrikeViewport::OnTick()
{
	TeamFortressViewport::OnTick();
	if ( m_pSpectatorGUI->IsGUIVisible() )
	{
		m_pSpectatorGUI->UpdateTimer();
	}
	if ( m_bOldStyleMenus && !IsVGUIMenuActive( MENU_INTRO ) )
	{
		HideBackGround();
	}
	if ( m_inClientUI )
	{
		if ( !CCareerBaseBox::HasVisibleChildPopup() )
			CCareerBaseBox::BringToFront();
	}
	else
	{
		CCareerBaseBox::UndoModal();
	}
}

void CounterStrikeViewport::UpdateSpectatorPanel()
{
	TeamFortressViewport::UpdateSpectatorPanel();
	m_pSpectatorGUI->UpdateTimer();
}

void CounterStrikeViewport::CenterWindow( vgui2::Frame* window )
{
	if ( window && m_pCSBackGround )
	{
		int x, y, offsetX, offsetY;
		window->GetPos( x, y );
		m_pCSBackGround->GetOffset( offsetX, offsetY );
		window->SetPos( x + offsetX, y - (int)( offsetY * -0.67f ) );
	}
}

void CounterStrikeViewport::DisplayVGUIMenu( int menu )
{
	m_pBackGround->SetVisible( true );
	m_pBackGround->Activate();
	m_pCSBackGround->SetTitleText( "" );
	switch ( menu )
	{
	case MENU_TEAM:
		m_pTeamMenu->Update( GetMapName(), m_iAllowSpectators != 0,
			(const char**)m_sTeamNames, m_iNumTeams );
		m_pTeamMenu->Activate();
		break;
	case MENU_INTRO:
		m_pCSBackGround->SetTitleText( "" );
		gHUD.m_iIntermission = 0;
		TeamFortressViewport::DisplayVGUIMenu( MENU_INTRO );
		break;
	case 26:
	case 27:
		if ( m_pClassMenu )
		{
			m_pClassMenu->MarkForDeletion();
		}
		g_lastPanel = NULL;
		m_pClassMenu = new CCSClassMenu( NULL );
		m_pClassMenu->SetParent( GetViewPortPanel() );
		m_pClassMenu->Update( NULL, menu == 26 ? 1 : 2 );
		m_pClassMenu->Activate();
		CenterWindow( m_pClassMenu );
		break;
	case 28:
	case 34:
	{
		m_pCSBackGround->SetTitleText( "" );
		if ( g_pCareerUI && g_pCareerUI->IsPlayingMatch() )
		{
			if ( m_pBuyMenu && !m_pBuyMenu->IsFormattedForCareer() )
			{
				m_pBuyMenu->MarkForDeletion();
				m_pBuyMenu = NULL;
			}
		}
		else if ( m_pBuyMenu && m_pBuyMenu->IsFormattedForCareer() )
		{
			m_pBuyMenu->MarkForDeletion();
			m_pBuyMenu = NULL;
		}
		if ( !m_pBuyMenu )
		{
			m_pBuyMenu = new CBuyMenu( NULL );
			m_pBuyMenu->SetParent( GetViewPortPanel() );
			CenterWindow( m_pBuyMenu );
		}
		if ( menu == 28 )
		{
			m_pBuyMenu->ActivateBuyMenu();
		}
		else
		{
			m_pBuyMenu->ActivateEquipmentMenu();
		}
		break;
	}
	case 35:
		if ( !m_pCareerRoundEndMenu )
		{
			m_pCareerRoundEndMenu = new CCSCareerRoundEndMenu( NULL );
			m_pCareerRoundEndMenu->SetParent( GetViewPortPanel() );
		}
		m_pCareerRoundEndMenu->Update();
		m_pCareerRoundEndMenu->Activate();
		break;
	case 36:
		if ( !m_pCareerMatchEndMenu )
		{
			m_pCareerMatchEndMenu = new CCSCareerMatchEndMenu( NULL );
			m_pCareerMatchEndMenu->SetParent( GetViewPortPanel() );
		}
		m_pCareerMatchEndMenu->Update();
		m_pCareerMatchEndMenu->Activate();
		break;
	case 37:
		if ( !m_pCreditsMenu )
		{
			m_pCreditsMenu = new CCSCreditsMenu( NULL );
			m_pCreditsMenu->SetParent( GetViewPortPanel() );
		}
		m_pCreditsMenu->Activate();
		break;
	case 38:
		m_pCSBackGround->SetTitleText( "" );
		if ( !m_pBuyPresetMainMenu )
		{
			m_pBuyPresetMainMenu = new CBuyPresetEditMainMenu( NULL );
			m_pBuyPresetMainMenu->SetParent( GetViewPortPanel() );
		}
		m_pBuyPresetMainMenu->Update();
		m_pBuyPresetMainMenu->Activate();
		break;
	case 39:
		m_pCSBackGround->SetTitleText( "" );
		if ( !m_pBuyPresetEditMenu )
		{
			m_pBuyPresetEditMenu = new CBuyPresetEditMenu( NULL );
			m_pBuyPresetEditMenu->SetParent( GetViewPortPanel() );
		}
		m_pBuyPresetEditMenu->Update();
		m_pBuyPresetEditMenu->Activate();
		break;
	default:
		TeamFortressViewport::DisplayVGUIMenu( menu );
		break;
	}
	if ( !m_bCenteredPersistentWindows )
	{
		CenterWindow( m_pMOTD );
		CenterWindow( m_pTeamMenu );
		CenterWindow( m_pSpecHelp );
		CenterWindow( m_pClassHelp );
		CenterWindow( m_pMapBriefing );
		m_bCenteredPersistentWindows = true;
	}
	CCareerBaseBox::BringToFront();
}


/*
==========================
HUD_ChatInputPosition

Sets the location of the input for chat text
==========================
*/
//MIKETODO: positioning of chat text (and other engine output)
/*
	#include "Exports.h"

	void CL_DLLEXPORT HUD_ChatInputPosition( int *x, int *y )
	{
		RecClChatInputPosition( x, y );
		if ( gViewPortInterface )
		{
			gViewPortInterface->ChatInputPosition( x, y );
		}
	}

	EXPOSE_SINGLE_INTERFACE(CounterStrikeViewport, IClientVGUI, CLIENTVGUI_INTERFACE_VERSION);
*/
