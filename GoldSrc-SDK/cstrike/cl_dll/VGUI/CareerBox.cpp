#include "CareerBox.h"
#include "hud.h"
#include "GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "IBaseUI.h"
#include "BitmapImagePanel.h"
#include "Button.h"
#include "Label.h"
#include "vgui_controls/TextEntry.h"
#include "BuildGroup.h"
#include "vgui_controls/ComboBox.h"
#include "Menu.h"
#include "TextImage.h"
#include "WeaponSetLabel.h"
#include "buypreset_listbox.h"
#include "buy_presets.h"
#include "shared_util.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui/IImage.h"
#include "r_studioint.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef PostMessage
#undef PostMessage
#endif

extern engine_studio_api_t IEngineStudio;

CCareerBaseBox* CCareerBaseBox::s_currentBox;

CCareerBaseBox::CCareerBaseBox( vgui2::Panel* parent, const char* panelName, bool loadResources )
	: vgui2::Frame( parent, panelName, false, false )
{
	CloseOpenBoxes( true );
	s_currentBox = this;
	if ( g_pCareerUI != NULL && g_pCareerUI->IsPlayingMatch() )
		SetScheme( "CareerClientScheme" );
	else
		SetScheme( "ClientScheme" );
	SetProportional( true );
	SetMoveable( false );
	SetSizeable( false );
	m_pTopLeftPanel = new vgui2::CBitmapImagePanel( this, "TopLeftPanel", "gfx/vgui/round_corner_nw" );
	m_pTopRightPanel = new vgui2::CBitmapImagePanel( this, "TopRightPanel", "gfx/vgui/round_corner_ne" );
	m_pBottomLeftPanel = new vgui2::CBitmapImagePanel( this, "BottomLeftPanel", "gfx/vgui/round_corner_sw" );
	m_pBottomRightPanel = new vgui2::CBitmapImagePanel( this, "BottomRightPanel", "gfx/vgui/round_corner_se" );
	m_pTopLeftBorderPanel = new vgui2::CBitmapImagePanel( this, "TopLeftBorderPanel", "gfx/vgui/round_corner_small_nw" );
	m_pTopRightBorderPanel = new vgui2::CBitmapImagePanel( this, "TopRightBorderPanel", "gfx/vgui/round_corner_small_ne" );
	m_pBottomLeftBorderPanel = new vgui2::CBitmapImagePanel( this, "BottomLeftBorderPanel", "gfx/vgui/round_corner_small_sw" );
	m_pBottomRightBorderPanel = new vgui2::CBitmapImagePanel( this, "BottomRightBorderPanel", "gfx/vgui/round_corner_small_se" );
	m_pGapPanel = new vgui2::Panel( this, "GapPanel" );
	m_pIconPanel = new vgui2::CBitmapImagePanel( this, "IconPanel", "gfx/vgui/icon_!" );
	m_pTextLabel = new vgui2::Label( this, "TextLabel", "" );
	m_pOkButton = new vgui2::Button( this, "OkButton", "" );
	m_pOkButton->SetVisible( false );
	new vgui2::Button( this, "CancelButton", "" );
	m_shouldAutoClose = true;
	if ( loadResources )
	{
		char* buf = new char[strlen( panelName ) + 17];
		strcpy( buf, "resource/UI/" );
		strcat( buf, panelName );
		strcat( buf, ".res" );
		LoadControlSettings( buf, NULL );
		delete[] buf;
	}
}

CCareerBaseBox::~CCareerBaseBox()
{
	if ( this == s_currentBox )
		s_currentBox = NULL;
}

void CCareerBaseBox::ShowWindow()
{
	SetVisible( true );
	SetEnabled( true );
	MoveToFront();
}

void CCareerBaseBox::DoModal()
{
	ShowWindow();
	vgui2::input()->SetAppModalSurface( GetVPanel() );
}

void CCareerBaseBox::CloseOpenBoxes( bool force )
{
	if ( s_currentBox != NULL && ( force || s_currentBox->m_shouldAutoClose ) )
	{
		s_currentBox->Close();
		s_currentBox = NULL;
	}
}

void CCareerBaseBox::BringToFront()
{
	if ( s_currentBox != NULL && s_currentBox->IsVisible() )
	{
		s_currentBox->MoveToFront();
		s_currentBox->DoModal();
	}
}

bool CCareerBaseBox::HasVisibleChildPopup()
{
	if ( !s_currentBox || !s_currentBox->IsVisible() ||
		vgui2::input()->GetAppModalSurface() != s_currentBox->GetVPanel() )
		return false;

	vgui2::VPANEL box = s_currentBox->GetVPanel();
	for ( int i = 0; i < vgui2::surface()->GetPopupCount(); ++i )
	{
		vgui2::VPANEL popup = vgui2::surface()->GetPopup( i );
		if ( !popup || popup == box )
			continue;
		bool descendant = false;
		vgui2::VPANEL panel = popup;
		for ( ; panel; panel = vgui2::ipanel()->GetParent( panel ) )
		{
			if ( !vgui2::ipanel()->IsVisible( panel ) )
				break;
			if ( panel == box )
				descendant = true;
		}
		if ( descendant && !panel )
			return true;
	}
	return false;
}

void CCareerBaseBox::UndoModal()
{
	if ( s_currentBox != NULL && s_currentBox->GetVPanel() == vgui2::input()->GetAppModalSurface() )
		vgui2::input()->SetAppModalSurface( 0 );
}

void CCareerBaseBox::SetLabelText( const char* text )
{
	m_pTextLabel->SetText( text );
}

