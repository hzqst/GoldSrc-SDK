//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: interface class between viewport msg funcs and "c" based client dll
//
// $NoKeywords: $
//=============================================================================

#ifndef IVIEWPORTMSGS_H
#define IVIEWPORTMSGS_H

class IViewPortMsgs
{
public:
	virtual int MsgFunc_ValClass( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_TeamNames( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_Feign( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_Detpack( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_VGUIMenu( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_TutorText( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_TutorLine( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_TutorState( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_TutorClose( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_MOTD( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_BuildSt( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_RandomPC( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_ServerName( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_ScoreInfo( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_TeamScore( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_TeamInfo( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_Spectator( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_AllowSpec( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_SpecFade( const char* name, int size, void* buffer ) = 0;
	virtual int MsgFunc_ResetFade( const char* name, int size, void* buffer ) = 0;
};

extern IViewPortMsgs* gViewPortMsgs;

#endif // IVIEWPORTMSGS_H
