#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "dodviewport.h"
#include "dodteammenu.h"
#include "dodclassmenu.h"
#include "dodclientscoreboard.h"
#include "dodspectatorgui.h"
#include "dodmotd.h"
#include "hintpanel.h"
#include "mortarpanel.h"
#include "widescreenpanel.h"
#include "controls.h"
#include "vgui/ISurface.h"
#include "interface.h"

using namespace vgui2;

IDoDViewPort* gDoDViewPortInterface;
IDoDViewPortMsgs* gDoDViewPortMsgs;
cvar_t* cl_dmsmallmap;
cvar_t* cl_dmshowmarkers;
cvar_t* cl_dmshowplayers;
cvar_t* cl_dmshowflags;
cvar_t* cl_dmshowobjects;
cvar_t* cl_dmshowgrenades;

extern Panel* g_lastPanel;

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
DoDViewport::DoDViewport()
{
	memset( m_BackgroundTopVerts, 0, sizeof( m_BackgroundTopVerts ) );
	memset( m_BackgroundMainVerts, 0, sizeof( m_BackgroundMainVerts ) );
	gDoDViewPortInterface = this;
	gDoDViewPortMsgs = this;
	m_iClassMask = -1;
}

void DoDViewport::Start()
{
	SetClientDllInterface( &clientInterface );
	m_pClassMenu = NULL;
	m_pTeamMenu = new CDoDTeamMenu( NULL );
	m_pHintPanel = new CHintPanel( NULL );
	m_pWidescreenPanel = new CWidescreenPanel( NULL );
	m_pMortarPanel = new CMortarPanel( NULL );
	TeamFortressViewport::Start();
	m_pPrivateSpectatorGUI = new CDoDSpectatorGUI( NULL );
	m_pSpectatorGUI = m_pPrivateSpectatorGUI;
	m_pPrivateClientScoreBoard = new CDoDClientScoreBoardDialog( NULL );
	m_pClientScoreBoard = m_pPrivateClientScoreBoard;
	m_pMOTD = new CDoDMOTD( NULL );
	m_iBackgroundTexture = surface()->CreateNewTextureID( false );
	surface()->DrawSetTextureFile( m_iBackgroundTexture, "gfx/vgui/white", false, false );
	int top[4][2] = { {48,42}, {406,42}, {428,64}, {48,64} };
	int main[5][2] = { {48,64}, {588,64}, {588,416}, {566,438}, {48,438} };
	for ( int vertex = 0; vertex < 4; vertex++ )
	{
		m_BackgroundTopVerts[vertex].x = scheme()->GetProportionalScaledValue( top[vertex][0] );
		m_BackgroundTopVerts[vertex].y = scheme()->GetProportionalScaledValue( top[vertex][1] );
		m_BackgroundTopVerts[vertex].u = m_BackgroundTopVerts[vertex].v = 0;
	}
	for ( int vertex = 0; vertex < 5; vertex++ )
	{
		m_BackgroundMainVerts[vertex].x = scheme()->GetProportionalScaledValue( main[vertex][0] );
		m_BackgroundMainVerts[vertex].y = scheme()->GetProportionalScaledValue( main[vertex][1] );
		m_BackgroundMainVerts[vertex].u = m_BackgroundMainVerts[vertex].v = 0;
	}
}

void DoDViewport::SetParent( VPANEL parent )
{
	TeamFortressViewport::SetParent( parent );
	if ( m_pClassMenu )
		m_pClassMenu->SetParent( parent );
	m_pTeamMenu->SetParent( parent );
	m_pHintPanel->SetParent( parent );
	m_pWidescreenPanel->SetParent( parent );
	m_pMortarPanel->SetParent( parent );
}

void DoDViewport::HideAllVGUIMenu()
{
	TeamFortressViewport::HideAllVGUIMenu();
	if ( m_pClassMenu )
		m_pClassMenu->SetVisible( false );
	m_pTeamMenu->SetVisible( false );
	m_pHintPanel->SetVisible( false );
	m_pWidescreenPanel->SetVisible( false );
	m_pMOTD->SetVisible( false );
	m_pMortarPanel->HideMortarPanel();
}

int DoDViewport::KeyInput( int down, int keynum, const char* binding )
{
	return m_pMortarPanel->IsVisible() ? m_pMortarPanel->KeyInput( down, keynum, binding ) : 1;
}

