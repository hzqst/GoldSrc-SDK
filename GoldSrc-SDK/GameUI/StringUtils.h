//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: bounded string helpers that report truncation instead of hiding it
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include <stddef.h>

// Formats into buffer; returns false when the result does not fit
bool	UTIL_FormatString( char* buffer, size_t bufferSize, const char* format, ... );
// Copies source into buffer; returns false and leaves buffer unchanged when it does not fit
bool	UTIL_CopyString( char* buffer, size_t bufferSize, const char* source );
// Returns whether text can follow a console command unquoted without adding
// commands, i.e. it has no command separator, quote or line break
bool	UTIL_IsUnquotedCommandArgument( const char* text );
// Uppercases only a-z so the user locale cannot rewrite UTF-8 bytes
void	UTIL_UppercaseASCII( char* text );
