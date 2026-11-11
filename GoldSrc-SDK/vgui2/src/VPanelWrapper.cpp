//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "VPanelWrapper.h"

#include "VPanel.h"
#include "vgui_internal.h"
#include "platform.h"
#include "tier1/interface.h"
#include "vgui/IClientPanel.h"
#include "vgui/ISurface.h"

#include <stddef.h>

void VPanelWrapper::Init( vgui2::VPANEL vguiPanel, vgui2::IClientPanel* panel )
{
	( (vgui2::VPanel*)vguiPanel )->Init( panel );
}

void VPanelWrapper::SetPos( vgui2::VPANEL vguiPanel, int x, int y )
{
	( (vgui2::VPanel*)vguiPanel )->SetPos( x, y );
}

void VPanelWrapper::GetPos( vgui2::VPANEL vguiPanel, int& x, int& y )
{
	( (vgui2::VPanel*)vguiPanel )->GetPos( x, y );
}

void VPanelWrapper::SetSize( vgui2::VPANEL vguiPanel, int wide, int tall )
{
	( (vgui2::VPanel*)vguiPanel )->SetSize( wide, tall );
}

void VPanelWrapper::GetSize( vgui2::VPANEL vguiPanel, int& wide, int& tall )
{
	( (vgui2::VPanel*)vguiPanel )->GetSize( wide, tall );
}

void VPanelWrapper::SetMinimumSize( vgui2::VPANEL vguiPanel, int wide, int tall )
{
	( (vgui2::VPanel*)vguiPanel )->SetMinimumSize( wide, tall );
}

void VPanelWrapper::GetMinimumSize( vgui2::VPANEL vguiPanel, int& wide, int& tall )
{
	( (vgui2::VPanel*)vguiPanel )->GetMinimumSize( wide, tall );
}

void VPanelWrapper::SetZPos( vgui2::VPANEL vguiPanel, int z )
{
	( (vgui2::VPanel*)vguiPanel )->SetZPos( z );
}

int VPanelWrapper::GetZPos( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->GetZPos();
}

void VPanelWrapper::GetAbsPos( vgui2::VPANEL vguiPanel, int& x, int& y )
{
	( (vgui2::VPanel*)vguiPanel )->GetAbsPos( x, y );
}

void VPanelWrapper::GetClipRect( vgui2::VPANEL vguiPanel, int& x0, int& y0, int& x1, int& y1 )
{
	( (vgui2::VPanel*)vguiPanel )->GetClipRect( x0, y0, x1, y1 );
}

void VPanelWrapper::SetInset( vgui2::VPANEL vguiPanel, int left, int top, int right, int bottom )
{
	( (vgui2::VPanel*)vguiPanel )->SetInset( left, top, right, bottom );
}

void VPanelWrapper::GetInset( vgui2::VPANEL vguiPanel, int& left, int& top, int& right, int& bottom )
{
	( (vgui2::VPanel*)vguiPanel )->GetInset( left, top, right, bottom );
}

void VPanelWrapper::SetVisible( vgui2::VPANEL vguiPanel, bool state )
{
	( (vgui2::VPanel*)vguiPanel )->SetVisible( state );
}

void VPanelWrapper::SetEnabled( vgui2::VPANEL vguiPanel, bool state )
{
	( (vgui2::VPanel*)vguiPanel )->SetEnabled( state );
}

bool VPanelWrapper::IsVisible( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->IsVisible();
}

bool VPanelWrapper::IsEnabled( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->IsEnabled();
}

void VPanelWrapper::SetParent( vgui2::VPANEL vguiPanel, vgui2::VPANEL newParent )
{
	( (vgui2::VPanel*)vguiPanel )->SetParent( (vgui2::VPanel*)newParent );
}

int VPanelWrapper::GetChildCount( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->GetChildCount();
}

vgui2::VPANEL VPanelWrapper::GetChild( vgui2::VPANEL vguiPanel, int index )
{
	return (vgui2::VPANEL)( (vgui2::VPanel*)vguiPanel )->GetChild( index );
}

vgui2::VPANEL VPanelWrapper::GetParent( vgui2::VPANEL vguiPanel )
{
	return (vgui2::VPANEL)( (vgui2::VPanel*)vguiPanel )->GetParent();
}

void VPanelWrapper::MoveToFront( vgui2::VPANEL vguiPanel )
{
	( (vgui2::VPanel*)vguiPanel )->MoveToFront();
}

void VPanelWrapper::MoveToBack( vgui2::VPANEL vguiPanel )
{
	( (vgui2::VPanel*)vguiPanel )->MoveToBack();
}

bool VPanelWrapper::IsPopup( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->IsPopup();
}

void VPanelWrapper::SetPopup( vgui2::VPANEL vguiPanel, bool state )
{
	( (vgui2::VPanel*)vguiPanel )->SetPopup( state );
}

bool VPanelWrapper::Render_GetPopupVisible( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->Render_IsPopupPanelVisible();
}

void VPanelWrapper::Render_SetPopupVisible( vgui2::VPANEL vguiPanel, bool state )
{
	( (vgui2::VPanel*)vguiPanel )->Render_SetPopupVisible( state );
}

void VPanelWrapper::Solve( vgui2::VPANEL vguiPanel )
{
	( (vgui2::VPanel*)vguiPanel )->Solve();
}

vgui2::SurfacePlat* VPanelWrapper::Plat( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->Plat();
}

static VPanelWrapper s_VPanelWrapper;

static void* CreateVPanelWrapperInterface()
{
	return (vgui2::IPanel*)&s_VPanelWrapper;
}

static InterfaceReg s_VPanelWrapperReg( CreateVPanelWrapperInterface, "VGUI_Panel007" );

