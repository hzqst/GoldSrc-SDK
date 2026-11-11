//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Button.h"

namespace vgui2
{
class IImage;
class Menu;
}

class CBackgroundMenuButton : public vgui2::Button
{
public:
	CBackgroundMenuButton( vgui2::Panel* parent, const char* name );
	~CBackgroundMenuButton() override;

	void SetImages( vgui2::IImage* image, vgui2::IImage* mouseOverImage );
	void SetArmed( bool state ) override;

protected:
	void OnCommand( const char* command ) override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

private:
	vgui2::Menu* m_pMenu;
	vgui2::IImage* m_pImage;
	vgui2::IImage* m_pMouseOverImage;
};
