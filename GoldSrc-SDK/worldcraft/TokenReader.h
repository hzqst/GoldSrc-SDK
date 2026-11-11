//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef TOKENREADER_H
#define TOKENREADER_H
#pragma once

#include <fstream>

#define MAX_TOKEN 128
#define MAX_STRING 128
#define MAX_IDENT 128
#define IsToken( token, expected ) ( !_stricmp( ( token ), ( expected ) ) )

enum trtoken_t
{
	TOKENSTRINGTOOLONG = -3,
	TOKENNONE = -2,
	TOKENEOF = -1,
	OPERATOR = 0,
	INTEGER,
	STRING,
	IDENT
};

class TokenReader
{
public:
	TokenReader();
	bool Open( const char* filename );
	void Close();
	trtoken_t NextToken( char* token, int size = MAX_TOKEN );
	trtoken_t NextTokenDynamic( char** token );
	trtoken_t PeekTokenType( char* token = NULL );
	void Stuff( trtoken_t type, const char* token );
	void IgnoreTill( trtoken_t type, const char* token );
	const char* Error( const char* message );
	int GetErrorCount() const { return m_nErrors; }

private:
	std::ifstream	m_File;
	int				m_nLine;
	int				m_nErrors;
	char			m_szFilename[MAX_STRING];
	char			m_szToken[MAX_TOKEN];
	bool			m_bStuffed;
	trtoken_t		m_eStuffedType;
};

#endif // TOKENREADER_H
