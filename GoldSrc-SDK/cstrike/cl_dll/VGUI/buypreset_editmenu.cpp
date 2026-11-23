#include "buypreset_editmenu.h"
#include "buypreset_mainmenu.h"
#include "WeaponSetLabel.h"
#include "CareerBox.h"
#include "Button.h"
#include "Label.h"
#include "TextEntry.h"
#include "game_controls/iviewport.h"
#include "vgui/IScheme.h"
#include "cl_util.h"
#include "shared_util.h"
#include <stdio.h>
#include <stdlib.h>

using namespace vgui2;

static CBuyPresetEditMenu* pMenu;
static int thePresetIndex;

class PresetNameTextEntry : public TextEntry
{
public:
	PresetNameTextEntry( CBuyPresetEditMenu* parent, const char* name ) : TextEntry( parent, name ), m_pMenu( parent ) {}
	void FireActionSignal() override;

private:
	CBuyPresetEditMenu* m_pMenu;
};

void PresetNameTextEntry::FireActionSignal()
{
	TextEntry::FireActionSignal();
	m_pMenu->SetDirty();
}

void ShowBuyPresetEditMenu( int index )
{
	if ( !gViewPortInterface )
		return;
	thePresetIndex = index;
	MaintainBuyPresetInputFocus();
	gViewPortInterface->HideVGUIMenu( 38 );
	gViewPortInterface->ShowVGUIMenu( 39 );
}

CBuyPresetEditMenu::CBuyPresetEditMenu( Panel* parent )
	: CCSCareerBaseMenu( parent, "BuyPresetEditOutfitListMenu", false )
{
	m_isDirty = false;
	m_numValidChoices = 0;
	m_selectedIndex = -1;
	SetTitle( "#Cstrike_BuyPresetEditPresets", true );
	m_pTitleLabel = new Label( this, "Title", "" );
	m_pAcceptButton = new Button( this, "ok", "" );
	m_pPresetName = new PresetNameTextEntry( this, "PresetName" );
	for ( int i = 0; i < 5; ++i )
		m_pChoicePanel[i] = new BuyPresetEditPanel( this, SharedVarArgs( "choice%d", i ),
			"Resource/UI/BuyPresetEditPanel.res", i );
	LoadControlSettings( "Resource/UI/BuyPresetEditSubMenu.res", NULL );
}

CBuyPresetEditMenu::~CBuyPresetEditMenu()
{
	if ( this == pMenu )
		pMenu = NULL;
}

void CBuyPresetEditMenu::SetDirty()
{
	m_isDirty = true;
	m_pTitleLabel->SetText( "#Cstrike_BuyPresetEditPresetTitleDirty" );
	m_pAcceptButton->SetEnabled( true );
	m_numValidChoices = 0;
	BuyPreset* preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	bool sawBlank = preset == NULL;
	for ( int i = 0; i < 5; ++i )
	{
		const WeaponSet* set = preset ? preset->GetSet( i ) : NULL;
		m_pChoicePanel[i]->SetWeaponSet( set, false );
		m_pChoicePanel[i]->SetVisible( !sawBlank );
		m_pChoicePanel[i]->SetPanelBgColor( m_selectedIndex == i ? m_selectedBgColor
			: ( i & 1 ) ? m_normalBgColor : m_alternateBgColor );
		if ( !set )
			sawBlank = true;
		else if ( !sawBlank )
			++m_numValidChoices;
	}
}

