#ifndef HINTMESSAGE_H
#define HINTMESSAGE_H

#include "utlvector.h"

class CBaseEntity;

//--------------------------------------------------------------------------------------------------------------
class CHintMessage
{
public:
	CHintMessage( const char* hintString, bool isHint, CUtlVector<const char*>* args, float duration );
	~CHintMessage();
	float GetDuration() const { return m_duration; }
	void Send( CBaseEntity& client );

private:
	const char* m_hintString;					///< hintString is a pointer to a string that should never be deleted.
	bool m_isHint;
	CUtlVector<char*> m_args;				///< list of arguments.  The memory for these strings is internal to the CHintMessage.
	float m_duration;							///< time until the next message can be displayed
};


//--------------------------------------------------------------------------------------------------------------
class CHintMessageQueue
{
public:
	~CHintMessageQueue() { Reset(); }
	void Reset();
	void Update( CBaseEntity& client );
	bool AddMessage( const char* message, float duration, bool isHint, CUtlVector<const char*>* args );
	bool IsEmpty() const { return m_messages.Count() == 0; }

private:
	float m_tmMessageEnd;
	CUtlVector<CHintMessage*> m_messages;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

#endif
