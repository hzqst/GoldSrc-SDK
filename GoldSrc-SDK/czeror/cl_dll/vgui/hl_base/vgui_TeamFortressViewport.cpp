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

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "cl_entity.h"
#include "demo_api.h"
#include "vgui_TeamFortressViewport.h"
#include "ClientScoreBoardDialog.h"
#include "SpectatorGUI.h"
#include "clientmotd.h"
#include "teammenu.h"
#include "classmenu.h"
#include "vguitextwindow.h"
#include "CreateCommandMenu.h"
#include "Menu.h"
#include "Controls.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "igameuifuncs.h"
#include "voice_status.h"
#include "keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

IViewPort* gViewPortInterface;
IViewPortMsgs* gViewPortMsgs;
IGameUIFuncs* gameuifuncs;
vgui2::Panel* g_lastPanel;
int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1;
int g_iUser2;
int g_iUser3;
char* sTFClassSelection[] =
{
	"civilian", "scout", "sniper", "soldier", "demoman", "medic", "hwguy", "pyro", "spy", "engineer", "randompc", NULL
};
extern team_info_t g_TeamInfo[MAX_PLAYERS + 1];
extern int g_IsSpectator[MAX_PLAYERS + 1];
extern CVoiceStatus* GetClientVoice();

void TeamFortressViewport::CBackGroundPanel::PerformLayout()
{
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	SetBounds( 0, 0, wide, tall );
	vgui2::Panel::PerformLayout();
}

void TeamFortressViewport::CBackGroundPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Panel::ApplySchemeSettings( scheme );
	SetBgColor( scheme->GetColor( "ViewportBG", Color( 0, 0, 0, 0 ) ) );
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
TeamFortressViewport::TeamFortressViewport() : m_bInitialized( false ), m_PendingDialogs( 0, 0 ), m_iNumTeams( 0 )
{
	gViewPortInterface = this;
	gViewPortMsgs = this;
	m_szServerName[0] = 0;
}

TeamFortressViewport::~TeamFortressViewport()
{
	m_bInitialized = false;
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
	gameuifuncs = (IGameUIFuncs*)factories[0]( "VENGINE_GAMEUIFUNCS_VERSION001", NULL );
}

void TeamFortressViewport::Start()
{
	m_pBackGround = new CBackGroundPanel;
	m_pBackGround->SetZPos( -20 ); // send it to the back 
	m_pBackGround->SetVisible( false );
	m_pPrivateClientScoreBoard = new CClientScoreBoardDialog( NULL );
	m_pClientScoreBoard = m_pPrivateClientScoreBoard;
	m_pMOTD = new CClientMOTD( NULL );
	m_pPrivateSpectatorGUI = new CSpectatorGUI( NULL );
	m_pSpectatorGUI = m_pPrivateSpectatorGUI;
	m_pTeamMenu = new CTeamMenu( NULL );
	m_pClassMenu = new CClassMenu( NULL );
	m_pSpecHelp = new CSpecHelpWindow( m_pSpectatorGUI );
	m_pClassHelp = new CVGUITextWindow( NULL );
	m_pMapBriefing = new CVGUITextWindow( NULL );
	m_iCurrentTeamNumber = 0;
	m_pCommandMenu = CreateCommandMenu( "commandmenu.txt", g_iTeamNumber, m_pMOTD );
	HideAllVGUIMenu();
	m_iGotAllMOTD = 1;
	m_iRandomPC = 0;
	m_flScoreBoardLastUpdated = 0;
	g_iPlayerClass = 0;
	g_iTeamNumber = 0;
	m_sMapName[0] = 0;
	m_szServerName[0] = 0;
	for ( int i = 0; i < 5; i++ )
	{
		m_iValidClasses[i] = 0;
		m_sTeamNames[i][0] = 0;
	}
	m_bInitialized = true;
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
	m_pClientScoreBoard->SetMouseInput( false );
}

