#ifndef LOCALIZEDSTRINGTABLE_H
#define LOCALIZEDSTRINGTABLE_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui/ILocalize.h"
#include "utlrbtree.h"
#include "utlsymbol.h"
#include "utlvector.h"

#include <stdarg.h>
#include <stddef.h>
#include <wchar.h>

#define INVALID_LOCALIZE_STRING_INDEX	((vgui2::StringIndex_t)-1)

class IFileSystem;
class KeyValues;

typedef unsigned short ucs2;

namespace vgui2
{

typedef unsigned long StringIndex_t;

typedef unsigned short OldStringIndex_t;

class CLocalizedStringTable_VGUI_Localize002 : public IBaseInterface
{
public:
	virtual ~CLocalizedStringTable_VGUI_Localize002() override;

	virtual bool AddFile( IFileSystem* fileSystem, const char* fileName );
	virtual void RemoveAll();
	virtual wchar_t* Find( const char* tokenName );

	virtual int ConvertANSIToUnicode( const char* ansi, wchar_t* unicode, int unicodeBufferSizeInBytes );
	virtual int ConvertUnicodeToANSI( const wchar_t* unicode, char* ansi, int ansiBufferSize );
	virtual OldStringIndex_t FindIndex( const char* tokenName );
	virtual void ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
		wchar_t* formatString, int numFormatParameters, ... );

	virtual const char* GetNameByIndex( OldStringIndex_t index );
	virtual wchar_t* GetValueByIndex( OldStringIndex_t index );
	virtual OldStringIndex_t GetFirstStringIndex();
	virtual OldStringIndex_t GetNextStringIndex( OldStringIndex_t index );
	virtual void AddString( const char* tokenName, wchar_t* unicodeString, const char* fileName );
	virtual void SetValueByIndex( OldStringIndex_t index, wchar_t* newValue );

	virtual bool SaveToFile( IFileSystem* fileSystem, const char* fileName );
	virtual int GetLocalizationFileCount();
	virtual const char* GetLocalizationFileName( int index );
	virtual const char* GetFileNameByIndex( OldStringIndex_t index );
};

}

class CLocalizedStringTable : public vgui2::ILocalize
{
public:
	CLocalizedStringTable();
	~CLocalizedStringTable() override;

	virtual bool AddFile( IFileSystem* fileSystem, const char* fileName ) override;
	virtual void RemoveAll() override;
	virtual wchar_t* Find( const char* pName ) override;

	virtual int ConvertANSIToUnicode( const char* ansi, wchar_t* unicode, int unicodeBufferSizeInBytes ) override;
	virtual int ConvertUnicodeToANSI( const wchar_t* unicode, char* ansi, int ansiBufferSize ) override;
	virtual vgui2::StringIndex_t FindIndex( const char* pName ) override;
	virtual void ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
		wchar_t* formatString, int numFormatParameters, ... ) override;

	virtual const char* GetNameByIndex( vgui2::StringIndex_t index ) override;
	virtual wchar_t* GetValueByIndex( vgui2::StringIndex_t index ) override;
	virtual vgui2::StringIndex_t GetFirstStringIndex() override;
	virtual vgui2::StringIndex_t GetNextStringIndex( vgui2::StringIndex_t index ) override;
	virtual void AddString( const char* tokenName, wchar_t* unicodeString, const char* fileName ) override;
	virtual void SetValueByIndex( vgui2::StringIndex_t index, wchar_t* newValue ) override;

	virtual bool SaveToFile( IFileSystem* fileSystem, const char* fileName ) override;
	virtual int GetLocalizationFileCount() override;
	virtual const char* GetLocalizationFileName( int index ) override;
	virtual const char* GetFileNameByIndex( vgui2::StringIndex_t index ) override;
	virtual void ReloadLocalizationFiles( IFileSystem* filesystem ) override;
	virtual void ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
		const char* tokenName, KeyValues* localizationVariables ) override;
	virtual void ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
		vgui2::StringIndex_t unlocalizedTextSymbol, KeyValues* localizationVariables ) override;

private:
	bool AddAllLanguageFiles( IFileSystem* fileSystem, const char* baseFileName );
	void BuildFastValueLookup();
	void DiscardFastValueLookup();
	int FindExistingValueIndex( const wchar_t* value );
	int AddValue( const wchar_t* value );
	void ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
		wchar_t* formatString, int numFormatParameters, va_list argList );

	struct localizedstring_t
	{
		vgui2::StringIndex_t	nameIndex;
		vgui2::StringIndex_t	valueIndex;
		CUtlSymbol				filename;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
	};

	struct fastvalue_t
	{
		int				valueindex;
		const wchar_t*	search;
	};

	static bool SymLess( const localizedstring_t& i1, const localizedstring_t& i2 );
	static bool FastValueLessFunc( const fastvalue_t& lhs, const fastvalue_t& rhs );

	char											m_szLanguage[64];
	bool											m_bUseOnlyLongestLanguageString;
	CUtlRBTree< localizedstring_t, unsigned long >	m_Lookup;
	CUtlVector< char >								m_Names;
	CUtlVector< wchar_t >							m_Values;
	CUtlSymbol										m_CurrentFile;
	CUtlVector< CUtlSymbol >						m_LocalizationFiles;
	CUtlRBTree< fastvalue_t, int >					m_FastValueLookup;

	static CLocalizedStringTable*	s_pTable;
};

extern CLocalizedStringTable							g_StringTable;
extern vgui2::CLocalizedStringTable_VGUI_Localize002	g_StringTable_Localize002;

char*	Q_strncat( char* pDest, const char* pSrc, size_t destBufferSize, int max_chars_to_copy );
int		Q_UCS2ToUTF8( const ucs2* pUCS2, int cubSrcInBytes, char* pUTF8, int cubDestSizeInBytes );

#endif
