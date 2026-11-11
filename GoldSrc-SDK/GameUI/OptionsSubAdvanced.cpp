//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "OptionsSubAdvanced.h"

#include "ContentControlDialog.h"
#include "CvarToggleCheckButton.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"

#include "CheckButton.h"
#include "igameuifuncs.h"
#include "wrect.h"
#include "cl_dll.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

COptionsSubAdvanced::COptionsSubAdvanced( vgui2::Panel* parent )
	: vgui2::PropertyPage( parent, NULL, true )
{
	m_pContentCheckButton = new vgui2::CheckButton( this, "ContentlockButton", "#GameUI_ContentLock" );
	m_pContentCheckButton->SetCommand( "ContentControl" );
	// set up the content control dialog
	m_pContentControlDialog = new CContentControlDialog( parent );
	m_pContentControlDialog->AddActionSignalTarget( this );

	int x, y, workWide, workTall;
	vgui2::surface()->GetWorkspaceBounds( x, y, workWide, workTall );
	int wide, tall;
	m_pContentControlDialog->GetSize( wide, tall );
	m_pContentControlDialog->SetPos( x + ( workWide - wide ) / 2, y + ( workTall - tall ) / 2 );
	m_pContentCheckButton->SetSelected( m_pContentControlDialog->IsPasswordEnabled() );

	char renderer[128];
	int windowed, hdModels, addons;
	gameuifuncs->GetCurrentRenderer( renderer, sizeof( renderer ), &windowed, &hdModels, &addons );
	m_pDetailTextures = new CCvarToggleCheckButton( this, "DetailTextures", "#GameUI_DetailTextures", "r_detailtextures" );
	m_pOriginalModels = new vgui2::CheckButton( this, "OriginalModels", "#GameUI_OriginalModels" );
	if ( m_pOriginalModels )
		m_pOriginalModels->SetSelected( !hdModels );

	m_pAddonsFolder = new vgui2::CheckButton( this, "AddonsFolder", "#GameUI_AddonsFolder" );
	m_pAddonsFolder->SetSelected( addons != 0 );
	LoadControlSettings( "Resource\\OptionsSubAdvanced.res", NULL );

	float detailTexturesSupported = engine->pfnGetCvarFloat( "r_detailtexturessupported" );
	if ( ModInfo()->GetDetailedTexture() )
	{
#ifdef _WIN32
		if ( !( detailTexturesSupported > 0.0f ) )
#else
		if ( detailTexturesSupported <= 0.0f )
#endif
			m_pDetailTextures->SetEnabled( false );
	}
	else
	{
		m_pDetailTextures->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsSubAdvanced::~COptionsSubAdvanced()
{
//    m_pContentControlDialog->MarkForDeletion();
}

void COptionsSubAdvanced::OnResetData()
{
	m_pContentControlDialog->ResetPassword();
	m_pContentCheckButton->SetSelected( m_pContentControlDialog->IsPasswordEnabled() );

	char renderer[128];
	int windowed, hdModels, addons;
	gameuifuncs->GetCurrentRenderer( renderer, sizeof( renderer ), &windowed, &hdModels, &addons );
	if ( m_pOriginalModels )
		m_pOriginalModels->SetSelected( !hdModels );

	m_pAddonsFolder->SetSelected( addons != 0 );
	m_pDetailTextures->Reset();
}

void COptionsSubAdvanced::OnDataChanged()
{
	PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
}

void COptionsSubAdvanced::OnApplyChanges()
{
	m_pContentControlDialog->ApplyPassword();
	char renderer[128];
	int windowed, hdModels, addons;
	gameuifuncs->GetCurrentRenderer( renderer, sizeof( renderer ), &windowed, &hdModels, &addons );

	bool restart = false;
	char command[256];
	if ( m_pOriginalModels )
	{
		int useHDModels = !m_pOriginalModels->IsSelected();
		if ( hdModels != useHDModels )
		{
			sprintf( command, "_sethdmodels %d\n", useHDModels );
			restart = true;
			engine->pfnClientCmd( command );
		}
	}

	if ( m_pAddonsFolder )
	{
		bool useAddons = m_pAddonsFolder->IsSelected();
		if ( useAddons != ( addons == 1 ) )
		{
			sprintf( command, "_setaddons_folder %d\n", useAddons );
			restart = true;
			engine->pfnClientCmd( command );
		}
	}

	if ( m_pDetailTextures )
		m_pDetailTextures->ApplyChanges();

	if ( restart )
		engine->pfnClientCmd( "_restart\n" );
}

void COptionsSubAdvanced::OnButtonChecked( KeyValues* data )
{
	int state = data->GetInt( "state", 0 );
	vgui2::Panel* panel = (vgui2::Panel*)data->GetPtr( "panel", NULL );
	char renderer[128];
	int windowed, hdModels, addons;
	gameuifuncs->GetCurrentRenderer( renderer, sizeof( renderer ), &windowed, &hdModels, &addons );
	if ( panel == m_pOriginalModels )
	{
		if ( state != (int)!hdModels )
			OnDataChanged();
	}
	else if ( panel == m_pAddonsFolder )
	{
		if ( state != addons )
			OnDataChanged();
	}
	else if ( panel == m_pDetailTextures )
	{
		OnDataChanged();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnContentControlClose()
{
	m_pContentCheckButton->SetSelected( m_pContentControlDialog->IsPasswordEnabledInDialog() );
	bool enabled = m_pContentControlDialog->IsPasswordEnabled();
	if ( m_pContentControlDialog->IsPasswordEnabledInDialog() != enabled )
		PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnOpenContentControlDialog()
{
	m_pContentControlDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnCommand( const char* command )
{
	if ( !stricmp( command, "ContentControl" ) )
	{
		if ( !m_pContentCheckButton->IsSelected() && !m_pContentControlDialog->IsPasswordEnabled() )
			m_pContentControlDialog->ResetPassword();
		else
			m_pContentControlDialog->Activate();
	}
	else
	{
		vgui2::Panel::OnCommand( command );
	}
}