bool DoDViewport::CanShowScoreBoard()
{
	return true;
}

int DoDViewport::GetForceCamera()
{
	if ( m_iFadeToBlack )
		return 2;
	return m_iForceChaseCam ? m_iForceChaseCam : m_iForceCamera;
}

void DoDViewport::setWidescreenVisible( bool visible )
{
	m_pWidescreenPanel->SetVisible( visible );
}

bool DoDViewport::isWidescreenVisible()
{
	return m_pWidescreenPanel->IsVisible();
}

void DoDViewport::setWidescreenSubtitle( int subtitle )
{
	m_pWidescreenPanel->setSubtitle( subtitle );
}

void DoDViewport::updateTeamMenu()
{
	m_pTeamMenu->Update();
}

void DoDViewport::ShowHintMessage( const char* message )
{
	int team = g_iTeamNumber == 1 ? !gHUD.m_bBritish : g_iTeamNumber;
	m_pHintPanel->ShowHintMessage( team, message );
	m_pHintPanel->SetVisible( true );
}

void DoDViewport::ShowSpectatorGUI()
{
	TeamFortressViewport::ShowSpectatorGUI();
}

void DoDViewport::DeactivateSpectatorGUI()
{
	TeamFortressViewport::DeactivateSpectatorGUI();
}

void DoDViewport::UpdateSpectatorPanel()
{
	TeamFortressViewport::UpdateSpectatorPanel();
	m_pSpectatorGUI->UpdateTimer();
}

void DoDViewport::UpdateOnPlayerInfo()
{
	TeamFortressViewport::UpdateSpectatorPanel();
	TeamFortressViewport::UpdateScoreBoard();
	m_pSpectatorGUI->UpdateSpectatorPlayerList();
}

int DoDViewport::GetTeamScore( int team )
{
	return m_pPrivateClientScoreBoard ? m_pPrivateClientScoreBoard->GetTeamScore( team ) : -1;
}

ExtraScoreBoardInfo DoDViewport::GetExtraScoreBoardInfo( int team )
{
	return m_pPrivateClientScoreBoard->GetExtraScoreBoardInfo( team );
}

VGuiLibraryTeamInfo_t DoDViewport::GetPlayerTeamInfo( int team )
{
	return m_pClientScoreBoard->GetPlayerTeamInfo( team );
}

void DoDViewport::ShowMortarPanel()
{
	m_pMortarPanel->ShowMortarPanel();
}

void DoDViewport::HideMortarPanel()
{
	m_pMortarPanel->HideMortarPanel();
}

void DoDViewport::PostMortarValue( float value )
{
	m_pMortarPanel->PostMortarValue( value );
}

void DoDViewport::PaintBackgroundPanel()
{
	surface()->DrawSetColor( 0, 0, 0, 255 );
	surface()->DrawSetTexture( m_iBackgroundTexture );
	surface()->DrawTexturedPolygon( m_BackgroundTopVerts, 4 );
	surface()->DrawTexturedPolygon( m_BackgroundMainVerts, 5 );
}

void DoDViewport::OnTick()
{
	TeamFortressViewport::OnTick();
	if ( m_pSpectatorGUI->IsVisible() )
		m_pSpectatorGUI->UpdateTimer();
	if ( m_pHintPanel )
		m_pHintPanel->OnTick();
	if ( m_pTeamMenu && m_pTeamMenu->IsVisible() )
		m_pTeamMenu->OnTick();
	if ( m_pClassMenu && m_pClassMenu->IsVisible() )
		m_pClassMenu->OnTick();
	if ( m_pMortarPanel->IsVisible() )
		m_pMortarPanel->OnTick();
}

bool DoDViewport::IsVGUIMenuActive( int menu )
{
	if ( menu == MENU_TEAM )
	{
		if ( m_pTeamMenu->IsVisible() )
			return true;
	}
	else if ( menu >= 10 && menu <= 14 )
	{
		if ( !m_pClassMenu )
			return false;
		if ( m_pClassMenu->IsVisible() )
			return true;
	}
	else
		return TeamFortressViewport::IsVGUIMenuActive( menu );
	return m_pClientScoreBoard->IsVisible();
}

