#include "vgui_worldmap.h"

#include "vgui/highlightbutton.h"
#include "vgui/vgui_worldmap_missionselect.h"
#include "ImageList.h"
#include "ImagePanel.h"
#include "Label.h"
#include "TextImage.h"
#include "Controls.h"
#include "wrect.h"
#include "cl_dll.h"
#include "FileSystem.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CWorldMap::CWorldMap( vgui2::Panel* parent ) : vgui2::Frame( parent, "WorldMap", true, false )
{
	SetTitle( "", false );
	SetMoveable( false );
	SetSizeable( false );
	SetTitleBarVisible( false );
	SetProportional( true );
	m_pPanel = new vgui2::Panel( this, "ItemInfo" );
	m_pMissionSelect = new CWorldMapMissionSelect( this );
	m_pMissionLabel = new vgui2::Label( this, "MissionLabel", "" );
	m_pResumeButton = new CHighlightButton( this, "Resume", "" );
	m_pResumeButton->SetTextInset( 0, 0 );
	vgui2::Frame::LoadControlSettings( "resource/UI/WorldMap.res", NULL );
}

CWorldMap::~CWorldMap()
{
}

void CWorldMap::UnlockMission( const char* mapName )
{
	MissionStatusHelper( mapName, MISSION_UNLOCKED );
}

void CWorldMap::LockMission( const char* mapName )
{
	MissionStatusHelper( mapName, MISSION_LOCKED );
}

void CWorldMap::CurrentMission( const char* mapName )
{
	MissionStatusHelper( mapName, MISSION_CURRENT );
}

void CWorldMap::MissionStatusHelper( const char* mapName, MissionType_E status )
{
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		CMissionButton* button = dynamic_cast<CMissionButton*>( GetChild( i ) );
		if ( button && strstr( button->GetMapName(), mapName ) )
		{
			button->SetMissionStatus( status );
			button->InvalidateLayout( false, false );
		}
	}
}

CWorldMap::MissionType_E CWorldMap::GetMissionStatus( const char* mapName )
{
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		CMissionButton* button = dynamic_cast<CMissionButton*>( GetChild( i ) );
		if ( button && strstr( button->GetMapName(), mapName ) )
			return button->GetMissionStatus();
	}
	return MISSION_LOCKED;
}

void CWorldMap::Open()
{
	vgui2::Button* resume = dynamic_cast<vgui2::Button*>( FindChildByName( "Resume", false ) );
	if ( resume )
		resume->SetEnabled( stricmp( gEngfuncs.pfnGetLevelName(), "maps/cz_worldmap.bsp" ) != 0 );
	gEngfuncs.pfnClientCmd( "setpause\n" );
	SetVisible( true );
}

void CWorldMap::OnCommand( const char* buttonCommand )
{
	const char* level = gEngfuncs.pfnGetLevelName();
	char* mission = NULL;
	char* map = NULL;
	char command[64] = { 0 };
	char combined[64];
	strncpy( combined, buttonCommand, sizeof( combined ) );
	combined[sizeof( combined ) - 1] = 0;
	char* separator = strstr( combined, ":" );
	if ( separator )
	{
		*separator = 0;
		mission = combined;
		map = separator + 1;
	}
	if ( !stricmp( buttonCommand, "resume" ) || ( level && strstr( level, buttonCommand ) ) )
		_snprintf( command, sizeof( command ), "unpause\n" );
	else if ( stricmp( buttonCommand, "resume" ) && mission && map )
		m_pMissionSelect->Open( mission, map );
	if ( command[0] )
	{
		gEngfuncs.pfnClientCmd( command );
		Close();
	}
}

vgui2::Panel* CWorldMap::CreateControlByName( const char* controlName )
{
	if ( stricmp( "MissionButton", controlName ) )
		return vgui2::EditablePanel::CreateControlByName( controlName );
	ClassHelperPanel* helper = new ClassHelperPanel( this, NULL );
	helper->SetVisible( false );
	int x, y, wide, tall;
	m_pPanel->GetBounds( x, y, wide, tall );
	helper->SetBounds( x, y, wide, tall );
	return new CMissionButton( this, "MissionButton", helper );
}

