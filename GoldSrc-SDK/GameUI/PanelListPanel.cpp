//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "PanelListPanel.h"

#include "Button.h"
#include "ScrollBar.h"
#include "vgui/IScheme.h"

class VScrollBarReversedButtons : public vgui2::ScrollBar
{
public:
	VScrollBarReversedButtons( vgui2::Panel* parent, const char* panelName, bool vertical ) :
			vgui2::ScrollBar( parent, panelName, vertical )
	{
	}

	void ApplySchemeSettings( vgui2::IScheme* scheme ) override
	{
		vgui2::ScrollBar::ApplySchemeSettings( scheme );
		for ( int i = 0; i < 2; i++ )
		{
			vgui2::Button* button = GetButton( i );
			button->SetArmedColor( button->GetSchemeColor( "DimBaseText", scheme ), button->GetBgColor() );
			button->SetDepressedColor( button->GetSchemeColor( "DimBaseText", scheme ), button->GetBgColor() );
			button->SetDefaultColor( button->GetFgColor(), button->GetBgColor() );
		}
	}
};

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CPanelListPanel::CPanelListPanel( vgui2::Panel* parent, const char* panelName, bool inverseButtons ) :
		vgui2::Panel( parent, panelName )
{
	SetBounds( 0, 0, GetProportionalScaledValue( 100 ), GetProportionalScaledValue( 100 ) );
	_sliderYOffset = 0;
	if ( inverseButtons )
		_vbar = new VScrollBarReversedButtons( this, "CPanelListPanelVScroll", true );
	else
		_vbar = new vgui2::ScrollBar( this, "CPanelListPanelVScroll", true );

	_vbar->SetBounds( 0, 0, GetProportionalScaledValue( 20 ), GetProportionalScaledValue( 20 ) );
	_vbar->SetVisible( false );
	_vbar->AddActionSignalTarget( this );
	_embedded = new vgui2::Panel( this, "PanelListEmbedded" );
	_embedded->SetBounds( 0, 0, GetProportionalScaledValue( 20 ), GetProportionalScaledValue( 20 ) );
	_embedded->SetPaintBackgroundEnabled( false );
	_embedded->SetPaintBorderEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPanelListPanel::~CPanelListPanel()
{
	DeleteAllItems();
}

vgui2::Panel* CPanelListPanel::GetCellRenderer( int row )
{
	DATAITEM* item = _dataItems[row];
	return item ? item->panel : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
int CPanelListPanel::GetItemCount()
{
	return _dataItems.GetCount();
}

vgui2::Panel* CPanelListPanel::GetItem( int itemIndex )
{
	if ( itemIndex < 0 || itemIndex >= _dataItems.GetCount() )
		return NULL;

	return _dataItems[itemIndex]->panel;
}

void CPanelListPanel::OnSliderMoved( int )
{
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPanelListPanel::OnMouseWheeled( int delta )
{
	int value = _vbar->GetValue();
	_vbar->SetValue( value - GetProportionalScaledValue( 15 ) * delta );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//-----------------------------------------------------------------------------
void CPanelListPanel::RemoveItem( int itemIndex )
{
	DATAITEM* item = _dataItems[itemIndex];
	delete item->panel;
	delete item;
	_dataItems.RemoveElementAt( itemIndex );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: clears and deletes all the memory used by the data items
//-----------------------------------------------------------------------------
void CPanelListPanel::DeleteAllItems()
{
	for ( int i = 0; i < _dataItems.GetCount(); i++ )
	{
		DATAITEM* item = _dataItems[i];
		if ( item )
			delete item->panel;

		delete _dataItems[i];
	}
	_dataItems.RemoveAll();
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPanelListPanel::computeVPixelsNeeded()
{
	int pixels = 0;
	for ( int i = 0; i < _dataItems.GetCount(); i++ )
	{
		DATAITEM* item = _dataItems[i];
		if ( item && item->panel )
		{
			int wide, tall;
			item->panel->GetSize( wide, tall );
			pixels += tall;
		}
	}
	return pixels + GetProportionalScaledValue( 5 );
}

//-----------------------------------------------------------------------------
// Purpose: adds an item to the view
//			data->GetName() is used to uniquely identify an item
//			data sub items are matched against column header name to be used in the table
// Input  : *item - 
//-----------------------------------------------------------------------------
int CPanelListPanel::AddItem( vgui2::Panel* panel )
{
	InvalidateLayout( false, false );
	DATAITEM* item = new DATAITEM;
	item->panel = panel;
	panel->SetParent( _embedded );
	int index = _dataItems.FindElement( item );
	if ( index >= 0 )
		return index;

	return _dataItems.AddElement( item );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPanelListPanel::PaintBackground()
{
	BaseClass::PaintBackground();
}

//-----------------------------------------------------------------------------
// Purpose: relayouts out the panel after any internal changes
//-----------------------------------------------------------------------------
void CPanelListPanel::PerformLayout()
{
	int wide, tall;
	GetSize( wide, tall );
	int pixels = computeVPixelsNeeded();
	//!! need to make it recalculate scroll positions
	_vbar->SetVisible( true );
	_vbar->SetEnabled( false );
	_vbar->SetRange( 0, pixels - tall + GetProportionalScaledValue( 24 ) );
	_vbar->SetRangeWindow( GetProportionalScaledValue( 24 ) );
	_vbar->SetButtonPressedScrollValue( GetProportionalScaledValue( 24 ) );
	_vbar->SetPos( wide - GetProportionalScaledValue( 20 ), _sliderYOffset );
	_vbar->SetSize( GetProportionalScaledValue( 18 ), tall - GetProportionalScaledValue( 2 ) - _sliderYOffset );
	_vbar->InvalidateLayout( false, false );
	_embedded->SetPos( 0, -_vbar->GetValue() );
	_embedded->SetSize( wide - GetProportionalScaledValue( 20 ), pixels );

	// Now lay out the controls on the embedded panel
	int y = 0;
	int rowTall = 0;
	for ( int i = 0; i < _dataItems.GetCount(); i++ )
	{
		DATAITEM* item = _dataItems[i];
		if ( item && item->panel )
		{
			rowTall = item->panel->GetTall();
			item->panel->SetBounds( GetProportionalScaledValue( 8 ), y,
					wide - GetProportionalScaledValue( 36 ), rowTall );
		}
		y += rowTall;
	}
}

CPanelListPanel::DATAITEM* CPanelListPanel::GetDataItem( int itemIndex )
{
	return _dataItems[itemIndex];
}

void CPanelListPanel::SetSliderYOffset( int pixels )
{
	_sliderYOffset = pixels;
}

void CPanelListPanel::ApplySchemeSettings( vgui2::IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
	SetBgColor( GetSchemeColor( "LabelBgColor", GetSchemeColor( "WindowDisabledBgColor", scheme ), scheme ) );
}
