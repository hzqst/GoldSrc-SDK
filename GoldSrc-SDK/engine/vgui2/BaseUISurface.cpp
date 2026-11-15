#include "BaseUISurface.h"
#include "../keys.h"
#include "../vgui/IPanel.h"
#include "../vgui/IInput.h"
#include "../vgui/IInputInternal.h"
#include "../../vgui2/vgui_surfacelib/vgui_key_translation.h"
#include "../../vgui2/vgui_surfacelib/FontManager.h"
#include "../../vgui2/vgui_surfacelib/FontTextureCache.h"
#include "../unicode_strtools.h"
#include "../vgui/Dar.h"
#include "../html/ichromehtmlwrapper.h"
#include "../igame.h"
#include "../vgui/IScheme.h"
#include "../../external/steamworks/isteamapps.h"
#include "../vgui/Cursor.h"
#include "../common.h"
#include "../cvar.h"
#include "../../external/SDL2/SDL.h"
#include "../filesystem.h"
#include "../ivideomode.h"
#include "../vid.h"
#include "../../vgui2/controls/controls.h"
#include "engine_apiproxy.h"
#include "cdll_int.h"

#include "../glquake.h"

#include <cstring>
#include <cstdlib>

#if !defined( _WIN32 )
#include <fontconfig/fontconfig.h>
#endif

#undef PlaySound
#ifdef CreateFont
#undef CreateFont
#endif

#define MAX_DEFAULT_CURSORS 20

#define MOUSE_BUTTON_EXTRA_3 6
#define MOUSE_BUTTON_EXTRA_4 7
#define MOUSE_BUTTON_EXTRA_5 8
#define MOUSE_BUTTON_EXTRA_6 9

#define MINIMIZED_PLAT ( (vgui2::SurfacePlat*)1 )

extern cl_enginefunc_t gEngfuncs;
extern qboolean BUsesSDLInput();
extern cvar_t m_rawinput;
extern cvar_t cl_mousegrab;
SDL_Keycode GetSDLKeycodeFromEngineKey( int keynum );

vgui2::IInputInternal* g_InputInternal;

static SDL_Cursor* staticDefaultCursor[MAX_DEFAULT_CURSORS];
static SDL_Cursor* staticCurrentCursor;
static bool s_bCursorVisible = true;
static bool s_bUsedRawInputLast;

cvar_t sdl_double_click_size = { "sdl_double_click_size", "2" };
cvar_t sdl_double_click_time = { "sdl_double_click_time", "400" };

static vgui2::Dar<vgui2::VPANEL> staticPopupList;

vgui2::IInputInternal* inputinternal()
{
	return g_InputInternal;
}

static void CheckModState()
{
	struct ModifierKey_t
	{
		SDL_Keymod modifier;
		vgui2::KeyCode key;
	};

	static const ModifierKey_t modifierKeys[] = {
		{ KMOD_LSHIFT, vgui2::KEY_LSHIFT },
		{ KMOD_LALT, vgui2::KEY_LALT },
		{ KMOD_LCTRL, vgui2::KEY_LCONTROL },
		{ KMOD_RSHIFT, vgui2::KEY_LSHIFT },
		{ KMOD_RALT, vgui2::KEY_RALT },
		{ KMOD_RCTRL, vgui2::KEY_RCONTROL },
		{ KMOD_CAPS, vgui2::KEY_CAPSLOCK },
		{ KMOD_LGUI, vgui2::KEY_LWIN },
		{ KMOD_RGUI, vgui2::KEY_RWIN },
	};
	static Uint32 s_lastModifierCode;

	Uint32 changed = s_lastModifierCode ^ SDL_GetModState();
	for ( const ModifierKey_t& mod : modifierKeys )
	{
		if ( !( changed & mod.modifier ) )
			continue;

		if ( s_lastModifierCode & mod.modifier )
		{
			g_InputInternal->InternalKeyCodeReleased( mod.key );
		}
		else
		{
			g_InputInternal->InternalKeyCodePressed( mod.key );
			g_InputInternal->InternalKeyCodeTyped( mod.key );
		}
	}
	s_lastModifierCode = SDL_GetModState();
}

