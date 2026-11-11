//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"
#include "common/steamloginfailure.h"

class CareerRichText;
class CBitmapImagePanel;

namespace vgui2
{
class Button;
class Label;
class ProgressBar;
}

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying level loading status
//-----------------------------------------------------------------------------
class CLoadingDialog : public vgui2::Frame
{
public:
	CLoadingDialog( vgui2::Panel* parent );
	~CLoadingDialog() override;
	void DisplayProgressBar( const char* resourceType, const char* resourceName );
	void SetupControlSettingsForErrorDisplay( const char* settingsFile );
	void DisplayGenericError( const char* failureReason, const char* extendedReason );
	void DisplayVACBannedError();
	void DisplayNoSteamConnectionError();
	void DisplaySteamConnectionError( const char* username, ESteamLoginFailure failure );
	void SetStatusText( const char* statusText );
	void SetProgressPoint( int progressPoint );
	void SetProgressRange( int min, int max );
	void SetSecondaryProgress( float progress );
	void SetSecondaryProgressText( const char* statusText );
	void SwitchToHttpDownloadDialog( const char* url );
	void SwitchToPausedCareerDialog();
	void CompleteSwitchToPausedCareerDialog();

protected:
	void OnThink() override;
	void PerformLayout() override;
	void OnClose() override;
	void OnCommand( const char* command ) override;
	void OnKeyCodePressed( vgui2::KeyCode code ) override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

private:
	vgui2::ProgressBar*	m_pProgress;
	vgui2::ProgressBar*	m_pProgress2;
	vgui2::Label*		m_pInfoLabel;
	vgui2::Label*		m_pTimeRemainingLabel;
	vgui2::Button*		m_pCancelButton;
	CareerRichText*		m_pCareerMapDescLabel;
	vgui2::Label*		m_pCareerMapLabel;
	vgui2::Label*		m_pCareerScenarioInfo;
	CBitmapImagePanel*	m_pCareerMapImagePanel;
	float				m_switchTime;
	int					m_iRangeMin;
	int					m_iRangeMax;
	bool				m_bShowingSecondaryProgress;
	float				m_flProgress;
	float				m_flSecondaryProgress;
	float				m_flLastSecondaryProgressUpdateTime;
	float				m_flSecondaryProgressStartTime;
	bool				m_isCareerLoadingDialog;
	bool				m_isHttpDownloadDialog;
	bool				m_bShowingVACInfo;
	bool				m_bShowingVACError;
	char				m_szUserName[64];
};

// singleton accessor
CLoadingDialog*	LoadingDialog();
