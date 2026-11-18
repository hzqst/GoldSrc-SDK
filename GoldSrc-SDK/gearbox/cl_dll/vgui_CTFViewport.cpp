#include "hud.h"
#include "cl_util.h"
#include "vgui_CTFViewport.h"
#include "vgui_StatsMenu.h"
#include "vgui_ScorePanel.h"
#include "vgui_ServerBrowser.h"
#include "vgui_int.h"
#include "voice_status.h"
#include "parsemsg.h"
#include "demo_api.h"
#include "keydefs.h"
#include <VGUI_Cursor.h>

extern int g_iVisibleMouse;
void IN_ResetMouse();
extern CMenuPanel* CMessageWindowPanel_Create( const char*, const char*, int, int, int, int, int, int );

int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1;
int g_iUser2;
int g_iUser3;

int iTeamColors[5][3] =
{
	{ 255, 255, 255 },
	{ 66, 115, 247 },
	{ 220, 51, 38 },
	{ 240, 135, 0 },
	{ 115, 240, 115 }
};

int iBuildingCosts[2] = { 100, 130 };
int sTFValidClassInts[11] = { 0, 1, 2, 4, 8, 16, 32, 64, 256, 512, 128 };
char* sCTFClassSelection[2][7] =
{
	{ "Barney", "Cleansuit", "Gina", "Gordon", "Otis", "Scientist", "Civ_Random" },
	{ "SquadLeader", "DrillSgt", "Grunt", "Recruit", "Adrian", "Tower", "Op4_Random" }
};
char* sLocalisedClasses[2][7] =
{
	{ "#Barney", "#Cleansuit", "#Gina", "#Gordon", "#Otis", "#Scientist", "#Civ_Random" },
	{ "#SquadLeader", "#DrillSgt", "#Grunt", "#Recruit", "#Adrian", "#Tower", "#Op4_Random" }
};

char* GetVGUITGAName( const char* name )
{
	char image[256];
	static char path[256];
	sprintf( image, name, ScreenWidth < 640 ? 320 : 640 );
	sprintf( path, "%s/gfx/vgui/%s.tga", gEngfuncs.pfnGetGameDirectory(), image );
	return path;
}

class CViewPortInputHandler : public InputSignal
{
public:
	bool bPressed;
	void cursorMoved( int, int, Panel* ) override {}
	void cursorEntered( Panel* ) override {}
	void cursorExited( Panel* ) override {}
	void mousePressed( MouseCode code, Panel* ) override
	{
		if ( code != MOUSE_LEFT )
			gEngfuncs.pfnClientCmd( "ForceCloseCommandMenu\n" );
	}
	void mouseReleased( MouseCode, Panel* ) override {}
	void mouseDoublePressed( MouseCode, Panel* ) override {}
	void mouseWheeled( int, Panel* ) override {}
	void keyPressed( KeyCode, Panel* ) override {}
	void keyTyped( KeyCode, Panel* ) override {}
	void keyReleased( KeyCode, Panel* ) override {}
	void keyFocusTicked( Panel* ) override {}
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

void* CTFViewport::operator new( size_t size )
{
	void* memory = ::operator new( size );
	memset( memory, 0, size );
	return memory;
}

CTFViewport::CTFViewport( int x, int y, int wide, int tall ) :
	Panel( x, y, wide, tall ), m_SchemeManager( wide, tall )
{
	m_pClassMenu = NULL;
	m_pScoreBoard = NULL;
	m_pSpectatorMenu = NULL;
	m_pCurrentMenu = NULL;
	m_pCurrentCommandMenu = NULL;
	m_iInitialized = 0;
	m_pTeamMenu = NULL;
	m_pStatsMenu = NULL;
	gViewPort = this;
	gEngfuncs.pfnRegisterVariable( "hud_classautokill", "1", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "hud_takesshots", "0", FCVAR_ARCHIVE );
	Initialize();
	addInputSignal( new CViewPortInputHandler );

	Scheme* scheme = App::getInstance()->getScheme();
	SchemeHandle_t handle = m_SchemeManager.getSchemeHandle( "Primary Button Text" );
	scheme->setFont( Scheme::sf_primary1, m_SchemeManager.getFont( handle ) );
	int r, g, b, a;
	m_SchemeManager.getFgColor( handle, r, g, b, a );
	scheme->setColor( Scheme::sc_primary1, r, g, b, a );
	m_SchemeManager.getBgColor( handle, r, g, b, a );
	scheme->setColor( Scheme::sc_primary3, r, g, b, a );
	m_SchemeManager.getFgArmedColor( handle, r, g, b, a );
	scheme->setColor( Scheme::sc_secondary2, r, g, b, a );
	m_SchemeManager.getBgArmedColor( handle, r, g, b, a );
	scheme->setColor( Scheme::sc_primary2, r, g, b, a );
	m_SchemeManager.getBorderColor( handle, r, g, b, a );
	scheme->setColor( Scheme::sc_secondary1, 178, 118, 0, 0 );
	handle = m_SchemeManager.getSchemeHandle( "Scoreboard Text" );
	scheme->setFont( Scheme::sf_primary2, m_SchemeManager.getFont( handle ) );
	handle = m_SchemeManager.getSchemeHandle( "CommandMenu Text" );
	scheme->setFont( Scheme::sf_primary3, m_SchemeManager.getFont( handle ) );
	App::getInstance()->setScheme( scheme );

	CreateTeamMenu();
	CreateClassMenu();
	CreateCommandMenu();
	CreateServerBrowser();
	CreateSpectatorMenu();
	CreateStatsMenu();
}

void CTFViewport::Initialize()
{
	if ( m_pTeamMenu )
		m_pTeamMenu->Initialize();
	if ( m_pClassMenu )
		m_pClassMenu->Initialize();
	if ( m_pScoreBoard )
		m_pScoreBoard->Initialize();
	if ( m_pSpectatorMenu )
		m_pSpectatorMenu->setVisible( false );
	if ( m_pStatsMenu )
		m_pStatsMenu->Initialize();
	while ( m_pCurrentMenu )
		HideTopMenu();
	HideCommandMenu();
	m_iGotAllMOTD = 1;
	m_iRandomPC = 0;
	g_iPlayerClass = 0;
	g_iTeamNumber = 0;
	memset( m_iValidClasses, 0, sizeof( m_iValidClasses ) );
	m_flScoreBoardLastUpdated = 0;
	m_sMapName[0] = 0;
	m_szServerName[0] = 0;
	for ( int i = 0; i < 5; i++ )
		m_sTeamNames[i][0] = 0;
	App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor( Scheme::scu_none ) );
}

