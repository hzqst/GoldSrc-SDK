//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "buy_presets.h"
#include "buymenu.h"
#include "buysubmenu.h"
#include "counterstrikeviewport.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "BuildGroup.h"
#include "Button.h"
#include "Label.h"
#include <stdio.h>

const wchar_t* NumAsWString( int value );

CBuyMenu::CBuyMenu( vgui2::Panel* parent ) : vgui2::WizardPanel( parent, "BuyMenu" )
{
	if ( g_pCareerUI && g_pCareerUI->IsPlayingMatch() )
	{
		SetScheme( "CareerClientScheme" );
		SetProportional( true );
		m_isCareer = true;
	}
	else
	{
		SetScheme( "ClientScheme" );
		SetProportional( true );
		m_isCareer = false;
	}
	SetTitle( "#Cstrike_Buy_Menu", true );
	SetMoveable( false );
	SetSizeable( false );

	// hide the system buttons
	SetTitleBarVisible( false );
	SetAutoDelete( false ); // we reuse this panel, don't let WizardPanel delete us

	// the base sub panels
	_mainMenu = new CBuySubMenu( this, "mainmenu" );
	for ( int i = 0; i < 4; ++i )
	{
		if ( gHUD.IsGame( "czero" ) )
		{
			char name[32];
			sprintf( name, "BuyPresetButton%c", 'A' + i );
			m_pBuyPresetButtons[i] = new vgui2::Button( _mainMenu, name, "" );
		}
		else
		{
			m_pBuyPresetButtons[i] = NULL;
		}
	}
	if ( gHUD.IsGame( "czero" ) )
	{
		m_pMoney = new vgui2::Label( _mainMenu, "money", "" );
		m_pMainBackground = new vgui2::Panel( _mainMenu, "mainBackground" );
	}
	else
	{
		m_pMoney = NULL;
		m_pMainBackground = NULL;
	}
	m_lastMoney = -1;
	_mainMenu->LoadControlSettings( "Resource/UI/MainBuyMenu.res", NULL );
	_mainMenu->SetVisible( false );
	_equipMenu = new CBuySubMenu( this, "equipmenu" );
	if ( g_iTeamNumber == 1 )
		_equipMenu->LoadControlSettings( "Resource/UI/BuyEquipment_TER.res", NULL );
	else
		_equipMenu->LoadControlSettings( "Resource/UI/BuyEquipment_CT.res", NULL );
	_equipMenu->SetVisible( false );
	LoadControlSettings( "Resource/UI/BuyMenu.res", NULL );
	ShowButtons( false );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBuyMenu::~CBuyMenu()
{
}

void CBuyMenu::ActivateBuyMenu()
{
	gEngfuncs.pfnServerCmd( "setpause\n" );
	_equipMenu->SetVisible( false );
	Run( _mainMenu );
	UpdateBuyPresets( true );
}

void CBuyMenu::ActivateEquipmentMenu()
{
	gEngfuncs.pfnServerCmd( "setpause\n" );
	_mainMenu->SetVisible( false );
	Run( _equipMenu );
}

void CBuyMenu::UpdateBuyPresets( bool )
{
	if ( TheBuyPresets )
	{
		int count = TheBuyPresets->GetNumPresets();
		if ( count > 4 )
		{
			count = 4;
		}
		for ( int i = 0; i < count; ++i )
		{
			if ( !m_pBuyPresetButtons[i] )
			{
				continue;
			}
			const BuyPreset* preset = TheBuyPresets->GetPreset( i );
			int currentCost = -1;
			WeaponSet currentSet;
			for ( int j = 0; j < preset->GetNumSets(); ++j )
			{
				const WeaponSet* set = preset->GetSet( j );
				if ( set )
				{
					set->GetCurrent( currentCost, currentSet, false );
					if ( currentCost >= 0 )
					{
						break;
					}
				}
			}
			char text[1024];
			wchar_t wideText[1024];
			_snprintf( text, sizeof( text ), "#Cstrike_BuyMenuPreset%d", i + 1 );
			vgui2::localize()->ConstructString( wideText, sizeof( wideText ),
				vgui2::localize()->Find( text ), 2, preset->GetName(),
				NumAsWString( currentCost < 0 ? 0 : currentCost ) );
			m_pBuyPresetButtons[i]->SetText( wideText );
			_snprintf( text, sizeof( text ), "buy_preset %d", i + 1 );
			m_pBuyPresetButtons[i]->SetCommand( text );
			m_pBuyPresetButtons[i]->SetVisible( true );
			m_pBuyPresetButtons[i]->SetEnabled( currentCost >= 0 );
		}
		for ( int i = count + 1; i < 4; ++i )
		{
			if ( m_pBuyPresetButtons[i] )
			{
				m_pBuyPresetButtons[i]->SetVisible( false );
				m_pBuyPresetButtons[i]->SetEnabled( true );
			}
		}
	}
	else
	{
		for ( int i = 0; i < 4; ++i )
		{
			if ( m_pBuyPresetButtons[i] )
			{
				m_pBuyPresetButtons[i]->SetVisible( false );
				m_pBuyPresetButtons[i]->SetEnabled( false );
			}
		}
	}
}

void CBuyMenu::OnClose()
{
	gEngfuncs.pfnServerCmd( "unpause\n" );
	vgui2::WizardPanel::OnClose();
	_mainMenu->DeleteSubPanels();
	_equipMenu->DeleteSubPanels();
	ResetHistory();
	SetVisible( false );
}

void CBuyMenu::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::WizardPanel::ApplySchemeSettings( scheme );
	if ( m_pMainBackground )
	{
		m_pMainBackground->SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
		m_pMainBackground->SetBgColor( GetSchemeColor( "ButtonBgColor", GetBgColor(), scheme ) );
	}
}

