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

#include "BuildGroup.h"
#include "Panel.h"
#include "Label.h"
#include "EditablePanel.h"
#include "MessageBox.h"
#include "controls.h"
#include "filesystem.h"
#include "vgui/IInput.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\BuildGroup.cpp" )

namespace vgui2
{
#ifdef _MSC_VER

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
#pragma warning( suppress : 26495 )
#endif
BuildGroup::BuildGroup( Panel* parentPanel, Panel* contextPanel )
{
	_enabled = false;
	_snapX = 1;
	_snapY = 1;
	_cursor_sizenwse = dc_sizenwse;
	_cursor_sizenesw = dc_sizenesw;
	_cursor_sizewe = dc_sizewe;
	_cursor_sizens = dc_sizens;
	_cursor_sizeall = dc_sizeall;
	_currentPanel = NULL;
	_dragging = false;
	m_pResourceName = NULL;
	m_pResourcePathID = NULL;
	m_hBuildDialog.Set( NULL );
	m_pParentPanel = parentPanel;
	for ( int i = 0; i < 4; i++ )
		_rulerNumber[i] = NULL;
	m_pBuildContext = contextPanel;
	_showRulers = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
BuildGroup::~BuildGroup()
{
	if ( m_hBuildDialog.Get() )
		delete m_hBuildDialog.Get();
	m_hBuildDialog.Set( NULL );

	delete[] m_pResourceName;
	delete[] m_pResourcePathID;

	for ( int i = 0; i < 4; i++ )
	{
		if ( _rulerNumber[i] )
		{
			delete _rulerNumber[i];
			_rulerNumber[i] = NULL;
		}
	}
}

void BuildGroup::LoadControlSettings( const char* resourceName, const char* pathID )
{
	// make sure the file is registered
	RegisterControlSettingsFile( resourceName, pathID );

	KeyValues* rDat = new KeyValues( resourceName );
	if ( pathID || !rDat->LoadFromFile( filesystem(), resourceName, "SKIN" ) )
		rDat->LoadFromFile( filesystem(), resourceName, pathID );

	char* oldResourceName = m_pResourceName;
	m_pResourceName = new char[strlen( resourceName ) + 1];
	strcpy( m_pResourceName, resourceName );
	delete[] oldResourceName;

	if ( pathID )
	{
		char* oldPathID = m_pResourcePathID;
		m_pResourcePathID = new char[strlen( pathID ) + 1];
		strcpy( m_pResourcePathID, pathID );
		delete[] oldPathID;
	}

	// delete any controls not in both files
	DeleteAllControlsCreatedByControlSettingsFile();

	// loop through the resource data sticking info into controls
	ApplySettings( rDat );

	if ( m_pParentPanel )
	{
		m_pParentPanel->InvalidateLayout( false, false );
		m_pParentPanel->Repaint();
	}

	rDat->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose:	Draw Rulers on screen if conditions are right
//-----------------------------------------------------------------------------
void BuildGroup::DrawRulers()
{
	// don't draw if visibility is off
	if ( !_showRulers )
		return;

	// no drawing if we selected the context panel
	if ( m_pBuildContext == _currentPanel )
	{
		SetRulerLabelsVisible( false );
		return;
	}
	SetRulerLabelsVisible( true );

	int wide, tall;
	int x, y;
	// get base panel's postition
	m_pBuildContext->GetBounds( x, y, wide, tall );
	m_pBuildContext->ScreenToLocal( x, y );

	int cx, cy, cwide, ctall;
	_currentPanel->GetBounds( cx, cy, cwide, ctall );

	surface()->PushMakeCurrent( m_pBuildContext->GetVPanel(), false );

	// draw rulers
	surface()->DrawSetColor( 255, 255, 255, 255 );	// white color

	surface()->DrawFilledRect( 0, cy, cx, cy + 1 );           //top horiz left
	surface()->DrawFilledRect( cx + cwide, cy, wide, cy + 1 );  //top horiz right
	surface()->DrawFilledRect( 0, cy + ctall - 1, cx, cy + ctall );   //bottom horiz left
	surface()->DrawFilledRect( cx + cwide, cy + ctall - 1, wide, cy + ctall );   //bottom	 horiz right

	surface()->DrawFilledRect( cx, 0, cx + 1, cy );         //top vert left
	surface()->DrawFilledRect( cx + cwide - 1, 0, cx + cwide, cy );  //top vert right
	surface()->DrawFilledRect( cx, cy + ctall, cx + 1, tall ); //bottom vert left
	surface()->DrawFilledRect( cx + cwide - 1, cy + ctall, cx + cwide, tall ); //bottom vert right   

	surface()->PopMakeCurrent( m_pBuildContext->GetVPanel() );

	// now let's put numbers with the rulers
	char textstring[20];
	int textWide, textTall;

	sprintf( textstring, "%d", cx );
	_rulerNumber[0]->SetText( textstring );
	_rulerNumber[0]->GetContentSize( textWide, textTall );
	_rulerNumber[0]->SetSize( textWide, textTall );
	_rulerNumber[0]->SetPos( cx / 2 - textWide / 2, cy - textTall + 3 );

	sprintf( textstring, "%d", cy );
	_rulerNumber[1]->SetText( textstring );
	_rulerNumber[1]->GetContentSize( textWide, textTall );
	_rulerNumber[1]->SetSize( textWide, textTall );
	_rulerNumber[1]->GetSize( textWide, textTall );
	_rulerNumber[1]->SetPos( cx - textWide + 3, cy / 2 - textTall / 2 );

	sprintf( textstring, "%d", cy );
	_rulerNumber[2]->SetText( textstring );
	_rulerNumber[2]->GetContentSize( textWide, textTall );
	_rulerNumber[2]->SetSize( textWide, textTall );
	_rulerNumber[2]->SetPos( ( wide - cx - cwide ) / 2 - textWide / 2 + cx + cwide, ctall - 3 + cy );

	sprintf( textstring, "%d", cy );
	_rulerNumber[3]->SetText( textstring );
	_rulerNumber[3]->GetContentSize( textWide, textTall );
	_rulerNumber[3]->SetSize( textWide, textTall );
	_rulerNumber[3]->SetPos( cx + cwide, ( tall - cy - ctall ) / 2 - textTall / 2 + cy + ctall );
}

void BuildGroup::DeleteAllControlsCreatedByControlSettingsFile()
{
	for ( int i = 0; i < _panelDar.Count(); i++ )
	{
		if ( _panelDar[i].Get() )
		{
			if ( !_panelDar[i].Get()->IsBuildModeDeletable() )
				continue;
			delete _panelDar[i].Get();
		}
		_panelDar.Remove( i );
		i--;
	}

	_currentPanel = m_pBuildContext;
	m_pBuildContext->InvalidateLayout( false, false );
	m_pBuildContext->Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: check to see if any buildgroup panels have this fieldname
// Input  : fieldName, name to check
// Output : ptr to a panel that has the name if it is taken
//-----------------------------------------------------------------------------
Panel* BuildGroup::FieldNameTaken( const char* fieldName )
{
	for ( int i = 0; i < _panelDar.Count(); i++ )
	{
		Panel* panel = _panelDar[i].Get();
		if ( panel && panel->GetName() && !_stricmp( panel->GetName(), fieldName ) )
			return panel;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get a new unique fieldname for a new control
//-----------------------------------------------------------------------------
void BuildGroup::GetNewFieldName( char* newFieldName, Panel* newPanel )
{
	char defaultName[25] = "Unnamed dialog";
	strcpy( defaultName, newPanel->GetClassName() );

	int axisPos = 1;
	while ( true )
	{
		sprintf( newFieldName, "%s%d", defaultName, axisPos );
		if ( !FieldNameTaken( newFieldName ) )
			return;
		axisPos++;
	}
}

int BuildGroup::GetRegisteredControlSettingsFileCount()
{
	return m_RegisteredControlSettingsFiles.Count();
}

const char* BuildGroup::GetRegisteredControlSettingsFileByIndex( int index )
{
	return m_RegisteredControlSettingsFiles[index].String();
}

void BuildGroup::RegisterControlSettingsFile( const char* resourceName, const char* )
{
	CUtlSymbol sym( resourceName );
	for ( int i = 0; i < m_RegisteredControlSettingsFiles.Count(); i++ )
	{
		if ( m_RegisteredControlSettingsFiles[i] == sym )
			return;
	}
	m_RegisteredControlSettingsFiles.AddToTail( sym );
}

//-----------------------------------------------------------------------------
// Purpose: Create a new control in the context panel
// Input:	controlKeys: keyvalues of settings for the panel only works when applying initial settings.
// Output:	Panel *newPanel, NULL if failed to create new control.
//-----------------------------------------------------------------------------
Panel* BuildGroup::NewControl( KeyValues* controlKeys, int x, int y )
{
	if ( !controlKeys )
		return NULL;

	KeyValues* keyVal = new KeyValues( "ControlFactory", "ControlName", controlKeys->GetString( "ControlName", "" ) );
	m_pBuildContext->RequestInfo( keyVal );
	// returns NULL on failure
	Panel* newPanel = (Panel*)keyVal->GetPtr( "PanelPtr", NULL );
	keyVal->deleteThis();

	if ( !newPanel )
		return NULL;

	// panel successfully created
	newPanel->SetParent( m_pParentPanel );
	newPanel->SetBuildGroup( this );
	newPanel->SetPos( x, y );
	newPanel->SetName( controlKeys->GetName() ); // name before applysettings :)
	newPanel->ApplySettings( controlKeys );
	newPanel->AddActionSignalTarget( m_pParentPanel );
	newPanel->SetBuildModeEditable( true );
	newPanel->SetBuildModeDeletable( true );
	newPanel->SetAutoDelete( true );
	return newPanel;
}

//-----------------------------------------------------------------------------
// Purpose: Create a new control in the context panel
// Input:	name: class name of control to create
//			controlKeys: keyvalues of settings for the panel.
//			name OR controlKeys should be set, not both.  
//			x,y position relative to base panel
// Output: Panel *newPanel, NULL if failed to create new control.
//-----------------------------------------------------------------------------
Panel* BuildGroup::NewControl( const char* name, int x, int y )
{
	ASSERT_AT( name, 947 );

	Panel* newPanel = ( (EditablePanel*)m_pParentPanel )->CreateControlByName( name );
	if ( newPanel )
	{
		// panel successfully created
		newPanel->SetParent( m_pParentPanel );
		newPanel->SetBuildGroup( this );
		newPanel->SetPos( x, y );

		char newFieldName[256];
		GetNewFieldName( newFieldName, newPanel );
		newPanel->SetName( newFieldName );

		newPanel->AddActionSignalTarget( m_pParentPanel );
		newPanel->SetBuildModeEditable( true );
		newPanel->SetBuildModeDeletable( true );

		// make sure it gets freed
		newPanel->SetAutoDelete( true );
	}
	return newPanel;
}

void BuildGroup::ChangeControlSettingsFile( const char* resourceName )
{
	_controlGroup.RemoveAll();
	_currentPanel = m_pParentPanel;

	EditablePanel* edit = dynamic_cast<EditablePanel*>( m_pParentPanel );
	if ( edit )
		edit->LoadControlSettings( resourceName, m_pResourcePathID );
	else
		LoadControlSettings( resourceName, m_pResourcePathID );

	KeyValues* keyval = new KeyValues( "SetActiveControl" );
	keyval->SetPtr( "PanelPtr", GetCurrentPanel() );
	ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), keyval, 0, 0.0f );
}

KeyValues* BuildGroup::GetDialogVariables()
{
	EditablePanel* edit = dynamic_cast<EditablePanel*>( m_pParentPanel );
	if ( !edit )
		return NULL;
	return edit->GetDialogVariables();
}

//-----------------------------------------------------------------------------
// Purpose: reloads the control settings from file
//-----------------------------------------------------------------------------
void BuildGroup::ReloadControlSettings()
{
	delete m_hBuildDialog.Get();
	m_hBuildDialog.Set( NULL );

	for ( int i = 1; i < _panelDar.Count(); i++ )
	{
		if ( _panelDar[i].Get() )
		{
			if ( !_panelDar[i].Get()->IsBuildModeDeletable() )
				continue;
			delete _panelDar[i].Get();
		}
		_panelDar.Remove( i );
		i--;
	}

	if ( m_pResourceName )
	{
		EditablePanel* edit = dynamic_cast<EditablePanel*>( m_pParentPanel );
		if ( edit )
			edit->LoadControlSettings( m_pResourceName, m_pResourcePathID );
		else
			LoadControlSettings( m_pResourceName, m_pResourcePathID );
	}

	_controlGroup.RemoveAll();
	ActivateBuildDialog();
}

Panel* BuildGroup::CreateBuildDialog()
{
	KeyValues* data = new KeyValues( "BuildDialog" );
	data->SetPtr( "BuildGroupPtr", this );
	if ( !m_pBuildContext->RequestInfo( data ) )
		return NULL;

	Panel* buildDialog = (Panel*)data->GetPtr( "PanelPtr", NULL );

	// initialize the build dialog if found
	if ( buildDialog )
		input()->ReleaseAppModalSurface();
	return buildDialog;
}

//-----------------------------------------------------------------------------
// Purpose: Activates the build mode settings dialog
//-----------------------------------------------------------------------------
void BuildGroup::ActivateBuildDialog()
{
	// create the build mode dialog first time through
	if ( !m_hBuildDialog.Get() )
	{
		m_hBuildDialog.Set( CreateBuildDialog() );
		if ( !m_hBuildDialog.Get() )
			return;
	}

	m_hBuildDialog.Get()->SetVisible( true );

	// send a message to set the initial dialog controls info
	_currentPanel = m_pParentPanel;
	KeyValues* keyval = new KeyValues( "SetActiveControl" );
	keyval->SetPtr( "PanelPtr", GetCurrentPanel() );
	ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), keyval, 0, 0.0f );
}

bool BuildGroup::SaveControlSettings()
{
	if ( !m_pResourceName )
		return false;

	KeyValues* rDat = new KeyValues( m_pResourceName );

	// get the data from our controls
	GetSettings( rDat );

	// save the data out to a file
	bool bSuccess = rDat->SaveToFile( filesystem(), m_pResourceName, m_pResourcePathID );
	if ( !bSuccess )
	{
		MessageBox* dlg = new MessageBox( "BuildMode - Error saving file",
			"Error: Could not save changes.  File is most likely read only.", NULL );
		dlg->DoModal( NULL );
	}

	rDat->deleteThis();
	return bSuccess;
}

void BuildGroup::MouseReleased( MouseCode, Panel* panel )
{
	ASSERT_AT( panel, 426 );

	_dragging = false;
	input()->SetMouseCapture( 0 );
}

void BuildGroup::MouseDoublePressed( MouseCode, Panel* panel )
{
	ASSERT_AT( panel, 437 );
}

HCursor BuildGroup::GetCursor( Panel* panel )
{
	ASSERT_AT( panel, 599 );

	int x, y;
	input()->GetCursorPos( x, y );
	panel->ScreenToLocal( x, y );

	int wide, tall;
	panel->GetSize( wide, tall );

	if ( x >= 2 )
		return _cursor_sizeall;
	if ( y < 4 )
		return _cursor_sizenwse;
	if ( y >= tall - 4 )
		return _cursor_sizenesw;
	return _cursor_sizewe;
}

//-----------------------------------------------------------------------------
// Purpose: Add panel the list of panels that are in the build group
//-----------------------------------------------------------------------------
void BuildGroup::PanelAdded( Panel* panel )
{
	ASSERT_AT( panel, 662 );

	PHandle temp;
	temp.Set( panel );
	_panelDar.AddToTail( temp );
}

//-----------------------------------------------------------------------------
// Purpose: loop though objects in the current control group and remove them all
//-----------------------------------------------------------------------------
void BuildGroup::RemoveSettings()
{
	// loop though objects in the current control group and remove them all
	for ( int i = 0; i < _controlGroup.Count(); i++ )
	{
		// only delete delatable panels
		if ( _controlGroup[i].Get()->IsBuildModeDeletable() )
		{
			delete _controlGroup[i].Get();
			_controlGroup.Remove( i );
			i--;
		}
	}

	// remove deleted panels from the handle list
	for ( int i = 0; i < _panelDar.Count(); i++ )
	{
		if ( !_panelDar[i].Get() )
		{
			_panelDar.Remove( i );
			i--;
		}
	}

	_currentPanel = m_pBuildContext;
	m_pBuildContext->InvalidateLayout( false, false );
	m_pBuildContext->Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Toggles build mode on/off
// Input  : state - new state
//-----------------------------------------------------------------------------
void BuildGroup::SetEnabled( bool state )
{
	if ( _enabled == state )
		return;

	_enabled = state;
	_currentPanel = NULL;

	if ( state )
	{
		ActivateBuildDialog();
	}
	else
	{
		// hide the build dialog
		if ( m_hBuildDialog.Get() )
			m_hBuildDialog.Get()->OnCommand( "Close" );

		// request focus for our main panel
		m_pParentPanel->RequestFocus( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check if buildgroup is enabled
//-----------------------------------------------------------------------------
bool BuildGroup::IsEnabled()
{
	return _enabled;
}

//-----------------------------------------------------------------------------
// Purpose:	Return the currently selected panel
//-----------------------------------------------------------------------------
Panel* BuildGroup::GetCurrentPanel()
{
	return _currentPanel;
}

//-----------------------------------------------------------------------------
// Purpose: sets the panel from which the build group gets all it's object creation info
//-----------------------------------------------------------------------------
void BuildGroup::SetContextPanel( Panel* contextPanel )
{
	m_pBuildContext = contextPanel;
}

//-----------------------------------------------------------------------------
// Purpose: gets the panel from which the build group gets all it's object creation info
//-----------------------------------------------------------------------------
Panel* BuildGroup::GetContextPanel()
{
	return m_pBuildContext;
}

const char* BuildGroup::GetResourceName()
{
	return m_pResourceName;
}

//-----------------------------------------------------------------------------
// Purpose: Get the list of panels that are currently selected
//-----------------------------------------------------------------------------
CUtlVector<PHandle>* BuildGroup::GetControlGroup()
{
	return &_controlGroup;
}

//-----------------------------------------------------------------------------
// Purpose: get the list of panels in the buildgroup
//-----------------------------------------------------------------------------
CUtlVector<PHandle>* BuildGroup::GetPanelList()
{
	return &_panelDar;
}

//-----------------------------------------------------------------------------
// Purpose:	Check if ruler display is activated
//-----------------------------------------------------------------------------
bool BuildGroup::HasRulersOn()
{
	return _showRulers;
}

void BuildGroup::ApplySchemeSettings( IScheme* )
{
	DrawRulers();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void BuildGroup::ApplySnap( Panel* panel )
{
	ASSERT_AT( panel, 631 );

	int x, y, wide, tall;
	panel->GetBounds( x, y, wide, tall );

	x = ( x / _snapX ) * _snapX;
	y = ( y / _snapY ) * _snapY;
	panel->SetPos( x, y );

	int xx = x + wide;
	int yy = y + tall;
	yy = yy - yy % _snapY;
	xx = xx - xx % _snapX;
	panel->SetSize( xx - x, yy - y );
}

//-----------------------------------------------------------------------------
// Purpose: respond to cursor movments
//-----------------------------------------------------------------------------
void BuildGroup::CursorMoved( int x, int y, Panel* panel )
{
	ASSERT_AT( panel, 257 );

	if ( !_dragging )
		return;

	input()->GetCursorPos( x, y );

	if ( _dragMouseCode == MOUSE_RIGHT )
	{
		panel->SetSize( x - _dragStartCursorPos[0], y - _dragStartCursorPos[1] );
		ApplySnap( panel );
	}
	else
	{
		for ( int i = 0; i < _controlGroup.Count(); i++ )
		{
			// now fix offset of member panels with respect to the one we are dragging
			Panel* groupMember = _controlGroup[i].Get();
			groupMember->SetPos( x + _dragStartPanelPos[0] + _groupDeltaX[i] - _dragStartCursorPos[0],
				y + _dragStartPanelPos[1] + _groupDeltaY[i] - _dragStartCursorPos[1] );
			ApplySnap( groupMember );
		}
	}

	// update the build dialog
	if ( m_hBuildDialog.Get() )
	{
		KeyValues* keyval = new KeyValues( "UpdateControlData" );
		keyval->SetPtr( "panel", GetCurrentPanel() );
		ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), keyval, 0, 0.0f );

		ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), new KeyValues( "EnableSaveButton" ), 0, 0.0f );
	}

	panel->Repaint();
	panel->CallParentFunction( new KeyValues( "Repaint" ) );
}

void BuildGroup::KeyCodeTyped( KeyCode code, Panel* panel )
{
	ASSERT_AT( panel, 446 );

	int dx = 0;
	int dy = 0;

	bool shift = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
	bool ctrl = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );

	switch ( code )
	{
	case KEY_DELETE:
		// delete the panel we have selected
		ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), new KeyValues( "DeletePanel" ), 0, 0.0f );
		break;
	case KEY_UP:
		dy = -_snapY;
		break;
	case KEY_LEFT:
		dx = -_snapX;
		break;
	case KEY_DOWN:
		dy = _snapY;
		break;
	case KEY_RIGHT:
		dx = _snapX;
		break;
	default:
		break;
	}

