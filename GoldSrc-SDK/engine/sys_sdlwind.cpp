#if defined( SW_RENDERER )

#include "sys_sdlwind.h"

#include "common.h"
#include "filesystem.h"
#include "host.h"
#include "keys.h"
#include "cmd.h"
#include "conprint.h"
#include "cvar.h"
#include "model.h"
#include "iengine.h"
#include "ivideomode.h"
#include "cd.h"
#include "server.h"
#include "sys.h"
#include "vgui_intwrap2.h"
#include "webm_video.h"
#include "vgui2/BaseUISurface.h"
#include "cdll_int.h"
#include "client.h"
#include "../common/dll_state.h"

#include "../external/SDL2/SDL.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define HALFLIFE_APPID			70
#define GAME_ICON_BUFFER_BYTES	( 256 * 256 * 4 )

extern char g_szfullClientName[];
extern cvar_t cl_mousegrab;
extern cvar_t cl_skipvid;
extern cvar_t m_rawinput;
extern bool s_bCursorVisible;

#if defined( _WIN32 )
extern int g_bIsCZeroRitual;
extern int g_bIsHL1;
#endif
qboolean bPausedByCareer;
unsigned mouseCode;

bool BIsValveGame();

CGame g_Game;
IGame* game = &g_Game;

CGame::~CGame()
{
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CGame::CGame()
{
	m_y = 0;
	m_x = 0;
	m_height = 0;
	m_width = 0;
	m_bActiveApp = true;
	m_bMultiplayer = false;
	m_bExpectSyntheticMouseMotion = false;
	m_nMouseTargetX = 0;
	m_nMouseTargetY = 0;
	m_nWarpDelta = 0;
	m_bCursorVisible = true;
	m_bLegacyRender = false;
}

bool CGame::Init( void* )
{
	return true;
}

HWND CGame::GetMainWindow()
{
	return (HWND)m_hSDLWindow;
}

HWND* CGame::GetMainWindowAddress()
{
	return (HWND*)&m_hSDLWindow;
}

void CGame::SetWindowXY( int x, int y )
{
	m_x = x;
	m_y = y;
}

void CGame::GetWindowRect( int* x, int* y, int* w, int* h )
{
	if ( x )
		*x = m_x;
	if ( y )
		*y = m_y;
	if ( w )
		*w = m_width;
	if ( h )
		*h = m_height;
}

bool CGame::IsActiveApp()
{
	return m_bActiveApp;
}

bool CGame::IsMultiplayer()
{
	return m_bMultiplayer;
}

bool CGame::Shutdown()
{
	if ( m_hSDLRenderer && m_bCreatedRenderer )
		SDL_DestroyRenderer( m_hSDLRenderer );
	SDL_Window* window = m_hSDLWindow;
	m_hSDLRenderer = NULL;
	SDL_DestroyWindow( window );
	m_hSDLWindow = NULL;
	SDL_Quit();
	return true;
}

void CGame::PlayAVIAndWait( const char* fileName )
{
	WebMPlayer player;
	player.PlayVideo( fileName, m_hSDLWindow );
}

void CGame::SetCursorVisible( bool bState )
{
	m_bCursorVisible = bState;
	if ( !bState && cl_mousegrab.value == 0.0f )
		SDL_SetWindowGrab( m_hSDLWindow, SDL_FALSE );
}

void CGame::SetWindowSize( int w, int h )
{
	m_width = w;
	m_height = h;
	SDL_SetWindowSize( m_hSDLWindow, w, h );
	const int height = m_height;
	m_nMouseTargetX = m_width / 2;
	m_nMouseTargetY = height / 2;
	int warpDelta = height / 3;
	if ( warpDelta <= 199 )
		warpDelta = 200;
	m_nWarpDelta = warpDelta;
	ClearScreen();
}

void CGame::PlayStartupVideos()
{
	if ( cl_skipvid.value > 0.0f )
		return;

	Con_Printf( "Playing Startup Videos...\n" );
	char szTemp[512];
	_snprintf( szTemp, sizeof( szTemp ) - 1, "../%s/media/StartupVids.txt", com_gamedir );
	COM_FixSlashes( szTemp );

	int length = 0;
	char* pbuffer = (char*)COM_LoadFile( szTemp, 5, &length );
	if ( !pbuffer || length == 0 )
		return;

	SetCursorVisible( false );

	char szFileName[MAX_PATH];
	char* parse = pbuffer;
	while ( true )
	{
		parse = (char*)COM_Parse( parse );
		if ( Q_strlen( com_token ) <= 0 )
			break;
		FS_GetLocalPath( com_token, szFileName, sizeof( szFileName ) );
		PlayAVIAndWait( szFileName );
	}

	free( pbuffer );
	SetCursorVisible( true );
}

qboolean BUsesSDLInput()
{
	static bool s_bUseRawInput;

	s_bUseRawInput = BIsValveGame();
	if ( Q_strstr( g_szfullClientName, "valve\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	if ( Q_strstr( g_szfullClientName, "cstrike\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	if ( Q_strstr( g_szfullClientName, "tfc\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	if ( Q_strstr( g_szfullClientName, "dod\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	if ( Q_strstr( g_szfullClientName, "gearbox\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	if ( Q_strstr( g_szfullClientName, "bshift\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	if ( Q_strstr( g_szfullClientName, "czeror\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	if ( Q_strstr( g_szfullClientName, "czero\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	if ( Q_strstr( g_szfullClientName, "opfor\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	if ( Q_strstr( g_szfullClientName, "dmc\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	if ( Q_strstr( g_szfullClientName, "ricochet\\cl_dlls\\client" ) )
		s_bUseRawInput = true;
	return s_bUseRawInput;
}

void GetWindowNameFromGameDir( char* output, int outputBufferSize )
{
	FileHandle_t file = FS_Open( "liblist.gam", "rt" );
	if ( !file )
		return;

	while ( !FS_EndOfFile( file ) )
	{
		char line[512];
		line[0] = 0;
		FS_ReadLine( line, sizeof( line ) - 1, file );
		line[sizeof( line ) - 1] = 0;
		if ( Q_strnicmp( line, "game", 4 ) != 0 )
			continue;

		char* openQuote = strchr( line, '\"' );
		if ( !openQuote )
			break;

		const char* nameStart = openQuote + 1;
		char* closeQuote = strchr( openQuote + 1, '\"' );
		if ( !closeQuote )
			break;

		const int nameLength = (int)( closeQuote - nameStart );
		if ( nameLength < outputBufferSize )
		{
			strncpy( output, nameStart, nameLength );
			output[nameLength] = 0;
		}
		break;
	}

	FS_Close( file );
}

void SDL_SetAttributes( bool tryNewCtx )
{
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 4 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 4 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 4 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 0 );

	if ( videomode->IsWindowedMode() )
	{
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 4 );
	}

	if ( tryNewCtx )
	{
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 2 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
	}
}

bool CGame::CreateGameWindow()
{
	if ( COM_CheckParm( "-noontop" ) )
		SDL_SetHint( "SDL_ALLOW_TOPMOST", "0" );
	SDL_InitSubSystem( SDL_INIT_EVERYTHING );
	SDL_SetAttributes( true );

	char gameWindowName[512];
	Q_strcpy( gameWindowName, "Half-Life" );
	GetWindowNameFromGameDir( gameWindowName, sizeof( gameWindowName ) );

	const Uint32 windowFlags = COM_CheckParm( "-noborder" ) == 0
		? SDL_WINDOW_HIDDEN : SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS;
	m_hSDLWindow = SDL_CreateWindow( gameWindowName, 0, 0, 640, 480, windowFlags );
	if ( !m_hSDLWindow )
	{
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 3 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 3 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 3 );
		m_hSDLWindow = SDL_CreateWindow( gameWindowName, 0, 0, 640, 480, windowFlags );
		if ( !m_hSDLWindow )
			Sys_Error( "Failed to create SDL Window" );
	}

	byte* buffer = (byte*)malloc( GAME_ICON_BUFFER_BYTES );
	int width = 0;
	int height = 0;
	if ( LoadTGA( "game.tga", buffer, GAME_ICON_BUFFER_BYTES, &width, &height ) )
	{
		if ( GetGameAppID() != HALFLIFE_APPID )
		{
			byte* rowScratch = (byte*)malloc( 4 * width );
			for ( int row = 0; row < height / 2; ++row )
			{
				memcpy( rowScratch, &buffer[4 * width * row], 4 * width );
				memcpy( &buffer[4 * width * row], &buffer[4 * width * ( height - row )], 4 * width );
				memcpy( &buffer[4 * width * ( height - row )], rowScratch, 4 * width );
			}
			free( rowScratch );
		}

		SDL_Surface* iconSurface = SDL_CreateRGBSurfaceFrom( buffer, width, height, 32,
			4 * width, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000 );
		if ( iconSurface )
		{
			SDL_SetWindowIcon( m_hSDLWindow, iconSurface );
			SDL_FreeSurface( iconSurface );
		}
	}
	free( buffer );

	SDL_ShowWindow( m_hSDLWindow );
	SDL_StartTextInput();

	m_bCreatedRenderer = false;
	m_hSDLRenderer = SDL_GetRenderer( m_hSDLWindow );
	if ( !m_hSDLRenderer )
	{
		m_hSDLRenderer = SDL_CreateRenderer( m_hSDLWindow, -1, 0 );
		m_bCreatedRenderer = true;
	}
	ClearScreen();

	m_nMouseTargetX = 320;
	m_nMouseTargetY = 240;
	m_nWarpDelta = 213;
	return true;
}

void CGame::SetActiveApp( bool active )
{
	m_bActiveApp = active;
}

void CGame::ClearScreen()
{
	if ( m_hSDLRenderer )
	{
		SDL_SetRenderDrawColor( m_hSDLRenderer, 0, 0, 0, 0 );
		SDL_RenderClear( m_hSDLRenderer );
		SDL_RenderPresent( m_hSDLRenderer );
	}
}

void CGame::AppActivate( bool fActive )
{
	if ( fActive )
	{
		fprintf( stderr, "AppActive: active\n" );
		if ( host_initialized )
		{
			ClearIOStates();
#if defined( _WIN32 )
			cdaudio->Resume();
#else
			cdaudio->MP3_Resume_Audio();
#endif
			if ( videomode->GetInitialized() )
			{
				if ( !VGuiWrap2_IsGameUIVisible() )
					ClientDLL_ActivateMouse();
				g_BaseUISurface.CalculateMouseVisible();
				int dx = 0;
				int dy = 0;
				g_BaseUISurface.GetMouseDelta( &dx, &dy );
			}
			if ( VGuiWrap2_IsInCareerMatch() && bPausedByCareer )
			{
				Cmd_ExecuteString( "unpause", src_command );
				bPausedByCareer = false;
			}
		}
	}
	else
	{
		fprintf( stderr, "AppActive: not active\n" );
		if ( host_initialized )
		{
			if ( videomode->GetInitialized() && !VGuiWrap2_IsGameUIVisible() )
				ClientDLL_DeactivateMouse();
			cdaudio->Pause();
			if ( VGuiWrap2_IsInCareerMatch() && !cl.paused )
			{
				Cmd_ExecuteString( "setpause", src_command );
				bPausedByCareer = true;
			}
		}
	}
	m_bActiveApp = fActive;
}

void CGame::SleepUntilInput( int time )
{
	SDL_Event ev;
	SDL_PumpEvents();
	if ( SDL_WaitEventTimeout( &ev, time ) <= 0 )
		return;

	do
	{
		if ( ev.type == SDL_TEXTINPUT )
		{
			if ( key_dest == key_message )
			{
				int copyLength = (int)strlen( ev.text.text );
				if ( KEY_CHAT_BUFFER_BYTES - 1 - chat_bufferlen <= copyLength )
					copyLength = KEY_CHAT_BUFFER_BYTES - 1 - chat_bufferlen;
				Q_strncpy( &chat_buffer[chat_bufferlen], ev.text.text, copyLength );
				chat_bufferlen += copyLength;
				chat_buffer[chat_bufferlen] = 0;
			}
		}
		else if ( ev.type <= SDL_TEXTINPUT )
		{
			if ( ev.type == SDL_WINDOWEVENT )
			{
				switch ( ev.window.event )
				{
				case SDL_WINDOWEVENT_SHOWN:
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					g_Game.AppActivate( true );
					break;
				case SDL_WINDOWEVENT_HIDDEN:
				case SDL_WINDOWEVENT_FOCUS_LOST:
					g_Game.AppActivate( false );
					break;
				case SDL_WINDOWEVENT_MOVED:
					if ( videomode->IsWindowedMode() )
					{
						game->SetWindowXY( ev.window.data1, ev.window.data2 );
						videomode->UpdateWindowPosition();
					}
					break;
				case SDL_WINDOWEVENT_RESIZED:
					game->SetWindowSize( ev.window.data1, ev.window.data2 );
					videomode->UpdateWindowPosition();
					break;
				case SDL_WINDOWEVENT_CLOSE:
					if ( eng->GetState() == DLL_ACTIVE )
						eng->SetQuitting( 1 );
					if ( m_hSDLRenderer && m_bCreatedRenderer )
						SDL_DestroyRenderer( m_hSDLRenderer );
					SDL_DestroyWindow( m_hSDLWindow );
					m_hSDLRenderer = NULL;
					m_hSDLWindow = NULL;
					return;
				default:
					break;
				}
			}
			else if ( ev.type <= SDL_WINDOWEVENT )
			{
				if ( ev.type == SDL_QUIT && eng->GetState() == DLL_ACTIVE )
					eng->SetQuitting( 1 );
			}
			else if ( ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP )
			{
				int key = 0;
				int scancode = ev.key.keysym.scancode;
				if ( scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_RGUI )
				{
					switch ( scancode )
					{
					case SDL_SCANCODE_TAB:
						key = K_TAB;
						break;
					case SDL_SCANCODE_RETURN:
						key = K_ENTER;
						break;
					case SDL_SCANCODE_ESCAPE:
						key = K_ESCAPE;
						break;
					case SDL_SCANCODE_SPACE:
						key = K_SPACE;
						break;
					case SDL_SCANCODE_APOSTROPHE:
						key = '\'';
						break;
					case SDL_SCANCODE_COMMA:
						key = ',';
						break;
					case SDL_SCANCODE_MINUS:
						key = '-';
						break;
					case SDL_SCANCODE_PERIOD:
						key = '.';
						break;
					case SDL_SCANCODE_SLASH:
						key = '/';
						break;
					case SDL_SCANCODE_0:
						key = '0';
						break;
					case SDL_SCANCODE_1:
						key = '1';
						break;
					case SDL_SCANCODE_2:
						key = '2';
						break;
					case SDL_SCANCODE_3:
						key = '3';
						break;
					case SDL_SCANCODE_4:
						key = '4';
						break;
					case SDL_SCANCODE_5:
						key = '5';
						break;
					case SDL_SCANCODE_6:
						key = '6';
						break;
					case SDL_SCANCODE_7:
						key = '7';
						break;
					case SDL_SCANCODE_8:
						key = '8';
						break;
					case SDL_SCANCODE_9:
						key = '9';
						break;
					case SDL_SCANCODE_SEMICOLON:
						key = ';';
						break;
					case SDL_SCANCODE_EQUALS:
						key = '=';
						break;
					case SDL_SCANCODE_LEFTBRACKET:
						key = '[';
						break;
					case SDL_SCANCODE_BACKSLASH:
						key = '\\';
						break;
					case SDL_SCANCODE_RIGHTBRACKET:
						key = ']';
						break;
					case SDL_SCANCODE_GRAVE:
						key = '`';
						break;
					case SDL_SCANCODE_A:
						key = 'a';
						break;
					case SDL_SCANCODE_B:
						key = 'b';
						break;
					case SDL_SCANCODE_C:
						key = 'c';
						break;
					case SDL_SCANCODE_D:
						key = 'd';
						break;
					case SDL_SCANCODE_E:
						key = 'e';
						break;
					case SDL_SCANCODE_F:
						key = 'f';
						break;
					case SDL_SCANCODE_G:
						key = 'g';
						break;
					case SDL_SCANCODE_H:
						key = 'h';
						break;
					case SDL_SCANCODE_I:
						key = 'i';
						break;
					case SDL_SCANCODE_J:
						key = 'j';
						break;
					case SDL_SCANCODE_K:
						key = 'k';
						break;
					case SDL_SCANCODE_L:
						key = 'l';
						break;
					case SDL_SCANCODE_M:
						key = 'm';
						break;
					case SDL_SCANCODE_N:
						key = 'n';
						break;
					case SDL_SCANCODE_O:
						key = 'o';
						break;
					case SDL_SCANCODE_P:
						key = 'p';
						break;
					case SDL_SCANCODE_Q:
						key = 'q';
						break;
					case SDL_SCANCODE_R:
						key = 'r';
						break;
					case SDL_SCANCODE_S:
						key = 's';
						break;
					case SDL_SCANCODE_T:
						key = 't';
						break;
					case SDL_SCANCODE_U:
						key = 'u';
						break;
					case SDL_SCANCODE_V:
						key = 'v';
						break;
					case SDL_SCANCODE_W:
						key = 'w';
						break;
					case SDL_SCANCODE_X:
						key = 'x';
						break;
					case SDL_SCANCODE_Y:
						key = 'y';
						break;
					case SDL_SCANCODE_Z:
						key = 'z';
						break;
					case SDL_SCANCODE_BACKSPACE:
						key = K_BACKSPACE;
						break;
					case SDL_SCANCODE_UP:
						key = K_UPARROW;
						break;
					case SDL_SCANCODE_DOWN:
						key = K_DOWNARROW;
						break;
					case SDL_SCANCODE_LEFT:
						key = K_LEFTARROW;
						break;
					case SDL_SCANCODE_RIGHT:
						key = K_RIGHTARROW;
						break;
					case SDL_SCANCODE_LALT:
					case SDL_SCANCODE_RALT:
						key = K_ALT;
						break;
					case SDL_SCANCODE_LCTRL:
					case SDL_SCANCODE_RCTRL:
						key = K_CTRL;
						break;
					case SDL_SCANCODE_LSHIFT:
					case SDL_SCANCODE_RSHIFT:
						key = K_SHIFT;
						break;
					case SDL_SCANCODE_F1:
						key = K_F1;
						break;
					case SDL_SCANCODE_F2:
						key = K_F2;
						break;
					case SDL_SCANCODE_F3:
						key = K_F3;
						break;
					case SDL_SCANCODE_F4:
						key = K_F4;
						break;
					case SDL_SCANCODE_F5:
						key = K_F5;
						break;
					case SDL_SCANCODE_F6:
						key = K_F6;
						break;
					case SDL_SCANCODE_F7:
						key = K_F7;
						break;
					case SDL_SCANCODE_F8:
						key = K_F8;
						break;
					case SDL_SCANCODE_F9:
						key = K_F9;
						break;
					case SDL_SCANCODE_F10:
						key = K_F10;
						break;
					case SDL_SCANCODE_F11:
						key = K_F11;
						break;
					case SDL_SCANCODE_F12:
						key = K_F12;
						break;
					case SDL_SCANCODE_INSERT:
						key = K_INS;
						break;
					case SDL_SCANCODE_DELETE:
						key = K_DEL;
						break;
					case SDL_SCANCODE_PAGEDOWN:
						key = K_PGDN;
						break;
					case SDL_SCANCODE_PAGEUP:
						key = K_PGUP;
						break;
					case SDL_SCANCODE_HOME:
						key = K_HOME;
						break;
					case SDL_SCANCODE_END:
						key = K_END;
						break;
					case SDL_SCANCODE_KP_7:
						key = K_KP_HOME;
						break;
					case SDL_SCANCODE_KP_8:
						key = K_KP_UPARROW;
						break;
					case SDL_SCANCODE_KP_9:
						key = K_KP_PGUP;
						break;
					case SDL_SCANCODE_KP_4:
						key = K_KP_LEFTARROW;
						break;
					case SDL_SCANCODE_KP_5:
						key = K_KP_5;
						break;
					case SDL_SCANCODE_KP_6:
						key = K_KP_RIGHTARROW;
						break;
					case SDL_SCANCODE_KP_1:
						key = K_KP_END;
						break;
					case SDL_SCANCODE_KP_2:
						key = K_KP_DOWNARROW;
						break;
					case SDL_SCANCODE_KP_3:
						key = K_KP_PGDN;
						break;
					case SDL_SCANCODE_KP_ENTER:
						key = K_KP_ENTER;
						break;
					case SDL_SCANCODE_KP_0:
						key = K_KP_INS;
						break;
					case SDL_SCANCODE_KP_PERIOD:
					case SDL_SCANCODE_KP_DECIMAL:
						key = K_KP_DEL;
						break;
					case SDL_SCANCODE_KP_DIVIDE:
						key = K_KP_SLASH;
						break;
					case SDL_SCANCODE_KP_MINUS:
						key = K_KP_MINUS;
						break;
					case SDL_SCANCODE_KP_PLUS:
						key = K_KP_PLUS;
						break;
					case SDL_SCANCODE_CAPSLOCK:
						key = K_CAPSLOCK;
						break;
					case SDL_SCANCODE_KP_MULTIPLY:
						key = K_KP_MUL;
						break;
					case SDL_SCANCODE_LGUI:
					case SDL_SCANCODE_RGUI:
						key = K_WIN;
						break;
					case SDL_SCANCODE_PAUSE:
						key = K_PAUSE;
						break;
					default:
						break;
					}
				}
				eng->TrapKey_Event( key, ev.type == SDL_KEYDOWN );
			}
		}
		else if ( ev.type > SDL_MOUSEBUTTONUP )
		{
			if ( ev.type == SDL_MOUSEWHEEL )
			{
				const int wheelKey = ( ev.wheel.y > 0 ) + 239;
				eng->TrapKey_Event( wheelKey, true );
				eng->TrapKey_Event( wheelKey, false );
			}
		}
		else if ( ev.type >= SDL_MOUSEBUTTONDOWN )
		{
			unsigned newMouseCode = mouseCode;
			const bool pressed = ev.type == SDL_MOUSEBUTTONDOWN;
			switch ( ev.button.button )
			{
			case SDL_BUTTON_LEFT:
				newMouseCode = pressed ? ( mouseCode | 1 ) : ( mouseCode & ~1u );
				mouseCode = newMouseCode;
				break;
			case SDL_BUTTON_MIDDLE:
				newMouseCode = pressed ? ( mouseCode | 4 ) : ( mouseCode & ~4u );
				mouseCode = newMouseCode;
				break;
			case SDL_BUTTON_RIGHT:
				newMouseCode = pressed ? ( mouseCode | 2 ) : ( mouseCode & ~2u );
				mouseCode = newMouseCode;
				break;
			case 4:
			case 6:
			case 8:
				newMouseCode = pressed ? ( mouseCode | 8 ) : ( mouseCode & ~8u );
				mouseCode = newMouseCode;
				break;
			case 5:
			case 7:
			case 9:
				newMouseCode = pressed ? ( mouseCode | 0x10 ) : ( mouseCode & ~0x10u );
				mouseCode = newMouseCode;
				break;
			default:
				break;
			}
			eng->TrapMouse_Event( newMouseCode, pressed );
		}
		else if ( ev.type == SDL_MOUSEMOTION
#if defined( _WIN32 )
				  && m_rawinput.value == 0.0f && !g_bIsCStrike && !g_bIsCZero && !g_bIsTFC && !g_bIsHL1
				  && !g_bIsCZeroRitual
#else
				  && ( cl_mousegrab.value == 0.0f || m_rawinput.value == 0.0f )
#endif
				  && m_bActiveApp && !g_BaseUISurface.IsCursorVisible() && !s_bCursorVisible && BUsesSDLInput() )
		{
			if ( m_bExpectSyntheticMouseMotion && ev.motion.x == m_nMouseTargetX && ev.motion.y == m_nMouseTargetY )
			{
				m_bExpectSyntheticMouseMotion = false;
				continue;
			}

			if ( !m_bCursorVisible )
			{
				const int warpDelta = m_nWarpDelta;
				if ( ev.motion.x < m_nMouseTargetX - warpDelta || ev.motion.x > warpDelta + m_nMouseTargetX
					 || ev.motion.y < m_nMouseTargetY - warpDelta || ev.motion.y > warpDelta + m_nMouseTargetY )
				{
					ClientDLL_IN_Accumulate();
					SDL_WarpMouseInWindow( m_hSDLWindow, m_nMouseTargetX, m_nMouseTargetY );
					m_bExpectSyntheticMouseMotion = true;
					SDL_PumpEvents();
					int deltaX = 0;
					int deltaY = 0;
					SDL_GetRelativeMouseState( &deltaX, &deltaY );
				}
			}
		}

		VGuiWrap2_CallEngineSurfaceAppHandler( &ev, NULL );
	} while ( SDL_PollEvent( &ev ) > 0 );
}

bool CGame::OnlyLegacyRender() const
{
	return m_bLegacyRender;
}

#else

#include "sys_sdlwind.h"

#include "common.h"
#include "filesystem.h"
#include "host.h"
#include "keys.h"
#include "cmd.h"
#include "conprint.h"
#include "cvar.h"
#include "gl_model.h"
#include "iengine.h"
#include "ivideomode.h"
#include "cd.h"
#include "server.h"
#include "sys.h"
#include "vgui_intwrap2.h"
#include "webm_video.h"
#include "vgui2/BaseUISurface.h"
#include "cdll_int.h"
#include "client.h"

#include "../external/SDL2/SDL.h"
#include "../external/SDL2/SDL_opengl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
constexpr int kHalfLifeAppId = 70;
constexpr int kGameIconBufferBytes = 0x40000;
constexpr int kEngineActive = 1;

bool s_bUseSDLInputRaw;
}

extern char g_szfullClientName[];
extern cvar_t cl_mousegrab;
extern cvar_t cl_skipvid;
extern cvar_t m_rawinput;
extern const char* gl_renderer;
extern bool s_bCursorVisible;

#if defined( _WIN32 )
bool bPausedByCareer;
extern int g_bIsCZeroRitual;
extern int g_bIsHL1;
#else
qboolean bPausedByCareer;
#endif
unsigned mouseCode;

bool BIsValveGame();

CGame g_Game;
IGame* game = &g_Game;

static int EngineKeyForSDLScancode( int scancode )
{
	if ( scancode < SDL_SCANCODE_A || scancode > SDL_SCANCODE_RGUI )
		return 0;

	switch ( static_cast<SDL_Scancode>( scancode ) )
	{
	case SDL_SCANCODE_TAB:
		return K_TAB;
	case SDL_SCANCODE_RETURN:
		return K_ENTER;
	case SDL_SCANCODE_ESCAPE:
		return K_ESCAPE;
	case SDL_SCANCODE_SPACE:
		return K_SPACE;
	case SDL_SCANCODE_APOSTROPHE:
		return '\'';
	case SDL_SCANCODE_COMMA:
		return ',';
	case SDL_SCANCODE_MINUS:
		return '-';
	case SDL_SCANCODE_PERIOD:
		return '.';
	case SDL_SCANCODE_SLASH:
		return '/';
	case SDL_SCANCODE_0:
		return '0';
	case SDL_SCANCODE_1:
		return '1';
	case SDL_SCANCODE_2:
		return '2';
	case SDL_SCANCODE_3:
		return '3';
	case SDL_SCANCODE_4:
		return '4';
	case SDL_SCANCODE_5:
		return '5';
	case SDL_SCANCODE_6:
		return '6';
	case SDL_SCANCODE_7:
		return '7';
	case SDL_SCANCODE_8:
		return '8';
	case SDL_SCANCODE_9:
		return '9';
	case SDL_SCANCODE_SEMICOLON:
		return ';';
	case SDL_SCANCODE_EQUALS:
		return '=';
	case SDL_SCANCODE_LEFTBRACKET:
		return '[';
	case SDL_SCANCODE_BACKSLASH:
		return '\\';
	case SDL_SCANCODE_RIGHTBRACKET:
		return ']';
	case SDL_SCANCODE_GRAVE:
		return '`';
	case SDL_SCANCODE_A:
		return 'a';
	case SDL_SCANCODE_B:
		return 'b';
	case SDL_SCANCODE_C:
		return 'c';
	case SDL_SCANCODE_D:
		return 'd';
	case SDL_SCANCODE_E:
		return 'e';
	case SDL_SCANCODE_F:
		return 'f';
	case SDL_SCANCODE_G:
		return 'g';
	case SDL_SCANCODE_H:
		return 'h';
	case SDL_SCANCODE_I:
		return 'i';
	case SDL_SCANCODE_J:
		return 'j';
	case SDL_SCANCODE_K:
		return 'k';
	case SDL_SCANCODE_L:
		return 'l';
	case SDL_SCANCODE_M:
		return 'm';
	case SDL_SCANCODE_N:
		return 'n';
	case SDL_SCANCODE_O:
		return 'o';
	case SDL_SCANCODE_P:
		return 'p';
	case SDL_SCANCODE_Q:
		return 'q';
	case SDL_SCANCODE_R:
		return 'r';
	case SDL_SCANCODE_S:
		return 's';
	case SDL_SCANCODE_T:
		return 't';
	case SDL_SCANCODE_U:
		return 'u';
	case SDL_SCANCODE_V:
		return 'v';
	case SDL_SCANCODE_W:
		return 'w';
	case SDL_SCANCODE_X:
		return 'x';
	case SDL_SCANCODE_Y:
		return 'y';
	case SDL_SCANCODE_Z:
		return 'z';
	case SDL_SCANCODE_BACKSPACE:
		return K_BACKSPACE;
	case SDL_SCANCODE_UP:
		return K_UPARROW;
	case SDL_SCANCODE_DOWN:
		return K_DOWNARROW;
	case SDL_SCANCODE_LEFT:
		return K_LEFTARROW;
	case SDL_SCANCODE_RIGHT:
		return K_RIGHTARROW;
	case SDL_SCANCODE_LALT:
	case SDL_SCANCODE_RALT:
		return K_ALT;
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL:
		return K_CTRL;
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT:
		return K_SHIFT;
	case SDL_SCANCODE_F1:
		return K_F1;
	case SDL_SCANCODE_F2:
		return K_F2;
	case SDL_SCANCODE_F3:
		return K_F3;
	case SDL_SCANCODE_F4:
		return K_F4;
	case SDL_SCANCODE_F5:
		return K_F5;
	case SDL_SCANCODE_F6:
		return K_F6;
	case SDL_SCANCODE_F7:
		return K_F7;
	case SDL_SCANCODE_F8:
		return K_F8;
	case SDL_SCANCODE_F9:
		return K_F9;
	case SDL_SCANCODE_F10:
		return K_F10;
	case SDL_SCANCODE_F11:
		return K_F11;
	case SDL_SCANCODE_F12:
		return K_F12;
	case SDL_SCANCODE_INSERT:
		return K_INS;
	case SDL_SCANCODE_DELETE:
		return K_DEL;
	case SDL_SCANCODE_PAGEDOWN:
		return K_PGDN;
	case SDL_SCANCODE_PAGEUP:
		return K_PGUP;
	case SDL_SCANCODE_HOME:
		return K_HOME;
	case SDL_SCANCODE_END:
		return K_END;
	case SDL_SCANCODE_KP_7:
		return K_KP_HOME;
	case SDL_SCANCODE_KP_8:
		return K_KP_UPARROW;
	case SDL_SCANCODE_KP_9:
		return K_KP_PGUP;
	case SDL_SCANCODE_KP_4:
		return K_KP_LEFTARROW;
	case SDL_SCANCODE_KP_5:
		return K_KP_5;
	case SDL_SCANCODE_KP_6:
		return K_KP_RIGHTARROW;
	case SDL_SCANCODE_KP_1:
		return K_KP_END;
	case SDL_SCANCODE_KP_2:
		return K_KP_DOWNARROW;
	case SDL_SCANCODE_KP_3:
		return K_KP_PGDN;
	case SDL_SCANCODE_KP_ENTER:
		return K_KP_ENTER;
	case SDL_SCANCODE_KP_0:
		return K_KP_INS;
	case SDL_SCANCODE_KP_PERIOD:
	case SDL_SCANCODE_KP_DECIMAL:
		return K_KP_DEL;
	case SDL_SCANCODE_KP_DIVIDE:
		return K_KP_SLASH;
	case SDL_SCANCODE_KP_MINUS:
		return K_KP_MINUS;
	case SDL_SCANCODE_KP_PLUS:
		return K_KP_PLUS;
	case SDL_SCANCODE_CAPSLOCK:
		return K_CAPSLOCK;
	case SDL_SCANCODE_KP_MULTIPLY:
		return K_KP_MUL;
	case SDL_SCANCODE_LGUI:
	case SDL_SCANCODE_RGUI:
		return K_WIN;
	case SDL_SCANCODE_PAUSE:
		return K_PAUSE;
	default:
		return 0;
	}
}

static void MarkClientUsesSDLInput( const char* pathFragment )
{
	if ( Q_strstr( g_szfullClientName, pathFragment ) )
		s_bUseSDLInputRaw = true;
}

CGame::~CGame()
{
}

CGame::CGame()
{
	m_y = 0;
	m_x = 0;
	m_height = 0;
	m_width = 0;
	m_bActiveApp = true;
	m_bMultiplayer = false;
	m_bExpectSyntheticMouseMotion = false;
	m_nMouseTargetX = 0;
	m_nMouseTargetY = 0;
	m_nWarpDelta = 0;
	m_bCursorVisible = true;
	m_bLegacyRender = false;
	m_hSDLWindow = nullptr;
	m_hSDLRenderer = nullptr;
	m_bCreatedRenderer = false;
	m_hSDLGLContext = nullptr;
}

bool CGame::Init( void* pvInstance )
{
	( void )pvInstance;
	return true;
}

HWND CGame::GetMainWindow()
{
	return reinterpret_cast<HWND>( m_hSDLWindow );
}

HWND* CGame::GetMainWindowAddress()
{
	return reinterpret_cast<HWND*>( &m_hSDLWindow );
}

void CGame::SetWindowXY( int x, int y )
{
	m_x = x;
	m_y = y;
}

void CGame::GetWindowRect( int* x, int* y, int* w, int* h )
{
	if ( x )
		*x = m_x;
	if ( y )
		*y = m_y;
	if ( w )
		*w = m_width;
	if ( h )
		*h = m_height;
}

bool CGame::IsActiveApp()
{
	return m_bActiveApp != 0;
}

bool CGame::IsMultiplayer()
{
	return m_bMultiplayer != 0;
}

bool CGame::Shutdown()
{
	if ( m_hSDLRenderer && m_bCreatedRenderer )
		SDL_DestroyRenderer( m_hSDLRenderer );
	SDL_Window* window = m_hSDLWindow;
	m_hSDLRenderer = nullptr;
	SDL_DestroyWindow( window );
	m_hSDLWindow = nullptr;
	SDL_Quit();
	return true;
}

void CGame::PlayAVIAndWait( const char* fileName )
{
	WebMPlayer player;
	player.PlayVideo( fileName, m_hSDLWindow );
}

void CGame::SetCursorVisible( bool bState )
{
	m_bCursorVisible = bState;
	if ( !bState && cl_mousegrab.value == 0.0f )
		SDL_SetWindowGrab( m_hSDLWindow, SDL_FALSE );
}

void CGame::SetWindowSize( int w, int h )
{
	m_width = w;
	m_height = h;
	SDL_SetWindowSize( m_hSDLWindow, w, h );
	const int height = m_height;
	m_nMouseTargetX = m_width / 2;
	m_nMouseTargetY = height / 2;
	int warpDelta = height / 3;
	if ( warpDelta <= 199 )
		warpDelta = 200;
	m_nWarpDelta = warpDelta;
	ClearScreen();
}

void CGame::PlayStartupVideos()
{
	if ( cl_skipvid.value > 0.0f )
		return;

	Con_Printf( "Playing Startup Videos...\n" );
	char szTemp[512];
	_snprintf( szTemp, sizeof( szTemp ) - 1, "../%s/media/StartupVids.txt", com_gamedir );
	COM_FixSlashes( szTemp );

	int length = 0;
	char* pbuffer = reinterpret_cast<char*>( COM_LoadFile( szTemp, 5, &length ) );
	if ( !pbuffer || length == 0 )
		return;

	SetCursorVisible( false );

	char szFileName[MAX_PATH];
	char* parse = pbuffer;
	while ( true )
	{
		parse = const_cast<char*>( COM_Parse( parse ) );
		if ( Q_strlen( com_token ) <= 0 )
			break;
		FS_GetLocalPath( com_token, szFileName, sizeof( szFileName ) );
		PlayAVIAndWait( szFileName );
	}

	std::free( pbuffer );
	SetCursorVisible( true );
}

qboolean BUsesSDLInput()
{
	s_bUseSDLInputRaw = BIsValveGame();
#if defined( _WIN32 )
	MarkClientUsesSDLInput( "valve\\cl_dlls\\client" );
	MarkClientUsesSDLInput( "cstrike\\cl_dlls\\client" );
	MarkClientUsesSDLInput( "tfc\\cl_dlls\\client" );
	MarkClientUsesSDLInput( "dod\\cl_dlls\\client" );
	MarkClientUsesSDLInput( "gearbox\\cl_dlls\\client" );
	MarkClientUsesSDLInput( "bshift\\cl_dlls\\client" );
	MarkClientUsesSDLInput( "czeror\\cl_dlls\\client" );
	MarkClientUsesSDLInput( "czero\\cl_dlls\\client" );
	MarkClientUsesSDLInput( "opfor\\cl_dlls\\client" );
	MarkClientUsesSDLInput( "dmc\\cl_dlls\\client" );
	MarkClientUsesSDLInput( "ricochet\\cl_dlls\\client" );
#else
	MarkClientUsesSDLInput( "valve/cl_dlls/client" );
	MarkClientUsesSDLInput( "cstrike/cl_dlls/client" );
	MarkClientUsesSDLInput( "tfc/cl_dlls/client" );
	MarkClientUsesSDLInput( "dod/cl_dlls/client" );
	MarkClientUsesSDLInput( "gearbox/cl_dlls/client" );
	MarkClientUsesSDLInput( "bshift/cl_dlls/client" );
	MarkClientUsesSDLInput( "czeror/cl_dlls/client" );
	MarkClientUsesSDLInput( "czero/cl_dlls/client" );
	MarkClientUsesSDLInput( "opfor/cl_dlls/client" );
	MarkClientUsesSDLInput( "dmc/cl_dlls/client" );
	MarkClientUsesSDLInput( "ricochet/cl_dlls/client" );
#endif
	return s_bUseSDLInputRaw ? 1 : 0;
}

void GetWindowNameFromGameDir( char* output, int outputBufferSize )
{
	FileHandle_t file = FS_Open( "liblist.gam", "rt" );
	if ( !file )
		return;

	while ( !FS_EndOfFile( file ) )
	{
		char line[512];
		line[0] = 0;
		FS_ReadLine( line, 511, file );
		line[511] = 0;
		if ( Q_strnicmp( line, "game", 4 ) != 0 )
			continue;

		char* openQuote = std::strchr( line, '\"' );
		if ( !openQuote )
			break;

		const char* nameStart = openQuote + 1;
		char* closeQuote = std::strchr( openQuote + 1, '\"' );
		if ( !closeQuote )
			break;

		const int nameLength = static_cast<int>( closeQuote - nameStart );
		if ( nameLength < outputBufferSize )
		{
			std::strncpy( output, nameStart, nameLength );
			output[nameLength] = 0;
		}
		break;
	}

	FS_Close( file );
}

void SDL_SetAttributes( bool tryNewCtx )
{
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 4 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 4 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 4 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 0 );

	if ( videomode->IsWindowedMode() )
	{
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 4 );
	}

	if ( tryNewCtx )
	{
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 2 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
	}
}

bool CGame::CreateGameWindow()
{
	if ( COM_CheckParm( "-noontop" ) )
		SDL_SetHint( "SDL_ALLOW_TOPMOST", "0" );
#if !defined( _WIN32 )
	SDL_SetHint( "SDL_VIDEO_X11_XRANDR", "1" );
	SDL_SetHint( "SDL_VIDEO_X11_XVIDMODE", "1" );
#endif
	SDL_InitSubSystem( SDL_INIT_EVERYTHING );
	SDL_SetAttributes( true );

	char gameWindowName[512];
	Q_strcpy( gameWindowName, "Half-Life" );
	GetWindowNameFromGameDir( gameWindowName, sizeof( gameWindowName ) );

	const Uint32 windowFlags = COM_CheckParm( "-noborder" ) == 0
								   ? SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
								   : SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS;
	m_hSDLWindow = SDL_CreateWindow( gameWindowName, 0, 0, 640, 480, windowFlags );
	if ( !m_hSDLWindow )
	{
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 3 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 3 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 3 );
		m_hSDLWindow = SDL_CreateWindow( gameWindowName, 0, 0, 640, 480, windowFlags );
		if ( !m_hSDLWindow )
			Sys_Error( "Failed to create SDL Window" );
	}

	byte* buffer = static_cast<byte*>( std::malloc( kGameIconBufferBytes ) );
	int width = 0;
	int height = 0;
	if ( LoadTGA( const_cast<char*>( "game.tga" ), buffer, kGameIconBufferBytes, &width, &height ) )
	{
		if ( GetGameAppID() != kHalfLifeAppId )
		{
			byte* rowScratch = static_cast<byte*>( std::malloc( 4 * width ) );
			for ( int row = 0; row < height / 2; ++row )
			{
				std::memcpy( rowScratch, &buffer[4 * width * row], 4 * width );
				std::memcpy( &buffer[4 * width * row], &buffer[4 * width * ( height - row )], 4 * width );
				std::memcpy( &buffer[4 * width * ( height - row )], rowScratch, 4 * width );
			}
			std::free( rowScratch );
		}

		SDL_Surface* iconSurface = SDL_CreateRGBSurfaceFrom( buffer, width, height, 32,
			4 * width, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000 );
		if ( iconSurface )
		{
			SDL_SetWindowIcon( m_hSDLWindow, iconSurface );
			SDL_FreeSurface( iconSurface );
		}
	}
	std::free( buffer );

	SDL_ShowWindow( m_hSDLWindow );
	SDL_StartTextInput();

	m_hSDLRenderer = nullptr;
	m_bCreatedRenderer = false;

	m_hSDLGLContext = SDL_GL_CreateContext( m_hSDLWindow );
	if ( !m_hSDLGLContext )
	{
		SDL_SetAttributes( false );
		m_bLegacyRender = true;
		m_hSDLGLContext = SDL_GL_CreateContext( m_hSDLWindow );
	}

	if ( !m_hSDLWindow || !m_hSDLGLContext )
		Sys_Error( "Failed to create SDL Window" );

	int r = 0;
	int g = 0;
	int b = 0;
	int a = 0;
	int depth = 0;

	if ( SDL_GL_GetAttribute( SDL_GL_RED_SIZE, &r ) != 0 )
	{
		const char* error = SDL_GetError();
		r = 0;
		Con_Printf( "Failed to get GL RED size (%s)\n", error );
	}
	if ( SDL_GL_GetAttribute( SDL_GL_GREEN_SIZE, &g ) != 0 )
	{
		const char* error = SDL_GetError();
		g = 0;
		Con_Printf( "Failed to get GL GREEN size (%s)\n", error );
	}
	if ( SDL_GL_GetAttribute( SDL_GL_BLUE_SIZE, &b ) != 0 )
	{
		const char* error = SDL_GetError();
		b = 0;
		Con_Printf( "Failed to get GL BLUE size (%s)\n", error );
	}
	if ( SDL_GL_GetAttribute( SDL_GL_ALPHA_SIZE, &a ) != 0 )
	{
		const char* error = SDL_GetError();
		a = 0;
		Con_Printf( "Failed to get GL ALPHA size (%s)\n", error );
	}
	if ( SDL_GL_GetAttribute( SDL_GL_DEPTH_SIZE, &depth ) != 0 )
	{
		const char* error = SDL_GetError();
		depth = 0;
		Con_Printf( "Failed to get GL DEPTH size (%s)\n", error );
	}

	Con_Printf( "GL_SIZES:  r:%d g:%d b:%d a:%d depth:%d\n", r, g, b, a, depth );

	if ( r <= 2 || g <= 2 || b <= 2 || depth <= 15 || ( gl_renderer && Q_strstr( gl_renderer, "GDI Generic" ) ) )
	{
		Sys_Error( "Failed to create SDL Window, unsupported video mode. A 16-bit color depth desktop is required and a"
				   " supported GL driver.\n" );
	}

	ClearScreen();

	m_nMouseTargetX = 320;
	m_nMouseTargetY = 240;
	m_nWarpDelta = 213;
	return true;
}

void CGame::SetActiveApp( bool active )
{
	m_bActiveApp = active;
}

void CGame::ClearScreen()
{
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );
	SDL_GL_SwapWindow( m_hSDLWindow );
}

void CGame::AppActivate( bool fActive )
{
	if ( fActive )
	{
		std::fprintf( stderr, "AppActive: active\n" );
		if ( host_initialized )
		{
			ClearIOStates();
#if defined( _WIN32 )
			cdaudio->Resume();
#else
			cdaudio->MP3_Resume_Audio();
#endif
			if ( videomode->GetInitialized() )
			{
				if ( !VGuiWrap2_IsGameUIVisible() )
					ClientDLL_ActivateMouse();
				g_BaseUISurface.CalculateMouseVisible();
				int dx = 0;
				int dy = 0;
				g_BaseUISurface.GetMouseDelta( &dx, &dy );
			}
			if ( VGuiWrap2_IsInCareerMatch() && bPausedByCareer )
			{
				Cmd_ExecuteString( "unpause", src_command );
				bPausedByCareer = false;
			}
		}
	}
	else
	{
		std::fprintf( stderr, "AppActive: not active\n" );
		if ( host_initialized )
		{
			if ( videomode->GetInitialized() && !VGuiWrap2_IsGameUIVisible() )
				ClientDLL_DeactivateMouse();
			cdaudio->Pause();
			if ( VGuiWrap2_IsInCareerMatch() && !cl.paused )
			{
				Cmd_ExecuteString( "setpause", src_command );
				bPausedByCareer = true;
			}
		}
	}
	m_bActiveApp = fActive;
}

void CGame::SleepUntilInput( int time )
{
	SDL_Event ev;
	SDL_PumpEvents();
	if ( SDL_WaitEventTimeout( &ev, time ) <= 0 )
		return;

	do
	{
		if ( ev.type == SDL_TEXTINPUT )
		{
			if ( key_dest == key_message )
			{
				const int textLength = static_cast<int>( std::strlen( ev.text.text ) );
				int copyLength = textLength;
				if ( KEY_CHAT_BUFFER_BYTES - 1 - chat_bufferlen <= copyLength )
					copyLength = KEY_CHAT_BUFFER_BYTES - 1 - chat_bufferlen;
				Q_strncpy( &chat_buffer[chat_bufferlen], ev.text.text, copyLength );
				chat_bufferlen += copyLength;
				chat_buffer[chat_bufferlen] = 0;
			}
		}
		else if ( ev.type <= SDL_TEXTINPUT )
		{
			if ( ev.type == SDL_WINDOWEVENT )
			{
				switch ( ev.window.event )
				{
				case SDL_WINDOWEVENT_SHOWN:
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					g_Game.AppActivate( true );
					break;
				case SDL_WINDOWEVENT_HIDDEN:
				case SDL_WINDOWEVENT_FOCUS_LOST:
					g_Game.AppActivate( false );
					break;
				case SDL_WINDOWEVENT_MOVED:
					if ( videomode->IsWindowedMode() )
					{
						game->SetWindowXY( ev.window.data1, ev.window.data2 );
						videomode->UpdateWindowPosition();
					}
					break;
				case SDL_WINDOWEVENT_RESIZED:
					game->SetWindowSize( ev.window.data1, ev.window.data2 );
					videomode->UpdateWindowPosition();
					break;
				case SDL_WINDOWEVENT_CLOSE:
					if ( eng->GetState() == kEngineActive )
						eng->SetQuitting( 1 );
					if ( m_hSDLRenderer && m_bCreatedRenderer )
						SDL_DestroyRenderer( m_hSDLRenderer );
					SDL_DestroyWindow( m_hSDLWindow );
					m_hSDLRenderer = nullptr;
					m_hSDLWindow = nullptr;
					return;
				default:
					break;
				}
			}
			else if ( ev.type <= SDL_WINDOWEVENT )
			{
				if ( ev.type == SDL_QUIT && eng->GetState() == kEngineActive )
					eng->SetQuitting( 1 );
			}
			else if ( ev.type == SDL_KEYDOWN )
			{
				const int key = EngineKeyForSDLScancode( ev.key.keysym.scancode );
				eng->TrapKey_Event( key, true );
			}
			else if ( ev.type == SDL_KEYUP )
			{
				const int key = EngineKeyForSDLScancode( ev.key.keysym.scancode );
				eng->TrapKey_Event( key, false );
			}
		}
		else if ( ev.type > SDL_MOUSEBUTTONUP )
		{
			if ( ev.type == SDL_MOUSEWHEEL )
			{
#if defined( _WIN32 )
				const int wheelKey = ( static_cast<Sint16>( ev.wheel.y ) > 0 ) + 239;
#else
				const int wheelKey = ( ev.wheel.y > 0 ) + 239;
#endif
				eng->TrapKey_Event( wheelKey, true );
				eng->TrapKey_Event( wheelKey, false );
			}
		}
		else if ( ev.type >= SDL_MOUSEBUTTONDOWN )
		{
			unsigned newMouseCode = mouseCode;
			const bool pressed = ev.type == SDL_MOUSEBUTTONDOWN;
			switch ( ev.button.button )
			{
			case SDL_BUTTON_LEFT:
				newMouseCode = pressed ? ( mouseCode | 1 ) : ( mouseCode & ~1u );
				mouseCode = newMouseCode;
				break;
			case SDL_BUTTON_MIDDLE:
				newMouseCode = pressed ? ( mouseCode | 4 ) : ( mouseCode & ~4u );
				mouseCode = newMouseCode;
				break;
			case SDL_BUTTON_RIGHT:
				newMouseCode = pressed ? ( mouseCode | 2 ) : ( mouseCode & ~2u );
				mouseCode = newMouseCode;
				break;
			case 4:
			case 6:
			case 8:
				newMouseCode = pressed ? ( mouseCode | 8 ) : ( mouseCode & ~8u );
				mouseCode = newMouseCode;
				break;
			case 5:
			case 7:
			case 9:
				newMouseCode = pressed ? ( mouseCode | 0x10 ) : ( mouseCode & ~0x10u );
				mouseCode = newMouseCode;
				break;
			default:
				break;
			}
			eng->TrapMouse_Event( newMouseCode, pressed );
		}
		else if ( ev.type == SDL_MOUSEMOTION
#if defined( _WIN32 )
				  && m_rawinput.value == 0.0f && !g_bIsCStrike && !g_bIsCZero && !g_bIsTFC && !g_bIsHL1
				  && !g_bIsCZeroRitual
#else
				  && ( cl_mousegrab.value == 0.0f || m_rawinput.value == 0.0f )
#endif
				  && m_bActiveApp && !g_BaseUISurface.IsCursorVisible() && !s_bCursorVisible && BUsesSDLInput() )
		{
			if ( m_bExpectSyntheticMouseMotion && ev.motion.x == m_nMouseTargetX && ev.motion.y == m_nMouseTargetY )
			{
				m_bExpectSyntheticMouseMotion = false;
				continue;
			}

			if ( !m_bCursorVisible )
			{
				const int warpDelta = m_nWarpDelta;
				if ( ev.motion.x < m_nMouseTargetX - warpDelta || ev.motion.x > warpDelta + m_nMouseTargetX
					 || ev.motion.y < m_nMouseTargetY - warpDelta || ev.motion.y > warpDelta + m_nMouseTargetY )
				{
					ClientDLL_IN_Accumulate();
					SDL_WarpMouseInWindow( m_hSDLWindow, m_nMouseTargetX, m_nMouseTargetY );
					m_bExpectSyntheticMouseMotion = true;
					SDL_PumpEvents();
					int deltaX = 0;
					int deltaY = 0;
					SDL_GetRelativeMouseState( &deltaX, &deltaY );
				}
			}
		}

		VGuiWrap2_CallEngineSurfaceAppHandler( &ev, nullptr );
	} while ( SDL_PollEvent( &ev ) > 0 );
}

bool CGame::OnlyLegacyRender() const
{
	return m_bLegacyRender != 0;
}

#endif
