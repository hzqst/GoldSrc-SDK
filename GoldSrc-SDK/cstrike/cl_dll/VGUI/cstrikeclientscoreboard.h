//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "game_controls/ClientScoreBoardDialog.h"

struct cvar_s;
namespace vgui2
{
	class CBitmapImagePanel;
	class ImageList;
	class Label;
}

class CCSClientScoreBoardDialog : public CClientScoreBoardDialog
{
public:
	CCSClientScoreBoardDialog( vgui2::Panel* parent );
	~CCSClientScoreBoardDialog() override;

protected:
	void InitScoreboardSections() override;
	void DeathMsg( int killer, int victim ) override;
	void UpdateTeamInfo() override;
	bool GetPlayerScoreInfo( int playerIndex, KeyValues* kv ) override;
	void UpdatePlayerInfo() override;
	Color GetTeamColor( int teamNumber ) override;
	void PaintBackground() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void Update( const char* servername, bool teamplay, bool spectator ) override;
	virtual void UpdatePlayerAvatar( int playerIndex, KeyValues* kv );

private:
	void AddHeader() override; // add the start header of the scoreboard
	virtual void AddSection( int teamType, int teamNumber ); // add a new section header for a team
	bool ShowAvatars();
	void UpdateHeaders();
	void UpdateColumnInAllSections( const char* column, const wchar_t* header, int width, bool spectator );
	void UpdateOptionalColumns();
	const wchar_t* GetHealthHeader();
	const wchar_t* GetMoneyHeader();
	const wchar_t* GetScoreHeader();
	const wchar_t* GetDeathHeader();
	const wchar_t* GetPingHeader();
	int GetHeaderWidth( int maximum, int minimum );
	int GetHeaderWidthNoScale( int maximum, int minimum );
	int GetNameHeaderWidth();
	int GetAvatarHeightForScreenHeight( int height );
	int GetAvatarHeaderWidth();

	enum
	{
		CSTRIKE_AVATAR_WIDTH = 56,
		CSTRIKE_NAME_WIDTH = 209,
		CSTRIKE_SPACER1_WIDTH = 5,
		CSTRIKE_CLASS_WIDTH = 56,
		CSTRIKE_HEALTH_WIDTH = 41,
		CSTRIKE_ACCOUNT_WIDTH = 57,
		CSTRIKE_SCORE_WIDTH = 45,
		CSTRIKE_DEATH_WIDTH = 46,
		CSTRIKE_PING_WIDTH = 50,
		CSTRIKE_MIN_WIDTH = 38,
		CSTRIKE_MAX_WIDTH = 509
	};
	enum { CSTRIKE_AVATAR_HEIGHT = 32 };
	struct Avatars
	{
		int m_iAvatar;
		int m_iImageList;
	};
	enum
	{
		SCOREBOARD_SHOW_DISABLED,
		SCOREBOARD_SHOW_T_ONLY,
		SCOREBOARD_SHOW_CT_ONLY,
		SCOREBOARD_SHOW_TEAM_ONLY,
		SCOREBOARD_SHOW_ALL,
		SCOREBOARD_SHOW_MAX
	};

	vgui2::CBitmapImagePanel* m_pTopLeftPanel;
	vgui2::CBitmapImagePanel* m_pTopRightPanel;
	vgui2::CBitmapImagePanel* m_pBottomLeftPanel;
	vgui2::CBitmapImagePanel* m_pBottomRightPanel;
	vgui2::CBitmapImagePanel* m_pTopLeftBorderPanel;
	vgui2::CBitmapImagePanel* m_pTopRightBorderPanel;
	vgui2::CBitmapImagePanel* m_pBottomLeftBorderPanel;
	vgui2::CBitmapImagePanel* m_pBottomRightBorderPanel;
	Color m_careerBgColor;
	Color m_borderColor;
	vgui2::Label* m_pTaskLabel;
	vgui2::Panel* m_pIGA;
	vgui2::ImageList* m_pImageList;
	Avatars m_iImageAvatars[65];
	int m_iPlayersToShow;
	cvar_s* m_pShowHealth;
	bool m_bHealthColumnEnabled;
	bool m_bHealthUpdateReceived;
	cvar_s* m_pShowMoney;
	bool m_bMoneyColumnEnabled;
	bool m_bMoneyUpdateReceived;
	cvar_s* m_pShortHeaders;
	bool m_bShortHeaders;
	cvar_s* m_pShowAvatars;
	bool m_bAvatarColumnEnabled;
	int m_nAvatarColumnWidth;
};
