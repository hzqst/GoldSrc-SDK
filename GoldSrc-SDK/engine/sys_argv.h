#pragma once

// Parse in place, removing grouping quotes without interpreting backslashes.
static int Sys_ParseArgv( char* cursor, char** arguments, int capacity )
{
	int argc = 1; // Slot zero is supplied by the caller.
	while ( cursor && *cursor && argc < capacity )
	{
		while ( *cursor && static_cast<unsigned char>( *cursor ) <= ' ' )
			++cursor;
		if ( !*cursor )
			break;
		char* output = cursor;
		arguments[argc++] = output;
		bool inQuotes = false;
		while ( *cursor )
		{
			if ( *cursor == '"' )
			{
				inQuotes = !inQuotes;
				++cursor;
			}
			else if ( !inQuotes && static_cast<unsigned char>( *cursor ) <= ' ' )
				break;
			else
				*output++ = *cursor++;
		}
		char* next = *cursor ? cursor + 1 : cursor;
		*output = '\0';
		cursor = next;
	}
	if ( argc < capacity )
		arguments[argc] = nullptr;
	return argc;
}
