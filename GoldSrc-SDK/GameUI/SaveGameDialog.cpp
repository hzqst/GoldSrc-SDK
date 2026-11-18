//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "SaveGameDialog.h"
#include "GameUI_Interface.h"
#include "StringUtils.h"
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
#include <algorithm>
#include <vector>

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
#define SAVEGAME_ELAPSED_TIME_SIZE	5	// "MM:SS" at the end of the comment
#define SAVEGAME_HEADER_SIZE	( 5 * sizeof( int ) )	// tag, version, size, tokenCount, tokenSize

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

// reads exactly size bytes, a short read fails
static bool SaveReadExact( FileHandle_t file, void* output, int size )
{
	return vgui2::filesystem()->Read( output, size, file ) == size;
}

// returns NULL for an index outside the table or an empty hash slot
static const char* SaveTokenAt( const std::vector<const char*>& tokenList, int token )
{
	if ( token < 0 || token >= (int)tokenList.size() )
		return NULL;
	return tokenList[token];
}

int SaveReadNameAndComment( FileHandle_t file, std::string& name, std::string& comment )
{
	int tag;
	if ( !SaveReadExact( file, &tag, sizeof( tag ) ) || tag != SAVEGAME_HEADER )
		return 0;

	if ( !SaveReadExact( file, &tag, sizeof( tag ) ) || tag != SAVEGAME_VERSION )	// Enforce version for now
		return 0;

	name.clear();
	comment.clear();

	int size;
	int tokenCount;
	int tokenSize;
	if ( !SaveReadExact( file, &size, sizeof( size ) )
		|| !SaveReadExact( file, &tokenCount, sizeof( tokenCount ) )	// These two ints are the token list
		|| !SaveReadExact( file, &tokenSize, sizeof( tokenSize ) ) )
		return 0;

	// Sanity Check.
	if ( tokenCount < 0 || tokenCount > MAX_SAVE_TOKENS )
		return 0;
	if ( tokenSize < 0 || tokenSize > MAX_SAVE_TOKENS )
		return 0;
	// the data must be in the file, so a damaged size can not allocate more than it
	unsigned int fileSize = vgui2::filesystem()->Size( file );
	if ( size < 0 || fileSize < SAVEGAME_HEADER_SIZE
		|| (unsigned int)size + tokenSize > fileSize - SAVEGAME_HEADER_SIZE )
		return 0;
	size += tokenSize;

	std::vector<char> saveData( size );
	if ( size > 0 && !SaveReadExact( file, &saveData[0], size ) )
		return 0;

	const char* data = saveData.data();
	const char* end = data + size;

	// Parse the table of the strings, empty slots of the token hash table stay NULL
	std::vector<const char*> tokenList( tokenCount, (const char*)NULL );
	if ( tokenSize > 0 )
	{
		for ( int i = 0; i < tokenCount && data < end; i++ )
		{
			if ( *data )
			{
				const char* terminator = (const char*)memchr( data, 0, end - data );
				if ( !terminator )
					return 0;
				tokenList[i] = data;
				data = terminator;
			}
			data++;
		}
	}

	if ( end - data < 2 * (int)sizeof( short ) + 1 )
		return 0;
	int fieldSize = *(const short*)data;
	data += sizeof( short );
	const char* fieldName = SaveTokenAt( tokenList, *(const short*)data );
	if ( !fieldName || stricmp( fieldName, "GameHeader" ) )
		return 0;

	data += sizeof( short );
	int fieldCount = *data;
	if ( fieldSize < 0 || fieldSize > end - data )
		return 0;
	data += fieldSize;

	// Each field is a short (size), short (index of name), binary string of "size" bytes (data)
	for ( int i = 0; i < fieldCount; i++ )
	{
		if ( end - data < 2 * (int)sizeof( short ) )
			return 0;
		fieldSize = *(const short*)data;
		data += sizeof( short );
		fieldName = SaveTokenAt( tokenList, *(const short*)data );
		data += sizeof( short );
		if ( !fieldName || fieldSize < 0 || fieldSize > end - data )
			return 0;
		// The text ends at the first NUL in the field or at the end of the field
		const char* fieldEnd = std::find( data, data + fieldSize, '\0' );
		if ( !stricmp( fieldName, "comment" ) )
			comment.assign( data, fieldEnd );
		else if ( !stricmp( fieldName, "mapName" ) )
			name.assign( data, fieldEnd );
		data += fieldSize;
	}

	if ( !name.empty() && !comment.empty() )
		return 1;
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: moves the elapsed time at the end of a save comment into elapsedTime
//          and drops the spaces before it, "??" when the comment is too short
//-----------------------------------------------------------------------------
void SplitSaveComment( std::string& comment, std::string& elapsedTime )
{
	elapsedTime = "??";
	if ( comment.size() <= SAVEGAME_ELAPSED_TIME_SIZE )
		return;

	elapsedTime = comment.substr( comment.size() - SAVEGAME_ELAPSED_TIME_SIZE );
	comment.erase( comment.size() - SAVEGAME_ELAPSED_TIME_SIZE );
	// The first character is kept even when it is a space
	size_t last = comment.find_last_not_of( ' ' );
	comment.erase( last == std::string::npos ? 1 : last + 1 );
}

//-----------------------------------------------------------------------------
// Purpose: formats "<command> <shortName>\n" for the engine, fails when the
//          name could split the console command or does not fit in buffer
//-----------------------------------------------------------------------------
bool FormatSaveGameCommand( char* buffer, size_t bufferSize, const char* command, const char* shortName )
{
	if ( strpbrk( shortName, ";\"\n\r" ) )
		return false;

	return UTIL_FormatString( buffer, bufferSize, "%s %s\n", command, shortName );
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
	if ( !FormatSaveGameCommand( saveCommand, sizeof( saveCommand ), "save", shortName ) )
		return;
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

	std::string mapName;
	std::string comment;
	int valid = SaveReadNameAndComment( file, mapName, comment );
	vgui2::filesystem()->Close( file );
	if ( !valid )
		return false;

	data->SetString( "Map", mapName.c_str() );
	data->SetInt( "Quick", strstr( filename, "quick" ) != NULL );
	data->SetInt( "Autosave", strstr( filename, "autosave" ) != NULL );

	std::string elapsedTime;
	SplitSaveComment( comment, elapsedTime );

	// calculate the file name to print
	const char* type = NULL;
	if ( strstr( filename, "quick" ) )
		type = "#GameUI_QuickSave";
	else if ( strstr( filename, "autosave" ) )
		type = "#GameUI_AutoSave";
	data->SetString( "Type", type );
	data->SetString( "Game", comment.c_str() );
	data->SetString( "Elapsed Time", elapsedTime.c_str() );

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
