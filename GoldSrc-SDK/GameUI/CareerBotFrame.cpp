#include "CareerBotFrame.h"
#include "CareerGame.h"
#include "GameUI_Interface.h"
#include "Taskbar.h"
#include "BitmapButton.h"
#include "DottedLabel.h"
#include "ImageButton.h"

#include "Button.h"
#include "Label.h"
#include "PanelHorizListPanel.h"
#include "ScrollBar.h"
#include "Random.h"
#include "TextImage.h"
#include "../vgui2/controls/BitmapImagePanel.h"
#include "../cstrike/game_shared/bot/bot_profile.h"
#include "../game_shared/shared_util.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"

#include <algorithm>
#include <cwchar>
#include <limits>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifdef _WIN32
#define snprintf _snprintf
#endif

class CRect
{
public:
	CRect() : m_x( 0 ), m_y( 0 ), m_w( 0 ), m_h( 0 )
	{
	}
	CRect( int x, int y, int w, int h ) : m_x( x ), m_y( y ), m_w( w ), m_h( h )
	{
	}

	int	m_x;
	int	m_y;
	int	m_w;
	int	m_h;
};

class CBotButton : public CImageButton
{
public:
	CBotButton( vgui2::Panel* parent, const char* panelName, const char* text );
	~CBotButton() override;

	void SetCharacter( const CCareerCharacter* character );
	const CCareerCharacter* GetCharacter();
	void SetAction( const char* action );
	bool HasCharacter( const char* name );
	Color GetImageColor() override;
	void PaintBackground() override;
	virtual void SetImgTexture( int num, const char* filename );
	void ForceReload() override;
	void PerformLayout() override;
	void Paint() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	Color GetBgColor() override;
	void UpdateBgColor();
	void SetCategoryText( vgui2::TextImage* label, vgui2::TextImage* value, const char* category, float rating );
	Color GetLevelColor( int level );
	vgui2::HFont GetLevelFont( int level, vgui2::IScheme* scheme );
	void SetArmedBorder( vgui2::IBorder* border );
	void ShowLock( bool show ) { m_showLock = show; }

	enum
	{
		ACTION_LEN = 8,
		IMAGE_CHAR = 0,
		IMAGE_WEAPON,
		IMAGE_MAX,
		TEXT_POINTS = 0,
		TEXT_NAME,
		TEXT_SKILLLABEL,
		TEXT_TEAMLABEL,
		TEXT_AGGRESSIONLABEL,
		TEXT_MAX_HEIGHT_CALC,
		TEXT_SKILL = TEXT_MAX_HEIGHT_CALC,
		TEXT_TEAM,
		TEXT_AGGRESSION,
		TEXT_MAX
	};

protected:
	vgui2::IBorder* GetBorder( bool depressed, bool armed, bool selected, bool keyFocus ) override;
	virtual void ForceImgUpload( int num );
	virtual void ForceImgReload( int num );

	vgui2::Panel*			m_pParent;
	const CCareerCharacter*	m_char;
	Color					m_normalColor;
	Color					m_armedColor;
	Color					m_depressedColor;
	Color					m_disabledColor;
	Color					m_bgColor;
	Color					m_disabledBgColor;
	Color					m_disabledTextColor;
	char					m_action[ACTION_LEN];
	vgui2::IBorder*			m_armedBorder;
	vgui2::TextImage*		m_pText[TEXT_MAX];
	CRect					m_textRect[TEXT_MAX];
	bool					m_bImgUploaded[IMAGE_MAX];
	int						m_nImgTextureId[IMAGE_MAX];
	char					m_szImgTexture[IMAGE_MAX][128];
	CRect					m_imgRect[IMAGE_MAX];
	bool					m_showLock;

	struct bimage_t
	{
		int	imageID;
		int	width;
		int	height;
	};

	bimage_t m_LockImageID;
};

