#pragma once

#include "pman_main.h"

class ForceMember
{
public:
	ForceMember()
	{
		m_pNext = NULL;
		m_pPrevious = NULL;
		m_flRadius = 0;
		m_flStrength = 0;
		m_flDieTime = 0;
	}

	int			m_iIndex;
	ForceMember*	m_pNext;
	ForceMember*	m_pPrevious;
	Vector		m_vOrigin;
	Vector		m_vDirection;
	float		m_flRadius;
	float		m_flStrength;
	float		m_flDieTime;
};

class ForceList
{
public:
	ForceList()
	{
		m_iElements = 0;
		pFirst = NULL;
		pCurrent = NULL;
		pLast = NULL;
	}

	ForceMember* AddForceMember()
	{
		if ( m_iElements >= 128 )
			return NULL;

		if ( !pFirst )
		{
			m_iElements++;

			pFirst = new ForceMember;
			pFirst->m_iIndex = m_iElements;
			pCurrent = pFirst;
			pLast = pCurrent;
		}
		else
		{
			m_iElements++;

			pCurrent->m_pNext = new ForceMember;
			pCurrent->m_pNext->m_iIndex = m_iElements;
			pCurrent->m_pNext->m_pPrevious = pCurrent;
			pCurrent = pCurrent->m_pNext;
			pLast = pCurrent;
		}

		return pCurrent;
	}

	void ClearForceMembers()
	{
		ForceMember* pTemp = pFirst;

		while ( pTemp )
		{
			ForceMember* pNext = pTemp->m_pNext;
			delete pTemp;
			pTemp = pNext;
		}

		pFirst = NULL;
		pCurrent = NULL;
		pLast = NULL;
		m_iElements = 0;
	}

	void UpdateForce( float time )
	{
		ForceMember* pTemp = pFirst;

		while ( pTemp )
		{
			ForceMember* pNext = pTemp->m_pNext;

			if ( pTemp->m_flDieTime != 0 && time > pTemp->m_flDieTime )
			{
				if ( pTemp->m_pPrevious )
					pTemp->m_pPrevious->m_pNext = pNext;
				else
					pFirst = pNext;

				if ( pNext )
					pNext->m_pPrevious = pTemp->m_pPrevious;
				else
					pLast = pTemp->m_pPrevious;

				if ( pCurrent == pTemp )
					pCurrent = pLast;

				delete pTemp;
				m_iElements--;
			}

			pTemp = pNext;
		}

		pTemp = pFirst;

		while ( pTemp )
		{
			CMiniMem::ApplyForce( pTemp->m_vOrigin, pTemp->m_vDirection, pTemp->m_flRadius, pTemp->m_flStrength );
			pTemp = pTemp->m_pNext;
		}
	}

	~ForceList()
	{
		ClearForceMembers();
	}

	int			m_iElements;
	ForceMember*	pFirst;
	ForceMember*	pCurrent;
	ForceMember*	pLast;
};
