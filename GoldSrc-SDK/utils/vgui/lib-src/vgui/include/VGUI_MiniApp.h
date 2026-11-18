#ifndef VGUI_MINIAPP_H
#define VGUI_MINIAPP_H

#include "VGUI.h"

namespace vgui
{

class Frame;

class VGUIAPI MiniApp
{
public:
	MiniApp();
	MiniApp( const MiniApp& other );
#if VGUI_ABI_COMPAT_MODE != 1
	virtual ~MiniApp();
#endif

	virtual void getName( char* name, int nameLen );
	virtual Frame* createInstance() = 0;

protected:
	virtual void setName( const char* name );

private:
	char*	_name;

public:
	MiniApp& operator=( const MiniApp& other );
};

}

#endif
