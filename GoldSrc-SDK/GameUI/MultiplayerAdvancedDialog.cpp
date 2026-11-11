//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "MultiplayerAdvancedDialog.h"
#include "PanelListPanel.h"
#include "ScriptObject.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"
#include "Taskbar.h"

#include "Button.h"
#include "CheckButton.h"
#include "ComboBox.h"
#include "Label.h"
#include "TextEntry.h"
#include "vgui_controls/Controls.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"GameUI\\MultiplayerAdvancedDialog.cpp" )

class CInfoDescription : public CDescription
{
public:
	CInfoDescription( CPanelListPanel* panel );
	void WriteFileHeader( FileHandle_t file ) override;
	void WriteScriptHeader( FileHandle_t file ) override;
	bool InitFromFile( char* filename ) override;
};

CMultiplayerAdvancedDialog::CMultiplayerAdvancedDialog( vgui2::Panel* parent )
	: CTaskFrame( parent, "MultiplayerAdvancedDialog" )
{
	SetBounds( 0, 0, GetProportionalScaledValue( 372 ), GetProportionalScaledValue( 160 ) );
	SetSizeable( false );
	MakePopup( true, false );
	SetCloseSound( "sound/UI/launch_dnmenu1.wav" );
	g_pTaskbar->AddTask( GetVPanel() );
	SetTitle( "#GameUI_MultiplayerAdvanced", true );

	vgui2::Button* cancel = new vgui2::Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );
	vgui2::Button* okay = new vgui2::Button( this, "OK", "#GameUI_OK" );
	okay->SetCommand( "Ok" );
	m_pListPanel = new CPanelListPanel( this, "PanelListPanel", false );
	m_pList = NULL;
	m_pDescription = new CInfoDescription( m_pListPanel );
	m_pDescription->InitFromFile( (char*)"user.scr" );
	m_pDescription->TransferCurrentValues( NULL );
	CreateControls();
	LoadControlSettings( "Resource\\MultiplayerAdvancedDialog.res", NULL );
}

CMultiplayerAdvancedDialog::~CMultiplayerAdvancedDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedDialog::OnClose()
{
	vgui2::Frame::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedDialog::OnCommand( const char* command )
{
	if ( !stricmp( command, "Ok" ) )
	{
		// OnApplyChanges();
		SaveValues();
		OnClose();
	}
	else
		vgui2::Frame::OnCommand( command );
}

vgui2::Panel* CMultiplayerAdvancedDialog::GetPanel()
{
	return this;
}

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
CMultiplayerAdvancedPage::CMultiplayerAdvancedPage( vgui2::Panel* parent )
	: vgui2::PropertyPage( parent, "MultiplayerAdvancedDialog", true )
{
	m_pListPanel = new CPanelListPanel( this, "PanelListPanel", false );
	m_pList = NULL;
	m_pDescription = new CInfoDescription( m_pListPanel );
	m_pDescription->InitFromFile( (char*)"user.scr" );
	m_pDescription->TransferCurrentValues( NULL );
	CreateControls();
	LoadControlSettings( "Resource\\MultiplayerAdvancedPage.res", NULL );
}

CMultiplayerAdvancedPage::~CMultiplayerAdvancedPage()
{
}

void CMultiplayerAdvancedPage::OnResetData()
{
	GatherCurrentValues();
}

void CMultiplayerAdvancedPage::OnApplyChanges()
{
	SaveValues();
}

vgui2::Panel* CMultiplayerAdvancedPage::GetPanel()
{
	return this;
}

void CMultiplayerAdvanced::GatherCurrentValues()
{
	if ( !m_pDescription )
		return;

	char value[256];
	char text[256];
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
			int activeItem = ( (vgui2::ComboBox*)item->pControl )->GetActiveItem();
			CScriptListItem* choice = object->pListItems;
			while ( choice )
			{
				if ( !activeItem-- )
					break;

				choice = choice->pNext;
			}

			if ( choice )
			{
				sprintf( value, "%s", choice->szValue );
			}
			else  // Couln't find index
			{
				ASSERT_AT( !("Couldn't find string in list, using default value"), 196 );
				sprintf( value, "%s", object->defValue );
			}
			break;
		}

		default:
			value[0] = 0;
			break;
		}

		// Remove double quotes and % characters
		UTIL_StripInvalidCharacters( value );
		strcpy( text, value );
		object->SetCurValue( text );
	}
}

void CMultiplayerAdvanced::DestroyControls()
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
}

void CMultiplayerAdvanced::SaveValues()
{
	// Get the values from the controls:
	GatherCurrentValues();
	if ( !m_pDescription )
		return;

	// Add settings to config.cfg
	m_pDescription->WriteToConfig();
	FileHandle_t file = vgui2::filesystem()->Open( "user.scr", "wb", "GAMECONFIG" );
	if ( file )
	{
		m_pDescription->WriteToScriptFile( file );
		vgui2::filesystem()->Close( file );
	}
}