void BaseUISurface::AppHandler( void* event, void* )
{
	SDL_Event ev = *(const SDL_Event*)event;
	switch ( ev.type )
	{
	case SDL_KEYDOWN:
		g_InputInternal->InternalKeyCodePressed( KeyCode_VirtualKeyToVGUI( ev.key.keysym.sym ) );
		g_InputInternal->InternalKeyCodeTyped( KeyCode_VirtualKeyToVGUI( ev.key.keysym.sym ) );
		CheckModState();
		break;
	case SDL_KEYUP:
		g_InputInternal->InternalKeyCodeReleased( KeyCode_VirtualKeyToVGUI( ev.key.keysym.sym ) );
		CheckModState();
		break;
	case SDL_TEXTINPUT:
		for ( unsigned int i = 0; i < sizeof( ev.text.text ) && ev.text.text[i]; )
		{
			uchar32 ch;
			bool error;
			int bytes = Q_UTF8ToUChar32( &ev.text.text[i], ch, error );
			if ( error )
				break;
			g_InputInternal->InternalKeyTyped( (wchar_t)ch );
			i += bytes;
		}
		break;
	case SDL_MOUSEMOTION:
		if ( VideoMode_IsWindowed() )
		{
			g_InputInternal->InternalCursorMoved( ev.motion.x, ev.motion.y );
		}
		else
		{
			int windowWide, windowTall;
			int videoWide, videoTall;
			SDL_GetWindowSize( (SDL_Window*)game->GetMainWindow(), &windowWide, &windowTall );
			VideoMode_GetCurrentVideoMode( &videoWide, &videoTall, NULL );

			int scaledX = (int)( ( (float)videoWide / windowWide ) * ev.motion.x );
			int scaledY = (int)( ( (float)videoTall / windowTall ) * ev.motion.y );
			int x = (int)( ( GetXMouseAspectRatioAdjustment() - 1.0 ) * (float)( scaledX - videoWide / 2 ) + scaledX );
			int y = (int)( ( GetYMouseAspectRatioAdjustment() - 1.0 ) * (float)( scaledY - videoTall / 2 ) + scaledY );
			g_InputInternal->InternalCursorMoved( x, y );
		}
		m_deltax += ev.motion.xrel;
		m_deltay += ev.motion.yrel;
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
	{
		vgui2::MouseCode button = vgui2::MOUSE_LEFT;
		switch ( ev.button.button )
		{
		case SDL_BUTTON_LEFT:
			button = vgui2::MOUSE_LEFT;
			break;
		case SDL_BUTTON_MIDDLE:
			button = vgui2::MOUSE_MIDDLE;
			break;
		case SDL_BUTTON_RIGHT:
			button = vgui2::MOUSE_RIGHT;
			break;
		case SDL_BUTTON_X1:
		case MOUSE_BUTTON_EXTRA_3:
		case MOUSE_BUTTON_EXTRA_5:
			button = vgui2::MOUSE_4;
			break;
		case SDL_BUTTON_X2:
		case MOUSE_BUTTON_EXTRA_4:
		case MOUSE_BUTTON_EXTRA_6:
			button = vgui2::MOUSE_5;
			break;
		}

		if ( ev.type != SDL_MOUSEBUTTONDOWN )
		{
			g_InputInternal->InternalMouseReleased( button );
			break;
		}

		if ( m_bGotMouseButtonDown
			 && sdl_double_click_time.value >= (float)(int)( ev.button.timestamp - m_MouseButtonDownTimeStamp )
			 && sdl_double_click_size.value >= (float)abs( ev.button.x - m_MouseButtonDownX )
			 && sdl_double_click_size.value >= (float)abs( ev.button.y - m_MouseButtonDownY ) )
		{
			m_bGotMouseButtonDown = false;
			g_InputInternal->InternalMouseDoublePressed( button );
		}
		else
		{
			m_MouseButtonDownTimeStamp = ev.button.timestamp;
			m_MouseButtonDownX = ev.button.x;
			m_MouseButtonDownY = ev.button.y;
			m_bGotMouseButtonDown = true;
			g_InputInternal->InternalMousePressed( button );
		}
		break;
	}
	case SDL_MOUSEWHEEL:
		g_InputInternal->InternalMouseWheeled( ev.wheel.y );
		break;
	}
}

bool BaseUISurface::DeleteTextureByID( int texture )
{
#if defined( SW_RENDERER )
	(void)texture;
	return false;
#else
	GLuint id = (GLuint)texture;
	qglDeleteTextures( 1, &id );
	return true;
#endif
}

void BaseUISurface::GetMouseDelta( int* x, int* y )
{
	*x = m_deltax;
	*y = m_deltay;
	m_deltay = 0;
	m_deltax = 0;
}

bool BaseUISurface::SupportsFeature( SurfaceFeature_e feature )
{
#if defined( SW_RENDERER )
	return feature == ESCAPE_KEY && m_bSupportsEsc;
#else
	switch ( feature )
	{
	case ANTIALIASED_FONTS:
	case DROPSHADOW_FONTS:
		return true;
	case ESCAPE_KEY:
		return m_bSupportsEsc;
	default:
		return false;
	}
#endif
}

void BaseUISurface::CalculateMouseVisible()
{
	if ( m_bIgnoreMouseVisCalc )
		return;

	_needMouse = false;
	_needKB = false;
	for ( int i = 0; i < vgui2::surface()->GetPopupCount(); i++ )
	{
		VPANEL panel = vgui2::surface()->GetPopup( i );
		bool visible = vgui2::ipanel()->IsVisible( panel );
		VPANEL parent = vgui2::ipanel()->GetParent( panel );
		while ( parent && visible )
		{
			if ( !vgui2::ipanel()->IsVisible( parent ) )
			{
				visible = false;
				break;
			}
			parent = vgui2::ipanel()->GetParent( parent );
		}

		if ( visible )
		{
			_needMouse |= vgui2::ipanel()->IsMouseInputEnabled( panel );
			_needKB |= vgui2::ipanel()->IsKeyBoardInputEnabled( panel );
		}
	}

	UnlockCursor();
	if ( _needMouse )
	{
		SetCursor( vgui2::dc_arrow );
		return;
	}

	SetCursor( vgui2::dc_none );
	LockCursor();
	if ( !m_rawinput.value && BUsesSDLInput() )
	{
		int wide, tall;
		SDL_GetWindowSize( (SDL_Window*)game->GetMainWindow(), &wide, &tall );
		SDL_WarpMouseInWindow( (SDL_Window*)game->GetMainWindow(), wide / 2, tall / 2 );

		SDL_PumpEvents();
		int dx, dy;
		SDL_GetRelativeMouseState( &dx, &dy );
	}
}

