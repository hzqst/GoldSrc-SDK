//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "OptionsSubKeyboard.h"
#include "GameUI_Interface.h"
#include "vcontrolslistpanel.h"

#include "Button.h"
#include "QueryBox.h"
#include "FileSystem.h"
#include "IKeyValues.h"
#include "../engine/utlbuffer.h"
#include "igameuifuncs.h"
#include "keydefs.h"
#include "vgui/Cursor.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"
#include "../external/SDL2/SDL_keyboard.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#define KEYBIND_PATH_MAX	_MAX_PATH
#define MOUSE4_KEYNAME	"MOUSE4"
#define MOUSE5_KEYNAME	"MOUSE5"
#else
#include <strings.h>
#define stricmp			strcasecmp
#define KEYBIND_PATH_MAX	PATH_MAX
#define MOUSE4_KEYNAME	"MOUSE5"
#define MOUSE5_KEYNAME	"MOUSE4"
#endif

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"GameUI\\OptionsSubKeyboard.cpp" )

#define VARARGS_BUFFER_COUNT	4
#define VARARGS_BUFFER_SIZE	1024

static char token[1024];

char* UTIL_va( char* format, ... )
{
	static char string[VARARGS_BUFFER_COUNT][VARARGS_BUFFER_SIZE];
	static int curstring;
	curstring = ( curstring + 1 ) % VARARGS_BUFFER_COUNT;

	va_list args;
	va_start( args, format );
#ifdef _WIN32
	_vsnprintf( string[curstring], sizeof( string[curstring] ), format, args );
#else
	vsnprintf( string[curstring], sizeof( string[curstring] ), format, args );
#endif
	va_end( args );
	return string[curstring];
}

