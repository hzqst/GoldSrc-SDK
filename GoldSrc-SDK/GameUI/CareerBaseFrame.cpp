#include "CareerBaseFrame.h"
#include "CareerDefinitions.h"
#include "CareerGame.h"
#include "GameUI_Interface.h"
#include "Taskbar.h"

#include "BuildGroup.h"
#include "Button.h"
#include "Label.h"
#include "ScrollBar.h"
#include "TextImage.h"
#include "vgui/IImage.h"
#include "../vgui2/controls/BitmapImagePanel.h"
#include "../game_shared/shared_util.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <climits>
#include <string>

#define AUTOSAVE_CORNER_SIZE		20
#define CAREER_BACKGROUND_WIDE	1024
#define CAREER_BACKGROUND_TALL	768

void CCareerPanelListPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	CPanelListPanel::ApplySchemeSettings( scheme );
	_vbar->SetBorder( scheme->GetBorder( "BrowserBorder" ) );
}

#pragma warning( suppress : 26495 )
CCareerButton::CCareerButton( vgui2::Panel* parent, const char* name, const char* text,
	const char* image, bool textFirst )
	: vgui2::Button( parent, name, "" )
{
	m_armedBorder = NULL;
	m_textImage = new vgui2::TextImage( text );
	m_image = vgui2::scheme()->GetImage( image, true );
	m_textFirst = textFirst;
}

CCareerButton::~CCareerButton()
{
}

void CCareerButton::SetArmedBorder( vgui2::IBorder* border )
{
	m_armedBorder = border;
	InvalidateLayout( false, false );
}

vgui2::IBorder* CCareerButton::GetBorder( bool depressed, bool armed, bool selected, bool keyFocus )
{
	if ( armed && !depressed && IsEnabled() )
		return m_armedBorder;
	return vgui2::Button::GetBorder( depressed, armed, selected, keyFocus );
}

void CCareerButton::Paint()
{
	int buttonWide, buttonTall, imageWide, imageTall;
	GetSize( buttonWide, buttonTall );
	m_image->GetSize( imageWide, imageTall );
	int textX = m_textPad;
	if ( m_textFirst )
	{
		m_image->SetPos( buttonWide - imageWide - m_imagePad, ( buttonTall - imageTall ) / 2 );
		m_image->Paint();
	}
	else
	{
		m_image->SetPos( m_imagePad, ( buttonTall - imageTall ) / 2 );
		m_image->Paint();
		textX += m_imagePad + imageWide;
	}

	int textWide, textTall;
	m_textImage->GetSize( textWide, textTall );
	int textAreaWide = buttonWide - imageWide - m_imagePad - 2 * m_textPad;
	m_textImage->SetColor( IsEnabled() ? m_textNormalColor : m_textDisabledColor );
	m_textImage->SetPos( textX + ( textAreaWide - textWide ) / 2, ( buttonTall - textTall ) / 2 );
	m_textImage->Paint();
	if ( HasFocus() && IsEnabled() )
		DrawFocusBorder( 3, 3, buttonWide - 4, buttonTall - 2 );
}

void CCareerButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Button::ApplySchemeSettings( scheme );
	SetDefaultBorder( scheme->GetBorder( "CareerButtonBorder" ) );
	SetDepressedBorder( scheme->GetBorder( "CareerButtonDepressedBorder" ) );
	m_armedBorder = scheme->GetBorder( "CareerButtonArmedBorder" );
	m_textNormalColor = scheme->GetColor( "BrightBaseText", Color( 0, 0, 0, 0 ) );
	m_textDisabledColor = scheme->GetColor( "DisabledText1", Color( 0, 0, 0, 0 ) );
	m_textImage->SetColor( m_textNormalColor );
	m_image->SetColor( GetFgColor() );
	m_textImage->SetFont( scheme->GetFontEx( "Default", IsProportional(), IsHDProportional() ) );
	m_textPad = atoi( scheme->GetResourceString( "CareerButtonTextPad" ) );
	m_imagePad = atoi( scheme->GetResourceString( "CareerButtonImagePad" ) );
	if ( IsProportional() )
	{
		m_textPad = GetProportionalScaledValue( m_textPad );
		m_imagePad = GetProportionalScaledValue( m_imagePad );
	}

	char text[128];
	GetText( text, sizeof( text ) );
	m_textImage->SetText( text );
	m_textImage->ResizeImageToContent();
	int buttonWide, buttonTall, imageWide, imageTall, textWide, textTall;
	GetSize( buttonWide, buttonTall );
	m_image->GetSize( imageWide, imageTall );
	int textAreaWide = buttonWide - imageWide - m_imagePad - 2 * m_textPad;
	m_textImage->GetContentSize( textWide, textTall );
	if ( textAreaWide < textWide )
		m_textImage->SetSize( textAreaWide, textTall );

	Color background = scheme->GetColor( "CareerButtonBG", Color( 0, 0, 0, 0 ) );
	SetDefaultColor( background, background );
	SetArmedColor( background, background );
	SetDepressedColor( background, background );
}

class CAutosaveLabel : public vgui2::Label
{
public:
	CAutosaveLabel( vgui2::Panel* parent, const char* panelName, const char* text );
	Color GetBgColor() override;

protected:
	void PerformLayout() override;
	void PaintBackground() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

private:
	Color m_bgColor;
	vgui2::CBitmapImagePanel* m_pTopLeftPanel;
	vgui2::CBitmapImagePanel* m_pTopRightPanel;
	vgui2::CBitmapImagePanel* m_pBottomLeftPanel;
	vgui2::CBitmapImagePanel* m_pBottomRightPanel;
};

CAutosaveLabel::CAutosaveLabel( vgui2::Panel* parent, const char* panelName, const char* text )
	: vgui2::Label( parent, panelName, text )
{
	m_pTopLeftPanel = new vgui2::CBitmapImagePanel( this, "TopLeftPanel", "gfx/career/round_corner_nw" );
	m_pTopRightPanel = new vgui2::CBitmapImagePanel( this, "TopRightPanel", "gfx/career/round_corner_ne" );
	m_pBottomLeftPanel = new vgui2::CBitmapImagePanel( this, "BottomLeftPanel", "gfx/career/round_corner_sw" );
	m_pBottomRightPanel = new vgui2::CBitmapImagePanel( this, "BottomRightPanel", "gfx/career/round_corner_se" );
}

Color CAutosaveLabel::GetBgColor()
{
	return m_bgColor;
}

void CAutosaveLabel::PerformLayout()
{
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	int textWide, textTall;
	GetContentSize( textWide, textTall );
	m_pTopLeftPanel->SetBounds( 0, 0, AUTOSAVE_CORNER_SIZE, AUTOSAVE_CORNER_SIZE );
	m_pTopRightPanel->SetBounds( wide - AUTOSAVE_CORNER_SIZE, 0, AUTOSAVE_CORNER_SIZE, AUTOSAVE_CORNER_SIZE );
	m_pBottomLeftPanel->SetBounds( 0, tall - AUTOSAVE_CORNER_SIZE, AUTOSAVE_CORNER_SIZE, AUTOSAVE_CORNER_SIZE );
	m_pBottomRightPanel->SetBounds( wide - AUTOSAVE_CORNER_SIZE, tall - AUTOSAVE_CORNER_SIZE,
		AUTOSAVE_CORNER_SIZE, AUTOSAVE_CORNER_SIZE );
}

void CAutosaveLabel::PaintBackground()
{
	int x1, y1, x2, y2;
	vgui2::surface()->DrawSetColor( m_bgColor );

	m_pTopLeftPanel->GetPos( x1, y1 );
	m_pBottomLeftPanel->GetPos( x2, y2 );
	y1 += m_pTopLeftPanel->GetTall();
	x2 += m_pBottomLeftPanel->GetWide();
	vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );

	m_pTopLeftPanel->GetPos( x1, y1 );
	m_pBottomRightPanel->GetPos( x2, y2 );
	x1 += m_pTopLeftPanel->GetWide();
	y2 += m_pBottomRightPanel->GetTall();
	vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );

	m_pTopRightPanel->GetPos( x1, y1 );
	m_pBottomRightPanel->GetPos( x2, y2 );
	y1 += m_pTopRightPanel->GetTall();
	x2 += m_pBottomRightPanel->GetWide();
	vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
}

