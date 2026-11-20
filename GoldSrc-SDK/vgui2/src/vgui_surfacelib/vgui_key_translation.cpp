#include "vgui_key_translation.h"

#include <assert.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\vgui_surfacelib\\vgui_key_translation.cpp" )

using namespace vgui2;

struct vgui_to_virtual_t
{
	KeyCode	vguiKeyCode;
	int		windowsKeyCode;
	int		sdlKeyCode;
};

static const vgui_to_virtual_t keyMap[] =
{
	{ KEY_0, '0', '0' },
	{ KEY_1, '1', '1' },
	{ KEY_2, '2', '2' },
	{ KEY_3, '3', '3' },
	{ KEY_4, '4', '4' },
	{ KEY_5, '5', '5' },
	{ KEY_6, '6', '6' },
	{ KEY_7, '7', '7' },
	{ KEY_8, '8', '8' },
	{ KEY_9, '9', '9' },
	{ KEY_A, 'A', 'a' },
	{ KEY_B, 'B', 'b' },
	{ KEY_C, 'C', 'c' },
	{ KEY_D, 'D', 'd' },
	{ KEY_E, 'E', 'e' },
	{ KEY_F, 'F', 'f' },
	{ KEY_G, 'G', 'g' },
	{ KEY_H, 'H', 'h' },
	{ KEY_I, 'I', 'i' },
	{ KEY_J, 'J', 'j' },
	{ KEY_K, 'K', 'k' },
	{ KEY_L, 'L', 'l' },
	{ KEY_M, 'M', 'm' },
	{ KEY_N, 'N', 'n' },
	{ KEY_O, 'O', 'o' },
	{ KEY_P, 'P', 'p' },
	{ KEY_Q, 'Q', 'q' },
	{ KEY_R, 'R', 'r' },
	{ KEY_S, 'S', 's' },
	{ KEY_T, 'T', 't' },
	{ KEY_U, 'U', 'u' },
	{ KEY_V, 'V', 'v' },
	{ KEY_W, 'W', 'w' },
	{ KEY_X, 'X', 'x' },
	{ KEY_Y, 'Y', 'y' },
	{ KEY_Z, 'Z', 'z' },
	{ KEY_PAD_0, 0, 0x40000062 },
	{ KEY_PAD_1, 0, 0x40000059 },
	{ KEY_PAD_2, 0, 0x4000005A },
	{ KEY_PAD_3, 0, 0x4000005B },
	{ KEY_PAD_4, 0, 0x4000005C },
	{ KEY_PAD_5, 0, 0x4000005D },
	{ KEY_PAD_6, 0, 0x4000005E },
	{ KEY_PAD_7, 0, 0x4000005F },
	{ KEY_PAD_8, 0, 0x40000060 },
	{ KEY_PAD_9, 0, 0x40000061 },
	{ KEY_PAD_DIVIDE, 0, 0x40000054 },
	{ KEY_PAD_MULTIPLY, 0, 0x40000055 },
	{ KEY_PAD_MINUS, 0, 0x40000056 },
	{ KEY_PAD_PLUS, 0, 0x40000057 },
	{ KEY_PAD_ENTER, 0, 0x40000058 },
	{ KEY_PAD_DECIMAL, 0, 0x400000DC },
	{ KEY_LBRACKET, 0, '[' },
	{ KEY_RBRACKET, 0, ']' },
	{ KEY_SEMICOLON, 0, ';' },
	{ KEY_APOSTROPHE, 0, '\'' },
	{ KEY_BACKQUOTE, 0, '`' },
	{ KEY_COMMA, 0, ',' },
	{ KEY_PERIOD, 0, '.' },
	{ KEY_SLASH, 0, '/' },
	{ KEY_BACKSLASH, 0, '\\' },
	{ KEY_MINUS, 0, '-' },
	{ KEY_EQUAL, 0, '=' },
	{ KEY_ENTER, 0, '\r' },
	{ KEY_SPACE, 0, ' ' },
	{ KEY_BACKSPACE, 0, '\b' },
	{ KEY_TAB, 0, '\t' },
	{ KEY_CAPSLOCK, 0, 0x40000039 },
	{ KEY_NUMLOCK, 0, 0x40000053 },
	{ KEY_ESCAPE, 0, '\x1B' },
	{ KEY_SCROLLLOCK, 0, 0x40000047 },
	{ KEY_INSERT, 0, 0x40000049 },
	{ KEY_DELETE, 0, '\x7F' },
	{ KEY_HOME, 0, 0x4000004A },
	{ KEY_END, 0, 0x4000004D },
	{ KEY_PAGEUP, 0, 0x4000004B },
	{ KEY_PAGEDOWN, 0, 0x4000004E },
	{ KEY_BREAK, 0, 0x40000048 },
	{ KEY_RSHIFT, 0, 0x400000E5 },
	{ KEY_LSHIFT, 0, 0x400000E1 },
	{ KEY_RALT, 0, 0x400000E6 },
	{ KEY_LALT, 0, 0x400000E2 },
	{ KEY_RCONTROL, 0, 0x400000E4 },
	{ KEY_LCONTROL, 0, 0x400000E0 },
	{ KEY_LWIN, 0, 0x400000E3 },
	{ KEY_RWIN, 0, 0x400000E7 },
	{ KEY_APP, 0, 0x40000065 },
	{ KEY_UP, 0, 0x40000052 },
	{ KEY_LEFT, 0, 0x40000050 },
	{ KEY_DOWN, 0, 0x40000051 },
	{ KEY_RIGHT, 0, 0x4000004F },
	{ KEY_F1, 0, 0x4000003A },
	{ KEY_F2, 0, 0x4000003B },
	{ KEY_F3, 0, 0x4000003C },
	{ KEY_F4, 0, 0x4000003D },
	{ KEY_F5, 0, 0x4000003E },
	{ KEY_F6, 0, 0x4000003F },
	{ KEY_F7, 0, 0x40000040 },
	{ KEY_F8, 0, 0x40000041 },
	{ KEY_F9, 0, 0x40000042 },
	{ KEY_F10, 0, 0x40000043 },
	{ KEY_F11, 0, 0x40000044 },
	{ KEY_F12, 0, 0x40000045 },
};