void CTFViewport::CreateTeamMenu()
{
	m_pTeamMenu = new CTeamMenuPanel( 100, 0, 0, 0, ScreenWidth, ScreenHeight );
	m_pTeamMenu->setParent( this );
	m_pTeamMenu->setVisible( false );
}

void CTFViewport::CreateClassMenu()
{
	m_pClassMenu = new CClassMenuPanel( 100, 0, 0, 0, ScreenWidth, ScreenHeight );
	m_pClassMenu->setParent( this );
	m_pClassMenu->setVisible( false );
}

void CTFViewport::CreateStatsMenu()
{
	m_pStatsMenu = new CStatsMenuPanel( 100, 0, 0, 0, ScreenWidth, ScreenHeight );
	m_pStatsMenu->setParent( this );
	m_pStatsMenu->setVisible( false );
}

void CTFViewport::CreateScoreBoard()
{
	int x = gHUD.m_Teamplay == 2 ? XRES( 52 ) : XRES( 104 );
	int y = YRES( 40 );
	m_pScoreBoard = new ScorePanel( 100, x, y, ScreenWidth - 2 * x, ScreenHeight - 2 * y );
	m_pScoreBoard->setParent( this );
	m_pScoreBoard->setVisible( false );
}

void CTFViewport::CreateServerBrowser()
{
	m_pServerBrowser = new ServerBrowser( 0, 0, ScreenWidth, ScreenHeight );
	m_pServerBrowser->setParent( this );
	m_pServerBrowser->setVisible( false );
}

CMenuPanel* CTFViewport::ShowTeamMenu()
{
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		return NULL;
	m_pTeamMenu->Reset();
	return m_pTeamMenu;
}

CMenuPanel* CTFViewport::ShowClassMenu()
{
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		return NULL;
	m_pClassMenu->Reset();
	return m_pClassMenu;
}

CMenuPanel* CTFViewport::ShowStatsMenu()
{
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		return NULL;
	m_pStatsMenu->Reset();
	m_pStatsMenu->m_pSaveButton->setVisible( true );
	return m_pStatsMenu;
}

CMenuPanel* CTFViewport::ShowScoreBoard()
{
	if ( gEngfuncs.GetMaxClients() <= 1 || gEngfuncs.pDemoAPI->IsPlayingback() )
		return NULL;
	m_pScoreBoard->Reset();
	return m_pScoreBoard;
}

bool CTFViewport::IsScoreBoardVisible()
{
	return m_pScoreBoard && m_pScoreBoard->isVisible();
}

void CTFViewport::SwitchToScoreBoard()
{
	m_pStatsMenu->setVisible( false );
	m_pScoreBoard->Update();
	m_pScoreBoard->setVisible( true );
	g_iVisibleMouse = 1;
	App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor( Scheme::scu_arrow ) );
}

void CTFViewport::SwitchToStatsMenu()
{
	m_pScoreBoard->setVisible( false );
	m_pStatsMenu->Update();
	m_pStatsMenu->setVisible( true );
	g_iVisibleMouse = 1;
	App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor( Scheme::scu_arrow ) );
}

void CTFViewport::HideScoreStatsWindow()
{
	if ( m_pScoreBoard && m_pScoreBoard->isVisible() )
		m_pScoreBoard->setVisible( false );
	if ( m_pStatsMenu && m_pStatsMenu->isVisible() )
		m_pStatsMenu->setVisible( false );
	IN_ResetMouse();
	g_iVisibleMouse = 0;
	App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor( Scheme::scu_none ) );
}

void CTFViewport::SaveStatsMenu()
{
	m_pStatsMenu->SaveStats();
}

void CTFViewport::HideScoreBoard()
{
	if ( !gHUD.m_iIntermission && m_pScoreBoard )
	{
		m_pScoreBoard->setVisible( false );
		GetClientVoiceMgr()->StopSquelchMode();
		UpdateCursorState();
	}
}

void CTFViewport::SetCurrentMenu( CMenuPanel* menu )
{
	m_pCurrentMenu = menu;
	if ( menu && !gEngfuncs.pDemoAPI->IsPlayingback() )
		menu->Open();
}

void CTFViewport::HideTopMenu()
{
	if ( m_pCurrentMenu )
	{
		m_pCurrentMenu->Close();
		gViewPort->SetCurrentMenu( m_pCurrentMenu->GetNextMenu() );
	}
	UpdateCursorState();
}

void CTFViewport::HideVGUIMenu()
{
	while ( m_pCurrentMenu )
		HideTopMenu();
}

void CTFViewport::ShowVGUIMenu( int menuID )
{
	if ( gEngfuncs.IsSpectateOnly() || gEngfuncs.pDemoAPI->IsPlayingback() )
		return;
	if ( menuID >= MENU_MAPBRIEFING && menuID <= MENU_CLASSHELP )
	{
		for ( CMenuPanel* menu = m_pCurrentMenu; menu; menu = menu->GetNextMenu() )
		{
			if ( menu->GetMenuID() == menuID )
				return;
		}
	}

	CMenuPanel* menu;
	switch ( menuID )
	{
	case MENU_TEAM: menu = ShowTeamMenu(); break;
	case MENU_CLASS: menu = ShowClassMenu(); break;
	case MENU_MAPBRIEFING: menu = CreateTextWindow( 1 ); break;
	case MENU_INTRO: menu = CreateTextWindow( 3 ); break;
	case MENU_CLASSHELP: menu = CreateTextWindow( 2 ); break;
	case 9:
		SwitchToStatsMenu();
		m_pStatsMenu->m_pSaveButton->setVisible( true );
		return;
	case 10:
		SwitchToScoreBoard();
		return;
	default: return;
	}
	if ( !menu )
		return;
	HideCommandMenu();
	menu->SetMenuID( menuID );
	menu->SetActive( true );
	if ( m_pCurrentMenu )
		m_pCurrentMenu->SetNextMenu( menu );
	else
	{
		m_pCurrentMenu = menu;
		menu->Open();
		UpdateCursorState();
	}
}

