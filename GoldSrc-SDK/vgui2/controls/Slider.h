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
class TextImage;

//-----------------------------------------------------------------------------
// Labeled horizontal slider
//-----------------------------------------------------------------------------
class Slider : public Panel
{
	DECLARE_CLASS_SIMPLE( Slider, Panel );

public:
	Slider( Panel* parent, const char* panelName );
	~Slider() override;

	// interface
	virtual void SetValue( int value, bool triggerChangeMessage = true );
	virtual int GetValue();
	virtual void SetRange( int min, int max ); // set to max and min range of rows to display
	virtual void GetRange( int& min, int& max );
	virtual void GetNobPos( int& min, int& max ); // get current Slider position
	virtual void SetButtonOffset( int offset );
	virtual void SetTickCaptions( const wchar_t* left, const wchar_t* right );
	virtual void SetTickCaptions( const char* left, const char* right );
	virtual void SetNumTicks( int numTicks );
	virtual void SetThumbWidth( int width );
	virtual void RecomputeNobPosFromValue();
	virtual void RecomputeValueFromNobPos();
	virtual void SendSliderMovedMessage();
	virtual void GetTrackRect( int& x, int& y, int& width, int& height );

protected:
	void OnMouseReleased( MouseCode code ) override;
	void OnMousePressed( MouseCode code ) override;
	void OnMouseDoublePressed( MouseCode code ) override;
	void OnCursorMoved( int x, int y ) override;
	void Paint() override;
	void OnSizeChanged( int wide, int tall ) override;
	void PerformLayout() override;
	void ApplySchemeSettings( IScheme* scheme ) override;
	void GetSettings( KeyValues* resourceData ) override;
	void ApplySettings( KeyValues* resourceData ) override;
	const char* GetDescription() override;
	void PaintBackground() override;
	void OnKeyCodeTyped( KeyCode code ) override;

private:
	void DrawTicks();
	void DrawTickLabels();
	void DrawNob();

	bool		_dragging;
	int			_nobPos[2];
	int			_nobDragStartPos[2];
	int			_dragStartPos[2];
	int			_range[2];
	int			_value; // the position of the Slider, in coordinates as specified by SetRange/SetRangeWindow
	int			_buttonOffset;
	IBorder*	_sliderBorder;
	IBorder*	_insetBorder;
	float		_nobSize;
	TextImage*	_leftCaption;
	TextImage*	_rightCaption;
	int			m_nNumTicks;
};

}
