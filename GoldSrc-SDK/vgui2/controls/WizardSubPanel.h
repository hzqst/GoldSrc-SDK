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

#include "EditablePanel.h"

namespace vgui2
{
class WizardPanel;

//-----------------------------------------------------------------------------
// Purpose: Base panel for use in Wizards and in property sheets
//-----------------------------------------------------------------------------
class WizardSubPanel : public EditablePanel
{
	DECLARE_CLASS_SIMPLE( WizardSubPanel, EditablePanel );

public:
	// constructor
	WizardSubPanel( Panel* parent, const char* name );
	~WizardSubPanel() override;

	// called when the subpanel is displayed
	// All controls & data should be reinitialized at this time
	virtual void OnDisplayAsNext();
	// called anytime the panel is first displayed, whether the user is moving forward or back
	// called immediately after OnDisplayAsNext/OnDisplayAsPrev
	virtual void OnDisplay();
	// called when displayed as previous
	virtual void OnDisplayAsPrev();
	virtual bool OnNextButton();
	virtual bool OnPrevButton();
	virtual bool OnFinishButton();
	virtual bool OnCancelButton();
	virtual bool ShouldDisplayPanel();
	virtual bool isNonWizardPanel();
	// returns a pointer to the next subpanel that should be displayed
	virtual WizardSubPanel* GetNextSubPanel() = 0;
	virtual WizardSubPanel* GetPrevSubPanel();
	virtual WizardPanel* GetWizardPanel();
	virtual void SetWizardPanel( WizardPanel* panel );
	// returns a pointer to the wizard's doc
	virtual KeyValues* GetWizardData();
	// returns a pointer
	virtual WizardSubPanel* GetSiblingSubPanelByName( const char* name );
	// gets the size this subpanel would like the wizard to be
	// returns true if it has a desired size
	virtual bool GetDesiredSize( int& width, int& height );

protected:
	void ApplySettings( KeyValues* settings ) override;
	void GetSettings( KeyValues* settings ) override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	const char* GetDescription() override;

private:
	WizardPanel* _wizardPanel;
	int m_iDesiredWide;
	int m_iDesiredTall;
};
} // namespace vgui