void CCareerBaseBox::SetLabelText( const wchar_t* text )
{
	m_pTextLabel->SetText( text );
}

void CCareerBaseBox::FriendLoadResources( const char* resource )
{
	if ( resource != NULL )
		LoadControlSettings( resource, NULL );
}

void CCareerBaseBox::ActivateBuildMode()
{
	SetProportional( false );
	SetScheme( "TrackerScheme" );
	InvalidateLayout( false, true );
	if ( GetBuildGroup() != NULL && GetBuildGroup()->GetResourceName() != NULL )
	{
		char* resourceName = CloneString( GetBuildGroup()->GetResourceName() );
		LoadControlSettings( resourceName, NULL );
		delete[] resourceName;
	}
	vgui2::EditablePanel::ActivateBuildMode();
}

void CCareerBaseBox::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	vgui2::Frame::ApplySchemeSettings( scheme );
	m_bgColor = GetSchemeColor( "CareerBoxBG", GetSchemeColor( "ControlBG", scheme ), scheme );
	m_borderColor = GetSchemeColor( "BorderBuyPreset", scheme );
	if ( !IEngineStudio.IsHardware() )
		m_bgColor[3] = 255;
	Color cornerColor( 255, 255, 255, m_bgColor[3] );
	m_pTopLeftPanel->m_bgColor = cornerColor;
	m_pTopRightPanel->m_bgColor = cornerColor;
	m_pBottomLeftPanel->m_bgColor = cornerColor;
	m_pBottomRightPanel->m_bgColor = cornerColor;
	m_pIconPanel->m_bgColor = GetSchemeColor( "BaseText", scheme );
	m_borderColor = GetSchemeColor( "BorderBuyPreset", scheme );
	m_pTopLeftBorderPanel->m_bgColor = m_borderColor;
	m_pTopRightBorderPanel->m_bgColor = m_borderColor;
	m_pBottomLeftBorderPanel->m_bgColor = m_borderColor;
	m_pBottomRightBorderPanel->m_bgColor = m_borderColor;
	SetBorder( NULL );
}

void CCareerBaseBox::PerformLayout()
{
	vgui2::Frame::PerformLayout();
	int x, y, wide, tall, screenWide, screenTall;
	GetBounds( x, y, wide, tall );
	vgui2::surface()->GetScreenSize( screenWide, screenTall );
	if ( wide / 2 + x != screenWide / 2 )
	{
		SetPos( screenWide / 2 - wide / 2, screenTall / 2 - tall / 2 );
		GetBounds( x, y, wide, tall );
	}
	int corner = IEngineStudio.IsHardware() == 1 ? 10 : 0;
	m_pTopLeftPanel->SetBounds( 0, 0, corner, corner );
	m_pTopRightPanel->SetBounds( wide - corner - 1, 0, corner, corner );
	m_pBottomLeftPanel->SetBounds( 0, tall - corner - 1, corner, corner );
	m_pBottomRightPanel->SetBounds( wide - corner - 1, tall - corner - 1, corner, corner );
	m_pTopLeftBorderPanel->SetBounds( 0, 0, corner, corner );
	m_pTopRightBorderPanel->SetBounds( wide - corner - 1, 0, corner, corner );
	m_pBottomLeftBorderPanel->SetBounds( 0, tall - corner - 1, corner, corner );
	m_pBottomRightBorderPanel->SetBounds( wide - corner - 1, tall - corner - 1, corner, corner );
	m_pTopLeftBorderPanel->MoveToFront();
	m_pTopRightBorderPanel->MoveToFront();
	m_pBottomLeftBorderPanel->MoveToFront();
	m_pBottomRightBorderPanel->MoveToFront();
	int gapX, gapY, gapWide, gapTall;
	m_pGapPanel->GetBounds( gapX, gapY, gapWide, gapTall );
	if ( gapTall > gapWide )
		m_pGapPanel->SetBounds( gapX, 0, gapWide, tall );
	else if ( gapTall < gapWide )
		m_pGapPanel->SetBounds( 0, gapY, wide, gapTall );
}

void CCareerBaseBox::PaintBackground()
{
	vgui2::surface()->DrawSetColor( m_bgColor );
	int x1, y1, x2, y2;
	if ( m_pGapPanel->GetTall() > m_pGapPanel->GetWide() )
	{
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pBottomLeftPanel->GetPos( x2, y2 );
		y1 += m_pTopLeftPanel->GetTall();
		x2 += m_pBottomLeftPanel->GetWide();
		vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pGapPanel->GetPos( x2, y2 );
		x1 += m_pTopLeftPanel->GetWide();
		y2 += m_pGapPanel->GetTall();
		vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pGapPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		x1 += m_pGapPanel->GetWide();
		y2 += m_pBottomRightPanel->GetTall();
		vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pTopRightPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		y1 += m_pTopRightPanel->GetTall();
		x2 += m_pBottomRightPanel->GetWide();
	}
	else
	{
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pTopRightPanel->GetPos( x2, y2 );
		x1 += m_pTopLeftPanel->GetWide();
		y2 += m_pTopRightPanel->GetTall();
		vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pTopLeftPanel->GetPos( x1, y1 );
		m_pGapPanel->GetPos( x2, y2 );
		y1 += m_pTopLeftPanel->GetTall();
		x2 += m_pGapPanel->GetWide();
		vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pGapPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		y1 += m_pGapPanel->GetTall();
		x2 += m_pBottomRightPanel->GetWide();
		vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
		m_pBottomLeftPanel->GetPos( x1, y1 );
		m_pBottomRightPanel->GetPos( x2, y2 );
		x1 += m_pBottomLeftPanel->GetWide();
		y2 += m_pBottomRightPanel->GetTall();
	}
	vgui2::surface()->DrawFilledRect( x1, y1, x2, y2 );
}

