#include "CareerMapFrame.h"
#include "CareerGame.h"
#include "DottedLabel.h"
#include "GameUI_Interface.h"
#include "ImageButton.h"
#include "Taskbar.h"

#include "Button.h"
#include "Label.h"
#include "MessageBox.h"
#include "RichText.h"
#include "ScrollBar.h"
#include "TextImage.h"
#include "../vgui2/controls/BitmapImagePanel.h"
#include "../game_shared/shared_util.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#define _snprintf snprintf
#endif

#ifdef MessageBox
#undef MessageBox
#endif

#define MAX_CAREER_MAP_DESCRIPTION 2048

class CMapButton : public CImageButton
{
public:
	CMapButton( CCareerMapFrame* parent, const char* panelName, const char* text, const char* mapName );
	~CMapButton() override;
	void SetActive( bool active );
	void SetDefeated( bool defeated );
	void SetMapName( const char* mapName );
	void IgnoreDoubleClick() { m_ignoreDoubleClicks = true; }
	Color GetBgColor() override;
	Color GetImageColor() override;

protected:
	void OnMousePressed( vgui2::MouseCode code ) override;
	void OnMouseDoublePressed( vgui2::MouseCode code ) override;
	void PaintBorder() override;
	void DrawFocusBorder( int x0, int y0, int x1, int y1 ) override;
	void PaintBackground() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

private:
	enum
	{
		IMAGE_UNLOCKED = 0,
		IMAGE_LOCKED,
		IMAGE_MAX
	};

	struct bimage_t
	{
		int	imageID;
		int	width;
		int	height;
	};

	CCareerMapFrame*		m_pParent;
	Color					m_activeColor;
	Color					m_inactiveColor;
	Color					m_defeatedColor;
	Color					m_armedColor;
	Color					m_depressedColor;
	Color					m_disabledColor;
	bool					m_bActive;
	bool					m_bDefeated;
	char*					m_pMapName;
	bool					m_ignoreDoubleClicks;
	vgui2::TextImage*		m_pDefeatedText;
	bimage_t				m_ImageID[IMAGE_MAX];
};

