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

#include "EditablePanel.h"
#include "AnimatingImagePanel.h"
#include "BitmapImagePanel.h"
#include "BuildGroup.h"
#include "BuildModeDialog.h"
#include "Button.h"
#include "CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "Divider.h"
#include "ImagePanel.h"
#include "Label.h"
#include "Menu.h"
#include "MenuItem.h"
#include "MessageBox.h"
#include "ProgressBar.h"
#include "RadioButton.h"
#include "RichText.h"
#include "ScrollBar.h"
#include "TextEntry.h"
#include "ToggleButton.h"
#include "URLLabel.h"
#include "vgui/IPanel.h"
#include "vgui/KeyValues.h"
#include "vgui/MessageMap.h"
#include "vgui/ISystem.h"
#include "controls.h"

#include <cstring>

namespace vgui2
{
EditablePanel::EditablePanel( Panel* parent, const char* name ) : Panel( parent, name ), m_NavGroup( this )
{
	_buildGroup = new BuildGroup( this, this );
	m_pszConfigName = NULL;
	m_iConfigID = 0;
	m_pDialogVariables = NULL;

	GetSize( _baseWide, _baseTall );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
EditablePanel::~EditablePanel()
{
	delete[] m_pszConfigName;
	delete _buildGroup;

	if ( m_pDialogVariables )
		m_pDialogVariables->deleteThis();
}

void EditablePanel::LoadUserConfig( const char* configName, int configID )
{
	KeyValues* data = system()->GetUserConfigFileData( configName, configID );

	delete[] m_pszConfigName;
	m_pszConfigName = new char[strlen( configName ) + 1];
	strcpy( m_pszConfigName, configName );
	m_iConfigID = configID;

	if ( data )
		ApplyUserConfigSettings( data );
}

//-----------------------------------------------------------------------------
// Purpose: saves all the settings to the document
//-----------------------------------------------------------------------------
void EditablePanel::SaveUserConfig()
{
	if ( !m_pszConfigName )
		return;

	KeyValues* data = system()->GetUserConfigFileData( m_pszConfigName, m_iConfigID );
	if ( data )
		GetUserConfigSettings( data );
}

void EditablePanel::ApplyUserConfigSettings( KeyValues* userConfig )
{
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel* child = GetChild( i );
		if ( !child->HasUserConfigSettings() )
			continue;

		const char* name = child->GetName();
		if ( name && *name )
			child->ApplyUserConfigSettings( userConfig->FindKey( name, true ) );
	}
}

void EditablePanel::GetUserConfigSettings( KeyValues* userConfig )
{
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel* child = GetChild( i );
		if ( !child->HasUserConfigSettings() )
			continue;

		const char* name = child->GetName();
		if ( name && *name )
			child->GetUserConfigSettings( userConfig->FindKey( name, true ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Loads panel settings from a resource file.
//-----------------------------------------------------------------------------
void EditablePanel::LoadControlSettings( const char* resourceName, const char* pathID )
{
	_buildGroup->LoadControlSettings( resourceName, pathID );
	ForceSubPanelsToUpdateWithNewDialogVariables();
	InvalidateLayout( false, false );
}

void EditablePanel::LoadControlSettingsAndUserConfig( const char* resourceName, int configID )
{
	LoadControlSettings( resourceName, NULL );
	LoadUserConfig( resourceName, configID );
}

//-----------------------------------------------------------------------------
// Purpose: Activates the build mode dialog for editing panels.
//-----------------------------------------------------------------------------
void EditablePanel::ActivateBuildMode()
{
	_buildGroup->SetEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: Return the buildgroup that this panel is part of.
// Input  : 
// Output : BuildGroup
//-----------------------------------------------------------------------------
BuildGroup* EditablePanel::GetBuildGroup()
{
	return _buildGroup;
}

//-----------------------------------------------------------------------------
// Purpose: Save user config settings
//-----------------------------------------------------------------------------
void EditablePanel::OnClose()
{
	SaveUserConfig();
}

KeyValues* EditablePanel::GetDialogVariables()
{
	if ( !m_pDialogVariables )
		m_pDialogVariables = new KeyValues( "DialogVariables" );
	return m_pDialogVariables;
}

void EditablePanel::SetControlString( const char* controlName, const char* string )
{
	Panel* control = FindChildByName( controlName, false );
	if ( control )
		PostMessage( control, new KeyValues( "SetText", "text", string ), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Shortcut function to set data in child controls
//-----------------------------------------------------------------------------
void EditablePanel::SetControlInt( const char* controlName, int state )
{
	Panel* control = FindChildByName( controlName, false );
	if ( control )
		PostMessage( control, new KeyValues( "SetState", "state", state ), 0.0f );
}

void EditablePanel::SetControlEnabled( const char* controlName, bool enabled )
{
	Panel* control = FindChildByName( controlName, false );
	if ( control )
		control->SetEnabled( enabled );
}

void EditablePanel::SetControlVisible( const char* controlName, bool visible )
{
	Panel* control = FindChildByName( controlName, false );
	if ( control )
		control->SetVisible( visible );
}

int EditablePanel::GetControlInt( const char* controlName, int defaultState )
{
	Panel* control = FindChildByName( controlName, false );
	if ( !control )
		return defaultState;

	KeyValues* data = new KeyValues( "GetState" );
	if ( !control->RequestInfo( data ) )
		return defaultState;

	int state = data->GetInt( "state", defaultState );
	data->deleteThis();
	return state;
}

//-----------------------------------------------------------------------------
// Purpose: Shortcut function to get data in child controls
//-----------------------------------------------------------------------------
void EditablePanel::GetControlString( const char* controlName, char* buf, int bufSize, const char* defaultString )
{
	Panel* control = FindChildByName( controlName, false );
	KeyValues* data = new KeyValues( "GetText" );

	const char* text = defaultString;
	if ( control && control->RequestInfo( data ) )
		text = data->GetString( "text", defaultString );

	strncpy( buf, text, bufSize - 1 );

	// ensure null termination of string
	buf[bufSize - 1] = 0;

	// free
	data->deleteThis();
}

void EditablePanel::SetDialogVariable( const char* varName, const char* value )
{
	GetDialogVariables()->SetString( varName, value );
	ForceSubPanelsToUpdateWithNewDialogVariables();
}

void EditablePanel::SetDialogVariable( const char* varName, const wchar_t* value )
{
	GetDialogVariables()->SetWString( varName, value );
	ForceSubPanelsToUpdateWithNewDialogVariables();
}

void EditablePanel::SetDialogVariable( const char* varName, int value )
{
	GetDialogVariables()->SetInt( varName, value );
	ForceSubPanelsToUpdateWithNewDialogVariables();
}

void EditablePanel::SetDialogVariable( const char* varName, float value )
{
	GetDialogVariables()->SetFloat( varName, value );
	ForceSubPanelsToUpdateWithNewDialogVariables();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *defaultButton - 
//-----------------------------------------------------------------------------
void EditablePanel::OnDefaultButtonSet( Panel* defaultButton )
{
	m_NavGroup.SetDefaultButton( defaultButton );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *defaultButton - 
//-----------------------------------------------------------------------------
void EditablePanel::OnCurrentDefaultButtonSet( Panel* defaultButton )
{
	m_NavGroup.SetCurrentDefaultButton( defaultButton->GetVPanel(), false );

	// forward the message up
	if ( GetVParent() )
	{
		KeyValues* msg = new KeyValues( "CurrentDefaultButtonSet" );
		msg->SetPtr( "button", defaultButton );
		PostMessage( GetVParent(), msg, 0.0f );
	}
}

void EditablePanel::OnFindDefaultButton()
{
	if ( m_NavGroup.GetDefaultButton() )
		m_NavGroup.SetCurrentDefaultButton( m_NavGroup.GetDefaultButton(), true );
	else if ( GetVParent() )
		PostMessage( GetVParent(), new KeyValues( "FindDefaultButton" ), 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: Return a pointer to the nav group
// Output : FocusNavGroup
//-----------------------------------------------------------------------------
FocusNavGroup& EditablePanel::GetFocusNavGroup()
{
	return m_NavGroup;
}

void EditablePanel::RegisterControlSettingsFile( const char* resourceName, const char* pathID )
{
	_buildGroup->RegisterControlSettingsFile( resourceName, pathID );
}

void EditablePanel::ForceSubPanelsToUpdateWithNewDialogVariables()
{
	if ( !m_pDialogVariables )
		return;

	ipanel()->SendMessage( GetVPanel(), m_pDialogVariables, GetVPanel() );
	for ( int i = 0; i < ipanel()->GetChildCount( GetVPanel() ); i++ )
		ipanel()->SendMessage( ipanel()->GetChild( GetVPanel(), i ), m_pDialogVariables, GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: Shortcut function to get data in child controls
//-----------------------------------------------------------------------------
const char* EditablePanel::GetControlString( const char* controlName, const char* defaultString )
{
	static char buf[512];
	GetControlString( controlName, buf, sizeof( buf ) - 1, defaultString );
	return buf;
}

EditablePanel::EditablePanel( Panel* parent, const char* name, HScheme scheme )
	: Panel( parent, name, scheme ), m_NavGroup( this )
{
	_buildGroup = new BuildGroup( this, this );
	m_pszConfigName = NULL;
	m_iConfigID = 0;
	m_pDialogVariables = NULL;
	GetSize( _baseWide, _baseTall );
}

Panel* EditablePanel::CreateControlByName( const char* controlName )
{
	if ( !_stricmp( controlName, "Label" ) )
		return new Label( NULL, NULL, "Label" );
	if ( !_stricmp( controlName, "Button" ) )
		return new Button( NULL, NULL, "Button" );
	if ( !_stricmp( controlName, "CheckButton" ) )
		return new CheckButton( NULL, NULL, "CheckButton" );
	if ( !_stricmp( controlName, "ComboBox" ) )
		return new ComboBox( NULL, NULL, 5, true );
	if ( !_stricmp( controlName, "Menu" ) )
		return new Menu( NULL, "Menu" );
	if ( !_stricmp( controlName, "MenuItem" ) )
		return new MenuItem( NULL, NULL, "MenuItem", NULL, false );
	if ( !_stricmp( controlName, "MessageBox" ) )
		return new MessageBox( "MessageBox", "MessageBoxText", NULL );
	if ( !_stricmp( controlName, "ProgressBar" ) )
		return new ProgressBar( NULL, NULL );
	if ( !_stricmp( controlName, "RadioButton" ) )
		return new RadioButton( NULL, NULL, "RadioButton" );
	if ( !_stricmp( controlName, "ScrollBar" ) )
		return new ScrollBar( NULL, NULL, false );
	if ( !_stricmp( controlName, "TextEntry" ) )
		return new TextEntry( NULL, NULL );
	if ( !_stricmp( controlName, "RichText" ) )
		return new RichText( NULL, NULL, true );
	if ( !_stricmp( controlName, "ToggleButton" ) )
		return new ToggleButton( NULL, NULL, "ToggleButton" );
	if ( !_stricmp( controlName, "ImagePanel" ) )
		return new ImagePanel( NULL, "ResourceImagePanel" );
	if ( !_stricmp( controlName, "AnimatingImagePanel" ) )
		return new AnimatingImagePanel( NULL, NULL );
	if ( !_stricmp( controlName, "Panel" ) )
		return new Panel( NULL, NULL );
	if ( !_stricmp( controlName, "Divider" ) )
		return new Divider( NULL, "Divider" );
	if ( !_stricmp( controlName, "URLLabel" ) )
		return new URLLabel( NULL, NULL, "URLLabel", NULL );
	if ( !_stricmp( controlName, "EditablePanel" ) )
		return new EditablePanel( NULL, NULL );
	if ( !_stricmp( controlName, "BitmapImagePanel" ) )
		return new CBitmapImagePanel( NULL, "BitmapImagePanel", NULL );
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the panel with the specified hotkey
//-----------------------------------------------------------------------------
Panel* EditablePanel::HasHotkey( wchar_t key )
{
	if ( !IsVisible() || !IsEnabled() ) // not visible, so can't respond to a hot key 
		return NULL;

	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel* hot = GetChild( i )->HasHotkey( key );
		if ( hot && hot->IsVisible() && hot->IsEnabled() )
			return hot;
	}
	return NULL;
}

void EditablePanel::RequestFocus( int direction )
{
	// we must be a sub panel for this to be called
	// delegate focus
	if ( direction == 1 )
		RequestFocusNext( 0 );
	else if ( direction == -1 )
		RequestFocusPrev( 0 );
	else
		Panel::RequestFocus( 0 );
}

bool EditablePanel::RequestFocusNext( VPANEL panel )
{
	return m_NavGroup.RequestFocusNext( panel );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
bool EditablePanel::RequestFocusPrev( VPANEL panel )
{
	return m_NavGroup.RequestFocusPrev( panel );
}

//-----------------------------------------------------------------------------
// Purpose: Pass the focus down onto the last used panel
//-----------------------------------------------------------------------------
void EditablePanel::OnSetFocus()
{
	Panel* focus = m_NavGroup.GetCurrentFocus();
	if ( focus && focus != this )
	{
		focus->RequestFocus( 0 );
	}
	else
	{
		focus = m_NavGroup.GetDefaultPanel();
		if ( focus )
		{
			focus->RequestFocus( 0 );
			focus->OnSetFocus();
		}
	}

	Panel::OnKillFocus();
}

void EditablePanel::OnRequestFocus( VPANEL subFocus, VPANEL defaultPanel )
{
	if ( !ipanel()->IsPopup( subFocus ) )
		defaultPanel = m_NavGroup.SetCurrentFocus( subFocus, defaultPanel );
	Panel::OnRequestFocus( GetVPanel(), defaultPanel );
}

VPANEL EditablePanel::GetCurrentKeyFocus()
{
	Panel* focus = m_NavGroup.GetCurrentFocus();
	if ( focus == this )
		return 0;

	if ( !focus || focus->IsPopup() )
		return Panel::GetCurrentKeyFocus();

	// chain down the editpanel hierarchy
	VPANEL subFocus = focus->GetCurrentKeyFocus();
	if ( !subFocus )
		return focus->GetVPanel();
	return subFocus;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void EditablePanel::OnKeyCodeTyped( KeyCode code )
{
	VPANEL panel = GetFocusNavGroup().GetCurrentDefaultButton();
	if ( ( code == KEY_ENTER || code == KEY_PAD_ENTER ) && panel && ipanel()->IsVisible( panel )
		&& ipanel()->IsEnabled( panel ) )
	{
		// Activate the button
		PostMessage( panel, new KeyValues( "Hotkey" ), 0.0f );
	}
	else
	{
		Panel::OnKeyCodeTyped( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle information requests
//-----------------------------------------------------------------------------
bool EditablePanel::RequestInfo( KeyValues* data )
{
	if ( !_stricmp( data->GetName(), "BuildDialog" ) )
	{
		// a build dialog is being requested, give it one
		BuildModeDialog* dlg = new BuildModeDialog( (BuildGroup*)data->GetPtr( "BuildGroupPtr", NULL ) );
		data->SetPtr( "PanelPtr", dlg );
		return true;
	}

	if ( !_stricmp( data->GetName(), "ControlFactory" ) )
	{
		Panel* newPanel = CreateControlByName( data->GetString( "ControlName", "" ) );
		if ( newPanel )
		{
			data->SetPtr( "PanelPtr", newPanel );
			return true;
		}
	}

	return Panel::RequestInfo( data );
}

void EditablePanel::PaintBackground()
{
	Panel::PaintBackground();
}

//-----------------------------------------------------------------------------
// Purpose: Called when a child is added to the panel.
//-----------------------------------------------------------------------------
void EditablePanel::OnChildAdded( VPANEL child )
{
	Panel::OnChildAdded( child );

	Panel* panel = ipanel()->GetPanel( child, GetModuleName() );
	if ( panel )
	{
		panel->SetBuildGroup( _buildGroup );
		panel->AddActionSignalTarget( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Callback for when the panel size has been changed
//-----------------------------------------------------------------------------
void EditablePanel::OnSizeChanged( int wide, int tall )
{
	Panel::OnSizeChanged( wide, tall );
	InvalidateLayout( false, false );

	int dx = wide - _baseWide;
	int dy = tall - _baseTall;

	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel* child = GetChild( i );
		if ( !child || child->IsPopup() )
			continue;

		int x, y, w, t;
		child->GetBounds( x, y, w, t );

		if ( child->GetPinCorner() == PIN_TOPRIGHT || child->GetPinCorner() == PIN_BOTTOMRIGHT )
			x += dx;
		if ( child->GetPinCorner() == PIN_BOTTOMLEFT || child->GetPinCorner() == PIN_BOTTOMRIGHT )
			y += dy;

		// check for resize
		if ( child->GetAutoResize() == AUTORESIZE_RIGHT || child->GetAutoResize() == AUTORESIZE_DOWNANDRIGHT )
			w += dx;
		if ( child->GetAutoResize() == AUTORESIZE_DOWN || child->GetAutoResize() == AUTORESIZE_DOWNANDRIGHT )
			t += dy;

		// make sure the child isn't too big...
		if ( x + w > wide )
		{
			w = wide - x;
			if ( w < 0 )
				continue;
		}
		if ( y + t > tall )
		{
			t = tall - y;
			if ( t < 0 )
				continue;
		}

		child->SetBounds( x, y, w, t );
		child->InvalidateLayout( false, false );
	}

	Repaint();

	// update the baselines
	_baseWide = wide;
	_baseTall = tall;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the resource file is loaded to set up the panel state
// Input  : *inResourceData - 
//-----------------------------------------------------------------------------
void EditablePanel::ApplySettings( KeyValues* inResourceData )
{
	Panel::ApplySettings( inResourceData );
	_buildGroup->ApplySettings( inResourceData );

	// reset the auto-resize baselines
	GetSize( _baseWide, _baseTall );
}
}

struct leaf_t
{
	short x;
	short y;
	short wide;
	short tall;
	unsigned char split;	// 0 no split; 1 x-axis, 2 y-axis
	bool filled;			// true if this is already filled
	short splitpos;			// place of split
	leaf_t* left;
	leaf_t* right;
};

#define SPLIT_X		1
#define SPLIT_Y		2

static int g_iNextLeaf;
static leaf_t g_Leaves[256];

void AddSolidToTree( leaf_t* leaf, int x, int y, int wide, int tall )
{
	while ( true )
	{
		int leafX = leaf->x;
		int leafY = leaf->y;

		// clip to this leaf
		if ( x < leafX )
		{
			wide -= leafX - x;
			if ( wide <= 0 )
				return;
			x = leafX;
		}
		if ( y < leafY )
		{
			tall -= leafY - y;
			if ( tall <= 0 )
				return;
			y = leafY;
		}

		int leafRight = leafX + leaf->wide;
		if ( x + wide > leafRight )
		{
			wide = leafRight - x;
			if ( wide <= 0 )
				return;
		}

		int leafBottom = leafY + leaf->tall;
		if ( y + tall > leafBottom )
		{
			tall = leafBottom - y;
			if ( tall <= 0 )
				return;
		}

		// the rect should now be completely within the leaf
		if ( leaf->split == SPLIT_X )
		{
			// see if it is to the left or the right of the split
			if ( x < leaf->splitpos )
				leaf = leaf->left;
			else if ( x + wide > leaf->splitpos )
				leaf = leaf->right;
			else
				return;
			continue;
		}

		if ( leaf->split == SPLIT_Y )
		{
			// check y
			// see if it is to the left (above) or the right (below) of the split
			if ( y < leaf->splitpos )
				leaf = leaf->left;
			else if ( y + tall > leaf->splitpos )
				leaf = leaf->right;
			else
				return;
			continue;
		}

		unsigned char split;
		int splitpos;
		bool goRight;
		if ( x > leafX )
		{
			split = SPLIT_X;
			splitpos = x;
			goRight = true;
		}
		else if ( y > leafY )
		{
			split = SPLIT_Y;
			splitpos = y;
			goRight = true;
		}
		else if ( x + wide < leafRight )
		{
			split = SPLIT_X;
			splitpos = x + wide;
			goRight = false;
		}
		else if ( y + tall < leafBottom )
		{
			split = SPLIT_Y;
			splitpos = y + tall;
			goRight = false;
		}
		else
		{
			// this is the exact same rect! don't draw this leaf
			leaf->filled = true;
			return;
		}

		leaf->split = split;
		leaf->splitpos = (short)splitpos;

		leaf_t* left = &g_Leaves[g_iNextLeaf];
		leaf_t* right = &g_Leaves[g_iNextLeaf + 1];
		g_iNextLeaf += 2;

		*left = {};
		left->x = leaf->x;
		left->y = leaf->y;
		left->wide = leaf->wide;
		left->tall = leaf->tall;
		*right = *left;

		if ( split == SPLIT_X )
		{
			left->wide = (short)( leaf->splitpos - leaf->x );
			right->x = leaf->splitpos;
			right->wide = (short)( leaf->wide - left->wide );
		}
		else
		{
			left->tall = (short)( leaf->splitpos - leaf->y );
			right->y = leaf->splitpos;
			right->tall = (short)( leaf->tall - left->tall );
		}

		leaf->left = left;
		leaf->right = right;
		leaf = goRight ? right : left;
	}
}
