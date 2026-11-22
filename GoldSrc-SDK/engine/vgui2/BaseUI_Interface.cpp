#include "BaseUI_Interface.h"
#include "EnginePanel.h"
#include "BaseUISurface.h"
#include "../common.h"
#include "../controls.h"
#include "../filesystem.h"
#include "../interface.h"
#include "../keys.h"
#include "../SystemWrapper.h"
#include "../utlvector.h"
#include "../vgui/Cursor.h"
#include "../vgui/IInput.h"
#include "../vgui/IInputInternal.h"
#include "../vgui/ISystem.h"
#include "../vgui/IVGui.h"
#include "vgui/MouseCode.h"
#include "../../vgui2/controls/controls.h"

#include <cstdio>
#include <cstring>

#if defined( _WIN32 )
#include <Windows.h>
#endif

#if !defined( _WIN32 )
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#endif

extern vgui2::IInputInternal* g_InputInternal;
extern HINSTANCE hClientDLL;
extern qboolean g_bIsDedicatedServer;
extern CUtlVector<char*> g_fallbackLocalizationFiles;

struct TSteamProgress;
struct TSteamError;

enum VGUIPANEL
{
	PANEL_ROOT = 0,
	PANEL_CLIENTDLL,
	PANEL_GAMEUIDLL
};

class CEngineVGui : public IBaseInterface
{
public:
	CEngineVGui();
	~CEngineVGui() override;
	virtual vgui2::VPANEL GetPanel( VGUIPANEL type );
	virtual bool SteamRefreshLogin( const char* password, bool isSecure );
	virtual bool SteamProcessCall( bool* finished, TSteamProgress* progress, TSteamError* steamError );
	void SetEngineVisible( bool state );

private:
	bool m_bVisible;
	int m_hRefreshLoginHandle;
};

IGameUI* staticGameUIFuncs;
IGameConsole* staticGameConsole;
ICareerUI* staticCareerUI;
IClientVGUI* staticClient;
BaseUISurface* staticSurface;
CEnginePanel* staticClientDLLPanel;
CEnginePanel* staticGameUIPanel;
IMouseControl* mousecontrol;
cl_enginefunc_t gEngfuncs;
bool m_bConsoleShowing;
static CStaticPanel* staticPanel;

CBaseUI* g_pBaseUI;

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CBaseUI::CBaseUI()
{
	m_hVGuiModule = nullptr;
	g_pBaseUI = this;
	m_hStaticGameUIModule = nullptr;
	m_bConsoleShowing = false;
	m_bHidingGameUI = false;
}

CBaseUI::~CBaseUI()
{
	g_pBaseUI = nullptr;
}

static CBaseUI g_BaseUI;

static void* __CreateCBaseUIIBaseUI_interface()
{
	return static_cast<IBaseUI*>( &g_BaseUI );
}

static InterfaceReg __g_CreateCBaseUIIBaseUI_reg( __CreateCBaseUIIBaseUI_interface, "BaseUI001" );

static CEngineVGui __g_CEngineVGui_singleton;

static void* __CreateCEngineVGuiIEngineVGui_interface()
{
	return &__g_CEngineVGui_singleton;
}

static InterfaceReg __g_CreateCEngineVGuiIEngineVGui_reg( __CreateCEngineVGuiIEngineVGui_interface, "VEngineVGui001" );

namespace vgui2
{
ISurface* g_pSurface;
}

vgui2::VPANEL CStaticPanel::IsWithinTraverse( int x, int y, bool traversePopups )
{
	vgui2::VPANEL panel = vgui2::Panel::IsWithinTraverse( x, y, traversePopups );
	if ( panel == GetVPanel() )
		return 0;
	return panel;
}

vgui2::VPANEL CStaticPanel::GetCurrentKeyFocus()
{
	return staticClientDLLPanel->GetCurrentKeyFocus();
}

int CBaseUI::Key_Event( int down, int keynum, const char* )
{
	if ( toggleconsole_key && keynum == toggleconsole_key )
		return 0;
	if ( down && keynum == K_ESCAPE )
	{
		const char* levelName = gEngfuncs.pfnGetLevelName();
		if ( staticGameUIFuncs->IsGameUIActive() && levelName && levelName[0] )
			HideGameUI();
		else
			ActivateGameUI();
		return 1;
	}
	if ( m_bHidingGameUI && keynum == K_MOUSE1 && down )
	{
		m_bHidingGameUI = false;
		return 1;
	}
	return vgui2::surface()->NeedKBInput() != 0;
}

void CBaseUI::CallEngineSurfaceAppHandler( void* event, void* userData )
{
	if ( staticSurface )
		staticSurface->AppHandler( event, userData );
}

