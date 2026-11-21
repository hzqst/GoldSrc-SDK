#pragma once

#include "net.h"
#include "utlrbtree.h"

class CIPRateLimit
{
public:
	CIPRateLimit();
	~CIPRateLimit();

	bool CheckIP( netadr_t adr );

private:
	enum
	{
		TREE_START_SIZE		= 256,
		TREE_MAX_SIZE		= 512,
		TREE_EXPIRE_SECONDS	= 120
	};

	struct iprate_s
	{
		int				ip;
		long			lastTime;
		int				count;
	};

	static bool LessIP( const iprate_s& lhs, const iprate_s& rhs );

	CUtlRBTree<iprate_s, int>	m_IPTree;
	int							m_iGlobalCount;
	long						m_lLastTime;
};
