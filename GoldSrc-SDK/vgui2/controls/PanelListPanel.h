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
#include "utlvector.h"

namespace vgui2
{
class ScrollBar;

//-----------------------------------------------------------------------------
// Purpose: A list of variable height child panels
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

	// DATA & ROW HANDLING
	// The list now owns the panel
	virtual int computeVPixelsNeeded();

	virtual int AddItem( Panel* labelPanel, Panel* panel );

	virtual int GetItemCount();
	virtual Panel* GetItemLabel( int itemID );
	virtual Panel* GetItemPanel( int itemID );

	virtual void RemoveItem( int itemID );
	virtual void DeleteAllItems();			// clears and deletes all the memory used by the data items

	// PAINTING
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