void BaseUISurface::PlaySound( const char* filename )
{
	gEngfuncs.pfnPlaySoundByName( (char*)filename, 1.0f );
}

BaseUISurface::VPANEL BaseUISurface::GetModalPanel()
{
	return _restrictedPanel;
}

bool BaseUISurface::VGUI2MouseControl()
{
	return m_bVGUI2MouseControl;
}

void BaseUISurface::SetVGUI2MouseControl( bool state )
{
	m_bVGUI2MouseControl = state;
}

void BaseUISurface::SetScreenBounds( int x, int y, int width, int height )
{
	m_iSurfaceBounds[0] = x;
	m_iSurfaceBounds[1] = y;
	m_iSurfaceBounds[2] = width;
	m_iSurfaceBounds[3] = height;
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
BaseUISurface::BaseUISurface()
	: _restrictedPanel( 0 ), m_hCurrentFont( 0 ), m_iCurrentTexture( 0 ), m_pChromeController( NULL ),
	  _cursorLocked( false ), _proportionalBaseWidth( 640 ), _proportionalBaseHeight( 480 ), _needMouse( true ),
	  _needKB( true ), m_bVGUI2MouseControl( false ), m_bFullScreen( false ), m_bSupportsEsc( false ), m_deltax( 0 ),
	  m_deltay( 0 ), _proportionalBaseWidth_HD( 1280 ), _proportionalBaseHeight_HD( 720 )
{
	m_bTranslateExtendedKeys = true;
	if ( !SDL_WasInit( SDL_INIT_VIDEO ) )
		SDL_InitSubSystem( SDL_INIT_VIDEO );

	staticDefaultCursor[vgui2::dc_none] = NULL;
	staticDefaultCursor[vgui2::dc_arrow] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_ARROW );
	staticDefaultCursor[vgui2::dc_ibeam] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_IBEAM );
	staticDefaultCursor[vgui2::dc_hourglass] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_WAIT );
	staticDefaultCursor[vgui2::dc_crosshair] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_CROSSHAIR );
	staticDefaultCursor[vgui2::dc_up] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENS );
	staticDefaultCursor[vgui2::dc_sizenwse] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENWSE );
	staticDefaultCursor[vgui2::dc_sizenesw] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENESW );
	staticDefaultCursor[vgui2::dc_sizewe] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEWE );
	staticDefaultCursor[vgui2::dc_sizens] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENS );
	staticDefaultCursor[vgui2::dc_sizeall] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEALL );
	staticDefaultCursor[vgui2::dc_no] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_NO );
	staticDefaultCursor[vgui2::dc_hand] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_HAND );
	staticCurrentCursor = staticDefaultCursor[vgui2::dc_arrow];
}

BaseUISurface g_BaseUISurface;

using vgui2::ISurface;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( BaseUISurface, ISurface, "VGUI_Surface026", g_BaseUISurface );

BaseUISurface::~BaseUISurface()
{
	m_bAllowJavaScript = false;
	m_CustomFontFileNames.RemoveAll();
}

void BaseUISurface::Init( VPANEL embeddedPanel, IEngineSurface* engineSurface, IHTMLChromeController* chromeController )
{
	_engineSurface = engineSurface;
	SetEmbeddedPanel( embeddedPanel );
	if ( SteamApps() )
		SetLanguage( SteamApps()->GetCurrentGameLanguage() );
	m_pEmulatedMouseImage = vgui2::scheme()->GetImage( "gfx/vgui/mouse", false );
	Cvar_RegisterVariable( &sdl_double_click_size );
	Cvar_RegisterVariable( &sdl_double_click_time );

	m_pChromeController = chromeController;
	if ( m_pChromeController )
	{
		m_pChromeController->Init( "htmlcache", "htmlcookies" );
		m_pChromeController->SetCefThreadTargetFrameRate( 60 );
	}
}

void BaseUISurface::Shutdown()
{
	if ( m_pChromeController )
		m_pChromeController->Shutdown();

#if defined( _WIN32 )
	for ( int i = 0; i < m_CustomFontFileNames.Count(); i++ )
		RemoveFontResourceExA( m_CustomFontFileNames[i].String(), FR_PRIVATE, NULL );
#endif
	m_CustomFontFileNames.RemoveAll();

	for ( int i = 0; i < MAX_DEFAULT_CURSORS; i++ )
	{
		if ( staticDefaultCursor[i] )
			SDL_FreeCursor( staticDefaultCursor[i] );
	}
	staticPopupList.RemoveAll();
}

void BaseUISurface::SurfaceGetCursorPos( int& x, int& y )
{
	SDL_GetMouseState( &x, &y );
	if ( VideoMode_IsWindowed() )
		return;

	int windowWide, windowTall;
	int videoWide, videoTall;
	SDL_GetWindowSize( (SDL_Window*)game->GetMainWindow(), &windowWide, &windowTall );
	VideoMode_GetCurrentVideoMode( &videoWide, &videoTall, NULL );

	x = (int)( ( (float)videoWide / windowWide ) * x );
	y = (int)( ( (float)videoTall / windowTall ) * y );
	x = (int)( x + ( GetXMouseAspectRatioAdjustment() - 1.0 ) * (float)( x - videoWide / 2 ) );
	y = (int)( y + ( GetYMouseAspectRatioAdjustment() - 1.0 ) * (float)( y - videoTall / 2 ) );
}