SDL_Keycode GetSDLKeycodeFromEngineKey( int key )
{
	SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
	if ( key >= 'a' && key <= 'z' )
		return SDL_GetKeyFromScancode( (SDL_Scancode)( SDL_SCANCODE_A + key - 'a' ) );
	if ( key >= K_F1 && key <= K_F12 )
		return SDL_GetKeyFromScancode( (SDL_Scancode)( SDL_SCANCODE_F1 + key - K_F1 ) );
	if ( key >= '1' && key <= '9' )
		return SDL_GetKeyFromScancode( (SDL_Scancode)( SDL_SCANCODE_1 + key - '1' ) );

	switch ( key )
	{
	case K_TAB: scancode = SDL_SCANCODE_TAB; break;
	case K_ENTER: scancode = SDL_SCANCODE_RETURN; break;
	case K_ESCAPE: scancode = SDL_SCANCODE_ESCAPE; break;
	case K_SPACE: scancode = SDL_SCANCODE_SPACE; break;
	case '\'': scancode = SDL_SCANCODE_APOSTROPHE; break;
	case ',': scancode = SDL_SCANCODE_COMMA; break;
	case '-': scancode = SDL_SCANCODE_MINUS; break;
	case '.': scancode = SDL_SCANCODE_PERIOD; break;
	case '/': scancode = SDL_SCANCODE_SLASH; break;
	case '0': scancode = SDL_SCANCODE_0; break;
	case ';': scancode = SDL_SCANCODE_SEMICOLON; break;
	case '=': scancode = SDL_SCANCODE_EQUALS; break;
	case '[': scancode = SDL_SCANCODE_LEFTBRACKET; break;
	case '\\': scancode = SDL_SCANCODE_BACKSLASH; break;
	case ']': scancode = SDL_SCANCODE_RIGHTBRACKET; break;
	case '`': scancode = SDL_SCANCODE_GRAVE; break;
	case K_BACKSPACE: scancode = SDL_SCANCODE_BACKSPACE; break;
	case K_UPARROW: scancode = SDL_SCANCODE_UP; break;
	case K_DOWNARROW: scancode = SDL_SCANCODE_DOWN; break;
	case K_LEFTARROW: scancode = SDL_SCANCODE_LEFT; break;
	case K_RIGHTARROW: scancode = SDL_SCANCODE_RIGHT; break;
	case K_ALT: scancode = SDL_SCANCODE_LALT; break;
	case K_CTRL: scancode = SDL_SCANCODE_LCTRL; break;
	case K_SHIFT: scancode = SDL_SCANCODE_LSHIFT; break;
	case K_INS: scancode = SDL_SCANCODE_INSERT; break;
	case K_DEL: scancode = SDL_SCANCODE_DELETE; break;
	case K_PGDN: scancode = SDL_SCANCODE_PAGEDOWN; break;
	case K_PGUP: scancode = SDL_SCANCODE_PAGEUP; break;
	case K_HOME: scancode = SDL_SCANCODE_HOME; break;
	case K_END: scancode = SDL_SCANCODE_END; break;
	case K_KP_HOME: scancode = SDL_SCANCODE_KP_7; break;
	case K_KP_UPARROW: scancode = SDL_SCANCODE_KP_8; break;
	case K_KP_PGUP: scancode = SDL_SCANCODE_KP_9; break;
	case K_KP_LEFTARROW: scancode = SDL_SCANCODE_KP_4; break;
	case K_KP_5: scancode = SDL_SCANCODE_KP_5; break;
	case K_KP_RIGHTARROW: scancode = SDL_SCANCODE_KP_6; break;
	case K_KP_END: scancode = SDL_SCANCODE_KP_1; break;
	case K_KP_DOWNARROW: scancode = SDL_SCANCODE_KP_2; break;
	case K_KP_PGDN: scancode = SDL_SCANCODE_KP_3; break;
	case K_KP_ENTER: scancode = SDL_SCANCODE_KP_ENTER; break;
	case K_KP_INS: scancode = SDL_SCANCODE_KP_0; break;
	case K_KP_DEL: scancode = SDL_SCANCODE_KP_PERIOD; break;
	case K_KP_SLASH: scancode = SDL_SCANCODE_KP_DIVIDE; break;
	case K_KP_MINUS: scancode = SDL_SCANCODE_KP_MINUS; break;
	case K_KP_PLUS: scancode = SDL_SCANCODE_KP_PLUS; break;
	case K_CAPSLOCK: scancode = SDL_SCANCODE_CAPSLOCK; break;
	case K_KP_MUL: scancode = SDL_SCANCODE_KP_MULTIPLY; break;
	case K_WIN: scancode = SDL_SCANCODE_LGUI; break;
	case K_PAUSE: scancode = SDL_SCANCODE_PAUSE; break;
	}
	return SDL_GetKeyFromScancode( scancode );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubKeyboard::COptionsSubKeyboard( vgui2::Panel* parent )
	: BaseClass( parent, NULL, true )
{
	memset( m_Bindings, 0, sizeof( m_Bindings ) );
	// create the key bindings list
	CreateKeyBindingList();
	// Store all current key bindings
	SaveCurrentBindings();
	// Parse default descriptions
	ParseActionDescriptions();
	m_pSetBindingButton = new vgui2::Button( this, "ChangeKeyButton", "" );
	m_pClearBindingButton = new vgui2::Button( this, "ClearKeyButton", "" );
	LoadControlSettings( "Resource/OptionsSubKeyboard.res", NULL );
	m_pSetBindingButton->SetEnabled( false );
	m_pClearBindingButton->SetEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsSubKeyboard::~COptionsSubKeyboard()
{
	DeleteSavedBindings();
}

//-----------------------------------------------------------------------------
// Purpose: Create key bindings list control
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::CreateKeyBindingList()
{
	// Create the control
	m_pKeyBindList = new VControlsListPanel( this, "listpanel_keybindlist" );
}

//-----------------------------------------------------------------------------
// Purpose: Search current data set for item which has the specified binding string
// Input  : *binding - string to find
// Output : KeyValues or NULL on failure
//-----------------------------------------------------------------------------
KeyValues* COptionsSubKeyboard::GetItemForBinding( const char* binding )
{
	static HKeySymbol bindingSymbol = keyvalues()->GetSymbolForString( "Binding" );
	// Loop through all items
	for ( int i = 0; i < m_pKeyBindList->GetItemCount(); i++ )
	{
		KeyValues* item = m_pKeyBindList->GetItemData( m_pKeyBindList->GetItemIDFromRow( i ) );
		if ( item && !stricmp( item->FindKey( bindingSymbol )->GetString( NULL, "" ), binding ) )
			return item;
	}
	// Didn't find it
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve the bindable key name for an ascii index
//-----------------------------------------------------------------------------
const char* COptionsSubKeyboard::GetKeyName( int keynum )
{
	return gameuifuncs->Key_NameForKey( keynum );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int COptionsSubKeyboard::FindKeyForName( const char* keyname )
{
	for ( int i = 0; i < 256; i++ )
	{
		const char* name = gameuifuncs->Key_NameForKey( i );
		if ( !name || !name[0] )
			continue;
		if ( strlen( keyname ) == 1 ? !strcmp( keyname, name ) : !stricmp( keyname, name ) )
			return i;
	}
	return -1;
}

void COptionsSubKeyboard::AddBinding( KeyValues* item, const char* keyname, const char* displayName )
{
	// See if it's already there as a primary or alternate binding
	if ( !stricmp( item->GetString( "Key", "" ), keyname ) )
		return;

	// Make sure it doesn't live anywhere
	RemoveKeyFromBindItems( keyname );
	item->SetString( "AltKey", item->GetString( "Key", "" ) );
	item->SetString( "Key", keyname );
	item->SetString( "AltCode", item->GetString( "Code", "" ) );
	item->SetString( "Code", displayName );
}

//-----------------------------------------------------------------------------
// Purpose: Remove all keys from list
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::ClearBindItems()
{
	m_KeysToUnbind.RemoveAll();
	for ( int i = 0; i < m_pKeyBindList->GetItemCount(); i++ )
	{
		KeyValues* item = m_pKeyBindList->GetItemData( m_pKeyBindList->GetItemIDFromRow( i ) );
		if ( !item )
			continue;
		// Reset keys
		item->SetString( "Key", "" );
		item->SetString( "AltKey", "" );
		item->SetString( "Code", "" );
		item->SetString( "AltCode", "" );
		m_pKeyBindList->InvalidateItem( i );
	}
	m_pKeyBindList->InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Remove all instances of the specified key from bindings
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::RemoveKeyFromBindItems( const char* key )
{
	ASSERT_AT( key && key[ 0 ], 639 );
	if ( !key || !key[0] )
		return;

	for ( int i = 0; i < m_pKeyBindList->GetItemCount(); i++ )
	{
		KeyValues* item = m_pKeyBindList->GetItemData( m_pKeyBindList->GetItemIDFromRow( i ) );
		if ( !item )
			continue;
		// If it's bound to the alternate: just unbind it and
		//  remove the alternate key
		if ( !stricmp( key, item->GetString( "AltKey", "" ) ) )
		{
			// Tell the item
			item->SetString( "AltKey", "" );
			item->SetString( "AltCode", "" );
			m_pKeyBindList->InvalidateItem( i );
		}
		// If it's bound to the primary: then remove it and copy
		//  any alternate over to the primary to take its place
		if ( !stricmp( key, item->GetString( "Key", "" ) ) )
		{
			item->SetString( "Key", "" );
			item->SetString( "Code", "" );
			m_pKeyBindList->InvalidateItem( i );
			// Is there still an alternate?
			const char* alternate = item->GetString( "AltKey", "" );
			// If so: shift it over
			if ( alternate && alternate[0] )
			{
				item->SetString( "Key", alternate );
				item->SetString( "Code", item->GetString( "AltCode", "" ) );
				item->SetString( "AltKey", "" );
				item->SetString( "AltCode", "" );
			}
		}
	}
	m_pKeyBindList->InvalidateLayout( false, false );
}

void COptionsSubKeyboard::FillInCurrentBindings()
{
	// Clear any current settings
	ClearBindItems();
	for ( int key = 0; key < 256; key++ )
	{
		// Look up binding
		const char* binding = gameuifuncs->Key_BindingForKey( key );
		if ( !binding )
			continue;
		SDL_Keycode sdlKey = GetSDLKeycodeFromEngineKey( key );
		const char* displayName;
		switch ( key )
		{
		case K_MOUSE1: displayName = "MOUSE1"; break;
		case K_MOUSE2: displayName = "MOUSE2"; break;
		case K_MOUSE3: displayName = "MOUSE3"; break;
		case K_MOUSE4: displayName = MOUSE4_KEYNAME; break;
		case K_MOUSE5: displayName = MOUSE5_KEYNAME; break;
		case K_MWHEELUP: displayName = "Mouse Wheel Up"; break;
		case K_MWHEELDOWN: displayName = "Mouse Wheel Down"; break;
		default: displayName = SDL_GetKeyName( sdlKey ); break;
		}
		// See if there is an item for this one?
		KeyValues* item = GetItemForBinding( binding );
		if ( !item )
			continue;
		const char* keyname = gameuifuncs->Key_NameForKey( key );
		AddBinding( item, keyname, displayName );
		m_KeysToUnbind.AddToTail( CUtlSymbol( keyname ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: reloads current keybinding
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnResetData()
{
	// debug timing code, this function takes too long
//	double s4 = system()->GetCurrentTime();
	// Populate list based on current settings
	FillInCurrentBindings();
	m_pKeyBindList->SetSelectedItem( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Clean up memory used by saved bindings
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::DeleteSavedBindings()
{
	for ( int i = 0; i < 256; i++ )
	{
		if ( m_Bindings[i].binding )
		{
#ifdef _WIN32
			free( m_Bindings[i].binding );
#else
			delete[] m_Bindings[i].binding;
#endif
		}
		m_Bindings[i].binding = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copy all bindings into save array
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::SaveCurrentBindings()
{
	DeleteSavedBindings();
	for ( int i = 0; i < 256; i++ )
	{
		const char* binding = gameuifuncs->Key_BindingForKey( i );
		if ( !binding || !binding[0] )
			continue;
#ifdef _WIN32
		m_Bindings[i].binding = (char*)malloc( strlen( binding ) + 1 );
#else
		m_Bindings[i].binding = new char[strlen( binding ) + 1];
#endif
		strcpy( m_Bindings[i].binding, binding );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells the engine to bind a key
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::BindKey( const char* key, const char* binding )
{
	engine->pfnClientCmd( UTIL_va( (char*)"bind \"%s\" \"%s\"\n", key, binding ) );
}

//-----------------------------------------------------------------------------
// Purpose: Tells the engine to unbind a key
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::UnbindKey( const char* key )
{
	engine->pfnClientCmd( UTIL_va( (char*)"unbind \"%s\"\n", key ) );
}

void COptionsSubKeyboard::ApplyAllBindings()
// unbind everything that the user unbound
{
	for ( int i = 0; i < m_KeysToUnbind.Count(); i++ )
		UnbindKey( m_KeysToUnbind[i].String() );
	m_KeysToUnbind.RemoveAll();
	// free binding memory
	DeleteSavedBindings();

	for ( int i = 0; i < m_pKeyBindList->GetItemCount(); i++ )
	{
		KeyValues* item = m_pKeyBindList->GetItemData( m_pKeyBindList->GetItemIDFromRow( i ) );
		if ( !item )
			continue;
		// See if it has a binding
		const char* binding = item->GetString( "Binding", "" );
		if ( !binding || !binding[0] )
			continue;
		const char* keyname = item->GetString( "Key", "" );
		if ( keyname && keyname[0] )
		{
			// Tell the engine
			BindKey( keyname, binding );
			int key = FindKeyForName( keyname );
			if ( key != -1 )
			{
#ifdef _WIN32
				m_Bindings[key].binding = (char*)malloc( strlen( binding ) + 1 );
#else
				m_Bindings[key].binding = new char[strlen( binding ) + 1];
#endif
				strcpy( m_Bindings[key].binding, binding );
			}
		}
		// Check alternate binding
		keyname = item->GetString( "AltKey", "" );
		if ( keyname && keyname[0] )
		{
			// Tell the engine
			BindKey( keyname, binding );
			int key = FindKeyForName( keyname );
			if ( key != -1 )
			{
#ifdef _WIN32
				m_Bindings[key].binding = (char*)malloc( strlen( binding ) + 1 );
#else
				m_Bindings[key].binding = new char[strlen( binding ) + 1];
#endif
				strcpy( m_Bindings[key].binding, binding );
			}
		}
	}
	engine->pfnClientCmd( (char*)"exec userconfig.cfg\n" );
}

//-----------------------------------------------------------------------------
// Purpose: saves out keybinding changes
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnApplyChanges()
{
	ApplyAllBindings();
}

void COptionsSubKeyboard::FillInDefaultBindings()
{
	char filename[KEYBIND_PATH_MAX];
	sprintf( filename, "%s/kb_def.lst", "gfx/shell" );
	FileHandle_t file = vgui2::filesystem()->Open( filename, "rb", NULL );
	if ( !file )
		return;
	int size = vgui2::filesystem()->Size( file );
	CUtlBuffer buffer( 0, size, true );
	vgui2::filesystem()->Read( buffer.Base(), size, file );
	vgui2::filesystem()->Close( file );
	// Clear out all current bindings
	ClearBindItems();

	char* data = (char*)buffer.Base();
	while ( true )
	{
		data = engine->COM_ParseFile( data, token );
		if ( !token[0] )
			break;
		char keyname[256];
		strcpy( keyname, token );
		data = engine->COM_ParseFile( data, token );
		if ( !token[0] )
			break;
		char binding[256];
		strcpy( binding, token );
		KeyValues* item = GetItemForBinding( binding );
		if ( !item )
			continue;
		int key = gameuifuncs->Key_KeyStringToKeyNum( keyname );
		SDL_Keycode sdlKey = GetSDLKeycodeFromEngineKey( key );
		const char* displayName;
		switch ( key )
		{
		case K_MOUSE1: displayName = "MOUSE1"; break;
		case K_MOUSE2: displayName = "MOUSE2"; break;
		case K_MOUSE3: displayName = "MOUSE3"; break;
		case K_MOUSE4: displayName = MOUSE4_KEYNAME; break;
		case K_MOUSE5: displayName = MOUSE5_KEYNAME; break;
		default: displayName = SDL_GetKeyName( sdlKey ); break;
		}
		AddBinding( item, keyname, displayName );
	}

	PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
	KeyValues* console = GetItemForBinding( "toggleconsole" );
	if ( console )
		AddBinding( console, "`", "`" );
	KeyValues* cancel = GetItemForBinding( "cancelselect" );
	if ( cancel )
		AddBinding( cancel, "ESCAPE", "ESCAPE" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::ParseActionDescriptions()
{
	char filename[KEYBIND_PATH_MAX];
	sprintf( filename, "%s/kb_act.lst", "gfx/shell" );
	FileHandle_t file = vgui2::filesystem()->Open( filename, "rb", NULL );
	if ( !file )
		return;
	int size = vgui2::filesystem()->Size( file );
	CUtlBuffer buffer( 0, size + 1, true );
	vgui2::filesystem()->Read( buffer.Base(), size, file );
	vgui2::filesystem()->Close( file );
	char* data = (char*)buffer.Base();
	data[size] = 0;
	int section = 0;
	while ( true )
	{
		data = engine->COM_ParseFile( data, token );
		if ( !token[0] )
			break;
		char binding[256];
		strcpy( binding, token );
		data = engine->COM_ParseFile( data, token );
		if ( !token[0] )
			break;
		char description[256];
		strcpy( description, token );
		if ( description[0] == '=' )
			continue;
		// Flag as special header row if binding is "blank"
		if ( !stricmp( binding, "blank" ) )
		{
			section++;
			m_pKeyBindList->AddSection( section, description, NULL );
			m_pKeyBindList->AddColumnToSection( section, "Action", description,
				vgui2::SectionedListPanel::COLUMN_BRIGHT, GetProportionalScaledValue( 226 ), 0 );
			m_pKeyBindList->AddColumnToSection( section, "Code", "#GameUI_KeyButton",
				vgui2::SectionedListPanel::COLUMN_BRIGHT, GetProportionalScaledValue( 128 ), 0 );
			m_pKeyBindList->AddColumnToSection( section, "AltCode", "#GameUI_Alternate",
				vgui2::SectionedListPanel::COLUMN_BRIGHT, GetProportionalScaledValue( 128 ), 0 );
		}
		else
		{
			KeyValues* item = new KeyValues( "Item" );
			// fill in data
			item->SetString( "Action", description );
			item->SetString( "Binding", binding );
			item->SetString( "Key", "" );
			item->SetString( "AltKey", "" );
			item->SetString( "Code", "" );
			item->SetString( "AltCode", "" );
			// Add to list
			m_pKeyBindList->AddItem( section, item );
			item->deleteThis();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: User clicked on item: remember where last active row/column was
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::ItemSelected( int itemID )
{
	m_pKeyBindList->SetItemOfInterest( itemID );
	if ( !m_pKeyBindList->IsItemIDValid( itemID ) )
	{
		m_pSetBindingButton->SetEnabled( false );
		m_pClearBindingButton->SetEnabled( false );
		return;
	}
	m_pSetBindingButton->SetEnabled( true );
	KeyValues* item = m_pKeyBindList->GetItemData( itemID );
	if ( !item )
		return;
	const char* key = item->GetString( "Key", NULL );
	m_pClearBindingButton->SetEnabled( key && key[0] );
	if ( item->GetInt( "Header", 0 ) )
		m_pSetBindingButton->SetEnabled( false );
}

void COptionsSubKeyboard::Finish( int key, int button, const char* keyName )
{
	int itemID = m_pKeyBindList->GetItemOfInterest();
	m_pKeyBindList->EndCaptureMode( vgui2::dc_arrow );
	// Find item for this row
	KeyValues* item = m_pKeyBindList->GetItemData( itemID );
	if ( item )
	{
		if ( button )
		{
			const char* keyname = "MOUSE1";
			if ( button & 1 )
				keyname = "MOUSE1";
			else if ( button & 2 )
				keyname = "MOUSE2";
			else if ( button & 4 )
				keyname = "MOUSE3";
			else if ( button & 8 )
				keyname = MOUSE4_KEYNAME;
			else if ( button & 16 )
				keyname = MOUSE5_KEYNAME;
			AddBinding( item, keyname, keyName );
			PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
		}
		else if ( key && key != K_ESCAPE )
		{
			// Bind the named key
			AddBinding( item, GetKeyName( key ), keyName );
			PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
		}
		m_pKeyBindList->InvalidateItem( itemID );
	}
	m_pSetBindingButton->SetEnabled( true );
	m_pClearBindingButton->SetEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: command handler
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnCommand( const char* command )
{
	if ( !stricmp( command, "Defaults" ) )
	{
		vgui2::QueryBox* query = new vgui2::QueryBox( "#GameUI_KeyboardSettings",
			"#GameUI_KeyboardSettingsText", NULL );
		query->AddActionSignalTarget( this );
		query->SetOKCommand( new KeyValues( "Command", "command", "DefaultsOK" ) );
		query->DoModal( NULL );
	}
	else if ( !stricmp( command, "DefaultsOK" ) )
	{
		// Restore defaults from default keybindings file
		FillInDefaultBindings();
		m_pKeyBindList->RequestFocus( 0 );
	}
	else if ( !m_pKeyBindList->IsCapturing() && !stricmp( command, "ChangeKey" ) )
	{
		m_pKeyBindList->StartCaptureMode( vgui2::dc_none );
	}
	else if ( !m_pKeyBindList->IsCapturing() && !stricmp( command, "ClearKey" ) )
	{
		OnKeyCodePressed( vgui2::KEY_DELETE );
		m_pKeyBindList->RequestFocus( 0 );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: key input handler
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnKeyTyped( wchar_t unichar )
{
	BaseClass::OnKeyTyped( unichar );
}

void COptionsSubKeyboard::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( !m_pKeyBindList->IsCapturing() )
	{
		BaseClass::OnKeyCodeTyped( code );
		return;
	}
	SDL_Keycode key = vgui2::system()->KeyCode_VGUIToVirtualKey( code );
	SDL_Scancode scancode = SDL_GetScancodeFromKey( key );
	int engineKey = 0;
	if ( scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z )
	{
		engineKey = 'a' + scancode - SDL_SCANCODE_A;
	}
	else if ( scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9 )
	{
		engineKey = '1' + scancode - SDL_SCANCODE_1;
	}
	else if ( scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12 )
	{
		engineKey = K_F1 + scancode - SDL_SCANCODE_F1;
	}
	else
	{
		switch ( scancode )
		{
		case SDL_SCANCODE_0: engineKey = '0'; break;
		case SDL_SCANCODE_RETURN: engineKey = K_ENTER; break;
		case SDL_SCANCODE_ESCAPE: engineKey = K_ESCAPE; break;
		case SDL_SCANCODE_BACKSPACE: engineKey = K_BACKSPACE; break;
		case SDL_SCANCODE_TAB: engineKey = K_TAB; break;
		case SDL_SCANCODE_SPACE: engineKey = K_SPACE; break;
		case SDL_SCANCODE_MINUS: engineKey = '-'; break;
		case SDL_SCANCODE_EQUALS: engineKey = '='; break;
		case SDL_SCANCODE_LEFTBRACKET: engineKey = '['; break;
		case SDL_SCANCODE_RIGHTBRACKET: engineKey = ']'; break;
		case SDL_SCANCODE_BACKSLASH: engineKey = '\\'; break;
		case SDL_SCANCODE_SEMICOLON: engineKey = ';'; break;
		case SDL_SCANCODE_APOSTROPHE: engineKey = '\''; break;
		case SDL_SCANCODE_GRAVE: engineKey = '`'; break;
		case SDL_SCANCODE_COMMA: engineKey = ','; break;
		case SDL_SCANCODE_PERIOD: engineKey = '.'; break;
		case SDL_SCANCODE_SLASH: engineKey = '/'; break;
		case SDL_SCANCODE_CAPSLOCK: engineKey = K_CAPSLOCK; break;
		case SDL_SCANCODE_PAUSE: engineKey = K_PAUSE; break;
		case SDL_SCANCODE_INSERT: engineKey = K_INS; break;
		case SDL_SCANCODE_HOME: engineKey = K_HOME; break;
		case SDL_SCANCODE_PAGEUP: engineKey = K_PGUP; break;
		case SDL_SCANCODE_DELETE: engineKey = K_DEL; break;
		case SDL_SCANCODE_END: engineKey = K_END; break;
		case SDL_SCANCODE_PAGEDOWN: engineKey = K_PGDN; break;
		case SDL_SCANCODE_RIGHT: engineKey = K_RIGHTARROW; break;
		case SDL_SCANCODE_LEFT: engineKey = K_LEFTARROW; break;
		case SDL_SCANCODE_DOWN: engineKey = K_DOWNARROW; break;
		case SDL_SCANCODE_UP: engineKey = K_UPARROW; break;
		case SDL_SCANCODE_KP_DIVIDE: engineKey = K_KP_SLASH; break;
		case SDL_SCANCODE_KP_MULTIPLY: engineKey = K_KP_MUL; break;
		case SDL_SCANCODE_KP_MINUS: engineKey = K_KP_MINUS; break;
		case SDL_SCANCODE_KP_PLUS: engineKey = K_KP_PLUS; break;
		case SDL_SCANCODE_KP_ENTER: engineKey = K_KP_ENTER; break;
		case SDL_SCANCODE_KP_1: engineKey = K_KP_END; break;
		case SDL_SCANCODE_KP_2: engineKey = K_KP_DOWNARROW; break;
		case SDL_SCANCODE_KP_3: engineKey = K_KP_PGDN; break;
		case SDL_SCANCODE_KP_4: engineKey = K_KP_LEFTARROW; break;
		case SDL_SCANCODE_KP_5: engineKey = K_KP_5; break;
		case SDL_SCANCODE_KP_6: engineKey = K_KP_RIGHTARROW; break;
		case SDL_SCANCODE_KP_7: engineKey = K_KP_HOME; break;
		case SDL_SCANCODE_KP_8: engineKey = K_KP_UPARROW; break;
		case SDL_SCANCODE_KP_9: engineKey = K_KP_PGUP; break;
		case SDL_SCANCODE_KP_0: engineKey = K_KP_INS; break;
		case SDL_SCANCODE_KP_PERIOD:
		case SDL_SCANCODE_KP_DECIMAL: engineKey = K_KP_DEL; break;
		case SDL_SCANCODE_LCTRL:
		case SDL_SCANCODE_RCTRL: engineKey = K_CTRL; break;
		case SDL_SCANCODE_LSHIFT:
		case SDL_SCANCODE_RSHIFT: engineKey = K_SHIFT; break;
		case SDL_SCANCODE_LALT:
		case SDL_SCANCODE_RALT: engineKey = K_ALT; break;
		case SDL_SCANCODE_LGUI:
		case SDL_SCANCODE_RGUI: engineKey = K_WIN; break;
		}
	}
	Finish( engineKey, 0, SDL_GetKeyName( key ) );
}

//-----------------------------------------------------------------------------
// Purpose: mouse input handler
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnMousePressed( vgui2::MouseCode code )
{
	if ( !m_pKeyBindList->IsCapturing() )
	{
		BaseClass::OnMousePressed( code );
		return;
	}
	switch ( code )
	{
	case vgui2::MOUSE_RIGHT: Finish( 0, 2, "MOUSE2" ); break;
	case vgui2::MOUSE_MIDDLE: Finish( 0, 4, "MOUSE3" ); break;
	case vgui2::MOUSE_4: Finish( 0, 8, MOUSE4_KEYNAME ); break;
	case vgui2::MOUSE_5: Finish( 0, 16, MOUSE5_KEYNAME ); break;
	default: Finish( 0, 1, "MOUSE1" ); break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets up mouse capture mode
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnMouseDoublePressed( vgui2::MouseCode code )
{
	if ( !m_pKeyBindList->IsCapturing() )
	{
		BaseClass::OnMouseDoublePressed( code );
		return;
	}
	switch ( code )
	{
	case vgui2::MOUSE_RIGHT: Finish( 0, 2, "MOUSE2" ); break;
	case vgui2::MOUSE_MIDDLE: Finish( 0, 4, "MOUSE3" ); break;
	case vgui2::MOUSE_4: Finish( 0, 8, MOUSE4_KEYNAME ); break;
	case vgui2::MOUSE_5: Finish( 0, 16, MOUSE5_KEYNAME ); break;
	default: Finish( 0, 1, "MOUSE1" ); break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: captures mouse input
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnMouseWheeled( int delta )
{
	if ( !m_pKeyBindList->IsCapturing() )
	{
		BaseClass::OnMouseWheeled( delta );
		return;
	}
	if ( delta > 0 )
		Finish( FindKeyForName( "MWHEELUP" ), 0, "MWHEELUP" );
	else
		Finish( FindKeyForName( "MWHEELDOWN" ), 0, "MWHEELDOWN" );
}

//-----------------------------------------------------------------------------
// Purpose: Check for enter key and go into keybinding mode if it was pressed
//-----------------------------------------------------------------------------
void COptionsSubKeyboard::OnKeyCodePressed( vgui2::KeyCode code )
{
	// Enter key pressed and not already trapping next key/button press
	if ( !m_pKeyBindList->IsCapturing() )
	{
		// Grab which item was set as interesting
		int itemID = m_pKeyBindList->GetItemOfInterest();
		int x, y, wide, tall;
		bool valid = m_pKeyBindList->GetCellBounds( itemID, 1, x, y, wide, tall );
		if ( code == vgui2::KEY_DELETE && valid )
		{
			// find the current binding and remove it
			KeyValues* item = m_pKeyBindList->GetItemData( itemID );
			RemoveKeyFromBindItems( item->GetString( "AltKey", NULL ) );
			RemoveKeyFromBindItems( item->GetString( "Key", NULL ) );
			m_pClearBindingButton->SetEnabled( false );
			m_pKeyBindList->InvalidateItem( itemID );
			// message handled, don't pass on
			return;
		}
	}
	BaseClass::OnKeyCodePressed( code );
}
