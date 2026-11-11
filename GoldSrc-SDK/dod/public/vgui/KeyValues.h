//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef KEYVALUES_H
#define KEYVALUES_H

#ifdef _WIN32
#pragma once
#endif

#include "../color.h"
#include "interface.h"
#include "../IKeyValues.h"

#include <stddef.h>
#include <wchar.h>

class IFileSystem;
class CUtlBuffer;

//-----------------------------------------------------------------------------
// Purpose: Simple recursive data access class
//			Destructor deletes all child KeyValues nodes
//-----------------------------------------------------------------------------
class KeyValues
{
public:
	enum types_t
	{
		TYPE_NONE	= 0,
		TYPE_STRING,
		TYPE_INT,
		TYPE_FLOAT,
		TYPE_PTR,
		TYPE_WSTRING,
		TYPE_COLOR,
		TYPE_UINT64,

		TYPE_NUMTYPES,
	};

	KeyValues( const char* setName );
	KeyValues( const char* setName, const char* firstKey, const char* firstValue );
	KeyValues( const char* setName, const char* firstKey, const wchar_t* firstValue );
	KeyValues( const char* setName, const char* firstKey, int firstValue );
	KeyValues( const char* setName, const char* firstKey, const char* firstValue,
		const char* secondKey, const char* secondValue );
	KeyValues( const char* setName, const char* firstKey, int firstValue, const char* secondKey, int secondValue );

	~KeyValues();

	void Init( const char* setName );

	virtual const char* GetName() const;
	virtual int GetNameSymbol() const;

	virtual bool LoadFromFile( IFileSystem* filesystem, const char* resourceName, const char* pathID = NULL );
	virtual bool SaveToFile( IFileSystem* filesystem, const char* resourceName, const char* pathID = NULL );

	virtual KeyValues* FindKey( const char* keyName, bool bCreate = false );
	virtual KeyValues* FindKey( int keySymbol ) const;
	virtual KeyValues* CreateNewKey();
	virtual void RemoveSubKey( KeyValues* subKey );
	virtual KeyValues* GetFirstSubKey();
	virtual KeyValues* GetNextKey();

	virtual int GetInt( const char* keyName = NULL, int defaultValue = 0 );
	virtual float GetFloat( const char* keyName = NULL, float defaultValue = 0.0f );
	virtual const char* GetString( const char* keyName = NULL, const char* defaultValue = "" );
	virtual const wchar_t* GetWString( const char* keyName = NULL, const wchar_t* defaultValue = L"" );
	virtual void* GetPtr( const char* keyName = NULL, void* defaultValue = NULL );
	virtual bool IsEmpty( const char* keyName = NULL );

	virtual void SetWString( const char* keyName, const wchar_t* value );
	virtual void SetString( const char* keyName, const char* value );
	virtual void SetInt( const char* keyName, int value );
	virtual void SetFloat( const char* keyName, float value );
	virtual void SetPtr( const char* keyName, void* value );

	virtual KeyValues* MakeCopy() const;
	virtual void Clear();
	virtual types_t GetDataType( const char* keyName = NULL );
	virtual void deleteThis();

	void LoadFromBuffer( const char* buffer );
	void AddSubKey( KeyValues* pSubkey );
	void SetNextKey( KeyValues* pDat );

	unsigned long long GetUint64( const char* keyName = NULL, unsigned long long defaultValue = 0 );
	Color GetColor( const char* keyName = NULL );
	void SetColor( const char* keyName, Color value );
	void SetUint64( const char* keyName, unsigned long long value );

	void* operator new( size_t iAllocSize );
	void* operator new( size_t iAllocSize, int nBlockUse, const char* pFileName, int nLine );
	void operator delete( void* pMem );
	void operator delete( void* pMem, int nBlockUse, const char* pFileName, int nLine );

private:
	void RecursiveLoadFromBuffer( char** buffer );
	void WriteConvertedString( CUtlBuffer& buffer, const char* pszString );
	void RecursiveSaveToFile( IFileSystem* filesystem, CUtlBuffer& buffer, int indentLevel );
	void FreeAllocatedValue();
	void AllocateValueBlock( int size );

	int	m_iKeyName;

	union
	{
		int				m_iValue;
		float			m_flValue;
		void*			m_pValue;
		char*			m_sValue;
		wchar_t*		m_wsValue;
		unsigned char	m_Color[4];
	};

	unsigned short	m_iDataType;
	short			m_iAllocationSize;

	KeyValues*	m_pPeer;	// pointer to next key in list
	KeyValues*	m_pSub;	// pointer to Start of a new sub key list
};

IKeyValues*	keyvalues();

#endif
