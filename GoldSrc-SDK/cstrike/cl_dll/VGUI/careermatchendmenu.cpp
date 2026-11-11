#include "careermatchendmenu.h"
#include "careermenus.h"
#include "creditsmenu.h"
#include "hud.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "BitmapImagePanel.h"
#include "Button.h"
#include "Label.h"
#include "vgui/KeyValues.h"
#include "game_controls/iviewport.h"
#include <stdio.h>
#include <string.h>

extern void IN_ScoreUp();

CCSCareerMatchEndMenu::CCSCareerMatchEndMenu( vgui2::Panel* parent )
	: CCSCareerBaseMenu( parent, "CSCareerMatchEnd", false )
{
	bool isCT = true;
	if ( g_pCareerUI != NULL )
		isCT = g_pCareerUI->PlayAsCT();

	int wins = isCT ? TheCareerMatchStatus.ctWins : TheCareerMatchStatus.tWins;
	int losses = isCT ? TheCareerMatchStatus.tWins : TheCareerMatchStatus.ctWins;

	char buf[32];
	sprintf( buf, "#Career_Match_%s", wins > losses ? "Win" : "Lose" );
	m_pResultLabel = new vgui2::Label( this, "GameResultLabel", buf );
	m_pRetryButton = new vgui2::Button( this, "retry", "" );
	m_pFinishButton = new vgui2::Button( this, "ok", "" );
	m_pWinsLabel = new vgui2::Label( this, "Wins", "" );
	m_pLossesLabel = new vgui2::Label( this, "Losses", "" );
	m_pNewPointsLabel = new vgui2::Label( this, "LabelNewPoints", "" );
	m_pNewLocationsLabel = new vgui2::Label( this, "LabelNewLocations", "" );
	m_pCampaignCompleteLabel = new vgui2::Label( this, "LabelCampaign", "" );
	m_pMedalEarnedLabel = new vgui2::Label( this, "LabelEarnedMedal", "" );
	m_pMedalNameLabel = new vgui2::Label( this, "LabelMedalName", "" );
	m_pMedalPanel = new vgui2::CBitmapImagePanel( this, "PanelMedal", "" );
	m_pMapCompleteLabel[0] = new vgui2::Label( this, "LabelMapComplete1", "" );
	m_pMapCompleteLabel[1] = new vgui2::Label( this, "LabelMapComplete2", "" );
	m_pMapCompleteLabel[2] = new vgui2::Label( this, "LabelMapComplete3", "" );
	m_pMapLabel[0] = new vgui2::Label( this, "LabelMap1", "" );
	m_pMapLabel[1] = new vgui2::Label( this, "LabelMap2", "" );
	m_pMapLabel[2] = new vgui2::Label( this, "LabelMap3", "" );
	m_pMapPanel[0] = new vgui2::CBitmapImagePanel( this, "PanelMap1", "" );
	m_pMapPanel[1] = new vgui2::CBitmapImagePanel( this, "PanelMap2", "" );
	m_pMapPanel[2] = new vgui2::CBitmapImagePanel( this, "PanelMap3", "" );
	LoadControlSettings( "Resource/UI/CSCareerMatchEnd.res", NULL );
}

CCSCareerMatchEndMenu::~CCSCareerMatchEndMenu()
{
}

void CCSCareerMatchEndMenu::ShowMap( int index, const char* name )
{
	m_pMapCompleteLabel[index]->SetVisible( true );
	m_pMapLabel[index]->SetVisible( true );
	m_pMapPanel[index]->SetVisible( true );
	char buf[64];
	_snprintf( buf, sizeof( buf ), "gfx/thumbnails/maps/%s", name );
	m_pMapPanel[index]->setTexture( buf, true );
	gEngfuncs.Con_DPrintf( "Setting map %d filename to [%s]\n", index, buf );
	_snprintf( buf, sizeof( buf ), "#Career_Map_%s", name );
	m_pMapLabel[index]->SetText( buf );
	gEngfuncs.Con_DPrintf( "Setting map %d label to [%s]\n", index, buf );
	m_pMapCompleteLabel[index]->MoveToFront();
}