void BaseUISurface::SurfaceSetCursorPos( int x, int y )
{
	if ( !VideoMode_IsWindowed() )
	{
		int windowWide, windowTall;
		int videoWide, videoTall;
		SDL_GetWindowSize( (SDL_Window*)game->GetMainWindow(), &windowWide, &windowTall );
		VideoMode_GetCurrentVideoMode( &videoWide, &videoTall, NULL );

		int scaledX = (int)( ( (float)videoWide / windowWide ) * x );
		int scaledY = (int)( ( (float)videoTall / windowTall ) * y );
		x = (int)( ( GetXMouseAspectRatioAdjustment() - 1.0 ) * (float)( scaledX - videoWide / 2 ) + scaledX );
		y = (int)( ( GetYMouseAspectRatioAdjustment() - 1.0 ) * (float)( scaledY - videoTall / 2 ) + scaledY );
	}

	SDL_WarpMouseInWindow( (SDL_Window*)game->GetMainWindow(), x, y );
}

void BaseUISurface::SetCursor( vgui2::HCursor cursor )
{
	_currentCursor = cursor;
	if ( _cursorLocked )
		return;

	bool wasVisible = s_bCursorVisible;
	s_bCursorVisible = true;
	switch ( cursor )
	{
	case vgui2::dc_user:
		s_bCursorVisible = false;
		staticCurrentCursor = NULL;
		break;
	case vgui2::dc_none:
		s_bCursorVisible = false;
		staticCurrentCursor = staticDefaultCursor[cursor];
		break;
	case vgui2::dc_arrow:
	case vgui2::dc_ibeam:
	case vgui2::dc_hourglass:
	case vgui2::dc_crosshair:
	case vgui2::dc_up:
	case vgui2::dc_sizenwse:
	case vgui2::dc_sizenesw:
	case vgui2::dc_sizewe:
	case vgui2::dc_sizens:
	case vgui2::dc_sizeall:
	case vgui2::dc_no:
	case vgui2::dc_hand:
		staticCurrentCursor = staticDefaultCursor[cursor];
		[[fallthrough]];
	default:
		SDL_SetCursor( staticCurrentCursor );
		break;
	}

	bool useRawInput = m_rawinput.value && BUsesSDLInput();
	if ( s_bUsedRawInputLast != useRawInput )
	{
		if ( wasVisible )
			SDL_SetRelativeMouseMode( SDL_FALSE );
		else
			SDL_ShowCursor( SDL_ENABLE );
		s_bUsedRawInputLast = useRawInput;
	}

	if ( wasVisible && !s_bCursorVisible )
	{
		if ( useRawInput )
			SDL_SetRelativeMouseMode( SDL_TRUE );
		else
			SDL_ShowCursor( SDL_DISABLE );
		if ( cl_mousegrab.value )
			SDL_SetWindowGrab( (SDL_Window*)game->GetMainWindow(), SDL_TRUE );

		SDL_PumpEvents();
		int dx, dy;
		SDL_GetRelativeMouseState( &dx, &dy );
		game->SetCursorVisible( s_bCursorVisible );
	}

	if ( !wasVisible && s_bCursorVisible )
	{
		if ( useRawInput )
			SDL_SetRelativeMouseMode( SDL_FALSE );
		else
			SDL_ShowCursor( SDL_ENABLE );
		if ( cl_mousegrab.value )
			SDL_SetWindowGrab( (SDL_Window*)game->GetMainWindow(), SDL_FALSE );

		SDL_PumpEvents();
		int dx, dy;
		SDL_GetRelativeMouseState( &dx, &dy );
		game->SetCursorVisible( s_bCursorVisible );
	}
}

bool BaseUISurface::m_bTranslateExtendedKeys = true;

vgui2::VPANEL BaseUISurface::GetEmbeddedPanel()
{
	return _embeddedPanel;
}

void BaseUISurface::SetEmbeddedPanel( vgui2::VPANEL panel )
{
	_embeddedPanel = panel;
}

void BaseUISurface::DrawSetTextFont( vgui2::HFont font )
{
	m_hCurrentFont = font;
}

bool BaseUISurface::IsCursorVisible()
{
	return _currentCursor != vgui2::dc_none;
}

void BaseUISurface::UnlockCursor()
{
	_cursorLocked = false;
}

void BaseUISurface::LockCursor()
{
	_cursorLocked = true;
}

void BaseUISurface::SetTranslateExtendedKeys( bool state )
{
	m_bTranslateExtendedKeys = state;
}

bool BaseUISurface::NeedKBInput()
{
	return _needKB;
}

bool BaseUISurface::HasCursorPosFunctions()
{
	return true;
}

void BaseUISurface::SetAllowHTMLJavaScript( bool state )
{
	m_bAllowJavaScript = state;
}

const char* BaseUISurface::GetLanguage()
{
	return m_szLanguage;
}

void BaseUISurface::SetProportionalBase( int width, int height )
{
	_proportionalBaseWidth = width;
	_proportionalBaseHeight = height;
}

void BaseUISurface::SetHDProportionalBase( int width, int height )
{
	_proportionalBaseWidth_HD = width;
	_proportionalBaseHeight_HD = height;
}

