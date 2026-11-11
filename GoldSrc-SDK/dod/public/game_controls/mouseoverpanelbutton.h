//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Button.h"

extern vgui2::Panel* g_lastPanel;

class MouseOverPanelButton : public vgui2::Button
{
public:
	MouseOverPanelButton( vgui2::Panel* parent, const char* name, vgui2::Panel* panel ) :
		vgui2::Button( parent, name, "MouseOverPanelButton" ), m_pPanel( panel ), m_iClass( 0 )
	{
	}

	void SetClass( int classNumber, int classMask );
	int GetClass() { return m_iClass; }
	virtual void SetPanel( vgui2::Panel* panel ) { m_pPanel = panel; }

	void ShowPage()
	{
		if ( m_pPanel )
		{
			m_pPanel->SetVisible( true );
			m_pPanel->MoveToFront();
			g_lastPanel = m_pPanel;
		}
	}

	void HidePage()
	{
		if ( m_pPanel )
			m_pPanel->SetVisible( false );
	}

private:
	void OnCursorEntered() override;

	vgui2::Panel*	m_pPanel;
	int				m_iClass;
};
