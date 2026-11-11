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

#include "WizardSubPanel.h"
#include "WizardPanel.h"
#include "vgui/KeyValues.h"
#include <cstdio>

using namespace vgui2;

WizardSubPanel::WizardSubPanel( Panel* parent, const char* name ) : EditablePanel( parent, name )
{
	_wizardPanel = NULL;
	SetVisible( false );
	m_iDesiredWide = 0;
	m_iDesiredTall = 0;
	SetBuildGroup( GetBuildGroup() );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
WizardSubPanel::~WizardSubPanel()
{
}

void WizardSubPanel::OnDisplayAsNext()
{
}

void WizardSubPanel::OnDisplay()
{
}

void WizardSubPanel::OnDisplayAsPrev()
{
}

bool WizardSubPanel::OnNextButton()
{
	return true;
}

bool WizardSubPanel::OnPrevButton()
{
	return true;
}

bool WizardSubPanel::OnFinishButton()
{
	return true;
}

bool WizardSubPanel::OnCancelButton()
{
	return true;
}

bool WizardSubPanel::ShouldDisplayPanel()
{
	return true;
}

bool WizardSubPanel::isNonWizardPanel()
{
	return false;
}

WizardSubPanel* WizardSubPanel::GetPrevSubPanel()
{
	return NULL;
}

WizardPanel* WizardSubPanel::GetWizardPanel()
{
	return _wizardPanel;
}

void WizardSubPanel::SetWizardPanel( WizardPanel* panel )
{
	_wizardPanel = panel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
KeyValues* WizardSubPanel::GetWizardData()
{
	return GetWizardPanel()->GetWizardData();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
WizardSubPanel* WizardSubPanel::GetSiblingSubPanelByName( const char* name )
{
	return GetWizardPanel()->GetSubPanelByName( name );
}

//-----------------------------------------------------------------------------
// Purpose: gets the size this subpanel would like the wizard to be
//-----------------------------------------------------------------------------
bool WizardSubPanel::GetDesiredSize( int& wide, int& tall )
{
	wide = m_iDesiredWide;
	tall = m_iDesiredTall;
	return wide != 0 && tall != 0;
}

void WizardSubPanel::ApplySettings( KeyValues* settings )
{
	bool visible = IsVisible();
	BaseClass::ApplySettings( settings );
	m_iDesiredWide = settings->GetInt( "WizardWide", 0 );
	m_iDesiredTall = settings->GetInt( "WizardTall", 0 );
	if ( GetWizardPanel() && m_iDesiredWide && m_iDesiredTall )
		GetWizardPanel()->SetSize( m_iDesiredWide, m_iDesiredTall );
	SetVisible( visible );
}

void WizardSubPanel::GetSettings( KeyValues* settings )
{
	Panel::GetSettings( settings );
	settings->SetInt( "WizardWide", m_iDesiredWide );
	settings->SetInt( "WizardTall", m_iDesiredTall );
}

void WizardSubPanel::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	SetBgColor( GetSchemeColor( "SubPanelBgColor", scheme ) );
}

//-----------------------------------------------------------------------------
// Purpose: build mode description
//-----------------------------------------------------------------------------
const char* WizardSubPanel::GetDescription()
{
	static char description[1024];
	_snprintf( description, sizeof( description ), "%s, int WizardWide, int WizardTall", Panel::GetDescription() );
	return description;
}
