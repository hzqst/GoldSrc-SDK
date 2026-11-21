#include "ipratelimit.h"
#include "cvar.h"

#include <cstring>
#include <ctime>

extern cvar_t max_queries_sec;
extern cvar_t max_queries_sec_global;
extern cvar_t max_queries_window;

CIPRateLimit::CIPRateLimit() : m_IPTree( 0, TREE_START_SIZE, LessIP )
{
	m_iGlobalCount = 0;
	m_lLastTime = -1;
}

CIPRateLimit::~CIPRateLimit()
{
}

bool CIPRateLimit::LessIP( const iprate_s& lhs, const iprate_s& rhs )
{
	return lhs.ip < rhs.ip;
}

bool CIPRateLimit::CheckIP( netadr_t adr )
{
	long curTime = (long)time( NULL );
	int ip;
	float window;
	int i;

	memcpy( &ip, adr.ip, sizeof( ip ) );

	if ( m_IPTree.Count() > TREE_MAX_SIZE )
	{
		int checked = 0;

		i = m_IPTree.LastInorder();
		while ( m_IPTree.Count() > TREE_MAX_SIZE * 2 / 3 && checked < m_IPTree.MaxElement() )
		{
			if ( !m_IPTree.IsValidIndex( i ) || curTime - m_IPTree.Element( i ).lastTime <= TREE_EXPIRE_SECONDS
				 || m_IPTree.Element( i ).ip == ip )
			{
				checked++;
				i = m_IPTree.PrevInorder( i );
				continue;
			}

			int prev = m_IPTree.PrevInorder( i );
			m_IPTree.RemoveAt( i );
			i = prev;
		}
	}

	iprate_s find = {};
	find.ip = ip;
	i = m_IPTree.Find( find );
	window = max_queries_window.value;

	if ( !m_IPTree.IsValidIndex( i ) )
	{
		iprate_s entry;
		entry.ip = ip;
		entry.lastTime = curTime;
		entry.count = 1;
		m_IPTree.Insert( entry );
	}
	else
	{
		iprate_s& entry = m_IPTree.Element( i );

		entry.count++;
		if ( curTime - entry.lastTime <= window )
		{
			if ( entry.count / window > max_queries_sec.value )
				return false;
		}
		else
		{
			entry.lastTime = curTime;
			entry.count = 1;
		}
	}

	m_iGlobalCount++;
	if ( curTime - m_lLastTime <= window )
		return m_iGlobalCount / window <= max_queries_sec_global.value;

	m_iGlobalCount = 1;
	m_lLastTime = curTime;
	return true;
}