	if ( ctrl )
	{
		const char* command = NULL;
		switch ( code )
		{
		case KEY_C:
			command = "Copy";
			break;
		case KEY_V:
			command = "Paste";
			break;
		case KEY_Z:
			command = "Undo";
			break;
		default:
			break;
		}

		if ( command )
			ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), new KeyValues( command ), 0, 0.0f );
	}

	if ( !dx && !dy )
		return;
	//TODO: make this stuff actually snap

	int x, y, wide, tall;
	panel->GetBounds( x, y, wide, tall );

	if ( shift )
		panel->SetSize( wide + dx, tall + dy );
	else
		panel->SetPos( x + dx, y + dy );

	ApplySnap( panel );
	panel->Repaint();

	if ( panel->GetVParent() )
		panel->PostMessage( panel->GetVParent(), new KeyValues( "Repaint" ), 0.0f );

	// update the build dialog
	if ( m_hBuildDialog.Get() )
	{
		// post that it's active
		KeyValues* keyval = new KeyValues( "SetActiveControl" );
		keyval->SetPtr( "PanelPtr", GetCurrentPanel() );
		ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), keyval, 0, 0.0f );

		ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), new KeyValues( "PanelMoved" ), 0, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BuildGroup::MousePressed( MouseCode code, Panel* panel )
{
	ASSERT_AT( panel, 306 );

	if ( panel == m_hBuildDialog.Get() )
	{
		ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), new KeyValues( "HideNewControlMenu" ), 0, 0.0f );
		return;
	}

	if ( !panel->GetName()[0] )
		return;

	if ( !input()->IsKeyDown( KEY_LSHIFT ) && !input()->IsKeyDown( KEY_RSHIFT ) )
		_controlGroup.RemoveAll();

	if ( code == MOUSE_RIGHT )
	{
		ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), new KeyValues( "ShowNewControlMenu" ), 0, 0.0f );
		return;
	}
	if ( code != MOUSE_LEFT )
		return;

	// don't respond if we click on ruler numbers
	if ( _showRulers ) // rulers are visible
	{
		for ( int i = 0; i < 4; i++ )
		{
			if ( panel == _rulerNumber[i] )
				return;
		}
	}

	_dragging = true;
	_dragMouseCode = MOUSE_LEFT;
	ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), new KeyValues( "HideNewControlMenu" ), 0, 0.0f );

	int x, y;
	input()->GetCursorPos( x, y );
	_dragStartCursorPos[0] = x;
	_dragStartCursorPos[1] = y;
	input()->SetMouseCapture( panel->GetVPanel() );

	_groupDeltaX.RemoveAll();
	_groupDeltaY.RemoveAll();

	int i;
	for ( i = 0; i < _controlGroup.Count(); i++ )
	{
		if ( panel == _controlGroup[i].Get() )
			break;
	}
	if ( i == _controlGroup.Count() )
	{
		PHandle temp;
		temp.Set( panel );
		_controlGroup.AddToTail( temp );
	}

	panel->GetPos( x, y );
	_dragStartPanelPos[0] = x;
	_dragStartPanelPos[1] = y;

	// figure out the deltas of the other panels from the base panel
	for ( i = 0; i < _controlGroup.Count(); i++ )
	{
		int cx, cy;
		_controlGroup[i].Get()->GetPos( cx, cy );
		_groupDeltaX.AddToTail( cx - x );
		_groupDeltaY.AddToTail( cy - y );
	}

	// if this panel wasn't already selected update the buildmode dialog controls to show its info
	if ( _currentPanel != panel )
	{
		_currentPanel = panel;
		if ( m_hBuildDialog.Get() )
		{
			// think this is taken care of by SetActiveControl.
			//ivgui()->PostMessage(m_hBuildDialog->GetVPanel(), new KeyValues("ApplyDataToControls"), NULL);

			KeyValues* keyval = new KeyValues( "SetActiveControl" );
			keyval->SetPtr( "PanelPtr", GetCurrentPanel() );
			ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), keyval, 0, 0.0f );
		}
	}

	ivgui()->PostMessage( m_hBuildDialog.Get()->GetVPanel(), new KeyValues( "StoreUndo" ), 0, 0.0f );

	panel->RequestFocus( 0 );
}