void CAutosaveLabel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Label::ApplySchemeSettings( scheme );
	m_bgColor = GetSchemeColor( "AutosaveNoticeLabelBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	Color cornerColor( 255, 255, 255, m_bgColor.a() );
	m_pTopLeftPanel->m_bgColor = cornerColor;
	m_pTopRightPanel->m_bgColor = cornerColor;
	m_pBottomLeftPanel->m_bgColor = cornerColor;
	m_pBottomRightPanel->m_bgColor = cornerColor;
	SetPaintBorderEnabled( false );
}

ColorFade::ColorFade()
{
	m_startColor = m_endColor;
	m_fadePercent = 0;
	m_direction = down;
}

void ColorFade::SetFade( Color startColor, Color endColor, float fadePercent, FadeDir direction )
{
	m_startColor = startColor;
	m_endColor = endColor;
	m_fadePercent = fadePercent;
	m_direction = direction;
}

void ColorFade::DrawFade( int x, int y, int wide, int tall )
{
	vgui2::surface()->DrawSetColor( m_startColor );
	vgui2::surface()->DrawFilledRect( x, y, x + wide, y + tall );
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 26495 )
#endif
CCareerBaseFrame::CCareerBaseFrame( vgui2::Panel* parent, const char* name, bool showTaskbarIcon )
	: vgui2::Frame( parent, name, showTaskbarIcon, false )
{
	SetHDProportional( HD_PROPORTIONAL_FORCE_DISABLED );
	CCareerGame::Create();
	SetScheme( "CareerScheme" );
	SetProportional( true );
	SetMoveable( false );
	SetSizeable( false );
	SetTitleBarVisible( false );
	SetCloseSound( "sound/UI/launch_dnmenu1.wav" );
	g_pTaskbar->AddTask( GetVPanel() );
	m_pTitleBackground = new vgui2::Label( this, "TitleBackground", "" );
	m_pPlayerName = new vgui2::Label( this, "PlayerNameLabel", "" );
	m_pMissionPackName = new vgui2::Label( this, "MissionPackNameLabel", "" );
	m_showImageBackground = false;
	m_pAutosaveNotice = new CAutosaveLabel( this, "AutosaveNoticeLabel", "" );
	m_pAutosaveNotice->MoveToFront();
	m_pAutosaveNotice->SetVisible( false );
	m_autosaveHideTime = 0;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CCareerBaseFrame::~CCareerBaseFrame()
{
}

void CCareerBaseFrame::ShowAutosaveNotice()
{
	if ( m_pAutosaveNotice )
	{
		float now = (float)vgui2::system()->GetFrameTime();
		m_autosaveHideTime = now + 2.0f;
		m_pAutosaveNotice->SetVisible( true );
		m_pAutosaveNotice->MoveToFront();
	}
}

void CCareerBaseFrame::OnThink()
{
	float now = (float)vgui2::system()->GetFrameTime();
	if ( m_autosaveHideTime == 0 )
		return;

	if ( now > m_autosaveHideTime )
	{
		if ( m_pAutosaveNotice )
			m_pAutosaveNotice->SetVisible( false );
		m_autosaveHideTime = 0;
	}
	else if ( m_pAutosaveNotice )
	{
		m_pAutosaveNotice->MoveToFront();
	}
}

void CCareerBaseFrame::SetImageBackground( bool state )
{
	m_showImageBackground = state;
	if ( !state )
		return;

	int screenWide, screenTall;
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	for ( int row = 0; row < 3; ++row )
	{
		for ( int column = 0; column < 4; ++column )
		{
			bimage_t& image = m_ImageID[row][column];
			image.imageID = vgui2::surface()->CreateNewTextureID( false );
			char filename[512];
			sprintf( filename, "resource/background/SplashGrey_%d_%d", column, row );
			vgui2::surface()->DrawSetTextureFile( image.imageID, filename, 0, false );
			vgui2::surface()->DrawGetTextureSize( image.imageID, image.width, image.height );
		}
	}
}

void CCareerBaseFrame::DrawBackgroundImage()
{
	int wide, tall, screenWide, screenTall;
	GetSize( wide, tall );
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	float xScale = (float)screenWide / CAREER_BACKGROUND_WIDE;
	float yScale = (float)screenTall / CAREER_BACKGROUND_TALL;
	int y = 0;
	for ( int row = 0; row < 3; ++row )
	{
		int x = 0;
		for ( int column = 0; column < 4; ++column )
		{
			bimage_t& image = m_ImageID[row][column];
			int right = x + (int)ceil( (float)( image.width * xScale ) );
			int bottom = y + (int)ceil( (float)( image.height * yScale ) );
			vgui2::surface()->DrawSetColor( 255, 255, 255, 255 );
			vgui2::surface()->DrawSetTexture( image.imageID );
			vgui2::surface()->DrawTexturedRect( x, y, right, bottom );
			x += (int)ceil( (float)( image.width * xScale ) );
		}
		y += (int)ceil( (float)( m_ImageID[row][0].height * yScale ) );
	}
}

void CCareerBaseFrame::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	m_pPlayerName->SetFgColor( scheme->GetColor( "PlayerName", Color( 0, 0, 0, 0 ) ) );
	m_pMissionPackName->SetFgColor( scheme->GetColor( "PlayerName", Color( 0, 0, 0, 0 ) ) );
	for ( int i = 0; i < m_buttons.GetCount(); ++i )
	{
		m_buttons[i]->SetArmedSound( "sound/UI/buttonrollover.wav" );
		m_buttons[i]->SetDepressedSound( "sound/UI/buttonclick.wav" );
		m_buttons[i]->SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
	}

	CareerDefinition* career = TheCareerDefinitions->GetActiveCareerDefinition();
	if ( career )
	{
		m_bgFade.SetFade( career->GetBgStartColor(), career->GetBgEndColor(), 25.0f, ColorFade::down );
		m_pPlayerName->SetFgColor( career->GetTextColor() );
		m_pMissionPackName->SetFgColor( career->GetTextColor() );
	}
}

