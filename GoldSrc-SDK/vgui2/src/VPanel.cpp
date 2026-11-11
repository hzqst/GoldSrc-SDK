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

#include "VPanel.h"

#include "vgui_internal.h"

#include "vgui/IClientPanel.h"
#include "vgui/ISurface.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

using namespace vgui2;

#define INVALID_PANELLIST	((HPanelList)0xFFFF)

static void DarEnsureCapacity( Dar< VPanel* >& dar, int wantedCapacity )
{
	int newCapacity;
	VPanel** newData;
	int i;

	if ( wantedCapacity <= dar._capacity )
	{
		return;
	}

	newCapacity = dar._capacity;
	if ( newCapacity == 0 )
	{
		newCapacity = 1;
	}

	while ( newCapacity < wantedCapacity )
	{
		newCapacity *= 2;
	}

	newData = new VPanel*[newCapacity];
	if ( !newData )
	{
		exit( 0 );
		return;
	}

	memset( newData, 0, sizeof( VPanel* ) * newCapacity );
	dar._capacity = newCapacity;

	for ( i = 0; i < dar._count; i++ )
	{
		newData[i] = dar._data[i];
	}

	delete[] dar._data;
	dar._data = newData;
}

static int DarAddElement( Dar< VPanel* >& dar, VPanel* elem )
{
	DarEnsureCapacity( dar, dar._count + 1 );
	dar._data[dar._count] = elem;
	dar._count++;

	return dar._count - 1;
}

static int DarFindElement( Dar< VPanel* >& dar, VPanel* elem )
{
	int i;

	for ( i = 0; i < dar._count; i++ )
	{
		if ( dar._data[i] == elem )
		{
			return i;
		}
	}

	return -1;
}

static int DarPutElement( Dar< VPanel* >& dar, VPanel* elem )
{
	int index;

	index = DarFindElement( dar, elem );
	if ( index >= 0 )
	{
		return index;
	}

	return DarAddElement( dar, elem );
}

static void DarSetCount( Dar< VPanel* >& dar, int count )
{
	if ( count < 0 || count > dar._capacity )
	{
		return;
	}

	dar._count = count;
}

static void DarSetElementAt( Dar< VPanel* >& dar, VPanel* elem, int index )
{
	int i;

	if ( index < 0 || index >= dar._capacity )
	{
		return;
	}

	if ( index >= dar._count )
	{
		for ( i = dar._count; i < index; i++ )
		{
			dar._data[i] = NULL;
		}

		DarSetCount( dar, index + 1 );
	}

	dar._data[index] = elem;
}

static void DarRemoveElementAt( Dar< VPanel* >& dar, int index )
{
	int i;

	if ( index < 0 || index >= dar._count )
	{
		return;
	}

	for ( i = index; i < dar._count - 1; i++ )
	{
		dar._data[i] = dar._data[i + 1];
	}

	dar._count--;
}

static void DarRemoveElement( Dar< VPanel* >& dar, VPanel* elem )
{
	int i;

	for ( i = 0; i < dar._count; i++ )
	{
		if ( dar._data[i] == elem )
		{
			DarRemoveElementAt( dar, i );
			break;
		}
	}
}

static void DarMoveElementToEnd( Dar< VPanel* >& dar, VPanel* elem )
{
	int i;

	if ( dar._data[dar._count - 1] == elem )
	{
		return;
	}

	for ( i = dar._count - 2; i >= 0; i-- )
	{
		if ( dar._data[i] == elem )
		{
			memmove( dar._data + i, dar._data + i + 1, sizeof( VPanel* ) * ( dar._count - ( i + 1 ) ) );
			dar._data[dar._count - 1] = elem;
			return;
		}
	}
}

static void DarInsertElementAt( Dar< VPanel* >& dar, VPanel* elem, int index )
{
	int i;

	if ( index < 0 || index > dar._count )
	{
		return;
	}

	if ( index == dar._count || dar._count == 0 )
	{
		DarAddElement( dar, elem );
	}
	else
	{
		DarAddElement( dar, elem );

		for ( i = dar._count - 1; i > index; i-- )
		{
			dar._data[i] = dar._data[i - 1];
		}

		dar._data[index] = elem;
	}
}

