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
#include "PHandle.h"
#include "utllinkedlist.h"
#include "utlvector.h"

namespace vgui2
{
class CItemButton;
class CSectionHeader;
class ImageList;
class ScrollBar;
class SectionedListPanel;

// sorting function, should return true if itemID1 should be displayed before itemID2
typedef bool ( *SectionSortFunc_t )( SectionedListPanel* list, int itemID1, int itemID2 );

//-----------------------------------------------------------------------------
// Purpose: List panel control that is divided up into discrete sections
//-----------------------------------------------------------------------------
class SectionedListPanel : public Panel
{
	DECLARE_CLASS_SIMPLE( SectionedListPanel, Panel );

public:
	SectionedListPanel( Panel* parent, const char* name );
	~SectionedListPanel() override;

	enum
	{
		COLUMN_HEADER_IMAGE = 0x01, // set if the header for the column is an image instead of text
		COLUMN_IMAGE = 0x02, // set if the column contains an image instead of text (images are looked up by index from the ImageList) (see SetImageList below)
		COLUMN_BRIGHT = 0x04, // set if the column text should be the bright color
		COLUMN_CENTER = 0x08, // set to center the text/image in the column
		COLUMN_RIGHT = 0x10
	};

	virtual void AddSection( int sectionID, const char* name, SectionSortFunc_t sortFunc = NULL );
	virtual void AddSection( int sectionID, const wchar_t* name, SectionSortFunc_t sortFunc = NULL );
	// clears all the sections - leaves the items in place
	virtual void RemoveAllSections();
	// modifies section info
	virtual void SetSectionFgColor( int sectionID, Color color );
	virtual void SetSectionAlwaysVisible( int sectionID, bool visible );
	virtual bool IsSectionVisible( int sectionID );
	virtual bool AddColumnToSection( int sectionID, const char* columnName, const char* columnText,
		int columnFlags, int width, HFont fallbackFont = 0 );
	virtual bool AddColumnToSection( int sectionID, const char* columnName, const wchar_t* columnText,
		int columnFlags, int width, HFont fallbackFont = 0 );
	// modifies the text in an existing column
	virtual bool ModifyColumn( int sectionID, const char* columnName, const wchar_t* columnText, int width );
	// adds an item to the list; returns the itemID of the new item
	virtual int AddItem( int sectionID, const KeyValues* data );
	// modifies an existing item; returns false if the item does not exist
	virtual bool ModifyItem( int itemID, int sectionID, const KeyValues* data );
	// removes an item from the list; returns false if the item does not exist or is already removed
	virtual bool RemoveItem( int itemID );
	virtual void RemoveAll();
	virtual void DeleteAllItems();
	// set the text color of an item
	virtual void SetItemFgColor( int itemID, Color color );
	// returns the number of columns in a section
	virtual int GetColumnCountBySection( int sectionID );
	// returns the name of a column by section and column index; returns NULL if there are no more columns
	// valid range of columnIndex is [0, GetColumnCountBySection)
	virtual const char* GetColumnNameBySection( int sectionID, int columnIndex );
	virtual const wchar_t* GetColumnTextBySection( int sectionID, int columnIndex );
	virtual int GetColumnFlagsBySection( int sectionID, int columnIndex );
	virtual int GetColumnWidthBySection( int sectionID, int columnIndex );
	virtual HFont GetColumnFallbackFontBySection( int sectionID, int columnIndex );
	virtual int GetSelectedItem();
	// sets which item is currently selected
	virtual void SetSelectedItem( int itemID );
	// returns the data of a selected item
	// InvalidateItem(itemID) needs to be called if the KeyValues are modified
	virtual KeyValues* GetItemData( int itemID );
	// returns what section an item is in
	virtual int GetItemSection( int itemID );
	virtual void InvalidateItem( int itemID );
	// returns true if the itemID is valid for use
	virtual bool IsItemIDValid( int itemID );
	virtual int GetHighestItemID();
	virtual int GetItemCount();
	// returns the item ID from the row, again ignoring section dividers - valid from [0, GetItemCount )
	virtual int GetItemIDFromRow( int row );
	// gets the local coordinates of a cell
	virtual bool GetCellBounds( int itemID, int column, int& x, int& y, int& wide, int& tall );
	// set up a field for editing
	virtual void EnterEditMode( int itemID, int column, Panel* editPanel );
	// leaves editing mode
	virtual void LeaveEditMode();
	// returns true if we are currently in inline editing mode
	virtual bool IsInEditMode();
	// sets whether or not the vertical scrollbar should ever be displayed
	virtual void SetVerticalScrollbar( bool state );
	// returns the size required to fully draw the contents of the panel
	virtual void GetContentSize( int& wide, int& tall );
	// image handling
	virtual void SetImageList( ImageList* imageList, bool deleteImageListWhenDone );
	virtual void ScrollToItem( int itemID );
	MESSAGE_FUNC( OnSliderMoved, "ScrollBarSliderMoved" );

	void SetProportional( bool state ) override;
	void OverrideLineSpacing( int spacing );
	int GetLineSpacing() const;

protected:
	void ApplySchemeSettings( IScheme* scheme ) override;
	void ApplySettings( KeyValues* resourceData ) override;
	void PerformLayout() override;
	void OnSizeChanged( int wide, int tall ) override;
	void OnSetFocus() override;
	void OnMousePressed( MouseCode code ) override;
	void OnMouseWheeled( int delta ) override;
	void OnKeyCodeTyped( KeyCode code ) override;

private:
	friend class CItemButton;
	friend class CSectionHeader;

	struct column_t
	{
		char		m_szColumnName[32];
		wchar_t		m_szColumnText[64];
		int			m_iColumnFlags;
		int			m_iWidth;
		HFont		m_hFallbackFont;
	};

	struct section_t
	{
		int					m_iID;
		bool				m_bAlwaysVisible;
		CSectionHeader*		m_pHeader;
		CUtlVector<column_t>	m_Columns;
		SectionSortFunc_t	m_pSortFunc;

#pragma warning( suppress : 26495 )
	};

	void AddSectionHelper( int sectionID, CSectionHeader* header, SectionSortFunc_t sortFunc );
	int FindSectionIndexByID( int sectionID );
	int GetNewItemButton();
	void SetSelectedItem( CItemButton* item );
	void LayoutPanels( int& contentTall );
	void ReSortList();
	int GetSectionTall();

	DHANDLE<CItemButton>				m_hSelectedItem;
	CUtlVector<section_t>			m_Sections;
	CUtlLinkedList<CItemButton*, int>	m_Items;
	CUtlLinkedList<CItemButton*, int>	m_FreeItems;
	CUtlVector<CItemButton*>			m_SortedItems;
	PHandle							m_hEditModePanel;
	int								m_iEditModeItemID;
	int								m_iEditModeColumn;
	int								m_iScrollbarSize;
	int								m_iContentHeight;
	int								m_iLineSpacing;
	ScrollBar*						m_pScrollBar;
	ImageList*						m_pImageList;
	bool							m_bDeleteImageListWhenDone;
	bool							m_bSortNeeded;
	bool							m_bVerticalScrollbarEnabled;
	int								m_iLineSpacingOverride;
};
} // namespace vgui
