//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Revision: $
// $NoKeywords: $
//
//=============================================================================

#include "keys.h"
#include "../external/SDL2/SDL.h"
#include "client.h"
#include "common.h"
#include "cmd.h"
#include "conprint.h"
#include "filesystem.h"
#include "sys.h"
#include "unicode_strtools.h"
#include "vgui_int.h"
#include "vgui_intwrap2.h"
#include "zone.h"
#include "../common/dll_state.h"

#include <cstdio>
#include <cstring>

#if !defined( _WIN32 )
#ifndef _snprintf
#define _snprintf snprintf
#endif
#ifndef _vsnprintf
#define _vsnprintf vsnprintf
#endif
#endif

/*

key up events are sent even if in console mode

*/
extern qboolean g_fUseDInput;

char* keybindings[KEY_BINDING_COUNT];
int toggleconsole_key;
char key_lines[KEY_HISTORY_LINES][KEY_CONSOLE_LINE_BYTES];
int key_linepos;
qboolean consolekeys[KEY_BINDING_COUNT]; // if true, can't be rebound while in console
qboolean menubound[KEY_BINDING_COUNT];   // if true, can't be rebound while in menu
int keyshift[KEY_BINDING_COUNT];         // key to map to if shift held down in console
keydest_t key_dest;
char chat_buffer[KEY_CHAT_BUFFER_BYTES];
char message_type[KEY_MESSAGE_TYPE_BYTES] = "say";
qboolean message_type_is_privileged = TRUE;
int chat_bufferlen;
qboolean keyGameUIdown[KEY_BINDING_COUNT];
qboolean keydown[KEY_BINDING_COUNT];
int key_repeats[KEY_BINDING_COUNT]; // if > 1, it is autorepeating
int history_line;
int edit_line;
int shift_down;

