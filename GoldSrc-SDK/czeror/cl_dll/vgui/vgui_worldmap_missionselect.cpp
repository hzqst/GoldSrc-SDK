#include "vgui_worldmap_missionselect.h"

#include "highlightbutton.h"
#include "missionradiobutton.h"
#include "ImagePanel.h"
#include "Label.h"
#include "Controls.h"
#include "wrect.h"
#include "cl_dll.h"
#include "FileSystem.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef PostMessage
#undef PostMessage
#endif

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CWorldMapMissionSelect::CWorldMapMissionSelect( vgui2::Panel* parent )
	: vgui2::Frame( parent, "MissionSelect", true, false )
{
	SetHDProportional( HD_PROPORTIONAL_FORCE_DISABLED );
	SetTitle( "", false );
	SetMoveable( false );
	SetSizeable( false );
	SetTitleBarVisible( false );
	SetProportional( true );
	m_pWorldMapImage00 = new vgui2::ImagePanel( this, "WorldMapImage00" );
	m_pWorldMapImage10 = new vgui2::ImagePanel( this, "WorldMapImage10" );
	m_pWorldMapImage20 = new vgui2::ImagePanel( this, "WorldMapImage20" );
	m_pWorldMapImage01 = new vgui2::ImagePanel( this, "WorldMapImage01" );
	m_pWorldMapImage11 = new vgui2::ImagePanel( this, "WorldMapImage11" );
	m_pWorldMapImage21 = new vgui2::ImagePanel( this, "WorldMapImage21" );
	m_pMissionLabel = new vgui2::Label( this, "MissionLabel", "" );
	m_pLocaleLabel = new vgui2::Label( this, "LocaleLabel", "" );
	m_pTimeLabel = new vgui2::Label( this, "TimeLabel", "" );
	m_pOrganizationLabel = new vgui2::Label( this, "Organizationlabel", "" );
	m_pOverviewLabel = new vgui2::Label( this, "OverviewLabel", "" );
	m_pObjectiveLabel = new vgui2::Label( this, "ObjectiveLabel", "" );
	m_pInfoPanel = new DataPanel( this, "ItemInfo" );
	m_pEasyButton = new CMissionRadioButton( this, "Easy", "" );
	m_pNormalButton = new CMissionRadioButton( this, "Normal", "" );
	m_pHardButton = new CMissionRadioButton( this, "Hard", "" );
	m_pOkayButton = new CHighlightButton( this, "Okay", "" );
	m_pCancelButton = new CHighlightButton( this, "Cancel", "" );
	m_pOkayButton->SetTextInset( 0, 0 );
	m_pCancelButton->SetTextInset( 0, 0 );
	vgui2::Frame::LoadControlSettings( "resource/UI/WorldMapMissionSelect.res", NULL );
}

CWorldMapMissionSelect::~CWorldMapMissionSelect()
{
}

const char* CWorldMapMissionSelect::GetClassPage( const char* className )
{
	static char classPanel[4096];
	_snprintf( classPanel, sizeof( classPanel ), "classes/%s_detailed.res", className );
	if ( vgui2::filesystem()->FileExists( classPanel ) )
		return classPanel;
	if ( !vgui2::filesystem()->FileExists( "classes/default_detailed.res" ) )
		return NULL;
	strcpy( classPanel, "classes/default_detailed.res" );
	return classPanel;
}

void CWorldMapMissionSelect::Open( const char* missionName, const char* mapName )
{
	strncpy( m_szMapName, mapName, sizeof( m_szMapName ) );
	float skill = gEngfuncs.pfnGetCvarFloat( "skill" );
	m_pEasyButton->SetSelected( (int)skill <= 1 );
	m_pNormalButton->SetSelected( (int)skill == 2 );
	m_pHardButton->SetSelected( (int)skill > 2 );
	m_pInfoPanel->LoadControlSettings( GetClassPage( missionName ), NULL );
	m_pInfoPanel->InvalidateLayout( false, true );
	Activate();
}

void CWorldMapMissionSelect::OnCommand( const char* command )
{
	if ( !stricmp( command, "okay" ) )
	{
		int skill;
		if ( m_pEasyButton->IsSelected() )
			skill = 1;
		else if ( m_pNormalButton->IsSelected() )
			skill = 2;
		else
			skill = m_pHardButton->IsSelected() ? 3 : 0;
		const char* level = gEngfuncs.pfnGetLevelName();
		char mapCommand[64] = { 0 };
		_snprintf( mapCommand, sizeof( mapCommand ), level ? "skill %i\nchangelevel %s\n" : "skill %i\nmap %s\n",
			skill, m_szMapName );
		gEngfuncs.pfnClientCmd( mapCommand );
		vgui2::ivgui()->PostMessage( GetParent()->GetVPanel(), new KeyValues( "Close" ), GetVPanel(), 0.0f );
	}
	Close();
}

