#ifndef VGUIKEYVALUES_H
#define VGUIKEYVALUES_H

#ifdef _WIN32
#pragma once
#endif

#include "IKeyValues.h"
#include "MemPool.h"
#include "utlvector.h"
#include "utlrbtree.h"

class CVGuiKeyValues : public IKeyValues
{
public:
	CVGuiKeyValues();
	virtual ~CVGuiKeyValues() override;

	virtual void RegisterSizeofKeyValues( int size ) override;
	virtual void* AllocKeyValuesMemory( int size ) override;
	virtual void FreeKeyValuesMemory( void* pMem ) override;

	virtual HKeySymbol GetSymbolForString( const char* name ) override;
	virtual const char* GetStringForSymbol( HKeySymbol symbol ) override;

	virtual void GetLocalizedFromANSI( const char* ansi, wchar_t* outBuf, int unicodeBufferSizeInBytes ) override;
	virtual void GetANSIFromLocalized( const wchar_t* wchar, char* outBuf, int ansiBufferSizeInBytes ) override;

	virtual void AddKeyValuesToMemoryLeakList( void* pMem, HKeySymbol name ) override;
	virtual void RemoveKeyValuesFromMemoryLeakList( void* pMem ) override;

private:
	CMemoryPool*	m_pMemPool;
	int				m_iMaxKeyValuesSize;

	CUtlVector<char>	m_Strings;

	struct hash_item_t
	{
		int				stringIndex;
		hash_item_t*	next;
	};

	CMemoryPool				m_HashItemMemPool;
	CUtlVector<hash_item_t>	m_HashTable;

	struct MemoryLeakTracker_t
	{
		int		nameIndex;
		void*	pMem;
	};

	static bool MemoryLeakTrackerLessFunc( const MemoryLeakTracker_t& lhs, const MemoryLeakTracker_t& rhs );

	CUtlRBTree<MemoryLeakTracker_t, int>	m_KeyValuesTrackingList;

	int CaseInsensitiveHash( const char* string, int iBounds );
};

#endif