void CCareerBaseBox::Paint()
{
	vgui2::Frame::Paint();
	vgui2::surface()->DrawSetColor( m_borderColor );
	vgui2::surface()->DrawSetTextColor( m_borderColor );
	int x1, y1, x2, y2;
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

void CCareerBaseBox::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_ESCAPE )
	{
		OnCommand( "Cancel" );
		if ( g_pBaseUI != NULL )
			g_pBaseUI->HideGameUI();
	}
	else if ( code == vgui2::KEY_ENTER )
	{
		OnCommand( "Ok" );
		return;
	}
	vgui2::Frame::OnKeyCodeTyped( code );
}

void CCareerBaseBox::OnCommand( const char* command )
{
	if ( !_stricmp( command, "Ok" ) )
	{
	}

	if ( !strncmp( command, "engine ", 7 ) )
		gEngfuncs.pfnClientCmd( SharedVarArgs( (char*)"%s\n", command + 7 ) );
	else if ( _stricmp( command, "close" ) )
		PostActionSignal( new KeyValues( "Command", "command", command ) );
	vgui2::Frame::OnCommand( command );
	Close();
}

void CCareerBaseBox::OnClose()
{
	if ( s_currentBox == this )
		s_currentBox = NULL;
	vgui2::Frame::OnClose();
	MarkForDeletion();
}

CCareerQueryBox::CCareerQueryBox( const char*, const char*, const char* name, vgui2::Panel* parent )
	: CCareerBaseBox( parent, name, true )
{
}

CCareerQueryBox::CCareerQueryBox( const wchar_t*, const wchar_t*, const char* name, vgui2::Panel* parent )
	: CCareerBaseBox( parent, name, true )
{
}

CCareerQueryBox::CCareerQueryBox( vgui2::Panel* parent, const char* name )
	: CCareerBaseBox( parent, name, true )
{
}

CCareerQueryBox::~CCareerQueryBox()
{
}

CCareerTextEntryBox::CCareerTextEntryBox( vgui2::Panel* parent, const char* title, const char* text,
	const char* command, void ( *callback )( vgui2::TextEntry* ) )
	: CCareerBaseBox( parent, "TextEntryBox", false )
{
	vgui2::Label* titleLabel = title != NULL ? new vgui2::Label( this, "TitleLabel", L"" ) : NULL;
	m_pEntry = new vgui2::TextEntry( this, "TextEntry" );
	LoadControlSettings( "resource/UI/TextEntryBox.res", NULL );
	m_callback = callback;
	m_command = NULL;
	if ( command != NULL )
	{
		m_command = new char[strlen( command ) + 1];
		strcpy( m_command, command );
	}
	if ( titleLabel != NULL )
		titleLabel->SetText( title );
	if ( text != NULL )
		m_pTextLabel->SetText( text );
	m_pEntry->SetText( L"" );
}

CCareerTextEntryBox::~CCareerTextEntryBox()
{
}

void CCareerTextEntryBox::ShowWindow()
{
	SetVisible( true );
	SetEnabled( true );
	MoveToFront();
	m_pEntry->RequestFocus( 0 );
	InvalidateLayout( false, false );
}

void CCareerTextEntryBox::SetCommand( const char* command )
{
	delete[] m_command;
	m_command = NULL;
	if ( command != NULL )
	{
		m_command = new char[strlen( command ) + 1];
		strcpy( m_command, command );
	}
}

void CCareerTextEntryBox::SetCallback( void ( *callback )( vgui2::TextEntry* ) )
{
	m_callback = callback;
}

vgui2::TextEntry* CCareerTextEntryBox::GetTextEntry()
{
	return m_pEntry;
}

void CCareerTextEntryBox::OnCommand( const char* command )
{
	if ( !_stricmp( command, "Ok" ) )
	{
		if ( m_callback != NULL )
			m_callback( m_pEntry );
		command = m_command != NULL ? m_command : "Ok";
	}
	CCareerBaseBox::OnCommand( command );
}

void CCareerTextEntryBox::OnKeyCodeTyped( vgui2::KeyCode code )
{
	if ( code == vgui2::KEY_ESCAPE )
	{
		OnCommand( "Cancel" );
		if ( g_pBaseUI != NULL )
			g_pBaseUI->HideGameUI();
	}
	else if ( code == vgui2::KEY_ENTER )
		OnCommand( "Ok" );
	else
		vgui2::Frame::OnKeyCodeTyped( code );
}

class EquipmentComboBox : public vgui2::ComboBox
{
public:
	EquipmentComboBox( CBaseSelectBox* box, const char* name, int lines, bool editable )
		: vgui2::ComboBox( box, name, lines, editable ), m_pBox( box ) {}
	void OnSetText( const wchar_t* text ) override
	{
		vgui2::ComboBox::OnSetText( text );
		m_pBox->OnControlChanged();
	}
private:
	CBaseSelectBox* m_pBox;
};

