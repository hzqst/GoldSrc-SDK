//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef MOUSEOVERPANELBUTTON_H
#define MOUSEOVERPANELBUTTON_H

#include "Button.h"

extern vgui2::Panel* g_lastPanel;

class MouseOverPanelButton : public vgui2::Button
{
public:
	MouseOverPanelButton( vgui2::Panel* parent, const char* name, vgui2::Panel* panel )
		: vgui2::Button( parent, name, "MouseOverPanelButton" ), m_pPanel( panel ), m_iClass( 0 )
	{
	}
	void SetClass( int, int classMask )
	{
		m_iClass = classMask;
	}
	int GetClass() { return m_iClass; }
	virtual void SetPanel( vgui2::Panel* panel )
	{
		m_pPanel = panel;
		m_pPanel->SetName( GetName() );
	}
	void SetVisible( bool state ) override
	{
		vgui2::Panel::SetVisible( state );
		m_pPanel->SetName( GetName() );
	}
	void ShowPage()
	{
		if ( m_pPanel )
		{
			m_pPanel->SetVisible( true );
			m_pPanel->MoveToFront();
			g_lastPanel = m_pPanel;
		}
	}
	void OnCursorEntered() override
	{
		vgui2::Button::OnCursorEntered();
		if ( m_pPanel )
		{
			if ( g_lastPanel )
				g_lastPanel->SetVisible( false );
			ShowPage();
		}
	}

private:
	vgui2::Panel* m_pPanel;
	int m_iClass;
};

#endif // MOUSEOVERPANELBUTTON_H
