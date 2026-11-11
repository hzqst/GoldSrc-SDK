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

#pragma once

#include "ISurface.h"

class KeyValues;

namespace vgui2
{
class IClientPanel;
class SurfacePlat;
//!! must be removed
class Panel;

//-----------------------------------------------------------------------------
// Purpose: interface from Client panels -> vgui panels
//-----------------------------------------------------------------------------
class IPanel : public IBaseInterface
{
public:
	virtual void Init( VPANEL panel, IClientPanel* client );

	// methods
	virtual void SetPos( VPANEL panel, int x, int y );
	virtual void GetPos( VPANEL panel, int& x, int& y );
	virtual void SetSize( VPANEL panel, int width, int height );
	virtual void GetSize( VPANEL panel, int& width, int& height );
	virtual void SetMinimumSize( VPANEL panel, int width, int height );
	virtual void GetMinimumSize( VPANEL panel, int& width, int& height );
	virtual void SetZPos( VPANEL panel, int z );
	virtual int GetZPos( VPANEL panel );
	virtual void GetAbsPos( VPANEL panel, int& x, int& y );
	virtual void GetClipRect( VPANEL panel, int& x0, int& y0, int& x1, int& y1 );
	virtual void SetInset( VPANEL panel, int left, int top, int right, int bottom );
	virtual void GetInset( VPANEL panel, int& left, int& top, int& right, int& bottom );

	virtual void SetVisible( VPANEL panel, bool state );
	virtual bool IsVisible( VPANEL panel );

	virtual void SetParent( VPANEL panel, VPANEL parent );
	virtual int GetChildCount( VPANEL panel );
	virtual VPANEL GetChild( VPANEL panel, int index );
	virtual VPANEL GetParent( VPANEL panel );
	virtual void MoveToFront( VPANEL panel );
	virtual void MoveToBack( VPANEL panel );
	virtual bool HasParent( VPANEL panel, VPANEL parent );
	virtual bool IsPopup( VPANEL panel );
	virtual void SetPopup( VPANEL panel, bool state );
	virtual bool Render_GetPopupVisible( VPANEL panel );
	virtual void Render_SetPopupVisible( VPANEL panel, bool state );

	virtual HScheme GetScheme( VPANEL panel );
	virtual bool IsProportional( VPANEL panel );
	virtual bool IsAutoDeleteSet( VPANEL panel );
	virtual void DeletePanel( VPANEL panel );

	virtual void SetKeyBoardInputEnabled( VPANEL panel, bool state );
	virtual void SetMouseInputEnabled( VPANEL panel, bool state );
	virtual bool IsKeyBoardInputEnabled( VPANEL panel );
	virtual bool IsMouseInputEnabled( VPANEL panel );

	virtual void Solve( VPANEL panel );
	virtual const char* GetName( VPANEL panel );
	virtual const char* GetClassName( VPANEL panel );

	virtual void SendMessage( VPANEL panel, KeyValues* message, VPANEL sender );
	virtual void Think( VPANEL panel );
	virtual void PerformApplySchemeSettings( VPANEL panel );
	virtual void PaintTraverse( VPANEL panel, bool forceRepaint, bool allowForce );
	virtual void Repaint( VPANEL panel );
	virtual VPANEL IsWithinTraverse( VPANEL panel, int x, int y, bool traversePopups );
	virtual void OnChildAdded( VPANEL panel, VPANEL child );
	virtual void OnSizeChanged( VPANEL panel, int width, int height );
	virtual void InternalFocusChanged( VPANEL panel, bool lost );
	virtual bool RequestInfo( VPANEL panel, KeyValues* output );
	virtual void RequestFocus( VPANEL panel, int direction );
	virtual bool RequestFocusPrev( VPANEL panel, VPANEL existing );
	virtual bool RequestFocusNext( VPANEL panel, VPANEL existing );
	virtual VPANEL GetCurrentKeyFocus( VPANEL panel );
	virtual int GetTabPosition( VPANEL panel );

	virtual SurfacePlat* Plat( VPANEL panel );
	virtual void SetPlat( VPANEL panel, SurfacePlat* platform );

	virtual Panel* GetPanel( VPANEL panel, const char* destinationModule );
	virtual bool IsEnabled( VPANEL panel );
	virtual void SetEnabled( VPANEL panel, bool state );
};

IPanel* ipanel();
} // namespace vgui