CGrenadeSelectBox::CGrenadeSelectBox( vgui2::Panel* parent, WeaponSet* weaponSet )
	: CBaseSelectBox( parent, "BuyBoxSelectGrenades", false )
{
	m_pWeaponSet = weaponSet;
	m_pHEGrenade = new EquipmentComboBox( this, "hegrenade", 2, false );
	m_pSmokeGrenade = new EquipmentComboBox( this, "smokegrenade", 2, false );
	m_pFlashbangs = new EquipmentComboBox( this, "flashbangs", 3, false );
	m_pHEGrenadeImage = new EquipmentLabel( this, "HEGrenadeImage", NULL );
	m_pSmokeGrenadeImage = new EquipmentLabel( this, "SmokeGrenadeImage", NULL );
	m_pFlashbangImage = new EquipmentLabel( this, "FlashbangImage", NULL );
	m_pHELabel = new vgui2::Label( this, "HECost", "" );
	m_pSmokeLabel = new vgui2::Label( this, "SmokeCost", "" );
	m_pFlashLabel = new vgui2::Label( this, "FlashCost", "" );
	LoadControlSettings( "Resource/UI/BuyBoxWizardGrenades.res", NULL );
	m_pHEGrenade->AddItem( "0", NULL );
	m_pHEGrenade->AddItem( "1", NULL );
	m_pSmokeGrenade->AddItem( "0", NULL );
	m_pSmokeGrenade->AddItem( "1", NULL );
	m_pFlashbangs->AddItem( "0", NULL );
	m_pFlashbangs->AddItem( "1", NULL );
	m_pFlashbangs->AddItem( "2", NULL );
	m_pHEGrenade->ActivateItem( m_pWeaponSet->m_HEGrenade );
	m_pSmokeGrenade->ActivateItem( m_pWeaponSet->m_smokeGrenade );
	m_pFlashbangs->ActivateItem( m_pWeaponSet->m_flashbangs );
	m_pHEGrenadeImage->SetItem( "gfx/vgui/buy_presets/hegrenade", 1 );
	m_pSmokeGrenadeImage->SetItem( "gfx/vgui/buy_presets/smokegrenade", 1 );
	m_pFlashbangImage->SetItem( "gfx/vgui/buy_presets/flashbang", 1 );
	OnControlChanged();
}

void CGrenadeSelectBox::OnControlChanged()
{
	wchar_t buffer[256];
	int count = m_pHEGrenade->GetActiveItem();
	vgui2::localize()->ConstructString( buffer, sizeof( buffer ), SharedFindString( (char*)"#Cstrike_BuyPresetPlainCost" ), 1, NumAsWString( 300 * count ) );
	m_pHELabel->SetText( buffer );
	count = m_pSmokeGrenade->GetActiveItem();
	vgui2::localize()->ConstructString( buffer, sizeof( buffer ), SharedFindString( (char*)"#Cstrike_BuyPresetPlainCost" ), 1, NumAsWString( 300 * count ) );
	m_pSmokeLabel->SetText( buffer );
	count = m_pFlashbangs->GetActiveItem();
	vgui2::localize()->ConstructString( buffer, sizeof( buffer ), SharedFindString( (char*)"#Cstrike_BuyPresetPlainCost" ), 1, NumAsWString( 200 * count ) );
	m_pFlashLabel->SetText( buffer );
}

void CGrenadeSelectBox::OnCommand( const char* command )
{
	if ( !_stricmp( command, "popup_ok" ) )
	{
		m_pWeaponSet->m_HEGrenade = m_pHEGrenade->GetActiveItem() != 0;
		m_pWeaponSet->m_smokeGrenade = m_pSmokeGrenade->GetActiveItem() != 0;
		m_pWeaponSet->m_flashbangs = m_pFlashbangs->GetActiveItem();
	}
	CCareerBaseBox::OnCommand( command );
}

CEquipmentSelectBox::CEquipmentSelectBox( vgui2::Panel* parent, WeaponSet* weaponSet )
	: CBaseSelectBox( parent, "BuyBoxSelectEquipment", false )
{
	m_pWeaponSet = weaponSet;
	m_pKevlar = new EquipmentComboBox( this, "kevlar", 2, false );
	m_pHelmet = new EquipmentComboBox( this, "helmet", 2, false );
	m_pDefuser = new EquipmentComboBox( this, "defuser", 2, false );
	m_pNightvision = new EquipmentComboBox( this, "nightvision", 2, false );
	m_pKevlarLabel = new vgui2::Label( this, "kevlarCost", "" );
	m_pHelmetLabel = new vgui2::Label( this, "helmetCost", "" );
	m_pDefuserLabel = new vgui2::Label( this, "defuserCost", "" );
	m_pNightvisionLabel = new vgui2::Label( this, "nightvisionCost", "" );
	m_pKevlarImage = new EquipmentLabel( this, "KevlarImage", NULL );
	m_pHelmetImage = new EquipmentLabel( this, "HelmetImage", NULL );
	m_pDefuserImage = new EquipmentLabel( this, "DefuserImage", NULL );
	m_pNightvisionImage = new EquipmentLabel( this, "NightvisionImage", NULL );
	LoadControlSettings( "Resource/UI/BuyBoxWizardEquipment.res", NULL );
	m_pKevlar->AddItem( "0", NULL );
	m_pKevlar->AddItem( "1", NULL );
	m_pHelmet->AddItem( "0", NULL );
	m_pHelmet->AddItem( "1", NULL );
	m_pDefuser->AddItem( "0", NULL );
	m_pDefuser->AddItem( "1", NULL );
	m_pNightvision->AddItem( "0", NULL );
	m_pNightvision->AddItem( "1", NULL );
	m_pKevlar->ActivateItem( m_pWeaponSet->m_armor > 0 );
	m_pHelmet->ActivateItem( m_pWeaponSet->m_armor != 0 && m_pWeaponSet->m_helmet );
	m_pDefuser->ActivateItem( m_pWeaponSet->m_defuser );
	m_pNightvision->ActivateItem( m_pWeaponSet->m_nightvision );
	m_pKevlarImage->SetItem( "gfx/vgui/buy_presets/kevlar", 1 );
	m_pHelmetImage->SetItem( "gfx/vgui/buy_presets/helmet", 1 );
	m_pDefuserImage->SetItem( "gfx/vgui/buy_presets/defuser", 1 );
	m_pNightvisionImage->SetItem( "gfx/vgui/buy_presets/nightvision", 1 );
	OnControlChanged();
}

