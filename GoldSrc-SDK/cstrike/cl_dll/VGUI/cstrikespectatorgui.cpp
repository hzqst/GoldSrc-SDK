//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "cstrikespectatorgui.h"
#include "icstrikeviewport.h"
#include "game_controls/iviewport.h"
#include "hud_iga.h"
#include "Label.h"
#include <stdio.h>
#include <wchar.h>

bool IsBombPlanted();

CCSSpectatorGUI::CCSSpectatorGUI( vgui2::Panel* parent ) : CSpectatorGUI( parent )
{
	m_pCTLabel = dynamic_cast<vgui2::Label*>( FindChildByName( "CTScoreLabel", false ) );
	m_pCTScore = dynamic_cast<vgui2::Label*>( FindChildByName( "CTScoreValue", false ) );
	m_pTerLabel = dynamic_cast<vgui2::Label*>( FindChildByName( "TerScoreLabel", false ) );
	m_pTerScore = dynamic_cast<vgui2::Label*>( FindChildByName( "TerScoreValue", false ) );
	m_pTimer = FindChildByName( "timerimage", false );
	m_pTimerLabel = dynamic_cast<vgui2::Label*>( FindChildByName( "timerlabel", false ) );
	m_pDivider = FindChildByName( "DividerBar", false );
	m_pExtraInfo = dynamic_cast<vgui2::Label*>( FindChildByName( "extrainfo", false ) );
	m_modifiedWidths = false;
	m_scoreWidth = 0;
	m_extraInfoWidth = 0;
	m_pIGA = NULL;
	m_pTopBar = FindChildByName( "topbar", false );
	m_pBanner = m_pTopBar->FindChildByName( "banner", false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSSpectatorGUI::~CCSSpectatorGUI()
{
}

bool CCSSpectatorGUI::CanSpectateTeam( int team )
{
	int forceCamera = gCSViewPortInterface->GetForceCamera();
	return g_iTeamNumber == 0 || gEngfuncs.IsSpectateOnly() || forceCamera == 0 || team == g_iTeamNumber;
}

bool CCSSpectatorGUI::ControlsOK()
{
	return m_pCTLabel && m_pCTScore && m_pTerLabel && m_pTerScore && m_pTimer &&
		m_pTimerLabel && m_pDivider && m_pExtraInfo;
}

void CCSSpectatorGUI::StoreWidths()
{
	if ( ControlsOK() && !m_modifiedWidths )
	{
		m_scoreWidth = m_pCTScore->GetWide();
		int terroristWidth = m_pTerScore->GetWide();
		m_extraInfoWidth = m_pExtraInfo->GetWide();
		if ( terroristWidth != m_scoreWidth )
			m_pTerScore = NULL;
	}
}

void CCSSpectatorGUI::LayoutAd()
{
	if ( !m_pIGA )
		return;

	bool shown = true;
	if ( m_pTopBar && g_pClientIGA->IsPanelReady( IGA_SPECTATOR, &shown ) )
	{
		if ( shown )
		{
			int wide, tall;
			m_pTopBar->GetSize( wide, tall );
			wide = (int)( wide * 0.6f );
			m_pIGA->SetBounds( 5, 5, ( tall - 10 ) * 4, tall - 10 );
		}
		else
		{
			m_pIGA->SetBounds( 0, 0, 1, 1 );
		}
		m_pIGA->SetVisible( true );
		m_pBanner->SetVisible( false );
	}
	else
	{
		if ( m_pIGA )
			m_pIGA->SetVisible( false );
		m_pBanner->SetVisible( true );
	}
}

void CCSSpectatorGUI::SizeControls()
{
	LayoutAd();
	StoreWidths();
	if ( !ControlsOK() )
		return;

	int x, y, width, height, ctWidth, ctHeight, terWidth, terHeight;
	m_pCTScore->GetBounds( x, y, width, height );
	m_pCTScore->GetContentSize( ctWidth, ctHeight );
	int terX, terY, terWide, terTall;
	m_pTerScore->GetBounds( terX, terY, terWide, terTall );
	m_pTerScore->GetContentSize( terWidth, terHeight );
	int scoreWidth = m_scoreWidth;
	if ( ctWidth >= scoreWidth )
		scoreWidth = ctWidth;
	if ( terWidth >= scoreWidth )
		scoreWidth = terWidth;
	int delta = scoreWidth - width;
	if ( delta )
	{
		m_pCTScore->GetBounds( x, y, width, height );
		m_pCTScore->SetBounds( x - delta, y, width + delta, height );
		m_pTerScore->GetBounds( x, y, width, height );
		m_pTerScore->SetBounds( x - delta, y, width + delta, height );
		m_pCTLabel->GetPos( x, y );
		m_pCTLabel->SetPos( x - delta, y );
		m_pTerLabel->GetPos( x, y );
		m_pTerLabel->SetPos( x - delta, y );
		m_modifiedWidths = true;
	}

	int extraWidth, extraHeight;
	m_pExtraInfo->GetBounds( x, y, width, height );
	m_pExtraInfo->GetContentSize( extraWidth, extraHeight );
	if ( extraWidth < m_extraInfoWidth )
		extraWidth = m_extraInfoWidth;
	delta = extraWidth - width;
	if ( delta )
	{
		m_pExtraInfo->GetBounds( x, y, width, height );
		m_pExtraInfo->SetBounds( x - delta, y, width + delta, height );
		m_pTimer->GetPos( x, y );
		m_pTimer->SetPos( x - delta, y );
		m_pTimerLabel->GetPos( x, y );
		m_pTimerLabel->SetPos( x - delta, y );
		m_pDivider->GetPos( x, y );
		m_pDivider->SetPos( x - delta, y );
		m_pCTScore->GetPos( x, y );
		m_pCTScore->SetPos( x - delta, y );
		m_pCTLabel->GetPos( x, y );
		m_pCTLabel->SetPos( x - delta, y );
		m_pTerScore->GetPos( x, y );
		m_pTerScore->SetPos( x - delta, y );
		m_pTerLabel->GetPos( x, y );
		m_pTerLabel->SetPos( x - delta, y );
		m_modifiedWidths = true;
	}
}

void CCSSpectatorGUI::UpdateTimer()
{
	if ( IsBombPlanted() )
	{
		SetLabelText( "timerlabel", "" );
	}
	else
	{
		int seconds = (int)( gHUD.m_roundTimer.m_flTimeEnd - gHUD.m_flTime );
		if ( seconds < 0 )
			seconds = 0;
		wchar_t text[64];
		_snwprintf( text, 63, L"%d:%02d", seconds / 60, seconds % 60 );
		text[62] = 0;
		SetLabelText( "timerlabel", text );
		LayoutAd();
	}
}



//-----------------------------------------------------------------------------
// Purpose: Resets the list of players
//-----------------------------------------------------------------------------
void CCSSpectatorGUI::UpdateSpectatorPlayerList()
{
	CSpectatorGUI::UpdateSpectatorPlayerList();
	bool foundCT = false;
	bool foundTerrorist = false;
	for ( int i = 0; i < 64; ++i )
	{
		VGuiLibraryTeamInfo_t team = gViewPortInterface->GetPlayerTeamInfo( i );
		if ( team.name )
		{
			wchar_t frags[10];
			if ( !stricmp( team.name, "TERRORIST" ) )
			{
				_snwprintf( frags, 10, L"%i", team.frags );
				SetLabelText( "TERScoreValue", frags );
				foundTerrorist = true;
			}
			else if ( !stricmp( team.name, "CT" ) )
			{
				_snwprintf( frags, 10, L"%i", team.frags );
				SetLabelText( "CTScoreValue", frags );
				foundCT = true;
			}
			if ( foundTerrorist == true && foundCT == true )
				break;
		}
	}
	if ( foundTerrorist && foundCT )
		SizeControls();
}

void CCSSpectatorGUI::Update( const char* serverName, int player, int mode,
	bool spectateOnly, int spectatorCount, const char* mapName )
{
	CSpectatorGUI::Update( serverName, player, mode, spectateOnly, spectatorCount, mapName );
	if ( g_iTeamNumber == 1 || g_iTeamNumber == 2 )
	{
		wchar_t text[64];
		_snwprintf( text, 63, L"$%i", gHUD.m_accountBalance.GetBalance() );
		text[62] = 0;
		SetLabelText( "extrainfo", text );
	}
	SizeControls();
}