CMenuPanel* CTFViewport::CreateTextWindow( int textToShow )
{
	char filename[256];
	char title[32];
	char* text;
	char* file = NULL;
	if ( textToShow == 1 )
	{
		if ( m_sMapName[0] )
			sprintf( filename, "maps/%s.txt", m_sMapName );
		else
		{
			const char* level = gEngfuncs.pfnGetLevelName();
			if ( !level )
				return NULL;
			strcpy( filename, level );
			*strchr( filename, '.' ) = 0;
			strcat( filename, ".txt" );
			strcpy( m_sMapName, level );
			char* end = strchr( m_sMapName, '.' );
			if ( end )
				*end = 0;
			char* slash = strchr( m_sMapName, '/' );
			if ( slash )
				memmove( m_sMapName, slash + 1, strlen( slash ) + 1 );
		}
		file = (char*)gEngfuncs.COM_LoadFile( filename, 5, NULL );
		if ( !file )
			return NULL;
		text = file;
		strncpy( title, m_sMapName, sizeof( title ) );
		title[31] = 0;
	}
	else if ( textToShow == 2 )
	{
		const char* help;
		const char* heading;
		switch ( g_iPlayerClass )
		{
		case 1: help = "#Help_scout"; heading = "#Title_scout"; break;
		case 2: help = "#Help_sniper"; heading = "#Title_sniper"; break;
		case 3: help = "#Help_soldier"; heading = "#Title_soldier"; break;
		case 4: help = "#Help_demoman"; heading = "#Title_demoman"; break;
		case 5: help = "#Help_medic"; heading = "#Title_medic"; break;
		case 6: help = "#Help_hwguy"; heading = "#Title_hwguy"; break;
		case 7: help = "#Help_pyro"; heading = "#Title_pyro"; break;
		case 8: help = "#Help_spy"; heading = "#Title_spy"; break;
		case 9: help = "#Help_engineer"; heading = "#Title_engineer"; break;
		case 11: help = "#Help_civilian"; heading = "#Title_civilian"; break;
		default: return NULL;
		}
		text = CHudTextMessage::BufferedLocaliseTextString( help );
		CHudTextMessage::LocaliseTextString( heading, title, sizeof( title ) );
		char* description = (char*)gEngfuncs.COM_LoadFile( "motd.txt", 5, NULL );
		if ( description )
			text = description;
	}
	else
	{
		if ( m_szServerName[0] )
			strncpy( title, m_szServerName, sizeof( title ) );
		else
			strcpy( title, "Half-Life" );
		title[31] = 0;
		text = m_szMOTD;
	}
	CMenuPanel* menu = CMessageWindowPanel_Create( text, title, g_iPlayerClass == 0,
		0, 0, 0, ScreenWidth, ScreenHeight );
	menu->setParent( this );
	if ( file )
		gEngfuncs.COM_FreeFile( file );
	return menu;
}

bool CTFViewport::AllowedToPrintText()
{
	if ( !m_pCurrentMenu || g_iPlayerClass )
		return true;
	int id = m_pCurrentMenu->GetMenuID();
	return id != 2 && id != 3 && id != 5 && id != 6;
}

void CTFViewport::CreateSpectatorMenu()
{
	m_pSpectatorMenu = new CTransparentPanel( 100, 0, ScreenHeight - YRES( 60 ), ScreenWidth, YRES( 60 ) );
	m_pSpectatorMenu->setParent( this );
	m_pSpectatorMenu->setVisible( false );
	SchemeHandle_t titleFont = m_SchemeManager.getSchemeHandle( "Title Font" );
	SchemeHandle_t helpFont = m_SchemeManager.getSchemeHandle( "Primary Button Text" );
	m_pSpectatorLabel = new Label( "Spectator", 0, 0, ScreenWidth, YRES( 25 ) );
	m_pSpectatorLabel->setParent( m_pSpectatorMenu );
	m_pSpectatorLabel->setFont( m_SchemeManager.getFont( titleFont ) );
	int r, g, b, a;
	m_SchemeManager.getFgColor( titleFont, r, g, b, a );
	m_pSpectatorLabel->setFgColor( r, g, b, a );
	m_SchemeManager.getBgColor( titleFont, r, g, b, a );
	m_pSpectatorLabel->setBgColor( r, g, b, 255 );
	m_pSpectatorLabel->setContentAlignment( Label::a_north );
	Label* help = new Label( CHudTextMessage::BufferedLocaliseTextString( "#Spec_Help" ),
		0, YRES( 25 ), ScreenWidth, YRES( 15 ) );
	help->setParent( m_pSpectatorMenu );
	help->setFont( m_SchemeManager.getFont( helpFont ) );
	m_SchemeManager.getFgColor( helpFont, r, g, b, a );
	help->setFgColor( r, g, b, a );
	m_SchemeManager.getBgColor( helpFont, r, g, b, a );
	help->setBgColor( r, g, b, 255 );
	help->setContentAlignment( Label::a_north );
	Label* help2 = new Label( CHudTextMessage::BufferedLocaliseTextString( "#Spec_Help2" ),
		0, YRES( 40 ), ScreenWidth, YRES( 20 ) );
	help2->setParent( m_pSpectatorMenu );
	help2->setFont( m_SchemeManager.getFont( helpFont ) );
	m_SchemeManager.getFgColor( helpFont, r, g, b, a );
	help2->setFgColor( r, g, b, a );
	m_SchemeManager.getBgColor( helpFont, r, g, b, a );
	help2->setBgColor( r, g, b, 255 );
	help2->setContentAlignment( Label::a_center );
}

void CTFViewport::UpdateSpectatorMenu()
{
	m_iUser1 = g_iUser1;
	m_iUser2 = g_iUser2;
	m_iUser3 = g_iUser3;
	if ( !m_pSpectatorMenu )
		return;
	char help[128];
	int mode;
	if ( gEngfuncs.IsSpectateOnly() )
	{
		mode = gHUD.m_Spectator.m_iMainMode;
		strcpy( help, "#Spec_Only_Help" );
	}
	else
	{
		mode = m_iUser1;
		strcpy( help, "#Spec_Help" );
	}
	if ( !mode || gEngfuncs.IsSpectateOnly() == 2 )
	{
		m_pSpectatorMenu->setVisible( false );
		return;
	}
	m_pSpectatorMenu->setVisible( true );
	char label[128];
	if ( m_iUser2 > 0 && m_iUser2 <= gEngfuncs.GetMaxClients() && g_PlayerInfoList[m_iUser2].name )
		sprintf( label, "#Spec_Mode%d : %s", mode, g_PlayerInfoList[m_iUser2].name );
	else
		sprintf( label, "#Spec_Mode%d", mode );
	if ( m_iUser1 == 3 )
	{
		char directed[128];
		sprintf( directed, "#Directed %s", label );
		strcpy( label, directed );
	}
	m_pSpectatorLabel->setText( CHudTextMessage::BufferedLocaliseTextString( label ) );
	m_pSpectatorHelpLabel->setText( CHudTextMessage::BufferedLocaliseTextString( help ) );
}

void CTFViewport::UpdateOnPlayerInfo()
{
	if ( m_pTeamMenu )
		m_pTeamMenu->Update();
	if ( m_pClassMenu )
		m_pClassMenu->Update();
	if ( m_pScoreBoard && !gHUD.m_iIntermission )
		m_pScoreBoard->Update();
}

