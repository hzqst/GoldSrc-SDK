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

#include "interface.h"

class IFileSystem;
class KeyValues;

namespace vgui2
{
typedef unsigned long StringIndex_t;

#define INVALID_STRING_INDEX ( (vgui2::StringIndex_t)-1 )

//-----------------------------------------------------------------------------
// Purpose: Handles localization of text
//			looks up string names and returns the localized unicode text
//-----------------------------------------------------------------------------
class ILocalize : public IBaseInterface
{
public:
	// adds the contents of a file to the localization table
	virtual bool AddFile( IFileSystem* fileSystem, const char* filename );

	// Remove all strings from the table
	virtual void RemoveAll();

	// Finds the localized text for tokenName
	virtual wchar_t* Find( const char* tokenName );

	// converts an english string to unicode
	// returns the number of wchar_t in resulting string, including null terminator
	virtual int ConvertANSIToUnicode( const char* ansi, wchar_t* unicode, int unicodeBufferSizeInBytes );

	// converts an unicode string to an english string
	// unrepresentable characters are converted to system default
	// returns the number of characters in resulting string, including null terminator
	virtual int ConvertUnicodeToANSI( const wchar_t* unicode, char* ansi, int ansiBufferSize );

	// finds the index of a token by token name, INVALID_STRING_INDEX if not found
	virtual StringIndex_t FindIndex( const char* tokenName );

	// builds a localized formatted string
	// uses the format strings first: %s1, %s2, ...  unicode strings (wchar_t *)
	virtual void ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes, wchar_t* formatString,
		int numFormatParameters, ... );

	// gets the values by the string index
	virtual const char* GetNameByIndex( StringIndex_t index );
	virtual wchar_t* GetValueByIndex( StringIndex_t index );

	// iteration functions
	virtual StringIndex_t GetFirstStringIndex();

	// returns the next index, or INVALID_STRING_INDEX if no more strings available
	virtual StringIndex_t GetNextStringIndex( StringIndex_t index );

	// adds a single name/unicode string pair to the table
	virtual void AddString( const char* tokenName, wchar_t* unicodeString, const char* filename );

	// changes the value of a string
	virtual void SetValueByIndex( StringIndex_t index, wchar_t* newValue );

	// saves the entire contents of the token tree to the file
	virtual bool SaveToFile( IFileSystem* fileSystem, const char* filename );

	// iterates the filenames
	virtual int GetLocalizationFileCount();
	virtual const char* GetLocalizationFileName( int index );

	// returns the name of the file the specified localized string is stored in
	virtual const char* GetFileNameByIndex( StringIndex_t index );

	// for development only, reloads localization files
	virtual void ReloadLocalizationFiles( IFileSystem* fileSystem );

	// need to replace the existing ConstructString with this
	virtual void ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes, const char* tokenName,
		KeyValues* localizationVariables );
	virtual void ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
		StringIndex_t unlocalizedTextSymbol, KeyValues* localizationVariables );
};
}
