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

#include "FocusNavGroup.h"
#include "Panel.h"
#include "controls.h"

#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"

namespace vgui2
{
FocusNavGroup::FocusNavGroup( Panel* mainPanel )
{
	_mainPanel = mainPanel;
	_currentFocus.Set( 0 );
	_topLevelFocus = false;
	_defaultButton.Set( 0 );
	_currentDefaultButton.Set( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
FocusNavGroup::~FocusNavGroup()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Panel* FocusNavGroup::GetDefaultPanel()
{
	for ( int i = 0; i < _mainPanel->GetChildCount(); i++ )
	{
		Panel* child = _mainPanel->GetChild( i );
		if ( child && child->GetTabPosition() == 1 )
			return child;
	}

	return NULL;	// no specific panel set
}

VPANEL FocusNavGroup::GetDefaultButton()
{
	return _defaultButton.Get();
}

VPANEL FocusNavGroup::GetCurrentDefaultButton()
{
	return _currentDefaultButton.Get();
}

Panel* FocusNavGroup::GetCurrentFocus()
{
	if ( !_currentFocus.Get() )
		return NULL;

	return ipanel()->GetPanel( _currentFocus.Get(), GetControlsModuleName() );
}

void FocusNavGroup::SetDefaultButton( Panel* panel )
{
	if ( !panel && !_defaultButton.Get() )
		return;

	if ( panel->GetVPanel() == _defaultButton.Get() )
		return;

	_defaultButton.Set( panel->GetVPanel() );
	SetCurrentDefaultButton( _defaultButton.Get(), true );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the specified panel can be the default
//-----------------------------------------------------------------------------
bool FocusNavGroup::CanButtonBeDefault( VPANEL panel )
{
	KeyValues* data = new KeyValues( "CanBeDefaultButton" );

	bool result = false;
	if ( ipanel()->RequestInfo( panel, data ) )
		result = data->GetInt( "result", 0 ) == 1;

	data->deleteThis();
	return result;
}

VPANEL FocusNavGroup::SetCurrentFocus( VPANEL focus, VPANEL defaultPanel )
{
	_currentFocus.Set( focus );

	if ( !defaultPanel )
	{
		// can this focus itself by the default
		if ( CanButtonBeDefault( focus ) )
			defaultPanel = focus;
		else if ( _defaultButton.Get() )
			defaultPanel = _defaultButton.Get();
	}

	SetCurrentDefaultButton( defaultPanel, true );
	return defaultPanel;
}

//-----------------------------------------------------------------------------
// Purpose: sets the panel that owns this FocusNavGroup to be the root in the focus traversal heirarchy
//-----------------------------------------------------------------------------
void FocusNavGroup::SetFocusTopLevel( bool state )
{
	_topLevelFocus = state;
}

void FocusNavGroup::SetCurrentDefaultButton( VPANEL panel, bool sendMessage )
{
	if ( panel == _currentDefaultButton.Get() )
		return;

	if ( sendMessage && _currentDefaultButton.Get() )
	{
		KeyValues* msg = new KeyValues( "SetAsCurrentDefaultButton", "state", 0 );
		ivgui()->PostMessage( _currentDefaultButton.Get(), msg, 0, 0.0f );
	}

	_currentDefaultButton.Set( panel );

	if ( sendMessage && _currentDefaultButton.Get() )
	{
		KeyValues* msg = new KeyValues( "SetAsCurrentDefaultButton", "state", 1 );
		ivgui()->PostMessage( _currentDefaultButton.Get(), msg, 0, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the focus to the previous panel in the tab order
// Input  : *panel - panel currently with focus
//-----------------------------------------------------------------------------
bool FocusNavGroup::RequestFocusPrev( VPANEL panel )
{
	if ( !panel )
		return false;

	_currentFocus.Set( 0 );

	int newPosition = ipanel()->GetTabPosition( panel );

	bool found = false;
	bool repeat = true;
	Panel* best = NULL;
	while ( 1 )
	{
		newPosition--;
		if ( newPosition > 0 )
		{
			int bestPosition = 0;

			// look for the next tab position
			for ( int i = 0; i < _mainPanel->GetChildCount(); i++ )
			{
				Panel* child = _mainPanel->GetChild( i );
				if ( !child || !child->IsVisible() || !child->IsEnabled() || !child->GetTabPosition() )
					continue;

				int tabPosition = child->GetTabPosition();
				if ( tabPosition == newPosition )
				{
					best = child;
					break;
				}

				if ( tabPosition < newPosition && tabPosition > bestPosition )
				{
					// record the match since this is the closest so far
					bestPosition = tabPosition;
					best = child;
				}
			}
		}

		if ( !repeat || best )
			break;

		if ( !_topLevelFocus && _mainPanel->GetVParent()
			 && _mainPanel->GetVParent() != surface()->GetEmbeddedPanel() )
		{
			// we're not a top level panel, so forward up the request instead of looping
			if ( ipanel()->RequestFocusPrev( _mainPanel->GetVParent(), _mainPanel->GetVPanel() ) )
			{
				found = true;
				SetCurrentDefaultButton( 0, true );
				break;
			}
		}

		newPosition = 9999999;
		repeat = false;
	}

	if ( best )
	{
		_currentFocus.Set( best->GetVPanel() );
		best->RequestFocus( -1 );
		found = true;

		if ( CanButtonBeDefault( best->GetVPanel() ) )
		{
			SetCurrentDefaultButton( best->GetVPanel(), true );
		}
		else if ( _defaultButton.Get() )
		{
			SetCurrentDefaultButton( _defaultButton.Get(), true );
		}
		else
		{
			SetCurrentDefaultButton( 0, true );

			if ( _mainPanel->GetVParent() )
				ivgui()->PostMessage( _mainPanel->GetVParent(), new KeyValues( "FindDefaultButton" ), 0, 0.0f );
		}
	}

	return found;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the focus to the previous panel in the tab order
// Input  : *panel - panel currently with focus
//-----------------------------------------------------------------------------
bool FocusNavGroup::RequestFocusNext( VPANEL panel )
{
	static int stackDepth = 0;
	stackDepth++;

	_currentFocus.Set( 0 );

	int newPosition = 0;
	if ( panel )
		newPosition = ipanel()->GetTabPosition( panel );

	bool found = false;
	bool repeat = true;
	Panel* best = NULL;
	while ( 1 )
	{
		newPosition++;
		int bestPosition = 999999;

		// look for the next tab position
		for ( int i = 0; i < _mainPanel->GetChildCount(); i++ )
		{
			Panel* child = _mainPanel->GetChild( i );
			if ( !child || !child->IsVisible() || !child->IsEnabled() || !child->GetTabPosition() )
				continue;

			int tabPosition = child->GetTabPosition();
			if ( tabPosition == newPosition )
			{
				best = child;
				break;
			}

			if ( tabPosition > newPosition && tabPosition < bestPosition )
			{
				// record the match since this is the closest so far
				bestPosition = tabPosition;
				best = child;
			}
		}

		if ( !repeat || best )
			break;

		if ( !_topLevelFocus && _mainPanel->GetVParent()
			 && _mainPanel->GetVParent() != surface()->GetEmbeddedPanel() && stackDepth < 15 )
		{
			if ( ipanel()->RequestFocusNext( _mainPanel->GetVParent(), _mainPanel->GetVPanel() ) )
			{
				found = true;
				SetCurrentDefaultButton( 0, true );
				break;
			}
		}

		// loop back
		newPosition = 0;
		repeat = false;
	}

	if ( best )
	{
		_currentFocus.Set( best->GetVPanel() );
		best->RequestFocus( 1 );
		found = true;

		if ( CanButtonBeDefault( best->GetVPanel() ) )
		{
			SetCurrentDefaultButton( best->GetVPanel(), true );
		}
		else if ( _defaultButton.Get() )
		{
			SetCurrentDefaultButton( _defaultButton.Get(), true );
		}
		else
		{
			SetCurrentDefaultButton( 0, true );

			if ( _mainPanel->GetVParent() )
				ivgui()->PostMessage( _mainPanel->GetVParent(), new KeyValues( "FindDefaultButton" ), 0, 0.0f );
		}
	}

	stackDepth--;
	return found;
}

//-----------------------------------------------------------------------------
// Purpose: finds the panel which is activated by the specified key
// Input  : code - the keycode of the hotkey
// Output : Panel * - NULL if no panel found
//-----------------------------------------------------------------------------
Panel* FocusNavGroup::FindPanelByHotkey( wchar_t key )
{
	for ( int i = 0; i < _mainPanel->GetChildCount(); i++ )
	{
		Panel* child = _mainPanel->GetChild( i );
		if ( !child )
			continue;

		Panel* hot = child->HasHotkey( key );
		if ( hot && hot->IsVisible() && hot->IsEnabled() )
			return hot;
	}

	return NULL;
}
}
