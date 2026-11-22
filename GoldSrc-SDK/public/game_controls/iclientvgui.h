#pragma once

#include "interface.h"

#define CLIENTVGUI_INTERFACE_VERSION "VClientVGUI001"

#include <stdint.h>

namespace vgui2
{
	typedef uintptr_t VPANEL;
}

class IClientVGUI : public IBaseInterface
{
public:
	virtual void Initialize( CreateInterfaceFn* factories, int count ) = 0;
	virtual void Start() = 0;
	virtual void SetParent( vgui2::VPANEL parent ) = 0;
	virtual bool UseVGUI1() = 0;
	virtual void HideScoreBoard() = 0;
	virtual void HideAllVGUIMenu() = 0;
	virtual void ActivateClientUI() = 0;
	virtual void HideClientUI() = 0;
	virtual void Shutdown() = 0;
};
