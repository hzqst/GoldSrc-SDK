//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// 
//
// $NoKeywords: $
//=============================================================================

#include "CreateMultiplayerGameBotPage.h"
#include "CvarToggleCheckButton.h"
#include "GameUI_Interface.h"

#include "ComboBox.h"
#include "TextEntry.h"
#include "vgui/KeyValues.h"
#include "wrect.h"
#include "cl_dll.h"

#include <string.h>

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

static const char* const joinTeamArg[] =
{
	"any",
	"ct",
	"t",
	NULL
};

static const char* const chatterArg[] =
{
	"normal",
	"minimal",
	"radio",
	"off",
	NULL
};

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameBotPage::CCreateMultiplayerGameBotPage( vgui2::Panel* parent, const char* name,
		KeyValues* botKeys ) : BaseClass( parent, name, true )
{
	m_allowRogues = new CCvarToggleCheckButton( this, "BotAllowRogueCheck", "", "bot_allow_rogues" );
	m_allowPistols = new CCvarToggleCheckButton( this, "BotAllowPistolsCheck", "", "bot_allow_pistols" );
	m_allowShotguns = new CCvarToggleCheckButton( this, "BotAllowShotgunsCheck", "", "bot_allow_shotguns" );
	m_allowSubmachineGuns = new CCvarToggleCheckButton( this, "BotAllowSubmachineGunsCheck", "",
			"bot_allow_sub_machine_guns" );
	m_allowRifles = new CCvarToggleCheckButton( this, "BotAllowRiflesCheck", "", "bot_allow_rifles" );
	m_allowMachineGuns = new CCvarToggleCheckButton( this, "BotAllowMachineGunsCheck", "", "bot_allow_machine_guns" );
	m_allowGrenades = new CCvarToggleCheckButton( this, "BotAllowGrenadesCheck", "", "bot_allow_grenades" );
	m_allowSnipers = new CCvarToggleCheckButton( this, "BotAllowSnipersCheck", "", "bot_allow_snipers" );
	m_allowShields = new CCvarToggleCheckButton( this, "BotAllowShieldCheck", "", "bot_allow_shield" );
	m_joinAfterPlayer = new CCvarToggleCheckButton( this, "BotJoinAfterPlayerCheck", "", "bot_join_after_player" );
	m_deferToHuman = new CCvarToggleCheckButton( this, "BotDeferToHumanCheck", "", "bot_defer_to_human" );

	// set up team join combo box
	// NOTE: If order of AddItem is changed, update the associated #defines
	m_joinTeamCombo = new vgui2::ComboBox( this, "BotJoinTeamCombo", 3, false );
	m_joinTeamCombo->AddItem( "#Cstrike_Random", NULL );
	m_joinTeamCombo->AddItem( "#Cstrike_ScoreBoard_CT", NULL );
	m_joinTeamCombo->AddItem( "#Cstrike_ScoreBoard_Ter", NULL );

	m_chatterCombo = new vgui2::ComboBox( this, "BotChatterCombo", 4, false );
	m_chatterCombo->AddItem( "#Cstrike_Bot_Chatter_Normal", NULL );
	m_chatterCombo->AddItem( "#Cstrike_Bot_Chatter_Minimal", NULL );
	m_chatterCombo->AddItem( "#Cstrike_Bot_Chatter_Radio", NULL );
	m_chatterCombo->AddItem( "#Cstrike_Bot_Chatter_Off", NULL );

	m_prefixEntry = new vgui2::TextEntry( this, "BotPrefixEntry" );
	LoadControlSettings( "Resource/CreateMultiplayerGameBotPage.res", NULL );

	const char* prefix;
	if ( engine->pfnGetCvarPointer( "bot_difficulty" ) )
	{
		SetJoinTeamCombo( engine->pfnGetCvarString( "bot_join_team" ) );
		SetChatterCombo( engine->pfnGetCvarString( "bot_chatter" ) );
		prefix = engine->pfnGetCvarString( "bot_prefix" );
	}
	else
	{
		m_joinAfterPlayer->SetSelected( botKeys->GetInt( "bot_join_after_player", 0 ) != 0 );
		m_allowRogues->SetSelected( botKeys->GetInt( "bot_allow_rogues", 0 ) != 0 );
		m_allowPistols->SetSelected( botKeys->GetInt( "bot_allow_pistols", 0 ) != 0 );
		m_allowShotguns->SetSelected( botKeys->GetInt( "bot_allow_shotguns", 0 ) != 0 );
		m_allowSubmachineGuns->SetSelected( botKeys->GetInt( "bot_allow_sub_machine_guns", 0 ) != 0 );
		m_allowMachineGuns->SetSelected( botKeys->GetInt( "bot_allow_machine_guns", 0 ) != 0 );
		m_allowRifles->SetSelected( botKeys->GetInt( "bot_allow_rifles", 0 ) != 0 );
		m_allowSnipers->SetSelected( botKeys->GetInt( "bot_allow_snipers", 0 ) != 0 );
		m_allowGrenades->SetSelected( botKeys->GetInt( "bot_allow_grenades", 0 ) != 0 );
		m_allowShields->SetSelected( botKeys->GetInt( "bot_allow_shield", 0 ) != 0 );
		m_deferToHuman->SetSelected( botKeys->GetInt( "bot_defer_to_human", 0 ) != 0 );
		SetJoinTeamCombo( botKeys->GetString( "bot_join_team", "" ) );
		SetChatterCombo( botKeys->GetString( "bot_chatter", "" ) );
		prefix = botKeys->GetString( "bot_prefix", "" );
	}

	if ( prefix )
		SetControlString( "BotPrefixEntry", prefix );
}

