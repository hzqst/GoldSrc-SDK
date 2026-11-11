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

namespace vgui2
{
class IBorder;

//-----------------------------------------------------------------------------
// Purpose: ScrollBarSlider bar, as used in ScrollBar's
//-----------------------------------------------------------------------------
class ScrollBarSlider : public Panel
{
	DECLARE_CLASS_SIMPLE( ScrollBarSlider, Panel );

public:
	ScrollBarSlider( Panel* parent, const char* name, bool vertical );
	~ScrollBarSlider() override;

	// Set the ScrollBarSlider value of the nob.
	virtual void SetValue( int value );
	virtual int GetValue();

	// Check whether the scroll bar is vertical or not
	virtual bool IsVertical();

	// Set max and min range of lines to display
	virtual void SetRange( int minimum, int maximum );
	virtual void GetRange( int& minimum, int& maximum );

	// Set number of rows that can be displayed in window
	virtual void SetRangeWindow( int window );

	// Get number of rows that can be displayed in window
	virtual int GetRangeWindow();

	virtual void SetSize( int width, int height );

	// Get current ScrollBarSlider bounds
	virtual void GetNobPos( int& minimum, int& maximum );

	virtual bool HasFullRange();
	virtual void SetButtonOffset( int offset );
	void OnCursorMoved( int x, int y ) override;
	void OnMousePressed( MouseCode code ) override;
	void OnMouseDoublePressed( MouseCode code ) override;
	void OnMouseReleased( MouseCode code ) override;

protected:
	void Paint() override;
	void PaintBackground() override;
	void PerformLayout() override;
	void ApplySchemeSettings( IScheme* scheme ) override;

private:
	virtual void RecomputeNobPosFromValue();
	virtual void RecomputeValueFromNobPos();
	virtual void SendScrollBarSliderMovedMessage();
	bool _vertical;
	bool _dragging;
	int _nobPos[2];
	int _nobDragStartPos[2];
	int _dragStartPos[2];
	int _range[2];
	int _value;		// the position of the ScrollBarSlider, in coordinates as specified by SetRange/SetRangeWindow
	int _rangeWindow;
	int _buttonOffset;
	IBorder* _ScrollBarSliderBorder;
};
}