void CEquipmentSelectBox::OnControlChanged()
{
	wchar_t buffer[256];
	int count = m_pKevlar->GetActiveItem();
	vgui2::localize()->ConstructString( buffer, sizeof( buffer ), SharedFindString( (char*)"#Cstrike_BuyPresetPlainCost" ), 1, NumAsWString( 650 * count ) );
	m_pKevlarLabel->SetText( buffer );
	m_pHelmet->SetEnabled( count != 0 );
	if ( count == 0 && m_pHelmet->GetActiveItem() != 0 )
		m_pHelmet->ActivateItem( 0 );
	count = m_pHelmet->GetActiveItem();
	vgui2::localize()->ConstructString( buffer, sizeof( buffer ), SharedFindString( (char*)"#Cstrike_BuyPresetPlainCost" ), 1, NumAsWString( 350 * count ) );
	m_pHelmetLabel->SetText( buffer );
	count = m_pDefuser->GetActiveItem();
	vgui2::localize()->ConstructString( buffer, sizeof( buffer ), SharedFindString( (char*)"#Cstrike_BuyPresetPlainCost" ), 1, NumAsWString( 200 * count ) );
	m_pDefuserLabel->SetText( buffer );
	count = m_pNightvision->GetActiveItem();
	vgui2::localize()->ConstructString( buffer, sizeof( buffer ), SharedFindString( (char*)"#Cstrike_BuyPresetPlainCost" ), 1, NumAsWString( 1250 * count ) );
	m_pNightvisionLabel->SetText( buffer );
}

void CEquipmentSelectBox::OnCommand( const char* command )
{
	if ( !_stricmp( command, "popup_ok" ) )
	{
		m_pWeaponSet->m_armor = m_pKevlar->GetActiveItem() != 0 ? 100 : 0;
		m_pWeaponSet->m_helmet = m_pWeaponSet->m_armor != 0 && m_pHelmet->GetActiveItem() != 0;
		m_pWeaponSet->m_defuser = m_pDefuser->GetActiveItem() != 0;
		m_pWeaponSet->m_nightvision = m_pNightvision->GetActiveItem() != 0;
	}
	CCareerBaseBox::OnCommand( command );
}

static WeaponIdType s_secondaryWeapons[] =
{
	WEAPON_NONE, WEAPON_USP, WEAPON_GLOCK18, WEAPON_DEAGLE,
	WEAPON_ELITE, WEAPON_P228, WEAPON_FIVESEVEN
};

static WeaponIdType s_primaryWeapons[] =
{
	WEAPON_NONE, (WeaponIdType)-7, WEAPON_SG552, WEAPON_AUG, WEAPON_AK47,
	WEAPON_M4A1, WEAPON_GALIL, WEAPON_FAMAS, (WeaponIdType)-8, WEAPON_AWP,
	WEAPON_SG550, WEAPON_G3SG1, WEAPON_SCOUT, (WeaponIdType)-4, WEAPON_P90,
	WEAPON_UMP45, WEAPON_MP5N, WEAPON_MAC10, WEAPON_TMP, (WeaponIdType)-5,
	WEAPON_M249, WEAPON_XM1014, WEAPON_M3, WEAPON_SHIELDGUN
};

class CWeaponButton : public vgui2::Button
{
public:
	CWeaponButton( BuyPresetListBox* list, WeaponIdType weapon, bool primary );
	~CWeaponButton() override;
	void SetWeapon( const BuyPresetWeapon& weapon );
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	Color GetBgColor() override;
	Color GetFgColor() override;
	void PerformLayout() override;
	void Paint() override;
	void OnMousePressed( vgui2::MouseCode code ) override;
	void OnMouseDoublePressed( vgui2::MouseCode code ) override;
	void SetCurrent() { s_current = this; }
	bool IsCurrent() const { return this == s_current; }
	const BuyPresetWeapon& GetWeapon() { return m_weapon; }
protected:
	BuyPresetListBox* m_pListBox;
	BuyPresetWeapon m_weapon;
	vgui2::IImage* m_pImage;
	vgui2::TextImage* m_pTitleImage;
	vgui2::TextImage* m_pCostImage;
	int m_imageWide;
	int m_imageTall;
	int m_imageX;
	int m_imageY;
	Color m_selectedBgColor;
	Color m_selectedFgColor;
	bool m_isPrimary;
	static CWeaponButton* s_current;
};

CWeaponButton* CWeaponButton::s_current;

CWeaponButton::CWeaponButton( BuyPresetListBox* list, WeaponIdType weapon, bool primary )
	: vgui2::Button( list->GetParent(), SharedVarArgs( (char*)"WeaponButton%d", weapon ), "" )
{
	m_isPrimary = primary;
	m_pImage = NULL;
	m_pTitleImage = new vgui2::TextImage( WeaponIDToDisplayName( weapon ) );
	m_pCostImage = new vgui2::TextImage( L"" );
	m_pListBox = list;
	BuyPresetWeapon preset( weapon );
	SetWeapon( preset );
}

