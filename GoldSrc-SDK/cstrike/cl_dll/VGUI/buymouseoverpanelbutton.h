//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef BUYMOUSEOVERPANELBUTTON_H
#define BUYMOUSEOVERPANELBUTTON_H

#include "game_controls/mouseoverpanelbutton.h"
//#include "Exports.h"

//-----------------------------------------------------------------------------
// Purpose: Triggers a new panel when the mouse goes over the button
//-----------------------------------------------------------------------------
class BuyMouseOverPanelButton : public MouseOverPanelButton
{
public:
	BuyMouseOverPanelButton( vgui2::Panel* parent, const char* name, vgui2::Panel* panel );
	void ApplySettings( KeyValues* settings ) override;
	int GetASRestrict() { return m_iASRestrict; }
	int GetDEUseOnly() { return m_iDEUseOnly; }

private:
	void PerformLayout() override;
	void SetPriceState();
	void SetMapTypeState();
	int m_iPrice;
	int m_iASRestrict;
	int m_iDEUseOnly;
};

#endif // BUYMOUSEOVERPANELBUTTON_H