void CBuyPresetEditMenu::UpdateButtons()
{
	m_pTitleLabel->SetText( m_isDirty ? "#Cstrike_BuyPresetEditPresetTitleDirty" : "#Cstrike_BuyPresetEditPresetTitle" );
	m_pAcceptButton->SetEnabled( m_isDirty );
	m_numValidChoices = 0;
	BuyPreset* preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	bool sawBlank = preset == NULL;
	for ( int i = 0; i < 5; ++i )
	{
		const WeaponSet* set = preset ? preset->GetSet( i ) : NULL;
		m_pChoicePanel[i]->SetWeaponSet( set, false );
		m_pChoicePanel[i]->SetVisible( !sawBlank );
		m_pChoicePanel[i]->SetPanelBgColor( m_selectedIndex == i ? m_selectedBgColor
			: ( i & 1 ) ? m_normalBgColor : m_alternateBgColor );
		if ( !set )
			sawBlank = true;
		else if ( !sawBlank )
			++m_numValidChoices;
	}
}

void CBuyPresetEditMenu::FinishEditingWeaponSet()
{
	BuyPreset* preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( preset )
		preset->ReplaceSet( m_selectedIndex, m_weaponSet );
	if ( !m_weaponSet.m_armor && !m_weaponSet.m_defuser && !m_weaponSet.m_flashbangs
		&& !m_weaponSet.m_HEGrenade && !m_weaponSet.m_helmet && !m_weaponSet.m_nightvision
		&& !m_weaponSet.m_smokeGrenade && m_weaponSet.m_primaryWeapon.GetWeaponID() == WEAPON_NONE
		&& !m_weaponSet.m_primaryWeapon.GetAmmoAmount() && m_weaponSet.m_secondaryWeapon.GetWeaponID() == WEAPON_NONE
		&& !m_weaponSet.m_secondaryWeapon.GetAmmoAmount() )
	{
		preset = TheBuyPresets->GetEditPreset( thePresetIndex );
		if ( preset )
			preset->DeleteSet( m_selectedIndex );
		m_selectedIndex = -1;
		UpdateButtons();
	}
	m_selectedIndex = -1;
	UpdateButtons();
}

void CBuyPresetEditMenu::EditFallback( int index )
{
	m_selectedIndex = index;
	BuyPreset* preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( !preset )
		return;
	const WeaponSet* set = preset->GetSet( m_selectedIndex );
	if ( set )
		m_weaponSet = *set;
	else if ( m_selectedIndex == preset->GetNumSets() )
		m_weaponSet.Reset();
	UpdateButtons();
}

void CBuyPresetEditMenu::DeleteWeaponSet()
{
	BuyPreset* preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( preset )
		preset->DeleteSet( m_selectedIndex );
	m_selectedIndex = -1;
	UpdateButtons();
}

void CBuyPresetEditMenu::Update()
{
	gEngfuncs.pfnClientCmd( "setpause\n" );
	gViewPortInterface->CloseTutorTextWindow();
	BuyPreset* preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( preset )
	{
		m_pPresetName->SetText( preset->GetName() );
		m_originalBuyPreset = *preset;
	}
	else
		m_originalBuyPreset = BuyPreset();
	m_isDirty = false;
	m_selectedIndex = -1;
	UpdateButtons();
}

void CBuyPresetEditMenu::ApplySchemeSettings( IScheme* scheme )
{
	Frame::ApplySchemeSettings( scheme );
	m_selectedFgColor = GetSchemeColor( "BrightControlText", scheme );
	m_selectedBgColor = GetSchemeColor( "SelectionBG", scheme );
	m_normalFgColor = GetSchemeColor( "BaseText", scheme );
	m_alternateFgColor = GetSchemeColor( "BaseText", scheme );
	m_normalBgColor = GetSchemeColor( "ControlDarkBG", scheme );
	m_alternateBgColor = GetSchemeColor( "ControlDarkBG", scheme );
	UpdateButtons();
}