void CBaseUI::Paint( int x, int y, int right, int bottom )
{
	if ( !staticSurface )
		return;
	vgui2::VPANEL panel = staticSurface->GetEmbeddedPanel();
	if ( !panel )
		return;
	staticGameUIFuncs->RunFrame();
	vgui2::ivgui()->RunFrame();
	int width = static_cast<int>( static_cast<unsigned int>( right ) - static_cast<unsigned int>( x ) );
	int height = static_cast<int>( static_cast<unsigned int>( bottom ) - static_cast<unsigned int>( y ) );
	g_BaseUISurface.SetScreenBounds( x, y, width, height );
	staticPanel->SetBounds( 0, 0, right, bottom );
	staticGameUIPanel->SetBounds( 0, 0, right, bottom );
	staticClientDLLPanel->SetBounds( 0, 0, right, bottom );
	staticPanel->Repaint();
	vgui2::surface()->PaintTraverse( panel );
}

void CBaseUI::HideGameUI()
{
	staticGameUIFuncs->HideGameUI();
	staticGameConsole->Hide();
	const char* levelName = gEngfuncs.pfnGetLevelName();
	if ( levelName && levelName[0] )
	{
		staticGameUIPanel->SetVisible( false );
		staticGameUIPanel->SetPaintBackgroundEnabled( false );
		staticClientDLLPanel->SetVisible( true );
		staticClientDLLPanel->SetMouseInputEnabled( true );
		if ( staticClient )
			staticClient->ActivateClientUI();
	}
	if ( vgui2::input()->IsMouseDown( vgui2::MOUSE_LEFT ) )
		m_bHidingGameUI = true;
}

void CBaseUI::ActivateGameUI()
{
	staticGameUIFuncs->ActivateGameUI();
	staticGameUIPanel->SetVisible( true );
	staticClientDLLPanel->SetVisible( false );
	staticClientDLLPanel->SetMouseInputEnabled( false );
	if ( staticClient )
	{
		staticClient->HideClientUI();
		staticClient->HideScoreBoard();
	}
	if ( m_bConsoleShowing )
		staticGameConsole->Activate();
}

void CBaseUI::HideConsole()
{
	m_bConsoleShowing = false;
	staticGameConsole->Hide();
}

void CBaseUI::ShowConsole()
{
	m_bConsoleShowing = true;
	staticGameConsole->Activate();
}

void CBaseUI::Initialize( CreateInterfaceFn* factories, int )
{
	if ( staticGameUIFuncs )
		return;

	CreateInterfaceFn engineFactory = factories[0];
	CreateInterfaceFn fileSystemFactory = factories[1];
	fileSystemFactory( "VFileSystem009", nullptr );
#ifdef _WIN32
	m_hVGuiModule = Sys_LoadModule( "vgui2.dll" );
#else
	m_hVGuiModule = Sys_LoadModule( "vgui2.so" );
#endif
	CreateInterfaceFn vguiFactory = Sys_GetFactory( m_hVGuiModule );
	m_hChromeModule = Sys_LoadModule( "chromehtml.dll" );
	m_FactoryList[3] = Sys_GetFactory( m_hChromeModule );
	m_FactoryList[0] = engineFactory;
	m_FactoryList[1] = vguiFactory;
	m_FactoryList[2] = fileSystemFactory;
	m_iNumFactories = 4;

	char filename[512];
#ifdef _WIN32
	_snprintf( filename, sizeof( filename ), "cl_dlls\\GameUI.dll" );
#else
	_snprintf( filename, sizeof( filename ), "cl_dlls/gameui.so" );
#endif
	COM_ExpandFilename( filename );
	COM_FixSlashes( filename );
	FS_GetLocalCopy( filename );
	m_hStaticGameUIModule = Sys_LoadModule( filename );
	CreateInterfaceFn gameUIFactory = Sys_GetFactory( m_hStaticGameUIModule );
	m_FactoryList[m_iNumFactories] = gameUIFactory;
	if ( gameUIFactory )
	{
		staticGameUIFuncs = static_cast<IGameUI*>( gameUIFactory( "GameUI007", nullptr ) );
		staticGameConsole = static_cast<IGameConsole*>( gameUIFactory( "GameConsole003", nullptr ) );
		staticCareerUI = static_cast<ICareerUI*>( gameUIFactory( "CareerUI001", nullptr ) );
		++m_iNumFactories;
	}
	g_InputInternal = static_cast<vgui2::IInputInternal*>( vguiFactory( "VGUI_InputInternal001", nullptr ) );
#if defined( _WIN32 )
	if ( g_InputInternal == nullptr )
		__debugbreak();
#endif

	if ( cl_funcs.pClientFactory )
	{
		CreateInterfaceFn clientFactory = Sys_GetFactory( reinterpret_cast<CSysModule*>( hClientDLL ) );
		if ( clientFactory )
		{
			++m_iNumFactories;
			m_FactoryList[m_iNumFactories - 1] = clientFactory;
			staticClient = static_cast<IClientVGUI*>( clientFactory( CLIENTVGUI_INTERFACE_VERSION, nullptr ) );
		}
	}
	else
	{
		m_FactoryList[m_iNumFactories] = nullptr;
	}
	vgui2::VGuiControls_Init( "BaseUI", m_FactoryList, m_iNumFactories );
}

