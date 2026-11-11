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

#include "SectionedListPanel.h"

#include "ImageList.h"
#include "Label.h"
#include "ScrollBar.h"
#include "TextImage.h"
#include "controls.h"
#include "../../../engine/controls.h"
#include "ILocalize.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"

#include <cstring>
#include <cwchar>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\SectionedListPanel.cpp" )

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Individual items in the list
//-----------------------------------------------------------------------------
class CItemButton : public Label
{
	DECLARE_CLASS_SIMPLE( CItemButton, Label );

public:
	CItemButton( SectionedListPanel* parent, int itemID ) : Label( parent, NULL, "< item >" )
	{
		m_pListPanel = parent;
		m_iID = itemID;
		m_iSectionID = -1;
		m_pData = NULL;
		SetProportionalScaleImages( false );
		m_bSelected = false;
		m_bOverrideColors = false;
		SetPaintBackgroundEnabled( false );
		SetTextImageIndex( -1 );
		ClearImages();
		MakeReadyForUse();
	}
	~CItemButton() override;
	virtual void SetOverrideColors( bool state );

	void OnSetFocus() override;
	void OnKillFocus() override;
	void OnMousePressed( MouseCode code ) override;
	void OnMouseDoublePressed( MouseCode code ) override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void PaintBackground() override;
	void PerformLayout() override;

	SectionedListPanel*		m_pListPanel;
	int						m_iID;
	int						m_iSectionID;
	KeyValues*				m_pData;
	Color					m_FgColor1;
	Color					m_FgColor2;
	Color					m_BgColor;
	Color					m_ArmedFgColor1;
	Color					m_ArmedFgColor2;
	Color					m_ArmedBgColor;
	Color					m_SelectionBG2Color;
	CUtlVector<TextImage*>	m_TextImages;
	bool					m_bSelected;
	bool					m_bOverrideColors;
};

//-----------------------------------------------------------------------------
// Purpose: header label that seperates and names each section
//-----------------------------------------------------------------------------
class CSectionHeader : public Label
{
	DECLARE_CLASS_SIMPLE( CSectionHeader, Label );

public:
	CSectionHeader( SectionedListPanel* parent, int sectionID, const char* name ) : Label( parent, name, "" )
	{
		m_pListPanel = parent;
		m_iSectionID = sectionID;
		SetTextImageIndex( -1 );
		SetProportionalScaleImages( false );
		ClearImages();
		SetPaintBackgroundEnabled( false );
		MakeReadyForUse();
	}
	CSectionHeader( SectionedListPanel* parent, int sectionID, const wchar_t* name )
		: Label( parent, "SectionHeader", "" )
	{
		SetText( name );
		SetVisible( false );
		m_pListPanel = parent;
		m_iSectionID = sectionID;
		SetTextImageIndex( -1 );
		ClearImages();
		MakeReadyForUse();
	}
	~CSectionHeader() override {}

	void Paint() override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void PerformLayout() override;

	int					m_iSectionID;
	Color				m_SectionDividerColor;
	SectionedListPanel*	m_pListPanel;
};

