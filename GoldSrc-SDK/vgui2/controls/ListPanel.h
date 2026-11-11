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

#include "Panel.h"
#include "utllinkedlist.h"
#include "utlrbtree.h"
#include "utlvector.h"

namespace vgui2
{
class Button;
class IImage;
class ImageList;
class ImagePanel;
class Label;
class ListPanel;
class ScrollBar;
class TextImage;

struct ListPanelItem
{
	KeyValues*		kv;
	unsigned int	userData;
};

typedef int SortFunc( ListPanel* panel, const ListPanelItem& item1, const ListPanelItem& item2 );

//-----------------------------------------------------------------------------
// Purpose: A spread-sheet type data view, similar to MFC's 
//-----------------------------------------------------------------------------
class ListPanel : public Panel
{
	DECLARE_CLASS_SIMPLE( ListPanel, Panel );

public:
	ListPanel( Panel* parent, const char* panelName );
	~ListPanel() override;

	// COLUMN HANDLING
	// all indices are 0 based, limit of 255 columns
	// columns are resizable by default
	enum ColumnFlags_e
	{
		COLUMN_FIXEDSIZE			= 0x01, // set to have the column be a fixed size
		COLUMN_RESIZEWITHWINDOW	= 0x02, // set to have the column grow with the parent dialog growing
		COLUMN_IMAGE			= 0x04,	// set if the column data is not text, but instead the index of the image to display
		COLUMN_HIDDEN			= 0x08,	// column is hidden by default
		COLUMN_UNHIDABLE			= 0x10, // column is unhidable
		COLUMN_URL				= 0x20,
		COLUMN_CUSTOMCOLOR		= 0x40
	};

	// adds a column header
	virtual void AddColumnHeader( int index, const char* columnName, const char* columnText,
		int width, int minWidth, int maxWidth, int columnFlags );
	virtual void AddColumnHeader( int index, const char* columnName, const char* columnText,
		int width, int columnFlags );
	virtual void RemoveColumn( int column ); // removes a column
	virtual int FindColumn( const char* columnName );
	virtual void SetColumnHeaderHeight( int height );
	virtual void SetColumnHeaderText( int column, const char* text );
	virtual void SetColumnHeaderText( int column, wchar_t* text );
	virtual void SetColumnHeaderImage( int column, int image );
	virtual void SetColumnHeaderTooltip( int column, const char* text );
	// Get information about the column headers.
	virtual int GetNumColumnHeaders() const;
	virtual bool GetColumnHeaderText( int column, char* text, int bufferSize );
	virtual void SetSortFunc( int column, SortFunc* sortFunc );
	virtual void SetSortColumn( int column );
	virtual void SortList();
	virtual void SetColumnSortable( int column, bool sortable );
	virtual void SetColumnVisible( int column, bool visible );
	// sets whether the user can add/remove columns (defaults to off)
	virtual void SetAllowUserModificationOfColumns( bool allow );

	virtual int AddItem( const KeyValues* data, unsigned int userData, bool scrollToItem, bool sortOnAdd );
	virtual int GetItemCount(); // returns the number of VISIBLE items
	virtual int GetItem( const char* itemName ); // gets the row index of an item by name (data->GetName())
	virtual KeyValues* GetItem( int itemID ); // returns pointer to data the row holds
	virtual int GetItemCurrentRow( int itemID ); // returns -1 if invalid index or item not visible
	virtual int GetItemIDFromRow( int row ); // returns -1 if invalid row
	virtual unsigned int GetItemUserData( int itemID );
	virtual ListPanelItem* GetItemData( int itemID );
	virtual void SetUserData( int itemID, unsigned int userData );
	virtual int GetItemIDFromUserData( unsigned int userData );
	virtual void ApplyItemChanges( int itemID ); // applies any changes to the data, performed by modifying the return of GetItem() above
	virtual void RemoveItem( int itemID );
	virtual void RereadAllItems(); // updates the view with the new data
	virtual void RemoveAll();		// clears and deletes all the memory used by the data items
	virtual void DeleteAllItems();	// obselete, use RemoveAll();
	bool GetCellText( int itemID, int column, wchar_t* text, int bufferSize );
	virtual IImage* GetCellImage( int itemID, int column ); //, ImagePanel *&buffer); // returns the image held by a specific cell
	// Use these until they return InvalidItemID to iterate all the items.
	virtual int FirstItem() const;
	virtual int NextItem( int itemID ) const;
	virtual int InvalidItemID() const;
	virtual bool IsValidItemID( int itemID );
	// sets whether the dataitem is visible or not
	// it is removed from the row list when it becomes invisible, but stays in the indexes
	// this is much faster than a normal remove
	virtual void SetItemVisible( int itemID, bool visible );
	virtual void SetItemDisabled( int itemID, bool disabled );
	virtual void SetFont( HFont font );
	// image handling
	virtual void SetImageList( ImageList* imageList, bool deleteImageListWhenDone );

	// SELECTION