void DoDViewport::HideVGUIMenu( int menu )
{
	if ( menu >= 10 && menu <= 14 )
	{
		if ( m_pClassMenu )
			m_pClassMenu->SetVisible( false );
	}
	else if ( menu == MENU_TEAM )
		m_pTeamMenu->SetVisible( false );
	else if ( menu == 15 )
		m_pMortarPanel->HideMortarPanel();
	else
		TeamFortressViewport::HideVGUIMenu( menu );
}

void DoDViewport::DisplayVGUIMenu( int menu )
{
	if ( gEngfuncs.IsSpectateOnly() )
		return;
	((Panel*)m_pBackGround)->SetVisible( true );
	if ( menu == MENU_TEAM )
	{
		m_pTeamMenu->Update( GetMapName(), m_iAllowSpectators != 0, (const char**)m_sTeamNames, m_iNumTeams );
		m_pTeamMenu->Activate();
	}
	else if ( menu >= 10 && menu <= 14 )
	{
		if ( m_pClassMenu )
			m_pClassMenu->MarkForDeletion();
		g_lastPanel = NULL;
		m_pClassMenu = new CDoDClassMenu( NULL );
		m_pClassMenu->SetParent( GetViewPortPanel() );
		m_pClassMenu->SetClassMask( m_iClassMask );
		m_pClassMenu->Update( NULL, menu );
		m_pClassMenu->Activate();
	}
	else if ( menu == 15 )
		m_pMortarPanel->ShowMortarPanel();
	else
	{
		if ( menu == MENU_INTRO )
			gHUD.m_iIntermission = 0;
		TeamFortressViewport::DisplayVGUIMenu( menu );
	}
}

int DoDViewport::MsgFunc_ForceCam( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iForceCamera = READ_BYTE();
	m_iForceChaseCam = READ_BYTE();
	m_iFadeToBlack = READ_BYTE();
	return 1;
}

int DoDViewport::MsgFunc_WaveStatus( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	gHUD.m_ObjectiveIcons.m_iWaveStatus = READ_BYTE();
	UpdateSpectatorPanel();
	return 1;
}

int DoDViewport::MsgFunc_WaveTime( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int seconds = READ_CHAR();
	gHUD.m_ObjectiveIcons.m_flWaveTime = seconds > 0 ? seconds + gHUD.m_flTime : 0;
	return 1;
}

int DoDViewport::MsgFunc_MapMarker( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	READ_BYTE();
	READ_SHORT();
	READ_SHORT();
	gEngfuncs.GetLocalPlayer();
	gEngfuncs.GetEntityByIndex( player );
	return 1;
}

int DoDViewport::MsgFunc_TeamScore( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int team = READ_BYTE();
	for ( int index = 1; index <= 64; index++ )
	{
		if ( m_pClientScoreBoard->GetPlayerTeamInfo( index ).teamnumber == team )
		{
			m_pPrivateClientScoreBoard->SetExtraScoreBoardInfo( index, READ_SHORT() );
			m_pSpectatorGUI->UpdateSpectatorPlayerList();
			return 1;
		}
	}
	READ_SHORT();
	return 1;
}

int DoDViewport::MsgFunc_WideScreen( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int visible = READ_BYTE();
	if ( m_pWidescreenPanel )
	{
		m_pWidescreenPanel->SetVisible( visible != 0 );
		if ( visible )
			m_pWidescreenPanel->Update();
	}
	return 1;
}

int DoDViewport::MsgFunc_VGUIMenu( const char* name, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int menu = READ_BYTE();
	if ( menu == MENU_MAPBRIEFING )
		return TeamFortressViewport::MsgFunc_VGUIMenu( name, size, buffer );
	int value = READ_LONG();
	if ( menu >= 10 && menu <= 14 )
		m_iClassMask = value;
	if ( menu == 15 && !value )
		HideVGUIMenu( menu );
	else
		ShowVGUIMenu( menu );
	return 1;
}

int DoDViewport::MsgFunc_CurMarker( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	SetMapMarker( READ_BYTE() );
	return 1;
}

void DoDViewport::SetMapMarker( int marker )
{
	if ( (unsigned int)marker <= 14 )
		gHUD.m_Icons.DrawMarkerIcon( gHUD.m_Spectator.GetMarkerSPR( marker ) );
}

HSPRITE DoDViewport::GetMapMarkerSpr()
{
	return 0;
}