CBaseUI* BaseUI()
{
	return &g_BaseUI;
}

CEngineVGui::CEngineVGui()
{
	m_bVisible = true;
	m_hRefreshLoginHandle = 0;
}

CEngineVGui::~CEngineVGui()
{
}

vgui2::VPANEL CEngineVGui::GetPanel( VGUIPANEL type )
{
	if ( type == PANEL_CLIENTDLL )
		return staticClientDLLPanel->GetVPanel();
	if ( type == PANEL_GAMEUIDLL )
		return staticGameUIPanel->GetVPanel();
	return staticPanel->GetVPanel();
}

bool CEngineVGui::SteamRefreshLogin( const char*, bool )
{
	return true;
}

bool CEngineVGui::SteamProcessCall( bool*, TSteamProgress*, TSteamError* )
{
	return true;
}

void CEngineVGui::SetEngineVisible( bool state )
{
	m_bVisible = state;
	if ( !g_bIsDedicatedServer )
	{
		if ( state )
			ClientDLL_ActivateMouse();
		else
			ClientDLL_DeactivateMouse();
	}
}

CStaticPanel::CStaticPanel( vgui2::Panel* parent, const char* name ) : vgui2::Panel( parent, name )
{
	SetCursor( static_cast<vgui2::HCursor>( vgui2::dc_none ) );
	Color color = GetBgColor();
	color.SetColor( color.r(), color.g(), color.b(), 255 );
	SetBgColor( color );
}

CStaticPanel::~CStaticPanel()
{
}

CEnginePanel::CEnginePanel( vgui2::Panel* parent, const char* name ) : vgui2::EditablePanel( parent, name )
{
	m_bCanFocus = true;
	Color color = GetBgColor();
	color.SetColor( color.r(), color.g(), color.b(), 255 );
	SetBgColor( color );
}

CEnginePanel::~CEnginePanel()
{
}

vgui2::VPANEL CEnginePanel::IsWithinTraverse( int x, int y, bool traversePopups )
{
	if ( !m_bCanFocus )
		return 0;
	vgui2::VPANEL panel = vgui2::Panel::IsWithinTraverse( x, y, traversePopups );
	if ( panel == GetVPanel() )
		return 0;
	return panel;
}

void CBaseUI::Shutdown()
{
	vgui2::ivgui()->RunFrame();
	staticGameUIFuncs->Shutdown();

	char gameDir[32];
	std::strncpy( gameDir, gEngfuncs.pfnGetGameDirectory(), 31 );
	gameDir[31] = 0;
	if ( staticClient != nullptr
		 && ( Q_strcasecmp( gameDir, "cstrike" ) == 0 || Q_strcasecmp( gameDir, "czero" ) == 0 ) )
	{
		staticClient->Shutdown();
	}

	Sys_UnloadModule( m_hStaticGameUIModule );
	m_hStaticGameUIModule = nullptr;
	staticCareerUI = nullptr;
	staticGameConsole = nullptr;
	staticGameUIFuncs = nullptr;
	vgui2::system()->SaveUserConfigFile();
	Sys_UnloadModule( m_hVGuiModule );
	m_hVGuiModule = nullptr;
	staticSurface->Shutdown();
	if ( m_hChromeModule != nullptr )
	{
		Sys_UnloadModule( m_hChromeModule );
		m_hChromeModule = nullptr;
	}
}

