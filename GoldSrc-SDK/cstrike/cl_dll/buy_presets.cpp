#include "buy_presets.h"
#include "hud.h"
#include "buy_preset_debug.h"
#include "game_controls/iviewport.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "parsemsg.h"
#include <stdlib.h>
#include <string.h>

BuyPresetManager* TheBuyPresets;
cvar_t* cl_preset_debug;

int __MsgFunc_ItemStatus( const char*, int size, void* buffer )
{
	BEGIN_READ( buffer, size );
	int items = READ_BYTE();
	TheBuyPresets->SetNightvision( ( items & 1 ) != 0 );
	TheBuyPresets->SetDefuser( ( items & 2 ) != 0 );
	return 0;
}

void __CmdFunc_BuyPresetEdit()
{
	if ( gViewPortInterface && gViewPortInterface->GetCurrentMenuID() != 38
		&& gViewPortInterface->GetCurrentMenuID() != 39 )
	{
		ShowBuyPresetMainMenu( true, REOPEN_NO );
	}
}

void __CmdFunc_BuyPreset()
{
	if ( gEngfuncs.Cmd_Argc() == 2 )
	{
		int index = atoi( gEngfuncs.Cmd_Argv( 1 ) ) - 1;
		if ( index >= 0 && index < TheBuyPresets->GetNumPresets() )
		{
			TheBuyPresets->PurchasePreset( index, false );
			return;
		}

		if ( IsPresetDebuggingEnabled() )
			gEngfuncs.Con_DPrintf( "buy_preset: preset %d doesn't exist\n", index );
	}
	else if ( IsPresetDebuggingEnabled() )
	{
		gEngfuncs.Con_DPrintf( "buy_preset: no preset specified\n" );
	}

	if ( TheBuyPresets->GetNumPresets() )
		gEngfuncs.Con_Printf( "usage:  buy_preset <1...%d>\n", TheBuyPresets->GetNumPresets() );
	else
		gEngfuncs.Con_Printf( "buy_preset: no presets are defined\n" );

	gEngfuncs.pfnPlaySoundByName( "common/wpn_select.wav", 1.0f );
}

BuyPresetManager::BuyPresetManager()
{
	ResetItems();
	m_rebuyString = NULL;
	Reset();
	gEngfuncs.pfnHookUserMsg( "ItemStatus", __MsgFunc_ItemStatus );

	if ( gHUD.IsGame( "czero" ) )
	{
		gEngfuncs.pfnAddCommand( "buy_preset", __CmdFunc_BuyPreset );
		gEngfuncs.pfnAddCommand( "buy_preset_edit", __CmdFunc_BuyPresetEdit );
	}
}

void BuyPresetManager::Save()
{
	KeyValues* data = new KeyValues( "Presets" );
	for ( BuyPresetList::iterator preset = m_presets.begin(); preset != m_presets.end(); ++preset )
		preset->Save( data );

	data->SaveToFile( vgui2::filesystem(), "BuyPresets.vdf", NULL );
	data->deleteThis();
}

const BuyPreset* BuyPresetManager::GetPreset( int index ) const
{
	if ( index >= 0 && (unsigned int)index < m_presets.size() )
		return &m_presets[index];

	return NULL;
}

BuyPreset* BuyPresetManager::GetEditPreset( int index )
{
	if ( index >= 0 && (unsigned int)index < m_editPresets.size() )
		return &m_editPresets[index];

	return NULL;
}