CBotButton::CBotButton( vgui2::Panel* parent, const char* panelName, const char* text ) :
	CImageButton( parent, panelName, text )
{
	m_pParent = parent;
	SetSize( 75, 150 );
	m_char = NULL;
	m_action[0] = 0;
	m_showLock = false;

	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
	m_disabledColor = GetSchemeColor( "BotDisabledBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	m_armedColor = GetSchemeColor( "BotArmedBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	m_depressedColor = GetSchemeColor( "BotDepressedBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	m_normalColor = GetSchemeColor( "BotNormalBG", GetSchemeColor( "ControlBG", scheme ), scheme );

	for ( int i = 0; i < IMAGE_MAX; i++ )
	{
		m_bImgUploaded[i] = false;
		m_nImgTextureId[i] = -1;
		m_szImgTexture[i][0] = 0;
	}
	for ( int i = 0; i < TEXT_MAX; i++ )
		m_pText[i] = new vgui2::TextImage( "" );

	m_armedBorder = NULL;
	m_LockImageID.imageID = vgui2::surface()->CreateNewTextureID( false );
	vgui2::surface()->DrawSetTextureFile( m_LockImageID.imageID, "gfx/career/Lock", false, false );
	vgui2::surface()->DrawGetTextureSize( m_LockImageID.imageID, m_LockImageID.width, m_LockImageID.height );
	m_LockImageID.width = GetProportionalScaledValue( m_LockImageID.width );
	m_LockImageID.height = GetProportionalScaledValue( m_LockImageID.height );
}

CBotButton::~CBotButton()
{
	for ( int i = 0; i < TEXT_MAX; i++ )
		delete m_pText[i];
}

void CBotButton::SetArmedBorder( vgui2::IBorder* border )
{
	m_armedBorder = border;
	InvalidateLayout( false, false );
}

vgui2::IBorder* CBotButton::GetBorder( bool depressed, bool armed, bool selected, bool keyFocus )
{
	if ( !depressed && armed && IsEnabled() )
		return m_armedBorder;
	return vgui2::Button::GetBorder( depressed, armed, selected, keyFocus );
}

Color CBotButton::GetBgColor()
{
	return IsEnabled() ? m_bgColor : m_disabledBgColor;
}

Color CBotButton::GetImageColor()
{
	if ( !IsEnabled() )
		return m_disabledColor;
	if ( IsDepressed() )
		return m_depressedColor;
	return IsArmed() ? m_armedColor : m_normalColor;
}

void CBotButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Button::ApplySchemeSettings( scheme );
	SetArmedSound( "sound/UI/buttonrollover.wav" );
	SetDepressedSound( "sound/UI/buttonclick.wav" );
	SetReleasedSound( "sound/UI/buttonclickrelease.wav" );

	char bgStr[16];
	if ( m_char )
		sprintf( bgStr, "BotBg%d", m_char->GetPoints() );
	else
		strcpy( bgStr, "BotBg0" );
	m_bgColor = scheme->GetColor( bgStr, Color( 0, 0, 0, 0 ) );
	m_disabledBgColor = scheme->GetColor( "BotBgDisabled", Color( 0, 0, 0, 0 ) );
	m_disabledTextColor = scheme->GetColor( "BotTextDisabled", Color( 0, 0, 0, 0 ) );
	SetDefaultBorder( scheme->GetBorder( "CareerButtonBorder" ) );
	SetDepressedBorder( scheme->GetBorder( "CareerButtonDepressedBorder" ) );
	m_armedBorder = scheme->GetBorder( "CareerButtonArmedBorder" );
}

void CBotButton::UpdateBgColor()
{
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
	char bgStr[16];
	if ( m_char )
		sprintf( bgStr, "BotBg%d", m_char->GetPoints() );
	else
		strcpy( bgStr, "BotBg0" );
	m_bgColor = scheme->GetColor( bgStr, Color( 0, 0, 0, 0 ) );
}

void CBotButton::SetAction( const char* action )
{
	strncpy( m_action, action, sizeof( m_action ) );
	m_action[sizeof( m_action ) - 1] = 0;
}

bool CBotButton::HasCharacter( const char* name )
{
	if ( !name )
		return m_char != NULL;
	return m_char && !strcmp( name, m_char->GetName() );
}

const CCareerCharacter* CBotButton::GetCharacter()
{
	return m_char;
}

void CBotButton::SetImgTexture( int num, const char* filename )
{
	if ( (unsigned int)num >= IMAGE_MAX )
		return;

	strcpy( m_szImgTexture[num], filename );
	if ( m_bImgUploaded[num] )
		ForceImgReload( num );
	else
		ForceImgUpload( num );
}

void CBotButton::ForceImgUpload( int num )
{
	if ( (unsigned int)num >= IMAGE_MAX || !m_szImgTexture[num][0] )
		return;

	m_bImgUploaded[num] = true;
	m_nImgTextureId[num] = vgui2::surface()->CreateNewTextureID( false );
	vgui2::surface()->DrawSetTextureFile( m_nImgTextureId[num], m_szImgTexture[num], false, true );
}

void CBotButton::ForceImgReload( int num )
{
	if ( (unsigned int)num >= IMAGE_MAX || !m_bImgUploaded[num] )
		return;
	vgui2::surface()->DrawSetTextureFile( m_nImgTextureId[num], m_szImgTexture[num], false, true );
}

void CBotButton::ForceReload()
{
	CImageButton::ForceReload();
	for ( int i = 0; i < IMAGE_MAX; i++ )
		ForceImgReload( i );
}

void CBotButton::SetCategoryText( vgui2::TextImage* label, vgui2::TextImage* value,
		const char* category, float rating )
{
	int level = m_char->GetRange( rating );
	char catBuf[128];
	char levelBuf[128];
	char colorBuf[128];
	snprintf( catBuf, sizeof( catBuf ), "#Career_%sDesc", category );
	snprintf( levelBuf, sizeof( levelBuf ), "#Career_LevelDesc%d", level );
	snprintf( colorBuf, sizeof( colorBuf ), "BotRange%d", level );
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
	Color color = GetSchemeColor( colorBuf, GetSchemeColor( "ControlBG", scheme ), scheme );
	label->SetText( vgui2::localize()->Find( catBuf ) );
	value->SetText( vgui2::localize()->Find( levelBuf ) );
	value->SetColor( color );
}

Color CBotButton::GetLevelColor( int level )
{
	char levelBuf[128];
	snprintf( levelBuf, sizeof( levelBuf ), "BotRange%d", level );
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
	return GetSchemeColor( levelBuf, GetSchemeColor( "ControlBG", scheme ), scheme );
}

vgui2::HFont CBotButton::GetLevelFont( int level, vgui2::IScheme* scheme )
{
	char levelBuf[128];
	snprintf( levelBuf, sizeof( levelBuf ), "BotRange%dFont", level );
	return scheme->GetFontEx( scheme->GetResourceString( levelBuf ), IsProportional(), IsHDProportional() );
}

const char* GetPreferredWeapon( const BotProfile* profile )
{
	const char* terroristWeapons[] = { "galil", "ak47", "sg552", "g3sg1", "mac10", "elites", NULL };
	const char* counterTerroristWeapons[] = { "shield", "famas", "m4a1", "aug", "sg550", "tmp", "fn57", NULL };
	const char** disallowed = TheCareerGame->PlayAsCT() ? terroristWeapons : counterTerroristWeapons;
	for ( int i = 0; i < profile->GetWeaponPreferenceCount(); i++ )
	{
		const char* weapon = profile->GetWeaponPreferenceAsString( i );
		if ( !weapon )
			continue;

		bool allowed = true;
		for ( int j = 0; disallowed[j]; j++ )
		{
			if ( !strcmp( weapon, disallowed[j] ) )
			{
				allowed = false;
				break;
			}
		}
		if ( allowed )
			return weapon;
	}
	return NULL;
}

int BotSortFunc( const void* first, const void* second )
{
	if ( !first || !second )
		return 0;
	const CCareerCharacter* a = *(const CCareerCharacter* const*)first;
	const CCareerCharacter* b = *(const CCareerCharacter* const*)second;
	if ( !a || !b )
		return 0;

	bool less = *a < *b;
	bool greater = *b < *a;
	return less ? -1 : greater;
}

void CBotButton::SetCharacter( const CCareerCharacter* character )
{
	vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
	if ( !GetFont() && scheme )
		SetFont( scheme->GetFontEx( "Default", IsProportional(), IsHDProportional() ) );

	m_char = character;
	if ( m_char )
	{
		SetImgTexture( IMAGE_CHAR, "" );
		SetImgTexture( IMAGE_WEAPON, "" );
		char buf[128];
		snprintf( buf, sizeof( buf ), "%s %s", m_action, m_char->GetName() );
		SetCommand( buf );
		SetText( "" );
		const BotProfile* profile = m_char->GetProfile();
		for ( int i = 0; i < TEXT_MAX; i++ )
		{
			m_pText[i]->SetFont( GetFont() );
			m_pText[i]->SetDrawWidth( GetWide() - 4 );
			m_pText[i]->SetColor( Color( 255, 255, 255, 255 ) );
		}

		wchar_t wInt[std::numeric_limits<int>::digits10 + 3];
		swprintf( wInt, sizeof( wInt ) / sizeof( wInt[0] ), L"%d", m_char->GetPoints() );
		const wchar_t* costFormat = vgui2::localize()->Find( "#Career_Cost" );
		if ( !costFormat )
			costFormat = L"Cost: %s1";
		std::vector<wchar_t> wbuf( wcslen( costFormat ) + sizeof( wInt ) / sizeof( wInt[0] ), L'\0' );
		vgui2::localize()->ConstructString( wbuf.data(), (int)( wbuf.size() * sizeof( wchar_t ) ),
			const_cast<wchar_t*>( costFormat ), 1, wInt );
		m_pText[TEXT_POINTS]->SetText( wbuf.data() );
		m_pText[TEXT_POINTS]->SetColor( GetSchemeColor( "BotPoints", GetSchemeColor( "ControlBG", scheme ), scheme ) );
		m_pText[TEXT_POINTS]->SetFont( scheme->GetFontEx( scheme->GetResourceString( "BotPointsFont" ),
				IsProportional(), IsHDProportional() ) );

		strcpy( buf, m_char->GetName() );
		for ( size_t i = 0; i < strlen( buf ); i++ )
			buf[i] = (char)toupper( buf[i] );
		m_pText[TEXT_NAME]->SetText( buf );
		m_pText[TEXT_NAME]->SetColor( GetSchemeColor( "BotName", GetSchemeColor( "ControlBG", scheme ), scheme ) );
		m_pText[TEXT_NAME]->SetFont( scheme->GetFontEx( scheme->GetResourceString( "BotNameFont" ),
				IsProportional(), IsHDProportional() ) );

		const char* categories[] = { "Skill", "Team", "Aggression" };
		for ( int i = 0; i < 3; i++ )
		{
			vgui2::TextImage* label = m_pText[TEXT_SKILLLABEL + i];
			vgui2::TextImage* value = m_pText[TEXT_SKILL + i];
			float rating = i == 0 ? profile->GetSkill() : i == 1 ? profile->GetTeamwork() : profile->GetAggression();
			SetCategoryText( label, value, categories[i], rating );
			label->SetColor( GetSchemeColor( "BotDesc", GetSchemeColor( "ControlBG", scheme ), scheme ) );
			rating = i == 0 ? profile->GetSkill() : i == 1 ? profile->GetTeamwork() : profile->GetAggression();
			value->SetFont( GetLevelFont( m_char->GetRange( rating ), scheme ) );
			label->SetFont( scheme->GetFontEx( scheme->GetResourceString( "BotCategoryFont" ),
					IsProportional(), IsHDProportional() ) );
		}

		const char* weapon = GetPreferredWeapon( profile );
		if ( weapon )
		{
			snprintf( buf, sizeof( buf ), "gfx/career/weapons/640_%s", weapon );
			SetImgTexture( IMAGE_WEAPON, buf );
		}
		else
		{
			SetImgTexture( IMAGE_WEAPON, "" );
		}
		const char* thumbnail = m_char->GetSkinThumbnailFname();
		if ( thumbnail )
			SetImgTexture( IMAGE_CHAR, thumbnail );
	}
	else
	{
		SetCommand( "EmptyChar" );
		SetText( "" );
		for ( int i = 0; i < TEXT_MAX; i++ )
		{
			m_pText[i]->SetText( "" );
			m_pText[i]->SetFont( GetFont() );
		}
		SetImgTexture( IMAGE_CHAR, "" );
		SetImgTexture( IMAGE_WEAPON, "" );
	}
	InvalidateLayout( false, false );
}

void CBotButton::PaintBackground()
{
	CImageButton::PaintBackground();
	for ( int i = 0; i < IMAGE_MAX; i++ )
	{
		if ( !m_szImgTexture[i][0] )
			continue;
		if ( !m_bImgUploaded[i] )
			ForceImgUpload( i );

		Color color = GetImageColor();
		vgui2::surface()->DrawSetColor( color );
		vgui2::surface()->DrawSetTexture( m_nImgTextureId[i] );
		CRect& rect = m_imgRect[i];
		vgui2::surface()->DrawTexturedRect( rect.m_x, rect.m_y, rect.m_x + rect.m_w, rect.m_y + rect.m_h );
		if ( i == IMAGE_CHAR )
		{
			vgui2::surface()->DrawSetColor( IsEnabled() ? Color( 255, 255, 255, 255 ) : m_disabledTextColor );
			vgui2::surface()->DrawOutlinedRect( rect.m_x - 1, rect.m_y - 1,
					rect.m_x + rect.m_w + 1, rect.m_y + rect.m_h + 1 );
		}
	}

	if ( m_showLock )
	{
		int wide, tall;
		GetSize( wide, tall );
		vgui2::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui2::surface()->DrawSetTexture( m_LockImageID.imageID );
		vgui2::surface()->DrawTexturedRect( wide - 3 - m_LockImageID.width,
				tall - 3 - m_LockImageID.height, wide - 3, tall - 3 );
	}
}

void CBotButton::Paint()
{
	vgui2::Button::Paint();
	for ( int i = 0; i < TEXT_MAX; i++ )
	{
		m_pText[i]->SetPos( m_textRect[i].m_x, m_textRect[i].m_y );
		if ( IsEnabled() )
		{
			m_pText[i]->Paint();
		}
		else
		{
			Color color = m_pText[i]->GetColor();
			m_pText[i]->SetColor( m_disabledTextColor );
			m_pText[i]->Paint();
			m_pText[i]->SetColor( color );
		}
	}
}

void CBotButton::PerformLayout()
{
	vgui2::Button::PerformLayout();
	int boxWidth, boxHeight;
	GetSize( boxWidth, boxHeight );
	int y = 24;
	for ( int i = 0; i < TEXT_MAX; i++ )
	{
		int wide, tall;
		m_pText[i]->ResizeImageToContent();
		m_pText[i]->GetSize( wide, tall );
		m_textRect[i] = CRect( std::max( 0, ( boxWidth - wide ) / 2 ), y, wide, tall );
		if ( i < TEXT_MAX_HEIGHT_CALC )
			y += tall;
	}

	int freeHeight = boxHeight - y;
	if ( freeHeight > 9 )
	{
		int portraitSize = std::min( boxWidth, 2 * freeHeight / 3 );
		m_imgRect[IMAGE_CHAR] = CRect( std::max( 0, ( boxWidth - portraitSize ) / 2 ), 0,
				portraitSize, portraitSize );
		if ( m_szImgTexture[IMAGE_WEAPON][0] )
		{
			int texWidth = 0, texHeight = 0;
			vgui2::surface()->DrawGetTextureSize( m_nImgTextureId[IMAGE_WEAPON], texWidth, texHeight );
			if ( texWidth && texHeight )
			{
#ifdef _WIN32
				float scale = std::min( (float)boxWidth / (float)texWidth,
						(float)( freeHeight - portraitSize ) / (float)texHeight );
				float wide = (float)texWidth * scale;
				float tall = (float)texHeight * scale;
#else
				long double scaleX = (long double)boxWidth / texWidth;
				long double scale = (long double)( freeHeight - portraitSize ) / texHeight;
				if ( scale > scaleX )
					scale = scaleX;
				float wide = (float)( texWidth * scale );
				float tall = (float)( texHeight * scale );
#endif
				m_imgRect[IMAGE_WEAPON] = CRect( std::max( 0, ( boxWidth - (int)wide ) / 2 ), 0,
						(int)wide, (int)tall );
			}
			else
			{
				m_imgRect[IMAGE_WEAPON] = CRect();
			}
		}
		else
		{
			m_imgRect[IMAGE_WEAPON] = CRect();
		}
	}
	else
	{
		m_imgRect[IMAGE_CHAR] = CRect();
		m_imgRect[IMAGE_WEAPON] = CRect();
	}

	m_textRect[TEXT_POINTS].m_y = 5;
	y = m_textRect[TEXT_POINTS].m_h + 10;
	if ( m_szImgTexture[IMAGE_CHAR][0] )
	{
		m_imgRect[IMAGE_CHAR].m_y = y;
		y += m_imgRect[IMAGE_CHAR].m_h + 5;
	}
	m_textRect[TEXT_NAME].m_y = y;
	y += m_textRect[TEXT_NAME].m_h + 1;
	int inset = GetProportionalScaledValue( 4 );
	for ( int i = 0; i < 3; i++ )
	{
		int wide, tall;
		m_pText[TEXT_SKILLLABEL + i]->GetSize( wide, tall );
		m_pText[TEXT_SKILL + i]->GetSize( wide, tall );
		m_textRect[TEXT_SKILLLABEL + i].m_y = y;
		m_textRect[TEXT_SKILL + i].m_y = y;
		m_textRect[TEXT_SKILLLABEL + i].m_x = inset;
		m_textRect[TEXT_SKILL + i].m_x = boxWidth - m_textRect[TEXT_SKILL + i].m_w - inset;
		if ( i < 2 )
			y += m_textRect[TEXT_SKILL + i].m_h + 1;
	}
	if ( m_szImgTexture[IMAGE_WEAPON][0] )
		m_imgRect[IMAGE_WEAPON].m_y = y + m_textRect[TEXT_SKILL].m_h + 1;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
CCareerBotFrame::CCareerBotFrame( vgui2::Panel* parent ) :
	CCareerBaseFrame( parent, "CareerBotFrame", false )
{
	vgui2::Frame::SetTitle( "#Career_Bot_Frame", true );
	m_pPoolBackground = new CDottedBgLabel( this, "PoolBackground", "", 0, 0, 0 );
	m_pPoolBackground->SetTextInset( 2, 2 );
	m_pHiredBackground = new CDottedBgLabel( this, "HiredBackground", "", 0, 0, 0 );
	m_pHiredBackground->SetTextInset( 2, 2 );
	new vgui2::CBitmapImagePanel( this, "TitlePanel", "gfx/career/Logo_CSCZ_Big" );
	new vgui2::CBitmapImagePanel( this, "AssembleTeamPanel", "gfx/career/hed_assembleteam" );
	new vgui2::CBitmapImagePanel( this, "CareerPointsPanel", "resource/career_points" );
	m_pPointsLabel = new vgui2::Label( this, "LabelPoints", "" );
	m_pListPanel = new vgui2::PanelHorizListPanel( this, "PanelListPanel" );

	int x = 555;
	for ( int i = MAX_HIRED - 1; i >= 0; i-- )
	{
		char name[64];
		sprintf( name, "HireButton%d", i );
		m_pHiredButton[i] = new CBotButton( this, name, name );
		m_pHiredButton[i]->SetCommand( name );
		m_pHiredButton[i]->SetPos( x, 75 );
		m_pHiredButton[i]->SetAction( "fire" );
		x -= m_pHiredButton[i]->GetWide() + 5;
	}

	m_pAutoRosterButton = new CBitmapButton( this, "ButtonAutoRoster", "",
			"gfx/career/circled_lightningbolt_away_big", "gfx/career/circled_lightningbolt_over_big",
			"gfx/career/circled_lightningbolt_clicked_big", NULL );
	if ( !m_buttons.HasElement( m_pAutoRosterButton ) )
		m_buttons.AddElement( m_pAutoRosterButton );

	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	m_pJumpButtons.AddToTail( (CBitmapButton*)NULL );
	for ( int i = 1; i <= MAX_HIRED; i++ )
	{
		char name[64], normalImage[64], armedImage[64], clickedImage[64], disabledImage[64];
		sprintf( name, "ButtonJump%d", i );
		sprintf( normalImage, "gfx/career/circled_%d_away%s", i, "_big" );
		sprintf( armedImage, "gfx/career/circled_%d_over%s", i, "_big" );
		sprintf( clickedImage, "gfx/career/circled_%d_clicked%s", i, "_big" );
		sprintf( disabledImage, "gfx/career/circled_%d_disabled%s", i, "_big" );
		CBitmapButton* button = new CBitmapButton( this, name, "", normalImage, armedImage,
				clickedImage, disabledImage );
		m_pJumpButtons.AddToTail( button );
		if ( !m_buttons.HasElement( m_pJumpButtons[i] ) )
			m_buttons.AddElement( m_pJumpButtons[i] );
	}

	m_pStartButton = new CCareerButton( this, "ButtonPlay", "#Career_Start", "gfx/career/icon_forward", true );
	if ( !m_buttons.HasElement( m_pStartButton ) )
		m_buttons.AddElement( m_pStartButton );
	m_pPrevButton = new CCareerButton( this, "ButtonPrev", "#Career_BotPrev", "gfx/career/icon_backward", false );
	if ( !m_buttons.HasElement( m_pPrevButton ) )
		m_buttons.AddElement( m_pPrevButton );
	m_pPoolPgUpButton = new CBitmapButton( this, "ButtonBotPageUp", "", "gfx/career/LeftArrow_away",
			"gfx/career/LeftArrow_over", "gfx/career/LeftArrow_clicked", NULL );
	if ( !m_buttons.HasElement( m_pPoolPgUpButton ) )
		m_buttons.AddElement( m_pPoolPgUpButton );
	m_pPoolPgDownButton = new CBitmapButton( this, "ButtonBotPageDown", "", "gfx/career/RightArrow_away",
			"gfx/career/RightArrow_over", "gfx/career/RightArrow_clicked", NULL );
	if ( !m_buttons.HasElement( m_pPoolPgDownButton ) )
		m_buttons.AddElement( m_pPoolPgDownButton );
	m_pPointsLeftLabel = new vgui2::Label( this, "LabelPointsLeft", "" );
	vgui2::Frame::LoadControlSettings( "Resource/CareerBotFrame.res", NULL );
	for ( int i = MAX_HIRED - 1; i >= 0; i-- )
		m_pHiredButton[i]->SetCharacter( NULL );
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CCareerBotFrame::~CCareerBotFrame()
{
}

void CCareerBotFrame::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	CCareerBaseFrame::ApplySchemeSettings( scheme );
	m_pPointsLabel->SetFgColor( scheme->GetColor( "WindowBG", Color( 0, 0, 0, 0 ) ) );
	m_pListPanel->SetBorder( NULL );
	Color color = scheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 0 ) );
	m_pPoolBackground->SetBgColor( color );
	m_pHiredBackground->SetBgColor( color );
	m_pPoolPgUpButton->SetBgColor( Color( 255, 255, 255, 255 ) );
	m_pPoolPgDownButton->SetBgColor( Color( 255, 255, 255, 255 ) );
	m_pPointsLeftLabel->SetFgColor( scheme->GetColor( "PlayerName", Color( 0, 0, 0, 0 ) ) );
}