void CTFViewport::UpdateCursorState()
{
	if ( m_pCurrentMenu || ( m_pTeamMenu && m_pTeamMenu->isVisible() ) ||
		( m_pServerBrowser && m_pServerBrowser->isVisible() ) || GetClientVoiceMgr()->IsInSquelchMode() ||
		( m_pCurrentCommandMenu && gHUD.m_pCvarStealMouse->value ) )
	{
		g_iVisibleMouse = 1;
		App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor( Scheme::scu_arrow ) );
	}
	else
	{
		IN_ResetMouse();
		g_iVisibleMouse = 0;
		App::getInstance()->setCursorOveride( App::getInstance()->getScheme()->getCursor( Scheme::scu_none ) );
	}
}

void CTFViewport::ToggleServerBrowser()
{
	if ( m_iInitialized && m_pServerBrowser )
	{
		m_pServerBrowser->setVisible( !m_pServerBrowser->isVisible() );
		UpdateCursorState();
	}
}

void CTFViewport::SetCurrentCommandMenu( CCommandMenu* menu )
{
	for ( int i = 0; i < m_iNumMenus; i++ )
		m_pCommandMenus[i]->setVisible( false );
	m_pCurrentCommandMenu = menu;
	for ( ; menu; menu = menu->GetParentMenu() )
		menu->setVisible( true );
}

void CTFViewport::UpdateCommandMenu()
{
	m_pCommandMenus[0]->RecalculateVisibles( 0, false );
	m_pCommandMenus[0]->RecalculatePositions( 0 );
}

void CTFViewport::HideCommandMenu()
{
	if ( !m_iInitialized )
		return;
	if ( m_pCommandMenus[0] )
		m_pCommandMenus[0]->ClearButtonsOfArmedState();
	m_flMenuOpenTime = 0;
	for ( int i = 0; i < m_iNumMenus; i++ )
		m_pCommandMenus[i]->setVisible( false );
	m_pCurrentCommandMenu = NULL;
	UpdateCursorState();
}

void CTFViewport::InputSignalHideCommandMenu()
{
	if ( m_iInitialized && m_flMenuOpenTime + 0.3 <= gHUD.m_flTime )
		HideCommandMenu();
}

void CTFViewport::ShowCommandMenu()
{
	if ( !m_iInitialized || !g_iPlayerClass )
		return;
	if ( m_pCurrentCommandMenu )
	{
		HideCommandMenu();
		return;
	}
	if ( gHUD.m_iIntermission )
		return;
	UpdateCommandMenu();
	while ( m_pCurrentMenu )
		HideTopMenu();
	SetCurrentCommandMenu( m_pCommandMenus[0] );
	m_flMenuOpenTime = gHUD.m_flTime;
	UpdateCursorState();
	for ( int i = 2; i < gEngfuncs.Cmd_Argc(); i++ )
	{
		const char* key = gEngfuncs.Cmd_Argv( i - 1 );
		if ( key && m_pCurrentCommandMenu->KeyInput( key[0] ) )
			HideCommandMenu();
	}
}

void CTFViewport::InputPlayerSpecial()
{
	if ( !m_iInitialized )
		return;
	if ( g_iPlayerClass == 8 || g_iPlayerClass == 9 )
	{
		ShowCommandMenu();
		if ( m_pCurrentCommandMenu )
			m_pCurrentCommandMenu->KeyInput( '7' );
	}
	else
		gEngfuncs.pfnClientCmd( "_special" );
}

void CTFViewport::UpdateHighlights()
{
	for ( CCommandMenu* menu = m_pCurrentCommandMenu; menu; menu = menu->GetParentMenu() )
		menu->setVisible( true );
}

void CTFViewport::GetAllPlayersInfo()
{
	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		gEngfuncs.pfnGetPlayerInfo( i, &g_PlayerInfoList[i] );
		if ( m_pScoreBoard && g_PlayerInfoList[i].thisplayer )
			m_pScoreBoard->m_iPlayerNum = i;
	}
}

void CTFViewport::paintBackground()
{
	if ( m_iCurrentTeamNumber != g_iTeamNumber )
	{
		UpdateCommandMenu();
		if ( m_pClassMenu )
			m_pClassMenu->Update();
		m_iCurrentTeamNumber = g_iTeamNumber;
	}
	if ( m_iCurrentPlayerClass != g_iPlayerClass )
	{
		UpdateCommandMenu();
		m_iCurrentPlayerClass = g_iPlayerClass;
	}
	if ( ( m_iUser1 != g_iUser1 || m_iUser2 != g_iUser2 ) && gEngfuncs.IsSpectateOnly() )
		UpdateSpectatorMenu();
	if ( m_pScoreBoard && m_pScoreBoard->isVisible() && gHUD.m_flTime > m_flScoreBoardLastUpdated )
	{
		m_pScoreBoard->Update();
		m_flScoreBoardLastUpdated = gHUD.m_flTime + 0.5f;
	}
	int extents[4];
	getAbsExtents( extents[0], extents[1], extents[2], extents[3] );
	VGui_ViewportPaintBackground( extents );
}

bool CTFViewport::SlotInput( int slot )
{
	return m_pCurrentMenu && m_pCurrentMenu->SlotInput( slot );
}

int CTFViewport::KeyInput( int down, int key, const char* binding )
{
	if ( m_iUser1 && !gEngfuncs.Con_IsVisible() && down && ( key == K_KP_ENTER || key == K_ENTER ) &&
		!gEngfuncs.IsSpectateOnly() && !gEngfuncs.pDemoAPI->IsPlayingback() )
		ShowVGUIMenu( MENU_TEAM );
	if ( m_pCurrentMenu && !gEngfuncs.Con_IsVisible() )
	{
		int menuID = m_pCurrentMenu->GetMenuID();
		if ( menuID == MENU_TEAM || menuID == MENU_CLASS )
		{
			if ( key == K_ESCAPE && ( menuID == MENU_TEAM ? g_iTeamNumber : g_iPlayerClass ) )
			{
				HideTopMenu();
				return 0;
			}
			if ( down && key >= '0' && key <= '9' )
			{
				m_pCurrentMenu->SlotInput( key - '0' );
				return 0;
			}
		}
		if ( !down )
			return 1;
		if ( ( key == K_KP_ENTER || key == K_ENTER || key == K_ESCAPE || key == K_SPACE ) &&
			menuID >= MENU_MAPBRIEFING && menuID <= MENU_CLASSHELP )
		{
			HideTopMenu();
			return 0;
		}
		if ( binding && !strcmp( binding, "+jump" ) && menuID == MENU_TEAM )
		{
			m_pTeamMenu->SlotInput( 5 );
			return 0;
		}
	}
	if ( down && m_pCurrentCommandMenu )
	{
		if ( key == K_ESCAPE )
		{
			HideCommandMenu();
			return 0;
		}
		if ( key >= '0' && key <= '9' )
		{
			if ( m_pCurrentCommandMenu->KeyInput( key ) )
				HideCommandMenu();
			return 0;
		}
	}
	return 1;
}

