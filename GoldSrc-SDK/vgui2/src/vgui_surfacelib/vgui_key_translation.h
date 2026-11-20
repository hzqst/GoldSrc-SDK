#ifndef VGUI_KEY_TRANSLATION_H
#define VGUI_KEY_TRANSLATION_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui/KeyCode.h"
#include "vgui/MouseCode.h"

namespace vgui2
{

void	InitKeyTranslationTable();
void	InitVGUIToVKTranslationTable();

}

extern "C" vgui2::KeyCode	KeyCode_VirtualKeyToVGUI( int keyCode );

int							KeyCode_VGUIToVirtualKey( vgui2::KeyCode keyCode );
bool						IsKeyCode( int vk_code );
bool						IsMouseCode( int vk_code );
vgui2::MouseCode			MouseCode_VirtualKeyToVGUI( int vk_code );
int							MouseCode_VGUIToVirtualKey( vgui2::MouseCode keyCode );
// copies the full name of a key code, e.g. "KEY_A", into buf; always NUL terminated, empty for unknown codes
void						KeyCode_GetText( vgui2::KeyCode code, char* buf, int buflen );

#endif // VGUI_KEY_TRANSLATION_H