void CBuyMenu::Paint()
{
	if ( m_pMoney && m_lastMoney != gHUD.m_accountBalance.m_iAccount )
	{
		m_lastMoney = gHUD.m_accountBalance.m_iAccount;
		wchar_t text[128] = {};
		wchar_t* format = vgui2::localize()->Find( "#Cstrike_Current_Money" );
		if ( !format )
		{
			format = (wchar_t*)L"%s1";
		}
		vgui2::localize()->ConstructString( text, sizeof( text ), format, 1, NumAsWString( m_lastMoney ) );
		m_pMoney->SetText( text );
	}
	vgui2::Panel::Paint();
}

void CBuyMenu::Activate()
{
	vgui2::Frame::Activate();
	FitToFullScreen( this );
}

void FitToFullScreen( vgui2::EditablePanel* panel )
{
	if ( !panel || !panel->IsProportional() )
	{
		return;
	}
	panel->SetPos( 0, 0 );
	int screenWide, screenTall, wide, tall;
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	panel->GetSize( wide, tall );
	if ( wide == screenWide && tall == screenTall )
	{
		return;
	}
	panel->SetSize( screenWide, screenTall );
	wide = GetAlternateProportionalValueFromScaled( vgui2::scheme()->GetProportionalScaledValue( 640 ) );
	tall = GetAlternateProportionalValueFromScaled( vgui2::scheme()->GetProportionalScaledValue( 480 ) );
	int offsetX = ( screenWide - wide ) / 2;
	int offsetY = ( screenTall - tall ) / 2;
	if ( offsetX && offsetY )
	{
		if ( offsetX <= offsetY )
		{
			offsetY -= offsetX;
			offsetX = 0;
		}
		else
		{
			offsetX -= offsetY;
			offsetY = 0;
		}
	}
	if ( panel->GetBuildGroup() && panel->GetBuildGroup()->GetPanelList() )
	{
		CUtlVector<vgui2::PHandle>* panels = panel->GetBuildGroup()->GetPanelList();
		CUtlVector<vgui2::Panel*> resizedPanels;
		CUtlVector<vgui2::Panel*> movedPanels;
		for ( int i = 0; i < panels->Count(); ++i )
		{
			vgui2::PHandle handle = ( *panels )[i];
			vgui2::Panel* child = handle.Get();

			bool found = false;
			for ( int j = 0; j < resizedPanels.Count(); ++j )
			{
				if ( child == resizedPanels[j] )
					found = true;
			}

			if ( child && !found )
			{
				resizedPanels.AddToTail( child );
				if ( child != panel )
				{
					RepositionControl( child );
				}
			}
		}
		for ( int i = 0; i < panels->Count(); ++i )
		{
			vgui2::PHandle handle = ( *panels )[i];
			vgui2::Panel* child = handle.Get();

			bool found = false;
			for ( int j = 0; j < movedPanels.Count(); ++j )
			{
				if ( child == movedPanels[j] )
					found = true;
			}

			if ( child && !found )
			{
				movedPanels.AddToTail( child );
				if ( child != panel )
				{
					int x, y;
					child->GetPos( x, y );
					child->SetPos( x + offsetX, y + offsetY );
				}
			}
		}
	}
}
