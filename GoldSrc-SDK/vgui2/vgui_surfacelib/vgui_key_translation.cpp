#include "vgui_key_translation.h"
#include "vgui/MouseCode.h"
#include "../../external/SDL2/SDL.h"

#include <cstdint>
#include <cstring>

using namespace vgui2;

struct vgui_to_virtual_t
{
	KeyCode vguiKeyCode;
	std::int32_t windowsKeyCode;
	std::int32_t sdlKeyCode;
};

static_assert( sizeof( vgui_to_virtual_t ) == 12 );

static constexpr int NO_VIRTUAL_KEY = 0;
static constexpr int VIRTUAL_KEY_COUNT = 256;

static const vgui_to_virtual_t keyMap[] = { { KEY_0, '0', SDLK_0 }, { KEY_1, '1', SDLK_1 }, { KEY_2, '2', SDLK_2 },
	{ KEY_3, '3', SDLK_3 }, { KEY_4, '4', SDLK_4 }, { KEY_5, '5', SDLK_5 }, { KEY_6, '6', SDLK_6 },
	{ KEY_7, '7', SDLK_7 }, { KEY_8, '8', SDLK_8 }, { KEY_9, '9', SDLK_9 }, { KEY_A, 'A', SDLK_a },
	{ KEY_B, 'B', SDLK_b }, { KEY_C, 'C', SDLK_c }, { KEY_D, 'D', SDLK_d }, { KEY_E, 'E', SDLK_e },
	{ KEY_F, 'F', SDLK_f }, { KEY_G, 'G', SDLK_g }, { KEY_H, 'H', SDLK_h }, { KEY_I, 'I', SDLK_i },
	{ KEY_J, 'J', SDLK_j }, { KEY_K, 'K', SDLK_k }, { KEY_L, 'L', SDLK_l }, { KEY_M, 'M', SDLK_m },
	{ KEY_N, 'N', SDLK_n }, { KEY_O, 'O', SDLK_o }, { KEY_P, 'P', SDLK_p }, { KEY_Q, 'Q', SDLK_q },
	{ KEY_R, 'R', SDLK_r }, { KEY_S, 'S', SDLK_s }, { KEY_T, 'T', SDLK_t }, { KEY_U, 'U', SDLK_u },
	{ KEY_V, 'V', SDLK_v }, { KEY_W, 'W', SDLK_w }, { KEY_X, 'X', SDLK_x }, { KEY_Y, 'Y', SDLK_y },
	{ KEY_Z, 'Z', SDLK_z }, { KEY_PAD_0, NO_VIRTUAL_KEY, SDLK_KP_0 }, { KEY_PAD_1, NO_VIRTUAL_KEY, SDLK_KP_1 },
	{ KEY_PAD_2, NO_VIRTUAL_KEY, SDLK_KP_2 }, { KEY_PAD_3, NO_VIRTUAL_KEY, SDLK_KP_3 },
	{ KEY_PAD_4, NO_VIRTUAL_KEY, SDLK_KP_4 }, { KEY_PAD_5, NO_VIRTUAL_KEY, SDLK_KP_5 },
	{ KEY_PAD_6, NO_VIRTUAL_KEY, SDLK_KP_6 }, { KEY_PAD_7, NO_VIRTUAL_KEY, SDLK_KP_7 },
	{ KEY_PAD_8, NO_VIRTUAL_KEY, SDLK_KP_8 }, { KEY_PAD_9, NO_VIRTUAL_KEY, SDLK_KP_9 },
	{ KEY_PAD_DIVIDE, NO_VIRTUAL_KEY, SDLK_KP_DIVIDE }, { KEY_PAD_MULTIPLY, NO_VIRTUAL_KEY, SDLK_KP_MULTIPLY },
	{ KEY_PAD_MINUS, NO_VIRTUAL_KEY, SDLK_KP_MINUS }, { KEY_PAD_PLUS, NO_VIRTUAL_KEY, SDLK_KP_PLUS },
	{ KEY_PAD_ENTER, NO_VIRTUAL_KEY, SDLK_KP_ENTER }, { KEY_PAD_DECIMAL, NO_VIRTUAL_KEY, SDLK_KP_DECIMAL },
	{ KEY_LBRACKET, NO_VIRTUAL_KEY, SDLK_LEFTBRACKET }, { KEY_RBRACKET, NO_VIRTUAL_KEY, SDLK_RIGHTBRACKET },
	{ KEY_SEMICOLON, NO_VIRTUAL_KEY, SDLK_SEMICOLON }, { KEY_APOSTROPHE, NO_VIRTUAL_KEY, SDLK_QUOTE },
	{ KEY_BACKQUOTE, NO_VIRTUAL_KEY, SDLK_BACKQUOTE }, { KEY_COMMA, NO_VIRTUAL_KEY, SDLK_COMMA },
	{ KEY_PERIOD, NO_VIRTUAL_KEY, SDLK_PERIOD }, { KEY_SLASH, NO_VIRTUAL_KEY, SDLK_SLASH },
	{ KEY_BACKSLASH, NO_VIRTUAL_KEY, SDLK_BACKSLASH }, { KEY_MINUS, NO_VIRTUAL_KEY, SDLK_MINUS },
	{ KEY_EQUAL, NO_VIRTUAL_KEY, SDLK_EQUALS }, { KEY_ENTER, NO_VIRTUAL_KEY, SDLK_RETURN },
	{ KEY_SPACE, NO_VIRTUAL_KEY, SDLK_SPACE }, { KEY_BACKSPACE, NO_VIRTUAL_KEY, SDLK_BACKSPACE },
	{ KEY_TAB, NO_VIRTUAL_KEY, SDLK_TAB }, { KEY_CAPSLOCK, NO_VIRTUAL_KEY, SDLK_CAPSLOCK },
	{ KEY_NUMLOCK, NO_VIRTUAL_KEY, SDLK_NUMLOCKCLEAR }, { KEY_ESCAPE, NO_VIRTUAL_KEY, SDLK_ESCAPE },
	{ KEY_SCROLLLOCK, NO_VIRTUAL_KEY, SDLK_SCROLLLOCK }, { KEY_INSERT, NO_VIRTUAL_KEY, SDLK_INSERT },
	{ KEY_DELETE, NO_VIRTUAL_KEY, SDLK_DELETE }, { KEY_HOME, NO_VIRTUAL_KEY, SDLK_HOME },
	{ KEY_END, NO_VIRTUAL_KEY, SDLK_END }, { KEY_PAGEUP, NO_VIRTUAL_KEY, SDLK_PAGEUP },
	{ KEY_PAGEDOWN, NO_VIRTUAL_KEY, SDLK_PAGEDOWN }, { KEY_BREAK, NO_VIRTUAL_KEY, SDLK_PAUSE },
	{ KEY_RSHIFT, NO_VIRTUAL_KEY, SDLK_RSHIFT }, { KEY_LSHIFT, NO_VIRTUAL_KEY, SDLK_LSHIFT },
	{ KEY_RALT, NO_VIRTUAL_KEY, SDLK_RALT }, { KEY_LALT, NO_VIRTUAL_KEY, SDLK_LALT },
	{ KEY_RCONTROL, NO_VIRTUAL_KEY, SDLK_RCTRL }, { KEY_LCONTROL, NO_VIRTUAL_KEY, SDLK_LCTRL },
	{ KEY_LWIN, NO_VIRTUAL_KEY, SDLK_LGUI }, { KEY_RWIN, NO_VIRTUAL_KEY, SDLK_RGUI },
	{ KEY_APP, NO_VIRTUAL_KEY, SDLK_APPLICATION }, { KEY_UP, NO_VIRTUAL_KEY, SDLK_UP },
	{ KEY_LEFT, NO_VIRTUAL_KEY, SDLK_LEFT }, { KEY_DOWN, NO_VIRTUAL_KEY, SDLK_DOWN },
	{ KEY_RIGHT, NO_VIRTUAL_KEY, SDLK_RIGHT }, { KEY_F1, NO_VIRTUAL_KEY, SDLK_F1 }, { KEY_F2, NO_VIRTUAL_KEY, SDLK_F2 },
	{ KEY_F3, NO_VIRTUAL_KEY, SDLK_F3 }, { KEY_F4, NO_VIRTUAL_KEY, SDLK_F4 }, { KEY_F5, NO_VIRTUAL_KEY, SDLK_F5 },
	{ KEY_F6, NO_VIRTUAL_KEY, SDLK_F6 }, { KEY_F7, NO_VIRTUAL_KEY, SDLK_F7 }, { KEY_F8, NO_VIRTUAL_KEY, SDLK_F8 },
	{ KEY_F9, NO_VIRTUAL_KEY, SDLK_F9 }, { KEY_F10, NO_VIRTUAL_KEY, SDLK_F10 }, { KEY_F11, NO_VIRTUAL_KEY, SDLK_F11 },
	{ KEY_F12, NO_VIRTUAL_KEY, SDLK_F12 } };