void BaseUISurface::ResetCurrentTexture()
{
	m_iCurrentTexture = 0;
}

void BaseUISurface::ResetViewPort()
{
	_engineSurface->resetViewPort();
}

void BaseUISurface::DrawFilledRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawFilledRect( x0, y0, x1, y1 );
}

void BaseUISurface::DrawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawOutlinedRect( x0, y0, x1, y1 );
}

void BaseUISurface::DrawLine( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawLine( x0, y0, x1, y1 );
}

void BaseUISurface::DrawPolyLine( int* x, int* y, int count )
{
	_engineSurface->drawPolyLine( x, y, count );
}

void BaseUISurface::DrawTexturedPolygon( vgui2::VGuiVertex* vertices, int count )
{
	_engineSurface->drawTexturedPolygon( vertices, count );
}

void BaseUISurface::DrawSetColor( int red, int green, int blue, int alpha )
{
	_engineSurface->drawSetColor( red, green, blue, 255 - alpha );
}

void BaseUISurface::DrawSetTextColor( int red, int green, int blue, int alpha )
{
	_engineSurface->drawSetTextColor( red, green, blue, 255 - alpha );
}

void BaseUISurface::DrawSetTextPos( int x, int y )
{
	_engineSurface->drawSetTextPos( x, y );
}

void BaseUISurface::DrawGetTextPos( int& x, int& y )
{
	_engineSurface->drawGetTextPos( x, y );
}

void BaseUISurface::DrawFlushText()
{
	_engineSurface->drawFlushText();
}

void BaseUISurface::DrawTexturedRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawTexturedRect( x0, y0, x1, y1 );
}

void BaseUISurface::DrawTexturedRectAdd( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawTexturedRectAdd( x0, y0, x1, y1 );
}

void BaseUISurface::DrawGetTextureSize( int texture, int& width, int& height )
{
	_engineSurface->drawGetTextureSize( texture, width, height );
}

void BaseUISurface::DrawSetTextureFile( int texture, const char* filename, int hardwareFilter, bool forceReload )
{
	_engineSurface->drawSetTextureFile( texture, filename, hardwareFilter, forceReload );
}

void BaseUISurface::DrawSetTextureRGBA(
	int texture, const unsigned char* rgba, int width, int height, int hardwareFilter, bool )
{
	_engineSurface->drawSetTextureRGBA( texture, (const char*)rgba, width, height, hardwareFilter, 0 );
}

void BaseUISurface::DrawUpdateRegionTextureBGRA(
	int texture, int x, int y, const unsigned char* bgra, int width, int height )
{
	_engineSurface->drawUpdateRegionTextureBGRA( texture, x, y, bgra, width, height );
}

void BaseUISurface::DrawSetTextureBGRA( int texture, const unsigned char* bgra, int width, int height )
{
	_engineSurface->drawSetTextureBGRA( texture, (const char*)bgra, width, height, 1, 0 );
}

void BaseUISurface::DrawSetColor( Color color )
{
	DrawSetColor( color.r(), color.g(), color.b(), color.a() );
}

void BaseUISurface::DrawSetTextColor( Color color )
{
	DrawSetTextColor( color.r(), color.g(), color.b(), color.a() );
}

void BaseUISurface::DrawSetTexture( int texture )
{
	if ( texture != m_iCurrentTexture )
	{
		_engineSurface->drawSetTexture( texture );
		m_iCurrentTexture = texture;
	}
}

int BaseUISurface::CreateNewTextureID( bool )
{
	return _engineSurface->createNewTextureID();
}

bool BaseUISurface::IsTextureIDValid( int texture )
{
	return _engineSurface->isTextureIdValid( texture ) != 0;
}

void BaseUISurface::GetScreenSize( int& width, int& height )
{
	SCREENINFO screenInfo;
	screenInfo.iSize = sizeof( screenInfo );
	gEngfuncs.pfnGetScreenInfo( &screenInfo );
	width = screenInfo.iWidth;
	height = screenInfo.iHeight;
}

void BaseUISurface::GetWorkspaceBounds( int& x, int& y, int& width, int& height )
{
	x = 0;
	y = 0;
	vgui2::ipanel()->GetSize( _embeddedPanel, width, height );
}

void BaseUISurface::GetAbsoluteWindowBounds( int& x, int& y, int& width, int& height )
{
	x = m_iSurfaceBounds[0];
	y = m_iSurfaceBounds[1];
	width = m_iSurfaceBounds[2];
	height = m_iSurfaceBounds[3];
}

void BaseUISurface::GetProportionalBase( int& width, int& height )
{
	int screenWide, screenTall;
	GetScreenSize( screenWide, screenTall );
	width = _proportionalBaseWidth < screenWide ? _proportionalBaseWidth : screenWide;
	height = _proportionalBaseHeight < screenTall ? _proportionalBaseHeight : screenTall;
}

void BaseUISurface::GetHDProportionalBase( int& width, int& height )
{
	int screenWide, screenTall;
	GetScreenSize( screenWide, screenTall );
	width = _proportionalBaseWidth_HD < screenWide ? _proportionalBaseWidth_HD : screenWide;
	height = _proportionalBaseHeight_HD < screenTall ? _proportionalBaseHeight_HD : screenTall;
}

void BaseUISurface::RunFrame()
{
	if ( m_pChromeController )
		m_pChromeController->RunFrame();
}

