#include "PanelHorizListPanel.h"

#include "Button.h"
#include "ScrollBar.h"
#include "vgui/IScheme.h"

class ScrollBarReversedButtons : public vgui2::ScrollBar
{
public:
	ScrollBarReversedButtons( vgui2::Panel* parent, const char* panelName, bool vertical )
		: vgui2::ScrollBar( parent, panelName, vertical )
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

namespace vgui2
{
// unscaled layout metrics; scaled to the current proportional state when used
static const int ITEM_PAD_BASE = 5;
static const int SCROLLBAR_AREA_BASE = 20;
static const int SCROLLBAR_SIZE_BASE = 18;
static const int SCROLLBAR_INSET_BASE = 2;
static const int SCROLL_BUTTON_STEP_BASE = 24;
static const int SCROLL_WHEEL_STEP_BASE = 15;

MessageMapItem_t PanelHorizListPanel::m_MessageMap[1] =
{
	{ "ScrollBarSliderMoved", (MessageFunc_t)(void ( Panel::* )( int ))&PanelHorizListPanel::OnSliderMoved,
		1, DATATYPE_INT, "position", DATATYPE_VOID, NULL, 0, 0, 0 }
};

PanelMap_t PanelHorizListPanel::m_PanelMap =
{
	m_MessageMap, 1, "PanelHorizListPanel", &Panel::m_PanelMap, 0
};

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
PanelHorizListPanel::PanelHorizListPanel( Panel* parent, const char* panelName ) : Panel( parent, panelName )
{
	SetBounds( 0, 0, 100, 100 );
	_hbar = new ScrollBarReversedButtons( this, "CPanelListPanelHScroll", false );
	_hbar->SetBounds( 0, 0, 20, 20 );
	_hbar->SetVisible( false );
	_hbar->AddActionSignalTarget( this );
	_embedded = new Panel( this, "PanelListEmbedded" );
	_embedded->SetBounds( 0, 0, 20, 20 );
	_embedded->SetPaintBackgroundEnabled( false );
	_embedded->SetPaintBorderEnabled( false );
	m_itemPad = ITEM_PAD_BASE;
}

PanelHorizListPanel::~PanelHorizListPanel()
{
	DeleteAllItems();
}

PanelMap_t* PanelHorizListPanel::GetPanelMap()
{
	return &m_PanelMap;
}

Panel* PanelHorizListPanel::GetCellRenderer( int row )
{
	DATAITEM* item = GetDataItem( row );
	return item ? item->panel : NULL;
}

int PanelHorizListPanel::GetItemCount()
{
	return _dataItems.GetCount();
}

Panel* PanelHorizListPanel::GetItem( int itemIndex )
{
	if ( itemIndex < 0 || itemIndex >= _dataItems.GetCount() )
		return NULL;
	return _dataItems[itemIndex]->panel;
}

PanelHorizListPanel::DATAITEM* PanelHorizListPanel::GetDataItem( int itemIndex )
{
	return _dataItems[itemIndex];
}

void PanelHorizListPanel::OnSliderMoved( int )
{
	InvalidateLayout( false, false );
	Repaint();
}

void PanelHorizListPanel::OnMouseWheeled( int delta )
{
	int value = _hbar->GetValue();
	_hbar->SetValue( value - GetProportionalScaledValue( SCROLL_WHEEL_STEP_BASE ) * delta );
}

void PanelHorizListPanel::RemoveItem( int itemIndex )
{
	DATAITEM* item = GetDataItem( itemIndex );
	if ( !item )
		return;
	delete item->panel;
	delete item;
	_dataItems.RemoveElementAt( itemIndex );
	InvalidateLayout( false, false );
}

void PanelHorizListPanel::DeleteAllItems()
{
	for ( int i = 0; i < _dataItems.GetCount(); i++ )
	{
		DATAITEM* item = _dataItems[i];
		if ( item )
			delete item->panel;
		delete item;
	}
	_dataItems.RemoveAll();
	InvalidateLayout( false, false );
}

int PanelHorizListPanel::computeHPixelsNeeded()
{
	int needed = m_itemPad;
	for ( int i = 0; i < _dataItems.GetCount(); i++ )
	{
		DATAITEM* item = GetDataItem( i );
		if ( item && item->panel && item->panel->IsVisible() )
			needed += m_itemPad + item->panel->GetWide();
	}
	return needed;
}

int PanelHorizListPanel::AddItem( Panel* panel )
{
	InvalidateLayout( false, false );
	DATAITEM* item = new DATAITEM;
	item->panel = panel;
	panel->SetParent( _embedded );
	int index = _dataItems.FindElement( item );
	return index != -1 ? index : _dataItems.AddElement( item );
}

void PanelHorizListPanel::PaintBackground()
{
	Panel::PaintBackground();
}

void PanelHorizListPanel::PerformLayout()
{
	int wide, tall;
	GetSize( wide, tall );
	// recompute from the base sizes so proportional, resolution and scheme changes are picked up
	m_itemPad = GetProportionalScaledValue( ITEM_PAD_BASE );
	int scrollbarArea = GetProportionalScaledValue( SCROLLBAR_AREA_BASE );
	int needed = computeHPixelsNeeded();
	int window = 3 * wide / 4;
	int value = _hbar->GetValue();
	_hbar->SetVisible( true );
	_hbar->SetEnabled( false );
	_hbar->SetRange( 0, window + needed - wide );
	_hbar->SetRangeWindow( window );
	_hbar->SetButtonPressedScrollValue( GetProportionalScaledValue( SCROLL_BUTTON_STEP_BASE ) );
	_hbar->SetPos( 0, tall - scrollbarArea );
	_hbar->SetSize( wide - GetProportionalScaledValue( SCROLLBAR_INSET_BASE ), GetProportionalScaledValue( SCROLLBAR_SIZE_BASE ) );
	_hbar->InvalidateLayout( false, false );
	_hbar->SetValue( value );
	_embedded->SetPos( -_hbar->GetValue(), 0 );
	_embedded->SetSize( needed, tall - scrollbarArea );

	int x = m_itemPad;
	for ( int i = 0; i < _dataItems.GetCount(); i++ )
	{
		DATAITEM* item = GetDataItem( i );
		if ( item && item->panel && item->panel->IsVisible() )
		{
			int itemWide = item->panel->GetWide();
			item->panel->SetBounds( x, m_itemPad, itemWide, tall - scrollbarArea - 2 * m_itemPad );
			x += m_itemPad + itemWide;
		}
	}
}

void PanelHorizListPanel::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
	SetBgColor( GetSchemeColor( "LabelBgColor", GetSchemeColor( "WindowDisabledBgColor", scheme ), scheme ) );
	_hbar->GetButton( 0 )->SetArmedSound( "sound/UI/buttonrollover.wav" );
	_hbar->GetButton( 0 )->SetDepressedSound( "sound/UI/buttonclick.wav" );
	_hbar->GetButton( 0 )->SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
	_hbar->GetButton( 1 )->SetArmedSound( "sound/UI/buttonrollover.wav" );
	_hbar->GetButton( 1 )->SetDepressedSound( "sound/UI/buttonclick.wav" );
	_hbar->GetButton( 1 )->SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
	_hbar->SetBorder( scheme->GetBorder( "BrowserBorder" ) );
}

void PanelHorizListPanel::GetSliderRange( int& low, int& high )
{
	_hbar->GetRange( low, high );
}

int PanelHorizListPanel::GetSliderPosition()
{
	return _hbar->GetValue();
}

void PanelHorizListPanel::SetSliderPosition( int position )
{
	_hbar->SetValue( position );
}
}
