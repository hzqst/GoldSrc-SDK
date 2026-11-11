#pragma once

#include <cctype>
#include <cstdlib>
#include <string>

namespace
{
struct CommandLineToken
{
	const char* begin;
	const char* end;
	std::string value;
	bool quoted = false;

	bool Matches( const char* name ) const { return !quoted && value == name; }
	bool IsParameter() const
	{
		if ( quoted || value.empty() || ( value[0] != '-' && value[0] != '+' ) )
			return false;
		if ( value.size() > 1 && ( isdigit( static_cast<unsigned char>( value[1] ) ) || value[1] == '.' ) )
		{
			char* endNumber = nullptr;
			strtod( value.c_str(), &endNumber );
			if ( endNumber == value.c_str() + value.size() )
				return false;
		}
		return true;
	}
};

CommandLineToken ReadToken( const char* cursor )
{
	while ( isspace( static_cast<unsigned char>( *cursor ) ) )
		++cursor;
	CommandLineToken token{ cursor, cursor };
	bool inQuotes = false;
	while ( *cursor && ( inQuotes || !isspace( static_cast<unsigned char>( *cursor ) ) ) )
	{
		if ( *cursor == '"' )
		{
			inQuotes = !inQuotes;
			token.quoted = true;
		}
		else
			token.value += *cursor;
		++cursor;
	}
	token.end = cursor;
	return token;
}
}