	// returns the count of selected items
	virtual int GetSelectedItemsCount();
	virtual int GetSelectedItem( int selectionIndex );
	virtual void ClearSelectedItems();
	virtual void AddSelectedItem( int itemID );
	// sets this single item as the only selected item
	virtual void SetSingleSelectedItem( int itemID );
	// returns the selected column, -1 for particular column selected
	virtual int GetSelectedColumn();
	virtual void SetSelectIndividualCells( bool state );
	void SetMultiSelectEnabled( bool state );
	// sets a single cell - all other previous rows are cleared
	virtual void SetSelectedCell( int itemID, int column );
	virtual bool GetCellAtPos( int x, int y, int& row, int& column ); // returns true if any found, row and column are filled out. x, y are in screen space
	virtual bool GetCellBounds( int row, int column, int& x, int& y, int& wide, int& tall );
	void SaveSelectionState();
	void RestoreSelectionState();
	// sets the text which is displayed when the list is empty
	virtual void SetEmptyListText( const char* text );
	virtual void SetEmptyListText( const wchar_t* text );
	void ResetScrollBar();
	bool IsScrollbarVisible();
	void SetIgnoreDoubleClick( bool state );
	void ScrollToItem( int itemID );

protected:
	// PAINTING
	virtual Panel* GetCellRenderer( int itemID, int column );
	void OnMouseWheeled( int delta ) override;
	void OnSizeChanged( int wide, int tall ) override;
	void PerformLayout() override;
	void Paint() override;
	void PaintBackground() override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void OnMousePressed( MouseCode code ) override;
	void OnMouseDoublePressed( MouseCode code ) override;
	void OnKeyCodeTyped( KeyCode code ) override;

	MESSAGE_FUNC( OnSliderMoved, "ScrollBarSliderMoved" );
	MESSAGE_FUNC_INT_INT( OnColumnResized, "ColumnResized", column, delta );
	MESSAGE_FUNC_INT( OnSetSortColumn, "SetSortColumn", column );
	MESSAGE_FUNC_INT( ResizeColumnToContents, "ResizeColumnToContents", column );
	MESSAGE_FUNC( OpenColumnChoiceMenu, "OpenColumnChoiceMenu" );
	MESSAGE_FUNC_INT( OnToggleColumnVisible, "ToggleColumnVisible", col );
	virtual float GetRowsPerPage();
	virtual int GetStartItem();
	void ApplyUserConfigSettings( KeyValues* settings ) override;
	void GetUserConfigSettings( KeyValues* settings ) override;
	bool HasUserConfigSettings() override;

private:
	// pre-sorted columns
	struct IndexItem_t
	{
		ListPanelItem*	dataItem;
		int				duplicateIndex;
	};

	typedef CUtlRBTree<IndexItem_t, int, bool ( * )( IndexItem_t&, IndexItem_t& )> IndexRBTree_t;

	struct column_t
	{
		Button*			m_pHeader;
		int				m_iMinWidth;
		int				m_iMaxWidth;
		int				m_iDesiredWidth;	// in case we get squeezed down by a neighbor
		bool			m_bResizesWithWindow;
		Panel*			m_pResizer;
		SortFunc*		m_pSortFunc;
		bool			m_bTypeIsText;
		bool			m_bHidden;
		bool			m_bUnhidable;
		IndexRBTree_t	m_SortedTree;

#pragma warning( suppress : 26495 )
	};

	void IndexItem( int itemID );
	void ResortColumnRBTree( int column );
	static bool RBTreeLessFunc( IndexItem_t& item1, IndexItem_t& item2 );
	void ResetColumnHeaderCommands();

	// list of the column headers
	CUtlLinkedList<column_t, unsigned char>	m_ColumnsData;
	// persistent list of all columns ever created, indexes into m_ColumnsData - used for matching up DATAITEM m_SortedTreeIndexes
	CUtlVector<unsigned char>				m_ColumnsHistory;
	// current list of columns, indexes into m_ColumnsData
	CUtlVector<unsigned char>				m_CurrentColumns;
	int										m_iColumnDraggerMoved; // which column dragger was moved->which header to resize
	int										m_lastBarWidth;
	CUtlLinkedList<ListPanelItem*, int>		m_DataItems;
	CUtlVector<int>							m_VisibleItems;
	bool									m_bNeedsSort;
	int										m_iSortColumn;
	int										m_iSortColumnSecondary;
	bool									m_bSortAscending;
	bool									m_bSortAscendingSecondary;
	TextImage*								m_pTextImage; // used in rendering
	ImagePanel*								m_pImagePanel; // used in rendering
	Label*									m_pLabel; // used in rendering
	ScrollBar*								m_hbar;
	ScrollBar*								m_vbar;
	bool									m_bCanSelectIndividualCells;
	int										m_iSelectedColumn;
	bool									m_bShiftHeldDown;
	int										m_iHeaderHeight;
	int										m_iRowHeight;
	// selection data
	CUtlVector<int>							m_SelectedItems; // array of selected rows
	int										m_LastItemSelected; // remember the last row selected for future shift clicks
	int										m_iTableStartX;
	int										m_iTableStartY;
	Color									m_LabelFgColor;
	Color									m_SelectionFgColor;
	bool									m_bAllowUserAddDeleteColumns;
	bool									m_bDeleteImageListWhenDone;
	ImageList*								m_pImageList;
	TextImage*								m_pEmptyListText;
};
}