void CWorldMapMissionSelect::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	GetBgColor();
	GetFgColor();
	m_pMissionLabel->SetFgColor( GetSchemeColor( "Mission/MissionLabelFgTextColor", scheme ) );
	m_pLocaleLabel->SetFgColor( GetSchemeColor( "Mission/MSLabelFgColor", scheme ) );
	m_pTimeLabel->SetFgColor( GetSchemeColor( "Mission/MSLabelFgColor", scheme ) );
	m_pOrganizationLabel->SetFgColor( GetSchemeColor( "Mission/MSLabelFgColor", scheme ) );
	m_pOverviewLabel->SetFgColor( GetSchemeColor( "Mission/MSLabelFgColor", scheme ) );
	m_pObjectiveLabel->SetFgColor( GetSchemeColor( "Mission/MSLabelFgColor", scheme ) );
	SetBgColor( GetSchemeColor( "Mission/BgColorMission", scheme ) );
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	for ( int row = 0; row < BACKGROUND_ROWS; row++ )
	{
		for ( int column = 0; column < BACKGROUND_COLUMNS; column++ )
		{
			bimage_t& image = m_ImageID[row][column];
			image.imageID = vgui2::surface()->CreateNewTextureID( false );
			char filename[512];
			sprintf( filename, "gfx/vgui/worldmap2_%d_%d", column, row );
			vgui2::surface()->DrawSetTextureFile( image.imageID, filename, 0, false );
			vgui2::surface()->DrawGetTextureSize( image.imageID, image.width, image.height );
		}
	}
}

void CWorldMapMissionSelect::PaintBackground()
{
	vgui2::Frame::PaintBackground();
	int wide, tall, screenWide, screenTall;
	GetSize( wide, tall );
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	double xScale = screenWide / 640.0f;
	double yScale = screenTall / 480.0f;
	int y = 0;
	for ( int row = 0; row < BACKGROUND_ROWS; row++ )
	{
		int x = 0;
		for ( int column = 0; column < BACKGROUND_COLUMNS; column++ )
		{
			bimage_t& image = m_ImageID[row][column];
			int imageWide = (int)ceil( image.width * xScale );
			int imageTall = (int)ceil( image.height * yScale );
			vgui2::surface()->DrawSetColor( 255, 255, 255, 255 );
			vgui2::surface()->DrawSetTexture( image.imageID );
			vgui2::surface()->DrawTexturedRect( x, y, x + imageWide, y + imageTall );
			x += (int)ceil( image.width * xScale );
		}
		y += (int)ceil( m_ImageID[row][0].height * yScale );
	}
}

CWorldMapMissionSelect::DataPanel::DataPanel( vgui2::Panel* parent, const char* name )
	: vgui2::EditablePanel( parent, name )
{
	m_pMissionTitle = new vgui2::Label( this, "MissionTitle", "" );
	m_pMissionTime = new vgui2::Label( this, "MissionTime", "" );
	m_pMissionLocation = new vgui2::Label( this, "MissionLocale", "" );
	m_pMissionOrganization = new vgui2::Label( this, "MissionOrganization", "" );
	m_pMissionOverview = new vgui2::Label( this, "MissionOverview", "" );
	m_pMissionObjective = new vgui2::Label( this, "MissionObjective", "" );
}

void CWorldMapMissionSelect::DataPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	m_pMissionTitle->SetFgColor( GetSchemeColor( "Mission/MissionTitleFgColor", scheme ) );
	m_pMissionTime->SetFgColor( GetSchemeColor( "Mission/MSLabelFgColor", scheme ) );
	m_pMissionLocation->SetFgColor( GetSchemeColor( "Mission/MSLabelFgColor", scheme ) );
	m_pMissionOrganization->SetFgColor( GetSchemeColor( "Mission/MSLabelFgColor", scheme ) );
	m_pMissionOverview->SetFgColor( GetSchemeColor( "Mission/MSLabelFgColor", scheme ) );
	m_pMissionObjective->SetFgColor( GetSchemeColor( "Mission/MSLabelFgColor", scheme ) );
	vgui2::Panel::ApplySchemeSettings( scheme );
}