CWeaponButton::~CWeaponButton()
{
	delete m_pTitleImage;
	delete m_pCostImage;
}

void CWeaponButton::SetWeapon( const BuyPresetWeapon& weapon )
{
	m_weapon = weapon;
	if ( m_weapon.GetName() != NULL )
	{
		m_pTitleImage->SetText( m_weapon.GetName() );
		const WeaponInfoStruct* info = GetWeaponInfo( m_weapon.GetWeaponID() );
		if ( info != NULL )
		{
			wchar_t buffer[256];
			vgui2::localize()->ConstructString( buffer, sizeof( buffer ), SharedFindString( (char*)"#Cstrike_BuyPresetPlainCost" ), 1, NumAsWString( info->cost ) );
			m_pCostImage->SetText( buffer );
		}
		else
			m_pCostImage->SetText( L"" );
	}
	else
	{
		m_pTitleImage->SetText( L"" );
		m_pCostImage->SetText( L"" );
	}
	m_pTitleImage->ResizeImageToContent();
	m_pCostImage->ResizeImageToContent();
	InvalidateLayout( true, false );
	m_pImage = vgui2::scheme()->GetImage( ImageFnameFromWeaponID( m_weapon.GetWeaponID(), m_isPrimary ), true );
}

void CWeaponButton::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	CWeaponButton* current = s_current;
	s_current = NULL;
	vgui2::Button::ApplySchemeSettings( scheme );
	m_selectedBgColor = GetSchemeColor( "ButtonArmedBgColor", vgui2::Panel::GetBgColor(), scheme );
	m_selectedFgColor = GetSchemeColor( "ButtonArmedFgColor", vgui2::Label::GetFgColor(), scheme );
	s_current = current;
	m_pTitleImage->SetColor( scheme->GetColor( "BaseText", Color( 0, 0, 0, 0 ) ) );
	m_pTitleImage->SetFont( scheme->GetFont( "Default", IsProportional() ) );
	m_pTitleImage->SetWrap( true );
	m_pCostImage->SetColor( scheme->GetColor( "BaseText", Color( 0, 0, 0, 0 ) ) );
	m_pCostImage->SetFont( scheme->GetFont( "Default", IsProportional() ) );
	m_pCostImage->SetWrap( true );
	SetWeapon( m_weapon );
}

Color CWeaponButton::GetBgColor()
{
	return this == s_current ? m_selectedBgColor : vgui2::Panel::GetBgColor();
}

Color CWeaponButton::GetFgColor()
{
	return this == s_current ? m_selectedFgColor : vgui2::Label::GetFgColor();
}

void CWeaponButton::PerformLayout()
{
	vgui2::Label::PerformLayout();
	int wide, oldTall;
	GetSize( wide, oldTall );
	int maxTextWide = (int)( 0.45f * wide );
	int maxImageWide = (int)( 0.42f * wide );
	int maxCostWide = (int)( 0.13000003f * wide );
	int tall = 2 * vgui2::surface()->GetFontTall( m_pTitleImage->GetFont() );
	if ( tall != oldTall )
	{
		oldTall = tall;
		SetSize( wide, tall );
		m_pListBox->InvalidateLayout( false, false );
	}
	m_pTitleImage->SetSize( maxTextWide, oldTall );
	m_pTitleImage->RecalculateNewLinePositions();
	m_pTitleImage->ResizeImageToContent();
	int contentWide, contentTall;
	m_pTitleImage->GetSize( contentWide, contentTall );
	if ( tall > contentTall )
	{
		m_pTitleImage->SetSize( maxTextWide, contentTall );
		m_pTitleImage->SetPos( maxImageWide, ( tall - contentTall ) / 2 );
	}
	else
		m_pTitleImage->SetPos( maxImageWide, 0 );
	m_pCostImage->SetSize( maxCostWide, oldTall );
	m_pCostImage->RecalculateNewLinePositions();
	m_pCostImage->ResizeImageToContent();
	m_pCostImage->GetSize( contentWide, contentTall );
	if ( tall > contentTall )
	{
		m_pCostImage->SetSize( maxCostWide, contentTall );
		m_pCostImage->SetPos( wide - maxCostWide, ( tall - contentTall ) / 2 );
	}
	else
		m_pCostImage->SetPos( wide - maxCostWide, 0 );
	m_pImage = vgui2::scheme()->GetImage( ImageFnameFromWeaponID( m_weapon.GetWeaponID(), m_isPrimary ), true );
	m_pImage->GetContentSize( m_imageWide, m_imageTall );
	if ( tall < m_imageTall )
	{
		m_imageWide = (int)( (float)m_imageWide * tall / m_imageTall );
		m_imageTall = tall;
	}
	if ( maxImageWide < m_imageWide )
	{
		m_imageTall = (int)( (float)m_imageTall * maxImageWide / m_imageWide );
		m_imageWide = maxImageWide;
	}
	m_imageY = ( tall - m_imageTall ) / 2;
	m_imageX = (int)( ( 0.42f * wide - m_imageWide ) * 0.5 );
}

void CWeaponButton::Paint()
{
	if ( m_pImage != NULL )
	{
		m_pImage->SetSize( m_imageWide, m_imageTall );
		m_pImage->SetPos( m_imageX, m_imageY );
		m_pImage->Paint();
		m_pImage->SetSize( 0, 0 );
	}
	m_pTitleImage->Paint();
	m_pCostImage->Paint();
	vgui2::Label::Paint();
}