void CCareerBotFrame::OnThink()
{
	CCareerBaseFrame::OnThink();
	if ( !m_initialLayoutDone )
	{
		int low, high;
		m_pListPanel->GetSliderRange( low, high );
		if ( low || high )
		{
			m_initialLayoutDone = true;
			m_pListPanel->SetSliderPosition( TheCareerGame->GetBotPoolSliderPosition() );
			m_pListPanel->InvalidateLayout( false, false );
		}
	}
}

void CCareerBotFrame::Activate()
{
	SetPos( 0, 0 );
	CCareerBaseFrame::Activate();
	int wide, tall;
	m_pHiredButton[0]->GetSize( wide, tall );
	m_pListPanel->DeleteAllItems();
	CharacterVec allChars = TheCareerGame->GetAvailableCharacters();
	CharacterVec hiredChars = TheCareerGame->GetHiredCharacters();
	while ( !hiredChars.empty() )
	{
		allChars.push_back( hiredChars.back() );
		hiredChars.pop_back();
	}
	CUtlVector<CCareerCharacter*> sorted;
	for ( CharacterVec::iterator it = allChars.begin(); it != allChars.end(); ++it )
		sorted.AddToTail( *it );
	if ( sorted.Count() )
		qsort( sorted.Base(), sorted.Count(), sizeof( CCareerCharacter* ), BotSortFunc );
	for ( int i = 0; i < sorted.Count(); i++ )
	{
		char name[64];
		sprintf( name, "PoolButton%s", sorted[i]->GetName() );
		CBotButton* button = new CBotButton( this, name, name );
		button->SetCommand( name );
		button->SetSize( wide, tall );
		button->SetAction( "hire" );
		button->SetCharacter( sorted[i] );
		m_pListPanel->AddItem( button );
	}

	m_initialLayoutDone = false;
	for ( int i = 0; i < MAX_HIRED; i++ )
		m_pHiredButton[i]->SetCharacter( NULL );
	int poolSize = m_pListPanel->GetItemCount();
	for ( int i = 0; i < poolSize; i++ )
	{
		vgui2::Panel* button = m_pListPanel->GetItem( i );
		if ( button )
			button->SetVisible( true );
	}
	hiredChars = TheCareerGame->GetHiredCharacters();
	for ( CharacterVec::iterator it = hiredChars.begin(); it != hiredChars.end(); ++it )
	{
		CBotButton* hired = GetHiredButton( NULL );
		CBotButton* pool = GetPoolButton( ( *it )->GetName() );
		if ( pool && hired )
		{
			pool->SetVisible( false );
			hired->SetCharacter( pool->GetCharacter() );
		}
	}
	SortHires();
	UpdatePointsRemaining();
	m_pListPanel->SetSliderPosition( TheCareerGame->GetBotPoolSliderPosition() );
	m_pListPanel->InvalidateLayout( false, false );
	int highestFlashed = TheCareerGame->GetHighestCostAvailable( TheCareerGame->GetPointsFlashed() );
	int highestAvailable = TheCareerGame->GetHighestCostAvailable( -1 );
	for ( int i = 1; i <= highestAvailable && i < m_pJumpButtons.Count(); i++ )
	{
		m_pJumpButtons[i]->SetEnabled( true );
		if ( highestFlashed < highestAvailable )
			m_pJumpButtons[i]->Flash();
	}
	for ( int i = highestAvailable + 1; i < m_pJumpButtons.Count(); i++ )
		m_pJumpButtons[i]->SetEnabled( false );
	TheCareerGame->MarkJumpButtonsFlashed();
	m_pStartButton->RequestFocus( 0 );
}

