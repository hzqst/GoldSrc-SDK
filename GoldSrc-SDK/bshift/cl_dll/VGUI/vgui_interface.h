#pragma once

#include "game_controls/iviewport.h"
#include "game_controls/iviewportmsgs.h"

class CCreateCommandComboMenu;

CCreateCommandComboMenu*	CreateCommandCombo();
int						GetMaxTeams();

extern "C" DLL_EXPORT void HUD_ChatInputPosition( int* x, int* y );
