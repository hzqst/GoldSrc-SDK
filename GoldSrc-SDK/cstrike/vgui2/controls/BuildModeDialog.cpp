//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BuildModeDialog.h"

#include "BuildGroup.h"
#include "Button.h"
#include "vgui_controls/ComboBox.h"
#include "Divider.h"
#include "Label.h"
#include "Menu.h"
#include "MenuButton.h"
#include "MessageBox.h"
#include "PanelListPanel.h"
#include "TextEntry.h"
#include "controls.h"
#include "ILocalize.h"

#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "BuildModeLayout.h"

#define BUILD_TYPE_STRING		0
#define BUILD_TYPE_ALIGNMENT	3
#define BUILD_TYPE_AUTORESIZE	4
#define BUILD_TYPE_CORNER		5
#define BUILD_TYPE_LOCALIZE		6

const char* ParseTokenFromString( const char** string )
{
	static char buf[128];

	// find the first alnum character
	const char* tok = *string;
	int pos = 0;

	while ( *tok && !isalnum( (unsigned char)*tok ) )
		tok++;

	// read in all the alnum characters
	while ( isalnum( (unsigned char)*tok ) )
		buf[pos++] = *tok++;

	buf[pos] = 0;
	*string = tok;
	return buf;
}

namespace vgui2
{
BuildModeDialog::BuildModeDialog( BuildGroup* buildGroup ) : Frame( NULL, NULL, true, false )
{
	Panel* context = buildGroup->GetContextPanel();
	SetProportional( context && context->IsProportional() );
	SetHDProportional( context && context->IsHDProportional() );
	SetMinimumSize( GetProportionalScaledValue( 300 ), GetProportionalScaledValue( 256 ) );
	SetSize( GetProportionalScaledValue( 300 ), GetProportionalScaledValue( 400 ) );

	m_pCurrentPanel = NULL;
	_undoSettings = NULL;
	m_pBuildGroup = buildGroup;
	_copySettings = NULL;
	_autoUpdate = false;

	SetTitle( "VGUI Build Mode Editor", true );
	CreateControls();
	LoadUserConfig( "BuildModeDialog", 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
BuildModeDialog::~BuildModeDialog()
{
	m_pPanelList->m_pResourceData->deleteThis();
	m_pPanelList->m_pControls->DeleteAllItems();

	if ( _undoSettings )
		_undoSettings->deleteThis();
	if ( _copySettings )
		_copySettings->deleteThis();

}


//-----------------------------------------------------------------------------
// Purpose: Revert to the stored the settings of the current panel in a keyValues
//-----------------------------------------------------------------------------
void BuildModeDialog::DoUndo()
{
	if ( _undoSettings )
	{
		m_pCurrentPanel->ApplySettings( _undoSettings );
		UpdateControlData( m_pCurrentPanel );
		_undoSettings->deleteThis();
		_undoSettings = NULL;
	}

	m_pSaveButton->SetEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: enable the save button, useful when buildgroup needs to Activate it.
//-----------------------------------------------------------------------------
void BuildModeDialog::EnableSaveButton()
{
	m_pSaveButton->SetEnabled( true );
}

void BuildModeDialog::ShutdownBuildMode()
{
	m_pBuildGroup->SetEnabled( false );
}

void BuildModeDialog::OnPanelMoved()
{
	m_pApplyButton->SetEnabled( true );
}

KeyValues* BuildModeDialog::StoreSettings()
{
	KeyValues* storedSettings = new KeyValues( m_pCurrentPanel->GetName() );

	for ( int i = 0; i < m_pPanelList->m_PanelList.Count(); i++ )
	{
		PanelItem_t& item = m_pPanelList->m_PanelList[i];

		char buf[512];
		if ( item.m_EditPanel )
			item.m_EditPanel->GetText( buf, 512 );
		else
			item.m_EditButton->GetText( buf, 512 );

		if ( item.m_iType == BUILD_TYPE_AUTORESIZE || item.m_iType == BUILD_TYPE_CORNER )
			storedSettings->SetInt( item.m_szName, atoi( buf ) );
		else
			storedSettings->SetString( item.m_szName, buf );
	}

	return storedSettings;
}

void BuildModeDialog::DoCopy()
{
	if ( _copySettings )
		_copySettings->deleteThis();

	_copySettings = StoreSettings();
	strcpy( _copyClassName, m_pCurrentPanel->GetClassName() );
}

//-----------------------------------------------------------------------------
// Purpose: message handles thats sets the text in the clipboard
//-----------------------------------------------------------------------------
void BuildModeDialog::OnSetClipboardText( const char* text )
{
	system()->SetClipboardText( text, (int)strlen( text ) );
}

void BuildModeDialog::StoreUndoSettings()
{
	bool editable = m_pCurrentPanel->IsBuildModeEditable();

	if ( _undoSettings )
		_undoSettings->deleteThis();
	_undoSettings = NULL;

	if ( editable )
		_undoSettings = StoreSettings();
}


//-----------------------------------------------------------------------------
// Purpose: Deletes a panel from the buildgroup
//-----------------------------------------------------------------------------
void BuildModeDialog::OnDeletePanel()
{
	if ( !m_pCurrentPanel->IsBuildModeEditable() )
		return;

	m_pBuildGroup->RemoveSettings();
	SetActiveControl( m_pBuildGroup->GetCurrentPanel() );

	_undoSettings->deleteThis();
	_undoSettings = NULL;
	m_pSaveButton->SetEnabled( true );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void BuildModeDialog::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ENTER ) // if someone hits return apply the changes
		ApplyDataToControls();
	else
		Frame::OnKeyCodeTyped( code );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the data in a single edit control
//-----------------------------------------------------------------------------
void BuildModeDialog::UpdateEditControl( PanelItem_t& panelItem, const char* datstring )
{
	switch ( panelItem.m_iType )
	{
	case BUILD_TYPE_AUTORESIZE:
	case BUILD_TYPE_CORNER:
		panelItem.m_pCombo->ActivateItemByRow( atoi( datstring ) );
		break;

	case BUILD_TYPE_LOCALIZE:
		panelItem.m_EditButton->SetText( datstring );
		break;

	default:
		{
			wchar_t unicode[512];
			localize()->ConvertANSIToUnicode( datstring, unicode, sizeof( unicode ) );
			panelItem.m_EditPanel->SetText( unicode );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the edit fields with information about the control
//-----------------------------------------------------------------------------
void BuildModeDialog::UpdateControlData( Panel* control )
{
	KeyValues* dat = m_pPanelList->m_pResourceData->FindKey( control->GetName(), true );
	control->GetSettings( dat );

	// apply the settings to the edit panels
	for ( int i = 0; i < m_pPanelList->m_PanelList.Count(); i++ )
	{
		PanelItem_t& item = m_pPanelList->m_PanelList[i];
		UpdateEditControl( item, dat->GetString( item.m_szName, "" ) );
	}

	char statusText[512];
	_snprintf( statusText, 511, "%s: '%s'", control->GetClassName(), control->GetName() );
	statusText[sizeof( statusText ) - 1] = 0;
	m_pStatusLabel->SetText( statusText );
	m_pStatusLabel->SetTextColorState( Label::CS_NORMAL );
}

//-----------------------------------------------------------------------------
// Purpose: Create a new Panel with the _copySettings applied
//-----------------------------------------------------------------------------
void BuildModeDialog::DoPaste()
{
	int x = 0;
	int y = 0;
	input()->GetCursorPos( x, y );
	m_pBuildGroup->GetContextPanel()->ScreenToLocal( x, y );

	Panel* newPanel = OnNewControl( _copyClassName, x, y );
	if ( newPanel )
	{
		newPanel->ApplySettings( _copySettings );
		newPanel->SetPos( x, y );

		char name[255];
		m_pBuildGroup->GetNewFieldName( name, newPanel );
		newPanel->SetName( name );
	}
}

void BuildModeDialog::OnTextChanged( Panel* panel )
{
	char buf[512];

	if ( panel == m_pFileSelectionCombo )
	{
		m_pFileSelectionCombo->GetText( buf, 512 );
		if ( _stricmp( buf, m_pBuildGroup->GetResourceName() ) )
		{
			// file has changed, reload
			SetActiveControl( NULL );
			m_pBuildGroup->ChangeControlSettingsFile( buf );
		}
		return;
	}

	if ( panel == m_pAddNewControlCombo )
	{
		m_pAddNewControlCombo->GetText( buf, 40 );
		if ( _stricmp( buf, "None" ) )
		{
			OnNewControl( buf, 0, 0 );
			// reset box back to None
			m_pAddNewControlCombo->ActivateItemByRow( 0 );
		}
	}

	if ( m_pCurrentPanel && m_pCurrentPanel->IsBuildModeEditable() )
		m_pApplyButton->SetEnabled( true );

	if ( _autoUpdate )
		ApplyDataToControls();
}

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
BuildModeDialog::PanelList::PanelList()
{
}

void BuildModeDialog::PanelList::AddItem(
	Panel* label, TextEntry* edit, ComboBox* combo, Button* button, const char* name, int type )
{
	PanelItem_t item;
	item.m_EditLabel = label;
	item.m_EditPanel = edit;
	item.m_pCombo = combo;
	item.m_EditButton = button;
	strncpy( item.m_szName, name, sizeof( item.m_szName ) - 1 );
	item.m_szName[sizeof( item.m_szName ) - 1] = 0;
	item.m_iType = type;

	m_PanelList.AddToTail( item );
}

void BuildModeDialog::PanelList::RemoveAll()
{
	for ( int i = 0; i < m_PanelList.Count(); i++ )
	{
		PanelItem_t& item = m_PanelList[i];
		delete item.m_EditLabel;
		delete item.m_EditPanel;
		delete item.m_EditButton;
	}

	m_PanelList.RemoveAll();
	m_pControls->RemoveAll();
}

void BuildModeDialog::RemoveAllControls()
{
	// free the array
	m_pPanelList->RemoveAll();
}

void BuildModeDialog::ExitBuildMode()
{
	// make sure rulers are off
	if ( m_pBuildGroup->HasRulersOn() )
		m_pBuildGroup->ToggleRulerDisplay();

	m_pBuildGroup->SetEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Create a new control in the context panel
//-----------------------------------------------------------------------------
Panel* BuildModeDialog::OnNewControl( const char* name, int x, int y )
{
	// returns NULL on failure
	Panel* newPanel = m_pBuildGroup->NewControl( name, x, y );
	if ( newPanel )
	{
	   // call mouse commands to simulate selecting the new
		// panel. This will set everything up correctly in the buildGroup.
		m_pBuildGroup->MousePressed( MOUSE_LEFT, newPanel );
		m_pBuildGroup->MouseReleased( MOUSE_LEFT, newPanel );
	}

	m_pSaveButton->SetEnabled( true );
	return newPanel;
}

//-----------------------------------------------------------------------------
// Purpose: Revert to the saved settings in the .res file
//-----------------------------------------------------------------------------
void BuildModeDialog::RevertToSaved()
{
	// hide the dialog as reloading will destroy it
	surface()->SetPanelVisible( GetVPanel(), false );
	m_pBuildGroup->ReloadControlSettings();
}

//-----------------------------------------------------------------------------
// Purpose: Display some information about the editor
//-----------------------------------------------------------------------------
void BuildModeDialog::ShowHelp()
{
	const char helpText[] =
		"In the Build Mode Dialog Window:\n"
		"Delete button - deletes the currently selected panel if it is deletable.\n"
		"Apply button - applies changes to the Context Panel.\n"
		"Save button - saves all settings to file. \n"
		"Revert to saved- reloads the last saved file.\n"
		"Auto Update - any changes apply instantly.\n"
		"Typing Enter in any text field applies changes.\n"
		"New Control menu - creates a new panel in the upper left corner.\n"
		"\n"
		"In the Context Panel:\n"
		"After selecting and moving a panel Ctrl-z will undo the move.\n"
		"Shift clicking panels allows multiple panels to be selected into a group.\n"
		"Ctrl-c copies the settings of the last selected panel.\n"
		"Ctrl-v creates a new panel with the copied settings at the location of the mouse pointer.\n"
		"Arrow keys slowly move panels, holding shift + arrow will slowly resize it.\n"
		"Holding right mouse button down opens a dropdown panel creation menu.\n"
		"  Panel will be created where the menu was opened.\n"
		"Delete key deletes the currently selected panel if it is deletable.\n"
		"  Does nothing to multiple selections.";

	MessageBox* helpDlg = new MessageBox( "Build Mode Help", helpText, this );
	helpDlg->AddActionSignalTarget( this );
	helpDlg->DoModal( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: makes sure build mode has been shut down properly
//-----------------------------------------------------------------------------
void BuildModeDialog::OnClose()
{
	if ( m_pBuildGroup->IsEnabled() )
	{
		m_pBuildGroup->SetEnabled( false );
	}
	else
	{
		Frame::OnClose();
		MarkForDeletion();
	}
}

void BuildModeDialog::PerformLayout()
{
	Frame::PerformLayout();

	int clientX, clientY, clientWide, clientTall;
	GetClientArea( clientX, clientY, clientWide, clientTall );
	int margin = GetProportionalScaledValue( 16 );
	int gap = GetProportionalScaledValue( 6 );
	int rowHeight = EditorControlHeight( m_pStatusLabel );
	Panel* controls[] = { m_pFileSelectionCombo, m_pAddNewControlCombo, m_pExitButton,
		m_pSaveButton, m_pApplyButton, m_pDeleteButton, m_pVarsButton };
	for ( Panel* control : controls )
		rowHeight = std::max( rowHeight, EditorControlHeight( control ) );
	int x = std::max( margin, clientX );
	int width = std::max( 0, GetWide() - 2 * x );
	int fileY = clientY + gap;
	m_pFileSelectionCombo->SetBounds( x, fileY, width, rowHeight );
	int statusY = fileY + rowHeight + gap;
	m_pStatusLabel->SetBounds( x, statusY, width, rowHeight );
	int buttonWidth = GetProportionalScaledValue( 64 );
	int buttonY = clientY + clientTall - gap - rowHeight;
	m_pApplyButton->SetBounds( x + width - buttonWidth, buttonY, buttonWidth, rowHeight );
	m_pExitButton->SetBounds( x + width - 2 * buttonWidth - gap, buttonY, buttonWidth, rowHeight );
	m_pSaveButton->SetBounds( x + width - 3 * buttonWidth - 2 * gap, buttonY, buttonWidth, rowHeight );
	int dividerHeight = std::max( 1, GetProportionalScaledValue( 2 ) );
	int dividerY = buttonY - gap - dividerHeight;
	m_pDivider->SetBounds( x, dividerY, width, dividerHeight );
	int toolY = dividerY - gap - rowHeight;
	int varsWidth = GetProportionalScaledValue( 72 );
	m_pVarsButton->SetBounds( x, toolY, varsWidth, rowHeight );
	int deleteX = x + varsWidth + gap;
	m_pDeleteButton->SetBounds( deleteX, toolY, buttonWidth, rowHeight );
	int addX = deleteX + buttonWidth + gap;
	m_pAddNewControlCombo->SetBounds( addX, toolY, std::max( 0, x + width - addX ), rowHeight );
	int listY = statusY + rowHeight + gap;
	m_pPanelList->m_pControls->SetBounds( x, listY, width, std::max( 0, toolY - gap - listY ) );
}

//-----------------------------------------------------------------------------
// Purpose: Called when one of the buttons is pressed
//-----------------------------------------------------------------------------
void BuildModeDialog::OnCommand( const char* command )
{
	if ( !_stricmp( command, "Save" ) )
	{
		// apply the current data and save it to disk
		ApplyDataToControls();
		if ( m_pBuildGroup->SaveControlSettings() )
			m_pSaveButton->SetEnabled( false );
	}
	else if ( !_stricmp( command, "Exit" ) )
	{
		// exit build mode
		ExitBuildMode();
	}
	else if ( !_stricmp( command, "Apply" ) )
	{
		// apply data to controls
		ApplyDataToControls();
	}
	else if ( !_stricmp( command, "DeletePanel" ) )
	{
		OnDeletePanel();
	}
	else if ( !_stricmp( command, "RevertToSaved" ) )
	{
		RevertToSaved();
	}
	else if ( !_stricmp( command, "ShowHelp" ) )
	{
		ShowHelp();
	}
	else
	{
		Frame::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets up the current control to edit
//-----------------------------------------------------------------------------
void BuildModeDialog::SetActiveControl( Panel* controlToEdit )
{
	if ( m_pCurrentPanel == controlToEdit )
	{
		// it's already set, so just update the property data and quit
		if ( m_pCurrentPanel )
			UpdateControlData( m_pCurrentPanel );
		return;
	}

	// reset the data
	m_pCurrentPanel = controlToEdit;
	RemoveAllControls();
	m_pPanelList->m_pControls->MoveScrollBarToTop();

	if ( !m_pCurrentPanel )
	{
		m_pStatusLabel->SetText( "[nothing currently selected]" );
		m_pStatusLabel->SetTextColorState( Label::CS_DULL );
		RemoveAllControls();
		return;
	}

	// get the control description string
	const char* controlDesc = m_pCurrentPanel->GetDescription();

	// parse out the control description
	int tabPosition = 1;
	while ( true )
	{
		const char* dataType = ParseTokenFromString( &controlDesc );
		if ( !dataType[0] )
			break;

		int datat;
		if ( !_stricmp( dataType, "int" ) )
			datat = BUILD_TYPE_STRING;
		else if ( !_stricmp( dataType, "alignment" ) )
			datat = BUILD_TYPE_ALIGNMENT;
		else if ( !_stricmp( dataType, "autoresize" ) )
			datat = BUILD_TYPE_AUTORESIZE;
		else if ( !_stricmp( dataType, "corner" ) )
			datat = BUILD_TYPE_CORNER;
		else if ( !_stricmp( dataType, "localize" ) )
			datat = BUILD_TYPE_LOCALIZE;
		else
			datat = BUILD_TYPE_STRING;

		// get the field name
		const char* fieldName = ParseTokenFromString( &controlDesc );

		// build a control & label
		Label* label = new Label( this, NULL, fieldName );
		label->SetSize( GetProportionalScaledValue( 96 ), EditorControlHeight( label ) );
		label->SetContentAlignment( Label::a_east );

		TextEntry* edit = NULL;
		ComboBox* editCombo = NULL;
		Button* editButton = NULL;
		if ( datat == BUILD_TYPE_ALIGNMENT )
		{
			// drop-down combo box
			editCombo = new ComboBox( this, NULL, 9, false );
			editCombo->AddItem( "north-west", NULL );
			editCombo->AddItem( "north", NULL );
			editCombo->AddItem( "north-east", NULL );
			editCombo->AddItem( "west", NULL );
			editCombo->AddItem( "center", NULL );
			editCombo->AddItem( "east", NULL );
			editCombo->AddItem( "south-west", NULL );
			editCombo->AddItem( "south", NULL );
			editCombo->AddItem( "south-east", NULL );
		}
		else if ( datat == BUILD_TYPE_AUTORESIZE )
		{
			// drop-down combo box
			editCombo = new ComboBox( this, NULL, 4, false );
			editCombo->AddItem( "0 - no auto-resize", NULL );
			editCombo->AddItem( "1 - resize right", NULL );
			editCombo->AddItem( "2 - resize down", NULL );
			editCombo->AddItem( "3 - down & right", NULL );
		}
		else if ( datat == BUILD_TYPE_CORNER )
		{
			// drop-down combo box
			editCombo = new ComboBox( this, NULL, 4, false );
			editCombo->AddItem( "0 - top-left", NULL );
			editCombo->AddItem( "1 - top-right", NULL );
			editCombo->AddItem( "2 - bottom-left", NULL );
			editCombo->AddItem( "3 - bottom-right", NULL );
		}
		else if ( datat == BUILD_TYPE_LOCALIZE )
		{
			editButton = new Button( this, NULL, "..." );
			editButton->SetParent( this );
			editButton->AddActionSignalTarget( this );
			editButton->SetTabPosition( tabPosition++ );
			label->SetAssociatedControl( editButton );
		}
		else
		{
			// normal string edit
			edit = new TextEntry( this, NULL );
		}

		Panel* editPanel = editCombo ? (Panel*)editCombo : (Panel*)edit;
		if ( datat != BUILD_TYPE_LOCALIZE && editPanel )
		{
			editPanel->SetParent( this );
			editPanel->AddActionSignalTarget( this );
			editPanel->SetTabPosition( tabPosition++ );
			label->SetAssociatedControl( editPanel );
		}

		m_pPanelList->AddItem( label, editCombo ? editCombo : edit, editCombo, editButton, fieldName, datat );

		if ( editPanel )
			m_pPanelList->m_pControls->AddItem( label, editPanel );
		else
			m_pPanelList->m_pControls->AddItem( label, editButton );
	}

	m_pDeleteButton->SetEnabled( controlToEdit->IsBuildModeDeletable() );

	// update the property data in the dialog
	UpdateControlData( m_pCurrentPanel );

	// set our title
	const char* resourceName = m_pBuildGroup->GetResourceName();
	if ( resourceName )
		m_pFileSelectionCombo->SetText( resourceName );
	else
		m_pFileSelectionCombo->SetText( "[ no resource file associated with dialog ]" );

	m_pApplyButton->SetEnabled( false );
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Creates the build mode editing controls
//-----------------------------------------------------------------------------
void BuildModeDialog::CreateControls()
{
	m_pPanelList = new PanelList;
	m_pPanelList->m_pResourceData = new KeyValues( "BuildDialog" );
	m_pPanelList->m_pControls = new BuildModePropertyList( this );

	// file to edit combo box is first
	m_pFileSelectionCombo = new ComboBox( this, "FileSelectionCombo", 10, false );
	for ( int i = 0; i < m_pBuildGroup->GetRegisteredControlSettingsFileCount(); i++ )
		m_pFileSelectionCombo->AddItem( m_pBuildGroup->GetRegisteredControlSettingsFileByIndex( i ), NULL );
	if ( m_pFileSelectionCombo->GetItemCount() <= 1 )
		m_pFileSelectionCombo->SetEnabled( false );

	// status info at top of dialog
	m_pStatusLabel = new Label( this, "StatusLabel", "[nothing currently selected]" );
	m_pStatusLabel->SetTextColorState( Label::CS_DULL );

	m_pDivider = new Divider( this, "Divider" );

	// drop-down combo box for adding new controls
	m_pAddNewControlCombo = new ComboBox( this, NULL, 15, false );
	m_pAddNewControlCombo->SetSize( 116, 24 );
	m_pAddNewControlCombo->SetOpenDirection( ComboBox::UP );

	// controls that can be added
	// this list comes from controls EditablePanel can create by name.
	int defaultItem = m_pAddNewControlCombo->AddItem( "None", NULL );
	m_pAddNewControlCombo->AddItem( "Button", NULL );
	m_pAddNewControlCombo->AddItem( "CheckButton", NULL );
	m_pAddNewControlCombo->AddItem( "ComboBox", NULL );
	m_pAddNewControlCombo->AddItem( "Divider", NULL );
	m_pAddNewControlCombo->AddItem( "ImagePanel", NULL );
	m_pAddNewControlCombo->AddItem( "Label", NULL );
	m_pAddNewControlCombo->AddItem( "URLLabel", NULL );
	m_pAddNewControlCombo->AddItem( "ProgressBar", NULL );
	m_pAddNewControlCombo->AddItem( "RadioButton", NULL );
	m_pAddNewControlCombo->AddItem( "TextEntry", NULL );
	m_pAddNewControlCombo->AddItem( "ToggleButton", NULL );
	m_pAddNewControlCombo->ActivateItem( defaultItem );

	m_pExitButton = new Button( this, "ExitButton", "&Exit" );
	m_pExitButton->SetSize( 64, 24 );
	m_pSaveButton = new Button( this, "SaveButton", "&Save" );
	m_pSaveButton->SetSize( 64, 24 );
	m_pApplyButton = new Button( this, "ApplyButton", "&Apply" );
	m_pApplyButton->SetSize( 64, 24 );

	m_pExitButton->SetCommand( "Exit" );
	m_pSaveButton->SetCommand( "Save" );
	m_pApplyButton->SetCommand( "Apply" );

	m_pDeleteButton = new Button( this, "DeletePanelButton", "Delete" );
	m_pDeleteButton->SetSize( 64, 24 );
	m_pDeleteButton->SetCommand( "DeletePanel" );

	m_pVarsButton = new MenuButton( this, "VarsButton", "Variables" );
	m_pVarsButton->SetSize( 72, 24 );
	m_pVarsButton->SetOpenDirection( MenuButton::UP );

	// iterate the vars
	KeyValues* vars = m_pBuildGroup->GetDialogVariables();
	if ( vars && vars->GetFirstSubKey() )
	{
		// create the menu
		m_pVarsButton->SetEnabled( true );
		Menu* menu = new Menu( m_pVarsButton, "VarsMenu" );

		// set all the variables to be copied to the clipboard when selected
		for ( KeyValues* kv = vars->GetFirstSubKey(); kv; kv = kv->GetNextKey() )
		{
			char buf[32];
			_snprintf( buf, sizeof( buf ), "%%%s%%", kv->GetName() );
			buf[sizeof( buf ) - 1] = 0;
			menu->AddMenuItem( kv->GetName(), new KeyValues( "SetClipboardText", "text", buf ), this, NULL );
		}
		m_pVarsButton->SetMenu( menu );
	}
	else
	{
		// no variables
		m_pVarsButton->SetEnabled( false );
	}

	m_pApplyButton->SetTabPosition( 1 );
	m_pPanelList->m_pControls->SetTabPosition( 2 );
	m_pVarsButton->SetTabPosition( 3 );
	m_pDeleteButton->SetTabPosition( 4 );
	m_pAddNewControlCombo->SetTabPosition( 5 );
	m_pSaveButton->SetTabPosition( 6 );
	m_pExitButton->SetTabPosition( 7 );
}

void BuildModeDialog::ApplyDataToControls()
{
	// don't apply if the panel is not editable
	if ( !m_pCurrentPanel->IsBuildModeEditable() )
	{
		UpdateControlData( m_pCurrentPanel );
		return; // return success, since we are behaving as expected.
	}

	PanelItem_t& nameItem = m_pPanelList->m_PanelList[0];
	char fieldName[512];
	if ( nameItem.m_EditPanel )
		nameItem.m_EditPanel->GetText( fieldName, 512 );
	else
		nameItem.m_EditButton->GetText( fieldName, 512 );

	// check to see if any buildgroup panels have this name
	Panel* panel = m_pBuildGroup->FieldNameTaken( fieldName );
	if ( panel && panel != m_pCurrentPanel )
	{
		char messageString[255];
		const size_t maxNameLength = sizeof( messageString ) - sizeof( "Fieldname is not unique: \nRename it and try again." );
		if ( strlen( fieldName ) > maxNameLength )
			fieldName[maxNameLength] = 0;
		sprintf( messageString, "Fieldname is not unique: %s\nRename it and try again.", fieldName );
		MessageBox* errorBox = new MessageBox( "Cannot Apply", messageString, NULL );
		errorBox->DoModal( NULL );
		UpdateControlData( m_pCurrentPanel );
		m_pApplyButton->SetEnabled( false );
		return;
	}

	localize()->ReloadLocalizationFiles( filesystem() );

	KeyValues* settings = BuildModeDialog::StoreSettings();
	m_pCurrentPanel->ApplySettings( settings );

	m_pApplyButton->SetEnabled( false );
	m_pSaveButton->SetEnabled( true );
}
}
