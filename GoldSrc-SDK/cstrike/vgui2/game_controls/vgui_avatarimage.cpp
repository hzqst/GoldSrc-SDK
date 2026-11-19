#include "vgui_avatarimage.h"
#include "iviewport.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "isteamfriends.h"
#include "isteamutils.h"
#include <stdlib.h>

CAvatarImage::CAvatarImage( vgui2::IImage* blockedImage, vgui2::IImage* friendImage )
{
	m_pBlockedImage = blockedImage;
	m_Color = Color( 0, 0, 0, 0 );
	ClearAvatarSteamID();
	m_pFriendImage = friendImage;
	m_bAdditive = false;
	m_iTextureID = 0;
}

CAvatarImage::~CAvatarImage()
{
	if ( m_iTextureID != 0 )
		vgui2::surface()->DeleteTextureByID( m_iTextureID );
}

void CAvatarImage::ClearAvatarSteamID()
{
	m_bValid = false;
	m_bBlockedFriend = false;
	m_bFriend = false;
	m_nY = 0;
	m_nX = 0;
	m_SteamID = CSteamID( 0, k_unSteamUserDefaultInstance, k_EUniverseInvalid, k_EAccountTypeInvalid );
}

bool CAvatarImage::SetAvatarSteamID( CSteamID steamID )
{
	ClearAvatarSteamID();
	if ( SteamFriends() && SteamUtils() )
	{
		m_SteamID = steamID;
		int image = SteamFriends()->GetMediumFriendAvatar( steamID );
		uint32 width, height;
		if ( SteamUtils()->GetImageSize( image, &width, &height ) )
		{
			unsigned int size = 4 * height * width;
			unsigned char* rgba = (unsigned char*)malloc( size );
			if ( SteamUtils()->GetImageRGBA( image, rgba, (int)size ) )
				InitFromRGBA( rgba, (int)width, (int)height );
			free( rgba );
		}
		UpdateFriendStatus();
	}
	return m_bValid;
}

void CAvatarImage::InitFromRGBA( const unsigned char* rgba, int width, int height )
{
	// a reused image, like the one of CAvatarImagePanel, frees the texture of its previous avatar
	if ( m_iTextureID != 0 )
		vgui2::surface()->DeleteTextureByID( m_iTextureID );
	m_iTextureID = vgui2::surface()->CreateNewTextureID( true );
	vgui2::surface()->DrawSetTextureRGBA( m_iTextureID, rgba, width, height, true, true );
	m_Color = Color( 255, 255, 255, 255 );
	m_nWide = width;
	m_nTall = height;
	m_bValid = true;
}

void CAvatarImage::UpdateFriendStatus()
{
	if ( m_SteamID.IsValid() && SteamFriends() && SteamUtils() )
	{
		m_bBlockedFriend = false;
		m_bFriend = false;
		EFriendRelationship relationship = SteamFriends()->GetFriendRelationship( m_SteamID );
		if ( relationship == k_EFriendRelationshipIgnored || relationship == k_EFriendRelationshipIgnoredFriend
			|| relationship == k_EFriendRelationshipBlocked )
			m_bBlockedFriend = true;
		if ( relationship == k_EFriendRelationshipIgnoredFriend || relationship == k_EFriendRelationshipFriend )
			m_bFriend = true;
	}
}

void CAvatarImage::Paint()
{
	if ( !m_bValid )
		return;

	if ( m_bFriend )
	{
		m_pFriendImage->SetPos( m_nX, m_nY + 1 );
		m_pFriendImage->SetSize( m_targetFriendImageWide, m_iAvatarHeight );
		m_pFriendImage->SetColor( Color( 255, 255, 255, 255 ) );
		m_pFriendImage->Paint();
	}
	if ( m_bBlockedFriend )
	{
		m_pBlockedImage->SetPos( m_nX + m_targetBlockedXInset, m_nY + m_targetBlockedYInset + 1 );
		m_pBlockedImage->SetSize( m_targetBlockedWide, m_targetBlockedWide );
		m_pBlockedImage->SetColor( Color( 255, 255, 255, 255 ) );
		m_pBlockedImage->Paint();
	}
	vgui2::surface()->DrawSetColor( m_Color );
	vgui2::surface()->DrawSetTexture( m_iTextureID );
	int x = m_nX + m_targetAvatarXInset;
	vgui2::surface()->DrawTexturedRect( x, m_nY + 1, x + m_iAvatarWidth, m_nY + 1 + m_iAvatarHeight );
}

void CAvatarImage::SetPos( int x, int y )
{
	m_nX = x;
	m_nY = y;
}

void CAvatarImage::GetContentSize( int& width, int& height )
{
	width = m_nWide;
	height = m_nTall;
}

void CAvatarImage::GetSize( int& width, int& height )
{
	GetContentSize( width, height );
}

void CAvatarImage::SetSize( int width, int height )
{
	m_nWide = width;
	m_nTall = height;
}

void CAvatarImage::SetColor( Color color )
{
	m_Color = color;
}

void CAvatarImage::SetAdditive( bool additive )
{
	m_bAdditive = additive;
}

CAvatarImagePanel::CAvatarImagePanel( vgui2::Panel* parent, const char* name ) : vgui2::ImagePanel( parent, name )
{
}

CAvatarImagePanel::~CAvatarImagePanel()
{
}

void CAvatarImagePanel::PaintBackground()
{
	if ( GetImage() )
		GetImage()->Paint();
}

void CAvatarImagePanel::SetPlayer( int player, vgui2::IImage* blockedImage, vgui2::IImage* friendImage )
{
	if ( GetImage() )
		( (CAvatarImage*)GetImage() )->ClearAvatarSteamID();
	if ( player && SteamUtils() )
	{
		VGuiLibraryPlayer_t info = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( player );
		if ( info.m_nSteamID )
		{
			CSteamID steamID( info.m_nSteamID );
			if ( !GetImage() )
				SetImage( new CAvatarImage( blockedImage, friendImage ) );
			CAvatarImage* image = (CAvatarImage*)GetImage();
			image->SetAvatarSteamID( steamID );
			image->m_iAvatarWidth = 32;
			image->m_iAvatarHeight = 32;
			image->m_targetFriendImageWide = 20;
			image->m_targetBlockedWide = 12;
			image->m_targetBlockedXInset = 2;
			image->m_targetBlockedYInset = 10;
			image->m_targetAvatarXInset = 16;
			image->SetSize( 53, 32 );
			SetSize( image->GetWide(), GetTall() );
		}
	}
}