void BaseUISurface::RestrictPaintToSinglePanel( vgui2::VPANEL panel )
{
	_restrictedPanel = panel;
	vgui2::input()->SetAppModalSurface( panel );
}

void BaseUISurface::SetTopLevelFocus( vgui2::VPANEL panel )
{
	while ( panel )
	{
		if ( vgui2::ipanel()->IsPopup( panel ) )
		{
			BringToFront( panel );
			return;
		}
		panel = vgui2::ipanel()->GetParent( panel );
	}
}

void BaseUISurface::AddPanel( vgui2::VPANEL panel )
{
	if ( vgui2::ipanel()->IsPopup( panel ) )
		CreatePopup( panel, false, false, false, true, true );
}

void BaseUISurface::SolveTraverse( vgui2::VPANEL panel, bool forceApplySchemeSettings )
{
	InternalSchemeSettingsTraverse( panel, forceApplySchemeSettings );
	InternalThinkTraverse( panel );
	InternalSolveTraverse( panel );
}

void BaseUISurface::InternalSchemeSettingsTraverse( vgui2::VPANEL panel, bool forceApplySchemeSettings )
{
	for ( int i = 0; i < vgui2::ipanel()->GetChildCount( panel ); i++ )
	{
		vgui2::VPANEL child = vgui2::ipanel()->GetChild( panel, i );
		if ( forceApplySchemeSettings || vgui2::ipanel()->IsVisible( child ) )
			InternalSchemeSettingsTraverse( child, forceApplySchemeSettings );
	}
	vgui2::ipanel()->PerformApplySchemeSettings( panel );
}

void BaseUISurface::InternalThinkTraverse( vgui2::VPANEL panel )
{
	vgui2::ipanel()->Think( panel );
	for ( int i = 0; i < vgui2::ipanel()->GetChildCount( panel ); i++ )
	{
		vgui2::VPANEL child = vgui2::ipanel()->GetChild( panel, i );
		if ( vgui2::ipanel()->IsVisible( child ) )
			InternalThinkTraverse( child );
	}
}

void BaseUISurface::InternalSolveTraverse( vgui2::VPANEL panel )
{
	vgui2::ipanel()->Solve( panel );
	for ( int i = 0; i < vgui2::ipanel()->GetChildCount( panel ); i++ )
	{
		vgui2::VPANEL child = vgui2::ipanel()->GetChild( panel, i );
		if ( vgui2::ipanel()->IsVisible( child ) )
			InternalSolveTraverse( child );
	}
}

vgui2::VPANEL BaseUISurface::GetTopmostPopup()
{
	if ( staticPopupList.GetCount() > 0 )
		return staticPopupList[staticPopupList.GetCount() - 1];
	return 0;
}

int BaseUISurface::GetPopupCount()
{
	return staticPopupList.GetCount();
}

vgui2::VPANEL BaseUISurface::GetPopup( int index )
{
	return staticPopupList[index];
}

bool BaseUISurface::ShouldPaintChildPanel( vgui2::VPANEL panel )
{
	if ( _restrictedPanel && !vgui2::ipanel()->HasParent( panel, _restrictedPanel ) )
		return false;

	bool isPopup = vgui2::ipanel()->IsPopup( panel );
	if ( isPopup )
	{
		int index = staticPopupList.FindElement( panel );
		if ( index != -1 )
			vgui2::ipanel()->Render_SetPopupVisible( staticPopupList[index], true );
	}
	return !isPopup;
}

void BaseUISurface::ReleasePanel( vgui2::VPANEL panel )
{
	staticPopupList.RemoveElement( panel );
	if ( panel == _restrictedPanel )
		_restrictedPanel = 0;
}

void BaseUISurface::CreatePopup( vgui2::VPANEL panel, bool, bool, bool, bool mouseInput, bool keyboardInput )
{
	if ( !vgui2::ipanel()->GetParent( panel ) )
		vgui2::ipanel()->SetParent( panel, GetEmbeddedPanel() );

	vgui2::ipanel()->SetPopup( panel, true );
	if ( !staticPopupList.HasElement( panel ) )
		staticPopupList.AddElement( panel );
	vgui2::ipanel()->SetKeyBoardInputEnabled( panel, keyboardInput );
	vgui2::ipanel()->SetMouseInputEnabled( panel, mouseInput );
}

void BaseUISurface::BringToFront( vgui2::VPANEL panel )
{
	vgui2::ipanel()->MoveToFront( panel );
	if ( vgui2::ipanel()->IsPopup( panel ) )
		staticPopupList.MoveElementToEnd( panel );
}

void BaseUISurface::MovePopupToFront( vgui2::VPANEL panel )
{
	staticPopupList.MoveElementToEnd( panel );
}

void BaseUISurface::MovePopupToBack( vgui2::VPANEL panel )
{
	staticPopupList.RemoveElement( panel );
	staticPopupList.InsertElementAt( panel, 0 );
}

void BaseUISurface::PaintTraverse( vgui2::VPANEL panel )
{
	if ( !vgui2::ipanel()->IsVisible( panel ) )
		return;

	if ( panel != GetEmbeddedPanel() )
	{
		vgui2::ipanel()->PaintTraverse( panel, true, true );
		return;
	}

	if ( _restrictedPanel )
		panel = _restrictedPanel;

	for ( int i = 0; i < staticPopupList.GetCount(); i++ )
		vgui2::ipanel()->Render_SetPopupVisible( staticPopupList[i], false );

	vgui2::ipanel()->PaintTraverse( panel, true, true );

	for ( int i = 0; i < staticPopupList.GetCount(); i++ )
	{
		if ( vgui2::ipanel()->Render_GetPopupVisible( staticPopupList[i] ) )
			vgui2::ipanel()->PaintTraverse( staticPopupList[i], true, true );
	}
}