void CCareerBaseFrame::PaintBackground()
{
	if ( m_showImageBackground )
	{
		DrawBackgroundImage();
		return;
	}

	vgui2::Frame::PaintBackground();
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	m_bgFade.DrawFade( x, y, wide, tall );
	m_pTitleBackground->GetBounds( x, y, wide, tall );
	Color titleColor = vgui2::scheme()->GetIScheme( GetScheme() )->GetColor( "WindowTitleBG", Color( 0, 0, 0, 0 ) );
	int screenWide, screenTall;
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	vgui2::surface()->DrawSetColor( titleColor );
	vgui2::surface()->DrawFilledRect( x, y, x + wide, y + tall );
}

void CCareerBaseFrame::Activate()
{
	vgui2::Frame::Activate();
	CareerDefinition* career = TheCareerDefinitions->GetActiveCareerDefinition();
	if ( career )
	{
		m_bgFade.SetFade( career->GetBgStartColor(), career->GetBgEndColor(), 25.0f, ColorFade::down );
		m_pPlayerName->SetFgColor( career->GetTextColor() );
		m_pMissionPackName->SetFgColor( career->GetTextColor() );
	}

	char nameCvar[] = "name";
	const char* name = engine->pfnGetCvarString( nameCvar );
	if ( !name )
		name = "";
	size_t nameLength = strlen( name ) + 1;
	if ( nameLength > INT_MAX / sizeof( wchar_t ) )
		return;
	std::wstring playerName( nameLength, L'\0' );
	vgui2::localize()->ConvertANSIToUnicode( name, &playerName[0], (int)( nameLength * sizeof( wchar_t ) ) );
	playerName.resize( wcslen( playerName.c_str() ) );
	char difficulty[32];
	sprintf( difficulty, "#Cstrike_Bot_Difficulty%d", TheCareerGame->GetDifficulty() );
	const wchar_t* difficultyText = vgui2::localize()->Find( difficulty );
	if ( difficultyText )
	{
		playerName += L" / ";
		playerName += difficultyText;
	}
	m_pPlayerName->SetText( playerName.c_str() );
	m_pMissionPackName->SetText( TheCareerDefinitions->GetActiveCareerDefinition()->GetTitle() );
}

