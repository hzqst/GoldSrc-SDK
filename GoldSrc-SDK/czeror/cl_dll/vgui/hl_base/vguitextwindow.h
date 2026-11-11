//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUITEXTWINDOW_H
#define VGUITEXTWINDOW_H

#include "Frame.h"

namespace vgui2
{
class TextEntry;
class Button;
}

//-----------------------------------------------------------------------------
// Purpose: displays the MOTD
//-----------------------------------------------------------------------------
class CVGUITextWindow : public vgui2::Frame
{
public:
	CVGUITextWindow( vgui2::Panel* parent );
	~CVGUITextWindow() override;
	void Activate( const char* title, const char* message );
	void Activate( const wchar_t* title, const wchar_t* message );
	void OnCommand( const char* command ) override;

private:
	vgui2::TextEntry* m_pMessage;
	vgui2::Button* m_pOK;
};

#endif