CBotButton* CCareerBotFrame::GetHiredButton( const char* name )
{
	for ( int i = 0; i < MAX_HIRED; i++ )
	{
		if ( name ? m_pHiredButton[i]->HasCharacter( name ) : !m_pHiredButton[i]->HasCharacter( NULL ) )
			return m_pHiredButton[i];
	}
	return NULL;
}

CBotButton* CCareerBotFrame::GetPoolButton( const char* name )
{
	if ( !name )
		return NULL;
	int count = m_pListPanel->GetItemCount();
	for ( int i = 0; i < count; i++ )
	{
		CBotButton* button = (CBotButton*)m_pListPanel->GetItem( i );
		if ( button && button->HasCharacter( name ) )
			return button;
	}
	return NULL;
}

void CCareerBotFrame::UpdatePointsRemaining()
{
	char tmp[16];
	sprintf( tmp, "%d", TheCareerGame->GetPointsRemaining() );
	m_pPointsLabel->SetText( tmp );
	bool hasEmptySlot = false;
	for ( int i = 0; i < MAX_HIRED; i++ )
	{
		m_pHiredButton[i]->UpdateBgColor();
		bool occupied = m_pHiredButton[i]->GetCharacter() != NULL;
		m_pHiredButton[i]->SetEnabled( occupied );
		if ( !occupied )
			hasEmptySlot = true;
	}
	int poolSize = m_pListPanel->GetItemCount();
	int highestAvailable = TheCareerGame->GetHighestCostAvailable( -1 );
	for ( int i = 0; i < poolSize; i++ )
	{
		CBotButton* button = (CBotButton*)m_pListPanel->GetItem( i );
		if ( !button )
			continue;
		const CCareerCharacter* character = button->GetCharacter();
		bool unlocked = character && highestAvailable >= character->GetPoints();
		button->SetEnabled( hasEmptySlot && unlocked && character->GetPoints() <= TheCareerGame->GetPointsRemaining() );
		button->ShowLock( !unlocked );
	}
}

