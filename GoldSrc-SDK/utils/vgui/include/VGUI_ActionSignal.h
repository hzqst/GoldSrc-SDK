//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_ACTIONSIGNAL_H
#define VGUI_ACTIONSIGNAL_H

#include<VGUI.h>

/*

TODO: maybe try something like this..

class VGUIAPI ActionSignal
{
};

class VGUIAPI ActionSignalSimple : public ActionSignal
{
public:
	virtual void actionPerformed(Panel* panel)=0;
};

class VGUIAPI ActionSignalInt : public ActionSignal
{
public:
	virtual void actionPerformed(int value,Panel* panel)=0;
};


DefaultButtonModel would implement:

virtual void addActionSignal(ActionSignal* s)
{
	if(s!=null)
	{
		_actionSignalDar.putElement(s);
	}
}

virtual void fireActionSignal()
{
	for(int i=0;i<_actionSignalDar.getCount();i++)
	{
		ActionSignal* signal=_actionSignalDar[i];
		
		ActionSignalSimple* ss=dynamic_cast<ActionSignalSimple*>(signal);
		if(ss!=null)
		{
			ss->actionPerformed(this);
		}
	
		ActionSignalCommand* si=dynamic_cast<ActionSignalInt*>(signal);
		if(si!=null)
		{
			si->actionPerformed(_intValue,this);
		}
	}
}


*/

#include<VGUI.h>

namespace vgui
{

class Panel;

class VGUIAPI ActionSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
	// Buttons and TextEntries delete their action signals through this
	// interface.
	virtual ~ActionSignal() {}
#endif
	virtual void actionPerformed(Panel* panel)=0;
};

}

#endif


