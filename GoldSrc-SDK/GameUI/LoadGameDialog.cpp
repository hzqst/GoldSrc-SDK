//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "LoadGameDialog.h"
#include "SaveGameDialog.h"
#include "GameUI_Interface.h"
#include "Taskbar.h"

#include "Button.h"
#include "FileSystem.h"
#include "Label.h"
#include "ListPanel.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define SAVEGAME_PATH_MAX	_MAX_PATH
#else
#include <strings.h>
#define stricmp			strcasecmp
#define strnicmp		strncasecmp
#define SAVEGAME_PATH_MAX	PATH_MAX
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CLoadGameDialog::CLoadGameDialog( vgui2::Panel* parent )
	: CTaskFrame( parent, "LoadGameDialog", true )
{
	SetHDProportional( true );
	SetBounds( 0, 0, GetProportionalScaledValue( 512 ), GetProportionalScaledValue( 384 ) );
	SetMinimumSize( GetProportionalScaledValue( 256 ), GetProportionalScaledValue( 300 ) );
	SetSizeable( true );
	MakePopup( true, false );
	SetCloseSound( "sound/UI/launch_dnmenu1.wav" );
	g_pTaskbar->AddTask( GetVPanel() );
	SetTitle( "#GameUI_LoadGame", true );

	new vgui2::Label( this, "HelpText", "#GameUI_LoadGameHelp" );
	vgui2::Button* load = new vgui2::Button( this, "Load", "#GameUI_Load" );
	load->SetCommand( "Load" );
	vgui2::Button* cancel = new vgui2::Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	CreateSavedGamesList();
	ScanSavedGames();
	LoadControlSettings( "Resource\\LoadGameDialog.res", NULL );
}

CLoadGameDialog::~CLoadGameDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadGameDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CLoadGameDialog::OnCommand( const char* command )
{
	if ( stricmp( command, "Load" ) )
	{
		BaseClass::OnCommand( command );
		return;
	}

	KeyValues* data = m_pGameList->GetItem( m_pGameList->GetSelectedItem( 0 ) );
	if ( !data )
		return;

	const char* shortName = data->GetString( "ShortName", "" );
	if ( !shortName || !shortName[0] )
		return;

	// Load the game, return to top and switch to engine
	char commandBuffer[256];
	sprintf( commandBuffer, "load %s\n", shortName );
	engine->pfnClientCmd( commandBuffer );
	OnClose();
}

void CLoadGameDialog::CreateSavedGamesList()
{
	// List
	m_pGameList = new vgui2::ListPanel( this, "listpanel_loadgame" );
	m_pGameList->AddColumnHeader( 0, "Game", "#GameUI_SavedGame", GetProportionalScaledValue( 175 ), 0 );
	m_pGameList->AddColumnHeader( 1, "Type", "#GameUI_Type", GetProportionalScaledValue( 100 ), 0 );
	m_pGameList->AddColumnHeader( 2, "Elapsed Time", "#GameUI_ElapsedTime", GetProportionalScaledValue( 75 ), 0 );
	m_pGameList->AddColumnHeader( 3, "Time", "#GameUI_TimeStamp", GetProportionalScaledValue( 175 ), 0 );
	m_pGameList->SetSortFunc( 3, TimeStampSortFunc );
	m_pGameList->SetSortColumn( 3 );
}

bool CLoadGameDialog::ParseSaveData( const char* filename, const char* shortName, KeyValues* data )
{
	if ( !filename || !shortName || !data )
		return false;

	data->SetString( "ShortName", shortName );
	data->SetString( "FileName", filename );

	FileHandle_t file = vgui2::filesystem()->Open( filename, "rb", "GAMECONFIG" );
	if ( !file )
		return false;

	char mapName[32];
	char comment[80];
	int valid = SaveReadNameAndComment( file, mapName, comment );
	vgui2::filesystem()->Close( file );
	if ( !valid )
		return false;

	data->SetString( "Map", mapName );
	int commentLength = (int)strlen( comment );
	char elapsedTime[32];
	sprintf( elapsedTime, "??" );
	if ( commentLength > 5 )
	{
		strncpy( elapsedTime, comment + commentLength - 5, 5 );
		elapsedTime[5] = 0;
		comment[commentLength - 5] = 0;
		for ( int i = commentLength - 6; i >= 1 && comment[i] == ' '; i-- )
			comment[i] = 0;
	}

	// calculate the file name to print
	const char* type = NULL;
	if ( strstr( filename, "quick" ) )
		type = "#GameUI_QuickSave";
	else if ( strstr( filename, "autosave" ) )
		type = "#GameUI_AutoSave";

	data->SetString( "Type", type );
	data->SetString( "Game", comment );
	data->SetString( "Elapsed Time", elapsedTime );

	long fileTime = vgui2::filesystem()->GetFileModificationTime( filename );
	char timeText[32];
	vgui2::filesystem()->FileTimeToString( timeText, sizeof( timeText ), fileTime );
	char* newline = strstr( timeText, "\n" );
	if ( newline )
		*newline = 0;
	data->SetString( "Time", timeText );
	data->SetInt( "timestamp", fileTime );
	return true;
}

void CLoadGameDialog::ScanSavedGames()
{
	char directory[SAVEGAME_PATH_MAX];
	sprintf( directory, "save/*.sav" );
	// FIXME:  Reset list?

	// Iterate the saved files
	FileFindHandle_t handle;
	for ( const char* name = vgui2::filesystem()->FindFirst( directory, &handle, "GAMECONFIG" );
		name; name = vgui2::filesystem()->FindNext( handle ) )
	{
		if ( !strnicmp( name, "HLSave", 6 ) )
			continue;

		char filename[SAVEGAME_PATH_MAX];
		sprintf( filename, "save/%s", name );
		// Parse in data.
		KeyValues* data = new KeyValues( "SavedGame" );
		if ( ParseSaveData( filename, name, data ) )
			m_pGameList->AddItem( data, 0, false, false );
		data->deleteThis();
	}
	vgui2::filesystem()->FindClose( handle );
	// sort the list
	m_pGameList->SortList();
	// set the first item to be selected
	m_pGameList->SetSingleSelectedItem( m_pGameList->GetItemIDFromRow( 0 ) );
}