void CWeaponButton::OnMousePressed( vgui2::MouseCode code )
{
	if ( code == vgui2::MOUSE_LEFT )
	{
		SetCurrent();
		m_pListBox->GetParent()->OnCommand( "select_weapon" );
	}
	vgui2::Button::OnMousePressed( code );
}

void CWeaponButton::OnMouseDoublePressed( vgui2::MouseCode code )
{
	if ( code == vgui2::MOUSE_LEFT )
	{
		SetCurrent();
		m_pListBox->GetParent()->OnCommand( "select_weapon" );
		m_pListBox->GetParent()->OnCommand( "popup_ok" );
	}
	vgui2::Button::OnMouseDoublePressed( code );
}

class WeaponComboBox : public vgui2::ComboBox
{
public:
	WeaponComboBox( CWeaponSelectBox* box, const char* name, int lines, bool editable )
		: vgui2::ComboBox( box, name, lines, editable ), m_pBox( box ) {}
	void OnSetText( const wchar_t* text ) override
	{
		vgui2::ComboBox::OnSetText( text );
		if ( m_pBox != NULL )
			m_pBox->UpdateClips();
	}
private:
	CWeaponSelectBox* m_pBox;
};

#pragma optimize( "", off )
CWeaponSelectBox::CWeaponSelectBox( vgui2::Panel* parent, WeaponSet* weaponSet, bool secondary )
	: CCareerBaseBox( parent, "BuyBoxSelectWeapon", false )
{
	m_pWeaponSet = weaponSet;
	m_isSecondary = secondary;
	m_numWeapons = 24;
	m_weaponIDs = s_primaryWeapons;

	BuyPresetWeapon* weapon;
	if ( !m_isSecondary )
	{
		weapon = &m_pWeaponSet->m_primaryWeapon;
		if ( !IsPrimaryWeapon( weapon->GetWeaponID() ) && weapon->GetWeaponID() != WEAPON_NONE )
			m_pWeaponSet->m_primaryWeapon = BuyPresetWeapon( WEAPON_NONE );
	}
	else
	{
		m_numWeapons = 7;
		m_weaponIDs = s_secondaryWeapons;
		weapon = &m_pWeaponSet->m_secondaryWeapon;
		if ( !IsSecondaryWeapon( weapon->GetWeaponID() ) && weapon->GetWeaponID() != WEAPON_NONE )
			*weapon = BuyPresetWeapon( WEAPON_NONE );
	}

	int numClips = 0;
	const WeaponInfoStruct* info = GetWeaponInfo( weapon->GetWeaponID() );
	if ( info != NULL )
		numClips = (int)( ( info->buyClipSize > 0 ) ? ceil( (float)info->maxRounds / (float)info->buyClipSize ) : 0 );
	else
		numClips = 4;

	m_pClips = new WeaponComboBox( this, "Clips", 2 * numClips + 1, false );
	m_pClips->SetOpenDirection( vgui2::ComboBox::UP );
	m_pListBox = new BuyPresetListBox( this, "WeaponListBox" );
	m_pBullets = new vgui2::Label( this, "bullets", "" );

	int selected = -1;
	for ( int i = 0; i < m_numWeapons; ++i )
	{
		if ( (int)m_weaponIDs[i] < 0 )
		{
			const char* text = "";
			switch ( (int)m_weaponIDs[i] )
			{
			case -7:
				text = "#Cstrike_BuyPresetCategoryRifle";
				break;
			case -8:
				text = "#Cstrike_BuyPresetCategorySniper";
				break;
			case -4:
				text = "#Cstrike_BuyPresetCategorySMG";
				break;
			case -5:
				text = "#Cstrike_BuyPresetCategoryHeavy";
				break;
			}
			vgui2::Label* label = new vgui2::Label( m_pListBox, SharedVarArgs( (char*)"weaponlabel%d", i ), text );
			m_pListBox->AddItem( label, NULL );
		}
		else
		{
			CWeaponButton* button = new CWeaponButton( m_pListBox, m_weaponIDs[i], !m_isSecondary );
			m_pListBox->AddItem( button, NULL );
			if ( m_weaponIDs[i] == weapon->GetWeaponID() )
			{
				button->SetCurrent();
				selected = i;
			}
		}
	}
	m_pListBox->MakeItemVisible( selected );
	LoadControlSettings( "resource/UI/BuyBoxSelectWeapon.res", NULL );
	PopulateControls();
}

CWeaponSelectBox::~CWeaponSelectBox()
{
}

void CWeaponSelectBox::ActivateBuildMode()
{
	SetClipsVisible( true );
	m_pListBox->DeleteAllItems();
	CCareerBaseBox::ActivateBuildMode();
}

void CWeaponSelectBox::SetClipsVisible( bool visible )
{
	SetLabelVisible( visible );
	m_pClips->SetVisible( visible );
}