static KeyCode s_pVirtualKeyToVGUI[256];
static int s_pVGUIToVirtualKey[256];

void vgui2::InitKeyTranslationTable()
{
	static bool bInitted = false;
	int i;

	if ( bInitted )
	{
		return;
	}

	bInitted = true;

	for ( i = 0; i < 256; ++i )
	{
		s_pVirtualKeyToVGUI[i] = KEY_NONE;
	}

	for ( i = 0; i < (int)( sizeof( keyMap ) / sizeof( keyMap[0] ) ); ++i )
	{
		s_pVirtualKeyToVGUI[keyMap[i].sdlKeyCode] = keyMap[i].vguiKeyCode;
	}
}

static const char* s_pKeyCodeText[KEY_LAST];

static void InitKeyCodeTextTable()
{
	static bool bInitted = false;

	if ( bInitted )
	{
		return;
	}

	bInitted = true;

	// build key to text translation table
	// first byte unshifted key
	// second byte shifted key
	// the rest is the name of the key
	s_pKeyCodeText[KEY_0]				= "0)KEY_0";
	s_pKeyCodeText[KEY_1]				= "1!KEY_1";
	s_pKeyCodeText[KEY_2]				= "2@KEY_2";
	s_pKeyCodeText[KEY_3]				= "3#KEY_3";
	s_pKeyCodeText[KEY_4]				= "4$KEY_4";
	s_pKeyCodeText[KEY_5]				= "5%KEY_5";
	s_pKeyCodeText[KEY_6]				= "6^KEY_6";
	s_pKeyCodeText[KEY_7]				= "7&KEY_7";
	s_pKeyCodeText[KEY_8]				= "8*KEY_8";
	s_pKeyCodeText[KEY_9]				= "9(KEY_9";
	s_pKeyCodeText[KEY_A]				= "aAKEY_A";
	s_pKeyCodeText[KEY_B]				= "bBKEY_B";
	s_pKeyCodeText[KEY_C]				= "cCKEY_C";
	s_pKeyCodeText[KEY_D]				= "dDKEY_D";
	s_pKeyCodeText[KEY_E]				= "eEKEY_E";
	s_pKeyCodeText[KEY_F]				= "fFKEY_F";
	s_pKeyCodeText[KEY_G]				= "gGKEY_G";
	s_pKeyCodeText[KEY_H]				= "hHKEY_H";
	s_pKeyCodeText[KEY_I]				= "iIKEY_I";
	s_pKeyCodeText[KEY_J]				= "jJKEY_J";
	s_pKeyCodeText[KEY_K]				= "kKKEY_K";
	s_pKeyCodeText[KEY_L]				= "lLKEY_L";
	s_pKeyCodeText[KEY_M]				= "mMKEY_M";
	s_pKeyCodeText[KEY_N]				= "nNKEY_N";
	s_pKeyCodeText[KEY_O]				= "oOKEY_O";
	s_pKeyCodeText[KEY_P]				= "pPKEY_P";
	s_pKeyCodeText[KEY_Q]				= "qQKEY_Q";
	s_pKeyCodeText[KEY_R]				= "rRKEY_R";
	s_pKeyCodeText[KEY_S]				= "sSKEY_S";
	s_pKeyCodeText[KEY_T]				= "tTKEY_T";
	s_pKeyCodeText[KEY_U]				= "uUKEY_U";
	s_pKeyCodeText[KEY_V]				= "vVKEY_V";
	s_pKeyCodeText[KEY_W]				= "wWKEY_W";
	s_pKeyCodeText[KEY_X]				= "xXKEY_X";
	s_pKeyCodeText[KEY_Y]				= "yYKEY_Y";
	s_pKeyCodeText[KEY_Z]				= "zZKEY_Z";
	s_pKeyCodeText[KEY_PAD_0]			= "0\0KEY_PAD_0";
	s_pKeyCodeText[KEY_PAD_1]			= "1\0KEY_PAD_1";
	s_pKeyCodeText[KEY_PAD_2]			= "2\0KEY_PAD_2";
	s_pKeyCodeText[KEY_PAD_3]			= "3\0KEY_PAD_3";
	s_pKeyCodeText[KEY_PAD_4]			= "4\0KEY_PAD_4";
	s_pKeyCodeText[KEY_PAD_5]			= "5\0KEY_PAD_5";
	s_pKeyCodeText[KEY_PAD_6]			= "6\0KEY_PAD_6";
	s_pKeyCodeText[KEY_PAD_7]			= "7\0KEY_PAD_7";
	s_pKeyCodeText[KEY_PAD_8]			= "8\0KEY_PAD_8";
	s_pKeyCodeText[KEY_PAD_9]			= "9\0KEY_PAD_9";
	s_pKeyCodeText[KEY_PAD_DIVIDE]		= "//KEY_PAD_DIVIDE";
	s_pKeyCodeText[KEY_PAD_MULTIPLY]	= "**KEY_PAD_MULTIPLY";
	s_pKeyCodeText[KEY_PAD_MINUS]		= "--KEY_PAD_MINUS";
	s_pKeyCodeText[KEY_PAD_PLUS]		= "++KEY_PAD_PLUS";
	s_pKeyCodeText[KEY_PAD_ENTER]		= "\0\0KEY_PAD_ENTER";
	s_pKeyCodeText[KEY_PAD_DECIMAL]		= ".\0KEY_PAD_DECIMAL";
	s_pKeyCodeText[KEY_LBRACKET]		= "[{KEY_LBRACKET";
	s_pKeyCodeText[KEY_RBRACKET]		= "]}KEY_RBRACKET";
	s_pKeyCodeText[KEY_SEMICOLON]		= ";:KEY_SEMICOLON";
	s_pKeyCodeText[KEY_APOSTROPHE]		= "'\"KEY_APOSTROPHE";
	s_pKeyCodeText[KEY_BACKQUOTE]		= "`~KEY_BACKQUOTE";
	s_pKeyCodeText[KEY_COMMA]			= ",<KEY_COMMA";
	s_pKeyCodeText[KEY_PERIOD]			= ".>KEY_PERIOD";
	s_pKeyCodeText[KEY_SLASH]			= "/?KEY_SLASH";
	s_pKeyCodeText[KEY_BACKSLASH]		= "\\|KEY_BACKSLASH";
	s_pKeyCodeText[KEY_MINUS]			= "-_KEY_MINUS";
	s_pKeyCodeText[KEY_EQUAL]			= "=+KEY_EQUAL";
	s_pKeyCodeText[KEY_ENTER]			= "\0\0KEY_ENTER";
	s_pKeyCodeText[KEY_SPACE]			= "  KEY_SPACE";
	s_pKeyCodeText[KEY_BACKSPACE]		= "\0\0KEY_BACKSPACE";
	s_pKeyCodeText[KEY_TAB]				= "\0\0KEY_TAB";
	s_pKeyCodeText[KEY_CAPSLOCK]		= "\0\0KEY_CAPSLOCK";
	s_pKeyCodeText[KEY_NUMLOCK]			= "\0\0KEY_NUMLOCK";
	s_pKeyCodeText[KEY_ESCAPE]			= "\0\0KEY_ESCAPE";
	s_pKeyCodeText[KEY_SCROLLLOCK]		= "\0\0KEY_SCROLLLOCK";
	s_pKeyCodeText[KEY_INSERT]			= "\0\0KEY_INSERT";
	s_pKeyCodeText[KEY_DELETE]			= "\0\0KEY_DELETE";
	s_pKeyCodeText[KEY_HOME]			= "\0\0KEY_HOME";
	s_pKeyCodeText[KEY_END]				= "\0\0KEY_END";
	s_pKeyCodeText[KEY_PAGEUP]			= "\0\0KEY_PAGEUP";
	s_pKeyCodeText[KEY_PAGEDOWN]		= "\0\0KEY_PAGEDOWN";
	s_pKeyCodeText[KEY_BREAK]			= "\0\0KEY_BREAK";
	s_pKeyCodeText[KEY_LSHIFT]			= "\0\0KEY_LSHIFT";
	s_pKeyCodeText[KEY_RSHIFT]			= "\0\0KEY_RSHIFT";
	s_pKeyCodeText[KEY_LALT]			= "\0\0KEY_LALT";
	s_pKeyCodeText[KEY_RALT]			= "\0\0KEY_RALT";
	s_pKeyCodeText[KEY_LCONTROL]		= "\0\0KEY_LCONTROL";
	s_pKeyCodeText[KEY_RCONTROL]		= "\0\0KEY_RCONTROL";
	s_pKeyCodeText[KEY_LWIN]			= "\0\0KEY_LWIN";
	s_pKeyCodeText[KEY_RWIN]			= "\0\0KEY_RWIN";
	s_pKeyCodeText[KEY_APP]				= "\0\0KEY_APP";
	s_pKeyCodeText[KEY_UP]				= "\0\0KEY_UP";
	s_pKeyCodeText[KEY_LEFT]			= "\0\0KEY_LEFT";
	s_pKeyCodeText[KEY_DOWN]			= "\0\0KEY_DOWN";
	s_pKeyCodeText[KEY_RIGHT]			= "\0\0KEY_RIGHT";
	s_pKeyCodeText[KEY_F1]				= "\0\0KEY_F1";
	s_pKeyCodeText[KEY_F2]				= "\0\0KEY_F2";
	s_pKeyCodeText[KEY_F3]				= "\0\0KEY_F3";
	s_pKeyCodeText[KEY_F4]				= "\0\0KEY_F4";
	s_pKeyCodeText[KEY_F5]				= "\0\0KEY_F5";
	s_pKeyCodeText[KEY_F6]				= "\0\0KEY_F6";
	s_pKeyCodeText[KEY_F7]				= "\0\0KEY_F7";
	s_pKeyCodeText[KEY_F8]				= "\0\0KEY_F8";
	s_pKeyCodeText[KEY_F9]				= "\0\0KEY_F9";
	s_pKeyCodeText[KEY_F10]				= "\0\0KEY_F10";
	s_pKeyCodeText[KEY_F11]				= "\0\0KEY_F11";
	s_pKeyCodeText[KEY_F12]				= "\0\0KEY_F12";
}