void BaseUISurface::PushMakeCurrent( vgui2::VPANEL panel, bool useInsets )
{
	int insets[4] = {};
	int absExtents[4];
	int clipRect[4];
	int wide, tall;

	m_iCurrentTexture = 0;
	if ( useInsets )
		vgui2::ipanel()->GetInset( panel, insets[0], insets[1], insets[2], insets[3] );

	vgui2::ipanel()->GetAbsPos( panel, absExtents[0], absExtents[1] );
	vgui2::ipanel()->GetSize( panel, wide, tall );
	absExtents[2] = absExtents[0] + wide;
	absExtents[3] = absExtents[1] + tall;

	vgui2::ipanel()->GetClipRect( panel, clipRect[0], clipRect[1], clipRect[2], clipRect[3] );
	_engineSurface->pushMakeCurrent( insets, absExtents, clipRect, false );
}

void BaseUISurface::PopMakeCurrent( vgui2::VPANEL )
{
	_engineSurface->popMakeCurrent();
}

bool BaseUISurface::AddCustomFontFile( const char* filename )
{
#if defined( _WIN32 )
	char localPath[MAX_PATH];
	vgui2::filesystem()->GetLocalPath( filename, localPath, sizeof( localPath ) );
	m_CustomFontFileNames.AddToTail( CUtlSymbol( filename ) );
	return AddFontResourceExA( localPath, FR_PRIVATE, NULL ) > 0 || AddFontResourceA( localPath ) > 0;
#else
	char localPath[4096];
	vgui2::filesystem()->GetLocalPath( filename, localPath, sizeof( localPath ) );
	m_CustomFontFileNames.AddToTail( CUtlSymbol( filename ) );
	return FcInit() == FcTrue && FcConfigAppFontAddFile( NULL, (const FcChar8*)localPath ) == FcTrue;
#endif
}

void BaseUISurface::DrawSetSubTextureRGBA( int texture, int x, int y, const unsigned char* rgba, int width, int height )
{
	_engineSurface->drawSetSubTextureRGBA( texture, x, y, rgba, width, height );
}

void BaseUISurface::DrawSetTextureRGBAWithAlphaChannel(
	int texture, const unsigned char* rgba, int width, int height, int hardwareFilter, bool )
{
	_engineSurface->drawSetTextureRGBA( texture, (const char*)rgba, width, height, hardwareFilter, 1 );
}

void BaseUISurface::SetLanguage( const char* language )
{
	if ( language )
		strncpy( m_szLanguage, language, sizeof( m_szLanguage ) );
	else
		strcpy( m_szLanguage, "english" );
}

void BaseUISurface::SetSupportsEsc( bool state )
{
	m_bSupportsEsc = state;
}

void BaseUISurface::CreateBrowser( vgui2::VPANEL, IHTMLResponses* responses, bool, const char* )
{
	if ( m_pChromeController )
		m_pChromeController->CreateBrowser( responses, false, "Valve Half-Life" );
}

void BaseUISurface::RemoveBrowser( vgui2::VPANEL, IHTMLResponses* responses )
{
	if ( m_pChromeController )
		m_pChromeController->RemoveBrowser( responses );
}

IHTMLChromeController* BaseUISurface::AccessChromeHTMLController()
{
	return m_pChromeController;
}

vgui2::HFont BaseUISurface::CreateFont()
{
	return FontManager().CreateFont();
}

bool BaseUISurface::AddGlyphSetToFont(
	vgui2::HFont font, const char* name, int tall, int weight, int blur, int scanlines, int flags, int, int )
{
	return FontManager().AddGlyphSetToFont( font, name, tall, weight, blur, scanlines, flags );
}

int BaseUISurface::GetFontTall( vgui2::HFont font )
{
	return FontManager().GetFontTall( font );
}

void BaseUISurface::GetCharABCwide( vgui2::HFont font, int ch, int& a, int& b, int& c )
{
	FontManager().GetCharABCwide( font, ch, a, b, c );
}

int BaseUISurface::GetCharacterWidth( vgui2::HFont font, int ch )
{
	return FontManager().GetCharacterWidth( font, ch );
}

void BaseUISurface::GetTextSize( vgui2::HFont font, const wchar_t* text, int& width, int& height )
{
	FontManager().GetTextSize( font, text, width, height );
}

int BaseUISurface::GetFontAscent( vgui2::HFont font, wchar_t ch )
{
	return FontManager().GetFontAscent( font, ch );
}

int BaseUISurface::GetFontBlur( vgui2::HFont font )
{
	return FontManager().GetFontBlur( font );
}

bool BaseUISurface::IsAdditive( vgui2::HFont font )
{
	return FontManager().IsAdditive( font );
}

bool BaseUISurface::IsMinimized( VPANEL panel )
{
	return vgui2::ipanel()->Plat( panel ) == MINIMIZED_PLAT;
}