int GetAlternateProportionalValueFromNormal( vgui2::Panel* panel, int normalizedValue )
{
	int wide, tall, proportionalWide, proportionalTall;
	vgui2::surface()->GetScreenSize( wide, tall );
	panel->GetProportionalBase( proportionalWide, proportionalTall );
	double scale = (double)tall / proportionalTall;
	double widthScale = (double)wide / proportionalWide;
	if ( scale > widthScale )
		scale = widthScale;
	return (int)( scale * normalizedValue );
}

int GetAlternateProportionalValueFromScaled( vgui2::Panel* panel, int scaledValue )
{
	return GetAlternateProportionalValueFromNormal( panel, panel->GetProportionalNormalizedValue( scaledValue ) );
}

void RepositionControl( vgui2::Panel* panel )
{
	int x, y, wide, tall;
	panel->GetBounds( x, y, wide, tall );
	x = GetAlternateProportionalValueFromScaled( panel, x );
	y = GetAlternateProportionalValueFromScaled( panel, y );
	wide = GetAlternateProportionalValueFromScaled( panel, wide );
	tall = GetAlternateProportionalValueFromScaled( panel, tall );
	panel->SetBounds( x, y, wide, tall );
}

void CCareerBaseFrame::PerformLayout()
{
	SetPos( 0, 0 );
	vgui2::Frame::PerformLayout();
	if ( !IsProportional() )
		return;

	int screenWide, screenTall, wide, tall;
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	GetSize( wide, tall );
	if ( wide == screenWide && tall == screenTall )
		return;

	SetSize( screenWide, screenTall );
	wide = GetAlternateProportionalValueFromScaled( this, wide );
	tall = GetAlternateProportionalValueFromScaled( this, tall );
	if ( !GetBuildGroup() || !GetBuildGroup()->GetPanelList() )
		return;

	CUtlVector<vgui2::PHandle>* panels = GetBuildGroup()->GetPanelList();
	CUtlVector<vgui2::Panel*> resizedPanels;
	CUtlVector<vgui2::Panel*> movedPanels;
	for ( int i = 0; i < panels->Count(); ++i )
	{
		vgui2::PHandle handle = ( *panels )[i];
		vgui2::Panel* panel = handle.Get();
		if ( !panel || resizedPanels.Find( panel ) >= 0 )
			continue;

		resizedPanels.AddToTail( panel );
		if ( panel == m_pTitleBackground )
			RepositionControl( m_pTitleBackground );
		else if ( panel != this )
			RepositionControl( panel );
	}

	int offsetX = ( screenWide - wide ) / 2;
	int offsetY = ( screenTall - tall ) / 2;
	for ( int i = 0; i < panels->Count(); ++i )
	{
		vgui2::PHandle handle = ( *panels )[i];
		vgui2::Panel* panel = handle.Get();
		if ( !panel || movedPanels.Find( panel ) >= 0 )
			continue;

		movedPanels.AddToTail( panel );
		if ( panel == m_pTitleBackground )
		{
			panel->SetSize( screenWide, offsetY + m_pTitleBackground->GetTall() );
		}
		else if ( panel != this )
		{
			int x, y;
			panel->GetPos( x, y );
			panel->SetPos( x + offsetX, y + offsetY );
		}
	}
}
