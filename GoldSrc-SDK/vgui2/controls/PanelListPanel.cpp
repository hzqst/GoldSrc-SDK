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

#include "PanelListPanel.h"

#include "EditablePanel.h"
#include "ScrollBar.h"
#include "vgui/IScheme.h"
#include "vgui/MessageMap.h"
#include "controls.h"

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\PanelListPanel.cpp" )
#else
#include <assert.h>
#define ASSERT_AT( expression, line )	assert( expression )
#endif

namespace vgui2
{
PanelListPanel::PanelListPanel( Panel* parent, const char* panelName ) : Panel( parent, panelName ), m_DataItems( 0, 0 )
{
	SetBounds( 0, 0, 100, 100 );

	m_vbar = new ScrollBar( this, "PanelListPanelVScroll", true );
	m_vbar->SetBounds( 0, 0, 20, 20 );
	m_vbar->SetVisible( false );
	m_vbar->AddActionSignalTarget( this );

	m_pPanelEmbedded = new EditablePanel( this, "PanelListEmbedded" );
	m_pPanelEmbedded->SetBounds( 0, 0, 20, 20 );
	m_pPanelEmbedded->SetPaintBackgroundEnabled( false );
	m_pPanelEmbedded->SetPaintBorderEnabled( false );

	m_iFirstColumnWidth = 100; // default width

	if ( IsProportional() )
	{
		int width, height;
		GetProportionalBase( width, height );

		int sw, sh;
		surface()->GetScreenSize( sw, sh );

		float scale = (float)sw / (float)width;
		m_iScrollbarSize = (int)( 18.0f * scale );
		m_iDefaultHeight = (int)( 24.0f * scale );
		m_iPanelBuffer = (int)( 5.0f * scale );
	}
	else
	{
		m_iScrollbarSize = 18;
		m_iDefaultHeight = 24;
		m_iPanelBuffer = 5;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
PanelListPanel::~PanelListPanel()
{
	// free data from table
	DeleteAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int PanelListPanel::computeVPixelsNeeded()
{
	int pixels = 0;
	for ( int i = 0; i < m_SortedItems.Count(); i++ )
	{
		Panel* panel = m_DataItems[m_SortedItems[i]]->panel;
		if ( !panel )
			continue;

		int w, h;
		panel->GetSize( w, h );
		pixels += h + m_iPanelBuffer;
	}

	return pixels + m_iPanelBuffer;
}

//-----------------------------------------------------------------------------
// Purpose: adds an item to the view
//			data->GetName() is used to uniquely identify an item
//			data sub items are matched against column header name to be used in the table
//-----------------------------------------------------------------------------
int PanelListPanel::AddItem( Panel* labelPanel, Panel* panel )
{
#if defined( _WIN32 )
	ASSERT_AT( labelPanel && panel, 124 );
#endif

	DATAITEM* newitem = new DATAITEM;
	newitem->labelPanel = labelPanel;
	newitem->panel = panel;

	labelPanel->SetParent( m_pPanelEmbedded );
	panel->SetParent( m_pPanelEmbedded );

	int itemID = m_DataItems.AddToTail( newitem );
	m_SortedItems.AddToTail( itemID );

	InvalidateLayout( false, false );
	return itemID;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
int PanelListPanel::GetItemCount()
{
	return m_DataItems.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns label panel for this itemID
//-----------------------------------------------------------------------------
Panel* PanelListPanel::GetItemLabel( int itemID )
{
	if ( !m_DataItems.IsValidIndex( itemID ) )
		return NULL;

	return m_DataItems[itemID]->labelPanel;
}

//-----------------------------------------------------------------------------
// Purpose: returns label panel for this itemID
//-----------------------------------------------------------------------------
Panel* PanelListPanel::GetItemPanel( int itemID )
{
	if ( !m_DataItems.IsValidIndex( itemID ) )
		return NULL;

	return m_DataItems[itemID]->panel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PanelListPanel::RemoveItem( int itemID )
{
	if ( !m_DataItems.IsValidIndex( itemID ) )
		return;

	DATAITEM* item = m_DataItems[itemID];
	if ( item->panel )
		item->panel->MarkForDeletion();
	if ( item->labelPanel )
		item->labelPanel->MarkForDeletion();
	delete item;

	m_DataItems.Remove( itemID );
	for ( int i = 0; i < m_SortedItems.Count(); i++ )
	{
		if ( m_SortedItems[i] == itemID )
		{
			m_SortedItems.Remove( i );
			break;
		}
	}

	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: clears and deletes all the memory used by the data items
//-----------------------------------------------------------------------------
void PanelListPanel::DeleteAllItems()
{
	for ( int i = m_DataItems.Head(); i != m_DataItems.InvalidIndex(); i = m_DataItems.Next( i ) )
	{
		if ( m_DataItems[i] )
			delete m_DataItems[i]->panel;
		delete m_DataItems[i];
	}

	m_DataItems.RemoveAll();
	m_SortedItems.RemoveAll();

	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the panel to use to render a cell
//-----------------------------------------------------------------------------
Panel* PanelListPanel::GetCellRenderer( int row )
{
	if ( row < 0 || row >= m_SortedItems.Count() )
		return NULL;

	return m_DataItems[m_SortedItems[row]]->panel;
}

void PanelListPanel::OnSliderMoved( int )
{
	InvalidateLayout( false, false );
	Repaint();
}

void PanelListPanel::MoveScrollBarToTop()
{
	m_vbar->SetValue( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: clears and deletes all the memory used by the data items
//-----------------------------------------------------------------------------
void PanelListPanel::RemoveAll()
{
	m_DataItems.RemoveAll();
	m_SortedItems.RemoveAll();

	// move the scrollbar to the top of the list
	m_vbar->SetValue( 0 );
	InvalidateLayout( false, false );
}

void PanelListPanel::SetFirstColumnWidth( int width )
{
	m_iFirstColumnWidth = width;
}

int PanelListPanel::GetFirstColumnWidth()
{
	return m_iFirstColumnWidth;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PanelListPanel::OnSizeChanged( int wide, int tall )
{
	Panel::OnSizeChanged( wide, tall );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: relayouts out the panel after any internal changes
//-----------------------------------------------------------------------------
void PanelListPanel::PerformLayout()
{
	int wide, tall;
	GetSize( wide, tall );

	int vpixels = computeVPixelsNeeded();

	//!! need to make it recalculate scroll positions
	m_vbar->SetVisible( true );
	m_vbar->SetEnabled( false );
	m_vbar->SetRange( 0, m_iDefaultHeight + vpixels - tall );
	m_vbar->SetRangeWindow( m_iDefaultHeight );
	m_vbar->SetButtonPressedScrollValue( m_iDefaultHeight ); // standard height of labels/buttons etc.
	m_vbar->SetPos( wide - m_iScrollbarSize, 1 );
	m_vbar->SetSize( m_iScrollbarSize, tall - 2 );

	m_pPanelEmbedded->SetPos( 1, -m_vbar->GetValue() );
	m_pPanelEmbedded->SetSize( wide - m_iScrollbarSize - 2, vpixels );

	// Now lay out the controls on the embedded panel
	int top = m_vbar->GetValue();
#if !defined( _WIN32 )
	int count = m_SortedItems.Count();
#endif
	int y = 0;
	int totalh = 0;
#if defined( _WIN32 )
	for ( int i = 0; i < m_SortedItems.Count(); i++ )
#else
	for ( int i = 0; i < count; i++ )
#endif
	{
		// add in a little buffer between panels
		y += m_iPanelBuffer;

		DATAITEM* item = m_DataItems[m_SortedItems[i]];
#if defined( _WIN32 )
		ASSERT_AT( item, 278 );
#endif

		int h = item->panel->GetTall();
		totalh += h;

		if ( totalh >= top )
		{
			item->labelPanel->SetBounds( 8, y, m_iFirstColumnWidth, h );
			item->panel->SetBounds( m_iFirstColumnWidth + m_iPanelBuffer + 8, y,
				wide - m_iFirstColumnWidth - m_iScrollbarSize - 10, h );
		}

		y += h;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PanelListPanel::PaintBackground()
{
	Panel::PaintBackground();
}

void PanelListPanel::ApplySchemeSettings( IScheme* pScheme )
{
	Panel::ApplySchemeSettings( pScheme );

	SetBorder( pScheme->GetBorder( "ButtonDepressedBorder" ) );
	SetBgColor( GetSchemeColor( "LabelBgColor", GetSchemeColor( "WindowDisabledBgColor", pScheme ), pScheme ) );
}
}