int DoDViewport::MsgFunc_ScoreInfo( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	int score = READ_BYTE();
	int frags = READ_BYTE();
	int deaths = READ_BYTE();
	int playerClass = READ_BYTE();
	int team = READ_BYTE();
	int status = READ_BYTE();
	if ( player >= 1 && player <= 64 )
	{
		extra_player_info_t& info = g_PlayerExtraInfo[player];
		info.objscore = (short)score;
		info.frags = (short)frags;
		info.deaths = (short)deaths;
		info.playerclass = (short)playerClass;
		info.teamnumber = (short)team;
		strncpy( info.teamname, gHUD.GetTeamName( team == 1 ? 1 : team == 2 ? 2 : 3 ), 16 );
		info.status = status;
		info.dead = status == 1;
		if ( info.teamnumber < 0 )
			info.teamnumber = 0;
		UpdateOnPlayerInfo();
	}
	return 1;
}

int DoDViewport::MsgFunc_ScoreInfoLong( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	int score = READ_SHORT();
	int frags = READ_SHORT();
	int deaths = READ_SHORT();
	int playerClass = READ_BYTE();
	int team = READ_BYTE();
	int status = READ_BYTE();
	if ( player >= 1 && player <= 64 )
	{
		extra_player_info_t& info = g_PlayerExtraInfo[player];
		info.objscore = (short)score;
		info.frags = (short)frags;
		info.deaths = (short)deaths;
		info.playerclass = (short)playerClass;
		info.teamnumber = (short)team;
		strncpy( info.teamname, gHUD.GetTeamName( team == 1 ? 1 : team == 2 ? 2 : 3 ), 16 );
		info.status = status;
		info.dead = status == 1;
		if ( info.teamnumber < 0 )
			info.teamnumber = 0;
		UpdateOnPlayerInfo();
	}
	return 1;
}

int DoDViewport::MsgFunc_ScoreShort( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	short score = (short)READ_SHORT();
	short frags = (short)READ_SHORT();
	short deaths = (short)READ_SHORT();
	int status = READ_BYTE();
	if ( player >= 1 && player <= 64 )
	{
		extra_player_info_t& info = g_PlayerExtraInfo[player];
		info.objscore = score;
		info.frags = frags;
		info.deaths = deaths;
		info.status = status;
		info.dead = status == 1;
		UpdateOnPlayerInfo();
	}
	return 1;
}

int DoDViewport::MsgFunc_Frags( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	short frags = (short)READ_SHORT();
	if ( player >= 1 && player <= 64 )
	{
		g_PlayerExtraInfo[player].frags = frags;
		UpdateOnPlayerInfo();
	}
	return 1;
}

int DoDViewport::MsgFunc_ObjScore( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	short score = (short)READ_SHORT();
	if ( player >= 1 && player <= 64 )
	{
		g_PlayerExtraInfo[player].objscore = score;
		UpdateOnPlayerInfo();
	}
	return 1;
}

int DoDViewport::MsgFunc_PStatus( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	int status = READ_BYTE();
	if ( player >= 1 && player <= 64 )
	{
		g_PlayerExtraInfo[player].status = status;
		g_PlayerExtraInfo[player].dead = status == 1;
		UpdateOnPlayerInfo();
	}
	return 1;
}

int DoDViewport::MsgFunc_PTeam( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	int team = READ_BYTE();
	if ( player >= 1 && player <= 64 )
	{
		g_PlayerExtraInfo[player].teamnumber = (short)team;
		if ( g_PlayerExtraInfo[player].teamnumber < 0 )
			g_PlayerExtraInfo[player].teamnumber = 0;
		strncpy( g_PlayerExtraInfo[player].teamname, gHUD.GetTeamName( team == 1 ? 1 : team == 2 ? 2 : 3 ), 16 );
		UpdateOnPlayerInfo();
	}
	return 1;
}

int DoDViewport::MsgFunc_PClass( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int player = READ_BYTE();
	short playerClass = (short)READ_BYTE();
	if ( player >= 1 && player <= 64 )
	{
		g_PlayerExtraInfo[player].playerclass = playerClass;
		UpdateOnPlayerInfo();
	}
	return 1;
}

EXPOSE_SINGLE_INTERFACE( DoDViewport, IClientVGUI, CLIENTVGUI_INTERFACE_VERSION );