static KeyCode s_pVirtualKeyToVGUI[VIRTUAL_KEY_COUNT];
static std::int32_t s_pVGUIToVirtualKey[VIRTUAL_KEY_COUNT];

void InitKeyTranslationTable()
{
	static bool bInitted;
	if ( bInitted )
		return;
	bInitted = true;
	std::memset( s_pVirtualKeyToVGUI, 0, sizeof( s_pVirtualKeyToVGUI ) );
	for ( const vgui_to_virtual_t& key : keyMap )
	{
		int index = key.sdlKeyCode & ~SDLK_SCANCODE_MASK;
		s_pVirtualKeyToVGUI[index] = key.vguiKeyCode;
	}
}

static KeyCode SDLKeysymToKeyCode( int keysym )
{
	switch ( keysym )
	{
	case SDLK_0:
		return KEY_0;
	case SDLK_1:
		return KEY_1;
	case SDLK_2:
		return KEY_2;
	case SDLK_3:
		return KEY_3;
	case SDLK_4:
		return KEY_4;
	case SDLK_5:
		return KEY_5;
	case SDLK_6:
		return KEY_6;
	case SDLK_7:
		return KEY_7;
	case SDLK_8:
		return KEY_8;
	case SDLK_9:
		return KEY_9;
	case SDLK_a:
		return KEY_A;
	case SDLK_b:
		return KEY_B;
	case SDLK_c:
		return KEY_C;
	case SDLK_d:
		return KEY_D;
	case SDLK_e:
		return KEY_E;
	case SDLK_f:
		return KEY_F;
	case SDLK_g:
		return KEY_G;
	case SDLK_h:
		return KEY_H;
	case SDLK_i:
		return KEY_I;
	case SDLK_j:
		return KEY_J;
	case SDLK_k:
		return KEY_K;
	case SDLK_l:
		return KEY_L;
	case SDLK_m:
		return KEY_M;
	case SDLK_n:
		return KEY_N;
	case SDLK_o:
		return KEY_O;
	case SDLK_p:
		return KEY_P;
	case SDLK_q:
		return KEY_Q;
	case SDLK_r:
		return KEY_R;
	case SDLK_s:
		return KEY_S;
	case SDLK_t:
		return KEY_T;
	case SDLK_u:
		return KEY_U;
	case SDLK_v:
		return KEY_V;
	case SDLK_w:
		return KEY_W;
	case SDLK_x:
		return KEY_X;
	case SDLK_y:
		return KEY_Y;
	case SDLK_z:
		return KEY_Z;
	case SDLK_KP_0:
		return KEY_PAD_0;
	case SDLK_KP_1:
		return KEY_PAD_1;
	case SDLK_KP_2:
		return KEY_PAD_2;
	case SDLK_KP_3:
		return KEY_PAD_3;
	case SDLK_KP_4:
		return KEY_PAD_4;
	case SDLK_KP_5:
		return KEY_PAD_5;
	case SDLK_KP_6:
		return KEY_PAD_6;
	case SDLK_KP_7:
		return KEY_PAD_7;
	case SDLK_KP_8:
		return KEY_PAD_8;
	case SDLK_KP_9:
		return KEY_PAD_9;
	case SDLK_KP_DIVIDE:
		return KEY_PAD_DIVIDE;
	case SDLK_KP_MULTIPLY:
		return KEY_PAD_MULTIPLY;
	case SDLK_KP_MINUS:
		return KEY_PAD_MINUS;
	case SDLK_KP_PLUS:
		return KEY_PAD_PLUS;
	case SDLK_KP_ENTER:
		return KEY_PAD_ENTER;
	case SDLK_KP_PERIOD:
	case SDLK_KP_DECIMAL:
		return KEY_PAD_DECIMAL;
	case SDLK_LEFTBRACKET:
		return KEY_LBRACKET;
	case SDLK_RIGHTBRACKET:
		return KEY_RBRACKET;
	case SDLK_SEMICOLON:
		return KEY_SEMICOLON;
	case SDLK_QUOTE:
		return KEY_APOSTROPHE;
	case SDLK_BACKQUOTE:
	case SDL_SCANCODE_TO_KEYCODE( SDL_SCANCODE_GRAVE ):
		return KEY_BACKQUOTE;
	case SDLK_COMMA:
		return KEY_COMMA;
	case SDLK_PERIOD:
		return KEY_PERIOD;
	case SDLK_SLASH:
		return KEY_SLASH;
	case SDLK_BACKSLASH:
		return KEY_BACKSLASH;
	case SDLK_MINUS:
	case SDL_SCANCODE_TO_KEYCODE( SDL_SCANCODE_MINUS ):
		return KEY_MINUS;
	case SDLK_EQUALS:
	case SDL_SCANCODE_TO_KEYCODE( SDL_SCANCODE_EQUALS ):
		return KEY_EQUAL;
	case SDLK_RETURN:
		return KEY_ENTER;
	case SDLK_SPACE:
		return KEY_SPACE;
	case SDLK_BACKSPACE:
		return KEY_BACKSPACE;
	case SDLK_TAB:
	case SDL_SCANCODE_TO_KEYCODE( SDL_SCANCODE_TAB ):
		return KEY_TAB;
	case SDLK_CAPSLOCK:
		return KEY_CAPSLOCK;
	case SDLK_NUMLOCKCLEAR:
		return KEY_NUMLOCK;
	case SDLK_ESCAPE:
		return KEY_ESCAPE;
	case SDLK_SCROLLLOCK:
		return KEY_SCROLLLOCK;
	case SDLK_INSERT:
		return KEY_INSERT;
	case SDLK_DELETE:
	case SDL_SCANCODE_TO_KEYCODE( SDL_SCANCODE_DELETE ):
		return KEY_DELETE;
	case SDLK_HOME:
		return KEY_HOME;
	case SDLK_END:
		return KEY_END;
	case SDLK_PAGEUP:
		return KEY_PAGEUP;
	case SDLK_PAGEDOWN:
		return KEY_PAGEDOWN;
	case SDLK_PAUSE:
		return KEY_BREAK;
	case SDLK_RSHIFT:
		return KEY_RSHIFT;
	case SDLK_LSHIFT:
		return KEY_LSHIFT;
	case SDLK_RALT:
		return KEY_RALT;
	case SDLK_LALT:
		return KEY_LALT;
	case SDLK_RCTRL:
		return KEY_RCONTROL;
	case SDLK_LCTRL:
		return KEY_LCONTROL;
	case SDLK_LGUI:
		return KEY_LWIN;
	case SDLK_RGUI:
		return KEY_RWIN;
	case SDLK_UP:
		return KEY_UP;
	case SDLK_LEFT:
		return KEY_LEFT;
	case SDLK_DOWN:
		return KEY_DOWN;
	case SDLK_RIGHT:
		return KEY_RIGHT;
	case SDLK_F1:
		return KEY_F1;
	case SDLK_F2:
		return KEY_F2;
	case SDLK_F3:
		return KEY_F3;
	case SDLK_F4:
		return KEY_F4;
	case SDLK_F5:
		return KEY_F5;
	case SDLK_F6:
		return KEY_F6;
	case SDLK_F7:
		return KEY_F7;
	case SDLK_F8:
		return KEY_F8;
	case SDLK_F9:
		return KEY_F9;
	case SDLK_F10:
		return KEY_F10;
	case SDLK_F11:
		return KEY_F11;
	case SDLK_F12:
		return KEY_F12;
	default:
		return KEY_NONE;
	}
}

