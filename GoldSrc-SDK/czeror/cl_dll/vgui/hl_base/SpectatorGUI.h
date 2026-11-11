//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SPECTATORGUI_H
#define SPECTATORGUI_H

#include "Frame.h"
#include "ispectatorinterface.h"

class CBottomBar;
namespace vgui2
{
class ImagePanel;
class Label;
}

class CSpectatorGUI : public vgui2::Frame, public ISpectatorInterface
{
	DECLARE_CLASS_SIMPLE( CSpectatorGUI, vgui2::Frame );

public:
	CSpectatorGUI( vgui2::Panel* parent );
	~CSpectatorGUI() override;
	void Activate() override;
	void ShowBottomBar() override;
	void EnableInsetView( int enabled ) override;
	void SetImage( const char* image ) override;
	void Update( const char* bottomText, int player, int mode, bool spectatorOnly, int spectatorCount,
		const char* mapName ) override;
	void UpdateSpectatorPlayerList() override;
	void UpdateTimer() override;
	void HideGUI() override;
	void DeactivateGUI() override;
	bool IsGUIVisible() override;
	bool IsActiveGUIVisible() override;
	bool IsVisible() override;
	void SetVisible( bool state ) override;
	void SetParent( vgui2::VPANEL parent ) override;
	int GetTopBarHeight() override;
	int GetBottomBarHeight() override;
	void SetLabelText( const char* name, const char* text );
	void SetLabelText( const char* name, wchar_t* text );
	void MoveLabelToFront( const char* name );
	virtual bool CanSpectateTeam( int team );
	void PerformLayout() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void OnKeyCodePressed( vgui2::KeyCode code ) override;
	void OnCommand( const char* command ) override;
	void LayoutAd();

private:
	vgui2::Panel* m_pTopBar;
	CBottomBar* m_pBottomBar;
	vgui2::Panel* m_pBottomBarBlank;
	vgui2::Panel* m_pInsetBox;
	vgui2::ImagePanel* m_pBannerImage;
	vgui2::Label* m_pPlayerLabel;
	vgui2::Panel* m_pBanner;
	vgui2::KeyCode m_iDuckKey;
	bool m_bHelpShown;
	bool m_bInsetVisible;
};

#endif // SPECTATORGUI_H
