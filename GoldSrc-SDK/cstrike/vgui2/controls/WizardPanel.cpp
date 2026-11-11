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

#include "WizardPanel.h"
#include "WizardSubPanel.h"
#include "Button.h"
#include "FocusNavGroup.h"
#include "controls.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include <cstring>

using namespace vgui2;

WizardPanel::WizardPanel( Panel* parent, const char* name ) : Frame( parent, name, true, false )
{
	_currentSubPanel = NULL;
	_currentData = new KeyValues( "WizardData" );
	_showButtons = true;
	SetSizeable( false );
	CreateButtons();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
WizardPanel::~WizardPanel()
{
	if ( _currentData )
		_currentData->deleteThis();
}

void WizardPanel::Run( WizardSubPanel* panel )
{
	ActivateNextSubPanel( FindNextValidSubPanel( panel ) );
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Advances to the next panel
//-----------------------------------------------------------------------------
void WizardPanel::OnNextButton()
{
	if ( !_currentSubPanel || !_currentSubPanel->OnNextButton() )
		return;

	WizardSubPanel* panel = FindNextValidSubPanel( _currentSubPanel->GetNextSubPanel() );
	if ( panel )
	{
		KeyValues* message = new KeyValues( "ActivateNextSubPanel" );
		message->SetPtr( "panel", panel );
		ivgui()->PostMessage( GetVPanel(), message, GetVPanel() );
	}
}

void WizardPanel::OnPrevButton()
{
	if ( !_currentSubPanel || _currentSubPanel->OnPrevButton() )
		ActivatePrevSubPanel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::OnFinishButton()
{
	if ( _currentSubPanel && _currentSubPanel->OnFinishButton() )
	{
		// hide ourselves away
		BaseClass::OnClose();
		if ( IsAutoDeleteSet() )
			MarkForDeletion();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::OnCancelButton()
{
	if ( _currentSubPanel && _currentSubPanel->OnCancelButton() )
	{
		// hide ourselves away
		BaseClass::OnClose();
		if ( IsAutoDeleteSet() )
			MarkForDeletion();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::SetNextButtonEnabled( bool state )
{
	if ( _nextButton->IsEnabled() != state )
	{
		_nextButton->SetEnabled( state );
		InvalidateLayout( false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::SetPrevButtonEnabled( bool state )
{
	if ( _prevButton->IsEnabled() != state )
	{
		_prevButton->SetEnabled( state );
		InvalidateLayout( false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::SetFinishButtonEnabled( bool state )
{
	if ( _finishButton->IsEnabled() != state )
	{
		_finishButton->SetEnabled( state );
		InvalidateLayout( false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::SetCancelButtonEnabled( bool state )
{
	if ( _cancelButton->IsEnabled() != state )
	{
		_cancelButton->SetEnabled( state );
		InvalidateLayout( false, false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::SetNextButtonVisible( bool state )
{
	_nextButton->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::SetPrevButtonVisible( bool state )
{
	_prevButton->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::SetFinishButtonVisible( bool state )
{
	_finishButton->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::SetCancelButtonVisible( bool state )
{
	_cancelButton->SetVisible( state );
}

void WizardPanel::SetNextButtonText( const char* text )
{
	_nextButton->SetText( text ? text : "#WizardPanel_Next" );
}

void WizardPanel::SetPrevButtonText( const char* text )
{
	_prevButton->SetText( text ? text : "#WizardPanel_Back" );
}

void WizardPanel::SetFinishButtonText( const char* text )
{
	_finishButton->SetText( text ? text : "#WizardPanel_Finish" );
}

void WizardPanel::SetCancelButtonText( const char* text )
{
	_cancelButton->SetText( text ? text : "#WizardPanel_Cancel" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
KeyValues* WizardPanel::GetWizardData()
{
	return _currentData;
}

void WizardPanel::ResetKeyFocus()
{
	Panel* focus = GetFocusNavGroup().GetCurrentFocus();
	if ( focus )
	{
		if ( focus->IsEnabled() && focus->IsVisible() )
			focus->RequestFocus( 0 );
		else
			focus->RequestFocusNext( 0 );
	}
	ResetDefaultButton();
}

void WizardPanel::ResetDefaultButton()
{
	// work out which is the default button
	if ( _nextButton->IsEnabled() )
		_nextButton->SetAsDefaultButton( true );
	else if ( _finishButton->IsEnabled() )
		_finishButton->SetAsDefaultButton( true );
	else if ( _prevButton->IsEnabled() )
		_prevButton->SetAsDefaultButton( true );

	_nextButton->InvalidateLayout( false, false );
	_prevButton->InvalidateLayout( false, false );
	_cancelButton->InvalidateLayout( false, false );
	_finishButton->InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: clears all previous history
//-----------------------------------------------------------------------------
void WizardPanel::ResetHistory()
{
	_subPanelStack.RemoveAll();
}

WizardSubPanel* WizardPanel::GetSubPanelByName( const char* name )
{
	return dynamic_cast<WizardSubPanel*>( FindChildByName( name, false ) );
}


//-----------------------------------------------------------------------------
// Purpose: whether to show the next,prev,finish and cancel buttons
//-----------------------------------------------------------------------------
void WizardPanel::ShowButtons( bool state )
{
	_showButtons = state; 	// hide the wizard panel buttons
	SetNextButtonVisible( state );
	SetPrevButtonVisible( state );
	SetFinishButtonVisible( state );
	SetCancelButtonVisible( state );
}


//-----------------------------------------------------------------------------
// Purpose: if we don't show buttons then let the sub panel occupy the whole screen
//-----------------------------------------------------------------------------
void WizardPanel::GetClientArea( int& x, int& y, int& wide, int& tall )
{
	if ( _showButtons )
		BaseClass::GetClientArea( x, y, wide, tall );
	else
	{
		x = y = 0;
		GetSize( wide, tall );
	}
}

void WizardPanel::InternalActivateNextSubPanel( Panel* panel )
{
	ActivateNextSubPanel( static_cast<WizardSubPanel*>( panel ) );
}

void WizardPanel::ActivateNextSubPanel( WizardSubPanel* panel )
{
	WizardSubPanel* previous = _currentSubPanel;
	if ( previous && previous->ShouldDisplayPanel() )
	{
		// hide
		previous->SetVisible( false );

		// push onto history stack
		_subPanelStack.AddElement( _currentSubPanel );
	}

	// reenable all buttons, returning them to their default state
	_prevButton->SetEnabled( true );
	_nextButton->SetEnabled( true );
	_cancelButton->SetEnabled( true );
	_finishButton->SetEnabled( true );
	if ( _showButtons )
	{
		_prevButton->SetVisible( true );
		_cancelButton->SetVisible( true );
	}

	// set up new subpanel
	_currentSubPanel = panel;
	_currentSubPanel->SetParent( this );
	_currentSubPanel->SetVisible( true );
	_currentSubPanel->SetWizardPanel( this );
	_currentSubPanel->OnDisplayAsNext();
	_currentSubPanel->OnDisplay();
	_currentSubPanel->InvalidateLayout( false, false );
	_currentSubPanel->MakeReadyForUse();

	int wide, tall;
	if ( _currentSubPanel->GetDesiredSize( wide, tall ) )
		SetSize( wide, tall );
	if ( !previous )
		_prevButton->SetEnabled( false );
	_currentSubPanel->RequestFocus( 0 );
	RecalculateTabOrdering();
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Pops the last panel off the stack and runs it
//-----------------------------------------------------------------------------
void WizardPanel::ActivatePrevSubPanel()
{
	_currentSubPanel->SetVisible( false );
	WizardSubPanel* panel = NULL;
	if ( _subPanelStack.GetCount() )
	{
		WizardSubPanel* previous = _currentSubPanel->GetPrevSubPanel();
		if ( previous && _subPanelStack.HasElement( previous ) )
		{
			do
			{
				int index = _subPanelStack.GetCount() - 1;
				panel = _subPanelStack[index];
				_subPanelStack.RemoveElementAt( index );
			}
			while ( panel != previous && _subPanelStack.GetCount() );
		}
		else
		{
			int index = _subPanelStack.GetCount() - 1;
			panel = _subPanelStack[index];
			_subPanelStack.RemoveElementAt( index );
		}
	}
	if ( !panel )
	{
		ivgui()->DPrintf( "Error: WizardPanel::ActivatePrevSubPanel(): no previous panel to go back to\n" );
		return;
	}

	// hide old panel
	_currentSubPanel->SetVisible( false );

	// reenable all buttons, returning them to their default state
	_prevButton->SetEnabled( true );
	_nextButton->SetEnabled( true );
	_cancelButton->SetEnabled( true );
	_finishButton->SetEnabled( true );
	_currentSubPanel = panel;
	_currentSubPanel->RequestFocus( 0 );
	_currentSubPanel->SetWizardPanel( this );
	_currentSubPanel->OnDisplayAsPrev();
	_currentSubPanel->OnDisplay();
	_currentSubPanel->InvalidateLayout( false, false );
	_currentSubPanel->MakeReadyForUse();
	int wide, tall;
	if ( _currentSubPanel->GetDesiredSize( wide, tall ) )
		SetSize( wide, tall );
	_currentSubPanel->SetVisible( true );
	if ( !_subPanelStack.GetCount() )
		_prevButton->SetEnabled( false );
	RecalculateTabOrdering();
	InvalidateLayout( false, false );
	Repaint();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::CreateButtons()
{
	_prevButton = new Button( this, "PrevButton", "" );
	_nextButton = new Button( this, "NextButton", "" );
	_cancelButton = new Button( this, "CancelButton", "" );
	_finishButton = new Button( this, "FinishButton", "" );
	_prevButton->SetCommand( new KeyValues( "PrevButton" ) );
	_nextButton->SetCommand( new KeyValues( "NextButton" ) );
	_cancelButton->SetCommand( new KeyValues( "CancelButton" ) );
	_finishButton->SetCommand( new KeyValues( "FinishButton" ) );
	SetNextButtonText( NULL );
	SetPrevButtonText( NULL );
	SetFinishButtonText( NULL );
	SetCancelButtonText( NULL );
	_prevButton->SetSize( 82, 24 );
	_nextButton->SetSize( 82, 24 );
	_cancelButton->SetSize( 82, 24 );
	_finishButton->SetSize( 82, 24 );
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the new tab ordering
//-----------------------------------------------------------------------------
void WizardPanel::RecalculateTabOrdering()
{
	if ( _currentSubPanel )
		_currentSubPanel->SetTabPosition( 1 );
	_prevButton->SetTabPosition( 2 );
	_nextButton->SetTabPosition( 3 );
	_finishButton->SetTabPosition( 4 );
	_cancelButton->SetTabPosition( 5 );
}

WizardSubPanel* WizardPanel::GetCurrentSubPanel()
{
	return _currentSubPanel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// resize the sub panel to fit in the Client area
	int x, y, wide, tall;
	GetClientArea( x, y, wide, tall );
	if ( _currentSubPanel )
	{
		if ( _currentSubPanel->isNonWizardPanel() )
		{
			// just have the subpanel cover the full size
			_currentSubPanel->SetBounds( x, y, wide, tall );
			_cancelButton->SetVisible( false );
			_prevButton->SetVisible( false );
			_nextButton->SetVisible( false );
			_finishButton->SetVisible( false );
			return;
		}
		_currentSubPanel->SetBounds( x, y, wide, tall - ( _showButtons ? 35 : 0 ) );
	}

	int buttonWide, buttonTall;
	GetSize( wide, tall );
	_cancelButton->GetSize( buttonWide, buttonTall );
	x = wide - buttonWide - 20;
	y = tall - buttonTall - 12;
	_cancelButton->SetPos( x, y );
	x -= buttonWide + 20;

	// only display one of the next or finish buttons (and only if both are visible)
	if ( _showButtons )
	{
		if ( _finishButton->IsEnabled() )
		{
			_nextButton->SetVisible( false );
			_finishButton->SetVisible( true );
			_finishButton->SetPos( x, y );
		}
		else
		{
			_nextButton->SetVisible( true );
			_finishButton->SetVisible( false );
			_nextButton->SetPos( x, y );
		}
	}
	x -= buttonWide + 1;
	_prevButton->SetPos( x, y );
	ResetDefaultButton();
}

void WizardPanel::ApplySchemeSettings( IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void WizardPanel::ActivateBuildMode()
{
	if ( _currentSubPanel )
		_currentSubPanel->ActivateBuildMode();
}


//-----------------------------------------------------------------------------
// Purpose: Maps close button to cancel button
//-----------------------------------------------------------------------------
void WizardPanel::OnClose()
{
	if ( _cancelButton->IsEnabled() )
		_cancelButton->DoClick();
	else if ( _finishButton->IsEnabled() )
		_finishButton->DoClick();
}


//-----------------------------------------------------------------------------
// Purpose: command handler for catching escape key presses
//-----------------------------------------------------------------------------
void WizardPanel::OnCommand( const char* command )
{
	if ( !_stricmp( command, "Cancel" ) )
	{
		if ( _cancelButton->IsEnabled() )
			_cancelButton->DoClick();
	}
	else
		BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: filters close buttons
//-----------------------------------------------------------------------------
void WizardPanel::OnCloseFrameButtonPressed()
{
	if ( _cancelButton->IsEnabled() )
		BaseClass::OnCloseFrameButtonPressed();
}

WizardSubPanel* WizardPanel::FindNextValidSubPanel( WizardSubPanel* panel )
{
	// skip over sub panels if they don't want to be displayed
	while ( panel )
	{
		panel->SetWizardPanel( this );
		if ( panel->ShouldDisplayPanel() )
			return panel;
		panel = panel->GetNextSubPanel();
	}
	return NULL;
}