bool TeamFortressViewport::UseVGUI1() { return false; }
void TeamFortressViewport::ActivateClientUI() {}
void TeamFortressViewport::HideClientUI() {}
void TeamFortressViewport::Shutdown() {}
void TeamFortressViewport::GetAllPlayersInfo() {}
int TeamFortressViewport::GetAllowSpectators() { return m_iAllowSpectators; }
int TeamFortressViewport::GetValidClasses( int team ) { return m_iValidClasses[team]; }
int TeamFortressViewport::GetNumberOfTeams() { return m_iNumTeams; }
bool TeamFortressViewport::GetIsFeigning() { return m_bIsFeigning; }
bool TeamFortressViewport::GetIsSettingDetpack() { return m_bIsSettingDetpack; }
int TeamFortressViewport::GetBuildState() { return m_iBuildState; }
int TeamFortressViewport::IsRandomPC() { return m_iRandomPC; }
char* TeamFortressViewport::GetTeamName( int team ) { return m_sTeamNames[team]; }
const char* TeamFortressViewport::GetServerName() { return m_szServerName; }
int TeamFortressViewport::GetCurrentMenuID() { return m_PendingDialogs.Count() ? m_PendingDialogs.Head() : -1; }
int TeamFortressViewport::GetViewPortScheme() { return (int)m_pBackGround->GetScheme(); }
vgui2::Panel* TeamFortressViewport::GetViewPortPanel() { return m_pBackGround->GetParent(); }
void TeamFortressViewport::HideBackGround() { m_pBackGround->SetVisible( false ); }
void TeamFortressViewport::ShowCommandMenu() { m_pCommandMenu->SetVisible( true ); }
void TeamFortressViewport::HideCommandMenu() { m_pCommandMenu->SetVisible( false ); }
int TeamFortressViewport::IsCommandMenuVisible() { return m_pCommandMenu->IsVisible(); }
bool TeamFortressViewport::IsAnyVGUIMenuActive() { return m_pBackGround->IsVisible(); }
void TeamFortressViewport::SetSpectatorBanner( const char* image ) { m_pSpectatorGUI->SetImage( image ); }
void TeamFortressViewport::SpectatorGUIEnableInsetView( int enabled ) { m_pSpectatorGUI->EnableInsetView( enabled ); }
int TeamFortressViewport::GetSpectatorTopBarHeight() { return m_pSpectatorGUI->GetTopBarHeight(); }
int TeamFortressViewport::GetSpectatorBottomBarHeight() { return m_pSpectatorGUI->GetBottomBarHeight(); }

const char* TeamFortressViewport::GetMapName()
{
	if ( !m_sMapName[0] )
		COM_FileBase( gEngfuncs.pfnGetLevelName(), m_sMapName );
	return m_sMapName;
}

void TeamFortressViewport::OnLevelChange()
{
	m_sMapName[0] = 0;
	gViewPortInterface->HideAllVGUIMenu();
}

void TeamFortressViewport::InputPlayerSpecial()
{
	if ( m_bInitialized && (unsigned int)( g_iPlayerClass - 8 ) > 1 )
		gEngfuncs.pfnClientCmd( "_special" );
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
	m_PendingDialogs.Purge();
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

bool TeamFortressViewport::IsVGUIMenuActive( int menu )
{
	switch ( menu )
	{
	case MENU_TEAM: return m_pTeamMenu->IsVisible();
	case MENU_CLASS: return m_pClassMenu->IsVisible();
	case MENU_MAPBRIEFING: return m_pMapBriefing->IsVisible();
	case MENU_INTRO: return m_pMOTD->IsVisible();
	case MENU_CLASSHELP: return m_pClassHelp->IsVisible();
	case MENU_SPECHELP: return m_pSpecHelp->IsVisible();
	default: return false;
	}
}

bool TeamFortressViewport::AllowedToPrintText()
{
	int menu = GetCurrentMenuID();
	return menu != MENU_TEAM && menu != MENU_CLASS && menu != MENU_INTRO && menu != MENU_CLASSHELP;
}

void TeamFortressViewport::ShowVGUIMenu( int menu )
{
	if ( !m_bInitialized || gEngfuncs.pDemoAPI->IsPlayingback() || ( gHUD.m_iIntermission && menu != MENU_INTRO ) )
		return;
	if ( m_PendingDialogs.Count() )
	{
		if ( !m_PendingDialogs.Check( menu ) )
			m_PendingDialogs.Insert( menu );
		return;
	}
	m_PendingDialogs.Insert( menu );
	m_pBackGround->SetVisible( true );
	if ( m_pSpectatorGUI->IsGUIVisible() )
		HideSpectatorGUI();
	DisplayVGUIMenu( menu );
}

void TeamFortressViewport::ShowScoreBoard()
{
	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		GetAllPlayersInfo();
		if ( m_bInitialized )
		{
			m_pClientScoreBoard->Activate( false );
			m_pClientScoreBoard->MoveToFront();
		}
	}
}