void CMultiplayerAdvanced::CreateControls()
{
	DestroyControls();
	for ( CScriptObject* object = m_pDescription->pObjList; object; object = object->pNext )
	{
		mpcontrol_t* item = new mpcontrol_t( m_pListPanel, "mpcontrol_t" );
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
			vgui2::TextEntry* entry = new vgui2::TextEntry( item, "DescTextEntry" );
			entry->InsertString( object->defValue );
			item->pControl = entry;
			break;
		}

		case O_LIST:
		{
			vgui2::ComboBox* combo = new vgui2::ComboBox( item, "DescComboBox", 5, false );
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
			item->pPrompt->SetTextInset( GetPanel()->GetProportionalScaledValue( 5 ), 0 );
			item->pPrompt->SetText( object->prompt );
		}
		item->pScrObj = object;
		int tall = GetPanel()->GetProportionalScaledValue( 28 );
		int wide = GetPanel()->GetProportionalScaledValue( 100 );
		item->SetSize( wide, tall );
		m_pListPanel->AddItem( item );
		// Link it in
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

CInfoDescription::CInfoDescription( CPanelListPanel* panel ) : CDescription( panel )
{
	setHint(
		"// NOTE:  THIS FILE IS AUTOMATICALLY REGENERATED, \r\n"
		"//DO NOT EDIT THIS HEADER, YOUR COMMENTS WILL BE LOST IF YOU DO\r\n"
		"// User options script\r\n"
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
	setDescription( "INFO_OPTIONS" );
}

bool CInfoDescription::InitFromFile( char* filename )
{
	bool loaded = CDescription::InitFromFile( filename );
	if ( !stricmp( ModInfo()->GetGameDescription(), "Condition Zero" ) )
	{
		for ( CScriptObject* object = pObjList; object; object = object->pNext )
		{
			if ( !stricmp( object->cvarname, "cl_tutor" ) )
				strcpy( object->cvarname, "tutor_enable" );
		}
	}

	CInfoDescription* defaults = new CInfoDescription( NULL );
	FileHandle_t file = vgui2::filesystem()->Open( "user_default.scr", "rb", NULL );
	if ( file )
	{
		int size = vgui2::filesystem()->Size( file );
		char* buffer = new char[size + 1];
		ASSERT_AT( buffer, 452 );
		vgui2::filesystem()->Read( buffer, size, file );
		vgui2::filesystem()->Close( file );
		buffer[size] = 0;

		char* data = buffer;
		defaults->ReadFromBuffer( &data );
		delete[] buffer;
	}

	for ( CScriptObject* object = defaults->pObjList; object; object = object->pNext )
	{
		CScriptObject* existing = pObjList;
		while ( existing && stricmp( existing->cvarname, object->cvarname ) )
			existing = existing->pNext;
		if ( existing )
			continue;

		CScriptObject* copy = new CScriptObject;
		copy->type = object->type;
		memcpy( copy->cvarname, object->cvarname, sizeof( copy->cvarname ) );
		memcpy( copy->prompt, object->prompt, sizeof( copy->prompt ) );
		copy->fMin = object->fMin;
		copy->fMax = object->fMax;
		memcpy( copy->defValue, object->defValue, sizeof( copy->defValue ) );
		copy->fdefValue = object->fdefValue;
		memcpy( copy->curValue, object->curValue, sizeof( copy->curValue ) );
		copy->fcurValue = object->fcurValue;
		copy->bSetInfo = object->bSetInfo;
		for ( CScriptListItem* choice = object->pListItems; choice; choice = choice->pNext )
		{
			CScriptListItem* item = new CScriptListItem;
			memcpy( item->szItemText, choice->szItemText, sizeof( item->szItemText ) );
			memcpy( item->szValue, choice->szValue, sizeof( item->szValue ) );
			copy->AddItem( item );
		}
		AddObject( copy );
	}
	delete defaults;
	return loaded;
}

void CInfoDescription::WriteFileHeader( FileHandle_t file )
{
	time_t currentTime;
	time( &currentTime );
	tm localTime = *localtime( &currentTime );
	vgui2::filesystem()->FPrintf( file, (char*)"// Half-Life User Info Configuration Settings\r\n" );
	vgui2::filesystem()->FPrintf( file, (char*)"// DO NOT EDIT, GENERATED BY HALF-LIFE\r\n" );
	vgui2::filesystem()->FPrintf( file, (char*)"// File generated:  %.19s %s\r\n", asctime( &localTime ), "AM" );
	vgui2::filesystem()->FPrintf( file, (char*)"//\r\n//\r\n// Cvar\t-\tSetting\r\n\r\n" );
}

void CInfoDescription::WriteScriptHeader( FileHandle_t file )
{
	time_t currentTime;
	time( &currentTime );
	tm localTime = *localtime( &currentTime );
	vgui2::filesystem()->FPrintf( file, (char*)getHint() );
	vgui2::filesystem()->FPrintf( file,
		(char*)"// Half-Life User Info Configuration Layout Script (stores last settings chosen, too)\r\n" );
	vgui2::filesystem()->FPrintf( file, (char*)"// File generated:  %.19s %s\r\n", asctime( &localTime ), "AM" );
	vgui2::filesystem()->FPrintf( file, (char*)"//\r\n//\r\n// Cvar\t-\tSetting\r\n\r\n" );
	vgui2::filesystem()->FPrintf( file, (char*)"VERSION %.1f\r\n\r\n", 1.0 );
	vgui2::filesystem()->FPrintf( file, (char*)"DESCRIPTION INFO_OPTIONS\r\n{\r\n" );
}