vgui2::IClientPanel* VPanelWrapper::Client( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->Client();
}

bool VPanelWrapper::HasParent( vgui2::VPANEL vguiPanel, vgui2::VPANEL potentialParent )
{
	if ( !vguiPanel )
	{
		return false;
	}

	return ( (vgui2::VPanel*)vguiPanel )->HasParent( (vgui2::VPanel*)potentialParent );
}

void VPanelWrapper::SetPlat( vgui2::VPANEL vguiPanel, vgui2::SurfacePlat* Plat )
{
	( (vgui2::VPanel*)vguiPanel )->SetPlat( Plat );
}

const char* VPanelWrapper::GetName( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->GetName();
}

const char* VPanelWrapper::GetClassName( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->GetClassName();
}

vgui2::HScheme VPanelWrapper::GetScheme( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->GetScheme();
}

bool VPanelWrapper::IsProportional( vgui2::VPANEL vguiPanel )
{
	return Client( vguiPanel )->IsProportional();
}

bool VPanelWrapper::IsAutoDeleteSet( vgui2::VPANEL vguiPanel )
{
	return Client( vguiPanel )->IsAutoDeleteSet();
}

void VPanelWrapper::DeletePanel( vgui2::VPANEL vguiPanel )
{
	Client( vguiPanel )->DeletePanel();
}

void VPanelWrapper::SendMessage( vgui2::VPANEL vguiPanel, KeyValues* params, vgui2::VPANEL ifrompanel )
{
	( (vgui2::VPanel*)vguiPanel )->SendMessage( params, ifrompanel );
}

void VPanelWrapper::Think( vgui2::VPANEL vguiPanel )
{
	Client( vguiPanel )->Think();
}

void VPanelWrapper::PerformApplySchemeSettings( vgui2::VPANEL vguiPanel )
{
	Client( vguiPanel )->PerformApplySchemeSettings();
}

void VPanelWrapper::PaintTraverse( vgui2::VPANEL vguiPanel, bool forceRepaint, bool allowForce )
{
	Client( vguiPanel )->PaintTraverse( forceRepaint, allowForce );
}

void VPanelWrapper::Repaint( vgui2::VPANEL vguiPanel )
{
	Client( vguiPanel )->Repaint();
}

vgui2::VPANEL VPanelWrapper::IsWithinTraverse( vgui2::VPANEL vguiPanel, int x, int y, bool traversePopups )
{
	return Client( vguiPanel )->IsWithinTraverse( x, y, traversePopups );
}

void VPanelWrapper::OnChildAdded( vgui2::VPANEL vguiPanel, vgui2::VPANEL child )
{
	Client( vguiPanel )->OnChildAdded( child );
}

void VPanelWrapper::OnSizeChanged( vgui2::VPANEL vguiPanel, int newWide, int newTall )
{
	Client( vguiPanel )->OnSizeChanged( newWide, newTall );
}

void VPanelWrapper::InternalFocusChanged( vgui2::VPANEL vguiPanel, bool lost )
{
	Client( vguiPanel )->InternalFocusChanged( lost );
}

bool VPanelWrapper::RequestInfo( vgui2::VPANEL vguiPanel, KeyValues* outputData )
{
	return Client( vguiPanel )->RequestInfo( outputData );
}

void VPanelWrapper::RequestFocus( vgui2::VPANEL vguiPanel, int direction )
{
	Client( vguiPanel )->RequestFocus( direction );
}

bool VPanelWrapper::RequestFocusPrev( vgui2::VPANEL vguiPanel, vgui2::VPANEL existingPanel )
{
	return Client( vguiPanel )->RequestFocusPrev( existingPanel );
}

bool VPanelWrapper::RequestFocusNext( vgui2::VPANEL vguiPanel, vgui2::VPANEL existingPanel )
{
	return Client( vguiPanel )->RequestFocusNext( existingPanel );
}

vgui2::VPANEL VPanelWrapper::GetCurrentKeyFocus( vgui2::VPANEL vguiPanel )
{
	return Client( vguiPanel )->GetCurrentKeyFocus();
}

int VPanelWrapper::GetTabPosition( vgui2::VPANEL vguiPanel )
{
	return Client( vguiPanel )->GetTabPosition();
}

vgui2::Panel* VPanelWrapper::GetPanel( vgui2::VPANEL vguiPanel, const char* moduleName )
{
	if ( vgui2::g_pSurface->GetEmbeddedPanel() == vguiPanel )
	{
		return NULL;
	}

	if ( strcasecmp( GetModuleName( vguiPanel ), moduleName ) )
	{
		return NULL;
	}

	return Client( vguiPanel )->GetPanel();
}

const char* VPanelWrapper::GetModuleName( vgui2::VPANEL vguiPanel )
{
	return Client( vguiPanel )->GetModuleName();
}

void VPanelWrapper::SetKeyBoardInputEnabled( vgui2::VPANEL vguiPanel, bool state )
{
	( (vgui2::VPanel*)vguiPanel )->SetKeyBoardInputEnabled( state );
}

void VPanelWrapper::SetMouseInputEnabled( vgui2::VPANEL vguiPanel, bool state )
{
	( (vgui2::VPanel*)vguiPanel )->SetMouseInputEnabled( state );
}

bool VPanelWrapper::IsMouseInputEnabled( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->IsMouseInputEnabled();
}

bool VPanelWrapper::IsKeyBoardInputEnabled( vgui2::VPANEL vguiPanel )
{
	return ( (vgui2::VPanel*)vguiPanel )->IsKeyBoardInputEnabled();
}

VPanelWrapper::~VPanelWrapper()
{
}
