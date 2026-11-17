#include "CreateCommandMenu.h"
#include "iviewport.h"
#include "ComboBox.h"
#include "Menu.h"
#include "MenuItem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\game_controls\\CreateCommandMenu.cpp" )

using namespace vgui2;

static char* ParseMenuToken( char* data, char* token )
{
	token[0] = 0;
	return data ? gViewPortInterface->GetClientDllInterface()->COM_ParseFile( data, token ) : NULL;
}

void CCreateCommandComboMenu::CreateCommandComboMenu( char* menuFile, ComboBox* parent, int playerTeam )
{
	m_iIgnoreDepth = 0;
	m_iIgnoring = 0;
	m_iDepth = 0;
	Menu* menu = CreateCommandMenu( menuFile, playerTeam, parent );
	if ( menu )
	{
		parent->SetMenu( menu );
		menu->AddActionSignalTarget( parent );
	}
}

Menu* CCreateCommandComboMenu::CreateCustomButton( Panel* parent, Menu* menu, char*, char* text,
	char* command, bool )
{
	if ( !strcmp( command, "!CHANGETEAM" ) )
	{
		Menu* submenu = new Menu( parent, "SubMenu" );
		menu->AddCascadingMenuItem( text, parent, submenu, NULL );
		int teams = gViewPortInterface->GetNumberOfTeams();
		for ( int team = 0; team < teams; team++ )
		{
			if ( gViewPortInterface->GetClientDllInterface()->TeamNumber() != team + 1 )
			{
				char teamCommand[256];
				sprintf( teamCommand, "jointeam %d", team + 1 );
				submenu->AddMenuItem( gViewPortInterface->GetTeamName( team ), teamCommand, parent, NULL );
			}
		}
		submenu->AddMenuItem( "#Team_AutoAssign", "jointeam 5", parent, NULL );
		submenu->AddMenuItem( "#Menu_Spectate", "spectate", parent, NULL );
	}
	else if ( !strcmp( command, "!CHANGECLASS" ) )
	{
		if ( gViewPortInterface->GetValidClasses( gViewPortInterface->GetClientDllInterface()->TeamNumber() ) != -1 )
		{
			Menu* submenu = new Menu( parent, "SubMenu" );
			menu->AddCascadingMenuItem( text, parent, submenu, NULL );
		}
	}
	else if ( !strcmp( command, "!MAPBRIEFING" ) || !strcmp( command, "!CLASSDESC" ) ||
		!strcmp( command, "!SERVERINFO" ) )
	{
		menu->AddMenuItem( text, command + 1, parent, NULL );
	}
	return menu;
}