CCreateMultiplayerGameBotPage::~CCreateMultiplayerGameBotPage()
{
}

void CCreateMultiplayerGameBotPage::SetJoinTeamCombo( const char* team )
{
	if ( !team )
	{
		m_joinTeamCombo->ActivateItemByRow( 0 );
		return;
	}

	for ( int i = 0; joinTeamArg[i]; i++ )
	{
		if ( !stricmp( team, joinTeamArg[i] ) )
		{
			m_joinTeamCombo->ActivateItemByRow( i );
			return;
		}
	}
}

void CCreateMultiplayerGameBotPage::SetChatterCombo( const char* chatter )
{
	if ( !chatter )
	{
		m_joinTeamCombo->ActivateItemByRow( 0 );
		return;
	}

	for ( int i = 0; chatterArg[i]; i++ )
	{
		if ( !stricmp( chatter, chatterArg[i] ) )
		{
			m_chatterCombo->ActivateItemByRow( i );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Put all cvars and their current values into 'botKeys'
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameBotPage::UpdateKeys( KeyValues* botKeys )
{
	botKeys->SetInt( "bot_join_after_player", m_joinAfterPlayer->IsSelected() );
	botKeys->SetInt( "bot_allow_rogues", m_allowRogues->IsSelected() );
	botKeys->SetInt( "bot_allow_pistols", m_allowPistols->IsSelected() );
	botKeys->SetInt( "bot_allow_shotguns", m_allowShotguns->IsSelected() );
	botKeys->SetInt( "bot_allow_sub_machine_guns", m_allowSubmachineGuns->IsSelected() );
	botKeys->SetInt( "bot_allow_machine_guns", m_allowMachineGuns->IsSelected() );
	botKeys->SetInt( "bot_allow_rifles", m_allowRifles->IsSelected() );
	botKeys->SetInt( "bot_allow_snipers", m_allowSnipers->IsSelected() );
	botKeys->SetInt( "bot_allow_grenades", m_allowGrenades->IsSelected() );
	botKeys->SetInt( "bot_allow_shield", m_allowShields->IsSelected() );
	botKeys->SetInt( "bot_defer_to_human", m_deferToHuman->IsSelected() );
	botKeys->SetString( "bot_join_team", joinTeamArg[m_joinTeamCombo->GetActiveItem()] );
	botKeys->SetString( "bot_chatter", chatterArg[m_chatterCombo->GetActiveItem()] );

	char entryBuffer[256];
	m_prefixEntry->GetText( entryBuffer, sizeof( entryBuffer ) );
	botKeys->SetString( "bot_prefix", entryBuffer );
}

void CCreateMultiplayerGameBotPage::OnResetChanges()
{
}

//-----------------------------------------------------------------------------
// Called to get data from the page
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameBotPage::OnApplyChanges()
{
}