void CBuyPresetEditMenu::OnCommand( const char* command )
{
	char commandBuf[32] = {};
	char args[2][32];
	const char* next = SharedParse( command );
	if ( next )
	{
		_snprintf( commandBuf, sizeof( commandBuf ), "%s", SharedGetToken() );
		commandBuf[sizeof( commandBuf ) - 1] = 0;
		next = SharedParse( next );
	}
	int numArgs = 0;
	for ( int i = 0; i < 2; ++i )
	{
		if ( next )
		{
			_snprintf( args[numArgs], sizeof( args[0] ), "%s", SharedGetToken() );
			args[numArgs][sizeof( args[0] ) - 1] = 0;
			++numArgs;
			next = SharedParse( next );
		}
	}
	if ( !stricmp( commandBuf, "Cancel" ) )
	{
		if ( numArgs == 1 && !stricmp( args[0], "clear" ) )
			m_selectedIndex = -1;
		UpdateButtons();
		Frame::OnCommand( command );
		return;
	}
	BuyPreset* preset = TheBuyPresets->GetEditPreset( thePresetIndex );
	if ( !preset )
	{
		ShowBuyPresetMainMenu( false, REOPEN_DONTCHANGE );
		Frame::OnCommand( "vguicancel" );
		return;
	}
	if ( !stricmp( command, "close_with_save_check" ) )
	{
		if ( m_isDirty )
		{
			CCareerQueryBox* box = new CCareerQueryBox( this, "BuyBoxQuitNoSave" );
			box->AddActionSignalTarget( this );
			box->ShowWindow();
		}
		else
		{
			if ( preset )
				*preset = m_originalBuyPreset;
			ShowBuyPresetMainMenu( false, REOPEN_DONTCHANGE );
		}
		Frame::OnCommand( command );
		return;
	}
	if ( !stricmp( commandBuf, "open_edit_main_menu" ) )
	{
		if ( numArgs == 1 && !stricmp( args[0], "save" ) )
		{
			wchar_t name[64];
			m_pPresetName->GetText( name, 64 );
			preset->SetName( name );
		}
		else if ( preset )
			*preset = m_originalBuyPreset;
		ShowBuyPresetMainMenu( false, REOPEN_DONTCHANGE );
		Frame::OnCommand( command );
		return;
	}
	CCareerBaseBox* box = NULL;
	if ( !stricmp( commandBuf, "primary" ) && numArgs == 1 )
	{
		EditFallback( atoi( args[0] ) );
		box = new CWeaponSelectBox( this, &m_weaponSet, false );
	}
	else if ( !stricmp( commandBuf, "secondary" ) && numArgs == 1 )
	{
		EditFallback( atoi( args[0] ) );
		box = new CWeaponSelectBox( this, &m_weaponSet, true );
	}
	else if ( !stricmp( commandBuf, "grenades" ) && numArgs == 1 )
	{
		EditFallback( atoi( args[0] ) );
		box = new CGrenadeSelectBox( this, &m_weaponSet );
	}
	else if ( !stricmp( commandBuf, "equipment" ) && numArgs == 1 )
	{
		EditFallback( atoi( args[0] ) );
		box = new CEquipmentSelectBox( this, &m_weaponSet );
	}
	else if ( !stricmp( commandBuf, "move" ) && numArgs == 2 )
	{
		int index = atoi( args[1] );
		if ( index >= 0 && index < preset->GetNumSets() )
		{
			if ( !stricmp( args[0], "up" ) )
			{
				if ( m_selectedIndex != 0 )
				{
					m_isDirty = true;
					preset->SwapSet( index, index - 1 );
					m_selectedIndex = -1;
				}
			}
			else if ( !stricmp( args[0], "down" ) && index < preset->GetNumSets() - 1 )
			{
				m_isDirty = true;
				preset->SwapSet( index, index + 1 );
				m_selectedIndex = -1;
			}
			UpdateButtons();
		}
	}
	else if ( !stricmp( command, "popup_ok" ) )
	{
		m_isDirty = true;
		FinishEditingWeaponSet();
	}
	if ( box )
	{
		box->AddActionSignalTarget( this );
		box->ShowWindow();
	}
	if ( !stricmp( command, "vguicancel" ) )
	{
		*preset = m_originalBuyPreset;
		ShowBuyPresetMainMenu( false, REOPEN_DONTCHANGE );
	}
	Frame::OnCommand( command );
}