void BuyPresetManager::PurchasePreset( int presetIndex, bool withCareer )
{
	if ( presetIndex < 0 || (unsigned int)presetIndex >= m_presets.size() )
	{
		if ( IsPresetDebuggingEnabled() )
			gEngfuncs.Con_DPrintf( "buy_preset: preset %d doesn't exist.\n", presetIndex );

		gEngfuncs.pfnPlaySoundByName( "common/wpn_select.wav", 1.0f );
		return;
	}

	const BuyPreset& preset = m_presets[presetIndex];
	int setIndex;
	for ( setIndex = 0; setIndex < preset.GetNumSets(); ++setIndex )
	{
		const WeaponSet* itemSet = preset.GetSet( setIndex );
		if ( !itemSet )
			continue;

		WeaponSet currentSet;
		int currentCost;
		itemSet->GetCurrent( currentCost, currentSet, withCareer );
		if ( currentCost > 0 )
		{
			if ( IsPresetDebuggingEnabled() )
			{
				gEngfuncs.Con_DPrintf( "buy_preset: buying %ls for a total of $%d.\n",
					preset.GetName(), currentCost );
			}

			char commands[BUY_PRESET_COMMAND_LEN];
			currentSet.GenerateBuyCommands( commands );
			if ( IsPresetDebuggingEnabled() )
				gEngfuncs.Con_DPrintf( "%s\n", commands );

			gEngfuncs.pfnClientCmd( commands );
			return;
		}

		if ( currentCost == 0 )
			break;
	}

	if ( IsPresetDebuggingEnabled() )
	{
		if ( setIndex == 0 && preset.GetNumSets() )
			gEngfuncs.Con_DPrintf( "buy_preset: already have a complete %ls set.\n", preset.GetName() );
		else
			gEngfuncs.Con_DPrintf( "buy_preset: can't afford anything better from %ls.\n", preset.GetName() );
	}

	gEngfuncs.pfnPlaySoundByName( "common/wpn_moveselect.wav", 1.0f );
}

const char* BuyPresetManager::GetRebuyString()
{
	if ( m_rebuyString )
		return m_rebuyString;

	char* data = (char*)gEngfuncs.COM_LoadFile( "rebuy.txt", 5, NULL );
	if ( !data )
		return "";

	char token[1024];
	char rebuystring[1024] = { 0 };
	char* remainder = gEngfuncs.COM_ParseFile( data, token );
	while ( remainder )
	{
		strncat( rebuystring, token, sizeof( rebuystring ) - strlen( rebuystring ) - 1 );
		remainder = gEngfuncs.COM_ParseFile( remainder, token );
		if ( remainder )
			strncat( rebuystring, " ", sizeof( rebuystring ) - strlen( rebuystring ) - 1 );
	}

	gEngfuncs.COM_FreeFile( data );
	m_rebuyString = new char[strlen( rebuystring ) + 1];
	strcpy( m_rebuyString, rebuystring );
	return m_rebuyString;
}

int BuyPresetManager::GetAutobuyPresetIndex() const
{
	return -1;
}

void BuyPresetManager::ResetEditToDefaults()
{
	m_editPresets.clear();
	KeyValues* data = new KeyValues( "Presets" );
	data->LoadFromFile( vgui2::filesystem(), "BuyPresetsDefault.vdf", NULL );
	for ( KeyValues* values = data->GetFirstSubKey(); values; values = values->GetNextKey() )
	{
		BuyPreset preset;
		preset.Parse( values );
		m_editPresets.push_back( preset );
	}

	data->deleteThis();
}

void BuyPresetManager::Reset()
{
	m_presets.clear();
	if ( m_rebuyString )
	{
		delete[] m_rebuyString;
		m_rebuyString = NULL;
	}

	m_presets.clear();
	KeyValues* data = new KeyValues( "Presets" );
	data->LoadFromFile( vgui2::filesystem(), "BuyPresets.vdf", NULL );
	for ( KeyValues* values = data->GetFirstSubKey(); values; values = values->GetNextKey() )
	{
		BuyPreset preset;
		preset.Parse( values );
		m_presets.push_back( preset );
	}

	if ( m_presets.empty() )
	{
		ResetEditToDefaults();
		m_presets = m_editPresets;
	}

	while ( m_presets.size() < NUM_PRESETS )
	{
		BuyPreset preset;
		m_presets.push_back( preset );
	}

	data->deleteThis();
	m_editPresets = m_presets;
}
