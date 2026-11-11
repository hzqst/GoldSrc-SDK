//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#pragma once

#include "Panel.h"
#include "utllinkedlist.h"
#include "utlvector.h"

namespace vgui2
{
class ScrollBar;

//-----------------------------------------------------------------------------
// Purpose: A list of variable height child panels
//  each list item consists of a label-panel pair. Height of the item is
// determined from the lable.
//-----------------------------------------------------------------------------
class PanelListPanel : public Panel
{
	DECLARE_CLASS_SIMPLE( PanelListPanel, Panel );

public:
	typedef struct dataitem_s
	{
		// Always store a panel pointer
		Panel* panel;
		Panel* labelPanel;
	} DATAITEM;

	PanelListPanel( Panel* parent, const char* panelName );
	~PanelListPanel() override;

	virtual int computeVPixelsNeeded();

	virtual int AddItem( Panel* labelPanel, Panel* panel );

	virtual int GetItemCount();
	virtual Panel* GetItemLabel( int itemID );
	virtual Panel* GetItemPanel( int itemID );

	virtual void RemoveItem( int itemID ); // removes an item from the table (changing the indices of all following items)
	virtual void DeleteAllItems();			// clears and deletes all the memory used by the data items

	virtual Panel* GetCellRenderer( int row );

	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );

	virtual void MoveScrollBarToTop();

	void RemoveAll();
	void SetFirstColumnWidth( int width );
	int GetFirstColumnWidth();
	void OnSizeChanged( int wide, int tall ) override;
	void PerformLayout() override;
	void PaintBackground() override;
	void ApplySchemeSettings( IScheme* pScheme ) override;

private:

	// list of the column headers

	CUtlLinkedList<DATAITEM*, int> m_DataItems;
	CUtlVector<int> m_SortedItems;
	ScrollBar* m_vbar;
	Panel* m_pPanelEmbedded;
	int m_iFirstColumnWidth;
	int m_iScrollbarSize;
	int m_iDefaultHeight;
	int m_iPanelBuffer;
};
}