void ShowCareerMapDescription( vgui2::RichText* text, CCareerMap* map, Color normalColor, Color friendlyFireColor )
{
	if ( !text )
		return;
	if ( !map )
	{
		text->SetText( L"" );
		return;
	}
	wchar_t description[MAX_CAREER_MAP_DESCRIPTION] = { 0 };
	int enemyCount = map->GetCurrentEnemyCount();
	char count[6] = { 0 };
	wchar_t wideCount[6];
#ifdef _WIN32
	_snprintf( count, sizeof( count ), "%d", enemyCount );
#else
	snprintf( count, sizeof( count ), "%d", enemyCount );
#endif
	count[sizeof( count ) - 1] = 0;
	vgui2::localize()->ConvertANSIToUnicode( count, wideCount, sizeof( wideCount ) );
	const wchar_t* singleEnemy = enemyCount == 1 ? vgui2::localize()->Find( "#Career_EnemyListSingle" ) : NULL;
	if ( singleEnemy )
	{
		wcsncat( description, singleEnemy, MAX_CAREER_MAP_DESCRIPTION - wcslen( description ) - 1 );
	}
	else
	{
		wchar_t* format = vgui2::localize()->Find( "#Career_EnemyList" );
		vgui2::localize()->ConstructString( description, MAX_CAREER_MAP_DESCRIPTION, format, 1, wideCount );
	}
	wcsncat( description, L"\n", MAX_CAREER_MAP_DESCRIPTION - wcslen( description ) - 1 );
	const CharacterVec& bots = map->GetBotList();
	int botsSeen = 0;
	for ( CharacterVec::const_iterator it = bots.begin(); it != bots.end() && botsSeen < enemyCount; ++it )
	{
#ifdef _WIN32
		if ( botsSeen )
		{
			const wchar_t* separator = vgui2::localize()->Find( "#Career_EnemyListSeparator" );
			wcsncat( description, separator, MAX_CAREER_MAP_DESCRIPTION - wcslen( description ) - 1 );
		}
#endif
		char name[64];
		wchar_t wideName[64];
#ifdef _WIN32
		_snprintf( name, sizeof( name ), " %s", ( *it )->GetName() );
#else
		snprintf( name, sizeof( name ), " %s", ( *it )->GetName() );
#endif
		name[sizeof( name ) - 1] = 0;
		vgui2::localize()->ConvertANSIToUnicode( name, wideName, sizeof( wideName ) );
		wcsncat( description, wideName, MAX_CAREER_MAP_DESCRIPTION - wcslen( description ) - 1 );
		++botsSeen;
#ifndef _WIN32
		if ( it + 1 != bots.end() && botsSeen < enemyCount )
		{
			const wchar_t* separator = vgui2::localize()->Find( "#Career_EnemyListSeparator" );
			wcsncat( description, separator, MAX_CAREER_MAP_DESCRIPTION - wcslen( description ) - 1 );
		}
#endif
	}
	wcsncat( description, L"\n\n", MAX_CAREER_MAP_DESCRIPTION - wcslen( description ) - 1 );
	wcsncat( description, vgui2::localize()->Find( "#Career_TasksColon" ),
			MAX_CAREER_MAP_DESCRIPTION - wcslen( description ) - 1 );
	wcsncat( description, L"\n", MAX_CAREER_MAP_DESCRIPTION - wcslen( description ) - 1 );
	ITaskVec* tasks = map->GetTasks();
	for ( ITaskVec::iterator it = tasks->begin(); it != tasks->end(); ++it )
	{
		if ( !wcslen( ( *it )->GetLocalizedTaskNameWithCompletion() ) )
			continue;
		wcsncat( description, ( *it )->GetLocalizedTaskNameWithCompletion(),
				MAX_CAREER_MAP_DESCRIPTION - wcslen( description ) - 1 );
		wcsncat( description, L"\n", MAX_CAREER_MAP_DESCRIPTION - wcslen( description ) - 1 );
	}
	text->SetText( L"" );
	text->InsertColorChange( normalColor );
	text->InsertString( description );
	if ( !map->AreTeammatesAllowed() )
	{
		text->InsertColorChange( friendlyFireColor );
		text->InsertString( "#Career_NoTeammates" );
	}
	else if ( map->IsFriendlyFireOn() )
	{
		text->InsertColorChange( friendlyFireColor );
		text->InsertString( "#Career_FriendlyFireOn" );
	}
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
CMapButton::CMapButton( CCareerMapFrame* parent, const char* panelName, const char* text, const char* mapName )
	: CImageButton( parent, panelName, text )
{
	m_pParent = parent;
	m_bActive = false;
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
	m_activeColor = GetSchemeColor( "MapActiveBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	m_disabledColor = GetSchemeColor( "MapDisabledBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	m_armedColor = GetSchemeColor( "MapArmedBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	m_depressedColor = GetSchemeColor( "MapDepressedBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	m_inactiveColor = GetSchemeColor( "MapInactiveBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	m_defeatedColor = GetSchemeColor( "MapDefeatedBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	if ( mapName )
	{
		m_pMapName = new char[strlen( mapName ) + 1];
		strcpy( m_pMapName, mapName );
	}
	else
	{
		m_pMapName = new char[1];
		m_pMapName[0] = 0;
	}
	m_ignoreDoubleClicks = false;
	m_ImageID[IMAGE_UNLOCKED].imageID = vgui2::surface()->CreateNewTextureID( false );
	vgui2::surface()->DrawSetTextureFile( m_ImageID[IMAGE_UNLOCKED].imageID, "gfx/career/Checkbox_Unlocked", 0, false );
	vgui2::surface()->DrawGetTextureSize( m_ImageID[IMAGE_UNLOCKED].imageID,
		m_ImageID[IMAGE_UNLOCKED].width, m_ImageID[IMAGE_UNLOCKED].height );
	m_ImageID[IMAGE_LOCKED].imageID = vgui2::surface()->CreateNewTextureID( false );
	vgui2::surface()->DrawSetTextureFile( m_ImageID[IMAGE_LOCKED].imageID, "gfx/career/Lock", 0, false );
	vgui2::surface()->DrawGetTextureSize( m_ImageID[IMAGE_LOCKED].imageID,
		m_ImageID[IMAGE_LOCKED].width, m_ImageID[IMAGE_LOCKED].height );
	m_ImageID[IMAGE_LOCKED].width = GetProportionalScaledValue( m_ImageID[IMAGE_LOCKED].width );
	m_ImageID[IMAGE_LOCKED].height = GetProportionalScaledValue( m_ImageID[IMAGE_LOCKED].height );
	m_pDefeatedText = new vgui2::TextImage( "" );
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CMapButton::~CMapButton()
{
	delete[] m_pMapName;
	delete m_pDefeatedText;
}

void CMapButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Button::ApplySchemeSettings( scheme );
	SetArmedSound( "sound/UI/buttonrollover.wav" );
	SetDepressedSound( "sound/UI/buttonclick.wav" );
	SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
	m_pDefeatedText->SetText( "#Career_Secured" );
	m_pDefeatedText->SetColor( GetSchemeColor( "SelectedMap", GetSchemeColor( "ControlBG", scheme ), scheme ) );
	bool hd = IsHDProportional();
	m_pDefeatedText->SetFont( scheme->GetFontEx( "MapComplete", IsProportional(), hd ) );
	m_pDefeatedText->ResizeImageToContent();
}

void CMapButton::PaintBackground()
{
	CImageButton::PaintBackground();
	if ( !TheCareerGame->GetPlayableMap( m_pMapName ) )
	{
		int wide, tall;
		GetSize( wide, tall );
		int x = wide - 2 - m_ImageID[IMAGE_LOCKED].width;
		int y = tall - 2 - m_ImageID[IMAGE_LOCKED].height;
		if ( m_bActive )
		{
			x -= 4;
			y -= 4;
		}
		vgui2::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui2::surface()->DrawSetTexture( m_ImageID[IMAGE_LOCKED].imageID );
		vgui2::surface()->DrawTexturedRect( x, y, x + m_ImageID[IMAGE_LOCKED].width,
			y + m_ImageID[IMAGE_LOCKED].height );
	}
	else if ( m_bDefeated )
	{
		int wide, tall, textWide, textTall;
		GetSize( wide, tall );
		m_pDefeatedText->GetSize( textWide, textTall );
		m_pDefeatedText->SetPos( ( wide - textWide ) / 2, ( tall - textTall ) / 2 );
		m_pDefeatedText->Paint();
	}
}

void CMapButton::PaintBorder()
{
	vgui2::Panel::PaintBorder();
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
	Color color = scheme->GetColor( IsArmed() || m_bActive ? "SelectedMap" : "BorderBright", Color( 0, 0, 0, 0 ) );
	vgui2::surface()->DrawSetColor( color );
	int border = m_bActive ? 4 : 1;
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	vgui2::surface()->DrawFilledRect( 0, 0, wide, border );
	vgui2::surface()->DrawFilledRect( 0, 0, border, tall );
	vgui2::surface()->DrawFilledRect( wide - border, 0, wide, tall );
	vgui2::surface()->DrawFilledRect( 0, tall - border, wide, tall );
}

void CMapButton::DrawFocusBorder( int, int, int, int )
{
}

void CMapButton::OnMousePressed( vgui2::MouseCode code )
{
	if ( !IsEnabled() )
		return;
	if ( code == vgui2::MOUSE_LEFT )
		m_ignoreDoubleClicks = false;
	vgui2::Button::OnMousePressed( code );
}

void CMapButton::OnMouseDoublePressed( vgui2::MouseCode code )
{
	if ( !IsEnabled() )
		return;
	if ( code == vgui2::MOUSE_LEFT && !m_ignoreDoubleClicks )
	{
		if ( m_pParent && TheCareerGame->GetPlayableMap( m_pMapName ) )
		{
			TheCareerGame->SetLastMap( m_pMapName );
			m_pParent->OnCommand( "next" );
			return;
		}
		m_ignoreDoubleClicks = false;
	}
	vgui2::Button::OnMouseDoublePressed( code );
}

Color CMapButton::GetImageColor()
{
	if ( !TheCareerGame->GetPlayableMap( m_pMapName ) || !IsEnabled() )
		return m_disabledColor;
	if ( IsDepressed() )
		return m_depressedColor;
	if ( IsArmed() )
		return m_armedColor;
	if ( m_bDefeated )
		return m_defeatedColor;
	return m_bActive ? m_activeColor : m_inactiveColor;
}

Color CMapButton::GetBgColor()
{
	if ( !TheCareerGame->GetPlayableMap( m_pMapName ) || !IsEnabled() )
		return m_disabledColor;
	if ( IsDepressed() )
		return m_depressedColor;
	if ( IsArmed() )
		return m_armedColor;
	if ( m_bActive )
		return m_activeColor;
	return m_bDefeated ? m_defeatedColor : m_inactiveColor;
}

void CMapButton::SetMapName( const char* mapName )
{
	delete[] m_pMapName;
	if ( mapName )
	{
		m_pMapName = new char[strlen( mapName ) + 1];
		strcpy( m_pMapName, mapName );
	}
	else
	{
		m_pMapName = new char[1];
		m_pMapName[0] = 0;
	}
}

void CMapButton::SetDefeated( bool defeated )
{
	m_bDefeated = defeated;
	InvalidateLayout( false, false );
}

void CMapButton::SetActive( bool active )
{
	if ( active )
	{
		SetFillPadding( 1 );
		m_pParent->ShowMapDescription( TheCareerGame->GetMap( m_pMapName ) );
		if ( m_pParent && TheCareerGame->GetPlayableMap( m_pMapName ) )
			TheCareerGame->SetLastMap( m_pMapName );
		m_pParent->GetNextButton()->SetEnabled( TheCareerGame->GetPlayableMap( m_pMapName ) != NULL );
	}
	else
		SetFillPadding( 4 );
	m_bActive = active;
	InvalidateLayout( false, false );
}

vgui2::MessageMapItem_t CCareerMapFrame::m_MessageMap[1] =
{
	{ "ScrollBarSliderMoved",
		(vgui2::MessageFunc_t)(void ( vgui2::Panel::* )( int ))&CCareerMapFrame::OnSliderMoved,
		1, vgui2::DATATYPE_INT, "position", vgui2::DATATYPE_VOID, NULL, 0, 0, 0 }
};

vgui2::PanelMap_t CCareerMapFrame::m_PanelMap =
{
	m_MessageMap, 1, "CCareerMapFrame", &vgui2::Panel::m_PanelMap, 0
};

vgui2::PanelMap_t* CCareerMapFrame::GetPanelMap()
{
	return &m_PanelMap;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
CCareerMapFrame::CCareerMapFrame( vgui2::Panel* parent )
	: CCareerBaseFrame( parent, "CareerMapFrame", false )
{
	SetTitle( "#Career_Map_Frame", true );
	memset( m_pMapButton, 0, sizeof( m_pMapButton ) );
	m_pMapSelectionBackground = new CDottedBgLabel( this, "MapSelectionBackground", "", 0, 0, 0 );
	m_pMapSelectionBackground->SetTextInset( 2, 2 );
	m_pMapDescriptionBackground = new CDottedBgLabel( this, "MapDescriptionBackground", "", 0, 0, 0 );
	m_pMapDescriptionBackground->SetTextInset( 2, 2 );
	m_pMapDesc = new vgui2::RichText( this, "LabelMapDesc", true );
	m_pMapDesc->SetVerticalScrollbar( false );
	m_pMapName = new vgui2::Label( this, "LabelMapName", "" );
	m_pNextButton = new CCareerButton( this, "ButtonNext", "#Career_MapNext", "gfx/career/icon_forward", true );
	m_pPlayButton = new CCareerButton( this, "ButtonPlay", "#Career_Start", "gfx/career/icon_forward", true );
	m_pCancelButton = new CCareerButton( this, "ButtonCancel", "#Career_Cancel", "gfx/career/icon_cancel", false );
	if ( m_buttons.FindElement( m_pNextButton ) < 0 )
		m_buttons.AddElement( m_pNextButton );
	if ( m_buttons.FindElement( m_pPlayButton ) < 0 )
		m_buttons.AddElement( m_pPlayButton );
	if ( m_buttons.FindElement( m_pCancelButton ) < 0 )
		m_buttons.AddElement( m_pCancelButton );

	char name[64];
	for ( int column = 0; column < 3; ++column )
	{
		_snprintf( name, sizeof( name ), "TripletBorder%d", column );
		if ( column == 1 )
		{
			CDottedBgLabel* border = new CDottedBgLabel( this, name, "", 1000, 0, 0 );
			vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
			border->SetColor( scheme->GetColor( "ActiveTriplet", Color( 0, 0, 0, 255 ) ) );
			m_pTripletBorder[column] = border;
		}
		else
			m_pTripletBorder[column] = new vgui2::Label( this, name, "" );
		_snprintf( name, sizeof( name ), "TripletStatus%d", column );
		m_pTripletStatus[column] = new vgui2::Label( this, name, "" );
	}
	for ( int column = 0; column < 3; ++column )
	{
		for ( int row = 0; row < 3; ++row )
		{
			_snprintf( name, sizeof( name ), "Map_%d_%d", column, row );
			CMapButton* button = new CMapButton( this, name, "", "" );
			button->SetContentAlignment( vgui2::Label::a_northwest );
			button->SetEnabled( false );
			button->SetTabPosition( 0 );
			button->SetImagePadding( 1 );
			button->SetFillPadding( 4 );
			m_pMapButton[column][row] = button;
		}
	}
	m_pMapScroll = new vgui2::ScrollBar( this, "MapScrollbar", false );
	vgui2::Button* button = m_pMapScroll->GetButton( 0 );
	if ( m_buttons.FindElement( button ) < 0 )
		m_buttons.AddElement( button );
	button = m_pMapScroll->GetButton( 1 );
	if ( m_buttons.FindElement( button ) < 0 )
		m_buttons.AddElement( button );
	new vgui2::CBitmapImagePanel( this, "TitlePanel", "gfx/career/Logo_CSCZ_Big" );
	new vgui2::CBitmapImagePanel( this, "SelectLocationPanel", "gfx/career/hed_selectlocation" );
	m_pMapImage = new vgui2::CBitmapImagePanel( this, "MapImagePanel", "resource/game_menu" );
	for ( int difficulty = 0; difficulty < MAX_CAREER_DIFFICULTY; ++difficulty )
	{
		_snprintf( name, sizeof( name ), "MedalPanel%d", difficulty );
		m_pMedalImage[difficulty] = new vgui2::CBitmapImagePanel( this, name, "" );
	}
	vgui2::Frame::LoadControlSettings( "Resource/CareerMapFrame.res", NULL );
	m_numTriplets = (int)TheCareerGame->GetMaps().size();
	m_pMapScroll->SetVisible( true );
	m_pMapScroll->AddActionSignalTarget( this );
	m_pMapScroll->SetRange( 0, m_numTriplets );
	m_pMapScroll->SetRangeWindow( 1 );
	m_pMapScroll->SetButtonPressedScrollValue( 1 );
	m_pMapScroll->SetSize( m_pMapScroll->GetWide(), 18 );
	m_lastTripletActive = -1;
	m_activeMapOffset = 0;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CCareerMapFrame::~CCareerMapFrame()
{
}

vgui2::Button* CCareerMapFrame::GetNextButton()
{
	if ( m_pNextButton && m_pNextButton->IsVisible() )
		return m_pNextButton;
	return m_pPlayButton;
}

void CCareerMapFrame::ShowMapDescription( CCareerMap* map )
{
	if ( !map )
	{
		m_pMapDesc->SetText( L"" );
		return;
	}
	if ( map->AreTeammatesAllowed() )
	{
		m_pNextButton->SetVisible( true );
		m_pPlayButton->SetVisible( false );
	}
	else
	{
		m_pNextButton->SetVisible( false );
		m_pPlayButton->SetVisible( true );
	}
	char name[128];
	_snprintf( name, sizeof( name ), "#Career_Map_%s_CAPS", map->GetName() );
	if ( !vgui2::localize()->Find( name ) )
		_snprintf( name, sizeof( name ), "%s", map->GetName() );
	m_pMapName->SetText( name );
	_snprintf( name, sizeof( name ), "gfx/thumbnails/maps_wide/%s", map->GetName() );
	m_pMapImage->setTexture( name, true );
	ShowCareerMapDescription( m_pMapDesc, map, m_mapDescriptionColor, m_friendlyFireColor );
}

void CCareerMapFrame::ChangeActiveMap( int right, int down )
{
	m_pMapScroll->SetValue( m_pMapScroll->GetValue() + right );
	for ( int column = 0; column < 3; ++column )
	{
		for ( int row = 0; row < 3; ++row )
		{
			if ( m_pMapButton[column][row] )
				m_pMapButton[column][row]->SetActive( false );
		}
	}
	m_activeMapOffset += down;
	if ( m_activeMapOffset > 2 )
		m_activeMapOffset = 2;
	if ( m_activeMapOffset < 0 )
		m_activeMapOffset = 0;
	m_pMapButton[1][m_activeMapOffset]->SetActive( true );
	GetNextButton()->RequestFocus( 0 );
}

void CCareerMapFrame::Activate()
{
	m_numTriplets = (int)TheCareerGame->GetMaps().size();
	m_pMapScroll->SetRange( 0, m_numTriplets );
	SetPos( 0, 0 );
	CCareerBaseFrame::Activate();
	GetNextButton()->RequestFocus( 0 );
	m_lastTripletActive = -1;
	int triplet = TheCareerGame->GetMapTripletNum( TheCareerGame->GetLastMap() );
	int offset = TheCareerGame->GetMapTripletOffset( TheCareerGame->GetLastMap() );
	m_pMapScroll->SetValue( triplet );
	OnSliderMoved( triplet );
	m_activeMapOffset = offset < 0 ? 0 : offset;
	m_doPostActivate = true;
	for ( int column = 0; column < 3; ++column )
	{
		for ( int row = 0; row < 3; ++row )
		{
			if ( m_pMapButton[column][row] )
				m_pMapButton[column][row]->SetActive( false );
		}
	}
	m_pMapButton[1][m_activeMapOffset]->SetActive( true );
	char image[64];
	for ( int difficulty = 0; difficulty < MAX_CAREER_DIFFICULTY; ++difficulty )
	{
		_snprintf( image, sizeof( image ), "gfx/career/medals/Medal.0%d_%s", difficulty + 1,
			TheCareerGame->IsMedalEarned( (CareerDifficultyType)difficulty ) ? "Active" : "Inactive" );
		m_pMedalImage[difficulty]->setTexture( image, true );
		m_pMedalImage[difficulty]->SetVisible( true );
		m_pMedalImage[difficulty]->MoveToFront();
	}
}

void CCareerMapFrame::OnCommand( const char* command )
{
	if ( !SharedTokenWaiting( command ) )
		return;
	const char* data = SharedParse( command );
	const char* token = SharedGetToken();
	if ( !stricmp( command, "close" ) )
	{
		Close();
		engine->pfnResetTutorMessageDecayData();
		g_pTaskbar->OnOpenNewCareerNameDialog();
	}
	else if ( !strcmp( token, "map" ) )
	{
		data = SharedParse( data );
		const char* enabled = SharedGetToken();
		SharedParse( data );
		const char* mapName = SharedGetToken();
		if ( !enabled || !mapName )
			return;
		int triplet = TheCareerGame->GetMapTripletNum( mapName );
		if ( triplet < 0 )
			return;
		m_pMapScroll->SetValue( triplet );
		for ( int column = 0; column < 3; ++column )
		{
			for ( int row = 0; row < 3; ++row )
			{
				if ( m_pMapButton[column][row] )
					m_pMapButton[column][row]->SetActive( false );
			}
		}
		int offset = TheCareerGame->GetMapTripletOffset( mapName );
		m_activeMapOffset = offset < 0 ? 0 : offset;
		m_pMapButton[1][m_activeMapOffset]->SetActive( true );
		GetNextButton()->RequestFocus( 0 );
	}
	else if ( !strcmp( command, "next" ) )
	{
		Career_Printf( "Going to team page using map [%s]\n", TheCareerGame->GetLastMap() );
		Close();
		if ( TheCareerGame->GetLastMapPtr()->AreTeammatesAllowed() )
			g_pTaskbar->OnOpenCareerBotDialog();
		else
			TheCareerGame->SendMatchCommand();
	}
	else
		Career_Printf( "Unknown command [%s] with map %s selected\n", command, TheCareerGame->GetLastMap() );
	vgui2::Frame::OnCommand( command );
}

void CCareerMapFrame::OnKeyCodeTyped( vgui2::KeyCode code )
{
	switch ( code )
	{
	case vgui2::KEY_ESCAPE:
		OnCommand( "close" );
		break;
	case vgui2::KEY_ENTER:
		vgui2::Frame::OnKeyCodeTyped( vgui2::KEY_SPACE );
		break;
	case vgui2::KEY_RIGHT:
		ChangeActiveMap( 1, 0 );
		break;
	case vgui2::KEY_LEFT:
		ChangeActiveMap( -1, 0 );
		break;
	case vgui2::KEY_UP:
		ChangeActiveMap( 0, -1 );
		break;
	case vgui2::KEY_DOWN:
		ChangeActiveMap( 0, 1 );
		break;
	default:
		vgui2::Frame::OnKeyCodeTyped( code );
		break;
	}
}

void CCareerMapFrame::OnThink()
{
	CCareerBaseFrame::OnThink();
	if ( m_doPostActivate )
	{
		m_doPostActivate = false;
		m_lastTripletActive = -1;
		OnSliderMoved( m_pMapScroll->GetValue() );
	}
	if ( !m_numTriplets || !TheCareerGame->AreChecksumsOk() )
	{
		OnCommand( "Close" );
		const wchar_t* title = vgui2::localize()->Find( "#Career_CannotStartGameTitle" );
		const wchar_t* text = vgui2::localize()->Find( "#Career_CorruptMissionDataText" );
		vgui2::MessageBox* error = new vgui2::MessageBox( title ? title : L"Cannot start game",
			text ? text : L"WARNING: Game data is corrupt or missing.", NULL );
		error->DoModal( NULL );
	}
}

void CCareerMapFrame::OnSliderMoved( int position )
{
	if ( position >= m_numTriplets - 1 )
		position = m_numTriplets - 1;
	if ( position < 0 )
		position = 0;
	if ( m_lastTripletActive == position )
		return;
	InvalidateLayout( false, false );
	Repaint();
	m_lastTripletActive = position;
	if ( !m_numTriplets )
		return;

	int hiddenColumn = position == 0 ? 0 : ( position == m_numTriplets - 1 ? 2 : -1 );
	if ( hiddenColumn >= 0 )
	{
		for ( int row = 0; row < 3; ++row )
			m_pMapButton[hiddenColumn][row]->SetVisible( false );
		m_pTripletBorder[hiddenColumn]->SetVisible( false );
		m_pTripletStatus[hiddenColumn]->SetVisible( false );
	}
	if ( position )
	{
		for ( int row = 0; row < 3; ++row )
			m_pMapButton[0][row]->SetVisible( true );
		m_pTripletBorder[0]->SetVisible( true );
		m_pTripletStatus[0]->SetVisible( true );
	}
	if ( position != m_numTriplets - 1 )
	{
		for ( int row = 0; row < 3; ++row )
			m_pMapButton[2][row]->SetVisible( true );
		m_pTripletBorder[2]->SetVisible( true );
		m_pTripletStatus[2]->SetVisible( true );
	}
	int firstTriplet = position > 0 ? position - 1 : 0;
	int firstColumn = position == 0 ? 1 : 0;
	int columnCount = position == 0 || position == m_numTriplets - 1 ? 2 : 3;
	int lastColumn = firstColumn + columnCount;
	if ( m_numTriplets == 1 )
	{
		for ( int column = 0; column < 3; column += 2 )
		{
			for ( int row = 0; row < 3; ++row )
				m_pMapButton[column][row]->SetVisible( false );
			m_pTripletBorder[column]->SetVisible( false );
			m_pTripletStatus[column]->SetVisible( false );
		}
		firstColumn = 1;
		lastColumn = 2;
	}
	for ( int column = firstColumn, triplet = firstTriplet; column < lastColumn; ++column, ++triplet )
	{
		std::vector<CCareerMap*> maps = TheCareerGame->GetMaps()[triplet];
		bool playable = TheCareerGame->GetPlayableMap( maps[0]->GetName() ) != NULL;
		int numDefeated = 0;
		char name[128];
		for ( int row = 0; row < 3; ++row )
		{
			CCareerMap* map = maps[row];
			CMapButton* button = m_pMapButton[column][row];
			button->SetEnabled( true );
			_snprintf( name, sizeof( name ), "#Career_Map_%s", map->GetName() );
			if ( !vgui2::localize()->Find( name ) )
				_snprintf( name, sizeof( name ), "%s", map->GetName() );
			button->SetText( name );
			button->SetTextInset( 8, 8 );
			button->SetDefeated( map->IsDefeated() );
			if ( map->IsDefeated() )
				++numDefeated;
			_snprintf( name, sizeof( name ), "gfx/thumbnails/maps/%s", map->GetName() );
			button->SetTexture( name );
			_snprintf( name, sizeof( name ), "map %d %s", playable, map->GetName() );
			button->SetCommand( name );
			button->SetTabPosition( 0 );
			button->IgnoreDoubleClick();
			button->SetMapName( map->GetName() );
			button->SetActive( false );
		}

		vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
		int number = triplet + 1;
		if ( number > 100 )
			number = 100;
		wchar_t digits[3] = { 0 };
		int digit = 0;
		if ( number >= 10 )
		{
			digits[digit++] = (wchar_t)( number / 10 + L'0' );
			number %= 10;
		}
		digits[digit] = (wchar_t)( number + L'0' );
		wchar_t title[128];
		vgui2::ILocalize* localize = vgui2::localize();
		wchar_t* format = vgui2::localize()->Find( "#Career_TripletNum" );
		localize->ConstructString( title, sizeof( title ), format, 1, digits );
		m_pTripletBorder[column]->SetText( title );
		Color color = scheme->GetColor( "TourOfDuty", Color( 0, 0, 0, 0 ) );
		m_pTripletBorder[column]->SetFgColor( color );
		Color statusColor;
		if ( numDefeated < 3 && playable )
		{
			m_pTripletStatus[column]->SetText( "#Career_Current" );
			statusColor = scheme->GetColor( "CurrentTriplet", color );
		}
		else if ( playable )
		{
			m_pTripletStatus[column]->SetText( "#Career_Unlocked" );
			statusColor = scheme->GetColor( "UnlockedTriplet", color );
		}
		else
		{
			m_pTripletStatus[column]->SetText( "#Career_Locked" );
			statusColor = scheme->GetColor( "LockedTriplet", color );
		}
		m_pTripletStatus[column]->SetFgColor( statusColor );
	}
	m_pMapButton[1][m_activeMapOffset]->SetActive( true );
}

void CCareerMapFrame::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	CCareerBaseFrame::ApplySchemeSettings( scheme );
	m_pMapImage->SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
	Color textColor = scheme->GetColor( "MapText", Color( 0, 0, 0, 0 ) );
	m_pMapName->SetFgColor( textColor );
	m_pMapDesc->SetFgColor( textColor );
	m_pMapDesc->SetBgColor( Color( 0, 0, 0, 0 ) );
	m_pMapSelectionBackground->SetBgColor( scheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 0 ) ) );
	m_pMapDescriptionBackground->SetBgColor( scheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 0 ) ) );
	for ( int index = 0; index < 2; ++index )
	{
		vgui2::Button* button = m_pMapScroll->GetButton( index );
		Color background = button->GetBgColor();
		Color foreground = button->GetSchemeColor( "DimBaseText", scheme );
		button->SetArmedColor( foreground, background );
		background = button->GetBgColor();
		foreground = button->GetSchemeColor( "DimBaseText", scheme );
		button->SetDepressedColor( foreground, background );
		background = button->GetBgColor();
		foreground = button->GetFgColor();
		button->SetDefaultColor( foreground, background );
	}
	m_pMapDesc->SetBorder( NULL );
	m_friendlyFireColor = scheme->GetColor( "FriendlyFire", Color( 255, 255, 255, 255 ) );
	m_mapDescriptionColor = scheme->GetColor( "MapText", Color( 255, 255, 255, 255 ) );
	m_pMapScroll->SetBorder( scheme->GetBorder( "BrowserBorder" ) );
}
