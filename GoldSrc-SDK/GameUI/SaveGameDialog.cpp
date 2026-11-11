//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "SaveGameDialog.h"
#include "GameUI_Interface.h"
#include "Taskbar.h"

#include "Button.h"
#include "Label.h"
#include "ListPanel.h"
#include "IBaseUI.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"GameUI\\SaveGameDialog.cpp" )

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#define SAVEGAME_HEADER	( 'J' | ( 'S' << 8 ) | ( 'A' << 16 ) | ( 'V' << 24 ) )
#define SAVEGAME_VERSION	113
#define MAX_SAVE_TOKENS	0x02000000
#define SAVEGAME_MAP_NAME_SIZE	32
#define SAVEGAME_COMMENT_SIZE	80

#ifdef _WIN32
#define SAVEGAME_PATH_MAX	_MAX_PATH
#else
#define SAVEGAME_PATH_MAX	PATH_MAX
#endif

int TimeStampSortFunc( vgui2::ListPanel*, const vgui2::ListPanelItem& first,
		const vgui2::ListPanelItem& second )
{
	int t1 = first.kv->GetInt( "timestamp", 0 );
	int t2 = second.kv->GetInt( "timestamp", 0 );

	ASSERT_AT( t1 > 0, 259 );
	ASSERT_AT( t2 > 0, 260 );

	return t1 < t2 ? 1 : -1;
}

int SaveReadNameAndComment( FileHandle_t file, char* name, char* comment )
{
	int tag;
	vgui2::filesystem()->Read( &tag, sizeof( tag ), file );
	if ( tag != SAVEGAME_HEADER )
		return 0;

	vgui2::filesystem()->Read( &tag, sizeof( tag ), file );
	if ( tag != SAVEGAME_VERSION )				// Enforce version for now
		return 0;

	name[0] = 0;
	comment[0] = 0;

	int size;
	int tokenCount;
	int tokenSize;
	vgui2::filesystem()->Read( &size, sizeof( size ), file );
	vgui2::filesystem()->Read( &tokenCount, sizeof( tokenCount ), file ); // These two ints are the token list
	vgui2::filesystem()->Read( &tokenSize, sizeof( tokenSize ), file );
	size += tokenSize;

	// Sanity Check.
	if ( tokenCount < 0 || tokenCount > MAX_SAVE_TOKENS )
		return 0;
	if ( tokenSize < 0 || tokenSize > MAX_SAVE_TOKENS )
		return 0;

	char* saveData = new char[size];
	vgui2::filesystem()->Read( saveData, size, file );

	char* end = saveData + size;

	char* data = saveData;
	char** tokenList;
	// Allocate a table for the strings, and parse the table
	if ( tokenSize > 0 )
	{
		tokenList = new char*[tokenCount];
		// Make sure the token strings pointed to by the pToken hashtable.
		for ( int i = 0; i < tokenCount; i++ )
		{
			tokenList[i] = ( data < end && *data ) ? data : NULL;
			while ( data < end && *data )
				data++;
			if ( data < end )
				data++;
		}
	}
	else
		tokenList = NULL;

	if ( end - data < 2 * (int)sizeof( short ) + 1 )
	{
		delete[] saveData;
		return 0;
	}
	int fieldSize = *(short*)data;
	data += sizeof( short );
	int gameHeaderToken = *(short*)data;
	if ( tokenList && ( gameHeaderToken < 0 || gameHeaderToken >= tokenCount ) )
	{
		delete[] saveData;
		return 0;
	}
#ifdef _MSC_VER
#pragma warning( suppress: 6011 )
#endif
	const char* fieldName = tokenList[gameHeaderToken];
#ifdef _MSC_VER
#pragma warning( suppress: 6387 )
#endif
	if ( stricmp( fieldName, "GameHeader" ) )
	{
		delete[] saveData;
		return 0;
	}

	data += sizeof( short );
	int fieldCount = *data;
	data += fieldSize;

	// Each field is a short (size), short (index of name), binary string of "size" bytes (data)
	for ( int i = 0; i < fieldCount; i++ )
	{
		if ( end - data < 2 * (int)sizeof( short ) )
		{
			delete[] tokenList;
			delete[] saveData;
			return 0;
		}
		fieldSize = *(short*)data;
		data += sizeof( short );
		int fieldToken = *(short*)data;
		data += sizeof( short );
		if ( tokenList && ( fieldToken < 0 || fieldToken >= tokenCount ) )
		{
			delete[] tokenList;
			delete[] saveData;
			return 0;
		}
		fieldName = tokenList[fieldToken];
		if ( fieldSize < 0 || fieldSize > end - data )
		{
			delete[] tokenList;
			delete[] saveData;
			return 0;
		}
		if ( !stricmp( fieldName, "comment" ) )
			strncpy( comment, data, fieldSize > SAVEGAME_COMMENT_SIZE ? SAVEGAME_COMMENT_SIZE : fieldSize );
		else if ( !stricmp( fieldName, "mapName" ) )
			strncpy( name, data, fieldSize > SAVEGAME_MAP_NAME_SIZE ? SAVEGAME_MAP_NAME_SIZE : fieldSize );
		data += fieldSize;
	}

	delete[] tokenList;
	delete[] saveData;

	name[SAVEGAME_MAP_NAME_SIZE - 1] = 0;
	comment[SAVEGAME_COMMENT_SIZE - 1] = 0;

	if ( strlen( name ) > 0 && strlen( comment ) > 0 )
		return 1;
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CSaveGameDialog::CSaveGameDialog( vgui2::Panel* parent ) : CTaskFrame( parent, "SaveGameDialog" )
{
	SetHDProportional( true );
	SetMinimumSize( GetProportionalScaledValue( 300 ), GetProportionalScaledValue( 300 ) );
	SetBounds( 0, 0, GetProportionalScaledValue( 512 ), GetProportionalScaledValue( 384 ) );
	SetSizeable( true );
	MakePopup( true, false );
	g_pTaskbar->AddTask( GetVPanel() );
	SetTitle( "#GameUI_SaveGame", true );

	new vgui2::Label( this, "HelpText", "#GameUI_SaveGameHelp" );
	vgui2::Button* save = new vgui2::Button( this, "Save", "#GameUI_Save" );
	save->SetCommand( "Save" );
	vgui2::Button* cancel = new vgui2::Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );
	CreateSavedGamesList();
	ScanSavedGames();
	LoadControlSettings( "Resource/SaveGameDialog.res", NULL );
}