void vgui2::InitVGUIToVKTranslationTable()
{
	static bool bInitted = false;
	int i;

	if ( bInitted )
	{
		return;
	}

	bInitted = true;

	for ( i = 0; i < 256; ++i )
	{
		s_pVGUIToVirtualKey[i] = 0;
	}

	for ( i = 0; i < (int)( sizeof( keyMap ) / sizeof( keyMap[0] ) ); ++i )
	{
		ASSERT_AT( s_pVGUIToVirtualKey[ keyMap[i].vguiKeyCode ] == 0, 464 );
		s_pVGUIToVirtualKey[keyMap[i].vguiKeyCode] = keyMap[i].sdlKeyCode;
	}
}

extern "C" KeyCode KeyCode_VirtualKeyToVGUI( int keyCode )
{
	InitKeyTranslationTable();

	switch ( keyCode )
	{
	case '\b':
		return KEY_BACKSPACE;
	case '\t':
	case 0x4000002B:
		return KEY_TAB;
	case '\r':
		return KEY_ENTER;
	case '\x1B':
		return KEY_ESCAPE;
	case ' ':
		return KEY_SPACE;
	case '\'':
		return KEY_APOSTROPHE;
	case ',':
		return KEY_COMMA;
	case '-':
	case 0x4000002D:
		return KEY_MINUS;
	case '.':
		return KEY_PERIOD;
	case '/':
		return KEY_SLASH;
	case '0':
		return KEY_0;
	case '1':
		return KEY_1;
	case '2':
		return KEY_2;
	case '3':
		return KEY_3;
	case '4':
		return KEY_4;
	case '5':
		return KEY_5;
	case '6':
		return KEY_6;
	case '7':
		return KEY_7;
	case '8':
		return KEY_8;
	case '9':
		return KEY_9;
	case ';':
		return KEY_SEMICOLON;
	case '=':
	case 0x4000002E:
		return KEY_EQUAL;
	case '[':
		return KEY_LBRACKET;
	case '\\':
		return KEY_BACKSLASH;
	case ']':
		return KEY_RBRACKET;
	case '`':
	case 0x40000035:
		return KEY_BACKQUOTE;
	case 'a':
		return KEY_A;
	case 'b':
		return KEY_B;
	case 'c':
		return KEY_C;
	case 'd':
		return KEY_D;
	case 'e':
		return KEY_E;
	case 'f':
		return KEY_F;
	case 'g':
		return KEY_G;
	case 'h':
		return KEY_H;
	case 'i':
		return KEY_I;
	case 'j':
		return KEY_J;
	case 'k':
		return KEY_K;
	case 'l':
		return KEY_L;
	case 'm':
		return KEY_M;
	case 'n':
		return KEY_N;
	case 'o':
		return KEY_O;
	case 'p':
		return KEY_P;
	case 'q':
		return KEY_Q;
	case 'r':
		return KEY_R;
	case 's':
		return KEY_S;
	case 't':
		return KEY_T;
	case 'u':
		return KEY_U;
	case 'v':
		return KEY_V;
	case 'w':
		return KEY_W;
	case 'x':
		return KEY_X;
	case 'y':
		return KEY_Y;
	case 'z':
		return KEY_Z;
	case '\x7F':
	case 0x4000004C:
		return KEY_DELETE;
	case 0x40000039:
		return KEY_CAPSLOCK;
	case 0x40000053:
		return KEY_NUMLOCK;
	case 0x40000047:
		return KEY_SCROLLLOCK;
	case 0x40000049:
		return KEY_INSERT;
	case 0x4000004A:
		return KEY_HOME;
	case 0x4000004D:
		return KEY_END;
	case 0x4000004B:
		return KEY_PAGEUP;
	case 0x4000004E:
		return KEY_PAGEDOWN;
	case 0x40000048:
		return KEY_BREAK;
	case 0x400000E5:
		return KEY_RSHIFT;
	case 0x400000E1:
		return KEY_LSHIFT;
	case 0x400000E6:
		return KEY_RALT;
	case 0x400000E2:
		return KEY_LALT;
	case 0x400000E4:
		return KEY_RCONTROL;
	case 0x400000E0:
		return KEY_LCONTROL;
	case 0x400000E3:
		return KEY_LWIN;
	case 0x400000E7:
		return KEY_RWIN;
	case 0x40000052:
		return KEY_UP;
	case 0x40000050:
		return KEY_LEFT;
	case 0x40000051:
		return KEY_DOWN;
	case 0x4000004F:
		return KEY_RIGHT;
	case 0x40000054:
		return KEY_PAD_DIVIDE;
	case 0x40000055:
		return KEY_PAD_MULTIPLY;
	case 0x40000056:
		return KEY_PAD_MINUS;
	case 0x40000057:
		return KEY_PAD_PLUS;
	case 0x40000058:
		return KEY_PAD_ENTER;
	case 0x40000059:
		return KEY_PAD_1;
	case 0x4000005A:
		return KEY_PAD_2;
	case 0x4000005B:
		return KEY_PAD_3;
	case 0x4000005C:
		return KEY_PAD_4;
	case 0x4000005D:
		return KEY_PAD_5;
	case 0x4000005E:
		return KEY_PAD_6;
	case 0x4000005F:
		return KEY_PAD_7;
	case 0x40000060:
		return KEY_PAD_8;
	case 0x40000061:
		return KEY_PAD_9;
	case 0x40000062:
		return KEY_PAD_0;
	case 0x40000063:
	case 0x400000DC:
		return KEY_PAD_DECIMAL;
	case 0x4000003A:
		return KEY_F1;
	case 0x4000003B:
		return KEY_F2;
	case 0x4000003C:
		return KEY_F3;
	case 0x4000003D:
		return KEY_F4;
	case 0x4000003E:
		return KEY_F5;
	case 0x4000003F:
		return KEY_F6;
	case 0x40000040:
		return KEY_F7;
	case 0x40000041:
		return KEY_F8;
	case 0x40000042:
		return KEY_F9;
	case 0x40000043:
		return KEY_F10;
	case 0x40000044:
		return KEY_F11;
	case 0x40000045:
		return KEY_F12;
	default:
		return KEY_NONE;
	}
}