static VPanel* DarElement( Dar< VPanel* >& dar, int index )
{
	if ( index < 0 || index >= dar._count )
	{
		return NULL;
	}

	return dar._data[index];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::Init( IClientPanel* attachedClientPanel )
{
	_clientPanel = attachedClientPanel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetPos( int x, int y )
{
	_pos[0] = (short)x;
	_pos[1] = (short)y;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetPos( int& x, int& y )
{
	x = _pos[0];
	y = _pos[1];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetSize( int& wide, int& tall )
{
	wide = _size[0];
	tall = _size[1];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetMinimumSize( int& wide, int& tall )
{
	wide = _minimumSize[0];
	tall = _minimumSize[1];
}

void VPanel::SetEnabled( bool state )
{
	_enabled = state;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool VPanel::IsVisible()
{
	return _visible;
}

bool VPanel::IsEnabled()
{
	return _enabled;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetAbsPos( int& x, int& y )
{
	x = _absPos[0];
	y = _absPos[1];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetClipRect( int& x0, int& y0, int& x1, int& y1 )
{
	x0 = _clipRect[0];
	y0 = _clipRect[1];
	x1 = _clipRect[2];
	y1 = _clipRect[3];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetInset( int left, int top, int right, int bottom )
{
	_inset[0] = (short)left;
	_inset[1] = (short)top;
	_inset[2] = (short)right;
	_inset[3] = (short)bottom;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetInset( int& left, int& top, int& right, int& bottom )
{
	left = _inset[0];
	top = _inset[1];
	right = _inset[2];
	bottom = _inset[3];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int VPanel::GetChildCount()
{
	return _childDar._count;
}

VPanel* VPanel::GetChild( int index )
{
	if ( index < 0 || index >= _childDar._count )
	{
		return NULL;
	}

	return _childDar._data[index];
}

VPanel* VPanel::GetParent()
{
	return _parent;
}

//-----------------------------------------------------------------------------
// Purpose: returns the z position of this panel
//-----------------------------------------------------------------------------
int VPanel::GetZPos()
{
	return _zpos;
}

SurfacePlat* VPanel::Plat()
{
	return _plat;
}

void VPanel::SetPlat( SurfacePlat* Plat )
{
	_plat = Plat;
}

bool VPanel::IsPopup()
{
	return _popup;
}

void VPanel::SetPopup( bool state )
{
	_popup = state;
}

void VPanel::Render_SetPopupVisible( bool state )
{
	_popupVisible = state;
}

VPanel::VPanel()
{
	_childDar._count = 0;
	_childDar._capacity = 0;
	_childDar._data = NULL;

	_pos[0] = _pos[1] = 0;
	_absPos[0] = _absPos[1] = 0;
	_size[0] = _size[1] = 0;

	_minimumSize[0] = 0;
	_minimumSize[1] = 0;

	_zpos = 0;

	_inset[0] = _inset[1] = _inset[2] = _inset[3] = 0;

	_visible = true;
	_enabled = true;
	_clientPanel = NULL;
	_parent = NULL;
	_plat = NULL;
	_popup = false;
	_popupVisible = false;
	_listEntry = INVALID_PANELLIST;

	_mouseInput = true; // by default you want mouse and kb input to this panel
	_kbInput = true;
}

VPanel::~VPanel()
{
	delete[] _childDar._data;
}

void VPanel::Solve()
{
	VPanel* parent;
	int pinset[4];
	int pabsX;
	int pabsY;
	int wide;
	int tall;
	int pclip[4];

	_absPos[0] = _pos[0];
	_absPos[1] = _pos[1];

	parent = GetParent();
	if ( IsPopup() )
	{
		parent = (VPanel*)vgui2::g_pSurface->GetEmbeddedPanel();
	}

	pinset[0] = 0;
	pinset[1] = 0;
	pinset[2] = 0;
	pinset[3] = 0;

	if ( parent )
	{
		parent->GetInset( pinset[0], pinset[1], pinset[2], pinset[3] );
		parent->GetAbsPos( pabsX, pabsY );

		_absPos[0] = (short)( _absPos[0] + pabsX + pinset[0] );
		_absPos[1] = (short)( _absPos[1] + pabsY + pinset[1] );
	}

	// set initial bounds
	_clipRect[0] = _absPos[0];
	_clipRect[1] = _absPos[1];

	GetSize( wide, tall );
	_clipRect[2] = (short)( _absPos[0] + wide );
	_clipRect[3] = (short)( _absPos[1] + tall );

	// clip to parent, if we're not a popup
	if ( parent && !IsPopup() )
	{
		parent->GetClipRect( pclip[0], pclip[1], pclip[2], pclip[3] );

		if ( _clipRect[0] < pclip[0] )
		{
			_clipRect[0] = (short)pclip[0];
		}

		if ( _clipRect[1] < pclip[1] )
		{
			_clipRect[1] = (short)pclip[1];
		}

		if ( _clipRect[2] > pclip[2] )
		{
			_clipRect[2] = (short)( pclip[2] - pinset[2] );
		}

		if ( _clipRect[3] > pclip[3] )
		{
			_clipRect[3] = (short)( pclip[3] - pinset[3] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetSize( int wide, int tall )
{
	if ( wide < _minimumSize[0] )
	{
		wide = _minimumSize[0];
	}

	if ( tall < _minimumSize[1] )
	{
		tall = _minimumSize[1];
	}

	if ( _size[0] == wide && _size[1] == tall )
	{
		return;
	}

	_size[0] = (short)wide;
	_size[1] = (short)tall;

	Client()->OnSizeChanged( wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetMinimumSize( int wide, int tall )
{
	int currentWidth;
	int currentHeight;

	_minimumSize[0] = (short)wide;
	_minimumSize[1] = (short)tall;

	currentWidth = _size[0];
	if ( currentWidth < wide )
	{
		currentWidth = wide;
	}

	currentHeight = _size[1];
	if ( currentHeight < tall )
	{
		currentHeight = tall;
	}

	// resize to new minimum size if necessary
	if ( currentWidth != _size[0] || currentHeight != _size[1] )
	{
		SetSize( currentWidth, currentHeight );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetVisible( bool state )
{
	if ( _visible == state )
	{
		return;
	}

	vgui2::g_pSurface->SetPanelVisible( (VPANEL)this, state );

	_visible = state;

	if ( IsPopup() )
	{
		vgui2::g_pSurface->CalculateMouseVisible();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetParent( VPanel* newParent )
{
	if ( this == newParent )
	{
		return;
	}

	if ( _parent == newParent )
	{
		return;
	}

	if ( _parent )
	{
		DarRemoveElement( _parent->_childDar, this );
		_parent = NULL;
	}

	if ( newParent )
	{
		_parent = newParent;
		DarPutElement( _parent->_childDar, this );
		SetZPos( _zpos ); // re-sort parent's panel order if necessary

		if ( _parent->Client() )
		{
			_parent->Client()->OnChildAdded( (VPANEL)this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the Z position of a panel and reorders it appropriately
//-----------------------------------------------------------------------------
void VPanel::SetZPos( int z )
{
	int childCount;
	int i;
	VPanel* prevChild;
	VPanel* nextChild;

	_zpos = (short)z;

	if ( _parent )
	{
		childCount = _parent->GetChildCount();

		for ( i = 0; i < childCount; i++ )
		{
			if ( _parent->GetChild( i ) == this )
			{
				break;
			}
		}

		if ( i == childCount )
		{
			return;
		}

		while ( 1 )
		{
			prevChild = NULL;
			nextChild = NULL;

			if ( i > 0 )
			{
				prevChild = _parent->GetChild( i - 1 );
			}

			if ( i < childCount - 1 )
			{
				nextChild = _parent->GetChild( i + 1 );
			}

			// check either side of the child to see if it should move
			if ( i > 0 && prevChild && prevChild->_zpos > _zpos )
			{
				DarSetElementAt( _parent->_childDar, prevChild, i );
				DarSetElementAt( _parent->_childDar, this, i - 1 );
				i--;
			}
			else if ( i < childCount - 1 && nextChild && nextChild->_zpos < _zpos )
			{
				DarSetElementAt( _parent->_childDar, nextChild, i );
				DarSetElementAt( _parent->_childDar, this, i + 1 );
				i++;
			}
			else
			{
				break;
			}
		}
	}
}

void VPanel::MoveToFront()
{
	int i;
	VPanel* child;

	vgui2::g_pSurface->MovePopupToFront( (VPANEL)this );

	if ( _parent )
	{
		DarMoveElementToEnd( _parent->_childDar, this );

		i = _parent->_childDar._count - 2;
		while ( i >= 0 )
		{
			child = DarElement( _parent->_childDar, i );
			if ( child->_zpos > _zpos )
			{
				DarSetElementAt( _parent->_childDar, child, i + 1 );
				DarSetElementAt( _parent->_childDar, this, i );
				// check the next value
				i--;
			}
			else
			{
				// order valid
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Moves the panel to the back of the z-order
//-----------------------------------------------------------------------------
void VPanel::MoveToBack()
{
	int i;
	VPanel* child;

	if ( _parent )
	{
		DarRemoveElement( _parent->_childDar, this );
		DarInsertElementAt( _parent->_childDar, this, 0 );

		i = 1;
		while ( i < _parent->_childDar._count )
		{
			child = DarElement( _parent->_childDar, i );
			if ( child->_zpos < _zpos )
			{
				DarSetElementAt( _parent->_childDar, child, i - 1 );
				DarSetElementAt( _parent->_childDar, this, i );
				// check the next value
				i++;
			}
			else
			{
				// order valid
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Iterates up the hierarchy looking to see if a panel has the specified ancestor
//-----------------------------------------------------------------------------
bool VPanel::HasParent( VPanel* potentialParent )
{
	if ( this == potentialParent )
	{
		return true;
	}

	if ( _parent )
	{
		return _parent->HasParent( potentialParent );
	}

	return false;
}

bool VPanel::Render_IsPopupPanelVisible()
{
	return _popupVisible;
}

const char* VPanel::GetName()
{
	return Client()->GetName();
}

const char* VPanel::GetClassName()
{
	return Client()->GetClassName();
}

HScheme VPanel::GetScheme()
{
	return Client()->GetScheme();
}

void VPanel::SendMessage( KeyValues* params, VPANEL ifrompanel )
{
	Client()->OnMessage( params, ifrompanel );
}

void VPanel::SetKeyBoardInputEnabled( bool state )
{
	_kbInput = state;
}

void VPanel::SetMouseInputEnabled( bool state )
{
	_mouseInput = state;
}

bool VPanel::IsKeyBoardInputEnabled()
{
	return _kbInput;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool VPanel::IsMouseInputEnabled()
{
	return _mouseInput;
}
