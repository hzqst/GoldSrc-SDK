//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "../../../vgui2/controls/Frame.h"

namespace vgui2
{
class ImagePanel;
class Label;
}
class CBottomBar;

class ISpectatorInterface
{
public:
	virtual void Activate() = 0;
	virtual void ShowBottomBar() = 0;
	virtual void HideGUI() = 0;
	virtual void DeactivateGUI() = 0;
	virtual bool IsGUIVisible() = 0;
	virtual bool IsActiveGUIVisible() = 0;
	virtual void EnableInsetView( int enabled ) = 0;
	virtual void SetImage( const char* image ) = 0;
	virtual void Update( const char* serverName, int player, int mode, bool spectateOnly,
		int spectatorCount, const char* mapName ) = 0;
	virtual void UpdateSpectatorPlayerList() = 0;
	virtual void UpdateTimer() = 0;
	virtual void SetVisible( bool state ) = 0;
	virtual bool IsVisible() = 0;
	virtual void SetParent( vgui2::VPANEL parent ) = 0;
	virtual int GetTopBarHeight() = 0;
	virtual int GetBottomBarHeight() = 0;
};

class CSpectatorGUI : public vgui2::Frame, public ISpectatorInterface
{
	DECLARE_CLASS_SIMPLE_NAMED( CSpectatorGUI, vgui2::Frame, "vgui::Frame" );

public:
	CSpectatorGUI( vgui2::Panel* parent );
	~CSpectatorGUI() override;
	void Activate() override;
	void ShowBottomBar() override;
	void EnableInsetView( int enabled ) override;
	void SetImage( const char* image ) override;
	void Update( const char* serverName, int player, int mode, bool spectateOnly,
		int spectatorCount, const char* mapName ) override;
	void UpdateSpectatorPlayerList() override;
	void UpdateTimer() override;
	void HideGUI() override;
	void DeactivateGUI() override;
	bool IsGUIVisible() override;
	bool IsActiveGUIVisible() override;
	int GetTopBarHeight() override;
	int GetBottomBarHeight() override;
	bool IsVisible() override { return BaseClass::IsVisible(); }
	void SetParent( vgui2::VPANEL parent ) override { BaseClass::SetParent( parent ); }
	void SetVisible( bool state ) override { BaseClass::SetVisible( state ); }

	void SetLabelText( const char* name, const char* text );
	void SetLabelText( const char* name, wchar_t* text );
	void MoveLabelToFront( const char* name );
	virtual bool CanSpectateTeam( int team );
	virtual void GetPlayerLabelText( int player, wchar_t* text, int textLength );
	virtual void GetPlayerLabelColor( int player, Color& color );

protected:
	void OnCommand( const char* command ) override;
	void OnKeyCodePressed( vgui2::KeyCode code ) override;
	void OnThink() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void PerformLayout() override;

private:
	vgui2::Panel*		m_pTopBar;
	CBottomBar*			m_pBottomBar;
	vgui2::Panel*		m_pBottomBarBlank;
	vgui2::Panel*		m_pInsetBox;
	vgui2::ImagePanel*	m_pBannerImage;
	vgui2::KeyCode		m_iDuckKey;
	vgui2::Label*		m_pPlayerLabel;
	vgui2::Label*		m_pExtraInfoLabel;
	vgui2::Label*		m_pTimerLabel;
	bool				m_bHelpShown;
	bool				m_bInsetVisible;
};
