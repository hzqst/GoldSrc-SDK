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

#pragma once

#include "Frame.h"
#include "vgui/Dar.h"

namespace vgui2
{
class Button;
class WizardSubPanel;

class WizardPanel : public Frame
{
	DECLARE_CLASS_SIMPLE( WizardPanel, Frame );

public:
	WizardPanel( Panel* parent, const char* name );
	~WizardPanel() override;

	// Start the wizard, starting with the startPanel
	virtual void Run( WizardSubPanel* panel );
	MESSAGE_FUNC( OnNextButton, "NextButton" );
	MESSAGE_FUNC( OnPrevButton, "PrevButton" );
	MESSAGE_FUNC( OnFinishButton, "FinishButton" );
	MESSAGE_FUNC( OnCancelButton, "CancelButton" );
	// sets whether or not a button is enabled
	// this state is managed, and will be reset whenever going to a new page
	virtual void SetNextButtonEnabled( bool state );
	virtual void SetPrevButtonEnabled( bool state );
	virtual void SetFinishButtonEnabled( bool state );
	virtual void SetCancelButtonEnabled( bool state );
	// sets whether or not a button is visible
	// this state is unmanaged, the user needs to ensure that the buttons state
	// is correct when going both back and prev through the wizard
	virtual void SetNextButtonVisible( bool state );
	virtual void SetPrevButtonVisible( bool state );
	virtual void SetFinishButtonVisible( bool state );
	virtual void SetCancelButtonVisible( bool state );
	// sets the text for a button
	// setting the text to be NULL resets the text to it's default state
	// this state is unmanaged, the user needs to ensure that the buttons state
	// is correct when going both back and prev through the wizard
	virtual void SetNextButtonText( const char* text );
	virtual void SetPrevButtonText( const char* text );
	virtual void SetFinishButtonText( const char* text );
	virtual void SetCancelButtonText( const char* text );
	// general wizard state for all the subpanels to access
	virtual KeyValues* GetWizardData();
	// recalculates where the key focus should be in the wizard
	virtual void ResetKeyFocus();
	virtual void ResetDefaultButton();
	// resets the sub panel history for the control
	virtual void ResetHistory();
	// returns a page by name
	virtual WizardSubPanel* GetSubPanelByName( const char* name );
	virtual void ShowButtons( bool state );
	void GetClientArea( int& x, int& y, int& width, int& height ) override;

protected:
	MESSAGE_FUNC_PTR( InternalActivateNextSubPanel, "ActivateNextSubPanel", panel );
	virtual void ActivateNextSubPanel( WizardSubPanel* panel );
	virtual void ActivatePrevSubPanel();
	virtual void CreateButtons();
	virtual void RecalculateTabOrdering();
	virtual WizardSubPanel* GetCurrentSubPanel();
	void PerformLayout() override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void ActivateBuildMode() override;
	void OnClose() override;
	void OnCommand( const char* command ) override;
	void OnCloseFrameButtonPressed() override;

private:
	WizardSubPanel* FindNextValidSubPanel( WizardSubPanel* panel );

	Button* _prevButton;
	Button* _nextButton;
	Button* _cancelButton;
	Button* _finishButton;
	WizardSubPanel* _currentSubPanel;
	KeyValues* _currentData;
	Dar<WizardSubPanel*> _subPanelStack;
	bool _showButtons;
};
} // namespace vgui
