#ifndef COMMANDLINEARGS_H
#define COMMANDLINEARGS_H

#ifdef _WIN32
#pragma once
#endif

#include <algorithm>
#include <stddef.h>
#include <string.h>
#include <string>
#include <vector>

// splits a Windows command line into arguments, spaces and tabs separate them and double quotes keep spaces inside one
inline std::vector<std::string> SplitCommandLine( const char* cmdLine )
{
	std::vector<std::string> args;
	std::string arg;
	bool inArg = false;
	bool quoted = false;
	for ( const char* p = cmdLine; *p; ++p )
	{
		if ( *p == '"' )
		{
			quoted = !quoted;
			inArg = true;
		}
		else if ( !quoted && ( *p == ' ' || *p == '\t' ) )
		{
			if ( inArg )
			{
				args.push_back( arg );
				arg.clear();
				inArg = false;
			}
		}
		else
		{
			arg += *p;
			inArg = true;
		}
	}
	if ( inArg )
	{
		args.push_back( arg );
	}
	return args;
}

// splits the arguments of /proc/self/cmdline, where a NUL ends each argument
inline std::vector<std::string> SplitNulSeparatedArgs( const char* data, size_t size )
{
	std::vector<std::string> args;
	size_t start = 0;
	for ( size_t i = 0; i < size; ++i )
	{
		if ( data[i] == '\0' )
		{
			args.push_back( std::string( data + start, i - start ) );
			start = i + 1;
		}
	}
	if ( start < size )
	{
		args.push_back( std::string( data + start, size - start ) );
	}
	return args;
}

// joins arguments into one line, quoting those with spaces so the line reads like the Windows command line
inline std::string JoinCommandLine( const std::vector<std::string>& args )
{
	std::string cmdLine;
	for ( const std::string& arg : args )
	{
		if ( !cmdLine.empty() )
		{
			cmdLine += ' ';
		}
		if ( arg.empty() || arg.find_first_of( " \t" ) != std::string::npos )
		{
			cmdLine += '"' + arg + '"';
		}
		else
		{
			cmdLine += arg;
		}
	}
	return cmdLine;
}

// returns whether paramName is one of the arguments, a longer argument that starts with it does not count
inline bool CommandLineArgExists( const std::vector<std::string>& args, const char* paramName )
{
	return paramName && std::find( args.begin(), args.end(), paramName ) != args.end();
}

// copies the argument after paramName into value, cut to fit valueBufferSize including the terminator
// returns false and leaves value untouched when paramName is missing, is the last argument or value cannot hold a string
inline bool CopyCommandLineArgValue( const std::vector<std::string>& args, const char* paramName, char* value, int valueBufferSize )
{
	if ( !paramName || !value || valueBufferSize <= 0 )
	{
		return false;
	}

	std::vector<std::string>::const_iterator it = std::find( args.begin(), args.end(), paramName );
	if ( it == args.end() || ++it == args.end() )
	{
		return false;
	}

	size_t count = ( std::min )( it->size(), (size_t)valueBufferSize - 1 );
	memcpy( value, it->data(), count );
	value[count] = 0;
	return true;
}

#endif // COMMANDLINEARGS_H