int CCareerBotFrame::Hire( const char* name )
{
	CBotButton* hired = GetHiredButton( NULL );
	CBotButton* pool = GetPoolButton( name );
	if ( !pool || !hired || !TheCareerGame->Hire( name ) )
		return 0;
	pool->SetVisible( false );
	hired->SetCharacter( pool->GetCharacter() );
	UpdatePointsRemaining();
	SortHires();
	m_pListPanel->InvalidateLayout( false, false );
	hired->InvalidateLayout( false, false );
	return pool->GetCharacter()->GetPoints();
}

void CCareerBotFrame::Fire( const char* name )
{
	if ( !name )
		return;
	CBotButton* hired = GetHiredButton( name );
	CBotButton* pool = GetPoolButton( name );
	if ( !pool || !hired || !TheCareerGame->Fire( name ) )
		return;
	pool->SetVisible( true );
	hired->SetCharacter( NULL );
	UpdatePointsRemaining();
	SortHires();
	m_pListPanel->InvalidateLayout( false, false );
}

void CCareerBotFrame::SortHires()
{
	std::vector<const CCareerCharacter*> hired;
	for ( int i = 0; i < MAX_HIRED; i++ )
	{
		const CCareerCharacter* character = m_pHiredButton[i]->GetCharacter();
		if ( character )
		{
			hired.push_back( character );
			m_pHiredButton[i]->SetCharacter( NULL );
		}
	}
	int slot = 0;
	for ( std::vector<const CCareerCharacter*>::iterator it = hired.begin(); it != hired.end(); ++it )
		m_pHiredButton[slot++]->SetCharacter( *it );
	UpdatePointsRemaining();
}