void TeamFortressViewport::HideScoreBoard()
{
	if ( !gHUD.m_iIntermission && m_bInitialized )
	{
		m_pClientScoreBoard->SetVisible( false );
		GetClientVoice()->StopSquelchMode();
	}
}

bool TeamFortressViewport::IsScoreBoardVisible()
{
	return m_bInitialized && m_pClientScoreBoard->IsVisible();
}


//-----------------------------------------------------------------------------
// Purpose: updates the scoreboard when something changes
//-----------------------------------------------------------------------------
void TeamFortressViewport::UpdateScoreBoard()
{
	GetAllPlayersInfo();
	m_pClientScoreBoard->RebuildTeams( m_szServerName, gHUD.m_Teamplay != 0, m_pSpectatorGUI->IsVisible() );
}

void TeamFortressViewport::DeathMsg( int killer, int victim )
{
	m_pClientScoreBoard->DeathMsg( killer, victim );
	if ( victim == gEngfuncs.GetLocalPlayer()->index )
		UpdateSpectatorPanel();
}

void TeamFortressViewport::ShowSpectatorGUI()
{
	if ( m_bInitialized )
	{
		m_pBackGround->SetVisible( false );
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

void DuckMessage( const char* text )
{
	static char tempString[128];
	static client_textmessage_t tst;
	strncpy( tempString, text, sizeof( tempString ) );
	tst.pName = "Spec_Duck";
	tst.effect = 2;
	tst.r1 = 255;
	tst.g1 = 200;
	tst.b1 = 0;
	tst.a1 = 200;
	tst.r2 = 0;
	tst.g2 = 255;
	tst.b2 = 0;
	tst.a2 = 200;
	tst.x = -1;
	tst.y = -1;
	tst.fadein = 0.01f;
	tst.fadeout = 0.7f;
	tst.fxtime = 0.07f;
	tst.holdtime = 6;
	tst.pMessage = tempString;
	gHUD.m_Message.MessageAdd( &tst );
}

void TeamFortressViewport::HideSpectatorGUI()
{
	if ( m_bInitialized )
	{
		if ( m_pSpectatorGUI->IsVisible() && g_iUser1 )
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

bool TeamFortressViewport::IsSpectatorGUIVisible() { return m_bInitialized && m_pSpectatorGUI->IsGUIVisible(); }
bool TeamFortressViewport::IsSpectatorBarVisible() { return m_bInitialized && m_pSpectatorGUI->IsActiveGUIVisible(); }

void TeamFortressViewport::ChatInputPosition( int*, int* y )
{
	if ( g_iUser1 || gEngfuncs.IsSpectateOnly() )
		*y = ( gHUD.m_Spectator.m_pip->value == 0 ? m_pSpectatorGUI->GetTopBarHeight() : gHUD.m_Spectator.m_OverviewData.insetWindowY + gHUD.m_Spectator.m_OverviewData.insetWindowHeight ) + 5;
}

void EXPORT HUD_ChatInputPosition( int* x, int* y )
{
	if ( gViewPortInterface )
		gViewPortInterface->ChatInputPosition( x, y );
}

void HudMessage( char* text )
{
	gHUD.m_TextMessage.MsgFunc_TextMsg( NULL, (int)strlen( text ) + 1, text );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if a particular class is valid for this map
//-----------------------------------------------------------------------------
int GetValidClasses( int team )
{
	return gViewPortInterface->GetValidClasses( team );
}

int TeamFortressViewport::MsgFunc_ValClass( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	for ( int i = 0; i < 5; i++ )
		m_iValidClasses[i] = READ_SHORT();
	m_pClassMenu->Update( m_iValidClasses, 5 );
	return 1;
}

int TeamFortressViewport::MsgFunc_Feign( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_bIsFeigning = READ_BYTE() != 0;
	return 1;
}

int TeamFortressViewport::MsgFunc_Detpack( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_bIsSettingDetpack = READ_BYTE() != 0;
	return 1;
}

int TeamFortressViewport::MsgFunc_BuildSt( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iBuildState = READ_SHORT();
	return 1;
}

int TeamFortressViewport::MsgFunc_RandomPC( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iRandomPC = READ_BYTE();
	return 1;
}

int TeamFortressViewport::MsgFunc_ServerName( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	strncpy( m_szServerName, READ_STRING(), sizeof( m_szServerName ) );
	m_szServerName[sizeof( m_szServerName ) - 1] = 0;
	return 1;
}

int TeamFortressViewport::MsgFunc_AllowSpec( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iAllowSpectators = READ_BYTE();
	if ( m_pTeamMenu )
		m_pTeamMenu->Update( m_sMapName, m_iAllowSpectators != 0, m_sTeamNames, m_iNumTeams );
	return 1;
}

int TeamFortressViewport::MsgFunc_VGUIMenu( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int menu = READ_BYTE();
	if ( menu == MENU_MAPBRIEFING )
	{
		strncpy( m_sMapName, READ_STRING(), sizeof( m_sMapName ) );
		m_sMapName[sizeof( m_sMapName ) - 1] = 0;
	}
	ShowVGUIMenu( menu );
	return 1;
}

int TeamFortressViewport::MsgFunc_MOTD( const char*, int size, void* buffer )
{
	if ( m_iGotAllMOTD )
		m_szMOTD[0] = 0;
	BEGIN_READ( buffer, size );
	m_iGotAllMOTD = READ_BYTE();
	int remaining = 1535 - (int)strlen( m_szMOTD );
	strncat( m_szMOTD, READ_STRING(), remaining < 0 ? 0 : remaining );
	m_szMOTD[1535] = 0;
	if ( m_iGotAllMOTD && !gEngfuncs.IsSpectateOnly() )
		ShowVGUIMenu( MENU_INTRO );
	return 1;
}

int TeamFortressViewport::MsgFunc_ScoreInfo( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	short player = (short)READ_BYTE();
	short frags = (short)READ_SHORT();
	short deaths = (short)READ_SHORT();
	short playerClass = (short)READ_SHORT();
	short team = (short)READ_SHORT();
	if ( (unsigned short)( player - 1 ) <= 63 )
	{
		g_PlayerExtraInfo[player].frags = frags;
		g_PlayerExtraInfo[player].deaths = deaths;
		g_PlayerExtraInfo[player].playerclass = playerClass;
		g_PlayerExtraInfo[player].teamnumber = team < 0 ? 0 : team;
		if ( m_pTeamMenu )
			m_pTeamMenu->Update( m_sMapName, m_iAllowSpectators != 0, m_sTeamNames, m_iNumTeams );
		GetAllPlayersInfo();
		m_pClientScoreBoard->Update( m_szServerName, gHUD.m_Teamplay != 0, m_pSpectatorGUI->IsVisible() );
		m_pSpectatorGUI->UpdateSpectatorPlayerList();
	}
	return 1;
}

int TeamFortressViewport::MsgFunc_TeamInfo( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	short player = (short)READ_BYTE();
	if ( (unsigned short)( player - 1 ) <= 63 )
		strncpy( g_PlayerExtraInfo[player].teamname, READ_STRING(), 16 );
	GetAllPlayersInfo();
	m_pClientScoreBoard->RebuildTeams( m_szServerName, gHUD.m_Teamplay != 0, m_pSpectatorGUI->IsVisible() );
	return 1;
}

int TeamFortressViewport::MsgFunc_TeamNames( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iNumTeams = READ_BYTE();
	if ( m_iNumTeams > (int)( sizeof( m_sTeamNames ) / sizeof( m_sTeamNames[0] ) ) - 2 )
		m_iNumTeams = (int)( sizeof( m_sTeamNames ) / sizeof( m_sTeamNames[0] ) ) - 2;
	for ( int i = 0; i < m_iNumTeams; i++ )
	{
		strncpy( m_sTeamNames[i], READ_STRING(), 32 );
		strncpy( g_TeamInfo[i + 1].name, m_sTeamNames[i], sizeof( g_TeamInfo[i + 1].name ) - 1 );
		g_TeamInfo[i + 1].name[sizeof( g_TeamInfo[i + 1].name ) - 1] = 0;
	}
	strncpy( m_sTeamNames[m_iNumTeams], "#Team_AutoAssign", 32 );
	strncpy( m_sTeamNames[m_iNumTeams + 1], "#Menu_Spectate", 32 );
	if ( m_pTeamMenu )
		m_pTeamMenu->Update( m_sMapName, m_iAllowSpectators != 0, m_sTeamNames, m_iNumTeams );
	return 1;
}

int TeamFortressViewport::MsgFunc_TeamScore( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	const char* name = READ_STRING();
	int team = 1;
	if ( m_iNumTeams > 0 )
	{
		while ( _strnicmp( name, g_TeamInfo[team].name, strlen( name ) ) )
		{
			++team;
			if ( team > m_iNumTeams )
				break;
		}
	}
	if ( team > m_iNumTeams && team < 64 )
	{
		++m_iNumTeams;
		strncpy( g_TeamInfo[team].name, name, sizeof( g_TeamInfo[team].name ) - 1 );
		g_TeamInfo[team].name[sizeof( g_TeamInfo[team].name ) - 1] = 0;
	}
	g_TeamInfo[team].scores_overriden = TRUE;
	g_TeamInfo[team].frags = (short)READ_SHORT();
	g_TeamInfo[team].deaths = (short)READ_SHORT();
	return 1;
}

int TeamFortressViewport::MsgFunc_Spectator( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	short player = (short)READ_BYTE();
	if ( (unsigned short)( player - 1 ) <= 63 )
		g_IsSpectator[player] = READ_BYTE();
	return 1;
}

int TeamFortressViewport::MsgFunc_SpecFade( const char*, int, void* ) { return 1; }
int TeamFortressViewport::MsgFunc_ResetFade( const char*, int, void* ) { return 1; }

int TeamFortressViewport::KeyInput( int down, int key, const char* )
{
	if ( m_iUser1 && !gEngfuncs.Con_IsVisible() && down && ( key == K_ENTER || key == K_KP_ENTER ) )
		ShowVGUIMenu( MENU_TEAM );
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Updates the spectator panel with new player info
//-----------------------------------------------------------------------------
void TeamFortressViewport::UpdateSpectatorPanel()
{
	char bottomText[128];
	sprintf( bottomText, "#Spec_Mode%d", g_iUser1 );
	gHUD.m_Spectator.CheckSettings();
	int player = g_iUser2 > 0 && g_iUser2 <= MAX_PLAYERS && gEngfuncs.GetMaxClients() >= g_iUser2 ? g_iUser2 : -1;
	if ( g_iUser1 == 3 )
		player = -1;
	if ( g_iUser1 != 5 || gHUD.m_Spectator.m_pip->value != 0 )
	{
		if ( player > 0 && g_PlayerInfoList[player].name )
		{
			strncpy( bottomText, g_PlayerInfoList[player].name, sizeof( bottomText ) - 1 );
			bottomText[sizeof( bottomText ) - 1] = 0;
		}
	}
	char mapName[64];
	COM_FileBase( gEngfuncs.pfnGetLevelName(), mapName );
	m_pSpectatorGUI->Update( bottomText, player, g_iUser1, gEngfuncs.IsSpectateOnly() != 0,
		gHUD.m_Spectator.m_iSpectatorNumber, mapName );
	m_pSpectatorGUI->UpdateSpectatorPlayerList();
}

//-----------------------------------------------------------------------------
// Purpose: think function, called in hud_redraw.cpp:Think()
//-----------------------------------------------------------------------------
void TeamFortressViewport::OnTick()
{
	if ( m_iUser1 != g_iUser1 || m_iUser2 != g_iUser2 )
	{
		m_iUser1 = g_iUser1;
		m_iUser2 = g_iUser2;
		UpdateSpectatorPanel();
	}
	if ( g_iUser1 )
	{
		if ( !IsSpectatorGUIVisible() && !IsAnyVGUIMenuActive() )
		{
			GetAllPlayersInfo();
			ShowSpectatorGUI();
		}
	}
	else if ( IsSpectatorGUIVisible() )
		HideSpectatorGUI();
	if ( IsSpectatorGUIVisible() )
		GetAllPlayersInfo();
	if ( m_pClientScoreBoard->IsVisible() && gHUD.m_flTime > m_flScoreBoardLastUpdated )
	{
		m_flScoreBoardLastUpdated = (float)( gHUD.m_flTime + 1.0 );
		GetAllPlayersInfo();
		m_pClientScoreBoard->Update( m_szServerName, gHUD.m_Teamplay != 0, m_pSpectatorGUI->IsVisible() );
	}
	if ( m_PendingDialogs.Count() > 1 && !IsVGUIMenuActive( m_PendingDialogs.Head() ) ) // last show menu isn't active, show the next
	{
		m_PendingDialogs.RemoveAtHead();
		int menu = m_PendingDialogs.Head();
		m_pBackGround->SetVisible( true );
		if ( m_pSpectatorGUI->IsGUIVisible() )
			HideSpectatorGUI();
		DisplayVGUIMenu( menu );
		return;
	}
	if ( m_PendingDialogs.Count() == 1 && !IsVGUIMenuActive( m_PendingDialogs.Head() ) )
		m_PendingDialogs.RemoveAtHead();
}

void TeamFortressViewport::DisplayVGUIMenu( int menu )
{
	m_pBackGround->SetVisible( true );
	if ( m_pSpectatorGUI->IsGUIVisible() )
		HideSpectatorGUI();
	char title[32];
	char fileName[64];
	switch ( menu )
	{
	case MENU_TEAM:
		COM_FileBase( gEngfuncs.pfnGetLevelName(), m_sMapName );
		m_pTeamMenu->Update( m_sMapName, m_iAllowSpectators != 0, m_sTeamNames, m_iNumTeams );
		m_pTeamMenu->Activate();
		break;
	case MENU_CLASS:
		m_pClassMenu->Update( m_iValidClasses, 5 );
		m_pClassMenu->Activate();
		break;
	case MENU_MAPBRIEFING:
		if ( m_sMapName[0] )
		{
			strcpy( fileName, "maps/" );
			strncat( fileName, m_sMapName, sizeof( fileName ) - sizeof( "maps/.txt" ) );
			strcat( fileName, ".txt" );
		}
		else
		{
			const char* level = gEngfuncs.pfnGetLevelName();
			if ( !level )
				return;
			strcpy( fileName, level );
			*strchr( fileName, '.' ) = 0;
			strcat( fileName, ".txt" );
			strcpy( m_sMapName, level );
			char* extension = strchr( m_sMapName, '.' );
			if ( extension )
				*extension = 0;
			char* slash = strchr( m_sMapName, '/' );
			if ( slash )
				memmove( m_sMapName, slash + 1, strlen( slash ) + 1 );
		}
		{
			const char* message = (const char*)gEngfuncs.COM_LoadFile( fileName, 5, NULL );
			strncpy( title, m_sMapName, sizeof( title ) );
			title[sizeof( title ) - 1] = 0;
			m_pMapBriefing->Activate( title, message );
		}
		break;
	case MENU_INTRO:
		strncpy( title, m_szServerName[0] ? m_szServerName : "Half-Life", sizeof( title ) );
		title[sizeof( title ) - 1] = 0;
		m_pMOTD->Activate( title, m_szMOTD );
		break;
	case MENU_CLASSHELP:
		{
			const char* classTitle;
			switch ( g_iPlayerClass )
			{
			case 1: classTitle = "#Title_scout"; break;
			case 2: classTitle = "#Title_sniper"; break;
			case 3: classTitle = "#Title_soldier"; break;
			case 4: classTitle = "#Title_demoman"; break;
			case 5: classTitle = "#Title_medic"; break;
			case 6: classTitle = "#Title_hwguy"; break;
			case 7: classTitle = "#Title_pyro"; break;
			case 8: classTitle = "#Title_spy"; break;
			case 9: classTitle = "#Title_engineer"; break;
			case 11: classTitle = "#Title_civilian"; break;
			default: return;
			}
			if ( g_iPlayerClass == 11 )
				sprintf( fileName, "classes/long_civilian.txt" );
			else
				sprintf( fileName, "classes/long_%s.txt", sTFClassSelection[g_iPlayerClass] );
			const char* message = (const char*)gEngfuncs.COM_LoadFile( fileName, 5, NULL );
			m_pClassHelp->Activate( classTitle, message );
		}
		break;
	case MENU_SPECHELP:
		m_pSpectatorGUI->SetVisible( false ); // hide the spectator UI, closing the spec help one reveals it again
		m_pSpecHelp->Activate( "#Spec_Help_Title", "#Spec_Help_Text" );
		break;
	}
}
