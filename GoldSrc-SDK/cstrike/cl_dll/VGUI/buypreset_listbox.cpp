#include "buypreset_listbox.h"
#include "EditablePanel.h"
#include "ScrollBar.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "minmax.h"

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\cl_dll\\VGUI\\buypreset_listbox.cpp" )

using namespace vgui2;

// unscaled layout metrics; scaled to the current proportional state in PerformLayout
static const int SCROLLBAR_BASE_SIZE = 18;
static const int DEFAULT_HEIGHT_BASE = 24;
static const int PANEL_BUFFER_BASE = 5;

BuyPresetListBox::BuyPresetListBox( Panel* parent, const char* panelName ) : Panel( parent, panelName )
{
	m_visibleIndex = 0;
	m_lastSize = 0;
	SetBounds( 0, 0, 100, 100 );
	m_vbar = new ScrollBar( this, "PanelListPanelVScroll", true );
	m_vbar->SetBounds( 0, 0, 20, 20 );
	m_vbar->SetVisible( false );
	m_vbar->AddActionSignalTarget( this );
	m_pPanelEmbedded = new EditablePanel( this, "PanelListEmbedded" );
	m_pPanelEmbedded->SetBounds( 0, 0, 20, 20 );
	m_pPanelEmbedded->SetPaintBackgroundEnabled( false );
	m_pPanelEmbedded->SetPaintBorderEnabled( false );
	m_iScrollbarSize = SCROLLBAR_BASE_SIZE;
	m_iDefaultHeight = DEFAULT_HEIGHT_BASE;
	m_iPanelBuffer = PANEL_BUFFER_BASE;
}

BuyPresetListBox::~BuyPresetListBox()
{
	DeleteAllItems();
}

int BuyPresetListBox::AddItem( Panel* panel, void* userData )
{
	ASSERT_AT( panel, 140 );
	panel->SetParent( m_pPanelEmbedded );
	dataitem_s item = { panel, userData };
	m_items.push_back( item );
	InvalidateLayout( false, false );
	return (int)m_items.size();
}

int BuyPresetListBox::GetItemCount() const
{
	return (int)m_items.size();
}

void BuyPresetListBox::SwapItems( int first, int second )
{
	if ( (unsigned)first >= m_items.size() || (unsigned)second >= m_items.size() )
		return;
	std::swap( m_items[first], m_items[second] );
	InvalidateLayout( false, false );
}

void BuyPresetListBox::MakeItemVisible( int index )
{
	m_visibleIndex = index;
	m_lastSize = 0;
}

Panel* BuyPresetListBox::GetItemPanel( int index ) const
{
	return (unsigned)index < m_items.size() ? m_items[index].panel : NULL;
}

void* BuyPresetListBox::GetItemUserData( int index )
{
	return (unsigned)index < m_items.size() ? m_items[index].userData : NULL;
}

void BuyPresetListBox::SetItemUserData( int index, void* data )
{
	if ( (unsigned)index < m_items.size() )
		m_items[index].userData = data;
}

void BuyPresetListBox::RemoveItem( int index )
{
	if ( (unsigned)index >= m_items.size() )
		return;
	if ( m_items[index].panel )
		m_items[index].panel->MarkForDeletion();
	if ( m_items[index].userData )
		::operator delete( m_items[index].userData );
	m_items.erase( m_items.begin() + index );
	InvalidateLayout( false, false );
}

void BuyPresetListBox::DeleteAllItems()
{
	while ( !m_items.empty() )
		RemoveItem( 0 );
	m_vbar->SetValue( 0 );
	InvalidateLayout( false, false );
}

void BuyPresetListBox::OnSizeChanged( int wide, int tall )
{
	Panel::OnSizeChanged( wide, tall );
	InvalidateLayout( false, false );
}

void BuyPresetListBox::OnSliderMoved( int )
{
	InvalidateLayout( false, false );
	Repaint();
}

void BuyPresetListBox::OnMouseWheeled( int delta )
{
	int step = 3;
	if ( !m_items.empty() && m_items[0].panel )
		step = m_iPanelBuffer + m_items[0].panel->GetTall();
	m_vbar->SetValue( m_vbar->GetValue() - step * delta );
}

void BuyPresetListBox::MoveScrollBarToTop()
{
	m_vbar->SetValue( 0 );
	OnSliderMoved( 0 );
}

int BuyPresetListBox::computeVPixelsNeeded()
{
	int height = 0;
	for ( unsigned i = 0; i < m_items.size(); ++i )
	{
		if ( m_items[i].panel )
			height += m_items[i].panel->GetTall() + m_iPanelBuffer;
	}
	return height + m_iPanelBuffer;
}

void BuyPresetListBox::PerformLayout()
{
	int wide, tall;
	GetSize( wide, tall );
	// recompute from the base sizes so proportional, resolution and scheme changes are picked up
	m_iScrollbarSize = GetProportionalScaledValue( SCROLLBAR_BASE_SIZE );
	m_iDefaultHeight = GetProportionalScaledValue( DEFAULT_HEIGHT_BASE );
	m_iPanelBuffer = GetProportionalScaledValue( PANEL_BUFFER_BASE );
	int pixels = computeVPixelsNeeded();
	int visible = m_visibleIndex;
	m_vbar->SetVisible( true );
	m_vbar->SetEnabled( false );
	m_vbar->SetRange( 0, max( 0, m_iDefaultHeight + pixels - tall ) );
	m_vbar->SetRangeWindow( m_iDefaultHeight );
	m_vbar->SetButtonPressedScrollValue( m_iDefaultHeight );
	m_vbar->SetPos( wide - m_iScrollbarSize, 1 );
	m_vbar->SetSize( m_iScrollbarSize, tall - 2 );
	m_visibleIndex = visible;
	int value = max( 0, m_vbar->GetValue() );
	m_pPanelEmbedded->SetPos( 1, -value );
	m_pPanelEmbedded->SetSize( wide - m_iScrollbarSize - 2, pixels );
	m_vbar->GetValue();
	int y = 0;
	for ( unsigned i = 0; i < m_items.size(); ++i )
	{
		Panel* panel = m_items[i].panel;
		int height = panel->GetTall();
		y += m_iPanelBuffer;
		panel->SetBounds( 8, y, wide - m_iScrollbarSize - 16, height );
		panel->InvalidateLayout( false, false );
		y += height;
	}
	if ( (unsigned)m_visibleIndex < m_items.size() )
	{
		GetSize( wide, tall );
		int rangeMin, rangeMax;
		m_vbar->GetRange( rangeMin, rangeMax );
		value = max( 0, m_vbar->GetValue() );
		m_vbar->GetValue();
		int x, top, width, height;
		m_items[m_visibleIndex].panel->GetBounds( x, top, width, height );
		int bottom = top + height;
		if ( top < value || value + tall - 2 < bottom )
		{
			int delta = top < value ? top - value : bottom - ( value + tall - 2 );
			m_vbar->SetValue( delta );
			OnSliderMoved( delta );
		}
	}
	if ( pixels == m_lastSize )
		m_visibleIndex = -1;
	m_lastSize = pixels;
}

void BuyPresetListBox::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	SetBgColor( GetSchemeColor( "LabelBgColor", GetSchemeColor( "WindowDisabledBgColor", scheme ), scheme ) );
	SetBorder( scheme->GetBorder( "BrowserBorder" ) );
	m_vbar->SetBorder( scheme->GetBorder( "BrowserBorder" ) );
	PerformLayout();
}

void BuyPresetListBox::OnCommand( const char* command )
{
	GetParent()->OnCommand( command );
}