int CTFViewport::MsgFunc_ValClass( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	for ( int i = 0; i < 5; i++ )
		m_iValidClasses[i] = READ_SHORT();
	UpdateCommandMenu();
	return 1;
}

int CTFViewport::MsgFunc_TeamNames( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iNumberOfTeams = READ_BYTE();
	for ( int i = 1; i <= m_iNumberOfTeams; i++ )
	{
		CHudTextMessage::LocaliseTextString( READ_STRING(), m_sTeamNames[i], MAX_TEAMNAME_SIZE );
		if ( m_pTeamButtons[i - 1] )
			m_pTeamButtons[i - 1]->setText( m_sTeamNames[i] );
		if ( m_pDisguiseButtons[i - 1] )
			m_pDisguiseButtons[i - 1]->setText( m_sTeamNames[i] );
	}
	if ( m_pTeamMenu )
		m_pTeamMenu->Update();
	return 1;
}

int CTFViewport::MsgFunc_Feign( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iIsFeigning = READ_BYTE();
	UpdateCommandMenu();
	return 1;
}

int CTFViewport::MsgFunc_Detpack( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iIsSettingDetpack = READ_BYTE();
	UpdateCommandMenu();
	return 1;
}

int CTFViewport::MsgFunc_VGUIMenu( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int menu = READ_BYTE();
	if ( menu == MENU_MAPBRIEFING )
	{
		strncpy( m_sMapName, READ_STRING(), sizeof( m_sMapName ) );
		m_sMapName[63] = 0;
	}
	ShowVGUIMenu( menu );
	return 1;
}

int CTFViewport::MsgFunc_MOTD( const char*, int size, void* buffer )
{
	if ( m_iGotAllMOTD )
		m_szMOTD[0] = 0;
	BEGIN_READ( buffer, size );
	m_iGotAllMOTD = READ_BYTE();
	strncat( m_szMOTD, READ_STRING(), MAX_MOTD_LENGTH - strlen( m_szMOTD ) );
	m_szMOTD[MAX_MOTD_LENGTH - 1] = 0;
	if ( m_iGotAllMOTD && !gEngfuncs.IsSpectateOnly() && !gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		CMenuPanel* menu = m_pCurrentMenu;
		while ( menu && menu->GetMenuID() != MENU_INTRO )
			menu = menu->GetNextMenu();
		if ( !menu )
			ShowVGUIMenu( MENU_INTRO );
	}
	return 1;
}

int CTFViewport::MsgFunc_BuildSt( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iBuildState = READ_BYTE();
	UpdateCommandMenu();
	return 1;
}

int CTFViewport::MsgFunc_RandomPC( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iRandomPC = READ_BYTE();
	return 1;
}

int CTFViewport::MsgFunc_ServerName( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	strncpy( m_szServerName, READ_STRING(), sizeof( m_szServerName ) );
	m_szServerName[31] = 0;
	return 1;
}

int CTFViewport::MsgFunc_ScoreInfo( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	short player = (short)READ_BYTE();
	short frags = (short)READ_SHORT();
	short deaths = (short)READ_SHORT();
	short playerclass = (short)READ_SHORT();
	short team = (short)READ_SHORT();
	if ( player >= 1 && player <= MAX_PLAYERS )
	{
		g_PlayerExtraInfo[player].teamnumber = team;
		g_PlayerExtraInfo[player].deaths = deaths;
		g_PlayerExtraInfo[player].playerclass = playerclass;
		g_PlayerExtraInfo[player].frags = frags;
		UpdateOnPlayerInfo();
	}
	return 1;
}

int CTFViewport::MsgFunc_TeamScore( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	char* name = READ_STRING();
	if ( m_pScoreBoard )
	{
		for ( int i = 1; i <= m_pScoreBoard->m_iNumTeams; i++ )
		{
			if ( !stricmp( name, g_TeamInfo[i].name ) )
			{
				g_TeamInfo[i].scores_overriden = true;
				g_TeamInfo[i].frags = (short)READ_SHORT();
				g_TeamInfo[i].deaths = (short)READ_SHORT();
				break;
			}
		}
	}
	return 1;
}

int CTFViewport::MsgFunc_TeamInfo( const char*, int size, void* buffer )
{
	if ( m_pScoreBoard )
	{
		BEGIN_READ( buffer, size );
		short player = (short)READ_BYTE();
		if ( player >= 1 && player <= MAX_PLAYERS )
			strncpy( g_PlayerExtraInfo[player].teamname, READ_STRING(), MAX_TEAM_NAME );
		m_pScoreBoard->RebuildTeams();
	}
	return 1;
}

void CTFViewport::DeathMsg( int killer, int victim )
{
	if ( m_pScoreBoard )
		m_pScoreBoard->DeathMsg( killer, victim );
}

int CTFViewport::MsgFunc_Spectator( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	short player = (short)READ_BYTE();
	if ( player >= 1 && player <= MAX_PLAYERS )
		g_IsSpectator[player] = READ_BYTE();
	return 1;
}

int CTFViewport::MsgFunc_AllowSpec( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iAllowSpectators = READ_BYTE();
	UpdateCommandMenu();
	if ( m_pTeamMenu )
		m_pTeamMenu->Update();
	return 1;
}

int CTFViewport::MsgFunc_SetMenuTeam( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	m_iCTFTeamNumber = READ_BYTE();
	return 1;
}

int CTFViewport::MsgFunc_StatsInfo( const char* name, int size, void* buffer )
{
	return m_pStatsMenu->MsgFunc_StatsInfo( name, size, buffer );
}

int CTFViewport::MsgFunc_StatsPlayer( const char* name, int size, void* buffer )
{
	return m_pStatsMenu->MsgFunc_StatsPlayer( name, size, buffer );
}

int CTFViewport::MsgFunc_TeamFull( const char* name, int size, void* buffer )
{
	return m_pTeamMenu->MsgFunc_TeamFull( name, size, buffer );
}

void CDragNDropHandler::cursorMoved( int x, int y, Panel* )
{
	if ( m_bDragging )
	{
		App::getInstance()->getCursorPos( x, y );
		m_pPanel->setPos( m_iaDragOrgPos[0] + x - m_iaDragStart[0],
			m_iaDragOrgPos[1] + y - m_iaDragStart[1] );
		if ( m_pPanel->getParent() )
			m_pPanel->getParent()->repaint();
	}
}

