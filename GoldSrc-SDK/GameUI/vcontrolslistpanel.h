//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#pragma once

#include "SectionedListPanel.h"

class CInlineEditPanel;

//-----------------------------------------------------------------------------
// Purpose: Special list subclass to handle drawing of trap mode prompt on top of
//			lists client area
//-----------------------------------------------------------------------------
class VControlsListPanel : public vgui2::SectionedListPanel
{
public:
	// Construction
	VControlsListPanel( vgui2::Panel* parent, const char* listName );
	~VControlsListPanel() override;

	virtual void StartCaptureMode( vgui2::HCursor cursor );
	virtual void EndCaptureMode( vgui2::HCursor cursor );
	virtual bool IsCapturing();
	// Set which item should be associated with the prompt
	virtual void SetItemOfInterest( int itemID );
	virtual int GetItemOfInterest();

protected:
	void OnMousePressed( vgui2::MouseCode code ) override;
	void OnMouseDoublePressed( vgui2::MouseCode code ) override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

private:
	// Are we showing the prompt?
	bool					m_bCaptureMode;
	// If so, where?
	int						m_nClickRow;
	vgui2::HFont				m_hFont;
	CInlineEditPanel*		m_pInlineEditPanel;
	int						m_iMouseX;
	int						m_iMouseY;
};