void BaseUISurface::SetMinimized( VPANEL panel, bool state )
{
	if ( state )
	{
		vgui2::ipanel()->SetPlat( panel, MINIMIZED_PLAT );
		vgui2::ipanel()->SetVisible( panel, false );
	}
	else
	{
		vgui2::ipanel()->SetPlat( panel, NULL );
	}
}

void BaseUISurface::SetForegroundWindow( VPANEL panel )
{
	BringToFront( panel );
}

void BaseUISurface::PaintHTMLWindow( IHTML* )
{
}

void BaseUISurface::DeleteHTMLWindow( IHTML* )
{
}

void BaseUISurface::SetAsTopMost( VPANEL, bool )
{
}

void BaseUISurface::SetPanelVisible( VPANEL, bool )
{
}

void BaseUISurface::FlashWindow( VPANEL, bool )
{
}

void BaseUISurface::SetTitle( VPANEL, const wchar_t* )
{
}

void BaseUISurface::SetAsToolBar( VPANEL, bool )
{
}

vgui2::VPANEL BaseUISurface::GetNotifyPanel()
{
	return 0;
}

bool BaseUISurface::RecreateContext( VPANEL )
{
	return true;
}

void BaseUISurface::SwapBuffers( VPANEL )
{
}

void BaseUISurface::Invalidate( VPANEL )
{
}

void BaseUISurface::SetModalPanel( VPANEL )
{
}

void BaseUISurface::PanelRequestFocus( VPANEL )
{
}

void BaseUISurface::SetNotifyIcon( VPANEL, HTexture, VPANEL, const char* )
{
}

void BaseUISurface::EnableMouseCapture( VPANEL, bool )
{
}

bool BaseUISurface::IsWithin( int, int )
{
	return true;
}

vgui2::IHTML* BaseUISurface::CreateHTMLWindow( IHTMLEvents*, VPANEL )
{
	return NULL;
}

void BaseUISurface::DrawPrintText( const wchar_t* text, int length )
{
	if ( !text )
		return;

	for ( int i = 0; i < length; i++ )
		DrawUnicodeChar( text[i] );
}

vgui2::KeyCode GetVGUI2KeyCodeForBind( const char* binding )
{
	const char* keyName = Key_NameForBinding( binding );
	if ( !keyName )
		return vgui2::KEY_NONE;

	char buf[256];
	strcpy( buf, keyName );
#if defined( _WIN32 )
	_strupr( buf );
#endif

	SDL_Keycode keyCode = GetSDLKeycodeFromEngineKey( Key_StringToKeynum( buf ) );
	if ( keyCode > 0 )
		return KeyCode_VirtualKeyToVGUI( keyCode );
	return vgui2::KEY_NONE;
}

void BaseUISurface::DrawUnicodeChar( wchar_t ch )
{
	if ( !m_hCurrentFont )
		return;

	int x, y;
	DrawGetTextPos( x, y );

	int tall = GetFontTall( m_hCurrentFont );
	int a, b, c;
	GetCharABCwide( m_hCurrentFont, ch, a, b, c );

	bool underlined = FontManager().GetFontUnderlined( m_hCurrentFont );
	x += a;
	int wide = b;
	if ( underlined )
	{
		wide = c + a + b;
		x -= a;
	}

	int texture = 0;
	float texCoords[4];
	if ( !g_FontTextureCache.GetTextureForChar( m_hCurrentFont, ch, &texture, texCoords ) )
		return;
	if ( texture != -1 )
		DrawSetTexture( texture );

	DrawPrintChar( x, y, wide, tall, texCoords[0], texCoords[1], texCoords[2], texCoords[3] );
	x += wide;
	if ( !underlined )
		x += c;
	DrawSetTextPos( x, y );
}

void BaseUISurface::DrawUnicodeCharAdd( wchar_t ch )
{
	if ( !m_hCurrentFont )
		return;

	int x, y;
	DrawGetTextPos( x, y );

	int tall = GetFontTall( m_hCurrentFont );
	int a, b, c;
	GetCharABCwide( m_hCurrentFont, ch, a, b, c );
	x += a;

	int texture = 0;
	float texCoords[4];
	if ( !g_FontTextureCache.GetTextureForChar( m_hCurrentFont, ch, &texture, texCoords ) )
		return;
	if ( texture != -1 )
		DrawSetTexture( texture );

	_engineSurface->drawPrintCharAdd( x, y, b, tall, texCoords[0], texCoords[1], texCoords[2], texCoords[3] );
	DrawSetTextPos( b + c + x, y );
}

bool BaseUISurface::setFullscreenMode( int, int, int )
{
	m_bFullScreen = true;
	return false;
}

void BaseUISurface::setWindowedMode()
{
	m_bFullScreen = false;
}

bool BaseUISurface::HasFocus()
{
	return true;
}

void BaseUISurface::SetAsTopMost( bool )
{
}

void BaseUISurface::SetAsToolBar( bool )
{
}

void BaseUISurface::EnableMouseCapture( bool )
{
}

void BaseUISurface::ApplyChanges()
{
}

void BaseUISurface::DrawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	_engineSurface->drawPrintChar( x, y, wide, tall, s0, t0, s1, t1 );
}

void BaseUISurface::SetNotifyIcon( vgui2::Image*, VPANEL, const char* )
{
}

bool BaseUISurface::SetWatchForComputerUse( bool )
{
	return false;
}

double BaseUISurface::GetTimeSinceLastUse()
{
	return 0.0;
}