Menu* CCreateCommandComboMenu::CreateCommandMenu( char* menuFile, int playerTeam, Panel* parent )
{
	m_pMenuStack.Clear();
	m_iIgnoreDepth = 0;
	m_iIgnoring = 0;
	m_iDepth = 0;
	char* file = (char*)gViewPortInterface->GetClientDllInterface()->COM_LoadFile( menuFile, 5, NULL );
	if ( !file )
		return NULL;
	Menu* root = new Menu( parent, menuFile );
	root->SetVisible( false );
	Menu* currentMenu = root;

	char lastButtonText[32] = "file start";
	char token[1024];
	char* data = ParseMenuToken( file, token );
	while ( strlen( token ) > 0 )
	{
		while ( token[0] != '}' && strlen( token ) > 0 )
		{
			char text[32] = {};
			char boundKey[32] = {};
			char custom[32] = {};
			char command[128] = {};
			char map[256] = {};
			bool isCustom = false;
			bool toggle = false;
			bool toggleState = false;
			int teamOnly = -1;
			strncpy( custom, token, sizeof( custom ) );
			custom[sizeof( custom ) - 1] = 0;
			if ( !strcmp( custom, "CUSTOM" ) )
			{
				isCustom = true;
				data = ParseMenuToken( data, token );
			}
			else if ( !strcmp( custom, "MAP" ) )
			{
				data = ParseMenuToken( data, token );
				strncpy( map, token, sizeof( map ) );
				map[sizeof( map ) - 1] = 0;
				data = ParseMenuToken( data, token );
			}
			else if ( !strncmp( custom, "TEAM", 4 ) )
			{
				teamOnly = atoi( custom + 4 );
				data = ParseMenuToken( data, token );
			}
			else if ( !strncmp( custom, "TOGGLE", 6 ) )
			{
				toggle = true;
				data = ParseMenuToken( data, token );
				toggleState = _strnicmp( token, "true", 4 ) == 0;
				data = ParseMenuToken( data, token );
			}
			else if ( IsCustomToken( token ) )
			{
				data = ParseMenuToken( data, token );
				break;
			}

			strncpy( boundKey, token, sizeof( boundKey ) );
			boundKey[sizeof( boundKey ) - 1] = 0;
			data = ParseMenuToken( data, token );
			strncpy( text, token, sizeof( text ) );
			text[sizeof( text ) - 1] = 0;
			strcpy( lastButtonText, text );
			data = ParseMenuToken( data, token );
			strncpy( command, token, sizeof( command ) );
			command[sizeof( command ) - 1] = 0;

			if ( isCustom )
			{
				data = ParseMenuToken( data, token );
				Menu* customMenu = CreateCustomButton( parent, currentMenu, boundKey, text, command, token[0] == '{' );
				if ( customMenu )
				{
					currentMenu = customMenu;
				}
				else
				{
					m_iIgnoreDepth++;
					while ( m_iIgnoreDepth && data )
					{
						data = ParseMenuToken( data, token );
						if ( token[0] == '{' )
							m_iIgnoreDepth++;
						else if ( token[0] == '}' )
							m_iIgnoreDepth--;
					}
				}
				if ( token[0] == '{' )
					data = ParseMenuToken( data, token );
				continue;
			}

			if ( command[0] == '{' )
			{
				bool createSubmenu = true;
				if ( map[0] )
				{
					const char* level = gViewPortInterface->GetClientDllInterface()->GetLevelName();
					if ( level )
					{
						char mapName[sizeof( map ) + sizeof( "maps/.bsp" )];
						_snprintf( mapName, sizeof( mapName ), "maps/%s.bsp", map );
						mapName[sizeof( mapName ) - 1] = 0;
						if ( strcmp( mapName, level ) )
						{
							createSubmenu = false;
							m_iIgnoreDepth++;
							while ( m_iIgnoreDepth && data )
							{
								data = ParseMenuToken( data, token );
								if ( token[0] == '{' )
									m_iIgnoreDepth++;
								else if ( token[0] == '}' )
									m_iIgnoreDepth--;
							}
						}
					}
				}
				if ( createSubmenu )
				{
					Menu* submenu = new Menu( parent, "SubMenu" );
					currentMenu->AddCascadingMenuItem( text, parent, submenu, NULL );
					m_pMenuStack.Push( currentMenu );
					currentMenu = submenu;
					m_iDepth++;
				}
			}
			else if ( teamOnly == -1 )
			{
				if ( toggle )
				{
					currentMenu->AddCheckableMenuItem( text, command, parent, NULL );
					currentMenu->GetMenuItem( currentMenu->GetMenuID( currentMenu->GetItemCount() - 1 ) )->SetChecked( toggleState );
				}
				else
				{
					currentMenu->AddMenuItem( text, command, parent, NULL );
				}
			}
			else if ( teamOnly == playerTeam )
			{
				currentMenu->AddMenuItem( text, command, parent, NULL );
			}

			data = ParseMenuToken( data, token );
		}

		if ( currentMenu != root )
		{
			if ( m_iDepth > 0 )
			{
				m_pMenuStack.Pop( currentMenu );
				m_iDepth--;
				ASSERT_AT( currentMenu != NULL, 269 );
			}
			else
			{
				currentMenu = root;
			}
		}

		data = ParseMenuToken( data, token );
	}

	gViewPortInterface->GetClientDllInterface()->COM_FreeFile( file );
	m_pMenuStack.Clear();
	return root;
}

bool CCreateCommandComboMenu::IsCustomToken( const char* )
{
	return false;
}
