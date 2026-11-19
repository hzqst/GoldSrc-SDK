#include "hud.h"
#include "buildslave_assert.h"
#include "CreateCommandMenu.h"
#include "Menu.h"
#include "MenuItem.h"
#include "ComboBox.h"
#include "vgui/KeyValues.h"
#include <stdlib.h>
#include <string.h>

using namespace vgui2;

#define ASSERT_FILE BUILDSLAVE_PATH( L"czero\\cl_dll\\vgui\\hl_base\\CreateCommandMenu.cpp" )

char* sTFClasses[] =
{
	"", "SCOUT", "SNIPER", "SOLDIER", "DEMOMAN", "MEDIC", "HWGUY", "PYRO", "SPY", "ENGINEER", "CIVILIAN"
};

void CreateCommandComboMenu( char* menuFile, ComboBox* parent, int playerTeam )
{
	Menu* menu = CreateCommandMenu( menuFile, playerTeam, parent );
	parent->SetMenu( menu );
	menu->AddActionSignalTarget( parent );
}

Menu* CreateCommandMenu( char* menuFile, int playerTeam, Panel* parent )
{
	Menu* root = new Menu( parent, menuFile );
	root->SetVisible( false );
	char* file = (char*)gEngfuncs.COM_LoadFile( menuFile, 5, NULL );
	if ( !file )
		gEngfuncs.Con_DPrintf( "Unable to open %s\n", menuFile );
	char token[1024];
	char* data = gEngfuncs.COM_ParseFile( file, token );
	Menu* currentMenu = root;
	while ( token[0] )
	{
		if ( token[0] == '}' )
		{
			if ( currentMenu != root )
			{
				Panel* menuParent = currentMenu->GetParent();
				Menu* parentMenu = menuParent ? dynamic_cast<Menu*>( menuParent ) : NULL;
				currentMenu = parentMenu ? parentMenu : root;
			}
			else
				ASSERT_AT( currentMenu != NULL, 235 );
			data = gEngfuncs.COM_ParseFile( data, token );
			continue;
		}
		char text[32] = {};
		char boundKey[32] = {};
		char custom[32] = {};
		char command[128] = {};
		char map[64] = {};
		bool toggle = false;
		bool toggleState = false;
		int teamOnly = -1;
		strncpy( custom, token, sizeof( custom ) );
		custom[sizeof( custom ) - 1] = 0;
		if ( !strcmp( custom, "CUSTOM" ) )
			data = gEngfuncs.COM_ParseFile( data, token );
		else if ( !strcmp( custom, "MAP" ) )
		{
			data = gEngfuncs.COM_ParseFile( data, token );
			strncpy( map, token, sizeof( map ) );
			map[sizeof( map ) - 1] = 0;
			data = gEngfuncs.COM_ParseFile( data, token );
		}
		else if ( !strncmp( custom, "TEAM", 4 ) )
		{
			teamOnly = atoi( custom + 4 );
			data = gEngfuncs.COM_ParseFile( data, token );
		}
		else if ( !strncmp( custom, "TOGGLE", 6 ) )
		{
			data = gEngfuncs.COM_ParseFile( data, token );
			toggle = true;
			toggleState = _strnicmp( token, "true", 4 ) == 0;
			data = gEngfuncs.COM_ParseFile( data, token );
		}
		else if ( !strcmp( token, sTFClasses[1] ) || !strcmp( token, sTFClasses[2] ) ||
			!strcmp( token, sTFClasses[3] ) || !strcmp( token, sTFClasses[4] ) ||
			!strcmp( token, sTFClasses[5] ) || !strcmp( token, sTFClasses[6] ) ||
			!strcmp( token, sTFClasses[7] ) || !strcmp( token, sTFClasses[8] ) || !strcmp( token, sTFClasses[9] ) )
			data = gEngfuncs.COM_ParseFile( data, token );
		strncpy( boundKey, token, sizeof( boundKey ) );
		text[sizeof( text ) - 1] = 0;
		data = gEngfuncs.COM_ParseFile( data, token );
		strncpy( text, token, sizeof( text ) );
		text[sizeof( text ) - 1] = 0;
		data = gEngfuncs.COM_ParseFile( data, token );
		strncpy( command, token, sizeof( command ) );
		command[sizeof( command ) - 1] = 0;
		if ( command[0] == '{' )
		{
			Menu* submenu = new Menu( parent, "SubMenu" );
			currentMenu->AddCascadingMenuItem( text, parent, submenu, NULL );
			currentMenu = submenu;
		}
		else if ( map[0] )
		{
			KeyValues* userData = new KeyValues( "Command", "command", command, "map", map );
			KeyValues* action = new KeyValues( "SetText", "text", text );
			currentMenu->AddMenuItem( text, action, parent, userData );
			userData->deleteThis();
		}
		else if ( teamOnly == -1 )
		{
			KeyValues* userData = new KeyValues( "Command", "command", command );
			KeyValues* action = new KeyValues( "SetText", "text", text );
			if ( toggle )
			{
				currentMenu->AddCheckableMenuItem( text, action, parent, userData );
				currentMenu->GetMenuItem( currentMenu->GetMenuID( currentMenu->GetItemCount() - 1 ) )->SetChecked( toggleState );
			}
			else
				currentMenu->AddMenuItem( text, action, parent, userData );
			userData->deleteThis();
		}
		else if ( teamOnly == playerTeam )
		{
			KeyValues* userData = new KeyValues( "Command", "command", command, "map", map );
			KeyValues* action = new KeyValues( "SetText", "text", text );
			currentMenu->AddMenuItem( text, action, parent, userData );
			userData->deleteThis();
		}
		data = gEngfuncs.COM_ParseFile( data, token );
	}
	gEngfuncs.COM_FreeFile( file );
	return root;
}
