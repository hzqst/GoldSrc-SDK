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
#include "../../engine/controls.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/KeyValues.h"
#include "vgui/ILocalize.h"

#include <string.h>
#include <wchar.h>

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\SectionedListPanel.cpp" )
#else
#include <assert.h>
#define ASSERT_AT( expression, line )	assert( expression )
#endif

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: header label that seperates and names each section
//-----------------------------------------------------------------------------
class CSectionHeader : public Label
{
	DECLARE_CLASS_SIMPLE( CSectionHeader, Label );

public:
	CSectionHeader( SectionedListPanel* list, const char* name, int sectionID ) : Label( list, name, "" )
	{
		m_pListPanel = list;
		m_iSectionID = sectionID;
		SetTextImageIndex( -1 );
		SetProportionalScaleImages( false );
		ClearImages();
		SetPaintBackgroundEnabled( false );
		MakeReadyForUse();
	}

	CSectionHeader( SectionedListPanel* list, const wchar_t* name, int sectionID )
		: Label( list, "SectionHeader", "" )
	{
		SetText( name );
		SetVisible( false );
		m_pListPanel = list;
		m_iSectionID = sectionID;
		SetTextImageIndex( -1 );
		ClearImages();
		MakeReadyForUse();
	}

	void SetColor( Color color )
	{
		m_SectionDividerColor = color;
		SetFgColor( color );
	}

protected:
	void ApplySchemeSettings( IScheme* scheme ) override;
	void Paint() override;
	void PerformLayout() override;

private:
	int					m_iSectionID;
	Color				m_SectionDividerColor;
	SectionedListPanel*	m_pListPanel;
};

//-----------------------------------------------------------------------------
// Purpose: Individual items in the list
//-----------------------------------------------------------------------------
class CItemButton : public Label
{
	DECLARE_CLASS_SIMPLE( CItemButton, Label );

public:
	CItemButton( SectionedListPanel* list, int itemID ) : Label( list, NULL, "< item >" )
	{
		m_pListPanel = list;
		m_pData = NULL;
		m_iID = itemID;
		SetProportionalScaleImages( false );
		Clear(); // force the layout to be redone so we can change text color according to focus
		MakeReadyForUse(); // force the layout to be redone so we can change text color according to focus
	}

	~CItemButton() override
	{
		if ( m_pData )
			m_pData->deleteThis();
		SetSectionID( -1 );
	}

	void Clear()
	{
		m_bSelected = false;
		m_bOverrideColors = false;
		m_iSectionID = -1;
		SetPaintBackgroundEnabled( false );
		SetTextImageIndex( -1 );
		ClearImages();
	}

	int GetID() { return m_iID; }
	void SetID( int itemID ) { m_iID = itemID; }
	int GetSectionID() { return m_iSectionID; }
	void SetSectionID( int sectionID )
	{
		if ( sectionID != m_iSectionID )
		{
			// free any existing textimage list 
			ClearImages();
			for ( int i = 0; i < m_TextImages.Count(); i++ )
				delete m_TextImages[i];
			m_TextImages.RemoveAll();
			InvalidateLayout( false, false );
		}
		m_iSectionID = sectionID;
	}

	void SetData( const KeyValues* data )
	{
		if ( m_pData )
			m_pData->deleteThis();
		m_pData = data->MakeCopy();
		InvalidateLayout( false, false );
	}

	KeyValues* GetData() { return m_pData; }
	void SetSelected( bool state )
	{
		if ( m_bSelected == state )
			return;
		if ( state )
			RequestFocus( 0 );
		m_bSelected = state;
		SetPaintBackgroundEnabled( state );
		InvalidateLayout( false, false );
		Repaint();
	}

	bool IsSelected() { return m_bSelected; }
	void GetCellBounds( int column, int& x, int& wide );
	virtual void SetOverrideColors( bool state ) { m_bOverrideColors = state; }

protected:
	void PerformLayout() override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void PaintBackground() override;
	void OnMousePressed( MouseCode code ) override;
	void OnSetFocus() override
	{
		InvalidateLayout( false, false );
		Panel::OnKillFocus();
	}

