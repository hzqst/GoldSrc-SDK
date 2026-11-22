#pragma once

#include "interface.h"

#include <stdint.h>

namespace vgui2
{
typedef uintptr_t VPANEL;
}

#ifdef _MSC_VER
class __declspec( novtable ) IGameConsole : public IBaseInterface
#else
class IGameConsole : public IBaseInterface
#endif
{
public:
	virtual void Activate() = 0;
	virtual void Initialize() = 0;
	virtual void Hide() = 0;
	virtual void Clear() = 0;
	virtual bool IsConsoleVisible() = 0;

	virtual void Printf( const char* format, ... ) = 0;
	virtual void DPrintf( const char* format, ... ) = 0;

	virtual void SetParent( vgui2::VPANEL parent ) = 0;
};