CUtlVector<const CCareerCharacter*>* CCareerBotFrame::GetPoolCharacters( int points )
{
	CUtlVector<const CCareerCharacter*>* characters = new CUtlVector<const CCareerCharacter*>;
	for ( int i = m_pListPanel->GetItemCount() - 1; i >= 0; i-- )
	{
		CBotButton* button = (CBotButton*)m_pListPanel->GetItem( i );
		if ( !button )
			continue;
		const CCareerCharacter* character = button->GetCharacter();
		bool visible = button->IsVisible();
		if ( character && visible && character->GetPoints() == points )
			characters->AddToTail( character );
	}
	return characters;
}

void CCareerBotFrame::AutoRoster()
{
	while ( m_pHiredButton[0]->GetCharacter() )
		Fire( m_pHiredButton[0]->GetCharacter()->GetName() );
	int numOpen = 0;
	for ( int i = 0; i < MAX_HIRED; i++ )
	{
		if ( !m_pHiredButton[i]->GetCharacter() )
			numOpen++;
	}
	if ( !numOpen )
		return;
	int points = TheCareerGame->GetPointsRemaining();
	int pointsPerSlot = points / numOpen;
	int remaining = points % numOpen;
	for ( int i = 0; i < numOpen; i++ )
	{
		int target = pointsPerSlot + ( remaining != 0 );
		bool hired = false;
		for ( int cost = target; cost > 0 && !hired; cost-- )
		{
			CUtlVector<const CCareerCharacter*>* pool = GetPoolCharacters( cost );
			while ( pool->Count() )
			{
				int index = RandomLong( 0, pool->Count() - 1 );
				const CCareerCharacter* character = ( *pool )[index];
				pool->FastRemove( index );
				if ( Hire( character->GetName() ) )
				{
					if ( cost == target && remaining )
						remaining--;
					hired = true;
					break;
				}
			}
			delete pool;
		}
	}
}

