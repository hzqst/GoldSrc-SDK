//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: interface class between viewport msg funcs and "c" based client dll
//
// $NoKeywords: $
//=============================================================================

#ifndef ICSTRIKEVIEWPORTMSGS_H
#define ICSTRIKEVIEWPORTMSGS_H

//#include "vgui/IViewPortMsgs.h"

class ICSViewPortMsgs /*: public IViewPortMsgs*/
{
public:
	virtual int MsgFunc_ForceCam( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_ScoreInfo( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_Location( const char* name, int size, void* buffer ) = 0;
};

extern ICSViewPortMsgs* gCSViewPortMsgs;

#endif // ICSVIEWPORTMSGS_H