void CDragNDropHandler::mousePressed( MouseCode, Panel* panel )
{
	App::getInstance()->getCursorPos( m_iaDragStart[0], m_iaDragStart[1] );
	m_bDragging = true;
	m_pPanel->getPos( m_iaDragOrgPos[0], m_iaDragOrgPos[1] );
	App::getInstance()->setMouseCapture( panel );
	m_pPanel->setDragged( m_bDragging );
	m_pPanel->requestFocus();
}

void CDragNDropHandler::mouseReleased( MouseCode, Panel* )
{
	m_bDragging = false;
	m_pPanel->setDragged( m_bDragging );
	App::getInstance()->setMouseCapture( NULL );
}

void CCommandMenu::AddButton( CommandButton* button )
{
	if ( m_iButtons >= MAX_BUTTONS )
		return;

	m_aButtons[m_iButtons++] = button;
	button->setParent( this );
	button->setFont( Scheme::sf_primary3 );
	if ( m_iButtons < 10 )
		button->setBoundKey( m_iButtons + '0' );
	else if ( m_iButtons == 10 )
		button->setBoundKey( '0' );
}

bool CCommandMenu::KeyInput( int keyNum )
{
	for ( int i = 0; i < m_iButtons; i++ )
	{
		CommandButton* button = m_aButtons[i];
		if ( button->IsNotValid() || button->getBoundKey() != keyNum )
			continue;

		if ( button->GetSubMenu() )
		{
			gViewPort->SetCurrentCommandMenu( button->GetSubMenu() );
			return false;
		}
		button->fireActionSignal();
		return true;
	}
	return false;
}

void CCommandMenu::ClearButtonsOfArmedState()
{
	for ( int i = 0; i < m_iButtons; i++ )
	{
		m_aButtons[i]->setArmed( false );
		if ( m_aButtons[i]->GetSubMenu() )
			m_aButtons[i]->GetSubMenu()->ClearButtonsOfArmedState();
	}
}

CommandButton* CCommandMenu::FindButtonWithSubmenu( CCommandMenu* submenu )
{
	for ( int i = 0; i < m_iButtons; i++ )
	{
		if ( m_aButtons[i]->GetSubMenu() == submenu )
			return m_aButtons[i];
	}
	return NULL;
}

bool CCommandMenu::RecalculateVisibles( int newY, bool hideAll )
{
	if ( newY )
		setPos( _pos[0], newY );

	int currentY = 0;
	bool hasButton = false;
	for ( int i = 0; i < m_iButtons; i++ )
	{
		CommandButton* button = m_aButtons[i];
		int playerClass = button->GetPlayerClass();
		if ( ( playerClass && playerClass != g_iPlayerClass ) || button->IsNotValid() || hideAll )
		{
			button->setVisible( false );
			if ( button->GetSubMenu() )
				button->GetSubMenu()->RecalculateVisibles( _pos[1] + currentY, true );
			continue;
		}

		if ( button->GetSubMenu() && !button->GetSubMenu()->RecalculateVisibles( _pos[1] + currentY, false ) )
		{
			button->setVisible( false );
			continue;
		}

		button->setVisible( true );
		int x, y;
		button->getPos( x, y );
		button->setPos( x, currentY );
		currentY += BUTTON_SIZE_Y - 1;
		hasButton = true;
	}
	setSize( _size[0], currentY + 1 );
	return hasButton;
}

void CCommandMenu::RecalculatePositions( int yOffset )
{
	int newY = _pos[1] + yOffset;
	int adjustment = 0;
	int bottom = newY + _size[1];
	if ( bottom > ScreenHeight )
	{
		while ( adjustment < bottom - ScreenHeight )
			adjustment += BUTTON_SIZE_Y - 1;
		newY -= adjustment;
		if ( newY < 0 )
		{
			adjustment += newY;
			newY = 0;
		}
	}
	for ( int i = 0; i < m_iButtons; i++ )
		m_aButtons[i]->UpdateSubMenus( adjustment );
	setPos( _pos[0], newY );
}

void CCommandMenu::MakeVisible( CCommandMenu* )
{
	setVisible( true );
	if ( m_pParentMenu )
		m_pParentMenu->MakeVisible( this );
}

CCommandMenu* CTFViewport::CreateSubMenu( CommandButton* button, CCommandMenu* parent )
{
	int x = 0;
	int y = 0;
	int wide = CMENU_SIZE_X;
	if ( parent )
	{
		x = parent->GetXOffset() + CMENU_SIZE_X - 1;
		y = parent->GetYOffset() + BUTTON_SIZE_Y * ( m_pCurrentCommandMenu->GetNumButtons() - 1 );
	}

	CCommandMenu* menu = new CCommandMenu( parent, x, y, wide, 0 );
	menu->setParent( this );
	button->AddSubMenu( menu );
	button->setFont( Scheme::sf_primary3 );
	InputSignal* popup = new CMenuHandler_PopupSubMenuInput( button, menu );
	button->addInputSignal( popup );

	CImageLabel* arrow = new CImageLabel( "arrow", CMENU_SIZE_X - SUBMENU_SIZE_X, SUBMENU_SIZE_Y );
	arrow->setParent( button );
	arrow->addInputSignal( new CMenuHandler_PopupSubMenuInput( button, menu ) );	// each panel owns its input signals
	arrow->getPos( x, y );
	arrow->setPos( CMENU_SIZE_X - arrow->getImageWide(), ( BUTTON_SIZE_Y - arrow->getImageTall() ) / 2 );
	if ( !button->m_bNoHighlight )
		arrow->addInputSignal( new CHandler_CommandButtonHighlight( button ) );
	return menu;
}