void CWorldMap::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	SetBgColor( GetSchemeColor( "WorldMap/BgColor", scheme ) );
	m_pMissionLabel->SetFgColor( GetSchemeColor( "WorldMap/MissionLabelFgTextColor", scheme ) );
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	for ( int row = 0; row < BACKGROUND_ROWS; row++ )
	{
		for ( int column = 0; column < BACKGROUND_COLUMNS; column++ )
		{
			bimage_t& image = m_ImageID[row][column];
			image.imageID = vgui2::surface()->CreateNewTextureID( false );
			char filename[512];
			sprintf( filename, "gfx/vgui/worldmap_%d_%d", column, row );
			vgui2::surface()->DrawSetTextureFile( image.imageID, filename, 0, false );
			vgui2::surface()->DrawGetTextureSize( image.imageID, image.width, image.height );
		}
	}
}

void CWorldMap::PaintBackground()
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

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CWorldMap::CMissionButton::CMissionButton( vgui2::Panel* parent, const char* name, vgui2::Panel* helper )
	: MouseOverPanelButton( parent, name, helper )
{
	m_pHelperPanel = dynamic_cast<ClassHelperPanel*>( helper );
	m_Status = MISSION_LOCKED;
	m_pMissionImageList = new vgui2::ImageList( true );
	m_pMissionImageList->SetImageAtIndex( 0, vgui2::scheme()->GetImage( "gfx/vgui/missionunlocked", false ) );
	m_pMissionImageList->SetImageAtIndex( 2, vgui2::scheme()->GetImage( "gfx/vgui/missionlocked", false ) );
	m_pMissionImageList->SetImageAtIndex( 1, vgui2::scheme()->GetImage( "gfx/vgui/missioncurrent", false ) );
}

CWorldMap::CMissionButton::~CMissionButton()
{
}

void CWorldMap::CMissionButton::ApplySettings( KeyValues* settings )
{
	vgui2::Button::ApplySettings( settings );
	KeyValues* map = settings->FindKey( "map", false );
	if ( map )
		strncpy( m_sMapName, map->GetString( NULL, "" ), sizeof( m_sMapName ) );
	else
		m_sMapName[0] = 0;
	char command[64];
	sprintf( command, "%s:%s", GetName(), m_sMapName );
	SetCommand( command );
}

void CWorldMap::CMissionButton::PerformLayout()
{
	vgui2::Button::PerformLayout();
	SetTextImageIndex( 1 );
	int image = m_Status == MISSION_UNLOCKED ? 0 : m_Status == MISSION_CURRENT ? 1 : 2;
	SetImageAtIndex( 0, m_pMissionImageList->GetImage( image ), 0 );
	SetEnabled( m_Status == MISSION_UNLOCKED || m_Status == MISSION_CURRENT );
}

void CWorldMap::CMissionButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	SetFont( scheme->GetFont( "MissionFont", IsProportional() ) );
	vgui2::Button::ApplySchemeSettings( scheme );
	SetDefaultBorder( scheme->GetBorder( "WorldMapMissionButtonBorder" ) );
	SetDepressedBorder( scheme->GetBorder( "WorldMapMissionButtonDepressedBorder" ) );
	SetKeyFocusBorder( scheme->GetBorder( "WorldMapMissionButtonKeyFocusBorder" ) );
	Color background = GetBgColor();
	Color foreground = GetFgColor();
	SetDefaultColor( GetSchemeColor( "WorldMap/MissionButtonFgColor", foreground, scheme ),
		GetSchemeColor( "WorldMap/MissionButtonBgColor", background, scheme ) );
	SetArmedColor( GetSchemeColor( "WorldMap/ButtonArmedFgColor", foreground, scheme ),
		GetSchemeColor( "WorldMap/ButtonArmedBgColor", background, scheme ) );
	SetDepressedColor( GetSchemeColor( "WorldMap/ButtonDepressedFgColor", foreground, scheme ),
		GetSchemeColor( "WorldMap/ButtonDepressedBgColor", background, scheme ) );
}

void CWorldMap::CMissionButton::DrawFocusBorder( int, int, int, int )
{
}

void CWorldMap::CMissionButton::SetMissionStatus( MissionType_E status )
{
	m_Status = status;
	if ( m_pHelperPanel )
		m_pHelperPanel->SetMissionStatus( status );
}