int KeyCode_VGUIToVirtualKey( KeyCode keyCode )
{
	InitVGUIToVKTranslationTable();

	if ( keyCode < 0 || keyCode >= 256 )
	{
		ASSERT_AT( false, 503 );
	}

	return s_pVGUIToVirtualKey[keyCode];
}

bool IsKeyCode( int vk_code )
{
	return (unsigned int)( vk_code - 1 ) <= 0xFE;
}

bool IsMouseCode( int )
{
	return false;
}

MouseCode MouseCode_VirtualKeyToVGUI( int )
{
	return MOUSE_LAST;
}

int MouseCode_VGUIToVirtualKey( MouseCode )
{
	return 0;
}

void KeyCode_GetText( KeyCode code, char* buf, int buflen )
{
	if ( !buf || buflen <= 0 )
	{
		return;
	}

	buf[0] = 0;

	InitKeyCodeTextTable();

	if ( code < 0 || code >= KEY_LAST || !s_pKeyCodeText[code] )
	{
		return;
	}

	// skip 2 in s_pKeyCodeText because the first two are the unshifted and shifted keys
	const char* text = s_pKeyCodeText[code] + 2;
	int i;
	for ( i = 0; i < buflen - 1 && text[i]; i++ )
	{
		buf[i] = text[i];
	}
	buf[i] = 0;
}