void CCareerBotFrame::OnCommand( const char* command )
{
	if ( !strcmp( command, "close" ) )
	{
		OnClose();
	}
	else if ( !strcmp( command, "prev" ) )
	{
		Career_Printf( "Going to map page\n" );
		OnClose();
		g_pTaskbar->OnOpenCareerMapDialog( false );
	}
	else if ( !strcmp( command, "EmptyChar" ) )
	{
		vgui2::Frame::OnCommand( command );
		return;
	}
	else if ( !strcmp( command, "AutoRoster" ) )
	{
		AutoRoster();
		m_pStartButton->RequestFocus( 0 );
	}
	else if ( !strncmp( command, "hire ", 5 ) && strlen( command ) > 5 )
	{
		Hire( command + 5 );
		m_pStartButton->RequestFocus( 0 );
	}
	else if ( !strncmp( command, "fire ", 5 ) && strlen( command ) > 5 )
	{
		Fire( command + 5 );
		m_pStartButton->RequestFocus( 0 );
	}
	else if ( !strcmp( command, "start" ) )
	{
		TheCareerGame->SetBotPoolSliderPosition( m_pListPanel->GetSliderPosition() );
		TheCareerGame->SendMatchCommand();
		OnClose();
	}
	else if ( !strcmp( command, "PoolPageUp" ) )
	{
		int position = m_pListPanel->GetSliderPosition();
		m_pListPanel->SetSliderPosition( position - m_pListPanel->GetScrollBar()->GetRangeWindow() );
		m_pStartButton->RequestFocus( 0 );
	}
	else if ( !strcmp( command, "PoolPageDown" ) )
	{
		int position = m_pListPanel->GetSliderPosition();
		m_pListPanel->SetSliderPosition( position + m_pListPanel->GetScrollBar()->GetRangeWindow() );
		m_pStartButton->RequestFocus( 0 );
	}
	else if ( !strncmp( command, "jump ", 5 ) && strlen( command ) == 6 )
	{
		int jumpToPoints = command[5] - '0';
		if ( jumpToPoints >= 1 && jumpToPoints <= MAX_HIRED )
		{
			int poolSize = m_pListPanel->GetItemCount();
			for ( int i = 0; i < poolSize; i++ )
			{
				CBotButton* button = (CBotButton*)m_pListPanel->GetItem( i );
				if ( button && button->GetCharacter() && button->GetCharacter()->GetPoints() == jumpToPoints
						&& button->IsVisible() )
				{
					int x, y;
					button->GetPos( x, y );
					Career_Printf( "Jump to %d found someone in index %d at %d, %d\n", jumpToPoints, i, x, y );
					m_pListPanel->SetSliderPosition( x );
					m_pStartButton->RequestFocus( 0 );
					return;
				}
			}
			m_pStartButton->RequestFocus( 0 );
			vgui2::Frame::OnCommand( command );
		}
		return;
	}
	else
	{
		Career_Printf( "Unknown command [%s]\n", command );
	}
	vgui2::Frame::OnCommand( command );
}

void CCareerBotFrame::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_ESCAPE )
		OnCommand( "prev" );
	else if ( code == vgui2::KEY_ENTER )
	{
		OnCommand( "start" );
		return;
	}
	vgui2::Frame::OnKeyCodeTyped( code );
}