CSaveGameDialog::~CSaveGameDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSaveGameDialog::OnClose()
{
	vgui2::Frame::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CSaveGameDialog::OnCommand( const char* command )
{
	if ( stricmp( command, "Save" ) )
	{
		vgui2::Frame::OnCommand( command );
		return;
	}

	KeyValues* data = m_pGameList->GetItem( m_pGameList->GetSelectedItem( 0 ) );
	if ( !data )
		return;

	const char* shortName = data->GetString( "ShortName", "" );
	if ( !shortName || !*shortName )
		return;

	// Load the game, return to top and switch to engine
	char saveCommand[256];
	sprintf( saveCommand, "save %s\n", shortName );
	engine->pfnClientCmd( saveCommand );
	if ( baseuifuncs )
		baseuifuncs->HideGameUI();
	OnClose();
}

void CSaveGameDialog::CreateSavedGamesList()
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

bool CSaveGameDialog::ParseSaveData( const char* filename, const char* shortName, KeyValues* data )
{
	if ( !filename || !shortName || !data )
		return false;

	data->SetString( "ShortName", shortName );
	data->SetString( "FileName", filename );
	FileHandle_t file = vgui2::filesystem()->Open( filename, "rb", NULL );
	if ( !file )
		return false;

	char mapName[SAVEGAME_MAP_NAME_SIZE];
	char comment[SAVEGAME_COMMENT_SIZE];
	int valid = SaveReadNameAndComment( file, mapName, comment );
	vgui2::filesystem()->Close( file );
	if ( !valid )
		return false;

	data->SetString( "Map", mapName );
	data->SetInt( "Quick", strstr( filename, "quick" ) != NULL );
	data->SetInt( "Autosave", strstr( filename, "autosave" ) != NULL );

	char elapsedTime[32];
	sprintf( elapsedTime, "??" );
	int length = (int)strlen( comment );
	if ( length > 5 )
	{
		strncpy( elapsedTime, comment + length - 5, 5 );
		elapsedTime[5] = 0;
		comment[length - 5] = 0;
		for ( int i = length - 6; i >= 1 && comment[i] == ' '; i-- )
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

	char fileTime[32];
	long timestamp = vgui2::filesystem()->GetFileTime( filename );
	vgui2::filesystem()->FileTimeToString( fileTime, sizeof( fileTime ), timestamp );
	char* newline = strstr( fileTime, "\n" );
	if ( newline )
		*newline = 0;
	data->SetString( "Time", fileTime );
	data->SetInt( "timestamp", timestamp );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: generates a new save game name
//-----------------------------------------------------------------------------
const char* CSaveGameDialog::FindSaveSlot()
{
	static char szFileName[512];
	for ( int i = 0; i < 1000; i++ )
	{
		sprintf( szFileName, "save/Half-Life-%03i.sav", i );
		FileHandle_t file = vgui2::filesystem()->Open( szFileName, "rb", "GAMECONFIG" );
		if ( !file )
		{
			return szFileName + 5;
		}
		vgui2::filesystem()->Close( file );
	}

	ASSERT_AT( !("Could not generate new save game file"), 428 );
	return "error.sav";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSaveGameDialog::ScanSavedGames()
{
	char filename[SAVEGAME_PATH_MAX];
	char directory[SAVEGAME_PATH_MAX];
	sprintf( directory, "save/*.sav" );
	// FIXME:  Reset list?

	// Iterate the saved files
	FileFindHandle_t handle;
	for ( const char* shortName = vgui2::filesystem()->FindFirst( directory, &handle, "GAMECONFIG" ); shortName;
			shortName = vgui2::filesystem()->FindNext( handle ) )
	{
		if ( !strnicmp( shortName, "HLSave", 6 ) )
			continue;
		sprintf( filename, "save/%s", shortName );
		// Parse in data.
		KeyValues* data = new KeyValues( "SavedGame" );
		if ( ParseSaveData( filename, shortName, data ) )
			m_pGameList->AddItem( data, 0, false, false );
		data->deleteThis();
	}
	vgui2::filesystem()->FindClose( handle );

	// create dummy item for current saved game
	KeyValues* newSave = new KeyValues( "SavedGame" );
	newSave->SetString( "Game", "#GameUI_SaveGame_NewSavedGame" );
	newSave->SetString( "Elapsed Time", "#GameUI_SaveGame_New" );
	newSave->SetString( "Time", "#GameUI_SaveGame_Current" );
	newSave->SetInt( "timestamp", INT_MAX );
	newSave->SetString( "ShortName", FindSaveSlot() );
	m_pGameList->AddItem( newSave, 0, false, false );
	// sort the list
	m_pGameList->SortList();
	// set the first item to be selected
	m_pGameList->SetSingleSelectedItem( m_pGameList->GetItemIDFromRow( 0 ) );
}
