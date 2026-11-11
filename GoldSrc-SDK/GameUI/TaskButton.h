//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#pragma once

#include "Image.h"
#include "Label.h"
#include "vgui/IVGui.h"

//-----------------------------------------------------------------------------
// Purpose: Image of the window state
//-----------------------------------------------------------------------------
class CWindowStateImage : public vgui2::Image
{
public:
	CWindowStateImage();
	~CWindowStateImage() override;

	// sets the height of the image
	void SetTall( int tall );
	// sets the image state
	void SetFocused();
	void SetOutOfFocus();
	void SetMinimized();
	void Paint() override;

private:
	int m_iState;
};

//-----------------------------------------------------------------------------
// Purpose: Button for a specific window
//-----------------------------------------------------------------------------
class CTaskButton : public vgui2::Label
{
public:
	CTaskButton( vgui2::Panel* parent, vgui2::VPANEL task );
	~CTaskButton() override;

	// Sets the title of the task button
	void SetTitle( const wchar_t* title );
	// Sets the selection state of the task button
	void SetSelected( bool state );
	// returns true if the task button should be displayed
	bool ShouldDisplay();
	vgui2::VPANEL GetTaskPanel();

protected:
	void OnMousePressed( vgui2::MouseCode mouseCode ) override;
	void OnMouseDoublePressed( vgui2::MouseCode mouseCode ) override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

private:
	vgui2::HPanel m_hTask; // panel handle to the associated task
	bool m_bHasTitle;
	bool m_bSelected;
	Color m_SelectedColor;
	Color m_LabelColor;
	CWindowStateImage m_WindowStateImage;
};