	void OnKillFocus() override
	{
		InvalidateLayout( false, false );
		Panel::OnKillFocus();
	}
	void OnMouseDoublePressed( MouseCode code ) override;

private:
	SectionedListPanel*	m_pListPanel;
	int					m_iID;
	int					m_iSectionID;
	KeyValues*			m_pData;
	Color				m_FgColor1;
	Color				m_FgColor2;
	Color				m_BgColor;
	Color				m_ArmedFgColor1;
	Color				m_ArmedFgColor2;
	Color				m_ArmedBgColor;
	Color				m_SelectionBG2Color;
	CUtlVector<TextImage*>	m_TextImages;
	bool				m_bSelected;
	bool				m_bOverrideColors;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SectionedListPanel::SectionedListPanel( Panel* parent, const char* name ) : Panel( parent, name )
{
	m_pScrollBar = new ScrollBar( this, "SectionedScrollBar", true );
	m_pScrollBar->SetVisible( false );
	m_pScrollBar->AddActionSignalTarget( this );
	m_iEditModeItemID = 0;
	m_iEditModeColumn = 0;
	m_bSortNeeded = false;
	m_bVerticalScrollbarEnabled = true;
	m_iContentHeight = 0;
	m_iLineSpacing = 20;
	m_iLineSpacingOverride = 0;
	m_pImageList = NULL;
	m_bDeleteImageListWhenDone = false;
	if ( Panel::IsProportional() )
	{
		int width, height, screenWide, screenTall;
		GetProportionalBase( width, height );
		surface()->GetScreenSize( screenWide, screenTall );
		float scrollbarSize = (float)screenWide / (float)width * 18.0f;
		m_iScrollbarSize = (int)scrollbarSize;
	}
	else
	{
		m_iScrollbarSize = 18;
	}
}

SectionedListPanel::~SectionedListPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: adds a new section
//-----------------------------------------------------------------------------
void SectionedListPanel::AddSection( int sectionID, const char* name, SectionSortFunc_t sortFunc )
{
	AddSectionHelper( sectionID, new CSectionHeader( this, name, sectionID ), sortFunc );
}

//-----------------------------------------------------------------------------
// Purpose: adds a new section
//-----------------------------------------------------------------------------
void SectionedListPanel::AddSection( int sectionID, const wchar_t* name, SectionSortFunc_t sortFunc )
{
	AddSectionHelper( sectionID, new CSectionHeader( this, name, sectionID ), sortFunc );
}

//-----------------------------------------------------------------------------
// Purpose: helper function for AddSection
//-----------------------------------------------------------------------------
void SectionedListPanel::AddSectionHelper( int sectionID, CSectionHeader* header, SectionSortFunc_t sortFunc )
{
	int index = m_Sections.AddToTail();
	m_Sections[index].m_iID = sectionID;
	m_Sections[index].m_pHeader = header;
	m_Sections[index].m_pSortFunc = sortFunc;
	m_Sections[index].m_bAlwaysVisible = false;
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

//-----------------------------------------------------------------------------
// Purpose: sets whether or not the vertical scrollbar should ever be displayed
//-----------------------------------------------------------------------------
void SectionedListPanel::SetVerticalScrollbar( bool state )
{
	m_bVerticalScrollbarEnabled = state;
}

//-----------------------------------------------------------------------------
// Purpose: sets the color of a section text & underline
//-----------------------------------------------------------------------------
void SectionedListPanel::SetSectionFgColor( int sectionID, Color color )
{
	if ( sectionID >= 0 && sectionID < m_Sections.Count() )
		m_Sections[sectionID].m_pHeader->SetColor( color );
}

void SectionedListPanel::SetSectionAlwaysVisible( int sectionID, bool visible )
{
	if ( sectionID >= 0 && sectionID < m_Sections.Count() )
		m_Sections[sectionID].m_bAlwaysVisible = visible;
}

bool SectionedListPanel::IsSectionVisible( int sectionID )
{
	int index = FindSectionIndexByID( sectionID );
	if ( index == -1 )
		return false;
	return m_Sections[index].m_pHeader->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of columns in a section
//-----------------------------------------------------------------------------
int SectionedListPanel::GetColumnCountBySection( int sectionID )
{
	int index = FindSectionIndexByID( sectionID );
	return index == -1 ? 0 : m_Sections[index].m_Columns.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of a column by section and column index; returns NULL if there are no more columns
//			valid range of columnIndex is [0, GetColumnCountBySection)
//-----------------------------------------------------------------------------
const char* SectionedListPanel::GetColumnNameBySection( int sectionID, int columnIndex )
{
	int index = FindSectionIndexByID( sectionID );
	if ( index == -1 || columnIndex >= m_Sections[index].m_Columns.Count() )
		return NULL;
	return m_Sections[index].m_Columns[columnIndex].m_szColumnName;
}

//-----------------------------------------------------------------------------
// Purpose: returns the text for a column by section and column index
//-----------------------------------------------------------------------------
const wchar_t* SectionedListPanel::GetColumnTextBySection( int sectionID, int columnIndex )
{
	int index = FindSectionIndexByID( sectionID );
	if ( index == -1 || columnIndex >= m_Sections[index].m_Columns.Count() )
		return NULL;
	return m_Sections[index].m_Columns[columnIndex].m_szColumnText;
}

//-----------------------------------------------------------------------------
// Purpose: returns the type of a column by section and column index
//-----------------------------------------------------------------------------
int SectionedListPanel::GetColumnFlagsBySection( int sectionID, int columnIndex )
{
	int index = FindSectionIndexByID( sectionID );
	if ( index == -1 || columnIndex >= m_Sections[index].m_Columns.Count() )
		return 0;
	return m_Sections[index].m_Columns[columnIndex].m_iColumnFlags;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int SectionedListPanel::GetColumnWidthBySection( int sectionID, int columnIndex )
{
	int index = FindSectionIndexByID( sectionID );
	if ( index == -1 || columnIndex >= m_Sections[index].m_Columns.Count() )
		return 0;
	return m_Sections[index].m_Columns[columnIndex].m_iWidth;
}

//-----------------------------------------------------------------------------
// Purpose: Returns fallback font to use for text image for this column
// Input  : sectionID - 
//			columnIndex - 
// Output : virtual HFont
//-----------------------------------------------------------------------------
HFont SectionedListPanel::GetColumnFallbackFontBySection( int sectionID, int columnIndex )
{
	int index = FindSectionIndexByID( sectionID );
	if ( index == -1 || columnIndex >= m_Sections[index].m_Columns.Count() )
		return 0;
	return m_Sections[index].m_Columns[columnIndex].m_hFallbackFont;
}

//-----------------------------------------------------------------------------
// Purpose: as above but with wchar_t's
//-----------------------------------------------------------------------------
bool SectionedListPanel::AddColumnToSection( int sectionID, const char* columnName, const wchar_t* columnText,
	int columnFlags, int width, HFont fallbackFont )
{
	int index = FindSectionIndexByID( sectionID );
	if ( index == -1 )
		return false;
	CUtlVector<column_t>& columns = m_Sections[index].m_Columns;
	column_t& column = columns[columns.AddToTail()];
	strncpy( column.m_szColumnName, columnName, sizeof( column.m_szColumnName ) );
	column.m_szColumnName[sizeof( column.m_szColumnName ) - 1] = 0;
	wcsncpy( column.m_szColumnText, columnText, sizeof( column.m_szColumnText ) / sizeof( wchar_t ) );
	column.m_szColumnText[sizeof( column.m_szColumnText ) / sizeof( wchar_t ) - 1] = 0;
	column.m_iColumnFlags = columnFlags;
	column.m_iWidth = width;
	column.m_hFallbackFont = fallbackFont;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: adds a new column to a section
//-----------------------------------------------------------------------------
bool SectionedListPanel::AddColumnToSection( int sectionID, const char* columnName, const char* columnText,
	int columnFlags, int width, HFont fallbackFont )
{
	wchar_t text[64];
	wchar_t* localizedText = localize()->Find( columnText );
	if ( !localizedText )
	{
		localize()->ConvertANSIToUnicode( columnText, text, sizeof( text ) );
		localizedText = text;
	}
	return AddColumnToSection( sectionID, columnName, localizedText, columnFlags, width, fallbackFont );
}

//-----------------------------------------------------------------------------
// Purpose: modifies the text in an existing column
//-----------------------------------------------------------------------------
bool SectionedListPanel::ModifyColumn( int sectionID, const char* columnName, const wchar_t* columnText, int width )
{
	int index = FindSectionIndexByID( sectionID );
	if ( index == -1 )
		return false;
	section_t& section = m_Sections[index];
	for ( int i = 0; i < section.m_Columns.Count(); i++ )
	{
		column_t& column = section.m_Columns[i];
		if ( stricmp( column.m_szColumnName, columnName ) )
			continue;
		if ( columnText || column.m_iWidth != width )
		{
			if ( width >= 0 )
				column.m_iWidth = width;
			if ( columnText )
			{
				// modify the text
				wcsncpy( column.m_szColumnText, columnText, sizeof( column.m_szColumnText ) / sizeof( wchar_t ) );
				column.m_szColumnText[sizeof( column.m_szColumnText ) / sizeof( wchar_t ) - 1] = 0;
			}
			section.m_pHeader->InvalidateLayout( false, false );
		}
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the index of a new item button
//-----------------------------------------------------------------------------
int SectionedListPanel::GetNewItemButton()
{
	int itemID = m_Items.AddToTail();
	if ( m_FreeItems.Count() )
	{
		int freeItem = m_FreeItems.Head();
		m_Items[itemID] = m_FreeItems[freeItem];
		m_Items[itemID]->SetID( itemID );
		m_Items[itemID]->SetVisible( true );
		m_FreeItems.Remove( freeItem );
	}
	else
	{
		m_Items[itemID] = new CItemButton( this, itemID );
	}
	return itemID;
}

//-----------------------------------------------------------------------------
// Purpose: adds an item to the list; returns itemID
//-----------------------------------------------------------------------------
int SectionedListPanel::AddItem( int sectionID, const KeyValues* data )
{
	int itemID = GetNewItemButton();
	ModifyItem( itemID, sectionID, data );
	// not sorted but in list
	m_SortedItems.AddToTail( m_Items[itemID] );
	m_bSortNeeded = true;
	return itemID;
}

//-----------------------------------------------------------------------------
// Purpose: modifies an existing item; returns false if the item does not exist
//-----------------------------------------------------------------------------
bool SectionedListPanel::ModifyItem( int itemID, int sectionID, const KeyValues* data )
{
	if ( !m_Items.IsValidIndex( itemID ) )
		return false;
	InvalidateLayout( false, false );
	m_Items[itemID]->SetSectionID( sectionID );
	m_Items[itemID]->SetData( data );
	m_Items[itemID]->InvalidateLayout( false, false );
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
	m_SortedItems.FindAndRemove( m_Items[itemID] );
	m_bSortNeeded = true;
	m_Items[itemID]->MarkForDeletion();
	m_Items.Remove( itemID );
	InvalidateLayout( false, false );
	return true;
}

void SectionedListPanel::RemoveAll()
{
	DeleteAllItems();
}

void SectionedListPanel::DeleteAllItems()
{
	for ( int i = m_Items.Head(); i != m_Items.InvalidIndex(); i = m_Items.Next( i ) )
	{
		m_Items[i]->SetVisible( false );
		m_Items[i]->Clear();
		m_FreeItems.AddToTail( m_Items[i] );
	}
	m_Items.RemoveAll();
	m_SortedItems.RemoveAll();
	m_hSelectedItem = NULL;
	InvalidateLayout( false, false );
	m_bSortNeeded = true;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the itemID is valid for use
//-----------------------------------------------------------------------------
bool SectionedListPanel::IsItemIDValid( int itemID )
{
	return m_Items.IsValidIndex( itemID );
}

//-----------------------------------------------------------------------------
// Purpose: returns the data of a selected item
//-----------------------------------------------------------------------------
KeyValues* SectionedListPanel::GetItemData( int itemID )
{
	ASSERT_AT( m_Items.IsValidIndex(itemID), 1512 );
	if ( !m_Items.IsValidIndex( itemID ) )
		return NULL;

	return m_Items[itemID]->GetData();
}

//-----------------------------------------------------------------------------
// Purpose: returns what section an item is in
//-----------------------------------------------------------------------------
int SectionedListPanel::GetItemSection( int itemID )
{
	return m_Items.IsValidIndex( itemID ) ? m_Items[itemID]->GetSectionID() : -1;
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

//-----------------------------------------------------------------------------
// Purpose: item iterators
//-----------------------------------------------------------------------------
int SectionedListPanel::GetItemIDFromRow( int row )
{
	if ( row < 0 || row >= m_SortedItems.Count() )
		return -1;
	return m_SortedItems[row]->GetID();
}

//-----------------------------------------------------------------------------
// Purpose: list used to match indexes in image columns to image pointers
//-----------------------------------------------------------------------------
void SectionedListPanel::SetImageList( ImageList* imageList, bool deleteImageListWhenDone )
{
	m_bDeleteImageListWhenDone = deleteImageListWhenDone;
	m_pImageList = imageList;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SectionedListPanel::SetItemFgColor( int itemID, Color color )
{
	ASSERT_AT( m_Items.IsValidIndex(itemID), 1106 );
	if ( !m_Items.IsValidIndex( itemID ) )
		return;

	m_Items[itemID]->SetFgColor( color );
	m_Items[itemID]->SetOverrideColors( true );
	m_Items[itemID]->InvalidateLayout( false, false );
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

int SectionedListPanel::GetSelectedItem()
{
	return m_hSelectedItem.Get() ? m_hSelectedItem.Get()->GetID() : -1;
}

//-----------------------------------------------------------------------------
// Purpose: sets which item is currently selected
//-----------------------------------------------------------------------------
void SectionedListPanel::SetSelectedItem( int itemID )
{
	if ( m_Items.IsValidIndex( itemID ) )
		SetSelectedItem( m_Items[itemID] );
}

//-----------------------------------------------------------------------------
// Purpose: Changes the current list selection
//-----------------------------------------------------------------------------
void SectionedListPanel::SetSelectedItem( CItemButton* item )
{
	if ( item == m_hSelectedItem.Get() )
		return;
	if ( m_hSelectedItem.Get() )
		m_hSelectedItem.Get()->SetSelected( false );
	// set the new item
	m_hSelectedItem = item;
	if ( m_hSelectedItem.Get() )
		m_hSelectedItem.Get()->SetSelected( true );
	Repaint();
	int itemID = m_hSelectedItem.Get() ? m_hSelectedItem.Get()->GetID() : -1;
	PostActionSignal( new KeyValues( "ItemSelected", "itemID", itemID ) );
}

//-----------------------------------------------------------------------------
// Purpose: set up a field for editing
//-----------------------------------------------------------------------------
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
// Purpose: leaves editing mode
//-----------------------------------------------------------------------------
void SectionedListPanel::LeaveEditMode()
{
	if ( m_hEditModePanel.Get() )
	{
		InvalidateItem( m_iEditModeItemID );
		m_hEditModePanel.Get()->SetVisible( false );
		m_hEditModePanel.Get()->SetParent( (Panel*)NULL );
		m_hEditModePanel = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we are currently in inline editing mode
//-----------------------------------------------------------------------------
bool SectionedListPanel::IsInEditMode()
{
	return m_hEditModePanel.Get() != NULL;
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
// Purpose: passes on proportional state to children
//-----------------------------------------------------------------------------
void SectionedListPanel::SetProportional( bool state )
{
	Panel::SetProportional( state );
	for ( int i = 0; i < m_Sections.Count(); i++ )
		m_Sections[i].m_pHeader->SetProportional( state );
	for ( int i = m_Items.Head(); i != m_Items.InvalidIndex(); i = m_Items.Next( i ) )
		m_Items[i]->SetProportional( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SectionedListPanel::OnSetFocus()
{
	if ( m_hSelectedItem.Get() )
		m_hSelectedItem.Get()->RequestFocus( 0 );
	else
		Panel::OnKillFocus();
}

void SectionedListPanel::OnMouseWheeled( int delta )
{
	if ( m_hEditModePanel.Get() )
		CallParentFunction( new KeyValues( "MouseWheeled", "delta", delta ) );
	else
		m_pScrollBar->SetValue( m_pScrollBar->GetValue() - 60 * delta );
}

//-----------------------------------------------------------------------------
// Purpose: deselects any items
//-----------------------------------------------------------------------------
void SectionedListPanel::OnMousePressed( MouseCode )
{
	SetSelectedItem( (CItemButton*)NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int SectionedListPanel::GetSectionTall()
{
	if ( !m_Sections.Count() )
		return 20;
	HFont font = m_Sections[0].m_pHeader->GetFont();
	return font ? surface()->GetFontTall( font ) + 7 : 20;
}

void SectionedListPanel::OverrideLineSpacing( int spacing )
{
	if ( IsProportional() )
		spacing = GetProportionalScaledValue( spacing );
	if ( spacing != m_iLineSpacingOverride )
	{
		m_iLineSpacingOverride = spacing;
		PerformLayout();
	}
}

int SectionedListPanel::GetLineSpacing() const
{
	return m_iLineSpacingOverride ? m_iLineSpacingOverride : m_iLineSpacing;
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

//-----------------------------------------------------------------------------
// Purpose: Sorts the list
//-----------------------------------------------------------------------------
void SectionedListPanel::ReSortList()
{
	m_SortedItems.RemoveAll();

	int sectionStart = 0;
	// layout the buttons
	for ( int sectionIndex = 0; sectionIndex < m_Sections.Count(); sectionIndex++ )
	{
		section_t& section = m_Sections[sectionIndex];
		sectionStart = m_SortedItems.Count();

		// find all the items in this section
		for ( int i = m_Items.Head(); i != m_Items.InvalidIndex(); i = m_Items.Next( i ) )
		{
			if ( m_Items[i]->GetSectionID() == m_Sections[sectionIndex].m_iID )
			{
				// insert the items sorted
				if ( section.m_pSortFunc )
				{
					int insertionPoint = sectionStart;
					for ( ; insertionPoint < m_SortedItems.Count(); insertionPoint++ )
					{
						if ( section.m_pSortFunc( this, i, m_SortedItems[insertionPoint]->GetID() ) )
							break;
					}

					if ( insertionPoint == m_SortedItems.Count() )
					{
						m_SortedItems.AddToTail( m_Items[i] );
					}
					else
					{
						m_SortedItems.InsertBefore( insertionPoint, m_Items[i] );
					}
				}
				else
				{
					// just add to the end
					m_SortedItems.AddToTail( m_Items[i] );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets background color & border
//-----------------------------------------------------------------------------
void SectionedListPanel::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	SetBgColor( GetSchemeColor( "BuddyListBgColor", GetBgColor(), scheme ) );
	SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SectionedListPanel::ApplySettings( KeyValues* resourceData )
{
	Panel::ApplySettings( resourceData );
	m_iLineSpacing = resourceData->GetInt( "linespacing", 0 );
	if ( !m_iLineSpacing )
		m_iLineSpacing = 20;
	if ( IsProportional() )
		m_iLineSpacing = GetProportionalScaledValue( m_iLineSpacing );
}

//-----------------------------------------------------------------------------
// Purpose: Resets the scrollbar position on size change
//-----------------------------------------------------------------------------
void SectionedListPanel::OnSizeChanged( int wide, int tall )
{
	Panel::OnSizeChanged( wide, tall );
	m_pScrollBar->SetValue( 0 );
	InvalidateLayout( false, false );
	Repaint();
}

void CSectionHeader::ApplySchemeSettings( IScheme* scheme )
{
	Label::ApplySchemeSettings( scheme );
	SetFgColor( GetSchemeColor( "SectionTextColor", scheme ) );
	m_SectionDividerColor = GetSchemeColor( "SectionDividerColor", scheme );
	SetBgColor( GetSchemeColor( "BuddyListBgColor", GetBgColor(), scheme ) );
	SetFont( scheme->GetFontEx( "DefaultVerySmall", IsProportional(), IsHDProportional() ) );
}

void CSectionHeader::Paint()
{
	Label::Paint();
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	y = tall - 2;
	surface()->DrawSetColor( m_SectionDividerColor );
	surface()->DrawFilledRect( 1, y, GetWide() - 2, y + 1 );
}

void CSectionHeader::PerformLayout()
{
	Label::PerformLayout();
	int count = m_pListPanel->GetColumnCountBySection( m_iSectionID );
	if ( count != GetImageCount() )
	{
		for ( int i = 0; i < count; i++ )
		{
			TextImage* image = NULL;
			if ( !( m_pListPanel->GetColumnFlagsBySection( m_iSectionID, i ) & SectionedListPanel::COLUMN_HEADER_IMAGE ) )
			{
				image = new TextImage( "" );
				image->SetFont( GetFont() );
				HFont font = m_pListPanel->GetColumnFallbackFontBySection( m_iSectionID, i );
				if ( font )
					image->SetUseFallbackFont( true, font );
				image->SetColor( GetFgColor() );
			}
			SetImageAtIndex( i, image, 0 );
		}
	}
	for ( int pass = 0; pass < 2; pass++ )
	{
		int x = 0;
		for ( int i = 0; i < count; i++ )
		{
			int flags = m_pListPanel->GetColumnFlagsBySection( m_iSectionID, i );
			int columnWidth = m_pListPanel->GetColumnWidthBySection( m_iSectionID, i );
			IImage* image = GetImageAtIndex( i );
			if ( !image )
			{
				x += columnWidth;
				continue;
			}
			int wide, tall;
			image->GetContentSize( wide, tall );
			int contentWidth = wide;
			int maxWidth = columnWidth;
			if ( !( flags & SectionedListPanel::COLUMN_RIGHT ) )
			{
				for ( int j = i + 1; j < count; j++ )
				{
					int imageWide = 0, imageTall = 0;
					if ( GetImageAtIndex( j ) )
						GetImageAtIndex( j )->GetContentSize( imageWide, imageTall );
					if ( !imageWide )
						maxWidth += m_pListPanel->GetColumnWidthBySection( m_iSectionID, j );
				}
			}
			if ( maxWidth >= 0 )
				wide = maxWidth;
			if ( flags & SectionedListPanel::COLUMN_CENTER )
				SetImageBounds( i, x + wide / 2 - contentWidth / 2, wide - contentWidth - 2 );
			else if ( flags & SectionedListPanel::COLUMN_RIGHT )
				SetImageBounds( i, x + wide - contentWidth, contentWidth - 2 );
			else
				SetImageBounds( i, x, wide - 2 );
			x += columnWidth;
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

void CItemButton::ApplySchemeSettings( IScheme* scheme )
{
	Label::ApplySchemeSettings( scheme );
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
	int count = m_pListPanel->GetColumnCountBySection( m_iSectionID );
	if ( !m_pData || count <= 0 )
	{
		SetText( "< unset >" );
		Label::PerformLayout();
		return;
	}
	if ( count != GetImageCount() )
	{
		for ( int i = 0; i < count; i++ )
		{
			if ( m_pListPanel->GetColumnFlagsBySection( m_iSectionID, i ) & SectionedListPanel::COLUMN_IMAGE )
				continue;
			TextImage* image = new TextImage( "" );
			m_TextImages.AddToTail( image );
			image->SetFont( GetFont() );
			HFont font = m_pListPanel->GetColumnFallbackFontBySection( m_iSectionID, i );
			if ( font )
				image->SetUseFallbackFont( true, font );
			SetImageAtIndex( i, image, 0 );
		}

		for ( int i = GetImageCount(); i < count; i++ )
			AddImage( NULL, 0 );
	}
	int x = 0;
	for ( int i = 0; i < count; i++ )
	{
		const char* name = m_pListPanel->GetColumnNameBySection( m_iSectionID, i );
		int flags = m_pListPanel->GetColumnFlagsBySection( m_iSectionID, i );
		int width = m_pListPanel->GetColumnWidthBySection( m_iSectionID, i );
		bool imageColumn = ( flags & SectionedListPanel::COLUMN_IMAGE ) != 0;
		IImage* image = NULL;
		if ( imageColumn )
		{
			ImageList* imageList = m_pListPanel->m_pImageList;
			if ( imageList )
			{
				int imageIndex = m_pData->GetInt( name, 0 );
				if ( imageList->IsValidIndex( imageIndex ) )
				{
					image = imageList->GetImage( imageIndex );
					SetImageAtIndex( i, image, 0 );
				}
			}
			else
			{
				ASSERT_AT( !("Images columns used in SectionedListPanel with no ImageList set"), 373 );
			}
		}
		else
		{
			ASSERT_AT( dynamic_cast<TextImage *>(GetImageAtIndex(i)) != NULL, 379 );
			TextImage* textImage = (TextImage*)GetImageAtIndex( i );
			image = textImage;
			if ( textImage )
			{
				textImage->SetText( m_pData->GetString( name, "" ) );
				textImage->ResizeImageToContentMaxWidth( width );
				VPANEL focus = input()->GetFocus();
				if ( m_bOverrideColors )
				{
					if ( m_bSelected && ( HasFocus() || ( focus && ipanel()->HasParent( focus, GetVParent() ) ) ) )
						textImage->SetColor( m_ArmedFgColor2 );
					else
						textImage->SetColor( GetFgColor() );
				}
				else if ( m_bSelected && ( HasFocus() || ( focus && ipanel()->HasParent( focus, GetVParent() ) ) ) )
				{
					textImage->SetColor( m_ArmedFgColor2 );
				}
				else
				{
					textImage->SetColor( ( flags & SectionedListPanel::COLUMN_BRIGHT ) ? m_ArmedFgColor1 : m_FgColor2 );
				}
			}
		}
		int imageWide = 0, imageTall = 0;
		if ( image )
			image->GetContentSize( imageWide, imageTall );
		if ( width < 0 )
			width = imageWide;
		if ( i == 0 && !imageColumn )
			SetImageBounds( 0, x + 6, width - 8 );
		else if ( flags & SectionedListPanel::COLUMN_CENTER )
			SetImageBounds( i, x + width / 2 - imageWide / 2, width - ( width / 2 - imageWide / 2 ) - 2 );
		else if ( flags & SectionedListPanel::COLUMN_RIGHT )
			SetImageBounds( i, x + width - imageWide, width - 2 );
		else
			SetImageBounds( i, x, width - 2 );
		x += width;
	}
	Label::PerformLayout();
}

void CItemButton::GetCellBounds( int column, int& x, int& wide )
{
	x = 0;
	wide = 0;
	int count = m_pListPanel->GetColumnCountBySection( m_iSectionID );
	for ( int i = 0; i < count; i++ )
	{
		int width = m_pListPanel->GetColumnWidthBySection( m_iSectionID, i );
		IImage* image = GetImageAtIndex( i );
		if ( !image )
			continue;
		int imageWide, imageTall;
		image->GetContentSize( imageWide, imageTall );
		if ( width < 0 )
			width = imageWide;
		if ( column == i )
		{
			wide = width;
			return;
		}
		x += width;
	}
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

//-----------------------------------------------------------------------------
// Purpose: gets the local coordinates of a cell
//-----------------------------------------------------------------------------
bool SectionedListPanel::GetCellBounds( int itemID, int column, int& x, int& y, int& wide, int& tall )
{
	tall = 0;
	wide = 0;
	y = 0;
	x = 0;
	if ( !IsItemIDValid( itemID ) )
		return false;
	// get the item
	CItemButton* item = m_Items[itemID];
	if ( !item->IsVisible() )
		return false;
	//!! ignores column for now
	item->GetBounds( x, y, wide, tall );
	item->GetCellBounds( column, x, wide );
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
		m_pScrollBar->SetValue( sectionTall + itemY - tall );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: lays out the sections and rows in the panel
//-----------------------------------------------------------------------------
void SectionedListPanel::LayoutPanels( int& contentTall )
{
	int tall = GetSectionTall();
	int x = 5, wide = GetWide() - 10;
	int y = 5;

	if ( m_pScrollBar->IsVisible() )
	{
		y -= m_pScrollBar->GetValue();
		wide -= m_pScrollBar->GetWide();
	}

	int iStart = -1;
	int iEnd = -1;

	// layout the buttons
	for ( int sectionIndex = 0; sectionIndex < m_Sections.Count(); sectionIndex++ )
	{
		section_t& section = m_Sections[sectionIndex];

		iStart = -1;
		iEnd = -1;
		for ( int i = 0; i < m_SortedItems.Count(); i++ )
		{
			if ( m_SortedItems[i]->GetSectionID() == m_Sections[sectionIndex].m_iID )
			{
				if ( iStart == -1 )
					iStart = i;
				iEnd = i;
			}
		}

		// don't draw this section at all if their are no item in it
		if ( iStart == -1 && !section.m_bAlwaysVisible )
		{
			section.m_pHeader->SetVisible( false );
			continue;
		}

		// draw the header
		section.m_pHeader->SetBounds( x, y, wide, tall );
		section.m_pHeader->SetVisible( true );
		y += tall;

		if ( iStart == -1 && section.m_bAlwaysVisible )
		{
		}
		else
		{
			// arrange all the items in this section underneath
			for ( int i = iStart; i <= iEnd; i++ )
			{
				CItemButton* item = m_SortedItems[i]; //items[i];
				item->SetBounds( x, y, wide, GetLineSpacing() );

				// setup edit mode
				if ( m_hEditModePanel.Get() && m_iEditModeItemID == item->GetID() )
				{
					int cx, cwide;
					item->GetCellBounds( 1, cx, cwide );
					m_hEditModePanel.Get()->SetBounds( cx, y, cwide, tall );
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
	{
		contentTall += m_pScrollBar->GetValue();
	}
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

	int cx, cy, cwide, ctall;
	GetBounds( cx, cy, cwide, ctall );
	if ( m_iContentHeight > ctall && m_bVerticalScrollbarEnabled )
	{
		m_pScrollBar->SetVisible( true );
		m_pScrollBar->MoveToFront();
		m_pScrollBar->SetPos( cwide - m_iScrollbarSize - 2, 0 );
		m_pScrollBar->SetSize( m_iScrollbarSize, ctall - 2 );
		m_pScrollBar->SetRangeWindow( ctall );
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
// Purpose: arrow key movement handler
//-----------------------------------------------------------------------------
void SectionedListPanel::OnKeyCodeTyped( KeyCode code )
{
	if ( m_hEditModePanel.Get() )
	{
		// ignore arrow keys in edit mode
		// forward right up to parent so that tab focus change doesn't occur
		CallParentFunction( new KeyValues( "KeyCodeTyped", "code", code ) );
		return;
	}
	int sectionTall = GetSectionTall();
	if ( code == KEY_DOWN )
	{
		int itemID = GetSelectedItem();
		ASSERT_AT( itemID != -1, 1303 );
		if ( !m_SortedItems.Count() )	// if the list has been emptied
			return;

		int i;
		for ( i = 0; i < m_SortedItems.Count(); i++ )
		{
			if ( m_SortedItems[i]->GetID() == itemID )
				break;
		}

		ASSERT_AT( i != m_SortedItems.Count(), 1315 );
		if ( i == m_SortedItems.Count() )
			return;

		// we're already on the end
		if ( i == m_SortedItems.Count() - 1 )
			return;

		int newItemID = m_SortedItems[i + 1]->GetID();
		SetSelectedItem( m_Items[newItemID] );
		ScrollToItem( newItemID );
		return;
	}
	if ( code == KEY_UP )
	{
		int itemID = GetSelectedItem();
		ASSERT_AT( itemID != -1, 1329 );
		if ( !m_SortedItems.Count() )	// if the list has been emptied
			return;

		int i;
		for ( i = 0; i < m_SortedItems.Count(); i++ )
		{
			if ( m_SortedItems[i]->GetID() == itemID )
				break;
		}

		ASSERT_AT( i != m_SortedItems.Count(), 1341 );

		// we're already on the end
		if ( i == 0 )
			return;

		int newItemID = m_SortedItems[i - 1]->GetID();
		SetSelectedItem( m_Items[newItemID] );
		ScrollToItem( newItemID );
		return;
	}
	if ( code != KEY_PAGEDOWN && code != KEY_PAGEUP )
	{
		if ( code != KEY_LEFT && code != KEY_RIGHT )
			Panel::OnKeyCodeTyped( code );
		return;
	}
	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	int rowsPerPage = tall / sectionTall;
	int selected = GetSelectedItem();
	if ( !m_Items.IsValidIndex( selected ) )
		return;
	CItemButton* item = m_Items[selected];
	int row = m_SortedItems.Find( item );
	int count = m_SortedItems.Count();
	int sectionID = item->GetSectionID();
	int lines = 0;
	if ( code == KEY_PAGEDOWN )
	{
		if ( rowsPerPage > 0 )
		{
			for ( int i = row + 1; i < count; i++ )
			{
				selected = m_SortedItems[i]->GetID();
				item = m_Items[selected];
				lines += item->GetSectionID() == sectionID ? 1 : 2;
				if ( lines >= rowsPerPage )
					break;
				sectionID = item->GetSectionID();
			}
		}
		SetSelectedItem( item );
		ScrollToItem( selected );
		return;
	}
	if ( rowsPerPage > 0 )
	{
		int i;
		for ( i = row - 1; i >= 0 && i < count; i-- )
		{
			selected = m_SortedItems[i]->GetID();
			item = m_Items[selected];
			lines += item->GetSectionID() == sectionID ? 1 : 2;
			if ( lines >= rowsPerPage )
				break;
			sectionID = item->GetSectionID();
		}
		if ( i < 0 || i >= count )
		{
			SetSelectedItem( item );
			m_pScrollBar->SetValue( 0 );
			return;
		}
	}
	SetSelectedItem( item );
	ScrollToItem( selected );
}
}
