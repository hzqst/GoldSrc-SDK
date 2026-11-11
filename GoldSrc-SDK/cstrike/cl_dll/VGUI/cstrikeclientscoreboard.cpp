//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "cstrikeclientscoreboard.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "game_controls/vgui_avatarimage.h"
#include "vgui_controls/Controls.h"
#include "BitmapImagePanel.h"
#include "ImageList.h"
#include "Label.h"
#include "SectionedListPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
#include "r_studioint.h"
#include "cvardef.h"
#include "shared_util.h"
#include "isteamfriends.h"
#include "isteamutils.h"
#include "hud_iga.h"
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <math.h>

extern engine_studio_api_t IEngineStudio;

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
CCSClientScoreBoardDialog::CCSClientScoreBoardDialog( vgui2::Panel* parent ) : CClientScoreBoardDialog( parent )
{
	m_careerBgColor = Color( 0, 0, 0, 0 );
	m_borderColor = Color( 0, 0, 0, 0 );
	m_iNumTeams = 2;
	m_pShowHealth = NULL;
	m_pShowMoney = NULL;
	m_pShortHeaders = NULL;
	m_pShowAvatars = NULL;
	m_pTopLeftPanel = new vgui2::CBitmapImagePanel( this, "TopLeftPanel", "gfx/vgui/round_corner_nw" );
	m_pTopRightPanel = new vgui2::CBitmapImagePanel( this, "TopRightPanel", "gfx/vgui/round_corner_ne" );
	m_pBottomLeftPanel = new vgui2::CBitmapImagePanel( this, "BottomLeftPanel", "gfx/vgui/round_corner_sw" );
	m_pBottomRightPanel = new vgui2::CBitmapImagePanel( this, "BottomRightPanel", "gfx/vgui/round_corner_se" );
	m_pTopLeftBorderPanel = new vgui2::CBitmapImagePanel( this, "TopLeftBorderPanel", "gfx/vgui/round_corner_small_nw" );
	m_pTopRightBorderPanel = new vgui2::CBitmapImagePanel( this, "TopRightBorderPanel", "gfx/vgui/round_corner_small_ne" );
	m_pBottomLeftBorderPanel = new vgui2::CBitmapImagePanel( this, "BottomLeftBorderPanel", "gfx/vgui/round_corner_small_sw" );
	m_pBottomRightBorderPanel = new vgui2::CBitmapImagePanel( this, "BottomRightBorderPanel", "gfx/vgui/round_corner_small_se" );
	m_pImageList = NULL;
	memset( m_iImageAvatars, 0, sizeof( m_iImageAvatars ) );
	m_pTaskLabel = new vgui2::Label( this, "TaskLabel", "" );
	m_pTaskLabel->SetVisible( false );
	m_pIGA = NULL;
	m_pIGA = g_pClientIGA->CreatePanel( IGA_SCOREBOARD, this );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCSClientScoreBoardDialog::~CCSClientScoreBoardDialog()
{
	if ( m_pImageList )
	{
		delete m_pImageList;
		m_pImageList = NULL;
	}
}

void CCSClientScoreBoardDialog::DeathMsg( int, int victim )
{
	if ( victim )
	{
		g_PlayerExtraInfo[victim].dead = true;
		g_PlayerExtraInfo[victim].has_c4 = false;
		g_PlayerExtraInfo[victim].has_defuse_kit = false;
	}
}

Color CCSClientScoreBoardDialog::GetTeamColor( int teamNumber )
{
	return CClientScoreBoardDialog::GetTeamColor( teamNumber );
}

bool CCSClientScoreBoardDialog::ShowAvatars()
{
	return m_bAvatarColumnEnabled;
}

const wchar_t* CCSClientScoreBoardDialog::GetHealthHeader()
{
	return m_bShortHeaders ? L"HP" : vgui2::localize()->Find( "#CStrike_HEALTH" );
}

const wchar_t* CCSClientScoreBoardDialog::GetMoneyHeader()
{
	return m_bShortHeaders ? L"$" : vgui2::localize()->Find( "#CStrike_ACCOUNT" );
}

const wchar_t* CCSClientScoreBoardDialog::GetScoreHeader()
{
	return m_bShortHeaders ? L"S" : vgui2::localize()->Find( "#PlayerScore" );
}

const wchar_t* CCSClientScoreBoardDialog::GetDeathHeader()
{
	return m_bShortHeaders ? L"D" : vgui2::localize()->Find( "#PlayerDeath" );
}

const wchar_t* CCSClientScoreBoardDialog::GetPingHeader()
{
	return m_bShortHeaders ? L"Ping" : vgui2::localize()->Find( "#PlayerPing" );
}

int CCSClientScoreBoardDialog::GetHeaderWidthNoScale( int maximum, int minimum )
{
	if ( m_bShortHeaders && maximum > CSTRIKE_MIN_WIDTH )
		maximum = CSTRIKE_MIN_WIDTH;
	return maximum < minimum ? minimum : maximum;
}

int CCSClientScoreBoardDialog::GetHeaderWidth( int maximum, int minimum )
{
	return vgui2::scheme()->GetProportionalScaledValue( GetHeaderWidthNoScale( maximum, minimum ) );
}

int CCSClientScoreBoardDialog::GetNameHeaderWidth()
{
	int width = m_bShortHeaders ? 239 : CSTRIKE_NAME_WIDTH;
	if ( !m_bHealthColumnEnabled )
		width = m_bShortHeaders ? 274 : 250;
	if ( !m_bMoneyColumnEnabled )
		width += CSTRIKE_ACCOUNT_WIDTH;
	return vgui2::scheme()->GetProportionalScaledValue( width ) - m_nAvatarColumnWidth;
}

int CCSClientScoreBoardDialog::GetAvatarHeightForScreenHeight( int height )
{
	if ( height < 0 )
	{
		int width;
		vgui2::surface()->GetScreenSize( width, height );
	}

	int avatarHeight = CSTRIKE_AVATAR_HEIGHT;
	if ( height < 600 )
		avatarHeight = 21;
	else if ( height < 768 )
		avatarHeight = 23;

	if ( m_iPlayersToShow > 23 )
		return 6 * avatarHeight / 11;
	if ( m_iPlayersToShow > 17 )
		return 5 * avatarHeight / 7;
	return avatarHeight;
}

int CCSClientScoreBoardDialog::GetAvatarHeaderWidth()
{
	if ( !m_bAvatarColumnEnabled )
		return 0;
	return CSTRIKE_AVATAR_WIDTH * GetAvatarHeightForScreenHeight( -1 ) / CSTRIKE_AVATAR_HEIGHT;
}

void CCSClientScoreBoardDialog::InitScoreboardSections()
{
	for ( int i = 1; i < 64; ++i )
	{
		g_PlayerExtraInfo[i].sb_health = -1;
		g_PlayerExtraInfo[i].sb_account = -1;
	}
	m_iPlayersToShow = 0;
	if ( !m_pShowHealth )
	{
		m_pShowHealth = gEngfuncs.pfnGetCvarPointer( "scoreboard_showhealth" );
		if ( !m_pShowHealth )
			m_pShowHealth = gEngfuncs.pfnRegisterVariable( "scoreboard_showhealth", "1", FCVAR_ARCHIVE );
	}
	if ( !m_pShowMoney )
	{
		m_pShowMoney = gEngfuncs.pfnGetCvarPointer( "scoreboard_showmoney" );
		if ( !m_pShowMoney )
			m_pShowMoney = gEngfuncs.pfnRegisterVariable( "scoreboard_showmoney", "1", FCVAR_ARCHIVE );
	}
	if ( !m_pShortHeaders )
	{
		m_pShortHeaders = gEngfuncs.pfnGetCvarPointer( "scoreboard_shortheaders" );
		if ( !m_pShortHeaders )
			m_pShortHeaders = gEngfuncs.pfnRegisterVariable( "scoreboard_shortheaders", "0", FCVAR_ARCHIVE );
	}
	if ( !m_pShowAvatars )
	{
		m_pShowAvatars = gEngfuncs.pfnGetCvarPointer( "scoreboard_showavatars" );
		if ( !m_pShowAvatars )
			m_pShowAvatars = gEngfuncs.pfnRegisterVariable( "scoreboard_showavatars", "1", FCVAR_ARCHIVE );
	}
	m_bHealthColumnEnabled = false;
	m_bHealthUpdateReceived = false;
	m_bMoneyColumnEnabled = false;
	m_bMoneyUpdateReceived = false;
	m_bShortHeaders = (int)m_pShortHeaders->value != 0;
	m_bAvatarColumnEnabled = (int)m_pShowAvatars->value != 0;
	m_nAvatarColumnWidth = GetAvatarHeaderWidth();
	// fill out the structure of the scoreboard
	AddHeader();
	AddSection( 1, 1 );
	AddSection( 1, 2 );
	AddSection( 2, 3 );
	AddSection( 3, 4 );
	AddSection( 3, 5 );
}

void CCSClientScoreBoardDialog::AddHeader()
{
	m_pPlayerList->AddSection( 0, "", NULL );
	m_pPlayerList->SetSectionAlwaysVisible( 0, true );
	vgui2::HFont font = vgui2::scheme()->GetIScheme( GetScheme() )->GetFont( "DefaultVerySmallFallBack", false );
	m_pPlayerList->AddColumnToSection( 0, "avatar", "", 0, m_nAvatarColumnWidth );
	m_pPlayerList->AddColumnToSection( 0, "name", "", 0, GetNameHeaderWidth(), font );
	m_pPlayerList->AddColumnToSection( 0, "spacer1", "", 0, CSTRIKE_SPACER1_WIDTH );
	m_pPlayerList->AddColumnToSection( 0, "class", "", 0, vgui2::scheme()->GetProportionalScaledValue( CSTRIKE_CLASS_WIDTH ) );
	m_pPlayerList->AddColumnToSection( 0, "health", "", vgui2::SectionedListPanel::COLUMN_RIGHT, 0 );
	m_pPlayerList->AddColumnToSection( 0, "account", "", vgui2::SectionedListPanel::COLUMN_RIGHT, 0 );
	m_pPlayerList->AddColumnToSection( 0, "frags", GetScoreHeader(), vgui2::SectionedListPanel::COLUMN_RIGHT, GetHeaderWidth( CSTRIKE_SCORE_WIDTH, 0 ) );
	m_pPlayerList->AddColumnToSection( 0, "deaths", GetDeathHeader(), vgui2::SectionedListPanel::COLUMN_RIGHT, GetHeaderWidth( CSTRIKE_DEATH_WIDTH, 0 ) );
	m_pPlayerList->AddColumnToSection( 0, "ping", GetPingHeader(), vgui2::SectionedListPanel::COLUMN_RIGHT, GetHeaderWidth( CSTRIKE_PING_WIDTH, 0 ) );
//	m_pPlayerList->AddColumnToSection(0, "voice", "#PlayerVoice", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::HEADER_TEXT| SectionedListPanel::COLUMN_CENTER, scheme()->GetProportionalScaledValue( CSTRIKE_VOICE_WIDTH ) );
//	m_pPlayerList->AddColumnToSection(0, "tracker", "#PlayerTracker", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::HEADER_TEXT, scheme()->GetProportionalScaledValue( CSTRIKE_FRIENDS_WIDTH ) );
}

void CCSClientScoreBoardDialog::AddSection( int teamType, int teamNumber )
{
	vgui2::HFont font = vgui2::scheme()->GetIScheme( GetScheme() )->GetFont( "DefaultVerySmallFallBack", false );
	int nameWidth = GetNameHeaderWidth();
	switch ( teamType )
	{
	case 1:
		{
			int section = teamNumber == 1 ? 1 : teamNumber == 2 ? 2 : 0;
			m_pPlayerList->AddSection( section, "", StaticPlayerSortFunc );
			m_pPlayerList->AddColumnToSection( section, "avatar", "", vgui2::SectionedListPanel::COLUMN_IMAGE, m_nAvatarColumnWidth );
			m_pPlayerList->AddColumnToSection( section, "name", "", 0, nameWidth, font );
			m_pPlayerList->AddColumnToSection( section, "spacer1", "", 0, CSTRIKE_SPACER1_WIDTH );
			m_pPlayerList->AddColumnToSection( section, "class", "", 0, vgui2::scheme()->GetProportionalScaledValue( CSTRIKE_CLASS_WIDTH ) );
			m_pPlayerList->AddColumnToSection( section, "health", "", vgui2::SectionedListPanel::COLUMN_RIGHT, 0 );
			m_pPlayerList->AddColumnToSection( section, "account", "", vgui2::SectionedListPanel::COLUMN_RIGHT, 0 );
			m_pPlayerList->AddColumnToSection( section, "frags", "", vgui2::SectionedListPanel::COLUMN_RIGHT, GetHeaderWidth( CSTRIKE_SCORE_WIDTH, 0 ) );
			m_pPlayerList->AddColumnToSection( section, "deaths", "", vgui2::SectionedListPanel::COLUMN_RIGHT, GetHeaderWidth( CSTRIKE_DEATH_WIDTH, 0 ) );
			m_pPlayerList->AddColumnToSection( section, "ping", "", vgui2::SectionedListPanel::COLUMN_RIGHT, GetHeaderWidth( CSTRIKE_PING_WIDTH, 0 ) );

			// set the section to have the team color
			if ( teamNumber )
				m_pPlayerList->SetSectionFgColor( section, GetTeamColor( teamNumber ) );
		}
		break;
	case 2:
		m_pPlayerList->AddSection( 3, "", NULL );
		m_pPlayerList->AddColumnToSection( 3, "avatar", "", vgui2::SectionedListPanel::COLUMN_IMAGE, m_nAvatarColumnWidth );
		m_pPlayerList->AddColumnToSection( 3, "name", "#Spectators", 0, nameWidth, font );
		break;
	case 3:
		if ( teamNumber == 4 )
		{
			m_pPlayerList->AddSection( 4, "", NULL );
			m_pPlayerList->AddColumnToSection( 4, "score", "", 0, vgui2::scheme()->GetProportionalScaledValue( 400 ) );
		}
		else
		{
			m_pPlayerList->AddSection( 5, "", NULL );
			m_pPlayerList->AddColumnToSection( 5, "name", "#Career_TasksColon", 0, vgui2::scheme()->GetProportionalScaledValue( 336 ) );
			m_pPlayerList->AddColumnToSection( 5, "status", "#Career_Task_PartialTitle", 0, vgui2::scheme()->GetProportionalScaledValue( 72 ) );
		}
		break;
	}
}

void CCSClientScoreBoardDialog::UpdateColumnInAllSections( const char* column, const wchar_t* header, int width, bool spectator )
{
	m_pPlayerList->ModifyColumn( 0, column, header, width );
	m_pPlayerList->ModifyColumn( 1, column, NULL, width );
	m_pPlayerList->ModifyColumn( 2, column, NULL, width );
	if ( spectator )
		m_pPlayerList->ModifyColumn( 3, column, NULL, width );
}

void CCSClientScoreBoardDialog::UpdateHeaders()
{
	bool shortHeaders = (int)m_pShortHeaders->value != 0;
	if ( shortHeaders == m_bShortHeaders )
		return;
	m_bShortHeaders = shortHeaders;
	UpdateColumnInAllSections( "frags", GetScoreHeader(), GetHeaderWidth( CSTRIKE_SCORE_WIDTH, 0 ), false );
	UpdateColumnInAllSections( "deaths", GetDeathHeader(), GetHeaderWidth( CSTRIKE_DEATH_WIDTH, 0 ), false );
	UpdateColumnInAllSections( "ping", GetPingHeader(), GetHeaderWidth( CSTRIKE_PING_WIDTH, 0 ), false );
	if ( m_bHealthColumnEnabled )
		UpdateColumnInAllSections( "health", GetHealthHeader(), GetHeaderWidth( CSTRIKE_HEALTH_WIDTH, 0 ), false );
	if ( m_bMoneyColumnEnabled )
		UpdateColumnInAllSections( "account", GetMoneyHeader(), GetHeaderWidth( CSTRIKE_ACCOUNT_WIDTH, CSTRIKE_ACCOUNT_WIDTH ), false );
	UpdateColumnInAllSections( "name", L"", GetNameHeaderWidth(), false );
}

void CCSClientScoreBoardDialog::UpdateOptionalColumns()
{
	bool resizeName = false;

	bool avatars = (int)m_pShowAvatars->value != 0;
	if ( m_bAvatarColumnEnabled != avatars )
	{
		m_bAvatarColumnEnabled = avatars;
		resizeName = true;
	}

	int avatarWidth = GetAvatarHeaderWidth();
	if ( avatarWidth != m_nAvatarColumnWidth )
	{
		UpdateColumnInAllSections( "avatar", NULL, avatarWidth, true );
		m_nAvatarColumnWidth = avatarWidth;
		resizeName = true;
	}

	bool health = m_bHealthUpdateReceived && (int)m_pShowHealth->value != 0;
	if ( m_bHealthColumnEnabled != health )
	{
		const wchar_t* header = L"";
		int width = 0;
		if ( health )
		{
			header = GetHealthHeader();
			width = GetHeaderWidth( CSTRIKE_HEALTH_WIDTH, 0 );
		}
		UpdateColumnInAllSections( "health", header, width, false );
		m_bHealthColumnEnabled = health;
		resizeName = true;
	}

	bool money = m_bMoneyUpdateReceived && (int)m_pShowMoney->value != 0;
	if ( m_bMoneyColumnEnabled != money )
	{
		const wchar_t* header = L"";
		int width = 0;
		if ( money )
		{
			header = GetMoneyHeader();
			width = GetHeaderWidth( CSTRIKE_ACCOUNT_WIDTH, CSTRIKE_ACCOUNT_WIDTH );
		}
		UpdateColumnInAllSections( "account", header, width, false );
		m_bMoneyColumnEnabled = money;
		resizeName = true;
	}

	if ( resizeName )
		UpdateColumnInAllSections( "name", NULL, GetNameHeaderWidth(), false );
}

void CCSClientScoreBoardDialog::UpdateTeamInfo()
{
	// clear out team scores
	for ( int i = 1; i <= m_iNumTeams; ++i )
	{
		ScoreBoardTeamInfo& team = m_TeamInfo[i];
		if ( !team.scores_overriden )
		{
			team.deaths = 0;
			team.frags = 0;
		}
		team.packetloss = 0;
		team.ping = 0;
		team.players = 0;
	}

	// recalc the team scores, then draw them
	for ( int i = 1; i < 64; ++i )
	{
		VGuiLibraryPlayer_t player = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( i );
		if ( player.name && ( player.teamnumber == 1 || player.teamnumber == 2 ) )
		{
			ScoreBoardTeamInfo& team = m_TeamInfo[player.teamnumber];
			team.ping += player.ping;
			team.packetloss += player.packetloss;
			++team.players;
			team.teamnumber = player.teamnumber;
			team.ownteam = player.thisplayer;
		}
	}

	// find team ping/packetloss averages
	for ( int i = 1; i <= m_iNumTeams; ++i )
	{
		ScoreBoardTeamInfo& team = m_TeamInfo[i];
		team.already_drawn = false;
		if ( team.players > 0 )
		{
			team.ping /= team.players;
			team.packetloss /= team.players;
		}
	}

	// update the team sections in the scoreboard
	for ( int i = 1; i <= m_iNumTeams; ++i )
	{
		ScoreBoardTeamInfo& team = m_TeamInfo[i];
		const wchar_t* teamName;
		int section;
		if ( team.name && !stricmp( team.name, "CT" ) )
		{
			teamName = vgui2::localize()->Find( "#Cstrike_ScoreBoard_CT" );
			section = 2;
		}
		else if ( team.name && !stricmp( team.name, "TERRORIST" ) )
		{
			teamName = vgui2::localize()->Find( "#Cstrike_ScoreBoard_Ter" );
			section = 1;
		}
		else
		{
			teamName = vgui2::localize()->Find( "#Spectators" );
			section = 3;
		}
		wchar_t count[6], name[64], text[1024], value[6];
		_snwprintf( count, 6, L"%i", team.players );
		if ( !teamName && team.name )
		{
			vgui2::localize()->ConvertANSIToUnicode( team.name, name, sizeof( name ) );
			teamName = name;
		}
		vgui2::localize()->ConstructString( text, sizeof( text ), vgui2::localize()->Find(
			team.players == 1 ? "#Cstrike_ScoreBoard_Player" : "#Cstrike_ScoreBoard_Players" ), 2, teamName, count );
		m_pPlayerList->ModifyColumn( section, "name", text, -1 );
		_snwprintf( value, 6, L"%d", team.frags );
		value[5] = 0;
		m_pPlayerList->ModifyColumn( section, "frags", value, -1 );
		if ( team.ping > 0 )
		{
			_snwprintf( value, 6, L"%d", team.ping );
			value[5] = 0;
			m_pPlayerList->ModifyColumn( section, "ping", value, -1 );
		}
		else
		{
			m_pPlayerList->ModifyColumn( section, "ping", L"", -1 );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new row to the scoreboard, from the playerinfo structure
//-----------------------------------------------------------------------------
bool CCSClientScoreBoardDialog::GetPlayerScoreInfo( int playerIndex, KeyValues* kv )
{
	extra_player_info_t& player = g_PlayerExtraInfo[playerIndex];
	if ( player.teamnumber )
	{
		kv->SetInt( "deaths", player.deaths );
		kv->SetInt( "frags", player.frags );
		if ( g_PlayerInfoList[playerIndex].ping > 0 )
		{
			kv->SetInt( "ping", g_PlayerInfoList[playerIndex].ping );
		}
		else
		{
			const char* bot = gEngfuncs.PlayerInfo_ValueForKey( playerIndex, "*bot" );
			if ( bot && atoi( bot ) > 0 )
				kv->SetString( "ping", "BOT" );
			else
				kv->SetString( "ping", "" );
		}
		char buffer[128];
		if ( player.sb_health >= 0 && !player.dead )
		{
			if ( (int)m_pShowHealth->value )
			{
				_snprintf( buffer, sizeof( buffer ), "%d", player.sb_health );
				kv->SetString( "health", buffer );
			}
			m_bHealthUpdateReceived = true;
		}
		if ( player.sb_account >= 0 )
		{
			if ( (int)m_pShowMoney->value )
			{
				_snprintf( buffer, sizeof( buffer ), "$%d", player.sb_account );
				kv->SetString( "account", buffer );
			}
			m_bMoneyUpdateReceived = true;
		}
	}
	kv->SetString( "name", g_PlayerInfoList[playerIndex].name );
	if ( player.dead == true )
		kv->SetString( "class", "#Cstrike_DEAD" );
	else if ( player.has_c4 && ( g_iTeamNumber == 1 || g_iTeamNumber == 0 || g_PlayerExtraInfo[gEngfuncs.GetLocalPlayer()->index].dead == true ) )
		kv->SetString( "class", "#Cstrike_BOMB" );
	else if ( player.vip && ( g_iTeamNumber == 2 || g_iTeamNumber == 0 || g_PlayerExtraInfo[gEngfuncs.GetLocalPlayer()->index].dead == true ) )
		kv->SetString( "class", "#Cstrike_VIP" );
	else if ( player.has_defuse_kit && ( g_iTeamNumber == 2 || g_iTeamNumber == 0 || g_PlayerExtraInfo[gEngfuncs.GetLocalPlayer()->index].dead == true ) )
		kv->SetString( "class", "#Cstrike_DEFUSE_KIT" );
	else
		kv->SetString( "class", "" );
	kv->SetInt( "playerIndex", playerIndex );
	return true;
}

void CCSClientScoreBoardDialog::UpdatePlayerAvatar( int playerIndex, KeyValues* kv )
{
	if ( !m_bAvatarColumnEnabled || !kv || !SteamFriends() || !SteamUtils() || !m_pImageList )
		return;
	VGuiLibraryPlayer_t player = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( playerIndex );
	CSteamID steamID( player.m_nSteamID );
	if ( !player.m_nSteamID )
		return;
	int avatar = SteamFriends()->GetSmallFriendAvatar( steamID );
	Avatars& cached = m_iImageAvatars[playerIndex];
	if ( !m_pImageList->IsValidIndex( cached.m_iImageList ) || cached.m_iImageList == 0 || cached.m_iAvatar != avatar )
	{
		int height = GetAvatarHeightForScreenHeight( -1 );
		cached.m_iAvatar = avatar;
		CAvatarImage* image = new CAvatarImage( vgui2::scheme()->GetImage( "resource/icon_blocked", true ),
			vgui2::scheme()->GetImage( "resource/icon_friend_indicator_scoreboard", true ) );
		image->SetAvatarSize( height, height );
		image->SetAvatarSteamID( steamID );
		cached.m_iImageList = m_pImageList->AddImage( image );
	}
	if ( cached.m_iImageList > 0 && m_pImageList->IsValidIndex( cached.m_iImageList ) )
	{
		kv->SetInt( "avatar", cached.m_iImageList );
		CAvatarImage* image = static_cast<CAvatarImage*>( m_pImageList->GetImage( cached.m_iImageList ) );
		if ( image )
		{
			int height = GetAvatarHeightForScreenHeight( -1 );
			image->SetAvatarSize( height, height );
			image->UpdateFriendStatus();
		}
	}
}

void CCSClientScoreBoardDialog::Update( const char* servername, bool teamplay, bool spectator )
{
	int cornerSize = 10;
	if ( IEngineStudio.IsHardware() == 1 )
	{
		char adjustedServerName[33];
		_snprintf( adjustedServerName, sizeof( adjustedServerName ), " %s", servername );
		adjustedServerName[sizeof( adjustedServerName ) - 1] = 0;
		CClientScoreBoardDialog::Update( adjustedServerName, teamplay, spectator );
	}
	else
	{
		cornerSize = 0;
		CClientScoreBoardDialog::Update( servername, teamplay, spectator );
	}
	bool hardware = IEngineStudio.IsHardware && IEngineStudio.IsHardware() != 0;
	m_pTopLeftPanel->SetVisible( hardware );
	m_pTopRightPanel->SetVisible( hardware );
	m_pBottomLeftPanel->SetVisible( hardware );
	m_pBottomRightPanel->SetVisible( hardware );
	int wide = GetWide();
	int tall = GetTall();
	m_pTopLeftPanel->SetBounds( 0, 0, cornerSize, cornerSize );
	int right = wide - cornerSize - 1;
	int bottom = tall - cornerSize - 1;
	m_pTopRightPanel->SetBounds( right, 0, cornerSize, cornerSize );
	m_pBottomLeftPanel->SetBounds( 0, bottom, cornerSize, cornerSize );
	m_pBottomRightPanel->SetBounds( right, bottom, cornerSize, cornerSize );
	m_pTopLeftBorderPanel->SetBounds( 0, 0, cornerSize, cornerSize );
	m_pTopRightBorderPanel->SetBounds( right, 0, cornerSize, cornerSize );
	m_pBottomLeftBorderPanel->SetBounds( 0, bottom, cornerSize, cornerSize );
	m_pBottomRightBorderPanel->SetBounds( right, bottom, cornerSize, cornerSize );
	m_pTopLeftBorderPanel->MoveToFront();
	m_pTopRightBorderPanel->MoveToFront();
	m_pBottomLeftBorderPanel->MoveToFront();
	m_pBottomRightBorderPanel->MoveToFront();

	if ( !m_pIGA )
		return;

	bool shown = true;
	if ( g_pClientIGA->IsPanelReady( IGA_SCOREBOARD, &shown ) && !gViewPortInterface->IsSpectatorGUIVisible() )
	{
		if ( shown )
		{
			int screenWide, screenTall;
			vgui2::surface()->GetScreenSize( screenWide, screenTall );

			int igaTall = (int)( screenTall * 0.125f );
			int minTall = (int)( screenTall / 10.0f );
			int maxTall = screenTall - tall - 10;
			if ( maxTall < minTall )
				maxTall = minTall;

			int igaWide;
			while ( true )
			{
				igaWide = igaTall * 4;
				if ( igaWide <= screenWide && igaTall <= maxTall )
					break;

				igaTall = (int)( igaTall * 0.9f );
				if ( igaTall <= minTall )
				{
					igaTall = minTall;
					igaWide = minTall * 4;
					break;
				}
			}

			int x, y;
			GetPos( x, y );

			int totalTall = tall + 10 + igaTall;
			if ( totalTall > screenTall )
				tall += screenTall - totalTall;

			int gap = ( screenTall - igaTall - tall - 10 ) / 4;
			SetBounds( x, gap + 5 + igaTall, wide, tall );
			m_pIGA->SetBounds( x + ( wide - igaWide ) / 2, gap, igaWide, igaTall );
		}
		else
		{
			m_pIGA->SetBounds( 0, 0, 1, 1 );
		}
		m_pIGA->SetVisible( true );
	}
	else if ( m_pIGA )
	{
		m_pIGA->SetVisible( false );
	}
}

void CCSClientScoreBoardDialog::PaintBackground()
{
	int x1, y1, x2, y2;
	vgui2::surface()->DrawSetColor( m_careerBgColor );
	if ( IEngineStudio.IsHardware() == 1 )
	{
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
		vgui2::surface()->DrawSetColor( m_borderColor );
		vgui2::surface()->DrawSetTextColor( m_borderColor );
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pBottomLeftPanel->GetPos( x2, y2 );
		y1 += m_pTopLeftPanel->GetTall();
		vgui2::surface()->DrawFilledRect( x1, y1, x2 + 1, y2 + 1 );
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pTopRightPanel->GetPos( x2, y2 );
		x1 += m_pTopLeftPanel->GetWide();
		vgui2::surface()->DrawFilledRect( x1, y1, x2 + 1, y2 + 1 );
		m_pBottomLeftPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		x1 += m_pBottomLeftPanel->GetWide();
		y1 += m_pBottomLeftPanel->GetTall();
		y2 += m_pBottomRightPanel->GetTall();
		vgui2::surface()->DrawFilledRect( x1, y1, x2 + 1, y2 + 1 );
		m_pTopRightPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		y1 += m_pTopRightPanel->GetTall();
		x1 += m_pTopRightPanel->GetWide();
		x2 += m_pBottomRightPanel->GetWide();
		vgui2::surface()->DrawFilledRect( x1, y1, x2 + 1, y2 + 1 );
	}
	else
	{
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		x2 += m_pBottomRightPanel->GetWide();
		y2 += m_pBottomRightPanel->GetTall();
		vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
		vgui2::surface()->DrawSetColor( m_borderColor );
		vgui2::surface()->DrawOutlinedRect( x1, y1, x2, y2 );
	}
}

void CCSClientScoreBoardDialog::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	CClientScoreBoardDialog::ApplySchemeSettings( scheme );
	m_careerBgColor = GetSchemeColor( "BuddyListBgColor", GetBgColor(), scheme );
	m_borderColor = scheme->GetColor( "BorderBright", Color( 0, 0, 0, 0 ) );
	m_pPlayerList->SetBgColor( Color( 0, 0, 0, 0 ) );
	m_pPlayerList->SetBorder( NULL );
	Color cornerColor( 255, 255, 255, m_careerBgColor[3] );
	m_pTopLeftPanel->m_bgColor = cornerColor;
	m_pTopRightPanel->m_bgColor = cornerColor;
	m_pBottomLeftPanel->m_bgColor = cornerColor;
	m_pBottomRightPanel->m_bgColor = cornerColor;
	m_pTopLeftBorderPanel->m_bgColor = m_borderColor;
	m_pTopRightBorderPanel->m_bgColor = m_borderColor;
	m_pBottomLeftBorderPanel->m_bgColor = m_borderColor;
	m_pBottomRightBorderPanel->m_bgColor = m_borderColor;
	if ( m_pImageList )
		delete m_pImageList;
	m_pImageList = new vgui2::ImageList( false );
	memset( m_iImageAvatars, 0, sizeof( m_iImageAvatars ) );
	m_pPlayerList->SetImageList( m_pImageList, false );
}

void CCSClientScoreBoardDialog::UpdatePlayerInfo()
{
	m_iSectionId = 0; // 0'th row is a header
	gEngfuncs.pfnGetCvarFloat( "developer" );
	m_pPlayerList->SetSectionFgColor( 1, GetTeamColor( 1 ) );
	m_pPlayerList->SetSectionFgColor( 2, GetTeamColor( 2 ) );
	int playersToShow = 0;
	for ( int i = 1; i < 64; ++i )
	{
		VGuiLibraryPlayer_t player = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( i );
		if ( player.name && i < gViewPortInterface->GetClientDllInterface()->GetMaxPlayers() )
			++playersToShow;
	}
	m_iPlayersToShow = playersToShow;
	m_pPlayerList->OverrideLineSpacing( playersToShow > 23 ? 11 : playersToShow <= 17 ? 21 : 15 );
	UpdateOptionalColumns();
	UpdateHeaders();
	int selectedItem = -1;
	for ( int i = 1; i < 64; ++i )
	{
		VGuiLibraryPlayer_t player = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( i );
		if ( player.name && gViewPortInterface->GetClientDllInterface()->GetMaxPlayers() > i )
		{
			KeyValues* data = new KeyValues( "data" );
			GetPlayerScoreInfo( i, data );
			UpdatePlayerAvatar( i, data );
			const char* name = data->GetString( "name", "" );
			int capacity = 2 * (int)strlen( name );
			char* safeName = (char*)malloc( capacity );
			gViewPortInterface->MakeSafeName( name, safeName, capacity );
			data->SetString( "name", safeName );
			free( safeName );
			int item = FindItemIDForPlayerIndex( i );
			int section = player.teamnumber ? player.teamnumber : 3;
			if ( player.thisplayer )
				selectedItem = item;
			if ( item == -1 )
				item = m_pPlayerList->AddItem( section, data );
			else
				m_pPlayerList->ModifyItem( item, section, data );
			m_pPlayerList->SetItemFgColor( item, GetTeamColor( player.teamnumber ) );
			data->deleteThis();
		}
		else
		{
			int item = FindItemIDForPlayerIndex( i );
			if ( item != -1 )
				m_pPlayerList->RemoveItem( item );
		}
	}
	if ( selectedItem != -1 )
		m_pPlayerList->SetSelectedItem( selectedItem );
	if ( !g_pCareerUI || !g_pCareerUI->IsPlayingMatch() )
	{
		m_pPlayerList->SetSectionAlwaysVisible( 4, false );
		m_pPlayerList->SetSectionAlwaysVisible( 5, false );
		return;
	}

	wchar_t value[6], ctText[512], terroristText[512], buffer[512];
	_snwprintf( value, 6, L"%d", m_TeamInfo[2].frags );
	value[5] = 0;
	vgui2::localize()->ConstructString( ctText, sizeof( ctText ),
		vgui2::localize()->Find( "#Career_RoundWinsCT" ), 1, value );
	_snwprintf( value, 6, L"%d", m_TeamInfo[1].frags );
	value[5] = 0;
	vgui2::localize()->ConstructString( terroristText, sizeof( terroristText ),
		vgui2::localize()->Find( "#Career_RoundWinsT" ), 1, value );
	int length = 512;
	BufWPrintf( buffer, length, L"%ls   %ls", ctText, terroristText );
	m_pPlayerList->ModifyColumn( 4, "score", buffer, -1 );
	m_pPlayerList->SetSectionAlwaysVisible( 4, true );
	m_pPlayerList->SetSectionAlwaysVisible( 5, true );
	ITaskVec* tasks = g_pCareerUI->GetCurrentTaskVec();
	int playerIndex = gViewPortInterface->GetClientDllInterface()->GetMaxPlayers();
	for ( ITaskVecIt it = tasks->begin(); it != tasks->end(); ++it )
	{
		ICareerTask* task = *it;
		const wchar_t* taskName = task->GetLocalizedTaskName();
		wchar_t taskLines[2][512] = { L"", L"      " };
		bool twoLines = false;
		if ( !taskName )
			continue;

		++playerIndex;
		m_pTaskLabel->SetText( taskName );
		m_pTaskLabel->SizeToContents();

		const wchar_t* split = taskName + wcslen( taskName );
		int labelWidth, labelHeight;
		m_pTaskLabel->GetSize( labelWidth, labelHeight );
		if ( labelWidth > vgui2::scheme()->GetProportionalScaledValue( 331 ) )
		{
			twoLines = true;
			do
			{
				while ( split && !iswspace( *split ) && split > taskName )
					--split;

				--split;
				int firstLength = (int)( split - taskName ) + 1;
				wcsncpy( taskLines[0], taskName, firstLength );
				taskLines[0][firstLength] = 0;
				wcscpy( taskLines[1] + 6, split + 2 );

				m_pTaskLabel->SetText( taskLines[0] );
				m_pTaskLabel->SizeToContents();
				m_pTaskLabel->GetSize( labelWidth, labelHeight );
			}
			while ( labelWidth > vgui2::scheme()->GetProportionalScaledValue( 331 ) );
		}

		wchar_t status[512] = {};
		if ( task->IsComplete() )
		{
			vgui2::localize()->ConstructString( status, sizeof( status ),
				vgui2::localize()->Find( "#Career_Task_Complete" ), 0 );
		}
		else
		{
			int remaining = task->GetRepeat() - task->GetPartial();
			if ( remaining > 0 && task->GetRepeat() > 1 )
			{
				char ascii[512];
				if ( !task->IsMaxRoundTime() )
				{
					sprintf( ascii, "%d", remaining );
					vgui2::localize()->ConvertANSIToUnicode( ascii, buffer, sizeof( buffer ) );
					vgui2::localize()->ConstructString( status, sizeof( status ),
						vgui2::localize()->Find( "#Career_Task_PartialRemaining" ), 1, buffer );
				}
				else
				{
					float elapsed = g_pCareerUI->GetRoundTimeLength() - ( gHUD.m_roundTimer.m_flTimeEnd - gHUD.m_flTime );
					bool spectator = gViewPortInterface->IsSpectatorGUIVisible();
					if ( task->GetRepeat() > elapsed && ( !spectator || !task->MustSurvive() ) )
					{
						int repeat = task->GetRepeat();
						sprintf( ascii, "%d:%2.2d", (int)( repeat - elapsed ) / 60, (int)( repeat - elapsed ) % 60 );
						vgui2::localize()->ConvertANSIToUnicode( ascii, buffer, sizeof( buffer ) );
						vgui2::localize()->ConstructString( status, sizeof( status ),
							vgui2::localize()->Find( "#Career_Task_PartialRemaining" ), 1, buffer );
					}
					else
					{
						vgui2::localize()->ConstructString( status, sizeof( status ),
							vgui2::localize()->Find( "#Career_Task_PartialIncomplete" ), 0 );
					}
				}
			}
			else if ( task->MustSurvive() && remaining <= 0 )
			{
				vgui2::localize()->ConstructString( status, sizeof( status ),
					vgui2::localize()->Find( "#Career_Task_PartialMustLive" ), 0 );
			}
			else
			{
				vgui2::localize()->ConstructString( status, sizeof( status ),
					vgui2::localize()->Find( "#Career_Task_PartialIncomplete" ), 0 );
			}
		}

		KeyValues* data = new KeyValues( "data" );
		data->SetWString( "name", twoLines ? taskLines[0] : taskName );
		data->SetWString( "status", status );
		data->SetInt( "playerIndex", playerIndex );
		int item = FindItemIDForPlayerIndex( playerIndex );
		if ( item == -1 )
			item = m_pPlayerList->AddItem( 5, data );
		else
			m_pPlayerList->ModifyItem( item, 5, data );
		m_pPlayerList->SetItemFgColor( item, task->IsComplete() ? Color( 20, 192, 20, 255 ) : GetTeamColor( 3 ) );
		if ( twoLines )
		{
			++playerIndex;
			data->SetWString( "name", taskLines[1] );
			data->SetWString( "status", L"" );
			data->SetInt( "playerIndex", playerIndex );
			item = FindItemIDForPlayerIndex( playerIndex );
			if ( item == -1 )
				item = m_pPlayerList->AddItem( 5, data );
			else
				m_pPlayerList->ModifyItem( item, 5, data );
			m_pPlayerList->SetItemFgColor( item, task->IsComplete() ? Color( 20, 192, 20, 255 ) : GetTeamColor( 3 ) );
		}
		data->deleteThis();
	}
}
