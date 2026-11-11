//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "CreateMultiplayerGameGameplayPage.h"
#include "PanelListPanel.h"
#include "ScriptObject.h"

#include "CheckButton.h"
#include "ComboBox.h"
#include "Label.h"
#include "TextEntry.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "../external/steamworks/isteamfriends.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

//-----------------------------------------------------------------------------
// Purpose: class for loading/saving server config file
//-----------------------------------------------------------------------------
class CServerDescription : public CDescription
{
public:
	CServerDescription( CPanelListPanel* panel );
	void WriteFileHeader( FileHandle_t file ) override;
	void WriteScriptHeader( FileHandle_t file ) override;
};

static void SanitizeToAscii( char* destination, int destinationSize, const char* text )
{
	const unsigned char* source = (const unsigned char*)text;
	const unsigned char* sourceEnd = source + strlen( text );
	char* end = destination + destinationSize - 1;

	while ( source != sourceEnd && destination != end )
	{
		unsigned char first = *source;
		unsigned int glyph;
		if ( first < 0x80 )
		{
			glyph = first;
			++source;
		}
		else if ( first < 0xC0 )
		{
			do
			{
				++source;
			}
			while ( source < sourceEnd && ( *source & 0xC0 ) == 0x80 );
			continue;
		}
		else
		{
			int length = sizeof( unsigned int ) * CHAR_BIT;
			if ( first != 0xFF )
			{
				length = 0;
				for ( unsigned int mask = 0x80; first & mask; mask >>= 1 )
					++length;
			}
			if ( sourceEnd - source < length )
				break;

			if ( first < 0xE0 )
				glyph = ( ( first & 0x1F ) << 6 ) | ( source[1] & 0x3F );
			else if ( first < 0xF0 )
				glyph = ( ( first & 0x0F ) << 12 ) | ( ( source[1] & 0x3F ) << 6 ) | ( source[2] & 0x3F );
			else if ( first < 0xF8 )
			{
				glyph = ( ( first & 7 ) << 18 ) | ( ( source[1] & 0x3F ) << 12 ) |
					( ( source[2] & 0x3F ) << 6 ) | ( source[3] & 0x3F );
			}
			else
				glyph = 0xFFFD;
			source += length;
		}

		if ( glyph < 0x80 )
			*destination++ = (char)first;
	}
	*destination = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameGameplayPage::CCreateMultiplayerGameGameplayPage( vgui2::Panel* parent, const char* name )
	: PropertyPage( parent, name, true )
{
	m_pOptionsList = new CPanelListPanel( this, "GameOptions", false );
	m_pDescription = new CServerDescription( m_pOptionsList );
	m_pDescription->InitFromFile( (char*)"settings.scr" );
	m_pList = NULL;
	LoadControlSettings( "Resource/CreateMultiplayerGameGameplayPage.res", NULL );
	LoadGameOptionsList();
}

CCreateMultiplayerGameGameplayPage::~CCreateMultiplayerGameGameplayPage()
{
}

const char* CCreateMultiplayerGameGameplayPage::GetValue( const char* cvarName, const char* defaultValue )
{
	for ( mpcontrol_t* item = m_pList; item; item = item->next )
	{
		if ( !item->pControl || stricmp( item->GetName(), cvarName ) )
			continue;

		static char buffer[128];
		KeyValues* data = new KeyValues( "GetText" );
		const char* value = defaultValue;
		if ( item->pControl->RequestInfo( data ) )
			value = data->GetString( "text", defaultValue );
		strncpy( buffer, value, sizeof( buffer ) - 1 );
		buffer[sizeof( buffer ) - 1] = 0;
		data->deleteThis();
		return buffer;
	}
	return defaultValue;
}

int CCreateMultiplayerGameGameplayPage::GetMaxPlayers()
{
	return atoi( GetValue( "maxplayers", "32" ) );
}

const char* CCreateMultiplayerGameGameplayPage::GetPassword()
{
	return GetValue( "sv_password", "" );
}

const char* CCreateMultiplayerGameGameplayPage::GetHostName()
{
	return GetValue( "hostname", "Half-Life" );
}

void CCreateMultiplayerGameGameplayPage::GatherCurrentValues()
{
	if ( !m_pDescription )
		return;

	char value[256];
	char text[256];
	wchar_t selected[256];
	wchar_t label[256];

	for ( mpcontrol_t* item = m_pList; item; item = item->next )
	{
		CScriptObject* object = item->pScrObj;
		if ( !item->pControl )
		{
			object->SetCurValue( object->defValue );
			continue;
		}

		switch ( object->type )
		{
		case O_BOOL:
			sprintf( value, "%s", ( (vgui2::CheckButton*)item->pControl )->IsSelected() ? "1" : "0" );
			break;

		case O_NUMBER:
		case O_STRING:
			( (vgui2::TextEntry*)item->pControl )->GetText( text, sizeof( text ) );
			sprintf( value, "%s", text );
			break;

		case O_LIST:
		{
			( (vgui2::ComboBox*)item->pControl )->GetText( selected, 256 );
			CScriptListItem* choice = object->pListItems;
			for ( ; choice; choice = choice->pNext )
			{
				wchar_t* localized = NULL;
				if ( choice->szItemText[0] == '#' )
					localized = vgui2::localize()->Find( choice->szItemText );
				if ( localized )
				{
					wcsncpy( label, localized, sizeof( label ) / sizeof( label[0] ) - 1 );
					label[sizeof( label ) / sizeof( label[0] ) - 1] = 0;
				}
				else
					vgui2::localize()->ConvertANSIToUnicode( choice->szItemText, label, sizeof( label ) );
#ifdef _WIN32
				if ( !_wcsicmp( label, selected ) )
#else
				if ( !wcscmp( label, selected ) )
#endif
					break;
			}
			sprintf( value, "%s", choice ? choice->szValue : object->defValue );
			break;
		}

		default:
			break;
		}

		// Remove double quotes and % characters
		UTIL_StripInvalidCharacters( value );
		strcpy( text, value );
		object->SetCurValue( text );
	}
}

//-----------------------------------------------------------------------------
// Purpose: called to get data from the page
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameGameplayPage::OnApplyChanges()
{
	// Get the values from the controls
	GatherCurrentValues();
	if ( !m_pDescription )
		return;

	// Add settings to config.cfg
	m_pDescription->WriteToConfig();
	FileHandle_t file = vgui2::filesystem()->Open( "settings.scr", "wb", NULL );
	if ( file )
	{
		m_pDescription->WriteToScriptFile( file );
		vgui2::filesystem()->Close( file );
	}
}

void CCreateMultiplayerGameGameplayPage::LoadGameOptionsList()
{
	for ( mpcontrol_t* item = m_pList; item; )
	{
		mpcontrol_t* next = item->next;
		//
		delete item->pControl;
		delete item->pPrompt;
		delete item;
		item = next;
	}
	m_pList = NULL;

	CScriptObject* object = m_pDescription->pObjList;
	vgui2::Panel* parent = m_pOptionsList;
	char personaName[128] = { 0 };
	char hostName[256];
	if ( SteamFriends() && SteamFriends()->GetPersonaName() )
	{
		SanitizeToAscii( personaName, sizeof( personaName ), SteamFriends()->GetPersonaName() );
		if ( !personaName[0] || !strlen( personaName ) )
		{
			strncpy( personaName, "Someone", sizeof( personaName ) - 1 );
			personaName[sizeof( personaName ) - 1] = 0;
		}
		for ( size_t length = strlen( personaName ); length && personaName[length - 1] == ' '; --length )
			personaName[length - 1] = 0;
	}
#ifdef _WIN32
	_snprintf( hostName, sizeof( hostName ) - 1, "%s's Server", personaName );
#else
	snprintf( hostName, sizeof( hostName ) - 1, "%s's Server", personaName );
#endif
	hostName[sizeof( hostName ) - 1] = 0;

	for ( ; object; object = object->pNext )
	{
		mpcontrol_t* item = new mpcontrol_t( parent, object->cvarname );
		item->type = object->type;
		switch ( object->type )
		{
		case O_BOOL:
		{
			vgui2::CheckButton* checkbox = new vgui2::CheckButton( item, "DescCheckButton", object->prompt );
			checkbox->SetSelected( object->fdefValue != 0.0f );
			item->pControl = checkbox;
			break;
		}

		case O_NUMBER:
		case O_STRING:
		{
			vgui2::TextEntry* entry = new vgui2::TextEntry( item, "DescEdit" );
			entry->InsertString( !strcmp( object->cvarname, "hostname" ) ? hostName : object->defValue );
			item->pControl = entry;
			break;
		}

		case O_LIST:
		{
			vgui2::ComboBox* combo = new vgui2::ComboBox( item, "DescEdit", 5, false );
			for ( CScriptListItem* choice = object->pListItems; choice; choice = choice->pNext )
				combo->AddItem( choice->szItemText, NULL );
			combo->ActivateItem( (int)object->fdefValue );
			item->pControl = combo;
			break;
		}

		default:
			break;
		}

		if ( item->type != O_BOOL )
		{
			item->pPrompt = new vgui2::Label( item, "DescLabel", "" );
			item->pPrompt->SetContentAlignment( vgui2::Label::a_west );
			item->pPrompt->SetTextInset( GetProportionalScaledValue( 5 ), 0 );
			item->pPrompt->SetText( object->prompt );
		}
		item->pScrObj = object;
		int tall = GetProportionalScaledValue( 28 );
		int wide = GetProportionalScaledValue( 100 );
		item->SetSize( wide, tall );
		m_pOptionsList->AddItem( item );

		if ( m_pList )
		{
			mpcontrol_t* last = m_pList;
			while ( last->next )
				last = last->next;
			last->next = item;
		}
		else
			m_pList = item;
		item->next = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor, load/save server settings object
//-----------------------------------------------------------------------------
CServerDescription::CServerDescription( CPanelListPanel* panel )
	: CDescription( panel )
{
	setHint(
		"// NOTE:  THIS FILE IS AUTOMATICALLY REGENERATED, \r\n"
		"//DO NOT EDIT THIS HEADER, YOUR COMMENTS WILL BE LOST IF YOU DO\r\n"
		"// Multiplayer options script\r\n"
		"//\r\n"
		"// Format:\r\n"
		"//  Version [float]\r\n"
		"//  Options description followed by \r\n"
		"//  Options defaults\r\n"
		"//\r\n"
		"// Option description syntax:\r\n"
		"//\r\n"
		"//  \"cvar\" { \"Prompt\" { type [ type info ] } { default } }\r\n"
		"//\r\n"
		"//  type = \r\n"
		"//   BOOL   (a yes/no toggle)\r\n"
		"//   STRING\r\n"
		"//   NUMBER\r\n"
		"//   LIST\r\n"
		"//\r\n"
		"// type info:\r\n"
		"// BOOL                 no type info\r\n"
		"// NUMBER       min max range, use -1 -1 for no limits\r\n"
		"// STRING       no type info\r\n"
		"// LIST          delimited list of options value pairs\r\n"
		"//\r\n"
		"//\r\n"
		"// default depends on type\r\n"
		"// BOOL is \"0\" or \"1\"\r\n"
		"// NUMBER is \"value\"\r\n"
		"// STRING is \"value\"\r\n"
		"// LIST is \"index\", where index \"0\" is the first element of the list\r\n"
		"\r\n"
		"\r\n" );
	setDescription( "SERVER_OPTIONS" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CServerDescription::WriteFileHeader( FileHandle_t file )
{
	char ampm[3] = "AM";
	time_t currentTime;
	time( &currentTime );
	tm localTime = *localtime( &currentTime );
	if ( localTime.tm_hour > 12 ) /* Set up extension. */
	{
		strcpy( ampm, "PM" );
		localTime.tm_hour -= 12; /*   to 12-hour clock.  */
	}
	else if ( !localTime.tm_hour )
		localTime.tm_hour = 12;

	vgui2::filesystem()->FPrintf( file, (char*)"// Half-Life Server Configuration Settings\r\n" );
	vgui2::filesystem()->FPrintf( file, (char*)"// DO NOT EDIT, GENERATED BY HALF-LIFE\r\n" );
	vgui2::filesystem()->FPrintf( file, (char*)"// File generated:  %.19s %s\r\n", asctime( &localTime ), ampm );
	vgui2::filesystem()->FPrintf( file, (char*)"//\r\n//\r\n// Cvar\t-\tSetting\r\n\r\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CServerDescription::WriteScriptHeader( FileHandle_t file )
{
	char ampm[3] = "AM";
	time_t currentTime;
	time( &currentTime );
	tm localTime = *localtime( &currentTime );
	if ( localTime.tm_hour > 12 ) /* Set up extension. */
	{
		strcpy( ampm, "PM" );
		localTime.tm_hour -= 12; /*   to 12-hour clock.  */
	}
	else if ( !localTime.tm_hour )
		localTime.tm_hour = 12;

	vgui2::filesystem()->FPrintf( file, (char*)getHint() );
	vgui2::filesystem()->FPrintf( file,
		(char*)"// Half-Life Server Configuration Layout Script (stores last settings chosen, too)\r\n" );
	vgui2::filesystem()->FPrintf( file, (char*)"// File generated:  %.19s %s\r\n", asctime( &localTime ), ampm );
	vgui2::filesystem()->FPrintf( file, (char*)"//\r\n//\r\n// Cvar\t-\tSetting\r\n\r\n" );
	vgui2::filesystem()->FPrintf( file, (char*)"VERSION %.1f\r\n\r\n", 1.0 );
	vgui2::filesystem()->FPrintf( file, (char*)"DESCRIPTION SERVER_OPTIONS\r\n{\r\n" );
}
