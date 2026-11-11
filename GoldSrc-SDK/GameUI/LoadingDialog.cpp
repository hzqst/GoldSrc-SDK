//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "LoadingDialog.h"
#include "BitmapImagePanel.h"
#include "CareerGame.h"
#include "CareerMapFrame.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"

#include "Button.h"
#include "Label.h"
#include "ProgressBar.h"
#include "RichText.h"
#include "igameuifuncs.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

class CareerRichText : public vgui2::RichText
{
public:
	CareerRichText( vgui2::Panel* parent, const char* name ) : vgui2::RichText( parent, name, true )
	{
		m_fontHeight = 0;
		m_addedScrollbar = false;
	}
	~CareerRichText() override;

protected:
	void PerformLayout() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

private:
	int		m_fontHeight;
	bool	m_addedScrollbar;
};

CareerRichText::~CareerRichText()
{
}

void CareerRichText::PerformLayout()
{
	vgui2::RichText::PerformLayout();
	int wide, tall;
	GetSize( wide, tall );
	if ( ( m_fontHeight + 1 ) * GetNumLines() + 3 > tall && !m_addedScrollbar )
	{
		SetVerticalScrollbar( true );
		m_addedScrollbar = true;
		InsertString( L"" );
	}
}

void CareerRichText::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::RichText::ApplySchemeSettings( scheme );
	vgui2::HFont font = scheme->GetFontEx( "Default", IsProportional(), IsHDProportional() );
	m_fontHeight = vgui2::surface()->GetFontTall( font );
	m_addedScrollbar = false;
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CLoadingDialog::CLoadingDialog( vgui2::Panel* parent )
	: vgui2::Frame( parent, "LoadingDialog", true, true )
{
	SetHDProportional( true );
	SetSize( GetProportionalScaledValue( 416 ), GetProportionalScaledValue( 100 ) );
	SetTitle( "#GameUI_Loading", true );
	m_bShowingSecondaryProgress = false;
	m_flSecondaryProgress = 0;
	m_flLastSecondaryProgressUpdateTime = 0;
	m_flSecondaryProgressStartTime = 0;
	m_isCareerLoadingDialog = false;
	m_switchTime = 0;
	m_flProgress = 0;
	m_isHttpDownloadDialog = false;
	m_bShowingVACInfo = false;
	m_bShowingVACError = false;
	m_szUserName[0] = 0;
	m_pCareerMapImagePanel = NULL;
	m_pProgress = new vgui2::ProgressBar( this, "Progress" );
	m_pProgress2 = new vgui2::ProgressBar( this, "Progress2" );
	m_pInfoLabel = new vgui2::Label( this, "InfoLabel", "" );
	m_pCancelButton = new vgui2::Button( this, "CancelButton", "#GameUI_Cancel" );
	m_pTimeRemainingLabel = new vgui2::Label( this, "TimeRemainingLabel", "" );
	m_pCancelButton->SetCommand( "Cancel" );
	m_pCareerMapDescLabel = NULL;
	m_pInfoLabel->SetBounds( GetProportionalScaledValue( 20 ), GetProportionalScaledValue( 32 ),
			GetProportionalScaledValue( 392 ), GetProportionalScaledValue( 24 ) );
	m_pProgress->SetBounds( GetProportionalScaledValue( 20 ), GetProportionalScaledValue( 64 ),
			GetProportionalScaledValue( 300 ), GetProportionalScaledValue( 24 ) );
	m_pCancelButton->SetBounds( GetProportionalScaledValue( 330 ), GetProportionalScaledValue( 64 ),
			GetProportionalScaledValue( 72 ), GetProportionalScaledValue( 24 ) );
	m_pInfoLabel->SetContentAlignment( vgui2::Label::a_north );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetSizeable( false );
	SetMoveable( false );
	m_pProgress2->SetVisible( false );
	if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
	{
		m_pCareerMapDescLabel = new CareerRichText( this, "CareerInfo" );
		m_isCareerLoadingDialog = true;
		char filename[128];
		filename[sizeof( filename ) - 1] = 0;
		sprintf( filename, "gfx/thumbnails/maps/%s", TheCareerGame->GetLastMap() );
		m_pCareerMapImagePanel = new CBitmapImagePanel( this, "CareerMapImage", NULL );
		m_pCareerMapImagePanel->AddActionSignalTarget( this );
		m_pCareerMapImagePanel->setTexture( filename );
		m_pCareerMapLabel = new vgui2::Label( this, "CareerMapTitle", "" );
#ifdef _WIN32
		_snprintf( filename, sizeof( filename ), "#Career_Map_%s", TheCareerGame->GetLastMap() );
#else
		snprintf( filename, sizeof( filename ), "#Career_Map_%s", TheCareerGame->GetLastMap() );
#endif
		filename[sizeof( filename ) - 1] = 0;
		if ( !vgui2::localize()->Find( filename ) )
		{
#ifdef _WIN32
			_snprintf( filename, sizeof( filename ), "%s", TheCareerGame->GetLastMap() );
#else
			snprintf( filename, sizeof( filename ), "%s", TheCareerGame->GetLastMap() );
#endif
		}
		m_pCareerMapLabel->SetText( filename );
		m_pCareerScenarioInfo = new vgui2::Label( this, "CareerScenarioInfo", "" );
		if ( TheCareerGame->PlayAsCT() )
		{
			m_pCareerScenarioInfo->SetText( !strncmp( "de_", TheCareerGame->GetLastMap(), 3 ) ?
					"#Career_BombDefusalCT" : "#Career_HostageRescueCT" );
		}
		else
		{
			m_pCareerScenarioInfo->SetText( !strncmp( "de_", TheCareerGame->GetLastMap(), 3 ) ?
					"#Career_BombDefusalT" : "#Career_HostageRescueT" );
		}
		vgui2::IScheme* scheme = vgui2::scheme()->GetIScheme( GetScheme() );
		Color normal = scheme->GetColor( "ControlText", Color( 255, 255, 255, 255 ) );
		Color friendlyFire = scheme->GetColor( "BrightControlText", Color( 255, 242, 0, 255 ) );
		ShowCareerMapDescription( m_pCareerMapDescLabel, TheCareerGame->GetLastMapPtr(), normal, friendlyFire );
		LoadControlSettings( "Resource/CareerLoadingDialog.res", NULL );
		m_pCareerMapDescLabel->SetVerticalScrollbar( false );
	}
	else if ( engine->CheckParm( "-steam", NULL ) || engine->CheckParm( "-showplatform", NULL ) )
	{
		if ( gameuifuncs->IsConnectedToVACSecureServer() )
		{
			LoadControlSettings( "Resource/LoadingDialogVAC.res", NULL );
			m_bShowingVACInfo = true;
		}
		else
		{
			LoadControlSettings( "Resource/LoadingDialog.res", NULL );
		}
	}
	else
	{
		LoadControlSettings( ModInfo()->BShowSimpleLoadingDialog() ?
				"Resource/LoadingDialogNoBannerSingle.res" : "Resource/LoadingDialogNoBanner.res", NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLoadingDialog::~CLoadingDialog()
{
}

CLoadingDialog* LoadingDialog()
{
	return g_hLoadingDialog.Get();
}

//-----------------------------------------------------------------------------
// Purpose: Activates the loading screen, initializing and making it visible
//-----------------------------------------------------------------------------
void CLoadingDialog::DisplayProgressBar( const char*, const char* )
{
	SetTitle( "#GameUI_Loading", true );
	vgui2::surface()->RestrictPaintToSinglePanel( GetVPanel() );
	vgui2::Frame::Activate();
	m_pProgress->SetVisible( true );
	if ( !ModInfo()->BShowSimpleLoadingDialog() )
		m_pInfoLabel->SetVisible( true );
	m_pInfoLabel->SetText( "" );
	m_pCancelButton->SetText( "#GameUI_Cancel" );
	m_pCancelButton->SetCommand( "Cancel" );
}

void CLoadingDialog::SetupControlSettingsForErrorDisplay( const char* settingsFile )
{
	SetTitle( "#GameUI_Disconnected", true );
	LoadControlSettings( settingsFile, NULL );
	vgui2::surface()->RestrictPaintToSinglePanel( GetVPanel() );
	vgui2::Frame::Activate();
	m_pProgress->SetVisible( false );
	m_pInfoLabel->SetVisible( true );
	m_pCancelButton->SetText( "#GameUI_Close" );
	m_pCancelButton->SetCommand( "Close" );
	m_pInfoLabel->SetText( "" );
	m_pCancelButton->RequestFocus( 0 );
	if ( m_pCareerMapDescLabel )
	{
		m_pCareerMapDescLabel->SetVisible( false );
		m_pCareerMapDescLabel->SetVisible( false );
		m_pCareerMapLabel->SetVisible( false );
		m_pCareerScenarioInfo->SetVisible( false );
		m_pCareerMapImagePanel->SetVisible( false );
	}
}

void CLoadingDialog::DisplayGenericError( const char* failureReason, const char* extendedReason )
{
	if ( m_bShowingVACError )
	{
		m_bShowingVACError = false;
		return;
	}
	SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogError.res" );
	if ( extendedReason && extendedReason[0] )
	{
		wchar_t compositeReason[256];
		wchar_t format[256];
		wchar_t message[512];
		const wchar_t* localized = extendedReason[0] == '#' ? vgui2::localize()->Find( extendedReason ) : NULL;
		if ( localized )
			wcsncpy( compositeReason, localized, sizeof( compositeReason ) / sizeof( compositeReason[0] ) );
		else
			vgui2::localize()->ConvertANSIToUnicode( extendedReason, compositeReason, sizeof( compositeReason ) );
		localized = failureReason[0] == '#' ? vgui2::localize()->Find( failureReason ) : NULL;
		if ( localized )
			wcsncpy( format, localized, sizeof( format ) / sizeof( format[0] ) );
		else
			vgui2::localize()->ConvertANSIToUnicode( failureReason, format, sizeof( format ) );
		vgui2::localize()->ConstructString( message, sizeof( message ), format, 1, compositeReason );
		m_pInfoLabel->SetText( message );
	}
	else
	{
		m_pInfoLabel->SetText( failureReason );
	}
}

void CLoadingDialog::DisplayVACBannedError()
{
	SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogErrorVACBanned.res" );
	SetTitle( "#VAC_ConnectionRefusedTitle", true );
	m_bShowingVACError = true;
}

void CLoadingDialog::DisplayNoSteamConnectionError()
{
	SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogErrorNoSteamConnection.res" );
	m_bShowingVACError = true;
}

void CLoadingDialog::DisplaySteamConnectionError( const char* username, ESteamLoginFailure failure )
{
	switch ( failure )
	{
	case STEAMLOGINFAILURE_CONNECTIONLOST:
		SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogConnectionLost.res" );
		break;
	case STEAMLOGINFAILURE_NOCONNECTION:
		SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogErrorNoSteamConnection.res" );
		break;
	case STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE:
		SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogErrorLoggedInElsewhere.res" );
		break;
	default:
		break;
	}
	m_bShowingVACError = true;
	m_pCancelButton->SetText( "#GameUI_Close" );
	m_pCancelButton->SetCommand( "close" );
	strncpy( m_szUserName, username, sizeof( m_szUserName ) );
	m_szUserName[sizeof( m_szUserName ) - 1] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: sets status info text
//-----------------------------------------------------------------------------
void CLoadingDialog::SetStatusText( const char* statusText )
{
	m_pInfoLabel->SetText( statusText );
}

void CLoadingDialog::SetProgressPoint( int progressPoint )
{
	if ( !m_bShowingVACInfo && gameuifuncs->IsConnectedToVACSecureServer() )
	{
		LoadControlSettings( "Resource/LoadingDialogVAC.res", NULL );
		m_bShowingVACInfo = true;
	}
#ifdef _WIN32
	m_flProgress = (float)progressPoint / (float)( m_iRangeMax - m_iRangeMin );
#else
	m_flProgress = (long double)progressPoint / (long double)( m_iRangeMax - m_iRangeMin );
#endif
	m_pProgress->SetProgress( m_flProgress );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadingDialog::SetProgressRange( int min, int max )
{
	m_iRangeMin = min;
	m_iRangeMax = max;
}

void CLoadingDialog::SetSecondaryProgress( float progress )
{
	if ( !m_bShowingSecondaryProgress )
	{
		if ( progress > 0.99f )
			return;
		if ( m_bShowingVACInfo || gameuifuncs->IsConnectedToVACSecureServer() )
		{
			LoadControlSettings( "Resource/LoadingDialogDualProgressVAC.res", NULL );
			m_bShowingVACInfo = true;
		}
		else
		{
			LoadControlSettings( "Resource/LoadingDialogDualProgress.res", NULL );
		}
		m_bShowingSecondaryProgress = true;
		m_pProgress->SetProgress( m_flProgress );
		m_pProgress2->SetVisible( true );
		m_flSecondaryProgressStartTime = (float)vgui2::system()->GetFrameTime();
	}
	// if progress has increased then update the progress counters
	if ( progress > m_flSecondaryProgress )
	{
		m_pProgress2->SetProgress( progress );
		m_flSecondaryProgress = progress;
		m_flLastSecondaryProgressUpdateTime = (float)vgui2::system()->GetFrameTime();
	}
	if ( m_flSecondaryProgress > progress )
	{
		m_pProgress2->SetProgress( progress );
		m_flSecondaryProgress = progress;
		m_flLastSecondaryProgressUpdateTime = (float)vgui2::system()->GetFrameTime();
		m_flSecondaryProgressStartTime = (float)vgui2::system()->GetFrameTime();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadingDialog::SetSecondaryProgressText( const char* statusText )
{
	SetControlString( "SecondaryProgressLabel", statusText );
}

void CLoadingDialog::SwitchToHttpDownloadDialog( const char* )
{
	m_isHttpDownloadDialog = true;
	LoadControlSettings( "Resource/LoadingDialog.res", NULL );
}

void CLoadingDialog::SwitchToPausedCareerDialog()
{
	if ( m_isCareerLoadingDialog )
	{
		m_switchTime = (float)( vgui2::system()->GetFrameTime() + 1.5 );
	}
	else
	{
		GameUI()->FinishCareerLoad();
		Close();
	}
}

void CLoadingDialog::CompleteSwitchToPausedCareerDialog()
{
	if ( !m_isCareerLoadingDialog )
		return;
	m_switchTime = 0;
	m_pCancelButton->SetText( "#Career_Start" );
	m_pCancelButton->SetCommand( "StartCareer" );
	int buttonX, buttonY, buttonWide, buttonTall;
	int progressX, progressY, progressWide, progressTall;
	int infoX, infoY, infoWide, infoTall;
	m_pCancelButton->GetBounds( buttonX, buttonY, buttonWide, buttonTall );
	m_pProgress->GetBounds( progressX, progressY, progressWide, progressTall );
	m_pInfoLabel->GetBounds( infoX, infoY, infoWide, infoTall );
	buttonX = GetWide() / 2 - buttonWide;
	buttonY = infoY + ( progressTall + progressY - infoY ) / 2 - buttonTall / 2;
	m_pProgress->SetVisible( false );
	m_pInfoLabel->SetVisible( false );
	m_pCancelButton->SetBounds( buttonX, buttonY, 2 * buttonWide, buttonTall );
	m_pCancelButton->SetContentAlignment( vgui2::Label::a_center );
	m_pCancelButton->RequestFocus( 0 );
}

void CLoadingDialog::OnThink()
{
	vgui2::Frame::OnThink();
	if ( m_bShowingSecondaryProgress )
	{
		if ( m_flSecondaryProgress < 1.0f )
		{
			wchar_t text[512];
			float progress = m_flSecondaryProgress;
			float lastUpdate = m_flLastSecondaryProgressUpdateTime;
			float now = (float)vgui2::system()->GetFrameTime();
			if ( vgui2::ProgressBar::ConstructTimeRemainingString( text, sizeof( text ),
					m_flSecondaryProgressStartTime, now, progress, lastUpdate, true ) )
				m_pTimeRemainingLabel->SetText( text );
			else
				m_pTimeRemainingLabel->SetText( "" );
		}
		else
		{
			m_pTimeRemainingLabel->SetText( "complete" );
		}
	}
	if ( m_switchTime )
	{
		float switchTime = m_switchTime;
		if ( vgui2::system()->GetFrameTime() > switchTime && m_isCareerLoadingDialog )
			CompleteSwitchToPausedCareerDialog();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadingDialog::PerformLayout()
{
	MoveToCenterOfScreen();
	vgui2::Frame::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CLoadingDialog::OnClose()
{
	vgui2::surface()->RestrictPaintToSinglePanel( 0 );
	vgui2::Frame::OnClose();
	// delete ourselves
	MarkForDeletion();
}

void CLoadingDialog::OnCommand( const char* command )
{
	if ( !stricmp( command, "Cancel" ) )
	{
		if ( m_isHttpDownloadDialog )
		{
			engine->pfnClientCmd( "httpstop\n" );
		}
		else
		{
			if ( TheCareerGame && TheCareerGame->IsPlayingMatch() && !m_pProgress->IsVisible() )
				GameUI()->FinishCareerLoad();
			engine->pfnClientCmd( "disconnect\n" );
		}
		Close();
	}
	else if ( !stricmp( command, "StartCareer" ) )
	{
		GameUI()->FinishCareerLoad();
		Close();
	}
	else
	{
		vgui2::Frame::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Maps ESC to quiting loading
//-----------------------------------------------------------------------------
void CLoadingDialog::OnKeyCodePressed( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_ESCAPE )
		OnCommand( "Cancel" );
	else
		vgui2::Frame::OnKeyCodePressed( code );
}

void CLoadingDialog::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	if ( m_pCareerMapImagePanel )
		m_pCareerMapImagePanel->SetBorder( scheme->GetBorder( "InsetBorder" ) );
}
