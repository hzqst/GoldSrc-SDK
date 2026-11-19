#pragma once

#include "ImagePanel.h"
#include "vgui/IImage.h"
#include "steamclientpublic.h"

#include <math.h>

class CCSClientScoreBoardDialog;
class CAvatarImagePanel;

class CAvatarImage : public vgui2::IImage
{
	friend class CCSClientScoreBoardDialog;
	friend class CAvatarImagePanel;

public:
	CAvatarImage( vgui2::IImage* blockedImage, vgui2::IImage* friendImage );
	~CAvatarImage() override;

	bool SetAvatarSteamID( CSteamID steamID );
	void UpdateFriendStatus();
	void ClearAvatarSteamID();
	void Paint() override;
	void SetPos( int x, int y ) override;
	void GetContentSize( int& width, int& height ) override;
	void GetSize( int& width, int& height ) override;
	void SetSize( int width, int height ) override;
	void SetAdditive( bool additive ) override;

	void SetAvatarSize( int wide, int tall )
	{
		m_iAvatarWidth = wide;
		m_iAvatarHeight = tall;

		double scale = wide / 32.0;
		m_targetFriendImageWide = (int)( scale * 20.0 );
		m_targetBlockedWide = (int)( scale * 12.0 );
		m_targetBlockedXInset = (int)( scale * 2.0 );
		m_targetBlockedYInset = (int)ceil( scale * 10.0 );
		m_targetAvatarXInset = (int)( scale * 16.0 );

		SetSize( wide + 5 + m_targetAvatarXInset, tall );
	}

	void SetColor( Color color ) override;
	bool IsValid();
	int GetWide() { return m_nWide; }
	void InitFromRGBA( const unsigned char* rgba, int width, int height );

private:
	Color m_Color;
	int m_iTextureID;
	int m_nX;
	int m_nY;
	int m_nWide;
	int m_nTall;
	int m_targetFriendImageWide;
	int m_targetBlockedWide;
	int m_targetBlockedXInset;
	int m_targetBlockedYInset;
	int m_targetAvatarXInset;
	bool m_bAdditive;
	bool m_bValid;
	bool m_bBlockedFriend;
	bool m_bFriend;
	int m_iAvatarWidth;
	int m_iAvatarHeight;
	CSteamID m_SteamID;
	vgui2::IImage* m_pBlockedImage;
	vgui2::IImage* m_pFriendImage;
};

class CAvatarImagePanel : public vgui2::ImagePanel
{
	DECLARE_CLASS_SIMPLE_NAMED( CAvatarImagePanel, vgui2::ImagePanel, "vgui::ImagePanel" );

public:
	CAvatarImagePanel( vgui2::Panel* parent, const char* name );
	~CAvatarImagePanel() override;
	void SetPlayer( int player, vgui2::IImage* blockedImage, vgui2::IImage* friendImage );
	using BaseClass::SetImage;
	void SetImage( vgui2::IImage* image ) override;
	void PaintBackground() override;
	bool IsValid();

private:
	// the avatar image this panel created and deletes, NULL once another image replaces it
	CAvatarImage* m_pAvatarImage;
};
