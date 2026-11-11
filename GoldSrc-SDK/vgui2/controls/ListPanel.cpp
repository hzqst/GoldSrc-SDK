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

#include "ListPanel.h"

#include "Button.h"
#include "ImageList.h"
#include "ImagePanel.h"
#include "Label.h"
#include "Menu.h"
#include "ScrollBar.h"
#include "TextImage.h"
#include "Tooltip.h"
#include "controls.h"
#include "vgui/Cursor.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#if !defined( _WIN32 )
#define _snprintf snprintf
#endif

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\ListPanel.cpp" )
#else
#include <assert.h>
#define ASSERT_AT( expression, line )	assert( expression )
#endif

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Handles resizing of columns
//-----------------------------------------------------------------------------
class Dragger : public Panel
{
public:
	Dragger( int column )
	{
		m_iDragger = column;
		SetPaintBackgroundEnabled( false );
		SetPaintEnabled( false );
		SetPaintBorderEnabled( false );
		SetCursor( dc_sizewe );
		m_bDragging = false;
		m_bMovable = true;
		m_iDragPos = 0;
	}

	virtual void SetMovable( bool state );
	void OnMouseDoublePressed( MouseCode code ) override;
	void OnMouseReleased( MouseCode code ) override;
	void OnMousePressed( MouseCode code ) override;
	void OnCursorMoved( int x, int y ) override;

private:
	int		m_iDragger;
	bool	m_bDragging;
	int		m_iDragPos;
	bool	m_bMovable; // whether this dragger is movable using mouse or not
};

class ColumnButton : public Button
{
public:
	ColumnButton( Panel* parent, const char* panelName, const char* text ) : Button( parent, panelName, text ) {}

protected:
	void OnMousePressed( MouseCode code ) override;
	void ApplySchemeSettings( IScheme* scheme ) override;
};

namespace vgui2
{
struct FastSortListPanelItem : public ListPanelItem
{
	// index into accessing item to sort
	CUtlVector<int>	m_SortedTreeIndexes;
	bool			visible;
	int				primarySortIndexValue = 0;
	int				secondarySortIndexValue = 0;

#pragma warning( suppress : 26495 )
};
}

static ListPanel* s_pCurrentSortingListPanel;
static const char* s_pCurrentSortingColumn;
static bool s_currentSortingColumnTypeIsText;
static SortFunc* s_pSortFunc;
static SortFunc* s_pSortFuncSecondary;
static bool s_bSortAscending;
static bool s_bSortAscendingSecondary;
static int s_iDuplicateIndex = 1;

//-----------------------------------------------------------------------------
// Purpose: Sorts items by comparing precalculated list values
//-----------------------------------------------------------------------------
static int FastSortFunc( ListPanel*, const ListPanelItem& item1, const ListPanelItem& item2 )
{
	const FastSortListPanelItem* p1 = static_cast<const FastSortListPanelItem*>( &item1 );
	const FastSortListPanelItem* p2 = static_cast<const FastSortListPanelItem*>( &item2 );

	ASSERT_AT( p1 && p2, 354 );

	if ( p1->primarySortIndexValue < p2->primarySortIndexValue )
		return 1;
	else if ( p1->primarySortIndexValue > p2->primarySortIndexValue )
		return -1;

	if ( p1->secondarySortIndexValue < p2->secondarySortIndexValue )
		return 1;
	else if ( p1->secondarySortIndexValue > p2->secondarySortIndexValue )
		return -1;

	return ( p1 < p2 ) ? 1 : -1;
}

static int DefaultSortFunc( ListPanel*, const ListPanelItem& item1, const ListPanelItem& item2 )
{
	if ( !&item1 || !&item2 )
		return 0;

	if ( s_currentSortingColumnTypeIsText ) // textImage column
	{
		if ( item1.kv->FindKey( s_pCurrentSortingColumn, true )->GetDataType() == KeyValues::TYPE_INT )
		{
			int first = item1.kv->GetInt( s_pCurrentSortingColumn );
			int second = item2.kv->GetInt( s_pCurrentSortingColumn );
			if ( first < second )
				return -1;
			return first > second;
		}

		return stricmp( item1.kv->GetString( s_pCurrentSortingColumn ),
			item2.kv->GetString( s_pCurrentSortingColumn ) );
	}

	unsigned int first = (unsigned int)item1.kv->GetPtr( s_pCurrentSortingColumn, (void*)"" );
	unsigned int second = (unsigned int)item2.kv->GetPtr( s_pCurrentSortingColumn, (void*)"" );
	if ( first < second )
		return -1;
	return first > second;
}

static int AscendingSortFunc( const void* elem1, const void* elem2 )
{
	ListPanelItem* item1 = s_pCurrentSortingListPanel->GetItemData( *(const int*)elem1 );
	ListPanelItem* item2 = s_pCurrentSortingListPanel->GetItemData( *(const int*)elem2 );
	int result = s_pSortFunc( s_pCurrentSortingListPanel, *item1, *item2 );
	if ( result )
		return s_bSortAscending ? result : -result;

	result = s_pSortFuncSecondary( s_pCurrentSortingListPanel, *item1, *item2 );
	if ( !s_bSortAscendingSecondary )
		result = -result;
	if ( !result )
		return item2 < item1 ? 1 : -1;
	return result;
}

void Dragger::SetMovable( bool state )
{
	m_bMovable = state;
	if ( IsVisible() )
		SetCursor( state ? dc_sizewe : dc_arrow );
}