void BuildGroup::GetSettings( KeyValues* resourceData )
{
	// loop through all the objects getting their settings
	for ( int i = 0; i < _panelDar.Count(); i++ )
	{
		Panel* panel = _panelDar[i].Get();
		if ( !panel )
			continue;

		if ( _showRulers
			&& ( panel == _rulerNumber[0] || panel == _rulerNumber[1] || panel == _rulerNumber[2]
				|| panel == _rulerNumber[3] ) )
			continue;

		if ( _stricmp( panel->GetName(), "BuildDialog" ) && panel->GetName() && panel->GetName()[0] )
			panel->GetSettings( resourceData->FindKey( panel->GetName(), true ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: serializes settings from a resource data container
//-----------------------------------------------------------------------------
void BuildGroup::ApplySettings( KeyValues* resourceData )
{
	// loop through all the keys, applying them wherever
	for ( KeyValues* controlKeys = resourceData->GetFirstSubKey(); controlKeys; controlKeys = controlKeys->GetNextKey() )
	{
		if ( controlKeys->GetDataType( NULL ) != KeyValues::TYPE_NONE )
			continue;

		bool bFound = false;
		for ( int i = 0; i < _panelDar.Count(); i++ )
		{
			Panel* panel = _panelDar[i].Get();
			if ( !panel ) // this can happen if we had two of the same handle in the list
			{
				_panelDar.Remove( i );
				i--;
				continue;
			}

			if ( !_stricmp( panel->GetName(), controlKeys->GetName() ) )
			{
				// apply the settings
				panel->ApplySettings( controlKeys );
				bFound = true;
				break;
			}
		}

		if ( bFound || !controlKeys->GetName() )
			continue;

		KeyValues* keyVal = new KeyValues( "ControlFactory", "ControlName", controlKeys->GetString( "ControlName", "" ) );
		m_pBuildContext->RequestInfo( keyVal );
		Panel* newPanel = (Panel*)keyVal->GetPtr( "PanelPtr", NULL );
		keyVal->deleteThis();

		if ( newPanel )
		{
			newPanel->SetParent( m_pParentPanel );
			newPanel->SetBuildGroup( this );
			newPanel->SetPos( 0, 0 );
			newPanel->SetName( controlKeys->GetName() );
			newPanel->ApplySettings( controlKeys );
			newPanel->AddActionSignalTarget( m_pParentPanel );
			newPanel->SetBuildModeEditable( true );
			newPanel->SetBuildModeDeletable( true );
			newPanel->SetAutoDelete( true );
		}
	}
}

void BuildGroup::SetRulerLabelsVisible( bool state )
{
	for ( int i = 0; i < 4; i++ )
		_rulerNumber[i]->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose:	Toggle ruler display 
//-----------------------------------------------------------------------------
void BuildGroup::ToggleRulerDisplay()
{
	_showRulers = !_showRulers;

	if ( !_rulerNumber[0] ) // rulers haven't been initialized
	{
		for ( int i = 0; i < 4; i++ )
			_rulerNumber[i] = new Label( m_pBuildContext, NULL, "" );
	}

	SetRulerLabelsVisible( _showRulers );
	m_pBuildContext->Repaint();
}
}