extern "C" KeyCode KeyCode_VirtualKeyToVGUI( int keyCode )
{
	InitKeyTranslationTable();
	return SDLKeysymToKeyCode( keyCode );
}

namespace vgui2
{
void InitVGUIToVKTranslationTable()
{
	static bool bInitted;
	if ( bInitted )
		return;
	bInitted = true;
	std::memset( s_pVGUIToVirtualKey, 0, sizeof( s_pVGUIToVirtualKey ) );
	for ( const vgui_to_virtual_t& key : keyMap )
	{
		s_pVGUIToVirtualKey[key.vguiKeyCode] = key.sdlKeyCode;
	}
}
}

int KeyCode_VGUIToVirtualKey( KeyCode keyCode )
{
	vgui2::InitVGUIToVKTranslationTable();
	if ( ( unsigned int )keyCode >= VIRTUAL_KEY_COUNT )
		return -1;
	return s_pVGUIToVirtualKey[keyCode];
}

bool IsKeyCode( int vk_code )
{
	return ( unsigned int )( vk_code - 1 ) <= 0xFE;
}

bool IsMouseCode( int vk_code )
{
	( void )vk_code;
	return false;
}

MouseCode MouseCode_VirtualKeyToVGUI( int vk_code )
{
	( void )vk_code;
	return MOUSE_LAST;
}

int MouseCode_VGUIToVirtualKey( MouseCode keyCode )
{
	( void )keyCode;
	return 0;
}