void Dragger::OnMouseDoublePressed( MouseCode )
{
	if ( m_bMovable )
		PostMessage( GetParent(), new KeyValues( "ResizeColumnToContents", "column", m_iDragger ), 0.0f );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Dragger::OnMouseReleased( MouseCode )
{
	if ( m_bMovable )
	{
		input()->SetMouseCapture( 0 );
		m_bDragging = false;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Dragger::OnMousePressed( MouseCode )
{
	if ( m_bMovable )
	{
		input()->SetMouseCapture( GetVPanel() );
		int x, y;
		input()->GetCursorPos( x, y );
		m_bDragging = true;
		m_iDragPos = x;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Dragger::OnCursorMoved( int x, int y )
{
	if ( !m_bDragging )
		return;

	input()->GetCursorPos( x, y );
	KeyValues* message = new KeyValues( "ColumnResized" );
	message->SetInt( "column", m_iDragger );
	message->SetInt( "delta", x - m_iDragPos );
	m_iDragPos = x;
	if ( GetVParent() )
		ivgui()->PostMessage( GetVParent(), message, GetVPanel(), 0.0f );
}

// Don't request focus.
// This will keep items in the listpanel selected.
void ColumnButton::OnMousePressed( MouseCode code )
{
	if ( !IsEnabled() )
		return;
	if ( code == MOUSE_RIGHT )
	{
		CallParentFunction( new KeyValues( "OpenColumnChoiceMenu" ) );
	}
	else if ( IsMouseClickEnabled( code ) && IsUseCaptureMouseEnabled() )
	{
		SetSelected( true );
		Repaint();
		// lock mouse input to going to this button
		input()->SetMouseCapture( GetVPanel() );
	}
}

void ColumnButton::ApplySchemeSettings( IScheme* scheme )
{
	Button::ApplySchemeSettings( scheme );
	SetContentAlignment( Label::a_west );
	SetFont( scheme->GetFontEx( "DefaultSmall", IsProportional(), IsHDProportional() ) );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
#pragma warning( suppress : 26495 )
ListPanel::ListPanel( Panel* parent, const char* panelName ) : Panel( parent, panelName )
{
	m_iHeaderHeight = GetProportionalScaledValue( 20 );
	m_iRowHeight = GetProportionalScaledValue( 20 );
	m_iTableStartX = 0;
	m_iTableStartY = m_iHeaderHeight + GetProportionalScaledValue( 1 );
	m_bCanSelectIndividualCells = false;
	m_iSelectedColumn = -1;
	m_bAllowUserAddDeleteColumns = false;

	m_hbar = new ScrollBar( this, "HorizScrollBar", false );
	m_hbar->AddActionSignalTarget( this );
	m_hbar->SetVisible( false );
	m_vbar = new ScrollBar( this, "VertScrollBar", true );
	m_vbar->SetVisible( false );
	m_vbar->AddActionSignalTarget( this );

	m_pLabel = new Label( this, NULL, "" );
	m_pLabel->SetVisible( false );
	m_pLabel->SetPaintBackgroundEnabled( false );
	m_pLabel->SetContentAlignment( Label::a_west );
	m_pLabel->SetProportionalScaleImages( false );
	m_pTextImage = new TextImage( "" );
	m_pImagePanel = new ImagePanel( NULL, "ListImage" );
	m_pImagePanel->SetAutoDelete( false );

	m_iSortColumn = -1;
	m_iSortColumnSecondary = -1;
	m_bSortAscending = true;
	m_bSortAscendingSecondary = true;
	m_lastBarWidth = 0;
	m_iColumnDraggerMoved = -1;
	m_bNeedsSort = false;
	m_LastItemSelected = -1;
	m_pImageList = NULL;
	m_bDeleteImageListWhenDone = false;
	m_pEmptyListText = new TextImage( "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ListPanel::~ListPanel()
{
	DeleteAllItems();
	for ( unsigned char column = m_ColumnsData.Head(); column != m_ColumnsData.InvalidIndex();
		column = m_ColumnsData.Next( column ) )
	{
		m_ColumnsData[column].m_pHeader->MarkForDeletion();
		m_ColumnsData[column].m_pResizer->MarkForDeletion();
	}
	m_ColumnsData.RemoveAll();
	delete m_pTextImage;
	delete m_pImagePanel;
	delete m_vbar;
	if ( m_bDeleteImageListWhenDone )
		delete m_pImageList;
	delete m_pEmptyListText;
}

bool ListPanel::RBTreeLessFunc( IndexItem_t& item1, IndexItem_t& item2 )
{
	int result = s_pSortFunc( s_pCurrentSortingListPanel, *item1.dataItem, *item2.dataItem );
	if ( result )
		return result > 0;
	if ( !item1.duplicateIndex )
	{
		if ( !item2.duplicateIndex )
			item2.duplicateIndex = s_iDuplicateIndex++;
		item1.duplicateIndex = item2.duplicateIndex;
	}
	else
	{
		item2.duplicateIndex = item1.duplicateIndex;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::SetColumnHeaderHeight( int height )
{
	m_iHeaderHeight = height;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the header text for a particular column.
//-----------------------------------------------------------------------------
void ListPanel::SetColumnHeaderText( int column, const char* text )
{
	m_ColumnsData[m_CurrentColumns[column]].m_pHeader->SetText( text );
}

void ListPanel::SetColumnHeaderText( int column, wchar_t* text )
{
	m_ColumnsData[m_CurrentColumns[column]].m_pHeader->SetText( text );
}

void ListPanel::SetColumnVisible( int column, bool visible )
{
	column_t& data = m_ColumnsData[m_CurrentColumns[column]];
	if ( !visible == data.m_bHidden || data.m_bUnhidable )
		return;
	data.m_bHidden = !visible;
	data.m_pHeader->SetVisible( visible );
	data.m_pResizer->SetVisible( visible );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int ListPanel::GetItemCount()
{
	return m_VisibleItems.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int ListPanel::GetItemCurrentRow( int itemID )
{
	return m_VisibleItems.Find( itemID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int ListPanel::GetItemIDFromRow( int row )
{
	if ( row < 0 || row >= m_VisibleItems.Count() )
		return -1;
	return m_VisibleItems[row];
}

int ListPanel::InvalidItemID() const
{
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::RereadAllItems()
{
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: returns the count of selected rows
//-----------------------------------------------------------------------------
int ListPanel::GetSelectedItemsCount()
{
	return m_SelectedItems.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns the selected item by selection index
// Input  : selectionIndex - valid in range [0, GetNumSelectedRows)
// Output : int - itemID
//-----------------------------------------------------------------------------
int ListPanel::GetSelectedItem( int selectionIndex )
{
	if ( selectionIndex < 0 || selectionIndex >= m_SelectedItems.Count() )
		return -1;
	return m_SelectedItems[selectionIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int ListPanel::GetSelectedColumn()
{
	return m_iSelectedColumn;
}

//-----------------------------------------------------------------------------
// Purpose: Clears all selected rows
//-----------------------------------------------------------------------------
void ListPanel::ClearSelectedItems()
{
	m_SelectedItems.RemoveAll();
}

void ListPanel::OnMouseWheeled( int delta )
{
	m_vbar->SetValue( m_vbar->GetValue() - 3 * delta );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::SetSortColumn( int column )
{
	m_iSortColumn = column;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::OnSliderMoved()
{
	InvalidateLayout( false, false );
	Repaint();
}

void ListPanel::SetSelectIndividualCells( bool state )
{
	m_bCanSelectIndividualCells = state;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text which is displayed when the list is empty
//-----------------------------------------------------------------------------
void ListPanel::SetEmptyListText( const char* text )
{
	m_pEmptyListText->SetText( text );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text which is displayed when the list is empty
//-----------------------------------------------------------------------------
void ListPanel::SetEmptyListText( const wchar_t* text )
{
	m_pEmptyListText->SetText( text );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: optimization, return true if this control has any user config settings
//-----------------------------------------------------------------------------
bool ListPanel::HasUserConfigSettings()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void ListPanel::SetAllowUserModificationOfColumns( bool allow )
{
	m_bAllowUserAddDeleteColumns = allow;
}

int ListPanel::FirstItem() const
{
	return m_DataItems.Head();
}

int ListPanel::NextItem( int itemID ) const
{
	return m_DataItems.Next( itemID );
}

int ListPanel::GetNumColumnHeaders() const
{
	return m_CurrentColumns.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::SetImageList( ImageList* imageList, bool deleteImageListWhenDone )
{
	if ( m_pImageList && m_bDeleteImageListWhenDone )
		delete m_pImageList;
	m_bDeleteImageListWhenDone = deleteImageListWhenDone;
	m_pImageList = imageList;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::PaintBackground()
{
	Panel::PaintBackground();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::OnSizeChanged( int wide, int tall )
{
	Panel::OnSizeChanged( wide, tall );
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the index of a column by column->GetName()
//-----------------------------------------------------------------------------
int ListPanel::FindColumn( const char* columnName )
{
	for ( int i = 0; i < m_CurrentColumns.Count(); i++ )
	{
		if ( !stricmp( columnName, m_ColumnsData[m_CurrentColumns[i]].m_pHeader->GetName() ) )
			return i;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: gets the item ID of an item by name (data->GetName())
//-----------------------------------------------------------------------------
int ListPanel::GetItem( const char* itemName )
{
	for ( int i = m_DataItems.Head(); i != m_DataItems.InvalidIndex(); i = m_DataItems.Next( i ) )
	{
		if ( !stricmp( m_DataItems[i]->kv->GetName(), itemName ) )
			return i;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::SetColumnSortable( int column, bool sortable )
{
	Button* header = m_ColumnsData[m_CurrentColumns[column]].m_pHeader;
	if ( sortable )
		header->SetCommand( new KeyValues( "SetSortColumn", "column", column ) );
	else
		header->SetCommand( (const char*)NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::SetFont( HFont font )
{
	ASSERT_AT( font, 2316 );
	if ( !font )
		return;

	m_pTextImage->SetFont( font );
	m_iRowHeight = surface()->GetFontTall( font ) + 2;
}

void ListPanel::AddColumnHeader( int index, const char* columnName, const char* columnText,
	int width, int columnFlags )
{
	if ( ( columnFlags & ( COLUMN_FIXEDSIZE | COLUMN_RESIZEWITHWINDOW ) ) == COLUMN_FIXEDSIZE )
		AddColumnHeader( index, columnName, columnText, width, width, width, columnFlags );
	else
		AddColumnHeader( index, columnName, columnText, width, GetProportionalScaledValue( 20 ),
			GetProportionalScaledValue( 10000 ), columnFlags );
}

bool ListPanel::GetColumnHeaderText( int column, char* text, int bufferSize )
{
	if ( column >= m_CurrentColumns.Count() )
		return false;
	m_ColumnsData[m_CurrentColumns[column]].m_pHeader->GetText( text, bufferSize );
	return true;
}

int ListPanel::GetItemIDFromUserData( unsigned int userData )
{
	for ( int i = m_DataItems.Head(); i != m_DataItems.InvalidIndex(); i = m_DataItems.Next( i ) )
	{
		if ( m_DataItems[i]->userData == userData )
			return i;
	}
	return InvalidItemID();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ListPanel::IsValidItemID( int itemID )
{
	return m_DataItems.IsValidIndex( itemID );
}

//-----------------------------------------------------------------------------
// Purpose: returns user data for itemID
//-----------------------------------------------------------------------------
unsigned int ListPanel::GetItemUserData( int itemID )
{
	if ( !m_DataItems.IsValidIndex( itemID ) )
		return 0;
	return m_DataItems[itemID]->userData;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ListPanelItem* ListPanel::GetItemData( int itemID )
{
	if ( !m_DataItems.IsValidIndex( itemID ) )
		return NULL;
	return m_DataItems[itemID];
}

void ListPanel::SetItemDisabled( int itemID, bool disabled )
{
	if ( m_DataItems.IsValidIndex( itemID ) )
		m_DataItems[itemID]->kv->SetInt( "disabled", disabled );
}

//-----------------------------------------------------------------------------
// Purpose: returns pointer to data the itemID holds
//-----------------------------------------------------------------------------
KeyValues* ListPanel::GetItem( int itemID )
{
	if ( !m_DataItems.IsValidIndex( itemID ) )
		return NULL;
	return m_DataItems[itemID]->kv;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::SetUserData( int itemID, unsigned int userData )
{
	if ( m_DataItems.IsValidIndex( itemID ) )
		m_DataItems[itemID]->userData = userData;
}

//-----------------------------------------------------------------------------
// Purpose: clears and deletes all the memory used by the data items
//-----------------------------------------------------------------------------
void ListPanel::DeleteAllItems()
{
	for ( int i = 0; i < m_ColumnsHistory.Count(); i++ )
		m_ColumnsData[m_ColumnsHistory[i]].m_SortedTree.RemoveAll();
	for ( int i = m_DataItems.Head(); i != m_DataItems.InvalidIndex(); i = m_DataItems.Next( i ) )
	{
		ListPanelItem* item = m_DataItems[i];
		if ( item )
		{
			if ( item->kv )
				item->kv->deleteThis();
			delete item;
		}
	}
	m_DataItems.RemoveAll();
	m_VisibleItems.RemoveAll();
	m_SelectedItems.RemoveAll();
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::AddSelectedItem( int itemID )
{
	if ( !m_DataItems.IsValidIndex( itemID ) )
		return;

	ASSERT_AT( !m_SelectedItems.HasElement(itemID), 1149 );

	m_SelectedItems.AddToTail( itemID );
	PostActionSignal( new KeyValues( "ItemSelected" ) );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::SetSingleSelectedItem( int itemID )
{
	ClearSelectedItems();
	AddSelectedItem( itemID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::SetSelectedCell( int itemID, int column )
{
	if ( !m_DataItems.IsValidIndex( itemID ) || column < 0 || column >= m_CurrentColumns.Count() )
		return;
	m_iSelectedColumn = column;
	SetSingleSelectedItem( itemID );
	// notify all watchers
	PostActionSignal( new KeyValues( "ItemSelected" ) );
	Repaint();
}

void ListPanel::SetMultiSelectEnabled( bool )
{
}

void ListPanel::SaveSelectionState()
{
}

void ListPanel::RestoreSelectionState()
{
}

bool ListPanel::IsScrollbarVisible()
{
	return m_vbar->IsVisible();
}

void ListPanel::SetIgnoreDoubleClick( bool )
{
}

void ListPanel::ScrollToItem( int itemID )
{
	int row = m_VisibleItems.Find( itemID );
	if ( row != -1 )
		m_vbar->SetValue( row );
}

void ListPanel::RemoveAll()
{
	DeleteAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: Calculate number of rows per page 
//-----------------------------------------------------------------------------
float ListPanel::GetRowsPerPage()
{
	return (float)( GetTall() - m_iHeaderHeight ) / (float)m_iRowHeight;
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the item we should Start on
//-----------------------------------------------------------------------------
int ListPanel::GetStartItem()
{
	if ( m_VisibleItems.Count() > GetRowsPerPage() )
		return m_vbar->GetValue();
	return 0;	// otherwise Start at top
}

bool ListPanel::GetCellText( int itemID, int column, wchar_t* text, int bufferSize )
{
	if ( !text || !bufferSize )
		return false;
	wcscpy( text, L"" );
	KeyValues* item = GetItem( itemID );
	if ( !item || column < 0 || column >= m_CurrentColumns.Count() )
		return false;
	const char* name = m_ColumnsData[m_CurrentColumns[column]].m_pHeader->GetName();
	if ( !name || !*name )
		return false;
	const wchar_t* value = item->GetWString( name, L"" );
	if ( !value || !*name )
		return false;
	wcsncpy( text, value, bufferSize );
	text[bufferSize - 1] = 0;
	return *text != 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns the data held by a specific cell
//-----------------------------------------------------------------------------
IImage* ListPanel::GetCellImage( int itemID, int column ) //, ImagePanel *&buffer)
{
//	if ( !buffer )
//		return;
	KeyValues* item = GetItem( itemID );
	if ( !item || column < 0 || column >= m_CurrentColumns.Count() )
		return NULL;
	const char* name = m_ColumnsData[m_CurrentColumns[column]].m_pHeader->GetName();
	if ( !name || !*name || !m_pImageList )
		return NULL;
	int image = item->GetInt( name );
	bool valid = m_pImageList->IsValidIndex( image );
	if ( image > 0 && valid )
		return m_pImageList->GetImage( image );
	return NULL;
}

void ListPanel::ResetScrollBar()
{
	// delete and reallocate to besure the scroll bar's
	// information is correct.
	delete m_vbar;
	m_vbar = new ScrollBar( this, "VertScrollBar", true );
	m_vbar->SetVisible( false );
	m_vbar->AddActionSignalTarget( this );
}

void ListPanel::OnToggleColumnVisible( int column )
{
	if ( column >= 0 && column < m_CurrentColumns.Count() )
		SetColumnVisible( column, m_ColumnsData[m_CurrentColumns[column]].m_bHidden );
}

//-----------------------------------------------------------------------------
// Purpose: Resets the "SetSortColumn" command for each column - in case columns were added or removed
//-----------------------------------------------------------------------------
void ListPanel::ResetColumnHeaderCommands()
{
	for ( int i = 0; i < m_CurrentColumns.Count(); i++ )
		m_ColumnsData[m_CurrentColumns[i]].m_pHeader->SetCommand( new KeyValues( "SetSortColumn", "column", i ) );
}

void ListPanel::ApplyUserConfigSettings( KeyValues* settings )
{
	char name[64];
	// read which columns are hidden
	for ( int i = 0; i < m_CurrentColumns.Count(); i++ )
	{
		_snprintf( name, sizeof( name ), "%d_hidden", i );
		int hidden = settings->GetInt( name, -1 );
		if ( hidden == 0 )
			SetColumnVisible( i, true );
		else if ( hidden == 1 )
			SetColumnVisible( i, false );
	}
}

void ListPanel::GetUserConfigSettings( KeyValues* settings )
{
	char name[64];
	// save which columns are hidden
	for ( int i = 0; i < m_CurrentColumns.Count(); i++ )
	{
		_snprintf( name, sizeof( name ), "%d_hidden", i );
		settings->SetInt( name, m_ColumnsData[m_CurrentColumns[i]].m_bHidden );
	}
}

//-----------------------------------------------------------------------------
// Purpose: opens the content menu
//-----------------------------------------------------------------------------
void ListPanel::OpenColumnChoiceMenu()
{
	if ( !m_bAllowUserAddDeleteColumns )
		return;
	Menu* menu = new Menu( this, "ContextMenu" );
	int x, y;
	input()->GetCursorPos( x, y );
	menu->SetPos( x, y );
	// add all the column choices to the menu
	for ( int i = 0; i < m_CurrentColumns.Count(); i++ )
	{
		column_t& column = m_ColumnsData[m_CurrentColumns[i]];
		char name[128];
		column.m_pHeader->GetText( name, sizeof( name ) );
		int item = menu->AddCheckableMenuItem( name,
			new KeyValues( "ToggleColumnVisible", "col", m_CurrentColumns[i] ), this, NULL );
		menu->SetMenuItemChecked( item, !column.m_bHidden );
		if ( column.m_bUnhidable )
			menu->SetItemEnabled( item, false );
	}
	menu->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the column header to have an image instead of text
//-----------------------------------------------------------------------------
void ListPanel::SetColumnHeaderImage( int column, int image )
{
	ASSERT_AT( m_pImageList, 690 );
	Button* header = m_ColumnsData[m_CurrentColumns[column]].m_pHeader;
	header->SetTextImageIndex( -1 );
	header = m_ColumnsData[m_CurrentColumns[column]].m_pHeader;
	header->SetImageAtIndex( 0, m_pImageList->GetImage( image ), 0 );
}

//-----------------------------------------------------------------------------
// Purpose: associates a tooltip with the column header
//-----------------------------------------------------------------------------
void ListPanel::SetColumnHeaderTooltip( int column, const char* text )
{
	m_ColumnsData[m_CurrentColumns[column]].m_pHeader->GetTooltip()->SetText( text );
	m_ColumnsData[m_CurrentColumns[column]].m_pHeader->GetTooltip()->SetTooltipFormatToSingleLine();
	m_ColumnsData[m_CurrentColumns[column]].m_pHeader->GetTooltip()->SetTooltipDelay( 0 );
}

void ListPanel::AddColumnHeader( int index, const char* columnName, const char* columnText,
	int width, int minWidth, int maxWidth, int columnFlags )
{
	ASSERT_AT( minWidth <= width, 547 );
	ASSERT_AT( maxWidth >= width, 548 );

	// get our permanent index
	unsigned char columnIndex = m_ColumnsData.AddToTail();
	// put this index on the tail, so all item's m_SortedTreeIndexes have a consistent mapping
	m_ColumnsHistory.AddToTail( columnIndex );
	// put this column in the right place visually
	m_CurrentColumns.InsertBefore( index, columnIndex );
	// create the actual column object
	column_t& column = m_ColumnsData[columnIndex];
	ColumnButton* header = new ColumnButton( this, columnName, columnText );
	header->MakeReadyForUse();
	header->SetSize( width, GetProportionalScaledValue( 24 ) );
	header->AddActionSignalTarget( this );
	header->SetContentAlignment( Label::a_west );
	header->SetTextInset( GetProportionalScaledValue( 5 ), 0 );
	column.m_pHeader = header;
	column.m_bHidden = false;
	column.m_iMinWidth = minWidth;
	column.m_iMaxWidth = maxWidth;
	column.m_bResizesWithWindow = ( columnFlags & COLUMN_RESIZEWITHWINDOW ) != 0;
	column.m_bTypeIsText = ( columnFlags & COLUMN_IMAGE ) == 0;
	column.m_bUnhidable = ( columnFlags & COLUMN_UNHIDABLE ) != 0;

	Dragger* resizer = new Dragger( index );
	resizer->SetParent( this );
	resizer->AddActionSignalTarget( this );
	resizer->MoveToFront();
	if ( minWidth == maxWidth || ( columnFlags & COLUMN_FIXEDSIZE ) )
		resizer->SetMovable( false );
	column.m_pResizer = resizer;
	column.m_pSortFunc = NULL;
	column.m_SortedTree.SetLessFunc( RBTreeLessFunc );
	// go through all the headers and make sure their Command has the right column ID
	ResetColumnHeaderCommands();
	// 
	ResortColumnRBTree( index );
	// ensure scroll bar is topmost compared to column headers
	m_vbar->MoveToFront();
	// fix up our visibility
	SetColumnVisible( index, ( columnFlags & COLUMN_HIDDEN ) == 0 );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::RemoveColumn( int column )
{
	unsigned char columnIndex = m_CurrentColumns[column];
	// remove it from the current columns
	m_CurrentColumns.Remove( column );

	// zero out this entry in m_ColumnsHistory
	unsigned char i;
	for ( i = 0; i < m_ColumnsHistory.Count(); i++ )
	{
		if ( m_ColumnsHistory[i] == columnIndex )
		{
			m_ColumnsHistory[i] = m_ColumnsData.InvalidIndex();
			break;
		}
	}
	ASSERT_AT( i != m_ColumnsHistory.Count(), 769 );

	// delete and remove the column data
	m_ColumnsData[columnIndex].m_SortedTree.RemoveAll();
	m_ColumnsData[columnIndex].m_pHeader->MarkForDeletion();
	m_ColumnsData[columnIndex].m_pResizer->MarkForDeletion();
	m_ColumnsData.Remove( columnIndex );
	ResetColumnHeaderCommands();
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Recreates a column's RB Sorted Tree
//-----------------------------------------------------------------------------
void ListPanel::ResortColumnRBTree( int col )
{
	ASSERT_AT( m_CurrentColumns.IsValidIndex(col), 615 );

	unsigned char dataColumnIndex = m_CurrentColumns[col];
	int columnHistoryIndex = m_ColumnsHistory.Find( dataColumnIndex );
	column_t& column = m_ColumnsData[dataColumnIndex];

	column.m_SortedTree.RemoveAll();

	s_pCurrentSortingListPanel = this;
	s_currentSortingColumnTypeIsText = column.m_bTypeIsText; // type of data in the column
	s_bSortAscending = true;
	s_pSortFunc = column.m_pSortFunc ? column.m_pSortFunc : DefaultSortFunc;
	s_pSortFuncSecondary = NULL;

	// sort all current data items for this column
	for ( int i = m_DataItems.Head(); i != m_DataItems.InvalidIndex(); i = m_DataItems.Next( i ) )
	{
		IndexItem_t item = { m_DataItems[i], 0 };
		FastSortListPanelItem* dataItem = (FastSortListPanelItem*)m_DataItems[i];

		if ( dataItem->m_SortedTreeIndexes.Count() == m_ColumnsHistory.Count() - 1 &&
			columnHistoryIndex == m_ColumnsHistory.Count() - 1 )
		{
			dataItem->m_SortedTreeIndexes.AddToTail();
		}

		ASSERT_AT( dataItem->m_SortedTreeIndexes.IsValidIndex(columnHistoryIndex), 654 );

		dataItem->m_SortedTreeIndexes[columnHistoryIndex] = column.m_SortedTree.Insert( item );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Adds the item into the column indexes
//-----------------------------------------------------------------------------
void ListPanel::IndexItem( int itemID )
{
	FastSortListPanelItem* item = (FastSortListPanelItem*)m_DataItems[itemID];
	int count = item->m_SortedTreeIndexes.Count();
	if ( count > m_ColumnsHistory.Count() )
		count = m_ColumnsHistory.Count();
	for ( int i = 0; i < count; i++ )
		m_ColumnsData[m_ColumnsHistory[i]].m_SortedTree.RemoveAt( item->m_SortedTreeIndexes[i] );
	item->m_SortedTreeIndexes.RemoveAll();
	item->m_SortedTreeIndexes.AddMultipleToTail( m_ColumnsHistory.Count() );
	// set the current sorting list (since the insert will need to sort)
	s_pCurrentSortingListPanel = this;
	// add the item into the RB tree for each column
	for ( int i = 0; i < m_ColumnsHistory.Count(); i++ )
	{
		unsigned char columnIndex = m_ColumnsHistory[i];
		if ( columnIndex == m_ColumnsData.InvalidIndex() )
			continue;
		column_t& column = m_ColumnsData[columnIndex];
		IndexItem_t indexItem = { item, 0 };
		// setup sort state
		s_pCurrentSortingListPanel = this;
		s_pCurrentSortingColumn = column.m_pHeader->GetName(); // name of current column for sorting
		s_bSortAscending = true;
		s_currentSortingColumnTypeIsText = column.m_bTypeIsText; // type of data in the column
		s_pSortFunc = column.m_pSortFunc ? column.m_pSortFunc : DefaultSortFunc;
		s_pSortFuncSecondary = NULL;
		item->m_SortedTreeIndexes[i] = column.m_SortedTree.Insert( indexItem );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::SetSortFunc( int col, SortFunc* sortFunc )
{
	ASSERT_AT( col < m_CurrentColumns.Count(), 2173 );
	unsigned char columnIndex = m_CurrentColumns[col];
	if ( !m_ColumnsData[columnIndex].m_bTypeIsText && sortFunc )
		m_ColumnsData[columnIndex].m_pHeader->SetMouseClickEnabled( MOUSE_LEFT, true );
	m_ColumnsData[columnIndex].m_pSortFunc = sortFunc;

	ResortColumnRBTree( col );
}

//-----------------------------------------------------------------------------
// Purpose: adds an item to the view
//			data->GetName() is used to uniquely identify an item
//			data sub items are matched against column header name to be used in the table
//-----------------------------------------------------------------------------
int ListPanel::AddItem( const KeyValues* data, unsigned int userData, bool scrollToItem, bool sortOnAdd )
{
	FastSortListPanelItem* item = new FastSortListPanelItem;
	item->kv = data->MakeCopy();
	item->userData = userData;
	int itemID = m_DataItems.AddToTail( item );
	int row = m_VisibleItems.AddToTail( itemID );
	item->visible = true;
	// put the item in each column's sorted Tree Index
	IndexItem( itemID );
	if ( sortOnAdd )
		m_bNeedsSort = true;
	InvalidateLayout( false, false );
	if ( scrollToItem )
		m_vbar->SetValue( row );
	return itemID;
}

//-----------------------------------------------------------------------------
// Purpose: updates the view with any changes to the data
// Input  : itemID - index to update
//-----------------------------------------------------------------------------
void ListPanel::ApplyItemChanges( int itemID )
{
	// reindex the item and then redraw
	IndexItem( itemID );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: Removes an item at the specified item
//-----------------------------------------------------------------------------
void ListPanel::RemoveItem( int itemID )
{
	FastSortListPanelItem* item = (FastSortListPanelItem*)m_DataItems[itemID];
	if ( !item )
		return;
	for ( int i = 0; i < m_ColumnsHistory.Count(); i++ )
	{
		unsigned char columnIndex = m_ColumnsHistory[i];
		if ( columnIndex != m_ColumnsData.InvalidIndex() )
			m_ColumnsData[columnIndex].m_SortedTree.RemoveAt( item->m_SortedTreeIndexes[i] );
	}
	// remove from selection
	m_SelectedItems.FindAndRemove( itemID );
	// remove from visible items
	m_VisibleItems.FindAndRemove( itemID );
	// remove from data
	m_DataItems.Remove( itemID );
	if ( item->kv )
		item->kv->deleteThis();
	delete item;
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: sets whether the item is visible or not
//-----------------------------------------------------------------------------
void ListPanel::SetItemVisible( int itemID, bool visible )
{
	if ( !m_DataItems.IsValidIndex( itemID ) )
		return;
	FastSortListPanelItem* item = (FastSortListPanelItem*)m_DataItems[itemID];
	if ( item->visible == visible )
		return;
	m_bNeedsSort = true;
	item->visible = visible;
	if ( visible )
	{
		// add back to end of list
		m_VisibleItems.AddToTail( itemID );
	}
	else
	{
		m_SelectedItems.FindAndRemove( itemID );
		// remove from data
		m_VisibleItems.FindAndRemove( itemID );
		InvalidateLayout( false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::SortList()
{
	m_bNeedsSort = false;
	if ( m_VisibleItems.Count() <= 1 )
		return;

	// check if the last selected item is on the screen - if so, we should try to maintain it on screen 
	int start = GetStartItem();
	int rowsPerPage = (int)GetRowsPerPage();
	int screenPosition = -1;
	if ( m_LastItemSelected != -1 && m_SelectedItems.Count() > 0 )
	{
		int row = m_VisibleItems.Find( m_LastItemSelected );
		if ( row >= start && row <= start + rowsPerPage )
			screenPosition = row - start;
	}
	// get the required sorting functions
	s_pCurrentSortingListPanel = this;
	// setup globals for use in qsort
	s_pSortFunc = FastSortFunc;
	s_pSortFuncSecondary = FastSortFunc;
	s_bSortAscending = m_bSortAscending;
	s_bSortAscendingSecondary = m_bSortAscendingSecondary;
	if ( m_iSortColumn >= 0 && m_iSortColumn < m_CurrentColumns.Count() )
	{
		IndexRBTree_t& tree = m_ColumnsData[m_CurrentColumns[m_iSortColumn]].m_SortedTree;
		int duplicateIndex = 0;
		int sortIndex = 1;
		for ( int i = tree.FirstInorder(); i != tree.InvalidIndex(); i = tree.NextInorder( i ) )
		{
			IndexItem_t& indexItem = tree.Element( i );
			FastSortListPanelItem* item = (FastSortListPanelItem*)indexItem.dataItem;
			if ( !item->visible )
				continue;
			if ( !duplicateIndex || duplicateIndex != indexItem.duplicateIndex )
				sortIndex++;
			item->primarySortIndexValue = sortIndex;
			duplicateIndex = indexItem.duplicateIndex;
		}
	}
	if ( m_iSortColumnSecondary >= 0 && m_iSortColumnSecondary < m_CurrentColumns.Count() )
	{
		IndexRBTree_t& tree = m_ColumnsData[m_CurrentColumns[m_iSortColumnSecondary]].m_SortedTree;
		int duplicateIndex = 0;
		int sortIndex = 1;
		for ( int i = tree.FirstInorder(); i != tree.InvalidIndex(); i = tree.NextInorder( i ) )
		{
			IndexItem_t& indexItem = tree.Element( i );
			FastSortListPanelItem* item = (FastSortListPanelItem*)indexItem.dataItem;
			if ( !item->visible )
				continue;
			if ( !duplicateIndex || duplicateIndex != indexItem.duplicateIndex )
				sortIndex++;
			item->secondarySortIndexValue = sortIndex;
			duplicateIndex = indexItem.duplicateIndex;
		}
	}
	qsort( m_VisibleItems.Base(), m_VisibleItems.Count(), sizeof( int ), AscendingSortFunc );
	if ( screenPosition != -1 )
	{
		int row = m_VisibleItems.Find( m_LastItemSelected );
		m_vbar->SetValue( screenPosition >= row ? 0 : row - screenPosition );
	}
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: sets which column we should sort with
//-----------------------------------------------------------------------------
void ListPanel::OnSetSortColumn( int column )
{
	if ( m_iSortColumn == column )
		m_bSortAscending = !m_bSortAscending;
	else
	{
		// switching sort columns, keep the old one as the secondary sort
		m_iSortColumnSecondary = m_iSortColumn;
		m_bSortAscendingSecondary = m_bSortAscending;
	}
	SetSortColumn( column );
	SortList();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::ApplySchemeSettings( IScheme* scheme )
{
	m_pLabel->InvalidateLayout( true, false );
	Panel::ApplySchemeSettings( scheme );
	SetBgColor( GetSchemeColor( "WindowBgColor", scheme ) );
	SetBorder( scheme->GetBorder( "ButtonDepressedBorder" ) );
	m_pLabel->SetBgColor( GetSchemeColor( "Menu/ArmedBgColor", scheme ) );
	SetBgColor( GetSchemeColor( "ListBgColor", GetSchemeColor( "WindowDisabledBgColor", scheme ), scheme ) );
	m_LabelFgColor = GetSchemeColor( "WindowFgColor", scheme );
	m_SelectionFgColor = GetSchemeColor( "ListSelectionFgColor", m_LabelFgColor, scheme );
	m_pEmptyListText->SetColor( GetSchemeColor( "LabelDimText", scheme ) );
	SetFont( scheme->GetFontEx( "Default", IsProportional(), IsHDProportional() ) );
	m_pEmptyListText->SetFont( scheme->GetFontEx( "Default", IsProportional(), IsHDProportional() ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the panel to use to render a cell
//-----------------------------------------------------------------------------
Panel* ListPanel::GetCellRenderer( int itemID, int column )
{
	ASSERT_AT( m_pTextImage, 1271 );
	ASSERT_AT( m_pImagePanel, 1272 );

	column_t& data = m_ColumnsData[m_CurrentColumns[column]];
	IScheme* panelScheme = scheme()->GetIScheme( GetScheme() );
	if ( !data.m_bTypeIsText )
	{
		if ( m_SelectedItems.Find( itemID ) != -1 &&
			( !m_bCanSelectIndividualCells || column == m_iSelectedColumn ) )
		{
			VPANEL focus = input()->GetFocus();
			if ( HasFocus() || ( focus && ipanel()->HasParent( focus, GetVParent() ) ) )
				m_pLabel->SetBgColor( GetSchemeColor( "Menu/ArmedBgColor", panelScheme ) );
			else
				m_pLabel->SetBgColor( GetSchemeColor( "SelectionBG2", panelScheme ) );
			m_pLabel->SetPaintBackgroundEnabled( true );
		}
		else
		{
			m_pLabel->SetPaintBackgroundEnabled( false );
		}
		m_pLabel->SetImageAtIndex( 0, GetCellImage( itemID, column ), 0 );
		m_pLabel->SetProportionalScaleImages( true );
		return m_pLabel;
	}

	wchar_t text[256];
	GetCellText( itemID, column, text, sizeof( text ) / sizeof( wchar_t ) );
	m_pTextImage->SetText( text );
	int wide, tall;
	m_pTextImage->GetContentSize( wide, tall );
	wide = data.m_pHeader->GetWide();
	m_pTextImage->SetSize( wide - GetProportionalScaledValue( 5 ), tall );
	m_pLabel->SetImageAtIndex( 0, m_pTextImage, 3 );
	if ( m_SelectedItems.Find( itemID ) != -1 &&
		( !m_bCanSelectIndividualCells || column == m_iSelectedColumn ) )
	{
		VPANEL focus = input()->GetFocus();
		if ( HasFocus() || ( focus && ipanel()->HasParent( focus, GetVParent() ) ) )
			m_pLabel->SetBgColor( GetSchemeColor( "Menu/ArmedBgColor", panelScheme ) );
		else
			m_pLabel->SetBgColor( GetSchemeColor( "SelectionBG2", panelScheme ) );
		m_pTextImage->SetColor( m_SelectionFgColor );
		m_pLabel->SetPaintBackgroundEnabled( true );
	}
	else
	{
		m_pTextImage->SetColor( m_LabelFgColor );
		m_pLabel->SetPaintBackgroundEnabled( false );
	}
	m_pLabel->SetProportionalScaleImages( false );
	return m_pLabel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : deltax - deltas from current position
//-----------------------------------------------------------------------------
void ListPanel::OnColumnResized( int column, int delta )
{
	m_iColumnDraggerMoved = column;
	column_t& data = m_ColumnsData[m_CurrentColumns[column]];
	Panel* header = data.m_pHeader;
	int wide, tall;
	header->GetSize( wide, tall );
	wide += delta;
	if ( wide < data.m_iMinWidth )
		wide = data.m_iMinWidth;
	if ( wide > data.m_iMaxWidth )
		wide = data.m_iMaxWidth;
	// make sure we have enough space for the columns to our right
	int panelWide, panelTall;
	GetSize( panelWide, panelTall );
	int x, y;
	header->GetPos( x, y );
	int minRemainingWidth = 0;
	for ( int i = column + 1; i < m_CurrentColumns.Count(); i++ )
		minRemainingWidth += m_ColumnsData[m_CurrentColumns[i]].m_iMinWidth;
	panelWide -= x + minRemainingWidth + GetProportionalScaledValue( 20 );
	if ( panelWide < wide )
		wide = panelWide;
	header->SetSize( wide, tall );
	header->InvalidateLayout( false, false );
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Resizes a column
//-----------------------------------------------------------------------------
void ListPanel::ResizeColumnToContents( int column )
{
	// iterate all the items in the column, getting the size of each
	column_t& data = m_ColumnsData[m_CurrentColumns[column]];
	if ( !data.m_bTypeIsText )
		return;
	int wide = 0, tall = 0, minRequiredWidth = 0;
	data.m_pHeader->GetContentSize( minRequiredWidth, tall );
	// iterate every item
	for ( int i = 0; i < m_VisibleItems.Count(); i++ )
	{
		// get the text
		wchar_t text[256];
		GetCellText( m_VisibleItems[i], column, text, sizeof( text ) / sizeof( wchar_t ) );
		m_pTextImage->SetText( text );
		m_pTextImage->GetContentSize( wide, tall );
		if ( wide > minRequiredWidth )
			minRequiredWidth = wide + 2;
	}
	data.m_pHeader->GetSize( wide, tall );
	OnColumnResized( column, minRequiredWidth - wide );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if any found, row and column are filled out
//-----------------------------------------------------------------------------
bool ListPanel::GetCellAtPos( int x, int y, int& row, int& column )
{
	// convert to local
	ScreenToLocal( x, y );
	// move to Start of table
	x -= m_iTableStartX;
	y -= m_iTableStartY;
	int start = GetStartItem();
	// make sure it's still in valid area
	if ( x >= 0 && y >= 0 )
	{
		row = start;
		while ( row < m_VisibleItems.Count() && y >= ( row - start + 1 ) * m_iRowHeight )
			row++;
		column = 0;
		int width = 0;
		while ( column < m_CurrentColumns.Count() )
		{
			width += m_ColumnsData[m_CurrentColumns[column]].m_pHeader->GetWide();
			if ( width > x )
				break;
			column++;
		}
		if ( row != m_VisibleItems.Count() && column != m_CurrentColumns.Count() )
			return true;
	}
	column = -1;
	row = -1;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ListPanel::GetCellBounds( int row, int column, int& x, int& y, int& wide, int& tall )
{
	if ( column < 0 || column >= m_CurrentColumns.Count() || row < 0 || row >= m_VisibleItems.Count() )
		return false;
	// Is row on screen?
	int start = GetStartItem();
	if ( row < start || (float)row >= GetRowsPerPage() + (float)start )
		return false;
	y = m_iTableStartY;
	y += ( row - start ) * m_iRowHeight;
	tall = m_iRowHeight;
	x = m_iTableStartX;
	for ( int i = 0; i < column; i++ )
		x += m_ColumnsData[m_CurrentColumns[i]].m_pHeader->GetWide();
	wide = m_ColumnsData[m_CurrentColumns[column]].m_pHeader->GetWide();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Renders the cells
//-----------------------------------------------------------------------------
void ListPanel::Paint()
{
	if ( m_bNeedsSort )
		SortList();
	// draw selection areas if any
	int wide, tall;
	GetSize( wide, tall );
	m_iTableStartX = 0;
	m_iTableStartY = m_iHeaderHeight + GetProportionalScaledValue( 1 );
	int start = 0;
	if ( m_VisibleItems.Count() >= (int)GetRowsPerPage() )
		start = m_vbar->GetValue();
	int drawCount = 0;
//	debug timing functions
//	double startTime, endTime;
//	startTime = system()->GetCurrentTime();

	// iterate through and draw each cell
	bool done = false;
	for ( int row = start; row < m_VisibleItems.Count() && !done; row++ )
	{
		if ( row < 0 )
			continue;
		int x = 0;
		int itemID = m_VisibleItems[row];
		for ( int column = 0; column < m_CurrentColumns.Count(); column++ )
		{
			Button* header = m_ColumnsData[m_CurrentColumns[column]].m_pHeader;
			Panel* renderer = GetCellRenderer( itemID, column );
			if ( !header->IsVisible() )
				continue;
			int cellWidth = header->GetWide();
			if ( renderer )
			{
				if ( renderer->GetVParent() != GetVPanel() )
					renderer->SetParent( GetVPanel() );
				if ( !renderer->IsVisible() )
					renderer->SetVisible( true );
				renderer->SetPos( m_iTableStartX + x + GetProportionalScaledValue( 2 ),
					m_iTableStartY + m_iRowHeight * drawCount );
				renderer->SetSize( cellWidth, m_iRowHeight - 1 );
				renderer->Repaint();
				surface()->SolveTraverse( renderer->GetVPanel(), false );
				int x0, y0, x1, y1;
				renderer->GetClipRect( x0, y0, x1, y1 );
				if ( y1 - y0 < m_iRowHeight - GetProportionalScaledValue( 3 ) )
				{
					done = true;
					break;
				}
				surface()->PaintTraverse( renderer->GetVPanel() );
			}
			/*
			// work in progress, optimized paint for text
			else
			{
				// just paint it ourselves
				char tempText[256];
				// Grab cell text
				GetCellText(i, j, tempText, sizeof(tempText));
				surface()->DrawSetTextPos(x + m_iTableStartX + 2, (drawcount * m_iRowHeight) + m_iTableStartY);

				for (const char *pText = tempText; *pText != 0; pText++)
				{
					surface()->DrawUnicodeChar((wchar_t)*pText);
				}
			}
			*/
			x += cellWidth;
		}
		drawCount++;
	}
	m_pLabel->SetVisible( false );
	// if the list is empty, draw some help text
	if ( m_VisibleItems.Count() <= 0 && m_pEmptyListText )
	{
		m_pEmptyListText->SetPos( m_iTableStartX + GetProportionalScaledValue( 8 ),
			m_iTableStartY + GetProportionalScaledValue( 4 ) );
		m_pEmptyListText->SetSize( wide - GetProportionalScaledValue( 8 ), m_iRowHeight );
		m_pEmptyListText->Paint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: relayouts out the panel after any internal changes
//-----------------------------------------------------------------------------
void ListPanel::PerformLayout()
{
	if ( m_bNeedsSort )
		SortList();
	float rowsPerPage = GetRowsPerPage();
	// count the number of visible items
	int rows = m_VisibleItems.Count();
	//!! need to make it recalculate scroll positions
	m_vbar->SetVisible( true );
	m_vbar->SetEnabled( false );
	m_vbar->SetRangeWindow( (int)rowsPerPage );
	m_vbar->SetRange( 0, rows );
	m_vbar->SetButtonPressedScrollValue( 1 );
	int wide, tall;
	GetSize( wide, tall );
	m_vbar->SetPos( wide - GetProportionalScaledValue( 20 ), 0 );
	m_vbar->SetSize( GetProportionalScaledValue( 18 ), tall - GetProportionalScaledValue( 2 ) );
	m_vbar->InvalidateLayout( false, false );

	int buttonMaxXPos = wide - GetProportionalScaledValue( 20 );
	int count = m_CurrentColumns.Count();
	int resizeCount = 1;
	if ( m_iColumnDraggerMoved == -1 ) // we're resizing in response to a column dragger
	{
		resizeCount = 0;
		for ( int i = 0; i < count; i++ )
		{
			column_t& column = m_ColumnsData[m_CurrentColumns[i]];
			if ( column.m_bResizesWithWindow && !column.m_bHidden )
				resizeCount++;
		}
	}

	int oldSizeX = 0, oldSizeY = 0;
	int lastColumnIndex = count - 1;
	int lastVisibleColumn = lastColumnIndex;
	for ( int i = lastColumnIndex; i >= 0; i-- )
	{
		column_t& column = m_ColumnsData[m_CurrentColumns[i]];
		if ( !column.m_bHidden )
		{
			lastVisibleColumn = i;
			column.m_pHeader->GetPos( oldSizeX, oldSizeY );
			break;
		}
	}

	bool forceShrink = false;
	int dxPerBar; // zero on window first opening
	if ( resizeCount )
	{
		int difference = buttonMaxXPos - m_lastBarWidth;
		if ( !oldSizeX )
		{
			int totalDesiredWidth = 0;
			for ( int i = 0; i < count; i++ )
			{
				column_t& column = m_ColumnsData[m_CurrentColumns[i]];
				if ( !column.m_bHidden )
					totalDesiredWidth += column.m_pHeader->GetWide();
			}
			difference = buttonMaxXPos - totalDesiredWidth;
		}
		float delta = (float)difference / (float)resizeCount;
		dxPerBar = (int)delta;
	}
	else // this is the first time we've opened the window, make sure all our colums fit! resize if needed
	{
		int totalMinWidth = 0;
		for ( int i = 0; i < count; i++ )
		{
			column_t& column = m_ColumnsData[m_CurrentColumns[i]];
			if ( !column.m_bHidden )
				totalMinWidth += column.m_iMinWidth;
		}
		dxPerBar = 0;
		if ( buttonMaxXPos < totalMinWidth )
		{
			forceShrink = true;
			float delta = (float)( buttonMaxXPos - totalMinWidth ) / (float)count;
			dxPerBar = (int)delta;
		}
	}
	m_lastBarWidth = buttonMaxXPos;

	while ( true )
	{
		// try and place headers as is - before we have to force items to be minimum width
		int x = -1;
		int columnIndex;
		for ( columnIndex = 0; columnIndex < count; columnIndex++ )
		{
			column_t& column = m_ColumnsData[m_CurrentColumns[columnIndex]];
			Panel* header = column.m_pHeader;
			if ( column.m_bHidden )
			{
				header->SetVisible( false );
				continue;
			}
			header->SetPos( x, 0 );
			header->SetVisible( true );
			if ( x + column.m_iMinWidth >= buttonMaxXPos && !forceShrink )
				break;

			int headerWidth = header->GetWide();
			if ( columnIndex == lastVisibleColumn )
			{
				headerWidth = buttonMaxXPos - x;
			}
			else if ( columnIndex == m_iColumnDraggerMoved ) // column resizing using dragger
			{
				headerWidth += dxPerBar; // adjust width of column
			}
			else if ( m_iColumnDraggerMoved == -1 )		// window is resizing
			{
				if ( column.m_bResizesWithWindow || forceShrink )
				{
					ASSERT_AT( column.m_iMinWidth <= column.m_iMaxWidth, 1510 );
					headerWidth += dxPerBar; // adjust width of column
				}
			}
			if ( headerWidth < column.m_iMinWidth && !forceShrink )
				headerWidth = column.m_iMinWidth; // adjust width of column
			else if ( headerWidth > column.m_iMaxWidth )
				headerWidth = column.m_iMaxWidth;
			header->SetSize( headerWidth, GetProportionalScaledValue( 18 ) );
			x += headerWidth;
			Panel* resizer = column.m_pResizer;
			resizer->SetVisible( columnIndex != lastVisibleColumn );
			resizer->MoveToFront();
			resizer->SetPos( x - GetProportionalScaledValue( 4 ), 0 );
			resizer->SetSize( GetProportionalScaledValue( 8 ), GetProportionalScaledValue( 18 ) );
		}
		if ( columnIndex == count )
			break;

		// we do this AFTER trying first, to let as many columns as possible try and get to their
		// desired width before we forcing the minimum width on them

		// get the total desired width of all the columns
		int totalDesiredWidth = 0;
		for ( int i = 0; i < count; i++ )
		{
			column_t& column = m_ColumnsData[m_CurrentColumns[i]];
			if ( !column.m_bHidden )
				totalDesiredWidth += column.m_pHeader->GetWide();
		}
		ASSERT_AT( totalDesiredWidth > buttonMaxXPos, 1562 );

		int i;
		for ( i = lastColumnIndex; i >= 0; i-- )
		{
			column_t& column = m_ColumnsData[m_CurrentColumns[i]];
			if ( column.m_bHidden )
				continue;
			totalDesiredWidth -= column.m_pHeader->GetWide();
			if ( totalDesiredWidth + column.m_iMinWidth <= buttonMaxXPos )
			{
				column.m_pHeader->SetSize( buttonMaxXPos - totalDesiredWidth, GetProportionalScaledValue( 18 ) );
				break;
			}
			totalDesiredWidth += column.m_iMinWidth;
			column.m_pHeader->SetSize( column.m_iMinWidth, GetProportionalScaledValue( 18 ) );
		}
#ifdef _WIN32
		ASSERT_AT( i != -1, 1586 );
#else
		if ( i == -1 )
			break;
#endif
	}
	Repaint();
	m_iColumnDraggerMoved = -1; // reset to invalid column
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::OnMousePressed( MouseCode code )
{
	if ( (unsigned int)code > MOUSE_RIGHT )
		return;
	// determine where we were pressed
	int x, y, row, column;
	input()->GetCursorPos( x, y );
	GetCellAtPos( x, y, row, column );
	bool canSelect = true;
	if ( m_LastItemSelected < 0 )
	{
		if ( m_VisibleItems.Count() > 0 && row < m_VisibleItems.Count() )
			m_LastItemSelected = 0;
		else if ( m_LastItemSelected == -1 )
			canSelect = false;
	}
	int itemID = -1;
	if ( row >= 0 && row < m_VisibleItems.Count() ) // make sure we're clicking on a real item
		itemID = m_VisibleItems[row];
	if ( canSelect && row >= 0 && row < m_VisibleItems.Count() &&
		( code != MOUSE_RIGHT || m_SelectedItems.Find( itemID ) == -1 ) )
	{
		int selectedColumn = -1;
		// if we're selecting individual cells, figure out the column
		if ( m_bCanSelectIndividualCells )
		{
			// only do column-specific positioning on single row select
			ScreenToLocal( x, y );
			for ( int i = 0; i < m_CurrentColumns.Count(); i++ )
			{
				int cellX, cellY, cellWide, cellTall;
				GetCellBounds( row, i, cellX, cellY, cellWide, cellTall );
				if ( x >= cellX && x <= cellX + cellWide )
				{
					selectedColumn = i;
					break;
				}
			}
		}
		// check for multi-select
		if ( input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT ) )
		{
			// no mulitple selecting of individual cells
			if ( m_bCanSelectIndividualCells )
			{
				if ( m_SelectedItems.Find( itemID ) == -1 )
				{
					ClearSelectedItems();
					AddSelectedItem( itemID );
					m_LastItemSelected = itemID;
				}
			}
			else // deal with 'multiple' row selection
			{
				// convert the last item selected to a row so we can multiply select by rows NOT items
				int startRow = m_VisibleItems.Find( m_LastItemSelected );
				int endRow = row;
				if ( endRow < startRow )
				{
					int swap = startRow;
					startRow = endRow;
					endRow = swap;
				}
				// clear the selection if neither control key was down - we are going to readd ALL selected items
				// in case the user changed the 'direction' of the shift add
				if ( !input()->IsKeyDown( KEY_LCONTROL ) && !input()->IsKeyDown( KEY_RCONTROL ) )
					ClearSelectedItems();
				// add any items that we haven't added
				for ( int i = startRow; i <= endRow; i++ )
				{
					// get the item indexes for these rows
					int selectedItem = m_VisibleItems[i];
					if ( m_SelectedItems.Find( selectedItem ) == -1 )
						AddSelectedItem( selectedItem );
				}
			}
		}
		// check for row-add select
		else if ( input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL ) )
		{
			// are we dealing with individual cell selection
			if ( m_bCanSelectIndividualCells )
			{
				if ( m_SelectedItems.Find( itemID ) != -1 )
				{
					if ( m_iSelectedColumn == selectedColumn )
						ClearSelectedItems();
				}
				else
				{
					// new row, so it doesn't matter if the columns line up
					ClearSelectedItems();
					AddSelectedItem( itemID );
				}
			}
			else if ( m_SelectedItems.Find( itemID ) != -1 )
			{
				// this row is already selected, remove
				m_SelectedItems.FindAndRemove( itemID );
			}
			else
			{
				// add the row to the selection
				AddSelectedItem( itemID );
			}
			m_LastItemSelected = itemID;
		}
		else // no CTRL or SHIFT keys
		{
			// reset the selection Start point
			m_LastItemSelected = itemID;
			SetSingleSelectedItem( itemID );
		}
		// if we're selecting individual cells
		if ( m_SelectedItems.Count() == 1 && m_bCanSelectIndividualCells )
		{
			m_iSelectedColumn = selectedColumn;
			SetSelectedCell( itemID, selectedColumn );
		}
		else
		{
			Repaint();
		}
	}
	// get the key focus
	RequestFocus( 0 );
	// check for context menu open
	if ( code == MOUSE_RIGHT )
	{
		int selectedItem = m_SelectedItems.Count() > 0 ? m_SelectedItems[0] : -1;
		// post it, but with the invalid row
		PostActionSignal( new KeyValues( "OpenContextMenu", "itemID", selectedItem ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Double-click act like the the item under the mouse was selected
//			and then the enter key hit
//-----------------------------------------------------------------------------
void ListPanel::OnMouseDoublePressed( MouseCode code )
{
	if ( code != MOUSE_LEFT )
		return;
	// select the item
	OnMousePressed( MOUSE_LEFT );
	// post up an enter key being hit if anything was selected
	if ( GetSelectedItemsCount() > 0 )
	{
		OnKeyCodeTyped( KEY_ENTER );
		PostActionSignal( new KeyValues( "ItemDoubleClicked", "itemID", m_SelectedItems[0] ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ListPanel::OnKeyCodeTyped( KeyCode code )
{
	if ( !m_VisibleItems.Count() )
	{
		Panel::OnKeyCodeTyped( code );
		return;
	}
	// calculate info for adjusting scrolling
	int start = GetStartItem();
	float itemCount = (float)m_VisibleItems.Count();
	int rowsPerPage = (int)GetRowsPerPage();
	int row = 0;
	if ( m_DataItems.IsValidIndex( m_LastItemSelected ) )
		row = m_VisibleItems.Find( m_LastItemSelected );
	int selectedRow;
	switch ( code )
	{
	case KEY_HOME:
		selectedRow = 0;
		break;
	case KEY_END:
		selectedRow = m_VisibleItems.Count() - 1;
		break;
	case KEY_PAGEUP:
		selectedRow = start;
		if ( start >= row )
			selectedRow = row - ( rowsPerPage - 1 );
		break;
	case KEY_PAGEDOWN:
		selectedRow = start + rowsPerPage - 1;
		if ( selectedRow <= row )
			selectedRow = row + rowsPerPage - 1;
		break;
	case KEY_UP:
		selectedRow = row - 1;
		break;
	case KEY_DOWN:
		selectedRow = row + 1;
		break;
	case KEY_LEFT:
		if ( m_bCanSelectIndividualCells && GetSelectedItemsCount() == 1 && m_iSelectedColumn >= 0 )
		{
			m_iSelectedColumn--;
			if ( m_iSelectedColumn == -1 )
				m_iSelectedColumn = 0;
			selectedRow = row;
			break;
		}
		Panel::OnKeyCodeTyped( code );
		return;
	case KEY_RIGHT:
		if ( m_bCanSelectIndividualCells && GetSelectedItemsCount() == 1 && m_iSelectedColumn >= 0 )
		{
			m_iSelectedColumn++;
			if ( m_iSelectedColumn >= m_CurrentColumns.Count() )
				m_iSelectedColumn = m_CurrentColumns.Count() - 1;
			selectedRow = row;
			break;
		}
		// fall through
		Panel::OnKeyCodeTyped( code );
		return;
	default:
		Panel::OnKeyCodeTyped( code );
		return;
	}
	if ( selectedRow >= m_VisibleItems.Count() )
		selectedRow = m_VisibleItems.Count() - 1;
	else if ( selectedRow < 0 )
		selectedRow = 0;
	m_LastItemSelected = m_VisibleItems[selectedRow];
	// make it so only one line is selected now
	SetSingleSelectedItem( m_LastItemSelected );
	if ( itemCount > rowsPerPage )
	{
		int scroll = m_vbar->GetValue();
		if ( selectedRow < scroll )
			m_vbar->SetValue( selectedRow );
		else if ( selectedRow >= rowsPerPage + scroll )
			m_vbar->SetValue( selectedRow - rowsPerPage + 1 );
	}
	InvalidateLayout( false, false );
	Repaint();
}