void CBaseUI::Start( cl_enginefuncs_s* engineFuncs, int )
{
	std::memcpy( &gEngfuncs, engineFuncs, sizeof( gEngfuncs ) );

	staticPanel = new CStaticPanel( nullptr, "StaticPanel" );
	staticPanel->SetBounds( 0, 0, 40, 30 );
	staticPanel->SetPaintBorderEnabled( false );
	staticPanel->SetPaintBackgroundEnabled( false );
	staticPanel->SetPaintEnabled( false );
	staticPanel->SetCursor( static_cast<vgui2::HCursor>( vgui2::dc_none ) );
	staticPanel->SetVisible( true );
	staticPanel->SetZPos( 0 );

	staticSurface = static_cast<BaseUISurface*>( m_FactoryList[0]( "VGUI_Surface026", nullptr ) );
	vgui2::g_pSurface = staticSurface;
	IHTMLChromeController* chromeController =
		static_cast<IHTMLChromeController*>( m_FactoryList[3]( "ChromeHTML_Controller_001", nullptr ) );
	IEngineSurface* engineSurface = static_cast<IEngineSurface*>( m_FactoryList[0]( "EngineSurface007", nullptr ) );
	staticSurface->Init( staticPanel->GetVPanel(), engineSurface, chromeController );
	mousecontrol = staticSurface;
	staticSurface->m_bIgnoreMouseVisCalc = true;

	if ( vgui2::scheme()->LoadSchemeFromFile( "resource/trackerscheme.res", "BaseUI" ) == 0 )
		gEngfuncs.Con_Printf( "Error loading '%s'\n", "resource/trackerScheme.res" );

	vgui2::localize()->AddFile( vgui2::filesystem(), "resource/valve_%language%.txt" );

	char gameDir[32];
	std::strncpy( gameDir, gEngfuncs.pfnGetGameDirectory(), 31 );
	gameDir[31] = 0;
	if ( g_fallbackLocalizationFiles.Count() > 0 )
	{
		for ( int index = 0; index < g_fallbackLocalizationFiles.Count(); ++index )
		{
			char* fileName = g_fallbackLocalizationFiles[index];
			vgui2::localize()->AddFile( vgui2::filesystem(), fileName );
			if ( fileName != nullptr )
				delete[] fileName;
		}
	}
	if ( g_fallbackLocalizationFiles.Count() > 0 )
		g_fallbackLocalizationFiles.RemoveAll();

	if ( std::strcmp( gameDir, "valve" ) != 0 )
	{
		char localizationFile[128];
		_snprintf( localizationFile, 127, "resource/%s_%%language%%.txt", gameDir );
		localizationFile[127] = 0;
		vgui2::localize()->AddFile( vgui2::filesystem(), localizationFile );
		vgui2::localize()->AddFile( vgui2::filesystem(), "../valve/resource/gameui_%language%.txt" );
	}

	vgui2::ivgui()->Start();
	vgui2::ivgui()->SetSleep( false );

	staticClientDLLPanel = new CEnginePanel( staticPanel, "BaseClientPanel" );
	staticClientDLLPanel->SetPaintBorderEnabled( false );
	staticClientDLLPanel->SetPaintBackgroundEnabled( false );
	staticClientDLLPanel->SetPaintEnabled( false );
	staticClientDLLPanel->SetVisible( true );
	staticClientDLLPanel->SetCursor( static_cast<vgui2::HCursor>( vgui2::dc_none ) );
	staticClientDLLPanel->SetZPos( 25 );

	staticGameUIPanel = new CEnginePanel( staticPanel, "BaseGameUIPanel" );
	staticGameUIPanel->SetPaintBorderEnabled( false );
	staticGameUIPanel->SetPaintBackgroundEnabled( false );
	staticGameUIPanel->SetPaintEnabled( false );
	staticGameUIPanel->SetVisible( true );
	staticGameUIPanel->SetCursor( static_cast<vgui2::HCursor>( vgui2::dc_none ) );
	staticGameUIPanel->SetZPos( 50 );

	if ( staticGameUIFuncs != nullptr )
		staticGameUIFuncs->Initialize( m_FactoryList, m_iNumFactories );
	if ( staticClient != nullptr )
	{
		staticClient->Initialize( m_FactoryList, m_iNumFactories );
		staticSurface->SetVGUI2MouseControl( true );
	}

	if ( staticGameUIFuncs != nullptr )
		staticGameUIFuncs->Start( &gEngfuncs, 7, &gSystemWrapper );

	staticClientDLLPanel->SetScheme( "ClientScheme" );
	if ( staticClient != nullptr )
	{
		staticClient->Start();
		staticClient->SetParent( staticClientDLLPanel->GetVPanel() );
	}

	int wide;
	int tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	staticPanel->SetBounds( 0, 0, wide, tall );
	staticGameUIPanel->SetBounds( 0, 0, wide, tall );
	staticClientDLLPanel->SetBounds( 0, 0, wide, tall );

	staticGameConsole->Initialize();
	if ( staticGameConsole != nullptr )
		staticGameConsole->SetParent( staticGameUIPanel->GetVPanel() );

	staticSurface->m_bIgnoreMouseVisCalc = false;
	staticGameUIFuncs->ActivateGameUI();
#if defined( _WIN32 )
	if ( std::strstr( GetCommandLineA(), "-console" ) != nullptr )
#else
	if ( COM_CheckParm( "-console" ) != 0 )
#endif
	{
		staticGameConsole->Activate();
		m_bConsoleShowing = true;
	}
}