void CCSCareerMatchEndMenu::Update()
{
	IN_ScoreUp();
	gEngfuncs.pfnClientCmd( (char*)"-score; -attack; -attack2; -voicerecord\n" );

	bool isCT = true;
	if ( g_pCareerUI != NULL )
		isCT = g_pCareerUI->PlayAsCT();

	int wins = isCT ? TheCareerMatchStatus.ctWins : TheCareerMatchStatus.tWins;
	int losses = isCT ? TheCareerMatchStatus.tWins : TheCareerMatchStatus.ctWins;

	char buf[32];
	sprintf( buf, "#Career_Match_%s", wins > losses ? "Win" : "Lose" );
	m_pResultLabel->SetText( buf );

	if ( g_pCareerUI != NULL )
	{
		g_pCareerUI->OnMatchEndMenuOpen( wins > losses );
	}

	PopulateWinsAndLosses( isCT, m_pWinsLabel, m_pLossesLabel );

	m_pFinishButton->SetCommand( new KeyValues( "Command", "command", "disconnect" ) );

	if ( g_pCareerUI == NULL || !g_pCareerUI->IsPlayingMatch() )
	{
		m_pRetryButton->SetVisible( false );
		m_pFinishButton->SetVisible( false );
	}
	else
	{
		m_pFinishButton->SetVisible( true );

		if ( wins > losses )
		{
			m_pRetryButton->SetVisible( false );
			m_pFinishButton->RequestFocus( 0 );
		}
		else
		{
			m_pRetryButton->SetVisible( true );
			m_pRetryButton->RequestFocus( 0 );
		}
	}

	if ( g_pCareerUI != NULL && wins > losses )
	{
		int pointsGained = g_pCareerUI->GetReputationGained();
		int mapsUnlocked = g_pCareerUI->GetNumMapsUnlocked();
		bool allUnlocked = g_pCareerUI->DoesWinUnlockAll();

		if ( allUnlocked )
		{
			m_pFinishButton->SetCommand( new KeyValues( "Command", "command", "Credits" ) );

			m_pWinsLabel->SetVisible( true );
			m_pLossesLabel->SetVisible( true );
			m_pResultLabel->SetVisible( true );

			m_pNewPointsLabel->SetVisible( false );
			m_pNewLocationsLabel->SetVisible( false );
			m_pCampaignCompleteLabel->SetVisible( true );
			m_pMedalEarnedLabel->SetVisible( true );
			m_pMedalNameLabel->SetVisible( true );

			for ( int i = 0; i < 3; ++i )
			{
				m_pMapCompleteLabel[i]->SetVisible( false );
				m_pMapLabel[i]->SetVisible( false );
				m_pMapPanel[i]->SetVisible( false );
			}

			m_pMedalPanel->SetVisible( true );
			m_pMedalNameLabel->SetVisible( true );
			switch ( g_pCareerUI->GetDifficulty() )
			{
			case CAREER_DIFFICULTY_EASY:
				m_pMedalPanel->setTexture( "gfx/career/medals/Medal.01_Active", true );
				m_pMedalNameLabel->SetText( "#Career_EasyMedal" );
				break;
			case CAREER_DIFFICULTY_NORMAL:
				m_pMedalPanel->setTexture( "gfx/career/medals/Medal.02_Active", true );
				m_pMedalNameLabel->SetText( "#Career_NormalMedal" );
				break;
			case CAREER_DIFFICULTY_HARD:
				m_pMedalPanel->setTexture( "gfx/career/medals/Medal.03_Active", true );
				m_pMedalNameLabel->SetText( "#Career_HardMedal" );
				break;
			case CAREER_DIFFICULTY_EXPERT:
				m_pMedalPanel->setTexture( "gfx/career/medals/Medal.04_Active", true );
				m_pMedalNameLabel->SetText( "#Career_ExpertMedal" );
				break;
			default:
				m_pMedalPanel->SetVisible( false );
				m_pMedalNameLabel->SetVisible( false );
				break;
			}
		}
		else
		{
			m_pWinsLabel->SetVisible( true );
			m_pLossesLabel->SetVisible( true );
			m_pResultLabel->SetVisible( true );

			m_pNewPointsLabel->SetVisible( false );
			m_pNewLocationsLabel->SetVisible( false );
			m_pCampaignCompleteLabel->SetVisible( false );
			m_pMedalEarnedLabel->SetVisible( false );
			m_pMedalNameLabel->SetVisible( false );

			for ( int i = 0; i < 3; ++i )
			{
				m_pMapCompleteLabel[i]->SetVisible( false );
				m_pMapLabel[i]->SetVisible( false );
				m_pMapPanel[i]->SetVisible( false );
			}

			m_pMedalPanel->SetVisible( false );
			m_pMedalNameLabel->SetVisible( false );

			MapInfo mapInfo[3];
			int triplet = g_pCareerUI->GetCurrentMapTriplet( mapInfo );
			if ( mapsUnlocked && triplet >= 0 )
			{
				m_pNewPointsLabel->SetVisible( true );
				m_pNewLocationsLabel->SetVisible( true );

				ShowMap( 0, mapInfo[0].name );
				ShowMap( 1, mapInfo[1].name );
				ShowMap( 2, mapInfo[2].name );
			}
			else if ( pointsGained )
			{
				m_pNewPointsLabel->SetVisible( true );

				int index = 0;
				for ( int i = 0; i < 3; ++i )
				{
					if ( mapInfo[i].defeated )
						ShowMap( index++, mapInfo[i].name );
				}
			}
		}
	}
	else
	{
		m_pWinsLabel->SetVisible( true );
		m_pLossesLabel->SetVisible( true );
		m_pResultLabel->SetVisible( true );

		m_pNewPointsLabel->SetVisible( false );
		m_pNewLocationsLabel->SetVisible( false );
		m_pCampaignCompleteLabel->SetVisible( false );
		m_pMedalEarnedLabel->SetVisible( false );
		m_pMedalNameLabel->SetVisible( false );
		m_pMedalPanel->SetVisible( false );

		for ( int i = 0; i < 3; ++i )
		{
			m_pMapCompleteLabel[i]->SetVisible( false );
			m_pMapLabel[i]->SetVisible( false );
			m_pMapPanel[i]->SetVisible( false );
		}
	}

	gViewPortInterface->CloseTutorTextWindow();
}

void CCSCareerMatchEndMenu::OnKeyCodePressed( vgui2::KeyCode code )
{
	if ( code != vgui2::KEY_ESCAPE )
		CCSCareerBaseMenu::OnKeyCodePressed( code );
}

void CCSCareerMatchEndMenu::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code != vgui2::KEY_ESCAPE )
		CCSCareerBaseMenu::OnKeyCodeTyped( code );
}

void CCSCareerMatchEndMenu::OnCommand( const char* command )
{
	if ( !_stricmp( command, "Close" ) || !_stricmp( command, "Cancel" ) )
		return;

	if ( !_stricmp( command, "Credits" ) )
	{
		ShowCreditsMenu( true );
		OnClose();
	}
	else
	{
		gEngfuncs.pfnClientCmd( (char*)"unpause\n" );
		if ( !_stricmp( command, "vguicancel" ) )
			gViewPortInterface->HideBackGround();
		else
			gEngfuncs.pfnClientCmd( (char*)command );
		OnClose();
		vgui2::Frame::OnCommand( command );
		g_pCareerUI->OnMatchEndMenuClose( strcmp( command, "career_restart" ) == 0 );
	}
}