CCommandMenu* CTFViewport::CreateDisguiseSubmenu( CommandButton* button, CCommandMenu* parent, const char* commandText )
{
	CCommandMenu* menu = CreateSubMenu( button, parent );
	m_pCommandMenus[m_iNumMenus++] = menu;
	for ( int i = 1; i <= 9; i++ )
	{
		CommandButton* disguise = new CommandButton(
			CHudTextMessage::BufferedLocaliseTextString( sLocalisedClasses[gViewPort->m_iCTFTeamNumber - 1][i] ),
			0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		char command[256];
		sprintf( command, "%s %d", commandText, i );
		disguise->addActionSignal( new CMenuHandler_StringCommand( command ) );
		menu->AddButton( disguise );
	}
	return menu;
}

void CTFViewport::CreateCommandMenu()
{
	m_pCommandMenus[0] = new CCommandMenu( NULL, 0, CMENU_TOP, CMENU_SIZE_X, 300 );
	m_pCommandMenus[0]->setParent( this );
	m_pCommandMenus[0]->setVisible( false );
	m_iNumMenus = 1;
	m_iCurrentTeamNumber = m_iUser1 = m_iUser2 = m_iUser3 = 0;

	char token[1024];
	char* file = (char*)gEngfuncs.COM_LoadFile( "commandmenu.txt", 5, NULL );
	if ( !file )
	{
		SetCurrentCommandMenu( NULL );
		return;
	}

	CHudTextMessage::LocaliseTextString( "#DetpackSet_For5Seconds", m_sDetpackStrings[0], MAX_BUTTON_SIZE );
	CHudTextMessage::LocaliseTextString( "#DetpackSet_For20Seconds", m_sDetpackStrings[1], MAX_BUTTON_SIZE );
	CHudTextMessage::LocaliseTextString( "#DetpackSet_For50Seconds", m_sDetpackStrings[2], MAX_BUTTON_SIZE );
	m_pCurrentCommandMenu = m_pCommandMenus[0];
	char lastButtonText[32] = "file start";
	file = gEngfuncs.COM_ParseFile( file, token );
	while ( token[0] && m_iNumMenus < MAX_MENUS )
	{
		while ( token[0] != '}' && token[0] )
		{
			char text[32] = "";
			char boundKey[32] = "";
			char customName[32] = "";
			char command[128] = "";
			char map[MAX_MAPNAME] = "";
			bool custom = false;
			int teamOnly = 0;
			bool getExtraToken = true;
			CommandButton* button = NULL;
			if ( !m_pCurrentCommandMenu )
			{
				gEngfuncs.Con_Printf( "Error in Commandmenu.txt file after '%s'.\n", lastButtonText );
				m_iInitialized = false;
				return;
			}

			strncpy( customName, token, sizeof( customName ) );
			customName[sizeof( customName ) - 1] = '\0';
			if ( !strcmp( customName, "CUSTOM" ) )
			{
				custom = true;
				file = gEngfuncs.COM_ParseFile( file, token );
			}
			else if ( !strcmp( customName, "MAP" ) )
			{
				file = gEngfuncs.COM_ParseFile( file, token );
				strncpy( map, token, sizeof( map ) );
				map[sizeof( map ) - 1] = '\0';
				file = gEngfuncs.COM_ParseFile( file, token );
			}
			else if ( !strncmp( customName, "TEAM", 4 ) )
			{
				teamOnly = atoi( customName + 4 );
				file = gEngfuncs.COM_ParseFile( file, token );
			}

			strncpy( boundKey, token, sizeof( boundKey ) );
			text[sizeof( text ) - 1] = '\0';
			file = gEngfuncs.COM_ParseFile( file, token );
			strncpy( text, token, sizeof( text ) );
			text[sizeof( text ) - 1] = '\0';
			strcpy( lastButtonText, text );
			file = gEngfuncs.COM_ParseFile( file, token );
			strncpy( command, token, sizeof( command ) );
			command[sizeof( command ) - 1] = '\0';

			if ( custom )
			{
				button = CreateCustomButton( text, command );
				file = gEngfuncs.COM_ParseFile( file, token );
				if ( token[0] == '{' )
					strcpy( command, token );
				else
					getExtraToken = false;
			}
			else if ( map[0] )
			{
				button = new MapButton( map, text, 0,
					BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y );
			}
			else if ( teamOnly )
			{
				button = new TeamOnlyCommandButton( teamOnly, text, 0,
					BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y );
			}
			else
			{
				button = new CommandButton( 0, text, 0,
					BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(), CMENU_SIZE_X, BUTTON_SIZE_Y );
			}

			if ( button )
			{
				m_pCurrentCommandMenu->AddButton( button );
				button->setBoundKey( boundKey[0] );
				button->setParentMenu( m_pCurrentCommandMenu );
				button->setFont( Scheme::sf_primary3 );
			}
			if ( command[0] == '{' )
			{
				if ( m_iNumMenus >= MAX_MENUS )
				{
					gEngfuncs.Con_Printf( "Too many menus in commandmenu.txt past '%s'\n", lastButtonText );
				}
				else
				{
					m_pCommandMenus[m_iNumMenus] = CreateSubMenu( button, m_pCurrentCommandMenu );
					m_pCurrentCommandMenu = m_pCommandMenus[m_iNumMenus++];
				}
			}
			else if ( !custom )
			{
				button->addActionSignal( new CMenuHandler_StringCommand( command ) );
				button->addInputSignal( new CMenuHandler_PopupSubMenuInput( button, m_pCurrentCommandMenu ) );
			}
			if ( getExtraToken )
				file = gEngfuncs.COM_ParseFile( file, token );
		}
		m_pCurrentCommandMenu = m_pCurrentCommandMenu->GetParentMenu();
		file = gEngfuncs.COM_ParseFile( file, token );
	}
	m_pCurrentMenu = NULL;
	SetCurrentCommandMenu( NULL );
	gEngfuncs.COM_FreeFile( file );
	m_iInitialized = true;
}

CommandButton* CTFViewport::CreateCustomButton( char* text, char* name )
{
	CommandButton* button = NULL;
	CCommandMenu* menu = NULL;
	if ( !strcmp( name, "!CHANGETEAM" ) )
	{
		button = new CommandButton( text, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		menu = CreateSubMenu( button, m_pCurrentCommandMenu );
		m_pCommandMenus[m_iNumMenus++] = menu;
		for ( int i = 0; i < 4; i++ )
		{
			char command[256];
			sprintf( command, "jointeam %d", i + 1 );
			m_pTeamButtons[i] = new TeamButton( i + 1, "teamname", 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
			m_pTeamButtons[i]->addActionSignal( new CMenuHandler_StringCommandWatch( command ) );
			menu->AddButton( m_pTeamButtons[i] );
		}
		m_pTeamButtons[4] = new TeamButton( 5, CHudTextMessage::BufferedLocaliseTextString( "#Team_AutoAssign" ),
			0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		m_pTeamButtons[4]->addActionSignal( new CMenuHandler_StringCommand( "jointeam 5" ) );
		menu->AddButton( m_pTeamButtons[4] );
		m_pTeamButtons[5] = new SpectateButton( CHudTextMessage::BufferedLocaliseTextString( "#Menu_Spectate" ),
			0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y, false );
		m_pTeamButtons[5]->addActionSignal( new CMenuHandler_StringCommand( "spectate" ) );
		menu->AddButton( m_pTeamButtons[5] );
		return button;
	}
	if ( !strcmp( name, "!CHANGECLASS" ) )
	{
		button = new ClassButton( -1, text, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y, false );
		menu = CreateSubMenu( button, m_pCurrentCommandMenu );
		m_pCommandMenus[m_iNumMenus++] = menu;
		for ( int i = 0; i < 7; i++ )
		{
			char command[256];
			CHudTextMessage::LocaliseTextString( sLocalisedClasses[gViewPort->m_iCTFTeamNumber - 1][i],
				command, sizeof( command ) );
			ClassButton* classButton = new ClassButton( i, command, 0, BUTTON_SIZE_Y,
				CMENU_SIZE_X, BUTTON_SIZE_Y, false );
			sprintf( command, "%s", sCTFClassSelection[gViewPort->m_iCTFTeamNumber - 1][i] );
			classButton->addActionSignal( new CMenuHandler_StringCommandClassSelect( command ) );
			menu->AddButton( classButton );
		}
		return button;
	}
	if ( !strcmp( name, "!MAPBRIEFING" ) )
	{
		button = new CommandButton( text, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(),
			CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_TextWindow( MENU_MAPBRIEFING ) );
	}
	else if ( !strcmp( name, "!CLASSDESC" ) )
	{
		button = new ClassButton( 0, text, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(),
			CMENU_SIZE_X, BUTTON_SIZE_Y, false );
		button->addActionSignal( new CMenuHandler_TextWindow( MENU_CLASSHELP ) );
	}
	else if ( !strcmp( name, "!SERVERINFO" ) )
	{
		button = new ClassButton( 0, text, 0, BUTTON_SIZE_Y * m_pCurrentCommandMenu->GetNumButtons(),
			CMENU_SIZE_X, BUTTON_SIZE_Y, false );
		button->addActionSignal( new CMenuHandler_TextWindow( MENU_INTRO ) );
	}
	else if ( !strcmp( name, "!SPY" ) )
	{
		return new DisguiseButton( 0, text, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
	}
	else if ( !strcmp( name, "!FEIGN" ) )
	{
		button = new FeignButton( FALSE, text, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "feign" ) );
	}
	else if ( !strcmp( name, "!FEIGNSILENT" ) )
	{
		button = new FeignButton( FALSE, text, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "sfeign" ) );
	}
	else if ( !strcmp( name, "!FEIGNSTOP" ) )
	{
		button = new FeignButton( TRUE, text, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "feign" ) );
	}
	else if ( !strcmp( name, "!DISGUISEENEMY" ) )
	{
		button = new DisguiseButton( DISGUISE_TEAM2, text, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		CreateDisguiseSubmenu( button, m_pCurrentCommandMenu, "disguise_enemy" );
		return button;
	}
	else if ( !strcmp( name, "!DISGUISEFRIENDLY" ) )
	{
		button = new DisguiseButton( DISGUISE_TEAM1 | DISGUISE_TEAM2, text, 0,
			BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		CreateDisguiseSubmenu( button, m_pCurrentCommandMenu, "disguise_friendly" );
		return button;
	}
	else if ( !strcmp( name, "!DISGUISE" ) )
	{
		button = new DisguiseButton( DISGUISE_TEAM3 | DISGUISE_TEAM4, text, 0,
			BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		menu = CreateSubMenu( button, m_pCurrentCommandMenu );
		m_pCommandMenus[m_iNumMenus++] = menu;
		for ( int i = 1; i <= 4; i++ )
		{
			int teams = ( i < 4 ? DISGUISE_TEAM3 : 0 ) | DISGUISE_TEAM4;
			m_pDisguiseButtons[i] = new DisguiseButton( teams, "Disguise", 0,
				BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
			menu->AddButton( m_pDisguiseButtons[i] );
			char command[256];
			sprintf( command, "disguise %d", i );
			CreateDisguiseSubmenu( m_pDisguiseButtons[i], menu, command );
		}
		return button;
	}
	else if ( !strcmp( name, "!DETPACKSTART" ) )
	{
		button = new DetpackButton( 2, text, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		menu = CreateSubMenu( button, m_pCurrentCommandMenu );
		m_pCommandMenus[m_iNumMenus++] = menu;
		CommandButton* detpack = new CommandButton( m_sDetpackStrings[0], 0,
			BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		detpack->addActionSignal( new CMenuHandler_StringCommand( "detstart 5" ) );
		menu->AddButton( detpack );
		detpack = new CommandButton( m_sDetpackStrings[1], 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		detpack->addActionSignal( new CMenuHandler_StringCommand( "detstart 20" ) );
		menu->AddButton( detpack );
		detpack = new CommandButton( m_sDetpackStrings[2], 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		detpack->addActionSignal( new CMenuHandler_StringCommand( "detstart 50" ) );
		menu->AddButton( detpack );
		return button;
	}
	else if ( !strcmp( name, "!DETPACKSTOP" ) )
	{
		button = new DetpackButton( 1, text, 0, BUTTON_SIZE_Y, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "detstop" ) );
	}
	else if ( !strcmp( name, "!BUILD" ) )
	{
		return new BuildButton( BUILDSTATE_BASE, 0, text, 0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
	}
	else if ( !strcmp( name, "!BUILDSENTRY" ) )
	{
		button = new BuildButton( BUILDSTATE_CANBUILD, BuildButton::SENTRYGUN, text,
			0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "build 2" ) );
	}
	else if ( !strcmp( name, "!BUILDDISPENSER" ) )
	{
		button = new BuildButton( BUILDSTATE_CANBUILD, BuildButton::DISPENSER, text,
			0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "build 1" ) );
	}
	else if ( !strcmp( name, "!ROTATESENTRY180" ) )
	{
		button = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::SENTRYGUN, text,
			0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "rotatesentry180" ) );
	}
	else if ( !strcmp( name, "!ROTATESENTRY" ) )
	{
		button = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::SENTRYGUN, text,
			0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "rotatesentry" ) );
	}
	else if ( !strcmp( name, "!DISMANTLEDISPENSER" ) )
	{
		button = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::DISPENSER, text,
			0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "dismantle 1" ) );
	}
	else if ( !strcmp( name, "!DISMANTLESENTRY" ) )
	{
		button = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::SENTRYGUN, text,
			0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "dismantle 2" ) );
	}
	else if ( !strcmp( name, "!DETONATEDISPENSER" ) )
	{
		button = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::DISPENSER, text,
			0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "detdispenser" ) );
	}
	else if ( !strcmp( name, "!DETONATESENTRY" ) )
	{
		button = new BuildButton( BUILDSTATE_HASBUILDING, BuildButton::SENTRYGUN, text,
			0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "detsentry" ) );
	}
	else if ( !strcmp( name, "!BUILDSTOP" ) )
	{
		button = new BuildButton( BUILDSTATE_BUILDING, 0, text,
			0, BUTTON_SIZE_Y * 2, CMENU_SIZE_X, BUTTON_SIZE_Y );
		button->addActionSignal( new CMenuHandler_StringCommand( "build" ) );
	}
	if ( button )
		button->addInputSignal( new CMenuHandler_PopupSubMenuInput( button, m_pCurrentCommandMenu ) );
	return button;
}