keyname_t keynames[] =
{
	{ "TAB", K_TAB },
	{ "ENTER", K_ENTER },
	{ "ESCAPE", K_ESCAPE },
	{ "SPACE", K_SPACE },
	{ "BACKSPACE", K_BACKSPACE },
	{ "UPARROW", K_UPARROW },
	{ "DOWNARROW", K_DOWNARROW },
	{ "LEFTARROW", K_LEFTARROW },
	{ "RIGHTARROW", K_RIGHTARROW },

	{ "ALT", K_ALT },
	{ "CTRL", K_CTRL },
	{ "SHIFT", K_SHIFT },

	{ "F1", K_F1 },
	{ "F2", K_F2 },
	{ "F3", K_F3 },
	{ "F4", K_F4 },
	{ "F5", K_F5 },
	{ "F6", K_F6 },
	{ "F7", K_F7 },
	{ "F8", K_F8 },
	{ "F9", K_F9 },
	{ "F10", K_F10 },
	{ "F11", K_F11 },
	{ "F12", K_F12 },

	{ "INS", K_INS },
	{ "DEL", K_DEL },
	{ "PGDN", K_PGDN },
	{ "PGUP", K_PGUP },
	{ "HOME", K_HOME },
	{ "END", K_END },

	{ "MOUSE1", K_MOUSE1 },
	{ "MOUSE2", K_MOUSE2 },
	{ "MOUSE3", K_MOUSE3 },
	{ "MOUSE4", K_MOUSE4 },
	{ "MOUSE5", K_MOUSE5 },

	{ "JOY1", K_JOY1 },
	{ "JOY2", K_JOY2 },
	{ "JOY3", K_JOY3 },
	{ "JOY4", K_JOY4 },

	{ "AUX1", K_AUX1 },
	{ "AUX2", K_AUX2 },
	{ "AUX3", K_AUX3 },
	{ "AUX4", K_AUX4 },
	{ "AUX5", K_AUX5 },
	{ "AUX6", K_AUX6 },
	{ "AUX7", K_AUX7 },
	{ "AUX8", K_AUX8 },
	{ "AUX9", K_AUX9 },
	{ "AUX10", K_AUX10 },
	{ "AUX11", K_AUX11 },
	{ "AUX12", K_AUX12 },
	{ "AUX13", K_AUX13 },
	{ "AUX14", K_AUX14 },
	{ "AUX15", K_AUX15 },
	{ "AUX16", K_AUX16 },
	{ "AUX17", K_AUX17 },
	{ "AUX18", K_AUX18 },
	{ "AUX19", K_AUX19 },
	{ "AUX20", K_AUX20 },
	{ "AUX21", K_AUX21 },
	{ "AUX22", K_AUX22 },
	{ "AUX23", K_AUX23 },
	{ "AUX24", K_AUX24 },
	{ "AUX25", K_AUX25 },
	{ "AUX26", K_AUX26 },
	{ "AUX27", K_AUX27 },
	{ "AUX28", K_AUX28 },
	{ "AUX29", K_AUX29 },
	{ "AUX30", K_AUX30 },
	{ "AUX31", K_AUX31 },
	{ "AUX32", K_AUX32 },

	{ "KP_HOME", K_KP_HOME },
	{ "KP_UPARROW", K_KP_UPARROW },
	{ "KP_PGUP", K_KP_PGUP },
	{ "KP_LEFTARROW", K_KP_LEFTARROW },
	{ "KP_5", K_KP_5 },
	{ "KP_RIGHTARROW", K_KP_RIGHTARROW },
	{ "KP_END", K_KP_END },
	{ "KP_DOWNARROW", K_KP_DOWNARROW },
	{ "KP_PGDN", K_KP_PGDN },
	{ "KP_ENTER", K_KP_ENTER },
	{ "KP_INS", K_KP_INS },
	{ "KP_DEL", K_KP_DEL },
	{ "KP_SLASH", K_KP_SLASH },
	{ "KP_MINUS", K_KP_MINUS },
	{ "KP_PLUS", K_KP_PLUS },
	{ "CAPSLOCK", K_CAPSLOCK },

	{ "MWHEELUP", K_MWHEELUP },
	{ "MWHEELDOWN", K_MWHEELDOWN },

	{ "PAUSE", K_PAUSE },

	{ "SEMICOLON", ';' }, // because a raw semicolon seperates commands

	{ "KP_MUL", K_KP_MUL },
	{ "WIN", K_WIN },

	{ NULL, 0 }
};

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char* Key_KeynumToString( int keynum )
{
	keyname_t* kn;
	static char tinystr[2];

	if ( keynum == -1 )
		return "<KEY NOT FOUND>";

	if ( keynum > 32 && keynum < 127 )
	{
		// printable ascii
		tinystr[0] = (char)keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for ( kn = keynames; kn->name; kn++ )
	{
		if ( keynum == kn->keynum )
			return kn->name;
	}

	return "<UNKNOWN KEYNUM>";
}

/*
============
Key_CountBindings

Count number of lines of bindings we'll be writing
============
*/
int Key_CountBindings()
{
	int i;
	int count = 0;

	for ( i = 0; i < KEY_BINDING_COUNT; i++ )
	{
		if ( keybindings[i] && keybindings[i][0] )
			count++;
	}

	return count;
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings( FileHandle_t f )
{
	int i;

	for ( i = 0; i < KEY_BINDING_COUNT; i++ )
	{
		if ( keybindings[i] && keybindings[i][0] )
			FS_FPrintf( f, "bind \"%s\" \"%s\"\n", Key_KeynumToString( i ), keybindings[i] );
	}
}

/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int Key_StringToKeynum( char* str )
{
	keyname_t* kn;

	if ( !str || !str[0] )
		return -1;
	if ( !str[1] )
		return str[0];

	for ( kn = keynames; kn->name; kn++ )
	{
		if ( !Q_strcasecmp( str, kn->name ) )
			return kn->keynum;
	}

	return -1;
}

/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding( int keynum, char* binding )
{
	char* newbinding;
	int l;

	if ( keynum == -1 || !binding || keynum >= KEY_BINDING_COUNT )
		return;

	// free old bindings
	if ( keybindings[keynum] )
	{
		// Exactly the same, don't re-bind and fragment memory
		if ( !Q_strcmp( keybindings[keynum], binding ) )
			return;

		Z_Free( keybindings[keynum] );
		keybindings[keynum] = NULL;
	}

	// allocate memory for new binding
	l = Q_strlen( binding );
	newbinding = (char*)Z_Malloc( l + 1 );
	Q_strcpy( newbinding, binding );
	newbinding[l] = 0;
	keybindings[keynum] = newbinding;

	if ( !_stricmp( binding, "toggleconsole" ) )
		toggleconsole_key = keynum;
}

/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f()
{
	int b;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "unbind <key> : remove commands from a key\n" );
		return;
	}

	b = Key_StringToKeynum( (char*)Cmd_Argv( 1 ) );
	if ( b == -1 )
	{
		Con_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	if ( b == K_ESCAPE )
	{
		Con_Printf( "Can't unbind ESCAPE key\n" );
		return;
	}

	Key_SetBinding( b, (char*)com_empty_string );
}

void Key_Unbindall_f()
{
	int i;

	for ( i = 0; i < KEY_BINDING_COUNT; i++ )
	{
		if ( keybindings[i] && i != K_ESCAPE )
			Key_SetBinding( i, (char*)com_empty_string );
	}

	Key_SetBinding( '~', "toggleconsole" );
	Key_SetBinding( '`', "toggleconsole" );
	Key_SetBinding( K_ESCAPE, "cancelselect" );
}

/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f()
{
	int i, c, b;
	char cmd[1024];

	c = Cmd_Argc();
	if ( c != 2 && c != 3 )
	{
		Con_Printf( "bind <key> [command] : attach a command to a key\n" );
		return;
	}

	b = Key_StringToKeynum( (char*)Cmd_Argv( 1 ) );
	if ( (unsigned int)b >= KEY_BINDING_COUNT )
	{
		Con_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	if ( c == 2 )
	{
		if ( keybindings[b] )
			Con_Printf( "\"%s\" = \"%s\"\n", Cmd_Argv( 1 ), keybindings[b] );
		else
			Con_Printf( "\"%s\" is not bound\n", Cmd_Argv( 1 ) );
		return;
	}

	if ( b == K_ESCAPE )
	{
		Q_strcpy( cmd, "cancelselect" );
		Key_SetBinding( b, cmd );
		return;
	}

	// copy the rest of the command line
	cmd[0] = 0; // start out with a null string
	for ( i = 2; i < c; i++ )
	{
		if ( i > 2 )
			strncat( cmd, " ", sizeof( cmd ) - 1 - strlen( cmd ) );
		strncat( cmd, Cmd_Argv( i ), sizeof( cmd ) - 1 - strlen( cmd ) );
	}

	Key_SetBinding( b, cmd );
}

/*
===================
Key_Init
===================
*/
void Key_Init()
{
	int i;

	for ( i = 0; i < KEY_HISTORY_LINES; i++ )
	{
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	//
	// init ascii characters in console mode
	//
	for ( i = 32; i < 128; i++ )
		consolekeys[i] = TRUE;
	consolekeys[K_ENTER] = TRUE;
	consolekeys[K_KP_ENTER] = TRUE;
	consolekeys[K_TAB] = TRUE;
	consolekeys[K_LEFTARROW] = TRUE;
	consolekeys[K_KP_LEFTARROW] = TRUE;
	consolekeys[K_RIGHTARROW] = TRUE;
	consolekeys[K_KP_RIGHTARROW] = TRUE;
	consolekeys[K_UPARROW] = TRUE;
	consolekeys[K_KP_UPARROW] = TRUE;
	consolekeys[K_DOWNARROW] = TRUE;
	consolekeys[K_KP_DOWNARROW] = TRUE;
	consolekeys[K_BACKSPACE] = TRUE;
	consolekeys[K_PGUP] = TRUE;
	consolekeys[K_KP_PGUP] = TRUE;
	consolekeys[K_PGDN] = TRUE;
	consolekeys[K_KP_PGDN] = TRUE;
	consolekeys[K_HOME] = TRUE;
	consolekeys[K_KP_HOME] = TRUE;
	consolekeys[K_END] = TRUE;
	consolekeys[K_KP_END] = TRUE;
	consolekeys[K_SHIFT] = TRUE;
	consolekeys[K_ALT] = TRUE;
	consolekeys[K_SPACE] = TRUE;
	consolekeys[K_CTRL] = TRUE;
	consolekeys[K_KP_SLASH] = TRUE;
	consolekeys[K_KP_PLUS] = TRUE;
	consolekeys[K_KP_MINUS] = TRUE;
	consolekeys[K_KP_5] = TRUE;
	consolekeys[K_KP_MUL] = TRUE;
	consolekeys[K_WIN] = FALSE;
	consolekeys['`'] = FALSE;
	consolekeys['~'] = FALSE;

	for ( i = 0; i < KEY_BINDING_COUNT; i++ )
		keyshift[i] = i;
	for ( i = 'a'; i <= 'z'; i++ )
		keyshift[i] = i - 'a' + 'A';
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = TRUE;
	for ( i = 0; i < 12; i++ )
		menubound[K_F1 + i] = TRUE;

	//
	// register our functions
	//
	Cmd_AddCommandWithFlags( "bind", Key_Bind_f, CMD_FLAG_PRIVILEGED );
	Cmd_AddCommandWithFlags( "unbind", Key_Unbind_f, CMD_FLAG_PRIVILEGED );
	Cmd_AddCommandWithFlags( "unbindall", Key_Unbindall_f, CMD_FLAG_PRIVILEGED );
	Cmd_AddCommand( "escape", Key_Escape_f );
}

/*
============
Key_NameForBinding

Returns the keyname to which a binding string is bound.  E.g., if 
TAB is bound to +use then searching for +use will return "TAB"
============
*/
const char* Key_NameForBinding( const char* pBinding )
{
	int i;
	const char* kb;

	g_engdstAddrs.Key_LookupBinding( &pBinding );

	for ( i = 0; i < KEY_BINDING_COUNT; i++ )
	{
		kb = keybindings[i];
		if ( !kb || !kb[0] )
			continue;

		if ( kb[0] == '+' )
			kb++;

		if ( !Q_strnicmp( kb, pBinding, Q_strlen( pBinding ) ) )
			return Key_KeynumToString( i );
	}

	return NULL;
}

const char* Key_BindingForKey( int keynum )
{
	if ( (unsigned int)keynum >= KEY_BINDING_COUNT )
		return NULL;

	return keybindings[keynum];
}

void Key_Shutdown()
{
	memset( keybindings, 0, sizeof( keybindings ) );
}

/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates()
{
	memset( keydown, 0, sizeof( keydown ) );
	memset( keyGameUIdown, 0, sizeof( keyGameUIdown ) );
	memset( key_repeats, 0, sizeof( key_repeats ) );
}

void Key_Message( int key )
{
	void ( *addText )( const char* text );
	char* end;

	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		Cbuf_ReplaceSemiColons();

		addText = message_type_is_privileged ? Cbuf_AddText : Cbuf_AddTextToUntrustedBuffer;
		addText( message_type );
		addText( " \"" );
		addText( chat_buffer );
		addText( "\"\n" );
	}
	else if ( key != K_ESCAPE )
	{
		if ( key == K_BACKSPACE && chat_bufferlen )
		{
			end = (char*)Q_UnicodeAdvance( chat_buffer, Q_UnicodeLength( chat_buffer ) - 1 );
			if ( end )
				*end = 0;
			if ( !Q_UnicodeValidate( chat_buffer ) )
				chat_buffer[0] = 0;
			chat_bufferlen = (int)strlen( chat_buffer );
		}
		return;
	}

	key_dest = key_game;
	chat_bufferlen = 0;
	chat_buffer[0] = 0;
	Q_strcpy( message_type, "say" );
	message_type_is_privileged = TRUE;
}

void Key_Console( int )
{
}

void Key_Escape_f()
{
	if ( giSubState & ENG_ESCAPEEXITS )
	{
		Cbuf_AddText( "disconnect\n" );
		giActive = DLL_PAUSED;
		giStateInfo = STATE_ENDLOGO;
		giSubState = ENG_NORMAL;
		Cbuf_Execute();
	}
	else if ( key_dest == key_game )
	{
		VGuiWrap2_HideGameUI();
		giActive = DLL_PAUSED;
	}
	else if ( key_dest == key_message )
	{
		Key_Message( K_ESCAPE );
	}
	else
	{
		Sys_Error( "Bad key_dest" );
	}
}

//////////////////////////////////////////////////
/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void Key_Event( int key, qboolean down )
{
	int shift;
	char* kb;
	char cmd[1024];

	if ( key >= KEY_BINDING_COUNT )
		return;

	g_engdstAddrs.Key_Event( &key, &down );

	keydown[key] = down;

	if ( keydown[K_CTRL] && keydown[K_ALT] && keydown[K_DEL] )
		Sys_Error( "ctrl-alt-del pressed" );

	if ( key_dest == key_game && ( down || keyGameUIdown[key] )
		&& !ClientDLL_Key_Event( down != 0, key, keybindings[key] ) )
	{
		if ( !key_repeats[key] && down )
			keyGameUIdown[key] = TRUE;
		if ( !down )
		{
			key_repeats[key] = 0;
			keyGameUIdown[key] = FALSE;
		}
		return;
	}

	// update auto-repeat status
	if ( down )
	{
		key_repeats[key]++;
		if ( key != K_BACKSPACE && key != K_PAUSE && key != K_PGUP && key != K_KP_PGUP && key != K_PGDN
			&& key != K_KP_PGDN && key_repeats[key] > 1 )
			return; // ignore most autorepeats

		if ( key >= 200 && !keybindings[key] )
			Con_Printf( "%s is unbound.\n", Key_KeynumToString( key ) );
	}
	else
	{
		key_repeats[key] = 0;
		keyGameUIdown[key] = FALSE;
	}

	if ( key == K_SHIFT )
		shift_down = down;
	shift = shift_down;

	//
	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	//
	if ( !down )
	{
		kb = keybindings[key];
		if ( kb && kb[0] == '+' )
		{
			_snprintf( cmd, sizeof( cmd ), "-%s %i\n", kb + 1, key );
			Cbuf_AddText( cmd );
		}

		if ( keyshift[key] != key )
		{
			kb = keybindings[keyshift[key]];
			if ( kb && kb[0] == '+' )
			{
				_snprintf( cmd, sizeof( cmd ), "-%s %i\n", kb + 1, key );
				Cbuf_AddText( cmd );
			}
		}
		return;
	}

	if ( ( giSubState & ENG_ESCAPEEXITS ) && key != K_ESCAPE )
		return;

	if ( key_dest == key_game && key == K_ESCAPE && VGui_GameUIKeyPressed() )
		return;

	if ( cls.demoplayback && !cls.spectator && consolekeys[key] && key_dest == key_game )
	{
		Con_ToggleConsole_f();
		return;
	}

	//
	// if not a consolekey, send to the interpreter no matter what mode is
	//
	if ( ( key_dest == key_menu && menubound[key] )
		|| ( key_dest == key_game && ( !con_forcedup || !consolekeys[key] ) ) )
	{
		//
		// Send to the interpreter
		//
		kb = keybindings[key];
		if ( !kb )
			return;

		if ( kb[0] == '+' )
		{
			// button commands add keynum as a parm
			_snprintf( cmd, sizeof( cmd ), "%s %i\n", kb, key );
			Cbuf_AddText( cmd );
		}
		else if ( _strnicmp( kb, "toggleconsole", 4 ) || !keydown[K_SHIFT] || con_shifttoggleconsole.value )
		{
			Cbuf_AddText( kb );
			Cbuf_AddText( "\n" );
		}
		return;
	}

	if ( shift )
		key = keyshift[key];

	if ( key_dest == key_message )
		Key_Message( key );
	else if ( key_dest != key_game )
		Sys_Error( "Bad key_dest" );
}

SDL_Keycode GetSDLKeycodeFromEngineKey( int key )
{
	SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;

	if ( key >= 'a' && key <= 'z' )
		return SDL_GetKeyFromScancode( (SDL_Scancode)( SDL_SCANCODE_A + key - 'a' ) );
	if ( key >= K_F1 && key <= K_F12 )
		return SDL_GetKeyFromScancode( (SDL_Scancode)( SDL_SCANCODE_F1 + key - K_F1 ) );

	switch ( key )
	{
	case K_TAB:
		scancode = SDL_SCANCODE_TAB;
		break;
	case K_ENTER:
		scancode = SDL_SCANCODE_RETURN;
		break;
	case K_ESCAPE:
		scancode = SDL_SCANCODE_ESCAPE;
		break;
	case K_SPACE:
		scancode = SDL_SCANCODE_SPACE;
		break;
	case '\'':
		scancode = SDL_SCANCODE_APOSTROPHE;
		break;
	case ',':
		scancode = SDL_SCANCODE_COMMA;
		break;
	case '-':
		scancode = SDL_SCANCODE_MINUS;
		break;
	case '.':
		scancode = SDL_SCANCODE_PERIOD;
		break;
	case '/':
		scancode = SDL_SCANCODE_SLASH;
		break;
	case '0':
		scancode = SDL_SCANCODE_0;
		break;
	case '1':
		scancode = SDL_SCANCODE_1;
		break;
	case '2':
		scancode = SDL_SCANCODE_2;
		break;
	case '3':
		scancode = SDL_SCANCODE_3;
		break;
	case '4':
		scancode = SDL_SCANCODE_4;
		break;
	case '5':
		scancode = SDL_SCANCODE_5;
		break;
	case '6':
		scancode = SDL_SCANCODE_6;
		break;
	case '7':
		scancode = SDL_SCANCODE_7;
		break;
	case '8':
		scancode = SDL_SCANCODE_8;
		break;
	case '9':
		scancode = SDL_SCANCODE_9;
		break;
	case ';':
		scancode = SDL_SCANCODE_SEMICOLON;
		break;
	case '=':
		scancode = SDL_SCANCODE_EQUALS;
		break;
	case '[':
		scancode = SDL_SCANCODE_LEFTBRACKET;
		break;
	case '\\':
		scancode = SDL_SCANCODE_BACKSLASH;
		break;
	case ']':
		scancode = SDL_SCANCODE_RIGHTBRACKET;
		break;
	case '`':
		scancode = SDL_SCANCODE_GRAVE;
		break;
	case K_BACKSPACE:
		scancode = SDL_SCANCODE_BACKSPACE;
		break;
	case K_UPARROW:
		scancode = SDL_SCANCODE_UP;
		break;
	case K_DOWNARROW:
		scancode = SDL_SCANCODE_DOWN;
		break;
	case K_LEFTARROW:
		scancode = SDL_SCANCODE_LEFT;
		break;
	case K_RIGHTARROW:
		scancode = SDL_SCANCODE_RIGHT;
		break;
	case K_ALT:
		scancode = SDL_SCANCODE_LALT;
		break;
	case K_CTRL:
		scancode = SDL_SCANCODE_LCTRL;
		break;
	case K_SHIFT:
		scancode = SDL_SCANCODE_LSHIFT;
		break;
	case K_INS:
		scancode = SDL_SCANCODE_INSERT;
		break;
	case K_DEL:
		scancode = SDL_SCANCODE_DELETE;
		break;
	case K_PGDN:
		scancode = SDL_SCANCODE_PAGEDOWN;
		break;
	case K_PGUP:
		scancode = SDL_SCANCODE_PAGEUP;
		break;
	case K_HOME:
		scancode = SDL_SCANCODE_HOME;
		break;
	case K_END:
		scancode = SDL_SCANCODE_END;
		break;
	case K_KP_HOME:
		scancode = SDL_SCANCODE_KP_7;
		break;
	case K_KP_UPARROW:
		scancode = SDL_SCANCODE_KP_8;
		break;
	case K_KP_PGUP:
		scancode = SDL_SCANCODE_KP_9;
		break;
	case K_KP_LEFTARROW:
		scancode = SDL_SCANCODE_KP_4;
		break;
	case K_KP_5:
		scancode = SDL_SCANCODE_KP_5;
		break;
	case K_KP_RIGHTARROW:
		scancode = SDL_SCANCODE_KP_6;
		break;
	case K_KP_END:
		scancode = SDL_SCANCODE_KP_1;
		break;
	case K_KP_DOWNARROW:
		scancode = SDL_SCANCODE_KP_2;
		break;
	case K_KP_PGDN:
		scancode = SDL_SCANCODE_KP_3;
		break;
	case K_KP_ENTER:
		scancode = SDL_SCANCODE_KP_ENTER;
		break;
	case K_KP_INS:
		scancode = SDL_SCANCODE_KP_0;
		break;
	case K_KP_DEL:
#if defined( _WIN32 )
		scancode = SDL_SCANCODE_KP_DECIMAL;
#else
		scancode = SDL_SCANCODE_KP_PERIOD;
#endif
		break;
	case K_KP_SLASH:
		scancode = SDL_SCANCODE_KP_DIVIDE;
		break;
	case K_KP_MINUS:
		scancode = SDL_SCANCODE_KP_MINUS;
		break;
	case K_KP_PLUS:
		scancode = SDL_SCANCODE_KP_PLUS;
		break;
	case K_CAPSLOCK:
		scancode = SDL_SCANCODE_CAPSLOCK;
		break;
	case K_KP_MUL:
		scancode = SDL_SCANCODE_KP_MULTIPLY;
		break;
	case K_WIN:
		scancode = SDL_SCANCODE_LGUI;
		break;
	case K_PAUSE:
		scancode = SDL_SCANCODE_PAUSE;
		break;
	}
	return SDL_GetKeyFromScancode( scancode );
}

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

qboolean CheckForCommand()
{
	char command[128];
	char* cmd;
	char* s;
	int i;

	s = key_lines[edit_line];
	for ( i = 0; i < (int)sizeof( command ) - 1; i++ )
	{
		if ( (unsigned char)s[i + 1] <= ' ' )
			break;
		command[i] = s[i + 1];
	}
	command[i] = 0;

	cmd = Cmd_CompleteCommand( command, shift_down < 1 ? 1 : -1 );
	if ( !cmd || Q_strcmp( cmd, command ) )
		cmd = Cvar_CompleteVariable( command, shift_down < 1 ? 1 : -1 );
	if ( !cmd || Q_strcmp( cmd, command ) )
		return FALSE; // just a chat message

	return TRUE;
}

void CompleteCommand()
{
	char* cmd;
	char* s;
	int len;

	s = key_lines[edit_line] + 1;
	if ( *s == '\\' || *s == '/' )
		s++;

	cmd = Cmd_CompleteCommand( s, shift_down < 1 ? 1 : -1 );
	if ( !cmd )
		cmd = Cvar_CompleteVariable( s, shift_down < 1 ? 1 : -1 );
	if ( !cmd )
		return;

	key_lines[edit_line][1] = '/';
	snprintf( key_lines[edit_line] + 2, KEY_CONSOLE_LINE_BYTES - 3, "%s", cmd );
	len = Q_strlen( key_lines[edit_line] + 2 );
	key_lines[edit_line][len + 2] = ' ';
	key_linepos = len + 3;
	key_lines[edit_line][key_linepos] = 0;
}

void Key_UpLine()
{
	if ( edit_line < 0 || edit_line >= KEY_HISTORY_LINES )
		return;
	do
	{
		history_line = ( history_line - 1 ) & ( KEY_HISTORY_LINES - 1 );
	} while ( history_line != edit_line && !key_lines[history_line][1] );

	if ( history_line == edit_line )
		history_line = ( edit_line + 1 ) & ( KEY_HISTORY_LINES - 1 );

	snprintf( key_lines[edit_line], KEY_CONSOLE_LINE_BYTES, "%s", key_lines[history_line] );
	key_linepos = Q_strlen( key_lines[edit_line] );
}

void Cbuf_ReplaceSemiColons()
{
	char* s;

	for ( s = chat_buffer; *s; s++ )
	{
		if ( *s == ';' )
			*s = ',';
	}
}

void Key_EventStub( int key, qboolean down )
{
	if ( !g_fUseDInput )
		Key_Event( key, down );
}