CWorldMap::MissionType_E CWorldMap::CMissionButton::GetMissionStatus()
{
	return m_Status;
}

const char* CWorldMap::CMissionButton::GetMapName()
{
	return m_sMapName;
}

const char* CWorldMap::CMissionButton::GetMissionName()
{
	return GetName();
}

void CWorldMap::CMissionButton::Paint()
{
	vgui2::IImage* image = GetImageAtIndex( 0 );
	vgui2::TextImage* textImage = dynamic_cast<vgui2::TextImage*>( GetImageAtIndex( 1 ) );
	wchar_t text[9];
	textImage->GetText( text, 2 );
	int fontTall = vgui2::surface()->GetFontTall( textImage->GetFont() );
	int descent = fontTall - vgui2::surface()->GetFontAscent( textImage->GetFont(), text[0] );
	int textWide;
	textImage->GetDrawWidth( textWide );
	int imageWide, imageTall;
	image->GetSize( imageWide, imageTall );
	int wide, tall;
	GetSize( wide, tall );
	double scaledHeight = fontTall * 0.6f;
	int scaledTall = (int)(float)scaledHeight;
	int scaledWide = (int)( scaledHeight / imageTall * imageWide );
	int textOffset = vgui2::scheme()->GetProportionalScaledValue( 3 );
	image->SetPos( 0, (int)( tall - scaledTall - descent * 0.9 ) );
	image->SetSize( scaledWide, scaledTall );
	image->Paint();
	int x = textOffset + scaledWide;
	int y = tall - fontTall - 1;
	textImage->SetPos( x, y );
	if ( !IsEnabled() )
	{
		textImage->SetPos( x + 1, y + 1 );
		textImage->SetColor( GetDisabledFgColor1() );
		textImage->Paint();
		vgui2::surface()->DrawFlushText();
		textImage->SetPos( x, y );
		textImage->SetColor( GetDisabledFgColor2() );
	}
	else
	{
		textImage->SetColor( GetFgColor() );
	}
	textImage->Paint();
}

CWorldMap::ClassHelperPanel::ClassHelperPanel( vgui2::Panel* parent, const char* name )
	: vgui2::EditablePanel( parent, name ), m_pMissionTitle( NULL ), m_pMissionOverview( NULL ),
	m_Status( MISSION_LOCKED )
{
	m_pMissionTitle = new vgui2::Label( this, "MissionTitle", "" );
	m_pMissionOverview = new vgui2::Label( this, "MissionOverview", "" );
}

const char* CWorldMap::ClassHelperPanel::GetClassPage( const char* className )
{
	static char classPanel[4096];
	_snprintf( classPanel, sizeof( classPanel ), "classes/%s.res", className );
	if ( vgui2::filesystem()->FileExists( classPanel ) )
		return classPanel;
	if ( !vgui2::filesystem()->FileExists( "classes/default.res" ) )
		return NULL;
	strcpy( classPanel, "classes/default.res" );
	return classPanel;
}

void CWorldMap::ClassHelperPanel::SetMissionStatus( MissionType_E status )
{
	m_Status = status;
}

void CWorldMap::ClassHelperPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	LoadControlSettings( GetClassPage( GetName() ), NULL );
	m_pMissionTitle->SetFgColor( GetSchemeColor( "WorldMap/MissionTitleFgColor", scheme ) );
	m_pMissionOverview->SetFgColor( GetSchemeColor( "WorldMap/MSLabelFgColor", scheme ) );
	vgui2::Panel::ApplySchemeSettings( scheme );
}

void CWorldMap::ClassHelperPanel::PerformLayout()
{
	vgui2::EditablePanel::PerformLayout();
	vgui2::ImagePanel* unlocked = dynamic_cast<vgui2::ImagePanel*>( FindChildByName( "Unlocked", false ) );
	vgui2::ImagePanel* locked = dynamic_cast<vgui2::ImagePanel*>( FindChildByName( "Locked", false ) );
	bool available = (unsigned int)m_Status < MISSION_LOCKED;
	if ( unlocked )
		unlocked->SetPaintEnabled( available );
	if ( locked )
		locked->SetPaintEnabled( !available );
}
