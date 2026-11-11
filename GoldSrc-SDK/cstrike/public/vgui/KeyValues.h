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

#pragma once

#include "../../../engine/color.h"

#include <stddef.h>
#include <wchar.h>

class IFileSystem;
class IKeyValues;
class CUtlBuffer;

//-----------------------------------------------------------------------------
// Purpose: Simple recursive data access class
//			Used in vgui for message parameters and resource files
//			Destructor deletes all child KeyValues nodes
//			Data is stored in key (string names) - (string/int/float)value pairs called nodes.
//
//	About KeyValues Text File Format:

//	It has 3 control characters '{', '}' and '"'. Names and values may be quoted or
//	not. The quote '"' character must not be used within name or values, only for
//	quoting whole tokens. You may use escape sequences wile parsing and add within a
//	quoted token a \" to add quotes within your name or token. When using Escape
//	Sequence the parser must now that by setting KeyValues::UsesEscapeSequences( true ),
//	which it's off by default. Non-quoted tokens ends with a whitespace, '{', '}' and '"'.
//	So you may use '{' and '}' within quoted tokens, but not for non-quoted tokens.
//  An open bracket '{' after a key name indicates a list of subkeys which is finished
//  with a closing bracket '}'. Subkeys use the same definitions recursively.
//  Whitespaces are space, return, newline and tabulator. Allowed Escape sequences
//	are \n, \t, \\, \n and \". The number character '#' is used for macro purposes 
//	(eg #include), don't use it as first character in key names.
//-----------------------------------------------------------------------------
class KeyValues
{
public:
	enum types_t
	{
		TYPE_NONE = 0,
		TYPE_STRING,
		TYPE_INT,
		TYPE_FLOAT,
		TYPE_PTR,
		TYPE_WSTRING,
		TYPE_COLOR,
		TYPE_UINT64
	};

	KeyValues( const char* name );
	KeyValues( const char* name, const char* firstKey, const char* firstValue );
	KeyValues( const char* name, const char* firstKey, const wchar_t* firstValue );
	KeyValues( const char* name, const char* firstKey, int firstValue );
	KeyValues( const char* name, const char* firstKey, const char* firstValue,
		const char* secondKey, const char* secondValue );
	KeyValues( const char* name, const char* firstKey, int firstValue, const char* secondKey, int secondValue );

	virtual const char* GetName() const;
	virtual int GetNameSymbol() const;
	virtual bool LoadFromFile( IFileSystem* fileSystem, const char* resource, const char* pathID = NULL );
	virtual bool SaveToFile( IFileSystem* fileSystem, const char* resource, const char* pathID = NULL );
	virtual KeyValues* FindKey( const char* name, bool create = false );
	virtual KeyValues* FindKey( int symbol ) const;
	virtual KeyValues* CreateNewKey();
	void AddSubKey( KeyValues* key );
	virtual void RemoveSubKey( KeyValues* key );
	virtual KeyValues* GetFirstSubKey();
	virtual KeyValues* GetNextKey();
	void SetNextKey( KeyValues* key );

	virtual int GetInt( const char* name = NULL, int defaultValue = 0 );
	unsigned long long GetUint64( const char* name = NULL, unsigned long long defaultValue = 0 );
	virtual float GetFloat( const char* name = NULL, float defaultValue = 0.0f );
	virtual const char* GetString( const char* name = NULL, const char* defaultValue = "" );
	virtual const wchar_t* GetWString( const char* name = NULL, const wchar_t* defaultValue = L"" );
	virtual void* GetPtr( const char* name = NULL, void* defaultValue = NULL );
	Color GetColor( const char* name = NULL );
	virtual bool IsEmpty( const char* name = NULL );

	virtual void SetWString( const char* name, const wchar_t* value );
	virtual void SetString( const char* name, const char* value );
	virtual void SetInt( const char* name, int value );
	void SetUint64( const char* name, unsigned long long value );
	virtual void SetFloat( const char* name, float value );
	virtual void SetPtr( const char* name, void* value );
	void SetColor( const char* name, Color value );

	void* operator new( size_t size );
	void* operator new( size_t size, int blockType, const char* filename, int line );
	void operator delete( void* memory );
	void operator delete( void* memory, int blockType, const char* filename, int line );

	virtual KeyValues* MakeCopy() const;
	virtual void Clear();
	virtual types_t GetDataType( const char* name = NULL );
	virtual void deleteThis();
	void LoadFromBuffer( const char* buffer );

private:
	KeyValues( KeyValues& other );	// prevent copy constructor being used

	// prevent delete being called except through deleteThis()
	~KeyValues();
	void RecursiveSaveToFile( IFileSystem* fileSystem, CUtlBuffer& buffer, int indent );
	void WriteConvertedString( CUtlBuffer& buffer, const char* text );
	void RecursiveLoadFromBuffer( char** buffer );
	void Init( const char* name );
	void FreeAllocatedValue();
	void AllocateValueBlock( int size );

	int m_iKeyName;	// keyname is a symbol defined in KeyValuesSystem

	// we don't delete these
	union
	{
		int m_iValue;
		float m_flValue;
		void* m_pValue;

		// These are needed out of the union because the API returns string pointers
		char* m_sValue;
		unsigned char m_Color[4];
	};
	unsigned short m_iDataType;
	short m_iAllocationSize;
	KeyValues* m_pPeer;	// pointer to next key in list
	KeyValues* m_pSub;	// pointer to Start of a new sub key list
};

IKeyValues* keyvalues();
