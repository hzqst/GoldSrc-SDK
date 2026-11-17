#include "hud.h"
#include "dodspectatorgui.h"
#include "idodviewport.h"
#include "game_controls/iviewport.h"
#include "Label.h"
#include "../../../engine/controls.h"
#include "../../public/utlvector.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <malloc.h>

using namespace vgui2;

extern float* GetTeamColor( int team );
extern float* GetClientColor( int player );

CDoDSpectatorGUI::CDoDSpectatorGUI( Panel* parent ) : BaseClass( parent )
{
	m_pReinforcementsLabel = dynamic_cast<Label*>( FindChildByName( "reinforcementslabel", false ) );
	m_pAlliesScoreLabel = dynamic_cast<Label*>( FindChildByName( "AlliesScoreValue", false ) );
	m_pAxisScoreLabel = dynamic_cast<Label*>( FindChildByName( "AxisScoreValue", false ) );
	m_pTimerLabel = dynamic_cast<Label*>( FindChildByName( "timerlabel", false ) );
	m_pExtraInfo = dynamic_cast<Label*>( FindChildByName( "extrainfo", false ) );
}

bool CDoDSpectatorGUI::CanSpectateTeam( int team )
{
	const int forceCamera = gDoDViewPortInterface->GetForceCamera();
	if ( g_iTeamNumber && !gEngfuncs.IsSpectateOnly() && forceCamera )
		return g_iTeamNumber == team;
	return true;
}

void CDoDSpectatorGUI::GetPlayerLabelText( int player, wchar_t* text, int textLength )
{
	wcscpy( text, L"" );
	wchar_t playerName[64] = {};
	wchar_t health[10];
	if ( gHUD.m_StatusBar.InDeathCamMode() )
		player = g_iUser3;
	if ( player > 0 )
	{
		VGuiLibraryPlayer_t info = gViewPortInterface->GetClientDllInterface()->CallEnghudGetPlayerInfo( player );
		if ( !info.name )
			return;
		const int bufferSize = 2 * (int)strlen( info.name ) + 1;
		CUtlVector<char> safeName;
		safeName.AddMultipleToTail( bufferSize );
		gViewPortInterface->MakeSafeName( info.name, safeName.Base(), bufferSize );
		localize()->ConvertANSIToUnicode( safeName.Base(), playerName, sizeof( playerName ) );
		if ( info.health <= 0 )
		{
			localize()->ConstructString( text, textLength, localize()->Find( "#Spec_PlayerItem" ), 1, playerName );
			return;
		}
		swprintf( health, 10, L"%i", min( info.health, 100 ) );
	}
	else
	{
		const char* name = gHUD.m_StatusBar.GetTargetName();
		if ( !name || !*name )
			return;
		const int targetHealth = gHUD.m_StatusBar.m_iHealth > 100 ? 0 : gHUD.m_StatusBar.m_iHealth;
		swprintf( health, 10, L"%i", targetHealth );
		localize()->ConvertANSIToUnicode( name, playerName, sizeof( playerName ) );
	}
	localize()->ConstructString( text, textLength, localize()->Find( "#Spec_PlayerItem_Team" ), 2, playerName, health );
}

void CDoDSpectatorGUI::GetPlayerLabelColor( int player, Color& color )
{
	if ( gHUD.m_StatusBar.InDeathCamMode() )
		player = g_iUser3;
	float* teamColor = player > 0 ? GetClientColor( player ) : GetTeamColor( gHUD.m_StatusBar.m_iTargetTeam );
	color.SetColor( (int)( teamColor[0] * 255 ), (int)( teamColor[1] * 255 ), (int)( teamColor[2] * 255 ), 255 );
}

void CDoDSpectatorGUI::UpdateSpectatorPlayerList()
{
	BaseClass::UpdateSpectatorPlayerList();
	int alliesScore = 0;
	int axisScore = 0;
	bool foundAllies = false;
	bool foundAxis = false;
	for ( int index = 0; index < 64; ++index )
	{
		VGuiLibraryTeamInfo_t info = gDoDViewPortInterface->GetPlayerTeamInfo( index );
		if ( info.teamnumber == 1 )
		{
			foundAllies = true;
			alliesScore = gDoDViewPortInterface->GetExtraScoreBoardInfo( index ).score;
		}
		else if ( info.teamnumber == 2 )
		{
			foundAxis = true;
			axisScore = gDoDViewPortInterface->GetExtraScoreBoardInfo( index ).score;
		}
		if ( foundAllies && foundAxis )
			break;
	}
	wchar_t score[10];
	swprintf( score, 10, L"%i", alliesScore );
	m_pAlliesScoreLabel->SetText( score );
	swprintf( score, 10, L"%i", axisScore );
	m_pAxisScoreLabel->SetText( score );
}

void CDoDSpectatorGUI::Update( const char* serverName, int player, int mode, bool spectateOnly,
	int spectatorCount, const char* mapName )
{
	UpdateSpectatorPlayerList();
	BaseClass::Update( serverName, player, mode, spectateOnly, spectatorCount, mapName );
	m_pExtraInfo->SetVisible( spectateOnly );
}

void CDoDSpectatorGUI::UpdateTimer()
{
	if ( gEngfuncs.IsSpectateOnly() )
	{
		const int seconds = max( 0, (int)( gHUD.m_fRoundEndsTime - gHUD.m_flTime ) );
		wchar_t timer[63];
		swprintf( timer, 63, L"%d:%02d", seconds / 60, seconds % 60 );
		m_pTimerLabel->SetText( timer );
		m_pReinforcementsLabel->SetText( " " );
	}
	else if ( m_pReinforcementsLabel && m_pTimerLabel )
	{
		m_pTimerLabel->SetText( " " );
		const int seconds = (int)( (float)(int)( gHUD.m_ObjectiveIcons.m_flWaveTime + 1 ) - gHUD.m_flTime );
		if ( seconds < 0 )
			m_pReinforcementsLabel->SetVisible( false );
		else
		{
			m_pReinforcementsLabel->SetVisible( true );
			char timer[20];
			wchar_t wideTimer[63];
			wchar_t text[128];
			snprintf( timer, 8, "%d:%02d", seconds / 60, seconds % 60 );
			localize()->ConvertANSIToUnicode( timer, wideTimer, 32 );
			localize()->ConstructString( text, sizeof( text ), localize()->Find( "#Dod_Reinforcements_in" ), 1, wideTimer );
			m_pReinforcementsLabel->SetText( text );
		}
	}
}
