//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#pragma once

#include "Panel.h"
#include "vgui/Dar.h"

namespace vgui2
{
class ScrollBar;
}

namespace vgui = vgui2;

//-----------------------------------------------------------------------------
// Purpose: A list of variable height child panels
//-----------------------------------------------------------------------------
class CPanelListPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CPanelListPanel, vgui::Panel );

public:
	CPanelListPanel( vgui2::Panel* parent, const char* panelName, bool inverseButtons );
	~CPanelListPanel() override;

	void PaintBackground() override;
	void PerformLayout() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void OnMouseWheeled( int delta ) override;

	// DATA & ROW HANDLING
	// The list now owns the panel
	virtual int computeVPixelsNeeded();
	virtual int AddItem( vgui2::Panel* panel );
	virtual int GetItemCount();
	virtual vgui2::Panel* GetItem( int itemIndex ); // returns pointer to data the row holds
	virtual void RemoveItem( int itemIndex );
	virtual void DeleteAllItems(); // clears and deletes all the memory used by the data items
	// PAINTING
	virtual vgui2::Panel* GetCellRenderer( int row );
	MESSAGE_FUNC_INT( OnSliderMoved, "ScrollBarSliderMoved", position );

	void SetSliderYOffset( int pixels );

protected:
	typedef struct dataitem_s
	{
		vgui2::Panel*	panel;
	} DATAITEM;

	DATAITEM* GetDataItem( int itemIndex );

	vgui2::Dar<DATAITEM*>	_dataItems;
	vgui2::ScrollBar*	_vbar;
	vgui2::Panel*		_embedded;
	int					_tableStartX;
	int					_tableStartY;
	int					_sliderYOffset;
};