SectionedListPanel::SectionedListPanel( Panel* parent, const char* name ) : Panel( parent, name )
{
	m_pScrollBar = new ScrollBar( this, "SectionedScrollBar", true );
	m_pScrollBar->SetVisible( false );
	m_pScrollBar->AddActionSignalTarget( this );
	m_iEditModeItemID = 0;
	m_iEditModeColumn = 0;
	m_bSortNeeded = false;
	m_iContentHeight = 0;
	m_bVerticalScrollbarEnabled = true;
	m_iLineSpacing = 20;
	m_iLineSpacingOverride = 0;
	m_pImageList = NULL;
	m_bDeleteImageListWhenDone = false;
	if ( IsProportional() )
	{
		int width, height, screenWidth, screenHeight;
		GetProportionalBase( width, height );
		surface()->GetScreenSize( screenWidth, screenHeight );
		float size = (float)screenWidth / (float)width * 18.0f;
		m_iScrollbarSize = (int)size;
	}
	else
	{
		m_iScrollbarSize = 18;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SectionedListPanel::~SectionedListPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: sets whether or not the vertical scrollbar should ever be displayed
//-----------------------------------------------------------------------------
void SectionedListPanel::SetVerticalScrollbar( bool state )
{
	m_bVerticalScrollbarEnabled = state;
}

void SectionedListPanel::SetSectionFgColor( int sectionID, Color color )
{
	if ( sectionID >= 0 && sectionID < m_Sections.Count() )
	{
		m_Sections[sectionID].m_pHeader->m_SectionDividerColor = color;
		m_Sections[sectionID].m_pHeader->SetFgColor( color );
	}
}

void SectionedListPanel::SetSectionAlwaysVisible( int sectionID, bool visible )
{
	if ( sectionID >= 0 && sectionID < m_Sections.Count() )
		m_Sections[sectionID].m_bAlwaysVisible = visible;
}

//-----------------------------------------------------------------------------
// Purpose: returns -1 if section not found
//-----------------------------------------------------------------------------
int SectionedListPanel::FindSectionIndexByID( int sectionID )
{
	for ( int i = 0; i < m_Sections.Count(); i++ )
	{
		if ( m_Sections[i].m_iID == sectionID )
			return i;
	}
	return -1;
}

int SectionedListPanel::GetColumnCountBySection( int sectionID )
{
	int section = FindSectionIndexByID( sectionID );
	return section < 0 ? 0 : m_Sections[section].m_Columns.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of a column by section and column index; returns NULL if there are no more columns
//			valid range of columnIndex is [0, GetColumnCountBySection)
//-----------------------------------------------------------------------------
const char* SectionedListPanel::GetColumnNameBySection( int sectionID, int columnIndex )
{
	int section = FindSectionIndexByID( sectionID );
	if ( section < 0 || columnIndex >= m_Sections[section].m_Columns.Count() )
		return NULL;
	return m_Sections[section].m_Columns[columnIndex].m_szColumnName;
}

//-----------------------------------------------------------------------------
// Purpose: returns the text for a column by section and column index
//-----------------------------------------------------------------------------
const wchar_t* SectionedListPanel::GetColumnTextBySection( int sectionID, int columnIndex )
{
	int section = FindSectionIndexByID( sectionID );
	if ( section < 0 || columnIndex >= m_Sections[section].m_Columns.Count() )
		return NULL;
	return m_Sections[section].m_Columns[columnIndex].m_szColumnText;
}

//-----------------------------------------------------------------------------
// Purpose: returns the type of a column by section and column index
//-----------------------------------------------------------------------------
int SectionedListPanel::GetColumnFlagsBySection( int sectionID, int columnIndex )
{
	int section = FindSectionIndexByID( sectionID );
	if ( section < 0 || columnIndex >= m_Sections[section].m_Columns.Count() )
		return 0;
	return m_Sections[section].m_Columns[columnIndex].m_iColumnFlags;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int SectionedListPanel::GetColumnWidthBySection( int sectionID, int columnIndex )
{
	int section = FindSectionIndexByID( sectionID );
	if ( section < 0 || columnIndex >= m_Sections[section].m_Columns.Count() )
		return 0;
	return m_Sections[section].m_Columns[columnIndex].m_iWidth;
}

HFont SectionedListPanel::GetColumnFallbackFontBySection( int sectionID, int columnIndex )
{
	int section = FindSectionIndexByID( sectionID );
	if ( section < 0 || columnIndex >= m_Sections[section].m_Columns.Count() )
		return 0;
	return m_Sections[section].m_Columns[columnIndex].m_hFallbackFont;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the scrollbar is moved
//-----------------------------------------------------------------------------
void SectionedListPanel::OnSliderMoved()
{
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the itemID is valid for use
//-----------------------------------------------------------------------------
int SectionedListPanel::GetHighestItemID()
{
	return m_Items.MaxElementIndex();
}

//-----------------------------------------------------------------------------
// Purpose: item iterators
//-----------------------------------------------------------------------------
int SectionedListPanel::GetItemCount()
{
	return m_SortedItems.Count();
}

int SectionedListPanel::GetItemIDFromRow( int row )
{
	if ( row < 0 || row >= m_SortedItems.Count() )
		return -1;
	return m_SortedItems[row]->m_iID;
}

//-----------------------------------------------------------------------------
// Purpose: list used to match indexes in image columns to image pointers
//-----------------------------------------------------------------------------
void SectionedListPanel::SetImageList( ImageList* imageList, bool deleteImageListWhenDone )
{
	m_bDeleteImageListWhenDone = deleteImageListWhenDone;
	m_pImageList = imageList;
}

bool SectionedListPanel::IsSectionVisible( int sectionID )
{
	int section = FindSectionIndexByID( sectionID );
	return section >= 0 && m_Sections[section].m_pHeader->IsVisible();
}

void SectionedListPanel::SetProportional( bool state )
{
	BaseClass::SetProportional( state );
	for ( int i = 0; i < m_Sections.Count(); i++ )
		m_Sections[i].m_pHeader->SetProportional( state );
	for ( int i = m_Items.Head(); i != m_Items.InvalidIndex(); i = m_Items.Next( i ) )
		m_Items[i]->SetProportional( state );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we are currently in inline editing mode
//-----------------------------------------------------------------------------
bool SectionedListPanel::IsInEditMode()
{
	return m_hEditModePanel.Get() != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SectionedListPanel::OnSetFocus()
{
	if ( m_hSelectedItem.Get() )
		m_hSelectedItem->RequestFocus( 0 );
	else
		BaseClass::OnSetFocus();
}

int SectionedListPanel::GetSelectedItem()
{
	return m_hSelectedItem.Get() ? m_hSelectedItem->m_iID : -1;
}

//-----------------------------------------------------------------------------
// Purpose: leaves editing mode
//-----------------------------------------------------------------------------
void SectionedListPanel::LeaveEditMode()
{
	if ( m_hEditModePanel.Get() )
	{
		InvalidateItem( m_iEditModeItemID );
		m_hEditModePanel->SetVisible( false );
		m_hEditModePanel->SetParent( (Panel*)NULL );
		m_hEditModePanel = NULL;
	}
}

void SectionedListPanel::EnterEditMode( int itemID, int column, Panel* editPanel )
{
	m_hEditModePanel = editPanel;
	m_iEditModeItemID = itemID;
	m_iEditModeColumn = column;
	editPanel->SetParent( this );
	editPanel->SetVisible( true );
	editPanel->RequestFocus( 0 );
	editPanel->MoveToFront();
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Resets the scrollbar position on size change
//-----------------------------------------------------------------------------
void SectionedListPanel::OnSizeChanged( int wide, int tall )
{
	BaseClass::OnSizeChanged( wide, tall );
	m_pScrollBar->SetValue( 0 );
	InvalidateLayout( false, false );
	Repaint();
}

bool SectionedListPanel::ModifyColumn( int sectionID, const char* columnName, const wchar_t* columnText, int width )
{
	int section = FindSectionIndexByID( sectionID );
	if ( section < 0 )
		return false;
	for ( int i = 0; i < m_Sections[section].m_Columns.Count(); i++ )
	{
		column_t& column = m_Sections[section].m_Columns[i];
		if ( _stricmp( column.m_szColumnName, columnName ) )
			continue;
		if ( columnText || column.m_iWidth != width )
		{
			if ( width >= 0 )
				column.m_iWidth = width;
			if ( columnText )
			{
				wcsncpy( column.m_szColumnText, columnText, 64 );
				column.m_szColumnText[63] = 0;
			}
			m_Sections[section].m_pHeader->InvalidateLayout( false, false );
		}
		return true;
	}
	return false;
}

bool SectionedListPanel::AddColumnToSection( int sectionID, const char* columnName, const char* columnText,
	int columnFlags, int width, HFont fallbackFont )
{
	wchar_t buffer[64];
	wchar_t* text = localize()->Find( columnText );
	if ( !text )
	{
		localize()->ConvertANSIToUnicode( columnText, buffer, sizeof( buffer ) );
		text = buffer;
	}
	return AddColumnToSection( sectionID, columnName, text, columnFlags, width, fallbackFont );
}

bool SectionedListPanel::AddColumnToSection( int sectionID, const char* columnName, const wchar_t* columnText,
	int columnFlags, int width, HFont fallbackFont )
{
	int section = FindSectionIndexByID( sectionID );
	if ( section < 0 )
		return false;
	column_t& column = m_Sections[section].m_Columns[m_Sections[section].m_Columns.AddToTail()];
	strncpy( column.m_szColumnName, columnName, sizeof( column.m_szColumnName ) );
	column.m_szColumnName[31] = 0;
	wcsncpy( column.m_szColumnText, columnText, 64 );
	column.m_szColumnText[63] = 0;
	column.m_iColumnFlags = columnFlags;
	column.m_iWidth = width;
	column.m_hFallbackFont = fallbackFont;
	return true;
}

void SectionedListPanel::ApplySchemeSettings( IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetBgColor( GetSchemeColor( "BuddyListBgColor", GetBgColor(), scheme ) );
	SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
}

void SectionedListPanel::OnMouseWheeled( int delta )
{
	if ( IsInEditMode() )
		CallParentFunction( new KeyValues( "MouseWheeled", "delta", delta ) );
	else
		m_pScrollBar->SetValue( m_pScrollBar->GetValue() - delta * 60 );
}

void SectionedListPanel::ApplySettings( KeyValues* resourceData )
{
	BaseClass::ApplySettings( resourceData );
	m_iLineSpacing = resourceData->GetInt( "linespacing", 0 );
	if ( !m_iLineSpacing )
		m_iLineSpacing = 20;
	if ( IsProportional() )
		m_iLineSpacing = GetProportionalScaledValue( m_iLineSpacing );
}

//-----------------------------------------------------------------------------
// Purpose: forces an item to redraw
//-----------------------------------------------------------------------------
void SectionedListPanel::InvalidateItem( int itemID )
{
	if ( IsItemIDValid( itemID ) )
	{
		m_Items[itemID]->InvalidateLayout( false, false );
		m_Items[itemID]->Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the itemID is valid for use
//-----------------------------------------------------------------------------
bool SectionedListPanel::IsItemIDValid( int itemID )
{
	return m_Items.IsValidIndex( itemID );
}

int SectionedListPanel::GetItemSection( int itemID )
{
	return m_Items.IsValidIndex( itemID ) ? m_Items[itemID]->m_iSectionID : -1;
}

//-----------------------------------------------------------------------------
// Purpose: returns the data of a selected item
//-----------------------------------------------------------------------------
KeyValues* SectionedListPanel::GetItemData( int itemID )
{
	ASSERT_AT( m_Items.IsValidIndex(itemID), 1512 );
	if ( !m_Items.IsValidIndex( itemID ) )
		return NULL;

	return m_Items[itemID]->m_pData;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SectionedListPanel::SetItemFgColor( int itemID, Color color )
{
	ASSERT_AT( m_Items.IsValidIndex(itemID), 1106 );
	if ( m_Items.IsValidIndex( itemID ) )
	{
		m_Items[itemID]->SetFgColor( color );
		m_Items[itemID]->SetOverrideColors( true );
		m_Items[itemID]->InvalidateLayout( false, false );
	}
}

bool SectionedListPanel::ModifyItem( int itemID, int sectionID, const KeyValues* data )
{
	if ( !m_Items.IsValidIndex( itemID ) )
		return false;
	InvalidateLayout( false, false );
	CItemButton* item = m_Items[itemID];
	if ( sectionID != item->m_iSectionID )
	{
		item->ClearImages();
		for ( int i = 0; i < item->m_TextImages.Count(); i++ )
			delete item->m_TextImages[i];
		item->m_TextImages.RemoveAll();
		item->InvalidateLayout( false, false );
	}
	item->m_iSectionID = sectionID;
	if ( item->m_pData )
		item->m_pData->deleteThis();
	item->m_pData = data->MakeCopy();
	item->InvalidateLayout( false, false );
	item->InvalidateLayout( false, false );
	m_bSortNeeded = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: removes an item from the list; returns false if the item does not exist or is already removed
//-----------------------------------------------------------------------------
bool SectionedListPanel::RemoveItem( int itemID )
{
	if ( !m_Items.IsValidIndex( itemID ) )
		return false;
	for ( int i = 0; i < m_SortedItems.Count(); i++ )
	{
		if ( m_SortedItems[i] == m_Items[itemID] )
		{
			m_SortedItems.Remove( i );
			break;
		}
	}
	m_bSortNeeded = true;
	m_Items[itemID]->MarkForDeletion();
	m_Items.Remove( itemID );
	InvalidateLayout( false, false );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Clears the list
//-----------------------------------------------------------------------------
void SectionedListPanel::DeleteAllItems()
{
	for ( int i = m_Items.Head(); i != m_Items.InvalidIndex(); i = m_Items.Next( i ) )
	{
		CItemButton* item = m_Items[i];
		item->SetVisible( false );
		item->m_bSelected = false;
		item->m_bOverrideColors = false;
		item->m_iSectionID = -1;
		item->SetPaintBackgroundEnabled( false );
		item->SetTextImageIndex( -1 );
		item->ClearImages();
		m_FreeItems.AddToTail( item );
	}
	m_Items.RemoveAll();
	m_SortedItems.RemoveAll();
	m_hSelectedItem = NULL;
	InvalidateLayout( false, false );
	m_bSortNeeded = true;
}

int SectionedListPanel::GetNewItemButton()
{
	int itemID = m_Items.AddToTail();
	if ( m_FreeItems.Count() )
	{
		int freeItem = m_FreeItems.Head();
		m_Items[itemID] = m_FreeItems[freeItem];
		m_Items[itemID]->m_iID = itemID;
		m_Items[itemID]->SetVisible( true );
		m_FreeItems.Remove( freeItem );
	}
	else
	{
		m_Items[itemID] = new CItemButton( this, itemID );
	}
	return itemID;
}

int SectionedListPanel::AddItem( int sectionID, const KeyValues* data )
{
	int itemID = GetNewItemButton();
	ModifyItem( itemID, sectionID, data );

	// not sorted but in list
	m_SortedItems.AddToTail( m_Items[itemID] );
	m_bSortNeeded = true;
	return itemID;
}

void SectionedListPanel::AddSectionHelper( int sectionID, CSectionHeader* header, SectionSortFunc_t sortFunc )
{
	section_t& section = m_Sections[m_Sections.AddToTail()];
	section.m_iID = sectionID;
	section.m_pHeader = header;
	section.m_pSortFunc = sortFunc;
	section.m_bAlwaysVisible = false;
}

void SectionedListPanel::AddSection( int sectionID, const char* name, SectionSortFunc_t sortFunc )
{
	AddSectionHelper( sectionID, new CSectionHeader( this, sectionID, name ), sortFunc );
}

void SectionedListPanel::AddSection( int sectionID, const wchar_t* name, SectionSortFunc_t sortFunc )
{
	AddSectionHelper( sectionID, new CSectionHeader( this, sectionID, name ), sortFunc );
}

//-----------------------------------------------------------------------------
// Purpose: removes all the sections from the current panel
//-----------------------------------------------------------------------------
void SectionedListPanel::RemoveAllSections()
{
	for ( int i = 0; i < m_Sections.Count(); i++ )
	{
		m_Sections[i].m_pHeader->SetVisible( false );
		m_Sections[i].m_pHeader->MarkForDeletion();
	}
	m_Sections.Purge();
	m_SortedItems.RemoveAll();
	InvalidateLayout( false, false );
	ReSortList();
}

void SectionedListPanel::RemoveAll()
{
	DeleteAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: Changes the current list selection
//-----------------------------------------------------------------------------
void SectionedListPanel::SetSelectedItem( CItemButton* item )
{
	if ( item == m_hSelectedItem.Get() )
		return;
	if ( m_hSelectedItem.Get() && m_hSelectedItem->m_bSelected )
	{
		m_hSelectedItem->m_bSelected = false;
		m_hSelectedItem->SetPaintBackgroundEnabled( false );
		m_hSelectedItem->InvalidateLayout( false, false );
		m_hSelectedItem->Repaint();
	}

	// set the new item
	m_hSelectedItem = item;
	if ( m_hSelectedItem.Get() && !m_hSelectedItem->m_bSelected )
	{
		m_hSelectedItem->RequestFocus( 0 );
		m_hSelectedItem->m_bSelected = true;
		m_hSelectedItem->SetPaintBackgroundEnabled( true );
		m_hSelectedItem->InvalidateLayout( false, false );
		m_hSelectedItem->Repaint();
	}
	Repaint();
	PostActionSignal( new KeyValues( "ItemSelected", "itemID", m_hSelectedItem.Get() ? m_hSelectedItem->m_iID : -1 ) );
}

//-----------------------------------------------------------------------------
// Purpose: sets which item is currently selected
//-----------------------------------------------------------------------------
void SectionedListPanel::SetSelectedItem( int itemID )
{
	if ( m_Items.IsValidIndex( itemID ) )
		SetSelectedItem( m_Items[itemID] );
}

void SectionedListPanel::OnMousePressed( MouseCode )
{
	SetSelectedItem( (CItemButton*)NULL );
}

int SectionedListPanel::GetSectionTall()
{
	if ( m_Sections.Count() && m_Sections[0].m_pHeader->GetFont() )
		return surface()->GetFontTall( m_Sections[0].m_pHeader->GetFont() ) + 7;
	return 20;
}

void SectionedListPanel::OverrideLineSpacing( int spacing )
{
	if ( IsProportional() )
		spacing = GetProportionalScaledValue( spacing );
	if ( m_iLineSpacingOverride != spacing )
	{
		m_iLineSpacingOverride = spacing;
		PerformLayout();
	}
}

int SectionedListPanel::GetLineSpacing() const
{
	return m_iLineSpacingOverride ? m_iLineSpacingOverride : m_iLineSpacing;
}

void SectionedListPanel::ReSortList()
{
	m_SortedItems.RemoveAll();
	for ( int section = 0; section < m_Sections.Count(); section++ )
	{
		int sectionStart = m_SortedItems.Count();
		for ( int itemID = m_Items.Head(); itemID != m_Items.InvalidIndex(); itemID = m_Items.Next( itemID ) )
		{
			if ( m_Items[itemID]->m_iSectionID != m_Sections[section].m_iID )
				continue;
			int insertionPoint = m_SortedItems.Count();
			if ( m_Sections[section].m_pSortFunc )
			{
				for ( insertionPoint = sectionStart; insertionPoint < m_SortedItems.Count(); insertionPoint++ )
				{
					if ( m_Sections[section].m_pSortFunc( this, itemID, m_SortedItems[insertionPoint]->m_iID ) )
						break;
				}
			}
			m_SortedItems.InsertBefore( insertionPoint, m_Items[itemID] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the size required to fully draw the contents of the panel
//-----------------------------------------------------------------------------
void SectionedListPanel::GetContentSize( int& wide, int& tall )
{
	// make sure our layout is done
	if ( IsLayoutInvalid() )
	{
		if ( m_bSortNeeded )
		{
			ReSortList();
			m_bSortNeeded = false;
		}
		LayoutPanels( m_iContentHeight );
	}
	wide = GetWide();
	tall = m_iContentHeight;
}

//-----------------------------------------------------------------------------
// Purpose: iterates through and sets up the position of all the sections and items
//-----------------------------------------------------------------------------
void SectionedListPanel::PerformLayout()
{
	// lazy resort the list
	if ( m_bSortNeeded )
	{
		ReSortList();
		m_bSortNeeded = false;
	}
	BaseClass::PerformLayout();
	LayoutPanels( m_iContentHeight );
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	if ( m_iContentHeight > tall && m_bVerticalScrollbarEnabled )
	{
		m_pScrollBar->SetVisible( true );
		m_pScrollBar->MoveToFront();
		m_pScrollBar->SetPos( wide - m_iScrollbarSize - 2, 0 );
		m_pScrollBar->SetSize( m_iScrollbarSize, tall - 2 );
		m_pScrollBar->SetRangeWindow( tall );
		m_pScrollBar->SetRange( 0, m_iContentHeight );
		m_pScrollBar->InvalidateLayout( false, false );
		m_pScrollBar->Repaint();

		// since we're just about to make the scrollbar visible, we need to re-layout
		// the buttons since they depend on the scrollbar size
		LayoutPanels( m_iContentHeight );
	}
	else
	{
		m_pScrollBar->SetValue( 0 );
		m_pScrollBar->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: gets the local coordinates of a cell
//-----------------------------------------------------------------------------
bool SectionedListPanel::GetCellBounds( int itemID, int column, int& x, int& y, int& wide, int& tall )
{
	x = y = wide = tall = 0;
	if ( !IsItemIDValid( itemID ) || !m_Items[itemID]->IsVisible() )
		return false;
	CItemButton* item = m_Items[itemID];

	//!! ignores column for now
	item->GetBounds( x, y, wide, tall );
	x = wide = 0;
	int columns = item->m_pListPanel->GetColumnCountBySection( item->m_iSectionID );
	for ( int i = 0; i < columns; i++ )
	{
		int columnWide = item->m_pListPanel->GetColumnWidthBySection( item->m_iSectionID, i );
		IImage* image = item->GetImageAtIndex( i );
		if ( !image )
			continue;
		int imageWide, imageTall;
		image->GetContentSize( imageWide, imageTall );
		if ( columnWide < 0 )
			columnWide = imageWide;
		if ( i == column )
		{
			wide = columnWide;
			return true;
		}
		x += columnWide;
	}
	return true;
}

void SectionedListPanel::ScrollToItem( int itemID )
{
	int sectionTall = GetSectionTall();
	int scroll = m_pScrollBar->GetValue();
	int itemX, itemY;
	m_Items[itemID]->GetPos( itemX, itemY );
	itemY += scroll;
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	if ( m_iContentHeight <= tall )
		m_pScrollBar->SetValue( 0 );
	else if ( scroll > itemY )
		m_pScrollBar->SetValue( itemY );
	else if ( itemY > tall + scroll - sectionTall )
		m_pScrollBar->SetValue( itemY - tall + sectionTall );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: lays out the sections and rows in the panel
//-----------------------------------------------------------------------------
void SectionedListPanel::LayoutPanels( int& contentTall )
{
	int sectionTall = GetSectionTall();
	int wide = GetWide() - 10;
	int y = 5;
	if ( m_pScrollBar->IsVisible() )
	{
		y -= m_pScrollBar->GetValue();
		wide -= m_pScrollBar->GetWide();
	}
	for ( int sectionIndex = 0; sectionIndex < m_Sections.Count(); sectionIndex++ )
	{
		section_t& section = m_Sections[sectionIndex];
		int first = -1;
		int last = -1;
		for ( int i = 0; i < m_SortedItems.Count(); i++ )
		{
			if ( m_SortedItems[i]->m_iSectionID == section.m_iID )
			{
				if ( first == -1 )
					first = i;
				last = i;
			}
		}

		// don't draw this section at all if their are no item in it
		if ( first == -1 && !section.m_bAlwaysVisible )
		{
			section.m_pHeader->SetVisible( false );
			continue;
		}

		// draw the header
		section.m_pHeader->SetBounds( 5, y, wide, sectionTall );
		section.m_pHeader->SetVisible( true );
		y += sectionTall;
		if ( first != -1 )
		{
			for ( int i = first; i <= last; i++ )
			{
				CItemButton* item = m_SortedItems[i]; //items[i];
				item->SetBounds( 5, y, wide, GetLineSpacing() );
				if ( m_hEditModePanel.Get() && m_iEditModeItemID == item->m_iID )
				{
					int cellX = 0;
					int cellWide = 0;
					int columns = item->m_pListPanel->GetColumnCountBySection( item->m_iSectionID );
					for ( int column = 0; column < columns; column++ )
					{
						int columnWide = item->m_pListPanel->GetColumnWidthBySection( item->m_iSectionID, column );
						IImage* image = item->GetImageAtIndex( column );
						if ( !image )
							continue;
						int imageWide, imageTall;
						image->GetContentSize( imageWide, imageTall );
						if ( columnWide < 0 )
							columnWide = imageWide;
						if ( column == 1 )
						{
							cellWide = columnWide;
							break;
						}
						cellX += columnWide;
					}
					m_hEditModePanel->SetBounds( cellX, y, cellWide, sectionTall );
				}
				y += GetLineSpacing();
			}
		}

		// add in a little boundry at the bottom
		y += 8;
	}

	// calculate height
	contentTall = y;
	if ( m_pScrollBar->IsVisible() )
		contentTall += m_pScrollBar->GetValue();
}

//-----------------------------------------------------------------------------
// Purpose: arrow key movement handler
//-----------------------------------------------------------------------------
void SectionedListPanel::OnKeyCodeTyped( KeyCode code )
{
	if ( m_hEditModePanel.Get() )
	{
		CallParentFunction( new KeyValues( "KeyCodeTyped", "code", code ) );
		return;
	}

	int buttonTall = GetSectionTall();

	if ( code == KEY_DOWN )
	{
		int itemID = GetSelectedItem();
		ASSERT_AT( itemID != -1, 1303 );
		if ( !m_SortedItems.Count() ) // if the list has been emptied
			return;

		int i;
		for ( i = 0; i < m_SortedItems.Count(); i++ )
		{
			if ( m_SortedItems[i]->m_iID == itemID )
				break;
		}

		ASSERT_AT( i != m_SortedItems.Count(), 1315 );
		if ( i == m_SortedItems.Count() )
			return;

		// we're already on the end
		if ( i == m_SortedItems.Count() - 1 )
			return;

		int newItemID = m_SortedItems[i + 1]->m_iID;
		SetSelectedItem( m_Items[newItemID] );
		ScrollToItem( newItemID );
	}
	else if ( code == KEY_UP )
	{
		int itemID = GetSelectedItem();
		ASSERT_AT( itemID != -1, 1329 );
		if ( !m_SortedItems.Count() ) // if the list has been emptied
			return;

		int i;
		for ( i = 0; i < m_SortedItems.Count(); i++ )
		{
			if ( m_SortedItems[i]->m_iID == itemID )
				break;
		}

		ASSERT_AT( i != m_SortedItems.Count(), 1341 );

		if ( i == 0 )
			return;

		int newItemID = m_SortedItems[i - 1]->m_iID;
		SetSelectedItem( m_Items[newItemID] );
		ScrollToItem( newItemID );
	}
	else if ( code == KEY_PAGEDOWN )
	{
		// calculate info for # of rows
		int cx, cy, cwide, ctall;
		GetBounds( cx, cy, cwide, ctall );
		int rowsperpage = ctall / buttonTall;

		int itemID = GetSelectedItem();
		if ( !m_Items.IsValidIndex( itemID ) )
			return;
		int lastValidItem = itemID;
		int secID = m_Items[itemID]->m_iSectionID;
		int i = 0;
		int row = m_SortedItems.Find( m_Items[itemID] );

		while ( i < rowsperpage )
		{
			if ( m_SortedItems.IsValidIndex( ++row ) )
			{
				itemID = m_SortedItems[row]->m_iID;
				lastValidItem = itemID;
				i++;

				// if we switched sections, then count the section header as a row
				if ( m_Items[itemID]->m_iSectionID != secID )
				{
					secID = m_Items[itemID]->m_iSectionID;
					i++;
				}
			}
			else
			{
				itemID = lastValidItem;
				break;
			}
		}
		SetSelectedItem( m_Items[itemID] );
		ScrollToItem( itemID );
	}
	else if ( code == KEY_PAGEUP )
	{
		// calculate info for # of rows
		int cx, cy, cwide, ctall;
		GetBounds( cx, cy, cwide, ctall );
		int rowsperpage = ctall / buttonTall;

		int itemID = GetSelectedItem();
		if ( !m_Items.IsValidIndex( itemID ) )
			return;
		int lastValidItem = itemID;
		int secID = m_Items[itemID]->m_iSectionID;
		int i = 0;
		int row = m_SortedItems.Find( m_Items[itemID] );
		while ( i < rowsperpage )
		{
			if ( m_SortedItems.IsValidIndex( --row ) )
			{
				itemID = m_SortedItems[row]->m_iID;
				lastValidItem = itemID;
				i++;

				// if we switched sections, then count the section header as a row
				if ( m_Items[itemID]->m_iSectionID != secID )
				{
					secID = m_Items[itemID]->m_iSectionID;
					i++;
				}
			}
			else
			{
				SetSelectedItem( m_Items[lastValidItem] );
				m_pScrollBar->SetValue( 0 );
				return;
			}
		}
		SetSelectedItem( m_Items[itemID] );
		ScrollToItem( itemID );
	}
	else if ( code == KEY_LEFT || code == KEY_RIGHT )
	{
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}

CItemButton::~CItemButton()
{
	if ( m_pData )
		m_pData->deleteThis();
	if ( m_iSectionID != -1 )
	{
		ClearImages();
		for ( int i = 0; i < m_TextImages.Count(); i++ )
			delete m_TextImages[i];
		m_TextImages.RemoveAll();
		InvalidateLayout( false, false );
	}
	m_iSectionID = -1;
}

void CItemButton::SetOverrideColors( bool state )
{
	m_bOverrideColors = state;
}

void CItemButton::OnSetFocus()
{
	InvalidateLayout( false, false );
	Panel::OnSetFocus();
}

void CItemButton::OnKillFocus()
{
	InvalidateLayout( false, false );
	Panel::OnSetFocus();
}

void CItemButton::OnMousePressed( MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		m_pListPanel->PostActionSignal( new KeyValues( "ItemLeftClick", "itemID", m_iID ) );
	}
	else if ( code == MOUSE_RIGHT )
	{
		KeyValues* message = new KeyValues( "ItemContextMenu", "itemID", m_iID );
		message->SetPtr( "SubPanel", this );
		m_pListPanel->PostActionSignal( message );
	}
	m_pListPanel->SetSelectedItem( this );
}

void CItemButton::OnMouseDoublePressed( MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		m_pListPanel->PostActionSignal( new KeyValues( "ItemDoubleLeftClick", "itemID", m_iID ) );
		m_pListPanel->OnKeyCodeTyped( KEY_ENTER );
	}
	else
	{
		OnMousePressed( code );
	}
	m_pListPanel->SetSelectedItem( this );
}

void CItemButton::ApplySchemeSettings( IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_ArmedFgColor1 = GetSchemeColor( "BuddyButton/ArmedFgColor1", scheme );
	m_ArmedFgColor2 = GetSchemeColor( "BuddyButton/ArmedFgColor2", scheme );
	m_ArmedBgColor = GetSchemeColor( "BuddyButton/ArmedBgColor", scheme );
	m_FgColor1 = GetSchemeColor( "BuddyButton/FgColor1", scheme );
	m_FgColor2 = GetSchemeColor( "BuddyButton/FgColor2", scheme );
	m_BgColor = GetSchemeColor( "BuddyListBgColor", GetBgColor(), scheme );
	m_SelectionBG2Color = GetSchemeColor( "SelectionBG2", scheme );
}

void CItemButton::PaintBackground()
{
	int wide, tall;
	GetSize( wide, tall );
	if ( !m_bSelected || m_pListPanel->IsInEditMode() )
	{
		surface()->DrawSetColor( GetBgColor() );
	}
	else
	{
		VPANEL focus = input()->GetFocus();
		if ( HasFocus() || ( focus && ipanel()->HasParent( focus, GetVParent() ) ) )
			surface()->DrawSetColor( m_ArmedBgColor );
		else
			surface()->DrawSetColor( m_SelectionBG2Color );
	}
	surface()->DrawFilledRect( 0, 0, wide, tall );
}

void CItemButton::PerformLayout()
{
	int columns = m_pListPanel->GetColumnCountBySection( m_iSectionID );
	if ( columns <= 0 || !m_pData )
	{
		SetText( "< unset >" );
		BaseClass::PerformLayout();
		return;
	}
	if ( columns != GetImageCount() )
	{
		for ( int i = 0; i < columns; i++ )
		{
			if ( m_pListPanel->GetColumnFlagsBySection( m_iSectionID, i ) & SectionedListPanel::COLUMN_IMAGE )
				continue;
			TextImage* image = new TextImage( "" );
			m_TextImages.AddToTail( image );
			image->SetFont( GetFont() );
			HFont fallback = m_pListPanel->GetColumnFallbackFontBySection( m_iSectionID, i );
			if ( fallback )
				image->SetUseFallbackFont( true, fallback );
			SetImageAtIndex( i, image, 0 );
		}
		for ( int i = GetImageCount(); i < columns; i++ )
			AddImage( NULL, 0 );
	}
	int xpos = 0;
	for ( int i = 0; i < columns; i++ )
	{
		const char* name = m_pListPanel->GetColumnNameBySection( m_iSectionID, i );
		int flags = m_pListPanel->GetColumnFlagsBySection( m_iSectionID, i );
		int columnWide = m_pListPanel->GetColumnWidthBySection( m_iSectionID, i );
		IImage* image = NULL;
		if ( flags & SectionedListPanel::COLUMN_IMAGE )
		{
			if ( !m_pListPanel->m_pImageList )
				ASSERT_AT( !("Images columns used in SectionedListPanel with no ImageList set"), 373 );
			int index = m_pData->GetInt( name, 0 );
			if ( m_pListPanel->m_pImageList->IsValidIndex( index ) )
			{
				image = m_pListPanel->m_pImageList->GetImage( index );
				SetImageAtIndex( i, image, 0 );
			}
		}
		else
		{
			ASSERT_AT( dynamic_cast<TextImage *>(GetImageAtIndex(i)) != NULL, 379 );
			TextImage* text = (TextImage*)GetImageAtIndex( i );
			if ( text )
			{
				text->SetText( m_pData->GetString( name, "" ) );
				text->ResizeImageToContentMaxWidth( columnWide );
				VPANEL focus = input()->GetFocus();
				bool selected = m_bSelected && ( HasFocus() || ( focus && ipanel()->HasParent( focus, GetVParent() ) ) );
				if ( m_bOverrideColors && !selected )
					text->SetColor( GetFgColor() );
				else if ( selected )
					text->SetColor( m_ArmedFgColor2 );
				else
					text->SetColor( flags & SectionedListPanel::COLUMN_BRIGHT ? m_ArmedFgColor1 : m_FgColor2 );
				image = text;
			}
		}
		int imageWide = 0;
		int imageTall = 0;
		if ( image )
			image->GetContentSize( imageWide, imageTall );
		if ( columnWide < 0 )
			columnWide = imageWide;
		if ( i == 0 && !( flags & SectionedListPanel::COLUMN_IMAGE ) )
		{
			SetImageBounds( i, xpos + 6, columnWide - 8 );
		}
		else if ( flags & SectionedListPanel::COLUMN_CENTER )
		{
			int offset = columnWide / 2 - imageWide / 2;
			SetImageBounds( i, xpos + offset, columnWide - offset - 2 );
		}
		else if ( flags & SectionedListPanel::COLUMN_RIGHT )
		{
			SetImageBounds( i, xpos + columnWide - imageWide, columnWide - 2 );
		}
		else
		{
			SetImageBounds( i, xpos, columnWide - 2 );
		}
		xpos += columnWide;
	}
	BaseClass::PerformLayout();
}

void CSectionHeader::Paint()
{
	BaseClass::Paint();
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	y = tall - 2;
	surface()->DrawSetColor( m_SectionDividerColor );
	surface()->DrawFilledRect( 1, y, GetWide() - 2, y + 1 );
}

void CSectionHeader::ApplySchemeSettings( IScheme* scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	SetFgColor( GetSchemeColor( "SectionTextColor", scheme ) );
	m_SectionDividerColor = GetSchemeColor( "SectionDividerColor", scheme );
	SetBgColor( GetSchemeColor( "BuddyListBgColor", GetBgColor(), scheme ) );
	SetFont( scheme->GetFont( "DefaultVerySmall", IsProportional() ) );
}

void CSectionHeader::PerformLayout()
{
	BaseClass::PerformLayout();
	int columns = m_pListPanel->GetColumnCountBySection( m_iSectionID );
	if ( columns != GetImageCount() )
	{
		for ( int i = 0; i < columns; i++ )
		{
			TextImage* image = NULL;
			if ( !( m_pListPanel->GetColumnFlagsBySection( m_iSectionID, i ) & SectionedListPanel::COLUMN_HEADER_IMAGE ) )
			{
				image = new TextImage( "" );
				image->SetFont( GetFont() );
				HFont fallback = m_pListPanel->GetColumnFallbackFontBySection( m_iSectionID, i );
				if ( fallback )
					image->SetUseFallbackFont( true, fallback );
				image->SetColor( GetFgColor() );
			}
			SetImageAtIndex( i, image, 0 );
		}
	}
	for ( int pass = 0; pass < 2; pass++ )
	{
		int xpos = 0;
		for ( int i = 0; i < columns; i++ )
		{
			int flags = m_pListPanel->GetColumnFlagsBySection( m_iSectionID, i );
			int columnWide = m_pListPanel->GetColumnWidthBySection( m_iSectionID, i );
			IImage* image = GetImageAtIndex( i );
			if ( !image )
			{
				xpos += columnWide;
				continue;
			}
			int contentWide, contentTall;
			image->GetContentSize( contentWide, contentTall );
			int maxWidth = columnWide;
			if ( !( flags & SectionedListPanel::COLUMN_RIGHT ) )
			{
				for ( int next = i + 1; next < columns; next++ )
				{
					int imageWide = 0;
					int imageTall = 0;
					IImage* nextImage = GetImageAtIndex( next );
					if ( nextImage )
						nextImage->GetContentSize( imageWide, imageTall );
					if ( !imageWide )
						maxWidth += m_pListPanel->GetColumnWidthBySection( m_iSectionID, next );
				}
			}
			int imageWide = maxWidth < 0 ? contentWide : maxWidth;
			if ( flags & SectionedListPanel::COLUMN_CENTER )
				SetImageBounds( i, xpos + imageWide / 2 - contentWide / 2, imageWide - contentWide - 2 );
			else if ( flags & SectionedListPanel::COLUMN_RIGHT )
				SetImageBounds( i, xpos + imageWide - contentWide, contentWide - 2 );
			else
				SetImageBounds( i, xpos, imageWide - 2 );
			xpos += columnWide;
			if ( !( flags & SectionedListPanel::COLUMN_HEADER_IMAGE ) )
			{
				ASSERT_AT( dynamic_cast<TextImage *>(image) != NULL, 204 );
				TextImage* textImage = (TextImage*)image;
				textImage->SetText( m_pListPanel->GetColumnTextBySection( m_iSectionID, i ) );
				textImage->ResizeImageToContentMaxWidth( maxWidth );
			}
		}
	}
}
}