void CWeaponSelectBox::PopulateControls()
{
	BuyPresetWeapon* weapon;
	if ( !m_isSecondary )
		weapon = &m_pWeaponSet->m_primaryWeapon;
	else
		weapon = &m_pWeaponSet->m_secondaryWeapon;

	int numClips = 0;
	const WeaponInfoStruct* info = GetWeaponInfo( weapon->GetWeaponID() );
	if ( info != NULL )
		numClips = (int)( ( info->buyClipSize > 0 ) ? ceil( (float)info->maxRounds / (float)info->buyClipSize ) : 0 );
	else
		numClips = 4;

	m_pClips->SetNumberOfEditLines( 2 * numClips + 1 );
	SetClipsVisible( numClips != 0 );
	m_pClips->DeleteAllItems();
	m_pClips->AddItem( "#Cstrike_BuyPresetEditWeaponFullClips", NULL );

	const int BufLen = 64;
	wchar_t wbuf[BufLen];
	for ( int i = numClips - 1; i >= 0; --i )
	{
		const char* orMoreFormat = "#Cstrike_BuyPresetEditClipsOrMore";
		const char* format = "#Cstrike_BuyPresetEditClips";
		if ( i == 1 )
		{
			orMoreFormat = "#Cstrike_BuyPresetEditClipOrMore";
			format = "#Cstrike_BuyPresetEditClip";
		}
		vgui2::localize()->ConstructString( wbuf, sizeof( wbuf ), SharedFindString( (char*)orMoreFormat ), 1, NumAsWString( i ) );
		m_pClips->AddItem( wbuf, NULL );
		vgui2::localize()->ConstructString( wbuf, sizeof( wbuf ), SharedFindString( (char*)format ), 1, NumAsWString( i ) );
		m_pClips->AddItem( wbuf, NULL );
	}

	int activeItem = weapon->GetFillAmmo();
	activeItem += 2 * weapon->GetAmmoAmount();
	activeItem = 2 * numClips - activeItem;
	m_pClips->ActivateItem( activeItem );

	if ( m_isSecondary )
	{
		vgui2::Panel* title = FindChildByName( "TitleLabel", false );
		if ( title != NULL )
		{
			const wchar_t* text = vgui2::localize()->Find( "#Cstrike_BuyPresetWizardSecondary" );
			if ( text != NULL )
				PostMessage( title, new KeyValues( "SetText", "text", text ), 0 );
		}
	}
	UpdateClips();
}
#pragma optimize( "", on )

void CWeaponSelectBox::UpdateClips()
{
	if ( m_pClips == NULL )
		return;
	int clips = ( m_pClips->GetItemCount() - m_pClips->GetActiveItem() + 1 ) / 2 - 1;
	BuyPresetWeapon* weapon = m_isSecondary ? &m_pWeaponSet->m_secondaryWeapon : &m_pWeaponSet->m_primaryWeapon;
	const WeaponInfoStruct* info = GetWeaponInfo( weapon->GetWeaponID() );
	if ( info == NULL )
	{
		m_pBullets->SetVisible( false );
		return;
	}
	m_pBullets->SetVisible( true );
	wchar_t buffer[64];
	const wchar_t* maxRounds = NumAsWString( info->maxRounds );
	int rounds = clips * info->buyClipSize;
	if ( rounds > info->maxRounds )
		rounds = info->maxRounds;
	const wchar_t* selectedRounds = NumAsWString( rounds );
	vgui2::localize()->ConstructString( buffer, sizeof( buffer ), vgui2::localize()->Find( "#Cstrike_BuyPresetsBullets" ), 2, selectedRounds, maxRounds );
	m_pBullets->SetText( buffer );
}

void CWeaponSelectBox::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	CCareerBaseBox::ApplySchemeSettings( scheme );
	vgui2::HFont font = scheme->GetFont( "Default", IsProportional() );
	int fontTall = vgui2::surface()->GetFontTall( font );
	if ( font != 0 )
		m_pClips->GetMenu()->SetMenuItemHeight( fontTall + 2 );
	for ( int i = 0; i < m_pListBox->GetItemCount(); ++i )
	{
		vgui2::Panel* item = m_pListBox->GetItemPanel( i );
		if ( (int)m_weaponIDs[i] < 0 )
		{
			if ( item != NULL )
			{
				vgui2::Label* label = (vgui2::Label*)item;
				label->SetContentAlignment( vgui2::Label::a_center );
				label->SetBorder( scheme->GetBorder( "ButtonBorder" ) );
				label->SetBgColor( scheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 255 ) ) );
			}
		}
		else if ( item != NULL && ( (CWeaponButton*)item )->IsCurrent() )
			m_pListBox->MakeItemVisible( i );
	}
}

WeaponIdType CWeaponSelectBox::GetSelectedWeaponID()
{
	int selected = -1;
	for ( int i = 0; i < m_pListBox->GetItemCount(); ++i )
	{
		if ( selected < 0 || (int)m_weaponIDs[selected] >= 0 )
		{
			vgui2::Panel* item = m_pListBox->GetItemPanel( i );
			if ( item != NULL && ( (CWeaponButton*)item )->IsCurrent() )
				selected = i;
		}
	}
	return selected < 0 ? WEAPON_NONE : m_weaponIDs[selected];
}

void CWeaponSelectBox::OnCommand( const char* command )
{
	if ( !_stricmp( command, "select_weapon" ) )
	{
		BuyPresetWeapon weapon( GetSelectedWeaponID() );
		if ( m_isSecondary )
			m_pWeaponSet->m_secondaryWeapon = weapon;
		else
			m_pWeaponSet->m_primaryWeapon = weapon;
		PopulateControls();
		return;
	}
	if ( !_stricmp( command, "popup_ok" ) )
	{
		if ( m_pClips == NULL )
			return;
		int choice = m_pClips->GetItemCount() - m_pClips->GetActiveItem() + 1;
		BuyPresetWeapon weapon( GetSelectedWeaponID() );
		weapon.SetAmmoType( AMMO_CLIPS );
		weapon.SetAmmoAmount( choice / 2 - 1 );
		weapon.SetFillAmmo( ( choice & 1 ) != 0 );
		if ( m_isSecondary )
			m_pWeaponSet->m_secondaryWeapon = weapon;
		else
			m_pWeaponSet->m_primaryWeapon = weapon;
	}
	CCareerBaseBox::OnCommand( command );
}
